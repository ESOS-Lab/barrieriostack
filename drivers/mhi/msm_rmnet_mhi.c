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
/*
 * MHI RMNET Network interface
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/msm_rmnet.h>
#include <linux/if_arp.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include "msm_mhi.h"
#include "mhi_macros.h"

#include <linux/ip.h>
#include <net/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/workqueue.h>
#include <linux/ipc_logging.h>

#include "mhi.h"

#define RMNET_MHI_DRIVER_NAME "rmnet_mhi"
#define RMNET_MHI_DEV_NAME    "rmnet_mhi%d"
#define MHI_DEFAULT_MTU        (4096 - 512)
#define MHI_DEFAULT_MRU        (4096 - 512)
 /* TODO: This will go to the MHI Core HDR I guess */
#define MHI_MAX_MRU            0xFFFF
#define MHI_NAPI_WEIGHT_VALUE  12
#define MHI_RX_HEADROOM        64
#define WATCHDOG_TIMEOUT       (30 * HZ)
#define MHI_RMNET_DEVICE_COUNT 1 /* TODO: Will be a compile-time definition */
#define DMA_RANGE_CHECK(dma_addr, size, mask) \
			(((dma_addr) | ((dma_addr) + (size) - 1)) & ~(mask))
#define D_RMNET_DUMP_BUFFER(prestr, cnt, buf) \
do { \
    print_hex_dump(KERN_INFO, prestr, \
        DUMP_PREFIX_NONE, 16, 1, \
        buf, cnt, 0); \
} while (0)

static int rmnet_mhi_remove(struct platform_device *dev);
static int rmnet_mhi_probe(struct platform_device *dev);

typedef enum RMNET_DBG_LEVEL {
	RMNET_DBG_VERBOSE = 0x0,
	RMNET_DBG_INFO = 0x1,
	RMNET_DBG_ERROR = 0x2,
} RMNET_DBG_LEVEL;

RMNET_DBG_LEVEL mhi_rmnet_msg_lvl = RMNET_DBG_ERROR;
RMNET_DBG_LEVEL mhi_rmnet_ipc_log_lvl = RMNET_DBG_VERBOSE;

module_param(mhi_rmnet_msg_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_rmnet_msg_lvl, "dbg lvl");
module_param(mhi_rmnet_ipc_log_lvl, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_rmnet_ipc_log_lvl, "dbg lvl");

static void *mhi_rmnet_ipc_log;
#define mhi_rmnet_log(_msg_lvl, _msg, ...) do { \
		if ((_msg_lvl) >= mhi_rmnet_msg_lvl) \
			pr_info("[%s] " _msg, __func__, ##__VA_ARGS__);\
		if (mhi_rmnet_ipc_log && ((_msg_lvl) >= mhi_rmnet_ipc_log_lvl))	\
			ipc_log_string(mhi_rmnet_ipc_log,			\
			       "[%s] " _msg, __func__, ##__VA_ARGS__);	\
} while (0)

#ifdef CONFIG_MHI_RMNET_DUMP
void mhi_rmnet_dump_log(int log_lvl, const char *prefix_str,
		size_t len, const void *buf)
{
	const u8 *ptr = buf;
	int i, linelen, remaining = len;
	unsigned char linebuf[32 * 3 + 2 + 32 + 1];
	int rowsize = 16;
	int groupsize = 1;

	if(log_lvl < mhi_rmnet_msg_lvl && log_lvl < mhi_rmnet_ipc_log_lvl)
		return;

	len = len < 16 ? len : 16;

	for (i = 0; i < len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;
		hex_dump_to_buffer(ptr + i, linelen, rowsize, groupsize,
				linebuf, sizeof(linebuf), false);

		mhi_rmnet_log(log_lvl, "%s%s\n",prefix_str,linebuf);
	}
}
#endif

static bool is_dns_packet(const u8 *ip_pkt)
{
	struct iphdr *iph = (struct iphdr *)ip_pkt;
	struct udphdr *udph = (struct udphdr *)(ip_pkt + (iph->ihl << 2));

	/* If this packet is not a UDP packet, return here. */
	if (iph->protocol != 17)
		return false;

	if (ntohs(udph->dest) == 53 || ntohs(udph->source) == 53)
		return true;
	else
		return false;
}

static struct platform_driver mhi_rmnet_driver = {
	.driver = {
		.name = "mhi_rmnet",
		.owner = THIS_MODULE,
	},
	.probe = rmnet_mhi_probe,
	.remove = rmnet_mhi_remove,
};

unsigned long tx_interrupts_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_interrupts_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_interrupts_count, "Tx interrupts");

unsigned long rx_interrupts_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_interrupts_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_interrupts_count, "RX interrupts");

unsigned long tx_ring_full_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_ring_full_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_ring_full_count, "RING FULL errors from MHI Core");

unsigned long tx_bounce_buffers_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_bounce_buffers_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_bounce_buffers_count, "TX bounce buffers used");

unsigned long tx_queued_packets_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_queued_packets_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_queued_packets_count, "TX packets queued in MHI core");

unsigned long rx_interrupts_in_masked_irq[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_interrupts_in_masked_irq, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_interrupts_in_masked_irq,
		 "RX interrupts while IRQs are masked");

unsigned long rx_napi_skb_burst_min[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_napi_skb_burst_min, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_napi_skb_burst_min, "MIN SKBs sent to NS during NAPI");

unsigned long rx_napi_skb_burst_max[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_napi_skb_burst_max, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_napi_skb_burst_max, "MAX SKBs sent to NS during NAPI");

unsigned long tx_cb_skb_free_burst_min[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_cb_skb_free_burst_min, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_cb_skb_free_burst_min, "MIN SKBs freed during TX CB");

unsigned long tx_cb_skb_free_burst_max[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_cb_skb_free_burst_max, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_cb_skb_free_burst_max, "MAX SKBs freed during TX CB");

