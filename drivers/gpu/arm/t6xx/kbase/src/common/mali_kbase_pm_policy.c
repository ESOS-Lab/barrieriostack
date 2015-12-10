/*
 *
 * (C) COPYRIGHT 2010-2013 ARM Limited. All rights reserved.
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
 * @file mali_kbase_pm_policy.c
 * Power policy API implementations
 */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_midg_regmap.h>
#include <kbase/src/common/mali_kbase_gator.h>
#include <kbase/src/common/mali_kbase_pm.h>

extern const kbase_pm_policy kbase_pm_always_on_policy_ops;
extern const kbase_pm_policy kbase_pm_coarse_demand_policy_ops;
extern const kbase_pm_policy kbase_pm_demand_policy_ops;

#if MALI_CUSTOMER_RELEASE == 0 
extern const kbase_pm_policy kbase_pm_fast_start_policy_ops;
extern const kbase_pm_policy kbase_pm_demand_always_powered_policy_ops;
#endif

static const kbase_pm_policy *const policy_list[] = {
#ifdef CONFIG_MALI_NO_MALI
	&kbase_pm_always_on_policy_ops,
	&kbase_pm_demand_policy_ops,
	&kbase_pm_coarse_demand_policy_ops,
#if MALI_CUSTOMER_RELEASE == 0 
	&kbase_pm_demand_always_powered_policy_ops,
	&kbase_pm_fast_start_policy_ops,
#endif
#else				/* CONFIG_MALI_NO_MALI */
	&kbase_pm_demand_policy_ops,
	&kbase_pm_always_on_policy_ops,
	&kbase_pm_coarse_demand_policy_ops,
#if MALI_CUSTOMER_RELEASE == 0        
	&kbase_pm_demand_always_powered_policy_ops,
	&kbase_pm_fast_start_policy_ops,
#endif
#endif				/* CONFIG_MALI_NO_MALI */
};

/** The number of policies available in the system.
 * This is derived from the number of functions listed in policy_get_functions.
 */
#define POLICY_COUNT (sizeof(policy_list)/sizeof(*policy_list))


/* Function IDs for looking up Timeline Trace codes in kbase_pm_change_state_trace_code */
typedef enum
{
	KBASE_PM_FUNC_ID_REQUEST_CORES_START,
	KBASE_PM_FUNC_ID_REQUEST_CORES_END,
	KBASE_PM_FUNC_ID_RELEASE_CORES_START,
	KBASE_PM_FUNC_ID_RELEASE_CORES_END,
	/* Note: kbase_pm_unrequest_cores() is on the slow path, and we neither
	 * expect to hit it nor tend to hit it very much anyway. We can detect
	 * whether we need more instrumentation by a difference between
	 * PM_CHECKTRANS events and PM_SEND/HANDLE_EVENT. */

	/* Must be the last */
	KBASE_PM_FUNC_ID_COUNT
} kbase_pm_func_id;


/* State changes during request/unrequest/release-ing cores */
enum
{
	KBASE_PM_CHANGE_STATE_SHADER = (1u << 0),
	KBASE_PM_CHANGE_STATE_TILER  = (1u << 1),

	/* These two must be last */
	KBASE_PM_CHANGE_STATE_MASK = (KBASE_PM_CHANGE_STATE_TILER|KBASE_PM_CHANGE_STATE_SHADER),
	KBASE_PM_CHANGE_STATE_COUNT = KBASE_PM_CHANGE_STATE_MASK + 1
};
typedef u32 kbase_pm_change_state;


