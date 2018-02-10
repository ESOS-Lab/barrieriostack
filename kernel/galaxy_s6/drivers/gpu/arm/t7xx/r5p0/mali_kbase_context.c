/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
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
 * @file mali_kbase_context.c
 * Base kernel context APIs
 */

#include <mali_kbase.h>
#include <mali_midg_regmap.h>

#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <platform/mali_kbase_platform.h>
#include <platform/gpu_dvfs_handler.h>

#ifdef MALI_SEC_HWCNT
#include <platform/gpu_hwcnt.h>
#endif

#define MEMPOOL_PAGES 16384


/**
 * @brief Create a kernel base context.
 *
 * Allocate and init a kernel base context.
 */
struct kbase_context *
kbase_create_context(struct kbase_device *kbdev, bool is_compat)
{
	struct kbase_context *kctx;
	mali_error mali_err;
	/* MALI_SEC_INTEGRATION */
	char current_name[sizeof(current->comm)];

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	/* zero-inited as lot of code assume it's zero'ed out on create */
	kctx = vzalloc(sizeof(*kctx));

	if (!kctx)
		goto out;

	/* creating a context is considered a disjoint event */
	kbase_disjoint_event(kbdev);

	kctx->kbdev = kbdev;
	kctx->as_nr = KBASEP_AS_NR_INVALID;
	kctx->is_compat = is_compat;
	/* MALI_SEC_INTEGRATION */
	kctx->ctx_status = CTX_UNINITIALIZED;
	kctx->ctx_need_qos = false;
#ifdef CONFIG_MALI_TRACE_TIMELINE
	kctx->timeline.owner_tgid = task_tgid_nr(current);
#endif
	atomic_set(&kctx->setup_complete, 0);
	atomic_set(&kctx->setup_in_progress, 0);
	kctx->keep_gpu_powered = MALI_FALSE;
	spin_lock_init(&kctx->mm_update_lock);
	kctx->process_mm = NULL;
	atomic_set(&kctx->nonmapped_pages, 0);
	/* MALI_SEC_INTEGRATION */
	get_task_comm(current_name, current);
	strncpy((char *)(&kctx->name), current_name, CTX_NAME_SIZE);

	if (MALI_ERROR_NONE != kbase_mem_allocator_init(&kctx->osalloc,
							MEMPOOL_PAGES,
							kctx->kbdev))
		goto free_kctx;

	kctx->pgd_allocator = &kctx->osalloc;
	atomic_set(&kctx->used_pages, 0);
	/* MALI_SEC_INTEGRATION */
	atomic_set(&kctx->used_pmem_pages, 0);
	atomic_set(&kctx->used_tmem_pages, 0);

	if (kbase_jd_init(kctx))
		goto free_allocator;

	mali_err = kbasep_js_kctx_init(kctx);
	if (MALI_ERROR_NONE != mali_err)
		goto free_jd;	/* safe to call kbasep_js_kctx_term  in this case */

	mali_err = kbase_event_init(kctx);
	if (MALI_ERROR_NONE != mali_err)
		goto free_jd;

	mutex_init(&kctx->reg_lock);

	INIT_LIST_HEAD(&kctx->waiting_soft_jobs);
#ifdef CONFIG_KDS
	INIT_LIST_HEAD(&kctx->waiting_kds_resource);
#endif

	mali_err = kbase_mmu_init(kctx);
	if (MALI_ERROR_NONE != mali_err)
		goto free_event;

	kctx->pgd = kbase_mmu_alloc_pgd(kctx);
	if (!kctx->pgd)
		goto free_mmu;

	if (MALI_ERROR_NONE != kbase_mem_allocator_alloc(&kctx->osalloc, 1, &kctx->aliasing_sink_page))
		goto no_sink_page;

	kctx->tgid = current->tgid;
	kctx->pid = current->pid; 
	init_waitqueue_head(&kctx->event_queue);

	kctx->cookies = KBASE_COOKIE_MASK;

	/* Make sure page 0 is not used... */
	if (kbase_region_tracker_init(kctx))
		goto no_region_tracker;
#ifdef CONFIG_GPU_TRACEPOINTS
	atomic_set(&kctx->jctx.work_id, 0);
#endif
#ifdef CONFIG_MALI_TRACE_TIMELINE
	atomic_set(&kctx->timeline.jd_atoms_in_flight, 0);
#endif

	kctx->id = atomic_add_return(1, &(kbdev->ctx_num)) - 1;

	mali_err = kbasep_mem_profile_debugfs_add(kctx);
	if (MALI_ERROR_NONE != mali_err)
		goto no_region_tracker;

	if (kbasep_jd_debugfs_ctx_add(kctx))
		goto free_mem_profile;

	/* MALI_SEC_INTEGRATION */
	kctx->ctx_status = CTX_INITIALIZED;

	/* MALI_SEC_SECURE_RENDERING */
	kctx->enabled_TZASC = MALI_FALSE;

	/* MALI_SEC_INTEGRATION */
	kctx->destroying_context = MALI_FALSE;

	return kctx;

free_mem_profile:
	kbasep_mem_profile_debugfs_remove(kctx);
no_region_tracker:
no_sink_page:
	kbase_mem_allocator_free(&kctx->osalloc, 1, &kctx->aliasing_sink_page, 0);
	kbase_mmu_free_pgd(kctx);
free_mmu:
	kbase_mmu_term(kctx);
free_event:
	kbase_event_cleanup(kctx);
free_jd:
	/* Safe to call this one even when didn't initialize (assuming kctx was sufficiently zeroed) */
	kbasep_js_kctx_term(kctx);
	kbase_jd_exit(kctx);
free_allocator:
	kbase_mem_allocator_term(&kctx->osalloc);
free_kctx:
	vfree(kctx);
out:
	return NULL;

}
KBASE_EXPORT_SYMBOL(kbase_create_context)

