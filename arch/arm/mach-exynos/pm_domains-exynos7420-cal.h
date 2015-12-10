/*
 * Cal header file for Exynos Generic power domain.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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

/* ########################
   ##### BLK_G3D info #####
   ######################## */

static struct exynos_pd_clk top_clks_g3d[] = {
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOPC1,		.bit_offset = 9, },
};

static struct exynos_pd_clk local_clks_g3d[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_G3D,		.bit_offset = 1, },
};

static struct exynos_pd_reg sys_pwr_regs_g3d[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_G3D_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_G3D_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_G3D_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_G3D_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_g3d[] = {
	{ .reg = EXYNOS7420_DIV_G3D, },
	{ .reg = G3D_LOCK },
	{ .reg = G3D_CON },
	{ .reg = G3D_CON1 },
	{ .reg = EXYNOS7420_MUX_SEL_G3D },
};

/* #########################
   ##### BLK_CAM0 info #####
   ######################### */

static struct exynos_pd_clk top_clks_cam0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 12, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP06,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP06,		.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP06,		.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP06,		.bit_offset = 16, },
};

static struct exynos_pd_clk local_clks_cam0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 18, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 17, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 10, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM01,		.bit_offset = 17, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM01,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 6, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 5, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 6, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 5, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 0, },
};

static struct exynos_pd_clk asyncbridge_clks_cam0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 23,	.belonged_pd = EXYNOS7420_CAM1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 12,	.belonged_pd = EXYNOS7420_ISP0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL0,	.bit_offset = 12,	.belonged_pd = EXYNOS7420_ISP0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 4,	.belonged_pd = EXYNOS7420_ISP1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1_LOCAL,	.bit_offset = 4,	.belonged_pd = EXYNOS7420_ISP1_STATUS, },
};

static struct exynos_pd_reg sys_pwr_regs_cam0[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_CAM0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_CAM0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_CAM0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_CAM0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_CAM0_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS7420_MEMORY_CAM0_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_MEMORY_CAM0_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_CAM0_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_CAM0_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_cam0[] = {
	{ .reg = EXYNOS7420_DIV_CAM0, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM00, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM01, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM02, },
};

/* #########################
   ##### BLK_CAM1 info #####
   ######################### */

static struct exynos_pd_clk top_clks_cam1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 12, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_CAM10,	.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_CAM10,	.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_CAM10,	.bit_offset = 4, },
};

static struct exynos_pd_clk local_clks_cam1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 26, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 25, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 23, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 22, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 11, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 10, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM13,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM13,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM10,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM10,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM10,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM1_LOCAL,	.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM1_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM1_LOCAL,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM1_LOCAL,	.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM1_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM1_LOCAL,	.bit_offset = 0, },
};

static struct exynos_pd_clk asyncbridge_clks_cam1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 17,	.belonged_pd = EXYNOS7420_ISP0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 15,	.belonged_pd = EXYNOS7420_ISP0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 12,	.belonged_pd = EXYNOS7420_ISP1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 9,	.belonged_pd = EXYNOS7420_ISP1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 8,	.belonged_pd = EXYNOS7420_ISP1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 16,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01,		.bit_offset = 1,	.belonged_pd = EXYNOS7420_ISP0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01,		.bit_offset = 0,	.belonged_pd = EXYNOS7420_ISP0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL1,	.bit_offset = 1,	.belonged_pd = EXYNOS7420_ISP0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL1,	.bit_offset = 0,	.belonged_pd = EXYNOS7420_ISP0_STATUS, },
};

static struct exynos_pd_reg sys_pwr_regs_cam1[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_CAM1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_CAM1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_CAM1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_CAM1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_CAM1_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS7420_MEMORY_CAM1_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_MEMORY_CAM1_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_CAM1_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_CAM1_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_cam1[] = {
	{ .reg = EXYNOS7420_DIV_CAM1, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM10, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM11, },
};

/* #########################
   ##### BLK_ISP0 info #####
   ######################### */

static struct exynos_pd_clk top_clks_isp0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 20, },
};

