/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "fingerprint.h"
#include "et310.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

static DECLARE_BITMAP(minors, N_SPI_MINORS);

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static int gpio_irq;
static struct etspi_data *g_data;
static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);
static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

static void etspi_interrupt_timer_routine(unsigned long _data)
{
	struct etspi_data *etspi = (struct etspi_data *)_data;
	pr_info("%s FPS interrupt count = %d\n", __func__, etspi->int_count);
	if (etspi->int_count >= etspi->detect_threshold) {
		etspi->finger_on = 1;
		pr_info("%s FPS triggered\n", __func__);
	}

	etspi->int_count = 0;
	wake_up_interruptible(&interrupt_waitq);
}

static irqreturn_t etspi_fingerprint_interrupt(int irq , void *dev_id)
{
	struct etspi_data *etspi = (struct etspi_data *)dev_id;

	if (!etspi->int_count)
		mod_timer(&etspi->timer,
			jiffies + msecs_to_jiffies(etspi->detect_period));
	etspi->int_count++;

	return IRQ_HANDLED;
}

int etspi_Interrupt_Init(
		struct etspi_data *etspi,
		int int_num,
		int detect_period,
		int detect_threshold)
{
	int status = 0;

	pr_info("%s\n", __func__);
	pr_info("FP %s int_num = %d detect_period = %d detect_threshold = %d\n",
				__func__,
				int_num,
				detect_period,
				detect_threshold);

	etspi->int_count = 0;
	etspi->detect_period = detect_period;
	etspi->detect_threshold = detect_threshold;
	gpio_irq = gpio_to_irq(etspi->drdyPin);

	if (gpio_irq < 0) {
		pr_err("%s gpio_to_irq failed\n", __func__);
		status = gpio_irq;
		goto done;
	}

	if (request_irq
		(gpio_irq, etspi_fingerprint_interrupt, IRQ_TYPE_EDGE_RISING,
					"etspi_irq", etspi) < 0) {
			etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;
			pr_err("%s drdy request_irq failed\n", __func__);
			status = -EBUSY;
			goto done;
	} else {
		etspi->drdy_irq_flag = DRDY_IRQ_ENABLE;
	}

done:
	etspi->finger_on = 0;
	return 0;
}

int etspi_Interrupt_Free(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);

	if (etspi != NULL)
		free_irq(gpio_irq, etspi);

	return 0;
}

void etspi_Interrupt_Abort(struct etspi_data *etspi)
{
	etspi->finger_on = 0;
	wake_up_interruptible(&interrupt_waitq);
}

unsigned int etspi_fps_interrupt_poll(
		struct file *file,
		struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct etspi_data *etspi = file->private_data;
	pr_info("%s FPS fps_interrupt_poll\n", __func__);

	etspi->int_count = 0;
	poll_wait(file, &interrupt_waitq, wait);
	if (etspi->finger_on) {
		mask |= POLLIN | POLLRDNORM;
		etspi->finger_on = 0;
	}
	return mask;
}

/*-------------------------------------------------------------------------*/

static void etspi_reset(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);
	pr_info("%s set gpio %d value %d\n", __func__,  etspi->sleepPin, 0);
	gpio_set_value(etspi->sleepPin, 0);
	msleep(30);
	pr_info("%s set gpio %d value %d\n", __func__, etspi->sleepPin, 1);
	gpio_set_value(etspi->sleepPin, 1);
	msleep(20);
	pr_info("%s end\n", __func__);
}

static void etspi_reset_set(struct etspi_data *etspi, int enable)
{
	pr_info("%s\n", __func__);
	pr_info("%s enable %d\n", __func__, enable);
	if (enable == 0) {
		gpio_set_value(etspi->sleepPin, 0);
		msleep(30);
	} else {
		gpio_set_value(etspi->sleepPin, 1);
		msleep(20);
	}
}

