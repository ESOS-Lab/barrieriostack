#ifndef __MDNIE_TABLE_H__
#define __MDNIE_TABLE_H__

#include "./panels/mdnie.h"
/* 2015.01.08 */

/* SCR Position can be different each panel */
#define MDNIE_RED_R		0xA1		/* ASCR_WIDE_CR[7:0] */
#define MDNIE_RED_G		0xA2		/* ASCR_WIDE_CG[7:0] */
#define MDNIE_RED_B		0xA3		/* ASCR_WIDE_CB[7:0] */
#define MDNIE_GREEN_R		0xA4		/* ASCR_WIDE_YR[7:0] */
#define MDNIE_GREEN_G		0xA5		/* ASCR_WIDE_YG[7:0] */
#define MDNIE_GREEN_B		0xA6		/* ASCR_WIDE_YB[7:0] */
#define MDNIE_BLUE_R		0xA7		/* ASCR_WIDE_MR[7:0] */
#define MDNIE_BLUE_G		0xA8		/* ASCR_WIDE_MG[7:0] */
#define MDNIE_BLUE_B		0xA9		/* ASCR_WIDE_MB[7:0] */
#define MDNIE_WHITE_R		0xAA		/* ASCR_WIDE_WR[7:0] */
#define MDNIE_WHITE_G		0xAB		/* ASCR_WIDE_WG[7:0] */
#define MDNIE_WHITE_B		0xAC		/* ASCR_WIDE_WB[7:0] */

#define MDNIE_COLOR_BLIND_OFFSET	MDNIE_RED_R

#define COLOR_OFFSET_F1(x, y)		(((y << 10) - (((x << 10) * 353) / 326) + (30 << 10)) >> 10)
#define COLOR_OFFSET_F2(x, y)		(((y << 10) - (((x << 10) * 20) / 19) - (14 << 10)) >> 10)
#define COLOR_OFFSET_F3(x, y)		(((y << 10) + (((x << 10) * 185) / 42) - (16412 << 10)) >> 10)
#define COLOR_OFFSET_F4(x, y)		(((y << 10) + (((x << 10) * 337) / 106) - (12601 << 10)) >> 10)

/* color coordination order is WR, WG, WB */
static unsigned short coordinate_data_1[][3] = {
	{0xff, 0xff, 0xff}, /* dummy */
	{0xff, 0xfb, 0xfb}, /* Tune_1 */
	{0xff, 0xfc, 0xff}, /* Tune_2 */
	{0xfb, 0xf9, 0xff}, /* Tune_3 */
	{0xff, 0xfe, 0xfc}, /* Tune_4 */
	{0xff, 0xff, 0xff}, /* Tune_5 */
	{0xfb, 0xfc, 0xff}, /* Tune_6 */
	{0xfd, 0xff, 0xfa}, /* Tune_7 */
	{0xfc, 0xff, 0xfc}, /* Tune_8 */
	{0xfb, 0xff, 0xff}, /* Tune_9 */
};

static unsigned short coordinate_data_2[][3] = {
	{0xff, 0xff, 0xff}, /* dummy */
	{0xff, 0xf3, 0xeb}, /* Tune_1 */
	{0xff, 0xf4, 0xef}, /* Tune_2 */
	{0xff, 0xf5, 0xf3}, /* Tune_3 */
	{0xff, 0xf6, 0xec}, /* Tune_4 */
	{0xff, 0xf7, 0xef}, /* Tune_5 */
	{0xff, 0xf8, 0xf3}, /* Tune_6 */
	{0xff, 0xf9, 0xec}, /* Tune_7 */
	{0xff, 0xfa, 0xef}, /* Tune_8 */
	{0xff, 0xfb, 0xf3}, /* Tune_9 */
};

static unsigned short (*coordinate_data[MODE_MAX])[3] = {
	coordinate_data_1,
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_1,
	coordinate_data_1,
};

