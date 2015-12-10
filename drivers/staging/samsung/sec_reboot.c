/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#endif
#include <mach/regs-pmu.h>
#include <linux/battery/sec_battery.h>
#include <linux/sec_batt.h>

void (*mach_restart)(char str, const char *cmd);
EXPORT_SYMBOL(mach_restart);

static void sec_power_off(void)
{
	int poweroff_try = 0;
	struct power_supply *ac_psy = power_supply_get_by_name("ac");
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	struct power_supply *wc_psy = power_supply_get_by_name("wireless");
	union power_supply_propval ac_val;
	union power_supply_propval usb_val;
	union power_supply_propval wc_val;

#ifdef CONFIG_OF
	int powerkey_gpio = -1;
	struct device_node *np, *pp;

	np = of_find_node_by_path("/gpio_keys");
	if (!np)
		return;
	for_each_child_of_node(np, pp) {
		uint keycode = 0;
		if (!of_find_property(pp, "gpios", NULL))
			continue;
		of_property_read_u32(pp, "linux,code", &keycode);
		if (keycode == KEY_POWER) {
			pr_info("%s: <%u>\n", __func__,  keycode);
			powerkey_gpio = of_get_gpio(pp, 0);
			break;
		}
	}
	of_node_put(np);

	if (!gpio_is_valid(powerkey_gpio)) {
		pr_err("Couldn't find power key node\n");
		return;
	}
#else
	int powerkey_gpio = GPIO_nPOWER;
#endif

	local_irq_disable();

	ac_psy->get_property(ac_psy, POWER_SUPPLY_PROP_ONLINE, &ac_val);
	usb_psy->get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &usb_val);
	wc_psy->get_property(wc_psy, POWER_SUPPLY_PROP_ONLINE, &wc_val);

	pr_info("[%s] AC[%d] : USB[%d] : WC[%d]\n", __func__,
		ac_val.intval, usb_val.intval, wc_val.intval);

	while (1) {
		/* Check reboot charging */
#ifdef CONFIG_SAMSUNG_BATTERY
			if ((ac_val.intval || usb_val.intval || wc_val.intval ||
			     (poweroff_try >= 5)) && !lpcharge) {
#else
			if ((ac_val.intval || usb_val.intval || wc_val.intval ||
			     (poweroff_try >= 5))) {
#endif
			pr_emerg("%s: charger connected() or power"
			     "off failed(%d), reboot!\n",
			     __func__, poweroff_try);
			/* To enter LP charging */
			writel(0x0, EXYNOS_PMU_INFORM2);

			flush_cache_all();
			mach_restart(0, 0);

			pr_emerg("%s: waiting for reboot\n", __func__);
			while (1)
				;
		}

		/* wait for power button release */
		if (gpio_get_value(powerkey_gpio)) {
			pr_emerg("%s: set PS_HOLD low\n", __func__);

			/* power off code
			 * PS_HOLD Out/High -->
			 * Low PS_HOLD_CONTROL, R/W, 0x1002_330C
			 */
			writel(readl(EXYNOS_PMU_PS_HOLD_CONTROL) & 0xFFFFFEFF,
			       EXYNOS_PMU_PS_HOLD_CONTROL);

			++poweroff_try;
			pr_emerg
			    ("%s: Should not reach here! (poweroff_try:%d)\n",
			     __func__, poweroff_try);
		} else {
		/* if power button is not released, wait and check TA again */
			pr_info("%s: PowerButton is not released.\n", __func__);
		}
		mdelay(1000);
	}
}

#define REBOOT_MODE_PREFIX	0x12345670
#define REBOOT_MODE_NONE	0
#define REBOOT_MODE_DOWNLOAD	1
#define REBOOT_MODE_UPLOAD	2
#define REBOOT_MODE_CHARGING	3
#define REBOOT_MODE_RECOVERY	4
#define REBOOT_MODE_FOTA	5
#define REBOOT_MODE_FOTA_BL	6	/* update bootloader */
#define REBOOT_MODE_SECURE	7	/* image secure check fail */
#define REBOOT_MODE_FWUP	9	/* emergency firmware update */
#if defined(CONFIG_DIAG_MODE)
#define REBOOT_MODE_DIAG	0xe		/* Diag enable for CP */
#endif

#define REBOOT_SET_PREFIX	0xabc00000
#define REBOOT_SET_DEBUG	0x000d0000
#define REBOOT_SET_SWSEL	0x000e0000
#define REBOOT_SET_SUD		0x000f0000
#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
#define REBOOT_SET_CP_DEBUG	0x000c0000
#endif

static void sec_reboot(char str, const char *cmd)
{

	local_irq_disable();

	pr_emerg("%s (%d, %s)\n", __func__, str, cmd ? cmd : "(null)");

	writel(0x12345678, EXYNOS_PMU_INFORM2);	/* Don't enter lpm mode */

	if (!cmd) {
		writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE, EXYNOS_PMU_INFORM3);
	} else {
		unsigned long value;
		if (!strcmp(cmd, "fota"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA,
			       EXYNOS_PMU_INFORM3);
		else if (!strcmp(cmd, "fota_bl"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA_BL,
			       EXYNOS_PMU_INFORM3);
		else if (!strcmp(cmd, "recovery"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY,
			       EXYNOS_PMU_INFORM3);
		else if (!strcmp(cmd, "download"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_DOWNLOAD,
			       EXYNOS_PMU_INFORM3);
		else if (!strcmp(cmd, "upload"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_UPLOAD,
			       EXYNOS_PMU_INFORM3);
		else if (!strcmp(cmd, "secure"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_SECURE,
			       EXYNOS_PMU_INFORM3);
		else if (!strcmp(cmd, "fwup"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FWUP,
			       EXYNOS_PMU_INFORM3);
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_DEBUG | value,
			       EXYNOS_PMU_INFORM3);
#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
		else if (!strncmp(cmd, "cpdebug", 7)
			 && !kstrtoul(cmd + 7, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_CP_DEBUG | value,
			       EXYNOS_PMU_INFORM3);
#endif
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SWSEL | value,
			       EXYNOS_PMU_INFORM3);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SUD | value,
			       EXYNOS_PMU_INFORM3);
		else if (!strncmp(cmd, "emergency", 9))
			writel(0, EXYNOS_PMU_INFORM3);
#if defined(CONFIG_DIAG_MODE)
		else if (!strncmp(cmd, "diag", 4)
				&& !kstrtoul(cmd + 4, 0, &value)) {
			writel(REBOOT_SET_PREFIX | REBOOT_MODE_DIAG | (value & 0x1),
				EXYNOS_PMU_INFORM3);
		}
#endif
		else
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE,
			       EXYNOS_PMU_INFORM3);
	}

	flush_cache_all();
	mach_restart(0, 0);

	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1)
		;
}

static int __init sec_reboot_init(void)
{
	mach_restart = arm_pm_restart;
	pm_power_off = sec_power_off;
	arm_pm_restart = sec_reboot;
	return 0;
}

subsys_initcall(sec_reboot_init);
