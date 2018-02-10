/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/i2c.h>
#include <linux/pinctrl/pinctrl-samsung.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/exynos-fimc-is-companion.h>

#include "fimc-is-video.h"
#include "fimc-is-dt.h"
#ifdef CONFIG_COMPANION_USE
#include "fimc-is-companion-dt.h"
#endif
#include "fimc-is-spi.h"
#include "fimc-is-device-companion.h"
#include "fimc-is-sec-define.h"
#if defined(CONFIG_OIS_USE)
#include "fimc-is-device-ois.h"
#endif
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"
#ifdef CONFIG_COMPANION_DCDC_USE
#include "fimc-is-pmic.h"
#endif

#define MAX_SENSOR_GPIO_ON_WAITING 1000 /* 1 second */

extern int fimc_is_comp_video_probe(void *data);

extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;
extern struct pm_qos_request exynos_isp_qos_disp;

int fimc_is_companion_g_module(struct fimc_is_device_companion *device,
	struct fimc_is_module_enum **module)
{
	int ret = 0;

	BUG_ON(!device);

	*module = device->module;
	if (!*module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_companion_mclk_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct platform_device *pdev;
	struct exynos_platform_fimc_is_companion *pdata;
	struct fimc_is_core *core;
	struct exynos_platform_fimc_is_sensor *pdata_sensor;
	struct exynos_platform_fimc_is_module *module_pdata_sensor;
	struct fimc_is_module_enum *module_sensor = NULL;
	struct fimc_is_device_sensor *device_sensor;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdev = device->pdev;
	pdata = device->pdata;

	device_sensor = &core->sensor[pdata->id];

	BUG_ON(!device_sensor);
	BUG_ON(!device_sensor->pdev);
	BUG_ON(!device_sensor->pdata);

	if (test_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state)) {
		err("%s : already clk on", __func__);
		goto p_err;
	}

	pdata_sensor = device_sensor->pdata;

	if (test_bit(FIMC_IS_SENSOR_MCLK_ON, &device_sensor->state)) {
		merr("%s : already sensor clk on", device_sensor, __func__);
		goto p_companion_mclk_on;
	}

	if (!pdata_sensor->mclk_on) {
		merr("sensor mclk_on is NULL", device_sensor);
		goto p_companion_mclk_on;
	}

	module_sensor = device->module;

	module_pdata_sensor = module_sensor->pdata;

	ret = pdata_sensor->mclk_on(device_sensor->pdev, pdata_sensor->scenario, module_pdata_sensor->mclk_ch);
	if (ret) {
		merr("sensor mclk_on is fail(%d)", device_sensor, ret);
		goto p_companion_mclk_on;
	}

	set_bit(FIMC_IS_SENSOR_MCLK_ON, &device_sensor->state);

p_companion_mclk_on:

