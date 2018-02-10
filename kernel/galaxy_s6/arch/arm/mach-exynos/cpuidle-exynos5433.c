/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * CPUIDLE driver for EXYNOS5433
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/tick.h>
#include <linux/cpu.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#ifdef CONFIG_EXYNOS_MIPI_LLI
#include <linux/mipi-lli.h>
#endif

#if defined(CONFIG_MUIC_NOTIFIER) && !defined(CONFIG_SEC_FACTORY)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#include <asm/suspend.h>
#include <asm/tlbflush.h>

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/cpufreq.h>
#include <mach/exynos-pm.h>
#include <mach/pmu_cal_sys.h>
#include <mach/cpuidle_profiler.h>
#include <mach/devfreq.h>

#ifdef CONFIG_SND_SAMSUNG_AUDSS
#include <sound/exynos.h>
#endif

#include <plat/pm.h>

#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM_NS + 0x24)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM_NS + 0x20)

#define EXYNOS_CHECK_DIRECTGO	0xFCBA0D10
#define EXYNOS_CHECK_C2		0xABAC0000
#define EXYNOS_CHECK_LPA	0xABAD0000
#define EXYNOS_CHECK_DSTOP	0xABAE0000

#define CTRL_FORCE_SHIFT        (0x7)
#define CTRL_FORCE_MASK         (0x1FF)
#define CTRL_LOCK_VALUE_SHIFT   (0x8)
#define CTRL_LOCK_VALUE_MASK    (0x1FF)

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
static spinlock_t c2_state_lock;

#define CLUSTER_OFF_TARGET_RESIDENCY	3000

#define L2_OFF		(1 << 0)
#define L2_CCI_OFF	(1 << 1)
#define CMU_OFF		(1 << 2)
#if defined(CONFIG_ARM_EXYNOS5433_BUS_DEVFREQ)
#define MIF_LIMIT	(0)
#endif
#endif

static int exynos_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			      int index);
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static int exynos_enter_c2(struct cpuidle_device *dev,
				 struct cpuidle_driver *drv,
				 int index);
#endif
static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index);

struct check_reg_lpa {
	void __iomem	*check_reg;
	unsigned int	check_bit;
};

/*
 * List of check power domain list for LPA mode
 * These register are have to power off to enter LPA mode
 */

static struct check_reg_lpa exynos5_power_domain[] = {
	{.check_reg = EXYNOS5433_GSCL_STATUS,	.check_bit = 0x7},	/* 0x4004 */
	{.check_reg = EXYNOS5433_CAM0_STATUS,	.check_bit = 0x7},	/* 0x4024 */
	{.check_reg = EXYNOS5433_CAM1_STATUS,	.check_bit = 0x7},	/* 0x40A4 */
	{.check_reg = EXYNOS5433_MFC_STATUS,	.check_bit = 0x7},	/* 0x4184 */
	{.check_reg = EXYNOS5433_HEVC_STATUS,	.check_bit = 0x7},	/* 0x41C4 */
	{.check_reg = EXYNOS5433_G3D_STATUS,	.check_bit = 0x7},	/* 0x4064 */
	{.check_reg = EXYNOS5433_DISP_STATUS,	.check_bit = 0x7},	/* 0x4084 */
};

static struct check_reg_lpa exynos5_dstop_power_domain[] = {
	{.check_reg = EXYNOS5433_AUD_STATUS,	.check_bit = 0xF},	/* 0x40C4 */
};

static struct check_reg_lpa exynos5_none_domain[] = {
	{.check_reg = EXYNOS5433_ISP_STATUS,	.check_bit = 0xF},	/* 0x4144 */
};

static struct check_reg_lpa exynos5_lpc_power_domain[] = {
	{.check_reg = EXYNOS5433_G2D_STATUS,	.check_bit = 0xF},	/* 0x4124 */
	{.check_reg = EXYNOS5433_CAM0_STATUS,	.check_bit = 0xF},	/* 0x4024 */
	{.check_reg = EXYNOS5433_CAM1_STATUS,	.check_bit = 0xF},	/* 0x40A4 */
	{.check_reg = EXYNOS5433_GSCL_STATUS,	.check_bit = 0xF},	/* 0x4004 */
};

/*
 * List of check clock gating list for LPA mode
 * If clock of list is not gated, system can not enter LPA mode.
 */

