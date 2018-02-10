/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * CPUIDLE driver for exynos 64bit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/reboot.h>
#include <linux/of.h>
#ifdef CONFIG_SEC_PM
#include <linux/moduleparam.h>
#endif

#include <asm/suspend.h>
#include <asm/tlbflush.h>
#include <asm/psci.h>

#include <mach/exynos-powermode.h>

#include "of_idle_states.h"
#include "cpuidle_profiler.h"

#ifdef CONFIG_SEC_PM
#define CPUIDLE_ENABLE_MASK (ENABLE_C2 | ENABLE_C3_LPM)

static enum {
	ENABLE_C2	= BIT(0),
	ENABLE_C3_LPM	= BIT(1),
} enable_mask = CPUIDLE_ENABLE_MASK;

DEFINE_SPINLOCK(enable_mask_lock);

static int set_enable_mask(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_uint(val, kp);
	unsigned long flags;

	pr_info("%s: enable_mask=0x%x\n", __func__, enable_mask);

	if (rv)
		return rv;

	spin_lock_irqsave(&enable_mask_lock, flags);

	if (!(enable_mask & ENABLE_C2)) {
		unsigned int cpuid = smp_processor_id();
		int i;
		for_each_online_cpu(i) {
			if (i == cpuid)
				continue;
			smp_send_reschedule(i);
		}
	}

	spin_unlock_irqrestore(&enable_mask_lock, flags);

	return 0;
}

static struct kernel_param_ops enable_mask_param_ops = {
	.set = set_enable_mask,
	.get = param_get_uint,
};

module_param_cb(enable_mask, &enable_mask_param_ops, &enable_mask, 0644);
MODULE_PARM_DESC(enable_mask, "bitmask for C states - C2, C3(LPM)");
#endif /* CONFIG_SEC_PM */

#ifdef CONFIG_SEC_PM_DEBUG
unsigned int log_en;
module_param_named(log_en, log_en, uint, 0644);
#endif /* CONFIG_SEC_PM_DEBUG */

/*
 * Exynos cpuidle driver supports the below idle states
 *
 * IDLE_C1 : WFI(Wait For Interrupt) low-power state
 * IDLE_C2 : Local CPU power gating
 * IDLE_LPM : Low Power Mode, specified by platform
 */
enum idle_state {
	IDLE_C1 = 0,
	IDLE_C2,
	IDLE_LPM,
};

/***************************************************************************
 *                             Helper function                             *
 ***************************************************************************/
static void prepare_idle(unsigned int cpuid)
{
	cpu_pm_enter();
}

static void post_idle(unsigned int cpuid)
{
	cpu_pm_exit();
}

static bool nonboot_cpus_working(void)
{
	return (num_online_cpus() > 1);
}

static int find_available_low_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, unsigned int index)
{
	while (--index > 0) {
		struct cpuidle_state *s = &drv->states[index];
		struct cpuidle_state_usage *su = &dev->states_usage[index];

		if (s->disabled || su->disable)
			continue;
		else
			return index;
	}

	return IDLE_C1;
}

/***************************************************************************
 *                           Cpuidle state handler                         *
 ***************************************************************************/
static int exynos_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	cpuidle_profile_start(dev->cpu, index);

	cpu_do_idle();

	cpuidle_profile_finish(dev->cpu, 0);

	return index;
}

static int exynos_enter_c2(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	int ret, entry_index, sub_state = 0;

#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(log_en & ENABLE_C2))
		pr_info("+++c2\n");
#endif
	prepare_idle(dev->cpu);

	entry_index = enter_c2(dev->cpu, index, &sub_state);

	cpuidle_profile_start(dev->cpu, index | sub_state);

	ret = cpu_suspend(entry_index);
	if (ret)
		flush_tlb_all();

	cpuidle_profile_finish(dev->cpu, ret);

	wakeup_from_c2(dev->cpu);

	post_idle(dev->cpu);

#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(log_en & ENABLE_C2))
		pr_info("---c2\n");
#endif
	return index;
}

