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

void BbdEngine_Open(struct BbdEngine *p);
void BbdEngine_Close(struct BbdEngine *p);

/**
 * initialize packet-specific variables
 *
 * @pbbd_dev: bbd driver pointer
 *
 * @return: 0 = success, others = failure
 */

void bbd_packet_init_vars(struct bbd_device *dev, int* result)
{
        struct bbd_packet_device *p = 0;

        if (*result)
                return;

	p = bbd_alloc(sizeof(struct bbd_packet_device));
        if (!p) {
		*result = -ENOMEM;
		return;
        }

        bbd_base_init_vars(dev, &p->base, result);
        if (*result) {
                bbd_free(p);
                return;
        }
        dev->bbd_ptr[BBD_MINOR_PACKET] = p;
        p->base.name = "packet";
}

struct bbd_packet_device* bbd_packet_ptr(void)
{
	if (!gpbbd_dev)
		return 0;
	return gpbbd_dev->bbd_ptr[BBD_MINOR_PACKET];
}

struct bbd_base* bbd_packet_ptr_base(void)
{
        struct bbd_packet_device *p = bbd_packet_ptr();
	if (!p)
		return 0;
	return &p->base;
}

int bbd_packet_uninit(void)
{
        struct bbd_packet_device* p = bbd_packet_ptr();
        int freed = 0;
        if (p) {
                freed = bbd_base_uninit(&p->base);
                bbd_free(p);
                gpbbd_dev->bbd_ptr[BBD_MINOR_PACKET] = 0;
        }
        return freed;
}

/**
 * open bbd driver
 */
int bbd_packet_open(struct inode *inode, struct file *filp)
{
	dprint("BBD:%s()\n", __func__);

	if (!gpbbd_dev)
		return -EFAULT;

	filp->private_data = gpbbd_dev;
        BbdEngine_Open(&gpbbd_dev->bbd_engine);

	return 0;
}

/**
 * close bbd driver
 */
int bbd_packet_release(struct inode *inode, struct file *filp)
{
	if (!gpbbd_dev)
		return -EFAULT;

	dprint("BBD:%s()\n", __func__);
        BbdEngine_Close(&gpbbd_dev->bbd_engine);

        return bbd_base_reinit(BBD_MINOR_PACKET);
}

/**
 * read signal/data from bbd_list_head to user
 */
ssize_t bbd_packet_read(struct file *filp,
			char __user *buf,
			size_t size, loff_t *ppos)
{
        return bbd_read(BBD_MINOR_PACKET, buf, size, true);
}

/*
 * send a packet from user space to external driver
 */
ssize_t bbd_packet_write(struct file *filp,
			const char __user *rpcStream,
			size_t size, loff_t *ppos)
{
	ssize_t len = bbd_write(BBD_MINOR_PACKET, rpcStream, size, true);
	if (len < 0)
		return len;
	BbdBridge_SendPacket(&bbd_engine()->bridge,
		bbd_packet_ptr_base()->rxb.pbuff, len);
	return len;
}

/**
 * this function is for tx packet
 */
unsigned int bbd_packet_poll(struct file *filp, poll_table * wait)
{
    return bbd_poll(BBD_MINOR_PACKET, filp, wait);
}
