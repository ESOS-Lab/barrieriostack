#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/maxim_dsm_a.h>

static uint32_t param[PARAM_DSM_3_5_MAX];

static DEFINE_MUTEX(dsm_lock);

static DEFINE_MUTEX(maxdsm_log_lock);

static uint32_t ex_seq_count_temp;
static uint32_t ex_seq_count_excur;
static uint32_t new_log_avail;

static bool maxdsm_log_present;
static struct tm maxdsm_log_timestamp;
static uint32_t maxdsm_byte_log_array[BEFORE_BUFSIZE];
static uint32_t maxdsm_int_log_array[BEFORE_BUFSIZE];
static uint32_t maxdsm_after_prob_byte_log_array[AFTER_BUFSIZE];
static uint32_t maxdsm_after_prob_int_log_array[AFTER_BUFSIZE];

void maxdsm_a_param_update(const void *param_array)
{
	memcpy(param, param_array, sizeof(param));
}

void maxdsm_a_log_update(const void *byte_log_array,
	const void *int_log_array,
	const void *after_prob_byte_log_array,
	const void *after_prob_int_log_array)
{
	struct timeval tv;
	mutex_lock(&maxdsm_log_lock);

	memcpy(maxdsm_byte_log_array, byte_log_array,
			sizeof(maxdsm_byte_log_array));
	memcpy(maxdsm_int_log_array, int_log_array,
			sizeof(maxdsm_int_log_array));

	memcpy(maxdsm_after_prob_byte_log_array, after_prob_byte_log_array,
		sizeof(maxdsm_after_prob_byte_log_array));
	memcpy(maxdsm_after_prob_int_log_array, after_prob_int_log_array,
		sizeof(maxdsm_after_prob_int_log_array));

	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, 0, &maxdsm_log_timestamp);

	maxdsm_log_present = true;

	mutex_unlock(&maxdsm_log_lock);
}

static void maxdsm_a_log_free(void **byte_log_array, void **int_log_array,
	void **after_byte_log_array, void **after_int_log_array)
{
	if (likely(*byte_log_array)) {
		kfree(*byte_log_array);
		*byte_log_array = NULL;
	}

	if (likely(*int_log_array)) {
		kfree(*int_log_array);
		*int_log_array = NULL;
	}

	if (likely(*after_byte_log_array)) {
		kfree(*after_byte_log_array);
		*after_byte_log_array = NULL;
	}

	if (likely(*after_int_log_array)) {
		kfree(*after_int_log_array);
		*after_int_log_array = NULL;
	}
}

static int maxdsm_a_log_duplicate(void **byte_log_array, void **int_log_array,
	void **after_byte_log_array, void **after_int_log_array)
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
	after_blog_buf =
		kzalloc(sizeof(maxdsm_after_prob_byte_log_array), GFP_KERNEL);
	after_ilog_buf =
		kzalloc(sizeof(maxdsm_after_prob_int_log_array), GFP_KERNEL);

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
	maxdsm_a_log_free(&blog_buf, &ilog_buf,
			&after_blog_buf, &after_ilog_buf);
out:
	*byte_log_array = blog_buf;
	*int_log_array  = ilog_buf;
	*after_byte_log_array = after_blog_buf;
	*after_int_log_array  = after_ilog_buf;
	mutex_unlock(&maxdsm_log_lock);

	return rc;
}

