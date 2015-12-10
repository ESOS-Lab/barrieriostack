/*
 * sec_slow.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/kernel_stat.h>
#include <linux/sec_sysfs.h>
#include <asm/cputime.h>

enum {
	TIMER_NONE,
	TIMER_START,
	TIMER_STOP,
	TIMER_CONT
};

struct observer_loadinfo {
	struct timer_list load_timer;
	struct rw_semaphore enable_sem;
	int observer_enabled;
};

static struct observer_loadinfo loadinfo;

struct observer_cpuinfo {
	spinlock_t load_lock;
	u64 time_in_idle;
	u64 time_in_idle_timestamp;
	u64 cputime_speedadj;
	u64 cputime_speedadj_timestamp;
	unsigned int cpuload;
	u32 freq_cur;
	u32 cpufreq_min;
	u32 cpufreq_max;
	bool online;
};

static DEFINE_PER_CPU(struct observer_cpuinfo, slow_cpuinfo);

#define CPU_REPRESENTIVE 0

#define DEFAULT_TIMER_RATE (40 * USEC_PER_MSEC)
#define DEFAULT_TARGET_LOAD 90
#define DEFAULT_FULL_ENTER_TIME (4 * DEFAULT_TIMER_RATE)
#define DEFAULT_FULL_EXIT_TIME (2 * DEFAULT_TIMER_RATE)
#define DEFAULT_MAX_FAST_LOAD	10
#define DEFAULT_MIN_FAST_LOAD	50

#define SLOW_MODE_BOOST 2
#define SLOW_MODE_FULL 1
#define SLOW_MODE_NORMAL 0
#define SLOW_MODE_CPU 0
#define FAST_MODE_CPU 4

extern struct cpumask hmp_fast_cpu_mask;
extern struct cpumask hmp_slow_cpu_mask;
static bool slow_hmp_boost;
static bool slow_hmp_semi_boost;

struct slow_mode_tunables {
	/*
	 * The sample rate of the timer used to check cpu load.
	 */
	unsigned long timer_rate;
	bool io_is_busy;

	spinlock_t slow_mode_lock;
	unsigned int slow_mode;
	unsigned int enforced_slow_mode;
	u64 slow_mode_check_timestamp;

	unsigned long full_enter_time;
	unsigned long time_in_full_enter;
	unsigned int full_enter_load;
	unsigned int full_enter_load_fast;

	unsigned long full_exit_time;
	unsigned long time_in_full_exit;
	unsigned int full_exit_load;
	unsigned int full_exit_load_fast;
};
static struct slow_mode_tunables slow_tunables;

static struct device *sec_slow;

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = cputime_to_usecs(cur_wall_time);

	return cputime_to_usecs(idle_time);
}

static u64 get_cpu_idle_time(unsigned int cpu, u64 *wall, int io_busy)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, io_busy ? wall : NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else if (!io_busy)
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static void observer_load_start(struct observer_cpuinfo *pcpu, int cpu, bool io_busy)
{
	pcpu->time_in_idle =
		get_cpu_idle_time(cpu, &pcpu->time_in_idle_timestamp,
				  io_busy);
	pcpu->cputime_speedadj = 0;
	pcpu->cputime_speedadj_timestamp = pcpu->time_in_idle_timestamp;
}

static void observer_timer_resched(void)
{
	int cpu;
	struct slow_mode_tunables *tunables = &slow_tunables;
	unsigned long expires;

	for_each_cpu(cpu, cpu_possible_mask) {
		struct observer_cpuinfo *pcpu = &per_cpu(slow_cpuinfo, cpu);

		spin_lock_bh(&pcpu->load_lock);
		observer_load_start(pcpu, cpu, tunables->io_is_busy);
		spin_unlock_bh(&pcpu->load_lock);
	}
	expires = jiffies + usecs_to_jiffies(tunables->timer_rate);
	mod_timer(&loadinfo.load_timer, expires);
}

