/**
@file		link_device_memory.h
@brief		header file for all types of memory interface media
@date		2014/02/05
@author		Hankook Jang (hankook.jang@samsung.com)
*/

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

#ifndef __MODEM_LINK_DEVICE_MEMORY_H__
#define __MODEM_LINK_DEVICE_MEMORY_H__

#include <linux/cpumask.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/sched/rt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/debugfs.h>

#include "modem_prj.h"
#include "include/link_device_memory_config.h"
#include "include/circ_queue.h"
#include "include/sbd.h"
#include "include/sipc5.h"

#ifdef GROUP_MEM_TYPE
/**
@addtogroup group_mem_type
@{
*/

enum mem_iface_type {
	MEM_EXT_DPRAM = 0x0001,	/* External DPRAM */
	MEM_AP_IDPRAM = 0x0002,	/* DPRAM in AP    */
	MEM_CP_IDPRAM = 0x0004,	/* DPRAM in CP    */
	MEM_PLD_DPRAM = 0x0008,	/* PLD or FPGA    */
	MEM_SYS_SHMEM = 0x0100,	/* Shared-memory (SHMEM) on a system bus   */
	MEM_C2C_SHMEM = 0x0200,	/* SHMEM with C2C (Chip-to-chip) interface */
	MEM_LLI_SHMEM = 0x0400,	/* SHMEM with MIPI-LLI interface           */
};

#define MEM_DPRAM_TYPE_MASK	0x00FF
#define MEM_SHMEM_TYPE_MASK	0xFF00

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_TYPE_SHMEM
/**
@addtogroup group_mem_type_shmem
@{
*/

#define SHM_4M_RESERVED_SZ	4056
#define SHM_4M_FMT_TX_BUFF_SZ	4096
#define SHM_4M_FMT_RX_BUFF_SZ	4096
#define SHM_4M_RAW_TX_BUFF_SZ	2084864
#define SHM_4M_RAW_RX_BUFF_SZ	2097152

#define SHM_UL_USAGE_LIMIT	SZ_32K	/* Uplink burst limit */

struct __packed shmem_4mb_phys_map {
	u32 magic;
	u32 access;

	u32 fmt_tx_head;
	u32 fmt_tx_tail;

	u32 fmt_rx_head;
	u32 fmt_rx_tail;

	u32 raw_tx_head;
	u32 raw_tx_tail;

	u32 raw_rx_head;
	u32 raw_rx_tail;

	char reserved[SHM_4M_RESERVED_SZ];

	char fmt_tx_buff[SHM_4M_FMT_TX_BUFF_SZ];
	char fmt_rx_buff[SHM_4M_FMT_RX_BUFF_SZ];

	char raw_tx_buff[SHM_4M_RAW_TX_BUFF_SZ];
	char raw_rx_buff[SHM_4M_RAW_RX_BUFF_SZ];
};

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_INTERRUPT
/**
@addtogroup group_mem_link_interrupt
@{
*/

#define MASK_INT_VALID		0x0080

#define MASK_CMD_VALID		0x0040
#define MASK_CMD_FIELD		0x003F

#define MASK_REQ_ACK_FMT	0x0020
#define MASK_REQ_ACK_RAW	0x0010
#define MASK_RES_ACK_FMT	0x0008
#define MASK_RES_ACK_RAW	0x0004
#define MASK_SEND_FMT		0x0002
#define MASK_SEND_RAW		0x0001
#define MASK_SEND_DATA		0x0001

#define CMD_INIT_START		0x0001
#define CMD_INIT_END		0x0002
#define CMD_REQ_ACTIVE		0x0003
#define CMD_RES_ACTIVE		0x0004
#define CMD_REQ_TIME_SYNC	0x0005
#define CMD_CRASH_RESET		0x0007
#define CMD_PHONE_START		0x0008
#define CMD_CRASH_EXIT		0x0009
#define CMD_CP_DEEP_SLEEP	0x000A
#define CMD_NV_REBUILDING	0x000B
#define CMD_EMER_DOWN		0x000C
#define CMD_PIF_INIT_DONE	0x000D
#define CMD_SILENT_NV_REBUILD	0x000E
#define CMD_NORMAL_POWER_OFF	0x000F

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_IPC_DEVICE
/**
@addtogroup group_mem_ipc_device
@{
*/

