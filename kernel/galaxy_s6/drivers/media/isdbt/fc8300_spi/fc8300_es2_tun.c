/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8300_es2_tun.c

	Description : source of FC8300 tuner driver

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
	0p1 -> initial driver
	0p2 -> VCO Currunt Setting Add
	1p2 -> 20130826
	1p0 -> 20130730
	1p1 -> 20130812
	1p4 -> 20130821
	1p5 -> 20130826
	1p6 -> 20130827
	1p7 -> 20130830
	1p8 -> 20130902
	2p1 -> 20130911
	2p5 -> 20131108
*******************************************************************************/
#include "fci_types.h"
#include "fci_oal.h"
#include "fci_tun.h"
#include "fci_hal.h"
#include "fc8300_regs.h"
#include "fc8300_es2_tun.h"

#define DRIVER_VERSION         0x25 /* Driver M2_V2p5*/
#define RFADC_Count 3
#define Cal_Repeat 1

#define FC8300_BAND_WIDTH       BBM_BAND_WIDTH

extern unsigned int fc8300_xtal_freq;

static enum BROADCAST_TYPE broadcast_type = ISDBT_13SEG;
static u8 catv_status;
static u8 broad_check;

/* RF_Driver */
static u32 thread_freq[4] = {707143, 707143, 707143, 707143};
static u32 bc_type[4] = {0, 0, 0, 0};
static u8 device_set;
static u8 thread_set;
static u8 thread_check;
static u8 fc_cal_value[4] = {0, 0, 0, 0};

static u8 status_1seg[4] = {0, 0, 0, 0};
static s8 status_13seg[4] = {0, 0, 0, 0};

static HANDLE t_handle;
static DEVICEID t_devid[4] = {DIV_MASTER, DIV_SLAVE0, DIV_SLAVE1, DIV_SLAVE2};

static s32 fc8300_write(HANDLE handle, DEVICEID devid, u8 addr, u8 data)
{
	s32 res;

	res = tuner_i2c_write(handle, devid, addr, 1, &data, 1);

	return res;
}

static s32 fc8300_read(HANDLE handle, DEVICEID devid, u8 addr, u8 *data)
{
	s32 res;

	res = tuner_i2c_read(handle, devid, addr, 1, data, 1);

	return res;
}

#if 0
static s32 fc8300_bb_write(HANDLE handle, DEVICEID devid, u16 addr, u8 data)
{
	s32 res;

	res = bbm_write(handle, devid, addr, data);

	return res;
}

static s32 fc8300_bb_read(HANDLE handle, DEVICEID devid, u16 addr, u8 *data)
{
	s32 res;

	res = bbm_read(handle, devid, addr, data);

	return res;
}
#endif

#ifdef BBM_ES_CURRENT
static u32 KbdFunc(void)
{

	s32 i = 0;

	u8 up_low_limit = 182;
	u8 up_high_limit = 217;
	u8 down_low_limit = 177;
	u8 down_high_limit = 212;
	u8 rssi_buf[4] = {0, 0, 0, 0};
	u8 min = 0;
	u8 max = 0;
	u8 check_thread;

	u8 rssi_buff[4][5] = {
		{0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0}
	};

	u8 lna_temp;
	u8 atten_temp;
	u8 filter_temp;

	u8 lna_set = 0x44;
	u8 lna_set1 = 0x44;
	u8 lna_set2 = 0x46;
	u8 lna_set3 = 0x47;

	while (thread_set) {

		fc8300_read(t_handle, t_devid[0], 0xfa, &check_thread);
		if (check_thread != 0) {
			fc8300_write(t_handle, t_devid[0], 0xfa, 0xee);
			thread_set = 0;
			_endthread();
		}

		if ((device_set & 0x01) == 1) {

			if (bc_type[0] == ISDBT_13SEG) {
				lna_set = 0x46;

				fc8300_read(t_handle, t_devid[0], 0xeb,
						&rssi_buff[0][0]);
				fc8300_read(t_handle, t_devid[0], 0xeb,
						&rssi_buff[0][1]);
				fc8300_read(t_handle, t_devid[0], 0xeb,
						&rssi_buff[0][2]);
				fc8300_read(t_handle, t_devid[0], 0xeb,
						&rssi_buff[0][3]);
				fc8300_read(t_handle, t_devid[0], 0xeb,
						&rssi_buff[0][4]);

				for (i = 0; i < 4; i++) {
					if (rssi_buff[0][max] >
						rssi_buff[0][i+1])
						max = i + 1;
				}
				for (i = 0; i < 4; i++) {
					if (rssi_buff[0][min] <
						rssi_buff[0][i+1])
						min = i + 1;
				}

				rssi_buff[0][min] = 0;
				rssi_buff[0][max] = 0;

				rssi_buf[0] = (rssi_buff[0][0] +
						rssi_buff[0][1] +
						rssi_buff[0][2] +
						rssi_buff[0][3] +
						rssi_buff[0][4]) / 3;

				/*status_13seg[0] 1 => Buff ON*/
				/*status_13seg[0] 0 => Buff OFF*/

				if (thread_freq[0] >= 647143) {
					if (status_13seg[0] == 0) {
						if (rssi_buf[0] > up_low_limit
							&& rssi_buf[0] <
							down_high_limit) {
								fc8300_write(
								t_handle,
								t_devid[0],
								0x03, 0x00);
								status_13seg[0]
									= 1;

						}
					} else if (status_13seg[0] == 1) {
						if (rssi_buf[0] <
							down_low_limit ||
							rssi_buf[0] >
							up_high_limit) {
								fc8300_write(
								t_handle,
								t_devid[0],
								0x03, 0x04);
								status_13seg[0]
									= 0;
						}
					}
				} else if (thread_freq[0] < 647143 &&
							status_13seg[0] == 1) {
					fc8300_write(t_handle, t_devid[0],
							0x03, 0x04);
					status_13seg[0] = 0;

				}
			}

			if (bc_type[0] == ISDBT_1SEG) {
				if (thread_freq[0] < 590000)
					lna_set = lna_set3;
				else
					lna_set = lna_set2;
			}

			fc8300_read(t_handle, t_devid[0], 0x95, &lna_temp);
			fc8300_read(t_handle, t_devid[0], 0x96, &atten_temp);
			fc8300_read(t_handle, t_devid[0], 0xd2, &filter_temp);
			lna_temp = lna_temp & 0x0f;
			atten_temp = atten_temp & 0x0f;

			if (filter_temp >= 9) {

				if (status_1seg[0] == 0) {
					fc8300_write(t_handle, t_devid[0],
							0x15, 0x44);
					status_1seg[0] = 1;
				}

			} else if (filter_temp < 9) {

				if ((atten_temp >= 1) || (lna_temp >= 1)) {

					if (status_1seg[0] == 1) {
						fc8300_write(t_handle,
							t_devid[0],
							0x15, lna_set);
						status_1seg[0] = 0;
					}

				} else {

					if (status_1seg[0] == 0) {
						fc8300_write(t_handle,
							t_devid[0], 0x15, 0x44);
						status_1seg[0] = 1;
					}
				}
			}

			/*
			if (lna_temp >= 1) {
				if ((filter_temp < 9) && (status_15[0] != 1)) {
					fc8300_write(t_handle, t_devid[0],
							0x15, lna_set);

					status_15[0] = 1;
				} else if ((filter_temp >= 9) &&
					(status_15[0] != 2)) {
					fc8300_write(t_handle, t_devid[0],
							0x15, 0x41);

					status_15[0] = 2;
				}
			}
			*/
		}
		if ((device_set & 0x02) == 2) {


			if (bc_type[1] == ISDBT_13SEG) {
				lna_set = 0x46;

				fc8300_read(t_handle, t_devid[1], 0xeb,
						&rssi_buff[1][0]);
				fc8300_read(t_handle, t_devid[1], 0xeb,
						&rssi_buff[1][1]);
				fc8300_read(t_handle, t_devid[1], 0xeb,
						&rssi_buff[1][2]);
				fc8300_read(t_handle, t_devid[1], 0xeb,
						&rssi_buff[1][3]);
				fc8300_read(t_handle, t_devid[1], 0xeb,
						&rssi_buff[1][4]);

				for (i = 0; i < 4; i++) {
					if (rssi_buff[1][max] >
						rssi_buff[1][i+1])
						max = i + 1;
				}
				for (i = 0; i < 4; i++) {
					if (rssi_buff[1][min] <
						rssi_buff[1][i+1])
						min = i + 1;
				}

				rssi_buff[1][min] = 0;
				rssi_buff[1][max] = 0;

				rssi_buf[1] = (rssi_buff[1][0] +
						rssi_buff[1][1] +
						rssi_buff[1][2] +
						rssi_buff[1][3] +
						rssi_buff[1][4]) / 3;

				/*status_13seg[1] 1 => Buff ON*/
				/*status_13seg[1] 0 => Buff OFF*/

				if (thread_freq[1] >= 647143) {
					if (status_13seg[1] == 0) {
						if (rssi_buf[1] >
							up_low_limit &&
							rssi_buf[1] <
							down_high_limit) {
								fc8300_write(
								t_handle,
								t_devid[1],
								0x03, 0x00);
							status_13seg[1] = 1;
						}
					} else if (status_13seg[1] == 1) {
						if (rssi_buf[1] <
							down_low_limit ||
							rssi_buf[1] >
							up_high_limit) {
							fc8300_write(t_handle,
								t_devid[1],
								0x03, 0x04);
							status_13seg[1] = 0;
						}
					}
				} else if (thread_freq[1] < 647143 &&
							status_13seg[1] == 1) {
					fc8300_write(t_handle, t_devid[1],
							0x03, 0x04);
					status_13seg[1] = 0;
				}
			}

			if (bc_type[1] == ISDBT_1SEG) {
				if (thread_freq[1] < 590000)
					lna_set = lna_set3;
				else
					lna_set = lna_set1;
			}

			fc8300_read(t_handle, t_devid[1], 0x95, &lna_temp);
			fc8300_read(t_handle, t_devid[1], 0x96, &atten_temp);
			fc8300_read(t_handle, t_devid[1], 0xd2, &filter_temp);
			lna_temp = lna_temp & 0x0f;
			atten_temp = atten_temp & 0x0f;

			if (filter_temp >= 9) {

				if (status_1seg[1] == 0) {
					fc8300_write(t_handle, t_devid[1],
							0x15, 0x44);
					status_1seg[1] = 1;
				}

			} else if (filter_temp < 9) {

				if ((atten_temp >= 1) || (lna_temp >= 1)) {

					if (status_1seg[1] == 1) {
						fc8300_write(t_handle,
							t_devid[1],
							0x15, lna_set);
						status_1seg[1] = 0;
					}

				} else {

					if (status_1seg[1] == 0) {
						fc8300_write(t_handle,
							t_devid[1],
							0x15, 0x44);
						status_1seg[1] = 1;
				}
			}
		}

		}
		if ((device_set & 0x04) == 4) {

			if (bc_type[2] == ISDBT_13SEG) {
				lna_set = 0x46;

				fc8300_read(t_handle, t_devid[2], 0xeb,
						&rssi_buff[2][0]);
				fc8300_read(t_handle, t_devid[2], 0xeb,
						&rssi_buff[2][1]);
				fc8300_read(t_handle, t_devid[2], 0xeb,
						&rssi_buff[2][2]);
				fc8300_read(t_handle, t_devid[2], 0xeb,
						&rssi_buff[2][3]);
				fc8300_read(t_handle, t_devid[2], 0xeb,
						&rssi_buff[2][4]);

				for (i = 0; i < 4; i++) {
					if (rssi_buff[2][max] >
						rssi_buff[2][i+1])
						max = i + 1;
				}
				for (i = 0; i < 4; i++) {
					if (rssi_buff[2][min] <
						rssi_buff[2][i+1])
						min = i + 1;
				}

				rssi_buff[2][min] = 0;
				rssi_buff[2][max] = 0;

				rssi_buf[2] = (rssi_buff[2][0] +
						rssi_buff[2][1] +
						rssi_buff[2][2] +
						rssi_buff[2][3] +
						rssi_buff[2][4]) / 3;

				/*status_13seg[2] 1 => Buff ON*/
				/*status_13seg[2] 0 => Buff OFF*/

				if (thread_freq[2] >= 647143) {
					if (status_13seg[2] == 0) {
						if (rssi_buf[2] > up_low_limit
							&& rssi_buf[2] <
							down_high_limit) {
								fc8300_write(
								t_handle,
								t_devid[2],
								0x03, 0x00);
								status_13seg[2]
									= 1;
						}
					} else if (status_13seg[2] == 1) {
						if (rssi_buf[2] <
							down_low_limit ||
							rssi_buf[2] >
							up_high_limit) {
								fc8300_write(
								t_handle,
								t_devid[2],
								0x03, 0x04);
								status_13seg[2]
									= 0;
						}
					}
				} else if (thread_freq[2] < 647143 &&
							status_13seg[2] == 1) {
					fc8300_write(t_handle, t_devid[2],
							0x03, 0x04);
					status_13seg[2] = 0;
				}
			}

			if (bc_type[2] == ISDBT_1SEG) {
				if (thread_freq[2] < 590000)
					lna_set = lna_set3;
				else
					lna_set = lna_set1;
			}

			fc8300_read(t_handle, t_devid[2], 0x95, &lna_temp);
			fc8300_read(t_handle, t_devid[2], 0x96, &atten_temp);
			fc8300_read(t_handle, t_devid[2], 0xd2, &filter_temp);
			lna_temp = lna_temp & 0x0f;
			atten_temp = atten_temp & 0x0f;

			if (filter_temp >= 9) {

				if (status_1seg[2] == 0) {
					fc8300_write(t_handle, t_devid[2],
							0x15, 0x44);
					status_1seg[2] = 1;
				}

			} else if (filter_temp < 9) {

				if ((atten_temp >= 1) || (lna_temp >= 1)) {

					if (status_1seg[2] == 1) {
						fc8300_write(t_handle,
							t_devid[2], 0x15,
							lna_set);
						status_1seg[2] = 0;
					}

				} else {

					if (status_1seg[2] == 0) {
						fc8300_write(t_handle,
							t_devid[2],
							0x15, 0x44);
						status_1seg[2] = 1;
				}
			}
		}

		}
		if ((device_set & 0x08) == 8) {

			if (bc_type[3] == ISDBT_13SEG) {
				lna_set = 0x46;

				fc8300_read(t_handle, t_devid[3], 0xeb,
						&rssi_buff[3][0]);
				fc8300_read(t_handle, t_devid[3], 0xeb,
						&rssi_buff[3][1]);
				fc8300_read(t_handle, t_devid[3], 0xeb,
						&rssi_buff[3][2]);
				fc8300_read(t_handle, t_devid[3], 0xeb,
						&rssi_buff[3][3]);
				fc8300_read(t_handle, t_devid[3], 0xeb,
						&rssi_buff[3][4]);

				for (i = 0; i < 4; i++) {
					if (rssi_buff[3][max] >
						rssi_buff[3][i+1])
						max = i + 1;
				}
				for (i = 0; i < 4; i++) {
					if (rssi_buff[3][min] <
						rssi_buff[3][i+1])
						min = i + 1;
				}

				rssi_buff[3][min] = 0;
				rssi_buff[3][max] = 0;

				rssi_buf[3] = (rssi_buff[3][0] +
						rssi_buff[3][1] +
						rssi_buff[3][2] +
						rssi_buff[3][3] +
						rssi_buff[3][4]) / 3;

				/*status_13seg[3] 1 => Buff ON*/
				/*status_13seg[3] 0 => Buff OFF*/

				if (thread_freq[3] >= 647143) {
					if (status_13seg[3] == 0) {
						if (rssi_buf[3] > up_low_limit
							&& rssi_buf[3] <
							down_high_limit) {
								fc8300_write(
								t_handle,
								t_devid[3],
								0x03, 0x00);
								status_13seg[3]
									= 1;
						}
					} else if (status_13seg[3] == 1) {
						if (rssi_buf[3] <
							down_low_limit ||
							rssi_buf[3] >
							up_high_limit) {
								fc8300_write(
								t_handle,
								t_devid[3],
								0x03, 0x04);
								status_13seg[3]
									= 0;
						}
					}
				} else if (thread_freq[3] < 647143 &&
							status_13seg[3] == 1) {
					fc8300_write(t_handle, t_devid[3],
							0x03, 0x04);
					status_13seg[3] = 0;
				}
			}

			if (bc_type[3] == ISDBT_1SEG) {
				if (thread_freq[3] < 590000)
					lna_set = lna_set3;
				else
					lna_set = lna_set1;
			}

			fc8300_read(t_handle, t_devid[3], 0x95, &lna_temp);
			fc8300_read(t_handle, t_devid[3], 0x96, &atten_temp);
			fc8300_read(t_handle, t_devid[3], 0xd2, &filter_temp);
			lna_temp = lna_temp & 0x0f;
			atten_temp = atten_temp & 0x0f;

			if (filter_temp >= 9) {

				if (status_1seg[3] == 0) {
					fc8300_write(t_handle, t_devid[3],
							0x15, 0x44);
					status_1seg[3] = 1;
				}

			} else if (filter_temp < 9) {

				if ((atten_temp >= 1) || (lna_temp >= 1)) {

					if (status_1seg[3] == 1) {
						fc8300_write(t_handle,
							t_devid[3], 0x15,
							lna_set);
						status_1seg[3] = 0;
					}

				} else {

					if (status_1seg[3] == 0) {
						fc8300_write(t_handle,
							t_devid[3],
							0x15, 0x44);
						status_1seg[3] = 1;
					}
				}
			}

		}
		msWait(300);
	}
	fc8300_write(t_handle, t_devid[0], 0xfa, 0xcc);
	_endthread();

	return BBM_OK;

}
#endif

