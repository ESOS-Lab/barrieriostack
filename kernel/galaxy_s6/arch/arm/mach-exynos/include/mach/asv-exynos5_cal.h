/*
 * linux/arch/arm/mach-exynos/include/mach/asv-exynos5_cal.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5 - support ASV drvier to interact with cal.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_NEW_EXYNOS5_ASV_CAL_H
#define __ASM_ARCH_NEW_EXYNOS5_ASV_CAL_H __FILE__

/* Use by ASV drvier */
#ifdef CONFIG_SOC_EXYNOS5433

#include <linux/io.h>
#include <mach/map.h>
#include <mach/otp.h>
u32 re_err(void);

#define Assert(b) (!(b) ? re_err() : 0)

#define SetBits(uAddr, uBaseBit, uMaskValue, uSetValue) \
	do { \
		__raw_writel((__raw_readl((const volatile void __iomem *)uAddr) & \
		 ~((uMaskValue) << (uBaseBit))) | \
		 (((uMaskValue) & (uSetValue)) << (uBaseBit)), uAddr) \
	} while (0);

#define GetBits(uAddr, uBaseBit, uMaskValue) \
	((__raw_readl((const volatile void __iomem *) uAddr) >> (uBaseBit)) & (uMaskValue))

#define CHIPID_BASE		S5P_VA_CHIPID
#define CHIPID_ASV_TBL_BASE	S5P_VA_CHIPID2
#define CHIPID_ABB_TBL_BASE	S5P_VA_CHIPID3
#define SYSREG_EGL_BASE		S5P_VA_CHIPID4
#define CHIPID_ABBG_BASE	S5P_VA_CHIPID5

/* Use by CAL */
#else

#ifndef __cplusplus
#define false		(0)
#define true		(1)
#endif

typedef unsigned char		bool;
typedef unsigned long long	u64;
typedef unsigned int		u32;
typedef unsigned short		u16;
typedef unsigned char		u8;
typedef signed long long	s64;
typedef signed int		s32;
typedef signed short		s16;
typedef signed char		s8;

#define __raw_writel(addr, data) (*(volatile u32 *)(addr) = (data))
#define __raw_readl(addr) (*(volatile u32 *)(addr))

#define NULL 0

#define SetBits(uAddr, uBaseBit, uMaskValue, uSetValue) \
	do { \
		__raw_writel(uAddr, (__raw_readl(uAddr) & \
		~((uMaskValue) << (uBaseBit))) | \
		(((uMaskValue)&(uSetValue)) << (uBaseBit))) \
	} while (0);

#define GetBits(uAddr, uBaseBit, uMaskValue) \
	((__raw_readl(uAddr) >> (uBaseBit)) & (uMaskValue))

u32 stop (void)
{
	while (1) {
	};
	return 1;
}
#define Assert(b) (!(b) ? stop() : 0)

#define CHIPID_BASE		0x10000000
#define CHIPID_ASV_TBL_BASE	0x10004000
#define CHIPID_ABB_TBL_BASE	0x105D0000
#define SYSREG_EGL_BASE		0x11810000
#define CHIPID_ABBG_BASE	0x105C0000

#endif

/* COMMON code */
#define ENABLE		(1)
#define DISABLE		(0)

#define PKG_ID			(CHIPID_BASE + 0x0004)
#define PRODUCT_ID		(CHIPID_BASE + 0x0000)

#define ABB_X060		0
#define ABB_X065		1
#define ABB_X070		2
#define ABB_X075		3
#define ABB_X080		4
#define ABB_X085		5
#define ABB_X090		6
#define ABB_X095		7
#define ABB_X100		8
#define ABB_X105		9
#define ABB_X110		10
#define ABB_X115		11
#define ABB_X120		12
#define ABB_X125		13
#define ABB_X130		14
#define ABB_X135		15
#define ABB_X140		16
#define ABB_X145		17
#define ABB_X150		18
#define ABB_X155		19
#define ABB_X160		20
#define ABB_BYPASS		255

enum SYSC_DVFS_SEL {
	SYSC_DVFS_EGL,
	SYSC_DVFS_KFC,
	SYSC_DVFS_INT,
	SYSC_DVFS_MIF,
	SYSC_DVFS_G3D,
	SYSC_DVFS_CAM,
	SYSC_DVFS_NUM
};

enum SYSC_DVFS_LVL {
	SYSC_DVFS_L0 = 0,
	SYSC_DVFS_L1,
	SYSC_DVFS_L2,
	SYSC_DVFS_L3,
	SYSC_DVFS_L4,
	SYSC_DVFS_L5,
	SYSC_DVFS_L6,
	SYSC_DVFS_L7,
	SYSC_DVFS_L8,
	SYSC_DVFS_L9,
	SYSC_DVFS_L10,
	SYSC_DVFS_L11,
	SYSC_DVFS_L12,
	SYSC_DVFS_L13,
	SYSC_DVFS_L14,
	SYSC_DVFS_L15,
	SYSC_DVFS_L16,
	SYSC_DVFS_L17,
	SYSC_DVFS_L18,
	SYSC_DVFS_L19,
	SYSC_DVFS_L20,
	SYSC_DVFS_L21,
	SYSC_DVFS_L22,
	SYSC_DVFS_L23,
	SYSC_DVFS_L24,
};


enum SYSC_ASV_GROUP {
	SYSC_ASV_0 = 0,
	SYSC_ASV_1,
	SYSC_ASV_2,
	SYSC_ASV_3,
	SYSC_ASV_4,
	SYSC_ASV_5 = 5,
	SYSC_ASV_6,
	SYSC_ASV_7,
	SYSC_ASV_8,
	SYSC_ASV_9,
	SYSC_ASV_10,
	SYSC_ASV_11 = 11,
	SYSC_ASV_12 = 12,
	SYSC_ASV_13,
	SYSC_ASV_14,
	SYSC_ASV_15 = 15,
	SYSC_ASV_MAX /* ASV limit */
};

void cal_init(void);
s32 cal_get_max_lv(u32 id);
s32 cal_get_min_lv(u32 id);
u32 cal_get_volt(u32 id, s32 level);
u32 cal_get_freq(u32 id, s32 level);

u32 cal_get_abb(u32 id, s32 level);
bool cal_use_dynimic_abb(u32 id);
void cal_set_abb(u32 id, u32 abb);

void cal_set_ema(u32 id , u32 setvolt);
bool cal_use_dynimic_ema(u32 id);

u32 cal_get_ids_group(void);
u32 cal_get_table_ver(void);
u32 cal_get_dram_size(void);
bool cal_is_fused_speed_grp(void);
u32 cal_get_ids(void);
u32 cal_get_asv_grp(u32 id, s32 level);
u32 is_max_limit_sample(void);

#endif