static struct check_reg_lpa exynos5_clock_gating[] = {
	{.check_reg = EXYNOS5430_ENABLE_IP_PERIC0,	.check_bit = 0xF00FFF},
};

#ifdef CONFIG_EXYNOS_MIPI_LLI
extern int mipi_lli_get_link_status(void);
#endif
#ifdef CONFIG_SAMSUNG_USBPHY
extern int samsung_usbphy_check_op(void);
extern void samsung_usb_lpa_resume(void);
#endif

#if defined(CONFIG_GPS_BCMxxxxx)
extern int check_gps_op(void);
#endif

#if defined(CONFIG_MMC_DW)
extern int dw_mci_exynos_request_status(void);
#endif

#ifdef CONFIG_PCI_EXYNOS
extern int check_wifi_op(void);
extern void exynos_pci_lpa_resume(void);
#endif
static int exynos_check_reg_status(struct check_reg_lpa *reg_list,
				    unsigned int list_cnt)
{
	unsigned int i;
	unsigned int tmp;

	for (i = 0; i < list_cnt; i++) {
		tmp = __raw_readl(reg_list[i].check_reg);
		if (tmp & reg_list[i].check_bit)
			return -EBUSY;
	}

	return 0;
}

#define UART_CLK_MASK	0x7
#define UART_CLK_SHIFT	12

static inline bool dbg_uart_clk_is_on(void)
{
	u32 val = (__raw_readl(EXYNOS5430_ENABLE_IP_PERIC0) >> UART_CLK_SHIFT)
		& UART_CLK_MASK;
	return (val >> CONFIG_S3C_LOWLEVEL_UART_PORT) & 0x1;
}

#define UFSTAT_REG_OFFSET	0x18
#define UFSTAT_TXMASK	0xFF
#define UFSTAT_TXSHIFT	16

static inline int dbg_uart_tx_fifo_cnt(void)
{
	u32 val = __raw_readl(S5P_VA_UART(CONFIG_S3C_LOWLEVEL_UART_PORT) +
			UFSTAT_REG_OFFSET);

	return (val >> UFSTAT_TXSHIFT) & UFSTAT_TXMASK;
}

static int exynos_uart_fifo_check(void)
{
	if (dbg_uart_clk_is_on())
		return dbg_uart_tx_fifo_cnt();

	return 0;
}

#if defined(CONFIG_PWM_SAMSUNG)
extern int pwm_check_enable_cnt(void);
#else
static inline int pwm_check_enable_cnt(void)
{
	return 0;
}
#endif

#if defined(CONFIG_SEC_FACTORY)
static int is_jig_attached(void)
{
	return 1;
}
#else
static bool muic_is_attached = false;
static int is_jig_attached(void)
{
	return muic_is_attached;
}
#endif

static int exynos_check_lpc(void)
{
	if (is_jig_attached())
		return 1;
	/* If MIF is  max clock, don't enter lpc mode */
#if defined(CONFIG_ARM_EXYNOS5433_BUS_DEVFREQ)
	if (exynos5_devfreq_get_mif_level() <= MIF_LIMIT)
		return 1;
#endif

	/* Check clock gating */
	if (exynos_check_reg_status(exynos5_clock_gating,
				ARRAY_SIZE(exynos5_clock_gating)))
		return 1;

#if defined(CONFIG_MMC_DW)
	if (dw_mci_exynos_request_status())
		return 1;
#endif

#ifdef CONFIG_EXYNOS_MIPI_LLI
	if (mipi_lli_get_link_status())
		return 1;
#endif

#ifdef CONFIG_SAMSUNG_USBPHY
	if (samsung_usbphy_check_op())
		return 1;
#endif

#ifdef CONFIG_PCI_EXYNOS
	if (check_wifi_op())
		return EXYNOS_CHECK_DIDLE;
#endif

#if 0
#if defined(CONFIG_GPS_BCMxxxxx)
	if (check_gps_op())
		return 1;
#endif
#endif

#if defined(CONFIG_VIDEO_EXYNOS_FIMG2D)
	if (exynos_check_reg_status(exynos5_lpc_power_domain,
				ARRAY_SIZE(exynos5_lpc_power_domain)))
		return 1;
#endif
	if (pwm_check_enable_cnt())
		return 1;

#if defined(CONFIG_SND_SAMSUNG_AUDSS)
	if (exynos_check_aud_pwr() > AUD_PWR_LPC)
		return 1;
#endif

	return 0;
}


