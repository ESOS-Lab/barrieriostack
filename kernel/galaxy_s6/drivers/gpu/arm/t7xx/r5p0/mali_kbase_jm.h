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
 * @file mali_kbase_jm.h
 * Job Manager Low-level APIs.
 */

#ifndef _KBASE_JM_H_
#define _KBASE_JM_H_

#include <mali_kbase_hw.h>
#include <mali_kbase_debug.h>
#include <linux/atomic.h>

/**
 * @addtogroup base_api
 * @{
 */

/**
 * @addtogroup base_kbase_api
 * @{
 */

/**
 * @addtogroup kbase_jm Job Manager Low-level APIs
 * @{
 *
 */

static INLINE int kbasep_jm_is_js_free(struct kbase_device *kbdev, int js, struct kbase_context *kctx)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(0 <= js && js < kbdev->gpu_props.num_job_slots);

	return !kbase_reg_read(kbdev, JOB_SLOT_REG(js, JS_COMMAND_NEXT), kctx);
}

/**
 * This checks that:
 * - there is enough space in the GPU's buffers (JS_NEXT and JS_HEAD registers) to accomodate the job.
 * - there is enough space to track the job in a our Submit Slots. Note that we have to maintain space to
 *   requeue one job in case the next registers on the hardware need to be cleared.
 */
static INLINE mali_bool kbasep_jm_is_submit_slots_free(struct kbase_device *kbdev, int js, struct kbase_context *kctx)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(0 <= js && js < kbdev->gpu_props.num_job_slots);

	if (atomic_read(&kbdev->reset_gpu) != KBASE_RESET_GPU_NOT_PENDING) {
		/* The GPU is being reset - so prevent submission */
		return MALI_FALSE;
	}

	return (mali_bool) (kbasep_jm_is_js_free(kbdev, js, kctx)
			    && kbdev->jm_slots[js].submitted_nr < (BASE_JM_SUBMIT_SLOTS - 2));
}

/**
 * Initialize a submit slot
 */
static INLINE void kbasep_jm_init_submit_slot(struct kbase_jm_slot *slot)
{
	slot->submitted_nr = 0;
	slot->submitted_head = 0;
}

/**
 * Find the atom at the idx'th element in the queue without removing it, starting at the head with idx==0.
 */
static INLINE struct kbase_jd_atom *kbasep_jm_peek_idx_submit_slot(struct kbase_jm_slot *slot, u8 idx)
{
	u8 pos;
	struct kbase_jd_atom *katom;

	KBASE_DEBUG_ASSERT(idx < BASE_JM_SUBMIT_SLOTS);

	pos = (slot->submitted_head + idx) & BASE_JM_SUBMIT_SLOTS_MASK;
	katom = slot->submitted[pos];

	return katom;
}

/**
 * Pop front of the submitted
 */
static INLINE struct kbase_jd_atom *kbasep_jm_dequeue_submit_slot(struct kbase_jm_slot *slot)
{
	u8 pos;
	struct kbase_jd_atom *katom;

	pos = slot->submitted_head & BASE_JM_SUBMIT_SLOTS_MASK;
	katom = slot->submitted[pos];
	slot->submitted[pos] = NULL;	/* Just to catch bugs... */
	KBASE_DEBUG_ASSERT(katom);

	/* rotate the buffers */
	slot->submitted_head = (slot->submitted_head + 1) & BASE_JM_SUBMIT_SLOTS_MASK;
	slot->submitted_nr--;

	dev_dbg(katom->kctx->kbdev->dev, "katom %p new head %u", (void *)katom, (unsigned int)slot->submitted_head);

	return katom;
}

/* Pop back of the submitted queue (unsubmit a job)
 */
static INLINE struct kbase_jd_atom *kbasep_jm_dequeue_tail_submit_slot(struct kbase_jm_slot *slot)
{
	u8 pos;

	slot->submitted_nr--;

	pos = (slot->submitted_head + slot->submitted_nr) & BASE_JM_SUBMIT_SLOTS_MASK;

	return slot->submitted[pos];
}

