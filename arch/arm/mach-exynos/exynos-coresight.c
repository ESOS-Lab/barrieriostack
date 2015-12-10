/*
 * linux/arch/arm/mach-exynos/exynos-coresight.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/of.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include <plat/cpu.h>
#include <mach/cpufreq.h>
#include <mach/regs-pmu.h>
#include <mach/exynos-coresight.h>

#include "common.h"

#define DBG_READ(base, offset)		__raw_readl(base + offset)
#define DBG_WRITE(val, base, offset)	__raw_writel(val, base + offset)

#define DBG_UNLOCK(base)					\
	do {							\
		mb();						\
		__raw_writel(OSLOCK_MAGIC, base + DBGLAR);	\
	}while(0)

#define DBG_LOCK(base)						\
	do {							\
		__raw_writel(0x1, base + DBGLAR);		\
		mb();						\
	}while(0)
#if defined(CONFIG_SOC_EXYNOS5433_REV_1) || defined(CONFIG_SOC_EXYNOS7420)
#define DBG_OSUNLOCK(base)					\
	do {							\
		mb();						\
		__raw_writel(0x0, base + DBGOSLAR);		\
	}while(0)
#else
#define DBG_OSUNLOCK(base)					\
	do {							\
		mb();						\
		__raw_writel(0x1, base + DBGOSLAR);		\
	}while(0)
#endif
#define DBG_OSLOCK(base)					\
	do {							\
		__raw_writel(OSLOCK_MAGIC, base + DBGOSLAR);	\
		mb();						\
	}while(0)

struct cs_dbg_cpu {
	void __iomem		*base;
	u32			state[NR_KEEP_REG];
};

struct cs_dbg {
	u8			arch;
	u8			nr_wp;
	u8			nr_bp;
	struct cs_dbg_cpu	cpu[NR_CPUS];
};

static struct cs_dbg dbg;
#if defined(CONFIG_EXYNOS_CORESIGHT_PC_INFO) ||		\
	defined(CONFIG_EXYNOS_CORESIGHT_MAINTAIN_DBG_REG)
static DEFINE_SPINLOCK(debug_lock);
#endif

#ifdef CONFIG_EXYNOS_CORESIGHT_PC_INFO
#define ITERATION	CONFIG_PC_ITERATION
static unsigned int cs_reg_base;
static int exynos_cs_stat;
unsigned int exynos_cs_pc[NR_CPUS][ITERATION];

void exynos_cs_show_pcval(void)
{
	unsigned long flags;
	unsigned int cpu, iter;
	unsigned int val = 0;
	void __iomem *base;
	char buf[KSYM_SYMBOL_LEN];

	if (exynos_cs_stat < 0)
		return;

	/* add code to prevent going into C2 mode*/
	cpu_idle_poll_ctrl(true);

	spin_lock_irqsave(&debug_lock, flags);

	for (iter = 0; iter < ITERATION; iter++) {
		for (cpu = 0; cpu < NR_CPUS; cpu++) {
			base = dbg.cpu[cpu].base;
			if (base == NULL || !exynos_cpu.power_state(cpu))
				continue;

			DBG_UNLOCK(base);
			/* Release OSlock */
			DBG_OSUNLOCK(base);

			/* Read current PC value */
			if (!exynos_cpu.power_state(cpu))
				continue;
			val = DBG_READ(base, DBGPCSR);
			if(val == 0x0) {
				if (!exynos_cpu.power_state(cpu))
					continue;
				val = DBG_READ(base, DBGPCSR_2);
			}

			if (cpu >= CORE_CNT) {
				/* The PCSR of A15 shoud be substracted 0x8 from
				 * curretnly PCSR value */
				val -= 0x8;
			}
			exynos_cs_pc[cpu][iter] = val;
		}
	}

	spin_unlock_irqrestore(&debug_lock, flags);
	/* revert prevent C2 mode*/
	cpu_idle_poll_ctrl(false);

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		pr_err("CPU[%d] saved pc value\n", cpu);
		for (iter = 0; iter < ITERATION; iter++) {
			if(exynos_cs_pc[cpu][iter] == 0)
				continue;

			sprint_symbol(buf, exynos_cs_pc[cpu][iter]);
			pr_err("      0x%08x : %s\n",
				exynos_cs_pc[cpu][iter], buf);
		}
	}
}
EXPORT_SYMBOL(exynos_cs_show_pcval);

