/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
	EXYNOS5433_EAGLE_NONCPU_OPTION,
	EXYNOS5433_KFC_NONCPU_OPTION,
	EXYNOS5_TOP_PWR_OPTION,
	EXYNOS5_TOP_PWR_SYSMEM_OPTION,
	EXYNOS5433_GSCL_OPTION,
	EXYNOS5433_CAM0_OPTION,
	EXYNOS5433_MSCL_OPTION,
	EXYNOS5433_G3D_OPTION,
	EXYNOS5433_DISP_OPTION,
	EXYNOS5433_CAM1_OPTION,
	EXYNOS5433_AUD_OPTION,
	EXYNOS5433_FSYS_OPTION,
	EXYNOS5433_BUS2_OPTION,
	EXYNOS5433_G2D_OPTION,
	EXYNOS5433_ISP_OPTION,
	EXYNOS5433_MFC_OPTION,
	EXYNOS5433_HEVC_OPTION,
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

void exynos5433_secondary_up(unsigned int cpu_id)
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

void exynos5433_cpu_up(unsigned int cpu_id)
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

void exynos5433_cpu_down(unsigned int cpu_id)
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

unsigned int exynos5433_cpu_state(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int core, cluster, val;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	val = __raw_readl(EXYNOS_ARM_CORE_STATUS(core + (4 * cluster)))
						& EXYNOS_CORE_PWR_EN;

	return val == EXYNOS_CORE_PWR_EN;
}

unsigned int exynos5433_cluster_state(unsigned int cluster)
{
	unsigned int cpu_start, cpu_end, ret;

	BUG_ON(cluster > 2);

	cpu_start = (cluster) ? 4 : 0;
	cpu_end = cpu_start + 4;

	for (;cpu_start < cpu_end; cpu_start++) {
		ret = exynos5433_cpu_state(cpu_start);
		if (ret)
			break;
	}

	return ret ? 1 : 0;
}

#ifdef CONFIG_SCHED_HMP
extern struct cpumask hmp_slow_cpu_mask;
extern struct cpumask hmp_fast_cpu_mask;

#define cpu_online_hmp(cpu, mask)      cpumask_test_cpu((cpu), mask)
#endif

bool exynos5433_is_last_core(unsigned int cpu)
{
#ifdef CONFIG_SCHED_HMP
	unsigned int cluster = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
#endif
	unsigned int cpu_id;
	struct cpumask mask, mask_and_online;

#ifdef CONFIG_SCHED_HMP
	if (cluster)
		cpumask_copy(&mask, &hmp_slow_cpu_mask);
	else
		cpumask_copy(&mask, &hmp_fast_cpu_mask);
#endif

	cpumask_and(&mask_and_online, &mask, cpu_online_mask);

	for_each_cpu(cpu_id, &mask) {
		if (cpu_id == cpu)
			continue;
#ifdef CONFIG_SCHED_HMP
		if (cpu_online_hmp(cpu_id, &mask_and_online))
			return false;
#else
		if (cpu_online(cpu_id))
			return false;
#endif
	}

	return true;
}

unsigned int exynos5433_l2_status(unsigned int cluster)
{
	unsigned int state;

	BUG_ON(cluster > 2);

	state = __raw_readl(EXYNOS_L2_STATUS(cluster));

	return state & EXYNOS_L2_PWR_EN;
}

void exynos5433_l2_up(unsigned int cluster)
{
	unsigned int tmp = (EXYNOS_L2_PWR_EN << 8) | EXYNOS_L2_PWR_EN;

	if (exynos5433_l2_status(cluster))
		return;

	tmp |= __raw_readl(EXYNOS_L2_CONFIGURATION(cluster));
	__raw_writel(tmp, EXYNOS_L2_CONFIGURATION(cluster));

	/* wait for turning on */
	while (exynos5433_l2_status(cluster));
}

void exynos5433_l2_down(unsigned int cluster)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS_L2_CONFIGURATION(cluster));
	tmp &= ~(EXYNOS_L2_PWR_EN);

	__raw_writel(tmp, EXYNOS_L2_CONFIGURATION(cluster));
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

void exynos_xxti_sys_powerdown(bool enable)
{
	unsigned int value;

	value = __raw_readl(EXYNOS5_XXTI_SYS_PWR_REG);

	if (enable)
		value |= EXYNOS_SYS_PWR_CFG;
	else
		value &= ~EXYNOS_SYS_PWR_CFG;

	__raw_writel(value, EXYNOS5_XXTI_SYS_PWR_REG);
}

void exynos_cpu_sequencer_ctrl(bool enable)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5433_EAGLE_CPUSEQ_OPTION);
	if (enable)
		tmp |= ENABLE_CPUSEQ;
	else
		tmp &= ~ENABLE_CPUSEQ;
	__raw_writel(tmp, EXYNOS5433_EAGLE_CPUSEQ_OPTION);
}

