/*
 * max98505.c -- ALSA SoC MAX98505 driver
 *
 * Copyright 2013-2014 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <sound/soc.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/tlv.h>
#include <sound/tlv.h>
#include <sound/max98505a.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include "max98505a.h"

struct class *g_class;
static struct regmap *regmap_g;
static u8 spk_volume;

#define DEBUG_MAX98505A
#ifdef DEBUG_MAX98505A
#define msg_maxim(format, args...)	\
pr_info("[MAX98505A_DEBUG] %s " format, __func__, ## args)
#else
#define msg_maxim(format, args...)
#endif

static DEFINE_MUTEX(max98505_lock);

static struct reg_default max98505_reg[] = {
	{ 0x00, 0x00 }, /* Battery Voltage Data */
	{ 0x01, 0x00 }, /* Boost Voltage Data */
	{ 0x02, 0x00 }, /* Live Status0 */
	{ 0x03, 0x00 }, /* Live Status1 */
	{ 0x04, 0x00 }, /* Live Status2 */
	{ 0x05, 0x00 }, /* State0 */
	{ 0x06, 0x00 }, /* State1 */
	{ 0x07, 0x00 }, /* State2 */
	{ 0x08, 0x00 }, /* Flag0 */
	{ 0x09, 0x00 }, /* Flag1 */
	{ 0x0A, 0x00 }, /* Flag2 */
	{ 0x0B, 0x00 }, /* IRQ Enable0 */
	{ 0x0C, 0x00 }, /* IRQ Enable1 */
	{ 0x0D, 0x00 }, /* IRQ Enable2 */
	{ 0x0E, 0x00 }, /* IRQ Clear0 */
	{ 0x0F, 0x00 }, /* IRQ Clear1 */
	{ 0x10, 0x00 }, /* IRQ Clear2 */
	{ 0x11, 0xC0 }, /* Map0 */
	{ 0x12, 0x00 }, /* Map1 */
	{ 0x13, 0x00 }, /* Map2 */
	{ 0x14, 0xF0 }, /* Map3 */
	{ 0x15, 0x00 }, /* Map4 */
	{ 0x16, 0xAB }, /* Map5 */
	{ 0x17, 0x89 }, /* Map6 */
	{ 0x18, 0x00 }, /* Map7 */
	{ 0x19, 0x00 }, /* Map8 */
	{ 0x1A, 0x06 }, /* DAI Clock Mode 1 */
	{ 0x1B, 0xC0 }, /* DAI Clock Mode 2 */
	{ 0x1C, 0x00 }, /* DAI Clock Divider Denominator MSBs */
	{ 0x1D, 0x00 }, /* DAI Clock Divider Denominator LSBs */
	{ 0x1E, 0xF0 }, /* DAI Clock Divider Numerator MSBs */
	{ 0x1F, 0x00 }, /* DAI Clock Divider Numerator LSBs */
	{ 0x20, 0x50 }, /* Format */
	{ 0x21, 0x00 }, /* TDM Slot Select */
	{ 0x22, 0x00 }, /* DOUT Configuration VMON */
	{ 0x23, 0x00 }, /* DOUT Configuration IMON */
	{ 0x24, 0x00 }, /* DOUT Configuration VBAT */
	{ 0x25, 0x00 }, /* DOUT Configuration VBST */
	{ 0x26, 0x00 }, /* DOUT Configuration FLAG */
	{ 0x27, 0xFF }, /* DOUT HiZ Configuration 1 */
	{ 0x28, 0xFF }, /* DOUT HiZ Configuration 2 */
	{ 0x29, 0xFF }, /* DOUT HiZ Configuration 3 */
	{ 0x2A, 0xFF }, /* DOUT HiZ Configuration 4 */
	{ 0x2B, 0x02 }, /* DOUT Drive Strength */
	{ 0x2C, 0x90 }, /* Filters */
	{ 0x2D, 0x00 }, /* Gain */
	{ 0x2E, 0x02 }, /* Gain Ramping */
	{ 0x2F, 0x00 }, /* Speaker Amplifier */
	{ 0x30, 0x0A }, /* Threshold */
	{ 0x31, 0x00 }, /* ALC Attack */
	{ 0x32, 0x80 }, /* ALC Atten and Release */
	{ 0x33, 0x00 }, /* ALC Infinite Hold Release */
	{ 0x34, 0x92 }, /* ALC Configuration */
	{ 0x35, 0x01 }, /* Boost Converter */
	{ 0x36, 0x00 }, /* Block Enable */
	{ 0x37, 0x00 }, /* Configuration */
	{ 0x38, 0x00 }, /* Global Enable */
	{ 0x3A, 0x00 }, /* Boost Limiter */
	{ 0xFF, 0x50 }, /* Revision ID */
};

