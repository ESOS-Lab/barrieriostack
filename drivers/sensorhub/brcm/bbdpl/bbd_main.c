/*
 * Copyright 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *  
 * A copy of the GPL is available at 
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The BBD (Broadcom Bridge Driver)
 *
 * tabstop = 8
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "bbd_internal.h"

#define BBD_DEVICE_MAJOR	239

int bbd_ssi_spi_open(void);
void bbd_ssi_spi_close(void);

int bbd_tty_close(void);

struct bbd_device *gpbbd_dev;

void bbd_sensor_init_vars  (struct bbd_device *dev, int* result);
void bbd_control_init_vars (struct bbd_device *dev, int* result);
void bbd_packet_init_vars  (struct bbd_device *dev, int* result);
void bbd_reliable_init_vars(struct bbd_device *dev, int* result);
void bbd_patch_init_vars     (struct bbd_device *dev, int* result);
void bbd_ssi_spi_debug_init_vars(struct bbd_device *dev, int* result);

int bbd_sensor_uninit(void);
int bbd_control_uninit(void);
int bbd_packet_uninit(void);
int bbd_reliable_uninit(void);
int bbd_patch_uninit(void);

int bbd_sensor_open(struct inode *inode, struct file *filp);
int bbd_sensor_release(struct inode *inode, struct file *filp);
ssize_t bbd_sensor_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos);
ssize_t bbd_sensor_write(struct file *filp,
				const char __user *buf,
				size_t size, loff_t *ppos);
unsigned int bbd_sensor_poll(struct file *filp, poll_table * wait);

int bbd_control_open(struct inode *inode, struct file *filp);
int bbd_control_release(struct inode *inode, struct file *filp);
ssize_t bbd_control_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos);
ssize_t bbd_control_write(struct file *filp,
				const char __user *buf,
				size_t size, loff_t *ppos);
unsigned int bbd_control_poll(struct file *filp, poll_table * wait);

int bbd_packet_open(struct inode *inode, struct file *filp);
int bbd_packet_release(struct inode *inode, struct file *filp);
ssize_t bbd_packet_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos);
ssize_t bbd_packet_write(struct file *filp,
				const char __user *buf,
				size_t size, loff_t *ppos);
unsigned int bbd_packet_poll(struct file *filp, poll_table * wait);

int bbd_reliable_open(struct inode *inode, struct file *filp);
int bbd_reliable_release(struct inode *inode, struct file *filp);
ssize_t bbd_reliable_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos);
ssize_t bbd_reliable_write(struct file *filp,
				const char __user *buf,
				size_t size, loff_t *ppos);
unsigned int bbd_reliable_poll(struct file *filp, poll_table * wait);

int bbd_patch_open(struct inode *inode, struct file *filp);
int bbd_patch_release(struct inode *inode, struct file *filp);
ssize_t bbd_patch_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos);

int bbd_ssi_spi_debug_open(struct inode *inode, struct file *filp);
int bbd_ssi_spi_debug_release(struct inode *inode, struct file *filp);
ssize_t bbd_ssi_spi_debug_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos);
ssize_t bbd_ssi_spi_debug_write(struct file *filp,
                                const char __user *buf,
                                size_t size, loff_t *ppos);

int bbd_alloc_count(void);

static const struct file_operations bbd_fops[BBD_DEVICE_INDEX] = {
	/* bbd sensor file operations (legacy) */
	{
		.owner		=  THIS_MODULE,
		.open		=  bbd_sensor_open,
		.release	=  bbd_sensor_release,
		.read		=  bbd_sensor_read,
		.write		=  bbd_sensor_write,
		.poll		=  bbd_sensor_poll,
	},
	/* bbd control file operations */
	{
		.owner		=  THIS_MODULE,
		.open		=  bbd_control_open,
		.release	=  bbd_control_release,
		.read		=  bbd_control_read,
		.write		=  bbd_control_write,
		.poll		=  bbd_control_poll,
	},
	/* bbd packet file operations */
	{
		.owner		=  THIS_MODULE,
		.open		=  bbd_packet_open,
		.release	=  bbd_packet_release,
		.read		=  bbd_packet_read,
		.write		=  bbd_packet_write,
		.poll		=  bbd_packet_poll,
	},
	/* bbd reliable file operations */
	{
		.owner		=  THIS_MODULE,
		.open		=  bbd_reliable_open,
		.release	=  bbd_reliable_release,
		.read		=  bbd_reliable_read,
		.write		=  bbd_reliable_write,
		.poll		=  bbd_reliable_poll,
	},
    /* bbd patch file operations */
	{
		.owner		=  THIS_MODULE,
		.open		=  bbd_patch_open,
		.release	=  bbd_patch_release,
		.read		=  bbd_patch_read,
		.write		=  NULL,
		.poll		=  NULL,
       },
       /* bbd ssi spi debug operations */
       {
                .owner          =  THIS_MODULE,
		.open		=  bbd_ssi_spi_debug_open,
		.release	=  bbd_ssi_spi_debug_release,
                .read           =  bbd_ssi_spi_debug_read,
                .write          =  bbd_ssi_spi_debug_write,
                .poll           =  NULL,
       }
};

