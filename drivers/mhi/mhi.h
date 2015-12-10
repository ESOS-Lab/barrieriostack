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

#ifndef _H_MHI
#define _H_MHI

#include "msm_mhi.h"
#include "mhi_macros.h"
#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/hrtimer.h>
#include <linux/pm.h>
#include "../pci/host/pci-noti.h"
#include <linux/completion.h>


typedef struct osal_thread osal_thread;
typedef struct mhi_meminfo mhi_meminfo;
typedef struct pci_dev pci_dev;
typedef struct device device;
typedef struct mhi_device_ctxt mhi_device_ctxt;
typedef struct mhi_pcie_devices mhi_pcie_devices;
typedef struct hrtimer hrtimer;
typedef struct pci_saved_state pci_saved_state;
extern mhi_pcie_devices mhi_devices;

typedef enum MHI_DEBUG_CLASS {
	MHI_DBG_DATA = 0x1000,
	MHI_DBG_POWER = 0x2000,
	MHI_DBG_reserved = 0x80000000
} MHI_DEBUG_CLASS;

typedef enum MHI_DEBUG_LEVEL {
	MHI_MSG_VERBOSE = 0x0,
	MHI_MSG_MSI_WAKE_GPIO = 0x1,
	MHI_MSG_INFO = 0x2,
	MHI_MSG_DBG = 0x4,
	MHI_MSG_WARNING = 0x8,
	MHI_MSG_ERROR = 0x10,
	MHI_MSG_CRITICAL = 0x20,
	MHI_MSG_reserved = 0x80000000
} MHI_DEBUG_LEVEL;

typedef struct pcie_core_info {
	u32 dev_id;
	u32 manufact_id;
	u32 mhi_ver;
	void __iomem *bar0_base;
	void __iomem *bar0_end;
	void __iomem *bar2_base;
	void __iomem *bar2_end;
	u32 device_wake_gpio;
	u32 irq_base;
	u32 max_nr_msis;
	pci_saved_state *pcie_state;
} pcie_core_info;

typedef struct bhi_ctxt_t {
	void __iomem *bhi_base;
	void *image_loc;
	dma_addr_t phy_image_loc;
	size_t image_size;
	void *unaligned_image_loc;
} bhi_ctxt_t;

typedef struct mhi_pcie_dev_info {
	pcie_core_info core;
	atomic_t ref_count;
	mhi_device_ctxt *mhi_ctxt;
	struct exynos_pcie_register_event mhi_pci_link_event;
	pci_dev *pcie_device;
	struct pci_driver *mhi_pcie_driver;
	bhi_ctxt_t bhi_ctxt;
	u32 link_down_cntr;
	u32 link_up_cntr;

} mhi_pcie_dev_info;

typedef struct mhi_pcie_devices {
	mhi_pcie_dev_info device_list[MHI_MAX_SUPPORTED_DEVICES];
	s32 nr_of_devices;
} mhi_pcie_devices;

typedef enum MHI_CHAN_TYPE {
	MHI_INVALID = 0x0,
	MHI_OUT = 0x1,
	MHI_IN = 0x2,
	MHI_CHAN_TYPE_reserved = 0x80000000
} MHI_CHAN_TYPE;

typedef enum MHI_CHAN_STATE {
	MHI_CHAN_STATE_DISABLED = 0x0,
	MHI_CHAN_STATE_ENABLED = 0x1,
	MHI_CHAN_STATE_RUNNING = 0x2,
	MHI_CHAN_STATE_SUSPENDED = 0x3,
	MHI_CHAN_STATE_STOP = 0x4,
	MHI_CHAN_STATE_ERROR = 0x5,
	MHI_CHAN_STATE_LIMIT = 0x6,
	MHI_CHAN_STATE_reserved = 0x80000000
} MHI_CHAN_STATE;

typedef enum MHI_RING_TYPE {
	MHI_RING_TYPE_CMD_RING = 0x0,
	MHI_RING_TYPE_XFER_RING = 0x1,
	MHI_RING_TYPE_EVENT_RING = 0x2,
	MHI_RING_TYPE_MAX = 0x4,
	MHI_RING_reserved = 0x80000000
} MHI_RING_TYPE;

