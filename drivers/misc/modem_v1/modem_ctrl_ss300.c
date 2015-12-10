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

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <linux/mipi-lli.h>
#include "modem_prj.h"
#include "modem_utils.h"

#include <linux/of_gpio.h>

#define MIF_INIT_TIMEOUT	(30 * HZ)

static struct wake_lock mc_wake_lock;

static void print_mc_state(struct modem_ctl *mc)
{
	int cp_on = gpio_get_value(mc->gpio_cp_on);
	int cp_reset  = gpio_get_value(mc->gpio_cp_reset);
	int cp_active = gpio_get_value(mc->gpio_phone_active);
	int cp_status = gpio_get_value(mc->gpio_cp_status);
	unsigned int event = mc->crash_info;

	mif_info("%s: %pf: MC state:%s on:%d reset:%d active:%d status:%d reason:%d\n",
		mc->name, CALLER, mc_state(mc), cp_on, cp_reset, cp_active,
		cp_status, event);
}

static irqreturn_t cp_active_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct link_device *ld;
	enum modem_state old_state;
	enum modem_state new_state;
	unsigned long flags;

	if (!cp_online(mc))
		goto exit;

	print_mc_state(mc);

	if (gpio_get_value(mc->gpio_cp_reset) == 0)
		goto exit;

	ld = get_current_link(mc->iod);

	spin_lock_irqsave(&mc->lock, flags);

	old_state = mc->phone_state;
	new_state = mc->phone_state;

	if (gpio_get_value(mc->gpio_phone_active) != 0) {
		if (gpio_get_value(mc->gpio_cp_on) == 0) {
			new_state = STATE_OFFLINE;
		} else if (old_state == STATE_ONLINE) {
			new_state = STATE_CRASH_EXIT;
		} else {
			mif_err("%s: %s: don't care!!!\n", mc->name, FUNC);
		}
	}

	if (new_state != old_state) {
		/* Change the modem state for RIL */
		mc->iod->modem_state_changed(mc->iod, new_state);

		/* Change the modem state for CBD */
		mc->bootd->modem_state_changed(mc->bootd, new_state);
	}

	spin_unlock_irqrestore(&mc->lock, flags);

	if ((old_state == STATE_ONLINE) && (new_state == STATE_CRASH_EXIT)) {
		if (ld->close_tx)
			ld->close_tx(ld);
	}

exit:
	return IRQ_HANDLED;
}

static int unmount_noti_cb(struct notifier_block *nb, unsigned long event,
			   void *data)
{
	struct modem_link_pm *pm;
	struct link_device *ld;
	struct modem_ctl *mc;

	if (!nb)
		return 0;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != PM_EVENT_LINK_UNMOUNTED)
		return 0;

	/* @data can be NULL if CP crash occurs during phone booting */
	if (!data)
		return 0;

	pm = (struct modem_link_pm *)data;
	ld = pm_to_link_device(pm);
	mc = ld->mc;

	complete_all(&mc->off_cmpl);

	return 0;
}

static inline void wait_for_link_unmount(struct modem_ctl *mc,
					 struct link_device *ld)
{
	long result;

	if (!ld->unmounted)
		return;

	if (ld->unmounted(ld))
		return;

	mif_info("%s: wait for %s link unmount ...\n", mc->name, ld->name);

	mc->unmount_nb.notifier_call = unmount_noti_cb;
	pm_register_unmount_notifier(&ld->pm, &mc->unmount_nb);

	result = wait_for_completion_interruptible_timeout(&mc->off_cmpl, 5*HZ);
	if (result <= 0) {
		if (result < 0)
			mif_err("%s: %s link unmount ... INTERRUPTED\n",
				mc->name, ld->name);
		else
			mif_err("%s: %s link unmount ... TIMEOUT\n",
				mc->name, ld->name);
	} else {
		mif_info("%s: %s link unmount ... DONE\n",
			mc->name, ld->name);
	}

	pm_unregister_unmount_notifier(&ld->pm, &mc->unmount_nb);
}

