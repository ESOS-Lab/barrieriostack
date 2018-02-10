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
 * @file mali_kbase_pm_driver.c
 * Base kernel Power Management hardware control
 */

#include <mali_kbase.h>
#include <mali_midg_regmap.h>
#include <mali_kbase_gator.h>
#include <mali_kbase_pm.h>

#if MALI_MOCK_TEST
#define MOCKABLE(function) function##_original
#else
#define MOCKABLE(function) function
#endif				/* MALI_MOCK_TEST */

/** Actions that can be performed on a core.
 *
 * This enumeration is private to the file. Its values are set to allow @ref core_type_to_reg function,
 * which decodes this enumeration, to be simpler and more efficient.
 */
typedef enum kbasep_pm_action {
	ACTION_PRESENT = 0,
	ACTION_READY = (SHADER_READY_LO - SHADER_PRESENT_LO),
	ACTION_PWRON = (SHADER_PWRON_LO - SHADER_PRESENT_LO),
	ACTION_PWROFF = (SHADER_PWROFF_LO - SHADER_PRESENT_LO),
	ACTION_PWRTRANS = (SHADER_PWRTRANS_LO - SHADER_PRESENT_LO),
	ACTION_PWRACTIVE = (SHADER_PWRACTIVE_LO - SHADER_PRESENT_LO)
} kbasep_pm_action;

/** Decode a core type and action to a register.
 *
 * Given a core type (defined by @ref kbase_pm_core_type) and an action (defined by @ref kbasep_pm_action) this
 * function will return the register offset that will perform the action on the core type. The register returned is
 * the \c _LO register and an offset must be applied to use the \c _HI register.
 *
 * @param core_type The type of core
 * @param action    The type of action
 *
 * @return The register offset of the \c _LO register that performs an action of type \c action on a core of type \c
 * core_type.
 */
static u32 core_type_to_reg(kbase_pm_core_type core_type, kbasep_pm_action action)
{
	return core_type + action;
}

/** Invokes an action on a core set
 *
 * This function performs the action given by \c action on a set of cores of a type given by \c core_type. It is a
 * static function used by @ref kbase_pm_transition_core_type
 *
 * @param kbdev     The kbase device structure of the device
 * @param core_type The type of core that the action should be performed on
 * @param cores     A bit mask of cores to perform the action on (low 32 bits)
 * @param action    The action to perform on the cores
 */
STATIC void kbase_pm_invoke(kbase_device *kbdev, kbase_pm_core_type core_type, u64 cores, kbasep_pm_action action)
{
	u32 reg;
	u32 lo = cores & 0xFFFFFFFF;
	u32 hi = (cores >> 32) & 0xFFFFFFFF;

	lockdep_assert_held(&kbdev->pm.power_change_lock);

	reg = core_type_to_reg(core_type, action);

	KBASE_DEBUG_ASSERT(reg);
#ifdef CONFIG_MALI_GATOR_SUPPORT
	if (cores) {
		if (action == ACTION_PWRON)
			kbase_trace_mali_pm_power_on(core_type, cores);
		else if (action == ACTION_PWROFF)
			kbase_trace_mali_pm_power_off(core_type, cores);
	}
#endif				/* CONFIG_MALI_GATOR_SUPPORT */
	/* Tracing */
	if (cores) {
		if (action == ACTION_PWRON)
			switch (core_type) {
				case KBASE_PM_CORE_SHADER:
					KBASE_TRACE_ADD(kbdev, PM_PWRON, NULL, NULL, 0u, lo);
					break;
				case KBASE_PM_CORE_TILER:
					KBASE_TRACE_ADD(kbdev, PM_PWRON_TILER, NULL, NULL, 0u, lo);
					break;
				case KBASE_PM_CORE_L2:
					KBASE_TRACE_ADD(kbdev, PM_PWRON_L2, NULL, NULL, 0u, lo);
					break;
				default:
					/* L3 not handled */
					break;
			}
		else if (action == ACTION_PWROFF)
			switch (core_type) {
				case KBASE_PM_CORE_SHADER:
					KBASE_TRACE_ADD(kbdev, PM_PWROFF, NULL, NULL, 0u, lo);
					break;
				case KBASE_PM_CORE_TILER:
					KBASE_TRACE_ADD(kbdev, PM_PWROFF_TILER, NULL, NULL, 0u, lo);
					break;
				case KBASE_PM_CORE_L2:
					KBASE_TRACE_ADD(kbdev, PM_PWROFF_L2, NULL, NULL, 0u, lo);
					break;
				default:
					/* L3 not handled */
					break;
			}
	}

	if (action == ACTION_PWROFF)
		return;

	if (lo != 0)
		kbase_reg_write(kbdev, GPU_CONTROL_REG(reg), lo, NULL);

	if (hi != 0)
		kbase_reg_write(kbdev, GPU_CONTROL_REG(reg + 4), hi, NULL);
}

/** Get information about a core set
 *
 * This function gets information (chosen by \c action) about a set of cores of a type given by \c core_type. It is a
 * static function used by @ref kbase_pm_get_present_cores, @ref kbase_pm_get_active_cores, @ref
 * kbase_pm_get_trans_cores and @ref kbase_pm_get_ready_cores.
 *
 * @param kbdev     The kbase device structure of the device
 * @param core_type The type of core that the should be queried
 * @param action    The property of the cores to query
 *
 * @return A bit mask specifying the state of the cores
 */
