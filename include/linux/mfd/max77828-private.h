/*
 * max778828-private.h - Voltage regulator driver for the Maxim 77882
 *
 *  Copyright (C) 2011 Samsung Electrnoics
 *  SangYoung Son <hello.son@samsung.com>
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

#ifndef __LINUX_MFD_MAX77828_PRIV_H
#define __LINUX_MFD_MAX77828_PRIV_H

#include <linux/i2c.h>

#define MAX77828_I2C_ADDR		(0x92)
#define MAX77828_REG_INVALID		(0xff)

enum max77828_reg {
	/* Slave addr = 0x92 */
	/* PMIC Top-Level Registers */
	MAX77828_PMIC_REG_PMICID1			= 0X00,
	MAX77828_PMIC_REG_PMICREV			= 0x01,
	MAX77828_PMIC_REG_MAINCTRL1			= 0x02,

	/* Haptic motor driver Registers */
	MAX77828_PMIC_REG_MCONFIG			= 0x10,

	MAX77828_PMIC_REG_END,
};

#define MAX77828_REG_MAINCTRL1_MRDBTMER_MASK	(0x7)
#define MAX77828_REG_MAINCTRL1_MREN		(1 << 3)
#define MAX77828_REG_MAINCTRL1_BIASEN		(1 << 7)

/* Slave addr = 0x4A: MUIC */
enum max77828_muic_reg {
	MAX77828_MUIC_REG_ID		= 0x00,
	MAX77828_MUIC_REG_INT1		= 0x01,
	MAX77828_MUIC_REG_INT2		= 0x02,
	MAX77828_MUIC_REG_INT3		= 0x03,
	MAX77828_MUIC_REG_STATUS1	= 0x04,
	MAX77828_MUIC_REG_STATUS2	= 0x05,
	MAX77828_MUIC_REG_STATUS3	= 0x06,
	MAX77828_MUIC_REG_INTMASK1	= 0x07,
	MAX77828_MUIC_REG_INTMASK2	= 0x08,
	MAX77828_MUIC_REG_INTMASK3	= 0x09,
	MAX77828_MUIC_REG_CDETCTRL1	= 0x0A,
	MAX77828_MUIC_REG_CDETCTRL2	= 0x0B,
	MAX77828_MUIC_REG_CTRL1		= 0x0C,
	MAX77828_MUIC_REG_CTRL2		= 0x0D,
	MAX77828_MUIC_REG_CTRL3		= 0x0E,
	MAX77828_MUIC_REG_CTRL4		= 0x16,
	MAX77828_MUIC_REG_HVCONTROL1	= 0x17,
	MAX77828_MUIC_REG_HVCONTROL2	= 0x18,

	MAX77828_MUIC_REG_END,
};

/* Slave addr = 0x94: RGB LED */
enum max77828_led_reg {
	MAX77828_RGBLED_REG_LEDEN			= 0x30,
	MAX77828_RGBLED_REG_LED0BRT			= 0x31,
	MAX77828_RGBLED_REG_LED1BRT			= 0x32,
	MAX77828_RGBLED_REG_LED2BRT			= 0x33,
	MAX77828_RGBLED_REG_LED3BRT			= 0x34,
	MAX77828_RGBLED_REG_LEDBLNK			= 0x35,
	MAX77828_RGBLED_REG_LEDRMP			= 0x36,

	MAX77828_LED_REG_END,
};

enum max77828_irq_source {
	MUIC_INT1,
	MUIC_INT2,
	MUIC_INT3,

	MAX77828_IRQ_GROUP_NR,
};

#define MUIC_MAX_INT			MUIC_INT3
#define MAX77828_NUM_IRQ_MUIC_REGS	(MUIC_MAX_INT - MUIC_INT1 + 1)

enum max77828_irq {
	/* MUIC INT1 */
	MAX77828_MUIC_IRQ_INT1_ADC,
	MAX77828_MUIC_IRQ_INT1_ADCERR,
	MAX77828_MUIC_IRQ_INT1_ADC1K,

	/* MUIC INT2 */
	MAX77828_MUIC_IRQ_INT2_CHGTYP,
	MAX77828_MUIC_IRQ_INT2_CHGDETREUN,
	MAX77828_MUIC_IRQ_INT2_DCDTMR,
	MAX77828_MUIC_IRQ_INT2_DXOVP,
	MAX77828_MUIC_IRQ_INT2_VBVOLT,

	/* MUIC INT3 */
	MAX77828_MUIC_IRQ_INT3_VBADC,
	MAX77828_MUIC_IRQ_INT3_VDNMON,
	MAX77828_MUIC_IRQ_INT3_DNRES,
	MAX77828_MUIC_IRQ_INT3_MPNACK,
	MAX77828_MUIC_IRQ_INT3_MRXBUFOW,
	MAX77828_MUIC_IRQ_INT3_MRXTRF,
	MAX77828_MUIC_IRQ_MRXPERR,
	MAX77828_MUIC_IRQ_MRXRDY,

	MAX77828_IRQ_NR,
};

struct max77828_dev {
	struct device *dev;
	struct i2c_client *i2c; /* 0x92; Haptic, PMIC */
	struct i2c_client *muic; /* 0x4A; MUIC */
	struct i2c_client *led; /* 0x94; RGB LED, FLASH LED */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[MAX77828_IRQ_GROUP_NR];
	int irq_masks_cache[MAX77828_IRQ_GROUP_NR];

#ifdef CONFIG_HIBERNATION
	/* For hibernation */
	u8 reg_pmic_dump[MAX77828_PMIC_REG_END];
	u8 reg_muic_dump[MAX77828_MUIC_REG_END];
	u8 reg_led_dump[MAX77828_LED_REG_END];
#endif

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	struct max77828_platform_data *pdata;
};

enum max77828_types {
	TYPE_MAX77828,
};

extern int max77828_irq_init(struct max77828_dev *max77828);
extern void max77828_irq_exit(struct max77828_dev *max77828);

/* MAX77828 shared i2c API function */
extern int max77828_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int max77828_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77828_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77828_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77828_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

/* MAX77828 Debug. ft */
extern void max77828_muic_read_register(struct i2c_client *i2c);

#endif /* __LINUX_MFD_MAX77828_PRIV_H */

