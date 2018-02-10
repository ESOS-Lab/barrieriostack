/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_PARAMS_H
#define FIMC_IS_PARAMS_H

#define IS_REGION_VER 150  /* IS REGION VERSION 1.50 */

/* MACROs */
#define IS_SET_PARAM_BIT(dev, num) \
	(num >= 32 ? set_bit((num-32), &dev->p_region_index2) \
		: set_bit(num, &dev->p_region_index1))
#define IS_INC_PARAM_NUM(dev)		atomic_inc(&dev->p_region_num)

#define IS_PARAM_GLOBAL(dev)		(dev->is_p_region->parameter.global)
#define IS_PARAM_ISP(dev)		(dev->is_p_region->parameter.isp)
#define IS_PARAM_DRC(dev)		(dev->is_p_region->parameter.drc)
#define IS_PARAM_FD(dev)		(dev->is_p_region->parameter.fd)
#define IS_HEADER(dev)			(dev->is_p_region->header)
#define IS_FACE(dev)			(dev->is_p_region->face)
#define IS_SHARED(dev)			(dev->is_shared_region)
#define IS_PARAM_SIZE			(FIMC_IS_REGION_SIZE + 1)

#ifndef BIT0
#define  BIT0     0x00000001
#define  BIT1     0x00000002
#define  BIT2     0x00000004
#define  BIT3     0x00000008
#define  BIT4     0x00000010
#define  BIT5     0x00000020
#define  BIT6     0x00000040
#define  BIT7     0x00000080
#define  BIT8     0x00000100
#define  BIT9     0x00000200
#define  BIT10    0x00000400
#define  BIT11    0x00000800
#define  BIT12    0x00001000
#define  BIT13    0x00002000
#define  BIT14    0x00004000
#define  BIT15    0x00008000
#define  BIT16    0x00010000
#define  BIT17    0x00020000
#define  BIT18    0x00040000
#define  BIT19    0x00080000
#define  BIT20    0x00100000
#define  BIT21    0x00200000
#define  BIT22    0x00400000
#define  BIT23    0x00800000
#define  BIT24    0x01000000
#define  BIT25    0x02000000
#define  BIT26    0x04000000
#define  BIT27    0x08000000
#define  BIT28    0x10000000
#define  BIT29    0x20000000
#define  BIT30    0x40000000
#define  BIT31    0x80000000
#define  BIT32    0x0000000100000000ULL
#define  BIT33    0x0000000200000000ULL
#define  BIT34    0x0000000400000000ULL
#define  BIT35    0x0000000800000000ULL
#define  BIT36    0x0000001000000000ULL
#define  BIT37    0x0000002000000000ULL
#define  BIT38    0x0000004000000000ULL
#define  BIT39    0x0000008000000000ULL
#define  BIT40    0x0000010000000000ULL
#define  BIT41    0x0000020000000000ULL
#define  BIT42    0x0000040000000000ULL
#define  BIT43    0x0000080000000000ULL
#define  BIT44    0x0000100000000000ULL
#define  BIT45    0x0000200000000000ULL
#define  BIT46    0x0000400000000000ULL
#define  BIT47    0x0000800000000000ULL
#define  BIT48    0x0001000000000000ULL
#define  BIT49    0x0002000000000000ULL
#define  BIT50    0x0004000000000000ULL
#define  BIT51    0x0008000000000000ULL
#define  BIT52    0x0010000000000000ULL
#define  BIT53    0x0020000000000000ULL
#define  BIT54    0x0040000000000000ULL
#define  BIT55    0x0080000000000000ULL
#define  BIT56    0x0100000000000000ULL
#define  BIT57    0x0200000000000000ULL
#define  BIT58    0x0400000000000000ULL
#define  BIT59    0x0800000000000000ULL
#define  BIT60    0x1000000000000000ULL
#define  BIT61    0x2000000000000000ULL
#define  BIT62    0x4000000000000000ULL
#define  BIT63    0x8000000000000000ULL
#define  INC_BIT(bit) (bit<<1)
#define  INC_NUM(bit) (bit + 1)
#endif

#define MAGIC_NUMBER 0x01020304

#define PARAMETER_MAX_SIZE    128  /* in byte */
#define PARAMETER_MAX_MEMBER  (PARAMETER_MAX_SIZE/4)

enum is_entry {
	ENTRY_GLOBAL,
	ENTRY_BUFFER,
	ENTRY_SENSOR,
	ENTRY_3AA,
	ENTRY_ISP,
	ENTRY_DRC,
	ENTRY_SCALERC,
	ENTRY_DIS,
	ENTRY_TDNR,
	ENTRY_SCALERP,
	ENTRY_LHFD,
	ENTRY_3AAC,
	ENTRY_3AAP,
	ENTRY_END
};

#define ADDRESS_TO_OFFSET(start, end)	((uint32)end - (uint32)start)
#define OFFSET_TO_NUM(offset)		((offset)>>6)
#define IS_OFFSET_LOWBIT(offset)	(OFFSET_TO_NUM(offset) >= \
						32 ? false : true)
