/* drivers/muic/muic-core.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <mach/irqs.h>

/* switch device header */
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif /* CONFIG_SWITCH */

#if defined(CONFIG_USE_SAFEOUT)
#include <linux/regulator/consumer.h>
#endif

#if defined(CONFIG_MFD_MAX77804) || defined(CONFIG_MFD_MAX77804K)
#include <linux/mfd/max77804.h>
#include <linux/mfd/max77804-private.h>
#elif defined(CONFIG_MFD_MAX77828)
#include <linux/mfd/max77828.h>
#include <linux/mfd/max77828-private.h>
#elif defined(CONFIG_MFD_MAX77843)
#include <linux/mfd/max77843.h>
#include <linux/mfd/max77843-private.h>
#elif defined(CONFIG_MFD_MAX77833)
#include <linux/mfd/max77833.h>
#include <linux/mfd/max77833-private.h>
#elif defined(CONFIG_MFD_MAX77888)
#include <linux/mfd/max77888.h>
#include <linux/mfd/max77888-private.h>
#endif

#include <linux/muic/muic.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#ifdef CONFIG_SWITCH
static struct switch_dev switch_dock = {
	.name = "dock",
};
#endif /* CONFIG_SWITCH */

#if defined(CONFIG_MUIC_NOTIFIER)
static struct notifier_block dock_notifier_block;

static void muic_send_dock_intent(int type)
{
	pr_info("%s: MUIC dock type(%d)\n", __func__, type);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif
}

static int muic_dock_attach_notify(int type, const char *name)
{
	pr_info("%s: %s\n", __func__, name);
	muic_send_dock_intent(type);

	return NOTIFY_OK;
}

static int muic_dock_detach_notify(void)
{
	pr_info("%s\n", __func__);
	muic_send_dock_intent(MUIC_DOCK_DETACHED);

	return NOTIFY_OK;
}

static int muic_handle_dock_notification(struct notifier_block *nb,
			unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
	int type = MUIC_DOCK_DETACHED;
	const char *name;

	switch (attached_dev) {
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_DESKDOCK;
			name = "Desk Dock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_CARDOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_CARDOCK;
			name = "Car Dock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_SMARTDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		if (action == MUIC_NOTIFY_CMD_LOGICALLY_ATTACH) {
			type = MUIC_DOCK_SMARTDOCK;
			name = "Smart Dock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_LOGICALLY_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_SMARTDOCK;
			name = "Universal MMDock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_AUDIODOCK;
			name = "Audio Dock Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_HMT_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_HMT;
			name = "HMT Attach";
			return muic_dock_attach_notify(type, name);
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	default:
		break;
	}

	pr_info("%s: ignore(%d)\n", __func__, attached_dev);
	return NOTIFY_DONE;
}
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_USE_SAFEOUT)
int muic_set_safeout(int safeout_path)
{
	struct regulator *regulator;
	int ret;

	pr_info("%s:MUIC safeout path=%d\n", __func__, safeout_path);

	if (safeout_path == MUIC_PATH_USB_CP) {
		regulator = regulator_get(NULL, "safeout1_range");
		if (IS_ERR(regulator) || regulator == NULL)
			return -ENODEV;

		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2_range");
		if (IS_ERR(regulator) || regulator == NULL)
			return -ENODEV;

		if (!regulator_is_enabled(regulator)) {
			ret = regulator_enable(regulator);
			if (ret)
				goto err;
		}
		regulator_put(regulator);
	} else if (safeout_path == MUIC_PATH_USB_AP) {
		regulator = regulator_get(NULL, "safeout1_range");
		if (IS_ERR(regulator) || regulator == NULL)
			return -ENODEV;

		if (!regulator_is_enabled(regulator)) {
			ret = regulator_enable(regulator);
			if (ret)
				goto err;
		}
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2_range");
		if (IS_ERR(regulator) || regulator == NULL)
			return -ENODEV;

		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}  else {
		pr_info("%s: not control safeout(%d)\n", __func__, safeout_path);
		return -EINVAL;
	}

	return 0;
err:
	pr_info("%s: cannot regulator_enable (%d)\n", __func__, ret);
	regulator_put(regulator);
	return ret;
}
#endif /* CONFIG_USE_SAFEOUT */

static void muic_init_switch_dev_cb(void)
{
#ifdef CONFIG_SWITCH
	int ret;

	/* for DockObserver */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0) {
		pr_err("%s: Failed to register dock switch(%d)\n",
				__func__, ret);
		return;
	}
