/**
@file		link_device_c2c.c
@brief		functions for a pseudo shared-memory based on a chip-to-chip
		(C2C) interface
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

#include <mach/c2c.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

static void print_pm_status(struct mem_link_device *mld)
{
#ifdef DEBUG_MODEM_IF
	struct link_device *ld = &mld->link_dev;
	unsigned int magic;
	int ap_wakeup;
	int ap_status;
	int cp_wakeup;
	int cp_status;

	magic = get_magic(mld);
	ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
	ap_status = gpio_get_value(mld->gpio_ap_status);
	cp_wakeup = gpio_get_value(mld->gpio_cp_wakeup);
	cp_status = gpio_get_value(mld->gpio_cp_status);

	/*
	** PM {ap_wakeup:cp_wakeup:cp_status:ap_status:magic} <CALLER>
	*/
	mif_info("%s: PM {%d:%d:%d:%d:%X} <%pf>\n", ld->name,
		ap_wakeup, cp_wakeup, cp_status, ap_status, magic, CALLER);
#endif
}

static void send_int2cp(struct mem_link_device *mld, u16 mask)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (!cp_online(mc))
		mif_err("%s: mask 0x%04X (%s.state == %s)\n", ld->name, mask,
			mc->name, mc_state(mc));

	c2c_send_interrupt(mask);
}

/**
@brief		finalize handling the PHONE_START command from CP

@param mld	the pointer to a mem_link_device instance
*/
static void finalize_cp_start(struct mem_link_device *mld)
{
	if (mld->gpio_ap_wakeup) {
		int ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);

		s5p_change_irq_type(mld->irq_ap_wakeup, ap_wakeup);

		if (ap_wakeup) {
			if (wake_lock_active(&mld->ap_wlock))
				wake_lock(&mld->ap_wlock);
		} else {
			if (wake_lock_active(&mld->ap_wlock))
				wake_unlock(&mld->ap_wlock);
		}
	}

	if (mld->gpio_cp_status) {
		int cp_status = gpio_get_value(mld->gpio_cp_status);

		s5p_change_irq_type(mld->irq_cp_status, cp_status);

		if (cp_status) {
			if (!wake_lock_active(&mld->ap_wlock))
				wake_lock(&mld->cp_wlock);
		} else {
			if (wake_lock_active(&mld->ap_wlock))
				wake_unlock(&mld->cp_wlock);
		}
	}
}

static inline int check_link_status(struct mem_link_device *mld)
{
	unsigned int magic = get_magic(mld);
	int cnt;

	if (gpio_get_value(mld->gpio_cp_status) != 0 && magic == MEM_IPC_MAGIC)
		return 0;

	cnt = 0;
	while (gpio_get_value(mld->gpio_cp_status) == 0) {
		if (gpio_get_value(mld->gpio_ap_status) == 0) {
			print_pm_status(mld);
			gpio_set_value(mld->gpio_ap_status, 1);
		}

		cnt++;
		if (cnt >= 100) {
			mif_err("ERR! cp_status != 1 (cnt %d)\n", cnt);
			return -EACCES;
		}

		if (in_interrupt())
			udelay(100);
		else
			usleep_range(100, 200);
	}

	cnt = 0;
	while (1) {
		magic = get_magic(mld);
		if (magic == MEM_IPC_MAGIC)
			break;

		cnt++;
		if (cnt >= 100) {
			mif_err("ERR! magic 0x%X != IPC_MAGIC (cnt %d)\n",
				magic, cnt);
			return -EACCES;
		}

		if (in_interrupt())
			udelay(100);
		else
			usleep_range(100, 200);
	}

	return 0;
}

static void release_cp_wakeup(struct work_struct *ws)
{
	struct mem_link_device *mld;
	struct link_device *ld;
	int i;
	unsigned long flags;

	mld = container_of(ws, struct mem_link_device, cp_sleep_dwork.work);

	if (work_pending(&mld->cp_sleep_dwork.work))
		cancel_delayed_work(&mld->cp_sleep_dwork);

	spin_lock_irqsave(&mld->pm_lock, flags);
	i = atomic_read(&mld->ref_cnt);
	spin_unlock_irqrestore(&mld->pm_lock, flags);
	if (i > 0)
		goto reschedule;

	/*
	 * If there is any IPC message remained in a TXQ, AP must prevent CP
	 * from going to sleep.
	 */
	ld = &mld->link_dev;
	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		if (ld->skb_txq[i]->qlen > 0)
			goto reschedule;
	}

	if (gpio_get_value(mld->gpio_ap_wakeup))
		goto reschedule;

	gpio_set_value(mld->gpio_cp_wakeup, 0);
	print_pm_status(mld);
	return;