static int exynos_enter_lpm(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	int ret, mode, sub_state = 0;;

	mode = determine_lpm();

#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(log_en & ENABLE_C3_LPM))
		pr_info("+++lpm:%d\n", mode);
#endif
	prepare_idle(dev->cpu);

	exynos_prepare_sys_powerdown(mode);

	sub_state = mode << LPM_SUB_STATE_OFFSET;
	cpuidle_profile_start(dev->cpu, index | sub_state);

	ret = cpu_suspend(index);

	cpuidle_profile_finish(dev->cpu, ret);

	exynos_wakeup_sys_powerdown(mode, (bool)ret);

	post_idle(dev->cpu);

#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(log_en & ENABLE_C3_LPM))
		pr_info("---lpm:%d\n", mode);
#endif
	return index;
}

static int exynos_enter_idle_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	int (*func)(struct cpuidle_device *, struct cpuidle_driver *, int);

#ifdef CONFIG_SEC_PM
	switch (index) {
	case IDLE_C2:
		if (unlikely(!(enable_mask & ENABLE_C2)))
			index = IDLE_C1;
		break;
	case IDLE_LPM:
		if (unlikely(!(enable_mask & ENABLE_C3_LPM))) {
			if (enable_mask & ENABLE_C2)
				index = IDLE_C2;
			else
				index = IDLE_C1;
		}
		break;
	default:
		break;
	}
#endif

	switch (index) {
	case IDLE_C1:
		func = exynos_enter_idle;
		break;
	case IDLE_C2:
		func = exynos_enter_c2;
		break;
	case IDLE_LPM:
		/*
		 * In exynos, system can enter LPM when only boot core is running.
		 * In other words, non-boot cores should be shutdown to enter LPM.
		 */
		if (nonboot_cpus_working()) {
			index = find_available_low_state(dev, drv, index);
			return exynos_enter_idle_state(dev, drv, index);
		} else {
			func = exynos_enter_lpm;
		}
		break;
	default:
		pr_err("%s : Invalid index: %d\n", __func__, index);
		return -EINVAL;
	}

	return (*func)(dev, drv, index);
}

/***************************************************************************
 *                            Define notifier call                         *
 ***************************************************************************/
static int exynos_cpuidle_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		cpu_idle_poll_ctrl(true);
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		cpu_idle_poll_ctrl(false);
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

/***************************************************************************
 *                     Association with PSCI and DT                        *
 ***************************************************************************/
typedef int (*suspend_init_fn)(struct cpuidle_driver *,
				struct device_node *[]);

struct cpu_suspend_ops {
        const char *id;
        suspend_init_fn init_fn;
};

static const struct cpu_suspend_ops suspend_operations[] __initconst = {
        {"arm,psci", psci_dt_register_idle_states},
        {}
};

static __init const struct cpu_suspend_ops *get_suspend_ops(const char *str)
{
        int i;

        if (!str)
                return NULL;

        for (i = 0; suspend_operations[i].id; i++)
                if (!strcmp(suspend_operations[i].id, str))
                        return &suspend_operations[i];

        return NULL;
}

/***************************************************************************
 *                         Initialize cpuidle driver                       *
 ***************************************************************************/
#ifndef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
static DEFINE_PER_CPU(struct cpuidle_device, exynos_cpuidle_device);
#endif
static struct cpuidle_driver exynos64_idle_cluster0_driver = {
	.name ="cluster0_driver",
	.owner = THIS_MODULE,
};

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
static struct cpuidle_driver exynos64_idle_cluster1_driver = {
	.name ="cluster1_driver",
	.owner = THIS_MODULE,
};
#endif

static struct device_node *state_nodes[CPUIDLE_STATE_MAX] __initdata;

