/*
 * max77888-private.h - Voltage regulator driver for the Maxim 77888
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

#ifndef __LINUX_MFD_MAX77888_PRIV_H
#define __LINUX_MFD_MAX77888_PRIV_H

#include <linux/i2c.h>

#define MAX77888_NUM_IRQ_MUIC_REGS	3
#define MAX77888_REG_INVALID		(0xff)

#define MAX77888_IRQSRC_CHG             (1 << 0)
#define MAX77888_IRQSRC_TOP             (1 << 1)
#define MAX77888_IRQSRC_FLASH           (1 << 2)
#define MAX77888_IRQSRC_MUIC            (1 << 3)

enum max77888_reg {
	MAX77888_PMIC_REG_PMICID1			= 0x20,
	MAX77888_PMIC_REG_PMICID2			= 0x21,
	MAX77888_PMIC_REG_INTSRC			= 0x22,
	MAX77888_PMIC_REG_INTSRC_MASK			= 0x23,
	MAX77888_PMIC_REG_TOPSYS_INT			= 0x24,
	MAX77888_PMIC_REG_RESERVED_25			= 0x25,
	MAX77888_PMIC_REG_TOPSYS_INT_MASK		= 0x26,
	MAX77888_PMIC_REG_RESERVED_27			= 0x27,
	MAX77888_PMIC_REG_TOPSYS_STAT			= 0x28,
	MAX77888_PMIC_REG_RESERVED_29			= 0x29,
	MAX77888_PMIC_REG_MAINCTRL1			= 0x2A,
	MAX77888_PMIC_REG_LSCNFG			= 0x2B,
	MAX77888_PMIC_REG_RESERVED_2C			= 0x2C,
	MAX77888_PMIC_REG_RESERVED_2D			= 0x2D,

	MAX77888_CHG_REG_CHG_INT			= 0xB0,
	MAX77888_CHG_REG_CHG_INT_MASK			= 0xB1,
	MAX77888_CHG_REG_CHG_INT_OK			= 0xB2,
	MAX77888_CHG_REG_CHG_DTLS_00			= 0xB3,
	MAX77888_CHG_REG_CHG_DTLS_01			= 0xB4,
	MAX77888_CHG_REG_CHG_DTLS_02			= 0xB5,
	MAX77888_CHG_REG_CHG_DTLS_03			= 0xB6,
	MAX77888_CHG_REG_CHG_CNFG_00			= 0xB7,
	MAX77888_CHG_REG_CHG_CNFG_01			= 0xB8,
	MAX77888_CHG_REG_CHG_CNFG_02			= 0xB9,
	MAX77888_CHG_REG_CHG_CNFG_03			= 0xBA,
	MAX77888_CHG_REG_CHG_CNFG_04			= 0xBB,
	MAX77888_CHG_REG_CHG_CNFG_05			= 0xBC,
	MAX77888_CHG_REG_CHG_CNFG_06			= 0xBD,
	MAX77888_CHG_REG_CHG_CNFG_07			= 0xBE,
	MAX77888_CHG_REG_CHG_CNFG_08			= 0xBF,
	MAX77888_CHG_REG_CHG_CNFG_09			= 0xC0,
	MAX77888_CHG_REG_CHG_CNFG_10			= 0xC1,
	MAX77888_CHG_REG_CHG_CNFG_11			= 0xC2,
	MAX77888_CHG_REG_CHG_CNFG_12			= 0xC3,
	MAX77888_CHG_REG_CHG_CNFG_13			= 0xC4,
	MAX77888_CHG_REG_CHG_CNFG_14			= 0xC5,
	MAX77888_CHG_REG_SAFEOUT_CTRL			= 0xC6,

	MAX77888_PMIC_REG_END,
};

#if 0
#define MAX77888_REG_MAINCTRL1_MRDBTMER_MASK	(0x7)
#define MAX77888_REG_MAINCTRL1_MREN		(1 << 3)
#define MAX77888_REG_MAINCTRL1_BIASEN		(1 << 7)
#endif

/* Slave addr = 0x4A: MUIC */
enum max77888_muic_reg {
	MAX77888_MUIC_REG_ID		= 0x00,
	MAX77888_MUIC_REG_INT1		= 0x01,
	MAX77888_MUIC_REG_INT2		= 0x02,
	MAX77888_MUIC_REG_STATUS1	= 0x04,
	MAX77888_MUIC_REG_STATUS2	= 0x05,
	MAX77888_MUIC_REG_INTMASK1	= 0x07,
	MAX77888_MUIC_REG_INTMASK2	= 0x08,
	MAX77888_MUIC_REG_CDETCTRL1	= 0x0A,
	MAX77888_MUIC_REG_CDETCTRL2	= 0x0B,
	MAX77888_MUIC_REG_CTRL1		= 0x0C,
	MAX77888_MUIC_REG_CTRL2		= 0x0D,
	MAX77888_MUIC_REG_CTRL3		= 0x0E,
	MAX77888_MUIC_REG_CTRL4		= 0x16,

