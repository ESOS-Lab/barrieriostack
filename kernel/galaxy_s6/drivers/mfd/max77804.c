/*
 * max77804.c - mfd core driver for the Maxim 77804
 *
 * Copyright (C) 2011 Samsung Electronics
 * SeoYoung Jeong <seo0.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.c
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77804.h>
#include <linux/mfd/max77804-private.h>
#include <linux/regulator/machine.h>

#if defined(CONFIG_USE_MUIC)
#if defined(CONFIG_MUIC_MAX77804)
#include <linux/muic/max77804-muic.h>
#elif defined(CONFIG_MUIC_MAX77804K)
#include <linux/muic/max77804k-muic.h>
#endif
#endif /* CONFIG_USE_MUIC */

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_PMIC	(0xCC >> 1)	/* Charger, Flash LED */
#define I2C_ADDR_MUIC	(0x4A >> 1)
#define I2C_ADDR_HAPTIC	(0x90 >> 1)

static struct mfd_cell max77804_devs[] = {
#if defined(CONFIG_CHARGER_MAX77804)
	{ .name = "max77804-charger", },
#endif /* CONFIG_CHARGER_MAX77804 */
#if defined(CONFIG_LEDS_MAX77804)
	{ .name = "max77804-led", },
#endif /* CONFIG_MAX77804_LED */
	{ .name = MUIC_DEV_NAME, },
	{ .name = "max77804-safeout", },
#if defined(CONFIG_MOTOR_DRV_MAX77804)
	{ .name = "max77804-haptic", },
#endif /* CONFIG_MAX77804_HAPTIC */
};

int max77804_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77804->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&max77804->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(max77804_read_reg);

int max77804_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77804->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77804->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(max77804_bulk_read);

int max77804_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77804->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&max77804->i2c_lock);
	if (ret < 0)
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(max77804_write_reg);

int max77804_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77804->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77804->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(max77804_bulk_write);

int max77804_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77804->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&max77804->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(max77804_update_reg);

