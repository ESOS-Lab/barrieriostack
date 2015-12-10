/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpuidle.h>

#include <asm/page.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include <mach/cpuidle_profiler.h>

#define to_cluster_id(cpu)	MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1)

static DEFINE_PER_CPU(struct cpuidle_profile_info, profile_info);
static struct cpuidle_profile_info cpd_info[NUM_CLUSTER];

static bool cpuidle_profile_ongoing;
static ktime_t profile_start_time;
static ktime_t profile_finish_time;

static struct cpuidle_profile_info* cpuidle_profile_get_info(unsigned int state,
								unsigned int cpu)
{
	/*
	 * Get profiling information.
	 * In case of cluster power down, it has own information.
	 */
	if (state == CPUIDLE_PROFILE_CPD)
		return &cpd_info[to_cluster_id(cpu)];
	else
		return &per_cpu(profile_info, cpu);
}

static void __cpuidle_profile_start(struct cpuidle_profile_info *info,
					unsigned int state, ktime_t cur_time)
{
	info->entry_count[state]++;
	info->entry_time = cur_time;
}

void cpuidle_profile_start(unsigned int state, unsigned int cpu)
{
	struct cpuidle_profile_info *info;
	ktime_t cur_time;

	if (!cpuidle_profile_ongoing)
		return;

	info = cpuidle_profile_get_info(state, cpu);

	info->cur_state = state;
	info->entered = 1;

	cur_time = ktime_get();
	__cpuidle_profile_start(info, state, cur_time);
}

static void __cpuidle_profile_finish(struct cpuidle_profile_info *info,
					unsigned int state, ktime_t cur_time)
{
	s64 diff;

	if (info->entry_time.tv64 <= 0)
		return;

	diff = ktime_to_us(ktime_sub(cur_time, info->entry_time));

	if (diff >= 0)
		info->residency[state] += diff;
}

void cpuidle_profile_finish(unsigned int state, unsigned int cpu,
				bool early_wakeup)
{
	struct cpuidle_profile_info *info;
	ktime_t cur_time;

	if (!cpuidle_profile_ongoing)
		return;

	info = cpuidle_profile_get_info(state, cpu);

	if (!info->entered)
		return;

	info->entered = 0;

	if (early_wakeup) {
		info->early_wakeup_count[state]++;

		/*
		 * If cpu cannot enter power mode, residency time
		 * should not be updated.
		 */
		return;
	}

	cur_time = ktime_get();
	__cpuidle_profile_finish(info, state, cur_time);
}

static unsigned int calculate_percent(struct cpuidle_profile_info *info,
					unsigned int state)
{
	s64 residency, total_time;

	residency = info->residency[state];
	total_time = ktime_to_us(ktime_sub(profile_finish_time, profile_start_time));

	if (residency) {
		residency = residency * 100;
		do_div(residency, total_time);
	}

	return residency;
}

static void cpuidle_profile_show_result(void)
{
	struct cpuidle_profile_info *info;
	unsigned int cpu, cluster;

	pr_info("==========================================================================\n");
	pr_info("| Total profiling Time = %10lldus                                     |\n",
				ktime_to_us(ktime_sub(profile_finish_time, profile_start_time)));
	pr_info("--------------------------------------------------------------------------\n");
	pr_info("|      | C1 entry |   C1 residency   | C2 entry(early) |   C2 residency   |\n");
	pr_info("--------------------------------------------------------------------------\n");
	for_each_possible_cpu(cpu) {
		info = &per_cpu(profile_info, cpu);
		pr_info("| cpu%u | %8u | %10llu(%3u%%) | %9u(%4u) | %10llu(%3u%%) |\n",
				cpu,
				info->entry_count[CPUIDLE_PROFILE_C1],
				info->residency[CPUIDLE_PROFILE_C1],
				calculate_percent(info, CPUIDLE_PROFILE_C1),
				info->entry_count[CPUIDLE_PROFILE_C2],
				info->early_wakeup_count[CPUIDLE_PROFILE_C2],
				info->residency[CPUIDLE_PROFILE_C2],
				calculate_percent(info, CPUIDLE_PROFILE_C2));
	}
	pr_info("==========================================================================\n");
	pr_info("|        | Cluster power down |                                         |\n");
	pr_info("--------------------------------------------------------------------------\n");
	for (cluster = 0; cluster < NUM_CLUSTER; cluster++) {
		info = &cpd_info[cluster];
		pr_info("| %6s | %12llu(%3u%%) |                                         |\n",
				cluster ? "LITTLE" : "BIG",
				info->residency[CPUIDLE_PROFILE_CPD],
				calculate_percent(info, CPUIDLE_PROFILE_CPD));
	}
	pr_info("==========================================================================\n");
}