unsigned long rx_napi_budget_overflow[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_napi_budget_overflow, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_napi_budget_overflow,
		 "Budget hit with more items to read counter");

struct rmnet_mhi_private {
	int                           dev_index;
	mhi_client_handle            *tx_client_handle;
	mhi_client_handle            *rx_client_handle;
	MHI_CLIENT_CHANNEL            tx_channel;
	MHI_CLIENT_CHANNEL            rx_channel;
	struct sk_buff_head           tx_buffers;
	struct sk_buff_head           rx_buffers;
	uint32_t                      mru;
	struct napi_struct            napi;
	struct sk_buff_head           tx_bounce_buffers;
	gfp_t                         allocation_flags;
	uint32_t                      tx_buffers_max;
	uint32_t                      rx_buffers_max;
	atomic_t		      irq_masked;

	struct work_struct           alloc_skb_work;
	struct workqueue_struct	     *alloc_work_queue;

	cpumask_var_t                 dmask; /* default_cpu_mask*/
	cpumask_var_t                 imask; /* irq_affinity_cpu_mask*/
};

struct tx_buffer_priv {
	dma_addr_t dma_addr;
	bool is_bounce_buffer;
};

static struct net_device *mhi_rmnet_devices[MHI_RMNET_DEVICE_COUNT] = { 0 };

static dma_addr_t rmnet_mhi_internal_get_dma_addr(struct sk_buff *skb,
						  enum dma_data_direction dir)
{
	if (DMA_TO_DEVICE == dir) {
		struct tx_buffer_priv *tx_priv =
			(struct tx_buffer_priv *)(skb->cb);
		return tx_priv->dma_addr;
	} else /* DMA_FROM_DEVICE */{
		uintptr_t *cb_ptr = 0;
		cb_ptr = (uintptr_t *)skb->cb;
		return (dma_addr_t)(uintptr_t)(*cb_ptr);
	}
}

static void rmnet_mhi_internal_clean_unmap_buffers(struct net_device *dev,
						   struct sk_buff_head *queue,
						   enum dma_data_direction dir)
{
	while (!skb_queue_empty(queue)) {
		struct sk_buff *skb = skb_dequeue(queue);
		if (0 != skb) {
			dma_addr_t dma_addr =
				rmnet_mhi_internal_get_dma_addr(skb, dir);
			dma_unmap_single(&(dev->dev), dma_addr, skb->len, dir);
			kfree_skb(skb);
		}
	}
}

static __be16 rmnet_mhi_ip_type_trans(struct sk_buff *skb)
{
	__be16 protocol = 0;

	/* Determine L3 protocol */
	switch (skb->data[0] & 0xf0) {
	case 0x40:
		protocol = htons(ETH_P_IP);
		break;
	case 0x60:
		protocol = htons(ETH_P_IPV6);
		break;
	default:
		/* Default is QMAP */
		protocol = htons(ETH_P_MAP);
		break;
	}
	return protocol;
}

void alloc_skb_work(struct work_struct *work)
{
	struct sk_buff *skb;
	struct net_device *dev = mhi_rmnet_devices[0];
	struct rmnet_mhi_private *rmnet_mhi_ptr = netdev_priv(dev);
	MHI_STATUS res = MHI_STATUS_reserved;
	uintptr_t *cb_ptr;
	dma_addr_t dma_addr;

	while(1) {
		skb = alloc_skb(rmnet_mhi_ptr->mru, GFP_DMA);

		if (unlikely(0 == skb)) {
			/* TODO: Handle error */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Can't allocate a new RX buffer for MHI",
					__func__);
			atomic_inc(&rmnet_mhi_ptr->rx_client_handle->mhi_dev_ctxt->counters.skb_alloc_fail_cntr);
			break;
		}

		skb_reserve(skb, MHI_RX_HEADROOM);

		cb_ptr = (uintptr_t *)skb->cb;
		dma_addr = dma_map_single(&(dev->dev), skb->data,
				rmnet_mhi_ptr->mru - MHI_RX_HEADROOM,
				DMA_FROM_DEVICE);
		*cb_ptr = (uintptr_t)dma_addr;

		if (unlikely(dma_mapping_error(&(dev->dev), dma_addr))) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: DMA mapping error in polling function",
					__func__);
			/* TODO: Handle error */
			kfree_skb(skb);
			break;
		}

		/* TODO: What do we do in such a scenario in
		   which we can't allocate a RX buffer? */
		if (unlikely(DMA_RANGE_CHECK(dma_addr,
						rmnet_mhi_ptr->mru,
						MHI_DMA_MASK))) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: RX buffer is out of MHI DMA address range",
					__func__);
			dma_unmap_single(&(dev->dev), dma_addr,
					(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
					DMA_FROM_DEVICE);
			kfree_skb(skb);
			break;
		}

		res = mhi_queue_xfer(
				rmnet_mhi_ptr->rx_client_handle,
				(uintptr_t)dma_addr, rmnet_mhi_ptr->mru, 0, 0);

		if (unlikely(MHI_STATUS_SUCCESS != res)) {
			/* TODO: Handle error */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: mhi_queue_xfer failed, error %d",
					__func__, res);
			dma_unmap_single(&(dev->dev), dma_addr,
					(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
					DMA_FROM_DEVICE);

			kfree_skb(skb);
			break;
		}


		skb_queue_tail(&(rmnet_mhi_ptr->rx_buffers), skb);

		if(rmnet_mhi_ptr->rx_buffers.qlen == rmnet_mhi_ptr->rx_buffers_max)
			break;

		mdelay(1);
	}
	mhi_rmnet_log(RMNET_DBG_ERROR,"%s MHI RX DATA re_alloc routine end, rx_qlen : %d\n",
				__func__,rmnet_mhi_ptr->rx_buffers.qlen);
	napi_reschedule(&rmnet_mhi_ptr->napi);
}