static int __init exynos_idle_state_init(struct cpuidle_driver *idle_drv,
					 const struct cpumask *mask)
{
	int i, ret;
	const char *entry_method;
	struct device_node *idle_states_node;
	const struct cpu_suspend_ops *suspend_init;
	struct cpuidle_driver *drv = idle_drv;

	idle_states_node = of_find_node_by_path("/cpus/idle-states");
	if (!idle_states_node)
		return -ENOENT;

	if (of_property_read_string(idle_states_node, "entry-method",
				    &entry_method)) {
		pr_warn(" * %s missing entry-method property\n",
			    idle_states_node->full_name);
		of_node_put(idle_states_node);
		return -EOPNOTSUPP;
	}

	suspend_init = get_suspend_ops(entry_method);
	if (!suspend_init) {
		pr_warn("Missing suspend initializer\n");
		of_node_put(idle_states_node);
		return -EOPNOTSUPP;
	}

	drv->cpumask = (struct cpumask *)mask;

	ret = of_init_idle_driver(drv, state_nodes, 0, true);
	if (ret)
		return ret;

	if (suspend_init->init_fn(drv, state_nodes))
		return -EOPNOTSUPP;

	for (i = 0; i < drv->state_count; i++)
		drv->states[i].enter = exynos_enter_idle_state;

	return 0;
}

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
/* TODO: must remove dependency about HMP */
extern struct cpumask hmp_fast_cpu_mask;
extern struct cpumask hmp_slow_cpu_mask;

static int __init exynos_init_cpuidle(void)
{
	int ret;

	ret = exynos_idle_state_init(&exynos64_idle_cluster0_driver, &hmp_fast_cpu_mask);
	if (ret) {
		pr_err("fail exynos_idle_state_init(cluster 0) ret = %d\n", ret);
		return ret;
	}

	cpuidle_profile_state_init(&exynos64_idle_cluster0_driver);

	exynos64_idle_cluster0_driver.safe_state_index = IDLE_C1;
	exynos64_idle_cluster0_driver.cpumask = &hmp_fast_cpu_mask;
	ret = cpuidle_register(&exynos64_idle_cluster0_driver, NULL);

	if (ret) {
		pr_err("fast cpu cpuidle_register fail ret = %d\n", ret);
		return ret;
	}

	ret = exynos_idle_state_init(&exynos64_idle_cluster1_driver, &hmp_slow_cpu_mask);
	if (ret) {
		pr_err("fail exynos_idle_state_init(cluster 1) ret = %d\n", ret);
		return ret;
	}

	exynos64_idle_cluster1_driver.safe_state_index = IDLE_C1;
	exynos64_idle_cluster1_driver.cpumask = &hmp_slow_cpu_mask;
	ret = cpuidle_register(&exynos64_idle_cluster1_driver, NULL);

	if (ret) {
		pr_err("slow cpu cpuidle_register fail ret = %d\n", ret);
		return ret;
	}

	/* TODO : SKIP idle correlation */

	register_pm_notifier(&exynos_cpuidle_notifier);
	register_reboot_notifier(&exynos_cpuidle_reboot_nb);

	pr_info("%s, finish initialization of cpuidle\n", __func__);

	return 0;
}
#else
static int __init exynos_init_cpuidle(void)
{
	int cpuid, ret;
	struct cpuidle_device *device;

	ret = exynos_idle_state_init(&exynos64_idle_cluster0_driver, cpu_online_mask);
	if (ret)
		return ret;

	cpuidle_profile_state_init(&exynos64_idle_cluster0_driver);

	exynos64_idle_cluster0_driver.safe_state_index = IDLE_C1;

	ret = cpuidle_register_driver(&exynos64_idle_cluster0_driver);
	if (ret) {
		pr_err("CPUidle register device failed\n");
		return ret;
	}

	for_each_cpu(cpuid, cpu_online_mask) {
		device = &per_cpu(exynos_cpuidle_device, cpuid);
		device->cpu = cpuid;

		device->state_count = exynos64_idle_cluster0_driver.state_count;

		/* Big core will not change idle time correlation factor */
		if (cpuid & 0x4)
			device->skip_idle_correlation = true;
		else
			device->skip_idle_correlation = false;

		if (cpuidle_register_device(device)) {
			printk(KERN_ERR "CPUidle register device failed\n,");
			return -EIO;
		}
	}

	register_pm_notifier(&exynos_cpuidle_notifier);
	register_reboot_notifier(&exynos_cpuidle_reboot_nb);

	return 0;
}
#endif
device_initcall(exynos_init_cpuidle);
