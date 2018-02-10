/*
 * es705-routes.h  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES705_ROUTES_H
#define _ES705_ROUTES_H

/*
 * Defines to control Audience routing in Audio FW (HAL) for Zero/Zero-F ATT, TMO, BMC model.
 * This header file should be copied in 'android/device/samsung/[productname]/AudioData/include/es705-routes.h' and then included by HAL.
 */
typedef unsigned int u32; /* define for platform build */
#define SYSFS_PATH_PRESET   "/sys/class/earsmart/control/route_value"
#define SYSFS_PATH_VEQ	"/sys/class/earsmart/control/veq_control_set"
#define SYSFS_PATH_EXTRAVOLUME	"/sys/class/earsmart/control/extra_volume"
#define Call_HS_NB			0  /* Call, Headset, Narrow Band */
#define Call_FT_NB			1  /* Call, Far Talk, Narrow Band */
#define Call_CT_NB			2  /* Call, Close Talk, Narrow Band */
#define Call_FT_NB_NR_OFF	3  /* Call, Far Talk, NB, NR off */
#define Call_CT_NB_NR_OFF	4  /* Call, Close Talk, NB, NR off */
#define Call_BT_NB			10 /* Call, BT, NB */
#define Call_TTY_VCO			11 /* Call, TTY HCO NB */
#define Call_TTY_HCO			12 /* Call, TTY VCO NB */
#define Call_TTY_FULL		13 /* Call, TTY FULL NB */
#define LOOPBACK_CT		17 /* Loopback, Close Talk */
#define LOOPBACK_FT			18 /* Loopback, Far Talk */
#define LOOPBACK_HS		19 /* Loopback, Headset */
#define Call_BT_WB			20 /* Call, BT, WB */
#define Call_HS_WB			21 /* Call, Headset, Wide Band */
#define Call_FT_WB			22 /* Call, Far Talk, Wide Band */
#define Call_CT_WB			23 /* Call, Close Talk, Wide Band */
#define Call_FT_WB_NR_OFF	24 /* Call, Far Talk, WB, NR off */
#define Call_CT_WB_NR_OFF	25 /* Call, Close Talk, WB, NR off */
#define AUDIENCE_SLEEP		40 /* Route none, Audience Sleep State */

enum {
	ROUTE_HS_NB,		/*  0 */
	ROUTE_FT_NB_NS_ON,
	ROUTE_CT_NB_NS_ON,
	ROUTE_FT_NB_NS_OFF,
	ROUTE_CT_NB_NS_OFF,
	ROUTE_SWBYPASS,		/*  5 */
	ROUTE_2CH_PB,
	ROUTE_AZOOM_INTERVIEW,
	ROUTE_AZOOM_CONVERSATION,
	ROUTE_DUMMY_A,
	ROUTE_BT_NB,		/* 10 */
	ROUTE_TTY_VCO,
	ROUTE_TTY_HCO,
	ROUTE_TTY_FULL,
	ROUTE_DUMMY_C,
	ROUTE_DUMMY_D,		/* 15 */
	ROUTE_DUMMY_E,
	ROUTE_CT_LOOPBACK,
	ROUTE_FT_LOOPBACK,
	ROUTE_HS_LOOPBACK,
	ROUTE_BT_WB,		/* 20 */
	ROUTE_HS_WB,
	ROUTE_FT_WB_NS_ON,
	ROUTE_CT_WB_NS_ON,
	ROUTE_FT_WB_NS_OFF,
	ROUTE_CT_WB_NS_OFF,	/* 25 */
	ROUTE_DUMMY_G,
	ROUTE_CVQ,
	ROUTE_FUSION_HS_NB_BYPASS,
	ROUTE_FUSION_HS_WB_BYPASS,
	ROUTE_FUSION_CT_LOOPBACK,	/* 30 */
	ROUTE_FUSION_FT_LOOPBACK,
	ROUTE_FUSION_FT_NB_NS_ON,
	ROUTE_FUSION_CT_NB_NS_ON,
	ROUTE_FUSION_FT_NB_NS_OFF,
	ROUTE_FUSION_CT_NB_NS_OFF,	/* 35 */
	ROUTE_FUSION_FT_WB_NS_ON,
	ROUTE_FUSION_CT_WB_NS_ON,
	ROUTE_FUSION_FT_WB_NS_OFF,
	ROUTE_FUSION_CT_WB_NS_OFF,
	ROUTE_MAX		/* 40 */
};

enum {
	RATE_NB,
	RATE_WB,
	RATE_SWB,
	RATE_FB,
	RATE_MAX
};

