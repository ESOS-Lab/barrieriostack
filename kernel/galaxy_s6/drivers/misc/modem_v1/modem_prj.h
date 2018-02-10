/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MODEM_PRJ_H__
#define __MODEM_PRJ_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>

#include <linux/platform_data/modem_debug.h>
#include <linux/platform_data/modem_v1.h>

#include <linux/mipi-lli.h>

#include "include/circ_queue.h"
#include "include/sipc5.h"
#include "modem_pm.h"

/*#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP*/
#define DEBUG_MODEM_IF
#include <trace/events/modem_if.h>
/*#endif*/

#ifdef DEBUG_MODEM_IF
#define DEBUG_MODEM_IF_LINK_TX
#define DEBUG_MODEM_IF_FLOW_CTRL
#endif

/*
IOCTL commands
*/
#define IOCTL_MODEM_ON			_IO('o', 0x19)
#define IOCTL_MODEM_OFF			_IO('o', 0x20)
#define IOCTL_MODEM_RESET		_IO('o', 0x21)
#define IOCTL_MODEM_BOOT_ON		_IO('o', 0x22)
#define IOCTL_MODEM_BOOT_OFF		_IO('o', 0x23)
#define IOCTL_MODEM_BOOT_DONE		_IO('o', 0x24)

#define IOCTL_MODEM_PROTOCOL_SUSPEND	_IO('o', 0x25)
#define IOCTL_MODEM_PROTOCOL_RESUME	_IO('o', 0x26)

#define IOCTL_MODEM_STATUS		_IO('o', 0x27)

#define IOCTL_MODEM_DL_START		_IO('o', 0x28)
#define IOCTL_MODEM_FW_UPDATE		_IO('o', 0x29)

#define IOCTL_MODEM_NET_SUSPEND		_IO('o', 0x30)
#define IOCTL_MODEM_NET_RESUME		_IO('o', 0x31)

#define IOCTL_MODEM_DUMP_START		_IO('o', 0x32)
#define IOCTL_MODEM_DUMP_UPDATE		_IO('o', 0x33)
#define IOCTL_MODEM_FORCE_CRASH_EXIT	_IO('o', 0x34)
#define IOCTL_MODEM_CP_UPLOAD		_IO('o', 0x35)
#define IOCTL_MODEM_DUMP_RESET		_IO('o', 0x36)

#define IOCTL_LINK_CONNECTED		_IO('o', 0x33)
#define IOCTL_MODEM_SET_TX_LINK		_IO('o', 0x37)

#define IOCTL_MODEM_RAMDUMP_START	_IO('o', 0xCE)
#define IOCTL_MODEM_RAMDUMP_STOP	_IO('o', 0xCF)

#define IOCTL_MODEM_XMIT_BOOT		_IO('o', 0x40)
#ifdef CONFIG_LINK_DEVICE_SHMEM
#define IOCTL_MODEM_GET_SHMEM_INFO	_IO('o', 0x41)
#endif
#define IOCTL_DPRAM_INIT_STATUS		_IO('o', 0x43)

#define IOCTL_LINK_DEVICE_RESET		_IO('o', 0x44)

/* ioctl command for IPC Logger */
#define IOCTL_MIF_LOG_DUMP		_IO('o', 0x51)
#define IOCTL_MIF_DPRAM_DUMP		_IO('o', 0x52)

/*
Definitions for IO devices
*/
#define MAX_IOD_RXQ_LEN		2048

#define CP_CRASH_INFO_SIZE	512
#define CP_CRASH_TAG		"CP Crash "
#define CP_CRASH_BY_RIL		"by RIL: "

#define IPv6			6
#define SOURCE_MAC_ADDR		{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}

/**
@addtogroup group_mem_cp_crash
@{
*/
#define FORCE_CRASH_ACK_TIMEOUT		(10 * HZ)

/* Loopback */
#define CP2AP_LOOPBACK_CHANNEL	30
#define DATA_LOOPBACK_CHANNEL	31

#define DATA_DRAIN_CHANNEL	30	/* Drain channel to drop RX packets */

/* Debugging features */
#define MIF_LOG_DIR		"/sdcard/log"
#define MIF_MAX_PATH_LEN	256

