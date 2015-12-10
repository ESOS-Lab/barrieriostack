/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_enc.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <media/videobuf2-core.h>
#include <linux/v4l2-controls.h>

#include "s5p_mfc_common.h"

#include "s5p_mfc_intr.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_reg.h"
#include "s5p_mfc_enc.h"
#include "s5p_mfc_pm.h"

#define DEF_SRC_FMT	1
#define DEF_DST_FMT	2

/*
 * RGB encoding information to avoid confusion.
 *
 * V4L2_PIX_FMT_ARGB32 takes ARGB data like below.
 * MSB                              LSB
 * 3       2       1
 * 2       4       6       8       0
 * |B......BG......GR......RA......A|
 */
static struct s5p_mfc_fmt formats[] = {
	{
		.name = "4:2:0 3 Planes Y/Cb/Cr",
		.fourcc = V4L2_PIX_FMT_YUV420M,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 3,
	},
	{
		.name = "4:2:0 3 Planes Y/Cr/Cb",
		.fourcc = V4L2_PIX_FMT_YVU420M,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 3,
	},
	{
		.name = "4:2:0 2 Planes 16x16 Tiles",
		.fourcc = V4L2_PIX_FMT_NV12MT_16X16,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 2,
	},
	{
		.name = "4:2:0 2 Planes 64x32 Tiles",
		.fourcc = V4L2_PIX_FMT_NV12MT,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 2,
	},
	{
		.name = "4:2:0 2 Planes",
		.fourcc = V4L2_PIX_FMT_NV12M,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 2,
	},
	{
		.name = "4:2:0 2 Planes Y/CrCb",
		.fourcc = V4L2_PIX_FMT_NV21M,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 2,
	},
	{
		.name = "RGB888 1 Plane 24bpp",
		.fourcc = V4L2_PIX_FMT_RGB24,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 1,
	},
	{
		.name = "RGB565 1 Plane 16bpp",
		.fourcc = V4L2_PIX_FMT_RGB565,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 1,
	},
	{
		.name = "RGBX8888 1 Plane 32bpp",
		.fourcc = V4L2_PIX_FMT_RGB32X,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 1,
	},
	{
		.name = "BGRA8888 1 Plane 32bpp",
		.fourcc = V4L2_PIX_FMT_BGR32,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 1,
	},
	{
		.name = "ARGB8888 1 Plane 32bpp",
		.fourcc = V4L2_PIX_FMT_ARGB32,
		.codec_mode = MFC_FORMATS_NO_CODEC,
		.type = MFC_FMT_RAW,
		.num_planes = 1,
	},
	{
		.name = "H264 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_H264,
		.codec_mode = S5P_FIMV_CODEC_H264_ENC,
		.type = MFC_FMT_ENC,
		.num_planes = 1,
	},
	{
		.name = "MPEG4 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.codec_mode = S5P_FIMV_CODEC_MPEG4_ENC,
		.type = MFC_FMT_ENC,
		.num_planes = 1,
	},
	{
		.name = "H263 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_H263,
		.codec_mode = S5P_FIMV_CODEC_H263_ENC,
		.type = MFC_FMT_ENC,
		.num_planes = 1,
	},
	{
		.name = "VP8 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_VP8,
		.codec_mode = S5P_FIMV_CODEC_VP8_ENC,
		.type = MFC_FMT_ENC,
		.num_planes = 1,
	},
	{
		.name = "HEVC Encoded Stream",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.codec_mode = S5P_FIMV_CODEC_HEVC_ENC,
		.type = MFC_FMT_ENC,
		.num_planes = 1,
	},
};

#define NUM_FORMATS ARRAY_SIZE(formats)

static struct s5p_mfc_fmt *find_format(struct v4l2_format *f, unsigned int t)
{
	unsigned long i;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (formats[i].fourcc == f->fmt.pix_mp.pixelformat &&
		    formats[i].type == t)
			return (struct s5p_mfc_fmt *)&formats[i];
	}

	return NULL;
}

