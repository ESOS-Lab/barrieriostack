/* drivers/video/decon_display/s6e3ha0k_lcd_ctrl.c
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
#include "s6e3ha0k_gamma.h"
#include "s6e3ha0k_param.h"
#include "lcd_ctrl.h"
#include "../decon_display/dsim_reg.h"

#define GAMMA_PARAM_SIZE 26

/* Porch values. It depends on command or video mode */
#define S6E3HA0K_CMD_VBP	15
#define S6E3HA0K_CMD_VFP	1
#define S6E3HA0K_CMD_VSA	1
#define S6E3HA0K_CMD_HBP	1
#define S6E3HA0K_CMD_HFP	1
#define S6E3HA0K_CMD_HSA	1

/* These need to define */
#define S6E3HA0K_VIDEO_VBP	15
#define S6E3HA0K_VIDEO_VFP	1
#define S6E3HA0K_VIDEO_VSA	1
#define S6E3HA0K_VIDEO_HBP	20
#define S6E3HA0K_VIDEO_HFP	20
#define S6E3HA0K_VIDEO_HSA	20

#define S6E3HA0K_HORIZONTAL	1440
#define S6E3HA0K_VERTICAL	2560

/*
 * 3HA0K lcd init sequence
 *
 * Parameters
 *	- mic_enabled : if mic is enabled, MIC_ENABLE command must be sent
 *	- mode : LCD init sequence depends on command or video mode
 */
void lcd_init(struct decon_lcd *lcd)
{
	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to write F0 init command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_MIPI_SINGLE_DSI_SET1,
				ARRAY_SIZE(SEQ_MIPI_SINGLE_DSI_SET1)) < 0)
		dsim_err("fail to write DSI_SET1 command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				(unsigned long)SEQ_MIPI_SINGLE_DSI_SET2[0],
				(unsigned long)SEQ_MIPI_SINGLE_DSI_SET2[1]) < 0)
		dsim_err("fail to write DSI_SET2 command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("fail to write FC init command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_REG_FF,
				ARRAY_SIZE(SEQ_REG_FF)) < 0)
		dsim_err("fail to write SEQ_REG_FF command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_OFF_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
		dsim_err("fail to write FC OFF command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)SEQ_SLEEP_OUT[0], 0) < 0)
		dsim_err("fail to write Exit_sleep init command.\n");

	msleep(100);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_CAPS_ELVSS_SET,
				ARRAY_SIZE(SEQ_CAPS_ELVSS_SET)) < 0)
		dsim_err("fail to write SEQ_CAPS_ELVSS_SET command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_OFF_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("fail to write KEY_OFF_F0 command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_SHORT_WRITE_PARAM, SEQ_TE_ON[0], 0) < 0)
		dsim_err("fail to write TE_on init command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to write KEY_OFF_F0 command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("fail to write FC init command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				(unsigned long)SEQ_TOUCH_HSYNC_ON[0],
				(unsigned long)SEQ_TOUCH_HSYNC_ON[1]) < 0)
		dsim_err("fail to write SEQ_TOUCH_HSYNC_ON command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				(unsigned long)SEQ_TOUCH_VSYNC_ON[0],
				(unsigned long)SEQ_TOUCH_VSYNC_ON[1]) < 0)
		dsim_err("fail to write SEQ_TOUCH_VSYNC_ON command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_OFF_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
		dsim_err("fail to write KEY_OFF_FC command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_PENTILE_CONTROL,
				ARRAY_SIZE(SEQ_PENTILE_CONTROL)) < 0)
		dsim_err("fail to write SEQ_PENTILE_CONTROL command.\n");
	msleep(50);

	if (dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_OFF_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("fail to write KEY_OFF_F0 command.\n");
	msleep(50);
}

void lcd_enable(void)
{
	if (dsim_wr_data(MIPI_DSI_DCS_SHORT_WRITE, SEQ_DISPLAY_ON[0], 0) < 0)
		dsim_err("fail to send SEQ_DISPLAY_ON command.\n");
}

void lcd_disable(void)
{
	/* This function needs to implement */
}

/*
 * Set gamma values
 *
 * Parameter
 *	- backlightlevel : It is from 0 to 26.
 */
int lcd_gamma_ctrl(u32 backlightlevel)
{
/* This will be implemented
	int ret;
	ret = dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)gamma22_table[backlightlevel],
			GAMMA_PARAM_SIZE);
	if (ret) {
		dsim_err("fail to write gamma value.\n");
		return ret;
	}
*/
	return 0;
}

int lcd_gamma_update(void)
{
/* This will be implemented
	int ret;
	ret = dsim_wr_data(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret) {
		dsim_err("fail to update gamma value.\n");
		return ret;
	}
*/
	return 0;
}