static void observer_timer_start(struct slow_mode_tunables *tunables)
{
	int cpu;
	unsigned long expires = jiffies +
		usecs_to_jiffies(tunables->timer_rate);

	loadinfo.load_timer.expires = expires;
	add_timer(&loadinfo.load_timer);

	for_each_cpu(cpu, cpu_possible_mask) {
		struct observer_cpuinfo *pcpu = &per_cpu(slow_cpuinfo, cpu);

		spin_lock_bh(&pcpu->load_lock);
		observer_load_start(pcpu, cpu, tunables->io_is_busy);
		spin_unlock_bh(&pcpu->load_lock);
	}
}

static u64 update_load(int cpu)
{
	struct observer_cpuinfo *pcpu = &per_cpu(slow_cpuinfo, cpu);
	struct slow_mode_tunables *tunables = &slow_tunables;

	u64 now;
	u64 now_idle;
	unsigned int delta_idle;
	unsigned int delta_time;
	u64 active_time;
	now_idle = get_cpu_idle_time(cpu, &now, tunables->io_is_busy);
	delta_idle = (unsigned int)(now_idle - pcpu->time_in_idle);
	delta_time = (unsigned int)(now - pcpu->time_in_idle_timestamp);

	if (!pcpu->online || delta_time <= delta_idle)
		active_time = 0;
	else
		active_time = delta_time - delta_idle;

	pcpu->cputime_speedadj += active_time * pcpu->freq_cur;

	pcpu->time_in_idle = now_idle;
	pcpu->time_in_idle_timestamp = now;

	pr_debug("%s: c%d: active:%lld, delta:%d, curfreq:%d\n", __func__,
			cpu, active_time, delta_time, pcpu->freq_cur);

	return now;
}

static int calculate_load(int cpu)
{
	struct observer_cpuinfo *pcpu = &per_cpu(slow_cpuinfo, cpu);
	u64 now;
	unsigned int delta_time;
	unsigned int loadadjfreq;
	u64 cputime_speedadj;

	if (!pcpu->cpufreq_max)
		return -EINVAL;

	if (pcpu->online) {
		spin_lock_bh(&pcpu->load_lock);
		now = update_load(cpu);
		delta_time = (unsigned int)(now - pcpu->cputime_speedadj_timestamp);
		cputime_speedadj = pcpu->cputime_speedadj;
		spin_unlock_bh(&pcpu->load_lock);

		if (WARN_ON_ONCE(!delta_time))
			return -1;

		do_div(cputime_speedadj, delta_time);
		loadadjfreq = (unsigned int)cputime_speedadj * 100;
		pcpu->cpuload = loadadjfreq / pcpu->cpufreq_max;

		pr_debug("%s: c%d: load:%d|t:%d, freq:%d/%d\n", __func__,
				cpu, pcpu->cpuload, delta_time, pcpu->freq_cur, pcpu->cpufreq_max);
	} else {
		pcpu->cpuload = 0;

		pr_debug("%s: c%d: load:%d, freq:%d/%d\n", __func__,
				cpu, pcpu->cpuload, pcpu->freq_cur, pcpu->cpufreq_max);
	}

	return pcpu->cpuload;
}

