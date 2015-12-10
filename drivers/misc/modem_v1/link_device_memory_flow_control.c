/**
@file		link_device_memory_flow_control.c
@brief		common functions for all types of memory interface media
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

#ifdef GROUP_MEM_FLOW_CONTROL
/**
@weakgroup group_mem_flow_control
@{
*/

void start_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev)
{
	struct link_device *ld = &mld->link_dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->txq.lock, flags);

	if (atomic_read(&dev->txq.busy) > 0) {
		spin_unlock_irqrestore(&dev->txq.lock, flags);
		mif_err("%s: %s TX already BUSY\n", ld->name, dev->name);
		return;
	}

	atomic_set(&dev->txq.busy, 1);

	spin_unlock_irqrestore(&dev->txq.lock, flags);

	send_req_ack(mld, dev);
}

void stop_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->txq.lock, flags);

	atomic_set(&dev->txq.busy, 0);

	spin_unlock_irqrestore(&dev->txq.lock, flags);

	if (dev->id == IPC_RAW)
		resume_net_ifaces(&mld->link_dev);
}

int under_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev)
{
	int busy;
	unsigned long flags;

	spin_lock_irqsave(&dev->txq.lock, flags);

	busy = atomic_read(&dev->txq.busy);

	spin_unlock_irqrestore(&dev->txq.lock, flags);

	return busy;
}

/**
@brief		check whether or not an IPC device is under TX flow control

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance

@retval "= 0"	NOT under TX flow control
@retval -EBUSY	UNDER TX flow control
@retval -ETIME	timeout in waiting for a RES_ACK from CP
@retval "< 0"	otherwise, an error code
*/
int check_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	int busy_count = atomic_read(&dev->txq.busy);
	unsigned long flags;

	if (txq_empty(dev)) {
#ifdef DEBUG_MODEM_IF_FLOW_CTRL
		if (cp_online(mc)) {
			mif_err("%s->%s: %s_TXQ: No RES_ACK, but EMPTY "
				"(busy_cnt %d)\n", ld->name, mc->name,
				dev->name, busy_count);
		}
#endif
		stop_tx_flow_ctrl(mld, dev);
		return 0;
	}

	spin_lock_irqsave(&dev->txq.lock, flags);
	atomic_inc(&dev->txq.busy);
	spin_unlock_irqrestore(&dev->txq.lock, flags);

	if (busy_count >= MAX_TX_BUSY_COUNT) {
		mif_err("%s->%s: ERR! %s TXQ must be DEAD (busy_cnt %d)\n",
			ld->name, mc->name, dev->name, busy_count);
		return -EPIPE;
	}

	if (cp_online(mc) && count_flood(busy_count, BUSY_COUNT_MASK)) {
		send_req_ack(mld, dev);
		return -ETIME;
	}

	return -EBUSY;
}

void send_req_ack(struct mem_link_device *mld, struct mem_ipc_device *dev)
{
#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	struct mst_buff *msb;
#endif

	send_ipc_irq(mld, mask2int(req_ack_mask(dev)));
	dev->req_ack_cnt[TX] += 1;

#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	msb = mem_take_snapshot(mld, TX);
	if (!msb)
		return;
	print_req_ack(mld, &msb->snapshot, dev, TX);
#if 0
	msb_queue_tail(&mld->msb_log, msb);
#else
	msb_free(msb);
#endif
#endif
}

void recv_res_ack(struct mem_link_device *mld, struct mem_ipc_device *dev,
		  struct mem_snapshot *mst)
{
	dev->req_ack_cnt[TX] -= 1;

	stop_tx_flow_ctrl(mld, dev);

#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	print_res_ack(mld, mst, dev, RX);
#endif
}

void recv_req_ack(struct mem_link_device *mld, struct mem_ipc_device *dev,
		  struct mem_snapshot *mst)
{
	dev->req_ack_cnt[RX] += 1;

#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	print_req_ack(mld, mst, dev, RX);
#endif
}

void send_res_ack(struct mem_link_device *mld, struct mem_ipc_device *dev)
{
#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	struct mst_buff *msb;
#endif

	send_ipc_irq(mld, mask2int(res_ack_mask(dev)));
	dev->req_ack_cnt[RX] -= 1;

#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	msb = mem_take_snapshot(mld, TX);
	if (!msb)
		return;
	print_res_ack(mld, &msb->snapshot, dev, TX);
#if 0
	msb_queue_tail(&mld->msb_log, msb);
#else
	msb_free(msb);
#endif
#endif
}

/**
@}
*/
#endif