static int ss300_on(struct modem_ctl *mc)
{
	struct io_device *iod = mc->iod;
	struct link_device *ld = get_current_link(iod);
	unsigned long flags;

	mif_disable_irq(&mc->irq_cp_active);

	mif_info("%s: %s: +++\n", mc->name, FUNC);

	print_mc_state(mc);

	spin_lock_irqsave(&mc->lock, flags);

	iod->modem_state_changed(iod, STATE_OFFLINE);

	gpio_set_value(mc->gpio_pda_active, 1);

	if (mc->wake_lock && !wake_lock_active(mc->wake_lock)) {
		wake_lock(mc->wake_lock);
		mif_err("%s->wake_lock locked\n", mc->name);
	}

	if (ld->ready)
		ld->ready(ld);

	spin_unlock_irqrestore(&mc->lock, flags);

	gpio_set_value(mc->gpio_cp_on, 0);
	msleep(100);

	gpio_set_value(mc->gpio_cp_reset, 0);
	print_mc_state(mc);
	msleep(500);

	gpio_set_value(mc->gpio_cp_on, 1);
	msleep(100);

	if (ld->reset)
		ld->reset(ld);

	if (mc->gpio_ap_dump_int)
		gpio_set_value(mc->gpio_ap_dump_int, 0);

	gpio_set_value(mc->gpio_cp_reset, 1);
	print_mc_state(mc);
	msleep(300);

	mif_info("%s: %s: ---\n", mc->name, FUNC);
	return 0;
}

static int ss300_off(struct modem_ctl *mc)
{
	struct io_device *iod = mc->iod;
	struct link_device *ld = get_current_link(iod);
	unsigned long flags;
	int i;

	mif_disable_irq(&mc->irq_cp_active);

	mif_info("%s: %s: +++\n", mc->name, FUNC);

	print_mc_state(mc);

	spin_lock_irqsave(&mc->lock, flags);

	if (cp_offline(mc)) {
		spin_unlock_irqrestore(&mc->lock, flags);
		mif_err("%s: %s: OFFLINE already!!!\n", mc->name, FUNC);
		goto exit;
	}

	iod->modem_state_changed(iod, STATE_OFFLINE);

	spin_unlock_irqrestore(&mc->lock, flags);

	if (ld->close_tx)
		ld->close_tx(ld);

#if 0
	wait_for_link_unmount(mc, ld);
#endif

	if (gpio_get_value(mc->gpio_cp_on) == 0) {
		mif_err("%s: cp_on == 0\n", mc->name);
		goto exit;
	}

	/* wait for cp_active for 3 seconds */
	for (i = 0; i < 150; i++) {
		if (gpio_get_value(mc->gpio_phone_active))
			break;
		msleep(20);
	}

	print_mc_state(mc);

	if (ld->off)
		ld->off(ld);

	if (gpio_get_value(mc->gpio_cp_reset)) {
		mif_err("%s: %s: cp_reset -> 0\n", mc->name, FUNC);
		gpio_set_value(mc->gpio_cp_reset, 0);
		print_mc_state(mc);
	}

exit:
	mif_info("%s: %s: ---\n", mc->name, FUNC);
	return 0;
}

static int ss300_reset(struct modem_ctl *mc)
{
	mif_err("+++\n");

	if (ss300_off(mc))
		return -EIO;

	msleep(100);

	if (ss300_on(mc))
		return -EIO;

	mif_err("---\n");
	return 0;
}

static int ss300_force_crash_exit(struct modem_ctl *mc)
{
	mif_err("+++\n");

	if (mc->wake_lock && !wake_lock_active(mc->wake_lock)) {
		wake_lock(mc->wake_lock);
		mif_err("%s->wake_lock locked\n", mc->name);
	}

	gpio_set_value(mc->gpio_ap_dump_int, 1);
	mif_info("set ap_dump_int(%d) to high=%d\n",
		mc->gpio_ap_dump_int, gpio_get_value(mc->gpio_ap_dump_int));

	mif_err("---\n");
	return 0;
}

