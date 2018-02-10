/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <plat/pm.h>

#include <mach/pm_domains.h>
#include <mach/devfreq.h>
#include <mach/tmu.h>

static DEFINE_SPINLOCK(rpmlock_cmutop);

struct exynos5430_pd_state {
	void __iomem *reg;
	unsigned long val;
};

#if defined(EXYNOS5430_CLK_SRC_GATING)
static struct exynos5430_pd_state cmutop_maudio[] = {
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP0,	.val = (1<<4), },
};
#endif

static struct exynos5430_pd_state cmutop_mfc0[] = {
#if defined(EXYNOS5430_CLK_SRC_GATING)
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP4,	.val = (1<<8 | 1<<4 | 1<<0), },
#endif
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<1), },
};

static struct exynos5430_pd_state cmutop_mfc1[] = {
#if defined(EXYNOS5430_CLK_SRC_GATING)
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP4,	.val = (1<<24 | 1<<20 | 1<<16), },
#endif
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<2), },
};

static struct exynos5430_pd_state cmutop_hevc[] = {
#if defined(EXYNOS5430_CLK_SRC_GATING)
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP2,	.val = (1<<28), },
#endif
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<3), },
};

static struct exynos5430_pd_state cmutop_gscl[] = {
#if defined(EXYNOS5430_CLK_SRC_GATING)
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP3,	.val = (1<<8), },
#endif
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<7), },
};

static struct exynos5430_pd_state cmutop_mscl[] = {
#if defined(EXYNOS5430_CLK_SRC_GATING)
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP_MSCL, .val = (1<<8 | 1<<4 | 1<<0), },
#endif
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<10), },
};

static struct exynos5430_pd_state cmutop_g2d[] = {
#if defined(EXYNOS5430_CLK_SRC_GATING)
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP3,	.val = (1<<4 | 1<<0), },
#endif
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<0), },
};

static struct exynos5430_pd_state cmutop_isp[] = {
#if defined(EXYNOS5430_CLK_SRC_GATING)
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP0,	 .val = (1<<4), },
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP2,	 .val = (1<<4 | 1<<0), },
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP_CAM1, .val = (1<<8 | 1<<4 | 1<<0), },
#endif
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<4), },
};

static struct exynos5430_pd_state cmutop_cam0[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<5), },
};

static struct exynos5430_pd_state cmutop_cam1[] = {
#if defined(EXYNOS5430_CLK_SRC_GATING)
	{ .reg = EXYNOS5430_SRC_ENABLE_TOP2,	 .val = (1<<16 | 1<<12 | 1<<8), },
#endif
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,.val = (1<<7 | 1<<5 | 1<<4 | 1<<1 | 1<<0), },
};

static void exynos5_pd_enable_clk(struct exynos5430_pd_state *ptr, int nr_regs)
{
	int i;
	unsigned long val;
	spin_lock(&rpmlock_cmutop);
	for (i = 0; i < nr_regs; i++, ptr++) {
		val = __raw_readl(ptr->reg);
		__raw_writel(val | ptr->val, ptr->reg);
	}
	spin_unlock(&rpmlock_cmutop);
}

static void exynos5_pd_disable_clk(struct exynos5430_pd_state *ptr, int nr_regs)
{
	unsigned long val;
	spin_lock(&rpmlock_cmutop);
	for (; nr_regs > 0; nr_regs--, ptr++) {
		val = __raw_readl(ptr->reg);
		__raw_writel(val & ~(ptr->val), ptr->reg);
	}
	spin_unlock(&rpmlock_cmutop);
}

#ifdef CONFIG_SOC_EXYNOS5430
static void exynos_pd_notify_power_state(struct exynos_pm_domain *pd, unsigned int turn_on)
{
#ifdef CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ
	exynos5_int_notify_power_status(pd->genpd.name, true);
	exynos5_isp_notify_power_status(pd->genpd.name, true);
	exynos5_disp_notify_power_status(pd->genpd.name, true);
#endif
}

