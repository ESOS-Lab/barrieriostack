/*
 * es705-slim.c  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 * Code Updates:
 *	Genisim Tsilker <gtsilker@audience.com>
 *              - Code refactoring
 *              - FW download functions update
 *              - Rewrite esxxx SLIMBus write / read functions
 *              - Add write_then_read function
 *              - Unify log messages format
 *              - Add detection eS70x (704 / 705)
 *                when using SLIMBus v2 (QDSP) and DTS
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
#include <linux/i2c.h>
#include <linux/slimbus/slimbus.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/slimbus/slimbus.h>
#include <linux/esxxx.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "es705.h"
#include "es705-platform.h"
#include "es705-uart-common.h"

#define CODEC_ID		"earSmart-codec"
#define CODEC_INTF_ID		"es70x-codec-intf"

#define CODEC_GEN0_ID_ES704	"es704-codec-gen0"
#define CODEC_GEN0_ID_ES705	"es705-codec-gen0"
#define CODEC_GEN0_ID_ES804	"es804-codec-gen0"

#define CODEC_INTF_PROP		"slim-ifd"
#define CODEC_ELEMENTAL_ADDR	"slim-ifd-elemental-addr"

#define ES705_SLIM_1_PB_MAX_CHANS	2
#define ES705_SLIM_1_CAP_MAX_CHANS	2
#define ES705_SLIM_2_PB_MAX_CHANS	2
#define ES705_SLIM_2_CAP_MAX_CHANS	2
#define ES705_SLIM_3_PB_MAX_CHANS	4
#define ES705_SLIM_3_CAP_MAX_CHANS	2

#define ES705_SLIM_1_PB_OFFSET	0
#define ES705_SLIM_2_PB_OFFSET	2
#define ES705_SLIM_3_PB_OFFSET	4
#define ES705_SLIM_1_CAP_OFFSET	0
#define ES705_SLIM_2_CAP_OFFSET	2
#define ES705_SLIM_3_CAP_OFFSET	4

/*
 * Delay for receiving response can be up to 20 ms.
 * To minimize waiting time, response is checking
 * up to 20 times with 1ms delay.
 */
#define MAX_SMB_TRIALS	3
#define MAX_WRITE_THEN_READ_TRIALS	5
#define SMB_DELAY	1000

#if defined(CONFIG_WCD9306_CODEC)

#define SMB_RX_PORT0 152	/*  AP to ADNC */
#define SMB_RX_PORT1 153	/*  AP to ADNC */
#define SMB_RX_PORT2 154	/*  CP to ADNC */
#define SMB_RX_PORT3 155	/*  CP to ADNC */
#define SMB_RX_PORT4 128	/*  Codec to ADNC */
#define SMB_RX_PORT5 129	/*  Codec to ADNC */

#define SMB_TX_PORT0 156	/*  ADNC to CP or AP */
#define SMB_TX_PORT1 157	/*  ADNC to CP or AP */
#define SMB_TX_PORT2 144	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT3 145	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT4 144	/*  ADNC(CP) to Codec */
#define SMB_TX_PORT5 145	/*  ADNC(CP) to Codec */

#elif defined(CONFIG_WCD9310_CODEC)

#define SMB_RX_PORT0 152	/*  AP to ADNC */
#define SMB_RX_PORT1 153	/*  AP to ADNC */
#define SMB_RX_PORT2 154	/*  CP to ADNC */
#define SMB_RX_PORT3 155	/*  CP to ADNC */
#define SMB_RX_PORT4 134	/*  Codec to ADNC */
#define SMB_RX_PORT5 135	/*  Codec to ADNC */

#define SMB_TX_PORT0 156	/*  ADNC to CP or AP */
#define SMB_TX_PORT1 157	/*  ADNC to CP or AP */
#define SMB_TX_PORT2 138	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT3 139	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT4 143	/*  ADNC(CP) to Codec */
#define SMB_TX_PORT5 144	/*  ADNC(CP) to Codec */

#elif defined(CONFIG_WCD9320_CODEC)

#define SMB_RX_PORT0 152	/*  AP to ADNC */
#define SMB_RX_PORT1 153	/*  AP to ADNC */
#define SMB_RX_PORT2 154	/*  CP to ADNC */
#define SMB_RX_PORT3 155	/*  CP to ADNC */
#define SMB_RX_PORT4 134	/*  Codec to ADNC */
#define SMB_RX_PORT5 135	/*  Codec to ADNC */

#define SMB_TX_PORT0 156	/*  ADNC to CP or AP */
#define SMB_TX_PORT1 157	/*  ADNC to CP or AP */
#define SMB_TX_PORT2 144	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT3 145	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT4 144	/*  ADNC(CP) to Codec */
#define SMB_TX_PORT5 145	/*  ADNC(CP) to Codec */

#elif defined(CONFIG_WCD9330_CODEC)

#define SMB_RX_PORT0 152	/*  AP to ADNC */
#define SMB_RX_PORT1 153	/*  AP to ADNC */
#define SMB_RX_PORT2 154	/*  CP to ADNC */
#define SMB_RX_PORT3 155	/*  CP to ADNC */
#define SMB_RX_PORT4 134	/*  Codec to ADNC */
#define SMB_RX_PORT5 135	/*  Codec to ADNC */

