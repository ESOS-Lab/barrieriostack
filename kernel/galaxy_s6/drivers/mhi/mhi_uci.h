/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#ifndef _H_MHI_UCI_INTERFACE
#define _H_MHI_UCI_INTERFACE

#include "msm_mhi.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/ipc_logging.h>
#include <linux/dma-mapping.h>

#define MIN(_x, _y)(((_x) < (_y)) ? (_x) : (_y))

#ifdef FEATURE_UKNIGHT_MHI_DEDICATE_CHANNEL
#define MHI_DEV_NODE_NAME_LEN 13
#define MHI_MAX_NR_OF_CLIENTS 24
#define MHI_SOFTWARE_CLIENT_START 0
#define MHI_SOFTWARE_CLIENT_LIMIT 24
#define MHI_MAX_SOFTWARE_CHANNELS 48
#else
#define MHI_DEV_NODE_NAME_LEN 13
#define MHI_MAX_NR_OF_CLIENTS 23
#define MHI_SOFTWARE_CLIENT_START 0
#define MHI_SOFTWARE_CLIENT_LIMIT 23
#define MHI_MAX_SOFTWARE_CHANNELS 46
#endif

#define MAX_NR_TRBS_PER_CHAN 10
#define MHI_PCIE_VENDOR_ID 0x17CB
#define MHI_PCIE_DEVICE_ID 0x0300
#define DEVICE_NAME "mhi"
#define CTRL_MAGIC 0x4C525443

#define CHAN_TO_CLIENT_INDEX(_CHAN_NR) (_CHAN_NR / 2)

typedef enum UCI_DBG_LEVEL {
	UCI_DBG_VERBOSE = 0x0,
	UCI_DBG_INFO = 0x1,
	UCI_DBG_DBG = 0x2,
	UCI_DBG_WARNING = 0x3,
	UCI_DBG_ERROR = 0x4,
	UCI_DBG_CRITICAL = 0x5,
	UCI_DBG_reserved = 0x80000000
} UCI_DBG_LEVEL;

extern UCI_DBG_LEVEL mhi_uci_msg_lvl;
extern UCI_DBG_LEVEL mhi_uci_ipc_log_lvl;
void *mhi_uci_ipc_log;