#ifdef CONFIG_MALI_TRACE_TIMELINE
/* Timeline Trace code lookups for each function */
static u32 kbase_pm_change_state_trace_code[KBASE_PM_FUNC_ID_COUNT][KBASE_PM_CHANGE_STATE_COUNT] =
{
	/* kbase_pm_request_cores */
	[KBASE_PM_FUNC_ID_REQUEST_CORES_START][0] = 0,
	[KBASE_PM_FUNC_ID_REQUEST_CORES_START][KBASE_PM_CHANGE_STATE_SHADER] =
		SW_FLOW_PM_CHECKTRANS_PM_REQUEST_CORES_SHADER_START,
	[KBASE_PM_FUNC_ID_REQUEST_CORES_START][KBASE_PM_CHANGE_STATE_TILER] =
		SW_FLOW_PM_CHECKTRANS_PM_REQUEST_CORES_TILER_START,
	[KBASE_PM_FUNC_ID_REQUEST_CORES_START][KBASE_PM_CHANGE_STATE_SHADER|KBASE_PM_CHANGE_STATE_TILER] =
		SW_FLOW_PM_CHECKTRANS_PM_REQUEST_CORES_SHADER_TILER_START,

	[KBASE_PM_FUNC_ID_REQUEST_CORES_END][0] = 0,
	[KBASE_PM_FUNC_ID_REQUEST_CORES_END][KBASE_PM_CHANGE_STATE_SHADER] =
		SW_FLOW_PM_CHECKTRANS_PM_REQUEST_CORES_SHADER_END,
	[KBASE_PM_FUNC_ID_REQUEST_CORES_END][KBASE_PM_CHANGE_STATE_TILER] =
		SW_FLOW_PM_CHECKTRANS_PM_REQUEST_CORES_TILER_END,
	[KBASE_PM_FUNC_ID_REQUEST_CORES_END][KBASE_PM_CHANGE_STATE_SHADER|KBASE_PM_CHANGE_STATE_TILER] =
		SW_FLOW_PM_CHECKTRANS_PM_REQUEST_CORES_SHADER_TILER_END,

	/* kbase_pm_release_cores */
	[KBASE_PM_FUNC_ID_RELEASE_CORES_START][0] = 0,
	[KBASE_PM_FUNC_ID_RELEASE_CORES_START][KBASE_PM_CHANGE_STATE_SHADER] =
		SW_FLOW_PM_CHECKTRANS_PM_RELEASE_CORES_SHADER_START,
	[KBASE_PM_FUNC_ID_RELEASE_CORES_START][KBASE_PM_CHANGE_STATE_TILER] =
		SW_FLOW_PM_CHECKTRANS_PM_RELEASE_CORES_TILER_START,
	[KBASE_PM_FUNC_ID_RELEASE_CORES_START][KBASE_PM_CHANGE_STATE_SHADER|KBASE_PM_CHANGE_STATE_TILER] =
		SW_FLOW_PM_CHECKTRANS_PM_RELEASE_CORES_SHADER_TILER_START,

	[KBASE_PM_FUNC_ID_RELEASE_CORES_END][0] = 0,
	[KBASE_PM_FUNC_ID_RELEASE_CORES_END][KBASE_PM_CHANGE_STATE_SHADER] =
		SW_FLOW_PM_CHECKTRANS_PM_RELEASE_CORES_SHADER_END,
	[KBASE_PM_FUNC_ID_RELEASE_CORES_END][KBASE_PM_CHANGE_STATE_TILER] =
		SW_FLOW_PM_CHECKTRANS_PM_RELEASE_CORES_TILER_END,
	[KBASE_PM_FUNC_ID_RELEASE_CORES_END][KBASE_PM_CHANGE_STATE_SHADER|KBASE_PM_CHANGE_STATE_TILER] =
		SW_FLOW_PM_CHECKTRANS_PM_RELEASE_CORES_SHADER_TILER_END
};

STATIC INLINE void kbase_timeline_pm_cores_func(kbase_device *kbdev,
                                                kbase_pm_func_id func_id,
                                                kbase_pm_change_state state)
{
	int trace_code;
	KBASE_DEBUG_ASSERT(func_id >= 0 && func_id < KBASE_PM_FUNC_ID_COUNT);
	KBASE_DEBUG_ASSERT(state != 0 && (state & KBASE_PM_CHANGE_STATE_MASK) == state);

	trace_code = kbase_pm_change_state_trace_code[func_id][state];
	KBASE_TIMELINE_PM_CHECKTRANS(kbdev, trace_code);
}

#else /* CONFIG_MALI_TRACE_TIMELINE */
STATIC INLINE void kbase_timeline_pm_cores_func(kbase_device *kbdev,
                                                kbase_pm_func_id func_id,
                                                kbase_pm_change_state state)
{
}

#endif /* CONFIG_MALI_TRACE_TIMELINE */