static int ss300_dump_reset(struct modem_ctl *mc)
{
	struct io_device *iod = mc->iod;
	struct link_device *ld = get_current_link(iod);
	unsigned int gpio_cp_reset = mc->gpio_cp_reset;
	unsigned long flags;

	mif_disable_irq(&mc->irq_cp_active);

	mif_info("%s: %s: +++\n", mc->name, FUNC);

	print_mc_state(mc);

	spin_lock_irqsave(&mc->lock, flags);

	iod->modem_state_changed(iod, STATE_CRASH_EXIT);

	if (mc->wake_lock && !wake_lock_active(mc->wake_lock)) {
		wake_lock(mc->wake_lock);
		mif_err("%s->wake_lock locked\n", mc->name);
	}

	if (ld->ready)
		ld->ready(ld);

	spin_unlock_irqrestore(&mc->lock, flags);

	gpio_set_value(gpio_cp_reset, 0);
	print_mc_state(mc);
	udelay(200);

	if (ld->reset)
		ld->reset(ld);

	gpio_set_value(gpio_cp_reset, 1);
	print_mc_state(mc);
	msleep(300);

	gpio_set_value(mc->gpio_ap_status, 1);

	mif_info("%s: %s: ---\n", mc->name, FUNC);
	return 0;
}

static int ss300_boot_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	unsigned long flags;
	mif_err("+++\n");

	if (ld->boot_on)
		ld->boot_on(ld, mc->bootd);

	gpio_set_value(mc->gpio_ap_status, 1);

	INIT_COMPLETION(mc->init_cmpl);
	INIT_COMPLETION(mc->off_cmpl);

	spin_lock_irqsave(&mc->lock, flags);
	mc->bootd->modem_state_changed(mc->bootd, STATE_BOOTING);
	mc->iod->modem_state_changed(mc->iod, STATE_BOOTING);
	spin_unlock_irqrestore(&mc->lock, flags);

	mif_err("---\n");
	return 0;
}

static int ss300_boot_off(struct modem_ctl *mc)
{
	unsigned long remain;
	unsigned long flags;
	mif_err("+++\n");

	remain = wait_for_completion_timeout(&mc->init_cmpl, MIF_INIT_TIMEOUT);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		mif_err("xxx\n");
		return -EAGAIN;
	}

	mif_enable_irq(&mc->irq_cp_active);

	spin_lock_irqsave(&mc->lock, flags);
	mc->bootd->modem_state_changed(mc->bootd, STATE_ONLINE);
	mc->iod->modem_state_changed(mc->iod, STATE_ONLINE);
	spin_unlock_irqrestore(&mc->lock, flags);

	atomic_set(&mc->forced_cp_crash, 0);

	mif_err("---\n");
	return 0;
}

static int ss300_boot_done(struct modem_ctl *mc)
{
	mif_err("+++\n");

	if (mc->wake_lock && wake_lock_active(mc->wake_lock)) {
		wake_unlock(mc->wake_lock);
		mif_err("%s->wake_lock unlocked\n", mc->name);
	}

	mif_err("---\n");
	return 0;
}

static void ss300_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = ss300_on;
	mc->ops.modem_off = ss300_off;
	mc->ops.modem_reset = ss300_reset;
	mc->ops.modem_boot_on = ss300_boot_on;
	mc->ops.modem_boot_off = ss300_boot_off;
	mc->ops.modem_boot_done = ss300_boot_done;
	mc->ops.modem_force_crash_exit = ss300_force_crash_exit;
	mc->ops.modem_dump_reset = ss300_dump_reset;
}

