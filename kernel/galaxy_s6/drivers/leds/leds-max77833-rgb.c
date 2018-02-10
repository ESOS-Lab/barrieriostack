/*
 * RGB-led driver for Maxim MAX77833
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/mfd/max77833.h>
#include <linux/mfd/max77833-private.h>
#include <linux/leds-max77833-rgb.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/sec_sysfs.h>

#define SEC_LED_SPECIFIC

/* Registers */
/*defined max77833-private.h*//*
 max77833_led_reg {
	MAX77833_RGBLED_REG_LEDEN           = 0x40,
	MAX77833_RGBLED_REG_LED0BRT         = 0x41,
	MAX77833_RGBLED_REG_LED1BRT         = 0x42,
	MAX77833_RGBLED_REG_LED2BRT         = 0x43,
	MAX77833_RGBLED_REG_LED3BRT         = 0x44,
	MAX77833_RGBLED_REG_LEDRMP          = 0x46,
	MAX77833_RGBLED_REG_LEDBLNK         = 0x45,
	MAX77833_RGBLED_REG_DIMMING         = 0x49,
	MAX77833_LED_REG_END,
};*/

/* MAX77833_REG_LED0BRT */
#define MAX77833_LED0BRT	0xFF

/* MAX77833_REG_LED1BRT */
#define MAX77833_LED1BRT	0xFF

/* MAX77833_REG_LED2BRT */
#define MAX77833_LED2BRT	0xFF

/* MAX77833_REG_LED3BRT */
#define MAX77833_LED3BRT	0xFF

/* MAX77833_REG_LEDBLNK */
#define MAX77833_LEDBLINKD	0xF0
#define MAX77833_LEDBLINKP	0x0F

/* MAX77833_REG_LEDRMP */
#define MAX77833_RAMPUP		0xF0
#define MAX77833_RAMPDN		0x0F

#define LED_R_MASK		0x00FF0000
#define LED_G_MASK		0x0000FF00
#define LED_B_MASK		0x000000FF
#define LED_MAX_CURRENT		0xFF

/* MAX77833_STATE*/
#define LED_DISABLE			0
#define LED_ALWAYS_ON			1
#define LED_BLINK			2

#define LEDBLNK_ON(time)	((time < 100) ? 0 :			\
				(time < 500) ? time/100-1 :		\
				(time < 3250) ? (time-500)/250+4 : 15)

#define LEDBLNK_OFF(time)	((time < 500) ? 0x00 :			\
				(time < 5000) ? time/500 :		\
				(time < 8000) ? (time-5000)/1000+10 :	 \
				(time < 12000) ? (time-8000)/2000+13 : 15)

static u8 led_dynamic_current = 0x14;
static u8 led_lowpower_mode = 0x0;

enum max77833_led_color {
	WHITE,
	RED,
	GREEN,
	BLUE,
};
enum max77833_led_pattern {
	PATTERN_OFF,
	CHARGING,
	CHARGING_ERR,
	MISSED_NOTI,
	LOW_BATTERY,
	FULLY_CHARGED,
	POWERING,
};

static struct device *led_dev;

struct max77833_rgb {
	struct led_classdev led[4];
	struct i2c_client *i2c;
	unsigned int delay_on_times_ms;
	unsigned int delay_off_times_ms;
};

#if defined(CONFIG_LEDS_USE_ED28) && defined(CONFIG_SEC_FACTORY)
extern unsigned int lcdtype;
extern bool jig_status;
#endif

static int max77833_rgb_number(struct led_classdev *led_cdev,
				struct max77833_rgb **p)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(parent);
	int i;

	*p = max77833_rgb;

	for (i = 0; i < 4; i++) {
		if (led_cdev == &max77833_rgb->led[i]) {
			pr_info("leds-max77833-rgb: %s, %d\n", __func__, i);
			return i;
		}
	}

	return -ENODEV;
}