mali_error kbase_pm_policy_init(kbase_device *kbdev)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	kbdev->pm.pm_current_policy = policy_list[0];

	kbdev->pm.pm_current_policy->init(kbdev);

	return MALI_ERROR_NONE;
}

void kbase_pm_policy_term(kbase_device *kbdev)
{
	kbdev->pm.pm_current_policy->term(kbdev);
}

void kbase_pm_update_active(kbase_device *kbdev)
{
	unsigned long flags;
	mali_bool active;

	lockdep_assert_held(&kbdev->pm.lock);

	/* pm_current_policy will never be NULL while pm.lock is held */
	KBASE_DEBUG_ASSERT(kbdev->pm.pm_current_policy);

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	active = kbdev->pm.pm_current_policy->get_core_active(kbdev);

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	if (active != MALI_FALSE) {
		/* Power on the GPU and any cores requested by the policy */
		kbase_pm_do_poweron(kbdev);
	} else {
		/* It is an error for the power policy to power off the GPU
		 * when there are contexts active */
		KBASE_DEBUG_ASSERT(kbdev->pm.active_count == 0);

		/* Power off the GPU */
		kbase_pm_do_poweroff(kbdev);
	}
}

void kbase_pm_update_cores_state_nolock(kbase_device *kbdev)
{
	u64 prev_shader_state = kbdev->pm.desired_shader_state;
	u64 desired_bitmap;
	mali_bool cores_are_available;

	lockdep_assert_held(&kbdev->pm.power_change_lock);

	if (kbdev->pm.pm_current_policy == NULL)
		return;

	desired_bitmap = kbdev->pm.pm_current_policy->get_core_mask(kbdev); 
	desired_bitmap &= kbase_pm_ca_get_core_mask(kbdev);

	/* Enable core 0 if tiler required, regardless of core availability */
	if (kbdev->tiler_needed_cnt > 0 || kbdev->tiler_inuse_cnt > 0)
		desired_bitmap |= 1;

	kbdev->pm.desired_shader_state = desired_bitmap;

	if (prev_shader_state != kbdev->pm.desired_shader_state)
		KBASE_TRACE_ADD(kbdev, PM_CORES_CHANGE_DESIRED, NULL, NULL, 0u, (u32) kbdev->pm.desired_shader_state);

	if (prev_shader_state != kbdev->pm.desired_shader_state ||
	    kbdev->pm.ca_in_transition != MALI_FALSE)
		cores_are_available = kbase_pm_check_transitions_nolock(kbdev);

	/* Don't need 'cores_are_available', because we don't return anything */
	CSTD_UNUSED(cores_are_available);
}

void kbase_pm_update_cores_state(kbase_device *kbdev)
{
	unsigned long flags;

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	kbase_pm_update_cores_state_nolock(kbdev);

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);
}

int kbase_pm_list_policies(const kbase_pm_policy * const **list)
{
	if (!list)
		return POLICY_COUNT;

	*list = policy_list;

	return POLICY_COUNT;
}

KBASE_EXPORT_TEST_API(kbase_pm_list_policies)

const kbase_pm_policy *kbase_pm_get_policy(kbase_device *kbdev)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	return kbdev->pm.pm_current_policy;
}

KBASE_EXPORT_TEST_API(kbase_pm_get_policy)

void kbase_pm_set_policy(kbase_device *kbdev, const kbase_pm_policy *new_policy)
{
	const kbase_pm_policy *old_policy;
	unsigned long flags;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(new_policy != NULL);

	KBASE_TRACE_ADD(kbdev, PM_SET_POLICY, NULL, NULL, 0u, new_policy->id);

	/* During a policy change we pretend the GPU is active */
	/* A suspend won't happen here, because we're in a syscall from a userspace thread */
	kbase_pm_context_active(kbdev);

	mutex_lock(&kbdev->pm.lock);

	/* Remove the policy to prevent IRQ handlers from working on it */
	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);
	old_policy = kbdev->pm.pm_current_policy;
	kbdev->pm.pm_current_policy = NULL;
	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	KBASE_TRACE_ADD(kbdev, PM_CURRENT_POLICY_TERM, NULL, NULL, 0u, old_policy->id);
	if (old_policy->term)
		old_policy->term(kbdev);

	KBASE_TRACE_ADD(kbdev, PM_CURRENT_POLICY_INIT, NULL, NULL, 0u, new_policy->id);
	if (new_policy->init)
		new_policy->init(kbdev);

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);
	kbdev->pm.pm_current_policy = new_policy;
	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	/* If any core power state changes were previously attempted, but couldn't
	 * be made because the policy was changing (current_policy was NULL), then
	 * re-try them here. */
	kbase_pm_update_active(kbdev);
	kbase_pm_update_cores_state(kbdev);

	mutex_unlock(&kbdev->pm.lock);

	/* Now the policy change is finished, we release our fake context active reference */
	kbase_pm_context_idle(kbdev);
}

