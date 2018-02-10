/*
 * Platform data for MAX98505
 *
 * Copyright 2011-2012 Maxim Integrated Products
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __SOUND_MAX98505_PDATA_H__
#define __SOUND_MAX98505_PDATA_H__

enum max98505_slave_master_mode {
	MODE_SLAVE,
	MODE_MASTER,
};

enum max98505_pcm_format {
	PCM_FORMAT_S16_LE,
	PCM_FORMAT_S24_LE,
	PCM_FORMAT_S32_LE,
};

enum max98505_dai_format {
	DAIFMT_I2S,
	DAIFMT_LEFT_J,
	DAIFMT_DSP_A,
};

enum max98505_dai_inv_format {
	DAIFMT_NB_NF,
	DAIFMT_NB_IF,
	DAIFMT_IB_NF,
	DAIFMT_IB_IF
};

struct max98505_cfg_data {
	u32 dout_hiz_cfg1;
	u32 dout_hiz_cfg2;
	u32 dout_hiz_cfg3;
	u32 dout_hiz_cfg4;
	u32 filters;
	u32 alc_cfg;
	u32 enable_cfg;
	u32 clik_mode1;
	u32 clik_mode2;
	u32 vmon_dout_cfg;
	u32 imon_dout_cfg;
};

struct max98505_format_data {
	u32 pcm_format;
	u32 dai_format;
	u32 dai_inv_format;
	u32 dai_data_dly;
	u32 pcm_samplerate;
};

/* codec platform data */
struct max98505_pdata {
	int irq;
	u32 tdm_slot_select;
	u32 boost_current_limit;
	u32 dac_input_sel;
	u32 slave_master_mode;
	u32 spk_volume;
	unsigned int sysclk;
	struct max98505_format_data format_data;
	struct max98505_cfg_data cfg_data;
};
#endif /* __SOUND_MAX98505_PDATA_H__ */
