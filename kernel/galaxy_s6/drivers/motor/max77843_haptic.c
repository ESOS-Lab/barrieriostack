/*
 * haptic motor driver for max77843 - max77673_haptic.c
 *
 * Copyright (C) 2011 ByungChang Cha <bc.cha@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max77843.h>
#include <linux/mfd/max77843-private.h>
#include <plat/devs.h>
#include <linux/sec_sysfs.h>

#define TEST_MODE_TIME 10000

#define MOTOR_LRA			(1<<7)
#define MOTOR_EN			(1<<6)
#define EXT_PWM				(0<<5)
#define DIVIDER_128			(1<<1)
#define MAX77843_REG_MAINCTRL1_MRDBTMER_MASK	(0x7)
#define MAX77843_REG_MAINCTRL1_MREN		(1 << 3)
#define MAX77843_REG_MAINCTRL1_BIASEN		(1 << 7)

void (*vibtonz_en)(bool);
void (*vibtonz_pwm)(int);

static struct device *motor_dev;

struct max77843_haptic_data {
	struct max77843_dev *max77843;
	struct i2c_client *i2c;
	struct max77843_haptic_platform_data *pdata;

	struct pwm_device *pwm;
	struct regulator *regulator;
	struct timed_output_dev tout_dev;
	struct hrtimer timer;
	unsigned int timeout;

	struct workqueue_struct *workqueue;
	struct work_struct work;
	spinlock_t lock;
	bool running;
	bool resumed;
};

struct max77843_haptic_data *g_hap_data;

static void max77843_haptic_i2c(struct max77843_haptic_data *hap_data, bool en)
{
	int ret;
	u8 lscnfg_val = 0x00;

	pr_info("[VIB] %s %d\n", __func__, en);

	if (en) {
		lscnfg_val = MAX77843_REG_MAINCTRL1_BIASEN;
	}

	ret = max77843_update_reg(hap_data->i2c, MAX77843_PMIC_REG_MAINCTRL1,
			lscnfg_val, MAX77843_REG_MAINCTRL1_BIASEN);
	if (ret)
		pr_err("[VIB] i2c REG_BIASEN update error %d\n", ret);

	if (en) {
		ret = max77843_update_reg(hap_data->i2c,
				MAX77843_PMIC_REG_MCONFIG, 0xff, MOTOR_EN);
	} else {
		ret = max77843_update_reg(hap_data->i2c,
				MAX77843_PMIC_REG_MCONFIG, 0x0, MOTOR_EN);
	}
	if (ret)
		pr_err("[VIB] i2c MOTOR_EN update error %d\n", ret);

	ret = max77843_update_reg(hap_data->i2c, MAX77843_PMIC_REG_MCONFIG,
			0xff, MOTOR_LRA);
	if (ret)
		pr_err("[VIB] i2c MOTOR_LPA update error %d\n", ret);
}

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct max77843_haptic_data *hap_data
		= container_of(tout_dev, struct max77843_haptic_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	return 0;
}

static void haptic_enable(struct timed_output_dev *tout_dev, int value)
{
	struct max77843_haptic_data *hap_data
		= container_of(tout_dev, struct max77843_haptic_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	unsigned long flags;


	cancel_work_sync(&hap_data->work);
	hrtimer_cancel(timer);
	hap_data->timeout = value;
	queue_work(hap_data->workqueue, &hap_data->work);
	spin_lock_irqsave(&hap_data->lock, flags);
	if (value > 0 && value != TEST_MODE_TIME) {
		pr_debug("[VIB] %s value %d\n", __func__, value);
		value = min(value, (int)hap_data->pdata->max_timeout);
		hrtimer_start(timer, ns_to_ktime((u64)value * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&hap_data->lock, flags);
}

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct max77843_haptic_data *hap_data
		= container_of(timer, struct max77843_haptic_data, timer);

	hap_data->timeout = 0;
	queue_work(hap_data->workqueue, &hap_data->work);
	return HRTIMER_NORESTART;
}

static int vibetonz_clk_on(struct device *dev, bool en)
{
	struct clk *vibetonz_clk = NULL;

#if defined(CONFIG_OF)
	struct device_node *np;

	np = of_find_node_by_name(NULL,"pwm");
	if (np == NULL) {
		pr_err("[VIB] %s : pwm error to get dt node\n", __func__);
		return -EINVAL;
	}

	vibetonz_clk = of_clk_get_by_name(np, "gate_timers");
	if (!vibetonz_clk) {
		pr_info("[VIB] %s fail to get the vibetonz_clk\n", __func__);
		return -EINVAL;
	}
#else
	vibetonz_clk = clk_get(dev, "timers");
#endif
	pr_info("[VIB] DEV NAME %s %lu\n",
			dev_name(dev), clk_get_rate(vibetonz_clk));

	if (IS_ERR(vibetonz_clk)) {
		pr_err("[VIB] failed to get clock for the motor\n");
		goto err_clk_get;
	}

	if (en)
		clk_enable(vibetonz_clk);
	else
		clk_disable(vibetonz_clk);

	clk_put(vibetonz_clk);
	return 0;

err_clk_get:
	clk_put(vibetonz_clk);
	return -EINVAL;
}

static void haptic_work(struct work_struct *work)
{
	struct max77843_haptic_data *hap_data
		= container_of(work, struct max77843_haptic_data, work);

	pr_info("[VIB] %s\n", __func__);
	if (hap_data->timeout > 0) {
		if (hap_data->running)
			return;

		max77843_haptic_i2c(hap_data, true);

		pwm_config(hap_data->pwm, hap_data->pdata->duty,
				hap_data->pdata->period);
		pwm_enable(hap_data->pwm);
		if (hap_data->pdata->motor_en)
			hap_data->pdata->motor_en(true);
		else {
			int ret;
			ret = regulator_enable(hap_data->regulator);
			pr_info("regulator_enable returns %d\n", ret);
		}
		hap_data->running = true;
	} else {
		if (!hap_data->running)
			return;
		if (hap_data->pdata->motor_en)
			hap_data->pdata->motor_en(false);
		else
			regulator_disable(hap_data->regulator);
		pwm_disable(hap_data->pwm);

		max77843_haptic_i2c(hap_data, false);

		hap_data->running = false;
	}
	return;
}

#ifdef CONFIG_VIBETONZ
void max77843_vibtonz_en(bool en)
{

	if (g_hap_data == NULL) {
		pr_err("[VIB] the motor is not ready!!!");
		return ;
	}

	if (en) {
		if (g_hap_data->running)
			return;

		max77843_haptic_i2c(g_hap_data, true);

		//must set pwm after resume. this may be workaround..
		if(g_hap_data->resumed)
		{
			pwm_config(g_hap_data->pwm, g_hap_data->pdata->period/2,
						g_hap_data->pdata->period);
			g_hap_data->resumed = false;
		}

		pwm_enable(g_hap_data->pwm);
		if (g_hap_data->pdata->motor_en)
			g_hap_data->pdata->motor_en(true);
		else {
			int ret;
			ret = regulator_enable(g_hap_data->regulator);
			pr_info("regulator_enable returns %d\n", ret);
		}
		g_hap_data->running = true;
	} else {
		if (!g_hap_data->running)
			return;
		if (g_hap_data->pdata->motor_en)
			g_hap_data->pdata->motor_en(false);
		else
			regulator_disable(g_hap_data->regulator);
		pwm_disable(g_hap_data->pwm);

		max77843_haptic_i2c(g_hap_data, false);

		g_hap_data->running = false;
	}
}

void max77843_vibtonz_pwm(int nForce)
{
	/* add to avoid the glitch issue */
	static int prev_duty;
	int pwm_period = 0, pwm_duty = 0;

	if (g_hap_data == NULL) {
		printk(KERN_ERR "[VIB] the motor is not ready!!!");
		return ;
	}

	pwm_period = g_hap_data->pdata->period;
	pwm_duty = pwm_period / 2 + ((pwm_period / 2 - 2) * nForce) / 127;

	if (pwm_duty > g_hap_data->pdata->duty)
		pwm_duty = g_hap_data->pdata->duty;
	else if (pwm_period - pwm_duty > g_hap_data->pdata->duty)
		pwm_duty = pwm_period - g_hap_data->pdata->duty;

	/* add to avoid the glitch issue */
	if (prev_duty != pwm_duty) {
		prev_duty = pwm_duty;
		pwm_config(g_hap_data->pwm, pwm_duty, pwm_period);
	}
}
#endif