/**
@brief		the structure for each logical IPC device (i.e. virtual lane) in
		a memory-type interface media
*/
struct mem_ipc_device {
	enum dev_format id;
	char name[16];

	struct circ_queue txq;
	struct circ_queue rxq;

	u16 msg_mask;
	u16 req_ack_mask;
	u16 res_ack_mask;

	spinlock_t *tx_lock;
	struct sk_buff_head *skb_txq;

	spinlock_t *rx_lock;
	struct sk_buff_head *skb_rxq;

	unsigned int req_ack_cnt[MAX_DIR];
};

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_FLOW_CONTROL
/**
@addtogroup group_mem_flow_control
@{
*/

#define MAX_SKB_TXQ_DEPTH		1024

#define TX_PERIOD_MS			1	/* 1 ms */
#define MAX_TX_BUSY_COUNT		1024
#define BUSY_COUNT_MASK			0xF

#define RES_ACK_WAIT_TIMEOUT		10	/* 10 ms */

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_SNAPSHOT
/**
@addtogroup group_mem_link_snapshot
@{
*/

struct __packed mem_snapshot {
	/* Timestamp */
	struct timespec ts;

	/* Direction (TX or RX) */
	enum direction dir;

	/* The status of memory interface at the time */
	unsigned int magic;
	unsigned int access;

	unsigned int head[MAX_SIPC5_DEVICES][MAX_DIR];
	unsigned int tail[MAX_SIPC5_DEVICES][MAX_DIR];

	u16 int2ap;
	u16 int2cp;
};

/**
@brief		Memory snapshot (MST) buffer
*/
struct mst_buff {
	/* These two members must be first. */
	struct mst_buff *next;
	struct mst_buff *prev;

	struct mem_snapshot snapshot;
};

struct mst_buff_head {
	/* These two members must be first. */
	struct mst_buff *next;
	struct mst_buff	*prev;

	u32 qlen;
	spinlock_t lock;
};

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_DEVICE
/**
@addtogroup group_mem_link_device
@{
*/

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
enum mem_ipc_mode {
	MEM_LEGACY_IPC,
	MEM_SBD_IPC,
};
#endif

/**
@brief		the structure for a memory-type link device
*/
struct mem_link_device {
	/**
	 * COMMON and MANDATORY to all link devices
	 */
	struct link_device link_dev;

	/**
	 * Global lock for a mem_link_device instance
	 */
	spinlock_t lock;

	/**
	 * MEMORY type
	 */
	enum mem_iface_type type;

	/**
	 * Attributes
	 */
	unsigned long attrs;		/* Set of link_attr_bit flags	*/

	/**
	 * Flags
	 */
	bool dpram_magic;		/* DPRAM-style magic code	*/
	bool iosm;			/* IOSM message			*/

	/**
	 * {physical address, size, virtual address} for BOOT region
	 */
	phys_addr_t boot_start;
	size_t boot_size;
	struct page **boot_pages;	/* pointer to the page table for vmap */
	u8 __iomem *boot_base;

	/**
	 * {physical address, size, virtual address} for IPC region
	 */
	phys_addr_t start;
	size_t size;
	struct page **pages;		/* pointer to the page table for vmap */
	u8 __iomem *base;

	/**
	 * Actual logical IPC devices (for IPC_FMT and IPC_RAW)
	 */
	struct mem_ipc_device ipc_dev[MAX_SIPC5_DEVICES];

	/**
	 * Pointers (aliases) to IPC device map
	 */
	u32 __iomem *magic;
	u32 __iomem *access;
	struct mem_ipc_device *dev[MAX_SIPC5_DEVICES];

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
	struct sbd_link_device sbd_link_dev;
	struct work_struct iosm_w;
#endif

	/**
	 * task & irq affinity cpu mask
	 */
	cpumask_var_t dmask;	/* default cpu mask */
	cpumask_var_t tmask;	/* task affinity cpu mask */
	cpumask_var_t imask;	/* irq affinity cpu mask */

	/**
	 * GPIO#, MBOX#, IRQ# for IPC
	 */
	unsigned int mbx_cp2ap_msg;	/* MBOX# for IPC RX */
	unsigned int irq_cp2ap_msg;	/* IRQ# for IPC RX  */

	unsigned int mbx_ap2cp_msg;	/* MBOX# for IPC TX */
	unsigned int int_ap2cp_msg;	/* INTR# for IPC TX */