/* TODO: No error handling yet */
static int rmnet_mhi_poll(struct napi_struct *napi, int budget)
{
	int received_packets = 0;
	struct net_device *dev = napi->dev;
	struct rmnet_mhi_private *rmnet_mhi_ptr = netdev_priv(dev);
	MHI_STATUS res = MHI_STATUS_reserved;
	bool should_reschedule = true;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	uintptr_t *cb_ptr;
	bool do_alloc_work = false ;
	/* Reset the watchdog? */

	while (received_packets < budget) {
		mhi_result *result =
		      mhi_poll(rmnet_mhi_ptr->rx_client_handle);

		if(result->transaction_status == MHI_STATUS_DEVICE_NOT_READY) {
			continue;
		} else if (result->transaction_status != MHI_STATUS_SUCCESS) {
			/* TODO: Handle error */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: mhi_poll failed, error is %d",
			       __func__, result->transaction_status);
			break;
		}

		/* Nothing more to read, or out of buffers in MHI layer */
		if (unlikely(0 == result->payload_buf || 0 == result->bytes_xferd)) {
			should_reschedule = false;
			break;
		}

		/* Assumption
		   ----------
		   The buffer returned back is guaranteed to be the first buffer
		   that was allocated for the RX, so we just dequeue the head.
		*/

		/* Take the first one */
		skb = skb_dequeue(&(rmnet_mhi_ptr->rx_buffers));
		if (unlikely(0 == skb)) {
			/* TODO: This shouldn't happen, we had a guard above */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: No RX buffers to match", __func__);
			break;
		}

		cb_ptr = (uintptr_t *)skb->cb;
		dma_addr = (dma_addr_t)(uintptr_t)(*cb_ptr);

		/* Sanity check, ensuring that this is actually the buffer */
		if (unlikely((uintptr_t)dma_addr != (uintptr_t)result->payload_buf)) {
			/* TODO: Handle error */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Unexpected physical address mismatch, expected 0x%lx, got 0x%lx",
			       __func__, (uintptr_t)dma_addr, (uintptr_t)result->payload_buf);
			break;
		}

		dma_unmap_single(&(dev->dev), dma_addr,
				(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
				 DMA_FROM_DEVICE);
		skb_put(skb, result->bytes_xferd);

		skb->dev = dev;
		skb->protocol = rmnet_mhi_ip_type_trans(skb);

		if(is_dns_packet((u8 *)(skb->data + 4)))
		{
			mhi_rmnet_log(RMNET_DBG_ERROR,"[RX] len %zu, type 0x%x\n",
				skb->len + sizeof (struct ethhdr), skb->protocol);
			D_RMNET_DUMP_BUFFER("[RX] ", (skb->len > 16 ? 16 : skb->len), skb->data);
		}

#ifdef CONFIG_MHI_RMNET_DUMP
		mhi_rmnet_dump_log(RMNET_DBG_VERBOSE, "mhi_rmnet poll[RX]: ", (skb->len > 16 ? 16 : skb->len), skb->data);
#endif
		netif_receive_skb(skb);

		/* Statistics */
		received_packets++;
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += result->bytes_xferd;

		/* Need to allocate a new buffer instead of this one
		   (TODO: Maybe we can do it @ the end?)
		 */
		if(rmnet_mhi_ptr->rx_buffers.qlen < 20){
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s do alloc work qlen : %d\n",
					__func__,rmnet_mhi_ptr->rx_buffers.qlen);
			do_alloc_work = true ;
			break;
		}

		skb = alloc_skb(rmnet_mhi_ptr->mru, GFP_ATOMIC | GFP_DMA  | __GFP_MEMALLOC);

		if (unlikely(0 == skb)) {
			/* TODO: Handle error */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Can't allocate a new RX buffer for MHI",
			       __func__);
			atomic_inc(&rmnet_mhi_ptr->rx_client_handle->mhi_dev_ctxt->counters.skb_alloc_fail_cntr);
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s : skb_alloc_fail_cntr %d\n", __func__,
				atomic_read(&rmnet_mhi_ptr->rx_client_handle->mhi_dev_ctxt->counters.skb_alloc_fail_cntr));
			break;
		}

		skb_reserve(skb, MHI_RX_HEADROOM);

		cb_ptr = (uintptr_t *)skb->cb;
		dma_addr = dma_map_single(&(dev->dev), skb->data,
					  rmnet_mhi_ptr->mru - MHI_RX_HEADROOM,
					  DMA_FROM_DEVICE);
		*cb_ptr = (uintptr_t)dma_addr;

		if (unlikely(dma_mapping_error(&(dev->dev), dma_addr))) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: DMA mapping error in polling function",
			       __func__);
			/* TODO: Handle error */
			kfree_skb(skb);
			break;
		}

		/* TODO: What do we do in such a scenario in
			which we can't allocate a RX buffer? */
		if (unlikely(DMA_RANGE_CHECK(dma_addr,
				    rmnet_mhi_ptr->mru,
				    MHI_DMA_MASK))) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: RX buffer is out of MHI DMA address range",
			       __func__);
			dma_unmap_single(&(dev->dev), dma_addr,
					(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
							 DMA_FROM_DEVICE);
			kfree_skb(skb);
			break;
		}

		res = mhi_queue_xfer(
			rmnet_mhi_ptr->rx_client_handle,
			(uintptr_t)dma_addr, rmnet_mhi_ptr->mru, 0, 0);

		if (unlikely(MHI_STATUS_SUCCESS != res)) {
			/* TODO: Handle error */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: mhi_queue_xfer failed, error %d",
			       __func__, res);
			dma_unmap_single(&(dev->dev), dma_addr,
					(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
							 DMA_FROM_DEVICE);

			kfree_skb(skb);
			break;
		}

		skb_queue_tail(&(rmnet_mhi_ptr->rx_buffers), skb);

	} /* while (received_packets < budget) or any other error */


	/* We got a NULL descriptor back */
	if(do_alloc_work == true) {
		if(received_packets < budget)
			napi_complete(napi);

		// rx buffer is decreased, alloc first
		queue_work(rmnet_mhi_ptr->alloc_work_queue,
				&rmnet_mhi_ptr->alloc_skb_work);
	} else {
		napi_complete(napi);
		if (false == should_reschedule) {
			if (atomic_read(&rmnet_mhi_ptr->irq_masked)) {
				atomic_dec(&rmnet_mhi_ptr->irq_masked);
				mhi_unmask_irq(rmnet_mhi_ptr->rx_client_handle);
			}
		} else {
			if (received_packets == budget)
				rx_napi_budget_overflow[rmnet_mhi_ptr->dev_index]++;
			napi_reschedule(napi);
		}
	}

	/* Start a watchdog? */

	rx_napi_skb_burst_min[rmnet_mhi_ptr->dev_index] =
	min((unsigned long)received_packets,
	    rx_napi_skb_burst_min[rmnet_mhi_ptr->dev_index]);

	rx_napi_skb_burst_max[rmnet_mhi_ptr->dev_index] =
	max((unsigned long)received_packets,
	    rx_napi_skb_burst_max[rmnet_mhi_ptr->dev_index]);

	return received_packets;
}