#define SMB_TX_PORT0 156	/*  ADNC to CP or AP */
#define SMB_TX_PORT1 157	/*  ADNC to CP or AP */
#define SMB_TX_PORT2 144	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT3 145	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT4 149	/*  ADNC(CP) to Codec */
#define SMB_TX_PORT5 150	/*  ADNC(CP) to Codec */

#else

#define SMB_RX_PORT0 152	/*  AP to ADNC */
#define SMB_RX_PORT1 153	/*  AP to ADNC */
#define SMB_RX_PORT2 154	/*  CP to ADNC */
#define SMB_RX_PORT3 155	/*  CP to ADNC */
#define SMB_RX_PORT4 134	/*  Codec to ADNC */
#define SMB_RX_PORT5 135	/*  Codec to ADNC */

#define SMB_TX_PORT0 156	/*  ADNC to CP or AP */
#define SMB_TX_PORT1 157	/*  ADNC to CP or AP */
#define SMB_TX_PORT2 144	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT3 145	/*  ADNC(AP) to Codec */
#define SMB_TX_PORT4 144	/*  ADNC(CP) to Codec */
#define SMB_TX_PORT5 145	/*  ADNC(CP) to Codec */

#endif

static int es705_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate);
static int es705_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate);
static int es705_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt);
static int es705_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt);

static int es705_slim_probe(struct slim_device *sbdev);

static const struct slim_device_id es70x_slim_id[] = {
	{CODEC_ID, 0},
	{CODEC_INTF_ID, 0},
	{CODEC_GEN0_ID_ES804, 0},
	{CODEC_GEN0_ID_ES704, 0},
	{CODEC_GEN0_ID_ES705, 0},
	{}
};

MODULE_DEVICE_TABLE(slim, es70x_slim_id);

static int es705_slim_rx_port_to_ch[ES705_SLIM_RX_PORTS] = {
	SMB_RX_PORT0, SMB_RX_PORT1,
	SMB_RX_PORT2, SMB_RX_PORT3,
	SMB_RX_PORT4, SMB_RX_PORT5,
#if defined(CONFIG_SND_SOC_3MIC_SUPPORT)
	SMB_RX_PORT5 + 1, SMB_RX_PORT5 + 2
#endif
};

static int es705_slim_tx_port_to_ch[ES705_SLIM_TX_PORTS] = {
	SMB_TX_PORT0, SMB_TX_PORT1,
	SMB_TX_PORT2, SMB_TX_PORT3,
	SMB_TX_PORT4, SMB_TX_PORT5
};

static int es705_slim_be_id[NUM_CODEC_SLIM_DAIS] = {
	ES705_SLIM_2_CAP,	/* for ES705_SLIM_1_PB tx from es705 */
	ES705_SLIM_3_PB,	/* for ES705_SLIM_1_CAP rx to es705 */
	ES705_SLIM_3_CAP,	/* for ES705_SLIM_2_PB tx from es705 */
	-1,			/* for ES705_SLIM_2_CAP */
	-1,			/* for ES705_SLIM_3_PB */
	-1,			/* for ES705_SLIM_3_CAP */
};

static int es705_rx_ch_num_to_idx(int ch_num)
{
	int i;
	int idx = -1;

	for (i = 0; i < ES705_SLIM_RX_PORTS; i++) {
		if (ch_num == es705_slim_rx_port_to_ch[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int es705_tx_ch_num_to_idx(int ch_num)
{
	int i;
	int idx = -1;

	for (i = 0; i < ES705_SLIM_TX_PORTS; i++) {
		if (ch_num == es705_slim_tx_port_to_ch[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

/* es705 -> codec - alsa playback function */
static int es705_codec_cfg_slim_tx(struct es705_priv *es705, int dai_id)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	/* start slim channels associated with id */
	rc = es705_cfg_slim_tx(es705->gen0_client,
			       es705->dai[DAI_INDEX(dai_id)].ch_num,
			       es705->dai[DAI_INDEX(dai_id)].ch_tot,
			       es705->dai[DAI_INDEX(dai_id)].rate);

	return rc;
}

/* es705 <- codec - alsa capture function */
static int es705_codec_cfg_slim_rx(struct es705_priv *es705, int dai_id)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	/* start slim channels associated with id */
	rc = es705_cfg_slim_rx(es705->gen0_client,
			       es705->dai[DAI_INDEX(dai_id)].ch_num,
			       es705->dai[DAI_INDEX(dai_id)].ch_tot,
			       es705->dai[DAI_INDEX(dai_id)].rate);

	return rc;
}

/* es705 -> codec - alsa playback function */
static int es705_codec_close_slim_tx(struct es705_priv *es705, int dai_id)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	/* close slim channels associated with id */
	rc = es705_close_slim_tx(es705->gen0_client,
				 es705->dai[DAI_INDEX(dai_id)].ch_num,
				 es705->dai[DAI_INDEX(dai_id)].ch_tot);

	return rc;
}

/* es705 <- codec - alsa capture function */
static int es705_codec_close_slim_rx(struct es705_priv *es705, int dai_id)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	/* close slim channels associated with id */
	rc = es705_close_slim_rx(es705->gen0_client,
				 es705->dai[DAI_INDEX(dai_id)].ch_num,
				 es705->dai[DAI_INDEX(dai_id)].ch_tot);

	return rc;
}

static void es705_alloc_slim_rx_chan(struct slim_device *sbdev)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *rx = es705_priv->slim_rx;
	int i;
	int port_id;

	for (i = 0; i < ES705_SLIM_RX_PORTS; i++) {
		port_id = i;
		rx[i].ch_num = es705_slim_rx_port_to_ch[i];
		slim_get_slaveport(sbdev->laddr, port_id, &rx[i].sph,
				   SLIM_SINK);
		slim_query_ch(sbdev, rx[i].ch_num, &rx[i].ch_h);
	}
}

static void es705_alloc_slim_tx_chan(struct slim_device *sbdev)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *tx = es705_priv->slim_tx;
	int i;
	int port_id;

	for (i = 0; i < ES705_SLIM_TX_PORTS; i++) {
		port_id = i + 10;	/* ES705_SLIM_RX_PORTS; */
		tx[i].ch_num = es705_slim_tx_port_to_ch[i];
		slim_get_slaveport(sbdev->laddr, port_id, &tx[i].sph, SLIM_SRC);
		slim_query_ch(sbdev, tx[i].ch_num, &tx[i].ch_h);
	}
}

static int es705_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *rx = es705_priv->slim_rx;
	struct slim_ch prop;

	u16 grph;
	u32 sph[ES705_SLIM_RX_PORTS] = { 0 };
	u16 ch_h[ES705_SLIM_RX_PORTS] = { 0 };
	int i;
	int idx;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(): ch_cnt = %d, rate = %d\n", __func__,
		 ch_cnt, rate);

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n", __func__,
			ch_num[i]);
		idx = es705_rx_ch_num_to_idx(ch_num[i]);
		ch_h[i] = rx[idx].ch_h;
		sph[i] = rx[idx].sph;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate / 4000);
	prop.sampleszbits = 16;

	rc = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, rc);
		goto slim_define_ch_error;
	}

	for (i = 0; i < ch_cnt; i++) {
		rc = slim_connect_sink(sbdev, &sph[i], 1, ch_h[i]);
		if (rc < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_connect_sink() failed: %d\n",
				__func__, rc);
			goto slim_connect_sink_error;
		}
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}

	for (i = 0; i < ch_cnt; i++) {
		dev_info(&sbdev->dev, "%s(): ch_num = %d\n", __func__,
			 ch_num[i]);
		idx = es705_rx_ch_num_to_idx(ch_num[i]);
		rx[idx].grph = grph;
	}

	return rc;