	/**
	 * Member variables for TX & RX
	 */
	struct mst_buff_head msb_rxq;
	struct mst_buff_head msb_log;

	struct tasklet_struct rx_tsk;

	struct hrtimer tx_timer;
	struct hrtimer sbd_tx_timer;

	/**
	 * Member variables for CP booting and crash dump
	 */
	struct delayed_work udl_rx_dwork;
	struct std_dload_info img_info;	/* Information of each binary image */
	atomic_t cp_boot_done;

	/**
	 * Mandatory methods for the common memory-type interface framework
	 */
	void (*send_ap2cp_irq)(struct mem_link_device *mld, u16 mask);

	/**
	 * Optional methods for some kind of memory-type interface media
	 */
	u16 (*recv_cp2ap_irq)(struct mem_link_device *mld);
	u16 (*read_ap2cp_irq)(struct mem_link_device *mld);
	void (*finalize_cp_start)(struct mem_link_device *mld);
	void (*unmap_region)(void *rgn);
	void (*debug_info)(void);
	void (*cmd_handler)(struct mem_link_device *mld, u16 cmd);

#ifdef CONFIG_LINK_POWER_MANAGEMENT
#ifdef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
	unsigned int gpio_ap_wakeup;		/* CP-to-AP wakeup GPIO */
	unsigned int gpio_cp_wakeup;		/* AP-to-CP wakeup GPIO */
	unsigned int gpio_cp_status;		/* CP-to-AP status GPIO */
	unsigned int gpio_ap_status;		/* AP-to-CP status GPIO */
#else
	unsigned int gpio_ap_wakeup;		/* CP-to-AP wakeup GPIO */
	struct modem_irq irq_ap_wakeup;		/* CP-to-AP wakeup IRQ  */

	unsigned int gpio_cp_wakeup;		/* AP-to-CP wakeup GPIO */

	unsigned int gpio_cp_status;		/* CP-to-AP status GPIO */
	struct modem_irq irq_cp_status;		/* CP-to-AP status IRQ  */

	unsigned int gpio_ap_status;		/* AP-to-CP status GPIO */

	struct wake_lock ap_wlock;		/* locked by ap_wakeup */
	struct wake_lock cp_wlock;		/* locked by cp_status */

	struct workqueue_struct *pm_wq;
	struct delayed_work cp_sleep_dwork;	/* to hold ap2cp_wakeup */

	spinlock_t pm_lock;

	unsigned long long last_cp2ap_intr;
#endif
	atomic_t ref_cnt;

	unsigned int gpio_ipc_int2cp;		/* AP-to-CP send signal GPIO */
	spinlock_t sig_lock;

	void (*start_pm)(struct mem_link_device *mld);
	void (*stop_pm)(struct mem_link_device *mld);
	void (*forbid_cp_sleep)(struct mem_link_device *mld);
	void (*permit_cp_sleep)(struct mem_link_device *mld);
	bool (*link_active)(struct mem_link_device *mld);
#endif

#ifdef DEBUG_MODEM_IF
	struct dentry *dbgfs_dir;
	struct debugfs_blob_wrapper mem_dump_blob;
	struct dentry *dbgfs_frame;
#endif
};

#define to_mem_link_device(ld) \
		container_of(ld, struct mem_link_device, link_dev)
#define ld_to_mem_link_device(ld) \
		container_of(ld, struct mem_link_device, link_dev)
#define sbd_to_mem_link_device(sl) \
		container_of(sl, struct mem_link_device, sbd_link_dev)

#define MEM_IPC_MAGIC		0xAA
#define MEM_CRASH_MAGIC		0xDEADDEAD
#define MEM_BOOT_MAGIC		0x424F4F54
#define MEM_DUMP_MAGIC		0x44554D50

/**
@}
*/
#endif

#ifdef GROUP_MEM_TYPE
/**
@addtogroup group_mem_type
@{
*/

