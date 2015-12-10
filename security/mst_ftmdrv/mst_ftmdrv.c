/*
 * MST FTM drv Support
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/smc.h>
#include <linux/wakelock.h>

#include "mst_ftmdrv.h"

#define MST_LDO3_0 "MST_LEVEL_3.0V"

static int mst_en = 0;
static int mst_pd = 0;
static int mst_md = 0;
static bool mst_power_on = 0;
static int escape_loop = 1;
static struct class *mst_ftmdrv_class;
struct device *mst_ftmdrv_dev;
static spinlock_t event_lock;
static struct wake_lock   mst_wakelock;

EXPORT_SYMBOL_GPL(mst_ftmdrv_dev);

static void of_mst_hw_onoff(bool on){
	struct regulator *regulator3_0;
	int ret;

	regulator3_0 = regulator_get(NULL, MST_LDO3_0);
	if (IS_ERR(regulator3_0)) {
		printk("%s : regulator 3.0 is not available", __func__);
		return;
	}

	if (mst_power_on == on) {
		printk("mst-drv : mst_power_onoff : already %d\n", on);
		regulator_put(regulator3_0);
		return;
	}

	mst_power_on = on;

	printk("mst-drv : mst_power_onoff : %d\n", on);

	if(regulator3_0 == NULL){
		printk(KERN_ERR "%s: regulator3_0 is invalid(NULL)\n", __func__);
		return ;
	}

	if(on) {
		ret = regulator_enable(regulator3_0);
		if (ret < 0) {
			printk("%s : regulator 3.0 is not enable", __func__);
		}
	}else{
		regulator_disable(regulator3_0);
	}

	regulator_put(regulator3_0);
}

int mst_change(int mst_val, int mst_stat) {
	if(mst_stat){
		gpio_set_value(mst_md, 0);
		gpio_set_value(mst_pd, 1);
		if(mst_val) {
			udelay(150);
			gpio_set_value(mst_pd, 0);
			gpio_set_value(mst_md, 1);			
			udelay(150);
			mst_stat = 1;
		} else {
			udelay(300);
			mst_stat = 0;
		}
	} else {		
		gpio_set_value(mst_pd, 0);
		gpio_set_value(mst_md, 1);
		if(mst_val) {
			udelay(150);
			gpio_set_value(mst_md, 0);
			gpio_set_value(mst_pd, 1);			
			udelay(150);
			mst_stat = 0;
		} else {
			udelay(300);
			mst_stat = 1;
		}
	}
	return mst_stat;
}

static int transmit_mst_data(int case_i)
{

	int rt = 1;
	int i = 0;
	int j = 0; 
	int mst_stat = 0;
	unsigned long flags;
	char midstring[] = 
			{69,98,21,84,82,84,81,88,16,22,22,
			81,87,16,21,88,88,22,62,104,117,97,
			110,103,79,37,110,121,97,110,103,64,
			64,64,64,64,64,64,64,64,64,64,64,62,
			81,82,81,16,81,16,81,16,16,16,16,16,
			16,82,21,25,16,81,16,16,16,16,16,16,
			19,88,19,16,16,16,16,16,16,31,70};

	char midstring2[] = 
			{11,22,16,1,16,21,22,7,8,19,22,19,
			25,2,25,8,8,13,2,21,16,1,16,16,16,
			21,16,16,16,16,22,16,16,7,19,25,19,16,31,13};
			
	gpio_set_value(mst_pd, 0);
	gpio_set_value(mst_md, 0);
	gpio_set_value(mst_en, 0);
	udelay(300);
	gpio_set_value(mst_en, 1);
	mdelay(11);

	if(case_i == 1){

		spin_lock_irqsave(&event_lock, flags);
		for(i = 0 ; i < 28 ; i++){
			mst_stat = mst_change(0, mst_stat);
		}
		
		i = 0;
		do{
			for(j = 0 ; j <= 6 ; j++) {
				if(((midstring[i] & (1 << j)) >> j) == 1) {
							mst_stat = mst_change(1, mst_stat);
						}
						else {
							mst_stat = mst_change(0, mst_stat);
						}
					}
					i++;
		}
		while ( ( i < sizeof(midstring)) && (midstring[i - 1] != 70) );

		for(i = 0 ; i < 28 ; i++){
			mst_stat = mst_change(0, mst_stat);
		}

		spin_unlock_irqrestore(&event_lock, flags);
		gpio_set_value(mst_md, 0);
		gpio_set_value(mst_pd, 0);
		gpio_set_value(mst_en, 0);
		return rt;
		
	}if(case_i == 2){
		spin_lock_irqsave(&event_lock, flags);
		for(i = 0 ; i < 28 ; i++){			
			mst_stat = mst_change(0, mst_stat);
		}

		i = 0;
		do{
			for(j = 0 ; j <= 4 ; j++) {
				if(((midstring2[i] & (1 << j)) >> j) == 1) {
							mst_stat = mst_change(1, mst_stat);
						}
						else {
							mst_stat = mst_change(0, mst_stat);
						}
					}
					i++;
		}
		while ( ( i < sizeof(midstring2)) );

		for(i = 0 ; i < 28 ; i++){
			mst_stat = mst_change(0, mst_stat);
		}

		spin_unlock_irqrestore(&event_lock, flags);
		gpio_set_value(mst_md, 0);
		gpio_set_value(mst_pd, 0);
		gpio_set_value(mst_en, 0);
		return rt;
		
	}else{
		rt = 0;
		return rt;
	}
}

/* mst ftm suspend&resume code */
static int mst_ftm_device_suspend(struct platform_device *dev, pm_message_t state)
{
	u64 r0 = 0, r1 = 0, r2 = 0, r3 = 0;
	int result=0;
	
	printk(KERN_ERR "%s\n", __func__);
	printk(KERN_ERR "MST_FTM_DRV]]] suspend");
	//Will Add here
	r0 = (0x8300000c);
	result = exynos_smc(r0, r1, r2, r3);

	printk(KERN_ERR "MST_FTM_DRV]]] suspend after smc : %d", result);
	return 0;
}
static int mst_ftm_device_resume(struct platform_device *dev)
{
	u64 r0 = 0, r1 = 0, r2 = 0, r3 = 0;
	int result=0;
	
	printk(KERN_ERR "%s\n", __func__);
	printk(KERN_ERR "MST_FTM_DRV]]] resume");
	//Will Add here
	r0 = (0x8300000d);
	result = exynos_smc(r0, r1, r2, r3);

	printk(KERN_ERR "MST_FTM_DRV]]] resume after smc : %d",result);
	return 0;
}

