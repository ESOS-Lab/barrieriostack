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
#include <linux/moduleparam.h>
#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/spi/spi.h>

#ifdef CONFIG_MHL3_SEC_FEATURE
#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/power_supply.h>
#include <linux/battery/sec_charger.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_batt.h>
#endif /* CONFIG_MHL3_SEC_FEATURE */

#include "si_fw_macros.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl_defs.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include <linux/input.h>
#include "si_mdt_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "mhl_supp.h"
#include "platform.h"
#include "si_mhl_callback_api.h"
#include "si_8620_drv.h"
#include "si_8620_regs.h"

#ifdef CONFIG_MHL3_SEC_FEATURE
#define MHL_LDO1_8 "VCC_1.8V_MHL"
#define MHL_LDO1_0 "VSIL_1.0V"

static bool mhl_power_on;
#endif /* CONFIG_MHL3_SEC_FEATURE */

int si_mhl_tx_get_num_block_reqs(void);	/*from mhl_supp.c */

struct semaphore platform_lock;
static uint32_t platform_flags;
bool probe_fail;

struct spi_device *spi_dev;
#define SPI_BUS_NUM		1
#define	SPI_CHIP_SEL		0
#define SPI_TRANSFER_MODE	SPI_MODE_0
#define SPI_BUS_SPEED		1000000

#define MAX_SPI_PAYLOAD_SIZE		256
#define MAX_SPI_CMD_SIZE		3
#define EMSC_WRITE_SPI_CMD_SIZE		1
#define EMSC_READ_SPI_CMD_SIZE		1
#define MAX_SPI_DUMMY_XFER_BYTES	20
#define MAX_SPI_XFER_BUFFER_SIZE	(MAX_SPI_CMD_SIZE + \
		MAX_SPI_DUMMY_XFER_BYTES + MAX_SPI_PAYLOAD_SIZE)
#define MAX_SPI_EMSC_BLOCK_SIZE (MAX_SPI_CMD_SIZE + MAX_SPI_PAYLOAD_SIZE)

#define MAX_I2C_PAYLOAD_SIZE		256
#define MAX_I2C_CMD_SIZE		0

#define MAX_I2C_EMSC_BLOCK_SIZE (MAX_I2C_CMD_SIZE + MAX_I2C_PAYLOAD_SIZE)

struct spi_xfer_mem {
	u8 *tx_buf;
	u8 *rx_buf;
	/* block commands are asynchronous to normal cbus traffic
	   and CANNOT share a buffer.
	 */
	uint8_t *block_tx_buffers;
	struct spi_transfer spi_xfer[2];
	struct spi_message spi_cmd;
} spi_mem;

struct i2c_xfer_mem {
	uint8_t *block_tx_buffers;
} i2c_mem;

static struct i2c_adapter *i2c_bus_adapter;

struct i2c_dev_info {
	uint8_t dev_addr;
	struct i2c_client *client;
};

#define I2C_DEV_INFO(addr) \
	{.dev_addr = addr >> 1, .client = NULL}

static struct i2c_dev_info device_addresses[] = {
	I2C_DEV_INFO(SA_TX_PAGE_0),
	I2C_DEV_INFO(SA_TX_PAGE_1),
	I2C_DEV_INFO(SA_TX_PAGE_2),
	I2C_DEV_INFO(SA_TX_PAGE_3),
	I2C_DEV_INFO(SA_TX_PAGE_6),
	I2C_DEV_INFO(SA_TX_CBUS),
	I2C_DEV_INFO(SA_TX_PAGE_7),
	I2C_DEV_INFO(SA_TX_PAGE_4),
};

int debug_level;
bool debug_reg_dump;
bool input_dev_rap = 1;
bool input_dev_rcp = 1;
bool input_dev_ucp = 1;
bool input_dev_rbp = 1;
int hdcp_content_type;
#ifdef CONFIG_MHL_SPI
bool use_spi = 1;
#else
bool use_spi = 0;
#endif
int	crystal_khz = 24000; /* S.LSI model uses a 24mhz-crystal-clock. */
int	use_heartbeat = 0;
bool wait_for_user_intr = 0;
int	tmds_link_speed = 0;

bool continue_to_ecbus = 1;
#ifdef FORCE_OCBUS_FOR_ECTS
#ifdef CONFIG_MHL3_DVI_WR
int force_ocbus_for_ects;
#else
bool force_ocbus_for_ects;
#endif
#endif
int gpio_index = 138;

module_param(debug_reg_dump, bool, S_IRUGO);
module_param(debug_level, int, S_IRUGO);

module_param(input_dev_rap, bool, S_IRUGO);
module_param(input_dev_rcp, bool, S_IRUGO);
module_param(input_dev_ucp, bool, S_IRUGO);
module_param(input_dev_rbp, bool, S_IRUGO);
module_param(hdcp_content_type, int, S_IRUGO);
module_param(use_spi, bool, S_IRUGO);
module_param(crystal_khz, int, S_IRUGO);
module_param(use_heartbeat, int, S_IRUGO);
module_param(wait_for_user_intr, bool, S_IRUGO);
module_param(tmds_link_speed, int, S_IRUGO);
module_param(continue_to_ecbus, bool, S_IRUGO);
#ifdef	FORCE_OCBUS_FOR_ECTS
#ifdef CONFIG_MHL3_DVI_WR
module_param(force_ocbus_for_ects, int, S_IRUGO);
#else
module_param(force_ocbus_for_ects, bool, S_IRUGO);
#endif
#endif

module_param_named(debug_msgs, debug_level, int, S_IRUGO);


static inline int platform_read_i2c_block(struct i2c_adapter *i2c_bus, u8 page,
	u8 offset, u16 count, u8 *values)
{
	struct i2c_msg msg[2];

	msg[0].flags = 0;
	msg[0].addr = page >> 1;
	msg[0].buf = &offset;
	msg[0].len = 1;

	msg[1].flags = I2C_M_RD;
	msg[1].addr = page >> 1;
	msg[1].buf = values;
	msg[1].len = count;

	return i2c_transfer(i2c_bus, msg, 2);
}

