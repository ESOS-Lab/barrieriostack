/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>

#include <plat/devs.h>

#include <mach/map.h>
#include <mach/bts.h>
#include <mach/regs-bts.h>
#include <mach/regs-pmu.h>

#define EXYNOS7_PA_DREX0	0x10800000
#define EXYNOS7_PA_DREX1	0x10900000
#define EXYNOS7_PA_DREX2	0x10A00000
#define EXYNOS7_PA_DREX3	0x10B00000
#define EXYNOS7_PA_NSP		0x10550000

enum bts_index {
	BTS_IDX_DISP_RO_0,
	BTS_IDX_DISP_RO_1,
	BTS_IDX_DISP_RW_0,
	BTS_IDX_DISP_RW_1,
	BTS_IDX_VPP0,
	BTS_IDX_VPP1,
	BTS_IDX_VPP2,
	BTS_IDX_VPP3,
	BTS_IDX_TREX_FIMC_BNS_A,
	BTS_IDX_TREX_FIMC_BNS_B,
	BTS_IDX_TREX_FIMC_BNS_C,
	BTS_IDX_TREX_FIMC_BNS_D,
	BTS_IDX_TREX_3AA0,
	BTS_IDX_MFC_0,
	BTS_IDX_MFC_1,
};

enum bts_id {
	BTS_DISP_RO_0 = (1 << BTS_IDX_DISP_RO_0),
	BTS_DISP_RO_1 = (1 << BTS_IDX_DISP_RO_1),
	BTS_DISP_RW_0 = (1 << BTS_IDX_DISP_RW_0),
	BTS_DISP_RW_1 = (1 << BTS_IDX_DISP_RW_1),
	BTS_VPP0 = (1 << BTS_IDX_VPP0),
	BTS_VPP1 = (1 << BTS_IDX_VPP1),
	BTS_VPP2 = (1 << BTS_IDX_VPP2),
	BTS_VPP3 = (1 << BTS_IDX_VPP3),
	BTS_TREX_FIMC_BNS_A = (1 << BTS_IDX_TREX_FIMC_BNS_A),
	BTS_TREX_FIMC_BNS_B = (1 << BTS_IDX_TREX_FIMC_BNS_B),
	BTS_TREX_FIMC_BNS_C = (1 << BTS_IDX_TREX_FIMC_BNS_C),
	BTS_TREX_FIMC_BNS_D = (1 << BTS_IDX_TREX_FIMC_BNS_D),
	BTS_TREX_3AA0 = (1 << BTS_IDX_TREX_3AA0),
	BTS_MFC_0 = (1 << BTS_IDX_MFC_0),
	BTS_MFC_1 = (1 << BTS_IDX_MFC_1),
};

enum exynos_bts_scenario {
	BS_DEFAULT,
	BS_MFC_UD_ENCODING_ENABLE,
	BS_MFC_UD_ENCODING_DISABLE,
	BS_DISABLE,
	BS_MAX,
};

struct bts_table {
	struct bts_set_table *table_list;
	unsigned int table_num;
};

struct bts_info {
	enum bts_id id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table[BS_MAX];
	const char *pd_name;
	const char *clk_name;
	struct clk *clk;
	bool on;
	enum exynos_bts_scenario scen;
	struct list_head list;
	struct list_head scen_list;
	bool enable;
};

struct bts_set_table {
	unsigned int reg;
	unsigned int val;
};

struct bts_scen_status {
	bool ud_scen;
};

struct bts_scenario {
	const char *name;
	unsigned int ip;
	enum exynos_bts_scenario id;
};

struct bts_scen_status pr_state = {
	.ud_scen = false,
};

#define update_ud_scen(a)	(pr_state.ud_scen = a)

#define BTS_DISP	(BTS_DISP_RO_0 | BTS_DISP_RO_1 | BTS_DISP_RW_0 | BTS_DISP_RW_1)
#define BTS_VPP		(BTS_VPP0 | BTS_VPP1 | BTS_VPP2 | BTS_VPP3)
#define BTS_CAM		(BTS_TREX_FIMC_BNS_A | BTS_TREX_FIMC_BNS_B | BTS_TREX_FIMC_BNS_C | \
			BTS_TREX_FIMC_BNS_D | BTS_TREX_3AA0)
#define BTS_MFC		(BTS_MFC_0 | BTS_MFC_1)

