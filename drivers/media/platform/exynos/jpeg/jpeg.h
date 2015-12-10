/* linux/drivers/media/platform/exynos/jpeg/jpeg.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Definition for core file of the jpeg operation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __JPEG_H__
#define __JPEG_H__

#include <linux/spinlock.h>
#include <media/m2m1shot.h>
#include <media/m2m1shot-helper.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>

#define MODULE_NAME		"exynos5-jpeg"

#define JPEG_MIN_SIZE	8
#define DCTSIZE		64
#define NUM_QTABLE_VALUES 64
#define NUM_QUANT_TBLS	2
#define NUM_HUFF_TBLS	2
#define MAX_COMPS	3
#define LEN_BIT		17
#define LEN_HUFF_TBL	256

#define DEV_RUN		1
#define DEV_SUSPEND	2
#define DEV_RUNTIME_RESUME	3

#define is_jpeg_fmt(img) ((img == V4L2_PIX_FMT_JPEG_444) || \
		(img == V4L2_PIX_FMT_JPEG_422) || \
		(img == V4L2_PIX_FMT_JPEG_420) || \
		(img == V4L2_PIX_FMT_JPEG_422V) || \
		(img == V4L2_PIX_FMT_JPEG_GRAY))

enum jpeg_mode {
	ENCODING,
	DECODING,
};

enum jpeg_clocks {
	JPEG_GATE_CLK,
	JPEG_GATE2_CLK,
	JPEG_CHLD1_CLK,
	JPEG_PARN1_CLK,
	JPEG_CHLD2_CLK,
	JPEG_PARN2_CLK,
	JPEG_CHLD3_CLK,
	JPEG_PARN3_CLK,
	JPEG_CLK_NUM,
};

enum  jpeg_img_quality_level {
	QUALITY_LEVEL_1 = 0,	/* high */
	QUALITY_LEVEL_2,
	QUALITY_LEVEL_3,
	QUALITY_LEVEL_4,
	QUALITY_LEVEL_5,
	QUALITY_LEVEL_6,	/* low */
};

struct jpeg_dev;

struct jpeg_fmt {
	char			*name;
	unsigned int		fourcc;
	u32			reg_cfg;
	u8			bitperpixel[MAX_COMPS];
	char			color_planes;
	char			mem_planes;
};

#define EXYNOS_JPEG_CTX_CUSTOM_QTBL (1 << 4)

struct jpeg_ctx {
	u32 width;
	u32 height;
	int quality;
	unsigned int flags;

	struct jpeg_fmt *in_fmt;
	struct jpeg_fmt *out_fmt;

	struct jpeg_dev *jpeg_dev;
	struct m2m1shot_context *m21ctx;

	unsigned char *custom_qtbl;
};

struct jpeg_dev {
	struct m2m1shot_device	*oneshot_dev;
	struct device		*dev;

	struct clk		*clocks[JPEG_CLK_NUM];
	void __iomem		*regs;

	int			id;
	int			ver;
	unsigned long		state;

	spinlock_t		slock;
};

enum jpeg_scale_value {
	JPEG_SCALE_NORMAL,
	JPEG_SCALE_2,
	JPEG_SCALE_4,
	JPEG_SCALE_8,
};

struct jpeg_tables_info {
	unsigned short	*quantval[4];
	unsigned char	*dc_bits[4];
	unsigned char	*dc_huffval[4];
	unsigned char	*ac_bits[4];
	unsigned char	*ac_huffval[4];
	unsigned char	quant_tbl_no[3];
	unsigned char	dc_tbl_no[3];
	unsigned char	ac_tbl_no[3];
	unsigned int	current_sos_position;
};

struct huff_tbl {
	unsigned char bit[LEN_BIT];
	unsigned char huf_tbl[LEN_HUFF_TBL];
};

struct jpeg_tables {
	unsigned short q_tbl[NUM_QUANT_TBLS][DCTSIZE];
	struct huff_tbl dc_huf_tbl[NUM_HUFF_TBLS];
	struct huff_tbl ac_huf_tbl[NUM_HUFF_TBLS];
};

#endif /*__JPEG_H__*/
