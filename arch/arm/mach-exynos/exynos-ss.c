/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-SnapShot for debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/memblock.h>

#include <asm/mach/map.h>
#include <asm/cacheflush.h>
#include <asm/ptrace.h>
#include <asm/memory.h>
#include <plat/cpu.h>
#include <mach/regs-pmu.h>
#include <mach/exynos-ss.h>

#ifdef CONFIG_EXYNOS_SNAPSHOT

/*  Size domain */
#define ESS_HEADER_SZ			SZ_4K
#define ESS_MMU_REG_SZ			SZ_4K
#define ESS_CORE_REG_SZ			SZ_4K
#define ESS_HEADER_TOTAL_SZ		(ESS_HEADER_SZ + ESS_MMU_REG_SZ + ESS_CORE_REG_SZ)

/*  Length domain */
#define ESS_LOG_STRING_LENGTH		SZ_128
#define ESS_MMU_REG_OFFSET		SZ_256
#define ESS_CORE_REG_OFFSET		SZ_256
#define ESS_LOG_MAX_NUM			SZ_1K
#define ESS_API_MAX_NUM			SZ_2K
#define ESS_EX_MAX_NUM			SZ_8
#define ESS_IN_MAX_NUM			SZ_8
#define ESS_CALLSTACK_MAX_NUM		4
#define ESS_ITERATION			5
#define ESS_NR_CPUS			NR_CPUS

/* Items domain */
#define ESS_ITEMS_NUM			4
#define ESS_ITEMS_KEVENTS		0
#define ESS_ITEMS_LOG_KERNEL		1
#define ESS_ITEMS_LOG_MAIN		2
#define ESS_ITEMS_LOG_SYSTEM		3

/*  Specific Address Information */
#define S5P_VA_SS_BASE			S3C_ADDR(0x03000000)
#define S5P_VA_SS_SCRATCH		(S5P_VA_SS_BASE + 0x100)
#define S5P_VA_SS_LAST_LOGBUF		(S5P_VA_SS_BASE + 0x200)

struct exynos_ss_base {
	size_t size;
	size_t vaddr;
	size_t paddr;
	unsigned int enabled;
};

struct exynos_ss_item {
	char *name;
	struct exynos_ss_base entry;
	unsigned char *head_ptr;
	unsigned char *curr_ptr;
};