/*****************************************************************************
*
* bbd platform
*
******************************************************************************/
static void bbd_platform_release(struct device *dev)
{
	printk(KERN_INFO "%s()\n", __func__);
}

static struct platform_device bbd_platform_dev = {
	.name = "bbd",
	.id = 0,
	.dev = {
		.release = bbd_platform_release,
	}
};

static int bbd_dev_major = BBD_DEVICE_MAJOR;

int  bbd_sysfs_init  (struct kobject *kobj);
void bbd_sysfs_uninit(struct kobject *kobj);

static int bbd_init_vars(struct bbd_device *pbbd_dev)
{
	int result = 0;

	if (!pbbd_dev)
		return -EINVAL;

	memset(pbbd_dev, 0, sizeof(*pbbd_dev));
        /* pbbd_dev->db    = false;           */
        /* pbbd_dev->qcount= 0;               */
        /* pbbd_dev->qfree = 0;               */

        /* pbbd_dev->db    = true;            ** KLUDGE here if needed   */
        /* d_dev->sio_type = BBD_SERIAL_SPI   ** preferred default       */
        /* d_dev->sio_type = BBD_SERIAL_TTY   ** for testing without SPI */
        pbbd_dev->sio_type = BBD_SERIAL_SPI;
        pbbd_dev->shmd     = true;
        pbbd_dev->tty_baud = 921600;
        mutex_init(&pbbd_dev->qlock);

        bbd_sensor_init_vars  (pbbd_dev, &result);
        bbd_control_init_vars (pbbd_dev, &result);
        bbd_packet_init_vars  (pbbd_dev, &result);
        bbd_reliable_init_vars(pbbd_dev, &result);
        bbd_patch_init_vars   (pbbd_dev, &result);
        bbd_ssi_spi_debug_init_vars   (pbbd_dev, &result);
        if (!result)
                BbdEngine_BbdEngine(&gpbbd_dev->bbd_engine);
        return result;
}