#if defined(CONFIG_OF)
static int of_max77804_dt(struct device *dev, struct max77804_platform_data *pdata)
{
	struct device_node *np_max77804 = dev->of_node;
	const char *prop_str;
	int ret;

	if(!np_max77804)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np_max77804, "max77804,irq-gpio", 0);
	pdata->wakeup = of_property_read_bool(np_max77804, "max77804,wakeup");

	ret = of_property_read_string(np_max77804, "max77804,irqf-trigger", &prop_str);
	if (ret) {
		pr_err("%s: not found irqf_trigger(%d)\n", __func__, ret);
		pdata->irqf_trigger = -1;
		return ret;
	}

	if (!strncasecmp(prop_str, "falling_edge", 7))
		pdata->irqf_trigger = IRQF_TRIGGER_FALLING;
	else if (!strncasecmp(prop_str, "low_level", 3))
		pdata->irqf_trigger = IRQF_TRIGGER_LOW;
	else {
		pr_info("%s: Do not exist irqf-trigger property\n", __func__);
		BUG();
	}

	pr_info("%s: irq-gpio: %u irqf-trigger:%d)\n", __func__, pdata->irq_gpio,
						pdata->irqf_trigger);

	return 0;
}
#else
static int of_max77804_dt(struct device *dev, struct max77804_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int max77804_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct max77804_dev *max77804;
	struct max77804_platform_data *pdata = i2c->dev.platform_data;

	u8 reg_data;
	int ret = 0;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	max77804 = kzalloc(sizeof(struct max77804_dev), GFP_KERNEL);
	if (!max77804) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for max77804\n", __func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct max77804_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory \n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_max77804_dt(&i2c->dev, pdata);
		if (ret < 0){
			dev_err(&i2c->dev, "Failed to get device of_node \n");
			return ret;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	max77804->dev = &i2c->dev;
	max77804->i2c = i2c;
	max77804->irq = i2c->irq;
	if (pdata) {
		max77804->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, MAX77804_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
					MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			max77804->irq_base = pdata->irq_base;

		max77804->irq_gpio = pdata->irq_gpio;
		max77804->irqf_trigger = pdata->irqf_trigger;
		max77804->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&max77804->i2c_lock);

	i2c_set_clientdata(i2c, max77804);

	if (max77804_read_reg(i2c, MAX77804_PMIC_REG_PMIC_ID2, &reg_data) < 0) {
		dev_err(max77804->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err;
	} else {
		/* print rev */
		max77804->pmic_rev = (reg_data & 0x7);
		max77804->pmic_ver = ((reg_data & 0xF8) >> 0x3);
		pr_info("%s:%s device found: rev.0x%x, ver.0x%x\n",
				MFD_DEV_NAME, __func__,
				max77804->pmic_rev, max77804->pmic_ver);
	}

	/* No active discharge on safeout ldo 1,2 */
	max77804_update_reg(i2c, MAX77804_CHG_REG_SAFEOUT_CTRL, 0x00, 0x30);

	max77804->muic = i2c_new_dummy(i2c->adapter, I2C_ADDR_MUIC);
	i2c_set_clientdata(max77804->muic, max77804);

	max77804->haptic = i2c_new_dummy(i2c->adapter, I2C_ADDR_HAPTIC);
	i2c_set_clientdata(max77804->haptic, max77804);

#if defined(CONFIG_MFD_MAX77804)
	ret = max77804_irq_init(max77804);
#elif defined(CONFIG_MFD_MAX77804K)
	ret = max77804k_irq_init(max77804);
#endif

	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(max77804->dev, -1, max77804_devs,
			ARRAY_SIZE(max77804_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(max77804->dev, pdata->wakeup);

	return ret;

err_mfd:
	mfd_remove_devices(max77804->dev);
err_irq_init:
	i2c_unregister_device(max77804->muic);
	i2c_unregister_device(max77804->haptic);
err:
	kfree(max77804);
	return ret;
}

static int max77804_i2c_remove(struct i2c_client *i2c)
{
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);

	mfd_remove_devices(max77804->dev);
	i2c_unregister_device(max77804->muic);
	i2c_unregister_device(max77804->haptic);
	kfree(max77804);

	return 0;
}

static const struct i2c_device_id max77804_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_MAX77804 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77804_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id max77804_i2c_dt_ids[] = {
	{ .compatible = "maxim,max77804" },
	{ },
};
MODULE_DEVICE_TABLE(of, max77804_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int max77804_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(max77804->irq);

	disable_irq(max77804->irq);

	return 0;
}

static int max77804_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(max77804->irq);

	enable_irq(max77804->irq);
#if defined(CONFIG_MFD_MAX77804)
	return max77804_irq_resume(max77804);
#elif defined(CONFIG_MFD_MAX77804K)
	return max77804k_irq_resume(max77804);
#endif
}
#else
#define max77804_suspend	NULL
#define max77804_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_HIBERNATION

u8 max77804_dumpaddr_pmic[] = {
	MAX77804_LED_REG_IFLASH,
	MAX77804_LED_REG_ITORCH,
	MAX77804_LED_REG_ITORCHTORCHTIMER,
	MAX77804_LED_REG_FLASH_TIMER,
	MAX77804_LED_REG_FLASH_EN,
	MAX77804_LED_REG_MAX_FLASH1,
	MAX77804_LED_REG_MAX_FLASH2,
	MAX77804_LED_REG_VOUT_CNTL,
	MAX77804_LED_REG_VOUT_FLASH,
	MAX77804_LED_REG_FLASH_INT_STATUS,

	MAX77804_PMIC_REG_TOPSYS_INT_MASK,
	MAX77804_PMIC_REG_MAINCTRL1,
	MAX77804_PMIC_REG_LSCNFG,
	MAX77804_CHG_REG_CHG_INT_MASK,
	MAX77804_CHG_REG_CHG_CNFG_00,
	MAX77804_CHG_REG_CHG_CNFG_01,
	MAX77804_CHG_REG_CHG_CNFG_02,
	MAX77804_CHG_REG_CHG_CNFG_03,
	MAX77804_CHG_REG_CHG_CNFG_04,
	MAX77804_CHG_REG_CHG_CNFG_05,
	MAX77804_CHG_REG_CHG_CNFG_06,
	MAX77804_CHG_REG_CHG_CNFG_07,
	MAX77804_CHG_REG_CHG_CNFG_08,
	MAX77804_CHG_REG_CHG_CNFG_09,
	MAX77804_CHG_REG_CHG_CNFG_10,
	MAX77804_CHG_REG_CHG_CNFG_11,
	MAX77804_CHG_REG_CHG_CNFG_12,
	MAX77804_CHG_REG_CHG_CNFG_13,
	MAX77804_CHG_REG_CHG_CNFG_14,
	MAX77804_CHG_REG_SAFEOUT_CTRL,
};

u8 max77804_dumpaddr_muic[] = {
	MAX77804_MUIC_REG_INTMASK1,
	MAX77804_MUIC_REG_INTMASK2,
	MAX77804_MUIC_REG_INTMASK3,
	MAX77804_MUIC_REG_CDETCTRL1,
	MAX77804_MUIC_REG_CDETCTRL2,
	MAX77804_MUIC_REG_CTRL1,
	MAX77804_MUIC_REG_CTRL2,
	MAX77804_MUIC_REG_CTRL3,
};


u8 max77804_dumpaddr_haptic[] = {
	MAX77804_HAPTIC_REG_CONFIG1,
	MAX77804_HAPTIC_REG_CONFIG2,
	MAX77804_HAPTIC_REG_CONFIG_CHNL,
	MAX77804_HAPTIC_REG_CONFG_CYC1,
	MAX77804_HAPTIC_REG_CONFG_CYC2,
	MAX77804_HAPTIC_REG_CONFIG_PER1,
	MAX77804_HAPTIC_REG_CONFIG_PER2,
	MAX77804_HAPTIC_REG_CONFIG_PER3,
	MAX77804_HAPTIC_REG_CONFIG_PER4,
	MAX77804_HAPTIC_REG_CONFIG_DUTY1,
	MAX77804_HAPTIC_REG_CONFIG_DUTY2,
	MAX77804_HAPTIC_REG_CONFIG_PWM1,
	MAX77804_HAPTIC_REG_CONFIG_PWM2,
	MAX77804_HAPTIC_REG_CONFIG_PWM3,
	MAX77804_HAPTIC_REG_CONFIG_PWM4,
};

static int max77804_freeze(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < ARRAY_SIZE(max77804_dumpaddr_pmic); i++)
		max77804_read_reg(i2c, max77804_dumpaddr_pmic[i],
				&max77804->reg_pmic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77804_dumpaddr_muic); i++)
		max77804_read_reg(i2c, max77804_dumpaddr_muic[i],
				&max77804->reg_muic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77804_dumpaddr_haptic); i++)
		max77804_read_reg(i2c, max77804_dumpaddr_haptic[i],
				&max77804->reg_haptic_dump[i]);

	disable_irq(max77804->irq);

	return 0;
}