struct exynos_ss_log {
	struct task_log {
		unsigned long long time;
		char comm[TASK_COMM_LEN];
		pid_t pid;
	} task[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct work_log {
		unsigned long long time;
		struct worker *worker;
		struct work_struct *work;
		work_func_t f;
		int en;
	} work[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct clockevent_log {
		unsigned long long time;
		unsigned long long clc;
		int64_t	delta;
		ktime_t	next_event;
	} clockevent[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct cpuidle_log {
		unsigned long long time;
		int index;
		unsigned state;
		u32 num_online_cpus;
		int delta;
		int en;
	} cpuidle[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct suspend_log {
		unsigned long long time;
		void *fn;
		struct device *dev;
		int en;
	} suspend[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct irq_log {
		unsigned long long time;
		int irq;
		void *fn;
		int preempt;
		int curr_disabled;
		int en;
	} irq[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	struct irq_exit_log {
		unsigned long long time;
		unsigned long long end_time;
		unsigned long long latency;
		int irq;
	} irq_exit[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
	struct irqs_disabled_log {
		unsigned long long time;
		unsigned long long latency;
		int try_disabled;
		int curr_disabled;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} irqs_disabled[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	struct reg_log {
		unsigned long long time;
		int read;
		size_t val;
		size_t reg;
		int en;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} reg[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
	struct hrtimer_log {
		unsigned long long time;
		unsigned long long now;
		struct hrtimer *timer;
		enum hrtimer_restart (*fn)(struct hrtimer *);
		int en;
	} hrtimers[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
	struct printkl_log {
		unsigned long long time;
		int cpu;
		size_t msg;
		size_t val;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} printkl[ESS_API_MAX_NUM];

	struct printk_log {
		unsigned long long time;
		int cpu;
		char log[ESS_LOG_STRING_LENGTH];
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} printk[ESS_API_MAX_NUM];
#ifdef CONFIG_EXYNOS_CORESIGHT
	struct core_log {
		void *last_pc[ESS_ITERATION];
	} core[ESS_NR_CPUS];
#endif
};

struct exynos_ss_log_idx {
	atomic_t task_log_idx[ESS_NR_CPUS];
	atomic_t work_log_idx[ESS_NR_CPUS];
	atomic_t clockevent_log_idx[ESS_NR_CPUS];
	atomic_t cpuidle_log_idx[ESS_NR_CPUS];
	atomic_t suspend_log_idx[ESS_NR_CPUS];
	atomic_t irq_log_idx[ESS_NR_CPUS];
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	atomic_t irq_exit_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
	atomic_t irqs_disabled_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	atomic_t reg_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
	atomic_t hrtimer_log_idx[ESS_NR_CPUS];
#endif
	atomic_t printkl_log_idx;
	atomic_t printk_log_idx;
};

struct exynos_ss_mmu_reg {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};

struct exynos_ss_interface {
	struct exynos_ss_log *info_event;
	struct exynos_ss_item info_log[ESS_ITEMS_NUM];
};

extern void *return_address(int);
extern void (*arm_pm_restart)(char str, const char *cmd);
#ifdef CONFIG_EXYNOS_CORESIGHT
extern unsigned int exynos_cs_pc[NR_CPUS][ESS_ITERATION];
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
extern void register_hook_logbuf(void (*)(const char));
#else
extern void register_hook_logbuf(void (*)(const char *, u64, size_t));
#endif
extern void register_hook_logger(void (*)(const char *, const char *, size_t));

static DEFINE_SPINLOCK(ess_lock);
static unsigned ess_callstack = CONFIG_EXYNOS_SNAPSHOT_CALLSTACK;

/*
 * ---------------------------------------------------------------------------
 *  User defined Start
 * ---------------------------------------------------------------------------
 *
 *  clarified exynos-snapshot items, before using exynos-snapshot we should
 *  evince memory-map of snapshot
 */
static struct exynos_ss_item ess_items[] = {
	{"log_kevents",	{SZ_8M,		0, 0, true}, NULL ,NULL},
	{"log_kernel",	{SZ_2M,		0, 0, true}, NULL ,NULL},
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	{"log_main",	{SZ_2M + SZ_1M,	0, 0, true}, NULL ,NULL},
	{"log_system",	{SZ_1M,		0, 0, true}, NULL ,NULL},
#endif
};

/*
 *  including or excluding options
 *  if you want to except some interrupt, it should be written in this array
 */
static unsigned int ess_irqlog_exlist[] = {
/*  interrupt number ex) 152, 153, 154, */
	0,0,0,0,0,0,0,0,
};

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static unsigned int ess_irqexit_exlist[] = {
/*  interrupt number ex) 152, 153, 154, */
	0,0,0,0,0,0,0,0,
};

static unsigned ess_irqexit_threshold =
		CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT_THRESHOLD;
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
static unsigned ess_irqdisabled_threshold =
		CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED_THRESHOLD;
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
struct ess_reg_list {
	size_t addr;
	size_t size;
};

static struct ess_reg_list ess_reg_exlist[] = {
/*
 *  if it wants to reduce effect enabled reg feautre to system,
 *  you must add these registers - mct, serial
 *  because they are called very often.
 *  physical address, size ex) {0x10C00000, 0x1000},
 */
	{ESS_REG_MCT_ADDR, ESS_REG_MCT_SIZE},
	{ESS_REG_UART_ADDR, ESS_REG_UART_SIZE},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
};
#endif

/*
 * ---------------------------------------------------------------------------
 *  User defined End
 * ---------------------------------------------------------------------------
 */

/*  External interface variable for trace debugging */
static struct exynos_ss_interface ess_info;

/*  Internal interface variable */
static struct exynos_ss_base ess_base;
static struct exynos_ss_log_idx ess_idx;
static struct exynos_ss_log *ess_log = NULL;

DEFINE_PER_CPU(struct pt_regs *, ess_core_reg);
DEFINE_PER_CPU(struct exynos_ss_mmu_reg *, ess_mmu_reg);
DEFINE_PER_CPU(enum ess_cause_emerg_events, ess_cause_emerg);

static void exynos_ss_save_mmu(struct exynos_ss_mmu_reg *mmu_reg)
{
#ifndef CONFIG_ARM64
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
	    "str r1, [%0]\n\t"
	    "mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
	    "str r1, [%0,#4]\n\t"
	    "mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
	    "str r1, [%0,#8]\n\t"
	    "mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
	    "str r1, [%0,#12]\n\t"
	    "mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
	    "str r1, [%0,#16]\n\t"
	    "mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
	    "str r1, [%0,#20]\n\t"
	    "mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
	    "str r1, [%0,#24]\n\t"
	    "mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
	    "str r1, [%0,#28]\n\t"
	    "mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
	    "str r1, [%0,#32]\n\t"
	    /* Don't populate DAFSR and RAFSR */
	    "mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
	    "str r1, [%0,#44]\n\t"
	    "mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
	    "str r1, [%0,#48]\n\t"
	    "mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
	    "str r1, [%0,#52]\n\t"
	    "mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
	    "str r1, [%0,#56]\n\t"
	    "mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
	    "str r1, [%0,#60]\n\t"
	    "mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
	    "str r1, [%0,#64]\n\t"
	    "mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
	    "str r1, [%0,#68]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%r1", "memory"			/* clobbered register */
	);
#endif
}

int exynos_ss_early_dump(void)
{
	/*  This function is for tracing registers more early */
	return 0;
}
EXPORT_SYMBOL(exynos_ss_early_dump);

int exynos_ss_dump(void)
{
#ifndef CONFIG_ARM64
	/*
	 *  Output CPU Memory Error syndrome Register
	 *  CPUMERRSR, L2MERRSR
	 */
	unsigned long reg0;

	asm ("mrc p15, 0, %0, c0, c0, 0\n": "=r" (reg0));
	if (((reg0 >> 4) & 0xFFF) == 0xC0F) {
		/*  Only Cortex-A15 */
		unsigned long reg1, reg2, reg3;
		asm ("mrrc p15, 0, %0, %1, c15\n\t"
			"mrrc p15, 1, %2, %3, c15\n"
			: "=r" (reg0), "=r" (reg1),
			"=r" (reg2), "=r" (reg3));
		pr_emerg("CPUMERRSR: %08lx_%08lx, L2MERRSR: %08lx_%08lx\n",
				reg1, reg0, reg3, reg2);
	}
#endif
	return 0;
}
EXPORT_SYMBOL(exynos_ss_dump);

int exynos_ss_save_reg(struct pt_regs *regs)
{
	struct pt_regs *core_reg =
			per_cpu(ess_core_reg, smp_processor_id());

	if (!regs)
		regs = task_pt_regs(current);

	memcpy(core_reg, regs, sizeof(struct pt_regs));

	pr_emerg("exynos-snapshot: core register saved(CPU:%d)\n",
						smp_processor_id());
	return 0;
}
EXPORT_SYMBOL(exynos_ss_save_reg);

int exynos_ss_save_context(struct pt_regs *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&ess_lock, flags);
	exynos_ss_save_mmu(per_cpu(ess_mmu_reg, smp_processor_id()));
	exynos_ss_save_reg(regs);
	exynos_ss_dump();
	pr_emerg("exynos-snapshot: context saved(CPU:%d)\n",
						smp_processor_id());
	flush_cache_all();
	spin_unlock_irqrestore(&ess_lock, flags);
	return 0;
}
EXPORT_SYMBOL(exynos_ss_save_context);

int exynos_ss_set_enable(const char *name, int en)
{
	struct exynos_ss_item *item = NULL;
	unsigned i;

	if (!strncmp(name, "base", strlen(name))) {
		ess_base.enabled = en;
		pr_info("exynos-snapshot: %sabled\n", en ? "en" : "dis");
	}
	else {
		for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
			if (!strncmp(ess_items[i].name, name, strlen(name))) {
				item = &ess_items[i];
				item->entry.enabled = en;
				pr_info("exynos-snapshot: item - %s is %sabled\n",
						name, en ? "en" : "dis");
				break;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_set_enable);

int exynos_ss_get_enable(const char *name)
{
	struct exynos_ss_item *item = NULL;
	unsigned i;
	int ret = -1;

	if (!strncmp(name, "base", strlen(name))) {
		ret = ess_base.enabled;
	}
	else {
		for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
			if (!strncmp(ess_items[i].name, name, strlen(name))) {
				item = &ess_items[i];
				ret = item->entry.enabled;
				break;
			}
		}
	}
	return ret;
}
EXPORT_SYMBOL(exynos_ss_get_enable);

static inline int exynos_ss_check_eob(struct exynos_ss_item *item,
						size_t size)
{
	size_t max, cur;

	max = (size_t)(item->head_ptr + item->entry.size);
	cur = (size_t)(item->curr_ptr + size);

	if (unlikely(cur > max))
		return -1;
	else
		return 0;
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
static inline void exynos_ss_hook_logger(const char *name,
					 const char *buf, size_t size)
{
	struct exynos_ss_item *item = NULL;
	int i;

	for (i = ESS_ITEMS_LOG_MAIN; i < ARRAY_SIZE(ess_items); i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name))) {
			item = &ess_items[i];
			break;
		}
	}

	if (unlikely(!item))
		return;

	if (likely(ess_base.enabled == true && item->entry.enabled == true)) {
		if (unlikely((exynos_ss_check_eob(item, size))))
			item->curr_ptr = item->head_ptr;
		memcpy(item->curr_ptr, buf, size);
		item->curr_ptr += size;
	}
}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
static inline void exynos_ss_hook_logbuf(const char buf)
{
	unsigned int last_buf;
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_LOG_KERNEL];

	if (likely(ess_base.enabled == true && item->entry.enabled == true)) {
		if (exynos_ss_check_eob(item, 1))
			item->curr_ptr = item->head_ptr;

		item->curr_ptr[0] = buf;
		item->curr_ptr++;

		/*  save the address of last_buf to physical address */
		last_buf = (unsigned int)item->curr_ptr;
		__raw_writel((last_buf & (SZ_16M - 1)) | ess_base.paddr, S5P_VA_SS_LAST_LOGBUF);
	}
}
#else
static inline void exynos_ss_hook_logbuf(const char *buf, u64 ts_nsec, size_t size)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_LOG_KERNEL];

	if (likely(ess_base.enabled == true && item->entry.enabled == true)) {
		unsigned long rem_nsec;
		size_t last_buf;
		size_t timelen = 0;

		if (exynos_ss_check_eob(item, size + 32))
			item->curr_ptr = item->head_ptr;

		rem_nsec = do_div(ts_nsec, 1000000000);

		/*  fixed exact size */
		timelen = snprintf(item->curr_ptr, 32,
			"[%5lu.%06lu] ", (unsigned long)ts_nsec, rem_nsec / 1000);

		item->curr_ptr += timelen;
		memcpy(item->curr_ptr, buf, size);
		item->curr_ptr += size;
		item->curr_ptr[0] = '\n';
		item->curr_ptr++;

		/*  save the address of last_buf to physical address */
		last_buf = (size_t)item->curr_ptr;
		__raw_writel((last_buf & (SZ_16M - 1)) | ess_base.paddr, S5P_VA_SS_LAST_LOGBUF);
	}
}
#endif

enum ess_cause_emerg_events {
	CAUSE_INVALID_DUMP = 0x00000000,
	CAUSE_KERNEL_PANIC = 0x00000001,
	CAUSE_FORCE_DUMP   = 0x0000000D,
	CAUSE_FORCE_REBOOT = 0x000000FF,
};

static void exynos_ss_scratch_reg(unsigned int val)
{
	__raw_writel(val, S5P_VA_SS_SCRATCH);
}

#if defined(CONFIG_EXYNOS_SNAPSHOT_FORCE_DUMP_MODE) || defined(CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT)
static void exynos_ss_report_cause_emerg(enum ess_cause_emerg_events event)
{
	per_cpu(ess_cause_emerg, smp_processor_id()) = event;
}
#endif

static int exynos_ss_reboot_handler(struct notifier_block *nb,
				    unsigned long l, void *p)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_FORCE_DUMP_MODE
	local_irq_disable();
	pr_emerg("exynos-snapshot: forced reboot [%s]\n", __func__);
	exynos_ss_report_cause_emerg(CAUSE_FORCE_DUMP);
	flush_cache_all();
	arm_pm_restart(0, "reset");
	while(1);
#else
	pr_emerg("exynos-snapshot: normal reboot [%s]\n", __func__);
	exynos_ss_scratch_reg(CAUSE_INVALID_DUMP);
	exynos_ss_save_context(NULL);
	flush_cache_all();
#endif
	return 0;
}

static int exynos_ss_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT
	unsigned long flags;
	spin_lock_irqsave(&ess_lock, flags);
	exynos_ss_report_cause_emerg(CAUSE_KERNEL_PANIC);
#ifdef CONFIG_EXYNOS_CORESIGHT
	memcpy(ess_log->core, exynos_cs_pc, sizeof(ess_log->core));
#endif
	pr_emerg("exynos-snapshot: panic - forced ramdump mode [%s]\n", __func__);
	flush_cache_all();
	arm_pm_restart(0, "reset");
	spin_unlock_irqrestore(&ess_lock, flags);
#else
	pr_emerg("exynos-snapshot: panic [%s]\n", __func__);
	flush_cache_all();
#endif
	return 0;
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = exynos_ss_reboot_handler
};

static struct notifier_block nb_panic_block = {
	.notifier_call = exynos_ss_panic_handler,
};

static unsigned int __init exynos_ss_remap(unsigned int base, unsigned int size)
{
	static struct map_desc ess_iodesc[ESS_ITEMS_NUM];
	int i;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		/* fill rest value of ess_items arrary */
		if (i == 0) {
			ess_items[i].entry.vaddr = (size_t)S5P_VA_SS_BASE;
			ess_items[i].entry.paddr = (size_t)base;
		} else {
			ess_items[i].entry.vaddr = ess_items[i - 1].entry.vaddr
						+ ess_items[i - 1].entry.size;
			ess_items[i].entry.paddr = ess_items[i - 1].entry.paddr
						+ ess_items[i - 1].entry.size;
		}
		ess_items[i].head_ptr = (unsigned char *)ess_items[i].entry.vaddr;
		ess_items[i].curr_ptr = (unsigned char *)ess_items[i].entry.vaddr;

		/* fill to ess_iodesc for mapping */
		ess_iodesc[i].type = MT_DEVICE;
		ess_iodesc[i].length = ess_items[i].entry.size;
		ess_iodesc[i].virtual = ess_items[i].entry.vaddr;
		ess_iodesc[i].pfn = __phys_to_pfn(ess_items[i].entry.paddr);
	}

	iotable_init(ess_iodesc, ARRAY_SIZE(ess_items));
	return (unsigned int)ess_items[0].entry.vaddr;
}

static int __init exynos_ss_setup(char *str)
{
	int i;
	size_t size = 0;
	size_t base = 0;

	if (kstrtoul(str, 0, (unsigned long *)&base))
		goto out;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (ess_items[i].entry.enabled)
			size += ess_items[i].entry.size;
	}

