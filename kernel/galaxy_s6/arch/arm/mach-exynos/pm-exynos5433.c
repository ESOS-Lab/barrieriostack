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
#include <mach/regs-clock-exynos5433.h>
#include <mach/regs-pmu.h>
#include <mach/pm-core.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/pmu_cal_sys.h>

#define REG_INFORM0			(S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1			(S5P_VA_SYSRAM_NS + 0xC)

#define EXYNOS_WAKEUP_STAT_EINT		(1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM	(1 << 1)

static struct sleep_save exynos5433_set_clk[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_FSYS0,		.val = 0x00007DF9, },
	{ .reg = EXYNOS5430_ENABLE_IP_PERIC0,		.val = 0xFFFFFEFF, },
	{ .reg = EXYNOS5430_SRC_SEL_TOP_PERIC1,		.val = 0x00000011, },
	{ .reg = EXYNOS5430_SRC_SEL_FSYS0,		.val = 0x00000000, },
	{ .reg = EXYNOS5430_SRC_SEL_BUS2,		.val = 0x00000000, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_FSYS,		.val = 0x00000000, },
	{ .reg = EXYNOS5430_ENABLE_IP_CPIF0,		.val = 0x000FF000, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,		.val = 0x67E8FFED, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_MIF3,		.val = 0x00000003, },
};

static struct sleep_save exynos_enable_xxti[] = {
	{ .reg = EXYNOS5_XXTI_SYS_PWR_REG,		.val = 0x1, },
};

#if 0
static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int reg_eintstart;
	long unsigned int ext_int_pend;
	unsigned long eint_wakeup_mask;
	bool found = 0;
	extern void __iomem *exynos_eint_base;

	eint_wakeup_mask = __raw_readl(EXYNOS5433_EINT_WAKEUP_MASK);

	for (reg_eintstart = 0; reg_eintstart <= 31; reg_eintstart += 8) {
		ext_int_pend =
			__raw_readl(EINT_PEND(exynos_eint_base,
					      IRQ_EINT(reg_eintstart)));

		for_each_set_bit(bit, &ext_int_pend, 8) {
			int irq = IRQ_EINT(reg_eintstart) + bit;
			struct irq_desc *desc = irq_to_desc(irq);

			if (eint_wakeup_mask & (1 << (reg_eintstart + bit)))
				continue;

			if (desc && desc->action && desc->action->name)
				pr_info("Resume caused by IRQ %d, %s\n", irq,
					desc->action->name);
			else
				pr_info("Resume caused by IRQ %d\n", irq);

			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}
#endif
static void exynos_show_wakeup_reason(void)
{
	unsigned long wakeup_stat;

	wakeup_stat = __raw_readl(EXYNOS5433_WAKEUP_STAT);

	if (wakeup_stat & EXYNOS_WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	//else if (wakeup_stat & EXYNOS_WAKEUP_STAT_EINT)
		//exynos_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat=0x%08lx\n",
			wakeup_stat);
}

#ifdef CONFIG_PCI_EXYNOS
extern int exynos_fsys_power_on(void);
extern int exynos_fsys_power_off(void);
int pcie_suspend_ok = 0;
#endif
static int exynos_cpu_suspend(unsigned long arg)
{
	flush_cache_all();

	if (call_firmware_op(do_idle))
		cpu_do_idle();
	pr_info("sleep resumed to originator?");

#ifdef CONFIG_PCI_EXYNOS
	/* If happen early wakeu, it is need to reset FSYS */
	exynos_fsys_power_off();
	mdelay(1);
	exynos_fsys_power_on();
	pr_info("not enter suspend fully\n");
#endif

	return 1; /* abort suspend */
}

static int exynos_pm_suspend(void)
{
	/* Setting Central Sequence Register for power down mode */
	exynos_central_sequencer_ctrl(true);

	return 0;
}

#ifdef CONFIG_PCI_EXYNOS
extern void exynos_pcie_suspend(void);
extern void exynos_pcie_resume(void);
#endif

static void exynos_pm_prepare(void)
{
#ifdef CONFIG_PCI_EXYNOS
	exynos_pcie_suspend();
	pcie_suspend_ok = 1;
#endif

	/* Set value of power down register for sleep mode */
	exynos_sys_powerdown_conf(SYS_SLEEP);

	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM1);
	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_INFORM0);

	if (!(__raw_readl(EXYNOS_PMU_DEBUG) & 0x1))
		s3c_pm_do_restore_core(exynos_enable_xxti,
				ARRAY_SIZE(exynos_enable_xxti));

	/* For Only exynos5433, BLK_FSYS is on */
	if (__raw_readl(EXYNOS5433_FSYS_STATUS) == 0x0) {
		__raw_writel(EXYNOS_PWR_EN, EXYNOS5433_FSYS_CONFIGURATION);
		do {
			if ((__raw_readl(EXYNOS5433_FSYS_STATUS) == EXYNOS_PWR_EN))
				break;
		} while (1);
	}

	/*
	 * Before enter central sequence mode,
	 * clock register have to set.
	 */
	s3c_pm_do_restore_core(exynos5433_set_clk, ARRAY_SIZE(exynos5433_set_clk));
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
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		/* No need to perform below restore code */
		goto early_wakeup;
	}

	/* Need to set again because LPI MASK is reset after wakeup */
	exynos_eagle_asyncbridge_ignore_lpi();

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
#ifdef CONFIG_PCI_EXYNOS
	if (pcie_suspend_ok) {
		pcie_suspend_ok = 0;
		exynos_pcie_resume();
	}
#endif
	exynos_show_wakeup_reason();

	return;
}

static int exynos_pm_add(struct device *dev, struct subsys_interface *sif)
{
	pm_cpu_prep = exynos_pm_prepare;
	pm_cpu_sleep = exynos_cpu_suspend;

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