#define OFFSET_TO_BIT(offset) \
		{(IS_OFFSET_LOWBIT(offset) ? (1<<OFFSET_TO_NUM(offset)) \
			: (1<<(OFFSET_TO_NUM(offset)-32))}
#define LOWBIT_OF_NUM(num)		(num >= 32 ? 0 : BIT0<<num)
#define HIGHBIT_OF_NUM(num)		(num >= 32 ? BIT0<<(num-32) : 0)

/* 0~31 */
#define PARAM_GLOBAL_SHOTMODE		0
#define PARAM_SENSOR_CONTROL		INC_NUM(PARAM_GLOBAL_SHOTMODE)
#define PARAM_SENSOR_OTF_INPUT		INC_NUM(PARAM_SENSOR_CONTROL)
#define PARAM_SENSOR_OTF_OUTPUT		INC_NUM(PARAM_SENSOR_OTF_INPUT)
#define PARAM_SENSOR_CONFIG		INC_NUM(PARAM_SENSOR_OTF_OUTPUT)
#define PARAM_SENSOR_DMA_OUTPUT		INC_NUM(PARAM_SENSOR_CONFIG)
#define PARAM_BUFFER_CONTROL		INC_NUM(PARAM_SENSOR_DMA_OUTPUT)
#define PARAM_BUFFER_OTF_INPUT		INC_NUM(PARAM_BUFFER_CONTROL)
#define PARAM_BUFFER_OTF_OUTPUT		INC_NUM(PARAM_BUFFER_OTF_INPUT)
#define PARAM_3AA_CONTROL		INC_NUM(PARAM_BUFFER_OTF_OUTPUT)
#define PARAM_3AA_OTF_INPUT		INC_NUM(PARAM_3AA_CONTROL)
#define PARAM_3AA_VDMA1_INPUT		INC_NUM(PARAM_3AA_OTF_INPUT)
#define PARAM_3AA_DDMA_INPUT		INC_NUM(PARAM_3AA_VDMA1_INPUT)
#define PARAM_3AA_OTF_OUTPUT		INC_NUM(PARAM_3AA_DDMA_INPUT)
#define PARAM_3AA_VDMA4_OUTPUT		INC_NUM(PARAM_3AA_OTF_OUTPUT)
#define PARAM_3AA_VDMA2_OUTPUT		INC_NUM(PARAM_3AA_VDMA4_OUTPUT)
#define PARAM_3AA_DDMA_OUTPUT		INC_NUM(PARAM_3AA_VDMA2_OUTPUT)
#define PARAM_ISP_CONTROL		INC_NUM(PARAM_3AA_DDMA_OUTPUT)
#define PARAM_ISP_OTF_INPUT		INC_NUM(PARAM_ISP_CONTROL)
#define PARAM_ISP_VDMA1_INPUT		INC_NUM(PARAM_ISP_OTF_INPUT)
#define PARAM_ISP_VDMA3_INPUT		INC_NUM(PARAM_ISP_VDMA1_INPUT)
#define PARAM_ISP_OTF_OUTPUT		INC_NUM(PARAM_ISP_VDMA3_INPUT)
#define PARAM_ISP_VDMA4_OUTPUT		INC_NUM(PARAM_ISP_OTF_OUTPUT)
#define PARAM_ISP_VDMA5_OUTPUT		INC_NUM(PARAM_ISP_VDMA4_OUTPUT)
#define PARAM_DRC_CONTROL		INC_NUM(PARAM_ISP_VDMA5_OUTPUT)
#define PARAM_DRC_OTF_INPUT		INC_NUM(PARAM_DRC_CONTROL)
#define PARAM_DRC_DMA_INPUT		INC_NUM(PARAM_DRC_OTF_INPUT)
#define PARAM_DRC_OTF_OUTPUT		INC_NUM(PARAM_DRC_DMA_INPUT)
#define PARAM_SCALERC_CONTROL		INC_NUM(PARAM_DRC_OTF_OUTPUT)
#define PARAM_SCALERC_OTF_INPUT		INC_NUM(PARAM_SCALERC_CONTROL)
#define PARAM_SCALERC_IMAGE_EFFECT	INC_NUM(PARAM_SCALERC_OTF_INPUT)
#define PARAM_SCALERC_INPUT_CROP	INC_NUM(PARAM_SCALERC_IMAGE_EFFECT)
/* 32~63 */
#define PARAM_SCALERC_OUTPUT_CROP	INC_NUM(PARAM_SCALERC_INPUT_CROP)
#define PARAM_SCALERC_OTF_OUTPUT	INC_NUM(PARAM_SCALERC_OUTPUT_CROP)
#define PARAM_SCALERC_DMA_OUTPUT	INC_NUM(PARAM_SCALERC_OTF_OUTPUT)
#define PARAM_ODC_CONTROL		INC_NUM(PARAM_SCALERC_DMA_OUTPUT)
#define PARAM_ODC_OTF_INPUT		INC_NUM(PARAM_ODC_CONTROL)
#define PARAM_ODC_OTF_OUTPUT		INC_NUM(PARAM_ODC_OTF_INPUT)
#define PARAM_DIS_CONTROL		INC_NUM(PARAM_ODC_OTF_OUTPUT)
#define PARAM_DIS_OTF_INPUT		INC_NUM(PARAM_DIS_CONTROL)
#define PARAM_DIS_OTF_OUTPUT		INC_NUM(PARAM_DIS_OTF_INPUT)
#define PARAM_TDNR_CONTROL		INC_NUM(PARAM_DIS_OTF_OUTPUT)
#define PARAM_TDNR_OTF_INPUT		INC_NUM(PARAM_TDNR_CONTROL)
#define PARAM_TDNR_1ST_FRAME		INC_NUM(PARAM_TDNR_OTF_INPUT)
#define PARAM_TDNR_OTF_OUTPUT		INC_NUM(PARAM_TDNR_1ST_FRAME)
#define PARAM_TDNR_DMA_OUTPUT		INC_NUM(PARAM_TDNR_OTF_OUTPUT)
#define PARAM_SCALERP_CONTROL		INC_NUM(PARAM_TDNR_DMA_OUTPUT)
#define PARAM_SCALERP_OTF_INPUT		INC_NUM(PARAM_SCALERP_CONTROL)
#define PARAM_SCALERP_IMAGE_EFFECT	INC_NUM(PARAM_SCALERP_OTF_INPUT)
#define PARAM_SCALERP_INPUT_CROP	INC_NUM(PARAM_SCALERP_IMAGE_EFFECT)
#define PARAM_SCALERP_OUTPUT_CROP	INC_NUM(PARAM_SCALERP_INPUT_CROP)
#define PARAM_SCALERP_ROTATION		INC_NUM(PARAM_SCALERP_OUTPUT_CROP)
#define PARAM_SCALERP_FLIP		INC_NUM(PARAM_SCALERP_ROTATION)
#define PARAM_SCALERP_OTF_OUTPUT	INC_NUM(PARAM_SCALERP_FLIP)
#define PARAM_SCALERP_DMA_OUTPUT	INC_NUM(PARAM_SCALERP_OTF_OUTPUT)
#define PARAM_FD_CONTROL		INC_NUM(PARAM_SCALERP_DMA_OUTPUT)
#define PARAM_FD_OTF_INPUT		INC_NUM(PARAM_FD_CONTROL)
#define PARAM_FD_DMA_INPUT		INC_NUM(PARAM_FD_OTF_INPUT)
#define PARAM_FD_CONFIG			INC_NUM(PARAM_FD_DMA_INPUT)
#define PARAM_END			INC_NUM(PARAM_FD_CONFIG)

#define PARAM_STRNUM_GLOBAL		(PARAM_GLOBAL_SHOTMODE)
#define PARAM_RANGE_GLOBAL		1
#define PARAM_STRNUM_SENSOR		(PARAM_SENSOR_CONTROL)
#define PARAM_RANGE_SENSOR		5
#define PARAM_STRNUM_BUFFER		(PARAM_BUFFER_CONTROL)
#define PARAM_RANGE_BUFFER		3
#define PARAM_STRNUM_3AA		(PARAM_3AA_CONTROL)
#define PARAM_RANGE_3AA			8
#define PARAM_STRNUM_ISP		(PARAM_ISP_CONTROL)
#define PARAM_RANGE_ISP			7
#define PARAM_STRNUM_DRC		(PARAM_DRC_CONTROL)
#define PARAM_RANGE_DRC			4
#define PARAM_STRNUM_SCALERC		(PARAM_SCALERC_CONTROL)
#define PARAM_RANGE_SCALERC		7
#define PARAM_STRNUM_ODC		(PARAM_ODC_CONTROL)
#define PARAM_RANGE_ODC			3
#define PARAM_STRNUM_DIS		(PARAM_DIS_CONTROL)
#define PARAM_RANGE_DIS			3
#define PARAM_STRNUM_TDNR		(PARAM_TDNR_CONTROL)
#define PARAM_RANGE_TDNR		5
#define PARAM_STRNUM_SCALERP		(PARAM_SCALERP_CONTROL)
#define PARAM_RANGE_SCALERP		9
#define PARAM_STRNUM_LHFD		(PARAM_FD_CONTROL)
#define PARAM_RANGE_LHFD		4

#define PARAM_LOW_MASK		(0xFFFFFFFF)
#define PARAM_HIGH_MASK		(0x07FFFFFF)

/* Enumerations
*
*/

/* ----------------------  Input  ----------------------------------- */
enum control_command {
	CONTROL_COMMAND_STOP	= 0,
	CONTROL_COMMAND_START	= 1,
	CONTROL_COMMAND_TEST	= 2
};

enum bypass_command {
	CONTROL_BYPASS_DISABLE		= 0,
	CONTROL_BYPASS_ENABLE		= 1
};

enum control_error {
	CONTROL_ERROR_NO		= 0
};

enum otf_input_command {
	OTF_INPUT_COMMAND_DISABLE	= 0,
	OTF_INPUT_COMMAND_ENABLE	= 1
};

enum otf_input_format {
	OTF_INPUT_FORMAT_BAYER		= 0, /* 1 Channel */
	OTF_INPUT_FORMAT_YUV444		= 1, /* 3 Channel */
	OTF_INPUT_FORMAT_YUV422		= 2, /* 3 Channel */
	OTF_INPUT_FORMAT_YUV420		= 3, /* 3 Channel */
	OTF_INPUT_FORMAT_STRGEN_COLORBAR_BAYER = 10,
	OTF_INPUT_FORMAT_BAYER_DMA	= 11,
};

enum otf_input_bitwidth {
	OTF_INPUT_BIT_WIDTH_14BIT	= 14,
	OTF_INPUT_BIT_WIDTH_12BIT	= 12,
	OTF_INPUT_BIT_WIDTH_11BIT	= 11,
	OTF_INPUT_BIT_WIDTH_10BIT	= 10,
	OTF_INPUT_BIT_WIDTH_9BIT	= 9,
	OTF_INPUT_BIT_WIDTH_8BIT	= 8
};

enum otf_input_order {
	OTF_INPUT_ORDER_BAYER_GR_BG	= 0,
	OTF_INPUT_ORDER_BAYER_RG_GB	= 1,
	OTF_INPUT_ORDER_BAYER_BG_GR	= 2,
	OTF_INPUT_ORDER_BAYER_GB_RG	= 3
};

enum otf_input_path {
	OTF_INPUT_SERIAL_PATH = 0,
	OTF_INPUT_PARAL_PATH = 1
};

enum otf_intput_error {
       OTF_INPUT_ERROR_NO		= 0 /* Input setting is done */
};

enum dma_input_command {
	DMA_INPUT_COMMAND_DISABLE	= 0,
	DMA_INPUT_COMMAND_ENABLE	= 1,
	DMA_INPUT_COMMAND_BUF_MNGR	= 2,
	DMA_INPUT_COMMAND_RUN_SINGLE	= 3,
};

enum dma_inut_format {
	DMA_INPUT_FORMAT_BAYER		= 0,
	DMA_INPUT_FORMAT_YUV444		= 1,
	DMA_INPUT_FORMAT_YUV422		= 2,
	DMA_INPUT_FORMAT_YUV420		= 3,
	DMA_INPUT_FORMAT_RGB		= 4,
	DMA_INPUT_FORMAT_BAYER_PACKED12	= 5,
};

enum dma_input_bitwidth {
	DMA_INPUT_BIT_WIDTH_14BIT	= 14,
	DMA_INPUT_BIT_WIDTH_12BIT	= 12,
	DMA_INPUT_BIT_WIDTH_11BIT	= 11,
	DMA_INPUT_BIT_WIDTH_10BIT	= 10,
	DMA_INPUT_BIT_WIDTH_9BIT	= 9,
	DMA_INPUT_BIT_WIDTH_8BIT	= 8
};

enum dma_input_plane {
	DMA_INPUT_PLANE_3	= 3,
	DMA_INPUT_PLANE_2	= 2,
	DMA_INPUT_PLANE_1	= 1
};

enum dma_input_order {
	/* (for DMA_INPUT_PLANE_3) */
	DMA_INPUT_ORDER_NO	= 0,
	/* (only valid at DMA_INPUT_PLANE_2) */
	DMA_INPUT_ORDER_CbCr	= 1,
	/* (only valid at DMA_INPUT_PLANE_2) */
	DMA_INPUT_ORDER_CrCb	= 2,
	/* (only valid at DMA_INPUT_PLANE_1 & DMA_INPUT_FORMAT_YUV444) */
	DMA_INPUT_ORDER_YCbCr	= 3,
	/* (only valid at DMA_INPUT_FORMAT_YUV422 & DMA_INPUT_PLANE_1) */
	DMA_INPUT_ORDER_YYCbCr	= 4,
	/* (only valid at DMA_INPUT_FORMAT_YUV422 & DMA_INPUT_PLANE_1) */
	DMA_INPUT_ORDER_YCbYCr	= 5,
	/* (only valid at DMA_INPUT_FORMAT_YUV422 & DMA_INPUT_PLANE_1) */
	DMA_INPUT_ORDER_YCrYCb	= 6,
	/* (only valid at DMA_INPUT_FORMAT_YUV422 & DMA_INPUT_PLANE_1) */
	DMA_INPUT_ORDER_CbYCrY	= 7,
	/* (only valid at DMA_INPUT_FORMAT_YUV422 & DMA_INPUT_PLANE_1) */
	DMA_INPUT_ORDER_CrYCbY	= 8,
	/* (only valid at DMA_INPUT_FORMAT_BAYER) */
	DMA_INPUT_ORDER_GR_BG	= 9
};

enum dma_input_MemoryWidthBits {
	DMA_INPUT_MEMORY_WIDTH_16BIT	= 16,
	DMA_INPUT_MEMORY_WIDTH_12BIT	= 12,
};

enum dma_input_error {
	DMA_INPUT_ERROR_NO	= 0 /*  DMA input setting is done */
};

/* ----------------------  Output  ----------------------------------- */
enum otf_output_crop {
	OTF_OUTPUT_CROP_DISABLE		= 0,
	OTF_OUTPUT_CROP_ENABLE		= 1
};

enum otf_output_command {
	OTF_OUTPUT_COMMAND_DISABLE	= 0,
	OTF_OUTPUT_COMMAND_ENABLE	= 1
};

enum orf_output_format {
	OTF_OUTPUT_FORMAT_YUV444		= 1,
	OTF_OUTPUT_FORMAT_YUV422		= 2,
	OTF_OUTPUT_FORMAT_YUV420		= 3,
	OTF_OUTPUT_FORMAT_RGB			= 4,
	OTF_OUTPUT_FORMAT_YUV444_TRUNCATED	= 5,
	OTF_OUTPUT_FORMAT_YUV422_TRUNCATED	= 6
};

enum otf_output_bitwidth {
	OTF_OUTPUT_BIT_WIDTH_14BIT	= 14,
	OTF_OUTPUT_BIT_WIDTH_12BIT	= 12,
	OTF_OUTPUT_BIT_WIDTH_11BIT	= 11,
	OTF_OUTPUT_BIT_WIDTH_10BIT	= 10,
	OTF_OUTPUT_BIT_WIDTH_9BIT	= 9,
	OTF_OUTPUT_BIT_WIDTH_8BIT	= 8
};

enum otf_output_order {
	OTF_OUTPUT_ORDER_BAYER_GR_BG	= 0,
};

enum otf_output_error {
	OTF_OUTPUT_ERROR_NO = 0 /* Output Setting is done */
};

enum dma_output_command {
	DMA_OUTPUT_COMMAND_DISABLE	= 0,
	DMA_OUTPUT_COMMAND_ENABLE	= 1,
	DMA_OUTPUT_COMMAND_BUF_MNGR	= 2,
	DMA_OUTPUT_UPDATE_MASK_BITS	= 3
};

enum dma_output_format {
	DMA_OUTPUT_FORMAT_BAYER		= 0,
	DMA_OUTPUT_FORMAT_YUV444	= 1,
	DMA_OUTPUT_FORMAT_YUV422	= 2,
	DMA_OUTPUT_FORMAT_YUV420	= 3,
	DMA_OUTPUT_FORMAT_RGB		= 4,
	DMA_OUTPUT_FORMAT_BAYER_PACKED12= 5,
};

enum dma_output_bitwidth {
	DMA_OUTPUT_BIT_WIDTH_14BIT	= 14,
	DMA_OUTPUT_BIT_WIDTH_12BIT	= 12,
	DMA_OUTPUT_BIT_WIDTH_11BIT	= 11,
	DMA_OUTPUT_BIT_WIDTH_10BIT	= 10,
	DMA_OUTPUT_BIT_WIDTH_9BIT	= 9,
	DMA_OUTPUT_BIT_WIDTH_8BIT	= 8
};

enum dma_output_plane {
	DMA_OUTPUT_PLANE_3		= 3,
	DMA_OUTPUT_PLANE_2		= 2,
	DMA_OUTPUT_PLANE_1		= 1
};

enum dma_output_order {
	DMA_OUTPUT_ORDER_NO		= 0,
	/* (for DMA_OUTPUT_PLANE_3) */
	DMA_OUTPUT_ORDER_CbCr		= 1,
	/* (only valid at DMA_INPUT_PLANE_2) */
	DMA_OUTPUT_ORDER_CrCb		= 2,
	/* (only valid at DMA_OUTPUT_PLANE_2) */
	DMA_OUTPUT_ORDER_YYCbCr		= 3,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV422 & DMA_OUTPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_YCbYCr		= 4,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV422 & DMA_OUTPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_YCrYCb		= 5,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV422 & DMA_OUTPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_CbYCrY		= 6,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV422 & DMA_OUTPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_CrYCbY		= 7,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV422 & DMA_OUTPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_YCbCr		= 8,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_CrYCb		= 9,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_CrCbY		= 10,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_CbYCr		= 11,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_YCrCb		= 12,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_CbCrY		= 13,
	/* (only valid at DMA_OUTPUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_OUTPUT_ORDER_BGR		= 14,
	/* (only valid at DMA_OUTPUT_FORMAT_RGB) */
	DMA_OUTPUT_ORDER_GB_BG		= 15
	/* (only valid at DMA_OUTPUT_FORMAT_BAYER) */
};

enum dma_output_notify_dma_done {
	DMA_OUTPUT_NOTIFY_DMA_DONE_DISABLE	= 0,
	DMA_OUTPUT_NOTIFY_DMA_DONE_ENBABLE	= 1,
};

enum dma_output_error {
	DMA_OUTPUT_ERROR_NO		= 0 /* DMA output setting is done */
};

/* ----------------------  Global  ----------------------------------- */
enum global_shotmode_error {
	GLOBAL_SHOTMODE_ERROR_NO	= 0 /* shot-mode setting is done */
};

/* -------------------------  AA  ------------------------------------ */
enum isp_lock_command {
	ISP_AA_COMMAND_START	= 0,
	ISP_AA_COMMAND_STOP	= 1
};

enum isp_lock_target {
	ISP_AA_TARGET_AF	= 1,
	ISP_AA_TARGET_AE	= 2,
	ISP_AA_TARGET_AWB	= 4
};

enum isp_af_mode {
	ISP_AF_MANUAL = 0,
	ISP_AF_SINGLE,
	ISP_AF_CONTINUOUS,
	ISP_AF_REGION,
	ISP_AF_SLEEP,
	ISP_AF_INIT,
	ISP_AF_SET_CENTER_WINDOW,
	ISP_AF_SET_TOUCH_WINDOW,
	ISP_AF_SET_FACE_WINDOW
};

enum isp_af_scene {
	ISP_AF_SCENE_NORMAL		= 0,
	ISP_AF_SCENE_MACRO		= 1
};

enum isp_af_touch {
	ISP_AF_TOUCH_DISABLE = 0,
	ISP_AF_TOUCH_ENABLE
};

enum isp_af_face {
	ISP_AF_FACE_DISABLE = 0,
	ISP_AF_FACE_ENABLE
};

enum isp_af_reponse {
	ISP_AF_RESPONSE_PREVIEW = 0,
	ISP_AF_RESPONSE_MOVIE
};

enum isp_af_sleep {
	ISP_AF_SLEEP_OFF		= 0,
	ISP_AF_SLEEP_ON			= 1
};

enum isp_af_continuous {
	ISP_AF_CONTINUOUS_DISABLE	= 0,
	ISP_AF_CONTINUOUS_ENABLE	= 1
};

enum isp_af_error {
	ISP_AF_ERROR_NO			= 0, /* AF mode change is done */
	ISP_AF_EROOR_NO_LOCK_DONE	= 1  /* AF lock is done */
};

/* -------------------------  Flash  ------------------------------------- */
enum isp_flash_command {
	ISP_FLASH_COMMAND_DISABLE	= 0,
	ISP_FLASH_COMMAND_MANUALON	= 1, /* (forced flash) */
	ISP_FLASH_COMMAND_AUTO		= 2,
	ISP_FLASH_COMMAND_TORCH		= 3,   /* 3 sec */
	ISP_FLASH_COMMAND_FLASH_ON	= 4,
	ISP_FLASH_COMMAND_CAPTURE	= 5,
	ISP_FLASH_COMMAND_TRIGGER	= 6,
	ISP_FLASH_COMMAND_CALIBRATION	= 7,
	ISP_FLASH_COMMAND_START		= 8,
	ISP_FLASH_COMMAND_CANCLE	= 9
};

enum isp_flash_redeye {
	ISP_FLASH_REDEYE_DISABLE	= 0,
	ISP_FLASH_REDEYE_ENABLE		= 1
};

enum isp_flash_error {
	ISP_FLASH_ERROR_NO		= 0 /* Flash setting is done */
};

/* --------------------------  AWB  ------------------------------------ */
enum isp_awb_command {
	ISP_AWB_COMMAND_AUTO		= 0,
	ISP_AWB_COMMAND_ILLUMINATION	= 1,
	ISP_AWB_COMMAND_MANUAL	= 2
};

enum isp_awb_illumination {
	ISP_AWB_ILLUMINATION_DAYLIGHT		= 0,
	ISP_AWB_ILLUMINATION_CLOUDY		= 1,
	ISP_AWB_ILLUMINATION_TUNGSTEN		= 2,
	ISP_AWB_ILLUMINATION_FLUORESCENT	= 3
};

enum isp_awb_error {
	ISP_AWB_ERROR_NO		= 0 /* AWB setting is done */
};

/* --------------------------  Effect  ----------------------------------- */
enum isp_imageeffect_command {
	ISP_IMAGE_EFFECT_DISABLE		= 0,
	ISP_IMAGE_EFFECT_MONOCHROME		= 1,
	ISP_IMAGE_EFFECT_NEGATIVE_MONO		= 2,
	ISP_IMAGE_EFFECT_NEGATIVE_COLOR		= 3,
	ISP_IMAGE_EFFECT_SEPIA			= 4,
	ISP_IMAGE_EFFECT_AQUA			= 5,
	ISP_IMAGE_EFFECT_EMBOSS			= 6,
	ISP_IMAGE_EFFECT_EMBOSS_MONO		= 7,
	ISP_IMAGE_EFFECT_SKETCH			= 8,
	ISP_IMAGE_EFFECT_RED_YELLOW_POINT	= 9,
	ISP_IMAGE_EFFECT_GREEN_POINT		= 10,
	ISP_IMAGE_EFFECT_BLUE_POINT		= 11,
	ISP_IMAGE_EFFECT_MAGENTA_POINT		= 12,
	ISP_IMAGE_EFFECT_WARM_VINTAGE		= 13,
	ISP_IMAGE_EFFECT_COLD_VINTAGE		= 14,
	ISP_IMAGE_EFFECT_POSTERIZE		= 15,
	ISP_IMAGE_EFFECT_SOLARIZE		= 16,
	ISP_IMAGE_EFFECT_WASHED			= 17,
	ISP_IMAGE_EFFECT_CCM			= 18,
};

enum isp_imageeffect_error {
	ISP_IMAGE_EFFECT_ERROR_NO	= 0 /* Image effect setting is done */
};

/* ---------------------------  ISO  ------------------------------------ */
enum isp_iso_command {
	ISP_ISO_COMMAND_AUTO		= 0,
	ISP_ISO_COMMAND_MANUAL		= 1
};

enum iso_error {
	ISP_ISO_ERROR_NO		= 0 /* ISO setting is done */
};

/* --------------------------  Adjust  ----------------------------------- */
enum iso_adjust_command {
	ISP_ADJUST_COMMAND_AUTO			= 0,
	ISP_ADJUST_COMMAND_MANUAL_CONTRAST	= (1 << 0),
	ISP_ADJUST_COMMAND_MANUAL_SATURATION	= (1 << 1),
	ISP_ADJUST_COMMAND_MANUAL_SHARPNESS	= (1 << 2),
	ISP_ADJUST_COMMAND_MANUAL_EXPOSURE	= (1 << 3),
	ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS	= (1 << 4),
	ISP_ADJUST_COMMAND_MANUAL_HUE		= (1 << 5),
	ISP_ADJUST_COMMAND_MANUAL_HOTPIXEL	= (1 << 6),
	ISP_ADJUST_COMMAND_MANUAL_NOISEREDUCTION = (1 << 7),
	ISP_ADJUST_COMMAND_MANUAL_SHADING	= (1 << 8),
	ISP_ADJUST_COMMAND_MANUAL_GAMMA		= (1 << 9),
	ISP_ADJUST_COMMAND_MANUAL_EDGEENHANCEMENT = (1 << 10),
	ISP_ADJUST_COMMAND_MANUAL_SCENE		= (1 << 11),
	ISP_ADJUST_COMMAND_MANUAL_FRAMETIME	= (1 << 12),
	ISP_ADJUST_COMMAND_MANUAL_ALL		= 0x1FFF
};

enum isp_adjust_scene_index {
	ISP_ADJUST_SCENE_NORMAL			= 0,
	ISP_ADJUST_SCENE_NIGHT_PREVIEW		= 1,
	ISP_ADJUST_SCENE_NIGHT_CAPTURE		= 2
};


enum isp_adjust_error {
	ISP_ADJUST_ERROR_NO		= 0 /* Adjust setting is done */
};

/* -------------------------  Metering  ---------------------------------- */
enum isp_metering_command {
	ISP_METERING_COMMAND_AVERAGE		= 0,
	ISP_METERING_COMMAND_SPOT		= 1,
	ISP_METERING_COMMAND_MATRIX		= 2,
	ISP_METERING_COMMAND_CENTER		= 3,
	ISP_METERING_COMMAND_EXPOSURE_MODE	= (1 << 8)
};

enum isp_exposure_mode {
	ISP_EXPOSUREMODE_OFF		= 1,
	ISP_EXPOSUREMODE_AUTO		= 2
};

enum isp_metering_error {
	ISP_METERING_ERROR_NO	= 0 /* Metering setting is done */
};

/* --------------------------  AFC  ----------------------------------- */
enum isp_afc_command {
	ISP_AFC_COMMAND_DISABLE		= 0,
	ISP_AFC_COMMAND_AUTO		= 1,
	ISP_AFC_COMMAND_MANUAL		= 2
};

enum isp_afc_manual {
	ISP_AFC_MANUAL_50HZ		= 50,
	ISP_AFC_MANUAL_60HZ		= 60
};

enum isp_afc_error {
	ISP_AFC_ERROR_NO	= 0 /* AFC setting is done */
};

enum isp_scene_command {
	ISP_SCENE_NONE		= 0,
	ISP_SCENE_PORTRAIT	= 1,
	ISP_SCENE_LANDSCAPE     = 2,
	ISP_SCENE_SPORTS        = 3,
	ISP_SCENE_PARTYINDOOR	= 4,
	ISP_SCENE_BEACHSNOW	= 5,
	ISP_SCENE_SUNSET	= 6,
	ISP_SCENE_DAWN		= 7,
	ISP_SCENE_FALL		= 8,
	ISP_SCENE_NIGHT		= 9,
	ISP_SCENE_AGAINSTLIGHTWLIGHT	= 10,
	ISP_SCENE_AGAINSTLIGHTWOLIGHT	= 11,
	ISP_SCENE_FIRE			= 12,
	ISP_SCENE_TEXT			= 13,
	ISP_SCENE_CANDLE		= 14
};

enum ISP_BDSCommandEnum {
	ISP_BDS_COMMAND_DISABLE		= 0,
	ISP_BDS_COMMAND_ENABLE		= 1
};

/* --------------------------  Scaler  --------------------------------- */
enum scaler_imageeffect_command {
	SCALER_IMAGE_EFFECT_COMMNAD_DISABLE	= 0,
	SCALER_IMAGE_EFFECT_COMMNAD_SEPIA_CB	= 1,
	SCALER_IMAGE_EFFECT_COMMAND_SEPIA_CR	= 2,
	SCALER_IMAGE_EFFECT_COMMAND_NEGATIVE	= 3,
	SCALER_IMAGE_EFFECT_COMMAND_ARTFREEZE	= 4,
	SCALER_IMAGE_EFFECT_COMMAND_EMBOSSING	= 5,
	SCALER_IMAGE_EFFECT_COMMAND_SILHOUETTE	= 6
};

enum scaler_imageeffect_error {
	SCALER_IMAGE_EFFECT_ERROR_NO		= 0
};

enum scaler_crop_command {
	SCALER_CROP_COMMAND_DISABLE		= 0,
	SCALER_CROP_COMMAND_ENABLE		= 1
};

enum scaler_crop_error {
	SCALER_CROP_ERROR_NO			= 0 /* crop setting is done */
};

enum scaler_scaling_command {
	SCALER_SCALING_COMMNAD_DISABLE		= 0,
	SCALER_SCALING_COMMAND_UP		= 1,
	SCALER_SCALING_COMMAND_DOWN		= 2
};

enum scaler_scaling_error {
	SCALER_SCALING_ERROR_NO			= 0
};

enum scaler_rotation_command {
	SCALER_ROTATION_COMMAND_DISABLE		= 0,
	SCALER_ROTATION_COMMAND_CLOCKWISE90	= 1
};

enum scaler_rotation_error {
	SCALER_ROTATION_ERROR_NO		= 0
};

enum scaler_flip_command {
	SCALER_FLIP_COMMAND_NORMAL		= 0,
	SCALER_FLIP_COMMAND_X_MIRROR		= 1,
	SCALER_FLIP_COMMAND_Y_MIRROR		= 2,
	SCALER_FLIP_COMMAND_XY_MIRROR		= 3 /* (180 rotation) */
};

enum scaler_flip_error {
	SCALER_FLIP_ERROR_NO			= 0 /* flip setting is done */
};

enum scaler_dma_out_sel {
	SCALER_DMA_OUT_IMAGE_EFFECT		= 0,
	SCALER_DMA_OUT_SCALED			= 1,
	SCALER_DMA_OUT_UNSCALED			= 2
};

enum scaler_output_yuv_range {
	SCALER_OUTPUT_YUV_RANGE_FULL = 0,
	SCALER_OUTPUT_YUV_RANGE_NARROW = 1,
};

/* --------------------------  3DNR  ----------------------------------- */
enum tdnr_1st_frame_command {
	TDNR_1ST_FRAME_COMMAND_NOPROCESSING	= 0,
	TDNR_1ST_FRAME_COMMAND_2DNR		= 1
};

enum tdnr_1st_frame_error {
	TDNR_1ST_FRAME_ERROR_NO			= 0
		/*1st frame setting is done*/
};

/* ----------------------------  FD  ------------------------------------- */
enum fd_config_command {
	FD_CONFIG_COMMAND_MAXIMUM_NUMBER	= 0x1,
	FD_CONFIG_COMMAND_ROLL_ANGLE		= 0x2,
	FD_CONFIG_COMMAND_YAW_ANGLE		= 0x4,
	FD_CONFIG_COMMAND_SMILE_MODE		= 0x8,
	FD_CONFIG_COMMAND_BLINK_MODE		= 0x10,
	FD_CONFIG_COMMAND_EYES_DETECT		= 0x20,
	FD_CONFIG_COMMAND_MOUTH_DETECT		= 0x40,
	FD_CONFIG_COMMAND_ORIENTATION		= 0x80,
	FD_CONFIG_COMMAND_ORIENTATION_VALUE	= 0x100
};

enum fd_config_roll_angle {
	FD_CONFIG_ROLL_ANGLE_BASIC		= 0,
	FD_CONFIG_ROLL_ANGLE_PRECISE_BASIC	= 1,
	FD_CONFIG_ROLL_ANGLE_SIDES		= 2,
	FD_CONFIG_ROLL_ANGLE_PRECISE_SIDES	= 3,
	FD_CONFIG_ROLL_ANGLE_FULL		= 4,
	FD_CONFIG_ROLL_ANGLE_PRECISE_FULL	= 5,
};

enum fd_config_yaw_angle {
	FD_CONFIG_YAW_ANGLE_0			= 0,
	FD_CONFIG_YAW_ANGLE_45			= 1,
	FD_CONFIG_YAW_ANGLE_90			= 2,
	FD_CONFIG_YAW_ANGLE_45_90		= 3,
};

enum fd_config_smile_mode {
	FD_CONFIG_SMILE_MODE_DISABLE		= 0,
	FD_CONFIG_SMILE_MODE_ENABLE		= 1
};

enum fd_config_blink_mode {
	FD_CONFIG_BLINK_MODE_DISABLE		= 0,
	FD_CONFIG_BLINK_MODE_ENABLE		= 1
};

enum fd_config_eye_result {
	FD_CONFIG_EYES_DETECT_DISABLE		= 0,
	FD_CONFIG_EYES_DETECT_ENABLE		= 1
};

enum fd_config_mouth_result {
	FD_CONFIG_MOUTH_DETECT_DISABLE		= 0,
	FD_CONFIG_MOUTH_DETECT_ENABLE		= 1
};

enum fd_config_orientation {
	FD_CONFIG_ORIENTATION_DISABLE		= 0,
	FD_CONFIG_ORIENTATION_ENABLE		= 1
};

struct param_control {
	u32	cmd;
	u32	bypass;
	u32	buffer_address;
	u32	buffer_number;
	/* 0: continuous, 1: single */
	u32	run_mode;
	u32	reserved[PARAMETER_MAX_MEMBER-6];
	u32	err;
};

struct param_otf_input {
	u32	cmd;
	u32	width; /* with margin */
	u32	height; /* with margine */
	u32	format;
	u32	bitwidth;
	u32	order;
	u32	sensor_binning_ratio_x; /* ex(x1: 1000, x0.5: 2000) */
	u32	sensor_binning_ratio_y; /* ex(x1: 1000, x0.5: 2000) */
	u32	bns_binning_enable;
	u32	bns_binning_ratio_x; /* ex(x1: 1000, x0.5: 2000) */
	u32	bns_binning_ratio_y; /* ex(x1: 1000, x0.5: 2000) */
	u32	bns_margin_left;
	u32	bns_margin_top;
	u32	bns_output_width; /* with margin */
	u32	bns_output_height; /* with margin */
	u32	bayer_crop_enable;
	u32	bayer_crop_offset_x;
	u32	bayer_crop_offset_y;
	u32	bayer_crop_width; /* without margin */
	u32	bayer_crop_height; /* without margin */
	u32	bds_out_enable;
	u32	bds_out_width; /* without margin */
	u32	bds_out_height; /* without margin */
	u32	frametime_min;
	u32	frametime_max;
	u32	scaler_path_sel; /* parallel or serial for SCC*/
	u32	reserved[PARAMETER_MAX_MEMBER-27];
	u32	err;
};

struct param_dma_input {
	u32	cmd;
	u32	width; /* with margin */
	u32	height; /* with margin */
	u32	format;
	u32	bitwidth;
	u32	plane;
	u32	order;
	u32	buffer_number;
	u32	buffer_address;
	u32	sensor_binning_ratio_x; /* ex(x1: 1000, x0.5: 2000) */
	u32	sensor_binning_ratio_y; /* ex(x1: 1000, x0.5: 2000) */
	u32	dma_crop_enable; /* should be 0 */
	u32	dma_crop_offset_x; /* not supported yet */
	u32	dma_crop_offset_y; /* not supported yet */
	u32	dma_crop_width; /* not supported yet */
	u32	dma_crop_height; /* not supported yet */
	u32	bayer_crop_enable;
	u32	bayer_crop_offset_x;
	u32	bayer_crop_offset_y;
	u32	bayer_crop_width; /* without margine */
	u32	bayer_crop_height; /* without margine */
	u32	bds_out_enable;
	u32	bds_out_width; /* without margine */
	u32	bds_out_height; /* without margine */
	u32	user_min_frame_time;
	u32	user_max_frame_time;
	u32	reserved[PARAMETER_MAX_MEMBER-27];
	u32	err;
};

struct param_otf_output {
	u32	cmd;
	u32	width;
	u32	height;
	u32	format;
	u32	bitwidth;
	u32	order;
	u32	crop_offset_x;
	u32	crop_offset_y;
	u32	reserved[PARAMETER_MAX_MEMBER-9];
	u32	err;
};

struct param_dma_output {
	u32	cmd;
	u32	width;
	u32	height;
	u32	format;
	u32	bitwidth;
	u32	plane;
	u32	order;
	u32	buffer_number;
	u32	buffer_address;
	u32	notify_dma_done;
	u32	dma_out_mask;
	u32	reserved[PARAMETER_MAX_MEMBER-12];
	u32	err;
};

struct param_global_shotmode {
	u32	cmd;
	u32	skip_frames;
	u32	reserved[PARAMETER_MAX_MEMBER-3];
	u32	err;
};

struct param_sensor_config {
	u32	framerate;
	u32	width;
	u32	height;
	u32	mode;
	u32	binning_ratio_x;
	u32	binning_ratio_y;
	u32	min_target_fps;
	u32	max_target_fps;
	u32	scene_mode;
	u32	reserved[PARAMETER_MAX_MEMBER-10];
	u32	err;
};

struct param_isp_aa {
	u32	cmd;
	u32	target;
	u32	mode;
	u32	scene;
	u32	af_touch;
	u32	af_face;
	u32	af_response;
	u32	sleep;
	u32	touch_x;
	u32	touch_y;
	u32	manual_af_setting;
	/*0: Legacy, 1: Camera 2.0*/
	u32	cam_api_2p0;
	/* For android.control.afRegions in Camera 2.0,
	Resolution based on YUV output size*/
	u32	af_region_left;
	/* For android.control.afRegions in Camera 2.0,
	Resolution based on YUV output size*/
	u32	af_region_top;
	/* For android.control.afRegions in Camera 2.0,
	Resolution based on YUV output size*/
	u32	af_region_right;
	/* For android.control.afRegions in Camera 2.0,
	Resolution based on YUV output size*/
	u32	af_region_bottom;
	u32	reserved[PARAMETER_MAX_MEMBER-17];
	u32	err;
};

struct param_isp_flash {
	u32	cmd;
	u32	redeye;
	u32	flashintensity;
	u32	reserved[PARAMETER_MAX_MEMBER-4];
	u32	err;
};

struct param_isp_awb {
	u32	cmd;
	u32	illumination;
	u32	reserved[PARAMETER_MAX_MEMBER-3];
	u32	err;
};

struct param_isp_imageeffect {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_isp_iso {
	u32	cmd;
	u32	value;
	u32	reserved[PARAMETER_MAX_MEMBER-3];
	u32	err;
};

struct param_isp_adjust {
	u32	cmd;
	s32	contrast;
	s32	saturation;
	s32	sharpness;
	s32	exposure;
	s32	brightness;
	s32	hue;
	/* 0 or 1 */
	u32	hot_pixel_enable;
	/* -127 ~ 127 */
	s32	noise_reduction_strength;
	/* 0 or 1 */
	u32	shading_correction_enable;
	/* 0 or 1 */
	u32	user_gamma_enable;
	/* -127 ~ 127 */
	s32	edge_enhancement_strength;
	/* ISP_AdjustSceneIndexEnum */
	u32	user_scene_mode;
	u32	min_frame_time;
	u32	max_frame_time;
	u32	reserved[PARAMETER_MAX_MEMBER-16];
	u32	err;
};

struct param_isp_metering {
	u32	cmd;
	u32	win_pos_x;
	u32	win_pos_y;
	u32	win_width;
	u32	win_height;
	u32	exposure_mode;
	/* 0: Legacy, 1: Camera 2.0 */
	u32	cam_api_2p0;
	u32	reserved[PARAMETER_MAX_MEMBER-8];
	u32	err;
};

struct param_isp_afc {
	u32	cmd;
	u32	manual;
	u32	reserved[PARAMETER_MAX_MEMBER-3];
	u32	err;
};

struct param_scaler_imageeffect {
	u32	cmd;
	u32	arbitrary_cb;
	u32	arbitrary_cr;
	u32	yuv_range;
	u32	reserved[PARAMETER_MAX_MEMBER-5];
	u32	err;
};

struct param_scaler_input_crop {
	u32  cmd;
	u32  pos_x;
	u32  pos_y;
	u32  crop_width;
	u32  crop_height;
	u32  in_width;
	u32  in_height;
	u32  out_width;
	u32  out_height;
	u32  reserved[PARAMETER_MAX_MEMBER-10];
	u32  err;
};

struct param_scaler_output_crop {
	u32  cmd;
	u32  pos_x;
	u32  pos_y;
	u32  crop_width;
	u32  crop_height;
	u32  format;
	u32  reserved[PARAMETER_MAX_MEMBER-7];
	u32  err;
};

struct param_scaler_rotation {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_scaler_flip {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_3dnr_1stframe {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_fd_config {
	u32	cmd;
	u32	max_number;
	u32	roll_angle;
	u32	yaw_angle;
	s32	smile_mode;
	s32	blink_mode;
	u32	eye_detect;
	u32	mouth_detect;
	u32	orientation;
	u32	orientation_value;
	u32	reserved[PARAMETER_MAX_MEMBER-11];
	u32	err;
};

struct global_param {
	struct param_global_shotmode	shotmode; /* 0 */
};

/* To be added */
struct sensor_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_otf_output		otf_output;
	struct param_sensor_config	config;
	struct param_dma_output		dma_output;
};

struct buffer_param {
	struct param_control	control;
	struct param_otf_input	otf_input;
	struct param_otf_output	otf_output;
};

struct taa_param {
	struct param_control		control;
	struct param_otf_input		otf_input;	/* otf_input */
	struct param_dma_input		vdma1_input;	/* dma1_input */
	struct param_dma_input		ddma_input;	/* not use */
	struct param_otf_output		otf_output;	/* not use */
	struct param_dma_output		vdma4_output;	/* Before BDS */
	struct param_dma_output		vdma2_output;	/* After BDS */
	struct param_dma_output		ddma_output;	/* not use */
};

struct isp_param {
	struct param_control		control;
	struct param_otf_input		otf_input;	/* not use */
	struct param_dma_input		vdma1_input;	/* dma1_input */
	struct param_dma_input		vdma3_input;	/* not use */
	struct param_otf_output		otf_output;	/* otf_out */
	struct param_dma_output		vdma4_output;	/* not use */
	struct param_dma_output		vdma5_output;	/* not use */
};

struct drc_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_otf_output		otf_output;
};

struct scalerc_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_scaler_imageeffect	effect;
	struct param_scaler_input_crop	input_crop;
	struct param_scaler_output_crop	 output_crop;
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output;
};

struct odc_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_otf_output		otf_output;
};

struct dis_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_otf_output		otf_output;
};

