/*
 * es705.c  --  Audience eS705 ALSA SoC Audio driver
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
#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#if defined(CONFIG_SND_SOC_ES_SLIM)
#include <linux/slimbus/slimbus.h>
#endif
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/ratelimit.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/esxxx.h>
#include <linux/wait.h>

#include <sound/core.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "es705.h"
#include "es705-platform.h"
#include "es705-routes.h"
#include "es705-profiles.h"
#include "es705-slim.h"
#include "es705-i2c.h"
#include "es705-i2s.h"
#include "es705-spi.h"
#include "es705-cdev.h"
#include "es705-uart.h"
#include "es705-uart-common.h"
#include "es705-veq-params.h"

#define CHECK_ROUTE_STATUS_AND_RECONFIG
#if defined(CHECK_ROUTE_STATUS_AND_RECONFIG)
static struct delayed_work chk_route_st_and_recfg_work;
static long cnt_reconfig;
#endif

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
#define ES705_NO_EVENT			0x0000

#define ES705_CVS_PRESET_CMD 0x906f270f	/* 9999 */

#define ES705_INT_OSC_MEASURE_START	0x805d
#define ES705_INT_OSC_MEASURE_QUERY	0x805e
#endif

#define es705_codec_suspend	NULL
#define es705_codec_resume	NULL

#define ES705_CMD_ACCESS_WR_MAX 2
#define ES705_CMD_ACCESS_RD_MAX 2

#if defined(CONFIG_SND_SOC_ES705)
#define FW_FILENAME "audience-es705-fw.bin"
#define VS_FILENAME "audience-es705-vs.bin"
#elif defined(CONFIG_SND_SOC_ES804)
#define FW_FILENAME "audience-es804-fw.bin"
#define VS_FILENAME "audience-es804-vs.bin"
#else
#define FW_FILENAME "audience-es705-fw.bin"
#define VS_FILENAME "audience-es705-vs.bin"
#endif

struct es705_api_access {
	u32 read_msg[ES705_CMD_ACCESS_RD_MAX];
	unsigned int read_msg_len;
	u32 write_msg[ES705_CMD_ACCESS_WR_MAX];
	unsigned int write_msg_len;
	unsigned int val_shift;
	unsigned int val_max;
};

#include "es705-access.h"

#define ENABLE_VS_DATA_DUMP
#define NARROW_BAND	0
#define WIDE_BAND	1
#define NETWORK_OFFSET	21

static int network_type = NARROW_BAND;
static int extra_volume;

/* Route state for Internal state management */
enum es705_power_state {
	ES705_POWER_FW_LOAD,
	ES705_POWER_SLEEP,
	ES705_POWER_SLEEP_PENDING,
	ES705_POWER_AWAKE
};

static const char * const power_state[] = {
	"boot",
	"sleep",
	"sleep pending",
	"awake",
};

static const char * const power_state_cmd[] = {
	"not defined",
	"sleep",
	"mp_sleep",
	"mp_cmd",
	"normal",
	"vs_overlay",
	"vs_lowpwr",
	"vs_streaming",
};

static int dhwpt_is_enabled;
#define SET_DHWPT_BTOD	0x9052005C
#define RESET_DHWPT		0x90520000

/* codec private data TODO: move to runtime init */
struct es705_priv es705_priv = {
	.pm_state = ES705_POWER_AWAKE,

	.rx1_route_enable = 0,
	.tx1_route_enable = 0,
	.rx2_route_enable = 0,

	.vs_enable = 0,
	.vs_wakeup_keyword = 0,

	.ap_tx1_ch_cnt = 2,

	.es705_power_state = ES705_SET_POWER_STATE_NORMAL,
	.streamdev.intf = -1,
	.ns = 1,

	.wakeup_method = 0,

#if defined(SAMSUNG_ES705_FEATURE)
	.voice_wakeup_enable = 0,
	.voice_lpm_enable = 0,
	/* for tuning : 1 */
	.change_uart_config = 0,
	.internal_route_num = ROUTE_MAX,
#endif
#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
#if defined(CONFIG_SEC_FACTORY)
	.grammar_path = "higalaxy_en_us_gram6.bin",
	.net_path = "higalaxy_en_us_am.bin",
#else
	.grammar_path = "voice_grammar.bin",
	.net_path = "voice_net.bin",
#endif
	.keyword_type = 0,
#endif

};

const char *esxxx_mode[] = {
	"SBL",
	"STANDARD",
	"VOICESENSE",
};

static int es705_sleep(struct es705_priv *es705);
static int es705_wakeup(struct es705_priv *es705);
static int abort_request;
static int es705_vs_load(struct es705_priv *es705);
static int es705_write_then_read(struct es705_priv *es705,
				 const void *buf, int len,
				 u32 *rspn, int match);
static unsigned int es705_read(struct snd_soc_codec *codec, unsigned int reg);
static int es705_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value);
int es705_put_veq_block(int volume);
#if defined(CONFIG_SND_SOC_ES_I2C)
static struct class *earsmart_class;
static struct device *control_dev;
#endif

/* indexed by ES705 INTF number */
u32 es705_streaming_cmds[4] = {
	0x90250200,		/* ES705_SLIM_INTF */
	0x90250000,		/* ES705_I2C_INTF  */
	0x90250300,		/* ES705_SPI_INTF  */
	0x90250100,		/* ES705_UART_INTF */
};

static int preset_delay_time = 5;
#if defined(CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW)
static int cnt_restore_std_fw_in_sleep;
static int cnt_restore_std_fw_in_wakeup;
static int cnt_restore_std_fw_in_vs;

int restore_std_fw(struct es705_priv *es705)
{
	int rc = 0;

	dev_info(es705->dev, "%s(): START!\n", __func__);

	es705->mode = SBL;
	es705_gpio_reset(es705);
	rc = es705_bootup(es705);

	dev_info(es705->dev, "%s(): rc = %d\n", __func__, rc);

	return rc;
}
#endif

int es705_write_block(struct es705_priv *es705, const u32 *cmd_block)
{
	u32 api_word;
	u8 msg[4];
	int rc = 0;

	mutex_lock(&es705->api_mutex);
	while (*cmd_block != 0xffffffff) {
#if defined(CONFIG_SND_SOC_ES_SLIM)
		api_word = cpu_to_le32(*cmd_block);
#else
		api_word = cpu_to_be32(*cmd_block);
#endif
		memcpy(msg, (char *)&api_word, 4);
		es705->dev_write(es705, msg, 4);
#if defined(CONFIG_SND_SOC_ES_SLIM)
		if (msg[3] == 0x90 && msg[2] == 0x31)
#else
		if (msg[0] == 0x90 && msg[1] == 0x31)
#endif
			usleep_range(preset_delay_time * 1000,
				     preset_delay_time * 1000);
		else
			usleep_range(1000, 1100);

		dev_dbg(es705->dev, "%s(): msg = 0x%02x%02x%02x%02x\n",
			__func__, msg[3], msg[2], msg[1], msg[0]);

		cmd_block++;
	}
	mutex_unlock(&es705->api_mutex);

	return rc;
}

#if defined(CHECK_ROUTE_STATUS_AND_RECONFIG)
static void chk_route_st_and_recfg(struct work_struct *w)
{
	struct es705_priv *es705 = &es705_priv;
	int rc = 0;
	unsigned int value = 0;

	if (es705->pm_state != ES705_POWER_AWAKE) {
		dev_info(es705->dev, "%s(): don't route when pm_state is %d\n",
					__func__, es705->pm_state);
		goto OUT;
	}

	if (es705->internal_route_num >= ROUTE_MAX) {
		dev_info(es705->dev, "%s(): don't route %ld\n",
					__func__, es705->internal_route_num);
		goto OUT;
	}

	/*
	 * Sometimes a large number of commands may be staged, the amount of
	 * time it takes for the system to perform the commit may vary.
	 * Therefore, the GetRouteChangeStatus (0x804F) command returns
	 * whether the Route setup is still processing
	 * or whether it has completed.
	 */
	value = es705_read(NULL, ES705_CHANGE_STATUS);
	if (value) {
		dev_info(es705->dev, "%s(): Re-Route : %ld\n",
					__func__, es705->internal_route_num);

		/* Re-route */
		value = es705->internal_route_num;
		rc = es705_write_block(es705, &es705_route_configs[value][0]);
		if (rc)
			dev_err(es705->dev, "%s(): routing fail (%d)\n",
					__func__, value);

		usleep_range(10000, 11000);

		/* Re-write VEQ */
		es705->current_veq_preset = 1;
		value = es705->current_veq;
		es705->current_veq = -1;
		rc = es705_put_veq_block(value);
		if (rc < 0)
			dev_err(es705->dev, "%s(): Veq setting failed (%d)\n",
						__func__, value);

		cnt_reconfig++;
	}

OUT:
	dev_info(es705->dev, "%s(): EXIT\n", __func__);
}
#endif

/* Note: this may only end up being called in a api locked context. In
 * that case the mutex locks need to be removed.
 */
#define WDB_RDB_BLOCK_MAX_SIZE	512
int es705_read_vs_data_block(struct es705_priv *es705, char *dump_data,
			     unsigned max_size)
{
	/* This function is not re-entrant so avoid stack bloat. */
	u32 cmd = 0x802e0008;
	u32 resp;
	int ret;
	unsigned size;

	es705->vs_keyword_param_size = 0;

	mutex_lock(&es705->api_mutex);

	if (es705->rdb_wdb_open) {
		ret = es705->rdb_wdb_open(es705);
		if (ret < 0) {
			dev_err(es705->dev, "%s(): RDB open fail\n", __func__);
			goto rdb_open_error;
		}
	}

	if (es705->rdb_wdb_open)
		cmd = cpu_to_be32(cmd);	/* over UART */
	else
		cmd = cpu_to_le32(cmd);	/* over SLIMBus */

	es705->wdb_write(es705, (char *)&cmd, 4);
	ret = es705->rdb_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto OUT;
	}

	if (es705->rdb_wdb_open)
		resp = be32_to_cpu(resp);	/* over UART */
	else
		resp = le32_to_cpu(resp);	/* over SLIMBus */

	if ((resp & 0xffff0000) != 0x802e0000) {
		dev_err(es705->dev,
			"%s(): invalid read v-s data block size = 0x%08x\n",
			__func__, resp);
		goto OUT;
	}
	size = resp & 0xffff;
	dev_dbg(es705->dev, "%s(): resp=0x%08x size=%d\n", __func__, resp,
		size);
	BUG_ON(size == 0);
	BUG_ON(size > max_size);

	/*
	 * This assumes we need to transfer the block in 4 byte
	 * increments. This is true on slimbus, but may not hold true
	 * for other buses.
	 */
	while (es705->vs_keyword_param_size < size) {
		if (es705->rdb_wdb_open) {
			/* over UART */
			ret = es705->rdb_read(es705, dump_data +
					      es705->vs_keyword_param_size,
					      size -
					      es705->vs_keyword_param_size);
		} else {
			/* over SLIMBus */
			ret = es705->rdb_read(es705, dump_data +
					      es705->vs_keyword_param_size, 4);
		}

		if (ret < 0) {
			dev_dbg(es705->dev, "%s(): error reading data block\n",
				__func__);
			es705->vs_keyword_param_size = 0;
			goto OUT;
		}
		es705->vs_keyword_param_size += ret;
		dev_dbg(es705->dev,
			"%s(): stored v-s read and saved of %d bytes\n",
			__func__, ret);
	}
	dev_dbg(es705->dev, "%s(): stored v-s keyword block of %d bytes\n",
		__func__, es705->vs_keyword_param_size);

OUT:
	if (es705->rdb_wdb_close)
		es705->rdb_wdb_close(es705);
rdb_open_error:
	mutex_unlock(&es705->api_mutex);
	if (ret < 0)
		dev_err(es705->dev, "%s(): v-s read data block failure=%d\n",
			__func__, ret);
	return ret;
}

int es705_write_vs_data_block(struct es705_priv *es705)
{
	u32 cmd;
	u32 resp;
	int ret = 0;
	u8 *dptr;
	u16 rem;
	u8 wdb[4];
	int end_data = 0;
	int sz;
	int blocks;

	if (es705->vs_keyword_param_size == 0) {
		dev_warn(es705->dev,
			 "%s(): attempt to write empty keyword data block\n",
			 __func__);
		return -ENOENT;
	}

	BUG_ON(es705->vs_keyword_param_size % 4 != 0);

	mutex_lock(&es705->api_mutex);
	/* Add code to support UART WDB feature */
	/* Get 512 blocks number */
	blocks = es705->vs_keyword_param_size / WDB_RDB_BLOCK_MAX_SIZE;
	sz = WDB_RDB_BLOCK_MAX_SIZE;

SEND_DATA:
	if (blocks) {
		cmd = 0x802f0000 | (sz & 0xffff);

		if (es705->rdb_wdb_open)
			cmd = cpu_to_be32(cmd);
		else
			cmd = cpu_to_le32(cmd);

		ret = es705->wdb_write(es705, (char *)&cmd, 4);
		if (ret < 0) {
			dev_err(es705->dev,
				"%s(): error writing cmd 0x%08x to device\n",
				__func__, cmd);
			goto EXIT;
		}

		ret = es705->rdb_read(es705, (char *)&resp, 4);
		if (ret < 0) {
			dev_dbg(es705->dev,
				"%s(): error sending request = %d\n", __func__,
				ret);
			goto EXIT;
		}

		if (es705->rdb_wdb_open)
			resp = be32_to_cpu(resp);
		else
			resp = le32_to_cpu(resp);

		if ((resp & 0xffff0000) != 0x802f0000) {
			dev_err(es705->dev,
				"%s(): invalid write data block 0x%08x\n",
				__func__, resp);
			goto EXIT;
		}

		if (es705->rdb_wdb_open) {
			/* send data over UART */
			do {
				ret = es705->wdb_write(es705,
						       (char *)es705->
						       vs_keyword_param, sz);
				if (ret < 0) {
					dev_err(es705->dev,
						"%s(): v-s wdb UART write error\n",
						__func__);
					goto EXIT;
				}

				resp = 0;
				ret = es705->rdb_read(es705, (char *)&resp, 4);
				if (ret < 0) {
					dev_err(es705->dev,
						"%s(): WDB ACK request fFAIL\n",
						__func__);
					goto EXIT;
				}
				resp = be32_to_cpu(resp);

				if (resp & 0xFFFF) {
					dev_err(es705->dev,
						"%s(): write WDB FAIL. ACK=0x%08x\n",
						__func__, resp);
					goto EXIT;
				}
			} while (--blocks);
		} else {
			/* send data over SLIMBus */
			dptr = es705->vs_keyword_param;
			for (rem = es705->vs_keyword_param_size; rem > 0;
			     rem -= 4, dptr += 4) {
				wdb[0] = dptr[3];
				wdb[1] = dptr[2];
				wdb[2] = dptr[1];
				wdb[3] = dptr[0];
				ret = es705->wdb_write(es705, (char *)wdb, 4);
				if (ret < 0) {
					dev_err(es705->dev,
						"%s(): v-s WDB write over SLIMBus FAIL, offset=%lu\n",
						__func__,
						dptr - es705->vs_keyword_param);
					goto EXIT;
				}
				resp = 0;
				ret = es705->rdb_read(es705, (char *)&resp, 4);
				if (ret < 0) {
					dev_err(es705->dev,
						"%s(): WDB ACK request FAIL\n",
						__func__);
					goto EXIT;
				}
				resp = le32_to_cpu(resp);
				dev_dbg(es705->dev, "%s(): resp=0x%08x\n",
					__func__, resp);
				if (resp & 0xFFFF) {
					dev_err(es705->dev,
						"%s(): write WDB FAIL. ACK=0x%08x\n",
						__func__, resp);
					goto EXIT;
				}
			}
		}
	}

	if (!end_data) {
		sz = es705->vs_keyword_param_size -
		    (es705->vs_keyword_param_size / WDB_RDB_BLOCK_MAX_SIZE) *
		    WDB_RDB_BLOCK_MAX_SIZE;
		if (sz) {
			blocks = 1;
			end_data = 1;
			goto SEND_DATA;
		}
	}
EXIT:
	mutex_unlock(&es705->api_mutex);
	if (ret < 0)
		dev_err(es705->dev, "%s(): v-s wdb failed ret=%d\n",
			__func__, ret);
	return ret;
}

static int es705_enable_dhwpt(int onoff)
{
	struct es705_priv *es705 = &es705_priv;
	int rc = 0;

	dev_info(es705->dev, "%s(): Entry dhwpt_is_enabled(%d) onoff(%d)\n",
		__func__, dhwpt_is_enabled, onoff);

	if (dhwpt_is_enabled != onoff) {
		if (onoff) {
			rc = es705_cmd(es705, SET_DHWPT_BTOD);
			if (rc < 0) {
				dev_err(es705->dev,
						"%s():Failed DHWPT\n",
						__func__);
				dhwpt_is_enabled = 0;
			} else {
				dhwpt_is_enabled = 1;
			}
			es705_sleep(es705);
		} else {
			if (es705->es705_power_state
					== ES705_SET_POWER_STATE_SLEEP)
				es705_wakeup(es705);

			rc = es705_cmd(es705, RESET_DHWPT);
			if (rc < 0) {
				dev_err(es705->dev,
						"%s():Failed DHWPT off\n",
						__func__);
				dhwpt_is_enabled = 1;
			} else {
				dhwpt_is_enabled = 0;
			}
		}
	}
	dev_info(es705->dev, "%s(): Exit %s\n",	__func__,
				dhwpt_is_enabled ? "DHWPT ON":"DHWPT OFF");
	return rc;
}

static int es705_chk_route_id_for_dhwpt(long route_index)
{
	struct es705_priv *es705 = &es705_priv;
	int rc = 0;

	if (es705->pdata->use_dhwpt) {
		switch (route_index) {
		case ROUTE_FT_NB_NS_ON:
		case ROUTE_CT_NB_NS_ON:
		case ROUTE_FT_NB_NS_OFF:
		case ROUTE_CT_NB_NS_OFF:
		case ROUTE_FT_WB_NS_ON:
		case ROUTE_CT_WB_NS_ON:
		case ROUTE_FT_WB_NS_OFF:
		case ROUTE_CT_WB_NS_OFF:
			es705_enable_dhwpt(0);
			break;

		default:
			es705_enable_dhwpt(1);
			rc = 1;
			break;
		}
	}
	return rc;
}

static void es705_switch_route(long route_index)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;

	if (route_index >= ROUTE_MAX) {
		dev_err(es705->dev,
			"%s: new es705_internal_route = %ld is out of range\n",
			__func__, route_index);
		if (route_index == ROUTE_MAX)
			es705->internal_route_num = route_index;
		return;
	}
	dev_info(es705->dev,
		"%s: switch current es705_internal_route = %ld to new route = %ld\n",
			__func__, es705->internal_route_num, route_index);

	if (es705->internal_route_num == route_index) {
		dev_info(es705->dev, "%s: same route with previous, skip!\n",
			__func__);
		return;
	}

	/* Check DHWPT interface */
	if (es705_chk_route_id_for_dhwpt(route_index)) {
		es705->internal_route_num = route_index;
		es705->current_veq = -1;
		return;
	}

	rc = es705_write_block(es705,
			       &es705_route_configs[route_index][0]);
	if (rc) {
		dev_err(es705->dev, "%s: routing fail", __func__);
	} else {
		es705->internal_route_num = route_index;
		es705->current_veq = -1;
	}

#if defined(CHECK_ROUTE_STATUS_AND_RECONFIG)
	dev_info(es705->dev, "%s(): cnt_reconfig(%ld)\n", __func__,
			cnt_reconfig);
	if (delayed_work_pending(&chk_route_st_and_recfg_work)) {
		dev_info(es705->dev,
			"%s(): cancel_delayed_work(chk_route_st_and_recfg_work)\n",
			__func__);
		cancel_delayed_work_sync(&chk_route_st_and_recfg_work);
	}
	if (es705->internal_route_num == ROUTE_FT_NB_NS_ON ||
		es705->internal_route_num == ROUTE_CT_NB_NS_ON ||
		es705->internal_route_num == ROUTE_FT_WB_NS_ON ||
		es705->internal_route_num == ROUTE_CT_WB_NS_ON )
		schedule_delayed_work(&chk_route_st_and_recfg_work,
			msecs_to_jiffies(600));