static void max77833_rgb_set(struct led_classdev *led_cdev,
				unsigned int brightness)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;

	ret = max77833_rgb_number(led_cdev, &max77833_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(led_cdev->dev,
			"max77833_rgb_number() returns %d.\n", ret);
		return;
	}

	dev = led_cdev->dev;
	n = ret;

	if (brightness == LED_OFF) {
		/* Flash OFF */
		ret = max77833_update_reg(max77833_rgb->i2c,
					MAX77833_RGBLED_REG_LEDEN, 0 , 3 << (2*n));
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write LEDEN : %d\n", ret);
			return;
		}
	} else {
		/* Set current */
		if (n==BLUE)
			brightness = (brightness * 5) / 2;
		ret = max77833_write_reg(max77833_rgb->i2c,
				MAX77833_RGBLED_REG_LED0BRT + n, brightness);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write LEDxBRT : %d\n", ret);
			return;
		}
		/* Flash ON */
		ret = max77833_update_reg(max77833_rgb->i2c,
				MAX77833_RGBLED_REG_LEDEN, 0x55, 3 << (2*n));
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write FLASH_EN : %d\n", ret);
			return;
		}
	}
}

static void max77833_rgb_set_state(struct led_classdev *led_cdev,
				unsigned int brightness, unsigned int led_state)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;

	pr_info("leds-max77833-rgb: %s\n", __func__);

	ret = max77833_rgb_number(led_cdev, &max77833_rgb);

	if (IS_ERR_VALUE(ret)) {
		dev_err(led_cdev->dev,
			"max77833_rgb_number() returns %d.\n", ret);
		return;
	}

	dev = led_cdev->dev;
	n = ret;

	max77833_rgb_set(led_cdev, brightness);

	ret = max77833_update_reg(max77833_rgb->i2c,
			MAX77833_RGBLED_REG_LEDEN, led_state << (2*n), 0x3 << 2*n);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't write FLASH_EN : %d\n", ret);
		return;
	}
}

static unsigned int max77833_rgb_get(struct led_classdev *led_cdev)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;
	u8 value;

	pr_info("leds-max77833-rgb: %s\n", __func__);

	ret = max77833_rgb_number(led_cdev, &max77833_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(led_cdev->dev,
			"max77833_rgb_number() returns %d.\n", ret);
		return 0;
	}
	n = ret;

	dev = led_cdev->dev;

	/* Get status */
	ret = max77833_read_reg(max77833_rgb->i2c,
				MAX77833_RGBLED_REG_LEDEN, &value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't read LEDEN : %d\n", ret);
		return 0;
	}
	if (!(value & (1 << n)))
		return LED_OFF;

	/* Get current */
	ret = max77833_read_reg(max77833_rgb->i2c,
				MAX77833_RGBLED_REG_LED0BRT + n, &value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't read LED0BRT : %d\n", ret);
		return 0;
	}

	return value;
}

static int max77833_rgb_ramp(struct device *dev, int ramp_up, int ramp_down)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	int value;
	int ret;

	pr_info("leds-max77833-rgb: %s\n", __func__);

	if (ramp_up <= 800) {
		ramp_up /= 100;
	} else {
		ramp_up = (ramp_up - 800) * 2 + 800;
		ramp_up /= 100;
	}

	if (ramp_down <= 800) {
		ramp_down /= 100;
	} else {
		ramp_down = (ramp_down - 800) * 2 + 800;
		ramp_down /= 100;
	}

	value = (ramp_down) | (ramp_up << 4);
	ret = max77833_write_reg(max77833_rgb->i2c,
					MAX77833_RGBLED_REG_LEDRMP, value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't write REG_LEDRMP : %d\n", ret);
		return -ENODEV;
	}

	return 0;
}

static int max77833_rgb_blink(struct device *dev,
				unsigned int delay_on, unsigned int delay_off)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	int value;
	int ret = 0;

	pr_info("leds-max77833-rgb: %s\n", __func__);

	if( delay_on > 3250 || delay_off > 12000 )
		return -EINVAL;
	else {
		value = (LEDBLNK_ON(delay_on) << 4) | LEDBLNK_OFF(delay_off);
		ret = max77833_write_reg(max77833_rgb->i2c,
					MAX77833_RGBLED_REG_LEDBLNK, value);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write REG_LEDBLNK : %d\n", ret);
			return -EINVAL;
		}
	}

	return ret;
}