static u64 kbase_pm_get_state(kbase_device *kbdev, kbase_pm_core_type core_type, kbasep_pm_action action)
{
	u32 reg;
	u32 lo, hi;

	reg = core_type_to_reg(core_type, action);

	KBASE_DEBUG_ASSERT(reg);

	lo = kbase_reg_read(kbdev, GPU_CONTROL_REG(reg), NULL);
	hi = kbase_reg_read(kbdev, GPU_CONTROL_REG(reg + 4), NULL);

	return (((u64) hi) << 32) | ((u64) lo);
}

void kbasep_pm_read_present_cores(kbase_device *kbdev)
{
	kbdev->shader_present_bitmap = kbase_pm_get_state(kbdev, KBASE_PM_CORE_SHADER, ACTION_PRESENT);
	kbdev->tiler_present_bitmap = kbase_pm_get_state(kbdev, KBASE_PM_CORE_TILER, ACTION_PRESENT);
	kbdev->l2_present_bitmap = kbase_pm_get_state(kbdev, KBASE_PM_CORE_L2, ACTION_PRESENT);
	kbdev->l3_present_bitmap = kbase_pm_get_state(kbdev, KBASE_PM_CORE_L3, ACTION_PRESENT);

	kbdev->shader_inuse_bitmap = 0;
	kbdev->shader_needed_bitmap = 0;
	kbdev->shader_available_bitmap = 0;
	kbdev->tiler_available_bitmap = 0;
	kbdev->l2_users_count = 0;
	kbdev->l2_available_bitmap = 0;
	kbdev->tiler_needed_cnt = 0;
	kbdev->tiler_inuse_cnt = 0;

	memset(kbdev->shader_needed_cnt, 0, sizeof(kbdev->shader_needed_cnt));
}

KBASE_EXPORT_TEST_API(kbasep_pm_read_present_cores)

/** Get the cores that are present
 */
u64 kbase_pm_get_present_cores(kbase_device *kbdev, kbase_pm_core_type type)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	switch (type) {
	case KBASE_PM_CORE_L3:
		return kbdev->l3_present_bitmap;
		break;
	case KBASE_PM_CORE_L2:
		return kbdev->l2_present_bitmap;
		break;
	case KBASE_PM_CORE_SHADER:
		return kbdev->shader_present_bitmap;
		break;
	case KBASE_PM_CORE_TILER:
		return kbdev->tiler_present_bitmap;
		break;
	}
	KBASE_DEBUG_ASSERT(0);
	return 0;
}

KBASE_EXPORT_TEST_API(kbase_pm_get_present_cores)

/** Get the cores that are "active" (busy processing work)
 */
u64 kbase_pm_get_active_cores(kbase_device *kbdev, kbase_pm_core_type type)
{
	return kbase_pm_get_state(kbdev, type, ACTION_PWRACTIVE);
}

KBASE_EXPORT_TEST_API(kbase_pm_get_active_cores)

/** Get the cores that are transitioning between power states
 */
u64 kbase_pm_get_trans_cores(kbase_device *kbdev, kbase_pm_core_type type)
{
	return kbase_pm_get_state(kbdev, type, ACTION_PWRTRANS);
}

KBASE_EXPORT_TEST_API(kbase_pm_get_trans_cores)
/** Get the cores that are powered on
 */
u64 kbase_pm_get_ready_cores(kbase_device *kbdev, kbase_pm_core_type type)
{
	u64 result;
	result = kbase_pm_get_state(kbdev, type, ACTION_READY);

	switch (type) {
		case KBASE_PM_CORE_SHADER:
			KBASE_TRACE_ADD(kbdev, PM_CORES_POWERED, NULL, NULL, 0u, (u32) result);
			break;
		case KBASE_PM_CORE_TILER:
			KBASE_TRACE_ADD(kbdev, PM_CORES_POWERED_TILER, NULL, NULL, 0u, (u32) result);
			break;
		case KBASE_PM_CORE_L2:
			KBASE_TRACE_ADD(kbdev, PM_CORES_POWERED_L2, NULL, NULL, 0u, (u32) result);
			break;
		default:
			/* NB: L3 not currently traced */
			break;
	}

	return result;
}

KBASE_EXPORT_TEST_API(kbase_pm_get_ready_cores)

/** Perform power transitions for a particular core type.
 *
 * This function will perform any available power transitions to make the actual hardware state closer to the desired
 * state. If a core is currently transitioning then changes to the power state of that call cannot be made until the
 * transition has finished. Cores which are not present in the hardware are ignored if they are specified in the
 * desired_state bitmask, however the return value will always be 0 in this case.
 *
 * @param kbdev             The kbase device
 * @param type              The core type to perform transitions for
 * @param desired_state     A bit mask of the desired state of the cores
 * @param in_use            A bit mask of the cores that are currently running jobs.
 *                          These cores have to be kept powered up because there are jobs
 *                          running (or about to run) on them.
 * @param[out] available    Receives a bit mask of the cores that the job scheduler can use to submit jobs to.
 *                          May be NULL if this is not needed.
 * @param[in,out] powering_on Bit mask to update with cores that are transitioning to a power-on state.
 *
 * @return MALI_TRUE if the desired state has been reached, MALI_FALSE otherwise
 */
