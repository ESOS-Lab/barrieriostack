/*
 * Platform data for MAX98504
 *
 * Copyright 2011-2012 Maxim Integrated Products
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __SOUND_MAXIM_DSM_H__
#define __SOUND_MAXIM_DSM_H__

enum	{
	PARAM_VOICE_COIL_TEMP,
	PARAM_EXCURSION,
	PARAM_RDC,
	PARAM_Q,
	PARAM_FRES,
	PARAM_EXCUR_LIMIT,
	PARAM_VOICE_COIL,
	PARAM_THERMAL_LIMIT,
	PARAM_RELEASE_TIME,
	PARAM_ONOFF,
	PARAM_STATIC_GAIN,
	PARAM_LFX_GAIN,
	PARAM_PILOT_GAIN,
	PARAM_WRITE_FLAG,
	PARAM_FEATURE_SET,
	PARAM_SMOOTH_VOLT,
	PARAM_HPF_CUTOFF,
	PARAM_LEAD_R,
	PARAM_RMS_SMOO_FAC,
	PARAM_CLIP_LIMIT,
	PARAM_THERMAL_COEF,
	PARAM_QSPK,
	PARAM_EXCUR_LOG_THRESH,
	PARAM_TEMP_LOG_THRESH,
	PARAM_RES_FREQ,
	PARAM_RES_FREQ_GUARD_BAND,
	PARAM_DSM_3_0_MAX,
};

enum {
	PARAM_AMBIENT_TEMP = PARAM_DSM_3_0_MAX,
	PARAM_STL_ATTACK_TIME,
	PARAM_STL_RELEASE_TIME,
	PARAM_ADMITTANCE_A1,
	PARAM_ADMITTANCE_A2,
	PARAM_ADMITTANCE_B0,
	PARAM_ADMITTANCE_B1,
	PARAM_ADMITTANCE_B2,
	PARAM_TCH1,
	PARAM_RTH1,
	PARAM_TCH2,
	PARAM_RTH2,
	PARAM_STL_ATTENUATION_GAIN,
	PARAM_SPT_RAMP_DOWN_FRAMES,
	PARAM_SPT_THRESHOLD,
	PARAM_T_HORIZON,
	PARAM_LFX_ADMITTANCE_A1,
	PARAM_LFX_ADMITTANCE_A2,
	PARAM_LFX_ADMITTANCE_B0,
	PARAM_LFX_ADMITTANCE_B1,
	PARAM_LFX_ADMITTANCE_B2,
	PARAM_RESERVE0,
	PARAM_RESERVE1,
	PARAM_RESERVE2,
	PARAM_RESERVE3,
	PARAM_RESERVE4,
	PARAM_DSM_3_5_MAX,
};

#define USE_DSM_LOG
#define LOG_BUFFER_ARRAY_SIZE 10

/* BUFSIZE must be 4 bytes allignment*/
#define BEFORE_BUFSIZE (4+(LOG_BUFFER_ARRAY_SIZE*2))
#define AFTER_BUFSIZE (LOG_BUFFER_ARRAY_SIZE*4)

extern ssize_t maxdsm_a_log_prepare(char *buf);
extern void maxdsm_a_param_update(const void *param_array);
extern void maxdsm_a_log_update(const void *byte_log_array,
	const void *int_log_array, const void *after_prob_byte_log_array,
	const void *after_prob_int_log_array);
#endif