/* Does modem ctl structure will use state ? or status defined below ?*/
enum modem_state {
	STATE_OFFLINE,
	STATE_CRASH_RESET,	/* silent reset */
	STATE_CRASH_EXIT,	/* cp ramdump */
	STATE_BOOTING,
	STATE_ONLINE,
	STATE_NV_REBUILDING,	/* <= rebuilding start */
	STATE_LOADER_DONE,
	STATE_SIM_ATTACH,
	STATE_SIM_DETACH,
};

enum link_state {
	LINK_STATE_OFFLINE = 0,
	LINK_STATE_IPC,
	LINK_STATE_CP_CRASH
};

struct sim_state {
	bool online;	/* SIM is online? */
	bool changed;	/* online is changed? */
};

struct modem_firmware {
	char *binary;
	u32 size;
};

#ifdef CONFIG_COMPAT
struct modem_firmware_64 {
	char *binary;
	unsigned long size;
};
#endif

#define SIPC_MULTI_FRAME_MORE_BIT	(0b10000000)	/* 0x80 */
#define SIPC_MULTI_FRAME_ID_MASK	(0b01111111)	/* 0x7F */
#define SIPC_MULTI_FRAME_ID_BITS	7
#define NUM_SIPC_MULTI_FRAME_IDS	(2 ^ SIPC_MULTI_FRAME_ID_BITS)
#define MAX_SIPC_MULTI_FRAME_ID		(NUM_SIPC_MULTI_FRAME_IDS - 1)

struct __packed sipc_fmt_hdr {
	u16 len;
	u8  msg_seq;
	u8  ack_seq;
	u8  main_cmd;
	u8  sub_cmd;
	u8  cmd_type;
};

/**
@brief		return true if the channel ID is for a CSD
@param ch	channel ID
*/
static inline bool sipc_csd_ch(u8 ch)
{
	return (ch >= SIPC_CH_ID_CS_VT_DATA && ch <= SIPC_CH_ID_CS_VT_VIDEO) ?
		true : false;
}

/**
@brief		return true if the channel ID is for a PS network
@param ch	channel ID
*/
static inline bool sipc_ps_ch(u8 ch)
{
	return (ch >= SIPC_CH_ID_PDP_0 && ch <= SIPC_CH_ID_PDP_14) ?
		true : false;
}

static inline bool sipc_major_ch(u8 ch)
{
	switch (ch) {
	case SIPC_CH_ID_PDP_0:
	case SIPC_CH_ID_PDP_1:
	case SIPC5_CH_ID_FMT_0:
	case SIPC5_CH_ID_RFS_0:
		return true;
	}
	return false;
}

/**
@brief		return true if the channel ID is for CP LOG channel
@param ch	channel ID
*/
static inline bool sipc_log_ch(u8 ch)
{
	return (ch >= SIPC_CH_ID_CPLOG1 && ch <= SIPC_CH_ID_CPLOG2) ?
		true : false;
}

/* If iod->id is 0, do not need to store to `iodevs_tree_fmt' in SIPC4 */
#define sipc4_is_not_reserved_channel(ch) ((ch) != 0)

/* Channel 0, 5, 6, 27, 255 are reserved in SIPC5.
 * see SIPC5 spec: 2.2.2 Channel Identification (Ch ID) Field.
 * They do not need to store in `iodevs_tree_fmt'
 */
#define sipc5_is_not_reserved_channel(ch) \
	((ch) != 0 && (ch) != 5 && (ch) != 6 && (ch) != 27 && (ch) != 255)

struct vnet {
	struct io_device *iod;
	struct link_device *ld;
};

/* for fragmented data from link devices */
struct fragmented_data {
	struct sk_buff *skb_recv;
	struct sipc5_frame_data f_data;
	/* page alloc fail retry*/
	unsigned int realloc_offset;
};
#define fragdata(iod, ld) (&(iod)->fragments[(ld)->link_type])

/** struct skbuff_priv - private data of struct sk_buff
 * this is matched to char cb[48] of struct sk_buff
 */
struct skbuff_private {
	struct io_device *iod;
	struct link_device *ld;

	u32 sipc_ch:8,	/* SIPC Channel Number			*/
	    frm_ctrl:8,	/* Multi-framing control		*/
	    reserved:15,
	    lnk_hdr:1;	/* Existence of a link-layer header	*/
} __packed;