#define TERMINATED			0xFFFFFFFF
#define AUD_FMT				0x903101F5 /* 501 */
#define ROUTE_1MIC_1FEOUT		0x903103E9 /* 1001 */
#define ROUTE_2MIC_1FEOUT_FT	0x9031041C /* 1052 */
#define ROUTE_2MIC_1FEOUT_CT	0x9031041B /* 1051 */
#define ROUTE_2CH_PT		0x90310483 /* 1155 */
#define ROUTE_2CH_16K_PT	0x90310358 /* 856 */
#define ALGO_1MIC_NS		0x9031024E /* 590 */
#define ALGO_2MIC_NS_FT_NB	0x90310391 /* 913 */
#define ALGO_2MIC_NS_CT_NB	0x9031022A /* 554 */
#define ALGO_2MIC_NS_FT_WB	0x90310392 /* 914 */
#define ALGO_2MIC_NS_CT_WB	0x9031022B /* 555 */
#define ALGO_NS_16K			0x9031024F /* 591 */
#define VP_OFF				0x9031026D /* 621 */
#define ROUTE_2CH_BYPASS	0x90310483 /* 1155 */
#define ALGO_2CH_BYPASS	0x90310365 /* 869 */
#define NS_LEVEL_CT_NB_NS_OFF	0x903103BD /* 957 */
#define NS_LEVEL_FT_WB_NS_OFF	0x903103E7 /* 999 */
#define NS_LEVEL_CT_WB_NS_OFF	0x903103BE /* 958 */

static const u32 route_off[] = {
	TERMINATED,
};