static struct sleep_save exynos_pd_maudio_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD1),
};

static int exynos_pd_maudio_power_on_pre(struct exynos_pm_domain *pd)
{
#if defined(EXYNOS5430_CLK_SRC_GATING)
	exynos5_pd_enable_clk(cmutop_maudio, ARRAY_SIZE(cmutop_maudio));
#endif
	return 0;
}

/* exynos_pd_maudio_power_on_post - callback after power on.
 * @pd: power domain.
 */
static int exynos_pd_maudio_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_maudio_clk_save,
			ARRAY_SIZE(exynos_pd_maudio_clk_save));

	return 0;
}

static int exynos_pd_maudio_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_maudio_clk_save,
			ARRAY_SIZE(exynos_pd_maudio_clk_save));

	return 0;
}

static int exynos_pd_maudio_power_off_post(struct exynos_pm_domain *pd)
{
#if defined(EXYNOS5430_CLK_SRC_GATING)
	exynos5_pd_disable_clk(cmutop_maudio, ARRAY_SIZE(cmutop_maudio));
#endif
	return 0;
}

static struct sleep_save exynos_pd_g3d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D1),
};

static int exynos_pd_g3d_power_on_pre(struct exynos_pm_domain *pd)
{
#if defined(CONFIG_EXYNOS_THERMAL) && defined(CONFIG_SOC_EXYNOS5430)
	exynos_tmu_core_control(false, 2);	/* disable G3D TMU core before G3D is turned off */
#endif
	return 0;
}

static int exynos_pd_g3d_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

#if defined(CONFIG_EXYNOS_THERMAL) && defined(CONFIG_SOC_EXYNOS5430)
	exynos_tmu_core_control(true, 2);	/* enable G3D TMU core after G3D is turned on */
#endif

	DEBUG_PRINT_INFO("EXYNOS5430_DIV_G3D: %08x\n", __raw_readl(EXYNOS5430_DIV_G3D));
	DEBUG_PRINT_INFO("EXYNOS5430_SRC_SEL_G3D: %08x\n", __raw_readl(EXYNOS5430_SRC_SEL_G3D));

	return 0;
}

static int exynos_pd_g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	return 0;
}

static struct sleep_save exynos_pd_mfc0_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC0_SECURE_SMMU_MFC),
};