#define mhi_uci_log(_msg_lvl, _msg, ...) do { \
	if (_msg_lvl >= mhi_uci_msg_lvl) { \
		pr_info("[%s] "_msg, __func__, ##__VA_ARGS__); \
	} \
	if (mhi_uci_ipc_log && (_msg_lvl >= mhi_uci_ipc_log_lvl)) { \
		ipc_log_string(mhi_uci_ipc_log,                     \
			"[%s] " _msg, __func__, ##__VA_ARGS__);     \
	} \
} while (0)

#pragma pack(1)
typedef struct rs232_ctrl_msg {
	u32 preamble;
	u32 msg_id;
	u32 dest_id;
	u32 size;
	u32 msg;
} rs232_ctrl_msg;
#pragma pack()

typedef enum MHI_SERIAL_STATE {
	MHI_SERIAL_STATE_DCD = 0x1,
	MHI_SERIAL_STATE_DSR = 0x2,
	MHI_SERIAL_STATE_RI = 0x3,
	MHI_SERIAL_STATE_reserved = 0x80000000,
} MHI_SERIAL_STATE;

typedef enum MHI_CTRL_LINE_STATE {
	MHI_CTRL_LINE_STATE_DTR = 0x1,
	MHI_CTRL_LINE_STATE_RTS = 0x2,
	MHI_CTRL_LINE_STATE_reserved = 0x80000000,
} MHI_CTRL_LINE_STATE;


typedef enum MHI_MSG_ID {
	MHI_CTRL_LINE_STATE_ID = 0x10,
	MHI_SERIAL_STATE_ID = 0x11,
	MHI_CTRL_MSG_ID = 0x12,
} MHI_MSG_ID;
typedef struct mhi_uci_ctxt_t mhi_uci_ctxt_t;

/* Begin MHI Specification Definition */
typedef enum MHI_CHAN_DIR {
	MHI_DIR_INVALID = 0x0,
	MHI_DIR_OUT = 0x1,
	MHI_DIR_IN = 0x2,
	MHI_DIR__reserved = 0x80000000
} MHI_CHAN_DIR;


typedef struct chan_attr {
	MHI_CLIENT_CHANNEL chan_id;
	size_t max_packet_size;
	size_t avg_packet_size;
	u32 max_nr_packets;
	u32 nr_trbs;
	MHI_CHAN_DIR dir;
} chan_attr;

typedef struct uci_client {
	u32 client_index;
	u32 out_chan;
	u32 in_chan;
	mhi_client_handle* outbound_handle;
	mhi_client_handle* inbound_handle;
	size_t pending_data;
	mhi_uci_ctxt_t *uci_ctxt;
	wait_queue_head_t read_wait_queue;
	wait_queue_head_t write_wait_queue;
	atomic_t avail_pkts;
	struct device *dev;
	u32 dev_node_enabled;
	u8 local_tiocm;
	atomic_t ref_count;
	int mhi_status;
	atomic_t out_pkt_pend_ack;
} uci_client;

typedef struct mhi_uci_ctxt_t {
	chan_attr channel_attributes[MHI_MAX_SOFTWARE_CHANNELS];
	uci_client client_handle_list[MHI_SOFTWARE_CLIENT_LIMIT];
	struct mutex client_chan_lock[MHI_MAX_SOFTWARE_CHANNELS];
	mhi_client_info_t client_info;
	dev_t start_ctrl_nr;
	mhi_client_handle* ctrl_handle;
	struct mutex ctrl_mutex;
	struct cdev cdev[MHI_MAX_SOFTWARE_CHANNELS];
	struct class *mhi_uci_class;
	struct platform_device *mhi_uci_dev;
	u32 ctrl_chan_id;
	atomic_t mhi_disabled;
} mhi_uci_ctxt_t;

void uci_xfer_cb(mhi_cb_info *client_info);
int mhi_uci_send_packet(mhi_client_handle **client_handle, void *buf,
		u32 size, u32 chan);
MHI_STATUS mhi_init_inbound(struct platform_device *dev, mhi_client_handle *client_handle,
		MHI_CLIENT_CHANNEL chan);
MHI_STATUS uci_init_client_attributes(mhi_uci_ctxt_t *mhi_uci_ctxt);
int mhi_uci_probe(struct platform_device *dev);
int mhi_uci_remove(struct platform_device *dev);
int mhi_uci_send_status_cmd(uci_client *client);
int mhi_uci_tiocm_set(uci_client *client_ctxt, u32 set, u32 clear);
void process_rs232_state(mhi_result *result);

#define CTRL_MSG_ID
#define MHI_CTRL_MSG_ID__MASK (0xFFFFFF)
#define MHI_CTRL_MSG_ID__SHIFT (0)
#define MHI_SET_CTRL_MSG_ID(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = ((_PKT)->msg_id); \
	new_val &= (~((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT)->msg_id = new_val; \
};
#define MHI_GET_CTRL_MSG_ID(_FIELD, _PKT, _VAL) \
{ \
	(_VAL) = ((_PKT)->msg_id); \
	(_VAL) >>= (MHI_##_FIELD ## __SHIFT);\
	(_VAL) &= (MHI_##_FIELD ## __MASK); \
};

#define CTRL_DEST_ID
#define MHI_CTRL_DEST_ID__MASK (0xFFFFFF)
#define MHI_CTRL_DEST_ID__SHIFT (0)
#define MHI_SET_CTRL_DEST_ID(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = ((_PKT)->dest_id); \
	new_val &= (~((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT)->dest_id = new_val; \
};
#define MHI_GET_CTRL_DEST_ID(_FIELD, _PKT, _VAL) \
{ \
	(_VAL) = ((_PKT)->dest_id); \
	(_VAL) >>= (MHI_##_FIELD ## __SHIFT);\
	(_VAL) &= (MHI_##_FIELD ## __MASK); \
};

#define CTRL_MSG_DTR
#define MHI_CTRL_MSG_DTR__MASK (0xFFFFFFFE)
#define MHI_CTRL_MSG_DTR__SHIFT (0)

#define CTRL_MSG_RTS
#define MHI_CTRL_MSG_RTS__MASK (0xFFFFFFFD)
#define MHI_CTRL_MSG_RTS__SHIFT (1)

#define STATE_MSG_DCD
#define MHI_STATE_MSG_DCD__MASK (0xFFFFFFFE)
#define MHI_STATE_MSG_DCD__SHIFT (0)

#define STATE_MSG_DSR
#define MHI_STATE_MSG_DSR__MASK (0xFFFFFFFD)
#define MHI_STATE_MSG_DSR__SHIFT (1)

#define STATE_MSG_RI
#define MHI_STATE_MSG_RI__MASK (0xFFFFFFFB)
#define MHI_STATE_MSG_RI__SHIFT (3)
#define MHI_SET_CTRL_MSG(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = (_PKT->msg); \
	new_val &= (~((MHI_##_FIELD ## __MASK)));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT)->msg = new_val; \
};
#define MHI_GET_STATE_MSG(_FIELD, _PKT) \
	(((_PKT)->msg & (~(MHI_##_FIELD ## __MASK))) \
		>> MHI_##_FIELD ## __SHIFT)

#define CTRL_MSG_SIZE
#define MHI_CTRL_MSG_SIZE__MASK (0xFFFFFF)
#define MHI_CTRL_MSG_SIZE__SHIFT (0)
#define MHI_SET_CTRL_MSG_SIZE(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = (_PKT->size); \
	new_val &= (~((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT)->size = new_val; \
};
#endif