struct tdnr_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_3dnr_1stframe	frame;
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output;
};

struct scalerp_param {
	struct param_control			control;
	struct param_otf_input			otf_input;
	struct param_scaler_imageeffect		effect;
	struct param_scaler_input_crop	input_crop;
	struct param_scaler_output_crop	 output_crop;
	struct param_scaler_rotation		rotation;
	struct param_scaler_flip		flip;
	struct param_otf_output			otf_output;
	struct param_dma_output			dma_output;
};

struct fd_param {
	struct param_control			control;
	struct param_otf_input			otf_input;
	struct param_dma_input			dma_input;
	struct param_fd_config			config;
};

struct is_param_region {
	struct global_param		global;
	struct sensor_param		sensor;
	struct buffer_param		buf;
	struct taa_param		taa;
	struct isp_param		isp;
	struct drc_param		drc;
	struct scalerc_param		scalerc;
	struct odc_param		odc;
	struct dis_param		dis;
	struct tdnr_param		tdnr;
	struct scalerp_param		scalerp;
	struct fd_param			fd;
};

#define	NUMBER_OF_GAMMA_CURVE_POINTS	32

struct is_sensor_tune {
	u32 exposure;
	u32 analog_gain;
	u32 frame_rate;
	u32 actuator_pos;
};