static unsigned int check_slow_mode(int cpu, unsigned int cur_mode, u64 now)
{
	int i;
	unsigned int ret = cur_mode;
	unsigned int total_slow_load = 0;
	unsigned int total_fast_load = 0;
	struct slow_mode_tunables *tunables = &slow_tunables;

	if (now - tunables->slow_mode_check_timestamp < tunables->timer_rate - USEC_PER_MSEC)
		return ret;

	if (now - tunables->slow_mode_check_timestamp > tunables->timer_rate + USEC_PER_MSEC)
		tunables->slow_mode_check_timestamp = now - tunables->timer_rate;

	for_each_cpu_mask(i, hmp_slow_cpu_mask) {
		struct observer_cpuinfo *picpu = &per_cpu(slow_cpuinfo, i);
		total_slow_load += picpu->cpuload;
	}
	for_each_cpu_mask(i, hmp_fast_cpu_mask) {
		struct observer_cpuinfo *picpu = &per_cpu(slow_cpuinfo, i);
		total_fast_load += picpu->cpuload;
	}

	pr_debug("check slow:%d, fast:%d; mode:%d, enter:%ld, exit:%ld\n",
			total_slow_load, total_fast_load, cur_mode,
			tunables->time_in_full_enter, tunables->time_in_full_exit);

	switch (cur_mode) {
	case SLOW_MODE_NORMAL:
		if (total_slow_load >= tunables->full_enter_load
			&& total_fast_load < tunables->full_enter_load_fast)
			tunables->time_in_full_enter += now - tunables->slow_mode_check_timestamp;
		else
			tunables->time_in_full_enter = 0;

		if (tunables->time_in_full_enter >= tunables->full_enter_time)
			ret = SLOW_MODE_FULL;
		break;

	case SLOW_MODE_FULL:
		if (total_slow_load >= tunables->full_enter_load
			&& total_fast_load < tunables->full_enter_load_fast)
			tunables->time_in_full_enter += now - tunables->slow_mode_check_timestamp;
		else
			tunables->time_in_full_enter = 0;

		if (total_slow_load < tunables->full_exit_load
			|| total_fast_load >= tunables->full_exit_load_fast)
			tunables->time_in_full_exit += now - tunables->slow_mode_check_timestamp;
		else
			tunables->time_in_full_exit = 0;

		if (tunables->time_in_full_exit >= tunables->full_exit_time)
			ret = SLOW_MODE_NORMAL;
		else if (tunables->time_in_full_enter >= tunables->full_enter_time)
			ret = SLOW_MODE_BOOST;
		break;

	case SLOW_MODE_BOOST:
		if (total_slow_load < tunables->full_enter_load
			|| total_fast_load >= tunables->full_enter_load_fast)
			ret = SLOW_MODE_FULL;
		break;

	default:
		break;
	}

	pr_debug("check ret:%d, enter:%ld, exit:%ld\n",
			ret, tunables->time_in_full_enter, tunables->time_in_full_exit);

	if (tunables->time_in_full_enter >= tunables->full_enter_time
		|| tunables->time_in_full_exit >= tunables->full_exit_time) {
		tunables->time_in_full_enter = 0;
		tunables->time_in_full_exit = 0;
	}
	tunables->slow_mode_check_timestamp = now;

	return ret;
}

static void enter_slow_mode(struct slow_mode_tunables *tunables)
{
	if (tunables->slow_mode == SLOW_MODE_BOOST) {
		if (!slow_hmp_boost) {
			pr_debug("%s (slow) mp boost on\n", __func__);
			set_hmp_boost(1);
			slow_hmp_boost = true;
		}
	} else {
		if (slow_hmp_boost) {
			pr_debug("%s (slow) mp boost off\n", __func__);
			set_hmp_boost(0);
			slow_hmp_boost = false;
		}
	}

	if (!slow_hmp_semi_boost) {
		pr_debug("%s (slow) mp semi boost on\n", __func__);
		set_hmp_semiboost(1);
		slow_hmp_semi_boost = true;
	}
}

static void exit_slow_mode(struct slow_mode_tunables *tunables)
{
	if (slow_hmp_semi_boost) {
		pr_debug("%s (slow) mp semi boost off\n", __func__);
		set_hmp_semiboost(0);
		slow_hmp_semi_boost = false;
	}
}

static void observer_timer(unsigned long data)
{
	int cpu;
	u64 now;
	struct slow_mode_tunables *tunables = &slow_tunables;
	struct observer_cpuinfo *pcpu_representive = &per_cpu(slow_cpuinfo, CPU_REPRESENTIVE);
	unsigned int new_slow_mode;

	if (!down_read_trylock(&loadinfo.enable_sem))
		return;

	/* calculate all cpu load */
	for_each_cpu(cpu, cpu_possible_mask) {
		if (calculate_load(cpu) < 0)
			goto rearm;
	}
	now = pcpu_representive->time_in_idle_timestamp;

	/* check slow mode */
	spin_lock_bh(&tunables->slow_mode_lock);
	if (tunables->enforced_slow_mode)
		new_slow_mode = tunables->enforced_slow_mode;
	else
		new_slow_mode = check_slow_mode(data, tunables->slow_mode, now);

	if (new_slow_mode != tunables->slow_mode) {
		tunables->slow_mode = new_slow_mode;
		if (new_slow_mode == SLOW_MODE_NORMAL)
			exit_slow_mode(tunables);
		else
			enter_slow_mode(tunables);
	}
	spin_unlock_bh(&tunables->slow_mode_lock);

	/*trace_cpufreq_interactive_target(data, cpu_load, pcpu->target_freq,
					 pcpu->policy->cur, new_freq);*/

	/* rearm if max */
	if (new_slow_mode == SLOW_MODE_NORMAL
			&& !loadinfo.observer_enabled)
		goto exit;

rearm:
	if (!timer_pending(&loadinfo.load_timer))
		observer_timer_resched();

exit:
	up_read(&loadinfo.enable_sem);
	return;
}