/* exynos_pd_mfc0_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mfc0_power_on_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_mfc0, ARRAY_SIZE(cmutop_mfc0));
	return 0;
}

static int exynos_pd_mfc0_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_mfc0_clk_save,
			ARRAY_SIZE(exynos_pd_mfc0_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(1, S5P_VA_SYSREG_MFC0 + 0x204);

	return 0;
}

static int exynos_pd_mfc0_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_mfc0_clk_save,
			ARRAY_SIZE(exynos_pd_mfc0_clk_save));

	return 0;
}

/* exynos_pd_mfc0_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mfc0_power_off_post(struct exynos_pm_domain *pd)
{
	exynos5_pd_disable_clk(cmutop_mfc0, ARRAY_SIZE(cmutop_mfc0));
	return 0;
}

static struct sleep_save exynos_pd_mfc1_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC10),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC11),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC1_SECURE_SMMU_MFC),
};

/* exynos_pd_mfc1_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mfc1_power_on_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_mfc1, ARRAY_SIZE(cmutop_mfc1));
	return 0;
}

static int exynos_pd_mfc1_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_mfc1_clk_save,
			ARRAY_SIZE(exynos_pd_mfc1_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(1, S5P_VA_SYSREG_MFC1 + 0x204);

	return 0;
}

static int exynos_pd_mfc1_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_mfc1_clk_save,
			ARRAY_SIZE(exynos_pd_mfc1_clk_save));

	return 0;
}

/* exynos_pd_mfc1_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mfc1_power_off_post(struct exynos_pm_domain *pd)
{
	exynos5_pd_disable_clk(cmutop_mfc1, ARRAY_SIZE(cmutop_mfc1));
	return 0;
}

static struct sleep_save exynos_pd_hevc_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC_SECURE_SMMU_HEVC),
};

/* exynos_pd_hevc_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_hevc_power_on_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_hevc, ARRAY_SIZE(cmutop_hevc));
	return 0;
}

static int exynos_pd_hevc_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_hevc_clk_save,
			ARRAY_SIZE(exynos_pd_hevc_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(1, S5P_VA_SYSREG_HEVC + 0x204);

	return 0;
}

static int exynos_pd_hevc_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_hevc_clk_save,
			ARRAY_SIZE(exynos_pd_hevc_clk_save));

	return 0;
}

/* exynos_pd_hevc_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_hevc_power_off_post(struct exynos_pm_domain *pd)
{
	exynos5_pd_disable_clk(cmutop_hevc, ARRAY_SIZE(cmutop_hevc));
	return 0;
}

static struct sleep_save exynos_pd_gscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL2),
};

/* exynos_pd_gscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_gscl_power_on_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_gscl, ARRAY_SIZE(cmutop_gscl));
	return 0;
}

static int exynos_pd_gscl_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

static int exynos_pd_gscl_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	return 0;
}

/* exynos_pd_gscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_gscl_power_off_post(struct exynos_pm_domain *pd)
{
	exynos5_pd_disable_clk(cmutop_gscl, ARRAY_SIZE(cmutop_gscl));
	return 0;
}

static struct sleep_save exynos_pd_disp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP1),
};

/* exynos_pd_disp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 */
static int exynos_pd_disp_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg |= ((1 << 8 ) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 1));
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	return 0;
}

/* exynos_pd_disp_power_on_post - setup after power on.
 * @pd: power domain.
 *
 * enable DISP dynamic clock gating
 */
static int exynos_pd_disp_power_on_post(struct exynos_pm_domain *pd)
{
	void __iomem* reg;

	s3c_pm_do_restore_core(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	/* Enable DISP dynamic clock gating */
	reg = ioremap(0x13B80000, SZ_4K);
	writel(0xf, reg + 0x204);
	writel(0x1f, reg + 0x208);
	writel(0x0, reg + 0x500);
	iounmap(reg);

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

/* exynos_pd_disp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 * check Decon has been reset.
 */
static int exynos_pd_disp_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("disp pre power off\n");
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg |= ((1 << 8 ) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 1));
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	s3c_pm_do_save(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	return 0;
}

/* exynos_pd_disp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable IP_MIF3 clk.
 * disable SRC_SEL_MIF4/5
 */
static int exynos_pd_disp_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg &= ~((1 << 8 ) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 1));
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	__raw_writel(0x0, EXYNOS5430_SRC_SEL_MIF4);
	__raw_writel(0x0, EXYNOS5430_SRC_SEL_MIF5);

	return 0;
}

static struct sleep_save exynos_pd_mscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_JPEG),
};

/* exynos_pd_mscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mscl_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_mscl, ARRAY_SIZE(cmutop_mscl));
	return 0;
}

static int exynos_pd_mscl_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_mscl_clk_save,
			ARRAY_SIZE(exynos_pd_mscl_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(1, S5P_VA_SYSREG_MSCL + 0x204);
	__raw_writel(0, S5P_VA_SYSREG_MSCL + 0x500);

	/* set MSCL EMA control */
	__raw_writel(0x114, S5P_VA_SYSREG_MSCL + 0x31C);

	return 0;
}

static int exynos_pd_mscl_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_mscl_clk_save,
			ARRAY_SIZE(exynos_pd_mscl_clk_save));

	return 0;
}