	pr_info("exynos-snapshot: try to reserve dedicated memory : 0x%zx, 0x%zx\n",
			base, size);

#ifdef CONFIG_NO_BOOTMEM
	if (!memblock_is_region_reserved(base, size) &&
		!memblock_reserve(base, size)) {

#else
	if (!reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE)) {
#endif
		ess_base.paddr = base;
		ess_base.vaddr = exynos_ss_remap(base,size);
		ess_base.size = size;
		ess_base.enabled = false;

		pr_info("exynos-snapshot: memory reserved complete : 0x%zx, 0x%zx\n",
			base, size);

		return 0;
	}
out:
	pr_err("exynos-snapshot: buffer reserved failed : 0x%zx, 0x%zx\n", base, size);
	return -1;
}
__setup("ess_setup=", exynos_ss_setup);

/*
 *  Normally, exynos-snapshot has 2-types debug buffer - log and hook.
 *  hooked buffer is for log_buf of kernel and loggers of platform.
 *  Each buffer has 2Mbyte memory except loggers. Loggers is consist of 4
 *  division. Each logger has 1Mbytes.
 *  ---------------------------------------------------------------------
 *  - dummy data:phy_addr, virtual_addr, buffer_size, magic_key(4K)	-
 *  ---------------------------------------------------------------------
 *  -		Cores MMU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		Cores CPU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		log buffer(3Mbyte - Headers(12K))			-
 *  ---------------------------------------------------------------------
 *  -		Hooked buffer of kernel's log_buf(2Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked main logger buffer of platform(3Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked system logger buffer of platform(1Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked radio logger buffer of platform(?Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked events logger buffer of platform(?Mbyte)		-
 *  ---------------------------------------------------------------------
 */
