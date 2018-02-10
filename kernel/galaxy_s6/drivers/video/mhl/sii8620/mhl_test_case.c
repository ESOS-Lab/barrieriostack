/*
 * Copyright (C) 2014 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/random.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/unistd.h>

#include "si_8620_regs.h"
#include "mhl_test_case.h"

#define BITS_HPD_CTRL_PUSH_PULL_HIGH (0x30)
#define BITS_HPD_CTRL_PUSH_PULL_LOW (0x10)

extern int mhl_tx_write_reg(void *drv_context, u16 address, u8 value);
extern int mhl_tx_read_reg(void *drv_context, u16 address);
extern int sii8620_detection_start(unsigned long event);
extern int debug_level;

static int test_result;

int mhl_get_test_result(void)
{
	return test_result;
}

static int mhl_test_case_devid(void)
{
	int ret_val, number;

	ret_val = mhl_tx_read_reg((void *)NULL, REG_DEV_IDH);
	if (ret_val < 0) {
		pr_info("mhl_test_case: reg read fail : 0x%x\n", ret_val);
		return ret_val;
	}
	number = ret_val << 8;

	ret_val = mhl_tx_read_reg((void *)NULL, REG_DEV_IDL);
	if (ret_val < 0) {
		pr_info("mhl_test_case: reg read fail :  0x%x\n", ret_val);
		return ret_val;
	}

	ret_val |= number;
	test_result = ret_val;

	return 0;
}

static int mhl_test_case_muic(unsigned long *muic_state,
				uint32_t delay, uint32_t count, int random)
{
	int i;
	unsigned int wait_sec;

	pr_info("mhl_test_case_muic count:%d, delay:%dms\n", count, delay);

	for (i = 0; i < count && *muic_state; i++) {
		if (random == 1) {
			wait_sec = get_random_int();
			wait_sec = (wait_sec % 10);
			wait_sec = delay + wait_sec * 1000;
		} else {
			wait_sec = delay;
		}
		pr_info("sii8620: mhl_test_case_muic %d wait %d sec\n",
							i, wait_sec);
		sii8620_detection_start(0);
		msleep(wait_sec);
		sii8620_detection_start(1);
		msleep(wait_sec);
	}

	return 0;
}

static int mhl_test_case_hpd(unsigned long *muic_state,
		uint32_t delay, uint32_t count, int random)
{
	int i;
	unsigned int wait_sec;

	pr_info("mhl_test_case_hpd count:%d, delay:%dms\n", count, delay);

	for (i = 0; i < count && *muic_state; i++) {
		if (random == 1) {
			wait_sec = get_random_int();
			wait_sec = (wait_sec % 10);
			wait_sec = delay + wait_sec * 1000;
		} else {
			wait_sec = delay;
		}

		mhl_tx_write_reg((void *)NULL, REG_HPD_CTRL,
			BITS_HPD_CTRL_PUSH_PULL_LOW);
		msleep(wait_sec);
		msleep(50);
		mhl_tx_write_reg((void *)NULL, REG_HPD_CTRL,
			BITS_HPD_CTRL_PUSH_PULL_HIGH);
		msleep(wait_sec);
	}

	return 0;
}

static int mhl_test_case_platform_reset(void)
{
	/*TODO?*/
	return 0;
}

static int mhl_test_case_debug_level(int value)
{
	pr_info("mhl_test_case_debug_level value:%d\n", value);
	debug_level = value;
	return 0;
}

void mhl_test_case_main(void *pdata, unsigned long *muic_state, int *val)
{

	pr_info("mhl_test_case_main %d,%d,%d,%d\n",
				val[0], val[1], val[2], val[3]);

	test_result = 0;

	switch (val[0]) {
	case MHL_TEST_DEVID:
		mhl_test_case_devid();
		break;
	case MHL_TEST_MUIC:
		mhl_test_case_muic(muic_state, val[1], val[2], val[3]);
		break;
	case MHL_TEST_HPD:
		mhl_test_case_hpd(muic_state, val[1], val[2], val[3]);
		break;
	case MHL_TEST_PLATFORM_RESET:
		mhl_test_case_platform_reset();
		break;
	case MHL_TEST_DEBUG_LEVEL:
		mhl_test_case_debug_level(val[1]);
		break;
	default:
		break;
	};
}

