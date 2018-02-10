/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5433 SoC.
*/

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock-exynos5433.h>
#include <mach/regs-pmu.h>

#include "clk.h"
#include "clk-pll.h"
#include "clk-exynos5433.h"

enum exynos5433_clks {
	none,

	/* core clocks */
	fin_pll = 1, mem0_pll, mem1_pll, mfc_pll, bus_pll,
	fout_mphy_pll, disp_pll, fout_isp_pll, aud_pll,
	fout_g3d_pll = 10, fout_aud_pll,

	/* gate for special clocks (sclk) */
	sclk_jpeg_mscl = 20, sclk_pcie_100_fsys,
	sclk_isp_spi0_cam1 = 30, sclk_isp_spi1_cam1,
	sclk_isp_uart_cam1, sclk_isp_sensor0, sclk_isp_sensor1,
	sclk_isp_sensor2, sclk_isp_mctadc_cam1,
	sclk_usbdrd30_fsys = 50, sclk_ufsunipro_fsys, sclk_mmc0_fsys, sclk_mmc1_fsys, sclk_mmc2_fsys,
	sclk_usbhost30_fsys,
	sclk_spi0_peric = 60, sclk_spi1_peric, sclk_spi2_peric,
	sclk_uart0_peric, sclk_uart1_peric, sclk_uart2_peric,
	sclk_pcm1_peric, sclk_i2s1_peric, sclk_spdif_peric,
	sclk_slimbus_peric,

	sclk_spi3_peric = 70, sclk_spi4_peric,
	sclk_hpm_mif = 80, sclk_decon_eclk_disp, sclk_decon_vclk_disp,
	sclk_dsd_disp, sclk_decon_tv_eclk_disp, sclk_dsim0_disp,

	/*sclk_mphy_pll,*/ sclk_ufs_mphy, sclk_lli_mphy,
	sclk_jpeg,

	sclk_decon_eclk = 100, sclk_decon_vclk, sclk_dsd, sclk_hdmi_spdif_disp,

	sclk_pcm1 = 110, sclk_i2s1, sclk_spi0, sclk_spi1, sclk_spi2,
	sclk_uart0, sclk_uart1, sclk_uart2, sclk_slimbus, sclk_spdif,

	sclk_isp_spi0 = 130, sclk_isp_spi1, sclk_isp_uart, sclk_isp_mtcadc,

	sclk_usbdrd30 = 140, sclk_mmc0, sclk_mmc1, sclk_mmc2, sclk_ufsunipro, sclk_mphy, sclk_pcie, sclk_usbhost30,

	/* gate clocks */
	aclk_cpif_200 = 320, aclk_disp_333, aclk_bus1_400, aclk_bus2_400, aclk_bus0_400,
	aclk_g3d_400,
	aclk_g2d_400 = 330, aclk_g2d_266, aclk_mfc0_333, aclk_mfc1_333, aclk_hevc_400,
	aclk_isp_400, aclk_isp_dis_400, aclk_cam0_552, aclk_cam0_400, aclk_cam0_333,
	aclk_cam1_552 = 340, aclk_cam1_400, aclk_cam1_333, aclk_gscl_333, aclk_gscl_111,
	aclk_fsys_200, aclk_mscl_400, aclk_peris_66, aclk_peric_66, aclk_imem_266,
	aclk_imem_200 = 350, aclk_imem_sssx_266,

	phyclk_mixer_pixel = 370, phyclk_hdmi_pixel, phyclk_hdmiphy_tmds_clko,
	phyclk_mipidphy_rxclkesc0, phyclk_mipidphy_bitclkdiv8,

	ioclk_i2s1_bclk = 390, ioclk_spi0_clk, ioclk_spi1_clk, ioclk_spi2_clk, ioclk_slimbus_clk,

	phyclk_usbdrd30_udrd30_phyclock = 410, phyclk_usbdrd30_udrd30_pipe_pclk,
	phyclk_usbhost20_phy_freeclk, phyclk_usbhost20_phy_phyclock,
	phyclk_usbhost20_phy_clk48mohci, phyclk_usbhost20_phy_hsic1,
	phyclk_ufs_tx0_symbol, phyclk_ufs_tx1_symbol,
	phyclk_ufs_rx0_symbol, phyclk_ufs_rx1_symbol,
	phyclk_usbhost30_uhost30_phyclock, phyclk_usbhost30_uhost30_pipe_pclk,

	aclk_bus1nd_400 = 430, aclk_bus1sw2nd_400, aclk_bus1np_133, aclk_bus1sw2np_133,
	aclk_ahb2apb_bus1p, pclk_bus1srvnd_133, pclk_sysreg_bus1, pclk_pmu_bus1,

	aclk_bus2rtnd_400 = 440, aclk_bus2bend_400, aclk_bus2np_133, aclk_ahb2apb_bus2p,
	pclk_bus2srvnd_133, pclk_sysreg_bus2, pclk_pmu_bus2,

	/* g2d gate */
	aclk_g2d = 450, aclk_g2dnd_400, aclk_xiu_g2dx, aclk_asyncaxi_sysx,
	aclk_axius_g2dx, aclk_alb_g2d, aclk_bts_g2d, aclk_smmu_g2d, aclk_ppmu_g2dx,
	aclk_mdma1 = 460, aclk_bts_mdma1, aclk_smmu_mdma1,
	aclk_g2dnp_133, aclk_ahb2apb_g2d0p, aclk_ahb2apb_g2d1p, pclk_g2d,
	pclk_sysreg_g2d = 470, pclk_pmu_g2d, pclk_asyncaxi_sysx, pclk_alb_g2d,
	pclk_bts_g2d, pclk_bts_mdma1, pclk_smmu_g2d, pclk_smmu_mdma1, pclk_ppmu_g2d,

	/* gscl gate */
	aclk_gscl0 = 480, aclk_gscl1, aclk_gscl2, aclk_gsd, aclk_gsclbend_333,
	aclk_gsclrtnd_333, aclk_xiu_gsclx, aclk_bts_gscl0, aclk_bts_gscl1, aclk_bts_gscl2,
	aclk_smmu_gscl0 = 490, aclk_smmu_gscl1, aclk_smmu_gscl2, aclk_ppmu_gscl0,
	aclk_ppmu_gscl1, aclk_ppmu_gscl2,
	aclk_ahb2apb_gsclp = 500, pclk_gscl0, pclk_gscl1, pclk_gscl2, pclk_sysreg_gscl,
	pclk_pmu_gscl, pclk_bts_gscl0, pclk_bts_gscl1, pclk_bts_gscl2,
	pclk_smmu_gscl0 = 510, pclk_smmu_gscl1, pclk_smmu_gscl2,
	pclk_ppmu_gscl0, pclk_ppmu_gscl1, pclk_ppmu_gscl2,

	/* mscl gate */
	aclk_m2mscaler0 = 520, aclk_m2mscaler1, aclk_jpeg, aclk_msclnd_400,
	aclk_xiu_msclx, aclk_bts_m2mscaler0, aclk_bts_m2mscaler1, aclk_bts_jpeg,
	aclk_smmu_m2mscaler0 = 530, aclk_smmu_m2mscaler1, aclk_smmu_jpeg,
	aclk_ppmu_m2mscaler0, aclk_ppmu_m2mscaler1,
	aclk_msclnp_100, aclk_ahb2apb_mscl0p, pclk_m2mscaler0, pclk_m2mscaler1,
	pclk_jpeg = 540, pclk_sysreg_mscl, pclk_pmu_mscl,
	pclk_bts_m2mscaler0, pclk_bts_m2mscaler1, pclk_bts_jpeg,
	pclk_smmu_m2mscaler0 = 550, pclk_smmu_m2mscaler1, pclk_smmu_jpeg,
	pclk_ppmu_m2mscaler0, pclk_ppmu_m2mscaler1, pclk_ppmu_jpeg,

	/* fsys gate */
	aclk_pdma = 560, aclk_usbdrd30, aclk_usbhost20, aclk_sromc, aclk_ufs,
	aclk_mmc0, aclk_mmc1, aclk_mmc2, aclk_tsi, aclk_usbhost30,
	aclk_fsysnp_200 = 570, aclk_fsysnd_200, aclk_xiu_fsyssx, aclk_xiu_fsysx,
	aclk_ahb_fsysh, aclk_ahb_usbhs, aclk_ahb_usblinkh,
	aclk_ahb2axi_usbhs = 580, aclk_ahb2apb_fsysp, aclk_axius_fsyssx,
	aclk_axius_usbhs, aclk_axius_pdma, aclk_bts_usbdrd30, aclk_bts_ufs, aclk_bts_usbhost30,
	aclk_smmu_pdma = 590, aclk_smmu_mmc0, aclk_smmu_mmc1, aclk_smmu_mmc2, aclk_ppmu_fsys,
	pclk_gpio_fsys, pclk_pmu_fsys, pclk_sysreg_fsys, pclk_bts_usbdrd30, pclk_bts_usbhost30,
	pclk_bts_ufs = 600, pclk_smmu_pdma, pclk_smmu_mmc0, pclk_smmu_mmc1, pclk_smmu_mmc2,
	pclk_ppmu_fsys,

	/* dis gate */
	aclk_decon = 610, aclk_disp0nd_333, aclk_disp1nd_333, aclk_xiu_disp1x,
	aclk_xiu_decon0x, aclk_xiu_decon1x, aclk_axius_disp1x,
	aclk_bts_deconm0 = 620, aclk_bts_deconm1, aclk_bts_deconm2, aclk_bts_deconm3,
	aclk_ppmu_decon0x = 626, aclk_ppmu_decon1x,
	aclk_dispnp_100 = 630, aclk_ahb_disph, aclk_ahb2apb_dispsfr0p,
	aclk_ahb2apb_dispsfr1p, aclk_ahb2apb_dispsfr2p, pclk_decon, pclk_tv_mixer,
	pclk_dsim0 = 640, pclk_mdnie, pclk_mic, pclk_hdmi, pclk_hdmiphy,
	pclk_sysreg_disp, pclk_pmu_disp, pclk_asyncaxi_tvx,
	pclk_bts_deconm0 = 650, pclk_bts_deconm1, pclk_bts_deconm2, pclk_bts_deconm3, pclk_bts_deconm4,
	pclk_bts_mixerm0, pclk_bts_mixerm1,
	pclk_ppmu_decon0x = 661, pclk_ppmu_decon1x, pclk_ppmu_tvx,

	/* mfc0 gate */
	aclk_mfc0 = 680, aclk_mfc0nd_333, aclk_xiu_mfc0x, aclk_bts_mfc0_0, aclk_bts_mfc0_1,
	aclk_smmu_mfc0_0, aclk_smmu_mfc0_1, aclk_ppmu_mfc0_0, aclk_ppmu_mfc0_1,
	aclk_mfc0np_83 = 690, aclk_ahb2apb_mfc0p, pclk_mfc0, pclk_sysreg_mfc0, pclk_pmu_mfc0,
	pclk_bts_mfc0_0 = 700, pclk_bts_mfc0_1, pclk_smmu_mfc0_0, pclk_smmu_mfc0_1,
	pclk_ppmu_mfc0_0, pclk_ppmu_mfc0_1,

	/* mfc1 gate */
	aclk_mfc1 = 710, aclk_mfc1nd_333, aclk_xiu_mfc1x, aclk_bts_mfc1_0, aclk_bts_mfc1_1,
	aclk_smmu_mfc1_0, aclk_smmu_mfc1_1, aclk_ppmu_mfc1_0, aclk_ppmu_mfc1_1,
	aclk_mfc1np_83 = 720, aclk_ahb2apb_mfc1p, pclk_mfc1, pclk_sysreg_mfc1, pclk_pmu_mfc1,
	pclk_bts_mfc1_0 = 730, pclk_bts_mfc1_1, pclk_smmu_mfc1_0, pclk_smmu_mfc1_1,
	pclk_ppmu_mfc1_0, pclk_ppmu_mfc1_1,

	/* hevc gate */
	aclk_hevc = 740, aclk_hevcnd_400, aclk_xiu_hevcx, aclk_bts_hevc_0, aclk_bts_hevc_1,
	aclk_smmu_hevc_0, aclk_smmu_hevc_1, aclk_ppmu_hevc_0, aclk_ppmu_hevc_1, aclk_hevcnp_100,
	aclk_ahb2apb_hevcp = 750, pclk_hevc, pclk_sysreg_hevc, pclk_pmu_hevc,
	pclk_bts_hevc_0, pclk_bts_hevc_1, pclk_smmu_hevc_0, pclk_smmu_hevc_1,
	pclk_ppmu_hevc_0, pclk_ppmu_hevc_1,

	/* aud gate */
	sclk_ca5 = 770, aclk_dmac, aclk_sramc, aclk_audnp_133, aclk_audnd_133,
	aclk_xiu_lpassx, aclk_axi2apb_lpassp, aclk_axids_lpassp, pclk_sfr_ctrl, pclk_intr_ctrl,
	pclk_timer = 780, pclk_i2s, pclk_pcm, pclk_uart, pclk_slimbus_aud, pclk_sysreg_aud,
	pclk_pmu_aud, pclk_gpio_aud, pclk_dbg_aud, sclk_aud_i2s,

	/* imem gate */
	aclk_sss = 800, aclk_slimsss, aclk_rtic, aclk_imemnd_266, aclk_xiu_sssx,
	aclk_asyncahbm_sss_egl, aclk_alb_imem, aclk_bts_sss_cci, aclk_bts_sss_dram, aclk_bts_slimsss,
	aclk_smmu_sss_cci = 810, aclk_smmu_sss_dram, aclk_smmu_slimsss, aclk_smmu_rtic,
	aclk_ppmu_sssx, aclk_gic, aclk_int_mem, aclk_xiu_pimemx,
	aclk_axi2apb_imem0p = 820, aclk_axi2apb_imem1p, aclk_asyncaxis_mif_pimemx,
	aclk_axids_pimemx_gic, aclk_axids_pimemx_imem0p, aclk_axids_pimemx_imem1p,
	pclk_sss, pclk_slimsss, pclk_rtic,
	pclk_sysreg_imem = 830, pclk_pmu_imem, pclk_alb_imem, pclk_bts_sss_cci, pclk_bts_sss_dram,
	pclk_bts_slimsss, pclk_smmu_sss_cci, pclk_smmu_sss_dram, pclk_smmu_slimsss,
	pclk_smmu_rtic = 840, pclk_ppmu_sssx,

	/* peris gate */
	aclk_perisnp_66 = 850, aclk_ahb2apb_peris0p, aclk_ahb2apb_peris1p, pclk_tzpc0,
	pclk_tzpc1, pclk_tzpc2, pclk_tzpc3, pclk_tzpc4, pclk_tzpc5, pclk_tzpc6,
	pclk_tzpc7 = 860, pclk_tzpc8, pclk_tzpc9, pclk_tzpc10, pclk_tzpc11, pclk_tzpc12,
	pclk_seckey_apbif, pclk_hdmi_cec, pclk_mct, pclk_wdt_egl,
	pclk_wdt_kfc = 870, pclk_chipid_apbif, pclk_toprtc, pclk_cmu_top_apbif, pclk_sysreg_peris,
	pclk_tmu0_apbif, pclk_tmu1_apbif, pclk_custom_efuse_apbif, pclk_antirbk_cnt_apbif,
	pclk_efuse_writer0_apbif = 880, pclk_efuse_writer1_apbif, pclk_hpm_apbif,

	/* peric gate */
	aclk_ahb2apb_peric0p = 890, aclk_ahb2apb_peric1p, aclk_ahb2apb_peric2p,
	pclk_i2c0, pclk_i2c1, pclk_i2c2, pclk_i2c3, pclk_i2c4, pclk_i2c5, pclk_i2c6,
	pclk_i2c7 = 900, pclk_hsi2c0, pclk_hsi2c1, pclk_hsi2c2, pclk_hsi2c3,
	pclk_uart0, pclk_uart1, pclk_uart2, pclk_gpio_peric, pclk_gpio_nfc,
	pclk_gpio_touch = 910, pclk_spi0, pclk_spi1, pclk_spi2, pclk_i2s1,
	pclk_pcm1, pclk_spdif, pclk_slimbus, pclk_pwm,

	/* g3d */
	aclk_g3d = 1000, aclk_g3dnd_600, aclk_asyncapbm_g3d, aclk_bts_g3d0, aclk_bts_g3d1,
	aclk_ppmu_g3d0, aclk_ppmu_g3d1, aclk_g3dnp_150, aclk_ahb2apb_g3dp, aclk_asyncapbs_g3d,
	pclk_sysreg_g3d = 1010, pclk_pmu_g3d, pclk_bts_g3d0, pclk_bts_g3d1,
	pclk_ppmu_g3d0, pclk_ppmu_g3d1,

	/* mif gate */
	aclk_mifnm_200 = 1020, aclk_asyncaxis_cp0, aclk_asyncaxis_cp1,
	aclk_mifnd_133, aclk_mifnp_133, aclk_ahb2apb_mif0p, aclk_ahb2apb_mif1p,
	aclk_ahb2apb_mif2p, aclk_asyncapbs_mif_cssys,
	pclk_drex0 = 1030, pclk_drex1, pclk_drex0_tz, pclk_drex1_tz,
	pclk_ddr_phy0, pclk_ddr_phy1, pclk_pmu_apbif, pclk_abb,
	pclk_monotonic_cnt = 1040, pclk_rtc, pclk_gpio_alive,
	pclk_sysreg_mif, pclk_pmu_mif, pclk_mifsrvnd_133,
	pclk_asyncaxi_drex0_0 = 1050, pclk_asyncaxi_drex0_1, pclk_asyncaxi_drex0_3,
	pclk_asyncaxi_drex1_0, pclk_asyncaxi_drex1_1, pclk_asyncaxi_drex1_3,
	pclk_asyncaxi_cp0 = 1060, pclk_asyncaxi_cp1, pclk_asyncaxi_noc_p_cci,
	pclk_bts_egl, pclk_bts_kfc,
	pclk_ppmu_drex0s0 = 1070, pclk_ppmu_drex0s1, pclk_ppmu_drex0s3,
	pclk_ppmu_drex1s0, pclk_ppmu_drex1s1, pclk_ppmu_drex1s3,
	pclk_ppmu_egl = 1080, pclk_ppmu_kfc,

	aclk_cci = 1090, aclk_mifnd_400, aclk_ixiu_cci,
	aclk_asyncaxis_drex0_0, aclk_asyncaxis_drex0_1, aclk_asyncaxis_drex0_3,
	aclk_asyncaxis_drex1_0, aclk_asyncaxis_drex1_1, aclk_asyncaxis_drex1_3,
	aclk_asyncaxim_egl_mif = 1100, aclk_asyncacem_egl_cci,
	aclk_asyncacem_kfc_cci, aclk_axisyncdns_cci, aclk_axius_egl_cci,
	aclk_ace_sel_egl = 1110, aclk_ace_sel_kfc, aclk_bts_egl, aclk_bts_kfc,
	aclk_ppmu_egl, aclk_ppmu_kfc, aclk_xiu_mifsfrx,
	aclk_asyncaxis_noc_p_cci = 1120, aclk_asyncaxis_mif_imem,
	aclk_asyncaxis_egl_mif, aclk_asyncaxim_egl_ccix,
	aclk_axisyncdn_noc_d, aclk_axisyncdn_cci, aclk_axids_cci_mifsfrx,
	clkm_phy0 = 1130, clkm_phy1, clk2x_phy0, aclk_drex0,
	aclk_drex0_busif_rd, aclk_drex0_busif_wr,
	aclk_drex0_sch, aclk_drex0_memif,
	aclk_drex0_perev = 1140, aclk_drex0_tz,
	aclk_asyncaxim_drex0_0, aclk_asyncaxim_drex0_1, aclk_asyncaxim_drex0_3,
	aclk_asyncaxim_cp0,
	aclk_ppmu_drex0s0 = 1150, aclk_ppmu_drex0s1, aclk_ppmu_drex0s3,
	clk2x_phy1, aclk_drex1, aclk_drex1_busif_rd, aclk_drex1_busif_wr,
	aclk_drex1_sch = 1160, aclk_drex1_memif, aclk_drex1_perev, aclk_drex1_tz,
	aclk_asyncaxim_drex1_0, aclk_asyncaxim_drex1_1, aclk_asyncaxim_drex1_3,
	aclk_asyncaxim_cp1 = 1170, aclk_ppmu_drex1s0,
	aclk_ppmu_drex1s1, aclk_ppmu_drex1s3,
	aclk_mifnd_266,

	/* cpif gate */
	aclk_mdma0 = 1180,
	aclk_lli_svc_loc, aclk_lli_svc_rem, aclk_lli_ll_init,
	aclk_lli_be_init, aclk_lli_ll_targ, aclk_lli_be_targ,
	aclk_cpifnp_200, aclk_cpifnm_200,
	aclk_xiu_cpifsfrx = 1190, aclk_xiu_llix, aclk_ahb2apb_cpifp,
	aclk_axius_lli_ll, aclk_axius_lli_be, aclk_bts_mdma0,
	aclk_ppmu_llix, aclk_smmu_mdma0, pclk_mdma0,
	pclk_sysreg_cpif = 1200, pclk_pmu_cpif, pclk_gpio_cpif,
	pclk_bts_mdma0, pclk_ppmu_llix, pclk_smmu_mdma0,
	sclk_lli_cmn_cfg, sclk_lli_tx0_cfg, sclk_lli_rx0_cfg,

	/* cam0 gate */
	aclk_csis1 = 1210, aclk_csis0,
	pclk_csis1 = 1220, pclk_csis0,
	sclk_phyclk_rxbyteclkhs0_s4 = 1230, sclk_phyclk_rxbyteclkhs0_s2a,

	/* aud0 ip gate */
	gate_gpio_aud = 2000, gate_pmu_aud, gate_sysreg_aud, gate_slimbus_aud,
	gate_uart, gate_pcm, gate_i2s, gate_timer, gate_intr_ctrl,
	gate_sfr_ctrl = 2010, gate_sramc, gate_dmac, gate_pclk_dbg,
	gate_ca5,

	/* aud1 ip gate */
	gate_smmu_lpassx = 2020, gate_axids_lpassp, gate_axi2apb_lpassp,
	gate_xiu_lpassx, gate_audnp_133, gate_audnd_133,

	/* bus ip gate */
	gate_bus1srvnd_133 = 2030, gate_ahb2apb_bus1p, gate_bus1np_133,
	gate_bus1nd_400, gate_pmu_bus1, gate_sysreg_bus1,

	gate_pmu_bus2, gate_sysreg_bus2,

	gate_bus2srvnd_133 = 2040, gate_ahb2apb_bus2p, gate_bus2np_133,
	gate_bus2bend_400, gate_bus2rtnd_400,

	/* cpif0 ip gate */
	gate_mphy_pll = 2050, gate_mphy_pll_mif, gate_freq_det_mphy_pll,
	gate_ufs_mphy, gate_lli_mphy, gate_gpio_cpif, gate_pmu_cpif,
	gate_sysreg_cpif,
	gate_lli_rx0_symbol = 2060, gate_lli_tx0_symbol,
	gate_lli_rx0_cfg, gate_lli_tx0_cfg,
	gate_lli_cmn_cfg, gate_lli_be_targ,
	gate_lli_ll_targ = 2070, gate_lli_be_init,
	gate_lli_ll_init, gate_lli_svc_rem,
	gate_lli_svc_loc, gate_mdma0,

	/* cpif1 ip gate */
	gate_smmu_mdma0 = 2080, gate_ppmu_llix, gate_bts_mdma0,
	gate_axius_lli_be, gate_axius_lli_ll, gate_ahb2apb_cpifp,
	gate_xiu_llix = 2090, gate_xiu_cpifsfrx,
	gate_cpifnp_200, gate_cpifnm_200,

	/* disp0 ip gate */
	gate_freq_det_disp_pll = 2100, gate_pmu_disp, gate_sysreg_disp,
	gate_dsd, gate_mic, gate_mdnie, gate_mic1, gate_dsim1, gate_dpu,
	gate_dsim0 = 2110, gate_decon_tv, gate_hdmiphy,
	gate_hdmi, gate_decon, gate_mipidphy,

	/* disp1 ip gate */
	gate_ppmu_tvx = 2120, gate_ppmu_decon1x, gate_ppmu_decon0x,
	gate_bts_mixerm1 = 2126, gate_bts_mixerm0,
	gate_bts_deconm4 = 2130, gate_bts_deconm3, gate_bts_deconm2,
	gate_bts_deconm1, gate_bts_deconm0,
	gate_ahb2apb_dispsfr2p, gate_ahb2apb_dispsfr1p,
	gate_ahb2apb_dispsfr0p,

	gate_axius_disp1x = 2140, gate_asyncaxi_tvx, gate_ahb_disph,
	gate_xiu_tvx, gate_xiu_decon1x, gate_xiu_decon0x,
	gate_xiu_disp1x = 2150, gate_dispnp_100,
	gate_disp1nd_333, gate_disp0nd_333,

	/* fsys0 ip gate */
	gate_mphy = 2160, gate_sysreg_fsys, gate_pmu_fsys, gate_gpio_fsys,
	gate_tsi, gate_mmc2, gate_mmc1, gate_mmc0,
	gate_ufs = 2170, gate_sromc, gate_usbhost20, gate_usbdrd30, gate_pdma0,
	gate_pdma1, gate_pcie, gate_pcie_phy, gate_tlnk, gate_usbhost30,

	/* fsys1 ip gate */
	gate_ppmu_fsys = 2180, gate_smmu_mmc2, gate_smmu_mmc1,
	gate_smmu_mmc0, gate_smmu_pdma,
	gate_bts_ufs, gate_bts_usbdrd30, gate_bts_usbhost30,
	gate_axius_pdma = 2190, gate_axius_usbhs, gate_axius_fsyssx,
	gate_ahb2apb_fsysp, gate_ahb2axi_usbhs, gate_ahb_usblinkh,
	gate_ahb_usbhs, gate_ahb_fsysh,
	gate_xiu_fsysx = 2200, gate_xiu_fsyssx,
	gate_fsysnp_200, gate_fsysnd_200,

	/* g2d ip gate */
	gate_pmu_g2d = 2210, gate_sysreg_g2d, gate_mdma1, gate_g2d,
	gate_ppmu_g2dx, gate_smmu_mdma1,

	gate_bts_mdma1 = 2220, gate_bts_g2d, gate_alb_g2d,
	gate_axius_g2dx, gate_asyncaxi_sysx,
	gate_ahb2apb_g2d1p, gate_ahb2apb_g2d0p,
	gate_xiu_g2dx = 2230, gate_g2dnp_133, gate_g2dnd_400,

	/* g3d ip gate */
	gate_freq_det_g3d_pll = 2240, gate_hpm_g3d, gate_pmu_g3d,
	gate_sysreg_g3d, gate_g3d,

	gate_ppmu_g3d1, gate_ppmu_g3d0,

	gate_bts_g3d1 = 2250, gate_bts_g3d0,
	gate_asyncapb_g3d, gate_ahb2apb_g3dp,
	gate_g3dnp_150, gate_g3dnd_600,

	/* gscl ip gate */
	gate_pmu_gscl = 2260, gate_sysreg_gscl,
	gate_gsd, gate_gscl2, gate_gscl1, gate_gscl0,

	gate_ppmu_gscl2 = 2270, gate_ppmu_gscl1, gate_ppmu_gscl0,
	gate_bts_gscl2, gate_bts_gscl1, gate_bts_gscl0,
	gate_ahb2apb_gsclp, gate_xiu_gsclx,
	gate_gsclnp_111 = 2280,
	gate_gsclrtnd_333, gate_gsclbend_333,
	gate_smmu_gscl0, gate_smmu_gscl1, gate_smmu_gscl2,

	/* hevc ip gate */
	gate_pmu_hevc = 2290, gate_sysreg_hevc, gate_hevc,

	gate_ppmu_hevc_1, gate_ppmu_hevc_0,
	gate_bts_hevc_1, gate_bts_hevc_0,
	gate_ahb2apb_hevcp = 2300, gate_xiu_hevcx,
	gate_hevcnp_100, gate_hevcnd_400,

	gate_smmu_hevc_1, gate_smmu_hevc_0,

	/* imem ip gate */
	gate_pmu_imem = 2310, gate_sysreg_imem, gate_gic,

	gate_ppmu_sssx, gate_bts_slimsss,
	gate_bts_sss_dram, gate_bts_sss_cci, gate_alb_imem,

	gate_axids_pimemx_imem1p = 2320, gate_axids_pimemx_imem0p,
	gate_axids_pimemx_gic, gate_axius_xsssx,
	gate_asyncahbm_sss_egl, gate_asyncaxis_mif_pimemx,
	gate_axi2apb_imem1p, gate_axi2apb_imem0p,
	gate_xiu_sssx = 2330, gate_xiu_pimemx, gate_imemnd_266,
	gate_int_mem, gate_sss, gate_slimsss,
	gate_rtic,

	gate_smmu_sss_dram = 2340, gate_smmu_sss_cci,
	gate_smmu_slimsss, gate_smmu_rtic,

	/* mfc ip gate */
	gate_pmu_mfc0 = 2350, gate_sysreg_mfc0, gate_mfc0,
	gate_ppmu_mfc0_1, gate_ppmu_mfc0_0,

	gate_bts_mfc0_1, gate_bts_mfc0_0, gate_ahb2apb_mfc0p,
	gate_xiu_mfc0x = 2360, gate_mfc0np_83, gate_mfc0nd_333,
	gate_smmu_mfc0_1, gate_smmu_mfc0_0,

	gate_pmu_mfc1 = 2370, gate_sysreg_mfc1, gate_mfc1,
	gate_ppmu_mfc1_1, gate_ppmu_mfc1_0,
	gate_bts_mfc1_1, gate_bts_mfc1_0,

	gate_ahb2apb_mfc1p = 2380, gate_xiu_mfc1x,
	gate_mfc1np_83, gate_mfc1nd_333,

	gate_smmu_mfc1_1, gate_smmu_mfc1_0,

	/* mscl ip gate */
	gate_pmu_mscl = 2390, gate_sysreg_mscl, gate_jpeg,
	gate_m2mscaler1, gate_m2mscaler0,

	gate_ppmu_jpeg,
	gate_ppmu_m2mscaler1, gate_ppmu_m2mscaler0,
	gate_bts_jpeg = 2400, gate_bts_m2mscaler1,
	gate_bts_m2mscaler0, gate_ahb2apb_mscl0p,
	gate_xiu_msclx, gate_msclnp_100, gate_msclnd_400,

	gate_smmu_m2mscaler0 = 2410,
	gate_smmu_m2mscaler1,
	gate_smmu_jpeg,

	/* peric ip gate */
	gate_slimbus = 2420, gate_pwm, gate_spdif, gate_pcm1,
	gate_i2s1, gate_spi2, gate_spi1, gate_spi0, gate_adcif,
	gate_gpio_touch = 2430, gate_gpio_nfc, gate_gpio_peric,
	gate_pmu_peric, gate_sysreg_peric,
	gate_uart2, gate_uart1, gate_uart0,
	gate_hsi2c3 = 2440, gate_hsi2c2, gate_hsi2c1, gate_hsi2c0,
	gate_i2c7, gate_i2c6, gate_i2c5, gate_i2c4,
	gate_i2c3 = 2450, gate_i2c2, gate_i2c1, gate_i2c0,

	gate_ahb2apb_peric2p, gate_ahb2apb_peric1p,
	gate_ahb2apb_peric0p, gate_pericnp_66,

	/* peris ip gate */
	gate_asv_tb = 2460, gate_hpm_apbif,
	gate_efuse_writer1, gate_efuse_writer1_apbif,
	gate_efuse_writer0, gate_efuse_writer0_apbif,
	gate_tmu1, gate_tmu1_apbif,
	gate_tmu0 = 2470, gate_tmu0_apbif, gate_pmu_peris,
	gate_sysreg_peris, gate_cmu_top_apbif,
	gate_wdt_kfc = 2480, gate_wdt_egl,
	gate_mct, gate_hdmi_cec,

	gate_ahb2apb_peris1p, gate_ahb2apb_peris0p,
	gate_perisnp_66,

	gate_tzpc12 = 2490, gate_tzpc11, gate_tzpc10,
	gate_tzpc9, gate_tzpc8, gate_tzpc7, gate_tzpc6,
	gate_tzpc5, gate_tzpc4,
	gate_tzpc3 = 2500, gate_tzpc2, gate_tzpc1, gate_tzpc0,
	gate_seckey, gate_seckey_apbif,
	gate_chipid, gate_chipid_apbif,

	gate_toprtc = 2510,
	gate_custom_efuse, gate_custom_efuse_apbif,
	gate_antirbk_cnt, gate_antirbk_cnt_apbif,
	gate_rtc = 2515,
	gate_otp_con,

	/* cam0 ip gate */
	gate_pmu_cam0 = 2520, gate_sysreg_cam0,
	gate_cmu_cam0_local, gate_csis1, gate_csis0,
	gate_3aa1, gate_3aa0,
	gate_lite_d = 2530, gate_lite_b, gate_lite_a,

	gate_ahbsyncdn, gate_axius_lite_d,
	gate_axius_lite_b, gate_axius_lite_a,
	gate_asyncapb_3aa1 = 2540, gate_asyncapb_3aa0,
	gate_asyncapb_lite_d, gate_asyncapb_lite_b,
	gate_asyncapb_lite_a, gate_asyncaxi_cam1,
	gate_asyncaxi_isp0p,
	gate_asyncaxi_3aa1 = 2550, gate_asyncaxi_3aa0,
	gate_asyncaxi_lite_d, gate_asyncaxi_lite_b,
	gate_asyncaxi_lite_a, gate_ahb2apb_ispsfrp,
	gate_axi2apb_isp0p = 2560, gate_axi2ahb_isp0p,
	gate_xiu_is0x, gate_xiu_is0ex,
	gate_cam0np_276, gate_cam0nd_400,

	gate_ppmu_isp0ex = 2570,
	gate_smmu_3aa1, gate_smmu_3aa0,
	gate_smmu_lite_d, gate_smmu_lite_b, gate_smmu_lite_a,
	gate_bts_3aa1, gate_bts_3aa0,
	gate_bts_lite_d = 2580, gate_bts_lite_b,
	gate_bts_lite_a,

	gate_lite_freecnt,
	gate_pixelasync_3aa1, gate_pixelasync_3aa0,
	gate_pixelasync_lite_c,
	gate_pixelasync_lite_c_init,

	gate_csis1_local = 2590, gate_csis0_local,
	gate_3aa1_local, gate_3aa0_local,
	gate_lite_d_local, gate_lite_b_local, gate_lite_a_local,

	gate_lite_freecnt_local = 2600,
	gate_pixelasync_3aa1_local, gate_pixelasync_3aa0_local,
	gate_pixelasync_lite_c_local, gate_pixelasync_lite_c_init_local,

	/* cam1 ip gate */
	gate_rxbyteclkhs0_s2b = 2610, gate_lite_c_freecnt,
	gate_pixelasyncm_fd, gate_pixelasyncs_lite_c,
	gate_cssys, gate_pmu_cam1,
	gate_sysreg_cam1, gate_cmu_cam1_local,
	gate_isp_mtcadc = 2620, gate_isp_wdt,
	gate_isp_pwm, gate_isp_uart,
	gate_isp_mcuctl, gate_isp_spi1,
	gate_isp_spi0, gate_isp_i2c2,
	gate_isp_i2c1 = 2630, gate_isp_i2c0,
	gate_isp_mpwm, gate_isp_gic,
	gate_fd, gate_lite_c,
	gate_csis2, gate_isp_ca5,

	gate_asyncapb_fd = 2640, gate_asyncapb_lite_c,
	gate_asyncahbs_sfrisp2h2, gate_asyncahbs_sfrisp2h1,
	gate_asyncaxi_ca5, gate_asyncaxi_ispx2,
	gate_asyncaxi_ispx1, gate_asyncaxi_ispx0,
	gate_asyncaxi_ispex = 2650, gate_asyncaxi_isp3p,
	gate_asyncaxi_fd, gate_asyncaxi_lite_c,
	gate_ahb2apb_isp5p, gate_ahb2apb_isp3p,
	gate_axi2apb_isp3p, gate_ahb_sfrisp2h,
	gate_axi_isp_hx = 2660, gate_axi_isp_cx,
	gate_xiu_ispx, gate_xiu_ispex,
	gate_cam1np_333, gate_cam1nd_400,

	gate_ppmu_ispex, gate_smmu_isp3p,
	gate_smmu_fd = 2670, gate_smmu_lite_c,
	gate_bts_isp3p, gate_bts_fd,
	gate_bts_lite_c, gate_ahbdn_sfrisp2h,
	gate_ahbdn_isp5p, gate_axius_isp3p,
	gate_axius_fd = 2680, gate_axius_lite_c,

	gate_isp_mtcadc_local, gate_isp_wdt_local,
	gate_isp_pwm_local, gate_isp_uart_local,
	gate_isp_mcuctl_local, gate_isp_spi1_local,
	gate_isp_spi0_local = 2690, gate_isp_i2c2_local,
	gate_isp_i2c1_local, gate_isp_i2c0_local,
	gate_isp_mpwm_local, gate_isp_gic_local,
	gate_fd_local, gate_lite_c_local,
	gate_csis2_local = 2700,