STATIC mali_bool kbase_pm_transition_core_type(kbase_device *kbdev, kbase_pm_core_type type, u64 desired_state,
					       u64 in_use, u64 * const available, u64 *powering_on)
{
	u64 present;
	u64 ready;
	u64 trans;
	u64 powerup;
	u64 powerdown;
	u64 powering_on_trans;
	u64 desired_state_in_use;

	lockdep_assert_held(&kbdev->pm.power_change_lock);

	/* Get current state */
	present = kbase_pm_get_present_cores(kbdev, type);
	trans = kbase_pm_get_trans_cores(kbdev, type);
	ready = kbase_pm_get_ready_cores(kbdev, type);

	/* mask off ready from trans in case transitions finished between the register reads */
	trans &= ~ready;

	powering_on_trans = trans & *powering_on;
	*powering_on = powering_on_trans;

	if (available != NULL)
		*available = (ready | powering_on_trans) & desired_state;

	/* Update desired state to include the in-use cores. These have to be kept powered up because there are jobs
	 * running or about to run on these cores
	 */
	desired_state_in_use = desired_state | in_use;

	/* Update state of whether l2 caches are powered */
	if (type == KBASE_PM_CORE_L2) {
		if ((ready == present) && (desired_state_in_use == ready) && (trans == 0)) {
			/* All are ready, none will be turned off, and none are transitioning */
			kbdev->pm.l2_powered = 1;
			if (kbdev->l2_users_count > 0) {
				/* Notify any registered l2 cache users (optimized out when no users waiting) */
				wake_up(&kbdev->pm.l2_powered_wait);
			}
		} else {
			kbdev->pm.l2_powered = 0;
		}
	}

	if (desired_state_in_use == ready && (trans == 0))
		return MALI_TRUE;

	/* Restrict the cores to those that are actually present */
	powerup = desired_state_in_use & present;
	powerdown = (~desired_state_in_use) & present;

	/* Restrict to cores that are not already in the desired state */
	powerup &= ~ready;
	powerdown &= ready;

	/* Don't transition any cores that are already transitioning, except for
	 * Mali cores that support the following case:
	 *
	 * If the SHADER_PWRON or TILER_PWRON registers are written to turn on
	 * a core that is currently transitioning to power off, then this is 
	 * remembered and the shader core is automatically powered up again once
	 * the original transition completes. Once the automatic power on is
	 * complete any job scheduled on the shader core should start.
	 */
	powerdown &= ~trans;

	if (kbase_hw_has_feature(kbdev, BASE_HW_FEATURE_PWRON_DURING_PWROFF_TRANS))
		if (KBASE_PM_CORE_SHADER == type || KBASE_PM_CORE_TILER == type)
			trans = powering_on_trans; /* for exception cases, only mask off cores in power on transitions */

	powerup &= ~trans;

	/* Perform transitions if any */
	kbase_pm_invoke(kbdev, type, powerup, ACTION_PWRON);
	kbase_pm_invoke(kbdev, type, powerdown, ACTION_PWROFF);

	/* Recalculate cores transitioning on, and re-evaluate our state */
	powering_on_trans |= powerup;
	*powering_on = powering_on_trans;
	if (available != NULL)
		*available = (ready | powering_on_trans) & desired_state;

	return MALI_FALSE;
}

KBASE_EXPORT_TEST_API(kbase_pm_transition_core_type)

/** Determine which caches should be on for a particular core state.
 *
 * This function takes a bit mask of the present caches and the cores (or caches) that are attached to the caches that
 * will be powered. It then computes which caches should be turned on to allow the cores requested to be powered up.
 *
 * @param present       The bit mask of present caches
 * @param cores_powered A bit mask of cores (or L2 caches) that are desired to be powered
 *
 * @return A bit mask of the caches that should be turned on
 */
STATIC u64 get_desired_cache_status(u64 present, u64 cores_powered)
{
	u64 desired = 0;

	while (present) {
		/* Find out which is the highest set bit */
		u64 bit = fls64(present) - 1;
		u64 bit_mask = 1ull << bit;
		/* Create a mask which has all bits from 'bit' upwards set */

		u64 mask = ~(bit_mask - 1);

		/* If there are any cores powered at this bit or above (that haven't previously been processed) then we need
		 * this core on */
		if (cores_powered & mask)
			desired |= bit_mask;

		/* Remove bits from cores_powered and present */
		cores_powered &= ~mask;
		present &= ~bit_mask;
	}

	return desired;
}

KBASE_EXPORT_TEST_API(get_desired_cache_status)

