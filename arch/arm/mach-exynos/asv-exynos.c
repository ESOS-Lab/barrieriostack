/* linux/arch/arm/mach-exynos/asv-exynos.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5 - ASV(Adoptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/asv-exynos.h>

static LIST_HEAD(asv_list);
static DEFINE_MUTEX(asv_mutex);

#ifdef CONFIG_ASV_MARGIN_TEST
#define MARGIN_UNIT	(12500)
#define ID_NAME(x)	\
	(x == ID_ARM) ? "ARM" : \
	((x == ID_KFC) ? "KFC" : \
	((x == ID_INT) ? "INT" : \
	((x == ID_MIF) ? "MIF" : \
	((x == ID_G3D) ? "G3D" : \
	((x == ID_ISP) ? "ISP" : \
	"OTHERS")))))

static int offset_percent;
static int __init get_offset_volt(char *str)
{
	get_option(&str, &offset_percent);
	return 0;
}
early_param("volt_offset_percent", get_offset_volt);
#endif

void add_asv_member(struct asv_info *exynos_asv_info)
{
	mutex_lock(&asv_mutex);
	list_add_tail(&exynos_asv_info->node, &asv_list);
	mutex_unlock(&asv_mutex);
}

struct asv_info *asv_get(enum asv_type_id exynos_asv_type_id)
{
	struct asv_info *match_asv_info;

	list_for_each_entry(match_asv_info, &asv_list, node)
		if (exynos_asv_type_id == match_asv_info->asv_type)
			return match_asv_info;

	return 0;
}

unsigned int get_match_volt(enum asv_type_id target_type, unsigned int target_freq)
{
	struct asv_info *match_asv_info = asv_get(target_type);
	unsigned int target_dvfs_level;
	unsigned int i;
#ifdef CONFIG_ASV_MARGIN_TEST
	int actual_volt = 0;
	int margin_volt;
#endif

	if (!match_asv_info) {
		pr_info("EXYNOS ASV: failed to get_match_volt(type: %d)\n", target_type);
		return 0;
	}

	target_dvfs_level = match_asv_info->dvfs_level_nr;

	for (i = 0; i < target_dvfs_level; i++) {
		if (match_asv_info->asv_volt[i].asv_freq == target_freq) {
#ifndef CONFIG_ASV_MARGIN_TEST
			return match_asv_info->asv_volt[i].asv_value;
#else
			actual_volt = match_asv_info->asv_volt[i].asv_value;
			break;
#endif
		}
	}

#ifdef CONFIG_ASV_MARGIN_TEST
	if (actual_volt) {
		margin_volt = actual_volt + ((actual_volt * offset_percent)/100);
		if (offset_percent < -5) {
			if (((actual_volt * offset_percent)/100) % MARGIN_UNIT != 0)
				margin_volt -= ((actual_volt * offset_percent)/100) % MARGIN_UNIT;
		} else if (offset_percent < 0) {
			if (((actual_volt * offset_percent)/100) % MARGIN_UNIT != 0)
				margin_volt -= MARGIN_UNIT + ((actual_volt * offset_percent)/100) % MARGIN_UNIT;
		} else if (offset_percent <= 5) {
			if ((actual_volt * offset_percent/100) % MARGIN_UNIT != 0)
				margin_volt += MARGIN_UNIT - ((actual_volt * offset_percent)/100) % MARGIN_UNIT;
		} else {
			if (((actual_volt * offset_percent)/100) % MARGIN_UNIT != 0)
				margin_volt -= ((actual_volt * offset_percent)/100) % MARGIN_UNIT;
		}
		pr_info("%s[%d] offset_percent: %d Freq: %dKHz Actual:Margin volts[%d : %d] ",
					 ID_NAME(target_type), target_type, offset_percent,
					 target_freq, actual_volt, margin_volt);
		return (unsigned int) margin_volt;
	}
#endif

	/* If there is no matched freq, return max supplied voltage */
	return match_asv_info->max_volt_value;
}

