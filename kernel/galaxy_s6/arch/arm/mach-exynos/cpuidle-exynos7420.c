/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * CPUIDLE driver for EXYNOS7420
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
#ifdef CONFIG_EXYNOS_MIPI_LLI
#include <linux/mipi-lli.h>
#endif

#include <asm/suspend.h>
#include <asm/tlbflush.h>

#include <mach/cpuidle.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/cpufreq.h>
#include <mach/exynos-pm.h>
#include <mach/pmu_cal_sys.h>

#include <plat/pm.h>

#ifdef CONFIG_SND_SAMSUNG_AUDSS
#include <sound/exynos.h>
#endif

#define EXYNOS_CHECK_DIRECTGO	0xFCBA0D10
#define EXYNOS_CHECK_LPA	0xABAD0000
#define EXYNOS_CHECK_DSTOP	0xABAE0000

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
void exynos_idle_clock_down(bool on, enum cpu_type cluster) { }
#else
void exynos_idle_clock_down(bool on, enum cpu_type cluster) { }
#endif

static int exynos_enter_idle(struct cpuidle_device *dev,
                                struct cpuidle_driver *drv,
                                int index)
{
	cpu_do_idle();

	return index;
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
#define CPD_TARGET_RESIDENCY	3400
static spinlock_t cpd_state_lock;
static struct cpumask cpd_state_mask;

static bool disabled_cluster_power_down = false;

static void __maybe_unused exynos_disable_cluster_power_down(bool disable)
{
	disabled_cluster_power_down = disable;
}

static int exynos_check_cpd_state(unsigned int cpuid)
{
#if defined(CONFIG_SCHED_HMP)
	ktime_t now = ktime_get();
	struct clock_event_device *dev;
	int cpu;

	if (disabled_cluster_power_down)
		return 0;

	for_each_cpu_and(cpu, cpu_possible_mask, cpu_coregroup_mask(cpuid)) {
		if (cpuid == cpu)
			continue;

		dev = per_cpu(tick_cpu_device, cpu).evtdev;
		if (!cpumask_test_cpu(cpu, &cpd_state_mask))
			return 0;

		if (ktime_to_us(ktime_sub(dev->next_event, now)) < CPD_TARGET_RESIDENCY)
			return 0;
	}

	return 1;
#else
	return 0;
#endif
}

/*
 * exynos_update_cpd_state - Update cluster power down state for per cpu
 */
static unsigned long exynos_update_cpd_state(bool pwr_down, unsigned int cpuid,
								bool early_wakeup)
{
	unsigned long flags = 0;

	if (!(cpuid & 0x4))
		return 0;

	spin_lock(&cpd_state_lock);

	if (pwr_down) {
		cpumask_set_cpu(cpuid, &cpd_state_mask);
		if (exynos_check_cpd_state(cpuid)) {
			flags = L2_CCI_OFF;
			exynos_cpu_sequencer_ctrl(true);
		}
	} else {
		cpumask_clear_cpu(cpuid, &cpd_state_mask);
		exynos_cpu_sequencer_ctrl(false);
	}

	spin_unlock(&cpd_state_lock);

	return flags;
}
#endif

static int c2_finisher(unsigned long flags)
{
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (flags == L2_CCI_OFF)
		exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, flags);
	else
#endif
		exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);


	/*
	 * Secure monitor disables the SMP bit and takes the CPU out of the
	 * coherency domain.
	 */
	local_flush_tlb_all();

	return 1;
}

extern void exynos7420_cpu_up(unsigned int);
extern void exynos7420_cpu_down(unsigned int);

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

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (index == 2)
		flags |= exynos_update_cpd_state(true, cpuid, 0);
#endif

	exynos7420_cpu_down(cpuid);

	ret = cpu_suspend(flags, c2_finisher);
	if (ret)
		exynos7420_cpu_up(cpuid);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (index == 2)
		exynos_update_cpd_state(false, cpuid, (bool)ret);
#endif

	cpu_pm_exit();
	clear_boot_flag(cpuid, C2_STATE);

	return index;
}
#endif

/*
 * List of check power domain list for LPA mode
 * These register are have to power off to enter LPA mode
 */

static struct check_reg exynos_check_pd_list[] = {
	{.reg = EXYNOS7420_CAM0_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_CAM1_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_DISP_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_G2D_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_G3D_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_HEVC_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_ISP0_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_ISP1_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_MFC_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_MSCL_STATUS,		.check_bit = 0xF},
	{.reg = EXYNOS7420_VPP_STATUS,		.check_bit = 0xF},
};

