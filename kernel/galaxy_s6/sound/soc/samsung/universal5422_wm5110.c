/*
 *  universal5422_wm5110.c
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
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/tlv.h>

#include <linux/mfd/arizona/registers.h>
#include <linux/mfd/arizona/core.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include <mach/regs-pmu.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/wm5102.h"
#include "../codecs/florida.h"

/* XYREF use CLKOUT from AP */
#define XYREF_MCLK_FREQ		24000000
#define XYREF_AUD_PLL_FREQ	393216018

#define XYREF_DEFAULT_MCLK1	24000000
#define XYREF_DEFAULT_MCLK2	32768

#define XYREF_TELCLK_RATE	(48000 * 1024)

static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);

enum {
	XYREF_PLAYBACK_DAI,
	XYREF_VOICECALL_DAI,
	XYREF_BT_SCO_DAI,
	XYREF_MULTIMEDIA_DAI,
	XYREF_HDMI_DAI,
};

struct arizona_machine_priv {
	int mic_bias_gpio;
	int clock_mode;
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct snd_soc_dai *aif[3];
	struct delayed_work mic_work;
	int aif2mode;

	int aif1rate;
	int aif2rate;

	unsigned int hp_impedance_step;
};

const char *aif2_mode_text[] = {
	"Slave", "Master"
};

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static struct snd_soc_card universal_card;

static const struct snd_soc_component_driver universal_cmpnt = {
	.name	= "universal-audio",
};

static struct {
	int min;           /* Minimum impedance */
	int max;           /* Maximum impedance */
	unsigned int gain; /* Register value to set for this measurement */
} hp_gain_table[] = {
	{    0,      42, 0 },
	{   43,     100, 2 },
	{  101,     200, 4 },
	{  201,     450, 6 },
	{  451,    1000, 8 },
	{ 1001, INT_MAX, 0 },
};

static struct snd_soc_codec *the_codec;

void universal5422_wm5110_hpdet_cb(unsigned int meas)
{
	int i;
	struct arizona_machine_priv *priv;

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	priv = snd_soc_card_get_drvdata(the_codec->card);

	for (i = 0; i < ARRAY_SIZE(hp_gain_table); i++) {
		if (meas < hp_gain_table[i].min || meas > hp_gain_table[i].max)
			continue;

		dev_info(the_codec->dev, "SET GAIN %d step for %d ohms\n",
			 hp_gain_table[i].gain, meas);
		priv->hp_impedance_step = hp_gain_table[i].gain;
	}
}

static int arizona_put_impedance_volsw(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct arizona_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int err;
	unsigned int val, val_mask;

	val = (ucontrol->value.integer.value[0] & mask);
	val += priv->hp_impedance_step;
	dev_info(codec->dev, "SET GAIN %d according to impedance, moved %d step\n",
			 val, priv->hp_impedance_step);

	if (invert)
		val = max - val;
	val_mask = mask << shift;
	val = val << shift;

	err = snd_soc_update_bits_locked(codec, reg, val_mask, val);
	if (err < 0)
		return err;

	return err;
}

static void universal_enable_mclk(bool on)
{
	pr_info("%s: %s\n", __func__, on ? "on" : "off");

	writel(on ? 0x1000 : 0x1001, EXYNOS_PMU_DEBUG);
}

static int get_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct arizona_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);

	ucontrol->value.integer.value[0] = priv->aif2mode;
	return 0;
}

static int set_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct arizona_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);

	priv->aif2mode = ucontrol->value.integer.value[0];

	dev_info(codec->dev, "set aif2 mode: %s\n",
					 aif2_mode_text[priv->aif2mode]);
	return  0;
}

static int universal_ext_mainmicbias(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct arizona_machine_priv *priv
		= snd_soc_card_get_drvdata(card);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		gpio_set_value(priv->mic_bias_gpio,  1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		gpio_set_value(priv->mic_bias_gpio,  0);
		break;
	}

	dev_err(w->dapm->dev, "Main Mic BIAS: %d\n", event);

	return 0;
}