static unsigned short GRAYSCALE_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x0c0c,	/*ASCR udist ddist*/
	0x93,0x0c0c,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0xaaab,	/*ASCR div_u*/
	0x96,0xaaab,	/*ASCR div_d*/
	0x97,0xaaab,	/*ASCR div_r*/
	0x98,0xaaab,	/*ASCR div_l*/
	0x9a,0xd5ff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x2cf5,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x2a63,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xfeff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x4af9,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xfff8,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0x4cb3,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x4cb3,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x4cb3,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x9669,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0x9669,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x9669,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x1de2,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1de2,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0x1de2,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short GRAYSCALE_NEGATIVE_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x0c0c,	/*ASCR udist ddist*/
	0x93,0x0c0c,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0xaaab,	/*ASCR div_u*/
	0x96,0xaaab,	/*ASCR div_d*/
	0x97,0xaaab,	/*ASCR div_r*/
	0x98,0xaaab,	/*ASCR div_l*/
	0x9a,0xd5ff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x2cf5,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x2a63,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xfeff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x4af9,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xfff8,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xb34c,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0xb34c,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0xb34c,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x6996,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0x6996,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x6996,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0xe21d,	/*ASCR wide_Br wide_Yr*/
	0xa8,0xe21d,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xe21d,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0x00ff,	/*ASCR wide_Wr wide_Kr*/
	0xab,0x00ff,	/*ASCR wide_Wg wide_Kg*/
	0xac,0x00ff,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short SCREEN_CURTAIN_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/*CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x0c0c,	/*ASCR udist ddist*/
	0x93,0x0c0c,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0xaaab,	/*ASCR div_u*/
	0x96,0xaaab,	/*ASCR div_d*/
	0x97,0xaaab,	/*ASCR div_r*/
	0x98,0xaaab,	/*ASCR div_l*/
	0x9a,0xd5ff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x2cf5,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x2a63,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xfeff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x4af9,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xfff8,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0x0000,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x0000,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x0000,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x0000,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0x0000,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x0000,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x0000,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x0000,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0x0000,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0x0000,	/*ASCR wide_Wr wide_Kr*/
	0xab,0x0000,	/*ASCR wide_Wg wide_Kg*/
	0xac,0x0000,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

////////////////// UI /// /////////////////////
static unsigned short STANDARD_UI_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xf700,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x29ec,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x30ed,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xdd44,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x32e9,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x39f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1fed,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xe147,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short NATURAL_UI_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short DYNAMIC_UI_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0007,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0012,	/*CS weight*/
	0x5f,0x0080,	/*CC chsel strength*/
	0x60,0x0000,	/*CC lut r   0*/
	0x61,0x0a95,	/*CC lut r  16 144*/
	0x62,0x17a8,	/*CC lut r  32 160*/
	0x63,0x26bb,	/*CC lut r  48 176*/
	0x64,0x36cb,	/*CC lut r  64 192*/
	0x65,0x49db,	/*CC lut r  80 208*/
	0x66,0x5ceb,	/*CC lut r  96 224*/
	0x67,0x6ff8,	/*CC lut r 112 240*/
	0x68,0x82ff,	/*CC lut r 128 255*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x3729,	/*ASCR udist ddist*/
	0x93,0x1947,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x253d,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x1cd8,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x62ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6c00,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00f4,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short MOVIE_UI_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short AUTO_UI_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