	MAX77888_MUIC_REG_END,
};

/* Slave addr = 0x4A: Charger */
enum max77888_charger_reg {
	MAX77888_CHG_REG_STATUS3	= 0x06,
	MAX77888_CHG_REG_CHG_CTRL1	= 0x0F,
	MAX77888_CHG_REG_CHG_CTRL2	= 0x10,
	MAX77888_CHG_REG_CHG_CTRL3	= 0x11,
	MAX77888_CHG_REG_CHG_CTRL4	= 0x12,
	MAX77888_CHG_REG_CHG_CTRL5	= 0x13,
	MAX77888_CHG_REG_CHG_CTRL6	= 0x14,
	MAX77888_CHG_REG_CHG_CTRL7	= 0x15,

	MAX77888_CHG_REG_END,
};

/* Slave addr = 0x90: Haptic */
enum max77888_haptic_reg {
	MAX77888_HAPTIC_REG_STATUS		= 0x00,
	MAX77888_HAPTIC_REG_CONFIG1		= 0x01,
	MAX77888_HAPTIC_REG_CONFIG2		= 0x02,
	MAX77888_HAPTIC_REG_CONFIG_CHNL		= 0x03,
	MAX77888_HAPTIC_REG_CONFG_CYC1		= 0x04,
	MAX77888_HAPTIC_REG_CONFG_CYC2		= 0x05,
	MAX77888_HAPTIC_REG_CONFIG_PER1		= 0x06,
	MAX77888_HAPTIC_REG_CONFIG_PER2		= 0x07,
	MAX77888_HAPTIC_REG_CONFIG_PER3		= 0x08,
	MAX77888_HAPTIC_REG_CONFIG_PER4		= 0x09,
	MAX77888_HAPTIC_REG_CONFIG_DUTY1	= 0x0A,
	MAX77888_HAPTIC_REG_CONFIG_DUTY2	= 0x0B,
	MAX77888_HAPTIC_REG_CONFIG_PWM1		= 0x0C,
	MAX77888_HAPTIC_REG_CONFIG_PWM2		= 0x0D,
	MAX77888_HAPTIC_REG_CONFIG_PWM3		= 0x0E,
	MAX77888_HAPTIC_REG_CONFIG_PWM4		= 0x0F,
	MAX77888_HAPTIC_REG_REV			= 0x10,

	MAX77888_HAPTIC_REG_END,
};