typedef enum MHI_CHAIN {
	MHI_TRE_CHAIN_OFF = 0x0,
	MHI_TRE_CHAIN_ON = 0x1,
	MHI_TRE_CHAIN_LIMIT = 0x2,
	MHI_TRE_CHAIN_reserved = 0x80000000
} MHI_CHAIN;

typedef enum MHI_EVENT_RING_STATE {
	MHI_EVENT_RING_UINIT = 0x0,
	MHI_EVENT_RING_INIT = 0x1,
	MHI_EVENT_RING_reserved = 0x80000000
} MHI_EVENT_RING_STATE;

typedef enum MHI_STATE {
	MHI_STATE_RESET = 0x0,
	MHI_STATE_READY = 0x1,
	MHI_STATE_M0 = 0x2,
	MHI_STATE_M1 = 0x3,
	MHI_STATE_M2 = 0x4,
	MHI_STATE_M3 = 0x5,
	MHI_STATE_BHI  = 0x7,
	MHI_STATE_LIMIT = 0x8,
	MHI_STATE_reserved = 0x80000000
} MHI_STATE;

#pragma pack(1)
typedef struct mhi_event_ctxt {
	u32 mhi_intmodt;
	u32 mhi_event_er_type;
	u32 mhi_msi_vector;
	u64 mhi_event_ring_base_addr;
	u64 mhi_event_ring_len;
	volatile u64 mhi_event_read_ptr;
	u64 mhi_event_write_ptr;
} mhi_event_ctxt;

typedef struct mhi_chan_ctxt {
	MHI_CHAN_STATE mhi_chan_state;
	MHI_CHAN_TYPE mhi_chan_type;
	u32 mhi_event_ring_index;
	u64 mhi_trb_ring_base_addr;
	u64 mhi_trb_ring_len;
	u64 mhi_trb_read_ptr;
	u64 mhi_trb_write_ptr;
} mhi_chan_ctxt;

typedef struct mhi_cmd_ctxt {
	u32 mhi_cmd_ctxt_reserved1;
	u32 mhi_cmd_ctxt_reserved2;
	u32 mhi_cmd_ctxt_reserved3;
	u64 mhi_cmd_ring_base_addr;
	u64 mhi_cmd_ring_len;
	u64 mhi_cmd_ring_read_ptr;
	u64 mhi_cmd_ring_write_ptr;
} mhi_cmd_ctxt;

#pragma pack()

typedef enum MHI_COMMAND {
	MHI_COMMAND_NOOP = 0x0,
	MHI_COMMAND_RESET_CHAN = 0x1,
	MHI_COMMAND_STOP_CHAN = 0x2,
	MHI_COMMAND_START_CHAN = 0x3,
	MHI_COMMAND_RESUME_CHAN = 0x4,
	MHI_COMMAND_MAX_NR = 0x5,
	MHI_COMMAND_reserved = 0x80000000
} MHI_COMMAND;

typedef enum MHI_PKT_TYPE {
	MHI_PKT_TYPE_RESERVED = 0x0,
	MHI_PKT_TYPE_NOOP_CMD = 0x1,
	MHI_PKT_TYPE_TRANSFER = 0x2,
	MHI_PKT_TYPE_RESET_CHAN_CMD = 0x10,
	MHI_PKT_TYPE_STOP_CHAN_CMD = 0x11,
	MHI_PKT_TYPE_START_CHAN_CMD = 0x12,
	MHI_PKT_TYPE_STATE_CHANGE_EVENT = 0x20,
	MHI_PKT_TYPE_CMD_COMPLETION_EVENT = 0x21,
	MHI_PKT_TYPE_TX_EVENT = 0x22,
	MHI_PKT_TYPE_EE_EVENT = 0x40,
} MHI_PKT_TYPE;

#pragma pack(1)