static int exynos_check_enter_mode(void)
{
	/* Check power domain */
	if (exynos_check_reg_status(exynos5_none_domain,
			    ARRAY_SIZE(exynos5_none_domain)))
		return EXYNOS_CHECK_C2;

	/* Check power domain */
	if (exynos_check_reg_status(exynos5_power_domain,
			    ARRAY_SIZE(exynos5_power_domain)))
		return EXYNOS_CHECK_DIDLE;

	/* Check clock gating */
	if (exynos_check_reg_status(exynos5_clock_gating,
			    ARRAY_SIZE(exynos5_clock_gating)))
		return EXYNOS_CHECK_DIDLE;

#if defined(CONFIG_MMC_DW)
	if (dw_mci_exynos_request_status())
		return EXYNOS_CHECK_DIDLE;
#endif

#ifdef CONFIG_EXYNOS_MIPI_LLI
	if (mipi_lli_get_link_status())
		return EXYNOS_CHECK_DIDLE;
#endif

#ifdef CONFIG_SAMSUNG_USBPHY
	if (samsung_usbphy_check_op())
		return EXYNOS_CHECK_DIDLE;
#endif

#ifdef CONFIG_PCI_EXYNOS
	if (check_wifi_op())
		return EXYNOS_CHECK_DIDLE;
#endif

	/* Check power domain for DSTOP */
	if (exynos_check_reg_status(exynos5_dstop_power_domain,
			    ARRAY_SIZE(exynos5_dstop_power_domain)))
		return EXYNOS_CHECK_LPA;

	return EXYNOS_CHECK_DSTOP;
}

static struct cpuidle_state exynos5_cpuidle_set[] __initdata = {
	[0] = {
		.enter			= exynos_enter_idle,
		.exit_latency		= 1,
		.target_residency	= 500,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "ARM clock gating(WFI)",
	},
	[1] = {
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		.enter                  = exynos_enter_c2,
		.exit_latency           = 30,
		.target_residency       = 1000,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C2",
		.desc                   = "ARM power down",
	},
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	[2] = {
		.enter                  = exynos_enter_c2,
		.exit_latency           = 300,
		.target_residency       = CLUSTER_OFF_TARGET_RESIDENCY,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C2-1",
		.desc                   = "Cluster power down",
	},
	[3] = {
#else
	[2] = {
#endif
#endif
		.enter                  = exynos_enter_lowpower,
		.exit_latency           = 300,
		.target_residency       = 5000,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C3",
		.desc                   = "System power down",
	},
};

static DEFINE_PER_CPU(struct cpuidle_device, exynos_cpuidle_device);

static struct cpuidle_driver exynos_idle_driver = {
	.name                   = "exynos_idle",
	.owner                  = THIS_MODULE,
};

/* Ext-GIC nIRQ/nFIQ is the only wakeup source in AFTR */
static void exynos_set_wakeupmask(enum sys_powerdown mode)
{
	switch (mode) {
	case SYS_AFTR:
		__raw_writel(0x40001000, EXYNOS5433_WAKEUP_MASK);
		__raw_writel(0xFFFF0000, EXYNOS5433_WAKEUP_MASK1);
		__raw_writel(0xFFFF0000, EXYNOS5433_WAKEUP_MASK2);
		break;
	case SYS_LPA:
		__raw_writel(0x40001000, EXYNOS5433_WAKEUP_MASK);
		__raw_writel(0xFFFF0000, EXYNOS5433_WAKEUP_MASK1);
		__raw_writel(0xFFFF0000, EXYNOS5433_WAKEUP_MASK2);
		break;
	case SYS_DSTOP:
		__raw_writel(0x40003000, EXYNOS5433_WAKEUP_MASK);
		__raw_writel(0xFFFF0000, EXYNOS5433_WAKEUP_MASK1);
		__raw_writel(0xFFFF0000, EXYNOS5433_WAKEUP_MASK2);
		break;
	default:
		break;
	}
}

static void exynos_clear_wakeupmask(void)
{
	__raw_writel(0x0, EXYNOS5433_WAKEUP_MASK);
	__raw_writel(0x0, EXYNOS5433_WAKEUP_MASK1);
	__raw_writel(0x0, EXYNOS5433_WAKEUP_MASK2);
}

static int idle_finisher(unsigned long flags)
{
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, 0);

