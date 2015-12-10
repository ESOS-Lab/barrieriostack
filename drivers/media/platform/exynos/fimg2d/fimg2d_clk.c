/* linux/drivers/media/video/exynos/fimg2d/fimg2d_clk.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <plat/cpu.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"

#include <../drivers/clk/samsung/clk.h>

void fimg2d_clk_on(struct fimg2d_control *ctrl)
{
	if (ip_is_g2d_5ar2() || ip_is_g2d_5h() ||
			ip_is_g2d_5hp() || ip_is_g2d_7i()) {
		clk_enable(ctrl->clock);
		clk_enable(ctrl->qe_clock);
	} else
		clk_enable(ctrl->clock);

	fimg2d_debug("%s : clock enable\n", __func__);
}

void fimg2d_clk_off(struct fimg2d_control *ctrl)
{
	if (ip_is_g2d_5ar2() || ip_is_g2d_5h() ||
			ip_is_g2d_5hp() || ip_is_g2d_7i()) {
		clk_disable(ctrl->clock);
		clk_disable(ctrl->qe_clock);
	} else
		clk_disable(ctrl->clock);

	fimg2d_debug("%s : clock disable\n", __func__);
}

#ifdef CONFIG_OF
int exynos7420_fimg2d_clk_setup(struct fimg2d_control *ctrl)
{
	struct fimg2d_platdata *pdata;

	pdata = ctrl->pdata;

	of_property_read_string_index(ctrl->dev->of_node,
			"clock-names", 0, (const char **)&(pdata->gate_clkname));
	of_property_read_string_index(ctrl->dev->of_node,
			"clock-names", 1, &(pdata->gate_clkname2));

	ctrl->qe_clock = devm_clk_get(ctrl->dev, pdata->gate_clkname2);
	if (IS_ERR(ctrl->qe_clock)) {
		if (PTR_ERR(ctrl->qe_clock) == -ENOENT)
			/* clock is not present */
			ctrl->qe_clock = NULL;
		else
			return PTR_ERR(ctrl->qe_clock);
		dev_info(ctrl->dev, "'gate_bts' clock is not present\n");
	}

	fimg2d_info("Done fimg2d clock setup\n");
	return 0;
}

int exynos5430_fimg2d_clk_setup(struct fimg2d_control *ctrl)
{
	struct fimg2d_platdata *pdata;

	pdata = ctrl->pdata;

	of_property_read_string_index(ctrl->dev->of_node,
			"clock-names", 0, (const char **)&(pdata->gate_clkname));

	ctrl->clk_osc = devm_clk_get(ctrl->dev, "fin_pll");
	if (IS_ERR(ctrl->clk_osc)) {
		if (PTR_ERR(ctrl->clk_osc) == -ENOENT)
			/* clock is not present */
			ctrl->clk_osc = NULL;
		else
			return PTR_ERR(ctrl->clk_osc);
		dev_info(ctrl->dev, "'fin_pll' clock is not present\n");
	}

	ctrl->qe_clock = devm_clk_get(ctrl->dev, "gate_qe_g2d");
	if (IS_ERR(ctrl->qe_clock)) {
		if (PTR_ERR(ctrl->qe_clock) == -ENOENT)
			/* clock is not present */
			ctrl->qe_clock = NULL;
		else
			return PTR_ERR(ctrl->qe_clock);
		dev_info(ctrl->dev, "'gate_qe_g2d' clock is not present\n");
	}

	ctrl->clk_parn1 = devm_clk_get(ctrl->dev, "mux_400_parent");
	if (IS_ERR(ctrl->clk_parn1)) {
		if (PTR_ERR(ctrl->clk_parn1) == -ENOENT)
			/* clock is not present */
			ctrl->clk_parn1 = NULL;
		else
			return PTR_ERR(ctrl->clk_parn1);
		dev_info(ctrl->dev, "'mux_400_parent' clock is not present\n");
	}

	ctrl->clk_chld1 = devm_clk_get(ctrl->dev, "mux_400_child");
	if (IS_ERR(ctrl->clk_chld1)) {
		if (PTR_ERR(ctrl->clk_chld1) == -ENOENT)
			/* clock is not present */
			ctrl->clk_chld1 = NULL;
		else
			return PTR_ERR(ctrl->clk_chld1);
		dev_info(ctrl->dev, "'mux_400_chlid' clock is not present\n");
	}

	ctrl->clk_parn2 = devm_clk_get(ctrl->dev, "mux_266_parent");
	if (IS_ERR(ctrl->clk_parn2)) {
		if (PTR_ERR(ctrl->clk_parn2) == -ENOENT)
			/* clock is not present */
			ctrl->clk_parn2 = NULL;
		else
			return PTR_ERR(ctrl->clk_parn2);
		dev_info(ctrl->dev, "'mux_266_parent' clock is not present\n");
	}

	ctrl->clk_chld2 = devm_clk_get(ctrl->dev, "mux_266_child");
	if (IS_ERR(ctrl->clk_chld2)) {
		if (PTR_ERR(ctrl->clk_chld2) == -ENOENT)
			/* clock is not present */
			ctrl->clk_chld2 = NULL;
		else
			return PTR_ERR(ctrl->clk_chld2);
		dev_info(ctrl->dev, "'mux_266_child' clock is not present\n");
	}

	fimg2d_debug("fimg2d clk name: %s, %s clkrate: %lu, %s clkrate: %lu\n",
			pdata->gate_clkname,
			"aclk_g2d_400", clk_get_rate(ctrl->clk_parn1),
			"aclk_g2d_266", clk_get_rate(ctrl->clk_parn2));

	fimg2d_info("Done fimg2d clock setup\n");
	return 0;
}

