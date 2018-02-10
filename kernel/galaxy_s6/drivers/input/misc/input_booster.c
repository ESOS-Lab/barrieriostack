/*
 *  Copyright (C) 2013, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "input_booster.h"

static const char * const booster_device_name[BOOSTER_DEVICE_MAX] = {
	"key",
	"touchkey",
	"touch",
	"pen",
};

static unsigned int dbg_level;
static s32 sysfs_level=BOOSTER_LEVEL2;	//default level
static s32 sysfs_head_time, head_cpu_freq, head_kfc_freq, head_mif_freq, head_int_freq, head_hmp_boost;
static s32 sysfs_tail_time, tail_cpu_freq, tail_kfc_freq, tail_mif_freq, tail_int_freq, tail_hmp_boost;

static struct input_booster *g_data;

#ifdef CONFIG_USE_VSYNC_SKIP
void decon_extra_vsync_wait_set(int);
#endif

static void input_booster_sysfs_freqs(struct booster_dvfs *dvfs)
{
//	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	dvfs->level = sysfs_level;
	dvfs->times[BOOSTER_LEVEL2].head_time = sysfs_head_time;
	dvfs->freqs[BOOSTER_LEVEL2].cpu_freq = head_cpu_freq;
	dvfs->freqs[BOOSTER_LEVEL2].kfc_freq = head_kfc_freq;
	dvfs->freqs[BOOSTER_LEVEL2].mif_freq = head_mif_freq;
	dvfs->freqs[BOOSTER_LEVEL2].int_freq = head_int_freq;
	dvfs->freqs[BOOSTER_LEVEL2].hmp_boost = head_hmp_boost;

	dvfs->times[BOOSTER_LEVEL2].tail_time = sysfs_tail_time;
	dvfs->freqs[BOOSTER_LEVEL2_CHG].cpu_freq = tail_cpu_freq;
	dvfs->freqs[BOOSTER_LEVEL2_CHG].kfc_freq = tail_kfc_freq;
	dvfs->freqs[BOOSTER_LEVEL2_CHG].mif_freq = tail_mif_freq;
	dvfs->freqs[BOOSTER_LEVEL2_CHG].int_freq = tail_int_freq;
	dvfs->freqs[BOOSTER_LEVEL2_CHG].hmp_boost = tail_hmp_boost;
}

#ifdef USE_HMP_BOOST
static void input_booster_set_hmp_boost(struct input_booster *data, enum booster_device_type device_type, bool enable)
{
	int retval = 0;
	struct booster_dvfs *dvfs = NULL;

	if (device_type >= BOOSTER_DEVICE_MAX) {
		dev_err(data->dev, "%s : Undefined Device type[%d].\n", __func__, device_type);
		return;
	}

	dvfs = data->dvfses[device_type];

	if (!dvfs || !dvfs->initialized) {
		dev_err(data->dev, "%s : Device %s is not initialized.\n",
			__func__, booster_device_name[device_type]);
		return;
	}

	if (enable == dvfs->hmp_boosted) {
		DVFS_DEV_DBG(DBG_MISC, data->dev, "%s : HMP already %s for %s.\n",
			__func__, enable ? "Enabled" : "Disabled", booster_device_name[device_type]);
		return;
	}

	retval = set_hmp_boost(enable);
	if (retval < 0) {
		dev_err(data->dev, "%s : Fail to %s HMP[%d] for %s.\n",
			__func__, enable ? "Enabled" : "Disabled", retval, booster_device_name[device_type]);
		return;
	}

	dvfs->hmp_boosted = enable;
	DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : HMP %s for %s.\n",
		__func__, enable ? "Enabled" : "Disabled",booster_device_name[device_type]);
}
#endif

/* Key */
DECLARE_DVFS_DELAYED_WORK_FUNC(CHG, KEY)
{
	struct booster_dvfs *dvfs =
		container_of(work, struct booster_dvfs, dvfs_chg_work.work);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);

	remove_qos(&dvfs->cpu_qos);
	SET_HMP(data, BOOSTER_DEVICE_KEY, false);

	dvfs->lock_status = false;
	DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS OFF\n", __func__);

	mutex_unlock(&dvfs->lock);
}

DECLARE_DVFS_DELAYED_WORK_FUNC(OFF, KEY)
{
	struct booster_dvfs *dvfs =
		container_of(work, struct booster_dvfs, dvfs_off_work.work);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);

	remove_qos(&dvfs->cpu_qos);
	SET_HMP(data, BOOSTER_DEVICE_KEY, false);

	dvfs->lock_status = false;
	DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS OFF\n", __func__);

	mutex_unlock(&dvfs->lock);
}

DECLARE_DVFS_WORK_FUNC(SET, KEY)
{
	struct input_booster *data = (struct input_booster *)booster_data;
	struct booster_dvfs *dvfs = data->dvfses[BOOSTER_DEVICE_KEY];

	if (!dvfs || !dvfs->initialized) {
		dev_err(data->dev, "%s: Dvfs is not initialized\n",	__func__);
		return;
	}

	mutex_lock(&dvfs->lock);

	if ((!dvfs->level)&&(!dvfs->lock_status)) {
		dev_err(data->dev, "%s : Skip to set booster due to level 0\n", __func__);
		goto out;
	}

	switch (booster_mode) {
	case BOOSTER_MODE_ON:
		cancel_delayed_work(&dvfs->dvfs_off_work);
		cancel_delayed_work(&dvfs->dvfs_chg_work);

		SET_HMP(data, BOOSTER_DEVICE_KEY, dvfs->freqs[BOOSTER_LEVEL1].hmp_boost);
		set_qos(&dvfs->cpu_qos, PM_QOS_CLUSTER1_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL1].cpu_freq);

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS ON [level %d->1][start time [%d]]\n",
			__func__, dvfs->level, dvfs->times[BOOSTER_LEVEL1].head_time);

		schedule_delayed_work(&dvfs->dvfs_chg_work,
			msecs_to_jiffies(dvfs->times[BOOSTER_LEVEL1].head_time));
		dvfs->lock_status = true;
	break;
	case BOOSTER_MODE_OFF:
		if (dvfs->lock_status)
			schedule_delayed_work(&dvfs->dvfs_off_work,
							msecs_to_jiffies(dvfs->times[BOOSTER_LEVEL1].tail_time));
	break;
	case BOOSTER_MODE_FORCE_OFF:
		if (dvfs->lock_status) {
			cancel_delayed_work(&dvfs->dvfs_chg_work);
			cancel_delayed_work(&dvfs->dvfs_off_work);
			schedule_work(&dvfs->dvfs_off_work.work);
		}
	break;
	default:
	break;
	}

out:
	mutex_unlock(&dvfs->lock);
	return;
}

/* Touchkey */
DECLARE_DVFS_DELAYED_WORK_FUNC(CHG, TOUCHKEY)
{
	struct booster_dvfs *dvfs =
		container_of(work, struct booster_dvfs, dvfs_chg_work.work);
	//struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);

	dvfs->lock_status = true;
	dvfs->short_press = false;

	mutex_unlock(&dvfs->lock);
}