////////////////// GALLERY /////////////////////
static unsigned short STANDARD_GALLERY_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x1818,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xf700,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x29ec,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x30ed,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xdd44,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x32e9,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x39f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1fed,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xe147,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short NATURAL_GALLERY_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x1818,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short DYNAMIC_GALLERY_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0007,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1 */
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0120,	/*AADE pe*/
	0x43,0x0120,	/*AADE pf*/
	0x44,0x0120,	/*AADE pb*/
	0x45,0x0120,	/*AADE ne*/
	0x46,0x0120,	/*AADE nf*/
	0x47,0x0120,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x2828,	/*AADE max_plus max_minus*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0012,	/*CS weight*/
	0x5f,0x0080,	/*CC chsel strength*/
	0x60,0x0000,	/*CC lut r   0*/
	0x61,0x0a95,	/*CC lut r  16 144*/
	0x62,0x17a8,	/*CC lut r  32 160*/
	0x63,0x26bb,	/*CC lut r  48 176*/
	0x64,0x36cb,	/*CC lut r  64 192*/
	0x65,0x49db,	/*CC lut r  80 208*/
	0x66,0x5ceb,	/*CC lut r  96 224*/
	0x67,0x6ff8,	/*CC lut r 112 240*/
	0x68,0x82ff,	/*CC lut r 128 255*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x3729,	/*ASCR udist ddist*/
	0x93,0x1947,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x253d,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x1cd8,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x62ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6c00,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00f4,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short MOVIE_GALLERY_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x1818,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short AUTO_GALLERY_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1 */
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0100,	/*AADE pe*/
	0x43,0x0100,	/*AADE pf*/
	0x44,0x0100,	/*AADE pb*/
	0x45,0x0100,	/*AADE ne*/
	0x46,0x0100,	/*AADE nf*/
	0x47,0x0100,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x2020,	/*AADE max_plus max_minus*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x48f0,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x4000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xd8ff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xd9ff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xe000,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00f6,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x3bd8,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00d9,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x14ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00f9,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

////////////////// VIDEO /////////////////////
static unsigned short STANDARD_VIDEO_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x0a0a,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xf700,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x29ec,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x30ed,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xdd44,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x32e9,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x39f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1fed,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xe147,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short NATURAL_VIDEO_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x0a0a,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short DYNAMIC_VIDEO_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0007,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1 */
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0100,	/*AADE pe*/
	0x43,0x0100,	/*AADE pf*/
	0x44,0x0100,	/*AADE pb*/
	0x45,0x0100,	/*AADE ne*/
	0x46,0x0100,	/*AADE nf*/
	0x47,0x0100,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x0a0a,	/*AADE max_plus max_minus*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0012,	/*CS weight*/
	0x5f,0x0080,	/*CC chsel strength*/
	0x60,0x0000,	/*CC lut r   0*/
	0x61,0x0a95,	/*CC lut r  16 144*/
	0x62,0x17a8,	/*CC lut r  32 160*/
	0x63,0x26bb,	/*CC lut r  48 176*/
	0x64,0x36cb,	/*CC lut r  64 192*/
	0x65,0x49db,	/*CC lut r  80 208*/
	0x66,0x5ceb,	/*CC lut r  96 224*/
	0x67,0x6ff8,	/*CC lut r 112 240*/
	0x68,0x82ff,	/*CC lut r 128 255*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x3729,	/*ASCR udist ddist*/
	0x93,0x1947,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x253d,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x1cd8,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x62ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6c00,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00f4,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short MOVIE_VIDEO_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x0a0a,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short AUTO_VIDEO_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0007,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1 */
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0100,	/*AADE pe*/
	0x43,0x0100,	/*AADE pf*/
	0x44,0x0100,	/*AADE pb*/
	0x45,0x0100,	/*AADE ne*/
	0x46,0x0100,	/*AADE nf*/
	0x47,0x0100,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x0a0a,	/*AADE max_plus max_minus*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0012,	/*CS weight*/
	0x5f,0x0080,	/*CC chsel strength*/
	0x60,0x0000,	/*CC lut r   0*/
	0x61,0x0a95,	/*CC lut r  16 144*/
	0x62,0x17a8,	/*CC lut r  32 160*/
	0x63,0x26bb,	/*CC lut r  48 176*/
	0x64,0x36cb,	/*CC lut r  64 192*/
	0x65,0x49db,	/*CC lut r  80 208*/
	0x66,0x5ceb,	/*CC lut r  96 224*/
	0x67,0x6ff8,	/*CC lut r 112 240*/
	0x68,0x82ff,	/*CC lut r 128 255*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x3729,	/*ASCR udist ddist*/
	0x93,0x1947,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x253d,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x1cd8,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x62ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6c00,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00f4,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

