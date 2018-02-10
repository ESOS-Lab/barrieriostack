/* linux/arch/arm/mach-exynos4/mct.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4 MCT(Multi-Core Timer) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clocksource.h>
#include <linux/sched_clock.h>

#include <asm/localtimer.h>

#include <plat/cpu.h>

#define EXYNOS4_MCTREG(x)		(x)
#define EXYNOS4_MCT_G_CNT_L		EXYNOS4_MCTREG(0x100)
#define EXYNOS4_MCT_G_CNT_U		EXYNOS4_MCTREG(0x104)
#define EXYNOS4_MCT_G_CNT_WSTAT		EXYNOS4_MCTREG(0x110)
#define EXYNOS4_MCT_G_COMP0_L		EXYNOS4_MCTREG(0x200)
#define EXYNOS4_MCT_G_COMP0_U		EXYNOS4_MCTREG(0x204)
#define EXYNOS4_MCT_G_COMP0_ADD_INCR	EXYNOS4_MCTREG(0x208)
#define EXYNOS4_MCT_G_TCON		EXYNOS4_MCTREG(0x240)
#define EXYNOS4_MCT_G_INT_CSTAT		EXYNOS4_MCTREG(0x244)
#define EXYNOS4_MCT_G_INT_ENB		EXYNOS4_MCTREG(0x248)
#define EXYNOS4_MCT_G_WSTAT		EXYNOS4_MCTREG(0x24C)
#define _EXYNOS4_MCT_L_BASE		EXYNOS4_MCTREG(0x300)
#define EXYNOS4_MCT_L_BASE(x)		(_EXYNOS4_MCT_L_BASE + (0x100 * x))
#define EXYNOS4_MCT_L_MASK		(0xffffff00)

#define MCT_L_TCNTB_OFFSET		(0x00)
#define MCT_L_ICNTB_OFFSET		(0x08)
#define MCT_L_TCON_OFFSET		(0x20)
#define MCT_L_INT_CSTAT_OFFSET		(0x30)
#define MCT_L_INT_ENB_OFFSET		(0x34)
#define MCT_L_WSTAT_OFFSET		(0x40)
#define MCT_G_TCON_START		(1 << 8)
#define MCT_G_TCON_COMP0_AUTO_INC	(1 << 1)
#define MCT_G_TCON_COMP0_ENABLE		(1 << 0)
#define MCT_L_TCON_INTERVAL_MODE	(1 << 2)
#define MCT_L_TCON_INT_START		(1 << 1)
#define MCT_L_TCON_TIMER_START		(1 << 0)

enum {
	MCT_INT_SPI,
	MCT_INT_PPI
};

enum {
	MCT_G0_IRQ,
	MCT_G1_IRQ,
	MCT_G2_IRQ,
	MCT_G3_IRQ,
	MCT_L0_IRQ,
	MCT_L1_IRQ,
	MCT_L2_IRQ,
	MCT_L3_IRQ,
	MCT_L4_IRQ,
	MCT_L5_IRQ,
	MCT_L6_IRQ,
	MCT_L7_IRQ,
	MCT_NR_IRQS,
};

static void __iomem *reg_base;
static unsigned long clk_rate;
static unsigned int mct_int_type;
static bool mct_use_clockevent_only = false;

#ifdef CONFIG_ARM_ARCH_TIMER
static bool mct_use_mux_arch_timer = true;
#else
static bool mct_use_mux_arch_timer = false;
#endif

static int mct_irqs[MCT_NR_IRQS];

struct mct_clock_event_device {
	struct clock_event_device *evt;
	unsigned long base;
	char name[10];
};

static void exynos4_mct_write(unsigned int value, unsigned long offset)
{
	unsigned long stat_addr;
	u32 mask;
	u32 i;

	__raw_writel(value, reg_base + offset);

	if (likely(offset >= EXYNOS4_MCT_L_BASE(0))) {
		stat_addr = (offset & ~EXYNOS4_MCT_L_MASK) + MCT_L_WSTAT_OFFSET;
		switch (offset & EXYNOS4_MCT_L_MASK) {
		case MCT_L_TCON_OFFSET:
			mask = 1 << 3;		/* L_TCON write status */
			break;
		case MCT_L_ICNTB_OFFSET:
			mask = 1 << 1;		/* L_ICNTB write status */
			break;
		case MCT_L_TCNTB_OFFSET:
			mask = 1 << 0;		/* L_TCNTB write status */
			break;
		default:
			return;
		}
	} else {
		switch (offset) {
		case EXYNOS4_MCT_G_TCON:
			stat_addr = EXYNOS4_MCT_G_WSTAT;
			mask = 1 << 16;		/* G_TCON write status */
			break;
		case EXYNOS4_MCT_G_COMP0_L:
			stat_addr = EXYNOS4_MCT_G_WSTAT;
			mask = 1 << 0;		/* G_COMP0_L write status */
			break;
		case EXYNOS4_MCT_G_COMP0_U:
			stat_addr = EXYNOS4_MCT_G_WSTAT;
			mask = 1 << 1;		/* G_COMP0_U write status */
			break;
		case EXYNOS4_MCT_G_COMP0_ADD_INCR:
			stat_addr = EXYNOS4_MCT_G_WSTAT;
			mask = 1 << 2;		/* G_COMP0_ADD_INCR w status */
			break;
		case EXYNOS4_MCT_G_CNT_L:
			stat_addr = EXYNOS4_MCT_G_CNT_WSTAT;
			mask = 1 << 0;		/* G_CNT_L write status */
			break;
		case EXYNOS4_MCT_G_CNT_U:
			stat_addr = EXYNOS4_MCT_G_CNT_WSTAT;
			mask = 1 << 1;		/* G_CNT_U write status */
			break;
		default:
			return;
		}
	}

	/* Wait until written values are applied */
	for (i = 0; i < 0x1000; i++)
		if (__raw_readl(reg_base + stat_addr) & mask) {
			__raw_writel(mask, reg_base + stat_addr);
			return;
		}

	panic("MCT hangs after writing %d (offset:0x%lx)\n", value, offset);
}