	if (!pdata->mclk_on) {
		err("mclk_on is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->mclk_on(pdev, pdata->scenario, pdata->mclk_ch);
	if (ret) {
		err("mclk_on is fail(%d)", ret);
		goto p_err;
	}

	set_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_mclk_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct platform_device *pdev;
	struct exynos_platform_fimc_is_companion *pdata;
	struct fimc_is_core *core;
	struct exynos_platform_fimc_is_sensor *pdata_sensor;
	struct exynos_platform_fimc_is_module *module_pdata_sensor;
	struct fimc_is_module_enum *module_sensor = NULL;
	struct fimc_is_device_sensor *device_sensor;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdev = device->pdev;
	pdata = device->pdata;

	device_sensor = &core->sensor[pdata->id];

	BUG_ON(!device_sensor);
	BUG_ON(!device_sensor->pdev);
	BUG_ON(!device_sensor->pdata);

	if (!test_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state)) {
		err("%s : already clk off", __func__);
		goto p_err;
	}

	pdata_sensor = device_sensor->pdata;

	if (!test_bit(FIMC_IS_SENSOR_MCLK_ON, &device_sensor->state)) {
		merr("%s : already sensor clk off", device_sensor, __func__);
		goto p_companion_mclk_off;
	}

	if (!pdata_sensor->mclk_off) {
		merr("sensor mclk_off is NULL", device_sensor);
		goto p_companion_mclk_off;
	}

	module_sensor = device->module;

	module_pdata_sensor = module_sensor->pdata;

	ret = pdata_sensor->mclk_off(device_sensor->pdev, pdata_sensor->scenario, module_pdata_sensor->mclk_ch);
	if (ret) {
		merr("sensor mclk_off is fail(%d)", device_sensor, ret);
		goto p_companion_mclk_off;
	}

	clear_bit(FIMC_IS_SENSOR_MCLK_ON, &device_sensor->state);

p_companion_mclk_off:

	if (!pdata->mclk_off) {
		err("mclk_off is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->mclk_off(pdev, pdata->scenario, pdata->mclk_ch);
	if (ret) {
		err("mclk_off is fail(%d)", ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_iclk_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct platform_device *pdev;
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdev = device->pdev;
	pdata = device->pdata;

	if (test_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state)) {
		err("%s : already clk on", __func__);
		goto p_err;
	}

	if (!pdata->iclk_cfg) {
		err("iclk_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->iclk_on) {
		err("iclk_on is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_cfg(pdev, pdata->scenario, 0);
	if (ret) {
		err("iclk_cfg is fail(%d)", ret);
		goto p_err;
	}

	ret = pdata->iclk_on(pdev, pdata->scenario, 0);
	if (ret) {
		err("iclk_on is fail(%d)", ret);
		goto p_err;
	}

	set_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_iclk_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct platform_device *pdev;
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdev = device->pdev;
	pdata = device->pdata;

	if (!test_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state)) {
		err("%s : already clk off", __func__);
		goto p_err;
	}

	if (!pdata->iclk_off) {
		err("iclk_off is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_off(pdev, pdata->scenario, 0);
	if (ret) {
		err("iclk_off is fail(%d)", ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_gpio_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_module_enum *module = NULL;
	u32 enable = GPIO_SCENARIO_ON;
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *device_sensor;
#ifdef CONFIG_COMPANION_DCDC_USE
	struct dcdc_power *dcdc;
	const char *vout_str = NULL;
	int vout = 0;
#endif

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	device_sensor = &core->sensor[device->pdata->id];

	BUG_ON(!device_sensor);

	if (test_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state)) {
		err("%s : already gpio on", __func__);
		goto p_err;
	}

	ret = fimc_is_companion_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (module->position == SENSOR_POSITION_FRONT){
		core->running_front_camera = true;
	} else {
		core->running_rear_camera = true;
	}

	if (!test_and_set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state)) {
		struct exynos_platform_fimc_is_module *pdata;

		scenario = device->pdata->scenario;
		pdata = module->pdata;
		if (!pdata) {
			clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			merr("pdata is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (!pdata->gpio_cfg) {
			clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg is NULL");
			ret = -EINVAL;
			goto p_err;
		}

#ifdef CONFIG_COMPANION_STANDBY_USE
		if (scenario == SENSOR_SCENARIO_NORMAL) {
			if (GET_SENSOR_STATE(device->pdata->standby_state, SENSOR_STATE_COMPANION) == SENSOR_STATE_STANDBY) {
				enable = GPIO_SCENARIO_STANDBY_DISABLE;
			}
		}
#endif

#ifdef CONFIG_COMPANION_DCDC_USE
		dcdc = &core->companion_dcdc;

		if (dcdc->type == DCDC_VENDOR_PMIC) {
			/* Set default voltage without power binning if FIMC_IS_ISP_CV not exist. */
			if (!fimc_is_sec_file_exist(FIMC_IS_ISP_CV)) {
				info("Companion file not exist (%s)\n", FIMC_IS_ISP_CV);

				/* Get default vout in power binning table if EVT1 */
				if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT1) {
					vout = dcdc->get_vout_val(0);
					vout_str = dcdc->get_vout_str(0);
				/* If not, get default vout for both EVT0 and EVT1 */
				} else {
					if (dcdc->get_default_vout_val(&vout, &vout_str))
						err("fail to get companion default vout");
				}

				info("Companion: Set default voltage %sV\n", vout_str ? vout_str : "0");
				dcdc->set_vout(dcdc->client, vout);
			/* Do power binning if FIMC_IS_ISP_CV exist with PMIC */
			} else {
				fimc_is_power_binning(core);
			}
		}

#else /* !CONFIG_COMPANION_DCDC_USE*/
		/* Temporary Fixes. Set voltage to 0.85V for EVT0, 0.8V for EVT1 */
		if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT1) {
			info("%s: Companion EVT1. Set voltage 0.8V\n", __func__);
			ret = fimc_is_comp_set_voltage("VDDD_CORE_0.8V_COMP", 800000);
		} else if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT0) {
			info("%s: Companion EVT0. Set voltage 0.85V\n", __func__);
			ret = fimc_is_comp_set_voltage("VDDD_CORE_0.8V_COMP", 850000);
		} else {
			info("%s: Companion unknown rev. Set default voltage 0.85V\n", __func__);
			ret = fimc_is_comp_set_voltage("VDDD_CORE_0.8V_COMP", 850000);
		}
		if (ret < 0) {
			err("Companion core_0.8v setting fail!");
		}
#endif /* CONFIG_COMPANION_DCDC_USE */

		ret = pdata->gpio_cfg(module->pdev, scenario, enable);
		if (ret) {
			clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg is fail(%d)", ret);
			goto p_err;
		}
		set_bit(FIMC_IS_SENSOR_GPIO_ON, &device_sensor->state);
	}

	set_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_gpio_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_module_enum *module = NULL;
	u32 enable = GPIO_SCENARIO_OFF;
	struct fimc_is_core *core;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state)) {
		err("%s : already gpio off", __func__);
		goto p_err;
	}

	ret = fimc_is_companion_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	scenario = device->pdata->scenario;

	if (test_and_clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state)) {
		struct exynos_platform_fimc_is_module *pdata;

		pdata = module->pdata;
		if (!pdata) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			merr("pdata is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (!pdata->gpio_cfg) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg is NULL");
			ret = -EINVAL;
			goto p_err;
		}

#ifdef CONFIG_COMPANION_STANDBY_USE
		if (scenario == SENSOR_SCENARIO_NORMAL) {
			if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT1) {
				if (GET_SENSOR_STATE(device->pdata->standby_state, SENSOR_STATE_COMPANION) == SENSOR_STATE_ON) {
					enable = GPIO_SCENARIO_STANDBY_ENABLE;
				}
			}
		}
#endif

		ret = pdata->gpio_cfg(module->pdev, scenario, enable);
		if (ret) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg is fail(%d)", ret);
			goto p_err;
		}
	}

#ifdef CONFIG_COMPANION_STANDBY_USE
	if (scenario == SENSOR_SCENARIO_NORMAL) {
		if (enable == GPIO_SCENARIO_STANDBY_ENABLE) {
			SET_SENSOR_STATE(device->pdata->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_STANDBY);
		} else if (enable == GPIO_SCENARIO_OFF) {
			SET_SENSOR_STATE(device->pdata->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_OFF);
		}
		info("%s: COMPANION STATE %u\n", __func__, GET_SENSOR_STATE(device->pdata->standby_state, SENSOR_STATE_COMPANION));
	}
#endif