#endif
}

static void es705_switch_route_config(long route_index)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;
	long route_config_idx = route_index;

	if (!(route_index < ARRAY_SIZE(es705_route_configs))) {
		dev_err(es705->dev,
			"%s(): new es705_internal_route = %ld is out of range\n",
			__func__, route_index);
		return;
	}

	switch (route_config_idx) {
	case 1:		/* SPK NS ON NB */
	case 2:		/* RCV NS ON NB */
	case 3:		/* SPK NS OFF NB */
	case 4:		/* RCV NS OFF NB */
	case 22:		/* SPK NS ON WB */
	case 23:		/* RCV NS ON WB */
	case 24:		/* SPK NS OFF WB */
	case 25:		/* RCV NS OFF WB */
		es705->current_veq_preset = 1;
		es705->current_veq = -1;
		es705->voice_wakeup_enable = 0;
		break;
	default:
		es705->current_veq_preset = 0;
	}
	es705->current_bwe = 0;

	if (network_type == WIDE_BAND) {
		if (route_config_idx >= 0 && route_config_idx < 5) {
			route_config_idx += NETWORK_OFFSET;
			dev_dbg(es705->dev,
				"%s() adjust wideband offset\n", __func__);
		}
	} else if (network_type == NARROW_BAND) {
		if (route_config_idx >= 0 + NETWORK_OFFSET
		    && route_config_idx < 5 + NETWORK_OFFSET) {
			route_config_idx -= NETWORK_OFFSET;
			dev_dbg(es705->dev,
				"%s() adjust narrowband offset\n", __func__);
		}
	}

	dev_info(es705->dev,
		 "%s(): converts route_index = %ld to %ld and old num is %ld\n",
		 __func__, route_index, route_config_idx,
		 es705->internal_route_num);

	if (es705->internal_route_num != route_config_idx) {
		es705->internal_route_num = route_config_idx;
		rc = es705_write_block(es705,
				       &es705_route_configs[route_config_idx]
				       [0]);
	}
}

/* Send a single command to the chip.
 *
 * If the SR (suppress response bit) is NOT set, will read the
 * response and cache it the driver object retrieve with es705_resp().
 *
 * Returns:
 * 0 - on success.
 * EITIMEDOUT - if the chip did not respond in within the expected time.
 * E* - any value that can be returned by the underlying HAL.
 */
int es705_cmd(struct es705_priv *es705, u32 cmd)
{
	int sr;
	int err;
	u32 resp;

	BUG_ON(!es705);
	sr = (cmd & BIT(28)) ? 1 : 0;

	err = es705->cmd(es705, cmd, sr, &resp);
	if (err || sr)
		return err;

	if (resp == 0) {
		err = -ETIMEDOUT;
		dev_err(es705->dev, "%s(): no response to command 0x%08x\n",
			__func__, cmd);
	} else {
		es705->last_response = resp;
		get_monotonic_boottime(&es705->last_resp_time);
	}

	return err;
}

static void es705_switch_to_normal_mode(struct es705_priv *es705)
{
	int rc;
	int match = 1;
	u32 cmd = (ES705_SET_POWER_STATE << 16) | ES705_SET_POWER_STATE_NORMAL;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_rspn = sync_cmd;

	es705_cmd(es705, cmd);
	msleep(20);
	es705->pm_state = ES705_POWER_AWAKE;

	rc = es705_write_then_read(es705, &sync_cmd,
				   sizeof(sync_cmd), &sync_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): es705 Sync FAIL\n", __func__);
}

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
static void es705_switch_vs_streaming_to_normal_mode(struct es705_priv *es705)
{
	int rc;
	u32 cmd = (ES705_SET_SMOOTH << 16) | ES705_SET_SMOOTH_RATE;

	dev_info(es705->dev, "%s()\n", __func__);

	if (es705->pdata->esxxx_clk_cb) {
		dev_info(es705->dev, "%s(): external clock on\n", __func__);
		es705->pdata->esxxx_clk_cb(es705->dev, 1);
		usleep_range(5000, 5500);
	}

	rc = es705_write(NULL, ES705_POWER_STATE,
			 ES705_SET_POWER_STATE_VS_OVERLAY);
	if (rc)
		dev_err(es705->dev,
			"%s(): Failed set power state to vs overlay\n",
			__func__);
	msleep(100);

	rc = es705_write(NULL, ES705_POWER_STATE, ES705_SET_POWER_STATE_NORMAL);
	if (rc)
		dev_err(es705->dev, "%s(): Failed set power state to normal\n",
			__func__);
	msleep(20);

	rc = es705_cmd(es705, cmd);

	es705->pm_state = ES705_POWER_AWAKE;
	es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
	es705->mode = STANDARD;

	dev_info(es705->dev, "%s(): Exit\n", __func__);
}
#endif

static int es705_switch_to_vs_mode(struct es705_priv *es705)
{
	int rc = 0;
	u32 cmd = (ES705_SET_POWER_STATE << 16) |
	    ES705_SET_POWER_STATE_VS_OVERLAY;

	if (!abort_request) {
		rc = es705_cmd(es705, cmd);
		if (rc < 0) {
			dev_err(es705->dev, "%s(): Set VS SBL Fail", __func__);
		} else {
			msleep(20);
			dev_dbg(es705->dev, "%s(): VS Overlay Mode", __func__);
			rc = es705_vs_load(es705);
		}
	} else {
		es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
		dev_info(es705->dev, "%s(): Skip to switch to vs mode",
			 __func__);
	}

	return rc;
}

static unsigned int es705_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct es705_priv *es705 = &es705_priv;
	struct es705_api_access *api_access;
	u32 api_word[2] = { 0 };
	char req_msg[8];
	u32 ack_msg;
	char *msg_ptr;
	unsigned int msg_len;
	unsigned int value;
	int match = 0;
	int rc;

	if (reg >= ES705_API_ADDR_MAX) {
		dev_err(es705->dev, "%s(): invalid address = 0x%04x\n",
			__func__, reg);
		return -EINVAL;
	}

	api_access = &es705_api_access[reg];
	msg_len = api_access->read_msg_len;
	memcpy((char *)api_word, (char *)api_access->read_msg, msg_len);
	switch (msg_len) {
	case 8:
		cpu_to_le32s(&api_word[1]);
	case 4:
		cpu_to_le32s(&api_word[0]);
	}
	memcpy(req_msg, (char *)api_word, msg_len);

	msg_ptr = req_msg;
	mutex_lock(&es705->api_mutex);
	rc = es705->dev_write_then_read(es705, msg_ptr, msg_len,
					&ack_msg, match);
	mutex_unlock(&es705->api_mutex);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): es705_xxxx_write()", __func__);
		return rc;
	}
	memcpy((char *)&api_word[0], &ack_msg, 4);
	le32_to_cpus(&api_word[0]);
	value = api_word[0] & 0xffff;

	return value;
}

static int es705_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	struct es705_priv *es705 = &es705_priv;
	struct es705_api_access *api_access;
	u32 api_word[2] = { 0 };
	char msg[8];
	char *msg_ptr;
	int msg_len;
	unsigned int val_mask;
	int rc = 0;

	if (reg >= ES705_API_ADDR_MAX) {
		dev_err(es705->dev, "%s(): invalid address = 0x%04x\n",
			__func__, reg);
		return -EINVAL;
	}

	api_access = &es705_api_access[reg];
	msg_len = api_access->write_msg_len;
	val_mask = (1 << get_bitmask_order(api_access->val_max)) - 1;
	dev_info(es705->dev,
		 "%s(): reg=%d val=%d msg_len = %d val_mask = 0x%08x\n",
		 __func__, reg, value, msg_len, val_mask);
	memcpy((char *)api_word, (char *)api_access->write_msg, msg_len);
	switch (msg_len) {
	case 8:
		api_word[1] |= (val_mask & value);
		cpu_to_le32s(&api_word[1]);
		cpu_to_le32s(&api_word[0]);
		break;
	case 4:
		api_word[0] |= (val_mask & value);
		cpu_to_le32s(&api_word[0]);
		break;
	}
	memcpy(msg, (char *)api_word, msg_len);

	msg_ptr = msg;
	mutex_lock(&es705->api_mutex);
	rc = es705->dev_write(es705, msg_ptr, msg_len);
	mutex_unlock(&es705->api_mutex);
	return rc;
}

static int es705_write_then_read(struct es705_priv *es705,
				 const void *buf, int len,
				 u32 *rspn, int match)
{
	int rc;
	rc = es705->dev_write_then_read(es705, buf, len, rspn, match);
	return rc;
}

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
#define POLY 0x8408
#define BLOCK_PAYLOAD_SIZE 508

static unsigned short crc_update(char *buf, int length, unsigned short crc)
{
	unsigned char i;
	unsigned short data;

	while (length--) {
		data = (unsigned short)(*buf++) & 0xff;
		for (i = 0; i < 8; i++) {
			if ((crc & 1) ^ (data & 1))
				crc = (crc >> 1) ^ POLY;
			else
				crc >>= 1;
			data >>= 1;
		}
	}
	return crc;

}

static void es705_write_sensory_vs_data_block(int type)
{
	int size, size4, i, rc = 0;
	const u8 *data;
	char *buf = NULL;
	unsigned char preamble[4];
	u32 cmd[] = { 0x9017e021, 0x90180001, 0xffffffff };
	u32 cmd_confirm[] = { 0x9017e002, 0x90180001, 0xffffffff };
	unsigned short crc;
	u32 check_cmd;
	u32 check_rspn = 0;

	/* check the type (0 = grammar, 1 = net) */
	if (!type) {
		size = es705_priv.vs_grammar->size;
		data = es705_priv.vs_grammar->data;
	} else {
		size = es705_priv.vs_net->size;
		data = es705_priv.vs_net->data;
		cmd[1] = 0x90180002;
		cmd_confirm[0] = 0x9017e003;
	}

	/* rdb/wdb mode = 1 for the grammar file */
	rc = es705_write_block(&es705_priv, cmd);

	/* Add packet data and CRC and then download */
	buf = kzalloc((size + 2 + 3), GFP_KERNEL);
	if (!buf) {
		dev_err(es705_priv.dev, "%s(): kzalloc fail\n", __func__);
		return;
	}
	memcpy(buf, data, size);

	size4 = size + 2;
	size4 = ((size4 + 3) >> 2) << 2;
	size4 -= 2;

	while (size < size4)
		buf[size++] = 0;

	crc = crc_update(buf, size, 0xffff);
	buf[size++] = (unsigned char)(crc & 0xff);
	crc >>= 8;
	buf[size++] = (unsigned char)(crc & 0xff);

	for (i = 0; i < size; i += BLOCK_PAYLOAD_SIZE) {
		es705_priv.vs_keyword_param_size = size - i;
		if (es705_priv.vs_keyword_param_size > BLOCK_PAYLOAD_SIZE)
			es705_priv.vs_keyword_param_size = BLOCK_PAYLOAD_SIZE;

		preamble[0] = 1;
		preamble[1] = 8;
		preamble[2] = 0;
		preamble[3] = (i == 0) ? 0 : 1;
		memcpy(es705_priv.vs_keyword_param, preamble, 4);
		memcpy(&es705_priv.vs_keyword_param[4], &buf[i],
		       es705_priv.vs_keyword_param_size);
		es705_priv.vs_keyword_param_size += 4;
		es705_write_vs_data_block(&es705_priv);
		memset(es705_priv.vs_keyword_param, 0,
		       ES705_VS_KEYWORD_PARAM_MAX);
	}
	kfree(buf);

	/* verify the download count and the CRC */
	check_cmd = 0x8016e031;
	check_rspn = 0;

	rc = es705_write_then_read(&es705_priv, &check_cmd, sizeof(check_cmd),
				   &check_rspn, 0);
	pr_info("%s: size = %x\n", __func__, check_rspn);

	check_cmd = 0x8016e02a;
	check_rspn = 0x80160000;

	rc = es705_write_then_read(&es705_priv, &check_cmd, sizeof(check_cmd),
				   &check_rspn, 1);
	if (rc)
		dev_err(es705_priv.dev, "%s(): es705 CRC check fail\n",
			__func__);

}

static int es705_write_sensory_vs_keyword(void)
{
	int rc = 0;
	u32 grammar_confirm[] = { 0x9017e002, 0x90180001, 0xffffffff };
	u32 net_confirm[] = { 0x9017e003, 0x90180001, 0xffffffff };
	u32 start_confirm[] = { 0x9017e000, 0x90180001, 0xffffffff };

	dev_info(es705_priv.dev, "%s(): ********* START VS Keyword Download\n",
		 __func__);

	if (abort_request) {
		dev_info(es705_priv.dev, "%s(): Skip to download VS Keyword\n",
			 __func__);
		es705_switch_to_normal_mode(&es705_priv);
		return rc;
	}

	if (es705_priv.rdb_wdb_open) {
		rc = es705_priv.rdb_wdb_open(&es705_priv);
		if (rc < 0) {
			dev_err(es705_priv.dev, "%s(): WDB UART open FAIL\n",
				__func__);
			return rc;
		}
	}

	/* get the grammar file */
	rc = request_firmware((const struct firmware **)&es705_priv.vs_grammar,
			      es705_priv.grammar_path, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev,
			"%s(): request_firmware(%s) failed %d\n", __func__,
			es705_priv.grammar_path, rc);
		return rc;
	}

	/* get the net file */
	rc = request_firmware((const struct firmware **)&es705_priv.vs_net,
			      es705_priv.net_path, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev,
			"%s(): request_firmware(%s) failed %d\n", __func__,
			es705_priv.net_path, rc);
		return rc;
	}

	/* download keyword using WDB */
	es705_write_sensory_vs_data_block(0);
	es705_write_sensory_vs_data_block(1);

	release_firmware(es705_priv.vs_grammar);
	release_firmware(es705_priv.vs_net);

	if (es705_priv.rdb_wdb_close)
		es705_priv.rdb_wdb_close(&es705_priv);

	/* mark the grammar and net as valid */
	rc = es705_write_block(&es705_priv, grammar_confirm);
	rc = es705_write_block(&es705_priv, net_confirm);
	rc = es705_write_block(&es705_priv, start_confirm);

	dev_info(es705_priv.dev, "%s(): ********* END VS Keyword Download\n",
		 __func__);

	return rc;
}
#endif

static ssize_t es705_route_status_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Route Status";

	value = es705_read(NULL, ES705_CHANGE_STATUS);

	ret = snprintf(buf, PAGE_SIZE, "%s=0x%04x\n", status_name, value);

	return ret;
}

static DEVICE_ATTR(route_status, 0444, es705_route_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/route_status */

static ssize_t es705_route_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct es705_priv *es705 = &es705_priv;

	dev_dbg(es705->dev, "%s(): route=%ld\n",
		__func__, es705->internal_route_num);
	return snprintf(buf, PAGE_SIZE, "route=%ld\n",
			es705->internal_route_num);
}

static DEVICE_ATTR(route, 0444, es705_route_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/route */

static ssize_t es705_rate_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct es705_priv *es705 = &es705_priv;

	dev_dbg(es705->dev, "%s(): rate=%ld\n", __func__, es705->internal_rate);
	return snprintf(buf, PAGE_SIZE, "rate=%ld\n", es705->internal_rate);
}

static DEVICE_ATTR(rate, 0444, es705_rate_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/rate */

#define SIZE_OF_VERBUF 256
/* TODO: fix for new read/write. use es705_read() instead of BUS ops */
static ssize_t es705_fw_version_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int idx = 0;
	unsigned int value;
	char versionbuffer[SIZE_OF_VERBUF];
	char *verbuf = versionbuffer;

	memset(verbuf, 0, SIZE_OF_VERBUF);

	if (es705_priv.pm_state == ES705_POWER_AWAKE) {
		value = es705_read(NULL, ES705_FW_FIRST_CHAR);
		*verbuf++ = (value & 0x00ff);
		for (idx = 0; idx < (SIZE_OF_VERBUF - 2); idx++) {
			value = es705_read(NULL, ES705_FW_NEXT_CHAR);
			*verbuf++ = (value & 0x00ff);
		}
		/* Null terminate the string */
		*verbuf = '\0';
		dev_info(dev, "Audience fw ver %s\n", versionbuffer);
	} else
		dev_info(dev, "Audience is not awake\n");

	return snprintf(buf, PAGE_SIZE, "FW Version = %s\n", versionbuffer);
}

static DEVICE_ATTR(fw_version, 0444, es705_fw_version_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/fw_version */

static ssize_t es705_clock_on_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int ret = 0;

	return ret;
}

static DEVICE_ATTR(clock_on, 0444, es705_clock_on_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/clock_on */

static ssize_t es705_vs_keyword_parameters_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;

	if (es705_priv.vs_keyword_param_size > 0) {
		memcpy(buf, es705_priv.vs_keyword_param,
		       es705_priv.vs_keyword_param_size);
		ret = es705_priv.vs_keyword_param_size;
		dev_dbg(dev, "%s(): keyword param size=%hu\n", __func__, ret);
	} else {
		dev_dbg(dev, "%s(): keyword param not set\n", __func__);
	}

	return ret;
}

static ssize_t es705_vs_keyword_parameters_set(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	int ret = 0;

	if (count <= ES705_VS_KEYWORD_PARAM_MAX) {
		memcpy(es705_priv.vs_keyword_param, buf, count);
		es705_priv.vs_keyword_param_size = count;
		dev_dbg(dev, "%s(): keyword param block set size = %zi\n",
			__func__, count);
		ret = count;
	} else {
		dev_dbg(dev, "%s(): keyword param block too big = %zi\n",
			__func__, count);
		ret = -ENOMEM;
	}

	return ret;
}

static DEVICE_ATTR(vs_keyword_parameters, 0644,
		   es705_vs_keyword_parameters_show,
		   es705_vs_keyword_parameters_set);

static ssize_t es705_vs_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Voice Sense Status";
	/* Disable vs status read for interrupt to work */
	struct es705_priv *es705 = &es705_priv;

	mutex_lock(&es705->api_mutex);
	value = es705->vs_get_event;
	/* Reset the detection status after read */
	es705->vs_get_event = NO_EVENT;
	mutex_unlock(&es705->api_mutex);

	ret = snprintf(buf, PAGE_SIZE, "%s=0x%04x\n", status_name, value);

	return ret;
}

static DEVICE_ATTR(vs_status, 0444, es705_vs_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/vs_status */

static ssize_t es705_ping_status_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct es705_priv *es705 = &es705_priv;
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_ack;
	char msg[4];
	char *status_name = "Ping";

	cpu_to_le32s(&sync_cmd);
	memcpy(msg, (char *)&sync_cmd, 4);

	rc = es705->dev_write(es705, msg, 4);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): firmware load failed sync write\n",
			__func__);
	}

	msleep(20);
	memset(msg, 0, 4);

	rc = es705->dev_read(es705, msg, 4);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): error reading sync ack rc=%d\n",
			__func__, rc);
	}

	memcpy((char *)&sync_ack, msg, 4);
	le32_to_cpus(&sync_ack);
	dev_dbg(es705->dev, "%s(): sync_ack = 0x%08x\n", __func__, sync_ack);

	rc = snprintf(buf, PAGE_SIZE, "%s=0x%08x\n", status_name, sync_ack);

	return rc;
}

static DEVICE_ATTR(ping_status, 0444, es705_ping_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/ping_status */

static ssize_t es705_gpio_reset_set(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;

	dev_dbg(es705->dev, "%s(): GPIO reset\n", __func__);

	es705->mode = SBL;
	es705_gpio_reset(es705);

	dev_dbg(es705->dev, "%s(): Ready for STANDARD download by proxy\n",
		__func__);

	return count;
}

static DEVICE_ATTR(gpio_reset, 0644, NULL, es705_gpio_reset_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/gpio_reset */

static ssize_t es705_overlay_mode_set(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;
	int value = ES705_SET_POWER_STATE_VS_OVERLAY;

	dev_dbg(es705->dev, "%s(): Set Overlay mode\n", __func__);

	es705->mode = SBL;

	rc = es705_write(NULL, ES705_POWER_STATE, value);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): Set Overlay mode failed\n",
			__func__);
	} else {
		msleep(50);

		es705_priv.es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;

		/* wait until es705 SBL mode activating */
		dev_info(es705->dev,
			 "%s(): After successful VOICESENSE download,"
			 "Enable Event Intr to Host\n", __func__);
	}

	return count;
}

