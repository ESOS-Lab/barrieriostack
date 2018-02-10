#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/antenna_switch.h>

static bool probe_flag = 0;
static bool if_detect = 0;
static bool earjack_detect = 0;
static struct antenna_switch_drvdata *g_ddata;

extern void antenna_switch_work_earjack(bool current_value)
{
	earjack_detect = current_value;

	if( probe_flag == 0 ) 
		return; 
	else {
		if ( if_detect || earjack_detect ) {
			printk(" if_detect or earjack_detect \n");		                                                                                                                  
			if (!gpio_get_value(g_ddata->gpio_antenna_switch)) {
				printk("enable antenna_switch to 1 \n");
				gpio_set_value(g_ddata->gpio_antenna_switch, 1);
			}
		} else {
			printk(" if_detach and earjack_detach \n");
			if (gpio_get_value(g_ddata->gpio_antenna_switch)) {
				printk("disable antenna_switch to 0 \n"); 
				gpio_set_value(g_ddata->gpio_antenna_switch, 0);
			}
		}
	}
}

extern void antenna_switch_work_if(bool current_value)
{
	if_detect = current_value;

	if( probe_flag == 0 )
		return;
	else {	
		if ( if_detect || earjack_detect ) {
			printk(" if_detect or earjack_detect \n");		                                                                                                                  
			if (!gpio_get_value(g_ddata->gpio_antenna_switch)) {
				printk("enable antenna_switch to 1 \n");
				gpio_set_value(g_ddata->gpio_antenna_switch, 1);
			}
		} else {
			printk(" if_detach and earjack_detach \n");
			if (gpio_get_value(g_ddata->gpio_antenna_switch)) {
				printk("disable antenna_switch to 0 \n"); 
				gpio_set_value(g_ddata->gpio_antenna_switch, 0);
			}
		}
	}
}

#ifdef CONFIG_OF
static int of_antenna_switch_data_parsing_dt(struct antenna_switch_drvdata *ddata)
{
	struct device_node *np_haptic;
	int gpio;
	enum of_gpio_flags flags;

	np_haptic = of_find_node_by_path("/antenna_switch");
	if (np_haptic == NULL) {
		printk("%s : error to get dt node\n", __func__);
		return -EINVAL;
	}

	gpio = of_get_named_gpio_flags(np_haptic, "antenna_switch,gpio_antenna_switch", 0, &flags);
	if (gpio < 0) {
		pr_info("%s: fail to get antenna_switch \n", __func__ );
		return -EINVAL;
	}
	ddata->gpio_antenna_switch = gpio;	

	return 0;
}
#endif

static int antenna_switch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct antenna_switch_drvdata *ddata;
	int error, rc;

	printk("%s start[%d]\n", __func__, __LINE__);

	ddata = kzalloc(sizeof(struct antenna_switch_drvdata), GFP_KERNEL);
	if (!ddata) {
		dev_err(dev, "failed to allocate state\n");
		return -ENOMEM;
	}
	
	probe_flag = 1;
#ifdef CONFIG_OF
	if(dev->of_node) {
		error = of_antenna_switch_data_parsing_dt(ddata);
		if (error < 0) {
			pr_info("%s : fail to get the dt (Antenna Switch)\n", __func__);
			goto fail1;
		}
	}
#endif
	platform_set_drvdata(pdev, ddata);
	g_ddata = ddata;

	rc = gpio_request(ddata->gpio_antenna_switch, "antenna_switch");
	if (rc)
		pr_err("%s gpio_request : error : %d\n", __func__, rc);
	gpio_direction_output(ddata->gpio_antenna_switch, 0);

	printk("%s complete[%d]\n", __func__, __LINE__);
	
	return 0;

fail1:
	kfree(ddata);
	return error;
}

static int antenna_switch_remove(struct platform_device *pdev)
{
	struct certify_hall_drvdata *ddata = platform_get_drvdata(pdev);
	printk("%s start\n", __func__);
	kfree(ddata);
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id switch_dt_ids[] = {
	{ .compatible = "antenna_switch" },
	{ },
};
MODULE_DEVICE_TABLE(of, switch_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver antenna_switch_device_driver = {
	.probe		= antenna_switch_probe,
	.remove		= antenna_switch_remove,
	.driver		= {
		.name	= "antenna_switch",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= switch_dt_ids,
#endif /* CONFIG_OF */
	},
};

static int __init antenna_switch_init(void)
{
	printk("%s start\n", __func__);
	return platform_driver_register(&antenna_switch_device_driver);
}

static void __exit antenna_switch_exit(void)
{
	printk("%s start\n", __func__);
	platform_driver_unregister(&antenna_switch_device_driver);
}

module_init(antenna_switch_init);
module_exit(antenna_switch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Seungjun Lee <jjuny.lee@samsung.com>");
MODULE_DESCRIPTION("Switch IC driver for GPIOs");
MODULE_ALIAS("platform:switchs");
