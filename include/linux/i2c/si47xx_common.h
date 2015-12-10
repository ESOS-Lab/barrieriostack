/*
 * Si47xx_common.h  --  Si47xx FM Radio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _COMMON_H
#define _COMMON_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>

#define error(fmt, arg...) printk(KERN_CRIT fmt "\n", ##arg)

#if 1 //def Si47xx_DEBUG
#define debug(fmt, arg...) printk(KERN_CRIT "--------" fmt "\n", ##arg)
#else
#define debug(fmt, arg...)
#endif

#define FM_PORT "GPX13"

#define SI47XX_VOLUME_NUM 16

struct si47xx_platform_data {
	u16 rx_vol[SI47XX_VOLUME_NUM];
	int (*power) (int on);
	int rst_gpio;
	int int_gpio;
	int rclk_gpio;
	struct pinctrl *si47xx_pinctrl;
	struct pinctrl_state *si47xx_rst_default;
	struct pinctrl_state *si47xx_rst_active;
	struct pinctrl_state *si47xx_rst_suspend;
	struct pinctrl_state *si47xx_int_default;
	struct pinctrl_state *si47xx_int_active;
	struct pinctrl_state *si47xx_int_suspend;

};

struct Si47xx_data {
	struct si47xx_platform_data	*pdata;
	struct i2c_client *client;
	struct device	*dev;
};

/* VNVS:28-OCT'09 : For testing FM tune and seek operation status */
#define TEST_FM

/* VNVS:7-JUNE'10 : RDS Interrupt ON Always */
/* (Enabling interrupt when RDS is enabled) */
#define RDS_INTERRUPT_ON_ALWAYS

/* VNVS:18-JUN'10 : For testing RDS */
/* Enable only for debugging RDS */
/* #define RDS_TESTING */
#ifdef RDS_TESTING
#define debug_rds(fmt, arg...) printk(KERN_CRIT "--------" fmt "\n", ##arg)
#define GROUP_TYPE_2A     (2 * 2 + 0)
#define GROUP_TYPE_2B     (2 * 2 + 1)
#else
#define debug_rds(fmt, arg...)
#endif

extern wait_queue_head_t Si47xx_waitq;
#endif
