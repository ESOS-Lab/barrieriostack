/* linux/arch/arm/mach-exynos/pmu.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/bug.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <mach/pmu_cal_sys.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include "common.h"

#define REG_CPU_STATE_ADDR     (S5P_VA_SYSRAM_NS + 0x28)

void __iomem *exynos_list_feed[] = {
	EXYNOS7420_ATLAS_NONCPU_OPTION,
	EXYNOS7420_APOLLO_NONCPU_OPTION,
	EXYNOS7420_TOP_PWR_OPTION,
	EXYNOS7420_TOP_PWR_MIF_OPTION,
	EXYNOS7420_AUD_OPTION,
	EXYNOS7420_BUS0_OPTION,
	EXYNOS7420_CAM0_OPTION,
	EXYNOS7420_CAM1_OPTION,
	EXYNOS7420_DISP_OPTION,
	EXYNOS7420_FSYS0_OPTION,
	EXYNOS7420_FSYS1_OPTION,
	EXYNOS7420_G2D_OPTION,
	EXYNOS7420_G3D_OPTION,
	EXYNOS7420_HEVC_OPTION,
	EXYNOS7420_ISP0_OPTION,
	EXYNOS7420_ISP1_OPTION,
	EXYNOS7420_MFC_OPTION,
	EXYNOS7420_MSCL_OPTION,
	EXYNOS7420_VPP_OPTION,
};

void set_boot_flag(unsigned int cpu, unsigned int mode)
{
	unsigned int phys_cpu = cpu_logical_map(cpu);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);
	addr = REG_CPU_STATE_ADDR + 4 * (core + cluster * 4);

	tmp = __raw_readl(addr);

	if (mode & BOOT_MODE_MASK)
		tmp &= ~BOOT_MODE_MASK;
	else
		BUG_ON(mode == 0);

	tmp |= mode;
	__raw_writel(tmp, addr);
}

void clear_boot_flag(unsigned int cpu, unsigned int mode)
{
	unsigned int phys_cpu = cpu_logical_map(cpu);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	BUG_ON(mode == 0);

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);
	addr = REG_CPU_STATE_ADDR + 4 * (core + cluster * 4);

	tmp = __raw_readl(addr);
	tmp &= ~mode;
	__raw_writel(tmp, addr);
}

void exynos7420_secondary_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_CORE_INIT_WAKEUP_FROM_LOWPWR | EXYNOS_CORE_PWR_EN;

	__raw_writel(tmp, addr);
}

void exynos7420_cpu_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_CORE_PWR_EN;
	__raw_writel(tmp, addr);
}

void exynos7420_cpu_down(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp &= ~(EXYNOS_CORE_PWR_EN);
	__raw_writel(tmp, addr);
}

unsigned int exynos7420_cpu_state(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int core, cluster, val;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	val = __raw_readl(EXYNOS_ARM_CORE_STATUS(core + (4 * cluster)))
						& EXYNOS_CORE_PWR_EN;

	return val == 0xf;
}

void exynos_cpu_sequencer_ctrl(bool enable)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS7420_ATLAS_CPUSEQUENCER_OPTION);
	if (enable)
		tmp |= ENABLE_CPUSEQ;
	else
		tmp &= ~ENABLE_CPUSEQ;
	__raw_writel(tmp, EXYNOS7420_ATLAS_CPUSEQUENCER_OPTION);
}

static void exynos_core_option(void)
{
	unsigned int cpu, tmp;

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		tmp = __raw_readl(EXYNOS_ARM_CORE_OPTION(cpu));
		tmp |= EXYNOS5_USE_SC_COUNTER;
		tmp |= EXYNOS5_USE_SC_FEEDBACK;
		tmp |= EXYNOS7420_USE_SMPEN;
		__raw_writel(tmp, EXYNOS_ARM_CORE_OPTION(cpu));
	}

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		tmp = __raw_readl(EXYNOS7420_ARM_CORE_DURATION(cpu));
		tmp &= ~EXYNOS7420_DUR_SCALL;
		tmp |= (0x1 << EXYNOS7420_DUR_SCALL_SHIFT);
		__raw_writel(tmp, EXYNOS7420_ARM_CORE_DURATION(cpu));
	}
}

static void exynos_use_feedback(void)
{
	unsigned int i;
	unsigned int tmp;

	/*
	 * Enable only SC_FEEDBACK
	 */
	for (i = 0; i < ARRAY_SIZE(exynos_list_feed); i++) {
		tmp = __raw_readl(exynos_list_feed[i]);
		tmp &= ~EXYNOS5_USE_SC_COUNTER;
		tmp |= EXYNOS5_USE_SC_FEEDBACK;
		__raw_writel(tmp, exynos_list_feed[i]);
	}
}