/* check sjtag status*/
static int __init exynos_cs_sjtag_init(void)
{
	void __iomem *sjtag_base;
	unsigned int sjtag;

	pr_debug("%s\n", __func__);

	/* Check Secure JTAG */
	sjtag_base = ioremap(cs_reg_base + CS_SJTAG_OFFSET, SZ_8);
	if (!sjtag_base) {
		pr_err("%s: cannot ioremap cs base.\n", __func__);
		exynos_cs_stat = -ENOMEM;
		goto err_func;
	}

	sjtag = __raw_readl(sjtag_base + SJTAG_STATUS);
	iounmap(sjtag_base);

	if (sjtag & SJTAG_SOFT_LOCK) {
		exynos_cs_stat = -EIO;
		goto err_func;
	}

	pr_info("exynos-coresight Jtag DBGEN signal on\n");
err_func:
	return exynos_cs_stat;
}
#endif

#ifdef CONFIG_EXYNOS_CORESIGHT_MAINTAIN_DBG_REG
/* With Trace32 attached debuging, T32 must set FLAG_T32_EN as true. */
bool FLAG_T32_EN = false;

/* save debug resgisters when suspending */
static void debug_suspend_cpu(int cpu)
{
	unsigned long flags;
	int i;
	struct cs_dbg_cpu *cpudata = &dbg.cpu[cpu];
	void __iomem *base = cpudata->base;

	pr_debug("%s: cpu %d\n", __func__, cpu);

	spin_lock_irqsave(&debug_lock, flags);
	cpudata->state[SLEEP_FLAG] = 1;
	spin_unlock_irqrestore(&debug_lock, flags);

	DBG_UNLOCK(base);

	switch(dbg.arch) {
	/* cortex A9 case */
	case DEBUG_ARCH_V7:
		cpudata->state[WFAR] = DBG_READ(base, DBGWFAR);
		cpudata->state[VCR] = DBG_READ(base, DBGVCR);

		for(i =0 ;i < dbg.nr_bp; i++) {
			cpudata->state[BVR0 + i] = DBG_READ(base, DBGBVRn(i));
			cpudata->state[BCR0 + i] = DBG_READ(base, DBGBCRn(i));
		}
		for(i =0 ;i < dbg.nr_wp; i++) {
			cpudata->state[WVR0 + i] = DBG_READ(base, DBGWVRn(i));
			cpudata->state[WCR0 + i] = DBG_READ(base, DBGWCRn(i));
		}

		cpudata->state[PRCR] = DBG_READ(base, DBGPRCR);
		cpudata->state[DTRTX] = DBG_READ(base, DBGDTRTX);
		cpudata->state[DTRRX] = DBG_READ(base, DBGDTRRX);
		cpudata->state[DSCR] = DBG_READ(base, DBGDSCR);
		break;
	/* cortex A7, A15 case */
	case DEBUG_ARCH_V7P1:
		/* Set OS lock */
		DBG_OSLOCK(base);

		cpudata->state[WFAR] = DBG_READ(base, DBGWFAR);
		cpudata->state[VCR] = DBG_READ(base, DBGVCR);

		for(i =0 ;i < dbg.nr_bp; i++) {
			cpudata->state[BVR0 + i] = DBG_READ(base, DBGBVRn(i));
			cpudata->state[BCR0 + i] = DBG_READ(base, DBGBCRn(i));
		}
		for(i =0 ;i < dbg.nr_wp; i++) {
			cpudata->state[WVR0 + i] = DBG_READ(base, DBGWVRn(i));
			cpudata->state[WCR0 + i] = DBG_READ(base, DBGWCRn(i));
		}

		cpudata->state[PRCR] = DBG_READ(base, DBGPRCR);
		cpudata->state[CLAIM] = DBG_READ(base, DBGCLAIMCLR);
		cpudata->state[DTRTX] = DBG_READ(base, DBGDTRTX);
		cpudata->state[DTRRX] = DBG_READ(base, DBGDTRRX);
		cpudata->state[DSCR] = DBG_READ(base, DBGDSCR);
		break;
	default:
		break;
	}
	DBG_LOCK(base);

	pr_debug("%s: cpu %d done\n", __func__, cpu);
}