typedef struct mhi_tx_pkt {
	u64 buffer_ptr;
	u32 buf_len;
	u32 info;
} mhi_tx_pkt;

typedef struct mhi_noop_tx_pkt {
	u64 reserved1;
	u32 reserved2;
	u32 info;
} mhi_noop_tx_pkt;

typedef struct mhi_noop_cmd_pkt {
	u64 reserved1;
	u32 reserved2;
	u32 info;
} mhi_noop_cmd_pkt;

typedef struct mhi_reset_chan_cmd_pkt {
	u32 reserved1;
	u32 reserved2;
	u32 reserved3;
	u32 info;
} mhi_reset_chan_cmd_pkt;

typedef struct mhi_stop_chan_cmd_pkt {
	u32 reserved1;
	u32 reserved2;
	u32 reserved3;
} mhi_stop_chan_cmd_pkt;
typedef struct mhi_ee_state_change_event {
	u64 reserved1;
	u32 exec_env;
	u32 info;
} mhi_ee_state_change_event;

typedef struct mhi_xfer_event_pkt {
	volatile u64 xfer_ptr;
	volatile u32 xfer_details;
	volatile u32 info;
} mhi_xfer_event_pkt;

typedef struct mhi_cmd_complete_event_pkt {
	u64 ptr;
	u32 code;
	u32 info;
} mhi_cmd_complete_event_pkt;

typedef struct mhi_state_change_event_pkt {
	u64 reserved1;
	u32 state;
	u32 info;
} mhi_state_change_event_pkt;

typedef union mhi_xfer_pkt {
	mhi_tx_pkt data_tx_pkt;
	mhi_noop_tx_pkt noop_tx_pkt;
	mhi_tx_pkt type;
} mhi_xfer_pkt;

typedef union mhi_cmd_pkt {
	mhi_stop_chan_cmd_pkt stop_cmd_pkt;
	mhi_reset_chan_cmd_pkt reset_cmd_pkt;
	mhi_noop_cmd_pkt noop_cmd_pkt;
	mhi_noop_cmd_pkt type;
} mhi_cmd_pkt;

typedef union mhi_event_pkt {
	mhi_xfer_event_pkt xfer_event_pkt;
	mhi_cmd_complete_event_pkt cmd_complete_event_pkt;
	mhi_state_change_event_pkt state_change_event_pkt;
	mhi_ee_state_change_event ee_event_pkt;
	mhi_xfer_event_pkt type;
} mhi_event_pkt;

#pragma pack()
typedef enum MHI_EVENT_CCS {
	MHI_EVENT_CC_INVALID = 0x0,
	MHI_EVENT_CC_SUCCESS = 0x1,
	MHI_EVENT_CC_EOT = 0x2,
	MHI_EVENT_CC_OVERFLOW = 0x3,
	MHI_EVENT_CC_EOB = 0x4,
	MHI_EVENT_CC_OOB = 0x5,
	MHI_EVENT_CC_DB_MODE = 0x6,
	MHI_EVENT_CC_UNDEFINED_ERR = 0x10,
	MHI_EVENT_CC_RING_EL_ERR = 0x11,
} MHI_EVENT_CCS;

typedef struct mhi_ring {
	void *base;
	void *volatile wp;
	void *volatile rp;
	void *volatile ack_rp;
	uintptr_t len;
	uintptr_t el_size;
	u32 overwrite_en;
	atomic_t nr_filled_elements;
} mhi_ring;

typedef enum MHI_CMD_STATUS {
	MHI_CMD_NOT_PENDING = 0x0,
	MHI_CMD_PENDING = 0x1,
	MHI_CMD_RESET_PENDING = 0x2,
	MHI_CMD_RESERVED = 0x80000000
} MHI_CMD_STATUS;

typedef enum MHI_EVENT_RING_TYPE {
	MHI_EVENT_RING_TYPE_INVALID = 0x0,
	MHI_EVENT_RING_TYPE_VALID = 0x1,
	MHI_EVENT_RING_TYPE_reserved = 0x80000000
} MHI_EVENT_RING_TYPE;