slim_control_ch_error:
slim_connect_sink_error:
	es705_close_slim_rx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	return rc;
}

static int es705_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *tx = es705_priv->slim_tx;
	struct slim_ch prop;

	u16 grph;
	u32 sph[ES705_SLIM_TX_PORTS] = { 0 };
	u16 ch_h[ES705_SLIM_TX_PORTS] = { 0 };
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): ch_cnt = %d, rate = %d\n", __func__, ch_cnt,
		rate);

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n", __func__,
			ch_num[i]);
		idx = es705_tx_ch_num_to_idx(ch_num[i]);
		ch_h[i] = tx[idx].ch_h;
		sph[i] = tx[idx].sph;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate / 4000);
	prop.sampleszbits = 16;

	rc = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, rc);
		goto slim_define_ch_error;
	}

	for (i = 0; i < ch_cnt; i++) {
		rc = slim_connect_src(sbdev, sph[i], ch_h[i]);
		if (rc < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_connect_src() failed: %d\n",
				__func__, rc);
			dev_err(&sbdev->dev, "%s(): ch_num[0] = %d\n", __func__,
				ch_num[0]);
			goto slim_connect_src_error;
		}
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}

	for (i = 0; i < ch_cnt; i++) {
		dev_info(&sbdev->dev, "%s(): ch_num = %d\n", __func__,
			 ch_num[i]);
		idx = es705_tx_ch_num_to_idx(ch_num[i]);
		tx[idx].grph = grph;
	}

	return rc;

slim_control_ch_error:
slim_connect_src_error:
	es705_close_slim_tx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	return rc;
}

static int es705_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *rx = es705_priv->slim_rx;

	u16 grph = 0;
	u32 sph[ES705_SLIM_RX_PORTS] = { 0 };
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): ch_cnt = %d\n", __func__, ch_cnt);

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n", __func__,
			ch_num[i]);
		idx = es705_rx_ch_num_to_idx(ch_num[i]);
		sph[i] = rx[idx].sph;
		grph = rx[idx].grph;
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed try one more: %d\n",
			__func__, rc);
		msleep(200);

		rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
		if (rc < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_control_ch() failed : %d\n",
				__func__, rc);
		}
		goto slim_control_ch_error;
	}

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n", __func__,
			ch_num[i]);
		idx = es705_rx_ch_num_to_idx(ch_num[i]);
		rx[idx].grph = 0;
	}

	rc = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_disconnect_ports() failed: %d\n", __func__,
			rc);
	}

slim_control_ch_error:
	return rc;
}

static int es705_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *tx = es705_priv->slim_tx;

	u16 grph = 0;
	u32 sph[ES705_SLIM_TX_PORTS] = { 0 };
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): ch_cnt = %d\n", __func__, ch_cnt);

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n", __func__,
			ch_num[i]);
		idx = es705_tx_ch_num_to_idx(ch_num[i]);
		sph[i] = tx[idx].sph;
		grph = tx[idx].grph;
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_connect_sink() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n", __func__,
			ch_num[i]);
		idx = es705_tx_ch_num_to_idx(ch_num[i]);
		tx[idx].grph = 0;
	}

	rc = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_disconnect_ports() failed: %d\n", __func__,
			rc);
	}

