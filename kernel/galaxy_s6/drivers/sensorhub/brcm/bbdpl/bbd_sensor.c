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

#include "bbd_internal.h"

struct bbd_control_device *bbd_control_ptr(void);

struct bbd_sensor_device *bbd_sensor_ptr(void)
{
	if (!gpbbd_dev)
		return 0;
	return (struct bbd_sensor_device *) gpbbd_dev->bbd_ptr[BBD_MINOR_SENSOR];
}

struct bbd_base *bbd_sensor_ptr_base(void)
{
        struct bbd_sensor_device *ps = bbd_sensor_ptr();
	if (!ps)
		return 0;
	return &ps->base;
}

/**
 * initialize sensor-specific variables
 *
 * @pbbd_dev: bbd driver pointer
 *
 * @return: 0 = success, others = failure
 */

void bbd_sensor_init_vars(struct bbd_device *dev, int* result)
{
        struct bbd_sensor_device *p = 0;

        if (*result)
                return;

	p = bbd_alloc(sizeof(struct bbd_sensor_device));
        if (!p) {
		*result = -ENOMEM;
		return;
        }

        bbd_base_init_vars(dev, &p->base, result);
        if (*result) {
                bbd_free(p);
                return;
        }
        p->priv_data = 0;
        p->callbacks = 0;
        dev->bbd_ptr[BBD_MINOR_SENSOR] = p;
        p->base.name = "sensor";
}

int bbd_sensor_uninit(void)
{
        struct bbd_sensor_device* p = bbd_sensor_ptr();
        int freed = 0;
        if (p) {
                freed = bbd_base_uninit(&p->base);
                bbd_free(p);
                gpbbd_dev->bbd_ptr[BBD_MINOR_SENSOR] = 0;
        }
        return freed;
}

/**
 * external driver registers callback function for getting data from bbd driver
 *
 * @ssh_data: sensor hub driver data pointer
 * @pcallbacks: callback function list
 */
void bbd_register(void *ssh_data, bbd_callbacks *pcallbacks)
{
        struct bbd_sensor_device *ps = bbd_sensor_ptr();
	if (!ps)
		return;
	ps->priv_data = ssh_data;
        ps->callbacks = pcallbacks;
}
EXPORT_SYMBOL(bbd_register);


/**
 * send packet from external (SHMD) driver to bbd driver
 *
 * @pbuff: buffer pointer
 * @len:   buffer length
 *
 * @return: pushed data length = success
 */

ssize_t bbd_send_packet(unsigned char *pbuff, size_t size)
{
        if (gpbbd_dev->shmd)
                return BbdEngine_SendSensorData(&gpbbd_dev->bbd_engine,
                                                pbuff, size);
        // Stage 1 fallback:
	return bbd_on_read(BBD_MINOR_SENSOR, pbuff, size);
}

EXPORT_SYMBOL(bbd_send_packet);

#if 0
ssize_t bbd_send_packet_to_user(unsigned char *pbuff, size_t size)
{
        int timeout = 0;

        /* Wake up poll_wait() for reading data from user */
        timeout = wait_event_interruptible_timeout(
                        ptxb->comp_inq,
                        (ptxb->buff_size == 0),
                        msecs_to_jiffies(BBD_PACKET_TIMEOUT));

        if (timeout == 0 || ptxb->buff_size < 0) {
                mutex_unlock(&ptxb->lock);
                ++ptxb->tout_cnt;
                dprint("TIMEOUT %d  size %d rdy %d TO_OK\n",
                                timeout, ptxb->buff_size, pc->esw_ready);
                mutex_unlock(&ptxb->lock);
                return -ETIMEDOUT;
        }
        return res;
}
#endif

/**
 * pull packet from rx_list buffer directly
 *
 * @pbuff: buffer pointer
 * @len  : length of pulling data
 * @timeout_ms: timeout for waiting data in rx_list buffer
 *              if timeout_ms is 0, pull one packet from rx_list buffer directly
 *
 * @return: length of buffer, -ETIMEDOUT = timeout
 */
ssize_t bbd_pull_packet(unsigned char *pbuff,
			size_t         len,
			unsigned int   timeout_ms)
{
	struct bbd_buffer *prxb = NULL;
	int pull_size = 0;
	int timeout = 0;
        struct bbd_base* p = bbd_sensor_ptr_base();

	if (!p || pbuff == NULL)
		return -EINVAL;

	prxb = &p->rxb;
#if 0
	//If SSP wants to wait for data and starts read, buff_size can be 0.
	//So, we should remove following if() clause
	if (prxb->buff_size <= 0) {
		printk("error : prxb->buff_size = %d\n", prxb->buff_size);
		return prxb->buff_size;
	}
#endif
	if (timeout_ms) {
		timeout = wait_event_interruptible_timeout(
						prxb->poll_inq,
						(prxb->buff_size > 0),
						msecs_to_jiffies(timeout_ms));
		if (!timeout) {
			//Following line is bug, we should NEVER set buff_size to invalid value like errno. It's bad hack.
			//prxb->buff_size = -ETIMEDOUT;
			prxb->buff_size = 0;
			return -ETIMEDOUT;
		}
	}

	mutex_lock(&prxb->lock);
	pull_size = MIN(prxb->buff_size, len);
	memcpy(pbuff, prxb->pbuff, pull_size);
	prxb->buff_size -= pull_size;

	//shift. TODO: circ buf
	{
		unsigned char *top =  prxb->pbuff + pull_size;
		memcpy(prxb->pbuff, top, prxb->buff_size);
	}
	mutex_unlock(&prxb->lock);

	wake_up(&prxb->comp_inq);

	return pull_size;
}
EXPORT_SYMBOL(bbd_pull_packet);

