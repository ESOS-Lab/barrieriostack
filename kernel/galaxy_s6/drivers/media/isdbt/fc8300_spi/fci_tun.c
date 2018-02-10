/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fci_tun.c

	Description : source of tuner control driver

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	History :
	----------------------------------------------------------------------
*******************************************************************************/
#include "fci_types.h"
#include "fc8300_regs.h"
#include "fci_hal.h"
#include "fci_tun.h"
#include "fci_hpi.h"
#include "fc8300_bb.h"
#include "fc8300_tun.h"
#include "fci_oal.h"

#define FC8300_TUNER_ADDR	0x58

struct I2C_DRV {
	int (*init)(HANDLE handle, DEVICEID devid,
			s32 speed, s32 slaveaddr);
	int (*read)(HANDLE handle, DEVICEID devid,
			u8 chip, u8 addr, u8 alen, u8 *data, u8 len);
	int (*write)(HANDLE handle, DEVICEID devid,
			u8 chip, u8 addr, u8 alen, u8 *data, u8 len);
	int (*deinit)(HANDLE handle, DEVICEID devid);
};

static struct I2C_DRV fcihpi = {
	&fci_hpi_init,
	&fci_hpi_read,
	&fci_hpi_write,
	&fci_hpi_deinit
};

struct TUNER_DRV {
	int (*init)(HANDLE handle, DEVICEID devid,
			enum BROADCAST_TYPE broadcast);
	int (*set_freq)(HANDLE handle, DEVICEID devid, u32 freq);
	int (*get_rssi)(HANDLE handle, DEVICEID devid, s32 *rssi);
	int (*deinit)(HANDLE handle, DEVICEID devid);
};

static struct TUNER_DRV fc8300_tuner = {
	&fc8300_tuner_init,
	&fc8300_set_freq,
	&fc8300_get_rssi,
	&fc8300_tuner_deinit
};

static u8 tuner_addr = FC8300_TUNER_ADDR;
static enum BROADCAST_TYPE broadcast_type = ISDBT_13SEG;
static enum I2C_TYPE tuner_i2c = FCI_HPI_TYPE;
static struct I2C_DRV *tuner_ctrl = &fcihpi;
static struct TUNER_DRV *tuner = &fc8300_tuner;

s32 tuner_ctrl_select(HANDLE handle, DEVICEID devid, enum I2C_TYPE type)
{
	print_log(0, " %s\n", __func__);
	switch (type) {
	case FCI_HPI_TYPE:
		print_log(0, " FCI_HPI_TYPE\n");
		tuner_ctrl = &fcihpi;
		break;
	default:
		print_log(0, " DEFAULT ERROR\n");
		return BBM_E_TN_CTRL_SELECT;
	}

	if (tuner_ctrl->init(handle, devid, 600, 0))
		return BBM_E_TN_CTRL_INIT;

	tuner_i2c = type;

	return BBM_OK;
}

s32 tuner_ctrl_deselect(HANDLE handle, DEVICEID devid)
{
	print_log(0, " %s\n", __func__);
	if (tuner_ctrl == NULL)
		return BBM_E_TN_CTRL_SELECT;

	tuner_ctrl->deinit(handle, devid);
	tuner_ctrl = NULL;

	return BBM_OK;
}

s32 tuner_i2c_read(HANDLE handle, DEVICEID devid,
		u8 addr, u8 alen, u8 *data, u8 len)
{
	if (tuner_ctrl == NULL)
		return BBM_E_TN_CTRL_SELECT;

	if (tuner_ctrl->read(handle, devid,
				tuner_addr, addr, alen, data, len))
		return BBM_E_TN_READ;

	return BBM_OK;
}

s32 tuner_i2c_write(HANDLE handle, DEVICEID devid,
		u8 addr, u8 alen, u8 *data, u8 len)
{
	if (tuner_ctrl == NULL)
		return BBM_E_TN_CTRL_SELECT;

	if (tuner_ctrl->write(handle, devid,
			tuner_addr, addr, alen, data, len))
		return BBM_E_TN_WRITE;

	return BBM_OK;
}

s32 tuner_set_freq(HANDLE handle, DEVICEID devid, u32 freq, u8 subch)
{
#ifdef BBM_I2C_TSIF
	u8 tsif_en = 0;
#endif

	if (tuner == NULL)
		return BBM_E_TN_SELECT;

#ifdef BBM_I2C_TSIF
	if (devid == DIV_MASTER || devid == DIV_BROADCAST) {
		bbm_byte_read(handle, DIV_MASTER, BBM_TS_SEL, &tsif_en);
		bbm_byte_write(handle, DIV_MASTER, BBM_TS_SEL, tsif_en & 0x7f);
	}
#endif

	fc8300_set_core_clk(handle, devid, broadcast_type, freq);

	bbm_byte_write(handle, devid, BBM_CENTER_CH_NUM, subch);

	switch (broadcast_type) {
	case ISDBT_1SEG:
	case ISDBTMM_1SEG:
	case ISDBTSB_1SEG:
	case ISDBT_CATV_1SEG:
		freq -= 380;
		break;
	case ISDBTSB_3SEG:
#if (BBM_BAND_WIDTH == 6)
		freq -= 1000;
#elif (BBM_BAND_WIDTH == 7)
		freq -= 1170;
#else /* BBM)BAND_WIDTH == 8 */
		freq -= 1330;
#endif
		break;
	case ISDBT_13SEG:
	case ISDBTMM_13SEG:
	case ISDBT_CATV_13SEG:
		break;
	}

	if (tuner->set_freq(handle, devid, freq))
		return BBM_E_TN_SET_FREQ;

	fc8300_reset(handle, devid);

#ifdef BBM_I2C_TSIF
	if (devid == DIV_MASTER || devid == DIV_BROADCAST)
		bbm_byte_write(handle, DIV_MASTER, BBM_TS_SEL, tsif_en);
#endif

	return BBM_OK;
}