unsigned int get_match_abb(enum asv_type_id target_type, unsigned int target_freq)
{
	struct asv_info *match_asv_info = asv_get(target_type);
	unsigned int target_dvfs_level;
	unsigned int i;

	if (!match_asv_info) {
		pr_info("EXYNOS ASV: failed to get_match_abb(type: %d)\n", target_type);
		return 0;
	}

	target_dvfs_level = match_asv_info->dvfs_level_nr;

	if (!match_asv_info->asv_abb) {
		pr_info("EXYNOS ASV: request for nonexist asv type(type: %d)\n", target_type);
		return 0;
	}

	for (i = 0; i < target_dvfs_level; i++)	{
		if (match_asv_info->asv_abb[i].asv_freq == target_freq)
			return match_asv_info->asv_abb[i].asv_value;
	}

	/* If there is no matched freq, return default BB value */
	return ABB_BYPASS;
}

bool is_set_abb_first(enum asv_type_id target_type, unsigned int old_freq, unsigned int target_freq)
{
	int old_abb, target_abb;
	old_abb = get_match_abb(target_type, old_freq);
	target_abb = get_match_abb(target_type, target_freq);

	if (old_abb == ABB_BYPASS && (target_abb == ABB_X070 || target_abb == ABB_X080))
		return true;
	else if ((old_abb == ABB_X120 || old_abb == ABB_X130) && target_abb == ABB_BYPASS)
		return true;
	else if (old_abb == ABB_BYPASS && (target_abb == ABB_X120 || target_abb == ABB_X130))
		return false;
	else
		return true;
}

unsigned int set_match_abb(enum asv_type_id target_type, unsigned int target_abb)
{
	struct asv_info *match_asv_info = asv_get(target_type);

	if (!match_asv_info) {
		pr_info("EXYNOS ASV: failed to set_match_abb(type: %d)\n", target_type);
		return 0;
	}

	if (!match_asv_info->abb_info) {
		pr_info("EXYNOS ASV: request for nonexist abb(type: %d)\n", target_type);
		return 0;
	}

	match_asv_info->abb_info->target_abb = target_abb;
	match_asv_info->abb_info->set_target_abb(match_asv_info);

	return 0;
}

static void set_asv_info(struct asv_common *exynos_asv_common, bool show_volt)
{
	struct asv_info *exynos_asv_info;
	unsigned int match_grp_nr = 0;

	list_for_each_entry(exynos_asv_info, &asv_list, node) {
		match_grp_nr = exynos_asv_info->ops->get_asv_group(exynos_asv_common);
		exynos_asv_info->result_asv_grp = match_grp_nr;
		pr_info("%s ASV group is %d\n", exynos_asv_info->name,
						exynos_asv_info->result_asv_grp);
		exynos_asv_info->ops->set_asv_info(exynos_asv_info, show_volt);
	}
}

static int __init asv_init(void)
{
	struct asv_common *exynos_asv_common;
	int ret = 0;

	exynos_asv_common = kzalloc(sizeof(struct asv_common), GFP_KERNEL);

	if (!exynos_asv_common) {
		pr_err("ASV : Allocation failed\n");
		ret = -EINVAL;
		goto out1;
	}

	/* Define init function for each SoC types */
	if (soc_is_exynos5430()) {
		ret = exynos5430_init_asv(exynos_asv_common);
	} else if (soc_is_exynos5422()) {
		ret = exynos5422_init_asv(exynos_asv_common);
	} else if (soc_is_exynos5433()) {
		ret = exynos5_init_asv(exynos_asv_common);
	} else if (soc_is_exynos7420()) {
		ret = exynos7420_init_asv(exynos_asv_common);
	} else {
		pr_err("ASV : Unknown SoC type\n");
		ret = -EINVAL;
		goto out2;
	}

	if (ret) {
		pr_err("ASV : asv initialize failed\n");
		goto out2;
	}

	/* If it is need to initialize, run init function */
	if (exynos_asv_common->init) {
		if (exynos_asv_common->init()) {
			pr_err("ASV : Can not run init functioin\n");
			ret = -EINVAL;
			goto out2;
		}
	}

	/* Regist ASV member for each SoC */
	if (exynos_asv_common->regist_asv_member) {
		ret = exynos_asv_common->regist_asv_member();
	} else {
		pr_err("ASV : There is no regist_asv_member function\n");
		ret = -EINVAL;
		goto out2;
	}

	set_asv_info(exynos_asv_common, true);

out2:
	kfree(exynos_asv_common);
out1:
	return ret;
}
arch_initcall_sync(asv_init);
