/*
 * max77833-muic.h - MUIC for the Maxim 77833
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * Seoyoung Jeong <seo0.jeong@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * This driver is based on max14577-muic.h
 *
 */

#ifndef __MAX77833_MUIC_H__
#define __MAX77833_MUIC_H__

#define MUIC_DEV_NAME			"muic-max77833"

typedef enum max77833_command_state {
	STATE_CMD_NONE	= 0,
	STATE_CMD_READ_TRANS,
	STATE_CMD_READ_TRANS_READY_W,
	STATE_CMD_WRITE_TRANS,
	STATE_CMD_RESPONSE,
} muic_command_state_t;

typedef enum max77833_muic_command_opcode {
	COMMAND_CONFIG_READ		= 0x01,
	COMMAND_CONFIG_WRITE		= 0x02,
	COMMAND_SWITCH_READ		= 0x03,
	COMMAND_SWITCH_WRITE		= 0x04,
	COMMAND_SYSTEM_MSG_READ		= 0x05,
	COMMAND_CHGDET_CONFIG_WRITE	= 0x12,
	COMMAND_ID_MONITOR_CONFIG_READ	= 0x21,
	COMMAND_ID_MONITOR_CONFIG_WRITE = 0x22,

	COMMAND_NONE			= 0xff,
} muic_command_opcode_t;

struct max77833_muic_command {
	struct mutex			command_mutex;
	muic_command_state_t		state;
	muic_command_opcode_t		opcode;
	u8				reg;
	u8				val;
	u8				rw_reg;
	u8				rw_mask;
	u8				rw_val;
	u8				rw_before_val;
	muic_command_opcode_t		rw_opcode;
};

/* muic chip specific internal data structure */
struct max77833_muic_data {
	struct device			*dev;
	struct i2c_client		*i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex			muic_mutex;
	struct mutex			reset_mutex;

	struct max77833_muic_command	*muic_command;

	/* model dependant mfd platform data */
	struct max77833_platform_data	*mfd_pdata;

	int				irq_idreg;
	int				irq_chgtyp;
	int				irq_sysmsg;
	int				irq_apcmdres;

	int				irq_mrxrdy;

	int				irq_reset_acokbf;

	/* model dependant muic platform data */
	struct muic_platform_data	*pdata;

	/* muic current attached device */
	muic_attached_dev_t		attached_dev;

	/* muic support vps list */
	bool muic_support_list[ATTACHED_DEV_NUM];

	bool				is_muic_ready;
	bool				is_muic_reset;

	bool				ignore_adcerr;

	/* check is otg test for jig uart off + vb */
	bool				is_otg_test;

	bool				is_factory_start;

	bool				is_afc_muic_ready;
	bool				is_afc_handshaking;
	bool				is_afc_muic_prepare;
	bool				is_charger_ready;
	bool				is_qc_vb_settle;

	u8				is_boot_dpdnvden;
	u8				tx_data;
	bool				is_mrxrdy;
	int				afc_count;
	muic_afc_data_t			afc_data;
	u8				qc_hv;
	struct delayed_work		hv_muic_qc_vb_work;

	/* muic status value */
	u8				status1;	//CIS
	u8				status2;
	u8				status3;
	u8                              status4;
	u8                              status5;
	u8                              status6;

	/* muic hvcontrol value */
	u8				hvcontrol1;
	u8				hvcontrol2;

};

/* max77833 muic register read/write related information defines. */

/* MAX77833 REGISTER ENABLE or DISABLE bit */
enum max77833_reg_bit_control {
	MAX77833_DISABLE_BIT		= 0,
	MAX77833_ENABLE_BIT,
};

/* MAX77833 STATUS1 register */	//CIS
#define STATUS1_IDRES_SHIFT             0
#define STATUS1_IDRES_MASK              (0xff << STATUS1_IDRES_SHIFT)

/* MAX77833 STATUS2 register */
#define STATUS2_CHGTYP_SHIFT            0
#define STATUS2_SPCHGTYP_SHIFT          3
#define STATUS2_CHGTYPRUN_SHIFT         7
#define STATUS2_CHGTYP_MASK             (0x7 << STATUS2_CHGTYP_SHIFT)
#define STATUS2_SPCHGTYP_MASK           (0x7 << STATUS2_SPCHGTYP_SHIFT)
#define STATUS2_CHGTYPRUN_MASK          (0x1 << STATUS2_CHGTYPRUN_SHIFT)

/* MAX77833 STATUS3 register */

/* MAX77833 DAT_IN register */
#define DAT_IN_SHIFT			0
#define DAT_IN_MASK			(0xff << DAT_IN_SHIFT)

/* MAX77833 DAT_OUT register */
#define DAT_OUT_SHIFT			0
#define DAT_OUT_MASK			(0xff << DAT_OUT_SHIFT)