/* MAX77888 CHG_CNFG_00 register */
#define CHG_CNFG_00_MODE_SHIFT			0
#define CHG_CNFG_00_CHG_SHIFT			0
#define CHG_CNFG_00_OTG_SHIFT			1
#define CHG_CNFG_00_BUCK_SHIFT			2
#define CHG_CNFG_00_BOOST_SHIFT			3
#define CHG_CNFG_00_DIS_MUIC_CTRL_SHIFT		5
#define CHG_CNFG_00_MODE_MASK			(0xf << CHG_CNFG_00_MODE_SHIFT)
#define CHG_CNFG_00_CHG_MASK			(1 << CHG_CNFG_00_CHG_SHIFT)
#define CHG_CNFG_00_OTG_MASK			(1 << CHG_CNFG_00_OTG_SHIFT)
#define CHG_CNFG_00_BUCK_MASK			(1 << CHG_CNFG_00_BUCK_SHIFT)
#define CHG_CNFG_00_BOOST_MASK			(1 << CHG_CNFG_00_BOOST_SHIFT)
#define CHG_CNFG_00_DIS_MUIC_CTRL_MASK		(1 << CHG_CNFG_00_DIS_MUIC_CTRL_SHIFT)

/* MAX77888 CHG_CNFG_04 register */
#define CHG_CNFG_04_CHG_CV_PRM_SHIFT		0
#define CHG_CNFG_04_CHG_CV_PRM_MASK		(0x1f << CHG_CNFG_04_CHG_CV_PRM_SHIFT)
enum max77888_irq_source {
	TOPSYS_INT,
	CHG_INT,
	MUIC_INT1,
	MUIC_INT2,

	MAX77888_IRQ_GROUP_NR,
};

enum max77888_irq {
	/* PMIC; TOPSYS */
	MAX77888_TOPSYS_IRQ_T120C_INT,
	MAX77888_TOPSYS_IRQ_T140C_INT,
	MAX77888_TOPSYS_IRQLOWSYS_INT,

	/* PMIC; Charger */
	MAX77888_CHG_IRQ_BYP_I,
	MAX77888_CHG_IRQ_BATP_I,
	MAX77888_CHG_IRQ_BAT_I,
	MAX77888_CHG_IRQ_CHG_I,
	MAX77888_CHG_IRQ_WCIN_I,
	MAX77888_CHG_IRQ_CHGIN_I,

	/* MUIC INT1 */
	MAX77888_MUIC_IRQ_INT1_ADC,
	MAX77888_MUIC_IRQ_INT1_ADCERR,
	MAX77888_MUIC_IRQ_INT1_ADC1K,

	/* MUIC INT2 */
	MAX77888_MUIC_IRQ_INT2_CHGTYP,
	MAX77888_MUIC_IRQ_INT2_CHGDETREUN,
	MAX77888_MUIC_IRQ_INT2_DCDTMR,
	MAX77888_MUIC_IRQ_INT2_DXOVP,
	MAX77888_MUIC_IRQ_INT2_VBVOLT,

	MAX77888_IRQ_NR,
};

struct max77888_dev {
	struct device *dev;
	struct i2c_client *i2c;		/* 0xCC: Charger */
	struct i2c_client *muic;	/* 0x4A; MUIC */
	struct i2c_client *haptic;	/* 0x90; Haptic */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[MAX77888_IRQ_GROUP_NR];
	int irq_masks_cache[MAX77888_IRQ_GROUP_NR];

#ifdef CONFIG_HIBERNATION
	/* For hibernation */
	u8 reg_pmic_dump[MAX77888_PMIC_REG_END];
	u8 reg_muic_dump[MAX77888_MUIC_REG_END];
	u8 reg_haptic_dump[MAX77888_HAPTIC_REG_END];
#endif

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	struct max77888_platform_data *pdata;
};

enum max77888_types {
	TYPE_MAX77888,
};

extern int max77888_irq_init(struct max77888_dev *max77888);
extern void max77888_irq_exit(struct max77888_dev *max77888);
extern int max77888_irq_resume(struct max77888_dev *max77888);

/* MAX77888 shared i2c API function */
extern int max77888_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int max77888_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77888_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77888_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77888_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#if 0	//temp
/* MAX77888 check muic path fucntion */
extern bool is_muic_usb_path_ap_usb(void);
extern bool is_muic_usb_path_cp_usb(void);
#endif
#endif /* __LINUX_MFD_MAX77888_PRIV_H */