ssize_t maxdsm_a_log_prepare(char *buf)
{
	uint32_t *byte_log_array = NULL;
	uint32_t *int_log_array = NULL;
	uint32_t *after_byte_log_array = NULL;
	uint32_t *after_int_log_array = NULL;
	int rc = 0;

	uint32_t log_available;
	uint32_t version_id;
	uint32_t *coiltemp_log_array;
	uint32_t *excur_log_array;
	uint32_t *after_coiltemp_log_array;
	uint32_t *after_excur_log_array;
	uint32_t *excur_after_coiltemp_log_array;
	uint32_t *excur_after_excur_log_array;
	uint32_t seq_count_temp;
	uint32_t seq_count_excur;
	uint32_t *rdc_log_array;
	uint32_t *freq_log_array;
	uint32_t *after_rdc_log_array;
	uint32_t *after_freq_log_array;
	uint32_t *excur_after_rdc_log_array;
	uint32_t *excur_after_freq_log_array;

	rc = maxdsm_a_log_duplicate((void **)&byte_log_array,
		(void **)&int_log_array, (void **)&after_byte_log_array,
		(void **)&after_int_log_array);

	if (unlikely(rc)) {
		rc = snprintf(buf, PAGE_SIZE, "no log\n");
		if (param[PARAM_EXCUR_LIMIT] != 0
			&& param[PARAM_THERMAL_LIMIT] != 0) {
			rc += snprintf(buf+rc, PAGE_SIZE,
				"[Parameter Set] excursionlimit:0x%x, "
				"rdcroomtemp:0x%x, coilthermallimit:0x%x, "
				"releasetime:0x%x\n"
				, param[PARAM_EXCUR_LIMIT],
				param[PARAM_VOICE_COIL],
				param[PARAM_THERMAL_LIMIT],
				param[PARAM_RELEASE_TIME]);
			rc += snprintf(buf+rc, PAGE_SIZE,
				"[Parameter Set] "
				"staticgain:0x%x, lfxgain:0x%x, "
				"pilotgain:0x%x\n",
				param[PARAM_STATIC_GAIN],
				param[PARAM_LFX_GAIN],
				param[PARAM_PILOT_GAIN]);
		}
		goto out;
	}

	log_available     = byte_log_array[0];
	version_id        = byte_log_array[1];
	coiltemp_log_array = &byte_log_array[2];
	excur_log_array    = &byte_log_array[2+LOG_BUFFER_ARRAY_SIZE];

	seq_count_temp       = int_log_array[0];
	seq_count_excur   = int_log_array[1];
	rdc_log_array  = &int_log_array[2];
	freq_log_array = &int_log_array[2+LOG_BUFFER_ARRAY_SIZE];

	after_coiltemp_log_array = &after_byte_log_array[0];
	after_excur_log_array = &after_byte_log_array[LOG_BUFFER_ARRAY_SIZE];
	after_rdc_log_array = &after_int_log_array[0];
	after_freq_log_array = &after_int_log_array[LOG_BUFFER_ARRAY_SIZE];

	excur_after_coiltemp_log_array =
		&after_byte_log_array[LOG_BUFFER_ARRAY_SIZE*2];
	excur_after_excur_log_array =
		&after_byte_log_array[LOG_BUFFER_ARRAY_SIZE*3];
	excur_after_rdc_log_array =
		&after_int_log_array[LOG_BUFFER_ARRAY_SIZE*2];
	excur_after_freq_log_array =
		&after_int_log_array[LOG_BUFFER_ARRAY_SIZE*3];

	if (log_available > 0 &&
		(ex_seq_count_temp != seq_count_temp
		|| ex_seq_count_excur != seq_count_excur))	{
		ex_seq_count_temp = seq_count_temp;
		ex_seq_count_excur = seq_count_excur;
		new_log_avail |= 0x2;
	}

	rc += snprintf(buf+rc, PAGE_SIZE,
		"DSM LogData saved at "
		"%4d-%02d-%02d %02d:%02d:%02d (UTC)\n",
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
			"Temperature="
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			excur_after_coiltemp_log_array[0],
			excur_after_coiltemp_log_array[1],
			excur_after_coiltemp_log_array[2],
			excur_after_coiltemp_log_array[3],
			excur_after_coiltemp_log_array[4],
			excur_after_coiltemp_log_array[5],
			excur_after_coiltemp_log_array[6],
			excur_after_coiltemp_log_array[7],
			excur_after_coiltemp_log_array[8],
			excur_after_coiltemp_log_array[9]);
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
			"Rdc="
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
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
			"Frequency="
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
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
			"Temperature="
			"{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			coiltemp_log_array[0],
			coiltemp_log_array[1],
			coiltemp_log_array[2],
			coiltemp_log_array[3],
			coiltemp_log_array[4],
			coiltemp_log_array[5],
			coiltemp_log_array[6],
			coiltemp_log_array[7],
			coiltemp_log_array[8],
			coiltemp_log_array[9]);
		rc += snprintf(buf+rc, PAGE_SIZE,
			"              "
			"%d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
			after_coiltemp_log_array[0],
			after_coiltemp_log_array[1],
			after_coiltemp_log_array[2],
			after_coiltemp_log_array[3],
			after_coiltemp_log_array[4],
			after_coiltemp_log_array[5],
			after_coiltemp_log_array[6],
			after_coiltemp_log_array[7],
			after_coiltemp_log_array[8],
			after_coiltemp_log_array[9]);
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
			"            "
			"%d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
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
			"            "
			"%d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
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

	if (param[PARAM_EXCUR_LIMIT] != 0 &&
		param[PARAM_THERMAL_LIMIT] != 0)	{
		rc += snprintf(buf+rc, PAGE_SIZE,
			"[Parameter Set] excursionlimit:0x%x, "
			"rdcroomtemp:0x%x, coilthermallimit:0x%x, "
			"releasetime:0x%x\n",
			param[PARAM_EXCUR_LIMIT],
			param[PARAM_VOICE_COIL],
			param[PARAM_THERMAL_LIMIT],
			param[PARAM_RELEASE_TIME]);
		rc += snprintf(buf+rc, PAGE_SIZE,
			"[Parameter Set] "
			"staticgain:0x%x, lfxgain:0x%x, pilotgain:0x%x\n",
			param[PARAM_STATIC_GAIN],
			param[PARAM_LFX_GAIN],
			param[PARAM_PILOT_GAIN]);
	}

out:
	maxdsm_a_log_free((void **)&byte_log_array, (void **)&int_log_array,
		(void **)&after_byte_log_array, (void **)&after_int_log_array);

	return (ssize_t)rc;
}