void rmnet_mhi_tx_cb(mhi_cb_info *cb_info)
{
	struct net_device *dev = NULL;
	struct rmnet_mhi_private *rmnet_mhi_ptr;
	unsigned long burst_counter = 0;
	mhi_result* result;

	if (NULL != cb_info && NULL != cb_info->result) {
		result = cb_info->result;
		dev = (struct net_device *)result->user_data;
		rmnet_mhi_ptr = netdev_priv(dev);

		switch (cb_info->cb_reason) {
			case MHI_CB_MHI_DISABLED:
				mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Got SSR notification %d from MHI CORE. Stopping stack.",
						__func__, cb_info->cb_reason);
				netif_stop_queue(dev);
				break;
			case MHI_CB_MHI_ENABLED:
				mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Got SSR notification %d from MHI CORE. Starting stack.",
						__func__, cb_info->cb_reason);
				netif_start_queue(dev);
				break;
			case MHI_CB_XFER_SUCCESS:
				tx_interrupts_count[rmnet_mhi_ptr->dev_index]++;

				if (0 == result->payload_buf || 0 == result->bytes_xferd) {
					return;
				}

				/* TODO: The code below might be "too much" for this TX context.
				   There might be a need to either optimize this code or move it to
				   a different thread/task */

				/* Free the buffers which are TX'd up to the provided address */
				while (!skb_queue_empty(&(rmnet_mhi_ptr->tx_buffers))) {
					struct sk_buff *skb = skb_dequeue(&(rmnet_mhi_ptr->tx_buffers));
					if (0 == skb) {
						/* Indicates an error and MHI Core should be reset */
						MHI_STATUS ret;
						ret = mhi_reset_channel(
								rmnet_mhi_ptr->tx_client_handle);
						if (MHI_STATUS_SUCCESS != ret) {
							mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Channel reset failed, error %d",
									__func__, ret);
							/* TODO: How do we handle this error? */
						}
						break;
					} else {
						struct tx_buffer_priv *tx_priv =
							(struct tx_buffer_priv *)(skb->cb);
						dma_addr_t dma_addr = tx_priv->dma_addr;
						int data_len = skb->len;

						if(is_dns_packet((u8 *)(skb->data + 4)))
						{
							mhi_rmnet_log(RMNET_DBG_ERROR,"[TX_CB] len %zu, type 0x%x\n",
									skb->len + sizeof (struct ethhdr), skb->protocol);
							D_RMNET_DUMP_BUFFER("[TX_CB] ", (skb->len > 16 ? 16 : skb->len), skb->data);
						}

#ifdef CONFIG_MHI_RMNET_DUMP
						mhi_rmnet_dump_log(RMNET_DBG_VERBOSE, "mhi_rmnet txcb[CB]: ", (skb->len > 16 ? 16 : skb->len), skb->data);
#endif
						/* Re-use the bounce buffer */
						if (tx_priv->is_bounce_buffer){
							// reset bounce buffer length
							skb_trim(skb, 0);
							skb_queue_tail(
									&(rmnet_mhi_ptr->tx_bounce_buffers),
									skb);
						} else {
							dma_unmap_single(&(dev->dev), dma_addr,
									skb->len, DMA_TO_DEVICE);
							kfree_skb(skb);
						}
						burst_counter++;

						/* Update statistics */
						dev->stats.tx_packets++;
						dev->stats.tx_bytes += data_len;

						/* The payload is expected to be the physical address.
						   Comparing to see if it's the last skb to replenish */
						if (dma_addr == (dma_addr_t)(uintptr_t)result->payload_buf)
							break;
					}
				} /* While TX queue is not empty */

				tx_cb_skb_free_burst_min[rmnet_mhi_ptr->dev_index] =
					min(burst_counter,
							tx_cb_skb_free_burst_min[rmnet_mhi_ptr->dev_index]);

				tx_cb_skb_free_burst_max[rmnet_mhi_ptr->dev_index] =
					max(burst_counter,
							tx_cb_skb_free_burst_max[rmnet_mhi_ptr->dev_index]);

				/* In case we couldn't write again, now we can! */
				netif_wake_queue(dev);
				break;
			default:
				mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Got SSR notification %d from MHI CORE.",
						__func__, cb_info->cb_reason);
				break;
		}
	}
}