struct is_tune_gammacurve {
	u32 num_pts_x[NUMBER_OF_GAMMA_CURVE_POINTS];
	u32 num_pts_y_r[NUMBER_OF_GAMMA_CURVE_POINTS];
	u32 num_pts_y_g[NUMBER_OF_GAMMA_CURVE_POINTS];
	u32 num_pts_y_b[NUMBER_OF_GAMMA_CURVE_POINTS];
};

struct is_isp_tune {
	/* Brightness level : range 0~100, default : 7 */
	u32 brightness_level;
	/* Contrast level : range -127~127, default : 0 */
	s32 contrast_level;
	/* Saturation level : range -127~127, default : 0 */
	s32 saturation_level;
	s32 gamma_level;
	struct is_tune_gammacurve gamma_curve[4];
	/* Hue : range -127~127, default : 0 */
	s32 hue;
	/* Sharpness blur : range -127~127, default : 0 */
	s32 sharpness_blur;
	/* Despeckle : range -127~127, default : 0 */
	s32 despeckle;
	/* Edge color supression : range -127~127, default : 0 */
	s32 edge_color_supression;
	/* Noise reduction : range -127~127, default : 0 */
	s32 noise_reduction;
	/* (32*4 + 9)*4 = 548 bytes */
};

struct is_tune_region {
	struct is_sensor_tune sensor_tune;
	struct is_isp_tune isp_tune;
};