static int max77804_restore(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77804_dev *max77804 = i2c_get_clientdata(i2c);
	int i;

	enable_irq(max77804->irq);

	for (i = 0; i < ARRAY_SIZE(max77804_dumpaddr_pmic); i++)
		max77804_write_reg(i2c, max77804_dumpaddr_pmic[i],
				max77804->reg_pmic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77804_dumpaddr_muic); i++)
		max77804_write_reg(i2c, max77804_dumpaddr_muic[i],
				max77804->reg_muic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77804_dumpaddr_haptic); i++)
		max77804_write_reg(i2c, max77804_dumpaddr_haptic[i],
				max77804->reg_haptic_dump[i]);

	return 0;
}
#endif

const struct dev_pm_ops max77804_pm = {
	.suspend = max77804_suspend,
	.resume = max77804_resume,
#ifdef CONFIG_HIBERNATION
	.freeze =  max77804_freeze,
	.thaw = max77804_restore,
	.restore = max77804_restore,
#endif
};

static struct i2c_driver max77804_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &max77804_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= max77804_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= max77804_i2c_probe,
	.remove		= max77804_i2c_remove,
	.id_table	= max77804_i2c_id,
};

static int __init max77804_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&max77804_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(max77804_i2c_init);

static void __exit max77804_i2c_exit(void)
{
	i2c_del_driver(&max77804_i2c_driver);
}
module_exit(max77804_i2c_exit);

MODULE_DESCRIPTION("MAXIM 77804 multi-function core driver");
MODULE_AUTHOR("SeoYoung, Jeong <seo0.jeong@samsung.com>");
MODULE_LICENSE("GPL");
