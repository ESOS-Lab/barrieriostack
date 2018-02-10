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

static unsigned char bbd_patch[] =
{
#include "bbd_patch_file.h"
};

#ifdef SUPPORT_MCU_HOST_WAKE
static unsigned char bbd_new_patch[] =
{
#include "bbd_new_patch_file.h"
};
#endif

void bbd_patch_init_vars(struct bbd_device *dev, int* result)
{
#ifdef SUPPORT_MCU_HOST_WAKE
	if(gpbbd_dev->hw_rev >= BBD_NEW_HW_REV)
		printk(KERN_INFO "BBD:%s() %u\n", __func__, (u32)sizeof(bbd_new_patch));
	else
#endif
		printk(KERN_INFO "BBD:%s() %u\n", __func__, (u32)sizeof(bbd_patch));

        dev->bbd_ptr[BBD_MINOR_PATCH] = 0;
}

int bbd_patch_uninit(void)
{
        return 0;
}

int bbd_patch_open(struct inode *inode, struct file *filp)
{
#ifdef SUPPORT_MCU_HOST_WAKE
	if(gpbbd_dev->hw_rev >= BBD_NEW_HW_REV)
		printk(KERN_INFO "BBD:%s() %u\n", __func__, (u32)sizeof(bbd_new_patch));
	else
#endif
		printk(KERN_INFO "BBD:%s() %u\n", __func__, (u32)sizeof(bbd_patch));

	if (!gpbbd_dev)
		return -EFAULT;

	filp->private_data = gpbbd_dev;

	return 0;
}

int bbd_patch_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t bbd_patch_read( struct file *filp,
			char __user *buf,
			size_t size, loff_t *ppos)
{
        ssize_t rd_size = size;
        size_t  offset = filp->f_pos;

#ifdef SUPPORT_MCU_HOST_WAKE
	if(gpbbd_dev->hw_rev >= BBD_NEW_HW_REV) {
	        if (offset >= sizeof(bbd_new_patch)) {       /* signal EOF */
	                *ppos = 0;
	                return 0;
	        }
	        if (offset+size > sizeof(bbd_new_patch))
	                rd_size = sizeof(bbd_new_patch) - offset;
	        if (copy_to_user(buf, bbd_new_patch + offset, rd_size))
	                rd_size = -EFAULT;
	        else
	                *ppos = filp->f_pos + rd_size;
	}
	else {
#endif
	        if (offset >= sizeof(bbd_patch)) {       /* signal EOF */
	                *ppos = 0;
	                return 0;
	        }
	        if (offset+size > sizeof(bbd_patch))
	                rd_size = sizeof(bbd_patch) - offset;
	        if (copy_to_user(buf, bbd_patch + offset, rd_size))
	                rd_size = -EFAULT;
	        else
	                *ppos = filp->f_pos + rd_size;
#ifdef SUPPORT_MCU_HOST_WAKE
	}
#endif

        return rd_size;
}
