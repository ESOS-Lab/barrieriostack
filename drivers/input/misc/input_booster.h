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

#ifndef _INPUT_BOOSTER_H_
#define _INPUT_BOOSTER_H_

#include <linux/input.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/input/input_booster.h>
#include <linux/pm_qos.h>
#include <linux/of.h>

#ifdef CONFIG_SCHED_HMP
#include <linux/sched.h>
#define USE_HMP_BOOST
#endif

#define BOOSTER_EVENT_BUFFER_SIZE	10

/* Feature define */
#define BOOSTER_SYSFS
#define USE_WORKQUEUING

#ifdef BOOSTER_SYSFS
extern struct class *sec_class;
#endif

#define set_qos(req, pm_qos_class, value) { \
	if (value) { \
		if (pm_qos_request_active(req)) \
			pm_qos_update_request(req, value); \
		else \
			pm_qos_add_request(req, pm_qos_class, value); \
	} \
}

#define remove_qos(req) { \
	if (pm_qos_request_active(req)) \
		pm_qos_remove_request(req); \
}

#ifdef USE_HMP_BOOST
#define SET_HMP(data, device_type, enable)	\
	input_booster_set_hmp_boost(data, device_type, enable);
#else
#define SET_HMP(data, device_type, enable)
#endif

#define DECLARE_DVFS_DELAYED_WORK_FUNC(mode, device)	\
	static void	input_booster_##mode##_dvfs_work_func_##device	\
			(struct work_struct *work)

#define DECLARE_DVFS_WORK_FUNC(mode, device)	\
	static void	input_booster_##mode##_dvfs_work_func_##device	\
			(void *booster_data, enum booster_mode booster_mode)

#define DVFS_WORK_FUNC(mode, device)		\
	input_booster_##mode##_dvfs_work_func_##device

enum booster_dbg_level {
	DBG_DVFS = 1,
	DBG_EVENT,
	DBG_MISC,
};

#define DVFS_DEV_DBG(level, dev, fmt, args...)	\
{											\
	if (level <= dbg_level)					\
		dev_info(dev, fmt, ## args);		\
	else									\
		dev_dbg(dev, fmt, ## args);			\
}

/*
 * struct booster_dvfs - struct for booster.
 * @name :
 * @initialized :
 * @lock_status :
 * @level :
 * @off_time_ms :
 * @chg_time_ms :
 * @lock :
 * @dvfs_chg_work :
 * @dvfs_off_work :
 * @cpu_qos :
 * @mif_qos :
 * @int_qos :
 * @set_dvfs_lock :
 * @phase_excuted : To check phase timer excuted or not.
 * @parent_dev :
 */
struct booster_dvfs {
	enum booster_device_type device_type;
	const char *name;
#ifdef BOOSTER_SYSFS
	struct device *sysfs_dev;
#endif
	bool initialized;
	bool lock_status;
	bool short_press;
	enum booster_level level;
	struct mutex lock;
	struct delayed_work	dvfs_chg_work;
	struct delayed_work	dvfs_off_work;
	struct dvfs_freq freqs[BOOSTER_LEVEL_MAX];
	struct dvfs_time times[BOOSTER_LEVEL_MAX];
	struct pm_qos_request	cpu_qos;
	struct pm_qos_request	kfc_qos;
	struct pm_qos_request	mif_qos;
	struct pm_qos_request	int_qos;
	void (*set_dvfs_lock) (void *booster_data, enum booster_mode mode);
#ifdef USE_HMP_BOOST
	bool hmp_boosted;
#endif
	bool phase_excuted;
	struct device *parent_dev;
};

/*
 * struct booster_event - struct for booster.
 * @type :
 * @mode :
 */
struct booster_event {
	enum booster_device_type device_type;
	enum booster_mode mode;
};

/*
 * struct input_booster - struct for booster.
 * @dev :
 * @input_handler :
 * @dev_bit :
 * @dvfses :
 * @ndevicess :
 * @dvfs_off_work :
 * @devices :
 */
struct input_booster {
	struct device *dev;
#ifdef BOOSTER_SYSFS
	struct class *class;
#endif
	unsigned long dev_bit[BITS_TO_LONGS(BOOSTER_DEVICE_MAX)];
	struct booster_dvfs *dvfses[BOOSTER_DEVICE_MAX];

	unsigned int rear;
	unsigned int front;
	spinlock_t buffer_lock; /* protects access to buffer, front and rear */
	unsigned int buffer_size;
	struct booster_event buffer[BOOSTER_EVENT_BUFFER_SIZE];
	struct work_struct event_work;

	int ndevices;
	struct booster_device devices[];
};

#endif