	return 1;
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
struct cpumask cpu_c2_state;

static int c2_finisher(unsigned long flags)
{
	unsigned int kind = OP_TYPE_CORE;
	unsigned int param = 0;

	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (flags & L2_CCI_OFF) {
		exynos_cpu_sequencer_ctrl(true);
		kind = OP_TYPE_CLUSTER;
	}
#endif

	if (flags & CMU_OFF)
		param = 1;

	exynos_smc(SMC_CMD_SHUTDOWN, kind, SMC_POWERSTATE_IDLE, param);

	/*
	 * Secure monitor disables the SMP bit and takes the CPU out of the
	 * coherency domain.
	 */
	local_flush_tlb_all();

	return 1;
}
#endif

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
void exynos_idle_clock_down(bool on, enum cpu_type cluster)
{
	void __iomem *reg_pwr_ctrl;
	unsigned int tmp;

	if (cluster == KFC)
		reg_pwr_ctrl = EXYNOS5430_KFC_PWR_CTRL;
	else
		reg_pwr_ctrl = EXYNOS5430_EGL_PWR_CTRL;

	/* Enable or Disable L2QACTIVE   */
	tmp = on ? EXYNOS5430_USE_L2QACTIVE : 0;
	__raw_writel(tmp, reg_pwr_ctrl);

	pr_debug("%s idle clock down is %s\n", (cluster == KFC ? "KFC" : "EAGLE"),
					(on ? "enabled" : "disabled"));
}
#endif

static int exynos_enter_core0_aftr(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	unsigned int ret = 0;
	unsigned int cpuid = smp_processor_id();

	exynos_set_wakeupmask(SYS_AFTR);

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_AFTR);

	/* Setting Central Sequence Register for power down mode */
	exynos_central_sequencer_ctrl(true);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	set_boot_flag(cpuid, C2_STATE);

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(false, ARM);
	exynos_idle_clock_down(false, KFC);
#endif

	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret)
		exynos_central_sequencer_ctrl(false);

	cpu_pm_exit();

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);
#endif

	clear_boot_flag(cpuid, C2_STATE);

	/* Clear wakeup state register */
	exynos_clear_wakeupmask();

	return index;
}

static struct sleep_save exynos5_lpa_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ISP_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_ISP_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_AUD_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_AUD_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_AUD_PLL_CON2),
	SAVE_ITEM(EXYNOS5430_MEM0_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_MEM0_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_MEM1_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_MEM1_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_BUS_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_BUS_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_MFC_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_MFC_PLL_CON1),

	SAVE_ITEM(EXYNOS5430_SRC_SEL_EGL0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_EGL1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_EGL2),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_KFC0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_KFC1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_KFC2),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP2),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP3),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_MSCL),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_CAM1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_DISP),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_FSYS0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_FSYS1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_PERIC0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_PERIC1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF2),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF3),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF4),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF5),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_BUS2),

	SAVE_ITEM(EXYNOS5430_DIV_EGL0),
	SAVE_ITEM(EXYNOS5430_DIV_EGL1),
	SAVE_ITEM(EXYNOS5430_DIV_KFC0),
	SAVE_ITEM(EXYNOS5430_DIV_KFC1),
	SAVE_ITEM(EXYNOS5430_DIV_TOP0),
	SAVE_ITEM(EXYNOS5430_DIV_TOP1),
	SAVE_ITEM(EXYNOS5430_DIV_TOP2),
	SAVE_ITEM(EXYNOS5430_DIV_TOP3),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_MSCL),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_CAM10),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_CAM11),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_FSYS0),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_FSYS1),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_FSYS2),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_PERIC0),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_PERIC1),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_PERIC2),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_PERIC3),
	SAVE_ITEM(EXYNOS5430_DIV_MIF1),
	SAVE_ITEM(EXYNOS5430_DIV_MIF2),
	SAVE_ITEM(EXYNOS5430_DIV_MIF3),
	SAVE_ITEM(EXYNOS5430_DIV_MIF4),
	SAVE_ITEM(EXYNOS5430_DIV_MIF5),
	SAVE_ITEM(EXYNOS5430_DIV_BUS1),
	SAVE_ITEM(EXYNOS5430_DIV_BUS2),

	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP0),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP2),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP3),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP4),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP_MSCL),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP_CAM1),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP_DISP),

	SAVE_ITEM(EXYNOS5430_ENABLE_IP_EGL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_KFC1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_TOP),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_FSYS0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_PERIC0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MIF1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CPIF0),
};

