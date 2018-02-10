/*
 * es705-uart.c  --  Audience eS705 UART interface
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Matt Lupfer <mlupfer@cardinalpeak.com>
 *
 * Code Updates:
 *       Genisim Tsilker <gtsilker@audience.com>
 *            - Add optional UART VS FW download
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
#include <linux/kthread.h>
#include <linux/esxxx.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "es705.h"
#include "es705-platform.h"
#include "es705-uart.h"
#include "es705-uart-common.h"
#include "es705-cdev.h"

#ifdef ES705_FW_LOAD_BUF_SZ
#undef ES705_FW_LOAD_BUF_SZ
#endif
#define ES705_FW_LOAD_BUF_SZ 1024

const u32 es705_uart_baud_rate[] = {
	0x80190002,		/* Default 460.8KBps */
	0x80190002,		/* baud rate 460.8Kbps, Clock 9.6Mhz */
	0x80190202,		/* baud rate 1Mbps, Clock 9.6Mhz */
	0x80190702,		/* baud rate 3Mbps, Clock 9.6Mhz */
};

static int es705_set_uart_baud_rate(struct es705_priv *es705)
{
	int rc;
	u32 uart_rate_request;
	u32 resp;
	const int match = 1;

	dev_dbg(es705->dev, "%s(): UART baud rate request = 0x%08x\n",
		__func__, es705_uart_baud_rate[es705->uart_fw_download_rate]);

	resp = uart_rate_request =
	    cpu_to_be32(es705_uart_baud_rate[es705->uart_fw_download_rate]);
	rc = es705_uart_write_then_read(es705, &uart_rate_request, 4, &resp,
					match);
	resp = be32_to_cpu(resp);
	if (rc < 0)
		dev_err(es705->dev,
			"%s(): UART baud rate set for FW download FAIL\n",
			__func__);
	else
		dev_dbg(es705->dev, "%s(): UART baud rate resp = 0x%08x\n",
			__func__, resp);

	return rc;
}

static int es705_uart_boot_setup(struct es705_priv *es705)
{
	u8 sbl_sync_cmd = ES705_SBL_SYNC_CMD;
	u8 sbl_boot_cmd = ES705_SBL_BOOT_CMD;
	u32 rspn = sbl_sync_cmd;
	const int match = 1;
	int rc;

	/* Detect BAUD RATE send SBL SYNC BYTE 0x00 */
	dev_dbg(es705->dev, "%s(): write ES705_SBL_SYNC_CMD = 0x%02x\n",
		__func__, sbl_sync_cmd);
	rc = es705_uart_write_then_read(es705, &sbl_sync_cmd, 1, &rspn, match);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): UART baud rate detection fail\n",
			__func__);
		goto es705_bootup_failed;
	}
	dev_dbg(es705->dev, "%s(): sbl sync ack = 0x%02x\n", __func__, rspn);

	es705->uart_fw_download_rate = 3;
	rc = es705_set_uart_baud_rate(es705);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): uart baud rate set error\n",
			__func__);
		goto es705_bootup_failed;
	}
	es705_set_tty_baud_rate(es705->uart_fw_download_rate);

	/* SBL SYNC BYTE 0x00 */
	dev_dbg(es705->dev, "%s(): write ES705_SBL_SYNC_CMD = 0x%02x\n",
		__func__, sbl_sync_cmd);
	rc = es705_uart_write_then_read(es705, &sbl_sync_cmd, 1, &rspn, match);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): UART FW Download SBL SYNC fail\n",
			__func__);
		goto es705_bootup_failed;
	}
	dev_dbg(es705->dev, "%s(): sbl sync ack = 0x%02x\n", __func__, rspn);

	/* SBL BOOT BYTE 0x01 */
	dev_dbg(es705->dev, "%s(): write ES705_SBL_BOOT_CMD = 0x%02x\n",
		__func__, sbl_boot_cmd);
	rspn = ES705_SBL_BOOT_ACK;
	rc = es705_uart_write_then_read(es705, &sbl_boot_cmd, 1, &rspn, match);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): UART FW Download BOOT CMD fail\n",
			__func__);
		goto es705_bootup_failed;
	}
	dev_dbg(es705->dev, "%s(): sbl boot ack = 0x%02x\n", __func__, rspn);