static int __init exynos_ss_output(void)
{
	int i;

	pr_info("exynos-snapshot physical / virtual memory layout:\n");
	for (i = 0; i < ARRAY_SIZE(ess_items); i++)
		pr_info("%-12s: phys:0x%zx / virt:0x%zx / size:0x%zx\n",
			ess_items[i].name,
			ess_items[i].entry.paddr,
			ess_items[i].entry.vaddr,
			ess_items[i].entry.size);

	return 0;
}

/*	Header dummy data(4K)
 *	-------------------------------------------------------------------------
 *		0		4		8		C
 *	-------------------------------------------------------------------------
 *	0	vaddr	phy_addr	size		magic_code
 *	4	Scratch_val	logbuf_addr	0		0
 *	-------------------------------------------------------------------------
*/
static void __init exynos_ss_fixmap_header(void)
{
	/*  fill 0 to next to header */
	size_t vaddr, paddr, size;
	size_t *addr;
	int i;

	vaddr = ess_items[ESS_ITEMS_KEVENTS].entry.vaddr;
	paddr = ess_items[ESS_ITEMS_KEVENTS].entry.paddr;
	size = ess_items[ESS_ITEMS_KEVENTS].entry.size;

	/*  set to confirm exynos-snapshot */
	addr = (size_t *)vaddr;
	memcpy(addr, &ess_base, sizeof(struct exynos_ss_base));

	/*  kernel log buf */
	ess_log = (struct exynos_ss_log *)(vaddr + ESS_HEADER_TOTAL_SZ);

	/*  set fake translation to virtual address to debug trace */
	ess_info.info_event = (struct exynos_ss_log *)(PAGE_OFFSET |
			    (0x0FFFFFFF & (paddr + ESS_HEADER_TOTAL_SZ)));

	atomic_set(&(ess_idx.printk_log_idx), -1);
	atomic_set(&(ess_idx.printkl_log_idx), -1);
	for (i = 0; i < ESS_NR_CPUS; i++) {
		atomic_set(&(ess_idx.task_log_idx[i]), -1);
		atomic_set(&(ess_idx.work_log_idx[i]), -1);
		atomic_set(&(ess_idx.clockevent_log_idx[i]), -1);
		atomic_set(&(ess_idx.cpuidle_log_idx[i]), -1);
		atomic_set(&(ess_idx.suspend_log_idx[i]), -1);
		atomic_set(&(ess_idx.irq_log_idx[i]), -1);
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
		atomic_set(&(ess_idx.irq_exit_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
		atomic_set(&(ess_idx.irqs_disabled_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
		atomic_set(&(ess_idx.reg_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
		atomic_set(&(ess_idx.hrtimer_log_idx[i]), -1);
#endif
		per_cpu(ess_mmu_reg, i) = (struct exynos_ss_mmu_reg *)
					  (vaddr + ESS_HEADER_SZ +
					   i * ESS_MMU_REG_OFFSET);
		per_cpu(ess_core_reg, i) = (struct pt_regs *)
					   (vaddr + ESS_HEADER_SZ + ESS_MMU_REG_SZ +
					    i * ESS_CORE_REG_OFFSET);
	}
	/*  initialize kernel event to 0 except only header */
	memset((size_t *)(vaddr + ESS_HEADER_SZ), 0, size - ESS_HEADER_SZ);
}

static int __init exynos_ss_fixmap(void)
{
	size_t last_buf, align;
	size_t vaddr, paddr, size;
	int i;

	/*  fixmap to header first */
	exynos_ss_fixmap_header();

	for (i = 1; i < ARRAY_SIZE(ess_items); i++) {
		/*  assign kernel log information */
		paddr = ess_items[i].entry.paddr;
		vaddr = ess_items[i].entry.vaddr;
		size = ess_items[i].entry.size;

		if (!strncmp(ess_items[i].name, "log_kernel", strlen(ess_items[i].name))) {
			/*  load last_buf address value(phy) by virt address */
			last_buf = (size_t)__raw_readl(S5P_VA_SS_LAST_LOGBUF);
			align = (last_buf & 0xFF000000);

			/*  check physical address offset of kernel logbuf */
			if (align == (size_t)(paddr & 0xFF000000)) {
				/*  assumed valid address, conversion to virt */
				ess_items[i].curr_ptr = (unsigned char *)
						((last_buf & UL(SZ_16M - 1)) |
						(size_t)vaddr);
			} else {
				/*  invalid address, set to first line */
				ess_items[i].curr_ptr = (unsigned char *)vaddr;
				/*  initialize logbuf to 0 */
				memset((size_t *)vaddr, 0, size);
			}
		} else {
			/*  initialized log to 0 */
			memset((size_t *)vaddr, 0, size);
		}
		ess_info.info_log[i - 1].name = kstrdup(ess_items[i].name, GFP_KERNEL);
		ess_info.info_log[i - 1].head_ptr =
			(unsigned char *)((PAGE_OFFSET | (UL(SZ_256M - 1) & paddr)));
		ess_info.info_log[i - 1].curr_ptr = NULL;
		ess_info.info_log[i - 1].entry.size = size;
	}
	exynos_ss_output();
	return 0;
}

static int __init exynos_ss_init(void)
{
	if (ess_base.vaddr && ess_base.paddr) {
	/*
	 *  for debugging when we don't know the virtual address of pointer,
	 *  In just privous the debug buffer, It is added 16byte dummy data.
	 *  start address(dummy 16bytes)
	 *  --> @virtual_addr | @phy_addr | @buffer_size | @magic_key(0xDBDBDBDB)
	 *  And then, the debug buffer is shown.
	 */
		exynos_ss_fixmap();

		exynos_ss_scratch_reg(CAUSE_FORCE_DUMP);
		exynos_ss_set_enable("base", true);

		register_hook_logbuf(exynos_ss_hook_logbuf);

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
		register_hook_logger(exynos_ss_hook_logger);
#endif
		register_reboot_notifier(&nb_reboot_block);
		atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	} else
		pr_err("exynos-snapshot: %s failed\n", __func__);

	return 0;
}
early_initcall(exynos_ss_init);

void __exynos_ss_task(int cpu, struct task_struct *task)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	unsigned i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	i = atomic_inc_return(&ess_idx.task_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->task[0]) - 1);

	ess_log->task[cpu][i].time = cpu_clock(cpu);
	strncpy(ess_log->task[cpu][i].comm, task->comm, TASK_COMM_LEN - 1);
	ess_log->task[cpu][i].pid = task->pid;
}

void __exynos_ss_work(struct worker *worker, struct work_struct *work,
			work_func_t f, int en)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	i = atomic_inc_return(&ess_idx.work_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->work[0]) - 1);

	ess_log->work[cpu][i].time = cpu_clock(cpu);
	ess_log->work[cpu][i].worker = worker;
	ess_log->work[cpu][i].work = work;
	ess_log->work[cpu][i].f = f;
	ess_log->work[cpu][i].en = en;
}

void __exynos_ss_clockevent(unsigned long long clc, int64_t delta, ktime_t next_event)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	i = atomic_inc_return(&ess_idx.clockevent_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->clockevent[0]) - 1);

	ess_log->clockevent[cpu][i].time = cpu_clock(cpu);
	ess_log->clockevent[cpu][i].clc = clc;
	ess_log->clockevent[cpu][i].delta = delta;
	ess_log->clockevent[cpu][i].next_event = next_event;
}