void exynos_atlas_asyncbridge_ignore_lpi(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS7420_LPI_MASK_ATLAS_ASYNCBRIDGE);
	tmp |= (ASATBZ_MSTIF_ATLAS_AUD | ASATBZ_MSTIF_ATLAS_CAM1);
	__raw_writel(tmp, EXYNOS7420_LPI_MASK_ATLAS_ASYNCBRIDGE);
}

void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type)
{
	unsigned int value;
	unsigned int wdt_auto_reset_dis;
	unsigned int wdt_reset_mask;

	/*
	 * When SYS_WDTRESET is set, watchdog timer reset request is ignored
	 * by power management unit.
	 */
	if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE0) {
		wdt_auto_reset_dis = EXYNOS_SYS_WDTRESET;
		wdt_reset_mask = EXYNOS_SYS_WDTRESET;
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE1) {
		wdt_auto_reset_dis = EXYNOS5410_SYS_WDTRESET;
		wdt_reset_mask = EXYNOS5410_SYS_WDTRESET;
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE2) {
		wdt_auto_reset_dis = EXYNOS_ARM_WDTRESET |
			EXYNOS_KFC_WDTRESET;
		wdt_reset_mask = EXYNOS_ARM_WDTRESET;
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE3) {
		wdt_auto_reset_dis = EXYNOS_ARM_WDTRESET |
			EXYNOS_KFC_WDTRESET;
		wdt_reset_mask = EXYNOS_KFC_WDTRESET;
	} else {
		pr_err("Failed to %s pmu wdt reset\n",
				on ? "enable" : "disable");
		return;
	}

	value = __raw_readl(EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	if (on)
		value &= ~wdt_auto_reset_dis;
	else
		value |= wdt_auto_reset_dis;
	__raw_writel(value, EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	value = __raw_readl(EXYNOS_MASK_WDT_RESET_REQUEST);
	if (on)
		value &= ~wdt_reset_mask;
	else
		value |= wdt_reset_mask;
	__raw_writel(value, EXYNOS_MASK_WDT_RESET_REQUEST);

	return;
}

int __init exynos7420_pmu_init(void)
{
	unsigned int tmp, i;

	/* Enable USE_STANDBY_WFI for all CORE */
	__raw_writel(EXYNOS5_USE_STANDBY_WFI_ALL |
		EXYNOS_USE_PROLOGNED_LOGIC_RESET, EXYNOS_CENTRAL_SEQ_OPTION);

	/* L2 use retention disable */
	tmp = __raw_readl(EXYNOS_L2_OPTION(0));
	tmp &= ~(USE_RETENTION | USE_STANDBYWFIL2);
	tmp |= USE_DEACTIVATE_ACP | USE_DEACTIVATE_ACE;
	__raw_writel(tmp, EXYNOS_L2_OPTION(0));

	/* Use SC_FEEDBACK */
	exynos_use_feedback();

	/* Initialize setting for core */
	exynos_core_option();

	/* Initialize L2 cache */
	for (i = 0; i < CLUSTER_NUM; i++) {
		tmp = __raw_readl(EXYNOS_L2_OPTION(i));
		tmp &= ~(USE_RETENTION | USE_AUTOMATIC_L2FLUSHREQ);
		if (i == 0)
			tmp |= (USE_STANDBYWFIL2 | USE_DEACTIVATE_ACP | USE_DEACTIVATE_ACE);
		__raw_writel(tmp, EXYNOS_L2_OPTION(i));
	}

	/* Ignore LPI for ASATB at ATLAS  */
	exynos_atlas_asyncbridge_ignore_lpi();

	/* UP Scheduler Enable */
	tmp = __raw_readl(EXYNOS7420_UP_SCHEDULER);
	tmp |= ENABLE_EAGLE_CPU;
	__raw_writel(tmp, EXYNOS7420_UP_SCHEDULER);

	/* Set PSHOLD port for output high and enable signal */
	tmp = __raw_readl(EXYNOS7420_PS_HOLD_CONTROL);
	tmp |= (EXYNOS_PS_HOLD_OUTPUT_HIGH | EXYNOS_PS_HOLD_EN);
	__raw_writel(tmp, EXYNOS7420_PS_HOLD_CONTROL);

	/* Skip block power down for ATLAS during automatic power down sequence */
	tmp = __raw_readl(EXYNOS7420_ATLAS_CPUSEQUENCER_OPTION);
	tmp |= SKIP_BLK_PWR_DOWN;
	__raw_writel(tmp, EXYNOS7420_ATLAS_CPUSEQUENCER_OPTION);

	/* Set clock freeze cycle before and after ARM clamp to 0 */
	__raw_writel(0x0, EXYNOS7420_ATLAS_ARMCLK_STOPCTRL);
	__raw_writel(0x0, EXYNOS7420_APOLLO_ARMCLK_STOPCTRL);

	exynos_cpu.power_up = exynos7420_secondary_up;
	exynos_cpu.power_down = exynos7420_cpu_down;
	exynos_cpu.power_state = exynos7420_cpu_state;

	pmu_cal_sys_init();

	return 0;
}