typedef enum MHI_INIT_ERROR_STAGE {
	MHI_INIT_ERROR_STAGE_UNWIND_ALL = 0x1,
	MHI_INIT_ERROR_STAGE_DEVICE_CTRL = 0x2,
	MHI_INIT_ERROR_STAGE_THREADS = 0x3,
	MHI_INIT_ERROR_STAGE_EVENTS = 0x4,
	MHI_INIT_ERROR_STAGE_MEM_ZONES = 0x5,
	MHI_INIT_ERROR_STAGE_SYNC = 0x6,
	MHI_INIT_ERROR_STAGE_THREAD_QUEUES = 0x7,
	MHI_INIT_ERROR_STAGE_RESERVED = 0x80000000
} MHI_INIT_ERROR_STAGE;

typedef enum STATE_TRANSITION {
	STATE_TRANSITION_RESET = 0x0,
	STATE_TRANSITION_READY = 0x1,
	STATE_TRANSITION_M0 = 0x2,
	STATE_TRANSITION_M1 = 0x3,
	STATE_TRANSITION_M2 = 0x4,
	STATE_TRANSITION_M3 = 0x5,
	STATE_TRANSITION_BHI = 0x6,
	STATE_TRANSITION_SBL = 0x7,
	STATE_TRANSITION_AMSS = 0x8,
	STATE_TRANSITION_LINK_DOWN = 0x9,
	STATE_TRANSITION_WAKE = 0xA,
	STATE_TRANSITION_SYS_ERR = 0xFF,
	STATE_TRANSITION_reserved = 0x80000000
} STATE_TRANSITION;

typedef enum MHI_EXEC_ENV {
	MHI_EXEC_ENV_SBL = 0x1,
	MHI_EXEC_ENV_AMSS = 0x2,
} MHI_EXEC_ENV;

typedef struct mhi_client_handle {
	mhi_device_ctxt *mhi_dev_ctxt;
	mhi_client_info_t client_info;
	struct completion chan_close_complete;
	void *user_data;
	u32 chan;
	mhi_result result;
	u32 device_index;
	u32 event_ring_index;
	u32 msi_vec;
	u32 cb_mod;
	u32 intmod_t;
	u32 pkt_count;
} mhi_client_handle;
typedef enum MHI_EVENT_POLLING {
	MHI_EVENT_POLLING_DISABLED = 0x0,
	MHI_EVENT_POLLING_ENABLED = 0x1,
	MHI_EVENT_POLLING_reserved = 0x80000000
} MHI_EVENT_POLLING;

typedef struct mhi_state_work_queue {
	spinlock_t *q_lock;
	mhi_ring q_info;
	u32 queue_full_cntr;
	STATE_TRANSITION buf[MHI_WORK_Q_MAX_SIZE];
} mhi_state_work_queue;

typedef struct mhi_control_seg {
	mhi_xfer_pkt *xfer_trb_list[MHI_MAX_CHANNELS];
	mhi_event_pkt *ev_trb_list[EVENT_RINGS_ALLOCATED];
	mhi_cmd_pkt cmd_trb_list[NR_OF_CMD_RINGS][CMD_EL_PER_RING + 1];
	mhi_cmd_ctxt mhi_cmd_ctxt_list[NR_OF_CMD_RINGS];
	mhi_chan_ctxt mhi_cc_list[MHI_MAX_CHANNELS];
	mhi_event_ctxt mhi_ec_list[MHI_MAX_CHANNELS];
	u32 padding;
} mhi_control_seg;

typedef struct mhi_chan_counters {
	u32 empty_ring_removal;
	u32 pkts_xferd;
	u32 ev_processed;
} mhi_chan_counters;

