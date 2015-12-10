/* linux/drivers/video/decon_display/decon_mipi_dsi_lowlevel.h
 *
 * Header file for Samsung MIPI-DSI lowlevel driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Haowei Li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _S5P_MIPI_DSI_LOWLEVEL_H
#define _S5P_MIPI_DSI_LOWLEVEL_H

#include "decon_mipi_dsi.h"

#ifdef CONFIG_DECON_MIC
unsigned int s5p_mipi_dsi_calc_bs_size(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_enable_mic(struct mipi_dsim_device *dsim, bool enable);
void s5p_mipi_dsi_set_3d_off_mic_on_h_size(struct mipi_dsim_device *dsim);
#endif
void s5p_mipi_dsi_func_reset(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_sw_reset(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_set_interrupt_mask(struct mipi_dsim_device *dsim,
		unsigned int mode, unsigned int mask);
void s5p_mipi_dsi_set_data_lane_number(struct mipi_dsim_device *dsim,
		unsigned int count);
void s5p_mipi_dsi_init_fifo_pointer(struct mipi_dsim_device *dsim,
		unsigned int cfg);
void s5p_mipi_dsi_set_phy_tunning(struct mipi_dsim_device *dsim,
		unsigned int value);
void s5p_mipi_dsi_set_main_disp_resol(struct mipi_dsim_device *dsim,
		unsigned int vert_resol, unsigned int hori_resol);
void s5p_mipi_dsi_set_main_disp_vporch(struct mipi_dsim_device *dsim,
	unsigned int cmd_allow, unsigned int vfront, unsigned int vback);
void s5p_mipi_dsi_set_main_disp_hporch(struct mipi_dsim_device *dsim,
		unsigned int front, unsigned int back);
void s5p_mipi_dsi_set_main_disp_sync_area(struct mipi_dsim_device *dsim,
		unsigned int vert, unsigned int hori);
void s5p_mipi_dsi_set_sub_disp_resol(struct mipi_dsim_device *dsim,
		unsigned int vert, unsigned int hori);
void s5p_mipi_dsi_init_config(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_display_config(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_set_data_lane_number(struct mipi_dsim_device *dsim,
		unsigned int count);
void s5p_mipi_dsi_enable_lane(struct mipi_dsim_device *dsim,
		unsigned int lane, unsigned int enable);
void s5p_mipi_dsi_enable_afc(struct mipi_dsim_device *dsim,
		unsigned int enable, unsigned int afc_code);
void s5p_mipi_dsi_enable_pll_bypass(struct mipi_dsim_device *dsim,
		unsigned int enable);
void s5p_mipi_dsi_set_pll_pms(struct mipi_dsim_device *dsim,
		unsigned int p, unsigned int m, unsigned int s);
void s5p_mipi_dsi_pll_freq_band(struct mipi_dsim_device *dsim,
		unsigned int freq_band);
void s5p_mipi_dsi_pll_freq(struct mipi_dsim_device *dsim,
		unsigned int pre_divider, unsigned int main_divider,
		unsigned int scaler);
void s5p_mipi_dsi_pll_stable_time(struct mipi_dsim_device *dsim,
		unsigned int lock_time);
void s5p_mipi_dsi_enable_pll(struct mipi_dsim_device *dsim,
		unsigned int enable);
void s5p_mipi_dsi_set_byte_clock_src(struct mipi_dsim_device *dsim,
		unsigned int src);
void s5p_mipi_dsi_enable_byte_clock(struct mipi_dsim_device *dsim,
		unsigned int enable);
void s5p_mipi_dsi_set_esc_clk_prs(struct mipi_dsim_device *dsim,
		unsigned int enable, unsigned int prs_val);
void s5p_mipi_dsi_enable_esc_clk_on_lane(struct mipi_dsim_device *dsim,
		unsigned int lane_sel, unsigned int enable);
void s5p_mipi_dsi_force_dphy_stop_state(struct mipi_dsim_device *dsim,
		unsigned int enable);
void s5p_mipi_dsi_force_bta(struct mipi_dsim_device *dsim);
unsigned int s5p_mipi_dsi_is_lane_state(struct mipi_dsim_device *dsim);
unsigned int s5p_mipi_dsi_is_hs_state(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_set_stop_state_counter(struct mipi_dsim_device *dsim,
		unsigned int cnt_val);
void s5p_mipi_dsi_set_bta_timeout(struct mipi_dsim_device *dsim,
		unsigned int timeout);
void s5p_mipi_dsi_set_lpdr_timeout(struct mipi_dsim_device *dsim,
		unsigned int timeout);
void s5p_mipi_dsi_set_lcdc_transfer_mode(struct mipi_dsim_device *dsim,
		unsigned int lp);
void s5p_mipi_dsi_set_cpu_transfer_mode(struct mipi_dsim_device *dsim,
		unsigned int lp);
void s5p_mipi_dsi_enable_hs_clock(struct mipi_dsim_device *dsim,
		unsigned int enable);
void s5p_mipi_dsi_dp_dn_swap(struct mipi_dsim_device *dsim,
		unsigned int swap_en);
void s5p_mipi_dsi_hs_zero_ctrl(struct mipi_dsim_device *dsim,
		unsigned int hs_zero);
void s5p_mipi_dsi_prep_ctrl(struct mipi_dsim_device *dsim, unsigned int
		prep);
void s5p_mipi_dsi_clear_interrupt(struct mipi_dsim_device *dsim,
		unsigned int int_src);
void s5p_mipi_dsi_clear_all_interrupt(struct mipi_dsim_device *dsim);
unsigned int s5p_mipi_dsi_is_pll_stable(struct mipi_dsim_device *dsim);
unsigned int s5p_mipi_dsi_get_fifo_state(struct mipi_dsim_device *dsim);
unsigned int _s5p_mipi_dsi_get_frame_done_status(struct mipi_dsim_device *dsim);
void _s5p_mipi_dsi_clear_frame_done(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_wr_tx_header(struct mipi_dsim_device *dsim,
		unsigned int di, unsigned int data0, unsigned int data1);
void s5p_mipi_dsi_wr_tx_data(struct mipi_dsim_device *dsim,
		unsigned int tx_data);
unsigned int s5p_mipi_dsi_get_int_status(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_clear_int_status(struct mipi_dsim_device *dsim, unsigned int intSrc);
unsigned int s5p_mipi_dsi_get_FIFOCTRL_status(struct mipi_dsim_device *dsim);
void s5p_mipi_dsi_set_b_dphyctrl(struct mipi_dsim_device *dsim,
	unsigned int ulpsexitctrl);
void s5p_mipi_dsi_set_timing_register0(struct mipi_dsim_device *dsim,
	unsigned int m_tlpxctl, unsigned int m_thsexitctl);
void s5p_mipi_dsi_set_timing_register1(struct mipi_dsim_device *dsim,
	unsigned int m_tclkprprctl, unsigned int m_tclkzeroctl,
	unsigned int m_tclkpostctl, unsigned int m_tclktrailctl);
void s5p_mipi_dsi_set_timing_register2(struct mipi_dsim_device *dsim,
	unsigned int m_thsprprctl, unsigned int m_thszeroctl,
	unsigned int m_thstrailctl);
void s5p_mipi_dsi_enable_ulps_clk_data(struct mipi_dsim_device *dsim,
	unsigned int enable);
void s5p_mipi_dsi_enable_ulps_exit_clk_data(struct mipi_dsim_device *dsim,
	unsigned int enable);
unsigned int s5p_mipi_dsi_is_ulps_lane_state(struct mipi_dsim_device *dsim,
	unsigned int enable);
void s5p_mipi_dsi_enable_main_standby(struct mipi_dsim_device *dsim,
	unsigned int enable);
int s5p_mipi_dsi_pkt_go_enable(struct mipi_dsim_device *dsim, bool enable);
int s5p_mipi_dsi_pkt_go_ready(struct mipi_dsim_device *dsim);
#ifdef CONFIG_SOC_EXYNOS5422
int s5p_mipi_dsi_pkt_go_cnt(struct mipi_dsim_device *dsim, unsigned int count);
#endif

#endif /* _S5P_MIPI_DSI_LOWLEVEL_H */
