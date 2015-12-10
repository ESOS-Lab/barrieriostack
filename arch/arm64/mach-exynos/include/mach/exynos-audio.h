/* linux/arch/arm/mach-exynos/include/mach/exynos-audio.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _EXYNOS_AUDIO_H
#define _EXYNOS_AUDIO_H __FILE__

#include <mach/regs-pmu.h>

static inline void exynos5_audio_set_mclk(bool enable, bool forced)
{
	static unsigned int mclk_usecount = 0;

	pr_debug("%s: %s forced %d, use_cnt %d\n", __func__,
				enable ? "on" : "off", forced, mclk_usecount);

	/* forced disable */
	if (forced && !enable) {
		pr_info("%s: mclk disbled\n", __func__);
		mclk_usecount = 0;
		writel(0x1001, EXYNOS_PMU_PMU_DEBUG);
		return;
	}

	if (enable) {
		if (mclk_usecount == 0) {
			pr_info("%s: mclk enabled\n", __func__);
			writel(0x1000, EXYNOS_PMU_PMU_DEBUG);
		}
		mclk_usecount++;
	} else {
		if (--mclk_usecount > 0)
			return;
		writel(0x1001, EXYNOS_PMU_PMU_DEBUG);
		pr_info("%s: mclk disabled\n", __func__);
	}
}


#endif /* _EXYNOS_AUDIO_H */