slim_control_ch_error:
	return rc;
}

int es705_remote_cfg_slim_rx(int dai_id)
{
	struct es705_priv *es705 = &es705_priv;
	struct slim_device *sbdev = es705->gen0_client;
	int be_id;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	if (dai_id != ES705_SLIM_1_PB && dai_id != ES705_SLIM_2_PB)
		return rc;

	if (es705->dai[DAI_INDEX(dai_id)].ch_tot != 0) {
		/* start slim channels associated with id */
		rc = es705_cfg_slim_rx(sbdev,
				       es705->dai[DAI_INDEX(dai_id)].ch_num,
				       es705->dai[DAI_INDEX(dai_id)].ch_tot,
				       es705->dai[DAI_INDEX(dai_id)].rate);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		es705->dai[DAI_INDEX(be_id)].ch_tot =
		    es705->dai[DAI_INDEX(dai_id)].ch_tot;
		es705->dai[DAI_INDEX(be_id)].rate =
		    es705->dai[DAI_INDEX(dai_id)].rate;
		rc = es705_codec_cfg_slim_tx(es705, be_id);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_remote_cfg_slim_rx);

int es705_remote_cfg_slim_tx(int dai_id)
{
	struct es705_priv *es705 = &es705_priv;
	struct slim_device *sbdev = es705->gen0_client;
	int be_id;
	int ch_cnt;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	if (dai_id != ES705_SLIM_1_CAP)
		return rc;

	if (es705->dai[DAI_INDEX(dai_id)].ch_tot != 0) {
		/* start slim channels associated with id */
		ch_cnt = es705->ap_tx1_ch_cnt;
#ifdef CONFIG_SND_SOC_3MIC_SUPPORT
		rc = es705_cfg_slim_tx(es705->gen0_client,
				       es705->dai[DAI_INDEX(dai_id)].ch_num,
				       es705->dai[DAI_INDEX(dai_id)].ch_tot,
				       es705->dai[DAI_INDEX(dai_id)].rate);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		es705->dai[DAI_INDEX(be_id)].ch_tot = ch_cnt;
		es705->dai[DAI_INDEX(be_id)].rate =
		    es705->dai[DAI_INDEX(dai_id)].rate;
		rc = es705_codec_cfg_slim_rx(es705, be_id);
#else
		rc = es705_cfg_slim_tx(es705->gen0_client,
				       es705->dai[DAI_INDEX(dai_id)].ch_num,
				       ch_cnt,
				       es705->dai[DAI_INDEX(dai_id)].rate);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		es705->dai[DAI_INDEX(be_id)].ch_tot =
		    es705->dai[DAI_INDEX(dai_id)].ch_tot;
		es705->dai[DAI_INDEX(be_id)].rate =
		    es705->dai[DAI_INDEX(dai_id)].rate;
		rc = es705_codec_cfg_slim_rx(es705, be_id);
#endif
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_remote_cfg_slim_tx);

int es705_remote_close_slim_rx(int dai_id)
{
	struct es705_priv *es705 = &es705_priv;
	struct slim_device *sbdev = es705->gen0_client;
	int be_id;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	if (dai_id != ES705_SLIM_1_PB && dai_id != ES705_SLIM_2_PB)
		return rc;

	if (es705->dai[DAI_INDEX(dai_id)].ch_tot != 0) {
		dev_info(&sbdev->dev, "%s(): dai_id = %d, ch_tot =%d\n",
			 __func__, dai_id,
			 es705->dai[DAI_INDEX(dai_id)].ch_tot);

		es705_close_slim_rx(es705->gen0_client,
				    es705->dai[DAI_INDEX(dai_id)].ch_num,
				    es705->dai[DAI_INDEX(dai_id)].ch_tot);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		rc = es705_codec_close_slim_tx(es705, be_id);

		es705->dai[DAI_INDEX(dai_id)].ch_tot = 0;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_remote_close_slim_rx);

int es705_remote_close_slim_tx(int dai_id)
{
	struct es705_priv *es705 = &es705_priv;
	struct slim_device *sbdev = es705->gen0_client;
	int be_id;
	int ch_cnt = 0;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	if (dai_id != ES705_SLIM_1_CAP)
		return rc;

	if (es705->dai[DAI_INDEX(dai_id)].ch_tot != 0) {
		dev_info(&sbdev->dev, "%s(): dai_id = %d, ch_tot = %d\n",
			 __func__, dai_id,
			 es705->dai[DAI_INDEX(dai_id)].ch_tot);
#if defined(SAMSUNG_ES705_FEATURE)
		if (dai_id == ES705_SLIM_1_CAP)
#endif
			ch_cnt = es705->ap_tx1_ch_cnt;
#ifdef CONFIG_SND_SOC_3MIC_SUPPORT
		es705_close_slim_tx(es705->gen0_client,
				    es705->dai[DAI_INDEX(dai_id)].ch_num,
				    es705->dai[DAI_INDEX(dai_id)].ch_tot);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		es705->dai[DAI_INDEX(be_id)].ch_tot = ch_cnt;
		rc = es705_codec_close_slim_rx(es705, be_id);
#else
		es705_close_slim_tx(es705->gen0_client,
				    es705->dai[DAI_INDEX(dai_id)].ch_num,
				    ch_cnt);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		rc = es705_codec_close_slim_rx(es705, be_id);
#endif

		es705->dai[DAI_INDEX(dai_id)].ch_tot = 0;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_remote_close_slim_tx);

void es705_init_slim_slave(struct slim_device *sbdev)
{
	es705_alloc_slim_rx_chan(sbdev);
	es705_alloc_slim_tx_chan(sbdev);
}

static int es705_slim_read(struct es705_priv *es705, void *rspn, int len)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc = 0;
	int i;

	struct slim_ele_access msg = {
		.start_offset = ES705_READ_VE_OFFSET,
		.num_bytes = ES705_READ_VE_WIDTH,
		.comp = NULL,
	};
	BUG_ON(len < 0);

	for (i = 0; i < MAX_SMB_TRIALS; i++) {
		char buf[4] = { 0 };

		rc = slim_request_val_element(sbdev, &msg, buf, 4);
		memcpy(rspn, buf, 4);
		if (!rc)
			break;

		usleep_range(SMB_DELAY, SMB_DELAY+100);
	}

	if (i == MAX_SMB_TRIALS)
		dev_err(&sbdev->dev, "%s(): reach SLIMBus read trials (%d)\n",
			__func__, MAX_SMB_TRIALS);

	return rc;
}

static int es705_slim_write(struct es705_priv *es705, const void *buf, int len)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc = 0;
	int wr = 0;

	struct slim_ele_access msg = {
		.start_offset = ES705_WRITE_VE_OFFSET,
		.num_bytes = ES705_WRITE_VE_WIDTH,
		.comp = NULL,
	};

	BUG_ON(len < 0);

	while (wr < len) {
		int i;
		int sz = min(len - wr, ES705_WRITE_VE_WIDTH);

		/*
		 * As long as the caller expects the most significant
		 * bytes of the VE value written to be zero, this is
		 * valid.
		 */

		for (i = 0; i < MAX_SMB_TRIALS; i++) {
			if (sz < ES705_WRITE_VE_WIDTH)
				dev_dbg(&sbdev->dev,
					"%s(): write smaller than VE size %d < %d\n",
					__func__, sz, ES705_WRITE_VE_WIDTH);

			rc = slim_change_val_element(sbdev, &msg,
						     (char *)buf + wr, sz);
			if (!rc)
				break;

			usleep_range(SMB_DELAY, SMB_DELAY+100);
		}

		if (i == MAX_SMB_TRIALS) {
			dev_err(&sbdev->dev, "%s(): reach MAX_TRIALS (%d)\n",
				__func__, MAX_SMB_TRIALS);
			break;
		}

		wr += sz;
	}

	return rc;
}

static int es705_slim_write_then_read(struct es705_priv *es705,
				      const void *buf, int len,
				      u32 *rspn, int match)
{
	struct slim_device *sbdev = es705->gen0_client;
	const u32 NOT_READY = 0;
	u32 response = NOT_READY;
	int rc = 0;
	int trials = 0;

	/*
	 * WARNING: for single success write is
	 * possible multiple reads, because of
	 * esxxx needs time to handle request
	 * In this case read function got 0x00000000
	 * what means no response from esxxx
	 */
	rc = es705_slim_write(es705, buf, len);
	if (rc)
		goto es705_slim_write_then_read_exit;

	do {
		usleep_range(SMB_DELAY * 4, SMB_DELAY * 4 + 500);

		rc = es705_slim_read(es705, &response, 4);
		if (rc)
			break;

		if (response != NOT_READY) {
			if (match && *rspn != response) {
				dev_err(&sbdev->dev,
					"%s(): unexpected response 0x%08x\n",
					__func__, response);
				rc = -EIO;
			}
			*rspn = response;
			break;
		} else {
			rc = -ETIMEDOUT;
		}
		trials++;
	} while (trials < MAX_WRITE_THEN_READ_TRIALS);

	if (rc == -ETIMEDOUT)
		dev_err(&sbdev->dev, "%s(): response=0x%08x\n", __func__,
			response);

es705_slim_write_then_read_exit:
	return rc;
}

int es705_slim_cmd(struct es705_priv *es705, u32 cmd, int sr, u32 *resp)
{
	int rc;

	dev_dbg(es705->dev, "%s(): cmd=0x%08x  sr=%i\n", __func__, cmd, sr);

	cmd = cpu_to_le32(cmd);

	if (sr) {
		dev_dbg(es705->dev, "%s(): Response not expected\n", __func__);
		rc = es705_slim_write(es705, &cmd, sizeof(cmd));
	} else {
		u32 rv;
		int match = 0;

		rc = es705_slim_write_then_read(es705, &cmd, sizeof(cmd), &rv,
						match);
		if (!rc) {
			*resp = le32_to_cpu(rv);
			dev_dbg(es705->dev, "%s(): resp=0x%08x\n", __func__,
				*resp);
		}
	}

	return rc;
}

int es705_slim_set_channel_map(struct snd_soc_dai *dai,
			       unsigned int tx_num, unsigned int *tx_slot,
			       unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es705_priv *es705 = &es705_priv;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES705_SLIM_2_CAP)
		id = ES705_SLIM_1_CAP;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES705_SLIM_1_PB || id == ES705_SLIM_2_PB
	    || id == ES705_SLIM_3_PB) {
		es705->dai[DAI_INDEX(id)].ch_tot = rx_num;
		es705->dai[DAI_INDEX(id)].ch_act = 0;
		for (i = 0; i < rx_num; i++)
			es705->dai[DAI_INDEX(id)].ch_num[i] = rx_slot[i];
	} else if (id == ES705_SLIM_1_CAP || id == ES705_SLIM_2_CAP
		   || id == ES705_SLIM_3_CAP) {
		es705->dai[DAI_INDEX(id)].ch_tot = tx_num;
		es705->dai[DAI_INDEX(id)].ch_act = 0;
		for (i = 0; i < tx_num; i++)
			es705->dai[DAI_INDEX(id)].ch_num[i] = tx_slot[i];
	} else {
		dev_dbg(codec->dev, "%s(): No selected id = %d\n", __func__,
			id);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_slim_set_channel_map);

#if defined(CONFIG_ARCH_MSM)
int es705_slim_get_channel_map(struct snd_soc_dai *dai,
			       unsigned int *tx_num, unsigned int *tx_slot,
			       unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es705_priv *es705 = &es705_priv;
	struct es705_slim_ch *rx = es705->slim_rx;
	struct es705_slim_ch *tx = es705->slim_tx;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES705_SLIM_2_CAP)
		id = ES705_SLIM_1_CAP;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, id);

	if (id == ES705_SLIM_1_PB) {
		*rx_num = es705_dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES705_SLIM_1_PB_OFFSET + i].ch_num;
	} else if (id == ES705_SLIM_2_PB) {
		*rx_num = es705_dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES705_SLIM_2_PB_OFFSET + i].ch_num;
	} else if (id == ES705_SLIM_3_PB) {
		*rx_num = es705_dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES705_SLIM_3_PB_OFFSET + i].ch_num;
	} else if (id == ES705_SLIM_1_CAP) {
		*tx_num = es705_dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES705_SLIM_1_CAP_OFFSET + i].ch_num;
	} else if (id == ES705_SLIM_2_CAP) {
		*tx_num = es705_dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES705_SLIM_2_CAP_OFFSET + i].ch_num;
	} else if (id == ES705_SLIM_3_CAP) {
		*tx_num = es705_dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES705_SLIM_3_CAP_OFFSET + i].ch_num;
	} else {
		dev_dbg(codec->dev, "%s(): No selected id = %d\n", __func__,
			id);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_slim_get_channel_map);