static void cpuidle_profile_clear_single(struct cpuidle_profile_info *info)
{
	unsigned int state;

	for (state = 0; state < NUM_STATE; state++) {
		info->entry_count[state] = 0;
		info->early_wakeup_count[state] = 0;
		info->residency[state] = 0;
	}

	info->entered = 0;
	info->entry_time.tv64 = 0;
}

static void cpuidle_profile_single_start(void *p)
{
	unsigned int cpu = smp_processor_id();
	struct cpuidle_profile_info *info = &per_cpu(profile_info, cpu);

	cpuidle_profile_clear_single(info);

	/* The first cpu in cluster clear own cluster data */
	if (cpumask_first(cpu_coregroup_mask(cpu))) {
		info = &cpd_info[to_cluster_id(cpu)];
		cpuidle_profile_clear_single(info);
	}
}

static void cpuidle_profile_single_finish(void *p)
{
	/* Nothing to do */
	return;
}

static void cpuidle_profile_main_start(void)
{
	if (cpuidle_profile_ongoing) {
		pr_err("CPUIDLE profile is ongoing\n");
		return;
	}

	pr_info("CPUIDLE profile start\n");

	/* Save profile starting time */
	profile_start_time = ktime_get();

	/* Set flag which indicate that profile is ongoing */
	cpuidle_profile_ongoing = 1;

	/* Wakeup all cpus and clear own profile data to start profile */
	preempt_disable();
	cpuidle_profile_clear_single(&per_cpu(profile_info, smp_processor_id()));
	smp_call_function(cpuidle_profile_single_start, NULL, 1);
	preempt_enable();
}

static void cpuidle_profile_main_finish(void)
{
	if (!cpuidle_profile_ongoing) {
		pr_err("CPUIDLE profile does not start yet\n");
		return;
	}

	pr_info("CPUIDLE profile finish\n");

	/* Wakeup all cpus and update own profile data to finish profile */
	preempt_disable();
	smp_call_function(cpuidle_profile_single_finish, NULL, 1);
	preempt_enable();

	/* Clear flag which indicate that profile is ongoing */
	cpuidle_profile_ongoing = 0;

	/* Get profile finish time to calculate profile time */
	profile_finish_time = ktime_get();

	/* Show and clear profile result */
	cpuidle_profile_show_result();

	/* Clear starting and finish time */
	profile_start_time.tv64 = 0;
	profile_finish_time.tv64 = 0;
}

/*********************************************************************
 *                          SYSFS INTERFACE                          *
 *********************************************************************/
static ssize_t show_cpuidle_profile(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;

	if (cpuidle_profile_ongoing)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profile is ongoing\n");
	else
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"echo <1/0> > profile\n");

	return ret;
}

static ssize_t store_cpuidle_profile(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	switch (input) {
	case 1:
		cpuidle_profile_main_start();
		break;
	case 0:
		cpuidle_profile_main_finish();
		break;
	default:
		pr_info("echo <1/0> > profile\n");
		break;
	}

	return count;
}

static struct kobj_attribute cpuidle_profile_attr =
	__ATTR(profile, 0644, show_cpuidle_profile, store_cpuidle_profile);

static struct attribute *cpuidle_profile_attrs[] = {
	&cpuidle_profile_attr.attr,
	NULL,
};

static const struct attribute_group cpuidle_profile_group = {
	.attrs = cpuidle_profile_attrs,
};


/*********************************************************************
 *                   Initialize cpuidle profiler                     *
 *********************************************************************/

static int __init cpuidle_profile_init(void)
{
	struct class *class;
	struct device *dev;
	int ret;

	class = class_create(THIS_MODULE, "cpuidle");
	dev = device_create(class, NULL, 0, NULL, "cpuidle_profiler");
	ret = sysfs_create_group(&dev->kobj, &cpuidle_profile_group);
	if (ret) {
		pr_err("CPUIDLE Profiler : error to create sysfs\n");
		return -EINVAL;
	}

	return 0;
}
late_initcall(cpuidle_profile_init);