static struct check_reg exynos_check_pd_list_dstop[] = {
	{.reg = EXYNOS7420_AUD_STATUS,		.check_bit = 0xF},
};

/*
 * List of check clock gating list for LPA mode
 * If clock of list is not gated, system can not enter LPA mode.
 */

static struct check_reg exynos_check_clk_list[] = {
};

#if defined(CONFIG_EXYNOS_MIPI_LLI)
extern int mipi_lli_get_link_status(void);
#endif
#if defined(CONFIG_SAMSUNG_USBPHY)
extern int samsung_usbphy_check_op(void);
#endif
#if defined(CONFIG_MMC_DW)
extern int dw_mci_exynos_request_status(void);
#endif

static int exynos_check_enter_mode(void)
{
	/* Check power domain */
	if (exynos_check_reg_status(exynos_check_pd_list,
			    ARRAY_SIZE(exynos_check_pd_list)))
		return EXYNOS_CHECK_DIDLE;

	/* Check clock gating */
	if (exynos_check_reg_status(exynos_check_clk_list,
			    ARRAY_SIZE(exynos_check_clk_list)))
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

	/* Check power domain for DSTOP */
	if (exynos_check_reg_status(exynos_check_pd_list_dstop,
			    ARRAY_SIZE(exynos_check_pd_list_dstop)))
		return EXYNOS_CHECK_LPA;

	return EXYNOS_CHECK_DSTOP;
}

static void exynos_set_wakeupmask(enum sys_powerdown mode)
{
	switch (mode) {
	case SYS_AFTR:
	case SYS_LPA:
		__raw_writel(0x40001000, EXYNOS7420_WAKEUP_MASK);
		__raw_writel(0xFFFF0000, EXYNOS7420_WAKEUP_MASK2);
		__raw_writel(0xFFFF0000, EXYNOS7420_WAKEUP_MASK3);
		break;
	case SYS_DSTOP:
		__raw_writel(0x40003000, EXYNOS7420_WAKEUP_MASK);
		__raw_writel(0xFFFF0000, EXYNOS7420_WAKEUP_MASK2);
		__raw_writel(0xFFFF0000, EXYNOS7420_WAKEUP_MASK3);
		break;
	default:
		break;
	}
}

static void exynos_clear_wakeupmask(void)
{
	__raw_writel(0x0, EXYNOS7420_WAKEUP_MASK);
	__raw_writel(0x0, EXYNOS7420_WAKEUP_MASK2);
	__raw_writel(0x0, EXYNOS7420_WAKEUP_MASK3);
}

static int idle_finisher(unsigned long flags)
{
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, 0);

	return 1;
}

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

	exynos_idle_clock_down(false, ARM);
	exynos_idle_clock_down(false, KFC);

	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret)
		exynos_central_sequencer_ctrl(false);

	cpu_pm_exit();

	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);

	clear_boot_flag(cpuid, C2_STATE);

	/* Clear wakeup state register */
	exynos_clear_wakeupmask();

	return index;
}

static struct sleep_save exynos_lpa_clk_save[] = {
	SAVE_ITEM(EXYNOS7420_MUX_SEL_TOP00),
	SAVE_ITEM(EXYNOS7420_MUX_SEL_TOP0_DISP),
	SAVE_ITEM(EXYNOS7420_MUX_SEL_TOP0_PERIC0),
};

static struct sfr_bit_ctrl exynos_lpa_clk_set[] = {
	/* SFR_CTRL(register, offset, mask, value) */
	SFR_CTRL(EXYNOS7420_MUX_SEL_TOP00, 0, 0x1, 0x0),
	SFR_CTRL(EXYNOS7420_MUX_SEL_TOP0_DISP, 12, 0x7, 0x3),
	SFR_CTRL(EXYNOS7420_MUX_SEL_TOP0_PERIC0, 20, 0x3, 0x1),
	SFR_CTRL(EXYNOS7420_MUX_SEL_TOP0_PERIC0, 8, 0x3, 0x1),
	SFR_CTRL(EXYNOS7420_MUX_SEL_TOP0_PERIC0, 4, 0x7, 0x3),
};

static int exynos_uart_fifo_check(void)
{
	unsigned int ret;
	unsigned int check_val;

	ret = 0;

	/* Check UART for console is empty */
	check_val = __raw_readl(S5P_VA_UART(CONFIG_S3C_LOWLEVEL_UART_PORT) +
				0x18);

	ret = ((check_val >> 16) & 0xff);

	return ret;
}