void __exynos_ss_cpuidle(int index, unsigned state, int diff, int en)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	i = atomic_inc_return(&ess_idx.cpuidle_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->cpuidle[0]) - 1);

	ess_log->cpuidle[cpu][i].time = cpu_clock(cpu);
	ess_log->cpuidle[cpu][i].index = index;
	ess_log->cpuidle[cpu][i].state = state;
	ess_log->cpuidle[cpu][i].num_online_cpus = num_online_cpus();
	ess_log->cpuidle[cpu][i].delta = diff;
	ess_log->cpuidle[cpu][i].en = en;
}

void __exynos_ss_suspend(void *fn, struct device *dev, int en)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	i = atomic_inc_return(&ess_idx.suspend_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->suspend[0]) - 1);

	ess_log->suspend[cpu][i].time = cpu_clock(cpu);
	ess_log->suspend[cpu][i].fn = fn;
	ess_log->suspend[cpu][i].dev = dev;
	ess_log->suspend[cpu][i].en = en;
}

void __exynos_ss_irq(unsigned int irq, void *fn, int curr_disabled, int en)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++)
		if (irq == 0 || irq == ess_irqlog_exlist[i])
			return;

	i = atomic_inc_return(&ess_idx.irq_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->irq[0]) - 1);

	ess_log->irq[cpu][i].time = cpu_clock(cpu);
	ess_log->irq[cpu][i].irq = irq;
	ess_log->irq[cpu][i].fn = (void *)fn;
	ess_log->irq[cpu][i].preempt = preempt_count();
	ess_log->irq[cpu][i].curr_disabled = curr_disabled;
	ess_log->irq[cpu][i].en = en;
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
void __exynos_ss_irq_exit(unsigned int irq, unsigned long long start_time)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned long long time, latency;
	unsigned i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++)
		if (irq == 0 || irq == ess_irqexit_exlist[i])
			return;

	i = atomic_inc_return(&ess_idx.irq_exit_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->irq_exit[0]) - 1);

	time = cpu_clock(cpu);
	latency = time - start_time;

	if (unlikely(latency >
		(ess_irqexit_threshold * 1000))) {
		ess_log->irq_exit[cpu][i].latency = latency;
		ess_log->irq_exit[cpu][i].end_time = time;
		ess_log->irq_exit[cpu][i].time = start_time;
		ess_log->irq_exit[cpu][i].irq = irq;
	} else
		atomic_dec(&ess_idx.irq_exit_log_idx[cpu]);
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
static inline void __exynos_ss_irqs_disabled(unsigned curr_disabled, int try_disabled)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned long long latency = 0;
	unsigned long long time = 0;
	long prev;
	unsigned i;