#endif

void es705_slim_map_channels(struct es705_priv *es705)
{
	/* front end for RX1 */
	es705->dai[DAI_INDEX(ES705_SLIM_1_PB)].ch_num[0] = SMB_RX_PORT0;
	es705->dai[DAI_INDEX(ES705_SLIM_1_PB)].ch_num[1] = SMB_RX_PORT1;
	/* front end for RX2 */
	es705->dai[DAI_INDEX(ES705_SLIM_2_PB)].ch_num[0] = SMB_RX_PORT2;
	es705->dai[DAI_INDEX(ES705_SLIM_2_PB)].ch_num[1] = SMB_RX_PORT3;
	/* back end for TX1 */
	es705->dai[DAI_INDEX(ES705_SLIM_3_PB)].ch_num[0] = SMB_RX_PORT4;
	es705->dai[DAI_INDEX(ES705_SLIM_3_PB)].ch_num[1] = SMB_RX_PORT5;
#if defined(CONFIG_SND_SOC_3MIC_SUPPORT)
	es705->dai[DAI_INDEX(ES705_SLIM_3_PB)].ch_num[2] = SMB_RX_PORT5 + 1;
	es705->dai[DAI_INDEX(ES705_SLIM_3_PB)].ch_num[3] = SMB_RX_PORT5 + 2;
#endif

	/* front end for TX1 */
	es705->dai[DAI_INDEX(ES705_SLIM_1_CAP)].ch_num[0] = SMB_TX_PORT0;
	es705->dai[DAI_INDEX(ES705_SLIM_1_CAP)].ch_num[1] = SMB_TX_PORT1;
	/* back end for RX1 */
	es705->dai[DAI_INDEX(ES705_SLIM_2_CAP)].ch_num[0] = SMB_TX_PORT2;
	es705->dai[DAI_INDEX(ES705_SLIM_2_CAP)].ch_num[1] = SMB_TX_PORT3;
	/* back end for RX2 */
	es705->dai[DAI_INDEX(ES705_SLIM_3_CAP)].ch_num[0] = SMB_TX_PORT4;
	es705->dai[DAI_INDEX(ES705_SLIM_3_CAP)].ch_num[1] = SMB_TX_PORT5;
}