static struct exynos_pd_clk local_clks_isp0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 17, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 15, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 13, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL0,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL0,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL1,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL1,	.bit_offset = 0, },
};

static struct exynos_pd_clk asyncbridge_clks_isp0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 25,	.belonged_pd = EXYNOS7420_CAM1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 10,	.belonged_pd = EXYNOS7420_CAM1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 1,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 0,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 1,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 0,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10,		.bit_offset = 19,	.belonged_pd = EXYNOS7420_CAM1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM1_LOCAL,	.bit_offset = 4,	.belonged_pd = EXYNOS7420_CAM1_STATUS, },
};

static struct exynos_pd_reg sys_pwr_regs_isp0[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_ISP0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_ISP0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_ISP0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_ISP0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_ISP0_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS7420_MEMORY_ISP0_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_MEMORY_ISP0_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_ISP0_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_ISP0_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_isp0[] = {
	{ .reg = EXYNOS7420_DIV_ISP0, },
	{ .reg = EXYNOS7420_MUX_SEL_ISP0, },
};

/* #########################
   ##### BLK_ISP1 info #####
   ######################### */

static struct exynos_pd_clk top_clks_isp1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 12, },
};

static struct exynos_pd_clk local_clks_isp1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 9, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 12, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_ISP1,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1_LOCAL,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_ISP1_LOCAL,	.bit_offset = 0, },
};

static struct exynos_pd_clk asyncbridge_clks_isp1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 26,	.belonged_pd = EXYNOS7420_CAM1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 11,	.belonged_pd = EXYNOS7420_CAM1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 12,	.belonged_pd = EXYNOS7420_CAM1_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 1,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 0,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 1,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 0,	.belonged_pd = EXYNOS7420_CAM0_STATUS, },
};

static struct exynos_pd_reg sys_pwr_regs_isp1[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_ISP1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_ISP1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_ISP1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_ISP1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_ISP1_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_ISP1_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_ISP1_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_isp1[] = {
	{ .reg = EXYNOS7420_DIV_ISP1, },
	{ .reg = EXYNOS7420_MUX_SEL_ISP1, },
};

/* ########################
   ##### BLK_VPP info #####
   ######################## */

static struct exynos_pd_clk top_clks_vpp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP03,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP03,		.bit_offset = 4, },
};

static struct exynos_pd_clk local_clks_vpp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 15, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 14, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 0, },
};

static struct exynos_pd_reg sys_pwr_regs_vpp[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_VPP_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_VPP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_VPP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_VPP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 9, },
	{ .reg = EXYNOS7420_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS7420_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_VPP_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_vpp[] = {
	{ .reg = EXYNOS7420_DIV_VPP, },
	{ .reg = EXYNOS7420_MUX_SEL_VPP, },
};

/* #########################
   ##### BLK_DISP info #####
   ######################### */

static struct exynos_pd_clk top_clks_disp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP03,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 12, },
};

static struct exynos_pd_clk local_clks_disp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP,		.bit_offset = 31, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP,		.bit_offset = 30, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP,		.bit_offset = 29, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP,		.bit_offset = 28, },
};

static struct exynos_pd_clk asyncbridge_clks_disp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 3,	.belonged_pd = EXYNOS7420_VPP_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 2,	.belonged_pd = EXYNOS7420_VPP_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 1,	.belonged_pd = EXYNOS7420_VPP_STATUS, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 0,	.belonged_pd = EXYNOS7420_VPP_STATUS, },
};

static struct exynos_pd_reg sys_pwr_regs_disp[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 9, },
	{ .reg = EXYNOS7420_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS7420_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_DISP_SYS_PWR_REG	,	.bit_offset = 0, },
};

