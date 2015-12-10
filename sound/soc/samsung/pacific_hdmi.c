/*
 *  pacific_hdmi.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include <mach/regs-pmu.h>

#include "i2s.h"
#include "i2s-regs.h"

/* PACIFIC use CLKOUT from AP */
#define PACIFIC_AUD_PLL_FREQ	393216018

static struct snd_soc_card pacific_hdmi_card;

static const struct snd_soc_component_driver pacific_cmpnt = {
	.name	= "pacific-hdmi",
};

static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_aud_pll;

	fout_aud_pll = clk_get(pacific_hdmi_card.dev, "fout_aud_pll");
	if (IS_ERR(fout_aud_pll)) {
		pr_err("%s: failed to get fout_aud_pll\n", __func__);
		return PTR_ERR(fout_aud_pll);
	}

	if (rate == clk_get_rate(fout_aud_pll))
		goto out;

	clk_set_rate(fout_aud_pll, rate);
out:
	clk_put(fout_aud_pll);

	return 0;
}

static int pacific_hdmi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, div, sclk, bfs, psr, rfs, ret;
	unsigned long rclk;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 64;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 48000:
	case 96000:
	case 192000:
		if (bfs == 64)
			rfs = 512;
		else if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 12288000:
	case 18432000:
		psr = 4;
		break;
	case 24576000:
	case 36864000:
		psr = 2;
		break;
	case 49152000:
	case 73728000:
	case 98304000:
		psr = 1;
		break;
	default:
		pr_err("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;
	for (div = 2; div <= 16; div++) {
		if (sclk * div > PACIFIC_AUD_PLL_FREQ)
			break;
	}
	pll = sclk * (div - 1);
	set_aud_pll_rate(pll);

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops pacific_hdmi_ops = {
	.hw_params = pacific_hdmi_hw_params,
};

static struct snd_soc_dai_link pacific_hdmi_dai[] = {
	{ /* Aux DAI i/f */
		.name = "pacific-hdmi",
		.stream_name = "HDMI PCM Playback",
		.cpu_dai_name = "i2s1",
		.codec_name = "dummy-codec",
		.codec_dai_name = "dummy-aif1",
		.platform_name = "i2s1",
		.ops = &pacific_hdmi_ops,
	},
};

static struct snd_soc_card pacific_hdmi_card = {
	.name = "Pacific HDMI",
	.owner = THIS_MODULE,

	.dai_link = pacific_hdmi_dai,
	.num_links = ARRAY_SIZE(pacific_hdmi_dai),
};

static int pacific_hdmi_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *codec_np, *cpu_np;
	struct snd_soc_card *card = &pacific_hdmi_card;

	card->dev = &pdev->dev;

	if (np) {
		for (n = 0; n < ARRAY_SIZE(pacific_hdmi_dai); n++) {
			cpu_np = of_parse_phandle(np,
					"samsung,audio-cpu", n);
			if (cpu_np) {
				pacific_hdmi_dai[n].cpu_name = NULL;
				pacific_hdmi_dai[n].cpu_dai_name = NULL;
				pacific_hdmi_dai[n].cpu_of_node = cpu_np;

				pacific_hdmi_dai[n].platform_name = NULL;
				pacific_hdmi_dai[n].platform_of_node = cpu_np;
			} else {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-cpu'"
					" missing or invalid\n");
			}

			codec_np = of_parse_phandle(np,
					"samsung,audio-codec", n);
			if (codec_np) {
				pacific_hdmi_dai[n].codec_name = NULL;
				pacific_hdmi_dai[n].codec_of_node = codec_np;
			} else {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-codec'"
					" missing or invalid\n");
			}
		}
	} else
		dev_err(&pdev->dev, "Failed to get device node\n");

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);
		return ret;
	}

	return 0;
}

static int pacific_hdmi_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id pacific_hdmi_of_match[] = {
	{ .compatible = "samsung,pacific-hdmi", },
	{ },
};
MODULE_DEVICE_TABLE(of, pacific_hdmi_of_match);

static struct platform_driver pacific_hdmi_driver = {
	.driver	= {
		.name	= "pacific-hdmi",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pacific_hdmi_of_match),
	},
	.probe	= pacific_hdmi_probe,
	.remove	= pacific_hdmi_remove,
};

module_platform_driver(pacific_hdmi_driver);

MODULE_DESCRIPTION("ALSA SoC PACIFIC HDMI");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pacific-audio");
