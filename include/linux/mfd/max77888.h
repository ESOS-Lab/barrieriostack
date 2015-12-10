/*
 * max77888.h - Driver for the Maxim 77888
 *
 *  Copyright (C) 2011 Samsung Electrnoics
 *  Seoyoung Jeong <seo0.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.h
 *
 * MAX77888 has Charger, Flash LED, Haptic, MUIC devices.
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __MAX77888_H__
#define __MAX77888_H__

#include <linux/battery/sec_charger.h>
#define MFD_DEV_NAME "max77888"
#if defined(CONFIG_CHARGER_MAX77888)
struct max77888_charger_platform_data {
	struct max77888_charger_reg_data *init_data;
	int num_init_data;
	sec_battery_platform_data_t *sec_battery;
#if defined(CONFIG_WIRELESS_CHARGING) || defined(CONFIG_CHARGER_MAX77888)
	int wpc_irq_gpio;
	int vbus_irq_gpio;
	bool wc_pwr_det;
#endif
};
#endif

#ifdef CONFIG_VIBETONZ

struct max77888_haptic_platform_data {
	u16 max_timeout;
	u16 duty;
	u16 period;
	u16 reg2;
	char *regulator_name;
	unsigned int pwm_id;

	void (*init_hw) (void);
	void (*motor_en) (bool);
};
#endif

struct max77888_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct max77888_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;
#if 0	//temp
	struct muic_platform_data *muic_pdata;
#endif

	int num_regulators;
	struct max77888_regulator_data *regulators;
#if 0	//temp
#ifdef CONFIG_VIBETONZ
	/* haptic motor data */
	struct max77888_haptic_platform_data *haptic_data;
#endif
#endif
#if defined(CONFIG_CHARGER_MAX77888)
	/* charger data */
	sec_battery_platform_data_t *charger_data;
#endif
};

#endif /* __MAX77888_H__ */