static u8 tf_cal(HANDLE handle, DEVICEID devid)
{
	u8 rfadc[RFADC_Count];
	u32 rfadc_avg = 0;

	u8 max_cb_code[Cal_Repeat] = {0};
	u8 max_rfadc[Cal_Repeat] = {0};
	u8 cal_temp;

	u32 x, i, j;

	for (x = 0; x < Cal_Repeat; x++) {
		for (i = 3; i < 12; i++) {
			cal_temp = (16 + i);
			fc8300_write(handle, devid, 0x1c, cal_temp);
			rfadc_avg = 0;

			for (j = 0; j < RFADC_Count; j++) {

				fc8300_read(handle, devid, 0xf5, &rfadc[j]);
				rfadc_avg = rfadc_avg + rfadc[j];

			}

			rfadc_avg = rfadc_avg / RFADC_Count;

			if (max_rfadc[x] < rfadc_avg) {

				max_rfadc[x] = (u8)rfadc_avg;
				max_cb_code[x] = (16 + i);

			}
		}
	}

	return max_cb_code[0];
}

static s32 fc8300_tuner_set_pll(HANDLE handle, DEVICEID devid, u32 freq,
							u32 offset)
{
	u8 data_0x51, data_0x57, lo_select, lo_div;
	u32 f_diff, f_diff_shifted, n_val, k_val;
	u32 f_vco, f_comp, f_lo_kHz;
	u32 f_vco_max = 6200000;
	u8 lo_divide_ratio[8] = {8, 12, 16, 24, 32, 48, 64, 96};
	u32 r_val;
	u8 pre_shift_bits = 4;
	u8 lock_detect = 0;
	u8 i;

	f_lo_kHz = freq;

	for (lo_select = 0; lo_select < 7; lo_select++)
		if (f_vco_max < f_lo_kHz * lo_divide_ratio[lo_select + 1])
			break;

	lo_div = lo_divide_ratio[lo_select];
	f_vco = f_lo_kHz * lo_div;

	r_val = 1;
	f_comp = fc8300_xtal_freq / r_val;

	n_val =	f_vco / f_comp;
	f_diff = f_vco - f_comp * n_val;

	f_diff_shifted = f_diff << (20 - pre_shift_bits);

	k_val = f_diff_shifted / (f_comp >> pre_shift_bits);
	k_val = k_val | 1;

	data_0x57 = ((n_val >> 3) & 0x20);
/* data_0x57 += (r_val == 1) ? 0 : 0x10;CID */
	data_0x57 += (k_val >> 16);

	data_0x51 = 0x00;

	data_0x51 += (lo_select * 2) + 1;

	fc8300_write(handle, devid, 0x7C, 0x00);
	fc8300_write(handle, devid, 0x7E, 0x00);
	fc8300_write(handle, devid, 0x7F, 0x00);
	fc8300_write(handle, devid, 0x80, 0x00);
	fc8300_write(handle, devid, 0x81, 0x00);
	fc8300_write(handle, devid, 0x15, 0x77);
	fc8300_write(handle, devid, 0x91, 0xA8);

	fc8300_write(handle, devid, 0x70, 0x63);
	fc8300_write(handle, devid, 0x6a, 0x00);

	fc8300_write(handle, devid, 0x51, data_0x51);
	fc8300_write(handle, devid, 0x57, data_0x57);
	fc8300_write(handle, devid, 0x58, (u8) ((k_val >> 8) & 0xFF));
	fc8300_write(handle, devid, 0x5a, (u8) n_val);
	fc8300_write(handle, devid, 0x59, (u8) ((k_val) & 0xFF));

	fc8300_write(handle, devid, 0x50, 0xe3);
	fc8300_write(handle, devid, 0x50, 0xff);

	for (i = 1; i < 15; i++) {

		fc8300_read(handle, devid, 0x64, &lock_detect);

		if ((lock_detect & 0x01) == 1) {
			fc8300_read(handle, devid, 0x65, &lock_detect);
			fc8300_write(handle, devid, 0x6d, (lock_detect | 0x0f));
			fc8300_write(handle, devid, 0x6f, (lock_detect | 0x0f) +
									0x10);
			fc8300_write(handle, devid, 0x70, 0x40);
			fc8300_write(handle, devid, 0x6a, 0x10);

			break;
		}
		msWait(2);
	}

	for (i = 1; i < 15; i++) {

		fc8300_read(handle, devid, 0x76, &lock_detect);

		if (lock_detect == 0x20) {
			fc8300_write(handle, devid, 0x5a, (u8) n_val);
			fc8300_write(handle, devid, 0x50, 0xe3);
			fc8300_write(handle, devid, 0x50, 0xff);
			break;
		}
		msWait(2);
	}

	if (broadcast_type == ISDBT_CATV_13SEG) {
		fc8300_write(handle, devid, 0x91, 0xa8);
		fc8300_write(handle, devid, 0x15, 0x47);
		fc8300_write(handle, devid, 0x19, 0x46);
	} else {
		fc8300_write(handle, devid, 0x91, 0x78);
		fc8300_write(handle, devid, 0x15, 0x46);
	}

	fc8300_write(handle, devid, 0x7C, 0x30);
	fc8300_write(handle, devid, 0x7E, 0x05);
	fc8300_write(handle, devid, 0x7F, 0x0A);
	fc8300_write(handle, devid, 0x80, 0x3F);
	fc8300_write(handle, devid, 0x81, 0x3F);

	return BBM_OK;
}

