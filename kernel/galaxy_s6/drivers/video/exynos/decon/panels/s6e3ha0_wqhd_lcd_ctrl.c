/*
 * drivers/video/decon/panels/S6E3HA0_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s6e3ha0_wqhd_param.h"
#include "../dsim.h"
#include <video/mipi_display.h>


static int s6e3ha0_wqhd_probe(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	return ret;
}


static int s6e3ha0_wqhd_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);
	
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
 		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
 		goto displayon_err;
	}

displayon_err:
	return ret;
}


static int s6e3ha0_wqhd_exit(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	msleep(10);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(120);

exit_err:
	return ret;
}

static int s6e3ha0_wqhd_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {	
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_SINGLE_DSI_1, ARRAY_SIZE(SEQ_SINGLE_DSI_1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_1\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_SINGLE_DSI_2, ARRAY_SIZE(SEQ_SINGLE_DSI_2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_2\n", __func__);
		goto init_err;
	}
	msleep(5);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_err;
	}

	msleep(20);

	/* Common Setting */
	ret = dsim_write_hl_data(dsim, SEQ_TOUCH_HSYNC, ARRAY_SIZE(SEQ_TOUCH_HSYNC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TOUCH_HSYNC\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TOUCH_VSYNC, ARRAY_SIZE(SEQ_TOUCH_VSYNC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TOUCH_VSYNC\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_PENTILE_SETTING, ARRAY_SIZE(SEQ_PENTILE_SETTING));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_PENTILE_SETTING\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_err;
	}

	msleep(120);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_err;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_err;
	}

	/* elvss */
	ret = dsim_write_hl_data(dsim, SEQ_TSET_GLOBAL, ARRAY_SIZE(SEQ_TSET_GLOBAL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ELVSS_SET\n", __func__);
		goto init_err;
	}
	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_err;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF_OPR, ARRAY_SIZE(SEQ_ACL_OFF_OPR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_err;
	}
#endif

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_err;
	}
init_err:
	return ret;
}

struct dsim_panel_ops s6e3ha0_panel_ops = {
	.probe		= s6e3ha0_wqhd_probe,
	.displayon	= s6e3ha0_wqhd_displayon,
	.exit		= s6e3ha0_wqhd_exit,
	.init		= s6e3ha0_wqhd_init,
};

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &s6e3ha0_panel_ops;
}