////////////////// VT /////////////////////
static unsigned short STANDARD_VT_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x1818,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xf700,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x29ec,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x30ed,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xdd44,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x32e9,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x39f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1fed,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xe147,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short NATURAL_VT_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x1818,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short DYNAMIC_VT_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0007,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1 */
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0120,	/*AADE pe*/
	0x43,0x0120,	/*AADE pf*/
	0x44,0x0120,	/*AADE pb*/
	0x45,0x0120,	/*AADE ne*/
	0x46,0x0120,	/*AADE nf*/
	0x47,0x0120,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x2828,	/*AADE max_plus max_minus*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0012,	/*CS weight*/
	0x5f,0x0080,	/*CC chsel strength*/
	0x60,0x0000,	/*CC lut r   0*/
	0x61,0x0a95,	/*CC lut r  16 144*/
	0x62,0x17a8,	/*CC lut r  32 160*/
	0x63,0x26bb,	/*CC lut r  48 176*/
	0x64,0x36cb,	/*CC lut r  64 192*/
	0x65,0x49db,	/*CC lut r  80 208*/
	0x66,0x5ceb,	/*CC lut r  96 224*/
	0x67,0x6ff8,	/*CC lut r 112 240*/
	0x68,0x82ff,	/*CC lut r 128 255*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x3729,	/*ASCR udist ddist*/
	0x93,0x1947,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x253d,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x1cd8,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x62ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6c00,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00f4,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short MOVIE_VT_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1*/
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0080,	/*AADE pe*/
	0x43,0x0080,	/*AADE pf*/
	0x44,0x0080,	/*AADE pb*/
	0x45,0x0080,	/*AADE ne*/
	0x46,0x0080,	/*AADE nf*/
	0x47,0x0080,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x1818,	/*AADE max_plus max_minus*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short BYPASS_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0006,	/*ASCR latency clk on 01 1 1*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short AUTO_VT_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1 */
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0100,	/*AADE pe*/
	0x43,0x0100,	/*AADE pf*/
	0x44,0x0100,	/*AADE pb*/
	0x45,0x0100,	/*AADE ne*/
	0x46,0x0100,	/*AADE nf*/
	0x47,0x0100,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x2020,	/*AADE max_plus max_minus*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

////////////////// CAMERA /////////////////////
static unsigned short CAMERA_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short AUTO_CAMERA_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x48f0,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x4000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xd8ff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xd9ff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xe000,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00f6,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x3bd8,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00d9,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x14ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00f9,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short NEGATIVE_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x0c0c,	/*ASCR udist ddist*/
	0x93,0x0c0c,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0xaaab,	/*ASCR div_u*/
	0x96,0xaaab,	/*ASCR div_d*/
	0x97,0xaaab,	/*ASCR div_r*/
	0x98,0xaaab,	/*ASCR div_l*/
	0x9a,0xd5ff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x2cf5,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x2a63,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xfeff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x4af9,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xfff8,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0x00ff,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0xff00,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0xff00,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0xff00,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0x00ff,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0xff00,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0xff00,	/*ASCR wide_Br wide_Yr*/
	0xa8,0xff00,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0x00ff,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0x00ff,	/*ASCR wide_Wr wide_Kr*/
	0xab,0x00ff,	/*ASCR wide_Wg wide_Kg*/
	0xac,0x00ff,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short COLOR_BLIND_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x0c0c,	/*ASCR udist ddist*/
	0x93,0x0c0c,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0xaaab,	/*ASCR div_u*/
	0x96,0xaaab,	/*ASCR div_d*/
	0x97,0xaaab,	/*ASCR div_r*/
	0x98,0xaaab,	/*ASCR div_l*/
	0x9a,0xd5ff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x2cf5,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x2a63,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xfeff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x4af9,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xfff8,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

////////////////// BROWSER /////////////////////
static unsigned short STANDARD_BROWSER_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xf700,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x29ec,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x30ed,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xdd44,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x32e9,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x39f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1fed,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xe147,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short NATURAL_BROWSER_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short DYNAMIC_BROWSER_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0007,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0012,	/*CS weight*/
	0x5f,0x0080,	/*CC chsel strength*/
	0x60,0x0000,	/*CC lut r   0*/
	0x61,0x0a95,	/*CC lut r  16 144*/
	0x62,0x17a8,	/*CC lut r  32 160*/
	0x63,0x26bb,	/*CC lut r  48 176*/
	0x64,0x36cb,	/*CC lut r  64 192*/
	0x65,0x49db,	/*CC lut r  80 208*/
	0x66,0x5ceb,	/*CC lut r  96 224*/
	0x67,0x6ff8,	/*CC lut r 112 240*/
	0x68,0x82ff,	/*CC lut r 128 255*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x3729,	/*ASCR udist ddist*/
	0x93,0x1947,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x253d,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x1cd8,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x62ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6c00,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00f4,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short MOVIE_BROWSER_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short AUTO_BROWSER_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x1729,	/*ASCR udist ddist*/
	0x93,0x1927,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x590b,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x3483,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x5cff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6800,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00f8,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

