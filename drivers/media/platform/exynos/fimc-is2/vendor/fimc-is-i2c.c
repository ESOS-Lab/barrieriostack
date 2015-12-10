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

#include <linux/module.h>
#include <linux/i2c.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#include "fimc-is-core.h"

static int fimc_is_i2c0_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_core *core;

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed(i2c0)");
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		panic("core is NULL");

	core->client0 = client;

	pr_info("%s %s: fimc_is_i2c0 driver probed!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;
}

static int fimc_is_i2c0_remove(struct i2c_client *client)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id fimc_is_i2c0_dt_ids[] = {
	{ .compatible = "samsung,fimc_is_i2c0",},
	{},
};
MODULE_DEVICE_TABLE(of, fimc_is_i2c0_dt_ids);
#endif

static const struct i2c_device_id fimc_is_i2c0_id[] = {
	{"fimc_is_i2c0", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fimc_is_i2c0_id);

static struct i2c_driver fimc_is_i2c0_driver = {
	.driver = {
		.name = "fimc_is_i2c0",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = fimc_is_i2c0_dt_ids,
#endif
	},
	.probe = fimc_is_i2c0_probe,
	.remove = fimc_is_i2c0_remove,
	.id_table = fimc_is_i2c0_id,
};
module_i2c_driver(fimc_is_i2c0_driver);

MODULE_DESCRIPTION("FIMC-IS I2C driver");
MODULE_LICENSE("GPL");