reschedule:
	queue_delayed_work(system_nrt_wq, &mld->cp_sleep_dwork,
			   msecs_to_jiffies(CP_WAKEUP_HOLD_TIME));
}

/**
@brief		forbid CP from going to sleep

Wakes up a CP if it can sleep and increases the "ref_cnt" counter in the
mem_link_device instance.

@param mld	the pointer to a mem_link_device instance

@remark		CAUTION!!! permit_cp_sleep() MUST be invoked after
		forbid_cp_sleep() success to decrease the "ref_cnt" counter.
*/
static void forbid_cp_sleep(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	int ap_status = gpio_get_value(mld->gpio_ap_status);
	int cp_wakeup = gpio_get_value(mld->gpio_cp_wakeup);
	unsigned long flags;

	spin_lock_irqsave(&mld->pm_lock, flags);

	atomic_inc(&mld->ref_cnt);

	gpio_set_value(mld->gpio_ap_status, 1);
	gpio_set_value(mld->gpio_cp_wakeup, 1);

	if (work_pending(&mld->cp_sleep_dwork.work))
		cancel_delayed_work(&mld->cp_sleep_dwork);

	spin_unlock_irqrestore(&mld->pm_lock, flags);

	if (!ap_status || !cp_wakeup)
		print_pm_status(mld);

	if (check_link_status(mld) < 0) {
		print_pm_status(mld);
		mif_err("%s: ERR! check_link_status fail\n", ld->name);
		mem_forced_cp_crash(mld);
	}
}

/**
@brief	permit CP to go sleep

Decreases the "ref_cnt" counter in the mem_link_device instance if it can go
sleep and allows CP to go sleep only if the value of "ref_cnt" counter is less
than or equal to 0.

@param mld	the pointer to a mem_link_device instance

@remark		MUST be invoked after forbid_cp_sleep() success to decrease the
		"ref_cnt" counter.
*/
static void permit_cp_sleep(struct mem_link_device *mld)
{
	unsigned long flags;

	spin_lock_irqsave(&mld->pm_lock, flags);
	if (atomic_dec_return(&mld->ref_cnt) > 0) {
		spin_unlock_irqrestore(&mld->pm_lock, flags);
		return;
	}
	atomic_set(&mld->ref_cnt, 0);
	spin_unlock_irqrestore(&mld->pm_lock, flags);

	/* Hold gpio_cp_wakeup for CP_WAKEUP_HOLD_TIME until CP finishes RX */
	if (!work_pending(&mld->cp_sleep_dwork.work)) {
		queue_delayed_work(system_nrt_wq, &mld->cp_sleep_dwork,
				   msecs_to_jiffies(CP_WAKEUP_HOLD_TIME));
	}
}

static bool link_active(struct mem_link_device *mld)
{
	unsigned int magic = get_magic(mld);

	if (magic == MEM_PM_MAGIC) {
		mif_err("%s: Going to SLEEP\n", ld->name);
		return true;
	} else {
		mif_err("%s: magic 0x%X\n", ld->name, magic);
		return false;
	}
}

/**
@brief	interrupt handler for a wakeup interrupt

1) Reads the interrupt value\n
2) Performs interrupt handling\n

@param irq	the IRQ number
@param data	the pointer to a data
*/
static irqreturn_t ap_wakeup_handler(int irq, void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct link_device *ld = &mld->link_dev;
	int ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
	int ap_status = gpio_get_value(mld->gpio_ap_status);

	s5p_change_irq_type(irq, ap_wakeup);

	if (!cp_online(ld->mc))
		goto exit;

	if (work_pending(&mld->cp_sleep_dwork.work))
		__cancel_delayed_work(&mld->cp_sleep_dwork);

	print_pm_status(mld);

	if (ap_wakeup) {
		if (!wake_lock_active(&mld->ap_wlock))
			wake_lock(&mld->ap_wlock);

		if (!c2c_suspended() && !ap_status)
			gpio_set_value(mld->gpio_ap_status, 1);
	} else {
		if (wake_lock_active(&mld->ap_wlock))
			wake_unlock(&mld->ap_wlock);

		queue_delayed_work(system_nrt_wq, &mld->cp_sleep_dwork,
				msecs_to_jiffies(CP_WAKEUP_HOLD_TIME));
	}

exit:
	return IRQ_HANDLED;
}

