/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Management support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>

#include <asm/cacheflush.h>
#include <asm/smp_scu.h>
#include <asm/cputype.h>
#include <asm/firmware.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/pll.h>
#include <plat/regs-srom.h>

#include <mach/regs-irq.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pm-core.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/pmu_cal_sys.h>

#define REG_INFORM0			(S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1			(S5P_VA_SYSRAM_NS + 0xC)

#define EXYNOS_WAKEUP_STAT_EINT		(1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM	(1 << 1)

#define SLEEP_MASK(r, m, v) \
	{ .reg = (r), .mask = (m), .val = (v) }

struct sleep_mask_reg {
	void __iomem *reg;
	unsigned long mask;
	unsigned long val;
};

static struct sleep_mask_reg enable_clk_regs[] = {
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOPC0,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOPC1,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOPC2,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOPC3,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOPC4,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOPC5,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOPC0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOPC1,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOPC2,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOPC0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOPC1,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP00,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP03,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP04,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP05,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP06,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP07,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP0_DISP,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP0_CAM10,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP0_CAM11,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP0_PERIC0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP0_PERIC1,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP0_PERIC2,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP0_PERIC3,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOP02,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOP03,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOP04,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOP05,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOP06,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOP07,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP0_CAM10,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP0_CAM11,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP0_PERIC0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP0_PERIC1,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP0_PERIC2,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP0_PERIC3,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP10,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP13,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP1_FSYS0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP1_FSYS1,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_TOP1_FSYS11,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOP12,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_TOP13,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP1_FSYS0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP1_FSYS1,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_TOP1_FSYS11,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_SEL_TOP00,		0x00000001, 0x00000000),
	SLEEP_MASK(EXYNOS7420_MUX_SEL_TOP0_DISP,	0x00007000, 0x00003000),
	SLEEP_MASK(EXYNOS7420_MUX_SEL_TOP0_PERIC0,	0x00300370, 0x00100130),

	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_MIF0,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_MIF1,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_MIF2,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_MIF3,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_CCORE0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_CCORE1,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_IMEM,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_IMEM0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_IMEM1,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_PCLK_IMEM,		0xffffffff, 0xffffffff),

	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_PERIC0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_PERIC0,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_PERIC11,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_SCLK_PERIC10,	0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_BUS0,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_MUX_ENABLE_BUS1,		0xffffffff, 0xffffffff),
	SLEEP_MASK(EXYNOS7420_ENABLE_ACLK_BUS1,		0xffffffff, 0xffffffff),

	SLEEP_MASK(EXYNOS7420_ATLAS_PWR_CTRL,		0x00034000, 0x00000000),
	SLEEP_MASK(EXYNOS7420_ATLAS_PWR_CTRL2,		0x03000000, 0x00000000),
	SLEEP_MASK(EXYNOS7420_APOLLO_PWR_CTRL,		0x00034000, 0x00000000),
	SLEEP_MASK(EXYNOS7420_APOLLO_PWR_CTRL2,		0x03000000, 0x00000000),
};

static struct sleep_mask_reg enable_pmu_regs[] = {
	SLEEP_MASK(EXYNOS7420_PMU_SYNC_CTRL,		0xffffffff, 0x00000000),
	SLEEP_MASK(EXYNOS7420_CENTRAL_SEQ_MIF_OPTION,	0xffffffff, 0x00000000),
	SLEEP_MASK(EXYNOS7420_WAKEUP_MASK_MIF,		0xffffffff, 0x00000000),
	SLEEP_MASK(EXYNOS7420_ATLAS_NONCPU_OPTION,	0xffffffff, 0x00000011),
	SLEEP_MASK(EXYNOS7420_APOLLO_NONCPU_OPTION,	0xffffffff, 0x00000011),
	SLEEP_MASK(EXYNOS7420_MEMORY_TOP_OPTION,	0xffffffff, 0x00000001),
	SLEEP_MASK(EXYNOS7420_MEMORY_TOP_ALV_OPTION,	0xffffffff, 0x00000011),
	SLEEP_MASK(EXYNOS7420_RESET_CMU_TOP_OPTION,	0xffffffff, 0x00000000),
	SLEEP_MASK(EXYNOS7420_ATALS_OPTION,		0xffffffff, 0x80001101),
	SLEEP_MASK(EXYNOS7420_APOLLO_OPTION,		0xffffffff, 0x80001101),
	SLEEP_MASK(EXYNOS7420_BUS0_OPTION,		0xffffffff, 0x00001101),
	SLEEP_MASK(EXYNOS7420_FSYS0_OPTION,		0xffffffff, 0x00001101),
	SLEEP_MASK(EXYNOS7420_FSYS1_OPTION,		0xffffffff, 0x00001101),
	SLEEP_MASK(EXYNOS7420_G3D_OPTION,		0xffffffff, 0x00000181),
	SLEEP_MASK(EXYNOS7420_AUD_OPTION,		0xffffffff, 0x00000101),
	SLEEP_MASK(EXYNOS7420_SLEEP_RESET_OPTION,	0xffffffff, 0x00100000),
	SLEEP_MASK(EXYNOS7420_TOP_PWR_OPTION,		0xffffffff, 0x00000001),
	SLEEP_MASK(EXYNOS7420_LOGIC_RESET_OPTION,	0xffffffff, 0x00000000),
	SLEEP_MASK(EXYNOS7420_TOP_RETENTION_OPTION,	0xffffffff, 0x00000000),
};