	clear_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

p_err:
	if (module != NULL) {
		if (module->position == SENSOR_POSITION_FRONT){
			core->running_front_camera = false;
		} else {
			core->running_rear_camera = false;
		}
	}

	return ret;
}

int fimc_is_companion_open(struct fimc_is_device_companion *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_spi_gpio *spi_gpio0;
	struct fimc_is_spi_gpio *spi_gpio1;
	struct fimc_is_core *core;
	struct exynos_platform_fimc_is_companion *pdata = device->pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdata);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		ret = -ENODEV;
		goto p_err;
	}
	spi_gpio0 = &core->spi0.gpio;
	spi_gpio1 = &core->spi1.gpio;

	if (test_bit(FIMC_IS_COMPANION_OPEN, &device->state)) {
		err("already open");
		ret = -EMFILE;
		goto p_err;
	}

	ret = fimc_is_resource_get(device->resourcemgr, RESOURCE_TYPE_COMPANION);
	if (ret) {
		merr("fimc_is_resource_get is fail", device);
		goto p_err;
	}

	device->vctx = vctx;

	fimc_is_spi_s_pin(spi_gpio0, PINCFG_TYPE_DAT, 0);
	fimc_is_spi_s_pin(spi_gpio1, PINCFG_TYPE_DAT, 0);

	fimc_is_spi_s_pin(spi_gpio0, PINCFG_TYPE_FUNC, FUNC_OUTPUT);
	fimc_is_spi_s_pin(spi_gpio1, PINCFG_TYPE_FUNC, FUNC_OUTPUT);

	/* Set to compaion PAF interrupt function */
	pin_config_set(pdata->comp_int_pinctrl, pdata->comp_int_pin,
			PINCFG_PACK(PINCFG_TYPE_FUNC, 0xf));

	set_bit(FIMC_IS_COMPANION_OPEN, &device->state);