#ifndef CONFIG_ARM64
	unsigned j;
#endif

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	prev = atomic_read(&ess_idx.irqs_disabled_log_idx[cpu]);
	if (likely(prev > -1)) {
		prev = ((unsigned long)prev & (ARRAY_SIZE(ess_log->irqs_disabled[0]) - 1));
		if (unlikely(ess_log->irqs_disabled[cpu][prev].try_disabled == try_disabled))
			return;
		time = cpu_clock(cpu);
		if (!try_disabled) {
			latency = time - ess_log->irqs_disabled[cpu][prev].time;
			/* remove status under threshold */
			if (likely(latency < ess_irqdisabled_threshold * 1000))
				return;
		}
	} else {
		/* We need only irq disable function at the first time */
		if (!try_disabled)
			return;
		time = cpu_clock(cpu);
	}

	/* It should be saved to the new array */
	i = atomic_inc_return(&ess_idx.irqs_disabled_log_idx[cpu]) &
		    (ARRAY_SIZE(ess_log->irqs_disabled[0]) - 1);

	ess_log->irqs_disabled[cpu][i].latency = latency;
	ess_log->irqs_disabled[cpu][i].time = time;
	ess_log->irqs_disabled[cpu][i].try_disabled = try_disabled;
	ess_log->irqs_disabled[cpu][i].curr_disabled = curr_disabled;

#ifndef CONFIG_ARM64
	for (j = 0; j < ess_callstack; j++) {
		ess_log->irqs_disabled[cpu][i].caller[j] =
				(void *)(return_address(j + 1) - sizeof(size_t));
	}
#endif
}

#ifdef CONFIG_ARM64
void arch_local_irq_restore(unsigned long flags)
{
	unsigned irqs_disabled = irqs_disabled();


	if (flags & PSR_I_BIT) {
		asm volatile(
			"msr	daif, %0		// arch_local_irq_restore"
		:
		: "r" (flags)
		: "memory");

		__exynos_ss_irqs_disabled(irqs_disabled, 1);
	} else {
		__exynos_ss_irqs_disabled(irqs_disabled, 0);

		asm volatile(
			"msr	daif, %0		// arch_local_irq_restore"
		:
		: "r" (flags)
		: "memory");
	}
}

unsigned long arch_local_irq_save(void)
{
	unsigned long flags;
	unsigned irqs_disabled = irqs_disabled();

	asm volatile(
		"mrs	%0, daif		// arch_local_irq_save\n"
		"msr	daifset, #2"
		: "=r" (flags)
		:
		: "memory");

	__exynos_ss_irqs_disabled(irqs_disabled, 1);

	return flags;
}

void arch_local_irq_enable(void)
{
	__exynos_ss_irqs_disabled(irqs_disabled(), 0);

	asm volatile(
		"msr	daifclr, #2		// arch_local_irq_enable"
		:
		:
		: "memory");
}

void arch_local_irq_disable(void)
{
	unsigned irqs_disabled = irqs_disabled();

	asm volatile(
		"msr	daifset, #2		// arch_local_irq_disable"
		:
		:
		: "memory");

	__exynos_ss_irqs_disabled(irqs_disabled, 1);
}

#else
void arch_local_irq_restore(unsigned long flags)
{
	unsigned irqs_disabled = irqs_disabled();

	if (flags & PSR_I_BIT) {
		asm volatile(
			"	msr	cpsr_c, %0	@ local_irq_restore"
			:
			: "r" (flags)
			: "memory", "cc");

		__exynos_ss_irqs_disabled(irqs_disabled, 1);
	} else {
		__exynos_ss_irqs_disabled(irqs_disabled, 0);

		asm volatile(
			"	msr	cpsr_c, %0	@ local_irq_restore"
			:
			: "r" (flags)
			: "memory", "cc");
	}
}

unsigned long arch_local_irq_save(void)
{
	unsigned long flags;
	unsigned irqs_disabled = irqs_disabled();

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	cpsid	i"
		: "=r" (flags) : : "memory", "cc");

	__exynos_ss_irqs_disabled(irqs_disabled, 1);

	return flags;
}

void arch_local_irq_disable(void)
{
	unsigned irqs_disabled = irqs_disabled();

	asm volatile(
		"	cpsid i			@ arch_local_irq_disable"
		:
		:
		: "memory", "cc");

	__exynos_ss_irqs_disabled(irqs_disabled, 1);
}