static struct sleep_save exynos5_set_clksrc[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_FSYS0,	.val = 0x00007dfb, },
	{ .reg = EXYNOS5430_ENABLE_IP_PERIC0,	.val = 0xffffffff, },
	{ .reg = EXYNOS5430_SRC_SEL_TOP_PERIC1,	.val = 0x00000011, },
	{ .reg = EXYNOS5430_ENABLE_IP_EGL1,	.val = 0x00000fff, },
	{ .reg = EXYNOS5430_ENABLE_IP_KFC1,	.val = 0x00000fff, },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF1,	.val = 0x01ffffff, },
	{ .reg = EXYNOS5430_ENABLE_IP_CPIF0,	.val = 0x000FF000, },
};

static int exynos_enter_core0_lpa(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int lp_mode, int index, int enter_mode)
{
	int ret = 0;
	unsigned int cpuid = smp_processor_id();

	/* Set value of power down register for aftr mode */
	if (enter_mode == EXYNOS_CHECK_LPA) {
		exynos_set_wakeupmask(SYS_LPA);
		exynos_sys_powerdown_conf(SYS_LPA);
	} else {
		exynos_set_wakeupmask(SYS_DSTOP);
		exynos_sys_powerdown_conf(SYS_DSTOP);
	}

	/* Setting Central Sequence Register for power down mode */
	exynos_central_sequencer_ctrl(true);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	set_boot_flag(cpuid, C2_STATE);

	do {
		/* Waiting for flushing UART fifo */
	} while (exynos_uart_fifo_check());

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(false, ARM);
	exynos_idle_clock_down(false, KFC);
#endif

	if (lp_mode == SYS_ALPA)
		__raw_writel(0x1, EXYNOS5433_PMU_SYNC_CTRL);

	/* Backup clock SFR which is reset after wakeup from LPA mode */
	s3c_pm_do_save(exynos5_lpa_clk_save, ARRAY_SIZE(exynos5_lpa_clk_save));

	/* Set clock SFR before enter LPA mode */
	s3c_pm_do_restore_core(exynos5_set_clksrc,
			       ARRAY_SIZE(exynos5_set_clksrc));

	cpu_pm_enter();
	exynos_lpa_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		exynos_central_sequencer_ctrl(false);

		goto early_wakeup;
	}

	/* For release retention */
	__raw_writel((1 << 28), EXYNOS_PAD_RET_DRAM_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_JTAG_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_MMC2_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_TOP_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_MMC0_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_MMC1_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_MIF_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_USBXTI_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_BOOTLDO_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_UFS_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_FSYSGENIO_OPTION);

early_wakeup:
	exynos_lpa_exit();
	cpu_pm_exit();

#ifdef CONFIG_SAMSUNG_USBPHY
	samsung_usb_lpa_resume();
#endif

	/* Restore clock SFR with which is saved before enter LPA mode */
	s3c_pm_do_restore_core(exynos5_lpa_clk_save,
			       ARRAY_SIZE(exynos5_lpa_clk_save));

	if (lp_mode == SYS_ALPA)
		__raw_writel(0x0, EXYNOS5433_PMU_SYNC_CTRL);

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);
#endif

#if defined(CONFIG_EXYNOS_MIPI_LLI)
	/* LLI_INTR_ENABLE is not retention register
	   So, when LPA is exiting. it has to be recovered. */
	mipi_lli_intr_enable();
#endif

#ifdef CONFIG_PCI_EXYNOS
	exynos_pci_lpa_resume();
#endif

	clear_boot_flag(cpuid, C2_STATE);

	/* Clear wakeup state register */
	exynos_clear_wakeupmask();

	return index;
}

