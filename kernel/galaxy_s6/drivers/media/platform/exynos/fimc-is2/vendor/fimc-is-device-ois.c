/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <mach/exynos-fimc-is-sensor.h>
#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
#include <linux/kthread.h>
#endif
#include <linux/pinctrl/pinctrl-samsung.h>

#include "fimc-is-core.h"
#include "fimc-is-interface.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-ois.h"
#ifdef CONFIG_AF_HOST_CONTROL
#include "fimc-is-device-af.h"
#endif

#define FIMC_IS_OIS_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_OIS_DEV_NAME		"exynos-fimc-is-ois"
#define FIMC_OIS_FW_NAME_SEC		"ois_fw_sec.bin"
#define FIMC_OIS_FW_NAME_DOM		"ois_fw_dom.bin"
#define OIS_BOOT_FW_SIZE	(1024 * 4)
#define OIS_PROG_FW_SIZE	(1024 * 24)
#define FW_GYRO_SENSOR		0
#define FW_DRIVER_IC		1
#define FW_CORE_VERSION		2
#define FW_RELEASE_MONTH		3
#define FW_RELEASE_COUNT		4
#define FW_TRANS_SIZE		256
#define OIS_VER_OFFSET	14
#define OIS_BIN_LEN		28672
#define OIS_BIN_HEADER		28658
#define OIS_FW_HEADER_SIZE		6
static u8 bootCode[OIS_BOOT_FW_SIZE] = {0,};
static u8 progCode[OIS_PROG_FW_SIZE] = {0,};

static struct fimc_is_ois_info ois_minfo;
static struct fimc_is_ois_info ois_pinfo;
static struct fimc_is_ois_info ois_uinfo;
static struct fimc_is_ois_exif ois_exif_data;
static bool fw_sdcard;
static bool not_crc_bin;
#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
static struct task_struct *ois_ts;
#endif

static void fimc_is_ois_i2c_config(struct i2c_client *client, bool onoff)
{
	struct pinctrl *pinctrl_i2c = NULL;
	struct device *i2c_dev = client->dev.parent->parent;
	struct fimc_is_device_ois *ois_device = i2c_get_clientdata(client);
	struct fimc_is_ois_gpio *gpio;

	gpio = &ois_device->gpio;

	if (ois_device->ois_hsi2c_status != onoff) {
		info("%s : ois_hsi2c_stauts(%d),onoff(%d)\n",__func__,
			ois_device->ois_hsi2c_status, onoff);

		if (onoff) {
			pin_config_set(gpio->pinname, gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 0));
			pin_config_set(gpio->pinname, gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 0));

			pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
			if (IS_ERR_OR_NULL(pinctrl_i2c)) {
				printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
			} else {
				devm_pinctrl_put(pinctrl_i2c);
			}
		} else {
			pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "off_i2c");
			if (IS_ERR_OR_NULL(pinctrl_i2c)) {
				printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
			} else {
				devm_pinctrl_put(pinctrl_i2c);
			}

			pin_config_set(gpio->pinname, gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
			pin_config_set(gpio->pinname, gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
		}
		ois_device->ois_hsi2c_status = onoff;
	}

}

int fimc_is_ois_i2c_read(struct i2c_client *client, u16 addr, u8 *data)
{
	int err;
	u8 txbuf[2], rxbuf[1];
	struct i2c_msg msg[2];

	*data = 0;
	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail err = %d\n", __func__, err);
		return -EIO;
	}

	*data = rxbuf[0];
	return 0;
}

int fimc_is_ois_i2c_write(struct i2c_client *client ,u16 addr, u8 data)
{
        int retries = I2C_RETRY_COUNT;
        int ret = 0, err = 0;
        u8 buf[3] = {0,};
        struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = 3,
                .buf    = buf,
        };

        buf[0] = (addr & 0xff00) >> 8;
        buf[1] = addr & 0xff;
        buf[2] = data;

#if 0
        info("%s : W(0x%02X%02X %02X)\n",__func__, buf[0], buf[1], buf[2]);
#endif

        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
                        err, addr, data, I2C_RETRY_COUNT - retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
        }

        return 0;
}

int fimc_is_ois_i2c_write_multi(struct i2c_client *client ,u16 addr, u8 *data, size_t size)
{
	int retries = I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	ulong i = 0;
	u8 buf[258] = {0,};
	struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = size,
                .buf    = buf,
	};

	buf[0] = (addr & 0xFF00) >> 8;
	buf[1] = addr & 0xFF;

	for (i = 0; i < size - 2; i++) {
	        buf[i + 2] = *(data + i);
	}
#if 0
        info("OISLOG %s : W(0x%02X%02X%02X)\n", __func__, buf[0], buf[1], buf[2]);
#endif
        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
                        err, addr, *data, I2C_RETRY_COUNT - retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
	}

        return 0;
}

static int fimc_is_ois_i2c_read_multi(struct i2c_client *client, u16 addr, u8 *data, size_t size)
{
	int err;
	u8 rxbuf[256], txbuf[2];
	struct i2c_msg msg[2];

	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail", __func__);
		return -EIO;
	}

	memcpy(data, rxbuf, size);
	return 0;
}