struct rational_t {
	u32 num;
	u32 den;
};

struct srational_t {
	s32 num;
	s32 den;
};

#define FLASH_FIRED_SHIFT	0
#define FLASH_NOT_FIRED		0
#define FLASH_FIRED		1

#define FLASH_STROBE_SHIFT				1
#define FLASH_STROBE_NO_DETECTION			0
#define FLASH_STROBE_RESERVED				1
#define FLASH_STROBE_RETURN_LIGHT_NOT_DETECTED		2
#define FLASH_STROBE_RETURN_LIGHT_DETECTED		3

#define FLASH_MODE_SHIFT			3
#define FLASH_MODE_UNKNOWN			0
#define FLASH_MODE_COMPULSORY_FLASH_FIRING	1
#define FLASH_MODE_COMPULSORY_FLASH_SUPPRESSION	2
#define FLASH_MODE_AUTO_MODE			3

#define FLASH_FUNCTION_SHIFT		5
#define FLASH_FUNCTION_PRESENT		0
#define FLASH_FUNCTION_NONE		1

#define FLASH_RED_EYE_SHIFT		6
#define FLASH_RED_EYE_DISABLED		0
#define FLASH_RED_EYE_SUPPORTED		1

enum apex_aperture_value {
	F1_0		= 0,
	F1_4		= 1,
	F2_0		= 2,
	F2_8		= 3,
	F4_0		= 4,
	F5_6		= 5,
	F8_9		= 6,
	F11_0		= 7,
	F16_0		= 8,
	F22_0		= 9,
	F32_0		= 10,
};

