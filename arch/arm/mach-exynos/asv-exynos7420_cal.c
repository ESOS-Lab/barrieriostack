/* linux/arch/arm/mach-exynos/include/mach/asv-exynos7420_cal.c
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS7420 - Adoptive Support Voltage Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <mach/asv-exynos7_cal.h>
#include <mach/asv-exynos7420.h>

static const u32 base_add_arr[SYSC_DVFS_NUM][5] = {
};

static volatile bool use_dynimic_abb[SYSC_DVFS_NUM];

#ifdef CONFIG_SOC_EXYNOS7420
u32 re_err(void)
{
	pr_err("ASV: CAL is working wrong. \n");
	return 0;
}
#endif

bool cal_is_fused_speed_grp(void)
{
	int group_fused = 0;

	return group_fused;
}

u32 cal_get_dram_size(void)
{
	return 0;
}

u32 cal_get_table_ver(void)
{
	return 0;
}

u32 cal_get_ids(void)
{
	return __raw_readl(CHIPID_ASV_INFO + 0x01C0) & 0xff;
}

u32 cal_get_hpm(void)
{
	return (__raw_readl(CHIPID_ASV_INFO + 0x01C4) >> 24) & 0xff ;
}

void cal_init(void)
{
	/* if (cal_get_table_ver() == 0) */
	use_dynimic_abb[SYSC_DVFS_ATL] = false;
	use_dynimic_abb[SYSC_DVFS_APO] = false;
	use_dynimic_abb[SYSC_DVFS_G3D] = false;
	use_dynimic_abb[SYSC_DVFS_INT] = false;
	use_dynimic_abb[SYSC_DVFS_MIF] = false;
	use_dynimic_abb[SYSC_DVFS_CAM] = false;

}

s32 cal_get_min_lv(u32 id)
{
	s32 level = 0;
	switch (id) {
	case SYSC_DVFS_ATL:
		level = SYSC_DVFS_END_LVL_ATL;
		break;
	case SYSC_DVFS_APO:
		level = SYSC_DVFS_END_LVL_APO;
		break;
	case SYSC_DVFS_G3D:
		level = SYSC_DVFS_END_LVL_G3D;
		break;
	case SYSC_DVFS_MIF:
		level = SYSC_DVFS_END_LVL_MIF;
		break;
	case SYSC_DVFS_INT:
		level = SYSC_DVFS_END_LVL_INT;
		break;
	case SYSC_DVFS_CAM:
		level = SYSC_DVFS_END_LVL_CAM;
		break;
	default:
		Assert(0);
	}
	return level;
}

u32 cal_get_ids_hpm_group(void)
{
	u32 ids_group = 0;
	u32 hpm_group = 0;
	u32 i;
	u32 atl_ids = cal_get_ids();
	u32 atl_hpm = cal_get_hpm();

	if (atl_ids >= ids_table_v050[0][MAX_ASV_GROUP-1]) {
		ids_group = MAX_ASV_GROUP - 1;
	} else if (atl_ids <= ids_table_v050[0][0]) {
		ids_group = 0;
	} else {
		for (i = MAX_ASV_GROUP - 1; i > 0; i--) {
			if (atl_ids >= ids_table_v050[0][i]) {
				ids_group = i;
				break;
			}
		}
	}

	if (atl_hpm >= ids_table_v050[1][MAX_ASV_GROUP-1]) {
		hpm_group = MAX_ASV_GROUP - 1;
	} else if (atl_hpm <= ids_table_v050[1][0]) {
		hpm_group = 0;
	} else {
		for (i = MAX_ASV_GROUP - 1; i > 0; i--) {
			if (atl_hpm >= ids_table_v050[1][i]) {
				hpm_group = i;
				break;
			}
		}
	}

	return 0; /* temporary code */

	if (ids_group < hpm_group)
		return ids_group;
	else
		return hpm_group;
}

