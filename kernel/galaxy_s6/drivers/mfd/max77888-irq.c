/*
 * max77888-irq.c - Interrupt controller support for MAX77888
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max77888-irq.c
 */

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/max77888.h>
#include <linux/mfd/max77888-private.h>
#include <plat/gpio-cfg.h>

static const u8 max77888_mask_reg[] = {
	[TOPSYS_INT] = MAX77888_PMIC_REG_TOPSYS_INT_MASK,
	[CHG_INT] = MAX77888_CHG_REG_CHG_INT_MASK,
	[MUIC_INT1] = MAX77888_MUIC_REG_INTMASK1,
	[MUIC_INT2] = MAX77888_MUIC_REG_INTMASK2,
};

static struct i2c_client *get_i2c(struct max77888_dev *max77888,
				enum max77888_irq_source src)
{
	switch (src) {
	case TOPSYS_INT ... CHG_INT:
		return max77888->i2c;
	case MUIC_INT1 ... MUIC_INT2:
		return max77888->muic;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct max77888_irq_data {
	int mask;
	enum max77888_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct max77888_irq_data max77888_irqs[] = {
	DECLARE_IRQ(MAX77888_TOPSYS_IRQ_T120C_INT,	TOPSYS_INT, 1 << 0),
	DECLARE_IRQ(MAX77888_TOPSYS_IRQ_T140C_INT,	TOPSYS_INT, 1 << 1),
	DECLARE_IRQ(MAX77888_TOPSYS_IRQLOWSYS_INT,	TOPSYS_INT, 1 << 3),

	DECLARE_IRQ(MAX77888_CHG_IRQ_BYP_I,	CHG_INT, 1 << 0),
	DECLARE_IRQ(MAX77888_CHG_IRQ_BAT_I,	CHG_INT, 1 << 3),
	DECLARE_IRQ(MAX77888_CHG_IRQ_CHG_I,	CHG_INT, 1 << 4),
	DECLARE_IRQ(MAX77888_CHG_IRQ_CHGIN_I,	CHG_INT, 1 << 6),

	DECLARE_IRQ(MAX77888_MUIC_IRQ_INT1_ADC,		MUIC_INT1, 1 << 0),
	DECLARE_IRQ(MAX77888_MUIC_IRQ_INT1_ADCERR,	MUIC_INT1, 1 << 2),
	DECLARE_IRQ(MAX77888_MUIC_IRQ_INT1_ADC1K,	MUIC_INT1, 1 << 3),

	DECLARE_IRQ(MAX77888_MUIC_IRQ_INT2_CHGTYP,	MUIC_INT2, 1 << 0),
	DECLARE_IRQ(MAX77888_MUIC_IRQ_INT2_CHGDETREUN,	MUIC_INT2, 1 << 1),
	DECLARE_IRQ(MAX77888_MUIC_IRQ_INT2_DCDTMR,	MUIC_INT2, 1 << 2),
	DECLARE_IRQ(MAX77888_MUIC_IRQ_INT2_DXOVP,	MUIC_INT2, 1 << 3),
	DECLARE_IRQ(MAX77888_MUIC_IRQ_INT2_VBVOLT,	MUIC_INT2, 1 << 4),
};

static void max77888_irq_lock(struct irq_data *data)
{
	struct max77888_dev *max77888 = irq_get_chip_data(data->irq);

	mutex_lock(&max77888->irqlock);
}

static void max77888_irq_sync_unlock(struct irq_data *data)
{
	struct max77888_dev *max77888 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < MAX77888_IRQ_GROUP_NR; i++) {
		u8 mask_reg = max77888_mask_reg[i];
		struct i2c_client *i2c = get_i2c(max77888, i);

		if (mask_reg == MAX77888_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		max77888->irq_masks_cache[i] = max77888->irq_masks_cur[i];

		max77888_write_reg(i2c, max77888_mask_reg[i],
				max77888->irq_masks_cur[i]);
	}

	mutex_unlock(&max77888->irqlock);
}

static const inline struct max77888_irq_data *
irq_to_max77888_irq(struct max77888_dev *max77888, int irq)
{
	return &max77888_irqs[irq - max77888->irq_base];
}

static void max77888_irq_mask(struct irq_data *data)
{
	struct max77888_dev *max77888 = irq_get_chip_data(data->irq);
	const struct max77888_irq_data *irq_data =
	    irq_to_max77888_irq(max77888, data->irq);

	if (irq_data->group >= MAX77888_IRQ_GROUP_NR)
		return;

	if (irq_data->group >= MUIC_INT1 && irq_data->group <= MUIC_INT2)
		max77888->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
	else
		max77888->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void max77888_irq_unmask(struct irq_data *data)
{
	struct max77888_dev *max77888 = irq_get_chip_data(data->irq);
	const struct max77888_irq_data *irq_data =
	    irq_to_max77888_irq(max77888, data->irq);

	if (irq_data->group >= MAX77888_IRQ_GROUP_NR)
		return;

	if (irq_data->group >= MUIC_INT1 && irq_data->group <= MUIC_INT2)
		max77888->irq_masks_cur[irq_data->group] |= irq_data->mask;
	else
		max77888->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip max77888_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= max77888_irq_lock,
	.irq_bus_sync_unlock	= max77888_irq_sync_unlock,
	.irq_mask		= max77888_irq_mask,
	.irq_unmask		= max77888_irq_unmask,
};

static irqreturn_t max77888_irq_thread(int irq, void *data)
{
	struct max77888_dev *max77888 = data;
	u8 irq_reg[MAX77888_IRQ_GROUP_NR] = {0};
	u8 tmp_irq_reg[MAX77888_IRQ_GROUP_NR] = {};
	u8 irq_src;
	int ret;
	int i;
	pr_debug("%s: irq gpio pre-state(0x%02x)\n", __func__,
				gpio_get_value(max77888->irq_gpio));

#if defined(CONFIG_MUIC_IRQ_CHECK_WR)
clear_retry:
#endif /* CONFIG_MUIC_IRQ_CHECK_WR */
	ret = max77888_read_reg(max77888->i2c, MAX77888_PMIC_REG_INTSRC, &irq_src);
	if (ret < 0) {
		pr_err("%s: Failed to read interrupt source: %d\n", __func__, ret);
		return IRQ_NONE;
	}
	pr_info("%s: interrupt source(0x%02x)\n", __func__, irq_src);

	if (irq_src & MAX77888_IRQSRC_CHG) {
		/* CHG_INT */
		ret = max77888_read_reg(max77888->i2c, MAX77888_CHG_REG_CHG_INT,
				&irq_reg[CHG_INT]);
		pr_info("%s: charger interrupt(0x%02x)\n",
				__func__, irq_reg[CHG_INT]);
		/* mask chgin to prevent chgin infinite interrupt
		 * chgin is unmasked chgin isr
		 */
		if (irq_reg[CHG_INT] &
				max77888_irqs[MAX77888_CHG_IRQ_CHGIN_I].mask) {
			u8 reg_data;
			max77888_read_reg(max77888->i2c,
				MAX77888_CHG_REG_CHG_INT_MASK, &reg_data);
			reg_data |= (1 << 6);
			max77888_write_reg(max77888->i2c,
				MAX77888_CHG_REG_CHG_INT_MASK, reg_data);
		}
	}

	if (irq_src & MAX77888_IRQSRC_TOP) {
		/* TOPSYS_INT */
		max77888_read_reg(max77888->i2c,
			MAX77888_PMIC_REG_TOPSYS_INT, &irq_reg[TOPSYS_INT]);
		pr_info("%s: topsys interrupt(0x%02x)\n",
			__func__, irq_reg[TOPSYS_INT]);
	}

	if (irq_src & MAX77888_IRQSRC_MUIC) {
		/* MUIC INT1 ~ INT2 */
		max77888_bulk_read(max77888->muic, MAX77888_MUIC_REG_INT1,
			MAX77888_NUM_IRQ_MUIC_REGS,
			&tmp_irq_reg[MUIC_INT1]);

		/* Or temp irq register to irq register for if it retries */
		for (i = MUIC_INT1; i < MAX77888_IRQ_GROUP_NR; i++)
			irq_reg[i] |= tmp_irq_reg[i];

		pr_info("%s: muic interrupt(0x%02x, 0x%02x)\n",
			__func__, irq_reg[MUIC_INT1], irq_reg[MUIC_INT2]);
	}

	pr_debug("%s: irq gpio post-state(0x%02x)\n", __func__,
		gpio_get_value(max77888->irq_gpio));

#if defined(CONFIG_MUIC_IRQ_CHECK_WR)
	if (gpio_get_value(max77888->irq_gpio) == 0) {
		pr_warn("%s: irq_gpio is not High!\n", __func__);
		goto clear_retry;
	}
#endif /* CONFIG_MUIC_IRQ_CHECK_WR */

	/* Apply masking */
	for (i = 0; i < MAX77888_IRQ_GROUP_NR; i++) {
		if (i >= MUIC_INT1 && i <= MUIC_INT2)
			irq_reg[i] &= max77888->irq_masks_cur[i];
		else
			irq_reg[i] &= ~max77888->irq_masks_cur[i];
	}

	/* Report */
	for (i = 0; i < MAX77888_IRQ_NR; i++) {
		if (irq_reg[max77888_irqs[i].group] & max77888_irqs[i].mask)
			handle_nested_irq(max77888->irq_base + i);
	}

	return IRQ_HANDLED;
}

int max77888_irq_resume(struct max77888_dev *max77888)
{
	int ret = 0;
	if (max77888->irq && max77888->irq_base)
		ret = max77888_irq_thread(max77888->irq_base, max77888);

	dev_info(max77888->dev, "%s: irq_resume ret=%d", __func__, ret);

	return ret >= 0 ? 0 : ret;
}

int max77888_irq_init(struct max77888_dev *max77888)
{
	int i;
	int cur_irq;
	int ret;
	u8 i2c_data;

	if (!max77888->irq_gpio) {
		dev_warn(max77888->dev, "No interrupt specified.\n");
		max77888->irq_base = 0;
		return 0;
	}

	if (!max77888->irq_base) {
		pr_err("%s: No interrupt base specified.\n", __func__);
		return 0;
	}

	mutex_init(&max77888->irqlock);

	max77888->irq = gpio_to_irq(max77888->irq_gpio);
	pr_info("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
			max77888->irq, max77888->irq_gpio);

	ret = gpio_request(max77888->irq_gpio, "if_pmic_irq");
	if (ret) {
		pr_err("%s: failed requesting gpio %d\n",
			__func__, max77888->irq_gpio);
		return ret;
	}
	gpio_direction_input(max77888->irq_gpio);
	gpio_free(max77888->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < MAX77888_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;
		/* MUIC IRQ  0:MASK 1:NOT MASK */
		/* Other IRQ 1:MASK 0:NOT MASK */
		if (i >= MUIC_INT1 && i <= MUIC_INT2) {
			max77888->irq_masks_cur[i] = 0x00;
			max77888->irq_masks_cache[i] = 0x00;
		} else {
			max77888->irq_masks_cur[i] = 0xff;
			max77888->irq_masks_cache[i] = 0xff;
		}
		i2c = get_i2c(max77888, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (max77888_mask_reg[i] == MAX77888_REG_INVALID)
			continue;
		if (i >= MUIC_INT1 && i <= MUIC_INT2)
			max77888_write_reg(i2c, max77888_mask_reg[i], 0x00);
		else
			max77888_write_reg(i2c, max77888_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < MAX77888_IRQ_NR; i++) {
		cur_irq = i + max77888->irq_base;
		irq_set_chip_data(cur_irq, max77888);
		irq_set_chip_and_handler(cur_irq, &max77888_irq_chip,
					 handle_edge_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	/* Unmask max77888 interrupt */
	ret = max77888_read_reg(max77888->i2c, MAX77888_PMIC_REG_INTSRC_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return ret;
	}

	i2c_data &= ~(MAX77888_IRQSRC_CHG);	/* Unmask charger interrupt */
	i2c_data &= ~(MAX77888_IRQSRC_MUIC);	/* Unmask muic interrupt */

	max77888_write_reg(max77888->i2c, MAX77888_PMIC_REG_INTSRC_MASK,
			   i2c_data);

	pr_info("%s:%s max77888_PMIC_REG_INTSRC_MASK=0x%02x\n",
			MFD_DEV_NAME, __func__, i2c_data);

	ret = request_threaded_irq(max77888->irq, NULL, max77888_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "max77888-irq", max77888);

	if (ret) {
		pr_err("%s: Failed to request IRQ %d: %d\n", __func__,
			max77888->irq, ret);
		return ret;
	}

	return 0;
}

void max77888_irq_exit(struct max77888_dev *max77888)
{
	if (max77888->irq)
		free_irq(max77888->irq, max77888);
}