static struct v4l2_queryctrl controls[] = {
	{
		.id = V4L2_CID_CACHEABLE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Cacheable flag",
		.minimum = 0,
		.maximum = 3,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "The period of intra frame",
		.minimum = 0,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "The slice partitioning method",
		.minimum = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.maximum = V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "The number of MB in a slice",
		.minimum = 1,
		.maximum = ENC_MULTI_SLICE_MB_MAX,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "The maximum bits per slices",
		.minimum = ENC_MULTI_SLICE_BYTE_MIN,
		.maximum = (1 << 30) - 1,
		.step = 1,
		.default_value = ENC_MULTI_SLICE_BYTE_MIN,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "The number of intra refresh MBs",
		.minimum = 0,
		.maximum = ENC_INTRA_REFRESH_MB_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_PADDING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Padding control enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_PADDING_YUV,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Padding Color YUV Value",
		.minimum = 0,
		.maximum = (1 << 25) - 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Frame level rate control enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Target bit rate rate-control",
		.minimum = 1,
		.maximum = (1 << 30) - 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Rate control reaction coeff.",
		.minimum = 1,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_STREAM_SIZE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Encoded stream size",
		.minimum = 0,
		.maximum = (1 << 30) - 1,
		.step = 1,
		.default_value = 0,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_COUNT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Encoded frame count",
		.minimum = 0,
		.maximum = (1 << 30) - 1,
		.step = 1,
		.default_value = 0,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TYPE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Encoded frame type",
		.minimum = 0,
		.maximum = 5,
		.step = 1,
		.default_value = 0,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Force frame type",
		.minimum = V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_DISABLED,
		.maximum = V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_NOT_CODED,
		.step = 1,
		.default_value = \
			V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_DISABLED,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VBV_SIZE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VBV buffer size (1Kbits)",
		.minimum = 0,
		.maximum = ENC_VBV_BUF_SIZE_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sequence header mode",
		.minimum = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		.maximum = V4L2_MPEG_VIDEO_HEADER_MODE_AT_THE_READY,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Frame skip enable",
		.minimum = V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_DISABLED,
		.maximum = V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT,
		.step = 1,
		.default_value = V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_DISABLED,
	},
	{	/* MFC5.x Only */
		.id = V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Fixed target bit enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{	/* MFC6.x Only for H.264 & H.263 */
		.id = V4L2_CID_MPEG_MFC6X_VIDEO_FRAME_DELTA,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 frame delta",
		.minimum = 1,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_B_FRAMES,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "The number of B frames",
		.minimum = 0,
		.maximum = 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 profile",
		.minimum = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.maximum = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 level",
		.minimum = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_5_1,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_H264_INTERLACE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "H264 interlace mode",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 loop filter mode",
		.minimum = V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED,
		.maximum = V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_S_B,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 loop filter alpha offset",
		.minimum = ENC_H264_LOOP_FILTER_AB_MIN,
		.maximum = ENC_H264_LOOP_FILTER_AB_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 loop filter beta offset",
		.minimum = ENC_H264_LOOP_FILTER_AB_MIN,
		.maximum = ENC_H264_LOOP_FILTER_AB_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 entorpy mode",
		.minimum = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
		.maximum = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_H264_NUM_REF_PIC_FOR_P,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "The number of ref. picture of P",
		.minimum = 1,
		.maximum = 2,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "H264 8x8 transform enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "MB level rate control",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_H264_RC_FRAME_RATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 Frame rate",
		.minimum = 1,
		.maximum = ENC_H264_RC_FRAME_RATE_MAX,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 Frame QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 Minimum QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		/* MAX_QP must be greater than or equal to MIN_QP */
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 Maximum QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_DARK,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "H264 dark region adaptive",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_SMOOTH,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "H264 smooth region adaptive",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_STATIC,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "H264 static region adaptive",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_ACTIVITY,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "H264 MB activity adaptive",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 P frame QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 B frame QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Aspect ratio VUI enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_IDC,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VUI aspect ratio IDC",
		.minimum = V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_UNSPECIFIED,
		.maximum = V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_EXTENDED,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Horizontal size of SAR",
		.minimum = 0,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Vertical size of SAR",
		.minimum = 0,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_GOP_CLOSURE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "GOP closure",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H264 I period",
		.minimum = 0,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Hierarchical Coding",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Type",
		.minimum = V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_B,
		.maximum = V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_P,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_B,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer",
		.minimum = 0,
		.maximum = 7,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer QP",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "frame pack sei generation flag",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_CURRENT_FRAME_0,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Current frame is frame 0 flag",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_ARRANGEMENT_TYPE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Frame packing arrangement type",
		.minimum = V4L2_MPEG_VIDEO_H264_SEI_FP_ARRANGEMENT_TYPE_SIDE_BY_SIDE,
		.maximum = V4L2_MPEG_VIDEO_H264_SEI_FP_ARRANGEMENT_TYPE_TEMPORAL,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_H264_SEI_FP_ARRANGEMENT_TYPE_SIDE_BY_SIDE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_FMO,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Flexible Macroblock Order",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_FMO_MAP_TYPE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Map type for FMO",
		.minimum = V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_INTERLEAVED_SLICES,
		.maximum = V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_WIPE_SCAN,
		.step = 1,
		.default_value = \
			V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_INTERLEAVED_SLICES,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_FMO_SLICE_GROUP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Number of slice groups for FMO",
		.minimum = 1,
		.maximum = 4,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_FMO_RUN_LENGTH,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "FMO Run Length",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_FMO_CHANGE_DIRECTION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Direction of the slice group",
		.minimum = V4L2_MPEG_VIDEO_H264_FMO_CHANGE_DIR_RIGHT,
		.maximum = V4L2_MPEG_VIDEO_H264_FMO_CHANGE_DIR_LEFT,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_H264_FMO_CHANGE_DIR_RIGHT,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_FMO_CHANGE_RATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Size of the first slice group",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_ASO,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Arbitrary Slice Order",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_ASO_SLICE_ORDER,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "ASO Slice order",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PREPEND_SPSPPS_TO_IDR,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Prepend SPS/PPS to every IDR",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 profile",
		.minimum = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		.maximum = V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 level",
		.minimum = V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,
		.maximum = V4L2_MPEG_VIDEO_MPEG4_LEVEL_6,
		.step = 1,
		.default_value = V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 Frame QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 Minimum QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 Maximum QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{	/* MFC5.x Only */
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_QPEL,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Quarter pixel search enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 P frame QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_B_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 B frame QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_MPEG4_VOP_TIME_RES,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 vop time resolution",
		.minimum = 0,
		.maximum = ENC_MPEG4_VOP_TIME_RES_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_MPEG4_VOP_FRM_DELTA,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MPEG4 frame delta",
		.minimum = 1,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_H263_RC_FRAME_RATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H263 Frame rate",
		.minimum = 1,
		.maximum = ENC_H263_RC_FRAME_RATE_MAX,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H263 Frame QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H263_MIN_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H263 Minimum QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H263_MAX_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H263 Maximum QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H263 P frame QP value",
		.minimum = 1,
		.maximum = 31,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Frame Tag",
		.minimum = 0,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Frame Status",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_QOS_RATIO,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "QoS ratio value",
		.minimum = 20,
		.maximum = 200,
		.step = 10,
		.default_value = 100,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_VERSION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 version",
		.minimum = 0,
		.maximum = 3,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_I_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 Frame QP value",
		.minimum = 0,
		.maximum = 127,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_P_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 Frame QP value",
		.minimum = 0,
		.maximum = 127,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_MAX_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 Frame QP MAX value",
		.minimum = 0,
		.maximum = 127,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_MIN_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 Frame QP MIN value",
		.minimum = 0,
		.maximum = 127,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_RC_FRAME_RATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 Frame rate",
		.minimum = 1,
		.maximum = ENC_H264_RC_FRAME_RATE_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_NUM_OF_PARTITIONS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 number of partitions",
		.minimum = 0,
		.maximum = 8,
		.step = 2,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_FILTER_LEVEL,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 loop filter level",
		.minimum = 0,
		.maximum = 63,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_FILTER_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 loop filter sharpness",
		.minimum = 0,
		.maximum = 7,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_GOLDEN_FRAMESEL,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 indication of golden frame",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_GF_REFRESH_PERIOD,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 indication of golden frame",
		.minimum = 0,
		.maximum = ((1 << 16) - 1),
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "VP8 hierarchy QP enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER0,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 layer0 QP value",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER1,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 layer1 QP value",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER2,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 layer2 QP value",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_REF_NUMBER_FOR_PFRAMES,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "VP8 Number of reference picture",
		.minimum = 1,
		.maximum = 2,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_DISABLE_INTRA_MD4X4,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "VP8 intra 4x4 mode disable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC70_VIDEO_VP8_NUM_TEMPORAL_LAYER,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "VP8 number of hierarchical layer",
		.minimum = 0,
		.maximum = 3,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC Frame QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC P frame QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC B frame QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},

	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC Minimum QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		/* MAX_QP must be greater than or equal to MIN_QP */
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC Maximum QP value",
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_DARK,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC dark region adaptive",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_SMOOTH,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC smooth region adaptive",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_STATIC,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC static region adaptive",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_ACTIVITY,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC activity adaptive",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC Profile",
		.minimum = 0,
		.maximum = 0,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC level",
		.minimum = 10,
		.maximum = 62,
		.step = 1,
		.default_value = 10,
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_TIER_FLAG,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC tier_flag default is Main",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_RC_FRAME_RATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC Frame rate",
		.minimum = 1,
		.maximum = ENC_HEVC_RC_FRAME_RATE_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_MAX_PARTITION_DEPTH,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC Maximum coding unit depth",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REF_NUMBER_FOR_PFRAMES,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC Number of reference picture",
		.minimum = 1,
		.maximum = 2,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REFRESH_TYPE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC Number of reference picture",
		.minimum = 0,
		.maximum = 2,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_CONST_INTRA_PRED_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC refresh type",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LOSSLESS_CU_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC lossless encoding select",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_WAVEFRONT_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC Wavefront enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_DISABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC Filter disable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_SLICE_BOUNDARY,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "across or not slice boundary",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LTR_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "long term reference enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_QP_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "QP values for temporal layer",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_TYPE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Hierarchical Coding Type",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer",
		.minimum = 0,
		.maximum = 7,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer QP",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer BIT",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_SIGN_DATA_HIDING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC Sign data hiding",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_GENERAL_PB_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC General pb enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_TEMPORAL_ID_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC Temporal id enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_STRONG_SMOTHING_FLAG,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC Strong intra smoothing flag",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_DISABLE_INTRA_PU_SPLIT,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC disable intra pu split",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_DISABLE_TMV_PREDICTION,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "HEVC disable tmv prediction",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_MAX_NUM_MERGE_MV_MINUS1,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "max number of candidate MVs",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_WITHOUT_STARTCODE_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "ENC without startcode enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REFRESH_PERIOD,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC Number of reference picture",
		.minimum = 0,
		.maximum = 10, /* need to check maximum size */
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_QP_INDEX_CR,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Chroma QP index offset Cr",
		.minimum = ENC_HEVC_QP_INDEX_MIN,
		.maximum = ENC_HEVC_QP_INDEX_MAX,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_QP_INDEX_CB,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Chroma QP index offset Cb",
		.minimum = ENC_HEVC_QP_INDEX_MIN,
		.maximum = ENC_HEVC_QP_INDEX_MAX,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_BETA_OFFSET_DIV2,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC loop filter beta offset",
		.minimum = ENC_HEVC_LOOP_FILTER_MIN,
		.maximum = ENC_HEVC_LOOP_FILTER_MAX,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_TC_OFFSET_DIV2,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC loop filter tc offset",
		.minimum = ENC_HEVC_LOOP_FILTER_MIN,
		.maximum = ENC_HEVC_LOOP_FILTER_MAX,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "HEVC size of length field",
		.minimum = 0,
		.maximum = 3,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_USER_REF,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "user long term reference frame",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC90_VIDEO_HEVC_STORE_REF,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "store long term reference frame",
		.minimum = 0,
		.maximum = 2,
		.step = 1,
		.default_value = 0, /* need to check defualt value */
	},
	{
		.id = V4L2_CID_MPEG_MFC_GET_VERSION_INFO,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Get MFC version information",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC_GET_EXTRA_BUFFER_SIZE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Get extra buffer size",
		.minimum = 0,
		.maximum = (2 << 31) - 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC_GET_EXT_INFO,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Get extra information",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT0,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit0",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT1,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit1",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT2,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit2",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT3,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit3",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT4,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit4",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT5,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit5",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT6,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit6",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Change",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT0,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit0",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT1,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit1",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT2,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Bit2",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hierarchical Coding Layer Change",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
};

#define NUM_CTRLS ARRAY_SIZE(controls)

static struct v4l2_queryctrl *get_ctrl(int id)
{
	unsigned long i;

	for (i = 0; i < NUM_CTRLS; ++i)
		if (id == controls[i].id)
			return &controls[i];
	return NULL;
}

static int check_ctrl_val(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct v4l2_queryctrl *c;

	c = get_ctrl(ctrl->id);
	if (!c)
		return -EINVAL;
	if (ctrl->value < c->minimum || ctrl->value > c->maximum
	    || (c->step != 0 && ctrl->value % c->step != 0)) {
		v4l2_err(&dev->v4l2_dev, "Invalid control value\n");
		return -ERANGE;
	}
	if (!FW_HAS_ENC_SPSPPS_CTRL(dev) && ctrl->id ==	\
			V4L2_CID_MPEG_VIDEO_H264_PREPEND_SPSPPS_TO_IDR) {
		mfc_err_ctx("Not support feature(0x%x) for F/W\n", ctrl->id);
		return -ERANGE;
	}

	return 0;
}

static int enc_ctrl_read_cst(struct s5p_mfc_ctx *ctx,
		struct s5p_mfc_buf_ctrl *buf_ctrl)
{
	int ret;
	struct s5p_mfc_enc *enc = ctx->enc_priv;

	switch (buf_ctrl->id) {
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS:
		ret = !enc->in_slice;
		break;
	default:
		mfc_err_ctx("not support custom per-buffer control\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct s5p_mfc_ctrl_cfg mfc_ctrl_list[] = {
	{	/* set frame tag */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_SHARED_SET_E_FRAME_TAG,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* get frame tag */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_SHARED_GET_E_FRAME_TAG,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* encoded y physical addr */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_LUMA_ADDR,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = S5P_FIMV_ENCODED_LUMA_ADDR,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* encoded c physical addr */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CHROMA_ADDR,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = S5P_FIMV_ENCODED_CHROMA_ADDR,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* I, not coded frame insertion */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = S5P_FIMV_FRAME_INSERTION,
		.mask = 0x3,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* I period change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_I_PERIOD,
		.mask = 0xFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 0,
	},
	{	/* frame rate change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_FRAME_RATE,
		.mask = 0xFFFF,
		.shft = 16,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 1,
	},
	{	/* bit rate change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_BIT_RATE_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_BIT_RATE,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 2,
	},
	{	/* frame status (in slice or not) */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_CST,
		.addr = 0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
		.read_cst = enc_ctrl_read_cst,
		.write_cst = NULL,
	},
	{	/* H.264 QP Max change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* H.264 QP Min change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* H.263 QP Max change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_H263_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* H.263 QP Min change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_H263_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* MPEG4 QP Max change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* MPEG4 QP Min change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* VP8 QP Max change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_VP8_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* VP8 QP Min change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_VP8_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* H.264 Dynamic Temporal Layer change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_E_H264_HIERARCHICAL_BIT_RATE_LAYER0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 10,
	},
	{	/* HEVC QP Max change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* HEVC QP Min change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_NEW_RC_QP_BOUND,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 4,
	},
	{	/* VP8 Dynamic Temporal Layer change */
		.type = MFC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_CUSTOM,
		.addr = S5P_FIMV_E_H264_HIERARCHICAL_BIT_RATE_LAYER0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_CUSTOM,
		.flag_addr = S5P_FIMV_PARAM_CHANGE_FLAG,
		.flag_shft = 10,
	},
};

#define NUM_CTRL_CFGS ARRAY_SIZE(mfc_ctrl_list)

int s5p_mfc_enc_ctx_ready(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;

	mfc_debug(2, "src=%d, dst=%d, state=%d\n",
		  ctx->src_queue_cnt, ctx->dst_queue_cnt, ctx->state);

	/* context is ready to make header */
	if (ctx->state == MFCINST_GOT_INST && ctx->dst_queue_cnt >= 1) {
		if (p->seq_hdr_mode == \
			V4L2_MPEG_VIDEO_HEADER_MODE_AT_THE_READY) {
			if (ctx->src_queue_cnt >= 1)
				return 1;
		} else {
			return 1;
		}
	}
	/* context is ready to allocate DPB */
	if (ctx->dst_queue_cnt >= 1 && ctx->state == MFCINST_HEAD_PARSED)
		return 1;
	/* context is ready to encode a frame */
	if (ctx->state == MFCINST_RUNNING &&
		ctx->src_queue_cnt >= 1 && ctx->dst_queue_cnt >= 1)
		return 1;
	/* context is ready to encode a frame in case of B frame */
	if (ctx->state == MFCINST_RUNNING_NO_OUTPUT &&
		ctx->src_queue_cnt >= 1 && ctx->dst_queue_cnt >= 1)
		return 1;
	/* context is ready to encode remain frames */
	if (ctx->state == MFCINST_FINISHING &&
		ctx->src_queue_cnt >= 1 && ctx->dst_queue_cnt >= 1)
		return 1;

	mfc_debug(2, "ctx is not ready.\n");

	return 0;
}

static int enc_cleanup_ctx_ctrls(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;

	while (!list_empty(&ctx->ctrls)) {
		ctx_ctrl = list_entry((&ctx->ctrls)->next,
				      struct s5p_mfc_ctx_ctrl, list);

		mfc_debug(7, "Cleanup context control "\
				"id: 0x%08x, type: %d\n",
				ctx_ctrl->id, ctx_ctrl->type);

		list_del(&ctx_ctrl->list);
		kfree(ctx_ctrl);
	}

	INIT_LIST_HEAD(&ctx->ctrls);

	return 0;
}

static int enc_get_buf_update_val(struct s5p_mfc_ctx *ctx,
			struct list_head *head, unsigned int id, int value)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if ((buf_ctrl->id == id)) {
			buf_ctrl->val = value;
			mfc_debug(5, "++id: 0x%08x val: %d\n",
					buf_ctrl->id, buf_ctrl->val);
			break;
		}
	}

	return 0;
}

static int enc_init_ctx_ctrls(struct s5p_mfc_ctx *ctx)
{
	unsigned long i;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;

	INIT_LIST_HEAD(&ctx->ctrls);

	for (i = 0; i < NUM_CTRL_CFGS; i++) {
		ctx_ctrl = kzalloc(sizeof(struct s5p_mfc_ctx_ctrl), GFP_KERNEL);
		if (ctx_ctrl == NULL) {
			mfc_err("Failed to allocate context control "\
					"id: 0x%08x, type: %d\n",
					mfc_ctrl_list[i].id,
					mfc_ctrl_list[i].type);

			enc_cleanup_ctx_ctrls(ctx);

			return -ENOMEM;
		}

		ctx_ctrl->type = mfc_ctrl_list[i].type;
		ctx_ctrl->id = mfc_ctrl_list[i].id;
		ctx_ctrl->has_new = 0;
		ctx_ctrl->val = 0;

		list_add_tail(&ctx_ctrl->list, &ctx->ctrls);

		mfc_debug(7, "Add context control id: 0x%08x, type : %d\n",
				ctx_ctrl->id, ctx_ctrl->type);
	}

	return 0;
}

static void __enc_reset_buf_ctrls(struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		mfc_debug(8, "Reset buffer control value "\
				"id: 0x%08x, type: %d\n",
				buf_ctrl->id, buf_ctrl->type);

		buf_ctrl->has_new = 0;
		buf_ctrl->val = 0;
		buf_ctrl->old_val = 0;
		buf_ctrl->updated = 0;
	}
}

static void __enc_cleanup_buf_ctrls(struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	while (!list_empty(head)) {
		buf_ctrl = list_entry(head->next,
				struct s5p_mfc_buf_ctrl, list);

		mfc_debug(7, "Cleanup buffer control "\
				"id: 0x%08x, type: %d\n",
				buf_ctrl->id, buf_ctrl->type);

		list_del(&buf_ctrl->list);
		kfree(buf_ctrl);
	}

	INIT_LIST_HEAD(head);
}

static int enc_init_buf_ctrls(struct s5p_mfc_ctx *ctx,
	enum s5p_mfc_ctrl_type type, unsigned int index)
{
	unsigned long i;
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct list_head *head;

	if (index >= MFC_MAX_BUFFERS) {
		mfc_err("Per-buffer control index is out of range\n");
		return -EINVAL;
	}