static inline struct skbuff_private *skbpriv(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct skbuff_private) > sizeof(skb->cb));
	return (struct skbuff_private *)&skb->cb;
}

enum iod_rx_state {
	IOD_RX_ON_STANDBY = 0,
	IOD_RX_HEADER,
	IOD_RX_PAYLOAD,
	IOD_RX_PADDING,
	MAX_IOD_RX_STATE
};

static const char const *rx_state_string[] = {
	[IOD_RX_ON_STANDBY]	= "RX_ON_STANDBY",
	[IOD_RX_HEADER]		= "RX_HEADER",
	[IOD_RX_PAYLOAD]	= "RX_PAYLOAD",
	[IOD_RX_PADDING]	= "RX_PADDING",
};

/**
@brief		return RX FSM state as a string.
@param state	the RX FSM state of an I/O device
*/
static const inline char *rx_state(enum iod_rx_state state)
{
	if (unlikely(state >= MAX_IOD_RX_STATE))
		return "INVALID_STATE";
	else
		return rx_state_string[state];
}

struct io_device {
	/* rb_tree node for an io device */
	struct rb_node node_chan;
	struct rb_node node_fmt;

	/* Name of the IO device */
	char *name;

	/* Reference count */
	atomic_t opened;

	/* Wait queue for the IO device */
	wait_queue_head_t wq;

	/* Misc and net device structures for the IO device */
	struct miscdevice  miscdev;
	struct net_device *ndev;
	struct napi_struct napi;

	/* ID and Format for channel on the link */
	unsigned int id;
	enum modem_link link_types;
	enum dev_format format;
	enum modem_io io_typ;
	enum modem_network net_typ;

	/* Attributes of an IO device */
	u32 attrs;

	/* The name of the application that will use this IO device */
	char *app;

	/* Whether or not handover among 2+ link devices */
	bool use_handover;

	/* SIPC version */
	enum sipc_ver ipc_version;

	/* Whether or not IPC is over SBD-based link device */
	bool sbd_ipc;

	/* Whether or not link-layer header is required */
	bool link_header;

	/* Rx queue of sk_buff */
	struct sk_buff_head sk_rx_q;

	/* For keeping multi-frame packets temporarily */
	struct sk_buff_head sk_multi_q[NUM_SIPC_MULTI_FRAME_IDS];

	/* RX state used in RX FSM */
	enum iod_rx_state curr_rx_state;
	enum iod_rx_state next_rx_state;

	/*
	** work for each io device, when delayed work needed
	** use this for private io device rx action
	*/
	struct delayed_work rx_work;

	struct fragmented_data fragments[LINKDEV_MAX];

	/* called by a link device when a packet arrives at this io device */
	int (*recv)(struct io_device *iod, struct link_device *ld,
		    const char *data, unsigned int len);

	/**
	 * If a link device can pass multiple IPC frames per each skb, this
	 * method must be used. (Each IPC frame can be processed with skb_clone
	 * function.)
	 */
	int (*recv_skb)(struct io_device *iod, struct link_device *ld,
			struct sk_buff *skb);

	/**
	 * If a link device passes only one IPC frame per each skb, this method
	 * should be used.
	 */
	int (*recv_skb_single)(struct io_device *iod, struct link_device *ld,
			       struct sk_buff *skb);

	/**
	 * If a link device passes only one NET frame per each skb, this method
	 * should be used.
	 */
	int (*recv_net_skb)(struct io_device *iod, struct link_device *ld,
			    struct sk_buff *skb);

	/* inform the IO device that the modem is now online or offline or
	 * crashing or whatever...
	 */
	void (*modem_state_changed)(struct io_device *iod, enum modem_state);

	/* inform the IO device that the SIM is not inserting or removing */
	void (*sim_state_changed)(struct io_device *iod, bool sim_online);

	struct modem_ctl *mc;
	struct modem_shared *msd;

	struct wake_lock wakelock;
	long waketime;

	/* DO NOT use __current_link directly
	 * you MUST use skbpriv(skb)->ld in mc, link, etc..
	 */
	struct link_device *__current_link;
};
#define to_io_device(misc) container_of(misc, struct io_device, miscdev)

