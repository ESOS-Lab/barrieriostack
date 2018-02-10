/*
 * es705-i2s.c  --  Audience eS705 I2S interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/esxxx.h>
#include "es705.h"

static int es705_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s():\n", __func__);

	return rc;
}

static int es705_i2s_set_pll(struct snd_soc_dai *dai, int pll_id,
			     int source, unsigned int freq_in,
			     unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s():\n", __func__);

	return rc;
}

static int es705_i2s_set_clkdiv(struct snd_soc_dai *dai, int div_id,
				int div)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s():\n", __func__);

	return rc;
}

static int es705_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s():\n", __func__);

	return rc;
}

static int es705_i2s_set_tdm_slot(struct snd_soc_dai *dai,
				  unsigned int tx_mask, unsigned int rx_mask,
				  int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s():\n", __func__);

	return rc;
}

static int es705_i2s_set_channel_map(struct snd_soc_dai *dai,
				     unsigned int tx_num, unsigned int *tx_slot,
				     unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s():\n", __func__);

	return rc;
}

static int es705_i2s_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int paramid = 0;
	unsigned int val = 0;

	dev_dbg(codec->dev, "%s():\n", __func__);

	switch (dai->id) {
	case 0:
		break;

	case 1:
		break;

	default:
		return -EINVAL;
	}

	if (tristate)
		val = 0x0001;
	else
		val = 0x0000;

	return snd_soc_write(codec, paramid, val);
}

static int es705_i2s_port_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int paramid = 0;
	unsigned int val = 0;

	dev_dbg(codec->dev, "%s():\n", __func__);

	/* Is this valid since DACs are not statically mapped to DAIs? */
	switch (dai->id) {
	case 0:
		break;

	case 1:
		break;

	default:
		return -EINVAL;
	}

	if (mute)
		val = 0x0000;
	else
		val = 0x0001;

	return snd_soc_write(codec, paramid, val);
}

static int es705_i2s_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	/* struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec); */
	int bits_per_sample = 0;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);
	switch (dai->id) {
	case 0:
		dev_dbg(codec->dev, "%s(): ES705_PORTA_PARAM_ID\n", __func__);
		break;

	case 1:
		dev_dbg(codec->dev, "%s(): ES705_PORTB_PARAM_ID\n", __func__);
		break;

	default:
		dev_dbg(codec->dev, "%s(): unknown port\n", __func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s(): params_channels(params) = %d\n", __func__,
		params_channels(params));
	switch (params_channels(params)) {
	case 1:
		dev_dbg(codec->dev, "%s(): 1 channel\n", __func__);
		break;

	case 2:
		dev_dbg(codec->dev, "%s(): 2 channels\n", __func__);
		break;

	case 4:
		dev_dbg(codec->dev, "%s(): 4 channels\n", __func__);
		break;

	default:
		dev_dbg(codec->dev, "%s(): unsupported number of channels\n",
		__func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s(): params_rate(params) = %d\n", __func__,
		params_rate(params));
	switch (params_rate(params)) {
	case 8000:
		dev_dbg(codec->dev, "%s(): 8000Hz\n", __func__);
		break;

	case 11025:
		dev_dbg(codec->dev, "%s(): 11025\n", __func__);
		break;

	case 16000:
		dev_dbg(codec->dev, "%s(): 16000\n", __func__);
		break;

	case 22050:
		dev_dbg(codec->dev, "%s(): 22050\n", __func__);
		break;

	case 32000:
		dev_dbg(codec->dev, "%s(): 32000\n", __func__);
		break;

	case 48000:
		dev_dbg(codec->dev, "%s(): 48000\n", __func__);
		break;

	case 96000:
		dev_dbg(codec->dev, "%s(): 96000\n", __func__);
		break;

	case 192000:
		dev_dbg(codec->dev, "%s(): 96000\n", __func__);
		break;

	default:
		dev_dbg(codec->dev, "%s(): unsupported rate = %d\n",
			__func__, params_rate(params));
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dev_dbg(codec->dev, "%s(): S16_LE\n", __func__);
		bits_per_sample = 16;
		break;

	case SNDRV_PCM_FORMAT_S16_BE:
		dev_dbg(codec->dev, "%s(): S16_BE\n", __func__);
		bits_per_sample = 16;
		break;

	case SNDRV_PCM_FORMAT_S20_3LE:
		dev_dbg(codec->dev, "%s(): S20_3LE\n", __func__);
		bits_per_sample = 20;
		break;

	case SNDRV_PCM_FORMAT_S20_3BE:
		dev_dbg(codec->dev, "%s(): S20_3BE\n", __func__);
		bits_per_sample = 20;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		dev_dbg(codec->dev, "%s(): S24_LE\n", __func__);
		bits_per_sample = 24;
		break;

	case SNDRV_PCM_FORMAT_S24_BE:
		dev_dbg(codec->dev, "%s(): S24_BE\n", __func__);
		bits_per_sample = 24;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		dev_dbg(codec->dev, "%s(): S32_LE\n", __func__);
		bits_per_sample = 32;
		break;

	case SNDRV_PCM_FORMAT_S32_BE:
		dev_dbg(codec->dev, "%s(): S32_BE\n", __func__);
		bits_per_sample = 32;
		break;

	default:
		dev_dbg(codec->dev, "%s(): unknown format\n", __func__);
		return -EINVAL;
	}

	if (rc) {
		dev_dbg(codec->dev, "%s(): snd_soc_update_bits() failed\n",
			__func__);
		return rc;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s(): PLAYBACK\n", __func__);
	else
		dev_dbg(codec->dev, "%s(): CAPTURE\n", __func__);

	return rc;
}

static int es705_i2s_hw_free(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s(): PLAYBACK\n", __func__);
	else
		dev_dbg(codec->dev, "%s(): CAPTURE\n", __func__);

	return rc;
}

struct snd_soc_dai_ops es705_i2s_port_dai_ops = {
	.set_sysclk	= es705_i2s_set_sysclk,
	.set_pll		= es705_i2s_set_pll,
	.set_clkdiv	= es705_i2s_set_clkdiv,
	.set_fmt		= es705_i2s_set_dai_fmt,
	.set_tdm_slot	= es705_i2s_set_tdm_slot,
	.set_channel_map	= es705_i2s_set_channel_map,
	.set_tristate	= es705_i2s_set_tristate,
	.digital_mute	= es705_i2s_port_mute,
	.startup		= NULL,
	.shutdown	= NULL,
	.hw_params	= es705_i2s_hw_params,
	.hw_free		= es705_i2s_hw_free,
	.prepare		= NULL,
	.trigger		= NULL,
};

#define ES705_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define ES705_SLIMBUS_RATES (SNDRV_PCM_RATE_48000)

#define ES705_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |\
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |\
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE)
#define ES705_SLIMBUS_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S16_BE)

MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL v2");
