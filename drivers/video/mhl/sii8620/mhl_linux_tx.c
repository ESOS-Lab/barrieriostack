/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/stringify.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#ifdef CONFIG_MUIC_NOTIFIER
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#include "si_fw_macros.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl_defs.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#include "mhl_rcp_inputdev.h"
#include "mhl_rbp_inputdev.h"
#include "mhl_linux_tx.h"
#include "mhl_supp.h"
#include "platform.h"
#include "si_mhl_callback_api.h"
#include "si_8620_drv.h"
#ifdef CONFIG_MHL3_SEC_FEATURE
#include "si_8620_regs.h"
#include "mhl_test_case.h"
#endif

#define MHL_DRIVER_MINOR_MAX 1

/* Convert a value specified in milliseconds to nanoseconds */
#define MSEC_TO_NSEC(x)	(x * 1000000UL)

static char *white_space = "' ', '\t'";
static dev_t dev_num;

static struct class *mhl_class;
#ifdef CONFIG_MHL3_SEC_FEATURE
static struct device *sii8620_dev;
#endif

static void mhl_tx_destroy_timer_support(struct mhl_dev_context *dev_context);

/* Define SysFs attribute names */
#define SYS_ATTR_NAME_CONN			connection_state
#define SYS_ATTR_NAME_DSHPD			ds_hpd
#define SYS_ATTR_NAME_HDCP2			hdcp2_status

#define SYS_ATTR_NAME_SPAD			spad
#define SYS_ATTR_NAME_I2C_REGISTERS		i2c_registers
#define SYS_ATTR_NAME_DEBUG_LEVEL		debug_level
#define SYS_ATTR_NAME_REG_DEBUG_LEVEL		debug_reg_dump
#define SYS_ATTR_NAME_AKSV			aksv
#define SYS_ATTR_NAME_EDID			edid
#define SYS_ATTR_NAME_HEV_3D_DATA		hev_3d_data
#define SYS_ATTR_NAME_GPIO_INDEX		gpio_index
#define SYS_ATTR_NAME_GPIO_VALUE		gpio_value
#define SYS_ATTR_NAME_PP_16BPP			pp_16bpp

#ifdef DEBUG
#define SYS_ATTR_NAME_TX_POWER			tx_power
#define SYS_ATTR_NAME_STARK_CTL			set_stark_ctl
#endif

/* define SysFs object names */

#define SYS_ATTR_NAME_BIST			bist
#define		SYS_ATTR_NAME_IN		in
#define		SYS_ATTR_NAME_IN_STATUS		in_status
#define		SYS_ATTR_NAME_OUT		out
#define		SYS_ATTR_NAME_OUT_STATUS	out_status
#define		SYS_ATTR_NAME_INPUT_DEV		input_dev

#define SYS_OBJECT_NAME_REG_ACCESS		reg_access
#define		SYS_ATTR_NAME_REG_ACCESS_PAGE	page
#define		SYS_ATTR_NAME_REG_ACCESS_OFFSET	offset
#define		SYS_ATTR_NAME_REG_ACCESS_LENGTH	length
#define		SYS_ATTR_NAME_REG_ACCESS_DATA	data

#define SYS_OBJECT_NAME_RAP			rap
#define		SYS_ATTR_NAME_RAP_IN		SYS_ATTR_NAME_IN
#define		SYS_ATTR_NAME_RAP_IN_STATUS	SYS_ATTR_NAME_IN_STATUS
#define		SYS_ATTR_NAME_RAP_OUT		SYS_ATTR_NAME_OUT
#define		SYS_ATTR_NAME_RAP_OUT_STATUS	SYS_ATTR_NAME_OUT_STATUS
#define		SYS_ATTR_NAME_RAP_INPUT_DEV	SYS_ATTR_NAME_INPUT_DEV

#define SYS_OBJECT_NAME_RCP			rcp
#define		SYS_ATTR_NAME_RCP_IN		SYS_ATTR_NAME_IN
#define		SYS_ATTR_NAME_RCP_IN_STATUS	SYS_ATTR_NAME_IN_STATUS
#define		SYS_ATTR_NAME_RCP_OUT		SYS_ATTR_NAME_OUT
#define		SYS_ATTR_NAME_RCP_OUT_STATUS	SYS_ATTR_NAME_OUT_STATUS
#define		SYS_ATTR_NAME_RCP_INPUT_DEV	SYS_ATTR_NAME_INPUT_DEV

#define SYS_OBJECT_NAME_RBP			rbp
#define		SYS_ATTR_NAME_RBP_IN		SYS_ATTR_NAME_IN
#define		SYS_ATTR_NAME_RBP_IN_STATUS	SYS_ATTR_NAME_IN_STATUS
#define		SYS_ATTR_NAME_RBP_INPUT_DEV	SYS_ATTR_NAME_INPUT_DEV

#define SYS_OBJECT_NAME_UCP			ucp
#define		SYS_ATTR_NAME_UCP_IN		SYS_ATTR_NAME_IN
#define		SYS_ATTR_NAME_UCP_IN_STATUS	SYS_ATTR_NAME_IN_STATUS
#define		SYS_ATTR_NAME_UCP_OUT		SYS_ATTR_NAME_OUT
#define		SYS_ATTR_NAME_UCP_OUT_STATUS	SYS_ATTR_NAME_OUT_STATUS
#define		SYS_ATTR_NAME_UCP_INPUT_DEV	SYS_ATTR_NAME_INPUT_DEV

#define SYS_OBJECT_NAME_DEVCAP				devcap
#define		SYS_ATTR_NAME_DEVCAP_LOCAL_OFFSET	local_offset
#define		SYS_ATTR_NAME_DEVCAP_LOCAL		local
#define		SYS_ATTR_NAME_DEVCAP_REMOTE_OFFSET	remote_offset
#define		SYS_ATTR_NAME_DEVCAP_REMOTE		remote

#define SYS_OBJECT_NAME_HDCP				hdcp
#define		SYS_ATTR_NAME_HDCP_CONTENT_TYPE		hdcp_content_type

#define SYS_OBJECT_NAME_VC				vc
#define		SYS_ATTR_NAME_VC_ASSIGN			vc_assign

/*
 * show_connection_state() - Handle read request to the connection_state
 *							attribute file.
 */
ssize_t show_connection_state(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED)
		return scnprintf(buf, PAGE_SIZE, "connected");
	else
		return scnprintf(buf, PAGE_SIZE, "not connected");
}

/*
 * show_ds_hpd_state() - Handle read request to the ds_hpd attribute file.
 */
ssize_t show_ds_hpd_state(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int hpd_status = 0;
	int status;

	status = si_8620_get_hpd_status(&hpd_status);
	if (status)
		return status;
	else
		return scnprintf(buf, PAGE_SIZE, "%d", hpd_status);
}

/*
 * show_ds_hdcp2_status() - Handle read request to the ds_hpd attribute file.
 */
ssize_t show_hdcp2_status(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	uint32_t hdcp2_status = 0;
	int status;

	status = si_8620_get_hdcp2_status(&hdcp2_status);
	if (status)
		return status;
	else
		return scnprintf(buf, PAGE_SIZE, "%x", hdcp2_status);
}

/*
 * Wrapper for kstrtoul() that nul-terminates the input string at
 * the first non-digit character instead of returning an error.
 *
 * This function is destructive to the input string.
 *
 */
static int si_strtoul(char **str, int base, unsigned long *val)
{
	int tok_length, status, nul_offset;
	char *tstr = *str;

	nul_offset = 1;
	status = -EINVAL;
	if ((base == 0) && (tstr[0] == '0') && (tolower(tstr[1]) == 'x')) {
		tstr += 2;
		base = 16;
	}

	tok_length = strspn(tstr, "0123456789ABCDEFabcdef");
	if (tok_length) {
		if ((tstr[tok_length] == '\n') || (tstr[tok_length] == 0))
			nul_offset = 0;

		tstr[tok_length] = 0;
		status = kstrtoul(tstr, base, val);
		if (status == 0) {
			tstr = (tstr + tok_length) + nul_offset;
			*str = tstr;
		}
	}
	return status;
}

/*
 * send_scratch_pad() - Handle write request to the spad attribute file.
 *
 * This file is used to either initiate a write to the scratch pad registers
 * of an attached device, or to set the offset and byte count for a subsequent
 * read from the local scratch pad registers.
 *
 * The format of the string in buf must be:
 *	offset=<offset_value> length=<Length_value> \
 *	data=data_byte_0 ... data_byte_length-1
 * where: <offset_value> specifies the starting register offset to begin
 *			 read/writing within the scratch pad register space
 *	 <length_value> number of scratch pad registers to be written/read
 *	 data_byte	space separated list of <length_value> data bytes to be
 *			written.  If no data bytes are present then the write
 *			to this file will only be used to set the offset and
 *			length for a subsequent read from this file.
 */
