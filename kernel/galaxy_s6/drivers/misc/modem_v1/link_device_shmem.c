/**
@file		link_device_shmem.c
@brief		functions for a real shared-memory on a system bus
@date		2014/02/05
@author		Hankook Jang (hankook.jang@samsung.com)
*/

/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/suspend.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <mach/shdmem.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

/**
@brief		read the interrupt value that AP received
*/
static u16 recv_cp2ap_irq(struct mem_link_device *mld)
{
	return (u16)mbox_get_value(mld->mbx_cp2ap_msg);
}

static void send_ap2cp_irq(struct mem_link_device *mld, u16 mask)
{
#if 0
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (!cp_online(mc))
		mif_err("%s: mask 0x%04X (%s.state == %s)\n", ld->name, mask,
			mc->name, mc_state(mc));
#endif
	mbox_set_value(mld->mbx_ap2cp_msg, mask);
	mbox_set_interrupt(mld->int_ap2cp_msg);
}

/**
@brief		read the interrupt value that has been sent to CP
*/
static inline u16 read_ap2cp_irq(struct mem_link_device *mld)
{
	return mbox_get_value(mld->mbx_ap2cp_msg);
}

static int shmem_ioctl(struct link_device *ld, struct io_device *iod,
		       unsigned int cmd, unsigned long arg)
{
	mif_err("%s: cmd 0x%08X\n", ld->name, cmd);

	switch (cmd) {
	case IOCTL_MODEM_GET_SHMEM_INFO:
	{
		struct shdmem_info mem_info;
		void __user *dst;
		unsigned long size;

		mif_err("%s: IOCTL_MODEM_GET_SHMEM_INFO\n", ld->name);

		mem_info.base = shm_get_phys_base();
		mem_info.size = shm_get_phys_size();
		dst = (void __user *)arg;
		size = sizeof(struct shdmem_info);

		if (copy_to_user(dst, &mem_info, size))
			return -EFAULT;

		break;
	}

	default:
		mif_err("%s: ERR! invalid cmd 0x%08X\n", ld->name, cmd);
		return -EINVAL;
	}

	return 0;
}

/**
@brief		shmem-specific interrupt handler for MCU_IPC interrupt

1) Gets a free snapshot slot.\n
2) Reads the RXQ status and saves the status to the free mst slot.\n
3) Invokes mem_irq_handler that is common to all memory-type interfaces.\n

@param data	the pointer to a mem_link_device instance
*/
static void shmem_irq_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct mst_buff *msb;

	msb = mem_take_snapshot(mld, RX);
	if (!msb)
		return;

	mem_irq_handler(mld, msb);
}

struct link_device *shmem_create_link_device(struct platform_device *pdev)
{
	struct modem_data *modem;
	struct mem_link_device *mld;
	struct link_device *ld;
	unsigned long start;
	unsigned long size;
	int err;
	mif_err("+++\n");

	/**
	 * Get the modem (platform) data
	 */
	modem = (struct modem_data *)pdev->dev.platform_data;
	if (!modem) {
		mif_err("ERR! modem == NULL\n");
		return NULL;
	}

	if (!modem->mbx) {
		mif_err("%s: ERR! mbx == NULL\n", modem->link_name);
		return NULL;
	}

	mif_err("MODEM:%s LINK:%s\n", modem->name, modem->link_name);

	/**
	 * Create a MEMORY link device instance
	 */
	mld = mem_create_link_device(MEM_SYS_SHMEM, modem);
	if (!mld) {
		mif_err("%s: ERR! create_link_device fail\n", modem->link_name);
		return NULL;
	}

	ld = &mld->link_dev;

	/**
	 * Link local functions to the corresponding function pointers that are
	 * optional for some link device
	 */
	ld->ioctl = shmem_ioctl;

	/**
	 * Link local functions to the corresponding function pointers that are
	 * mandatory for all memory-type link devices
	 */
	mld->recv_cp2ap_irq = recv_cp2ap_irq;
	mld->send_ap2cp_irq = send_ap2cp_irq;

	/**
	 * Link local functions to the corresponding function pointers that are
	 * optional for some memory-type link devices
	 */
	mld->read_ap2cp_irq = read_ap2cp_irq;
	mld->unmap_region = shm_release_region;

	/**
	 * Initialize SHMEM maps for BOOT (physical map -> logical map)
	 */
	start = shm_get_phys_base() + shm_get_boot_rgn_offset();
	size = shm_get_boot_rgn_size();
	err = mem_register_boot_rgn(mld, start, size);
	if (err < 0) {
		mif_err("%s: ERR! register_boot_rgn fail (%d)\n",
			ld->name, err);
		goto error;
	}
	err = mem_setup_boot_map(mld);
	if (err < 0) {
		mif_err("%s: ERR! setup_boot_map fail (%d)\n", ld->name, err);
		mem_unregister_boot_rgn(mld);
		goto error;
	}

	/**
	 * Initialize SHMEM maps for IPC (physical map -> logical map)
	 */
	start = shm_get_phys_base() + shm_get_ipc_rgn_offset();
	size = shm_get_ipc_rgn_size();
	err = mem_register_ipc_rgn(mld, start, size);
	if (err < 0) {
		mif_err("%s: ERR! register_ipc_rgn fail (%d)\n", ld->name, err);
		mem_unregister_boot_rgn(mld);
		goto error;
	}
	err = mem_setup_ipc_map(mld);
	if (err < 0) {
		mif_err("%s: ERR! setup_ipc_map fail (%d)\n", ld->name, err);
		mem_unregister_boot_rgn(mld);
		mem_unregister_ipc_rgn(mld);
		goto error;
	}

	/**
	 * Retrieve SHMEM MBOX#, IRQ#, etc.
	 */
	mld->mbx_cp2ap_msg = modem->mbx->mbx_cp2ap_msg;
	mld->irq_cp2ap_msg = modem->mbx->irq_cp2ap_msg;

	mld->mbx_ap2cp_msg = modem->mbx->mbx_ap2cp_msg;
	mld->int_ap2cp_msg = modem->mbx->int_ap2cp_msg;

	/**
	 * Register interrupt handlers
	 */
	err = mbox_request_irq(mld->irq_cp2ap_msg, shmem_irq_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq fail (%d)\n", ld->name, err);
		goto error;
	}

	mif_err("---\n");
	return ld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}