static bool max98505_volatile_register(
		struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98505_R000_VBAT_DATA:
	case MAX98505_R001_VBST_DATA:
	case MAX98505_R002_LIVE_STATUS0:
	case MAX98505_R003_LIVE_STATUS1:
	case MAX98505_R004_LIVE_STATUS2:
	case MAX98505_R005_STATE0:
	case MAX98505_R006_STATE1:
	case MAX98505_R007_STATE2:
	case MAX98505_R008_FLAG0:
	case MAX98505_R009_FLAG1:
	case MAX98505_R00A_FLAG2:
	case MAX98505_R0FF_VERSION:
		return true;
	default:
		return false;
	}
}

static bool max98505_readable_register(
		struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98505_R00E_IRQ_CLEAR0:
	case MAX98505_R00F_IRQ_CLEAR1:
	case MAX98505_R010_IRQ_CLEAR2:
	case MAX98505_R033_ALC_HOLD_RLS:
		return false;
	default:
		return true;
	}
};

#ifdef USE_DSM_LOG
#define DEFAULT_LOG_CLASS_NAME "dsm"
static const char *class_name_log = DEFAULT_LOG_CLASS_NAME;
static ssize_t max98505_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_SND_SOC_MAXIM_DSM
	return maxdsm_log_prepare(buf);
#else
	return 0;
#endif /* CONFIG_SND_SOC_MAXIM_DSM */
}

static DEVICE_ATTR(dsm_log, S_IRUGO, max98505_log_show, NULL);
static struct attribute *max98505_attributes[] = {
	&dev_attr_dsm_log.attr,
	NULL
};

static struct attribute_group max98505_attribute_group = {
	.attrs = max98505_attributes
};
#endif /* USE_DSM_LOG */

static int reg_set_optimum_mode_check(
		struct regulator *reg, int load_ua)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_ua) : 0;
}

#define VCC_I2C_MIN_UV	1800000
#define VCC_I2C_MAX_UV	1800000
#define I2C_LOAD_UA	300000

static int max98505_regulator_config(
		struct i2c_client *i2c, bool pullup, bool on)
{
	struct regulator *max98505_vcc_i2c;
	int rc;

	msg_maxim("enter\n");

	if (pullup) {
		msg_maxim("I2C PULL UP.\n");

		max98505_vcc_i2c = regulator_get(&i2c->dev, "vcc_i2c");
		if (IS_ERR(max98505_vcc_i2c)) {
			rc = PTR_ERR(max98505_vcc_i2c);
			msg_maxim("regulator get failed rc=%d\n", rc);
			goto error_get_vtg_i2c;
		}
		if (regulator_count_voltages(max98505_vcc_i2c) > 0) {
			rc = regulator_set_voltage(max98505_vcc_i2c,
					VCC_I2C_MIN_UV, VCC_I2C_MAX_UV);
			if (rc) {
				msg_maxim("regulator set_vtg failed rc=%d\n"
						, rc);
				goto error_set_vtg_i2c;
			}
		}

		rc = reg_set_optimum_mode_check(max98505_vcc_i2c, I2C_LOAD_UA);
		if (rc < 0) {
			msg_maxim("regulator vcc_i2c set_opt failed rc=%d\n"
					, rc);
			goto error_reg_opt_i2c;
		}

		rc = regulator_enable(max98505_vcc_i2c);
		if (rc) {
			msg_maxim("regulator vcc_i2c enable failed rc=%d\n"
					, rc);
			goto error_reg_en_vcc_i2c;
		}

	}

	return 0;

error_set_vtg_i2c:
	regulator_put(max98505_vcc_i2c);
error_get_vtg_i2c:
	if (regulator_count_voltages(max98505_vcc_i2c) > 0)
		regulator_set_voltage(max98505_vcc_i2c, 0, VCC_I2C_MAX_UV);
error_reg_en_vcc_i2c:
	if (pullup)
		reg_set_optimum_mode_check(max98505_vcc_i2c, 0);
error_reg_opt_i2c:
	regulator_disable(max98505_vcc_i2c);

	return rc;
}

#ifdef USE_REG_DUMP
static void reg_dump(struct regmap *max98505_regmap_l)
{
	int val_l;
	int i, j;

	static const struct {
		int start;
		int count;
	} reg_table[] = {
		{ 0x02, 0x03 },
		{ 0x1A, 0x1F },
		{ 0x3A, 0x01 },
		{ 0x00, 0x00 }
	};

	i = 0;
	while (reg_table[i].count != 0)	{
		for (j = 0; j < reg_table[i].count; j++) {
			int addr = j + reg_table[i].start;
			regmap_read(max98505_regmap_l, addr, &val_l);
			msg_maxim("reg 0x%02X, val_l 0x%02X\n",
					addr, val_l);
		}
		i++;
	}
}
#endif /* USE_REG_DUMP */