p_err:
	minfo("[COM:D] %s():%d\n", device, __func__, ret);
	return ret;
}

int fimc_is_companion_close(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct exynos_platform_fimc_is_companion *pdata = device->pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdata);

	if (!test_bit(FIMC_IS_COMPANION_OPEN, &device->state)) {
		err("already close");
		ret = -EMFILE;
		goto p_err;
	}

	ret = fimc_is_resource_put(device->resourcemgr, RESOURCE_TYPE_COMPANION);
	if (ret)
		merr("fimc_is_resource_put is fail", device);

	core = (struct fimc_is_core *)device->resourcemgr->private_data;

	if (atomic_read(&core->resourcemgr.resource_ischain.rsccount) == 0) {
		if (core->spi0.device) {
			fimc_is_spi_s_pin(&core->spi0.gpio, PINCFG_TYPE_DAT, 0);
			fimc_is_spi_s_pin(&core->spi0.gpio, PINCFG_TYPE_FUNC, FUNC_OUTPUT);
		}
		if (core->spi1.device) {
			fimc_is_spi_s_pin(&core->spi1.gpio, PINCFG_TYPE_DAT, 0);
			fimc_is_spi_s_pin(&core->spi1.gpio, PINCFG_TYPE_FUNC, FUNC_OUTPUT);
		}

		pin_config_set(pdata->comp_int_pinctrl, pdata->comp_int_pin,
				PINCFG_PACK(PINCFG_TYPE_DAT, 0));
		pin_config_set(pdata->comp_int_pinctrl, pdata->comp_int_pin,
				PINCFG_PACK(PINCFG_TYPE_FUNC, FUNC_OUTPUT));
	}

	clear_bit(FIMC_IS_COMPANION_OPEN, &device->state);
	clear_bit(FIMC_IS_COMPANION_S_INPUT, &device->state);

p_err:
	minfo("[COM:D] %s(%d)\n", device, __func__, ret);
	return ret;
}

static int fimc_is_companion_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 instance = -1;
	atomic_t device_id;
	struct fimc_is_core *core = NULL;
	struct fimc_is_device_companion *device = NULL;
	struct exynos_platform_fimc_is_companion *pdata = NULL;

	BUG_ON(!pdev);

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed(companion)");
		pdev->dev.init_name = FIMC_IS_COMPANION_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

#ifdef CONFIG_OF
	ret = fimc_is_companion_parse_dt(pdev);
	if (ret) {
		err("parsing device tree is fail(%d)", ret);
		goto p_err;
	}