es705_bootup_failed:
	return rc;
}

static int es705_uart_boot_finish(struct es705_priv *es705)
{
	u32 sync_cmd;
	u32 sync_resp;
	char msg[4];
	int rc, retry = 3;

	/*
	 * Give the chip some time to become ready after firmware
	 * download. (FW is still transferring)
	 */
	msleep(50);

	/*
	 * Read 4 bytes of VS FW download status
	 * TODO To avoid possible problem in the future
	 * if FW goes to send less or more then 4 bytes
	 * Need modify code to read UART until read
	 * buffer is empty
	 */
	rc = es705_uart_read(es705, msg, 4);
	if (rc < 0)
		dev_err(es705->dev, "%s(): UART read fail\n", __func__);
	else
		dev_dbg(es705->dev, "%s(): read byte = 0x%02x%02x%02x%02x\n",
			__func__, msg[0], msg[1], msg[2], msg[3]);

	if (es705->es705_power_state == ES705_SET_POWER_STATE_VS_OVERLAY) {
		sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_INTR_RISING_EDGE;
		dev_info(es705->dev, "%s(): FW type : VOICESENSE\n", __func__);
	} else {
		sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
		dev_info(es705->dev, "%s(): FW type : STANDARD\n", __func__);
	}

	dev_dbg(es705->dev, "%s(): write ES705_SYNC_CMD = 0x%08x\n", __func__,
		sync_cmd);

es705_boot_retry:
	rc = es705_uart_cmd(es705, sync_cmd, 0, &sync_resp);
	if (rc < 0) {
		dev_err(es705->dev,
			"%s(): firmware load failed (no sync response)\n",
			__func__);
		goto es705_boot_finish_failed;
	}
	if (sync_cmd != sync_resp) {
		dev_err(es705->dev,
			"%s(): firmware load failed, invalid sync response 0x%08x\n",
			__func__, sync_resp);
		if (--retry) {
			dev_info(es705->dev, "%s(): buffer flush & retry\n",
				 __func__);
			tty_perform_flush(es705->uart_dev.tty, TCIFLUSH);
			goto es705_boot_retry;
		}
		rc = -EIO;
		goto es705_boot_finish_failed;
	}

	dev_dbg(es705->dev, "%s(): firmware load success 0x%08x\n", __func__,
		sync_resp);

es705_boot_finish_failed:
	return rc;
}

int es705_uart_dev_rdb(struct es705_priv *es705, void *buf, int id)
{
	u32 cmd;
	u32 resp;
	u8 *dptr;
	int ret;
	int size;
	int rdcnt = 0;

	dptr = (u8 *) buf;
	/* Read voice sense keyword data block request. */
	cmd = (ES705_RDB_CMD << 16) | (id & 0xFFFF);
	cmd = cpu_to_be32(cmd);

	ret = es705_uart_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): RDB cmd write err = %d\n", __func__,
			ret);
		goto rdb_err;
	}

	/* Refer to "ES705 Advanced API Guide" for details of interface */
	usleep_range(10000, 11000);

	ret = es705_uart_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto rdb_err;
	}

	be32_to_cpus(&resp);
	size = resp & 0xffff;

	dev_dbg(es705->dev, "%s(): resp=0x%08x size=%d\n", __func__, resp,
		size);

	if ((resp & 0xffff0000) != (ES705_RDB_CMD << 16)) {
		dev_err(es705->dev,
			"%s(): invalid read v-s data block response = 0x%08x\n",
			__func__, resp);
		goto rdb_err;
	}

	if (size == 0) {
		dev_err(es705->dev, "%s(): read request return size of 0\n",
			__func__);
		goto rdb_err;
	}
	if (size > PARSE_BUFFER_SIZE)
		size = PARSE_BUFFER_SIZE;

	for (rdcnt = 0; rdcnt < size; rdcnt++, dptr++) {
		ret = es705_uart_read(es705, dptr, 1);
		if (ret < 0) {
			dev_err(es705->dev,
				"%s(): data block ed error %d bytes ret = %d\n",
				__func__, rdcnt, ret);
			goto rdb_err;
		}
	}

	es705->rdb_read_count = size;

	ret = 0;
	goto exit;