DECLARE_DVFS_DELAYED_WORK_FUNC(OFF, TOUCHKEY)
{
	struct booster_dvfs *dvfs =
		container_of(work, struct booster_dvfs, dvfs_off_work.work);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);

	remove_qos(&dvfs->cpu_qos);
	SET_HMP(data, BOOSTER_DEVICE_KEY, false);

	dvfs->lock_status = false;
	dvfs->short_press = false;

	DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS OFF\n", __func__);

	mutex_unlock(&dvfs->lock);
}

DECLARE_DVFS_WORK_FUNC(SET, TOUCHKEY)
{
	struct input_booster *data = (struct input_booster *)booster_data;
	struct booster_dvfs *dvfs = data->dvfses[BOOSTER_DEVICE_TOUCHKEY];

	if (!dvfs || !dvfs->initialized) {
		dev_err(data->dev, "%s: Dvfs is not initialized\n",	__func__);
		return;
	}

	mutex_lock(&dvfs->lock);

	if (!dvfs->level) {
		dev_err(data->dev, "%s : Skip to set booster due to level 0\n", __func__);
		goto out;
	}

	switch (booster_mode) {
	case BOOSTER_MODE_ON:
		cancel_delayed_work(&dvfs->dvfs_off_work);
		if (!dvfs->lock_status && !dvfs->short_press) {
			schedule_delayed_work(&dvfs->dvfs_chg_work,
				msecs_to_jiffies(dvfs->times[BOOSTER_LEVEL1].head_time));
			dvfs->short_press = true;
			DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS init \n", __func__);
		}
	break;
	case BOOSTER_MODE_OFF:
		dvfs->lock_status = true;
		SET_HMP(data, BOOSTER_DEVICE_KEY, dvfs->freqs[BOOSTER_LEVEL1].hmp_boost);
		set_qos(&dvfs->cpu_qos, PM_QOS_CLUSTER1_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL1].cpu_freq);
		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS ON [level %d->1][start time [%d]]\n",
			__func__, dvfs->level, dvfs->times[BOOSTER_LEVEL1].tail_time);

		if (dvfs->short_press) {
			cancel_delayed_work(&dvfs->dvfs_chg_work);
			schedule_work(&dvfs->dvfs_chg_work.work);
			schedule_delayed_work(&dvfs->dvfs_off_work,
							msecs_to_jiffies(dvfs->times[BOOSTER_LEVEL1].tail_time));
			goto out;
		}
		if (dvfs->lock_status)
			schedule_delayed_work(&dvfs->dvfs_off_work,
							msecs_to_jiffies(dvfs->times[BOOSTER_LEVEL1].tail_time));
	break;
	case BOOSTER_MODE_FORCE_OFF:
		if (dvfs->lock_status) {
			cancel_delayed_work(&dvfs->dvfs_chg_work);
			cancel_delayed_work(&dvfs->dvfs_off_work);
			schedule_work(&dvfs->dvfs_off_work.work);
		}
	break;
	default:
	break;
	}

out:
	mutex_unlock(&dvfs->lock);
	return;
}

/* Touch */
DECLARE_DVFS_DELAYED_WORK_FUNC(CHG, TOUCH)
{
	struct booster_dvfs *dvfs =
		container_of(work, struct booster_dvfs, dvfs_chg_work.work);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);

	if (!dvfs->times[dvfs->level].phase_time)
		dvfs->phase_excuted = false;

	switch (dvfs->level) {
	case BOOSTER_LEVEL0:
	case BOOSTER_LEVEL1:
		remove_qos(&dvfs->cpu_qos);
		remove_qos(&dvfs->kfc_qos);
		remove_qos(&dvfs->mif_qos);
		remove_qos(&dvfs->int_qos);
		SET_HMP(data, BOOSTER_DEVICE_TOUCH, false);
		dvfs->lock_status = false;

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS OFF\n", __func__);
	break;
	case BOOSTER_LEVEL2:
		SET_HMP(data, BOOSTER_DEVICE_TOUCH, dvfs->freqs[BOOSTER_LEVEL2_CHG].hmp_boost);
		if(dvfs->freqs[BOOSTER_LEVEL2_CHG].kfc_freq==0){
			remove_qos(&dvfs->kfc_qos);}
		else{
			set_qos(&dvfs->kfc_qos, PM_QOS_CLUSTER0_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2_CHG].kfc_freq);}
		if(dvfs->freqs[BOOSTER_LEVEL2_CHG].int_freq==0){
			remove_qos(&dvfs->int_qos);}
		else{
			set_qos(&dvfs->int_qos, PM_QOS_DEVICE_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2_CHG].int_freq);}
		if(dvfs->freqs[BOOSTER_LEVEL2_CHG].cpu_freq==0){
			remove_qos(&dvfs->cpu_qos);}
		else{
			set_qos(&dvfs->cpu_qos, PM_QOS_CLUSTER1_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2_CHG].cpu_freq);}
		if(dvfs->freqs[BOOSTER_LEVEL2_CHG].mif_freq==0){
			remove_qos(&dvfs->mif_qos);}
		else{
			set_qos(&dvfs->mif_qos, PM_QOS_BUS_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2_CHG].mif_freq);}

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS CHANGED [level %d]\n", __func__, dvfs->level);
	break;
	default:
		dev_err(data->dev, "%s : Undefined type passed[%d]\n", __func__, dvfs->level);
	break;
	}

	if (!dvfs->lock_status)
		dvfs->phase_excuted = false;

	mutex_unlock(&dvfs->lock);
}

DECLARE_DVFS_DELAYED_WORK_FUNC(OFF, TOUCH)
{
	struct booster_dvfs *dvfs =
		container_of(work, struct booster_dvfs, dvfs_off_work.work);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);
	if (dvfs->lock_status) {
		remove_qos(&dvfs->cpu_qos);
		remove_qos(&dvfs->kfc_qos);
		remove_qos(&dvfs->mif_qos);
		remove_qos(&dvfs->int_qos);
		SET_HMP(data, BOOSTER_DEVICE_TOUCH, false);
		dvfs->lock_status = false;
		dvfs->phase_excuted = false;

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS OFF\n", __func__);
	}
	mutex_unlock(&dvfs->lock);
}

