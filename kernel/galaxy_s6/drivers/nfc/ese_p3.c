/*
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

 /**
 * \addtogroup spi_driver for Oberthur ESE P3
 *
 * @{ */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/ese_p3.h>

#include <linux/platform_data/spi-s3c64xx.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/clk.h>
#include <linux/wakelock.h>


/* Device driver's configuration macro */
/* Macro to configure poll/interrupt based req*/
#undef P3_IRQ_ENABLE
//#define P3_IRQ_ENABLE

/* Macro to configure reset to p3*/
#undef P3_RST_ENABLE
//#define P3_RST_ENABLE

/* Macro to configure Hard/Soft reset to P3 */
//#define P3_HARD_RESET
#undef P3_HARD_RESET

#ifdef P3_HARD_RESET
static struct regulator *p3_regulator = NULL;
#else
#endif

#undef PANIC_DEBUG

#define P3_IRQ   33 /* this is the same used in omap3beagle.c */
#define P3_RST  138

#define P3_SPI_CLOCK     8000000;

/* size of maximum read/write buffer supported by driver */
#define MAX_BUFFER_SIZE   259U

//static struct class *p3_device_class;

/* Different driver debug lever */
enum P3_DEBUG_LEVEL {
    P3_DEBUG_OFF,
    P3_FULL_DEBUG
};

/* Variable to store current debug level request by ioctl */
static unsigned char debug_level = P3_FULL_DEBUG; /////

#define P3_DBG_MSG(msg...)  \
        switch(debug_level)      \
        {                        \
        case P3_DEBUG_OFF:      \
        break;                 \
        case P3_FULL_DEBUG:     \
        printk(KERN_INFO "[ESE-P3] :  " msg); \
        break; \
        default:                 \
        printk(KERN_ERR "[ESE-P3] :  Wrong debug level %d", debug_level); \
        break; \
        } \

#define P3_ERR_MSG(msg...) printk(KERN_ERR "[ESE-P3] : " msg );

/* Device specific macro and structure */
struct p3_dev {
	wait_queue_head_t read_wq; /* wait queue for read interrupt */
	struct mutex buffer_mutex; /* buffer mutex */
	struct spi_device *spi;  /* spi device structure */
	struct miscdevice p3_device; /* char device as misc driver */
	unsigned int rst_gpio; /* SW Reset gpio */
	unsigned int irq_gpio; /* P3 will interrupt DH for any ntf */
	bool irq_enabled; /* flag to indicate irq is used */
	unsigned char enable_poll_mode; /* enable the poll mode */
	spinlock_t irq_enabled_lock; /*spin lock for read irq */

	unsigned char *null_buffer;
	unsigned char *buffer;

	bool tz_mode;
	spinlock_t ese_spi_lock;

	unsigned int mosipin;
	unsigned int misopin;
	unsigned int cspin;
	unsigned int clkpin;
	bool isGpio_cfgDone;
	bool enabled_clk;
#ifdef FEATURE_ESE_WAKELOCK
	struct wake_lock ese_lock;
#endif
	int cs_gpio;
};

/* T==1 protocol specific global data */
const unsigned char SOF = 0xA5u;

/* Only for Qcom*/
/*static int p3_ioctl_config_spi_gpio(
	struct p3_dev *p3_device)
{
	struct spi_device *spidev = NULL;
	int ret_val = 0;

	if (!p3_device->isGpio_cfgDone) {
		P3_DBG_MSG("%s SET_SPI_CONFIGURATION\n", __func__);
		spin_lock_irq(&p3_device->ese_spi_lock);
		spidev = spi_dev_get(p3_device->spi);
		spin_unlock_irq(&p3_device->ese_spi_lock);

		//ret_val = ese_spi_request_gpios(spidev);
		if (ret_val < 0)
			P3_ERR_MSG("%s: couldn't config spi gpio\n", __func__);
		p3_device->isGpio_cfgDone = true;

		p3_device->null_buffer =
			kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (p3_device->null_buffer == NULL) {
			ret_val = -ENOMEM;
			P3_ERR_MSG("%s null_buffer == NULL, -ENOMEM\n",
				__func__);
			//goto vfsspi_open_out;
		}
		p3_device->buffer =
			kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (p3_device->buffer == NULL) {
			ret_val = -ENOMEM;
			kfree(p3_device->null_buffer);
			P3_ERR_MSG("%s buffer == NULL, -ENOMEM\n",
				__func__);
			//goto vfsspi_open_out;
		}

		spi_dev_put(spidev);
		usleep_range(950, 1000);
	}
	return ret_val;
}
*/

static int p3_set_clk(struct p3_dev *p3_device, unsigned long arg)
{
	int ret_val = 0;
	unsigned long clock = 0;
	struct spi_device *spidev = NULL;
	struct s3c64xx_spi_driver_data *sdd = NULL;

	if (arg >= 100000)
		clock = arg;
	else
		return -1;
	P3_DBG_MSG("%s CLK is %lu.\n", __func__, clock);

	spin_lock_irq(&p3_device->ese_spi_lock);
	spidev = spi_dev_get(p3_device->spi);
	spin_unlock_irq(&p3_device->ese_spi_lock);
	if (spidev == NULL) {
		P3_ERR_MSG("%s - Failed to get spi dev.\n", __func__);
		return -1;
	}

	//if (clock < 10000)
	//	clock = P3_SPI_CLOCK;
	spidev->max_speed_hz = (u32)clock;

	sdd = spi_master_get_devdata(spidev->master);
	if (!sdd){
		P3_ERR_MSG("%s - Failed to get spi dev.\n", __func__);
		return -EFAULT;
	}
	/*pm_runtime_get_sync(&sdd->pdev->dev);*/ /*Need to move to Enable*/
	/*P3_DBG_MSG("%s pm_runtime_get_sync.\n", __func__);*/

	/* set spi clock rate */
	clk_set_rate(sdd->src_clk, clock * 2);

	spi_dev_put(spidev);

	return ret_val;
}