	if (type & MFC_CTRL_TYPE_SRC) {
		if (test_bit(index, &ctx->src_ctrls_avail)) {
			mfc_debug(7, "Source per-buffer control is already "\
					"initialized [%d]\n", index);

			__enc_reset_buf_ctrls(&ctx->src_ctrls[index]);

			return 0;
		}

		head = &ctx->src_ctrls[index];
	} else if (type & MFC_CTRL_TYPE_DST) {
		if (test_bit(index, &ctx->dst_ctrls_avail)) {
			mfc_debug(7, "Dest. per-buffer control is already "\
					"initialized [%d]\n", index);

			__enc_reset_buf_ctrls(&ctx->dst_ctrls[index]);

			return 0;
		}

		head = &ctx->dst_ctrls[index];
	} else {
		mfc_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	INIT_LIST_HEAD(head);

	for (i = 0; i < NUM_CTRL_CFGS; i++) {
		if (!(type & mfc_ctrl_list[i].type))
			continue;

		buf_ctrl = kzalloc(sizeof(struct s5p_mfc_buf_ctrl), GFP_KERNEL);
		if (buf_ctrl == NULL) {
			mfc_err("Failed to allocate buffer control "\
					"id: 0x%08x, type: %d\n",
					mfc_ctrl_list[i].id,
					mfc_ctrl_list[i].type);

			__enc_cleanup_buf_ctrls(head);

			return -ENOMEM;
		}

		buf_ctrl->type = mfc_ctrl_list[i].type;
		buf_ctrl->id = mfc_ctrl_list[i].id;
		buf_ctrl->is_volatile = mfc_ctrl_list[i].is_volatile;
		buf_ctrl->mode = mfc_ctrl_list[i].mode;
		buf_ctrl->addr = mfc_ctrl_list[i].addr;
		buf_ctrl->mask = mfc_ctrl_list[i].mask;
		buf_ctrl->shft = mfc_ctrl_list[i].shft;
		buf_ctrl->flag_mode = mfc_ctrl_list[i].flag_mode;
		buf_ctrl->flag_addr = mfc_ctrl_list[i].flag_addr;
		buf_ctrl->flag_shft = mfc_ctrl_list[i].flag_shft;
		if (buf_ctrl->mode == MFC_CTRL_MODE_CST) {
			buf_ctrl->read_cst = mfc_ctrl_list[i].read_cst;
			buf_ctrl->write_cst = mfc_ctrl_list[i].write_cst;
		}

		list_add_tail(&buf_ctrl->list, head);

		mfc_debug(7, "Add buffer control id: 0x%08x, type : %d\n",
				buf_ctrl->id, buf_ctrl->type);
	}

	__enc_reset_buf_ctrls(head);

	if (type & MFC_CTRL_TYPE_SRC)
		set_bit(index, &ctx->src_ctrls_avail);
	else
		set_bit(index, &ctx->dst_ctrls_avail);

	return 0;
}

static int enc_cleanup_buf_ctrls(struct s5p_mfc_ctx *ctx,
	enum s5p_mfc_ctrl_type type, unsigned int index)
{
	struct list_head *head;

	if (index >= MFC_MAX_BUFFERS) {
		mfc_err("Per-buffer control index is out of range\n");
		return -EINVAL;
	}

	if (type & MFC_CTRL_TYPE_SRC) {
		if (!(test_and_clear_bit(index, &ctx->src_ctrls_avail))) {
			mfc_debug(7, "Source per-buffer control is "\
					"not available [%d]\n", index);
			return 0;
		}

		head = &ctx->src_ctrls[index];
	} else if (type & MFC_CTRL_TYPE_DST) {
		if (!(test_and_clear_bit(index, &ctx->dst_ctrls_avail))) {
			mfc_debug(7, "Dest. per-buffer Control is "\
					"not available [%d]\n", index);
			return 0;
		}

		head = &ctx->dst_ctrls[index];
	} else {
		mfc_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	__enc_cleanup_buf_ctrls(head);

	return 0;
}

static int enc_to_buf_ctrls(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (!(ctx_ctrl->type & MFC_CTRL_TYPE_SET) || !ctx_ctrl->has_new)
			continue;

		list_for_each_entry(buf_ctrl, head, list) {
			if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET))
				continue;

			if (buf_ctrl->id == ctx_ctrl->id) {
				buf_ctrl->has_new = 1;
				buf_ctrl->val = ctx_ctrl->val;
				if (buf_ctrl->is_volatile)
					buf_ctrl->updated = 0;

				ctx_ctrl->has_new = 0;
				break;
			}
		}
	}

	list_for_each_entry(buf_ctrl, head, list) {
		if (buf_ctrl->has_new)
			mfc_debug(8, "Updated buffer control "\
					"id: 0x%08x val: %d\n",
					buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int enc_to_ctx_ctrls(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET) || !buf_ctrl->has_new)
			continue;

		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & MFC_CTRL_TYPE_GET))
				continue;

			if (ctx_ctrl->id == buf_ctrl->id) {
				if (ctx_ctrl->has_new)
					mfc_debug(8,
					"Overwrite context control "\
					"value id: 0x%08x, val: %d\n",
						ctx_ctrl->id, ctx_ctrl->val);

				ctx_ctrl->has_new = 1;
				ctx_ctrl->val = buf_ctrl->val;

				buf_ctrl->has_new = 0;
			}
		}
	}

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (ctx_ctrl->has_new)
			mfc_debug(8, "Updated context control "\
					"id: 0x%08x val: %d\n",
					ctx_ctrl->id, ctx_ctrl->val);
	}

	return 0;
}

static int enc_set_buf_ctrls_val(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	unsigned int value = 0;
	struct temporal_layer_info temporal_LC;
	unsigned int i;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET) || !buf_ctrl->has_new)
			continue;

		/* read old vlaue */
		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			value = s5p_mfc_read_reg(dev, buf_ctrl->addr);
		else if (buf_ctrl->mode == MFC_CTRL_MODE_SHM)
			value = s5p_mfc_read_info(ctx, buf_ctrl->addr);

		/* save old value for recovery */
		if (buf_ctrl->is_volatile)
			buf_ctrl->old_val = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		/* write new value */
		value &= ~(buf_ctrl->mask << buf_ctrl->shft);
		value |= ((buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft);

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			s5p_mfc_write_reg(dev, value, buf_ctrl->addr);
		else if (buf_ctrl->mode == MFC_CTRL_MODE_SHM)
			s5p_mfc_write_info(ctx, value, buf_ctrl->addr);

		/* set change flag bit */
		if (buf_ctrl->flag_mode == MFC_CTRL_MODE_SFR) {
			value = s5p_mfc_read_reg(dev, buf_ctrl->flag_addr);
			value |= (1 << buf_ctrl->flag_shft);
			s5p_mfc_write_reg(dev, value, buf_ctrl->flag_addr);
		} else if (buf_ctrl->flag_mode == MFC_CTRL_MODE_SHM) {
			value = s5p_mfc_read_info(ctx, buf_ctrl->flag_addr);
			value |= (1 << buf_ctrl->flag_shft);
			s5p_mfc_write_info(ctx, value, buf_ctrl->flag_addr);
		}

		buf_ctrl->has_new = 0;
		buf_ctrl->updated = 1;

		if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG)
			enc->stored_tag = buf_ctrl->val;

		if (buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH ||
			buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH) {
			memcpy(&temporal_LC,
				enc->sh_handle.virt, sizeof(struct temporal_layer_info));

			if((temporal_LC.temporal_layer_count < 1) ||
				((temporal_LC.temporal_layer_count > 7) &&
				(ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC)) ||
				((temporal_LC.temporal_layer_count > 3) &&
				(ctx->codec_mode == S5P_FIMV_CODEC_VP8_ENC))) {
				value = s5p_mfc_read_reg(dev, buf_ctrl->flag_addr);
				value &= ~(1 << 10);
				s5p_mfc_write_reg(dev, value, buf_ctrl->flag_addr);
				mfc_err_ctx("temporal layer count is invalid : %d\n"
					, temporal_LC.temporal_layer_count);
				goto invalid_layer_count;
			}

			value = s5p_mfc_read_reg(dev, buf_ctrl->flag_addr);
			/* enable RC_BIT_RATE_CHANGE */
			value |= (1 << 2);
			s5p_mfc_write_reg(dev, value, buf_ctrl->flag_addr);

			mfc_debug(2, "temporal layer count : %d\n", temporal_LC.temporal_layer_count);
			if(ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC)
				s5p_mfc_write_reg(dev,
					temporal_LC.temporal_layer_count, S5P_FIMV_E_H264_NUM_T_LAYER);
			else if(ctx->codec_mode == S5P_FIMV_CODEC_VP8_ENC)
				s5p_mfc_write_reg(dev,
					temporal_LC.temporal_layer_count, S5P_FIMV_E_VP8_NUM_T_LAYER);
			for(i = 0; i < temporal_LC.temporal_layer_count; i++) {
				mfc_debug(2, "temporal layer bitrate[%d] : %d\n",
					i, temporal_LC.temporal_layer_bitrate[i]);
				s5p_mfc_write_reg(dev,
					temporal_LC.temporal_layer_bitrate[i], buf_ctrl->addr + i * 4);
			}
		}
invalid_layer_count:
		mfc_debug(8, "Set buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int enc_get_buf_ctrls_val(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET))
			continue;

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			value = s5p_mfc_read_reg(dev, buf_ctrl->addr);
		else if (buf_ctrl->mode == MFC_CTRL_MODE_SHM)
			value = s5p_mfc_read_info(ctx, buf_ctrl->addr);
		else if (buf_ctrl->mode == MFC_CTRL_MODE_CST)
			value = call_bop(buf_ctrl, read_cst, ctx, buf_ctrl);

		value = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		buf_ctrl->val = value;
		buf_ctrl->has_new = 1;

		mfc_debug(8, "Get buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int enc_recover_buf_ctrls_val(struct s5p_mfc_ctx *ctx,
						struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET)
			|| !buf_ctrl->is_volatile
			|| !buf_ctrl->updated)
			continue;

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			value = s5p_mfc_read_reg(dev, buf_ctrl->addr);
		else if (buf_ctrl->mode == MFC_CTRL_MODE_SHM)
			value = s5p_mfc_read_info(ctx, buf_ctrl->addr);

		value &= ~(buf_ctrl->mask << buf_ctrl->shft);
		value |= ((buf_ctrl->old_val & buf_ctrl->mask)
							<< buf_ctrl->shft);

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			s5p_mfc_write_reg(dev, value, buf_ctrl->addr);
		else if (buf_ctrl->mode == MFC_CTRL_MODE_SHM)
			s5p_mfc_write_info(ctx, value, buf_ctrl->addr);

		/* clear change flag bit */
		if (buf_ctrl->flag_mode == MFC_CTRL_MODE_SFR) {
			value = s5p_mfc_read_reg(dev, buf_ctrl->flag_addr);
			value &= ~(1 << buf_ctrl->flag_shft);
			s5p_mfc_write_reg(dev, value, buf_ctrl->flag_addr);
		} else if (buf_ctrl->flag_mode == MFC_CTRL_MODE_SHM) {
			value = s5p_mfc_read_info(ctx, buf_ctrl->flag_addr);
			value &= ~(1 << buf_ctrl->flag_shft);
			s5p_mfc_write_info(ctx, value, buf_ctrl->flag_addr);
		}

		mfc_debug(8, "Recover buffer control "\
				"id: 0x%08x old val: %d\n",
				buf_ctrl->id, buf_ctrl->old_val);
	}

	return 0;
}

static void cleanup_ref_queue(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_buf *mb_entry;
	dma_addr_t mb_addr;
	int i;

	/* move buffers in ref queue to src queue */
	while (!list_empty(&enc->ref_queue)) {
		mb_entry = list_entry((&enc->ref_queue)->next, struct s5p_mfc_buf, list);

		for (i = 0; i < ctx->raw_buf.num_planes; i++) {
			mb_addr = s5p_mfc_mem_plane_addr(ctx, &mb_entry->vb, i);
			mfc_debug(2, "enc ref[%d] addr: 0x%08lx", i,
					(unsigned long)mb_addr);
		}

		list_del(&mb_entry->list);
		enc->ref_queue_cnt--;

		list_add_tail(&mb_entry->list, &ctx->src_queue);
		ctx->src_queue_cnt++;
	}

	mfc_debug(2, "enc src count: %d, enc ref count: %d\n",
		  ctx->src_queue_cnt, enc->ref_queue_cnt);

	INIT_LIST_HEAD(&enc->ref_queue);
	enc->ref_queue_cnt = 0;
}

static int enc_pre_seq_start(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_buf *dst_mb;
	dma_addr_t dst_addr;
	unsigned int dst_size;
	unsigned long flags;

	spin_lock_irqsave(&dev->irqlock, flags);

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	dst_addr = s5p_mfc_mem_plane_addr(ctx, &dst_mb->vb, 0);
	dst_size = (unsigned int)vb2_plane_size(&dst_mb->vb, 0);
	s5p_mfc_set_enc_stream_buffer(ctx, dst_addr, dst_size);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	return 0;
}