static ssize_t show_mst_ftmdrv(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    if (!dev)
        return -ENODEV;

    // todo
    return sprintf(buf, "%s\n", "MST ftmdrv data");
}

static ssize_t store_mst_ftmdrv(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char test_result[256]={0,};
	int case_i;
	
#if 1
	sscanf(buf, "%88s\n", test_result);

	printk(KERN_ERR "MST Store test result : %s\n", test_result);
	
#endif
	printk("[MST] Data Value : %d, %d, %d\n", mst_en, mst_pd, mst_md);


	of_mst_hw_onoff(1);
	mdelay(10);

	switch(test_result[0]){
		case '1':
			case_i= 1;
			printk("MST send track1 data, case_i %d\n", case_i);
			if (transmit_mst_data(case_i)) {
				printk("track1 data is sent\n");
			}
			break;
	
		case '2':
			case_i = 2; 
			printk("MST send track2 data, case_i : %d\n", case_i);
			if (transmit_mst_data(case_i)) {
				printk("track2 data is sent\n");
			}
			break;
				
		case '3':
			case_i = 1;
			printk("MST send track1 data, case_i : %d\n", case_i);
			if (transmit_mst_data(case_i)) {
				printk("track1 data is sent\n");
			}

			mdelay(300);
			
			case_i = 2; 
			printk("MST send track2 data, case_i : %d\n", case_i);
			if (transmit_mst_data(case_i)) {
				printk("track2 data is sent\n");
			}

			break;

		case '4':
			case_i = 2; 
			if(escape_loop){
				wake_lock_init(&mst_wakelock, WAKE_LOCK_SUSPEND, "mst_wakelock");
				wake_lock(&mst_wakelock);
			}
			escape_loop = 0;
			while( 1 ) {
				if(escape_loop == 1)
					break;
				of_mst_hw_onoff(1);
				mdelay(10);
				printk("MST send track2 data, case_i : %d\n", case_i);
				if (transmit_mst_data(case_i)) {
					printk("track2 data is sent\n");
				}
			of_mst_hw_onoff(0);
			mdelay(1000);
			}
			break;

		case '5':
			if(!escape_loop)
				wake_lock_destroy(&mst_wakelock);
			escape_loop = 1;
			printk("MST escape_loop value = 1\n");
			break;
			
		default:
			printk("MST data send failed\n");
			break;
	}		
	
	of_mst_hw_onoff(0);

	return count;
}

static DEVICE_ATTR(transmit, 0777, show_mst_ftmdrv, store_mst_ftmdrv);