ssize_t send_scratch_pad(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long offset = 0x100;	/* initialize with invalid values */
	unsigned long length = 0x100;
	unsigned long value;
	u8 data[MAX_SCRATCH_PAD_TRANSFER_SIZE];
	unsigned int idx;
	char *str;
	int status = -EINVAL;
	struct bist_setup_burst *setup_burst;
	uint16_t burst_id;
	char *pinput = kmalloc(count, GFP_KERNEL);

	MHL_TX_DBG_ERR("received string(%d): " "%s" "\n", count, buf);

	if (pinput == 0)
		return -EINVAL;
	memcpy(pinput, buf, count);

	/*
	 * Parse the input string and extract the scratch pad register selection
	 * parameters
	 */
	str = strstr(pinput, "offset=");
	if (str != NULL) {
		str += 7;
		status = si_strtoul(&str, 0, &offset);
		if ((status != 0) || (offset > SCRATCH_PAD_SIZE)) {
			MHL_TX_DBG_ERR("Invalid offset value entered\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(
			"Invalid string format, can't find offset value\n");
		goto err_exit_2;
	}

	str = strstr(str, "length=");
	if (str != NULL) {
		str += 7;
		status = si_strtoul(&str, 0, &length);
		if ((status != 0) || (length > MAX_SCRATCH_PAD_TRANSFER_SIZE)) {
			MHL_TX_DBG_ERR("Transfer length too large\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(
			"Invalid string format, can't find length value\n");
		goto err_exit_2;
	}

	str = strstr(str, "data=");
	if (str != NULL) {
		str += 5;
		for (idx = 0; idx < length; idx++) {
			str += strspn(str, white_space);
			if (*str == 0) {
				MHL_TX_DBG_ERR(
					"Too few data values provided\n");
				goto err_exit_2;
			}

			status = si_strtoul(&str, 0, &value);
			if ((status != 0) || (value > 0xFF)) {
				MHL_TX_DBG_ERR(
					"Invalid scratch pad data detected\n");
				goto err_exit_2;
			}
			data[idx] = value;
		}

	} else {
		idx = 0;
	}

	if ((offset + length) > SCRATCH_PAD_SIZE) {
		MHL_TX_DBG_ERR("Invalid offset/length combination entered");
		goto err_exit_2;
	}

	dev_context->spad_offset = offset;
	dev_context->spad_xfer_length = length;
	if (idx == 0) {
		MHL_TX_DBG_INFO("No data specified, storing offset "
				"and length for subsequent scratch pad read\n");

		goto err_exit_2;
	}

	if (down_interruptible(&dev_context->isr_lock)) {
		status = -ERESTARTSYS;
		goto err_exit_2;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit_1;
	}

	/*
	 * Make sure there is an MHL connection and that the requested
	 * data transfer parameters don't exceed the address space of
	 * the scratch pad.  NOTE: The address space reserved for the
	 * Scratch Pad registers is 64 bytes but sources and sink devices
	 * are only required to implement the 1st 16 bytes.
	 */
	if (!(dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) ||
	    (length < ADOPTER_ID_SIZE) ||
	    (offset > (SCRATCH_PAD_SIZE - ADOPTER_ID_SIZE)) ||
	    (offset + length > SCRATCH_PAD_SIZE)) {
		status = -EINVAL;
		goto err_exit_1;
	}

	dev_context->mhl_flags |= MHL_STATE_FLAG_SPAD_SENT;
	dev_context->spad_send_status = 0;

	setup_burst = (struct bist_setup_burst *)data;
	burst_id = setup_burst->burst_id_h << 8;
	burst_id |= setup_burst->burst_id_l;
	if (offset == 0 && length == 16 && burst_id == burst_id_BIST_SETUP) {
		struct bist_setup_info setup;
		enum bist_cmd_status bcs;

		setup.e_cbus_duration = setup_burst->e_cbus_duration;
		setup.e_cbus_pattern = setup_burst->e_cbus_pattern;
		setup.e_cbus_fixed_pat = (setup_burst->e_cbus_fixed_h << 8) |
			setup_burst->e_cbus_fixed_l;
		setup.avlink_data_rate = setup_burst->avlink_data_rate;
		setup.avlink_pattern = setup_burst->avlink_pattern;
		setup.avlink_video_mode = setup_burst->avlink_video_mode;
		setup.avlink_duration = setup_burst->avlink_duration;
		setup.avlink_fixed_pat = (setup_burst->avlink_fixed_h << 8) |
			setup_burst->avlink_fixed_l;
		setup.avlink_randomizer = setup_burst->avlink_randomizer;
		setup.impedance_mode = setup_burst->impedance_mode;

		bcs = si_mhl_tx_bist_setup(dev_context, &setup);

		MHL_TX_DBG_ERR("BIST command status = %d\n", bcs);

	} else {
		enum scratch_pad_status scratch_pad_status;

		scratch_pad_status = si_mhl_tx_request_write_burst(dev_context,
			offset, length, data);
		switch (scratch_pad_status) {
		case SCRATCHPAD_SUCCESS:
			/* Return the number of bytes written to this file */
			status = count;
			break;

		case SCRATCHPAD_BUSY:
			status = -EAGAIN;
			break;

		default:
			status = -EFAULT;
			break;
		}
	}

err_exit_1:
	up(&dev_context->isr_lock);

err_exit_2:
	kfree(pinput);
	return status;
}

/*
 * show_scratch_pad() - Handle read request to the spad attribute file.
 *
 * Reads from this file return one or more scratch pad register values
 * in hexadecimal string format.  The registers returned are specified
 * by the offset and length values previously written to this file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 *
 * The format of the string returned in buf is:
 *	"offset:<offset> length:<lenvalue> data:<datavalues>
 *	where:	<offset> is the last scratch pad register offset
 *			 written to this file
 *		<lenvalue> is the last scratch pad register transfer length
 *			   written to this file
 *		<datavalue> space separated list of <lenvalue> scratch pad
 *			    register values in OxXX format
 */
ssize_t show_scratch_pad(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	u8 data[MAX_SCRATCH_PAD_TRANSFER_SIZE];
	u8 idx;
	enum scratch_pad_status scratch_pad_status;
	int status = -EINVAL;

	MHL_TX_DBG_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) {

		scratch_pad_status = si_get_scratch_pad_vector(dev_context,
			dev_context->spad_offset,
			dev_context->spad_xfer_length, data);

		switch (scratch_pad_status) {
		case SCRATCHPAD_SUCCESS:
			status = scnprintf(buf, PAGE_SIZE, "offset:0x%02x "
					   "length:0x%02x data:",
					   dev_context->spad_offset,
					   dev_context->spad_xfer_length);

			for (idx = 0; idx < dev_context->spad_xfer_length;
			     idx++) {
				status +=
				    scnprintf(&buf[status], PAGE_SIZE,
					      "0x%02x ", data[idx]);
			}
			break;

		case SCRATCHPAD_BUSY:
			status = -EAGAIN;
			break;

		default:
			status = -EFAULT;
			break;
		}
	}

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_reg_access_page() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_page(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long address = 0x100;
	int status = -EINVAL;
	char my_buf[20];
	unsigned int i;

	MHL_TX_COMM_INFO("received string: %c%s%c\n", '"', buf, '"');
	if (count >= sizeof(my_buf)) {
		MHL_TX_DBG_ERR("string too long %c%s%c\n", '"', buf, '"');
		return status;
	}
	for (i = 0; i < count; ++i) {
		if ('\n' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		if ('\t' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		if (' ' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		my_buf[i] = buf[i];
	}

	status = kstrtoul(my_buf, 0, &address);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, my_buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, my_buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, my_buf, ANSI_ESC_RESET_TEXT);
	} else if (address > 0xFF) {
		MHL_TX_DBG_ERR("address:0x%x buf:%s%s%s\n", address,
			ANSI_ESC_RED_TEXT, my_buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->debug_i2c_address = address;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

/*
 * show_reg_access_page()	- Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_page(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_COMM_INFO("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x",
		dev_context->debug_i2c_address);

	return status;
}

/*
 * set_reg_access_offset() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_offset(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long offset = 0x100;
	int status = -EINVAL;

	MHL_TX_COMM_INFO("received string: " "%s" "\n", buf);

	status = kstrtoul(buf, 0, &offset);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%c%s%c\n",
				status, '"', buf, '"');
	} else if (offset > 0xFF) {
		MHL_TX_DBG_ERR("offset:0x%x buf:%c%s%c\n",
				offset, '"', buf, '"');
	} else {

		if (down_interruptible(&dev_context->isr_lock))
			return -ERESTARTSYS;
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			status = -ENODEV;
		} else {
			dev_context->debug_i2c_offset = offset;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

/*
 * show_reg_access_offset()	- Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_COMM_INFO("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x",
		dev_context->debug_i2c_offset);

	return status;
}

/*
 * set_reg_access_length() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_length(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long length = 0x100;
	int status = -EINVAL;

	MHL_TX_COMM_INFO("received string: " "%s" "\n", buf);

	status = kstrtoul(buf, 0, &length);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%c%s%c\n",
				status, '"', buf, '"');
	} else if (length > 0xFF) {
		MHL_TX_DBG_ERR("length:0x%x buf:%c%s%c\n",
				length, '"', buf, '"');
	} else {

		if (down_interruptible(&dev_context->isr_lock))
			return -ERESTARTSYS;
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			status = -ENODEV;
		} else {
			dev_context->debug_i2c_xfer_length = length;
			status = count;
		}
		up(&dev_context->isr_lock);
	}


	return status;
}

/*
 * show_reg_access_length()	- Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_length(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_COMM_INFO("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x",
		dev_context->debug_i2c_xfer_length);

	return status;
}

/*
 * set_reg_access_data() - Handle write request to the
 *	reg_access_data attribute file.
 *
 * This file is used to either perform a write to registers of the transmitter
 * or to set the address, offset and byte count for a subsequent from the
 * register(s) of the transmitter.
 *
 * The format of the string in buf must be:
 *	data_byte_0 ... data_byte_length-1
 * Where: data_byte is a space separated list of <length_value> data bytes
 *			to be written.  If no data bytes are present then
 *			the write to this file will only be used to set
 *			the  page address, offset and length for a
 *			subsequent read from this file.
 */
ssize_t set_reg_access_data(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long value;
	u8 data[MAX_DEBUG_TRANSFER_SIZE];
	int i;
	char *str;
	int status = -EINVAL;
	char *pinput = kmalloc(count, GFP_KERNEL);

	MHL_TX_COMM_INFO("received string: %c%s%c\n", '"', buf, '"');

	if (pinput == 0)
		return -EINVAL;
	memcpy(pinput, buf, count);

	/*
	 * Parse the input string and extract the scratch pad register
	 * selection parameters
	 */
	str = pinput;
	for (i = 0; (i < MAX_DEBUG_TRANSFER_SIZE) && ('\0' != *str); i++) {

		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (-EINVAL == status) {
			MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (status != 0) {
			MHL_TX_DBG_ERR("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (value > 0xFF) {
			MHL_TX_DBG_ERR("value:0x%x str:%s%s%s\n", value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else {
			data[i] = value;
		}
	}

	if (i == 0) {
		MHL_TX_COMM_INFO("No data specified\n");
		goto exit_reg_access_data;
	}

	if (down_interruptible(&dev_context->isr_lock)) {
		status = -ERESTARTSYS;
		goto exit_reg_access_data;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {

		status = dev_context->drv_info->mhl_device_dbg_i2c_reg_xfer(
			&dev_context->drv_context,
			 dev_context->debug_i2c_address,
			 dev_context->debug_i2c_offset,
			 i, DEBUG_I2C_WRITE, data);
		if (status == 0)
			status = count;
	}

	up(&dev_context->isr_lock);

exit_reg_access_data:
	kfree(pinput);
	return status;
}

/*
 * show_reg_access_data()	- Handle read request to the
				reg_access_data attribute file.
 *
 * Reads from this file return one or more transmitter register values in
 * hexadecimal string format.  The registers returned are specified by the
 * address, offset and length values previously written to this file.
 *
 * The return value is the number characters written to buf, or an error
 * code if the I2C read fails.
 *
 * The format of the string returned in buf is:
 *	"address:<pageaddr> offset:<offset> length:<lenvalue> data:<datavalues>
 *	where:	<pageaddr>  is the last I2C register page address written
 *				to this file
 *		<offset>    is the last register offset written to this file
 *		<lenvalue>  is the last register transfer length written
 *				to this file
 *		<datavalue> space separated list of <lenvalue> register
 *				values in OxXX format
 */
ssize_t show_reg_access_data(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	u8 data[MAX_DEBUG_TRANSFER_SIZE] = {0};
	u8 idx;
	int status = -EINVAL;

	MHL_TX_COMM_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto no_dev;
	}

	status = dev_context->drv_info->mhl_device_dbg_i2c_reg_xfer(
		&dev_context->drv_context,
		dev_context->debug_i2c_address,
		dev_context->debug_i2c_offset,
		dev_context->debug_i2c_xfer_length,
		DEBUG_I2C_READ,
		data);
no_dev:
	up(&dev_context->isr_lock);

	if (status == 0) {
		status = scnprintf(buf, PAGE_SIZE,
			"0x%02x'0x%02x:",
			dev_context->debug_i2c_address,
			dev_context->debug_i2c_offset
			);

		for (idx = 0; idx < dev_context->debug_i2c_xfer_length;
			idx++) {
			status += scnprintf(&buf[status], PAGE_SIZE, " 0x%02x",
				data[idx]);
		}
	}

	return status;
}

static struct device_attribute reg_access_page_attr =
	__ATTR(SYS_ATTR_NAME_REG_ACCESS_PAGE, 0664,
		show_reg_access_page, set_reg_access_page);

static struct device_attribute reg_access_offset_attr =
	__ATTR(SYS_ATTR_NAME_REG_ACCESS_OFFSET, 0664,
		show_reg_access_offset, set_reg_access_offset);

static struct device_attribute reg_access_length_attr =
	__ATTR(SYS_ATTR_NAME_REG_ACCESS_LENGTH, 0664,
		show_reg_access_length, set_reg_access_length);

static struct device_attribute reg_access_data_attr =
	__ATTR(SYS_ATTR_NAME_REG_ACCESS_DATA, 0664,
		show_reg_access_data, set_reg_access_data);

static struct attribute *reg_access_attrs[] = {
	&reg_access_page_attr.attr,
	&reg_access_offset_attr.attr,
	&reg_access_length_attr.attr,
	&reg_access_data_attr.attr,
	NULL
};

static struct attribute_group reg_access_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_REG_ACCESS),
	.attrs = reg_access_attrs
};
/*
 * show_debug_level() - Handle read request to the debug_level attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_debug_level(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}

	status = scnprintf(buf, PAGE_SIZE, "level=%d %s",
			   debug_level,
			   debug_reg_dump ? "(includes reg dump)" : "");
	MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');

err_exit:
	up(&dev_context->isr_lock);
/* this should be a separate sysfs!!!
	si_dump_important_regs((struct drv_hw_context *)&dev_context->
			       drv_context);
*/

	return status;
}

/*
 * set_debug_level() - Handle write request to the debug_level attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_debug_level(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long new_debug_level = 0x100;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (buf != NULL) {
		/* BUGZILLA 27012 no prefix here, only on module parameter. */
		status = kstrtoul(buf, 0, &new_debug_level);
		if ((status != 0) || (new_debug_level > 0xFF)) {
			MHL_TX_DBG_ERR("Invalid debug_level: 0x%02lX\n",
				new_debug_level);
			goto err_exit;
		}
	} else {
		MHL_TX_DBG_ERR("Missing debug_level parameter\n");
		status = -EINVAL;
		goto err_exit;
	}

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	debug_level = new_debug_level;
	status = count;

	MHL_TX_DBG_ERR("new debug_level=0x%02X\n", debug_level);

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_debug_reg_dump()
 *
 * Handle read request to the debug_reg_dump attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_debug_reg_dump(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}

	status = scnprintf(buf, PAGE_SIZE, "%s",
			   debug_reg_dump ? "(includes reg dump)" : "");
	MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_debug_reg_dump()
 *
 * Handle write request to the debug_reg_dump attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_debug_reg_dump(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long new_debug_reg_dump = 0x100;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &new_debug_reg_dump);
	if (status != 0)
		goto err_exit2;

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	debug_reg_dump = (new_debug_reg_dump > 0) ? true : false;
	MHL_TX_DBG_ERR("debug dump %s\n", debug_reg_dump ? "ON" : "OFF");

	status = count;
err_exit:
	up(&dev_context->isr_lock);
err_exit2:
	return status;
}

/*
 * show_gpio_index() - Handle read request to the gpio_index attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_gpio_index(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}

	status = scnprintf(buf, PAGE_SIZE, "%d", gpio_index);
	MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_gpio_index() - Handle write request to the gpio_index attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_gpio_index(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long new_gpio_index;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	if (kstrtoul(buf, 0, &new_gpio_index) != 0)
		goto err_exit;

	gpio_index = new_gpio_index;
	MHL_TX_DBG_INFO("gpio: %d\n", gpio_index);

	status = count;
err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_gpio_level() - Handle read request to the gpio_level attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_gpio_level(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	int gpio_level;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}
	gpio_level = gpio_get_value(gpio_index);
	status = scnprintf(buf, PAGE_SIZE, "%d", gpio_level);
	MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_gpio_level() - Handle write request to the gpio_level attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_gpio_level(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long gpio_level;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	if (kstrtoul(buf, 0, &gpio_level) != 0)
		goto err_exit;

	MHL_TX_DBG_INFO("gpio: %d<-%d\n", gpio_index, gpio_level);
	gpio_set_value(gpio_index, gpio_level);
	status = count;

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_pp_16bpp() - Handle read request to the gpio_level attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t show_pp_16bpp(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	int pp_16bpp;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		pp_16bpp = si_mhl_tx_drv_get_pp_16bpp_override(dev_context);
		status = scnprintf(buf, PAGE_SIZE, "%d", pp_16bpp);
		MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_pp_16bpp() - Handle write request to the gpio_level attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_pp_16bpp(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	unsigned long pp_16bpp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &pp_16bpp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n"
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			si_mhl_tx_drv_set_pp_16bpp_override(dev_context,
				pp_16bpp);
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_aksv()	- Handle read request to the aksv attribute file.
 *
 * Reads from this file return the 5 bytes of the transmitter's
 * Key Selection Vector (KSV). The registers are returned as a string
 * of space separated list of hexadecimal values formated as 0xXX.
 *
 * The return value is the number characters written to buf.
 */
ssize_t show_aksv(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	u8 data[5] = {0};
	u8 idx;
	int status = -EINVAL;

	MHL_TX_DBG_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto no_dev;
	}

	status = dev_context->drv_info->mhl_device_get_aksv(
		(struct drv_hw_context *)&dev_context->drv_context, data);
no_dev:
	up(&dev_context->isr_lock);

	if (status == 0) {

		for (idx = 0; idx < 5; idx++) {
			status += scnprintf(&buf[status], PAGE_SIZE, "0x%02x ",
					    data[idx]);
		}
	}

	return status;
}

/*
 * show_edid()	- Handle read request to the aksv attribute file.
 *
 * Reads from this file return the edid, as processed by this driver and
 *	presented upon the upstream DDC interface.
 *
 * The return value is the number characters written to buf.
 */
ssize_t show_edid(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	u8 edid_buffer[256] = {0};
	int status = -EINVAL;

	MHL_TX_DBG_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		status = si_mhl_tx_drv_sample_edid_buffer(
			(struct drv_hw_context *)&dev_context->drv_context,
			edid_buffer);
	}

	up(&dev_context->isr_lock);

	if (status == 0) {
		int idx, i;
		for (idx = 0, i = 0; i < 16; i++) {
			u8 j;
			for (j = 0; j < 16; ++j, ++idx) {
				status += scnprintf(&buf[status],
					PAGE_SIZE, "0x%02x ",
					edid_buffer[idx]);
			}
			status += scnprintf(&buf[status], PAGE_SIZE, "\n");
		}
	}

	return status;
}

/*
 * show_hev_3d()
 *
 * Reads from this file return the HEV_VIC, HEV_DTD,3D_VIC, and
 *	3D_DTD WRITE_BURST information presented in a format showing the
 *	respective associations.
 *
 * The return value is the number characters written to buf.
 */
ssize_t show_hev_3d(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_DBG_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		struct edid_3d_data_t *p_edid_data =
			dev_context->edid_parser_context;
		int i;
		status = 0;
		status += scnprintf(&buf[status], PAGE_SIZE, "HEV_DTD list:\n");
		for (i = 0; i < (int)p_edid_data->hev_dtd_info.num_items; ++i) {

			status += scnprintf(&buf[status], PAGE_SIZE,
				"%s %s %s 0x%02x 0x%04x 0x%04x 0x%04x 0x%04x "
				"0x%04x 0x%02x -- 0x%04x 0x%02x 0x%02x 0x%02x "
				"0x%02x 0x%02x\n",
				p_edid_data->hev_dtd_list[i]._3d_info.vdi_l.
					top_bottom ? "TB" : "--",
				p_edid_data->hev_dtd_list[i]._3d_info.vdi_l.
					left_right ? "LR" : "--",
				p_edid_data->hev_dtd_list[i]._3d_info.vdi_l.
					frame_sequential ? "FS" : "--",
				p_edid_data->hev_dtd_list[i].sequence_index,
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.pixel_clock_in_MHz),
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.h_active_in_pixels),
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.h_blank_in_pixels),
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.h_front_porch_in_pixels),
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.h_sync_width_in_pixels),
				p_edid_data->hev_dtd_list[i].a.h_flags,
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					b.v_total_in_lines),
				p_edid_data->hev_dtd_list[i].b.
					v_blank_in_lines,
				p_edid_data->hev_dtd_list[i].b.
					v_front_porch_in_lines,
				p_edid_data->hev_dtd_list[i].b.
					v_sync_width_in_lines,
				p_edid_data->hev_dtd_list[i].b.
					v_refresh_rate_in_fields_per_second,
				p_edid_data->hev_dtd_list[i].b.v_flags);
		}

		status += scnprintf(&buf[status], PAGE_SIZE, "HEV_VIC list:\n");
		for (i = 0; i < (int)p_edid_data->hev_vic_info.num_items; ++i) {
			status += scnprintf(&buf[status], PAGE_SIZE,
			"%s %s %s 0x%02x 0x%02x\n",
			p_edid_data->hev_vic_list[i]._3d_info.vdi_l.
				top_bottom ? "TB" : "--",
			p_edid_data->hev_vic_list[i]._3d_info.vdi_l.
				left_right ? "LR" : "--",
			p_edid_data->hev_vic_list[i]._3d_info.vdi_l.
				frame_sequential ? "FS" : "--",
			p_edid_data->hev_vic_list[i].mhl3_hev_vic_descriptor.
				vic_cea861f,
			p_edid_data->hev_vic_list[i].mhl3_hev_vic_descriptor.
				reserved);
		}

		status += scnprintf(&buf[status], PAGE_SIZE, "3D_DTD list:\n");
		for (i = 0; i < (int)p_edid_data->_3d_dtd_info.num_items; ++i) {
			status += scnprintf(&buf[status], PAGE_SIZE,
				/* pixel clk */
				"%s %s %s " "0x%02x 0x%02x "
				/* horizontal active and blanking */
				"0x%02x 0x%02x {0x%1x 0x%1x} "
				/* vertical active and blanking */
				"0x%02x 0x%02x {0x%1x 0x%1x} "
				/* sync pulse width and offset */
				"0x%02x 0x%02x {0x%1x 0x%1x} "
				"{0x%1x 0x%1x 0x%1x 0x%1x} "
				/* Image sizes */
				"0x%02x 0x%02x {0x%1x 0x%1x} "
				/* borders and flags */
				"0x%1x 0x%1x {0x%1x 0x%1x 0x%1x 0x%1x %s}\n",
				p_edid_data->_3d_dtd_list[i]._3d_info.vdi_l.
					top_bottom ? "TB" : "--",
				p_edid_data->_3d_dtd_list[i]._3d_info.vdi_l.
					left_right ? "LR" : "--",
				p_edid_data->_3d_dtd_list[i]._3d_info.vdi_l.
					frame_sequential ? "FS" : "--",
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					pixel_clock_low,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					pixel_clock_high,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_active_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_blanking_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_active_blanking_high.
					horz_blanking_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_active_blanking_high.
					horz_active_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_active_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_blanking_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_active_blanking_high.
					vert_blanking_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_active_blanking_high.
					vert_active_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_sync_offset_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_sync_pulse_width7_0,
				p_edid_data->_3d_dtd_list[i].
					dtd_cea_861.vert_sync_offset_width.
					vert_sync_pulse_width_3_0,
				p_edid_data->_3d_dtd_list[i].
					dtd_cea_861.vert_sync_offset_width.
					vert_sync_offset_3_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					hs_vs_offset_pulse_width.
					vert_sync_pulse_width_5_4,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					hs_vs_offset_pulse_width.
					vert_sync_offset_5_4,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					hs_vs_offset_pulse_width.
					horz_sync_pulse_width_9_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					hs_vs_offset_pulse_width.
					horz_sync_offset_9_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_image_size_in_mm_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_image_size_in_mm_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					image_size_high.
					vert_image_size_in_mm_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					image_size_high.
					horz_image_size_in_mm_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_border_in_lines,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_border_in_pixels,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					stereo_bit_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					sync_signal_options,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					sync_signal_type,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					stereo_bits_2_1,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					interlaced ? "interlaced" :
					"progressive");
		}

		status += scnprintf(&buf[status], PAGE_SIZE, "3d_VIC list:\n");
		for (i = 0; i < (int)p_edid_data->_3d_vic_info.num_items; ++i) {
			status +=
			    scnprintf(&buf[status], PAGE_SIZE,
				      "%s %s %s %s 0x%02x\n",
				      p_edid_data->_3d_vic_list[i]._3d_info.
				      vdi_l.top_bottom ? "TB" : "--",
				      p_edid_data->_3d_vic_list[i]._3d_info.
				      vdi_l.left_right ? "LR" : "--",
				      p_edid_data->_3d_vic_list[i]._3d_info.
				      vdi_l.frame_sequential ? "FS" : "--",
				      p_edid_data->_3d_vic_list[i].svd.
				      native ? "N" : " ",
				      p_edid_data->_3d_vic_list[i].svd.VIC);
		}
	}

	up(&dev_context->isr_lock);
	return status;
}