struct exif_attribute {
	struct rational_t exposure_time;
	struct srational_t shutter_speed;
	u32 iso_speed_rating;
	u32 flash;
	struct srational_t brightness;
};

struct is_frame_header {
	u32 valid;
	u32 bad_mark;
	u32 captured;
	u32 frame_number;
	struct exif_attribute	exif;
};

struct is_fd_rect {
	u32 offset_x;
	u32 offset_y;
	u32 width;
	u32 height;
};

struct is_face_marker {
	u32	frame_number;
	struct is_fd_rect face;
	struct is_fd_rect left_eye;
	struct is_fd_rect right_eye;
	struct is_fd_rect mouth;
	u32	roll_angle;
	u32 yaw_angle;
	u32	confidence;
	u32	is_tracked;
	u32	tracked_face_id;
	u32	smile_level;
	u32	blink_level;
};

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS4415)
struct is_debug_region {
	u32	frame_count;
	u32	reserved[PARAMETER_MAX_MEMBER-1];
};
#endif

#define MAX_FRAME_COUNT		8
#define MAX_FRAME_COUNT_PREVIEW	4
#define MAX_FRAME_COUNT_CAPTURE	1
#define MAX_FACE_COUNT		16

#define MAX_SHARED_COUNT	500

struct is_region {
	struct is_param_region	parameter;
	struct is_tune_region	tune;
	struct is_frame_header	header[MAX_FRAME_COUNT];
	struct is_face_marker	face[MAX_FACE_COUNT];
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS4415)
	struct is_debug_region	debug;
