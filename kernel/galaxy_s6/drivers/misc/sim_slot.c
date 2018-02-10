/*

 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.


*/

#include <linux/module.h>

#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/i2c-gpio.h>
#include <asm/gpio.h>

/* GPIO STATUS */
#define SS_VALUE	0	/* Low  = Single Sim */
#define DS_VALUE	1	/* High = Dual Sim */
static unsigned int gpio_number = -1;

enum { NO_SIM = 0, SINGLE_SIM, DUAL_SIM, TRIPLE_SIM };

static int check_simslot_count(struct seq_file *m, void *v)
{
	int retval, support_number_of_simslot;


	printk("\n SIM_SLOT_PIN : %d\n", gpio_number);  //temp log for checking GPIO Setting correctly applyed or not

	retval = gpio_request(gpio_number,"SIM_SLOT_PIN");
	if (retval) {
			pr_err("%s:Failed to reqeust GPIO, code = %d.\n",
				__func__, retval);
			support_number_of_simslot = retval;
	}
	else
	{
		retval = gpio_direction_input(gpio_number);

		if (retval){
			pr_err("%s:Failed to set direction of GPIO, code = %d.\n",
				__func__, retval);
			support_number_of_simslot = retval;
		}
		else
		{
			retval = gpio_get_value(gpio_number);

			/* This codes are implemented assumption that count of GPIO about simslot is only one on H/W schematic
                           You may change this codes if count of GPIO about simslot has change */
			switch(retval)
			{
				case SS_VALUE:
					support_number_of_simslot = SINGLE_SIM;
					break;
				case DS_VALUE:
					support_number_of_simslot = DUAL_SIM;
					break;
				default :
					support_number_of_simslot = -1;
					break;
			}
		}
		gpio_free(gpio_number);
	}

	if(support_number_of_simslot < 0)
	{
		pr_err("******* Make a forced kernel panic because can't check simslot count******\n");
		panic("kernel panic");
	}

	printk("%s: Number of simslot: %u\n", __func__, support_number_of_simslot);
	seq_printf(m, "%u\n", support_number_of_simslot);

	return 0;

}

static int check_simslot_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, check_simslot_count, NULL);
}

static const struct file_operations check_simslot_count_fops = {
	.open	= check_simslot_count_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release= single_release,
};

static int simslot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	gpio_number = of_get_gpio(dev->of_node, 0);
	if (gpio_number < 0) {
		dev_err(dev, "failed to get proper gpio number\n");
		return -EINVAL;
	}

	if (!proc_create("simslot_count",0,NULL,&check_simslot_count_fops))
	{
		pr_err("***** Make a forced kernel panic because can't make a simslot_count file node ******\n");
		panic("kernel panic");
		return -ENOMEM;
	}

	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id simslot_dt_ids[] = {
	{ .compatible = "simslot_count" },
	{ },
};
MODULE_DEVICE_TABLE(of, simslot_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver simslot_device_driver = {
	.probe		= simslot_probe,
	.driver		= {
		.name	= "simslot_count",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = simslot_dt_ids,
#endif /* CONFIG_OF */
	}
};

static int __init simslot_count_init(void)
{
	printk("%s start\n", __func__);
	return platform_driver_register(&simslot_device_driver);
}

static void __exit simslot_count_exit(void)
{
	printk("%s start\n", __func__);
	platform_driver_unregister(&simslot_device_driver);
}

late_initcall(simslot_count_init);
module_exit(simslot_count_exit)
