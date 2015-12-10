/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <mach/exynos-fimc-is-companion.h>
#include <mach/exynos-fimc-is.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-config.h"
#include "fimc-is-companion-dt.h"
#include "fimc-is-dt.h"
#include "fimc-is-core.h"

#ifdef CONFIG_OF
int fimc_is_companion_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is_companion *pdata;
	struct device_node *dnode;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_companion), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_string(dnode, "comp_int_pin", (const char **)&pdata->comp_int_pin);
	if (ret) {
		err("fail to read, comp_paf_int");
		ret = -EINVAL;
		goto p_err;
	}

	ret = of_property_read_string(dnode, "comp_int_pinctrl", (const char **)&pdata->comp_int_pinctrl);
	if (ret) {
		err("fail to read, comp_int_pinctrl");
		ret = -EINVAL;
		goto p_err;
	}

	pdata->iclk_cfg = exynos_fimc_is_companion_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_companion_iclk_on;
	pdata->iclk_off = exynos_fimc_is_companion_iclk_off;
	pdata->mclk_on = exynos_fimc_is_companion_mclk_on;
	pdata->mclk_off = exynos_fimc_is_companion_mclk_off;

	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}
#endif
