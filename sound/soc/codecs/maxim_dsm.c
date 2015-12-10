#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/maxim_dsm.h>
#if defined(CONFIG_SND_SOC_QDSP6V2) || defined(CONFIG_SND_SOC_QDSP6)
#include <sound/q6afe-v2.h>
#endif /* CONFIG_SND_SOC_QDSP6V2 || CONFIG_SND_SOC_QDSP6 */

#define DEBUG_MAXIM_DSM
#ifdef DEBUG_MAXIM_DSM
#define dbg_maxdsm(format, args...)	\
pr_info("[MAXIM_DSM] %s: " format "\n", __func__, ## args)
#else
#define dbg_maxdsm(format, args...)
#endif /* DEBUG_MAX98505 */

static struct maxim_dsm maxdsm = {
	.regmap = NULL,
	.platform_type = 0,
	.port_id = DSM_RX_PORT_ID,
	.rx_mod_id = AFE_PARAM_ID_ENABLE_DSM_RX,
	.tx_mod_id = AFE_PARAM_ID_ENABLE_DSM_TX,
	.filter_set = DSM_ID_FILTER_GET_AFE_PARAMS,
	.version = VERSION_3_5,
	.registered = 0,
};

static struct param_set_data maxdsm_saved_params[] = {
	{
		.name = PARAM_FEATURE_SET,
		.value = 0x1F,
	},
};

static DEFINE_MUTEX(dsm_lock);

static struct param_lsi_info g_pbi[PARAM_LSI_DSM_3_5_MAX] = {
	{
		.id = PARAM_LSI_VOICE_COIL_TEMP,
		.addr = 0x2A004C,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_EXCURSION,
		.addr = 0x2A004E,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RDC,
		.addr = 0x2A0050,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_Q_LO,
		.addr = 0x2A0052,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_Q_HI,
		.addr = 0x2A0054,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_FRES_LO,
		.addr = 0x2A0056,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_FRES_HI,
		.addr = 0x2A0058,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_EXCUR_LIMIT,
		.addr = 0x2A005A,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_VOICE_COIL,
		.addr = 0x2A005C,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_THERMAL_LIMIT,
		.addr = 0x2A005E,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RELEASE_TIME,
		.addr = 0x2A0060,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_ONOFF,
		.addr = 0x2A0062,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_STATIC_GAIN,
		.addr = 0x2A0064,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_LFX_GAIN,
		.addr = 0x2A0066,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_PILOT_GAIN,
		.addr = 0x2A0068,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_FEATURE_SET,
		.addr = 0x2A006A,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_SMOOTH_VOLT,
		.addr = 0x2A006C,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_HPF_CUTOFF,
		.addr = 0x2A006E,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_LEAD_R,
		.addr = 0x2A0070,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RMS_SMOO_FAC,
		.addr = 0x2A0072,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_CLIP_LIMIT,
		.addr = 0x2A0074,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_THERMAL_COEF,
		.addr = 0x2A0076,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_QSPK,
		.addr = 0x2A0078,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_EXCUR_LOG_THRESH,
		.addr = 0x2A007A,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_TEMP_LOG_THRESH,
		.addr = 0x2A007C,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RES_FREQ,
		.addr = 0x2A007E,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RES_FREQ_GUARD_BAND,
		.addr = 0x2A0080,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_AMBIENT_TEMP,
		.addr = 0x2A0182,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_ADMITTANCE_A1,
		.addr = 0x2A0184,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_ADMITTANCE_A2,
		.addr = 0x2A0186,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_ADMITTANCE_B0,
		.addr = 0x2A0188,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_ADMITTANCE_B1,
		.addr = 0x2A018A,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_ADMITTANCE_B2,
		.addr = 0x2A018C,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RTH1_HI,
		.addr = 0x2A018E,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RTH2_HI,
		.addr = 0x2A0190,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RTH1_LO,
		.addr = 0x2A0192,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RTH2_LO,
		.addr = 0x2A0194,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_STL_ATENGAIN_HI,
		.addr = 0x2A0196,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_STL_ATENGAIN_LO,
		.addr = 0x2A0198,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_SPT_RAMP_DOWN_FRAMES,
		.addr = 0x2A019A,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_SPT_THRESHOLD,
		.addr = 0x2A019C,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_T_HORIZON,
		.addr = 0x2A019E,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_LFX_ADMITTANCE_A1,
		.addr = 0x2A01A0,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_LFX_ADMITTANCE_A2,
		.addr = 0x2A01A2,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_LFX_ADMITTANCE_B0,
		.addr = 0x2A01A4,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_LFX_ADMITTANCE_B1,
		.addr = 0x2A01A6,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_LFX_ADMITTANCE_B2,
		.addr = 0x2A01A8,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = PARAM_LSI_RESERVE0,
		.addr = 0x2A01AA,
		.size = 2,
	},
	{
		.id = PARAM_LSI_RESERVE1,
		.addr = 0x2A01AC,
		.size = 2,
	},
	{
		.id = PARAM_LSI_RESERVE2,
		.addr = 0x2A01AE,
		.size = 2,
	},
	{
		.id = PARAM_LSI_RESERVE3,
		.addr = 0x2A01B0,
		.size = 2,
	},
	{
		.id = PARAM_LSI_RESERVE4,
		.addr = 0x2A01B2,
		.size = 2,
	},
};

#ifdef USE_DSM_LOG
static DEFINE_MUTEX(maxdsm_log_lock);

static uint32_t ex_seq_count_temp;
static uint32_t ex_seq_count_excur;
static uint32_t new_log_avail;

static int maxdsm_log_present;
static struct tm maxdsm_log_timestamp;
static uint8_t maxdsm_byte_log_array[BEFORE_BUFSIZE];
static uint32_t maxdsm_int_log_array[BEFORE_BUFSIZE];
static uint8_t maxdsm_after_prob_byte_log_array[AFTER_BUFSIZE];
static uint32_t maxdsm_after_prob_int_log_array[AFTER_BUFSIZE];

static struct param_lsi_info g_lbi[MAX_LOG_BUFFER_POS] = {
	{
		.id = WRITE_PROTECT,
		.addr = 0x2A0082,
		.size = 2,
	},
	{
		.id = LOG_AVAILABLE,
		.addr = 0x2A0084,
		.size = 2,
		.type = sizeof(uint8_t),
	},
	{
		.id = VERSION_INFO,
		.addr = 0x2A0086,
		.size = 2,
		.type = sizeof(uint8_t),
	},
	{
		.id = LAST_2_SEC_TEMP,
		.addr = 0x2A0088,
		.size = 20,
		.type = sizeof(uint8_t),
	},
	{
		.id = LAST_2_SEC_EXCUR,
		.addr = 0x2A009C,
		.size = 20,
		.type = sizeof(uint8_t),
	},
	{
		.id = RESERVED_1,
		.addr = 0x2A00B0,
		.size = 2,
	},
	{
		.id = SEQUENCE_OF_TEMP,
		.addr = 0x2A00B2,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = SEQUENCE_OF_EXCUR,
		.addr = 0x2A00B4,
		.size = 2,
		.type = sizeof(uint32_t),
	},
	{
		.id = LAST_2_SEC_RDC,
		.addr = 0x2A00B6,
		.size = 20,
		.type = sizeof(uint32_t),
	},
	{
		.id = LAST_2_SEC_FREQ,
		.addr = 0x2A00CA,
		.size = 20,
		.type = sizeof(uint32_t),
	},
	{
		.id = RESERVED_2,
		.addr = 0x2A00DE,
		.size = 2,
	},
	{
		.id = RESERVED_3,
		.addr = 0x2A00E0,
		.size = 2,
	},
	{
		.id = AFTER_2_SEC_TEMP_TEMP,
		.addr = 0x2A00E2,
		.size = 20,
		.type = sizeof(uint8_t) * 2,
	},
	{
		.id = AFTER_2_SEC_EXCUR_TEMP,
		.addr = 0x2A00F6,
		.size = 20,
		.type = sizeof(uint8_t) * 2,
	},
	{
		.id = AFTER_2_SEC_TEMP_EXCUR,
		.addr = 0x2A010A,
		.size = 20,
		.type = sizeof(uint8_t) * 2,
	},
	{
		.id = AFTER_2_SEC_EXCUR_EXCUR,
		.addr = 0x2A011E,
		.size = 20,
		.type = sizeof(uint8_t) * 2,
	},
	{
		.id = AFTER_2_SEC_RDC_TEMP,
		.addr = 0x2A0132,
		.size = 20,
		.type = sizeof(uint32_t) * 2,
	},
	{
		.id = AFTER_2_SEC_FREQ_TEMP,
		.addr = 0x2A0146,
		.size = 20,
		.type = sizeof(uint32_t) * 2,
	},
	{
		.id = AFTER_2_SEC_RDC_EXCUR,
		.addr = 0x2A015A,
		.size = 20,
		.type = sizeof(uint32_t) * 2,
	},
	{
		.id = AFTER_2_SEC_FREQ_EXCUR,
		.addr = 0x2A016E,
		.size = 20,
		.type = sizeof(uint32_t) * 2,
	},
};
#endif /* USE_DSM_LOG */

#if !defined(CONFIG_SND_SOC_QDSP6V2) && !defined(CONFIG_SND_SOC_QDSP6)
static inline int32_t dsm_open(void *data)
{
	return 0;
}
#endif /* !CONFIG_SND_SOC_QDSP6V2 && !CONFIG_SND_SOC_QDSP6 */

void maxdsm_set_regmap(struct regmap *regmap)
{
	maxdsm.regmap = regmap;
	dbg_maxdsm("Regmap for maxdsm was set by 0x%p",
			maxdsm.regmap);
}
EXPORT_SYMBOL_GPL(maxdsm_set_regmap);

static int maxdsm_regmap_read(unsigned int reg,
		unsigned int *val)
{
	return maxdsm.regmap ?
		regmap_read(maxdsm.regmap, reg, val) : -ENXIO;
}

static int maxdsm_regmap_write(unsigned int reg,
		unsigned int val)
{
	return maxdsm.regmap ?
		regmap_write(maxdsm.regmap, reg, val) : -ENXIO;
}

static void maxdsm_read_all_reg(void)
{
	int idx = 0;
	uint32_t data;

	memset(maxdsm.param, 0x00,
			sizeof(int) * maxdsm.param_size);
	while (idx++ < PARAM_LSI_DSM_3_5_MAX) {
		if (!g_pbi[idx-1].type)
			continue;
		maxdsm_regmap_read(g_pbi[idx-1].addr, &data);
		maxdsm.param[idx-1] = data;
		dbg_maxdsm("maxdsm.param[%d]=0x%x", idx - 1,
				maxdsm.param[idx - 1]);
	}
}

#ifdef USE_DSM_LOG
void maxdsm_log_update(const void *byte_log_array,
		const void *int_log_array,
		const void *after_prob_byte_log_array,
		const void *after_prob_int_log_array)
{
	struct timeval tv;
	mutex_lock(&maxdsm_log_lock);

	memcpy(maxdsm_byte_log_array,
			byte_log_array, sizeof(maxdsm_byte_log_array));
	memcpy(maxdsm_int_log_array,
			int_log_array, sizeof(maxdsm_int_log_array));

	memcpy(maxdsm_after_prob_byte_log_array,
			after_prob_byte_log_array,
			sizeof(maxdsm_after_prob_byte_log_array));
	memcpy(maxdsm_after_prob_int_log_array,
			after_prob_int_log_array,
			sizeof(maxdsm_after_prob_int_log_array));

	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, 0, &maxdsm_log_timestamp);

	maxdsm_log_present = 1;

	mutex_unlock(&maxdsm_log_lock);
}