/* exynos_pd_mscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mscl_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_mscl, ARRAY_SIZE(cmutop_mscl));
	return 0;
}

static struct sleep_save exynos_pd_g2d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D_SECURE_SMMU_G2D),
};

/* exynos_pd_g2d_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_g2d_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_g2d, ARRAY_SIZE(cmutop_g2d));
	return 0;
}

static int exynos_pd_g2d_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

static int exynos_pd_g2d_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	return 0;
}

/* exynos_pd_g2d_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_g2d_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_g2d, ARRAY_SIZE(cmutop_g2d));
	return 0;
}

static struct sleep_save exynos_pd_isp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP2),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP3),
};

/* exynos_pd_isp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_isp_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_isp, ARRAY_SIZE(cmutop_isp));

#if defined(CONFIG_EXYNOS_THERMAL) && defined(CONFIG_SOC_EXYNOS5430)
	exynos_tmu_core_control(false, 4);	/* disable ISP TMU core before ISP is turned off */
#endif

	return 0;
}

static int exynos_pd_isp_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(0x3, S5P_VA_SYSREG_ISP + 0x204);
	__raw_writel(0x7, S5P_VA_SYSREG_ISP + 0x208);
	__raw_writel(0x7F, S5P_VA_SYSREG_ISP + 0x20C);
	__raw_writel(0xC, S5P_VA_SYSREG_ISP + 0x500);

#if defined(CONFIG_EXYNOS_THERMAL) && defined(CONFIG_SOC_EXYNOS5430)
	exynos_tmu_core_control(true, 4);	/* enable ISP TMU core after ISP is turned on */
#endif

	return 0;
}

/* exynos_pd_isp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_ISP1 clk.
 */
static int exynos_pd_isp_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_ISP1);
	reg |= (1 << 12 | 1 << 11 | 1 << 8 | 1 << 7 | 1 << 2);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_ISP1);

	s3c_pm_do_save(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	return 0;
}

/* exynos_pd_isp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_isp_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_isp, ARRAY_SIZE(cmutop_isp));
	return 0;
}

static struct sleep_save exynos_pd_cam0_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM02),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM03),
#if defined(CONFIG_EXYNOS5430_HD)
	SAVE_ITEM(EXYNOS5430_ENABLE_SCLK_CAM0),
#endif
};

/* exynos_pd_cam0_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam0_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_cam0, ARRAY_SIZE(cmutop_cam0));
	return 0;
}

static int exynos_pd_cam0_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_cam0_clk_save,
			ARRAY_SIZE(exynos_pd_cam0_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(0x3, S5P_VA_SYSREG_CAM0 + 0x204);
	__raw_writel(0x7, S5P_VA_SYSREG_CAM0 + 0x208);
	__raw_writel(0x7FF, S5P_VA_SYSREG_CAM0 + 0x20C);
	__raw_writel(0x1F, S5P_VA_SYSREG_CAM0 + 0x500);

	return 0;
}

/* exynos_pd_cam0_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_CAM01 clk.
 */
static int exynos_pd_cam0_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_CAM01);
	reg |= (1 << 12);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_CAM01);

	s3c_pm_do_save(exynos_pd_cam0_clk_save,
			ARRAY_SIZE(exynos_pd_cam0_clk_save));

	return 0;
}

/* exynos_pd_cam0_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam0_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_cam0, ARRAY_SIZE(cmutop_cam0));
	return 0;
}

static struct sleep_save exynos_pd_cam1_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM10),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM11),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM12),
};

/* exynos_pd_cam1_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam1_power_on_pre(struct exynos_pm_domain *pd)
{
	pr_info(PM_DOMAIN_PREFIX "%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_cam1, ARRAY_SIZE(cmutop_cam1));
	return 0;
}

static int exynos_pd_cam1_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_cam1_clk_save,
			ARRAY_SIZE(exynos_pd_cam1_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(0x3, S5P_VA_SYSREG_CAM1 + 0x204);
	__raw_writel(0x7, S5P_VA_SYSREG_CAM1 + 0x208);
	__raw_writel(0xFFF, S5P_VA_SYSREG_CAM1 + 0x20C);

	return 0;
}

/* exynos_pd_cam1_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_CAM11 clk.
 */