DECLARE_DVFS_WORK_FUNC(SET, TOUCH)
{
	struct input_booster *data = (struct input_booster *)booster_data;
	struct booster_dvfs *dvfs = data->dvfses[BOOSTER_DEVICE_TOUCH];

	if (!dvfs || !dvfs->initialized) {
		dev_err(data->dev, "%s: Dvfs is not initialized\n",	__func__);
		return;
	}

	mutex_lock(&dvfs->lock);

	input_booster_sysfs_freqs(dvfs);

	if ((!dvfs->level)&&(!dvfs->lock_status)) {
		dev_err(data->dev, "%s : Skip to set booster due to level 0\n", __func__);
		goto out;
	}

	switch (booster_mode) {
	case BOOSTER_MODE_ON:
#ifdef CONFIG_USE_VSYNC_SKIP
		decon_extra_vsync_wait_set(ERANGE);
#endif /* CONFIG_USE_VSYNC_SKIP */
		cancel_delayed_work(&dvfs->dvfs_off_work);
		cancel_delayed_work(&dvfs->dvfs_chg_work);
		switch (dvfs->level) {
		case BOOSTER_LEVEL1:
			SET_HMP(data, BOOSTER_DEVICE_TOUCH, dvfs->freqs[BOOSTER_LEVEL2].hmp_boost);
			set_qos(&dvfs->kfc_qos, PM_QOS_CLUSTER0_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2].kfc_freq);
			set_qos(&dvfs->int_qos, PM_QOS_DEVICE_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2].int_freq);
			set_qos(&dvfs->cpu_qos, PM_QOS_CLUSTER1_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2].cpu_freq);
			set_qos(&dvfs->mif_qos, PM_QOS_BUS_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2].mif_freq);
		break;
		case BOOSTER_LEVEL2:
			SET_HMP(data, BOOSTER_DEVICE_TOUCH, dvfs->freqs[BOOSTER_LEVEL2].hmp_boost);
			if(dvfs->freqs[BOOSTER_LEVEL2].kfc_freq==0){
				remove_qos(&dvfs->kfc_qos);}
			else{
				set_qos(&dvfs->kfc_qos, PM_QOS_CLUSTER0_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2].kfc_freq);}
			if(dvfs->freqs[BOOSTER_LEVEL2].int_freq==0){
				remove_qos(&dvfs->int_qos);}
			else{
				set_qos(&dvfs->int_qos, PM_QOS_DEVICE_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2].int_freq);}
			if(dvfs->freqs[BOOSTER_LEVEL2].cpu_freq==0){
				remove_qos(&dvfs->cpu_qos);}
			else{
				set_qos(&dvfs->cpu_qos, PM_QOS_CLUSTER1_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2].cpu_freq);}
			if(dvfs->freqs[BOOSTER_LEVEL2].mif_freq==0){
				remove_qos(&dvfs->mif_qos);}
			else{
				set_qos(&dvfs->mif_qos, PM_QOS_BUS_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2].mif_freq);}
		break;
		default:
			dev_err(data->dev, "%s : Undefined type passed[%d]\n", __func__, dvfs->level);
		break;
		}

		if (dvfs->times[dvfs->level].phase_time) {
			schedule_delayed_work(&dvfs->dvfs_chg_work,
								msecs_to_jiffies(dvfs->times[dvfs->level].phase_time));
			if (dvfs->phase_excuted)
				dvfs->phase_excuted = false;
		} else {
			schedule_delayed_work(&dvfs->dvfs_chg_work,
								msecs_to_jiffies(dvfs->times[dvfs->level].head_time));
		}
		dvfs->lock_status = true;

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS ON [level %d][start time [%d]]\n",
			__func__, dvfs->level, dvfs->times[dvfs->level].phase_time ?: dvfs->times[dvfs->level].head_time);

	break;
	case BOOSTER_MODE_OFF:
		if (dvfs->lock_status)
			schedule_delayed_work(&dvfs->dvfs_off_work,
						msecs_to_jiffies(dvfs->times[dvfs->level].tail_time));
	break;
	case BOOSTER_MODE_FORCE_OFF:
		if (dvfs->lock_status) {
			cancel_delayed_work(&dvfs->dvfs_chg_work);
			cancel_delayed_work(&dvfs->dvfs_off_work);
			schedule_work(&dvfs->dvfs_off_work.work);
		}
	break;
	default:
	break;
	}

out:
	mutex_unlock(&dvfs->lock);
	return;
}

/* Pen */
DECLARE_DVFS_DELAYED_WORK_FUNC(CHG, PEN)
{
	struct booster_dvfs *dvfs =
		container_of(work, struct booster_dvfs, dvfs_chg_work.work);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);

	if (!dvfs->times[dvfs->level].phase_time)
		dvfs->phase_excuted = false;

	switch (dvfs->level) {
	case BOOSTER_LEVEL0:
	case BOOSTER_LEVEL1:
		remove_qos(&dvfs->cpu_qos);
		remove_qos(&dvfs->kfc_qos);
		remove_qos(&dvfs->mif_qos);
		remove_qos(&dvfs->int_qos);
		SET_HMP(data, BOOSTER_DEVICE_PEN, false);
		dvfs->lock_status = false;

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS OFF\n", __func__);
		break;
	case BOOSTER_LEVEL2:
		SET_HMP(data, BOOSTER_DEVICE_PEN, dvfs->freqs[BOOSTER_LEVEL2_CHG].hmp_boost);
		if(dvfs->freqs[BOOSTER_LEVEL2_CHG].kfc_freq==0){
			remove_qos(&dvfs->kfc_qos);}
		else{
			set_qos(&dvfs->kfc_qos, PM_QOS_CLUSTER0_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2_CHG].kfc_freq);}
		if(dvfs->freqs[BOOSTER_LEVEL2_CHG].int_freq==0){
			remove_qos(&dvfs->int_qos);}
		else{
			set_qos(&dvfs->int_qos, PM_QOS_DEVICE_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2_CHG].int_freq);}
		if(dvfs->freqs[BOOSTER_LEVEL2_CHG].cpu_freq==0){
			remove_qos(&dvfs->cpu_qos);}
		else{
			set_qos(&dvfs->cpu_qos, PM_QOS_CLUSTER1_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2_CHG].cpu_freq);}
		if(dvfs->freqs[BOOSTER_LEVEL2_CHG].mif_freq==0){
			remove_qos(&dvfs->mif_qos);}
		else{
			set_qos(&dvfs->mif_qos, PM_QOS_BUS_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2_CHG].mif_freq);}

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS CHANGED [level %d]\n", __func__, dvfs->level);
		break;
	default:
		dev_err(data->dev, "%s : Undefined type passed[%d]\n", __func__, dvfs->level);
		break;
	}

	if (!dvfs->lock_status)
		dvfs->phase_excuted = false;

	mutex_unlock(&dvfs->lock);
}

DECLARE_DVFS_DELAYED_WORK_FUNC(OFF, PEN)
{
	struct booster_dvfs *dvfs =
		container_of(work, struct booster_dvfs, dvfs_off_work.work);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);
	if (dvfs->lock_status) {
		remove_qos(&dvfs->cpu_qos);
		remove_qos(&dvfs->kfc_qos);
		remove_qos(&dvfs->mif_qos);
		remove_qos(&dvfs->int_qos);
		SET_HMP(data, BOOSTER_DEVICE_PEN, false);
		dvfs->lock_status = false;
		dvfs->phase_excuted = false;

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS OFF\n", __func__);
	}
	mutex_unlock(&dvfs->lock);
}