static void maxdsm_read_logbuf_reg(void)
{
	int idx;
	int b_idx, i_idx;
	int apb_idx, api_idx;
	int loop;
	uint32_t data;
	struct timeval tv;

	if (maxdsm.platform_type)
		return;

	mutex_lock(&maxdsm_log_lock);

	/* If the following variables are not initialized,
	* these can not have zero data on some linux platform.
	*/
	idx = b_idx = i_idx = apb_idx = api_idx = 0;

	while (idx < MAX_LOG_BUFFER_POS) {
		for (loop = 0; loop < g_lbi[idx].size; loop += 2) {
			if (!g_lbi[idx].type)
				continue;
			maxdsm_regmap_read(g_lbi[idx].addr + loop, &data);
			switch (g_lbi[idx].type) {
			case sizeof(uint8_t):
				maxdsm_byte_log_array[b_idx++] =
					data & 0xFF;
				break;
			case sizeof(uint32_t):
				maxdsm_int_log_array[i_idx++] =
					data & 0xFFFFFFFF;
				break;
			case sizeof(uint8_t)*2:
				maxdsm_after_prob_byte_log_array[apb_idx++] =
					data & 0xFF;
				break;
			case sizeof(uint32_t)*2:
				maxdsm_after_prob_int_log_array[api_idx++] =
					data & 0xFFFFFFFF;
				break;
			}
		}
		idx++;
	}

	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, 0, &maxdsm_log_timestamp);
	maxdsm_log_present = 1;

	mutex_unlock(&maxdsm_log_lock);
}