/* codec sample rate and n/m dividers parameter table */
static const struct {
	u32 rate;
	u8  sr;
	u32 divisors[3][2];
} rate_table[] = {
	{ 8000, 0, {{  1,   375} , {5, 1764} , {  1,   384} } },
	{11025, 1, {{147, 40000} , {1,	256} , {147, 40960} } },
	{12000, 2, {{  1,   250} , {5, 1176} , {  1,   256} } },
	{16000, 3, {{  2,   375} , {5,	882} , {  1,   192} } },
	{22050, 4, {{147, 20000} , {1,	128} , {147, 20480} } },
	{24000, 5, {{  1,   125} , {5,	588} , {  1,   128} } },
	{32000, 6, {{  4,   375} , {5,	441} , {  1,	96} } },
	{44100, 7, {{147, 10000} , {1,	 64} , {147, 10240} } },
	{48000, 8, {{  2,   125} , {5,	294} , {  1,	64} } },
};

static inline int max98505_rate_value(
		int rate, int clock, u8 *value, int *n, int *m)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
		if (rate_table[i].rate >= rate) {
			*value = rate_table[i].sr;
			*n = rate_table[i].divisors[clock][0];
			*m = rate_table[i].divisors[clock][1];
			ret = 0;
			break;
		}
	}

	msg_maxim("sample rate is %d, returning %d\n",
			rate_table[i].rate, *value);

	return ret;
}

static void max98505_set_slave(struct max98505_priv *max98505)
{
	msg_maxim("ENTER\n");

	/*
	 * 1. use BCLK instead of MCLK
	 */
	regmap_update_bits(max98505->regmap,
			MAX98505_R01A_DAI_CLK_MODE1,
			MAX98505_DAI_CLK_SOURCE_MASK,
			MAX98505_DAI_CLK_SOURCE_MASK);
	/*
	 * 2. set DAI to slave mode
	 */
	regmap_update_bits(max98505->regmap,
			MAX98505_R01B_DAI_CLK_MODE2,
			MAX98505_DAI_MAS_MASK,
			0);
	/*
	 * 3. set BLCKs to LRCLKs to 64
	 */
	regmap_update_bits(max98505->regmap,
			MAX98505_R01B_DAI_CLK_MODE2,
			MAX98505_DAI_BSEL_MASK,
			MAX98505_DAI_BSEL_32);
	/*
	 * 4. set VMON slots
	 */
	regmap_update_bits(max98505->regmap,
			MAX98505_R022_DOUT_CFG_VMON,
			MAX98505_DAI_VMON_EN_MASK,
			MAX98505_DAI_VMON_EN_MASK);
	regmap_update_bits(max98505->regmap,
			MAX98505_R022_DOUT_CFG_VMON,
			MAX98505_DAI_VMON_SLOT_MASK,
			MAX98505_DAI_VMON_SLOT_00_01);
	/*
	 * 5. set IMON slots
	 */
	regmap_update_bits(max98505->regmap,
			MAX98505_R023_DOUT_CFG_IMON,
			MAX98505_DAI_IMON_EN_MASK,
			MAX98505_DAI_IMON_EN_MASK);
	regmap_update_bits(max98505->regmap,
			MAX98505_R023_DOUT_CFG_IMON,
			MAX98505_DAI_IMON_SLOT_MASK,
			MAX98505_DAI_IMON_SLOT_02_03);
}

static void max98505_set_master(struct max98505_priv *max98505)
{
	msg_maxim("ENTER\n");

	/*
	 * 1. use MCLK for Left channel, right channel always BCLK
	 */
	regmap_update_bits(max98505->regmap, MAX98505_R01A_DAI_CLK_MODE1,
			MAX98505_DAI_CLK_SOURCE_MASK, 0);
	/*
	 * 2. set left channel DAI to master mode, right channel always slave
	 */
	regmap_update_bits(max98505->regmap, MAX98505_R01B_DAI_CLK_MODE2,
			MAX98505_DAI_MAS_MASK, MAX98505_DAI_MAS_MASK);
}