/* Clocksource handling */
static void exynos4_mct_frc_start(u32 hi, u32 lo)
{
	u32 reg;

	reg = __raw_readl(reg_base + EXYNOS4_MCT_G_TCON);
	if (!(reg & MCT_G_TCON_START)) {
		exynos4_mct_write(lo, EXYNOS4_MCT_G_CNT_L);
		exynos4_mct_write(hi, EXYNOS4_MCT_G_CNT_U);

		reg |= MCT_G_TCON_START;
		exynos4_mct_write(reg, EXYNOS4_MCT_G_TCON);
	}
}

static notrace u32 exynos4_read_sched_clock(void)
{
	static u32 val;

	static DEFINE_SPINLOCK(exynos_mct_spinlock);
	unsigned long flags;

	if (soc_is_exynos5430() && samsung_rev() == EXYNOS5430_REV_0) {
		local_irq_save(flags);
		if (spin_trylock(&exynos_mct_spinlock)) {
			val = __raw_readl(reg_base + EXYNOS4_MCT_G_CNT_L);
			spin_unlock(&exynos_mct_spinlock);
		} else {
			spin_unlock_wait(&exynos_mct_spinlock);
		}
		local_irq_restore(flags);
	} else {
		val = __raw_readl(reg_base + EXYNOS4_MCT_G_CNT_L);
	}

	return val;
}

static cycle_t exynos4_frc_read(struct clocksource *cs)
{
	/* Exynos series supports only 32bit clocksource */
	return (cycle_t)exynos4_read_sched_clock();
}

static void exynos4_frc_resume(struct clocksource *cs)
{
	exynos4_mct_frc_start(0, 0);
}

struct clocksource mct_frc = {
	.name		= "mct-frc",
	.rating		= 400,
	.read		= exynos4_frc_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
	.resume		= exynos4_frc_resume,
};

static void __init exynos4_clocksource_init(void)
{
	exynos4_mct_frc_start(0, 0);

	if (clocksource_register_hz(&mct_frc, clk_rate))
		panic("%s: can't register clocksource\n", mct_frc.name);

/*	setup_sched_clock(exynos4_read_sched_clock, 32, clk_rate);	*/
}

static void exynos4_mct_comp0_stop(void)
{
	unsigned int tcon;

	tcon = __raw_readl(reg_base + EXYNOS4_MCT_G_TCON);
	tcon &= ~(MCT_G_TCON_COMP0_ENABLE | MCT_G_TCON_COMP0_AUTO_INC);

	exynos4_mct_write(tcon, EXYNOS4_MCT_G_TCON);
	exynos4_mct_write(0, EXYNOS4_MCT_G_INT_ENB);
}

