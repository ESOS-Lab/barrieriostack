/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.

 * Alternatively, this program is free software in case of open source projec;
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */

#ifndef __DSIM_H__
#define __DSIM_H__

#include <linux/device.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/workqueue.h>

#include <media/v4l2-subdev.h>
#include <media/media-entity.h>

#include "./panels/decon_lcd.h"
#include "regs-dsim.h"
#include "dsim_common.h"

#define DSIM_PAD_SINK		0
#define DSIM_PADS_NUM		1

#define DSIM_RX_FIFO_READ_DONE	(0x30800002)
#define DSIM_MAX_RX_FIFO	(64)

#define dsim_err(fmt, ...)					\
	do {							\
		pr_err(pr_fmt(fmt), ##__VA_ARGS__);		\
		exynos_ss_printk(fmt, ##__VA_ARGS__);		\
	} while (0)

#define dsim_info(fmt, ...)					\
	do {							\
		pr_info(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define dsim_dbg(fmt, ...)					\
	do {							\
		pr_debug(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define call_panel_ops(q, op, args...)				\
	(((q)->panel_ops->op) ? ((q)->panel_ops->op(args)) : 0)

extern struct dsim_device *dsim0_for_decon;
extern struct dsim_device *dsim1_for_decon;

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED		1
#define PANEL_STATE_SUSPENDING	2

#define PANEL_DISCONNEDTED		0
#define PANEL_CONNECTED			1

enum mipi_dsim_pktgo_state {
	DSIM_PKTGO_DISABLED,
	DSIM_PKTGO_ENABLED
};

/* operation state of dsim driver */
enum dsim_state {
	DSIM_STATE_HSCLKEN,	/* HS clock was enabled. */
	DSIM_STATE_ULPS,	/* DSIM was entered ULPS state */
	DSIM_STATE_SUSPEND	/* DSIM is suspend state */
};

struct dsim_resources {
	struct clk *pclk;
	struct clk *dphy_esc;
	struct clk *dphy_byte;
	struct clk *rgb_vclk0;
	struct clk *pclk_disp;
	int lcd_power[2];
	int lcd_reset;
	struct regulator *regulator_30V;
	struct regulator *regulator_18V;
	struct regulator *regulator_16V;
};

struct panel_private {

	struct backlight_device *bd;
	unsigned char id[3];
	unsigned char code[5];
	unsigned char elvss_set[22];
	unsigned char tset[8];
	int	temperature;
	unsigned int coordinate[2];
	unsigned char date[4];
	unsigned int lcdConnected;
	unsigned int state;
	unsigned int auto_brightness;
	unsigned int br_index;
	unsigned int acl_enable;
	unsigned int caps_enable;
	unsigned int current_acl;
	unsigned int current_hbm;
	unsigned int current_vint;
	unsigned int siop_enable;

	void *dim_data;
	void *dim_info;
	unsigned int *br_tbl;
	unsigned int *hbm_inter_br_tbl;
	unsigned char **hbm_tbl;
	unsigned char **acl_cutoff_tbl;
	unsigned char **acl_opr_tbl;
	struct mutex lock;
	struct dsim_panel_ops *ops;
	unsigned int panel_type;
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	unsigned int mdnie_support;
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_MCD
	unsigned int mcd_on;
#endif

#ifdef CONFIG_LCD_HMT
	unsigned int hmt_on;
	unsigned int hmt_br_index;
	unsigned int hmt_brightness;
	void *hmt_dim_data;
	void *hmt_dim_info;
	unsigned int *hmt_br_tbl;
#endif

#ifdef CONFIG_LCD_ALPM
	unsigned int 	alpm;
	unsigned int 	current_alpm;
	struct mutex	alpm_lock;
#endif
	unsigned int interpolation;
	char hbm_elvss_comp;
	unsigned char hbm_elvss;
	unsigned int hbm_index;
/* hbm interpolation for color weakness */
	unsigned int weakness_hbm_comp;

/*variable for A3 Line */
	struct delayed_work octa_a3_read_data_work;
	struct workqueue_struct	*octa_a3_read_data_work_q;
	unsigned int octa_a3_read_cnt;

	unsigned int a3_elvss_updated;
	char a3_elvss_temp_0[5];
	char a3_elvss_temp_20[5];
	unsigned char a3_vint[10];
	unsigned int a3_vint_updated;

};

struct dsim_panel_ops {
	int (*early_probe)(struct dsim_device *dsim);
	int	(*probe)(struct dsim_device *dsim);
	int	(*displayon)(struct dsim_device *dsim);
	int	(*exit)(struct dsim_device *dsim);
	int	(*init)(struct dsim_device *dsim);
	int (*dump)(struct dsim_device *dsim);
};

struct dsim_device {
	struct device *dev;
	struct dsim_resources res;
	unsigned int irq;
	void __iomem *reg_base;

	enum dsim_state state;

	unsigned int data_lane;
	unsigned long hs_clk;
	unsigned long byte_clk;
	unsigned long escape_clk;
	unsigned char freq_band;
	struct notifier_block fb_notif;

	struct lcd_device	*lcd;
	unsigned int enabled;
	struct decon_lcd lcd_info;
	struct dphy_timing_value	timing;
	int				pktgo;

	int id;
	u32 data_lane_cnt;
	struct mipi_dsim_lcd_driver *panel_ops;

	spinlock_t slock;
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct pinctrl *pinctrl;
	struct pinctrl_state *turnon_tes;
	struct pinctrl_state *turnoff_tes;
	struct mutex rdwr_lock;

	struct panel_private priv;

	struct dsim_clks_param clks_param;
#ifdef CONFIG_LCD_ALPM
	int 			alpm;
#endif

};

/**
 * driver structure for mipi-dsi based lcd panel.
 *
 * this structure should be registered by lcd panel driver.
 * mipi-dsi driver seeks lcd panel registered through name field
 * and calls these callback functions in appropriate time.
 */

struct mipi_dsim_lcd_driver {
	int (*early_probe)(struct dsim_device *dsim);
	int	(*probe)(struct dsim_device *dsim);
	int	(*suspend)(struct dsim_device *dsim);
	int	(*displayon)(struct dsim_device *dsim);
	int	(*resume)(struct dsim_device *dsim);
	int (*dump)(struct dsim_device *dsim);
};

int dsim_write_data(struct dsim_device *dsim, unsigned int data_id,
		unsigned long data0, unsigned int data1);
int dsim_read_data(struct dsim_device *dsim, u32 data_id, u32 addr,
		u32 count, u8 *buf);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
void dsim_pkt_go_ready(struct dsim_device *dsim);
void dsim_pkt_go_enable(struct dsim_device *dsim, bool enable);
#endif

#ifdef CONFIG_LCD_ALPM
#define	ALPM_OFF							0
#define ALPM_ON							1
int alpm_set_mode(struct dsim_device *dsim, int enable);
#endif

static inline struct dsim_device *get_dsim_drvdata(u32 id)
{
	if (id)
		return dsim1_for_decon;
	else
		return dsim0_for_decon;
}

static inline int dsim_wr_data(u32 id, u32 cmd_id, unsigned long d0, u32 d1)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_write_data(dsim, cmd_id, d0, d1);
	if (ret)
		return ret;

	return 0;
}

/* register access subroutines */
static inline u32 dsim_read(u32 id, u32 reg_id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	return readl(dsim->reg_base + reg_id);
}

static inline u32 dsim_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dsim_read(id, reg_id);
	val &= (mask);
	return val;
}

static inline void dsim_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	writel(val, dsim->reg_base + reg_id);
}

static inline void dsim_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	u32 old = dsim_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->reg_base + reg_id);
}

#define DSIM_IOC_ENTER_ULPS		_IOW('D', 0, u32)
#define DSIM_IOC_LCD_OFF		_IOW('D', 1, u32)
#define DSIM_IOC_PKT_GO_ENABLE		_IOW('D', 2, u32)
#define DSIM_IOC_PKT_GO_DISABLE		_IOW('D', 3, u32)
#define DSIM_IOC_PKT_GO_READY		_IOW('D', 4, u32)
#define DSIM_IOC_GET_LCD_INFO		_IOW('D', 5, struct decon_lcd *)
#define DSIM_IOC_PARTIAL_CMD		_IOW('D', 6, u32)
#define DSIM_IOC_SET_PORCH		_IOW('D', 7, struct decon_lcd *)
#define DSIM_IOC_DUMP			_IOW('D', 8, u32)

u32 dsim_reg_get_lineval(u32 id);
u32 dsim_reg_get_hozval(u32 id);

int dsim_write_hl_data(struct dsim_device *dsim, const u8 *cmd, u32 cmdSize);
int dsim_read_hl_data(struct dsim_device *dsim, u8 addr, u32 size, u8 *buf);
#endif /* __DSIM_H__ */