s32 tuner_select(HANDLE handle, DEVICEID devid,
		enum PRODUCT_TYPE product, enum BROADCAST_TYPE broadcast)
{
	switch (product) {
	case FC8300_TUNER:
		tuner = &fc8300_tuner;
		tuner_addr = FC8300_TUNER_ADDR;
		broadcast_type = broadcast;
		break;
	}

	if (tuner == NULL) {
			print_log(0, "ERROR tuner == NULL\n");
		return BBM_E_TN_SELECT;
	}
	if (tuner->init(handle, devid, broadcast)) {
			print_log(0, "tuner->init\n");
		return BBM_E_TN_INIT;
	}
	fc8300_set_broadcast_mode(handle, devid, broadcast);

#ifdef BBM_ES

	if (product == FC8300_TUNER) {
		u8 chip_ver = 0x00;
		tuner_i2c_read(handle, devid, 0xff, 1, &chip_ver, 1);

		if (chip_ver == 0xc0)
			return BBM_OK;

		bbm_byte_write(handle, DIV_MASTER, BBM_RESYNC_ENABLE, 0xcf);
		bbm_long_write(handle, DIV_BROADCAST, BBM_MEMORY_RWM0,
							0x05555555);
		bbm_byte_write(handle, DIV_BROADCAST, BBM_SFS_FTS_ERR_MAX_1SEG,
							0x08);
		bbm_byte_write(handle, DIV_BROADCAST, BBM_SFS_FTS_ERR_MAX_3SEG,
							0x08);
		bbm_byte_write(handle, DIV_BROADCAST, BBM_PGA_GAIN_MAX, 0x0c);
		bbm_byte_write(handle, DIV_BROADCAST, BBM_CSF_GAIN_MAX, 0x09);
		bbm_byte_write(handle, DIV_MASTER, BBM_FD_OUT_MODE, 0x03);
		bbm_byte_write(handle, DIV_MASTER, BBM_DIV_START_MODE, 0x17);
		bbm_byte_write(handle, DIV_BROADCAST,
					BBM_PSAT_ON_REF_1SEG_QPSK, 0x1a);
		bbm_byte_write(handle, DIV_BROADCAST,
					BBM_PSAT_ON_REF_1SEG_16QAM, 0x1b);

		switch (broadcast) {
		case ISDBT_1SEG:
		case ISDBTMM_1SEG:
		case ISDBTSB_1SEG:
		case ISDBT_CATV_1SEG:
		case ISDBTSB_3SEG:
			bbm_byte_write(handle, DIV_BROADCAST, BBM_SFS_MTH,
							0x32);
			break;
		case ISDBT_13SEG:
		case ISDBTMM_13SEG:
		case ISDBT_CATV_13SEG:
			bbm_byte_write(handle, DIV_BROADCAST, BBM_SFS_MTH,
							0x31);
			break;
		}

#if defined(BBM_2_DIVERSITY) || defined(BBM_4_DIVERSITY)
		bbm_byte_write(handle, DIV_MASTER, BBM_XTAL_OUTBUF_EN, 0x00);
		bbm_byte_write(handle, DIV_MASTER, BBM_XTAL_OUTBUF_GAIN, 0x03);
		bbm_word_write(handle, DIV_BROADCAST, BBM_FD_RD_LATENCY_1SEG,
								0x1840);
		bbm_byte_write(handle, DIV_BROADCAST, BBM_COMB_OFF, 0x80);
#else /* SINGLE */
		bbm_word_write(handle, DIV_BROADCAST, BBM_FD_RD_LATENCY_1SEG,
								0x0002);
#endif /* #if defined(BBM_2_DIVERSITY) || defined(BBM_4_DIVERSITY) */
	}
#endif /* #ifdef BBM_ES */

	return BBM_OK;
}

s32 tuner_deselect(HANDLE handle, DEVICEID devid)
{
	if (tuner == NULL)
		return BBM_E_TN_SELECT;

	if (tuner->deinit(handle, devid))
		return BBM_NOK;

	tuner = NULL;

	return BBM_OK;
}

s32 tuner_get_rssi(HANDLE handle, DEVICEID devid, s32 *rssi)
{
	if (tuner == NULL)
		return BBM_E_TN_SELECT;

	if (tuner->get_rssi(handle, devid, rssi))
		return BBM_E_TN_RSSI;

	return BBM_OK;
}

