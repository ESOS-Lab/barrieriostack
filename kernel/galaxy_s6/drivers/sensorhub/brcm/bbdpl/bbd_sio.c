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
#include "transport/bbd_engine.h"
#include "transport/bbd_bridge_c.h"

/**
 * initialize sio-specific variables
 *
 * @pbbd_dev: bbd driver pointer
 *
 * @return: 0 = success, others = failure
 */

void bbd_sio_init_vars(struct bbd_device *dev, int* result)
{
        struct bbd_sio_device *p = 0;
        if (*result)
                return;

	p = bbd_alloc(sizeof(struct bbd_sio_device));
        if (!p) {
		*result = -ENOMEM;
		return;
        }

        bbd_base_init_vars(dev, &p->base, result);
        if (*result) {
                bbd_free(p);
                return;
        }
        dev->bbd_ptr[BBD_MINOR_SIO] = p;
        p->base.name = "sio";
}

struct bbd_sio_device* bbd_sio_ptr(void)
{
	if (!gpbbd_dev)
		return 0;
	return gpbbd_dev->bbd_ptr[BBD_MINOR_SIO];
}

struct bbd_base* bbd_sio_ptr_base(void)
{
        struct bbd_sio_device *p = bbd_sio_ptr();
	if (!p)
		return 0;
	return &p->base;
}

int bbd_sio_uninit(void)
{
        struct bbd_sio_device* p = bbd_sio_ptr();
        int freed = 0;
        if (p) {
                freed = bbd_base_uninit(&p->base);
                bbd_free(p);
                gpbbd_dev->bbd_ptr[BBD_MINOR_SIO] = 0;
        }
        return freed;
}

/*      We are passed a pointer to the bridge
 *      if we're active, zero otherwise
 *      Could consider a pass-thru flag
 *      from /dev/bbd_control for testing.
 */
void bbd_sio_install(struct BbdBridge* bbd)
{
    FUNC();
}

/**
 * open bbd driver
 */
int bbd_sio_open(struct inode *inode, struct file *filp)
{
	dprint(KERN_INFO "BBD:%s()\n", __func__);

	if (!gpbbd_dev)
		return -EFAULT;

	filp->private_data = gpbbd_dev;

	return 0;
}

/**
 * close bbd driver
 */
int bbd_sio_release(struct inode *inode, struct file *filp)
{
	if (!gpbbd_dev)
		return -EFAULT;

        return bbd_base_reinit(BBD_MINOR_SIO);
}

/**
 * read signal/data from bbd_list_head to user
 */
ssize_t bbd_sio_read(struct file *filp,
			char __user *buf,
			size_t size, loff_t *ppos)
{
        return bbd_read(BBD_MINOR_SIO, buf, size, true);
}

/**
 * send data from user space to external driver
 */
ssize_t bbd_sio_write(struct file *filp,
			const char __user *buf,
			size_t size, loff_t *ppos)
{
        ssize_t len = bbd_write(BBD_MINOR_SIO, buf, size, true);
        if (len != size)
            return len;

        // if (gpbbd_dev->sio_dummy)            // assume this until the sub-driver is 
        //                                      // ready.
        {
            struct BbdEngine* pEng = bbd_engine();
            struct BbdBridge* bbd =  0;
            if (pEng) {
                bbd = &pEng->bridge;
                BbdBridge_SetData(bbd, buf, size);
            }
            else 
                len = -EINVAL;
        }
        // else
        // {
        //      tickle the sub-device driver to send data?
        // }
        return len;
}

/**
 * this function is for tx sio
 */
unsigned int bbd_sio_poll(struct file *filp, poll_table * wait)
{
        return bbd_poll(BBD_MINOR_SIO, filp, wait);
}