static int enc_post_seq_start(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_buf *dst_mb;
	unsigned long flags;

	mfc_debug(2, "seq header size: %d", s5p_mfc_get_enc_strm_size());

	if ((p->seq_hdr_mode == V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE) ||
	    (p->seq_hdr_mode == V4L2_MPEG_VIDEO_HEADER_MODE_AT_THE_READY)) {
		spin_lock_irqsave(&dev->irqlock, flags);

		dst_mb = list_entry(ctx->dst_queue.next,
				struct s5p_mfc_buf, list);
		list_del(&dst_mb->list);
		ctx->dst_queue_cnt--;

		vb2_set_plane_payload(&dst_mb->vb, 0, s5p_mfc_get_enc_strm_size());
		vb2_buffer_done(&dst_mb->vb, VB2_BUF_STATE_DONE);

		spin_unlock_irqrestore(&dev->irqlock, flags);
	}

	if (IS_MFCV6(dev))
		ctx->state = MFCINST_HEAD_PARSED; /* for INIT_BUFFER cmd */
	else {
		ctx->state = MFCINST_RUNNING;

		if (s5p_mfc_enc_ctx_ready(ctx)) {
			spin_lock_irq(&dev->condlock);
			set_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}
		queue_work(dev->sched_wq, &dev->sched_work);
	}
	if (IS_MFCV6(dev))
		ctx->dpb_count = s5p_mfc_get_enc_dpb_count();

	return 0;
}

static int enc_pre_frame_start(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_buf *dst_mb;
	struct s5p_mfc_buf *src_mb;
	struct s5p_mfc_raw_info *raw;
	unsigned long flags;
	dma_addr_t src_addr[3] = { 0, 0, 0 };
	dma_addr_t dst_addr;
	unsigned int dst_size, i;

	raw = &ctx->raw_buf;
	spin_lock_irqsave(&dev->irqlock, flags);

	src_mb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	for (i = 0; i < raw->num_planes; i++) {
		src_addr[i] = s5p_mfc_mem_plane_addr(ctx, &src_mb->vb, i);
		mfc_debug(2, "enc src[%d] addr: 0x%08lx",
					i, (unsigned long)src_addr[i]);
	}
	s5p_mfc_set_enc_frame_buffer(ctx, &src_addr[0], raw->num_planes);

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	dst_addr = s5p_mfc_mem_plane_addr(ctx, &dst_mb->vb, 0);
	dst_size = (unsigned int)vb2_plane_size(&dst_mb->vb, 0);
	s5p_mfc_set_enc_stream_buffer(ctx, dst_addr, dst_size);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	mfc_debug(2, "enc dst addr: 0x%08lx", (unsigned long)dst_addr);

	return 0;
}

static int enc_post_frame_start(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_buf *mb_entry, *next_entry;
	struct s5p_mfc_raw_info *raw;
	dma_addr_t enc_addr[3] = { 0, 0, 0 };
	dma_addr_t mb_addr[3] = { 0, 0, 0 };
	int slice_type, i;
	unsigned int strm_size;
	unsigned int pic_count;
	unsigned long flags;
	unsigned int index;

	slice_type = s5p_mfc_get_enc_slice_type();
	strm_size = s5p_mfc_get_enc_strm_size();
	pic_count = s5p_mfc_get_enc_pic_count();

	mfc_debug(2, "encoded slice type: %d", slice_type);
	mfc_debug(2, "encoded stream size: %d", strm_size);
	mfc_debug(2, "display order: %d", pic_count);

	/* set encoded frame type */
	enc->frame_type = slice_type;
	raw = &ctx->raw_buf;

	spin_lock_irqsave(&dev->irqlock, flags);

	ctx->sequence++;
	if (strm_size > 0 || ctx->state == MFCINST_FINISHING) {
		/* at least one more dest. buffers exist always  */
		mb_entry = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);

		mb_entry->vb.v4l2_buf.flags &=
			~(V4L2_BUF_FLAG_KEYFRAME |
			  V4L2_BUF_FLAG_PFRAME |
			  V4L2_BUF_FLAG_BFRAME);

		switch (slice_type) {
		case S5P_FIMV_ENCODED_TYPE_I:
			mb_entry->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_KEYFRAME;
			break;
		case S5P_FIMV_ENCODED_TYPE_P:
			mb_entry->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_PFRAME;
			break;
		case S5P_FIMV_ENCODED_TYPE_B:
			mb_entry->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_BFRAME;
			break;
		default:
			mb_entry->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_KEYFRAME;
			break;
		}
		mfc_debug(2, "Slice type : %d\n", mb_entry->vb.v4l2_buf.flags);

		list_del(&mb_entry->list);
		ctx->dst_queue_cnt--;
		vb2_set_plane_payload(&mb_entry->vb, 0, strm_size);

		index = mb_entry->vb.v4l2_buf.index;
		if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->dst_ctrls[index]) < 0)
			mfc_err_ctx("failed in get_buf_ctrls_val\n");

		if (strm_size == 0 && ctx->state == MFCINST_FINISHING)
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
				enc->stored_tag);

		vb2_buffer_done(&mb_entry->vb, VB2_BUF_STATE_DONE);
	}

	if (enc->in_slice) {
		if (ctx->dst_queue_cnt == 0) {
			spin_lock_irq(&dev->condlock);
			clear_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}

		spin_unlock_irqrestore(&dev->irqlock, flags);

		return 0;
	}

	if (slice_type >= 0 &&
			ctx->state != MFCINST_FINISHING) {
		if (ctx->state == MFCINST_RUNNING_NO_OUTPUT)
			ctx->state = MFCINST_RUNNING;

		s5p_mfc_get_enc_frame_buffer(ctx, &enc_addr[0], raw->num_planes);

		for (i = 0; i < raw->num_planes; i++)
			mfc_debug(2, "encoded[%d] addr: 0x%08lx", i,
					(unsigned long)enc_addr[i]);

		list_for_each_entry_safe(mb_entry, next_entry,
						&ctx->src_queue, list) {
			for (i = 0; i < raw->num_planes; i++) {
				mb_addr[i] = s5p_mfc_mem_plane_addr(ctx,
							&mb_entry->vb, i);
				mfc_debug(2, "enc src[%d] addr: 0x%08lx", i,
						(unsigned long)mb_addr[i]);
			}

			if (enc_addr[0] == mb_addr[0]) {
				index = mb_entry->vb.v4l2_buf.index;
				if (call_cop(ctx, recover_buf_ctrls_val, ctx,
						&ctx->src_ctrls[index]) < 0)
					mfc_err_ctx("failed in recover_buf_ctrls_val\n");

				list_del(&mb_entry->list);
				ctx->src_queue_cnt--;

				vb2_buffer_done(&mb_entry->vb, VB2_BUF_STATE_DONE);
				break;
			}
		}

		list_for_each_entry(mb_entry, &enc->ref_queue, list) {
			for (i = 0; i < raw->num_planes; i++) {
				mb_addr[i] = s5p_mfc_mem_plane_addr(ctx,
						&mb_entry->vb, i);
				mfc_debug(2, "enc ref[%d] addr: 0x%08lx", i,
						(unsigned long)mb_addr[i]);
			}

			if (enc_addr[0] == mb_addr[0]) {
				list_del(&mb_entry->list);
				enc->ref_queue_cnt--;

				vb2_buffer_done(&mb_entry->vb, VB2_BUF_STATE_DONE);
				break;
			}
		}
	} else if (ctx->state == MFCINST_FINISHING) {
		mb_entry = list_entry(ctx->src_queue.next,
						struct s5p_mfc_buf, list);
		list_del(&mb_entry->list);
		ctx->src_queue_cnt--;

		vb2_buffer_done(&mb_entry->vb, VB2_BUF_STATE_DONE);
	}

	if ((ctx->src_queue_cnt > 0) &&
		((ctx->state == MFCINST_RUNNING) ||
		 (ctx->state == MFCINST_RUNNING_NO_OUTPUT))) {
		mb_entry = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);

		if (mb_entry->used) {
			list_del(&mb_entry->list);
			ctx->src_queue_cnt--;

			list_add_tail(&mb_entry->list, &enc->ref_queue);
			enc->ref_queue_cnt++;
		}
		/* slice_type = 4 && strm_size = 0, skipped enable
		   should be considered */
		if ((slice_type == -1) && (strm_size == 0))
			ctx->state = MFCINST_RUNNING_NO_OUTPUT;

		mfc_debug(2, "slice_type: %d, ctx->state: %d\n", slice_type, ctx->state);
		mfc_debug(2, "enc src count: %d, enc ref count: %d\n",
			  ctx->src_queue_cnt, enc->ref_queue_cnt);
	}

	if ((ctx->src_queue_cnt == 0) || (ctx->dst_queue_cnt == 0)) {
		spin_lock_irq(&dev->condlock);
		clear_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	return 0;
}

static struct s5p_mfc_codec_ops encoder_codec_ops = {
	.get_buf_update_val	= enc_get_buf_update_val,
	.init_ctx_ctrls		= enc_init_ctx_ctrls,
	.cleanup_ctx_ctrls	= enc_cleanup_ctx_ctrls,
	.init_buf_ctrls		= enc_init_buf_ctrls,
	.cleanup_buf_ctrls	= enc_cleanup_buf_ctrls,
	.to_buf_ctrls		= enc_to_buf_ctrls,
	.to_ctx_ctrls		= enc_to_ctx_ctrls,
	.set_buf_ctrls_val	= enc_set_buf_ctrls_val,
	.get_buf_ctrls_val	= enc_get_buf_ctrls_val,
	.recover_buf_ctrls_val	= enc_recover_buf_ctrls_val,
	.pre_seq_start		= enc_pre_seq_start,
	.post_seq_start		= enc_post_seq_start,
	.pre_frame_start	= enc_pre_frame_start,
	.post_frame_start	= enc_post_frame_start,
};

/* Query capabilities of the device */
static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	strncpy(cap->driver, "MFC", sizeof(cap->driver) - 1);
	strncpy(cap->card, "encoder", sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE
			| V4L2_CAP_VIDEO_OUTPUT
			| V4L2_CAP_VIDEO_CAPTURE_MPLANE
			| V4L2_CAP_VIDEO_OUTPUT_MPLANE
			| V4L2_CAP_STREAMING;

	return 0;
}

static int vidioc_enum_fmt(struct v4l2_fmtdesc *f, bool mplane, bool out)
{
	struct s5p_mfc_fmt *fmt;
	unsigned long i;
	int j = 0;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (mplane && formats[i].num_planes == 1)
			continue;
		else if (!mplane && formats[i].num_planes > 1)
			continue;
		if (out && formats[i].type != MFC_FMT_RAW)
			continue;
		else if (!out && formats[i].type != MFC_FMT_ENC)
			continue;

		if (j == f->index) {
			fmt = &formats[i];
			strlcpy(f->description, fmt->name,
				sizeof(f->description));
			f->pixelformat = fmt->fourcc;

			return 0;
		}

		++j;
	}

	return -EINVAL;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *pirv,
				   struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false, false);
}

static int vidioc_enum_fmt_vid_cap_mplane(struct file *file, void *pirv,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true, false);
}

static int vidioc_enum_fmt_vid_out(struct file *file, void *prov,
				   struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false, true);
}

static int vidioc_enum_fmt_vid_out_mplane(struct file *file, void *prov,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true, true);
}

static int vidioc_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	struct s5p_mfc_raw_info *raw;
	int i;

	mfc_debug_enter();

	mfc_debug(2, "f->type = %d ctx->state = %d\n", f->type, ctx->state);

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* This is run on output (encoder dest) */
		pix_fmt_mp->width = 0;
		pix_fmt_mp->height = 0;
		pix_fmt_mp->field = V4L2_FIELD_NONE;
		pix_fmt_mp->pixelformat = ctx->dst_fmt->fourcc;
		pix_fmt_mp->num_planes = ctx->dst_fmt->num_planes;

		pix_fmt_mp->plane_fmt[0].bytesperline = enc->dst_buf_size;
		pix_fmt_mp->plane_fmt[0].sizeimage = (unsigned int)(enc->dst_buf_size);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/* This is run on capture (encoder src) */
		raw = &ctx->raw_buf;

		pix_fmt_mp->width = ctx->img_width;
		pix_fmt_mp->height = ctx->img_height;
		pix_fmt_mp->field = V4L2_FIELD_NONE;
		pix_fmt_mp->pixelformat = ctx->src_fmt->fourcc;
		pix_fmt_mp->num_planes = ctx->src_fmt->num_planes;

		for (i = 0; i < raw->num_planes; i++) {
			pix_fmt_mp->plane_fmt[i].bytesperline = raw->stride[i];
			pix_fmt_mp->plane_fmt[i].sizeimage = raw->plane_size[i];
		}
	} else {
		mfc_err("invalid buf type\n");
		return -EINVAL;
	}

	mfc_debug_leave();

	return 0;
}