static void kbase_reg_pending_dtor(struct kbase_va_region *reg)
{
	dev_dbg(reg->kctx->kbdev->dev, "Freeing pending unmapped region\n");
	kbase_mem_phy_alloc_put(reg->alloc);
	kfree(reg);
}

/**
 * @brief Destroy a kernel base context.
 *
 * Destroy a kernel base context. Calls kbase_destroy_os_context() to
 * free OS specific structures. Will release all outstanding regions.
 */
void kbase_destroy_context(struct kbase_context *kctx)
{
	struct kbase_device *kbdev;
	int pages;
	unsigned long pending_regions_to_clean;
	/* MALI_SEC_INTEGRATION */
	if (!kctx) {
		printk("An uninitialized or destroyed context is tried to be destroyed. kctx is null\n");
		return ;
	}
	else if (kctx->ctx_status != CTX_INITIALIZED) {
		printk("An uninitialized or destroyed context is tried to be destroyed\n");
		printk("kctx: 0x%p, kctx->tgid: %d, kctx->ctx_status: 0x%x\n", kctx, kctx->tgid, kctx->ctx_status);
		return ;
	}

	KBASE_DEBUG_ASSERT(NULL != kctx);

	kbdev = kctx->kbdev;
	KBASE_DEBUG_ASSERT(NULL != kbdev);

	KBASE_TRACE_ADD(kbdev, CORE_CTX_DESTROY, kctx, NULL, 0u, 0u);

	/* MALI_SEC_INTEGRATION */
	kctx->destroying_context = MALI_TRUE;

	/* MALI_SEC_SECURE_RENDERING */
	if (kctx->enabled_TZASC == MALI_TRUE &&
		kbdev->js_data.secure_ops != NULL &&
		kbdev->js_data.secure_ops->secure_mem_disable != NULL) {
		int res = 0;

		/* Switch GPU to non-secure world mode */
		res = kbdev->js_data.secure_ops->secure_mem_disable();
		if (res == SMC_CALL_ERROR)
			dev_err(kbdev->dev, "G3D - context : cannot disable the protection mode.\n");
		else
			printk("[G3D] - context : disable the protection mode, kctx : %p\n", kctx);
		BUG_ON(res == SMC_CALL_ERROR);
		kctx->enabled_TZASC = MALI_FALSE;
	}

	kbasep_jd_debugfs_ctx_remove(kctx);

	kbasep_mem_profile_debugfs_remove(kctx);

	/* Ensure the core is powered up for the destroy process */
	/* A suspend won't happen here, because we're in a syscall from a userspace
	 * thread. */
	kbase_pm_context_active(kbdev);

	if (kbdev->hwcnt.kctx == kctx) {
		/* disable the use of the hw counters if the app didn't use the API correctly or crashed */
		KBASE_TRACE_ADD(kbdev, CORE_CTX_HWINSTR_TERM, kctx, NULL, 0u, 0u);
		dev_warn(kbdev->dev, "The privileged process asking for instrumentation forgot to disable it " "before exiting. Will end instrumentation for them");
		kbase_instr_hwcnt_disable(kctx);
	}
#ifdef MALI_SEC_HWCNT
	else if (kbdev->hwcnt.kctx_gpr == kctx) {
		if (kbdev->hwcnt.is_init) {
			kbdev->hwcnt.triggered = 1;
			kbdev->hwcnt.trig_exception = 1;
			wake_up(&kbdev->hwcnt.wait);

			mutex_lock(&kbdev->hwcnt.mlock);

			if (kbdev->hwcnt.kctx) {
				kbdev->hwcnt.state = KBASE_INSTR_STATE_IDLE;
				hwcnt_stop(kbdev);
			}

			kbdev->hwcnt.enable_for_gpr = FALSE;
			kbdev->hwcnt.enable_for_utilization = kbdev->hwcnt.s_enable_for_utilization;
			kbdev->hwcnt.kctx_gpr = NULL;

			mutex_unlock(&kbdev->hwcnt.mlock);
		}
	}
#endif

	kbase_jd_zap_context(kctx);
	kbase_event_cleanup(kctx);

	kbase_gpu_vm_lock(kctx);

	/* MMU is disabled as part of scheduling out the context */
	kbase_mmu_free_pgd(kctx);

	/* drop the aliasing sink page now that it can't be mapped anymore */
	kbase_mem_allocator_free(&kctx->osalloc, 1, &kctx->aliasing_sink_page, 0);

	/* free pending region setups */
	pending_regions_to_clean = (~kctx->cookies) & KBASE_COOKIE_MASK;
	while (pending_regions_to_clean) {
		unsigned int cookie = __ffs(pending_regions_to_clean);
		BUG_ON(!kctx->pending_regions[cookie]);

		kbase_reg_pending_dtor(kctx->pending_regions[cookie]);

		kctx->pending_regions[cookie] = NULL;
		pending_regions_to_clean &= ~(1UL << cookie);
	}

	kbase_region_tracker_term(kctx);
	kbase_gpu_vm_unlock(kctx);

	/* Safe to call this one even when didn't initialize (assuming kctx was sufficiently zeroed) */
	kbasep_js_kctx_term(kctx);

	kbase_jd_exit(kctx);

	kbase_pm_context_idle(kbdev);

	kbase_mmu_term(kctx);

	pages = atomic_read(&kctx->used_pages);
	if (pages != 0)
		dev_warn(kbdev->dev, "%s: %d pages in use!\n", __func__, pages);

	if (kctx->keep_gpu_powered) {
		atomic_dec(&kbdev->keep_gpu_powered_count);
		kbase_pm_context_idle(kbdev);
	}

	kbase_mem_allocator_term(&kctx->osalloc);
	WARN_ON(atomic_read(&kctx->nonmapped_pages) != 0);
	/* MALI_SEC_INTEGRATION */
	kctx->ctx_status = CTX_DESTROYED;

	if (kctx->ctx_need_qos) {
		kctx->ctx_need_qos = false;
		set_hmp_boost(0);
		set_hmp_aggressive_up_migration(false);
		set_hmp_aggressive_yield(false);
#ifdef CONFIG_MALI_DVFS
		gpu_dvfs_boost_lock(GPU_DVFS_BOOST_UNSET);
#endif /* CONFIG_MALI_DVFS */
	}

	vfree(kctx);
	/* MALI_SEC_INTEGRATION */
	kctx = NULL;
}
KBASE_EXPORT_SYMBOL(kbase_destroy_context)