int maxdsm_get_dump_status(void)
{
	int ret = 0;

	if (!maxdsm.platform_type)
		ret = maxdsm_regmap_read(g_lbi[LOG_AVAILABLE].addr,
				&new_log_avail);

	return !ret ? (new_log_avail & 0x03) : ret;
}

void maxdsm_update_param(void)
{
	if (!maxdsm.platform_type) {
		maxdsm_regmap_write(g_lbi[WRITE_PROTECT].addr, 1);
		maxdsm_read_all_reg();
		maxdsm_read_logbuf_reg();
		maxdsm_regmap_write(g_lbi[WRITE_PROTECT].addr, 0);
	} else {
		mutex_lock(&dsm_lock);

		/* get params from the algorithm */
		maxdsm.filter_set = DSM_ID_FILTER_GET_AFE_PARAMS;
		dsm_open(&maxdsm);
		if (maxdsm.param[PARAM_EXCUR_LIMIT] != 0
				&& maxdsm.param[PARAM_THERMAL_LIMIT] != 0)
			new_log_avail |= 0x1;

		mutex_unlock(&dsm_lock);
	}
}

static void maxdsm_log_free(void **byte_log_array, void **int_log_array,
		void **afterbyte_log_array, void **after_int_log_array)
{
	if (likely(*byte_log_array)) {
		kfree(*byte_log_array);
		*byte_log_array = NULL;
	}

	if (likely(*int_log_array)) {
		kfree(*int_log_array);
		*int_log_array = NULL;
	}

	if (likely(*afterbyte_log_array)) {
		kfree(*afterbyte_log_array);
		*afterbyte_log_array = NULL;
	}

	if (likely(*after_int_log_array)) {
		kfree(*after_int_log_array);
		*after_int_log_array = NULL;
	}
}