int exynos5430_fimg2d_clk_set(struct fimg2d_control *ctrl)
{
	if (clk_set_parent(ctrl->clk_chld1, ctrl->clk_parn1))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_user", "aclk_g2d_400");

	if (clk_set_parent(ctrl->clk_chld2, ctrl->clk_parn2))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_266_user", "aclk_g2d_266");

	return 0;
}

int exynos5433_fimg2d_clk_set_osc(struct fimg2d_control *ctrl)
{
	clk_set_parent(ctrl->clk_chld1, ctrl->clk_osc);
	clk_set_parent(ctrl->clk_chld2, ctrl->clk_osc);

	return 0;
}
#endif

int fimg2d_clk_setup(struct fimg2d_control *ctrl)
{
	struct fimg2d_platdata *pdata;
	struct clk *parent, *sclk;
	int ret = 0;

	sclk = parent = NULL;
#ifdef CONFIG_OF
	pdata = ctrl->pdata;
#else
	pdata = to_fimg2d_plat(ctrl->dev);
#endif

	if (ip_is_g2d_5g() || ip_is_g2d_5a()) {
		fimg2d_info("aclk_acp(%lu) pclk_acp(%lu)\n",
				clk_get_rate(clk_get(NULL, "aclk_acp")),
				clk_get_rate(clk_get(NULL, "pclk_acp")));
	} else if (ip_is_g2d_5ar2()) {
		of_property_read_string_index(ctrl->dev->of_node,
				"clock-names", 0, (const char **)&(pdata->gate_clkname));
		of_property_read_string_index(ctrl->dev->of_node,
				"clock-names", 1, &(pdata->gate_clkname2));

		ctrl->qe_clock = clk_get(ctrl->dev, pdata->gate_clkname2);
		if (IS_ERR(ctrl->qe_clock)) {
			dev_err(ctrl->dev, "failed to get clk_get():%s\n",
					pdata->gate_clkname2);
			ret = -ENOENT;
			goto err_clk2;

		}
	} else if (ip_is_g2d_5h() || ip_is_g2d_5hp()) {
#ifdef CONFIG_OF
		if (exynos5430_fimg2d_clk_setup(ctrl)) {
			fimg2d_err("failed to setup clk\n");
			ret = -ENOENT;
			goto err_clk1;
		}
#endif
	} else if (ip_is_g2d_7i()) {
#ifdef CONFIG_OF
		if (exynos7420_fimg2d_clk_setup(ctrl)) {
			fimg2d_err("failed to setup clk\n");
			ret = -ENOENT;
			goto err_clk1;
		}
#endif
	} else {
		sclk = clk_get(ctrl->dev, pdata->clkname);
		if (IS_ERR(sclk)) {
			fimg2d_err("failed to get fimg2d clk\n");
			ret = -ENOENT;
			goto err_clk1;
		}
		fimg2d_info("fimg2d clk name: %s clkrate: %ld\n",
				pdata->clkname, clk_get_rate(sclk));
	}

	/* clock for gating */
	ctrl->clock = clk_get(ctrl->dev, pdata->gate_clkname);
	if (IS_ERR(ctrl->clock)) {
		fimg2d_err("failed to get gate clk\n");
		ret = -ENOENT;
		goto err_clk2;
	}

	if (ip_is_g2d_5ar2() || ip_is_g2d_5h() ||
			ip_is_g2d_5hp() || ip_is_g2d_7i()) {
		if (clk_prepare(ctrl->clock))
			fimg2d_err("failed to prepare gate clock\n");

		if (clk_prepare(ctrl->qe_clock))
			fimg2d_err("failed to prepare gate qe_clock\n");
	}

	fimg2d_info("gate clk: %s\n", pdata->gate_clkname);

	return ret;

err_clk2:
	if (sclk)
		clk_put(sclk);
	if (ctrl->clock)
		clk_put(ctrl->clock);
	if (ctrl->qe_clock)
		clk_put(ctrl->qe_clock);

err_clk1:
	return ret;
}