static INLINE u8 kbasep_jm_nr_jobs_submitted(struct kbase_jm_slot *slot)
{
	return slot->submitted_nr;
}

static INLINE u8 kbasep_jm_nr_jobs_submitted_total(struct kbase_device *kbdev)
{
	u8 total = 0;
	int i;

	for (i = 0; i < kbdev->gpu_props.num_job_slots; i++) {
		total += kbdev->jm_slots[i].submitted_nr;
	}

	return total;
}

/**
 * Push back of the submitted
 */
static INLINE void kbasep_jm_enqueue_submit_slot(struct kbase_jm_slot *slot, struct kbase_jd_atom *katom)
{
	u8 nr;
	u8 pos;
	nr = slot->submitted_nr++;
	KBASE_DEBUG_ASSERT(nr < BASE_JM_SUBMIT_SLOTS);

	pos = (slot->submitted_head + nr) & BASE_JM_SUBMIT_SLOTS_MASK;
	slot->submitted[pos] = katom;
}

/**
 * @brief Query whether a job peeked/dequeued from the submit slots is a
 * 'dummy' job that is used for hardware workaround purposes.
 *
 * Any time a job is peeked/dequeued from the submit slots, this should be
 * queried on that job.
 *
 * If a \a atom is indicated as being a dummy job, then you <b>must not attempt
 * to use \a atom</b>. This is because its members will not necessarily be
 * initialized, and so could lead to a fault if they were used.
 *
 * @param[in] kbdev kbase device pointer
 * @param[in] atom The atom to query
 *
 * @return    MALI_TRUE if \a atom is for a dummy job, in which case you must not
 *            attempt to use it.
 * @return    MALI_FALSE otherwise, and \a atom is safe to use.
 */
static INLINE mali_bool kbasep_jm_is_dummy_workaround_job(struct kbase_device *kbdev, struct kbase_jd_atom *atom)
{
	/* Query the set of workaround jobs here */
	/* none exists today */
	return MALI_FALSE;
}

/**
 * @brief Submit a job to a certain job-slot
 *
 * The caller must check kbasep_jm_is_submit_slots_free() != MALI_FALSE before calling this.
 *
 * The following locking conditions are made on the caller:
 * - it must hold the kbasep_js_device_data::runpoool_irq::lock
 */
void kbase_job_submit_nolock(struct kbase_device *kbdev, struct kbase_jd_atom *katom, int js);

/**
 * @brief Complete the head job on a particular job-slot
 */
void kbase_job_done_slot(struct kbase_device *kbdev, int s, u32 completion_code, u64 job_tail, ktime_t *end_timestamp);


/**
 * Enumerate atoms that are likely to be running on the HW
 *
 * This gives a consistent snapshot of atoms that are likely to be running on
 * the HW.
 *
 * Note that no check is made to see if an atom is really running on the HW,
 * because atoms could be completing whilst this function is
 * processing. However, you can be sure that the IRQ for the enumerated atom
 * has not been processed yet. That is, every atom passed into @a callback will
 * also be processed by kbase_jd_done() at some point later.
 *
 * Whilst more atoms can be enqueued on the HW than there are available entries
 * in the HW slots, this function only enumerates the last few that might be
 * running. Hence, it might not enumerate all atoms that are currently
 * completed but waiting for an IRQ to be handled. That is, some atoms
 * processed by kbase_jd_done() might not have @a callback called on them.
 *
 * The atoms are not enumerated in any particular order. That is, the
 * implementation might be changed to enumerate atoms in a different order in
 * future.
 *
 * The caller must ensure they have locked
 * kbasep_js_device_data::runpool_irq::lock
 */
void kbase_jm_enumerate_running_atoms_locked(struct kbase_device *kbdev,
		kbase_jm_running_atoms_cb *callback, void *private);


	  /** @} *//* end group kbase_jm */
	  /** @} *//* end group base_kbase_api */
	  /** @} *//* end group base_api */

#endif				/* _KBASE_JM_H_ */