/* get_current_link, set_current_link don't need to use locks.
 * In ARM, set_current_link and get_current_link are compiled to
 * each one instruction (str, ldr) as atomic_set, atomic_read.
 * And, the order of set_current_link and get_current_link is not important.
 */
#define get_current_link(iod) ((iod)->__current_link)
#define set_current_link(iod, ld) ((iod)->__current_link = (ld))

struct link_device {
	struct list_head  list;

	spinlock_t lock;

	enum modem_link link_type;

	struct modem_ctl *mc;

	struct modem_shared *msd;

	enum link_state state;

	/*
	Above are mandatory attributes for all link devices which are set or
	initialized AFTER each link device instance is created.
	========================================================================
	Below are private attributes for each link device which are set or
	initialized WHILE each link device instance is created.
	*/

	char *name;
	bool sbd_ipc;
	bool aligned;

	/* SIPC version */
	enum sipc_ver ipc_version;

	/* Modem data */
	struct modem_data *mdm_data;

	/* Power management */
	struct modem_link_pm pm;

	/* TX queue of socket buffers */
	struct sk_buff_head sk_fmt_tx_q;
	struct sk_buff_head sk_raw_tx_q;
	struct sk_buff_head sk_rfs_tx_q;

	struct sk_buff_head *skb_txq[MAX_SIPC_DEVICES];

	/* RX queue of socket buffers */
	struct sk_buff_head sk_fmt_rx_q;
	struct sk_buff_head sk_raw_rx_q;
	struct sk_buff_head sk_rfs_rx_q;

	struct sk_buff_head *skb_rxq[MAX_SIPC_DEVICES];

	/* Spinlocks for TX & RX */
	spinlock_t tx_lock[MAX_SIPC_DEVICES];
	spinlock_t rx_lock[MAX_SIPC_DEVICES];

	bool raw_tx_suspended; /* for misc dev */
	struct completion raw_tx_resumed_by_cp;

	/**
	 * This flag is for TX flow control on network interface.
	 * This must be set and clear only by a flow control command from CP.
	 */
	bool suspend_netif_tx;

	/* Stop/resume control for network ifaces */
	spinlock_t netif_lock;

	/* bit mask for stopped channel */
	unsigned long netif_stop_mask;
	/* flag of stopped state for all channels */
	atomic_t netif_stopped;

	struct workqueue_struct *tx_wq;
	struct work_struct tx_work;
	struct delayed_work tx_delayed_work;

	struct delayed_work *tx_dwork[MAX_SIPC_DEVICES];
	struct delayed_work fmt_tx_dwork;
	struct delayed_work raw_tx_dwork;
	struct delayed_work rfs_tx_dwork;

	struct workqueue_struct *rx_wq;
	struct work_struct rx_work;
	struct delayed_work rx_delayed_work;

	/* init communication - setting link driver */
	int (*init_comm)(struct link_device *ld, struct io_device *iod);

	/* terminate communication */
	void (*terminate_comm)(struct link_device *ld, struct io_device *iod);

	/* called by an io_device when it has a packet to send over link
	 * - the io device is passed so the link device can look at id and
	 *   format fields to determine how to route/format the packet
	 */
	int (*send)(struct link_device *ld, struct io_device *iod,
		    struct sk_buff *skb);

	/* method for CP booting */
	void (*boot_on)(struct link_device *ld, struct io_device *iod);
	int (*xmit_boot)(struct link_device *ld, struct io_device *iod,
			 unsigned long arg);
	int (*dload_start)(struct link_device *ld, struct io_device *iod);
	int (*firm_update)(struct link_device *ld, struct io_device *iod,
			   unsigned long arg);

	/* methods for CP crash dump */
	int (*force_dump)(struct link_device *ld, struct io_device *iod);
	int (*dump_start)(struct link_device *ld, struct io_device *iod);
	int (*dump_update)(struct link_device *ld, struct io_device *iod,
			   unsigned long arg);
	int (*dump_finish)(struct link_device *ld, struct io_device *iod,
			   unsigned long arg);

	/* IOCTL extension */
	int (*ioctl)(struct link_device *ld, struct io_device *iod,
		     unsigned int cmd, unsigned long arg);

	/* Close (stop) TX with physical link (on CP crash, etc.) */
	void (*close_tx)(struct link_device *ld);

	int (*netdev_poll)(struct napi_struct *napi, int budget);