#ifdef DEBUG
/*
 * set_tx_power() - Handle write request to the tx_power attribute file.
 *
 * Write the string "on" or "off" to this file to power on or off the
 * MHL transmitter.
 *
 * The return value is the number of characters in buf if successful or an
 * error code if not successful.
 */
ssize_t set_tx_power(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = 0;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else if (strnicmp("on", buf, count - 1) == 0) {
		status = si_8620_power_control(true);
	} else if (strnicmp("off", buf, count - 1) == 0) {
		status = si_8620_power_control(false);
	} else {
		MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}

	if (status != 0)
		return status;
	else
		return count;
}

/*
 * set_stark_ctl() - Handle write request to the tx_power attribute file.
 *
 * Write the string "on" or "off" to this file to power on or off the
 * MHL transmitter.
 *
 * The return value is the number of characters in buf if successful or an
 * error code if not successful.
 */
ssize_t set_stark_ctl(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = 0;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		if (strnicmp("on", buf, count - 1) == 0) {
			/*
			 * Set MHL/USB switch to USB
			 * NOTE: Switch control is implemented differently on
			 * each version of the starter kit.
			 */
			set_pin(X02_USB_SW_CTRL, 1);
		} else if (strnicmp("off", buf, count - 1) == 0) {
			/*
			 * Set MHL/USB switch to USB
			 * NOTE: Switch control is implemented differently on
			 * each version of the starter kit.
			 */
			set_pin(X02_USB_SW_CTRL, 0);
		} else {
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
		}
	}

	up(&dev_context->isr_lock);

	if (status != 0)
		return status;
	else
		return count;
}
#endif

/*
 * set_bist() - Handle write request to the bist attribute file.
 *
 * The format of the string in buf must be:
 *  cmd={bist_cmd} data={xx}
 *   where:
 *	bist_cmd = BIST_TRIGGER or BIST_STOP or BIST_REQUEST_STAT
 *	xx = the value of the second message byte needed by the BIST_TRIGGER
 *		and BIST_REQUEST_STAT commands
 *
 * The return value is the number of characters in buf if successful or an
 * error code if not successful.
 */
ssize_t set_bist(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	const char *str;
	int status = 0;
	unsigned long dat = 0;

	MHL_TX_DBG_INFO("bist -- received string: %s\n", buf);

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto exit;
	}

	str = strstr(buf, "data=");
	if (str != NULL) {
		status = kstrtoul(str + 5, 0, &dat);
		if (status == 0)
			MHL_TX_DBG_INFO("bist data: %d\n", dat);
	}
	if ((str == NULL) || (status != 0)) {
		MHL_TX_DBG_ERR(
			"Invalid string format, can't find data value\n");
		status = -EINVAL;
		goto exit;
	}

	str = strstr(buf, "cmd=");
	if (str != NULL) {
		if (strnicmp("trigger", str + 4, strlen("trigger")) == 0) {
			status = si_mhl_tx_bist_trigger(dev_context, (u8)dat);
			MHL_TX_DBG_ERR("trigger status: %d\n", status);
		} else if (strnicmp("stop", str + 4, strlen("stop")) == 0) {
			status = si_mhl_tx_bist_stop(dev_context);
			MHL_TX_DBG_ERR("stop status: %d\n", status);
		} else if (strnicmp
			("request_stat", str + 4,
			 strlen("request_stat")) == 0) {
			status = si_mhl_tx_bist_request_stat(dev_context,
				(u8)dat);
			MHL_TX_DBG_ERR("request stat status: %d\n", status);
		} else {
			MHL_TX_DBG_ERR("Unrecognized BIST cmd string\n");
		}

		if (status == BIST_STATUS_NO_ERROR)
			status = count;
		else
			status = -EINVAL;
	} else {
		MHL_TX_DBG_ERR(
			"Invalid string format, can't find cmd parameter\n");
		status = -EINVAL;
	}

exit:
	MHL_TX_DBG_INFO("status = %d\n", status);
	return status;
}

#define MAX_EVENT_STRING_LEN 128
/*
 * Handler for event notifications from the MhlTx layer.
 *
 */
void mhl_event_notify(struct mhl_dev_context *dev_context, u32 event,
	u32 event_param, void *data)
{
	char event_string[MAX_EVENT_STRING_LEN];
	char *envp[] = { event_string, NULL };
	char *buf;
	u32 length;
	u32 count;
	int idx;

	MHL_TX_DBG_INFO("called, event: 0x%08x event_param: 0x%08x\n",
		event, event_param);

	switch (event) {

	case MHL_TX_EVENT_CONNECTION:
		dev_context->mhl_flags |= MHL_STATE_FLAG_CONNECTED;
#ifndef CONFIG_MHL3_SEC_FEATURE
		init_rcp_input_dev(dev_context);
#endif
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_CONN));

		strncpy(event_string, "MHLEVENT=connected",
			MAX_EVENT_STRING_LEN);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
			envp);
		break;

	case MHL_TX_EVENT_DISCONNECTION:
		dev_context->mhl_flags = 0;
		dev_context->rbp_in_button_code = 0;
		dev_context->rbp_err_code = 0;
		dev_context->rbp_send_status = 0;
#ifndef CONFIG_MHL3_SEC_FEATURE
		dev_context->rcp_in_key_code = 0;
		dev_context->rcp_out_key_code = 0;
		dev_context->rcp_err_code = 0;
		dev_context->rcp_send_status = 0;
#endif
		dev_context->ucp_in_key_code = 0;
		dev_context->ucp_out_key_code = 0;
		dev_context->ucp_err_code = 0;
		dev_context->spad_send_status = 0;

#ifdef MEDIA_DATA_TUNNEL_SUPPORT
		mdt_destroy(dev_context);
#endif
#ifndef CONFIG_MHL3_SEC_FEATURE
		destroy_rcp_input_dev(dev_context);