int es705_slim_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es705_priv *es705 = &es705_priv;
	int id = dai->id;
	int channels;
	int rate;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES705_SLIM_2_CAP)
		id = ES705_SLIM_1_CAP;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, id);

	channels = params_channels(params);

	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		es705->dai[DAI_INDEX(id)].ch_tot = channels;
		break;
	default:
		dev_err(codec->dev,
			"%s(): unsupported number of channels, %d\n", __func__,
			channels);
		return -EINVAL;
	}

	rate = params_rate(params);

	switch (rate) {
	case 8000:
	case 16000:
	case 32000:
	case 48000:
		es705->dai[DAI_INDEX(id)].rate = rate;
		break;
	default:
		dev_err(codec->dev, "%s(): unsupported rate, %d\n", __func__,
			rate);
		return -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_slim_hw_params);

struct snd_soc_dai_ops es705_slim_port_dai_ops = {
	.set_fmt = NULL,
	.set_channel_map = es705_slim_set_channel_map,
#if defined(CONFIG_ARCH_MSM)
	.get_channel_map = es705_slim_get_channel_map,
#endif
	.set_tristate = NULL,
	.digital_mute = NULL,
	.startup = NULL,
	.shutdown = NULL,
	.hw_params = es705_slim_hw_params,
	.hw_free = NULL,
	.prepare = NULL,
	.trigger = NULL,
};