static const struct snd_kcontrol_new universal_codec_controls[] = {
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),

	SOC_SINGLE_EXT_TLV("HPOUT1L Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1L,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, arizona_put_impedance_volsw,
		digital_tlv),

	SOC_SINGLE_EXT_TLV("HPOUT1R Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1R,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, arizona_put_impedance_volsw,
		digital_tlv),
};

static const struct snd_kcontrol_new universal_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("VPS"),
	SOC_DAPM_PIN_SWITCH("HDMI"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Third Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

const struct snd_soc_dapm_widget universal_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("HDMIL"),
	SND_SOC_DAPM_OUTPUT("HDMIR"),
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("VPS", NULL),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", universal_ext_mainmicbias),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Third Mic", NULL),
};

const struct snd_soc_dapm_route universal_dapm_routes[] = {
	{ "HDMIL", NULL, "AIF1RX1" },
	{ "HDMIR", NULL, "AIF1RX2" },
	{ "HDMI", NULL, "HDMIL" },
	{ "HDMI", NULL, "HDMIR" },

	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "VPS", NULL, "HPOUT2L" },
	{ "VPS", NULL, "HPOUT2R" },

#ifdef CONFIG_MFD_FLORIDA
	{ "RCV", NULL, "HPOUT3L" },
	{ "RCV", NULL, "HPOUT3R" },
#else
	{ "RCV", NULL, "EPOUTN" },
	{ "RCV", NULL, "EPOUTP" },
#endif

	/* SEL of main mic is connected to GND */
	{ "IN1R", NULL, "Main Mic" },
	{ "Main Mic", NULL, "MICVDD" },

	/* Headset mic is Analog Mic */
	{ "Headset Mic", NULL, "MICBIAS1" },
	{ "IN2R", NULL, "Headset Mic" },

	/* SEL of 2nd mic is connected to MICBIAS2 */
	{ "Sub Mic", NULL, "MICBIAS2" },
	{ "IN4L", NULL, "Sub Mic" },

	/* SEL of 3rd mic is connected to GND */
	{ "Third Mic", NULL, "MICBIAS3" },
	{ "IN3R", NULL, "Third Mic" },
};

int universal_set_media_clocking(struct arizona_machine_priv *priv)
{
	struct snd_soc_codec *codec = priv->codec;
	struct snd_soc_card *card = codec->card;
	int ret, fs;

	if (priv->aif1rate >= 192000)
		fs = 256;
	else
		fs = 1024;

#ifdef CONFIG_MFD_FLORIDA
	ret = snd_soc_codec_set_pll(codec, FLORIDA_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}
	ret = snd_soc_codec_set_pll(codec, FLORIDA_FLL1, ARIZONA_CLK_SRC_MCLK1,
				    XYREF_DEFAULT_MCLK1,
				    priv->aif1rate * fs);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}
#else
	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}
	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1, ARIZONA_CLK_SRC_MCLK1,
				    XYREF_DEFAULT_MCLK1,
				    priv->aif1rate * fs);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}
#endif

	ret = snd_soc_codec_set_sysclk(codec,
				       ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       priv->aif1rate * fs,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL1: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				       ARIZONA_CLK_SRC_FLL2,
				       XYREF_TELCLK_RATE,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev,
				 "Unable to set ASYNCCLK to FLL2: %d\n", ret);

	/* AIF1 from SYSCLK, AIF2 and 3 from ASYNCCLK */
/*	ret = snd_soc_dai_set_sysclk(priv->aif[0], ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret < 0)
		dev_err(card->dev, "Can't set AIF1 to SYSCLK: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(priv->aif[1], ARIZONA_CLK_ASYNCCLK, 0, 0);
	if (ret < 0)
		dev_err(card->dev, "Can't set AIF2 to ASYNCCLK: %d\n", ret);
*/
	return 0;
}

static int universal_aif1_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static void universal_aif1_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);
}

static int universal_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct arizona_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	dev_info(card->dev, "aif1: %dch, %dHz, %dbytes\n",
			params_channels(params), params_rate(params),
			params_buffer_bytes(params));

	priv->aif1rate = params_rate(params);

	universal_set_media_clocking(priv);

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 codec fmt: %d\n", ret);
		return ret;
	}

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 cpu fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_CDCL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_OPCL: %d\n", ret);
		return ret;
	}

	return ret;
}