static DEVICE_ATTR(overlay_mode, 0644, NULL, es705_overlay_mode_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/overlay_mode */

static ssize_t es705_vs_event_set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;
	int value = ES705_SYNC_INTR_RISING_EDGE;

	dev_dbg(es705->dev, "%s(): Enable Voice Sense Event to Host\n",
		__func__);

	es705->mode = VOICESENSE;

	/* Enable Voice Sense Event INTR to Host */
	rc = es705_write(NULL, ES705_EVENT_RESPONSE, value);
	if (rc)
		dev_err(es705->dev, "%s(): Enable Event Intr fail\n", __func__);
	return count;
}

static DEVICE_ATTR(vs_event, 0644, NULL, es705_vs_event_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/vs_event */

static ssize_t es705_tuning_set(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	unsigned int value = 0;

	sscanf(buf, "%d", &value);

	pr_info("%s : [ES705] uart_pin_config = %d\n", __func__, value);

	if (value == 0)
		es705->change_uart_config = 0;
	else
		es705->change_uart_config = 1;

	return count;
}

static DEVICE_ATTR(tuning, 0644, NULL, es705_tuning_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/tuning */
#define PATH_SIZE 100

static ssize_t es705_keyword_grammar_path_set(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count)
{
	char path[PATH_SIZE];
	char *index = 0;

	/* buf can have 100 chars, with an extra terminating '0' */
	if (strlen(buf) > PATH_SIZE - 1) {
		dev_err(es705_priv.dev, "%s(): invalid buf length\n", __func__);
		return count;
	}

	sscanf(buf, "%s", path);
	pr_info("%s : [ES705] grammar path = %s\n", __func__, path);

	memset(es705_priv.grammar_path, 0, sizeof(es705_priv.grammar_path));

	/* replace absolute path with relative path */
	index = strrchr(path, '/');
	if (index) {
		strlcpy(es705_priv.grammar_path, index + 1, strlen(index + 1));
	} else {
		pr_info("%s : [ES705] cannot find /\n", __func__);
		strlcpy(es705_priv.grammar_path, path, strlen(path));
	}

	pr_info("%s : [ES705] keyword_grammar_final_path = %s\n", __func__,
		es705_priv.grammar_path);

	es705_priv.vs_grammar_set_flag = 1;

	return count;
}

static DEVICE_ATTR(keyword_grammar_path, 0664, NULL,
		   es705_keyword_grammar_path_set);

static ssize_t es705_keyword_net_path_set(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	char path[PATH_SIZE];
	char *index = 0;

	/* buf can have 100 chars, with an extra terminating '0' */
	if (strlen(buf) > PATH_SIZE - 1) {
		dev_err(es705_priv.dev, "%s(): invalid buf length\n", __func__);
		return count;
	}

	sscanf(buf, "%s", path);
	pr_info("%s : [ES705] net path = %s\n", __func__, path);

	memset(es705_priv.net_path, 0, sizeof(es705_priv.net_path));

	/* replace absolute path with relative path */
	index = strrchr(path, '/');
	if (index)
		strlcpy(es705_priv.net_path, index + 1, strlen(index + 1));
	else {
		pr_info("%s : [ES705] cannot find /\n", __func__);
		strlcpy(es705_priv.net_path, path, strlen(path));
	}

	pr_info("%s : [ES705] keyword_net_final_path = %s\n", __func__,
		es705_priv.net_path);

	es705_priv.vs_net_set_flag = 1;

	return count;
}

static DEVICE_ATTR(keyword_net_path, 0664, NULL, es705_keyword_net_path_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/keyword_net_path */
static ssize_t es705_sleep_delay_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", es705_priv.sleep_delay);
	return ret;
}

static ssize_t es705_sleep_delay_set(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int rc;

	rc = kstrtoint(buf, 0, &es705_priv.sleep_delay);

	dev_info(es705_priv.dev, "%s(): sleep delay = %d\n", __func__,
		 es705_priv.sleep_delay);

	return count;
}

static DEVICE_ATTR(sleep_delay, 0644, es705_sleep_delay_show,
		   es705_sleep_delay_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/sleep_delay */

static ssize_t es705_preset_delay_time_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", preset_delay_time);

	return ret;
}

static ssize_t es705_preset_delay_time_set(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	int rc;

	rc = kstrtoint(buf, 0, &preset_delay_time);
	dev_info(es705_priv.dev, "%s(): preset_delay_time = %d\n",
		 __func__, preset_delay_time);

	return count;
}

static DEVICE_ATTR(preset_delay, 0644, es705_preset_delay_time_show,
		   es705_preset_delay_time_set);

static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_route.attr,
	&dev_attr_rate.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_vs_keyword_parameters.attr,
	&dev_attr_vs_status.attr,
	&dev_attr_ping_status.attr,
	&dev_attr_gpio_reset.attr,
	&dev_attr_overlay_mode.attr,
	&dev_attr_vs_event.attr,
	&dev_attr_tuning.attr,
	&dev_attr_keyword_grammar_path.attr,
	&dev_attr_keyword_net_path.attr,
	&dev_attr_sleep_delay.attr,
	&dev_attr_preset_delay.attr,
	NULL
};

static struct attribute_group core_sysfs = {
	.attrs = core_sysfs_attrs
};

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
static ssize_t es705_keyword_type_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct es705_priv *es705 = &es705_priv;
	int ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", es705->keyword_type);
	return ret;
}

static ssize_t es705_keyword_type_set(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;
	rc = kstrtoint(buf, 0, &(es705->keyword_type));
	dev_info(es705_priv.dev, "%s(): keyword_type = %d\n",
		 __func__, es705->keyword_type);
	return count;
}

static DEVICE_ATTR(keyword_type, 0644, es705_keyword_type_show,
		   es705_keyword_type_set);
#endif

#if defined(CONFIG_SND_SOC_ES_I2C)
static ssize_t es705_power_control_set(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned int value = 0;
	int ret;

	ret = sscanf(buf, "%d", &value);

	dev_info(es705_priv.dev, "%s: power control=%d\n", __func__, value);

	if (value == 0) {
		if (es705_priv.power_control) {
			es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
							ES705_POWER_STATE);
		}
	} else {
		if (es705_priv.power_control) {
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
							ES705_POWER_STATE);
		}
	}

	return count;
}

static DEVICE_ATTR(power_control_set, 0664, NULL, es705_power_control_set);

static ssize_t es705_veq_control_set(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int value = 0;
	int ret = 0;

	ret = sscanf(buf, "%d", &value);

	dev_dbg(es705_priv.dev, "%s(): VEQ New Volume = %d\n", __func__, value);

	if (es705_priv.pm_state != ES705_POWER_AWAKE) {
		dev_info(es705_priv.dev,
		"%s(): can't do veq volume setting pm_state(%d)\n",
		__func__, es705_priv.pm_state);
		return count;
	}

	es705_priv.current_veq_preset = 1;
	ret = es705_put_veq_block(value);
	if (ret < 0)
		dev_err(es705_priv.dev, "%s(): Veq setting failed\n",
				__func__);

	return count;
}

static DEVICE_ATTR(veq_control_set, 0664, NULL, es705_veq_control_set);

static ssize_t es705_route_value_set(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	long value = 0;
	int ret;

	ret = sscanf(buf, "%ld", &value);

	dev_info(es705->dev, "%s: route=%ld pm_state=%d power_state=%d\n",
		__func__, value, es705->pm_state, es705->es705_power_state);

	if (value == ROUTE_MAX) {
#if defined(CHECK_ROUTE_STATUS_AND_RECONFIG)
		if (delayed_work_pending(&chk_route_st_and_recfg_work)) {
			dev_info(es705->dev,
				"%s(): cancel_delayed_work(chk_route_st_and_recfg_work)\n",
				__func__);
			cancel_delayed_work_sync(&chk_route_st_and_recfg_work);
		}
#endif
		/* Check DHWPT interface */
		if (es705->pdata->use_dhwpt && dhwpt_is_enabled)
			es705_enable_dhwpt(0);

		dev_info(es705->dev, "%s: going to sleep\n", __func__);
		es705->internal_route_num = value;
		if (es705_priv.power_control) {
			es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
							ES705_POWER_STATE);
		}
		return count;
	}

	if (delayed_work_pending(&es705->sleep_work) ||
	    (es705->pm_state == ES705_POWER_SLEEP_PENDING)) {
		dev_info(es705->dev, "%s: cancel sleep_work\n", __func__);
		cancel_delayed_work_sync(&es705->sleep_work);

		if (es705->pm_state == ES705_POWER_SLEEP_PENDING)
			es705->pm_state = ES705_POWER_AWAKE;
	}

	/* Check DHWPT interface */
	if (es705->pdata->use_dhwpt && dhwpt_is_enabled) {
		if (es705_chk_route_id_for_dhwpt(value)) {
			es705->internal_route_num = value;
			es705->current_veq = -1;
			return count;
		}
	}

	if (es705->es705_power_state != ES705_SET_POWER_STATE_NORMAL) {
		if (es705_priv.power_control) {
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
							ES705_POWER_STATE);
		}
	}

	es705_switch_route(value);

	return count;
}

static DEVICE_ATTR(route_value, 0664, NULL, es705_route_value_set);

static ssize_t es705_extra_volume_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)

{
	struct es705_priv *es705 = &es705_priv;
	int value = 0;
	int ret;

	ret = sscanf(buf, "%d", &value);

	dev_info(es705->dev, "%s(): extra volume = %d pm state = %d\n",
				__func__, value, es705->pm_state);

	if (es705->pm_state != ES705_POWER_AWAKE) {
		dev_info(es705->dev,
		"%s(): can't do extra volume setting pm_state(%d)\n",
		__func__, es705->pm_state);
		return count;
	}
	if (value == extra_volume) {
		dev_info(es705->dev,
			"%s(): same volume with previous,skip\n", __func__);
		return count;
	}
	extra_volume = value;
	es705->current_veq = -1;
	return count;
}

static DEVICE_ATTR(extra_volume, 0664, NULL, es705_extra_volume_set);

static struct attribute *earsmart_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_route.attr,
	&dev_attr_rate.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_vs_keyword_parameters.attr,
	&dev_attr_vs_status.attr,
	&dev_attr_ping_status.attr,
	&dev_attr_gpio_reset.attr,
	&dev_attr_overlay_mode.attr,
	&dev_attr_vs_event.attr,
	&dev_attr_tuning.attr,
	&dev_attr_keyword_grammar_path.attr,
	&dev_attr_keyword_net_path.attr,
	&dev_attr_sleep_delay.attr,
	&dev_attr_preset_delay.attr,
	&dev_attr_power_control_set.attr,
	&dev_attr_veq_control_set.attr,
	&dev_attr_route_value.attr,
	&dev_attr_extra_volume.attr,
	NULL
};

static struct attribute_group earsmart_attr_group = {
	.attrs	= earsmart_sysfs_attrs,
};
#endif

static int es705_fw_download(struct es705_priv *es705, int fw_type)
{
	int rc = 0;

	dev_info(es705->dev, "%s(): fw download type %d begin\n", __func__,
		 fw_type);

	mutex_lock(&es705->api_mutex);

	if (fw_type != VOICESENSE && fw_type != STANDARD) {
		dev_err(es705->dev, "%s(): Unexpected FW type\n", __func__);
		goto es705_fw_download_exit;
	}

	if (es705->uart_fw_download_rate) {
		if (es705->uart_fw_download
		    && es705->uart_fw_download(es705, fw_type) >= 0) {
			es705->mode = fw_type;
		} else {
			dev_err(es705->dev, "%s(): UART fw download failed\n",
				__func__);
			rc = -EIO;
		}
	} else {

		rc = es705->boot_setup(es705);
		if (rc) {
			dev_err(es705->dev, "%s(): fw download start error\n",
				__func__);
			goto es705_fw_download_exit;
		}

		dev_info(es705->dev, "%s(): write firmware image\n", __func__);

		if (fw_type == VOICESENSE)
			rc = es705->dev_write(es705, (char *)es705->vs->data,
					      es705->vs->size);
		else
			rc = es705->dev_write(es705,
					      (char *)es705->standard->data,
					      es705->standard->size);
		if (rc) {
			dev_err(es705->dev,
				"%s(): firmware image write error\n", __func__);
			rc = -EIO;
			goto es705_fw_download_exit;
		}

		es705->mode = fw_type;
		rc = es705->boot_finish(es705);
		if (rc) {
			dev_err(es705->dev, "%s() fw download finish error\n",
				__func__);
			goto es705_fw_download_exit;
		}
		dev_info(es705->dev, "%s(): fw download type %d done\n",
			 __func__, fw_type);
	}

es705_fw_download_exit:
	mutex_unlock(&es705->api_mutex);
	return rc;
}

int es705_bootup(struct es705_priv *es705)
{
	int rc;
	int fw_type = STANDARD;

#if defined(CONFIG_SND_SOC_ESXXX_STAND_ALONE_VS_FW)
	fw_type = VOICESENSE;
	BUG_ON(es705->vs->size == 0);
#else
	BUG_ON(es705->standard->size == 0);
#endif

	mutex_lock(&es705->pm_mutex);
	es705->pm_state = ES705_POWER_FW_LOAD;
	mutex_unlock(&es705->pm_mutex);

	rc = es705_fw_download(es705, fw_type);
	if (rc) {
		dev_err(es705->dev, "%s(): STANDARD fw download error\n",
			__func__);
	} else {
		mutex_lock(&es705->pm_mutex);
		es705->pm_state = ES705_POWER_AWAKE;
#if defined(SAMSUNG_ES705_FEATURE)
		es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
#endif
		mutex_unlock(&es705->pm_mutex);
	}

	return rc;
}

static int es705_set_lpm(struct es705_priv *es705)
{
	int rc;
	const int max_retry_to_switch_to_lpm = 5;
	int retry = max_retry_to_switch_to_lpm;

	rc = es705_write(NULL, ES705_VS_INT_OSC_MEASURE_START, 0);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): OSC Measure Start fail\n",
			__func__);
		goto es705_set_lpm_exit;
	}

	do {
		/* Wait 20ms before reading up to 5 times (total 100ms) */
		msleep(20);

		rc = es705_read(NULL, ES705_VS_INT_OSC_MEASURE_STATUS);
		if (rc < 0) {
			dev_err(es705_priv.dev,
				"%s(): OSC Measure Read Status fail\n",
				__func__);
			goto es705_set_lpm_exit;
		}
		dev_dbg(es705_priv.dev, "%s(): OSC Measure Status = 0x%04x\n",
			__func__, rc);
	} while (rc && --retry);

	if (rc) {
		dev_err(es705_priv.dev, "%s(): OSC Measure Read Status fail\n",
			__func__);
		goto es705_set_lpm_exit;
	}

	dev_dbg(es705_priv.dev, "%s(): Activate Low Power Mode\n", __func__);

	rc = es705_write(NULL, ES705_POWER_STATE,
			 ES705_SET_POWER_STATE_VS_LOWPWR);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): Write cmd fail\n", __func__);
		goto es705_set_lpm_exit;
	}

	es705_priv.es705_power_state = ES705_SET_POWER_STATE_VS_LOWPWR;

	if (es705_priv.pdata->esxxx_clk_cb) {
		/* ext clock off */
		es705_priv.pdata->esxxx_clk_cb(es705_priv.dev, 0);
		dev_info(es705_priv.dev, "%s(): external clock off\n",
			 __func__);
	}

es705_set_lpm_exit:
	return rc;
}

static int es705_vs_load(struct es705_priv *es705)
{
	int rc = 0;
#ifdef CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW
	u32 cmd =
	    (ES705_SET_POWER_STATE << 16) | ES705_SET_POWER_STATE_VS_OVERLAY;
#endif
	BUG_ON(es705->vs->size == 0);

	/* wait es705 SBL mode */
	msleep(50);

	es705->es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;

	mutex_lock(&es705->pm_mutex);
	rc = es705_fw_download(es705, VOICESENSE);
#ifdef CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW
	if (rc < 0) {
		mutex_unlock(&es705->pm_mutex);
		es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
		rc = restore_std_fw(es705);
		rc = es705_cmd(es705, cmd);
		msleep(100);
		es705->es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;
		rc = es705_fw_download(es705, VOICESENSE);
		cnt_restore_std_fw_in_vs++;
		mutex_lock(&es705->pm_mutex);
	}

	if (cnt_restore_std_fw_in_vs) {
		dev_info(es705->dev, "%s(): cnt_restore_std_fw_in_vs = %d\n",
			 __func__, cnt_restore_std_fw_in_vs);
	}
#endif /* CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW */
	mutex_unlock(&es705->pm_mutex);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): FW download fail\n", __func__);
		goto es705_vs_load_fail;
	}

	msleep(50);

	/* Enable Voice Sense Event INTR to Host */
	rc = es705_write(NULL, ES705_EVENT_RESPONSE,
			 ES705_SYNC_INTR_RISING_EDGE);
	if (rc) {
		dev_err(es705->dev, "%s(): Enable Event Intr fail\n", __func__);
		goto es705_vs_load_fail;
	}

es705_vs_load_fail:
	return rc;
}

#if defined(CONFIG_SND_SOC_ES_SLIM)
static int register_snd_soc(struct es705_priv *priv);
#endif

int fw_download(void *arg)
{
	struct es705_priv *priv = (struct es705_priv *)arg;
	int rc;

	dev_dbg(priv->dev, "%s(): fw download\n", __func__);

	rc = es705_bootup(priv);
	if (rc)
		dev_err(priv->dev, "%s(): bootup failed(%d)\n", __func__, rc);
#if defined(CONFIG_SND_SOC_ES_SLIM)
	rc = register_snd_soc(priv);
	if (rc) {
		dev_err(priv->dev, "%s(): register_snd_soc failed(%d)\n",
			__func__, rc);
	}
#endif
	dev_info(priv->dev, "%s(): release module\n", __func__);
	module_put(THIS_MODULE);

	return 0;
}

#if !defined(CONFIG_SLIMBUS_MSM_NGD)
#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
static void es705_vs_fw_loaded(const struct firmware *fw, void *context)
{
	struct es705_priv *es705 = (struct es705_priv *)context;
	int rc = 0;
	dev_info(es705->dev, "%s:\n", __func__);
	if (!fw) {
		dev_err(es705->dev, "firmware request failed\n");
		goto request_firmware_error;
	}
	es705->vs = fw;
	dev_info(es705->dev, "%s: Done!\n", __func__);

request_firmware_error:
	es705->vs = NULL;
}


#endif
static void es705_std_fw_loaded(const struct firmware *fw, void *context)
{
	struct es705_priv *es705 = (struct es705_priv *)context;
	int rc = 0;
	dev_info(es705->dev, "%s:\n", __func__);
	if (!fw) {
		dev_err(es705->dev, "firmware request failed\n");
		goto request_firmware_error;
	}

	/* enable clk and reset */
	if (es705->pdata->esxxx_clk_cb) {
		es705->pdata->esxxx_clk_cb(es705->dev, 1);
		dev_info(es705->dev, "%s(): external clock on\n", __func__);
	}
	usleep_range(5000, 5100);
	es705_gpio_reset(es705);

	es705->standard = fw;
	es705_priv.pm_state = ES705_POWER_FW_LOAD;
	es705_priv.fw_requested = 1;

	rc = es705_bootup(&es705_priv);
	if (rc) {
		dev_err(es705->dev, "%s(): es705_bootup failed %d\n",
			__func__, rc);
		goto err;
	}
	dev_info(es705->dev, "%s: Done!\n", __func__);
err:
	release_firmware(es705->standard);
request_firmware_error:
	es705->standard = NULL;

	if (es705_priv.power_control) {
		dev_info(es705->dev, "%s(): is going to sleep\n", __func__);
		es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
					ES705_POWER_STATE);
	}
}
#endif