static int exynos_pd_cam1_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);
	pr_info(PM_DOMAIN_PREFIX "%s try to power off\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_CAM11);
	reg |= (1 << 19 | 1 << 18 | 1 << 16 | 1 << 15 | 1 << 14 | 1 << 13);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_CAM11);

	s3c_pm_do_save(exynos_pd_cam1_clk_save,
			ARRAY_SIZE(exynos_pd_cam1_clk_save));

	return 0;
}

/* exynos_pd_cam1_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam1_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_cam1, ARRAY_SIZE(cmutop_cam1));
	return 0;
}

/* helpers for special power-off sequence with LPI control */
#define __set_mask(name) __raw_writel(name##_ALL, name)
#define __clr_mask(name) __raw_writel(~(name##_ALL), name)

static int force_down_pre(const char *name)
{
	if (strncmp(name, "pd-cam0", 7) == 0) {
		/* For prevent FW clock gating */
		__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_CAM0_LOCAL);
		__raw_writel(0x0000007F, EXYNOS5430_ENABLE_PCLK_CAM0_LOCAL);
		__raw_writel(0x0000007F, EXYNOS5430_ENABLE_SCLK_CAM0_LOCAL);
		__raw_writel(0x0000007F, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0);
		__raw_writel(0x0000001F, EXYNOS5430_ENABLE_IP_CAM0_LOCAL1);

		/* For prevent HOST clock gating */
		__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_CAM00);
		__raw_writel(0xFFFFFFFF, EXYNOS5430_ENABLE_ACLK_CAM01);
		__raw_writel(0x000007FF, EXYNOS5430_ENABLE_ACLK_CAM02);
		__raw_writel(0x07FFFFFF, EXYNOS5430_ENABLE_PCLK_CAM0);
		__raw_writel(0x000001FF, EXYNOS5430_ENABLE_SCLK_CAM0);
		__raw_writel(0x000003FF, EXYNOS5430_ENABLE_IP_CAM00);
		__raw_writel(0x007FFFFF, EXYNOS5430_ENABLE_IP_CAM01);
		__raw_writel(0x000007FF, EXYNOS5430_ENABLE_IP_CAM02);
		__raw_writel(0x0000001F, EXYNOS5430_ENABLE_IP_CAM03);

		/* LPI disable */
		__set_mask(EXYNOS5430_LPI_MASK_CAM0_BUSMASTER);
	} else if (strncmp(name, "pd-cam1", 7) == 0) {
		/* For prevent FW clock gating */
		__raw_writel(0x0000001B, EXYNOS5430_ENABLE_ACLK_CAM1_LOCAL);
		__raw_writel(0x00003FFF, EXYNOS5430_ENABLE_PCLK_CAM1_LOCAL);
		__raw_writel(0x00000007, EXYNOS5430_ENABLE_SCLK_CAM1_LOCAL);
		__raw_writel(0x00007FFF, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0);
		__raw_writel(0x00000007, EXYNOS5430_ENABLE_IP_CAM1_LOCAL1);

		/* For prevent HOST clock gating */
		__raw_writel(0x0000001B, EXYNOS5430_ENABLE_ACLK_CAM10);
		__raw_writel(0x3FFFFFFF, EXYNOS5430_ENABLE_ACLK_CAM11);
		__raw_writel(0x00000FFF, EXYNOS5430_ENABLE_ACLK_CAM12);
		__raw_writel(0x1FFFFFFF, EXYNOS5430_ENABLE_PCLK_CAM1);
		__raw_writel(0x00001FFF, EXYNOS5430_ENABLE_SCLK_CAM1);
		__raw_writel(0x00FFFFFF, EXYNOS5430_ENABLE_IP_CAM10);
		__raw_writel(0x003FFFFF, EXYNOS5430_ENABLE_IP_CAM11);
		__raw_writel(0x00000FFF, EXYNOS5430_ENABLE_IP_CAM12);

		/* LPI disable */
		__set_mask(EXYNOS5430_LPI_MASK_CAM1_BUSMASTER);
	} else if (strncmp(name, "pd-isp", 6) == 0) {
		/* For prevent FW clock gating */
		__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_ISP_LOCAL);
		__raw_writel(0x0000003F, EXYNOS5430_ENABLE_PCLK_ISP_LOCAL);
		__raw_writel(0x0000003F, EXYNOS5430_ENABLE_SCLK_ISP_LOCAL);
		__raw_writel(0x0000007F, EXYNOS5430_ENABLE_IP_ISP_LOCAL0);
		__raw_writel(0x0000000F, EXYNOS5430_ENABLE_IP_ISP_LOCAL1);

		/* For prevent HOST clock gating */
		__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_ISP0);
		__raw_writel(0x0003FFFF, EXYNOS5430_ENABLE_ACLK_ISP1);
		__raw_writel(0x00007FFF, EXYNOS5430_ENABLE_ACLK_ISP2);
		__raw_writel(0x07FFFFFF, EXYNOS5430_ENABLE_PCLK_ISP);
		__raw_writel(0x0000003F, EXYNOS5430_ENABLE_SCLK_ISP);
		__raw_writel(0x000003FF, EXYNOS5430_ENABLE_IP_ISP0);
		__raw_writel(0x0000FFFF, EXYNOS5430_ENABLE_IP_ISP1);
		__raw_writel(0x00007FFF, EXYNOS5430_ENABLE_IP_ISP2);
		__raw_writel(0x0000000F, EXYNOS5430_ENABLE_IP_ISP3);

		__set_mask(EXYNOS5430_LPI_MASK_ISP_BUSMASTER);
	} else {
		pr_err(PM_DOMAIN_PREFIX "%s invalid pd name\n",
					name);
		return -EINVAL;
	}

	return 0;
}