static ssize_t show_timer_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%lu\n", tunables->timer_rate);
}

static ssize_t store_timer_rate(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	unsigned long val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->timer_rate = val;
	return count;
}

static ssize_t show_io_is_busy(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%u\n", tunables->io_is_busy);
}

static ssize_t store_io_is_busy(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	unsigned long val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->io_is_busy = val;
	return count;
}

static ssize_t show_slow_mode(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%u\n", tunables->slow_mode);
}

static ssize_t store_slow_mode(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	unsigned int val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	val &= SLOW_MODE_FULL;
	tunables->slow_mode = val;
	return count;
}

static ssize_t show_enforced_slow_mode(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%u\n", tunables->enforced_slow_mode);
}

static ssize_t store_enforced_slow_mode(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	unsigned int val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	val &= SLOW_MODE_FULL;
	tunables->enforced_slow_mode = val;
	return count;
}

static ssize_t show_full_enter_load(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%u\n", tunables->full_enter_load);
}

static ssize_t store_full_enter_load(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	unsigned int val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->full_enter_load = val;
	return count;
}

static ssize_t show_full_enter_load_fast(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%u\n", tunables->full_enter_load_fast);
}

static ssize_t store_full_enter_load_fast(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	unsigned int val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->full_enter_load_fast = val;
	return count;
}

static ssize_t show_full_exit_load(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%u\n", tunables->full_exit_load);
}

static ssize_t store_full_exit_load(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	unsigned int val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->full_exit_load = val;
	return count;
}

static ssize_t show_full_exit_load_fast(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%u\n", tunables->full_exit_load_fast);
}

static ssize_t store_full_exit_load_fast(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	unsigned int val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->full_exit_load_fast = val;
	return count;
}

static ssize_t show_full_enter_time(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%lu\n", tunables->full_enter_time);
}

static ssize_t store_full_enter_time(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	long unsigned int val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->full_enter_time = val;
	return count;
}

static ssize_t show_full_exit_time(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct slow_mode_tunables *tunables = &slow_tunables;

	return sprintf(buf, "%lu\n", tunables->full_exit_time);
}

static ssize_t store_full_exit_time(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int ret;
	long unsigned int val;
	struct slow_mode_tunables *tunables = &slow_tunables;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->full_exit_time = val;
	return count;
}