static int vidioc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct s5p_mfc_fmt *fmt;
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = find_format(f, MFC_FMT_ENC);
		if (!fmt) {
			mfc_err("failed to try output format\n");
			return -EINVAL;
		}

		if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
			mfc_err("must be set encoding output size\n");
			return -EINVAL;
		}

		pix_fmt_mp->plane_fmt[0].bytesperline =
			pix_fmt_mp->plane_fmt[0].sizeimage;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = find_format(f, MFC_FMT_RAW);
		if (!fmt) {
			mfc_err("failed to try output format\n");
			return -EINVAL;
		}

		if (fmt->num_planes != pix_fmt_mp->num_planes) {
			mfc_err("failed to try output format\n");
			return -EINVAL;
		}
	} else {
		mfc_err("invalid buf type\n");
		return -EINVAL;
	}

	return 0;
}

static int vidioc_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_fmt *fmt;
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	int ret = 0;

	mfc_debug_enter();

	ret = vidioc_try_fmt(file, priv, f);
	if (ret)
		return ret;

	if (ctx->vq_src.streaming || ctx->vq_dst.streaming) {
		v4l2_err(&dev->v4l2_dev, "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = find_format(f, MFC_FMT_ENC);
		if (!fmt) {
			mfc_err_ctx("failed to set capture format\n");
			return -EINVAL;
		}
		ctx->state = MFCINST_INIT;

		ctx->dst_fmt = fmt;
		ctx->codec_mode = ctx->dst_fmt->codec_mode;
		mfc_info_ctx("Enc output codec(%d) : %s\n",
				ctx->dst_fmt->codec_mode, ctx->dst_fmt->name);

		enc->dst_buf_size = pix_fmt_mp->plane_fmt[0].sizeimage;
		pix_fmt_mp->plane_fmt[0].bytesperline = 0;

		ctx->capture_state = QUEUE_FREE;

		s5p_mfc_alloc_instance_buffer(ctx);

		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		s5p_mfc_try_run(dev);
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_OPEN_INSTANCE_RET)) {
			s5p_mfc_cleanup_timeout(ctx);
			s5p_mfc_release_instance_buffer(ctx);
			ret = -EIO;
			goto out;
		}
		mfc_debug(2, "Got instance number: %d\n", ctx->inst_no);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = find_format(f, MFC_FMT_RAW);
		if (!fmt) {
			mfc_err_ctx("failed to set output format\n");
			return -EINVAL;
		}
		if (!IS_MFCV6(dev)) {
			if (fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) {
				mfc_err_ctx("Not supported format.\n");
				return -EINVAL;
			}
		} else if (IS_MFCV6(dev)) {
			if (fmt->fourcc == V4L2_PIX_FMT_NV12MT) {
				mfc_err_ctx("Not supported format.\n");
				return -EINVAL;
			}
		}

		if (fmt->num_planes != pix_fmt_mp->num_planes) {
			mfc_err_ctx("failed to set output format\n");
			ret = -EINVAL;
			goto out;
		}

		ctx->src_fmt = fmt;
		ctx->raw_buf.num_planes = ctx->src_fmt->num_planes;
		ctx->img_width = pix_fmt_mp->width;
		ctx->img_height = pix_fmt_mp->height;
		ctx->buf_stride = pix_fmt_mp->plane_fmt[0].bytesperline;

		mfc_info_ctx("Enc input pixelformat : %s\n", ctx->src_fmt->name);
		mfc_info_ctx("fmt - w: %d, h: %d, stride: %d\n",
			pix_fmt_mp->width, pix_fmt_mp->height, ctx->buf_stride);

		s5p_mfc_enc_calc_src_size(ctx);

		ctx->output_state = QUEUE_FREE;
	} else {
		mfc_err_ctx("invalid buf type\n");
		return -EINVAL;
	}
out:
	mfc_debug_leave();
	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
					  struct v4l2_requestbuffers *reqbufs)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;
	void *alloc_ctx;

	mfc_debug_enter();

	mfc_debug(2, "type: %d\n", reqbufs->memory);

	if ((reqbufs->memory != V4L2_MEMORY_MMAP) &&
		(reqbufs->memory != V4L2_MEMORY_USERPTR) &&
		(reqbufs->memory != V4L2_MEMORY_DMABUF))
		return -EINVAL;

	if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (reqbufs->count == 0) {
			mfc_debug(2, "Freeing buffers.\n");
			ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
			ctx->capture_state = QUEUE_FREE;
			return ret;
		}

		if (ctx->is_drm)
			alloc_ctx = ctx->dev->alloc_ctx_drm;
		else
			alloc_ctx =
				ctx->dev->alloc_ctx[MFC_BANK_A_ALLOC_CTX];

		if (ctx->capture_state != QUEUE_FREE) {
			mfc_err_ctx("invalid capture state: %d\n", ctx->capture_state);
			return -EINVAL;
		}

		if (ctx->cacheable & MFCMASK_DST_CACHE)
			s5p_mfc_mem_set_cacheable(alloc_ctx, true);

		ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
		if (ret) {
			mfc_err_ctx("error in vb2_reqbufs() for E(D)\n");
			s5p_mfc_mem_set_cacheable(alloc_ctx, false);
			return ret;
		}

		s5p_mfc_mem_set_cacheable(alloc_ctx, false);
		ctx->capture_state = QUEUE_BUFS_REQUESTED;

		if (!IS_MFCV6(dev)) {
			ret = s5p_mfc_alloc_codec_buffers(ctx);
			if (ret) {
				mfc_err_ctx("Failed to allocate encoding buffers.\n");
				reqbufs->count = 0;
				vb2_reqbufs(&ctx->vq_dst, reqbufs);
				return -ENOMEM;
			}
		}
	} else if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (reqbufs->count == 0) {
			mfc_debug(2, "Freeing buffers.\n");
			ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
			ctx->output_state = QUEUE_FREE;
			return ret;
		}

		if (ctx->is_drm) {
			alloc_ctx = ctx->dev->alloc_ctx_drm;
		} else {
			alloc_ctx = (!IS_MFCV6(dev)) ?
				ctx->dev->alloc_ctx[MFC_BANK_B_ALLOC_CTX] :
				ctx->dev->alloc_ctx[MFC_BANK_A_ALLOC_CTX];
		}

		if (ctx->output_state != QUEUE_FREE) {
			mfc_err_ctx("invalid output state: %d\n", ctx->output_state);
			return -EINVAL;
		}

		if (ctx->cacheable & MFCMASK_SRC_CACHE)
			s5p_mfc_mem_set_cacheable(alloc_ctx, true);

		ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
		if (ret) {
			mfc_err_ctx("error in vb2_reqbufs() for E(S)\n");
			s5p_mfc_mem_set_cacheable(alloc_ctx, false);
			return ret;
		}

		s5p_mfc_mem_set_cacheable(alloc_ctx, false);
		ctx->output_state = QUEUE_BUFS_REQUESTED;
	} else {
		mfc_err_ctx("invalid buf type\n");
		return -EINVAL;
	}

	mfc_debug_leave();

	return ret;
}

static int vidioc_querybuf(struct file *file, void *priv,
						   struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;

	mfc_debug_enter();

	mfc_debug(2, "type: %d\n", buf->memory);

	if ((buf->memory != V4L2_MEMORY_MMAP) &&
		(buf->memory != V4L2_MEMORY_USERPTR) &&
		(buf->memory != V4L2_MEMORY_DMABUF))
		return -EINVAL;

	mfc_debug(2, "state: %d, buf->type: %d\n", ctx->state, buf->type);

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (ctx->state != MFCINST_GOT_INST) {
			mfc_err("invalid context state: %d\n", ctx->state);
			return -EINVAL;
		}

		ret = vb2_querybuf(&ctx->vq_dst, buf);
		if (ret != 0) {
			mfc_err("error in vb2_querybuf() for E(D)\n");
			return ret;
		}
		buf->m.planes[0].m.mem_offset += DST_QUEUE_OFF_BASE;

	} else if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_querybuf(&ctx->vq_src, buf);
		if (ret != 0) {
			mfc_err("error in vb2_querybuf() for E(S)\n");
			return ret;
		}
	} else {
		mfc_err("invalid buf type\n");
		return -EINVAL;
	}

	mfc_debug_leave();

	return ret;
}

/* Queue a buffer */
static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = -EINVAL;

	mfc_debug_enter();
	mfc_debug(2, "Enqueued buf: %d (type = %d)\n", buf->index, buf->type);
	if (ctx->state == MFCINST_ERROR) {
		mfc_err_ctx("Call on QBUF after unrecoverable error.\n");
		return -EIO;
	}
	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_qbuf(&ctx->vq_src, buf);
		if (!ret) {
			mfc_debug(7, "timestamp: %ld %ld\n",
					buf->timestamp.tv_sec,
					buf->timestamp.tv_usec);
			mfc_debug(7, "qos ratio: %d\n", ctx->qos_ratio);
			ctx->last_framerate = (ctx->qos_ratio *
						get_framerate(&buf->timestamp,
						&ctx->last_timestamp)) / 100;
			memcpy(&ctx->last_timestamp, &buf->timestamp,
				sizeof(struct timeval));

#ifdef MFC_ENC_AVG_FPS_MODE
			if (ctx->src_queue_cnt > 0) {
				if (ctx->frame_count > ENC_AVG_FRAMES &&
					ctx->framerate == ENC_MAX_FPS) {
					if (ctx->is_max_fps)
						goto out;
					else
						goto calc_again;
				}

				ctx->frame_count++;
				ctx->avg_framerate =
					(ctx->avg_framerate *
						(ctx->frame_count - 1) +
						ctx->last_framerate) /
						ctx->frame_count;

				if (ctx->frame_count < ENC_AVG_FRAMES)
					goto out;

				if (ctx->avg_framerate > ENC_HIGH_FPS) {
					if (ctx->frame_count == ENC_AVG_FRAMES) {
						mfc_debug(2, "force fps: %d\n", ENC_MAX_FPS);
						ctx->is_max_fps = 1;
					}
					goto out;
				}
			} else {
				ctx->last_framerate = ENC_MAX_FPS;
				mfc_debug(2, "fps set to %d\n", ctx->last_framerate);
			}
calc_again:
#endif
			if (ctx->last_framerate != 0 &&
				ctx->last_framerate != ctx->framerate) {
				mfc_debug(2, "fps changed: %d -> %d\n",
					ctx->framerate, ctx->last_framerate);
				ctx->framerate = ctx->last_framerate;
				s5p_mfc_qos_on(ctx);
			}
		}
	} else {
		ret = vb2_qbuf(&ctx->vq_dst, buf);
	}

out:
	mfc_debug_leave();
	return ret;
}

/* Dequeue a buffer */
static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret;

	mfc_debug_enter();
	mfc_debug(2, "Addr: %p %p %p Type: %d\n", &ctx->vq_src, buf, buf->m.planes,
								buf->type);
	if (ctx->state == MFCINST_ERROR) {
		mfc_err_ctx("Call on DQBUF after unrecoverable error.\n");
		return -EIO;
	}
	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		ret = vb2_dqbuf(&ctx->vq_src, buf, file->f_flags & O_NONBLOCK);
	else
		ret = vb2_dqbuf(&ctx->vq_dst, buf, file->f_flags & O_NONBLOCK);
	mfc_debug_leave();
	return ret;
}

/* Stream on */
static int vidioc_streamon(struct file *file, void *priv,
			   enum v4l2_buf_type type)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = -EINVAL;

	mfc_debug_enter();
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_streamon(&ctx->vq_src, type);

		if (!ret) {
			ctx->frame_count = 0;
			ctx->is_max_fps = 0;
			ctx->avg_framerate = 0;
			s5p_mfc_qos_on(ctx);
		}
	} else {
		ret = vb2_streamon(&ctx->vq_dst, type);
	}
	mfc_debug(2, "ctx->src_queue_cnt = %d ctx->state = %d "
		  "ctx->dst_queue_cnt = %d ctx->dpb_count = %d\n",
		  ctx->src_queue_cnt, ctx->state, ctx->dst_queue_cnt,
		  ctx->dpb_count);
	mfc_debug_leave();
	return ret;
}

/* Stream off, which equals to a pause */
static int vidioc_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type type)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret;

	mfc_debug_enter();
	ret = -EINVAL;
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->last_framerate = 0;
		memset(&ctx->last_timestamp, 0, sizeof(struct timeval));
		ret = vb2_streamoff(&ctx->vq_src, type);
		if (!ret)
			s5p_mfc_qos_off(ctx);
	} else {
		ret = vb2_streamoff(&ctx->vq_dst, type);
	}

	mfc_debug(2, "streamoff\n");
	mfc_debug_leave();

	return ret;
}