static void exynos4_mct_comp0_start(enum clock_event_mode mode,
				    unsigned long cycles)
{
	unsigned int tcon;
	cycle_t comp_cycle = 0;

	tcon = __raw_readl(reg_base + EXYNOS4_MCT_G_TCON);

	if (mode == CLOCK_EVT_MODE_PERIODIC) {
		tcon |= MCT_G_TCON_COMP0_AUTO_INC;
		exynos4_mct_write(cycles, EXYNOS4_MCT_G_COMP0_ADD_INCR);
	}

	if (mct_frc.mask == (cycle_t)CLOCKSOURCE_MASK(32))
		comp_cycle = __raw_readl(reg_base + EXYNOS4_MCT_G_CNT_U);

	comp_cycle += exynos4_frc_read(&mct_frc) + cycles;
	exynos4_mct_write((u32)comp_cycle, EXYNOS4_MCT_G_COMP0_L);
	exynos4_mct_write((u32)(comp_cycle >> 32), EXYNOS4_MCT_G_COMP0_U);

	exynos4_mct_write(0x1, EXYNOS4_MCT_G_INT_ENB);

	tcon |= MCT_G_TCON_COMP0_ENABLE;
	exynos4_mct_write(tcon , EXYNOS4_MCT_G_TCON);
}

static int exynos4_comp_set_next_event(unsigned long cycles,
				       struct clock_event_device *evt)
{
	exynos4_mct_comp0_start(evt->mode, cycles);

	return 0;
}