#define slow_attr_rw(_name)					\
	DEVICE_ATTR(_name, 0660, show_##_name, store_##_name)

slow_attr_rw(timer_rate);
slow_attr_rw(io_is_busy);
slow_attr_rw(slow_mode);
slow_attr_rw(enforced_slow_mode);
slow_attr_rw(full_enter_load);
slow_attr_rw(full_enter_load_fast);
slow_attr_rw(full_exit_load);
slow_attr_rw(full_exit_load_fast);
slow_attr_rw(full_enter_time);
slow_attr_rw(full_exit_time);

static struct attribute *sec_slow_attrs[] = {
	&dev_attr_timer_rate.attr,
	&dev_attr_io_is_busy.attr,
	&dev_attr_slow_mode.attr,
	&dev_attr_enforced_slow_mode.attr,
	&dev_attr_full_enter_load.attr,
	&dev_attr_full_enter_load_fast.attr,
	&dev_attr_full_exit_load.attr,
	&dev_attr_full_exit_load_fast.attr,
	&dev_attr_full_enter_time.attr,
	&dev_attr_full_exit_time.attr,
	NULL,
};

static struct attribute_group sec_slow_attr_group = {
	.attrs = sec_slow_attrs,
};

static int observer_timer_condition(struct cpufreq_freqs *freq)
{
	struct observer_cpuinfo *slow_pcpu = &per_cpu(slow_cpuinfo, SLOW_MODE_CPU);
	struct observer_cpuinfo *fast_pcpu = &per_cpu(slow_cpuinfo, FAST_MODE_CPU);
	u32 slow_freq, fast_freq;
	u32 slow_max, fast_max;

	/* for slow mode */
	if (freq->cpu == SLOW_MODE_CPU) {
		slow_freq = freq->new;
		fast_freq = fast_pcpu->freq_cur;
	} else if (freq->cpu == FAST_MODE_CPU) {
		slow_freq = slow_pcpu->freq_cur;
		fast_freq = freq->new;
	} else {
		return TIMER_CONT;
	}

	slow_max = slow_pcpu->cpufreq_max;
	fast_max = fast_pcpu->cpufreq_min;

	if (slow_freq == slow_max && fast_freq <= fast_max && !loadinfo.observer_enabled)
		return TIMER_START;
	else if (loadinfo.observer_enabled && (slow_freq < slow_max || fast_freq > fast_max))
		return TIMER_STOP;

	return TIMER_CONT;
}

static int cpufreq_observer_notifier(
	struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct observer_cpuinfo *pcpu = &per_cpu(slow_cpuinfo, freq->cpu);
	int timer_condition;

	if (val == CPUFREQ_POSTCHANGE) {
		pr_debug("%s: c%d: freq:%d->%d\n", __func__, freq->cpu, freq->old, freq->new);

		if (!down_read_trylock(&loadinfo.enable_sem))
			return 0;

		timer_condition = observer_timer_condition(freq);

		up_read(&loadinfo.enable_sem);

		if (timer_condition == TIMER_START) {
			down_write(&loadinfo.enable_sem);
			if (!loadinfo.observer_enabled) {
				del_timer_sync(&loadinfo.load_timer);
				observer_timer_start(&slow_tunables);
				loadinfo.observer_enabled = 1;
			}

			pcpu->freq_cur = freq->new;
			up_write(&loadinfo.enable_sem);

		} else if (timer_condition == TIMER_STOP) {
			down_write(&loadinfo.enable_sem);
			if (loadinfo.observer_enabled)
				loadinfo.observer_enabled = 0;

			pcpu->freq_cur = freq->new;
			up_write(&loadinfo.enable_sem);

		} else if (timer_condition == TIMER_CONT) {
			/* update_load */
			if (!down_read_trylock(&loadinfo.enable_sem))
				return 0;

			if (loadinfo.observer_enabled) {
				spin_lock_bh(&pcpu->load_lock);
				update_load(freq->cpu);
				spin_unlock_bh(&pcpu->load_lock);
			}

			pcpu->freq_cur = freq->new;
			up_read(&loadinfo.enable_sem);

		} else {
			pcpu->freq_cur = freq->new;
		}

	}
	return 0;
}

static int cpufreq_policy_observer_notifier(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	int cpu;

	pr_debug("%s: event:%ld, policy:%p(%s), cpus:%lx min:%d, max:%d cur:%d\n",
			__func__, event, policy, policy->governor->name,
			policy->cpus->bits[0], policy->cpuinfo.min_freq,
			policy->cpuinfo.max_freq, policy->cur);

	if (event != CPUFREQ_INCOMPATIBLE)
		return 0;

	/* Make sure that all CPUs impacted by this policy are
	 * updated since we will only get a notification when the
	 * user explicitly changes the policy on a CPU.
	 */
	for_each_cpu(cpu, policy->cpus) {
		struct observer_cpuinfo *pcpu = &per_cpu(slow_cpuinfo, cpu);
		pcpu->cpufreq_min = policy->cpuinfo.min_freq;
		pcpu->cpufreq_max = policy->cpuinfo.max_freq;
	}

	return 0;
}

static int cpu_observer_notifier(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct observer_cpuinfo *pcpu = &per_cpu(slow_cpuinfo, cpu);
	struct slow_mode_tunables *tunables = &slow_tunables;

	switch (action) {
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		pr_debug("%s: c%u down\n", __func__, cpu);

		pcpu->online = false;
		return NOTIFY_OK;

	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		pr_debug("%s: c%u up\n", __func__, cpu);

		spin_lock_bh(&pcpu->load_lock);
		observer_load_start(pcpu, cpu, tunables->io_is_busy);
		spin_unlock_bh(&pcpu->load_lock);
		pcpu->freq_cur = cpufreq_quick_get(cpu);
		pcpu->online = true;
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static struct notifier_block cpufreq_notifier_block = {
	.notifier_call = cpufreq_observer_notifier,
};

static struct notifier_block cpufreq_policy_notifier_block = {
	.notifier_call  = cpufreq_policy_observer_notifier,
};

static struct notifier_block cpufreq_cpu_notifier_block = {
	.notifier_call = cpu_observer_notifier,
	.priority = -1,		/* low priority for getting cpufreq in a notifier */
};

static void __init slow_mode_init(void)
{
	slow_tunables.full_enter_time = DEFAULT_FULL_ENTER_TIME;
	slow_tunables.full_enter_load = 4 * DEFAULT_TARGET_LOAD;
	slow_tunables.full_enter_load_fast = 4 * DEFAULT_MAX_FAST_LOAD;
	slow_tunables.full_exit_time = DEFAULT_FULL_EXIT_TIME;
	slow_tunables.full_exit_load = 4 * DEFAULT_TARGET_LOAD;
	slow_tunables.full_exit_load_fast = 4 * DEFAULT_MIN_FAST_LOAD;
	spin_lock_init(&slow_tunables.slow_mode_lock);
}

static int __init observer_init(void)
{
	int ret = 0;
	unsigned int i;
	struct observer_cpuinfo *pcpu;

	init_timer_deferrable(&loadinfo.load_timer);
	loadinfo.load_timer.function = observer_timer;
	init_rwsem(&loadinfo.enable_sem);
	for_each_possible_cpu(i) {
		pcpu = &per_cpu(slow_cpuinfo, i);
		pcpu->online = true;
		spin_lock_init(&pcpu->load_lock);
	}

	slow_tunables.timer_rate = DEFAULT_TIMER_RATE;
	slow_tunables.io_is_busy = false;

	slow_mode_init();

	sec_slow = sec_device_create(NULL, "sec_slow");
	if (IS_ERR(sec_slow)) {
		pr_err("%s: failed to create sec_slow: %d\n", __func__, ret);
		ret = PTR_ERR(sec_slow);
		goto err_sec_device;
	}

	ret = sysfs_create_group(&sec_slow->kobj, &sec_slow_attr_group);
	if (ret) {
		pr_err("%s: unable to create group, error: %d\n", __func__, ret);
		goto err_sysfs;
	}

	pr_info("observer: registering notifiers\n");
	ret = cpufreq_register_notifier(&cpufreq_policy_notifier_block,
			CPUFREQ_POLICY_NOTIFIER);
	if (ret) {
		pr_err("%s: failed to register CPUFREQ_POLICY_NOTIFIER: %d\n", __func__, ret);
		goto err_policy;
	}

	ret = cpufreq_register_notifier(&cpufreq_notifier_block,
			CPUFREQ_TRANSITION_NOTIFIER);
	if (ret) {
		pr_err("%s: failed to register CPUFREQ_TRANSITION_NOTIFIER: %d\n", __func__, ret);
		goto err_cpufreq;
	}

	ret = register_hotcpu_notifier(&cpufreq_cpu_notifier_block);
	if (ret) {
		pr_err("%s: failed to register hotplug notifier: %d\n", __func__, ret);
		goto err_cpu;
	}

	return ret;

err_cpu:
	cpufreq_unregister_notifier(&cpufreq_notifier_block,
			CPUFREQ_TRANSITION_NOTIFIER);
err_cpufreq:
	cpufreq_unregister_notifier(&cpufreq_policy_notifier_block,
			CPUFREQ_POLICY_NOTIFIER);
err_policy:
	sysfs_remove_group(&sec_slow->kobj, &sec_slow_attr_group);
err_sysfs:
	sec_device_destroy(sec_slow->devt);
err_sec_device:
	return ret;
}

subsys_initcall(observer_init);