static irqreturn_t cp_status_handler(int irq, void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct link_device *ld = &mld->link_dev;
	int cp_status = gpio_get_value(mld->gpio_cp_status);
	unsigned long flags;

	spin_lock_irqsave(&mld->pm_lock, flags);

	s5p_change_irq_type(irq, cp_status);

	if (!cp_online(ld->mc))
		goto exit;

	print_pm_status(mld);

	if (cp_status) {
		if (!wake_lock_active(&mld->cp_wlock))
			wake_lock(&mld->cp_wlock);
	} else {
		if (atomic_read(&mld->ref_cnt) > 0) {
			/*
			** This status means that IPC TX is in progress from AP
			** to CP. So, CP_WAKEUP must be set to 1. Otherwise, it
			** is a critically erroneous status.
			*/
			if (gpio_get_value(mld->gpio_cp_wakeup) == 0) {
				mif_err("%s: ERR! cp_wakeup == 0\n", ld->name);
				goto exit;
			}
			/* CP_STATUS will be reset to 1 soon due to CP_WAKEUP.*/
		} else {
			gpio_set_value(mld->gpio_ap_status, 0);

			if (wake_lock_active(&mld->cp_wlock))
				wake_unlock(&mld->cp_wlock);
		}
	}

exit:
	spin_unlock_irqrestore(&mld->pm_lock, flags);
	return IRQ_HANDLED;
}

#if 1
#endif

/**
@brief	interrupt handler for a C2C IPC interrupt

1) Get a free mst buffer\n
2) Reads the RXQ status and saves the status to the mst buffer\n
3) Saves the interrupt value to the mst buffer\n
4) Invokes mem_irq_handler that is common to all memory-type interfaces\n

@param data	the pointer to a mem_link_device instance
@param intr	the interrupt value
*/
static void c2c_irq_handler(void *data, u32 intr)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct mst_buff *msb;

	/* Prohibit CP from going to sleep */
	if (gpio_get_value(mld->gpio_cp_wakeup) == 0) {
		gpio_set_value(mld->gpio_cp_wakeup, 1);
		print_pm_status(mld);
	}

	msb = mem_take_snapshot(mld, RX);
	if (!msb)
		return;

	msb->snapshot.int2ap = (u16)intr;
	mem_irq_handler(mld, msb);
}

struct link_device *c2c_create_link_device(struct platform_device *pdev)
{
	struct modem_data *modem;
	struct mem_link_device *mld;
	struct link_device *ld;
	int err;
	int i;
	unsigned int irq;
	unsigned long flags;
	char name[MAX_NAME_LEN];
	mif_err("+++\n");

	/**
	 * Get the modem (platform) data
	 */
	modem = (struct modem_data *)pdev->dev.platform_data;
	if (!modem) {
		mif_err("ERR! modem == NULL\n");
		return NULL;
	}

	if (modem->irq_ap_wakeup == 0) {
		mif_err("ERR! no irq_ap_wakeup\n");
		return NULL;
	}

	if (modem->irq_cp_status == 0) {
		mif_err("ERR! no irq_cp_status\n");
		return NULL;
	}

	mif_err("MODEM:%s LINK:%s\n", modem->name, modem->link_name);

	/**
	 * Create a MEMORY link device instance
	 */
	mld = mem_create_link_device(MEM_C2C_SHMEM, modem);
	if (!mld) {
		mif_err("%s: ERR! create_link_device fail\n", modem->link_name);
		return NULL;
	}

	ld = &mld->link_dev;

	/*
	** Link local functions to the corresponding function pointers
	*/
	mld->send_ap2cp_irq = send_int2cp;