#endif

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		err("pdata is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	atomic_set(&device_id, 0);
	device = &core->companion;

	memset(&device->v4l2_dev, 0, sizeof(struct v4l2_device));
	instance = v4l2_device_set_name(&device->v4l2_dev, "exynos-fimc-is-companion", &device_id);
	device->instance = instance;
	device->resourcemgr = &core->resourcemgr;
	device->pdev = pdev;
	device->private_data = core;
	device->regs = core->regs;
	device->pdata = pdata;
	platform_set_drvdata(pdev, device);
	device_init_wakeup(&pdev->dev, true);
	device->retention_data.firmware_size = 0;
	memset(&device->retention_data.firmware_crc32, 0, FIMC_IS_COMPANION_CRC_SIZE);

#ifdef CONFIG_COMPANION_STANDBY_USE
	SET_SENSOR_STATE(pdata->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_OFF);
	info("%s: COMPANION STATE %u\n", __func__, GET_SENSOR_STATE(pdata->standby_state, SENSOR_STATE_COMPANION));
#endif

	/* init state */
	clear_bit(FIMC_IS_COMPANION_OPEN, &device->state);
	clear_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_S_INPUT, &device->state);

	ret = v4l2_device_register(&pdev->dev, &device->v4l2_dev);
	if (ret) {
		err("v4l2_device_register is fail(%d)", ret);
		goto p_err;
	}

	ret = fimc_is_comp_video_probe(device);
	if (ret) {
		err("fimc_is_companion_video_probe is fail(%d)", ret);
		goto p_err;
	}

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

#ifdef CONFIG_COMPANION_DCDC_USE
	/* Init companion dcdc */
	comp_pmic_init(&core->companion_dcdc, NULL);
#endif

	info("[%d][COM:D] %s():%d\n", instance, __func__, ret);
	return ret;
p_err:
	info("[%d][COM:D] %s():%d\n", instance, __func__, ret);
	v4l2_device_unregister(&device->v4l2_dev);
	return ret;
}