mali_bool MOCKABLE(kbase_pm_check_transitions_nolock) (struct kbase_device *kbdev)
{
	mali_bool cores_are_available = MALI_FALSE;
	mali_bool in_desired_state = MALI_TRUE;
	u64 desired_l2_state;
	u64 desired_l3_state;
	u64 cores_powered;
	u64 tiler_available_bitmap;
	u64 shader_available_bitmap;
	u64 shader_ready_bitmap;
	u64 shader_transitioning_bitmap;
	u64 l2_available_bitmap;

	KBASE_DEBUG_ASSERT(NULL != kbdev);
	lockdep_assert_held(&kbdev->pm.power_change_lock);

	spin_lock(&kbdev->pm.gpu_powered_lock);
	if (kbdev->pm.gpu_powered == MALI_FALSE) {
		spin_unlock(&kbdev->pm.gpu_powered_lock);
		if (kbdev->pm.desired_shader_state == 0 && kbdev->pm.desired_tiler_state == 0)
			return MALI_TRUE;
		return MALI_FALSE;
	}

	/* Trace that a change-state is being requested, and that it took
	 * (effectively) no time to start it. This is useful for counting how many
	 * state changes occurred, in a way that's backwards-compatible with
	 * processing the trace data */
	kbase_timeline_pm_send_event(kbdev, KBASE_TIMELINE_PM_EVENT_CHANGE_GPU_STATE);
	kbase_timeline_pm_handle_event(kbdev, KBASE_TIMELINE_PM_EVENT_CHANGE_GPU_STATE);

	/* If any cores are already powered then, we must keep the caches on */
	cores_powered = kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_SHADER);

	cores_powered |= kbdev->pm.desired_shader_state;

	/* If there are l2 cache users registered, keep all l2s powered even if all other cores are off. */
	if (kbdev->l2_users_count > 0)
		cores_powered |= kbdev->l2_present_bitmap;

	desired_l2_state = get_desired_cache_status(kbdev->l2_present_bitmap, cores_powered);

	/* If any l2 cache is on, then enable l2 #0, for use by job manager */
	if (0 != desired_l2_state) {
		desired_l2_state |= 1;
		/* Also enable tiler if l2 cache is powered */
		kbdev->pm.desired_tiler_state = kbdev->tiler_present_bitmap;
	} else {
		kbdev->pm.desired_tiler_state = 0;
	}

	desired_l3_state = get_desired_cache_status(kbdev->l3_present_bitmap, desired_l2_state);

	in_desired_state &= kbase_pm_transition_core_type(kbdev, KBASE_PM_CORE_L3, desired_l3_state, 0, NULL, &kbdev->pm.powering_on_l3_state);
	in_desired_state &= kbase_pm_transition_core_type(kbdev, KBASE_PM_CORE_L2, desired_l2_state, 0, &l2_available_bitmap, &kbdev->pm.powering_on_l2_state);

	if( kbdev->l2_available_bitmap != l2_available_bitmap)
	{
		KBASE_TIMELINE_POWER_L2(kbdev,l2_available_bitmap);
	}

	kbdev->l2_available_bitmap = l2_available_bitmap;

	if (in_desired_state) {

		in_desired_state &= kbase_pm_transition_core_type(kbdev, KBASE_PM_CORE_TILER, kbdev->pm.desired_tiler_state, 0, &tiler_available_bitmap, &kbdev->pm.powering_on_tiler_state);
		in_desired_state &= kbase_pm_transition_core_type(kbdev, KBASE_PM_CORE_SHADER, kbdev->pm.desired_shader_state, kbdev->shader_inuse_bitmap, &shader_available_bitmap, &kbdev->pm.powering_on_shader_state);

		if (kbdev->shader_available_bitmap != shader_available_bitmap) {
			KBASE_TRACE_ADD(kbdev, PM_CORES_CHANGE_AVAILABLE, NULL, NULL, 0u, (u32) shader_available_bitmap);
			KBASE_TIMELINE_POWER_SHADER(kbdev, shader_available_bitmap);
		}

		kbdev->shader_available_bitmap = shader_available_bitmap;

		if (kbdev->tiler_available_bitmap != tiler_available_bitmap) {
			KBASE_TRACE_ADD(kbdev, PM_CORES_CHANGE_AVAILABLE_TILER, NULL, NULL, 0u, (u32) tiler_available_bitmap);
			KBASE_TIMELINE_POWER_TILER(kbdev, tiler_available_bitmap);
		}

		kbdev->tiler_available_bitmap = tiler_available_bitmap;

	} else if ((l2_available_bitmap & kbdev->tiler_present_bitmap) != kbdev->tiler_present_bitmap) {
		tiler_available_bitmap = 0;

		if (kbdev->tiler_available_bitmap != tiler_available_bitmap) {
			KBASE_TIMELINE_POWER_TILER(kbdev, tiler_available_bitmap);
		}

		kbdev->tiler_available_bitmap = tiler_available_bitmap;
	}

	/* State updated for slow-path waiters */
	kbdev->pm.gpu_in_desired_state = in_desired_state;

	shader_ready_bitmap = kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_SHADER);
	shader_transitioning_bitmap = kbase_pm_get_trans_cores(kbdev, KBASE_PM_CORE_SHADER);

	/* Determine whether the cores are now available (even if the set of
	 * available cores is empty). Note that they can be available even if we've
	 * not finished transitioning to the desired state */
	if ((kbdev->shader_available_bitmap & kbdev->pm.desired_shader_state) == kbdev->pm.desired_shader_state
		&& (kbdev->tiler_available_bitmap & kbdev->pm.desired_tiler_state) == kbdev->pm.desired_tiler_state) {
		cores_are_available = MALI_TRUE;

		KBASE_TRACE_ADD(kbdev, PM_CORES_AVAILABLE, NULL, NULL, 0u, (u32)(kbdev->shader_available_bitmap & kbdev->pm.desired_shader_state));
		KBASE_TRACE_ADD(kbdev, PM_CORES_AVAILABLE_TILER, NULL, NULL, 0u, (u32)(kbdev->tiler_available_bitmap & kbdev->pm.desired_tiler_state));

		/* Log timelining information about handling events that power up
		 * cores, to match up either with immediate submission either because
		 * cores already available, or from PM IRQ */
		if (!in_desired_state)
			kbase_timeline_pm_send_event(kbdev, KBASE_TIMELINE_PM_EVENT_GPU_STATE_CHANGED);
	}

	if (in_desired_state) {
		KBASE_DEBUG_ASSERT(cores_are_available);

#ifdef CONFIG_MALI_GATOR_SUPPORT
		kbase_trace_mali_pm_status(KBASE_PM_CORE_L3, kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_L3));
		kbase_trace_mali_pm_status(KBASE_PM_CORE_L2, kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_L2));
		kbase_trace_mali_pm_status(KBASE_PM_CORE_SHADER, kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_SHADER));
		kbase_trace_mali_pm_status(KBASE_PM_CORE_TILER, kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_TILER));
