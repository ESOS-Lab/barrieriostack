/*
 *
 * (C) COPYRIGHT 2011-2013 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * @file mali_kbase_js_policy_cfs.h
 * Completely Fair Job Scheduler Policy structure definitions
 */

#ifndef _KBASE_JS_POLICY_CFS_H_
#define _KBASE_JS_POLICY_CFS_H_

#define KBASE_JS_POLICY_AVAILABLE_CFS

/** @addtogroup base_api
 * @{ */
/** @addtogroup base_kbase_api
 * @{ */
/** @addtogroup kbase_js_policy
 * @{ */

/**
 * Internally, this policy keeps a few internal queues for different variants
 * of core requirements, which are used to decide how to schedule onto the
 * different job slots.
 *
 * Must be a power of 2 to keep the lookup math simple
 */
#define KBASEP_JS_MAX_NR_CORE_REQ_VARIANTS_LOG2 3
#define KBASEP_JS_MAX_NR_CORE_REQ_VARIANTS      (1u << KBASEP_JS_MAX_NR_CORE_REQ_VARIANTS_LOG2)

/** Bits needed in the lookup to support all slots */
#define KBASEP_JS_VARIANT_LOOKUP_BITS_NEEDED (BASE_JM_MAX_NR_SLOTS * KBASEP_JS_MAX_NR_CORE_REQ_VARIANTS)
/** Number of u32s needed in the lookup array to support all slots */
#define KBASEP_JS_VARIANT_LOOKUP_WORDS_NEEDED ((KBASEP_JS_VARIANT_LOOKUP_BITS_NEEDED + 31) / 32)

#define KBASEP_JS_RUNTIME_EMPTY 	((u64)-1)

typedef struct kbasep_js_policy_cfs {
	/** List of all contexts in the context queue. Hold
	 * kbasep_js_device_data::queue_mutex whilst accessing. */
	struct list_head ctx_queue_head;

	/** List of all contexts in the realtime (priority) context queue */
	struct list_head ctx_rt_queue_head;

	/** List of scheduled contexts. Hold kbasep_jd_device_data::runpool_irq::lock
	 * whilst accessing, which is a spinlock */
	struct list_head scheduled_ctxs_head;

	/** Number of valid elements in the core_req_variants member, and the
	 * kbasep_js_policy_rr_ctx::job_list_head array */
	u32 num_core_req_variants;

	/** Variants of the core requirements */
	kbasep_atom_req core_req_variants[KBASEP_JS_MAX_NR_CORE_REQ_VARIANTS];

	/* Lookups per job slot against which core_req_variants match it */
	u32 slot_to_variant_lookup_ss_state[KBASEP_JS_VARIANT_LOOKUP_WORDS_NEEDED];
	u32 slot_to_variant_lookup_ss_allcore_state[KBASEP_JS_VARIANT_LOOKUP_WORDS_NEEDED];

	/* The timer tick used for rescheduling jobs */
	struct hrtimer scheduling_timer;

	/* Is the timer running?
	 *
	 * The kbasep_js_device_data::runpool_mutex must be held whilst modifying this.  */
	mali_bool timer_running;

	/* Number of us the least-run context has been running for
	 *
	 * The kbasep_js_device_data::queue_mutex must be held whilst updating this
	 * Reads are possible without this mutex, but an older value might be read
	 * if no memory barriers are issued beforehand. */
	u64 head_runtime_us;

	/* Number of us the least-run context in the context queue has been running for.
	 * -1 if context queue is empty. */
	atomic64_t least_runtime_us;

	/* Number of us the least-run context in the realtime (priority) context queue
	 * has been running for. -1 if realtime context queue is empty. */
	atomic64_t rt_least_runtime_us;
} kbasep_js_policy_cfs;

/**
 * This policy contains a single linked list of all contexts.
 */
typedef struct kbasep_js_policy_cfs_ctx {
	/** Link implementing the Policy's Queue, and Currently Scheduled list */
	struct list_head list;

	/** Job lists for use when in the Run Pool - only using
	 * kbasep_js_policy_fcfs::num_unique_slots of them. We still need to track
	 * the jobs when we're not in the runpool, so this member is accessed from
	 * outside the policy queue (for the first job), inside the policy queue,
	 * and inside the runpool.
	 *
	 * If the context is in the runpool, then this must only be accessed with
	 * kbasep_js_device_data::runpool_irq::lock held
	 *
	 * Jobs are still added to this list even when the context is not in the
	 * runpool. In that case, the kbasep_js_kctx_info::ctx::jsctx_mutex must be
	 * held before accessing this. */
	struct list_head job_list_head[KBASEP_JS_MAX_NR_CORE_REQ_VARIANTS];

	/** Number of us this context has been running for
	 *
	 * The kbasep_js_device_data::runpool_irq::lock (a spinlock) must be held
	 * whilst updating this. Initializing will occur on context init and
	 * context enqueue (which can only occur in one thread at a time), but
	 * multi-thread access only occurs while the context is in the runpool.
	 *
	 * Reads are possible without this spinlock, but an older value might be read
	 * if no memory barriers are issued beforehand */
	u64 runtime_us;

	/* Calling process policy scheme is a realtime scheduler and will use the priority queue
	 * Non-mutable after ctx init */
	mali_bool process_rt_policy;
	/* Calling process NICE priority */
	int process_priority;
	/* Average NICE priority of all atoms in bag:
	 * Hold the kbasep_js_kctx_info::ctx::jsctx_mutex when accessing  */
	int bag_priority;
	/* Total NICE priority of all atoms in bag
	 * Hold the kbasep_js_kctx_info::ctx::jsctx_mutex when accessing  */
	int bag_total_priority;
	/* Total number of atoms in the bag
	 * Hold the kbasep_js_kctx_info::ctx::jsctx_mutex when accessing  */
	int bag_total_nr_atoms;

} kbasep_js_policy_cfs_ctx;

/**
 * In this policy, each Job is part of at most one of the per_corereq lists
 */
typedef struct kbasep_js_policy_cfs_job {
	struct list_head list;	    /**< Link implementing the Run Pool list/Jobs owned by the ctx */
	u32 cached_variant_idx;	  /**< Cached index of the list this should be entered into on re-queue */

	/** Number of ticks that this job has been executing for
	 *
	 * To access this, the kbasep_js_device_data::runpool_irq::lock must be held */
	u32 ticks;
} kbasep_js_policy_cfs_job;

	  /** @} *//* end group kbase_js_policy */
	  /** @} *//* end group base_kbase_api */
	  /** @} *//* end group base_api */

#endif