/* Query a ctrl */
static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	struct v4l2_queryctrl *c;

	c = get_ctrl(qc->id);
	if (!c)
		return -EINVAL;
	*qc = *c;
	return 0;
}

static int enc_ext_info(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int val = 0;

	if (IS_MFCv7X(dev) || IS_MFCV8(dev)) {
		val |= ENC_SET_RGB_INPUT;
		val |= ENC_SET_SPARE_SIZE;
	}
	if (FW_HAS_TEMPORAL_SVC_CH(dev))
		val |= ENC_SET_TEMP_SVC_CH;

	return val;
}

static int get_ctrl_val(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	int ret = 0;
	int found = 0;

	switch (ctrl->id) {
	case V4L2_CID_CACHEABLE:
		ctrl->value = ctx->cacheable;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_STREAM_SIZE:
		ctrl->value = enc->dst_buf_size;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_COUNT:
		ctrl->value = enc->frame_count;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TYPE:
		ctrl->value = enc->frame_type;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_CHECK_STATE:
		if (ctx->state == MFCINST_RUNNING_NO_OUTPUT)
			ctrl->value = MFCSTATE_ENC_NO_OUTPUT;
		else
			ctrl->value = MFCSTATE_PROCESSING;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
	case V4L2_CID_MPEG_MFC51_VIDEO_LUMA_ADDR:
	case V4L2_CID_MPEG_MFC51_VIDEO_CHROMA_ADDR:
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS:
		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & MFC_CTRL_TYPE_GET))
				continue;

			if (ctx_ctrl->id == ctrl->id) {
				if (ctx_ctrl->has_new) {
					ctx_ctrl->has_new = 0;
					ctrl->value = ctx_ctrl->val;
				} else {
					mfc_debug(8, "Control value "\
							"is not up to date: "\
							"0x%08x\n", ctrl->id);
					return -EINVAL;
				}

				found = 1;
				break;
			}
		}

		if (!found) {
			v4l2_err(&dev->v4l2_dev, "Invalid control: 0x%08x\n",
					ctrl->id);
			return -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MFC_GET_VERSION_INFO:
		ctrl->value = mfc_version(dev);
		break;
	case V4L2_CID_MPEG_MFC_GET_EXTRA_BUFFER_SIZE:
		ctrl->value = mfc_linear_buf_size(mfc_version(dev));
		break;
	case V4L2_CID_MPEG_VIDEO_QOS_RATIO:
		ctrl->value = ctx->qos_ratio;
		break;
	case V4L2_CID_MPEG_MFC_GET_EXT_INFO:
		ctrl->value = enc_ext_info(ctx);
		break;
	default:
		v4l2_err(&dev->v4l2_dev, "Invalid control: 0x%08x\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;

	mfc_debug_enter();
	ret = get_ctrl_val(ctx, ctrl);
	mfc_debug_leave();

	return ret;
}

static inline int h264_level(enum v4l2_mpeg_video_h264_level lvl)
{
	static unsigned int t[V4L2_MPEG_VIDEO_H264_LEVEL_5_1 + 1] = {
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1_0   */ 10,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1B    */ 9,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1_1   */ 11,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1_2   */ 12,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1_3   */ 13,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_2_0   */ 20,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_2_1   */ 21,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_2_2   */ 22,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_3_0   */ 30,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_3_1   */ 31,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_3_2   */ 32,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_4_0   */ 40,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_4_1   */ 41,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_4_2   */ 42,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_5_0   */ 50,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_5_1   */ 51,
	};
	return t[lvl];
}

static inline int mpeg4_level(enum v4l2_mpeg_video_mpeg4_level lvl)
{
	static unsigned int t[V4L2_MPEG_VIDEO_MPEG4_LEVEL_6 + 1] = {
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_0	     */ 0,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_0B, Simple    */ 9,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_1	     */ 1,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_2	     */ 2,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_3	     */ 3,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_3B, Advanced  */ 7,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_4	     */ 4,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_5	     */ 5,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_6,  Simple    */ 6,
	};
	return t[lvl];
}

static inline int vui_sar_idc(enum v4l2_mpeg_video_h264_vui_sar_idc sar)
{
	static unsigned int t[V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_EXTENDED + 1] = {
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_UNSPECIFIED     */ 0,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_1x1             */ 1,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_12x11           */ 2,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_10x11           */ 3,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_16x11           */ 4,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_40x33           */ 5,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_24x11           */ 6,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_20x11           */ 7,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_32x11           */ 8,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_80x33           */ 9,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_18x11           */ 10,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_15x11           */ 11,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_64x33           */ 12,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_160x99          */ 13,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_4x3             */ 14,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_3x2             */ 15,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_2x1             */ 16,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_EXTENDED        */ 255,
	};
	return t[sar];
}

static int set_enc_param(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		p->gop_size = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		p->slice_mode =
			(enum v4l2_mpeg_video_multi_slice_mode)ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		p->slice_mb = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
		p->slice_bit = ctrl->value * 8;
		break;
	case V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB:
		p->intra_refresh_mb = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_PADDING:
		p->pad = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_PADDING_YUV:
		p->pad_luma = (ctrl->value >> 16) & 0xff;
		p->pad_cb = (ctrl->value >> 8) & 0xff;
		p->pad_cr = (ctrl->value >> 0) & 0xff;
		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
		p->rc_frame = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		p->rc_bitrate = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF:
		p->rc_reaction_coeff = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
		enc->force_frame_type = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VBV_SIZE:
		p->vbv_buf_size = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		p->seq_hdr_mode =
			(enum v4l2_mpeg_video_header_mode)(ctrl->value);
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE:
		p->frame_skip_mode =
			(enum v4l2_mpeg_mfc51_video_frame_skip_mode)
			(ctrl->value);
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT: /* MFC5.x Only */
		p->fixed_target_bit = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
		p->num_b_frame = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		switch ((enum v4l2_mpeg_video_h264_profile)(ctrl->value)) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
			p->codec.h264.profile =
				S5P_FIMV_ENC_PROFILE_H264_MAIN;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
			p->codec.h264.profile =
				S5P_FIMV_ENC_PROFILE_H264_HIGH;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			p->codec.h264.profile =
				S5P_FIMV_ENC_PROFILE_H264_BASELINE;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
			if (IS_MFCV6(dev))
				p->codec.h264.profile =
				S5P_FIMV_ENC_PROFILE_H264_CONSTRAINED_BASELINE;
			else
				ret = -EINVAL;
			break;
		default:
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		p->codec.h264.level =
		h264_level((enum v4l2_mpeg_video_h264_level)(ctrl->value));
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_INTERLACE:
		p->codec.h264.interlace = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
		p->codec.h264.loop_filter_mode =
		(enum v4l2_mpeg_video_h264_loop_filter_mode)(ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA:
		p->codec.h264.loop_filter_alpha = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA:
		p->codec.h264.loop_filter_beta = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		p->codec.h264.entropy_mode =
			(enum v4l2_mpeg_video_h264_entropy_mode)(ctrl->value);
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_NUM_REF_PIC_FOR_P:
		p->codec.h264.num_ref_pic_4p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
		p->codec.h264._8x8_transform = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
		p->rc_mb = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_RC_FRAME_RATE:
		p->codec.h264.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
		p->codec.h264.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
		p->codec.h264.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		p->codec.h264.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_DARK:
		p->codec.h264.rc_mb_dark = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_SMOOTH:
		p->codec.h264.rc_mb_smooth = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_STATIC:
		p->codec.h264.rc_mb_static = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_ACTIVITY:
		p->codec.h264.rc_mb_activity = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
		p->codec.h264.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
		p->codec.h264.rc_b_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE:
		p->codec.h264.ar_vui = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_IDC:
		p->codec.h264.ar_vui_idc =
		vui_sar_idc((enum v4l2_mpeg_video_h264_vui_sar_idc)(ctrl->value));
		break;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH:
		p->codec.h264.ext_sar_width = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT:
		p->codec.h264.ext_sar_height = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_GOP_CLOSURE:
		p->codec.h264.open_gop = ctrl->value ? 0 : 1;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
		p->codec.h264.open_gop_size = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING:
		p->codec.h264.hier_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE:
		p->codec.h264.hier_qp_type =
		(enum v4l2_mpeg_video_h264_hierarchical_coding_type)(ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER:
		p->codec.h264.hier_qp_layer = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_QP:
		p->codec.h264.hier_qp_layer_qp[(ctrl->value >> 16) & 0x7]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT0:
		p->codec.h264.hier_qp_layer_bit[0] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT1:
		p->codec.h264.hier_qp_layer_bit[1] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT2:
		p->codec.h264.hier_qp_layer_bit[2] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT3:
		p->codec.h264.hier_qp_layer_bit[3] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT4:
		p->codec.h264.hier_qp_layer_bit[4] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT5:
		p->codec.h264.hier_qp_layer_bit[5] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT6:
		p->codec.h264.hier_qp_layer_bit[6] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING:
		p->codec.h264.sei_gen_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FP_CURRENT_FRAME_0:
		p->codec.h264.sei_fp_curr_frame_0 = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FP_ARRANGEMENT_TYPE:
		p->codec.h264.sei_fp_arrangement_type = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO:
		p->codec.h264.fmo_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_MAP_TYPE:
		switch ((enum v4l2_mpeg_video_h264_fmo_map_type)(ctrl->value)) {
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_INTERLEAVED_SLICES:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_SCATTERED_SLICES:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_RASTER_SCAN:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_WIPE_SCAN:
			p->codec.h264.fmo_slice_map_type = ctrl->value;
			break;
		default:
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_SLICE_GROUP:
		p->codec.h264.fmo_slice_num_grp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_RUN_LENGTH:
		p->codec.h264.fmo_run_length[(ctrl->value >> 30) & 0x3]
			= ctrl->value & 0x3FFFFFFF;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_CHANGE_DIRECTION:
		p->codec.h264.fmo_sg_dir = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_CHANGE_RATE:
		p->codec.h264.fmo_sg_rate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ASO:
		p->codec.h264.aso_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ASO_SLICE_ORDER:
		p->codec.h264.aso_slice_order[(ctrl->value >> 18) & 0x7]
			&= ~(0xFF << (((ctrl->value >> 16) & 0x3) << 3));
		p->codec.h264.aso_slice_order[(ctrl->value >> 18) & 0x7]
			|= (ctrl->value & 0xFF) << \
				(((ctrl->value >> 16) & 0x3) << 3);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PREPEND_SPSPPS_TO_IDR:
		p->codec.h264.prepend_sps_pps_to_idr = ctrl->value ? 1 : 0;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		switch ((enum v4l2_mpeg_video_mpeg4_profile)(ctrl->value)) {
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE:
			p->codec.mpeg4.profile =
				S5P_FIMV_ENC_PROFILE_MPEG4_SIMPLE;
			break;
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE:
			p->codec.mpeg4.profile =
			S5P_FIMV_ENC_PROFILE_MPEG4_ADVANCED_SIMPLE;
			break;
		default:
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		p->codec.mpeg4.level =
		mpeg4_level((enum v4l2_mpeg_video_mpeg4_level)(ctrl->value));
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP:
		p->codec.mpeg4.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
		p->codec.mpeg4.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
		p->codec.mpeg4.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_QPEL:
		p->codec.mpeg4.quarter_pixel = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP:
		p->codec.mpeg4.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_B_FRAME_QP:
		p->codec.mpeg4.rc_b_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_MPEG4_VOP_TIME_RES:
		p->codec.mpeg4.vop_time_res = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_MPEG4_VOP_FRM_DELTA:
		p->codec.mpeg4.vop_frm_delta = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H263_RC_FRAME_RATE:
		p->codec.mpeg4.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP:
		p->codec.mpeg4.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
		p->codec.mpeg4.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
		p->codec.mpeg4.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP:
		p->codec.mpeg4.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_VERSION:
		p->codec.vp8.vp8_version = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_RC_FRAME_RATE:
		p->codec.vp8.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP:
		p->codec.vp8.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP:
		p->codec.vp8.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_I_FRAME_QP:
		p->codec.vp8.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_P_FRAME_QP:
		p->codec.vp8.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_NUM_OF_PARTITIONS:
		p->codec.vp8.vp8_numberofpartitions = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_FILTER_LEVEL:
		p->codec.vp8.vp8_filterlevel = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_FILTER_SHARPNESS:
		p->codec.vp8.vp8_filtersharpness = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_GOLDEN_FRAMESEL:
		p->codec.vp8.vp8_goldenframesel = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_GF_REFRESH_PERIOD:
		p->codec.vp8.vp8_gfrefreshperiod = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_ENABLE:
		p->codec.vp8.hierarchy_qp_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER0:
		p->codec.vp8.hier_qp_layer_qp[(ctrl->value >> 16) & 0x3]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER1:
		p->codec.vp8.hier_qp_layer_qp[(ctrl->value >> 16) & 0x3]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER2:
		p->codec.vp8.hier_qp_layer_qp[(ctrl->value >> 16) & 0x3]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT0:
		p->codec.vp8.hier_qp_layer_bit[0] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT1:
		p->codec.vp8.hier_qp_layer_bit[1] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT2:
		p->codec.vp8.hier_qp_layer_bit[2] = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_REF_NUMBER_FOR_PFRAMES:
		p->codec.vp8.num_refs_for_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_DISABLE_INTRA_MD4X4:
		p->codec.vp8.intra_4x4mode_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_NUM_TEMPORAL_LAYER:
		p->codec.vp8.num_temporal_layer = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP:
		p->codec.hevc.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP:
		p->codec.hevc.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP:
		p->codec.hevc.rc_b_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_RC_FRAME_RATE:
		p->codec.hevc.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP:
		p->codec.hevc.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP:
		p->codec.hevc.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		p->codec.hevc.level = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_DARK:
		p->codec.hevc.rc_lcu_dark = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_SMOOTH:
		p->codec.hevc.rc_lcu_smooth = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_STATIC:
		p->codec.hevc.rc_lcu_static = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_ACTIVITY:
		p->codec.hevc.rc_lcu_activity = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_TIER_FLAG:
		p->codec.hevc.tier_flag = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_MAX_PARTITION_DEPTH:
		p->codec.hevc.max_partition_depth = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REF_NUMBER_FOR_PFRAMES:
		p->codec.hevc.num_refs_for_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REFRESH_TYPE:
		p->codec.hevc.refreshtype = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_CONST_INTRA_PRED_ENABLE:
		p->codec.hevc.const_intra_period_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LOSSLESS_CU_ENABLE:
		p->codec.hevc.lossless_cu_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_WAVEFRONT_ENABLE:
		p->codec.hevc.wavefront_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_DISABLE:
		p->codec.hevc.loopfilter_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_SLICE_BOUNDARY:
		p->codec.hevc.loopfilter_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LTR_ENABLE:
		p->codec.hevc.longterm_ref_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_QP_ENABLE:
		p->codec.hevc.hier_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_TYPE:
		p->codec.hevc.hier_qp_type = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER:
		p->codec.hevc.hier_qp_layer = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_QP:
		p->codec.hevc.hier_qp_layer_qp = ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT:
		p->codec.hevc.hier_qp_layer_bit = ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_SIGN_DATA_HIDING:
		p->codec.hevc.sign_data_hiding = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_GENERAL_PB_ENABLE:
		p->codec.hevc.general_pb_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_TEMPORAL_ID_ENABLE:
		p->codec.hevc.temporal_id_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_STRONG_SMOTHING_FLAG:
		p->codec.hevc.strong_intra_smooth = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_DISABLE_INTRA_PU_SPLIT:
		p->codec.hevc.intra_pu_split_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_DISABLE_TMV_PREDICTION:
		p->codec.hevc.tmv_prediction_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_MAX_NUM_MERGE_MV_MINUS1:
		p->codec.hevc.max_num_merge_mv = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_WITHOUT_STARTCODE_ENABLE:
		p->codec.hevc.encoding_nostartcode_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REFRESH_PERIOD:
		p->codec.hevc.refreshperiod = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_QP_INDEX_CR:
		p->codec.hevc.croma_qp_offset_cr = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_QP_INDEX_CB:
		p->codec.hevc.croma_qp_offset_cb = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_BETA_OFFSET_DIV2:
		p->codec.hevc.lf_beta_offset_div2 = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_TC_OFFSET_DIV2:
		p->codec.hevc.lf_tc_offset_div2 = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD:
		p->codec.hevc.size_of_length_field = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_USER_REF:
		p->codec.hevc.user_ref = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_STORE_REF:
		p->codec.hevc.store_ref = ctrl->value;
		break;
	default:
		v4l2_err(&dev->v4l2_dev, "Invalid control\n");
		ret = -EINVAL;
	}

	return ret;
}

static int process_user_shared_handle_enc(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	int ret = 0;

	enc->sh_handle.ion_handle =
		ion_import_dma_buf(dev->mfc_ion_client, enc->sh_handle.fd);
	if (IS_ERR(enc->sh_handle.ion_handle)) {
		mfc_err_ctx("Failed to import fd\n");
		ret = PTR_ERR(enc->sh_handle.ion_handle);
		goto import_dma_fail;
	}

	enc->sh_handle.virt =
		ion_map_kernel(dev->mfc_ion_client, enc->sh_handle.ion_handle);
	if (enc->sh_handle.virt == NULL) {
		mfc_err_ctx("Failed to get kernel virtual address\n");
		ret = -EINVAL;
		goto map_kernel_fail;
	}

	mfc_debug(2, "User Handle: fd = %d, virt = 0x%p\n",
				enc->sh_handle.fd, enc->sh_handle.virt);

	return 0;

map_kernel_fail:
	ion_free(dev->mfc_ion_client, enc->sh_handle.ion_handle);

import_dma_fail:
	return ret;
}


int enc_cleanup_user_shared_handle(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;

	if (enc->sh_handle.fd == -1)
		return 0;

	if (enc->sh_handle.virt)
		ion_unmap_kernel(dev->mfc_ion_client,
					enc->sh_handle.ion_handle);

	ion_free(dev->mfc_ion_client, enc->sh_handle.ion_handle);

	return 0;
}


static int set_ctrl_val(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{

	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	int ret = 0;
	int found = 0;

	switch (ctrl->id) {
	case V4L2_CID_CACHEABLE:
		if (ctrl->value)
			ctx->cacheable |= ctrl->value;
		else
			ctx->cacheable = 0;
		break;
	case V4L2_CID_MPEG_VIDEO_QOS_RATIO:
		ctx->qos_ratio = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP:
		ctx->qp_max_change = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP:
		ctx->qp_min_change = ctrl->value;
		ctrl->value = ((ctx->qp_max_change << 8)
			| (ctx->qp_min_change));
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
	case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
	case V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH:
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH:
	case V4L2_CID_MPEG_MFC51_VIDEO_BIT_RATE_CH:
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH:
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH:
		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & MFC_CTRL_TYPE_SET))
				continue;

			if (ctx_ctrl->id == ctrl->id) {
				ctx_ctrl->has_new = 1;
				ctx_ctrl->val = ctrl->value;
				if (ctx_ctrl->id == \
					V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH)
					ctx_ctrl->val *= p->rc_frame_delta;

				if (((ctx_ctrl->id == \
					V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH) ||
					(ctx_ctrl->id == \
					V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH)) &&
					(enc->sh_handle.fd == -1)) {
						enc->sh_handle.fd = ctrl->value;
						if (process_user_shared_handle_enc(ctx)) {
							enc->sh_handle.fd = -1;
							return -EINVAL;
						}
				}
				found = 1;
				break;
			}
		}

		if (!found) {
			v4l2_err(&dev->v4l2_dev, "Invalid control 0x%08x\n",
					ctrl->id);
			return -EINVAL;
		}
		break;
	default:
		ret = set_enc_param(ctx, ctrl);
		break;
	}

	return ret;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;

	mfc_debug_enter();

	ret = check_ctrl_val(ctx, ctrl);
	if (ret != 0)
		return ret;

	ret = set_ctrl_val(ctx, ctrl);

	mfc_debug_leave();

	return ret;
}

static int vidioc_g_ext_ctrls(struct file *file, void *priv,
			      struct v4l2_ext_controls *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	if (f->ctrl_class != V4L2_CTRL_CLASS_MPEG)
		return -EINVAL;

	for (i = 0; i < f->count; i++) {
		ext_ctrl = (f->controls + i);

		ctrl.id = ext_ctrl->id;

		ret = get_ctrl_val(ctx, &ctrl);
		if (ret == 0) {
			ext_ctrl->value = ctrl.value;
		} else {
			f->error_idx = i;
			break;
		}

		mfc_debug(2, "[%d] id: 0x%08x, value: %d", i, ext_ctrl->id, ext_ctrl->value);
	}

	return ret;
}

static int vidioc_s_ext_ctrls(struct file *file, void *priv,
				struct v4l2_ext_controls *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	if (f->ctrl_class != V4L2_CTRL_CLASS_MPEG)
		return -EINVAL;

	for (i = 0; i < f->count; i++) {
		ext_ctrl = (f->controls + i);

		ctrl.id = ext_ctrl->id;
		ctrl.value = ext_ctrl->value;

		ret = check_ctrl_val(ctx, &ctrl);
		if (ret != 0) {
			f->error_idx = i;
			break;
		}

		ret = set_enc_param(ctx, &ctrl);
		if (ret != 0) {
			f->error_idx = i;
			break;
		}

		mfc_debug(2, "[%d] id: 0x%08x, value: %d", i, ext_ctrl->id, ext_ctrl->value);
	}

	return ret;
}

static int vidioc_try_ext_ctrls(struct file *file, void *priv,
				struct v4l2_ext_controls *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	if (f->ctrl_class != V4L2_CTRL_CLASS_MPEG)
		return -EINVAL;

	for (i = 0; i < f->count; i++) {
		ext_ctrl = (f->controls + i);

		ctrl.id = ext_ctrl->id;
		ctrl.value = ext_ctrl->value;

		ret = check_ctrl_val(ctx, &ctrl);
		if (ret != 0) {
			f->error_idx = i;
			break;
		}

		mfc_debug(2, "[%d] id: 0x%08x, value: %d", i, ext_ctrl->id, ext_ctrl->value);
	}

	return ret;
}

static const struct v4l2_ioctl_ops s5p_mfc_enc_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_cap_mplane = vidioc_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out = vidioc_enum_fmt_vid_out,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_enum_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane = vidioc_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = vidioc_g_fmt,
	.vidioc_try_fmt_vid_cap_mplane = vidioc_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = vidioc_try_fmt,
	.vidioc_s_fmt_vid_cap_mplane = vidioc_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = vidioc_s_fmt,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_queryctrl = vidioc_queryctrl,
	.vidioc_g_ctrl = vidioc_g_ctrl,
	.vidioc_s_ctrl = vidioc_s_ctrl,
	.vidioc_g_ext_ctrls = vidioc_g_ext_ctrls,
	.vidioc_s_ext_ctrls = vidioc_s_ext_ctrls,
	.vidioc_try_ext_ctrls = vidioc_try_ext_ctrls,
};

static int check_vb_with_fmt(struct s5p_mfc_fmt *fmt, struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	int i;

	if (!fmt)
		return -EINVAL;

	if (fmt->num_planes != vb->num_planes) {
		mfc_err("invalid plane number for the format\n");
		return -EINVAL;
	}

	for (i = 0; i < fmt->num_planes; i++) {
		if (!s5p_mfc_mem_plane_addr(ctx, vb, i)) {
			mfc_err("failed to get plane cookie\n");
			return -EINVAL;
		}

		mfc_debug(2, "index: %d, plane[%d] cookie: 0x%08lx",
			vb->v4l2_buf.index, i,
			(unsigned long)s5p_mfc_mem_plane_addr(ctx, vb, i));
	}

	return 0;
}

static int s5p_mfc_queue_setup(struct vb2_queue *vq,
				const struct v4l2_format *fmt,
				unsigned int *buf_count, unsigned int *plane_count,
				unsigned int psize[], void *allocators[])
{
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_raw_info *raw;
	int i;
	void *alloc_ctx1 = ctx->dev->alloc_ctx[MFC_BANK_A_ALLOC_CTX];
	void *alloc_ctx2 = ctx->dev->alloc_ctx[MFC_BANK_B_ALLOC_CTX];
	void *output_ctx;

	mfc_debug_enter();

	if (ctx->state != MFCINST_GOT_INST &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		mfc_err_ctx("invalid state: %d\n", ctx->state);
		return -EINVAL;
	}
	if (ctx->state >= MFCINST_FINISHING &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		mfc_err_ctx("invalid state: %d\n", ctx->state);
		return -EINVAL;
	}

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (ctx->dst_fmt)
			*plane_count = ctx->dst_fmt->num_planes;
		else
			*plane_count = MFC_ENC_CAP_PLANE_COUNT;

		if (*buf_count < 1)
			*buf_count = 1;
		if (*buf_count > MFC_MAX_BUFFERS)
			*buf_count = MFC_MAX_BUFFERS;

		psize[0] = (unsigned int)(enc->dst_buf_size);
		if (ctx->is_drm)
			allocators[0] = ctx->dev->alloc_ctx_drm;
		else
			allocators[0] = alloc_ctx1;
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		raw = &ctx->raw_buf;

		if (ctx->src_fmt)
			*plane_count = ctx->src_fmt->num_planes;
		else
			*plane_count = MFC_ENC_OUT_PLANE_COUNT;

		if (*buf_count < 1)
			*buf_count = 1;
		if (*buf_count > MFC_MAX_BUFFERS)
			*buf_count = MFC_MAX_BUFFERS;

		if (ctx->is_drm) {
			output_ctx = ctx->dev->alloc_ctx_drm;
		} else {
			if (IS_MFCV6(dev))
				output_ctx = alloc_ctx1;
			else
				output_ctx = alloc_ctx2;
		}

		for (i = 0; i < *plane_count; i++) {
			psize[i] = raw->plane_size[i];
			allocators[i] = output_ctx;
		}
	} else {
		mfc_err_ctx("invalid queue type: %d\n", vq->type);
		return -EINVAL;
	}

	mfc_debug(2, "buf_count: %d, plane_count: %d\n", *buf_count, *plane_count);
	for (i = 0; i < *plane_count; i++)
		mfc_debug(2, "plane[%d] size=%d\n", i, psize[i]);

	mfc_debug_leave();

	return 0;
}