typedef struct mhi_counters {
	u32 m0_m1;
	u32 m1_m0;
	u32 m1_m2;
	u32 m2_m0;
	u32 m0_m3;
	u32 m3_m0;
	u32 m1_m3;
	u32 mhi_reset_cntr;
	u32 mhi_ready_cntr;
	u32 m3_event_timeouts;
	u32 m0_event_timeouts;
	u32 msi_disable_cntr;
	u32 msi_enable_cntr;
	u32 nr_irq_migrations;
	atomic_t outbound_acks;
	u32 failed_recycle[MHI_RING_TYPE_MAX];
	atomic_t skb_alloc_fail_cntr;
} mhi_counters;

typedef struct mhi_flags {
	u32 mhi_initialized;
	u32 mhi_clients_probed;
	u32 uci_probed;
	volatile u32 pending_M3;
	volatile u32 pending_M0;
	u32 link_up;
	volatile u32 kill_threads;
	atomic_t mhi_link_off;
	atomic_t data_pending;
	atomic_t events_pending;
	atomic_t m0_work_enabled;
	atomic_t m3_work_enabled;
	atomic_t pending_resume;
	atomic_t cp_m1_state;
	volatile int stop_threads;
	u32 ssr;
} mhi_flags;

struct mhi_device_ctxt {
	mhi_pcie_dev_info *dev_info;
	pcie_core_info *dev_props;
	void __iomem *mmio_addr;
	void __iomem *channel_db_addr;
	void __iomem *event_db_addr;
	void __iomem *cmd_db_addr;
	mhi_control_seg *mhi_ctrl_seg;
	mhi_meminfo *mhi_ctrl_seg_info;
	u64 nr_of_cc;
	u64 nr_of_ec;
	u64 nr_of_cmdc;
	MHI_STATE mhi_state;
	volatile u64 mmio_len;
	mhi_ring mhi_local_chan_ctxt[MHI_MAX_CHANNELS];
	mhi_ring mhi_local_event_ctxt[MHI_MAX_CHANNELS];
	mhi_ring mhi_local_cmd_ctxt[NR_OF_CMD_RINGS];
	struct mutex *mhi_chan_mutex;
	struct mutex mhi_link_state;
	spinlock_t *mhi_ev_spinlock_list;
	struct mutex *mhi_cmd_mutex_list;
	mhi_client_handle *client_handle_list[MHI_MAX_CHANNELS];
	struct task_struct *event_thread_handle;
	struct task_struct *st_thread_handle;
	volatile u32 ev_thread_stopped;
	volatile u32 st_thread_stopped;
	wait_queue_head_t *event_handle;
	wait_queue_head_t *state_change_event_handle;
	wait_queue_head_t *M0_event;
	wait_queue_head_t *M3_event;
	wait_queue_head_t *chan_start_complete;

	u32 mhi_chan_db_order[MHI_MAX_CHANNELS];
	u32 mhi_ev_db_order[MHI_MAX_CHANNELS];
	spinlock_t *db_write_lock;

	struct platform_device *mhi_uci_dev;
	struct platform_device *mhi_rmnet_dev;
	atomic_t link_ops_flag;

	mhi_state_work_queue state_change_work_item_list;
	MHI_CMD_STATUS mhi_chan_pend_cmd_ack[MHI_MAX_CHANNELS];

	atomic_t start_cmd_pending_ack;
	u32 cmd_ring_order;
	u32 alloced_ev_rings[EVENT_RINGS_ALLOCATED];
	u32 ev_ring_props[EVENT_RINGS_ALLOCATED];
	u32 msi_counter[EVENT_RINGS_ALLOCATED];
	u32 db_mode[MHI_MAX_CHANNELS];
	u32 uldl_enabled;
	u32 hw_intmod_rate;
	u32 outbound_evmod_rate;
	mhi_counters counters;
	mhi_flags flags;

	rwlock_t xfer_lock;
	hrtimer m1_timer;
	ktime_t m1_timeout;
	struct delayed_work m3_work;
	struct work_struct m0_work;

	struct workqueue_struct *work_queue;
	mhi_chan_counters mhi_chan_cntr[MHI_MAX_CHANNELS];
	u32 ev_counter[MHI_MAX_CHANNELS];
//	struct msm_bus_scale_pdata bus_scale_table;
//	struct msm_bus_vectors bus_vectors[2];
//	struct msm_bus_paths usecase[2];
//	u32 bus_client;
	struct esoc_desc *esoc_handle;
	void *esoc_ssr_handle;

