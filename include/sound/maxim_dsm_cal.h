/*
 * maxim_dsm_cal.c -- Module for Rdc calibration
 *
 * Copyright 2014 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SOUND_MAXIM_DSM_CAL_H__
#define __SOUND_MAXIM_DSM_CAL_H__

#define DRIVER_AUTHOR		"Kyounghun Jeon<hun.jeon@maximintegrated.com>"
#define DRIVER_DESC			"For Rdc calibration of MAX98xxx"
#define DRIVER_SUPPORTED	"MAX98xxx"

#define WQ_NAME				"maxdsm_wq"

#define FILEPATH_TEMP_CAL	"/efs/maxim/temp_cal"
#define FILEPATH_RDC_CAL	"/efs/maxim/rdc_cal"

#define CLASS_NAME			"maxdsm_cal"
#define DSM_NAME			"dsm"

#define ADDR_RDC			0x2A0050
#define ADDR_FEATURE_ENABLE 0x2A006A

#define maxdsm_cal_make_get(__name, __own, __value) \
int maxdsm_cal_get_##__own(uint32_t *__value) \
{ \
	char data[12]; \
	int ret; \
	memset(data, 0x00, sizeof(data)); \
	ret = maxdsm_cal_read_file(__name, data, sizeof(data)); \
	if (ret < 0) \
		g_mdc->values.__own = ret; \
	else \
		sscanf(data, "%x", __value); \
	return ret; \
} \
EXPORT_SYMBOL_GPL(maxdsm_cal_get_##__own);

#define maxdsm_cal_make_set(__name, __own, __value) \
int maxdsm_cal_set_##__own(uint32_t __value) \
{ \
	char data[12]; \
	int ret; \
	memset(data, 0x00, sizeof(data)); \
	sprintf(data, "%x", __value); \
	ret = maxdsm_cal_write_file(__name, data, sizeof(data)); \
	g_mdc->values.__own = ret < 0 ? ret : __value; \
	return ret; \
} \
EXPORT_SYMBOL_GPL(maxdsm_cal_set_##__own);

struct maxim_dsm_cal_info {
	uint32_t min;
	uint32_t max;
	uint32_t feature_en;
	int interval;
	int duration;
	int remaining;
	int ignored_t;
	unsigned long previous_jiffies;
};

struct maxim_dsm_cal_values {
	uint32_t status;
	int rdc;
	int temp;
	uint64_t avg;
	int count;
};

struct maxim_dsm_cal {
	struct device *dev;
	struct class *class;
	struct mutex mutex;
	struct workqueue_struct *wq;
	struct delayed_work work;
	struct maxim_dsm_cal_values values;
	struct maxim_dsm_cal_info info;
	struct regmap *regmap;
	uint32_t platform_type;
};

#if defined(CONFIG_SND_SOC_MAX98505) || defined(CONFIG_SND_SOC_MAX98505A)
extern struct class *g_class;
#else
struct class *g_class;
#endif /* CONFIG_SND_SOC_MAX98505 || CONFIG_SND_SOC_MAX98505A */

extern struct regmap *maxdsm_cal_set_regmap(
		struct regmap *regmap);
extern int maxdsm_cal_get_temp(uint32_t *temp);
extern int maxdsm_cal_set_temp(uint32_t temp);
extern int maxdsm_cal_get_rdc(uint32_t *rdc);
extern int maxdsm_cal_set_rdc(uint32_t rdc);
#endif /* __SOUND_MAXIM_DSM_CAL_H__ */