s32 fc8300_es2_vhf(HANDLE handle, DEVICEID devid)
{
	u8 i = 0;
	u8 filter_cal_60 = 0;
	u8 temp = 0;

	filter_cal_60 = fc8300_xtal_freq * 72 / 30000;

	fc8300_write(handle, devid, 0x00, 0x00);
	fc8300_write(handle, devid, 0x02, 0x03);
	fc8300_write(handle, devid, 0x03, 0x00);
	fc8300_write(handle, devid, 0x08, 0xe5);

	fc8300_write(handle, devid, 0x1c, 0x00);
	fc8300_write(handle, devid, 0x1d, 0x00);
	fc8300_write(handle, devid, 0x21, 0x20);
	fc8300_write(handle, devid, 0x24, 0x98);

	fc8300_write(handle, devid, 0x5f, 0x40);
	fc8300_write(handle, devid, 0x3f, 0x01);
	fc8300_write(handle, devid, 0x41, filter_cal_60);
	fc8300_write(handle, devid, 0x3f, 0x00);

	fc8300_write(handle, devid, 0x3c, 0xff);
	fc8300_write(handle, devid, 0x3d, 0xff);

	fc8300_write(handle, devid, 0x70, 0x5f);
	fc8300_write(handle, devid, 0x04, 0x10);

	fc8300_write(handle, devid, 0xed, 0xcc);

	fc8300_write(handle, devid, 0x82, 0x88);
	fc8300_write(handle, devid, 0x7c, 0x01);
	fc8300_write(handle, devid, 0x84, 0x0f);
	fc8300_write(handle, devid, 0x85, 0x0a);
	fc8300_write(handle, devid, 0x86, 0x1f);
	fc8300_write(handle, devid, 0x87, 0x0f);
	fc8300_write(handle, devid, 0x88, 0x0f);
	fc8300_write(handle, devid, 0x89, 0x0b);
	fc8300_write(handle, devid, 0x8a, 0x15);
	fc8300_write(handle, devid, 0x8b, 0x0b);
	fc8300_write(handle, devid, 0x78, 0x30);
	fc8300_write(handle, devid, 0x7e, 0x05);
	fc8300_write(handle, devid, 0x7f, 0x0a);
	fc8300_write(handle, devid, 0xb2, 0x00);
	fc8300_write(handle, devid, 0xb3, 0x0c);
	fc8300_write(handle, devid, 0xb4, 0x00);
	fc8300_write(handle, devid, 0xb5, 0x0a);
	fc8300_write(handle, devid, 0xb6, 0x00);
	fc8300_write(handle, devid, 0xb7, 0x0a);
	fc8300_write(handle, devid, 0xb8, 0x00);
	fc8300_write(handle, devid, 0xb9, 0x0a);
	fc8300_write(handle, devid, 0xba, 0x00);
	fc8300_write(handle, devid, 0xbb, 0x0a);
	fc8300_write(handle, devid, 0xbc, 0x00);
	fc8300_write(handle, devid, 0xbd, 0x0a);
	fc8300_write(handle, devid, 0xbe, 0x00);
	fc8300_write(handle, devid, 0xbf, 0x0a);
	fc8300_write(handle, devid, 0xc0, 0x0a);
	fc8300_write(handle, devid, 0xc1, 0x00);
	fc8300_write(handle, devid, 0xc2, 0x13);
	fc8300_write(handle, devid, 0xc3, 0x00);
	fc8300_write(handle, devid, 0xf1, 0x53);

	fc8300_write(handle, devid, 0xd3, 0x00);
	fc8300_write(handle, devid, 0xd4, 0x02);
	fc8300_write(handle, devid, 0xd5, 0x04);
	fc8300_write(handle, devid, 0xd6, 0x14);
	fc8300_write(handle, devid, 0xd7, 0x34);
	fc8300_write(handle, devid, 0xd8, 0x36);
	fc8300_write(handle, devid, 0xd9, 0x38);
	fc8300_write(handle, devid, 0xda, 0x78);
	fc8300_write(handle, devid, 0xdb, 0xb8);
	fc8300_write(handle, devid, 0xdc, 0xf8);
	fc8300_write(handle, devid, 0xdd, 0xf8);

	fc8300_write(handle, devid, 0x33, 0x88);
	fc8300_write(handle, devid, 0x34, 0x86);
	fc8300_write(handle, devid, 0x37, 0x64);
	fc8300_write(handle, devid, 0x38, 0x44);
	fc8300_write(handle, devid, 0x39, 0x02);
	fc8300_write(handle, devid, 0x3e, 0xab);

	fc8300_write(handle, devid, 0x13, 0x07);
	fc8300_write(handle, devid, 0x15, 0x47);
	fc8300_write(handle, devid, 0x84, 0x0f);
	fc8300_write(handle, devid, 0x85, 0x0a);
	fc8300_write(handle, devid, 0x86, 0x3f);
	fc8300_write(handle, devid, 0x87, 0x2f);
	fc8300_write(handle, devid, 0x8f, 0xb6);
	fc8300_write(handle, devid, 0x90, 0xb2);

	fc8300_write(handle, devid, 0x91, 0xa8);
	fc8300_write(handle, devid, 0x16, 0x42);
	fc8300_write(handle, devid, 0x17, 0x42);
	fc8300_write(handle, devid, 0x1a, 0x42);
	fc8300_write(handle, devid, 0x1f, 0x77);
	fc8300_write(handle, devid, 0x20, 0x42);
	fc8300_write(handle, devid, 0xe9, 0x79);
	fc8300_write(handle, devid, 0xef, 0xb7);

	for (i = 0; i < 30; i++) {
		msWait(1);

		fc8300_read(handle, devid, 0x43, &temp);

		if (((temp >> 4) & 0x01) == 1)
			break;
	}

	fc8300_write(handle, devid, 0x5f, 0x00);

	fc8300_write(handle, devid, 0x78, 0xf0);

	msWait(5);

	fc8300_write(handle, devid, 0x78, 0x30);

	fc8300_write(handle, devid, 0xfb, FC8300_BAND_WIDTH);
	fc8300_write(handle, devid, 0xfc, broadcast_type);
	fc8300_write(handle, devid, 0xfd, (u8)(fc8300_xtal_freq / 1000));
	fc8300_write(handle, devid, 0xfe, DRIVER_VERSION);

	if (fc8300_xtal_freq < 20000)
		fc8300_write(handle, devid, 0xF3, 0x55);
	else if (fc8300_xtal_freq > 20000)
		fc8300_write(handle, devid, 0xF3, 0x54);

	/* PLL AGC All Broadcast */
	fc8300_write(handle, devid, 0xF4, 0xFF);
	fc8300_write(handle, devid, 0xB5, 0x0A);
	fc8300_write(handle, devid, 0xB7, 0x0A);
	fc8300_write(handle, devid, 0xB9, 0x0A);
	fc8300_write(handle, devid, 0xBB, 0x0A);

	fc8300_write(handle, devid, 0x50, 0xff);
	fc8300_write(handle, devid, 0x5f, 0x00);
	fc8300_write(handle, devid, 0x6a, 0x00);
	fc8300_write(handle, devid, 0x6b, 0x99);
	fc8300_write(handle, devid, 0x6c, 0x99);
	fc8300_write(handle, devid, 0x6e, 0x03);
	fc8300_write(handle, devid, 0x73, 0x66);
	fc8300_write(handle, devid, 0x74, 0x66);
	fc8300_write(handle, devid, 0x75, 0x30);

	fc8300_write(handle, devid, 0xfa, 0xff);

	return BBM_OK;

}


s32 fc8300_es2_uhf(HANDLE handle, DEVICEID devid)
{

	u8 i = 0;
	u8 filter_cal_60 = 0;
	u8 temp = 0;

	filter_cal_60 = fc8300_xtal_freq * 72 / 30000;

	fc8300_write(handle, devid, 0x00, 0x00);
	fc8300_write(handle, devid, 0x02, 0x01);
	fc8300_write(handle, devid, 0x03, 0x04);
	fc8300_write(handle, devid, 0x08, 0xe5);

	fc8300_write(handle, devid, 0x1d, 0x10);
	fc8300_write(handle, devid, 0x21, 0x30);
	fc8300_write(handle, devid, 0x24, 0x98);

	fc8300_write(handle, devid, 0x5f, 0x40);
	fc8300_write(handle, devid, 0x3f, 0x01);
	fc8300_write(handle, devid, 0x41, filter_cal_60);
	fc8300_write(handle, devid, 0x3f, 0x00);

	fc8300_write(handle, devid, 0x3c, 0xff);
	fc8300_write(handle, devid, 0x3d, 0xff);

	fc8300_write(handle, devid, 0x54, 0x80);
	fc8300_write(handle, devid, 0x70, 0x5f);
	fc8300_write(handle, devid, 0x04, 0x10);

	fc8300_write(handle, devid, 0xed, 0xcc);

	fc8300_write(handle, devid, 0x82, 0x88);
	fc8300_write(handle, devid, 0x7c, 0x01);
	fc8300_write(handle, devid, 0x88, 0x0f);
	fc8300_write(handle, devid, 0x89, 0x0b);
	fc8300_write(handle, devid, 0x8a, 0x15);
	fc8300_write(handle, devid, 0x8b, 0x0b);
	fc8300_write(handle, devid, 0x78, 0x30);
	fc8300_write(handle, devid, 0x7e, 0x05);
	fc8300_write(handle, devid, 0x7f, 0x0a);
	fc8300_write(handle, devid, 0xb2, 0x00);
	fc8300_write(handle, devid, 0xb3, 0x0c);
	fc8300_write(handle, devid, 0xb4, 0x00);
	fc8300_write(handle, devid, 0xb5, 0x0a);
	fc8300_write(handle, devid, 0xb6, 0x00);
	fc8300_write(handle, devid, 0xb7, 0x0a);
	fc8300_write(handle, devid, 0xb8, 0x00);
	fc8300_write(handle, devid, 0xb9, 0x0a);
	fc8300_write(handle, devid, 0xba, 0x00);
	fc8300_write(handle, devid, 0xbb, 0x0a);
	fc8300_write(handle, devid, 0xbc, 0x00);
	fc8300_write(handle, devid, 0xbd, 0x0a);
	fc8300_write(handle, devid, 0xbe, 0x00);
	fc8300_write(handle, devid, 0xbf, 0x0a);
	fc8300_write(handle, devid, 0xc0, 0x0a);
	fc8300_write(handle, devid, 0xc1, 0x00);
	fc8300_write(handle, devid, 0xc2, 0x13);
	fc8300_write(handle, devid, 0xc3, 0x00);
	fc8300_write(handle, devid, 0xf1, 0x53);

	fc8300_write(handle, devid, 0xd3, 0x00);
	fc8300_write(handle, devid, 0xd4, 0x02);
	fc8300_write(handle, devid, 0xd5, 0x04);
	fc8300_write(handle, devid, 0xd6, 0x14);
	fc8300_write(handle, devid, 0xd7, 0x34);
	fc8300_write(handle, devid, 0xd8, 0x36);
	fc8300_write(handle, devid, 0xd9, 0x38);
	fc8300_write(handle, devid, 0xda, 0x78);
	fc8300_write(handle, devid, 0xdb, 0xb8);
	fc8300_write(handle, devid, 0xdc, 0xf8);
	fc8300_write(handle, devid, 0xdd, 0xf8);

	fc8300_write(handle, devid, 0x91, 0xa8);
	fc8300_write(handle, devid, 0x33, 0x88);
	fc8300_write(handle, devid, 0x34, 0x86);
	fc8300_write(handle, devid, 0x37, 0x65);
	fc8300_write(handle, devid, 0x38, 0x55);
	fc8300_write(handle, devid, 0x39, 0x02);
	fc8300_write(handle, devid, 0x3e, 0xab);

	fc8300_write(handle, devid, 0x13, 0x07);
	fc8300_write(handle, devid, 0x15, 0x47);
	fc8300_write(handle, devid, 0x84, 0x0f);
	fc8300_write(handle, devid, 0x85, 0x0a);
	fc8300_write(handle, devid, 0x86, 0x3f);
	fc8300_write(handle, devid, 0x87, 0x2f);
	fc8300_write(handle, devid, 0x8f, 0xb6);
	fc8300_write(handle, devid, 0x90, 0xb2);
	fc8300_write(handle, devid, 0xe9, 0x79);
	fc8300_write(handle, devid, 0xef, 0xb7);

	for (i = 0; i < 30; i++) {
		msWait(1);

		fc8300_read(handle, devid, 0x43, &temp);

		if (((temp >> 4) & 0x01) == 1)
			break;
	}

	fc8300_write(handle, devid, 0x5f, 0x00);

	fc8300_write(handle, devid, 0x78, 0xf0);

	msWait(5);

	fc8300_write(handle, devid, 0x78, 0x30);

	if (fc8300_xtal_freq == 19200)
		fc8300_write(handle, devid, 0x53, 0x16);
	else
		fc8300_write(handle, devid, 0x53, 0x1a);


	fc8300_write(handle, devid, 0xfb, FC8300_BAND_WIDTH);
	fc8300_write(handle, devid, 0xfc, broadcast_type);
	fc8300_write(handle, devid, 0xfd, (u8)(fc8300_xtal_freq / 1000));
	fc8300_write(handle, devid, 0xfe, DRIVER_VERSION);

	if (fc8300_xtal_freq < 20000)
		fc8300_write(handle, devid, 0xF3, 0x55);
	else if (fc8300_xtal_freq > 20000)
		fc8300_write(handle, devid, 0xF3, 0x54);

	/* PLL AGC All Broadcast */
	fc8300_write(handle, devid, 0xF4, 0xFF);
	fc8300_write(handle, devid, 0xB5, 0x0A);
	fc8300_write(handle, devid, 0xB7, 0x0A);
	fc8300_write(handle, devid, 0xB9, 0x0A);
	fc8300_write(handle, devid, 0xBB, 0x0A);

	fc8300_write(handle, devid, 0x50, 0xff);
	fc8300_write(handle, devid, 0x5f, 0x00);
	fc8300_write(handle, devid, 0x6a, 0x00);
	fc8300_write(handle, devid, 0x6b, 0x99);
	fc8300_write(handle, devid, 0x6c, 0x99);
	fc8300_write(handle, devid, 0x6e, 0x03);
	fc8300_write(handle, devid, 0x73, 0x66);
	fc8300_write(handle, devid, 0x74, 0x66);
	fc8300_write(handle, devid, 0x75, 0x30);

	fc8300_write(handle, devid, 0xfa, 0x20);

	fc8300_tuner_set_pll(handle, DIV_MASTER, 545143, 0);

	fc8300_write(handle, devid, 0x1e, 0x10);
	fc8300_write(handle, devid, 0x1f, 0xf2);
	fc8300_write(handle, devid, 0x78, 0x06);

	fc8300_write(handle, devid, 0x04, 0x12);
	fc8300_write(handle, devid, 0x8d, 0x08);
	fc8300_write(handle, devid, 0x8e, 0x13);

	fc8300_write(handle, devid, 0x13, 0x07);

	fc8300_write(handle, devid, 0x22, 0x01);
	fc8300_write(handle, devid, 0x24, 0x11);
	fc8300_write(handle, devid, 0x23, 0x17);

	if (broad_check == 1) { /* BROADCAST Control */

		if ((device_set & 0x01) == 1)
			fc_cal_value[0] = (tf_cal(handle, DIV_MASTER) -
								7);
		if ((device_set & 0x02) == 2)
			fc_cal_value[1] = (tf_cal(handle, DIV_SLAVE0) -
								7);
		if ((device_set & 0x04) == 4)
			fc_cal_value[2] = (tf_cal(handle, DIV_SLAVE1) -
								7);
		if ((device_set & 0x08) == 8)
			fc_cal_value[3] = (tf_cal(handle, DIV_SLAVE2) -
								7);

	} else if (broad_check == 0) { /* Single Control */

		fc_cal_value[0] = (tf_cal(handle, devid) - 7);

	}

	fc8300_write(handle, devid, 0x1e, 0x00);
	fc8300_write(handle, devid, 0x1f, 0x72);
	fc8300_write(handle, devid, 0x78, 0x30);
	fc8300_write(handle, devid, 0x04, 0x10);
	fc8300_write(handle, devid, 0x22, 0x00);

	fc8300_write(handle, devid, 0xfa, 0xff);

	return BBM_OK;
}