u32 cal_get_match_subgrp(u32 id, s32 level)
{
	u32 subgrp = 0;	/*  version 0 */

	switch (id) {
	case SYSC_DVFS_ATL:
		subgrp = (level <= SYSC_DVFS_L8) ? 0 :
			(level <= SYSC_DVFS_L13) ? 1 : 2;
		break;
	case SYSC_DVFS_APO:
		subgrp = (level <= SYSC_DVFS_L8) ? 0 :
			(level <= SYSC_DVFS_L13) ? 1 : 2;
		break;
	case SYSC_DVFS_G3D:
		subgrp = (level <= SYSC_DVFS_L2) ? 0 :
			(level <= SYSC_DVFS_L4) ? 1 : 2;
		break;
	case SYSC_DVFS_MIF:
		subgrp = (level <= SYSC_DVFS_L2) ? 0 : /*  L0 1066 L1 933 L2:825 */
			(level <= SYSC_DVFS_L5) ? 1 : 2; /* 633,543,413 // 275~ */
		break;
	case SYSC_DVFS_INT:
		subgrp = (level <= SYSC_DVFS_L0) ? 0 : /*  L0_A, L0 */
			(level <= SYSC_DVFS_L3) ? 1 : 2;
		break;
	case SYSC_DVFS_CAM:
		subgrp = (level <= SYSC_DVFS_L0) ? 0 :
			(level <= SYSC_DVFS_L2) ? 1 : 2;
		break;
	default:
		Assert(0);
	}

	return subgrp;
}


u32 cal_get_lock_volt(u32 id)
{
#if 0
	u32 volt, lockvalue;
	lockvalue = GetBits(base_add_arr[id][0], base_add_arr[id][4], 0xf);

	if (lockvalue == 0)
		volt = 0;
	else if (id == SYSC_DVFS_ATL)
		volt = 85000 + lockvalue * 25000;
	else
		volt = 70000 + lockvalue * 25000;

	return volt;
# endif
	return 0;
}

u32 cal_get_asv_grp(u32 id, s32 level)
{
	u32 subgrp, asv_group = 0;
	if (cal_is_fused_speed_grp())	{
		subgrp = cal_get_match_subgrp(id, level);
		asv_group = GetBits(base_add_arr[id][0], base_add_arr[id][1 + subgrp], 0xf);
	} else {
		asv_group = cal_get_ids_hpm_group();
	}

	if (asv_group > MAX_ASV_GROUP)
		return 0;

	return asv_group;
}

u32 cal_get_volt(u32 id, s32 level)
{
	u32 volt, lock_volt;
	u32 asvgrp;
	u32 minlvl = cal_get_min_lv(id);
	const u32 *p_table;
	u32 idx;

	if (level >= minlvl)
		level = minlvl;

	idx = level;

	p_table = ((id == SYSC_DVFS_ATL) ? volt_table_atl_v050[idx] :
			(id == SYSC_DVFS_APO) ? volt_table_apo_v050[idx] :
			(id == SYSC_DVFS_G3D) ? volt_table_g3d_v050[idx] :
			(id == SYSC_DVFS_MIF) ? volt_table_mif_v050[idx] :
			(id == SYSC_DVFS_INT) ? volt_table_int_v050[idx] :
			(id == SYSC_DVFS_CAM) ? volt_table_cam_v050[idx] :
			NULL);

	if (p_table == NULL) {
		pr_info("%s : voltae table pointer is NULL\n", __func__);
		return 0;
	}

	asvgrp = cal_get_asv_grp(id, level);
	volt = p_table[asvgrp + 1];
	lock_volt = cal_get_lock_volt(id);

	if (lock_volt > volt)
		volt = lock_volt;

	return volt;
}

u32 cal_get_freq(u32 id, s32 level)
{
	u32 freq = 0;
	switch (id) {
	case SYSC_DVFS_ATL:
		freq = volt_table_atl_v050[level][0];
		break;
	case SYSC_DVFS_APO:
		freq = volt_table_apo_v050[level][0];
		break;
	case SYSC_DVFS_G3D:
		freq = volt_table_g3d_v050[level][0];
		break;
	case SYSC_DVFS_MIF:
		freq = volt_table_mif_v050[level][0];
		break;
	case SYSC_DVFS_INT:
		freq = volt_table_int_v050[level][0];
		break;
	case SYSC_DVFS_CAM:
		freq = volt_table_cam_v050[level][0];
		break;
	default:
		freq = 0;
	}
	return freq;
}

bool cal_use_dynimic_abb(u32 id)
{
	return use_dynimic_abb[id];
}

bool cal_use_dynimic_ema(u32 id)
{
	return false;
}

void cal_set_ema(u32 id, u32 setvolt)
{
}