/* restore debug registers when resuming */
static void debug_resume_cpu(int cpu)
{
	unsigned long flags;
	int i;
	struct cs_dbg_cpu *cpudata = &dbg.cpu[cpu];
	void __iomem *base = cpudata->base;

	pr_debug("%s: cpu %d\n", __func__, cpu);

	if(!cpudata->state[SLEEP_FLAG])
		return;

	spin_lock_irqsave(&debug_lock, flags);
	cpudata->state[SLEEP_FLAG] = 0;
	spin_unlock_irqrestore(&debug_lock, flags);

	DBG_UNLOCK(base);

	switch(dbg.arch) {
	/* cortex A9 case */
	case DEBUG_ARCH_V7:
		DBG_WRITE(cpudata->state[WFAR], base, DBGWFAR);
		DBG_WRITE(cpudata->state[VCR], base, DBGVCR);

		for(i =0 ;i < dbg.nr_bp; i++) {
			DBG_WRITE(cpudata->state[BVR0 + i], base, DBGBVRn(i));
			DBG_WRITE(cpudata->state[BCR0 + i], base, DBGBCRn(i));
		}
		for(i =0 ;i < dbg.nr_wp; i++) {
			DBG_WRITE(cpudata->state[WVR0 + i], base, DBGWVRn(i));
			DBG_WRITE(cpudata->state[WCR0 + i], base, DBGWCRn(i));
		}

		DBG_WRITE(cpudata->state[PRCR], base, DBGPRCR);
		DBG_WRITE(cpudata->state[DTRTX], base, DBGDTRTX);
		DBG_WRITE(cpudata->state[DTRRX], base, DBGDTRRX);
		DBG_WRITE(cpudata->state[DSCR] & DBGDSCR_MASK, base, DBGDSCR);
		break;
	/* cortex A7, A15 case */
	case DEBUG_ARCH_V7P1:
		/* Set OS lock */
		DBG_OSLOCK(base);

		DBG_WRITE(cpudata->state[WFAR], base, DBGWFAR);
		DBG_WRITE(cpudata->state[VCR], base, DBGVCR);

		for(i =0 ;i < dbg.nr_bp; i++) {
			DBG_WRITE(cpudata->state[BVR0 + i], base, DBGBVRn(i));
			DBG_WRITE(cpudata->state[BCR0 + i], base, DBGBCRn(i));
		}
		for(i =0 ;i < dbg.nr_wp; i++) {
			DBG_WRITE(cpudata->state[WVR0 + i], base, DBGWVRn(i));
			DBG_WRITE(cpudata->state[WCR0 + i], base, DBGWCRn(i));
		}

		DBG_WRITE(cpudata->state[PRCR], base, DBGPRCR);
		DBG_WRITE(cpudata->state[CLAIM], base, DBGCLAIMSET);
		DBG_WRITE(cpudata->state[DTRTX], base, DBGDTRTX);
		DBG_WRITE(cpudata->state[DTRRX], base, DBGDTRRX);
		DBG_WRITE(cpudata->state[DSCR] & DBGDSCR_MASK, base, DBGDSCR);

		DBG_OSUNLOCK(base);
		break;
	default:
		break;
	}
	DBG_LOCK(base);

	pr_debug("%s: %d done\n", __func__, cpu);
}

static inline bool dbg_arch_supported(u8 arch)
{
	switch(arch) {
	case DEBUG_ARCH_V7:
	case DEBUG_ARCH_V7P1:
		break;
	default:
		return false;
	}
	return true;
}

