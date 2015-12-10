/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *		http://www.samsung.com/
 *
 * Chip Abstraction Layer for System power down support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PMU_CAL_SYS_H__
#define __PMU_CAL_SYS_H__

#ifdef CONFIG_CAL_SYS_PWRDOWN
#if defined(CONFIG_SOC_EXYNOS5433)
#include <mach/pmu_cal_sys_exynos5433.h>
#elif defined(CONFIG_SOC_EXYNOS7420)
#include <mach/pmu_cal_sys_exynos7420.h>
#endif

extern void (*exynos_sys_powerdown_conf)(enum sys_powerdown mode);
extern void (*exynos_central_sequencer_ctrl)(bool enable);
extern void pmu_cal_sys_init(void);
#endif

#endif /* __PMU_CAL_SYS_H__ */