DECLARE_DVFS_WORK_FUNC(SET, PEN)
{
	struct input_booster *data = (struct input_booster *)booster_data;
	struct booster_dvfs *dvfs = data->dvfses[BOOSTER_DEVICE_PEN];

	if (!dvfs || !dvfs->initialized) {
		dev_err(data->dev, "%s: Dvfs is not initialized\n",	__func__);
		return;
	}

	mutex_lock(&dvfs->lock);

	input_booster_sysfs_freqs(dvfs);

	if ((!dvfs->level)&&(!dvfs->lock_status)) {
		dev_err(data->dev, "%s : Skip to set booster due to level 0\n", __func__);
		goto out;
	}

	switch (booster_mode) {
	case BOOSTER_MODE_ON:
#ifdef CONFIG_USE_VSYNC_SKIP
		decon_extra_vsync_wait_set(ERANGE);
#endif /* CONFIG_USE_VSYNC_SKIP */
		cancel_delayed_work(&dvfs->dvfs_off_work);
		cancel_delayed_work(&dvfs->dvfs_chg_work);
		switch (dvfs->level) {
		case BOOSTER_LEVEL1:
			SET_HMP(data, BOOSTER_DEVICE_PEN, dvfs->freqs[BOOSTER_LEVEL2].hmp_boost);
			set_qos(&dvfs->kfc_qos, PM_QOS_CLUSTER0_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2].kfc_freq);
			set_qos(&dvfs->int_qos, PM_QOS_DEVICE_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2].int_freq);
			set_qos(&dvfs->cpu_qos, PM_QOS_CLUSTER1_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2].cpu_freq);
			set_qos(&dvfs->mif_qos, PM_QOS_BUS_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2].mif_freq);
			break;
		case BOOSTER_LEVEL2:
			SET_HMP(data, BOOSTER_DEVICE_PEN, dvfs->freqs[BOOSTER_LEVEL2].hmp_boost);
			if(dvfs->freqs[BOOSTER_LEVEL2].kfc_freq==0){
				remove_qos(&dvfs->kfc_qos);}
			else{
				set_qos(&dvfs->kfc_qos, PM_QOS_CLUSTER0_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2].kfc_freq);}
			if(dvfs->freqs[BOOSTER_LEVEL2].int_freq==0){
				remove_qos(&dvfs->int_qos);}
			else{
				set_qos(&dvfs->int_qos, PM_QOS_DEVICE_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2].int_freq);}
			if(dvfs->freqs[BOOSTER_LEVEL2].cpu_freq==0){
				remove_qos(&dvfs->cpu_qos);}
			else{
				set_qos(&dvfs->cpu_qos, PM_QOS_CLUSTER1_FREQ_MIN, dvfs->freqs[BOOSTER_LEVEL2].cpu_freq);}
			if(dvfs->freqs[BOOSTER_LEVEL2].mif_freq==0){
				remove_qos(&dvfs->mif_qos);}
			else{
				set_qos(&dvfs->mif_qos, PM_QOS_BUS_THROUGHPUT, dvfs->freqs[BOOSTER_LEVEL2].mif_freq);}
			break;
		default:
			dev_err(data->dev, "%s : Undefined type passed[%d]\n", __func__, dvfs->level);
			break;
		}

		if (dvfs->times[dvfs->level].phase_time) {
			schedule_delayed_work(&dvfs->dvfs_chg_work,
				msecs_to_jiffies(dvfs->times[dvfs->level].phase_time));
			if (dvfs->phase_excuted)
				dvfs->phase_excuted = false;
		} else {
			schedule_delayed_work(&dvfs->dvfs_chg_work,
				msecs_to_jiffies(dvfs->times[dvfs->level].head_time));
		}
		dvfs->lock_status = true;

		DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s : DVFS ON [level %d][start time [%d]]\n",
			__func__, dvfs->level, dvfs->times[dvfs->level].phase_time ?: dvfs->times[dvfs->level].head_time);

		break;
	case BOOSTER_MODE_OFF:
		if (dvfs->lock_status)
			schedule_delayed_work(&dvfs->dvfs_off_work,
			msecs_to_jiffies(dvfs->times[dvfs->level].tail_time));
		break;
	case BOOSTER_MODE_FORCE_OFF:
		if (dvfs->lock_status) {
			cancel_delayed_work(&dvfs->dvfs_chg_work);
			cancel_delayed_work(&dvfs->dvfs_off_work);
			schedule_work(&dvfs->dvfs_off_work.work);
		}
		break;
	default:
		break;
	}

out:
	mutex_unlock(&dvfs->lock);
	return;
}

static void input_booster_event_work(struct work_struct *work)
{
	struct input_booster *data =
		container_of(work, struct input_booster, event_work);
	struct booster_event event;
	int i;

	for (i = 0; data->front != data->rear; i++) {
		spin_lock(&data->buffer_lock);
		event = data->buffer[data->front];
		data->front = (data->front + 1) % data->buffer_size;
		spin_unlock(&data->buffer_lock);

		DVFS_DEV_DBG(DBG_EVENT, data->dev, "%s :[%d] Device type[%s] mode[%d]\n", __func__,
			i, booster_device_name[event.device_type], event.mode);

		switch (event.device_type) {
		case BOOSTER_DEVICE_KEY:
			DVFS_WORK_FUNC(SET, KEY)(data, event.mode);
		break;
		case BOOSTER_DEVICE_TOUCHKEY:
			DVFS_WORK_FUNC(SET, TOUCHKEY)(data, event.mode);
		break;
		case BOOSTER_DEVICE_TOUCH:
			DVFS_WORK_FUNC(SET, TOUCH)(data, event.mode);
		break;
		case BOOSTER_DEVICE_PEN:
			DVFS_WORK_FUNC(SET, PEN)(data, event.mode);
		break;
		default:
		break;
		}
	}
}

void input_booster_send_event(unsigned int type, int value)
{
	struct booster_event event;

	if (!g_data) {
		pr_err("%s: booster is not loaded\n", __func__);
		return;
	}

	if (!test_bit(type, g_data->dev_bit))
		return;

	event.device_type = type;
	if (event.device_type >= BOOSTER_DEVICE_MAX)
		return;

	event.mode = value;

	spin_lock(&g_data->buffer_lock);
	g_data->buffer[g_data->rear] = event;
	g_data->rear = (g_data->rear + 1) % g_data->buffer_size;
	spin_unlock(&g_data->buffer_lock);

	/* call the workqueue */
	schedule_work(&g_data->event_work);
}
EXPORT_SYMBOL(input_booster_send_event);

/* Now we support sysfs file to change booster level under input_booster class.
 * But previous sysfs is scatterd into each driver, so we need to support it also.
 * Enable below define. if you want to use previous sysfs files.
 */