void rmnet_mhi_rx_cb(mhi_cb_info *cb_info)
{
	struct net_device *dev;
	struct rmnet_mhi_private *rmnet_mhi_ptr;
	if (NULL != cb_info && NULL != cb_info->result) {
		dev = (struct net_device *)cb_info->result->user_data;
		rmnet_mhi_ptr = netdev_priv(dev);

		switch(cb_info->cb_reason) {
			case MHI_CB_XFER_SUCCESS:
			case MHI_CB_MHI_ENABLED:
				break;
			case MHI_CB_MHI_DISABLED:
				return;
				break;
			default:
				mhi_rmnet_log(RMNET_DBG_ERROR,"%s(): Received bad return code %d from core", __func__,
						cb_info->cb_reason);
				break;
		}
		rx_interrupts_count[rmnet_mhi_ptr->dev_index]++;

		/* Disable interrupts */
		mhi_mask_irq(rmnet_mhi_ptr->rx_client_handle);
		atomic_inc(&rmnet_mhi_ptr->irq_masked);

		/* We need to start a watchdog here, not sure how to do that yet */

		if (napi_schedule_prep(&(rmnet_mhi_ptr->napi)))
			__napi_schedule(&(rmnet_mhi_ptr->napi));
		else
			rx_interrupts_in_masked_irq[rmnet_mhi_ptr->dev_index]++;
	}
}

/* TODO: BIG TODO here, not sure yet what needs to be cleaned and when,
	depending on the state machine */
void rmnet_mhi_reset_cb(void *user_data)
{
	struct net_device *dev = (struct net_device *)user_data;
	struct rmnet_mhi_private *rmnet_mhi_ptr = netdev_priv(dev);


	/* Clean TX buffers */
	rmnet_mhi_internal_clean_unmap_buffers(dev,
					       &(rmnet_mhi_ptr->tx_buffers),
					       DMA_TO_DEVICE);

	/* Clean TX bounce buffers */
	rmnet_mhi_internal_clean_unmap_buffers(dev,
					    &(rmnet_mhi_ptr->tx_bounce_buffers),
					    DMA_TO_DEVICE);

	/* Clean RX buffers */
	rmnet_mhi_internal_clean_unmap_buffers(dev,
					       &(rmnet_mhi_ptr->rx_buffers),
					       DMA_FROM_DEVICE);

}

static mhi_client_info_t tx_cbs = { rmnet_mhi_tx_cb, 1};
static mhi_client_info_t rx_cbs = { rmnet_mhi_rx_cb, 1};
static int mhi_rmnet_initialized = 0;
static int rmnet_mhi_open(struct net_device *dev)
{
	MHI_STATUS res = MHI_STATUS_reserved;
	struct rmnet_mhi_private *rmnet_mhi_ptr = netdev_priv(dev);
	int index = 0;

	if (mhi_rmnet_initialized) {
		napi_enable(&(rmnet_mhi_ptr->napi));
		netif_start_queue(dev);
		return 0;
	}
	mhi_rmnet_log(RMNET_DBG_ERROR,"%s: First time channel open", __func__);
	mhi_rmnet_initialized = 1;

	res = mhi_open_channel(
		&(rmnet_mhi_ptr->tx_client_handle),
		rmnet_mhi_ptr->tx_channel, 0,
		&tx_cbs, (void *)dev);

	if (MHI_STATUS_SUCCESS != res) {
		rmnet_mhi_ptr->tx_client_handle = 0;
		mhi_rmnet_log(RMNET_DBG_ERROR,"%s: mhi_open_channel failed for TX, error is %d",
		       __func__, res);
		goto cleanup;
	}

	res = mhi_open_channel(
		&(rmnet_mhi_ptr->rx_client_handle),
		rmnet_mhi_ptr->rx_channel, 0,
		&rx_cbs, (void *)dev);

	if (MHI_STATUS_SUCCESS != res) {
		rmnet_mhi_ptr->rx_client_handle = 0;
		mhi_rmnet_log(RMNET_DBG_ERROR,"%s: mhi_open_channel failed for RX, error is %d",
		       __func__, res);
		goto cleanup;

	}

	rmnet_mhi_ptr->tx_buffers_max =
		mhi_get_max_buffers(
			rmnet_mhi_ptr->tx_client_handle);
	rmnet_mhi_ptr->rx_buffers_max =
		mhi_get_max_buffers(
			rmnet_mhi_ptr->rx_client_handle);

	skb_queue_head_init(&(rmnet_mhi_ptr->tx_buffers));

	/* Create RX buffers for MHI core */

	skb_queue_head_init(&(rmnet_mhi_ptr->rx_buffers));

	for (index = 0; index < rmnet_mhi_ptr->rx_buffers_max; index++) {
		struct sk_buff *skb = 0;
		dma_addr_t dma_addr;
		uintptr_t *cb_ptr = 0;

		skb = alloc_skb(rmnet_mhi_ptr->mru,
				rmnet_mhi_ptr->allocation_flags);

		if (0 == skb) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: SKB allocation failure during open",
			       __func__);
			goto cleanup;
		}

		skb_reserve(skb, MHI_RX_HEADROOM);

		cb_ptr = (uintptr_t *)skb->cb;

		dma_addr = dma_map_single(&(dev->dev), skb->data,
				      (rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
				      DMA_FROM_DEVICE);
		*cb_ptr = (uintptr_t)dma_addr;
		if (dma_mapping_error(&(dev->dev), dma_addr)) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: DMA mapping for RX buffers has failed",
			       __func__);
			kfree_skb(skb);
			goto cleanup;
		}

		skb_queue_tail(&(rmnet_mhi_ptr->rx_buffers), skb);

	}

	/* Create bounce buffers for TX */

	skb_queue_head_init(&(rmnet_mhi_ptr->tx_bounce_buffers));

	for (index = 0; index < rmnet_mhi_ptr->tx_buffers_max; index++) {
		struct sk_buff *skb = alloc_skb(dev->mtu,
					rmnet_mhi_ptr->allocation_flags);
		struct tx_buffer_priv *tx_priv = 0;
		if (0 == skb) {
			/* TODO: Handle error */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: SKB allocation failure during open",
			       __func__);
			goto cleanup;
		}

		tx_priv = (struct tx_buffer_priv *)(skb->cb);
		tx_priv->is_bounce_buffer = true;

		skb_queue_tail(&(rmnet_mhi_ptr->tx_bounce_buffers), skb);
	}

	/* Submit the RX buffers */
	for (index = 0; index < rmnet_mhi_ptr->rx_buffers_max; index++) {
		struct sk_buff *skb = skb_dequeue(&(rmnet_mhi_ptr->rx_buffers));
		/* TODO: Rework the casting here */
		res = mhi_queue_xfer(
			rmnet_mhi_ptr->rx_client_handle,
			(uintptr_t)(*(uintptr_t *)(skb->cb)),
			rmnet_mhi_ptr->mru, 0, 0);
		if (MHI_STATUS_SUCCESS != res) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: mhi_queue_xfer failed, error %d",
			       __func__, res);
			/* TODO: Handle this error. Do we reset the MHI Core? */
			goto cleanup;
		}

		 skb_queue_tail(&(rmnet_mhi_ptr->rx_buffers), skb);
	}

	rmnet_mhi_ptr->alloc_work_queue = create_singlethread_workqueue("mhi_rmnet");
	if (NULL == rmnet_mhi_ptr->alloc_work_queue) {
		mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Failed to create MHI RMNET work queue.\n",__func__);
	}
	INIT_WORK(&rmnet_mhi_ptr->alloc_skb_work, alloc_skb_work);

	napi_enable(&(rmnet_mhi_ptr->napi));
	netif_start_queue(dev);