static struct sleep_save save_list_disp[] = {
	{ .reg = EXYNOS7420_DIV_DISP, },
	{ .reg = DISP_LOCK, },
	{ .reg = DISP_CON, },
	{ .reg = DISP_CON1, },
	{ .reg = DISP_CON2, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP0, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP1, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP2, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP3, },
};

/* ########################
   ##### BLK_AUD info #####
   ######################## */

static struct exynos_pd_clk local_clks_aud[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_AUD,		.bit_offset = 28, },
};

static struct exynos_pd_reg sys_pwr_regs_aud[] = {
	{ .reg = EXYNOS7420_PAD_RETENTION_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_GPIO_MODE_AUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKRUN_CMU_AUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_AUD_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_AUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_AUD_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_aud[] = {
	{ .reg = EXYNOS7420_DIV_AUD0, },
	{ .reg = EXYNOS7420_DIV_AUD1, },
	{ .reg = EXYNOS7420_MUX_SEL_AUD, },
};

/* ########################
   ##### BLK_G2D info #####
   ######################## */

static struct exynos_pd_clk top_clks_g2d[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOPC1,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOPC1,		.bit_offset = 0, },
};
static struct exynos_pd_clk local_clks_g2d[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_G2D,		.bit_offset = 31, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_G2D,		.bit_offset = 30, },
};

static struct exynos_pd_reg sys_pwr_regs_g2d[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_G2D_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_G2D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_G2D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_G2D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_G2D_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_G2D_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_G2D_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_g2d[] = {
	{ .reg = EXYNOS7420_DIV_G2D, },
	{ .reg = EXYNOS7420_MUX_SEL_G2D, },
};

/* #########################
   ##### BLK_MSCL info #####
   ######################### */

static struct exynos_pd_clk top_clks_mscl[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOPC1,		.bit_offset = 20, },
};

static struct exynos_pd_clk local_clks_mscl[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 31, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 30, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 29, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 27, },
};

static struct exynos_pd_reg sys_pwr_regs_mscl[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_MSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_MSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_MSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_MSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 9, },
	{ .reg = EXYNOS7420_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS7420_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_MSCL_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_mscl[] = {
	{ .reg = EXYNOS7420_DIV_MSCL, },
	{ .reg = EXYNOS7420_MUX_SEL_MSCL, },
};

/* ########################
   ##### BLK_MFC info #####
   ######################## */

static struct exynos_pd_clk top_clks_mfc[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOPC1,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOPC1,		.bit_offset = 8, },
};

static struct exynos_pd_clk local_clks_mfc[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC,		.bit_offset = 31, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC,		.bit_offset = 30, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC,		.bit_offset = 29, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC,		.bit_offset = 19, },
};

static struct exynos_pd_reg sys_pwr_regs_mfc[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_MFC_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_MFC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_MFC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_MFC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_MFC_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS7420_MEMORY_MFC_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_MEMORY_MFC_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_MFC_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_MFC_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_mfc[] = {
	{ .reg = EXYNOS7420_DIV_MFC, },
	{ .reg = EXYNOS7420_MUX_SEL_MFC, },
};

/* #########################
   ##### BLK_HEVC info #####
   ######################### */

static struct exynos_pd_clk top_clks_hevc[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOPC1,		.bit_offset = 12, },
};

static struct exynos_pd_clk local_clks_hevc[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_HEVC,		.bit_offset = 31, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_HEVC,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_HEVC,		.bit_offset = 27, },
};

static struct exynos_pd_reg sys_pwr_regs_hevc[] = {
	{ .reg = EXYNOS7420_CLKRUN_CMU_HEVC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_CLKSTOP_CMU_HEVC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_DISABLE_PLL_CMU_HEVC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_LOGIC_HEVC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_MEMORY_HEVC_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_MEMORY_HEVC_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_RESET_CMU_HEVC_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sleep_save save_list_hevc[] = {
	{ .reg = EXYNOS7420_DIV_HEVC, },
	{ .reg = EXYNOS7420_MUX_SEL_HEVC, },
};