static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	int new_index = index;
	int enter_mode;

	/* This mode only can be entered when other core's are offline */
	if (num_online_cpus() > 1)
		goto idle;

	enter_mode = exynos_check_enter_mode();

	if (enter_mode == EXYNOS_CHECK_C2)
		goto idle;
	else if (enter_mode == EXYNOS_CHECK_DIDLE)
		return exynos_enter_core0_aftr(dev, drv, new_index);
#ifdef CONFIG_SND_SAMSUNG_AUDSS
	else if (exynos_check_aud_pwr() == AUD_PWR_ALPA)
		return exynos_enter_core0_aftr(dev, drv, new_index);
	else
#endif
		return exynos_enter_core0_lpa(dev, drv, SYS_LPA, new_index, enter_mode);
idle:
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
		return exynos_enter_c2(dev, drv, 2);
#else
		return exynos_enter_c2(dev, drv, 1);
#endif
#else
		return exynos_enter_idle(dev, drv, 0);
#endif
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
static bool disabled_cluster_power_down = false;

static void exynos_disable_c3_idle(bool disable)
{
	disabled_cluster_power_down = disable;
}
#endif

#ifdef CONFIG_EXYNOS_DECON_DISPLAY
extern unsigned int *ref_power_status;
enum s3c_fb_pm_status {
	POWER_ON = 0,
	POWER_DOWN = 1,
	POWER_HIBER_DOWN = 2,
};
#endif

static __maybe_unused int check_matched_state(int cpu_id, struct cpumask *matched_state, const struct cpumask *cpu_group)
{
	ktime_t now = ktime_get();
	struct clock_event_device *dev;
	int cpu;

	if (disabled_cluster_power_down)
		return 0;

	for_each_cpu_and(cpu, cpu_possible_mask, cpu_group) {
		if (cpu_id == cpu)
			continue;

		dev = per_cpu(tick_cpu_device, cpu).evtdev;
		if (!cpumask_test_cpu(cpu, matched_state))
			return 0;

		if (ktime_to_us(ktime_sub(dev->next_event, now)) < CLUSTER_OFF_TARGET_RESIDENCY)
			return 0;
	}
	return 1;
}

static unsigned long exynos_cluster_power_down(bool pwr_down, unsigned int cpuid,
								bool early_wakeup)
{
	unsigned long flags = 0;
	spin_lock(&c2_state_lock);

	if (pwr_down) {
		cpumask_set_cpu(cpuid, &cpu_c2_state);
		if ((cpuid & 0x4) && check_matched_state(cpuid, &cpu_c2_state, cpu_coregroup_mask(cpuid))) {
			flags |= L2_CCI_OFF;
			cpuidle_profile_start(CPUIDLE_PROFILE_CPD, cpuid);
		}
		if (check_matched_state(cpuid, &cpu_c2_state, cpu_possible_mask) && !exynos_check_lpc()
#ifdef CONFIG_EXYNOS_DECON_DISPLAY
				&& ref_power_status != NULL && (*ref_power_status) == POWER_HIBER_DOWN
#endif
			) {
				do {
				/* Waiting for flushing UART fifo */
				} while (exynos_uart_fifo_check());
				flags |= CMU_OFF;
		}
	} else {
		cpumask_clear_cpu(cpuid, &cpu_c2_state);
		cpuidle_profile_finish(CPUIDLE_PROFILE_CPD, cpuid, early_wakeup);
	}

	spin_unlock(&c2_state_lock);

	return flags;
}
#endif

extern void exynos5433_cpu_up(unsigned int);
extern void exynos5433_cpu_down(unsigned int);

static int exynos_enter_c2(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	int ret = 0;
	unsigned int cpuid = smp_processor_id();
	unsigned long flags = 0;

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	set_boot_flag(cpuid, C2_STATE);
	cpu_pm_enter();

	cpuidle_profile_start(CPUIDLE_PROFILE_C2, cpuid);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (index == 2)
		flags = exynos_cluster_power_down(true, cpuid, 0);
#endif

	exynos5433_cpu_down(cpuid);

	ret = cpu_suspend(flags, c2_finisher);
	if (ret)
		exynos5433_cpu_up(cpuid);

	/* Disable cpu sequencer as soon as wakeup from local power gating */
	if (flags & L2_CCI_OFF)
		exynos_cpu_sequencer_ctrl(false);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (index == 2) {
		exynos_cpu_sequencer_ctrl(false);
		flags = exynos_cluster_power_down(false, cpuid, (bool)ret);
	}
#endif

	cpuidle_profile_finish(CPUIDLE_PROFILE_C2, cpuid, (bool)ret);

	cpu_pm_exit();
	clear_boot_flag(cpuid, C2_STATE);

	return index;
}
#endif

