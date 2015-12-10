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

#ifndef __MHL_TEST_CASE_H__
#define __MHL_TEST_CASE_H__

enum test_case_t {
	MHL_TEST_DEVID = 0,
	MHL_TEST_MUIC,
	MHL_TEST_HPD,
	MHL_TEST_PLATFORM_RESET,
	MHL_TEST_DEBUG_LEVEL,
	MHL_TEST_MAX,
};

void mhl_test_case_main(void *pdata, unsigned long *muic_state, int *val);
int mhl_get_test_result(void);
#endif
