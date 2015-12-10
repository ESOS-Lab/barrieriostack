/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
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

#include <mach/map.h>
#include <mach/devfreq.h>
#include <mach/regs-clock.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-hw.h"
#include "fimc-is-video.h"
#include "fimc-is-dt.h"

#include "fimc-is-device-sensor.h"
#ifdef CONFIG_USE_VENDER_FEATURE
#include "fimc-is-sec-define.h"
#endif

#ifdef USE_SPI
#include "fimc-is-spi.h"
#include <linux/pinctrl/pinctrl-samsung.h>
#endif

extern struct device *camera_front_dev;
extern struct device *camera_rear_dev;

extern int fimc_is_ssx_video_probe(void *data);
struct pm_qos_request exynos_sensor_qos_int;
struct pm_qos_request exynos_sensor_qos_cam;
struct pm_qos_request exynos_sensor_qos_mem;

int fimc_is_sensor_runtime_suspend(struct device *dev);
int fimc_is_sensor_runtime_resume(struct device *dev);

extern u32 __iomem *notify_fcount_sen0;
extern u32 __iomem *notify_fcount_sen1;
extern u32 __iomem *notify_fcount_sen2;
u32 notify_fcount_sen0_fw;
u32 notify_fcount_sen1_fw;
u32 notify_fcount_sen2_fw;
u32 notify_fcount_dummy;

static int fimc_is_sensor_back_stop(void *qdevice,
	struct fimc_is_queue *queue);

#define BINNING(x, y) roundup((x) * 1000 / (y), 250)

int fimc_is_sensor_read8(struct i2c_client *client,
	u16 addr, u8 *val)
{
	int ret = 0;
	struct i2c_msg msg[2];
	u8 wbuf[2];

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 2;
	msg[0].buf = wbuf;
	/* TODO : consider other size of buffer */
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		err("i2c treansfer fail");
		goto p_err;
	}

#ifdef PRINT_I2CCMD
	info("I2CR08(%d) [0x%04X] : 0x%04X\n", client->addr, addr, *val);
#endif

p_err:
	return ret;
}

int fimc_is_sensor_read16(struct i2c_client *client,
	u16 addr, u16 *val)
{
	int ret = 0;
	struct i2c_msg msg[2];
	u8 wbuf[2], rbuf[2];

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 2;
	msg[0].buf = wbuf;
	/* TODO : consider other size of buffer */
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rbuf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		err("i2c treansfer fail");
		goto p_err;
	}

	*val = ((rbuf[0] << 8) | rbuf[1]);

#ifdef PRINT_I2CCMD
	info("I2CR16(%d) [0x%04X] : 0x%04X\n", client->addr, addr, *val);
#endif

p_err:
	return ret;
}

int fimc_is_sensor_write(struct i2c_client *client,
	u8 *buf, u32 size)
{
	int ret = 0;
	int retry_count = 5;
	struct i2c_msg msg = {client->addr, 0, size, buf};

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		msleep(10);
	} while (retry_count-- > 0);

	if (ret != 1) {
		dev_err(&client->dev, "%s: I2C is not working.\n", __func__);
		return -EIO;
	}

	return 0;
}

int fimc_is_sensor_write8(struct i2c_client *client,
	u16 addr, u8 val)
{
	int ret = 0;
	struct i2c_msg msg[1];
	u8 wbuf[3];

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 3;
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	wbuf[2] = val;

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		err("i2c treansfer fail(%d)", ret);
		goto p_err;
	}

#ifdef PRINT_I2CCMD
	info("I2CW08(%d) [0x%04X] : 0x%04X\n", client->addr, addr, val);
#endif

p_err:
	return ret;
}

int fimc_is_sensor_write16(struct i2c_client *client,
	u16 addr, u16 val)
{
	int ret = 0;
	struct i2c_msg msg[1];
	u8 wbuf[4];

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 4;
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	wbuf[2] = (val & 0xFF00) >> 8;
	wbuf[3] = (val & 0xFF);

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		err("i2c treansfer fail(%d)", ret);
		goto p_err;
	}

#ifdef PRINT_I2CCMD
	info("I2CW16(%d) [0x%04X] : 0x%04X\n", client->addr, addr, val);
#endif

p_err:
	return ret;
}

int fimc_is_search_sensor_module(struct fimc_is_device_sensor *device,
	u32 sensor_id, struct fimc_is_module_enum **module)
{
	int ret = 0;
	u32 mindex, mmax;
	struct fimc_is_module_enum *module_enum;
	struct fimc_is_resourcemgr *resourcemgr;

	resourcemgr = device->resourcemgr;
	module_enum = device->module_enum;
	*module = NULL;

	mmax = atomic_read(&resourcemgr->rsccount_module);
	for (mindex = 0; mindex < mmax; mindex++) {
		if (module_enum[mindex].sensor_id == sensor_id) {
			*module = &module_enum[mindex];
			break;
		}
	}

	if (mindex >= mmax) {
		merr("module(%d) is not found", device, sensor_id);
		ret = -EINVAL;
	}

	return ret;
}

static int get_sensor_mode(struct fimc_is_sensor_cfg *cfg,
	u32 cfgs, u32 width, u32 height, u32 framerate)
{
	int mode = -1;
	int idx = -1;
	u32 i;

	/* find sensor mode by w/h and fps range */
	for (i = 0; i < cfgs; i++) {
		if ((cfg[i].width == width) &&
				(cfg[i].height == height)) {
			if (cfg[i].framerate == framerate) {
				/* You don't need to find another sensor mode */
				mode = cfg[i].mode;
				idx = i;
				break;
			} else if (cfg[i].framerate > framerate) {
				/* try to find framerate smaller than previous */
				if (mode < 0) {
					mode = cfg[i].mode;
					idx = i;
				} else {
					/* try to compare previous with current */
					if (cfg[idx].framerate > cfg[i].framerate) {
						mode = cfg[i].mode;
						idx = i;
					}
				}
			}
		}
	}

	if (idx < 0)
		err("could not find proper sensor mode: %dx%d@%dfps",
			width, height, framerate);
	else
		info("sensor mode(%dx%d@%d) = %d\n",
			cfg[idx].width,
			cfg[idx].height,
			cfg[idx].framerate,
			cfg[idx].mode);

	return mode;
}

static int fimc_is_sensor_mclk_on(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_device_companion *device_companion;
	struct fimc_is_core *core;
	struct fimc_is_module_enum *module_companion = NULL;

	core = device->private_data;
	device_companion = &core->companion;
	module_companion = device_companion->module;

	BUG_ON(!device_companion);
#endif
	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (test_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state)) {
		merr("%s : already clk on", device, __func__);
		goto p_err;
	}