static int exynos_enter_idle(struct cpuidle_device *dev,
                                struct cpuidle_driver *drv,
                                int index)
{
	unsigned int cpuid = smp_processor_id();

	cpuidle_profile_start(CPUIDLE_PROFILE_C1, cpuid);
	cpu_do_idle();
	cpuidle_profile_finish(CPUIDLE_PROFILE_C1, cpuid, 0);

	return index;
}

static int exynos_cpuidle_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		/* To enter sleep mode quickly, disable idle clock down  */
		exynos_idle_clock_down(false, ARM);
		exynos_idle_clock_down(false, KFC);

		cpu_idle_poll_ctrl(true);
		pr_debug("PM_SUSPEND_PREPARE for CPUIDLE\n");
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		/* Enable idle clock down after wakeup from skeep mode */
		exynos_idle_clock_down(true, ARM);
		exynos_idle_clock_down(true, KFC);

		cpu_idle_poll_ctrl(false);
		pr_debug("PM_POST_SUSPEND for CPUIDLE\n");
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block exynos_cpuidle_notifier = {
	.notifier_call = exynos_cpuidle_notifier_event,
};

static int exynos_cpuidle_reboot_notifier(struct notifier_block *this,
				unsigned long event, void *_cmd)
{
	switch (event) {
	case SYSTEM_POWER_OFF:
	case SYS_RESTART:
		cpu_idle_poll_ctrl(true);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpuidle_reboot_nb = {
	.notifier_call = exynos_cpuidle_reboot_notifier,
};

static int __init exynos_init_cpuidle(void)
{
	int i, cpu_id, ret;
	struct cpuidle_device *device;

	exynos_idle_driver.state_count = ARRAY_SIZE(exynos5_cpuidle_set);

	for (i = 0; i < exynos_idle_driver.state_count; i++) {
		memcpy(&exynos_idle_driver.states[i],
				&exynos5_cpuidle_set[i],
				sizeof(struct cpuidle_state));
	}

	exynos_idle_driver.safe_state_index = 0;

	ret = cpuidle_register_driver(&exynos_idle_driver);
	if (ret) {
		printk(KERN_ERR "CPUidle register device failed\n,");
		return ret;
	}

	for_each_cpu(cpu_id, cpu_online_mask) {
		device = &per_cpu(exynos_cpuidle_device, cpu_id);
		device->cpu = cpu_id;

		device->state_count = exynos_idle_driver.state_count;
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
		cpumask_clear(&cpu_c2_state);
#endif

		/* Eagle will not change idle time correlation factor */
		if (cpu_id & 0x4)
			device->skip_idle_correlation = true;
		else
			device->skip_idle_correlation = false;

		if (cpuidle_register_device(device)) {
			printk(KERN_ERR "CPUidle register device failed\n,");
			return -EIO;
		}
	}

#if defined(CONFIG_EXYNOS_CLUSTER_POWER_DOWN) && defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
	disable_c3_idle = exynos_disable_c3_idle;
#endif

	register_pm_notifier(&exynos_cpuidle_notifier);
	register_reboot_notifier(&exynos_cpuidle_reboot_nb);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	spin_lock_init(&c2_state_lock);
#endif

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);
#endif

	return 0;
}
device_initcall(exynos_init_cpuidle);

#if defined(CONFIG_MUIC_NOTIFIER) && !defined(CONFIG_SEC_FACTORY)
struct notifier_block cpuidle_muic_nb;

static int exynos_cpuidle_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			muic_is_attached = false;
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			muic_is_attached = true;
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	default:
		break;
	}

	pr_info("%s - action=%ld, attached_dev=%d\n", __func__, action, attached_dev);

	return NOTIFY_DONE;
}

static int __init exynos_muic_notifier_init(void)
{
	return muic_notifier_register(&cpuidle_muic_nb,
			exynos_cpuidle_muic_notifier, MUIC_NOTIFY_DEV_CPUIDLE);
}
late_initcall(exynos_muic_notifier_init);

#endif