static void s5p_mfc_unlock(struct vb2_queue *q)
{
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;

	mutex_unlock(&dev->mfc_mutex);
}

static void s5p_mfc_lock(struct vb2_queue *q)
{
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;

	mutex_lock(&dev->mfc_mutex);
}

static int s5p_mfc_buf_init(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_buf *buf = vb_to_mfc_buf(vb);
	int i, ret;

	mfc_debug_enter();

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = check_vb_with_fmt(ctx->dst_fmt, vb);
		if (ret < 0)
			return ret;

		buf->planes.stream = s5p_mfc_mem_plane_addr(ctx, vb, 0);

		if (call_cop(ctx, init_buf_ctrls, ctx, MFC_CTRL_TYPE_DST,
					vb->v4l2_buf.index) < 0)
			mfc_err_ctx("failed in init_buf_ctrls\n");

	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = check_vb_with_fmt(ctx->src_fmt, vb);
		if (ret < 0)
			return ret;

		for (i = 0; i < ctx->src_fmt->num_planes; i++)
			buf->planes.raw[i] = s5p_mfc_mem_plane_addr(ctx, vb, i);

		if (call_cop(ctx, init_buf_ctrls, ctx, MFC_CTRL_TYPE_SRC,
					vb->v4l2_buf.index) < 0)
			mfc_err_ctx("failed in init_buf_ctrls\n");

	} else {
		mfc_err_ctx("inavlid queue type: %d\n", vq->type);
		return -EINVAL;
	}

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_raw_info *raw;
	unsigned int index = vb->v4l2_buf.index;
	int ret, i;

	mfc_debug_enter();

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = check_vb_with_fmt(ctx->dst_fmt, vb);
		if (ret < 0)
			return ret;

		mfc_debug(2, "plane size: %lu, dst size: %zu\n",
			vb2_plane_size(vb, 0), enc->dst_buf_size);

		if (vb2_plane_size(vb, 0) < enc->dst_buf_size) {
			mfc_err_ctx("plane size is too small for capture\n");
			return -EINVAL;
		}
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = check_vb_with_fmt(ctx->src_fmt, vb);
		if (ret < 0)
			return ret;

		raw = &ctx->raw_buf;
		for (i = 0; i < raw->num_planes; i++) {
			mfc_debug(2, "plane[%d] size: %ld, src[%d] size: %d\n",
				i, vb2_plane_size(vb, i),
				i, raw->plane_size[i]);
			if (vb2_plane_size(vb, i) < raw->plane_size[i]) {
				mfc_err_ctx("Output plane[%d] is too smalli\n", i);
				mfc_err_ctx("plane size: %ld, src size: %d\n",
						vb2_plane_size(vb, i),
						raw->plane_size[i]);
				return -EINVAL;
			}
		}

		if (call_cop(ctx, to_buf_ctrls, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err_ctx("failed in to_buf_ctrls\n");
	} else {
		mfc_err_ctx("inavlid queue type: %d\n", vq->type);
		return -EINVAL;
	}

	s5p_mfc_mem_prepare(vb);

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	unsigned int index = vb->v4l2_buf.index;


	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (call_cop(ctx, to_ctx_ctrls, ctx, &ctx->dst_ctrls[index]) < 0)
			mfc_err_ctx("failed in to_ctx_ctrls\n");
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (call_cop(ctx, to_ctx_ctrls, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err_ctx("failed in to_ctx_ctrls\n");
	}

	s5p_mfc_mem_finish(vb);

	return 0;
}

static void s5p_mfc_buf_cleanup(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	unsigned int index = vb->v4l2_buf.index;

	mfc_debug_enter();

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (call_cop(ctx, cleanup_buf_ctrls, ctx,
					MFC_CTRL_TYPE_DST, index) < 0)
			mfc_err_ctx("failed in cleanup_buf_ctrls\n");
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (call_cop(ctx, cleanup_buf_ctrls, ctx,
					MFC_CTRL_TYPE_SRC, index) < 0)
			mfc_err_ctx("failed in cleanup_buf_ctrls\n");
	} else {
		mfc_err_ctx("s5p_mfc_buf_cleanup: unknown queue type.\n");
	}

	mfc_debug_leave();
}