static int universal_aif1_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static int universal_aif1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static int universal_aif1_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static struct snd_soc_ops universal_aif1_ops = {
	.startup = universal_aif1_startup,
	.shutdown = universal_aif1_shutdown,
	.hw_params = universal_aif1_hw_params,
	.hw_free = universal_aif1_hw_free,
	.prepare = universal_aif1_prepare,
	.trigger = universal_aif1_trigger,
};

#ifdef CONFIG_SND_SAMSUNG_AUX_HDMI
static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_aud_pll;

	fout_aud_pll = clk_get(universal_card.dev, "fout_aud_pll");
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
#endif

static struct snd_soc_dai_driver universal_ext_dai[] = {
	{
		.name = "universal-ext voice call",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "universal-ext bluetooth sco",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static struct snd_soc_dai_link universal_dai[] = {
	{ /* playback & recording */
		.name = "universal-arizona playback",
		.stream_name = "i2s0-pri",
#ifdef CONFIG_MFD_FLORIDA
		.codec_dai_name = "florida-aif1",
#else
		.codec_dai_name = "wm5102-aif1",
#endif
		.ops = &universal_aif1_ops,
	},
	{ /* deep buffer playback */
		.name = "universal-arizona multimedia playback",
		.stream_name = "i2s0-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
#ifdef CONFIG_MFD_FLORIDA
		.codec_dai_name = "florida-aif1",
#else
		.codec_dai_name = "wm5102-aif1",
#endif
		.ops = &universal_aif1_ops,
	},
};

static int universal_of_get_pdata(struct snd_soc_card *card)
{
	struct device_node *pdata_np;
	struct arizona_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	pdata_np = of_find_node_by_path("/audio_pdata");
	if (!pdata_np) {
		dev_err(card->dev,
			"Property 'samsung,audio-cpu' missing or invalid\n");
		return -EINVAL;
	}

	priv->mic_bias_gpio = of_get_named_gpio(pdata_np, "mic_bias_gpio", 0);

	ret = gpio_request(priv->mic_bias_gpio, "MICBIAS_EN_AP");
	if (ret)
		dev_err(card->dev, "Failed to request gpio: %d\n", ret);

	gpio_direction_output(priv->mic_bias_gpio, 0);

	return 0;
}

static void ez2ctrl_debug_cb(void)
{
	printk("Ez2Control Callback invoked!\n");
	return;
}

static int universal_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct arizona_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int i, ret;

	priv->codec = codec;
	the_codec = codec;

	universal_of_get_pdata(card);

	for (i = 0; i < 3; i++)
		priv->aif[i] = card->rtd[i].codec_dai;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	/* close codec device immediately when pcm is closed */
	codec->ignore_pmdown_time = true;

	universal_enable_mclk(true);

	ret = snd_soc_add_codec_controls(codec, universal_codec_controls,
					ARRAY_SIZE(universal_codec_controls));

	if (ret < 0) {
		dev_err(codec->dev,
				"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	snd_soc_dapm_ignore_suspend(&card->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VPS");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HP");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Capture");
	snd_soc_dapm_sync(&codec->dapm);

	ret = snd_soc_codec_set_sysclk(codec,
				       ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       48000 * 1024,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL1: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				       ARIZONA_CLK_SRC_FLL2,
				       XYREF_TELCLK_RATE,
				       SND_SOC_CLOCK_IN);

	dev_info(card->dev, "%s: Successfully created\n", __func__);
	arizona_set_hpdet_cb(codec, universal5422_wm5110_hpdet_cb);
	arizona_set_ez2ctrl_cb(codec, ez2ctrl_debug_cb);