#ifdef CONFIG_ARGOS
	if (!zalloc_cpumask_var(&rmnet_mhi_ptr->dmask, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&rmnet_mhi_ptr->imask, GFP_KERNEL))
		return -ENOMEM;

	/* Below hard-coded mask values should be removed later on.
	 * Like net-sysfs, argos module also should support sysfs knob,
	 * so that user layer must be able to control these cpu mask. */

	cpumask_copy(rmnet_mhi_ptr->dmask, &hmp_slow_cpu_mask);

	argos_irq_affinity_setup_label(
		MSI_TO_IRQ(rmnet_mhi_ptr->rx_client_handle->mhi_dev_ctxt,
				rmnet_mhi_ptr->rx_client_handle->msi_vec),
		"QIPC", rmnet_mhi_ptr->imask, rmnet_mhi_ptr->dmask);
#endif

	return 0;

cleanup:
	if (0 != rmnet_mhi_ptr->tx_client_handle)
		mhi_close_channel(rmnet_mhi_ptr->tx_client_handle);

	if (0 != rmnet_mhi_ptr->rx_client_handle)
		mhi_close_channel(rmnet_mhi_ptr->rx_client_handle);

  /* Clean TX bounce buffers */
	rmnet_mhi_internal_clean_unmap_buffers(dev,
					    &(rmnet_mhi_ptr->tx_bounce_buffers),
					    DMA_TO_DEVICE);

	/* Clean RX buffers */
	rmnet_mhi_internal_clean_unmap_buffers(dev,
					       &(rmnet_mhi_ptr->rx_buffers),
					       DMA_FROM_DEVICE);

	return -ENODEV;
}

static int rmnet_mhi_close(struct net_device *dev)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr = netdev_priv(dev);

	napi_disable(&(rmnet_mhi_ptr->napi));
	if (atomic_read(&rmnet_mhi_ptr->irq_masked)) {
		atomic_dec(&rmnet_mhi_ptr->irq_masked);
		mhi_unmask_irq(rmnet_mhi_ptr->rx_client_handle);
	}

	return 0;
}

static int rmnet_mhi_stop(struct net_device *dev)
{
	rmnet_mhi_close(dev);
	netif_stop_queue(dev);
	return 0;
}

static int rmnet_mhi_change_mtu(struct net_device *dev, int new_mtu)
{
	if (0 > new_mtu || MHI_MAX_MTU < new_mtu)
		return -EINVAL;

	dev->mtu = new_mtu;


	return 0;
}

static int rmnet_mhi_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr = netdev_priv(dev);
	MHI_STATUS res = MHI_STATUS_reserved;
	dma_addr_t dma_addr;
	bool bounce_buffer_used = false;
	struct sk_buff *old_skb;

	if(is_dns_packet((u8 *)(skb->data + 4)))
	{
		mhi_rmnet_log(RMNET_DBG_ERROR,"[TX] len %zu, type 0x%x\n",
			skb->len + sizeof (struct ethhdr), skb->protocol);
		D_RMNET_DUMP_BUFFER("[TX] ", (skb->len > 16 ? 16 : skb->len), skb->data);
	}

#ifdef CONFIG_MHI_RMNET_DUMP
	mhi_rmnet_dump_log(RMNET_DBG_VERBOSE, "mhi_rmnet xmit[TX]: ", (skb->len > 16 ? 16 : skb->len), skb->data);