void change_booster_level_for_tsp(unsigned char level)
{
	struct booster_dvfs *dvfs = NULL;
	struct input_booster *data = NULL;

	if (!g_data) {
		pr_err("%s: booster is not loaded\n", __func__);
		return;
	}

	dvfs = g_data->dvfses[BOOSTER_DEVICE_TOUCH];
	if (!dvfs || !dvfs->initialized) {
		DVFS_DEV_DBG(DBG_DVFS, g_data->dev, "%s: Dvfs is not initialized\n",
			__func__);
		return;
	}

	data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);

	dvfs->level = level;

	DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s: %s's LEVEL [%d]\n",
			__func__, dvfs->name, dvfs->level);
	mutex_unlock(&dvfs->lock);
}

void change_booster_level_for_pen(unsigned char level)
{
	struct booster_dvfs *dvfs = NULL;
	struct input_booster *data = NULL;

	if (!g_data) {
		pr_err("%s: booster is not loaded\n", __func__);
		return;
	}

	dvfs = g_data->dvfses[BOOSTER_DEVICE_PEN];
	if (!dvfs || !dvfs->initialized) {
		DVFS_DEV_DBG(DBG_DVFS, g_data->dev, "%s: Dvfs is not initialized\n",
			__func__);
		return;
	}

	data = dev_get_drvdata(dvfs->parent_dev);

	mutex_lock(&dvfs->lock);

	dvfs->level = level;

	DVFS_DEV_DBG(DBG_DVFS, data->dev, "%s: %s's LEVEL [%d]\n",
		__func__, dvfs->name, dvfs->level);
	mutex_unlock(&dvfs->lock);
}

static void	input_booster_lookup_freqs(struct booster_dvfs *dvfs)
{
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);
	int i;

	dev_info(data->dev, "#############################################################\n");
	for (i = 0; i < BOOSTER_LEVEL_MAX; i++) {
		if (!dvfs->freqs[i].cpu_freq
			&& !dvfs->freqs[i].kfc_freq
			&& !dvfs->freqs[i].mif_freq
			&& !dvfs->freqs[i].int_freq
			&& !dvfs->freqs[i].hmp_boost)
			continue;

		dev_info(data->dev, "%s: %s's LEVEL%d cpu %d kfc %d mif %d int %d hmp %d\n",
			__func__, dvfs->name, i, dvfs->freqs[i].cpu_freq,
			dvfs->freqs[i].kfc_freq, dvfs->freqs[i].mif_freq, dvfs->freqs[i].int_freq, dvfs->freqs[i].hmp_boost);
	}
	dev_info(data->dev, "#############################################################\n");
}

static void	input_booster_lookup_times(struct booster_dvfs *dvfs)
{
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);
	int i;

	dev_info(data->dev, "#############################################################\n");
	for (i = 0; i < BOOSTER_LEVEL_MAX; i++) {
		if (!dvfs->times[i].head_time
			&& !dvfs->times[i].tail_time
			&& !dvfs->times[i].phase_time)
			continue;

		dev_info(data->dev, "%s: %s's LEVEL%d head %d tail %d phase %d\n",
			__func__, dvfs->name, i, dvfs->times[i].head_time,
			dvfs->times[i].tail_time, dvfs->times[i].phase_time);
	}
	dev_info(data->dev, "#############################################################\n");
}

#ifdef BOOSTER_SYSFS
static ssize_t input_booster_get_debug_level(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", dbg_level);
}

static ssize_t input_booster_set_debug_level(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val) < 0)
		return -EINVAL;

	dbg_level = val;

	return count;
}

static ssize_t input_booster_get_head(struct class *dev,
		struct class_attribute *attr, char *buf)
{

	return sprintf(buf, "%d %d %d %d %d %d \n",\
		sysfs_head_time, head_cpu_freq, head_kfc_freq, head_mif_freq, head_int_freq, head_hmp_boost);
}

static ssize_t input_booster_set_head(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long cpu_freq, kfc_freq, mif_freq, int_freq, hmp_boost;
	unsigned long head_time;

	if (sscanf(buf, "%lu%lu%lu%lu%lu%lu",
		&head_time,&cpu_freq, &kfc_freq, &mif_freq, &int_freq, &hmp_boost) != 6) {
		printk("### Keep this format : [head_time cpu_freq kfc_freq mif_freq int_freq hmp_boost] (Ex: 1600000 0 1500000 667000 333000 1###\n");
		goto out;
	}

	sysfs_head_time = head_time;
	head_cpu_freq = cpu_freq;
	head_kfc_freq = kfc_freq;
	head_mif_freq = mif_freq;
	head_int_freq = int_freq;
	head_hmp_boost = hmp_boost;

out:
	return count;
}

static ssize_t input_booster_get_tail(struct class *dev,
		struct class_attribute *attr, char *buf)
{

	return sprintf(buf, "%d %d %d %d %d %d \n",\
		sysfs_tail_time, tail_cpu_freq, tail_kfc_freq, tail_mif_freq, tail_int_freq, tail_hmp_boost);
}

static ssize_t input_booster_set_tail(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long cpu_freq, kfc_freq, mif_freq, int_freq, hmp_boost;
	unsigned long tail_time;

	if (sscanf(buf, "%lu%lu%lu%lu%lu%lu",
		&tail_time,&cpu_freq, &kfc_freq, &mif_freq, &int_freq, &hmp_boost) != 6) {
		printk("### Keep this format : [tail_time cpu_freq kfc_freq mif_freq int_freq hmp_boost] (Ex: 1600000 0 1500000 667000 333000 1###\n");
		goto out;
	}
	sysfs_tail_time = tail_time;
	tail_cpu_freq = cpu_freq;
	tail_kfc_freq = kfc_freq;
	tail_mif_freq = mif_freq;
	tail_int_freq = int_freq;
	tail_hmp_boost = hmp_boost;

out:
	return count;
}

static ssize_t input_booster_get_level(struct class *dev,
		struct class_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", sysfs_level);
}

static ssize_t input_booster_set_level(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val, type;

	if (sscanf(buf,"%d",&val) != 1) {
		return count;
	}

	if(val<0){
		for (type = 0; type < g_data->ndevices; type++) {
			input_booster_send_event(type, BOOSTER_MODE_FORCE_OFF);
			printk( "%s: type = %s BOOSTER_MODE_FORCE_OFF\n",__func__, booster_device_name[type]);
		}
	}
	else
		sysfs_level = val;

	return count;
}


static CLASS_ATTR(debug_level, S_IRUGO | S_IWUSR, input_booster_get_debug_level, input_booster_set_debug_level);
static CLASS_ATTR(head, S_IRUGO | S_IWUSR, input_booster_get_head, input_booster_set_head);
static CLASS_ATTR(tail, S_IRUGO | S_IWUSR, input_booster_get_tail, input_booster_set_tail);
static CLASS_ATTR(level, S_IRUGO | S_IWUSR, input_booster_get_level, input_booster_set_level);

static ssize_t input_booster_get_dvfs_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct booster_dvfs *dvfs = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", dvfs->level);
}

static ssize_t input_booster_set_dvfs_level(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct booster_dvfs *dvfs = dev_get_drvdata(dev);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);
	unsigned int val;

	if (kstrtouint(buf, 0, &val) < 0)
		return -EINVAL;

	dvfs->level = val;

	dev_info(data->dev, "%s: %s's LEVEL [%d]\n",
			__func__, dvfs->name, dvfs->level);

	return count;
}

