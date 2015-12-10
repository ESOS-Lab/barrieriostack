/*
 * stbcfg01.h - Voltage regulator driver for the STBCFG01 driver
 *
 *  Copyright (C) 2009-2010 Samsung Electrnoics
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
 */

#ifndef __LINUX_MFD_STBCFG01_H
#define __LINUX_MFD_STBCFG01_H

#include <linux/battery/sec_charger.h>
#include <linux/battery/sec_fuelgauge.h>

struct max77823_platform_data {
	sec_battery_platform_data_t *charger_data;
	sec_battery_platform_data_t *fuelgauge_data;

	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
};

#endif /*  __LINUX_MFD_MAX8998_H */