/**
 * Set creation flags on a context
 */
mali_error kbase_context_set_create_flags(struct kbase_context *kctx, u32 flags)
{
	mali_error err = MALI_ERROR_NONE;
	struct kbasep_js_kctx_info *js_kctx_info;
	KBASE_DEBUG_ASSERT(NULL != kctx);

	js_kctx_info = &kctx->jctx.sched_info;

	/* Validate flags */
	if (flags != (flags & BASE_CONTEXT_CREATE_KERNEL_FLAGS)) {
		err = MALI_ERROR_FUNCTION_FAILED;
		goto out;
	}

	mutex_lock(&js_kctx_info->ctx.jsctx_mutex);

	/* Translate the flags */
	if ((flags & BASE_CONTEXT_SYSTEM_MONITOR_SUBMIT_DISABLED) == 0)
		js_kctx_info->ctx.flags &= ~((u32) KBASE_CTX_FLAG_SUBMIT_DISABLED);

	if ((flags & BASE_CONTEXT_HINT_ONLY_COMPUTE) != 0)
		js_kctx_info->ctx.flags |= (u32) KBASE_CTX_FLAG_HINT_ONLY_COMPUTE;

	/* Latch the initial attributes into the Job Scheduler */
	kbasep_js_ctx_attr_set_initial_attrs(kctx->kbdev, kctx);

	mutex_unlock(&js_kctx_info->ctx.jsctx_mutex);
 out:
	return err;
}
KBASE_EXPORT_SYMBOL(kbase_context_set_create_flags)