#ifdef CONFIG_OF
static struct max77833_rgb_platform_data
			*max77833_rgb_parse_dt(struct device *dev)
{
	struct max77833_rgb_platform_data *pdata;
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;
	int ret;
	int i;

	pr_info("leds-max77833-rgb: %s\n", __func__);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(nproot, "rgb");
	if (unlikely(np == NULL)) {
		dev_err(dev, "rgb node not found\n");
		devm_kfree(dev, pdata);
		return ERR_PTR(-EINVAL);
	}

	for (i = 0; i < 4; i++)	{
		ret = of_property_read_string_index(np, "rgb-name", i,
						(const char **)&pdata->name[i]);

		pr_info("leds-max77833-rgb: %s, %s\n", __func__,pdata->name[i]);

		if (IS_ERR_VALUE(ret)) {
			devm_kfree(dev, pdata);
			return ERR_PTR(ret);
		}
	}

	return pdata;
}
#endif

static void max77833_rgb_reset(struct device *dev)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	max77833_rgb_set_state(&max77833_rgb->led[RED], LED_OFF, LED_DISABLE);
	max77833_rgb_set_state(&max77833_rgb->led[GREEN], LED_OFF, LED_DISABLE);
	max77833_rgb_set_state(&max77833_rgb->led[BLUE], LED_OFF, LED_DISABLE);
	max77833_rgb_ramp(dev, 0, 0);
}

static ssize_t store_max77833_rgb_lowpower(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int ret;
	u8 led_lowpower;

	ret = kstrtou8(buf, 0, &led_lowpower);
	if (ret != 0) {
		dev_err(dev, "fail to get led_lowpower.\n");
		return count;
	}

	led_lowpower_mode = led_lowpower;

	dev_dbg(dev, "led_lowpower mode set to %i\n", led_lowpower);

	return count;
}
static ssize_t store_max77833_rgb_brightness(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int ret;
	u8 brightness;
	pr_info("leds-max77833-rgb: %s\n", __func__);

	ret = kstrtou8(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get led_brightness.\n");
		return count;
	}

	led_lowpower_mode = 0;

	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;

	led_dynamic_current = brightness;

	dev_dbg(dev, "led brightness set to %i\n", brightness);

	return count;
}

static ssize_t store_max77833_rgb_pattern(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	unsigned int mode = 0;
	int ret;
	pr_info("leds-max77833-rgb: %s\n", __func__);

	ret = sscanf(buf, "%1d", &mode);
	if (ret == 0) {
		dev_err(dev, "fail to get led_pattern mode.\n");
		return count;
	}

	/* Set all LEDs Off */
	max77833_rgb_reset(dev);
	if (mode == PATTERN_OFF)
		return count;

	/* Set to low power consumption mode */
	if (led_lowpower_mode == 1)
		led_dynamic_current = 0x5;
	else
		led_dynamic_current = 0x14;

	switch (mode) {

	case CHARGING:
	{
		max77833_rgb_set_state(&max77833_rgb->led[RED], led_dynamic_current, LED_ALWAYS_ON);
		break;
	}
	case CHARGING_ERR:
		max77833_rgb_blink(dev, 500, 500);
		max77833_rgb_set_state(&max77833_rgb->led[RED], led_dynamic_current, LED_BLINK);
		break;
	case MISSED_NOTI:
		max77833_rgb_blink(dev, 500, 5000);
		max77833_rgb_set_state(&max77833_rgb->led[BLUE], led_dynamic_current, LED_BLINK);
		break;
	case LOW_BATTERY:
		max77833_rgb_blink(dev, 500, 5000);
		max77833_rgb_set_state(&max77833_rgb->led[RED], led_dynamic_current, LED_BLINK);
		break;
	case FULLY_CHARGED:
		max77833_rgb_set_state(&max77833_rgb->led[GREEN], led_dynamic_current, LED_ALWAYS_ON);
		break;
	case POWERING:
		max77833_rgb_ramp(dev, 800, 800);
		max77833_rgb_blink(dev, 200, 200);
		max77833_rgb_set_state(&max77833_rgb->led[BLUE], led_dynamic_current, LED_ALWAYS_ON);
		max77833_rgb_set_state(&max77833_rgb->led[GREEN], led_dynamic_current, LED_BLINK);
		break;
	default:
		break;
	}

	return count;
}