static ssize_t input_booster_get_dvfs_freq(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct booster_dvfs *dvfs = dev_get_drvdata(dev);

	input_booster_lookup_freqs(dvfs);

	return 0;
}

static ssize_t input_booster_set_dvfs_freq(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct booster_dvfs *dvfs = dev_get_drvdata(dev);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);
	int level;
	unsigned long cpu_freq, kfc_freq, mif_freq, int_freq, hmp_boost;

	if (sscanf(buf, "%d%lu%lu%lu%lu%lu",
		&level, &cpu_freq, &kfc_freq, &mif_freq, &int_freq, &hmp_boost) != 7) {
		dev_err(data->dev, "### Keep this format : [level cpu_freq kfc_freq mif_freq int_freq hmp_boost] (Ex: 1 1600000 0 1500000 667000 333000 1###\n");
		goto out;
	}

	if (level >= BOOSTER_LEVEL_MAX) {
		dev_err(data->dev, "%s : Entered level is not permitted\n", __func__);
		goto out;
	}

	dvfs->freqs[level].cpu_freq = cpu_freq;
	dvfs->freqs[level].kfc_freq = kfc_freq;
	dvfs->freqs[level].mif_freq = mif_freq;
	dvfs->freqs[level].int_freq = int_freq;
	dvfs->freqs[level].hmp_boost = hmp_boost;

	input_booster_lookup_freqs(dvfs);

out:
	return count;
}

static ssize_t input_booster_get_dvfs_time(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct booster_dvfs *dvfs = dev_get_drvdata(dev);

	input_booster_lookup_times(dvfs);

	return 0;
}

static ssize_t input_booster_set_dvfs_time(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct booster_dvfs *dvfs = dev_get_drvdata(dev);
	struct input_booster *data = dev_get_drvdata(dvfs->parent_dev);
	int level;
	unsigned long head_time, tail_time, phase_time;

	if (sscanf(buf, "%d%lu%lu%lu",
		&level, &head_time, &tail_time, &phase_time) != 4) {
		dev_err(data->dev, "### Keep this format : [level head_time tail_time phase_time] (Ex: 1 130 500 50 ###\n");
		goto out;
	}

	if (level >= BOOSTER_LEVEL_MAX) {
		dev_err(data->dev, "%s : Entered level is not permitted\n", __func__);
		goto out;
	}

	dvfs->times[level].head_time = head_time;
	dvfs->times[level].tail_time = tail_time;
	dvfs->times[level].phase_time = phase_time;

	input_booster_lookup_times(dvfs);

out:
	return count;
}

static DEVICE_ATTR(level, S_IRUGO | S_IWUSR, input_booster_get_dvfs_level, input_booster_set_dvfs_level);
static DEVICE_ATTR(freq, S_IRUGO | S_IWUSR, input_booster_get_dvfs_freq, input_booster_set_dvfs_freq);
static DEVICE_ATTR(time, S_IRUGO | S_IWUSR, input_booster_get_dvfs_time, input_booster_set_dvfs_time);

static struct attribute *dvfs_attributes[] = {
	&dev_attr_level.attr,
	&dev_attr_freq.attr,
	&dev_attr_time.attr,
	NULL,
};

static struct attribute_group dvfs_attr_group = {
	.attrs = dvfs_attributes,
};

static void input_booster_free_sysfs(struct input_booster *data)
{
	class_remove_file(data->class, &class_attr_debug_level);
	class_remove_file(data->class, &class_attr_head);
	class_remove_file(data->class, &class_attr_tail);
	class_remove_file(data->class, &class_attr_level);
	class_destroy(data->class);
}

static int input_booster_init_sysfs(struct input_booster *data)
{
	int ret;

	data->class = class_create(THIS_MODULE, "input_booster");
	if (IS_ERR(data->class)) {
		dev_err(data->dev, "Failed to create class\n");
		ret = IS_ERR(data->class);
		goto err_create_class;
	}

	ret = class_create_file(data->class, &class_attr_debug_level);
	if (ret) {
		dev_err(data->dev, "Failed to create class\n");
		goto err_create_class_file;
	}
	ret = class_create_file(data->class, &class_attr_head);
	if (ret) {
		dev_err(data->dev, "Failed to create class\n");
		goto err_create_class_file;
	}
	ret = class_create_file(data->class, &class_attr_tail);
	if (ret) {
		dev_err(data->dev, "Failed to create class\n");
		goto err_create_class_file;
	}
	ret = class_create_file(data->class, &class_attr_level);
	if (ret) {
		dev_err(data->dev, "Failed to create class\n");
		goto err_create_class_file;
	}


	return 0;

err_create_class_file:
	class_destroy(data->class);
err_create_class:
	return ret;
}

static void input_booster_free_dvfs_sysfs(struct input_booster *data, struct booster_dvfs *dvfs)
{
	sysfs_remove_group(&dvfs->sysfs_dev->kobj,
		&dvfs_attr_group);
	device_destroy(data->class, 0);
}

static void input_booster_init_dvfs_sysfs(struct input_booster *data, struct booster_dvfs *dvfs)
{
	int ret;

	dvfs->sysfs_dev = device_create(data->class,
			NULL, 0, dvfs, dvfs->name);
	if (IS_ERR(dvfs->sysfs_dev)) {
		ret = IS_ERR(dvfs->sysfs_dev);
		dev_err(data->dev, "Failed to create %s sysfs device[%d]\n", dvfs->name, ret);
		goto err_create_device;
	}

	ret = sysfs_create_group(&dvfs->sysfs_dev->kobj,
			&dvfs_attr_group);
	if (ret) {
		dev_err(data->dev, "Failed to create %s sysfs group\n", dvfs->name);
		goto err_create_group;
	}

	return;

err_create_group:
err_create_device:

	return;
}
#endif


static void input_booster_free_dvfs(struct input_booster *data)
{
	int i;

	cancel_work_sync(&data->event_work);
	for (i = 0; i < BOOSTER_DEVICE_MAX; i++) {
		if (!data->dvfses[i])
			continue;

		if (data->dvfses[i]->initialized) {
			cancel_delayed_work(&data->dvfses[i]->dvfs_chg_work);
			cancel_delayed_work(&data->dvfses[i]->dvfs_off_work);
#ifdef BOOSTER_SYSFS
			input_booster_free_dvfs_sysfs(data, data->dvfses[i]);
#endif
			kfree(data->dvfses[i]);
		}
	}
}