static int es705_slim_vs_streaming(struct es705_priv *es705)
{
	int rc;
	/*
	 * VS BUFFER via SLIMBus
	 * end point is
	 * 156 - 0x9c for pass through 1 chan mode
	 * 156 - 0x9c and 157 - 0x9d for pass through 2 channels mode
	 */
	u32 vs_stream_end_point = 0x8028809C;
	u32 rspn = vs_stream_end_point;
	int match = 1;
	u32 vs_stream_cmd = 0x90250202 | ES705_STREAM_ENABLE;

	/* select streaming pathID */
	dev_dbg(es705->dev, "%s(): Set VS Streaming PathID\n", __func__);
	rc = es705_slim_write_then_read(es705, &vs_stream_end_point,
					sizeof(vs_stream_end_point), &rspn,
					match);
	if (rc) {
		dev_err(es705->dev, "%s(): Select VS stream Path ID Fail\n",
			__func__);
		goto es705_vs_streaming_error;
	}

	/* enable streaming */
	dev_dbg(es705->dev, "%s(): Enable VS Streaming\n", __func__);
	rc = es705_slim_write(es705, &vs_stream_cmd, 4);
	if (rc) {
		dev_err(es705->dev, "%s(): Enable VS streaming Fail\n",
			__func__);
		goto es705_vs_streaming_error;
	}

	/* TODO wait end of streaming and disable */
	/*
	   vs_stream = 0x90250200 | ES705_STREAM_DISABLE;
	   rc = es705_slim_write(es705, &vs_stream_cmd, 4);
	   if (rc)
	   dev_err(es705->dev, "%s(): Disable VS streaming Fail\n", __func__);
	 */

es705_vs_streaming_error:
	return rc;
}

static int es705_slim_boot_setup(struct es705_priv *es705)
{
	u32 boot_cmd = ES705_BOOT_CMD;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sbl_rspn = ES705_SBL_ACK;
	u32 ack_rspn = ES705_BOOT_ACK;
	int match = 1;
	int rc;

	dev_info(es705->dev, "%s(): prepare for fw download\n", __func__);

	rc = es705_slim_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
					&sbl_rspn, match);
	if (rc) {
		dev_err(es705->dev, "%s(): SYNC_SBL fail\n", __func__);
		goto es705_slim_boot_setup_failed;
	}

	es705->mode = SBL;

	rc = es705_slim_write_then_read(es705, &boot_cmd, sizeof(boot_cmd),
					&ack_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): BOOT_CMD fail\n", __func__);

es705_slim_boot_setup_failed:
	return rc;
}

static int es705_slim_boot_finish(struct es705_priv *es705)
{
	u32 sync_cmd;
	u32 sync_rspn;
	int match = 1;
	int rc = 0;

	dev_info(es705->dev, "%s(): finish fw download\n", __func__);

	if (es705->es705_power_state == ES705_SET_POWER_STATE_VS_OVERLAY) {
		sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_INTR_RISING_EDGE;
		dev_info(es705->dev, "%s(): FW type : VOICESENSE\n", __func__);
	} else {
		sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
		dev_info(es705->dev, "%s(): fw type : STANDARD\n", __func__);
	}
	sync_rspn = sync_cmd;

	/* Give the chip some time to become ready after firmware download. */
	msleep(20);

	/* finish es705 boot, check es705 readiness */
	rc = es705_slim_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
					&sync_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): SYNC fail\n", __func__);

	return rc;
}

static int es705_dt_parse_slim_interface_dev_info(struct device *dev,
						  struct slim_device *slim_ifd)
{
	int rc;
	struct property *prop;

	rc = of_property_read_string(dev->of_node, CODEC_INTF_PROP,
				     &slim_ifd->name);
	if (rc) {
		dev_err(dev, "%s(): %s failed", __func__,
			dev->of_node->full_name);
		return -ENODEV;
	}