#if defined(CONFIG_SAMSUNG_USBPHY)
extern void samsung_usb_lpa_resume(void);
#endif

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

	exynos_idle_clock_down(false, ARM);
	exynos_idle_clock_down(false, KFC);

	if (lp_mode == SYS_ALPA)
		__raw_writel(0x1, EXYNOS7420_PMU_SYNC_CTRL);

	/* Save clock SFR which is reset after wakeup from LPA mode */
	s3c_pm_do_save(exynos_lpa_clk_save, ARRAY_SIZE(exynos_lpa_clk_save));

	/* Set clock SFR before enter LPA mode */
	exynos_set_sfr(exynos_lpa_clk_set,
			       ARRAY_SIZE(exynos_lpa_clk_set));

	cpu_pm_enter();
	exynos_lpa_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		exynos_central_sequencer_ctrl(false);
		goto early_wakeup;
	}

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
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_LLI_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_UFS_OPTION);
	__raw_writel((1 << 28), EXYNOS7420_PAD_RETENTION_FSYSGENIO_OPTION);

early_wakeup:
	exynos_lpa_exit();
	cpu_pm_exit();

#ifdef CONFIG_SAMSUNG_USBPHY
	samsung_usb_lpa_resume();
#endif

	/* Restore clock SFR with which is saved before enter LPA mode */
	s3c_pm_do_restore_core(exynos_lpa_clk_save,
			       ARRAY_SIZE(exynos_lpa_clk_save));

	if (lp_mode == SYS_ALPA)
		__raw_writel(0x0, EXYNOS7420_PMU_SYNC_CTRL);

	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);

#if defined(CONFIG_EXYNOS_MIPI_LLI)
	/* LLI_INTR_ENABLE is not retention register
	   So, when LPA is exiting. it has to be recovered. */
	mipi_lli_intr_enable();
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
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
		return exynos_enter_c2(dev, drv, 2);
#else
		return exynos_enter_c2(dev, drv, 1);
#endif
#else
		return exynos_enter_idle(dev, drv, 0);
#endif

	enter_mode = exynos_check_enter_mode();

	if (enter_mode == EXYNOS_CHECK_DIDLE)
		return exynos_enter_core0_aftr(dev, drv, new_index);
#ifdef CONFIG_SND_SAMSUNG_AUDSS
	else if (exynos_check_aud_pwr() == AUD_PWR_ALPA)
		return exynos_enter_core0_lpa(dev, drv, SYS_ALPA, new_index, enter_mode);
	else
#endif
		return exynos_enter_core0_lpa(dev, drv, SYS_LPA, new_index, enter_mode);
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
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		cpu_idle_poll_ctrl(false);

		/* Enable idle clock down after wakeup from skeep mode */
		exynos_idle_clock_down(true, ARM);
		exynos_idle_clock_down(true, KFC);
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

static struct cpuidle_state exynos_cpuidle_set[] __initdata = {
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
		.target_residency       = 2100,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C2",
		.desc                   = "ARM power down",
	},
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	[2] = {
		.enter                  = exynos_enter_c2,
		.exit_latency           = 300,
		.target_residency       = CPD_TARGET_RESIDENCY,
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

static struct cpuidle_driver exynos_idle_driver = {
	.name                   = "exynos_idle",
	.owner                  = THIS_MODULE,
};

static DEFINE_PER_CPU(struct cpuidle_device, exynos_cpuidle_device);

static int __init exynos_init_cpuidle(void)
{
	int i, cpu_id, ret;
	struct cpuidle_device *device;

	exynos_idle_driver.state_count = ARRAY_SIZE(exynos_cpuidle_set);

	for (i = 0; i < exynos_idle_driver.state_count; i++) {
		memcpy(&exynos_idle_driver.states[i],
				&exynos_cpuidle_set[i],
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

#if defined(CONFIG_EXYNOS_CLUSTER_POWER_DOWN) && defined(CONFIG_EXYNOS_MP_CPUFREQ)
	disable_c3_idle = exynos_disable_cluster_power_down;
#endif

	register_pm_notifier(&exynos_cpuidle_notifier);
	register_reboot_notifier(&exynos_cpuidle_reboot_nb);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	spin_lock_init(&cpd_state_lock);
	cpumask_clear(&cpd_state_mask);
#endif

	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);

	return 0;
}
device_initcall(exynos_init_cpuidle);