/* MAX77833 SWITCH COMMAND */
#define COMN1SW_SHIFT                   0
#define COMP2SW_SHIFT                   3
#define IDBEN_SHIFT                     7
#define COMN1SW_MASK                    (0x7 << COMN1SW_SHIFT)
#define COMP2SW_MASK                    (0x7 << COMP2SW_SHIFT)
#define IDBEN_MASK                      (0x1 << IDBEN_SHIFT)
#define CLEAR_IDBEN_RSVD_MASK           (COMN1SW_MASK | COMP2SW_MASK) // ??

/* MAX77833 CONFIG COMMAND */
#define JIGSET_SHIFT			0
#define JIGSET_MASK			(0x3 << JIGSET_SHIFT)
#define SFOUT_SHIFT			2
#define SFOUT_MASK			(0x3 << SFOUT_SHIFT)
#define IDMONEN_SHIFT			6
#define IDMONEN_MASK			(0x1 << IDMONEN_SHIFT)
#define CHGDETEN_SHIFT                  7
#define CHGDETEN_MASK                   (0x1 << CHGDETEN_SHIFT)

/* MAX77833 ID Monitor Config */
#define MODE_SHIFT			2
#define MODE_MASK			(0x3 << MODE_SHIFT)

/// MAX77843 ///
/* MAX77843 STATUS1 register */
#define STATUS1_ADC_SHIFT               0
#define STATUS1_ADCERR_SHIFT            6
#define STATUS1_ADC1K_SHIFT             7
#define STATUS1_ADC_MASK                (0x1f << STATUS1_ADC_SHIFT)
#define STATUS1_ADCERR_MASK             (0x1 << STATUS1_ADCERR_SHIFT)
#define STATUS1_ADC1K_MASK              (0x1 << STATUS1_ADC1K_SHIFT)

/* MAX77843 STATUS2 register */
#define STATUS2_CHGDETRUN_SHIFT         3
#define STATUS2_DCDTMR_SHIFT            4
#define STATUS2_DXOVP_SHIFT             5
#define STATUS2_VBVOLT_SHIFT            6
#define STATUS2_CHGDETRUN_MASK          (0x1 << STATUS2_CHGDETRUN_SHIFT)
#define STATUS2_DCDTMR_MASK             (0x1 << STATUS2_DCDTMR_SHIFT)
#define STATUS2_DXOVP_MASK              (0x1 << STATUS2_DXOVP_SHIFT)
#define STATUS2_VBVOLT_MASK             (0x1 << STATUS2_VBVOLT_SHIFT)

/* MAX77843 CDETCTRL1 register */
#define CHGTYPM_SHIFT                   1
#define CDDELAY_SHIFT                   4
#define CHGTYPM_MASK                    (0x1 << CHGTYPM_SHIFT)
#define CDDELAY_MASK                    (0x1 << CDDELAY_SHIFT)

/* MAX77843 CONTROL1 register */

/* MAX77843 CONTROL2 register */
#define CTRL2_LOWPWR_SHIFT              0
#define CTRL2_CPEN_SHIFT                2
#define CTRL2_ACCDET_SHIFT              5
#define CTRL2_LOWPWR_MASK               (0x1 << CTRL2_LOWPWR_SHIFT)
#define CTRL2_CPEN_MASK                 (0x1 << CTRL2_CPEN_SHIFT)
#define CTRL2_ACCDET_MASK               (0x1 << CTRL2_ACCDET_SHIFT)
#define CTRL2_CPEN1_LOWPWD0     ((MAX77843_ENABLE_BIT << CTRL2_CPEN_SHIFT) | \
                                (MAX77843_DISABLE_BIT << CTRL2_ADCLOWPWR_SHIFT))
#define CTRL2_CPEN0_LOWPWD1     ((MAX77843_DISABLE_BIT << CTRL2_CPEN_SHIFT) | \
                                (MAX77843_ENABLE_BIT << CTRL2_ADCLOWPWR_SHIFT))

/* MAX77843 CONTROL3 register */

/* MAX77843 CONTROL4 register */
#define CTRL4_ADCDBSET_SHIFT            0
#define CTRL4_ADCMODE_SHIFT             6
#define CTRL4_ADCDBSET_MASK             (0x3 << CTRL4_ADCDBSET_SHIFT)
#define CTRL4_ADCMODE_MASK              (0x3 << CTRL4_ADCMODE_SHIFT)
/////////////////////////////////////////////////////////////////////////////

typedef enum {
	VB_LOW			= 0x00,
	VB_HIGH			= (0x1 << STATUS2_VBVOLT_SHIFT),

	VB_DONTCARE		= 0xff,
} vbvolt_t;

typedef enum {
	CHGDETRUN_FALSE		= 0x00,
	CHGDETRUN_TRUE		= (0x1 << STATUS2_CHGDETRUN_SHIFT),

	CHGDETRUN_DONTCARE	= 0xff,
} chgdetrun_t;