/* Hold the pm_mutex before calling this function */
#define CLK_OFF_DELAY 30

static int es705_sleep(struct es705_priv *es705)
{
	u32 cmd = (ES705_SET_SMOOTH << 16) | ES705_SET_SMOOTH_RATE;
	int rc;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_rspn = sync_cmd;
	int match = 1;

	dev_info(es705->dev, "%s\n", __func__);

	mutex_lock(&es705->pm_mutex);

	es705->current_veq_preset = 0;
	es705->current_veq = -1;
	es705->current_bwe = 0;

	rc = es705_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
				   &sync_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): send sync command failed rc = %d\n",
			__func__, rc);
#ifdef CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW
	if (rc) {
		mutex_unlock(&es705->pm_mutex);
		rc = restore_std_fw(es705);
		cnt_restore_std_fw_in_sleep++;
		mutex_lock(&es705->pm_mutex);
	}

	if (cnt_restore_std_fw_in_sleep)
		dev_info(es705->dev, "%s(): cnt_restore_std_fw_in_sleep = %d\n",
			 __func__, cnt_restore_std_fw_in_sleep);
#endif /* CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW */

	/* Avoid smoothing time */
	rc = es705_cmd(es705, cmd);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): Reset Smooth Rate Fail", __func__);
		goto es705_sleep_exit;
	}
#ifdef AUDIENCE_VS_IMPLEMENTATION
	if (es705->voice_wakeup_enable) {
		rc = es705_switch_to_vs_mode(&es705_priv);
		if (rc) {
			dev_err(es705->dev, "%s(): Set VS Overlay FAIL",
				__func__);
		} else {
			/* Set PDM route */
			es705_switch_route(22);
			msleep(20);
			rc = es705_set_lpm(es705);
			if (rc) {
				dev_err(es705->dev, "%s(): Set LPM FAIL",
					__func__);
			} else {
				es705->pm_state = ES705_POWER_SLEEP;
				goto es705_sleep_exit;
			}
		}
	}
#endif
	/*
	 * write Set Power State Sleep - 0x9010_0001
	 * wait 20 ms, and then turn ext clock off
	 * There will not be any response after
	 * sleep command from chip
	 */
	cmd = (ES705_SET_POWER_STATE << 16) | ES705_SET_POWER_STATE_SLEEP;

	rc = es705_cmd(es705, cmd);
	if (rc)
		dev_err(es705->dev, "%s(): send sleep command failed rc = %d\n",
			__func__, rc);

	dev_dbg(es705->dev, "%s: wait %dms for execution\n", __func__,
		CLK_OFF_DELAY);

	msleep(CLK_OFF_DELAY);

	es705->es705_power_state = ES705_SET_POWER_STATE_SLEEP;
	es705->pm_state = ES705_POWER_SLEEP;

	if (es705->pdata->esxxx_clk_cb) {
		es705->pdata->esxxx_clk_cb(es705->dev, 0);
		dev_info(es705->dev, "%s(): external clock off\n", __func__);
	}

es705_sleep_exit:
	dev_info(es705->dev, "%s(): Exit\n", __func__);
	mutex_unlock(&es705->pm_mutex);

	return rc;
}

static void es705_delayed_sleep(struct work_struct *w)
{
	int ch_tot;
	int ports_active = (es705_priv.rx1_route_enable ||
			    es705_priv.rx2_route_enable ||
			    es705_priv.tx1_route_enable ||
			    es705_priv.voice_wakeup_enable);
	/*
	 * If there are active streams we do not sleep.
	 * Count the front end (FE) streams ONLY.
	 */
	ch_tot = 0;
	ch_tot += es705_priv.dai[ES705_SLIM_1_PB].ch_tot;
	ch_tot += es705_priv.dai[ES705_SLIM_2_PB].ch_tot;
	ch_tot += es705_priv.dai[ES705_SLIM_1_CAP].ch_tot;

	dev_dbg(es705_priv.dev, "%s %d active channels, ports_active: %d\n",
		__func__, ch_tot, ports_active);

/*	mutex_lock(&es705_priv.pm_mutex); */
	if ((ch_tot <= 0) && (ports_active == 0)
	    && (es705_priv.pm_state == ES705_POWER_SLEEP_PENDING)) {
		es705_sleep(&es705_priv);
	}
/*	mutex_unlock(&es705_priv.pm_mutex); */
}

static void es705_sleep_request(struct es705_priv *es705)
{
	dev_dbg(es705->dev, "%s internal es705_power_state = %d\n", __func__,
		es705_priv.pm_state);

#if defined(CHECK_ROUTE_STATUS_AND_RECONFIG)
	if (delayed_work_pending(&chk_route_st_and_recfg_work)) {
		dev_info(es705->dev,
			"%s(): cancel_delayed_work(chk_route_st_and_recfg_work)\n",
			__func__);
		cancel_delayed_work_sync(&chk_route_st_and_recfg_work);
	}
#endif

	mutex_lock(&es705->pm_mutex);
	if (es705->uart_state == UART_OPEN)
		es705_uart_close(es705);

	if (es705->pm_state == ES705_POWER_AWAKE) {
		schedule_delayed_work(&es705->sleep_work,
				      msecs_to_jiffies(es705->sleep_delay));
		es705->pm_state = ES705_POWER_SLEEP_PENDING;
	}
	mutex_unlock(&es705->pm_mutex);
}

#define SYNC_DELAY 35
#define MAX_RETRY_WAKEUP_CNT 1

static int es705_wakeup(struct es705_priv *es705)
{
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_rspn = sync_cmd;
	int match = 1;
#ifndef CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW
	int retry = 0;
	int retry_wakeup_cnt = 0;
#endif
	dev_info(es705->dev, "%s\n", __func__);
	/* 1 - clocks on
	 * 2 - wakeup 1 -> 0
	 * 3 - sleep 30 ms
	 * 4 - Send sync command (0x8000, 0x0001)
	 * 5 - Read sync ack
	 * 6 - wakeup 0 -> 1
	 */

	mutex_lock(&es705->pm_mutex);

#if defined(CONFIG_SLIMBUS_MSM_NGD)
	if (es705->wakeup_bus)
		es705->wakeup_bus(es705);
#endif

	if (delayed_work_pending(&es705->sleep_work) ||
	    (es705->pm_state == ES705_POWER_SLEEP_PENDING)) {
		cancel_delayed_work_sync(&es705->sleep_work);
		es705->pm_state = ES705_POWER_AWAKE;
		goto es705_wakeup_exit;
	}

	/* Check if previous power state is not sleep then return */
	if (es705->pm_state != ES705_POWER_SLEEP) {
		dev_err(es705->dev, "%s(): no need to go to Normal Mode\n",
			__func__);
		goto es705_wakeup_exit;
	}

	if (es705->pdata->esxxx_clk_cb) {
		es705->pdata->esxxx_clk_cb(es705->dev, 1);
		usleep_range(3000, 3100);
		dev_info(es705->dev, "%s(): external clock on\n", __func__);
	}
#ifndef CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW
RETRY_TO_WAKEUP:
#endif

#if defined(CONFIG_SND_SOC_ESXXX_UART_WAKEUP)
	if (es705->change_uart_config)
		es705_uart_pin_preset(es705);

	es705_uart_es705_wakeup(es705);

	if (es705->change_uart_config)
		es705_uart_pin_postset(es705);
#else
	es705_gpio_wakeup(es705);
#endif
	msleep(SYNC_DELAY);

#ifdef CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW
	rc = es705_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
				   &sync_rspn, match);
	if (rc) {
		mutex_unlock(&es705->pm_mutex);
		rc = restore_std_fw(es705);
		cnt_restore_std_fw_in_wakeup++;
		mutex_lock(&es705->pm_mutex);
	}

	if (cnt_restore_std_fw_in_wakeup)
		dev_info(es705->dev,
			 "%s(): cnt_restore_std_fw_in_wakeup = %d\n", __func__,
			 cnt_restore_std_fw_in_wakeup);
#else /* !CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW */
	/* retry wake up */
	retry = 0;
	do {
		rc = es705_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
					   &sync_rspn, match);
		if (rc) {
			dev_info(es705->dev,
				 "%s(): wait %dms wakeup, then ping SYNC to es705, retry(%d) rspn=0x%08x\n",
				 __func__, SYNC_DELAY, retry, sync_rspn);
			msleep(SYNC_DELAY);
			sync_rspn = sync_cmd;
		} else
			break;
	} while (++retry < MAX_RETRY_WAKEUP_CNT);

	/* if wakeup fail Retry wakeup pin falling */
	if (rc) {
		if (retry_wakeup_cnt++ < MAX_RETRY_WAKEUP_CNT) {
			dev_info(es705->dev,
				 "%s(): Retry wakeup pin falling (%d)\n",
				 __func__, retry_wakeup_cnt);
			usleep_range(10000, 11000);
			goto RETRY_TO_WAKEUP;
		}
	}
#endif /* CONFIG_SND_SOC_ESXXX_RESTORE_STD_FW */

	if (rc) {
		dev_err(es705->dev, "%s(): es705 wakeup FAIL\n", __func__);

		if (es705->pdata->esxxx_clk_cb) {
			es705->pdata->esxxx_clk_cb(es705->dev, 0);
			usleep_range(3000, 3100);
			dev_info(es705->dev, "%s(): external clock off\n",
				 __func__);
		}
		goto es705_wakeup_exit;
	} else {
		es705->pm_state = ES705_POWER_AWAKE;
		es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
		es705->mode = STANDARD;
	}

	dev_dbg(es705->dev, "%s(): wakeup success, SYNC response 0x%08x\n",
		__func__, sync_rspn);

es705_wakeup_exit:
#if !defined(CONFIG_SND_SOC_ESXXX_UART_WAKEUP)
	es705_gpio_wakeup_deassert(es705);
#endif
	dev_info(es705->dev, "%s(): Exit\n", __func__);
	mutex_unlock(&es705->pm_mutex);
	return rc;
}

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
static void es705_read_write_power_control(int read_write)
{
	if (read_write) {
		es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
					 ES705_POWER_STATE);
	} else
	    if (!
		(es705_priv.rx1_route_enable || es705_priv.rx2_route_enable
		 || es705_priv.tx1_route_enable
		 || es705_priv.voice_wakeup_enable)) {
		es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
					 ES705_POWER_STATE);
	}
}
#endif

static int es705_put_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = es705_priv.codec; */
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	size_t value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];
	rc = es705_write(NULL, reg, value);

	return 0;
}

static int es705_get_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = es705_priv.codec; */
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = 0;

	if (es705_priv.rx1_route_enable ||
	    es705_priv.tx1_route_enable || es705_priv.rx2_route_enable) {
		value = es705_read(NULL, reg);
	}
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int es705_put_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];
	rc = es705_write(NULL, reg, value);

	return 0;
}

static int es705_get_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value = 0;

	if (es705_priv.rx1_route_enable ||
	    es705_priv.tx1_route_enable || es705_priv.rx2_route_enable) {
		value = es705_read(NULL, reg);
	}
	ucontrol->value.enumerated.item[0] = value;
	return 0;
}

static int es705_get_power_control_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value;

	value = es705_priv.es705_power_state;
	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int es705_get_uart_fw_download_rate(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.uart_fw_download_rate;
	return 0;
}

static int es705_put_uart_fw_download_rate(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	/*
	 * 0 - no use UART
	 * 1 - use UART for FW download. Baud Rate 4.608 KBps
	 * 2 - use UART for FW download. Baud Rate 1 MBps
	 * 3 - use UART for FW download. Baud Rate 3 Mbps
	 */
	es705_priv.uart_fw_download_rate = ucontrol->value.enumerated.item[0];
	return 0;
}

static int es705_get_vs_stream_enable(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.vs_stream_enable;
	return 0;
}

static int es705_put_vs_stream_enable(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.vs_stream_enable = ucontrol->value.enumerated.item[0];
	return 0;
}

#if defined(SAMSUNG_ES705_FEATURE)
static int es705_sleep_power_control(unsigned int value, unsigned int reg)
{
	int rc = 0;
	u32 cmd_stop[] = { 0x9017e000, 0x90180000, 0xffffffff };

	switch (es705_priv.es705_power_state) {
	case ES705_SET_POWER_STATE_SLEEP:
		if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			es705_priv.es705_power_state =
			    ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
			es705_priv.pm_state = ES705_POWER_AWAKE;
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
#if !defined(CONFIG_SND_SOC_ESXXX_STAND_ALONE_VS_FW)
			rc = es705_switch_to_vs_mode(&es705_priv);
#endif
		}
		break;

	case ES705_SET_POWER_STATE_NORMAL:
	case ES705_SET_POWER_STATE_VS_OVERLAY:
		return -EINVAL;

	case ES705_SET_POWER_STATE_VS_LOWPWR:
		if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			es705_switch_to_normal_mode(&es705_priv);
			/* Wait for 100ms to switch from Overlay mode */
			msleep(100);
			es705_priv.es705_power_state =
			    ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}

			/* stop interrupt */
			es705_write_block(&es705_priv, cmd_stop);

			es705_priv.es705_power_state =
			    ES705_SET_POWER_STATE_VS_OVERLAY;
			es705_priv.pm_state = ES705_POWER_AWAKE;
		}
		break;

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
	case ES705_SET_POWER_STATE_VS_STREAMING:
		if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			es705_switch_vs_streaming_to_normal_mode(&es705_priv);
			rc = es705_switch_to_vs_mode(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			es705_switch_vs_streaming_to_normal_mode(&es705_priv);
		} else
			dev_info(es705_priv.dev, "%s(): nothing to do\n",
				 __func__);
		break;
#endif

	default:
		return -EINVAL;
	}

	return rc;
}

static int es705_sleep_pending_power_control(unsigned int value,
					     unsigned int reg)
{
	int rc = 0;
	int retry = 10;

	switch (es705_priv.es705_power_state) {
	case ES705_SET_POWER_STATE_SLEEP:
		return -EINVAL;

	case ES705_SET_POWER_STATE_NORMAL:
		if (value == ES705_SET_POWER_STATE_SLEEP) {
			es705_sleep_request(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			es705_priv.es705_power_state =
			    ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
			es705_priv.pm_state = ES705_POWER_AWAKE;
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			rc = es705_switch_to_vs_mode(&es705_priv);
			if (rc < 0)
				return rc;
		}
		break;

	case ES705_SET_POWER_STATE_VS_OVERLAY:
		if (value == ES705_SET_POWER_STATE_SLEEP) {
			es705_sleep_request(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			rc = es705_write(NULL, reg, value);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): Power state command write failed\n",
					__func__);
				return rc;
			}
			/* Wait for 100ms to switch from Overlay mode */
			msleep(100);
			es705_priv.es705_power_state =
			    ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
		} else if (value == ES705_SET_POWER_STATE_VS_LOWPWR) {
			rc = es705_write(NULL, ES705_VS_INT_OSC_MEASURE_START,
					 0);
			do {
				/* Wait 20ms each time before reading,
				   Status may take 100ms to be done
				   added retries */
				msleep(20);
				rc = es705_read(NULL,
					ES705_VS_INT_OSC_MEASURE_STATUS);
				if (rc == 0) {
					dev_dbg(es705_priv.dev,
						"%s(): Activate Low Power Mode\n",
						__func__);
					es705_write(NULL, reg, value);
					es705_priv.es705_power_state =
					    ES705_SET_POWER_STATE_VS_LOWPWR;
					es705_priv.pm_state = ES705_POWER_SLEEP;
					return rc;
				}
			} while (retry--);
		}
		break;

	case ES705_SET_POWER_STATE_VS_LOWPWR:
	default:
		return -EINVAL;
	}

	return rc;
}

static int es705_awake_power_control(unsigned int value, unsigned int reg)
{
	int rc = 0;
	int retry = 10;

	switch (es705_priv.es705_power_state) {
	case ES705_SET_POWER_STATE_SLEEP:
		return -EINVAL;

	case ES705_SET_POWER_STATE_NORMAL:
		if (value == ES705_SET_POWER_STATE_SLEEP) {
			es705_sleep_request(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			rc = es705_switch_to_vs_mode(&es705_priv);
			if (rc < 0)
				return rc;
		} else {
			return -EINVAL;
		}
		break;

	case ES705_SET_POWER_STATE_VS_OVERLAY:
		if (value == ES705_SET_POWER_STATE_SLEEP) {
			es705_sleep_request(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_write(NULL, reg, value);
			if (rc < 0) {
				dev_err(es705_priv.dev,
					"%s(): Power state command write failed\n",
					__func__);
				return rc;
			}
			/* Wait for 100ms to switch from Overlay mode */
			msleep(100);
			es705_priv.es705_power_state =
			    ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
		} else if (value == ES705_SET_POWER_STATE_VS_LOWPWR) {
			rc = es705_write(NULL, ES705_VS_INT_OSC_MEASURE_START,
					 0);

			do {
				/* Wait 20ms each time before reading,
				   Status may take 100ms to be done
				   added retries */
				msleep(20);
				rc = es705_read(NULL,
					ES705_VS_INT_OSC_MEASURE_STATUS);
				if (rc == 0) {
					dev_dbg(es705_priv.dev,
						"%s(): Activate Low Power Mode\n",
						__func__);
					es705_write(NULL, reg, value);

					/* Disable external clock */
					msleep(20);
					/* clocks off */
					if (es705_priv.pdata->esxxx_clk_cb) {
						es705_priv.pdata->
						    esxxx_clk_cb(es705_priv.dev,
								 0);
						dev_info(es705_priv.dev,
							 "%s(): external clock off\n",
							 __func__);
					}
					es705_priv.es705_power_state =
					    ES705_SET_POWER_STATE_VS_LOWPWR;
					es705_priv.pm_state = ES705_POWER_SLEEP;
					return rc;
				}
			} while (retry--);
		}
		break;

	case ES705_SET_POWER_STATE_VS_LOWPWR:
		dev_dbg(es705_priv.dev, "%s(): Set Overlay mode\n", __func__);
		es705_priv.mode = VOICESENSE;
		es705_priv.es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;
		break;

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
	case ES705_SET_POWER_STATE_VS_STREAMING:
		if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			es705_switch_vs_streaming_to_normal_mode(&es705_priv);
			rc = es705_switch_to_vs_mode(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			es705_switch_vs_streaming_to_normal_mode(&es705_priv);
		} else
			dev_info(es705_priv.dev, "%s(): nothing to do\n",
				 __func__);
		break;
#endif

	default:
		return -EINVAL;
	}
	return rc;
}

static int es705_power_control(unsigned int value, unsigned int reg)
{
	int rc = 0;

	dev_info(es705_priv.dev,
		 "%s(): entry pm state %d es705 state %d value %d\n", __func__,
		 es705_priv.pm_state, es705_priv.es705_power_state, value);

	if (value == 0 || value == ES705_SET_POWER_STATE_MP_SLEEP ||
	    value == ES705_SET_POWER_STATE_MP_CMD) {
		dev_err(es705_priv.dev, "%s(): Unsupported value in es705\n",
			__func__);
		return -EINVAL;
	}

	switch (es705_priv.pm_state) {
	case ES705_POWER_FW_LOAD:
		return -EINVAL;

	case ES705_POWER_SLEEP:
		rc = es705_sleep_power_control(value, reg);
		break;

	case ES705_POWER_SLEEP_PENDING:
		rc = es705_sleep_pending_power_control(value, reg);
		break;

	case ES705_POWER_AWAKE:
		rc = es705_awake_power_control(value, reg);
		break;

	default:
		dev_err(es705_priv.dev,
			"%s(): Unsupported pm state [%d] in es705\n", __func__,
			es705_priv.pm_state);
		break;
	}

	dev_info(es705_priv.dev,
		 "%s(): exit pm state %d es705 state %d value %d\n", __func__,
		 es705_priv.pm_state, es705_priv.es705_power_state, value);

	return rc;
}
#endif

#define MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE	5
static int es705_put_power_control_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];
	dev_dbg(es705_priv.dev,
		"%s(): Previous power state = %s, power set cmd = %s\n",
		__func__, power_state[es705_priv.pm_state],
		power_state_cmd[es705_priv.es705_power_state]);
	dev_dbg(es705_priv.dev, "%s(): Requested power set cmd = %s\n",
		__func__, power_state_cmd[value]);

	if (value == 0 || value == ES705_SET_POWER_STATE_MP_SLEEP ||
	    value == ES705_SET_POWER_STATE_MP_CMD) {
		dev_err(es705_priv.dev, "%s(): Unsupported state in es705\n",
			__func__);
		rc = -EINVAL;
		goto es705_put_power_control_enum_exit;
	} else {
		if ((es705_priv.pm_state == ES705_POWER_SLEEP) &&
		    (value != ES705_SET_POWER_STATE_NORMAL) &&
		    (value != ES705_SET_POWER_STATE_VS_OVERLAY)) {
			dev_err(es705_priv.dev,
				"%s(): ES705 is in sleep mode.", __func__);
			rc = -EPERM;
			goto es705_put_power_control_enum_exit;
		}

		if (value == ES705_SET_POWER_STATE_SLEEP) {
			dev_dbg(es705_priv.dev,
				"%s(): Activate Sleep Request\n", __func__);
			es705_sleep_request(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			/* Overlay mode doesn't need wakeup */
			if (es705_priv.es705_power_state !=
			    ES705_SET_POWER_STATE_VS_OVERLAY) {
				rc = es705_wakeup(&es705_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
				if (es705_priv.es705_power_state ==
				    ES705_SET_POWER_STATE_VS_LOWPWR) {
					es705_switch_to_normal_mode
					    (&es705_priv);
					/* Wait for 100ms */
					msleep(100);
					es705_priv.es705_power_state =
					    ES705_SET_POWER_STATE_NORMAL;
					es705_priv.mode = STANDARD;
				}
			} else {
				rc = es705_write(NULL, ES705_POWER_STATE,
						 value);
				if (rc) {
					dev_err(es705_priv.dev,
						"%s(): Power state command write failed\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				}
				/* Wait for 100ms to switch from Overlay mode */
				msleep(100);
			}
			es705_priv.es705_power_state =
			    ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
		} else if (value == ES705_SET_POWER_STATE_VS_LOWPWR) {
			if (es705_priv.es705_power_state ==
			    ES705_SET_POWER_STATE_VS_OVERLAY) {
				rc = es705_set_lpm(&es705_priv);
				if (rc)
					dev_err(es705_priv.dev,
						"%s(): Can't switch to Low Power Mode\n",
						__func__);
				else
					es705_priv.pm_state = ES705_POWER_SLEEP;
			} else {
				dev_err(es705_priv.dev,
					"%s(): ES705 should be in VS Overlay"
					"mode. Select the VS Overlay Mode.\n",
					__func__);
				rc = -EINVAL;
			}
			goto es705_put_power_control_enum_exit;
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			if (es705_priv.es705_power_state ==
			    ES705_SET_POWER_STATE_VS_LOWPWR) {
				rc = es705_wakeup(&es705_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
				es705_priv.es705_power_state =
				    ES705_SET_POWER_STATE_VS_OVERLAY;
			} else {
				rc = es705_switch_to_vs_mode(&es705_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
			}
		}
	}

es705_put_power_control_enum_exit:
	dev_dbg(es705_priv.dev,
		"%s(): Current power state = %s, power set cmd = %s\n",
		__func__, power_state[es705_priv.pm_state],
		power_state_cmd[es705_priv.es705_power_state]);

	return rc;
}

static int es705_get_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.rx1_route_enable;

	dev_dbg(es705_priv.dev, "%s(): rx1_route_enable = %zu\n",
		__func__, es705_priv.rx1_route_enable);

	return 0;
}

static int es705_put_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.rx1_route_enable = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): rx1_route_enable = %zu\n",
		__func__, es705_priv.rx1_route_enable);

#if defined(SAMSUNG_ES705_FEATURE)
	if (es705_priv.power_control) {
		if (es705_priv.rx1_route_enable) {
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
						 ES705_POWER_STATE);
		} else
		    if (!
			(es705_priv.rx1_route_enable
			 || es705_priv.rx2_route_enable
			 || es705_priv.tx1_route_enable)) {
			es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
						 ES705_POWER_STATE);
		}
	}
#endif

	return 0;
}

