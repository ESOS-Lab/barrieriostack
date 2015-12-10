/*
 * Universal7580-COD3022X Audio Machine driver.
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Sayanta Pattanayak <sayanta.p@samsung.com>
 *			<sayantapattanayak@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/s2801x.h>
#include <sound/cod3022x.h>

#include <mach/regs-pmu.h>

#include "i2s.h"
#include "i2s-regs.h"

#define COD3022X_DEFAULT_BFS		32
#define COD3022X_DEFAULT_RFS		512

#define JD_SELECT_3022_INTERNEL		0
#define JD_SELECT_EXTERNEL		1

static struct snd_soc_card universal7580_cod3022x_card;

struct cod3022x_machine_priv {
	struct snd_soc_codec *codec;
	int aifrate;
	int gpio_jd_ic_select; /* jack detection ic selection gpio */
	struct clk *bclk_codec;
};

static const struct snd_soc_component_driver universal7580_cmpnt = {
	.name = "universal7580-audio",
};

static int universal7580_aif1_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	dev_info(card->dev, "aif1: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	priv->aifrate = params_rate(params);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set Codec DAIFMT\n");
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set CPU DAIFMT\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
				COD3022X_DEFAULT_RFS, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set SAMSUNG_I2S_CDCLK\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
						0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set SAMSUNG_I2S_OPCLK\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0) {
		dev_err(card->dev,
				"aif1: Failed to set SAMSUNG_I2S_RCLKSRC_1\n");
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai,
			SAMSUNG_I2S_DIV_BCLK, COD3022X_DEFAULT_BFS);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set BFS\n");
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai,
			SAMSUNG_I2S_DIV_RCLK, COD3022X_DEFAULT_RFS);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set RFS\n");
		return ret;
	}

	ret = s2801x_hw_params(substream, params, COD3022X_DEFAULT_BFS, 1);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7580_aif2_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int bfs, ret;

	dev_info(card->dev, "aif2: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		dev_err(card->dev, "aif2: Unsupported PCM_FORMAT\n");
		return -EINVAL;
	}

	ret = s2801x_hw_params(substream, params, bfs, 2);
	if (ret < 0) {
		dev_err(card->dev, "aif2: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7580_aif3_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int bfs, ret;

	dev_info(card->dev, "aif3: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		dev_err(card->dev, "aif3: Unsupported PCM_FORMAT\n");
		return -EINVAL;
	}

	ret = s2801x_hw_params(substream, params, bfs, 3);
	if (ret < 0) {
		dev_err(card->dev, "aif3: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7580_set_bias_level(struct snd_soc_card *card,
				 struct snd_soc_dapm_context *dapm,
				 enum snd_soc_bias_level level)
{
	dev_dbg(card->dev, "%s called\n", __func__);

	return 0;
}

static int universal7580_jack_detect_dev_select(struct snd_soc_card *card)
{
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	struct device *dev = card->dev;
	int ret;

	if (of_get_property(dev->of_node, "gpios", NULL) != NULL) {
		priv->gpio_jd_ic_select = of_get_gpio(dev->of_node, 0);
		if (priv->gpio_jd_ic_select < 0) {
			dev_err(dev, "failed to get JD gpio info\n");
			return -ENODEV;
		}

		ret = gpio_request_one(priv->gpio_jd_ic_select,
				GPIOF_OUT_INIT_LOW, "jd_select");
		if (ret < 0) {
			dev_err(dev, "JD select gpio request failed\n");
			return -EINVAL;
		}

		if (of_find_property(dev->of_node, "samsung,use-externel-jd",
					NULL) != NULL) {
			gpio_set_value(priv->gpio_jd_ic_select, 1);
			cod3022x_set_externel_jd(priv->codec);
		} else
			gpio_set_value(priv->gpio_jd_ic_select, 0);
	} else {
		dev_err(dev, "Property 'gpios' not found\n");
			return -EINVAL;
	}

	return 0;
}

static int universal7580_enable_codec_bclk(struct snd_soc_card *card)
{
	struct device *dev = card->dev;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	priv->bclk_codec = clk_get(dev, "codec_bclk");
	if (IS_ERR(priv->bclk_codec)) {
		dev_err(dev, "codec_bclk not found\n");
		return PTR_ERR(priv->bclk_codec);
	}

	ret = clk_prepare_enable(priv->bclk_codec);
	if (ret < 0) {
		dev_err(dev, "clk enable failed for codec bclk\n");
		clk_put(priv->bclk_codec);
		return ret;
	}

	return 0;
}

static int universal7580_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	priv->codec = codec;

	/**
	 * If jack detector selection fails, default internel jack detector will
	 * be enabled in codec.
	 */
	ret = universal7580_jack_detect_dev_select(card);
	if (ret)
		dev_warn(codec->dev, "Failed to get jd gpios :%d\n", ret);

	/**
	 * On Universal board, Codec COD3022X requires the I2SCDCLK as
	 * bit-clock. This clock is default enabled, but it requires the I2S
	 * block to be active. Hence we need to keep this clock enabled for CP
	 * call mode.
	 */
	ret = universal7580_enable_codec_bclk(card);
	if (ret) {
		dev_err(codec->dev, "Failed to enable bclk for codec\n");
		return ret;
	}

	return 0;
}

static int s2801x_init(struct snd_soc_dapm_context *dapm)
{
	dev_dbg(dapm->dev, "%s called\n", __func__);

	return 0;
}

/**
 * TODO:
 * These are board-specific controls, widgets which bind various components of
 * audio block.
 */
static const struct snd_kcontrol_new universal7580_controls[] = {
};

const struct snd_soc_dapm_widget universal7580_dapm_widgets[] = {
};

const struct snd_soc_dapm_route universal7580_dapm_routes[] = {
};

static struct snd_soc_ops universal7580_aif1_ops = {
	.hw_params = universal7580_aif1_hw_params,
};

static struct snd_soc_ops universal7580_aif2_ops = {
	.hw_params = universal7580_aif2_hw_params,
};

static struct snd_soc_ops universal7580_aif3_ops = {
	.hw_params = universal7580_aif3_hw_params,
};

static struct snd_soc_dai_driver universal7580_ext_dai[] = {
	{
		.name = "universal7580 voice call",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
	},
	{
		.name = "universal7580 BT",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
	},
};

static struct snd_soc_dai_link universal7580_cod3022x_dai[] = {
	/* Playback and Recording */
	{
		.name = "universal7580-cod3022x",
		.stream_name = "i2s0-pri",
		.codec_dai_name = "cod3022x-aif",
		.ops = &universal7580_aif1_ops,
	},
	/* Deep buffer playback */
	{
		.name = "universal7580-cod3022x-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.stream_name = "i2s0-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "cod3022x-aif",
		.ops = &universal7580_aif1_ops,
	},
	/* Voice Call */
	{
		.name = "cp",
		.stream_name = "voice call",
		.cpu_dai_name = "universal7580 voice call",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "cod3022x-aif2",
		.ops = &universal7580_aif2_ops,
		.ignore_suspend = 1,
	},
	/* BT */
	{
		.name = "bt",
		.stream_name = "bluetooth audio",
		.cpu_dai_name = "universal7580 BT",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "dummy-aif2",
		.ops = &universal7580_aif3_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_aux_dev s2801x_aux_dev[] = {
	{
		.init = s2801x_init,
	},
};

static struct snd_soc_codec_conf s2801x_codec_conf[] = {
	{
		.name_prefix = "S2801",
	},
};

static struct snd_soc_card universal7580_cod3022x_card = {
	.name = "Universal7580-I2S",
	.owner = THIS_MODULE,

	.dai_link = universal7580_cod3022x_dai,
	.num_links = ARRAY_SIZE(universal7580_cod3022x_dai),

	.controls = universal7580_controls,
	.num_controls = ARRAY_SIZE(universal7580_controls),
	.dapm_widgets = universal7580_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal7580_dapm_widgets),
	.dapm_routes = universal7580_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal7580_dapm_routes),

	.late_probe = universal7580_late_probe,
	.set_bias_level = universal7580_set_bias_level,
	.aux_dev = s2801x_aux_dev,
	.num_aux_devs = ARRAY_SIZE(s2801x_aux_dev),
	.codec_conf = s2801x_codec_conf,
	.num_configs = ARRAY_SIZE(s2801x_codec_conf),
};

static int universal7580_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpu_np, *codec_np, *auxdev_np;
	struct snd_soc_card *card = &universal7580_cod3022x_card;
	struct cod3022x_machine_priv *priv;

	if (!np) {
		dev_err(&pdev->dev, "Failed to get device node\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	card->dev = &pdev->dev;

	ret = snd_soc_register_component(card->dev, &universal7580_cmpnt,
					universal7580_ext_dai,
					ARRAY_SIZE(universal7580_ext_dai));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);
		return ret;
	}

	for (n = 0; n < ARRAY_SIZE(universal7580_cod3022x_dai); n++) {
		/* Skip parsing DT for fully formed dai links */
		if (universal7580_cod3022x_dai[n].platform_name &&
				universal7580_cod3022x_dai[n].codec_name) {
			dev_dbg(card->dev,
			"Skipping dt for populated dai link %s\n",
			universal7580_cod3022x_dai[n].name);
			continue;
		}

		cpu_np = of_parse_phandle(np, "samsung,audio-cpu", n);
		if (!cpu_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,audio-cpu' missing\n");
			if (!universal7580_cod3022x_dai[n].cpu_dai_name)
				return -EINVAL;
		}

		codec_np = of_parse_phandle(np, "samsung,audio-codec", n);
		if (!codec_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,audio-codec' missing\n");
			return -EINVAL;
		}

		universal7580_cod3022x_dai[n].codec_of_node = codec_np;
		if (!universal7580_cod3022x_dai[n].cpu_dai_name)
			universal7580_cod3022x_dai[n].cpu_of_node = cpu_np;

		if (!universal7580_cod3022x_dai[n].platform_name)
			universal7580_cod3022x_dai[n].platform_of_node = cpu_np;
	}

	for (n = 0; n < ARRAY_SIZE(s2801x_aux_dev); n++) {
		auxdev_np = of_parse_phandle(np, "samsung,auxdev", n);
		if (!auxdev_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,auxdev' missing\n");
			return -EINVAL;
		}
		s2801x_aux_dev[n].of_node = auxdev_np;
		s2801x_codec_conf[n].of_node = auxdev_np;
	}

	snd_soc_card_set_drvdata(card, priv);

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);

	return ret;
}

static int universal7580_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);

	if (!IS_ERR(priv->bclk_codec)) {
		clk_disable_unprepare(priv->bclk_codec);
		clk_put(priv->bclk_codec);
	}

	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id universal7580_cod3022x_of_match[] = {
	{.compatible = "samsung,universal7580-cod3022x",},
	{},
};
MODULE_DEVICE_TABLE(of, universal7580_cod3022x_of_match);

static struct platform_driver universal7580_audio_driver = {
	.driver = {
		.name = "Universal7580-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(universal7580_cod3022x_of_match),
	},
	.probe = universal7580_audio_probe,
	.remove = universal7580_audio_remove,
};

module_platform_driver(universal7580_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Universal7580 COD3022X");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:universal7580-audio");