#endif /* CONFIG_SWITCH */

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&dock_notifier_block,
			muic_handle_dock_notification, MUIC_NOTIFY_DEV_DOCK);
#endif /* CONFIG_MUIC_NOTIFIER */

	pr_info("%s: done\n", __func__);
}

static void muic_cleanup_switch_dev_cb(void)
{
#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_unregister(&dock_notifier_block);
#endif /* CONFIG_MUIC_NOTIFIER */

	pr_info("%s: done\n", __func__);
}

extern struct muic_platform_data muic_pdata;

/* func : set_switch_sel
 * switch_sel value get from bootloader comand line
 * switch_sel data consist 8 bits (xxxxyyyyzzzz)
 * first 4bits(zzzz) mean path infomation.
 * next 4bits(yyyy) mean if pmic version info
 * next 4bits(xxxx) mean afc disable info
 */
static int set_switch_sel(char *str)
{
	get_option(&str, &muic_pdata.switch_sel);
	muic_pdata.switch_sel = (muic_pdata.switch_sel) & 0xfff;
	pr_info("%s: switch_sel: 0x%03x\n", __func__,
			muic_pdata.switch_sel);

	return muic_pdata.switch_sel;
}
__setup("pmic_info=", set_switch_sel);

int get_switch_sel(void)
{
	return muic_pdata.switch_sel;
}

bool is_muic_usb_path_ap_usb(void)
{
	if (MUIC_PATH_USB_AP == muic_pdata.usb_path) {
		pr_info("%s: [%d]\n", __func__, muic_pdata.usb_path);
		return true;
	}

	return false;
}

bool is_muic_usb_path_cp_usb(void)
{
	if (MUIC_PATH_USB_CP == muic_pdata.usb_path) {
		pr_info("%s: [%d]\n", __func__, muic_pdata.usb_path);
		return true;
	}

	return false;
}

static int muic_init_gpio_cb(int switch_sel)
{
	struct muic_platform_data *pdata = &muic_pdata;
	const char *usb_mode;
	const char *uart_mode;
	int ret = 0;

	pr_info("%s (%d)\n", __func__, switch_sel);

	if (switch_sel & SWITCH_SEL_USB_MASK) {
		pdata->usb_path = MUIC_PATH_USB_AP;
		usb_mode = "PDA";
	} else {
		pdata->usb_path = MUIC_PATH_USB_CP;
		usb_mode = "MODEM";
	}

	if (pdata->set_gpio_usb_sel)
		ret = pdata->set_gpio_usb_sel(pdata->uart_path);

	if (switch_sel & SWITCH_SEL_UART_MASK) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		uart_mode = "AP";
	} else {
		pdata->uart_path = MUIC_PATH_UART_CP;
		uart_mode = "CP";
	}

	pdata->rustproof_on = false;

#if defined(CONFIG_MUIC_RUSTPROOF_ON_USER) && !defined(CONFIG_SEC_FACTORY)
	if (!(switch_sel & SWITCH_SEL_RUSTPROOF_MASK))
		pdata->rustproof_on = true;
#endif /* CONFIG_MUIC_RUSTPROOF_ON_USER && !CONFIG_SEC_FACTORY */

	pdata->afc_disable = false;
#if defined(CONFIG_HV_MUIC_MAX77843_AFC) || defined(CONFIG_HV_MUIC_MAX77833_AFC)
	if (switch_sel & SWITCH_SEL_AFC_DISABLE_MASK)
		pdata->afc_disable = true;
	pr_info("%s afc_disable(%c)\n", __func__, (pdata->afc_disable ? 'T' : 'F'));
#endif /* CONFIG_HV_MUIC_MAX77843_AFC */

	if (pdata->set_gpio_uart_sel)
		ret = pdata->set_gpio_uart_sel(pdata->uart_path);

	pr_info("%s: usb_path(%s), uart_path(%s), rustproof(%c)\n", __func__, usb_mode,
			uart_mode, (pdata->rustproof_on ? 'T' : 'F'));

	return ret;
}

struct muic_platform_data muic_pdata = {
	.init_switch_dev_cb	= muic_init_switch_dev_cb,
	.cleanup_switch_dev_cb	= muic_cleanup_switch_dev_cb,
	.init_gpio_cb		= muic_init_gpio_cb,
#if defined(CONFIG_USE_SAFEOUT)
	.set_safeout		= muic_set_safeout,
#endif /* CONFIG_USE_SAFEOUT */
};