static void etspi_power_control(struct etspi_data *etspi, int status)
{
	pr_info("%s status = %d\n", __func__, status);
	if (status == 1) {
		gpio_set_value(etspi->ocp_en, status);
		usleep_range(2950, 3000);
		gpio_set_value(etspi->ldo_pin, status);
		gpio_set_value(etspi->ldo_pin2, status);
		msleep(20);
	} else if (status == 0) {
		gpio_set_value(etspi->ldo_pin, status);
		gpio_set_value(etspi->ldo_pin2, status);
		usleep_range(2950, 3000);
		gpio_set_value(etspi->ocp_en, status);
	}
	pr_info("%s end\n", __func__);
}

static ssize_t etspi_read(struct file *filp,
						char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static ssize_t etspi_write(struct file *filp,
						const char __user *buf,
						size_t count,
						loff_t *f_pos)
{
/*Implement by vendor if needed*/
	return 0;
}

static long etspi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, retval = 0;
	struct etspi_data *etspi;
	struct spi_device *spi;
	u32 tmp;
	struct egis_ioc_transfer *ioc = NULL;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != EGIS_IOC_MAGIC) {
		pr_err("%s _IOC_TYPE(cmd) != EGIS_IOC_MAGIC", __func__);
		return -ENOTTY;
	}

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
						(void __user *)arg,
						_IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
						(void __user *)arg,
						_IOC_SIZE(cmd));
	if (err) {
		pr_err("%s err", __func__);
		return -EFAULT;
	}

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	etspi = filp->private_data;
	spin_lock_irq(&etspi->spi_lock);
	spi = spi_dev_get(etspi->spi);
	spin_unlock_irq(&etspi->spi_lock);

	if (spi == NULL) {
		pr_err("%s spi == NULL", __func__);
		return -ESHUTDOWN;
	}

	mutex_lock(&etspi->buf_lock);

	/* segmented and/or full-duplex I/O request */
	if (_IOC_NR(cmd) != _IOC_NR(EGIS_IOC_MESSAGE(0))
					|| _IOC_DIR(cmd) != _IOC_WRITE) {
		retval = -ENOTTY;
		goto out;
	}

	tmp = _IOC_SIZE(cmd);
	if ((tmp == 0) || (tmp % sizeof(struct egis_ioc_transfer)) != 0) {
		retval = -EINVAL;
		goto out;
	}

	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (ioc == NULL) {
		retval = -ENOMEM;
		goto out;
	}
	if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
		retval = -EFAULT;
		goto out;
	}

	/*
	 * Read register
	 * tx_buf include register address will be read
	 */
	if (ioc->opcode == FP_REGISTER_READ) {
		u8 *address = ioc->tx_buf;
		u8 *result = ioc->rx_buf;
		pr_debug("etspi FP_REGISTER_READ\n");

		retval = etspi_io_read_register(etspi, address, result);
		if (retval < 0)	{
			pr_err("%s FP_REGISTER_READ error retval = %d\n"
			, __func__, retval);
			goto out;
		}
	}

	/*
	 * Write data to register
	 * tx_buf includes address and value will be wrote
	 */
	if (ioc->opcode == FP_REGISTER_WRITE) {
		u8 *buf = ioc->tx_buf;
		pr_debug("%s FP_REGISTER_WRITE\n", __func__);

		retval = etspi_io_write_register(etspi, buf);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_WRITE error retval = %d\n"
			, __func__, retval);
			goto out;
		}
	}

	/* Get one frame data from sensor */
	if (ioc->opcode == FP_GET_ONE_IMG) {
		u8 *buf = ioc->tx_buf;
		u8 *image_buf = ioc->rx_buf;
		pr_debug("%s FP_GET_ONE_IMG\n", __func__);

		retval = etspi_io_get_one_image(etspi, buf, image_buf);
		if (retval < 0) {
			pr_err("%s FP_GET_ONE_IMG error retval = %d\n"
			, __func__, retval);
			goto out;
		}
	}

	if (ioc->opcode == FP_SENSOR_RESET) {
		pr_info("%s FP_SENSOR_RESET\n", __func__);
		etspi_reset(etspi);
	}

	if (ioc->opcode == FP_RESET_SET) {
		pr_info("%s FP_SENSOR_RESET\n", __func__);
		pr_info("%s status = %d\n", __func__, ioc->len);
		etspi_reset_set(etspi, ioc->len);
	}


	if (ioc->opcode == FP_POWER_CONTROL) {
		pr_info("%s FP_POWER_CONTROL\n", __func__);
		pr_info("%s status = %d\n", __func__, ioc->len);
		etspi_power_control(etspi, ioc->len);
	}

	if (ioc->opcode == FP_SET_SPI_CLOCK) {
		__u32 current_speed = spi->max_speed_hz;
		pr_info("%s FP_SET_SPI_CLOCK\n", __func__);
		pr_info("%s clock = %d\n", __func__, ioc->speed_hz);
		spi->max_speed_hz = ioc->speed_hz;
		retval = spi_setup(spi);
		if (retval < 0) {
			pr_err("%s spi_setup error %d\n", __func__, retval);
			spi->max_speed_hz = current_speed;
		}
	}

	if (ioc->opcode == FP_EEPROM_WREN) {
		pr_info("%s FP_EEPROM_WREN\n", __func__);
		etspi_reset_set(etspi, 0);
		etspi_eeprom_wren(etspi);
		etspi_reset_set(etspi, 1);
	}

	if (ioc->opcode == FP_EEPROM_WRDI) {
		pr_info("%s FP_EEPROM_WRDI\n", __func__);
		etspi_reset_set(etspi, 0);
		etspi_eeprom_wrdi(etspi);
		etspi_reset_set(etspi, 1);
	}

	if (ioc->opcode == FP_EEPROM_RDSR) {
		u8 *result = ioc->rx_buf;
		pr_info("%s FP_EEPROM_RDSR\n", __func__);
		etspi_reset_set(etspi, 0);
		etspi_eeprom_rdsr(etspi, result);
		etspi_reset_set(etspi, 1);
	}

	if (ioc->opcode == FP_EEPROM_WRSR) {
		u8 *buf = ioc->tx_buf;
		pr_info("%s FP_EEPROM_WRSR\n", __func__);
		etspi_reset_set(etspi, 0);
		etspi_eeprom_wrsr(etspi, buf);
		etspi_reset_set(etspi, 1);
	}

	if (ioc->opcode == FP_EEPROM_READ) {
		u8 *buf = ioc->tx_buf;
		u8 *result = ioc->rx_buf;
		pr_info("%s FP_EEPROM_READ\n", __func__);
		etspi_reset_set(etspi, 0);
		etspi_eeprom_read(etspi, buf, result, ioc->len);
		etspi_reset_set(etspi, 1);
	}

	if (ioc->opcode == FP_EEPROM_WRITE) {
		u8 *buf = ioc->tx_buf;
		pr_info("%s FP_EEPROM_WRITE\n", __func__);
		etspi_reset_set(etspi, 0);
		etspi_eeprom_write(etspi, buf, ioc->len);
		etspi_reset_set(etspi, 1);
	}

	/*
	 * Trigger inital routine
	 */
	if (ioc->opcode == INT_TRIGGER_INIT) {
		pr_info("%s Trigger function init\n", __func__);
		retval = etspi_Interrupt_Init(
				etspi,
				(int)ioc->pad[0],
				(int)ioc->pad[1],
				(int)ioc->pad[2]);
		pr_info("%s trigger init = %d\n",  __func__, retval);
	}
	/* trigger */
	if (ioc->opcode == INT_TRIGGER_CLOSE) {
		pr_info("%s Trigger function close\n", __func__);
		retval = etspi_Interrupt_Free(etspi);
		pr_info("%s trigger close = %d\n", __func__, retval);
	}
	/* Poll Abort */
	if (ioc->opcode == INT_TRIGGER_ABORT) {
		pr_info("%s Trigger function close\n", __func__);
		etspi_Interrupt_Abort(etspi);
	}