#endif
	/* Lets map it first! */
	dma_addr = dma_map_single(&(dev->dev), skb->data, skb->len,
				  DMA_TO_DEVICE);
	if (dma_mapping_error(&(dev->dev), dma_addr)) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: DMA mapping error in transmit function",
			       __func__);
			/* TODO: How do we handle this? */
			return NETDEV_TX_BUSY;
	}

	if (!VALID_BUF(dma_addr, skb->len)) {
		struct sk_buff *bounce_skb = 0;

		bounce_buffer_used = true;

		/* Packet not in range. Use a bounce buffer if available */
		dma_unmap_single(&(dev->dev), dma_addr, skb->len,
				 DMA_TO_DEVICE);

		bounce_skb = skb_dequeue(&(rmnet_mhi_ptr->tx_bounce_buffers));

		if (0 == bounce_skb) {
			/* This shouldn't happen */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: No available bounce TX buffers",
			       __func__);
			netif_stop_queue(dev);
			return NETDEV_TX_BUSY;
		}

		skb_copy_from_linear_data(skb, bounce_skb->data, skb->len);
		skb_put(bounce_skb, skb->len);

		old_skb = skb;

		skb = bounce_skb;
		dma_addr = dma_map_single(&(dev->dev), skb->data, skb->len, DMA_TO_DEVICE);
		if (dma_mapping_error(&(dev->dev), dma_addr)) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"Bounced buffer DMA mapping error in transmit function");
			goto rmnet_mhi_xmit_error_cleanup;
		}

		/* Need to ensure that the bounce buffers are within range */
		if (DMA_RANGE_CHECK(dma_addr, skb->len, MHI_DMA_MASK)) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"Bounce buffer is out of MHI address range");
			goto rmnet_mhi_xmit_error_cleanup;
		}

		((struct tx_buffer_priv *)(skb->cb))->dma_addr = dma_addr;
	} else {
		/* DMA mapping is OK, need to update the cb field properly */
		struct tx_buffer_priv *tx_priv =
			(struct tx_buffer_priv *)(skb->cb);
		tx_priv->dma_addr = dma_addr;
		tx_priv->is_bounce_buffer = false;
	}

	res = mhi_queue_xfer(rmnet_mhi_ptr->tx_client_handle,
				     (uintptr_t)(dma_addr), skb->len, 0, 0);

	if (MHI_STATUS_RING_FULL == res) {
		/* Need to stop writing until we can write again */
		tx_ring_full_count[rmnet_mhi_ptr->dev_index]++;
		netif_stop_queue(dev);
			goto rmnet_mhi_xmit_error_cleanup;
	}

	if (MHI_STATUS_SUCCESS != res) {
		/* A more fatal error? */
		/* TODO: Is this what we want to do in case of an error here? */
		netif_stop_queue(dev);
		mhi_rmnet_log(RMNET_DBG_ERROR,"%s: mhi_queue_xfer failed, error %d", __func__, res);
		goto rmnet_mhi_xmit_error_cleanup;
	}

	skb_queue_tail(&(rmnet_mhi_ptr->tx_buffers), skb);

	dev->trans_start = jiffies;

	if (bounce_buffer_used) {
		kfree_skb(old_skb);
		tx_bounce_buffers_count[rmnet_mhi_ptr->dev_index]++;
	}
	tx_queued_packets_count[rmnet_mhi_ptr->dev_index]++;
	return 0;

rmnet_mhi_xmit_error_cleanup:
	if (bounce_buffer_used) {
		skb_trim(skb, 0);
		/* Replenish the SKB in case of an error */
		skb_queue_head(&(rmnet_mhi_ptr->tx_bounce_buffers), skb);
	}
	else
		dma_unmap_single(&(dev->dev), dma_addr, skb->len,
				 DMA_TO_DEVICE);
	return NETDEV_TX_BUSY;

}

static int rmnet_mhi_ioctl_extended(struct net_device *dev, struct ifreq *ifr)
{
	struct rmnet_ioctl_extended_s ext_cmd;
	int rc = 0;
	struct rmnet_mhi_private *rmnet_mhi_ptr = netdev_priv(dev);


	rc = copy_from_user(&ext_cmd, ifr->ifr_ifru.ifru_data,
			    sizeof(struct rmnet_ioctl_extended_s));

	if (rc) {
		mhi_rmnet_log(RMNET_DBG_ERROR,"%s: copy_from_user failed ,error %d", __func__, rc);
		return rc;
	}

	switch (ext_cmd.extended_ioctl) {
	case RMNET_IOCTL_SET_MRU:
		if ((0 > ext_cmd.u.data) || (ext_cmd.u.data > MHI_MAX_MRU)) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Can't set MRU, value %u is invalid",
			       __func__, ext_cmd.u.data);
			return -EINVAL;
		}
		rmnet_mhi_ptr->mru = ext_cmd.u.data;
		break;
	case RMNET_IOCTL_GET_EPID:
		/* TODO: TX or RX handle? */
		ext_cmd.u.data =
			mhi_get_epid(rmnet_mhi_ptr->tx_client_handle);
		break;
	case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
		ext_cmd.u.data = 0;
		break;
	case RMNET_IOCTL_GET_DRIVER_NAME:
		strlcpy(ext_cmd.u.if_name, RMNET_MHI_DRIVER_NAME,
			sizeof(ext_cmd.u.if_name));
		break;
	case RMNET_IOCTL_SET_SLEEP_STATE:
		mhi_set_lpm(rmnet_mhi_ptr->tx_client_handle, ext_cmd.u.data);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	rc = copy_to_user(ifr->ifr_ifru.ifru_data, &ext_cmd,
			  sizeof(struct rmnet_ioctl_extended_s));

	if (rc)
		mhi_rmnet_log(RMNET_DBG_ERROR,"%s: copy_to_user failed, error %d", __func__, rc);

	return rc;
}

/* TODO: Which IOCTL do we actually need here? */
static int rmnet_mhi_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int rc = 0;
	struct rmnet_ioctl_data_s ioctl_data;

	switch (cmd) {
	case RMNET_IOCTL_SET_LLP_IP:        /* Set RAWIP protocol */
		break;
	case RMNET_IOCTL_GET_LLP:           /* Get link protocol state */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
		    sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_GET_OPMODE:        /* Get operation mode      */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
		    sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_SET_QOS_ENABLE:
		rc = -EINVAL;
		break;
	case RMNET_IOCTL_SET_QOS_DISABLE:
		rc = 0;
		break;
	case RMNET_IOCTL_OPEN:
	case RMNET_IOCTL_CLOSE:
		/* We just ignore them and return success */
		rc = 0;
		break;
	case RMNET_IOCTL_EXTENDED:
		rc = rmnet_mhi_ioctl_extended(dev, ifr);
		break;
	default:
		/* Don't fail any IOCTL right now */
		rc = 0;
		break;
	}

	return rc;
}