#endif
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_CONN));

		strncpy(event_string, "MHLEVENT=disconnected",
			MAX_EVENT_STRING_LEN);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
			envp);
		break;

	case MHL_TX_EVENT_RCP_RECEIVED:
		if (input_dev_rcp) {
			int result;

			result = generate_rcp_input_event(dev_context,
				(uint8_t) event_param);
			if (0 == result)
				si_mhl_tx_rcpk_send(dev_context,
					(uint8_t) event_param);
			else
				si_mhl_tx_rcpe_send(dev_context,
					MHL_RCPE_STATUS_INEFFECTIVE_KEY_CODE);
		} else {
			dev_context->mhl_flags &= ~MHL_STATE_FLAG_RCP_SENT;
			dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_RECEIVED;
			dev_context->rcp_in_key_code = event_param;

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_RCP));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_RCP key code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		}
		break;

	case MHL_TX_EVENT_RCPK_RECEIVED:
		if ((dev_context->mhl_flags & MHL_STATE_FLAG_RCP_SENT)
		    && (dev_context->rcp_out_key_code == event_param)) {

			dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_ACK;

			MHL_TX_DBG_INFO(
				"Generating RCPK rcvd event, keycode: 0x%02x\n",
				event_param);

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_RCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_RCPK key code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR(
				"Unexpected RCPK rcvd event, keycode: 0x%02x\n",
				event_param);
		}
		break;

	case MHL_TX_EVENT_RCPE_RECEIVED:
		if (input_dev_rcp) {
			/* do nothing */
		} else {
			if (dev_context->mhl_flags & MHL_STATE_FLAG_RCP_SENT) {

				dev_context->rcp_err_code = event_param;
				dev_context->mhl_flags |=
					MHL_STATE_FLAG_RCP_NAK;

				MHL_TX_DBG_INFO(
					"Generating RCPE received event, "
					"error code: 0x%02x\n", event_param);

				sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
					__stringify(SYS_ATTR_NAME_RCPK));

				snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=rcvd_RCPE error code=0x%02x",
					event_param);
				kobject_uevent_env(&dev_context->mhl_dev->kobj,
					KOBJ_CHANGE, envp);
			} else {
				MHL_TX_DBG_ERR(
					"Ignoring unexpected RCPE received "
					"event, error code: 0x%02x\n",
					event_param);
			}
		}
		break;

	case MHL_TX_EVENT_UCP_RECEIVED:
		if (input_dev_ucp) {
			if (event_param <= 0xFF)
				si_mhl_tx_ucpk_send(dev_context,
					(uint8_t) event_param);
			else
				si_mhl_tx_ucpe_send(dev_context,
					MHL_UCPE_STATUS_INEFFECTIVE_KEY_CODE);
		} else {
			dev_context->mhl_flags &= ~MHL_STATE_FLAG_UCP_SENT;
			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_RECEIVED;
			dev_context->ucp_in_key_code = event_param;
			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_UCP));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_UCP key code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		}
		break;

	case MHL_TX_EVENT_UCPK_RECEIVED:
		if ((dev_context->mhl_flags & MHL_STATE_FLAG_UCP_SENT) &&
			(dev_context->ucp_out_key_code == event_param)) {

			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_ACK;

			MHL_TX_DBG_INFO("Generating UCPK received event, "
				"keycode: 0x%02x\n", event_param);

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_UCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				 "MHLEVENT=received_UCPK key code=0x%02x",
				 event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR("Ignoring unexpected UCPK received "
				"event, keycode: 0x%02x\n", event_param);
		}
		break;

	case MHL_TX_EVENT_UCPE_RECEIVED:
		if (dev_context->mhl_flags & MHL_STATE_FLAG_UCP_SENT) {

			dev_context->ucp_err_code = event_param;
			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_NAK;

			MHL_TX_DBG_INFO("Generating UCPE received event, "
				"error code: 0x%02x\n", event_param);

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_UCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_UCPE error code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR("Ignoring unexpected UCPE received "
				"event, error code: 0x%02x\n",
				event_param);
		}
		break;

	case MHL_TX_EVENT_RBP_RECEIVED:
		if (input_dev_rbp) {
			if (((event_param >= 0x01) && (event_param <= 0x07)) ||
			    ((event_param >= 0x20) && (event_param <= 0x21)) ||
			    ((event_param >= 0x30) && (event_param <= 0x35)))
				si_mhl_tx_rbpk_send(dev_context,
					(uint8_t) event_param);
			else
				si_mhl_tx_rbpe_send(dev_context,
					MHL_RBPE_STATUS_INEFFECTIVE_BUTTON_CODE
					);
		} else {
			dev_context->mhl_flags |= MHL_STATE_FLAG_RBP_RECEIVED;
			dev_context->rbp_in_button_code = event_param;

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_RBP));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_RBP button code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		}
		break;

	case MHL_TX_EVENT_SPAD_RECEIVED:
		length = event_param;
		buf = data;

		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_SPAD));

		idx = snprintf(event_string, MAX_EVENT_STRING_LEN,
			"MHLEVENT=SPAD_CHG length=0x%02x data=", length);

		count = 0;
		while (idx < MAX_EVENT_STRING_LEN) {
			if (count >= length)
				break;

			idx += snprintf(&event_string[idx],
				MAX_EVENT_STRING_LEN - idx, "0x%02x ",
				buf[count]);
			count++;
		}

		if (idx < MAX_EVENT_STRING_LEN) {
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR(
				"Buffer too small for scratch pad data!\n");
		}
		break;

	case MHL_TX_EVENT_POW_BIT_CHG:
		MHL_TX_DBG_INFO("Generating VBUS power bit change "
				"event, POW bit is %s\n",
				event_param ? "ON" : "OFF");
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=MHL VBUS power %s",
			 event_param ? "ON" : "OFF");
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);
		break;

	case MHL_TX_EVENT_RAP_RECEIVED:
		MHL_TX_DBG_INFO("Generating RAP received event, "
				"action code: 0x%02x\n", event_param);

		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
			     __stringify(SYS_ATTR_NAME_RAP_IN));

		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=received_RAP action code=0x%02x",
			 event_param);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);
		break;

	case MHL_TX_EVENT_BIST_READY_RECEIVED:
		MHL_TX_DBG_INFO("Received BIST_READY 0x%02X\n", event_param);
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=received_BIST_READY code=0x%02x",
			 event_param);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);
		break;

	case MHL_TX_EVENT_BIST_TEST_DONE:
		MHL_TX_DBG_INFO("Triggered BIST test completed.\n");
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=BIST_complete");
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);
		break;

	case MHL_TX_EVENT_BIST_STATUS_RECEIVED:
		{
			struct bist_stat_info *stat =
			    (struct bist_stat_info *)data;
			MHL_TX_DBG_INFO("Received BIST_RETURN_STAT",
					event_param);
			if (stat != NULL) {
				MHL_TX_DBG_INFO
				    ("eCBUS: 0x%04X, AV_LINK: 0x%04X\n",
				     stat->e_cbus_stat, stat->avlink_stat);
				snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=received_BIST_STATUS "\
					"e_cbus=0x%04x av_link=0x%04x",
					stat->e_cbus_stat, stat->avlink_stat);
				kobject_uevent_env(&dev_context->
					mhl_dev->kobj, KOBJ_CHANGE, envp);
			} else {
				MHL_TX_DBG_INFO("\n");
			}
		}
		break;
	case MHL_TX_EVENT_T_RAP_MAX_EXPIRED:
		MHL_TX_DBG_INFO("T_RAP_MAX expired\n");
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=T_RAP_MAX_EXPIRED");
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);

		break;
	default:
		MHL_TX_DBG_ERR("called with unrecognized event code!\n");
		break;
	}
}

/*
 *  File operations supported by the MHL driver
 */
static const struct file_operations mhl_fops = {
	.owner = THIS_MODULE
};

/*
 * Sysfs attribute files supported by this driver.
 */
struct device_attribute driver_attribs[] = {
	__ATTR(SYS_ATTR_NAME_CONN, 0444, show_connection_state, NULL),
	__ATTR(SYS_ATTR_NAME_DSHPD, 0444, show_ds_hpd_state, NULL),
	__ATTR(SYS_ATTR_NAME_HDCP2, 0444, show_hdcp2_status, NULL),
	__ATTR(SYS_ATTR_NAME_SPAD, 0644, show_scratch_pad, send_scratch_pad),
	__ATTR(SYS_ATTR_NAME_DEBUG_LEVEL, 0644, get_debug_level,
	       set_debug_level),
	__ATTR(SYS_ATTR_NAME_REG_DEBUG_LEVEL, 0644, get_debug_reg_dump,
	       set_debug_reg_dump),
	__ATTR(SYS_ATTR_NAME_GPIO_INDEX, 0644, get_gpio_index, set_gpio_index),
	__ATTR(SYS_ATTR_NAME_GPIO_VALUE, 0644, get_gpio_level, set_gpio_level),
	__ATTR(SYS_ATTR_NAME_PP_16BPP  , 0644, show_pp_16bpp, set_pp_16bpp),
	__ATTR(SYS_ATTR_NAME_AKSV, 0444, show_aksv, NULL),
	__ATTR(SYS_ATTR_NAME_EDID, 0444, show_edid, NULL),
	__ATTR(SYS_ATTR_NAME_HEV_3D_DATA, 0444, show_hev_3d, NULL),
#ifdef DEBUG
	__ATTR(SYS_ATTR_NAME_TX_POWER, 0200, NULL, set_tx_power),
	__ATTR(SYS_ATTR_NAME_STARK_CTL, 0200, NULL, set_stark_ctl),
#endif
	__ATTR(SYS_ATTR_NAME_BIST, 0200, NULL, set_bist),
	__ATTR_NULL
};