#endif				/* CONFIG_MALI_GATOR_SUPPORT */

		KBASE_TRACE_ADD(kbdev, PM_DESIRED_REACHED, NULL, NULL, kbdev->pm.gpu_in_desired_state, (u32)kbdev->pm.desired_shader_state);
		KBASE_TRACE_ADD(kbdev, PM_DESIRED_REACHED_TILER, NULL, NULL, 0u, (u32)kbdev->pm.desired_tiler_state);

		/* Log timelining information for synchronous waiters */
		kbase_timeline_pm_send_event(kbdev, KBASE_TIMELINE_PM_EVENT_GPU_STATE_CHANGED);
		/* Wake slow-path waiters. Job scheduler does not use this. */
		KBASE_TRACE_ADD(kbdev, PM_WAKE_WAITERS, NULL, NULL, 0u, 0);
		wake_up(&kbdev->pm.gpu_in_desired_state_wait);
	}
	
	spin_unlock(&kbdev->pm.gpu_powered_lock);

	/* kbase_pm_ca_update_core_status can cause one-level recursion into
	 * this function, so it must only be called once all changes to kbdev
	 * have been committed, and after the gpu_powered_lock has been
	 * dropped. */
	if (kbdev->shader_ready_bitmap != shader_ready_bitmap ||
	    kbdev->shader_transitioning_bitmap != shader_transitioning_bitmap) {
		kbdev->shader_ready_bitmap = shader_ready_bitmap;
		kbdev->shader_transitioning_bitmap = shader_transitioning_bitmap;

		kbase_pm_ca_update_core_status(kbdev, shader_ready_bitmap, shader_transitioning_bitmap);
	}

	/* The core availability policy is not allowed to keep core group 0 off */
	if (!((shader_ready_bitmap | shader_transitioning_bitmap) & kbdev->gpu_props.props.coherency_info.group[0].core_mask) &&
	    !(kbase_pm_ca_get_core_mask(kbdev) & kbdev->gpu_props.props.coherency_info.group[0].core_mask))
		BUG();

	/* The core availability policy is allowed to keep core group 1 off, 
	 * but all jobs specifically targeting CG1 must fail */
	if (!((shader_ready_bitmap | shader_transitioning_bitmap) & kbdev->gpu_props.props.coherency_info.group[1].core_mask) &&
	    !(kbase_pm_ca_get_core_mask(kbdev) & kbdev->gpu_props.props.coherency_info.group[1].core_mask))
		kbdev->pm.cg1_disabled = MALI_TRUE;
	else
		kbdev->pm.cg1_disabled = MALI_FALSE;

	return cores_are_available;
}
KBASE_EXPORT_TEST_API(kbase_pm_check_transitions_nolock)

void kbase_pm_check_transitions_sync(struct kbase_device *kbdev)
{
	unsigned long flags;
	mali_bool cores_are_available;
	/* Force the transition to be checked and reported - the cores may be
	 * 'available' (for job submission) but not fully powered up. */
	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);
	cores_are_available = kbase_pm_check_transitions_nolock(kbdev);
	/* Don't need 'cores_are_available', because we don't return anything */
	CSTD_UNUSED(cores_are_available);
	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	/* Wait for cores */
	wait_event(kbdev->pm.gpu_in_desired_state_wait, kbdev->pm.gpu_in_desired_state);

	/* Log timelining information that a change in state has completed */
	kbase_timeline_pm_handle_event(kbdev, KBASE_TIMELINE_PM_EVENT_GPU_STATE_CHANGED);
}
KBASE_EXPORT_TEST_API(kbase_pm_check_transitions_sync)

void kbase_pm_enable_interrupts(kbase_device *kbdev)
{
	unsigned long flags;

	KBASE_DEBUG_ASSERT(NULL != kbdev);
	/*
	 * Clear all interrupts,
	 * and unmask them all.
	 */
	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_CLEAR), GPU_IRQ_REG_ALL, NULL);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), GPU_IRQ_REG_ALL, NULL);
	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	kbase_reg_write(kbdev, JOB_CONTROL_REG(JOB_IRQ_CLEAR), 0xFFFFFFFF, NULL);
	kbase_reg_write(kbdev, JOB_CONTROL_REG(JOB_IRQ_MASK), 0xFFFFFFFF, NULL);

	kbase_reg_write(kbdev, MMU_REG(MMU_IRQ_CLEAR), 0xFFFFFFFF, NULL);
	kbase_reg_write(kbdev, MMU_REG(MMU_IRQ_MASK), 0xFFFFFFFF, NULL);
}

