/*
 * max77833-muic.c - MUIC driver for the Maxim 77833
 *
 *  Copyright (C) 2012 Samsung Electronics
 *  Seoyoung Jeong <seo0.jeong@samsung.com>
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <linux/mfd/max77833.h>
#include <linux/mfd/max77833-private.h>

/* MUIC header file */
#include <linux/muic/muic.h>
#include <linux/muic/max77833-muic-hv-typedef.h>
#include <linux/muic/max77833-muic.h>
//#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
#include <linux/muic/max77833-muic-hv.h>
//#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if !defined(CONFIG_SEC_FACTORY)
#if defined(CONFIG_MUIC_ADCMODE_SWITCH_WA)
#include <linux/delay.h>
#endif /* CONFIG_MUIC_ADCMODE_SWITCH_WA */
#endif /* !CONFIG_SEC_FACTORY */

#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
#include <linux/mfd/samsung/irq.h>
#endif

extern struct muic_platform_data muic_pdata;
static bool debug_en_vps = false;

struct max77833_muic_vps_data {
	muic_adc_t			adc;
	chgdetrun_t			chgdetrun;
	chgtyp_t			chgtyp;
	max77833_reg_ctrl1_t		control1;
	const char			*vps_name;
	const muic_attached_dev_t	attached_dev;
};

static const struct max77833_muic_vps_data muic_vps_table[] = {
#if 1
	{
		.adc		= ADC_JIG_USB_ON,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "Jig USB On",
		.attached_dev	= ATTACHED_DEV_JIG_USB_ON_MUIC,
	},
	{
		.adc		= ADC_JIG_USB_ON,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp 	= CHGTYP_1A,
		.control1	= CTRL1_USB,
		.vps_name	= "Jig USB On + VB",
		.attached_dev	= ATTACHED_DEV_JIG_USB_ON_MUIC,
	},
	{
		.adc		= ADC_JIG_UART_OFF,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART Off",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_MUIC,
	},
	{
		.adc		= ADC_OPEN,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp 	= CHGTYP_DEDICATED_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "TA",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
	{
		.adc		= ADC_OPEN,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp 	= CHGTYP_UNOFFICIAL_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "unofficial TA",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_TA_MUIC,
	},
	{
		.adc		= ADC_OPEN,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp 	= CHGTYP_USB,
		.control1	= CTRL1_USB,
		.vps_name	= "USB",
		.attached_dev	= ATTACHED_DEV_USB_MUIC,
	},
#else
	{
		.adc1k		= (0x1 << STATUS1_ADC1K_SHIFT),
		.adcerr		= 0x00,
		.adc		= ADC_DONTCARE,
		.vbvolt		= VB_DONTCARE,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "MHL",
		.attached_dev	= ATTACHED_DEV_MHL_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_GND,
		.vbvolt		= VB_LOW,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "OTG",
		.attached_dev	= ATTACHED_DEV_OTG_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_GND,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "OTG charging pump (vbvolt)",
		.attached_dev	= ATTACHED_DEV_OTG_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_CHARGING_CABLE,
		.vbvolt		= VB_DONTCARE,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "Charging Cable",
		.attached_dev	= ATTACHED_DEV_CHARGING_CABLE_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_JIG_USB_ON,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "Jig USB On",
		.attached_dev	= ATTACHED_DEV_JIG_USB_ON_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_JIG_UART_OFF,
		.vbvolt		= VB_LOW,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART Off",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_JIG_UART_OFF,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART Off + VB",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_VB_MUIC,
	},
#if defined(CONFIG_SEC_FACTORY)
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_JIG_UART_ON,
		.vbvolt		= VB_LOW,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART On",
		.attached_dev	= ATTACHED_DEV_JIG_UART_ON_MUIC,
	},
#endif /* CONFIG_SEC_FACTORY */
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "TA",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_UNOFFICIAL_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "unofficial TA",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_TA_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_USB,
		.control1	= CTRL1_USB,
		.vps_name	= "USB",
		.attached_dev	= ATTACHED_DEV_USB_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_CDP,
		.control1	= CTRL1_USB,
		.vps_name	= "CDP",
		.attached_dev	= ATTACHED_DEV_CDP_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_JIG_USB_OFF,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Unofficial ID",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_219,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Unofficial ID + TA",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_219,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_CDP,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Unofficial ID + CDP",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_219,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_UNOFFICIAL_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Unofficial ID + ANY TA",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_219,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_USB,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Unofficial ID + USB",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "TA or AFC",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_UNDEFINED,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Undefined Charging",
		.attached_dev	= ATTACHED_DEV_UNDEFINED_CHARGING_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_DESKDOCK,
		.vbvolt		= VB_LOW,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Deskdock",
		.attached_dev	= ATTACHED_DEV_DESKDOCK_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_DESKDOCK,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Deskdock + VB",
		.attached_dev	= ATTACHED_DEV_DESKDOCK_VB_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_SMARTDOCK,
		.vbvolt		= VB_LOW,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Smartdock",
		.attached_dev	= ATTACHED_DEV_SMARTDOCK_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_SMARTDOCK,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Smartdock + VB",
		.attached_dev	= ATTACHED_DEV_SMARTDOCK_VB_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_SMARTDOCK,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.control1	= CTRL1_USB_DOCK,
		.vps_name	= "Smartdock + TA",
		.attached_dev	= ATTACHED_DEV_SMARTDOCK_TA_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_SMARTDOCK,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_USB,
		.control1	= CTRL1_USB_DOCK,
		.vps_name	= "Smartdock + USB",
		.attached_dev	= ATTACHED_DEV_SMARTDOCK_USB_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_AUDIODOCK,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_USB,
		.vps_name	= "Audiodock",
		.attached_dev	= ATTACHED_DEV_AUDIODOCK_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_HMT,
		.vbvolt		= VB_DONTCARE,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "HMT",
		.attached_dev	= ATTACHED_DEV_HMT_MUIC,
	},
	/* Unsupported Device Type - Charging */
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_UNIVERSAL_MMDOCK,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_USB,
		.vps_name	= "Universal Multimedia dock",
		.attached_dev	= ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_RESERVED_VZW,
		.vbvolt		= VB_DONTCARE,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "VZW Accessory",
		.attached_dev	= ATTACHED_DEV_VZW_ACC_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_INCOMPATIBLE_VZW,
		.vbvolt		= VB_DONTCARE,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "VZW Incompatible",
		.attached_dev	= ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_USB_LANHUB,
		.vbvolt		= VB_DONTCARE,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "USB LANHUB",
		.attached_dev	= ATTACHED_DEV_USB_LANHUB_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adc		= ADC_CEA936ATYPE2_CHG,
		.vbvolt		= VB_DONTCARE,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "TYPE2 Charger",
		.attached_dev	= ATTACHED_DEV_TYPE2_CHG_MUIC,
	},
#endif
};

static int muic_lookup_vps_table(muic_attached_dev_t new_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		const struct max77833_muic_vps_data *tmp_vps;
		tmp_vps = &(muic_vps_table[i]);

		if (tmp_vps->attached_dev != new_dev)
			continue;

		pr_info("%s:%s (%d) vps table match found at i(%d), %s\n",
				MUIC_DEV_NAME, __func__, new_dev, i,
				tmp_vps->vps_name);

		return i;
	}

	pr_info("%s:%s can't find (%d) on vps table\n", MUIC_DEV_NAME,
			__func__, new_dev);

	return -ENODEV;
}

static bool muic_check_support_dev(struct max77833_muic_data *muic_data,
			muic_attached_dev_t attached_dev)
{
	bool ret = muic_data->muic_support_list[attached_dev];

	if (debug_en_vps)
		pr_info("%s:%s [%c]\n", MUIC_DEV_NAME, __func__, ret ? 'T':'F');

	return ret;
}

static int max77833_muic_write_reg
		(struct i2c_client *i2c, u8 reg, u8 value, bool debug_en)
{
	int ret = max77833_write_reg(i2c, reg, value);

	if (debug_en)
		pr_info("%s:%s Reg[0x%02x]: 0x%02x\n",
					MUIC_DEV_NAME, __func__, reg, value);

	return ret;
}