static int input_booster_init_dvfs(struct input_booster *data, struct booster_device *device)
{
	enum booster_device_type device_type = device->type;
	struct booster_dvfs *dvfs;
	int ret = 0;

	if (device_type >= BOOSTER_DEVICE_MAX) {
		dev_err(data->dev, "%s: Fail to init booster dvfs due to irregal device type[%d][%s]\n",
			__func__, device_type, device->desc);
		goto out;
	}

	if (data->dvfses[device_type] != NULL && data->dvfses[device_type]->initialized) {
		dev_info(data->dev, "%s: %s is already initialized [device = %s]\n",
			__func__, booster_device_name[device_type], device->desc);
		goto out;
	}

	dvfs = kzalloc(sizeof(struct booster_dvfs), GFP_KERNEL);
	if (!dvfs) {
		dev_err(data->dev, "%s: Fail to allocation for [%d][%s]\n",
			__func__, device_type, device->desc);
		ret = -ENOMEM;
		goto out;
	}

	switch (device_type) {
	case BOOSTER_DEVICE_KEY:
		INIT_DELAYED_WORK(&dvfs->dvfs_chg_work, DVFS_WORK_FUNC(CHG, KEY));
		INIT_DELAYED_WORK(&dvfs->dvfs_off_work, DVFS_WORK_FUNC(OFF, KEY));
		dvfs->set_dvfs_lock = DVFS_WORK_FUNC(SET, KEY);
	break;
	case BOOSTER_DEVICE_TOUCHKEY:
		INIT_DELAYED_WORK(&dvfs->dvfs_chg_work, DVFS_WORK_FUNC(CHG, TOUCHKEY));
		INIT_DELAYED_WORK(&dvfs->dvfs_off_work, DVFS_WORK_FUNC(OFF, TOUCHKEY));
		dvfs->set_dvfs_lock = DVFS_WORK_FUNC(SET, TOUCHKEY);
	break;
	case BOOSTER_DEVICE_TOUCH:
		INIT_DELAYED_WORK(&dvfs->dvfs_chg_work, DVFS_WORK_FUNC(CHG, TOUCH));
		INIT_DELAYED_WORK(&dvfs->dvfs_off_work, DVFS_WORK_FUNC(OFF, TOUCH));
		dvfs->set_dvfs_lock = DVFS_WORK_FUNC(SET, TOUCH);
	break;
	case BOOSTER_DEVICE_PEN:
		INIT_DELAYED_WORK(&dvfs->dvfs_chg_work, DVFS_WORK_FUNC(CHG, PEN));
		INIT_DELAYED_WORK(&dvfs->dvfs_off_work, DVFS_WORK_FUNC(OFF, PEN));
		dvfs->set_dvfs_lock = DVFS_WORK_FUNC(SET, PEN);
	break;
	default:
		dev_err(data->dev, "%s: Fail to init booster dvfs due to irregal device type[%d]\n",
			__func__, device_type);
		goto out;
	break;
	}

	dvfs->level = BOOSTER_LEVEL2;
	memcpy(dvfs->freqs, device->freq_table, sizeof(struct dvfs_freq) * BOOSTER_LEVEL_MAX);
	memcpy(dvfs->times, device->time_table, sizeof(struct dvfs_time) * BOOSTER_LEVEL_MAX);

	dvfs->device_type = device_type;
	dvfs->name = booster_device_name[device_type];
#ifdef BOOSTER_SYSFS
	input_booster_init_dvfs_sysfs(data, dvfs);
#endif
	dvfs->parent_dev = data->dev;

	mutex_init(&dvfs->lock);
	dvfs->initialized = true;

	data->dvfses[device_type] = dvfs;

	dev_info(data->dev, "%s: %s is registered and intialized for %s dvfs\n",
		__func__, device->desc, dvfs->name);
#ifdef BOOSTER_DEBUG
	input_booster_lookup_freqs(dvfs);
	input_booster_lookup_times(dvfs);
#endif
out:
	return ret;
}

#ifdef CONFIG_OF
static int input_booster_parse_dt(struct device *dev)
{
	struct input_booster_platform_data *pdata = dev->platform_data;
	struct booster_device *booster_device;
	struct device_node *np, *cnp;
	enum booster_device_type device_type;
	const u32 *plevels = NULL;
	int device_count = 0, num_level = 0, level = 0;
	int retval = 0, i;

	np = dev->of_node;
	pdata->ndevices = of_get_child_count(np);
	if (!pdata->ndevices) {
		dev_err(dev, "There are no booster devices\n");
		return -ENODEV;
	}
	pdata->devices = devm_kzalloc(dev,
			sizeof(struct booster_device) * pdata->ndevices, GFP_KERNEL);
	if (!pdata->devices) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	for_each_child_of_node(np, cnp) {
		booster_device = &pdata->devices[device_count];
		booster_device->desc = of_get_property(cnp, "input_booster,label", NULL);
		if (of_property_read_u32(cnp, "input_booster,type", &booster_device->type)) {
			dev_err(dev, "Failed to get type property\n");
			retval = -EINVAL;
			goto err_out;
		}

		/* Get levels and device type */
		plevels = of_get_property(cnp, "input_booster,levels", &num_level);
		if (plevels && num_level) {
			num_level = num_level / sizeof(u32);
			device_type = booster_device->type;
			if (device_type >= BOOSTER_DEVICE_MAX) {
				dev_err(dev, "Device type[%d] is over than BOOSTER_DEVICE_MAX.\n", device_type);
				retval = -EINVAL;
				goto err_out;
			}
		} else {
			dev_err(dev, "Failed to calculate number of frequency.\n");
			retval = -EINVAL;
			goto err_out;
		}

		/* Allocation for freq and time table */
		if (!pdata->freq_tables[device_type]) {
			pdata->freq_tables[device_type] = devm_kzalloc(dev,
				sizeof(struct dvfs_freq) * BOOSTER_LEVEL_MAX, GFP_KERNEL);
			if (!pdata->freq_tables[device_type]) {
				dev_err(dev, "Failed to allocate memory of freq_table\n");
				retval = -ENOMEM;
				goto err_out;
			}
		}

		if (!pdata->time_tables[device_type]) {
			pdata->time_tables[device_type] = devm_kzalloc(dev,
				sizeof(struct dvfs_time) * BOOSTER_LEVEL_MAX, GFP_KERNEL);
			if (!pdata->time_tables[device_type] ) {
				dev_err(dev, "Failed to allocate memory of time_table\n");
				retval = -ENOMEM;
				goto err_out;
			}
		}

		/* Get and add frequncy and time table */
		for (i = 0; i < num_level; i++) {
			if (of_property_read_u32_index(cnp, "input_booster,levels", i, &level)) {
				dev_err(dev, "Failed to get levels property\n");
				retval = -EINVAL;
				goto err_out;
			}
			retval = of_property_read_u32_index(cnp, "input_booster,cpu_freqs", i,
				 &pdata->freq_tables[device_type][level].cpu_freq);
			retval |= of_property_read_u32_index(cnp, "input_booster,kfc_freqs", i,
				 &pdata->freq_tables[device_type][level].kfc_freq);
			retval |= of_property_read_u32_index(cnp, "input_booster,mif_freqs", i,
				 &pdata->freq_tables[device_type][level].mif_freq);
			retval |= of_property_read_u32_index(cnp, "input_booster,int_freqs", i,
				 &pdata->freq_tables[device_type][level].int_freq);
			retval |= of_property_read_u32_index(cnp, "input_booster,hmp_boost", i,
				 &pdata->freq_tables[device_type][level].hmp_boost);
			if (retval) {
				dev_err(dev, "Failed to get xxx_freqs property\n");
				retval = -EINVAL;
				goto err_out;
			}

			retval = of_property_read_u32_index(cnp, "input_booster,head_times", i,
				 &pdata->time_tables[device_type][level].head_time);
			retval |= of_property_read_u32_index(cnp, "input_booster,tail_times", i,
				 &pdata->time_tables[device_type][level].tail_time);
			retval |= of_property_read_u32_index(cnp, "input_booster,phase_times", i,
				 &pdata->time_tables[device_type][level].phase_time);
			if (retval) {
				dev_err(dev, "Failed to get xxx_times property\n");
				retval = -EINVAL;
				goto err_out;
			}
		}
		booster_device->freq_table = pdata->freq_tables[device_type];
		booster_device->time_table = pdata->time_tables[device_type];

		if(device_type==BOOSTER_DEVICE_TOUCH){
			sysfs_head_time = booster_device->time_table[BOOSTER_LEVEL2].head_time;
			head_cpu_freq = booster_device->freq_table[BOOSTER_LEVEL2].cpu_freq;
			head_kfc_freq = booster_device->freq_table[BOOSTER_LEVEL2].kfc_freq;
			head_mif_freq = booster_device->freq_table[BOOSTER_LEVEL2].mif_freq;
			head_int_freq = booster_device->freq_table[BOOSTER_LEVEL2].int_freq;
			head_hmp_boost = booster_device->freq_table[BOOSTER_LEVEL2].hmp_boost;

			sysfs_tail_time = booster_device->time_table[BOOSTER_LEVEL2].tail_time;
			tail_cpu_freq = booster_device->freq_table[BOOSTER_LEVEL2_CHG].cpu_freq;
			tail_kfc_freq = booster_device->freq_table[BOOSTER_LEVEL2_CHG].kfc_freq;
			tail_mif_freq = booster_device->freq_table[BOOSTER_LEVEL2_CHG].mif_freq;
			tail_int_freq = booster_device->freq_table[BOOSTER_LEVEL2_CHG].int_freq;
			tail_hmp_boost = booster_device->freq_table[BOOSTER_LEVEL2_CHG].hmp_boost;
		}

#ifdef BOOSTER_DEBUG
		dev_info(dev, "device_type:%d, label :%s, type: 0x%02x, num_level[%d]\n",
			device_type, booster_device->desc, booster_device->type, num_level);
		for (i = 0; i < BOOSTER_LEVEL_MAX; i++) {
			dev_info(dev, "Level %d : frequency[%d,%d,%d,%d] hmp_boost[%d]\n", i,
				booster_device->freq_table[i].cpu_freq,
				booster_device->freq_table[i].kfc_freq,
				booster_device->freq_table[i].mif_freq,
				booster_device->freq_table[i].int_freq,
				booster_device->freq_table[i].hmp_boost);
			dev_info(dev, "Level %d : times[%d,%d,%d]\n", i,
				booster_device->time_table[i].head_time,
				booster_device->time_table[i].tail_time,
				booster_device->time_table[i].phase_time);
		}
#endif
		device_count++;
	}

	return 0;

err_out:
	return retval;
}
#endif