s32 fc8300_es2_tuner_init(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast)
{
	u8 temp;
	u8 filter_cal_09;
	u8 filter_cal_18;
	u8 filter_cal_60;
	s32 i, cal_temp;

	broadcast_type = broadcast;

	device_set = 0;

	if (devid == DIV_BROADCAST)
		broad_check = 1;
	else
		broad_check = 0;

#if defined(BBM_4_DIVERSITY)
	fc8300_read(handle, DIV_SLAVE2, 0x01, &temp);
	if (temp == 0xaa) {
		device_set = device_set + 8;
		bc_type[3] = broadcast_type;
	}
	fc8300_read(handle, DIV_SLAVE1, 0x01, &temp);
	if (temp == 0xaa) {
		device_set = device_set + 4;
		bc_type[2] = broadcast_type;
	}
	fc8300_read(handle, DIV_SLAVE0, 0x01, &temp);
	if (temp == 0xaa) {
		device_set = device_set + 2;
		bc_type[1] = broadcast_type;
	}
#elif defined(BBM_2_DIVERSITY)
	fc8300_read(handle, DIV_SLAVE0, 0x01, &temp);
	if (temp == 0xaa) {
		device_set = device_set + 2;
		bc_type[1] = broadcast_type;
	}
#endif /* #if defined(BBM_4_DIVERSITY) */
	fc8300_read(handle, DIV_MASTER, 0x01, &temp);
	if (temp == 0xaa) {
		device_set = device_set + 1;
		bc_type[0] = broadcast_type;
	}

	/*
	 * ISDBT_1SEG = 0  ==> 0.9
	 * ISDBTMM_1SEG = 1 ==> 0.9
	 * ISDBTSB_1SEG = 2 ==> 0.9
	 * ISDBTSB_3SEG = 3 ==> 1.8
	 * ISDBT_13SEG = 4  ==> 6 / 7 / 8
	 * ISDBTMM_13SEG = 5 ==> 6 / 7 / 8
	 * CATV ==> 6 / 7 / 8
	 */

	if (fc8300_xtal_freq < 32000) {
		cal_temp = fc8300_xtal_freq * 72 / 9000;
		filter_cal_09 = (u8) cal_temp;
	} else {
		filter_cal_09 = 0x5f;
	}
	cal_temp = fc8300_xtal_freq * 72 / 18000;
	filter_cal_18 = (u8) cal_temp;

#if (BBM_BAND_WIDTH == 6)
	cal_temp = fc8300_xtal_freq * 72 / 30000;
#elif (BBM_BAND_WIDTH == 7)
	cal_temp = fc8300_xtal_freq * 72 / 35000;
#else /* BBM_BAND_WIDTH == 8 */
	cal_temp = fc8300_xtal_freq * 72 / 40000;
#endif /* #if (BBM_BAND_WIDTH == 6) */
	filter_cal_60 = (u8) cal_temp;

	if (broadcast_type == ISDBT_1SEG) {
		fc8300_write(handle, devid, 0x00, 0x00);
		fc8300_write(handle, devid, 0x02, 0x01);
		fc8300_write(handle, devid, 0x03, 0x04);
		fc8300_write(handle, devid, 0x08, 0xe5);

		fc8300_write(handle, devid, 0x1d, 0x10);
		fc8300_write(handle, devid, 0x21, 0x30);
		fc8300_write(handle, devid, 0x24, 0x98);

		fc8300_write(handle, devid, 0x5f, 0x40);

		if (fc8300_xtal_freq < 32000) {
			fc8300_write(handle, devid, 0x3f, 0x01);
			fc8300_write(handle, devid, 0x41, filter_cal_09);
			fc8300_write(handle, devid, 0x3f, 0x00);
		} else {
			fc8300_write(handle, devid, 0x3f, 0x02);
			fc8300_write(handle, devid, 0x42, filter_cal_09);
		}

		fc8300_write(handle, devid, 0x3c, 0xff);
		fc8300_write(handle, devid, 0x3d, 0xff);

		fc8300_write(handle, devid, 0x54, 0x80);
		fc8300_write(handle, devid, 0x70, 0x5f);
		fc8300_write(handle, devid, 0x04, 0x10);

		fc8300_write(handle, devid, 0xed, 0xcc);

		fc8300_write(handle, devid, 0x82, 0x88);
		fc8300_write(handle, devid, 0x7c, 0x01);
		fc8300_write(handle, devid, 0x84, 0x1a);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x1f);
		fc8300_write(handle, devid, 0x87, 0x0f);
		fc8300_write(handle, devid, 0x88, 0x0f);
		fc8300_write(handle, devid, 0x89, 0x0b);
		fc8300_write(handle, devid, 0x8a, 0x15);
		fc8300_write(handle, devid, 0x8b, 0x0b);
		fc8300_write(handle, devid, 0x78, 0x30);
		fc8300_write(handle, devid, 0x7e, 0x05);
		fc8300_write(handle, devid, 0x7f, 0x0a);
		fc8300_write(handle, devid, 0xb2, 0x00);
		fc8300_write(handle, devid, 0xb3, 0x0c);
		fc8300_write(handle, devid, 0xb4, 0x00);
		fc8300_write(handle, devid, 0xb5, 0x0a);
		fc8300_write(handle, devid, 0xb6, 0x00);
		fc8300_write(handle, devid, 0xb7, 0x0a);
		fc8300_write(handle, devid, 0xb8, 0x00);
		fc8300_write(handle, devid, 0xb9, 0x0a);
		fc8300_write(handle, devid, 0xba, 0x00);
		fc8300_write(handle, devid, 0xbb, 0x0a);
		fc8300_write(handle, devid, 0xbc, 0x00);
		fc8300_write(handle, devid, 0xbd, 0x0a);
		fc8300_write(handle, devid, 0xbe, 0x00);
		fc8300_write(handle, devid, 0xbf, 0x0a);
		fc8300_write(handle, devid, 0xc0, 0x0a);
		fc8300_write(handle, devid, 0xc1, 0x00);
		fc8300_write(handle, devid, 0xc2, 0x13);
		fc8300_write(handle, devid, 0xc3, 0x00);
		fc8300_write(handle, devid, 0xf1, 0x53);

		fc8300_write(handle, devid, 0xd3, 0x00);
		fc8300_write(handle, devid, 0xd4, 0x02);
		fc8300_write(handle, devid, 0xd5, 0x04);
		fc8300_write(handle, devid, 0xd6, 0x14);
		fc8300_write(handle, devid, 0xd7, 0x34);
		fc8300_write(handle, devid, 0xd8, 0x36);
		fc8300_write(handle, devid, 0xd9, 0x38);
		fc8300_write(handle, devid, 0xda, 0x78);
		fc8300_write(handle, devid, 0xdb, 0xb8);
		fc8300_write(handle, devid, 0xdc, 0xf8);
		fc8300_write(handle, devid, 0xdd, 0xf8);

		fc8300_write(handle, devid, 0x91, 0x68);
		fc8300_write(handle, devid, 0x19, 0x21);
		fc8300_write(handle, devid, 0x1f, 0x72);
		fc8300_write(handle, devid, 0x20, 0x22);
		fc8300_write(handle, devid, 0x33, 0x83);
		fc8300_write(handle, devid, 0x34, 0x44);
		fc8300_write(handle, devid, 0x37, 0x65);
		fc8300_write(handle, devid, 0x38, 0x55);
		fc8300_write(handle, devid, 0x39, 0x02);
		fc8300_write(handle, devid, 0x3e, 0xab);
		fc8300_write(handle, devid, 0x3c, 0xaa);
		fc8300_write(handle, devid, 0x3d, 0xaa);

		fc8300_write(handle, devid, 0x13, 0x07);
		fc8300_write(handle, devid, 0x84, 0x14);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x3f);
		fc8300_write(handle, devid, 0x87, 0x2f);
		fc8300_write(handle, devid, 0x8f, 0xb6);
		fc8300_write(handle, devid, 0x90, 0xb2);
		fc8300_write(handle, devid, 0xe9, 0x79);
		fc8300_write(handle, devid, 0xef, 0xb7);

		for (i = 0; i < 30; i++) {
			msWait(1);

			fc8300_read(handle, devid, 0x43, &temp);

			if (((temp >> 4) & 0x01) == 1)
				break;
		}

		fc8300_write(handle, devid, 0x5f, 0x00);

		fc8300_write(handle, devid, 0x78, 0xf0);

		msWait(5);

		fc8300_write(handle, devid, 0x78, 0x30);

		if (fc8300_xtal_freq == 19200)
			fc8300_write(handle, devid, 0x53, 0x16);
		else
			fc8300_write(handle, devid, 0x53, 0x1a);

	} else if (broadcast_type == ISDBTMM_1SEG) {
		fc8300_write(handle, devid, 0x00, 0x00);
		fc8300_write(handle, devid, 0x02, 0x03);
		fc8300_write(handle, devid, 0x03, 0x00);
		fc8300_write(handle, devid, 0x08, 0xe5);

		fc8300_write(handle, devid, 0x1c, 0x00);
		fc8300_write(handle, devid, 0x1d, 0x00);
		fc8300_write(handle, devid, 0x21, 0x20);
		fc8300_write(handle, devid, 0x24, 0x98);

		fc8300_write(handle, devid, 0x5f, 0x40);

		if (fc8300_xtal_freq < 32000) {
			fc8300_write(handle, devid, 0x3f, 0x01);
			fc8300_write(handle, devid, 0x41, filter_cal_09);
			fc8300_write(handle, devid, 0x3f, 0x00);
		} else {
			fc8300_write(handle, devid, 0x3f, 0x02);
			fc8300_write(handle, devid, 0x42, filter_cal_09);
		}

		fc8300_write(handle, devid, 0x3c, 0xff);
		fc8300_write(handle, devid, 0x3d, 0xff);

		fc8300_write(handle, devid, 0x70, 0x5f);
		fc8300_write(handle, devid, 0x04, 0x10);

		fc8300_write(handle, devid, 0xed, 0xcc);

		fc8300_write(handle, devid, 0x82, 0x88);
		fc8300_write(handle, devid, 0x7c, 0x01);
		fc8300_write(handle, devid, 0x84, 0x1a);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x1f);
		fc8300_write(handle, devid, 0x87, 0x0f);
		fc8300_write(handle, devid, 0x88, 0x0f);
		fc8300_write(handle, devid, 0x89, 0x0b);
		fc8300_write(handle, devid, 0x8a, 0x15);
		fc8300_write(handle, devid, 0x8b, 0x0b);
		fc8300_write(handle, devid, 0x78, 0x30);
		fc8300_write(handle, devid, 0x7e, 0x05);
		fc8300_write(handle, devid, 0x7f, 0x0a);
		fc8300_write(handle, devid, 0xb2, 0x00);
		fc8300_write(handle, devid, 0xb3, 0x0c);
		fc8300_write(handle, devid, 0xb4, 0x00);
		fc8300_write(handle, devid, 0xb5, 0x0a);
		fc8300_write(handle, devid, 0xb6, 0x00);
		fc8300_write(handle, devid, 0xb7, 0x0a);
		fc8300_write(handle, devid, 0xb8, 0x00);
		fc8300_write(handle, devid, 0xb9, 0x0a);
		fc8300_write(handle, devid, 0xba, 0x00);
		fc8300_write(handle, devid, 0xbb, 0x0a);
		fc8300_write(handle, devid, 0xbc, 0x00);
		fc8300_write(handle, devid, 0xbd, 0x0a);
		fc8300_write(handle, devid, 0xbe, 0x00);
		fc8300_write(handle, devid, 0xbf, 0x0a);
		fc8300_write(handle, devid, 0xc0, 0x0a);
		fc8300_write(handle, devid, 0xc1, 0x00);
		fc8300_write(handle, devid, 0xc2, 0x13);
		fc8300_write(handle, devid, 0xc3, 0x00);
		fc8300_write(handle, devid, 0xf1, 0x53);

		fc8300_write(handle, devid, 0xd3, 0x00);
		fc8300_write(handle, devid, 0xd4, 0x02);
		fc8300_write(handle, devid, 0xd5, 0x04);
		fc8300_write(handle, devid, 0xd6, 0x14);
		fc8300_write(handle, devid, 0xd7, 0x34);
		fc8300_write(handle, devid, 0xd8, 0x36);
		fc8300_write(handle, devid, 0xd9, 0x38);
		fc8300_write(handle, devid, 0xda, 0x78);
		fc8300_write(handle, devid, 0xdb, 0xb8);
		fc8300_write(handle, devid, 0xdc, 0xf8);
		fc8300_write(handle, devid, 0xdd, 0xf8);

		fc8300_write(handle, devid, 0x33, 0x88);
		fc8300_write(handle, devid, 0x34, 0x86);
		fc8300_write(handle, devid, 0x37, 0x64);
		fc8300_write(handle, devid, 0x38, 0x44);
		fc8300_write(handle, devid, 0x39, 0x02);
		fc8300_write(handle, devid, 0x3e, 0xab);

		fc8300_write(handle, devid, 0x13, 0x07);
		fc8300_write(handle, devid, 0x15, 0x46);
		fc8300_write(handle, devid, 0x84, 0x14);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x3f);
		fc8300_write(handle, devid, 0x87, 0x2f);
		fc8300_write(handle, devid, 0x8f, 0xb6);
		fc8300_write(handle, devid, 0x90, 0xb2);

		fc8300_write(handle, devid, 0x91, 0x68);
		fc8300_write(handle, devid, 0x16, 0x42);
		fc8300_write(handle, devid, 0x17, 0x42);
		fc8300_write(handle, devid, 0x1a, 0x42);
		fc8300_write(handle, devid, 0x1f, 0x77);
		fc8300_write(handle, devid, 0x20, 0x42);
		fc8300_write(handle, devid, 0xe9, 0x79);
		fc8300_write(handle, devid, 0xef, 0xb7);

		for (i = 0; i < 30; i++) {
			msWait(1);

			fc8300_read(handle, devid, 0x43, &temp);

			if (((temp >> 4) & 0x01) == 1)
				break;
		}

		fc8300_write(handle, devid, 0x5f, 0x00);

		fc8300_write(handle, devid, 0x78, 0xf0);

		msWait(5);

		fc8300_write(handle, devid, 0x78, 0x30);
	} else if (broadcast_type == ISDBTSB_1SEG) {
		fc8300_write(handle, devid, 0x00, 0x00);
		fc8300_write(handle, devid, 0x02, 0x05);
		fc8300_write(handle, devid, 0x03, 0x04);
		fc8300_write(handle, devid, 0x08, 0x21);

		fc8300_write(handle, devid, 0x1d, 0x00);
		fc8300_write(handle, devid, 0x21, 0x10);
		fc8300_write(handle, devid, 0x24, 0x98);

		fc8300_write(handle, devid, 0x5f, 0x40);

		if (fc8300_xtal_freq < 32000) {
			fc8300_write(handle, devid, 0x3f, 0x01);
			fc8300_write(handle, devid, 0x41, filter_cal_09);
			fc8300_write(handle, devid, 0x3f, 0x00);
		} else {
			fc8300_write(handle, devid, 0x3f, 0x02);
			fc8300_write(handle, devid, 0x42, filter_cal_09);
		}

		for (i = 0; i <  5; i++) {
			msWait(10);

			fc8300_read(handle, devid, 0x43, &temp);

			if (((temp >> 4) & 0x01) == 1)
				break;
		}

		fc8300_write(handle, devid, 0x3c, 0xff);
		fc8300_write(handle, devid, 0x3d, 0xff);

		fc8300_write(handle, devid, 0x70, 0x5f);
		fc8300_write(handle, devid, 0x04, 0x10);

		fc8300_write(handle, devid, 0xed, 0xcc);

		fc8300_write(handle, devid, 0x82, 0x88);
		fc8300_write(handle, devid, 0x7c, 0x01);
		fc8300_write(handle, devid, 0x84, 0x1a);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x1f);
		fc8300_write(handle, devid, 0x87, 0x0f);
		fc8300_write(handle, devid, 0x88, 0x0f);
		fc8300_write(handle, devid, 0x89, 0x0b);
		fc8300_write(handle, devid, 0x8a, 0x15);
		fc8300_write(handle, devid, 0x8b, 0x0b);
		fc8300_write(handle, devid, 0x78, 0x30);
		fc8300_write(handle, devid, 0x7e, 0x05);
		fc8300_write(handle, devid, 0x7f, 0x0a);
		fc8300_write(handle, devid, 0xb2, 0x00);
		fc8300_write(handle, devid, 0xb3, 0x0c);
		fc8300_write(handle, devid, 0xb4, 0x00);
		fc8300_write(handle, devid, 0xb5, 0x0a);
		fc8300_write(handle, devid, 0xb6, 0x00);
		fc8300_write(handle, devid, 0xb7, 0x0a);
		fc8300_write(handle, devid, 0xb8, 0x00);
		fc8300_write(handle, devid, 0xb9, 0x0a);
		fc8300_write(handle, devid, 0xba, 0x00);
		fc8300_write(handle, devid, 0xbb, 0x0a);
		fc8300_write(handle, devid, 0xbc, 0x00);
		fc8300_write(handle, devid, 0xbd, 0x0a);
		fc8300_write(handle, devid, 0xbe, 0x00);
		fc8300_write(handle, devid, 0xbf, 0x0a);
		fc8300_write(handle, devid, 0xc0, 0x0a);
		fc8300_write(handle, devid, 0xc1, 0x00);
		fc8300_write(handle, devid, 0xc2, 0x13);
		fc8300_write(handle, devid, 0xc3, 0x00);
		fc8300_write(handle, devid, 0xf1, 0x53);

		fc8300_write(handle, devid, 0xd3, 0x00);
		fc8300_write(handle, devid, 0xd4, 0x02);
		fc8300_write(handle, devid, 0xd5, 0x04);
		fc8300_write(handle, devid, 0xd6, 0x14);
		fc8300_write(handle, devid, 0xd7, 0x34);
		fc8300_write(handle, devid, 0xd8, 0x36);
		fc8300_write(handle, devid, 0xd9, 0x38);
		fc8300_write(handle, devid, 0xda, 0x78);
		fc8300_write(handle, devid, 0xdb, 0xb8);
		fc8300_write(handle, devid, 0xdc, 0xf8);
		fc8300_write(handle, devid, 0xdd, 0xf8);

		fc8300_write(handle, devid, 0x33, 0x88);
		fc8300_write(handle, devid, 0x34, 0x86);
		fc8300_write(handle, devid, 0x37, 0x64);
		fc8300_write(handle, devid, 0x38, 0x44);
		fc8300_write(handle, devid, 0x39, 0x02);
		fc8300_write(handle, devid, 0x3e, 0xab);

		fc8300_write(handle, devid, 0x13, 0x07);
		fc8300_write(handle, devid, 0x15, 0x46);
		fc8300_write(handle, devid, 0x84, 0x14);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x3f);
		fc8300_write(handle, devid, 0x87, 0x2f);
		fc8300_write(handle, devid, 0x8f, 0xb6);
		fc8300_write(handle, devid, 0x90, 0xb2);

		fc8300_write(handle, devid, 0x91, 0x68);
		fc8300_write(handle, devid, 0x16, 0x42);
		fc8300_write(handle, devid, 0x17, 0x42);
		fc8300_write(handle, devid, 0x1a, 0x42);
		fc8300_write(handle, devid, 0x1f, 0x77);
		fc8300_write(handle, devid, 0x20, 0x42);
		fc8300_write(handle, devid, 0xe9, 0x79);
		fc8300_write(handle, devid, 0xef, 0xb7);

		fc8300_write(handle, devid, 0x5f, 0x00);

		fc8300_write(handle, devid, 0x78, 0xf0);

		msWait(5);

		fc8300_write(handle, devid, 0x78, 0x30);

	} else if (broadcast_type == ISDBTSB_3SEG) {
		fc8300_write(handle, devid, 0x00, 0x00);
		fc8300_write(handle, devid, 0x02, 0x05);
		fc8300_write(handle, devid, 0x03, 0x04);
		fc8300_write(handle, devid, 0x08, 0x21);

		fc8300_write(handle, devid, 0x1d, 0x00);
		fc8300_write(handle, devid, 0x21, 0x10);
		fc8300_write(handle, devid, 0x24, 0x98);

		fc8300_write(handle, devid, 0x5f, 0x40);
		fc8300_write(handle, devid, 0x3f, 0x01);
		fc8300_write(handle, devid, 0x41, filter_cal_18);
		fc8300_write(handle, devid, 0x3f, 0x00);

		fc8300_write(handle, devid, 0x3c, 0xff);
		fc8300_write(handle, devid, 0x3d, 0xff);

		fc8300_write(handle, devid, 0x70, 0x5f);
		fc8300_write(handle, devid, 0x04, 0x10);

		fc8300_write(handle, devid, 0xed, 0xcc);

		fc8300_write(handle, devid, 0x82, 0x88);
		fc8300_write(handle, devid, 0x7c, 0x01);
		fc8300_write(handle, devid, 0x84, 0x1a);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x1f);
		fc8300_write(handle, devid, 0x87, 0x0f);
		fc8300_write(handle, devid, 0x88, 0x0f);
		fc8300_write(handle, devid, 0x89, 0x0b);
		fc8300_write(handle, devid, 0x8a, 0x15);
		fc8300_write(handle, devid, 0x8b, 0x0b);
		fc8300_write(handle, devid, 0x78, 0x30);
		fc8300_write(handle, devid, 0x7e, 0x05);
		fc8300_write(handle, devid, 0x7f, 0x0a);
		fc8300_write(handle, devid, 0xb2, 0x00);
		fc8300_write(handle, devid, 0xb3, 0x0c);
		fc8300_write(handle, devid, 0xb4, 0x00);
		fc8300_write(handle, devid, 0xb5, 0x0a);
		fc8300_write(handle, devid, 0xb6, 0x00);
		fc8300_write(handle, devid, 0xb7, 0x0a);
		fc8300_write(handle, devid, 0xb8, 0x00);
		fc8300_write(handle, devid, 0xb9, 0x0a);
		fc8300_write(handle, devid, 0xba, 0x00);
		fc8300_write(handle, devid, 0xbb, 0x0a);
		fc8300_write(handle, devid, 0xbc, 0x00);
		fc8300_write(handle, devid, 0xbd, 0x0a);
		fc8300_write(handle, devid, 0xbe, 0x00);
		fc8300_write(handle, devid, 0xbf, 0x0a);
		fc8300_write(handle, devid, 0xc0, 0x0a);
		fc8300_write(handle, devid, 0xc1, 0x00);
		fc8300_write(handle, devid, 0xc2, 0x13);
		fc8300_write(handle, devid, 0xc3, 0x00);
		fc8300_write(handle, devid, 0xf1, 0x53);

		fc8300_write(handle, devid, 0xd3, 0x00);
		fc8300_write(handle, devid, 0xd4, 0x02);
		fc8300_write(handle, devid, 0xd5, 0x04);
		fc8300_write(handle, devid, 0xd6, 0x14);
		fc8300_write(handle, devid, 0xd7, 0x34);
		fc8300_write(handle, devid, 0xd8, 0x36);
		fc8300_write(handle, devid, 0xd9, 0x38);
		fc8300_write(handle, devid, 0xda, 0x78);
		fc8300_write(handle, devid, 0xdb, 0xb8);
		fc8300_write(handle, devid, 0xdc, 0xf8);
		fc8300_write(handle, devid, 0xdd, 0xf8);

		fc8300_write(handle, devid, 0x33, 0x88);
		fc8300_write(handle, devid, 0x34, 0x86);
		fc8300_write(handle, devid, 0x37, 0x64);
		fc8300_write(handle, devid, 0x38, 0x44);
		fc8300_write(handle, devid, 0x39, 0x02);
		fc8300_write(handle, devid, 0x3e, 0xab);

		fc8300_write(handle, devid, 0x13, 0x07);
		fc8300_write(handle, devid, 0x15, 0x46);
		fc8300_write(handle, devid, 0x84, 0x14);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x3f);
		fc8300_write(handle, devid, 0x87, 0x2f);
		fc8300_write(handle, devid, 0x8f, 0xb6);
		fc8300_write(handle, devid, 0x90, 0xb2);

		fc8300_write(handle, devid, 0x91, 0x68);
		fc8300_write(handle, devid, 0x16, 0x42);
		fc8300_write(handle, devid, 0x17, 0x42);
		fc8300_write(handle, devid, 0x1a, 0x42);
		fc8300_write(handle, devid, 0x1f, 0x77);
		fc8300_write(handle, devid, 0x20, 0x42);
		fc8300_write(handle, devid, 0xe9, 0x79);
		fc8300_write(handle, devid, 0xef, 0xb7);

		for (i = 0; i < 30; i++) {
			msWait(1);

			fc8300_read(handle, devid, 0x43, &temp);

			if (((temp >> 4) & 0x01) == 1)
				break;
		}

		fc8300_write(handle, devid, 0x5f, 0x00);

		fc8300_write(handle, devid, 0x78, 0xf0);

		msWait(5);

		fc8300_write(handle, devid, 0x78, 0x30);

	} else if (broadcast_type == ISDBT_13SEG) {
		fc8300_write(handle, devid, 0x00, 0x00);
		fc8300_write(handle, devid, 0x02, 0x01);
		fc8300_write(handle, devid, 0x03, 0x04);
		fc8300_write(handle, devid, 0x08, 0xe5);

		fc8300_write(handle, devid, 0x1d, 0x10);
		fc8300_write(handle, devid, 0x21, 0x30);
		fc8300_write(handle, devid, 0x24, 0x98);

		fc8300_write(handle, devid, 0x5f, 0x40);
		fc8300_write(handle, devid, 0x3f, 0x01);
		fc8300_write(handle, devid, 0x41, filter_cal_60);
		fc8300_write(handle, devid, 0x3f, 0x00);

		fc8300_write(handle, devid, 0x3c, 0xff);
		fc8300_write(handle, devid, 0x3d, 0xff);

		fc8300_write(handle, devid, 0x54, 0x80);
		fc8300_write(handle, devid, 0x70, 0x5f);
		fc8300_write(handle, devid, 0x04, 0x10);

		fc8300_write(handle, devid, 0xed, 0xcc);

		fc8300_write(handle, devid, 0x82, 0x88);
		fc8300_write(handle, devid, 0x7c, 0x01);
		fc8300_write(handle, devid, 0x88, 0x0f);
		fc8300_write(handle, devid, 0x89, 0x0b);
		fc8300_write(handle, devid, 0x8a, 0x15);
		fc8300_write(handle, devid, 0x8b, 0x0b);
		fc8300_write(handle, devid, 0x78, 0x30);
		fc8300_write(handle, devid, 0x7e, 0x05);
		fc8300_write(handle, devid, 0x7f, 0x0a);
		fc8300_write(handle, devid, 0xb2, 0x00);
		fc8300_write(handle, devid, 0xb3, 0x0c);
		fc8300_write(handle, devid, 0xb4, 0x00);
		fc8300_write(handle, devid, 0xb5, 0x0a);
		fc8300_write(handle, devid, 0xb6, 0x00);
		fc8300_write(handle, devid, 0xb7, 0x0a);
		fc8300_write(handle, devid, 0xb8, 0x00);
		fc8300_write(handle, devid, 0xb9, 0x0a);
		fc8300_write(handle, devid, 0xba, 0x00);
		fc8300_write(handle, devid, 0xbb, 0x0a);
		fc8300_write(handle, devid, 0xbc, 0x00);
		fc8300_write(handle, devid, 0xbd, 0x0a);
		fc8300_write(handle, devid, 0xbe, 0x00);
		fc8300_write(handle, devid, 0xbf, 0x0a);
		fc8300_write(handle, devid, 0xc0, 0x0a);
		fc8300_write(handle, devid, 0xc1, 0x00);
		fc8300_write(handle, devid, 0xc2, 0x13);
		fc8300_write(handle, devid, 0xc3, 0x00);
		fc8300_write(handle, devid, 0xf1, 0x53);

		fc8300_write(handle, devid, 0xd3, 0x00);
		fc8300_write(handle, devid, 0xd4, 0x02);
		fc8300_write(handle, devid, 0xd5, 0x04);
		fc8300_write(handle, devid, 0xd6, 0x14);
		fc8300_write(handle, devid, 0xd7, 0x34);
		fc8300_write(handle, devid, 0xd8, 0x36);
		fc8300_write(handle, devid, 0xd9, 0x38);
		fc8300_write(handle, devid, 0xda, 0x78);
		fc8300_write(handle, devid, 0xdb, 0xb8);
		fc8300_write(handle, devid, 0xdc, 0xf8);
		fc8300_write(handle, devid, 0xdd, 0xf8);

		fc8300_write(handle, devid, 0x91, 0x78);
		fc8300_write(handle, devid, 0x33, 0x88);
		fc8300_write(handle, devid, 0x34, 0x86);
		fc8300_write(handle, devid, 0x37, 0x65);
		fc8300_write(handle, devid, 0x38, 0x55);
		fc8300_write(handle, devid, 0x39, 0x02);
		fc8300_write(handle, devid, 0x3e, 0xab);

		fc8300_write(handle, devid, 0x13, 0x07);
		fc8300_write(handle, devid, 0x15, 0x46);
		fc8300_write(handle, devid, 0x84, 0x14);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x3f);
		fc8300_write(handle, devid, 0x87, 0x2f);
		fc8300_write(handle, devid, 0x8f, 0xb6);
		fc8300_write(handle, devid, 0x90, 0xb2);
		fc8300_write(handle, devid, 0xe9, 0x79);
		fc8300_write(handle, devid, 0xef, 0xb7);

		for (i = 0; i < 30; i++) {
			msWait(1);

			fc8300_read(handle, devid, 0x43, &temp);

			if (((temp >> 4) & 0x01) == 1)
				break;
		}

		fc8300_write(handle, devid, 0x5f, 0x00);

		fc8300_write(handle, devid, 0x78, 0xf0);

		msWait(5);

		fc8300_write(handle, devid, 0x78, 0x30);

		if (fc8300_xtal_freq == 19200)
			fc8300_write(handle, devid, 0x53, 0x16);
		else
			fc8300_write(handle, devid, 0x53, 0x1a);

	} else if (broadcast_type == ISDBTMM_13SEG) {
		fc8300_write(handle, devid, 0x00, 0x00);
		fc8300_write(handle, devid, 0x02, 0x03);
		fc8300_write(handle, devid, 0x03, 0x00);
		fc8300_write(handle, devid, 0x08, 0xe5);

		fc8300_write(handle, devid, 0x1c, 0x00);
		fc8300_write(handle, devid, 0x1d, 0x00);
		fc8300_write(handle, devid, 0x21, 0x20);
		fc8300_write(handle, devid, 0x24, 0x98);

		fc8300_write(handle, devid, 0x5f, 0x40);
		fc8300_write(handle, devid, 0x3f, 0x01);
		fc8300_write(handle, devid, 0x41, filter_cal_60);
		fc8300_write(handle, devid, 0x3f, 0x00);

		fc8300_write(handle, devid, 0x3c, 0xff);
		fc8300_write(handle, devid, 0x3d, 0xff);

		fc8300_write(handle, devid, 0x70, 0x5f);
		fc8300_write(handle, devid, 0x04, 0x10);

		fc8300_write(handle, devid, 0xed, 0xcc);

		fc8300_write(handle, devid, 0x82, 0x88);
		fc8300_write(handle, devid, 0x7c, 0x01);
		fc8300_write(handle, devid, 0x84, 0x1a);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x1f);
		fc8300_write(handle, devid, 0x87, 0x0f);
		fc8300_write(handle, devid, 0x88, 0x0f);
		fc8300_write(handle, devid, 0x89, 0x0b);
		fc8300_write(handle, devid, 0x8a, 0x15);
		fc8300_write(handle, devid, 0x8b, 0x0b);
		fc8300_write(handle, devid, 0x78, 0x30);
		fc8300_write(handle, devid, 0x7e, 0x05);
		fc8300_write(handle, devid, 0x7f, 0x0a);
		fc8300_write(handle, devid, 0xb2, 0x00);
		fc8300_write(handle, devid, 0xb3, 0x0c);
		fc8300_write(handle, devid, 0xb4, 0x00);
		fc8300_write(handle, devid, 0xb5, 0x0a);
		fc8300_write(handle, devid, 0xb6, 0x00);
		fc8300_write(handle, devid, 0xb7, 0x0a);
		fc8300_write(handle, devid, 0xb8, 0x00);
		fc8300_write(handle, devid, 0xb9, 0x0a);
		fc8300_write(handle, devid, 0xba, 0x00);
		fc8300_write(handle, devid, 0xbb, 0x0a);
		fc8300_write(handle, devid, 0xbc, 0x00);
		fc8300_write(handle, devid, 0xbd, 0x0a);
		fc8300_write(handle, devid, 0xbe, 0x00);
		fc8300_write(handle, devid, 0xbf, 0x0a);
		fc8300_write(handle, devid, 0xc0, 0x0a);
		fc8300_write(handle, devid, 0xc1, 0x00);
		fc8300_write(handle, devid, 0xc2, 0x13);
		fc8300_write(handle, devid, 0xc3, 0x00);
		fc8300_write(handle, devid, 0xf1, 0x53);

		fc8300_write(handle, devid, 0xd3, 0x00);
		fc8300_write(handle, devid, 0xd4, 0x02);
		fc8300_write(handle, devid, 0xd5, 0x04);
		fc8300_write(handle, devid, 0xd6, 0x14);
		fc8300_write(handle, devid, 0xd7, 0x34);
		fc8300_write(handle, devid, 0xd8, 0x36);
		fc8300_write(handle, devid, 0xd9, 0x38);
		fc8300_write(handle, devid, 0xda, 0x78);
		fc8300_write(handle, devid, 0xdb, 0xb8);
		fc8300_write(handle, devid, 0xdc, 0xf8);
		fc8300_write(handle, devid, 0xdd, 0xf8);

		fc8300_write(handle, devid, 0x33, 0x88);
		fc8300_write(handle, devid, 0x34, 0x86);
		fc8300_write(handle, devid, 0x37, 0x64);
		fc8300_write(handle, devid, 0x38, 0x44);
		fc8300_write(handle, devid, 0x39, 0x02);
		fc8300_write(handle, devid, 0x3e, 0xab);

		fc8300_write(handle, devid, 0x13, 0x07);
		fc8300_write(handle, devid, 0x15, 0x46);
		fc8300_write(handle, devid, 0x84, 0x14);
		fc8300_write(handle, devid, 0x85, 0x0f);
		fc8300_write(handle, devid, 0x86, 0x3f);
		fc8300_write(handle, devid, 0x87, 0x2f);
		fc8300_write(handle, devid, 0x8f, 0xb6);
		fc8300_write(handle, devid, 0x90, 0xb2);

		fc8300_write(handle, devid, 0x91, 0x68);
		fc8300_write(handle, devid, 0x16, 0x42);
		fc8300_write(handle, devid, 0x17, 0x42);
		fc8300_write(handle, devid, 0x1a, 0x42);
		fc8300_write(handle, devid, 0x1f, 0x77);
		fc8300_write(handle, devid, 0x20, 0x42);
		fc8300_write(handle, devid, 0xe9, 0x79);
		fc8300_write(handle, devid, 0xef, 0xb7);

		for (i = 0; i < 30; i++) {
			msWait(1);

			fc8300_read(handle, devid, 0x43, &temp);

			if (((temp >> 4) & 0x01) == 1)
				break;
		}

		fc8300_write(handle, devid, 0x5f, 0x00);

		fc8300_write(handle, devid, 0x78, 0xf0);

		msWait(5);

		fc8300_write(handle, devid, 0x78, 0x30);
	}

	fc8300_write(handle, devid, 0xfb, FC8300_BAND_WIDTH);
	fc8300_write(handle, devid, 0xfc, broadcast_type);
	fc8300_write(handle, devid, 0xfd, (u8)(fc8300_xtal_freq / 1000));
	fc8300_write(handle, devid, 0xfe, DRIVER_VERSION);

	/* PLL AGC All Broadcast */

	if (fc8300_xtal_freq < 20000)
		fc8300_write(handle, devid, 0xF3, 0x55);
	else if (fc8300_xtal_freq > 20000)
		fc8300_write(handle, devid, 0xF3, 0x54);

	fc8300_write(handle, devid, 0xF4, 0xFF);
	fc8300_write(handle, devid, 0xB5, 0x0A);
	fc8300_write(handle, devid, 0xB7, 0x0A);
	fc8300_write(handle, devid, 0xB9, 0x0A);
	fc8300_write(handle, devid, 0xBB, 0x0A);

	fc8300_write(handle, devid, 0x50, 0xff);
	fc8300_write(handle, devid, 0x5f, 0x00);
	fc8300_write(handle, devid, 0x6a, 0x00);
	fc8300_write(handle, devid, 0x6b, 0x99);
	fc8300_write(handle, devid, 0x6c, 0x99);
	fc8300_write(handle, devid, 0x6e, 0x03);
	fc8300_write(handle, devid, 0x73, 0x66);
	fc8300_write(handle, devid, 0x74, 0x66);
	fc8300_write(handle, devid, 0x75, 0x30);

	if ((broadcast_type == ISDBT_13SEG) || (broadcast_type == ISDBT_1SEG)) {

		fc8300_write(handle, devid, 0xfa, 0x20);

		fc8300_es2_set_freq(handle, devid, 545143);

		fc8300_write(handle, devid, 0x1e, 0x10);
		fc8300_write(handle, devid, 0x1f, 0xf2);
		fc8300_write(handle, devid, 0x78, 0x06);

		fc8300_write(handle, devid, 0x04, 0x12);
		fc8300_write(handle, devid, 0x8d, 0x08);
		fc8300_write(handle, devid, 0x8e, 0x13);

		fc8300_write(handle, devid, 0x13, 0x07);

		fc8300_write(handle, devid, 0x22, 0x01);
		fc8300_write(handle, devid, 0x24, 0x11);
		fc8300_write(handle, devid, 0x23, 0x17);

		if (broad_check == 1) { /* BROADCAST Control */

			if ((device_set & 0x01) == 1)
				fc_cal_value[0] = (tf_cal(handle, DIV_MASTER) -
									7);
			if ((device_set & 0x02) == 2)
				fc_cal_value[1] = (tf_cal(handle, DIV_SLAVE0) -
									7);
			if ((device_set & 0x04) == 4)
				fc_cal_value[2] = (tf_cal(handle, DIV_SLAVE1) -
									7);
			if ((device_set & 0x08) == 8)
				fc_cal_value[3] = (tf_cal(handle, DIV_SLAVE2) -
									7);

		} else if (broad_check == 0) { /* Single Control */

			fc_cal_value[0] = (tf_cal(handle, devid) - 7);

		}

		fc8300_write(handle, devid, 0x1e, 0x00);
		fc8300_write(handle, devid, 0x1f, 0x72);
		fc8300_write(handle, devid, 0x78, 0x30);
		fc8300_write(handle, devid, 0x04, 0x10);
		fc8300_write(handle, devid, 0x22, 0x00);

		fc8300_write(handle, devid, 0xfa, 0x00);
	}

	return BBM_OK;
}