void arch_local_irq_enable(void)
{
	__exynos_ss_irqs_disabled(irqs_disabled(), 0);

	asm volatile(
		"	cpsie i			@ arch_local_irq_enable"
		:
		:
		: "memory", "cc");
}
#endif
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
void __exynos_ss_hrtimer(struct hrtimer *timer, s64 *now,
		     enum hrtimer_restart (*fn) (struct hrtimer *), int en)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	i = atomic_inc_return(&ess_idx.hrtimer_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->hrtimers[0]) - 1);
	ess_log->hrtimers[cpu][i].time = cpu_clock(cpu);
	ess_log->hrtimers[cpu][i].now = *now;
	ess_log->hrtimers[cpu][i].timer = timer;
	ess_log->hrtimers[cpu][i].fn = fn;
	ess_log->hrtimers[cpu][i].en = en;
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static phys_addr_t virt_to_phys_high(size_t vaddr)
{
	phys_addr_t paddr = 0;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	if (virt_addr_valid((void *) vaddr)) {
		paddr = virt_to_phys((void *) vaddr);
		goto out;
	}

	pgd = pgd_offset_k(vaddr);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		goto out;

	if (pgd_val(*pgd) & 2) {
		paddr = pgd_val(*pgd) & SECTION_MASK;
		goto out;
	}

	pmd = pmd_offset((pud_t *)pgd, vaddr);
	if (pmd_none_or_clear_bad(pmd))
		goto out;

	pte = pte_offset_kernel(pmd, vaddr);
	if (pte_none(*pte))
		goto out;

	paddr = pte_val(*pte) & PAGE_MASK;

out:
	return paddr | (vaddr & UL(SZ_4K - 1));
}

void __exynos_ss_reg(unsigned int read, size_t val,
			size_t reg, int en)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned i, j;
	size_t phys_reg, start_addr, end_addr;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	if (ess_reg_exlist[0].addr == 0)
		return;

	phys_reg = virt_to_phys_high(reg);
	if (unlikely(!phys_reg))
		return;

	for (j = 0; j < ARRAY_SIZE(ess_reg_exlist); j++) {
		if (ess_reg_exlist[j].addr == 0)
			break;
		start_addr = ess_reg_exlist[j].addr;
		end_addr = start_addr + ess_reg_exlist[j].size;
		if (start_addr <= phys_reg && phys_reg <= end_addr)
			return;
	}

	i = atomic_inc_return(&ess_idx.reg_log_idx[cpu]) &
		(ARRAY_SIZE(ess_log->reg[0]) - 1);

	ess_log->reg[cpu][i].time = cpu_clock(cpu);
	ess_log->reg[cpu][i].read = read;
	ess_log->reg[cpu][i].val = val;
	ess_log->reg[cpu][i].reg = phys_reg;
	ess_log->reg[cpu][i].en = en;

#ifndef CONFIG_ARM64
	for (j = 0; j < ess_callstack; j++) {
		if (unlikely(j == 0))
			ess_log->reg[cpu][i].caller[j] =
				(void *)((size_t)__builtin_return_address(0) - sizeof(size_t));
		else
			ess_log->reg[cpu][i].caller[j] =
				(void *)((size_t)return_address(j) - sizeof(size_t));
	}
#endif
}
#endif

void exynos_ss_printk(const char *fmt, ...)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	va_list args;
	int ret;
	unsigned i;
#ifndef CONFIG_ARM64
	unsigned j;
#endif

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	i = atomic_inc_return(&ess_idx.printk_log_idx) &
	    (ARRAY_SIZE(ess_log->printk) - 1);

	va_start(args, fmt);
	ret = vsnprintf(ess_log->printk[i].log,
			sizeof(ess_log->printk[i].log), fmt, args);
	va_end(args);

	ess_log->printk[i].time = cpu_clock(cpu);
	ess_log->printk[i].cpu = cpu;

#ifndef CONFIG_ARM64
	for (j = 0; j < ess_callstack; j++) {
		if (unlikely(j == 0))
			ess_log->printk[i].caller[j] =
				(void *)((size_t)__builtin_return_address(0) - sizeof(size_t));
		else
			ess_log->printk[i].caller[j] =
				(void *)((size_t)return_address(j) - sizeof(size_t));
	}
#endif
}

void exynos_ss_printkl(size_t msg, size_t val)
{
	struct exynos_ss_item *item = &ess_items[ESS_ITEMS_KEVENTS];
	int cpu = raw_smp_processor_id();
	unsigned i;
#ifndef CONFIG_ARM64
	unsigned j;
#endif

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	i = atomic_inc_return(&ess_idx.printkl_log_idx) &
	    (ARRAY_SIZE(ess_log->printkl) - 1);

	ess_log->printkl[i].time = cpu_clock(cpu);
	ess_log->printkl[i].cpu = cpu;
	ess_log->printkl[i].msg = msg;
	ess_log->printkl[i].val = val;

#ifndef CONFIG_ARM64
	for (j = 0; j < ess_callstack; j++) {
		if (unlikely(j == 0))
			ess_log->printkl[i].caller[j] =
				(void *)((size_t)__builtin_return_address(0) - sizeof(size_t));
		else
			ess_log->printkl[i].caller[j] =
				(void *)((size_t)return_address(j) - sizeof(size_t));
	}
#endif
}

/*
 *  sysfs implementation for exynos-snapshot
 *  you can access the sysfs of exynos-snapshot to /sys/devices/system/exynos-ss
 *  path.
 */
static struct bus_type ess_subsys = {
	.name = "exynos-ss",
	.dev_name = "exynos-ss",
};

static ssize_t ess_enable_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	struct exynos_ss_item *item;
	unsigned i;
	ssize_t n = 0;

	/*  item  */
	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		item = &ess_items[i];
		n += scnprintf(buf + n, 24, "%-12s : %sable\n",
			item->name, item->entry.enabled ? "en" : "dis");
        }

	/*  base  */
	n += scnprintf(buf + n, 24, "%-12s : %sable\n",
			"base", ess_base.enabled ? "en" : "dis");

	return n;
}

static ssize_t ess_enable_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int en;
	char *name;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	name[count - 1] = '\0';

	en = exynos_ss_get_enable(name);

	if (en == -1)
		pr_info("echo name > enabled\n");
	else {
		if (en)
			exynos_ss_set_enable(name, false);
		else
			exynos_ss_set_enable(name, true);
	}

	kfree(name);
	return count;
}

static ssize_t ess_callstack_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n = scnprintf(buf, 24, "callstack depth : %d\n", ess_callstack);

	return n;
}

static ssize_t ess_callstack_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long callstack;

	callstack = simple_strtoul(buf, NULL, 0);
	pr_info("callstack depth(min 1, max 4) : %lu\n", callstack);

	if (callstack < 5 && callstack > 0) {
		ess_callstack = callstack;
		pr_info("success inserting %lu to callstack value\n", callstack);
	}
	return count;
}

