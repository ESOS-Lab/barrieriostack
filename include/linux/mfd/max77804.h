/*
 * max77804.h - Driver for the Maxim 77804
 *
 *  Copyright (C) 2011 Samsung Electrnoics
 *  SangYoung Son <hello.son@samsung.com>
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
 * MAX77804 has Charger, Flash LED, Haptic, MUIC devices.
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __MAX77804_H__
#define __MAX77804_H__

#if defined(CONFIG_CHARGER_MAX77804)
#include <linux/battery/sec_charger.h>
#endif

#define MFD_DEV_NAME "max77804"

#ifdef CONFIG_VIBETONZ
#define MOTOR_LRA			(1<<7)
#define MOTOR_EN			(1<<6)
#define EXT_PWM				(0<<5)
#define DIVIDER_128			(1<<1)

struct max77804_haptic_platform_data {
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

#ifdef CONFIG_LEDS_MAX77804
struct max77804_led_platform_data;
#endif

struct max77804_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct max77804_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	int irqf_trigger;
	bool wakeup;

	struct muic_platform_data *muic_pdata;

	int num_regulators;
	struct max77804_regulator_data *regulators;
#ifdef CONFIG_VIBETONZ
	/* haptic motor data */
	struct max77804_haptic_platform_data *haptic_data;
#endif
#ifdef CONFIG_LEDS_MAX77804
	/* led (flash/torch) data */
	struct max77804_led_platform_data *led_data;
#endif
#if defined(CONFIG_CHARGER_MAX77804)
	/* charger data */
	sec_battery_platform_data_t *charger_data;
#endif
};

#endif /* __MAX77804_H__ */