static ssize_t store_max77833_rgb_blink(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	int led_brightness = 0;
	int delay_on_time = 0;
	int delay_off_time = 0;
	u8 led_r_brightness = 0;
	u8 led_g_brightness = 0;
	u8 led_b_brightness = 0;
	int ret;

	ret = sscanf(buf, "0x%8x %5d %5d", &led_brightness,
					&delay_on_time, &delay_off_time);
	if (ret == 0) {
		dev_err(dev, "fail to get led_blink value.\n");
		return count;
	}

	/*Reset led*/
	max77833_rgb_reset(dev);

	led_r_brightness = (led_brightness & LED_R_MASK) >> 16;
	led_g_brightness = (led_brightness & LED_G_MASK) >> 8;
	led_b_brightness = led_brightness & LED_B_MASK;

	/* In user case, LED current is restricted to less than 2mA */
	led_r_brightness = (led_r_brightness * led_dynamic_current) / LED_MAX_CURRENT;
	led_g_brightness = (led_g_brightness * led_dynamic_current) / LED_MAX_CURRENT;
	led_b_brightness = (led_b_brightness * led_dynamic_current) / LED_MAX_CURRENT;

	if (led_r_brightness) {
		max77833_rgb_set_state(&max77833_rgb->led[RED], led_r_brightness, LED_BLINK);
	}
	if (led_g_brightness) {
		max77833_rgb_set_state(&max77833_rgb->led[GREEN], led_g_brightness, LED_BLINK);
	}
	if (led_b_brightness) {
		max77833_rgb_set_state(&max77833_rgb->led[BLUE], led_b_brightness, LED_BLINK);
	}
	/*Set LED blink mode*/
	max77833_rgb_blink(dev, delay_on_time, delay_off_time);

	pr_info("leds-max77833-rgb: %s\n", __func__);
	dev_dbg(dev, "led_blink is called, Color:0x%X Brightness:%i\n",
			led_brightness, led_dynamic_current);
	return count;
}

static ssize_t store_led_r(struct device *dev,
			struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	char buff[10] = {0,};
	int cnt, ret;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';
	ret = kstrtouint(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		max77833_rgb_set_state(&max77833_rgb->led[RED], brightness, LED_ALWAYS_ON);
	} else {
		max77833_rgb_set_state(&max77833_rgb->led[RED], LED_OFF, LED_DISABLE);
	}
out:
	pr_info("leds-max77833-rgb: %s\n", __func__);
	return count;
}
static ssize_t store_led_g(struct device *dev,
			struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	char buff[10] = {0,};
	int cnt, ret;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';
	ret = kstrtouint(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		max77833_rgb_set_state(&max77833_rgb->led[GREEN], brightness, LED_ALWAYS_ON);
	} else {
		max77833_rgb_set_state(&max77833_rgb->led[GREEN], LED_OFF, LED_DISABLE);
	}
out:
	pr_info("leds-max77833-rgb: %s\n", __func__);
	return count;
}
static ssize_t store_led_b(struct device *dev,
		struct device_attribute *devattr,
		const char *buf, size_t count)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	char buff[10] = {0,};
	int cnt, ret;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';
	ret = kstrtouint(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		max77833_rgb_set_state(&max77833_rgb->led[BLUE], brightness, LED_ALWAYS_ON);
	} else	{
		max77833_rgb_set_state(&max77833_rgb->led[BLUE], LED_OFF, LED_DISABLE);
	}