static int max77833_muic_update_reg(struct i2c_client *i2c, const u8 reg,
		const u8 val, const u8 mask, const bool debug_en)
{
	int ret = 0;
	u8 before_val, new_val, after_val;

	ret = max77833_read_reg(i2c, reg, &before_val);
	if (ret < 0)
		pr_err("%s:%s err read REG(0x%02x) [%d]\n", MUIC_DEV_NAME,
				__func__, reg, ret);

	new_val = (val & mask) | (before_val & (~mask));

	if (before_val ^ new_val) {
		ret = max77833_write_reg(i2c, reg, new_val);
		if (ret < 0)
			pr_err("%s:%s err write REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);
	} else if (debug_en) {
		pr_info("%s:%s REG(0x%02x): already [0x%02x], don't write reg\n",
				MUIC_DEV_NAME, __func__, reg, before_val);
		goto out;
	}

	if (debug_en) {
		ret = max77833_read_reg(i2c, reg, &after_val);
		if (ret < 0)
			pr_err("%s:%s err read REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);

		pr_info("%s:%s REG(0x%02x): [0x%02x]+[0x%02x:0x%02x]=[0x%02x]\n",
				MUIC_DEV_NAME, __func__, reg, before_val,
				val, mask, after_val);
	}

out:
	return ret;
}

static void muic_command_reset(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s \n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_command->command_mutex);
	muic_data->muic_command->state = STATE_CMD_NONE;
	muic_data->muic_command->opcode = COMMAND_NONE;
	muic_data->muic_command->reg = 0xff;
	muic_data->muic_command->val = 0xff;
	muic_data->muic_command->rw_reg = 0xff;
	muic_data->muic_command->rw_mask = 0xff;
	muic_data->muic_command->rw_val = 0xff;
	muic_data->muic_command->rw_before_val = 0xff;
	muic_data->muic_command->rw_opcode = COMMAND_NONE;
	mutex_unlock(&muic_data->muic_command->command_mutex);

	return;
}

static void write_muic_end_command(struct max77833_muic_data *muic_data)
{
	u8 reg;
	int ret;

	reg = MAX77833_MUIC_REG_DAT_IN8;
	ret = max77833_muic_write_reg(muic_data->i2c, reg, 0x00, true);
	if (ret < 0)
		pr_err("%s:%s err write REG(0x%02x):0x%02x [%d]\n",
			MUIC_DEV_NAME, __func__, reg, 0x00, ret);
}

#if defined(CONFIG_MUIC_USE_READ_WRITE_COMMAND)
static void write_muic_read_command(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg;
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg = MAX77833_MUIC_REG_DAT_IN_OP;
	ret = max77833_muic_write_reg(i2c, reg, muic_command->opcode, true);
	if (ret < 0)
		pr_err("%s:%s err write REG(0x%02x):0x%02x [%d]\n",
			MUIC_DEV_NAME, __func__, reg, muic_command->opcode, ret);

	write_muic_end_command(muic_data);

	mutex_lock(&muic_command->command_mutex);
	muic_command->state = STATE_CMD_READ_TRANS;
	mutex_unlock(&muic_command->command_mutex);
}

static void write_muic_write_command(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg;
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg = MAX77833_MUIC_REG_DAT_IN_OP;
	ret = max77833_muic_write_reg(i2c, reg, muic_command->opcode, true);
	if (ret < 0)
		pr_err("%s:%s err write REG(0x%02x):0x%02x [%d]\n",
				MUIC_DEV_NAME, __func__, reg, opcode, ret);

	reg = muic_command->reg;
	ret = max77833_muic_write_reg(i2c, reg, muic_command->val, true);
	if (ret < 0)
		pr_err("%s:%s err write REG(0x%02x):0x%02x [%d]\n",
			MUIC_DEV_NAME, __func__, reg, muic_command->val, ret);
	else
		write_muic_end_command(muic_data);

	mutex_lock(&muic_command->command_mutex);
	muic_data->state = STATE_CMD_WRITE_TRANS;
	mutex_unlock(&muic_command->command_mutex);
}

#endif

static void write_muic_rw_read_command(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg;
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg = MAX77833_MUIC_REG_DAT_IN_OP;
	ret = max77833_muic_write_reg(i2c, reg, muic_command->opcode, true);
	if (ret < 0)
		pr_err("%s:%s err write REG(0x%02x):0x%02x [%d]\n",
			MUIC_DEV_NAME, __func__, reg, muic_command->opcode, ret);

	write_muic_end_command(muic_data);

	mutex_lock(&muic_command->command_mutex);
	muic_command->state = STATE_CMD_READ_TRANS_READY_W;
	mutex_unlock(&muic_command->command_mutex);
}

static void write_muic_rw_write_command(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg, new_val;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_command->opcode = muic_command->rw_opcode;

	new_val = (muic_command->rw_val & muic_command->rw_mask) | \
		(muic_command->rw_before_val & (~(muic_command->rw_mask)));

	if (muic_command->rw_before_val ^ new_val) {
		int ret;
		reg = MAX77833_MUIC_REG_DAT_IN_OP;
		ret = max77833_muic_write_reg(i2c, reg, muic_command->opcode, true);
		if (ret < 0)
			pr_err("%s:%s err write REG(0x%02x):0x%02x [%d]\n",
					MUIC_DEV_NAME, __func__,
					reg, muic_command->opcode, ret);

		reg = muic_command->rw_reg;
		ret = max77833_muic_write_reg(i2c, reg, new_val, true);
		if (ret < 0)
			pr_err("%s:%s err write REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);

		write_muic_end_command(muic_data);
	} else {
		pr_info("%s:%s REG(0x%02x): already [0x%02x], don't write reg\n",
				MUIC_DEV_NAME, __func__,
				reg, muic_command->rw_before_val);

		mutex_lock(&muic_command->command_mutex);
		muic_command->state = STATE_CMD_NONE;
		mutex_unlock(&muic_command->command_mutex);

		return;
	}

	mutex_lock(&muic_command->command_mutex);
	muic_command->state = STATE_CMD_WRITE_TRANS;
	mutex_unlock(&muic_command->command_mutex);

	return;
}

static void write_muic_ctrl_reg(struct max77833_muic_data *muic_data,
				const u8 reg, const u8 val)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_command->opcode	= COMMAND_SWITCH_READ;

	muic_command->rw_reg	= reg;
	muic_command->rw_mask	= 0xff;
	muic_command->rw_val	= val;
	muic_command->rw_opcode	= COMMAND_SWITCH_WRITE;

	write_muic_rw_read_command(muic_data);
}

static void com_to_open(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_command->opcode	= COMMAND_SWITCH_READ;

	muic_command->rw_reg	= MAX77833_MUIC_REG_DAT_IN1;
	muic_command->rw_mask	= 0xff;
	muic_command->rw_val	= CTRL1_OPEN;
	muic_command->rw_opcode	= COMMAND_SWITCH_WRITE;

	/* write control1 register */
	write_muic_rw_read_command(muic_data);
}

static void com_to_usb_ap(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_command->opcode	= COMMAND_SWITCH_READ;

	muic_command->rw_reg	= MAX77833_MUIC_REG_DAT_IN1;
	muic_command->rw_mask	= 0xff;
	muic_command->rw_val	= CTRL1_USB;
	muic_command->rw_opcode	= COMMAND_SWITCH_WRITE;

	/* write control1 register */
	write_muic_rw_read_command(muic_data);
}

static void com_to_usb_cp(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_command->opcode	= COMMAND_SWITCH_READ;

	muic_command->rw_reg	= MAX77833_MUIC_REG_DAT_IN1;
	muic_command->rw_mask	= 0xff;
	muic_command->rw_val	= CTRL1_USB_CP;
	muic_command->rw_opcode	= COMMAND_SWITCH_WRITE;

	/* write control1 register */
	write_muic_rw_read_command(muic_data);
}

static void com_to_uart_ap(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_command->opcode	= COMMAND_SWITCH_READ;

	muic_command->rw_reg	= MAX77833_MUIC_REG_DAT_IN1;
	muic_command->rw_mask	= 0xff;
	muic_command->rw_val	= CTRL1_UART;
	muic_command->rw_opcode	= COMMAND_SWITCH_WRITE;

	/* write control1 register */
	write_muic_rw_read_command(muic_data);
}

static void com_to_uart_cp(struct max77833_muic_data *muic_data)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_command->opcode	= COMMAND_SWITCH_READ;

	muic_command->rw_reg	= MAX77833_MUIC_REG_DAT_IN1;
	muic_command->rw_mask	= 0xff;
	muic_command->rw_val	= CTRL1_UART_CP;
	muic_command->rw_opcode	= COMMAND_SWITCH_WRITE;

	/* write control1 register */
	write_muic_rw_read_command(muic_data);
}

static int write_vps_regs(struct max77833_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	const struct max77833_muic_vps_data *tmp_vps;
	int vps_index;

	vps_index = muic_lookup_vps_table(new_dev);
	if (vps_index < 0)
		return -ENODEV;

	tmp_vps = &(muic_vps_table[vps_index]);

	/* write control1 register */
	write_muic_ctrl_reg(muic_data, MAX77833_MUIC_REG_DAT_IN1,
			tmp_vps->control1);
	return 0;
}

/* muic uart path control function */
static int switch_to_ap_uart(struct max77833_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	int ret = 0;

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		com_to_uart_ap(muic_data);
		break;
	default:
		pr_warn("%s:%s current attached is (%d) not Jig UART Off\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev);
		break;
	}

	return ret;
}

static int switch_to_cp_uart(struct max77833_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	int ret = 0;

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		com_to_uart_cp(muic_data);
		break;
	default:
		pr_warn("%s:%s current attached is (%d) not Jig UART Off\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev);
		break;
	}

	return ret;
}

static int max77833_muic_enable_chgdet(struct max77833_muic_data *muic_data)
{
	return 0;
}

static int max77833_muic_disable_chgdet(struct max77833_muic_data *muic_data)
{
	return 0;
}

static int max77833_muic_enable_accdet(struct max77833_muic_data *muic_data)
{
	return 0;
}

static int max77833_muic_disable_accdet(struct max77833_muic_data *muic_data)
{
	return 0;
}

static u8 max77833_muic_get_adc_value(struct max77833_muic_data *muic_data)
{
	u8 status;
	u8 adc = ADC_ERROR;
	int ret;

	ret = max77833_read_reg(muic_data->i2c, MAX77833_MUIC_REG_STATUS1,
			&status);
	if (ret)
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
	else
		adc = status & STATUS1_ADC_MASK;

	if(adc >= 0x10) adc-= 0x10;

	return adc;
}

static ssize_t max77833_muic_show_uart_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		mode = "AP\n";
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP\n";
		break;
	default:
		break;
	}

	pr_info("%s:%s %s", MUIC_DEV_NAME, __func__, mode);
	return sprintf(buf, mode);
}