static inline int platform_write_i2c_block(struct i2c_adapter *i2c_bus, u8 page,
	u8 offset, u16 count, u8 *values)
{
	struct i2c_msg msg;
	u8 *buffer;
	int ret;

	buffer = kmalloc(count + 1, GFP_KERNEL);
	if (!buffer) {
		printk(KERN_ERR "%s:%d buffer allocation failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	buffer[0] = offset;
	memmove(&buffer[1], values, count);

	msg.flags = 0;
	msg.addr = page >> 1;
	msg.buf = buffer;
	msg.len = count + 1;

	ret = i2c_transfer(i2c_bus, &msg, 1);

	kfree(buffer);

	if (ret != 1) {
		printk(KERN_ERR "%s:%d I2c write failed 0x%02x:0x%02x\n",
			__func__, __LINE__, page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

#ifndef CONFIG_MHL3_SEC_FEATURE
static int platform_read_i2c_reg(struct i2c_adapter *bus_adapter_i2c, u8 page,
	u8 offset)
{
	int ret;
	u8 byte_read;

	ret = platform_read_i2c_block(bus_adapter_i2c, page, offset, 1,
		&byte_read);
	MHL_TX_DBG_INFO("\tGI2C_R %2x:%2x = %2x\n", page, offset, ret);
	if (ret != 2) {
		printk(KERN_ERR "%s:%d I2c read failed, 0x%02x:0x%02x\n",
			__func__, __LINE__, page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}
	return ret ? ret : byte_read;
}

static int platform_write_i2c_reg(struct i2c_adapter *bus_adapter_i2c, u8 page,
	u8 offset, u8 value)
{
	MHL_TX_DBG_INFO("\tGI2C_W %2x:%2x <- %2x\n", page, offset, value);
	return platform_write_i2c_block(bus_adapter_i2c, page, offset, 1,
					&value);
}
#endif /* CONFIG_MHL3_SEC_FEATURE */

uint32_t platform_get_flags(void)
{
	return platform_flags;
}

/*
 * since we've agreed that the interrupt pin will never move
 *  we've special cased it for performance reasons.
 * This is why there is no set_pin() index for it.
 */
int is_interrupt_asserted(void)
{
#ifdef CONFIG_MHL_SPI
	struct device *parent_dev = &spi_dev->dev;
#else
	struct device *parent_dev = &device_addresses[0].client->dev;
#endif
	struct mhl_dev_context *sii8620 = dev_get_drvdata(parent_dev);
	struct sii8620_platform_data *pdata = sii8620->pdata;
	return (gpio_get_value(pdata->gpio_mhl_irq) ? 1 : 0);
}

#ifndef CONFIG_MHL3_SEC_FEATURE
int get_config(void *dev_context, int config_idx)
{
	int pin_state = 0;

	if (config_idx < ARRAY_SIZE(platform_signals)) {
		switch (platform_signals[config_idx].gpio_number) {
		case GET_FROM_MODULE_PARAM:
			pin_state = *(platform_signals[config_idx].param);
			break;
		case GPIO_ON_EXPANDER:
			pin_state =
			    (platform_read_i2c_reg
			     (i2c_bus_adapter,
			      platform_signals[config_idx].gpio_reg_PCA950x.
			      slave_addr,
			      platform_signals[config_idx].gpio_reg_PCA950x.
			      offset)
			     & platform_signals[config_idx].
			     gpio_mask_PCA950x) ? 1 : 0;
			break;
		default:
			pin_state =
				gpio_get_value(platform_signals[config_idx].
				gpio_number);
			break;
		}
	}
	return pin_state;
}
#endif /* CONFIG_MHL3_SEC_FEATURE */

void set_pin_impl(int pin_idx, int value,
		  const char *function_name, int line_num)
{
#ifndef CONFIG_MHL3_SEC_FEATURE
	uint8_t bank_value;

	if (pin_idx < ARRAY_SIZE(platform_signals)) {

		MHL_TX_DBG_INFO("set_pin(%s,%d)\n",
				platform_signals[pin_idx].name, value);
		switch (platform_signals[pin_idx].gpio_number) {
		case GET_FROM_MODULE_PARAM:
			break;
		case GPIO_ON_EXPANDER:
			bank_value =
			    *(platform_signals[pin_idx].gpio_bank_value);
			if (value)
				bank_value |=
				    platform_signals[pin_idx].gpio_mask_PCA950x;
			else
				bank_value &= ~platform_signals[pin_idx].
					gpio_mask_PCA950x;

			*(platform_signals[pin_idx].gpio_bank_value) =
				bank_value;
			platform_write_i2c_reg(i2c_bus_adapter,
					       platform_signals[pin_idx].
					       gpio_reg_PCA950x.slave_addr,
					       platform_signals[pin_idx].
					       gpio_reg_PCA950x.offset,
					       bank_value);
			break;
		default:
			gpio_set_value(platform_signals[pin_idx].gpio_number,
				value);
			break;
		}
	}
#endif /* CONFIG_MHL3_SEC_FEATURE */
}

void platform_mhl_tx_hw_reset(uint32_t reset_period, uint32_t reset_delay)
{
#ifdef CONFIG_MHL_SPI
	struct device *parent_dev = &spi_dev->dev;
#else
	struct device *parent_dev = &device_addresses[0].client->dev;
#endif
	struct mhl_dev_context *sii8620 = dev_get_drvdata(parent_dev);
	struct sii8620_platform_data *pdata = sii8620->pdata;

	pdata->hw_reset();

	if (reset_delay)
		msleep(reset_delay);

	if (use_spi) {
		u8 cmd = spi_op_enable;
		spi_write(spi_dev, &cmd, 1);
	}
}

void mhl_tx_vbus_control(enum vbus_power_state power_state)
{
	struct device *parent_dev;
	struct mhl_dev_context *dev_context;

	if (use_spi)
		parent_dev = &spi_dev->dev;
	else
		parent_dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(parent_dev);

	switch (power_state) {
	case VBUS_OFF:
		/* set_pin(dev_context, M2U_VBUS_CTRL, 0); */
		/* set_pin(dev_context, TX2MHLRX_PWR, 1); */
		break;

	case VBUS_ON:
		/* set_pin(dev_context, TX2MHLRX_PWR, 0); */
		/* set_pin(dev_context, M2U_VBUS_CTRL, 1); */
		break;

	default:
		dev_err(dev_context->mhl_dev,
			"%s: Invalid power state %d received!\n",
			__func__, power_state);
		break;
	}
}

void mhl_tx_vbus_current_ctl(uint16_t max_current_in_milliamps)
{
	/*
		Starter kit does not have a PMIC.
		Implement VBUS input current limit control here.
	*/

}

int si_device_dbg_i2c_reg_xfer(void *dev_context, u8 page, u8 offset,
			       u16 count, bool rw_flag, u8 *buffer)
{
	u16 address = (page << 8) | offset;

	if (rw_flag == DEBUG_I2C_WRITE)
		return mhl_tx_write_reg_block(dev_context, address, count,
					      buffer);
	else
		return mhl_tx_read_reg_block(dev_context, address, count,
					     buffer);
}

#define MAX_DEBUG_MSG_SIZE	1024

#if defined(DEBUG)

/*
 * Return a pointer to the file name part of the
 * passed path spec string.
 */
char *find_file_name(const char *path_spec)
{
	char *pc;

	for (pc = (char *)&path_spec[strlen(path_spec)];
		pc != path_spec; --pc) {
		if ('\\' == *pc) {
			++pc;
			break;
		}
		if ('/' == *pc) {
			++pc;
			break;
		}
	}
	return pc;
}

void print_formatted_debug_msg(char *file_spec, const char *func_name,
			       int line_num, char *fmt, ...)
{
	uint8_t *msg = NULL;
	uint8_t *msg_offset;
	char *file_spec_sep = NULL;
	int remaining_msg_len = MAX_DEBUG_MSG_SIZE;
	int len;
	va_list ap;

	if (fmt == NULL)
		return;

	msg = kmalloc(remaining_msg_len, GFP_KERNEL);
	if (msg == NULL)
		return;

	msg_offset = msg;

	len = scnprintf(msg_offset, remaining_msg_len, "sii8620 : ");
	msg_offset += len;
	remaining_msg_len -= len;

	/* Only print the file name, not the path */
	if (file_spec != NULL)
		file_spec = find_file_name(file_spec);

	if (file_spec != NULL) {
		if (func_name != NULL)
			file_spec_sep = "->";
		else if (line_num != -1)
			file_spec_sep = ":";
	}

	if (file_spec) {
		len = scnprintf(msg_offset, remaining_msg_len, "%s", file_spec);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (file_spec_sep) {
		len =
		    scnprintf(msg_offset, remaining_msg_len, "%s",
			      file_spec_sep);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (func_name) {
		len = scnprintf(msg_offset, remaining_msg_len, "%s", func_name);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (line_num != -1) {
		if ((file_spec != NULL) || (func_name != NULL))
			len =
			    scnprintf(msg_offset, remaining_msg_len, ":%d ",
				      line_num);
		else
			len =
			    scnprintf(msg_offset, remaining_msg_len, "%d ",
				      line_num);

		msg_offset += len;
		remaining_msg_len -= len;
	}

	va_start(ap, fmt);
	len = vscnprintf(msg_offset, remaining_msg_len, fmt, ap);
	va_end(ap);

	printk(msg);

	kfree(msg);
}

void dump_transfer(enum tx_interface_types if_type,
		   u8 page, u8 offset, u16 count, u8 *values, bool write)
{
	if (debug_reg_dump != 0) {
		int buf_size = 64;
		u16 idx;
		int buf_offset;
		char *buf;
		char *if_type_msg;

		switch (if_type) {
		case TX_INTERFACE_TYPE_I2C:
			if_type_msg = "I2C";
			break;
		case TX_INTERFACE_TYPE_SPI:
			if_type_msg = "SPI";
			break;
		default:
			return;
		};

		if (count > 1) {
			/* 3 chars per byte displayed */
			buf_size += count * 3;
			/* plus per display row overhead */
			buf_size += ((count / 16) + 1) * 8;
		}

		buf = kmalloc(buf_size, GFP_KERNEL);
		if (!buf)
			return;

		if (count == 1) {

			scnprintf(buf, buf_size, "   %s %02X.%02X %s %02X\n",
				  if_type_msg,
				  page, offset, write ? "W" : "R", values[0]);
		} else {
			idx = 0;
			buf_offset =
			    scnprintf(buf, buf_size, "%s %02X.%02X %s(%d)",
				      if_type_msg, page, offset,
				      write ? "W" : "R", count);

			for (idx = 0; idx < count; idx++) {
				if (0 == (idx & 0x0F))
					buf_offset +=
					    scnprintf(&buf[buf_offset],
						      buf_size - buf_offset,
						      "\n%04X: ", idx);

				buf_offset += scnprintf(&buf[buf_offset],
							buf_size - buf_offset,
							"%02X ", values[idx]);
			}
			buf_offset +=
			    scnprintf(&buf[buf_offset], buf_size - buf_offset,
				      "\n");
		}

		print_formatted_debug_msg(NULL, NULL, -1, buf);
		kfree(buf);
	}
}
#endif /* #if defined(DEBUG) */

static struct mhl_drv_info drv_info = {
	.drv_context_size = sizeof(struct drv_hw_context),
	.mhl_device_initialize = si_mhl_tx_chip_initialize,
	.mhl_device_isr = si_mhl_tx_drv_device_isr,
	.mhl_device_dbg_i2c_reg_xfer = si_device_dbg_i2c_reg_xfer,
	.mhl_device_get_aksv = si_mhl_tx_drv_get_aksv
};

int mhl_tx_write_reg_block_i2c(void *drv_context, u8 page, u8 offset,
			       u16 count, u8 *values)
{
	DUMP_I2C_TRANSFER(page, offset, count, values, true);

	return platform_write_i2c_block(i2c_bus_adapter, page, offset, count,
					values);
}

int mhl_tx_write_reg_i2c(void *drv_context, u8 page, u8 offset, u8 value)
{
	return mhl_tx_write_reg_block_i2c(drv_context, page, offset, 1, &value);
}

int mhl_tx_read_reg_block_i2c(void *drv_context, u8 page, u8 offset,
	u16 count, u8 *values)
{
	int ret;

	if (count == 0) {
		MHL_TX_DBG_ERR("Tried to read 0 bytes\n");
		return -EINVAL;
	}

	ret = platform_read_i2c_block(i2c_bus_adapter, page, offset, count,
		values);
	if (ret != 2) {
		MHL_TX_DBG_ERR("I2c read failed, 0x%02x:0x%02x\n", page,
			offset);
		ret = -EIO;
	} else {
		ret = 0;
		DUMP_I2C_TRANSFER(page, offset, count, values, false);
	}

	return ret;
}

int mhl_tx_read_reg_i2c(void *drv_context, u8 page, u8 offset)
{
	u8 byte_read;
	int status;

	status = mhl_tx_read_reg_block_i2c(drv_context, page, offset,
					   1, &byte_read);

	return status ? status : byte_read;
}

static int i2c_addr_to_spi_cmd(void *drv_context, bool write, u8 *page,
			       u8 *opcode, u8 *dummy_bytes)
{
	if (write) {
		*opcode = spi_op_reg_write;
		*dummy_bytes = 0;
	} else {
		*opcode = spi_op_reg_read;
		*dummy_bytes = 5;
	}

	switch (*page) {
	case SA_TX_PAGE_0:
		*page = 0;
		break;
	case SA_TX_PAGE_1:
		*page = 1;
		break;
	case SA_TX_PAGE_2:
		*page = 2;
		break;
	case SA_TX_PAGE_3:
		*page = 3;
		break;
	case SA_TX_PAGE_4:
		*page = 4;
		break;
	case SA_TX_CBUS:
		*page = 5;
		break;
	case SA_TX_PAGE_6:
		*page = 6;
		break;
	case SA_TX_PAGE_7:
		*page = 7;
		break;
	case SA_TX_PAGE_8:
		*page = 8;
		break;
	default:
		MHL_TX_DBG_ERR("Called with unknown page 0x%02x\n", *page);
		return -EINVAL;
	}
	return 0;
}

inline uint8_t reg_page(uint16_t address)
{
	return (uint8_t)((address >> 8) & 0x00FF);
}

inline uint8_t reg_offset(uint16_t address)
{
	return (uint8_t)(address & 0x00FF);
}

static int mhl_tx_write_reg_block_spi(void *drv_context, u8 page, u8 offset,
	u16 count, u8 *values)
{
	u8 opcode;
	u8 dummy_bytes;
	u16 length = count + 3;
	int ret;

	DUMP_SPI_TRANSFER(page, offset, count, values, true);

	ret = i2c_addr_to_spi_cmd(drv_context, true, &page, &opcode,
				  &dummy_bytes);
	if (ret != 0)
		return ret;

	length = 3 + count + dummy_bytes;

	if (length > MAX_SPI_XFER_BUFFER_SIZE) {
		MHL_TX_DBG_ERR("Transfer count (%d) is too large!\n", count);
		return -EINVAL;
	}

	spi_mem.tx_buf[0] = opcode;
	spi_mem.tx_buf[1] = page;
	spi_mem.tx_buf[2] = offset;
	if (dummy_bytes)
		memset(&spi_mem.tx_buf[3], 0, dummy_bytes);

	memmove(&spi_mem.tx_buf[dummy_bytes + 3], values, count);

	ret = spi_write(spi_dev, spi_mem.tx_buf, length);

	if (ret != 0) {
		MHL_TX_DBG_ERR("SPI write block failed, "
			       "page: 0x%02x, register: 0x%02x\n",
			       page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

static int mhl_tx_write_reg_spi(void *drv_context, u8 page, u8 offset, u8 value)
{
	return mhl_tx_write_reg_block_spi(drv_context, page, offset, 1, &value);
}

static int mhl_tx_read_reg_block_spi(void *drv_context, u8 page, u8 offset,
	u16 count, u8 *values)
{
	u8 page_num = page;
	u8 opcode;
	u8 dummy_bytes;
	u16 length;
	int ret = 0;

	if (count > MAX_SPI_PAYLOAD_SIZE) {
		MHL_TX_DBG_ERR("Requested transfer size is too large\n");
		return -EINVAL;
	}

	ret = i2c_addr_to_spi_cmd(drv_context, false, &page_num, &opcode,
				  &dummy_bytes);
	if (ret != 0)
		return ret;

	if ((reg_page(REG_DDC_DATA) == page) &&
	    (reg_offset(REG_DDC_DATA) == offset)) {
		dummy_bytes = 11;
		opcode = 0x62;
	}

	length = 3 + count + dummy_bytes;
	if (length > MAX_SPI_XFER_BUFFER_SIZE) {
		MHL_TX_DBG_ERR("Requested transfer size is too large\n");
		return -EINVAL;
	}

	spi_message_init(&spi_mem.spi_cmd);
	memset(&spi_mem.spi_xfer, 0, sizeof(spi_mem.spi_xfer));
	spi_mem.tx_buf[0] = opcode;
	spi_mem.tx_buf[1] = page_num;
	spi_mem.tx_buf[2] = offset;

	spi_mem.spi_xfer[0].tx_buf = spi_mem.tx_buf;
	spi_mem.spi_xfer[0].len = 3 + dummy_bytes;
	spi_message_add_tail(&spi_mem.spi_xfer[0], &spi_mem.spi_cmd);

	spi_mem.spi_xfer[1].rx_buf = spi_mem.rx_buf;
	spi_mem.spi_xfer[1].len = count;
#ifdef CONFIG_MHL3_SEC_FEATURE /* spi read-fail issue(QC<->OMAP) */
	spi_mem.spi_xfer[1].cs_change = 0;
#else
	spi_mem.spi_xfer[1].cs_change = 1;
#endif
	spi_message_add_tail(&spi_mem.spi_xfer[1], &spi_mem.spi_cmd);

	ret = spi_sync(spi_dev, &spi_mem.spi_cmd);

	if (ret != 0) {
		MHL_TX_DBG_ERR("SPI read block failed, "
			"page: 0x%02x, register: 0x%02x\n",
			page, offset);
	} else {
		memcpy(values, spi_mem.rx_buf, count);
		DUMP_SPI_TRANSFER(page, offset, count, values, false);
	}

	return ret;
}

int mhl_tx_read_reg_block_spi_emsc(void *drv_context, u16 count, u8 *values)
{
	u8 dummy_bytes = 1;
	u16 length;
	int ret;

	if (count > MAX_SPI_PAYLOAD_SIZE) {
		MHL_TX_DBG_ERR("Requested transfer size is too large\n");
		return -EINVAL;
	}

	length = EMSC_READ_SPI_CMD_SIZE + dummy_bytes + count;
	if (length > MAX_SPI_XFER_BUFFER_SIZE) {
		MHL_TX_DBG_ERR("Requested transfer size is too large\n");
		return -EINVAL;
	}

	spi_message_init(&spi_mem.spi_cmd);
	memset(&spi_mem.spi_xfer, 0, sizeof(spi_mem.spi_xfer));
	spi_mem.tx_buf[0] = spi_op_emsc_read;

	spi_mem.spi_xfer[0].tx_buf = spi_mem.tx_buf;
	spi_mem.spi_xfer[0].len = EMSC_READ_SPI_CMD_SIZE + dummy_bytes;
	spi_message_add_tail(&spi_mem.spi_xfer[0], &spi_mem.spi_cmd);

	spi_mem.spi_xfer[1].rx_buf = spi_mem.rx_buf;
	spi_mem.spi_xfer[1].len = count;
#ifdef CONFIG_MHL3_SEC_FEATURE
	spi_mem.spi_xfer[1].cs_change = 0;
#else
	spi_mem.spi_xfer[1].cs_change = 1;
#endif
	spi_message_add_tail(&spi_mem.spi_xfer[1], &spi_mem.spi_cmd);

	ret = spi_sync(spi_dev, &spi_mem.spi_cmd);

	if (ret != 0) {
		MHL_TX_DBG_ERR("SPI eMSC read block failed ");
	} else {
		memcpy(values, spi_mem.rx_buf, count);
		/* DUMP_SPI_TRANSFER(page, offset, count, values, false); */
	}

	return ret;
}

static int mhl_tx_read_reg_spi(void *drv_context, u8 page, u8 offset)
{
	u8 byte_read = 0;
	int status;

	status = mhl_tx_read_reg_block_spi(drv_context, page, offset, 1,
					   &byte_read);

	return status ? status : byte_read;
}

int mhl_tx_write_reg_block(void *drv_context, u16 address, u16 count,
	u8 *values)
{
	u8 page = (u8)(address >> 8);
	u8 offset = (u8)address;

	if (use_spi)
		return mhl_tx_write_reg_block_spi(drv_context, page, offset,
						  count, values);
	else
		return mhl_tx_write_reg_block_i2c(drv_context, page, offset,
						  count, values);
}

void si_mhl_tx_platform_get_block_buffer_info(struct block_buffer_info_t
					      *block_buffer_info)
{
	if (use_spi) {
		block_buffer_info->buffer = spi_mem.block_tx_buffers;
		block_buffer_info->req_size = MAX_SPI_EMSC_BLOCK_SIZE;
		block_buffer_info->payload_offset = EMSC_WRITE_SPI_CMD_SIZE;
	} else {
		block_buffer_info->buffer = i2c_mem.block_tx_buffers;
		block_buffer_info->req_size = MAX_I2C_EMSC_BLOCK_SIZE;
		block_buffer_info->payload_offset = MAX_I2C_CMD_SIZE;
	}
}

int mhl_tx_write_block_spi_emsc(void *drv_context, struct block_req *req)
{
	u16 length;
	int ret;

	/* DUMP_SPI_TRANSFER(page, offset, req->count, req->payload->as_bytes,
	 * true);
	 */

	/* dummy bytes will always be zero */
	length = EMSC_WRITE_SPI_CMD_SIZE + req->count;

	if (length > MAX_SPI_EMSC_BLOCK_SIZE) {
		MHL_TX_DBG_ERR("Transfer count (%d) is too large!\n",
			       req->count);
		return -EINVAL;
	}

	req->platform_header[0] = spi_op_emsc_write;

	ret = spi_write(spi_dev, req->platform_header, length);

	if (ret != 0) {
		MHL_TX_DBG_ERR("SPI write block failed\n");
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

#ifdef CONFIG_TESTONLY_SYSFS_SW_REG_TUNING
int mhl_tx_write_reg_elc(void *drv_context, u8 page, u8 offset, u8 value)
{
	if (use_spi)
		return mhl_tx_write_reg_spi(drv_context, page, offset, value);
	else
		return mhl_tx_write_reg_i2c(drv_context, page, offset, value);
}
#endif

int mhl_tx_write_reg(void *drv_context, u16 address, u8 value)
{
	u8 page = (u8)(address >> 8);
	u8 offset = (u8)address;

	if (use_spi)
		return mhl_tx_write_reg_spi(drv_context, page, offset, value);
	else
		return mhl_tx_write_reg_i2c(drv_context, page, offset, value);
}

int mhl_tx_read_reg_block(void *drv_context, u16 address, u16 count, u8 *values)
{
	u8 page = (u8)(address >> 8);
	u8 offset = (u8)address;

	if (use_spi)
		return mhl_tx_read_reg_block_spi(drv_context, page, offset,
						 count, values);
	else
		return mhl_tx_read_reg_block_i2c(drv_context, page, offset,
						 count, values);
}

int mhl_tx_read_reg(void *drv_context, u16 address)
{
	u8 page = (u8)(address >> 8);
	u8 offset = (u8)address;

	if (use_spi)
		return mhl_tx_read_reg_spi(drv_context, page, offset);
	else
		return mhl_tx_read_reg_i2c(drv_context, page, offset);
}

int mhl_tx_modify_reg(void *drv_context, u16 address, u8 mask, u8 value)
{
	int reg_value;
	int write_status;

	reg_value = mhl_tx_read_reg(drv_context, address);
	if (reg_value < 0)
		return reg_value;

	reg_value &= ~mask;
	reg_value |= mask & value;

	write_status = mhl_tx_write_reg(drv_context, address, reg_value);

	if (write_status < 0)
		return write_status;
	else
		return reg_value;
}

/*
 * Return a value indicating how upstream HPD is
 * implemented on this platform.
 */
enum hpd_control_mode platform_get_hpd_control_mode(void)
{
	return HPD_CTRL_PUSH_PULL;
}

int si_8620_pm_suspend(struct device *dev)
{
	int status = -EINVAL;

	if (dev == 0)
		goto done;

	status = down_interruptible(&platform_lock);
	if (status)
		goto done;

	status = mhl_handle_power_change_request(dev, false);
	/*
	 * Set MHL/USB switch to USB
	 * NOTE: Switch control is implemented differently on each
	 * version of the starter kit.
	 */
	/* set_pin(dev,X02_USB_SW_CTRL,0); */

	up(&platform_lock);
done:
	return status;
}

int si_8620_pm_resume(struct device *dev)
{
	int status = -EINVAL;

	if (dev == 0)
		goto done;

	status = down_interruptible(&platform_lock);
	if (status)
		goto done;

	/* set_pin(X02_USB_SW_CTRL, 1); */
	status = mhl_handle_power_change_request(dev, true);

	up(&platform_lock);
done:
	return status;
}

int si_8620_power_control(bool power_up)
{
	struct device *dev = NULL;
	int status;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	if (power_up)
		status = si_8620_pm_resume(dev);
	else
		status = si_8620_pm_suspend(dev);
	return status;
}
EXPORT_SYMBOL_GPL(si_8620_power_control);


/* Following code is which having the dependence of the AP. */
#ifdef CONFIG_MHL3_SEC_FEATURE
/*FIXME, need to use more common/proper function
for checking a VBUS regardless of H/W charger IC*/
static void sii8620_charger_mhl_cb(bool otg_enable, int charger)
{
	struct device *parent_dev = &device_addresses[0].client->dev;
	struct mhl_dev_context *sii8620 = dev_get_drvdata(parent_dev);
	struct sii8620_platform_data *pdata = sii8620->pdata;
	union power_supply_propval value;
	int i, ret = 0;
	struct power_supply *psy;
	pdata->charging_type = POWER_SUPPLY_TYPE_MISC;

	if (sii8620->mhl_muic_type == MHL_SMART_DOCK) {
		pr_info("sii8620 : SMART_DOCK connected, mhl_cb do nothing\n");
		return;
	}

	pr_info("sii8620 : %s:%d, otg_en:%d, charger:%d\n", __func__,
		__LINE__, otg_enable, charger);
	if (charger == 0x00) {
		pr_info("sii8620 : USB charger\n");
		pdata->charging_type = POWER_SUPPLY_TYPE_MHL_USB;
	} else if (charger == 0x01) {
		pr_info("sii8620 : TA charger 900mA\n");
		pdata->charging_type = POWER_SUPPLY_TYPE_MHL_900;
	} else if (charger == 0x02) {
		pr_info("sii8620 : TA charger 1500mA\n");
		pdata->charging_type = POWER_SUPPLY_TYPE_MHL_1500;
	} else if (charger == 0x03) {
		pr_info("sii8620 : TA charger 100mA\n");
		pdata->charging_type = POWER_SUPPLY_TYPE_MHL_USB_100;
	} else if (charger == 0x04) {
		pr_info("sii8620 : TA charger 2000mA\n");
		pdata->charging_type = POWER_SUPPLY_TYPE_MHL_2000;
	} else
		pdata->charging_type = POWER_SUPPLY_TYPE_BATTERY;

	if (sii8620->mhl_muic_type == MHL_MM_DOCK) {
		if (otg_enable == true || charger == 0x00) {
			MHL_TX_DBG_ERR("otg_enable = %d  charger = 0x%2x\n", otg_enable, charger);
			return;
		} else if (pdata->charging_type != POWER_SUPPLY_TYPE_BATTERY) {
/* disable MMdock feature */
/*			pdata->charging_type = POWER_SUPPLY_TYPE_MDOCK_TA; */
			MHL_TX_DBG_ERR("POWER_SUPPLY_TYPE_MDOCK_TA\n");
		}
	}

	for (i = 0; i < 10; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}
	if (i == 10) {
		pr_err("sii8620 : %s: fail to get battery ps\n", __func__);
		return;
	}

	if (otg_enable) {
		psy_do_property(pdata->charger_name, get,
					POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_BATTERY ||
				value.intval == POWER_SUPPLY_TYPE_WIRELESS) {
			if (!lpcharge) {
				value.intval = true;
				psy_do_property(pdata->charger_name, set,
						POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
				pdata->charging_type = POWER_SUPPLY_TYPE_OTG;
			}
		}
	} else {
		value.intval = false;
		pr_info("sii8620 : %s Power supply set Battery\n", __func__);
		psy_do_property(pdata->charger_name, set, POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
	}

	value.intval = pdata->charging_type;
	pr_info("sii8620 : %s Power supply set (%d)\n", __func__, value.intval);
	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE,
		&value);
	if (ret)
		pr_err("sii8620 : %s fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);

	if (pdata->charging_type == POWER_SUPPLY_TYPE_OTG)
		msleep(50);

	return;
}

#ifdef CONFIG_SII8620_CHECK_MONITOR
static inline u32 exynos_smc(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
{
#if  0 // have to modify for 64bit.
	register u32 reg0 __asm__("r0") = cmd;
	register u32 reg1 __asm__("r1") = arg1;
	register u32 reg2 __asm__("r2") = arg2;
	register u32 reg3 __asm__("r3") = arg3;
	__asm__ volatile (
#ifdef MC_ARCH_EXTENSION_SEC
	/* This pseudo op is supported and required from
	* binutils 2.21 on */
	".arch_extension sec\n"
#endif
	"smc 0\n"
	: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)

	);
	return reg1;
#else
	return 0;
#endif
}

static void sii8620_link_monitor(u8 cmd_value)
{
	int ret;
	struct device *parent_dev = &device_addresses[0].client->dev;
	struct mhl_dev_context *sii8620 = dev_get_drvdata(parent_dev);
	struct sii8620_platform_data *pdata = sii8620->pdata;

	switch(cmd_value) {
	case(0x01):
		pdata->monitor_cmd = 0x01;
		break;

	case(0x02):
		pdata->monitor_cmd |= 0x02;
		pdata->monitor_cmd &= ~(0x04);
		break;

	case(0x03):
		pdata->monitor_cmd |= 0x04;
		pdata->monitor_cmd &= ~(0x02);
		break;

	case(0x04):
		pdata->monitor_cmd = 0x08;
		break;
	}

	ret = exynos_smc(MC_FC_HDCP_VALUE, 0, pdata->monitor_cmd, 0);
	if (ret != MC_FC_RET_OK)
		pr_err("%s() exynos_smc fail ret = %d\n", __func__, ret);
	else
		pr_info("%s() monitor_cmd = %d\n", __func__, pdata->monitor_cmd);

	if (cmd_value == 0x03) {
		sii8620_link_monitor(0x04);
		sii8620_link_monitor(0x01);
	}
}
#endif

static void of_sii8620_hw_onoff(bool onoff)
{
	struct device *parent_dev = &device_addresses[0].client->dev;
	struct mhl_dev_context *sii8620 = dev_get_drvdata(parent_dev);
	struct sii8620_platform_data *pdata = sii8620->pdata;
	struct regulator *regulator1_8, *regulator1_0;
	int ret = 0;
	pr_info("sii8620 : %s:%d\n",__func__,__LINE__);

	regulator1_8 = regulator_get(NULL, MHL_LDO1_8);
	if (IS_ERR(regulator1_8)) {
		pr_err("%s : regulator 1.8 is not available", __func__);
		return;
	}
	regulator1_0 = regulator_get(NULL, MHL_LDO1_0);
	if (IS_ERR(regulator1_0)) {
		pr_err("%s : regulator 1.0 is not available", __func__);
		goto err_exit0;
	}

	if (mhl_power_on == onoff) {
		pr_info("sii8260 : sii8260_power_onoff : alread %d\n", onoff);
		regulator_put(regulator1_0);
		regulator_put(regulator1_8);
		return;
	}

	mhl_power_on = onoff;
	pr_info("sii8620 : sii8620_power_onoff : %d\n", onoff);

	if (onoff) {
		/* To avoid floating state of the HPD pin *
		 * in the absence of external pull-up     */
		ret = regulator_enable(regulator1_8);
		if (ret < 0) {
			pr_err("sii8620 : %s : regulator 1.8 is not enable", __func__);
			goto err_ext1;
		}
		ret = regulator_enable(regulator1_0);
		if (ret < 0) {
			pr_err("sii8620 : %s : regulator 1.0 is not enable", __func__);
			goto err_ext1;
		}
		usleep_range(10000, 20000);
	} else {
		/* To avoid floating state of the HPD pin *
		 * in the absence of external pull-up     */
		regulator_disable(regulator1_8);
		regulator_disable(regulator1_0);

		gpio_set_value_cansleep(pdata->gpio_mhl_reset, 0);
	}

	regulator_put(regulator1_0);
err_exit0:
	regulator_put(regulator1_8);
	return;

err_ext1:
	regulator_disable(regulator1_8);
	regulator_disable(regulator1_0);

	regulator_put(regulator1_0);
	regulator_put(regulator1_8);

	return;
}

static void of_sii8620_hw_reset(void)
{
	struct device *parent_dev = &device_addresses[0].client->dev;
	struct mhl_dev_context *sii8620 = dev_get_drvdata(parent_dev);
	struct sii8620_platform_data *pdata = sii8620->pdata;
	pr_info("sii8620 : %s:%d: HW-Reset\n", __func__, __LINE__);

	usleep_range(10000, 20000);
	gpio_set_value_cansleep(pdata->gpio_mhl_reset, 1);
	usleep_range(5000, 20000);
	gpio_set_value_cansleep(pdata->gpio_mhl_reset, 0);
	usleep_range(10000, 20000);
	gpio_set_value_cansleep(pdata->gpio_mhl_reset, 1);
	msleep(30);
}

enum mhl_gpio_type of_sii8620_get_gpio_type(char* str_type)
{
#if 0
	if(strcmp(str_type, "msmgpio") == 0)
		return MHL_GPIO_AP_GPIO;
	else if(strcmp(str_type, "pm8941_gpios") == 0)
		return MHL_GPIO_PM_GPIO;
	else if(strcmp(str_type, "pm8941_mpps") == 0)
		return MHL_GPIO_PM_MPP;
	else
		return MHL_GPIO_UNKNOWN_TYPE;
#endif
	return 0;
}


static int of_sii8620_parse_dt(struct sii8620_platform_data *pdata, struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct device_node *np_charger;
	const char* temp_string = NULL;
	int ret;

	pdata->gpio_mhl_irq = of_get_named_gpio_flags(np,
		"sii8620,gpio_mhl_irq", 0, NULL);
	if(pdata->gpio_mhl_irq > 0)
		pr_info("sii8620 : %s:%d: gpio_mhl_irq = %d\n",  __func__, __LINE__,
				pdata->gpio_mhl_irq);

	pdata->gpio_mhl_reset = of_get_named_gpio_flags(np,
		"sii8620,gpio_mhl_reset", 0, NULL);
	if(pdata->gpio_mhl_reset > 0)
		pr_info("sii8620 : %s:%d: gpio_mhl_reset = %d\n", __func__,
					__LINE__, pdata->gpio_mhl_reset);

	pdata->gpio_mhl_scl = of_get_named_gpio_flags(np,
		"sii8620,gpio_mhl_scl", 0, NULL);
	if(pdata->gpio_mhl_scl > 0)
		pr_info("sii8620 : %s:%d: gpio_mhl_scl = %d\n", __func__,
					__LINE__, pdata->gpio_mhl_scl);

	pdata->gpio_mhl_sda = of_get_named_gpio_flags(np,
		"sii8620,gpio_mhl_sda", 0, NULL);
	if(pdata->gpio_mhl_sda > 0)
		pr_info("sii8620 : %s:%d: gpio_mhl_sda = %d\n", __func__,
					__LINE__, pdata->gpio_mhl_sda);

	/* swing_level */
	if (!of_property_read_u32(np, "sii8620,swing_level_v2",
				&pdata->swing_level_v2))
		pr_info("sii8620 : %s:%d: swing_level_v2 = 0x%X\n", __func__, __LINE__,
				pdata->swing_level_v2);

	if (!of_property_read_u32(np, "sii8620,swing_level_v3",
				&pdata->swing_level_v3))
		pr_info("sii8620 : %s:%d: swing_level_v3 = 0x%X\n", __func__, __LINE__,
				pdata->swing_level_v3);

	/* xtal_mhl */
	ret = of_property_read_string(np, "clock-names", &temp_string);
	if (ret) {
		pr_err("sii8620 : %s: cannot get clock name(%d)", __func__, ret);
		return ret;
	}

	pdata->mhl_clk = clk_get(&client->dev, temp_string);
	if (IS_ERR(pdata->mhl_clk)) {
		pr_err("sii8620 : %s: cannot get clock(%d)", __func__, ret);
		return ret;
	}

	/* get charger_name */
	np_charger = of_find_node_by_name(NULL, "battery");
	if (np_charger != NULL) {
		if (!of_property_read_string(np_charger, "battery,charger_name",
					(char const **)&pdata->charger_name))
			pr_info("sii8620 : %s:%d: charger_name = %s\n", __func__, __LINE__,
					pdata->charger_name);
	}

	return 0;
}

static void of_sii8620_gpio_init(struct sii8620_platform_data *pdata)
{
	/* MHL_RESET */
	if(pdata->gpio_mhl_reset > 0) {
		if (gpio_request(pdata->gpio_mhl_reset, "mhl_reset")) {
			pr_err("[ERROR] %s: unable to request gpio_mhl_reset [%d]\n",
					__func__,pdata->gpio_mhl_reset);
			return;
		}
		if(gpio_direction_output(pdata->gpio_mhl_reset, 0)) {
			pr_err("[ERROR] %s: unable to gpio_mhl_reset low[%d]\n",
				__func__,pdata->gpio_mhl_reset);
			return;
		}
	}
	return;
}

static BLOCKING_NOTIFIER_HEAD(acc_mhl_notifier);
void of_sii8620_muic_mhl_notify(int event)
{
	pr_info("sii8620 : %s Attached: %d\n", __func__, event);
	blocking_notifier_call_chain(&acc_mhl_notifier, event, NULL);
}
#endif /* CONFIG_MHL3_SEC_FEATURE */

int si_8620_get_hpd_status(int *hpd_status)
{
	struct device *dev = NULL;
	int status = 0;
	struct mhl_dev_context *dev_context;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(dev);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("%scouldn't acquire mutex%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		return  -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("%sshutting down%s\n",
			ANSI_ESC_YELLOW_TEXT, ANSI_ESC_RESET_TEXT);
		status = -ENODEV;
	} else {
		*hpd_status = si_mhl_tx_drv_get_hpd_status(dev_context);
		MHL_TX_DBG_INFO("%HPD status: %s%d%s\n",
			ANSI_ESC_YELLOW_TEXT,
			*hpd_status,
			ANSI_ESC_RESET_TEXT);
	}
	up(&dev_context->isr_lock);
	return status;
}
EXPORT_SYMBOL_GPL(si_8620_get_hpd_status);

int si_8620_get_hdcp2_status(uint32_t *hdcp2_status)
{
	struct device *dev = NULL;
	int status = 0;
	struct mhl_dev_context *dev_context;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(dev);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("%scouldn't acquire mutex%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		return  -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("%sshutting down%s\n",
			ANSI_ESC_YELLOW_TEXT, ANSI_ESC_RESET_TEXT);
		status = -ENODEV;
	} else {
		*hdcp2_status = si_mhl_tx_drv_get_hdcp2_status(dev_context);
		MHL_TX_DBG_INFO("%HDCP2 status: %s0x%8x%s\n",
			ANSI_ESC_YELLOW_TEXT,
			*hdcp2_status,
			ANSI_ESC_RESET_TEXT);
	}
	up(&dev_context->isr_lock);
	return status;
}
EXPORT_SYMBOL_GPL(si_8620_get_hdcp2_status);

#ifdef CONFIG_MHL_SPI
static int si_8620_mhl_tx_spi_probe(struct spi_device *spi)
{
	int ret;
	static struct sii8620_platform_data *pdata = NULL;

	pr_info("sii8620 : %s:%d: start\n", __func__, __LINE__);
	pdata = kzalloc(sizeof(struct sii8620_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&spi->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	spi_dev = spi;
	if(!spi_dev->dev.of_node) {
		dev_err(&spi_dev->dev, "sii8620: spi node not-found\n");
		return -1;
	}

	pr_info("sii8620 : %s(), spi = %p\n", __func__, spi);
	spi->bits_per_word = 8;
	spi_dev = spi;

	spi_mem.tx_buf = kmalloc(MAX_SPI_XFER_BUFFER_SIZE, GFP_KERNEL);
	if (!spi_mem.tx_buf) {
		ret = -ENOMEM;
		goto tx_failed;
	}
	spi_mem.rx_buf = kmalloc(MAX_SPI_XFER_BUFFER_SIZE, GFP_KERNEL);
	if (!spi_mem.rx_buf) {
		ret = -ENOMEM;
		goto rx_failed;
	}
	spi_mem.block_tx_buffers = kmalloc(MAX_SPI_EMSC_BLOCK_SIZE *
		NUM_BLOCK_QUEUE_REQUESTS, GFP_KERNEL);
	if (!spi_mem.block_tx_buffers) {
		ret = -ENOMEM;
		goto blk_failed;
	}

	pdata->power = of_sii8620_hw_onoff;
	pdata->hw_reset = of_sii8620_hw_reset;
	pdata->vbus_present = sii8620_vbus_present;
	pdata->charger_mhl_cb = sii8620_charger_mhl_cb;
#ifdef MFD_MAX778XX_COMMON
	pdata->muic_otg_set = muic_otg_control;
#endif

#ifdef CONFIG_SII8620_CHECK_MONITOR
	pdata->link_monitor = sii8620_link_monitor;
#endif
	ret = of_sii8620_parse_dt(pdata, &spi_dev->dev);
	if (ret) {
		pr_err("sii8620 : %s:%d: Failed to parse DT\n",  __func__, __LINE__);
		goto failed;
	}

	of_sii8620_gpio_init(pdata);
	sii8620_hdmi_register_mhl(pdata);

	drv_info.irq = gpio_to_irq(pdata->gpio_mhl_irq);
	ret = mhl_tx_init(&drv_info, &spi_dev->dev, pdata);
	if (ret) {
		pr_err("sii8620 : %s:%d: mhl_tx_init failed, error code %d\n", __func__, __LINE__, ret);
	}

	pr_info("sii8620 : %s:%d: Probe Successful!\n", __func__, __LINE__);
	return ret;

failed:
	kfree(spi_mem.block_tx_buffers);
	spi_mem.block_tx_buffers = NULL;

blk_failed:
	kfree(spi_mem.rx_buf);
	spi_mem.rx_buf = NULL;

rx_failed:
	kfree(spi_mem.tx_buf);
	spi_mem.tx_buf = NULL;

tx_failed:
	probe_fail = true;
	kfree(pdata);
	pr_err("sii8620:%s:%d: probe failed!!!\n", __func__, __LINE__);

	return ret;
}

static int si_8620_mhl_spi_remove(struct spi_device *spi_dev)
{
	pr_info("%s() called\n", __func__);
	kfree(spi_mem.tx_buf);
	kfree(spi_mem.rx_buf);
	kfree(spi_mem.block_tx_buffers);
	return 0;
}

#else /* CONFIG_MHL_SPI */

static int __devinit of_sii8620_probe_dt(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	static struct sii8620_platform_data *pdata = NULL;
	int client_id = -1;

	if(!client->dev.of_node) {
		dev_err(&client->dev, "sii8620: Client node not-found\n");
		return -1;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	/* going to use block read/write, so check for this too */
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	if (of_property_read_u32(client->dev.of_node, "sii8620,client_id", &client_id)) {
		dev_err(&client->dev, "Wrong Client_id# %d", client_id);
		return -EINVAL;
	}

	device_addresses[client_id].client = client;

	if (0 == client_id) {
		pdata = kzalloc(sizeof(struct sii8620_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "failed to allocate driver data\n");
			return -ENOMEM;
		}

		i2c_mem.block_tx_buffers = kmalloc(MAX_I2C_EMSC_BLOCK_SIZE * NUM_BLOCK_QUEUE_REQUESTS, GFP_KERNEL);
		if (NULL == i2c_mem.block_tx_buffers) {
			kfree(pdata);
			return -ENOMEM;
		}

		i2c_bus_adapter = client->adapter;

		pdata->hw_reset = of_sii8620_hw_reset;
		pdata->power = of_sii8620_hw_onoff;
		pdata->charger_mhl_cb = sii8620_charger_mhl_cb;
		client->dev.platform_data = pdata;

#ifdef CONFIG_SII8620_CHECK_MONITOR
		pdata->link_monitor = sii8620_link_monitor;
#endif
		of_sii8620_parse_dt(pdata, client);
		of_sii8620_gpio_init(pdata);
	} else {
		client->dev.platform_data = pdata;
	}

	if (client_id == 7) {
		drv_info.irq = gpio_to_irq(pdata->gpio_mhl_irq);
		//drv_info.irq = device_addresses[0].client->irq;
		mhl_tx_init(&drv_info, &device_addresses[0].client->dev);
	}

	return 0;
}

static int si_8620_mhl_tx_remove(struct i2c_client *client)
{
	kfree(i2c_mem.block_tx_buffers);
	return 0;
}
#endif /* CONFIG_MHL_SPI */


#ifndef CONFIG_MHL_SPI
static const struct i2c_device_id sii8620_id[] = {
	{"sii8620_mhlv3", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, si_8620_mhl_tx_id);

static struct of_device_id sii8620_dt_ids[] = {
	{ .compatible = "sii8620,tmds",},		/*page0*/
	{ .compatible = "sii8620,usbt",},		/*page1*/
	{ .compatible = "sii8620,hdmi",},		/*page2*/
	{ .compatible = "sii8620,disc",},		/*page3*/
	{ .compatible = "sii8620,tpi",},		/*page6*/
	{ .compatible = "sii8620,cbus",},		/*page5*/
	{ .compatible = "sii8620,coc",},		/*page7*/
	{ .compatible = "sii8620,devcap",},		/*page4*/
	{}
};
MODULE_DEVICE_TABLE(of, sii8620_dt_ids);


static struct i2c_driver sii8620_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = MHL_DEVICE_NAME,
		   .of_match_table = of_match_ptr(sii8620_dt_ids),
		   },
	.id_table = sii8620_id,
	.probe = of_sii8620_probe_dt,
	.remove = si_8620_mhl_tx_remove,
};

#else /* CONFIG_MHL_SPI */

static const struct spi_device_id sii8620_id[] = {
	{"sii8620", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, sii8620_id);

static struct of_device_id sii8620_dt_ids[] = {
	{ .compatible = "sii8620,mhl3-spi",},
	{},
};

static struct spi_driver si_86x0_mhl_tx_spi_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = MHL_DRIVER_NAME,
		   .of_match_table = sii8620_dt_ids,
		   },
	.probe = si_8620_mhl_tx_spi_probe,
	.remove = si_8620_mhl_spi_remove,
#ifdef CONFIG_MHL3_SEC_FEATURE
	.id_table = sii8620_id,
#endif
};

static int __init spi_init(void)
{
	int status;

	status = spi_register_driver(&si_86x0_mhl_tx_spi_driver);
	if (status < 0) {
		pr_err("[ERROR] %s():%d failed !\n", __func__, __LINE__);
		goto exit;
	}

exit:
	if (probe_fail)
		status = -ENODEV;

	return status;
}
#endif /* #ifndef CONFIG_MHL_SPI */


static int __init si_8620_init(void)
{
	int ret;

	pr_info("sii8620 : %s:%d: Starting SiI8620 Driver\n", __func__, __LINE__);

	sema_init(&platform_lock, 1);

	platform_flags &= ~PLATFORM_FLAG_HEARTBEAT_MASK;
	switch (use_heartbeat) {
	case 0:
		/* don't do anything with heatbeat */
		break;
	case 1:
		platform_flags |= PLATFORM_VALUE_ISSUE_HEARTBEAT;
		break;
	case 2:
		platform_flags |= PLATFORM_VALUE_DISCONN_HEARTBEAT;
		break;
	default:
		MHL_TX_DBG_ERR("%sinvalid use_heartbeat parameter%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	}

	if (tmds_link_speed == 15)
		platform_flags |= PLATFORM_FLAG_1_5GBPS;
	else if (tmds_link_speed == 3)
		platform_flags |= PLATFORM_FLAG_3GBPS;
	else if (tmds_link_speed == 6)
		platform_flags |= PLATFORM_FLAG_6GBPS;

#ifdef CONFIG_MHL_SPI
	ret = spi_init();
	if (ret < 0) {
		pr_err("sii8620:%s:%d: [ERROR] mhl_v3 spi driver init failed", __func__, __LINE__);
		return ret;
	}
#else
	ret = i2c_add_driver(&sii8620_i2c_driver);
	if (ret < 0) {
		pr_err("sii8620:%s:%d: [ERROR] mhl_v3 i2c driver init failed", __func__, __LINE__);
		return ret;
	}
#endif /* CONFIG_MHL_SPI */

	return ret;
}

static void __exit si_8620_exit(void)
{
	int idx;

	pr_info("sii8620 : %s:%d: called\n", __func__, __LINE__);

	si_8620_power_control(false);

	if (use_spi) {
		mhl_tx_remove(&spi_dev->dev);
#ifndef CONFIG_MHL3_SEC_FEATURE
		spi_unregister_driver(&si_86x0_mhl_tx_spi_driver);
#endif
		spi_unregister_device(spi_dev);
	} else {
		if (device_addresses[0].client != NULL) {
			mhl_tx_remove(&device_addresses[0].client->dev);
			MHL_TX_DBG_INFO("client removed\n");
		}
#ifndef CONFIG_MHL3_SEC_FEATURE
		i2c_del_driver(&si_8620_mhl_tx_i2c_driver);
#endif
		MHL_TX_DBG_INFO("i2c driver deleted from context\n");

		for (idx = 0; idx < ARRAY_SIZE(device_addresses); idx++) {
			MHL_TX_DBG_INFO("\n");
			if (device_addresses[idx].client != NULL) {
				MHL_TX_DBG_INFO("unregistering device:%p\n",
						device_addresses[idx].client);
				i2c_unregister_device(device_addresses[idx].
						      client);
			}
		}
	}
	MHL_TX_DBG_ERR("driver unloaded.\n");
}

static int debug_level_stack[15];
static unsigned int debug_level_stack_ptr;

void push_debug_level(int new_verbosity)
{
	if (debug_level_stack_ptr < ARRAY_SIZE(debug_level_stack)) {
		/* stack is initially empty */
		debug_level_stack[debug_level_stack_ptr++] = debug_level;
		debug_level = new_verbosity;
	} else {
		MHL_TX_DBG_ERR("%sdebug_level_stack overflowed%s\n",
			       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	}
}

void pop_debug_level(void)
{
	if (debug_level_stack_ptr > 0) {
		if (debug_level_stack_ptr > ARRAY_SIZE(debug_level_stack)) {
			MHL_TX_DBG_ERR("%sdebug_level_stack overflowed%s\n",
				       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		} else {
			debug_level =
			    debug_level_stack[--debug_level_stack_ptr];
		}
	}
}

int si_8620_register_callbacks(struct si_mhl_callback_api_t *p_callbacks)
{
	struct device *dev = NULL;
	int status = 0;
	struct mhl_dev_context *dev_context;
	struct drv_hw_context *hw_context;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(dev);
	hw_context = (struct drv_hw_context *)&dev_context->drv_context;

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		if (NULL != p_callbacks)
			hw_context->callbacks = *p_callbacks;
	}

	up(&dev_context->isr_lock);
	return status;
}
EXPORT_SYMBOL(si_8620_register_callbacks);

int si_8620_info_frame_change(enum hpd_high_callback_status mode_parm,
	union avif_or_cea_861_dtd_u *p_avif_or_dtd,
	size_t avif_or_dtd_max_length, union vsif_mhl3_or_hdmi_u *p_vsif,
	size_t vsif_max_length)
{
	struct device *dev = NULL;
	int status;
	struct mhl_dev_context *dev_context;
	struct drv_hw_context *hw_context;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(dev);
	hw_context = (struct drv_hw_context *)&dev_context->drv_context;

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		size_t xfer_size;

		memset(&hw_context->vsif_mhl3_or_hdmi_from_callback, 0,
			sizeof(hw_context->vsif_mhl3_or_hdmi_from_callback));
		memset(&hw_context->avif_or_dtd_from_callback, 0,
			sizeof(hw_context->avif_or_dtd_from_callback));

		if (sizeof(hw_context->vsif_mhl3_or_hdmi_from_callback) <
			vsif_max_length) {
			xfer_size = sizeof(
				hw_context->vsif_mhl3_or_hdmi_from_callback);
		} else {
			xfer_size = vsif_max_length;
		}
		memcpy(&hw_context->vsif_mhl3_or_hdmi_from_callback, p_vsif,
			xfer_size);

		if (sizeof(hw_context->avif_or_dtd_from_callback) <
			avif_or_dtd_max_length) {
			xfer_size = sizeof(
				hw_context->avif_or_dtd_from_callback);
		} else {
			xfer_size = avif_or_dtd_max_length;
		}
		memcpy(&hw_context->avif_or_dtd_from_callback, p_avif_or_dtd,
			xfer_size);

		status = si_mhl_tx_drv_set_display_mode(dev_context, mode_parm);
	}

	up(&dev_context->isr_lock);
	return status;
}
EXPORT_SYMBOL(si_8620_info_frame_change);

module_init(si_8620_init);
module_exit(si_8620_exit);

MODULE_DESCRIPTION("Silicon Image MHL Transmitter driver");
MODULE_AUTHOR("Silicon Image <http://www.siliconimage.com>");
MODULE_LICENSE("GPL");
