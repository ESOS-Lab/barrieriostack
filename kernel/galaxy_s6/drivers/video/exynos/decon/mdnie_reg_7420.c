/* linux/drivers/video/exynos/decon/mdnie_reg_7420.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Jiun Yu <jiun.yu@samsung.com>
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "mdnie_reg.h"

void mdnie_reg_set_img_size(u32 w, u32 h)
{
	mdnie_write(IHSIZE, w);
	mdnie_write(IVSIZE, h);
}

void mdnie_reg_set_hsync_period(u32 hync)
{
	mdnie_write(HSYNC_PERIOD, hync);
}

u32 mdnie_reg_read_input_data(void)
{
	return mdnie_read(INPUT_DATA_DISABLE);
}

void mdnie_reg_enable_input_data(void)
{
	mdnie_write(INPUT_DATA_DISABLE, 0);
}

/*void mdnie_reg_set_sLCE(u32 en)
{
	mdnie_write_mask(SLCE, en ? 1 : ~1, 0x1);
}*/

void mdnie_reg_set_AADE_LB(u32 en)
{
	mdnie_write_mask(AADE_LB, en ? 1 : ~1, 0x1);
}

void mdnie_reg_set_AADE(u32 en)
{
	mdnie_write_mask(AADE, en ? 1 : ~1, 0x1);
}

void mdnie_reg_set_SLCE_INT(u32 en)
{
	u32 val = en ? SLCE_INT_RESET : SLCE_INT_UNRESET;
	mdnie_write_mask(SLCE_INT, val, SLCE_INT_MASK);
}

void mdnie_reg_unmask_all(void)
{
	mdnie_write(REGISTER_MASK, 0);
}

void mdnie_reg_start(u32 w, u32 h)
{
	u32 hsync_period;
	decon_reg_set_mdnie_pclk(0, 1);

	mdnie_reg_set_img_size(w, h);
	hsync_period = (w >> 1) + MDNIE_FSM_HFP + MDNIE_FSM_HSW + MDNIE_FSM_HBP;
	mdnie_reg_set_hsync_period(hsync_period);
	mdnie_reg_enable_input_data();
	mdnie_reg_unmask_all();

	decon_reg_set_mdnie_blank(0, MDNIE_FSM_VFP, MDNIE_FSM_VSW, MDNIE_FSM_VBP,
			MDNIE_FSM_HFP + MDNIE_FSM_HSW + MDNIE_FSM_HBP);
	decon_reg_enable_mdnie(0, 1);
	decon_reg_update_standalone(0);
}

void mdnie_reg_frame_update(u32 w, u32 h)
{
	u32 hsync_period, pre_width, pre_height, aade_enable=0;

	pre_width = mdnie_read(IHSIZE);
	pre_height = mdnie_read(IVSIZE);

	if(pre_width != w || pre_height !=h)
		mdnie_reg_set_SLCE_INT(1);
	else
		mdnie_reg_set_SLCE_INT(0);

	mdnie_reg_set_img_size(w, h);
	hsync_period = (w >> 1) + MDNIE_FSM_HFP + MDNIE_FSM_HSW + MDNIE_FSM_HBP;
	mdnie_reg_set_hsync_period(hsync_period);

	aade_enable |= (0x01 & mdnie_read(AADE_LB));
	aade_enable |= (0x01 & mdnie_read(AADE));

	if(aade_enable && (w<=AADE_MIN_WIDTH || h<=AADE_MIN_HEIGHT)){
		decon_info("decon mdnie : aade_enable=%d, xres=%d, yres=%d\n", aade_enable, w, h);
		mdnie_reg_set_AADE_LB(0);
		mdnie_reg_set_AADE(0);
	}

	mdnie_reg_enable_input_data();
	mdnie_reg_unmask_all();
}

void mdnie_reg_stop(void)
{
	decon_reg_enable_mdnie(0, 0);
	decon_reg_set_mdnie_pclk(0, 0);
}
