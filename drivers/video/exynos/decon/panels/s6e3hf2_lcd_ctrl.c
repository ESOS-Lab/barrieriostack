/*
 * drivers/video/decon/panels/s6e3hf2_lcd_ctrl.c
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

#include "s6e3hf2_param.h"
#include "lcd_ctrl.h"

/* use FW_TEST definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "../dsim_fw.h"
#include "mipi_display.h"
#else
#include "../dsim.h"
#include <video/mipi_display.h>
#endif

/* Porch values. It depends on command or video mode */
#define S6E3HF2_CMD_VBP	15
#define S6E3HF2_CMD_VFP	1
#define S6E3HF2_CMD_VSA	1
#define S6E3HF2_CMD_HBP	1
#define S6E3HF2_CMD_HFP	1
#define S6E3HF2_CMD_HSA	1

/* These need to define */
#define S6E3HF2_VIDEO_VBP	15
#define S6E3HF2_VIDEO_VFP	1
#define S6E3HF2_VIDEO_VSA	1
#define S6E3HF2_VIDEO_HBP	20
#define S6E3HF2_VIDEO_HFP	20
#define S6E3HF2_VIDEO_HSA	20

#define S6E3HF2_HORIZONTAL	1600
#define S6E3HF2_VERTICAL	2560

#ifdef FW_TEST /* This information is moved to DT */
#define CONFIG_FB_I80_COMMAND_MODE

struct decon_lcd s6e3hf2_lcd_info = {
#ifdef CONFIG_FB_I80_COMMAND_MODE
	.mode = DECON_MIPI_COMMAND_MODE,
	.vfp = S6E3HF2_CMD_VFP,
	.vbp = S6E3HF2_CMD_VBP,
	.hfp = S6E3HF2_CMD_HFP,
	.hbp = S6E3HF2_CMD_HBP,
	.vsa = S6E3HF2_CMD_VSA,
	.hsa = S6E3HF2_CMD_HSA,
#else
	.mode = DECON_VIDEO_MODE,
	.vfp = S6E3HF2_VIDEO_VFP,
	.vbp = S6E3HF2_VIDEO_VBP,
	.hfp = S6E3HF2_VIDEO_HFP,
	.hbp = S6E3HF2_VIDEO_HBP,
	.vsa = S6E3HF2_VIDEO_VSA,
	.hsa = S6E3HF2_VIDEO_HSA,
#endif
	.xres = S6E3HF2_HORIZONTAL,
	.yres = S6E3HF2_VERTICAL,

	/* Maybe, width and height will be removed */
	.width = 70,
	.height = 121,

	/* Mhz */
	.hs_clk = 1100,
	.esc_clk = 20,

	.fps = 60,
	.mic_enabled = 1,
	.mic_ver = MIC_VER_1_2,
};
#endif

/*
 * 3HF2 lcd init sequence
 *
 * Parameters
 *	- mic_enabled : if mic is enabled, MIC_ENABLE command must be sent
 *	- mode : LCD init sequence depends on command or video mode
 */

void lcd_init(int id, struct decon_lcd *lcd)
{
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to write F0 init command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("fail to write FC init command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)SEQ_SLEEP_OUT[0], 0) < 0)
		dsim_err("fail to write SLEEP_OUT init command.\n");
	msleep(100);

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_F2,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F2)) < 0)
		dsim_err("fail to write F2 init command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_F9,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F9)) < 0)
		dsim_err("fail to write F9 init command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_CA,
				ARRAY_SIZE(SEQ_TEST_KEY_CA)) < 0)
		dsim_err("fail to write CA init command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_GAMMA_UPDATE,
				ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("fail to write GAMMA_UPDATE init command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TE_ON,
				ARRAY_SIZE(SEQ_TE_ON)) < 0)
		dsim_err("fail to write TE_ON init command.\n");
}

void lcd_enable(int id)
{
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)SEQ_DISPLAY_ON[0], 0) < 0)
		dsim_err("fail to write DISPLAY_ON command.\n");
}

void lcd_disable(int id)
{
	/* This function needs to implement */
}

/*
 * Set gamma values
 *
 * Parameter
 *	- backlightlevel : It is from 0 to 26.
 */
int lcd_gamma_ctrl(int id, u32 backlightlevel)
{
/* This will be implemented
	int ret;
	ret = dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (u32)gamma22_table[backlightlevel],
			GAMMA_PARAM_SIZE);
	if (ret) {
		dsim_err("fail to write gamma value.\n");
		return ret;
	}
*/
	return 0;
}

int lcd_gamma_update(int id)
{
/* This will be implemented
	int ret;
	ret = dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (u32)SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret) {
		dsim_err("fail to update gamma value.\n");
		return ret;
	}
*/
	return 0;
}