KBASE_EXPORT_TEST_API(kbase_pm_enable_interrupts)

void kbase_pm_disable_interrupts(kbase_device *kbdev)
{
	unsigned long flags;

	KBASE_DEBUG_ASSERT(NULL != kbdev);
	/*
	 * Mask all interrupts,
	 * and clear them all.
	 */
	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), 0, NULL);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_CLEAR), GPU_IRQ_REG_ALL, NULL);
	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	kbase_reg_write(kbdev, JOB_CONTROL_REG(JOB_IRQ_MASK), 0, NULL);
	kbase_reg_write(kbdev, JOB_CONTROL_REG(JOB_IRQ_CLEAR), 0xFFFFFFFF, NULL);

	kbase_reg_write(kbdev, MMU_REG(MMU_IRQ_MASK), 0, NULL);
	kbase_reg_write(kbdev, MMU_REG(MMU_IRQ_CLEAR), 0xFFFFFFFF, NULL);
}

KBASE_EXPORT_TEST_API(kbase_pm_disable_interrupts)

/*
 * pmu layout:
 * 0x0000: PMU TAG (RO) (0xCAFECAFE)
 * 0x0004: PMU VERSION ID (RO) (0x00000000)
 * 0x0008: CLOCK ENABLE (RW) (31:1 SBZ, 0 CLOCK STATE)
 */
void kbase_pm_clock_on(kbase_device *kbdev, mali_bool is_resume)
{
	mali_bool reset_required = is_resume;
	unsigned long flags;
	KBASE_DEBUG_ASSERT(NULL != kbdev);
	lockdep_assert_held(&kbdev->pm.lock);

	if (kbdev->pm.gpu_powered) {
		/* Already turned on */
		KBASE_DEBUG_ASSERT(!is_resume);
		return;
	}

	KBASE_TRACE_ADD(kbdev, PM_GPU_ON, NULL, NULL, 0u, 0u);

	if (is_resume && kbdev->pm.callback_power_resume) {
		kbdev->pm.callback_power_resume(kbdev);
	} else if (kbdev->pm.callback_power_on) {
		if (kbdev->pm.callback_power_on(kbdev))
			reset_required = MALI_TRUE;
	}

#ifdef GPU_EARLY_CLK_GATING
	spin_lock_irqsave(&kbdev->pm.gpu_powered_lock, flags);
	kbdev->pm.gpu_powered = MALI_TRUE;
	spin_unlock_irqrestore(&kbdev->pm.gpu_powered_lock, flags);
#endif /* GPU_EARLY_CLK_GATING */

	if (reset_required) {
		/* GPU state was lost, reset GPU to ensure it is in a
		 * consistent state */
		kbase_pm_init_hw(kbdev, MALI_TRUE);
	}

#ifndef GPU_EARLY_CLK_GATING
	spin_lock_irqsave(&kbdev->pm.gpu_powered_lock, flags);
	kbdev->pm.gpu_powered = MALI_TRUE;
	spin_unlock_irqrestore(&kbdev->pm.gpu_powered_lock, flags);
#endif /* GPU_EARLY_CLK_GATING */

	/* Lastly, enable the interrupts */
	kbase_pm_enable_interrupts(kbdev);
}

KBASE_EXPORT_TEST_API(kbase_pm_clock_on)

void kbase_pm_clock_off(kbase_device *kbdev, mali_bool is_suspend)
{
	unsigned long flags;
	KBASE_DEBUG_ASSERT(NULL != kbdev);
	lockdep_assert_held(&kbdev->pm.lock);

	/* ASSERT that the cores should now be unavailable. No lock needed. */
	KBASE_DEBUG_ASSERT(kbdev->shader_available_bitmap == 0u);

	if (!kbdev->pm.gpu_powered) {
		/* Already turned off */
		if (is_suspend && kbdev->pm.callback_power_suspend)
			kbdev->pm.callback_power_suspend(kbdev);
		return;
	}

	KBASE_TRACE_ADD(kbdev, PM_GPU_OFF, NULL, NULL, 0u, 0u);

	/* Disable interrupts. This also clears any outstanding interrupts */
	kbase_pm_disable_interrupts(kbdev);
	/* Ensure that any IRQ handlers have finished */
	kbase_synchronize_irqs(kbdev);

	/* The GPU power may be turned off from this point */
	spin_lock_irqsave(&kbdev->pm.gpu_powered_lock, flags);
	kbdev->pm.gpu_powered = MALI_FALSE;
	spin_unlock_irqrestore(&kbdev->pm.gpu_powered_lock, flags);

	if (is_suspend && kbdev->pm.callback_power_suspend)
		kbdev->pm.callback_power_suspend(kbdev);
	else if (kbdev->pm.callback_power_off)
		kbdev->pm.callback_power_off(kbdev);
}

KBASE_EXPORT_TEST_API(kbase_pm_clock_off)

struct kbasep_reset_timeout_data {
	struct hrtimer timer;
	mali_bool timed_out;
	kbase_device *kbdev;
};

