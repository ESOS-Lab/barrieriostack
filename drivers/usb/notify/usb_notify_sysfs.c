/*
 *  drivers/usb/notify/usb_notify_sysfs.c
 *
 * Copyright (C) 2015 Samsung, Inc.
 * Author: Dongrak Shin <dongrak.shin@samsung.com>
 *
*/
#define pr_fmt(fmt) "usb_notify: " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/err.h>
#include "usb_notify_sysfs.h"

struct notify_data {
	struct class *usb_notify_class;
	atomic_t device_count;
};

static struct notify_data usb_notify_data;

static ssize_t disable_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usb_notify_dev *udev = (struct usb_notify_dev *)
		dev_get_drvdata(dev);
	char *disable;

	if (udev->disable_state)
		disable = "ON";
	else
		disable = "OFF";

	pr_info("read disable_state %s\n", disable);
	return snprintf(buf,  sizeof(disable)+1, "%s\n", disable);
}

static ssize_t disable_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct usb_notify_dev *udev = (struct usb_notify_dev *)
		dev_get_drvdata(dev);

	char *disable;
	size_t ret = -ENOMEM;

	disable = kzalloc(size+1, GFP_KERNEL);
	if (!disable)
		goto error;

	sscanf(buf, "%s", disable);

	if (udev->set_disable) {

		pr_info("set disable state %s\n", disable);

		if (!strncmp(disable, "ON", 2))
			udev->set_disable(1);
		else if (!strncmp(disable, "OFF", 3))
			udev->set_disable(0);
		else
			pr_err("set disable cmd error=%s\n", disable);
	} else
		pr_info("set_disable func is NULL\n");
	ret = size;
	kfree(disable);
error:
	return ret;
}

static DEVICE_ATTR(disable, 0664, disable_show, disable_store);

static struct attribute *usb_notify_attrs[] = {
	&dev_attr_disable.attr,
	NULL,
};

static struct attribute_group usb_notify_attr_grp = {
	.attrs = usb_notify_attrs,
};

static int create_usb_notify_class(void)
{
	if (!usb_notify_data.usb_notify_class) {
		usb_notify_data.usb_notify_class
			= class_create(THIS_MODULE, "usb_notify");
		if (IS_ERR(usb_notify_data.usb_notify_class))
			return PTR_ERR(usb_notify_data.usb_notify_class);
		atomic_set(&usb_notify_data.device_count, 0);
	}

	return 0;
}

int usb_notify_dev_register(struct usb_notify_dev *udev)
{
	int ret;

	if (!usb_notify_data.usb_notify_class) {
		ret = create_usb_notify_class();
		if (ret < 0)
			return ret;
	}

	udev->index = atomic_inc_return(&usb_notify_data.device_count);
	udev->dev = device_create(usb_notify_data.usb_notify_class, NULL,
		MKDEV(0, udev->index), NULL, udev->name);
	if (IS_ERR(udev->dev))
		return PTR_ERR(udev->dev);

	udev->disable_state = 0;
	ret = sysfs_create_group(&udev->dev->kobj, &usb_notify_attr_grp);
	if (ret < 0) {
		device_destroy(usb_notify_data.usb_notify_class,
				MKDEV(0, udev->index));
		return ret;
	}

	dev_set_drvdata(udev->dev, udev);
	return 0;
}
EXPORT_SYMBOL_GPL(usb_notify_dev_register);

void usb_notify_dev_unregister(struct usb_notify_dev *udev)
{
	sysfs_remove_group(&udev->dev->kobj, &usb_notify_attr_grp);
	device_destroy(usb_notify_data.usb_notify_class, MKDEV(0, udev->index));
	dev_set_drvdata(udev->dev, NULL);
}
EXPORT_SYMBOL_GPL(usb_notify_dev_unregister);

int usb_notify_class_init(void)
{
	return create_usb_notify_class();
}
EXPORT_SYMBOL_GPL(usb_notify_class_init);

void usb_notify_class_exit(void)
{
	class_destroy(usb_notify_data.usb_notify_class);
}
EXPORT_SYMBOL_GPL(usb_notify_class_exit);