static int es705_get_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.tx1_route_enable;

	dev_dbg(es705_priv.dev, "%s(): tx1_route_enable = %zu\n",
		__func__, es705_priv.tx1_route_enable);

	return 0;
}

static int es705_put_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.tx1_route_enable = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): tx1_route_enable = %zu\n",
		__func__, es705_priv.tx1_route_enable);

#if defined(SAMSUNG_ES705_FEATURE)
	if (es705_priv.power_control) {
		if (es705_priv.tx1_route_enable) {
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
						 ES705_POWER_STATE);
		} else
		    if (!
			(es705_priv.rx1_route_enable
			 || es705_priv.rx2_route_enable
			 || es705_priv.tx1_route_enable)) {
			es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
						 ES705_POWER_STATE);
		}
	}
#endif

	return 0;
}

static int es705_get_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.rx2_route_enable;

	dev_dbg(es705_priv.dev, "%s(): rx2_route_enable = %zu\n",
		__func__, es705_priv.rx2_route_enable);

	return 0;
}

static int es705_put_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.rx2_route_enable = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): rx2_route_enable = %zu\n",
		__func__, es705_priv.rx2_route_enable);

#if defined(SAMSUNG_ES705_FEATURE)
	if (es705_priv.power_control) {
		if (es705_priv.rx2_route_enable) {
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
						 ES705_POWER_STATE);
		} else
		    if (!
			(es705_priv.rx1_route_enable
			 || es705_priv.rx2_route_enable
			 || es705_priv.tx1_route_enable)) {
			es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
						 ES705_POWER_STATE);
		}
	}
#endif
	return 0;
}

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
static int es705_get_voice_wakeup_enable_value(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_value
					       *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.voice_wakeup_enable;

	dev_dbg(es705_priv.dev, "%s(): voice_wakeup_enable = %d\n",
		__func__, es705_priv.voice_wakeup_enable);

	return 0;
}

static int es705_put_voice_wakeup_enable_value(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_value
					       *ucontrol)
{
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_INTR_RISING_EDGE;
	u32 sync_rspn = sync_cmd;
	u32 cmd_lpsd[] = { 0x9017e03c, 0x901804b0,
		0x9017e03d, 0x90181000,
		0x9017e03e, 0x90180580,
		0x9017e03f, 0x90180000,
		0x9017e040, 0x90180005,
		0x9017e000, 0x90180002,
		0xffffffff
	};
	int match = 1;
	u32 vs_keyword_length_cmd =
	    0x902C0000 | (es705_priv.vs_keyword_length & 0xFFFF);

	unsigned int value = 0;
	int wakeup_onoff = ucontrol->value.integer.value[0];

	if (es705_priv.voice_wakeup_enable == wakeup_onoff) {
		dev_info(es705_priv.dev,
			 "%s(): skip to set voice_wakeup_enable[%d->%d]\n",
			 __func__, es705_priv.voice_wakeup_enable,
			 wakeup_onoff);
		return rc;
	}

	mutex_lock(&es705_priv.cvq_mutex);
	es705_priv.voice_wakeup_enable = wakeup_onoff;

	dev_info(es705_priv.dev, "%s(): voice_wakeup_enable = %d\n", __func__,
		 es705_priv.voice_wakeup_enable);

	if (es705_priv.power_control) {
		if (es705_priv.voice_wakeup_enable == 1) { /* Voice wakeup */
			es705_priv.
			    power_control(ES705_SET_POWER_STATE_VS_OVERLAY,
					  ES705_POWER_STATE);

			if (es705_priv.es705_power_state ==
			    ES705_SET_POWER_STATE_VS_OVERLAY) {
				es705_cmd(&es705_priv, ES705_CVS_PRESET_CMD);
				es705_priv.cvs_preset =
				    (ES705_CVS_PRESET_CMD & 0xffff);

				dev_info(es705_priv.dev,
					 "%s(): CVS preset = %d\n", __func__,
					 es705_priv.cvs_preset);

				es705_cmd(&es705_priv, vs_keyword_length_cmd);
				dev_info(es705_priv.dev,
					 "%s(): VS Keyword length = %d ms\n",
					 __func__,
					 es705_priv.vs_keyword_length);
				/* initialize route number */
				es705_priv.internal_route_num = 40;
				es705_switch_route_config(27);

				rc = es705_write_then_read(&es705_priv,
							   &sync_cmd,
							   sizeof(sync_cmd),
							   &sync_rspn, match);
				if (rc) {
					dev_err(es705_priv.dev,
						"%s(): es705 Sync FAIL\n",
						__func__);
				} else {
					rc = es705_write_sensory_vs_keyword();
					if (rc)
						dev_err(es705_priv.dev,
							"%s(): es705 keyword download FAIL\n",
							__func__);
				}
			}
			es705_priv.
			    power_control(ES705_SET_POWER_STATE_VS_LOWPWR,
					  ES705_POWER_STATE);
		} else if (es705_priv.voice_wakeup_enable == 2) {
			/* Voice wakeup LPSD - for Baby cry */
			es705_priv.power_control(
				ES705_SET_POWER_STATE_VS_OVERLAY,
				ES705_POWER_STATE);

			if (es705_priv.es705_power_state ==
			    ES705_SET_POWER_STATE_VS_OVERLAY) {
				/* initialize route number */
				es705_priv.internal_route_num = 40;
				es705_switch_route_config(27);
				es705_write_block(&es705_priv, cmd_lpsd);

				rc = es705_write_then_read(&es705_priv,
							   &sync_cmd,
							   sizeof(sync_cmd),
							   &sync_rspn, match);
				if (rc) {
					dev_err(es705_priv.dev,
						"%s(): es705 Sync FAIL\n",
						__func__);
				}
			}
			es705_priv.
			    power_control(ES705_SET_POWER_STATE_VS_LOWPWR,
					  ES705_POWER_STATE);
		} else {
			if (es705_priv.es705_power_state ==
			    ES705_SET_POWER_STATE_VS_LOWPWR) {
				es705_priv.
				    power_control
				    (ES705_SET_POWER_STATE_VS_OVERLAY,
				     ES705_POWER_STATE);
				es705_priv.voice_lpm_enable = 0;
			}

			if (es705_priv.es705_power_state ==
			    ES705_SET_POWER_STATE_VS_OVERLAY) {
				/* initialize route number */
				es705_priv.internal_route_num = 40;
				es705_priv.
				    power_control(ES705_SET_POWER_STATE_NORMAL,
						  ES705_POWER_STATE);
				if (es705_priv.es705_power_state ==
				    ES705_SET_POWER_STATE_NORMAL) {
					value =
					    es705_read(NULL, ES705_POWER_STATE);
					dev_info(es705_priv.dev,
						 "%s(): current power state = %d\n",
						 __func__, value);
				}
#if defined(SAMSUNG_ES705_FEATURE)
				es705_read_write_power_control(0);
#endif
			}
		}
	}
	mutex_unlock(&es705_priv.cvq_mutex);
	return rc;
}

static int es705_get_voice_lpm_enable_value(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.voice_lpm_enable;

	dev_dbg(es705_priv.dev, "%s(): voice_lpm_enable = %d\n", __func__,
		es705_priv.voice_lpm_enable);

	return 0;
}

static int es705_put_voice_lpm_enable_value(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	if (es705_priv.voice_lpm_enable == ucontrol->value.integer.value[0]) {
		dev_info(es705_priv.dev,
			 "%s(): skip to set voice_lpm_enable[%d->%ld]\n",
			 __func__, es705_priv.voice_lpm_enable,
			 ucontrol->value.integer.value[0]);
		return 0;
	}

	es705_priv.voice_lpm_enable = ucontrol->value.integer.value[0];

	dev_info(es705_priv.dev, "%s(): voice_lpm_enable = %d\n",
		 __func__, es705_priv.voice_lpm_enable);

	if (!es705_priv.power_control) {
		dev_err(es705_priv.dev, "%s(): lpm enable error\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int es705_get_vs_abort_value(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = abort_request;

	dev_dbg(es705_priv.dev, "%s(): abort request = %d\n", __func__,
		abort_request);

	return 0;
}

static int es705_put_vs_abort_value(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	abort_request = ucontrol->value.integer.value[0];

	dev_info(es705_priv.dev, "%s(): abort request = %d\n", __func__,
		 abort_request);

	return 0;
}

static int es705_get_vs_make_internal_dump(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_vs_make_internal_dump(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	u32 cmd_capture_mode[] = { 0x9017e039, 0x90180001, 0xffffffff };
	int mode = ucontrol->value.integer.value[0];
	int rc;

	dev_info(es705_priv.dev, "%s(): start internal dump, mode=%d\n",
		 __func__, mode);

	if (mode == 2)
		cmd_capture_mode[1] = 0x90180002;

	/* sensory pid capture mode = 1 for audio capture 960msec */
	rc = es705_write_block(&es705_priv, cmd_capture_mode);

	usleep_range(10000, 11000);

	return rc;
}

static int es705_get_vs_make_external_dump(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

#define MAX_VS_RECORD_SIZE	65536	/* 64 KB */
static int es705_put_vs_make_external_dump(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	u32 cmd_rwdb_mode[] = { 0x9017e021, 0x90180003, 0xffffffff };
	u32 cmd_get_capture_mode = 0x8016e039;
	u32 check_rspn;
	mm_segment_t old_fs;
	struct file *fp;
	int match = 1;
	char *dump_data;
	int rc;

	dev_info(es705_priv.dev, "%s(): start\n", __func__);

	/* check that 0xE039 = 255 */
	check_rspn = 0x801600ff;
	rc = es705_write_then_read(&es705_priv, &cmd_get_capture_mode,
				   sizeof(cmd_get_capture_mode), &check_rspn,
				   match);
	if (rc)
		dev_err(es705_priv.dev,
			"%s(): internal dump is failed rspn 0x%08x\n", __func__,
			check_rspn);
	else
		dev_info(es705_priv.dev,
			 "%s(): internal dump is generated rspn 0x%08x\n",
			 __func__, check_rspn);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	dump_data = kmalloc(MAX_VS_RECORD_SIZE, GFP_KERNEL);
	if (!dump_data) {
		dev_err(es705_priv.dev, "%s(): Memory allocation FAIL\n",
			__func__);
		rc = -ENOMEM;
		goto es705_put_vs_make_external_dump_exit;
	}
	/* Need to check samsung own's debug code */
	/*
	fp = filp_open("/sdcard/vs_capture.bin", O_RDWR | O_CREAT, S_IRWXU);
	if (!fp) {
		dev_err(es705_priv.dev, "%s() : fail to open fp\n", __func__);
		rc = -ENOENT;
		set_fs(old_fs);
		goto es705_put_vs_make_external_dump_exit;
	}
	*/

	/* set the rwdbmode to 3 to upload the audio buffer contents via RDB */
	rc = es705_write_block(&es705_priv, cmd_rwdb_mode);

	es705_read_vs_data_block(&es705_priv, dump_data, MAX_VS_RECORD_SIZE);

	dev_info(es705_priv.dev, "%s(): pos %d write = %d bytes\n", __func__,
		 (int)fp->f_pos, es705_priv.vs_keyword_param_size);

	fp->f_pos = 0;
	vfs_write(fp, dump_data, es705_priv.vs_keyword_param_size, &fp->f_pos);
	filp_close(fp, NULL);

es705_put_vs_make_external_dump_exit:
	if (dump_data != NULL)
		kfree(dump_data);
	set_fs(old_fs);

	return rc;
}
#endif

#if !defined(SAMSUNG_ES705_FEATURE)
static int es705_get_wakeup_method_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.wakeup_method;

	dev_dbg(es705_priv.dev, "%s(): es705 wakeup method by %s\n",
		__func__, es705_priv.wakeup_method ? "UART" : "wakeup GPIO");
	return 0;
}

static int es705_put_wakeup_method_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.wakeup_method = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): enable es705 wakeup by %s\n",
		__func__, es705_priv.wakeup_method ? "UART" : "wakeup GPIO");
	return 0;
}
#endif

static int es705_get_ns_value(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	dev_dbg(es705_priv.dev, "%s(): NS = %d\n", __func__, es705_priv.ns);

	ucontrol->value.enumerated.item[0] = es705_priv.ns;

	return 0;
}

static int es705_put_ns_value(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705_priv.dev, "%s(): NS = %d\n", __func__, value);

	es705_priv.ns = value;

	/* 0 = NS off, 1 = NS on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_NS_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET, ES705_NS_OFF_PRESET);

	return rc;
}

static int es705_get_sw_value(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_sw_value(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705_priv.dev, "%s(): SW = %d\n", __func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_SW_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET, ES705_SW_OFF_PRESET);

	return rc;
}

static int es705_get_sts_value(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_sts_value(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705_priv.dev, "%s(): STS = %d\n", __func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_STS_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET, ES705_STS_OFF_PRESET);

	return rc;
}

static int es705_get_rx_ns_value(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_rx_ns_value(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705_priv.dev, "%s(): RX_NS = %d\n", __func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_RX_NS_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET, ES705_RX_NS_OFF_PRESET);

	return rc;
}

static int es705_get_wnf_value(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_wnf_value(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705_priv.dev, "%s(): WNF = %d\n", __func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_WNF_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET, ES705_WNF_OFF_PRESET);

	return rc;
}

static int es705_get_bwe_value(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_bwe_value(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705->dev, "%s(): BWE = %d\n", __func__, value);

	if (es705->pm_state != ES705_POWER_AWAKE) {
		dev_info(es705->dev, "%s(): can't bwe on/off, pm_state(%d)\n",
			 __func__, es705->pm_state);
		return rc;
	}

	if (es705->current_bwe == value) {
		dev_info(es705->dev, "%s(): Avoid duplication value (%d)\n",
			 __func__, value);
		return 0;
	}

	if (network_type == WIDE_BAND) {
		dev_dbg(es705->dev,
			"%s(): WideBand does not need BWE feature\n", __func__);
		return rc;
	}

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_BWE_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET, ES705_BWE_OFF_PRESET);

	es705->current_bwe = value;
	es705->current_veq_preset = 1;
	es705->current_veq = -1;

	return rc;
}

static int es705_get_avalon_value(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_avalon_value(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(es705_priv.dev, "%s(): Avalon Wind Noise = %d\n",
		__func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_AVALON_WN_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
				 ES705_AVALON_WN_OFF_PRESET);

	return rc;
}

static int es705_get_vbb_value(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_vbb_value(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705_priv.dev, "%s(): Virtual Bass Boost = %d\n", __func__,
		value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_VBB_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET, ES705_VBB_OFF_PRESET);

	return rc;
}

static int es705_get_aud_zoom(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	dev_dbg(es705_priv.dev, "%s(): Zoom = %d\n", __func__, es705_priv.zoom);

	ucontrol->value.enumerated.item[0] = es705_priv.zoom;

	return 0;
}

static int es705_put_aud_zoom(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705_priv.dev, "%s(): Zoom = %d\n", __func__, value);

	es705_priv.zoom = value;

	if (value == ES705_AUD_ZOOM_NARRATOR) {
		rc = es705_write(NULL, ES705_PRESET, ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
				 ES705_AUD_ZOOM_NARRATOR_PRESET);
	} else if (value == ES705_AUD_ZOOM_SCENE) {
		rc = es705_write(NULL, ES705_PRESET, ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
				 ES705_AUD_ZOOM_SCENE_PRESET);
	} else if (value == ES705_AUD_ZOOM_NARRATION) {
		rc = es705_write(NULL, ES705_PRESET, ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
				 ES705_AUD_ZOOM_NARRATION_PRESET);
	} else {
		rc = es705_write(NULL, ES705_PRESET, 0);
	}

	return rc;
}

static int es705_get_veq_preset_value(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return es705_priv.current_veq_preset;
}

static int es705_put_veq_preset_value(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;

	dev_dbg(es705->dev, "%s(): VEQ Preset = %d\n", __func__, value);

	if (es705->pm_state != ES705_POWER_AWAKE) {
		dev_info(es705->dev, "%s(): can't bwe on/off, pm_state(%d)\n",
			 __func__, es705->pm_state);
		return rc;
	}

	if (es705->current_veq_preset == value) {
		dev_info(es705->dev, "%s(): Avoid duplication value (%d)\n",
			 __func__, value);
		return 0;
	}

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET, ES705_VEQ_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET, ES705_VEQ_OFF_PRESET);

	es705->current_veq_preset = value;

	return rc;
}

/* Get for streming is not avaiable. Tinymix "set" method first executes get
 * and then put method. Thus dummy get method is implemented. */