ssize_t show_rap_in(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("could not acquire mutex!!!\n");
		return -ERESTARTSYS;
	}
	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			dev_context->rap_in_sub_command);
	}
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_rap_in_status() - Handle write request to the rap attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_rap_in_status(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0x00:
			dev_context->mhl_flags &=
			    ~MHL_STATE_APPLICATION_RAP_BUSY;
			break;
		case 0x03:
			dev_context->mhl_flags |=
			    MHL_STATE_APPLICATION_RAP_BUSY;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rap_out(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->rap_out_sub_command);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rap_out() - Handle write request to the rap attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rap_out(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case MHL_RAP_POLL:
		case MHL_RAP_CONTENT_ON:
		case MHL_RAP_CONTENT_OFF:
		case MHL_RAP_CBUS_MODE_DOWN:
		case MHL_RAP_CBUS_MODE_UP:
			if (!si_mhl_tx_rap_send(dev_context, param)) {
				MHL_TX_DBG_ERR("-EPERM\n");
				status = -EPERM;
			} else {
				dev_context->rap_out_sub_command = (u8) param;
			}
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rap_out_status(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		/* todo: check for un-ack'ed RAP sub command
		 * and block until ack received.
		 */
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->rap_out_status);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rap_input_dev(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "%d\n", input_dev_rap);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_rap_input_dev(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0:
		case 1:
			input_dev_rap = (bool)param;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute rap_in_attr =
	__ATTR(SYS_ATTR_NAME_RAP_IN, 0444, show_rap_in, NULL);
static struct device_attribute rap_in_status_attr =
	__ATTR(SYS_ATTR_NAME_RAP_IN_STATUS, 0200, NULL, set_rap_in_status);
static struct device_attribute rap_out_attr =
	__ATTR(SYS_ATTR_NAME_RAP_OUT, 0644, show_rap_out, send_rap_out);
static struct device_attribute rap_out_status_attr =
	__ATTR(SYS_ATTR_NAME_RAP_OUT_STATUS, 0444, show_rap_out_status, NULL);
static struct device_attribute rap_input_dev =
	__ATTR(SYS_ATTR_NAME_RAP_INPUT_DEV, 0644, show_rap_input_dev,
	set_rap_input_dev);

static struct attribute *rap_attrs[] = {
	&rap_in_attr.attr,
	&rap_in_status_attr.attr,
	&rap_out_attr.attr,
	&rap_out_status_attr.attr,
	&rap_input_dev.attr,
	NULL
};

static struct attribute_group rap_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_RAP),
	.attrs = rap_attrs
};

ssize_t show_rcp_in(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("could not acquire mutex!!!\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else if (input_dev_rcp) {
		MHL_TX_DBG_INFO("\n");
		status = scnprintf(buf, PAGE_SIZE, "rcp_input_dev\n");
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
				   dev_context->rcp_in_key_code);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rcp_in_status() - Handle write request to the rcp attribute file.
 *
 * Writes to this file cause a RCP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rcp_in_status(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		unsigned long err_code;
		if (kstrtoul(buf, 0, &err_code) != 0)
			status = -EINVAL;
		else {
			status = count;
			if (err_code == 0) {
				if (!si_mhl_tx_rcpk_send(
				    dev_context,
				    dev_context->rcp_in_key_code)) {
					status = -ENOMEM;
				}
			} else if (!si_mhl_tx_rcpe_send(dev_context,
				(u8)err_code)) {
				status = -EINVAL;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rcp_out(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02X\n",
				   dev_context->rcp_out_key_code);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rcp_out() - Handle write request to the rcp attribute file.
 *
 * Writes to this file cause a RCP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rcp_out(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			status = -EINVAL;
		else {
			dev_context->mhl_flags &=
					~(MHL_STATE_FLAG_RCP_RECEIVED |
					MHL_STATE_FLAG_RCP_ACK |
					MHL_STATE_FLAG_RCP_NAK);
			dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_SENT;
			dev_context->rcp_send_status = 0;
			if (!si_mhl_tx_rcp_send(dev_context, (u8) param)) {
				MHL_TX_DBG_ERR("-EPERM\n");
				status = -EPERM;
			} else {
				dev_context->rcp_out_key_code = param;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rcp_out_status(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		/* todo: check for un-ack'ed RCP sub command
		 * and block until ack received.
		 */
		if (dev_context->
		    mhl_flags & (MHL_STATE_FLAG_RCP_ACK |
				 MHL_STATE_FLAG_RCP_NAK)) {
			status =
			    scnprintf(buf, PAGE_SIZE, "0x%02X\n",
				      dev_context->rcp_err_code);
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rcp_input_dev(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n", input_dev_rcp);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_rcp_input_dev(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0:
		case 1:
			input_dev_rcp = (bool)param;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute rcp_in_attr =
	__ATTR(SYS_ATTR_NAME_RCP_IN, 0444, show_rcp_in, NULL);
static struct device_attribute rcp_in_status_attr =
	__ATTR(SYS_ATTR_NAME_RCP_IN_STATUS, 0200, NULL, send_rcp_in_status);
static struct device_attribute rcp_out_attr =
	__ATTR(SYS_ATTR_NAME_RCP_OUT, 0644, show_rcp_out, send_rcp_out);
static struct device_attribute rcp_out_status_attr =
	__ATTR(SYS_ATTR_NAME_RCP_OUT_STATUS, 0444, show_rcp_out_status, NULL);
static struct device_attribute rcp_input_dev =
	__ATTR(SYS_ATTR_NAME_RCP_INPUT_DEV, 0644, show_rcp_input_dev,
	set_rcp_input_dev);

static struct attribute *rcp_attrs[] = {
	&rcp_in_attr.attr,
	&rcp_in_status_attr.attr,
	&rcp_out_attr.attr,
	&rcp_out_status_attr.attr,
	&rcp_input_dev.attr,
	NULL
};

static struct attribute_group rcp_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_RCP),
	.attrs = rcp_attrs
};

ssize_t show_rbp_in(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("could not acquire mutex!!!\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->rbp_in_button_code);
	}
	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rbp_in_status() - Handle write request to the rbp attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rbp_in_status(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		unsigned long err_code;
		status = kstrtoul(buf, 0, &err_code);
		if (status == 0) {
			status = count;
			if (err_code == 0) {
				if (!si_mhl_tx_rbpk_send(
					dev_context,
					dev_context->rbp_in_button_code)) {
					status = -ENOMEM;
				}
			} else if (!si_mhl_tx_rbpe_send(
					dev_context, (u8)err_code)) {
				status = -EINVAL;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rbp_input_dev(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n", input_dev_rbp);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_rbp_input_dev(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0:
		case 1:
			input_dev_rbp = (bool)param;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute rbp_in_attr =
	__ATTR(SYS_ATTR_NAME_RBP_IN, 0444, show_rbp_in, NULL);
static struct device_attribute rbp_in_status_attr =
	__ATTR(SYS_ATTR_NAME_RBP_IN_STATUS, 0200, NULL, send_rbp_in_status);
static struct device_attribute rbp_input_dev =
	__ATTR(SYS_ATTR_NAME_RBP_INPUT_DEV, 0644, show_rbp_input_dev,
	set_rbp_input_dev);

static struct attribute *rbp_attrs[] = {
	&rbp_in_attr.attr,
	&rbp_in_status_attr.attr,
	&rbp_input_dev.attr,
	NULL
};

static struct attribute_group rbp_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_RBP),
	.attrs = rbp_attrs
};

ssize_t show_ucp_in(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("could not acquire mutex!!!\n");
		return -ERESTARTSYS;
	}
	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->ucp_in_key_code);
	}
	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_ucp_in_status() - Handle write request to the ucp attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_ucp_in_status(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		unsigned long err_code;
		status = kstrtoul(buf, 0, &err_code);
		if (status == 0) {
			status = count;
			if (err_code == 0) {
				if (!si_mhl_tx_ucpk_send(
					dev_context,
					dev_context->ucp_in_key_code)) {
					status = -ENOMEM;
				}
			} else if (!si_mhl_tx_ucpe_send(
					dev_context, (u8) err_code)) {
				status = -EINVAL;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_ucp_out(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->ucp_out_key_code);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_ucp_out() - Handle write request to the ucp attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_ucp_out(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		status = kstrtoul(buf, 0, &param);
		if (status == 0) {
			dev_context->mhl_flags &=
					~(MHL_STATE_FLAG_UCP_RECEIVED |
					MHL_STATE_FLAG_UCP_ACK |
					MHL_STATE_FLAG_UCP_NAK);
			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_SENT;
			if (!si_mhl_tx_ucp_send(dev_context, (u8)param)) {
				MHL_TX_DBG_ERR("-EPERM\n");
				status = -EPERM;
			} else {
				dev_context->ucp_out_key_code = (u8)param;
				status = count;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_ucp_out_status(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		/* todo: check for un-ack'ed RAP sub command
		 * and block until ack received.
		 */
		if (dev_context->
		    mhl_flags & (MHL_STATE_FLAG_UCP_ACK |
				 MHL_STATE_FLAG_UCP_NAK)) {
			status =
			    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
				      dev_context->ucp_err_code);
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_ucp_input_dev(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n", input_dev_ucp);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_ucp_input_dev(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0:
		case 1:
			input_dev_ucp = (bool)param;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute ucp_in_attr =
	__ATTR(SYS_ATTR_NAME_UCP_IN, 0444, show_ucp_in, NULL);
static struct device_attribute ucp_in_status_attr =
	__ATTR(SYS_ATTR_NAME_UCP_IN_STATUS, 0200, NULL, send_ucp_in_status);
static struct device_attribute ucp_out_attr =
	__ATTR(SYS_ATTR_NAME_UCP_OUT, 0644, show_ucp_out, send_ucp_out);
static struct device_attribute ucp_out_status_attr =
	__ATTR(SYS_ATTR_NAME_UCP_OUT_STATUS, 0444, show_ucp_out_status, NULL);

static struct device_attribute ucp_input_dev =
	__ATTR(SYS_ATTR_NAME_UCP_INPUT_DEV, 0644, show_ucp_input_dev,
	set_ucp_input_dev);

static struct attribute *ucp_attrs[] = {
	&ucp_in_attr.attr,
	&ucp_in_status_attr.attr,
	&ucp_out_attr.attr,
	&ucp_out_status_attr.attr,
	&ucp_input_dev.attr,
	NULL
};

static struct attribute_group ucp_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_UCP),
	.attrs = ucp_attrs
};

ssize_t show_devcap_local_offset(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->dev_cap_local_offset);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_devcap_local_offset(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		status = kstrtoul(buf, 0, &param);
		if ((status != 0) || (param >= 16)) {
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
		} else {
			dev_context->dev_cap_local_offset = param;
			status = count;
		}
	}

	up(&dev_context->isr_lock);
	return status;
}

ssize_t show_devcap_local(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			dev_cap_values[dev_context->dev_cap_local_offset]);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_devcap_local(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		status = kstrtoul(buf, 0, &param);
		if ((status != 0) || (param > 0xFF)) {
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
		} else {
			dev_cap_values[dev_context->dev_cap_local_offset] =
			    param;
			status = count;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_devcap_remote_offset(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->dev_cap_remote_offset);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_devcap_remote_offset(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		status = kstrtoul(buf, 0, &param);
		if ((status == 0) && (param < 16)) {
			dev_context->dev_cap_remote_offset = (u8)param;
			status = count;
		} else {
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_devcap_remote(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		uint8_t regValue;
		status = si_mhl_tx_get_peer_dev_cap_entry(dev_context,
							  dev_context->
							  dev_cap_remote_offset,
							  &regValue);
		if (status != 0) {
			/*
			 * Driver is busy and cannot provide the requested
			 * DEVCAP register value right now so inform caller
			 * they need to try again later.
			 */
			status = -EAGAIN;
		} else {
			status = scnprintf(buf, PAGE_SIZE, "0x%02x", regValue);
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute attr_devcap_local_offset =
	__ATTR(SYS_ATTR_NAME_DEVCAP_LOCAL_OFFSET, 0644,
	show_devcap_local_offset, set_devcap_local_offset);
static struct device_attribute attr_devcap_local =
	__ATTR(SYS_ATTR_NAME_DEVCAP_LOCAL, 0644, show_devcap_local,
	set_devcap_local);
static struct device_attribute attr_devcap_remote_offset =
	__ATTR(SYS_ATTR_NAME_DEVCAP_REMOTE_OFFSET, 0644,
	show_devcap_remote_offset, set_devcap_remote_offset);
static struct device_attribute attr_devcap_remote =
	__ATTR(SYS_ATTR_NAME_DEVCAP_REMOTE, 0444, show_devcap_remote, NULL);

static struct attribute *devcap_attrs[] = {
	&attr_devcap_local_offset.attr,
	&attr_devcap_local.attr,
	&attr_devcap_remote_offset.attr,
	&attr_devcap_remote.attr,
	NULL
};

static struct attribute_group devcap_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_DEVCAP),
	.attrs = devcap_attrs
};

ssize_t set_hdcp_force_content_type(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long new_hdcp_content_type;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = kstrtoul(buf, 0, &new_hdcp_content_type);
		if (status == 0) {
			status = count;
			hdcp_content_type = new_hdcp_content_type;
		}
	}

	up(&dev_context->isr_lock);
	return status;
}

ssize_t show_hdcp_force_content_type(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "%d", hdcp_content_type);
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute attr_hdcp_force_content_type =
	__ATTR(SYS_ATTR_NAME_HDCP_CONTENT_TYPE, 0644,
	show_hdcp_force_content_type, set_hdcp_force_content_type);

static struct attribute *hdcp_attrs[] = {
	&attr_hdcp_force_content_type.attr,
	NULL
};

static struct attribute_group hdcp_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_HDCP),
	.attrs = hdcp_attrs
};

ssize_t set_vc_assign(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	struct tdm_alloc_burst tdm_burst;
	uint8_t slots;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		enum cbus_mode_e cbus_mode;
		status = kstrtoul(buf, 0, &param);
		if (status != 0)
			goto input_err;
		status = count;

		cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
		switch (cbus_mode) {
		case CM_eCBUS_S:
			if (param > 24)
				goto input_err;
			break;
		case CM_eCBUS_D:
			if (param > 200)
				goto input_err;
			break;
		default:
input_err:
			MHL_TX_DBG_ERR(
				"Invalid number of eMSC slots specified\n");
			status = -EINVAL;
			goto done;
		}

		slots = (u8)param;

		memset(&tdm_burst, 0, sizeof(tdm_burst));
		tdm_burst.header.burst_id.high = burst_id_VC_ASSIGN >> 8;
		tdm_burst.header.burst_id.low = (uint8_t) burst_id_VC_ASSIGN;
		tdm_burst.header.sequence_index = 1;
		tdm_burst.header.total_entries = 2;
		tdm_burst.num_entries_this_burst = 2;
		tdm_burst.vc_info[0].vc_num = TDM_VC_E_MSC;
		tdm_burst.vc_info[0].feature_id = FEATURE_ID_E_MSC;
		tdm_burst.vc_info[0].req_resp.channel_size = slots;
		tdm_burst.vc_info[1].vc_num = TDM_VC_T_CBUS;
		tdm_burst.vc_info[1].feature_id = FEATURE_ID_USB;
		tdm_burst.vc_info[1].req_resp.channel_size = (u8) (24 - slots);
		tdm_burst.vc_info[2].vc_num = 0;
		tdm_burst.vc_info[2].feature_id = 0;
		tdm_burst.vc_info[2].req_resp.channel_size = 0;
		tdm_burst.header.checksum = 0;
		tdm_burst.header.checksum =
		    calculate_generic_checksum((uint8_t *) (&tdm_burst), 0,
					       sizeof(tdm_burst));

		dev_context->prev_virt_chan_slot_counts[TDM_VC_E_MSC] =
		    dev_context->virt_chan_slot_counts[TDM_VC_E_MSC];
		dev_context->prev_virt_chan_slot_counts[TDM_VC_T_CBUS] =
		    dev_context->virt_chan_slot_counts[TDM_VC_T_CBUS];

		dev_context->virt_chan_slot_counts[TDM_VC_E_MSC] = slots;
		dev_context->virt_chan_slot_counts[TDM_VC_T_CBUS] =
		    (u8) (24 - slots);

		si_mhl_tx_request_write_burst(dev_context, 0, sizeof(tdm_burst),
					      (uint8_t *) &tdm_burst);
		status = count;
	}
done:
	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_vc_assign(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "%d", 0);
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute attr_vc_assign =
	__ATTR(SYS_ATTR_NAME_VC_ASSIGN, 0644, show_vc_assign, set_vc_assign);

static struct attribute *vc_attrs[] = {
	&attr_vc_assign.attr,
	NULL
};

static struct attribute_group vc_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_VC),
	.attrs = vc_attrs
};

static void check_dcap_status(struct mhl_dev_context *dev)
{
	if (DRV_INTR_TDM_SYNC & dev->intr_info.flags) {
		if (MHL_STATUS_DCAP_RDY & dev->status_0)
			si_mhl_tx_ecbus_started(dev);
		if (dev->misc_flags.flags.mhl_hpd && !dev->edid_valid) {
			if (dev->misc_flags.flags.have_complete_devcap) {
				si_mhl_tx_initiate_edid_sequence(
					dev->edid_parser_context);
			}
		}
	}
}
/*
 * Called from the Titan interrupt handler to parse data
 * received from the eSMC BLOCK hardware. Process all waiting input
 * buffers. If multiple messages are sent in the same BLOCK Command buffer,
 * they are sorted out here also.
 */
static void si_mhl_tx_emsc_received(struct mhl_dev_context *context)
{
	struct drv_hw_context *hw_context =
		(struct drv_hw_context *)(&context->drv_context);
	int length, index, pass;
	uint16_t burst_id;
	uint8_t *prbuf, *pmsg;

	while (si_mhl_tx_drv_peek_block_input_buffer(context,
		&prbuf, &length) == 0) {
		index = 0;
		pass = 0;
/*		dump_array(DBG_MSG_LEVEL_INFO, "si_mhl_tx_emsc_received",
			prbuf, length); */
		do {
			struct MHL_burst_id_t *p_burst_id;
			pmsg = &prbuf[index];
			p_burst_id = (struct MHL_burst_id_t *)pmsg;
			burst_id = (((uint16_t)pmsg[0]) << 8) | pmsg[1];
			/* Move past the BURST ID */
			index += sizeof(*p_burst_id);

			switch (burst_id) {
			case burst_id_HID_PAYLOAD:
				build_received_hid_message(context,
					&pmsg[3], pmsg[2]);
				index += (1 + pmsg[2]);
				break;

			case burst_id_BLK_RCV_BUFFER_INFO:
				hw_context->block_protocol.peer_blk_rx_buf_max
					= pmsg[3];
				hw_context->block_protocol.peer_blk_rx_buf_max
					<<= 8;
				hw_context->block_protocol.peer_blk_rx_buf_max
					|= pmsg[2];

				hw_context->block_protocol.
					peer_blk_rx_buf_avail =
					hw_context->block_protocol.
					peer_blk_rx_buf_max -
					(EMSC_RCV_BUFFER_DEFAULT -
						hw_context->block_protocol.
						peer_blk_rx_buf_avail);
				MHL3_HID_DBG_INFO(
					"Total PEER Buffer Size: %d\n",
					hw_context->block_protocol.
					peer_blk_rx_buf_max);

				index += 2;
				break;

			case burst_id_BITS_PER_PIXEL_FMT:
				index += (4 + (2 * pmsg[5]));
				/* inform the rest of the code of the
				 * acknowledgment
				 */
				/*si_mhl_tx_check_av_link_status(context);*/
				break;
				/* any BURST_ID reported by EMSC_SUPPORT can be
				 * sent using eMSC BLOCK
				 */
			case burst_id_HEV_DTDA:{
				struct MHL3_hev_dtd_a_data_t *p_dtda =
				    (struct MHL3_hev_dtd_a_data_t *) p_burst_id;
				index +=
				    sizeof(*p_dtda) -
				    sizeof(*p_burst_id);
				}
				break;
			case burst_id_HEV_DTDB:{
				struct MHL3_hev_dtd_b_data_t *p_dtdb =
				    (struct MHL3_hev_dtd_b_data_t *) p_burst_id;
				index +=
				    sizeof(*p_dtdb) -
				    sizeof(*p_burst_id);
				}
				/* for now, as a robustness excercise, adjust
				 * index according to the this burst field
				 */
				break;
			case burst_id_HEV_VIC:{
				struct MHL3_hev_vic_data_t *p_burst;
				p_burst =
				    (struct MHL3_hev_vic_data_t *) p_burst_id;
				index +=
				    sizeof(*p_burst) -
				    sizeof(*p_burst_id) +
				    sizeof(p_burst->
				    video_descriptors[0]) *
				    p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_3D_VIC:{
				struct MHL3_hev_vic_data_t *p_burst =
				    (struct MHL3_hev_vic_data_t *) p_burst_id;
				index +=
				    sizeof(*p_burst) -
				    sizeof(*p_burst_id)
				    - sizeof(p_burst->video_descriptors) +
				    sizeof(p_burst->
					   video_descriptors[0]) *
				    p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_3D_DTD:{
				struct MHL2_video_format_data_t *p_burst =
				    (struct MHL2_video_format_data_t *)
				    p_burst_id;
				index +=
				    sizeof(*p_burst) -
				    sizeof(*p_burst_id)
				    - sizeof(p_burst->video_descriptors)
				    +
				    sizeof(p_burst->
					   video_descriptors[0]) *
				    p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_VC_ASSIGN:{
				struct SI_PACK_THIS_STRUCT
				    tdm_alloc_burst * p_burst;
				p_burst =
				    (struct SI_PACK_THIS_STRUCT
				     tdm_alloc_burst*)p_burst_id;
				index +=
				    sizeof(*p_burst) -
				    sizeof(*p_burst_id)
				    - sizeof(p_burst->reserved)
				    - sizeof(p_burst->vc_info)
				    +
				    sizeof(p_burst->vc_info[0]) *
				    p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_VC_CONFIRM:{
				struct SI_PACK_THIS_STRUCT
				    tdm_alloc_burst * p_burst;
				p_burst =
				    (struct SI_PACK_THIS_STRUCT
				     tdm_alloc_burst*)p_burst_id;
				index +=
				    sizeof(*p_burst) -
				    sizeof(*p_burst_id)
				    - sizeof(p_burst->reserved)
				    - sizeof(p_burst->vc_info) +
				    sizeof(p_burst->vc_info[0]) *
				    p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_AUD_DELAY:{
				struct MHL3_audio_delay_burst_t
				    *p_burst =
				    (struct MHL3_audio_delay_burst_t *)
				    p_burst_id;
				index +=
				    sizeof(*p_burst) -
				    sizeof(*p_burst_id)
				    - sizeof(p_burst->reserved);
				}
				break;
			case burst_id_ADT_BURSTID:{
				struct MHL3_adt_data_t *p_burst =
				    (struct MHL3_adt_data_t *)
				    p_burst_id;
				index +=
				    sizeof(*p_burst) -
				    sizeof(*p_burst_id);
				}
				break;
			case burst_id_BIST_SETUP:{
				struct SI_PACK_THIS_STRUCT
				    bist_setup_burst * p_bist_setup;
				p_bist_setup =
				    (struct SI_PACK_THIS_STRUCT
				     bist_setup_burst*)p_burst_id;
				index +=
				    sizeof(*p_bist_setup) -
				    sizeof(*p_burst_id);
				}
				break;
			case burst_id_BIST_RETURN_STAT:{
				struct SI_PACK_THIS_STRUCT
				    bist_return_stat_burst
				    *p_bist_return;
				p_bist_return =
				    (struct SI_PACK_THIS_STRUCT
				     bist_return_stat_burst*)
				    p_burst_id;
				index +=
				    sizeof(*p_bist_return) -
				    sizeof(*p_burst_id);
				}
				break;
			case burst_id_EMSC_SUPPORT:{
				/* for robustness, adjust index
				 * according to the num-entries this
				 * burst field
				 */
				struct MHL3_emsc_support_data_t *p_burst =
				    (struct MHL3_emsc_support_data_t *)
				    p_burst_id;
				index +=
				    sizeof(*p_burst) -
				    sizeof(*p_burst_id)
				    - sizeof(p_burst->payload)
				    +
				    sizeof(p_burst->payload.
					   burst_ids[0]) *
				    p_burst->num_entries_this_burst;
				}
				break;
			default:
				if ((MHL_TEST_ADOPTER_ID == burst_id) ||
				    (burst_id >= adopter_id_RANGE_START)) {
					struct EMSC_BLK_ADOPT_ID_PAYLD_HDR
					    *p_adopter_id;
					p_adopter_id =
					    (struct EMSC_BLK_ADOPT_ID_PAYLD_HDR
					     *)pmsg;
					MHL_TX_DBG_ERR
					    ("Unsupported ADOPTER_ID: %04X\n",
					     burst_id);
					index += p_adopter_id->remaining_length;
				} else {
					MHL_TX_DBG_ERR
					    ("Unsupported BURST ID: %04X\n",
					     burst_id);
					index = length;
				}
				break;
			}

			length -= index;
		} while (length > 0);
		si_mhl_tx_drv_free_block_input_buffer(context);
	}
}

/*
 * Interrupt handler for MHL transmitter interrupts.
 *
 * @irq:	The number of the asserted IRQ line that caused
 *		this handler to be called.
 * @data:	Data pointer passed when the interrupt was enabled,
 *		which in this case is a pointer to an mhl_dev_context struct.
 *
 * Always returns IRQ_HANDLED.
 */
static irqreturn_t mhl_irq_handler(int irq, void *data)
{
	struct mhl_dev_context *dev_context = (struct mhl_dev_context *)data;

	MHL_TX_DBG_INFO("called\n");

	if (!down_interruptible(&dev_context->isr_lock)) {
		int loop_limit = 5;
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN)
			goto irq_done;
		if (dev_context->dev_flags & DEV_FLAG_COMM_MODE)
			goto irq_done;

		do {
			memset(&dev_context->intr_info, 0,
				sizeof(*(&dev_context->intr_info)));

			dev_context->intr_info.edid_parser_context =
				dev_context->edid_parser_context;

			dev_context->drv_info->
				mhl_device_isr((struct drv_hw_context *)
				(&dev_context->drv_context),
				&dev_context->intr_info);

			/* post process events raised by interrupt handler */
			if (dev_context->intr_info.flags &
				DRV_INTR_DISCONNECT) {
				dev_context->misc_flags.flags.rap_content_on =
					false;
				dev_context->misc_flags.flags.mhl_rsen = false;
				dev_context->mhl_connection_event = true;
				dev_context->mhl_connected =
					MHL_TX_EVENT_DISCONNECTION;
				si_mhl_tx_process_events(dev_context);
			} else {
				if (dev_context->intr_info.flags &
					DRV_INTR_CONNECT) {
					dev_context->misc_flags.flags.
						rap_content_on = true;
					dev_context->rap_in_sub_command =
						MHL_RAP_CONTENT_ON;
					dev_context->misc_flags.flags.mhl_rsen =
						true;
					dev_context->mhl_connection_event =
						true;
					dev_context->mhl_connected =
						MHL_TX_EVENT_CONNECTION;
					si_mhl_tx_process_events(dev_context);
				}

				if (dev_context->intr_info.flags &
					DRV_INTR_CBUS_ABORT)
					process_cbus_abort(dev_context);

				if (dev_context->intr_info.flags &
					DRV_INTR_WRITE_BURST)
					si_mhl_tx_process_write_burst_data
						(dev_context);

				/* sometimes SET_INT and WRITE_STAT come at the
				 * same time. Always process WRITE_STAT first.
				 */
				if (dev_context->intr_info.flags &
					DRV_INTR_WRITE_STAT)
					si_mhl_tx_got_mhl_status(dev_context,
						&dev_context->intr_info.
						dev_status);

				if (dev_context->intr_info.flags &
					DRV_INTR_SET_INT)
					si_mhl_tx_got_mhl_intr(dev_context,
						dev_context->intr_info.
						int_msg[0],
						dev_context->intr_info.
						int_msg[1]);

				if (dev_context->intr_info.flags &
					DRV_INTR_MSC_DONE)
					si_mhl_tx_msc_command_done(dev_context,
						dev_context->intr_info.
						msc_done_data);

				if (dev_context->intr_info.flags &
					DRV_INTR_EMSC_INCOMING)
					si_mhl_tx_emsc_received(dev_context);

				if (dev_context->intr_info.flags &
					DRV_INTR_HPD_CHANGE)
					si_mhl_tx_notify_downstream_hpd_change
					    (dev_context,
					     dev_context->intr_info.hpd_status);

				if (dev_context->intr_info.flags &
					DRV_INTR_MSC_RECVD) {
					dev_context->msc_msg_arrived = true;
					dev_context->msc_msg_sub_command =
					    dev_context->intr_info.msc_msg[0];
					dev_context->msc_msg_data =
					    dev_context->intr_info.msc_msg[1];
					si_mhl_tx_process_events(dev_context);
				}

				/*
				   MHL3 spec requires the process of reading
				   the remainder of XDEVCAP and all of DEVCAP
				   to be deferred until after the first
				   transition to eCBUS mode.
				   Check DCAP_RDY, initiate XDEVCAP read here.

				 */
				check_dcap_status(dev_context);
			}
			/*
			 * Send any messages that may have been queued up
			 * as the result of interrupt processing.
			 */
			si_mhl_tx_drive_states(dev_context);
			if (si_mhl_tx_drv_get_cbus_mode(dev_context) >=
			    CM_eCBUS_S) {
				si_mhl_tx_push_block_transactions(dev_context);
			}
		} while ((--loop_limit > 0) && is_interrupt_asserted());
irq_done:
		up(&dev_context->isr_lock);
	}

	return IRQ_HANDLED;
}

/* APIs provided by the MHL layer to the lower level driver */

int mhl_handle_power_change_request(struct device *parent_dev, bool power_up)
{
	struct mhl_dev_context *dev_context;
	int status;

	dev_context = dev_get_drvdata(parent_dev);

	MHL_TX_DBG_INFO("\n");

	/* Power down the MHL transmitter hardware. */
	status = down_interruptible(&dev_context->isr_lock);
	if (status) {
		MHL_TX_DBG_ERR("failed to acquire ISR semaphore,"
			       "status: %d\n", status);
		goto done;
	}

	if (power_up)
		status = si_mhl_tx_initialize(dev_context);
	else
		status = si_mhl_tx_shutdown(dev_context);

	up(&dev_context->isr_lock);
done:
	return status;

}

#ifdef CONFIG_MHL3_SEC_FEATURE
#ifdef CONFIG_TESTONLY_SYSFS_SW_REG_TUNING	/* TESTONLY - HW Eye verification */
static int sii8620_electrical_eye_verification(struct mhl_dev_context *dev_context,
		unsigned long event)

{
	int i = 0;

	if (event) {
		pr_info("sii8620 : %s:%d:(%d) MHL-Connection(TESTONLY)*****\n", __func__, __LINE__, (int)event);
		clk_prepare_enable(dev_context->pdata->mhl_clk);
		dev_context->pdata->charger_mhl_cb(true, 0x03);
		dev_context->pdata->power(true);
		si_mhl_tx_initialize(dev_context);
		pr_info("sii8620 : mhl3-elc.defualt reg set++ \n");

		mhl_tx_write_reg_elc(dev_context, (0x72), 0x0B, 0xFE);  /* TX_PAGE_0 */
		mhl_tx_write_reg_elc(dev_context, (0x72), 0x80, 0x33);  /* TX_PAGE_0 */

		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x37, 0x07); /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x39, 0x00); /* TX_PAGE_3 */

		mhl_tx_write_reg_elc(dev_context, (0x72), 0x0E, 0xA0);  /* TX_PAGE_0 */

		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x62, 0x00); /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x48, 0x80); /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x47, 0xF9); /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x45, 0x01); /* TX_PAGE_3 */

		mhl_tx_write_reg_elc(dev_context, (0x92), 0x4C, 0xE8);  /* TX_PAGE_2 */
		mhl_tx_write_reg_elc(dev_context, (0x92), 0x4D, 0x04);  /* TX_PAGE_2 */

		mhl_tx_write_reg_elc(dev_context, (0x7A), 0x16, 0x05);
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x31, 0xF0); /* TX_PAGE_3 */

		mhl_tx_write_reg_elc(dev_context, (0x92), 0x10, 0x90);  /* TX_PAGE_2 */
		mhl_tx_write_reg_elc(dev_context, (0x92), 0x12, 0x11);  /* TX_PAGE_2 */
		mhl_tx_write_reg_elc(dev_context, (0x92), 0x11, 0x01);  /* TX_PAGE_2 */
		mhl_tx_write_reg_elc(dev_context, (0x92), 0x15, 0x87);  /* TX_PAGE_2 */

		mhl_tx_write_reg_elc(dev_context, (0x72), 0x85, 0x04);  /* TX_PAGE_0 */
		mhl_tx_write_reg_elc(dev_context, (0x72), 0xF7, 0x00);  /* TX_PAGE_0 */
		mhl_tx_write_reg_elc(dev_context, (0x92), 0x81, 0x33);  /* TX_PAGE_2 */
		mhl_tx_write_reg_elc(dev_context, (0x72), 0x80, 0x33);  /* TX_PAGE_0 */
		mhl_tx_write_reg_elc(dev_context, (0x92), 0x85, 0xA0);  /* TX_PAGE_2 */
		mhl_tx_write_reg_elc(dev_context, (0x72), 0x0D, 0x02);  /* TX_PAGE_0 */

		mhl_tx_write_reg_elc(dev_context, (0x9A), 0xE0, 0x0F); /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0xE1, 0x01); /* TX_PAGE_3 */

		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x11, 0x10);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x12, 0x18);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x2A, 0x61);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x2B, 0x46);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x2C, 0x15);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x2D, 0x01);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x17, 0x01);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x21, 0xF8);  /* TX_PAGE_7 */

		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x43, 0xBA);  /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x46, 0x2D);  /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x28, 0x02);  /* TX_PAGE_7 */

		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x30, 0x01);  /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x32, 0xA8);  /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x33, 0x03);  /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x34, 0x35);  /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x36, 0x01);  /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x50, 0x02);  /* TX_PAGE_3 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x51, 0x04);  /* TX_PAGE_3 */

		mhl_tx_write_reg_elc(dev_context, (0x72), 0x0E, 0x20);  /* TX_PAGE_0 */
		mhl_tx_write_reg_elc(dev_context, (0x9A), 0x39, 0x80);  /* TX_PAGE_3 */

		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x10, 0x5C);  /* TX_PAGE_7 */

		mhl_tx_write_reg_elc(dev_context, (0x7A), 0x3B, 0x1C);  /* TX_PAGE_1 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x10, 0x48);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x24, 0x0C);  /* TX_PAGE_7 */
		mhl_tx_write_reg_elc(dev_context, (0xC2), 0x25, 0x80);  /* TX_PAGE_7 */
		pr_info("sii8620 : mhl3-elc.defualt reg set-- \n");

		/*--Add--*/
		for (i = 0; i < dev_context->pdata->m_cnt; i++) {
			mhl_tx_write_reg_elc(dev_context, dev_context->pdata->m_page[i],
					dev_context->pdata->m_offset[i], dev_context->pdata->m_value[i]);
			pr_info("sii8620 : 0x%02x x 0x%02x = 0x%02x \n", dev_context->pdata->m_page[i],
					dev_context->pdata->m_offset[i], dev_context->pdata->m_value[i]);
		}
	} else {
		pr_info("sii8620 : %s:%d:(%d) MHL-Disconnection(TESTONLY)*****\n", __func__, __LINE__, (int)event);
		clk_disable_unprepare(dev_context->pdata->mhl_clk);
		disable_irq(dev_context->drv_info->irq);
		dev_context->pdata->charger_mhl_cb(false, -1);
		dev_context->pdata->power(false);
		dev_context->pdata->m_cnt = 0;
	}

	return 0;
}
#endif /* CONFIG_TESTONLY_SYSFS_SW_REG_TUNING */