	/* Above are common methods to all memory-type link devices           */
	/*====================================================================*/
	/* Below are specific methods to each physical link device            */

	/* ready a link_device instance */
	void (*ready)(struct link_device *ld);

	/* reset physical link */
	void (*reset)(struct link_device *ld);

	/* reload physical link set-up */
	void (*reload)(struct link_device *ld);

	/* turn off physical link */
	void (*off)(struct link_device *ld);

	/* Whether the physical link is unmounted or not */
	bool (*unmounted)(struct link_device *ld);

	/* Whether the physical link is suspended or not */
	bool (*suspended)(struct link_device *ld);

	/* Enable/disable IRQ for flow control */
	void (*enable_irq)(struct link_device *ld);
	void (*disable_irq)(struct link_device *ld);
};

#define pm_to_link_device(pm)	container_of(pm, struct link_device, pm)

#ifdef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
#ifdef CONFIG_OF
int link_pm_parse_dt_gpio_pdata(struct device_node *np, struct modem_data *mdm);
#endif

int init_link_device_pm(struct link_device *ld,
			struct modem_link_pm *pm,
			struct link_pm_svc *pm_svc,
			void (*fail_fn)(struct modem_link_pm *),
			void (*cp_fail_fn)(struct modem_link_pm *));

void check_lli_irq(struct modem_link_pm *pm, enum mipi_lli_event event);
#endif

/**
@brief		allocate an skbuff and set skb's iod, ld

@param length	the length to allocate
@param iod	struct io_device *
@param ld	struct link_device *

@retval "NULL"	if there is no free memory.
*/
static inline struct sk_buff *rx_alloc_skb(unsigned int length,
		struct io_device *iod, struct link_device *ld)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(length);
	if (likely(skb)) {
		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;
	}

	return skb;
}

struct modemctl_ops {
	int (*modem_on)(struct modem_ctl *);
	int (*modem_off)(struct modem_ctl *);
	int (*modem_reset)(struct modem_ctl *);
	int (*modem_boot_on)(struct modem_ctl *);
	int (*modem_boot_off)(struct modem_ctl *);
	int (*modem_boot_done)(struct modem_ctl *);
	int (*modem_force_crash_exit)(struct modem_ctl *);
	int (*modem_dump_reset)(struct modem_ctl *);
	int (*modem_dump_start)(struct modem_ctl *);
};

/* for IPC Logger */
struct mif_storage {
	char *addr;
	unsigned int cnt;
};

/* modem_shared - shared data for all io/link devices and a modem ctl
 * msd : mc : iod : ld = 1 : 1 : M : N
 */
struct modem_shared {
	/* list of link devices */
	struct list_head link_dev_list;

	/* Array of pointers to IO devices corresponding to ch[n] */
	struct io_device *ch2iod[256];

	/* Array of active channels */
	u8 ch[256];

	/* The number of active channels in the array @ch[] */
	unsigned int num_channels;

	/* rb_tree root of io devices. */
	struct rb_root iodevs_tree_fmt; /* group by dev_format */

	/* for IPC Logger */
	struct mif_storage storage;
	spinlock_t lock;

	/* CP crash information */
	char cp_crash_info[530];
	bool is_crash_by_ril;

	/* loopbacked IP address
	 * default is 0.0.0.0 (disabled)
	 * after you setted this, you can use IP packet loopback using this IP.
	 * exam: echo 1.2.3.4 > /sys/devices/virtual/misc/umts_multipdp/loopback
	 */
	__be32 loopback_ipaddr;
};

struct modem_ctl {
	struct device *dev;
	char *name;
	struct modem_data *mdm_data;

	struct modem_shared *msd;

	enum modem_state phone_state;
	struct sim_state sim_state;

	/* spin lock for each modem_ctl instance */
	spinlock_t lock;

	/* completion for waiting for CP initialization */
	struct completion init_cmpl;

	/* completion for waiting for CP power-off */
	struct completion off_cmpl;

	/*
	wake_lock for CP booting (download) and/or crash dump (upload)
	- The use of wake_lock for CP booting/crash is dependent on each modem
	  control implementation, so the actual instance of this pointer must be
	  in each modem_ctrl source file.
	*/
	struct wake_lock *wake_lock;