bool fimc_is_ois_check_sensor(struct fimc_is_core *core)
{
	int i = 0;
	bool ret = false;
	int retry_count = 20;

	do {
		ret = false;
		for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
			if (!core->sensor[i].is_probed) {
				ret = true;
				break;
			}
		}

		if (i == FIMC_IS_SENSOR_COUNT && ret == false) {
			info("Retry count = %d\n", retry_count);
			break;
		}

		mdelay(100);
		if (retry_count > 0) {
			--retry_count;
		} else {
			err("Could not get sensor before start ois fw update routine.\n");
			break;
		}
	} while (ret);

	return ret;
}

int fimc_is_ois_gpio_on(struct fimc_is_core *core)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;
	struct device *dev = &core->ischain[0].pdev->dev;

	ret = fimc_is_sec_run_fw_sel(dev, SENSOR_POSITION_REAR);
	if (ret) {
		err("fimc_is_sec_run_fw_sel for rear is fail(%d)", ret);
	}

	ret = fimc_is_sec_run_fw_sel(dev, SENSOR_POSITION_FRONT);
	if (ret) {
		err("fimc_is_sec_run_fw_sel for front is fail(%d)", ret);
	}

	sensor_id = core->pdata->rear_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
		else {
			err("%s: Could not find sensor id.", __func__);
			ret = -EINVAL;
			goto p_err;
		}
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module->pdev, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ois_gpio_off(struct fimc_is_core *core)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;

	sensor_id = core->pdata->rear_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
		else {
			err("%s: Could not find sensor id.", __func__);
			ret = -EINVAL;
			goto p_err;
		}
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module->pdev, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

void fimc_is_ois_enable(struct fimc_is_core *core)
{
	int ret = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return;
	}

	info("%s : E\n", __FUNCTION__);
	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x02, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x00, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}
	info("%s : X\n", __FUNCTION__);
}

int fimc_is_ois_sine_mode(struct fimc_is_core *core, int mode)
{
	int ret = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	info("%s : E\n", __FUNCTION__);
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, true);
	}

	if (mode == OPTICAL_STABILIZATION_MODE_SINE_X) {
		ret = fimc_is_ois_i2c_write(core->client1, 0x18, 0x01);
	} else if (mode == OPTICAL_STABILIZATION_MODE_SINE_Y) {
		ret = fimc_is_ois_i2c_write(core->client1, 0x18, 0x02);
	}
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x19, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x1A, 0x1E);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x02, 0x03);
	if (ret) {
		err("i2c write fail\n");
	}

	msleep(20);

	ret = fimc_is_ois_i2c_write(core->client1, 0x00, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	if (core_pdata->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}
	info("%s : X\n", __FUNCTION__);

	return ret;
}
EXPORT_SYMBOL(fimc_is_ois_sine_mode);

void fimc_is_ois_version(struct fimc_is_core *core)
{
	int ret = 0;
	u8 val_c = 0, val_d = 0;
	u16 version = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return;
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00FC, &val_c);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00FD, &val_d);
	if (ret) {
		err("i2c write fail\n");
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	version = (val_d << 8) | val_c;
	info("OIS version = 0x%04x\n", version);
}

void fimc_is_ois_offset_test(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y)
{
	int ret = 0, i = 0;
	u8 val = 0, x = 0, y = 0;
	int x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 20;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return;
	}

	info("%s : E\n", __FUNCTION__);
	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x0014, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	retries = avg_count;
	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0014, &val);
		if (ret != 0) {
			break;
		}
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x0248, &val);
		x = val;
		fimc_is_ois_i2c_read(core->client1, 0x0249, &val);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / 175 / 10;

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x024A, &val);
		y = val;
		fimc_is_ois_i2c_read(core->client1, 0x024B, &val);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / 175 / 10;

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	fimc_is_ois_version(core);
	info("%s : X\n", __FUNCTION__);
	return;
}

void fimc_is_ois_get_offset_data(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y)
{
	int i = 0;
	u8 val = 0, x = 0, y = 0;
	int x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 20;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return;
	}

	info("%s : E\n", __FUNCTION__);
	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x0248, &val);
		x = val;
		fimc_is_ois_i2c_read(core->client1, 0x0249, &val);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / 175 / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x024A, &val);
		y = val;
		fimc_is_ois_i2c_read(core->client1, 0x024B, &val);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / 175 / 10;

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	fimc_is_ois_version(core);
	info("%s : X\n", __FUNCTION__);
	return;
}

int fimc_is_ois_self_test(struct fimc_is_core *core)
{
	int ret = 0;
	u8 val = 0;
	int retries = 20;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	info("%s : E\n", __FUNCTION__);
	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x0014, 0x08);
	if (ret) {
		err("i2c write fail\n");
	}

	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0014, &val);
		if (ret != 0) {
			val = -EIO;
			break;
		}
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	ret = fimc_is_ois_i2c_read(core->client1, 0x0004, &val);
	if (ret != 0) {
		val = -EIO;
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	info("%s(%d) : X\n", __FUNCTION__, val);
	return (int)val;
}

bool fimc_is_ois_diff_test(struct fimc_is_core *core, int *x_diff, int *y_diff)
{
	int ret = 0;
	u8 val = 0, x = 0, y = 0;
	u16 x_min = 0, y_min = 0, x_max = 0, y_max = 0;
	int retries = 20, default_diff = 1100;
	u8 read_x[2], read_y[2];
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return false;
	}

#ifdef CONFIG_AF_HOST_CONTROL
	fimc_is_af_move_lens(core);
	msleep(30);