static int maxdsm_log_duplicate(void **byte_log_array, void **int_log_array,
		void **afterbyte_log_array, void **after_int_log_array)
{
	void *blog_buf = NULL, *ilog_buf = NULL;
	void *after_blog_buf = NULL, *after_ilog_buf = NULL;
	int rc = 0;

	mutex_lock(&maxdsm_log_lock);

	if (unlikely(!maxdsm_log_present)) {
		rc = -ENODATA;
		goto abort;
	}

	blog_buf = kzalloc(sizeof(maxdsm_byte_log_array), GFP_KERNEL);
	ilog_buf = kzalloc(sizeof(maxdsm_int_log_array), GFP_KERNEL);
	after_blog_buf
		= kzalloc(sizeof(maxdsm_after_prob_byte_log_array), GFP_KERNEL);
	after_ilog_buf
		= kzalloc(sizeof(maxdsm_after_prob_int_log_array), GFP_KERNEL);

	if (unlikely(!blog_buf || !ilog_buf
				|| !after_blog_buf || !after_ilog_buf)) {
		rc = -ENOMEM;
		goto abort;
	}

	memcpy(blog_buf, maxdsm_byte_log_array, sizeof(maxdsm_byte_log_array));
	memcpy(ilog_buf, maxdsm_int_log_array, sizeof(maxdsm_int_log_array));
	memcpy(after_blog_buf, maxdsm_after_prob_byte_log_array,
			sizeof(maxdsm_after_prob_byte_log_array));
	memcpy(after_ilog_buf, maxdsm_after_prob_int_log_array,
			sizeof(maxdsm_after_prob_int_log_array));

	goto out;

abort:
	maxdsm_log_free(&blog_buf, &ilog_buf, &after_blog_buf, &after_ilog_buf);
out:
	*byte_log_array = blog_buf;
	*int_log_array  = ilog_buf;
	*afterbyte_log_array = after_blog_buf;
	*after_int_log_array  = after_ilog_buf;
	mutex_unlock(&maxdsm_log_lock);

	return rc;
}

