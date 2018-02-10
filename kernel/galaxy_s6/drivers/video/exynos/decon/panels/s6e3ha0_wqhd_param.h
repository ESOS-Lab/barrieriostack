#ifndef __S6E3HA0_WQHD_PARAM_H__
#define __S6E3HA0_WQHD_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

#define POWER_IS_ON(pwr)			(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)			(level >= 6)
#define LEVEL_IS_CAPS_OFF(level)	(level <= 19)
#define UNDER_MINUS_20(temperature)	(temperature <= -20)


#define NORMAL_TEMPERATURE			25	/* 25 degrees Celsius */
#define UI_MAX_BRIGHTNESS 	255
#define UI_MIN_BRIGHTNESS 	0
#define UI_DEFAULT_BRIGHTNESS 120

#define S6E3HA0_MTP_ADDR 			0xC8
#define S6E3HA0_MTP_SIZE 			35
#define S6E3HA0_MTP_DATE_SIZE 		S6E3HA0_MTP_SIZE + 9
#define S6E3HA0_COORDINATE_REG		0xA1
#define S6E3HA0_COORDINATE_LEN		4
#define S6E3HA0_HBMGAMMA_REG		0xB4
#define S6E3HA0_HBMGAMMA_LEN		31
#define S6E3HA0_HBM_INDEX			65
#define S6E3HA0_ID_REG				0x04
#define S6E3HA0_ID_LEN				3
#define S6E3HA0_CODE_REG			0xD6
#define S6E3HA0_CODE_LEN			5
#define S6E3HA0_TSET_REG			0xB8 /* TSET: Global para 8th */
#define S6E3HA0_TSET_LEN			9
#define S6E3HA0_ELVSS_REG			0xB6
#define S6E3HA0_ELVSS_LEN			23   /* elvss: Global para 22th */

#define S6E3HA0_GAMMA_CMD_CNT 		36
#define S6E3HA0_AID_CMD_CNT			3
#define S6E3HA0_ELVSS_CMD_CNT		3


static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_TEST_KEY_ON_F1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_F1[] = {
	0xF1,
	0xA5, 0xA5,
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28
};

static const unsigned char SEQ_SINGLE_DSI_1[] = {
	0xF2,
	0x67
};

static const unsigned char SEQ_SINGLE_DSI_2[] = {
	0xF9,
	0x29
};

static const unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80,	0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x00, 0x00, 0x00
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x01, 0x03
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x98,	/* MPS_CON: ACL OFF */
	0x04	/* ELVSS: MAX*/
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03
};

static const unsigned char SEQ_GAMMA_UPDATE_L[] = {
	0xF7,
	0x00
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_OFF_OPR[] = {
	0xB5,
	0x41
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00
};

static const unsigned char SEQ_TSET_GLOBAL[] = {
	0xB0,
	0x05
};

static const unsigned char SEQ_TSET[] = {
	0xB8,
	0x19
};

static const unsigned char SEQ_PENTILE_SETTING[] = {
	0xC0,
	0x30, 0x00, 0xD8, 0xD8
};

static const unsigned char SEQ_TOUCH_HSYNC[] = {
	0xBD,
	0x85, 0x02, 0x16
};

static const unsigned char SEQ_TOUCH_VSYNC[] = {
	0xFF,
	0x02
};


#endif /* __S6E3HA0_PARAM_H__ */