	return 0;
}

static int universal_suspend_post(struct snd_soc_card *card)
{
	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static int universal_resume_pre(struct snd_soc_card *card)
{
	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static int universal_start_sysclk(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret, fs;

	if (!priv->aif1rate)
		priv->aif1rate = 48000;

	if (priv->aif1rate >= 192000)
		fs = 256;
	else
		fs = 1024;

	universal_enable_mclk(1);

	ret = snd_soc_codec_set_pll(priv->codec, FLORIDA_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(priv->codec, FLORIDA_FLL1,
					ARIZONA_CLK_SRC_MCLK1,
					XYREF_DEFAULT_MCLK1,
					priv->aif1rate * fs);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}

	return ret;
}

static int universal_stop_sysclk(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	ret = snd_soc_codec_set_pll(priv->codec, FLORIDA_FLL1, 0, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(priv->codec, FLORIDA_FLL2, 0, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);
		return ret;
	}

	universal_enable_mclk(0);

	return ret;
}

static int universal_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct arizona_machine_priv *priv = snd_soc_card_get_drvdata(card);

	if (!priv->codec || dapm != &priv->codec->dapm)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (card->dapm.bias_level == SND_SOC_BIAS_OFF)
			universal_start_sysclk(card);
		break;
	case SND_SOC_BIAS_OFF:
		universal_stop_sysclk(card);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	default:
	break;
	}

	card->dapm.bias_level = level;
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int universal_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static struct snd_soc_card universal_card = {
	.name = "universal5422 Sound",
	.owner = THIS_MODULE,

	.dai_link = universal_dai,
	.num_links = ARRAY_SIZE(universal_dai),

	.controls = universal_controls,
	.num_controls = ARRAY_SIZE(universal_controls),
	.dapm_widgets = universal_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal_dapm_widgets),
	.dapm_routes = universal_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal_dapm_routes),

	.late_probe = universal_late_probe,

	.suspend_post = universal_suspend_post,
	.resume_pre = universal_resume_pre,

	.set_bias_level = universal_set_bias_level,
	.set_bias_level_post = universal_set_bias_level_post,
};

static int universal_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *codec_np, *cpu_np;
	struct snd_soc_card *card = &universal_card;
	struct arizona_machine_priv *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	card->dev = &pdev->dev;

	ret = snd_soc_register_component(card->dev, &universal_cmpnt,
				universal_ext_dai, ARRAY_SIZE(universal_ext_dai));
	if (ret != 0)
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);

	if (np) {
		for (n = 0; n < ARRAY_SIZE(universal_dai); n++) {

			/* Skip parsing DT for fully formed dai links */
			if (universal_dai[n].platform_name &&
			    universal_dai[n].codec_name) {
				dev_dbg(card->dev,
					"Skipping dt for populated"
					"dai link %s\n",
					universal_dai[n].name);
				continue;
			}

			cpu_np = of_parse_phandle(np,
					"samsung,audio-cpu", n);
			if (!cpu_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-cpu'"
					" missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			codec_np = of_parse_phandle(np,
					"samsung,audio-codec", n);
			if (!codec_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-codec'"
					" missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			if (!universal_dai[n].cpu_dai_name)
				universal_dai[n].cpu_of_node = cpu_np;

			if (!universal_dai[n].platform_name)
				universal_dai[n].platform_of_node = cpu_np;

			universal_dai[n].codec_of_node = codec_np;
		}
	} else
		dev_err(&pdev->dev, "Failed to get device node\n");

	snd_soc_card_set_drvdata(card, priv);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);
		goto out;
	}

	return ret;

out:
	kfree(priv);

	return ret;
}

static int universal_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct arizona_machine_priv *priv = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	kfree(priv);

	return 0;
}

static const struct of_device_id universal_wm5110_of_match[] = {
	{ .compatible = "samsung,universal_wm5110", },
	{ },
};
MODULE_DEVICE_TABLE(of, universal5422_wm5110_of_match);

static struct platform_driver universal_audio_driver = {
	.driver	= {
		.name	= "universal-audio",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(universal_wm5110_of_match),
	},
	.probe	= universal_audio_probe,
	.remove	= universal_audio_remove,
};

module_platform_driver(universal_audio_driver);

MODULE_DESCRIPTION("ALSA SoC UNIVERSAL ARIZONA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:universal-audio");