static int max98505_set_clock(struct max98505_priv *max98505)
{
	struct max98505_pdata *pdata = max98505->pdata;
	struct max98505_format_data *fmt_data = &pdata->format_data;

	unsigned int clock;
	unsigned int mdll;
	unsigned int n;
	unsigned int m;
	u8 dai_sr;

	switch (pdata->sysclk) {
	case 6000000:
		clock = 0;
		mdll  = MAX98505_MDLL_MULT_MCLKx16;
		break;
	case 11289600:
		clock = 1;
		mdll  = MAX98505_MDLL_MULT_MCLKx8;
		break;
	case 12000000:
		clock = 0;
		mdll  = MAX98505_MDLL_MULT_MCLKx8;
		break;
	case 12288000:
		clock = 2;
		mdll  = MAX98505_MDLL_MULT_MCLKx8;
		break;
	default:
		msg_maxim("unsupported sysclk %d\n",
				pdata->sysclk);
		return -EINVAL;
	}

	if (max98505_rate_value(fmt_data->pcm_samplerate,
				clock, &dai_sr, &n, &m))
		return -EINVAL;

	/*
	 * 1. set DAI_SR to correct LRCLK frequency
	 */
	regmap_update_bits(max98505->regmap, MAX98505_R01B_DAI_CLK_MODE2,
			MAX98505_DAI_SR_MASK, dai_sr << MAX98505_DAI_SR_SHIFT);
	/*
	 * 2. set DAI m divider
	 */
	regmap_write(max98505->regmap, MAX98505_R01C_DAI_CLK_DIV_M_MSBS,
			m >> 8);
	regmap_write(max98505->regmap, MAX98505_R01D_DAI_CLK_DIV_M_LSBS,
			m & 0xFF);
	/*
	 * 3. set DAI n divider
	 */
	regmap_write(max98505->regmap, MAX98505_R01E_DAI_CLK_DIV_N_MSBS,
			n >> 8);
	regmap_write(max98505->regmap, MAX98505_R01F_DAI_CLK_DIV_N_LSBS,
			n & 0xFF);
	/*
	 * 4. set MDLL
	 */
	regmap_update_bits(max98505->regmap,
			MAX98505_R01A_DAI_CLK_MODE1,
			MAX98505_MDLL_MULT_MASK,
			mdll << MAX98505_MDLL_MULT_SHIFT);

	return 0;
}

static int max98505_dai_set_fmt(struct max98505_priv *max98505)
{
	struct max98505_pdata *pdata = max98505->pdata;
	struct max98505_format_data *fmt_data = &pdata->format_data;

	unsigned int invert = 0;

	msg_maxim("\n");

	switch (pdata->slave_master_mode) {
	case MODE_SLAVE:
		max98505_set_slave(max98505);
		break;
	case MODE_MASTER:
		max98505_set_master(max98505);
		break;
	default:
		pr_err("DAI clock mode (%d) unsupported",
				pdata->slave_master_mode);
		return -EINVAL;
	}

	switch (fmt_data->dai_format) {
	case DAIFMT_I2S:
		msg_maxim("set DAIFMT_I2S\n");
		break;
	case DAIFMT_LEFT_J:
		msg_maxim("set DAIFMT_LEFT_J\n");
		break;
	case DAIFMT_DSP_A:
		msg_maxim("set DAIFMT_DSP_A\n");
	default:
		pr_err("DAI format unsupported, fmt : %d",
				fmt_data->dai_format);
		return -EINVAL;
	}

	switch (fmt_data->dai_inv_format) {
	case DAIFMT_NB_NF:
		break;
	case DAIFMT_NB_IF:
		invert = MAX98505_DAI_WCI_MASK;
		break;
	case DAIFMT_IB_NF:
		invert = MAX98505_DAI_BCI_MASK;
		break;
	case DAIFMT_IB_IF:
		invert = MAX98505_DAI_BCI_MASK | MAX98505_DAI_WCI_MASK;
		break;
	default:
		pr_err("DAI invert mode (%d) unsupported",
				fmt_data->dai_inv_format);
		return -EINVAL;
	}

	regmap_update_bits(max98505->regmap,
			MAX98505_R020_FORMAT,
			MAX98505_DAI_BCI_MASK | MAX98505_DAI_WCI_MASK,
			invert);

	return 0;

}

static int max98505_dai_hw_params(struct max98505_priv *max98505)
{
	struct max98505_pdata *pdata = max98505->pdata;
	struct max98505_format_data *fmt_data = &pdata->format_data;

	msg_maxim("enter\n");

	switch (fmt_data->pcm_format) {
	case PCM_FORMAT_S16_LE:
		msg_maxim("set SNDRV_PCM_FORMAT_S16_LE\n");
		regmap_update_bits(max98505->regmap,
				MAX98505_R020_FORMAT,
				MAX98505_DAI_CHANSZ_MASK,
				MAX98505_DAI_CHANSZ_16);
		break;
	case PCM_FORMAT_S24_LE:
		msg_maxim("set SNDRV_PCM_FORMAT_S24_LE\n");
#ifdef RIVER
		regmap_update_bits(max98505->regmap,
				MAX98505_R020_FORMAT,
				MAX98505_DAI_CHANSZ_MASK,
				MAX98505_DAI_CHANSZ_32);
		msg_maxim("(really set to 32 bits)\n");
#else
		regmap_update_bits(max98505->regmap,
				MAX98505_R020_FORMAT,
				MAX98505_DAI_CHANSZ_MASK,
				MAX98505_DAI_CHANSZ_24);
#endif /* RIVER */
		break;
	case PCM_FORMAT_S32_LE:
		msg_maxim("set SNDRV_PCM_FORMAT_S32_LE\n");
		regmap_update_bits(max98505->regmap,
				MAX98505_R020_FORMAT,
				MAX98505_DAI_CHANSZ_MASK,
				MAX98505_DAI_CHANSZ_32);
		break;
	default:
		msg_maxim("format unsupported %d",
				fmt_data->pcm_format);
		return -EINVAL;
	}

	return max98505_set_clock(max98505);
}