static int s5p_mfc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;

	/* If context is ready then dev = work->data;schedule it to run */
	if (s5p_mfc_enc_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}

	s5p_mfc_try_run(dev);

	return 0;
}

#define need_to_wait_frame_start(ctx)		\
	(((ctx->state == MFCINST_FINISHING) ||	\
	  (ctx->state == MFCINST_RUNNING)) &&	\
	 test_bit(ctx->num, &ctx->dev->hw_lock))
static int s5p_mfc_stop_streaming(struct vb2_queue *q)
{
	unsigned long flags;
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	int index = 0;
	int aborted = 0;

	if (need_to_wait_frame_start(ctx)) {
		ctx->state = MFCINST_ABORT;
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_FRAME_DONE_RET))
			s5p_mfc_cleanup_timeout(ctx);
		aborted = 1;
	}

	if (enc->in_slice) {
		ctx->state = MFCINST_ABORT_INST;
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		s5p_mfc_try_run(dev);
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_NAL_ABORT_RET))
			s5p_mfc_cleanup_timeout(ctx);

		enc->in_slice = 0;
		aborted = 1;
	}

	spin_lock_irqsave(&dev->irqlock, flags);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		s5p_mfc_cleanup_queue(&ctx->dst_queue, &ctx->vq_dst);
		INIT_LIST_HEAD(&ctx->dst_queue);
		ctx->dst_queue_cnt = 0;

		while (index < MFC_MAX_BUFFERS) {
			index = find_next_bit(&ctx->dst_ctrls_avail,
					MFC_MAX_BUFFERS, index);
			if (index < MFC_MAX_BUFFERS)
				__enc_reset_buf_ctrls(&ctx->dst_ctrls[index]);
			index++;
		}
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		cleanup_ref_queue(ctx);

		s5p_mfc_cleanup_queue(&ctx->src_queue, &ctx->vq_src);
		INIT_LIST_HEAD(&ctx->src_queue);
		ctx->src_queue_cnt = 0;

		while (index < MFC_MAX_BUFFERS) {
			index = find_next_bit(&ctx->src_ctrls_avail,
					MFC_MAX_BUFFERS, index);
			if (index < MFC_MAX_BUFFERS)
				__enc_reset_buf_ctrls(&ctx->src_ctrls[index]);
			index++;
		}
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (aborted)
		ctx->state = MFCINST_RUNNING;

	return 0;
}

static void s5p_mfc_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long flags;
	struct s5p_mfc_buf *buf = vb_to_mfc_buf(vb);
	int i;

	mfc_debug_enter();

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf->used = 0;
		mfc_debug(2, "dst queue: %p\n", &ctx->dst_queue);
		mfc_debug(2, "adding to dst: %p (%08llx, %08llx)\n", vb,
			(unsigned long long)s5p_mfc_mem_plane_addr(ctx, vb, 0),
			(unsigned long long)buf->planes.stream);

		/* Mark destination as available for use by MFC */
		spin_lock_irqsave(&dev->irqlock, flags);
		list_add_tail(&buf->list, &ctx->dst_queue);
		ctx->dst_queue_cnt++;
		spin_unlock_irqrestore(&dev->irqlock, flags);
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		buf->used = 0;
		mfc_debug(2, "src queue: %p\n", &ctx->src_queue);

		for (i = 0; i < ctx->src_fmt->num_planes; i++)
			mfc_debug(2, "adding to src[%d]: %p (%08llx, %08llx)\n", i, vb,
				(unsigned long long)s5p_mfc_mem_plane_addr(ctx, vb, i),
				(unsigned long long)buf->planes.raw[i]);

		spin_lock_irqsave(&dev->irqlock, flags);

		list_add_tail(&buf->list, &ctx->src_queue);
		ctx->src_queue_cnt++;

		spin_unlock_irqrestore(&dev->irqlock, flags);
	} else {
		mfc_err_ctx("unsupported buffer type (%d)\n", vq->type);
	}

	if (s5p_mfc_enc_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}
	s5p_mfc_try_run(dev);

	mfc_debug_leave();
}

static struct vb2_ops s5p_mfc_enc_qops = {
	.queue_setup		= s5p_mfc_queue_setup,
	.wait_prepare		= s5p_mfc_unlock,
	.wait_finish		= s5p_mfc_lock,
	.buf_init		= s5p_mfc_buf_init,
	.buf_prepare		= s5p_mfc_buf_prepare,
	.buf_finish		= s5p_mfc_buf_finish,
	.buf_cleanup		= s5p_mfc_buf_cleanup,
	.start_streaming	= s5p_mfc_start_streaming,
	.stop_streaming		= s5p_mfc_stop_streaming,
	.buf_queue		= s5p_mfc_buf_queue,
};

const struct v4l2_ioctl_ops *get_enc_v4l2_ioctl_ops(void)
{
	return &s5p_mfc_enc_ioctl_ops;
}

int s5p_mfc_init_enc_ctx(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_enc *enc;
	int ret = 0;
	int i;

	enc = kzalloc(sizeof(struct s5p_mfc_enc), GFP_KERNEL);
	if (!enc) {
		mfc_err("failed to allocate encoder private data\n");
		return -ENOMEM;
	}
	ctx->enc_priv = enc;

	ctx->inst_no = MFC_NO_INSTANCE_SET;

	INIT_LIST_HEAD(&ctx->src_queue);
	INIT_LIST_HEAD(&ctx->dst_queue);
	ctx->src_queue_cnt = 0;
	ctx->dst_queue_cnt = 0;

	ctx->framerate = ENC_MAX_FPS;
	ctx->qos_ratio = 100;
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	INIT_LIST_HEAD(&ctx->qos_list);
#endif

	for (i = 0; i < MFC_MAX_BUFFERS; i++) {
		INIT_LIST_HEAD(&ctx->src_ctrls[i]);
		INIT_LIST_HEAD(&ctx->dst_ctrls[i]);
	}
	ctx->src_ctrls_avail = 0;
	ctx->dst_ctrls_avail = 0;

	ctx->type = MFCINST_ENCODER;
	ctx->c_ops = &encoder_codec_ops;
	ctx->src_fmt = &formats[DEF_SRC_FMT];
	ctx->dst_fmt = &formats[DEF_DST_FMT];

	INIT_LIST_HEAD(&enc->ref_queue);
	enc->ref_queue_cnt = 0;
	enc->sh_handle.fd = -1;

	/* Init videobuf2 queue for OUTPUT */
	ctx->vq_src.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ctx->vq_src.drv_priv = ctx;
	ctx->vq_src.buf_struct_size = sizeof(struct s5p_mfc_buf);
	ctx->vq_src.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vq_src.ops = &s5p_mfc_enc_qops;
	ctx->vq_src.mem_ops = s5p_mfc_mem_ops();
	ctx->vq_src.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&ctx->vq_src);
	if (ret) {
		mfc_err("Failed to initialize videobuf2 queue(output)\n");
		return ret;
	}

	/* Init videobuf2 queue for CAPTURE */
	ctx->vq_dst.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->vq_dst.drv_priv = ctx;
	ctx->vq_dst.buf_struct_size = sizeof(struct s5p_mfc_buf);
	ctx->vq_dst.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vq_dst.ops = &s5p_mfc_enc_qops;
	ctx->vq_dst.mem_ops = s5p_mfc_mem_ops();
	ctx->vq_dst.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&ctx->vq_dst);
	if (ret) {
		mfc_err("Failed to initialize videobuf2 queue(capture)\n");
		return ret;
	}

	return 0;
}