KBASE_EXPORT_TEST_API(kbase_pm_set_policy)

/** Check whether a state change has finished, and trace it as completed */
STATIC void kbase_pm_trace_check_and_finish_state_change(kbase_device *kbdev)
{
	if ((kbdev->shader_available_bitmap & kbdev->pm.desired_shader_state) == kbdev->pm.desired_shader_state
		&& (kbdev->tiler_available_bitmap & kbdev->pm.desired_tiler_state) == kbdev->pm.desired_tiler_state)
		kbase_timeline_pm_check_handle_event(kbdev, KBASE_TIMELINE_PM_EVENT_GPU_STATE_CHANGED);
}

void kbase_pm_request_cores(kbase_device *kbdev, mali_bool tiler_required, u64 shader_cores)
{
	unsigned long flags;
	u64 cores;

	kbase_pm_change_state change_gpu_state = 0u;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	cores = shader_cores;
	while (cores) {
		int bitnum = fls64(cores) - 1;
		u64 bit = 1ULL << bitnum;

		/* It should be almost impossible for this to overflow. It would require 2^32 atoms
		 * to request a particular core, which would require 2^24 contexts to submit. This 
		 * would require an amount of memory that is impossible on a 32-bit system and 
		 * extremely unlikely on a 64-bit system. */
		int cnt = ++kbdev->shader_needed_cnt[bitnum];

		if (1 == cnt) {
			kbdev->shader_needed_bitmap |= bit;
			change_gpu_state |= KBASE_PM_CHANGE_STATE_SHADER;
		}

		cores &= ~bit;
	}

	if (tiler_required != MALI_FALSE) {
		++kbdev->tiler_needed_cnt;

		KBASE_DEBUG_ASSERT(kbdev->tiler_needed_cnt != 0);

		/* For tiler jobs, we must make sure that core 0 is not turned off if it's already on.
	         * However, it's safe for core 0 to be left off and turned on later whilst a tiler job
		 * is running. Hence, we don't need to update the cores state immediately. Also,
		 * attempts to turn off cores will always check the tiler_needed/inuse state first anyway.
		 *
		 * Finally, kbase_js_choose_affinity() ensures core 0 is always requested for tiler jobs
		 * anyway. Hence when there's only a tiler job in the system, this will still cause
		 * kbase_pm_update_cores_state_nolock() to be called.
		 *
		 * Note that we still need to keep track of tiler_needed/inuse_cnt, to ensure that
		 * kbase_pm_update_cores_state_nolock() can override the core availability policy and
		 * force core 0 to be powered when a tiler job is in the system. */
	}

	if (change_gpu_state) {
		KBASE_TRACE_ADD(kbdev, PM_REQUEST_CHANGE_SHADER_NEEDED, NULL, NULL, 0u, (u32) kbdev->shader_needed_bitmap);

		kbase_timeline_pm_cores_func(kbdev, KBASE_PM_FUNC_ID_REQUEST_CORES_START, change_gpu_state);
		kbase_pm_update_cores_state_nolock(kbdev);
		kbase_timeline_pm_cores_func(kbdev, KBASE_PM_FUNC_ID_REQUEST_CORES_END, change_gpu_state);
	}

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);
}

KBASE_EXPORT_TEST_API(kbase_pm_request_cores)