static void exynos_sleep_mask(struct sleep_mask_reg *ptr, unsigned int cnt)
{
	unsigned long tmp;
	for (; cnt > 0; ptr++, cnt--) {
		tmp = __raw_readl(ptr->reg);
		__raw_writel((tmp & ~ptr->mask) | ptr->val, ptr->reg);
	}
}

static void exynos_show_wakeup_reason(void)
{
	unsigned long wakeup_stat;

	wakeup_stat = __raw_readl(EXYNOS7420_WAKEUP_STAT);

	if (wakeup_stat & EXYNOS_WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else
		pr_info("Resume caused by wakeup_stat=0x%08lx\n",
			wakeup_stat);
}

static void exynos_pm_wakeup_mask(void)
{
	unsigned long eintmask, intmask;

	if (of_have_populated_dt())
		eintmask = exynos_get_eint_wake_mask();
	else
		eintmask = s3c_irqwake_eintmask;

	intmask = s3c_irqwake_intmask & ~WAKEUP_MASK_EVENT_MON_SW;

	S3C_PMDBG("sleep: irq wakeup mask: eint %08lx, int %08lx\n",
			eintmask, intmask);

	__raw_writel(eintmask, EXYNOS7420_EINT_WAKEUP_MASK);
	__raw_writel(intmask, EXYNOS7420_WAKEUP_MASK);
	__raw_writel(0xFFFF0000, EXYNOS7420_WAKEUP_MASK2);
	__raw_writel(0xFFFF0000, EXYNOS7420_WAKEUP_MASK3);
}

static int exynos_pm_suspend(void)
{
	exynos_pm_wakeup_mask();
	exynos_sys_powerdown_conf(SYS_SLEEP);
	exynos_central_sequencer_ctrl(true);

	if (!(__raw_readl(EXYNOS_PMU_DEBUG) & CLKOUT_DISABLE))
		__raw_writel(0x1, EXYNOS5_XXTI_SYS_PWR_REG);

	return 0;
}

static void exynos_cpu_prepare(void)
{
	/* INFORM0: set resume address, INFORM1: 0x00000BAD */
	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM1);
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_INFORM0);

	exynos_sleep_mask(enable_clk_regs, ARRAY_SIZE(enable_clk_regs));
	exynos_sleep_mask(enable_pmu_regs, ARRAY_SIZE(enable_pmu_regs));
}

static int exynos_cpu_suspend(unsigned long arg)
{
	flush_cache_all();

	if (call_firmware_op(do_idle))
		cpu_do_idle();

	pr_info("sleep resumed to originator?");
	return 1; /* abort suspend */
}

static void exynos_pm_resume(void)
{
	unsigned long tmp;

	/*
	 * If PMU failed while entering sleep mode, WFI will be
	 * ignored by PMU and then exiting cpu_do_idle().
	 * S5P_CENTRAL_LOWPWR_CFG bit will not be set automatically
	 * in this situation.
	 */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	if (!(tmp & EXYNOS_CENTRAL_LOWPWR_CFG)) {
		exynos_central_sequencer_ctrl(false);
		pr_info("early wakeup from sleep");
		/* No need to perform below restore code */
		goto early_wakeup;
	}

	/* restore atlas LPI mask */
	exynos_atlas_asyncbridge_ignore_lpi();

	/* For release retention */
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_AUD_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_MMC2_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_TOP_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_MMC0_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_MMC1_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_MIF_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_UFS_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_FSYSGENIO_OPTION);

early_wakeup:
	exynos_show_wakeup_reason();
	return;
}

static int exynos_pm_add(struct device *dev, struct subsys_interface *sif)
{
	pm_cpu_prep	= exynos_cpu_prepare;
	pm_cpu_sleep	= exynos_cpu_suspend;
	return 0;
}

static struct subsys_interface exynos_pm_interface = {
	.name		= "exynos_pm",
	.subsys		= &exynos_subsys,
	.add_dev	= exynos_pm_add,
};

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_suspend,
	.resume		= exynos_pm_resume,
};

static __init int exynos_pm_drvinit(void)
{
	s3c_pm_init();
	return subsys_interface_register(&exynos_pm_interface);
}
arch_initcall(exynos_pm_drvinit);

static __init int exynos_pm_syscore_init(void)
{
	register_syscore_ops(&exynos_pm_syscore_ops);
	return 0;
}
arch_initcall(exynos_pm_syscore_init);