static int es705_get_streaming_select(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = -1;

	return 0;
}

int es705_remote_route_enable(struct snd_soc_dai *dai)
{
	dev_dbg(es705_priv.dev, "%s():dai->name = %s dai->id = %d\n",
		__func__, dai->name, dai->id);

	switch (dai->id) {
	case ES705_SLIM_1_PB:
		return es705_priv.rx1_route_enable;
	case ES705_SLIM_1_CAP:
		return es705_priv.tx1_route_enable;
	case ES705_SLIM_2_PB:
		return es705_priv.rx2_route_enable;
	default:
		return 0;
	}
}
EXPORT_SYMBOL_GPL(es705_remote_route_enable);

int es705_put_veq_block(int volume)
{
	struct es705_priv *es705 = &es705_priv;

	u32 cmd;
	int ret;
#if defined(CONFIG_SND_SOC_ESXXX_VEQ_FILTER)
	u32 resp;
	u8 veq_coeff_size = 0;
	u32 fin_resp;
	u8 count = 0;
	u8 *wdbp = NULL;
	u8 wr = 0;
	int max_retry_cnt = 10;
#endif
	u8 veq_use_case = VEQ_CT;

	if (es705->pm_state != ES705_POWER_AWAKE) {
		dev_info(es705->dev,
			 "%s(): can't set veq block, pm_state(%d)\n", __func__,
			 es705->pm_state);
		return 0;
	}

	if ((volume >= MAX_VOLUME_INDEX) || (volume < 0)) {
		dev_info(es705->dev, "%s(): Invalid volume (%d)\n", __func__,
			 volume);
		return 0;
	}

	if ((es705->current_veq_preset == 0)
		|| (es705->current_veq == volume)) {
		dev_info(es705->dev,
			 "%s(): veq off or avoid duplication value(%d)\n",
			 __func__, volume);
		return 0;
	}
	dev_info(es705->dev,
		"%s(): volume=%d internal_route_num=%ld extra_volume=%d\n",
		__func__, volume, es705->internal_route_num, extra_volume);

	mutex_lock(&es705->api_mutex);

	es705->current_veq = volume;

	switch (es705->internal_route_num) {
	case 1:		/* FO_NSOn */
	case 3:		/* FO_NSOff */
	case 22:		/* FO_WB_NSOn */
	case 24:		/* FO_WB_NSOff */
		veq_use_case = VEQ_FT;
	default:
		break;
	}

	/* VEQ Max Gain */
	cmd = 0xB017003d;
#if defined(CONFIG_SND_SOC_ES_SLIM)
	cmd = cpu_to_le32(cmd);
#else
	cmd = cpu_to_be32(cmd);
#endif
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev,
			"%s(): write veq max gain cmd 0xB017003d failed\n",
			__func__);
		goto EXIT;
	}

	/* 0x00 ~ 0x0f(15dB) */
	if (network_type == WIDE_BAND ||
		(es705->internal_route_num >= ROUTE_FT_WB_NS_ON
		 && es705->internal_route_num <= ROUTE_CT_WB_NS_OFF)) {
		if (extra_volume || es705->current_bwe)
			cmd = veq_max_gains_extra_wb[veq_use_case][volume];
		else
			cmd = veq_max_gains_wb[veq_use_case][volume];
	} else {
		if (extra_volume || es705->current_bwe)
			cmd = veq_max_gains_extra[veq_use_case][volume];
		else
			cmd = veq_max_gains_nb[veq_use_case][volume];
	}

	dev_dbg(es705->dev, "%s(): write veq max gain 0x%08x to volume (%d)\n",
		    __func__, cmd, volume);

	if (cmd > 0x9018000f)
		cmd = 0x9018000f;
#if defined(CONFIG_SND_SOC_ES_SLIM)
	cmd = cpu_to_le32(cmd);
#else
	cmd = cpu_to_be32(cmd);
#endif
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): write veq max gain 0x9018000f failed\n",
			__func__);
		goto EXIT;
	}

	/* VEQ Noise Estimate Adj */
	cmd = 0xB0170025;
#if defined(CONFIG_SND_SOC_ES_SLIM)
	cmd = cpu_to_le32(cmd);
#else
	cmd = cpu_to_be32(cmd);
#endif
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev,
			"%s(): write veq estimate adj 0xb0170025 failed\n",
			__func__);
		goto EXIT;
	}

	/* 0x00 ~ 0x1e(30dB) */
	if (network_type == WIDE_BAND ||
		(es705->internal_route_num >= ROUTE_FT_WB_NS_ON
		 && es705->internal_route_num <= ROUTE_CT_WB_NS_OFF)) {
		if (extra_volume || es705->current_bwe) {
			cmd =
			veq_noise_estimate_adjs_extra_wb[veq_use_case][volume];
		} else {
			cmd = veq_noise_estimate_adjs_wb[veq_use_case][volume];
		}
	} else {
		if (extra_volume || es705->current_bwe) {
			cmd =
			veq_noise_estimate_adjs_extra[veq_use_case][volume];
		} else {
			cmd = veq_noise_estimate_adjs_nb[veq_use_case][volume];
		}
	}

	dev_dbg(es705->dev, "%s(): write veq estimate adj 0x%08x to volume (%d)\n",
		    __func__, cmd, volume);

	if (cmd > 0x9018001e)
		cmd = 0x9018001e;
#if defined(CONFIG_SND_SOC_ES_SLIM)
	cmd = cpu_to_le32(cmd);
#else
	cmd = cpu_to_be32(cmd);
#endif
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev,
			"%s(): write veq estimate adj 0x9018001e failed\n",
			__func__);
		goto EXIT;
	}

#if defined(CONFIG_SND_SOC_ESXXX_VEQ_FILTER)
	/* VEQ Coefficients Filter */
	if (network_type == WIDE_BAND) {
		veq_coeff_size = 0x4A;
		wdbp = (char *)&veq_coefficients_wb[veq_use_case][volume][0];
	} else {
		veq_coeff_size = 0x3E;
		wdbp = (char *)&veq_coefficients_nb[veq_use_case][volume][0];
	}

	cmd = 0x802f0000 | (veq_coeff_size & 0xffff);
#if defined(CONFIG_SND_SOC_ES_SLIM)
	cmd = cpu_to_le32(cmd);
#else
	cmd = cpu_to_be32(cmd);
#endif
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	dev_dbg(es705->dev, "%s(): write veq coeff size 0x8028f000\n",
			__func__);
	if (ret < 0) {
		dev_err(es705->dev,
			"%s(): write veq coeff size 0x8028f000 failed\n",
			__func__);
		goto EXIT;
	}

	usleep_range(10000, 11000);

	do {
		ret =
		    es705->dev_read(es705, (char *)&resp, ES705_READ_VE_WIDTH);
		count++;
	} while (resp != cmd && count < max_retry_cnt);

	if (resp != cmd) {
#if !defined(CONFIG_SND_SOC_ES_SLIM)
		resp = be32_to_cpu(resp);
#endif
		dev_err(es705->dev,
			"%s(): error writing veq coeff size, resp is 0x%08x\n",
			__func__, resp);
		goto EXIT;
	}

	while (wr < veq_coeff_size) {
		int sz = min(veq_coeff_size - wr, ES705_WRITE_VE_WIDTH);

		if (sz < ES705_WRITE_VE_WIDTH) {
			cmd = 0;
			count = 0;
			while (sz > 0) {
				cmd = cmd << 8;
				cmd = cmd | wdbp[wr++];
				sz--;
				count++;
			}
			cmd = cmd << ((ES705_WRITE_VE_WIDTH - count) * 8);
		} else {
			cmd = *((int *)(wdbp + wr));
			cmd = cpu_to_be32(cmd);
		}
		es705->dev_write(es705, (char *)&cmd, ES705_WRITE_VE_WIDTH);
		wr += sz;
	}

	usleep_range(10000, 11000);

	count = 0;
	do {
		fin_resp = 0xFFFFFFFF;
		ret = es705->dev_read(es705, (char *)&fin_resp,
				      ES705_READ_VE_WIDTH);
#if !defined(CONFIG_SND_SOC_ES_SLIM)
		fin_resp = be32_to_cpu(fin_resp);
#endif
		count++;
	} while (fin_resp != 0x802f0000 && count < max_retry_cnt);

	if (fin_resp != 0x802f0000) {
		dev_err(es705->dev,
			"%s(): error writing veq coeff block, resp is 0x%08x\n",
			__func__, fin_resp);
		goto EXIT;
	}
#endif
	dev_info(es705->dev, "%s(): success\n", __func__);

EXIT:
	mutex_unlock(&es705->api_mutex);
	if (ret != 0)
		dev_err(es705->dev, "%s(): failed ret=%d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(es705_put_veq_block);

static int es705_put_internal_route_config(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	es705_switch_route_config(ucontrol->value.integer.value[0]);

	return 0;
}

static int es705_get_internal_route_config(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.integer.value[0] = es705->internal_route_num;

	return 0;
}

static int es705_put_network_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	dev_info(es705->dev, "%s():network type = %ld pm_state = %d\n",
		 __func__, ucontrol->value.integer.value[0], es705->pm_state);

	if (ucontrol->value.integer.value[0] == WIDE_BAND)
		network_type = WIDE_BAND;
	else
		network_type = NARROW_BAND;

	mutex_lock(&es705->pm_mutex);
	if (es705->pm_state == ES705_POWER_AWAKE)
		es705_switch_route_config(es705->internal_route_num);

	mutex_unlock(&es705->pm_mutex);

	return 0;
}

static int es705_get_network_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = network_type;

	return 0;
}

static int es705_put_extra_volume(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	dev_info(es705->dev, "%s():extra volume = %ld pm_state = %d\n",
		 __func__, ucontrol->value.integer.value[0], es705->pm_state);

	extra_volume = ucontrol->value.integer.value[0];

	mutex_lock(&es705->pm_mutex);
	if (es705->pm_state == ES705_POWER_AWAKE)
		es705_switch_route_config(es705->internal_route_num);

	mutex_unlock(&es705->pm_mutex);

	return 0;
}

static int es705_get_extra_volume(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = extra_volume;

	return 0;
}

static int es705_put_internal_route(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	dev_info(es705_priv.dev, "%s : put internal route = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);

	es705_switch_route(ucontrol->value.integer.value[0]);

	return 0;
}

static int es705_get_internal_route(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.integer.value[0] = es705->internal_route_num;

	return 0;
}

static int es705_put_internal_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;
	const u32 *rate_macro = NULL;
	int rc = 0;

	dev_dbg(es705->dev, "%s():es705->internal_rate = %d ucontrol = %d\n",
		__func__, (int)es705->internal_rate,
		(int)ucontrol->value.enumerated.item[0]);

	if (!rate_macro) {
		dev_err(es705->dev, "%s(): internal rate, %d, out of range\n",
			__func__, ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}

	es705->internal_rate = ucontrol->value.enumerated.item[0];
	rc = es705_write_block(es705, rate_macro);

	return rc;
}

static int es705_get_internal_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.enumerated.item[0] = es705->internal_rate;
	dev_dbg(es705->dev, "%s():es705->internal_rate = %d ucontrol = %d\n",
		__func__, (int)es705->internal_rate,
		(int)ucontrol->value.enumerated.item[0]);

	return 0;
}

static int es705_put_preset_value(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	size_t value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	rc = es705_write(NULL, reg, value);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): Set Preset failed\n", __func__);
		return rc;
	}

	es705_priv.preset = value;

	return rc;
}

static int es705_get_preset_value(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.preset;

	return 0;
}

static int es705_get_audio_custom_profile(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_audio_custom_profile(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	if (index < ES705_CUSTOMER_PROFILE_MAX) {
		es705_write_block(&es705_priv,
				  &es705_audio_custom_profiles[index][0]);
	}

	return 0;
}

static int es705_ap_put_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.ap_tx1_ch_cnt = ucontrol->value.enumerated.item[0] + 1;

	pr_info("%s : cnt = %d\n", __func__, es705_priv.ap_tx1_ch_cnt);

	return 0;
}

static int es705_ap_get_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.enumerated.item[0] = es705->ap_tx1_ch_cnt - 1;

	return 0;
}

static const char *const es705_ap_tx1_ch_cnt_texts[] = {
	"One", "Two", "Three"
};

static const struct soc_enum es705_ap_tx1_ch_cnt_enum =
SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(es705_ap_tx1_ch_cnt_texts),
		es705_ap_tx1_ch_cnt_texts);

static const char *const es705_vs_power_state_texts[] = {
	"None", "Sleep", "MP_Sleep", "MP_Cmd", "Normal", "Overlay", "Low_Power"
};

static const struct soc_enum es705_vs_power_state_enum =
SOC_ENUM_SINGLE(ES705_POWER_STATE, 0,
		ARRAY_SIZE(es705_vs_power_state_texts),
		es705_vs_power_state_texts);

/* generic gain translation */
static int es705_index_to_gain(int min, int step, int index)
{
	return min + (step * index);
}

static int es705_gain_to_index(int min, int step, int gain)
{
	return (gain - min) / step;
}

/* dereverb gain */
static int es705_put_dereverb_gain_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 12) {
		value =
		    es705_index_to_gain(-12, 1,
					ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return rc;
}

static int es705_get_dereverb_gain_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = 0;

	if (es705_priv.rx1_route_enable ||
	    es705_priv.tx1_route_enable || es705_priv.rx2_route_enable) {
		value = es705_read(NULL, reg);
	}
	ucontrol->value.integer.value[0] = es705_gain_to_index(-12, 1, value);
	return 0;
}

/* bwe high band gain */
static int es705_put_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value
					      *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 30) {
		value =
		    es705_index_to_gain(-10, 1,
					ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return 0;
}

static int es705_get_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value
					      *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = 0;

	if (es705_priv.rx1_route_enable ||
	    es705_priv.tx1_route_enable || es705_priv.rx2_route_enable) {
		value = es705_read(NULL, reg);
	}
	ucontrol->value.integer.value[0] = es705_gain_to_index(-10, 1, value);
	return 0;
}

/* bwe max snr */
static int es705_put_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 70) {
		value =
		    es705_index_to_gain(-20, 1,
					ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return 0;
}

static int es705_get_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = 0;

	if (es705_priv.rx1_route_enable ||
	    es705_priv.tx1_route_enable || es705_priv.rx2_route_enable) {
		value = es705_read(NULL, reg);
	}
	ucontrol->value.integer.value[0] = es705_gain_to_index(-20, 1, value);
	return 0;
}

static const char *const es705_mic_config_texts[] = {
	"CT 2-mic", "FT 2-mic", "DV 1-mic", "EXT 1-mic", "BT 1-mic",
	"CT ASR 2-mic", "FT ASR 2-mic", "EXT ASR 1-mic", "FT ASR 1-mic",
};

static const struct soc_enum es705_mic_config_enum =
SOC_ENUM_SINGLE(ES705_MIC_CONFIG, 0,
		ARRAY_SIZE(es705_mic_config_texts),
		es705_mic_config_texts);

static const char *const es705_aec_mode_texts[] = {
	"Off", "On", "rsvrd2", "rsvrd3", "rsvrd4", "On half-duplex"
};

static const struct soc_enum es705_aec_mode_enum =
SOC_ENUM_SINGLE(ES705_AEC_MODE, 0,
		ARRAY_SIZE(es705_aec_mode_texts),
		es705_aec_mode_texts);

static const char *const es705_algo_rates_text[] = {
	"fs=8khz", "fs=16khz", "fs=24khz", "fs=48khz", "fs=96khz", "fs=192khz"
};

static const struct soc_enum es705_algo_sample_rate_enum =
SOC_ENUM_SINGLE(ES705_ALGO_SAMPLE_RATE, 0,
		ARRAY_SIZE(es705_algo_rates_text),
		es705_algo_rates_text);

static const struct soc_enum es705_algo_mix_rate_enum =
SOC_ENUM_SINGLE(ES705_MIX_SAMPLE_RATE, 0,
		ARRAY_SIZE(es705_algo_rates_text),
		es705_algo_rates_text);

static const char *const es705_internal_rate_text[] = {
	"NB", "WB", "SWB", "FB"
};

static const struct soc_enum es705_internal_rate_enum =
SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(es705_internal_rate_text),
		es705_internal_rate_text);

static const char *const es705_algorithms_text[] = {
	"None", "VP", "Two CHREC", "AUDIO", "Four CHPASS"
};

static const struct soc_enum es705_algorithms_enum =
SOC_ENUM_SINGLE(ES705_ALGO_SAMPLE_RATE, 0,
		ARRAY_SIZE(es705_algorithms_text),
		es705_algorithms_text);

static const char *const es705_off_on_texts[] = {
	"Off", "On"
};

static const char *const es705_audio_zoom_texts[] = {
	"disabled", "Narrator", "Scene", "Narration"
};

static const struct soc_enum es705_veq_enable_enum =
SOC_ENUM_SINGLE(ES705_VEQ_ENABLE, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_dereverb_enable_enum =
SOC_ENUM_SINGLE(ES705_DEREVERB_ENABLE, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_bwe_enable_enum =
SOC_ENUM_SINGLE(ES705_BWE_ENABLE, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_bwe_post_eq_enable_enum =
SOC_ENUM_SINGLE(ES705_BWE_POST_EQ_ENABLE, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_algo_processing_enable_enum =
SOC_ENUM_SINGLE(ES705_ALGO_PROCESSING, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_ns_enable_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_audio_zoom_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_audio_zoom_texts),
		es705_audio_zoom_texts);
static const struct soc_enum es705_rx_enable_enum =
SOC_ENUM_SINGLE(ES705_RX_ENABLE, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_sw_enable_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_sts_enable_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_rx_ns_enable_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_wnf_enable_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_bwe_preset_enable_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_avalon_wn_enable_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);
static const struct soc_enum es705_vbb_enable_enum =
SOC_ENUM_SINGLE(ES705_PRESET, 0,
		ARRAY_SIZE(es705_off_on_texts),
		es705_off_on_texts);

static int es705_put_power_state_enum(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_get_power_state_enum(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static const char *const es705_power_state_texts[] = {
	"Sleep", "Active"
};

static const struct soc_enum es705_power_state_enum =
SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(es705_power_state_texts),
		es705_power_state_texts);

static int es705_get_vs_enable(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.vs_enable;
	return 0;
}

static int es705_put_vs_enable(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.vs_enable = ucontrol->value.enumerated.item[0];
	es705_priv.vs_streaming(&es705_priv);
	return 0;
}

static int es705_get_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.vs_wakeup_keyword;
	return 0;
}

static int es705_put_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.vs_wakeup_keyword = ucontrol->value.enumerated.item[0];
	return 0;
}

static int es705_put_vs_stored_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int op;
	int ret = 0;

	op = ucontrol->value.enumerated.item[0];

	dev_dbg(es705_priv.dev, "%s(): op=%d\n", __func__, op);

	switch (op) {
	case 0:
		dev_dbg(es705_priv.dev, "%s(): keyword params put...\n",
			__func__);
		if (es705_priv.rdb_wdb_open) {
			ret = es705_priv.rdb_wdb_open(&es705_priv);
			if (ret < 0) {
				dev_err(es705_priv.dev,
					"%s(): WDB UART open FAIL\n", __func__);
				break;
			}
		}
		ret = es705_write_vs_data_block(&es705_priv);
		if (es705_priv.rdb_wdb_close) {
			ret = es705_priv.rdb_wdb_close(&es705_priv);
			if (ret < 0)
				dev_err(es705_priv.dev,
					"%s(): WDB UART close FAIL\n",
					__func__);
		}
		break;
	case 1:
		dev_dbg(es705_priv.dev, "%s(): keyword params get...\n",
			__func__);
		ret =
		    es705_read_vs_data_block(&es705_priv,
					     (char *)es705_priv.
					     vs_keyword_param,
					     ES705_VS_KEYWORD_PARAM_MAX);
		break;
	case 2:
		dev_dbg(es705_priv.dev, "%s(): keyword params clear...\n",
			__func__);
		es705_priv.vs_keyword_param_size = 0;
		break;
	default:
		BUG_ON(0);
	};

	return ret;
}

/* Voice Sense Detection Sensitivity */
static int es705_put_vs_detection_sensitivity(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value
					      *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	size_t value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): ucontrol = %ld value = %zu\n",
		__func__, ucontrol->value.integer.value[0], value);

	rc = es705_write(NULL, reg, value);

	return rc;
}