ssize_t maxdsm_log_prepare(char *buf)
{
	uint8_t *byte_log_array = NULL;
	uint32_t *int_log_array = NULL;
	uint8_t *afterbyte_log_array = NULL;
	uint32_t *after_int_log_array = NULL;
	int rc = 0;

	uint8_t log_available;
	uint8_t version_id;
	uint8_t *coil_temp_log_array;
	uint8_t *excur_log_array;
	uint8_t *after_coil_temp_log_array;
	uint8_t *after_excur_log_array;
	uint8_t *excur_after_coil_temp_log_array;
	uint8_t *excur_after_excur_log_array;

	uint32_t seq_count_temp;
	uint32_t seq_count_excur;
	uint32_t *rdc_log_array;
	uint32_t *freq_log_array;
	uint32_t *after_rdc_log_array;
	uint32_t *after_freq_log_array;
	uint32_t *excur_after_rdc_log_array;
	uint32_t *excur_after_freq_log_array;

	int param_excur_limit;
	int param_thermal_limit;
	int param_voice_coil;
	int param_release_time;
	int param_static_gain;
	int param_lfx_gain;
	int param_pilot_gain;

	rc = maxdsm_log_duplicate((void **)&byte_log_array,
			(void **)&int_log_array, (void **)&afterbyte_log_array,
			(void **)&after_int_log_array);

	switch (maxdsm.platform_type) {
	case 0: /* LSI */
	default:
		param_excur_limit = PARAM_LSI_EXCUR_LIMIT;
		param_thermal_limit = PARAM_LSI_THERMAL_LIMIT;
		param_voice_coil = PARAM_LSI_VOICE_COIL;
		param_release_time = PARAM_LSI_RELEASE_TIME;
		param_static_gain = PARAM_LSI_STATIC_GAIN;
		param_lfx_gain = PARAM_LSI_LFX_GAIN;
		param_pilot_gain = PARAM_LSI_PILOT_GAIN;
		break;
	case 1: /* QCOM */
		param_excur_limit = PARAM_EXCUR_LIMIT;
		param_thermal_limit = PARAM_THERMAL_LIMIT;
		param_voice_coil = PARAM_VOICE_COIL;
		param_release_time = PARAM_RELEASE_TIME;
		param_static_gain = PARAM_STATIC_GAIN;
		param_lfx_gain = PARAM_LFX_GAIN;
		param_pilot_gain = PARAM_PILOT_GAIN;
		break;
	}

	if (unlikely(rc)) {
		rc = snprintf(buf, PAGE_SIZE, "no log\n");
		if (maxdsm.param[param_excur_limit] != 0 &&
				maxdsm.param[param_thermal_limit] != 0) {
			rc += snprintf(buf+rc, PAGE_SIZE,
					"[Parameter Set] excursionlimit:0x%x, "
					"rdcroomtemp:0x%x, coilthermallimit:0x%x, "
					"releasetime:0x%x\n"
					, maxdsm.param[param_excur_limit],
					maxdsm.param[param_voice_coil],
					maxdsm.param[param_thermal_limit],
					maxdsm.param[param_release_time]);
			rc += snprintf(buf+rc, PAGE_SIZE,
					"[Parameter Set] staticgain:0x%x, lfxgain:0x%x, "
					"pilotgain:0x%x\n",
					maxdsm.param[param_static_gain],
					maxdsm.param[param_lfx_gain],
					maxdsm.param[param_pilot_gain]);
		}
		goto out;
	}

	log_available     = byte_log_array[0];
	version_id        = byte_log_array[1];
	coil_temp_log_array = &byte_log_array[2];
	excur_log_array    = &byte_log_array[2+LOG_BUFFER_ARRAY_SIZE];

	seq_count_temp       = int_log_array[0];
	seq_count_excur   = int_log_array[1];
	rdc_log_array  = &int_log_array[2];
	freq_log_array = &int_log_array[2+LOG_BUFFER_ARRAY_SIZE];

	after_coil_temp_log_array = &afterbyte_log_array[0];
	after_excur_log_array = &afterbyte_log_array[LOG_BUFFER_ARRAY_SIZE];
	after_rdc_log_array = &after_int_log_array[0];
	after_freq_log_array = &after_int_log_array[LOG_BUFFER_ARRAY_SIZE];

	excur_after_coil_temp_log_array
		= &afterbyte_log_array[LOG_BUFFER_ARRAY_SIZE*2];
	excur_after_excur_log_array
		= &afterbyte_log_array[LOG_BUFFER_ARRAY_SIZE*3];
	excur_after_rdc_log_array
		= &after_int_log_array[LOG_BUFFER_ARRAY_SIZE*2];
	excur_after_freq_log_array
		= &after_int_log_array[LOG_BUFFER_ARRAY_SIZE*3];

	if (log_available > 0 &&
			(ex_seq_count_temp != seq_count_temp
			 || ex_seq_count_excur != seq_count_excur)) {
		ex_seq_count_temp = seq_count_temp;
		ex_seq_count_excur = seq_count_excur;
		new_log_avail |= 0x2;
	}

	rc += snprintf(buf+rc, PAGE_SIZE,
			"DSM LogData saved at %4d-%02d-%02d %02d:%02d:%02d (UTC)\n",
			(int)(maxdsm_log_timestamp.tm_year + 1900),
			(int)(maxdsm_log_timestamp.tm_mon + 1),
			(int)(maxdsm_log_timestamp.tm_mday),
			(int)(maxdsm_log_timestamp.tm_hour),
			(int)(maxdsm_log_timestamp.tm_min),
			(int)(maxdsm_log_timestamp.tm_sec));

	if ((log_available & 0x1) == 0x1) {
		rc += snprintf(buf+rc, PAGE_SIZE,
				"*** Excursion Limit was exceeded.\n");
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Seq:%d, log_available=%d, version_id:3.1.%d\n",
				seq_count_excur, log_available, version_id);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Temperature={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
				excur_after_coil_temp_log_array[0],
				excur_after_coil_temp_log_array[1],
				excur_after_coil_temp_log_array[2],
				excur_after_coil_temp_log_array[3],
				excur_after_coil_temp_log_array[4],
				excur_after_coil_temp_log_array[5],
				excur_after_coil_temp_log_array[6],
				excur_after_coil_temp_log_array[7],
				excur_after_coil_temp_log_array[8],
				excur_after_coil_temp_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Excursion="
				"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
				excur_after_excur_log_array[0],
				excur_after_excur_log_array[1],
				excur_after_excur_log_array[2],
				excur_after_excur_log_array[3],
				excur_after_excur_log_array[4],
				excur_after_excur_log_array[5],
				excur_after_excur_log_array[6],
				excur_after_excur_log_array[7],
				excur_after_excur_log_array[8],
				excur_after_excur_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Rdc={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
				excur_after_rdc_log_array[0],
				excur_after_rdc_log_array[1],
				excur_after_rdc_log_array[2],
				excur_after_rdc_log_array[3],
				excur_after_rdc_log_array[4],
				excur_after_rdc_log_array[5],
				excur_after_rdc_log_array[6],
				excur_after_rdc_log_array[7],
				excur_after_rdc_log_array[8],
				excur_after_rdc_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Frequency={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
				excur_after_freq_log_array[0],
				excur_after_freq_log_array[1],
				excur_after_freq_log_array[2],
				excur_after_freq_log_array[3],
				excur_after_freq_log_array[4],
				excur_after_freq_log_array[5],
				excur_after_freq_log_array[6],
				excur_after_freq_log_array[7],
				excur_after_freq_log_array[8],
				excur_after_freq_log_array[9]);
	}

	if ((log_available & 0x2) == 0x2) {
		rc += snprintf(buf+rc, PAGE_SIZE,
				"*** Temperature Limit was exceeded.\n");
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Seq:%d, log_available=%d, version_id:3.1.%d\n",
				seq_count_temp, log_available, version_id);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Temperature={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
				coil_temp_log_array[0],
				coil_temp_log_array[1],
				coil_temp_log_array[2],
				coil_temp_log_array[3],
				coil_temp_log_array[4],
				coil_temp_log_array[5],
				coil_temp_log_array[6],
				coil_temp_log_array[7],
				coil_temp_log_array[8],
				coil_temp_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"              %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
				after_coil_temp_log_array[0],
				after_coil_temp_log_array[1],
				after_coil_temp_log_array[2],
				after_coil_temp_log_array[3],
				after_coil_temp_log_array[4],
				after_coil_temp_log_array[5],
				after_coil_temp_log_array[6],
				after_coil_temp_log_array[7],
				after_coil_temp_log_array[8],
				after_coil_temp_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Excursion={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
				excur_log_array[0],
				excur_log_array[1],
				excur_log_array[2],
				excur_log_array[3],
				excur_log_array[4],
				excur_log_array[5],
				excur_log_array[6],
				excur_log_array[7],
				excur_log_array[8],
				excur_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"             %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
				after_excur_log_array[0],
				after_excur_log_array[1],
				after_excur_log_array[2],
				after_excur_log_array[3],
				after_excur_log_array[4],
				after_excur_log_array[5],
				after_excur_log_array[6],
				after_excur_log_array[7],
				after_excur_log_array[8],
				after_excur_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Rdc={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
				rdc_log_array[0],
				rdc_log_array[1],
				rdc_log_array[2],
				rdc_log_array[3],
				rdc_log_array[4],
				rdc_log_array[5],
				rdc_log_array[6],
				rdc_log_array[7],
				rdc_log_array[8],
				rdc_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"      %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
				after_rdc_log_array[0],
				after_rdc_log_array[1],
				after_rdc_log_array[2],
				after_rdc_log_array[3],
				after_rdc_log_array[4],
				after_rdc_log_array[5],
				after_rdc_log_array[6],
				after_rdc_log_array[7],
				after_rdc_log_array[8],
				after_rdc_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"Frequency={ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
				freq_log_array[0],
				freq_log_array[1],
				freq_log_array[2],
				freq_log_array[3],
				freq_log_array[4],
				freq_log_array[5],
				freq_log_array[6],
				freq_log_array[7],
				freq_log_array[8],
				freq_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"             %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
				after_freq_log_array[0],
				after_freq_log_array[1],
				after_freq_log_array[2],
				after_freq_log_array[3],
				after_freq_log_array[4],
				after_freq_log_array[5],
				after_freq_log_array[6],
				after_freq_log_array[7],
				after_freq_log_array[8],
				after_freq_log_array[9]);
	}

	if (maxdsm.param[param_excur_limit] != 0 &&
			maxdsm.param[param_thermal_limit] != 0) {
		rc += snprintf(buf+rc, PAGE_SIZE,
				"[Parameter Set] excursionlimit:0x%x, "
				"rdcroomtemp:0x%x, coilthermallimit:0x%x, "
				"releasetime:0x%x\n",
				maxdsm.param[param_excur_limit],
				maxdsm.param[param_voice_coil],
				maxdsm.param[param_thermal_limit],
				maxdsm.param[param_release_time]);
		rc += snprintf(buf+rc, PAGE_SIZE,
				"[Parameter Set] staticgain:0x%x, lfxgain:0x%x, pilotgain:0x%x\n",
				maxdsm.param[param_static_gain],
				maxdsm.param[param_lfx_gain],
				maxdsm.param[param_pilot_gain]);
	}

out:

	return (ssize_t)rc;
}
#endif /* USE_DSM_LOG */

static int maxdsm_set_param(
		uint32_t wflag, struct param_set_data *data, int size)
{
	int loop, ret = 0;

	if (!maxdsm.platform_type) {
		pr_err("%s: Can't set. platform_type=%d\n",
				__func__, maxdsm.platform_type);
		return -ENODATA;
	}

	mutex_lock(&dsm_lock);

	maxdsm.param[PARAM_WRITE_FLAG] = wflag;
	for (loop = 0; loop < size; loop++)
		maxdsm.param[data[loop].name] = data[loop].value;
	maxdsm.filter_set = DSM_ID_FILTER_SET_AFE_CNTRLS;
	ret = dsm_open(&maxdsm);

	mutex_unlock(&dsm_lock);

	return ret < 0 ? ret : 0;
}

static int maxdsm_find_index_of_saved_params(
		struct param_set_data *params,
		int size,
		uint32_t param_name)
{
	while (size-- > 0)
		if (params[size].name == param_name)
			break;

	return size;
}

uint32_t maxdsm_get_platform_type(void)
{
	return maxdsm.platform_type;
}
EXPORT_SYMBOL_GPL(maxdsm_get_platform_type);

int maxdsm_set_feature_en(int on)
{
	int index;
	struct param_set_data data = {
		.name = PARAM_FEATURE_SET,
		.value = 0,
	};

	index = maxdsm_find_index_of_saved_params(
				maxdsm_saved_params,
				sizeof(maxdsm_saved_params)
					/ sizeof(struct param_set_data),
				PARAM_FEATURE_SET);
	if (index < 0 || !maxdsm.platform_type)
		return -ENODATA;

	if (on) {
		if (maxdsm_saved_params[index].value & 0x40) {
			pr_err("%s: feature_en has already 0x40\n",
					__func__);
			return -EALREADY;
		}
		maxdsm.filter_set = DSM_ID_FILTER_GET_AFE_PARAMS;
		dsm_open(&maxdsm);
		maxdsm_saved_params[index].value
			= maxdsm.param[PARAM_FEATURE_SET];
		data.value =
			maxdsm_saved_params[index].value | 0x40;
		dbg_maxdsm(
				"data.value=0x%08x maxdsm_saved_params[%d]"
				".value=0x%08x",
				data.value,
				index, maxdsm_saved_params[index].value);
	} else {
		data.value =
			maxdsm_saved_params[index].value & ~0x40;
		dbg_maxdsm(
				"data.value=0x%08x maxdsm_saved_params[%d]"
				".value=0x%08x",
				data.value,
				index, maxdsm_saved_params[index].value);
	}

	return maxdsm_set_param(
			FLAG_WRITE_FEATURE_ONLY,
			&data, 1);
}
EXPORT_SYMBOL_GPL(maxdsm_set_feature_en);

int maxdsm_set_rdc_temp(uint32_t rdc, uint32_t temp)
{
	struct param_set_data data[] = {
		{
			.name = PARAM_VOICE_COIL,
			.value = rdc << 19,
		},
		{
			.name = PARAM_AMBIENT_TEMP,
			.value = temp << 19,
		},
	};

	dbg_maxdsm("rdc=0x%08x(0x%08x) temp=0x%08x(0x%08x)",
			rdc, data[0].value, temp, data[1].value);

	return maxdsm_set_param(
			FLAG_WRITE_RDC_CAL_ONLY,
			data,
			sizeof(data) / sizeof(struct param_set_data));
}
EXPORT_SYMBOL_GPL(maxdsm_set_rdc_temp);

#ifdef USE_DSM_CTRL
void maxdsm_set_dsm_onoff_status(int on)
{
	struct param_set_data data[] = {
		{
			.name = PARAM_ONOFF,
			.value = on,
		},
	};

	return maxdsm_set_param(
			FLAG_WRITE_ONOFF_ONLY,
			data,
			sizeof(data) / sizeof(struct param_set_data));
}
#endif /* USE_DSM_CTRL */

uint32_t maxdsm_get_dcresistance(void)
{
	maxdsm.filter_set = DSM_ID_FILTER_GET_AFE_PARAMS;
	dsm_open(&maxdsm);

	return maxdsm.param[PARAM_RDC]
				<< maxdsm.param[PARAM_RDC_SZ];
}
EXPORT_SYMBOL_GPL(maxdsm_get_dcresistance);

uint32_t maxdsm_get_dsm_onoff_status(void)
{
	return maxdsm.param[PARAM_ONOFF];
}

void maxdsm_update_info(uint32_t *pinfo, uint32_t *binfo)
{
	int32_t *data = pinfo;

	if (pinfo == NULL || binfo == NULL) {
		pr_debug("%s: pinfo or binfo was not set.\n",
				__func__);
		return;
	}

	maxdsm.platform_type = data[PARAM_OFFSET_PLATFORM];
	maxdsm.port_id = data[PARAM_OFFSET_PORT_ID];
	maxdsm.rx_mod_id = data[PARAM_OFFSET_RX_MOD_ID];
	maxdsm.tx_mod_id = data[PARAM_OFFSET_TX_MOD_ID];
	maxdsm.filter_set = data[PARAM_OFFSET_FILTER_SET];
	maxdsm.version = data[PARAM_OFFSET_VERSION];
	memcpy(maxdsm.binfo, binfo, sizeof(maxdsm.binfo));

	switch (maxdsm.version) {
	case VERSION_3_5:
		maxdsm.param_size = PARAM_DSM_3_5_MAX;
		break;
	case VERSION_3_0:
	default:
		maxdsm.param_size = PARAM_DSM_3_0_MAX;
		break;
	}

	kfree(maxdsm.param);
	maxdsm.param = kzalloc(sizeof(int) * maxdsm.param_size, GFP_KERNEL);
	if (!maxdsm.param) {
		pr_err("%s: Failed to allocate memory for maxdsm.param\n",
				__func__);
	}

	dbg_maxdsm("maxdsm.param_size=%d maxdsm.version=%d",
			maxdsm.param_size, maxdsm.version);
}
int maxdsm_get_port_id(void)
{
	return maxdsm.port_id;
}
EXPORT_SYMBOL_GPL(maxdsm_get_port_id);

int maxdsm_get_rx_mod_id(void)
{
	return maxdsm.rx_mod_id;
}
EXPORT_SYMBOL_GPL(maxdsm_get_rx_mod_id);

int maxdsm_get_tx_mod_id(void)
{
	return maxdsm.tx_mod_id;
}
EXPORT_SYMBOL_GPL(maxdsm_get_tx_mod_id);

static int maxdsm_open(struct inode *inode, struct file *filep)
{
	return 0;
}

static ssize_t maxdsm_read(struct file *filep, char __user *buf,
		size_t count, loff_t *ppos)
{
	int rc;

	mutex_lock(&dsm_lock);

	switch (maxdsm.platform_type) {
	case 0: /* LSI */
		maxdsm_read_all_reg();
		break;
	case 1: /* QCOM */
		/* get params from the algorithm */
		maxdsm.filter_set = DSM_ID_FILTER_GET_AFE_PARAMS;
		dsm_open(&maxdsm);
		break;
	}

	/* copy params to user */
	rc = copy_to_user(buf, maxdsm.param, count);
	if (rc != 0) {
		pr_err("%s: copy_to_user failed - %d\n", __func__, rc);
		mutex_unlock(&dsm_lock);
		return 0;
	}

	mutex_unlock(&dsm_lock);

	return rc;
}

static ssize_t maxdsm_write(struct file *filep, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int x, rc;

	mutex_lock(&dsm_lock);

	rc = copy_from_user(maxdsm.param, buf, count);
	if (rc != 0) {
		pr_err("%s: copy_from_user failed - %d\n", __func__, rc);
		mutex_unlock(&dsm_lock);
		return rc;
	}

	switch (maxdsm.platform_type) {
	case 0: /* LSI */
		/* We will write only 1 param. */
		dbg_maxdsm("reg=0x%x val=0x%x regmap=%p",
				maxdsm.param[0],
				maxdsm.param[1],
				maxdsm.regmap);
		maxdsm_regmap_write(maxdsm.param[0], maxdsm.param[1]);
		break;
	case 1: /* QCOM */
		if (maxdsm.param[PARAM_WRITE_FLAG] == 0) {
			/* validation check */
			for (x = PARAM_THERMAL_LIMIT;
					x < maxdsm.param_size; x += 2) {
				if ((x != PARAM_ONOFF)
						&& (maxdsm.param[x] != 0)) {
					maxdsm.param[PARAM_WRITE_FLAG] =
						FLAG_WRITE_ALL;
					break;
				}
			}
		}

		if (maxdsm.param[PARAM_WRITE_FLAG]
					== FLAG_WRITE_ONOFF_ONLY
				|| maxdsm.param[PARAM_WRITE_FLAG]
					== FLAG_WRITE_ALL) {
			/* set params from the algorithm to application */
			maxdsm.filter_set = DSM_ID_FILTER_SET_AFE_CNTRLS;
			dsm_open(&maxdsm);
		}
		break;
	}

	mutex_unlock(&dsm_lock);

	return rc;
}

static const struct file_operations dsm_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= maxdsm_open,
	.release	= NULL,
	.read		= maxdsm_read,
	.write		= maxdsm_write,
	.mmap		= NULL,
	.poll		= NULL,
	.fasync		= NULL,
	.llseek		= NULL,
};

static struct miscdevice dsm_ctrl_miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     "dsm_ctrl_dev",
	.fops =     &dsm_ctrl_fops
};

int maxdsm_deinit(void)
{
	kfree(maxdsm.param);
	return misc_deregister(&dsm_ctrl_miscdev);
}

int maxdsm_init(void)
{
	int ret;

	if (maxdsm.registered) {
		pr_debug("%s: Already registered.\n", __func__);
		return -EALREADY;
	}

	ret = misc_register(&dsm_ctrl_miscdev);
	if (!ret) {
		maxdsm.registered = 1;
		maxdsm.param = kzalloc(
			sizeof(uint32_t) * PARAM_DSM_3_5_MAX, GFP_KERNEL);
		if (!maxdsm.param)
			pr_err("%s: Failed to allocate memory for maxdsm.param\n",
					__func__);
	}

	return ret;
}

MODULE_DESCRIPTION("Module for test Maxim DSM");
MODULE_LICENSE("GPL");