out:
	if (ioc != NULL)
		kfree(ioc);

	mutex_unlock(&etspi->buf_lock);
	spi_dev_put(spi);
	if (retval < 0)
		pr_err("%s retval = %d\n", __func__, retval);
	return retval;
}

#ifdef CONFIG_COMPAT
static long etspi_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return etspi_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define etspi_compat_ioctl NULL
#endif
/* CONFIG_COMPAT */

static int etspi_open(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;
	int	status = -ENXIO;

	pr_info("%s\n", __func__);
	mutex_lock(&device_list_lock);

	list_for_each_entry(etspi, &device_list, device_entry)
	{
		if (etspi->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (etspi->buffer == NULL) {
			etspi->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (etspi->buffer == NULL) {
				dev_dbg(&etspi->spi->dev, "open/ENOMEM\n");
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			etspi->users++;
			filp->private_data = etspi;
			nonseekable_open(inode, filp);
		}
	} else
		pr_debug("%s nothing for minor %d\n"
			, __func__, iminor(inode));

	mutex_unlock(&device_list_lock);
	return status;
}

static int etspi_release(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;

	pr_info("%s\n", __func__);
	mutex_lock(&device_list_lock);
	etspi = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	etspi->users--;
	if (etspi->users == 0) {
		int	dofree;

		kfree(etspi->buffer);
		etspi->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&etspi->spi_lock);
		dofree = (etspi->spi == NULL);
		spin_unlock_irq(&etspi->spi_lock);

		if (dofree)
			kfree(etspi);
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

int etspi_platformInit(struct etspi_data *etspi)
{
	int status = 0;
	pr_info("%s\n", __func__);

	setup_timer(&etspi->timer, etspi_interrupt_timer_routine,
		(unsigned long)etspi);

	/* gpio setting for ldo, ldo2, sleep, drdy pin */
	if (etspi != NULL) {
		if (etspi->ocp_en) {
			status = gpio_request(etspi->ocp_en, "etspi_ocp_en");
			if (status < 0) {
				pr_err("%s gpio_request etspi_ocp_en failed\n",
					__func__);
			}
			gpio_direction_output(etspi->ocp_en, 1);
		}
		status = gpio_request(etspi->ldo_pin, "etspi_ldo_en");
		if (status < 0) {
			pr_err("%s gpio_request etspi_ldo_en failed\n",
				__func__);
			goto etspi_platformInit_ldo_failed;
		}
		if (etspi->ldo_pin2) {
			status = gpio_request(etspi->ldo_pin2, "etspi_ldo_en2");
			if (status < 0) {
				pr_err("%s gpio_request etspi_ldo_en2 failed\n",
					__func__);
				goto etspi_platformInit_ldo2_failed;
			}
		}
		status = gpio_request(etspi->sleepPin, "etspi_sleep");
		if (status < 0) {
			pr_err("%s gpio_requset etspi_sleep failed\n",
				__func__);
			goto etspi_platformInit_sleep_failed;
		}
		status = gpio_request(etspi->drdyPin, "etspi_drdy");
		if (status < 0) {
			pr_err("%s gpio_request etspi_drdy failed\n",
				__func__);
			goto etspi_platformInit_drdy_failed;
		}
		if (etspi->ldo_pin) {
			gpio_direction_output(etspi->ldo_pin, 1);
			mdelay(1);
			if (etspi->ldo_pin2) {
				gpio_direction_output(etspi->ldo_pin2, 1);
				mdelay(1);
			}
		}

		status = gpio_direction_input(etspi->drdyPin);
		if (status < 0) {
			pr_err("%s gpio_direction_input DRDY failed\n",
				__func__);
			goto etspi_platformInit_gpio_init_failed;
		}
		if (etspi->sleepPin) {
			gpio_direction_output(etspi->sleepPin, 1);
			mdelay(1);
		}
		pr_info("%s sleep value =%d\n"
				"%s ldo en value =%d\n",
				__func__, gpio_get_value(etspi->sleepPin),
				__func__, gpio_get_value(etspi->ldo_pin));
		if (etspi->ldo_pin2) {
			pr_info("%s ldo en2 value =%d\n",
				__func__, gpio_get_value(etspi->ldo_pin2));
		}
	} else {
		status = -EFAULT;
	}

	pr_info("%s reset sensor in platforminit\n", __func__);
	etspi_reset(etspi);

	pr_info("%s successful status=%d\n", __func__, status);
	return status;
etspi_platformInit_gpio_init_failed:
	gpio_free(etspi->drdyPin);
etspi_platformInit_drdy_failed:
	gpio_free(etspi->sleepPin);
etspi_platformInit_sleep_failed:
	gpio_free(etspi->ldo_pin2);
etspi_platformInit_ldo2_failed:
	gpio_free(etspi->ldo_pin);
etspi_platformInit_ldo_failed:
	gpio_free(etspi->ocp_en);
	pr_err("%s is failed\n", __func__);
	return status;
}

void etspi_platformUninit(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);

	if (etspi != NULL) {
		disable_irq(gpio_irq);
		free_irq(gpio_irq, etspi);
		etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;
		if (etspi->ldo_pin)
			gpio_free(etspi->ldo_pin);
		if (etspi->ldo_pin2)
			gpio_free(etspi->ldo_pin2);
		gpio_free(etspi->sleepPin);
		gpio_free(etspi->drdyPin);
	}
}

static int etspi_parse_dt(struct device *dev,
	struct etspi_data *data)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int errorno = 0;
	int gpio;

	gpio = of_get_named_gpio_flags(np, "etspi-sleepPin",
		0, &flags);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->sleepPin = gpio;
		pr_info("%s: sleepPin=%d\n",
			__func__, data->sleepPin);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-drdyPin",
		0, &flags);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->drdyPin = gpio;
		pr_info("%s: drdyPin=%d\n",
			__func__, data->drdyPin);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-ocpen",
		0, &flags);
	if (gpio < 0) {
		data->ocp_en = 0;
		pr_err("%s: fail to get ocp_en\n", __func__);
	} else {
		data->ocp_en = gpio;
		pr_info("%s: ocp_en=%d\n",
			__func__, data->ocp_en);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-ldoPin",
		0, &flags);
	if (gpio < 0) {
		data->ldo_pin = 0;
		pr_err("%s: fail to get ldo_pin\n", __func__);
	} else {
		data->ldo_pin = gpio;
		pr_info("%s: ldo_pin=%d\n",
			__func__, data->ldo_pin);
	}
	if (!of_find_property(np, "etspi-ldoPin2", NULL)) {
		pr_err("%s: not set ldo2 in dts\n", __func__);
	} else {
		gpio = of_get_named_gpio(np, "etspi-ldoPin2", 0);
		if (gpio < 0) {
			data->ldo_pin2 = 0;
			pr_err("%s: fail to get ldo_pin2 : (%d)\n",
				__func__, gpio);
		} else {
			data->ldo_pin2 = gpio;
			pr_info("%s: ldo_pin2=%d\n",
				__func__, data->ldo_pin2);
		}
	}

	pr_info("%s is successful\n", __func__);
	return errorno;
