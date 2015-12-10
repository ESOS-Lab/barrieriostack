/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef MSM_MHI_H
#define MSM_MHI_H
#include <linux/types.h>

typedef struct mhi_client_handle mhi_client_handle;

#define MHI_DMA_MASK       0x7FFFFFFF
#define MHI_MAX_MTU        0xFFFF

typedef enum MHI_STATUS {
	MHI_STATUS_SUCCESS = 0,
	MHI_STATUS_ERROR = 1,
	MHI_STATUS_DEV_NOT_FOUND = 2,
	MHI_STATUS_RING_FULL = 3,
	MHI_STATUS_RING_EMPTY = 4,
	MHI_STATUS_ALLOC_ERROR = 5,
	MHI_STATUS_OBJ_BUSY = 6,
	MHI_STATUS_DEVICE_NOT_READY = 7,
	MHI_STATUS_INACTIVE = 8,
	MHI_STATUS_BAD_STATE = 9,
	MHI_STATUS_CHAN_NOT_READY = 10,
	MHI_STATUS_CMD_PENDING = 11,
	MHI_STATUS_LINK_DOWN = 12,
	MHI_STATUS_ALREADY_REGISTERED = 13,
	MHI_STATUS_USERSPACE_MEM_ERR = 14,
	MHI_STATUS_BAD_HANDLE = 15,
	MHI_STATUS_INVALID_CHAN_ERR = 16,
	MHI_STATUS_reserved = 0x80000000
} MHI_STATUS;

typedef enum MHI_CLIENT_CHANNEL {
	MHI_CLIENT_LOOPBACK_OUT = 0,
	MHI_CLIENT_LOOPBACK_IN = 1,
	MHI_CLIENT_SAHARA_OUT = 2,
	MHI_CLIENT_SAHARA_IN = 3,
	MHI_CLIENT_DIAG_OUT = 4,
	MHI_CLIENT_DIAG_IN = 5,
	MHI_CLIENT_SSR_OUT = 6,
	MHI_CLIENT_SSR_IN = 7,
	MHI_CLIENT_QDSS_OUT = 8,
	MHI_CLIENT_QDSS_IN = 9,
	MHI_CLIENT_EFS_OUT = 10,
	MHI_CLIENT_EFS_IN = 11,
	MHI_CLIENT_MBIM_OUT = 12,
	MHI_CLIENT_MBIM_IN = 13,
	MHI_CLIENT_QMI_OUT = 14,
	MHI_CLIENT_QMI_IN = 15,
	MHI_CLIENT_IP_CTRL_0_OUT = 16,
	MHI_CLIENT_IP_CTRL_0_IN = 17,
	MHI_CLIENT_IP_CTRL_1_OUT = 18,
	MHI_CLIENT_IP_CTRL_1_IN = 19,
	MHI_CLIENT_IP_CTRL_2_OUT = 20,
	MHI_CLIENT_IP_CTRL_2_IN = 21,
	MHI_CLIENT_IP_CTRL_3_OUT = 22,
	MHI_CLIENT_IP_CTRL_3_IN = 23,
	MHI_CLIENT_IP_CTRL_4_OUT = 24,
	MHI_CLIENT_IP_CTRL_4_IN = 25,
	MHI_CLIENT_IP_CTRL_5_OUT = 26,
	MHI_CLIENT_IP_CTRL_5_IN = 27,
	MHI_CLIENT_IP_CTRL_6_OUT = 28,
	MHI_CLIENT_IP_CTRL_6_IN = 29,
	MHI_CLIENT_IP_CTRL_7_OUT = 30,
	MHI_CLIENT_IP_CTRL_7_IN = 31,
	MHI_CLIENT_DUN_OUT = 32,
	MHI_CLIENT_DUN_IN = 33,
	MHI_CLIENT_IP_SW_0_OUT = 34,
	MHI_CLIENT_IP_SW_0_IN = 35,
	MHI_CLIENT_IP_SW_1_OUT = 36,
	MHI_CLIENT_IP_SW_1_IN = 37,
	MHI_CLIENT_IP_SW_2_OUT = 38,
	MHI_CLIENT_IP_SW_2_IN = 39,
	MHI_CLIENT_IP_SW_3_OUT = 40,
	MHI_CLIENT_IP_SW_3_IN = 41,
	MHI_CLIENT_CSVT_OUT = 42,
	MHI_CLIENT_CSVT_IN = 43,
	MHI_CLIENT_SMCT_OUT = 44,
	MHI_CLIENT_SMCT_IN = 45,
#ifdef FEATURE_UKNIGHT_MHI_DEDICATE_CHANNEL
	MHI_CLIENT_UKNIGHT_OUT = 46,
	MHI_CLIENT_UKNIGHT_IN = 47,
	MHI_CLIENT_RESERVED_1_LOWER = 48,
#else
	MHI_CLIENT_RESERVED_1_LOWER = 46,
#endif
	MHI_CLIENT_RESERVED_1_UPPER = 99,
	MHI_CLIENT_IP_HW_0_OUT = 100,
	MHI_CLIENT_IP_HW_0_IN = 101,
	MHI_CLIENT_RESERVED_2_LOWER = 102,
	MHI_CLIENT_RESERVED_2_UPPER = 127,
	MHI_MAX_CHANNELS = 102
} MHI_CLIENT_CHANNEL;

typedef enum MHI_CB_REASON {
	MHI_CB_XFER_SUCCESS = 0x0,
	MHI_CB_XFER_ERROR = 0x1,
	MHI_CB_MHI_DISABLED = 0x4,
	MHI_CB_MHI_ENABLED = 0x8,
	MHI_CB_CHAN_RESET_COMPLETE = 0x10,
	MHI_CB_reserved = 0x80000000,
} MHI_CB_REASON;
typedef struct mhi_result {
	void *user_data;
	void *payload_buf;
	u32 bytes_xferd;
	MHI_STATUS transaction_status;
} mhi_result;

typedef struct mhi_cb_info {
	mhi_result *result;
	MHI_CB_REASON cb_reason;
} mhi_cb_info;
typedef struct mhi_client_info_t {
	void (*mhi_client_cb)(mhi_cb_info *);
	u32 cb_mod;
	u32 intmod_t;
} mhi_client_info_t;

MHI_STATUS mhi_open_channel(mhi_client_handle **client_handle,
		MHI_CLIENT_CHANNEL chan, s32 device_index,
		mhi_client_info_t *info, void *user_data);
MHI_STATUS mhi_queue_xfer(mhi_client_handle *client_handle,
		uintptr_t buf, size_t buf_len, u32 chain, u32 eob);
void mhi_close_channel(mhi_client_handle *client_handle);
void mhi_mask_irq(mhi_client_handle *client_handle);
void mhi_unmask_irq(mhi_client_handle *client_handle);
mhi_result *mhi_poll(mhi_client_handle *client_handle);
int get_free_trbs(mhi_client_handle *client_handle);
int mhi_get_epid(mhi_client_handle* mhi_handle);
MHI_STATUS mhi_reset_channel(mhi_client_handle *mhi_handle);
void mhi_poll_inbound(mhi_client_handle *client_handle,
			uintptr_t *buf, ssize_t *buf_size);
MHI_STATUS mhi_client_recycle_trb(mhi_client_handle *client_handle);
int mhi_get_max_buffers(mhi_client_handle *client_handle);

int mhi_uci_init(void);
int rmnet_mhi_init(void);
int mhi_set_lpm(mhi_client_handle *client_handle, int enable_lpm);

#endif
