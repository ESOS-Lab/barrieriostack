/**
@file		link_device_memory_debug.c
@brief		functions for debugging memory-type interfaces
@date		2014/02/18
@author		Hankook Jang (hankook.jang@samsung.com)
*/

/*
 * Copyright (C) 2011 Samsung Electronics.
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

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

#ifdef GROUP_MEM_LINK_DEBUG
/**
@weakgroup group_mem_link_debug
@{
*/

void print_pm_status(struct mem_link_device *mld)
{
#ifdef CONFIG_LINK_POWER_MANAGEMENT
	struct link_device *ld = &mld->link_dev;
	unsigned int magic;
	int ap_wakeup;
	int ap_status;
	int cp_wakeup;
	int cp_status;

	magic = get_magic(mld);
	ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
	ap_status = gpio_get_value(mld->gpio_ap_status);
	cp_wakeup = gpio_get_value(mld->gpio_cp_wakeup);
	cp_status = gpio_get_value(mld->gpio_cp_status);

	/*
	** PM {ap_wakeup:cp_wakeup:cp_status:ap_status:magic} <CALLER>
	*/
	mif_info("%s: PM {%d:%d:%d:%d:%X} %d <%pf>\n", ld->name,
		ap_wakeup, cp_wakeup, cp_status, ap_status, magic,
		atomic_read(&mld->ref_cnt), CALLER);
#endif
}

/**
@brief		print a REQ_ACK

Prints a snapshot of the status of the @b @@dev circular queue when AP sends or
receives an REQ_ACK.

@param mld	the pointer to a mem_link_device instance
@param mst	the pointer to a mem_snapshot instance
@param dev	the pointer to a mem_ipc_device instance (IPC_FMT, etc.)
@param dir	the direction of communication (TX or RX)
*/
void print_req_ack(struct mem_link_device *mld, struct mem_snapshot *mst,
		   struct mem_ipc_device *dev, enum direction dir)
{
#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	enum dev_format id = dev->id;
	unsigned int qsize = get_size(cq(dev, dir));
	unsigned int in = mst->head[id][dir];
	unsigned int out = mst->tail[id][dir];
	unsigned int usage = circ_get_usage(qsize, in, out);
	unsigned int space = circ_get_space(qsize, in, out);

	mif_info("REQ_ACK: %s%s%s: %s_%s.%d "
		"{in:%u out:%u usage:%u space:%u}\n",
		ld->name, arrow(dir), mc->name, dev->name, q_dir(dir),
		dev->req_ack_cnt[dir], in, out, usage, space);
#endif
}

/**
@brief		print a RES_ACK

Prints a snapshot of the status of the @b @@dev circular queue when AP sends or
receives an RES_ACK.

@param mld	the pointer to a mem_link_device instance
@param mst	the pointer to a mem_snapshot instance
@param dev	the pointer to a mem_ipc_device instance (IPC_FMT, etc.)
@param dir	the direction of communication (TX or RX)

@remark		When a RES_ACK is sent, the status of the RXQ in @b @@dev must
		be shown. On the contrary, the status of the TXQ in @b @@dev
		must be shown When a RES_ACK is received.
*/
void print_res_ack(struct mem_link_device *mld, struct mem_snapshot *mst,
		   struct mem_ipc_device *dev, enum direction dir)
{
#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	enum dev_format id = dev->id;
	enum direction opp_dir = opposite(dir);	/* opposite direction */
	unsigned int qsize = get_size(cq(dev, opp_dir));
	unsigned int in = mst->head[id][opp_dir];
	unsigned int out = mst->tail[id][opp_dir];
	unsigned int usage = circ_get_usage(qsize, in, out);
	unsigned int space = circ_get_space(qsize, in, out);

	mif_info("RES_ACK: %s%s%s: %s_%s.%d "
		"{in:%u out:%u usage:%u space:%u}\n",
		ld->name, arrow(dir), mc->name, dev->name, q_dir(opp_dir),
		dev->req_ack_cnt[opp_dir], in, out, usage, space);
#endif
}

/**
@brief		print a snapshot of the status of a memory interface

@param mld	the pointer to a mem_link_device instance
@param mst	the pointer to a mem_snapshot instance
*/
void print_mem_snapshot(struct mem_link_device *mld, struct mem_snapshot *mst)
{
#ifdef DEBUG_MODEM_IF
	struct link_device *ld = &mld->link_dev;

	mif_info("%s: [%s] ACC{%X %d} FMT{TI:%u TO:%u RI:%u RO:%u} "
		"RAW{TI:%u TO:%u RI:%u RO:%u} INTR{RX:0x%X TX:0x%X}\n",
		ld->name, ipc_dir(mst->dir), mst->magic, mst->access,
		mst->head[IPC_FMT][TX], mst->tail[IPC_FMT][TX],
		mst->head[IPC_FMT][RX], mst->tail[IPC_FMT][RX],
		mst->head[IPC_RAW][TX], mst->tail[IPC_RAW][TX],
		mst->head[IPC_RAW][RX], mst->tail[IPC_RAW][RX],
		mst->int2ap, mst->int2cp);
#endif
}

/**
@brief		print a snapshot of the status of a circular queue

@param mld	the pointer to a mem_link_device instance
@param mst	the pointer to a mem_snapshot instance
@param dev	the pointer to a mem_ipc_device instance (IPC_FMT, etc.)
*/
void print_dev_snapshot(struct mem_link_device *mld, struct mem_snapshot *mst,
			struct mem_ipc_device *dev)
{
#ifdef DEBUG_MODEM_IF
	struct link_device *ld = &mld->link_dev;
	enum dev_format id = dev->id;

	if (id > IPC_RAW)
		return;

	mif_info("%s: [%s] %s | TXQ{in:%u out:%u} RXQ{in:%u out:%u} | "
		"INTR{0x%02X}\n",
		ld->name, ipc_dir(mst->dir), dev->name,
		mst->head[id][TX], mst->tail[id][TX],
		mst->head[id][RX], mst->tail[id][RX],
		(mst->dir == RX) ? mst->int2ap : mst->int2cp);
#endif
}

#endif