/* MAX77833 MUIC Output of USB Charger Detection */
typedef enum {
	/* No Valid voltage at VB (Vvb < Vvbdet) */
	CHGTYP_NO_VOLTAGE		= 0x00,
	/* Unknown (D+/D- does not present a valid USB charger signature) */
	CHGTYP_USB			= 0x01,
	/* Charging Downstream Port */
	CHGTYP_CDP			= 0x02,
	/* Dedicated Charger (D+/D- shorted) */
	CHGTYP_DEDICATED_CHARGER	= 0x03,
	/* Special 500mA charger, max current 500mA */
	CHGTYP_500MA			= 0x04,
	/* Special 1A charger, max current 1A */
	CHGTYP_1A			= 0x05,
	/* Special charger - 3.3V bias on D+/D- */
	CHGTYP_SPECIAL_3_3V_CHARGER	= 0x06,
	/* Reserved */
	CHGTYP_RFU			= 0x07,
	/* Any charger w/o USB */
	CHGTYP_UNOFFICIAL_CHARGER	= 0xfc,
	/* Any charger type */
	CHGTYP_ANY			= 0xfd,
	/* Don't care charger type */
	CHGTYP_DONTCARE			= 0xfe,

	CHGTYP_MAX,

	CHGTYP_INIT,
	CHGTYP_MIN = CHGTYP_NO_VOLTAGE
} chgtyp_t;

typedef enum {
	PROCESS_ATTACH		= 0,
	PROCESS_LOGICALLY_DETACH,
	PROCESS_NONE,
} process_t;

/* muic register value for COMN1, COMN2 in Switch command  */

/*
 * MAX77833 Switch command
 * ID Bypass [7] / Mic En [6] / D+ [5:3] / D- [2:0]
 * 0: ID Bypass Open / 1: IDB connect to UID
 * 0: Mic En Open / 1: Mic connect to VB
 * 111: Open / 001: USB / 010(enable),011(disable): Audio / 100: UART
 */
enum max77833_reg_switch_op_val {	//CIS
	MAX77833_MUIC_SWITCH_OP_ID_OPEN		= 0x0,
	MAX77833_MUIC_SWITCH_OP_ID_BYPASS	= 0x1,
	MAX77833_MUIC_SWITCH_OP_COM_OPEN	= 0x07,
	MAX77833_MUIC_SWITCH_OP_COM_USB		= 0x01,
	MAX77833_MUIC_SWITCH_OP_COM_AUDIO_ON	= 0x02,
	MAX77833_MUIC_SWITCH_OP_COM_AUDIO_OFF   = 0x03,
	MAX77833_MUIC_SWITCH_OP_COM_UART	= 0x04,
	MAX77833_MUIC_SWITCH_OP_COM_USB_CP	= 0x05,
	MAX77833_MUIC_SWITCH_OP_COM_UART_CP	= 0x06,
};

typedef enum {
	CTRL1_OPEN	= (MAX77833_MUIC_SWITCH_OP_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_OPEN << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_OPEN << COMN1SW_SHIFT),
	CTRL1_USB		= (MAX77833_MUIC_SWITCH_OP_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_USB << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_USB << COMN1SW_SHIFT),
	CTRL1_AUDIO	= (MAX77833_MUIC_SWITCH_OP_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_AUDIO_ON << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_AUDIO_ON << COMN1SW_SHIFT),
	CTRL1_UART	= (MAX77833_MUIC_SWITCH_OP_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_UART << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_UART << COMN1SW_SHIFT),
	CTRL1_USB_CP	= (MAX77833_MUIC_SWITCH_OP_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_USB_CP << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_USB_CP << COMN1SW_SHIFT),
	CTRL1_UART_CP	= (MAX77833_MUIC_SWITCH_OP_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_UART_CP << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_UART_CP << COMN1SW_SHIFT),
	CTRL1_USB_DOCK  = (MAX77833_MUIC_SWITCH_OP_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_USB << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_OP_COM_USB << COMN1SW_SHIFT),
// COM_OPEN, COM_USB, COM_AUDIO, COM_UART, COM_USB_CP, COM_UART_CP, COM_USB_DOCK

} max77833_reg_ctrl1_t; //max77833_reg_switch_op_t;

enum {
	MAX77833_MUIC_IDMODE_CONTINUOUS			= 0x3,
	MAX77833_MUIC_IDMODE_FACTORY_ONE_SHOT		= 0x2,
	MAX77833_MUIC_IDMODE_ONE_SHOT			= 0x1,
	MAX77833_MUIC_IDMODE_PULSE			= 0x0,
};
enum {
	MAX77833_MUIC_CTRL4_ADCMODE_ALWAYS_ON		= 0x00,
	MAX77833_MUIC_CTRL4_ADCMODE_ALWAYS_ON_1M_MON	= 0x01,
	MAX77833_MUIC_CTRL4_ADCMODE_ONE_SHOT		= 0x02,
	MAX77833_MUIC_CTRL4_ADCMODE_2S_PULSE		= 0x03
};

extern struct device *switch_device;

#endif /* __MAX77833_MUIC_H__ */