static const u32 route_id_00[] = { /* ROUTE_HS_NB */
	AUD_FMT,
	ROUTE_1MIC_1FEOUT,
	ALGO_1MIC_NS,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_01[] = { /* ROUTE_FT_NB_NS_ON */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_FT,
	ALGO_2MIC_NS_FT_NB,
	TERMINATED
};

static const u32 route_id_02[] = { /* ROUTE_CT_NB_NS_ON */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_CT,
	ALGO_2MIC_NS_CT_NB,
	TERMINATED
};

static const u32 route_id_03[] = { /* ROUTE_FT_NB_NS_OFF */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_FT,
	ALGO_2MIC_NS_FT_NB,
	TERMINATED
};

static const u32 route_id_04[] = { /* ROUTE_CT_NB_NS_OFF */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_CT,
	ALGO_2MIC_NS_CT_NB,	
	NS_LEVEL_CT_NB_NS_OFF,
	TERMINATED
};

static const u32 route_id_05[] = { /* ROUTE_SWBYPASS  */
	AUD_FMT,
	ROUTE_2CH_BYPASS,
	ALGO_2CH_BYPASS,
	TERMINATED
};

static const u32 route_id_06[] = { /* ROUTE_2CH_PB */
	AUD_FMT,
	ROUTE_2CH_PT,
	ROUTE_2CH_16K_PT,
	TERMINATED
};

static const u32 route_id_07[] = { /* ROUTE_AZOOM_INTERVIEW */
	TERMINATED
};

static const u32 route_id_08[] = { /* ROUTE_AZOOM_CONVERSATION */
	TERMINATED
};

static const u32 route_id_09[] = { /* ROUTE_DUMMY_A */
	TERMINATED
};

static const u32 route_id_10[] = { /* ROUTE_BT_NB */
	AUD_FMT,
	ROUTE_1MIC_1FEOUT,
	ALGO_1MIC_NS,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_11[] = { /* ROUTE_TTY_VCO */
	AUD_FMT,
	ROUTE_1MIC_1FEOUT,
	ALGO_1MIC_NS,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_12[] = { /* ROUTE_TTY_HCO */
	AUD_FMT,
	ROUTE_1MIC_1FEOUT,
	ALGO_1MIC_NS,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_13[] = { /* ROUTE_TTY_FULL */
	AUD_FMT,
	ROUTE_1MIC_1FEOUT,
	ALGO_1MIC_NS,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_14[] = { /* ROUTE_DUMMY_C */
	TERMINATED
};

static const u32 route_id_15[] = { /* ROUTE_DUMMY_D */
	TERMINATED
};

static const u32 route_id_16[] = { /* ROUTE_DUMMY_E */
	TERMINATED
};

static const u32 route_id_17[] = { /* ROUTE_CT_LOOPBACK */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_CT,
	ALGO_2MIC_NS_CT_WB,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_18[] = { /* ROUTE_FT_LOOPBACK */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_FT,
	ALGO_2MIC_NS_FT_WB,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_19[] = { /* ROUTE_HS_LOOPBACK */
	AUD_FMT,
	ROUTE_1MIC_1FEOUT,
	ALGO_NS_16K,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_20[] = { /* ROUTE_BT_WB */
	AUD_FMT,
	ROUTE_1MIC_1FEOUT,
	ALGO_NS_16K,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_21[] = { /* ROUTE_HS_WB */
	AUD_FMT,
	ROUTE_1MIC_1FEOUT,
	ALGO_NS_16K,
	VP_OFF,
	TERMINATED
};

static const u32 route_id_22[] = { /* ROUTE_FT_WB_NS_ON */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_FT,
	ALGO_2MIC_NS_FT_WB,
	TERMINATED
};

static const u32 route_id_23[] = { /* ROUTE_CT_WB_NS_ON */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_CT,
	ALGO_2MIC_NS_CT_WB,
	TERMINATED
};

static const u32 route_id_24[] = { /* ROUTE_FT_WB_NS_OFF */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_FT,
	ALGO_2MIC_NS_FT_WB,
	NS_LEVEL_FT_WB_NS_OFF,
	TERMINATED
};

static const u32 route_id_25[] = { /* ROUTE_CT_WB_NS_OFF */
	AUD_FMT,
	ROUTE_2MIC_1FEOUT_CT,
	ALGO_2MIC_NS_CT_WB,
	NS_LEVEL_CT_WB_NS_OFF,
	TERMINATED
};


static const u32 route_id_26[] = { /* ROUTE_DUMMY_G */
	TERMINATED
};

static const u32 route_id_27[] = { /* ROUTE_CVQ */
	0xb05a0c00,
	0xb05b0000,
	0xb0640038,
	0xb0640190,
	0xb0630103,
	0xb0630119,
	0xb0680300,
	0xb0681900,
	0xb0690000,
	0xb015000b,
	0x9017e02b,		/* Sensory input gain reg = 0xe02b */
	0x90181000,		/* Sensory input gain value = 0x1000 */
	TERMINATED,
};

static const u32 route_id_28[] = { /* ROUTE_FUSION_HS_NB_BYPASS */
	TERMINATED
};

static const u32 route_id_29[] = { /* ROUTE_FUSION_HS_WB_BYPASS */
	TERMINATED
};

static const u32 route_id_30[] = { /* ROUTE_FUSION_CT_LOOPBACK */
	TERMINATED
};

static const u32 route_id_31[] = { /* ROUTE_FUSION_FT_LOOPBACK */
	TERMINATED
};

static const u32 route_id_32[] = { /* ROUTE_FUSION_FT_NB_NS_ON */
	TERMINATED
};

static const u32 route_id_33[] = { /* ROUTE_FUSION_CT_NB_NS_ON */
	TERMINATED
};

static const u32 route_id_34[] = { /* ROUTE_FUSION_FT_NB_NS_OFF */
	TERMINATED
};

static const u32 route_id_35[] = { /* ROUTE_FUSION_CT_NB_NS_OFF */
	TERMINATED
};

static const u32 route_id_36[] = { /* ROUTE_FUSION_FT_WB_NS_ON */
	TERMINATED
};

static const u32 route_id_37[] = { /* ROUTE_FUSION_CT_WB_NS_ON */
	TERMINATED
};

static const u32 route_id_38[] = { /* ROUTE_FUSION_FT_WB_NS_OFF */
	TERMINATED
};

static const u32 route_id_39[] = { /* ROUTE_FUSION_CT_WB_NS_OFF */
	TERMINATED
};

static const u32 route_none[] = { /* ROUTE_MAX */
	TERMINATED
};

static const u32 *es705_route_configs[] = {
	route_id_00,	
	route_id_01,	
	route_id_02,	
	route_id_03,	
	route_id_04,	
	route_id_05,	
	route_id_06,	
	route_id_07,	
	route_id_08,	
	route_id_09,	
	route_id_10,
	route_id_11,
	route_id_12,
	route_id_13,
	route_id_14,
	route_id_15,
	route_id_16,
	route_id_17,
	route_id_18,
	route_id_19,
	route_id_20,
	route_id_21,
	route_id_22,
	route_id_23,
	route_id_24,
	route_id_25,
	route_id_26,
	route_id_27,
	route_id_28,
	route_id_29,
	route_id_30,
	route_id_31,
	route_id_32,
	route_id_33,
	route_id_34,
	route_id_35,
	route_id_36,
	route_id_37,
	route_id_38,
	route_id_39,
	route_none,
};
#endif