static int __init bbd_device_init(void)
{
	int i, ret = -ENOMEM;
	dev_t devno[BBD_DEVICE_INDEX] = {0,};
	struct device *dev = NULL;
	struct class *bbd_class;	

	const char *dev_name[BBD_DEVICE_INDEX] = {
		"bbd_sensor",
		"bbd_control",
		"bbd_packet",
		"bbd_reliable",
		"bbd_patch",
		"bbd_ssi_spi_debug" };

	gpbbd_dev = bbd_alloc(sizeof(struct bbd_device));
	if (!gpbbd_dev)
		goto exit;

	ret = bbd_init_vars(gpbbd_dev);
	if (ret < 0)
		goto free_bbd_dev;

	bbd_class = class_create(THIS_MODULE, "bbd");
	if (IS_ERR(bbd_class)) {
		printk(KERN_ERR "BBD:%s() failed to create class\n", __func__);
		goto free_bbd_dev;
	}

	for (i=0; i<BBD_DEVICE_INDEX; i++) {
		BUG_ON(!dev_name[i]);
		printk(KERN_INFO "BBD:%s(%d,%d)##### 204024 /dev/%s\n", __func__, BBD_DEVICE_MAJOR,i, dev_name[i]);
		devno[i] = MKDEV(BBD_DEVICE_MAJOR, i);
		ret = register_chrdev_region(devno[i], 1, dev_name[i]);
		if (ret) {
			printk(KERN_ERR "BBD:%s() failed to register char dev(%d) (ret=%d)\n", __func__, i, ret);
			while(--i)
				unregister_chrdev_region(devno[i], 1);
			goto free_bbd_class;
		}
		}

	for (i=0; i<BBD_DEVICE_INDEX; i++) {
		dev = device_create(bbd_class, NULL, MKDEV(BBD_DEVICE_MAJOR, i), NULL, "%s", dev_name[i]);
		if (IS_ERR_OR_NULL(dev)) {
			printk(KERN_ERR "BBD:%s() failed to create device %s\n", __func__, dev_name[i]);
			while(--i) 
				device_destroy(bbd_class, devno[i]);
			goto unregister_dev;
		}
		}

	for (i=0; i<BBD_DEVICE_INDEX; i++) {
		cdev_init(&gpbbd_dev->dev[i], &bbd_fops[i]);
		gpbbd_dev->dev[i].owner = THIS_MODULE;
		gpbbd_dev->dev[i].ops   = &bbd_fops[i];
		ret = cdev_add(&gpbbd_dev->dev[i], devno[i], 1);
		if (ret) {
			printk(KERN_ERR "failed to add bbd_device device(ret=%d).\n", ret);
			while(--i)
				cdev_del(&gpbbd_dev->dev[i]);
			goto destroy_dev;
		}
	}

	if (platform_device_register(&bbd_platform_dev)) {
		printk(KERN_ERR "%s(): failed to register platfrom device(bbd_device)\n", __func__);
		goto del_cdev;
	}

	dev = &bbd_platform_dev.dev;
	dev_set_drvdata(dev, (void *)gpbbd_dev);

	if (bbd_sysfs_init(&dev->kobj))
		goto unreg_platform_dev;

        bbd_ssi_spi_open();

	return 0;

unreg_platform_dev:
	platform_device_unregister(&bbd_platform_dev);	
del_cdev:
	for (i=0; i<BBD_DEVICE_INDEX; i++)
		cdev_del(&gpbbd_dev->dev[i]);
destroy_dev:
	for (i=0; i<BBD_DEVICE_INDEX; i++)
		device_destroy(bbd_class, devno[i]);
unregister_dev:
	for (i=0; i<BBD_DEVICE_INDEX; i++)
		unregister_chrdev_region(MKDEV(bbd_dev_major, i), 1);
free_bbd_class:
	class_destroy(bbd_class);	
free_bbd_dev:
            bbd_free(gpbbd_dev);
            gpbbd_dev = 0;
exit:
	return ret;
}

int bbd_qitem_cache_uninit(void);

static void __exit bbd_device_exit(void)
{
	struct bbd_device *pbbd_dev = gpbbd_dev;
	struct device *dev = &bbd_platform_dev.dev;
	int i=0;
	int freedActive = 0;
	int freedIdle   = 0;

	if (pbbd_dev == NULL)
		return;


	for (i=0; i<BBD_DEVICE_INDEX; i++) {
		cdev_del(&pbbd_dev->dev[i]);
		unregister_chrdev_region(MKDEV(bbd_dev_major, i), 1);
	}

        BbdEngine_dtor(&pbbd_dev->bbd_engine);

        bbd_ssi_spi_close();
        bbd_tty_close();
        // freedActive += bbd_ssi_spi_debug_uninit(); not needed
        freedActive += bbd_patch_uninit();
        freedActive += bbd_reliable_uninit();
        freedActive += bbd_packet_uninit();
        freedActive += bbd_control_uninit();
        freedActive += bbd_sensor_uninit();

        freedIdle    = bbd_qitem_cache_uninit();
        pbbd_dev->qfree = 0;
	bbd_free(pbbd_dev);
	gpbbd_dev = 0;

        printk("%s Buffer stats: %d active %d idle  %d lost\n", __func__,
                        freedActive, freedIdle, bbd_alloc_count());

        bbd_sysfs_uninit(&dev->kobj);
	platform_device_unregister(&bbd_platform_dev);
}

MODULE_AUTHOR("Broadcom");
MODULE_LICENSE("Dual BSD/GPL");
/* bbd_device_init() should be called before shmd/ssp init function */
subsys_initcall(bbd_device_init);
module_exit(bbd_device_exit);