static
int es705_get_vs_detection_sensitivity(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = 0;

	if (es705_priv.rx1_route_enable ||
	    es705_priv.tx1_route_enable || es705_priv.rx2_route_enable) {
		value = es705_read(NULL, reg);
	}
	ucontrol->value.integer.value[0] = value;
	dev_dbg(es705_priv.dev, "%s(): value = %d ucontrol = %ld\n",
		__func__, value, ucontrol->value.integer.value[0]);
	return 0;
}

static int es705_put_vad_sensitivity(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	size_t value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): ucontrol = %ld value = %zu\n",
		__func__, ucontrol->value.integer.value[0], value);

	rc = es705_write(NULL, reg, value);

	return rc;
}

static int es705_get_vad_sensitivity(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = 0;

	if (es705_priv.rx1_route_enable ||
	    es705_priv.tx1_route_enable || es705_priv.rx2_route_enable) {
		value = es705_read(NULL, reg);
	}
	ucontrol->value.integer.value[0] = value;
	dev_dbg(es705_priv.dev, "%s(): value = %d ucontrol = %ld\n",
		__func__, value, ucontrol->value.integer.value[0]);
	return 0;
}

static int es705_put_cvs_preset_value(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	size_t value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	dev_info(es705_priv.dev, "%s(): cvs_preset=0x%zx\n", __func__, value);

	rc = es705_write(NULL, reg, value);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): Set CVS Preset failed\n",
			__func__);
		return rc;
	}

	es705_priv.cvs_preset = value;

	return rc;
}

static int es705_get_cvs_preset_value(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.cvs_preset;

	return 0;
}

static int get_detected_keyword(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.detected_vs_keyword;

	dev_info(es705_priv.dev, "%s(): keyword is = 0x%x\n",
		 __func__, es705_priv.detected_vs_keyword);

	return 0;
}

static int get_vs_keyword_length(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.vs_keyword_length;

	dev_info(es705_priv.dev, "%s(): vs_keyword_length=%d\n",
		 __func__, es705_priv.vs_keyword_length);

	return 0;
}

static int put_vs_keyword_length(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.vs_keyword_length = ucontrol->value.integer.value[0];

	dev_info(es705_priv.dev, "%s(): vs_keyword_length=%d\n",
		 __func__, es705_priv.vs_keyword_length);

	return 0;
}

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
static int switch_get_osc(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int switch_ext_osc(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;
	u32 es_set_power_level =
	    ES705_SET_POWER_LEVEL << 16 | ES705_POWER_LEVEL_6;
	u32 preset_cmd = 0x90311bbc;	/* PRESET cmd */
	u32 get_status = 0x8012 << 16 | 0x0000;
	int rc = 0;
	u32 resp;
	int value = ucontrol->value.enumerated.item[0];

	pr_debug("%s(): Switch ext oscillator", __func__);

	/* 0 = off, 1 = on */
	if (value) {
		pr_debug("%s(): Turn on external oscillator", __func__);

		rc = es705->cmd(es705, es_set_power_level, 0, &resp);
		if (rc < 0) {
			pr_err("%s(): Error setting power level\n", __func__);
			goto cmd_error;
		}
		usleep_range(2000, 2005);

		rc = es705->dev_write(es705, &preset_cmd, sizeof(preset_cmd));
		if (rc) {
			dev_err(es705->dev, "%s(): Set Preset failed\n",
				__func__);
			goto cmd_error;
		}
		usleep_range(2000, 2005);

		rc = es705->cmd(es705, get_status, 0, &resp);
		if (rc < 0) {
			pr_err("%s(): Error setting power level\n", __func__);
			goto cmd_error;
		}
		usleep_range(2000, 2005);
	} else {
		pr_err("%s(): Cannot turn off external oscillator", __func__);
		rc = -1;
	}

cmd_error:
	return rc;
}
#endif

static const char *const es705_vs_wakeup_keyword_texts[] = {
	"Default", "One", "Two", "Three", "Four"
};

static const struct soc_enum es705_vs_wakeup_keyword_enum =
SOC_ENUM_SINGLE(ES705_VOICE_SENSE_SET_KEYWORD, 0,
		ARRAY_SIZE(es705_vs_wakeup_keyword_texts),
		es705_vs_wakeup_keyword_texts);

static const char *const es705_vs_event_texts[] = {
	"No Event", "Codec Event", "VS Keyword Event",
};

static const struct soc_enum es705_vs_event_enum =
SOC_ENUM_SINGLE(ES705_VOICE_SENSE_EVENT, 0,
		ARRAY_SIZE(es705_vs_event_texts),
		es705_vs_event_texts);

static const char *const es705_vs_training_status_texts[] = {
	"Training", "Done",
};

static const struct soc_enum es705_vs_training_status_enum =
SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_STATUS, 0,
		ARRAY_SIZE(es705_vs_training_status_texts),
		es705_vs_training_status_texts);

static const char *const es705_vs_training_record_texts[] = {
	"Stop", "Start",
};

static const char *const es705_vs_stored_keyword_texts[] = {
	"Put", "Get", "Clear"
};

static const struct soc_enum es705_vs_stored_keyword_enum =
SOC_ENUM_SINGLE(ES705_VS_STORED_KEYWORD, 0,
		ARRAY_SIZE(es705_vs_stored_keyword_texts),
		es705_vs_stored_keyword_texts);

static const struct soc_enum es705_vs_training_record_enum =
SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_RECORD, 0,
		ARRAY_SIZE(es705_vs_training_record_texts),
		es705_vs_training_record_texts);

static const char *const es705_vs_training_mode_texts[] = {
	"Detect Keyword", "N/A", "Train User-defined Keyword",
};

static const struct soc_enum es705_vs_training_mode_enum =
SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_MODE, 0,
		ARRAY_SIZE(es705_vs_training_mode_texts),
		es705_vs_training_mode_texts);

static struct snd_kcontrol_new es705_digital_ext_snd_controls[] = {
	SOC_SINGLE_EXT("ES705 RX1 Enable",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_rx1_route_enable_value,
		       es705_put_rx1_route_enable_value),
	SOC_SINGLE_EXT("ES705 TX1 Enable",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_tx1_route_enable_value,
		       es705_put_tx1_route_enable_value),
	SOC_SINGLE_EXT("ES705 RX2 Enable",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_rx2_route_enable_value,
		       es705_put_rx2_route_enable_value),
	SOC_ENUM_EXT("Mic Config",
		     es705_mic_config_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
	SOC_ENUM_EXT("AEC Mode",
		     es705_aec_mode_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
#if defined(SAMSUNG_ES705_FEATURE)
	SOC_ENUM_EXT("VEQ Enable",
		     es705_veq_enable_enum,
		     es705_get_veq_preset_value,
		     es705_put_veq_preset_value),
#else
	SOC_ENUM_EXT("VEQ Enable",
		     es705_veq_enable_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
#endif
	SOC_ENUM_EXT("Dereverb Enable", es705_dereverb_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("Dereverb Gain",
		       ES705_DEREVERB_GAIN, 0, 100, 0,
		       es705_get_dereverb_gain_value,
		       es705_put_dereverb_gain_value),
	SOC_ENUM_EXT("BWE Enable", es705_bwe_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("BWE High Band Gain",
		       ES705_BWE_HIGH_BAND_GAIN, 0, 100, 0,
		       es705_get_bwe_high_band_gain_value,
		       es705_put_bwe_high_band_gain_value),
	SOC_SINGLE_EXT("BWE Max SNR",
		       ES705_BWE_MAX_SNR, 0, 100, 0,
		       es705_get_bwe_max_snr_value,
		       es705_put_bwe_max_snr_value),
	SOC_ENUM_EXT("BWE Post EQ Enable",
		     es705_bwe_post_eq_enable_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
	SOC_SINGLE_EXT("SLIMbus Link Multi Channel",
		       ES705_SLIMBUS_LINK_MULTI_CHANNEL, 0, 65535, 0,
		       es705_get_control_value,
		       es705_put_control_value),
	SOC_ENUM_EXT("Set Power State",
		     es705_power_state_enum,
		     es705_get_power_state_enum,
		     es705_put_power_state_enum),
	SOC_ENUM_EXT("Algorithm Processing",
		     es705_algo_processing_enable_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
	SOC_ENUM_EXT("Algorithm Sample Rate",
		     es705_algo_sample_rate_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
	SOC_ENUM_EXT("Algorithm",
		     es705_algorithms_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
	SOC_ENUM_EXT("Mix Sample Rate",
		     es705_algo_mix_rate_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
	SOC_SINGLE_EXT("Internal Route",
		       SND_SOC_NOPM, 0, 100, 0,
		       es705_get_internal_route,
		       es705_put_internal_route),
	SOC_ENUM_EXT("Internal Rate",
		     es705_internal_rate_enum,
		     es705_get_internal_rate,
		     es705_put_internal_rate),
	SOC_SINGLE_EXT("Preset",
		       ES705_PRESET, 0, 65535, 0,
		       es705_get_preset_value,
		       es705_put_preset_value),
	SOC_SINGLE_EXT("Audio Custom Profile",
		       SND_SOC_NOPM, 0, 100, 0,
		       es705_get_audio_custom_profile,
		       es705_put_audio_custom_profile),
	SOC_ENUM_EXT("ES705-AP Tx Channels",
		     es705_ap_tx1_ch_cnt_enum,
		     es705_ap_get_tx1_ch_cnt,
		     es705_ap_put_tx1_ch_cnt),
	SOC_SINGLE_EXT("Voice Sense Enable",
		       ES705_VOICE_SENSE_ENABLE, 0, 1, 0,
		       es705_get_vs_enable,
		       es705_put_vs_enable),
	SOC_ENUM_EXT("Voice Sense Set Wakeup Word",
		     es705_vs_wakeup_keyword_enum,
		     es705_get_vs_wakeup_keyword,
		     es705_put_vs_wakeup_keyword),
	SOC_ENUM_EXT("Voice Sense Status",
		     es705_vs_event_enum,
		     es705_get_control_enum,
		     NULL),
	SOC_ENUM_EXT("Voice Sense Training Mode",
		     es705_vs_training_mode_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Training Status",
		     es705_vs_training_status_enum,
		     es705_get_control_enum,
		     NULL),
	SOC_ENUM_EXT("Voice Sense Training Record",
		     es705_vs_training_record_enum,
		     NULL,
		     es705_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Stored Keyword",
		     es705_vs_stored_keyword_enum,
		     NULL,
		     es705_put_vs_stored_keyword),
	SOC_SINGLE_EXT("Voice Sense Detect Sensitivity",
		       ES705_VOICE_SENSE_DETECTION_SENSITIVITY, 0, 10, 0,
		       es705_get_vs_detection_sensitivity,
		       es705_put_vs_detection_sensitivity),
	SOC_SINGLE_EXT("Voice Activity Detect Sensitivity",
		       ES705_VOICE_ACTIVITY_DETECTION_SENSITIVITY, 0, 10, 0,
		       es705_get_vad_sensitivity,
		       es705_put_vad_sensitivity),
	SOC_SINGLE_EXT("Continuous Voice Sense Preset",
		       ES705_CVS_PRESET, 0, 65535, 0,
		       es705_get_cvs_preset_value,
		       es705_put_cvs_preset_value),
	SOC_ENUM_EXT("ES705 Power State",
		     es705_vs_power_state_enum,
		     es705_get_power_control_enum,
		     es705_put_power_control_enum),
	SOC_ENUM_EXT("Noise Suppression",
		     es705_ns_enable_enum,
		     es705_get_ns_value,
		     es705_put_ns_value),
	SOC_ENUM_EXT("Audio Zoom", es705_audio_zoom_enum,
		     es705_get_aud_zoom,
		     es705_put_aud_zoom),
	SOC_SINGLE_EXT("Enable/Disable Streaming PATH/Endpoint",
		       ES705_FE_STREAMING, 0, 65535, 0,
		       es705_get_streaming_select,
		       es705_put_control_value),
	SOC_ENUM_EXT("RX Enable",
		     es705_rx_enable_enum,
		     es705_get_control_enum,
		     es705_put_control_enum),
	SOC_ENUM_EXT("Stereo Widening",
		     es705_sw_enable_enum,
		     es705_get_sw_value,
		     es705_put_sw_value),
	SOC_ENUM_EXT("Speech Time Stretching",
		     es705_sts_enable_enum,
		     es705_get_sts_value,
		     es705_put_sts_value),
	SOC_ENUM_EXT("RX Noise Suppression",
		     es705_rx_ns_enable_enum,
		     es705_get_rx_ns_value,
		     es705_put_rx_ns_value),
	SOC_ENUM_EXT("Wind Noise Filter",
		     es705_wnf_enable_enum,
		     es705_get_wnf_value,
		     es705_put_wnf_value),
	SOC_ENUM_EXT("BWE Preset",
		     es705_bwe_preset_enable_enum,
		     es705_get_bwe_value,
		     es705_put_bwe_value),
	SOC_ENUM_EXT("AVALON Wind Noise",
		     es705_avalon_wn_enable_enum,
		     es705_get_avalon_value,
		     es705_put_avalon_value),
	SOC_ENUM_EXT("Virtual Bass Boost",
		     es705_vbb_enable_enum,
		     es705_get_vbb_value,
		     es705_put_vbb_value),
	SOC_SINGLE_EXT("UART FW Download Rate",
		       SND_SOC_NOPM, 0, 3, 0,
		       es705_get_uart_fw_download_rate,
		       es705_put_uart_fw_download_rate),
	SOC_SINGLE_EXT("Voice Sense Stream Enable",
		       ES705_VS_STREAM_ENABLE, 0, 1, 0,
		       es705_get_vs_stream_enable,
		       es705_put_vs_stream_enable),
#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
	SOC_SINGLE_EXT("ES705 Voice Wakeup Enable",
		       SND_SOC_NOPM, 0, 2, 0,
		       es705_get_voice_wakeup_enable_value,
		       es705_put_voice_wakeup_enable_value),
	SOC_SINGLE_EXT("ES705 VS Abort",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_vs_abort_value,
		       es705_put_vs_abort_value),
	SOC_SINGLE_EXT("ES705 VS Make Internal Dump",
		       SND_SOC_NOPM, 0, 2, 0,
		       es705_get_vs_make_internal_dump,
		       es705_put_vs_make_internal_dump),
	SOC_SINGLE_EXT("ES705 VS Make External Dump",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_vs_make_external_dump,
		       es705_put_vs_make_external_dump),
	SOC_SINGLE_EXT("ES705 Voice LPM Enable",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_voice_lpm_enable_value,
		       es705_put_voice_lpm_enable_value),
	SOC_ENUM_EXT("Switch Ext Osc",
		     es705_vbb_enable_enum,
		     switch_get_osc,
		     switch_ext_osc),
#endif
#if defined(SAMSUNG_ES705_FEATURE)
	SOC_SINGLE_EXT("Internal Route Config",
		       SND_SOC_NOPM, 0, 100, 0,
		       es705_get_internal_route_config,
		       es705_put_internal_route_config),
	SOC_SINGLE_EXT("Current Network Type",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_network_type,
		       es705_put_network_type),
	SOC_SINGLE_EXT("Extra Volume Enable",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_extra_volume,
		       es705_put_extra_volume),
#endif
#if !defined(SAMSUNG_ES705_FEATURE)
	SOC_SINGLE_EXT("ES705 Wakeup Method",
		       SND_SOC_NOPM, 0, 1, 0,
		       es705_get_wakeup_method_value,
		       es705_put_wakeup_method_value),
#endif
	SOC_SINGLE_EXT("Get Detected Keyword",
		       SND_SOC_NOPM, 0, 65535, 0,
		       get_detected_keyword,
		       NULL),
	SOC_SINGLE_EXT("VS Keyword Length",
		       SND_SOC_NOPM, 0, 65535, 0,
		       get_vs_keyword_length,
		       put_vs_keyword_length),
};

static int es705_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	int rc = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		break;
	default:
		break;
	}

	codec->dapm.bias_level = level;

	return rc;
}

#if defined(CONFIG_SND_SOC_ES_SLIM)
struct snd_soc_dai_driver es705_dai[] = {
	{
	 .name = "es705-slim-rx1",
	 .id = ES705_SLIM_1_PB,
	 .playback = {
		      .stream_name = "SLIM_PORT-1 Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = ES705_SLIMBUS_RATES,
		      .formats = ES705_SLIMBUS_FORMATS,
		      },
	 .ops = &es705_slim_port_dai_ops,
	 },
	{
	 .name = "es705-slim-tx1",
	 .id = ES705_SLIM_1_CAP,
	 .capture = {
		     .stream_name = "SLIM_PORT-1 Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = ES705_SLIMBUS_RATES,
		     .formats = ES705_SLIMBUS_FORMATS,
		     },
	 .ops = &es705_slim_port_dai_ops,
	 },
	{
	 .name = "es705-slim-rx2",
	 .id = ES705_SLIM_2_PB,
	 .playback = {
		      .stream_name = "SLIM_PORT-2 Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = ES705_SLIMBUS_RATES,
		      .formats = ES705_SLIMBUS_FORMATS,
		      },
	 .ops = &es705_slim_port_dai_ops,
	 },
	{
	 .name = "es705-slim-tx2",
	 .id = ES705_SLIM_2_CAP,
	 .capture = {
		     .stream_name = "SLIM_PORT-2 Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = ES705_SLIMBUS_RATES,
		     .formats = ES705_SLIMBUS_FORMATS,
		     },
	 .ops = &es705_slim_port_dai_ops,
	 },
	{
	 .name = "es705-slim-rx3",
	 .id = ES705_SLIM_3_PB,
	 .playback = {
		      .stream_name = "SLIM_PORT-3 Playback",
		      .channels_min = 1,
		      .channels_max = 4,
		      .rates = ES705_SLIMBUS_RATES,
		      .formats = ES705_SLIMBUS_FORMATS,
		      },
	 .ops = &es705_slim_port_dai_ops,
	 },
	{
	 .name = "es705-slim-tx3",
	 .id = ES705_SLIM_3_CAP,
	 .capture = {
		     .stream_name = "SLIM_PORT-3 Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = ES705_SLIMBUS_RATES,
		     .formats = ES705_SLIMBUS_FORMATS,
		     },
	 .ops = &es705_slim_port_dai_ops,
	 },
};
#endif

int es705_remote_add_codec_controls(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_add_codec_controls(codec, es705_digital_ext_snd_controls,
					ARRAY_SIZE
					(es705_digital_ext_snd_controls));
	if (rc) {
		dev_err(codec->dev,
			"%s(): es705_digital_ext_snd_controls failed\n",
			__func__);
	}

	return rc;
}

static int es705_codec_probe(struct snd_soc_codec *codec)
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s()\n", __func__);

	es705->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);

	es705_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int es705_codec_remove(struct snd_soc_codec *codec)
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	es705_set_bias_level(codec, SND_SOC_BIAS_OFF);

	kfree(es705);

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_es705 = {
	.probe = es705_codec_probe,
	.remove = es705_codec_remove,
	.suspend = es705_codec_suspend,
	.resume = es705_codec_resume,
	.read = es705_read,
	.write = es705_write,
	.set_bias_level = es705_set_bias_level,
};

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
/* check which vs keyword has detected */
void check_detected_vs_keyword(struct es705_priv *es705)
{
	int rc = 0;
	u32 rspn = 0;
	int match = 0;
	u32 chk_vs_keyword = 0x8016E007;
	u16 vs_keyword_response = 0;

	dev_info(es705->dev, "%s(): check which vs keyword has detected\n",
		 __func__);

	rc = es705_write_then_read(es705, &chk_vs_keyword,
				   sizeof(chk_vs_keyword), &rspn, match);

	dev_info(es705->dev, "%s(): response is  0x%x\n", __func__, rspn);

	if (rc < 0) {
		dev_err(es705->dev, "%s(): cannot check the VS keyword\n",
			__func__);
	} else {
		vs_keyword_response = rspn & 0xFFFF;
		es705_priv.detected_vs_keyword = vs_keyword_response & 0x00FF;
		dev_info(es705->dev, "%s(): detected keyword is %d\n", __func__,
			 es705_priv.detected_vs_keyword);
	}
}

void check_vs_status(struct es705_priv *es705)
{
	int rc = 0;
	u32 rspn = 0;
	u32 chk_vs_svscore = 0x8016E008;
	u32 chk_vs_fscore = 0x8016E009;
	u32 chk_vs_noisefloor = 0x8016E048;
	u16 vs_status_response = 0;

	rc = es705_write_then_read(es705, &chk_vs_svscore,
				   sizeof(chk_vs_svscore), &rspn, 0);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): cannot check the status\n",
			__func__);
	} else {
		vs_status_response = rspn & 0xFFFF;
		dev_info(es705->dev, "%s(): svscore is %d\n", __func__,
			 vs_status_response);
	}

	rc = es705_write_then_read(es705, &chk_vs_fscore, sizeof(chk_vs_fscore),
				   &rspn, 0);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): cannot check the status\n",
			__func__);
	} else {
		vs_status_response = rspn & 0xFFFF;
		dev_info(es705->dev, "%s(): fscore is %d\n", __func__,
			 vs_status_response);
	}
	rc = es705_write_then_read(es705, &chk_vs_noisefloor,
				   sizeof(chk_vs_noisefloor), &rspn, 0);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): cannot check the status\n",
			__func__);
	} else {
		vs_status_response = rspn & 0xFFFF;
		dev_info(es705->dev, "%s(): noisefloor is %d\n", __func__,
			 vs_status_response);
	}
}