static ssize_t ess_irqlog_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned i;
	ssize_t n = 0;

	n = scnprintf(buf, 24, "excluded irq number\n");

	for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++) {
		if (ess_irqlog_exlist[i] == 0)
			break;
		n += scnprintf(buf + n, 24, "irq num: %-4d\n", ess_irqlog_exlist[i]);
        }
	return n;
}

static ssize_t ess_irqlog_exlist_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned i;
	unsigned long irq;

	irq = simple_strtoul(buf, NULL, 0);
	pr_info("irq number : %lu\n", irq);

	for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++) {
		if (ess_irqlog_exlist[i] == 0)
			break;
	}

	if (i == ARRAY_SIZE(ess_irqlog_exlist)) {
		pr_err("list is full\n");
		return count;
	}

	if (irq != 0) {
		ess_irqlog_exlist[i] = irq;
		pr_info("success inserting %lu to list\n", irq);
	}
	return count;
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static ssize_t ess_irqexit_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned i;
	ssize_t n = 0;

	n = scnprintf(buf, 36, "Excluded irq number\n");
	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++) {
		if (ess_irqexit_exlist[i] == 0)
			break;
		n += scnprintf(buf + n, 24, "IRQ num: %-4d\n", ess_irqexit_exlist[i]);
        }
	return n;
}

static ssize_t ess_irqexit_exlist_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned i;
	unsigned long irq;

	irq = simple_strtoul(buf, NULL, 0);
	pr_info("irq number : %lu\n", irq);

	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++) {
		if (ess_irqexit_exlist[i] == 0)
			break;
	}

	if (i == ARRAY_SIZE(ess_irqexit_exlist)) {
		pr_err("list is full\n");
		return count;
	}

	if (irq != 0) {
		ess_irqexit_exlist[i] = irq;
		pr_info("success inserting %lu to list\n", irq);
	}
	return count;
}

static ssize_t ess_irqexit_threshold_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	ssize_t n;

	n = scnprintf(buf, 46, "threshold : %12u us\n", ess_irqexit_threshold);
	return n;
}

static ssize_t ess_irqexit_threshold_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;

	val = simple_strtoul(buf, NULL, 0);
	pr_info("threshold value : %lu\n", val);

	if (val != 0) {
		ess_irqexit_threshold = val;
		pr_info("success %lu to threshold\n", val);
	}
	return count;
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
static ssize_t ess_irqdisabled_threshold_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	ssize_t n;

	n = scnprintf(buf, 46, "threshold : %12u us\n",
				ess_irqdisabled_threshold);
	return n;
}

static ssize_t ess_irqdisabled_threshold_store(struct kobject *kobj,
			struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;

	val = simple_strtoul(buf, NULL, 0);
	pr_info("threshold value : %lu\n", val);

	if (val != 0) {
		ess_irqdisabled_threshold = val;
		pr_info("success %lu to threshold\n", val);
	}
	return count;
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static ssize_t ess_reg_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned i;
	ssize_t n = 0;

	n = scnprintf(buf, 36, "excluded register address\n");
	for (i = 0; i < ARRAY_SIZE(ess_reg_exlist); i++) {
		if (ess_reg_exlist[i].addr == 0)
			break;
		n += scnprintf(buf + n, 40, "register addr: %08x size: %08x\n",
				ess_reg_exlist[i].addr, ess_reg_exlist[i].size);
        }
	return n;
}

static ssize_t ess_reg_exlist_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned i;
	size_t addr;

	addr = simple_strtoul(buf, NULL, 0);
	pr_info("register addr: %zx\n", addr);

	for (i = 0; i < ARRAY_SIZE(ess_reg_exlist); i++) {
		if (ess_reg_exlist[i].addr == 0)
			break;
	}
	if (addr != 0) {
		ess_reg_exlist[i].size = SZ_4K;
		ess_reg_exlist[i].addr = addr;
		pr_info("success %zx to threshold\n", (addr);
	}
	return count;
}
#endif

static struct kobj_attribute ess_enable_attr =
        __ATTR(enabled, 0644, ess_enable_show, ess_enable_store);
static struct kobj_attribute ess_callstack_attr =
        __ATTR(callstack, 0644, ess_callstack_show, ess_callstack_store);
static struct kobj_attribute ess_irqlog_attr =
        __ATTR(exlist_irqdisabled, 0644, ess_irqlog_exlist_show,
					ess_irqlog_exlist_store);
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static struct kobj_attribute ess_irqexit_attr =
        __ATTR(exlist_irqexit, 0644, ess_irqexit_exlist_show,
					ess_irqexit_exlist_store);
static struct kobj_attribute ess_irqexit_threshold_attr =
        __ATTR(threshold_irqexit, 0644, ess_irqexit_threshold_show,
					ess_irqexit_threshold_store);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
static struct kobj_attribute ess_irqdisabled_threshold_attr =
        __ATTR(threshold_irqdisabled, 0644, ess_irqdisabled_threshold_show,
					ess_irqdisabled_threshold_store);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static struct kobj_attribute ess_reg_attr =
        __ATTR(exlist_reg, 0644, ess_reg_exlist_show, ess_reg_exlist_store);
#endif

static struct attribute *ess_sysfs_attrs[] = {
	&ess_enable_attr.attr,
	&ess_callstack_attr.attr,
	&ess_irqlog_attr.attr,
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	&ess_irqexit_attr.attr,
	&ess_irqexit_threshold_attr.attr,
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
	&ess_irqdisabled_threshold_attr.attr,
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	&ess_reg_attr.attr,
#endif
	NULL,
};

static struct attribute_group ess_sysfs_group = {
	.attrs = ess_sysfs_attrs,
};

static const struct attribute_group *ess_sysfs_groups[] = {
	&ess_sysfs_group,
	NULL,
};

static int __init exynos_ss_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&ess_subsys, ess_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos-snapshop subsys\n");

	return ret;
}
late_initcall(exynos_ss_sysfs_init);
#endif
