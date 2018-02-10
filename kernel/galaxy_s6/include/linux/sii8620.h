/*
 * Copyright (C) 2013 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SII8620_H_
#define _SII8620_H_

#ifdef __KERNEL__
/* Different MHL Dongle/bridge which are used; may be
 * these enums are Samsung-Specific,but they do represent various
 * versions of dongles.
 */
enum dongle_type {
	DONGLE_NONE = 0,
	DONGLE_9290,
	DONGLE_9292,
};

enum connector_type {
	CONN_NONE = 0,
	CONN_5_PIN,     /* USB-type */
	CONN_11_PIN,    /* Galaxy-S3? */
	CONN_30_PIN,    /* Galaxy Tabs */
};

enum mhl_attached_type {
	MHL_UNHANDLED = 0,
	MHL_MUIC_DEV,
	MHL_SMART_DOCK,
	MHL_MM_DOCK,
};

enum mhl_sleep_state {
	MHL_SUSPEND_STATE = 0,
	MHL_RESUME_STATE = 1,
};

enum mhl_gpio_type {
	MHL_GPIO_UNKNOWN_TYPE = 0,
	MHL_GPIO_AP_GPIO = 1,
	MHL_GPIO_PM_GPIO = 2,
	MHL_GPIO_PM_MPP = 3,
};

#define MAX_ELEC_TUNING_CNT 150

struct sii8620_platform_data {
/* Called to setup board-specific power operations */
	void (*power)(bool on);
	/* In case,when connectors are not able to automatically switch the
	* D+/D- Path to SII8240,do the switching manually.
	*/
	void (*enable_path)(bool enable);
	struct i2c_client *tmds_client;
	struct i2c_client *usbt_client;
	struct i2c_client *hdmi_client;
	struct i2c_client *disc_client;
	struct i2c_client *tpi_client;
	struct i2c_client *cbus_client;
	struct clk *mhl_clk;
	/* to handle board-specific connector info & callback */
	enum dongle_type dongle;
	enum connector_type conn;
	int (*reg_notifier)(struct notifier_block *nb);
	int (*unreg_notifier)(struct notifier_block *nb);
	u8 power_state;
	u32 swing_level_v2;
	u32 swing_level_v3;
#ifdef CONFIG_TESTONLY_SYSFS_SW_REG_TUNING
	u32 m_offset[MAX_ELEC_TUNING_CNT];
	u32 m_page[MAX_ELEC_TUNING_CNT];
	u32 m_value[MAX_ELEC_TUNING_CNT];
	int m_cnt;
	u16 m_reg_addr;
#endif

	bool drm_workaround;
	int ddc_i2c_num;
	void (*mhl_sel)(bool enable);
	int (*get_irq)(void);
	int gpio;
	void (*hw_reset)(void);
	void (*gpio_cfg)(enum mhl_sleep_state sleep_status);
	void (*charger_mhl_cb)(bool on, int mhl_charger);
	int (*muic_otg_set)(int on);
	int charging_type;
	/* void (*vbus_present)(bool on); */
#ifdef CONFIG_SAMSUNG_MHL_UNPOWERED
	int (*get_vbus_status)(void);
	void (*sii9234_otg_control)(bool onoff);
#endif
#if defined(CONFIG_OF)
	int gpio_mhl_scl;
	int gpio_mhl_sda;
	int gpio_mhl_irq;
	int gpio_mhl_power_en;
	int gpio_mhl_en;
	int gpio_mhl_reset;
	int gpio_mhl_wakeup;
	int gpio_ta_int;
	enum mhl_gpio_type gpio_mhl_reset_type;
	enum mhl_gpio_type gpio_mhl_en_type;
	char *charger_name;
#endif
#ifdef CONFIG_SII8620_CHECK_MONITOR
	uint32_t monitor_cmd;
	void (*link_monitor)(u8 cmd_value);
#endif
	bool mhl3_dvi_set;
	bool dvi_ver_ck;
};
extern int system_rev;
int acc_register_notifier(struct notifier_block *nb);
#endif /* __SII8620_H__ */

#endif