static int input_booster_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	struct input_booster *data;
	struct input_booster_platform_data *pdata;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct input_booster_platform_data), GFP_KERNEL);

		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate platform data\n");
			return -ENOMEM;
		}

		pdev->dev.platform_data = pdata;
		ret = input_booster_parse_dt(&pdev->dev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to parse dt data\n");
			return ret;
		}
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&pdev->dev, "There are no platform data\n");
		return -EINVAL;
	}

	if (!pdata->ndevices || !pdata->devices) {
		dev_err(&pdev->dev, "There are no devices for booster\n");
		return -EINVAL;
	}

	data = kzalloc(sizeof(struct input_booster)
			+ sizeof(struct booster_device) * pdata->ndevices, GFP_KERNEL);

	if (!data) {
		dev_err(&pdev->dev, "Fail to allocate booster data\n");
		return -ENOMEM;
	}

	memcpy(data->devices, pdata->devices, sizeof(struct booster_device) * pdata->ndevices);

#ifdef BOOSTER_DEBUG
	for (i = 0; i < pdata->ndevices; i++) {
		dev_info(&pdev->dev, "%s: data->devices[%d] : desc=%s, type=0x%02X\n",
			__func__, i, data->devices[i].desc, data->devices[i].type);
	}
#endif

	data->ndevices = pdata->ndevices;
	data->dev = &pdev->dev;
	g_data = data;
	g_data->ndevices = pdata->ndevices;

#ifdef BOOSTER_SYSFS
	ret = input_booster_init_sysfs(data);
	if (ret)
		goto err_init_sysfs;
#endif

	spin_lock_init(&data->buffer_lock);
	data->buffer_size = sizeof(data->buffer)/sizeof(struct booster_event);
	INIT_WORK(&data->event_work, input_booster_event_work);

	for (i = 0; i < data->ndevices; i++) {
		if (data->devices[i].type >= BOOSTER_DEVICE_MAX) {
			dev_err(data->dev, "%s: data->devices[%d].type = 0x%02X\n", __func__, i, data->devices[i].type);
			continue;
		}

		ret = input_booster_init_dvfs(data, &data->devices[i]);
		if (ret) {
			dev_err(&pdev->dev, "Failed to init dvfs for %s\n", data->devices[i].desc);
			goto err_init_dvfs;
		}
		__set_bit(data->devices[i].type, data->dev_bit);
	}

	platform_set_drvdata(pdev, data);

	return 0;

err_init_dvfs:
	input_booster_free_dvfs(data);
err_init_sysfs:
	kfree(data);
	g_data = NULL;

	return ret;
}

static int input_booster_remove(struct platform_device *pdev)
{
	struct input_booster *data = platform_get_drvdata(pdev);

	input_booster_free_dvfs(data);
#ifdef BOOSTER_SYSFS
	input_booster_free_sysfs(data);
#endif
	kfree(data);
	g_data = NULL;

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id input_booster_dt_ids[] = {
	{ .compatible = "input_booster" },
	{ }
};
#endif

static struct platform_driver input_booster_driver = {
	.driver = {
		.name = INPUT_BOOSTER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(input_booster_dt_ids),
#endif
	},
	.probe = input_booster_probe,
	.remove = input_booster_remove,
};

static int __init input_booster_init(void)
{
	return platform_driver_register(&input_booster_driver);
}

static void __exit input_booster_exit(void)
{
	return platform_driver_unregister(&input_booster_driver);
}

late_initcall(input_booster_init);
module_exit(input_booster_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SAMSUNG Electronics");
MODULE_DESCRIPTION("SEC INPUT BOOSTER DEVICE");