dt_exit:
	pr_err("%s is failed\n", __func__);
	return errorno;
}

static const struct file_operations etspi_fops = {
	.owner = THIS_MODULE,
	.write = etspi_write,
	.read = etspi_read,
	.unlocked_ioctl = etspi_ioctl,
	.compat_ioctl = etspi_compat_ioctl,
	.open = etspi_open,
	.release = etspi_release,
	.llseek = no_llseek,
	.poll = etspi_fps_interrupt_poll
};

static int etspi_type_check(struct etspi_data *etspi)
{
	u8 buf1, buf2, buf3, buf4;

	etspi_read_register(etspi, 0x10, &buf1);
	etspi_read_register(etspi, 0x11, &buf2);
	etspi_read_register(etspi, 0x12, &buf3);
	etspi_read_register(etspi, 0x13, &buf4);
	pr_info("%s buf1: %x, buf2: %x, buf3: %x, buf4: %x\n",
				__func__, buf1, buf2, buf3, buf4);

	if ((buf1 == 0x00) && (buf2 == 0x3F)
				&& (buf3 == 0x00) && (buf4 == 0x8f)) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS sensor\n", __func__);
		return 0;
	} else {
		etspi->sensortype = SENSOR_FAILED;
		pr_info("%s sensor type is FAILED\n", __func__);
		return -1;
	}
}
#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
static ssize_t etspi_type_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct etspi_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->sensortype);
}

