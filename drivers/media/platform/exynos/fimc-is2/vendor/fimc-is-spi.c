/*
 * driver for FIMC-IS SPI
 *
 * Copyright (c) 2011, Samsung Electronics. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/pinctrl/pinctrl-samsung.h>

#include "fimc-is-core.h"
#include "fimc-is-dt.h"
#include "fimc-is-regs.h"

#define STREAM_TO_U16(var16, p)	{(var16) = ((u16)(*((u8 *)p+1)) + \
				((u8)(*((u8 *)p) << 8))); }

int fimc_is_spi_s_pin(struct fimc_is_spi_gpio *spi_gpio, int pincfg_type, int value)
{
	if (value > 0xff) {
		err("value(%d) should be smaller than 0xff", value);
		return -EINVAL;
	}

	if (pincfg_type > PINCFG_TYPE_NUM) {
		err("pincfg_type(%d) should be smaller than %d", pincfg_type, PINCFG_TYPE_NUM);
		return -EINVAL;
	}

	pin_config_set(spi_gpio->pinname, spi_gpio->clk,
			PINCFG_PACK(pincfg_type, value));
	pin_config_set(spi_gpio->pinname, spi_gpio->miso,
			PINCFG_PACK(pincfg_type, value));
	pin_config_set(spi_gpio->pinname, spi_gpio->mosi,
			PINCFG_PACK(pincfg_type, value));
	pin_config_set(spi_gpio->pinname, spi_gpio->ssn,
			PINCFG_PACK(pincfg_type, value));

	return 0;
}

void fimc_is_spi_s_port(struct fimc_is_spi_gpio *spi_gpio, int func, bool ssn)
{
	pin_config_set(spi_gpio->pinname, spi_gpio->clk,
			PINCFG_PACK(PINCFG_TYPE_FUNC, func));
	pin_config_set(spi_gpio->pinname, spi_gpio->miso,
			PINCFG_PACK(PINCFG_TYPE_FUNC, func));
	pin_config_set(spi_gpio->pinname, spi_gpio->mosi,
			PINCFG_PACK(PINCFG_TYPE_FUNC, func));
	if (ssn == true) {
		pin_config_set(spi_gpio->pinname, spi_gpio->ssn,
				PINCFG_PACK(PINCFG_TYPE_FUNC, func));
	}
}

int fimc_is_spi_reset(struct fimc_is_spi *spi)
{
	int ret = 0;
	unsigned char req_rst[1] = { 0x99 };
	struct spi_transfer t_c;
	struct spi_transfer t_r;
	struct spi_message m;

	BUG_ON(!spi->device);

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	t_c.tx_buf = req_rst;
	t_c.len = 1;
	t_c.cs_change = 0;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);

	ret = spi_sync(spi->device, &m);
	if (ret) {
		err("spi sync error - can't get device information");
		ret = -EIO;
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_spi_read(struct fimc_is_spi *spi, void *buf, u32 addr, size_t size)
{
	int ret = 0;
	unsigned char req_data[4];
	struct spi_transfer t_c;
	struct spi_transfer t_r;
	struct spi_message m;

	BUG_ON(!spi->device);

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	req_data[0] = 0x3;
	req_data[1] = (addr & 0xFF0000) >> 16;
	req_data[2] = (addr & 0xFF00) >> 8;
	req_data[3] = (addr & 0xFF);

	t_c.tx_buf = req_data;
	t_c.len = 4;
	t_c.cs_change = 1;
	t_c.bits_per_word = 32;

	t_r.rx_buf = buf;
	t_r.len = (u32)size;
	t_r.cs_change = 0;

	switch (size % 4) {
	case 0:
		t_r.bits_per_word = 32;
		break;
	case 2:
		t_r.bits_per_word = 16;
		break;
	default:
		t_r.bits_per_word = 8;
		break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);
	spi_message_add_tail(&t_r, &m);

	ret = spi_sync(spi->device, &m);
	if (ret) {
		err("spi sync error - can't read data");
		ret = -EIO;
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_spi_read_module_id(struct fimc_is_spi *spi, void *buf, u16 addr, size_t size)
{
	unsigned char req_data[4] = { 0x90,  };
	int ret;

	struct spi_transfer t_c;
	struct spi_transfer t_r;

	struct spi_message m;

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	req_data[1] = (addr & 0xFF00) >> 8;
	req_data[2] = (addr & 0xFF);

	t_c.tx_buf = req_data;
	t_c.len = 4;
	t_c.cs_change = 1;
	t_c.bits_per_word = 32;

	t_r.rx_buf = buf;
	t_r.len = (u32)size;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);
	spi_message_add_tail(&t_r, &m);

	spi->device->max_speed_hz = 48000000;
	ret = spi_sync(spi->device, &m);
	if (ret) {
		err("spi sync error - can't read data");
		return -EIO;
	} else {
		return 0;
	}
}

static int fimc_is_spi_probe(struct spi_device *device)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_spi *spi;

	dbg_core("%s\n", __func__);

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed(spi)");
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* spi->bits_per_word = 16; */
	if (spi_setup(device)) {
		pr_err("failed to setup spi for fimc_is_spi\n");
		ret = -EINVAL;
		goto p_err;
	}

	if (!strncmp(device->modalias, "fimc_is_spi0", 12)) {
		spi = &core->spi0;
		spi->node = "samsung,fimc_is_spi0";
		spi->device = device;
		ret = fimc_is_spi_parse_dt(spi);
		if (ret) {
			pr_err("[%s] of_fimc_is_spi_dt parse dt failed\n", __func__);
			return ret;
		}
	}

	if (!strncmp(device->modalias, "fimc_is_spi1", 12)) {
		spi = &core->spi1;
		spi->node = "samsung,fimc_is_spi1";
		spi->device = device;
		ret = fimc_is_spi_parse_dt(spi);
		if (ret) {
			pr_err("[%s] of_fimc_is_spi_dt parse dt failed\n", __func__);
			return ret;
		}
	}

p_err:
	info("[SPI] %s(%s):%d\n", __func__, device->modalias, ret);
	return ret;
}

static int fimc_is_spi_remove(struct spi_device *spi)
{
	return 0;
}


static const struct of_device_id exynos_fimc_is_spi_match[] = {
	{
		.compatible = "samsung,fimc_is_spi0",
	},
	{
		.compatible = "samsung,fimc_is_spi1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_spi_match);

static struct spi_driver fimc_is_spi0_driver = {
	.driver = {
		.name = "fimc_is_spi0",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = exynos_fimc_is_spi_match,
	},
	.probe	= fimc_is_spi_probe,
	.remove	= fimc_is_spi_remove,
};

module_spi_driver(fimc_is_spi0_driver);

static struct spi_driver fimc_is_spi1_driver = {
	.driver = {
		.name = "fimc_is_spi1",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = exynos_fimc_is_spi_match,
	},
	.probe	= fimc_is_spi_probe,
	.remove = fimc_is_spi_remove,
};

module_spi_driver(fimc_is_spi1_driver);

MODULE_DESCRIPTION("FIMC-IS SPI driver");
MODULE_LICENSE("GPL");