#endif

	info("(%s) : E\n", __FUNCTION__);
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read_multi(core->client1, 0x021A, read_x, 2);
	ret |= fimc_is_ois_i2c_read_multi(core->client1, 0x021C, read_y, 2);
	if (ret) {
		err("i2c read fail\n");
	}

	ret = fimc_is_ois_i2c_write_multi(core->client1, 0x0022, read_x, 4);
	ret |= fimc_is_ois_i2c_write_multi(core->client1, 0x0024, read_y, 4);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0002, 0x02);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0000, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}
	msleep(400);
	info("(%s) : OIS Position = Center\n", __FUNCTION__);

	ret = fimc_is_ois_i2c_write(core->client1, 0x0000, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0001, &val);
		if (ret != 0) {
			break;
		}
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val != 1);

	ret = fimc_is_ois_i2c_write(core->client1, 0x0034, 0x64);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0230, 0x64);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0231, 0x00);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0232, 0x64);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0233, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0034, &val);
	info("OIS[read_val_0x0034::0x%04x]\n", val);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0230, &val);
	info("OIS[read_val_0x0230::0x%04x]\n", val);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0231, &val);
	info("OIS[read_val_0x0231::0x%04x]\n", val);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0232, &val);
	info("OIS[read_val_0x0232::0x%04x]\n", val);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0233, &val);
	info("OIS[read_val_0x0233::0x%04x]\n", val);
	if (ret) {
		err("i2c read fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x020E, 0x00);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x020F, 0x00);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0210, 0x50);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0211, 0x50);
	if (ret) {
		err("i2c write fail\n");
	}
	ret = fimc_is_ois_i2c_write(core->client1, 0x0013, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

#if 0
	ret = fimc_is_ois_i2c_write(core->client1, 0x0012, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	retries = 30;
	do { //polarity check
		ret = fimc_is_ois_i2c_read(core->client1, 0x0012, &val);
		if (ret != 0) {
			break;
		}
		msleep(100);
		if (--retries < 0) {
			err("Polarity check is not done or not [read_val_0x0012::0x%04x]\n", val);
			break;
		}
	} while (val);
	fimc_is_ois_i2c_read(core->client1, 0x0200, &val);
	err("OIS[read_val_0x0200::0x%04x]\n", val);
#endif

	retries = 120;
	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0013, &val);
		if (ret != 0) {
			break;
		}
		msleep(100);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	fimc_is_ois_i2c_read(core->client1, 0x0004, &val);
	info("OIS[read_val_0x0004::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0005, &val);
	info("OIS[read_val_0x0005::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0006, &val);
	info("OIS[read_val_0x0006::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0022, &val);
	info("OIS[read_val_0x0022::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0023, &val);
	info("OIS[read_val_0x0023::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0024, &val);
	info("OIS[read_val_0x0024::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0025, &val);
	info("OIS[read_val_0x0025::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0200, &val);
	info("OIS[read_val_0x0200::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x020E, &val);
	info("OIS[read_val_0x020E::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x020F, &val);
	info("OIS[read_val_0x020F::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0210, &val);
	info("OIS[read_val_0x0210::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0211, &val);
	info("OIS[read_val_0x0211::0x%04x]\n", val);

	fimc_is_ois_i2c_read(core->client1, 0x0212, &val);
	x = val;
	info("OIS[read_val_0x0212::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0213, &val);
	x_max = (val << 8) | x;
	info("OIS[read_val_0x0213::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0214, &val);
	x = val;
	info("OIS[read_val_0x0214::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0215, &val);
	x_min = (val << 8) | x;
	info("OIS[read_val_0x0215::0x%04x]\n", val);

	fimc_is_ois_i2c_read(core->client1, 0x0216, &val);
	y = val;
	info("OIS[read_val_0x0216::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0217, &val);
	y_max = (val << 8) | y;
	info("OIS[read_val_0x0217::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0218, &val);
	y = val;
	info("OIS[read_val_0x0218::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0219, &val);
	y_min = (val << 8) | y;
	info("OIS[read_val_0x0219::0x%04x]\n", val);

	fimc_is_ois_i2c_read(core->client1, 0x021A, &val);
	info("OIS[read_val_0x021A::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x021B, &val);
	info("OIS[read_val_0x021B::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x021C, &val);
	info("OIS[read_val_0x021C::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x021D, &val);
	info("OIS[read_val_0x021D::0x%04x]\n", val);

	*x_diff = abs(x_max - x_min);
	*y_diff = abs(y_max - y_min);

	info("(%s) : X (default_diff:%d)(%d,%d)\n", __FUNCTION__,
			default_diff, *x_diff, *y_diff);

	ret = fimc_is_ois_i2c_read_multi(core->client1, 0x021A, read_x, 2);
	ret |= fimc_is_ois_i2c_read_multi(core->client1, 0x021C, read_y, 2);
	if (ret) {
		err("i2c read fail\n");
	}

	ret = fimc_is_ois_i2c_write_multi(core->client1, 0x0022, read_x, 4);
	ret |= fimc_is_ois_i2c_write_multi(core->client1, 0x0024, read_y, 4);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0002, 0x02);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0000, 0x01);
	msleep(400);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0000, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	if (core_pdata->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	if (*x_diff > default_diff  && *y_diff > default_diff) {
		return true;
	} else {
		return false;
	}
}

u16 fimc_is_ois_calc_checksum(u8 *data, int size)
{
	int i = 0;
	u16 result = 0;

	for(i = 0; i < size; i += 2) {
		result = result + (0xFFFF & (((*(data + i + 1)) << 8) | (*(data + i))));
	}

	return result;
}

void fimc_is_ois_exif_data(struct fimc_is_core *core)
{
	int ret = 0;
	u8 error_reg[2], status_reg;
	u16 error_sum;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return;
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0004, &error_reg[0]);
	if (ret) {
		err("i2c read fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0005, &error_reg[1]);
	if (ret) {
		err("i2c read fail\n");
	}

	error_sum = (error_reg[1] << 8) | error_reg[0];

	ret = fimc_is_ois_i2c_read(core->client1, 0x0001, &status_reg);
	if (ret) {
		err("i2c read fail\n");
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	ois_exif_data.error_data = error_sum;
	ois_exif_data.status_data = status_reg;

	return;
}

int fimc_is_ois_get_exif_data(struct fimc_is_ois_exif **exif_info)
{
	*exif_info = &ois_exif_data;
	return 0;
}

bool fimc_is_ois_fw_version(struct fimc_is_core *core)
{
	int ret = 0;
	char version[7] = {0, };
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return false;
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00F9, &version[0]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00F8, &version[1]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007C, &version[2]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007D, &version[3]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007E, &version[4]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007F, &version[5]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	memcpy(ois_minfo.header_ver, version, 6);
	core->ois_ver_read = true;

	return true;

exit:
	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return false;
}

int fimc_is_ois_get_module_version(struct fimc_is_ois_info **minfo)
{
	*minfo = &ois_minfo;
	return 0;
}

int fimc_is_ois_get_phone_version(struct fimc_is_ois_info **pinfo)
{
	*pinfo = &ois_pinfo;
	return 0;
}

int fimc_is_ois_get_user_version(struct fimc_is_ois_info **uinfo)
{
	*uinfo = &ois_uinfo;
	return 0;
}


int fimc_is_ois_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_RELEASE_MONTH] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_RELEASE_COUNT] - 48) * 10;
	revision = revision + (int)fw_ver[FW_RELEASE_COUNT + 1] - 48;

	return revision;
}

bool fimc_is_ois_version_compare(char *fw_ver1, char *fw_ver2, char *fw_ver3)
{
	if (fw_ver1[FW_GYRO_SENSOR] != fw_ver2[FW_GYRO_SENSOR]
		|| fw_ver1[FW_DRIVER_IC] != fw_ver2[FW_DRIVER_IC]
		|| fw_ver1[FW_CORE_VERSION] != fw_ver2[FW_CORE_VERSION]) {
		return false;
	}

	if (fw_ver2[FW_GYRO_SENSOR] != fw_ver3[FW_GYRO_SENSOR]
		|| fw_ver2[FW_DRIVER_IC] != fw_ver3[FW_DRIVER_IC]
		|| fw_ver2[FW_CORE_VERSION] != fw_ver3[FW_CORE_VERSION]) {
		return false;
	}

	return true;
}

bool fimc_is_ois_version_compare_default(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_GYRO_SENSOR] != fw_ver2[FW_GYRO_SENSOR]
		|| fw_ver1[FW_DRIVER_IC] != fw_ver2[FW_DRIVER_IC]
		|| fw_ver1[FW_CORE_VERSION] != fw_ver2[FW_CORE_VERSION]) {
		return false;
	}

	return true;
}

bool fimc_is_ois_read_userdata(struct fimc_is_core *core)
{
	u8 SendData[2], RcvData[256];
	u8 ois_shift_info = 0;

	struct i2c_client *client = core->client1;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return false;
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	/* Read OIS Shift CAL */
	fimc_is_ois_i2c_write(client ,0x000F, 0x0E);
	SendData[0] = 0x00;
	SendData[1] = 0x04;
	fimc_is_ois_i2c_write_multi(client, 0x0010, SendData, 4);
	fimc_is_ois_i2c_write(client ,0x000E, 0x04);

	fimc_is_ois_i2c_read(client, 0x0100, &ois_shift_info);
	info("OIS Shift Info : 0x%x\n", ois_shift_info);

	if (ois_shift_info == 0x11) {
		u16 ois_shift_checksum = 0;
		u16 ois_shift_x_diff = 0;
		u16 ois_shift_y_diff = 0;
		s16 ois_shift_x_cal = 0;
		s16 ois_shift_y_cal = 0;
		int i = 0;
		u8 calData[2];

		/* OIS Shift CheckSum */
		calData[0] = 0;
		calData[1] = 0;
		fimc_is_ois_i2c_read_multi(client, 0x0102, calData, 2);
		ois_shift_checksum = (calData[1] << 8)|(calData[0]);
		info("OIS Shift CheckSum = 0x%x\n", ois_shift_checksum);

		/* OIS Shift X Diff */
		calData[0] = 0;
		calData[1] = 0;
		fimc_is_ois_i2c_read_multi(client, 0x0104, calData, 2);
		ois_shift_x_diff = (calData[1] << 8)|(calData[0]);
		info("OIS Shift X Diff = 0x%x\n", ois_shift_x_diff);

		/* OIS Shift Y Diff */
		calData[0] = 0;
		calData[1] = 0;
		fimc_is_ois_i2c_read_multi(client, 0x0106, calData, 2);
		ois_shift_y_diff = (calData[1] << 8)|(calData[0]);
		info("OIS Shift Y Diff = 0x%x\n", ois_shift_y_diff);

		/* OIS Shift CAL DATA */
		for (i = 0; i< 9; i++) {
			calData[0] = 0;
			calData[1] = 0;
			fimc_is_ois_i2c_read_multi(client, 0x0108 + i*2, calData, 2);
			ois_shift_x_cal = (calData[1] << 8)|(calData[0]);

			calData[0] = 0;
			calData[1] = 0;
			fimc_is_ois_i2c_read_multi(client, 0x0120 + i*2, calData, 2);
			ois_shift_y_cal = (calData[1] << 8)|(calData[0]);
			info("OIS CAL[%d]:X[%d], Y[%d]\n", i, ois_shift_x_cal, ois_shift_y_cal);
		}
	}

	/* Read OIS FW */
	fimc_is_ois_i2c_write(client ,0x000F, 0x40);
	SendData[0] = 0x00;
	SendData[1] = 0x00;
	fimc_is_ois_i2c_write_multi(client, 0x0010, SendData, 4);
	fimc_is_ois_i2c_write(client ,0x000E, 0x04);

	fimc_is_ois_i2c_read(client, 0x000E, &RcvData[0]);
	if (RcvData[0] == 0x14) {
		fimc_is_ois_i2c_read_multi(client, 0x0100, RcvData, FW_TRANS_SIZE);
		if (core_pdata->use_ois_hsi2c) {
			fimc_is_ois_i2c_config(core->client1, false);
		}
		memcpy(ois_uinfo.header_ver, RcvData, 6);
		return true;
	} else {
		if (core_pdata->use_ois_hsi2c) {
			fimc_is_ois_i2c_config(core->client1, false);
		}
		return false;
	}
}

int fimc_is_ois_open_fw(struct fimc_is_core *core, char *name, u8 **buf)
{
	int ret = 0;
	ulong size = 0;
	const struct firmware *fw_blob = NULL;
	static char fw_name[100];
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread;
	int fw_requested = 1;
	int retry_count = 0;

	fw_sdcard = false;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	snprintf(fw_name, sizeof(fw_name), "%s%s", FIMC_IS_OIS_SDCARD_PATH, name);
	fp = filp_open(fw_name, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		info("failed to open SDCARD fw!!!\n");
		goto request_fw;
	}

	fw_requested = 0;
	size = fp->f_path.dentry->d_inode->i_size;
	info("start read sdcard, file path %s, size %lu Bytes\n", fw_name, size);

	*buf = vmalloc(size);
	if (!(*buf)) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	nread = vfs_read(fp, (char __user *)(*buf), size, &fp->f_pos);
	if (nread != size) {
		err("failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto p_err;
	}

	memcpy(ois_pinfo.header_ver, *buf + OIS_BIN_HEADER, OIS_FW_HEADER_SIZE);
	memcpy(bootCode, *buf, OIS_BOOT_FW_SIZE);
	memcpy(progCode, *buf + OIS_BIN_LEN - OIS_PROG_FW_SIZE, OIS_PROG_FW_SIZE);
	fw_sdcard = true;
	if (OIS_BIN_LEN >= nread) {
		not_crc_bin = true;
		err("ois fw binary size = %ld.\n", nread);
	}

request_fw:
	if (fw_requested) {
		snprintf(fw_name, sizeof(fw_name), "%s", name);
		set_fs(old_fs);
		retry_count = 3;
		ret = request_firmware(&fw_blob, fw_name, &core->companion.pdev->dev);
		while (--retry_count && ret == -EAGAIN) {
			err("request_firmware retry(count:%d)", retry_count);
			ret = request_firmware(&fw_blob, fw_name, &core->companion.pdev->dev);
		}

		if (ret) {
			err("request_firmware is fail(ret:%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}

		if (!fw_blob) {
			err("fw_blob is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		if (!fw_blob->data) {
			err("fw_blob->data is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		size = fw_blob->size;

		*buf = vmalloc(size);
		if (!(*buf)) {
			err("failed to allocate memory");
			ret = -ENOMEM;
			goto p_err;
		}
		memcpy((void *)(*buf), fw_blob->data, size);
		memcpy(ois_pinfo.header_ver, *buf + OIS_BIN_HEADER, OIS_FW_HEADER_SIZE);
		memcpy(bootCode, *buf, OIS_BOOT_FW_SIZE);
		memcpy(progCode, *buf + OIS_BIN_LEN - OIS_PROG_FW_SIZE, OIS_PROG_FW_SIZE);
		if (OIS_BIN_LEN >= size) {
			not_crc_bin = true;
			err("ois fw binary size = %lu.\n", size);
		}
		info("OIS firmware is loaded from Phone binary.\n");
	}

p_err:
	if (!fw_requested) {
		if (!IS_ERR_OR_NULL(fp)) {
			filp_close(fp, current->files);
		}
		set_fs(old_fs);
	} else {
		if (!IS_ERR_OR_NULL(fw_blob)) {
			release_firmware(fw_blob);
		}
	}

	return ret;
}

bool fimc_is_ois_check_fw(struct fimc_is_core *core)
{
	u8 *buf = NULL;
	int ret = 0;

	ret = fimc_is_ois_fw_version(core);
	if (!ret) {
		err("Failed to read ois fw version.");
		return false;
	}
	fimc_is_ois_read_userdata(core);

	if (ois_minfo.header_ver[2] == 'C' || ois_minfo.header_ver[2] == 'E') {
		ret = fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME_DOM, &buf);
	} else if (ois_minfo.header_ver[2] == 'D' || ois_minfo.header_ver[2] == 'F') {
		ret = fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME_SEC, &buf);
	} else {
		info("Module FW version does not matched with phone FW version.\n");
		strcpy(ois_pinfo.header_ver, "NULL");
		return true;
	}
	if (ret) {
		err("fimc_is_ois_open_fw failed(%d)\n", ret);
	}

	if (buf) {
		vfree(buf);
	}

	return true;
}

u8 fimc_is_ois_read_status(struct fimc_is_core *core)
{
	int ret = 0;
	u8 status = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0006, &status);
	if (ret) {
		err("i2c read fail\n");
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return status;
}

u8 fimc_is_ois_read_cal_checksum(struct fimc_is_core *core)
{
	int ret = 0;
	u8 status = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0005, &status);
	if (ret) {
		err("i2c read fail\n");
	}

	if (core_pdata->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return status;
}

void fimc_is_ois_fw_status(struct fimc_is_core *core, u8 *checksum, u8 *caldata)
{
	*checksum = fimc_is_ois_read_status(core);
	*caldata = fimc_is_ois_read_cal_checksum(core);

	return;
}

bool fimc_is_ois_crc_check(struct fimc_is_core *core, char *buf)
{
	u8 check_8[4] = {0, };
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_bin;

	if (not_crc_bin) {
		err("ois binary does not conatin crc checksum.\n");
		return false;
	}

	if (buf == NULL) {
		err("buf is NULL. CRC check failed.");
		return false;
	}

	buf32 = (u32 *)buf;

	memcpy(check_8, buf + OIS_BIN_LEN, 4);
	checksum_bin = (check_8[3] << 24) | (check_8[2] << 16) | (check_8[1] << 8) | (check_8[0]);

	checksum = (u32)getCRC((u16 *)&buf32[0], OIS_BIN_LEN, NULL, NULL);
	if (checksum != checksum_bin) {
		return false;
	} else {
		return true;
	}
}

void fimc_is_ois_fw_update(struct fimc_is_core *core)
{
	u8 SendData[256], RcvData;
	u16 RcvDataShort = 0;
	u8 data[2] = {0, };
	int block, i, position = 0;
	u16 checkSum;
	u8 *buf = NULL;
	int ret = 0, sum_size = 0, retry_count = 3;
	int module_ver = 0, binary_ver = 0;
	u8 error_status = 0;
	bool forced_update = false, crc_result = false;
	bool need_reset = false;
	struct i2c_client *client = core->client1;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return;
	}

	/* OIS Status Check */
	error_status = fimc_is_ois_read_status(core);
	if (error_status == 0x01) {
		forced_update = true;
	}

	ret = fimc_is_ois_fw_version(core);
	if (!ret) {
		err("Failed to read ois fw version.");
		return;
	}
	fimc_is_ois_read_userdata(core);

	if (ois_minfo.header_ver[2] == 'C' || ois_minfo.header_ver[2] == 'E') {
		ret = fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME_DOM, &buf);
	} else if (ois_minfo.header_ver[2] == 'D' || ois_minfo.header_ver[2] == 'F') {
		ret = fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME_SEC, &buf);
	} else {
		info("Module FW version does not matched with phone FW version.\n");
		strcpy(ois_pinfo.header_ver, "NULL");
		return;
	}
	if (ret) {
		err("fimc_is_ois_open_fw failed(%d)", ret);
		goto p_err;
	}

	if (ois_minfo.header_ver[2] < 'E') {
		info("Do not update ois Firmware. FW version is low.\n");
		goto p_err;
	}

	if (buf == NULL) {
		err("buf is NULL. OIS FW Update failed.");
		return;
	}

	if (!forced_update) {
		if (ois_uinfo.header_ver[0] == 0xFF && ois_uinfo.header_ver[1] == 0xFF &&
			ois_uinfo.header_ver[2] == 0xFF) {
			err("OIS userdata is not valid.");
			goto p_err;
		}
	}

	/*Update OIS FW when Gyro sensor/Driver IC/ Core version is same*/
	if (!forced_update) {
		if (!fimc_is_ois_version_compare(ois_minfo.header_ver, ois_pinfo.header_ver,
			ois_uinfo.header_ver)) {
			err("Does not update ois fw. OIS module ver = %s, binary ver = %s, userdata ver = %s",
				ois_minfo.header_ver, ois_pinfo.header_ver, ois_uinfo.header_ver);
			goto p_err;
		}
	} else {
		if (!fimc_is_ois_version_compare_default(ois_minfo.header_ver, ois_pinfo.header_ver)) {
			err("Does not update ois fw. OIS module ver = %s, binary ver = %s",
				ois_minfo.header_ver, ois_pinfo.header_ver);
			goto p_err;
		}
	}

	crc_result = fimc_is_ois_crc_check(core, buf);
	if (crc_result == false) {
		err("OIS CRC32 error.\n");
		goto p_err;
	}

	if (!fw_sdcard && !forced_update) {
		module_ver = fimc_is_ois_fw_revision(ois_minfo.header_ver);
		binary_ver = fimc_is_ois_fw_revision(ois_pinfo.header_ver);
		if (binary_ver <= module_ver) {
			err("OIS module ver = %d, binary ver = %d", module_ver, binary_ver);
			goto p_err;
		}
	}
	info("OIS fw update started. module ver = %s, binary ver = %s, userdata ver = %s\n",
		ois_minfo.header_ver, ois_pinfo.header_ver, ois_uinfo.header_ver);

	if (core_pdata->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, true);
	}

retry:
	if (need_reset) {
		fimc_is_ois_gpio_off(core);
		msleep(30);
		fimc_is_ois_gpio_on(core);
		msleep(150);
	}

	fimc_is_ois_i2c_read(client, 0x01, &RcvData); /* OISSTS Read */

	if (RcvData != 0x01) {/* OISSTS != IDLE */
		err("RCV data return!!");
		goto p_err;
	}

	/* Update a Boot Program */
	/* SET FWUP_CTRL REG */
	fimc_is_ois_i2c_write(client ,0x000C, 0x0B);
	msleep(20);

	position = 0;
	/* Write Boot Data */
	for (block = 4; block < 8; block++) {       /* 0x1000 - 0x1FFF */
		for (i = 0; i < 4; i++) {
			memcpy(SendData, bootCode + position, FW_TRANS_SIZE);
			fimc_is_ois_i2c_write_multi(client, 0x0100, SendData, FW_TRANS_SIZE + 2);
			position += FW_TRANS_SIZE;
			if (i == 0 ) {
				msleep(20);
			} else if ((i == 1) || (i == 2)) {	 /* Wait Erase and Write process */
				 msleep(10);		/* Wait Write process */
			} else {
				msleep(15); /* Wait Write and Verify process */
			}
		}
	}

	/* CHECKSUM (Boot) */
	sum_size = OIS_BOOT_FW_SIZE;
	checkSum = fimc_is_ois_calc_checksum(&bootCode[0], sum_size);
	SendData[0] = (checkSum & 0x00FF);
	SendData[1] = (checkSum & 0xFF00) >> 8;
	SendData[2] = 0x00;
	SendData[3] = 0x00;
	fimc_is_ois_i2c_write_multi(client, 0x08, SendData, 6);
	msleep(10);			 /* (RUMBA Self Reset) */

	fimc_is_ois_i2c_read_multi(client, 0x06, data, 2); /* Error Status read */
	info("%s: OISSTS Read = 0x%02x%02x\n", __func__, data[1], data[0]);

	if (data[1] == 0x00 && data[0] == 0x01) {					/* Boot F/W Update Error check */
		/* Update a User Program */
		/* SET FWUP_CTRL REG */
		position = 0;
		SendData[0] = 0x09;		/* FWUP_DSIZE=256Byte, FWUP_MODE=PROG, FWUPEN=Start */
		fimc_is_ois_i2c_write(client, 0x0C, SendData[0]);		/* FWUPCTRL REG(0x000C) 1Byte Send */

		/* Write UserProgram Data */
		for (block = 4; block <= 27; block++) {		/* 0x1000 -0x 6FFF (RUMBA-SA)*/
			for (i = 0; i < 4; i++) {
				memcpy(SendData, &progCode[position], (size_t)FW_TRANS_SIZE);
				fimc_is_ois_i2c_write_multi(client, 0x100, SendData, FW_TRANS_SIZE + 2); /* FLS_DATA REG(0x0100) 256Byte Send */
				position += FW_TRANS_SIZE;
				if (i ==0 ) {
					msleep(20);		/* Wait Erase and Write process */
				} else if ((i==1) || (i==2)) {
					msleep(10);		/* Wait Write process */
				} else {
					msleep(15);		/* Wait Write and Verify process */
				}
			}
		}

		/* CHECKSUM (Program) */
		sum_size = OIS_PROG_FW_SIZE;
		checkSum = fimc_is_ois_calc_checksum(&progCode[0], sum_size);
		SendData[0] = (checkSum & 0x00FF);
		SendData[1] = (checkSum & 0xFF00) >> 8;
		SendData[2] = 0;		/* Don't Care */
		SendData[3] = 0x80;		/* Self Reset Request */

		fimc_is_ois_i2c_write_multi(client, 0x08, SendData, 6);  /* FWUP_CHKSUM REG(0x0008) 4Byte Send */
		msleep(60);		/* (Wait RUMBA Self Reset) */
		fimc_is_ois_i2c_read(client, 0x6, (u8*)&RcvDataShort); /* Error Status read */

		if (RcvDataShort == 0x0000) {
			/* F/W Update Success Process */
			info("%s: OISLOG OIS program update success\n", __func__);
		} else {
			/* Retry Process */
			if (retry_count > 0) {
				err("OISLOG OIS program update fail, retry update. count = %d", retry_count);
				retry_count--;
				need_reset = true;
				goto retry;
			}

			/* F/W Update Error Process */
			err("OISLOG OIS program update fail");
			goto p_err;
		}
	} else {
		/* Retry Process */
		if (retry_count > 0) {
			err("OISLOG OIS boot update fail, retry update. count = %d", retry_count);
			retry_count--;
			need_reset = true;
			goto retry;
		}

		/* Error Process */
		err("CAN'T process OIS program update");
		goto p_err;
	}

	/* S/W Reset */
	fimc_is_ois_i2c_write(client ,0x000D, 0x01);
	fimc_is_ois_i2c_write(client ,0x000E, 0x06);

	if (core_pdata->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	msleep(30);
	ret = fimc_is_ois_fw_version(core);
	if (!ret) {
		err("Failed to read ois fw version.");
		goto p_err;
	}

	if (core_pdata->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, true);
	}

	if (!fimc_is_ois_version_compare_default(ois_minfo.header_ver, ois_pinfo.header_ver)) {
		err("After update ois fw is not correct. OIS module ver = %s, binary ver = %s",
			ois_minfo.header_ver, ois_pinfo.header_ver);
		goto p_err;
	}

	/* Param init - Flash to Rumba */
	fimc_is_ois_i2c_write(client ,0x0036, 0x03);
	msleep(30);

p_err:
	info("OIS module ver = %s, binary ver = %s, userdata ver = %s\n",
		ois_minfo.header_ver, ois_pinfo.header_ver, ois_uinfo.header_ver);

	if (core_pdata->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	if (buf) {
		vfree(buf);
	}
	return;
}

#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
int fimc_is_ois_thread(void *data)
{
	struct fimc_is_core *core = data;
	bool ret = false;

	ret = fimc_is_ois_check_sensor(core);
	if (ret) {
		err("Do not update ois fw update. Check sensor failed!\n");
		return -EINVAL;
	} else {
		info("Start ois fw update. Check sensor success!\n");
	}

	fimc_is_ois_gpio_on(core);
	msleep(150);
	fimc_is_ois_fw_update(core);
	fimc_is_ois_gpio_off(core);

	return 0;
}

void fimc_is_ois_init_thread(struct fimc_is_core *core)
{
	info("OIS fimc_is_ois_init_thread\n");

	ois_ts = kthread_run(fimc_is_ois_thread, core, "ois_thread");
	if (IS_ERR_OR_NULL(ois_ts))
		err("failed to create a thread for ois fw update\n");

	return;
}
#endif /* CONFIG_OIS_FW_UPDATE_THREAD_USE */

int fimc_is_ois_parse_dt(struct i2c_client *client)
{
	int ret = 0;
	struct fimc_is_device_ois *device = i2c_get_clientdata(client);
	struct fimc_is_ois_gpio *gpio;
	struct device_node *np = client->dev.of_node;

	gpio = &device->gpio;

	ret = of_property_read_string(np, "fimc_is_ois_sda", (const char **) &gpio->sda);
	if (ret) {
		err("ois gpio: fail to read, ois_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_ois_scl",(const char **) &gpio->scl);
	if (ret) {
		err("ois gpio: fail to read, ois_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_ois_pinname",(const char **) &gpio->pinname);
	if (ret) {
		err("ois gpio: fail to read, ois_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	info("[OIS] sda = %s, scl = %s, pinname = %s\n", gpio->sda, gpio->scl, gpio->pinname);

p_err:
	return ret;
}

static int fimc_is_ois_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_device_ois *device;
	struct fimc_is_core *core;
	struct device *i2c_dev;
	struct pinctrl *pinctrl_i2c = NULL;
	int ret = 0;
	struct fimc_is_ois_gpio *gpio;

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed");
		client->dev.init_name = FIMC_IS_OIS_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		return -EINVAL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err("No I2C functionality found\n");
		return -ENODEV;
	}

	device = kzalloc(sizeof(struct fimc_is_device_ois), GFP_KERNEL);
	if (!device) {
		err("fimc_is_device_companion is NULL");
		return -ENOMEM;
	}

	device->ois_hsi2c_status = false;
	core->client1 = client;
	i2c_set_clientdata(client, device);
	fw_sdcard = false;
	core->ois_ver_read = false;
	not_crc_bin = false;

	if (client->dev.of_node) {
		ret = fimc_is_ois_parse_dt(client);
		if (ret) {
			err("parsing device tree is fail(%d)", ret);
			return -ENODEV;
		}
	}

	/* Initial i2c pin */
	i2c_dev = client->dev.parent->parent;
	pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "off_i2c");
	if (IS_ERR_OR_NULL(pinctrl_i2c)) {
		printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
	} else {
		devm_pinctrl_put(pinctrl_i2c);
	}

	gpio = &device->gpio;
	pin_config_set(gpio->pinname, gpio->sda,
		PINCFG_PACK(PINCFG_TYPE_FUNC, 0));
	pin_config_set(gpio->pinname, gpio->scl,
		PINCFG_PACK(PINCFG_TYPE_FUNC, 0));
	pin_config_set(gpio->pinname, gpio->sda,
		PINCFG_PACK(PINCFG_TYPE_PUD, 0));
	pin_config_set(gpio->pinname, gpio->scl,
		PINCFG_PACK(PINCFG_TYPE_PUD, 0));

	return 0;
}

static int fimc_is_ois_remove(struct i2c_client *client)
{
#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
	if (ois_ts) {
		kthread_stop(ois_ts);
		ois_ts = NULL;
	}
#endif

	return 0;
}

static const struct i2c_device_id ois_id[] = {
	{FIMC_IS_OIS_DEV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ois_id);

#ifdef CONFIG_OF
static struct of_device_id ois_dt_ids[] = {
	{ .compatible = "rumba,ois",},
	{},
};
#endif

static struct i2c_driver ois_i2c_driver = {
	.driver = {
		   .name = FIMC_IS_OIS_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ois_dt_ids,
#endif
	},
	.probe = fimc_is_ois_probe,
	.remove = fimc_is_ois_remove,
	.id_table = ois_id,
};
module_i2c_driver(ois_i2c_driver);

MODULE_DESCRIPTION("OIS driver for Rumba");
MODULE_AUTHOR("kyoungho yun <kyoungho.yun@samsung.com>");
MODULE_LICENSE("GPL v2");