out:
	pr_info("leds-max77833-rgb: %s\n", __func__);
	return count;
}

/* Added for led common class */
static ssize_t led_delay_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", max77833_rgb->delay_on_times_ms);
}

static ssize_t led_delay_on_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_on\n");
		return count;
	}

	max77833_rgb->delay_on_times_ms = time;

	return count;
}

static ssize_t led_delay_off_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", max77833_rgb->delay_off_times_ms);
}

static ssize_t led_delay_off_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_off\n");
		return count;
	}

	max77833_rgb->delay_off_times_ms = time;

	return count;
}

static ssize_t led_blink_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	const struct device *parent = dev->parent;
	struct max77833_rgb *max77833_rgb_num = dev_get_drvdata(parent);
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	unsigned int blink_set;
	int n = 0;
	int i;

	if (!sscanf(buf, "%1d", &blink_set)) {
		dev_err(dev, "can not write led_blink\n");
		return count;
	}

	if (!blink_set) {
		max77833_rgb->delay_on_times_ms = LED_OFF;
		max77833_rgb->delay_off_times_ms = LED_OFF;
	}

	for (i = 0; i < 4; i++) {
		if (dev == max77833_rgb_num->led[i].dev)
			n = i;
	}

	max77833_rgb_blink(max77833_rgb_num->led[n].dev->parent,
		max77833_rgb->delay_on_times_ms,
		max77833_rgb->delay_off_times_ms);
	max77833_rgb_set_state(&max77833_rgb_num->led[n], led_dynamic_current, LED_BLINK);

	pr_info("leds-max77833-rgb: %s\n", __func__);
	return count;
}

/* permission for sysfs node */
static DEVICE_ATTR(delay_on, 0640, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, 0640, led_delay_off_show, led_delay_off_store);
static DEVICE_ATTR(blink, 0640, NULL, led_blink_store);

#ifdef SEC_LED_SPECIFIC
/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(led_r, 0660, NULL, store_led_r);
static DEVICE_ATTR(led_g, 0660, NULL, store_led_g);
static DEVICE_ATTR(led_b, 0660, NULL, store_led_b);
/* led_pattern node permission is 222 */
/* To access sysfs node from other groups */
static DEVICE_ATTR(led_pattern, 0660, NULL, store_max77833_rgb_pattern);
static DEVICE_ATTR(led_blink, 0660, NULL,  store_max77833_rgb_blink);
static DEVICE_ATTR(led_brightness, 0660, NULL, store_max77833_rgb_brightness);
static DEVICE_ATTR(led_lowpower, 0660, NULL,  store_max77833_rgb_lowpower);
#endif

static struct attribute *led_class_attrs[] = {
	&dev_attr_delay_on.attr,
	&dev_attr_delay_off.attr,
	&dev_attr_blink.attr,
	NULL,
};

static struct attribute_group common_led_attr_group = {
	.attrs = led_class_attrs,
};

#ifdef SEC_LED_SPECIFIC
static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_lowpower.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif
static int max77833_rgb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77833_rgb_platform_data *pdata;
	struct max77833_rgb *max77833_rgb;
	struct max77833_dev *max77833_dev = dev_get_drvdata(dev->parent);
	char temp_name[4][40] = {{0,},}, name[40] = {0,}, *p;
	int i, ret;

	pr_info("leds-max77833-rgb: %s\n", __func__);

