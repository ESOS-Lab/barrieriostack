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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/list.h>

#include <linux/kernel.h>	/* printk()		*/
#include <linux/slab.h>		/* kmalloc()		*/
#include <linux/fs.h>		/* everything		*/
#include <linux/errno.h>	/* error codes		*/
#include <linux/types.h>	/* size_t		*/
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE		*/
#include <linux/sysfs.h>
#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>

#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/poll.h>

#include "bbd_internal.h"
#include "transport/transport_layer_c.h"

struct  bbd_control_device* bbd_control_ptr    (void);
struct  bbd_base*           bbd_sensor_ptr_base(void);
struct  bbd_sensor_device*  bbd_sensor_ptr     (void);
ssize_t bbd_ESW_control     (char *buf, ssize_t len);
struct  bbd_ssi_spi_debug_device*  bbd_ssi_spi_debug_ptr(void);

/*****************************************************************************
*
* sysfs for bbd
* sysfs directory : /sys/devices/platform/bbd/
* files : BBD = send control to lhd
*         ESW = send control to ssp
*         bbd_info = display information of bbd driver
*
******************************************************************************/

static ssize_t store_sysfs_BBD_control(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
        bbd_on_read(BBD_MINOR_CONTROL, buf, len);
	return len;
}

static ssize_t store_sysfs_ESW_control(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
        struct bbd_sensor_device* ps = bbd_sensor_ptr();
	/* struct bbd_device *gpbbd_dev = dev_get_drvdata(dev); */

	if (!ps || !ps->callbacks || !buf) {
		printk(KERN_ERR "%s():callbacks not registered.\n", __func__);
		return -EFAULT;
	}

        {
                char str_ctrl[BBD_ESW_CTRL_MAX_STR_LEN];
		strlcpy(str_ctrl, buf, BBD_ESW_CTRL_MAX_STR_LEN);
                bbd_ESW_control(str_ctrl, strlen(buf) + 1);
        }
	return len;
}

/*      Same as ESW, except callbacks can be missing.
 */

static ssize_t store_sysfs_DEV_control(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
        struct bbd_sensor_device* ps = bbd_sensor_ptr();
	if (ps && buf) {
                char str_ctrl[BBD_ESW_CTRL_MAX_STR_LEN];
		strlcpy(str_ctrl, buf, BBD_ESW_CTRL_MAX_STR_LEN);
                bbd_ESW_control(str_ctrl, strlen(buf) + 1);
        }
        else
		len = -EFAULT;
	return len;
}

static ssize_t show_sysfs_bbd_shmd(struct device *dev,
				struct device_attribute *attr, char *buf)
{
        struct bbd_base* p  = bbd_sensor_ptr_base();
	struct bbd_queue  *ptxq = NULL;
	struct bbd_buffer *prxb = NULL;
	int len = 0;

	if (!p)
		return -EFAULT;

	ptxq = &p->txq;
	prxb = &p->rxb;

	len = sprintf(buf, "Tx:total=%dB,tout=%d, Rx:total=%dB, tout=%d\n",
				ptxq->count, ptxq->tout_cnt,
				prxb->count, prxb->tout_cnt);
	return len;
}

static ssize_t show_sysfs_bbd_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (!gpbbd_dev)
		return 0;
	return sprintf(buf, "%d", gpbbd_dev->db);
}

static ssize_t store_sysfs_bbd_debug(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
        int debug = 0;
	if (!gpbbd_dev)
		return 0;
        sscanf(buf, "%d", &debug);
	gpbbd_dev->db = debug;
	return len;
}

static ssize_t show_sysfs_bbd_baud(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (!gpbbd_dev)
		return 0;
	return sprintf(buf, "%d", gpbbd_dev->tty_baud);
}

static ssize_t store_sysfs_bbd_baud(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
        int tty_baud = 0;
	if (!gpbbd_dev)
		return 0;
        sscanf(buf, "%d", &tty_baud);
	gpbbd_dev->tty_baud = tty_baud;
	return len;
}

static ssize_t show_sysfs_bbd_passthru(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (!gpbbd_dev)
		return 0;
	return sprintf(buf, "%d",
                gpbbd_dev->bbd_engine.bridge.m_otTL.m_bPassThrough);
}

static ssize_t show_sysfs_bbd_pl(struct device *dev,
				 struct device_attribute *attr,
                                 char   *buf)
{
        const struct stTransportLayerStats *p = 0;
	if (!gpbbd_dev)
		return 0;
        p = TransportLayer_GetStats(&gpbbd_dev->bbd_engine.bridge.m_otTL);
        if (!p)
		return 0;

	return sprintf(buf,
                    "RxGarbageBytes=%lu\n"
                    "RxPacketLost=%lu\n"
                    "RemotePacketLost=%lu\n"
                    "RemoteGarbage=%lu\n"
                    "PacketSent=%lu\n"
                    "PacketReceived=%lu\n"
                    "AckReceived=%lu\n"
                    "ReliablePacketSent=%lu\n"
                    "ReliableRetransmit=%lu\n"
                    "ReliablePacketReceived=%lu\n"
                    "MaxRetransmitCount=%lu\n",
            p->ulRxGarbageBytes,
            p->ulRxPacketLost,
            p->ulRemotePacketLost, /* approximate */
            p->ulRemoteGarbage, /* approximate */
            p->ulPacketSent,         /* number of normal packets sent */
            p->ulPacketReceived,
            p->ulAckReceived,
            p->ulReliablePacketSent,
            p->ulReliableRetransmit,
            p->ulReliablePacketReceived,
            p->ulMaxRetransmitCount);
}