static int dt_gpio_config(struct modem_ctl *mc, struct modem_data *pdata)
{
	int ret = 0;
	struct device_node *np = mc->dev->of_node;

	/* GPIO_AP_DUMP_INT */
	pdata->gpio_ap_dump_int =
		of_get_named_gpio(np, "mif,gpio_ap_dump_int", 0);
	if (!gpio_is_valid(pdata->gpio_ap_dump_int)) {
		mif_err("ap_dump_int: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_ap_dump_int, "AP_DUMP_INT");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "AP_DUMP_INT", ret);
	gpio_direction_output(pdata->gpio_ap_dump_int, 0);

	return ret;
}

static int modemctl_notify_call(struct notifier_block *nfb,
		unsigned long event, void *arg)
{
	struct modem_ctl *mc = container_of(nfb, struct modem_ctl, event_nfb);
	static int abnormal_rx_cnt = 0;

	mif_info("got event: %ld\n", event);
	mc->crash_info = (unsigned int)event;

	switch (event) {
	case MDM_EVENT_CP_FORCE_RESET:
		if (mc->iod && mc->iod->modem_state_changed)
			mc->iod->modem_state_changed(mc->iod, STATE_CRASH_RESET);
		if (mc->bootd && mc->bootd->modem_state_changed)
			mc->bootd->modem_state_changed(mc->bootd, STATE_CRASH_RESET);
		break;
	case MDM_CRASH_PM_FAIL:
	case MDM_CRASH_PM_CP_FAIL:
	case MDM_CRASH_INVALID_RB:
	case MDM_CRASH_INVALID_IOD:
	case MDM_CRASH_INVALID_SKBCB:
	case MDM_CRASH_INVALID_SKBIOD:
	case MDM_CRASH_NO_MEM:
	case MDM_CRASH_CMD_RESET:
	case MDM_CRASH_CMD_EXIT:
	case MDM_EVENT_CP_FORCE_CRASH:
		ss300_force_crash_exit(mc);
		break;
	case MDM_EVENT_CP_ABNORMAL_RX:
		if (abnormal_rx_cnt < 5) {
			abnormal_rx_cnt++;
		} else {
			mif_err("abnormal rx count was overflowed.\n");
			abnormal_rx_cnt = 0;
		}
		break;
	}

	return 0;
}

int ss300_init_modemctl_device(struct modem_ctl *mc, struct modem_data *pdata)
{
	int ret = 0;
	unsigned int irq = 0;
	unsigned long flag = 0;
	char name[MAX_NAME_LEN];
	mif_err("+++\n");

	ret = dt_gpio_config(mc, pdata);
	if (ret < 0)
		return ret;

	if (!pdata->gpio_cp_on || !pdata->gpio_cp_reset
	    || !pdata->gpio_pda_active || !pdata->gpio_phone_active
	    || !pdata->gpio_ap_wakeup || !pdata->gpio_ap_status
	    || !pdata->gpio_cp_wakeup || !pdata->gpio_cp_status) {
		mif_err("ERR! no GPIO data\n");
		mif_err("xxx\n");
		return -ENXIO;
	}

	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_ap_wakeup = pdata->gpio_ap_wakeup;
	mc->gpio_ap_status = pdata->gpio_ap_status;
	mc->gpio_cp_wakeup = pdata->gpio_cp_wakeup;
	mc->gpio_cp_status = pdata->gpio_cp_status;
	mc->gpio_ap_dump_int = pdata->gpio_ap_dump_int;

	gpio_set_value(mc->gpio_cp_reset, 0);
	gpio_set_value(mc->gpio_cp_on, 0);
	print_mc_state(mc);

	ss300_get_ops(mc);
	dev_set_drvdata(mc->dev, mc);

	wake_lock_init(&mc_wake_lock, WAKE_LOCK_SUSPEND, "ss300_wake_lock");
	mc->wake_lock = &mc_wake_lock;

	irq = gpio_to_irq(mc->gpio_phone_active);
	if (!irq) {
		mif_err("ERR! no irq_cp_active\n");
		mif_err("xxx\n");
		return -EINVAL;
	}
	mif_err("PHONE_ACTIVE IRQ# = %d\n", irq);

	mc->event_nfb.notifier_call = modemctl_notify_call;
	register_cp_crash_notifier(&mc->event_nfb);

	flag = IRQF_TRIGGER_RISING | IRQF_NO_THREAD | IRQF_NO_SUSPEND;
	snprintf(name, MAX_NAME_LEN, "%s_active", mc->name);
	mif_init_irq(&mc->irq_cp_active, irq, name, flag);

	ret = mif_request_irq(&mc->irq_cp_active, cp_active_handler, mc);
	if (ret) {
		mif_err("%s: ERR! request_irq(%s#%d) fail (%d)\n",
			mc->name, mc->irq_cp_active.name,
			mc->irq_cp_active.num, ret);
		mif_err("xxx\n");
		return ret;
	}

	mif_err("---\n");
	return 0;
}