static ssize_t max77833_muic_set_uart_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "AP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		switch_to_ap_uart(muic_data, muic_data->attached_dev);
	} else if (!strncasecmp(buf, "CP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		switch_to_cp_uart(muic_data, muic_data->attached_dev);
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s uart_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->uart_path);

	return count;
}

static ssize_t max77833_muic_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		mode = "PDA\n";
		break;
	case MUIC_PATH_USB_CP:
		mode = "MODEM\n";
		break;
	default:
		break;
	}

	pr_info("%s:%s %s", MUIC_DEV_NAME, __func__, mode);
	return sprintf(buf, mode);
}

static ssize_t max77833_muic_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3)) {
		pdata->usb_path = MUIC_PATH_USB_AP;
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		pdata->usb_path = MUIC_PATH_USB_CP;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	return count;
}

static ssize_t max77833_muic_show_uart_en(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!pdata->rustproof_on) {
		pr_info("%s:%s UART ENABLE\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s:%s UART DISABLE", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "0\n");
}

static ssize_t max77833_muic_set_uart_en(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "1", 1)) {
		pdata->rustproof_on = false;
	} else if (!strncasecmp(buf, "0", 1)) {
		pdata->rustproof_on = true;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s uart_en(%d)\n", MUIC_DEV_NAME, __func__,
			!pdata->rustproof_on);

	return count;
}

static ssize_t max77833_muic_show_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	u8 adc;

	adc = max77833_muic_get_adc_value(muic_data);
	pr_info("%s:%s adc(0x%02x)\n", MUIC_DEV_NAME, __func__, adc);

	if (adc == ADC_ERROR) {
		pr_err("%s:%s fail to read adc value\n", MUIC_DEV_NAME,
				__func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", adc);
}

static ssize_t max77833_muic_show_usb_state(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return sprintf(buf, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t max77833_muic_show_attached_dev(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	const struct max77833_muic_vps_data *tmp_vps;
	int vps_index;

	vps_index = muic_lookup_vps_table(muic_data->attached_dev);
	if (vps_index < 0)
		return sprintf(buf, "Error No Device\n");

	tmp_vps = &(muic_vps_table[vps_index]);

	return sprintf(buf, "%s\n", tmp_vps->vps_name);
}

static ssize_t max77833_muic_show_otg_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct i2c_client *i2c = muic_data->i2c;
	int ret = -ENODEV;
	u8 val;

	if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC)) {
		ret = max77833_read_reg(i2c, MAX77833_MUIC_REG_CDETCTRL1, &val);
		pr_info("%s:%s ret:%d val:%x buf:%s\n", MUIC_DEV_NAME, __func__, ret, val, buf);
		if (ret) {
			pr_err("%s:%s: fail to read muic reg\n", MUIC_DEV_NAME, __func__);
			return sprintf(buf, "UNKNOWN\n");
		}
		val &= CHGDETEN_MASK;
		return sprintf(buf, "%x\n", val);
	} else
		return ret;
}

static ssize_t max77833_muic_set_otg_test(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct i2c_client *i2c = muic_data->i2c;
	u8 val = 0;
	int ret = -ENODEV;

	if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC)) {
		pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);
		if (!strncmp(buf, "0", 1)) {
			muic_data->is_otg_test = true;
			ret = max77833_muic_disable_chgdet(muic_data);
			if (ret)
				goto err_chgdet;
			ret = max77833_muic_enable_accdet(muic_data);
			if (ret)
				goto err_accdet;
		} else if (!strncmp(buf, "1", 1)) {
			muic_data->is_otg_test = false;
			ret = max77833_muic_enable_chgdet(muic_data);
			if (ret)
				goto err_chgdet;
		} else {
			pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
			return count;
		}

		max77833_read_reg(i2c, MAX77833_MUIC_REG_CDETCTRL1, &val);
		pr_info("%s:%s CDETCTRL(0x%02x)\n", MUIC_DEV_NAME, __func__, val);

		return count;
	} else
		return ret;

err_chgdet:
	pr_err("%s:%s cannot change chgdet!\n", MUIC_DEV_NAME, __func__);
	return ret;

err_accdet:
	pr_err("%s:%s cannot change accdet!\n", MUIC_DEV_NAME, __func__);
	return ret;
}

static ssize_t max77833_muic_show_apo_factory(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t max77833_muic_set_apo_factory(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
#if defined(CONFIG_SEC_FACTORY)
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
#endif /* CONFIG_SEC_FACTORY */
	const char *mode;

	pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
#if defined(CONFIG_SEC_FACTORY)
		muic_data->is_factory_start = true;
#endif /* CONFIG_SEC_FACTORY */
		mode = "FACTORY_MODE";
	} else {
		pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return count;
}

#if defined(CONFIG_MUIC_MAX77833_IGNORE_ADCERR_WA) && defined(CONFIG_SEC_FACTORY)
static ssize_t max77833_muic_show_ignore_adcerr(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s ignore_adcerr[%c]", MUIC_DEV_NAME, __func__,
				(muic_data->ignore_adcerr ? 'T' : 'F'));

	if (muic_data->ignore_adcerr)
		return sprintf(buf, "1\n");

	return sprintf(buf, "0\n");
}

static ssize_t max77833_muic_set_ignore_adcerr(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);

	if (!strncasecmp(buf, "1", 1)) {
		muic_data->ignore_adcerr = true;
	} else if (!strncasecmp(buf, "0", 0)) {
		muic_data->ignore_adcerr = false;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s ignore adc_err(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->ignore_adcerr);

	return count;
}
#endif /* CONFIG_MUIC_MAX77833_IGNORE_ADCERR_WA && CONFIG_SEC_FACTORY */

static void max77833_muic_set_adcdbset
			(struct max77833_muic_data *muic_data, int value)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret;
	u8 val;

	if (value > 3) {
		pr_err("%s:%s invalid value(%d)\n", MUIC_DEV_NAME, __func__,
				value);
		return;
	}

	if (!i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	val = (value << CTRL4_ADCDBSET_SHIFT);
	ret = max77833_muic_update_reg(i2c, MAX77833_MUIC_REG_CTRL4, val,
			CTRL4_ADCDBSET_MASK, true);
	if (ret < 0)
		pr_err("%s: fail to write reg\n", __func__);
}

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
static ssize_t max77833_muic_show_afc_disable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (pdata->afc_disable) {
		pr_info("%s:%s AFC DISABLE\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s:%s AFC ENABLE", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "0\n");
}

static ssize_t max77833_muic_set_afc_disable(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "1", 1)) {
		pdata->afc_disable = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		pdata->afc_disable = false;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s afc_disable(%d)\n", MUIC_DEV_NAME, __func__, pdata->afc_disable);

	return count;
}
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

static DEVICE_ATTR(uart_sel, 0664, max77833_muic_show_uart_sel,
		max77833_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664, max77833_muic_show_usb_sel,
		max77833_muic_set_usb_sel);
static DEVICE_ATTR(uart_en, 0660, max77833_muic_show_uart_en,
		max77833_muic_set_uart_en);
static DEVICE_ATTR(adc, S_IRUGO, max77833_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, S_IRUGO, max77833_muic_show_usb_state, NULL);
static DEVICE_ATTR(attached_dev, S_IRUGO, max77833_muic_show_attached_dev, NULL);
#if defined(CONFIG_MUIC_MAX77833_SUPPROT_AUDIO_LINE_OUT_CONTROL)
static DEVICE_ATTR(audio_path, 0664,
		max77833_muic_show_audio_path, max77833_muic_set_audio_path);
#endif /* CONFIG_MUIC_MAX77833_SUPPROT_AUDIO_LINE_OUT_CONTROL */
static DEVICE_ATTR(otg_test, 0664,
		max77833_muic_show_otg_test, max77833_muic_set_otg_test);
static DEVICE_ATTR(apo_factory, 0664,
		max77833_muic_show_apo_factory, max77833_muic_set_apo_factory);
#if defined(CONFIG_MUIC_MAX77833_IGNORE_ADCERR_WA) && defined(CONFIG_SEC_FACTORY)
static DEVICE_ATTR(ignore_adcerr, 0664,
		max77833_muic_show_ignore_adcerr, max77833_muic_set_ignore_adcerr);
#endif /* CONFIG_MUIC_MAX77833_IGNORE_ADCERR_WA && CONFIG_SEC_FACTORY */
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
static DEVICE_ATTR(afc_disable, 0664,
		max77833_muic_show_afc_disable, max77833_muic_set_afc_disable);
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

static struct attribute *max77833_muic_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_en.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_attached_dev.attr,
#if defined(CONFIG_MUIC_MAX77833_SUPPROT_AUDIO_LINE_OUT_CONTROL)
	&dev_attr_audio_path.attr,
#endif /* CONFIG_MUIC_MAX77833_SUPPROT_AUDIO_LINE_OUT_CONTROL */
	&dev_attr_otg_test.attr,
	&dev_attr_apo_factory.attr,
#if defined(CONFIG_MUIC_MAX77833_IGNORE_ADCERR_WA) && defined(CONFIG_SEC_FACTORY)
	&dev_attr_ignore_adcerr.attr,
#endif /* CONFIG_MUIC_MAX77833_IGNORE_ADCERR_WA && CONFIG_SEC_FACTORY */
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	&dev_attr_afc_disable.attr,
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */
	NULL
};

static const struct attribute_group max77833_muic_group = {
	.attrs = max77833_muic_attributes,
};