#endif
	u32			shared[MAX_SHARED_COUNT];
};

struct is_time_measure_us {
	u32  min_time_us;
	u32  max_time_us;
	u32  avrg_time_us;
	u32  current_time_us;
};

struct is_debug_frame_descriptor {
	u32	sensor_frame_time;
	u32	sensor_exposure_time;
	u32	sensor_analog_gain;
	u32	req_lei;
};

#define MAX_FRAMEDESCRIPTOR_CONTEXT_NUM	(30 * 20)	/* 600 frame */
#define MAX_VERSION_DISPLAY_BUF		(32)

struct is_share_region {
	u32	frame_time;
	u32	exposure_time;
	u32	analog_gain;

	u32	r_gain;
	u32	g_gain;
	u32	b_gain;

	u32	af_position;
	u32	af_status;
	u32 af_scene_type;

	u32	frame_descp_onoff_control;
	u32	frame_descp_update_done;
	u32	frame_descp_idx;
	u32	frame_descp_max_idx;

	struct is_debug_frame_descriptor
		dbg_frame_descp_ctx[MAX_FRAMEDESCRIPTOR_CONTEXT_NUM];

	u32 chip_id;
	u32 chip_rev_no;
	u8	ispfw_version_no[MAX_VERSION_DISPLAY_BUF];
	u8	ispfw_version_date[MAX_VERSION_DISPLAY_BUF];
	u8	sirc_sdk_version_no[MAX_VERSION_DISPLAY_BUF];
	u8	sirc_sdk_revsion_no[MAX_VERSION_DISPLAY_BUF];
	u8	sirc_sdk_version_date[MAX_VERSION_DISPLAY_BUF];