void kbase_pm_unrequest_cores(kbase_device *kbdev, mali_bool tiler_required, u64 shader_cores)
{
	unsigned long flags;

	kbase_pm_change_state change_gpu_state = 0u;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	while (shader_cores) {
		int bitnum = fls64(shader_cores) - 1;
		u64 bit = 1ULL << bitnum;
		int cnt;

		KBASE_DEBUG_ASSERT(kbdev->shader_needed_cnt[bitnum] > 0);

		cnt = --kbdev->shader_needed_cnt[bitnum];

		if (0 == cnt) {
			kbdev->shader_needed_bitmap &= ~bit;
			change_gpu_state |= KBASE_PM_CHANGE_STATE_SHADER;
		}

		shader_cores &= ~bit;
	}

	if (tiler_required != MALI_FALSE) {
		KBASE_DEBUG_ASSERT(kbdev->tiler_needed_cnt > 0);

		--kbdev->tiler_needed_cnt;

		/* Whilst tiler jobs must not allow core 0 to be turned off, we don't need to make an
		 * extra call to kbase_pm_update_cores_state_nolock() to ensure core 0 is turned off
		 * when the last tiler job unrequests cores: kbase_js_choose_affinity() ensures core 0
		 * was originally requested for tiler jobs. Hence when there's only a tiler job in the
		 * system, this will still cause kbase_pm_update_cores_state_nolock() to be called. */
	}

	if (change_gpu_state) {
		KBASE_TRACE_ADD(kbdev, PM_UNREQUEST_CHANGE_SHADER_NEEDED, NULL, NULL, 0u, (u32) kbdev->shader_needed_bitmap);

		kbase_pm_update_cores_state_nolock(kbdev);

		/* Trace that any state change effectively completes immediately -
		 * no-one will wait on the state change */
		kbase_pm_trace_check_and_finish_state_change(kbdev);
	}

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);
}

KBASE_EXPORT_TEST_API(kbase_pm_unrequest_cores)

kbase_pm_cores_ready kbase_pm_register_inuse_cores(kbase_device *kbdev, mali_bool tiler_required, u64 shader_cores)
{
	unsigned long flags;
	u64 prev_shader_needed;	/* Just for tracing */
	u64 prev_shader_inuse;	/* Just for tracing */

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	prev_shader_needed = kbdev->shader_needed_bitmap;
	prev_shader_inuse = kbdev->shader_inuse_bitmap;

	/* If desired_shader_state does not contain the requested cores, then power
	 * management is not attempting to powering those cores (most likely
	 * due to core availability policy) and a new job affinity must be
	 * chosen */
	if ((kbdev->pm.desired_shader_state & shader_cores) != shader_cores) {
		spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

		return KBASE_NEW_AFFINITY;
	}

	if ((kbdev->shader_available_bitmap & shader_cores) != shader_cores ||
	    (tiler_required != MALI_FALSE && !kbdev->tiler_available_bitmap)) {
		/* Trace ongoing core transition */
		kbase_timeline_pm_l2_transition_start(kbdev);
		spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);
		return KBASE_CORES_NOT_READY;
	}

	/* If we started to trace a state change, then trace it has being finished
	 * by now, at the very latest */
	kbase_pm_trace_check_and_finish_state_change(kbdev);
	/* Trace core transition done */
	kbase_timeline_pm_l2_transition_done(kbdev);

	while (shader_cores) {
		int bitnum = fls64(shader_cores) - 1;
		u64 bit = 1ULL << bitnum;
		int cnt;

		KBASE_DEBUG_ASSERT(kbdev->shader_needed_cnt[bitnum] > 0);

		cnt = --kbdev->shader_needed_cnt[bitnum];

		if (0 == cnt)
			kbdev->shader_needed_bitmap &= ~bit;

		/* shader_inuse_cnt should not overflow because there can only be a
		 * very limited number of jobs on the h/w at one time */

		kbdev->shader_inuse_cnt[bitnum]++;
		kbdev->shader_inuse_bitmap |= bit;

		shader_cores &= ~bit;
	}

	if (tiler_required != MALI_FALSE) {
		KBASE_DEBUG_ASSERT(kbdev->tiler_needed_cnt > 0);

		--kbdev->tiler_needed_cnt;

		kbdev->tiler_inuse_cnt++;

		KBASE_DEBUG_ASSERT(kbdev->tiler_inuse_cnt != 0);
	}

	if (prev_shader_needed != kbdev->shader_needed_bitmap)
		KBASE_TRACE_ADD(kbdev, PM_REGISTER_CHANGE_SHADER_NEEDED, NULL, NULL, 0u, (u32) kbdev->shader_needed_bitmap);

	if (prev_shader_inuse != kbdev->shader_inuse_bitmap)
		KBASE_TRACE_ADD(kbdev, PM_REGISTER_CHANGE_SHADER_INUSE, NULL, NULL, 0u, (u32) kbdev->shader_inuse_bitmap);

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	return KBASE_CORES_READY;
}

