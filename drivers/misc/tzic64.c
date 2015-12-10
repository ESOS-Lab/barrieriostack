/*
 * Samsung TZIC Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define KMSG_COMPONENT "TZIC"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
//#include <linux/android_pmem.h>
#include <linux/io.h>
#include <linux/types.h>
//#include <asm/smc.h>

#define TZIC_DEV "tzic"
#define SMC_CMD_STORE_BINFO	 (-201)

static int gotoCpu0(void);
static int gotoAllCpu(void) __attribute__ ((unused));

u64 exynos_smc1(u64 cmd, u64 arg1, u64 arg2, u64 arg3)
{
	register u64 reg0 __asm__("x0") = cmd;
	register u64 reg1 __asm__("x1") = arg1;
	register u64 reg2 __asm__("x2") = arg2;
	register u64 reg3 __asm__("x3") = arg3;

	__asm__ volatile (
	        "dsb    sy\n"
	        "smc    0\n"
	        : "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	
	);


	return reg0;
}

u64 exynos_smc_new(u64 cmd, u64 arg1, u64 arg2, u64 arg3)
{
	register u64 reg0 __asm__("x0") = cmd;
	register u64 reg1 __asm__("x1") = arg1;
	register u64 reg2 __asm__("x2") = arg2;
	register u64 reg3 __asm__("x3") = arg3;

	__asm__ volatile (
	        "dsb    sy\n"
	        "smc    0\n"
	        : "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	
	);

    return reg1;
}

int exynos_smc_read_oemflag(u64 ctrl_word, u64 *val)
{
	u64	cmd = 0;
	u64	arg1 = 0;
	u64	arg2 = 0;
	u64	arg3 = 0;
	register u64 reg0 __asm__("x0") = cmd;
	register u64 reg1 __asm__("x1") = arg1;
	register u64 reg2 __asm__("x2") = arg2;
	register u64 reg3 __asm__("x3") = arg3;
	u64 idx = 0;

	for (idx = 0; reg2 != ctrl_word; idx++) {
		reg0 = -202;
		reg1 = 1;
		reg2 = idx;
		__asm__ volatile (
		        "dsb    sy\n"
		        "smc    0\n"
		        : "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
		
		);
		if (reg1)
			return -1;
	}

	reg0 = -202;
	reg1 = 1;
	reg2 = idx;

	__asm__ volatile (
	        "dsb    sy\n"
	        "smc    0\n"
	        : "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	
	);
	if (reg1)
		return -1;

	*val = reg2;

	return 0;
}

int exynos_smc_read_oemflag_new(u64 getflag, u64 *val)
{
	u64	cmd = 0;
	u64	arg1 = 0;
	u64	arg2 = 0;
	u64	arg3 = 0;
	register u64 reg0 __asm__("x0") = cmd;
	register u64 reg1 __asm__("x1") = arg1;
	register u64 reg2 __asm__("x2") = arg2;
	register u64 reg3 __asm__("x3") = arg3;
  u64 idx = 0;

   reg0 = 0x83000003;
   reg1 = 1;
   reg2 = getflag;
   reg3 = idx;

	__asm__ volatile (
	        "dsb    sy\n"
	        "smc    0\n"
	        : "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	
	);

	if (reg1 == -1) {
	   return -1;
	}

	*val = reg1;
	return 0;
}


static DEFINE_MUTEX(tzic_mutex);
static struct class *driver_class;
static dev_t tzic_device_no;
static struct cdev tzic_cdev;

#define LOG printk
#define TZIC_IOC_MAGIC 0x9E
#define TZIC_IOCTL_SET_FUSE_REQ _IO(TZIC_IOC_MAGIC, 1)
#define TZIC_IOCTL_GET_FUSE_REQ _IOR(TZIC_IOC_MAGIC, 0, unsigned int)

#define TZIC_IOCTL_SET_FUSE_REQ_NEW _IO(TZIC_IOC_MAGIC, 11)
#define TZIC_IOCTL_GET_FUSE_REQ_NEW _IO(TZIC_IOC_MAGIC, 10)

typedef enum {
    OEMFLAG_MIN_FLAG = 2,
    OEMFLAG_TZ_DRM,
    OEMFLAG_FIDD,
    OEMFLAG_CC,
    OEMFLAG_NUM_OF_FLAG,
} Sec_OemFlagID_t;

typedef struct
{
    u64  name;
    u64  value;
}t_flag;


static long tzic_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int ret = 0;
	t_flag param;

	ret = gotoCpu0();
	if (0 != ret) {
		LOG(KERN_INFO "[oemflag]changing core failed!\n");
		return -1;
	}
	LOG(KERN_INFO "[oemflag]CPU 0\n");
	switch(cmd){
		case TZIC_IOCTL_SET_FUSE_REQ:
			LOG(KERN_INFO "[oemflag]set_fuse\n");
			exynos_smc1(SMC_CMD_STORE_BINFO, 0x80010001, 0, 0);
			exynos_smc1(SMC_CMD_STORE_BINFO, 0x00000001, 0, 0);
			goto return_default;
		break;

		case TZIC_IOCTL_GET_FUSE_REQ:
			LOG(KERN_INFO "[oemflag]get_fuse\n");
			exynos_smc_read_oemflag(0x80010001, (u64 *) arg);
			goto return_default;
		break;

		case TZIC_IOCTL_SET_FUSE_REQ_NEW:
			LOG(KERN_INFO "[oemflag]set_fuse\n");
			ret=copy_from_user( &param, (void *)arg, sizeof(param) );
			if(ret) {
				LOG(KERN_INFO "[oemflag]ERROR copy from user\n");
				goto return_new_from;
			}
			if ((OEMFLAG_MIN_FLAG < param.name) && (param.name < OEMFLAG_NUM_OF_FLAG)){
				LOG(KERN_INFO "[oemflag]set_fuse_name : %lld\n", param.name);
				exynos_smc_new(0x83000004, 0, param.name, 0);
				goto return_new_to;
			} else {
				LOG(KERN_INFO "[oemflag]command error\n");
				param.value = -1;
				goto return_new_to;
			}
		break;

		case TZIC_IOCTL_GET_FUSE_REQ_NEW:
			LOG(KERN_INFO "[oemflag]get_fuse\n");
			ret=copy_from_user( &param, (void *)arg, sizeof(param) );
			if(ret) {
				LOG(KERN_INFO "[oemflag]ERROR copy from user\n");
				goto return_new_from;
			}
			if ((OEMFLAG_MIN_FLAG < param.name) && (param.name < OEMFLAG_NUM_OF_FLAG)){
				LOG(KERN_INFO "[oemflag]get_fuse_name : %lld\n", param.name);
				exynos_smc_read_oemflag_new(param.name, &param.value) ;
				LOG(KERN_INFO "[oemflag]get_oemflag_value : %lld\n", param.value);
				goto return_new_to;
			} else {
				LOG(KERN_INFO "[oemflag]command error\n");
				param.value = -1;
				goto return_new_to;
			}
		break;

		default:
			LOG(KERN_INFO "[oemflag]command error\n");
			param.value = -1;
			goto return_new_to;
	}

 return_default:
	gotoAllCpu();
	LOG(KERN_INFO "[oemflag]ALL CPU (default)\n");
	return 0;
 return_new_from:
	gotoAllCpu();
	LOG(KERN_INFO "[oemflag]ALL CPU (new_from)\n");
	return copy_from_user( &param, (void *)arg, sizeof(param) );
 return_new_to:
	gotoAllCpu();
	LOG(KERN_INFO "[oemflag]ALL CPU (new_to)\n");
	return copy_to_user( (void *)arg, &param, sizeof(param) );
}

static const struct file_operations tzic_fops = {
	.owner = THIS_MODULE,
	.compat_ioctl = tzic_ioctl,
	.unlocked_ioctl = tzic_ioctl,
};

static int __init tzic_init(void)
{
	int rc;
	struct device *class_dev;

	rc = alloc_chrdev_region(&tzic_device_no, 0, 1, TZIC_DEV);
	if (rc < 0) {
		LOG(KERN_INFO "alloc_chrdev_region failed %d\n", rc);
		return rc;
	}

	driver_class = class_create(THIS_MODULE, TZIC_DEV);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		LOG(KERN_INFO "class_create failed %d\n", rc);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, tzic_device_no, NULL,
				  TZIC_DEV);
	if (!class_dev) {
		LOG(KERN_INFO "class_device_create failed %d\n", rc);
		rc = -ENOMEM;
		goto class_destroy;
	}

	cdev_init(&tzic_cdev, &tzic_fops);
	tzic_cdev.owner = THIS_MODULE;

	rc = cdev_add(&tzic_cdev, MKDEV(MAJOR(tzic_device_no), 0), 1);
	if (rc < 0) {
		LOG(KERN_INFO "cdev_add failed %d\n", rc);
		goto class_device_destroy;
	}

	return 0;

 class_device_destroy:
	device_destroy(driver_class, tzic_device_no);
 class_destroy:
	class_destroy(driver_class);
 unregister_chrdev_region:
	unregister_chrdev_region(tzic_device_no, 1);
	return rc;
}

static void __exit tzic_exit(void)
{
	device_destroy(driver_class, tzic_device_no);
	class_destroy(driver_class);
	unregister_chrdev_region(tzic_device_no, 1);
}

static int gotoCpu0(void)
{
	int ret = 0;
	struct cpumask mask = CPU_MASK_CPU0;

	LOG(KERN_INFO "System has %d CPU's, we are on CPU #%d\n"
	    "\tBinding this process to CPU #0.\n"
	    "\tactive mask is %lx, setting it to mask=%lx\n",
	    nr_cpu_ids,
	    raw_smp_processor_id(), cpu_active_mask->bits[0], mask.bits[0]);
	ret = set_cpus_allowed_ptr(current, &mask);
	if (0 != ret)
		LOG(KERN_INFO "set_cpus_allowed_ptr=%d.\n", ret);
	LOG(KERN_INFO "And now we are on CPU #%d\n", raw_smp_processor_id());

	return ret;
}

static int gotoAllCpu(void)
{
	int ret = 0;
	struct cpumask mask = CPU_MASK_ALL;

	LOG(KERN_INFO "System has %d CPU's, we are on CPU #%d\n"
	    "\tBinding this process to CPU #0.\n"
	    "\tactive mask is %lx, setting it to mask=%lx\n",
	    nr_cpu_ids,
	    raw_smp_processor_id(), cpu_active_mask->bits[0], mask.bits[0]);
	ret = set_cpus_allowed_ptr(current, &mask);
	if (0 != ret)
		LOG(KERN_INFO "set_cpus_allowed_ptr=%d.\n", ret);
	LOG(KERN_INFO "And now we are on CPU #%d\n", raw_smp_processor_id());

	return ret;
}

int tzic_get_tamper_flag(void)
{
	u64 arg;
	exynos_smc_read_oemflag(0x80010001, &arg);
	return arg;
}
EXPORT_SYMBOL(tzic_get_tamper_flag);

int tzic_set_tamper_flag(void)
{
	exynos_smc1(SMC_CMD_STORE_BINFO, 0x80010001, 0, 0);
	return exynos_smc1(SMC_CMD_STORE_BINFO, 0x00000001, 0, 0);
}
EXPORT_SYMBOL(tzic_set_tamper_flag);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Samsung TZIC Driver");
MODULE_VERSION("1.00");

module_init(tzic_init);
module_exit(tzic_exit);