	/*
	variables for CP crash
	1) forced_cp_crash: flag for indicating a forced CP crash
	2) crash_ack_timer: timer for waiting for the ACK of a forced CP crash
	*/
	atomic_t forced_cp_crash;
	struct timer_list crash_ack_timer;

	unsigned int gpio_cp_on;
	unsigned int gpio_cp_off;
	unsigned int gpio_reset_req_n;
	unsigned int gpio_cp_reset;

	/* for broadcasting AP's PM state (active or sleep) */
	unsigned int gpio_pda_active;
	unsigned int int_pda_active;

	/* for checking aliveness of CP */
	unsigned int gpio_phone_active;
	unsigned int irq_phone_active;
	struct modem_irq irq_cp_active;

	/* for AP-CP power management (PM) handshaking */
	unsigned int gpio_ap_wakeup;
	unsigned int irq_ap_wakeup;

	unsigned int gpio_ap_status;
	unsigned int int_ap_status;

	unsigned int gpio_cp_wakeup;
	unsigned int int_cp_wakeup;

	unsigned int gpio_cp_status;
	unsigned int irq_cp_status;

	/* for performance tuning */
	unsigned int gpio_perf_req;
	unsigned int irq_perf_req;

	/* for USB/HSIC PM */
	unsigned int gpio_host_wakeup;
	unsigned int irq_host_wakeup;
	unsigned int gpio_host_active;
	unsigned int gpio_slave_wakeup;

	unsigned int gpio_cp_dump_int;
	unsigned int gpio_ap_dump_int;
	unsigned int gpio_flm_uart_sel;
	unsigned int gpio_cp_warm_reset;

	unsigned int gpio_sim_detect;
	unsigned int irq_sim_detect;

#ifdef CONFIG_LINK_DEVICE_SHMEM
	unsigned int mbx_pda_active;
	unsigned int mbx_phone_active;
	unsigned int mbx_ap_wakeup;
	unsigned int mbx_ap_status;
	unsigned int mbx_cp_wakeup;
	unsigned int mbx_cp_status;
	unsigned int mbx_perf_req;

	/* for system revision information */
	unsigned int sys_rev;
	unsigned int pmic_rev;
	unsigned int pkg_id;
	unsigned int mbx_sys_rev;
	unsigned int mbx_pmic_rev;
	unsigned int mbx_pkg_id;

	struct modem_pmu *pmu;

	/* for checking aliveness of CP */
	struct modem_irq irq_cp_wdt;		/* watchdog timer */
	struct modem_irq irq_cp_fail;
#endif

	struct work_struct pm_qos_work;

	/* Switch with 2 links in a modem */
	unsigned int gpio_link_switch;

	const struct attribute_group *group;

	struct delayed_work dwork;
	struct work_struct work;

	struct modemctl_ops ops;
	struct io_device *iod;
	struct io_device *iod_ds;
	struct io_device *bootd;

	void (*gpio_revers_bias_clear)(void);
	void (*gpio_revers_bias_restore)(void);

	bool need_switch_to_usb;
	bool sim_polarity;

	bool sim_shutdown_req;
	void (*modem_complete)(struct modem_ctl *mc);

	struct notifier_block event_nfb;

	/* Notifier for unmount event from a link device */
	struct notifier_block unmount_nb;

	enum modemctl_event crash_info;
};

static inline bool cp_offline(struct modem_ctl *mc)
{
	if (!mc)
		return true;
	return (mc->phone_state == STATE_OFFLINE);
}

static inline bool cp_online(struct modem_ctl *mc)
{
	if (!mc)
		return false;
	return (mc->phone_state == STATE_ONLINE);
}

static inline bool cp_booting(struct modem_ctl *mc)
{
	if (!mc)
		return false;
	return (mc->phone_state == STATE_BOOTING);
}

static inline bool cp_crashed(struct modem_ctl *mc)
{
	if (!mc)
		return false;
	return (mc->phone_state == STATE_CRASH_EXIT);
}

static inline bool rx_possible(struct modem_ctl *mc)
{
	if (likely(cp_online(mc)))
		return true;

	if (cp_booting(mc) || cp_crashed(mc))
		return true;

	return false;
}

int sipc5_init_io_device(struct io_device *iod);

int mem_netdev_poll(struct napi_struct *napi, int budget);

#endif