KBASE_EXPORT_TEST_API(kbase_pm_register_inuse_cores)

void kbase_pm_release_cores(kbase_device *kbdev, mali_bool tiler_required, u64 shader_cores)
{
	unsigned long flags;
	kbase_pm_change_state change_gpu_state = 0u;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	while (shader_cores) {
		int bitnum = fls64(shader_cores) - 1;
		u64 bit = 1ULL << bitnum;
		int cnt;

		KBASE_DEBUG_ASSERT(kbdev->shader_inuse_cnt[bitnum] > 0);

		cnt = --kbdev->shader_inuse_cnt[bitnum];

		if (0 == cnt) {
			kbdev->shader_inuse_bitmap &= ~bit;
			change_gpu_state |= KBASE_PM_CHANGE_STATE_SHADER;
		}

		shader_cores &= ~bit;
	}

	if (tiler_required != MALI_FALSE) {
		KBASE_DEBUG_ASSERT(kbdev->tiler_inuse_cnt > 0);

		--kbdev->tiler_inuse_cnt;

		/* Whilst tiler jobs must not allow core 0 to be turned off, we don't need to make an
		 * extra call to kbase_pm_update_cores_state_nolock() to ensure core 0 is turned off
		 * when the last tiler job finishes: kbase_js_choose_affinity() ensures core 0 was
		 * originally requested for tiler jobs. Hence when there's only a tiler job in the
		 * system, this will still cause kbase_pm_update_cores_state_nolock() to be called */
	}

	if (change_gpu_state) {
		KBASE_TRACE_ADD(kbdev, PM_RELEASE_CHANGE_SHADER_INUSE, NULL, NULL, 0u, (u32) kbdev->shader_inuse_bitmap);

		kbase_timeline_pm_cores_func(kbdev, KBASE_PM_FUNC_ID_RELEASE_CORES_START, change_gpu_state);
		kbase_pm_update_cores_state_nolock(kbdev);
		kbase_timeline_pm_cores_func(kbdev, KBASE_PM_FUNC_ID_RELEASE_CORES_END, change_gpu_state);

		/* Trace that any state change completed immediately */
		kbase_pm_trace_check_and_finish_state_change(kbdev);
	}

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);
}

KBASE_EXPORT_TEST_API(kbase_pm_release_cores)

void kbase_pm_request_cores_sync(struct kbase_device *kbdev, mali_bool tiler_required, u64 shader_cores)
{
	kbase_pm_request_cores(kbdev, tiler_required, shader_cores);

	kbase_pm_check_transitions_sync(kbdev);
}

KBASE_EXPORT_TEST_API(kbase_pm_request_cores_sync)

void kbase_pm_request_l2_caches(kbase_device *kbdev)
{
	unsigned long flags;
	u32 prior_l2_users_count;
	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	prior_l2_users_count = kbdev->l2_users_count++;

	KBASE_DEBUG_ASSERT(kbdev->l2_users_count != 0);

	if (!prior_l2_users_count)
		kbase_pm_update_cores_state_nolock(kbdev);

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);
	wait_event(kbdev->pm.l2_powered_wait, kbdev->pm.l2_powered == 1);

	/* Trace that any state change completed immediately */
	kbase_pm_trace_check_and_finish_state_change(kbdev);
}

KBASE_EXPORT_TEST_API(kbase_pm_request_l2_caches)

void kbase_pm_release_l2_caches(kbase_device *kbdev)
{
	unsigned long flags;
	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	KBASE_DEBUG_ASSERT(kbdev->l2_users_count > 0);

	--kbdev->l2_users_count;

	if (!kbdev->l2_users_count) {
		kbase_pm_update_cores_state_nolock(kbdev);
		/* Trace that any state change completed immediately */
		kbase_pm_trace_check_and_finish_state_change(kbdev);
	}

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);
}

KBASE_EXPORT_TEST_API(kbase_pm_release_l2_caches)