	struct notifier_block mhi_cpu_notifier;

	unsigned long esoc_notif;
	STATE_TRANSITION base_state;
	atomic_t db_skip_cnt;

	struct mutex pm_lock;
	struct wakeup_source wake_lock;
	int enable_lpm;
};

MHI_STATUS mhi_reset_all_thread_queues(mhi_device_ctxt *mhi_dev_ctxt);

MHI_STATUS mhi_add_elements_to_event_rings(mhi_device_ctxt *mhi_dev_ctxt,
		STATE_TRANSITION new_state);
MHI_STATUS validate_xfer_el_addr(mhi_chan_ctxt *ring, uintptr_t addr);
int get_nr_avail_ring_elements(mhi_ring *ring);
MHI_STATUS get_nr_enclosed_el(mhi_ring *ring, void *loc_1,
		void *loc_2, u32 *nr_el);
MHI_STATUS mhi_init_contexts(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS mhi_init_device_ctrl(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS mhi_init_mmio(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS mhi_init_device_ctxt(mhi_pcie_dev_info *dev_info,
		mhi_device_ctxt **mhi_dev_ctxt);


MHI_STATUS mhi_init_event_ring(mhi_device_ctxt *mhi_dev_ctxt,
		u32 nr_ev_el, u32 event_ring_index);

MHI_STATUS mhi_event_ring_init(mhi_event_ctxt *ev_list, uintptr_t trb_list_phy,
		uintptr_t trb_list_virt,
		size_t el_per_ring, mhi_ring *ring,
		u32 intmodt_val, u32 msi_vec);

/*Mhi Initialization functions */
MHI_STATUS mhi_create_ctxt(mhi_device_ctxt **mhi_dev_ctxt);
MHI_STATUS mhi_clean_init_stage(mhi_device_ctxt *mhi_dev_ctxt,
				MHI_INIT_ERROR_STAGE cleanup_stage);
MHI_STATUS mhi_init_sync(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS mhi_init_ctrl_zone(mhi_pcie_dev_info *dev_info,
				mhi_device_ctxt *mhi_dev_ctxt);

MHI_STATUS mhi_init_events(mhi_device_ctxt *mhi_dev_ctxt);

MHI_STATUS mhi_send_cmd(mhi_device_ctxt *dest_device,
			MHI_COMMAND which_cmd, u32 chan);

MHI_STATUS mhi_start(mhi_pcie_dev_info *new_devices, u32 nr_devices);
MHI_STATUS mhi_queue_tx_pkt(mhi_device_ctxt *mhi_dev_ctxt,
				MHI_CLIENT_CHANNEL chan,
				void *payload,
				size_t payload_size);
MHI_STATUS mhi_cmd_ring_init(mhi_cmd_ctxt *cmd_ring_id,
			uintptr_t trb_list_phy, uintptr_t trb_list_virt,
			size_t el_per_ring, mhi_ring *ring);
MHI_STATUS parse_cmd_completion_event(mhi_device_ctxt *mhi_dev_ctxt,
					mhi_event_pkt *ev_pkt);

MHI_STATUS mhi_init_chan_ctxt(mhi_chan_ctxt *cc_list,
		uintptr_t trb_list_phy,
		uintptr_t trb_list_virt,
		u64 el_per_ring,
		MHI_CHAN_TYPE chan_type,
		u32 event_ring,
		mhi_ring *ring);

MHI_STATUS add_element(mhi_ring *ring, void *volatile *rp,
			void *volatile *wp, void **assigned_addr);
MHI_STATUS delete_element(mhi_ring *ring, void *volatile *rp,
			 void *volatile *wp, void **assigned_addr);
MHI_STATUS ctxt_add_element(mhi_ring *ring, void **assigned_addr);
MHI_STATUS ctxt_del_element(mhi_ring *ring, void **assigned_addr);
MHI_STATUS get_element_index(mhi_ring *ring, void *address, uintptr_t *index);
MHI_STATUS get_element_addr(mhi_ring *ring, uintptr_t index, void **address);

MHI_STATUS recycle_trb_and_ring(mhi_device_ctxt *mhi_dev_ctxt, mhi_ring *ring,
		MHI_RING_TYPE ring_type, u32 ring_index);
MHI_STATUS mhi_change_chan_state(mhi_device_ctxt *mhi_dev_ctxt, u32 chan_id,
					MHI_CHAN_STATE new_state);
MHI_STATUS parse_xfer_event(mhi_device_ctxt *ctxt, mhi_event_pkt *event);
MHI_STATUS parse_cmd_event(mhi_device_ctxt *ctxt, mhi_event_pkt *event);
int parse_event_thread(void *ctxt);

MHI_STATUS mhi_init_device(mhi_pcie_dev_info *new_device);
MHI_STATUS mhi_process_event_ring(mhi_device_ctxt *mhi_dev_ctxt,
		u32 ev_ring_nr, u32 quota);

MHI_STATUS mhi_init_outbound(mhi_device_ctxt *mhi_dev_ctxt,
					MHI_CLIENT_CHANNEL);
MHI_STATUS mhi_test_for_device_ready(mhi_device_ctxt *mhi_dev_ctxt);

MHI_STATUS validate_ring_el_addr(mhi_ring *ring, uintptr_t addr);
MHI_STATUS validate_ev_el_addr(mhi_ring *ring, uintptr_t addr);

MHI_STATUS reset_chan_cmd(mhi_device_ctxt *mhi_dev_ctxt, mhi_cmd_pkt *cmd_pkt);
MHI_STATUS start_chan_cmd(mhi_device_ctxt *mhi_dev_ctxt, mhi_cmd_pkt *cmd_pkt);
MHI_STATUS parse_outbound(mhi_device_ctxt *mhi_dev_ctxt, u32 chan,
			mhi_xfer_pkt *local_ev_trb_loc, u16 xfer_len);
MHI_STATUS parse_inbound(mhi_device_ctxt *mhi_dev_ctxt, u32 chan,
			mhi_xfer_pkt *local_ev_trb_loc, u16 xfer_len);

int mhi_state_change_thread(void *ctxt);
MHI_STATUS mhi_init_state_change_thread_work_queue(mhi_state_work_queue *q);
MHI_STATUS mhi_init_state_transition(mhi_device_ctxt *mhi_dev_ctxt,
					STATE_TRANSITION new_state);
MHI_STATUS mhi_set_state_of_all_channels(mhi_device_ctxt *mhi_dev_ctxt,
					MHI_CHAN_STATE new_state);
void ring_all_chan_dbs(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS process_stt_work_item(mhi_device_ctxt  *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_M0_transition(mhi_device_ctxt  *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_M1_transition(mhi_device_ctxt  *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_M3_transition(mhi_device_ctxt  *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_READY_transition(mhi_device_ctxt *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_RESET_transition(mhi_device_ctxt *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_SYSERR_transition(mhi_device_ctxt *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_BHI_transition(mhi_device_ctxt *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_AMSS_transition(mhi_device_ctxt *mhi_dev_ctxt,
				STATE_TRANSITION cur_work_item);
MHI_STATUS process_SBL_transition(mhi_device_ctxt *mhi_dev_ctxt,
				STATE_TRANSITION cur_work_item);
MHI_STATUS process_LINK_DOWN_transition(mhi_device_ctxt *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS process_WAKE_transition(mhi_device_ctxt *mhi_dev_ctxt,
			STATE_TRANSITION cur_work_item);
MHI_STATUS mhi_wait_for_mdm(mhi_device_ctxt *mhi_dev_ctxt);
void conditional_chan_db_write(mhi_device_ctxt *mhi_dev_ctxt, u32 chan);
void ring_all_ev_dbs(mhi_device_ctxt *mhi_dev_ctxt);
enum hrtimer_restart mhi_initiate_M1(struct hrtimer *timer);
int mhi_suspend(struct pci_dev *dev, pm_message_t state);
int mhi_resume(struct pci_dev *dev);
MHI_STATUS probe_clients(mhi_device_ctxt *mhi_dev_ctxt,STATE_TRANSITION new_state);
int mhi_shim_probe(struct pci_dev *dev);
int mhi_init_pcie_device(mhi_pcie_dev_info *mhi_pcie_dev);
int mhi_init_gpios(mhi_pcie_dev_info *mhi_pcie_dev);
int mhi_init_pm_sysfs(struct device *dev);
MHI_STATUS mhi_init_timers(mhi_device_ctxt *mhi_dev_ctxt);
void mhi_remove(struct pci_dev *mhi_device);
int mhi_startup_thread(void *ctxt);
int mhi_get_chan_max_buffers(u32 chan);
void ring_all_cmd_dbs(mhi_device_ctxt *mhi_dev_ctxt);
int mhi_esoc_register(mhi_device_ctxt* mhi_dev_ctxt);
void mhi_link_state_cb(struct exynos_pcie_notify *notify);
void mhi_notify_clients(mhi_device_ctxt *mhi_dev_ctxt, MHI_CB_REASON reason);
int mhi_deassert_device_wake(mhi_device_ctxt *mhi_dev_ctxt);
int mhi_assert_device_wake(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS mhi_reg_notifiers(mhi_device_ctxt *mhi_dev_ctxt);
int mhi_cpu_notifier_cb(struct notifier_block *nfb, unsigned long action,
			void *hcpu);
MHI_STATUS mhi_init_wakelock(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS init_mhi_base_state(mhi_device_ctxt* mhi_dev_ctxt);
MHI_STATUS mhi_turn_off_pcie_link(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS mhi_turn_on_pcie_link(mhi_device_ctxt *mhi_dev_ctxt);

void delayed_m3(struct work_struct *work);
MHI_STATUS mhi_notify_device(mhi_device_ctxt *mhi_dev_ctxt, u32 chan);
MHI_STATUS mhi_init_work_queues(mhi_device_ctxt *mhi_dev_ctxt);
void mhi_set_m_state(mhi_device_ctxt *mhi_dev_ctxt, MHI_STATE new_state);
void m0_work(struct work_struct *work);
MHI_STATUS mhi_spawn_threads(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS mhi_wake_dev_from_m3(mhi_device_ctxt *mhi_dev_ctxt);
MHI_STATUS mhi_process_link_down(mhi_device_ctxt *mhi_dev_ctxt);
int mhi_initiate_m0(mhi_device_ctxt *mhi_dev_ctxt);
int mhi_initiate_m3(mhi_device_ctxt *mhi_dev_ctxt);
int mhi_set_bus_request(struct mhi_device_ctxt *mhi_dev_ctxt,
					int index);


void mhi_reg_write_field(void __iomem *io_addr, uintptr_t io_offset,
                         u32 mask, u32 shift, u32 val);
void mhi_reg_write(void __iomem *io_addr, uintptr_t io_offset, u32 val);
u32 mhi_reg_read_field(void __iomem *io_addr, uintptr_t io_offset,
                         u32 mask, u32 shift);
u32 mhi_reg_read(void __iomem *io_addr, uintptr_t io_offset);

#ifdef CONFIG_ARGOS
/* kernel team needs to provide argos header file. !!!
 * As of now, there's nothing to use. */
extern struct cpumask hmp_slow_cpu_mask;
extern struct cpumask hmp_fast_cpu_mask;

int argos_irq_affinity_setup_label(unsigned int irq, const char *label,
		struct cpumask *affinity_cpu_mask,
		struct cpumask *default_cpu_mask);
int argos_task_affinity_setup_label(struct task_struct *p, const char *label,
		struct cpumask *affinity_cpu_mask,
		struct cpumask *default_cpu_mask);
#endif

#endif