rdb_err:
	es705->rdb_read_count = 0;
	ret = -EIO;
exit:
	return ret;
}

int es705_uart_dev_wdb(struct es705_priv *es705, const void *buf, int len)
{
	/* At this point the command has been determined and is not part
	 * of the data in the buffer. Its just data. Note that we donot
	 * evaluate the contents of the data. It is up to the higher layers
	 * to insure the the codes mode of operation matchs what is being
	 * sent.
	 */
	int ret;
	u32 resp;
	u8 *dptr;

	u32 cmd = ES705_WDB_CMD << 16;

	dev_dbg(es705->dev, "%s(): len = 0x%08x\n", __func__, len);
	dptr = (char *)buf;

	cmd = cmd | (len & 0xFFFF);
	dev_dbg(es705->dev, "%s(): cmd = 0x%08x\n", __func__, cmd);
	cmd = cpu_to_be32(cmd);
	ret = es705_uart_write_then_read(es705, &cmd, sizeof(cmd), &resp, 0);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): cmd write err=%hu\n", __func__, ret);
		goto wdb_err;
	}

	be32_to_cpus(&resp);
	dev_dbg(es705->dev, "%s(): resp = 0x%08x\n", __func__, resp);
	if ((resp & 0xffff0000) != (ES705_WDB_CMD << 16)) {
		dev_err(es705->dev, "%s(): invalid write data block 0x%08x\n",
			__func__, resp);
		goto wdb_err;
	}

	/* The API requires that the subsequent writes are to be
	 * a byte stream (one byte per transport transaction)
	 */
	ret = es705_uart_write(es705, dptr, len);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): wdb error =%d\n", __func__, ret);
		goto wdb_err;
	}

	/* One last ACK read */
	ret = es705_uart_read(es705, &resp, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): last ack %d\n", __func__, ret);
		goto wdb_err;
	}

	if (resp & 0xff000000) {
		dev_err(es705->dev, "%s(): write data block error 0x%0x\n",
			__func__, resp);
		goto wdb_err;
	}

	dev_dbg(es705->dev, "%s(): len = %d\n", __func__, len);

	goto exit;

wdb_err:
	len = -EIO;
exit:
	return len;
}

int es705_uart_es705_wakeup(struct es705_priv *es705)
{
	int rc;
	char wakeup_char = 'A';

	dev_info(es705_priv.dev, "%s(): ********* START UART wakeup\n",
		 __func__);

	rc = es705_uart_open(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): uart open error\n", __func__);
		goto es705_uart_es705_wakeup_exit;
	}

	/* eS705 wakeup. Write wakeup character to UART */
	rc = es705_uart_write(es705, &wakeup_char, sizeof(wakeup_char));
	if (rc < 0)
		dev_err(es705->dev, "%s(): wakeup via uart FAIL\n", __func__);

	es705_uart_close(es705);

es705_uart_es705_wakeup_exit:
	dev_info(es705_priv.dev, "%s(): ********* END UART wakeup\n", __func__);

	return rc;
}

#define UART_DOWNLOAD_INITIAL_SYNC_BAUD_RATE_INDEX 1
int es705_uart_fw_download(struct es705_priv *es705, int fw_type)
{
	int rc;

	dev_info(es705_priv.dev, "%s(): ********* START VS FW Download\n",
		 __func__);

	rc = es705_uart_open(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): uart open error\n", __func__);
		goto uart_open_error;
	}
	es705_set_tty_baud_rate(UART_DOWNLOAD_INITIAL_SYNC_BAUD_RATE_INDEX);

	rc = es705_uart_boot_setup(es705);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): uart boot setup error\n", __func__);
		goto uart_download_error;
	}

	if (fw_type == VOICESENSE) {
		rc = es705_uart_write(es705, (char *)es705->vs->data,
				      es705->vs->size);
	} else {
		rc = es705_uart_write(es705, (char *)es705->standard->data,
				      es705->standard->size);
	}
	if (rc < 0) {
		dev_err(es705->dev, "%s(): uart %s image write fail\n",
			__func__, fw_type == VOICESENSE ? "vs" : "standard");
		rc = -EIO;
		goto uart_download_error;
	}

	dev_dbg(es705->dev, "%s(): %s fw download done\n",
		__func__, fw_type == VOICESENSE ? "vs" : "standard");

	rc = es705_uart_boot_finish(es705);
	if (rc < 0)
		dev_err(es705->dev, "%s(): uart boot finish error\n", __func__);

