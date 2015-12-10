/* /linux/drivers/misc/modem_if_v2/modem_modemctl_device_xmm7260.c
 *
 * Copyright (C) 2013 Samsung Electronics.
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

#include "modem_prj.h"
#include "modem_utils.h"

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

static int xmm7260_on(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_cp_reset || !mc->gpio_cp_on || !mc->gpio_reset_req_n) {
		mif_err("no gpio data\n");
		return -ENXIO;
	}

	gpio_set_value(mc->gpio_reset_req_n, 0);
	gpio_set_value(mc->gpio_cp_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(100);
	gpio_set_value(mc->gpio_cp_reset, 1);

	/* If XMM7160 was connected with C2C, AP wait 50ms to BB Reset*/
	msleep(50);

	gpio_set_value(mc->gpio_reset_req_n, 1);
	gpio_set_value(mc->gpio_cp_on, 1);
	udelay(60);
	gpio_set_value(mc->gpio_cp_on, 0);
	msleep(20);
	gpio_set_value(mc->gpio_pda_active, 1);

	if (mc->gpio_ap_dump_int)
		gpio_set_value(mc->gpio_ap_dump_int, 0);

	mc->phone_state = STATE_BOOTING;
	return 0;
}

static int xmm7260_off(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_cp_reset || !mc->gpio_cp_on) {
		mif_err("no gpio data\n");
		return -ENXIO;
	}

	gpio_set_value(mc->gpio_reset_req_n, 0);
	gpio_set_value(mc->gpio_pda_active, 0);
	gpio_set_value(mc->gpio_cp_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);

	mc->phone_state = STATE_OFFLINE;
	return 0;
}


static int xmm7260_reset(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_reset_req_n)
		return -ENXIO;

	gpio_set_value(mc->gpio_reset_req_n, 0);
	mc->phone_state = STATE_OFFLINE;

	msleep(10);
	gpio_set_value(mc->gpio_reset_req_n, 1);
	gpio_set_value(mc->gpio_pda_active, 1);

	if (mc->gpio_ap_dump_int)
		gpio_set_value(mc->gpio_ap_dump_int, 0);

	mc->phone_state = STATE_BOOTING;
	return 0;
}

static int xmm7260_boot_on(struct modem_ctl *mc)
{
	struct io_device *iod;
	struct link_device *ld;

	/* As of now, fmt0 always use LLI link */
	iod = get_iod_with_channel(mc->msd, SIPC5_CH_ID_FMT_0);

	if (!iod) {
		mif_err("There's no boot_on handler\n");
		return -ENXIO;
	}

	ld = get_current_link(iod);

	if (ld && ld->boot_on) {
		ld->boot_on(ld, NULL);
	} else {
		mif_err("There's no boot_on handler\n");
		return -ENXIO;
	}

	return 0;
}

static int xmm7260_force_crash_exit(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_ap_dump_int)
		return -ENXIO;

	gpio_set_value(mc->gpio_ap_dump_int, 1);
	mif_info("set ap_dump_int(%d) to high=%d\n",
		mc->gpio_ap_dump_int, gpio_get_value(mc->gpio_ap_dump_int));
	return 0;
}

static irqreturn_t phone_active_irq_handler(int irq, void *_mc)
{
	int phone_reset = 0;
	int phone_active_value = 0;
	int cp_dump_value = 0;
	int phone_state = 0;
	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	/* disable_irq_nosync(mc->irq_phone_active); */

	if (!mc->gpio_cp_reset || !mc->gpio_phone_active ||
			!mc->gpio_cp_dump_int) {
		mif_err("no gpio data\n");
		return IRQ_HANDLED;
	}

	phone_reset = gpio_get_value(mc->gpio_cp_reset);
	phone_active_value = gpio_get_value(mc->gpio_phone_active);
	cp_dump_value = gpio_get_value(mc->gpio_cp_dump_int);

	mif_info("PA EVENT : reset =%d(%d), pa=%d(%d), cp_dump=%d(%d)\n",
				phone_reset, mc->gpio_cp_reset,
				phone_active_value, mc->gpio_phone_active,
				cp_dump_value, mc->gpio_cp_dump_int);

	if (!phone_active_value) {
		if (cp_dump_value)
			phone_state = STATE_CRASH_EXIT;
		else
			phone_state = STATE_CRASH_RESET;

		if (mc->iod && mc->iod->modem_state_changed)
			mc->iod->modem_state_changed(mc->iod, phone_state);

		if (mc->bootd && mc->bootd->modem_state_changed)
			mc->bootd->modem_state_changed(mc->bootd, phone_state);
	}

	/* if (phone_active_value)
		irq_set_irq_type(mc->irq_phone_active, IRQ_TYPE_LEVEL_LOW);
	else
		irq_set_irq_type(mc->irq_phone_active, IRQ_TYPE_LEVEL_HIGH);
	enable_irq(mc->irq_phone_active); */

	return IRQ_HANDLED;
}

static irqreturn_t sim_detect_irq_handler(int irq, void *_mc)
{
	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	mif_info("SD EVENT : level=%d, online=%d, changed=%d\n",
		gpio_get_value(mc->gpio_sim_detect), mc->sim_state.online,
		mc->sim_state.changed);

	if (mc->iod && mc->iod->sim_state_changed)
		mc->iod->sim_state_changed(mc->iod,
			!gpio_get_value(mc->gpio_sim_detect));

	return IRQ_HANDLED;
}