	gate_lite_c_freecnt_local, gate_pixelasyncm_fd_local,
	gate_pixelasyncs_lite_c_local,

	/* isp ip gate */
	gate_isp_d_glue = 2710, gate_pmu_isp,
	gate_sysreg_isp, gate_cmu_isp_local,
	gate_scalerp, gate_3dnr,
	gate_dis, gate_scalerc,
	gate_drc, gate_isp,

	gate_axius_scalerp = 2720, gate_axius_scalerc,
	gate_axius_drc,
	gate_asyncahbm_isp2p, gate_asyncahbm_isp1p,
	gate_asyncaxi_dis1, gate_asyncaxi_dis0,
	gate_asyncaxim_isp2p, gate_asyncaxim_isp1p,
	gate_ahb2apb_isp2p = 2730, gate_ahb2apb_isp1p,
	gate_axi2apb_isp2p, gate_axi2apb_isp1p,
	gate_xiu_ispex1, gate_xiu_ispex0,
	gate_ispnd_400,

	gate_ppmu_ispex0, gate_smmu_scalerp,
	gate_smmu_3dnr = 2740, gate_smmu_dis1,
	gate_smmu_dis0, gate_smmu_scalerc,
	gate_smmu_drc, gate_smmu_isp,
	gate_bts_scalerp, gate_bts_3dnr,
	gate_bts_dis1 = 2750, gate_bts_dis0,
	gate_bts_scalerc, gate_bts_drc,
	gate_bts_isp,

	gate_pixelasync_dis, gate_pixelasyncs_scalerp,
	gate_pixelasyncm_ispd, gate_pixelasync_ispc,

	gate_isp_d_glue_local = 2760, gate_scalerp_local,
	gate_3dnr_local, gate_dis_local,
	gate_scalerc_local, gate_drc_local,
	gate_isp_local,

	gate_pixelasync_dis_local = 2770,
	gate_pixelasyncs_scalerp_local,
	gate_pixelasyncm_ispd_local,
	gate_pixelasync_ispc_local,

	gate_top_cam1 = 2780, gate_top_cam0, gate_top_isp, gate_top_mfc, gate_top_hevc, gate_top_gscl,
	gate_top_mscl, gate_top_g2d, gate_top_g3d,

	gate_disp_333 = 2790, gate_cpif_200,
	gate_decon_eclk, gate_decon_vclk, gate_decontv_eclk, gate_dsd_clk, gate_dsim0_clk,

	/* gates for EVT1 */
	gate_ppmu_tv1x = 2800, gate_ppmu_tv0x,
	gate_bts_decontv_m3 = 2804,
	gate_bts_decontv_m2, gate_bts_decontv_m1,
	gate_bts_decontv_m0, gate_xiu_tv1x,
	gate_xiu_tv0x,

	/* gates for Exynos5433 */
	gate_hsi2c4 = 2900, gate_hsi2c5, gate_hsi2c6, gate_hsi2c7,
	gate_hsi2c8, gate_hsi2c9, gate_hsi2c10, gate_hsi2c11,
	gate_spi3, gate_spi4,
	pclk_hsi2c4 = 2910, pclk_hsi2c5, pclk_hsi2c6, pclk_hsi2c7,
	pclk_hsi2c8, pclk_hsi2c9, pclk_hsi2c10, pclk_hsi2c11,

	/* MUX */
	/* eagle & kfc mux */
	mout_egl_pll = 3000,
	mout_bus_pll_egl_user, mout_egl,
	mout_kfc_pll ,
	mout_bus_pll_kfc_user, mout_kfc,

	/* top mux */
	mout_bus_pll_user = 3010,
	mout_mfc_pll_user, mout_mphy_pll_user,
	mout_isp_pll, mout_aud_pll, mout_aud_pll_user_top,
	mout_aclk_g2d_400_a = 3020, mout_aclk_g2d_400_b,
	mout_aclk_gscl_333,
	mout_aclk_mscl_400_a, mout_aclk_mscl_400_b,
	mout_aclk_hevc_400,
	mout_aclk_isp_400 = 3030, mout_aclk_isp_dis_400,
	mout_aclk_cam1_552_a, mout_aclk_cam1_552_b,
	mout_aclk_cam1_333,
	mout_sclk_audio0, mout_sclk_audio1,
	mout_sclk_spi0, mout_sclk_spi1, mout_sclk_spi2,
	mout_sclk_uart0 = 3040,	mout_sclk_uart1, mout_sclk_uart2, mout_sclk_slimbus, mout_sclk_spdif,
	mout_sclk_usbdrd30, mout_sclk_usbhost30, mout_sclk_spi3, mout_sclk_spi4, mout_sclk_pcie_100,
	mout_sclk_mmc0_a = 3050, mout_sclk_mmc0_b,
	mout_sclk_mmc0_c, mout_sclk_mmc0_d,
	mout_sclk_mmc1_a, mout_sclk_mmc1_b,
	mout_sclk_mmc2_a, mout_sclk_mmc2_b,
	mout_sclk_ufsunipro,
	mout_sclk_jpeg_a = 3060,
	mout_sclk_jpeg_b, mout_sclk_jpeg_c,
	mout_sclk_isp_spi0, mout_sclk_isp_spi1,
	mout_sclk_isp_uart,
	mout_sclk_isp_sensor0, mout_sclk_isp_sensor1,
	mout_sclk_isp_sensor2,
	mout_sclk_hdmi_spdif = 3070,

	/* mif mux */
	mout_mem0_pll = 3071, mout_mem1_pll,
	mout_mfc_pll, mout_bus_pll,
	mout_aclk_mif_400,
	mout_clkm_phy_a = 3080, mout_clkm_phy_b,
	mout_clkm_phy_c,
	mout_clk2x_phy_a, mout_clk2x_phy_b,
	mout_clk2x_phy_c,
	mout_aclk_disp_333_a, mout_aclk_disp_333_b,
	mout_aclk_mifnm_200,
	mout_sclk_decon_eclk_a = 3090, mout_sclk_decon_eclk_b,
	mout_sclk_decon_eclk_c,
	mout_sclk_decon_vclk_a, mout_sclk_decon_vclk_b,
	mout_sclk_decon_vclk_c,
	mout_sclk_dsd_a = 3100, mout_sclk_dsd_b, mout_sclk_dsd_c,

	/* cpif mux */
	mout_phyclk_lli_tx0_symbol_user = 3103,
	mout_phyclk_lli_rx0_symbol_user,
	mout_mphy_pll, mout_phyclk_ufs_mphy_to_lli_user,
	mout_sclk_mphy,

	/* bus mux */
	mout_aclk_bus2_400_user = 3110,

	/* g2d mux */
	mout_aclk_g2d_400_user = 3111, mout_aclk_g2d_266_user,

	/* gscler mux */
	mout_aclk_gscl_333_user = 3113, mout_aclk_gscl_111_user,

	/* mscaler mux */
	mout_aclk_mscl_400_user = 3115, mout_sclk_jpeg_user,
	mout_sclk_jpeg,

	/* g3d mux */
	mout_g3d_pll = 3120,

	/* mfc mux */
	mout_aclk_mfc0_333_user = 3121, mout_aclk_mfc1_333_user,

	/* hevc mux */
	mout_aclk_hevc_400_user = 3123,

	/* disp mux */
	mout_disp_pll = 3124,
	mout_aclk_disp_333_user,
	mout_sclk_decon_eclk_user = 3130, mout_sclk_decon_eclk,
	mout_sclk_decon_vclk_user, mout_sclk_decon_vclk,
	mout_sclk_dsd_user,
	mout_phyclk_hdmiphy_pixel_clko_user,
	mout_phyclk_hdmiphy_tmds_clko_user,
	mout_phyclk_mipidphy_rxclkesc0_user,
	mout_phyclk_mipidphy_bitclkdiv8_user,

	/* fsys mux */
	mout_aclk_fsys_200_user = 3140, mout_sclk_usbdrd30_user,
	mout_sclk_mmc0_user, mout_sclk_mmc1_user, mout_sclk_mmc2_user,
	mout_sclk_ufsunipro_user, mout_ufs_pll_user,
	mout_phyclk_lli_mphy_to_ufs_user,
	mout_sclk_mphy_fsys,
	mout_sclk_pcie_100_user,
	mout_phyclk_usbdrd30_udrd30_phyclock = 3150,
	mout_phyclk_usbdrd30_udrd30_pipe_pclk,
	mout_phyclk_usbhost20_phy_freeclk,
	mout_phyclk_usbhost20_phy_phyclock,
	mout_phyclk_usbhost20_phy_clk48mohci,
	mout_phyclk_usbhost20_phy_hsic1,
	mout_sclk_usbhost30_user,
	mout_phyclk_usbhost30_uhost30_phyclock,
	mout_phyclk_usbhost30_uhost30_pipe_pclk,
	mout_phyclk_ufs_tx0_symbol_user = 3160,
	mout_phyclk_ufs_tx1_symbol_user,
	mout_phyclk_ufs_rx0_symbol_user,
	mout_phyclk_ufs_rx1_symbol_user,

	/* audio mux */
	mout_aud_pll_user = 3164,
	mout_sclk_aud_i2s = 3170,
	mout_sclk_aud_pcm = 3172,

	/* isp & cam mux */
	mout_phyclk_rxbyteclkhs0_s4 = 3175,
	mout_phyclk_rxbyteclkhs0_s2a,
	mout_sclk_isp_spi0_user = 3180, mout_sclk_isp_spi1_user,
	mout_sclk_isp_uart_user, mout_phyclk_rxbyteclkhs0_s2b,
	mout_aclk_cam0_552_user, mout_aclk_cam0_400_user,
	mout_aclk_cam0_333_user,
	mout_aclk_lite_a_a = 3190, mout_aclk_lite_a_b,
	mout_aclk_lite_b_a, mout_aclk_lite_b_b,
	mout_aclk_lite_d_a, mout_aclk_lite_d_b,
	mout_sclk_pixelasync_lite_c_init_a,
	mout_sclk_pixelasync_lite_c_init_b,
	mout_sclk_pixelasync_lite_c_a,
	mout_sclk_pixelasync_lite_c_b,
	mout_aclk_3aa0_a = 3200, mout_aclk_3aa0_b,
	mout_aclk_3aa1_a, mout_aclk_3aa1_b,
	mout_aclk_csis0_a, mout_aclk_csis0_b,
	mout_aclk_csis1_a, mout_aclk_csis1_b,
	mout_aclk_cam0_400 = 3210,
	mout_sclk_lite_freecnt_a = 3220,
	mout_sclk_lite_freecnt_b, mout_sclk_lite_freecnt_c,
	mout_aclk_cam1_552_user,
	mout_aclk_cam1_400_user, mout_aclk_cam1_333_user,
	mout_aclk_lite_c_a,
	mout_aclk_lite_c_b,
	mout_aclk_fd_a = 3230,
	mout_aclk_fd_b,
	mout_aclk_csis2_a,
	mout_aclk_csis2_b,
	mout_aclk_isp_400_user,
	mout_aclk_isp_dis_400_user,

	/* muxes for EVT1 */
	mout_mem0_pll_div2 = 3240, mout_mem1_pll_div2,
	mout_mfc_pll_div2, mout_bus_pll_div2,
	mout_aclk_mfc0_333_a = 3250, mout_aclk_mfc0_333_b,
	mout_aclk_mfc0_333_c, mout_aclk_mfc1_333_a,
	mout_aclk_mfc1_333_b, mout_aclk_mfc1_333_c,
	mout_sclk_decon_tv_eclk_a = 3260, mout_sclk_decon_tv_eclk_b,
	mout_sclk_decon_tv_eclk_c,
	mout_sclk_dsim0_a, mout_sclk_dsim0_b, mout_sclk_dsim0_c,
	mout_sclk_decon_tv_eclk_user, mout_sclk_decon_tv_eclk,
	mout_sclk_dsim0_user, mout_sclk_dsim0,

	mout_aclk_mfc_400_a = 3280, mout_aclk_mfc_400_b, mout_aclk_mfc_400_c, mout_aclk_mfc_400_user, mout_aclk_bus0_400,
	mout_sclk_decon_tv_vclk_a, mout_sclk_decon_tv_vclk_b, mout_sclk_decon_tv_vclk_c, mout_sclk_decon_tv_vclk_user, mout_sclk_dsim1_user,
	mout_sclk_decon_tv_vclk_a_disp = 3290, mout_sclk_decon_tv_vclk_b_disp, mout_sclk_decon_tv_vclk_c_disp, mout_sclk_dsim1_a_disp, mout_sclk_dsim1_b_disp,
	mout_sclk_dsim1_a, mout_sclk_dsim1_b, mout_sclk_dsim1_c, mout_phyclk_mipidphy1_rxclkesc0_user, mout_phyclk_mipidphy1_bitclkdiv8_user,

	/* DIV */
	/* eagle & kfc div */
	dout_egl1 = 4000, dout_egl2,
	dout_aclk_egl, dout_atclk_egl,
	dout_pclk_dbg_egl, dout_pclk_egl,
	dout_sclk_hpm_egl, dout_egl_pll,
	dout_kfc1 = 4010, dout_kfc2,
	dout_aclk_kfc, dout_atclk_kfc,
	dout_pclk_dbg_kfc, dout_pclk_kfc,
	dout_cntclk_kfc, dout_sclk_hpm_kfc,
	dout_kfc_pll,

	/* top div */
	dout_aclk_g2d_400 = 4020, dout_aclk_g2d_266,
	dout_aclk_gscl_333, dout_aclk_gscl_111,
	dout_aclk_mscl_400,
	dout_aclk_imem_266, dout_aclk_imem_200,
	dout_aclk_imem_sssx_266,
	dout_aclk_peris_66_a = 4030, dout_aclk_peris_66_b,
	dout_aclk_peric_66_a, dout_aclk_peric_66_b,
	dout_aclk_mfc0_333, dout_aclk_mfc1_333,
	dout_aclk_hevc_400, dout_aclk_fsys_200,
	dout_aclk_mfc_400, dout_aclk_g3d_400,
	dout_aclk_isp_400 = 4040, dout_aclk_isp_dis_400,
	dout_aclk_cam0_552, dout_aclk_cam0_400,
	dout_aclk_cam0_333, dout_aclk_cam1_552,
	dout_aclk_cam1_400, dout_aclk_cam1_333,
	dout_aclk_bus1_400, dout_aclk_bus0_400,
	dout_sclk_audio0 = 4050, dout_sclk_audio1,
	dout_sclk_pcm1, dout_sclk_i2s1,
	dout_sclk_spi0_a, dout_sclk_spi1_a, dout_sclk_spi2_a,
	dout_sclk_spi0_b, dout_sclk_spi1_b, dout_sclk_spi2_b,
	dout_sclk_uart0 = 4060, dout_sclk_uart1, dout_sclk_uart2,
	dout_sclk_slimbus, dout_usbdrd30, dout_usbhost30,
	dout_mmc0_a = 4070, dout_mmc0_b,
	dout_mmc1_a, dout_mmc1_b,
	dout_mmc2_a, dout_mmc2_b,
	dout_sclk_ufsunipro, dout_sclk_jpeg,
	dout_sclk_isp_spi0_a = 4080, dout_sclk_isp_spi0_b,
	dout_sclk_isp_spi1_a, dout_sclk_isp_spi1_b,
	dout_sclk_isp_uart,
	dout_sclk_spi3_a, dout_sclk_spi3_b, dout_sclk_spi4_a, dout_sclk_spi4_b,
	dout_sclk_isp_sensor0_a = 4090, dout_sclk_isp_sensor0_b,
	dout_sclk_isp_sensor1_a, dout_sclk_isp_sensor1_b,
	dout_sclk_isp_sensor2_a, dout_sclk_isp_sensor2_b,
	dout_sclk_pcie_100,

	/* mif div */
	dout_mem0_pll = 4100, dout_mem1_pll,
	dout_mfc_pll, dout_bus_pll,
	dout_mif_pre, dout_clk2x_phy,
	dout_aclk_mif_400, dout_aclk_drex0,
	dout_aclk_drex1, dout_sclk_hpm_mif,
	dout_aclk_mif_200 = 4110, dout_aclk_mifnm_200,
	dout_aclk_mifnd_133, dout_aclk_mif_133,
	dout_aclk_cpif_200, dout_aclk_bus2_400,
	dout_aclk_disp_333, dout_sclk_decon_eclk,
	dout_sclk_decon_vclk, dout_sclk_dsd,

	/* cpif div */
	dout_sclk_mphy = 4120,

	/* bus div */
	dout_pclk_bus1_133 = 4121, dout_pclk_bus2_133,

	/* mif 266 div */
	dout_aclk_mif_266 = 4123,

	/* g2d div */
	dout_pclk_g2d = 4130,

	/* mscler div */
	dout_pclk_mscl = 4131,

	/* g3d div */
	dout_aclk_g3d = 4132, dout_pclk_g3d, dout_sclk_hpm_g3d,

	/* mfc div */
	dout_pclk_mfc0 = 4135, dout_pclk_mfc1,

	/* hevc div */
	dout_pclk_hevc = 4137, dout_pclk_mfc,

	/* disp div */
	dout_pclk_disp = 4140,
	dout_sclk_decon_eclk_disp, dout_sclk_decon_vclk_disp,
	dout_sclk_decon_tv_eclk_disp, dout_sclk_dsim0_disp,

	/* aud div */
	dout_aud_ca5 = 4145, dout_aclk_aud, dout_pclk_dbg_aud,
	dout_sclk_aud_i2s = 4150,
	dout_sclk_aud_pcm = 4152,
	dout_sclk_aud_slimbus = 4154,
	dout_sclk_aud_uart = 4156,

	/* isp & cam div */
	dout_aclk_lite_a = 4160,
	dout_aclk_lite_b, dout_aclk_lite_d,
	dout_sclk_pixelasync_lite_c_init,
	dout_sclk_pixelasync_lite_c,
	dout_aclk_3aa0, dout_aclk_3aa1,
	dout_aclk_csis0, dout_aclk_csis1,
	dout_aclk_cam0_bus_400 = 4170, dout_aclk_cam0_200,
	dout_pclk_lite_a, dout_pclk_lite_b, dout_pclk_lite_d,
	dout_pclk_pixelasync_lite_c,
	dout_pclk_3aa0, dout_pclk_3aa1,
	dout_pclk_cam0_50 = 4180, dout_atclk_cam1,
	dout_pclk_dbg_cam1, dout_pclk_cam1_166,
	dout_pclk_cam1_83, dout_sclk_isp_mpwm,
	dout_aclk_lite_c, dout_aclk_fd,
	dout_aclk_csis2 = 4190, dout_pclk_lite_c,
	dout_pclk_fd,
	dout_aclk_isp_c_200, dout_aclk_isp_d_200,
	dout_pclk_isp, dout_pclk_isp_dis,

	/* dividers for EVT1*/
	dout_sclk_decon_tv_eclk = 4200, dout_sclk_dsim0, dout_cntclk_egl, dout_sclk_decon_tv_vclk, dout_sclk_decon_tv_vclk_disp,
	dout_sclk_dsim1, dout_sclk_dsim1_disp, dout_atclk_aud,

	/* mux gate */
	mgate_sclk_mmc2_b = 4210, mgate_sclk_mmc1_b,
	mgate_sclk_mmc0_d,

	/* CLKOUT */
	clkout = 4300,

	/* fixed clock */
	oscclk = 5000,
	phyclk_usbdrd30_udrd30_phyclock_phy,
	phyclk_usbdrd30_udrd30_pipe_pclk_phy,
	phyclk_usbhost20_phy_freeclk_phy,
	phyclk_usbhost20_phy_phyclock_phy,
	phyclk_usbhost20_phy_clk48mohci_phy,
	phyclk_usbhost20_phy_hsic1_phy,
	phyclk_usbhost30_uhost30_phyclock_phy,
	phyclk_usbhost30_uhost30_pipe_pclk_phy,
	phyclk_ufs_tx0_symbol_phy = 5010,
	phyclk_ufs_tx1_symbol_phy,
	phyclk_ufs_rx0_symbol_phy,
	phyclk_ufs_rx1_symbol_phy,
	phyclk_lli_tx0_symbol,
	phyclk_lli_rx0_symbol,
	phyclk_rxbyteclkhs0_s4_phy = 5020,
	phyclk_rxbyteclkhs0_s2a_phy,
	phyclk_rxbyteclkhs0_s2b_phy,
	phyclk_hdmiphy_pixel_clko_phy,
	phyclk_hdmiphy_tmds_clko_phy,
	phyclk_mipidphy_rxclkesc0_phy,
	phyclk_mipidphy_bitclkdiv8_phy,
	phyclk_ufs_mphy_to_lli = 5030,
	phyclk_lli_mphy_to_ufs_phy,
	ioclk_audiocdclk0,
	ioclk_audiocdclk1,
	ioclk_spdif_extclk,
	sclk_mphy_pll_26m,

	gate_freq_det_kfc_pll,
	gate_hpm_kfc,
	gate_freq_det_egl_pll,
	gate_hpm_egl_enable,
	gate_freq_det_aud_pll,
	gate_freq_det_isp_pll,

	/* additory gate clock for Exynos5433 */
	aclk_mfc_400 = 5050, aclk_mfc, aclk_mfcnd_400, aclk_xiu_mfcx, aclk_bts_mfc_0,
	aclk_bts_mfc_1, aclk_smmu_mfc_0, aclk_smmu_mfc_1, aclk_ppmu_mfc_0, aclk_ppmu_mfc_1,
	aclk_mfcnp_83 = 5060, aclk_ahb2apb_mfcp, pclk_mfc, pclk_sysreg_mfc, pclk_pmu_mfc,
	pclk_bts_mfc_0, pclk_bts_mfc_1, pclk_smmu_mfc_0, pclk_smmu_mfc_1, pclk_ppmu_mfc_0,
	pclk_ppmu_mfc_1 = 5070, mout_aclk_g3d_400, mout_aclk_g3d, sclk_hpm_g3d, aclk_async_g3d,
	sclk_decon_tv_vclk_disp, sclk_decon_tv_vclk, sclk_dsim1_disp, phyclk_mipidphy1_rxclkesc0, phyclk_mipidphy1_bitclkdiv8,
	atclk_aud = 5080, gate_decontv_vclk, gate_dsim1_clk, gate_sci,

	nr_clks,
};

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
static __initdata void *exynos5433_clk_regs[] = {
	EXYNOS5430_ENABLE_ACLK_TOP,
	EXYNOS5430_ENABLE_ACLK_MIF3,
	EXYNOS5430_ENABLE_ACLK_CPIF,
	EXYNOS5430_ENABLE_ACLK_FSYS0,
	EXYNOS5430_ENABLE_ACLK_FSYS1,
	EXYNOS5430_ENABLE_ACLK_PERIC,
	EXYNOS5430_ENABLE_PCLK_CPIF,
	EXYNOS5430_ENABLE_PCLK_FSYS,
	EXYNOS5430_ENABLE_PCLK_PERIC,
	EXYNOS5430_ENABLE_PCLK_PERIC1,
	EXYNOS5430_ENABLE_SCLK_CPIF,
	EXYNOS5430_ENABLE_SCLK_FSYS,
	EXYNOS5430_ENABLE_SCLK_PERIC,
	EXYNOS5430_ENABLE_SCLK_TOP_CAM1,
	EXYNOS5430_AUD_PLL_CON0,
	EXYNOS5430_SRC_SEL_EGL0,
	EXYNOS5430_SRC_SEL_EGL1,
	EXYNOS5430_SRC_SEL_EGL2,
	EXYNOS5430_SRC_SEL_KFC0,
	EXYNOS5430_SRC_SEL_KFC1,
	EXYNOS5430_SRC_SEL_KFC2,
	EXYNOS5430_SRC_SEL_BUS2,
	EXYNOS5430_SRC_SEL_FSYS0,
	EXYNOS5430_SRC_SEL_FSYS1,
	EXYNOS5430_SRC_SEL_FSYS2,
	EXYNOS5430_SRC_SEL_FSYS3,
	EXYNOS5430_SRC_SEL_FSYS4,
	EXYNOS5430_SRC_SEL_MIF0,
	EXYNOS5430_SRC_SEL_MIF1,
	EXYNOS5430_SRC_SEL_MIF2,
	EXYNOS5430_SRC_SEL_MIF3,
	EXYNOS5430_SRC_SEL_MIF4,
	EXYNOS5430_SRC_SEL_MIF5,
	EXYNOS5430_SRC_SEL_MIF6,
	EXYNOS5430_SRC_SEL_MIF7,
	EXYNOS5430_SRC_SEL_TOP0,
	EXYNOS5430_SRC_SEL_TOP1,
	EXYNOS5430_SRC_SEL_TOP2,
	EXYNOS5430_SRC_SEL_TOP3,
	EXYNOS5430_SRC_SEL_TOP4,
	EXYNOS5430_SRC_SEL_TOP_MSCL,
	EXYNOS5430_SRC_SEL_TOP_CAM1,
	EXYNOS5430_SRC_SEL_TOP_DISP,
	EXYNOS5430_SRC_SEL_TOP_FSYS0,
	EXYNOS5430_SRC_SEL_TOP_FSYS1,
	EXYNOS5430_SRC_SEL_TOP_PERIC0,
	EXYNOS5430_SRC_SEL_TOP_PERIC1,
	EXYNOS5430_SRC_SEL_CPIF0,
	EXYNOS5430_DIV_EGL0,
	EXYNOS5430_DIV_EGL1,
	EXYNOS5430_DIV_KFC0,
	EXYNOS5430_DIV_KFC1,
	EXYNOS5430_DIV_BUS1,
	EXYNOS5430_DIV_BUS2,
	EXYNOS5430_DIV_MIF1,
	EXYNOS5430_DIV_MIF2,
	EXYNOS5430_DIV_MIF3,
	EXYNOS5430_DIV_MIF4,
	EXYNOS5430_DIV_MIF5,
	EXYNOS5430_DIV_TOP0,
	EXYNOS5430_DIV_TOP1,
	EXYNOS5430_DIV_TOP2,
	EXYNOS5430_DIV_TOP3,
	EXYNOS5430_DIV_TOP4,
	EXYNOS5430_DIV_TOP_MSCL,
	EXYNOS5430_DIV_TOP_CAM10,
	EXYNOS5430_DIV_TOP_CAM11,
	EXYNOS5430_DIV_TOP_FSYS0,
	EXYNOS5430_DIV_TOP_FSYS1,
	EXYNOS5430_DIV_TOP_FSYS2,
	EXYNOS5430_DIV_TOP_PERIC0,
	EXYNOS5430_DIV_TOP_PERIC1,
	EXYNOS5430_DIV_TOP_PERIC2,
	EXYNOS5430_DIV_TOP_PERIC3,
	EXYNOS5430_DIV_TOP_PERIC4,
	EXYNOS5430_DIV_FSYS,
	EXYNOS5430_DIV_CPIF,
	/* CLK_SRC */
	EXYNOS5430_SRC_ENABLE_TOP0,
	EXYNOS5430_SRC_ENABLE_TOP2,
	EXYNOS5430_SRC_ENABLE_TOP3,
	EXYNOS5430_SRC_ENABLE_TOP4,
	EXYNOS5430_SRC_ENABLE_TOP_MSCL,
	EXYNOS5430_SRC_ENABLE_TOP_CAM1,
	EXYNOS5430_SRC_ENABLE_TOP_DISP,
	EXYNOS5430_SRC_ENABLE_FSYS0,
	EXYNOS5430_SRC_ENABLE_FSYS1,
	EXYNOS5430_SRC_ENABLE_FSYS2,
	EXYNOS5430_SRC_ENABLE_FSYS3,
	EXYNOS5430_SRC_ENABLE_FSYS4,
	/* CLK_ENALBE_IP */
	EXYNOS5430_ENABLE_IP_TOP,
	EXYNOS5430_EGL_STOPCTRL,
	EXYNOS5430_KFC_STOPCTRL,
	EXYNOS5430_ENABLE_IP_CPIF0,
	EXYNOS5430_ENABLE_IP_CPIF1,
	EXYNOS5430_ENABLE_IP_PERIC0,
	EXYNOS5430_ENABLE_IP_PERIC1,
	EXYNOS5430_ENABLE_IP_PERIC2,
	EXYNOS5430_ENABLE_IP_PERIS0,
	EXYNOS5430_ENABLE_IP_FSYS0,
	EXYNOS5430_ENABLE_IP_FSYS1,
	EXYNOS5430_ENABLE_IP_IMEM0,
	EXYNOS5430_ENABLE_IP_IMEM1,
	EXYNOS5430_ENABLE_IP_IMEM_SECURE_SMMU_SSS,
	EXYNOS5430_ENABLE_IP_IMEM_SECURE_SMMU_SLIMSSS,
	EXYNOS5430_ENABLE_IP_IMEM_SECURE_SMMU_RTIC,
	EXYNOS5430_ENABLE_IP_IMEM_SECURE_RTIC,
	EXYNOS5430_ENABLE_IP_PERIS_SECURE_CUSTOM_EFUSE,
	EXYNOS5430_ENABLE_IP_PERIS_SECURE_ANTIRBK_CNT,
	EXYNOS5430_ENABLE_IP_PERIS_SECURE_OTP_CON,
	/* PLL_FREQ_DET */
	EXYNOS5430_DIV_EGL_PLL_FREQ_DET,
	/* PLL */
	EXYNOS5430_AUD_PLL_LOCK,
	EXYNOS5430_AUD_PLL_CON0,
	EXYNOS5430_MPHY_PLL_CON0,
	EXYNOS5430_ISP_PLL_LOCK,
	EXYNOS5430_ISP_PLL_CON0,
};

/* EGL & KFC */
PNAME(mout_egl_pll_p)		= { "fin_pll", "fout_egl_pll" };
PNAME(mout_bus_pll_egl_user_p)	= { "fin_pll", "mout_bus_pll_div2" };
PNAME(mout_egl_p)		= { "mout_egl_pll", "mout_bus_pll_egl_user" };
PNAME(mout_kfc_pll_p)		= { "fin_pll", "fout_kfc_pll" };
PNAME(mout_bus_pll_kfc_user_p)	= { "fin_pll", "mout_bus_pll_div2" };
PNAME(mout_kfc_p)		= { "mout_kfc_pll", "mout_bus_pll_kfc_user" };

