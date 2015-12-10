/*
 * linux/arch/arm/mach-exynos/include/mach/exynos-coresight.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _EXYNOS_CORESIGHT_H
#define _EXYNOS_CORESIGHT_H

#define CORE_CNT	(4)
#define CLUSTER_CNT	(2)

#define CS_SJTAG_OFFSET	(0x8000)
#define SJTAG_STATUS	(0x4)
#define SJTAG_SOFT_LOCK	(1<<2)

/* DBG Registers */
#define DBGDIDR		(0x0)	/* RO */
#define DBGWFAR		(0x018)	/* RW */
#define DBGVCR		(0x01c)	/* RW */
#define DBGECR		(0x024)	/* RW or RAZ */
#define DBGDSCCR	(0x028)	/* RW or RAZ */
#define DBGDSMCR	(0x02c)	/* RW or RAZ */
#define DBGDTRRX	(0x080)	/* RW */
#define DBGPCSR		(0x084)	/* RO */
#define DBGDSCR		(0x088)	/* RW */
#define DBGDTRTX	(0x08c)	/* RW */
#define DBGDRCR		(0x090)	/* WO */
#define DBGEACR		(0x094)	/* RW */
#define DBGPCSR_2	(0x0a0)	/* RO */
#define DBGCIDSR	(0x0a4)	/* RO */
#define DBGVIDSR	(0x0a8) /* RO */

#define DBGBXVR0	(0x250) /* RW */
#define DBGBXVR1	(0x254) /* RW */

#define DBGOSLAR	(0x300) /* WO */
#define DBGOSLSR	(0x304) /* RO */

#define DBGPRCR		(0x310) /* RW */
#define DBGPRSR		(0x314) /* RO */

#define DBGITOCTRL	(0xef8) /* WO */
#define DBGITISR	(0xefc) /* RO */
#define DBGITCTRL	(0xf00) /* RW */

#define DBGCLAIMSET	(0xfa0) /* RW */
#define DBGCLAIMCLR	(0xfa4) /* RW */

#define DBGLAR		(0xfb0) /* WO */
#define DBGLSR		(0xfb4) /* RO */
#define DBGAUTHSTATUS	(0xfb8) /* RO */
#define DBGDEVID2	(0xfc0) /* RO */
#define DBGDEVID1	(0xfc4)	/* RO */
#define DBGDEVID0	(0xfc8) /* RO */
#define DBGDEVTYPE	(0xfcc) /* RO */

/* DBG breakpoint registers */
#define DBGBVRn(n)	(0x100 + (n * 4)) /* All RW */
#define DBGBCRn(n)	(0x140 + (n * 4))

/* DBG watchpoint registers */
#define DBGWVRn(n)	(0x180 + (n * 4)) /* All RW */
#define DBGWCRn(n)	(0x1c0 + (n * 4))

#define OSLOCK_MAGIC	(0xc5acce55)
#define DBGDSCR_MASK	(0x6c30fc3c)

/* DBGDIDR infomation */
#define DEBUG_ARCH_V7	(0x3)
#define DEBUG_ARCH_V7P1	(0x5)

enum KEEP_REG {

	BVR0 = 0,
	BVR1,
	BVR2,
	BVR3,
	BVR4,
	BVR5,
	BCR0,
	BCR1,
	BCR2,
	BCR3,
	BCR4,
	BCR5,

	WVR0,
	WVR1,
	WVR2,
	WVR3,
	WCR0,
	WCR1,
	WCR2,
	WCR3,

	WFAR,
	VCR,
	PRCR,

	/*Read DBGCLAIMCLR in save, write DBGCLAIMSET in restore. */
	CLAIM,
	DTRTX,
	DTRRX,
	/* Write saved value & DBGDSCR_MASK in restore. */
	DSCR,

	SLEEP_FLAG,
	NR_KEEP_REG,
};

#ifdef CONFIG_EXYNOS_CORESIGHT_PC_INFO
extern void exynos_cs_show_pcval(void);
#endif

#endif /* _EXYNOS_CORESIGHT_H */
