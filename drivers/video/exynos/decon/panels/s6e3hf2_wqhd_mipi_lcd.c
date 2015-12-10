/* drivers/video/exynos/decon/panels/s6e3hf2_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/reboot.h>
#include <linux/of_gpio.h>
#include <linux/of.h>

#include "../dsim.h"
#include "decon_lcd.h"

#include "s6e3hf2_wqhd_param.h"

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#endif
//static struct dsim_device *dsim_base;
//static struct backlight_device *bd;

extern int dsim_panel_resume(struct dsim_device *dsim);
extern int dsim_panel_suspend(struct dsim_device *dsim);
extern int dsim_panel_probe(struct dsim_device *dsim);
extern int dsim_panel_on(struct dsim_device *dsim);
extern int dsim_panel_set_brightness(struct dsim_device *dsim ,int force);

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
extern void lcd_init_sysfs(struct dsim_device *dsim);
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
extern int s6e3hf2_send_seq(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num);
extern int s6e3hf2_read(struct dsim_device *dsim, u8 addr, u8 *buf, u32 size);
#endif

static int s6e3hf2_wqhd_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int s6e3hf2_wqhd_set_brightness(struct backlight_device *bd)
{
	struct dsim_device *dsim;
	struct panel_private *priv = bl_get_data(bd);
	int brightness = bd->props.brightness;

	dsim = container_of(priv, struct dsim_device, priv);
	if (brightness < MIN_PLATFORM_BRIGHTNESS || brightness > MAX_PLATFORM_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	dsim_panel_set_brightness(dsim, 0);

	return 0;
}

static const struct backlight_ops s6e3hf2_wqhd_backlight_ops = {
	.get_brightness = s6e3hf2_wqhd_get_brightness,
	.update_status = s6e3hf2_wqhd_set_brightness,
};

static int s6e3hf2_wqhd_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;

	dsim_info("%s was called\n", __func__);

	dsim->lcd = lcd_device_register("panel", dsim->dev, &dsim->priv, NULL);
	if (IS_ERR(dsim->lcd)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(dsim->lcd);
		goto probe_err;
	}

	priv->bd = backlight_device_register("panel", dsim->dev, &dsim->priv, &s6e3hf2_wqhd_backlight_ops, NULL);

	if (IS_ERR(priv->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(priv->bd);
		goto probe_err;
	}
	priv->bd->props.max_brightness = MAX_PLATFORM_BRIGHTNESS;
	priv->bd->props.brightness = DEFAULT_PLATFORM_BRIGHTNESS;

	/*Todo need to rearrange below value */
	priv->lcdConnected = PANEL_CONNECTED;
	priv->state = PANEL_STATE_ACTIVE;
	priv->power = FB_BLANK_UNBLANK;
	priv->temperature = NORMAL_TEMPERATURE;
	priv->acl_enable= 0;
	priv->current_acl = 0;
	priv->auto_brightness = 0;
	priv->siop_enable = 0;
	priv->current_hbm = 0;

	mutex_init(&priv->lock);
#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(dsim);
#endif
	dsim_panel_probe(dsim);

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	mdnie_register(&dsim->lcd->dev, dsim, (mdnie_w)s6e3hf2_send_seq, (mdnie_r)s6e3hf2_read);
#endif
probe_err:
	return ret;
}

static int s6e3hf2_wqhd_displayon(struct dsim_device *dsim)
{
	dsim_info("%s was called\n", __func__);
	dsim_panel_on(dsim);
	return 1;
}

static int s6e3hf2_wqhd_suspend(struct dsim_device *dsim)
{
	dsim_info("%s was called\n", __func__);
	dsim_panel_suspend(dsim);
	return 0;
}

static int s6e3hf2_wqhd_resume(struct dsim_device *dsim)
{
	dsim_info("%s was called\n", __func__);
	return 0;
}

struct mipi_dsim_lcd_driver s6e3hf2_wqhd_mipi_lcd_driver = {
	.probe		= s6e3hf2_wqhd_probe,
	.displayon	= s6e3hf2_wqhd_displayon,
	.suspend	= s6e3hf2_wqhd_suspend,
	.resume		= s6e3hf2_wqhd_resume,
};


#define S6E3HF2_WQXGA_ID1 0x404013
#define S6E3HF2_WQXGA_ID2 0x404014

unsigned int s6e3hf2_lcd_type = 0;


unsigned int lcdtype = 0;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	if ((lcdtype == S6E3HF2_WQXGA_ID1) || (lcdtype == S6E3HF2_WQXGA_ID2)) {
		s6e3hf2_lcd_type = 1;
		dsim_info("S6E3HF2 LCD TYPE : %d (WQXGA)\n", s6e3hf2_lcd_type);
	} else {
		s6e3hf2_lcd_type = 0;
		dsim_info("S6E3HF2 LCD TYPE : %d (WQHD)\n", s6e3hf2_lcd_type);
	}

	return 0;
}
early_param("lcdtype", get_lcd_type);