static void __maybe_unused exynos_cpu_reset_assert_ctrl(bool on, enum cpu_type cluster)
{
	unsigned int i;
	unsigned int option;
	unsigned int cpu_s, cpu_f;

	if (cluster == KFC) {
		cpu_s = CPUS_PER_CLUSTER;
		cpu_f = cpu_s + CPUS_PER_CLUSTER - 1;
	} else {
		cpu_s = 0;
		cpu_f = CPUS_PER_CLUSTER - 1;
	}

	for (i = cpu_s; i <= cpu_f; i++) {
		option = __raw_readl(EXYNOS_ARM_CORE_OPTION(i));
		option = on ? (option | EXYNOS_USE_DELAYED_RESET_ASSERTION) :
				   (option & ~EXYNOS_USE_DELAYED_RESET_ASSERTION);
		__raw_writel(option, EXYNOS_ARM_CORE_OPTION(i));
	}

}

/*
 * The slave of ATB_AUD_CSSYS's async bridge bus does not have
 * LPI interface, so the problem occur when BLK_AUD is powered off.
 * To avoid this situation, ignore LPI.
 */
void exynos_eagle_asyncbridge_ignore_lpi(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5433_LPI_MASK_EAGLE_ASYNCBRIDGE);
	tmp |= EXYNOS5433_LPI_MASK_EAGLE_ASYNCBRIDGE_ATB_AUD_CSSYS;
	__raw_writel(tmp, EXYNOS5433_LPI_MASK_EAGLE_ASYNCBRIDGE);
}

/*
 * USE_SMPEN:
 *  If USE_SMPEN is set to HIGH, SMPEN bit in CPUECTLR_EL1 register
 *  should be LOW for power-down sequence to begin. ARM TRM guides
 *  that SMPEN is kept to HIGH and it should be set LOW before
 *  power-down. In Helsinki Prime EVT1, follow this guide.
 */