void max77833_muic_read_register(struct i2c_client *i2c)
{
	const enum max77833_muic_reg regfile[] = {
		MAX77833_MUIC_REG_ID,
		MAX77833_MUIC_REG_STATUS1,
		MAX77833_MUIC_REG_STATUS2,
		MAX77833_MUIC_REG_STATUS3,
		MAX77833_MUIC_REG_INTMASK1,
		MAX77833_MUIC_REG_INTMASK2,
		MAX77833_MUIC_REG_INTMASK3,
		MAX77833_MUIC_REG_CDETCTRL1,
		MAX77833_MUIC_REG_CDETCTRL2,
		MAX77833_MUIC_REG_CTRL1,
		MAX77833_MUIC_REG_CTRL2,
		MAX77833_MUIC_REG_CTRL3,
		MAX77833_MUIC_REG_CTRL4,
		MAX77833_MUIC_REG_HVCONTROL1,
		MAX77833_MUIC_REG_HVCONTROL2,
	};
	u8 val;
	int i;

	pr_info("%s:%s read register--------------\n", MUIC_DEV_NAME, __func__);
	for (i = 0; i < ARRAY_SIZE(regfile); i++) {
		int ret = 0;
		ret = max77833_read_reg(i2c, regfile[i], &val);
		if (ret) {
			pr_err("%s:%s fail to read muic reg(0x%02x), ret=%d\n",
					MUIC_DEV_NAME, __func__, regfile[i], ret);
			continue;
		}

		pr_info("%s:%s reg(0x%02x)=[0x%02x]\n", MUIC_DEV_NAME, __func__,
				regfile[i], val);
	}
	pr_info("%s:%s end register---------------\n", MUIC_DEV_NAME, __func__);
}

static bool max77833_muic_check_dev_factory_charging(struct max77833_muic_data *muic_data)
{
	u8 adc = (muic_data->status1) & STATUS1_ADC_MASK;
	u8 vbvolt = (muic_data->status2) & STATUS2_VBVOLT_MASK;

	switch (adc) {
	case ADC_JIG_USB_OFF:
		if (vbvolt == VB_HIGH)
			return true;
		break;
	default:
		break;
	}

	return false;
}

static int max77833_muic_set_adcmode(struct max77833_muic_data *muic_data,
		u8 val)
{
#if 0
	struct i2c_client *i2c = muic_data->i2c;
	u8 adc_val = (val << MODE_SHIFT);
	u8 response, reg;
	int ret = 0;

	reg = MAX77833_MUIC_REG_DAT_IN_OP;
	ret = max77833_write_reg(i2c, reg, COMMAND_ID_MONITOR_CONFIG_WRITE);
	if (ret)
		pr_err("%s:%s fail to write reg[0x%02x], ret(%d)\n",
				MUIC_DEV_NAME, __func__, reg, ret);

	reg = MAX77833_MUIC_REG_DAT_IN1;
	ret = max77833_write_reg(i2c, reg, adc_val);
	if (ret)
		pr_err("%s:%s fail to read reg[0x%02x], ret(%d)\n",
				MUIC_DEV_NAME, __func__, reg, ret);

	reg = MAX77833_MUIC_REG_DAT_IN8;
	ret = max77833_write_reg(i2c, reg, 0x00);
	if (ret)
		pr_err("%s:%s fail to read reg[0x%02x], ret(%d)\n",
				MUIC_DEV_NAME, __func__, reg, ret);

	mdelay(10);

	reg = MAX77833_MUIC_REG_DAT_OUT_OP;
	ret = max77833_read_reg(i2c, reg, &response);
	if (ret)
		pr_err("%s:%s fail to read reg[0x%02x], ret(%d)\n",
				MUIC_DEV_NAME, __func__, reg, ret);
	else
		pr_info("%s:%s reg(0x%02x) = 0x%02x\n",
				MUIC_DEV_NAME, __func__, reg, response);

	return ret;
#endif
	return 0;
}

static void max77833_muic_adcmode_switch(struct max77833_muic_data *muic_data,
		const u8 val)
{
	const char *name;
	int ret = 0;

	switch (val) {
	case MAX77833_MUIC_IDMODE_CONTINUOUS:
		name = "Continuous";
		break;
	case MAX77833_MUIC_IDMODE_FACTORY_ONE_SHOT:
		name = "Factory One Shot";
		break;
	case MAX77833_MUIC_IDMODE_ONE_SHOT:
		name = "One Shot";
		break;
	case MAX77833_MUIC_IDMODE_PULSE:
		name = "Pulse";
		break;
	default:
		pr_warn("%s:%s wrong ADCMode val[0x%02x]\n", MUIC_DEV_NAME,
				__func__, val);
		return;
	}

	ret = max77833_muic_set_adcmode(muic_data, val);
	if (ret) {
		pr_err("%s:%s fail to adcmode change to %s ret:%d\n",
				MUIC_DEV_NAME, __func__, name, ret);
		return;
	}
}

void max77833_muic_set_idmode_continuous(struct max77833_muic_data *muic_data)
{
	const u8 val = MAX77833_MUIC_IDMODE_CONTINUOUS;

	max77833_muic_adcmode_switch(muic_data, val);

	return;
}

#if !defined(CONFIG_SEC_FACTORY)
static void max77833_muic_set_idmode_factory_oneshot(struct max77833_muic_data *muic_data)
{
	const u8 val = MAX77833_MUIC_IDMODE_FACTORY_ONE_SHOT;

	max77833_muic_adcmode_switch(muic_data, val);

	return;
}

void max77833_muic_set_idmode_oneshot(struct max77833_muic_data *muic_data)
{
	const u8 val = MAX77833_MUIC_IDMODE_ONE_SHOT;

	max77833_muic_adcmode_switch(muic_data, val);

	return;
}
#endif /* !CONFIG_SEC_FACTORY */

static int max77833_muic_attach_uart_path(struct max77833_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->uart_path == MUIC_PATH_UART_AP) {
		ret = switch_to_ap_uart(muic_data, new_dev);
	} else if (pdata->uart_path == MUIC_PATH_UART_CP) {
		ret = switch_to_cp_uart(muic_data, new_dev);
	}
	else
		pr_warn("%s:%s invalid uart_path\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int max77833_muic_attach_usb_path(struct max77833_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s usb_path=%d\n", MUIC_DEV_NAME, __func__, pdata->usb_path);

	if (pdata->usb_path == MUIC_PATH_USB_AP) {
		com_to_usb_ap(muic_data);
	}
	else if (pdata->usb_path == MUIC_PATH_USB_CP) {
		com_to_usb_cp(muic_data);
	}
	else
		pr_warn("%s:%s invalid usb_path\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static muic_attached_dev_t check_jig_uart_on_factory_test
			(struct max77833_muic_data *muic_data,	muic_attached_dev_t new_dev)
{
	muic_attached_dev_t ret_ndev;

	if (muic_data->is_factory_start &&
			muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) {
			ret_ndev = ATTACHED_DEV_JIG_UART_ON_MUIC;
	} else
		ret_ndev = ATTACHED_DEV_JIG_UART_OFF_MUIC;

	pr_info("%s:%s is_factory_start = %c\n", MUIC_DEV_NAME, __func__,
			(muic_data->is_factory_start) ? 'T' : 'F');

	return ret_ndev;
}

static int max77833_muic_handle_detach(struct max77833_muic_data *muic_data)
{
	int ret = 0;
	bool noti = true;
	bool logically_noti = false;

	if (muic_data->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s:%s Duplicated(%d), just ignore\n", MUIC_DEV_NAME,
				__func__, muic_data->attached_dev);
		goto out_without_noti;
	}

	/* Enable Factory Accessory Detection State Machine */
	max77833_muic_enable_accdet(muic_data);

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	max77833_muic_set_afc_ready(muic_data, false);
	muic_data->is_afc_muic_prepare = false;

	cancel_delayed_work_sync(&muic_data->hv_muic_qc_vb_work);
#endif

	muic_lookup_vps_table(muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
		/* Enable Charger Detection */
		max77833_muic_enable_chgdet(muic_data);
		break;
	case ATTACHED_DEV_UNOFFICIAL_ID_MUIC:
		goto out_without_noti;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
		goto out_without_noti;
	case ATTACHED_DEV_SMARTDOCK_VB_MUIC:
		noti = false;
		logically_noti = true;
		break;
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		logically_noti = true;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
		break;
	default:
		break;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti)
		muic_notifier_detach_attached_dev(muic_data->attached_dev);

	if (logically_noti)
		muic_notifier_logically_detach_attached_dev(ATTACHED_DEV_SMARTDOCK_VB_MUIC);
#endif /* CONFIG_MUIC_NOTIFIER */

out_without_noti:
	com_to_open(muic_data);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int max77833_muic_logically_detach(struct max77833_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	bool noti = true;
	bool logically_notify = false;
	bool force_path_open = true;
	bool enable_accdet = true;
	int ret = 0;

	if (max77833_muic_check_dev_factory_charging(muic_data))
		enable_accdet = false;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_MHL_MUIC:
	case ATTACHED_DEV_AUDIODOCK_MUIC:
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
		break;
	case ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC:
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		break;
	case ATTACHED_DEV_UNOFFICIAL_ID_MUIC:
		goto out;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC ||
			new_dev == ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC ||
			new_dev == ATTACHED_DEV_JIG_UART_ON_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_UNKNOWN_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (new_dev == ATTACHED_DEV_DESKDOCK_VB_MUIC)
			goto out;
		break;
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		if (new_dev == ATTACHED_DEV_DESKDOCK_MUIC)
			goto out;
		break;
	case ATTACHED_DEV_SMARTDOCK_VB_MUIC:
		if ((new_dev == ATTACHED_DEV_SMARTDOCK_USB_MUIC) ||
				(new_dev == ATTACHED_DEV_SMARTDOCK_TA_MUIC))
			goto out;

		logically_notify = true;
		break;
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
		if (new_dev != ATTACHED_DEV_SMARTDOCK_VB_MUIC)
			logically_notify = true;
		break;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
		goto out;
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		noti = max77833_muic_check_change_dev_afc_charger(muic_data, new_dev);
		if (noti) {
			max77833_muic_set_afc_ready(muic_data, false);
			muic_data->is_afc_muic_prepare = false;
			max77833_hv_muic_reset_hvcontrol_reg(muic_data);
			cancel_delayed_work_sync(&muic_data->hv_muic_qc_vb_work);
		}
#endif
		break;
	case ATTACHED_DEV_NONE_MUIC:
		force_path_open = false;
		goto out;
	default:
		pr_warn("%s:%s try to attach without logically detach\n",
				MUIC_DEV_NAME, __func__);
		goto out;
	}

	pr_info("%s:%s attached(%d)!=new(%d), assume detach\n", MUIC_DEV_NAME,
			__func__, muic_data->attached_dev, new_dev);

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti)
		muic_notifier_detach_attached_dev(muic_data->attached_dev);

	if (logically_notify)
		muic_notifier_logically_detach_attached_dev(ATTACHED_DEV_SMARTDOCK_VB_MUIC);