/* TOP */
PNAME(mout_bus_pll_user_p)	= { "fin_pll", "mout_bus_pll_div2" };
PNAME(mout_mfc_pll_user_p)	= { "fin_pll", "mout_mfc_pll_div2" };
PNAME(mout_mphy_pll_user_p)	= { "fin_pll", "mout_mphy_pll" };
PNAME(mout_isp_pll_p)		= { "fin_pll", "fout_isp_pll" };
PNAME(mout_aud_pll_p)		= { "fin_pll", "fout_aud_pll" };
PNAME(mout_aud_pll_user_top_p)	= { "fin_pll", "mout_aud_pll" };
PNAME(mout_aclk_gscl_333_p)	= { "mout_mfc_pll_user", "mout_bus_pll_user" };
PNAME(mout_aclk_hevc_400_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_mscl_400_a_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_mscl_400_b_p)	= { "mout_aclk_mscl_400_a", "mout_mphy_pll_user" };
PNAME(mout_aclk_g2d_400_a_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_g2d_400_b_p)	= { "mout_aclk_g2d_400_a", "mout_mphy_pll_user" };
PNAME(mout_aclk_mfc_400_a_p)	= { "mout_mfc_pll_user", "mout_isp_pll" };
PNAME(mout_aclk_mfc_400_b_p)	= { "mout_aclk_mfc_400_a", "mout_bus_pll_user" };
PNAME(mout_aclk_mfc_400_c_p)	= { "mout_aclk_mfc_400_b", "mout_mphy_pll_user" };
PNAME(mout_aclk_isp_400_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_isp_dis_400_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_cam1_552_a_p)	= { "mout_isp_pll", "mout_bus_pll_user" };
PNAME(mout_aclk_cam1_552_b_p)	= { "mout_aclk_cam1_552_a", "mout_mfc_pll_user" };
PNAME(mout_aclk_cam1_333_p)	= { "mout_mfc_pll_user", "mout_bus_pll_user" };
PNAME(mout_sclk_audio0_p)	= { "ioclk_audiocdclk0", "oscclk", "mout_aud_pll_user_top" };
PNAME(mout_sclk_audio1_p)	= { "ioclk_audiocdclk1", "oscclk", "mout_aud_pll_user_top" };
PNAME(mout_sclk_spi0_p)		= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_spi1_p)		= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_spi2_p)		= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_spi3_p)		= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_spi4_p)		= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_uart0_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_uart1_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_uart2_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_spdif_p)	= { "dout_sclk_audio0", "dout_sclk_audio1", "ioclk_spdif_extclk" };
PNAME(mout_sclk_usbdrd30_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_usbhost30_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_mmc0_a_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_mmc0_b_p)	= { "mout_sclk_mmc0_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_mmc0_c_p)	= { "mout_sclk_mmc0_b", "mout_mphy_pll_user" };
PNAME(mout_sclk_mmc0_d_p)	= { "mout_sclk_mmc0_c", "mout_isp_pll" };
PNAME(mout_sclk_mmc1_a_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_mmc1_b_p)	= { "mout_sclk_mmc1_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_mmc2_a_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_mmc2_b_p)	= { "mout_sclk_mmc2_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_ufsunipro_p)	= { "oscclk", "mout_mphy_pll_user" };
PNAME(mout_sclk_jpeg_a_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_jpeg_b_p)	= { "mout_sclk_jpeg_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_jpeg_c_p)	= { "mout_sclk_jpeg_b", "mout_mphy_pll_user" };
PNAME(mout_sclk_isp_spi0_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_isp_spi1_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_isp_uart_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_isp_sensor0_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_isp_sensor1_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_isp_sensor2_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_hdmi_spdif_p)	= { "dout_sclk_audio1", "ioclk_spdif_extclk" };
PNAME(mout_sclk_pcie_100_user_p) = { "oscclk", "sclk_pcie_100_fsys" };

/* MIF */
PNAME(mout_mem0_pll_p)		= { "fin_pll", "fout_mem0_pll" };
PNAME(mout_mem1_pll_p)		= { "fin_pll", "fout_mem1_pll" };
PNAME(mout_mfc_pll_p)		= { "fin_pll", "fout_mfc_pll" };
PNAME(mout_bus_pll_p)		= { "fin_pll", "fout_bus_pll" };
PNAME(mout_mem0_pll_div2_p)	= { "mout_mem0_pll", "dout_mem0_pll" };
PNAME(mout_mem1_pll_div2_p)	= { "mout_mem1_pll", "dout_mem1_pll" };
PNAME(mout_mfc_pll_div2_p)	= { "mout_mfc_pll", "dout_mfc_pll" };
PNAME(mout_bus_pll_div2_p)	= { "mout_bus_pll", "dout_bus_pll" };
PNAME(mout_clkm_phy_a_p)	= { "mout_bus_pll_div2", "mout_mfc_pll_div2" };
PNAME(mout_clkm_phy_b_p)	= { "mout_mem1_pll_div2", "mout_clkm_phy_a" };
PNAME(mout_clkm_phy_c_p)	= { "mout_mem0_pll_div2", "mout_clkm_phy_b" };
PNAME(mout_clk2x_phy_a_p)	= { "mout_bus_pll_div2", "mout_mfc_pll_div2" };
PNAME(mout_clk2x_phy_b_p)	= { "mout_mem1_pll_div2", "mout_clk2x_phy_a" };
PNAME(mout_clk2x_phy_c_p)	= { "mout_mem0_pll_div2", "mout_clk2x_phy_b" };
PNAME(mout_aclk_mif_400_p)	= { "mout_mem1_pll_div2", "mout_bus_pll_div2", };
PNAME(mout_aclk_mifnm_200_p)	= { "mout_mem0_pll_div2", "dout_mif_pre" };
PNAME(mout_aclk_disp_333_a_p)	= { "mout_mfc_pll_div2", "mout_mphy_pll" };
PNAME(mout_aclk_disp_333_b_p)	= { "mout_aclk_disp_333_a", "mout_bus_pll_div2" };
PNAME(mout_sclk_decon_eclk_a_p) = { "oscclk", "mout_bus_pll_div2" };
PNAME(mout_sclk_decon_eclk_b_p) = { "mout_sclk_decon_eclk_a", "mout_mfc_pll_div2" };
PNAME(mout_sclk_decon_eclk_c_p) = { "mout_sclk_decon_eclk_b", "mout_mphy_pll" };
PNAME(mout_sclk_decon_vclk_a_p) = { "oscclk", "mout_bus_pll_div2" };
PNAME(mout_sclk_decon_vclk_b_p) = { "mout_sclk_decon_vclk_a", "dout_mfc_pll_div2" };
PNAME(mout_sclk_decon_vclk_c_p) = { "mout_sclk_decon_vclk_b", "mout_mphy_pll" };
PNAME(mout_sclk_decon_tv_eclk_a_p) = { "oscclk", "mout_bus_pll_div2" };
PNAME(mout_sclk_decon_tv_eclk_b_p) = { "mout_sclk_decon_tv_eclk_a", "mout_mfc_pll_div2" };
PNAME(mout_sclk_decon_tv_eclk_c_p) = { "mout_sclk_decon_tv_eclk_b", "mout_mphy_pll" };
PNAME(mout_sclk_dsd_a_p)	= { "oscclk", "mout_mfc_pll_div2" };
PNAME(mout_sclk_dsd_b_p)	= { "mout_sclk_dsd_a", "mout_mphy_pll" };
PNAME(mout_sclk_dsd_c_p)	= { "mout_sclk_dsd_b", "mout_bus_pll_div2" };
PNAME(mout_sclk_dsim0_a_p)	= { "oscclk", "mout_bus_pll_div2" };
PNAME(mout_sclk_dsim0_b_p)	= { "mout_sclk_dsim0_a", "mout_mfc_pll_div2" };
PNAME(mout_sclk_dsim0_c_p)	= { "mout_sclk_dsim0_b", "mout_mphy_pll" };

/* CPIF */
PNAME(mout_phyclk_lli_tx0_symbol_user_p) = { "oscclk", "phyclk_lli_tx0_symbol", };
PNAME(mout_phyclk_lli_rx0_symbol_user_p) = { "oscclk", "phyclk_lli_rx0_symbol", };
PNAME(mout_mphy_pll_p)		= { "fin_pll", "fout_mphy_pll", };
PNAME(mout_phyclk_ufs_mphy_to_lli_user_p) = { "oscclk", "phyclk_ufs_mphy_to_lli", };
PNAME(mout_sclk_mphy_p)		= { "dout_sclk_mphy", "mout_phyclk_ufs_mphy_to_lli_user", };

/* BUS */
PNAME(mout_aclk_bus2_400_user_p) = { "oscclk", "aclk_bus2_400" };
PNAME(mout_aclk_g2d_400_user_p) = { "oscclk", "aclk_g2d_400" };
PNAME(mout_aclk_g2d_266_user_p) = { "oscclk", "aclk_g2d_266" };

/* GSCL */
PNAME(mout_aclk_gscl_333_user_p) = { "oscclk", "aclk_gscl_333" };
PNAME(mout_aclk_gscl_111_user_p) = { "oscclk", "aclk_gscl_111" };

/* MSCL */
PNAME(mout_aclk_mscl_400_user_p) = { "oscclk", "aclk_mscl_400" };
PNAME(mout_sclk_jpeg_user_p)	= { "oscclk", "sclk_jpeg_mscl" };
PNAME(mout_sclk_jpeg_p)		= { "mout_sclk_jpeg_user", "mout_aclk_mscl_400_user" };

/* G3D */
PNAME(mout_g3d_pll_p)		= { "fin_pll", "fout_g3d_pll" };
PNAME(mout_aclk_g3d_400_p)	= { "mout_g3d_pll", "aclk_g3d_400" };
/* MFC */
PNAME(mout_aclk_mfc_400_user_p) = { "oscclk", "aclk_mfc_400" };
/* HEVC */
PNAME(mout_aclk_hevc_400_user_p) = { "oscclk", "aclk_hevc_400" };

/* DISP1 */
PNAME(mout_disp_pll_p)		= { "fin_pll", "fout_disp_pll" };
PNAME(mout_aclk_disp_333_user_p) = { "oscclk", "aclk_disp_333" };
PNAME(mout_sclk_decon_eclk_user_p) = { "oscclk", "sclk_decon_eclk_disp" };
PNAME(mout_sclk_decon_eclk_p) = { "mout_disp_pll", "mout_sclk_decon_eclk_user" };
PNAME(mout_sclk_decon_vclk_user_p) = { "oscclk", "sclk_decon_vclk_disp" };
PNAME(mout_sclk_decon_vclk_p) = { "mout_disp_pll", "mout_sclk_decon_vclk_user" };
PNAME(mout_sclk_decon_tv_eclk_user_p) = { "oscclk", "sclk_decon_tv_eclk_disp" };
PNAME(mout_sclk_decon_tv_eclk_p) = { "mout_disp_pll", "mout_sclk_decon_tv_eclk_user" };
PNAME(mout_sclk_dsd_user_p)	= { "oscclk", "sclk_dsd_disp" };
PNAME(mout_sclk_dsim0_user_p) = { "oscclk", "sclk_dsim0_disp" };
PNAME(mout_sclk_dsim0_p) = { "mout_disp_pll", "mout_sclk_dsim0_user" };
PNAME(mout_phyclk_hdmiphy_pixel_clko_user_p) = { "oscclk", "phyclk_hdmiphy_pixel_clko_phy" };
PNAME(mout_phyclk_hdmiphy_tmds_clko_user_p) = { "oscclk", "phyclk_hdmiphy_tmds_clko_phy" };
PNAME(mout_phyclk_mipidphy_rxclkesc0_user_p) = { "oscclk", "phyclk_mipidphy_rxclkesc0_phy" };
PNAME(mout_phyclk_mipidphy_bitclkdiv8_user_p) = { "oscclk", "phyclk_mipidphy_bitclkdiv8_phy" };


/* FSYS */
PNAME(mout_aclk_fsys_200_user_p) = { "oscclk", "aclk_fsys_200" };
PNAME(mout_sclk_usbdrd30_user_p) = { "oscclk", "sclk_usbdrd30_fsys" };
PNAME(mout_sclk_usbhost30_user_p) = { "oscclk", "sclk_usbhost30_fsys" };
PNAME(mout_sclk_mmc0_user_p) = { "oscclk", "sclk_mmc0_fsys" };
PNAME(mout_sclk_mmc1_user_p) = { "oscclk", "sclk_mmc1_fsys" };
PNAME(mout_sclk_mmc2_user_p) = { "oscclk", "sclk_mmc2_fsys" };
PNAME(mout_sclk_ufsunipro_user_p) = { "oscclk", "sclk_ufsunipro_fsys" };
PNAME(mout_ufs_pll_user_p) = { "oscclk", "sclk_ufs_pll" };
PNAME(mout_phyclk_lli_mphy_to_ufs_user_p) = { "oscclk", "phyclk_lli_mphy_to_ufs_phy" };
PNAME(mout_sclk_mphy_fsys_p) = { "mout_ufs_pll_user", "mout_phyclk_lli_mphy_to_ufs_user" };
PNAME(mout_phyclk_usbdrd30_udrd30_phyclock_p) = { "oscclk", "phyclk_usbdrd30_udrd30_phyclock_phy" };
PNAME(mout_phyclk_usbdrd30_udrd30_pipe_pclk_p) = { "oscclk", "phyclk_usbdrd30_udrd30_pipe_pclk_phy" };
PNAME(mout_phyclk_usbhost20_phy_freeclk_p) = { "oscclk", "phyclk_usbhost20_phy_freeclk_phy" };
PNAME(mout_phyclk_usbhost20_phy_phyclock_p) = { "oscclk", "phyclk_usbhost20_phy_phyclock_phy" };
PNAME(mout_phyclk_usbhost20_phy_clk48mohci_p) = { "oscclk", "phyclk_usbhost20_phy_clk48mohci_phy" };
PNAME(mout_phyclk_usbhost30_uhost30_phyclock_p) = { "oscclk", "phyclk_usbhost30_uhost30_phyclock_phy" };
PNAME(mout_phyclk_usbhost30_uhost30_pipe_pclk_p) = { "oscclk", "phyclk_usbhost30_uhost30_pipe_pclk_phy" };
PNAME(mout_phyclk_usbhost20_phy_hsic1_p) = { "oscclk", "phyclk_usbhost20_phy_hsic1_phy" };
PNAME(mout_phyclk_ufs_tx0_symbol_user_p) = { "oscclk", "phyclk_ufs_tx0_symbol_phy" };
PNAME(mout_phyclk_ufs_tx1_symbol_user_p) = { "oscclk", "phyclk_ufs_tx1_symbol_phy" };
PNAME(mout_phyclk_ufs_rx0_symbol_user_p) = { "oscclk", "phyclk_ufs_rx0_symbol_phy" };
PNAME(mout_phyclk_ufs_rx1_symbol_user_p) = { "oscclk", "phyclk_ufs_rx1_symbol_phy" };
PNAME(mout_sclk_pcie_100_p) = { "oscclk", "mout_bus_pll_user" };

/* AUD0 */
PNAME(mout_aud_pll_user_p)	= { "fin_pll", "fout_aud_pll" };
PNAME(mout_sclk_aud_i2s_p)	= { "mout_aud_pll_user", "ioclk_audiocdclk0" };
PNAME(mout_sclk_aud_pcm_p)	= { "mout_aud_pll_user", "ioclk_audiocdclk0" };

/* CAM0 */
PNAME(mout_phyclk_rxbyteclkhs0_s4_p) = { "oscclk", "phyclk_rxbyteclkhs0_s4_phy" };
PNAME(mout_phyclk_rxbyteclkhs0_s2a_p) = { "oscclk", "phyclk_rxbyteclkhs0_s2a_phy" };

PNAME(mout_aclk_cam0_552_user_p) = { "oscclk", "aclk_cam0_552" };
PNAME(mout_aclk_cam0_400_user_p) = { "oscclk", "aclk_cam0_400" };
PNAME(mout_aclk_cam0_333_user_p) = { "oscclk", "aclk_cam0_333" };
PNAME(mout_aclk_lite_a_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_aclk_lite_a_b_p) = { "mout_aclk_lite_a_a", "mout_aclk_cam0_333_user" };
PNAME(mout_aclk_lite_b_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_aclk_lite_b_b_p) = { "mout_aclk_lite_b_a", "mout_aclk_cam0_333_user" };
PNAME(mout_aclk_lite_d_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_aclk_lite_d_b_p) = { "mout_aclk_lite_d_a", "mout_aclk_cam0_333_user" };
PNAME(mout_sclk_pixelasync_lite_c_init_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_sclk_pixelasync_lite_c_init_b_p) = { "mout_sclk_pixelasync_lite_c_init_a", "mout_aclk_cam0_333_user" };
PNAME(mout_sclk_pixelasync_lite_c_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_sclk_pixelasync_lite_c_b_p) = { "mout_sclk_pixelasync_lite_c_a", "mout_aclk_cam0_333_user" };
PNAME(mout_aclk_3aa0_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_aclk_3aa0_b_p) = { "mout_aclk_3aa0_a", "mout_aclk_cam0_333_user" };
PNAME(mout_aclk_3aa1_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_aclk_3aa1_b_p) = { "mout_aclk_3aa1_a", "mout_aclk_cam0_333_user" };
PNAME(mout_aclk_csis0_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_aclk_csis0_b_p) = { "mout_aclk_csis0_a", "mout_aclk_cam0_333_user" };
PNAME(mout_aclk_csis1_a_p) = { "mout_aclk_cam0_552_user", "mout_aclk_cam0_400_user" };
PNAME(mout_aclk_csis1_b_p) = { "mout_aclk_csis1_a", "mout_aclk_cam0_333_user" };
PNAME(mout_aclk_cam0_400_p) = { "mout_aclk_cam0_400_user", "mout_aclk_cam0_333_user" };
PNAME(mout_sclk_lite_freecnt_a_p) = { "dout_pclk_lite_a", "dout_pclk_lite_b" };
PNAME(mout_sclk_lite_freecnt_b_p) = { "mout_sclk_lite_freecnt_a", "dout_pclk_pixelasync_lite_c" };
PNAME(mout_sclk_lite_freecnt_c_p) = { "mout_sclk_lite_freecnt_b", "dout_pclk_lite_d" };

/* CAM1 */
PNAME(mout_sclk_isp_spi0_user_p) = { "oscclk", "sclk_isp_spi0_cam1" };
PNAME(mout_sclk_isp_spi1_user_p) = { "oscclk", "sclk_isp_spi1_cam1" };
PNAME(mout_sclk_isp_uart_user_p) = { "oscclk", "sclk_isp_uart_cam1" };
PNAME(mout_phyclk_rxbyteclkhs0_s2b_p) = { "oscclk", "phyclk_rxbyteclkhs0_s2b_phy" };

PNAME(mout_aclk_cam1_552_user_p) = { "oscclk", "aclk_cam1_552" };
PNAME(mout_aclk_cam1_400_user_p) = { "oscclk", "aclk_cam1_400" };
PNAME(mout_aclk_cam1_333_user_p) = { "oscclk", "aclk_cam1_333" };
PNAME(mout_aclk_lite_c_a_p) = { "mout_aclk_cam1_552_user", "mout_aclk_cam1_400_user" };
PNAME(mout_aclk_lite_c_b_p) = { "mout_aclk_lite_c_a", "mout_aclk_cam1_333_user" };
PNAME(mout_aclk_fd_a_p) = { "mout_aclk_cam1_552_user", "mout_aclk_cam1_400_user" };
PNAME(mout_aclk_fd_b_p) = { "mout_aclk_fd_a", "mout_aclk_cam1_333_user" };
PNAME(mout_aclk_csis2_a_p) = { "mout_aclk_cam1_552_user", "mout_aclk_cam1_400_user" };
PNAME(mout_aclk_csis2_b_p) = { "mout_aclk_csis2_a", "mout_aclk_cam1_333_user" };

PNAME(mout_aclk_isp_400_user_p) = { "oscclk", "aclk_isp_400" };
PNAME(mout_aclk_isp_dis_400_user_p) = { "oscclk", "aclk_isp_dis_400" };

/* added for exynos5433 */
PNAME(mout_aclk_bus0_400_p) = { "mout_bus_pll_user", "mout_mphy_pll_user" };
PNAME(mout_sclk_decon_tv_vclk_a_p) = { "fin_pll", "mout_bus_pll_div2" };
PNAME(mout_sclk_decon_tv_vclk_b_p) = { "mout_sclk_decon_tv_vclk_a", "mout_mfc_pll_div2" };
PNAME(mout_sclk_decon_tv_vclk_c_p) = { "mout_sclk_decon_tv_vclk_b", "mout_mphy_pll" };
PNAME(mout_sclk_decon_tv_vclk_a_disp_p) = { "mout_disp_pll", "mout_sclk_decon_vclk_user" };
PNAME(mout_sclk_decon_tv_vclk_b_disp_p) = { "mout_sclk_decon_tv_vclk_a_disp", "mout_sclk_decon_tv_vclk_user" };
PNAME(mout_sclk_decon_tv_vclk_c_disp_p) = { "mout_phyclk_hdmiphy_pixel_clko_user", "mout_sclk_decon_tv_vclk_b_disp" };
PNAME(mout_sclk_decon_tv_vclk_user_p) = { "fin_pll", "sclk_decon_tv_vclk_disp" };

PNAME(mout_sclk_dsim1_a_p) = { "fin_pll", "mout_bus_pll_div2" };
PNAME(mout_sclk_dsim1_b_p) = { "mout_sclk_dsim1_a", "mout_mfc_pll_div2" };
PNAME(mout_sclk_dsim1_c_p) = { "mout_sclk_dsim1_b", "mout_mphy_pll" };
PNAME(mout_sclk_dsim1_a_disp_p) = { "mout_disp_pll", "mout_sclk_dsim0_user" };
PNAME(mout_sclk_dsim1_b_disp_p) = { "mout_sclk_dsim1_a_disp", "mout_sclk_dsim1_user" };
PNAME(mout_sclk_dsim1_user_p) = { "fin_pll", "sclk_dsim1_disp" };

PNAME(mout_phyclk_mipidphy1_rxclkesc0_user_p) = { "fin_pll", "phyclk_mipidphy1_rxclkesc0_phy" };
PNAME(mout_phyclk_mipidphy1_bitclkdiv8_user_p) = { "fin_pll", "phyclk_mipidphy1_bitclkdiv8_phy" };

/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos5433_fixed_rate_ext_clks[] __initdata = {
	FRATE(fin_pll, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

#define CFRATE(_id, pname, f, rate) \
		FRATE(_id, #_id, pname, f, rate)
struct samsung_fixed_rate_clock exynos5433_fixed_rate_clks[] __initdata = {
	CFRATE(oscclk, NULL, CLK_IS_ROOT, 24000000),
	CFRATE(phyclk_usbdrd30_udrd30_phyclock_phy, NULL, CLK_IS_ROOT, 60000000),
	CFRATE(phyclk_usbdrd30_udrd30_pipe_pclk_phy, NULL, CLK_IS_ROOT, 125000000),
	CFRATE(phyclk_usbhost20_phy_freeclk_phy, NULL, CLK_IS_ROOT, 60000000),
	CFRATE(phyclk_usbhost20_phy_phyclock_phy, NULL, CLK_IS_ROOT, 60000000),
	CFRATE(phyclk_usbhost20_phy_clk48mohci_phy, NULL, CLK_IS_ROOT, 48000000),
	CFRATE(phyclk_usbhost20_phy_hsic1_phy, NULL, CLK_IS_ROOT, 60000000),
	CFRATE(phyclk_usbhost30_uhost30_phyclock_phy, NULL, CLK_IS_ROOT, 60000000),
	CFRATE(phyclk_usbhost30_uhost30_pipe_pclk_phy, NULL, CLK_IS_ROOT, 125000000),
	CFRATE(phyclk_ufs_tx0_symbol_phy, NULL, CLK_IS_ROOT, 300000000),
	CFRATE(phyclk_ufs_tx1_symbol_phy, NULL, CLK_IS_ROOT, 300000000),
	CFRATE(phyclk_ufs_rx0_symbol_phy, NULL, CLK_IS_ROOT, 300000000),
	CFRATE(phyclk_ufs_rx1_symbol_phy, NULL, CLK_IS_ROOT, 300000000),
	CFRATE(phyclk_lli_tx0_symbol, NULL, CLK_IS_ROOT, 150000000),
	CFRATE(phyclk_lli_rx0_symbol, NULL, CLK_IS_ROOT, 150000000),
	CFRATE(phyclk_rxbyteclkhs0_s4_phy, NULL, CLK_IS_ROOT, 188000000),
	CFRATE(phyclk_rxbyteclkhs0_s2a_phy, NULL, CLK_IS_ROOT, 188000000),
	CFRATE(phyclk_rxbyteclkhs0_s2b_phy, NULL, CLK_IS_ROOT, 188000000),
	CFRATE(phyclk_hdmiphy_pixel_clko_phy, NULL, CLK_IS_ROOT, 166000000),
	CFRATE(phyclk_hdmiphy_tmds_clko_phy, NULL, CLK_IS_ROOT, 250000000),
	CFRATE(phyclk_mipidphy_rxclkesc0_phy, NULL, CLK_IS_ROOT, 20000000),
	CFRATE(phyclk_mipidphy_bitclkdiv8_phy, NULL, CLK_IS_ROOT, 188000000),
	FRATE(none, "phyclk_mipidphy1_rxclkesc0_phy", NULL, CLK_IS_ROOT, 10000000),
	FRATE(none, "phyclk_mipidphy1_bitclkdiv8_phy", NULL, CLK_IS_ROOT, 188000000),

	CFRATE(phyclk_ufs_mphy_to_lli, NULL, CLK_IS_ROOT, 26000000),
	CFRATE(phyclk_lli_mphy_to_ufs_phy, NULL, CLK_IS_ROOT, 26000000),
	CFRATE(sclk_mphy_pll_26m, NULL, CLK_IS_ROOT, 26000000),

	CFRATE(ioclk_audiocdclk0, NULL, CLK_IS_ROOT, 83400000),
	CFRATE(ioclk_audiocdclk1, NULL, CLK_IS_ROOT, 83400000),
	CFRATE(ioclk_spdif_extclk, NULL, CLK_IS_ROOT, 36864000),
};

struct samsung_fixed_factor_clock exynos5433_fixed_factor_clks[] __initdata = {
	FFACTOR(none, "dout_mem0_pll", "mout_mem0_pll", 1, 2, 0),
	FFACTOR(none, "dout_mem1_pll", "mout_mem1_pll", 1, 2, 0),
	FFACTOR(none, "dout_mfc_pll", "mout_mfc_pll", 1, 2, 0),
	FFACTOR(none, "dout_bus_pll", "mout_bus_pll", 1, 2, 0),
};

#define CMX(_id, cname, pnames, o, s, w) \
		MUX(_id, cname, pnames, (unsigned long)o, s, w)
#define CMUX(_id, o, s, w) \
		MUX(_id, #_id, _id##_p, (unsigned long)o, s, w)
#define CMX_A(_id, o, s, w, a) \
		MUX_A(_id, #_id, _id##_p, (unsigned long)o, s, w, a)
#define CMX_STAT(_id, o, s, w, so, ss, sw) \
		MUX_STAT(_id, #_id, _id##_p, (unsigned long)o, s, w, (unsigned long)so, ss, sw)
struct samsung_mux_clock exynos5433_mux_clks[] __initdata = {
	/* EAGLE */
	CMX_S_A(mout_egl_pll, EXYNOS5430_SRC_SEL_EGL0, 0, 1, "mout_egl_pll", EXYNOS5430_SRC_STAT_EGL0, 0, 3),
	CMX_S_A(mout_bus_pll_egl_user, EXYNOS5430_SRC_SEL_EGL1, 0, 1, "mout_bus_pll_egl_user", EXYNOS5430_SRC_STAT_EGL1, 0, 3),
	CMX_S_A(mout_egl, EXYNOS5430_SRC_SEL_EGL2, 0, 1, "mout_egl", EXYNOS5430_SRC_STAT_EGL2, 0, 3),

	/* KFC */
	CMX_S_A(mout_kfc_pll, EXYNOS5430_SRC_SEL_KFC0, 0, 1, "mout_kfc_pll", EXYNOS5430_SRC_STAT_KFC0, 0, 3),
	CMX_S_A(mout_bus_pll_kfc_user, EXYNOS5430_SRC_SEL_KFC1, 0, 1, "mout_bus_pll_kfc_user", EXYNOS5430_SRC_STAT_KFC1, 0, 3),
	CMX_S_A(mout_kfc, EXYNOS5430_SRC_SEL_KFC2, 0, 1, "mout_kfc", EXYNOS5430_SRC_STAT_KFC2, 0, 3),

	/* TOP */
	CMX_S_A(mout_bus_pll_user, EXYNOS5430_SRC_SEL_TOP1, 0, 1, "mout_bus_pll_user", EXYNOS5430_SRC_STAT_TOP1, 0, 3),
	CMX_S_A(mout_mfc_pll_user, EXYNOS5430_SRC_SEL_TOP1, 4, 1, "mout_mfc_pll_user", EXYNOS5430_SRC_STAT_TOP1, 4, 3),
	CMX_S_A(mout_mphy_pll_user, EXYNOS5430_SRC_SEL_TOP1, 8, 1, "mout_mphy_pll_user", EXYNOS5430_SRC_STAT_TOP1, 8, 3),
	CMX_S_A(mout_isp_pll, EXYNOS5430_SRC_SEL_TOP0, 0, 1, "mout_isp_pll", EXYNOS5430_SRC_STAT_TOP0, 0, 3),
	CMX_STAT(mout_aud_pll, EXYNOS5430_SRC_SEL_TOP0, 4, 1, EXYNOS5430_SRC_STAT_TOP0, 4, 3),
	CMX_STAT(mout_aud_pll_user_top, EXYNOS5430_SRC_SEL_TOP1, 12, 1, EXYNOS5430_SRC_STAT_TOP1, 12, 3),
	CMX_STAT(mout_aclk_gscl_333, EXYNOS5430_SRC_SEL_TOP3, 8, 1, EXYNOS5430_SRC_STAT_TOP3, 8, 3),
	CMX_STAT(mout_aclk_hevc_400, EXYNOS5430_SRC_SEL_TOP2, 28, 1, EXYNOS5430_SRC_STAT_TOP2, 28, 3),
	CMX_S_A(mout_aclk_mscl_400_a, EXYNOS5430_SRC_SEL_TOP3, 12, 1, "mout_aclk_mscl_400_a", EXYNOS5430_SRC_STAT_TOP3, 12, 3),
	CMX_STAT(mout_aclk_mscl_400_b, EXYNOS5430_SRC_SEL_TOP3, 16, 1, EXYNOS5430_SRC_STAT_TOP3, 16, 3),
	CMX_S_A(mout_aclk_g2d_400_a, EXYNOS5430_SRC_SEL_TOP3, 0, 1, "mout_aclk_g2d_400_a", EXYNOS5430_SRC_STAT_TOP3, 0, 3),
	CMX_STAT(mout_aclk_g2d_400_b, EXYNOS5430_SRC_SEL_TOP3, 4, 1, EXYNOS5430_SRC_STAT_TOP3, 4, 3),
	CMX_S_A(mout_aclk_isp_400, EXYNOS5430_SRC_SEL_TOP2, 0, 1, "mout_aclk_isp_400", EXYNOS5430_SRC_STAT_TOP2, 0, 3),
	CMX_S_A(mout_aclk_isp_dis_400, EXYNOS5430_SRC_SEL_TOP2, 4, 1, "mout_aclk_isp_dis_400", EXYNOS5430_SRC_STAT_TOP2, 4, 3),
	CMX_S_A(mout_aclk_cam1_552_a, EXYNOS5430_SRC_SEL_TOP2, 8, 1, "mout_aclk_cam1_552_a", EXYNOS5430_SRC_STAT_TOP2, 8, 3),
	CMX_S_A(mout_aclk_cam1_552_b, EXYNOS5430_SRC_SEL_TOP2, 12, 1, "mout_aclk_cam1_552_b", EXYNOS5430_SRC_STAT_TOP2, 12, 3),
	CMX_STAT(mout_aclk_cam1_333, EXYNOS5430_SRC_SEL_TOP2, 16, 1, EXYNOS5430_SRC_STAT_TOP2, 16, 3),
	CMUX(mout_sclk_audio0, EXYNOS5430_SRC_SEL_TOP_PERIC1, 0, 2),
	CMUX(mout_sclk_audio1, EXYNOS5430_SRC_SEL_TOP_PERIC1, 4, 2),
	CMX_STAT(mout_sclk_spi0, EXYNOS5430_SRC_SEL_TOP_PERIC0, 0, 1, EXYNOS5430_SRC_STAT_TOP_PERIC0, 0, 3),
	CMX_STAT(mout_sclk_spi1, EXYNOS5430_SRC_SEL_TOP_PERIC0, 4, 1, EXYNOS5430_SRC_STAT_TOP_PERIC0, 4, 3),
	CMX_STAT(mout_sclk_spi2, EXYNOS5430_SRC_SEL_TOP_PERIC0, 8, 1, EXYNOS5430_SRC_STAT_TOP_PERIC0, 8, 3),
	CMX_STAT(mout_sclk_uart0, EXYNOS5430_SRC_SEL_TOP_PERIC0, 12, 1, EXYNOS5430_SRC_STAT_TOP_PERIC0, 12, 3),
	CMX_STAT(mout_sclk_uart1, EXYNOS5430_SRC_SEL_TOP_PERIC0, 16, 1, EXYNOS5430_SRC_STAT_TOP_PERIC0, 16, 3),
	CMX_STAT(mout_sclk_uart2, EXYNOS5430_SRC_SEL_TOP_PERIC0, 20, 1, EXYNOS5430_SRC_STAT_TOP_PERIC0, 20, 3),
	CMX_A(mout_sclk_spdif, EXYNOS5430_SRC_SEL_TOP_PERIC1, 12, 2, "sclk_spdif"),
	CMX_STAT(mout_sclk_usbdrd30, EXYNOS5430_SRC_SEL_TOP_FSYS1, 0, 1, EXYNOS5430_SRC_STAT_TOP_FSYS1, 0, 3),
	CMX_STAT(mout_sclk_usbhost30, EXYNOS5430_SRC_SEL_TOP_FSYS1, 4, 1, EXYNOS5430_SRC_STAT_TOP_FSYS1, 4, 3),
	CMX_STAT(mout_sclk_mmc0_a, EXYNOS5430_SRC_SEL_TOP_FSYS0, 0, 1, EXYNOS5430_SRC_STAT_TOP_FSYS0, 0, 3),
	CMX_STAT(mout_sclk_mmc0_b, EXYNOS5430_SRC_SEL_TOP_FSYS0, 4, 1, EXYNOS5430_SRC_STAT_TOP_FSYS0, 4, 3),
	CMX_STAT(mout_sclk_mmc0_c, EXYNOS5430_SRC_SEL_TOP_FSYS0, 8, 1, EXYNOS5430_SRC_STAT_TOP_FSYS0, 8, 3),
	CMX_STAT(mout_sclk_mmc0_d, EXYNOS5430_SRC_SEL_TOP_FSYS0, 12, 1, EXYNOS5430_SRC_STAT_TOP_FSYS0, 12, 3),
	CMX_STAT(mout_sclk_mmc1_a, EXYNOS5430_SRC_SEL_TOP_FSYS0, 16, 1, EXYNOS5430_SRC_STAT_TOP_FSYS0, 16, 3),
	CMX_STAT(mout_sclk_mmc1_b, EXYNOS5430_SRC_SEL_TOP_FSYS0, 20, 1, EXYNOS5430_SRC_STAT_TOP_FSYS0, 20, 3),
	CMX_STAT(mout_sclk_mmc2_a, EXYNOS5430_SRC_SEL_TOP_FSYS0, 24, 1, EXYNOS5430_SRC_STAT_TOP_FSYS0, 24, 3),
	CMX_STAT(mout_sclk_mmc2_b, EXYNOS5430_SRC_SEL_TOP_FSYS0, 28, 1, EXYNOS5430_SRC_STAT_TOP_FSYS0, 28, 3),
	CMX_STAT(mout_sclk_ufsunipro, EXYNOS5430_SRC_SEL_TOP_FSYS1, 8, 1, EXYNOS5430_SRC_STAT_TOP_FSYS1, 8, 3),
	CMX_S_A(mout_sclk_jpeg_a, EXYNOS5430_SRC_SEL_TOP_MSCL, 0, 1, "mout_sclk_jpeg_a", EXYNOS5430_SRC_STAT_TOP_MSCL, 0, 3),
	CMX_S_A(mout_sclk_jpeg_b, EXYNOS5430_SRC_SEL_TOP_MSCL, 4, 1, "mout_sclk_jpeg_b", EXYNOS5430_SRC_STAT_TOP_MSCL, 4, 3),
	CMX_S_A(mout_sclk_jpeg_c, EXYNOS5430_SRC_SEL_TOP_MSCL, 8, 1, "mout_sclk_jpeg_c", EXYNOS5430_SRC_STAT_TOP_MSCL, 8, 3),
	CMX_STAT(mout_sclk_isp_spi0, EXYNOS5430_SRC_SEL_TOP_CAM1, 0, 1, EXYNOS5430_SRC_STAT_TOP_CAM1, 0, 3),
	CMX_STAT(mout_sclk_isp_spi1, EXYNOS5430_SRC_SEL_TOP_CAM1, 4, 1, EXYNOS5430_SRC_STAT_TOP_CAM1, 4, 3),
	CMX_STAT(mout_sclk_isp_uart, EXYNOS5430_SRC_SEL_TOP_CAM1, 8, 1, EXYNOS5430_SRC_STAT_TOP_CAM1, 8, 3),
	CMX_STAT(mout_sclk_isp_sensor0, EXYNOS5430_SRC_SEL_TOP_CAM1, 16, 1, EXYNOS5430_SRC_STAT_TOP_CAM1, 16, 3),
	CMX_STAT(mout_sclk_isp_sensor1, EXYNOS5430_SRC_SEL_TOP_CAM1, 20, 1, EXYNOS5430_SRC_STAT_TOP_CAM1, 20, 3),
	CMX_STAT(mout_sclk_isp_sensor2, EXYNOS5430_SRC_SEL_TOP_CAM1, 24, 1, EXYNOS5430_SRC_STAT_TOP_CAM1, 24, 3),
	CMUX(mout_sclk_hdmi_spdif, EXYNOS5430_SRC_SEL_TOP_DISP, 0, 1),

	/* MIF */
	CMX_S_A(mout_mem0_pll, EXYNOS5430_SRC_SEL_MIF0, 0, 1, "mout_mem0_pll", EXYNOS5430_SRC_STAT_MIF0, 0, 3),
	CMX_S_A(mout_mem1_pll, EXYNOS5430_SRC_SEL_MIF0, 4, 1, "mout_mem1_pll", EXYNOS5430_SRC_STAT_MIF0, 4, 3),
	CMX_S_A(mout_mfc_pll, EXYNOS5430_SRC_SEL_MIF0, 12, 1, "mout_mfc_pll", EXYNOS5430_SRC_STAT_MIF0, 12, 3),
	CMX_S_A(mout_bus_pll, EXYNOS5430_SRC_SEL_MIF0, 8, 1, "mout_bus_pll", EXYNOS5430_SRC_STAT_MIF0, 8, 3),
	CMX_S_A(mout_mem0_pll_div2, EXYNOS5430_SRC_SEL_MIF0, 16, 1, "mout_mem0_pll_div2", EXYNOS5430_SRC_STAT_MIF0, 16, 3),
	CMX_S_A(mout_mem1_pll_div2, EXYNOS5430_SRC_SEL_MIF0, 20, 1, "mout_mem1_pll_div2", EXYNOS5430_SRC_STAT_MIF0, 20, 3),
	CMX_S_A(mout_mfc_pll_div2, EXYNOS5430_SRC_SEL_MIF0, 28, 1, "mout_mfc_pll_div2", EXYNOS5430_SRC_STAT_MIF0, 28, 3),
	CMX_S_A(mout_bus_pll_div2, EXYNOS5430_SRC_SEL_MIF0, 24, 1, "mout_bus_pll_div2", EXYNOS5430_SRC_STAT_MIF0, 24, 3),
	CMX_STAT(mout_clkm_phy_a, EXYNOS5430_SRC_SEL_MIF1, 4, 1, EXYNOS5430_SRC_STAT_MIF1, 4, 3),
	CMX_STAT(mout_clkm_phy_b, EXYNOS5430_SRC_SEL_MIF1, 8, 1, EXYNOS5430_SRC_STAT_MIF1, 8, 3),
	CMX_STAT(mout_clkm_phy_c, EXYNOS5430_SRC_SEL_MIF1, 12, 1, EXYNOS5430_SRC_STAT_MIF1, 12, 3),
	CMX_STAT(mout_clk2x_phy_a, EXYNOS5430_SRC_SEL_MIF1, 16, 1, EXYNOS5430_SRC_STAT_MIF1, 16, 3),
	CMX_STAT(mout_clk2x_phy_b, EXYNOS5430_SRC_SEL_MIF1, 20, 1, EXYNOS5430_SRC_STAT_MIF1, 20, 3),
	CMX_STAT(mout_clk2x_phy_c, EXYNOS5430_SRC_SEL_MIF1, 24, 1, EXYNOS5430_SRC_STAT_MIF1, 24, 3),
	CMX_S_A(mout_aclk_mif_400, EXYNOS5430_SRC_SEL_MIF2, 0, 1, "mout_aclk_mif_400", EXYNOS5430_SRC_STAT_MIF2, 0, 3),
	CMX_S_A(mout_aclk_mifnm_200, EXYNOS5430_SRC_SEL_MIF2, 8, 1, "mout_aclk_mifnm_200", EXYNOS5430_SRC_STAT_MIF2, 8, 3),
	CMX_STAT(mout_aclk_disp_333_a, EXYNOS5430_SRC_SEL_MIF3, 0, 1, EXYNOS5430_SRC_STAT_MIF3, 0, 3),
	CMX_STAT(mout_aclk_disp_333_b, EXYNOS5430_SRC_SEL_MIF3, 4, 1, EXYNOS5430_SRC_STAT_MIF3, 4, 3),
	CMX_STAT(mout_sclk_decon_eclk_a, EXYNOS5430_SRC_SEL_MIF4, 0, 1, EXYNOS5430_SRC_STAT_MIF4, 0, 3),
	CMX_STAT(mout_sclk_decon_eclk_b, EXYNOS5430_SRC_SEL_MIF4, 4, 1, EXYNOS5430_SRC_STAT_MIF4, 4, 3),
	CMX_STAT(mout_sclk_decon_eclk_c, EXYNOS5430_SRC_SEL_MIF4, 8, 1, EXYNOS5430_SRC_STAT_MIF4, 8, 3),
	CMX_STAT(mout_sclk_decon_vclk_a, EXYNOS5430_SRC_SEL_MIF4, 16, 1, EXYNOS5430_SRC_STAT_MIF4, 16, 3),
	CMX_STAT(mout_sclk_decon_vclk_b, EXYNOS5430_SRC_SEL_MIF4, 20, 1, EXYNOS5430_SRC_STAT_MIF4, 20, 3),
	CMX_STAT(mout_sclk_decon_vclk_c, EXYNOS5430_SRC_SEL_MIF4, 24, 1, EXYNOS5430_SRC_STAT_MIF4, 24, 3),
	CMX_STAT(mout_sclk_decon_tv_eclk_a, EXYNOS5430_SRC_SEL_MIF5, 16, 1, EXYNOS5430_SRC_STAT_MIF5, 16, 3),
	CMX_STAT(mout_sclk_decon_tv_eclk_b, EXYNOS5430_SRC_SEL_MIF5, 20, 1, EXYNOS5430_SRC_STAT_MIF5, 20, 3),
	CMX_STAT(mout_sclk_decon_tv_eclk_c, EXYNOS5430_SRC_SEL_MIF5, 24, 1, EXYNOS5430_SRC_STAT_MIF5, 24, 3),
	CMX_STAT(mout_sclk_dsd_a, EXYNOS5430_SRC_SEL_MIF5, 0, 1,  EXYNOS5430_SRC_STAT_MIF5, 0, 3),
	CMX_STAT(mout_sclk_dsd_b, EXYNOS5430_SRC_SEL_MIF5, 4, 1, EXYNOS5430_SRC_STAT_MIF5, 4, 3),
	CMX_STAT(mout_sclk_dsd_c, EXYNOS5430_SRC_SEL_MIF5, 8, 1, EXYNOS5430_SRC_STAT_MIF5, 8, 3),
	CMX_STAT(mout_sclk_dsim0_a, EXYNOS5430_SRC_SEL_MIF6, 0, 1, EXYNOS5430_SRC_STAT_MIF6, 0, 3),
	CMX_STAT(mout_sclk_dsim0_b, EXYNOS5430_SRC_SEL_MIF6, 4, 1, EXYNOS5430_SRC_STAT_MIF6, 4, 3),
	CMX_STAT(mout_sclk_dsim0_c, EXYNOS5430_SRC_SEL_MIF6, 8, 1, EXYNOS5430_SRC_STAT_MIF6, 8, 3),

	/* CPIF */
	CMUX(mout_phyclk_lli_tx0_symbol_user, EXYNOS5430_SRC_SEL_CPIF1, 4, 1),
	CMUX(mout_phyclk_lli_rx0_symbol_user, EXYNOS5430_SRC_SEL_CPIF1, 8, 1),
	CMX_STAT(mout_mphy_pll, EXYNOS5430_SRC_SEL_CPIF0, 0, 1, EXYNOS5430_SRC_STAT_CPIF0, 0, 3),
	CMUX(mout_phyclk_ufs_mphy_to_lli_user, EXYNOS5430_SRC_SEL_CPIF1, 0, 1),
	CMX_STAT(mout_sclk_mphy, EXYNOS5430_SRC_SEL_CPIF2, 0, 1, EXYNOS5430_SRC_STAT_CPIF2, 0, 3),

	CMX_STAT(mout_aclk_bus2_400_user, EXYNOS5430_SRC_SEL_BUS2, 0, 1, EXYNOS5430_SRC_STAT_BUS2, 0, 3),

	CMX_STAT(mout_aclk_g2d_400_user, EXYNOS5430_SRC_SEL_G2D, 0, 1, EXYNOS5430_SRC_STAT_G2D, 0, 3),
	CMX_STAT(mout_aclk_g2d_266_user, EXYNOS5430_SRC_SEL_G2D, 4, 1, EXYNOS5430_SRC_STAT_G2D, 4, 3),

	CMX_STAT(mout_aclk_gscl_333_user, EXYNOS5430_SRC_SEL_GSCL, 0, 1, EXYNOS5430_SRC_STAT_GSCL, 0, 3),
	CMX_STAT(mout_aclk_gscl_111_user, EXYNOS5430_SRC_SEL_GSCL, 4, 1, EXYNOS5430_SRC_STAT_GSCL, 4, 3),

	CMX_S_A(mout_aclk_mscl_400_user, EXYNOS5430_SRC_SEL_MSCL0, 0, 1, "mux_aclk_mscl_400_user", EXYNOS5430_SRC_STAT_MSCL0, 0, 3),
	CMX_STAT(mout_sclk_jpeg_user, EXYNOS5430_SRC_SEL_MSCL0, 4, 1, EXYNOS5430_SRC_STAT_MSCL0, 4, 3),
	CMX_STAT(mout_sclk_jpeg, EXYNOS5430_SRC_SEL_MSCL1, 0, 1, EXYNOS5430_SRC_STAT_MSCL1, 0, 3),

	CMX_STAT(mout_g3d_pll, EXYNOS5430_SRC_SEL_G3D, 0, 1, EXYNOS5430_SRC_STAT_G3D, 0, 3),

	CMX_STAT(mout_aclk_hevc_400_user, EXYNOS5430_SRC_SEL_HEVC, 0, 1, EXYNOS5430_SRC_STAT_HEVC, 0, 3),

	CMX_STAT(mout_disp_pll, EXYNOS5430_SRC_SEL_DISP0, 0, 1, EXYNOS5430_SRC_STAT_DISP0, 0, 3),
	CMX_STAT(mout_aclk_disp_333_user, EXYNOS5430_SRC_SEL_DISP1, 0, 1, EXYNOS5430_SRC_STAT_DISP1, 0, 3),
	CMX_STAT(mout_sclk_decon_eclk_user, EXYNOS5430_SRC_SEL_DISP1, 8, 1, EXYNOS5430_SRC_STAT_DISP1, 8, 3),
	CMX_STAT(mout_sclk_decon_eclk, EXYNOS5430_SRC_SEL_DISP3, 0, 1, EXYNOS5430_SRC_STAT_DISP3, 0, 3),
	CMX_STAT(mout_sclk_decon_vclk_user, EXYNOS5430_SRC_SEL_DISP1, 12, 1, EXYNOS5430_SRC_STAT_DISP1, 12, 3),
	CMX_STAT(mout_sclk_decon_vclk, EXYNOS5430_SRC_SEL_DISP3, 4, 1, EXYNOS5430_SRC_STAT_DISP3, 4, 3),
	CMX_STAT(mout_sclk_decon_tv_eclk_user, EXYNOS5430_SRC_SEL_DISP1, 16, 1, EXYNOS5430_SRC_STAT_DISP1, 16, 3),
	CMX_STAT(mout_sclk_decon_tv_eclk, EXYNOS5430_SRC_SEL_DISP3, 8, 1, EXYNOS5430_SRC_STAT_DISP3, 8, 3),
	CMX_STAT(mout_sclk_dsd_user, EXYNOS5430_SRC_SEL_DISP1, 20, 1, EXYNOS5430_SRC_STAT_DISP1, 20, 3),
	CMX_STAT(mout_sclk_dsim0_user, EXYNOS5430_SRC_SEL_DISP1, 24, 1, EXYNOS5430_SRC_STAT_DISP1, 24, 3),
	CMX_STAT(mout_sclk_dsim0, EXYNOS5430_SRC_SEL_DISP3, 12, 1,  EXYNOS5430_SRC_STAT_DISP3, 12, 3),

	CMUX(mout_phyclk_hdmiphy_pixel_clko_user, EXYNOS5430_SRC_SEL_DISP2, 0, 1),
	CMUX(mout_phyclk_hdmiphy_tmds_clko_user, EXYNOS5430_SRC_SEL_DISP2, 4, 1),
	CMUX(mout_phyclk_mipidphy_rxclkesc0_user, EXYNOS5430_SRC_SEL_DISP2, 8, 1),
	CMUX(mout_phyclk_mipidphy_bitclkdiv8_user, EXYNOS5430_SRC_SEL_DISP2, 12, 1),

	CMX_STAT(mout_aclk_fsys_200_user, EXYNOS5430_SRC_SEL_FSYS0, 0, 1, EXYNOS5430_SRC_STAT_FSYS0, 0, 3),
	CMX_STAT(mout_sclk_usbdrd30_user, EXYNOS5430_SRC_SEL_FSYS1, 0, 1, EXYNOS5430_SRC_STAT_FSYS1, 0, 3),
	CMX_STAT(mout_sclk_usbhost30_user, EXYNOS5430_SRC_SEL_FSYS1, 4, 1, EXYNOS5430_SRC_STAT_FSYS1, 4, 3),
	CMX_STAT(mout_sclk_mmc0_user, EXYNOS5430_SRC_SEL_FSYS1, 12, 1, EXYNOS5430_SRC_STAT_FSYS1, 12, 3),
	CMX_STAT(mout_sclk_mmc1_user, EXYNOS5430_SRC_SEL_FSYS1, 16, 1, EXYNOS5430_SRC_STAT_FSYS1, 16, 3),
	CMX_STAT(mout_sclk_mmc2_user, EXYNOS5430_SRC_SEL_FSYS1, 20, 1, EXYNOS5430_SRC_STAT_FSYS1, 20, 3),
	CMX_STAT(mout_sclk_ufsunipro_user, EXYNOS5430_SRC_SEL_FSYS1, 24, 1, EXYNOS5430_SRC_STAT_FSYS1, 24, 3),
	CMX_STAT(mout_ufs_pll_user, EXYNOS5430_SRC_SEL_FSYS0, 4, 1, EXYNOS5430_SRC_STAT_FSYS0, 4, 3),
	CMX_STAT(mout_phyclk_lli_mphy_to_ufs_user, EXYNOS5430_SRC_SEL_FSYS3, 0, 1, EXYNOS5430_SRC_STAT_FSYS3, 0, 3),
	CMX_STAT(mout_sclk_mphy_fsys, EXYNOS5430_SRC_SEL_FSYS4, 0, 1, EXYNOS5430_SRC_STAT_FSYS4, 0, 3),

	CMUX(mout_phyclk_usbdrd30_udrd30_phyclock, EXYNOS5430_SRC_SEL_FSYS2, 0, 1),
	CMUX(mout_phyclk_usbdrd30_udrd30_pipe_pclk, EXYNOS5430_SRC_SEL_FSYS2, 4, 1),
	CMUX(mout_phyclk_usbhost20_phy_freeclk, EXYNOS5430_SRC_SEL_FSYS2, 8, 1),
	CMUX(mout_phyclk_usbhost20_phy_phyclock, EXYNOS5430_SRC_SEL_FSYS2, 12, 1),
	CMUX(mout_phyclk_usbhost20_phy_clk48mohci, EXYNOS5430_SRC_SEL_FSYS2, 16, 1),
	CMUX(mout_phyclk_usbhost20_phy_hsic1, EXYNOS5430_SRC_SEL_FSYS2, 20, 1),
	CMUX(mout_phyclk_usbhost30_uhost30_phyclock, EXYNOS5430_SRC_SEL_FSYS2, 24, 1),
	CMUX(mout_phyclk_usbhost30_uhost30_pipe_pclk, EXYNOS5430_SRC_SEL_FSYS2, 28, 1),
	CMUX(mout_phyclk_ufs_tx0_symbol_user, EXYNOS5430_SRC_SEL_FSYS3, 4, 1),
	CMUX(mout_phyclk_ufs_tx1_symbol_user, EXYNOS5430_SRC_SEL_FSYS3, 8, 1),
	CMUX(mout_phyclk_ufs_rx0_symbol_user, EXYNOS5430_SRC_SEL_FSYS3, 12, 1),
	CMUX(mout_phyclk_ufs_rx1_symbol_user, EXYNOS5430_SRC_SEL_FSYS3, 16, 1),

	CMX_STAT(mout_aud_pll_user, EXYNOS5430_SRC_SEL_AUD0, 0, 1, EXYNOS5430_SRC_STAT_AUD0, 0, 3),
	CMUX(mout_sclk_aud_i2s, EXYNOS5430_SRC_SEL_AUD1, 0, 1),
	CMUX(mout_sclk_aud_pcm, EXYNOS5430_SRC_SEL_AUD1, 8, 1),

	CMUX(mout_phyclk_rxbyteclkhs0_s4, EXYNOS5430_SRC_SEL_CAM01, 4, 1),
	CMUX(mout_phyclk_rxbyteclkhs0_s2a, EXYNOS5430_SRC_SEL_CAM01, 0, 1),

	CMX_STAT(mout_sclk_isp_spi0_user, EXYNOS5430_SRC_SEL_CAM10, 12, 1, EXYNOS5430_SRC_STAT_CAM10, 12, 3),
	CMX_STAT(mout_sclk_isp_spi1_user, EXYNOS5430_SRC_SEL_CAM10, 16, 1, EXYNOS5430_SRC_STAT_CAM10, 16, 3),
	CMX_STAT(mout_sclk_isp_uart_user, EXYNOS5430_SRC_SEL_CAM10, 20, 1, EXYNOS5430_SRC_STAT_CAM10, 20, 3),
	CMUX(mout_phyclk_rxbyteclkhs0_s2b, EXYNOS5430_SRC_SEL_CAM11, 0, 1),

	CMX_S_A(mout_aclk_cam0_552_user, EXYNOS5430_SRC_SEL_CAM00, 0, 1, "mout_aclk_cam0_552_user", EXYNOS5430_SRC_STAT_CAM00, 0, 3),
	CMX_S_A(mout_aclk_cam0_400_user, EXYNOS5430_SRC_SEL_CAM00, 4, 1, "mout_aclk_cam0_400_user", EXYNOS5430_SRC_STAT_CAM00, 4, 3),
	CMX_S_A(mout_aclk_cam0_333_user, EXYNOS5430_SRC_SEL_CAM00, 8, 1, "mout_aclk_cam0_333_user", EXYNOS5430_SRC_STAT_CAM00, 8, 3),
	CMX_S_A(mout_aclk_lite_a_a, EXYNOS5430_SRC_SEL_CAM02, 4, 1, "mout_aclk_lite_a_a", EXYNOS5430_SRC_STAT_CAM02, 4, 3),
	CMX_STAT(mout_aclk_lite_a_b, EXYNOS5430_SRC_SEL_CAM02, 8, 1, EXYNOS5430_SRC_STAT_CAM02, 8, 3),
	CMX_S_A(mout_aclk_lite_b_a, EXYNOS5430_SRC_SEL_CAM02, 12, 1, "mout_aclk_lite_b_a", EXYNOS5430_SRC_STAT_CAM02, 12, 3),
	CMX_STAT(mout_aclk_lite_b_b, EXYNOS5430_SRC_SEL_CAM02, 16, 1, EXYNOS5430_SRC_STAT_CAM02, 16, 3),
	CMX_STAT(mout_aclk_lite_d_a, EXYNOS5430_SRC_SEL_CAM02, 20, 1, EXYNOS5430_SRC_STAT_CAM02, 20, 3),
	CMX_STAT(mout_aclk_lite_d_b, EXYNOS5430_SRC_SEL_CAM02, 24, 1, EXYNOS5430_SRC_STAT_CAM02, 24, 3),
	CMX_STAT(mout_sclk_pixelasync_lite_c_init_a, EXYNOS5430_SRC_SEL_CAM04, 0, 1, EXYNOS5430_SRC_STAT_CAM04, 0, 3),
	CMX_STAT(mout_sclk_pixelasync_lite_c_init_b, EXYNOS5430_SRC_SEL_CAM04, 4, 1, EXYNOS5430_SRC_STAT_CAM04, 4, 3),
	CMX_STAT(mout_sclk_pixelasync_lite_c_a, EXYNOS5430_SRC_SEL_CAM04, 8, 1, EXYNOS5430_SRC_STAT_CAM04, 8, 3),
	CMX_S_A(mout_sclk_pixelasync_lite_c_b, EXYNOS5430_SRC_SEL_CAM04, 12, 1, "mout_sclk_pixelasync_lite_c_b", EXYNOS5430_SRC_STAT_CAM04, 12, 3),
	CMX_S_A(mout_aclk_3aa0_a, EXYNOS5430_SRC_SEL_CAM03, 0, 1, "mout_aclk_3aa0_a", EXYNOS5430_SRC_STAT_CAM03, 0, 3),
	CMX_STAT(mout_aclk_3aa0_b, EXYNOS5430_SRC_SEL_CAM03, 4, 1, EXYNOS5430_SRC_STAT_CAM03, 4, 3),
	CMX_S_A(mout_aclk_3aa1_a, EXYNOS5430_SRC_SEL_CAM03, 8, 1, "mout_aclk_3aa1_a", EXYNOS5430_SRC_STAT_CAM03, 8, 3),
	CMX_STAT(mout_aclk_3aa1_b, EXYNOS5430_SRC_SEL_CAM03, 12, 1, EXYNOS5430_SRC_STAT_CAM03, 12, 3),
	CMX_S_A(mout_aclk_csis0_a, EXYNOS5430_SRC_SEL_CAM03, 16, 1, "mout_aclk_csis0_a", EXYNOS5430_SRC_STAT_CAM03, 16, 3),
	CMX_S_A(mout_aclk_csis0_b, EXYNOS5430_SRC_SEL_CAM03, 20, 1, "mout_aclk_csis0_b", EXYNOS5430_SRC_STAT_CAM03, 20, 3),
	CMX_S_A(mout_aclk_csis1_a, EXYNOS5430_SRC_SEL_CAM03, 24, 1, "mout_aclk_csis1_a", EXYNOS5430_SRC_STAT_CAM03, 24, 3),
	CMX_S_A(mout_aclk_csis1_b, EXYNOS5430_SRC_SEL_CAM03, 28, 1, "mout_aclk_csis1_b", EXYNOS5430_SRC_STAT_CAM03, 28, 3),
	CMX_STAT(mout_aclk_cam0_400, EXYNOS5430_SRC_SEL_CAM02, 0, 1, EXYNOS5430_SRC_STAT_CAM02, 0, 3),
	CMX_STAT(mout_sclk_lite_freecnt_a, EXYNOS5430_SRC_SEL_CAM04, 16, 1, EXYNOS5430_SRC_STAT_CAM04, 16, 3),
	CMX_S_A(mout_sclk_lite_freecnt_b, EXYNOS5430_SRC_SEL_CAM04, 20, 1, "mout_sclk_lite_freecnt_b", EXYNOS5430_SRC_STAT_CAM04, 20, 3),
	CMX_S_A(mout_sclk_lite_freecnt_c, EXYNOS5430_SRC_SEL_CAM04, 24, 1, "mout_sclk_lite_freecnt_c", EXYNOS5430_SRC_STAT_CAM04, 24, 3),

	CMX_S_A(mout_aclk_cam1_552_user, EXYNOS5430_SRC_SEL_CAM10, 0, 1, "mout_aclk_cam1_552_user", EXYNOS5430_SRC_STAT_CAM10, 0, 3),
	CMX_S_A(mout_aclk_cam1_400_user, EXYNOS5430_SRC_SEL_CAM10, 4, 1, "mout_aclk_cam1_400_user", EXYNOS5430_SRC_STAT_CAM10, 4, 3),
	CMX_S_A(mout_aclk_cam1_333_user, EXYNOS5430_SRC_SEL_CAM10, 8, 1, "mout_aclk_cam1_333_user", EXYNOS5430_SRC_STAT_CAM10, 8, 3),
	CMX_STAT(mout_aclk_lite_c_a, EXYNOS5430_SRC_SEL_CAM12, 0, 1, EXYNOS5430_SRC_STAT_CAM12, 0, 3),
	CMX_S_A(mout_aclk_lite_c_b, EXYNOS5430_SRC_SEL_CAM12, 4, 1, "mout_aclk_lite_c_b", EXYNOS5430_SRC_STAT_CAM12, 4, 3),
	CMX_S_A(mout_aclk_fd_a, EXYNOS5430_SRC_SEL_CAM12, 8, 1, "mout_aclk_fd_a", EXYNOS5430_SRC_STAT_CAM12, 8, 3),
	CMX_S_A(mout_aclk_fd_b, EXYNOS5430_SRC_SEL_CAM12, 12, 1, "mout_aclk_fd_b", EXYNOS5430_SRC_STAT_CAM12, 12, 3),
	CMX_STAT(mout_aclk_csis2_a, EXYNOS5430_SRC_SEL_CAM12, 16, 1, EXYNOS5430_SRC_STAT_CAM12, 16, 3),
	CMX_S_A(mout_aclk_csis2_b, EXYNOS5430_SRC_SEL_CAM12, 20, 1, "mout_aclk_csis2_b", EXYNOS5430_SRC_STAT_CAM12, 20, 3),

	CMX_STAT(mout_aclk_isp_400_user, EXYNOS5430_SRC_SEL_ISP, 0, 1, EXYNOS5430_SRC_STAT_ISP, 0, 3),
	CMX_STAT(mout_aclk_isp_dis_400_user, EXYNOS5430_SRC_SEL_ISP, 4, 1, EXYNOS5430_SRC_STAT_ISP, 4, 3),

	/* additional clocks on exynos5433 series */
	CMX_S_A(mout_aclk_mfc_400_a, EXYNOS5430_SRC_SEL_TOP4, 0, 1, "mout_aclk_mfc_400_a", EXYNOS5430_SRC_STAT_TOP4, 0, 3),
	CMX_S_A(mout_aclk_mfc_400_b, EXYNOS5430_SRC_SEL_TOP4, 4, 1, "mout_aclk_mfc_400_b", EXYNOS5430_SRC_STAT_TOP4, 4, 3),
	CMX_S_A(mout_aclk_mfc_400_c, EXYNOS5430_SRC_SEL_TOP4, 8, 1, "mout_aclk_mfc_400_c", EXYNOS5430_SRC_STAT_TOP4, 8, 3),
	CMX_STAT(mout_aclk_mfc_400_user, EXYNOS5430_SRC_SEL_MFC0, 0, 1, EXYNOS5430_SRC_STAT_MFC0, 0, 3),

	CMX_STAT(mout_sclk_usbhost30, EXYNOS5430_SRC_SEL_FSYS1, 4, 1, EXYNOS5430_SRC_STAT_TOP_FSYS1, 4, 1),
	CMX_STAT(mout_sclk_pcie_100_user, EXYNOS5430_SRC_SEL_FSYS1, 28, 1, EXYNOS5430_SRC_STAT_FSYS1, 28, 3),
	CMX_STAT(mout_sclk_pcie_100, EXYNOS5430_SRC_SEL_TOP_FSYS1, 12, 1, EXYNOS5430_SRC_STAT_TOP_FSYS1, 12, 3),
	CMX_STAT(mout_sclk_spi3, EXYNOS5430_SRC_SEL_TOP_PERIC0, 24, 1, EXYNOS5430_SRC_STAT_TOP_PERIC0, 24, 3),
	CMX_STAT(mout_sclk_spi4, EXYNOS5430_SRC_SEL_TOP_PERIC0, 28, 1, EXYNOS5430_SRC_STAT_TOP_PERIC0, 28, 3),

	CMUX(mout_aclk_g3d_400, EXYNOS5430_SRC_SEL_G3D, 8, 1),

	CMX_S_A(mout_aclk_bus0_400, EXYNOS5430_SRC_SEL_TOP3, 20, 1, "mout_aclk_bus0_400", EXYNOS5430_SRC_STAT_TOP3, 20, 3),
	/* disp block */
	CMX_STAT(mout_sclk_decon_tv_vclk_a, EXYNOS5430_SRC_SEL_MIF7, 16, 1, EXYNOS5430_SRC_STAT_MIF7, 16, 3),
	CMX_STAT(mout_sclk_decon_tv_vclk_b, EXYNOS5430_SRC_SEL_MIF7, 20, 1, EXYNOS5430_SRC_STAT_MIF7, 20, 3),
	CMX_STAT(mout_sclk_decon_tv_vclk_c, EXYNOS5430_SRC_SEL_MIF7, 24, 1, EXYNOS5430_SRC_STAT_MIF7, 24, 3),
	CMX_STAT(mout_sclk_decon_tv_vclk_a_disp, EXYNOS5430_SRC_SEL_DISP4, 0, 1, EXYNOS5430_SRC_STAT_DISP4, 0, 3),
	CMX_STAT(mout_sclk_decon_tv_vclk_b_disp, EXYNOS5430_SRC_SEL_DISP4, 4, 1, EXYNOS5430_SRC_STAT_DISP4, 4, 3),
	CMX_STAT(mout_sclk_decon_tv_vclk_c_disp, EXYNOS5430_SRC_SEL_DISP4, 8, 1, EXYNOS5430_SRC_STAT_DISP4, 8, 3),
	CMX_STAT(mout_sclk_decon_tv_vclk_user, EXYNOS5430_SRC_SEL_DISP1, 12, 1, EXYNOS5430_SRC_STAT_DISP1, 12, 3),

	CMX_STAT(mout_sclk_dsim1_a, EXYNOS5430_SRC_SEL_MIF7, 0, 1, EXYNOS5430_SRC_STAT_MIF7, 0, 3),
	CMX_STAT(mout_sclk_dsim1_b, EXYNOS5430_SRC_SEL_MIF7, 4, 1, EXYNOS5430_SRC_STAT_MIF7, 4, 3),
	CMX_STAT(mout_sclk_dsim1_c, EXYNOS5430_SRC_SEL_MIF7, 8, 1, EXYNOS5430_SRC_STAT_MIF7, 8, 3),
	CMX_STAT(mout_sclk_dsim1_a_disp, EXYNOS5430_SRC_SEL_DISP4, 12, 1, EXYNOS5430_SRC_STAT_DISP4, 12, 3),
	CMX_STAT(mout_sclk_dsim1_b_disp, EXYNOS5430_SRC_SEL_DISP4, 16, 1, EXYNOS5430_SRC_STAT_DISP4, 16, 3),
	CMX_STAT(mout_sclk_dsim1_user, EXYNOS5430_SRC_SEL_DISP1, 28, 1, EXYNOS5430_SRC_STAT_DISP1, 28, 3),

	CMUX(mout_phyclk_mipidphy1_rxclkesc0_user, EXYNOS5430_SRC_SEL_DISP2, 16, 1),
	CMUX(mout_phyclk_mipidphy1_bitclkdiv8_user, EXYNOS5430_SRC_SEL_DISP2, 20, 1),
};

#define CDV(_id, cname, pname, o, s, w) \
		DIV(_id, cname, pname, (unsigned long)o, s, w)
#define CDIV(_id, pname, o, s, w) \
		DIV(_id, #_id, pname, (unsigned long)o, s, w)
#define CDV_A(_id, cname, pname, o, s, w, a) \
		DIV_A(_id, cname, pname, (unsigned long)o, s, w, a)
struct samsung_div_clock exynos5433_div_clks[] __initdata = {
	CDIV(dout_egl1, "mout_egl", EXYNOS5430_DIV_EGL0, 0, 3),
	CDIV(dout_egl2, "dout_egl1", EXYNOS5430_DIV_EGL0, 4, 3),
	CDIV(dout_aclk_egl, "dout_egl2", EXYNOS5430_DIV_EGL0, 8, 3),
	CDIV(dout_atclk_egl, "dout_egl2", EXYNOS5430_DIV_EGL0, 16, 4),
	CDIV(dout_pclk_egl, "dout_egl2", EXYNOS5430_DIV_EGL0, 12, 3),
	CDIV(dout_sclk_hpm_egl, "mout_egl", EXYNOS5430_DIV_EGL1, 4, 3),
	CDIV(dout_egl_pll, "mout_egl", EXYNOS5430_DIV_EGL1, 0, 3),
	CDIV(dout_kfc1, "mout_kfc", EXYNOS5430_DIV_KFC0, 0, 3),
	CDIV(dout_kfc2, "dout_kfc1", EXYNOS5430_DIV_KFC0, 4, 3),
	CDIV(dout_aclk_kfc, "dout_kfc2", EXYNOS5430_DIV_KFC0, 8, 3),
	CDIV(dout_atclk_kfc, "dout_kfc2", EXYNOS5430_DIV_KFC0, 16, 3),
	CDIV(dout_pclk_dbg_kfc, "dout_kfc2", EXYNOS5430_DIV_KFC0, 20, 3),
	CDIV(dout_pclk_kfc, "dout_kfc2", EXYNOS5430_DIV_KFC0, 12, 3),
	CDIV(dout_cntclk_kfc, "dout_kfc2", EXYNOS5430_DIV_KFC0, 24, 3),
	CDIV(dout_sclk_hpm_kfc, "mout_kfc", EXYNOS5430_DIV_KFC1, 4, 3),
	CDIV(dout_kfc_pll, "mout_kfc", EXYNOS5430_DIV_KFC1, 0, 3),

	/* TOP */
	CDIV(dout_aclk_imem_266, "mout_bus_pll_user", EXYNOS5430_DIV_TOP3, 16, 3),
	CDIV(dout_aclk_imem_200, "mout_bus_pll_user", EXYNOS5430_DIV_TOP3, 20, 3),
	CDIV(dout_aclk_peris_66_a, "mout_bus_pll_user", EXYNOS5430_DIV_TOP3, 0, 3),
	CDIV(dout_aclk_peris_66_b, "dout_aclk_peris_66_a", EXYNOS5430_DIV_TOP3, 4, 3),
	CDIV(dout_aclk_peric_66_a, "mout_bus_pll_user", EXYNOS5430_DIV_TOP3, 8, 3),
	CDIV(dout_aclk_peric_66_b, "dout_aclk_peric_66_a", EXYNOS5430_DIV_TOP3, 12, 3),
	CDIV(dout_aclk_fsys_200, "mout_bus_pll_user", EXYNOS5430_DIV_TOP2, 0, 3),
	CDV_A(dout_aclk_gscl_333, "dout_aclk_gscl_333", "mout_aclk_gscl_333", EXYNOS5430_DIV_TOP1, 24, 3, "dout_aclk_gscl_333"),
	CDIV(dout_aclk_gscl_111, "mout_aclk_gscl_333", EXYNOS5430_DIV_TOP1, 28, 3),
	CDV_A(dout_aclk_hevc_400, "dout_aclk_hevc_400", "mout_aclk_hevc_400", EXYNOS5430_DIV_TOP1, 20, 3, "dout_aclk_hevc_400"),
	CDV_A(dout_aclk_mscl_400, "dout_aclk_mscl_400", "mout_aclk_mscl_400_b", EXYNOS5430_DIV_TOP2, 4, 3, "dout_aclk_mscl_400"),
	CDV_A(dout_aclk_g2d_400, "dout_aclk_g2d_400", "mout_aclk_g2d_400_b", EXYNOS5430_DIV_TOP1, 0, 3, "dout_aclk_g2d_400"),
	CDV_A(dout_aclk_g2d_266, "dout_aclk_g2d_266", "mout_bus_pll_user", EXYNOS5430_DIV_TOP1, 8, 3, "dout_aclk_g2d_266"),
	CDV_A(dout_aclk_isp_400, "dout_aclk_isp_400", "mout_aclk_isp_400", EXYNOS5430_DIV_TOP0, 0, 3, "dout_aclk_isp_400"),
	CDV_A(dout_aclk_isp_dis_400, "dout_aclk_isp_dis_400", "mout_aclk_isp_dis_400", EXYNOS5430_DIV_TOP0, 4, 3, "dout_aclk_isp_dis_400"),
	CDV_A(dout_aclk_cam0_552, "dout_aclk_cam0_552", "mout_isp_pll", EXYNOS5430_DIV_TOP0, 8, 3, "dout_aclk_cam0_552"),
	CDV_A(dout_aclk_cam0_400, "dout_aclk_cam0_400", "mout_bus_pll_user", EXYNOS5430_DIV_TOP0, 12, 3, "dout_aclk_cam0_400"),
	CDV_A(dout_aclk_cam0_333, "dout_aclk_cam0_333", "mout_mfc_pll_user", EXYNOS5430_DIV_TOP0, 16, 3, "dout_aclk_cam0_333"),
	CDV_A(dout_aclk_cam1_552, "dout_aclk_cam1_552", "mout_aclk_cam1_552_b", EXYNOS5430_DIV_TOP0, 20, 3, "dout_aclk_cam1_552"),
	CDV_A(dout_aclk_cam1_400, "dout_aclk_cam1_400", "mout_bus_pll_user", EXYNOS5430_DIV_TOP0, 24, 3, "dout_aclk_cam1_400"),
	CDV_A(dout_aclk_cam1_333, "dout_aclk_cam1_333", "mout_aclk_cam1_333", EXYNOS5430_DIV_TOP0, 28, 3, "dout_aclk_cam1_333"),
	CDIV(dout_sclk_audio0, "mout_sclk_audio0", EXYNOS5430_DIV_TOP_PERIC3, 0, 4),
	CDIV(dout_sclk_audio1, "mout_sclk_audio1", EXYNOS5430_DIV_TOP_PERIC3, 4, 4),
	CDIV(dout_sclk_pcm1, "dout_sclk_audio1", EXYNOS5430_DIV_TOP_PERIC3, 8, 8),
	CDIV(dout_sclk_i2s1, "dout_sclk_audio1", EXYNOS5430_DIV_TOP_PERIC3, 16, 6),
	CDIV(dout_sclk_spi0_a, "mout_sclk_spi0", EXYNOS5430_DIV_TOP_PERIC0, 0, 4),
	CDIV(dout_sclk_spi1_a, "mout_sclk_spi1", EXYNOS5430_DIV_TOP_PERIC0, 12, 4),
	CDIV(dout_sclk_spi2_a, "mout_sclk_spi2", EXYNOS5430_DIV_TOP_PERIC1, 0, 4),
	CDIV(dout_sclk_spi0_b, "dout_sclk_spi0_a", EXYNOS5430_DIV_TOP_PERIC0, 4, 8),
	CDIV(dout_sclk_spi1_b, "dout_sclk_spi1_a", EXYNOS5430_DIV_TOP_PERIC0, 16, 8),
	CDIV(dout_sclk_spi2_b, "dout_sclk_spi2_a", EXYNOS5430_DIV_TOP_PERIC1, 4, 8),
	CDIV(dout_sclk_uart0, "mout_sclk_uart0", EXYNOS5430_DIV_TOP_PERIC2, 0, 4),
	CDIV(dout_sclk_uart1, "mout_sclk_uart1", EXYNOS5430_DIV_TOP_PERIC2, 4, 4),
	CDIV(dout_sclk_uart2, "mout_sclk_uart2", EXYNOS5430_DIV_TOP_PERIC2, 8, 4),
	CDIV(dout_usbdrd30, "mout_sclk_usbdrd30", EXYNOS5430_DIV_TOP_FSYS2, 0, 4),
	CDIV(dout_mmc0_a, "mout_sclk_mmc0_d", EXYNOS5430_DIV_TOP_FSYS0, 0, 4),
	CDIV(dout_mmc0_b, "dout_mmc0_a", EXYNOS5430_DIV_TOP_FSYS0, 4, 8),
	CDIV(dout_mmc1_a, "mout_sclk_mmc1_b", EXYNOS5430_DIV_TOP_FSYS0, 12, 4),
	CDIV(dout_mmc1_b, "dout_mmc1_a", EXYNOS5430_DIV_TOP_FSYS0, 16, 8),
	CDIV(dout_mmc2_a, "mout_sclk_mmc2_b", EXYNOS5430_DIV_TOP_FSYS1, 0, 4),
	CDIV(dout_mmc2_b, "dout_mmc2_a", EXYNOS5430_DIV_TOP_FSYS1, 4, 8),
	CDIV(dout_sclk_ufsunipro, "mout_sclk_ufsunipro", EXYNOS5430_DIV_TOP_FSYS2, 4, 4),
	CDV_A(dout_sclk_jpeg, "dout_sclk_jpeg", "mout_sclk_jpeg_c", EXYNOS5430_DIV_TOP_MSCL, 0, 4, "dout_sclk_jpeg"),
	CDIV(dout_sclk_isp_spi0_a, "mout_sclk_isp_spi0", EXYNOS5430_DIV_TOP_CAM10, 0, 4),
	CDIV(dout_sclk_isp_spi0_b, "dout_sclk_isp_spi0_a", EXYNOS5430_DIV_TOP_CAM10, 4, 8),
	CDIV(dout_sclk_isp_spi1_a, "mout_sclk_isp_spi1", EXYNOS5430_DIV_TOP_CAM10, 12, 4),
	CDIV(dout_sclk_isp_spi1_b, "dout_sclk_isp_spi1_a", EXYNOS5430_DIV_TOP_CAM10, 16, 8),
	CDIV(dout_sclk_isp_uart, "mout_sclk_isp_uart", EXYNOS5430_DIV_TOP_CAM10, 24, 4),
	CDIV(dout_sclk_isp_sensor0_a, "mout_sclk_isp_sensor0", EXYNOS5430_DIV_TOP_CAM11, 0, 4),
	CDIV(dout_sclk_isp_sensor0_b, "dout_sclk_isp_sensor0_a", EXYNOS5430_DIV_TOP_CAM11, 4, 4),
	CDIV(dout_sclk_isp_sensor1_a, "mout_sclk_isp_sensor1", EXYNOS5430_DIV_TOP_CAM11, 8, 4),
	CDIV(dout_sclk_isp_sensor1_b, "dout_sclk_isp_sensor1_a", EXYNOS5430_DIV_TOP_CAM11, 12, 4),
	CDIV(dout_sclk_isp_sensor2_a, "mout_sclk_isp_sensor2", EXYNOS5430_DIV_TOP_CAM11, 16, 4),
	CDIV(dout_sclk_isp_sensor2_b, "dout_sclk_isp_sensor2_a", EXYNOS5430_DIV_TOP_CAM11, 20, 4),

	/* MIF */
	CDV_A(dout_mif_pre, "dout_mif_pre", "mout_bus_pll_div2", EXYNOS5430_DIV_MIF5, 0, 3, "dout_mif_pre"),
	CDV_A(dout_clk2x_phy, "dout_clk2x_phy", "mout_clk2x_phy_c", EXYNOS5430_DIV_MIF1, 4, 4, "dout_clk2x_phy"),
	CDV_A(dout_aclk_mif_400, "dout_aclk_mif_400", "mout_aclk_mif_400", EXYNOS5430_DIV_MIF2, 0, 3, "dout_aclk_mif_400"),
	CDV_A(dout_aclk_drex0, "dout_aclk_drex0", "dout_clk2x_phy", EXYNOS5430_DIV_MIF1, 8, 2, "dout_aclk_drex0"),
	CDV_A(dout_aclk_drex1, "dout_aclk_drex1", "dout_clk2x_phy", EXYNOS5430_DIV_MIF1, 12, 2, "dout_aclk_drex1"),
	CDV_A(dout_sclk_hpm_mif, "dout_sclk_hpm_mif", "dout_clk2x_phy", EXYNOS5430_DIV_MIF1, 16, 2, "dout_sclk_hpm_mif"),
	CDV_A(dout_aclk_mif_200, "dout_aclk_mif_200", "dout_aclk_mif_400", EXYNOS5430_DIV_MIF2, 4, 2, "dout_aclk_mif_200"),
	CDV_A(dout_aclk_mifnm_200, "dout_aclk_mifnm_200", "mout_aclk_mifnm_200", EXYNOS5430_DIV_MIF2, 8, 3, "dout_aclk_mifnm_200"),
	CDV_A(dout_aclk_mifnd_133, "dout_aclk_mifnd_133", "dout_mif_pre", EXYNOS5430_DIV_MIF2, 16, 4, "dout_aclk_mifnd_133"),
	CDV_A(dout_aclk_mif_133, "dout_aclk_mif_133", "dout_mif_pre", EXYNOS5430_DIV_MIF2, 12, 4, "dout_aclk_mif_133"),
	CDV_A(dout_aclk_cpif_200, "dout_aclk_cpif_200", "mout_aclk_mifnm_200", EXYNOS5430_DIV_MIF3, 0, 3, "dout_aclk_cpif_200"),
	CDV_A(dout_aclk_bus2_400, "dout_aclk_bus2_400", "dout_mif_pre", EXYNOS5430_DIV_MIF3, 16, 4, "dout_aclk_bus2_400"),
	CDV_A(dout_aclk_disp_333, "dout_aclk_disp_333", "mout_aclk_disp_333_b", EXYNOS5430_DIV_MIF3, 4, 3, "dout_aclk_disp_333"),
	CDIV(dout_sclk_decon_eclk, "mout_sclk_decon_eclk_c", EXYNOS5430_DIV_MIF4, 0, 4),
	CDIV(dout_sclk_decon_vclk, "mout_sclk_decon_vclk_c", EXYNOS5430_DIV_MIF4, 4, 4),
	CDIV(dout_sclk_decon_tv_eclk, "mout_sclk_decon_tv_eclk_c", EXYNOS5430_DIV_MIF4, 8, 4),
	CDV_A(dout_sclk_dsd, "dout_sclk_dsd", "mout_sclk_dsd_c", EXYNOS5430_DIV_MIF4, 12, 4, "dout_sclk_dsd"),
	CDIV(dout_sclk_dsim0, "mout_sclk_dsim0_c", EXYNOS5430_DIV_MIF4, 16, 4),

	/* CPIF */
	CDIV(dout_sclk_mphy, "mout_mphy_pll", EXYNOS5430_DIV_CPIF, 0, 5),

	CDIV(dout_pclk_bus1_133, "aclk_bus1_400", EXYNOS5430_DIV_BUS1, 0, 3),
	CDIV(dout_pclk_bus2_133, "mout_aclk_bus2_400_user", EXYNOS5430_DIV_BUS2, 0, 3),

	CDIV(dout_pclk_g2d, "mout_aclk_g2d_266_user", EXYNOS5430_DIV_G2D, 0, 2),

	CDIV(dout_pclk_mscl, "mout_aclk_mscl_400_user", EXYNOS5430_DIV_MSCL, 0, 3),

	CDIV(dout_pclk_hevc, "mout_aclk_hevc_400_user", EXYNOS5430_DIV_HEVC, 0, 2),

	CDIV(dout_pclk_disp, "mout_aclk_disp_333_user", EXYNOS5430_DIV_DISP, 0, 2),
	CDIV(dout_sclk_decon_eclk_disp, "mout_sclk_decon_eclk", EXYNOS5430_DIV_DISP, 4, 3),
	CDIV(dout_sclk_decon_vclk_disp, "mout_sclk_decon_vclk", EXYNOS5430_DIV_DISP, 8, 3),
	CDIV(dout_sclk_decon_tv_eclk_disp, "mout_sclk_decon_tv_eclk", EXYNOS5430_DIV_DISP, 12, 3),
	CDIV(dout_sclk_dsim0_disp, "mout_sclk_dsim0", EXYNOS5430_DIV_DISP, 16, 3),

	CDIV(dout_aud_ca5, "mout_aud_pll_user", EXYNOS5430_DIV_AUD0, 0, 4),
	CDIV(dout_aclk_aud, "dout_aud_ca5", EXYNOS5430_DIV_AUD0, 4, 4),
	CDIV(dout_pclk_dbg_aud, "dout_aud_ca5", EXYNOS5430_DIV_AUD0, 8, 4),
	CDIV(dout_sclk_aud_i2s, "mout_sclk_aud_i2s", EXYNOS5430_DIV_AUD1, 0, 4),
	CDIV(dout_sclk_aud_pcm, "mout_sclk_aud_pcm", EXYNOS5430_DIV_AUD1, 4, 8),
	CDIV(dout_sclk_aud_slimbus, "mout_aud_pll_user", EXYNOS5430_DIV_AUD1, 16, 5),
	CDIV(dout_sclk_aud_uart, "mout_aud_pll_user", EXYNOS5430_DIV_AUD1, 12, 4),

	CDV_A(dout_aclk_lite_a, "dout_aclk_lite_a", "mout_aclk_lite_a_b", EXYNOS5430_DIV_CAM01, 0, 3, "dout_aclk_lite_a"),
	CDV_A(dout_aclk_lite_b, "dout_aclk_lite_b", "mout_aclk_lite_b_b", EXYNOS5430_DIV_CAM01, 8, 3, "dout_aclk_lite_b"),
	CDV_A(dout_aclk_lite_d, "dout_aclk_lite_d", "mout_aclk_lite_d_b", EXYNOS5430_DIV_CAM01, 16, 3, "dout_aclk_lite_d"),
	CDV_A(dout_sclk_pixelasync_lite_c_init, "dout_sclk_pixelasync_lite_c_init", "mout_sclk_pixelasync_lite_c_init_b", EXYNOS5430_DIV_CAM03, 0, 3, "dout_sclk_pixelasync_lite_c_init"),
	CDV_A(dout_sclk_pixelasync_lite_c, "dout_sclk_pixelasync_lite_c", "mout_sclk_pixelasync_lite_c_b", EXYNOS5430_DIV_CAM03, 8, 3, "dout_sclk_pixelasync_lite_c"),
	CDV_A(dout_aclk_3aa0, "dout_aclk_3aa0", "mout_aclk_3aa0_b", EXYNOS5430_DIV_CAM02, 0, 3, "dout_aclk_3aa0"),
	CDV_A(dout_aclk_3aa1, "dout_aclk_3aa1", "mout_aclk_3aa1_b", EXYNOS5430_DIV_CAM02, 8, 3, "dout_aclk_3aa1"),
	CDV_A(dout_aclk_csis0, "dout_aclk_csis0", "mout_aclk_csis0_b", EXYNOS5430_DIV_CAM02, 16, 3, "dout_aclk_csis0"),
	CDV_A(dout_aclk_csis1, "dout_aclk_csis1", "mout_aclk_csis1_b", EXYNOS5430_DIV_CAM02, 20, 3, "dout_aclk_csis1"),
	CDV_A(dout_aclk_cam0_bus_400, "dout_aclk_cam0_bus_400", "mout_aclk_cam0_400", EXYNOS5430_DIV_CAM00, 0, 3, "dout_aclk_cam0_bus_400"),
	CDIV(dout_aclk_cam0_200, "mout_aclk_cam0_400", EXYNOS5430_DIV_CAM00, 4, 3),
	CDIV(dout_pclk_lite_a, "dout_aclk_lite_a", EXYNOS5430_DIV_CAM01, 4, 2),
	CDIV(dout_pclk_lite_b, "dout_aclk_lite_b", EXYNOS5430_DIV_CAM01, 12, 2),
	CDV_A(dout_pclk_lite_d, "dout_pclk_lite_d", "dout_aclk_lite_d", EXYNOS5430_DIV_CAM01, 20, 2, "dout_pclk_lite_d"),
	CDV_A(dout_pclk_pixelasync_lite_c, "dout_pclk_pixelasync_lite_c", "dout_sclk_pixelasync_lite_c_init", EXYNOS5430_DIV_CAM03, 4, 2, "dout_pclk_pixelasync_lite_c"),
	CDIV(dout_pclk_3aa0, "dout_aclk_3aa0", EXYNOS5430_DIV_CAM02, 4, 2),
	CDIV(dout_pclk_3aa1, "dout_aclk_3aa1", EXYNOS5430_DIV_CAM02, 12, 2),
	CDIV(dout_pclk_cam0_50, "dout_aclk_cam0_200", EXYNOS5430_DIV_CAM00, 8, 2),

	CDIV(dout_atclk_cam1, "mout_aclk_cam1_552_user", EXYNOS5430_DIV_CAM10, 0, 3),
	CDIV(dout_pclk_dbg_cam1, "mout_aclk_cam1_552_user", EXYNOS5430_DIV_CAM10, 4, 3),
	CDIV(dout_pclk_cam1_166, "mout_aclk_cam1_333_user", EXYNOS5430_DIV_CAM10, 8, 2),
	CDV_A(dout_pclk_cam1_83, "dout_pclk_cam1_83", "mout_aclk_cam1_333_user", EXYNOS5430_DIV_CAM10, 12, 2, "dout_pclk_cam1_83"),
	CDIV(dout_sclk_isp_mpwm, "dout_pclk_cam1_83", EXYNOS5430_DIV_CAM10, 16, 2),
	CDV_A(dout_aclk_lite_c, "dout_aclk_lite_c", "mout_aclk_lite_c_b", EXYNOS5430_DIV_CAM11, 0, 3, "dout_aclk_lite_c"),
	CDV_A(dout_aclk_fd, "dout_aclk_fd", "mout_aclk_fd_b", EXYNOS5430_DIV_CAM11, 8, 3, "dout_aclk_fd"),
	CDV_A(dout_aclk_csis2, "dout_aclk_csis2", "mout_aclk_csis2_b", EXYNOS5430_DIV_CAM11, 16, 3, "dout_aclk_csis2"),
	CDIV(dout_pclk_lite_c, "dout_aclk_lite_c", EXYNOS5430_DIV_CAM11, 4, 2),
	CDIV(dout_pclk_fd, "dout_aclk_fd", EXYNOS5430_DIV_CAM11, 12, 2),

	CDIV(dout_aclk_isp_c_200, "mout_aclk_isp_400_user", EXYNOS5430_DIV_ISP, 0, 3),
	CDIV(dout_aclk_isp_d_200, "mout_aclk_isp_400_user", EXYNOS5430_DIV_ISP, 4, 3),
	CDIV(dout_pclk_isp, "mout_aclk_isp_400_user", EXYNOS5430_DIV_ISP, 8, 3),
	CDIV(dout_pclk_isp_dis, "mout_aclk_isp_dis_400_user", EXYNOS5430_DIV_ISP, 12, 3),

	/* additional clocks on exynos5433 series */
	CDV_A(dout_aclk_mfc_400, "dout_aclk_mfc_400", "mout_aclk_mfc_400_c", EXYNOS5430_DIV_TOP1, 12, 3, "dout_aclk_mfc_400"),
	CDIV(dout_pclk_mfc, "mout_aclk_mfc_400_user", EXYNOS5430_DIV_MFC0, 0, 2),
	/* DIV clocks for TOP */
	CDIV(dout_aclk_imem_sssx_266, "mout_bus_pll_user", EXYNOS5430_DIV_TOP3, 24, 3),
	CDV_A(dout_aclk_bus0_400, "dout_aclk_bus0_400", "mout_aclk_bus0_400", EXYNOS5430_DIV_TOP4, 4, 3, "dout_aclk_bus0_400"),
	CDV_A(dout_aclk_bus1_400, "dout_aclk_bus1_400", "mout_bus_pll_user", EXYNOS5430_DIV_TOP4, 0, 3, "dout_aclk_bus1_400"),
	CDIV(dout_usbhost30, "mout_sclk_usbhost30", EXYNOS5430_DIV_TOP_FSYS2, 8, 4),
	CDIV(dout_sclk_pcie_100, "mout_sclk_pcie_100", EXYNOS5430_DIV_TOP_FSYS2, 12, 3),
	CDIV(dout_sclk_spi3_a, "mout_sclk_spi3", EXYNOS5430_DIV_TOP_PERIC4, 0, 4),
	CDIV(dout_sclk_spi4_a, "mout_sclk_spi4", EXYNOS5430_DIV_TOP_PERIC4, 12, 4),
	CDIV(dout_sclk_spi3_b, "dout_sclk_spi3_a", EXYNOS5430_DIV_TOP_PERIC4, 4, 8),
	CDIV(dout_sclk_spi4_b, "dout_sclk_spi4_a", EXYNOS5430_DIV_TOP_PERIC4, 16, 8),
	CDIV(dout_aclk_g3d_400, "mout_bus_pll_user", EXYNOS5430_DIV_TOP4, 8, 3),
	/* DIV clocks for EGL */
	CDIV(dout_pclk_dbg_egl, "dout_atclk_egl", EXYNOS5430_DIV_EGL0, 20, 3),
	CDIV(dout_cntclk_egl, "dout_egl2", EXYNOS5430_DIV_EGL0, 24, 3),
	/* DIV clocks for MIF */
	CDV_A(dout_aclk_mif_266, "dout_aclk_mif_266", "mout_bus_pll_div2", EXYNOS5430_DIV_MIF2, 20, 3, "dout_aclk_mif_266"),
	/* DIV clocks for G3D */
	CDIV(dout_aclk_g3d, "mout_aclk_g3d_400", EXYNOS5430_DIV_G3D, 0, 3),
	CDIV(dout_pclk_g3d, "dout_aclk_g3d", EXYNOS5430_DIV_G3D, 4, 3),
	CDIV(dout_sclk_hpm_g3d, "mout_g3d_pll", EXYNOS5430_DIV_G3D, 8, 2),
	/* DIV clocks for DISP */
	CDIV(dout_sclk_decon_tv_vclk, "mout_sclk_decon_tv_vclk_c", EXYNOS5430_DIV_MIF4, 20, 4),
	CDIV(dout_sclk_decon_tv_vclk_disp, "mout_sclk_decon_tv_vclk_a_disp", EXYNOS5430_DIV_DISP, 20, 3),
	CDIV(dout_sclk_dsim1, "mout_sclk_dsim1_c", EXYNOS5430_DIV_MIF4, 24, 4),
	CDIV(dout_sclk_dsim1_disp, "mout_sclk_dsim1_b_disp", EXYNOS5430_DIV_DISP, 24, 3),
	/* DIV clocks for AUD */
	CDIV(dout_atclk_aud, "dout_aud_ca5", EXYNOS5430_DIV_AUD0, 12, 5),
};

#define CGTE(_id, cname, pname, o, b, f, gf) \
		GATE(_id, cname, pname, (unsigned long)o, b, f, gf)
struct samsung_gate_clock exynos5433_gate_clks[] __initdata = {
	/* TOP */
	CGTE(aclk_g2d_400, "aclk_g2d_400", "dout_aclk_g2d_400", EXYNOS5430_ENABLE_ACLK_TOP, 0, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_g2d_266, "aclk_g2d_266", "dout_aclk_g2d_266", EXYNOS5430_ENABLE_ACLK_TOP, 2, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_hevc_400, "aclk_hevc_400", "dout_aclk_hevc_400", EXYNOS5430_ENABLE_ACLK_TOP, 5, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_isp_400, "aclk_isp_400", "dout_aclk_isp_400", EXYNOS5430_ENABLE_ACLK_TOP, 6, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_isp_dis_400, "aclk_isp_dis_400", "dout_aclk_isp_dis_400", EXYNOS5430_ENABLE_ACLK_TOP, 7, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE_A(aclk_cam0_552, "dout_aclk_cam0_552", EXYNOS5430_ENABLE_ACLK_TOP, 8, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0, "aclk_cam0_552"),
	CGTE_A(aclk_cam0_400, "dout_aclk_cam0_400", EXYNOS5430_ENABLE_ACLK_TOP, 9, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0, "aclk_cam0_400"),
	CGTE_A(aclk_cam0_333, "dout_aclk_cam0_333", EXYNOS5430_ENABLE_ACLK_TOP, 10, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0, "aclk_cam0_333"),
	CGTE_A(aclk_cam1_552, "dout_aclk_cam1_552", EXYNOS5430_ENABLE_ACLK_TOP, 11, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0, "aclk_cam1_552"),
	CGTE_A(aclk_cam1_400, "dout_aclk_cam1_400", EXYNOS5430_ENABLE_ACLK_TOP, 12, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0, "aclk_cam1_400"),
	CGTE_A(aclk_cam1_333, "dout_aclk_cam1_333", EXYNOS5430_ENABLE_ACLK_TOP, 13, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0, "aclk_cam1_333"),
	CGTE(aclk_gscl_333, "aclk_gscl_333", "dout_aclk_gscl_333", EXYNOS5430_ENABLE_ACLK_TOP, 14, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_gscl_111, "aclk_gscl_111", "dout_aclk_gscl_111", EXYNOS5430_ENABLE_ACLK_TOP, 15, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_fsys_200, "aclk_fsys_200", "dout_aclk_fsys_200", EXYNOS5430_ENABLE_ACLK_TOP, 18, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	GATE_A(aclk_mscl_400, "aclk_mscl_400", "dout_aclk_mscl_400", (unsigned long)EXYNOS5430_ENABLE_ACLK_TOP, 19, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0, "gate_aclk_mscl_400"),
	CGTE(aclk_peris_66, "aclk_peris_66", "dout_aclk_peris_66_b", EXYNOS5430_ENABLE_ACLK_TOP, 21, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_peric_66, "aclk_peric_66", "dout_aclk_peric_66_b", EXYNOS5430_ENABLE_ACLK_TOP, 22, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_imem_266, "aclk_imem_266", "dout_aclk_imem_266", EXYNOS5430_ENABLE_ACLK_TOP, 23, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_imem_200, "aclk_imem_200", "dout_aclk_imem_200", EXYNOS5430_ENABLE_ACLK_TOP, 24, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),

	/* sclk TOP */

	/* sclk TOP_MSCL */
	CGTE(sclk_jpeg_mscl, "sclk_jpeg_mscl", "dout_sclk_jpeg", EXYNOS5430_ENABLE_SCLK_TOP_MSCL, 0, CLK_IGNORE_UNUSED, 0),

	/* sclk TOP_CAM1 */
	CGTE(sclk_isp_spi0_cam1, "sclk_isp_spi0_cam1", "dout_sclk_isp_spi0_b", EXYNOS5430_ENABLE_SCLK_TOP_CAM1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_spi1_cam1, "sclk_isp_spi1_cam1", "dout_sclk_isp_spi1_b", EXYNOS5430_ENABLE_SCLK_TOP_CAM1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_uart_cam1, "sclk_isp_uart_cam1", "dout_sclk_isp_uart", EXYNOS5430_ENABLE_SCLK_TOP_CAM1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_mctadc_cam1, "sclk_isp_mctadc_cam1", "oscclk", EXYNOS5430_ENABLE_SCLK_TOP_CAM1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_sensor0, "sclk_isp_sensor0", "dout_sclk_isp_sensor0_b", EXYNOS5430_ENABLE_SCLK_TOP_CAM1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_sensor1, "sclk_isp_sensor1", "dout_sclk_isp_sensor1_b", EXYNOS5430_ENABLE_SCLK_TOP_CAM1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_sensor2, "sclk_isp_sensor2", "dout_sclk_isp_sensor2_b", EXYNOS5430_ENABLE_SCLK_TOP_CAM1, 7, CLK_IGNORE_UNUSED, 0),

	/* sclk TOP_FSYS */
	CGTE(sclk_usbdrd30_fsys, "sclk_usbdrd30_fsys", "dout_usbdrd30", EXYNOS5430_ENABLE_SCLK_TOP_FSYS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_ufsunipro_fsys, "sclk_ufsunipro_fsys", "dout_sclk_ufsunipro", EXYNOS5430_ENABLE_SCLK_TOP_FSYS, 3, 0, 0),
	CGTE(sclk_mmc0_fsys, "sclk_mmc0_fsys", "dout_mmc0_b", EXYNOS5430_ENABLE_SCLK_TOP_FSYS, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_mmc1_fsys, "sclk_mmc1_fsys", "dout_mmc1_b", EXYNOS5430_ENABLE_SCLK_TOP_FSYS, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_mmc2_fsys, "sclk_mmc2_fsys", "dout_mmc2_b", EXYNOS5430_ENABLE_SCLK_TOP_FSYS, 6, CLK_IGNORE_UNUSED, 0),

	/* sclk TOP_PERIC */
	CGTE(sclk_spi0_peric, "sclk_spi0_peric", "dout_sclk_spi0_b", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spi1_peric, "sclk_spi1_peric", "dout_sclk_spi1_b", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spi2_peric, "sclk_spi2_peric", "dout_sclk_spi2_b", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_uart0_peric, "sclk_uart0_peric", "dout_sclk_uart0", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_uart1_peric, "sclk_uart1_peric", "dout_sclk_uart1", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_uart2_peric, "sclk_uart2_peric", "dout_sclk_uart2", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_pcm1_peric, "sclk_pcm1_peric", "dout_sclk_pcm1", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_i2s1_peric, "sclk_i2s1_peric", "dout_sclk_i2s1", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spdif_peric, "sclk_spdif_peric", "mout_sclk_spdif", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 9, CLK_IGNORE_UNUSED, 0),

	/* MIF */
	CGTE(sclk_hpm_mif, "sclk_hpm_mif", "dout_sclk_hpm_mif", EXYNOS5430_ENABLE_SCLK_MIF, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_cpif_200, "aclk_cpif_200", "dout_aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_MIF3, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_disp_333, "aclk_disp_333", "dout_aclk_disp_333", EXYNOS5430_ENABLE_ACLK_MIF3, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bus2_400, "aclk_bus2_400", "dout_aclk_bus2_400", EXYNOS5430_ENABLE_ACLK_MIF3, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_decon_eclk_disp, "sclk_decon_eclk_disp", "dout_sclk_decon_eclk", EXYNOS5430_ENABLE_SCLK_MIF, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_decon_vclk_disp, "sclk_decon_vclk_disp", "dout_sclk_decon_vclk", EXYNOS5430_ENABLE_SCLK_MIF, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_decon_tv_eclk_disp, "sclk_decon_tv_eclk_disp", "dout_sclk_decon_tv_eclk", EXYNOS5430_ENABLE_SCLK_MIF, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_dsd_disp, "sclk_dsd_disp", "dout_sclk_dsd", EXYNOS5430_ENABLE_SCLK_MIF, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_dsim0_disp, "sclk_dsim0_disp", "dout_sclk_dsim0", EXYNOS5430_ENABLE_SCLK_MIF, 9, CLK_IGNORE_UNUSED, 0),

	/* CPIF */
	CGTE(sclk_ufs_mphy, "sclk_ufs_mphy", "dout_sclk_mphy", EXYNOS5430_ENABLE_SCLK_CPIF, 4, 0, 0),
	CGTE(sclk_lli_mphy, "sclk_lli_mphy", "mout_sclk_mphy", EXYNOS5430_ENABLE_SCLK_CPIF, 3, CLK_IGNORE_UNUSED, 0),

	/* DISP */
	CGTE(sclk_decon_eclk, "sclk_decon_eclk", "dout_sclk_decon_eclk_disp", EXYNOS5430_ENABLE_SCLK_DISP, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_decon_vclk, "sclk_decon_vclk", "dout_sclk_decon_vclk_disp", EXYNOS5430_ENABLE_SCLK_DISP, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_hdmi_spdif_disp, "sclk_hdmi_spdif_disp", "mout_sclk_hdmi_spdif", EXYNOS5430_ENABLE_SCLK_DISP, 4, CLK_IGNORE_UNUSED, 0),
	/* CGTE(phyclk_mixer_pixel, "phyclk_mixer_pixel", "mout_phyclk_hdmiphy_pixel_clko_user", EXYNOS5430_ENABLE_SCLK_DISP, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_hdmi_pixel, "phyclk_hdmi_pixel", "mout_phyclk_hdmiphy_pixel_clko_user", EXYNOS5430_ENABLE_SCLK_DISP, 12, CLK_IGNORE_UNUSED, 0), */
	CGTE(phyclk_hdmiphy_tmds_clko, "phyclk_hdmiphy_tmds_clko", "mout_phyclk_hdmiphy_tmds_clko_user", EXYNOS5430_ENABLE_SCLK_DISP, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_mipidphy_rxclkesc0, "phyclk_mipidphy_rxclkesc0", "mout_phyclk_mipidphy_rxclkesc0_user", EXYNOS5430_ENABLE_SCLK_DISP, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_mipidphy_bitclkdiv8, "phyclk_mipidphy_bitclkdiv8", "mout_phyclk_mipidphy_bitclkdiv8_user", EXYNOS5430_ENABLE_SCLK_DISP, 15, CLK_IGNORE_UNUSED, 0),

	/* PERIC */
	CGTE(sclk_pcm1, "sclk_pcm1", "sclk_pcm1_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_i2s1, "sclk_i2s1", "sclk_i2s1_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spi0, "sclk_spi0", "sclk_spi0_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spi1, "sclk_spi1", "sclk_spi1_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spi2, "sclk_spi2", "sclk_spi2_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_uart0, "sclk_uart0", "sclk_uart0_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_uart1, "sclk_uart1", "sclk_uart1_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_uart2, "sclk_uart2", "sclk_uart2_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spdif, "sclk_spdif", "sclk_spdif_peric", EXYNOS5430_ENABLE_SCLK_PERIC, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(ioclk_i2s1_bclk, "ioclk_i2s1_bclk", NULL, EXYNOS5430_ENABLE_SCLK_PERIC, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(ioclk_spi0_clk, "ioclk_spi0_clk", NULL, EXYNOS5430_ENABLE_SCLK_PERIC, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(ioclk_spi1_clk, "ioclk_spi1_clk", NULL, EXYNOS5430_ENABLE_SCLK_PERIC, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(ioclk_spi2_clk, "ioclk_spi2_clk", NULL, EXYNOS5430_ENABLE_SCLK_PERIC, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(ioclk_slimbus_clk, "ioclk_slimbus_clk", NULL, EXYNOS5430_ENABLE_SCLK_PERIC, 14, CLK_IGNORE_UNUSED, 0),

	/* MSCL */
	CGTE(sclk_jpeg, "sclk_jpeg", "mout_sclk_jpeg", EXYNOS5430_ENABLE_SCLK_MSCL, 0, CLK_IGNORE_UNUSED, 0),

	/* CAM1 */
	CGTE(sclk_isp_spi0, "sclk_isp_spi0", "mout_sclk_isp_spi0_user", EXYNOS5430_ENABLE_SCLK_CAM1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_spi1, "sclk_isp_spi1", "mout_sclk_isp_spi1_user", EXYNOS5430_ENABLE_SCLK_CAM1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_uart, "sclk_isp_uart", "mout_sclk_isp_uart_user", EXYNOS5430_ENABLE_SCLK_CAM1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_isp_mtcadc, "sclk_isp_mtcadc", NULL, EXYNOS5430_ENABLE_SCLK_CAM1, 7, CLK_IGNORE_UNUSED, 0),

	/* FSYS */
	CGTE(sclk_usbdrd30, "sclk_usbdrd30", "mout_sclk_usbdrd30_user", EXYNOS5430_ENABLE_SCLK_FSYS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_usbhost30, "sclk_usbhost30", "mout_sclk_usbhost30_user", EXYNOS5430_ENABLE_SCLK_FSYS, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_mmc0, "sclk_mmc0", "mout_sclk_mmc0_user", EXYNOS5430_ENABLE_SCLK_FSYS, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_mmc1, "sclk_mmc1", "mout_sclk_mmc1_user", EXYNOS5430_ENABLE_SCLK_FSYS, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_mmc2, "sclk_mmc2", "mout_sclk_mmc2_user", EXYNOS5430_ENABLE_SCLK_FSYS, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_ufsunipro, "sclk_ufsunipro", "mout_sclk_ufsunipro_user", EXYNOS5430_ENABLE_SCLK_FSYS, 5, 0, 0),
	CGTE(sclk_mphy, "sclk_mphy", "mout_sclk_mphy", EXYNOS5430_ENABLE_SCLK_FSYS, 6, 0, 0),
	CGTE(phyclk_usbdrd30_udrd30_phyclock, "phyclk_usbdrd30_udrd30_phyclock", "mout_phyclk_usbdrd30_udrd30_phyclock", EXYNOS5430_ENABLE_SCLK_FSYS, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_usbdrd30_udrd30_pipe_pclk, "phyclk_usbdrd30_udrd30_pipe_pclk", "mout_phyclk_usbdrd30_udrd30_pipe_pclk", EXYNOS5430_ENABLE_SCLK_FSYS, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_usbhost20_phy_freeclk, "phyclk_usbhost20_phy_freeclk", "mout_phyclk_usbhost20_phy_freeclk", EXYNOS5430_ENABLE_SCLK_FSYS, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_usbhost20_phy_phyclock, "phyclk_usbhost20_phy_phyclock", "mout_phyclk_usbhost20_phy_phyclock", EXYNOS5430_ENABLE_SCLK_FSYS, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_usbhost20_phy_clk48mohci, "phyclk_usbhost20_phy_clk48mohci", "mout_phyclk_usbhost20_phy_clk48mohci", EXYNOS5430_ENABLE_SCLK_FSYS, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_usbhost20_phy_hsic1, "phyclk_usbhost20_phy_hsic1", "mout_phyclk_usbhost20_phy_hsic1", EXYNOS5430_ENABLE_SCLK_FSYS, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_usbhost30_uhost30_phyclock, "phyclk_usbhost30_uhost_phyclock", "mout_phyclk_usbhost30_uhost30_phyclock", EXYNOS5430_ENABLE_SCLK_FSYS, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_usbhost30_uhost30_pipe_pclk, "phyclk_usbhost30_uhost_pipe_pclk", "mout_phyclk_usbhost30_uhost30_pipe_pclk", EXYNOS5430_ENABLE_SCLK_FSYS, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_ufs_tx0_symbol, "phyclk_ufs_tx0_symbol", "mout_phyclk_ufs_tx0_symbol_user", EXYNOS5430_ENABLE_SCLK_FSYS, 13, 0, 0),
	CGTE(phyclk_ufs_tx1_symbol, "phyclk_ufs_tx1_symbol", "mout_phyclk_ufs_tx1_symbol_user", EXYNOS5430_ENABLE_SCLK_FSYS, 14, 0, 0),
	CGTE(phyclk_ufs_rx0_symbol, "phyclk_ufs_rx0_symbol", "mout_phyclk_ufs_rx0_symbol_user", EXYNOS5430_ENABLE_SCLK_FSYS, 15, 0, 0),
	CGTE(phyclk_ufs_rx1_symbol, "phyclk_ufs_rx1_symbol", "mout_phyclk_ufs_rx1_symbol_user", EXYNOS5430_ENABLE_SCLK_FSYS, 16, 0, 0),
	CGTE(sclk_pcie, "sclk_pcie", "mout_sclk_pcie_100_user", EXYNOS5430_ENABLE_SCLK_FSYS, 21, CLK_IGNORE_UNUSED, 0),

	/* BUS1 */
	CGTE(aclk_bus1nd_400, "aclk_bus1nd_400", "mout_aclk_bus1_400_user", EXYNOS5430_ENABLE_ACLK_BUS1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bus1sw2nd_400, "aclk_bus1sw2nd_400", "mout_aclk_bus1_400_user", EXYNOS5430_ENABLE_ACLK_BUS1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bus1np_133, "aclk_bus1np_133", "dout_pclk_bus1_133", EXYNOS5430_ENABLE_ACLK_BUS1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bus1sw2np_133, "aclk_bus1sw2np_133", "dout_pclk_bus1_133", EXYNOS5430_ENABLE_ACLK_BUS1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_bus1p, "aclk_ahb2apb_bus1p", "dout_pclk_bus1_133", EXYNOS5430_ENABLE_ACLK_BUS1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bus1srvnd_133, "pclk_bus1srvnd_133", "dout_pclk_bus1_133", EXYNOS5430_ENABLE_PCLK_BUS1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_bus1, "pclk_sysreg_bus1", "dout_pclk_bus1_133", EXYNOS5430_ENABLE_PCLK_BUS1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_bus1, "pclk_pmu_bus1", "dout_pclk_bus1_133", EXYNOS5430_ENABLE_PCLK_BUS1, 0, CLK_IGNORE_UNUSED, 0),

	/* BUS2 */
	CGTE(aclk_bus2rtnd_400, "aclk_bus2rtnd_400", "mout_aclk_bus2_400_user", EXYNOS5430_ENABLE_ACLK_BUS2, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bus2bend_400, "aclk_bus2bend_400", "mout_aclk_bus2_400_user", EXYNOS5430_ENABLE_ACLK_BUS2, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bus2np_133, "aclk_bus2np_133", "dout_pclk_bus2_133", EXYNOS5430_ENABLE_ACLK_BUS2, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_bus2p, "aclk_ahb2apb_bus2p", "dout_pclk_bus2_133", EXYNOS5430_ENABLE_ACLK_BUS2, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bus2srvnd_133, "pclk_bus2srvnd_133", "dout_pclk_bus2_133", EXYNOS5430_ENABLE_PCLK_BUS2, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_bus2, "pclk_sysreg_bus2", "dout_pclk_bus2_133", EXYNOS5430_ENABLE_PCLK_BUS2, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_bus2, "pclk_pmu_bus2", "dout_pclk_bus2_133", EXYNOS5430_ENABLE_PCLK_BUS2, 1, CLK_IGNORE_UNUSED, 0),

	/* G2D */
	CGTE(aclk_g2d, "aclk_g2d", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_g2dnd_400, "aclk_g2dnd_400", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_g2dx, "aclk_xiu_g2dx", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxi_sysx, "aclk_asyncaxi_sysx", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axius_g2dx, "aclk_axius_g2dx", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_alb_g2d, "aclk_alb_g2d", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_g2d, "aclk_bts_g2d", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_g2d, "aclk_smmu_g2d", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D_SECURE_SMMU_G2D, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_g2dx, "aclk_ppmu_g2dx", "mout_aclk_g2d_400_user", EXYNOS5430_ENABLE_ACLK_G2D, 13, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_mdma1, "aclk_mdma1", "mout_aclk_g2d_266_user", EXYNOS5430_ENABLE_ACLK_G2D, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_mdma1, "aclk_bts_mdma1", "mout_aclk_g2d_266_user", EXYNOS5430_ENABLE_ACLK_G2D, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_mdma1, "aclk_smmu_mdma1", "mout_aclk_g2d_266_user", EXYNOS5430_ENABLE_ACLK_G2D, 12, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_g2dnp_133, "aclk_g2dnp_133", "dout_pclk_g2d", EXYNOS5430_ENABLE_ACLK_G2D, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_g2d0p, "aclk_ahb2apb_g2d0p", "dout_pclk_g2d", EXYNOS5430_ENABLE_ACLK_G2D, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_g2d1p, "aclk_ahb2apb_g2d1p", "dout_pclk_g2d", EXYNOS5430_ENABLE_ACLK_G2D, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_g2d, "pclk_g2d", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_g2d, "pclk_sysreg_g2d", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_g2d, "pclk_pmu_g2d", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_sysx, "pclk_asyncaxi_sysx", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_alb_g2d, "pclk_alb_g2d", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_g2d, "pclk_bts_g2d", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_mdma1, "pclk_bts_mdma1", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_g2d, "pclk_smmu_g2d", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D_SECURE_SMMU_G2D, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_mdma1, "pclk_smmu_mdma1", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_g2d, "pclk_ppmu_g2d", "dout_pclk_g2d", EXYNOS5430_ENABLE_PCLK_G2D, 8, CLK_IGNORE_UNUSED, 0),

	/* GSCL */
	CGTE(aclk_gscl0, "aclk_gscl0", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_gscl1, "aclk_gscl1", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_gscl2, "aclk_gscl2", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_gsd, "aclk_gsd", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_gsclbend_333, "aclk_gsclbend_333", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_gsclrtnd_333, "aclk_gsclrtnd_333", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_gsclx, "clk_xiu_gsclx", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_gscl0, "clk_bts_gscl0", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_gscl1, "clk_bts_gscl1", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_gscl2, "clk_bts_gscl2", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_gscl0, "aclk_smmu_gscl0", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL_SECURE_SMMU_GSCL0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_gscl1, "aclk_smmu_gscl1", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL_SECURE_SMMU_GSCL1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_gscl2, "aclk_smmu_gscl2", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL_SECURE_SMMU_GSCL2, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_gscl0, "aclk_ppmu_gscl0", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_gscl1, "aclk_ppmu_gscl1", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_gscl2, "aclk_ppmu_gscl2", "mout_aclk_gscl_333_user", EXYNOS5430_ENABLE_ACLK_GSCL, 17, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_ahb2apb_gsclp, "aclk_ahb2apb_gsclp", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_ACLK_GSCL, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gscl0, "pclk_gscl0", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gscl1, "pclk_gscl1", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gscl2, "pclk_gscl2", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_gscl, "pclk_sysreg_gscl", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_gscl, "pclk_pmu_gscl", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_gscl0, "pclk_pk_bts_gscl0", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_gscl1, "pclk_pk_bts_gscl1", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_gscl2, "pclk_pk_bts_gscl2", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_gscl0, "pclk_smmu_gscl0", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL_SECURE_SMMU_GSCL0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_gscl1, "pclk_smmu_gscl1", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL_SECURE_SMMU_GSCL1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_gscl2, "pclk_smmu_gscl2", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL_SECURE_SMMU_GSCL2, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_gscl0, "pclk_ppmu_gscl0", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_gscl1, "pclk_ppmu_gscl1", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_gscl2, "pclk_ppmu_gscl2", "mout_aclk_gscl_111_user", EXYNOS5430_ENABLE_PCLK_GSCL, 13, CLK_IGNORE_UNUSED, 0),

	/*MSCL*/
	CGTE(aclk_m2mscaler0, "aclk_m2mscaler0", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_m2mscaler1, "aclk_m2mscaler1", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_jpeg, "aclk_jpeg", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_msclnd_400, "aclk_msclnd_400", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_msclx, "aclk_xiu_msclx", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_m2mscaler0, "aclk_bts_m2mscaler0", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_m2mscaler1, "aclk_bts_m2mscaler1", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_jpeg, "aclk_bts_jpeg", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_m2mscaler0, "aclk_smmu_m2mscaler0", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL_SECURE_SMMU_M2MSCALER0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_m2mscaler1, "aclk_smmu_m2mscaler1", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL_SECURE_SMMU_M2MSCALER1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_jpeg, "aclk_smmu_jpeg", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL_SECURE_SMMU_JPEG, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_m2mscaler0, "aclk_ppmu_m2mscaler0", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_m2mscaler1, "aclk_ppmu_m2mscaler1", "mout_aclk_mscl_400_user", EXYNOS5430_ENABLE_ACLK_MSCL, 11, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_msclnp_100, "aclk_msclnp_100", "dout_pclk_mscl", EXYNOS5430_ENABLE_ACLK_MSCL, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_mscl0p, "aclk_ahb2apb_mscl0p", "dout_pclk_mscl", EXYNOS5430_ENABLE_ACLK_MSCL, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_m2mscaler0, "pclk_m2mscaler0", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_m2mscaler1, "pclk_m2mscaler1", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_jpeg, "pclk_jpeg", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_mscl, "pclk_sysreg_mscl", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_mscl, "pclk_pmu_mscl", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_m2mscaler0, "pclk_bts_m2mscaler0", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_m2mscaler1, "pclk_bts_m2mscaler1", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_jpeg, "pclk_bts_jpeg", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_m2mscaler0, "pclk_smmu_m2mscaler0", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL_SECURE_SMMU_M2MSCALER0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_m2mscaler1, "pclk_smmu_m2mscaler1", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL_SECURE_SMMU_M2MSCALER1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_jpeg, "pclk_smmu_jpeg", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL_SECURE_SMMU_JPEG, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_m2mscaler0, "pclk_ppmu_m2mscaler0", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_m2mscaler1, "pclk_ppmu_m2mscaler1", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_jpeg, "pclk_ppmu_jpeg", "dout_pclk_mscl", EXYNOS5430_ENABLE_PCLK_MSCL, 10, CLK_IGNORE_UNUSED, 0),

	/*FSYS*/
	CGTE(aclk_pdma, "aclk_pdma", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_usbdrd30, "aclk_usbdrd30", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_usbhost30, "aclk_usbhost30", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_usbhost20, "aclk_usbhost20", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_sromc, "aclk_sromc", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ufs, "aclk_ufs", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_mmc0, "aclk_mmc0", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_mmc1, "aclk_mmc1", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_mmc2, "aclk_mmc2", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_tsi, "aclk_tsi", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS0, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_fsysnp_200, "aclk_fsysnp_200", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_fsysnd_200, "aclk_fsysnd_200", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_fsyssx, "aclk_xiu_fsyssx", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_fsysx, "aclk_xiu_fsysx", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb_fsysh, "aclk_ahb_fsysh", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb_usbhs, "aclk_ahb_usbhs", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb_usblinkh, "aclk_ahb_usblinkh", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2axi_usbhs, "aclk_ahb2axi_usbhs", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_fsysp, "aclk_ahb2apb_fsysp", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axius_fsyssx, "aclk_axius_fsyssx", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axius_usbhs, "aclk_axius_usbhs", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axius_pdma, "aclk_axius_pdma", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_usbdrd30, "aclk_bts_usbdrd30", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_usbhost30, "aclk_bts_usbhost30", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_ufs, "aclk_bts_ufs", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_pdma, "aclk_smmu_pdma", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_mmc0, "aclk_smmu_mmc0", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_mmc1, "aclk_smmu_mmc1", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_mmc2, "aclk_smmu_mmc2", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_fsys, "aclk_ppmu_fsys", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_ACLK_FSYS1, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gpio_fsys, "pclk_gpio_fsys", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_fsys, "pclk_pmu_fsys", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_fsys, "pclk_sysreg_fsys", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_usbdrd30, "pclk_bts_usbdrd30", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_usbhost30, "pclk_bts_usbhost30", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_ufs, "pclk_bts_ufs", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_pdma, "pclk_smmu_pdma", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_mmc0, "pclk_smmu_mmc0", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_mmc1, "pclk_smmu_mmc1", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_mmc2, "pclk_smmu_mmc2", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_fsys, "pclk_ppmu_fsys", "mout_aclk_fsys_200_user", EXYNOS5430_ENABLE_PCLK_FSYS, 12, CLK_IGNORE_UNUSED, 0),

	/*DISP*/
	CGTE(aclk_decon, "aclk_decon", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_disp0nd_333, "aclk_disp0nd_333", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_disp1nd_333, "aclk_disp1nd_333", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_disp1x, "aclk_xiu_disp1x", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_decon0x, "aclk_xiu_decon0x", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_decon1x, "aclk_xiu_decon1x", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axius_disp1x, "aclk_axius_disp1x", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_deconm0, "aclk_bts_deconm0", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_deconm1, "aclk_bts_deconm1", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_deconm2, "aclk_bts_deconm2", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_deconm3, "aclk_bts_deconm3", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_decon0x, "aclk_ppmu_decon0x", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 29, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_decon1x, "aclk_ppmu_decon1x", "mout_aclk_disp_333_user", EXYNOS5430_ENABLE_ACLK_DISP1, 30, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_dispnp_100, "aclk_dispnp_100", "dout_pclk_disp", EXYNOS5430_ENABLE_ACLK_DISP1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb_disph, "aclk_ahb_disph", "dout_pclk_disp", EXYNOS5430_ENABLE_ACLK_DISP1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_dispsfr0p, "aclk_ahb2apb_dispsfr0p", "dout_pclk_disp", EXYNOS5430_ENABLE_ACLK_DISP1, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_dispsfr1p, "aclk_ahb2apb_dispsfr1p", "dout_pclk_disp", EXYNOS5430_ENABLE_ACLK_DISP1, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_dispsfr2p, "aclk_ahb2apb_dispsfr2p", "dout_pclk_disp", EXYNOS5430_ENABLE_ACLK_DISP1, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_decon, "pclk_decon", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tv_mixer, "pclk_tv_mixer", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_dsim0, "pclk_dsim0", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_mdnie, "pclk_mdnie", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_mic, "pclk_mic", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hdmi, "pclk_hdmi", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hdmiphy, "pclk_hdmiphy", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_disp, "pclk_sysreg_disp", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_disp, "pclk_pmu_disp", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_tvx, "pclk_asyncaxi_tvx", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_deconm0, "pclk_bts_deconm0", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_deconm1, "pclk_bts_deconm1", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_deconm2, "pclk_bts_deconm2", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_deconm3, "pclk_bts_deconm3", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_deconm4, "pclk_bts_deconm4", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_mixerm0, "pclk_bts_mixerm0", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_mixerm1, "pclk_bts_mixerm1", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_decon0x, "pclk_ppmu_decon0x", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_decon1x, "pclk_ppmu_decon1x", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_tvx, "pclk_ppmu_tvx", "dout_pclk_disp", EXYNOS5430_ENABLE_PCLK_DISP, 23, CLK_IGNORE_UNUSED, 0),

	/*HEVC*/
	CGTE(aclk_hevc, "aclk_hevc", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_hevcnd_400, "aclk_hevcnd_400", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_hevcx, "aclk_xiu_hevcx", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_hevc_0, "aclk_bts_hevc_0", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_hevc_1, "aclk_bts_hevc_1", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_hevc_0, "aclk_smmu_hevc_0", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC_SECURE_SMMU_HEVC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_hevc_1, "aclk_smmu_hevc_1", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC_SECURE_SMMU_HEVC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_hevc_0, "aclk_ppmu_hevc_0", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_hevc_1, "aclk_ppmu_hevc_1", "mout_aclk_hevc_400_user", EXYNOS5430_ENABLE_ACLK_HEVC, 10, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_hevcnp_100, "aclk_hevcnp_100", "dout_pclk_hevc", EXYNOS5430_ENABLE_ACLK_HEVC, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_hevcp, "aclk_ahb2apb_hevcp", "dout_pclk_hevc", EXYNOS5430_ENABLE_ACLK_HEVC, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hevc, "pclk_hevc", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_hevc, "pclk_sysreg_hevc", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_hevc, "pclk_pmu_hevc", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_hevc_0, "pclk_bts_hevc_0", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_hevc_1, "pclk_bts_hevc_1", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_hevc_0, "pclk_smmu_hevc_0", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC_SECURE_SMMU_HEVC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_hevc_1, "pclk_smmu_hevc_1", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC_SECURE_SMMU_HEVC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_hevc_0, "pclk_ppmu_hevc_0", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_hevc_1, "pclk_ppmu_hevc_1", "dout_pclk_hevc", EXYNOS5430_ENABLE_PCLK_HEVC, 8, CLK_IGNORE_UNUSED, 0),

	/*AUD*/
	CGTE(sclk_ca5, "sclk_ca5", "dout_aud_ca5", EXYNOS5430_ENABLE_SCLK_AUD0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_dmac, "aclk_dmac", "dout_aclk_aud", EXYNOS5430_ENABLE_ACLK_AUD, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_sramc, "aclk_sramc", "dout_aclk_aud", EXYNOS5430_ENABLE_ACLK_AUD, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_audnp_133, "aclk_audnp_133", "dout_aclk_aud", EXYNOS5430_ENABLE_ACLK_AUD, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_audnd_133, "aclk_audnd_133", "dout_aclk_aud", EXYNOS5430_ENABLE_ACLK_AUD, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_lpassx, "aclk_xiu_lpassx", "dout_aclk_aud", EXYNOS5430_ENABLE_ACLK_AUD, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axi2apb_lpassp, "aclk_axi2apb_lpassp", "dout_aclk_aud", EXYNOS5430_ENABLE_ACLK_AUD, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axids_lpassp, "aclk_axids_lpassp", "dout_aclk_aud", EXYNOS5430_ENABLE_ACLK_AUD, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sfr_ctrl, "pclk_sfr_ctrl", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_intr_ctrl, "pclk_intr_ctrl", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_timer, "pclk_timer", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2s, "pclk_i2s", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pcm, "pclk_pcm", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_uart, "pclk_uart", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_slimbus_aud, "pclk_slimbus_aud", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_aud, "pclk_sysreg_aud", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_aud, "pclk_pmu_aud", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gpio_aud, "pclk_gpio_aud", "dout_aclk_aud", EXYNOS5430_ENABLE_PCLK_AUD, 9, CLK_IGNORE_UNUSED, 0),

	CGTE(pclk_dbg_aud, "pclk_dbg_aud", "dout_pclk_dbg_aud", EXYNOS5430_ENABLE_SCLK_AUD0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_aud_i2s, "sclk_aud_i2s", "dout_sclk_aud_i2s", EXYNOS5430_SRC_ENABLE_AUD1, 0, CLK_IGNORE_UNUSED, 0),

	/*IMEM*/
	CGTE(aclk_sss, "aclk_sss", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM_SECURE_SSS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_slimsss, "aclk_slimsss", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM_SECURE_SLIMSSS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_rtic, "aclk_rtic", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM_SECURE_RTIC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_imemnd_266, "aclk_imemnd_266", "aclk_imem_266", EXYNOS5430_ENABLE_ACLK_IMEM, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_sssx, "aclk_xiu_sssx", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncahbm_sss_egl, "aclk_asyncahbm_sss_egl", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_alb_imem, "aclk_alb_imem", "aclk_imem_266", EXYNOS5430_ENABLE_ACLK_IMEM, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_sss_cci, "aclk_bts_sss_cci", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_sss_dram, "aclk_bts_sss_dram", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_slimsss, "aclk_bts_slimsss", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_sss_cci, "aclk_smmu_sss_cci", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM_SECURE_SMMU_SSS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_sss_dram, "aclk_smmu_sss_dram", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM_SECURE_SMMU_SSS, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_slimsss, "aclk_smmu_slimsss", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM_SECURE_SMMU_SLIMSSS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_rtic, "aclk_smmu_rtic", "aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_IMEM_SECURE_SMMU_RTIC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_sssx, "aclk_ppmu_sssx", "aclk_imem_266", EXYNOS5430_ENABLE_ACLK_IMEM, 16, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_gic, "aclk_gic", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_int_mem, "aclk_int_mem", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM_SECURE_INT_MEM, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_pimemx, "aclk_xiu_pimemx", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axi2apb_imem0p, "aclk_axi2apb_imem0p", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axi2apb_imem1p, "aclk_axi2apb_imem1p", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_mif_pimemx, "aclk_asyncaxis_mif_pimemx", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axids_pimemx_gic, "aclk_axids_pimemx_gic", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axids_pimemx_imem0p, "aclk_axids_pimemx_imem0p", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axids_pimemx_imem1p, "aclk_axids_pimemx_imem1p", "aclk_imem_200", EXYNOS5430_ENABLE_ACLK_IMEM, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sss, "pclk_sss", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM_SECURE_SSS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_slimsss, "pclk_slimsss", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM_SECURE_SLIMSSS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_rtic, "pclk_rtic", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM_SECURE_RTIC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_imem, "pclk_sysreg_imem", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_imem, "pclk_pmu_imem", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_alb_imem, "pclk_alb_imem", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_sss_cci, "pclk_bts_sss_cci", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_sss_dram, "pclk_bts_sss_dram", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_slimsss, "pclk_bts_slimsss", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_sss_cci, "pclk_smmu_sss_cci", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM_SECURE_SMMU_SSS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_sss_dram, "pclk_smmu_sss_dram", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM_SECURE_SMMU_SSS, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_slimsss, "pclk_smmu_slimsss", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM_SECURE_SMMU_SLIMSSS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_rtic, "pclk_smmu_rtic", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM_SECURE_SMMU_RTIC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_sssx, "pclk_ppmu_sssx", "aclk_imem_200", EXYNOS5430_ENABLE_PCLK_IMEM, 13, CLK_IGNORE_UNUSED, 0),

	/*PERIS*/
	CGTE(aclk_perisnp_66, "aclk_perisnp_66", "aclk_peris_66", EXYNOS5430_ENABLE_ACLK_PERIS, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_peris0p, "aclk_ahb2apb_peris0p", "aclk_peris_66", EXYNOS5430_ENABLE_ACLK_PERIS, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_peris1p, "aclk_ahb2apb_peris1p", "aclk_peris_66", EXYNOS5430_ENABLE_ACLK_PERIS, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc0, "pclk_tzpc0", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc1, "pclk_tzpc1", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc2, "pclk_tzpc2", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc3, "pclk_tzpc3", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc4, "pclk_tzpc4", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc5, "pclk_tzpc5", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc6, "pclk_tzpc6", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc7, "pclk_tzpc7", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc8, "pclk_tzpc8", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc9, "pclk_tzpc9", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc10, "pclk_tzpc10", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc11, "pclk_tzpc11", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tzpc12, "pclk_tzpc12", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TZPC, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_seckey_apbif, "pclk_seckey_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_SECKEY_APBIF, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hdmi_cec, "pclk_hdmi_cec", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_mct, "pclk_mct", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_wdt_egl, "pclk_wdt_egl", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_wdt_kfc, "pclk_wdt_kfc", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_chipid_apbif, "pclk_chipid_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_CHIPID_APBIF, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_toprtc, "pclk_toprtc", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_TOPRTC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_cmu_top_apbif, "pclk_cmu_top_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_peris, "pclk_sysreg_peris", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tmu0_apbif, "pclk_tmu0_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 23, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_tmu1_apbif, "pclk_tmu1_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 24, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_custom_efuse_apbif, "pclk_custom_efuse_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_CUSTOM_EFUSE_APBIF, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_antirbk_cnt_apbif, "pclk_antirbk_cnt_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS_SECURE_ANTIRBK_CNT_APBIF, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_efuse_writer0_apbif, "pclk_efuse_writer0_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 28, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_efuse_writer1_apbif, "pclk_efuse_writer1_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 29, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hpm_apbif, "pclk_hpm_apbif", "aclk_peris_66", EXYNOS5430_ENABLE_PCLK_PERIS, 30, CLK_IGNORE_UNUSED, 0),

	/*PERIC*/
	CGTE(aclk_ahb2apb_peric0p, "aclk_ahb2apb_peric0p", "aclk_peric_66", EXYNOS5430_ENABLE_ACLK_PERIC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_peric1p, "aclk_ahb2apb_peric1p", "aclk_peric_66", EXYNOS5430_ENABLE_ACLK_PERIC, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_peric2p, "aclk_ahb2apb_peric2p", "aclk_peric_66", EXYNOS5430_ENABLE_ACLK_PERIC, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2c0, "pclk_i2c0", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2c1, "pclk_i2c1", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2c2, "pclk_i2c2", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2c3, "pclk_i2c3", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2c4, "pclk_i2c4", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2c5, "pclk_i2c5", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2c6, "pclk_i2c6", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2c7, "pclk_i2c7", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c0, "pclk_hsi2c0", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c1, "pclk_hsi2c1", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c2, "pclk_hsi2c2", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c3, "pclk_hsi2c3", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c4, "pclk_hsi2c4", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c5, "pclk_hsi2c5", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c6, "pclk_hsi2c6", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c7, "pclk_hsi2c7", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c8, "pclk_hsi2c8", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c9, "pclk_hsi2c9", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c10, "pclk_hsi2c10", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_hsi2c11, "pclk_hsi2c11", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_uart0, "pclk_uart0", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_uart1, "pclk_uart1", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_uart2, "pclk_uart2", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gpio_peric, "pclk_gpio_peric", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gpio_nfc, "pclk_gpio_nfc", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gpio_touch, "pclk_gpio_touch", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_spi0, "pclk_spi0", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_spi1, "pclk_spi1", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_spi2, "pclk_spi2", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 23, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_i2s1, "pclk_i2s1", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 24, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pcm1, "pclk_pcm1", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 25, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_spdif, "pclk_spdif", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 26, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_slimbus, "pclk_slimbus", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 27, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pwm, "pclk_pwm", "aclk_peric_66", EXYNOS5430_ENABLE_PCLK_PERIC, 28, CLK_IGNORE_UNUSED, 0),

	/*G3D*/
	CGTE(aclk_g3d, "aclk_g3d", "dout_aclk_g3d", EXYNOS5430_ENABLE_ACLK_G3D, 0, CLK_IGNORE_UNUSED, 0),

	/*MIF*/
	CGTE(aclk_mifnm_200, "aclk_mifnm_200", "dout_aclk_mifnm_200", EXYNOS5430_ENABLE_ACLK_MIF1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_cp0, "aclk_asyncaxis_cp0", "dout_aclk_mifnm_200", EXYNOS5430_ENABLE_ACLK_MIF1, 23, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_cp1, "aclk_asyncaxis_cp1", "dout_aclk_mifnm_200", EXYNOS5430_ENABLE_ACLK_MIF1, 24, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_mifnd_133, "aclk_mifnd_133", "dout_aclk_mifnd_133", EXYNOS5430_ENABLE_ACLK_MIF1, 2, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_mifnp_133, "aclk_mifnp_133", "dout_aclk_mif_133", EXYNOS5430_ENABLE_ACLK_MIF1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_mif0p, "aclk_ahb2apb_mif0p", "dout_aclk_mif_133", EXYNOS5430_ENABLE_ACLK_MIF1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_mif1p, "aclk_ahb2apb_mif1p", "dout_aclk_mif_133", EXYNOS5430_ENABLE_ACLK_MIF1, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_mif2p, "aclk_ahb2apb_mif2p", "dout_aclk_mif_133", EXYNOS5430_ENABLE_ACLK_MIF1, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncapbs_mif_cssys, "aclk_asyncapbs_mif_cssys", "dout_aclk_mif_133", EXYNOS5430_ENABLE_ACLK_MIF2, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_drex0, "pclk_drex0", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_drex1, "pclk_drex1", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_drex0_tz, "pclk_drex0_tz", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF_SECURE_DREX0_TZ, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_drex1_tz, "pclk_drex1_tz", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF_SECURE_DREX1_TZ, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ddr_phy0, "pclk_ddr_phy0", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ddr_phy1, "pclk_ddr_phy1", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_apbif, "pclk_pmu_apbif", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_abb, "pclk_abb", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_monotonic_cnt, "pclk_monotonic_cnt", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF_SECURE_MONOTONIC_CNT, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_rtc, "pclk_rtc", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF_SECURE_RTC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gpio_alive, "pclk_gpio_alive", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_mif, "pclk_sysreg_mif", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_mif, "pclk_pmu_mif", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_mifsrvnd_133, "pclk_mifsrvnd_133", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_drex0_0, "pclk_asyncaxi_drex0_0", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_drex0_1, "pclk_asyncaxi_drex0_1", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_drex0_3, "pclk_asyncaxi_drex0_3", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_drex1_0, "pclk_asyncaxi_drex1_0", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_drex1_1, "pclk_asyncaxi_drex1_1", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_drex1_3, "pclk_asyncaxi_drex1_3", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_cp0, "pclk_asyncaxi_cp0", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_cp1, "pclk_asyncaxi_cp1", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_asyncaxi_noc_p_cci, "pclk_asyncaxi_noc_p_cci", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_egl, "pclk_bts_egl", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_kfc, "pclk_bts_kfc", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 23, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_drex0s0, "pclk_ppmu_drex0s0", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 24, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_drex0s1, "pclk_ppmu_drex0s1", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 25, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_drex0s3, "pclk_ppmu_drex0s3", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 26, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_drex1s0, "pclk_ppmu_drex1s0", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 27, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_drex1s1, "pclk_ppmu_drex1s1", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 28, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_drex1s3, "pclk_ppmu_drex1s3", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 29, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_egl, "pclk_ppmu_egl", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 30, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_kfc, "pclk_ppmu_kfc", "dout_aclk_mif_133", EXYNOS5430_ENABLE_PCLK_MIF, 31, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_cci, "aclk_cci", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_mifnd_400, "aclk_mifnd_400", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ixiu_cci, "aclk_ixiu_cci", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_drex0_0, "aclk_asyncaxis_drex0_0", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_drex0_1, "aclk_asyncaxis_drex0_1", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_drex0_3, "aclk_asyncaxis_drex0_3", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_drex1_0, "aclk_asyncaxis_drex1_0", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_drex1_1, "aclk_asyncaxis_drex1_1", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_drex1_3, "aclk_asyncaxis_drex1_3", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_egl_mif, "aclk_asyncaxim_egl_mif", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF1, 29, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncacem_egl_cci, "aclk_asyncacem_egl_cci", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncacem_kfc_cci, "aclk_asyncacem_kfc_cci", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axisyncdns_cci, "aclk_axisyncdns_cci", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axius_egl_cci, "aclk_axius_egl_cci", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ace_sel_egl, "aclk_ace_sel_egl", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ace_sel_kfc, "aclk_ace_sel_kfc", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_egl, "aclk_bts_egl", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_kfc, "aclk_bts_kfc", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_egl, "aclk_ppmu_egl", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_kfc, "aclk_ppmu_kfc", "dout_aclk_mif_400", EXYNOS5430_ENABLE_ACLK_MIF2, 19, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_xiu_mifsfrx, "aclk_xiu_mifsfrx", "dout_aclk_mif_200", EXYNOS5430_ENABLE_ACLK_MIF1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_noc_p_cci, "aclk_asyncaxis_noc_p_cci", "dout_aclk_mif_200", EXYNOS5430_ENABLE_ACLK_MIF1, 27, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_mif_imem, "aclk_asyncaxis_mif_imem", "dout_aclk_mif_200", EXYNOS5430_ENABLE_ACLK_MIF1, 28, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxis_egl_mif, "aclk_asyncaxis_egl_mif", "dout_aclk_mif_200", EXYNOS5430_ENABLE_ACLK_MIF1, 30, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_egl_ccix, "aclk_asyncaxim_egl_ccix", "dout_aclk_mif_200", EXYNOS5430_ENABLE_ACLK_MIF1, 31, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axisyncdn_noc_d, "aclk_axisyncdn_noc_d", "dout_aclk_mif_200", EXYNOS5430_ENABLE_ACLK_MIF2, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axisyncdn_cci, "aclk_axisyncdn_cci", "dout_aclk_mif_200", EXYNOS5430_ENABLE_ACLK_MIF2, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axids_cci_mifsfrx, "aclk_axids_cci_mifsfrx", "dout_aclk_mif_200", EXYNOS5430_ENABLE_ACLK_MIF2, 7, CLK_IGNORE_UNUSED, 0),

	CGTE(clkm_phy0, "clkm_phy0", "dout_clkm_phy", EXYNOS5430_ENABLE_ACLK_MIF0, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(clkm_phy1, "clkm_phy1", "dout_clkm_phy", EXYNOS5430_ENABLE_ACLK_MIF0, 17, CLK_IGNORE_UNUSED, 0),

	CGTE(clk2x_phy0, "clk2x_phy0", "dout_clk2x_phy", EXYNOS5430_ENABLE_ACLK_MIF0, 18, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_drex0, "aclk_drex0", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex0_busif_rd, "aclk_drex0_busif_rd", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex0_busif_wr, "aclk_drex0_busif_wr", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex0_sch, "aclk_drex0_sch", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex0_memif, "aclk_drex0_memif", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF0, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex0_perev, "aclk_drex0_perev", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF0, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex0_tz, "aclk_drex0_tz", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF0, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_drex0_0, "aclk_asyncaxim_drex0_0", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF1, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_drex0_1, "aclk_asyncaxim_drex0_1", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF1, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_drex0_3, "aclk_asyncaxim_drex0_3", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF1, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_cp0, "aclk_asyncaxim_cp0", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF1, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_drex0s0, "aclk_ppmu_drex0s0", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF2, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_drex0s1, "aclk_ppmu_drex0s1", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF2, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_drex0s3, "aclk_ppmu_drex0s3", "dout_aclk_drex0", EXYNOS5430_ENABLE_ACLK_MIF2, 14, CLK_IGNORE_UNUSED, 0),

	CGTE(clk2x_phy1, "clk2x_phy1", "dout_clk2x_phy", EXYNOS5430_ENABLE_ACLK_MIF0, 19, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_drex1, "aclk_drex1", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex1_busif_rd, "aclk_drex1_busif_rd", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex1_busif_wr, "aclk_drex1_busif_wr", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex1_sch, "aclk_drex1_sch", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF0, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex1_memif, "aclk_drex1_memif", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF0, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex1_perev, "aclk_drex1_perev", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF0, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_drex1_tz, "aclk_drex1_tz", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF0, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_drex1_0, "aclk_asyncaxim_drex1_0", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF1, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_drex1_1, "aclk_asyncaxim_drex1_1", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF1, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_drex1_3, "aclk_asyncaxim_drex1_3", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF1, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncaxim_cp1, "aclk_asyncaxim_cp1", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF1, 24, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_drex1s0, "aclk_ppmu_drex1s0", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF2, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_drex1s1, "aclk_ppmu_drex1s1", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF2, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_drex1s3, "aclk_ppmu_drex1s3", "dout_aclk_drex1", EXYNOS5430_ENABLE_ACLK_MIF2, 17, CLK_IGNORE_UNUSED, 0),

	/*CPIF*/
	CGTE(aclk_mdma0, "aclk_mdma0", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_lli_svc_loc, "aclk_lli_svc_loc", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_lli_svc_rem, "aclk_lli_svc_rem", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_lli_ll_init, "aclk_lli_ll_init", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_lli_be_init, "aclk_lli_be_init", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_lli_ll_targ, "aclk_lli_ll_targ", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_lli_be_targ, "aclk_lli_be_targ", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_cpifnp_200, "aclk_cpifnp_200", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_cpifnm_200, "aclk_cpifnm_200", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_cpifsfrx, "aclk_xiu_cpifsfrx", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_llix, "aclk_xiu_llix", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_cpifp, "aclk_ahb2apb_cpifp", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axius_lli_ll, "aclk_axius_lli_ll", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_axius_lli_be, "aclk_axius_lli_be", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_mdma0, "aclk_bts_mdma0", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_llix, "aclk_ppmu_llix", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_mdma0, "aclk_smmu_mdma0", "aclk_cpif_200", EXYNOS5430_ENABLE_ACLK_CPIF, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_mdma0, "pclk_mdma0", "aclk_cpif_200", EXYNOS5430_ENABLE_PCLK_CPIF, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_cpif, "pclk_sysreg_cpif", "aclk_cpif_200", EXYNOS5430_ENABLE_PCLK_CPIF, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_cpif, "pclk_pmu_cpif", "aclk_cpif_200", EXYNOS5430_ENABLE_PCLK_CPIF, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_gpio_cpif, "pclk_gpio_cpif", "aclk_cpif_200", EXYNOS5430_ENABLE_PCLK_CPIF, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_mdma0, "pclk_bts_mdma0", "aclk_cpif_200", EXYNOS5430_ENABLE_PCLK_CPIF, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_llix, "pclk_ppmu_llix", "aclk_cpif_200", EXYNOS5430_ENABLE_PCLK_CPIF, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_mdma0, "pclk_smmu_mdma0", "aclk_cpif_200", EXYNOS5430_ENABLE_PCLK_CPIF, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_lli_cmn_cfg, "sclk_lli_cmn_cfg", "aclk_cpif_200", EXYNOS5430_ENABLE_SCLK_CPIF, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_lli_tx0_cfg, "sclk_lli_tx0_cfg", "aclk_cpif_200", EXYNOS5430_ENABLE_SCLK_CPIF, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_lli_rx0_cfg, "sclk_lli_rx0_cfg", "aclk_cpif_200", EXYNOS5430_ENABLE_SCLK_CPIF, 2, CLK_IGNORE_UNUSED, 0),

	/* CAM0 */
	CGTE(aclk_csis1, "aclk_csis1", NULL, EXYNOS5430_ENABLE_ACLK_CAM00, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_csis0, "aclk_csis0", NULL, EXYNOS5430_ENABLE_ACLK_CAM00, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_csis1, "pclk_csis1", NULL, EXYNOS5430_ENABLE_PCLK_CAM0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_csis0, "pclk_csis0", NULL, EXYNOS5430_ENABLE_PCLK_CAM0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_phyclk_rxbyteclkhs0_s4, "sclk_phyclk_rxbyteclkhs0_s4", NULL, EXYNOS5430_ENABLE_SCLK_CAM0, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_phyclk_rxbyteclkhs0_s2a, "sclk_phyclk_rxbyteclkhs0_s2a", NULL, EXYNOS5430_ENABLE_SCLK_CAM0, 7, CLK_IGNORE_UNUSED, 0),

	/* IP Gate */
	/* KFC */
	CGTE(gate_freq_det_kfc_pll, "gate_freq_det_kfc_pll", NULL, EXYNOS5430_ENABLE_IP_KFC0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_hpm_kfc, "gate_hpm_kfc", NULL, EXYNOS5430_ENABLE_IP_KFC0, 3, CLK_IGNORE_UNUSED, 0),

	/* EGL */
	CGTE(gate_freq_det_egl_pll, "gate_freq_det_egl_pll", NULL, EXYNOS5430_ENABLE_IP_EGL0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_hpm_egl_enable, "gate_hpm_egl_enable", NULL, EXYNOS5430_ENABLE_IP_EGL0, 5, CLK_IGNORE_UNUSED, 0),

	/* CLKOUT */
	CGTE(clkout, "clk_out", "fin_pll", EXYNOS_PMU_PMU_DEBUG, 0, 0, CLK_GATE_SET_TO_DISABLE),

	/* TOP */
	CGTE(gate_top_cam1, "gate_top_cam1", NULL, EXYNOS5430_ENABLE_IP_TOP, 6, 0, 0),
	CGTE(gate_top_cam0, "gate_top_cam0", NULL, EXYNOS5430_ENABLE_IP_TOP, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_top_isp, "gate_top_isp", NULL, EXYNOS5430_ENABLE_IP_TOP, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_freq_det_aud_pll, "gate_freq_det_aud_pll", NULL, EXYNOS5430_ENABLE_IP_TOP, 16, 0, 0),
	CGTE(gate_freq_det_isp_pll, "gate_freq_det_isp_pll", NULL, EXYNOS5430_ENABLE_IP_TOP, 15, 0, 0),

	CGTE(gate_top_mfc, "gate_top_mfc", NULL, EXYNOS5430_ENABLE_IP_TOP, 1, 0, 0),
	CGTE(gate_top_hevc, "gate_top_hevc", NULL, EXYNOS5430_ENABLE_IP_TOP, 3, 0, 0),
	CGTE(gate_top_gscl, "gate_top_gscl", NULL, EXYNOS5430_ENABLE_IP_TOP, 7, 0, 0),
	CGTE(gate_top_mscl, "gate_top_mscl", NULL, EXYNOS5430_ENABLE_IP_TOP, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_top_g2d, "gate_top_g2d", NULL, EXYNOS5430_ENABLE_IP_TOP, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_top_g3d, "gate_top_g3d", NULL, EXYNOS5430_ENABLE_IP_TOP, 18, 0, 0),

	/* AUD0 */
	CGTE(gate_gpio_aud, "gate_gpio_aud", NULL, EXYNOS5430_ENABLE_IP_AUD0, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pmu_aud, "gate_pmu_aud", NULL, EXYNOS5430_ENABLE_IP_AUD0, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_aud, "gate_sysreg_aud", NULL, EXYNOS5430_ENABLE_IP_AUD0, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_slimbus_aud, "gate_slimbus_aud", NULL, EXYNOS5430_ENABLE_IP_AUD0, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_uart, "gate_uart", NULL, EXYNOS5430_ENABLE_IP_AUD0, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pcm, "gate_pcm", NULL, EXYNOS5430_ENABLE_IP_AUD0, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_i2s, "gate_i2s", NULL, EXYNOS5430_ENABLE_IP_AUD0, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_timer, "gate_timer", NULL, EXYNOS5430_ENABLE_IP_AUD0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_intr_ctrl, "gate_intr_ctrl", NULL, EXYNOS5430_ENABLE_IP_AUD0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sfr_ctrl, "gate_sfr_ctrl", NULL, EXYNOS5430_ENABLE_IP_AUD0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sramc, "gate_sramc", NULL, EXYNOS5430_ENABLE_IP_AUD0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_dmac, "gate_dmac", NULL, EXYNOS5430_ENABLE_IP_AUD0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pclk_dbg, "gate_pclk_dbg", NULL, EXYNOS5430_ENABLE_IP_AUD0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ca5, "gate_ca5", NULL, EXYNOS5430_ENABLE_IP_AUD0, 0, CLK_IGNORE_UNUSED, 0),

	/* AUD1 */
	CGTE(gate_smmu_lpassx, "gate_smmu_lpassx", NULL, EXYNOS5430_ENABLE_IP_AUD1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axids_lpassp, "gate_axids_lpassp", NULL, EXYNOS5430_ENABLE_IP_AUD1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi2apb_lpassp, "gate_axi2apb_lpassp", NULL, EXYNOS5430_ENABLE_IP_AUD1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_lpassx, "gate_xiu_lpassx", NULL, EXYNOS5430_ENABLE_IP_AUD1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_audnp_133, "gate_audnp_133", NULL, EXYNOS5430_ENABLE_IP_AUD1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_audnd_133, "gate_audnd_133", NULL, EXYNOS5430_ENABLE_IP_AUD1, 0, CLK_IGNORE_UNUSED, 0),

	/* BUS */
	CGTE(gate_pmu_bus1, "gate_pmu_bus1", NULL, EXYNOS5430_ENABLE_IP_BUS10, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_bus1, "gate_sysreg_bus1", NULL, EXYNOS5430_ENABLE_IP_BUS10, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_bus1srvnd_133, "gate_bus1srvnd_133", NULL, EXYNOS5430_ENABLE_IP_BUS11, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_bus1p, "gate_ahb2apb_bus1p", NULL, EXYNOS5430_ENABLE_IP_BUS11, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bus1np_133, "gate_bus1np_133", NULL, EXYNOS5430_ENABLE_IP_BUS11, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bus1nd_400, "gate_bus1nd_400", NULL, EXYNOS5430_ENABLE_IP_BUS11, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_pmu_bus2, "gate_pmu_bus2", NULL, EXYNOS5430_ENABLE_IP_BUS20, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_bus2, "gate_sysreg_bus2", NULL, EXYNOS5430_ENABLE_IP_BUS20, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_bus2srvnd_133, "gate_bus2srvnd_133", NULL, EXYNOS5430_ENABLE_IP_BUS21, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_bus2p, "gate_ahb2apb_bus2p", NULL, EXYNOS5430_ENABLE_IP_BUS21, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bus2np_133, "gate_bus2np_133", NULL, EXYNOS5430_ENABLE_IP_BUS21, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bus2bend_400, "gate_bus2bend_400", NULL, EXYNOS5430_ENABLE_IP_BUS21, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bus2rtnd_400, "gate_bus2rtnd_400", NULL, EXYNOS5430_ENABLE_IP_BUS21, 0, CLK_IGNORE_UNUSED, 0),

	/* CPIF0 */
	CGTE(gate_mphy_pll, "gate_mphy_pll", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 19, 0, 0),
	CGTE(gate_mphy_pll_mif, "gate_mphy_pll_mif", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 18, 0, 0),
	CGTE(gate_freq_det_mphy_pll, "gate_freq_det_mphy_pll", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 17, 0, 0),
	CGTE(gate_ufs_mphy, "gate_ufs_mphy", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 16, 0, 0),
	CGTE(gate_lli_mphy, "gate_lli_mphy", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 15, 0, 0),
	CGTE(gate_gpio_cpif, "gate_gpio_cpif", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pmu_cpif, "gate_pmu_cpif", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_cpif, "gate_sysreg_cpif", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lli_rx0_symbol, "gate_lli_rx0_symbol", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 11, 0, 0),
	CGTE(gate_lli_tx0_symbol, "gate_lli_tx0_symbol", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 10, 0, 0),
	CGTE(gate_lli_rx0_cfg, "gate_lli_rx0_cfg", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 9, 0, 0),
	CGTE(gate_lli_tx0_cfg, "gate_lli_tx0_cfg", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 8, 0, 0),
	CGTE(gate_lli_cmn_cfg, "gate_lli_cmn_cfg", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 7, 0, 0),
	CGTE(gate_lli_be_targ, "gate_lli_be_targ", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 6, 0, 0),
	CGTE(gate_lli_ll_targ, "gate_lli_ll_targ", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 5, 0, 0),
	CGTE(gate_lli_be_init, "gate_lli_be_init", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 4, 0, 0),
	CGTE(gate_lli_ll_init, "gate_lli_ll_init", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 3, 0, 0),
	CGTE(gate_lli_svc_rem, "gate_lli_svc_rem", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 2, 0, 0),
	CGTE(gate_lli_svc_loc, "gate_lli_svc_loc", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 1, 0, 0),
	CGTE(gate_mdma0, "gate_mdma0", NULL, EXYNOS5430_ENABLE_IP_CPIF0, 0, 0, 0),

	/* CPIF1 */
	CGTE(gate_smmu_mdma0, "gate_smmu_mdma0", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 9, 0, 0),
	CGTE(gate_ppmu_llix, "gate_ppmu_llix", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 8, 0, 0),
	CGTE(gate_bts_mdma0, "gate_bts_mdma0", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 7, 0, 0),
	CGTE(gate_axius_lli_be, "gate_axius_lli_be", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_lli_ll, "gate_axius_lli_ll", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_cpifp, "gate_ahb2apb_cpifp", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_llix, "gate_xiu_llix", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_cpifsfrx, "gate_xiu_cpifsfrx", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cpifnp_200, "gate_cpifnp_200", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cpifnm_200, "gate_cpifnm_200", NULL, EXYNOS5430_ENABLE_IP_CPIF1, 0, 0, 0),

	/* DISP0 */
	CGTE(gate_mic1, "gate_mic1", NULL, EXYNOS5430_ENABLE_IP_DISP0, 15, 0, 0),
	CGTE(gate_dsim1, "gate_dsim1", NULL, EXYNOS5430_ENABLE_IP_DISP0, 14, 0, 0),
	CGTE(gate_dpu, "gate_dpu", NULL, EXYNOS5430_ENABLE_IP_DISP0, 12, 0, 0),
	CGTE(gate_freq_det_disp_pll, "gate_freq_det_disp_pll", NULL, EXYNOS5430_ENABLE_IP_DISP0, 11, 0, 0),
	CGTE(gate_pmu_disp, "gate_pmu_disp", NULL, EXYNOS5430_ENABLE_IP_DISP0, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_disp, "gate_sysreg_disp", NULL, EXYNOS5430_ENABLE_IP_DISP0, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_dsd, "gate_dsd", NULL, EXYNOS5430_ENABLE_IP_DISP0, 8, 0, 0),
	CGTE(gate_mic, "gate_mic", NULL, EXYNOS5430_ENABLE_IP_DISP0, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_mdnie, "gate_mdnie", NULL, EXYNOS5430_ENABLE_IP_DISP0, 6, 0, 0),
	CGTE(gate_mipidphy, "gate_mipidphy", NULL, EXYNOS5430_ENABLE_IP_DISP0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_dsim0, "gate_dsim0", NULL, EXYNOS5430_ENABLE_IP_DISP0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_decon_tv, "gate_decon_tv", NULL, EXYNOS5430_ENABLE_IP_DISP0, 3, 0, 0),
	CGTE(gate_hdmiphy, "gate_hdmiphy", NULL, EXYNOS5430_ENABLE_IP_DISP0, 2, 0, 0),
	CGTE(gate_hdmi, "gate_hdmi", NULL, EXYNOS5430_ENABLE_IP_DISP0, 1, 0, 0),
	CGTE(gate_decon, "gate_decon", NULL, EXYNOS5430_ENABLE_IP_DISP0, 0, CLK_IGNORE_UNUSED, 0),

	/* DISP1 */
	CGTE(gate_ppmu_tv1x, "gate_ppmu_tv1x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 30, 0, 0),
	CGTE(gate_ppmu_tv0x, "gate_ppmu_tv0x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 29, 0, 0),
	CGTE(gate_ppmu_decon1x, "gate_ppmu_decon1x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 28, 0, 0),
	CGTE(gate_ppmu_decon0x, "gate_ppmu_decon0x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 27, 0, 0),
	CGTE(gate_bts_decontv_m3, "gate_bts_decontv_m3", NULL, EXYNOS5430_ENABLE_IP_DISP1, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_decontv_m2, "gate_bts_decontv_m2", NULL, EXYNOS5430_ENABLE_IP_DISP1, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_decontv_m1, "gate_bts_decontv_m1", NULL, EXYNOS5430_ENABLE_IP_DISP1, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_decontv_m0, "gate_bts_decontv_m0", NULL, EXYNOS5430_ENABLE_IP_DISP1, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_deconm4, "gate_bts_deconm4", NULL, EXYNOS5430_ENABLE_IP_DISP1, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_deconm3, "gate_bts_deconm3", NULL, EXYNOS5430_ENABLE_IP_DISP1, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_deconm2, "gate_bts_deconm2", NULL, EXYNOS5430_ENABLE_IP_DISP1, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_deconm1, "gate_bts_deconm1", NULL, EXYNOS5430_ENABLE_IP_DISP1, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_deconm0, "gate_bts_deconm0", NULL, EXYNOS5430_ENABLE_IP_DISP1, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_dispsfr2p, "gate_ahb2apb_dispsfr2p", NULL, EXYNOS5430_ENABLE_IP_DISP1, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_dispsfr1p, "gate_ahb2apb_dispsfr1p", NULL, EXYNOS5430_ENABLE_IP_DISP1, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_dispsfr0p, "gate_ahb2apb_dispsfr0p", NULL, EXYNOS5430_ENABLE_IP_DISP1, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb_disph, "gate_ahb_disph", NULL, EXYNOS5430_ENABLE_IP_DISP1, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_tv1x, "gate_xiu_tv1x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_tv0x, "gate_xiu_tv0x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_decon1x, "gate_xiu_decon1x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_decon0x, "gate_xiu_decon0x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_disp1x, "gate_xiu_disp1x", NULL, EXYNOS5430_ENABLE_IP_DISP1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_dispnp_100, "gate_dispnp_100", NULL, EXYNOS5430_ENABLE_IP_DISP1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_disp1nd_333, "gate_disp1nd_333", NULL, EXYNOS5430_ENABLE_IP_DISP1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_disp0nd_333, "gate_disp0nd_333", NULL, EXYNOS5430_ENABLE_IP_DISP1, 0, CLK_IGNORE_UNUSED, 0),

	/* FSYS0 */
	CGTE(gate_pcie_phy, "gate_pcie_phy", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pcie, "gate_pcie", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tlnk, "gate_tlnk", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pdma1, "gate_pdma1", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_mphy, "gate_mphy", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 14, 0, 0),
	CGTE(gate_sysreg_fsys, "gate_sysreg_fsys", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pmu_fsys, "gate_pmu_fsys", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_gpio_fsys, "gate_gpio_fsys", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tsi, "gate_tsi", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 10, 0, 0),
	CGTE(gate_mmc2, "gate_mmc2", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_mmc1, "gate_mmc1", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 7, 0, 0),
	CGTE(gate_mmc0, "gate_mmc0", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ufs, "gate_ufs", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 5, 0, 0),
	CGTE(gate_sromc, "gate_sromc", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 4, 0, 0),
	CGTE(gate_usbhost20, "gate_usbhost20", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 3, 0, 0),
	CGTE(gate_usbhost30, "gate_usbhost30", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 2, 0, 0),
	CGTE(gate_usbdrd30, "gate_usbdrd30", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pdma0, "gate_pdma0", NULL, EXYNOS5430_ENABLE_IP_FSYS0, 0, CLK_IGNORE_UNUSED, 0),

	/* FSYS1 */
	CGTE(gate_ppmu_fsys, "gate_ppmu_fsys", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 21, 0, 0),
	CGTE(gate_smmu_mmc2, "gate_smmu_mmc2", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 20, 0, 0),
	CGTE(gate_smmu_mmc1, "gate_smmu_mmc1", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 19, 0, 0),
	CGTE(gate_smmu_mmc0, "gate_smmu_mmc0", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 18, 0, 0),
	CGTE(gate_smmu_pdma, "gate_smmu_pdma", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 17, 0, 0),
	CGTE(gate_bts_ufs, "gate_bts_ufs", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 14, 0, 0),
	CGTE(gate_bts_usbdrd30, "gate_bts_usbdrd30", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 12, 0, 0),
	CGTE(gate_bts_usbhost30, "gate_bts_usbhost30", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 13, 0, 0),
	CGTE(gate_axius_pdma, "gate_axius_pdma", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_usbhs, "gate_axius_usbhs", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_fsyssx, "gate_axius_fsyssx", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_fsysp, "gate_ahb2apb_fsysp", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2axi_usbhs, "gate_ahb2axi_usbhs", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb_usblinkh, "gate_ahb_usblinkh", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb_usbhs, "gate_ahb_usbhs", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb_fsysh, "gate_ahb_fsysh", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_fsysx, "gate_xiu_fsysx", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_fsyssx, "gate_xiu_fsyssx", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_fsysnp_200, "gate_fsysnp_200", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_fsysnd_200, "gate_fsysnd_200", NULL, EXYNOS5430_ENABLE_IP_FSYS1, 0, CLK_IGNORE_UNUSED, 0),

	/* G2D */
	CGTE(gate_pmu_g2d, "gate_pmu_g2d", NULL, EXYNOS5430_ENABLE_IP_G2D0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_g2d, "gate_sysreg_g2d", NULL, EXYNOS5430_ENABLE_IP_G2D0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_mdma1, "gate_mdma1", NULL, EXYNOS5430_ENABLE_IP_G2D0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_g2d, "gate_g2d", NULL, EXYNOS5430_ENABLE_IP_G2D0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_g2dx, "gate_ppmu_g2dx", NULL, EXYNOS5430_ENABLE_IP_G2D1, 12, 0, 0),
	CGTE(gate_smmu_mdma1, "gate_smmu_mdma1", NULL, EXYNOS5430_ENABLE_IP_G2D1, 11, 0, 0),
	CGTE(gate_bts_mdma1, "gate_bts_mdma1", NULL, EXYNOS5430_ENABLE_IP_G2D1, 9, 0, 0),
	CGTE(gate_bts_g2d, "gate_bts_g2d", NULL, EXYNOS5430_ENABLE_IP_G2D1, 8, 0, 0),
	CGTE(gate_alb_g2d, "gate_alb_g2d", NULL, EXYNOS5430_ENABLE_IP_G2D1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_g2dx, "gate_axius_g2dx", NULL, EXYNOS5430_ENABLE_IP_G2D1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_sysx, "gate_asyncaxi_sysx", NULL, EXYNOS5430_ENABLE_IP_G2D1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_g2d1p, "gate_ahb2apb_g2d1p", NULL, EXYNOS5430_ENABLE_IP_G2D1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_g2d0p, "gate_ahb2apb_g2d0p", NULL, EXYNOS5430_ENABLE_IP_G2D1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_g2dx, "gate_xiu_g2dx", NULL, EXYNOS5430_ENABLE_IP_G2D1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_g2dnp_133, "gate_g2dnp_133", NULL, EXYNOS5430_ENABLE_IP_G2D1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_g2dnd_400, "gate_g2dnd_400", NULL, EXYNOS5430_ENABLE_IP_G2D1, 0, CLK_IGNORE_UNUSED, 0),

	/* G3D */
	CGTE(gate_freq_det_g3d_pll, "gate_freq_det_g3d_pll", NULL, EXYNOS5430_ENABLE_IP_G3D0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_hpm_g3d, "gate_hpm_g3d", NULL, EXYNOS5430_ENABLE_IP_G3D0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pmu_g3d, "gate_pmu_g3d", NULL, EXYNOS5430_ENABLE_IP_G3D0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_g3d, "gate_sysreg_g3d", NULL, EXYNOS5430_ENABLE_IP_G3D0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_g3d, "gate_g3d", NULL, EXYNOS5430_ENABLE_IP_G3D0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_g3d1, "gate_ppmu_g3d1", NULL, EXYNOS5430_ENABLE_IP_G3D1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ppmu_g3d0, "gate_ppmu_g3d0", NULL, EXYNOS5430_ENABLE_IP_G3D1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_g3d1, "gate_bts_g3d1", NULL, EXYNOS5430_ENABLE_IP_G3D1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_g3d0, "gate_bts_g3d0", NULL, EXYNOS5430_ENABLE_IP_G3D1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncapb_g3d, "gate_asyncapb_g3d", NULL, EXYNOS5430_ENABLE_IP_G3D1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_g3dp, "gate_ahb2apb_g3dp", NULL, EXYNOS5430_ENABLE_IP_G3D1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_g3dnp_150, "gate_g3dnp_150", NULL, EXYNOS5430_ENABLE_IP_G3D1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_g3dnd_600, "gate_g3dnd_600", NULL, EXYNOS5430_ENABLE_IP_G3D1, 0, CLK_IGNORE_UNUSED, 0),

	/* MIF */
	CGTE(gate_disp_333, "gate_disp_333", NULL, EXYNOS5430_ENABLE_IP_MIF3, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cpif_200, "gate_cpif_200", NULL, EXYNOS5430_ENABLE_IP_MIF3, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_decon_eclk, "gate_decon_eclk", NULL, EXYNOS5430_ENABLE_IP_MIF3, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_decon_vclk, "gate_decon_vclk", NULL, EXYNOS5430_ENABLE_IP_MIF3, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_decontv_eclk, "gate_decontv_eclk", NULL, EXYNOS5430_ENABLE_IP_MIF3, 7, 0, 0),
	CGTE(gate_dsd_clk, "gate_dsd_clk", NULL, EXYNOS5430_ENABLE_IP_MIF3, 8, 0, 0),
	CGTE(gate_dsim0_clk, "gate_dsim0_clk", NULL, EXYNOS5430_ENABLE_IP_MIF3, 9, CLK_IGNORE_UNUSED, 0),

	/* GSCL */
	CGTE(gate_pmu_gscl, "gate_pmu_gscl", NULL, EXYNOS5430_ENABLE_IP_GSCL0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_gscl, "gate_sysreg_gscl", NULL, EXYNOS5430_ENABLE_IP_GSCL0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_gsd, "gate_gsd", NULL, EXYNOS5430_ENABLE_IP_GSCL0, 3,	0, 0),
	CGTE(gate_gscl2, "gate_gscl2", NULL, EXYNOS5430_ENABLE_IP_GSCL0, 2, 0, 0),
	CGTE(gate_gscl1, "gate_gscl1", NULL, EXYNOS5430_ENABLE_IP_GSCL0, 1, 0, 0),
	CGTE(gate_gscl0, "gate_gscl0", NULL, EXYNOS5430_ENABLE_IP_GSCL0, 0, 0, 0),
	CGTE(gate_ppmu_gscl2, "gate_ppmu_gscl2", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 13, 0, 0),
	CGTE(gate_ppmu_gscl1, "gate_ppmu_gscl1", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 12, 0, 0),
	CGTE(gate_ppmu_gscl0, "gate_ppmu_gscl0", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 11, 0, 0),

	CGTE(gate_bts_gscl2, "gate_bts_gscl2", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_gscl1, "gate_bts_gscl1", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_gscl0, "gate_bts_gscl0", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_gsclp, "gate_ahb2apb_gsclp", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_gsclx, "gate_xiu_gsclx", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_gsclnp_111, "gate_gsclnp_111", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_gsclrtnd_333, "gate_gsclrtnd_333", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_gsclbend_333, "gate_gsclbend_333", NULL, EXYNOS5430_ENABLE_IP_GSCL1, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_smmu_gscl0, "gate_smmu_gscl0", NULL, EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL0, 0, 0, 0),

	CGTE(gate_smmu_gscl1, "gate_smmu_gscl1", NULL, EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL1, 0, 0, 0),

	CGTE(gate_smmu_gscl2, "gate_smmu_gscl2", NULL, EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL2, 0, 0, 0),

	/* HEVC */
	CGTE(gate_pmu_hevc, "gate_pmu_hevc", NULL, EXYNOS5430_ENABLE_IP_HEVC0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_hevc, "gate_sysreg_hevc", NULL, EXYNOS5430_ENABLE_IP_HEVC0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_hevc, "gate_hevc", NULL, EXYNOS5430_ENABLE_IP_HEVC0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_hevc_1, "gate_ppmu_hevc_1", NULL, EXYNOS5430_ENABLE_IP_HEVC1, 9, 0, 0),
	CGTE(gate_ppmu_hevc_0, "gate_ppmu_hevc_0", NULL, EXYNOS5430_ENABLE_IP_HEVC1, 8, 0, 0),
	CGTE(gate_bts_hevc_1, "gate_bts_hevc_1", NULL, EXYNOS5430_ENABLE_IP_HEVC1, 5, 0, 0),
	CGTE(gate_bts_hevc_0, "gate_bts_hevc_0", NULL, EXYNOS5430_ENABLE_IP_HEVC1, 4, 0, 0),
	CGTE(gate_ahb2apb_hevcp, "gate_ahb2apb_hevcp", NULL, EXYNOS5430_ENABLE_IP_HEVC1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_hevcx, "gate_xiu_hevcx", NULL, EXYNOS5430_ENABLE_IP_HEVC1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_hevcnp_100, "gate_hevcnp_100", NULL, EXYNOS5430_ENABLE_IP_HEVC1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_hevcnd_400, "gate_hevcnd_400", NULL, EXYNOS5430_ENABLE_IP_HEVC1, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_smmu_hevc_1, "gate_smmu_hevc_1", NULL, EXYNOS5430_ENABLE_IP_HEVC_SECURE_SMMU_HEVC, 1, 0, 0),
	CGTE(gate_smmu_hevc_0, "gate_smmu_hevc_0", NULL, EXYNOS5430_ENABLE_IP_HEVC_SECURE_SMMU_HEVC, 0, 0, 0),

	/* IMEM */
	CGTE(gate_pmu_imem, "gate_pmu_imem", NULL, EXYNOS5430_ENABLE_IP_IMEM0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_imem, "gate_sysreg_imem", NULL, EXYNOS5430_ENABLE_IP_IMEM0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_gic, "gate_gic", NULL, EXYNOS5430_ENABLE_IP_IMEM0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_sssx, "gate_ppmu_sssx", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 19, 0, 0),
	CGTE(gate_bts_slimsss, "gate_bts_slimsss", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 14, 0, 0),
	CGTE(gate_bts_sss_dram, "gate_bts_sss_dram", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 13, 0, 0),
	CGTE(gate_bts_sss_cci, "gate_bts_sss_cci", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 12, 0, 0),
	CGTE(gate_alb_imem, "gate_alb_imem", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axids_pimemx_imem1p, "gate_axids_pimemx_imem1p", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axids_pimemx_imem0p, "gate_axids_pimemx_imem0p", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axids_pimemx_gic, "gate_axids_pimemx_gic", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_xsssx, "gate_axius_xsssx", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncahbm_sss_egl, "gate_asyncahbm_sss_egl", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxis_mif_pimemx, "gate_asyncaxis_mif_pimemx", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi2apb_imem1p, "gate_axi2apb_imem1p", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi2apb_imem0p, "gate_axi2apb_imem0p", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_sssx, "gate_xiu_sssx", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_pimemx, "gate_xiu_pimemx", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_imemnd_266, "gate_imemnd_266", NULL, EXYNOS5430_ENABLE_IP_IMEM1, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_int_mem, "gate_int_mem", NULL, EXYNOS5430_ENABLE_IP_IMEM_SECURE_INT_MEM, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_sss, "gate_sss", NULL, EXYNOS5430_ENABLE_IP_IMEM_SECURE_SSS, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_slimsss, "gate_slimsss", NULL, EXYNOS5430_ENABLE_IP_IMEM_SECURE_SLIMSSS, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_rtic, "gate_rtic", NULL, EXYNOS5430_ENABLE_IP_IMEM_SECURE_RTIC, 0, 0, 0),

	CGTE(gate_smmu_sss_dram, "gate_smmu_sss_dram", NULL, EXYNOS5430_ENABLE_IP_IMEM_SECURE_SMMU_SSS, 1, 0, 0),
	CGTE(gate_smmu_sss_cci, "gate_smmu_sss_cci", NULL, EXYNOS5430_ENABLE_IP_IMEM_SECURE_SMMU_SSS, 0, 0, 0),

	CGTE(gate_smmu_slimsss, "gate_smmu_slimsss", NULL, EXYNOS5430_ENABLE_IP_IMEM_SECURE_SMMU_SLIMSSS, 0, 0, 0),

	CGTE(gate_smmu_rtic, "gate_smmu_rtic", NULL, EXYNOS5430_ENABLE_IP_IMEM_SECURE_SMMU_RTIC, 0, 0, 0),

	/* MFC0 */
	CGTE(gate_pmu_mfc0, "gate_pmu_mfc0", NULL, EXYNOS5430_ENABLE_IP_MFC00, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_mfc0, "gate_sysreg_mfc0", NULL, EXYNOS5430_ENABLE_IP_MFC00, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_mfc0, "gate_mfc0", NULL, EXYNOS5430_ENABLE_IP_MFC00, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_mfc0_1, "gate_ppmu_mfc0_1", NULL, EXYNOS5430_ENABLE_IP_MFC01, 9, 0, 0),
	CGTE(gate_ppmu_mfc0_0, "gate_ppmu_mfc0_0", NULL, EXYNOS5430_ENABLE_IP_MFC01, 8, 0, 0),

	CGTE(gate_bts_mfc0_1, "gate_bts_mfc0_1", NULL, EXYNOS5430_ENABLE_IP_MFC01, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_mfc0_0, "gate_bts_mfc0_0", NULL, EXYNOS5430_ENABLE_IP_MFC01, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_mfc0p, "gate_ahb2apb_mfc0p", NULL, EXYNOS5430_ENABLE_IP_MFC01, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_mfc0x, "gate_xiu_mfc0x", NULL, EXYNOS5430_ENABLE_IP_MFC01, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_mfc0np_83, "gate_mfc0np_83", NULL, EXYNOS5430_ENABLE_IP_MFC01, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_mfc0nd_333, "gate_mfc0nd_333", NULL, EXYNOS5430_ENABLE_IP_MFC01, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_smmu_mfc0_1, "gate_smmu_mfc0_1", NULL, EXYNOS5430_ENABLE_IP_MFC0_SECURE_SMMU_MFC, 1, 0, 0),
	CGTE(gate_smmu_mfc0_0, "gate_smmu_mfc0_0", NULL, EXYNOS5430_ENABLE_IP_MFC0_SECURE_SMMU_MFC, 0, 0, 0),

	/* MSCL */
	CGTE(gate_pmu_mscl, "gate_pmu_mscl", NULL, EXYNOS5430_ENABLE_IP_MSCL0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_mscl, "gate_sysreg_mscl", NULL, EXYNOS5430_ENABLE_IP_MSCL0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_jpeg, "gate_jpeg", NULL, EXYNOS5430_ENABLE_IP_MSCL0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_m2mscaler1, "gate_m2mscaler1", NULL, EXYNOS5430_ENABLE_IP_MSCL0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_m2mscaler0, "gate_m2mscaler0", NULL, EXYNOS5430_ENABLE_IP_MSCL0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_jpeg, "gate_ppmu_jpeg", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 9, 0, 0),
	CGTE(gate_ppmu_m2mscaler1, "gate_ppmu_m2mscaler1", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 8, 0, 0),
	CGTE(gate_ppmu_m2mscaler0, "gate_ppmu_m2mscaler0", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 7, 0, 0),
	CGTE(gate_bts_jpeg, "gate_bts_jpeg", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 6, 0, 0),
	CGTE(gate_bts_m2mscaler1, "gate_bts_m2mscaler1", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 5, 0, 0),
	CGTE(gate_bts_m2mscaler0, "gate_bts_m2mscaler0", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 4, 0, 0),
	CGTE(gate_ahb2apb_mscl0p, "gate_ahb2apb_mscl0p", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_msclx, "gate_xiu_msclx", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_msclnp_100, "gate_msclnp_100", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_msclnd_400, "gate_msclnd_400", NULL, EXYNOS5430_ENABLE_IP_MSCL1, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_smmu_m2mscaler0, "gate_smmu_m2mscaler0", NULL, EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER0, 0, 0, 0),

	CGTE(gate_smmu_m2mscaler1, "gate_smmu_m2mscaler1", NULL, EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER1, 0, 0, 0),

	CGTE(gate_smmu_jpeg, "gate_smmu_jpeg", NULL, EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_JPEG, 0, 0, 0),

	/* PERIC */
	CGTE(gate_sci, "gate_sci", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 31, 0, 0),
	CGTE(gate_slimbus, "gate_slimbus", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 28, 0, 0),
	CGTE(gate_pwm, "gate_pwm", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 27, 0, 0),
	CGTE(gate_spdif, "gate_spdif", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 26, 0, 0),
	CGTE(gate_pcm1, "gate_pcm1", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 25, 0, 0),
	CGTE(gate_i2s1, "gate_i2s1", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 24, 0, 0),
	CGTE(gate_spi4, "gate_spi4", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 9, 0, 0),
	CGTE(gate_spi3, "gate_spi3", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 8, 0, 0),
	CGTE(gate_spi2, "gate_spi2", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 23, 0, 0),
	CGTE(gate_spi1, "gate_spi1", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 22, 0, 0),
	CGTE(gate_spi0, "gate_spi0", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 21, 0, 0),
	CGTE(gate_adcif, "gate_adcif", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 20, 0, 0),
	CGTE(gate_gpio_touch, "gate_gpio_touch", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_gpio_nfc, "gate_gpio_nfc", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_gpio_peric, "gate_gpio_peric", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pmu_peric, "gate_pmu_peric", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_peric, "gate_sysreg_peric", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 15, CLK_IGNORE_UNUSED, 0),
	GATE_A(gate_uart2, "gate_uart2", NULL, (unsigned long)EXYNOS5430_ENABLE_IP_PERIC0, 14, 0, 0, "console-sclk2"),
	GATE_A(gate_uart1, "gate_uart1", NULL, (unsigned long)EXYNOS5430_ENABLE_IP_PERIC0, 13, 0, 0, "console-sclk1"),
	GATE_A(gate_uart0, "gate_uart0", NULL, (unsigned long)EXYNOS5430_ENABLE_IP_PERIC0, 12, 0, 0, "console-sclk0"),
	CGTE(gate_hsi2c11, "gate_hsi2c11", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 7, 0, 0),
	CGTE(gate_hsi2c10, "gate_hsi2c10", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 6, 0, 0),
	CGTE(gate_hsi2c9, "gate_hsi2c9", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 5, 0, 0),
	CGTE(gate_hsi2c8, "gate_hsi2c8", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 4, 0, 0),
	CGTE(gate_hsi2c7, "gate_hsi2c7", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 3, 0, 0),
	CGTE(gate_hsi2c6, "gate_hsi2c6", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 2, 0, 0),
	CGTE(gate_hsi2c5, "gate_hsi2c5", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 1, 0, 0),
	CGTE(gate_hsi2c4, "gate_hsi2c4", NULL, EXYNOS5430_ENABLE_IP_PERIC2, 0, 0, 0),
	CGTE(gate_hsi2c3, "gate_hsi2c3", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 11, 0, 0),
	CGTE(gate_hsi2c2, "gate_hsi2c2", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 10, 0, 0),
	CGTE(gate_hsi2c1, "gate_hsi2c1", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 9, 0, 0),
	CGTE(gate_hsi2c0, "gate_hsi2c0", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 8, 0, 0),
	CGTE(gate_i2c7, "gate_i2c7", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 7, 0, 0),
	CGTE(gate_i2c6, "gate_i2c6", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 6, 0, 0),
	CGTE(gate_i2c5, "gate_i2c5", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 5, 0, 0),
	CGTE(gate_i2c4, "gate_i2c4", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 4, 0, 0),
	CGTE(gate_i2c3, "gate_i2c3", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 3, 0, 0),
	CGTE(gate_i2c2, "gate_i2c2", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 2, 0, 0),
	CGTE(gate_i2c1, "gate_i2c1", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 1, 0, 0),
	CGTE(gate_i2c0, "gate_i2c0", NULL, EXYNOS5430_ENABLE_IP_PERIC0, 0, 0, 0),

	CGTE(gate_ahb2apb_peric2p, "gate_ahb2apb_peric2p", NULL, EXYNOS5430_ENABLE_IP_PERIC1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_peric1p, "gate_ahb2apb_peric1p", NULL, EXYNOS5430_ENABLE_IP_PERIC1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_peric0p, "gate_ahb2apb_peric0p", NULL, EXYNOS5430_ENABLE_IP_PERIC1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pericnp_66, "gate_pericnp_66", NULL, EXYNOS5430_ENABLE_IP_PERIC1, 0, CLK_IGNORE_UNUSED, 0),

	/* PERIS */
	CGTE(gate_asv_tb, "gate_asv_tb", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 23, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_hpm_apbif, "gate_hpm_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_efuse_writer1, "gate_efuse_writer1", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_efuse_writer1_apbif, "gate_efuse_writer1_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_efuse_writer0, "gate_efuse_writer0", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_efuse_writer0_apbif, "gate_efuse_writer0_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tmu1, "gate_tmu1", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tmu1_apbif, "gate_tmu1_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tmu0, "gate_tmu0", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tmu0_apbif, "gate_tmu0_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pmu_peris, "gate_pmu_peris", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_peris, "gate_sysreg_peris", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cmu_top_apbif, "gate_cmu_top_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_wdt_kfc, "gate_wdt_kfc", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_wdt_egl, "gate_wdt_egl", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_mct, "gate_mct", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_hdmi_cec, "gate_hdmi_cec", NULL, EXYNOS5430_ENABLE_IP_PERIS0, 2, 0, 0),

	CGTE(gate_ahb2apb_peris1p, "gate_ahb2apb_peris1p", NULL, EXYNOS5430_ENABLE_IP_PERIS1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_peris0p, "gate_ahb2apb_peris0p", NULL, EXYNOS5430_ENABLE_IP_PERIS1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_perisnp_66, "gate_perisnp_66", NULL, EXYNOS5430_ENABLE_IP_PERIS1, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_tzpc12, "gate_tzpc12", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc11, "gate_tzpc11", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc10, "gate_tzpc10", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc9, "gate_tzpc9", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc8, "gate_tzpc8", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc7, "gate_tzpc7", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc6, "gate_tzpc6", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc5, "gate_tzpc5", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc4, "gate_tzpc4", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc3, "gate_tzpc3", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc2, "gate_tzpc2", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc1, "gate_tzpc1", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_tzpc0, "gate_tzpc0", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TZPC, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_seckey, "gate_seckey", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_SECKEY, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_seckey_apbif, "gate_seckey_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_SECKEY, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_chipid, "gate_chipid", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_CHIPID, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_chipid_apbif, "gate_chipid_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_CHIPID, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_toprtc, "gate_toprtc", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_TOPRTC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_rtc, "gate_rtc", NULL, EXYNOS5430_ENABLE_IP_MIF_SECURE_RTC, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_custom_efuse, "gate_custom_efuse", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_CUSTOM_EFUSE, 1, 0, 0),
	CGTE(gate_custom_efuse_apbif, "gate_custom_efuse_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_CUSTOM_EFUSE, 0, 0, 0),

	CGTE(gate_antirbk_cnt, "gate_antirbk_cnt", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_ANTIRBK_CNT, 1, 0, 0),
	CGTE(gate_antirbk_cnt_apbif, "gate_antirbk_cnt_apbif", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_ANTIRBK_CNT, 0, 0, 0),

	CGTE(gate_otp_con, "gate_otp_con", NULL, EXYNOS5430_ENABLE_IP_PERIS_SECURE_OTP_CON, 1, 0, 0),

	/* CAM0 */
	CGTE(gate_pmu_cam0, "gate_pmu_cam0", NULL, EXYNOS5430_ENABLE_IP_CAM00, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_cam0, "gate_sysreg_cam0", NULL, EXYNOS5430_ENABLE_IP_CAM00, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cmu_cam0_local, "gate_cmu_cam0_local", NULL, EXYNOS5430_ENABLE_IP_CAM00, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_csis1, "gate_csis1", NULL, EXYNOS5430_ENABLE_IP_CAM00, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_csis0, "gate_csis0", NULL, EXYNOS5430_ENABLE_IP_CAM00, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_3aa1, "gate_3aa1", NULL, EXYNOS5430_ENABLE_IP_CAM00, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_3aa0, "gate_3aa0", NULL, EXYNOS5430_ENABLE_IP_CAM00, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_d, "gate_lite_d", NULL, EXYNOS5430_ENABLE_IP_CAM00, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_b, "gate_lite_b", NULL, EXYNOS5430_ENABLE_IP_CAM00, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_a, "gate_lite_a", NULL, EXYNOS5430_ENABLE_IP_CAM00, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ahbsyncdn, "gate_ahbsyncdn", NULL, EXYNOS5430_ENABLE_IP_CAM01, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_lite_d, "gate_axius_lite_d", NULL, EXYNOS5430_ENABLE_IP_CAM01, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_lite_b, "gate_axius_lite_b", NULL, EXYNOS5430_ENABLE_IP_CAM01, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_lite_a, "gate_axius_lite_a", NULL, EXYNOS5430_ENABLE_IP_CAM01, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncapb_3aa1, "gate_asyncapb_3aa1", NULL, EXYNOS5430_ENABLE_IP_CAM01, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncapb_3aa0, "gate_asyncapb_3aa0", NULL, EXYNOS5430_ENABLE_IP_CAM01, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncapb_lite_d, "gate_asyncapb_lite_d", NULL, EXYNOS5430_ENABLE_IP_CAM01, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncapb_lite_b, "gate_asyncapb_lite_b", NULL, EXYNOS5430_ENABLE_IP_CAM01, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncapb_lite_a, "gate_asyncapb_lite_a", NULL, EXYNOS5430_ENABLE_IP_CAM01, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_cam1, "gate_asyncaxi_cam1", NULL, EXYNOS5430_ENABLE_IP_CAM01, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_isp0p, "gate_asyncaxi_isp0p", NULL, EXYNOS5430_ENABLE_IP_CAM01, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_3aa1, "gate_asyncaxi_3aa1", NULL, EXYNOS5430_ENABLE_IP_CAM01, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_3aa0, "gate_asyncaxi_3aa0", NULL, EXYNOS5430_ENABLE_IP_CAM01, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_lite_d, "gate_asyncaxi_lite_d", NULL, EXYNOS5430_ENABLE_IP_CAM01, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_lite_b, "gate_asyncaxi_lite_b", NULL, EXYNOS5430_ENABLE_IP_CAM01, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_lite_a, "gate_asyncaxi_lite_a", NULL, EXYNOS5430_ENABLE_IP_CAM01, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_ispsfrp, "gate_ahb2apb_ispsfrp", NULL, EXYNOS5430_ENABLE_IP_CAM01, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi2apb_isp0p, "gate_axi2apb_isp0p", NULL, EXYNOS5430_ENABLE_IP_CAM01, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi2ahb_isp0p, "gate_axi2ahb_isp0p", NULL, EXYNOS5430_ENABLE_IP_CAM01, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_is0x, "gate_xiu_is0x", NULL, EXYNOS5430_ENABLE_IP_CAM01, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_is0ex, "gate_xiu_is0ex", NULL, EXYNOS5430_ENABLE_IP_CAM01, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cam0np_276, "gate_cam0np_276", NULL, EXYNOS5430_ENABLE_IP_CAM01, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cam0nd_400, "gate_cam0nd_400", NULL, EXYNOS5430_ENABLE_IP_CAM01, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_isp0ex, "gate_ppmu_isp0ex", NULL, EXYNOS5430_ENABLE_IP_CAM02, 10, 0, 0),
	CGTE(gate_smmu_3aa1, "gate_smmu_3aa1", NULL, EXYNOS5430_ENABLE_IP_CAM02, 9, 0, 0),
	CGTE(gate_smmu_3aa0, "gate_smmu_3aa0", NULL, EXYNOS5430_ENABLE_IP_CAM02, 8, 0, 0),
	CGTE(gate_smmu_lite_d, "gate_smmu_lite_d", NULL, EXYNOS5430_ENABLE_IP_CAM02, 7, 0, 0),
	CGTE(gate_smmu_lite_b, "gate_smmu_lite_b", NULL, EXYNOS5430_ENABLE_IP_CAM02, 6, 0, 0),
	CGTE(gate_smmu_lite_a, "gate_smmu_lite_a", NULL, EXYNOS5430_ENABLE_IP_CAM02, 5, 0, 0),
	CGTE(gate_bts_3aa1, "gate_bts_3aa1", NULL, EXYNOS5430_ENABLE_IP_CAM02, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_3aa0, "gate_bts_3aa0", NULL, EXYNOS5430_ENABLE_IP_CAM02, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_lite_d, "gate_bts_lite_d", NULL, EXYNOS5430_ENABLE_IP_CAM02, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_lite_b, "gate_bts_lite_b", NULL, EXYNOS5430_ENABLE_IP_CAM02, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_lite_a, "gate_bts_lite_a", NULL, EXYNOS5430_ENABLE_IP_CAM02, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_lite_freecnt, "gate_lite_freecnt", NULL, EXYNOS5430_ENABLE_IP_CAM03, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_3aa1, "gate_pixelasync_3aa1", NULL, EXYNOS5430_ENABLE_IP_CAM03, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_3aa0, "gate_pixelasync_3aa0", NULL, EXYNOS5430_ENABLE_IP_CAM03, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_lite_c, "gate_pixelasync_lite_c", NULL, EXYNOS5430_ENABLE_IP_CAM03, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_lite_c_init, "gate_pixelasync_lite_c_init", NULL, EXYNOS5430_ENABLE_IP_CAM03, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_csis1_local, "gate_csis1_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_csis0_local, "gate_csis0_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_3aa1_local, "gate_3aa1_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_3aa0_local, "gate_3aa0_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_d_local, "gate_lite_d_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_b_local, "gate_lite_b_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_a_local, "gate_lite_a_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_lite_freecnt_local, "gate_lite_freecnt_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_3aa1_local, "gate_pixelasync_3aa1_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_3aa0_local, "gate_pixelasync_3aa0_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_lite_c_local, "gate_pixelasync_lite_c_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_lite_c_init_local, "gate_pixelasync_lite_c_init_local", NULL, EXYNOS5430_ENABLE_IP_CAM0_LOCAL1, 0, CLK_IGNORE_UNUSED, 0),

	/* CAM1 */
	CGTE(gate_rxbyteclkhs0_s2b, "gate_rxbyteclkhs0_s2b", NULL, EXYNOS5430_ENABLE_IP_CAM10, 23, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_c_freecnt, "gate_lite_c_freecnt", NULL, EXYNOS5430_ENABLE_IP_CAM10, 22, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasyncm_fd, "gate_pixelasyncm_fd", NULL, EXYNOS5430_ENABLE_IP_CAM10, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasyncs_lite_c, "gate_pixelasyncs_lite_c", NULL, EXYNOS5430_ENABLE_IP_CAM10, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cssys, "gate_cssys", NULL, EXYNOS5430_ENABLE_IP_CAM10, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pmu_cam1, "gate_pmu_cam1", NULL, EXYNOS5430_ENABLE_IP_CAM10, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_cam1, "gate_sysreg_cam1", NULL, EXYNOS5430_ENABLE_IP_CAM10, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cmu_cam1_local, "gate_cmu_cam1_local", NULL, EXYNOS5430_ENABLE_IP_CAM10, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_mtcadc, "gate_isp_mtcadc", NULL, EXYNOS5430_ENABLE_IP_CAM10, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_wdt, "gate_isp_wdt", NULL, EXYNOS5430_ENABLE_IP_CAM10, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_pwm, "gate_isp_pwm", NULL, EXYNOS5430_ENABLE_IP_CAM10, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_uart, "gate_isp_uart", NULL, EXYNOS5430_ENABLE_IP_CAM10, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_mcuctl, "gate_isp_mcuctl", NULL, EXYNOS5430_ENABLE_IP_CAM10, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_spi1, "gate_isp_spi1", NULL, EXYNOS5430_ENABLE_IP_CAM10, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_spi0, "gate_isp_spi0", NULL, EXYNOS5430_ENABLE_IP_CAM10, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_i2c2, "gate_isp_i2c2", NULL, EXYNOS5430_ENABLE_IP_CAM10, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_i2c1, "gate_isp_i2c1", NULL, EXYNOS5430_ENABLE_IP_CAM10, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_i2c0, "gate_isp_i2c0", NULL, EXYNOS5430_ENABLE_IP_CAM10, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_mpwm, "gate_isp_mpwm", NULL, EXYNOS5430_ENABLE_IP_CAM10, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_gic, "gate_isp_gic", NULL, EXYNOS5430_ENABLE_IP_CAM10, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_fd, "gate_fd", NULL, EXYNOS5430_ENABLE_IP_CAM10, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_c, "gate_lite_c", NULL, EXYNOS5430_ENABLE_IP_CAM10, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_csis2, "gate_csis2", NULL, EXYNOS5430_ENABLE_IP_CAM10, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_ca5, "gate_isp_ca5", NULL, EXYNOS5430_ENABLE_IP_CAM10, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_asyncapb_fd, "gate_asyncapb_fd", NULL, EXYNOS5430_ENABLE_IP_CAM11, 21, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncapb_lite_c, "gate_asyncapb_lite_c", NULL, EXYNOS5430_ENABLE_IP_CAM11, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncahbs_sfrisp2h2, "gate_asyncahbs_sfrisp2h2", NULL, EXYNOS5430_ENABLE_IP_CAM11, 19, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncahbs_sfrisp2h1, "gate_asyncahbs_sfrisp2h1", NULL, EXYNOS5430_ENABLE_IP_CAM11, 18, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_ca5, "gate_asyncaxi_ca5", NULL, EXYNOS5430_ENABLE_IP_CAM11, 17, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_ispx2, "gate_asyncaxi_ispx2", NULL, EXYNOS5430_ENABLE_IP_CAM11, 16, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_ispx1, "gate_asyncaxi_ispx1", NULL, EXYNOS5430_ENABLE_IP_CAM11, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_ispx0, "gate_asyncaxi_ispx0", NULL, EXYNOS5430_ENABLE_IP_CAM11, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_ispex, "gate_asyncaxi_ispex", NULL, EXYNOS5430_ENABLE_IP_CAM11, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_isp3p, "gate_asyncaxi_isp3p", NULL, EXYNOS5430_ENABLE_IP_CAM11, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_fd, "gate_asyncaxi_fd", NULL, EXYNOS5430_ENABLE_IP_CAM11, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_lite_c, "gate_asyncaxi_lite_c", NULL, EXYNOS5430_ENABLE_IP_CAM11, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_isp5p, "gate_ahb2apb_isp5p", NULL, EXYNOS5430_ENABLE_IP_CAM11, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_isp3p, "gate_ahb2apb_isp3p", NULL, EXYNOS5430_ENABLE_IP_CAM11, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi2apb_isp3p, "gate_axi2apb_isp3p", NULL, EXYNOS5430_ENABLE_IP_CAM11, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb_sfrisp2h, "gate_ahb_sfrisp2h", NULL, EXYNOS5430_ENABLE_IP_CAM11, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi_isp_hx, "gate_axi_isp_hx", NULL, EXYNOS5430_ENABLE_IP_CAM11, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi_isp_cx, "gate_axi_isp_cx", NULL, EXYNOS5430_ENABLE_IP_CAM11, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_ispx, "gate_xiu_ispx", NULL, EXYNOS5430_ENABLE_IP_CAM11, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_ispex, "gate_xiu_ispex", NULL, EXYNOS5430_ENABLE_IP_CAM11, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cam1np_333, "gate_cam1np_333", NULL, EXYNOS5430_ENABLE_IP_CAM11, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cam1nd_400, "gate_cam1nd_400", NULL, EXYNOS5430_ENABLE_IP_CAM11, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_ispex, "gate_ppmu_ispex", NULL, EXYNOS5430_ENABLE_IP_CAM12, 11, 0, 0),
	CGTE(gate_smmu_isp3p, "gate_smmu_isp3p", NULL, EXYNOS5430_ENABLE_IP_CAM12, 10, 0, 0),
	CGTE(gate_smmu_fd, "gate_smmu_fd", NULL, EXYNOS5430_ENABLE_IP_CAM12, 9, 0, 0),
	CGTE(gate_smmu_lite_c, "gate_smmu_lite_c", NULL, EXYNOS5430_ENABLE_IP_CAM12, 8, 0, 0),
	CGTE(gate_bts_isp3p, "gate_bts_isp3p", NULL, EXYNOS5430_ENABLE_IP_CAM12, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_fd, "gate_bts_fd", NULL, EXYNOS5430_ENABLE_IP_CAM12, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_bts_lite_c, "gate_bts_lite_c", NULL, EXYNOS5430_ENABLE_IP_CAM12, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahbdn_sfrisp2h, "gate_ahbdn_sfrisp2h", NULL, EXYNOS5430_ENABLE_IP_CAM12, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahbdn_isp5p, "gate_ahbdn_isp5p", NULL, EXYNOS5430_ENABLE_IP_CAM12, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_isp3p, "gate_axius_isp3p", NULL, EXYNOS5430_ENABLE_IP_CAM12, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_fd, "gate_axius_fd", NULL, EXYNOS5430_ENABLE_IP_CAM12, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_lite_c, "gate_axius_lite_c", NULL, EXYNOS5430_ENABLE_IP_CAM12, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_isp_mtcadc_local, "gate_isp_mtcadc_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_wdt_local, "gate_isp_wdt_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_pwm_local, "gate_isp_pwm_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_uart_local, "gate_isp_uart_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_mcuctl_local, "gate_isp_mcuctl_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_spi1_local, "gate_isp_spi1_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_spi0_local, "gate_isp_spi0_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_i2c2_local, "gate_isp_i2c2_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_i2c1_local, "gate_isp_i2c1_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_i2c0_local, "gate_isp_i2c0_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_mpwm_local, "gate_isp_mpwm_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp_gic_local, "gate_isp_gic_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_fd_local, "gate_fd_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_lite_c_local, "gate_lite_c_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_csis2_local, "gate_csis2_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_lite_c_freecnt_local, "gate_lite_c_freecnt_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasyncm_fd_local, "gate_pixelasyncm_fd_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasyncs_lite_c_local, "gate_pixelasyncs_lite_c_local", NULL, EXYNOS5430_ENABLE_IP_CAM1_LOCAL1, 0, CLK_IGNORE_UNUSED, 0),

	/* ISP */
	CGTE(gate_isp_d_glue, "gate_isp_d_glue", NULL, EXYNOS5430_ENABLE_IP_ISP0, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pmu_isp, "gate_pmu_isp", NULL, EXYNOS5430_ENABLE_IP_ISP0, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_sysreg_isp, "gate_sysreg_isp", NULL, EXYNOS5430_ENABLE_IP_ISP0, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_cmu_isp_local, "gate_cmu_isp_local", NULL, EXYNOS5430_ENABLE_IP_ISP0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_scalerp, "gate_scalerp", NULL, EXYNOS5430_ENABLE_IP_ISP0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_3dnr, "gate_3dnr", NULL, EXYNOS5430_ENABLE_IP_ISP0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_dis, "gate_dis", NULL, EXYNOS5430_ENABLE_IP_ISP0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_scalerc, "gate_scalerc", NULL, EXYNOS5430_ENABLE_IP_ISP0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_drc, "gate_drc", NULL, EXYNOS5430_ENABLE_IP_ISP0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_isp, "gate_isp", NULL, EXYNOS5430_ENABLE_IP_ISP0, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_axius_scalerp, "gate_axius_scalerp", NULL, EXYNOS5430_ENABLE_IP_ISP1, 15, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_scalerc, "gate_axius_scalerc", NULL, EXYNOS5430_ENABLE_IP_ISP1, 14, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axius_drc, "gate_axius_drc", NULL, EXYNOS5430_ENABLE_IP_ISP1, 13, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncahbm_isp2p, "gate_asyncahbm_isp2p", NULL, EXYNOS5430_ENABLE_IP_ISP1, 12, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncahbm_isp1p, "gate_asyncahbm_isp1p", NULL, EXYNOS5430_ENABLE_IP_ISP1, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_dis1, "gate_asyncaxi_dis1", NULL, EXYNOS5430_ENABLE_IP_ISP1, 10, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxi_dis0, "gate_asyncaxi_dis0", NULL, EXYNOS5430_ENABLE_IP_ISP1, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxim_isp2p, "gate_asyncaxim_isp2p", NULL, EXYNOS5430_ENABLE_IP_ISP1, 8, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_asyncaxim_isp1p, "gate_asyncaxim_isp1p", NULL, EXYNOS5430_ENABLE_IP_ISP1, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_isp2p, "gate_ahb2apb_isp2p", NULL, EXYNOS5430_ENABLE_IP_ISP1, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ahb2apb_isp1p, "gate_ahb2apb_isp1p", NULL, EXYNOS5430_ENABLE_IP_ISP1, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi2apb_isp2p, "gate_axi2apb_isp2p", NULL, EXYNOS5430_ENABLE_IP_ISP1, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_axi2apb_isp1p, "gate_axi2apb_isp1p", NULL, EXYNOS5430_ENABLE_IP_ISP1, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_ispex1, "gate_xiu_ispex1", NULL, EXYNOS5430_ENABLE_IP_ISP1, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_xiu_ispex0, "gate_xiu_ispex0", NULL, EXYNOS5430_ENABLE_IP_ISP1, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_ispnd_400, "gate_ispnd_400", NULL, EXYNOS5430_ENABLE_IP_ISP1, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_ppmu_ispex0, "gate_ppmu_ispex0", NULL, EXYNOS5430_ENABLE_IP_ISP2, 14, 0, 0),
	CGTE(gate_smmu_scalerp, "gate_smmu_scalerp", NULL, EXYNOS5430_ENABLE_IP_ISP2, 13, 0, 0),
	CGTE(gate_smmu_3dnr, "gate_smmu_3dnr", NULL, EXYNOS5430_ENABLE_IP_ISP2, 12, 0, 0),
	CGTE(gate_smmu_dis1, "gate_smmu_dis1", NULL, EXYNOS5430_ENABLE_IP_ISP2, 11, 0, 0),
	CGTE(gate_smmu_dis0, "gate_smmu_dis0", NULL, EXYNOS5430_ENABLE_IP_ISP2, 10, 0, 0),
	CGTE(gate_smmu_scalerc, "gate_smmu_scalerc", NULL, EXYNOS5430_ENABLE_IP_ISP2, 9, 0, 0),
	CGTE(gate_smmu_drc, "gate_smmu_drc", NULL, EXYNOS5430_ENABLE_IP_ISP2, 8, 0, 0),
	CGTE(gate_smmu_isp, "gate_smmu_isp", NULL, EXYNOS5430_ENABLE_IP_ISP2, 7, 0, 0),
	CGTE(gate_bts_scalerp, "gate_bts_scalerp", NULL, EXYNOS5430_ENABLE_IP_ISP2, 6, 0, 0),
	CGTE(gate_bts_3dnr, "gate_bts_3dnr", NULL, EXYNOS5430_ENABLE_IP_ISP2, 5, 0, 0),
	CGTE(gate_bts_dis1, "gate_bts_dis1", NULL, EXYNOS5430_ENABLE_IP_ISP2, 4, 0, 0),
	CGTE(gate_bts_dis0, "gate_bts_dis0", NULL, EXYNOS5430_ENABLE_IP_ISP2, 3, 0, 0),
	CGTE(gate_bts_scalerc, "gate_bts_scalerc", NULL, EXYNOS5430_ENABLE_IP_ISP2, 2, 0, 0),
	CGTE(gate_bts_drc, "gate_bts_drc", NULL, EXYNOS5430_ENABLE_IP_ISP2, 1, 0, 0),
	CGTE(gate_bts_isp, "gate_bts_isp", NULL, EXYNOS5430_ENABLE_IP_ISP2, 0, 0, 0),

	CGTE(gate_pixelasync_dis, "gate_pixelasync_dis", NULL, EXYNOS5430_ENABLE_IP_ISP3, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasyncs_scalerp, "gate_pixelasyncs_scalerp", NULL, EXYNOS5430_ENABLE_IP_ISP3, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasyncm_ispd, "gate_pixelasyncm_ispd", NULL, EXYNOS5430_ENABLE_IP_ISP3, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_pixelasync_ispc, "gate_pixelasync_ispc", NULL, EXYNOS5430_ENABLE_IP_ISP3, 0, CLK_IGNORE_UNUSED, 0),

	CGTE(gate_isp_d_glue_local, "gate_isp_d_glue_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL0, 6, 0, 0),
	CGTE(gate_scalerp_local, "gate_scalerp_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL0, 5, 0, 0),
	CGTE(gate_3dnr_local, "gate_3dnr_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL0, 4, 0, 0),
	CGTE(gate_dis_local, "gate_dis_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL0, 3, 0, 0),
	CGTE(gate_scalerc_local, "gate_scalerc_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL0, 2, 0, 0),
	CGTE(gate_drc_local, "gate_drc_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL0, 1, 0, 0),
	CGTE(gate_isp_local, "gate_isp_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL0, 0, 0, 0),

	CGTE(gate_pixelasync_dis_local, "gate_pixelasync_dis_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL1, 3, 0, 0),
	CGTE(gate_pixelasyncs_scalerp_local, "gate_pixelasyncs_scalerp_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL1, 2, 0, 0),
	CGTE(gate_pixelasyncm_ispd_local, "gate_pixelasyncm_ispd_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL1, 1, 0, 0),
	CGTE(gate_pixelasync_ispc_local, "gate_pixelasync_ispc_local", NULL, EXYNOS5430_ENABLE_IP_ISP_LOCAL1, 0, 0, 0),

	/* MUX gate */
	CGTE(mgate_sclk_mmc2_b, "mgate_sclk_mmc2_b", NULL, EXYNOS5430_SRC_ENABLE_TOP_FSYS0, 28, CLK_IGNORE_UNUSED, 0),
	CGTE(mgate_sclk_mmc1_b, "mgate_sclk_mmc1_b", NULL, EXYNOS5430_SRC_ENABLE_TOP_FSYS0, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(mgate_sclk_mmc0_d, "mgate_sclk_mmc0_d", NULL, EXYNOS5430_SRC_ENABLE_TOP_FSYS0, 12, CLK_IGNORE_UNUSED, 0),

	/* Additional/Modified clocks for exynos5433 series */
	/* TOP */
	CGTE(aclk_imem_sssx_266, "aclk_imem_sssx_266", "dout_aclk_imem_sssx_266", EXYNOS5430_ENABLE_ACLK_TOP, 29, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bus0_400, "aclk_bus0_400", "dout_aclk_bus0_400", EXYNOS5430_ENABLE_ACLK_TOP, 26, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bus1_400, "aclk_bus1_400", "dout_aclk_bus1_400", EXYNOS5430_ENABLE_ACLK_MIF3, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_g3d_400, "aclk_g3d_400", "dout_aclk_g3d_400", EXYNOS5430_ENABLE_ACLK_TOP, 30, CLK_IGNORE_UNUSED, 0),/* TOP */
	CGTE(sclk_usbhost30_fsys, "sclk_usbhost30_fsys", "dout_usbhost30", EXYNOS5430_ENABLE_SCLK_TOP_FSYS, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_pcie_100_fsys, "sclk_pcie_100_fsys", "dout_sclk_pcie_100", EXYNOS5430_ENABLE_SCLK_TOP_FSYS, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spi3_peric, "sclk_spi3_peric", "dout_sclk_spi3_b", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 11, CLK_IGNORE_UNUSED, 0),
	CGTE(sclk_spi4_peric, "sclk_spi4_peric", "dout_sclk_spi4_b", EXYNOS5430_ENABLE_SCLK_TOP_PERIC, 12, CLK_IGNORE_UNUSED, 0),
	/* ATLAS */
	/* MIF */
	CGTE(aclk_mifnd_266, "aclk_mifnd_266", "dout_aclk_mif_266", EXYNOS5430_ENABLE_ACLK_MIF2, 20, CLK_IGNORE_UNUSED, 0),
	CGTE(gate_decontv_vclk, "gate_decontv_vclk", NULL, EXYNOS5430_ENABLE_IP_MIF3, 10, 0, 0),
	CGTE(gate_dsim1_clk, "gate_dsim1_clk", NULL, EXYNOS5430_ENABLE_IP_MIF3, 11, 0, 0),
	/* Gate for MFC block */
	CGTE(aclk_mfc_400, "aclk_mfc_400", "dout_aclk_mfc_400", EXYNOS5430_ENABLE_ACLK_TOP, 3, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_mfc, "aclk_mfc", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_mfcnd_400, "aclk_mfcnd", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_xiu_mfcx, "aclk_xiu_mfcx", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_mfc_0, "aclk_bts_mfc_0", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_mfc_1, "aclk_bts_mfc_1", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_mfc_0, "aclk_smmu_mfc_0", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0_SECURE_SMMU_MFC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_smmu_mfc_1, "aclk_smmu_mfc_1", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0_SECURE_SMMU_MFC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_mfc_0, "aclk_ppmu_mfc_0", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0, 9, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ppmu_mfc_1, "aclk_ppmu_mfc_1", "mout_aclk_mfc_400_user", EXYNOS5430_ENABLE_ACLK_MFC0, 10, CLK_IGNORE_UNUSED, 0),

	CGTE(aclk_mfcnp_83, "aclk_mfcnp_83", "dout_pclk_mfc", EXYNOS5430_ENABLE_ACLK_MFC0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_mfcp, "aclk_ahb2apb_mfcp", "dout_pclk_mfc", EXYNOS5430_ENABLE_ACLK_MFC0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_mfc, "pclk_mfc", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_mfc, "pclk_sysreg_mfc", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_mfc, "pclk_pmu_mfc", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_mfc_0, "pclk_bts_mfc_0", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_mfc_1, "pclk_bts_mfc_1", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_mfc_0, "pclk_smmu_mfc_0", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0_SECURE_SMMU_MFC, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_smmu_mfc_1, "pclk_smmu_mfc_1", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0_SECURE_SMMU_MFC, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_mfc_0, "pclk_ppmu_mfc_0", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_ppmu_mfc_1, "pclk_ppmu_mfc_1", "dout_pclk_mfc", EXYNOS5430_ENABLE_PCLK_MFC0, 8, CLK_IGNORE_UNUSED, 0),

	/* Gate for G3D block */
	CGTE(sclk_hpm_g3d, "sclk_hpm_g3d", "dout_sclk_hpm_g3d", EXYNOS5430_ENABLE_SCLK_G3D, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_g3dnd_600, "aclk_g3dnd_600", "dout_aclk_g3d", EXYNOS5430_ENABLE_ACLK_G3D, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_async_g3d, "aclk_async_g3d", "dout_aclk_g3d", EXYNOS5430_ENABLE_ACLK_G3D, 4, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_g3d0, "aclk_bts_g3d0", "dout_aclk_g3d", EXYNOS5430_ENABLE_ACLK_G3D, 6, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_bts_g3d1, "aclk_bts_g3d1", "dout_aclk_g3d", EXYNOS5430_ENABLE_ACLK_G3D, 7, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_g3dnp_150, "aclk_g3dnp_150", "dout_pclk_g3d", EXYNOS5430_ENABLE_ACLK_G3D, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_ahb2apb_g3dp, "aclk_ahb2apb_g3dp", "dout_pclk_g3d", EXYNOS5430_ENABLE_ACLK_G3D, 3, CLK_IGNORE_UNUSED, 0),
	CGTE(aclk_asyncapbs_g3d, "aclk_asyncapbs_g3d", "dout_pclk_g3d", EXYNOS5430_ENABLE_ACLK_G3D, 5, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_sysreg_g3d, "pclk_sysreg_g3d", "dout_pclk_g3d", EXYNOS5430_ENABLE_PCLK_G3D, 0, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_pmu_g3d, "pclk_pmu_g3d", "dout_pclk_g3d", EXYNOS5430_ENABLE_PCLK_G3D, 1, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_g3d0, "pclk_bts_g3d0", "dout_pclk_g3d", EXYNOS5430_ENABLE_PCLK_G3D, 2, CLK_IGNORE_UNUSED, 0),
	CGTE(pclk_bts_g3d1, "pclk_bts_g3d1", "dout_pclk_g3d", EXYNOS5430_ENABLE_PCLK_G3D, 3, CLK_IGNORE_UNUSED, 0),
	/* DISP */
	CGTE(sclk_decon_tv_vclk_disp, "sclk_decon_tv_vclk_disp", "dout_sclk_decon_tv_vclk", EXYNOS5430_ENABLE_SCLK_MIF, 14, 0, 0),
	CGTE(sclk_decon_tv_vclk, "sclk_decon_tv_vclk", "dout_sclk_decon_tv_vclk_disp", EXYNOS5430_ENABLE_SCLK_DISP, 21, 0, 0),
	CGTE(sclk_dsim1_disp, "sclk_dsim1_disp", "dout_sclk_dsim1", EXYNOS5430_ENABLE_SCLK_MIF, 15, 0, 0),
	CGTE(phyclk_mipidphy1_rxclkesc0, "phyclk_mipidphy1_rxclkesc0", "mout_phyclk_mipidphy1_rxclkesc0_user", EXYNOS5430_ENABLE_SCLK_DISP, 25, CLK_IGNORE_UNUSED, 0),
	CGTE(phyclk_mipidphy1_bitclkdiv8, "phyclk_mipidphy1_bitclkdiv8", "mout_phyclk_mipidphy1_bitclkdiv8_user", EXYNOS5430_ENABLE_SCLK_DISP, 26, CLK_IGNORE_UNUSED, 0),
	/* AUD */
	CGTE(atclk_aud, "atclk_aud", "dout_atclk_aud", EXYNOS5430_ENABLE_SCLK_AUD0, 2, CLK_IGNORE_UNUSED, 0),
};

struct samsung_pll_rate_table pll_g3d_rate_table[] = {
	/* rate		p	m	s	k */
	{ 730000000U,	6,	365,	1,	0},
	{ 700000000U,	6,	350,	1,	0},
	{ 600000000U,	5,	500,	2,	0},
	{ 550000000U,	6,	550,	2,	0},
	{ 500000000U,	6,	500,	2,	0},
	{ 420000000U,	5,	350,	2,	0},
	{ 350000000U,	6,	350,	2,	0},
	{ 266000000U,	6,	532,	3,	0},
	{ 160000000U,	6,	320,	3,	0},
};

struct samsung_pll_rate_table pll_mem0_rate_table[] = {
	/* rate		p	m	s	k */
	{1086000000U,	4,	362,	1,	0},
	{1066000000U,	6,	533,	1,	0},
	{ 933000000U,	4,	311,	0,	0},
	{ 733000000U,	12,	733,	1,	0},
	{ 667000000U,	12,	667,	1,	0},
	{ 533000000U,	6,	533,	2,	0},
	{ 333000000U,	4,	222,	2,	0},
	{ 266000000U,	6,	532,	3,	0},
	{ 200000000U,	6,	400,	3,	0},
	{ 166000000U,	6,	332,	3,	0},
	{ 133000000U,	6,	532,	4,	0},
	{ 100000000U,	6,	400,	4,	0},
};

struct samsung_pll_rate_table pll_mem1_rate_table[] = {
	/* rate		p	m	s	k */
	{921000000U,	4,	307,	1,	0},
};

struct samsung_pll_rate_table pll_bus_rate_table[] = {
	/* rate		p	m	s	k */
	{ 825000000U,	4,	275,	1,	0},
	{ 800000000U,	3,	100,	0,	0},
	{ 400000000U,	3,	100,	1,	0},
	{ 200000000U,	3,	100,	2,	0},
};

struct samsung_pll_rate_table pll_mfc_rate_table[] = {
	/* rate		p	m	s	k */
	{1332000000U,	4,	222,	0,	0},
	{ 666000000U,	4,	222,	1,	0},
	{ 633000000U,	4,	211,	1,	0},
};

struct samsung_pll_rate_table pll_mphy_rate_table[] = {
	/* rate		p	m	s	k */
	{ 910000000U,	6,	455,	1,	0},
	{ 598000000U,	6,	598,	2,	0},
};

struct samsung_pll_rate_table pll_isp_rate_table[] = {
	/* rate		p	m	s	k */
	{ 552000000U,	5,	460,	2,	0},
	{ 444000000U,	5,	370,	2,	0},
};

struct samsung_pll_rate_table pll_aud_rate_table[] = {
	/* rate		p	m	s	k */
	{ 393216018U,	2,	131,	2,	4719},
	{ 196608009U,	2,	131,	3,	4719},
};

struct samsung_pll_rate_table pll_disp_rate_table[] = {
	/* rate		p	m	s	k */
	{ 250000000U,	3,	250,	3,	0},
	{ 142000000U,	3,	142,	3,	0},
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos5433-oscclk", .data = (void *)0, },
	{ },
};

/* register exynos5433 clocks */
void __init exynos5433_clk_init(struct device_node *np)
{
	struct clk *egl_pll, *kfc_pll, *mem0_pll, *mem1_pll, *mfc_pll,
		*bus_pll, *mphy_pll, *clk_disp_pll, *aud_pll, *g3d_pll, *isp_pll;

	samsung_clk_init(np, 0, nr_clks, (unsigned long *)exynos5433_clk_regs,
			ARRAY_SIZE(exynos5433_clk_regs), NULL, 0);

	samsung_clk_of_register_fixed_ext(exynos5433_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos5433_fixed_rate_ext_clks),
			ext_clk_match);

	egl_pll = samsung_clk_register_pll35xx("fout_egl_pll", "fin_pll",
			EXYNOS5430_EGL_PLL_LOCK,
			EXYNOS5430_EGL_PLL_CON0, NULL, 0);

	if (egl_pll == NULL)
		panic("%s: Fail to register egl_pll", __func__);

	kfc_pll = samsung_clk_register_pll35xx("fout_kfc_pll", "fin_pll",
			EXYNOS5430_KFC_PLL_LOCK,
			EXYNOS5430_KFC_PLL_CON0, NULL, 0);

	if (kfc_pll == NULL)
		panic("%s: Fail to register kfc_pll", __func__);

	mem0_pll = samsung_clk_register_pll35xx("fout_mem0_pll", "fin_pll",
			EXYNOS5430_MEM0_PLL_LOCK,
			EXYNOS5430_MEM0_PLL_CON0, pll_mem0_rate_table, ARRAY_SIZE(pll_mem0_rate_table));

	if (mem0_pll == NULL)
		panic("%s: Fail to register mem0_pll", __func__);

	mem1_pll = samsung_clk_register_pll35xx("fout_mem1_pll", "fin_pll",
			EXYNOS5430_MEM1_PLL_LOCK,
			EXYNOS5430_MEM1_PLL_CON0, pll_mem1_rate_table, ARRAY_SIZE(pll_mem1_rate_table));

	if (mem1_pll == NULL)
		panic("%s: Fail to register mem1_pll", __func__);

	mfc_pll = samsung_clk_register_pll35xx("fout_mfc_pll", "fin_pll",
			EXYNOS5430_MFC_PLL_LOCK,
			EXYNOS5430_MFC_PLL_CON0, pll_mfc_rate_table, ARRAY_SIZE(pll_mfc_rate_table));

	if (mfc_pll == NULL)
		panic("%s: Fail to register mfc_pll", __func__);

	bus_pll = samsung_clk_register_pll35xx("fout_bus_pll", "fin_pll",
			EXYNOS5430_BUS_PLL_LOCK,
			EXYNOS5430_BUS_PLL_CON0, pll_bus_rate_table, ARRAY_SIZE(pll_bus_rate_table));

	if (bus_pll == NULL)
		panic("%s: Fail to register bus_pll", __func__);

	mphy_pll = samsung_clk_register_pll35xx("fout_mphy_pll", "fin_pll",
			EXYNOS5430_MPHY_PLL_LOCK,
			EXYNOS5430_MPHY_PLL_CON0, pll_mphy_rate_table, ARRAY_SIZE(pll_mphy_rate_table));

	if (mphy_pll == NULL)
		panic("%s: Fail to register mphy_pll", __func__);

	clk_disp_pll = samsung_clk_register_pll35xx("fout_disp_pll", "fin_pll",
			EXYNOS5430_DISP_PLL_LOCK,
			EXYNOS5430_DISP_PLL_CON0, pll_disp_rate_table, ARRAY_SIZE(pll_disp_rate_table));

	if (clk_disp_pll == NULL)
		panic("%s: Fail to register disp_pll", __func__);

	aud_pll = samsung_clk_register_pll36xx("fout_aud_pll", "fin_pll",
			EXYNOS5430_AUD_PLL_LOCK,
			EXYNOS5430_AUD_PLL_CON0, pll_aud_rate_table, ARRAY_SIZE(pll_aud_rate_table));

	if (aud_pll == NULL)
		panic("%s: Fail to register aud_pll", __func__);

	g3d_pll = samsung_clk_register_pll35xx("fout_g3d_pll", "fin_pll",
			EXYNOS5430_G3D_PLL_LOCK,
			EXYNOS5430_G3D_PLL_CON0, pll_g3d_rate_table, ARRAY_SIZE(pll_g3d_rate_table));

	if (g3d_pll == NULL)
		panic("%s: Fail to register g3d_pll", __func__);

	isp_pll = samsung_clk_register_pll35xx("fout_isp_pll", "fin_pll",
			EXYNOS5430_ISP_PLL_LOCK,
			EXYNOS5430_ISP_PLL_CON0, pll_isp_rate_table, ARRAY_SIZE(pll_isp_rate_table));

	if (isp_pll == NULL)
		panic("%s: Fail to register isp_pll", __func__);
	set_pll35xx_ops(isp_pll, FULL_PLL_OPS);

	samsung_clk_add_lookup(clk_disp_pll, disp_pll);
	samsung_clk_add_lookup(isp_pll, fout_isp_pll);
	samsung_clk_add_lookup(mphy_pll, fout_mphy_pll);
	samsung_clk_add_lookup(g3d_pll, fout_g3d_pll);
	samsung_clk_add_lookup(aud_pll, fout_aud_pll);

	samsung_clk_register_fixed_rate(exynos5433_fixed_rate_clks,
			ARRAY_SIZE(exynos5433_fixed_rate_clks));
	samsung_clk_register_fixed_factor(exynos5433_fixed_factor_clks,
			ARRAY_SIZE(exynos5433_fixed_factor_clks));
	samsung_clk_register_mux(exynos5433_mux_clks,
			ARRAY_SIZE(exynos5433_mux_clks));
	samsung_clk_register_div(exynos5433_div_clks,
			ARRAY_SIZE(exynos5433_div_clks));
	samsung_clk_register_gate(exynos5433_gate_clks,
			ARRAY_SIZE(exynos5433_gate_clks));

	exynos5433_clock_init();

	pr_info("Exynos5433: Clock setup completed\n");
}
CLK_OF_DECLARE(exynos5433_clk, "samsung,exynos5433-clock", exynos5433_clk_init);
