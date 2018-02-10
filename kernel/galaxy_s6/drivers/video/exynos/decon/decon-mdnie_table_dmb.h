#ifndef __MDNIE_TABLE_DMB_H__
#define __MDNIE_TABLE_DMB_H__

/* 2013.01.17 */

static unsigned short STANDARD_DMB_1[] = {
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

static unsigned short NATURAL_DMB_1[] = {
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

static unsigned short DYNAMIC_DMB_1[] = {
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

static unsigned short MOVIE_DMB_1[] = {
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

static unsigned short AUTO_DMB_1[] = {
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

struct mdnie_table dmb_table[MODE_MAX] = {
	MDNIE_SET(DYNAMIC_DMB),
	MDNIE_SET(STANDARD_DMB),
	MDNIE_SET(NATURAL_DMB),
	MDNIE_SET(MOVIE_DMB),
	MDNIE_SET(AUTO_DMB)
};

#endif