static int force_down_post(const char *name)
{
	return 0;
}

static unsigned int check_power_status(struct exynos_pm_domain *pd, int power_flags,
					unsigned int timeout)
{
	/* check STATUS register */
	while ((__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
		if (timeout == 0) {
			pr_err("%s@%p: %08x, %08x, %08x\n",
					pd->genpd.name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			return 0;
		}
		--timeout;
		cpu_relax();
		usleep_range(8, 10);
	}

	return timeout;
}

#define TIMEOUT_COUNT	5000 /* about 50ms, based on 1ms */
static int exynos_pd_power_off_custom(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		if (force_down_pre(pd->name))
			pr_warn("%s: failed to make force down state\n", pd->name);

		/* sc_feedback to OPTION register */
		__raw_writel(0x0102, pd->base+0x8);

		/* on/off value to CONFIGURATION register */
		__raw_writel(power_flags, pd->base);

		timeout = check_power_status(pd, power_flags, TIMEOUT_COUNT);

		if (force_down_post(pd->name))
			pr_warn("%s: failed to restore normal state\n", pd->name);

		if (unlikely(!timeout)) {
			pr_err(PM_DOMAIN_PREFIX "%s can't control power, timeout\n",
					pd->name);
			mutex_unlock(&pd->access_lock);
			return -ETIMEDOUT;
		} else {
			pr_info(PM_DOMAIN_PREFIX "%s power down success\n", pd->name);
		}

		if (unlikely(timeout < (TIMEOUT_COUNT >> 1))) {
			pr_warn("%s@%p: %08x, %08x, %08x\n",
					pd->name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			pr_warn(PM_DOMAIN_PREFIX "long delay found during %s is %s\n",
					pd->name, power_flags ? "on":"off");
		}
	}
	pd->status = power_flags;
	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s@%p: %08x, %08x, %08x\n",
				pd->genpd.name, pd->base,
				__raw_readl(pd->base),
				__raw_readl(pd->base+4),
				__raw_readl(pd->base+8));

	return 0;
}

static struct exynos_pd_callback pd_callback_list[] = {
	{
		.name = "pd-maudio",
		.on_pre = exynos_pd_maudio_power_on_pre,
		.on_post = exynos_pd_maudio_power_on_post,
		.off_pre = exynos_pd_maudio_power_off_pre,
		.off_post = exynos_pd_maudio_power_off_post,
	}, {
		.name = "pd-mfc0",
		.on_pre = exynos_pd_mfc0_power_on_pre,
		.on_post = exynos_pd_mfc0_power_on_post,
		.off_pre = exynos_pd_mfc0_power_off_pre,
		.off_post = exynos_pd_mfc0_power_off_post,
	}, {
		.name = "pd-mfc1",
		.on_pre = exynos_pd_mfc1_power_on_pre,
		.on_post = exynos_pd_mfc1_power_on_post,
		.off_pre = exynos_pd_mfc1_power_off_pre,
		.off_post = exynos_pd_mfc1_power_off_post,
	}, {
		.name = "pd-hevc",
		.on_pre = exynos_pd_hevc_power_on_pre,
		.on_post = exynos_pd_hevc_power_on_post,
		.off_pre = exynos_pd_hevc_power_off_pre,
		.off_post = exynos_pd_hevc_power_off_post,
	}, {
		.name = "pd-gscl",
		.on_pre = exynos_pd_gscl_power_on_pre,
		.on_post = exynos_pd_gscl_power_on_post,
		.off_pre = exynos_pd_gscl_power_off_pre,
		.off_post = exynos_pd_gscl_power_off_post,
	}, {
		.name = "pd-g3d",
		.on_pre = exynos_pd_g3d_power_on_pre,
		.on_post = exynos_pd_g3d_power_on_post,
		.off_pre = exynos_pd_g3d_power_off_pre,
	}, {
		.name = "pd-disp",
		.on_pre = exynos_pd_disp_power_on_pre,
		.on_post = exynos_pd_disp_power_on_post,
		.off_pre = exynos_pd_disp_power_off_pre,
		.off_post = exynos_pd_disp_power_off_post,
	}, {
		.name = "pd-mscl",
		.on_pre = exynos_pd_mscl_power_on_pre,
		.on_post = exynos_pd_mscl_power_on_post,
		.off_pre = exynos_pd_mscl_power_off_pre,
		.off_post = exynos_pd_mscl_power_off_post,
	}, {
		.name = "pd-g2d",
		.on_pre = exynos_pd_g2d_power_on_pre,
		.on_post = exynos_pd_g2d_power_on_post,
		.off_pre = exynos_pd_g2d_power_off_pre,
		.off_post = exynos_pd_g2d_power_off_post,
	}, {
		.name = "pd-isp",
		.on_pre = exynos_pd_isp_power_on_pre,
		.on_post = exynos_pd_isp_power_on_post,
		.off_pre = exynos_pd_isp_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_isp_power_off_post,
	}, {
		.name = "pd-cam0",
		.on_pre = exynos_pd_cam0_power_on_pre,
		.on_post = exynos_pd_cam0_power_on_post,
		.off_pre = exynos_pd_cam0_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_cam0_power_off_post,
	}, {
		.name = "pd-cam1",
		.on_pre = exynos_pd_cam1_power_on_pre,
		.on_post = exynos_pd_cam1_power_on_post,
		.off_pre = exynos_pd_cam1_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_cam1_power_off_post,
	},
};

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;

	/* find callback function for power domain */
	for (i=0, cb = &pd_callback_list[0]; i<ARRAY_SIZE(pd_callback_list); i++, cb++) {
		if (strcmp(cb->name, pd->name))
			continue;

		DEBUG_PRINT_INFO("%s: found callback function\n", pd->name);
		break;
	}

	pd->cb = cb;
	return cb;
}

#endif
