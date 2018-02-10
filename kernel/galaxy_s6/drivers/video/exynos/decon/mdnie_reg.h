/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos mDNIE driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_MDNIE_H__
#define __SAMSUNG_MDNIE_H__

#include "decon.h"

#define MDNIE_BASE  0xd000

#define INPUT_DATA_DISABLE		0x008
#define IHSIZE				0x00C
#define IVSIZE				0x010
#define HSYNC_PERIOD			0x014
#define SLCE				0x018
#define AADE_LB				0x01C
#define AADE				0x020
#define SLCE_INT			0x05C
#define SLCE_INT_MASK			(0x1 << 4)
#define SLCE_INT_RESET			(0x1 << 4)
#define SLCE_INT_UNRESET		(0x0 << 4)
#define REGISTER_MASK			0x3FC

#define AADE_MIN_WIDTH		8
#define AADE_MIN_HEIGHT		6

#define MDNIE_FSM_VFP		50
#define MDNIE_FSM_VSW		3
#define MDNIE_FSM_VBP		2
#define MDNIE_FSM_HFP		1
#define MDNIE_FSM_HSW		1
#define MDNIE_FSM_HBP		1

static inline u32 mdnie_read(u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(0);
	return readl(decon->regs + MDNIE_BASE + reg_id);
}

static inline u32 mdnie_read_mask(u32 reg_id, u32 mask)
{
	u32 val = mdnie_read(reg_id);
	val &= (~mask);
	return val;
}

static inline void mdnie_write(u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(0);
	writel(val, decon->regs + MDNIE_BASE + reg_id);
}

static inline void mdnie_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u32 old = mdnie_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, decon->regs + MDNIE_BASE + reg_id);
}

void mdnie_reg_set_img_size(u32 w, u32 h);
void mdnie_reg_set_hsync_period(u32 hync);
u32 mdnie_reg_read_input_data(void);
void mdnie_reg_enable_input_data(void);
void mdnie_reg_unmask_all(void);

void mdnie_reg_start(u32 w, u32 h);
void mdnie_reg_frame_update(u32 w, u32 h);
void mdnie_reg_stop(void);

#endif /* __SAMSUNG_MDNIE_H__ */