#ifdef CONFIG_OF
	pdata = max77833_rgb_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

	max77833_rgb = devm_kzalloc(dev, sizeof(struct max77833_rgb), GFP_KERNEL);
	if (unlikely(!max77833_rgb))
		return -ENOMEM;
	pr_info("leds-max77833-rgb: %s 1 \n", __func__);

	max77833_rgb->i2c = max77833_dev->i2c;

	for (i = 0; i < 4; i++) {
		ret = snprintf(name, 30, "%s", pdata->name[i])+1;
		if (1 > ret)
			goto alloc_err_flash;

		p = devm_kzalloc(dev, ret, GFP_KERNEL);
		if (unlikely(!p))
			goto alloc_err_flash;

		strcpy(p, name);
		strcpy(temp_name[i], name);
		max77833_rgb->led[i].name = p;
		max77833_rgb->led[i].brightness_set = max77833_rgb_set;
		max77833_rgb->led[i].brightness_get = max77833_rgb_get;
		max77833_rgb->led[i].max_brightness = LED_MAX_CURRENT;

		ret = led_classdev_register(dev, &max77833_rgb->led[i]);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "unable to register RGB : %d\n", ret);
			goto alloc_err_flash_plus;
		}
		ret = sysfs_create_group(&max77833_rgb->led[i].dev->kobj,
						&common_led_attr_group);
		if (ret) {
			dev_err(dev, "can not register sysfs attribute\n");
			goto register_err_flash;
		}
	}

	led_dev = sec_device_create(max77833_rgb, "led");
	if (IS_ERR(led_dev)) {
		dev_err(dev, "Failed to create device for samsung specific led\n");
		goto create_err_flash;
	}


	ret = sysfs_create_group(&led_dev->kobj, &sec_led_attr_group);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs group for samsung specific led\n");
		goto device_create_err;
	}

	platform_set_drvdata(pdev, max77833_rgb);
#if defined(CONFIG_LEDS_USE_ED28) && defined(CONFIG_SEC_FACTORY)
	if( lcdtype == 0 && jig_status == false) {
		max77833_rgb_set_state(&max77833_rgb->led[RED], led_dynamic_current, LED_ALWAYS_ON);
	}
#endif
	pr_info("leds-max77833-rgb: %s done\n", __func__);

	return 0;

device_create_err:
	sec_device_destroy(led_dev->devt);
create_err_flash:
    sysfs_remove_group(&led_dev->kobj, &common_led_attr_group);
register_err_flash:
	led_classdev_unregister(&max77833_rgb->led[i]);
alloc_err_flash_plus:
	devm_kfree(dev, temp_name[i]);
alloc_err_flash:
	while (i--) {
		led_classdev_unregister(&max77833_rgb->led[i]);
		devm_kfree(dev, temp_name[i]);
	}
	devm_kfree(dev, max77833_rgb);
	return -ENOMEM;
}

static int max77833_rgb_remove(struct platform_device *pdev)
{
	struct max77833_rgb *max77833_rgb = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < 4; i++)
		led_classdev_unregister(&max77833_rgb->led[i]);

	return 0;
}

static void max77833_rgb_shutdown(struct device *dev)
{
	struct max77833_rgb *max77833_rgb = dev_get_drvdata(dev);
	int i;

	if (!max77833_rgb->i2c)
		return;

	max77833_rgb_reset(dev);

	sysfs_remove_group(&led_dev->kobj, &sec_led_attr_group);

	for (i = 0; i < 4; i++){
		sysfs_remove_group(&max77833_rgb->led[i].dev->kobj,
						&common_led_attr_group);
		led_classdev_unregister(&max77833_rgb->led[i]);
	}
	devm_kfree(dev, max77833_rgb);
}
static struct platform_driver max77833_fled_driver = {
	.driver		= {
		.name	= "leds-max77833-rgb",
		.owner	= THIS_MODULE,
		.shutdown = max77833_rgb_shutdown,
	},
	.probe		= max77833_rgb_probe,
	.remove		= __devexit_p(max77833_rgb_remove),
};

static int __init max77833_rgb_init(void)
{
	pr_info("leds-max77833-rgb: %s\n", __func__);
	return platform_driver_register(&max77833_fled_driver);
}
module_init(max77833_rgb_init);

static void __exit max77833_rgb_exit(void)
{
	platform_driver_unregister(&max77833_fled_driver);
}
module_exit(max77833_rgb_exit);

MODULE_ALIAS("platform:max77833-rgb");
MODULE_AUTHOR("Jeongwoong Lee<jell.lee@samsung.com>");
MODULE_DESCRIPTION("MAX77833 RGB driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