static ssize_t etspi_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t etspi_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static DEVICE_ATTR(type_check, S_IRUGO,
	etspi_type_check_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO,
	etspi_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO,
	etspi_name_show, NULL);

static struct device_attribute *fp_attrs[] = {
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	NULL,
};
#endif

static void etspi_work_func_debug(struct work_struct *work)
{
	u8 ldo_value = 0;
	if (g_data->ldo_pin) {
		ldo_value = gpio_get_value(g_data->ldo_pin);
		if (g_data->ldo_pin2) {
			ldo_value = (gpio_get_value(g_data->ldo_pin2) << 1)
				| gpio_get_value(g_data->ldo_pin);
		}
	}
	pr_info("%s ocpen: %d, ldo: %d, sleep: %d, tz: %d, type: %s\n",
		__func__, gpio_get_value(g_data->ocp_en),
		ldo_value, gpio_get_value(g_data->sleepPin),
		g_data->tz_mode,
		sensor_status[g_data->sensortype]);
}

static void etspi_enable_debug_timer(void)
{
	mod_timer(&g_data->dbg_timer,
		round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static void etspi_disable_debug_timer(void)
{
	del_timer_sync(&g_data->dbg_timer);
	cancel_work_sync(&g_data->work_debug);
}

static void etspi_timer_func(unsigned long ptr)
{
	queue_work(g_data->wq_dbg, &g_data->work_debug);
	mod_timer(&g_data->dbg_timer,
		round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static int etspi_set_timer(struct etspi_data *etspi)
{
	int status = 0;
	setup_timer(&etspi->dbg_timer,
		etspi_timer_func, (unsigned long)etspi);
	etspi->wq_dbg =
		create_singlethread_workqueue("etspi_debug_wq");
	if (!etspi->wq_dbg) {
		status = -ENOMEM;
		pr_err("%s could not create workqueue\n", __func__);
		return status;
	}
	INIT_WORK(&etspi->work_debug, etspi_work_func_debug);
	return status;
}

/*-------------------------------------------------------------------------*/

static struct class *etspi_class;

/*-------------------------------------------------------------------------*/

static int etspi_probe(struct spi_device *spi)
{
	struct etspi_data *etspi;
	int status;
	unsigned long minor;
	int retry = 0;

	pr_info("%s\n", __func__);

	/* Allocate driver data */
	etspi = kzalloc(sizeof(*etspi), GFP_KERNEL);
	if (etspi == NULL) {
		pr_err("%s - Failed to kzalloc\n", __func__);
		return -ENOMEM;
	}

	/* device tree call */
	if (spi->dev.of_node) {
		status = etspi_parse_dt(&spi->dev, etspi);
		if (status) {
			pr_err("%s - Failed to parse DT\n", __func__);
			goto etspi_probe_parse_dt_failed;
		}
	}

	/* Initialize the driver data */
	etspi->spi = spi;
	g_data = etspi;

	spin_lock_init(&etspi->spi_lock);
	mutex_init(&etspi->buf_lock);
	mutex_init(&device_list_lock);

	INIT_LIST_HEAD(&etspi->device_entry);

	/* platform init */
	status = etspi_platformInit(etspi);
	if (status != 0) {
		pr_err("%s platforminit failed\n", __func__);
		goto etspi_probe_platformInit_failed;
	}

	spi->bits_per_word = 8;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_3;
	spi->chip_select = 0;
	status = spi_setup(spi);
	if (status != 0) {
		pr_err("%s spi_setup() is failed. status : %d\n",
			__func__, status);
		return status;
	}

	/* sensor hw type check */
	do {
		etspi_type_check(etspi);
		pr_info("%s type (%u), retry (%d)\n"
			, __func__, etspi->sensortype, retry);
	} while (!etspi->sensortype && ++retry < 3);

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;
		etspi->devt = MKDEV(ET310_MAJOR, minor);
		dev = device_create(etspi_class, &spi->dev, etspi->devt,
				    etspi, "esfp0");
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else{
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&etspi->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	if (status == 0)
		spi_set_drvdata(spi, etspi);
	else
		goto etspi_probe_failed;
#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
	status = fingerprint_register(etspi->fp_device,
		etspi, fp_attrs, "fingerprint");
	if (status) {
		pr_err("%s sysfs register failed\n", __func__);
		goto etspi_probe_failed;
	}
#endif

	status = etspi_set_timer(etspi);
	if (status)
		goto etspi_sysfs_failed;
	etspi_enable_debug_timer();
	pr_info("%s is successful\n", __func__);

	return status;

etspi_sysfs_failed:
#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
		fingerprint_unregister(etspi->fp_device, fp_attrs);
#endif
etspi_probe_failed:
	device_destroy(etspi_class, etspi->devt);
	class_destroy(etspi_class);
	etspi_platformUninit(etspi);
etspi_probe_platformInit_failed:
etspi_probe_parse_dt_failed:
	kfree(etspi);
	pr_err("%s is failed\n", __func__);

	return status;
}

static int etspi_remove(struct spi_device *spi)
{
	struct etspi_data *etspi = spi_get_drvdata(spi);

	pr_info("%s\n", __func__);

	if (etspi != NULL) {
		etspi_disable_debug_timer();
		etspi_platformUninit(etspi);

		/* make sure ops on existing fds can abort cleanly */
		spin_lock_irq(&etspi->spi_lock);
		etspi->spi = NULL;
		spi_set_drvdata(spi, NULL);
		spin_unlock_irq(&etspi->spi_lock);

		/* prevent new opens */
		mutex_lock(&device_list_lock);
#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
		fingerprint_unregister(etspi->fp_device, fp_attrs);
#endif
		list_del(&etspi->device_entry);
		device_destroy(etspi_class, etspi->devt);
		clear_bit(MINOR(etspi->devt), minors);
		if (etspi->users == 0)
			kfree(etspi);
		mutex_unlock(&device_list_lock);
	}
	return 0;
}

static struct of_device_id etspi_match_table[] = {
	{ .compatible = "etspi,et310",},
	{},
};

static struct spi_driver etspi_spi_driver = {
	.driver = {
		.name =	"egis_fingerprint",
		.owner = THIS_MODULE,
		.of_match_table = etspi_match_table
	},
	.probe = etspi_probe,
	.remove = etspi_remove,
};

/*-------------------------------------------------------------------------*/

static int __init etspi_init(void)
{
	int status;
	pr_info("%s\n", __func__);

#ifdef ENABLE_VENDOR_CHECK
	/* vendor check */
	pr_info("%s FP_CHECK value (%d)\n", __func__, FP_CHECK);
	if (FP_CHECK) {
		pr_err("%s It is not egis sensor\n", __func__);
		return -ENODEV;
	}
#endif
	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(ET310_MAJOR, "egis_fingerprint", &etspi_fops);
	if (status < 0) {
		pr_err("%s register_chrdev error.\n", __func__);
		return status;
	}

	etspi_class = class_create(THIS_MODULE, "egis_fingerprint");
	if (IS_ERR(etspi_class)) {
		pr_err("%s class_create error.\n", __func__);
		unregister_chrdev(ET310_MAJOR, etspi_spi_driver.driver.name);
		return PTR_ERR(etspi_class);
	}

	status = spi_register_driver(&etspi_spi_driver);
	if (status < 0) {
		pr_err("%s spi_register_driver error.\n", __func__);
		class_destroy(etspi_class);
		unregister_chrdev(ET310_MAJOR, etspi_spi_driver.driver.name);
	}

	pr_info("%s is successful\n", __func__);

	return status;
}

static void __exit etspi_exit(void)
{
	pr_info("%s\n", __func__);

	spi_unregister_driver(&etspi_spi_driver);
	class_destroy(etspi_class);
	unregister_chrdev(ET310_MAJOR, etspi_spi_driver.driver.name);
}

module_init(etspi_init);
module_exit(etspi_exit);

MODULE_AUTHOR("Wang YuWei, <robert.wang@egistec.com>");
MODULE_DESCRIPTION("SPI Interface for ET310");
MODULE_LICENSE("GPL");
