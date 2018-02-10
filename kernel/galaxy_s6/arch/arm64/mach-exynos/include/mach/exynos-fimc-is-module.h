/*
 * /include/media/exynos_fimc_is_sensor.h
 *
 * Copyright (C) 2012 Samsung Electronics, Co. Ltd
 *
 * Exynos series exynos_fimc_is_sensor device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MEDIA_EXYNOS_MODULE_H
#define MEDIA_EXYNOS_MODULE_H

#include <linux/platform_device.h>

#define GPIO_SCENARIO_ON		0
#define GPIO_SCENARIO_OFF		1

#ifdef CONFIG_COMPANION_STANDBY_USE
#define GPIO_SCENARIO_STANDBY_ENABLE	2
#define GPIO_SCENARIO_STANDBY_DISABLE	3
#define GPIO_SCENARIO_MAX		4
#else
#define GPIO_SCENARIO_MAX		2
#endif

#define GPIO_CTRL_MAX			32

#define SENSOR_SCENARIO_NORMAL		0
#define SENSOR_SCENARIO_VISION		1
#define SENSOR_SCENARIO_EXTERNAL	2
#define SENSOR_SCENARIO_OIS_FACTORY		3
#define SENSOR_SCENARIO_MAX		10

#ifdef CONFIG_COMPANION_STANDBY_USE
#define SENSOR_STATE_OFF		0x0
#define SENSOR_STATE_ON			0x1
#define SENSOR_STATE_STANDBY		0x2
#define SENSOR_STATE_UNKNOWN		0x3
#define SENSOR_MASK			0x3
#define SENSOR_STATE_SENSOR		0
#define SENSOR_STATE_COMPANION		2

#define SET_SENSOR_STATE(state, sensor, value) \
	state = (state & ~(SENSOR_MASK << sensor)) | value << sensor;
#define GET_SENSOR_STATE(state, sensor) \
	(((state & (SENSOR_MASK << sensor)) >> sensor) & SENSOR_MASK)
#endif

enum pin_act {
	PIN_NONE = 0,
	PIN_OUTPUT,
	PIN_INPUT,
	PIN_RESET,
	PIN_FUNCTION,
	PIN_REGULATOR
};

struct exynos_sensor_pin {
	ulong pin;
	u32 delay;
	u32 value;
	char *name;
	enum pin_act act;
	u32 voltage;
};

#define SET_PIN_INIT(d, s, c) d->pinctrl_index[s][c] = 0;

#define SET_PIN(d, s, c, p, n, a, v, t) \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].pin	= p; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].name	= n; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].act	= a; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].value	= v; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].delay	= t; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].voltage = 0; \
		(d)->pinctrl_index[s][c]++;

#define SET_PIN_VOLTAGE(d, s, c, p, n, a, v, t, e) \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].pin	= p; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].name	= n; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].act	= a; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].value	= v; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].delay	= t; \
		(d)->pin_ctrls[s][c][d->pinctrl_index[s][c]].voltage	= e; \
		(d)->pinctrl_index[s][c]++;

struct exynos_platform_fimc_is_module {
	int (*gpio_cfg)(struct platform_device *pdev, u32 scenario, u32 enable);
	int (*gpio_dbg)(struct platform_device *pdev, u32 scenario, u32 enable);
	struct exynos_sensor_pin pin_ctrls[SENSOR_SCENARIO_MAX][GPIO_SCENARIO_MAX][GPIO_CTRL_MAX];
	u32 pinctrl_index[SENSOR_SCENARIO_MAX][GPIO_SCENARIO_MAX];
	struct pinctrl *pinctrl;
	u32 position;
	u32 id;
	u32 sensor_i2c_ch;
	u32 sensor_i2c_addr;
	u32 is_bns;
	u32 flash_product_name;
	u32 flash_first_gpio;
	u32 flash_second_gpio;
	u32 af_product_name;
	u32 af_i2c_addr;
	u32 af_i2c_ch;
	u32 ois_product_name;
	u32 ois_i2c_addr;
	u32 ois_i2c_ch;
	u32 companion_product_name;
	u32 companion_spi_channel;
	u32 companion_i2c_addr;
	u32 companion_i2c_ch;
	u32 is_softlanding;
	u32 sensor_id;
	u32 mclk_ch;
	bool companion_use_pmic;
};

extern int exynos_fimc_is_module_pins_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 enable);
extern int exynos_fimc_is_module_pins_dbg(struct platform_device *pdev,
	u32 scenario,
	u32 enable);

#endif /* MEDIA_EXYNOS_MODULE_H */
