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

#include <linux/types.h>
#include <mach/pmu.h>

void (*exynos_sys_powerdown_conf)(enum sys_powerdown mode);
void (*exynos_central_sequencer_ctrl)(bool enable);