#endif /* CONFIG_MUIC_NOTIFIER */

out:
	if (enable_accdet)
		max77833_muic_enable_accdet(muic_data);

	if (force_path_open)
		com_to_open(muic_data);

	return ret;
}

static int max77833_muic_handle_attach(struct max77833_muic_data *muic_data,
		muic_attached_dev_t new_dev)
{
	bool logically_notify = false;
	bool noti_smartdock = false;
	int ret = 0;

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	if (muic_data->attached_dev == ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC ||
		muic_data->attached_dev == ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC ||
		muic_data->attached_dev == ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC)
		return ret;
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

	if (new_dev == muic_data->attached_dev) {
		if (muic_data->attached_dev != ATTACHED_DEV_AFC_CHARGER_5V_MUIC ||
			muic_data->is_mrxrdy != true) {
			pr_info("%s:%s Duplicated(%d), mrxrdy(%c), just ignore \n",
					MUIC_DEV_NAME, __func__, muic_data->attached_dev,
					muic_data->is_mrxrdy ? 'T' : 'F');
			return ret;
		}
	}

	ret = max77833_muic_logically_detach(muic_data, new_dev);
	if (ret)
		pr_warn("%s:%s fail to logically detach(%d)\n", MUIC_DEV_NAME,
				__func__, ret);

	switch (new_dev) {
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		/* Disable Charger Detection */
		max77833_muic_disable_chgdet(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		ret = max77833_muic_attach_uart_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		new_dev = check_jig_uart_on_factory_test(muic_data, new_dev);
		if (new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC)
			goto out;
		break;
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
		if (muic_data->pdata->afc_disable)
			pr_info("%s:%s AFC Disable(%d) by USER!\n", MUIC_DEV_NAME,
				__func__, muic_data->pdata->afc_disable);
		else {
			if (muic_data->is_afc_muic_ready == false)
				max77833_muic_prepare_afc_charger(muic_data);
		}
#endif
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = max77833_muic_attach_usb_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_UNOFFICIAL_ID_MUIC:
		max77833_muic_disable_accdet(muic_data);
		goto out_without_noti;
	case ATTACHED_DEV_MHL_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (muic_data->attached_dev == ATTACHED_DEV_DESKDOCK_VB_MUIC)
			logically_notify = true;
		break;
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		if (muic_data->attached_dev == ATTACHED_DEV_DESKDOCK_MUIC)
			logically_notify = true;
		break;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		goto out_without_noti;
	case ATTACHED_DEV_SMARTDOCK_VB_MUIC:
		logically_notify = true;
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		if (muic_data->attached_dev != ATTACHED_DEV_SMARTDOCK_VB_MUIC)
			noti_smartdock = true;
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		write_muic_ctrl_reg(muic_data, MAX77833_MUIC_REG_DAT_IN1,
					CTRL1_OPEN);
		break;
	default:
		pr_warn("%s:%s unsupported dev(%d)\n", MUIC_DEV_NAME, __func__,
				new_dev);
		ret = -ENODEV;
		goto out;
	}

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	new_dev = hv_muic_check_id_err(muic_data, new_dev);
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti_smartdock)
		muic_notifier_logically_attach_attached_dev(ATTACHED_DEV_SMARTDOCK_VB_MUIC);

	if (logically_notify)
		muic_notifier_logically_attach_attached_dev(new_dev);
	else
		muic_notifier_attach_attached_dev(new_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

out_without_noti:
	muic_data->attached_dev = new_dev;
out:
	return ret;
}

static bool muic_check_vps_adc
			(const struct max77833_muic_vps_data *tmp_vps, u8 adc)
{
	bool ret = false;

	if (tmp_vps->adc == adc) {
		ret = true;
		goto out;
	}

	if (tmp_vps->adc == ADC_219) {
		switch(adc) {
		case ADC_CEA936ATYPE1_CHG:
		case ADC_JIG_USB_OFF:
			ret = true;
			goto out;
			break;
		default:
			break;
		}
	}

	if (tmp_vps->adc == ADC_UNDEFINED) {
		switch(adc) {
		case ADC_SEND_END ... ADC_REMOTE_S12:
		case ADC_UART_CABLE:
		case ADC_AUDIOMODE_W_REMOTE:
			ret = true;
			goto out;
			break;
		default:
			break;
		}
	}

/*
	if (tmp_vps->adc == ADC_JIG_UART_OFF_WA) {
		switch(adc) {
		case ADC_DESKDOCK:
		case ADC_CEA936ATYPE2_CHG:
		case ADC_JIG_UART_ON:
			ret = true;
			goto out;
			break;
		default:
			break;
		}
	}
*/

	if (tmp_vps->adc == ADC_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_info("%s:%s vps(%s) adc(0x%02x) ret(%c)\n",
				MUIC_DEV_NAME, __func__, tmp_vps->vps_name,
				adc, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_vps_chgdetrun
			(const struct max77833_muic_vps_data *tmp_vps, u8 chgdetrun)
{
	bool ret = false;

	if (tmp_vps->chgdetrun == chgdetrun) {
		ret = true;
		goto out;
	}

	if (tmp_vps->chgdetrun == CHGDETRUN_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_info("%s:%s vps(%s) chgdetrun(0x%02x) ret(%c)\n",
				MUIC_DEV_NAME, __func__, tmp_vps->vps_name,
				chgdetrun, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_vps_chgtyp
			(const struct max77833_muic_vps_data *tmp_vps, u8 chgtyp)
{
	bool ret = false;

	if (tmp_vps->chgtyp == chgtyp) {
		ret = true;
		goto out;
	}

	if (tmp_vps->chgtyp == CHGTYP_UNOFFICIAL_CHARGER) {
		switch (chgtyp) {
		case CHGTYP_500MA:
		case CHGTYP_1A:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_vps->chgtyp == CHGTYP_ANY) {
		switch (chgtyp) {
		case CHGTYP_USB:
		case CHGTYP_CDP:
		case CHGTYP_DEDICATED_CHARGER:
		case CHGTYP_500MA:
		case CHGTYP_1A:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_vps->chgtyp == CHGTYP_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_info("%s:%s vps(%s) chgtyp(0x%02x) ret(%c)\n",
				MUIC_DEV_NAME, __func__, tmp_vps->vps_name,
				chgtyp, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_otg_test(struct max77833_muic_data *muic_data)
{
	bool ret = false;

	if (muic_data->is_otg_test) {
		if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC))
			ret = true;
		else
			pr_info("%s:%s Not support 'OTG'\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s [%c]\n", MUIC_DEV_NAME, __func__, ret ? 'T':'F');

	return ret;
}

static muic_attached_dev_t muic_get_new_dev
	(struct max77833_muic_data *muic_data, muic_attached_dev_t new_dev)
{
	muic_attached_dev_t tmp_dev = new_dev;

	if (new_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC) {
		if(muic_check_otg_test(muic_data))
			tmp_dev = ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC;
		else
			tmp_dev = ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC;

		pr_info("%s:%s new_dev(%d), is_otg_test[%c]\n", MUIC_DEV_NAME,
			__func__, tmp_dev, muic_data->is_otg_test ? 'T' : 'F');
	}

	return tmp_dev;
}

#if !defined(CONFIG_SEC_FACTORY)
static bool is_need_muic_idmode_continuous(muic_attached_dev_t new_dev)
{
	bool ret = false;

	switch (new_dev) {
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_SMARTDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		ret = true;
		break;
	default:
		break;
	}

	pr_info("%s:%s (%d)%c\n", MUIC_DEV_NAME, __func__, new_dev,
			ret ? 'T' : 'F');

	return ret;
}

static bool is_need_muic_idmode_factory_oneshot(struct max77833_muic_data *muic_data)
{
	u8 adc = (muic_data->status1) & STATUS1_IDRES_MASK;
	bool ret = false;

//	if (adc == ADC_JIG_UART_OFF)
//		ret = true;

	pr_info("%s:%s id(0x%02x)%c\n", MUIC_DEV_NAME, __func__, adc,
			ret ? 'T' : 'F');

	return ret;
}
#endif /* !CONFIG_SEC_FACTORY */

static void max77833_muic_detect_dev(struct max77833_muic_data *muic_data, int irq)
{
	struct i2c_client *i2c = muic_data->i2c;
	const struct max77833_muic_vps_data *tmp_vps;
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	u8 status[3];
	u8 hvcontrol[2];
	u8 adc1k, adcerr, adc, vbvolt, chgdetrun, chgtyp;
	u8 vdnmon, dpdnvden, mpnack, vbadc;
	int ret;
	int i;

	ret = max77833_bulk_read(i2c, MAX77833_MUIC_REG_STATUS1, 3, status);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
		return;
	}
	pr_info("%s:%s STATUS1:0x%02x, 2:0x%02x, 3:0x%02x\n", MUIC_DEV_NAME,
			__func__, status[0], status[1], status[2]);

	ret = max77833_bulk_read(i2c, MAX77833_MUIC_REG_HVCONTROL1, 2, hvcontrol);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
		return;
	}

	pr_info("%s:%s HVCONTROL1:0x%02x, 2:0x%02x\n", MUIC_DEV_NAME, __func__,
		hvcontrol[0], hvcontrol[1]);


	/* attached status */
	muic_data->status1 = status[0];
	muic_data->status2 = status[1];

	adc = status[0] & STATUS1_IDRES_MASK;
	if(adc >= 0x10) adc-= 0x10;
	chgdetrun = status[1] & STATUS2_CHGTYPRUN_MASK;
	chgtyp = status[1] & STATUS2_CHGTYP_MASK;

	mpnack = status[2] & STATUS3_MPNACK_MASK;
	vdnmon = status[2] & STATUS3_VDNMON_MASK;
	vbadc = status[2] & STATUS3_VBADC_MASK;
	dpdnvden = hvcontrol[0] & HVCONTROL1_DPDNVDEN_MASK;

	if (irq == muic_data->irq_mrxrdy)
		muic_data->is_mrxrdy = true;
	else
		muic_data->is_mrxrdy = false;

	pr_info("%s:%s adc1k:0x%x adcerr:0x%x[%c] adc:0x%x vb:0x%x chgdetrun:0x%x"
		" chgtyp:0x%x\n", MUIC_DEV_NAME, __func__, adc1k,
		adcerr, (muic_data->ignore_adcerr ? 'T' : 'F'),
		adc, vbvolt, chgdetrun, chgtyp);

	pr_info("%s:%s vdnmon:0x%x mpnack:0x%x vbadc:0x%x dpdnvden:0x%x\n",
		MUIC_DEV_NAME, __func__, vdnmon, mpnack, vbadc, dpdnvden);

	/* Workaround for Factory mode.
	 * Abandon adc interrupt of approximately +-100K range
	 * if previous cable status was JIG UART BOOT OFF.
	 */
	if (muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) {
		if ((adc == (ADC_JIG_UART_OFF + 1)) ||
				(adc == (ADC_JIG_UART_OFF - 1))) {
			if (!muic_data->is_factory_start || adc != ADC_JIG_UART_ON) {
				pr_warn("%s:%s abandon ADC\n", MUIC_DEV_NAME, __func__);
				return;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		tmp_vps = &(muic_vps_table[i]);

		if (!(muic_check_vps_adc(tmp_vps, adc)))
			continue;

		if (!(muic_check_vps_chgdetrun(tmp_vps, chgdetrun)))
			continue;

		if (!(muic_check_vps_chgtyp(tmp_vps, chgtyp)))
			continue;

		if (!(muic_check_support_dev(muic_data, tmp_vps->attached_dev))) {
			if (vbvolt == VB_HIGH) {
				new_dev = ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC;
				intr = MUIC_INTR_ATTACH;
				pr_info("%s:%s unsupported ID + VB\n", MUIC_DEV_NAME, __func__);
			}
			break;
		}

		pr_info("%s:%s vps table match found at i(%d), %s\n",
				MUIC_DEV_NAME, __func__, i, tmp_vps->vps_name);

		new_dev = tmp_vps->attached_dev;

		/* check a muic device type - JIG_UART_OFF_VB */
		new_dev = muic_get_new_dev(muic_data, new_dev);

		intr = MUIC_INTR_ATTACH;
		break;
	}

	pr_info("%s:%s %d->%d\n", MUIC_DEV_NAME, __func__, muic_data->attached_dev,
							new_dev);

#if !defined(CONFIG_SEC_FACTORY)
	if (is_need_muic_idmode_continuous(new_dev)) {
		/* ADC Mode switch to the Continuous Mode */
		max77833_muic_set_idmode_continuous(muic_data);
	} else if (is_need_muic_idmode_factory_oneshot(muic_data)) {
		/* ID Mode switch to the Factory One Shot Mode */
		max77833_muic_set_idmode_factory_oneshot(muic_data);
	} else {
		/* ADC Mode restore to the One Shot Mode */
		max77833_muic_set_idmode_oneshot(muic_data);
	}
#endif /* CONFIG_SEC_FACTORY */

	if (intr == MUIC_INTR_ATTACH) {
		pr_info("%s:%s ATTACHED\n", MUIC_DEV_NAME, __func__);
		ret = max77833_muic_handle_attach(muic_data, new_dev);
		if (ret)
			pr_err("%s:%s cannot handle attach(%d)\n", MUIC_DEV_NAME,
								__func__, ret);
	} else {
		pr_info("%s:%s DETACHED\n", MUIC_DEV_NAME, __func__);
		ret = max77833_muic_handle_detach(muic_data);
		if (ret)
			pr_err("%s:%s cannot handle detach(%d)\n", MUIC_DEV_NAME,
								__func__, ret);
	}

	return;
}

static u8 read_muic_response(struct max77833_muic_data *muic_data, u8 reg)
{
	u8 response;

	max77833_read_reg(muic_data->i2c, reg, &response);
	pr_info("%s:%s REG[0x%02x]:0x%02x\n", MUIC_DEV_NAME, __func__, reg, response);

	return response;
}

static void max77833_muic_handle_cmd(struct max77833_muic_data *muic_data, int irq)
{
	struct max77833_muic_command *muic_command = muic_data->muic_command;
	u8 opcode = read_muic_response(muic_data, MAX77833_MUIC_REG_DAT_OUT_OP);
	u8 data;

	mutex_lock(&muic_command->command_mutex);
	if (muic_command->state == STATE_CMD_RESPONSE ||
		muic_command->state == STATE_CMD_NONE) {
		mutex_unlock(&muic_command->command_mutex);
		pr_err("%s:%s Not transmit(%d)\n", MUIC_DEV_NAME, __func__,
			muic_command->state);
		return;
	}
	mutex_unlock(&muic_command->command_mutex);

	if (muic_command->opcode != opcode) {
		pr_err("%s:%s Not correct opcode - 0x%02x!=0x%02x\n",
				MUIC_DEV_NAME, __func__,
				muic_command->opcode, opcode);
		muic_command_reset(muic_data);
		return;
	}

	switch (opcode) {
	case COMMAND_CONFIG_READ:
		break;
	case COMMAND_CONFIG_WRITE:
		break;
	case COMMAND_SWITCH_READ:
		data = read_muic_response(muic_data, MAX77833_MUIC_REG_DAT_OUT1);
		pr_info("%s:%s opcode(0x%02x):data(0x%02x)\n",
					MUIC_DEV_NAME, __func__, opcode, data);

		mutex_lock(&muic_command->command_mutex);
		if (muic_command->state == STATE_CMD_READ_TRANS_READY_W) {
			muic_command->rw_before_val = data;
			mutex_unlock(&muic_command->command_mutex);
			write_muic_rw_write_command(muic_data);
		} else
			mutex_unlock(&muic_command->command_mutex);

		break;
	case COMMAND_SWITCH_WRITE:
		pr_info("%s:%s Write command, opcode(0x%02x)\n",
					MUIC_DEV_NAME, __func__, opcode);
		break;
	case COMMAND_SYSTEM_MSG_READ:
		break;
	case COMMAND_CHGDET_CONFIG_WRITE:
		break;
	case COMMAND_ID_MONITOR_CONFIG_READ:
		break;
	case COMMAND_ID_MONITOR_CONFIG_WRITE:
		break;
	default :
		pr_err("%s:%s Not correct response\n", MUIC_DEV_NAME, __func__);
		break;
	}

	mutex_lock(&muic_command->command_mutex);
	if (muic_command->state != STATE_CMD_READ_TRANS_READY_W)
		muic_command->state = STATE_CMD_RESPONSE;
	mutex_unlock(&muic_command->command_mutex);
}

#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
inline static bool is_muic_check_reset(struct max77833_muic_data *muic_data)
{
	bool ret;

	mutex_lock(&muic_data->reset_mutex);
	if (muic_data->is_muic_reset)
		ret = true;
	else
		ret = false;
	mutex_unlock(&muic_data->reset_mutex);

	return ret;
}

inline static void set_muic_reset(struct max77833_muic_data *muic_data, bool val)
{
	mutex_lock(&muic_data->reset_mutex);
	muic_data->is_muic_reset = val;
	mutex_unlock(&muic_data->reset_mutex);
}
#endif

static irqreturn_t max77833_muic_irq(int irq, void *data)
{
	struct max77833_muic_data *muic_data = data;
	pr_info("%s:%s irq:%d\n", MUIC_DEV_NAME, __func__, irq);

#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
	if (is_muic_check_reset(muic_data)) {
		pr_info("%s:%s is_muic_reset is true, just return\n",
			MFD_DEV_NAME, __func__);
		return IRQ_HANDLED;
	}
#endif

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
		max77833_muic_detect_dev(muic_data, irq);
	else
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static irqreturn_t max77833_muic_irq_cmd(int irq, void *data)
{
	struct max77833_muic_data *muic_data = data;
	pr_info("%s:%s irq:%d\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
		max77833_muic_handle_cmd(muic_data, irq);
	else
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

#define REQUEST_IRQ(_irq, _dev_id, _name)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, max77833_muic_irq,	\
				IRQF_NO_SUSPEND, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
				MUIC_DEV_NAME, __func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

#define REQUEST_IRQ_CMD(_irq, _dev_id, _name)				\
	do {									\
		ret = request_threaded_irq(_irq, NULL, max77833_muic_irq_cmd,	\
					IRQF_NO_SUSPEND, _name, _dev_id);	\
		if (ret < 0) {							\
			pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
					MUIC_DEV_NAME, __func__, _irq, ret);	\
			_irq = 0;						\
		}								\
	} while (0)

static int max77833_muic_irq_init(struct max77833_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->mfd_pdata && (muic_data->mfd_pdata->irq_base > 0)) {
		int irq_base = muic_data->mfd_pdata->irq_base;

		/* request MUIC IRQ */
		muic_data->irq_idreg = irq_base + MAX77833_MUIC_IRQ_INT3_IDRESINT;
		REQUEST_IRQ(muic_data->irq_idreg, muic_data, "muic-idreg");

		muic_data->irq_chgtyp = irq_base + MAX77833_MUIC_IRQ_INT3_CHGTYPINT;
		REQUEST_IRQ(muic_data->irq_chgtyp, muic_data, "muic-chgtyp");

		muic_data->irq_sysmsg = irq_base + MAX77833_MUIC_IRQ_INT3_UICSYSMSGINT;
		REQUEST_IRQ(muic_data->irq_sysmsg, muic_data, "muic-sysmsg");

		muic_data->irq_apcmdres = irq_base + MAX77833_MUIC_IRQ_INT3_APCMDRESPINT;
		REQUEST_IRQ_CMD(muic_data->irq_apcmdres, muic_data, "muic-apcmdres");
	}

	pr_info("%s:%s idreg(%d), chgtyp(%d), sysmsg(%d), apcmdres(%d)\n",
			MUIC_DEV_NAME, __func__,
			muic_data->irq_idreg, muic_data->irq_chgtyp,
			muic_data->irq_sysmsg, muic_data->irq_apcmdres);
	return ret;
}

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s:%s IRQ(%d):%s free done\n", MUIC_DEV_NAME,	\
				__func__, _irq, _name);			\
	}								\
} while (0)

static void max77833_muic_free_irqs(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* free MUIC IRQ */
	FREE_IRQ(muic_data->irq_idreg, muic_data, "muic-idreg");
	FREE_IRQ(muic_data->irq_chgtyp, muic_data, "muic-chgtyp");
	FREE_IRQ(muic_data->irq_sysmsg, muic_data, "muic-sysmsg");
	FREE_IRQ(muic_data->irq_apcmdres, muic_data, "muic-apcmdres");
}

#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
static void max77833_muic_free_reset_irq(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* free MUIC RESET IRQ */
	FREE_IRQ(muic_data->irq_reset_acokbf, muic_data, "muic-reset-acokbf");
}
#endif /* CONFIG_MUIC_MAX77833_RESET_WA */

#define CHECK_GPIO(_gpio, _name)					\
do {									\
	if (!_gpio) {							\
		pr_err("%s:%s " _name " GPIO defined as 0 !\n",		\
				MUIC_DEV_NAME, __func__);		\
		WARN_ON(!_gpio);					\
		ret = -EIO;						\
		goto err_kfree;						\
	}								\
} while (0)

static void max77833_muic_init_detect(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);
	muic_data->is_muic_ready = true;

	max77833_muic_detect_dev(muic_data, -1);

	mutex_unlock(&muic_data->muic_mutex);
}

#if defined(CONFIG_OF)
static int of_max77833_muic_dt(struct max77833_muic_data *muic_data)
{
	struct device_node *np_muic;
	const char *prop_support_list;
	int i, j, prop_num;
	int ret = 0;

	np_muic = of_find_node_by_path("/muic");
	if (np_muic == NULL)
		return -EINVAL;

	prop_num = of_property_count_strings(np_muic, "muic,support-list");
	if (prop_num < 0) {
		pr_warn("%s:%s Cannot parse 'muic support list dt node'[%d]\n",
				MUIC_DEV_NAME, __func__, prop_num);
		ret = prop_num;
		goto err;
	}

	/* for debug */
	for (i = 0; i < prop_num; i++) {
		ret = of_property_read_string_index(np_muic, "muic,support-list", i,
							&prop_support_list);
		if (ret) {
			pr_err("%s:%s Cannot find string at [%d], ret[%d]\n",
					MUIC_DEV_NAME, __func__, i, ret);
			goto err;
		}

		pr_debug("%s:%s prop_support_list[%d] is %s\n", MUIC_DEV_NAME, __func__,
				i, prop_support_list);

		for (j = 0; j < ARRAY_SIZE(muic_vps_table); j++) {
			if (!strcmp(muic_vps_table[j].vps_name, prop_support_list)) {
				muic_data->muic_support_list[(muic_vps_table[j].attached_dev)] = true;
				break;
			}
		}
	}

	/* for debug */
	for (i = 0; i < ATTACHED_DEV_NUM; i++) {
		pr_debug("%s:%s pmuic_support_list[%d] = %c\n", MUIC_DEV_NAME, __func__,
				i, (muic_data->muic_support_list[i] ? 'T' : 'F'));
	}

	ret = of_property_read_u8(np_muic, "muic,afcmode-tx", &muic_data->tx_data);
	if (ret) {
		pr_err("%s:%s There is no Property of muic,afcmode-tx\n",
				MUIC_DEV_NAME, __func__);
		return -EINVAL;
	}

	pr_info("%s:%s muic_data->tx_data:0x%02x\n", MUIC_DEV_NAME, __func__,
				muic_data->tx_data);

err:
	of_node_put(np_muic);

	return ret;
}
#endif /* CONFIG_OF */

static void max77833_muic_clear_interrupt(struct max77833_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 interrupt1, interrupt2, interrupt3;
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = max77833_read_reg(i2c, MAX77833_MUIC_REG_INT1, &interrupt1);
	if (ret)
		pr_err("%s:%s fail to read muic INT1 reg(%d)\n", MUIC_DEV_NAME, __func__, ret);

	ret = max77833_read_reg(i2c, MAX77833_MUIC_REG_INT2, &interrupt2);
	if (ret)
		pr_err("%s:%s fail to read muic INT2 reg(%d)\n", MUIC_DEV_NAME, __func__, ret);

	ret = max77833_read_reg(i2c, MAX77833_MUIC_REG_INT3, &interrupt3);
	if (ret)
		pr_err("%s:%s fail to read muic INT3 reg(%d)\n", MUIC_DEV_NAME, __func__, ret);

	pr_info("%s:%s CLEAR!! INT1:0x%02x, 2:0x%02x, 3:0x%02x\n", MUIC_DEV_NAME,
				__func__, interrupt1, interrupt2, interrupt3);
}

static int max77833_muic_init_regs(struct max77833_muic_data *muic_data)
{
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	max77833_muic_clear_interrupt(muic_data);

	/* Set ADC debounce time: 25ms */
	max77833_muic_set_adcdbset(muic_data, 2);

#if defined(CONFIG_SEC_FACTORY)
	/* ADC Mode switch to the Continuous Mode */
//	max77833_muic_set_idmode_continuous(muic_data);
#endif /* CONFIG_SEC_FACTORY */

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	ret = max77833_afc_muic_irq_init(muic_data);
	if (ret < 0) {
		pr_err("%s:%s Failed to initialize HV MUIC irq:%d\n", MUIC_DEV_NAME,
				__func__, ret);
		max77833_hv_muic_free_irqs(muic_data);
	}
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

	ret = max77833_muic_irq_init(muic_data);
	if (ret < 0) {
		pr_err("%s:%s Failed to initialize MUIC irq:%d\n", MUIC_DEV_NAME,
				__func__, ret);
		max77833_muic_free_irqs(muic_data);
	}

	return ret;
}

#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
static irqreturn_t muic_reset_irq_thread(int irq, void* data)
{
	struct max77833_muic_data *muic_data = data;
	u8 muic_irq_mask[3] = {};
	u8 reset_val = 0x0;

	mutex_lock(&muic_data->muic_mutex);

	pr_info("%s:%s irq:%d\n", MUIC_DEV_NAME, __func__, irq);

	/* MUIC INT1 ~ INT3 MASK register check */
	max77833_bulk_read(muic_data->i2c, MAX77833_MUIC_REG_INTMASK1, 3,
				muic_irq_mask);

	if ((reset_val == muic_irq_mask[0])
		&& (reset_val == muic_irq_mask[1])
		&& (reset_val == muic_irq_mask[2])) {
		pr_warn("%s:%s MUIC was reset, try re-write MUIC registers\n",
				MUIC_DEV_NAME, __func__);

		set_muic_reset(muic_data, true);

		max77833_muic_handle_detach(muic_data);

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
		max77833_hv_muic_remove_wo_free_irq(muic_data);
		max77833_hv_muic_reset_hvcontrol_reg(muic_data);
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */
		mutex_unlock(&muic_data->muic_mutex);

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
		max77833_hv_muic_free_irqs(muic_data);
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */
		max77833_muic_free_irqs(muic_data);

		mutex_lock(&muic_data->muic_mutex);
		max77833_muic_init_regs(muic_data);

		set_muic_reset(muic_data, false);
	} else {
		pr_info("%s:%s MUIC was not reset, just return\n", MUIC_DEV_NAME,
			__func__);
	}

	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static int max77833_muic_reset_irq_init(struct max77833_muic_data *muic_data)
{
	int irq_reset_base = sec_pmic_get_irq_base();
	int ret = 0;

	if (irq_reset_base > 0) {
		muic_data->irq_reset_acokbf = irq_reset_base + S2MPS13_IRQ_ACOKBF;

		ret = request_threaded_irq(muic_data->irq_reset_acokbf, NULL,
				muic_reset_irq_thread, 0, "muic-reset-acokbf",
				muic_data);
		if (ret) {
			pr_err("%s:%s Failed to request, pmic IRQ %d [%d]\n",
				MUIC_DEV_NAME, __func__, muic_data->irq_reset_acokbf,
				ret);
			muic_data->irq_reset_acokbf = 0;
			goto out;
		}
	} else {
		pr_err("%s:%s cannot find reset IRQ base(%d)\n", MUIC_DEV_NAME,
					__func__, irq_reset_base);
		muic_data->irq_reset_acokbf = 0;
		ret = -EINVAL;
		goto out;
	}

	pr_info("%s:%s reset_acokbf(%d)\n", MUIC_DEV_NAME, __func__,
		muic_data->irq_reset_acokbf);

out:
	return ret;
}
#endif

static int max77833_muic_probe(struct platform_device *pdev)
{
	struct max77833_dev *max77833 = dev_get_drvdata(pdev->dev.parent);
	struct max77833_platform_data *mfd_pdata = dev_get_platdata(max77833->dev);
	struct max77833_muic_data *muic_data;
	struct max77833_muic_command *muic_command;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_data = kzalloc(sizeof(struct max77833_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	muic_command = kzalloc(sizeof(struct max77833_muic_command), GFP_KERNEL);
	if (!muic_command) {
		pr_err("%s: failed to allocate driver command data\n", __func__);
		ret = -ENOMEM;
		goto err_return_free_data;
	}

	if (!mfd_pdata) {
		pr_err("%s: failed to get max77833 mfd platform data\n", __func__);
		ret = -ENOMEM;
		goto err_kfree;
	}

#if defined(CONFIG_OF)
	ret = of_max77833_muic_dt(muic_data);
	if (ret < 0) {
		pr_err("%s:%s not found muic dt! ret[%d]\n", MUIC_DEV_NAME, __func__, ret);
	}

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	ret = of_max77833_hv_muic_dt(muic_data);
	if (ret < 0) {
		pr_err("%s:%s not found muic dt! ret[%d]\n", MUIC_DEV_NAME, __func__, ret);
	}
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

#endif /* CONFIG_OF */

	muic_data->dev = &pdev->dev;
	mutex_init(&muic_data->muic_mutex);
	mutex_init(&muic_data->reset_mutex);
	mutex_init(&muic_command->command_mutex);
	muic_data->i2c = max77833->muic;
	muic_data->mfd_pdata = mfd_pdata;
	muic_data->pdata = &muic_pdata;
	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->is_muic_ready = false;
	muic_data->is_otg_test = false;
	muic_data->is_factory_start = false;
	muic_data->muic_command = muic_command;
//	max77833_muic_set_afc_ready(muic_data, false);	//CIS
	muic_data->afc_count = 0;
#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
	set_muic_reset(muic_data, false);
#endif

	muic_command_reset(muic_data);

	platform_set_drvdata(pdev, muic_data);

	if (muic_data->pdata->init_gpio_cb) {
		ret = muic_data->pdata->init_gpio_cb(get_switch_sel());
		if (ret) {
			pr_err("%s: failed to init gpio(%d)\n", __func__, ret);
			goto fail;
		}
	}

	mutex_lock(&muic_data->muic_mutex);

	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &max77833_muic_group);
	if (ret) {
		pr_err("%s: failed to create max77833 muic attribute group\n",
				__func__);
		goto fail_sysfs_create;
	}
	dev_set_drvdata(switch_device, muic_data);

	if (muic_data->pdata->init_switch_dev_cb)
		muic_data->pdata->init_switch_dev_cb();

	ret = max77833_muic_init_regs(muic_data);
	if (ret < 0) {
		pr_err("%s:%s Failed to initialize MUIC irq:%d\n",
				MUIC_DEV_NAME, __func__, ret);
		goto fail_init_irq;
	}

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	max77833_hv_muic_initialize(muic_data);
#endif

#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
	ret = max77833_muic_reset_irq_init(muic_data);
	if (ret < 0) {
		pr_err("%s:%s Failed to initialize MUIC RESET irq:%d\n",
			MUIC_DEV_NAME, __func__, ret);
	}
#endif

	mutex_unlock(&muic_data->muic_mutex);

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	/* initial check dpdnvden before cable detection */
	max77833_hv_muic_init_check_dpdnvden(muic_data);
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

	/* initial cable detection */
	max77833_muic_init_detect(muic_data);

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	/* initial hv cable detection */
	if (muic_data->is_afc_muic_ready)
		max77833_hv_muic_init_detect(muic_data);
#endif

	return 0;

fail_init_irq:
	if (muic_data->pdata->cleanup_switch_dev_cb)
		muic_data->pdata->cleanup_switch_dev_cb();
	sysfs_remove_group(&switch_device->kobj, &max77833_muic_group);
fail_sysfs_create:
	mutex_unlock(&muic_data->muic_mutex);
fail:
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&muic_data->muic_mutex);
	mutex_destroy(&muic_data->reset_mutex);
	mutex_destroy(&muic_data->muic_command->command_mutex);
err_kfree:
	kfree(muic_command);
err_return_free_data:
	kfree(muic_data);
err_return:
	return ret;
}

static int max77833_muic_remove(struct platform_device *pdev)
{
	struct max77833_muic_data *muic_data = platform_get_drvdata(pdev);

	sysfs_remove_group(&switch_device->kobj, &max77833_muic_group);

	if (muic_data) {
		pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
		max77833_hv_muic_remove(muic_data);
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
		max77833_muic_free_reset_irq(muic_data);
#endif
		max77833_muic_free_irqs(muic_data);

		if (muic_data->pdata->cleanup_switch_dev_cb)
			muic_data->pdata->cleanup_switch_dev_cb();

		platform_set_drvdata(pdev, NULL);
		mutex_destroy(&muic_data->muic_mutex);
		mutex_destroy(&muic_data->reset_mutex);
		mutex_destroy(&muic_data->muic_command->command_mutex);
		kfree(muic_data->muic_command);
		kfree(muic_data);
	}

	return 0;
}

void max77833_muic_shutdown(struct device *dev)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct i2c_client *i2c;
	struct max77833_dev *max77833;
	int ret;
	u8 val;

	pr_info("%s:%s +\n", MUIC_DEV_NAME, __func__);

	sysfs_remove_group(&switch_device->kobj, &max77833_muic_group);

	if (!muic_data) {
		pr_err("%s:%s no drvdata\n", MUIC_DEV_NAME, __func__);
		goto out;
	}

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	max77833_hv_muic_remove(muic_data);
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

#if defined(CONFIG_MUIC_MAX77833_RESET_WA)
	max77833_muic_free_reset_irq(muic_data);
#endif
	max77833_muic_free_irqs(muic_data);

	i2c = muic_data->i2c;

	if (!i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		goto out_cleanup;
	}

	max77833 = i2c_get_clientdata(i2c);
	pr_info("%s:%s max77833->i2c_lock.count.counter=%d\n", MUIC_DEV_NAME,
		__func__, max77833->i2c_lock.count.counter);

	ret = max77833_read_reg(i2c, MAX77833_MUIC_REG_CTRL3, &val);
	if (ret < 0)
		pr_err("%s:%s fail to update reg\n", MUIC_DEV_NAME, __func__);

	pr_info("%s:%s CTRL3: 0x%02x\n", MUIC_DEV_NAME, __func__, val);

out_cleanup:
	if (muic_data->pdata && muic_data->pdata->cleanup_switch_dev_cb)
		muic_data->pdata->cleanup_switch_dev_cb();

	mutex_destroy(&muic_data->muic_mutex);
	mutex_destroy(&muic_data->reset_mutex);
	mutex_destroy(&muic_data->muic_command->command_mutex);
	kfree(muic_data->muic_command);
	kfree(muic_data);

out:
	pr_info("%s:%s -\n", MUIC_DEV_NAME, __func__);
}

static struct platform_driver max77833_muic_driver = {
	.driver		= {
		.name	= MUIC_DEV_NAME,
		.owner	= THIS_MODULE,
		.shutdown = max77833_muic_shutdown,
	},
	.probe		= max77833_muic_probe,
	.remove		= max77833_muic_remove,
};

static int __init max77833_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return platform_driver_register(&max77833_muic_driver);
}
module_init(max77833_muic_init);

static void __exit max77833_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	platform_driver_unregister(&max77833_muic_driver);
}
module_exit(max77833_muic_exit);

MODULE_DESCRIPTION("Maxim MAX77833 MUIC driver");
MODULE_AUTHOR("<seo0.jeong@samsung.com>");
MODULE_LICENSE("GPL");