static int max98505_probe(struct max98505_priv *max98505)
{
	struct max98505_pdata *pdata = max98505->pdata;
	struct max98505_cfg_data *cfg_data = &pdata->cfg_data;
	struct max98505_format_data *fmt_data = &pdata->format_data;

	int ret = 0;
	int reg = 0;

	msg_maxim("build number %s\n", MAX98505_REVISION);

	reg = 0;
	ret = regmap_read(max98505->regmap, MAX98505_R0FF_VERSION, &reg);
	if ((ret < 0) || ((reg != MAX98505_VERSION)
				&& (reg != MAX98505_VERSION1)
				&& (reg != MAX98505_VERSION2))) {
		pr_err("Failed to read device revision: %d\n", ret);
		goto err_access;
	}

	msg_maxim("device version 0x%02X\n", reg);

	if (!pdata) {
		pr_err("No platform data\n");
		return ret;
	}

	spk_volume = (u8)pdata->spk_volume;

	regmap_write(max98505->regmap, MAX98505_R038_GLOBAL_ENABLE, 0x00);

	/* It's not the default but we need to set DAI_DLY */
	regmap_write(max98505->regmap,
			MAX98505_R020_FORMAT,
			(u8)fmt_data->dai_data_dly);
	regmap_write(max98505->regmap,
			MAX98505_R021_TDM_SLOT_SELECT,
			(u8)pdata->tdm_slot_select);
	regmap_write(max98505->regmap,
			MAX98505_R027_DOUT_HIZ_CFG1,
			(u8)cfg_data->dout_hiz_cfg1);
	regmap_write(max98505->regmap,
			MAX98505_R028_DOUT_HIZ_CFG2,
			(u8)cfg_data->dout_hiz_cfg2);
	regmap_write(max98505->regmap,
			MAX98505_R029_DOUT_HIZ_CFG3,
			(u8)cfg_data->dout_hiz_cfg3);
	regmap_write(max98505->regmap,
			MAX98505_R02A_DOUT_HIZ_CFG4,
			(u8)cfg_data->dout_hiz_cfg4);
	regmap_write(max98505->regmap,
			MAX98505_R02C_FILTERS,
			(u8)cfg_data->filters);
	regmap_write(max98505->regmap,
			MAX98505_R034_ALC_CONFIGURATION,
			(u8)cfg_data->alc_cfg);

	/* Set boost output to maximum */
	regmap_write(max98505->regmap,
			MAX98505_R037_CONFIGURATION, 0x00);

	/* Disable ALC muting */
	regmap_write(max98505->regmap,
			MAX98505_R03A_BOOST_LIMITER,
			(u8)pdata->boost_current_limit);

	regmap_update_bits(max98505->regmap, MAX98505_R02D_GAIN,
			MAX98505_DAC_IN_SEL_MASK, (u8)pdata->dac_input_sel);

	ret = max98505_dai_set_fmt(max98505);
	if (ret) {
		pr_err("%s : failed dai fmt !!!", __func__);
		return ret;
	}
	ret = max98505_dai_hw_params(max98505);
	if (ret) {
		pr_err("%s : failed hw params !!!", __func__);
		return ret;
	}

err_access:
	msg_maxim("exit %d\n", ret);
	return ret;
}