////////////////// eBOOK /////////////////////
static unsigned short AUTO_EBOOK_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf500,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xe300,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short DYNAMIC_EBOOK_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0007,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0012,	/*CS weight*/
	0x5f,0x0080,	/*CC chsel strength*/
	0x60,0x0000,	/*CC lut r   0*/
	0x61,0x0a95,	/*CC lut r  16 144*/
	0x62,0x17a8,	/*CC lut r  32 160*/
	0x63,0x26bb,	/*CC lut r  48 176*/
	0x64,0x36cb,	/*CC lut r  64 192*/
	0x65,0x49db,	/*CC lut r  80 208*/
	0x66,0x5ceb,	/*CC lut r  96 224*/
	0x67,0x6ff8,	/*CC lut r 112 240*/
	0x68,0x82ff,	/*CC lut r 128 255*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x3729,	/*ASCR udist ddist*/
	0x93,0x1947,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x253d,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x1cd8,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x62ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6c00,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00f4,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short STANDARD_EBOOK_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xf700,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x29ec,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x30ed,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xdd44,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x32e9,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x39f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1fed,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xe147,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short NATURAL_EBOOK_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short MOVIE_EBOOK_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x1000,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xd994,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x20f0,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x2ced,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x8ae4,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xe72f,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x50e3,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x38f6,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x1dee,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xdd59,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xef00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short AUTO_EMAIL_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf900,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xed00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short HMT_GRAY_8_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0006,	/*ASCR latency clk on 01 1 1*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short HMT_GRAY_16_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0006,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0006,	/*ASCR latency clk on 01 1 1*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short LOCAL_CE_1[] = {
	0x06,0x0007,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0007,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x10,0x0630,	/*SLCE gain cgain 00 000000 00 000000*/
	0x11,0x0014,	/*SLCE scene_on scene_tran scene_min_diff*/
	0x12,0x0090,	/*SLCE illum_gain*/
	0x13,0x71bf,	/*SLCE hb_size ref_offset*/
	0x14,0x70b0,	/*SLCE vb_size ref_gain*/
	0x15,0xfa7f,	/*SLCE bth bin_size_ratio*/
	0x16,0x0040,	/*SLCE dth min_ref_offset*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1 */
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0100,	/*AADE pe*/
	0x43,0x0100,	/*AADE pf*/
	0x44,0x0100,	/*AADE pb*/
	0x45,0x0100,	/*AADE ne*/
	0x46,0x0100,	/*AADE nf*/
	0x47,0x0100,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x2020,	/*AADE max_plus max_minus*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0014,	/*CS weight*/
	0x5f,0x0080,	/*CC chsel strength*/
	0x60,0x0000,	/*CC lut r   0*/
	0x61,0x22b1,	/*CC lut r  16 144*/
	0x62,0x38be,	/*CC lut r  32 160*/
	0x63,0x4cc9,	/*CC lut r  48 176*/
	0x64,0x63d5,	/*CC lut r  64 192*/
	0x65,0x7ae0,	/*CC lut r  80 208*/
	0x66,0x89eb,	/*CC lut r  96 224*/
	0x67,0x97f5,	/*CC lut r 112 240*/
	0x68,0xa4ff,	/*CC lut r 128 255*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x1729,	/*ASCR udist ddist*/
	0x93,0x1927,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x590b,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x3483,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x50ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x6000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short LOCAL_CE_TEXT_1[] = {
	0x06,0x0007,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0001,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0007,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x10,0x0630,	/*SLCE gain cgain 00 000000 00 000000*/
	0x11,0x0014,	/*SLCE scene_on scene_tran scene_min_diff*/
	0x12,0x0090,	/*SLCE illum_gain*/
	0x13,0x71bf,	/*SLCE hb_size ref_offset*/
	0x14,0x70b0,	/*SLCE vb_size ref_gain*/
	0x15,0xfa7f,	/*SLCE bth bin_size_ratio*/
	0x16,0x0040,	/*SLCE dth min_ref_offset*/
	0x30,0x0001,	/*AADE re nb nf ne pb pf pe fa_en re de_off re aa_off 0 0 0 0 0 0 0 0 000 0 000 1 */
	0x40,0x0080,	/*AADE egth*/
	0x42,0x0100,	/*AADE pe*/
	0x43,0x0100,	/*AADE pf*/
	0x44,0x0100,	/*AADE pb*/
	0x45,0x0100,	/*AADE ne*/
	0x46,0x0100,	/*AADE nf*/
	0x47,0x0100,	/*AADE nb*/
	0x48,0xffff,	/*AADE max ratio*/
	0x49,0x0001,	/*AADE min ratio*/
	0x4a,0x2020,	/*AADE max_plus max_minus*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0014,	/*CS weight*/
	0x90,0x0110,	/*ASCR Linearize_on Skin_on Strength 0 1 10*/
	0x91,0x67a9,	/*ASCR cb cr*/
	0x92,0x5629,	/*ASCR udist ddist*/
	0x93,0x1967,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x17d0,	/*ASCR div_u*/
	0x96,0x31f4,	/*ASCR div_d*/
	0x97,0x51ec,	/*ASCR div_r*/
	0x98,0x13e2,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0xa090,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0xa000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xff00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xff00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,

};