static void xmm7260_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = xmm7260_on;
	mc->ops.modem_off = xmm7260_off;
	mc->ops.modem_reset = xmm7260_reset;
	mc->ops.modem_force_crash_exit = xmm7260_force_crash_exit;
	mc->ops.modem_boot_on = xmm7260_boot_on;
}

static int modemctl_notify_call(struct notifier_block *nfb,
		unsigned long event, void *arg)
{
	struct modem_ctl *mc = container_of(nfb, struct modem_ctl, event_nfb);

	mif_info("got event: %ld\n", event);

	switch (event) {
	case MDM_EVENT_CP_FORCE_RESET:
		if (mc->iod && mc->iod->modem_state_changed)
			mc->iod->modem_state_changed(mc->iod, STATE_CRASH_RESET);
		if (mc->bootd && mc->bootd->modem_state_changed)
			mc->bootd->modem_state_changed(mc->bootd, STATE_CRASH_RESET);
		break;
	case MDM_EVENT_CP_FORCE_CRASH:
		xmm7260_force_crash_exit(mc);
		break;
	}

	return 0;
}

#ifdef CONFIG_OF
static int dt_gpio_config(struct modem_ctl *mc,
			struct modem_data *pdata)
{
	int ret = 0;

	struct device_node *np = mc->dev->of_node;

	/* GPIO_RESET_REQ_N */
	pdata->gpio_reset_req_n =
		of_get_named_gpio(np, "mif,gpio_reset_req_n", 0);
	if (!gpio_is_valid(pdata->gpio_reset_req_n)) {
		mif_err("reset_req_n: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_reset_req_n, "RESET_REQ_N");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "RESET_REQ_N", ret);
	gpio_direction_output(pdata->gpio_reset_req_n, 0);

	/* GPIO_CP_DUMP_INT */
	pdata->gpio_cp_dump_int =
		of_get_named_gpio(np, "mif,gpio_cp_dump_int", 0);
	if (!gpio_is_valid(pdata->gpio_cp_dump_int)) {
		mif_err("cp_dump_int: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_cp_dump_int, "CP_DUMP_INT");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "CP_DUMP_INT", ret);
	gpio_direction_input(pdata->gpio_cp_dump_int);

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
#endif

noinline int xmm7260_init_modemctl_device(struct modem_ctl *mc,
			struct modem_data *pdata)
{
	int ret;

#ifdef CONFIG_OF
	ret = dt_gpio_config(mc, pdata);
	if (ret < 0)
		return ret;
#endif
	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_reset_req_n = pdata->gpio_reset_req_n;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_ap_dump_int = pdata->gpio_ap_dump_int;
	mc->gpio_cp_dump_int = pdata->gpio_cp_dump_int;
	mc->gpio_sim_detect = pdata->gpio_sim_detect;
	mc->gpio_ap_wakeup = pdata->gpio_ap_wakeup;
	mc->gpio_ap_status = pdata->gpio_ap_status;
	mc->gpio_cp_wakeup = pdata->gpio_cp_wakeup;
	mc->gpio_cp_status = pdata->gpio_cp_status;

	mif_info("cp_on=%d, reset_req_n=%d, cp_reset=%d, pda_active=%d\n",
		mc->gpio_cp_on, mc->gpio_reset_req_n,
		mc->gpio_cp_reset, mc->gpio_pda_active);
	mif_info("phone_active=%d, ap_dump_int=%d, cp_dump_int=%d," \
		" sim_detect=%d\n", mc->gpio_phone_active, mc->gpio_ap_dump_int,
		mc->gpio_cp_dump_int, mc->gpio_sim_detect);
	mif_info("ap_wakeup=%d, ap_status=%d, cp_wakeup=%d, cp_status=%d,",
		mc->gpio_ap_wakeup, mc->gpio_ap_status,
		mc->gpio_cp_wakeup, mc->gpio_cp_status);

	mc->irq_phone_active = gpio_to_irq(mc->gpio_phone_active);

	if (mc->gpio_sim_detect)
		mc->irq_sim_detect = gpio_to_irq(mc->gpio_sim_detect);

	mc->event_nfb.notifier_call = modemctl_notify_call;
	register_cp_crash_notifier(&mc->event_nfb);

	xmm7260_get_ops(mc);

	ret = request_irq(mc->irq_phone_active, phone_active_irq_handler,
			/* IRQF_NO_SUSPEND | IRQF_TRIGGER_HIGH, */
			IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING,
				"phone_active", mc);
	if (ret) {
		mif_err("failed to request_irq:%d\n", ret);
		return ret;
	}

	ret = enable_irq_wake(mc->irq_phone_active);
	if (ret) {
		mif_err("failed to enable_irq_wake:%d\n", ret);
		goto err_exit;
	}

	/* initialize sim_state if gpio_sim_detect exists */
	mc->sim_state.online = false;
	mc->sim_state.changed = false;
	if (mc->gpio_sim_detect) {
		ret = request_irq(mc->irq_sim_detect, sim_detect_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"sim_detect", mc);
		if (ret) {
			mif_err("failed to SD request_irq:%d\n", ret);
			goto err_exit;
		}

		ret = enable_irq_wake(mc->irq_sim_detect);
		if (ret) {
			mif_err("failed to SD enable_irq:%d\n", ret);
			free_irq(mc->irq_sim_detect, mc);
			goto err_exit;
		}

		/* initialize sim_state => insert: gpio=0, remove: gpio=1 */
		mc->sim_state.online = !gpio_get_value(mc->gpio_sim_detect);
		mif_info("SIM detected online=%d\n", mc->sim_state.online);
	}

	return ret;

err_exit:
	free_irq(mc->irq_phone_active, mc);
	return ret;
}