static void es705_event_status(struct es705_priv *es705)
{
	const u32 vs_get_key_word_status = 0x806D0000;
	u32 rspn = 0;
	u16 event_response = 0;
	int match = 0;
	int rc = 0;
	u32 es_set_power_level =
	    ES705_SET_POWER_LEVEL << 16 | ES705_POWER_LEVEL_6;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_ack;
	u32 int_osc_measure_start_cmd =
	    (ES705_INT_OSC_MEASURE_START << 16) | 0x0000;
	u32 int_osc_measure_query_cmd =
	    (ES705_INT_OSC_MEASURE_QUERY << 16) | 0x0000;
	u32 cmd_resp;
	u16 configure_uart = cpu_to_be32(ES705_NO_EVENT);
	u32 get_status = 0x8012 << 16 | 0x0000;
	int sync_retry = 5;

	dev_info(es705->dev, "%s(): voice wakeup type is %d\n", __func__,
		 es705->voice_wakeup_enable);

	es705->internal_route_num = 40;	/* initialize route number */

	/* wait for command mode to be ready in case of CVS */
	msleep(50);

	if (es705->voice_wakeup_enable == 1) {
		check_detected_vs_keyword(es705);

		if (es705->detected_vs_keyword == 1)
			es705->es705_power_state =
			    ES705_SET_POWER_STATE_VS_STREAMING;
	}

	if (es705->es705_power_state == ES705_SET_POWER_STATE_VS_LOWPWR) {
		dev_info(es705->dev, "%s(): wakeup the chip form low power\n",
			 __func__);
		rc = es705_wakeup(es705);
		if (rc) {
			dev_err(es705->dev,
				"%s(): Get VS Status Fail - wakeup fail\n",
				__func__);
			return;
		} else {
			/* wait VS status readiness */
			msleep(50);
		}
	}

	cpu_to_le32s(&vs_get_key_word_status);
	rc = es705_write_then_read(es705, &vs_get_key_word_status,
				   sizeof(vs_get_key_word_status), &rspn,
				   match);
	if (rc) {
		dev_err(es705->dev, "%s(): Get VS Status Fail\n", __func__);
		return;
	}

	le32_to_cpus(&rspn);
	event_response = rspn & 0xFFFF;

	/* Check VS detection status. */
	dev_info(es705->dev, "%s(): VS status 0x%04x\n", __func__, rspn);

	/* Work around not to check flag the status for testing */
	if ((event_response & 0x00FF) == KW_DETECTED) {
		if ((es705->voice_wakeup_enable == 1)
		    && (es705->detected_vs_keyword == 1)) {
			if (es705->uart_state != UART_OPEN) {
				rc = es705_uart_open(&es705_priv);
				if (rc) {
					dev_err(es705->dev,
						"%s(): uart open failed %d\n",
						__func__, rc);
					goto voiceq_isr_exit;
				}
			}

			do {
				rc = es705->cmd(es705,
						int_osc_measure_start_cmd, 0,
						&cmd_resp);
				if (rc) {
					dev_err(es705->dev,
						"%s(): sending configure command on slim failed\n",
						__func__);
				}

				rc = es705_uart_write(es705, &configure_uart,
						      sizeof(configure_uart));
				if (rc < 0) {
					dev_err(es705->dev,
						"%s(): sending configure command on uart failed\n",
						__func__);
				}

				usleep_range(5000, 5005);

				rc = es705_uart_cmd(es705, sync_cmd, 0,
						    &sync_ack);
				if (rc < 0) {
					dev_err(es705->dev,
						"%s(): sending sync command failed - %d\n",
						__func__, rc);
				} else {
					dev_info(es705->dev,
						 "%s(): sync ack is = 0x%08x\n",
						 __func__, sync_ack);
				}

				rc = es705->cmd(es705,
						int_osc_measure_query_cmd, 0,
						&cmd_resp);
				if (rc) {
					dev_err(es705->dev,
						"%s(): sending caliberation command on slim failed\n",
						__func__);
				} else if (cmd_resp ==
					   int_osc_measure_query_cmd) {
					dev_info(es705->dev,
						 "%s(): caliberation was successful\n",
						 __func__);
					break;
				} else {
					dev_info(es705->dev,
						 "%s(): caliberation response 0x%08x\n",
						 __func__, cmd_resp);
				}
			} while (sync_retry--);

			if (!sync_retry) {
				dev_err(es705->dev,
					"%s(): UART internal clock is failed to configure, using fallback mechanism\n",
					__func__);

				rc = es705->cmd(es705, es_set_power_level, 0,
						&cmd_resp);
				if (rc < 0) {
					dev_err(es705->dev,
						"%s(): Error setting power level\n",
						__func__);
					goto voiceq_isr_exit;
				}

				usleep_range(2000, 2005);

				rc = es705->cmd(es705, get_status, 0,
						&cmd_resp);
				if (rc < 0) {
					dev_err(es705->dev,
						"%s(): Error setting power level\n",
						__func__);
					goto voiceq_isr_exit;
				}

				usleep_range(2000, 2005);
			} else {
				dev_info(es705->dev,
					 "%s(): calibrating the uart was successful\n",
					 __func__);
			}

			dev_info(es705->dev, "%s(), closing the UART\n",
				 __func__);

			rc = es705_uart_close(es705);
			if (rc) {
				dev_err(es705->dev,
					"%s(): high_bw_close failed %d\n",
					__func__, rc);
			}

voiceq_isr_exit:
			dev_info(es705->dev, "%s : voiceq isr exit\n",
				 __func__);
			/* check_vs_status(es705); */
		} else {
			es705->pm_state = ES705_POWER_AWAKE;
			es705_priv.
			    power_control(ES705_SET_POWER_STATE_VS_OVERLAY,
					  ES705_POWER_STATE);
		}

		es705->vs_get_event = event_response;

		dev_info(es705->dev,
			 "%s(): Generate VS keyword detected event to User space\n",
			 __func__);

		es705_vs_event(es705);
	}
}

irqreturn_t es705_irq_event(int irq, void *irq_data)
{
	struct es705_priv *es705 = (struct es705_priv *)irq_data;
	u32 cmd_stop[] = { 0x9017e000, 0x90180000, 0xffffffff };

	mutex_lock(&es705->cvq_mutex);
	dev_info(es705->dev, "%s(): %s mode, Interrupt event", __func__,
		 esxxx_mode[es705->mode]);

	if (es705_priv.voice_wakeup_enable == 0) {
		dev_info(es705->dev,
			 "%s : voice wakeup disable setting, skip interrupt\n",
			 __func__);
		mutex_unlock(&es705->cvq_mutex);
		return IRQ_HANDLED;
	}
	/* Get Event status, reset Interrupt */
	es705_event_status(es705);

	mutex_unlock(&es705->cvq_mutex);
	if ((es705->voice_wakeup_enable == 2) ||
	    ((es705->voice_wakeup_enable == 1) &&
	     (es705->detected_vs_keyword == 2)))
		es705_write_block(es705, cmd_stop);

	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_SND_SOC_ES_SLIM)
static int register_snd_soc(struct es705_priv *priv)
{
	int rc = 0;
	int i;
	int ch_cnt;
	struct slim_device *sbdev = priv->gen0_client;

	es705_init_slim_slave(sbdev);

	dev_dbg(&sbdev->dev, "%s(): name = %s\n", __func__, sbdev->name);

	rc = snd_soc_register_codec(&sbdev->dev, &soc_codec_dev_es705,
				    es705_dai, ARRAY_SIZE(es705_dai));

	dev_dbg(&sbdev->dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n",
		__func__, rc);

	/* allocate ch_num array for each DAI */
	for (i = 0; i < NUM_CODEC_SLIM_DAIS; i++) {
		switch (es705_dai[i].id) {
		case ES705_SLIM_1_PB:
		case ES705_SLIM_2_PB:
		case ES705_SLIM_3_PB:
			ch_cnt = es705_dai[i].playback.channels_max;
			break;
		case ES705_SLIM_1_CAP:
		case ES705_SLIM_2_CAP:
		case ES705_SLIM_3_CAP:
			ch_cnt = es705_dai[i].capture.channels_max;
			break;
		default:
			continue;
		}

		es705_priv.dai[i].ch_num =
		    kzalloc((ch_cnt * sizeof(unsigned int)), GFP_KERNEL);
	}

	es705_slim_map_channels(priv);

	return rc;
}
#endif

int es705_core_init(struct device *dev)
{
	struct esxxx_platform_data *pdata = dev->platform_data;
	int rc = 0;

	if (pdata == NULL) {
		dev_err(dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	es705_priv.dev = dev;
	es705_priv.pdata = pdata;
	es705_priv.fw_requested = 0;
	es705_priv.rdb_wdb_open = es705_uart_open;
	es705_priv.rdb_wdb_close = es705_uart_close;

	if (es705_priv.rdb_wdb_open && es705_priv.rdb_wdb_close) {
		es705_priv.rdb_read = es705_uart_read;
		es705_priv.wdb_write = es705_uart_write;
	} else {
		es705_priv.rdb_read = es705_priv.dev_read;
		es705_priv.wdb_write = es705_priv.dev_write;
	}

	es705_clk_init(&es705_priv);

#if defined(CONFIG_SND_SOC_ESXXX_UART_FW_DOWNLOAD)
	/*
	 * Set UART interface as a default for STANDARD FW
	 * download at boot time. And select Baud Rate 3MBPs
	 */
	es705_priv.uart_fw_download = es705_uart_fw_download;
	es705_priv.uart_fw_download_rate = 3;
#else
	/*
	 * Set default interface for STANDARD FW
	 * download at boot time.
	 */
	es705_priv.uart_fw_download = NULL;
	es705_priv.uart_fw_download_rate = 0;
#endif

#if defined(CONFIG_SND_SOC_ESXXX_UART_WAKEUP)
	/* Select UART eS705 Wakeup mechanism */
	es705_priv.uart_es705_wakeup = es705_uart_es705_wakeup;
#else
	es705_priv.uart_es705_wakeup = NULL;
#endif
	es705_priv.power_control = es705_power_control;

	es705_priv.uart_state = UART_CLOSE;
	es705_priv.sleep_delay = ES705_SLEEP_DEFAULT_DELAY;
#if defined(SAMSUNG_ES705_FEATURE)
	es705_priv.current_veq = -1;
	es705_priv.current_veq_preset = 0;
	es705_priv.current_bwe = 0;
#endif
	es705_priv.datablockdev = uart_datablockdev;
	es705_priv.detected_vs_keyword = 0xFFFF;
	/* default : 1000 ms (for "Hi Galaxy") */
	es705_priv.vs_keyword_length = 1000;

	/* No keyword parameters available until set. */
	es705_priv.vs_keyword_param_size = 0;

	rc = sysfs_create_group(&es705_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(es705_priv.dev,
			"%s(): failed to create core sysfs entries: %d\n",
			__func__, rc);
	}
#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
	es705_priv.svoice = class_create(THIS_MODULE, "svoice");
	if (IS_ERR(es705_priv.svoice))
		pr_err("Failed to create class(svoice)!\n");

	es705_priv.keyword =
	    device_create(es705_priv.svoice, NULL, 0, NULL, "keyword");
	if (IS_ERR(es705_priv.keyword))
		pr_err("Failed to create device(keyword)!\n");

	rc = device_create_file(es705_priv.keyword, &dev_attr_keyword_type);
	if (rc)
		pr_err("Failed to create device file in sysfs entries(%s)!\n",
		       dev_attr_keyword_type.attr.name);
#endif

#if defined(CONFIG_SND_SOC_ES_I2C)
	earsmart_class = class_create(THIS_MODULE, "earsmart");
	if (IS_ERR(earsmart_class))
		pr_err("Failed to create class(earsmart)!\n");

	control_dev =
	    device_create(earsmart_class, NULL, 0, NULL, "control");
	if (IS_ERR(control_dev))
		pr_err("Failed to create device(control)!\n");

	rc = sysfs_create_group(&control_dev->kobj, &earsmart_attr_group);
	if (rc) {
		dev_err(es705_priv.dev,
			"%s(): failed to create earsmart_sysfs_attrs entries: %d\n",
			__func__, rc);
	}
#endif

	INIT_DELAYED_WORK(&es705_priv.sleep_work, es705_delayed_sleep);
#if defined(CHECK_ROUTE_STATUS_AND_RECONFIG)
	INIT_DELAYED_WORK(&chk_route_st_and_recfg_work, chk_route_st_and_recfg);
#endif

#if defined(SAMSUNG_ES705_FEATURE)
	rc = es705_init_input_device(&es705_priv);
	if (rc < 0)
		goto init_input_device_error;
#endif

	rc = es705_gpio_init(&es705_priv);
	if (rc)
		goto gpio_init_error;

	if (es705_priv.pdata->fw_filename == NULL) {
		es705_priv.pdata->fw_filename = FW_FILENAME;
		dev_info(es705_priv.dev, "%s: fw_filename is NULL, Set as %s", __func__, es705_priv.pdata->fw_filename);
	}

#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
	if (es705_priv.pdata->vs_filename == NULL) {
		es705_priv.pdata->vs_filename = VS_FILENAME;
		dev_info(es705_priv.dev, "%s: vs_filename is NULL, Set as %s", __func__, es705_priv.pdata->vs_filename);
	}
#endif

#if defined(CONFIG_SLIMBUS_MSM_NGD)
#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
	/* VS firmware */
	rc = request_firmware((const struct firmware **)&es705_priv.vs,
			      es705_priv.pdata->vs_filename, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev,
			"%s(): request_firmware(%s) failed %d\n", __func__,
			es705_priv.pdata->vs_filename, rc);
		goto request_vs_firmware_error;
	}
#endif
	rc = request_firmware((const struct firmware **)&es705_priv.standard,
			      es705_priv.pdata->fw_filename, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev,
			"%s(): request_firmware(%s) failed %d\n", __func__,
			es705_priv.pdata->fw_filename, rc);
		goto request_firmware_error;
	}

	es705_priv.pm_state = ES705_POWER_FW_LOAD;
	es705_priv.fw_requested = 1;
#else
#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
	/* VS firmware */
	request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				es705_priv.pdata->vs_filename, es705_priv.dev, GFP_KERNEL,
				&es705_priv, es705_vs_fw_loaded);
#endif
	request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				es705_priv.pdata->fw_filename, es705_priv.dev, GFP_KERNEL,
				&es705_priv, es705_std_fw_loaded);
#endif
	if (!es705_get_stimulate_status()) {
		if (pdata->esxxx_clk_cb) {
			pdata->esxxx_clk_cb(es705_priv.dev, 1);
			dev_info(es705_priv.dev, "%s(): external clock on\n",
				 __func__);
		}
		usleep_range(5000, 5100);
		es705_gpio_reset(&es705_priv);
	}

	return rc;

#if defined(CONFIG_SLIMBUS_MSM_NGD)
#if defined(CONFIG_SND_SOC_ESXXX_SEAMLESS_VOICE_WAKEUP)
request_vs_firmware_error:
#endif
	release_firmware(es705_priv.standard);
request_firmware_error:
#endif
gpio_init_error:
#if defined(SAMSUNG_ES705_FEATURE)
init_input_device_error:
#endif
pdata_error:
	dev_dbg(es705_priv.dev, "%s(): exit with error\n", __func__);
	return rc;
}
EXPORT_SYMBOL_GPL(es705_core_init);

static __init int es705_init(void)
{
	int rc = 0;

	mutex_init(&es705_priv.api_mutex);
	mutex_init(&es705_priv.pm_mutex);
	mutex_init(&es705_priv.cvq_mutex);
	mutex_init(&es705_priv.streaming_mutex);

	init_waitqueue_head(&es705_priv.stream_in_q);

#if defined(CONFIG_SND_SOC_ES_SLIM)
	rc = es705_slimbus_init(&es705_priv);
	if (!rc) {
		pr_debug("%s(): registered as SLIMBUS", __func__);
		es705_priv.intf = ES705_SLIM_INTF;
	}
#elif defined(CONFIG_SND_SOC_ES_I2C)
	rc = es705_i2c_init(&es705_priv);
	if (!rc) {
		pr_debug("%s(): registered as I2C", __func__);
		es705_priv.intf = ES705_I2C_INTF;
	}
#elif defined(CONFIG_SND_SOC_ES_SPI)
	rc = spi_register_driver(&es705_spi_driver);
	if (!rc) {
		pr_debug("%s() registered as SPI", __func__);
		es705_priv.intf = ES705_SPI_INTF;
	}
#elif defined(CONFIG_SND_SOC_ES_UART)
	rc = es705_uart_bus_init(&es705_priv);
	if (!rc) {
		pr_debug("%s() registered as UART", __func__);
		es705_priv.intf = ES705_UART_INTF;
	}
#else
	/* Need Bus Specifc Registration */
#endif

	if (rc) {
		pr_debug("Error registering Audience es705 driver: %d\n", rc);
		goto INIT_ERR;
	}
#if !defined(CONFIG_SND_SOC_ES705_UART)
	/* If CONFIG_SND_SOC_ES705_UART,
	* UART probe will initialize char device if a es705 is found
	*/
	rc = es705_init_cdev(&es705_priv);
	if (rc) {
		pr_err("es705 failed to initialize char device = %d\n", rc);
		goto INIT_ERR;
	}
#endif

INIT_ERR:
	return rc;
}

module_init(es705_init);

static __exit void es705_exit(void)
{
#if defined(SAMSUNG_ES705_FEATURE)
	es705_unregister_input_device(&es705_priv);
#endif

	if (es705_priv.fw_requested) {
		release_firmware(es705_priv.standard);
		release_firmware(es705_priv.vs);
	}

	es705_cleanup_cdev(&es705_priv);

#if defined(CONFIG_SND_SOC_ES_SLIM)
	slim_driver_unregister(&es70x_slim_driver);
#elif defined(CONFIG_SND_SOC_ES_I2C)
	i2c_del_driver(&es705_i2c_driver);
#elif defined(CONFIG_SND_SOC_ES_SPI)
	spi_unregister_driver(&es705_spi_driver);
#else
	/* Need Bus Specifc Registration */
#endif
}

module_exit(es705_exit);

MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_AUTHOR("Genisim Tsilker <gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
MODULE_FIRMWARE("audience-es705-fw.bin");
