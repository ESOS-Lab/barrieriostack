/*
 * Exynos-SnapShot for Samsung's SoC's.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS_SNAPSHOT_H
#define EXYNOS_SNAPSHOT_H

#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/device.h>
#include <plat/map-base.h>
#include <mach/exynos-ss-soc.h>

struct worker;
struct work_struct;

/**
 * esslog_flag - added log information supported.
 * @ESS_FLAG_IN: Generally, marking into the function
 * @ESS_FLAG_ON: Generally, marking the status not in, not out
 * @ESS_FLAG_OUT: Generally, marking come out the function
 * @ESS_FLAG_SOFTIRQ: Marking to pass the softirq function
 * @ESS_FLAG_SOFTIRQ_HI_TASKLET: Marking to pass the tasklet function
 * @ESS_FLAG_SOFTIRQ_TASKLET: Marking to pass the tasklet function
 */
enum esslog_flag {
	ESS_FLAG_IN = 1,
	ESS_FLAG_ON = 2,
	ESS_FLAG_OUT = 3,
	ESS_FLAG_SOFTIRQ = 100,
	ESS_FLAG_SOFTIRQ_HI_TASKLET,
	ESS_FLAG_SOFTIRQ_TASKLET
};

#ifdef CONFIG_EXYNOS_SNAPSHOT
extern void __exynos_ss_task(int cpu, struct task_struct *task);
extern void __exynos_ss_work(struct worker *worker, struct work_struct *work,
				work_func_t f, int en);
extern void __exynos_ss_clockevent(unsigned long long clc, int64_t delta, ktime_t next_event);
extern void __exynos_ss_cpuidle(int index, unsigned state, int diff, int en);
extern void __exynos_ss_suspend(void *fn, struct device *dev, int en);
extern void __exynos_ss_irq(unsigned int irq, void *fn, int irqs_disabled, int en);
extern void __exynos_ss_irq_exit(unsigned int irq, unsigned long long start_time);
extern void __exynos_ss_hrtimer(struct hrtimer *timer, s64 *now,
				enum hrtimer_restart (*fn) (struct hrtimer *),
				int en);
extern void __exynos_ss_reg(unsigned int read, size_t val, size_t reg, int en);
extern void exynos_ss_printk(const char *fmt, ...);
extern void exynos_ss_printkl(size_t msg, size_t val);
extern int exynos_ss_set_enable(const char *name, int en);
extern int exynos_ss_get_enable(const char *name);
extern int exynos_ss_save_context(struct pt_regs *);
extern int exynos_ss_save_reg(struct pt_regs *);
extern int exynos_ss_early_dump(void);

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
extern void arch_local_irq_restore(unsigned long flags);
extern unsigned long arch_local_irq_save(void);
extern void arch_local_irq_enable(void);
extern void arch_local_irq_disable(void);
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
#define exynos_ss_irq_exit_var(v)	do {	v = cpu_clock(raw_smp_processor_id());	\
					} while(0)
#else
#define exynos_ss_irq_exit_var(v)	do {	v = 0; \
					} while(0)
#endif

static inline void exynos_ss_task(int cpu, struct task_struct *task)
{
	__exynos_ss_task(cpu, task);
}

static inline void exynos_ss_work(struct worker *worker, struct work_struct *work,
					work_func_t f, int en)
{
	__exynos_ss_work(worker, work, f, en);
}

static inline void exynos_ss_clockevent(unsigned long long clc, int64_t delta, ktime_t next_event)
{
	__exynos_ss_clockevent(clc, delta, next_event);
}

static inline void exynos_ss_cpuidle(int index, unsigned state, int diff, int en)
{
	__exynos_ss_cpuidle(index, state, diff, en);
}

static inline void exynos_ss_suspend(void *fn, struct device *dev, int en)
{
	__exynos_ss_suspend(fn, dev, en);
}

static inline void exynos_ss_irq(unsigned int irq, void *fn, int irqs_disabled, int en)
{
	__exynos_ss_irq(irq, fn, irqs_disabled, en);
}

static inline void exynos_ss_irq_exit(unsigned int irq, unsigned long long start_time)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	__exynos_ss_irq_exit(irq, start_time);
#endif
}

static inline void exynos_ss_reg(unsigned int read, size_t val, size_t reg, int en)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	__exynos_ss_reg(read, val, reg, en);
#endif
}

static inline void exynos_ss_hrtimer(struct hrtimer *timer, s64 *now,
			 enum hrtimer_restart (*fn) (struct hrtimer *), int en)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
	__exynos_ss_hrtimer(timer, now, fn, en);
#endif
}

static inline void exynos_ss_softirq(unsigned int irq, void *fn, int irqs_disabled, int en)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_SOFTIRQ
	__exynos_ss_irq(irq, fn, irqs_disabled, en);
#endif
}

#else
#define exynos_ss_task(a,b)		do { } while(0)
#define exynos_ss_work(a,b,c,d)		do { } while(0)
#define exynos_ss_clockevent(a,b,c)	do { } while(0)
#define exynos_ss_cpuidle(a,b,c,d)	do { } while(0)
#define exynos_ss_suspend(a,b,c)	do { } while(0)
#define exynos_ss_irq(a,b,c,d)		do { } while(0)
#define exynos_ss_irq_exit(a,b)		do { } while(0)
#define exynos_ss_irq_exit_var(v)	do { v = 0; } while(0)
#define exynos_ss_reg(a,b,c,d)		do { } while(0)
#define exynos_ss_hrtimer(a,b,c,d)	do { } while(0)
#define exynos_ss_softirq(a,b,c,d)	do { } while(0)
#define exynos_ss_printk(...)		do { } while(0)
#define exynos_ss_printkl(a,b)		do { } while(0)
#define exynos_ss_save_context(a)	do { } while(0)
#define exynos_ss_set_enable(a,b)	do { } while(0)
#define exynos_ss_early_dump()		do { } while(0)
#endif /* CONFIG_EXYNOS_SNAPSHOT */

#endif