static void exynos4_comp_set_mode(enum clock_event_mode mode,
				  struct clock_event_device *evt)
{
	unsigned long cycles_per_jiffy;
	exynos4_mct_comp0_stop();

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		cycles_per_jiffy =
			(((unsigned long long) NSEC_PER_SEC / HZ * evt->mult) >> evt->shift);
		exynos4_mct_comp0_start(mode, cycles_per_jiffy);
		break;

	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static struct clock_event_device mct_comp_device = {
	.name		= "mct-comp",
	.features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.rating		= 250,
	.set_next_event	= exynos4_comp_set_next_event,
	.set_mode	= exynos4_comp_set_mode,
};

static irqreturn_t exynos4_mct_comp_isr(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	exynos4_mct_write(0x1, EXYNOS4_MCT_G_INT_CSTAT);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction mct_comp_event_irq = {
	.name		= "mct_comp_irq",
	.flags		= IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= exynos4_mct_comp_isr,
	.dev_id		= &mct_comp_device,
};

static void exynos4_clockevent_init(void)
{
	mct_comp_device.cpumask = cpumask_of(0);
	clockevents_config_and_register(&mct_comp_device, clk_rate,
					0xf, 0xffffffff);
	setup_irq(mct_irqs[MCT_G0_IRQ], &mct_comp_event_irq);
}

#ifdef CONFIG_LOCAL_TIMERS

static DEFINE_PER_CPU(struct mct_clock_event_device, percpu_mct_tick);
static int tick_base_cnt;

int exynos4_mct_tick_dump(int timer)
{
	pr_info("mct_tick%d - TCNTB:%08X, TCNTO:%08X, ICNTB:%08X, ICNTO:%08X\n",
		timer, __raw_readl(reg_base + EXYNOS4_MCT_L_BASE(timer)),
		__raw_readl(reg_base + EXYNOS4_MCT_L_BASE(timer) + 0x4),
		__raw_readl(reg_base + EXYNOS4_MCT_L_BASE(timer) + 0x8),
		__raw_readl(reg_base + EXYNOS4_MCT_L_BASE(timer) + 0xC));

	return 0;
}
EXPORT_SYMBOL(exynos4_mct_tick_dump);

/* Clock event handling */
static void exynos4_mct_tick_stop(struct mct_clock_event_device *mevt, int force)
{
	unsigned long tmp;
	struct clock_event_device *evt = mevt->evt;

	/* clear MCT local interrupt */
	exynos4_mct_write(0x1, mevt->base + MCT_L_INT_CSTAT_OFFSET);

	if (force || evt->mode != CLOCK_EVT_MODE_PERIODIC) {
		tmp = __raw_readl(reg_base + mevt->base + MCT_L_TCON_OFFSET);
		tmp &= ~(MCT_L_TCON_INT_START | MCT_L_TCON_TIMER_START);
		exynos4_mct_write(tmp, mevt->base + MCT_L_TCON_OFFSET);
	}
}

static void exynos4_mct_tick_start(unsigned long cycles, int periodic,
				   struct mct_clock_event_device *mevt)
{
	unsigned long tmp;

	tmp = (1 << 31) | cycles;	/* MCT_L_UPDATE_ICNTB */

	/* update interrupt count buffer */
	exynos4_mct_write(tmp, mevt->base + MCT_L_ICNTB_OFFSET);

	/* enable MCT tick interrupt */
	exynos4_mct_write(0x1, mevt->base + MCT_L_INT_ENB_OFFSET);

	tmp = __raw_readl(reg_base + mevt->base + MCT_L_TCON_OFFSET);
	tmp |= MCT_L_TCON_INT_START | MCT_L_TCON_TIMER_START;

	if (periodic)
		tmp |= MCT_L_TCON_INTERVAL_MODE;

	exynos4_mct_write(tmp, mevt->base + MCT_L_TCON_OFFSET);
}

static int exynos4_tick_set_next_event(unsigned long cycles,
				       struct clock_event_device *evt)
{
	struct mct_clock_event_device *mevt = this_cpu_ptr(&percpu_mct_tick);

	exynos4_mct_tick_start(cycles, 0, mevt);

	return 0;
}

static inline void exynos4_tick_set_mode(enum clock_event_mode mode,
					 struct clock_event_device *evt)
{
	struct mct_clock_event_device *mevt = this_cpu_ptr(&percpu_mct_tick);
	unsigned long cycles_per_jiffy;

	exynos4_mct_tick_stop(mevt, 1);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		cycles_per_jiffy =
			(((unsigned long long) NSEC_PER_SEC / HZ * evt->mult) >> evt->shift);
		exynos4_mct_tick_start(cycles_per_jiffy, 1, mevt);
		break;
	case CLOCK_EVT_MODE_RESUME:
		if (!soc_is_exynos5433())
			exynos4_mct_write(tick_base_cnt, mevt->base + MCT_L_TCNTB_OFFSET);
		break;

	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		break;
	}
}

static irqreturn_t exynos4_mct_tick_isr(int irq, void *dev_id)
{
	struct mct_clock_event_device *mevt = dev_id;
	struct clock_event_device *evt = mevt->evt;

	exynos4_mct_tick_stop(mevt, 0);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static const char *irq_names[] = {
	"mct_tick0",
	"mct_tick1",
	"mct_tick2",
	"mct_tick3",
	"mct_tick4",
	"mct_tick5",
	"mct_tick6",
	"mct_tick7",
};

static DEFINE_PER_CPU(struct irqaction, percpu_mct_irq) = {
	.flags          = IRQF_TIMER | IRQF_NOBALANCING,
	.handler        = exynos4_mct_tick_isr,
};

static int exynos4_local_timer_setup(struct clock_event_device *evt)
{
	struct mct_clock_event_device *mevt;
	unsigned int cpu = smp_processor_id();

	mevt = this_cpu_ptr(&percpu_mct_tick);
	mevt->evt = evt;

	mevt->base = EXYNOS4_MCT_L_BASE(cpu);
	snprintf(mevt->name, sizeof(mevt->name), "mct_tick%d", cpu);

	evt->name = mevt->name;
	evt->cpumask = cpumask_of(cpu);
	evt->set_next_event = exynos4_tick_set_next_event;
	evt->set_mode = exynos4_tick_set_mode;
	evt->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
	evt->rating = 450;
	tick_base_cnt = 0;

	if (!soc_is_exynos5433()) {
		tick_base_cnt = 1;
		exynos4_mct_write(tick_base_cnt, mevt->base + MCT_L_TCNTB_OFFSET);
	}

	if (mct_int_type == MCT_INT_SPI) {
		struct irqaction *mct_irq = this_cpu_ptr(&percpu_mct_irq);

		mct_irq->dev_id = mevt;
		evt->irq = mct_irqs[MCT_L0_IRQ + cpu];
		irq_set_affinity(evt->irq, cpumask_of(cpu));
		enable_irq(evt->irq);
	} else {
		enable_percpu_irq(mct_irqs[MCT_L0_IRQ], 0);
	}

	clockevents_config_and_register(evt, clk_rate / (tick_base_cnt + 1),
					0xf, 0x7fffffff);

	return 0;
}

static void exynos4_local_timer_stop(struct clock_event_device *evt)
{
	evt->set_mode(CLOCK_EVT_MODE_UNUSED, evt);
	if (mct_int_type == MCT_INT_SPI)
		disable_irq(evt->irq);
	else
		disable_percpu_irq(mct_irqs[MCT_L0_IRQ]);
}

static struct local_timer_ops exynos4_mct_tick_ops = {
	.setup	= exynos4_local_timer_setup,
	.stop	= exynos4_local_timer_stop,
};

static void __init exynos4_local_timer_init(void)
{
	int cpu;

	if (mct_int_type == MCT_INT_SPI) {
		for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
			struct irqaction *mct_irq
					= per_cpu_ptr(&percpu_mct_irq, cpu);
			mct_irq->name = irq_names[cpu];
			setup_irq(mct_irqs[MCT_L0_IRQ + cpu], mct_irq);
			disable_irq(mct_irqs[MCT_L0_IRQ + cpu]);
		}
	}
}
#endif /* CONFIG_LOCAL_TIMERS */

static void __init exynos4_timer_resources(struct device_node *np, void __iomem *base)
{
	struct clk *mct_clk, *tick_clk;

	tick_clk = np ? of_clk_get_by_name(np, "fin_pll") :
				clk_get(NULL, "fin_pll");
	if (IS_ERR(tick_clk))
		panic("%s: unable to determine tick clock rate\n", __func__);
	clk_rate = clk_get_rate(tick_clk);

	/* If it is failed to get clk_rate, clk_rate is set by default to osc */
	if (!clk_rate)
		clk_rate = 24000000;

	mct_clk = np ? of_clk_get_by_name(np, "mct") : clk_get(NULL, "mct");
	if (IS_ERR(mct_clk))
		panic("%s: unable to retrieve mct clock instance\n", __func__);
	clk_prepare_enable(mct_clk);

	reg_base = base;
	if (!reg_base)
		panic("%s: unable to ioremap mct address space\n", __func__);

#ifdef CONFIG_LOCAL_TIMERS
	if (mct_int_type == MCT_INT_PPI) {
		int err;

		err = request_percpu_irq(mct_irqs[MCT_L0_IRQ],
					 exynos4_mct_tick_isr, "MCT",
					 &percpu_mct_tick);
		WARN(err, "MCT: can't request IRQ %d (%d)\n",
		     mct_irqs[MCT_L0_IRQ], err);
	}

	local_timer_register(&exynos4_mct_tick_ops);
#endif /* CONFIG_LOCAL_TIMERS */
}

#ifndef CONFIG_ARM64
static struct delay_timer mct_delay_timer;
#endif

unsigned long exynos_mct_read_current_timer(void)
{
	return (unsigned long)exynos4_read_sched_clock();
}

static void __init exynos4_timer_delay_init(void)
{
#ifndef CONFIG_ARM64
	mct_delay_timer.read_current_timer = &exynos_mct_read_current_timer;
	mct_delay_timer.freq = clk_rate;
	register_current_timer_delay(&mct_delay_timer);
#endif
}

void __init mct_init(void __iomem *base, int irq_g0, int irq_l0, int irq_l1)
{
	mct_irqs[MCT_G0_IRQ] = irq_g0;
	mct_irqs[MCT_L0_IRQ] = irq_l0;
	mct_irqs[MCT_L1_IRQ] = irq_l1;
	mct_int_type = MCT_INT_SPI;

	exynos4_timer_resources(NULL, base);
	exynos4_clocksource_init();
	exynos4_clockevent_init();
	exynos4_timer_delay_init();
}

static void __init mct_init_dt(struct device_node *np, unsigned int int_type)
{
	u32 nr_irqs, i;

	mct_int_type = int_type;

	/* This driver uses only one global timer interrupt */
	mct_irqs[MCT_G0_IRQ] = irq_of_parse_and_map(np, MCT_G0_IRQ);

	/*
	 * Find out the number of local irqs specified. The local
	 * timer irqs are specified after the four global timer
	 * irqs are specified.
	 */
#ifdef CONFIG_OF
	nr_irqs = of_irq_count(np);
#else
	nr_irqs = 0;
#endif
	for (i = MCT_L0_IRQ; i < nr_irqs; i++)
		mct_irqs[i] = irq_of_parse_and_map(np, i);

	if (mct_use_mux_arch_timer) {
		if (of_property_read_bool(np, "use-clockevent-only")) {
			pr_info("%s: exynos_mct is used only clockevent\n", __func__);
			mct_use_clockevent_only = true;
		}
	}

	exynos4_timer_resources(np, of_iomap(np, 0));
#ifdef CONFIG_LOCAL_TIMERS
	exynos4_local_timer_init();
#endif
	if (!mct_use_clockevent_only) {
		exynos4_clocksource_init();
		exynos4_timer_delay_init();
	} else {
		/*  just start mct_frc for muxed timer */
		exynos4_mct_frc_start(0, 0);
	}

	exynos4_clockevent_init();
}


static void __init mct_init_spi(struct device_node *np)
{
	return mct_init_dt(np, MCT_INT_SPI);
}

static void __init mct_init_ppi(struct device_node *np)
{
	return mct_init_dt(np, MCT_INT_PPI);
}
CLOCKSOURCE_OF_DECLARE(exynos4210, "samsung,exynos4210-mct", mct_init_spi);
CLOCKSOURCE_OF_DECLARE(exynos4412, "samsung,exynos4412-mct", mct_init_ppi);