/* DEFAULT BEHAVIOR MODE */
int sii8620_detection_start(unsigned long event)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(sii8620_dev);

	if (event == dev_context->detection_state) {
		pr_info("sii8240 : Same muic event, Ignored!\n");
		return MHL_CON_UNHANDLED;
	}
	dev_context->detection_state = event;

#ifdef CONFIG_TESTONLY_SYSFS_SW_REG_TUNING	/* TESTONLY - HW Eye verification */
	sii8620_electrical_eye_verification(dev_context, event);
#else
	if (event) {
		pr_info("sii8620 : %s:%d: MHL-Connection(%d)*****\n", __func__, __LINE__, (int)event);
		wake_lock(&dev_context->mhl_wake_lock);
		dev_context->pdata->power(true);
		clk_prepare_enable(dev_context->pdata->mhl_clk);
		si_mhl_tx_initialize(dev_context);
		enable_irq(dev_context->drv_info->irq);
	} else {
		pr_info("sii8620 : %s:%d: MHL-Disconnection(%d)*****\n", __func__, __LINE__, (int)event);
		clk_disable_unprepare(dev_context->pdata->mhl_clk);
		disable_irq(dev_context->drv_info->irq);
		dev_context->pdata->charger_mhl_cb(false, -1);
		dev_context->pdata->power(false);
		/* MHL version reset for Factory test */
		dev_context->dev_cap_cache.mdc.mhl_version = 0;
		dev_context->peer_mhl3_version = 0;
#ifdef CONFIG_MHL3_DVI_WR
		if(force_ocbus_for_ects == 2) {
			force_ocbus_for_ects = 0;
			dev_context->mhl3_to_mhl1_2 = false;
		}
#endif
#ifdef CONFIG_MHL3_SEC_FEATURE
		if (dev_context->mhl_event_switch.state != MHL_EVENT_CLR) {
			pr_info("sii8620 : MHL switch event sent : %d\n", MHL_EVENT_CLR);
			switch_set_state(&dev_context->mhl_event_switch, MHL_EVENT_CLR);
		}
#endif
#ifdef CONFIG_SII8620_CHECK_MONITOR
		dev_context->pdata->link_monitor(0x04);
#endif
		wake_unlock(&dev_context->mhl_wake_lock);
		goto power_down;
	}

	return MHL_CON_HANDLED;

power_down:
	return MHL_CON_UNHANDLED;