#ifdef CONFIG_COMPANION_USE
	if (module_companion != NULL && module_companion->position == device->position) {
		if (test_bit(FIMC_IS_COMPANION_MCLK_ON, &device_companion->state)) {
			minfo("%s : sensor mclk will on with companion mclk.\n", device, __func__);
			set_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);
			goto p_err;
		}
	}
#endif

	if (!pdata->mclk_on) {
		merr("mclk_on is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	module_pdata = module->pdata;

	ret = pdata->mclk_on(device->pdev, pdata->scenario, module_pdata->mclk_ch);
	if (ret) {
		merr("mclk_on is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);

p_err:
	return ret;
}

int fimc_is_sensor_mclk_off(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_device_companion *device_companion;
	struct fimc_is_core *core;
	struct fimc_is_module_enum *module_companion = NULL;

	core = device->private_data;
	device_companion = &core->companion;
	module_companion = device_companion->module;

	BUG_ON(!device_companion);
#endif
	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state)) {
		merr("%s : already clk off", device, __func__);
		goto p_err;
	}

#ifdef CONFIG_COMPANION_USE
	if (module_companion != NULL && module_companion->position == device->position) {
		if (test_bit(FIMC_IS_COMPANION_MCLK_ON, &device_companion->state)) {
			info("%s : companion clk is on. sensor mclk will off with companion mclk\n", __func__);
			goto p_err;
		}
	}
#endif
	if (!pdata->mclk_off) {
		merr("mclk_off is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	module_pdata = module->pdata;

	ret = pdata->mclk_off(device->pdev, pdata->scenario, module_pdata->mclk_ch);
	if (ret) {
		merr("mclk_off is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_sensor_iclk_on(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_core *core;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);
	BUG_ON(!device->private_data);

	core = device->private_data;
	pdata = device->pdata;

	if (test_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state)) {
		merr("%s : already clk on", device, __func__);
		goto p_err;
	}

	if (!pdata->iclk_cfg) {
		merr("iclk_cfg is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->iclk_on) {
		merr("iclk_on is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_cfg(core->pdev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_cfg is fail(%d)", device, ret);
		goto p_err;
	}

	ret = pdata->iclk_on(core->pdev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_on is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state);

p_err:
	return ret;
}

int fimc_is_sensor_iclk_off(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_core *core;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);
	BUG_ON(!device->private_data);

	core = device->private_data;
	pdata = device->pdata;

	if (!test_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state)) {
		merr("%s : already clk off", device, __func__);
		goto p_err;
	}

	if (!pdata->iclk_off) {
		merr("iclk_off is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_off(core->pdev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_off is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state);

p_err:
	return ret;
}

int fimc_is_sensor_gpio_on(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_core *core;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_device_companion *device_companion;
	struct fimc_is_module_enum *module_companion = NULL;

	core = device->private_data;
	device_companion = &core->companion;
	module_companion = device_companion->module;

	BUG_ON(!device_companion);
#endif
	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	if (test_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state)) {
		merr("%s : already gpio on", device, __func__);
		goto p_err;
	}

#ifdef CONFIG_COMPANION_USE
	if (module_companion != NULL && module_companion->position == device->position) {
		if (test_bit(FIMC_IS_COMPANION_GPIO_ON, &device_companion->state)) {
			info("%s : sensor gpio is already on.\n", __func__);
			set_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);
			goto p_err;
		}
	}
#endif

	ret = fimc_is_sensor_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	if (module->position == SENSOR_POSITION_FRONT) {
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
			merr("gpio_cfg is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		ret = pdata->gpio_cfg(module->pdev, scenario, GPIO_SCENARIO_ON);
		if (ret) {
			clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			merr("gpio_cfg is fail(%d)", device, ret);
			goto p_err;
		}
		set_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);
	}

p_err:
	return ret;
}

int fimc_is_sensor_gpio_off(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_core *core;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_device_companion *device_companion;
	struct fimc_is_module_enum *module_companion = NULL;

	core = device->private_data;
	device_companion = &core->companion;
	module_companion = device_companion->module;

	BUG_ON(!device_companion);
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

	if (!test_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state)) {
		merr("%s : already gpio off", device, __func__);
		goto p_err;
	}

#ifdef CONFIG_COMPANION_USE
	if (module_companion != NULL && module_companion->position == device->position) {
		if (test_bit(FIMC_IS_COMPANION_GPIO_ON, &device_companion->state)) {
			info("%s : companion gpio is on. we will gpio off on device-companion\n", __func__);
			clear_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);
			goto p_err;
		}
	}
#endif

	ret = fimc_is_sensor_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (test_and_clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state)) {
		struct exynos_platform_fimc_is_module *pdata;

		scenario = device->pdata->scenario;
		pdata = module->pdata;
		if (!pdata) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			merr("pdata is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (!pdata->gpio_cfg) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			merr("gpio_cfg is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		ret = pdata->gpio_cfg(module->pdev, scenario, GPIO_SCENARIO_OFF);
		if (ret) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			merr("gpio_cfg is fail(%d)", device, ret);
			goto p_err;
		}
	}

	clear_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);

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

