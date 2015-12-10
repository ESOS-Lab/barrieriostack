/* linux/arch/arm/mach-exynos/asv-exynos7420.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS7420 - ASV(Adoptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#include <linux/of.h>

#include <asm/system_info.h>

#include <mach/asv-exynos.h>
#include <mach/asv-exynos7_cal.h>

#include <mach/map.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>
#include <plat/pm.h>

#define ASV_GROUP_NR		16
#define ARM_MAX_VOLT		(1300000)
#define KFC_MAX_VOLT		(1200000)
#define G3D_MAX_VOLT		(950000)
#define MIF_MAX_VOLT		(1000000)
#define INT_MAX_VOLT		(950000)
#define ISP_MAX_VOLT		(800000)

#ifdef CONFIG_ASV_MARGIN_TEST
static int set_arm_volt;
static int set_kfc_volt;
static int set_int_volt;
static int set_mif_volt;
static int set_g3d_volt;
static int set_isp_volt;

static int __init get_arm_volt(char *str)
{
	get_option(&str, &set_arm_volt);
	return 0;
}
early_param("arm", get_arm_volt);

static int __init get_kfc_volt(char *str)
{
	get_option(&str, &set_kfc_volt);
	return 0;
}
early_param("kfc", get_kfc_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	return 0;
}
early_param("int", get_int_volt);

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_isp_volt(char *str)
{
	get_option(&str, &set_isp_volt);
	return 0;
}
early_param("isp", get_isp_volt);

static int exynos7420_get_margin_test_param(enum asv_type_id target_type)
{
	int add_volt = 0;

	switch (target_type) {
	case ID_ARM:
		add_volt = set_arm_volt;
		break;
	case ID_KFC:
		add_volt = set_kfc_volt;
		break;
	case ID_G3D:
		add_volt = set_g3d_volt;
		break;
	case ID_MIF:
		add_volt = set_mif_volt;
		break;
	case ID_INT:
		add_volt = set_int_volt;
		break;
	case ID_ISP:
		add_volt = set_isp_volt;
		break;
	default:
		return add_volt;
	}
	return add_volt;
}

#endif

static unsigned int exynos7420_get_asv_group(struct asv_common *asv_comm)
{
	int asv_group;
	if (asv_comm->ops_cal->get_asv_gr == NULL)
		return asv_group = 0;

	asv_group = (int) asv_comm->ops_cal->get_asv_gr();
	if (asv_group < 0) {
		pr_err("%s: Faile get ASV group from CAL\n", __func__);
		return 0;
	}
	return asv_group;
}

static void exynos7420_set_asv_info(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	bool useABB;

	if (asv_inform->ops_cal->get_use_abb == NULL || asv_inform->ops_cal->get_abb == NULL)
		useABB = false;
	else
		useABB = asv_inform->ops_cal->get_use_abb(asv_inform->asv_type);

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table)
		* asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	if (useABB) {
		asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table)
			* asv_inform->dvfs_level_nr), GFP_KERNEL);
		if (!asv_inform->asv_abb) {
			pr_err("%s: Memory allocation failed for asv abb\n", __func__);
			kfree(asv_inform->asv_volt);
			return;
		}
	}

	if (asv_inform->ops_cal->get_freq == NULL || asv_inform->ops_cal->get_vol == NULL) {
		pr_err("%s: ASV : Fail get call back function for CAL\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = asv_inform->ops_cal->get_freq(asv_inform->asv_type, i) * 1000;
		if (asv_inform->asv_volt[i].asv_freq == 0)
			continue;
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value = asv_inform->ops_cal->get_vol(asv_inform->asv_type, i);

		if (asv_inform->asv_volt[i].asv_value == 0)
			asv_inform->asv_volt[i].asv_value = asv_inform->max_volt_value;
		else
			asv_inform->asv_volt[i].asv_value += exynos7420_get_margin_test_param(asv_inform->asv_type);
#else
		asv_inform->asv_volt[i].asv_value = asv_inform->ops_cal->get_vol(asv_inform->asv_type, i);

		if (asv_inform->asv_volt[i].asv_value == 0)
			asv_inform->asv_volt[i].asv_value = asv_inform->max_volt_value;
#endif
		if (useABB) {
			asv_inform->asv_abb[i].asv_freq = asv_inform->asv_volt[i].asv_freq;
			asv_inform->asv_abb[i].asv_value = asv_inform->ops_cal->get_abb(asv_inform->asv_type, i);
		}
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			if (useABB)
				pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
						asv_inform->name, i,
						asv_inform->asv_volt[i].asv_freq,
						asv_inform->asv_volt[i].asv_value,
						asv_inform->asv_abb[i].asv_value);
			else
				pr_info("%s LV%d freq : %d volt : %d\n",
						asv_inform->name, i,
						asv_inform->asv_volt[i].asv_freq,
						asv_inform->asv_volt[i].asv_value);
		}
	}
}

static struct asv_ops exynos7420_asv_ops = {
	.get_asv_group  = exynos7420_get_asv_group,
	.set_asv_info   = exynos7420_set_asv_info,
};

static struct asv_ops_cal exynos7420_asv_ops_cal = {
	.get_vol = cal_get_volt,
	.get_freq = cal_get_freq,
	.get_use_abb = cal_use_dynimic_abb,
};

struct asv_info exynos7420_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos7420_asv_ops,
		.ops_cal	= &exynos7420_asv_ops_cal,
		.asv_group_nr	= ASV_GROUP_NR,
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_KFC,
		.name		= "VDD_KFC",
		.ops		= &exynos7420_asv_ops,
		.ops_cal	= &exynos7420_asv_ops_cal,
		.asv_group_nr	= ASV_GROUP_NR,
		.max_volt_value = MAX_VOLT(KFC),
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos7420_asv_ops,
		.ops_cal	= &exynos7420_asv_ops_cal,
		.asv_group_nr	= ASV_GROUP_NR,
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos7420_asv_ops,
		.ops_cal	= &exynos7420_asv_ops_cal,
		.asv_group_nr	= ASV_GROUP_NR,
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos7420_asv_ops,
		.ops_cal	= &exynos7420_asv_ops_cal,
		.asv_group_nr	= ASV_GROUP_NR,
		.max_volt_value = MAX_VOLT(G3D),
	}, {
		.asv_type	= ID_ISP,
		.name		= "VDD_ISP",
		.ops		= &exynos7420_asv_ops,
		.ops_cal	= &exynos7420_asv_ops_cal,
		.asv_group_nr	= ASV_GROUP_NR,
		.max_volt_value = MAX_VOLT(ISP),
	},
};

unsigned int exynos7420_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos7420_asv_member); i++)
		add_asv_member(&exynos7420_asv_member[i]);

	return 0;
}

void exynos7420_set_asv_level_nr(struct asv_common *asv_inform)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(exynos7420_asv_member); i++)
		if (exynos7420_asv_member[i].asv_type == ID_ARM)
			exynos7420_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_ARM);
		else if (exynos7420_asv_member[i].asv_type == ID_KFC)
			exynos7420_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_KFC);
		else if (exynos7420_asv_member[i].asv_type == ID_MIF)
			exynos7420_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_MIF);
		else if (exynos7420_asv_member[i].asv_type == ID_INT)
			exynos7420_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_INT);
		else if (exynos7420_asv_member[i].asv_type == ID_G3D)
			exynos7420_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_G3D);
		else if (exynos7420_asv_member[i].asv_type == ID_ISP)
			exynos7420_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_ISP);
}

static struct asv_common_ops_cal exynos7420_asv_ops_common_cal = {
	.get_min_lv 		= cal_get_min_lv,
	.init			= cal_init,
	.get_table_ver		= cal_get_table_ver,
	.is_fused_sp_gr		= cal_is_fused_speed_grp,
	.get_asv_gr		= cal_get_ids_hpm_group,
	.get_ids		= cal_get_ids,
	.get_hpm		= cal_get_hpm,
};

int exynos7420_init_asv(struct asv_common *asv_info)
{
	asv_info->ops_cal = &exynos7420_asv_ops_common_cal;

	asv_info->ops_cal->init();	/* CAL initiallize */

	if (asv_info->ops_cal->get_ids != NULL)
		pr_info("ASV: EGL IDS: %d \n",
			asv_info->ops_cal->get_ids());
	if (asv_info->ops_cal->get_hpm != NULL)
		pr_info("ASV: EGL HPM: %d \n",
			asv_info->ops_cal->get_hpm());
	if (asv_info->ops_cal->get_table_ver != NULL)
		pr_info("ASV: ASV Table Ver : %d \n",
			asv_info->ops_cal->get_table_ver());

	if (asv_info->ops_cal->is_fused_sp_gr())
		pr_info("ASV: Use Speed Group\n");
	else {
		pr_info("ASV: Use not Speed Group\n");
		if (asv_info->ops_cal->get_asv_gr != NULL)
			pr_info("ASV: ASV_Group : %d \n",
			asv_info->ops_cal->get_asv_gr());
	}

	asv_info->regist_asv_member = exynos7420_regist_asv_member;
	exynos7420_set_asv_level_nr(asv_info);
	return 0;
}