/**
 * open bbd driver
 */
int bbd_sensor_open(struct inode *inode, struct file *filp)
{
	if (!gpbbd_dev)
		return -EFAULT;

	filp->private_data = gpbbd_dev;

	return 0;
}

/**
 * close bbd driver
 */
int bbd_sensor_release(struct inode *inode, struct file *filp)
{
	if (gpbbd_dev)
                return bbd_base_reinit(BBD_MINOR_SENSOR);
        return -EFAULT;
}

/**
 * read sensor data from bbd_list_head to user
 * (This should be used only when shmd-->bridge is disabled
 */
ssize_t bbd_sensor_read(struct file *filp,
			char __user *buf,
			size_t size, loff_t *ppos)
{
        return bbd_read(BBD_MINOR_SENSOR, buf, size, true);
}

static ssize_t bbd_sensor_write_handshake(struct bbd_sensor_device *ps,
                                          struct bbd_buffer        *prxb)
{
//	int timeout = 0;
//	int wr_size = 0;

	if (!ps->callbacks)
            return 0;

//	wr_size = prxb->buff_size;

	/* If registred on_packet_alarm: Inform alarm to ssh driver.
	 * ssh driver can pull the received data from rx_list directly
         */
	if (ps->callbacks && ps->callbacks->on_packet_alarm) {
		/* dprint("call alarm function\n"); */
		ps->callbacks->on_packet_alarm(ps->priv_data);
		wake_up(&prxb->poll_inq);
	}

#if 0	//Don't wait. It can cause rx routine stop 
	timeout = wait_event_interruptible_timeout(prxb->comp_inq,
			(prxb->buff_size == 0),
			msecs_to_jiffies(BBD_PACKET_TIMEOUT));

	if (timeout == 0 || prxb->buff_size < 0) {
		prxb->buff_size = -ETIMEDOUT;
		++prxb->tout_cnt;
		return -ETIMEDOUT;
	}
#endif
//	prxb->count += wr_size;
//	return wr_size;
	return 0;
}

/**
 * send data from user space to external driver
 */

ssize_t bbd_sensor_write(struct file        *filp,
			const char __user   *buf,
			size_t size, loff_t *ppos)
{
	int                       wr_size = MIN(BBD_MAX_DATA_SIZE, size);
	struct bbd_buffer        *prxb    = NULL;
        struct bbd_sensor_device* ps = bbd_sensor_ptr();
        struct bbd_base*          p  = bbd_sensor_ptr_base();

	if (!p || !ps)
		return -EINVAL;

	dprint("%s[%d]\n", __func__, (int) size);

	prxb = &p->rxb;

	mutex_lock(&prxb->lock);
	if (copy_from_user(prxb->pbuff, buf, wr_size)) {
		wr_size = -EFAULT;
        }
        else {
		prxb->buff_size = wr_size;
		bbd_sensor_write_handshake(ps, prxb); 
        }
	mutex_unlock(&prxb->lock);
	return wr_size;
}


/**
 * send data from user space to external driver
 */

ssize_t bbd_sensor_write_internal(const char *sensorData, size_t size)
{
	int                       wr_size;
	struct bbd_buffer        *prxb    = NULL;
        struct bbd_sensor_device* ps = bbd_sensor_ptr();
        struct bbd_base*          p  = bbd_sensor_ptr_base();

	if (!p || !ps)
		return -EINVAL;

	dprint("%s[%d]\n", __func__, (int) size);

	prxb = &p->rxb;

	mutex_lock(&prxb->lock);

	wr_size = MIN((BBD_MAX_DATA_SIZE - prxb->buff_size), size);
        memcpy(prxb->pbuff + prxb->buff_size, sensorData, wr_size);
        prxb->buff_size += wr_size;
	prxb->count += wr_size;
        bbd_sensor_write_handshake(ps, prxb); 

	mutex_unlock(&prxb->lock);

	return wr_size;
}

/**
 * this function is for tx packet
 */
unsigned int bbd_sensor_poll(struct file *filp, poll_table * wait)
{
        return bbd_poll(BBD_MINOR_SENSOR, filp, wait);
}
