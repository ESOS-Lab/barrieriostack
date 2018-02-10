/*
 *
 * (C) COPYRIGHT 2010-2012 ARM Limited. All rights reserved.
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



#include <kbase/src/common/mali_kbase.h>

#define beenthere(f, a...)	pr_debug("%s:" f, __func__, ##a)

STATIC base_jd_udata kbase_event_process(kbase_context *kctx, kbase_jd_atom *katom)
{
	base_jd_udata data;

#ifdef SLSI_INTEGRATION
	pgd_t *pgd;
	struct mm_struct *mm;

	if (!kctx) {
		memset(data.blob, 0, sizeof(data.blob));
		printk("kctx is NULL! \n");
		return data;
	}
	if (!katom) {
		memset(data.blob, 0, sizeof(data.blob));
		printk("katom is NULL! \n");
		return data;
	}
	if (katom->status != KBASE_JD_ATOM_STATE_COMPLETED) {
		memset(data.blob, 0, sizeof(data.blob));
		printk("katom->status (0x%x) isn't completed! \n", katom->status);
		return data;
	}

	mm = katom->kctx->process_mm;
	pgd = pgd_offset(mm, (unsigned long)&katom->completed);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		printk("Abnormal katom\n");
		printk("katom->kctx: 0x%p, katom->kctx->osctx.tgid: %d, katom->kctx->process_mm: 0x%p, pgd: 0x%px\n", katom->kctx, katom->kctx->osctx.tgid, katom->kctx->process_mm, pgd);
		return data;
	}
#endif
	data = katom->udata;

	KBASE_TIMELINE_ATOMS_IN_FLIGHT(kctx, atomic_sub_return(1, &kctx->timeline.jd_atoms_in_flight));

	katom->status = KBASE_JD_ATOM_STATE_UNUSED;

	wake_up(&katom->completed);

	return data;
}

int kbase_event_pending(kbase_context *ctx)
{
	int ret;

	KBASE_DEBUG_ASSERT(ctx);

	mutex_lock(&ctx->event_mutex);
	ret = (!list_empty(&ctx->event_list)) || (MALI_TRUE == ctx->event_closed);
	mutex_unlock(&ctx->event_mutex);

	return ret;
}

KBASE_EXPORT_TEST_API(kbase_event_pending)

int kbase_event_dequeue(kbase_context *ctx, base_jd_event_v2 *uevent)
{
	kbase_jd_atom *atom;

	KBASE_DEBUG_ASSERT(ctx);

	mutex_lock(&ctx->event_mutex);

	if (list_empty(&ctx->event_list)) {
		if (ctx->event_closed) {
			/* generate the BASE_JD_EVENT_DRV_TERMINATED message on the fly */
			mutex_unlock(&ctx->event_mutex);
			uevent->event_code = BASE_JD_EVENT_DRV_TERMINATED;
			memset(&uevent->udata, 0, sizeof(uevent->udata));
			beenthere("event system closed, returning BASE_JD_EVENT_DRV_TERMINATED(0x%X)\n", BASE_JD_EVENT_DRV_TERMINATED);
			return 0;
		} else {
			mutex_unlock(&ctx->event_mutex);
			return -1;
		}
	}

	/* normal event processing */
	atom = list_entry(ctx->event_list.next, kbase_jd_atom, dep_item[0]);
	list_del(ctx->event_list.next);

	mutex_unlock(&ctx->event_mutex);

	beenthere("event dequeuing %p\n", (void *)atom);
	uevent->event_code = atom->event_code;
	uevent->atom_number = (atom - ctx->jctx.atoms);
	uevent->udata = kbase_event_process(ctx, atom);

	return 0;
}

KBASE_EXPORT_TEST_API(kbase_event_dequeue)

static void kbase_event_post_worker(struct work_struct *data)
{
	kbase_jd_atom *atom = CONTAINER_OF(data, kbase_jd_atom, work);
	kbase_context *ctx = atom->kctx;

	if (atom->core_req & BASE_JD_REQ_EXTERNAL_RESOURCES)
		kbase_jd_free_external_resources(atom);

	if (atom->core_req & BASE_JD_REQ_EVENT_ONLY_ON_FAILURE) {
		if (atom->event_code == BASE_JD_EVENT_DONE) {
			/* Don't report the event */
			kbase_event_process(ctx, atom);
			return;
		}
	}

	mutex_lock(&ctx->event_mutex);
	list_add_tail(&atom->dep_item[0], &ctx->event_list);
	mutex_unlock(&ctx->event_mutex);

	kbase_event_wakeup(ctx);
}

void kbase_event_post(kbase_context *ctx, kbase_jd_atom *atom)
{
	KBASE_DEBUG_ASSERT(ctx);
	KBASE_DEBUG_ASSERT(ctx->event_workq);
	KBASE_DEBUG_ASSERT(atom);

	INIT_WORK(&atom->work, kbase_event_post_worker);
	queue_work(ctx->event_workq, &atom->work);
}

KBASE_EXPORT_TEST_API(kbase_event_post)

void kbase_event_close(kbase_context *kctx)
{
	mutex_lock(&kctx->event_mutex);
	kctx->event_closed = MALI_TRUE;
	mutex_unlock(&kctx->event_mutex);
	kbase_event_wakeup(kctx);
}

mali_error kbase_event_init(kbase_context *kctx)
{
	KBASE_DEBUG_ASSERT(kctx);

	INIT_LIST_HEAD(&kctx->event_list);
	mutex_init(&kctx->event_mutex);
	kctx->event_closed = MALI_FALSE;
	kctx->event_workq = alloc_workqueue("kbase_event", WQ_MEM_RECLAIM, 1);

	if (NULL == kctx->event_workq)
		return MALI_ERROR_FUNCTION_FAILED;

	return MALI_ERROR_NONE;
}

KBASE_EXPORT_TEST_API(kbase_event_init)

void kbase_event_cleanup(kbase_context *kctx)
{
	KBASE_DEBUG_ASSERT(kctx);
	KBASE_DEBUG_ASSERT(kctx->event_workq);

	flush_workqueue(kctx->event_workq);
	destroy_workqueue(kctx->event_workq);

	/* We use kbase_event_dequeue to remove the remaining events as that
	 * deals with all the cleanup needed for the atoms.
	 *
	 * Note: use of kctx->event_list without a lock is safe because this must be the last
	 * thread using it (because we're about to terminate the lock)
	 */
	while (!list_empty(&kctx->event_list)) {
		base_jd_event_v2 event;
		kbase_event_dequeue(kctx, &event);
	}
}

KBASE_EXPORT_TEST_API(kbase_event_cleanup)