static const struct net_device_ops rmnet_mhi_ops_ip = {
	.ndo_open = rmnet_mhi_open,
	.ndo_stop = rmnet_mhi_stop,
	.ndo_start_xmit = rmnet_mhi_xmit,
	.ndo_do_ioctl = rmnet_mhi_ioctl,
	.ndo_change_mtu = rmnet_mhi_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

static void rmnet_mhi_setup(struct net_device *dev)
{
	dev->netdev_ops = &rmnet_mhi_ops_ip;
	ether_setup(dev);

	/* set this after calling ether_setup */
	dev->header_ops = 0;  /* No header */
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->mtu = MHI_DEFAULT_MTU;
	dev->addr_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
	dev->watchdog_timeo = WATCHDOG_TIMEOUT;
}

int rmnet_mhi_probe(struct platform_device *dev)
{
	int ret = 0, index = 0, cleanup_index = 0;
	struct rmnet_mhi_private *rmnet_mhi_ptr = 0;

	memset(tx_interrupts_count, 0, sizeof(tx_interrupts_count));
	memset(rx_interrupts_count, 0, sizeof(rx_interrupts_count));
	memset(rx_interrupts_in_masked_irq, 0,
	       sizeof(rx_interrupts_in_masked_irq));
	memset(rx_napi_skb_burst_min, 0, sizeof(rx_napi_skb_burst_min));
	memset(rx_napi_skb_burst_max, 0, sizeof(rx_napi_skb_burst_max));
	memset(tx_cb_skb_free_burst_min, 0, sizeof(tx_cb_skb_free_burst_min));
	memset(tx_cb_skb_free_burst_max, 0, sizeof(tx_cb_skb_free_burst_max));
	memset(tx_ring_full_count, 0, sizeof(tx_ring_full_count));
	memset(tx_bounce_buffers_count, 0, sizeof(tx_bounce_buffers_count));
	memset(tx_queued_packets_count, 0, sizeof(tx_queued_packets_count));
	memset(rx_napi_budget_overflow, 0, sizeof(rx_napi_budget_overflow));


	mhi_rmnet_ipc_log = ipc_log_context_create(MHI_IPC_LOG_PAGES, "mhi-rmnet");
	if (!mhi_rmnet_ipc_log) {
		mhi_rmnet_log(RMNET_DBG_ERROR,
				"Failed to create IPC logging context\n");
	}

	for (index = 0; index < MHI_RMNET_DEVICE_COUNT; index++) {
		mhi_rmnet_devices[index] =
			alloc_netdev(sizeof(struct rmnet_mhi_private),
				     RMNET_MHI_DEV_NAME, rmnet_mhi_setup);
		if (!mhi_rmnet_devices[index]) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Network device allocation failed",
			       __func__);
			ret = -ENOMEM;
			goto fail;
		}

		rmnet_mhi_ptr = netdev_priv(mhi_rmnet_devices[index]);

		(mhi_rmnet_devices[index]->dev).dma_mask = kmalloc(sizeof(*(mhi_rmnet_devices[index]->dev).dma_mask), GFP_KERNEL);
		if(!(mhi_rmnet_devices[index]->dev).dma_mask)
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: dma_mask alloc failed",__func__);

		ret = dma_set_mask(&(mhi_rmnet_devices[index]->dev), MHI_DMA_MASK);
		if (0 != ret) {
			/* Not supported for now */
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: dma_set_mask has failed, error %d",
				__func__, ret);
			rmnet_mhi_ptr->allocation_flags = GFP_DMA;
		} else {
			/* We can use the DMA flag! */
			rmnet_mhi_ptr->allocation_flags = GFP_DMA;
		}

		rmnet_mhi_ptr->tx_channel = MHI_CLIENT_IP_HW_0_OUT +
				(MHI_CLIENT_CHANNEL)(index * 2);
		rmnet_mhi_ptr->rx_channel = MHI_CLIENT_IP_HW_0_IN +
				(MHI_CLIENT_CHANNEL)((index * 2));
		rmnet_mhi_ptr->tx_client_handle = 0;
		rmnet_mhi_ptr->rx_client_handle = 0;
		rmnet_mhi_ptr->mru = MHI_DEFAULT_MRU;
		rmnet_mhi_ptr->dev_index = index;

		netif_napi_add(mhi_rmnet_devices[index], &(rmnet_mhi_ptr->napi),
			       rmnet_mhi_poll, MHI_NAPI_WEIGHT_VALUE);

		ret = register_netdev(mhi_rmnet_devices[index]);
		if (ret) {
			mhi_rmnet_log(RMNET_DBG_ERROR,"%s: Network device registration failed",
			       __func__);
			goto fail;
		}

		rx_napi_skb_burst_min[index] = UINT_MAX;
		tx_cb_skb_free_burst_min[index] = UINT_MAX;
	}

	mhi_rmnet_log(RMNET_DBG_ERROR,"MHI RMNET Probe done");
	return 0;

fail:
	for (cleanup_index = 0; cleanup_index <= index; cleanup_index++) {
		if (0 != mhi_rmnet_devices[cleanup_index]) {
			netif_napi_del(&(rmnet_mhi_ptr->napi));
			unregister_netdev(mhi_rmnet_devices[cleanup_index]);
			free_netdev(mhi_rmnet_devices[cleanup_index]);
			mhi_rmnet_devices[cleanup_index] = 0;
		}
	}

	return ret;
}

static int rmnet_mhi_remove(struct platform_device *dev)
{
	platform_driver_unregister(&mhi_rmnet_driver);
	return 0;
}

int rmnet_mhi_init(void)
{
	return platform_driver_register(&mhi_rmnet_driver);
}