static int p3_enable_clk(struct p3_dev *p3_device)
{
	int ret_val = 0;
	struct spi_device *spidev = NULL;
	struct s3c64xx_spi_driver_data *sdd = NULL;

	/* for defence MULTI-OPEN */
	if (p3_device->enabled_clk) {
		P3_ERR_MSG("%s - clock was ALREADY enabled!\n", __func__);
		return -EBUSY;
	}

	spin_lock_irq(&p3_device->ese_spi_lock);
	spidev = spi_dev_get(p3_device->spi);
	spin_unlock_irq(&p3_device->ese_spi_lock);
	if (spidev == NULL) {
		P3_ERR_MSG("%s - Failed to get spi dev.\n", __func__);
		return -1;
	}

	sdd = spi_master_get_devdata(spidev->master);
	if (!sdd){
		P3_ERR_MSG("%s - Failed to get spi dev.\n", __func__);
		return -EFAULT;
	}
	pm_runtime_get_sync(&sdd->pdev->dev); /* Enable clk */

	/* set spi clock rate */
	clk_set_rate(sdd->src_clk, spidev->max_speed_hz * 2);
#ifdef FEATURE_ESE_WAKELOCK
	wake_lock(&p3_device->ese_lock);
#endif
	p3_device->enabled_clk = true;
	spi_dev_put(spidev);

	return ret_val;
}

static int p3_disable_clk(struct p3_dev *p3_device)
{
	int ret_val = 0;
	//unsigned short clock = 0;
	struct spi_device *spidev = NULL;
	struct s3c64xx_spi_driver_data *sdd = NULL;

	if (!p3_device->enabled_clk) {
		P3_ERR_MSG("%s - clock was not enabled!\n", __func__);
		return ret_val;
	}

	spin_lock_irq(&p3_device->ese_spi_lock);
	spidev = spi_dev_get(p3_device->spi);
	spin_unlock_irq(&p3_device->ese_spi_lock);
	if (spidev == NULL) {
		P3_ERR_MSG("%s - Failed to get spi dev!\n", __func__);
		return -1;
	}

	sdd = spi_master_get_devdata(spidev->master);
	if (!sdd){
		P3_ERR_MSG("%s - Failed to get spi dev.\n", __func__);
		return -EFAULT;
	}

	p3_device->enabled_clk = false;
	pm_runtime_put_sync(&sdd->pdev->dev); /* Disable clock */

	spi_dev_put(spidev);
#ifdef FEATURE_ESE_WAKELOCK
	if (wake_lock_active(&p3_device->ese_lock))
		wake_unlock(&p3_device->ese_lock);
#endif
	return ret_val;
}

static int p3_enable_cs(struct p3_dev *p3_device)
{
	int ret_val = 0;

	/*  CS control low(active)*/
	gpio_set_value(p3_device->cs_gpio, 0);
	usleep_range(50,70);

	return ret_val;
}


static int p3_disable_cs(struct p3_dev *p3_device)
{
	int ret_val = 0;

	/* CS High(decative) */
	gpio_set_value(p3_device->cs_gpio, 1);

	return ret_val;
}

static int p3_enable_clk_cs(struct p3_dev *p3_device)
{
	int ret_val = 0;
	struct spi_device *spidev = NULL;
	struct s3c64xx_spi_driver_data *sdd = NULL;

	//P3_DBG_MSG("%s CLK is %lu.\n", __func__, clock);

	spin_lock_irq(&p3_device->ese_spi_lock);
	spidev = spi_dev_get(p3_device->spi);
	spin_unlock_irq(&p3_device->ese_spi_lock);
	if (spidev == NULL) {
		P3_ERR_MSG("%s - Failed to get spi dev.\n", __func__);
		return -1;
	}

	sdd = spi_master_get_devdata(spidev->master);
	if (!sdd){
		P3_ERR_MSG("%s - Failed to get spi dev.\n", __func__);
		return -EFAULT;
	}
	pm_runtime_get_sync(&sdd->pdev->dev); /* Enable clk */

	/* set spi clock rate */
	//clk_set_rate(sdd->src_clk, clock * 2);

	p3_device->enabled_clk = true;
	spi_dev_put(spidev);

	/*  CS control*/
	gpio_set_value(p3_device->cs_gpio, 0);
	usleep_range(50,70);

	return ret_val;
}

static int p3_disable_clk_cs(struct p3_dev *p3_device)
{
	int ret_val = 0;
	//unsigned short clock = 0;
	struct spi_device *spidev = NULL;
	struct s3c64xx_spi_driver_data *sdd = NULL;

	if (!p3_device->enabled_clk) {
		P3_ERR_MSG("%s - clock was not enabled!\n", __func__);
		return ret_val;
	}

	/* CS High */
	gpio_set_value(p3_device->cs_gpio, 1);

	spin_lock_irq(&p3_device->ese_spi_lock);
	spidev = spi_dev_get(p3_device->spi);
	spin_unlock_irq(&p3_device->ese_spi_lock);
	if (spidev == NULL) {
		P3_ERR_MSG("%s - Failed to get spi dev!\n", __func__);
		return -1;
	}

	P3_DBG_MSG("%s DISABLE_SPI_CLOCK\n", __func__);

	sdd = spi_master_get_devdata(spidev->master);
	if (!sdd){
		P3_ERR_MSG("%s - Failed to get spi dev.\n", __func__);
		return -EFAULT;
	}

	p3_device->enabled_clk = false;
	pm_runtime_put(&sdd->pdev->dev);

	spi_dev_put(spidev);

	return ret_val;
}

