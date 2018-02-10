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
 * @file
 * Run-time work-arounds helpers
 */

#include <mali_base_hwconfig.h>
#include <mali_midg_regmap.h>
#include "mali_kbase.h"
#include "mali_kbase_hw.h"

void kbase_hw_set_features_mask(kbase_device *kbdev)
{
	const base_hw_feature *features;
	u32 gpu_id;

	gpu_id = kbdev->gpu_props.props.raw_props.gpu_id;

	switch (gpu_id) {
	case GPU_ID_MAKE(GPU_ID_PI_T76X, 0, 0, 0):
	case GPU_ID_MAKE(GPU_ID_PI_T76X, 0, 0, 1):
	case GPU_ID_MAKE(GPU_ID_PI_T76X, 0, 1, 1):
		features = base_hw_features_t76x;
		break;
	default:
		features = base_hw_features_generic;
		break;
	}

	for (; *features != BASE_HW_FEATURE_END; features++)
		set_bit(*features, &kbdev->hw_features_mask[0]);
}

mali_error kbase_hw_set_issues_mask(kbase_device *kbdev)
{
	const base_hw_issue *issues;
	u32 gpu_id;
	u32 impl_tech;

	gpu_id = kbdev->gpu_props.props.raw_props.gpu_id;
	impl_tech = kbdev->gpu_props.props.thread_props.impl_tech;

	if (impl_tech != IMPLEMENTATION_MODEL) {
		switch (gpu_id) {
		case GPU_ID_MAKE(GPU_ID_PI_T60X, 0, 0, GPU_ID_S_15DEV0):
			issues = base_hw_issues_t60x_r0p0_15dev0;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T60X, 0, 0, GPU_ID_S_EAC):
			issues = base_hw_issues_t60x_r0p0_eac;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T60X, 0, 1, 0):
			issues = base_hw_issues_t60x_r0p1;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T62X, 0, 1, 0):
			issues = base_hw_issues_t62x_r0p1;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T62X, 1, 0, 0):
		case GPU_ID_MAKE(GPU_ID_PI_T62X, 1, 0, 1):
			issues = base_hw_issues_t62x_r1p0;
	 		break;
		case GPU_ID_MAKE(GPU_ID_PI_T67X, 1, 0, 0):
		case GPU_ID_MAKE(GPU_ID_PI_T67X, 1, 0, 1):
			issues = base_hw_issues_t67x_r1p0;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T76X, 0, 0, 0):
			issues = base_hw_issues_t76x_r0p0_beta;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T76X, 0, 0, 1):
			issues = base_hw_issues_t76x_r0p0;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T76X, 0, 1, 1):
			issues = base_hw_issues_t76x_r0p1;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T72X, 0, 0, 0):
		case GPU_ID_MAKE(GPU_ID_PI_T72X, 0, 0, 1):
		case GPU_ID_MAKE(GPU_ID_PI_T72X, 0, 0, 2):
			issues = base_hw_issues_t72x_r0p0;
			break;
		default:
			KBASE_DEBUG_PRINT_ERROR(KBASE_CORE, "Unknown GPU ID %x", gpu_id);
			return MALI_ERROR_FUNCTION_FAILED;
		}
	} else {
		/* Software model */
		switch (gpu_id >> GPU_ID_VERSION_PRODUCT_ID_SHIFT) {
		case GPU_ID_PI_T60X:
		case GPU_ID_PI_T62X:
		case GPU_ID_PI_T67X:
			issues = base_hw_issues_model_t6xx;
			break;
		case GPU_ID_PI_T72X:
			issues = base_hw_issues_model_t72x;
			break;
		case GPU_ID_PI_T76X:
			issues = base_hw_issues_model_t7xx;
			break;

		default:
			KBASE_DEBUG_PRINT_ERROR(KBASE_CORE, "Unknown GPU ID %x", gpu_id);
			return MALI_ERROR_FUNCTION_FAILED;
		}
	}

	KBASE_DEBUG_PRINT_INFO(KBASE_CORE, "GPU identified as 0x%04x r%dp%d status %d", (gpu_id & GPU_ID_VERSION_PRODUCT_ID) >> GPU_ID_VERSION_PRODUCT_ID_SHIFT, (gpu_id & GPU_ID_VERSION_MAJOR) >> GPU_ID_VERSION_MAJOR_SHIFT, (gpu_id & GPU_ID_VERSION_MINOR) >> GPU_ID_VERSION_MINOR_SHIFT, (gpu_id & GPU_ID_VERSION_STATUS) >> GPU_ID_VERSION_STATUS_SHIFT);

	for (; *issues != BASE_HW_ISSUE_END; issues++)
		set_bit(*issues, &kbdev->hw_issues_mask[0]);

	return MALI_ERROR_NONE;
}
