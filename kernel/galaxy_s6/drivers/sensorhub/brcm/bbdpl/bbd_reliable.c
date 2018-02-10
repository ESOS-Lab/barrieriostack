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

/**
 * initialize reliable-specific variables
 *
 * @pbbd_dev: bbd driver pointer
 *
 * @return: 0 = success, others = failure
 */

void bbd_reliable_init_vars(struct bbd_device *dev, int* result)
{
        struct bbd_reliable_device *p = 0;

        if (*result)
                return;

	p = bbd_alloc(sizeof(struct bbd_reliable_device));
        if (!p) {
		*result = -ENOMEM;
		return;
        }

        bbd_base_init_vars(dev, &p->base, result);
        if (*result) {
                bbd_free(p);
                return;
        }
        dev->bbd_ptr[BBD_MINOR_RELIABLE] = p;
        p->base.name = "reliable";
}

struct bbd_reliable_device* bbd_reliable_ptr(void)
{
	if (!gpbbd_dev)
		return 0;
	return gpbbd_dev->bbd_ptr[BBD_MINOR_RELIABLE];
}

struct bbd_base* bbd_reliable_ptr_base(void)
{
        struct bbd_reliable_device *ps = bbd_reliable_ptr();
	if (!ps)
		return 0;
	return &ps->base;
}

int bbd_reliable_uninit(void)
{
        struct bbd_reliable_device* p = bbd_reliable_ptr();
        int freed = 0;
        if (p) {
                freed = bbd_base_uninit(&p->base);
                bbd_free(p);
                gpbbd_dev->bbd_ptr[BBD_MINOR_RELIABLE] = 0;
        }
        return freed;
}

int bbd_tty_open (void);
int bbd_tty_close(void);
int bbd_ssi_spi_close(void);

/**
 * open bbd driver
 */
int bbd_reliable_open(struct inode *inode, struct file *filp)
{
	dprint(KERN_INFO "BBD:%s()\n", __func__);

	if (!gpbbd_dev)
		return -EFAULT;

	filp->private_data = gpbbd_dev;

        if (gpbbd_dev->sio_type == BBD_SERIAL_TTY)
                bbd_tty_open();

	return 0;
}

/**
 * close bbd driver
 */
int bbd_reliable_release(struct inode *inode, struct file *filp)
{
	if (!gpbbd_dev)
		return -EFAULT;

        bbd_ssi_spi_close();
        bbd_tty_close();

        return bbd_base_reinit(BBD_MINOR_RELIABLE);
}

/**
 * read signal/data from bbd_list_head to user
 */
ssize_t bbd_reliable_read(struct file *filp,
			char __user *buf,
			size_t size, loff_t *ppos)
{
        return bbd_read(BBD_MINOR_RELIABLE, buf, size, true);
}

/**
 * send data from user space to external driver
 */
ssize_t bbd_reliable_write(struct file *filp,
			const char __user *buf,
			size_t size, loff_t *ppos)
{
	ssize_t len = -EINVAL;
        if (size != sizeof(struct sBbdReliableTransaction))
                return len;

	len = bbd_write(BBD_MINOR_RELIABLE, buf, size, true);
	if (len < 0)
		return len;

        /* Send reliable data to the BbdBridge --> PacketLayer
         * --> uart --> ESW */
	if (BbdBridge_SendReliablePacket(&bbd_engine()->bridge,
            (struct sBbdReliableTransaction *) bbd_reliable_ptr_base()->rxb.pbuff))
            return len;
        return -EINVAL;
}

/*
 *
 */
unsigned int bbd_reliable_poll(struct file *filp, poll_table * wait)
{
	return bbd_poll(BBD_MINOR_RELIABLE, filp, wait);
}