static u32 tunning_mode_0[57][5] = {
	{473143, 0x7d, 0x21, 0x33, 0x1b},
	{479143, 0x7d, 0x77, 0x31, 0x1b},
	{485143, 0x7b, 0x44, 0x31, 0x1a},
	{491143, 0x7b, 0x44, 0x31, 0x1a},
	{497143, 0x7b, 0x44, 0x31, 0x19},
	{503143, 0x7b, 0x44, 0x31, 0x19},
	{509143, 0x7b, 0x44, 0x31, 0x19},
	{515143, 0x7b, 0x44, 0x31, 0x18},
	{521143, 0x7c, 0x44, 0x31, 0x18},
	{527143, 0x7c, 0x44, 0x31, 0x18},
	{533143, 0x7c, 0x44, 0x31, 0x17},
	{539143, 0x7c, 0x44, 0x31, 0x17},
	{545143, 0x7c, 0x44, 0x31, 0x17},
	{551143, 0x7c, 0x44, 0x31, 0x17},
	{557143, 0x7c, 0x44, 0x31, 0x16},
	{563143, 0x7c, 0x44, 0x31, 0x16},
	{569143, 0x7c, 0x44, 0x31, 0x16},
	{575143, 0x7e, 0x44, 0x31, 0x16},
	{581143, 0x7d, 0x44, 0x31, 0x15},
	{587143, 0x7d, 0x44, 0x31, 0x15},
	{593143, 0x7e, 0x44, 0x21, 0x15},
	{599143, 0x7e, 0x44, 0x21, 0x15},
	{605143, 0x7d, 0x44, 0x21, 0x14},
	{611143, 0x7d, 0x44, 0x21, 0x14},
	{617143, 0x7d, 0x44, 0x21, 0x14},
	{623143, 0x7e, 0x44, 0x21, 0x14},
	{629143, 0x7d, 0x44, 0x21, 0x14},
	{635143, 0x80, 0x44, 0x21, 0x13},
	{641143, 0x80, 0x44, 0x21, 0x13},
	{647143, 0x7d, 0x32, 0x21, 0x13},
	{653143, 0x7e, 0x32, 0x21, 0x13},
	{659143, 0x7d, 0x32, 0x21, 0x13},
	{665143, 0x7d, 0x32, 0x21, 0x13},
	{671143, 0x7c, 0x32, 0x21, 0x12},
	{677143, 0x7c, 0x32, 0x21, 0x12},
	{683143, 0x7c, 0x32, 0x21, 0x12},
	{689143, 0x7c, 0x32, 0x21, 0x12},
	{695143, 0x7c, 0x32, 0x21, 0x12},
	{701143, 0x7c, 0x32, 0x21, 0x12},
	{707143, 0x7b, 0x32, 0x21, 0x11},
	{713143, 0x7a, 0x32, 0x21, 0x11},
	{719143, 0x7a, 0x32, 0x21, 0x11},
	{725143, 0x7a, 0x32, 0x21, 0x11},
	{731143, 0x7a, 0x32, 0x21, 0x11},
	{737143, 0x7a, 0x32, 0x21, 0x11},
	{741143, 0x7a, 0x32, 0x21, 0x11},
	{747143, 0x7a, 0x32, 0x21, 0x11},
	{753143, 0x7a, 0x32, 0x21, 0x11},
	{759143, 0x7a, 0x32, 0x21, 0x11},
	{765143, 0x7a, 0x32, 0x21, 0x11},
	{772143, 0x7a, 0x32, 0x21, 0x11},
	{778143, 0x7a, 0x32, 0x21, 0x11},
	{785143, 0x7a, 0x32, 0x21, 0x11},
	{791143, 0x7a, 0x32, 0x21, 0x11},
	{797143, 0x7a, 0x32, 0x21, 0x11},
	{803143, 0x7a, 0x32, 0x21, 0x11},
	{809143, 0x7a, 0x32, 0x21, 0x11}
};
static u32 tunning_mode_1[8][7] = {
	{210429, 0x7c, 0x53, 0x42, 0x07, 0x22, 0x1b},
	{216000, 0x7b, 0x52, 0x42, 0x07, 0x22, 0x1b},
	{219000, 0x7b, 0x53, 0x42, 0x07, 0x22, 0x1b},
	{219429, 0x7b, 0x53, 0x42, 0x07, 0x22, 0x1b},
	{219857, 0x7a, 0x53, 0x42, 0x07, 0x22, 0x1b},
	{220714, 0x7a, 0x53, 0x31, 0x04, 0x22, 0x1b},
	{221143, 0x7a, 0x53, 0x31, 0x04, 0x22, 0x1b},
	{221571, 0x7a, 0x53, 0x31, 0x04, 0x22, 0x1b}
};