void fimg2d_clk_release(struct fimg2d_control *ctrl)
{
	clk_put(ctrl->clock);

	if (ip_is_g2d_5ar2() || ip_is_g2d_5h() ||
			ip_is_g2d_5hp() || ip_is_g2d_7i()) {
		clk_unprepare(ctrl->clock);
		clk_unprepare(ctrl->qe_clock);
	} else
		clk_unprepare(ctrl->clock);


	if (ip_is_g2d_5ar2() || ip_is_g2d_5h() ||
			ip_is_g2d_5hp() || ip_is_g2d_7i())
		clk_put(ctrl->qe_clock);

	if (ip_is_g2d_4p()) {
		struct fimg2d_platdata *pdata;
#ifdef CONFIG_OF
		pdata = ctrl->pdata;
#else
		pdata = to_fimg2d_plat(ctrl->dev);
#endif
		clk_put(clk_get(ctrl->dev, pdata->clkname));
		clk_put(clk_get(ctrl->dev, pdata->parent_clkname));
	}
}

int fimg2d_clk_set_gate(struct fimg2d_control *ctrl)
{
	/* CPLL:666MHz */
	struct clk *aclk_g2d_333, *aclk_g2d_333_nogate;
	struct fimg2d_platdata *pdata;
	int ret = 0;

#ifdef CONFIG_OF
	pdata = ctrl->pdata;
#else
	pdata = to_fimg2d_plat(ctrl->dev);
#endif

	aclk_g2d_333 = clk_get(NULL, "aclk_g2d_333");
	if (IS_ERR(aclk_g2d_333)) {
		pr_err("failed to get %s clock\n", "aclk_g2d_333");
		ret = PTR_ERR(aclk_g2d_333);
		goto err_clk1;
	}

	aclk_g2d_333_nogate = clk_get(NULL, "aclk_g2d_333_nogate");
	if (IS_ERR(aclk_g2d_333_nogate)) {
		pr_err("failed to get %s clock\n", "aclk_g2d_333_nogate");
		ret = PTR_ERR(aclk_g2d_333_nogate);
		goto err_clk2;
	}

	if (clk_set_parent(aclk_g2d_333_nogate, aclk_g2d_333))
		pr_err("Unable to set parent %s of clock %s\n",
			"aclk_g2d_333", "aclk_g2d_333_nogate");

	/* clock for gating */
	ctrl->clock = clk_get(ctrl->dev, pdata->gate_clkname);
	if (IS_ERR(ctrl->clock)) {
		fimg2d_err("failed to get gate clk\n");
		ret = -ENOENT;
		goto err_clk3;
	}
	fimg2d_debug("gate clk: %s\n", pdata->gate_clkname);

err_clk3:
	if (aclk_g2d_333_nogate)
		clk_put(aclk_g2d_333_nogate);
err_clk2:
	if (aclk_g2d_333)
		clk_put(aclk_g2d_333);
err_clk1:

	return ret;
}