int max98505_set_speaker_status(int on)
{
	msg_maxim("on=%d\n", on);

	if (regmap_g == NULL) {
		pr_err("Speaker control is not available.\n");
		return 0;
	}

	mutex_lock(&max98505_lock);
	if (on) {
		regmap_update_bits(regmap_g, MAX98505_R02D_GAIN,
				MAX98505_SPK_GAIN_MASK, spk_volume);

		regmap_update_bits(regmap_g,
				MAX98505_R036_BLOCK_ENABLE,
				MAX98505_BST_EN_MASK |
				MAX98505_SPK_EN_MASK |
				MAX98505_ADC_IMON_EN_MASK |
				MAX98505_ADC_VMON_EN_MASK,
				MAX98505_BST_EN_MASK |
				MAX98505_SPK_EN_MASK |
				MAX98505_ADC_IMON_EN_MASK |
				MAX98505_ADC_VMON_EN_MASK);
		regmap_write(regmap_g, MAX98505_R038_GLOBAL_ENABLE,
				MAX98505_EN_MASK);
	} else {
		regmap_update_bits(regmap_g, MAX98505_R02D_GAIN,
				MAX98505_SPK_GAIN_MASK, 0x00);

		usleep_range(4999, 5000);

		regmap_update_bits(regmap_g, MAX98505_R038_GLOBAL_ENABLE,
				MAX98505_EN_MASK, 0x0);
	}
#ifdef USE_REG_DUMP
	reg_dump(regmap_g);
#endif /* USE_REG_DUMP */
	mutex_unlock(&max98505_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(max98505_set_speaker_status);

bool max98505_get_speaker_status(void)
{
	unsigned int ret;

	if (regmap_g == NULL) {
		pr_err("Speaker control is not allowed.\n");
		return 0;
	}

	mutex_lock(&max98505_lock);
	regmap_read(regmap_g, MAX98505_R038_GLOBAL_ENABLE, &ret);
	mutex_unlock(&max98505_lock);

	return (ret > 0) ? true : false;
}
EXPORT_SYMBOL_GPL(max98505_get_speaker_status);

static const struct regmap_config max98505_regmap = {
	.reg_bits         = 8,
	.val_bits         = 8,
	.max_register     = MAX98505_R0FF_VERSION,
	.reg_defaults     = max98505_reg,
	.num_reg_defaults = ARRAY_SIZE(max98505_reg),
	.volatile_reg     = max98505_volatile_register,
	.readable_reg     = max98505_readable_register,
	.cache_type       = REGCACHE_RBTREE,
};

#ifdef USE_MAX98505_IRQ
static irqreturn_t max98505_interrupt(int irq, void *data)
{
	struct max98505_priv *max98505 = (struct max98505_priv *) data;

	unsigned int mask0;
	unsigned int mask1;
	unsigned int mask2;
	unsigned int flag0;
	unsigned int flag1;
	unsigned int flag2;

	regmap_read(max98505->regmap, MAX98505_R00B_IRQ_ENABLE0, &mask0);
	regmap_read(max98505->regmap, MAX98505_R008_FLAG0, &flag0);

	regmap_read(max98505->regmap, MAX98505_R00C_IRQ_ENABLE1, &mask1);
	regmap_read(max98505->regmap, MAX98505_R009_FLAG1, &flag1);

	regmap_read(max98505->regmap, MAX98505_R00D_IRQ_ENABLE2, &mask2);
	regmap_read(max98505->regmap, MAX98505_R00A_FLAG2, &flag2);

	flag0 &= mask0;
	flag1 &= mask1;
	flag2 &= mask2;

	if (!flag0 && !flag1 && !flag2)
		return IRQ_NONE;

	/* Send work to be scheduled */
	if (flag0 & MAX98505_THERMWARN_END_STATE_MASK)
		msg_maxim("MAX98505_THERMWARN_STATUS_MASK active!\n");

	if (flag0 & MAX98505_THERMWARN_BGN_STATE_MASK)
		msg_maxim("MAX98505_THERMWARN_BGN_STATE_MASK active!\n");

	if (flag0 & MAX98505_THERMSHDN_END_STATE_MASK)
		msg_maxim("MAX98505_THERMSHDN_END_STATE_MASK active!\n");

	if (flag0 & MAX98505_THERMSHDN_BGN_STATE_MASK)
		msg_maxim("MAX98505_THERMSHDN_BGN_STATE_MASK active!\n");

	if (flag1 & MAX98505_SPRCURNT_STATE_MASK)
		msg_maxim("MAX98505_SPRCURNT_STATE_MASK active!\n");

	if (flag1 & MAX98505_WATCHFAIL_STATE_MASK)
		msg_maxim("MAX98505_WATCHFAIL_STATE_MASK active!\n");

	if (flag1 & MAX98505_ALCINFH_STATE_MASK)
		msg_maxim("MAX98505_ALCINFH_STATE_MASK active!\n");

	if (flag1 & MAX98505_ALCACT_STATE_MASK)
		msg_maxim("MAX98505_ALCACT_STATE_MASK active!\n");

	if (flag1 & MAX98505_ALCMUT_STATE_MASK)
		msg_maxim("MAX98505_ALCMUT_STATE_MASK active!\n");

	if (flag1 & MAX98505_ALCP_STATE_MASK)
		msg_maxim("MAX98505_ALCP_STATE_MASK active!\n");

	if (flag2 & MAX98505_SLOTOVRN_STATE_MASK)
		msg_maxim("MAX98505_SLOTOVRN_STATE_MASK active!\n");

	if (flag2 & MAX98505_INVALSLOT_STATE_MASK)
		msg_maxim("MAX98505_INVALSLOT_STATE_MASK active!\n");

	if (flag2 & MAX98505_SLOTCNFLT_STATE_MASK)
		msg_maxim("MAX98505_SLOTCNFLT_STATE_MASK active!\n");

	if (flag2 & MAX98505_VBSTOVFL_STATE_MASK)
		msg_maxim("MAX98505_VBSTOVFL_STATE_MASK active!\n");

	if (flag2 & MAX98505_VBATOVFL_STATE_MASK)
		msg_maxim("MAX98505_VBATOVFL_STATE_MASK active!\n");

	if (flag2 & MAX98505_IMONOVFL_STATE_MASK)
		msg_maxim("MAX98505_IMONOVFL_STATE_MASK active!\n");

	if (flag2 & MAX98505_VMONOVFL_STATE_MASK)
		msg_maxim("MAX98505_VMONOVFL_STATE_MASK active!\n");

	regmap_write(max98505->regmap, MAX98505_R00E_IRQ_CLEAR0,
			flag0&0xff);
	regmap_write(max98505->regmap, MAX98505_R00F_IRQ_CLEAR1,
			flag1&0xff);
	regmap_write(max98505->regmap, MAX98505_R010_IRQ_CLEAR2,
			flag2&0xff);

	return IRQ_HANDLED;
}
#endif /* USE_MAX98505_IRQ */

static int max98505_i2c_probe(struct i2c_client *i2c_l,
		const struct i2c_device_id *id)
{
	struct max98505_priv *max98505;
	struct max98505_pdata *pdata = NULL;

	int ret;

	msg_maxim("enter, device '%s'\n", id->name);

	max98505 = kzalloc(sizeof(struct max98505_priv), GFP_KERNEL);
	if (max98505 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c_l, max98505);

	if (i2c_l->dev.of_node) {
		max98505->pdata = devm_kzalloc(&i2c_l->dev,
				sizeof(struct max98505_pdata), GFP_KERNEL);
		if (!max98505->pdata) {
			dev_err(&i2c_l->dev, "Failed to allocate memory\n");
			kfree(max98505);
			return -ENOMEM;
		} else
			pdata = max98505->pdata;

#ifdef USE_MAX98505_IRQ
		pdata->irq = of_get_named_gpio_flags(i2c_l->dev.of_node,
				"max98505,irq-gpio", 0, NULL);
#endif /* USE_MAX98505_IRQ */

		ret = of_property_read_u32(i2c_l->dev.of_node,
				"max98505,tdm_slot_select",
				&pdata->tdm_slot_select);
		if (ret) {
			dev_err(&i2c_l->dev,
					"There is no tdm_slot_select.\n");
			devm_kfree(&i2c_l->dev, max98505->pdata);
			kfree(max98505);
			return -EINVAL;
		}

		ret = of_property_read_u32(i2c_l->dev.of_node,
				"max98505,boost_current_limit",
				&pdata->boost_current_limit);
		if (ret) {
			dev_err(&i2c_l->dev,
					"There is no boost_current_limit.\n");
			devm_kfree(&i2c_l->dev, max98505->pdata);
			kfree(max98505);
			return -EINVAL;
		}

		ret = of_property_read_u32(i2c_l->dev.of_node,
				"max98505,dac_input_sel",
				&pdata->dac_input_sel);
		if (ret) {
			dev_err(&i2c_l->dev, "There is no dac_input_sel.\n");
			devm_kfree(&i2c_l->dev, max98505->pdata);
			kfree(max98505);
			return -EINVAL;
		}

		ret = of_property_read_u32(i2c_l->dev.of_node,
				"max98505,slave_master_mode",
				&pdata->slave_master_mode);
		if (ret) {
			dev_err(&i2c_l->dev,
					"There is no slave_master_mode.\n");
			devm_kfree(&i2c_l->dev, max98505->pdata);
			kfree(max98505);
			return -EINVAL;
		}

		ret = of_property_read_u32(i2c_l->dev.of_node,
				"max98505,spk_volume", &pdata->spk_volume);
		if (ret) {
			dev_err(&i2c_l->dev, "There is no spk_volume.\n");
			devm_kfree(&i2c_l->dev, max98505->pdata);
			kfree(max98505);
			return -EINVAL;
		}

		ret = of_property_read_u32(i2c_l->dev.of_node,
				"max98505,sysclk", &pdata->sysclk);
		if (ret) {
			dev_err(&i2c_l->dev, "There is no sysclk.\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_array(i2c_l->dev.of_node,
				"max98505,format_data",
				(u32 *)&pdata->format_data,
				sizeof(struct max98505_format_data)
					/ sizeof(u32));
		if (ret) {
			dev_err(&i2c_l->dev, "There is no format_data.\n");
			devm_kfree(&i2c_l->dev, max98505->pdata);
			kfree(max98505);
			return -EINVAL;
		}

		ret = of_property_read_u32_array(i2c_l->dev.of_node,
				"max98505,cfg_data", (u32 *) &pdata->cfg_data,
				sizeof(struct max98505_cfg_data)/sizeof(u32));
		if (ret) {
			dev_err(&i2c_l->dev, "There is no cfg_data.\n");
			devm_kfree(&i2c_l->dev, max98505->pdata);
			kfree(max98505);
			return -EINVAL;
		}

#ifdef USE_DSM_LOG
		ret = of_property_read_string(i2c_l->dev.of_node,
				"max98505,log_class", &class_name_log);
		if (ret) {
			dev_err(&i2c_l->dev, "There is no log_class.\n");
			class_name_log = DEFAULT_LOG_CLASS_NAME;
		}
#endif /* USE_DSM_LOG */
	} else {
		max98505->pdata = i2c_l->dev.platform_data;
		pdata = max98505->pdata;
	}

	max98505_regulator_config(i2c_l,
			of_property_read_bool(i2c_l->dev.of_node,
				"max98505,i2c-pull-up"), 1);

	max98505->regmap = regmap_g = regmap_init_i2c(i2c_l, &max98505_regmap);
	if (IS_ERR(max98505->regmap)) {
		ret = PTR_ERR(max98505->regmap);
		dev_err(&i2c_l->dev, "Failed to allocate regmap: %d\n", ret);
		goto err_out;
	}

	max98505_probe(max98505);

#ifdef USE_DSM_LOG
	if (!g_class)
		g_class = class_create(THIS_MODULE, class_name_log);
	max98505->dev_log_class = g_class;
	if (max98505->dev_log_class) {
		max98505->dev_log =
			device_create(max98505->dev_log_class,
					NULL, 1, NULL, "max98505");
		if (IS_ERR(max98505->dev_log)) {
			ret = sysfs_create_group(&i2c_l->dev.kobj,
					&max98505_attribute_group);
			if (ret)
				msg_maxim("failed to create sysfs group [%d]\n",
						ret);
		} else {
			ret = sysfs_create_group(&max98505->dev_log->kobj,
					&max98505_attribute_group);
			if (ret)
				msg_maxim("failed to create sysfs group [%d]\n",
						ret);
		}
	}
	msg_maxim("g_class=%p %p\n", g_class, max98505->dev_log_class);
#endif /* USE_DSM_LOG */

#ifdef CONFIG_SND_SOC_MAXIM_DSM
	maxdsm_init();
#endif /* CONFIG_SND_SOC_MAXIM_DSM */

#ifdef USE_MAX98505_IRQ
	if (pdata != NULL && gpio_is_valid(pdata->irq)) {
		ret = gpio_request(pdata->irq, "max98505_irq_gpio");
		if (ret) {
			dev_err(&i2c_l->dev, "unable to request gpio [%d]\n",
					pdata->irq);
			goto err_irq_gpio_req;
		}
		ret = gpio_direction_input(pdata->irq);
		if (ret) {
			dev_err(&i2c_l->dev,
					"unable to set direction for gpio [%d]\n",
					pdata->irq);
			goto err_irq_gpio_req;
		}
		i2c_l->irq = gpio_to_irq(pdata->irq);

		ret = request_threaded_irq(i2c_l->irq, NULL, max98505_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"max98505_interrupt", max98505);
		if (ret)
			dev_err(&i2c_l->dev, "Failed to register interrupt\n");

	} else {
		dev_err(&i2c_l->dev, "irq gpio not provided\n");
	}
	return 0;

err_irq_gpio_req:
	if (gpio_is_valid(pdata->irq))
		gpio_free(pdata->irq);
	devm_kfree(&i2c_l->dev, max98505->pdata);
	kfree(max98505);
	return 0;
#endif /* USE_MAX98505_IRQ */

err_out:

	if (ret < 0) {
		if (max98505->regmap)
			regmap_exit(max98505->regmap);
		devm_kfree(&i2c_l->dev, max98505->pdata);
		kfree(max98505);
	}

	msg_maxim("ret %d\n", ret);

	return ret;
}

static int max98505_i2c_remove(struct i2c_client *client)
{
	struct max98505_priv *max98505 = dev_get_drvdata(&client->dev);

	if (max98505->regmap)
		regmap_exit(max98505->regmap);

	if (gpio_is_valid(max98505->pdata->irq))
		gpio_free(max98505->pdata->irq);

	msg_maxim("exit\n");

#ifdef USE_DSM_LOG
	sysfs_remove_group(&max98505->dev_log->kobj,
			&max98505_attribute_group);
#endif /* USE_DSM_LOG */
#ifdef CONFIG_SND_SOC_MAXIM_DSM
	maxdsm_deinit();
#endif /* CONFIG_SND_SOC_MAXIM_DSM */

	kfree(i2c_get_clientdata(client));

	return 0;
}

static const struct i2c_device_id max98505_i2c_id[] = {
	{ "max98505", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max98505_i2c_id);

static struct i2c_driver max98505_i2c_driver = {
	.driver = {
		.name = "max98505",
		.owner = THIS_MODULE,
	},
	.probe = max98505_i2c_probe,
	.remove = __devexit_p(max98505_i2c_remove),
	.id_table = max98505_i2c_id,
};

static int __init max98505_init(void)
{
	return i2c_add_driver(&max98505_i2c_driver);
}
module_init(max98505_init);

static void __exit max98505_exit(void)
{
	i2c_del_driver(&max98505_i2c_driver);
}
module_exit(max98505_exit);

MODULE_DESCRIPTION("SoC MAX98505 driver");
MODULE_AUTHOR("George Song <george.song@maximintegrated.com>");
MODULE_LICENSE("GPL");