void kbase_pm_reset_done(kbase_device *kbdev)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);
	kbdev->pm.reset_done = MALI_TRUE;
	wake_up(&kbdev->pm.reset_done_wait);
}

/**
 * Wait for the RESET_COMPLETED IRQ to occur, then reset the waiting state.
 */
STATIC void kbase_pm_wait_for_reset(kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->pm.lock);

	wait_event(kbdev->pm.reset_done_wait, (kbdev->pm.reset_done));
	kbdev->pm.reset_done = MALI_FALSE;
}

KBASE_EXPORT_TEST_API(kbase_pm_reset_done)

static enum hrtimer_restart kbasep_reset_timeout(struct hrtimer *timer)
{
	struct kbasep_reset_timeout_data *rtdata = container_of(timer, struct kbasep_reset_timeout_data, timer);

	rtdata->timed_out = 1;

	/* Set the wait queue to wake up kbase_pm_init_hw even though the reset hasn't completed */
	kbase_pm_reset_done(rtdata->kbdev);

	return HRTIMER_NORESTART;
}

static void kbase_pm_hw_issues(kbase_device *kbdev)
{
	u32 value = 0;
	u32 config_value;

	/* Needed due to MIDBASE-1494: LS_PAUSEBUFFER_DISABLE. See PRLAM-8443. */
	if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8443))
		value |= SC_LS_PAUSEBUFFER_DISABLE;

	/* Needed due to MIDBASE-2054: SDC_DISABLE_OQ_DISCARD. See PRLAM-10327. */
	if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_10327))
		value |= SC_SDC_DISABLE_OQ_DISCARD;

	/* Enable alternative hardware counter selection if configured. */
	if (kbasep_get_config_value(kbdev, kbdev->config_attributes, KBASE_CONFIG_ATTR_ALTERNATIVE_HWC))
		value |= SC_ALT_COUNTERS;

	/* Use software control of forward pixel kill when needed. See MIDEUR-174. */
	if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_T76X_2121))
		value |= SC_OVERRIDE_FWD_PIXEL_KILL;

	/* Needed due to MIDBASE-2795. ENABLE_TEXGRD_FLAGS. See PRLAM-10797. */
	if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_10797))
		value |= SC_ENABLE_TEXGRD_FLAGS;

	if (value != 0)
		kbase_reg_write(kbdev, GPU_CONTROL_REG(SHADER_CONFIG), value, NULL);

	/* Limit the GPU bus bandwidth if the platform needs this. */
	value = kbase_reg_read(kbdev, GPU_CONTROL_REG(L2_MMU_CONFIG), NULL);

	/* Limit read ID width for AXI */
	config_value = (u32) kbasep_get_config_value(kbdev, kbdev->config_attributes, KBASE_CONFIG_ATTR_ARID_LIMIT);
	value &= ~(L2_MMU_CONFIG_LIMIT_EXTERNAL_READS);
	value |= (config_value & 0x3) << L2_MMU_CONFIG_LIMIT_EXTERNAL_READS_SHIFT;

	/* Limit write ID width for AXI */
	config_value = (u32) kbasep_get_config_value(kbdev, kbdev->config_attributes, KBASE_CONFIG_ATTR_AWID_LIMIT);
	value &= ~(L2_MMU_CONFIG_LIMIT_EXTERNAL_WRITES);
	value |= (config_value & 0x3) << L2_MMU_CONFIG_LIMIT_EXTERNAL_WRITES_SHIFT;

	kbase_reg_write(kbdev, GPU_CONTROL_REG(L2_MMU_CONFIG), value, NULL);
}