#define is_bts_scen_ip(a) (a & BTS_MFC)

#define BTS_AXI_TABLE(num)					\
static struct bts_set_table axiqos_##num##_table[] = {	\
	{READ_QOS_CONTROL, 0x0},			\
	{WRITE_QOS_CONTROL, 0x0},			\
	{READ_CHANNEL_PRIORITY, num},			\
	{READ_TOKEN_MAX_VALUE, 0xffdf},			\
	{READ_BW_UPPER_BOUNDARY, 0x18},			\
	{READ_BW_LOWER_BOUNDARY, 0x1},			\
	{READ_INITIAL_TOKEN_VALUE, 0x8},		\
	{WRITE_CHANNEL_PRIORITY, num},			\
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},		\
	{WRITE_BW_UPPER_BOUNDARY, 0x18},		\
	{WRITE_BW_LOWER_BOUNDARY, 0x1},			\
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},		\
	{READ_QOS_CONTROL, 0x1},			\
	{WRITE_QOS_CONTROL, 0x1}			\
}
BTS_AXI_TABLE(0xaaaa);
BTS_AXI_TABLE(0x4444);

#define BTS_TREX_TABLE(num)					\
static struct bts_set_table trexqos_##num##_table[] = {		\
	{TREX_QOS_CONTROL, 0x0},				\
	{TREX_DEMOTION_QOS_VALUE, num},				\
	{TREX_DEFAULT_QOS_VALUE, num},				\
	{TREX_PROMOTION_QOS_VALUE, num},			\
	{TREX_QOS_THRESHOLD_FOR_EMERGENCY_RISING, 0x3},		\
	{TREX_QOS_THRESHOLD_FOR_EMERGENCY_FALLING, 0x3},	\
	{TREX_QOS_CONTROL, 0x1},				\
}
BTS_TREX_TABLE(0xbbbb);

static struct bts_set_table axiqos_uhd_mfc_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0xEEEE},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{WRITE_CHANNEL_PRIORITY, 0xDDDD},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{READ_QOS_CONTROL, 0x1},
	{WRITE_QOS_CONTROL, 0x1},
};

static struct bts_set_table axibts_disable_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
};

static struct bts_set_table trexbts_disable_table[] = {
	{TREX_QOS_CONTROL, 0x0},
};

#ifdef BTS_DBGGEN
#define BTS_DBG(x...) pr_err(x)
#else
#define BTS_DBG(x...) do {} while (0)
#endif

#ifdef BTS_DBGGEN1
#define BTS_DBG1(x...) pr_err(x)
#else
#define BTS_DBG1(x...) do {} while (0)
#endif

static unsigned int exynos7_drex[] = {
	EXYNOS7_PA_DREX0,
	EXYNOS7_PA_DREX1,
	EXYNOS7_PA_DREX2,
	EXYNOS7_PA_DREX3,
};