void exynos_core_option(void)
{
	unsigned int cpu, tmp;

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		tmp = __raw_readl(EXYNOS_ARM_CORE_OPTION(cpu));
		tmp |= EXYNOS5_USE_SC_COUNTER;
		tmp |= EXYNOS5_USE_SC_FEEDBACK;
		tmp |= EXYNOS5433_USE_SMPEN;
		__raw_writel(tmp, EXYNOS_ARM_CORE_OPTION(cpu));
	}

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		tmp = __raw_readl(EXYNOS5433_ARM_CORE_DURATION(cpu));
		tmp |= EXYNOS5433_DUR_WAIT_RESET;
		tmp &= ~EXYNOS5433_DUR_SCALL;
		tmp |= EXYNOS5433_DUR_SCALL_VALUE;
		__raw_writel(tmp, EXYNOS5433_ARM_CORE_DURATION(cpu));
	}
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
		wdt_auto_reset_dis = EXYNOS5433_SYS_WDTRESET_EGL |
			EXYNOS5433_SYS_WDTRESET_KFC;
		wdt_reset_mask = EXYNOS5433_SYS_WDTRESET_EGL;
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE3) {
		wdt_auto_reset_dis = EXYNOS5433_SYS_WDTRESET_EGL |
			EXYNOS5433_SYS_WDTRESET_KFC;
		wdt_reset_mask = EXYNOS5433_SYS_WDTRESET_KFC;
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

#define EXYNOS5433_PRINT_PMU(name) \
	pr_info("  - %s  CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n", \
			#name, \
			__raw_readl(EXYNOS5433_##name##_CONFIGURATION), \
			__raw_readl(EXYNOS5433_##name##_STATUS), \
			__raw_readl(EXYNOS5433_##name##_OPTION))

void show_exynos_pmu(void)
{
	int i;
	pr_info("\n");
	pr_info(" -----------------------------------------------------------------------------------\n");
	pr_info(" **** CPU PMU register dump ****\n");
	for (i=0; i < NR_CPUS; i++) {
		printk("[%d]   CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n", i,
			__raw_readl(EXYNOS_ARM_CORE0_CONFIGURATION + i * 0x80),
			__raw_readl(EXYNOS_ARM_CORE0_STATUS + i * 0x80),
			__raw_readl(EXYNOS_ARM_CORE0_OPTION + i * 0x80));
	}
	pr_info(" **** EGL L2 CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n",
			__raw_readl(EXYNOS_L2_CONFIGURATION(0)),
			__raw_readl(EXYNOS_L2_STATUS(0)),
			__raw_readl(EXYNOS_L2_OPTION(0)));
	pr_info(" **** KFC L2 CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n",
			__raw_readl(EXYNOS_L2_CONFIGURATION(1)),
			__raw_readl(EXYNOS_L2_STATUS(1)),
			__raw_readl(EXYNOS_L2_OPTION(1)));
	pr_info(" **** CPUSEQ CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n",
			__raw_readl(EXYNOS5433_EAGLE_CPUSEQ_CONFIGURATION),
			__raw_readl(EXYNOS5433_EAGLE_CPUSEQ_STATUS),
			__raw_readl(EXYNOS5433_EAGLE_CPUSEQ_OPTION));
	pr_info(" **** CENTRL CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n",
			__raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION),
			__raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION + 0x4),
			__raw_readl(EXYNOS_CENTRAL_SEQ_OPTION));
	pr_info(" **** LOCAL BLOCK POWER ****\n");
	EXYNOS5433_PRINT_PMU(GSCL);
	EXYNOS5433_PRINT_PMU(CAM0);
	EXYNOS5433_PRINT_PMU(CAM1);
	EXYNOS5433_PRINT_PMU(MSCL);
	EXYNOS5433_PRINT_PMU(G3D);
	EXYNOS5433_PRINT_PMU(DISP);
	EXYNOS5433_PRINT_PMU(AUD);
	EXYNOS5433_PRINT_PMU(FSYS);
	EXYNOS5433_PRINT_PMU(BUS2);
	EXYNOS5433_PRINT_PMU(G2D);
	EXYNOS5433_PRINT_PMU(ISP);
	EXYNOS5433_PRINT_PMU(MFC);
	EXYNOS5433_PRINT_PMU(HEVC);
	pr_info(" -----------------------------------------------------------------------------------\n");
}

int __init exynos5433_pmu_init(void)
{
	unsigned int tmp;
	/*
	 * Set measure power on/off duration
	 * Use SC_USE_FEEDBACK
	 */
	exynos_use_feedback();

	__raw_writel(0x3A98, EXYNOS5433_OSC_DURATION);

	/* Enable USE_STANDBY_WFI for all CORE */
	__raw_writel(EXYNOS5_USE_STANDBY_WFI_ALL |
		EXYNOS_USE_PROLOGNED_LOGIC_RESET, EXYNOS_CENTRAL_SEQ_OPTION);

	exynos_eagle_asyncbridge_ignore_lpi();

	/*
	 * Disable ARM USE_AUTOMATIC_L2FLUSHREQ which exists in only EVT1
	 */
	tmp = __raw_readl(EXYNOS_L2_OPTION(0));
	tmp &= ~EXYNOS5433_USE_AUTOMATIC_L2FLUSHREQ;
	__raw_writel(tmp, EXYNOS_L2_OPTION(0));

	tmp = __raw_readl(EXYNOS_L2_OPTION(1));
	tmp &= ~EXYNOS5433_USE_AUTOMATIC_L2FLUSHREQ;
	__raw_writel(tmp, EXYNOS_L2_OPTION(1));

	/* Mask external interrupts which belong EINT_GPF */
	__raw_writel(0xFFFFFFFF, EXYNOS5433_EINT_WAKEUP_MASK1);

	/* L2 use retention disable */
	tmp = __raw_readl(EXYNOS_L2_OPTION(0));
	tmp &= ~USE_RETENTION;
	tmp |= USE_STANDBYWFIL2 | USE_DEACTIVATE_ACP | USE_DEACTIVATE_ACE;
	__raw_writel(tmp, EXYNOS_L2_OPTION(0));
	tmp = __raw_readl(EXYNOS_L2_OPTION(1));
	tmp &= ~USE_RETENTION;
	__raw_writel(tmp, EXYNOS_L2_OPTION(1));

	/* UP Scheduler Enable */
	tmp = __raw_readl(EXYNOS5_UP_SCHEDULER);
	tmp |= ENABLE_EAGLE_CPU;
	__raw_writel(tmp, EXYNOS5_UP_SCHEDULER);

	/*
	 * Set PSHOLD port for output high
	 */
	tmp = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
	tmp |= EXYNOS_PS_HOLD_OUTPUT_HIGH;
	__raw_writel(tmp, EXYNOS_PS_HOLD_CONTROL);

	/*
	 * Enable signal for PSHOLD port
	 */
	tmp = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
	tmp |= EXYNOS_PS_HOLD_EN;
	__raw_writel(tmp, EXYNOS_PS_HOLD_CONTROL);

	/* Enable non retention flip-flop reset */
	tmp = __raw_readl(EXYNOS_PMU_SPARE0);
	tmp |= EXYNOS_EN_NONRET_RESET;
	__raw_writel(tmp, EXYNOS_PMU_SPARE0);

	/* Set clock freeze cycle before and after ARM clamp to 0 */
	__raw_writel(0x0, EXYNOS5430_EGL_STOPCTRL);
	__raw_writel(0x0, EXYNOS5430_KFC_STOPCTRL);

	exynos_cpu.power_up = exynos5433_secondary_up;
	exynos_cpu.power_state = exynos5433_cpu_state;
	exynos_cpu.power_down = exynos5433_cpu_down;
	exynos_cpu.cluster_down = exynos5433_l2_down;
	exynos_cpu.cluster_state = exynos5433_cluster_state;
	exynos_cpu.is_last_core = exynos5433_is_last_core;

	pmu_cal_sys_init();

	pr_info("EXYNOS5433 PMU Initialize\n");

	return 0;
}

static int __init arch_init_core_option(void)
{
	exynos_core_option();
	return 0;
}

arch_initcall(arch_init_core_option);