int fimc_is_sensor_gpio_dbg(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_module_enum *module;
	struct exynos_platform_fimc_is_module *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	ret = fimc_is_sensor_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	scenario = device->pdata->scenario;
	pdata = module->pdata;
	if (!pdata) {
		merr("pdata is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->gpio_dbg) {
		merr("gpio_dbg is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_dbg(module->pdev, scenario, GPIO_SCENARIO_ON);
	if (ret) {
		merr("gpio_dbg is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

#ifdef ENABLE_DTP
static void fimc_is_sensor_dtp(unsigned long data)
{
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_sensor *device = (struct fimc_is_device_sensor *)data;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	unsigned long flags;
	u32 i;

	BUG_ON(!device);

	err("forcely reset due to 0x%08lx", device->force_stop);
	device->force_stop = 0;

	set_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state);
	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

	vctx = device->vctx;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	framemgr = GET_FRAMEMGR(vctx);
	if ((framemgr->frame_cnt == 0) || (framemgr->frame_cnt > FRAMEMGR_MAX_REQUEST)) {
		err("frame count of framemgr is invalid(%d)", framemgr->frame_cnt);
		return;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_1, flags);

	for (i = 0; i < framemgr->frame_cnt; i++) {
		frame = &framemgr->frame[i];
		if (frame->state == FIMC_IS_FRAME_STATE_REQUEST) {
			err("buffer done1!!!! %d", i);
			fimc_is_frame_trans_req_to_com(framemgr, frame);
			buffer_done(vctx, i, VB2_BUF_STATE_ERROR);
		} else if (frame->state == FIMC_IS_FRAME_STATE_PROCESS) {
			err("buffer done1!!!! %d", i);
			fimc_is_frame_trans_pro_to_com(framemgr, frame);
			buffer_done(vctx, i, VB2_BUF_STATE_ERROR);
		}
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_1, flags);
}
#endif

static int fimc_is_sensor_start(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	BUG_ON(!device);

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		struct v4l2_subdev *subdev;

		subdev = device->subdev_module;
		if (!subdev) {
			merr("subdev is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}
		ret = v4l2_subdev_call(subdev, video, s_stream, true);
		if (ret) {
			merr("v4l2_subdev_call(s_stream) is fail(%d)", device, ret);
			goto p_err;
		}
	} else {
		struct fimc_is_device_ischain *ischain;

		ischain = device->ischain;
		if (!ischain) {
			merr("ischain is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		ret = fimc_is_itf_stream_on(ischain);
		if (ret) {
			merr("fimc_is_itf_stream_on is fail(%d)", device, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int fimc_is_sensor_stop(struct fimc_is_device_sensor *device)
{
	int ret = 0;

	BUG_ON(!device);

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		struct v4l2_subdev *subdev;

		subdev = device->subdev_module;
		if (!subdev) {
			merr("subdev is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}
		ret = v4l2_subdev_call(subdev, video, s_stream, false);
		if (ret) {
			merr("v4l2_subdev_call(s_stream) is fail(%d)", device, ret);
			goto p_err;
		}
	} else {
		struct fimc_is_device_ischain *ischain;

		ischain = device->ischain;
		if (!ischain) {
			merr("ischain is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}
		ret = fimc_is_itf_stream_off(ischain);
		if (ret) {
			merr("fimc_is_itf_stream_off is fail(%d)", device, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

int fimc_is_sensor_tag(struct fimc_is_device_sensor *device,
	struct fimc_is_frame *frame)
{
	int ret = 0;
	u32 hashkey;

	BUG_ON(!device);
	BUG_ON(!frame);

	hashkey = frame->fcount % FIMC_IS_TIMESTAMP_HASH_KEY;
	frame->shot->dm.request.frameCount = frame->fcount;
	frame->shot->dm.sensor.timeStamp = device->timestamp[hashkey];
	frame->shot->udm.sensor.timeStampBoot = device->timestampboot[hashkey];
#ifdef DBG_JITTER
	fimc_is_jitter(frame->shot->dm.sensor.timeStamp);
#endif

	return ret;
}

static void fimc_is_sensor_control(struct work_struct *data)
{
/*
 * HAL can't send meta data for vision
 * We accepted vision control by s_ctrl
 */
#if 0
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct camera2_sensor_ctl *rsensor_ctl;
	struct camera2_sensor_ctl *csensor_ctl;
	struct fimc_is_device_sensor *device;

	device = container_of(data, struct fimc_is_device_sensor, control_work);
	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		return;
	}

	module = v4l2_get_subdevdata(subdev_module);
	rsensor_ctl = &device->control_frame->shot->ctl.sensor;
	csensor_ctl = &device->sensor_ctl;

	if (rsensor_ctl->exposureTime != csensor_ctl->exposureTime) {
		CALL_MOPS(module, s_exposure, subdev_module, rsensor_ctl->exposureTime);
		csensor_ctl->exposureTime = rsensor_ctl->exposureTime;
	}

	if (rsensor_ctl->frameDuration != csensor_ctl->frameDuration) {
		CALL_MOPS(module, s_duration, subdev_module, rsensor_ctl->frameDuration);
		csensor_ctl->frameDuration = rsensor_ctl->frameDuration;
	}

	if (rsensor_ctl->sensitivity != csensor_ctl->sensitivity) {
		CALL_MOPS(module, s_again, subdev_module, rsensor_ctl->sensitivity);
		csensor_ctl->sensitivity = rsensor_ctl->sensitivity;
	}
#endif
}

static int fimc_is_sensor_notify_by_fstr(struct fimc_is_device_sensor *device, void *arg)
{
	int ret = 0;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	u32 hashkey;

	BUG_ON(!device);
	BUG_ON(!arg);

	device->fcount = *(u32 *)arg;
	framemgr = GET_FRAMEMGR(device->vctx);

	if (device->instant_cnt) {
		device->instant_cnt--;
		if (device->instant_cnt == 0)
			wake_up(&device->instant_wait);
	}

	hashkey = device->fcount % FIMC_IS_TIMESTAMP_HASH_KEY;
	device->timestamp[hashkey] = fimc_is_get_timestamp();
	device->timestampboot[hashkey] = fimc_is_get_timestamp_boot();

	framemgr_e_barrier(framemgr, FMGR_IDX_28);

	fimc_is_frame_process_head(framemgr, &frame);
	if (frame) {
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
		do_gettimeofday(&frame->tzone[TM_FLITE_STR]);
#endif
#endif

		frame->fcount = device->fcount;
		fimc_is_sensor_tag(device, frame);
	}

#ifdef TASKLET_MSG
	if (!frame) {
		merr("[SEN] process is empty", device);
		fimc_is_frame_print_all(framemgr);
	}
#endif

	framemgr_x_barrier(framemgr, FMGR_IDX_28);

	return ret;
}

static int fimc_is_sensor_notify_by_fend(struct fimc_is_device_sensor *device, void *arg)
{
	int ret = 0;
	struct fimc_is_frame *frame;

	BUG_ON(!device);
	BUG_ON(!device->vctx);

#ifdef ENABLE_DTP
	if (device->dtp_check) {
		device->dtp_check = false;
		del_timer(&device->dtp_timer);
	}

	if (device->force_stop)
		fimc_is_sensor_dtp((unsigned long)device);
#endif

	frame = (struct fimc_is_frame *)arg;
	if (frame) {
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
		do_gettimeofday(&frame->tzone[TM_FLITE_END]);
#endif
#endif

		buffer_done(device->vctx, frame->index, VB2_BUF_STATE_DONE);

		/* device driving */
		if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
			device->control_frame = frame;
			schedule_work(&device->control_work);
		}
	}

	return ret;
}

static void fimc_is_sensor_notify(struct v4l2_subdev *subdev,
	unsigned int notification,
	void *arg)
{
	int ret = 0;
	struct fimc_is_device_sensor *device;

	BUG_ON(!subdev);

	device = v4l2_get_subdev_hostdata(subdev);

	switch(notification) {
	case FLITE_NOTIFY_FSTART:
		ret = fimc_is_sensor_notify_by_fstr(device, arg);
		if (ret)
			merr("fimc_is_sensor_notify_by_fstr is fail(%d)", device, ret);
		break;
	case FLITE_NOTIFY_FEND:
		ret = fimc_is_sensor_notify_by_fend(device, arg);
		if (ret)
			merr("fimc_is_sensor_notify_by_fend is fail(%d)", device, ret);
		break;
	}
}

static void fimc_is_sensor_instanton(struct work_struct *data)
{
	int ret = 0;
	u32 instant_cnt;
	struct fimc_is_device_sensor *device;

	BUG_ON(!data);

	device = container_of(data, struct fimc_is_device_sensor, instant_work);
	instant_cnt = device->instant_cnt;

	clear_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

	ret = fimc_is_sensor_start(device);
	if (ret) {
		merr("fimc_is_sensor_start is fail(%d)\n", device, ret);
		goto p_err;
	}
	set_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);

#ifdef ENABLE_DTP
	if (device->dtp_check) {
		setup_timer(&device->dtp_timer, fimc_is_sensor_dtp, (unsigned long)device);
		mod_timer(&device->dtp_timer, jiffies +  msecs_to_jiffies(300));
		info("DTP checking...\n");
	}
#endif

	if (instant_cnt) {
		ulong timetowait, timetoelapse, timeout;

		timeout = FIMC_IS_FLITE_STOP_TIMEOUT + msecs_to_jiffies(instant_cnt*60);
		timetowait = wait_event_timeout(device->instant_wait,
			(device->instant_cnt == 0),
			timeout);
		if (!timetowait) {
			merr("wait_event_timeout is invalid", device);
			ret = -ETIME;
		}

		fimc_is_sensor_front_stop(device);

		timetoelapse = (jiffies_to_msecs(timeout) - jiffies_to_msecs(timetowait));
		info("[FRT:D:%d] instant off(fcount : %d, time : %ldms)\n", device->instance,
			device->instant_cnt,
			timetoelapse);
	}

p_err:
	device->instant_ret = ret;
}

static int fimc_is_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 instance = -1;
	atomic_t device_id;
	struct fimc_is_core *core = NULL;
	struct fimc_is_device_sensor *device = NULL;
	void *pdata;

	BUG_ON(!pdev);

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed(sensor)");
		pdev->dev.init_name = FIMC_IS_SENSOR_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

#ifdef CONFIG_OF
	ret = fimc_is_sensor_parse_dt(pdev);
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

	/* 1. get device */
	atomic_set(&device_id, pdev->id);
	device = &core->sensor[pdev->id];

	/* v4l2 device and device init */
	memset(&device->v4l2_dev, 0, sizeof(struct v4l2_device));
	instance = v4l2_device_set_name(&device->v4l2_dev, FIMC_IS_SENSOR_DEV_NAME, &device_id);
	device->v4l2_dev.notify = fimc_is_sensor_notify;
	device->instance = instance;
	device->position = SENSOR_POSITION_REAR;
	device->resourcemgr = &core->resourcemgr;
	device->pdev = pdev;
	device->private_data = core;
	device->pdata = pdata;
	platform_set_drvdata(pdev, device);
	init_waitqueue_head(&device->instant_wait);
	INIT_WORK(&device->instant_work, fimc_is_sensor_instanton);
	INIT_WORK(&device->control_work, fimc_is_sensor_control);
	spin_lock_init(&device->slock_state);
	device_init_wakeup(&pdev->dev, true);

	/* 3. state init*/
	clear_bit(FIMC_IS_SENSOR_OPEN, &device->state);
	clear_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_POWER_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_S_INPUT, &device->state);
	clear_bit(FIMC_IS_SENSOR_DRIVING, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

#ifdef ENABLE_DTP
	device->dtp_check = false;
#endif

	ret = fimc_is_mem_probe(&device->mem, device->pdev);
	if (ret) {
		merr("fimc_is_mem_probe is fail(%d)", device, ret);
		goto p_err;
	}

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

	ret = v4l2_device_register(&pdev->dev, &device->v4l2_dev);
	if (ret) {
		merr("v4l2_device_register is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_csi_probe(device, device->pdata->csi_ch);
	if (ret) {
		merr("fimc_is_csi_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_flite_probe(device, device->pdata->flite_ch);
	if (ret) {
		merr("fimc_is_flite_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ssx_video_probe(device);
	if (ret) {
		merr("fimc_is_sensor_video_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = device->pdata->iclk_get(core->pdev, device->pdata->scenario, device->pdata->csi_ch);
	if (ret) {
		merr("iclk_probe is fail(%d)", device, ret);
		goto p_err;
	}

	device->is_probed = true;
	info("[%d][SEN:D] %s(%d)\n", instance, __func__, ret);
	return ret;
p_err:
	info("[%d][SEN:D] %s(%d)\n", instance, __func__, ret);
	v4l2_device_unregister(&device->v4l2_dev);
	return ret;
}

static int fimc_is_sensor_remove(struct platform_device *pdev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

int fimc_is_sensor_open(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	int retry = 4;

	BUG_ON(!device);
	BUG_ON(!device->subdev_csi);

	if (test_bit(FIMC_IS_SENSOR_OPEN, &device->state)) {
		merr("already open", device);
		ret = -EMFILE;
		goto p_err;
	}

	/* runtime suspend callback can be called lately because of power relationship */
	while (test_bit(FIMC_IS_SENSOR_POWER_ON, &device->state) && (retry > 0)) {
		warn("sensor is not yet power off");
		msleep(500);
		--retry;
	}
	if (!retry) {
		ret = -EBUSY;
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_S_INPUT, &device->state);
	clear_bit(FIMC_IS_SENSOR_DRIVING, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state);
	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

	device->vctx = vctx;
	device->fcount = 0;
	device->instant_cnt = 0;
	device->instant_ret = 0;
	device->ischain = NULL;
	device->subdev_module = NULL;
	device->exposure_time = 0;
	device->frame_duration = 0;
	device->force_stop = 0;
	device->fade_in_mode = 0;
	memset(&device->sensor_ctl, 0, sizeof(struct camera2_sensor_ctl));
	memset(&device->lens_ctl, 0, sizeof(struct camera2_lens_ctl));
	memset(&device->flash_ctl, 0, sizeof(struct camera2_flash_ctl));

	ret = fimc_is_csi_open(device->subdev_csi);
	if (ret) {
		merr("fimc_is_csi_open is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_flite_open(device->subdev_flite, GET_FRAMEMGR(vctx));
	if (ret) {
		merr("fimc_is_flite_open is fail(%d)", device, ret);
		goto p_err;
	}

	/* for mediaserver force close */
	ret = fimc_is_resource_get(device->resourcemgr, device->instance);
	if (ret) {
		merr("fimc_is_resource_get is fail", device);
		goto p_err;
	}

#ifdef ENABLE_DTP
	device->dtp_check = true;
#endif

	set_bit(FIMC_IS_SENSOR_OPEN, &device->state);

p_err:
	minfo("[SEN:D] %s():%d\n", device, __func__, ret);
	return ret;
}

int fimc_is_sensor_close(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_group *group_3aa;
#ifdef USE_SPI
	struct fimc_is_spi_gpio *spi_gpio0;
	struct fimc_is_core *core;
#endif

	BUG_ON(!device);
	BUG_ON(!device->vctx);

	if (!test_bit(FIMC_IS_SENSOR_OPEN, &device->state)) {
		merr("already close", device);
		ret = -EMFILE;
		goto p_err;
	}

#ifdef USE_SPI
	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	spi_gpio0 = &core->spi0.gpio;

	fimc_is_spi_s_pin(spi_gpio0, PINCFG_TYPE_FUNC, 0);
#endif

	/* for mediaserver force close */
	ischain = device->ischain;
	if (ischain) {
		group_3aa = &ischain->group_3aa;
		if (test_bit(FIMC_IS_GROUP_START, &group_3aa->state)) {
			info("media server is dead, 3ax forcely done\n");
			set_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group_3aa->state);
		}
	}

	ret = fimc_is_sensor_back_stop(device, GET_QUEUE(device->vctx));
	if (ret)
		merr("fimc_is_sensor_back_stop is fail(%d)", device, ret);

	ret = fimc_is_sensor_front_stop(device);
	if (ret)
		merr("fimc_is_sensor_front_stop is fail(%d)", device, ret);

	ret = fimc_is_csi_close(device->subdev_csi);
	if (ret)
		merr("fimc_is_flite_close is fail(%d)", device, ret);

	ret = fimc_is_flite_close(device->subdev_flite);
	if (ret)
		merr("fimc_is_flite_close is fail(%d)", device, ret);

	/* cancel a work and wait for it to finish */
	cancel_work_sync(&device->control_work);
	cancel_work_sync(&device->instant_work);

	/* for mediaserver force close */
	ret = fimc_is_resource_put(device->resourcemgr, device->instance);
	if (ret)
		merr("fimc_is_resource_put is fail", device);

	clear_bit(FIMC_IS_SENSOR_OPEN, &device->state);
	clear_bit(FIMC_IS_SENSOR_S_INPUT, &device->state);
	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

p_err:
	minfo("[SEN:D] %s():%d\n", device, __func__, ret);
	return ret;
}

int fimc_is_sensor_s_input(struct fimc_is_device_sensor *device,
	u32 input,
	u32 scenario)
{
	int ret = 0;
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct v4l2_subdev *subdev_flite;
	struct fimc_is_module_enum *module_enum, *module;
	u32 sensor_index;

	BUG_ON(!device);
	BUG_ON(!device->pdata);
	BUG_ON(!device->subdev_csi);
	BUG_ON(input >= SENSOR_NAME_END);

	if (test_bit(FIMC_IS_SENSOR_S_INPUT, &device->state)) {
		merr("already s_input", device);
		ret = -EINVAL;
		goto p_err;
	}

	module = NULL;
	module_enum = device->module_enum;

	for (sensor_index = 0; sensor_index < SENSOR_MAX_ENUM; sensor_index++) {
		if (module_enum[sensor_index].sensor_id == input) {
			module = &module_enum[sensor_index];

			/*
			 * If it is not normal scenario,
			 * try to find proper sensor module which has a i2c client
			 */
			if (scenario != SENSOR_SCENARIO_NORMAL && module->client == NULL)
				continue;
			else
				break;
		}
	}

	if (sensor_index >= SENSOR_MAX_ENUM) {
		merr("module is not probed", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev_module = module->subdev;
	if (!subdev_module) {
		merr("module is not probed", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev_csi = device->subdev_csi;
	subdev_flite = device->subdev_flite;
	device->position = module->position;
	device->image.framerate = min_t(u32, SENSOR_DEFAULT_FRAMERATE, module->max_framerate);
	device->image.window.width = module->pixel_width;
	device->image.window.height = module->pixel_height;
	device->image.window.o_width = device->image.window.width;
	device->image.window.o_height = device->image.window.height;

	core->current_position = module->position;

	/* send csi chennel to FW */
	module->ext.sensor_con.csi_ch = device->pdata->csi_ch;
	module->ext.sensor_con.csi_ch |= 0x0100;

	if (scenario) {
		device->pdata->scenario = scenario;
		set_bit(FIMC_IS_SENSOR_DRIVING, &device->state);
	} else {
		device->pdata->scenario = SENSOR_SCENARIO_NORMAL;
		clear_bit(FIMC_IS_SENSOR_DRIVING, &device->state);
	}

	if (device->subdev_module) {
		mwarn("subdev_module is already registered", device);
		v4l2_device_unregister_subdev(device->subdev_module);
	}

	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_module);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto p_err;
	}

	device->subdev_module = subdev_module;

#if defined(CONFIG_PM_DEVFREQ)
	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		pm_qos_add_request(&exynos_sensor_qos_cam, PM_QOS_DEVICE_THROUGHPUT, 500000);
		pm_qos_add_request(&exynos_sensor_qos_mem, PM_QOS_BUS_THROUGHPUT, 632000);
	}
#endif
	ret = fimc_is_sensor_mclk_on(device);
	if (ret) {
		merr("fimc_is_sensor_mclk_on is fail(%d)", device, ret);
		goto p_err;
	}

	/* configuration clock control */
	ret = fimc_is_sensor_iclk_on(device);
	if (ret) {
		merr("fimc_is_sensor_iclk_on is fail(%d)", device, ret);
		goto p_err;
	}

	/* Sensor power on */
	ret = fimc_is_sensor_gpio_on(device);
	if (ret) {
		merr("fimc_is_sensor_gpio_on is fail(%d)", device, ret);
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_flite, core, init, device->pdata->csi_ch);
	if (ret) {
		merr("v4l2_flite_call(init) is fail(%d)", device, ret);
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, core, init, sensor_index);
	if (ret) {
		merr("v4l2_csi_call(init) is fail(%d)", device, ret);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		ret = v4l2_subdev_call(subdev_module, core, init, 0);
		if (ret) {
			merr("v4l2_module_call(init) is fail(%d)", device, ret);
			goto p_err;
		}
	}

	set_bit(FIMC_IS_SENSOR_S_INPUT, &device->state);

p_err:
	minfo("[SEN:D] %s(%d, %d):%d\n", device, __func__, input, scenario, ret);
	return ret;
}

static int fimc_is_sensor_s_format(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_sensor *device = qdevice;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct v4l2_subdev *subdev_flite;
	struct fimc_is_module_enum *module;
	struct v4l2_mbus_framefmt subdev_format;
	struct fimc_is_fmt *format;
	u32 width;
	u32 height;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!device->subdev_flite);
	BUG_ON(!device->subdev_module);
	BUG_ON(!queue);

	subdev_module = device->subdev_module;
	subdev_csi = device->subdev_csi;
	subdev_flite = device->subdev_flite;
	format = &queue->framecfg.format;
	width = queue->framecfg.width;
	height = queue->framecfg.height;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	device->image.format = *format;
	device->image.window.offs_h = 0;
	device->image.window.offs_v = 0;
	device->image.window.width = width;
	device->image.window.o_width = width;
	device->image.window.height = height;
	device->image.window.o_height = height;

	subdev_format.code = format->pixelformat;
	subdev_format.field = format->field;
	subdev_format.width = width;
	subdev_format.height = height;

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		ret = v4l2_subdev_call(subdev_module, video, s_mbus_fmt, &subdev_format);
		if (ret) {
			merr("v4l2_module_call(s_format) is fail(%d)", device, ret);
			goto p_err;
		}
	}

	ret = v4l2_subdev_call(subdev_csi, video, s_mbus_fmt, &subdev_format);
	if (ret) {
		merr("v4l2_csi_call(s_format) is fail(%d)", device, ret);
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_flite, video, s_mbus_fmt, &subdev_format);
	if (ret) {
		merr("v4l2_flite_call(s_format) is fail(%d)", device, ret);
		goto p_err;
	}

	/* if sensor is driving mode, skip finding sensor mode */
	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		device->mode = 0;
	} else {
		device->mode = get_sensor_mode(module->cfg, module->cfgs,
				device->image.window.width, device->image.window.height,
				device->image.framerate);
		if (device->mode < 0) {
			ret = -EINVAL;
			goto p_err;
		}
	}

p_err:
	return ret;
}

int fimc_is_sensor_s_framerate(struct fimc_is_device_sensor *device,
	struct v4l2_streamparm *param)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct fimc_is_module_enum *module;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;
	u32 framerate = 0;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!param);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	if (!tpf->numerator) {
		merr("numerator is 0", device);
		ret = -EINVAL;
		goto p_err;
	}

	framerate = tpf->denominator / tpf->numerator;
	subdev_module = device->subdev_module;
	subdev_csi = device->subdev_csi;

	if (framerate == 0) {
		mwarn("frame rate 0 request is ignored", device);
		goto p_err;
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
		merr("front is already stream on", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		merr("type is invalid(%d)", device, param->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (framerate > module->max_framerate) {
		merr("framerate is invalid(%d > %d)", device, framerate, module->max_framerate);
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, video, s_parm, param);
	if (ret) {
		merr("v4l2_csi_call(s_param) is fail(%d)", device, ret);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		ret = v4l2_subdev_call(subdev_module, video, s_parm, param);
		if (ret) {
			merr("v4l2_module_call(s_param) is fail(%d)", device, ret);
			goto p_err;
		}
	}

	device->image.framerate = framerate;
	device->max_target_fps = framerate;

	device->mode = get_sensor_mode(module->cfg, module->cfgs,
			device->image.window.width, device->image.window.height,
			framerate);

	info("[SEN:D:%d] framerate: req@%dfps, cur@%dfps\n", device->instance,
		framerate, device->image.framerate);

p_err:
	return ret;
}

int fimc_is_sensor_s_ctrl(struct fimc_is_device_sensor *device,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!ctrl);

	subdev_module = device->subdev_module;

	ret = v4l2_subdev_call(subdev_module, core, s_ctrl, ctrl);
	if (ret) {
		err("s_ctrl is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sensor_s_bns(struct fimc_is_device_sensor *device,
	u32 ratio)
{
	int ret = 0;
	struct v4l2_subdev *subdev_flite;
	u32 sensor_width, sensor_height;

	BUG_ON(!device);
	BUG_ON(!device->subdev_flite);

	subdev_flite = device->subdev_flite;

	sensor_width = fimc_is_sensor_g_width(device);
	sensor_height = fimc_is_sensor_g_height(device);
	if (!sensor_width || !sensor_height) {
		merr("Sensor size is zero. Sensor set_format first.\n", device);
		ret = -EINVAL;
		goto p_err;
	}

	device->image.window.otf_width
		= rounddown((sensor_width * 1000 / ratio), 4);
	device->image.window.otf_height
		= rounddown((sensor_height * 1000 / ratio), 2);

p_err:
	return ret;
}

int fimc_is_sensor_s_frame_duration(struct fimc_is_device_sensor *device,
	u32 framerate)
{
	int ret = 0;
	u64 frame_duration;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* unit : nano */
	frame_duration = (1000 * 1000 * 1000) / framerate;
	if (frame_duration <= 0) {
		err("it is wrong frame duration(%lld)", frame_duration);
		ret = -EINVAL;
		goto p_err;
	}

	if (device->frame_duration != frame_duration) {
		CALL_MOPS(module, s_duration, subdev_module, frame_duration);
		device->frame_duration = frame_duration;
	}

p_err:
	return ret;
}

int fimc_is_sensor_s_exposure_time(struct fimc_is_device_sensor *device,
	u32 exposure_time)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (exposure_time <= 0) {
		err("it is wrong exposure time (%d)", exposure_time);
		ret = -EINVAL;
		goto p_err;
	}

	if (device->exposure_time != exposure_time) {
		CALL_MOPS(module, s_exposure, subdev_module, exposure_time);
		device->exposure_time = exposure_time;
	}
p_err:
	return ret;
}

int fimc_is_sensor_g_ctrl(struct fimc_is_device_sensor *device,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!ctrl);

	subdev_module = device->subdev_module;

	ret = v4l2_subdev_call(subdev_module, core, g_ctrl, ctrl);
	if (ret) {
		err("g_ctrl is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}


int fimc_is_sensor_g_instance(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->instance;
}

int fimc_is_sensor_g_fcount(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->fcount;
}

int fimc_is_sensor_g_framerate(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->image.framerate;
}

int fimc_is_sensor_g_width(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->image.window.width;
}

int fimc_is_sensor_g_height(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->image.window.height;
}

int fimc_is_sensor_g_bns_width(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);

	if (device->image.window.otf_width)
		return device->image.window.otf_width;

	return device->image.window.width;
}

int fimc_is_sensor_g_bns_height(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	if (device->image.window.otf_height)
		return device->image.window.otf_height;

	return device->image.window.height;
}

int fimc_is_sensor_g_bns_ratio(struct fimc_is_device_sensor *device)
{
	int binning = 0;
	u32 sensor_width, sensor_height;
	u32 bns_width, bns_height;

	BUG_ON(!device);

	sensor_width = fimc_is_sensor_g_width(device);
	sensor_height = fimc_is_sensor_g_height(device);
	bns_width = fimc_is_sensor_g_bns_width(device);
	bns_height = fimc_is_sensor_g_bns_height(device);

	binning = min(BINNING(sensor_width, bns_width),
		BINNING(sensor_height, bns_height));

	return binning;
}

int fimc_is_sensor_g_bratio(struct fimc_is_device_sensor *device)
{
	int binning = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (!module) {
		merr("module is NULL", device);
		goto p_err;
	}

	binning = min(BINNING(module->active_width, device->image.window.width),
		BINNING(module->active_height, device->image.window.height));
	/* sensor binning only support natural number */
	binning = (binning / 1000) * 1000;

p_err:
	return binning;
}

int fimc_is_sensor_g_module(struct fimc_is_device_sensor *device,
	struct fimc_is_module_enum **module)
{
	int ret = 0;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);

	*module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (!*module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sensor_g_postion(struct fimc_is_device_sensor *device)
{
	return device->position;
}

int fimc_is_sensor_buffer_queue(struct fimc_is_device_sensor *device,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;
	struct v4l2_subdev *subdev_flite;

	BUG_ON(!device);

	if (index >= FRAMEMGR_MAX_REQUEST) {
		merr("index(%d) is invalid", device, index);
		ret = -EINVAL;
		goto p_err;
	}

	subdev_flite = device->subdev_flite;
	if (unlikely(!subdev_flite)) {
		merr("subdev_flite is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = GET_FRAMEMGR(device->vctx);
	if (unlikely(!framemgr)) {
		merr("framemgr is null", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame = &framemgr->frame[index];
	if (unlikely(!test_bit(FRAME_INI_MEM, &frame->memory))) {
		merr("frame %d is NOT init", device, index);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_2, flags);
	fimc_is_frame_trans_fre_to_req(framemgr, frame);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_2, flags);

	ret = v4l2_subdev_call(subdev_flite, video, s_rx_buffer, NULL, NULL);
	if (ret) {
		merr("v4l2_subdev_call(s_rx_buffer) is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sensor_buffer_finish(struct fimc_is_device_sensor *device,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	framemgr = GET_FRAMEMGR(device->vctx);
	frame = &framemgr->frame[index];

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_3, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_COMPLETE) {
		if (!frame->shot->dm.request.frameCount)
			err("request.frameCount is 0\n");
		fimc_is_frame_trans_com_to_fre(framemgr, frame);

		frame->shot_ext->free_cnt = framemgr->frame_fre_cnt;
		frame->shot_ext->request_cnt = framemgr->frame_req_cnt;
		frame->shot_ext->process_cnt = framemgr->frame_pro_cnt;
		frame->shot_ext->complete_cnt = framemgr->frame_com_cnt;
	} else {
		err("frame(%d) is not com state(%d)", index, frame->state);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_3, flags);

exit:
	return ret;
}

static int fimc_is_sensor_back_start(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	int enable;
	struct fimc_is_device_sensor *device = qdevice;
	struct v4l2_subdev *subdev_flite;
	struct fimc_is_device_flite *flite;

	BUG_ON(!device);
	BUG_ON(!device->subdev_flite);

	subdev_flite = device->subdev_flite;
	enable = FLITE_ENABLE_FLAG;

	if (test_bit(FIMC_IS_SENSOR_BACK_START, &device->state)) {
		err("already back start");
		ret = -EINVAL;
		goto p_err;
	}

	flite = (struct fimc_is_device_flite *)v4l2_get_subdevdata(subdev_flite);
	if (!flite) {
		merr("flite is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	/* to determine flite buffer done mode (early/normal) when not vision mode */
	if (!test_bit(FIMC_IS_SENSOR_DRIVING, &device->state) && flite->chk_early_buf_done) {
		flite->chk_early_buf_done(flite, device->image.framerate,
			device->pdev->id);
	}

	ret = v4l2_subdev_call(subdev_flite, video, s_stream, enable);
	if (ret) {
		merr("v4l2_flite_call(s_stream) is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_SENSOR_BACK_START, &device->state);

p_err:
	minfo("[SEN:D] %s(%dx%d, %d)\n", device, __func__,
		device->image.window.width, device->image.window.height, ret);
	return ret;
}

static int fimc_is_sensor_back_stop(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	int enable;
	struct fimc_is_device_sensor *device = qdevice;
	struct v4l2_subdev *subdev_flite;

	BUG_ON(!device);
	BUG_ON(!device->subdev_flite);

	enable = 0;
	subdev_flite = device->subdev_flite;

	if (!test_bit(FIMC_IS_SENSOR_BACK_START, &device->state)) {
		mwarn("already back stop", device);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state)) {
		mwarn("fimc_is_flite_stop, no waiting...", device);
		enable = FLITE_NOWAIT_FLAG << FLITE_NOWAIT_SHIFT;
	}

	ret = v4l2_subdev_call(subdev_flite, video, s_stream, enable);
	if (ret) {
		merr("v4l2_flite_call(s_stream) is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state);

p_err:
	minfo("[BAK:D] %s(%d)\n", device, __func__, ret);
	return ret;
}

int fimc_is_sensor_front_start(struct fimc_is_device_sensor *device,
	u32 instant_cnt,
	u32 nonblock)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct fimc_is_module_enum *module;

	BUG_ON(!device);
	BUG_ON(!device->pdata);
	BUG_ON(!device->subdev_csi);

	if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
		merr("already front start", device);
		ret = -EINVAL;
		goto p_err;
	}

	memset(device->timestamp, 0x0, FIMC_IS_TIMESTAMP_HASH_KEY * sizeof(u64));
	memset(device->timestampboot, 0x0, FIMC_IS_TIMESTAMP_HASH_KEY * sizeof(u64));
	device->instant_cnt = instant_cnt;
	subdev_csi = device->subdev_csi;
	subdev_module = device->subdev_module;
	if (!subdev_module) {
		merr("subdev_module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, video, s_stream, IS_ENABLE_STREAM);
	if (ret) {
		merr("v4l2_csi_call(s_stream) is fail(%d)", device, ret);
		goto p_err;
	}

	mdbgd_sensor("%s(snesor id : %d, csi ch : %d, size : %d x %d)\n", device,
		__func__,
		module->id,
		device->pdata->csi_ch,
		device->image.window.width,
		device->image.window.height);

	if (nonblock) {
		schedule_work(&device->instant_work);
	} else {
		fimc_is_sensor_instanton(&device->instant_work);
		if (device->instant_ret) {
			merr("fimc_is_sensor_instanton is fail(%d)", device, device->instant_ret);
			ret = device->instant_ret;
			goto p_err;
		}
	}

p_err:
	return ret;
}

int fimc_is_sensor_front_stop(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct v4l2_subdev *subdev_csi;

	BUG_ON(!device);

	if (!test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
		mwarn("already front stop", device);
		goto p_err;
	}

	subdev_csi = device->subdev_csi;

	ret = fimc_is_sensor_stop(device);
	if (ret)
		merr("sensor stream off is failed(%d)\n", device, ret);

	ret = v4l2_subdev_call(subdev_csi, video, s_stream, IS_DISABLE_STREAM);
	if (ret)
		merr("v4l2_csi_call(s_stream) is fail(%d)", device, ret);

	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);

p_err:
	minfo("[FRT:D] %s(%d)\n", device, __func__, ret);
	return ret;
}

const struct fimc_is_queue_ops fimc_is_sensor_ops = {
	.start_streaming	= fimc_is_sensor_back_start,
	.stop_streaming		= fimc_is_sensor_back_stop,
	.s_format		= fimc_is_sensor_s_format
};

static int fimc_is_sensor_suspend(struct device *dev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static int fimc_is_sensor_resume(struct device *dev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

int fimc_is_sensor_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_device_sensor *device;
	struct v4l2_subdev *subdev_csi;

	BUG_ON(!pdev);

	device = (struct fimc_is_device_sensor *)platform_get_drvdata(pdev);
	if (!device) {
		err("device is NULL");
		return -EINVAL;
	}

	ret = fimc_is_sensor_runtime_suspend_pre(dev);
	if (ret)
		err("fimc_is_sensor_runtime_suspend_pre is fail(%d)", ret);

#if defined(CONFIG_VIDEOBUF2_ION)
	if (device->mem.alloc_ctx)
		vb2_ion_detach_iommu(device->mem.alloc_ctx);
#endif

	subdev_csi = device->subdev_csi;
	if (!subdev_csi)
		mwarn("subdev_csi is NULL", device);

	ret = fimc_is_sensor_gpio_off(device);
	if (ret)
		mwarn("fimc_is_sensor_gpio_off is fail(%d)", device, ret);

	ret = fimc_is_sensor_iclk_off(device);
	if (ret)
		mwarn("fimc_is_sensor_iclk_off is fail(%d)", device, ret);

	ret = fimc_is_sensor_mclk_off(device);
	if (ret)
		mwarn("fimc_is_sensor_mclk_off is fail(%d)", device, ret);

	ret = v4l2_subdev_call(subdev_csi, core, s_power, 0);
	if (ret)
		mwarn("v4l2_csi_call(s_power) is fail(%d)", device, ret);

	if (device->subdev_module) {
		v4l2_device_unregister_subdev(device->subdev_module);
		device->subdev_module = NULL;
	}

#if defined(CONFIG_PM_DEVFREQ)
	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		pm_qos_remove_request(&exynos_sensor_qos_cam);
		pm_qos_remove_request(&exynos_sensor_qos_mem);
	}
#endif

	clear_bit(FIMC_IS_SENSOR_POWER_ON, &device->state);

	info("[SEN:D:%d] %s():%d\n", device->instance, __func__, ret);
	return 0;
}

int fimc_is_sensor_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_device_sensor *device;
	struct v4l2_subdev *subdev_csi;

	device = (struct fimc_is_device_sensor *)platform_get_drvdata(pdev);
	if (!device) {
		err("device is NULL");
		return -EINVAL;
	}

	subdev_csi = device->subdev_csi;
	if (!subdev_csi) {
		merr("subdev_csi is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_runtime_resume_pre(dev);
	if (ret) {
		merr("fimc_is_sensor_runtime_resume_pre is fail(%d)", device, ret);
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, core, s_power, 1);
	if (ret) {
		merr("v4l2_csi_call(s_power) is fail(%d)", device, ret);
		goto p_err;
	}

#if defined(CONFIG_VIDEOBUF2_ION)
	if (device->mem.alloc_ctx)
		vb2_ion_attach_iommu(device->mem.alloc_ctx);
	pr_debug("FIMC_IS runtime resume - ion attach complete\n");
#endif

	set_bit(FIMC_IS_SENSOR_POWER_ON, &device->state);

p_err:
	info("[SEN:D:%d] %s():%d\n", device->instance, __func__, ret);
	return ret;
}

static const struct dev_pm_ops fimc_is_sensor_pm_ops = {
	.suspend		= fimc_is_sensor_suspend,
	.resume			= fimc_is_sensor_resume,
	.runtime_suspend	= fimc_is_sensor_runtime_suspend,
	.runtime_resume		= fimc_is_sensor_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_match);

static struct platform_driver fimc_is_sensor_driver = {
	.probe = fimc_is_sensor_probe,
	.remove = fimc_is_sensor_remove,
	.driver = {
		.name	= FIMC_IS_SENSOR_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_sensor_pm_ops,
		.of_match_table = exynos_fimc_is_sensor_match,
	}
};

#else
static struct platform_device_id fimc_is_sensor_driver_ids[] = {
	{
		.name		= FIMC_IS_SENSOR_DEV_NAME,
		.driver_data	= 0,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, fimc_is_sensor_driver_ids);

static struct platform_driver fimc_is_sensor_driver = {
	.probe = fimc_is_sensor_probe,
	.remove = __devexit_p(fimc_is_sensor_remove),
	.id_table = fimc_is_sensor_driver_ids,
	.driver	  = {
		.name	= FIMC_IS_SENSOR_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_sensor_pm_ops,
	}
};
#endif

static int __init fimc_is_sensor_init(void)
{
	int ret = platform_driver_register(&fimc_is_sensor_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}
#if 0 // defined(CONFIG_DEFERRED_INITCALLS)
deferred_module_init(fimc_is_sensor_init);
#else
late_initcall(fimc_is_sensor_init);
#endif

static void __exit fimc_is_sensor_exit(void)
{
	platform_driver_unregister(&fimc_is_sensor_driver);
}
module_exit(fimc_is_sensor_exit);

MODULE_AUTHOR("Gilyeon lim<kilyeon.im@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS_SENSOR driver");
MODULE_LICENSE("GPL");