#endif /* #ifdef CONFG_TESTONLY_SYSFS_SW_REG_TUNING */
	return 0;
}

/* DEFAULT BEHAVIOR MODE */
static int sii8620_detection_callback(struct notifier_block *this,
					unsigned long event, void *ptr)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(sii8620_dev);
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)ptr;

	switch (attached_dev) {
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		dev_context->mhl_muic_type = MHL_SMART_DOCK;
		break;
/* disable MMdock feature */
/*	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
 *		dev_context->mhl_muic_type = MHL_MM_DOCK;
 *		break; */
	case ATTACHED_DEV_MHL_MUIC:
		dev_context->mhl_muic_type = MHL_MUIC_DEV;
		break;
	default:
		dev_context->mhl_muic_type = MHL_UNHANDLED;
		return MHL_CON_UNHANDLED;
	}
	dev_context->muic_state = event;

	return sii8620_detection_start(event);
}
#endif /* CONFIG_MHL3_SEC_FEATURE */

#ifdef CONFIG_MUIC_NOTIFIER
static void sii8620_notifier_delay_work(struct work_struct *work)
{
	struct mhl_dev_context *dev_context = container_of(work, struct mhl_dev_context,
				notifier_register_work.work);

	muic_notifier_register(&dev_context->mhl_nb,
			sii8620_detection_callback, MUIC_NOTIFY_DEV_MHL);
}
#endif /* CONFIG_MUIC_NOTIFIER */

#ifdef CONFIG_SS_FACTORY
#define SII_ID 34336 /*8620*/
static ssize_t sii8620_test_show(struct class *dev,
		struct class_attribute *attr,
		char *buf)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	struct sii8620_platform_data *pdata = sii8620->pdata;

	int size, ret_val;
	uint16_t number;

	pdata->power(1);
	msleep(20);
	pdata->hw_reset();
#ifdef CONFIG_MHL_SPI
	if (use_spi) {
		u8 cmd = spi_op_enable;
		spi_write(spi_dev, &cmd, 1);
	}
#endif
	ret_val = mhl_tx_read_reg(sii8620, REG_DEV_IDH);
	if (ret_val < 0) {
		MHL_TX_DBG_ERR("1st i2c fail : 0x%x : retry\n", ret_val);
		pdata->hw_reset();
		ret_val = mhl_tx_read_reg(sii8620, REG_DEV_IDH);
	}
	if (ret_val < 0) {
		MHL_TX_DBG_ERR("i2c read fail : 0x%x\n", ret_val);
		pdata->power(0);
		return ret_val;
	}
	number = ret_val << 8;

	ret_val = mhl_tx_read_reg(sii8620, REG_DEV_IDL);
	if (ret_val < 0) {
		MHL_TX_DBG_ERR("i2c read fail :  0x%x\n", ret_val);
		pdata->power(0);
		return ret_val;
	}
	ret_val |= number;

	pr_info("sii8620 : %s:%d: get Device_ID, i2c read : ret_val = %04x\n",
			__func__, __LINE__, ret_val);
	pdata->power(0);

	size = snprintf(buf, 10, "%d\n", ret_val == SII_ID ? 1 : 0);
	return size;
}
static CLASS_ATTR(test_result, 0664, sii8620_test_show, NULL);
#endif /* CONFIG_SS_FACTORY */

#ifdef CONFIG_MHL_CONFIG_TOOLS
static ssize_t sii8620_swing_v2_test_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	char str_buf[] = "ex) echo 0x9b > swing_v2\n"
		"   -> clock = 0x9, data =0xb\n";

	sprintf(buf, str_buf);
	sprintf(str_buf, "current value : 0x%02x\n",
			sii8620->pdata->swing_level_v2);
	buf = strcat(buf, str_buf);

	return strlen(buf) + 1;
}
static ssize_t sii8620_swing_v2_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	int numbers[2];

	get_options(buf, 2, numbers);
	if (numbers[0] == 1 && numbers[1] >=0 && numbers[1] <= 0xff)
		sii8620->pdata->swing_level_v2 = numbers[1];
	else
		sii8620->pdata->swing_level_v2 = 0xBB; /*Clk=B and Data=B*/

	return size;
}

static CLASS_ATTR(swing_v2, 0664,
		sii8620_swing_v2_test_show, sii8620_swing_v2_test_store);


static ssize_t sii8620_swing_v3_test_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	char str_buf[] = "ex) echo 0x9b > swing_v2\n"
		"   -> clock = 0x9, data =0xb\n";

	sprintf(buf, str_buf);
	sprintf(str_buf, "current value : 0x%02x\n",
			sii8620->pdata->swing_level_v3);
	buf = strcat(buf, str_buf);

	return strlen(buf) + 1;
}
static ssize_t sii8620_swing_v3_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	int numbers[2];

	get_options(buf, 2, numbers);
	if (numbers[0] == 1 && numbers[1] >=0 && numbers[1] <= 0xff)
		sii8620->pdata->swing_level_v3 = numbers[1];
	else
		sii8620->pdata->swing_level_v3 = 0xA2;

	return size;
}

static CLASS_ATTR(swing_v3, 0664,
		sii8620_swing_v3_test_show, sii8620_swing_v3_test_store);



extern int hdmi_forced_resolution;
static ssize_t sii8620_timing_test_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hdmi_forced_resolution);

}

static ssize_t sii8620_timing_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int timing, ret;

	ret = kstrtoint(buf, 10, &timing);
	if (unlikely(ret < 0))
		return size;

	if (timing >= 0)
		hdmi_forced_resolution = timing;
	else
		hdmi_forced_resolution = -1;

	return size;
}

static CLASS_ATTR(timing, 0664,
		sii8620_timing_test_show, sii8620_timing_test_store);

static ssize_t sii8620_test_case_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	int size;

	size = snprintf(buf, 10, "%d\n", mhl_get_test_result());
	return size;
}

static ssize_t sii8620_test_case_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	int numbers[6] = {0,};

	get_options(buf, 5, numbers);
	if (numbers[0] == 0)
		return size;

	mhl_test_case_main(sii8620->pdata, &sii8620->muic_state, &numbers[1]);

	return size;
}

static CLASS_ATTR(test_case, 0444,
		sii8620_test_case_show, sii8620_test_case_store);

static ssize_t sii8620_hdcp_status(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	int size;
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);

	size = snprintf(buf, 10,"%d", sii8620->pdata->monitor_cmd);
	return size;
}

static CLASS_ATTR(hdcp_status, 0444, sii8620_hdcp_status, NULL);
#endif /* CONFIG_MHL_CONFIG_TOOLS */

#ifdef FORCE_OCBUS_FOR_ECTS
uint8_t mhl_get_version(void)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(sii8620_dev);
	uint8_t ret_val = dev_context->dev_cap_cache.mdc.mhl_version;

	if(0 == dev_context->dev_cap_cache.mdc.mhl_version)
	{
		/* If we come here it means we have not read devcap and VERSION_STAT must have
		   placed the version asynchronously in peer_mhl3_version */
		ret_val = dev_context->peer_mhl3_version;
	}

	return ret_val < CONFIG_MHL_VERSION ? ret_val : CONFIG_MHL_VERSION;
}

static ssize_t sii8620_test_v2_test_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	MHL_TX_DBG_INFO("force_ocbus_for_ects = %d mhl version = %d\n",
		force_ocbus_for_ects, mhl_get_version()/0x10);
	if (force_ocbus_for_ects)
		return sprintf(buf, "%d\n", 2);
	else
		return sprintf(buf, "%d\n", mhl_get_version()/0x10);
}

static ssize_t sii8620_test_v2_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	if (buf[0] == '1')
		force_ocbus_for_ects = 1;
	else
		force_ocbus_for_ects = 0;

	pr_info("sii8620 : force_ocbus_for_ects = %d\n", force_ocbus_for_ects);

	return size;
}

static CLASS_ATTR(test_v2, 0664,
		sii8620_test_v2_test_show, sii8620_test_v2_test_store);
#endif

#ifdef CONFIG_MHL3_SEC_FEATURE
static ssize_t sii8620_connection_test_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	int status;

	status = sii8620->muic_state;
	pr_info("sii8620 : %s muic_status : %d\n", __func__, status);
	return scnprintf(buf, PAGE_SIZE, "%d", status);
}
static ssize_t sii8620_connection_test_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	int status, ret;
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);

	if (sii8620->muic_state == MHL_ATTACHED) {
		ret = kstrtoint(buf, 10, &status);
		if (unlikely(ret < 0))
			return size;

		pr_info("sii8620 : %s MHL connection change: status %d\n", __func__, status);
		sii8620_detection_start(status);
	} else
		pr_info("sii8620 : MHL is not connected\n");

	return size;
}

static CLASS_ATTR(connection, 0664,
		sii8620_connection_test_show, sii8620_connection_test_store);
#endif /* CONFIG_MHL3_SEC_FEATURE */

#ifdef CONFIG_TESTONLY_SYSFS_SW_REG_TUNING
static ssize_t sii8620_reg_tuning_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	int i;
	char str_buf[] = "ex) echo 0x9a,0x32,0x02 > reg_tuning\n"
		"   -> page = 0x9a, offset = 0x32, value = 0x02\n"
		"current value list :\n";

	buf = strcat(buf, str_buf);

	for (i=0; i < sii8620->pdata->m_cnt; i++) {
		sprintf(str_buf, "[%d] change_list_value : 0x%02x x 0x%02x = 0x%02x\n",
				i, sii8620->pdata->m_page[i], sii8620->pdata->m_offset[i],
				sii8620->pdata->m_value[i]);
		buf = strcat(buf, str_buf);
	}

	return strlen(buf) + 1;
}

static ssize_t sii8620_reg_tuning_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	int numbers[4];

	if (buf[0] == 'x') {
		sii8620->pdata->m_cnt = 0;
	} else {
		get_options(buf, 4, numbers);
		if (numbers[0] == 3)
		{
			sii8620->pdata->m_page[sii8620->pdata->m_cnt] = numbers[1];
			sii8620->pdata->m_offset[sii8620->pdata->m_cnt] = numbers[2];
			sii8620->pdata->m_value[sii8620->pdata->m_cnt] = numbers[3];

			sii8620->pdata->m_cnt++;
			if(sii8620->pdata->m_cnt >= MAX_ELEC_TUNING_CNT)
				sii8620->pdata->m_cnt = MAX_ELEC_TUNING_CNT -1;
		}
	}
	return size;
}


static CLASS_ATTR(reg_tuning, 0664,
		sii8620_reg_tuning_show, sii8620_reg_tuning_store);

static ssize_t sii8620_reg_set_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	u8 page = (u8)(sii8620->pdata->m_reg_addr >> 8);
	u8 offset = (u8)sii8620->pdata->m_reg_addr;
	int reg_value;

	if (sii8620->pdata->m_reg_addr == 0) {
		sprintf(buf, "[error] page:0x%02x, offset:0x%02x", page, offset);
	} else {
		reg_value = mhl_tx_read_reg(sii8620, sii8620->pdata->m_reg_addr);
		sprintf(buf, "page:0x%02x, offset:0x%02x, value:0x%02x", page, offset, reg_value);
	}
	return strlen(buf);
}

static ssize_t sii8620_reg_set_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct mhl_dev_context *sii8620 = dev_get_drvdata(sii8620_dev);
	int numbers[4];

	get_options(buf, 4, numbers);
	if (numbers[0] == 3)
	{
		pr_info("[sii8620] write page:%02x offset:%02x data:%02x\n",
				numbers[1], numbers[2], numbers[3]);
		mhl_tx_write_reg_elc(sii8620, numbers[1], numbers[2], numbers[3]);
	} else if (numbers[0] == 2) {
		pr_info("[sii8620] read page:0x%02x offset:0x%02x\n", numbers[1], numbers[2]);
		sii8620->pdata->m_reg_addr = (numbers[1] << 8) |  numbers[2];
	}

	return size;
}

static CLASS_ATTR(reg_set, 0664,
		sii8620_reg_set_show, sii8620_reg_set_store);

#endif /* CONFIG_TESTONLY_SYSFS_SW_REG_TUNING */