static u32 tunning_mode_4[57][7] = { /* ISDBT 13SEG*/
	{473143, 0x79, 0x77, 0x46, 0x07, 0x22, 0x1b},
	{479143, 0x79, 0x77, 0x46, 0x07, 0x22, 0x1b},
	{485143, 0x79, 0x77, 0x46, 0x07, 0x22, 0x1a},
	{491143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x1a},
	{497143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x19},
	{503143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x19},
	{509143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x19},
	{515143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x18},
	{521143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x18},
	{527143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x18},
	{533143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x17},
	{539143, 0x78, 0x77, 0x46, 0x04, 0x22, 0x17},
	{545143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x17},
	{551143, 0x79, 0x77, 0x46, 0x04, 0x22, 0x17},
	{557143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x16},
	{563143, 0x77, 0x77, 0x46, 0x04, 0x22, 0x16},
	{569143, 0x78, 0x77, 0x46, 0x04, 0x22, 0x16},
	{575143, 0x79, 0x77, 0x46, 0x04, 0x22, 0x16},
	{581143, 0x79, 0x77, 0x46, 0x04, 0x22, 0x15},
	{587143, 0x79, 0x77, 0x46, 0x04, 0x22, 0x15},
	{593143, 0x79, 0x77, 0x46, 0x04, 0x22, 0x15},
	{599143, 0x79, 0x77, 0x46, 0x04, 0x22, 0x15},
	{605143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x14},
	{611143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x14},
	{617143, 0x7b, 0x77, 0x46, 0x04, 0x22, 0x14},
	{623143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x14},
	{629143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x14},
	{635143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x13},
	{641143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x13},
	{647143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x13},
	{653143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x13},
	{659143, 0x7a, 0x77, 0x46, 0x04, 0x22, 0x13},
	{665143, 0x7c, 0x77, 0x46, 0x07, 0x22, 0x13},
	{671143, 0x7c, 0x77, 0x46, 0x07, 0x22, 0x12},
	{677143, 0x7c, 0x77, 0x46, 0x07, 0x22, 0x12},
	{683143, 0x7c, 0x77, 0x46, 0x07, 0x22, 0x12},
	{689143, 0x7c, 0x77, 0x46, 0x07, 0x22, 0x12},
	{695143, 0x7c, 0x77, 0x46, 0x07, 0x22, 0x12},
	{701143, 0x7b, 0x77, 0x46, 0x07, 0x22, 0x12},
	{707143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{713143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{719143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{725143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{731143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{737143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{741143, 0x7a, 0x77, 0x66, 0x07, 0x22, 0x11},
	{747143, 0x7a, 0x77, 0x66, 0x07, 0x22, 0x11},
	{753143, 0x7a, 0x77, 0x66, 0x07, 0x22, 0x11},
	{759143, 0x7a, 0x77, 0x66, 0x07, 0x22, 0x11},
	{765143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{772143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{778143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{785143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x11},
	{791143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x10},
	{797143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x10},
	{803143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x10},
	{809143, 0x7a, 0x77, 0x46, 0x07, 0x22, 0x10}
};
static u32 tunning_mode_5[40][7] = {
	{210429, 0x75, 0x54, 0x46, 0x07, 0x22, 0x1b},
	{216000, 0x73, 0x52, 0x46, 0x07, 0x22, 0x1b}
};

static u32 tf_offset[2][13] = {
	{473000, 485000, 497000, 515000, 533000, 557000, 581000, 605000,
		635000, 671000, 707000, 755000, 811000},
	{11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0}
};

s32 fc8300_es2_set_freq(HANDLE handle, DEVICEID devid, u32 freq)
{
	u8 i;
	u8 broad_check_set = 0;
	u8 offset = 0;
	u8 bc_type_freq = 0;
	u8 tf_cal_set = 0;
	u8 over_freq = 0;
	t_handle = handle;

	if (devid == DIV_MASTER) {  /* Single Control */
		bc_type_freq = bc_type[0];
		t_devid[0] = devid;
		thread_freq[0] = freq;
	} else if (devid == DIV_SLAVE0) {  /* Single Control */
		bc_type_freq = bc_type[1];
		t_devid[1] = devid;
		thread_freq[1] = freq;
	} else if (devid == DIV_SLAVE1) {  /* Single Control */
		bc_type_freq = bc_type[2];
		t_devid[2] = devid;
		thread_freq[2] = freq;
	} else if (devid == DIV_SLAVE2) {  /* Single Control */
		bc_type_freq = bc_type[3];
		t_devid[3] = devid;
		thread_freq[3] = freq;
	} else if (devid == DIV_BROADCAST) { /* BROADCAST Control */
		broad_check_set = 1;
		bc_type_freq = bc_type[0];
		t_devid[0] = DIV_MASTER;
		t_devid[1] = DIV_SLAVE0;
		t_devid[2] = DIV_SLAVE1;
		t_devid[3] = DIV_SLAVE2;
		thread_freq[0] = freq;
		thread_freq[1] = freq;
		thread_freq[2] = freq;
		thread_freq[3] = freq;
	}

	if (bc_type_freq == ISDBT_CATV_13SEG) {  /* mode 6*/
		fc8300_write(handle, devid, 0xfa, 0xff);

		if (213143 >= freq) {

			if (catv_status == 0) {

				catv_status = 1;

				fc8300_es2_vhf(handle, devid);
				msWait(1000);

				bc_type_freq = ISDBTMM_13SEG;

			} else if (catv_status == 1) {

				bc_type_freq = ISDBTMM_13SEG;

			} else if (catv_status == 2) {

				catv_status = 1;

				fc8300_es2_vhf(handle, devid);
				msWait(1000);
			}

		} else if (213143 < freq) {

			if (catv_status == 0) {

				catv_status = 2;

				fc8300_es2_uhf(handle, devid);
				msWait(1000);

				bc_type_freq = ISDBT_13SEG;

			} else if (catv_status == 2) {

				bc_type_freq = ISDBT_13SEG;

			} else if (catv_status == 1) {

				catv_status = 1;

				fc8300_es2_uhf(handle, devid);
				msWait(1000);
			}
		}
	}

	if (bc_type_freq == ISDBT_1SEG) { /* mode 0 */
		for (i = 0; i < 57; i++) {
			if (((tunning_mode_0[i][0] + 3000) > freq) &&
				((tunning_mode_0[i][0] - 3000) <= freq)) {
				fc8300_write(handle, devid, 0xe9,
						tunning_mode_0[i][1]);
				fc8300_write(handle, devid, 0xef,
						tunning_mode_0[i][2]);
				fc8300_write(handle, devid, 0x19,
						tunning_mode_0[i][3]);
				fc8300_write(handle, devid, 0x1c,
						tunning_mode_0[i][4]);
				over_freq = 1;
				break;
			}
		}
		if (over_freq == 0) {
			fc8300_write(handle, devid, 0xe9,
					tunning_mode_0[56][1]);
			fc8300_write(handle, devid, 0xef,
					tunning_mode_0[56][2]);
			fc8300_write(handle, devid, 0x19,
					tunning_mode_0[56][3]);
			fc8300_write(handle, devid, 0x1c,
					tunning_mode_0[56][4]);
		}
	} else if (bc_type_freq == ISDBTMM_1SEG) {   /* mode 1 */
		for (i = 0; i < 8; i++) {
			if (((tunning_mode_1[i][0] + 200) > freq) &&
				((tunning_mode_1[i][0] - 200) <= freq)) {
				fc8300_write(handle, devid, 0xe9,
						tunning_mode_1[i][1]);
				fc8300_write(handle, devid, 0xef,
						tunning_mode_1[i][2]);
				fc8300_write(handle, devid, 0x19,
						tunning_mode_1[i][3]);
				fc8300_write(handle, devid, 0x1f,
						tunning_mode_1[i][4]);
				fc8300_write(handle, devid, 0x20,
						tunning_mode_1[i][5]);
				over_freq = 1;
				break;
			}
		}
		if (over_freq == 0) {
			fc8300_write(handle, devid, 0xe9,
					tunning_mode_1[7][1]);
			fc8300_write(handle, devid, 0xef,
					tunning_mode_1[7][2]);
			fc8300_write(handle, devid, 0x19,
					tunning_mode_1[7][3]);
			fc8300_write(handle, devid, 0x1f,
					tunning_mode_1[7][4]);
			fc8300_write(handle, devid, 0x20,
					tunning_mode_1[7][5]);
		}
	} else if (bc_type_freq == ISDBT_13SEG) {  /* mode 4*/
		for (i = 0; i < 57; i++) {
			if (((tunning_mode_4[i][0] + 3000) > freq) &&
				((tunning_mode_4[i][0] - 3000) <= freq)) {
				fc8300_write(handle, devid, 0xe9,
						tunning_mode_4[i][1]);
				fc8300_write(handle, devid, 0xef,
						tunning_mode_4[i][2]);
				fc8300_write(handle, devid, 0x19,
						tunning_mode_4[i][3]);
				fc8300_write(handle, devid, 0x1f,
						tunning_mode_4[i][4]);
				fc8300_write(handle, devid, 0x20,
						tunning_mode_4[i][5]);
				fc8300_write(handle, devid, 0x1c,
						tunning_mode_4[i][6]);
				over_freq = 1;
				break;
			}
		}
		if (over_freq == 0) {
			fc8300_write(handle, devid, 0xe9,
					tunning_mode_4[56][1]);
			fc8300_write(handle, devid, 0xef,
					tunning_mode_4[56][2]);
			fc8300_write(handle, devid, 0x19,
					tunning_mode_4[56][3]);
			fc8300_write(handle, devid, 0x1f,
					tunning_mode_4[56][4]);
			fc8300_write(handle, devid, 0x20,
					tunning_mode_4[56][5]);
			fc8300_write(handle, devid, 0x1c,
					tunning_mode_4[56][6]);
		}
	} else if (bc_type_freq == ISDBTMM_13SEG) {  /* mode 5*/
		for (i = 0; i < 40; i++) {
			if (((tunning_mode_5[i][0] + 1500) > freq) &&
				((tunning_mode_5[i][0] - 1500) <= freq)) {
				fc8300_write(handle, devid, 0xe9,
						tunning_mode_5[i][1]);
				fc8300_write(handle, devid, 0xef,
						tunning_mode_5[i][2]);
				fc8300_write(handle, devid, 0x19,
						tunning_mode_5[i][3]);
				fc8300_write(handle, devid, 0x1f,
						tunning_mode_5[i][4]);
				fc8300_write(handle, devid, 0x20,
						tunning_mode_5[i][5]);
				over_freq = 1;
				break;
			}
		}
		if (over_freq == 0) {
			fc8300_write(handle, devid, 0xe9,
					tunning_mode_5[39][1]);
			fc8300_write(handle, devid, 0xef,
					tunning_mode_5[39][2]);
			fc8300_write(handle, devid, 0x19,
					tunning_mode_5[39][3]);
			fc8300_write(handle, devid, 0x1f,
					tunning_mode_5[39][4]);
			fc8300_write(handle, devid, 0x20,
					tunning_mode_5[39][5]);
		}
	}

	if (broad_check_set == 0) {           /* Single Control */
		if (fc_cal_value[0] != 0) {
			for (i = 0; i < 12; i++) {
				if ((tf_offset[0][i] < freq) &&
					(tf_offset[0][i+1] >= freq)) {

					tf_cal_set = fc_cal_value[0] +
						tf_offset[1][i];

					if (tf_cal_set < 0x10)
						tf_cal_set = 0x10;
					else if (tf_cal_set > 0x1f)
						tf_cal_set = 0x1f;

					fc8300_write(handle, devid, 0x1c,
							tf_cal_set);
						break;
				}
			}
		}
		fc8300_tuner_set_pll(handle, devid, freq, offset);

	} else if (broad_check_set == 1) {  /* BROADCAST Control */
		if (fc_cal_value[0] != 0) {
			for (i = 0; i < 12; i++) {
				if ((tf_offset[0][i] < freq) &&
						(tf_offset[0][i+1] >= freq)) {

					tf_cal_set = fc_cal_value[0] +
						tf_offset[1][i];

					if (tf_cal_set < 0x10)
						tf_cal_set = 0x10;
					else if (tf_cal_set > 0x1f)
						tf_cal_set = 0x1f;

					fc8300_write(handle, DIV_MASTER, 0x1c,
							tf_cal_set);
			break;
	}
			}
		}
		if (fc_cal_value[1] != 0) {
			for (i = 0; i < 12; i++) {
				if ((tf_offset[0][i] < freq) &&
					(tf_offset[0][i+1] >= freq)) {

					tf_cal_set = fc_cal_value[1] +
						tf_offset[1][i];

					if (tf_cal_set < 0x10)
						tf_cal_set = 0x10;
					else if (tf_cal_set > 0x1f)
						tf_cal_set = 0x1f;

					fc8300_write(handle, DIV_SLAVE0, 0x1c,
							tf_cal_set);
					break;
				}
			}
		}
		if (fc_cal_value[2] != 0) {
			for (i = 0; i < 12; i++) {
				if ((tf_offset[0][i] < freq) &&
					(tf_offset[0][i+1] >= freq)) {

					tf_cal_set = fc_cal_value[2] +
						tf_offset[1][i];

					if (tf_cal_set < 0x10)
						tf_cal_set = 0x10;
					else if (tf_cal_set > 0x1f)
						tf_cal_set = 0x1f;

					fc8300_write(handle, DIV_SLAVE1, 0x1c,
							tf_cal_set);
					break;
				}
			}
		}
		if (fc_cal_value[3] != 0) {
			for (i = 0; i < 12; i++) {
				if ((tf_offset[0][i] < freq) &&
					(tf_offset[0][i+1] >= freq)) {

					tf_cal_set = fc_cal_value[3] +
						tf_offset[1][i];

					if (tf_cal_set < 0x10)
						tf_cal_set = 0x10;
					else if (tf_cal_set > 0x1f)
						tf_cal_set = 0x1f;

					fc8300_write(handle, DIV_SLAVE2, 0x1c,
							tf_cal_set);
					break;
				}
			}
		}

		if ((device_set & 0x01) == 1)
			fc8300_tuner_set_pll(handle, DIV_MASTER, freq, offset);
		if ((device_set & 0x02) == 2)
			fc8300_tuner_set_pll(handle, DIV_SLAVE0, freq, offset);
		if ((device_set & 0x04) == 4)
			fc8300_tuner_set_pll(handle, DIV_SLAVE1, freq, offset);
		if ((device_set & 0x08) == 8)
			fc8300_tuner_set_pll(handle, DIV_SLAVE2, freq, offset);
	}

	if (bc_type_freq == ISDBT_CATV_13SEG)
		fc8300_write(handle, devid, 0x91, 0xa8);
	else if (bc_type_freq != ISDBT_13SEG)
		fc8300_write(handle, devid, 0x91, 0x68);

	fc8300_read(handle, devid, 0xfa, &thread_check);

	if (thread_check == 0 && thread_set == 0) {
#ifdef BBM_ES_CURRENT
		_beginthread(KbdFunc , 0, NULL); /* Thread Go */
#endif
		thread_set = 1;
	} else if (thread_check == 0 && thread_set == 1) {
		status_1seg[0] = 0;
		status_1seg[1] = 0;
		status_1seg[2] = 0;
		status_1seg[3] = 0;
		status_13seg[0] = 0;
		status_13seg[1] = 0;
		status_13seg[2] = 0;
		status_13seg[3] = 0;
	} else if (thread_check != 0) { /* Thread Stop */
		thread_set = 0;
	}

	if (catv_status == 1 || catv_status == 2)
		bc_type_freq = ISDBT_CATV_13SEG;

	return BBM_OK;
}

s32 fc8300_es2_get_rssi(HANDLE handle, DEVICEID devid, s32 *rssi)
{
	u8 reg_value = 0;
	s32 rssi_value = 0;

	fc8300_read(handle, devid, 0xeb, &reg_value);

	rssi_value = reg_value - 256;

	*rssi = rssi_value;

	return BBM_OK;
}

s32 fc8300_es2_tuner_deinit(HANDLE handle, DEVICEID devid)
{
	thread_set = 0;
	msWait(1000);
	return BBM_OK;
}