mali_error kbase_pm_init_hw(kbase_device *kbdev, mali_bool enable_irqs )
{
	unsigned long flags;
	struct kbasep_reset_timeout_data rtdata;

	KBASE_DEBUG_ASSERT(NULL != kbdev);
	lockdep_assert_held(&kbdev->pm.lock);

	/* Ensure the clock is on before attempting to access the hardware */
	if (!kbdev->pm.gpu_powered) {
		if (kbdev->pm.callback_power_on)
			kbdev->pm.callback_power_on(kbdev);

		spin_lock_irqsave(&kbdev->pm.gpu_powered_lock, flags);
		kbdev->pm.gpu_powered = MALI_TRUE;
		spin_unlock_irqrestore(&kbdev->pm.gpu_powered_lock, flags);
	}

	/* Ensure interrupts are off to begin with, this also clears any outstanding interrupts */
	kbase_pm_disable_interrupts(kbdev);

	/* Prepare for the soft-reset */
	kbdev->pm.reset_done = MALI_FALSE;

	/* The cores should be made unavailable due to the reset */
	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);
	if (kbdev->shader_available_bitmap != 0u)
			KBASE_TRACE_ADD(kbdev, PM_CORES_CHANGE_AVAILABLE, NULL, NULL, 0u, (u32)0u);
	if (kbdev->tiler_available_bitmap != 0u)
			KBASE_TRACE_ADD(kbdev, PM_CORES_CHANGE_AVAILABLE_TILER, NULL, NULL, 0u, (u32)0u);
	kbdev->shader_available_bitmap = 0u;
	kbdev->tiler_available_bitmap = 0u;
	kbdev->l2_available_bitmap = 0u;
	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	/* Soft reset the GPU */
	KBASE_TRACE_ADD(kbdev, CORE_GPU_SOFT_RESET, NULL, NULL, 0u, 0);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_SOFT_RESET, NULL);

	/* Unmask the reset complete interrupt only */
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), RESET_COMPLETED, NULL);

	/* Initialize a structure for tracking the status of the reset */
	rtdata.kbdev = kbdev;
	rtdata.timed_out = 0;

	/* Create a timer to use as a timeout on the reset */
	hrtimer_init_on_stack(&rtdata.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rtdata.timer.function = kbasep_reset_timeout;

	hrtimer_start(&rtdata.timer, HR_TIMER_DELAY_MSEC(RESET_TIMEOUT), HRTIMER_MODE_REL);

	/* Wait for the RESET_COMPLETED interrupt to be raised */
	kbase_pm_wait_for_reset(kbdev);

	if (rtdata.timed_out == 0) {
		/* GPU has been reset */
		hrtimer_cancel(&rtdata.timer);
		destroy_hrtimer_on_stack(&rtdata.timer);
		goto out;
	}

	/* No interrupt has been received - check if the RAWSTAT register says the reset has completed */
	if (kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_IRQ_RAWSTAT), NULL) & RESET_COMPLETED) {
		/* The interrupt is set in the RAWSTAT; this suggests that the interrupts are not getting to the CPU */
		KBASE_DEBUG_PRINT_WARN(KBASE_PM, "Reset interrupt didn't reach CPU. Check interrupt assignments.\n");
		/* If interrupts aren't working we can't continue. */
		destroy_hrtimer_on_stack(&rtdata.timer);
		goto out;
	}

	/* The GPU doesn't seem to be responding to the reset so try a hard reset */
	KBASE_DEBUG_PRINT_ERROR(KBASE_PM, "Failed to soft-reset GPU (timed out after %d ms), now attempting a hard reset\n", RESET_TIMEOUT);
	KBASE_TRACE_ADD(kbdev, CORE_GPU_HARD_RESET, NULL, NULL, 0u, 0);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_HARD_RESET, NULL);

	/* Restart the timer to wait for the hard reset to complete */
	rtdata.timed_out = 0;

	hrtimer_start(&rtdata.timer, HR_TIMER_DELAY_MSEC(RESET_TIMEOUT), HRTIMER_MODE_REL);

	/* Wait for the RESET_COMPLETED interrupt to be raised */
	kbase_pm_wait_for_reset(kbdev);

	if (rtdata.timed_out == 0) {
		/* GPU has been reset */
		hrtimer_cancel(&rtdata.timer);
		destroy_hrtimer_on_stack(&rtdata.timer);
		goto out;
	}

	destroy_hrtimer_on_stack(&rtdata.timer);

	KBASE_DEBUG_PRINT_ERROR(KBASE_PM, "Failed to hard-reset the GPU (timed out after %d ms)\n", RESET_TIMEOUT);

	/* The GPU still hasn't reset, give up */
	return MALI_ERROR_FUNCTION_FAILED;

 out:
	/* Re-enable interrupts if requested*/
	if ( enable_irqs )
	{
		kbase_pm_enable_interrupts(kbdev);
	}
	/* If cycle counter was in use-re enable it */
	spin_lock_irqsave(&kbdev->pm.gpu_cycle_counter_requests_lock, flags);

	if (kbdev->pm.gpu_cycle_counter_requests)
		kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_CYCLE_COUNT_START, NULL);

	spin_unlock_irqrestore(&kbdev->pm.gpu_cycle_counter_requests_lock, flags);

	kbase_pm_hw_issues(kbdev);

	return MALI_ERROR_NONE;
}

KBASE_EXPORT_TEST_API(kbase_pm_init_hw)

void kbase_pm_request_gpu_cycle_counter(kbase_device *kbdev)
{
	unsigned long flags;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	KBASE_DEBUG_ASSERT(kbdev->pm.gpu_powered);

	spin_lock_irqsave(&kbdev->pm.gpu_cycle_counter_requests_lock, flags);

	KBASE_DEBUG_ASSERT(kbdev->pm.gpu_cycle_counter_requests < INT_MAX);

	++kbdev->pm.gpu_cycle_counter_requests;

	if (1 == kbdev->pm.gpu_cycle_counter_requests)
		kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_CYCLE_COUNT_START, NULL);

	spin_unlock_irqrestore(&kbdev->pm.gpu_cycle_counter_requests_lock, flags);
}

KBASE_EXPORT_TEST_API(kbase_pm_request_gpu_cycle_counter)

void kbase_pm_release_gpu_cycle_counter(kbase_device *kbdev)
{
	unsigned long flags;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.gpu_cycle_counter_requests_lock, flags);

	KBASE_DEBUG_ASSERT(kbdev->pm.gpu_cycle_counter_requests > 0);

	--kbdev->pm.gpu_cycle_counter_requests;

	if (0 == kbdev->pm.gpu_cycle_counter_requests)
		kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_CYCLE_COUNT_STOP, NULL);

	spin_unlock_irqrestore(&kbdev->pm.gpu_cycle_counter_requests_lock, flags);
}

KBASE_EXPORT_TEST_API(kbase_pm_release_gpu_cycle_counter)