	mld->finalize_cp_start = finalize_cp_start;

	mld->forbid_cp_sleep = forbid_cp_sleep;
	mld->permit_cp_sleep = permit_cp_sleep;
	mld->link_active = link_active;

	/*
	** Retrieve SHMEM resource
	*/
	mld->start = c2c_get_phys_base() + c2c_get_sh_rgn_offset();
	mld->size = c2c_get_sh_rgn_size();
	mld->base = mem_register_ipc_rgn(mld, mld->start, mld->size);
	if (!mld->base) {
		mif_err("%s: ERR! register_ipc_rgn fail\n", ld->name);
		goto error;
	}

	/*
	** Initialize SHMEM maps (physical map -> logical map)
	*/
	err = mem_setup_ipc_map(mld);
	if (err < 0) {
		mif_err("%s: ERR! init_ipc_map fail (err %d)\n", ld->name, err);
		goto error;
	}

	/*
	** Register interrupt handlers
	*/
	err = c2c_register_handler(c2c_irq_handler, mld);
	if (err) {
		mif_err("%s: ERR! c2c_register_handler fail (err %d)\n",
			ld->name, err);
		goto error;
	}

	/*
	** Retrieve GPIO#, IRQ#, and IRQ flags for PM
	*/
	mld->gpio_ap_wakeup = modem->gpio_ap_wakeup;
	mld->irq_ap_wakeup = modem->irq_ap_wakeup;

	mld->gpio_cp_wakeup = modem->gpio_cp_wakeup;

	mld->gpio_cp_status = modem->gpio_cp_status;
	mld->irq_cp_status = modem->irq_cp_status;

	mld->gpio_ap_status = modem->gpio_ap_status;

	snprintf(name, MAX_NAME_LEN, "%s_ap_wakeup", ld->name);
	irq = mld->irq_ap_wakeup;
	flags = (IRQF_NO_THREAD | IRQF_NO_SUSPEND | IRQF_TRIGGER_HIGH);
	err = mif_register_isr(irq, ap_wakeup_handler, flags, name, mld);
	if (err)
		goto error;

	snprintf(name, MAX_NAME_LEN, "%s_cp_status", ld->name);
	irq = mld->irq_cp_status;
	flags = (IRQF_NO_THREAD | IRQF_NO_SUSPEND | IRQF_TRIGGER_HIGH);
	err = mif_register_isr(irq, cp_status_handler, flags, name, mld);
	if (err)
		goto error;

	mif_err("CP2AP_WAKEUP GPIO# = %d\n", mld->gpio_ap_wakeup);
	mif_err("CP2AP_WAKEUP IRQ# = %d\n", mld->irq_ap_wakeup);

	mif_err("AP2CP_WAKEUP GPIO# = %d\n", mld->gpio_cp_wakeup);

	mif_err("CP2AP_STATUS GPIO# = %d\n", mld->gpio_cp_status);
	mif_err("CP2AP_STATUS IRQ# = %d\n", mld->irq_cp_status);

	mif_err("AP2CP_STATUS GPIO# = %d\n", mld->gpio_ap_status);

	/*
	** Initialize locks, completions, bottom halves, etc. for PM
	*/
	sprintf(name, "%s_ap_wlock", ld->name);
	wake_lock_init(&mld->ap_wlock, WAKE_LOCK_SUSPEND, name);

	sprintf(name, "%s_cp_wlock", ld->name);
	wake_lock_init(&mld->cp_wlock, WAKE_LOCK_SUSPEND, name);

	INIT_DELAYED_WORK(&mld->cp_sleep_dwork, release_cp_wakeup);

	spin_lock_init(&mld->pm_lock);
	atomic_set(&mld->ref_cnt, 0);

	gpio_set_value(mld->gpio_ap_status, 0);

	c2c_assign_gpio_ap_wakeup(mld->gpio_ap_wakeup);
	c2c_assign_gpio_ap_status(mld->gpio_ap_status);
	c2c_assign_gpio_cp_wakeup(mld->gpio_cp_wakeup);
	c2c_assign_gpio_cp_status(mld->gpio_cp_status);

	mif_err("---\n");
	return ld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}

