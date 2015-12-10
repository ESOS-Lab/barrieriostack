/* linux/drivers/video/decon_display/s6e3fa0_param.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __S6E3HA0K_PARAM_H__
#define __S6E3HA0K_PARAM_H__

/* MIPI commands list */
static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_MIPI_SINGLE_DSI_SET1[] = {
	0xF2,
	0x07, 0x00, 0x01, 0xA4, 0x03, 0x0d, 0xA0
};

static const unsigned char SEQ_MIPI_SINGLE_DSI_SET2[] = {
	0xF9,
	0x29
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_REG_FF[] = {
	0xFF,
	0x00, 0x00, 0x20, 0x00
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_CAPS_ELVSS_SET[] = {
	0xB6,
	0x98, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x55, 0x54,
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x22, 0x22, 0x10
};

static const unsigned char SEQ_TOUCH_HSYNC_ON[] = {
	0xBD,
	0x05
};

static const unsigned char SEQ_TOUCH_VSYNC_ON[] = {
	0xFF,
	0x02
};

static const unsigned char SEQ_PENTILE_CONTROL[] = {
	0xC0,
	0x74, 0x00, 0xD8, 0xD8
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
};

#endif /* __S6E3HA0K_PARAM_H__ */
