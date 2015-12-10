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

#ifndef _MHL_RCP_INPUTDEV_H_
#define _MHL_RCP_INPUTDEV_H_

struct mhl_dev_context;

struct rcp_keymap_t {
	unsigned multicode:1;
	unsigned press_and_hold_key:1;
	unsigned reserved:6;
	uint16_t map[2];
	uint8_t rcp_support;
};

#ifdef CONFIG_MHL3_SEC_FEATURE
#define MHL_LOGICAL_DEVICE_MAP (MHL_DEV_LD_GUI)
#else
#define MHL_LOGICAL_DEVICE_MAP (MHL_DEV_LD_AUDIO | MHL_DEV_LD_VIDEO | \
	MHL_DEV_LD_MEDIA | MHL_DEV_LD_GUI)
#endif

#define MHL_NUM_RCP_KEY_CODES 0x80
extern struct rcp_keymap_t rcpSupportTable[MHL_NUM_RCP_KEY_CODES];

int generate_rcp_input_event(struct mhl_dev_context *dev_context,
			     uint8_t rcp_keycode);

int init_rcp_input_dev(struct mhl_dev_context *dev_context);
void destroy_rcp_input_dev(struct mhl_dev_context *dev_context);
void rcp_input_dev_one_time_init(struct mhl_dev_context *dev_context);

#endif /* #ifndef _MHL_RCP_INPUTDEV_H_ */