static int p3_regulator_onoff(struct p3_dev *data, int onoff)
{
	int rc = 0;
	struct regulator *regulator_vdd_1p8;

	regulator_vdd_1p8 = regulator_get(NULL, "VDD_1.8V_ESE"); // to be changed
	if (IS_ERR(regulator_vdd_1p8) || regulator_vdd_1p8 == NULL) {
		P3_ERR_MSG("%s - vdd_1p8 regulator_get fail\n", __func__);
		return -ENODEV;
	}

	P3_DBG_MSG("%s - onoff = %d\n", __func__, onoff);
	if (onoff == 1) {
		rc = regulator_enable(regulator_vdd_1p8);
		if (rc) {
			P3_ERR_MSG("%s - enable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_vdd_1p8);
		if (rc) {
			P3_ERR_MSG("%s - disable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	}

	/*data->regulator_is_enable = (u8)onoff;*/

	done:
	regulator_put(regulator_vdd_1p8);

	return rc;
}

static int p3_xfer(struct p3_dev *p3_device,
			struct p3_ioctl_transfer *tr)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("%s\n", __func__);

	if (p3_device == NULL || tr == NULL)
		return -EFAULT;

	if (tr->len > DEFAULT_BUFFER_SIZE || !tr->len)
		return -EMSGSIZE;

	if (tr->tx_buffer != NULL) {
		if (copy_from_user(p3_device->null_buffer,
				tr->tx_buffer, tr->len) != 0)
			return -EFAULT;
	}

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = p3_device->null_buffer;
	t.rx_buf = p3_device->buffer;
	t.len = tr->len;

	spi_message_add_tail(&t, &m);

	status = spi_sync(p3_device->spi, &m);

	if (status == 0) {
		if (tr->rx_buffer != NULL) {
			unsigned int missing = 0;

			missing = (unsigned int)copy_to_user(tr->rx_buffer,
					       p3_device->buffer, tr->len);

			if (missing != 0)
				tr->len = tr->len - missing;
		}
	}
	pr_debug("%s p3_xfer,length=%d\n", __func__, tr->len);
	return status;

} /* vfsspi_xfer */

static int p3_rw_spi_message(struct p3_dev *p3_device,
				 unsigned long arg)
{
	struct p3_ioctl_transfer   *dup = NULL;
	int err = 0;
#ifdef PANIC_DEBUG
	unsigned int addr_rx = 0;
	unsigned int addr_tx = 0;
	unsigned int addr_len = 0;
#endif

	dup = kmalloc(sizeof(struct p3_ioctl_transfer), GFP_KERNEL);
	if (dup == NULL)
		return -ENOMEM;

#if 0//def PANIC_DEBUG
	addr_rx = (unsigned int)(&dup->rx_buffer);
	addr_tx = (unsigned int)(&dup->tx_buffer);
	addr_len = (unsigned int)(&dup->len);
#endif
	if (copy_from_user(dup, (void *)arg,
			   sizeof(struct p3_ioctl_transfer)) != 0) {
		kfree(dup);
		return -EFAULT;
	} else {
#if 0//def PANIC_DEBUG
		if ((addr_rx != (unsigned int)(&dup->rx_buffer)) ||
			(addr_tx != (unsigned int)(&dup->tx_buffer)) ||
			(addr_len != (unsigned int)(&dup->len)))
		P3_ERR_MSG("%s invalid addr!!! rx=%x, tx=%x, len=%x\n",
			__func__, (unsigned int)(&dup->rx_buffer),
			(unsigned int)(&dup->tx_buffer),
			(unsigned int)(&dup->len));
#endif
		err = p3_xfer(p3_device, dup);
		if (err != 0) {
			kfree(dup);
			P3_ERR_MSG("%s xfer failed!\n", __func__);
			return err;
		}
	}
	if (copy_to_user((void *)arg, dup,
			 sizeof(struct p3_ioctl_transfer)) != 0)
		return -EFAULT;
	kfree(dup);
	return 0;
}

static int p3_swing_release_cs(struct p3_dev *p3_device, int cnt)
{
	int i;

	for(i=0; i< cnt ; i++)
	{
		udelay(1);
		gpio_direction_output(p3_device->cs_gpio, 1);
		udelay(1);
		gpio_direction_output(p3_device->cs_gpio, 0);
	}

	P3_DBG_MSG("%s cnt:%d ", __func__, cnt);
	return 0;
}

static int p3_swing_on_cs(struct p3_dev *p3_device, int cnt)
{
	int i;

	gpio_direction_output(p3_device->cs_gpio, 1);
	udelay(1);

#ifdef ENABLE_ESE_P3_EXYNO_SPI
{
	void __iomem *gpg4con, *gpg4dat;

	gpg4con = ioremap(0x14c90000, SZ_4);
	gpg4dat = ioremap(0x14c90004, SZ_4);
	__raw_writel(0x1211, gpg4con);
	__raw_writel(0x2, gpg4dat);

	iounmap(gpg4con);
	iounmap(gpg4dat);
}
#endif

	for(i=0; i< cnt ; i++)
	{
		udelay(1);
		gpio_direction_output(p3_device->cs_gpio, 0);
		udelay(1);
		gpio_direction_output(p3_device->cs_gpio, 1);
	}
	//P3_DBG_MSG("%s cnt:%d ", __func__, cnt);

#ifdef ENABLE_ESE_P3_EXYNO_SPI
{
	void __iomem *gpg4con;

	gpg4con = ioremap(0x14c90000, SZ_4);
	__raw_writel(0x2212, gpg4con);

	iounmap(gpg4con);
}
#endif

	udelay(1);
	gpio_set_value(p3_device->cs_gpio, 0);

	return 0;
}
/**
 * \ingroup spi_driver
 * \brief Called from SPI LibEse to initilaize the P3 device
 *
 * \param[in]       struct inode *
 * \param[in]       struct file *
 *
 * \retval 0 if ok.
 *
*/

static int p3_dev_open(struct inode *inode, struct file *filp)
{
	struct p3_dev
	*p3_dev = container_of(filp->private_data,
			struct p3_dev, p3_device);
	/* GPIO ctrl for Power  */
	int ret = 0;

	/* for defence MULTI-OPEN */
	if (p3_dev->enabled_clk) {
		P3_ERR_MSG("%s - ALREADY opened!\n", __func__);
		return -EBUSY;
	}

	/* power ON at probe */
	ret = p3_regulator_onoff(p3_dev, 1);
	if(ret < 0)
		P3_ERR_MSG(" test: failed to turn on LDO()\n");
	usleep_range(5000, 5500);

	/*p3_dev->spi->max_speed_hz = 100000L;
	p3_dev->spi->mode = SPI_MODE_3;
	ret = spi_setup(p3_dev->spi);
	if (ret < 0)
		P3_ERR_MSG(" test: failed to do spi_setup-0()\n");
	*/
	gpio_set_value(p3_dev->cs_gpio, 1);

#ifdef ENABLE_ESE_P3_EXYNO_SPI
	if(s3c64xx_spi_change_gpio(ESE_DEFAULT) < 0) {
		P3_ERR_MSG("fail to set pinctrl default\n"); }
	else
		P3_ERR_MSG("ok to set pinctrl default\n");
#endif

	filp->private_data = p3_dev;
	P3_DBG_MSG("%s : Major No: %d, Minor No: %d\n", __func__,
		imajor(inode), iminor(inode));

	p3_dev->null_buffer =
		kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
	if (p3_dev->null_buffer == NULL) {
		P3_ERR_MSG("%s null_buffer == NULL, -ENOMEM\n",
			__func__);
		return -ENOMEM;
	}
	p3_dev->buffer =
		kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
	if (p3_dev->buffer == NULL) {
		kfree(p3_dev->null_buffer);
		P3_ERR_MSG("%s : buffer == NULL, -ENOMEM\n",
			__func__);
		return -ENOMEM;
	}
	return 0;
}


static int p3_dev_release(struct inode *inode, struct file *filp)
{
	struct p3_dev
	*p3_dev = container_of(filp->private_data,
			struct p3_dev, p3_device);

	/* GPIO ctrl for Power  */
	int ret = 0;

	p3_dev = filp->private_data;

	if (p3_dev->enabled_clk) {
		P3_DBG_MSG("%s disable CLK at release!!!\n", __func__);
		ret = p3_disable_clk(p3_dev);
		if(ret < 0)
			P3_ERR_MSG("failed to disable CLK\n");
	}
#ifdef FEATURE_ESE_WAKELOCK
	if (wake_lock_active(&p3_dev->ese_lock)) {
		P3_DBG_MSG("%s: wake unlock at release!!\n", __func__);
		wake_unlock(&p3_dev->ese_lock);
	}
#endif

#ifdef ENABLE_ESE_P3_EXYNO_SPI
	if(s3c64xx_spi_change_gpio(ESE_POWER_OFF) < 0) {
		P3_ERR_MSG("fail to set pinctrl power_off\n");
	}
	//else
		//P3_DBG_MSG("ok to set pinctrl off 183[%d]\n",gpio_get_value(183));
#if 0
{
	void __iomem *gpg4con, *gpg4dat;

	gpg4con = ioremap(0x14c90000, SZ_4);
	gpg4dat = ioremap(0x14c90004, SZ_4);
	__raw_writel(0x1111, gpg4con);
	__raw_writel(0x2, gpg4dat);

	iounmap(gpg4con);
	iounmap(gpg4dat);
}
#endif
#endif
	udelay(2);
	gpio_direction_output(p3_dev->cs_gpio, 0);

	/* Defence for bit shifting while CLOSE~OPEN */
	p3_swing_release_cs(p3_dev, 7);

	ret = p3_regulator_onoff(p3_dev, 0);
	if(ret < 0)
		P3_ERR_MSG(" test: failed to turn off LDO()\n");

	filp->private_data = p3_dev;
	P3_DBG_MSG("%s : Major No: %d, Minor No: %d\n", __func__,
			imajor(inode), iminor(inode));
	return 0;
}

/**
 * \ingroup spi_driver
 * \brief To configure the P3_SET_PWR/P3_SET_DBG/P3_SET_POLL
 * \n		  P3_SET_PWR - hard reset (arg=2), soft reset (arg=1)
 * \n		  P3_SET_DBG - Enable/Disable (based on arg value) the driver logs
 * \n		  P3_SET_POLL - Configure the driver in poll (arg = 1), interrupt (arg = 0) based read operation
 * \param[in]       struct file *
 * \param[in]       unsigned int
 * \param[in]       unsigned long
 *
 * \retval 0 if ok.
 *
*/

static long p3_dev_ioctl(struct file *filp, unsigned int cmd,
        unsigned long arg)
{
	int ret = 0;
	struct p3_dev *p3_dev = NULL;
#ifdef P3_RST_ENABLE
	unsigned char buf[100];
#endif

	if (_IOC_TYPE(cmd) != P3_MAGIC) {
		P3_ERR_MSG("%s invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
			__func__, cmd, _IOC_TYPE(cmd), P3_MAGIC);
		return -ENOTTY;
	}
	/*P3_DBG_MSG(KERN_ALERT "p3_dev_ioctl-Enter %u arg = %ld\n", cmd, arg);*/
	p3_dev = filp->private_data;

	mutex_lock(&p3_dev->buffer_mutex);
	switch (cmd) {
	case P3_SET_PWR:
		if (arg == 2)
		{

		}
		break;

	case P3_SET_DBG:
		debug_level = (unsigned char )arg;
		P3_DBG_MSG(KERN_INFO"[NXP-P3] -  Debug level %d", debug_level);
		break;
	case P3_SET_POLL:
		p3_dev-> enable_poll_mode = (unsigned char )arg;
		if (p3_dev-> enable_poll_mode == 0)
		{
			P3_DBG_MSG(KERN_INFO"[NXP-P3] - IRQ Mode is set \n");
		}
		else
		{
			P3_DBG_MSG(KERN_INFO"[NXP-P3] - Poll Mode is set \n");
			p3_dev->enable_poll_mode = 1;
		}
		break;
	/*non TZ */
	case P3_SET_SPI_CONFIG:
		/*p3_ioctl_config_spi_gpio(p3_dev);*/
		break;
	case P3_ENABLE_SPI_CLK:
		P3_DBG_MSG("%s P3_ENABLE_SPI_CLK\n", __func__);
		ret = p3_enable_clk(p3_dev);
		break;
	case P3_DISABLE_SPI_CLK:
		P3_DBG_MSG("%s P3_DISABLE_SPI_CLK\n", __func__);
		ret = p3_disable_clk(p3_dev);
		break;
	case P3_ENABLE_SPI_CS:
		/*P3_DBG_MSG("%s P3_ENABLE_SPI_CS\n", __func__);*/
		ret = p3_enable_cs(p3_dev);
		break;
	case P3_DISABLE_SPI_CS:
		/*P3_DBG_MSG("%s P3_DISABLE_SPI_CS\n", __func__);*/
		ret = p3_disable_cs(p3_dev);
		break;

	case P3_RW_SPI_DATA:
		ret = p3_rw_spi_message(p3_dev, arg);
		if (ret < 0)
			P3_ERR_MSG("%s P3_RW_SPI_DATA failed [%d].\n",__func__,ret);
		break;
	case P3_SET_SPI_CLK: /* To change CLK */
		ret = p3_set_clk(p3_dev, arg);
		if (ret < 0)
			P3_ERR_MSG("%s P3_SET_SPI_CLK failed [%d].\n",__func__,ret);
		break;

	case P3_ENABLE_CLK_CS:
		P3_DBG_MSG("%s P3_ENABLE_CLK_CS\n", __func__);
		ret = p3_enable_clk_cs(p3_dev);
		break;
	case P3_DISABLE_CLK_CS:
		P3_DBG_MSG("%s P3_DISABLE_CLK_CS\n", __func__);
		ret = p3_disable_clk_cs(p3_dev);
		break;

	case P3_SWING_CS:
		p3_swing_on_cs(p3_dev, (int)arg);
		P3_DBG_MSG("%s P3_SWING_CS set %d\n", __func__, (int)arg);
		break;

	default:
		P3_DBG_MSG("%s no matching ioctl!\n", __func__);
		ret = -EINVAL;
	}
	mutex_unlock(&p3_dev->buffer_mutex);

	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Write data to P3 on SPI
 *
 * \param[in]       struct file *
 * \param[in]       const char *
 * \param[in]       size_t
 * \param[in]       loff_t * 
 *
 * \retval data size
 *
*/

static ssize_t p3_dev_write(struct file *filp, const char *buf, size_t count,
        loff_t *offset)
{
	int ret = -1;
	struct p3_dev *p3_dev;
	unsigned char tx_buffer[MAX_BUFFER_SIZE];

	P3_DBG_MSG(KERN_ALERT "p3_dev_write -Enter count %zu\n", count);

	p3_dev = filp->private_data;

	mutex_lock(&p3_dev->buffer_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(&tx_buffer[0], 0, sizeof(tx_buffer));
	if (copy_from_user(&tx_buffer[0], &buf[0], count))
	{
		P3_ERR_MSG("%s : failed to copy from user space\n", __func__);
		mutex_unlock(&p3_dev->buffer_mutex);
		return -EFAULT;
	}

#if 1 /*  CS control*/
	gpio_set_value(p3_dev->cs_gpio, 0);
	usleep_range(90,100);
#endif 

	/* Write data */
	ret = spi_write(p3_dev->spi, &tx_buffer[0], count);
	if (ret < 0)
	{
		ret = -EIO;
	}
	else
	{
		ret = count;
	}
#if 1 /*  CS control*/
	gpio_set_value(p3_dev->cs_gpio, 1);
#endif

	mutex_unlock(&p3_dev->buffer_mutex);
	P3_DBG_MSG(KERN_ALERT "p3_dev_write ret %d- Exit \n", ret);
	return ret;
}

#ifdef P3_IRQ_ENABLE

/**
 * \ingroup spi_driver
 * \brief To disable IRQ
 *
 * \param[in]       struct p3_dev *
 *
 * \retval void
 *
*/

static void p3_disable_irq(struct p3_dev *p3_dev)
{
	unsigned long flags;

	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);

	spin_lock_irqsave(&p3_dev->irq_enabled_lock, flags);
	if (p3_dev->irq_enabled)
	{
		disable_irq_nosync(p3_dev->spi->irq);
		p3_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&p3_dev->irq_enabled_lock, flags);

	P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

/**
 * \ingroup spi_driver
 * \brief Will get called when interrupt line asserted from P3
 *
 * \param[in]       int
 * \param[in]       void * 
 *
 * \retval IRQ handle
 *
*/

static irqreturn_t p3_dev_irq_handler(int irq, void *dev_id)
{
	struct p3_dev *p3_dev = dev_id;

	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);
	p3_disable_irq(p3_dev);

	/* Wake up waiting readers */
	wake_up(&p3_dev->read_wq);

	P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return IRQ_HANDLED;
}
#endif

/**
 * \ingroup spi_driver
 * \brief Used to read data from P3 in Poll/interrupt mode configured using ioctl call
 *
 * \param[in]       struct file *
 * \param[in]       char *
 * \param[in]       size_t
 * \param[in]       loff_t * 
 *
 * \retval read size
 *
*/

static ssize_t p3_dev_read(struct file *filp, char *buf, size_t count,
        loff_t *offset)
{
	int ret = -EIO;
	struct p3_dev *p3_dev = filp->private_data;
	unsigned char rx_buffer[MAX_BUFFER_SIZE];

	P3_DBG_MSG("p3_dev_read count %zu - Enter \n", count);

	mutex_lock(&p3_dev->buffer_mutex);

	memset(&rx_buffer[0], 0x00, sizeof(rx_buffer));

	P3_DBG_MSG(KERN_INFO"Data Lenth = %zu", count);

#if 1 /*  CS control*/
	gpio_set_value(p3_dev->cs_gpio, 0);
	usleep_range(70,90);
#endif

	/* Read the availabe data along with one byte LRC */
	ret = spi_read(p3_dev->spi, (void *)rx_buffer, count);
	if (ret < 0)
	{
		P3_ERR_MSG("spi_read failed \n");
		ret = -EIO;
		goto fail;
	}
#if 1 /*  CS control*/
	gpio_set_value(p3_dev->cs_gpio, 1);
#endif

	/*P3_DBG_MSG(KERN_INFO"total_count = %d", total_count);*/

	if (copy_to_user(buf, &rx_buffer[0], count))
	{
		P3_ERR_MSG("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
		goto fail;
	}
	ret = count;
	P3_DBG_MSG("%s ret %d Exit\n",__func__, ret);

	mutex_unlock(&p3_dev->buffer_mutex);

	return ret;

	fail:
	P3_ERR_MSG("Error %s ret %d Exit\n", __func__, ret);
	mutex_unlock(&p3_dev->buffer_mutex);
	return ret;
}


/**
 * \ingroup spi_driver
 * \brief It will configure the GPIOs required for soft reset, read interrupt & regulated power supply to P3.
 *
 * \param[in]       struct p3_spi_platform_data *
 * \param[in]       struct p3_dev *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
*/
#if 0
static int p3_hw_setup(struct p3_spi_platform_data *platform_data,
       struct p3_dev *p3_dev, struct spi_device *spi)
{
    int ret = -1;

    P3_DBG_MSG("Entry : %s\n", __FUNCTION__);
#ifdef P3_IRQ_ENABLE
    ret = gpio_request(platform_data->irq_gpio, "p3 irq");
    if (ret < 0)
    {
        P3_ERR_MSG("gpio request failed gpio = 0x%x\n", platform_data->irq_gpio);
        goto fail;
    }

    ret = gpio_direction_input(platform_data->irq_gpio);
    if (ret < 0)
    {
        P3_ERR_MSG("gpio request failed gpio = 0x%x\n", platform_data->irq_gpio);
        goto fail_irq;
    }
#endif

#ifdef P3_HARD_RESET
    /* RC : platform specific settings need to be declare */
    p3_regulator = regulator_get( &spi->dev, "vaux3");
    if (IS_ERR(p3_regulator))
    {
        ret = PTR_ERR(p3_regulator);
        P3_ERR_MSG(" Error to get vaux3 (error code) = %d\n", ret);
        return  -ENODEV;
    }
    else
    {
        P3_DBG_MSG("successfully got regulator\n");
    }

    ret = regulator_set_voltage(p3_regulator, 1800000, 1800000);
    if (ret != 0)
    {
        P3_ERR_MSG("Error setting the regulator voltage %d\n", ret);
        regulator_put(p3_regulator);
        return ret;
    }
    else
    {
        regulator_enable(p3_regulator);
        P3_DBG_MSG("successfully set regulator voltage\n");

    }
#endif
#ifdef P3_RST_ENABLE
    ret = gpio_request( platform_data->rst_gpio, "p3 reset");
    if (ret < 0)
    {
        P3_ERR_MSG("gpio reset request failed = 0x%x\n", platform_data->rst_gpio);
        goto fail_gpio;
    }

    ret = gpio_direction_output(platform_data->rst_gpio,0);
    if (ret < 0)
    {
        P3_ERR_MSG("gpio rst request failed gpio = 0x%x\n", platform_data->rst_gpio);
        goto fail_gpio;
    }
#endif

    ret = 0;
    P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
    return ret;

#ifdef P3_RST_ENABLE
    fail_gpio:
    gpio_free(platform_data->rst_gpio);
#endif
#ifdef P3_IRQ_ENABLE
    fail_irq:
    gpio_free(platform_data->irq_gpio);
    fail:
    P3_ERR_MSG("hw_setup failed\n");
#endif
    return ret;
}
#endif

/**
 * \ingroup spi_driver
 * \brief Set the P3 device specific context for future use.
 *
 * \param[in]       struct spi_device *
 * \param[in]       void *
 *
 * \retval void
 *
*/

static inline void p3_set_data(struct spi_device *spi, void *data)
{
	dev_set_drvdata(&spi->dev, data);
}

/**
 * \ingroup spi_driver
 * \brief Get the P3 device specific context.
 *
 * \param[in]       const struct spi_device *
 *
 * \retval Device Parameters
 *
*/

static inline void *p3_get_data(const struct spi_device *spi)
{
	return dev_get_drvdata(&spi->dev);
}

/* possible fops on the p3 device */
static const struct file_operations p3_dev_fops = {
	.owner = THIS_MODULE,
	.read = p3_dev_read,
	.write = p3_dev_write,
	.open = p3_dev_open,
	.release = p3_dev_release,
	.unlocked_ioctl = p3_dev_ioctl,
};

#if 0
static ssize_t p3_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	//struct spi_device *spi = to_spi_device(dev);

	//ret = spi_read(p3_dev->spi, (void *)&sof, 1);

	P3_DBG_MSG("%s\n", __func__);
	data='a';
	return sprintf(buf, "%d\n", data);
}

static ssize_t p3_test_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	//struct spi_device *spi = to_spi_device(dev);
	

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	P3_DBG_MSG("%s [%lu]\n", __func__, data);


	
	return count;
}

static DEVICE_ATTR(test, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		p3_test_show, p3_test_store);
#endif

static int p3_parse_dt(struct device *dev,
	struct p3_dev *data)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int errorno = 0;
	//int gpio;

#if 0//def ENABLE_SENSORS_FPRINT_SECURE
	if (of_property_read_u32(np, "p3-mosipin", &data->mosipin))
		data->mosipin = 0;
	if (of_property_read_u32(np, "p3-misopin", &data->misopin))
		data->misopin = 0;
	if (of_property_read_u32(np, "p3-cspin", &data->cspin))
		data->cspin = 0;
	if (of_property_read_u32(np, "p3-clkpin", &data->clkpin))
		data->clkpin = 0;

	//data->tz_mode = true;
#endif
	data->cs_gpio = of_get_named_gpio_flags(np,
		"p3-cs-gpio", 0, &flags);
	if (data->cs_gpio < 0) {
		P3_ERR_MSG("%s - get cs_gpio error\n", __func__);
		//return -ENODEV;
	}
	else {
		errorno = gpio_request(data->cs_gpio, "ese_cs");
		if (errorno) {
			P3_ERR_MSG("%s - failed to request ese_cs\n", __func__);
			//sreturn errorno;
		}
#if 1 /* Keep CS up*/
		gpio_direction_output(data->cs_gpio, 1);
#else /*CS down for later power up */
		gpio_direction_output(data->cs_gpio, 0);
#endif
	}

	P3_DBG_MSG("%s: mosipin=%d, %d, %d, %d cs_gpio:%d\n", __func__,
		data->mosipin, data->misopin, data->cspin, data->clkpin, data->cs_gpio);
//dt_exit:
	return errorno;
}

/**
 * \ingroup spi_driver
 * \brief To probe for P3 SPI interface. If found initialize the SPI clock, bit rate & SPI mode. 
		  It will create the dev entry (P3) for user space.
 *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
*/

static int p3_probe(struct spi_device *spi)
{
	int ret = -1;
	//struct p3_spi_platform_data *platform_data = NULL;
	//struct p3_spi_platform_data platform_data1;
	struct p3_dev *p3_dev = NULL;

	P3_DBG_MSG("%s chip select : %d , bus number = %d \n",
		__FUNCTION__, spi->chip_select, spi->master->bus_num);

	p3_dev = kzalloc(sizeof(*p3_dev), GFP_KERNEL);
	if (p3_dev == NULL)
	{
		P3_ERR_MSG("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = p3_regulator_onoff(p3_dev, 1);
	if (ret) {
		P3_ERR_MSG("%s - Failed to enable regulator\n", __func__);
		goto p3_parse_dt_failed;
	}

	ret = p3_parse_dt(&spi->dev, p3_dev);
	if (ret) {
		P3_ERR_MSG("%s - Failed to parse DT\n", __func__);
		goto p3_parse_dt_failed;
	}
	P3_DBG_MSG("%s: tz_mode=%d, isGpio_cfgDone:%d\n", __func__,
			p3_dev->tz_mode, p3_dev->isGpio_cfgDone);

	//P3_DBG_MSG("%s ------\n", __FUNCTION__);	
#if 0
	ret = p3_hw_setup (platform_data, p3_dev, spi);
	if (ret < 0)
	{
		P3_ERR_MSG("Failed to enable IRQ_ENABLE\n");
		goto err_exit0;
	}
#endif
	//usleep_range(3000,3100);
	//gpio_direction_output(p3_dev->cs_gpio, 1);

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;
	spi->max_speed_hz = 1000000L;
	ret = spi_setup(spi);
	if (ret < 0)
	{
		P3_ERR_MSG("failed to do spi_setup()\n");
		goto p3_spi_setup_failed;
	}

	p3_dev -> spi = spi;
	p3_dev -> p3_device.minor = MISC_DYNAMIC_MINOR;
	p3_dev -> p3_device.name = "p3";
	p3_dev -> p3_device.fops = &p3_dev_fops;
	p3_dev -> p3_device.parent = &spi->dev;

	dev_set_drvdata(&spi->dev, p3_dev);

	/* init mutex and queues */
	init_waitqueue_head(&p3_dev->read_wq);
	mutex_init(&p3_dev->buffer_mutex);
	spin_lock_init(&p3_dev->ese_spi_lock);
#ifdef FEATURE_ESE_WAKELOCK
	wake_lock_init(&p3_dev->ese_lock,
		WAKE_LOCK_SUSPEND, "ese_wake_lock");
#endif

#ifdef P3_IRQ_ENABLE
	spin_lock_init(&p3_dev->irq_enabled_lock);
#endif

	ret = misc_register(&p3_dev->p3_device);
	if (ret < 0)
	{
		P3_ERR_MSG("misc_register failed! %d\n", ret);
		goto err_exit0;
	}

#if 0 //test
{
	struct device *dev;

	p3_device_class = class_create(THIS_MODULE, "ese");
	if (IS_ERR(p3_device_class)) {
		P3_ERR_MSG("%s class_create() is failed:%lu\n",
			__func__,  PTR_ERR(p3_device_class));
		//status = PTR_ERR(p3_device_class);
		//goto vfsspi_probe_class_create_failed;
	}
	dev = device_create(p3_device_class, NULL,
			    0, p3_dev, "p3");
		P3_ERR_MSG("%s device_create() is failed:%lu\n",
			__func__,  PTR_ERR(dev));

	if ((device_create_file(dev, &dev_attr_test)) < 0)
		P3_ERR_MSG("%s device_create_file failed !!!\n", __func__); 
	else
		P3_DBG_MSG("%s device_create_file success.\n", __func__);
	//ret = sysfs_create_group(&spi->dev.kobj,
	//		&p3_attribute_group);
	//if (ret < 0)
	//	P3_ERR_MSG("%s class_create() is failed - \n",
	//		__func__,  PTR_ERR(p3_device_class));
}
#endif

#ifdef ENABLE_ESE_P3_EXYNO_SPI
{
	void __iomem *gpg4con, *gpg4dat;

	gpg4con = ioremap(0x14c90000, SZ_4);
	gpg4dat = ioremap(0x14c90004, SZ_4);
	__raw_writel(0x1111, gpg4con);
	__raw_writel(0x0, gpg4dat);

	iounmap(gpg4con);
	iounmap(gpg4dat);
}
#endif
	gpio_direction_output(p3_dev->cs_gpio, 0);

	ret = p3_regulator_onoff(p3_dev, 0);
	if(ret < 0)
		P3_ERR_MSG(" test: failed to turn off LDO()\n");

	p3_dev-> enable_poll_mode = 1; /* Default IRQ read mode */
	P3_DBG_MSG("%s finished...\n", __FUNCTION__);
	return ret;

	/*err_exit1:*/
	misc_deregister(&p3_dev->p3_device);
	err_exit0:
#ifdef FEATURE_ESE_WAKELOCK
	wake_lock_destroy(&p3_dev->ese_lock);
#endif
	mutex_destroy(&p3_dev->buffer_mutex);
	p3_spi_setup_failed:
	p3_parse_dt_failed:
	kfree(p3_dev);
	err_exit:
	P3_DBG_MSG("ERROR: Exit : %s ret %d\n", __FUNCTION__, ret);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Will get called when the device is removed to release the resources.
 *
 * \param[in]       struct spi_device
 *
 * \retval 0 if ok.
 *
*/

static int p3_remove(struct spi_device *spi)
{
	struct p3_dev *p3_dev = p3_get_data(spi);

	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);
	if (p3_dev == NULL)
	{
		P3_ERR_MSG("%s p3_dev is null!\n", __func__);
		return 0;
	}
#ifdef P3_HARD_RESET
	if (p3_regulator != NULL)
	{
		regulator_disable(p3_regulator);
		regulator_put(p3_regulator);
	}
	else
	{
		P3_ERR_MSG("ERROR %s regulator not enabled \n", __FUNCTION__);
	}
#endif
#ifdef P3_RST_ENABLE
	gpio_free(p3_dev->rst_gpio);
#endif

#ifdef P3_IRQ_ENABLE
	free_irq(p3_dev->spi->irq, p3_dev);
	gpio_free(p3_dev->irq_gpio);
#endif

#ifdef FEATURE_ESE_WAKELOCK
	wake_lock_destroy(&p3_dev->ese_lock);
#endif
	mutex_destroy(&p3_dev->buffer_mutex);
	misc_deregister(&p3_dev->p3_device);

	kfree(p3_dev);
	P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id p3_match_table[] = {
	{ .compatible = "ese_p3",},
	{},
};
#else
#define ese_match_table NULL
#endif

static struct spi_driver p3_driver = {
	.driver = {
		.name = "p3",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = p3_match_table,
#endif
        },
	.probe =  p3_probe,
	.remove = p3_remove,
};

/**
 * \ingroup spi_driver
 * \brief Module init interface
 *
 * \param[in]       void
 *
 * \retval handle
 *
*/

static int __init p3_dev_init(void)
{
	debug_level = P3_FULL_DEBUG;

	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);
#if (!defined(CONFIG_ESE_FACTORY_ONLY) || defined(CONFIG_SEC_FACTORY))
	return spi_register_driver(&p3_driver);
#else
	return -1;
#endif
}

/**
 * \ingroup spi_driver
 * \brief Module exit interface
 *
 * \param[in]       void
 *
 * \retval void
 *
*/

static void __exit p3_dev_exit(void)
{
	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);

	spi_unregister_driver(&p3_driver);
	P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

module_init( p3_dev_init);
module_exit( p3_dev_exit);

MODULE_AUTHOR("Sec");
MODULE_DESCRIPTION("Oberthur ese SPI driver");
MODULE_LICENSE("GPL");

/** @} */