	prop = of_find_property(dev->of_node, CODEC_ELEMENTAL_ADDR, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		rc = -ENODATA;
	if (prop->length < sizeof(slim_ifd->e_addr))
		rc = -EOVERFLOW;

	if (!rc) {
		memcpy(slim_ifd->e_addr, prop->value, sizeof(slim_ifd->e_addr));

		dev_info(dev,
			 "%s(): slim ifd addr 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			 __func__, slim_ifd->e_addr[0], slim_ifd->e_addr[1],
			 slim_ifd->e_addr[2], slim_ifd->e_addr[3],
			 slim_ifd->e_addr[4], slim_ifd->e_addr[5]);
	}

	return rc;
}

static int es705_slim_dts_init(struct slim_device *sbdev)
{
	static struct slim_device intf_device;
	struct esxxx_platform_data *pdata;
	int rc = 0;

	dev_info(&sbdev->dev,
		 "%s(): slim gen0 addr 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		 __func__, sbdev->e_addr[0], sbdev->e_addr[1], sbdev->e_addr[2],
		 sbdev->e_addr[3], sbdev->e_addr[4], sbdev->e_addr[5]);

	pdata = es705_populate_dt_pdata(&sbdev->dev);
	if (pdata == NULL) {
		dev_err(&sbdev->dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	sbdev->dev.platform_data = pdata;
	es705_priv.gen0_client = sbdev;

	rc = es705_dt_parse_slim_interface_dev_info(&sbdev->dev, &intf_device);
	if (rc) {
		dev_err(&sbdev->dev, "%s(): Error, parsing slim interface\n",
			__func__);
		devm_kfree(&sbdev->dev, pdata);
		rc = -EINVAL;
		goto pdata_error;
	}

	es705_priv.intf_client = &intf_device;

pdata_error:
	return rc;
}

static void es705_slim_nodts_init(struct slim_device *sbdev)
{
	if (strncmp
	    (sbdev->name, CODEC_INTF_ID,
	     strnlen(CODEC_INTF_ID, SLIMBUS_NAME_SIZE)) == 0) {
		dev_dbg(&sbdev->dev, "%s(): interface device probe\n",
			__func__);
		es705_priv.intf_client = sbdev;
	} else {
		dev_dbg(&sbdev->dev, "%s(): generic device probe\n", __func__);
		es705_priv.gen0_client = sbdev;
	}
}

#if defined(CONFIG_SLIMBUS_MSM_NGD)
int es705_slim_wakeup_bus(struct es705_priv *es705)
{
	int rc = 0;
	dev_info(es705->dev, "%s()", __func__);
	rc = slim_ctrl_clk_pause(es705->gen0_client->ctrl, 1, 0);
	return rc;
}
#endif

static int es705_slim_dev_init(struct slim_device *sbdev)
{
	int rc;

	if (sbdev->dev.of_node) {
		rc = es705_slim_dts_init(sbdev);
		if (rc)
			goto es705_core_probe_error;
	} else {
		es705_slim_nodts_init(sbdev);
	}

	if (es705_priv.intf_client == NULL || es705_priv.gen0_client == NULL) {
		dev_err(&sbdev->dev, "%s(): incomplete initialization\n",
			__func__);
		return 0;
	}

	slim_set_clientdata(sbdev, &es705_priv);

	es705_priv.intf = ES705_SLIM_INTF;
	es705_priv.dev_read = es705_slim_read;
	es705_priv.dev_write = es705_slim_write;
	es705_priv.dev_write_then_read = es705_slim_write_then_read;
	es705_priv.vs_streaming = es705_slim_vs_streaming;
	es705_priv.streamdev = uart_streamdev;
	es705_priv.boot_setup = es705_slim_boot_setup;
	es705_priv.boot_finish = es705_slim_boot_finish;
#if defined(CONFIG_SLIMBUS_MSM_NGD)
	es705_priv.wakeup_bus = es705_slim_wakeup_bus;
#endif
	es705_priv.cmd = es705_slim_cmd;
	es705_priv.dev = &es705_priv.gen0_client->dev;

	rc = es705_core_init(&es705_priv.gen0_client->dev);
	if (rc) {
		dev_err(&sbdev->dev, "%s(): es705_core_init() failed %d\n",
			__func__, rc);
		goto es705_core_probe_error;
	}

	return 0;

es705_core_probe_error:
	dev_err(&sbdev->dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es705_slim_device_up(struct slim_device *sbdev)
{
	int rc = 0;

	dev_info(&sbdev->dev, "%s: name=%s, laddr=%d\n", __func__, sbdev->name,
		 sbdev->laddr);

	slim_get_devicedata(sbdev);

	rc = es705_slim_dev_init(sbdev);
	if (rc) {
		dev_err(&sbdev->dev, "%s(): es705_slim_dev_init failed(%d)\n",
			__func__, rc);
	}

	/* Start the firmware download in the workqueue context. */
	rc = fw_download(&es705_priv);
	BUG_ON(rc != 0);

#if defined(SAMSUNG_ES705_FEATURE)
	if (es705_priv.power_control) {
		es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
					 ES705_POWER_STATE);
	}
#endif

	return rc;
}

static int es705_slim_probe(struct slim_device *sbdev)
{
	int rc = 0;

	dev_info(&sbdev->dev, "%s: sbdev=%s\n", __func__, sbdev->name);

	if (!sbdev->dev.of_node) {
		dev_err(&sbdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	rc = es705_stimulate_start(&sbdev->dev);
	if (rc) {
		dev_err(&sbdev->dev, "%s(): es705_stimulate_start failed %d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}

static int es705_slim_remove(struct slim_device *sbdev)
{
	struct esxxx_platform_data *pdata = sbdev->dev.platform_data;

	dev_dbg(&sbdev->dev, "%s(): sbdev->name = %s\n", __func__, sbdev->name);

	es705_gpio_free(pdata);

	snd_soc_unregister_codec(&sbdev->dev);

	return 0;
}

struct slim_driver es70x_slim_driver = {
	.driver = {
		   .name = CODEC_ID,
		   .owner = THIS_MODULE,
		   },
	.probe = es705_slim_probe,
	.remove = es705_slim_remove,
	.device_up = es705_slim_device_up,
	.id_table = es70x_slim_id,
};

int es705_slimbus_init(struct es705_priv *es705)
{
	int rc;

	rc = slim_driver_register(&es70x_slim_driver);
	if (!rc) {
		pr_info("%s(): slim driver for es705 is registered\n",
			__func__);
	}

	return rc;
}

MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_AUTHOR("Genisim Tsilker <gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