static int exynos_cs_pm_notifier(struct notifier_block *self,
		unsigned long cmd, void *v)
{
	int cpu = smp_processor_id();

	switch (cmd) {
	case CPU_PM_ENTER:
		debug_suspend_cpu(cpu);
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		debug_resume_cpu(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER:
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		break;
	}

	return NOTIFY_OK;
}

static int __cpuinit exynos_cs_cpu_notifier(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	int cpu = (unsigned long)hcpu;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
	case CPU_DOWN_FAILED:
		debug_resume_cpu(cpu);
		break;
	case CPU_DYING:
		debug_suspend_cpu(cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata exynos_cs_pm_notifier_block = {
	.notifier_call = exynos_cs_pm_notifier,
};

static struct notifier_block __cpuinitdata exynos_cs_cpu_notifier_block = {
	.notifier_call = exynos_cs_cpu_notifier,
};

static int __init exynos_cs_debug_init(void)
{
	unsigned int dbgdidr;
	int ret = 0;

	if(!FLAG_T32_EN) {
		pr_err("%s: FLAG_T32_EN value is not true.\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	dbgdidr = DBG_READ(dbg.cpu[0].base, DBGDIDR);
	dbg.arch = (dbgdidr >> 16) & 0xf;
	dbg.nr_bp = ((dbgdidr >> 24) & 0xf) + 1;
	dbg.nr_wp = ((dbgdidr >> 28) & 0xf) + 1;

	if( !dbg_arch_supported(dbg.arch)) {
		pr_err("%s: DBG archtecture is not supported.\n", __func__);
		ret = -EPERM;
		goto err;
	}

	/* check core to support. */
	ret = cpu_pm_register_notifier(&exynos_cs_pm_notifier_block);
	if (ret < 0)
		goto err;

	ret = register_cpu_notifier(&exynos_cs_cpu_notifier_block);
	if (ret < 0)
		goto err;

	pr_info("exynos-coresight debug enable: arch: %d bp:%d, wp:%d\n",
			dbg.arch, dbg.nr_bp, dbg.nr_wp);
err:
	return ret;
}
#endif

static const struct of_device_id of_exynos_cs_matches[] __initconst= {
	{.compatible = "exynos,coresight"},
	{},
};

static int exynos_cs_init_dt(void)
{
	struct device_node *np = NULL;
	const unsigned int *cs_reg, *offset;
	unsigned int cs_offset;
	int len, i = 0;

	np = of_find_matching_node(NULL, of_exynos_cs_matches);

	cs_reg = of_get_property(np, "reg", &len);
	if (!cs_reg)
		return -ESPIPE;

	cs_reg_base = be32_to_cpup(cs_reg);

	while ((np = of_find_node_by_type(np, "cs"))) {
		offset = of_get_property(np, "offset", &len);
		if (!offset)
			return -ESPIPE;

		cs_offset = be32_to_cpup(offset);
		dbg.cpu[i].base = ioremap(cs_reg_base + cs_offset, SZ_4K);
		if (!dbg.cpu[i].base) {
			pr_err("%s: cannot ioremap cs base.\n", __func__);
			return -ENOMEM;
		}

		i++;
	}
	return 0;
}

static int __init exynos_cs_init(void)
{
	int ret = 0;
	unsigned int dvfs_id;

	ret = exynos_cs_init_dt();
	if(ret < 0)
		goto err;

	/* Impossible to access KFC coresight before DVFS v2.5. */
	if (soc_is_exynos5422()) {
		dvfs_id = __raw_readl(S5P_VA_CHIPID + 4);
		dvfs_id = (dvfs_id >> 8) & 0x3;
		if (dvfs_id != 0x3) {
			dbg.cpu[0].base = NULL;
			dbg.cpu[1].base = NULL;
			dbg.cpu[2].base = NULL;
			dbg.cpu[3].base = NULL;
		}
	}

#ifdef CONFIG_EXYNOS_CORESIGHT_PC_INFO
	ret = exynos_cs_sjtag_init();
	if(ret < 0)
		goto err;
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_MAINTAIN_DBG_REG
	ret = exynos_cs_debug_init();
	if(ret < 0)
		goto err;
#endif
err:
	return ret;
}
subsys_initcall(exynos_cs_init);