static int fimc_is_companion_remove(struct platform_device *pdev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static int fimc_is_companion_suspend(struct device *dev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static int fimc_is_companion_resume(struct device *dev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

int fimc_is_companion_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_device_companion *device;

	info("%s\n", __func__);

	device = (struct fimc_is_device_companion *)platform_get_drvdata(pdev);
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* gpio uninit */
	ret = fimc_is_companion_gpio_off(device);
	if (ret) {
		err("fimc_is_companion_gpio_off is fail(%d)", ret);
		goto p_err;
	}

	/* periperal internal clock off */
	ret = fimc_is_companion_iclk_off(device);
	if (ret) {
		err("fimc_is_companion_iclk_off is fail(%d)", ret);
		goto p_err;
	}

	/* companion clock off */
	ret = fimc_is_companion_mclk_off(device);
	if (ret) {
		err("fimc_is_companion_mclk_off is fail(%d)", ret);
		goto p_err;
	}

	if(device->module) {
		device->module = NULL;
	}
p_err:
	info("[COM:D] %s(%d)\n", __func__, ret);
	return ret;
}

int fimc_is_companion_runtime_resume(struct device *dev)
{
	int ret = 0;

	info("[COM:D] %s(%d)\n", __func__, ret);
	return ret;
}

int fimc_is_companion_s_input(struct fimc_is_device_companion *device,
	u32 input,
	u32 scenario)
{
	int ret = 0;
	int waiting = 0;
	struct fimc_is_core *core;
	/* Workaround for Host to use ISP-SPI. Will be removed later.*/
	struct fimc_is_spi_gpio *spi_gpio0;
	struct fimc_is_spi_gpio *spi_gpio1;
#if defined(CONFIG_OIS_USE)
	struct fimc_is_device_ois *ois_device;
	struct fimc_is_ois_gpio *ois_gpio;
#endif
	struct exynos_platform_fimc_is_companion *pdata;
	struct exynos_platform_fimc_is *core_pdata = NULL;
	struct fimc_is_device_sensor *device_sensor;

	BUG_ON(!device);
	BUG_ON(!device->pdata);
	BUG_ON(input >= SENSOR_NAME_END);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	spi_gpio0 = &core->spi0.gpio;
	spi_gpio1 = &core->spi1.gpio;
	pdata = device->pdata;
	pdata->scenario = scenario;

	ret = fimc_is_search_sensor_module(&core->sensor[pdata->id], input, &device->module);
	if (ret) {
		err("fimc_is_search_sensor_module is fail(%d)", ret);
		goto p_err;
	}

	device_sensor = &core->sensor[pdata->id];
	BUG_ON(!device_sensor);

#if defined(CONFIG_OIS_USE)
	ois_device  = (struct fimc_is_device_ois *)i2c_get_clientdata(core->client1);
	ois_gpio = &ois_device->gpio;
#endif
	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_COMPANION_S_INPUT, &device->state)) {
		err("already s_input");
		ret = -EMFILE;
		goto p_err;
	}

	core->current_position = device->module->position;

	/* Initial SPI pins */
	fimc_is_spi_s_pin(spi_gpio0, PINCFG_TYPE_DAT, 0);
	fimc_is_spi_s_pin(spi_gpio1, PINCFG_TYPE_DAT, 0);

	fimc_is_spi_s_pin(spi_gpio0, PINCFG_TYPE_FUNC, FUNC_OUTPUT);
	fimc_is_spi_s_pin(spi_gpio1, PINCFG_TYPE_FUNC, FUNC_OUTPUT);

	ret = fimc_is_companion_mclk_on(device);
	if (ret) {
		err("fimc_is_companion_mclk_on is fail(%d)", ret);
		goto p_err;
	}

	/* gpio init */
	ret = fimc_is_companion_gpio_on(device);
	if (ret) {
		err("fimc_is_companion_gpio_on is fail(%d)", ret);
		goto p_err;
	}

	/* periperal internal clock on */
	ret = fimc_is_companion_iclk_on(device);
	if (ret) {
		err("fimc_is_companion_iclk_on is fail(%d)", ret);
		goto p_err;
	}

	/* set pin output for Host to use SPI*/
	pin_config_set(spi_gpio0->pinname, spi_gpio0->ssn,
		PINCFG_PACK(PINCFG_TYPE_FUNC, FUNC_OUTPUT));
	fimc_is_spi_s_port(spi_gpio0, FIMC_IS_SPI_FUNC, false);

	ret = fimc_is_sec_run_fw_sel(&device->pdev->dev, SENSOR_POSITION_REAR);
	if (core->current_position == SENSOR_POSITION_REAR) {
		if (ret < 0) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
			goto p_err;
		}
	}

	ret = fimc_is_sec_run_fw_sel(&device->pdev->dev, SENSOR_POSITION_FRONT);
	if (core->current_position == SENSOR_POSITION_FRONT) {
		if (ret < 0) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
			goto p_err;
		}
	}

	ret = fimc_is_sec_concord_fw_sel(core, &device->pdev->dev);

	/* TODO: loading firmware */
	fimc_is_s_int_comb_isp(core, false, INTMR2_INTMCIS22);

	// Workaround for Host to use ISP-SPI. Will be removed later.
	/* set pin output for Host to use SPI*/
	pin_config_set(spi_gpio1->pinname, spi_gpio1->ssn,
		PINCFG_PACK(PINCFG_TYPE_FUNC, FUNC_OUTPUT));
	fimc_is_spi_s_port(spi_gpio1, FIMC_IS_SPI_FUNC, false);

	while (!test_bit(FIMC_IS_SENSOR_GPIO_ON, &device_sensor->state)) {
		err("Power sequence does not on, waiting 1ms...");
		usleep_range(1000, 1000);
		if (waiting++ >= MAX_SENSOR_GPIO_ON_WAITING) {
			err("Power sequence does not on");
			break;
		}
	}

	if (fimc_is_comp_is_valid(core) == 0) {
#ifdef CONFIG_COMPANION_STANDBY_USE
		if (GET_SENSOR_STATE(pdata->standby_state, SENSOR_STATE_COMPANION) == SENSOR_STATE_OFF) {
			ret = fimc_is_comp_loadfirm(core);
		} else {
			ret = fimc_is_comp_retention(core);
			if (ret == -EINVAL) {
				info("companion restart..\n");
				ret = fimc_is_comp_loadfirm(core);
			}
		}
		SET_SENSOR_STATE(pdata->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_ON);
		info("%s: COMPANION STATE %u\n", __func__, GET_SENSOR_STATE(pdata->standby_state, SENSOR_STATE_COMPANION));
#else
		ret = fimc_is_comp_loadfirm(core);
#endif
		if (ret) {
			err("fimc_is_comp_loadfirm() fail");
			goto p_err;
		}

#ifdef CONFIG_COMPANION_DCDC_USE
#ifdef CONFIG_COMPANION_C2_USE
		if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT1)