#if defined(CONFIG_LCD_HMT)
static unsigned short HMT_3000K_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0014,	/*CS weight*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xc800,	/*ASCR wide_Wg wide_Kg*/
	0xac,0x8000,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short HMT_4000K_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0014,	/*CS weight*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xde00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xac00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short HMT_5000K_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0014,	/*CS weight*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xeb00,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xcc00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short HMT_6500K_1[] = {
	0x06,0x0006,	/*SLCE mem_clk mem_cs avg latency clk on 0 0 0 01 1 0*/
	0x07,0x0000,	/*AADE mem_clk mem_cs lb_clk lb_module 0 0 0 0*/
	0x08,0x0006,	/*AADE latency clk on 01 1 0*/
	0x09,0x0007,	/* CS  latency clk on 01 1 0*/
	0x0a,0x0006,	/* CC  latency clk on 01 1 0*/
	0x0b,0x0007,	/*ASCR latency clk on 01 1 1*/
	0x50,0x1010,	/*CS hg ry*/
	0x51,0x1010,	/*CS hg gc*/
	0x52,0x1010,	/*CS hg bm*/
	0x53,0x0014,	/*CS weight*/
	0x90,0x0000,	/*ASCR Linearize_on Skin_on Strength 0 0 00*/
	0x91,0x6a9a,	/*ASCR cb cr*/
	0x92,0x251a,	/*ASCR udist ddist*/
	0x93,0x162a,	/*ASCR rdist ldist*/
	0x94,0x0000,	/*ASCR div_u div_d div_r div_l*/
	0x95,0x375a,	/*ASCR div_u*/
	0x96,0x4ec5,	/*ASCR div_d*/
	0x97,0x5d17,	/*ASCR div_r*/
	0x98,0x30c3,	/*ASCR div_l*/
	0x9a,0xffff,	/*ASCR skin_Rr skin_Yr*/
	0x9b,0x00ff,	/*ASCR skin_Rg skin_Yg*/
	0x9c,0x0000,	/*ASCR skin_Rb skin_Yb*/
	0x9d,0xffff,	/*ASCR skin_Mr skin_Wr*/
	0x9e,0x00ff,	/*ASCR skin_Mg skin_Wg*/
	0x9f,0xffff,	/*ASCR skin_Mb skin_Wb*/
	0xa1,0xff00,	/*ASCR wide_Rr wide_Cr*/
	0xa2,0x00ff,	/*ASCR wide_Rg wide_Cg*/
	0xa3,0x00ff,	/*ASCR wide_Rb wide_Cb*/
	0xa4,0x00ff,	/*ASCR wide_Gr wide_Mr*/
	0xa5,0xff00,	/*ASCR wide_Gg wide_Mg*/
	0xa6,0x00ff,	/*ASCR wide_Gb wide_Mb*/
	0xa7,0x00ff,	/*ASCR wide_Br wide_Yr*/
	0xa8,0x00ff,	/*ASCR wide_Bg wide_Yg*/
	0xa9,0xff00,	/*ASCR wide_Bb wide_Yb*/
	0xaa,0xff00,	/*ASCR wide_Wr wide_Kr*/
	0xab,0xf700,	/*ASCR wide_Wg wide_Kg*/
	0xac,0xed00,	/*ASCR wide_Wb wide_Kb*/
	0xff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};
