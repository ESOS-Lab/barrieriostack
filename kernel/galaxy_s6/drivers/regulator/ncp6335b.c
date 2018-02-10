/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#ifdef CONFIG_SOC_EXYNOS5422
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#endif

#define NCP6335B_DRV_NAME "ncp6335b"

struct ncp6335b_dev {
	struct i2c_client *client;
	struct mutex lock;
};

struct ncp6335b_platform_data {
	void	*data;
};

static int ncp6335b_set_voltage(struct i2c_client *client)
{
	int ret = i2c_smbus_write_byte_data(client, 0x14, 0x00);
	if (ret < 0)
		pr_err("[%s::%d]Write Error [%d]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_write_byte_data(client, 0x10, 0xC0); /* 1.05V -> 1V (0xC0) */
	if (ret < 0)
		pr_err("[%s::%d]Write Error [%d]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_write_byte_data(client, 0x11, 0xC0); /* 1.05V -> 1V (0xC0) */
	if (ret < 0)
		pr_err("[%s::%d]Write Error [%d]\n", __func__, __LINE__, ret);

	return ret;
}

static int ncp6335b_read_voltage(struct i2c_client *client)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, 0x3);
	if (ret < 0)
		pr_err("[%s::%d]Read Error [%d]\n", __func__, __LINE__, ret);
	pr_err("[%s::%d]NCP6335B PID[%x]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_read_byte_data(client, 0x10);
	if (ret < 0)
		pr_err("[%s::%d]Read Error [%d]\n", __func__, __LINE__, ret);
	pr_err("[%s::%d]NCP6335B [0x10 Read :: %x]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_read_byte_data(client, 0x11);
	if (ret < 0)
		pr_err("[%s::%d]Read Error [%d]\n", __func__, __LINE__, ret);
	pr_err("[%s::%d]NCP6335B [0x11 Read :: %x]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_read_byte_data(client, 0x14);
	if (ret < 0)
		pr_err("[%s::%d]Read Error [%d]\n", __func__, __LINE__, ret);
	pr_err("[%s::%d]NCP6335B [0x14 Read :: %x]\n", __func__, __LINE__, ret);

	return ret;
}

#ifdef CONFIG_OF
static int ncp6335b_parse_dt(struct device *dev,
		struct ncp6335b_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	char *str = NULL;
	int ret;

	ret = of_property_read_string(np, "ncp6335b,dev_name", (const char **)&str);
	if (ret) {
		pr_err("ncp6335b: fail to read, ncp6335b_parse_dt\n");
		return -ENODEV;
	}

	if (str)
		pr_devel("ncp6335b: DT dev name = %s\n", str);

	dev->platform_data = pdata;

	return 0;
}
#else
static int ncp6335b_parse_dt(struct device *dev,
		struct ncp6335b_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int __devinit ncp6335b_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct ncp6335b_platform_data *pdata;
	struct ncp6335b_dev *dev = NULL;
#ifdef CONFIG_SOC_EXYNOS5422
	struct regulator *regulator = NULL;
	const char power_name[] = "CAM_IO_1.8V_AP";
#endif
	int ret = 0;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct ncp6335b_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = ncp6335b_parse_dt(&client->dev, pdata);
		if (ret) {
			pr_err("%s: ncp6335b parse dt failed\n", __func__);
			return ret;
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		pr_err("%s: missing platform data\n", __func__);
		ret =  -ENODEV;
		goto pdata_err;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: SMBUS Byte Data not Supported\n", __func__);
		ret =  -EIO;
		goto pdata_err;
	}

	dev = (struct ncp6335b_dev *)kzalloc(sizeof(*dev), GFP_KERNEL);
	if (unlikely(!dev)) {
		pr_err("%s: failed to alloc memory\n", __func__);
		ret =  -ENOMEM;
		goto pdata_err;
	}

	dev->client = client;

	mutex_init(&dev->lock);

#ifdef CONFIG_SOC_EXYNOS5422
	regulator = regulator_get(NULL, power_name);
	if (IS_ERR(regulator)) {
		pr_err("%s : regulator_get(%s) fail\n", __func__, power_name);
		goto err;
	}

	if (regulator_is_enabled(regulator)) {
		pr_info("%s regulator is already enabled\n", power_name);
	} else {
		ret = regulator_enable(regulator);
		if (unlikely(ret)) {
			pr_err("%s : regulator_enable(%s) fail\n", __func__, power_name);
			goto err;
		}
	}
	usleep_range(1000, 1000);
#endif

	/*ret = ncp6335b_read_voltage(dev->client);
	if (ret < 0) {
		pr_err("%s: error, first read\n", __func__);
		goto err;
	}*/

	ret = ncp6335b_set_voltage(dev->client);
	if (ret < 0) {
		pr_err("%s: error, fail to set voltage\n", __func__);
		goto err;
	}

	ret = ncp6335b_read_voltage(dev->client);
	if (ret < 0) {
		pr_err("%s: error, fail to read voltage\n", __func__);
		goto err;
	}

#ifdef CONFIG_SOC_EXYNOS5422
	ret = regulator_disable(regulator);
	if (unlikely(ret)) {
		pr_err("%s: regulator_disable(%s) fail\n", __func__, power_name);
		goto err;
	}
	regulator_put(regulator);
#endif

	i2c_set_clientdata(client, dev);

	pr_info("%s %s: ncp6335b probed\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;

err:
	mutex_destroy(&dev->lock);
	kfree((void *)dev);
pdata_err:
	devm_kfree(&client->dev, (void *)pdata);
#ifdef CONFIG_SOC_EXYNOS5422
	if (!IS_ERR_OR_NULL(regulator)) {
		ret = regulator_disable(regulator);
		if (unlikely(ret)) {
			pr_err("%s: regulator_disable(%s) fail\n", __func__, power_name);
		}
		regulator_put(regulator);
	}
#endif
	return ret;
}

static int __devexit ncp6335b_remove(struct i2c_client *client)
{
	struct ncp6335b_dev *dev = i2c_get_clientdata(client);

	kfree(dev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id ncp6335b_dt_ids[] = {
	{ .compatible = "ncp6335b",},
	{},
};
/*MODULE_DEVICE_TABLE(of, ncp6335b_dt_ids);*/
#endif

static const struct i2c_device_id ncp6335b_id[] = {
	{NCP6335B_DRV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ncp6335b_id);

static struct i2c_driver ncp6335b_driver = {
	.driver = {
		.name = NCP6335B_DRV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ncp6335b_dt_ids,
#endif
	},
	.probe = ncp6335b_probe,
	.remove = ncp6335b_remove,
	.id_table = ncp6335b_id,
};

static int __init ncp6335b_init_module(void)
{
	int ret;

	/* pr_info("%s: ncp6335b init\n", __func__); */

	ret = i2c_add_driver(&ncp6335b_driver);
	if (ret)
		pr_err("%s: fail to add driver, %d\n", __func__, ret);

	return ret;
}

static void __exit ncp6335b_exit_module(void)
{
	/*pr_info("%s: ncp6335b exit\n", __func__);*/
	i2c_del_driver(&ncp6335b_driver);
}

module_init(ncp6335b_init_module);
module_exit(ncp6335b_exit_module);


MODULE_DESCRIPTION("ncp6335b");
MODULE_LICENSE("GPL v2");