#endif
		{
			if (core->companion_dcdc.type == DCDC_VENDOR_PMIC) {
				if (!fimc_is_sec_file_exist(FIMC_IS_ISP_CV))
					fimc_is_power_binning(core);
			} else {
				fimc_is_power_binning(core);
			}
		}
#endif /* CONFIG_COMPANION_DCDC_USE*/

		ret = fimc_is_comp_loadsetf(core);
		if (ret) {
			err("fimc_is_comp_loadsetf() fail");
			goto p_err;
		}
	}

	// Workaround for Host to use ISP-SPI. Will be removed later.
	/* Set SPI pins to low before changing pin function */
	fimc_is_spi_s_pin(spi_gpio0, PINCFG_TYPE_DAT, 0);
	fimc_is_spi_s_pin(spi_gpio1, PINCFG_TYPE_DAT, 0);

	/* Set pin function for A5 to use SPI */
	pin_config_set(spi_gpio0->pinname, spi_gpio0->ssn,
		PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
	pin_config_set(spi_gpio1->pinname, spi_gpio1->ssn,
		PINCFG_PACK(PINCFG_TYPE_FUNC, 2));

	/* Set pin function to ISP I2C for A5 to use I2C0 */
	pin_config_set("13470000.pinctrl", "gpc2-0",
		PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
	pin_config_set("13470000.pinctrl", "gpc2-1",
		PINCFG_PACK(PINCFG_TYPE_FUNC, 2));

	set_bit(FIMC_IS_COMPANION_S_INPUT, &device->state);

#if defined(CONFIG_OIS_USE)
	if(core_pdata->use_ois && core->current_position == SENSOR_POSITION_REAR) {
		if (!core_pdata->use_ois_hsi2c) {
			pin_config_set(ois_gpio->pinname, ois_gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
			pin_config_set(ois_gpio->pinname, ois_gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
		}

		if (!core->ois_ver_read) {
			fimc_is_ois_check_fw(core);
		}

		fimc_is_ois_exif_data(core);

		if (!core_pdata->use_ois_hsi2c) {
			pin_config_set(ois_gpio->pinname, ois_gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
			pin_config_set(ois_gpio->pinname, ois_gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
		}
	}
#endif

p_err:
	minfo("[COM:D] %s(%d, %d):%d\n", device, __func__, scenario, input, ret);
	return ret;
}

static const struct dev_pm_ops fimc_is_companion_pm_ops = {
	.suspend		= fimc_is_companion_suspend,
	.resume			= fimc_is_companion_resume,
	.runtime_suspend	= fimc_is_companion_runtime_suspend,
	.runtime_resume		= fimc_is_companion_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_companion_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-companion",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_companion_match);

static struct platform_driver fimc_is_companion_driver = {
	.probe = fimc_is_companion_probe,
	.remove = fimc_is_companion_remove,
	.driver = {
		.name	= FIMC_IS_COMPANION_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_companion_pm_ops,
		.of_match_table = exynos_fimc_is_companion_match,
	}
};

#else
static struct platform_device_id fimc_is_companion_driver_ids[] = {
	{
		.name		= FIMC_IS_COMPANION_DEV_NAME,
		.driver_data	= 0,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, fimc_is_companion_driver_ids);

static struct platform_driver fimc_is_companion_driver = {
	.probe = fimc_is_companion_probe,
	.remove = __devexit_p(fimc_is_companion_remove),
	.id_table = fimc_is_companion_driver_ids,
	.driver	  = {
		.name	= FIMC_IS_COMPANION_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_companion_pm_ops,
	}
};
#endif

static int __init fimc_is_companion_init(void)
{
	int ret = platform_driver_register(&fimc_is_companion_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}
late_initcall(fimc_is_companion_init);

static void __exit fimc_is_companion_exit(void)
{
	platform_driver_unregister(&fimc_is_companion_driver);
}
module_exit(fimc_is_companion_exit);

MODULE_AUTHOR("Wooki Min<wooki.min@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS_COMPANION driver");
MODULE_LICENSE("GPL");
