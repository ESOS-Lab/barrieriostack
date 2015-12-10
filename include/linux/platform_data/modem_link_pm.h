/*
 * Copyright (C) 2012 Samsung Electronics.
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

#ifndef __MODEM_LINK_PM_H__
#define __MODEM_LINK_PM_H__

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/wakelock.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include "modem_v1.h"

#ifdef CONFIG_OF
#define PM_DT_NODE_NAME		"link_pm"
#endif

enum pm_state {
	PM_STATE_UNMOUNTED,
	PM_STATE_SUSPENDED,
	PM_STATE_WAKE_BY_AP,
	PM_STATE_MOUNTING,
	PM_STATE_ACTIVE,
	PM_STATE_AP_FREE,
	PM_STATE_CP_FREE,
	PM_STATE_ACTIVATING,
	PM_STATE_UNMOUNTING,
	PM_STATE_CP_BOOTING,
	PM_STATE_LOCKED_ON,
	PM_STATE_CP_CRASH
};

static const char const *pm_state_string[] = {
	[PM_STATE_UNMOUNTED] = "UNMOUNTED",
	[PM_STATE_CP_BOOTING] = "CP_BOOTING",
	[PM_STATE_SUSPENDED] = "SUSPENDED",
	[PM_STATE_WAKE_BY_AP] = "WAKE_BY_AP",
	[PM_STATE_MOUNTING] = "MOUNTING",
	[PM_STATE_ACTIVE] = "ACTIVE",
	[PM_STATE_AP_FREE] = "AP_FREE",
	[PM_STATE_CP_FREE] = "CP_FREE",
	[PM_STATE_UNMOUNTING] = "UNMOUNTING",
	[PM_STATE_ACTIVATING] = "ACTIVATING",
	[PM_STATE_LOCKED_ON] = "LOCKED_ON",
	[PM_STATE_CP_CRASH] = "CP_CRASH"
};

static const inline char *pm_state2str(enum pm_state state)
{
	if (unlikely(state > PM_STATE_CP_CRASH))
		return "INVALID";
	else
		return pm_state_string[state];
}

enum pm_event {
	PM_EVENT_NO_EVENT,
	PM_EVENT_LOCK_ON,
	PM_EVENT_CP_BOOTING,
	PM_EVENT_CP2AP_WAKEUP_LOW,
	PM_EVENT_CP2AP_WAKEUP_HIGH,
	PM_EVENT_CP2AP_STATUS_LOW,
	PM_EVENT_CP2AP_STATUS_HIGH,
	PM_EVENT_CP_HOLD_REQUEST,
	PM_EVENT_CP_HOLD_TIMEOUT,
	PM_EVENT_LINK_UNMOUNTED,
	PM_EVENT_LINK_SUSPENDED,
	PM_EVENT_LINK_RESUMED,
	PM_EVENT_LINK_MOUNTED,
	PM_EVENT_WDOG_TIMEOUT
};

static const char const *pm_event_string[] = {
	[PM_EVENT_NO_EVENT] = "NO_EVENT",
	[PM_EVENT_LOCK_ON] = "LOCK_ON",
	[PM_EVENT_CP_BOOTING] = "CP_BOOTING",
	[PM_EVENT_CP2AP_WAKEUP_LOW] = "CP2AP_WAKEUP_LOW",
	[PM_EVENT_CP2AP_WAKEUP_HIGH] = "CP2AP_WAKEUP_HIGH",
	[PM_EVENT_CP2AP_STATUS_LOW] = "CP2AP_STATUS_LOW",
	[PM_EVENT_CP2AP_STATUS_HIGH] = "CP2AP_STATUS_HIGH",
	[PM_EVENT_CP_HOLD_REQUEST] = "CP_HOLD_REQUEST",
	[PM_EVENT_CP_HOLD_TIMEOUT] = "CP_HOLD_TIMEOUT",
	[PM_EVENT_LINK_SUSPENDED] = "LINK_SUSPENDED",
	[PM_EVENT_LINK_UNMOUNTED] = "LINK_UNMOUNTED",
	[PM_EVENT_LINK_RESUMED] = "LINK_RESUMED",
	[PM_EVENT_LINK_MOUNTED] = "LINK_MOUNTED",
	[PM_EVENT_WDOG_TIMEOUT] = "WDOG_TIMEOUT"
};

static const inline char *pm_event2str(enum pm_event event)
{
	if (unlikely(event > PM_EVENT_WDOG_TIMEOUT))
		return "INVALID";
	else
		return pm_event_string[event];
}

struct pm_fsm {
	enum pm_state prev_state;
	enum pm_event event;
	enum pm_state state;
};

#define CP_HOLD_TIME		100	/* ms */
#define PM_WDOG_TIMEOUT		msecs_to_jiffies(100)
#define PM_MOUNT_TIMEOUT	msecs_to_jiffies(500)

struct phys_link_pm_ops {
	void (*hold_link)(void);
	void (*free_link)(void);
};

struct link_pm {
	/*
	Private variables must be set first of all while the PM framework is set
	up by the corresponding link device driver in the modem interface
	*/
	char *link_name;
	void (*fail_handler)(struct link_pm *pm);
	void (*attach_to_phys)(struct link_pm *pm);

	/*
	GPIO pins for PM
	*/
	unsigned int gpio_cp2ap_wakeup;
	unsigned int gpio_ap2cp_wakeup;
	unsigned int gpio_cp2ap_status;
	unsigned int gpio_ap2cp_status;

	/*
	IRQ numbers for PM
	*/
	unsigned int irq_cp2ap_wakeup;
	unsigned int irq_cp2ap_status;

	/*
	Common variables for PM
	*/
	spinlock_t lock;

	struct pm_fsm fsm;

	struct wake_lock wlock;

	struct workqueue_struct *wq;
	struct delayed_work cp_free_dwork;	/* to hold ap2cp_wakeup */
	bool hold_requested;

	struct timer_list wdog_timer;

	/*
	PM functions set by the common link PM framework and used by each link
	device driver
	*/
	void (*start)(struct link_pm *pm, enum pm_event event);
	void (*stop)(struct link_pm *pm);
	void (*request_hold)(struct link_pm *pm);
	void (*release_hold)(struct link_pm *pm);
	bool (*link_active)(struct link_pm *pm);

	/*
	Call-back functions set by the common link PM framework and used by the
	physical link medium driver
	*/
	void (*unmount_cb)(struct link_pm *pm);
	void (*suspend_cb)(struct link_pm *pm);
	void (*resume_cb)(struct link_pm *pm);
	void (*mount_cb)(struct link_pm *pm);

	/*
	Physical link operations attached by the corresponding physical link
	medium driver with the attach_to_phys method
	*/
	struct phys_link_pm_ops *ops;
};

#endif