#if defined(CONFIG_OF)
static struct max77843_haptic_platform_data *of_max77843_haptic_dt(struct device *dev)
{
	struct device_node *np_root = dev->parent->of_node;
	struct device_node *np_haptic;
	struct max77843_haptic_platform_data *pdata;
	u32 temp;
	const char *temp_str;
	int ret;

	pdata = kzalloc(sizeof(struct max77843_haptic_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		return NULL;
	}

	printk("%s : start dt parsing\n", __func__);

	np_haptic = of_find_node_by_name(np_root, "haptic");
	if (np_haptic == NULL) {
		pr_err("[VIB] %s : error to get dt node\n", __func__);
		goto err_parsing_dt;
	}

	ret = of_property_read_u32(np_haptic, "haptic,max_timeout", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node max_timeout\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->max_timeout = (u16)temp;

	ret = of_property_read_u32(np_haptic, "haptic,duty", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node duty\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->duty = (u16)temp;

	ret = of_property_read_u32(np_haptic, "haptic,period", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node period\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->period = (u16)temp;

	ret = of_property_read_u32(np_haptic, "haptic,pwm_id", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node pwm_id\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->pwm_id = (u16)temp;

	ret = of_property_read_string(np_haptic, "haptic,regulator_name", &temp_str);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node regulator_name\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->regulator_name = (char *)temp_str;


	/* debugging */
	printk("%s : max_timeout = %d\n", __func__, pdata->max_timeout);
	printk("%s : duty = %d\n", __func__, pdata->duty);
	printk("%s : period = %d\n", __func__, pdata->period);
	printk("%s : pwm_id = %d\n", __func__, pdata->pwm_id);
	printk("%s : regulator_name = %s\n", __func__, pdata->regulator_name);

	pdata->init_hw = NULL;
	pdata->motor_en = NULL;

	return pdata;

err_parsing_dt:
	kfree(pdata);
	return NULL;
}
#endif
static ssize_t store_duty(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	char buff[10] = {0,};
	int cnt, ret;
	u16 duty;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou16(buff, 0, &duty);
	if (ret != 0) {
		dev_err(dev, "[VIB] fail to get duty.\n");
		return count;
	}
	g_hap_data->pdata->duty = (u16)duty;

	return count;
}

static ssize_t store_period(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	char buff[10] = {0,};
	int cnt, ret;
	u16 period;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou16(buff, 0, &period);
	if (ret != 0) {
		dev_err(dev, "[VIB] fail to get period.\n");
		return count;
	}
	g_hap_data->pdata->period = (u16)period;

	return count;
}

static ssize_t show_duty_period(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "duty: %u, period%u\n", g_hap_data->pdata->duty,
						g_hap_data->pdata->period);
}

/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(set_duty, 0220, NULL, store_duty);
static DEVICE_ATTR(set_period, 0220, NULL, store_period);
static DEVICE_ATTR(show_duty_period, 0440, show_duty_period, NULL);

static struct attribute *sec_motor_attributes[] = {
	&dev_attr_set_duty.attr,
	&dev_attr_set_period.attr,
	&dev_attr_show_duty_period.attr,
	NULL,
};

static struct attribute_group sec_motor_attr_group = {
	.attrs = sec_motor_attributes,
};

static int max77843_haptic_probe(struct platform_device *pdev)
{
	int error = 0;
	struct max77843_dev *max77843 = dev_get_drvdata(pdev->dev.parent);
	struct max77843_platform_data *max77843_pdata
		= dev_get_platdata(max77843->dev);
	struct max77843_haptic_platform_data *pdata
		= max77843_pdata->haptic_data;
	struct max77843_haptic_data *hap_data;

	pr_info("[VIB] ++ %s\n", __func__);

#if defined(CONFIG_OF)
	if (pdata == NULL) {
		pdata = of_max77843_haptic_dt(&pdev->dev);
		if (!pdata) {
			pr_err("[VIB] max77843-haptic : %s not found haptic dt!\n",
					__func__);
			return -1;
		}
	}
#else
	if (pdata == NULL) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}
#endif /* CONFIG_OF */

	hap_data = kzalloc(sizeof(struct max77843_haptic_data), GFP_KERNEL);
	if (!hap_data) {
		kfree(pdata);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, hap_data);
	g_hap_data = hap_data;
	hap_data->max77843 = max77843;
	hap_data->i2c = max77843->i2c;
	hap_data->pdata = pdata;

	hap_data->workqueue = create_singlethread_workqueue("hap_work");
	if (NULL == hap_data->workqueue) {
		error = -EFAULT;
		pr_err("[VIB] Failed to create workqueue, err num: %d\n", error);
		goto err_work_queue;
	}
	INIT_WORK(&(hap_data->work), haptic_work);
	spin_lock_init(&(hap_data->lock));

	hap_data->pwm = pwm_request(hap_data->pdata->pwm_id, "vibrator");
	if (IS_ERR(hap_data->pwm)) {
		error = -EFAULT;
		pr_err("[VIB] Failed to request pwm, err num: %d\n", error);
		goto err_pwm_request;
	}

	pwm_config(hap_data->pwm, pdata->period / 2, pdata->period);

	vibetonz_clk_on(&pdev->dev, true);
	if (pdata->init_hw)
		pdata->init_hw();
	else
		hap_data->regulator
			= regulator_get(NULL, pdata->regulator_name);

	if (IS_ERR(hap_data->regulator)) {
		error = -EFAULT;
		pr_err("[VIB] Failed to get vmoter regulator, err num: %d\n", error);
		goto err_regulator_get;
	}
	/* hrtimer init */
	hrtimer_init(&hap_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap_data->timer.function = haptic_timer_func;

	/* timed_output_dev init*/
	hap_data->tout_dev.name = "vibrator";
	hap_data->tout_dev.get_time = haptic_get_time;
	hap_data->tout_dev.enable = haptic_enable;

	hap_data->resumed = false;

	motor_dev = sec_device_create(hap_data, "motor");
	if (IS_ERR(motor_dev)) {
		error = -ENODEV;
		pr_err("[VIB] Failed to create device\
				for samsung specific motor, err num: %d\n", error);
		goto exit_sec_devices;
	}
	error = sysfs_create_group(&motor_dev->kobj, &sec_motor_attr_group);
	if (error) {
		error = -ENODEV;
		pr_err("[VIB] Failed to create sysfs group\
				for samsung specific motor, err num: %d\n", error);
		goto exit_sysfs;
	}

#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	error = timed_output_dev_register(&hap_data->tout_dev);
	if (error < 0) {
		error = -EFAULT;
		pr_err("[VIB] Failed to register timed_output : %d\n", error);
		goto err_timed_output_register;
	}
#endif

	vibtonz_en = max77843_vibtonz_en;
	vibtonz_pwm = max77843_vibtonz_pwm;

	pr_debug("[VIB] -- %s\n", __func__);

	return error;

err_timed_output_register:
	sysfs_remove_group(&motor_dev->kobj, &sec_motor_attr_group);
exit_sysfs:
	sec_device_destroy(motor_dev->devt);
exit_sec_devices:
	regulator_put(hap_data->regulator);
err_regulator_get:
	pwm_free(hap_data->pwm);
err_pwm_request:
	destroy_workqueue(hap_data->workqueue);
err_work_queue:
	kfree(hap_data);
	kfree(pdata);
	g_hap_data = NULL;
	return error;
}

static int __devexit max77843_haptic_remove(struct platform_device *pdev)
{
	struct max77843_haptic_data *data = platform_get_drvdata(pdev);
#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	timed_output_dev_unregister(&data->tout_dev);
#endif

	sysfs_remove_group(&motor_dev->kobj, &sec_motor_attr_group);
	sec_device_destroy(motor_dev->devt);
	regulator_put(data->regulator);
	pwm_free(data->pwm);
	destroy_workqueue(data->workqueue);
	max77843_haptic_i2c(data, false);
	kfree(data);
	g_hap_data = NULL;

	return 0;
}

static int max77843_haptic_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct max77843_haptic_data *data = platform_get_drvdata(pdev);
	pr_info("[VIB] %s\n", __func__);
	cancel_work_sync(&g_hap_data->work);
	hrtimer_cancel(&g_hap_data->timer);
	vibetonz_clk_on(&pdev->dev, false);
	max77843_haptic_i2c(data, false);
	return 0;
}
static int max77843_haptic_resume(struct platform_device *pdev)
{
	pr_info("[VIB] %s\n", __func__);
	vibetonz_clk_on(&pdev->dev, true);
	g_hap_data->resumed = true;
	return 0;
}

static struct platform_driver max77843_haptic_driver = {
	.probe		= max77843_haptic_probe,
	.remove		= max77843_haptic_remove,
	.suspend	= max77843_haptic_suspend,
	.resume		= max77843_haptic_resume,
	.driver = {
		.name	= "max77843-haptic",
		.owner	= THIS_MODULE,
	},
};

static int __init max77843_haptic_init(void)
{
	pr_debug("[VIB] %s\n", __func__);
	return platform_driver_register(&max77843_haptic_driver);
}
module_init(max77843_haptic_init);

static void __exit max77843_haptic_exit(void)
{
	platform_driver_unregister(&max77843_haptic_driver);
}
module_exit(max77843_haptic_exit);

MODULE_AUTHOR("ByungChang Cha <bc.cha@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("max77843 haptic driver");