	/*measure timing*/
	struct is_time_measure_us	isp_sdk_Time;
};

struct is_debug_control {
	u32 write_point;	/* 0~500KB boundary*/
	u32 assert_flag;	/* 0:Not Inovked, 1:Invoked*/
	u32 pabort_flag;	/* 0:Not Inovked, 1:Invoked*/
	u32 dabort_flag;	/* 0:Not Inovked, 1:Invoked*/
	u32 pd_Ready_flag;	/* 0:Normal, 1:EnterIdle(Ready to power down)*/
	u32 isp_frameErr;	/* Frame Error Count.*/
	u32 drc_frame_err;	/* Frame Error Count.*/
	u32 scc_frame_err;	/* Frame Error Count.*/
	u32 odc_frame_err;	/* Frame Error Count.*/
	u32 dis_frame_err;	/* Frame Error Count.*/
	u32 tdnr_frame_err;	/* Frame Error Count.*/
	u32 scp_frame_err;	/* Frame Error Count.*/
	u32 fd_frame_err;	/* Frame Error Count.*/
	u32 isp_frame_drop;	/* Frame Drop Count.*/
	u32 drc_frame_drop;	/* Frame Drop Count.*/
	u32 dis_frame_drop;	/* Frame Drop Count.*/
	u32 fd_frame_drop;
};
#endif