/**
@brief		check the type of a memory interface medium
@retval true	if @b @@type indicates a type of shared memory media
@retval false	otherwise
*/
static inline bool mem_type_shmem(enum mem_iface_type type)
{
	return (type & MEM_SHMEM_TYPE_MASK) ? true : false;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_INTERRUPT
/**
@addtogroup group_mem_link_interrupt
@{
*/

static inline bool int_valid(u16 x)
{
	return (x & MASK_INT_VALID) ? true : false;
}

static inline u16 mask2int(u16 mask)
{
	return mask | MASK_INT_VALID;
}

/**
@remark		This must be invoked after validation with int_valid().
*/
static inline bool cmd_valid(u16 x)
{
	return (x & MASK_CMD_VALID) ? true : false;
}

static inline u16 int2cmd(u16 x)
{
	return x & MASK_CMD_FIELD;
}

static inline u16 cmd2int(u16 cmd)
{
	return mask2int(cmd | MASK_CMD_VALID);
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_IPC_DEVICE
/**
@addtogroup group_mem_ipc_device
@{
*/

/**
@brief		get a circ_queue (CQ) instance in an IPC device

@param dev	the pointer to a mem_ipc_device instance
@param dir	the direction of communication (TX or RX)

@return		the pointer to the @b @@dir circ_queue (CQ) instance in @b @@dev
*/
static inline struct circ_queue *cq(struct mem_ipc_device *dev,
				    enum direction dir)
{
	return (dir == TX) ? &dev->txq : &dev->rxq;
}

/**
@brief		get the value of the @e head (in) pointer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the value of the @e head pointer in the TXQ of @b @@dev
*/
static inline unsigned int get_txq_head(struct mem_ipc_device *dev)
{
	return get_head(&dev->txq);
}

/**
@brief		set the value of the @e head (in) pointer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance
@param in	the value to be written to the @e head pointer in the TXQ of
		@b @@dev
*/
static inline void set_txq_head(struct mem_ipc_device *dev, unsigned int in)
{
	set_head(&dev->txq, in);
}

/**
@brief		get the value of the @e tail (out) pointer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the value of the @e tail pointer in the TXQ of @b @@dev

@remark		It is useless for AP to read the tail pointer in a TX queue
		twice to verify whether or not the value in the pointer is
		valid, because it can already have been updated by CP after the
		first access from AP.
*/
static inline unsigned int get_txq_tail(struct mem_ipc_device *dev)
{
	return get_tail(&dev->txq);
}

/**
@brief		set the value of the @e tail (out) pointer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance
@param out	the value to be written to the @e tail pointer in the TXQ of
		@b @@dev
*/
static inline void set_txq_tail(struct mem_ipc_device *dev, unsigned int out)
{
	set_tail(&dev->txq, out);
}

/**
@brief		get the start address of the data buffer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the start address of the data buffer in the TXQ of @b @@dev
*/
static inline char *get_txq_buff(struct mem_ipc_device *dev)
{
	return get_buff(&dev->txq);
}

/**
@brief		get the size of the data buffer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the size of the data buffer in the TXQ of @e @@dev
*/
static inline unsigned int get_txq_buff_size(struct mem_ipc_device *dev)
{
	return get_size(&dev->txq);
}

/**
@brief		get the value of the @e head (in) pointer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the value of the @e head pointer in the RXQ of @b @@dev

@remark		It is useless for AP to read the head pointer in an RX queue
		twice to verify whether or not the value in the pointer is
		valid, because it can already have been updated by CP after the
		first access from AP.
*/
static inline unsigned int get_rxq_head(struct mem_ipc_device *dev)
{
	return get_head(&dev->rxq);
}

/**
@brief		set the value of the @e head (in) pointer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance
@param in	the value to be written to the @e head pointer in the RXQ of
		@b @@dev
*/
static inline void set_rxq_head(struct mem_ipc_device *dev, unsigned int in)
{
	set_head(&dev->rxq, in);
}

/**
@brief		get the value of the @e tail (out) pointer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the value of the @e tail pointer in the RXQ of @b @@dev
*/
static inline unsigned int get_rxq_tail(struct mem_ipc_device *dev)
{
	return get_tail(&dev->rxq);
}

/**
@brief		set the value of the @e tail (out) pointer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance
@param out	the value to be written to the @e tail pointer of the RXQ of
		@b @@dev
*/
static inline void set_rxq_tail(struct mem_ipc_device *dev, unsigned int out)
{
	set_tail(&dev->rxq, out);
}

/**
@brief		get the start address of the data buffer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the start address of the data buffer in the RXQ of @b @@dev
*/
static inline char *get_rxq_buff(struct mem_ipc_device *dev)
{
	return get_buff(&dev->rxq);
}

/**
@brief		get the size of the data buffer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the size of the data buffer in the RXQ of @b @@dev
*/
static inline unsigned int get_rxq_buff_size(struct mem_ipc_device *dev)
{
	return get_size(&dev->rxq);
}

/**
@brief		get the MSG_SEND mask value for an IPC device

@param dev	the pointer to a mem_ipc_device instance

@return		the MSG_SEND mask of @b @@dev
*/
static inline u16 msg_mask(struct mem_ipc_device *dev)
{
	return dev->msg_mask;
}

/**
@brief		get the REQ_ACK mask value for an IPC device

@param dev	the pointer to a mem_ipc_device instance

@return		the REQ_ACK mask of @b @@dev
*/
static inline u16 req_ack_mask(struct mem_ipc_device *dev)
{
	return dev->req_ack_mask;
}

/**
@brief		get the RES_ACK mask value for an IPC device

@param dev	the pointer to a mem_ipc_device instance

@return		the RES_ACK mask of @b @@dev
*/
static inline u16 res_ack_mask(struct mem_ipc_device *dev)
{
	return dev->res_ack_mask;
}

/**
@brief		check whether or not @b @@val matches up with the REQ_ACK mask
		of an IPC device

@param dev	the pointer to a mem_ipc_device instance
@param val	the value to be checked

@return		"true" if @b @@val matches up with the REQ_ACK mask of @b @@dev
*/
static inline bool req_ack_valid(struct mem_ipc_device *dev, u16 val)
{
	if (!cmd_valid(val) && (val & req_ack_mask(dev)))
		return true;
	else
		return false;
}

/**
@brief		check whether or not @b @@val matches up with the RES_ACK mask
		of an IPC device

@param dev	the pointer to a mem_ipc_device instance
@param val	the value to be checked

@return		"true" if @b @@val matches up with the RES_ACK mask of @b @@dev
*/
static inline bool res_ack_valid(struct mem_ipc_device *dev, u16 val)
{
	if (!cmd_valid(val) && (val & res_ack_mask(dev)))
		return true;
	else
		return false;
}

/**
@brief		check whether or not an RXQ is empty

@param dev	the pointer to a mem_ipc_device instance

@return		"true" if the RXQ of @b @@dev is empty, i.e. HEAD == TAIL
*/
static inline bool rxq_empty(struct mem_ipc_device *dev)
{
	u32 head;
	u32 tail;
	unsigned long flags;

	spin_lock_irqsave(&dev->rxq.lock, flags);

	head = get_rxq_head(dev);
	tail = get_rxq_tail(dev);

	spin_unlock_irqrestore(&dev->rxq.lock, flags);

	return circ_empty(head, tail);
}

/**
@brief		check whether or not an TXQ is empty

@param dev	the pointer to a mem_ipc_device instance

@return		"true" if the TXQ of @b @@dev is empty, i.e. HEAD == TAIL
*/
static inline bool txq_empty(struct mem_ipc_device *dev)
{
	u32 head;
	u32 tail;
	unsigned long flags;

	spin_lock_irqsave(&dev->txq.lock, flags);

	head = get_txq_head(dev);
	tail = get_txq_tail(dev);

	spin_unlock_irqrestore(&dev->txq.lock, flags);

	return circ_empty(head, tail);
}

/**
@brief		get the ID of the IPC device for a SIPC channel ID

@param ch	the channel ID

@return		the ID of the IPC device for @b @@ch
*/
static inline enum dev_format dev_id(enum sipc_ch_id ch)
{
	return sipc5_fmt_ch(ch) ? IPC_FMT : IPC_RAW;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_DEVICE
/**
@addtogroup group_mem_link_device
@{
*/

static inline unsigned int get_magic(struct mem_link_device *mld)
{
	return ioread32(mld->magic);
}

static inline unsigned int get_access(struct mem_link_device *mld)
{
	return ioread32(mld->access);
}

static inline void set_magic(struct mem_link_device *mld, unsigned int val)
{
	iowrite32(val, mld->magic);
}

static inline void set_access(struct mem_link_device *mld, unsigned int val)
{
	iowrite32(val, mld->access);
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_SNAPSHOT
/**
@addtogroup group_mem_link_snapshot
@{
*/

int msb_init(void);

struct mst_buff *msb_alloc(void);
void msb_free(struct mst_buff *msb);

void msb_queue_head_init(struct mst_buff_head *list);
void msb_queue_tail(struct mst_buff_head *list, struct mst_buff *msb);
void msb_queue_head(struct mst_buff_head *list, struct mst_buff *msb);

struct mst_buff *msb_dequeue(struct mst_buff_head *list);

void msb_queue_purge(struct mst_buff_head *list);

/**
@brief		take a snapshot of the current status of a memory interface

Takes a snapshot of current @b @@dir memory status

@param mld	the pointer to a mem_link_device instance
@param dir	the direction of a communication (TX or RX)

@return		the pointer to a mem_snapshot instance
*/
struct mst_buff *mem_take_snapshot(struct mem_link_device *mld,
				   enum direction dir);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_INTERRUPT
/**
@addtogroup group_mem_link_interrupt
@{
*/

static inline void send_ipc_irq(struct mem_link_device *mld, u16 val)
{
	if (likely(mld->send_ap2cp_irq))
		mld->send_ap2cp_irq(mld, val);
}

void mem_irq_handler(struct mem_link_device *mld, struct mst_buff *msb);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_SETUP
/**
@addtogroup group_mem_link_setup
@{
*/
void __iomem *mem_vmap(phys_addr_t pa, size_t size, struct page *pages[]);
void mem_vunmap(void *va);

int mem_register_boot_rgn(struct mem_link_device *mld, phys_addr_t start,
			  size_t size);
void mem_unregister_boot_rgn(struct mem_link_device *mld);
int mem_setup_boot_map(struct mem_link_device *mld);

int mem_register_ipc_rgn(struct mem_link_device *mld, phys_addr_t start,
			 size_t size);
void mem_unregister_ipc_rgn(struct mem_link_device *mld);
int mem_setup_ipc_map(struct mem_link_device *mld);

struct mem_link_device *mem_create_link_device(enum mem_iface_type type,
					       struct modem_data *modem);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_COMMAND
/**
@addtogroup group_mem_link_command
@{
*/

int mem_reset_ipc_link(struct mem_link_device *mld);
void mem_cmd_handler(struct mem_link_device *mld, u16 cmd);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_FLOW_CONTROL
/**
@addtogroup group_mem_flow_control
@{
*/

void start_tx_flow_ctrl(struct mem_link_device *mld,
			struct mem_ipc_device *dev);
void stop_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev);
int under_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev);
int check_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev);

void send_req_ack(struct mem_link_device *mld, struct mem_ipc_device *dev);
void recv_res_ack(struct mem_link_device *mld, struct mem_ipc_device *dev,
		  struct mem_snapshot *mst);

void recv_req_ack(struct mem_link_device *mld, struct mem_ipc_device *dev,
		  struct mem_snapshot *mst);
void send_res_ack(struct mem_link_device *mld, struct mem_ipc_device *dev);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_CP_CRASH
/**
@addtogroup group_mem_cp_crash
@{
*/

void mem_forced_cp_crash(struct mem_link_device *mld);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_DEBUG
/**
@addtogroup group_mem_link_debug
@{
*/
void print_pm_status(struct mem_link_device *mld);

void print_req_ack(struct mem_link_device *mld, struct mem_snapshot *mst,
		   struct mem_ipc_device *dev, enum direction dir);
void print_res_ack(struct mem_link_device *mld, struct mem_snapshot *mst,
		   struct mem_ipc_device *dev, enum direction dir);

void print_mem_snapshot(struct mem_link_device *mld, struct mem_snapshot *mst);
void print_dev_snapshot(struct mem_link_device *mld, struct mem_snapshot *mst,
			struct mem_ipc_device *dev);

/**
@}
*/
#endif

/*============================================================================*/

static inline struct sk_buff *mem_alloc_skb(unsigned int len)
{
	gfp_t priority;
	struct sk_buff *skb;

	priority = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;

	skb = alloc_skb(len + NET_SKB_PAD, priority);
	if (!skb) {
		mif_err("ERR! alloc_skb(len:%d + pad:%d, gfp:0x%x) fail\n",
			len, NET_SKB_PAD, priority);
#ifdef CONFIG_SEC_DEBUG_MIF_OOM
		show_mem(SHOW_MEM_FILTER_NODES);
#endif
		return NULL;
	}

	skb_reserve(skb, NET_SKB_PAD);
	return skb;
}

#endif