int mhl_tx_init(struct mhl_drv_info const *drv_info, struct device *parent_dev)
{
	struct mhl_dev_context *dev_context;
	int ret;

	pr_info("sii8620 : %s:%d: MHL transmitter init start\n", __func__, __LINE__);
	if (drv_info == NULL || parent_dev == NULL) {
		pr_err("sii8620:%s:%d: Null parameter passed to mhl_tx_init\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (drv_info->mhl_device_isr == NULL || drv_info->irq == 0) {
		dev_err(parent_dev, "sii8620:%s:%d: No IRQ specified!\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_context = kzalloc(sizeof(*dev_context) + drv_info->drv_context_size,
			      GFP_KERNEL);
	if (!dev_context) {
		dev_err(parent_dev, "sii8620:%s:%d: failed to allocate driver data\n", __func__, __LINE__);
		return -ENOMEM;
	}

	dev_context->signature = MHL_DEV_CONTEXT_SIGNATURE;
	dev_context->drv_info = drv_info;
#ifdef CONFIG_MHL3_SEC_FEATURE
	dev_context->pdata = parent_dev->platform_data;
	dev_set_drvdata(parent_dev, dev_context);
	sii8620_dev = parent_dev;
	/* dev_context->pdata->power(true); */
#endif

	sema_init(&dev_context->isr_lock, 1);
	INIT_LIST_HEAD(&dev_context->timer_list);
	dev_context->timer_work_queue = create_workqueue(MHL_DRIVER_NAME);
	if (dev_context->timer_work_queue == NULL) {
		ret = -ENOMEM;
		goto free_mem;
	}

	if (mhl_class == NULL) {
		mhl_class = class_create(THIS_MODULE, "mhl");
		if (IS_ERR(mhl_class)) {
			ret = PTR_ERR(mhl_class);
			pr_info("sii8620 : %s:%d: class_create failed %d\n",  __func__, __LINE__, ret);
			goto err_exit;
		}

		mhl_class->dev_attrs = driver_attribs;

		ret = alloc_chrdev_region(&dev_num,
			0, MHL_DRIVER_MINOR_MAX, MHL_DRIVER_NAME);

		if (ret) {
			pr_info("sii8620 : %s:%d: register_chrdev %s failed, error code: %d\n",
				__func__, __LINE__,
				MHL_DRIVER_NAME, ret);
			goto free_class;
		}

		cdev_init(&dev_context->mhl_cdev, &mhl_fops);
		dev_context->mhl_cdev.owner = THIS_MODULE;
		ret =
		    cdev_add(&dev_context->mhl_cdev, MINOR(dev_num),
			     MHL_DRIVER_MINOR_MAX);
		if (ret) {
			pr_info("cdev_add %s failed %d\n", MHL_DRIVER_NAME,
				ret);
			goto free_chrdev;
		}
#ifdef CONFIG_MHL3_SEC_FEATURE
#ifdef CONFIG_SS_FACTORY
		ret = class_create_file(mhl_class, &class_attr_test_result);
		if (ret)
			pr_err("sii8620:%s:%d: failed to create device file in sysfs entries!\n",
				__func__, __LINE__);
#endif
		ret = class_create_file(mhl_class, &class_attr_connection);
		if (ret)
			MHL_TX_DBG_ERR(": failed to create connection sysfs file\n");

#ifdef CONFIG_MHL_CONFIG_TOOLS
		ret = class_create_file(mhl_class, &class_attr_swing_v2);
		if (ret)
			pr_err("sii8620:%s:%d: failed to create swing_2 sysfs file\n",
				__func__, __LINE__);

		ret = class_create_file(mhl_class, &class_attr_swing_v3);
		if (ret)
			pr_err("sii8620:%s:%d: failed to create swing_3 sysfs file\n",
				__func__, __LINE__);

		ret = class_create_file(mhl_class, &class_attr_timing);
		if (ret)
			pr_err("sii8620:%s:%d: failed to create timing sysfs file\n",
				__func__, __LINE__);
		ret = class_create_file(mhl_class, &class_attr_test_case);
		if (ret)
			pr_err("sii8620:%s:%d: failed to create test_case sysfs file\n",
					__func__, __LINE__);
		ret = class_create_file(mhl_class, &class_attr_hdcp_status);
		if (ret)
			MHL_TX_DBG_ERR(": failed to create hdcp_status sysfs file\n",
					__func__, __LINE__);
#endif
#ifdef FORCE_OCBUS_FOR_ECTS
		ret = class_create_file(mhl_class, &class_attr_test_v2);
		if (ret)
			pr_err("sii8620:%s:%d: failed to create test_v2 sysfs file\n",
				__func__, __LINE__);
#endif
#ifdef CONFIG_TESTONLY_SYSFS_SW_REG_TUNING
		ret = class_create_file(mhl_class, &class_attr_reg_tuning);
		if (ret)
			pr_err("sii8620:%s:%d: failed to create reg_tuning sysfs file\n",
				__func__, __LINE__);

		ret = class_create_file(mhl_class, &class_attr_reg_set);
		if (ret)
			pr_err("sii8620:%s:%d: failed to create reg_set sysfs file\n",
					__func__, __LINE__);
#endif
#endif /* CONFIG_MHL3_SEC_FEATURE */

	}

	dev_context->mhl_dev = device_create(mhl_class, parent_dev,
					     dev_num, dev_context,
					     "%s", MHL_DEVICE_NAME);
	if (IS_ERR(dev_context->mhl_dev)) {
		ret = PTR_ERR(dev_context->mhl_dev);
		pr_info("sii8620 : %s:%d: device_create failed %s %d\n",  __func__, __LINE__,
				MHL_DEVICE_NAME, ret);
		goto free_cdev;
	}
	{
		int status = 0;
		status =
		    sysfs_create_group(&dev_context->mhl_dev->kobj,
					&reg_access_attribute_group);
		if (status) {
			MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
				       status);
		}
		status =
		    sysfs_create_group(&dev_context->mhl_dev->kobj,
				       &rap_attribute_group);
		if (status) {
			MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
				       status);
		}
		status =
		    sysfs_create_group(&dev_context->mhl_dev->kobj,
				       &rbp_attribute_group);
		if (status) {
			MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
				       status);
		}
		status =
		    sysfs_create_group(&dev_context->mhl_dev->kobj,
				       &rcp_attribute_group);
		if (status) {
			MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
				       status);
		}
		status =
		    sysfs_create_group(&dev_context->mhl_dev->kobj,
				       &ucp_attribute_group);
		if (status) {
			MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
				       status);
		}
		status =
		    sysfs_create_group(&dev_context->mhl_dev->kobj,
				       &devcap_attribute_group);
		if (status) {
			MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
				       status);
		}
		status =
		    sysfs_create_group(&dev_context->mhl_dev->kobj,
				       &hdcp_attribute_group);
		if (status) {
			MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
				       status);
		}
		status =
		    sysfs_create_group(&dev_context->mhl_dev->kobj,
				       &vc_attribute_group);
		if (status) {
			MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
				       status);
		}
	}

	ret = request_threaded_irq(drv_info->irq, NULL, mhl_irq_handler,
#ifdef CONFIG_MHL3_SEC_FEATURE
				   IRQF_TRIGGER_HIGH
#else
				   IRQF_TRIGGER_LOW
#endif
				   | IRQF_ONESHOT,
				   MHL_DEVICE_NAME, dev_context);
	if (ret < 0) {
		dev_err(parent_dev, "sii8620:%s:%d: request_threaded_irq failed, status: %d\n",
			__func__, __LINE__, ret);
		goto free_device;
	}

#ifdef CONFIG_MHL3_SEC_FEATURE
	disable_irq(drv_info->irq);
#endif

	/* Initialize the MHL transmitter hardware. */
	ret = down_interruptible(&dev_context->isr_lock);
	if (ret) {
		dev_err(parent_dev, "sii8620:%s:%d: failed to acquire ISR semaphore, status: %d\n", 
			__func__, __LINE__, ret);
		goto free_irq_handler;
	}

	/* Initialize EDID parser module */
	dev_context->edid_parser_context =
		si_edid_create_context(dev_context,
			(struct drv_hw_context *)&dev_context->drv_context);
	pr_info("sii8620 : %s:%d: Initialize EDID parser module\n", __func__, __LINE__);
	rcp_input_dev_one_time_init(dev_context);
#ifndef CONFIG_MHL3_SEC_FEATURE
	ret = si_mhl_tx_initialize(dev_context);
#endif
	up(&dev_context->isr_lock);

#ifndef CONFIG_MHL3_SEC_FEATURE
	if (ret)
		goto free_edid_context;

	MHL_TX_DBG_INFO("MHL transmitter successfully initialized\n");
	dev_set_drvdata(parent_dev, dev_context);
#endif
#ifdef CONFIG_MHL3_SEC_FEATURE
	if (input_dev_rcp)
		init_rcp_input_dev(dev_context);

	dev_context->mhl_event_switch.name = "mhl_event_switch";
	ret = switch_dev_register(&dev_context->mhl_event_switch);
	if (ret < 0)
		pr_err("sii8620:%s:%d mhl_event_switch register is fail\n", __func__, __LINE__);

	wake_lock_init(&dev_context->mhl_wake_lock,
					WAKE_LOCK_SUSPEND, "mhl_wake_lock");
#endif


#ifdef CONFIG_MUIC_NOTIFIER
	INIT_DELAYED_WORK(&dev_context->notifier_register_work, sii8620_notifier_delay_work);
	queue_delayed_work(system_nrt_wq,
		&dev_context->notifier_register_work, msecs_to_jiffies(15000));
	/* change timer 15000 -> 5000 after booting stability */
#else
	dev_context->mhl_nb.notifier_call = sii8620_detection_callback;
	/* acc_register_notifier(&dev_context->mhl_nb); */
#endif /* CONFIG_MUIC_NOTIFIER */

	pr_info("sii8620 : %s:%d: MHL transmitter successfully initialized\n", __func__, __LINE__);

	return ret;

#ifndef CONFIG_MHL3_SEC_FEATURE
free_edid_context:
	MHL_TX_DBG_INFO("%sMHL transmitter initialization failed%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	ret = down_interruptible(&dev_context->isr_lock);
	if (ret) {
		dev_err(parent_dev,
			"Failed to acquire ISR semaphore, "
			"could not destroy EDID context. Status: %d\n", ret);
		goto free_irq_handler;
	}
	if (dev_context->edid_parser_context)
		si_edid_destroy_context(dev_context->edid_parser_context);
	up(&dev_context->isr_lock);
#endif

free_irq_handler:
	if (dev_context->client)
		free_irq(dev_context->client->irq, dev_context);

free_device:
	device_destroy(mhl_class, dev_num);

free_cdev:
	cdev_del(&dev_context->mhl_cdev);

free_chrdev:
	unregister_chrdev_region(dev_num, MHL_DRIVER_MINOR_MAX);
	dev_num = 0;

free_class:
	class_destroy(mhl_class);

err_exit:
	destroy_workqueue(dev_context->timer_work_queue);

free_mem:
	kfree(dev_context);

	return ret;
}

int mhl_tx_remove(struct device *parent_dev)
{
	struct mhl_dev_context *dev_context;
	int ret = 0;

	dev_context = dev_get_drvdata(parent_dev);

	if (dev_context != NULL) {
		MHL_TX_DBG_INFO("%x\n", dev_context);
		ret = down_interruptible(&dev_context->isr_lock);

		dev_context->dev_flags |= DEV_FLAG_SHUTDOWN;

		mhl3_hid_remove_all(dev_context);

		ret = si_mhl_tx_shutdown(dev_context);

		mhl_tx_destroy_timer_support(dev_context);

		up(&dev_context->isr_lock);

		free_irq(dev_context->drv_info->irq, dev_context);

		sysfs_remove_group(&dev_context->mhl_dev->kobj,
				   &reg_access_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
				   &rap_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
				   &rbp_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
				   &rcp_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
				   &ucp_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
				   &devcap_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
				   &hdcp_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
				   &vc_attribute_group);

		device_destroy(mhl_class, dev_num);

		cdev_del(&dev_context->mhl_cdev);

		unregister_chrdev_region(dev_num, MHL_DRIVER_MINOR_MAX);
		dev_num = 0;

		class_destroy(mhl_class);
		mhl_class = NULL;

#ifdef MEDIA_DATA_TUNNEL_SUPPORT
		mdt_destroy(dev_context);
#endif
		destroy_rcp_input_dev(dev_context);

		si_edid_destroy_context(dev_context->edid_parser_context);

		kfree(dev_context);

		MHL_TX_DBG_INFO("%x\n", dev_context);
	}
	return ret;
}

static void mhl_tx_destroy_timer_support(struct mhl_dev_context *dev_context)
{
	struct timer_obj *mhl_timer;

	/*
	 * Make sure all outstanding timer objects are canceled and the
	 * memory allocated for them is freed.
	 */
	while (!list_empty(&dev_context->timer_list)) {
		mhl_timer = list_first_entry(&dev_context->timer_list,
					     struct timer_obj, list_link);
		hrtimer_cancel(&mhl_timer->hr_timer);
		list_del(&mhl_timer->list_link);
		kfree(mhl_timer);
	}

	destroy_workqueue(dev_context->timer_work_queue);
	dev_context->timer_work_queue = NULL;
}

static void mhl_tx_timer_work_handler(struct work_struct *work)
{
	struct timer_obj *mhl_timer;

	mhl_timer = container_of(work, struct timer_obj, work_item);

	mhl_timer->flags |= TIMER_OBJ_FLAG_WORK_IP;
	if (!down_interruptible(&mhl_timer->dev_context->isr_lock)) {

		mhl_timer->timer_callback_handler(mhl_timer->callback_param);

		up(&mhl_timer->dev_context->isr_lock);
	}
	mhl_timer->flags &= ~TIMER_OBJ_FLAG_WORK_IP;

	if (mhl_timer->flags & TIMER_OBJ_FLAG_DEL_REQ) {
		/*
		 * Deletion of this timer was requested during the execution of
		 * the callback handler so go ahead and delete it now.
		 */
		kfree(mhl_timer);
	}
}

static enum hrtimer_restart mhl_tx_timer_handler(struct hrtimer *timer)
{
	struct timer_obj *mhl_timer;

	mhl_timer = container_of(timer, struct timer_obj, hr_timer);

	queue_work(mhl_timer->dev_context->timer_work_queue,
		   &mhl_timer->work_item);

	return HRTIMER_NORESTART;
}

static int is_timer_handle_valid(struct mhl_dev_context *dev_context,
				 void *timer_handle)
{
	struct timer_obj *timer = timer_handle; /* Set to avoid lint warning. */

	list_for_each_entry(timer, &dev_context->timer_list, list_link) {
		if (timer == timer_handle)
			break;
	}

	if (timer != timer_handle) {
		MHL_TX_DBG_WARN("Invalid timer handle %p received\n",
				timer_handle);
		return -EINVAL;
	}
	return 0;
}

int mhl_tx_create_timer(void *context,
			void (*callback_handler) (void *callback_param),
			void *callback_param, void **timer_handle)
{
	struct mhl_dev_context *dev_context;
	struct timer_obj *new_timer;

	dev_context = get_mhl_device_context(context);

	if (callback_handler == NULL)
		return -EINVAL;

	if (dev_context->timer_work_queue == NULL)
		return -ENOMEM;

	new_timer = kmalloc(sizeof(*new_timer), GFP_KERNEL);
	if (new_timer == NULL)
		return -ENOMEM;

	new_timer->timer_callback_handler = callback_handler;
	new_timer->callback_param = callback_param;
	new_timer->flags = 0;

	new_timer->dev_context = dev_context;
	INIT_WORK(&new_timer->work_item, mhl_tx_timer_work_handler);

	list_add(&new_timer->list_link, &dev_context->timer_list);

	hrtimer_init(&new_timer->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	new_timer->hr_timer.function = mhl_tx_timer_handler;
	*timer_handle = new_timer;
	return 0;
}

int mhl_tx_delete_timer(void *context, void **timer_handle)
{
	struct mhl_dev_context *dev_context;
	struct timer_obj *timer;
	int status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, *timer_handle);
	if (status == 0) {
		timer = *timer_handle;

		list_del(&timer->list_link);

		hrtimer_cancel(&timer->hr_timer);

		if (timer->flags & TIMER_OBJ_FLAG_WORK_IP) {
			/*
			 * Request to delete timer object came from within the
			 * timer's callback handler. If we were to proceed with
			 * the timer deletion we would deadlock at
			 * cancel_work_sync(). Instead, just flag that the user
			 * wants the timer deleted. Later when the timer
			 * callback completes the timer's work handler will
			 * complete the process of deleting this timer.
			 */
			timer->flags |= TIMER_OBJ_FLAG_DEL_REQ;
		} else {
			cancel_work_sync(&timer->work_item);
			*timer_handle = NULL;
			kfree(timer);
		}
	}

	return status;
}

int mhl_tx_start_timer(void *context, void *timer_handle, uint32_t time_msec)
{
	struct mhl_dev_context *dev_context;
	struct timer_obj *timer;
	ktime_t timer_period;
	int status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, timer_handle);
	if (status == 0) {
		long secs = 0;
		timer = timer_handle;

		secs = time_msec / 1000;
		time_msec %= 1000;
		timer_period = ktime_set(secs, MSEC_TO_NSEC(time_msec));
		hrtimer_start(&timer->hr_timer, timer_period, HRTIMER_MODE_REL);
	}

	return status;
}

int mhl_tx_stop_timer(void *context, void *timer_handle)
{
	struct mhl_dev_context *dev_context;
	struct timer_obj *timer;
	int status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, timer_handle);
	if (status == 0) {
		timer = timer_handle;
		hrtimer_cancel(&timer->hr_timer);
	}
	return status;
}