static struct bts_info exynos7_bts[] = {
	[BTS_IDX_DISP_RO_0] = {
		.id = BTS_DISP_RO_0,
		.name = "disp_ro_0",
		.pa_base = EXYNOS7420_PA_BTS_DISP_RO_0,
		.pd_name = "spd-decon0",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DISP_RO_1] = {
		.id = BTS_DISP_RO_1,
		.name = "disp_ro_1",
		.pa_base = EXYNOS7420_PA_BTS_DISP_RO_1,
		.pd_name = "spd-decon0",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DISP_RW_0] = {
		.id = BTS_DISP_RW_0,
		.name = "disp_rw_0",
		.pa_base = EXYNOS7420_PA_BTS_DISP_RW_0,
		.pd_name = "spd-decon0",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DISP_RW_1] = {
		.id = BTS_DISP_RW_1,
		.name = "disp_rw_1",
		.pa_base = EXYNOS7420_PA_BTS_DISP_RW_1,
		.pd_name = "spd-decon1",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_VPP0] = {
		.id = BTS_VPP0,
		.name = "vpp_0",
		.pa_base = EXYNOS7420_PA_BTS_VPP0,
		.pd_name = "pd-vpp",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_VPP1] = {
		.id = BTS_VPP1,
		.name = "vpp_1",
		.pa_base = EXYNOS7420_PA_BTS_VPP1,
		.pd_name = "pd-vpp",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_VPP2] = {
		.id = BTS_VPP2,
		.name = "vpp_2",
		.pa_base = EXYNOS7420_PA_BTS_VPP2,
		.pd_name = "pd-vpp",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_VPP3] = {
		.id = BTS_VPP3,
		.name = "vpp_3",
		.pa_base = EXYNOS7420_PA_BTS_VPP3,
		.pd_name = "pd-vpp",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FIMC_BNS_A] = {
		.id = BTS_TREX_FIMC_BNS_A,
		.name = "fimc_bns_a",
		.pa_base = EXYNOS7420_PA_BTS_TREX_FIMC_BNS_A,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].table_list = axiqos_0xaaaa_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xaaaa_table),
		.table[BS_DISABLE].table_list = trexbts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(trexbts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FIMC_BNS_B] = {
		.id = BTS_TREX_FIMC_BNS_B,
		.name = "fimc_bns_b",
		.pa_base = EXYNOS7420_PA_BTS_TREX_FIMC_BNS_B,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].table_list = trexqos_0xbbbb_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(trexqos_0xbbbb_table),
		.table[BS_DISABLE].table_list = trexbts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(trexbts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FIMC_BNS_C] = {
		.id = BTS_TREX_FIMC_BNS_C,
		.name = "fimc_bns_c",
		.pa_base = EXYNOS7420_PA_BTS_TREX_FIMC_BNS_C,
		.pd_name = "pd-cam1",
		.table[BS_DEFAULT].table_list = trexqos_0xbbbb_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(trexqos_0xbbbb_table),
		.table[BS_DISABLE].table_list = trexbts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(trexbts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FIMC_BNS_D] = {
		.id = BTS_TREX_FIMC_BNS_D,
		.name = "fimc_bns_d",
		.pa_base = EXYNOS7420_PA_BTS_TREX_FIMC_BNS_D,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].table_list = trexqos_0xbbbb_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(trexqos_0xbbbb_table),
		.table[BS_DISABLE].table_list = trexbts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(trexbts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_3AA0] = {
		.id = BTS_TREX_3AA0,
		.name = "3aa0",
		.pa_base = EXYNOS7420_PA_BTS_TREX_3AA0,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].table_list = trexqos_0xbbbb_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(trexqos_0xbbbb_table),
		.table[BS_DISABLE].table_list = trexbts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(trexbts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_MFC_0] = {
		.id = BTS_MFC_0,
		.name = "mfc_0",
		.pa_base = EXYNOS7420_PA_BTS_MFC_0,
		.pd_name = "pd-mfc",
		.table[BS_DEFAULT].table_list = axiqos_0x4444_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0x4444_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = axiqos_uhd_mfc_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(axiqos_uhd_mfc_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0x4444_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0x4444_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_MFC_1] = {
		.id = BTS_MFC_1,
		.name = "mfc_1",
		.pa_base = EXYNOS7420_PA_BTS_MFC_1,
		.pd_name = "pd-mfc",
		.table[BS_DEFAULT].table_list = axiqos_0x4444_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0x4444_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = axiqos_uhd_mfc_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(axiqos_uhd_mfc_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0x4444_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0x4444_table),
		.table[BS_DISABLE].table_list = axibts_disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(axibts_disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
};

static struct bts_scenario bts_scen[] = {
	[BS_DEFAULT] = {
		.name = "bts_default",
		.id = BS_DEFAULT,
	},
	[BS_MFC_UD_ENCODING_ENABLE] = {
		.name = "bts_mfc_ud_encoding_enabled",
		.ip = BTS_MFC,
		.id = BS_MFC_UD_ENCODING_ENABLE,
	},
	[BS_MFC_UD_ENCODING_DISABLE] = {
		.name = "bts_mfc_ud_encoding_disable",
		.ip = BTS_MFC,
		.id = BS_MFC_UD_ENCODING_DISABLE,
	},
	[BS_DISABLE] = {
		.name = "bts_disable",
		.id = BS_DISABLE,
	},
	[BS_MAX] = {
		.name = "undefined"
	}
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);
static LIST_HEAD(bts_scen_list);

static void bts_set_ip_table(enum exynos_bts_scenario scen,
				struct bts_info *bts)
{
	int i;
	struct bts_set_table *table = bts->table[scen].table_list;

	BTS_DBG("[BTS] %s on:%d bts scen: [%s]->[%s]\n", bts->name, bts->on,
			bts_scen[bts->scen].name, bts_scen[scen].name);

	if (bts->scen == scen)
		return;


	for (i = 0; i < bts->table[scen].table_num; i++) {
		__raw_writel(table->val, bts->va_base + table->reg);
		BTS_DBG1("[BTS] %x-%x\n", table->reg, table->val);
		table++;
	}

	bts->scen = scen;
}

static enum exynos_bts_scenario bts_get_scen(struct bts_info *bts)
{
	enum exynos_bts_scenario scen;

	scen = bts->on ? BS_DEFAULT : BS_DISABLE;

	return scen;
}

static void bts_set_scen(enum exynos_bts_scenario scen)
{
	struct bts_info *bts;

	if (scen == BS_DEFAULT)
		return;

	list_for_each_entry(bts, &bts_scen_list, scen_list)
		if (bts->id & bts_scen[scen].ip)
			if (bts->scen != scen && bts->on)
				bts_set_ip_table(scen, bts);
}

void bts_scen_update(enum bts_scen_type type, unsigned int val)
{
	enum exynos_bts_scenario scen = BS_DEFAULT;

	spin_lock(&bts_lock);

	switch (type) {
	case TYPE_MFC_UD_ENCODING:

		scen = val ? BS_MFC_UD_ENCODING_ENABLE : BS_MFC_UD_ENCODING_DISABLE;
		BTS_DBG("[BTS] MFC_UD_ENCODING: %s\n", bts_scen[scen].name);

		update_ud_scen(val);
		break;
	default:
		break;
	}

	bts_set_scen(scen);

	spin_unlock(&bts_lock);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;
	enum exynos_bts_scenario scen = BS_DEFAULT;

	spin_lock(&bts_lock);

	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);
	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			BTS_DBG("[BTS] %s on/off:%d->%d\n", bts->name, bts->on, on);
			bts->on = on;

			if (!bts->enable) continue;

			scen = bts_get_scen(bts);
			bts_set_ip_table(scen, bts);
		}

	bts_set_scen(scen);

	spin_unlock(&bts_lock);
}

static void bts_drex_init(unsigned int pa_base)
{
	void __iomem *base;

	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	base = ioremap(pa_base, SZ_4K);

	__raw_writel(0x00000000, base + 0x00D8);
	__raw_writel(0x00200020, base + 0x00D0);
	__raw_writel(0x02000200, base + 0x00C8);
	__raw_writel(0x00000FFF, base + 0x00C0);
	__raw_writel(0x0FFF0FFF, base + 0x00B8);
	__raw_writel(0x0FFF0FFF, base + 0x00B0);
	__raw_writel(0x00000000, base + 0x0100);
	__raw_writel(0x88688868, base + 0x0104);

	iounmap(base);
}

static void bts_noc_init(unsigned int pa_base)
{

	void __iomem *base;

	base = ioremap(pa_base, SZ_4K);

	__raw_writel(0x00000008, base + 0x0008);
	__raw_writel(0x00000008, base + 0x0408);
	__raw_writel(0x00000008, base + 0x0808);
	__raw_writel(0x00000008, base + 0x0C08);

	iounmap(base);
}

static int exynos_bts_notifier_event(struct notifier_block *this,
					unsigned long event,
					void *ptr)
{
	int i;

	switch (event) {
	case PM_POST_SUSPEND:

		for (i = 0; i < ARRAY_SIZE(exynos7_drex); i++)
			bts_drex_init(exynos7_drex[i]);

		bts_noc_init(EXYNOS7_PA_NSP + 0x2000);

		return NOTIFY_OK;
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

static int __init exynos7_bts_init(void)
{
	int i;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(exynos7_bts); i++) {
		exynos7_bts[i].va_base
			= ioremap(exynos7_bts[i].pa_base, SZ_4K);

		list_add(&exynos7_bts[i].list, &bts_list);

		if (is_bts_scen_ip(exynos7_bts[i].id))
			list_add(&exynos7_bts[i].scen_list, &bts_scen_list);
	}

	for (i = 0; i < ARRAY_SIZE(exynos7_drex); i++)
		bts_drex_init(exynos7_drex[i]);

	bts_noc_init(EXYNOS7_PA_NSP + 0x2000);

	register_pm_notifier(&exynos_bts_notifier);

	return 0;
}
arch_initcall(exynos7_bts_init);