/*      Show the number of buffers
 */

int bbd_count(int minor);
int bbd_alloc_count(void);

static ssize_t show_sysfs_bbd_buf(struct device *dev,
				 struct device_attribute *attr,
                                 char   *buf)
{
	if (!gpbbd_dev)
		return 0;
	return sprintf(buf,
                    "sensor=%d\n"
                    "control=%d\n"
                    "packet=%d\n"
                    "reliable=%d\n"
                    "freed=%d\n"
                    "nAlloc=%d\n",
            bbd_count(BBD_MINOR_SENSOR),
            bbd_count(BBD_MINOR_CONTROL),
            bbd_count(BBD_MINOR_PACKET),
            bbd_count(BBD_MINOR_RELIABLE),
            gpbbd_dev->qcount,
	    bbd_alloc_count());
}

static ssize_t show_sysfs_bbd_ssi_xfer(struct device *dev,
				 struct device_attribute *attr,
                                 char   *buf)
{
        struct  bbd_ssi_spi_debug_device* p = bbd_ssi_spi_debug_ptr();
	if (!gpbbd_dev || !p)
		return 0;
	return sprintf(buf, "%lu\n", p->numXfers);
}

static ssize_t show_sysfs_bbd_ssi_count(struct device *dev,
				 struct device_attribute *attr,
                                 char   *buf)
{
        struct  bbd_ssi_spi_debug_device* p = bbd_ssi_spi_debug_ptr();
	if (!gpbbd_dev || !p)
		return 0;
	return sprintf(buf, "%lu\n", p->numBytes);
}

static ssize_t show_sysfs_bbd_ssi_trace(struct device *dev,
				        struct device_attribute *attr,
                                        char   *buf)
{
        struct bbd_ssi_spi_debug_device* d = bbd_ssi_spi_debug_ptr();
        const unsigned char* p = 0;
	if (!gpbbd_dev || !d || !d->numXfers)
		return 0;
        p = (const unsigned char*) &d->trace;
        return sprintf(buf,
                "a { %02X %02X %02X   %02X.%02X.%02X.%02X  %02X.%02X  %02X }\n"
                "b { %02X %02X %02X   %02X... }\n",
                  p[ 0],p[ 1],p[ 2],  p[ 3],p[4],p[5],p[6], p[7],p[8], p[9],
                  p[10],p[11],p[12],  p[13]);
}

static DEVICE_ATTR(BBD,     0222, NULL,                store_sysfs_BBD_control);
static DEVICE_ATTR(ESW,     0222, NULL,                store_sysfs_ESW_control);
static DEVICE_ATTR(DEV,     0222, NULL,                store_sysfs_DEV_control);
static DEVICE_ATTR(debug,   0666, show_sysfs_bbd_debug,store_sysfs_bbd_debug);
static DEVICE_ATTR(baud,    0666, show_sysfs_bbd_baud, store_sysfs_bbd_baud);
static DEVICE_ATTR(shmd,    0444, show_sysfs_bbd_shmd,     NULL);
static DEVICE_ATTR(pl,      0444, show_sysfs_bbd_pl,       NULL);
static DEVICE_ATTR(buf,     0444, show_sysfs_bbd_buf,      NULL);
static DEVICE_ATTR(passthru,0444, show_sysfs_bbd_passthru, NULL);
static DEVICE_ATTR(ssi_xfer,    0444, show_sysfs_bbd_ssi_xfer,  NULL);
static DEVICE_ATTR(ssi_count,   0444, show_sysfs_bbd_ssi_count, NULL);
static DEVICE_ATTR(ssi_trace,   0444, show_sysfs_bbd_ssi_trace, NULL);

static struct attribute *bbd_esw_attributes[] = {
	&dev_attr_BBD.attr,
	&dev_attr_ESW.attr,
	&dev_attr_DEV.attr,
	&dev_attr_shmd.attr,
	&dev_attr_debug.attr,
	&dev_attr_baud.attr,
	&dev_attr_pl.attr,
	&dev_attr_buf.attr,
	&dev_attr_passthru.attr,
	&dev_attr_ssi_xfer.attr,
	&dev_attr_ssi_count.attr,
	&dev_attr_ssi_trace.attr,
	NULL
};

static const struct attribute_group bbd_esw_group = {
	.attrs = bbd_esw_attributes,
};

int bbd_sysfs_init(struct kobject *kobj)
{
	int ret = sysfs_create_group(kobj, &bbd_esw_group);
	if (ret < 0) {
		printk(KERN_ERR "%s():couldn't register current attribute\n",
						__func__);
	}
        return ret;
}

void bbd_sysfs_uninit(struct kobject *kobj)
{
	sysfs_remove_group(kobj, &bbd_esw_group);
}