static int sec_mst_gpio_init(struct device *dev)
{
	int ret = 0;
	mst_en = of_get_named_gpio(dev->of_node, "sec-mst,mst-pwr-gpio", 0);
	mst_pd = of_get_named_gpio(dev->of_node, "sec-mst,mst-pd-gpio", 0);
	mst_md = of_get_named_gpio(dev->of_node, "sec-mst,mst-md-gpio", 0);
	printk(KERN_INFO "[MST] Data Value : %d, %d, %d\n", 
			mst_en, mst_pd, mst_md);

	if (mst_en < 0) { 
		printk(KERN_ERR "%s : Cannot create the gpio\n", __func__);
		return 1;
	}
	printk(KERN_ERR "MST_DRV]]] gpio en inited\n");
	
	if (mst_pd < 0) {
		printk(KERN_ERR "%s : Cannot create the gpio\n", __func__);
		return 1;
	}
	printk(KERN_ERR "MST_DRV]]] gpio pd inited\n");
	
	if (mst_md < 0) {
		printk(KERN_ERR "%s : Cannot create the gpio\n", __func__);
		return 1;
	}
	printk(KERN_ERR "MST_DRV]]] gpio md inited\n");
	
	ret = gpio_request(mst_en, "sec-mst,mst-pwr-gpio");
	if (ret) {
		printk(KERN_ERR "[MST] failed to get en gpio : %d, %d\n", ret, mst_en);
	}
	
	ret = gpio_request(mst_pd, "sec-mst,mst-pd-gpio");
	if (ret) {
		printk(KERN_ERR "[MST] failed to get pd gpio : %d, %d\n", ret, mst_pd);
	}
	
	ret = gpio_request(mst_md, "sec-mst,mst-md-gpio");
	if (ret) {
		printk(KERN_ERR "[MST] failed to get md gpio : %d, %d\n", ret, mst_md);
	}

	if (!(ret < 0)  && (mst_en > 0)) {
		gpio_direction_output(mst_en, 0);
		printk(KERN_ERR "%s : Send Output\n", __func__);
	}
	
	if (!(ret < 0) && (mst_pd > 0)) {
		gpio_direction_output(mst_pd, 0);
		printk(KERN_ERR "%s : Send Output\n", __func__);
	}
	
	if (!(ret < 0)  && (mst_md > 0)) {
		gpio_direction_output(mst_md, 0);
		printk(KERN_ERR "%s : Send Output\n", __func__);
	}
	return 0;
}


static int mst_tempdevice_probe(struct platform_device *pdev)
{
	int err;
	
	printk("%s init start\n", __func__);
	
	spin_lock_init(&event_lock);
		
	if (sec_mst_gpio_init(&pdev->dev))
		return -1;
	
	printk(KERN_ALERT "%s\n", __func__);
	mst_ftmdrv_class = class_create(THIS_MODULE, "mst");
	if (IS_ERR(mst_ftmdrv_class)) {
		err = PTR_ERR(mst_ftmdrv_class);
		goto error;
	}

	mst_ftmdrv_dev = device_create(mst_ftmdrv_class,
			NULL /* parent */, 0 /* dev_t */, NULL /* drvdata */,
			MST_FTMDRV_DEV);
	if (IS_ERR(mst_ftmdrv_dev)) {
		err = PTR_ERR(mst_ftmdrv_dev);
		goto error_destroy;
	}

	/* register this mst device with the driver core */
	err = device_create_file(mst_ftmdrv_dev, &dev_attr_transmit);
	if (err)
		goto error_destroy;

	printk(KERN_DEBUG "MST ftmdrv driver (%s) is initialized.\n", MST_FTMDRV_DEV);
	return 0;

error_destroy:
	kfree(mst_ftmdrv_dev);
	device_destroy(mst_ftmdrv_class, 0);
error:
	printk(KERN_ERR "%s: MST ftmdrv driver initialization failed\n", __FILE__);
	return err;

}


static struct of_device_id mst_match_table[] = {
	{ .compatible = "sec-mst",},
	{},
};

static struct platform_driver sec_mst_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sec-mst",
		.of_match_table = mst_match_table,
	},
	.probe = mst_tempdevice_probe,
	.suspend = mst_ftm_device_suspend,
	.resume = mst_ftm_device_resume,
};

static int __init mst_ftmdrv_init(void)
{
	int ret=0;
	printk(KERN_ERR "%s\n", __func__);

	ret = platform_driver_register(&sec_mst_driver);

	printk(KERN_ERR "MST_DRV]]] init , ret val : %d\n",ret);

	return ret;
}

static void __exit mst_ftmdrv_exit (void)
{
    class_destroy(mst_ftmdrv_class);
    printk(KERN_ALERT "%s\n", __func__);
}

MODULE_AUTHOR("JASON KANG, j_seok.kang@samsung.com");
MODULE_DESCRIPTION("MST ftmdrv driver");
MODULE_VERSION("0.1");

module_init(mst_ftmdrv_init);
module_exit(mst_ftmdrv_exit);