uart_download_error:
	es705_uart_close(es705);
uart_open_error:

	dev_info(es705_priv.dev, "%s(): ********* END VS FW Download\n",
		 __func__);
	return rc;
}

static int es705_uart_probe_thread(void *ptr)
{
	int rc = 0;
	struct device *dev = (struct device *)ptr;

	rc = es705_uart_open(&es705_priv);
	if (rc) {
		dev_err(dev, "%s(): es705_uart_open() failed %d\n", __func__,
			rc);
		return rc;
	}

	/* set es705 function pointers */
	es705_priv.dev_read = es705_uart_read;
	es705_priv.dev_write = es705_uart_write;
	es705_priv.cmd = es705_uart_cmd;
	es705_priv.boot_setup = es705_uart_boot_setup;
	es705_priv.boot_finish = es705_uart_boot_finish;

	es705_priv.streamdev = uart_streamdev;
	es705_priv.datablockdev = uart_datablockdev;

	rc = es705_core_init(dev);
	if (rc) {
		dev_err(dev, "%s(): es705_core_init() failed %d\n", __func__,
			rc);
		goto bootup_error;
	}

	rc = es705_bootup(&es705_priv);

	if (rc) {
		dev_err(dev, "%s(): es705_bootup failed %d\n", __func__, rc);
		goto bootup_error;
	}

	/* init es705 character device here, now that the UART is discovered */
	rc = es705_init_cdev(&es705_priv);
	if (rc) {
		dev_err(dev, "%s(): failed to initialize char device = %d\n",
			__func__, rc);
		goto cdev_init_error;
	}

	return rc;

bootup_error:
	/* close filp */
	es705_uart_close(&es705_priv);
cdev_init_error:
	dev_dbg(es705_priv.dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es705_uart_probe(struct platform_device *dev)
{
	int rc = 0;
	struct task_struct *uart_probe_thread = NULL;

	uart_probe_thread = kthread_run(es705_uart_probe_thread,
					(void *)&dev->dev, "es705 uart thread");
	if (IS_ERR_OR_NULL(uart_probe_thread)) {
		dev_err(&dev->dev,
			"%s(): can't create es705 UART probe thread = %p\n",
			__func__, uart_probe_thread);
		rc = -ENOMEM;
	}

	return rc;
}

static int es705_uart_remove(struct platform_device *dev)
{
	int rc = 0;

	if (es705_priv.uart_dev.file)
		es705_uart_close(&es705_priv);

	es705_priv.uart_dev.tty = NULL;
	es705_priv.uart_dev.file = NULL;

	snd_soc_unregister_codec(es705_priv.dev);

	return rc;
}

struct platform_driver es705_uart_driver = {
	.driver = {
		   .name = "es705-codec",
		   .owner = THIS_MODULE,
		   },
	.probe = es705_uart_probe,
	.remove = es705_uart_remove,
};

static struct esxxx_platform_data esxxx_platform_data = {
	.irq_base = 0,
	.reset_gpio = -1,
	.gpiob_gpio = -1,
	.esxxx_clk_cb = NULL,
};

struct platform_device es705_uart_device = {
	.name = "es705-codec",
	.resource = NULL,
	.num_resources = 0,
	.dev = {
		.platform_data = &esxxx_platform_data,
		}
};

/* FIXME: Kludge for es705_bus_init abstraction */
int es705_uart_bus_init(struct es705_priv *es705)
{
	int rc;

	rc = platform_driver_register(&es705_uart_driver);
	if (rc)
		return rc;

	rc = platform_device_register(&es705_uart_device);
	if (rc)
		return rc;

	dev_dbg(es705->dev, "%s(): registered as UART", __func__);

	return rc;
}

MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_AUTHOR("Genisim Tsilker<gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