#endif

struct mdnie_table bypass_table[BYPASS_MAX] = {
	[BYPASS_ON] = MDNIE_SET(BYPASS)
};

struct mdnie_table accessibility_table[ACCESSIBILITY_MAX] = {
	[NEGATIVE] = MDNIE_SET(NEGATIVE),
	MDNIE_SET(COLOR_BLIND),
	MDNIE_SET(SCREEN_CURTAIN),
	MDNIE_SET(GRAYSCALE),
	MDNIE_SET(GRAYSCALE_NEGATIVE)
};

struct mdnie_table hbm_table[HBM_MAX] = {
	[HBM_ON] = MDNIE_SET(LOCAL_CE),
	MDNIE_SET(LOCAL_CE_TEXT)
};
#if defined(CONFIG_LCD_HMT)
struct mdnie_table hmt_table[HMT_MDNIE_MAX] = {
	[HMT_MDNIE_ON] = MDNIE_SET(HMT_3000K),
	MDNIE_SET(HMT_4000K),
	MDNIE_SET(HMT_5000K),
	MDNIE_SET(HMT_6500K)
};
#endif
struct mdnie_table tuning_table[SCENARIO_MAX][MODE_MAX] = {
	{
		MDNIE_SET(DYNAMIC_UI),
		MDNIE_SET(STANDARD_UI),
		MDNIE_SET(NATURAL_UI),
		MDNIE_SET(MOVIE_UI),
		MDNIE_SET(AUTO_UI)
	}, {
		MDNIE_SET(DYNAMIC_VIDEO),
		MDNIE_SET(STANDARD_VIDEO),
		MDNIE_SET(NATURAL_VIDEO),
		MDNIE_SET(MOVIE_VIDEO),
		MDNIE_SET(AUTO_VIDEO)
	},
	[CAMERA_MODE] = {
		MDNIE_SET(CAMERA),
		MDNIE_SET(CAMERA),
		MDNIE_SET(CAMERA),
		MDNIE_SET(CAMERA),
		MDNIE_SET(AUTO_CAMERA)
	},
	[GALLERY_MODE] = {
		MDNIE_SET(DYNAMIC_GALLERY),
		MDNIE_SET(STANDARD_GALLERY),
		MDNIE_SET(NATURAL_GALLERY),
		MDNIE_SET(MOVIE_GALLERY),
		MDNIE_SET(AUTO_GALLERY)
	}, {
		MDNIE_SET(DYNAMIC_VT),
		MDNIE_SET(STANDARD_VT),
		MDNIE_SET(NATURAL_VT),
		MDNIE_SET(MOVIE_VT),
		MDNIE_SET(AUTO_VT)
	}, {
		MDNIE_SET(DYNAMIC_BROWSER),
		MDNIE_SET(STANDARD_BROWSER),
		MDNIE_SET(NATURAL_BROWSER),
		MDNIE_SET(MOVIE_BROWSER),
		MDNIE_SET(AUTO_BROWSER)
	},{
		MDNIE_SET(DYNAMIC_EBOOK),
		MDNIE_SET(STANDARD_EBOOK),
		MDNIE_SET(NATURAL_EBOOK),
		MDNIE_SET(MOVIE_EBOOK),
		MDNIE_SET(AUTO_EBOOK)
	}, {
		MDNIE_SET(AUTO_EMAIL),
		MDNIE_SET(AUTO_EMAIL),
		MDNIE_SET(AUTO_EMAIL),
		MDNIE_SET(AUTO_EMAIL),
		MDNIE_SET(AUTO_EMAIL)
	}, {
		MDNIE_SET(HMT_GRAY_8),
		MDNIE_SET(HMT_GRAY_8),
		MDNIE_SET(HMT_GRAY_8),
		MDNIE_SET(HMT_GRAY_8),
		MDNIE_SET(HMT_GRAY_8)
	}, {
		MDNIE_SET(HMT_GRAY_16),
		MDNIE_SET(HMT_GRAY_16),
		MDNIE_SET(HMT_GRAY_16),
		MDNIE_SET(HMT_GRAY_16),
		MDNIE_SET(HMT_GRAY_16)
	}
};
#endif
