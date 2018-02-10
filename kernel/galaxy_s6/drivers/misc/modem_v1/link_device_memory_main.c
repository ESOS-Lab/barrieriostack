/**
@file		link_device_memory_main.c
@brief		common functions for all types of memory interface media
@date		2014/02/05
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

#define CREATE_TRACE_POINTS

#include <linux/module.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"
#include "link_ctrlmsg_iosm.h"

static unsigned long tx_timer_ns = 1000000;
module_param(tx_timer_ns, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_timer_ns, "modem_v1 tx_timer period time");

#ifdef GROUP_MEM_LINK_DEVICE
/**
@weakgroup group_mem_link_device
@{
*/

/**
@brief		common interrupt handler for all MEMORY interfaces

@param mld	the pointer to a mem_link_device instance
@param msb	the pointer to a mst_buff instance
*/
void mem_irq_handler(struct mem_link_device *mld, struct mst_buff *msb)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	u16 intr = msb->snapshot.int2ap;

	if (unlikely(!int_valid(intr))) {
		mif_err("%s: ERR! invalid intr 0x%X\n", ld->name, intr);
		msb_free(msb);
		return;
	}

	if (unlikely(!rx_possible(mc))) {
		mif_err("%s: ERR! %s.state == %s\n", ld->name, mc->name,
			mc_state(mc));
		msb_free(msb);
		return;
	}

	msb_queue_tail(&mld->msb_rxq, msb);

	tasklet_schedule(&mld->rx_tsk);
}

/**
@brief		check whether or not IPC link is active

@param mld	the pointer to a mem_link_device instance

@retval "TRUE"	if IPC via the mem_link_device instance is possible.
@retval "FALSE"	otherwise.
*/
static inline bool ipc_active(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (unlikely(!cp_online(mc))) {
		mif_err("%s<->%s: %s.state %s != ONLINE <%pf>\n",
			ld->name, mc->name, mc->name, mc_state(mc), CALLER);
		return false;
	}

	if (atomic_read(&mc->forced_cp_crash)) {
		mif_err("%s<->%s: ERR! forced_cp_crash:%d <%pf>\n",
			ld->name, mc->name, atomic_read(&mc->forced_cp_crash),
			CALLER);
		return false;
	}

	if (mld->dpram_magic) {
		unsigned int magic = get_magic(mld);
		unsigned int access = get_access(mld);
		if (magic != MEM_IPC_MAGIC || access != 1) {
			mif_err("%s<->%s: ERR! magic:0x%X access:%d <%pf>\n",
				ld->name, mc->name, magic, access, CALLER);
			return false;
		}
	}

	return true;
}

static inline void stop_tx(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	stop_net_ifaces(ld);
}

static inline void purge_txq(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	int i;

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
	/* Purge the skb_q in every TX RB */
	if (ld->sbd_ipc) {
		struct sbd_link_device *sl = &mld->sbd_link_dev;
		for (i = 0; i < sl->num_channels; i++) {
			struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, TX);
			skb_queue_purge(&rb->skb_q);
		}
	}
#endif

	/* Purge the skb_txq in every IPC device (IPC_FMT, IPC_RAW, etc.) */
	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		skb_queue_purge(dev->skb_txq);
	}
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_IPC_TX

/**
@weakgroup group_mem_ipc_tx

@dot
digraph mem_tx {
graph [
	label="\n<TX Flow of Memory-type Interface>\n\n"
	labelloc="top"
	fontname="Helvetica"
	fontsize=20
];

node [shape=box fontname="Helvetica"];

edge [fontname="Helvetica"];

node []
	mem_send [
		label="mem_send"
		URL="@ref mem_send"
	];

	xmit_udl [
		label="xmit_udl"
		URL="@ref xmit_udl"
	];

	xmit_ipc [
		label="xmit_ipc"
		URL="@ref xmit_ipc"
	];

	skb_txq [
		shape=record
		label="<f0>| |skb_txq| |<f1>"
	];

	start_tx_timer [
		label="start_tx_timer"
		URL="@ref start_tx_timer"
	];

	tx_timer_func [
		label="tx_timer_func"
		URL="@ref tx_timer_func"
	];

	tx_frames_to_dev [
		label="tx_frames_to_dev"
		URL="@ref tx_frames_to_dev"
	];

	txq_write [
		label="txq_write"
		URL="@ref txq_write"
	];

edge [color=blue fontcolor=blue];
	mem_send -> xmit_udl [
		label="1. UDL frame\n(skb)"
	];

	xmit_udl -> skb_txq:f1 [
		label="2. skb_queue_tail\n(UDL frame skb)"
		arrowhead=vee
		style=dashed
	];

	xmit_udl -> start_tx_timer [
		label="3. Scheduling"
	];

edge [color=brown fontcolor=brown];
	mem_send -> xmit_ipc [
		label="1. IPC frame\n(skb)"
	];

	xmit_ipc -> skb_txq:f1 [
		label="2. skb_queue_tail\n(IPC frame skb)"
		arrowhead=vee
		style=dashed
	];

	xmit_ipc -> start_tx_timer [
		label="3. Scheduling"
	];

edge [color=black fontcolor=black];
	start_tx_timer -> tx_timer_func [
		label="4. HR timer"
		arrowhead=vee
		style=dotted
	];

	tx_timer_func -> tx_frames_to_dev [
		label="for (every IPC device)"
	];

	skb_txq:f0 -> tx_frames_to_dev [
		label="5. UDL/IPC frame\n(skb)"
		arrowhead=vee
		style=dashed
	];

	tx_frames_to_dev -> txq_write [
		label="6. UDL/IPC frame\n(skb)"
	];
}
@enddot
*/

/**
@weakgroup group_mem_ipc_tx
@{
*/

/**
@brief		check the free space in a circular TXQ

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance
@param qsize	the size of the buffer in @b @@dev TXQ
@param in	the IN (HEAD) pointer value of the TXQ
@param out	the OUT (TAIL) pointer value of the TXQ
@param count	the size of the data to be transmitted

@retval "> 0"	the size of free space in the @b @@dev TXQ
@retval "< 0"	an error code
*/
static inline int check_txq_space(struct mem_link_device *mld,
				  struct mem_ipc_device *dev,
				  unsigned int qsize, unsigned int in,
				  unsigned int out, unsigned int count)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned int usage;
	unsigned int space;

	if (!circ_valid(qsize, in, out)) {
		mif_err("%s: ERR! Invalid %s_TXQ{qsize:%d in:%d out:%d}\n",
			ld->name, dev->name, qsize, in, out);
		return -EIO;
	}

	usage = circ_get_usage(qsize, in, out);
	if (unlikely(usage > SHM_UL_USAGE_LIMIT) && cp_online(mc)) {
		mif_debug("%s: CAUTION! BUSY in %s_TXQ{qsize:%d in:%d out:%d "
			"usage:%d (count:%d)}\n", ld->name, dev->name, qsize,
			in, out, usage, count);
		return -EBUSY;
	}

	space = circ_get_space(qsize, in, out);
	if (unlikely(space < count)) {
		if (cp_online(mc)) {
			mif_err("%s: CAUTION! NOSPC in %s_TXQ{qsize:%d in:%d "
				"out:%d space:%d count:%d}\n", ld->name,
				dev->name, qsize, in, out, space, count);
		}
		return -ENOSPC;
	}

	return space;
}

/**
@brief		copy data in an skb to a circular TXQ

Enqueues a frame in @b @@skb to the @b @@dev TXQ if there is enough space in the
TXQ, then releases @b @@skb.

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance
@param skb	the pointer to an sk_buff instance

@retval "> 0"	the size of the frame written in the TXQ
@retval "< 0"	an error code (-EBUSY, -ENOSPC, or -EIO)
*/
static int txq_write(struct mem_link_device *mld, struct mem_ipc_device *dev,
		     struct sk_buff *skb)
{
	char *src = skb->data;
	unsigned int count = skb->len;
	char *dst = get_txq_buff(dev);
	unsigned int qsize = get_txq_buff_size(dev);
	unsigned int in = get_txq_head(dev);
	unsigned int out = get_txq_tail(dev);
	int space;

	space = check_txq_space(mld, dev, qsize, in, out, count);
	if (unlikely(space < 0))
		return space;

	barrier();

	circ_write(dst, src, qsize, in, count);

	barrier();

	set_txq_head(dev, circ_new_ptr(qsize, in, count));

	/* Commit the item before incrementing the head */
	smp_mb();

	return count;
}

/**
@brief		try to transmit all IPC frames in an skb_txq to CP

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance

@retval "> 0"	accumulated data size written to a circular TXQ
@retval "< 0"	an error code
*/
static int tx_frames_to_dev(struct mem_link_device *mld,
			    struct mem_ipc_device *dev)
{
	struct sk_buff_head *skb_txq = dev->skb_txq;
	int tx_bytes = 0;
	int ret = 0;

	while (1) {
		struct sk_buff *skb;

		skb = skb_dequeue(skb_txq);
		if (unlikely(!skb))
			break;

		ret = txq_write(mld, dev, skb);
		if (unlikely(ret < 0)) {
			/* Take the skb back to the skb_txq */
			skb_queue_head(skb_txq, skb);
			break;
		}

		tx_bytes += ret;
		dev_kfree_skb_any(skb);
	}

	return (ret < 0) ? ret : tx_bytes;
}

static enum hrtimer_restart tx_timer_func(struct hrtimer *timer)
{
	struct mem_link_device *mld;
	struct link_device *ld;
	struct modem_ctl *mc;
	int i;
	bool need_schedule;
	u16 mask;
	unsigned long flags;

	mld = container_of(timer, struct mem_link_device, tx_timer);
	ld = &mld->link_dev;
	mc = ld->mc;

	need_schedule = false;
	mask = 0;

	spin_lock_irqsave(&mc->lock, flags);

	if (unlikely(!ipc_active(mld)))
		goto exit;

	if (mld->link_active) {
		if (!mld->link_active(mld)) {
			need_schedule = true;
			goto exit;
		}
	}

	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		int ret;

		if (unlikely(under_tx_flow_ctrl(mld, dev))) {
			ret = check_tx_flow_ctrl(mld, dev);
			if (ret < 0) {
				if (ret == -EBUSY || ret == -ETIME) {
					need_schedule = true;
					continue;
				} else {
					modemctl_notify_event(MDM_EVENT_CP_FORCE_CRASH);
					need_schedule = false;
					goto exit;
				}
			}
		}

		ret = tx_frames_to_dev(mld, dev);
		if (unlikely(ret < 0)) {
			if (ret == -EBUSY || ret == -ENOSPC) {
				need_schedule = true;
				start_tx_flow_ctrl(mld, dev);
				continue;
			} else {
				modemctl_notify_event(MDM_EVENT_CP_FORCE_CRASH);
				need_schedule = false;
				goto exit;
			}
		}

		if (ret > 0)
			mask |= msg_mask(dev);

		if (!skb_queue_empty(dev->skb_txq))
			need_schedule = true;
	}

	if (!need_schedule) {
		for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
			if (!txq_empty(mld->dev[i])) {
				need_schedule = true;
				break;
			}
		}
	}

	if (mask)
		send_ipc_irq(mld, mask2int(mask));

exit:
	if (need_schedule) {
		ktime_t ktime = ktime_set(0, ms2ns(TX_PERIOD_MS));
		hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
	}

	spin_unlock_irqrestore(&mc->lock, flags);

	return HRTIMER_NORESTART;
}

static inline void start_tx_timer(struct mem_link_device *mld,
				  struct hrtimer *timer)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;

	spin_lock_irqsave(&mc->lock, flags);

	if (unlikely(cp_offline(mc)))
		goto exit;

	if (!hrtimer_is_queued(timer)) {
		ktime_t ktime = ktime_set(0, tx_timer_ns);
		hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
	}

exit:
	spin_unlock_irqrestore(&mc->lock, flags);
}

static inline void cancel_tx_timer(struct mem_link_device *mld,
				   struct hrtimer *timer)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;

	spin_lock_irqsave(&mc->lock, flags);

	if (hrtimer_active(timer))
		hrtimer_cancel(timer);

	spin_unlock_irqrestore(&mc->lock, flags);
}

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
static int tx_frames_to_rb(struct sbd_ring_buffer *rb)
{
	struct sk_buff_head *skb_txq = &rb->skb_q;
	int tx_bytes = 0;
	int ret = 0;

	while (1) {
		struct sk_buff *skb;

		skb = skb_dequeue(skb_txq);
		if (unlikely(!skb))
			break;

		ret = sbd_pio_tx(rb, skb);
		if (unlikely(ret < 0)) {
			/* Take the skb back to the skb_txq */
			skb_queue_head(skb_txq, skb);
			break;
		}

		tx_bytes += ret;

		log_ipc_pkt(LNK_TX, rb->ch, skb);

		trace_mif_event(skb, skb->len, FUNC);

		dev_kfree_skb_any(skb);
	}

	return (ret < 0) ? ret : tx_bytes;
}

static enum hrtimer_restart sbd_tx_timer_func(struct hrtimer *timer)
{
	struct mem_link_device *mld;
	struct link_device *ld;
	struct modem_ctl *mc;
	struct sbd_link_device *sl;
	int i;
	bool need_schedule;
	u16 mask;
	unsigned long flags = 0;

	mld = container_of(timer, struct mem_link_device, sbd_tx_timer);
	ld = &mld->link_dev;
	mc = ld->mc;
	sl = &mld->sbd_link_dev;

	need_schedule = false;
	mask = 0;

	spin_lock_irqsave(&mc->lock, flags);
	if (unlikely(!ipc_active(mld))) {
		spin_unlock_irqrestore(&mc->lock, flags);
		goto exit;
	}
	spin_unlock_irqrestore(&mc->lock, flags);

	if (mld->link_active) {
		if (!mld->link_active(mld)) {
			need_schedule = true;
			goto exit;
		}
	}

	for (i = 0; i < sl->num_channels; i++) {
		struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, TX);
		int ret;

		ret = tx_frames_to_rb(rb);

		if (unlikely(ret < 0)) {
			if (ret == -EBUSY || ret == -ENOSPC) {
				need_schedule = true;
				mask = MASK_SEND_DATA;
				continue;
			} else {
				modemctl_notify_event(MDM_CRASH_INVALID_RB);
				need_schedule = false;
				goto exit;
			}
		}

		if (ret > 0)
			mask = MASK_SEND_DATA;

		if (!skb_queue_empty(&rb->skb_q))
			need_schedule = true;
	}

	if (!need_schedule) {
		for (i = 0; i < sl->num_channels; i++) {
			struct sbd_ring_buffer *rb;

			rb = sbd_id2rb(sl, i, TX);
			if (!rb_empty(rb)) {
				need_schedule = true;
				break;
			}
		}
	}

	if (mask) {
		spin_lock_irqsave(&mc->lock, flags);
		if (unlikely(!ipc_active(mld))) {
			spin_unlock_irqrestore(&mc->lock, flags);
			need_schedule = false;
			goto exit;
		}
		send_ipc_irq(mld, mask2int(mask));
		spin_unlock_irqrestore(&mc->lock, flags);
	}

exit:
	if (need_schedule) {
		ktime_t ktime = ktime_set(0, ms2ns(TX_PERIOD_MS));
		hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
	}

	return HRTIMER_NORESTART;
}

/**
@brief		transmit an IPC message packet

@param mld	the pointer to a mem_link_device instance
@param ch	the channel ID
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb
@retval "< 0"	an error code (-ENODEV, -EBUSY)
*/
static int xmit_ipc_to_rb(struct mem_link_device *mld, enum sipc_ch_id ch,
			  struct sk_buff *skb)
{
	int ret;
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod = skbpriv(skb)->iod;
	struct modem_ctl *mc = ld->mc;
	struct sbd_ring_buffer *rb = sbd_ch2rb(&mld->sbd_link_dev, ch, TX);
	struct sk_buff_head *skb_txq;
	unsigned long flags;

	if (!rb) {
		mif_err("%s: %s->%s: ERR! NO SBD RB {ch:%d}\n",
			ld->name, iod->name, mc->name, ch);
		return -ENODEV;
	}

	skb_txq = &rb->skb_q;

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->forbid_cp_sleep)
		mld->forbid_cp_sleep(mld);
#endif

	spin_lock_irqsave(&rb->lock, flags);

	if (unlikely(skb_txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		mif_err_limited("%s: %s->%s: ERR! {ch:%d} "
				"skb_txq.len %d >= limit %d\n",
				ld->name, iod->name, mc->name, ch,
				skb_txq->qlen, MAX_SKB_TXQ_DEPTH);
		ret = -EBUSY;
	} else {
		skb->len = min_t(int, skb->len, rb->buff_size);

		ret = skb->len;
		skb_queue_tail(skb_txq, skb);
		start_tx_timer(mld, &mld->sbd_tx_timer);

		trace_mif_event(skb, skb->len, FUNC);
	}

	spin_unlock_irqrestore(&rb->lock, flags);

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->permit_cp_sleep)
		mld->permit_cp_sleep(mld);
#endif

	return ret;
}
#endif

/**
@brief		transmit an IPC message packet

@param mld	the pointer to a mem_link_device instance
@param ch	the channel ID
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb
@retval "< 0"	an error code (-ENODEV or -EBUSY)
*/
static int xmit_ipc_to_dev(struct mem_link_device *mld, enum sipc_ch_id ch,
			   struct sk_buff *skb)
{
	int ret;
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod = skbpriv(skb)->iod;
	struct modem_ctl *mc = ld->mc;
	struct mem_ipc_device *dev = mld->dev[dev_id(ch)];
	struct sk_buff_head *skb_txq;
	unsigned long flags;

	if (!dev) {
		mif_err("%s: %s->%s: ERR! NO IPC DEV {ch:%d}\n",
			ld->name, iod->name, mc->name, ch);
		return -ENODEV;
	}

	skb_txq = dev->skb_txq;

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->forbid_cp_sleep)
		mld->forbid_cp_sleep(mld);
#endif

	spin_lock_irqsave(dev->tx_lock, flags);

	if (unlikely(skb_txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		mif_err_limited("%s: %s->%s: ERR! %s TXQ.qlen %d >= limit %d\n",
				ld->name, iod->name, mc->name, dev->name,
				skb_txq->qlen, MAX_SKB_TXQ_DEPTH);
		ret = -EBUSY;
	} else {
		ret = skb->len;
		skb_queue_tail(dev->skb_txq, skb);
		start_tx_timer(mld, &mld->tx_timer);
	}

	spin_unlock_irqrestore(dev->tx_lock, flags);

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->permit_cp_sleep)
		mld->permit_cp_sleep(mld);
#endif

	return ret;
}

/**
@brief		transmit an IPC message packet

@param mld	the pointer to a mem_link_device instance
@param ch	the channel ID
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb
@retval "< 0"	an error code (-EIO, -ENODEV, -EBUSY)
*/
static int xmit_ipc(struct mem_link_device *mld, struct io_device *iod,
		    enum sipc_ch_id ch, struct sk_buff *skb)
{
	if (unlikely(!ipc_active(mld)))
		return -EIO;

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
	if (iod->sbd_ipc) {
		if (likely(sbd_active(&mld->sbd_link_dev)))
			return xmit_ipc_to_rb(mld, ch, skb);
		else
			return -ENODEV;
	} else {
		return xmit_ipc_to_dev(mld, ch, skb);
	}
#else
	return xmit_ipc_to_dev(mld, ch, skb);
#endif
}

static inline int check_udl_space(struct mem_link_device *mld,
				  struct mem_ipc_device *dev,
				  unsigned int qsize, unsigned int in,
				  unsigned int out, unsigned int count)
{
	struct link_device *ld = &mld->link_dev;
	unsigned int space;

	if (!circ_valid(qsize, in, out)) {
		mif_err("%s: ERR! Invalid %s_TXQ{qsize:%d in:%d out:%d}\n",
			ld->name, dev->name, qsize, in, out);
		return -EIO;
	}

	space = circ_get_space(qsize, in, out);
	if (unlikely(space < count)) {
		mif_err("%s: NOSPC in %s_TXQ{qsize:%d in:%d "
			"out:%d space:%d count:%d}\n", ld->name,
			dev->name, qsize, in, out, space, count);
		return -ENOSPC;
	}

	return 0;
}

/**
@brief		copy UDL data in an skb to a circular TXQ

Enqueues a frame in @b @@skb to the @b @@dev TXQ if there is enough space in the
TXQ, then releases @b @@skb.

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance
@param skb	the pointer to an sk_buff instance

@retval "> 0"	the size of the frame written in the TXQ
@retval "< 0"	an error code (-EBUSY, -ENOSPC, or -EIO)
*/
static inline int udl_write(struct mem_link_device *mld,
			    struct mem_ipc_device *dev, struct sk_buff *skb)
{
	unsigned int count = skb->len;
	char *src = skb->data;
	char *dst = get_txq_buff(dev);
	unsigned int qsize = get_txq_buff_size(dev);
	unsigned int in = get_txq_head(dev);
	unsigned int out = get_txq_tail(dev);
	int space;

	space = check_udl_space(mld, dev, qsize, in, out, count);
	if (unlikely(space < 0))
		return space;

	barrier();

	circ_write(dst, src, qsize, in, count);

	barrier();

	set_txq_head(dev, circ_new_ptr(qsize, in, count));

	/* Commit the item before incrementing the head */
	smp_mb();

	return count;
}

/**
@brief		transmit a BOOT/DUMP data packet

@param mld	the pointer to a mem_link_device instance
@param ch	the channel ID
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb
*/
static int xmit_udl(struct mem_link_device *mld, struct io_device *iod,
		    enum sipc_ch_id ch, struct sk_buff *skb)
{
	int ret;
	struct mem_ipc_device *dev = mld->dev[IPC_RAW];
	int count = skb->len;
	int tried = 0;

	while (1) {
		ret = udl_write(mld, dev, skb);
		if (ret == count)
			break;

		if (ret != -ENOSPC)
			goto exit;

		tried++;
		if (tried >= 20)
			goto exit;

		if (in_interrupt())
			mdelay(50);
		else
			msleep(50);
	}

	dev_kfree_skb_any(skb);

exit:
	return ret;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_IPC_RX

/**
@weakgroup group_mem_ipc_rx

@dot
digraph mem_rx {
graph [
	label="\n<RX Flow of Memory-type Interface>\n\n"
	labelloc="top"
	fontname="Helvetica"
	fontsize=20
];

node [shape=box fontname="Helvetica"];

edge [fontname="Helvetica"];

node []
	subgraph irq_handling {
	node []
		mem_irq_handler [
			label="mem_irq_handler"
			URL="@ref mem_irq_handler"
		];

		mem_rx_task [
			label="mem_rx_task"
			URL="@ref mem_rx_task"
		];

		udl_rx_work [
			label="udl_rx_work"
			URL="@ref udl_rx_work"
		];

		ipc_rx_func [
			label="ipc_rx_func"
			URL="@ref ipc_rx_func"
		];

	edge [color="red:green4:blue" fontcolor=black];
		mem_irq_handler -> mem_rx_task [
			label="Tasklet"
		];

	edge [color=blue fontcolor=blue];
		mem_rx_task -> udl_rx_work [
			label="Delayed\nWork"
			arrowhead=vee
			style=dotted
		];

		udl_rx_work -> ipc_rx_func [
			label="Process\nContext"
		];

	edge [color="red:green4" fontcolor=brown];
		mem_rx_task -> ipc_rx_func [
			label="Tasklet\nContext"
		];
	}

	subgraph rx_processing {
	node []
		mem_cmd_handler [
			label="mem_cmd_handler"
			URL="@ref mem_cmd_handler"
		];

		recv_ipc_frames [
			label="recv_ipc_frames"
			URL="@ref recv_ipc_frames"
		];

		rx_frames_from_dev [
			label="rx_frames_from_dev"
			URL="@ref rx_frames_from_dev"
		];

		rxq_read [
			label="rxq_read"
			URL="@ref rxq_read"
		];

		schedule_link_to_demux [
			label="schedule_link_to_demux"
			URL="@ref schedule_link_to_demux"
		];

		link_to_demux_work [
			label="link_to_demux_work"
			URL="@ref link_to_demux_work"
		];

		pass_skb_to_demux [
			label="pass_skb_to_demux"
			URL="@ref pass_skb_to_demux"
		];

		skb_rxq [
			shape=record
			label="<f0>| |skb_rxq| |<f1>"
			color=green4
			fontcolor=green4
		];

	edge [color=black fontcolor=black];
		ipc_rx_func -> mem_cmd_handler [
			label="command"
		];

	edge [color="red:green4:blue" fontcolor=black];
		ipc_rx_func -> recv_ipc_frames [
			label="while (msb)"
		];

		recv_ipc_frames -> rx_frames_from_dev [
			label="1. for (each IPC device)"
		];

		rx_frames_from_dev -> rxq_read [
			label="2.\nwhile (rcvd < size)"
		];

	edge [color=blue fontcolor=blue];
		rxq_read -> rx_frames_from_dev [
			label="2-1.\nUDL skb\n[process]"
		];

		rx_frames_from_dev -> pass_skb_to_demux [
			label="2-2.\nUDL skb\n[process]"
		];

	edge [color="red:green4" fontcolor=brown];
		rxq_read -> rx_frames_from_dev [
			label="2-1.\nFMT skb\nRFS skb\nPS skb\n[tasklet]"
		];

	edge [color=red fontcolor=red];
		rx_frames_from_dev -> pass_skb_to_demux [
			label="2-2.\nFMT skb\nRFS skb\n[tasklet]"
		];

	edge [color=green4 fontcolor=green4];
		rx_frames_from_dev -> skb_rxq:f1 [
			label="2-2.\nPS skb\n[tasklet]"
			arrowhead=vee
			style=dashed
		];

	edge [color="red:green4" fontcolor=brown];
		rx_frames_from_dev -> recv_ipc_frames [
			label="3. Return"
		];

	edge [color=green4 fontcolor=green4];
		recv_ipc_frames -> schedule_link_to_demux [
			label="4. Scheduling"
		];

	edge [color="red:green4" fontcolor=brown];
		recv_ipc_frames -> ipc_rx_func [
			label="5. Return"
		];

	edge [color=green4 fontcolor=green4];
		schedule_link_to_demux -> link_to_demux_work [
			label="6. Delayed\nwork\n[process]"
			arrowhead=vee
			style=dotted
		];

		skb_rxq:f0 -> link_to_demux_work [
			label="6-1.\nPS skb"
			arrowhead=vee
			style=dashed
		];

		link_to_demux_work -> pass_skb_to_demux [
			label="6-2.\nPS skb"
		];
	}
}
@enddot
*/

/**
@weakgroup group_mem_ipc_rx
@{
*/

/**
@brief		pass a socket buffer to the DEMUX layer

Invokes the recv_skb_single method in the io_device instance to perform
receiving IPC messages from each skb.

@param mld	the pointer to a mem_link_device instance
@param skb	the pointer to an sk_buff instance

@retval "> 0"	if succeeded to pass an @b @@skb to the DEMUX layer
@retval "< 0"	an error code
*/
static void pass_skb_to_demux(struct mem_link_device *mld, struct sk_buff *skb)
{
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod = skbpriv(skb)->iod;
	int ret;
	u8 ch = skbpriv(skb)->sipc_ch;

	if (unlikely(!iod)) {
		mif_err("%s: ERR! No IOD for CH.%d\n", ld->name, ch);
		dev_kfree_skb_any(skb);
		modemctl_notify_event(MDM_CRASH_INVALID_IOD);
		return;
	}

	log_ipc_pkt(LNK_RX, ch, skb);

	ret = iod->recv_skb_single(iod, ld, skb);
	if (unlikely(ret < 0)) {
		struct modem_ctl *mc = ld->mc;
		mif_err_limited("%s: %s<-%s: ERR! %s->recv_skb fail (%d)\n",
				ld->name, iod->name, mc->name, iod->name, ret);
		dev_kfree_skb_any(skb);
	}
}

static inline void link_to_demux(struct mem_link_device  *mld)
{
	int i;

	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		struct sk_buff_head *skb_rxq = dev->skb_rxq;

		while (1) {
			struct sk_buff *skb;

			skb = skb_dequeue(skb_rxq);
			if (!skb)
				break;

			pass_skb_to_demux(mld, skb);
		}
	}
}

/**
@brief		pass socket buffers in every skb_rxq to the DEMUX layer

@param ws	the pointer to a work_struct instance

@see		schedule_link_to_demux()
@see		rx_frames_from_dev()
@see		mem_create_link_device()
*/
static void link_to_demux_work(struct work_struct *ws)
{
	struct link_device *ld;
	struct mem_link_device *mld;

	ld = container_of(ws, struct link_device, rx_delayed_work.work);
	mld = to_mem_link_device(ld);

	link_to_demux(mld);
}

static inline void schedule_link_to_demux(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct delayed_work *dwork = &ld->rx_delayed_work;

	/*queue_delayed_work(ld->rx_wq, dwork, 0);*/
	queue_work_on(7, ld->rx_wq, &dwork->work);
}

/**
@brief		copy each IPC link frame from a circular queue to an skb

1) Analyzes a link frame header and get the size of the current link frame.\n
2) Allocates a socket buffer (skb).\n
3) Extracts a link frame from the current @b $out (tail) pointer in the @b
   @@dev RXQ up to @b @@in (head) pointer in the @b @@dev RXQ, then copies it
   to the skb allocated in the step 2.\n
4) Updates the TAIL (OUT) pointer in the @b @@dev RXQ.\n

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance (IPC_FMT, etc.)
@param in	the IN (HEAD) pointer value of the @b @@dev RXQ

@retval "struct sk_buff *"	if there is NO error
@retval "NULL"		if there is ANY error
*/
static struct sk_buff *rxq_read(struct mem_link_device *mld,
				struct mem_ipc_device *dev,
				unsigned int in)
{
	struct link_device *ld = &mld->link_dev;
	struct sk_buff *skb;
	char *src = get_rxq_buff(dev);
	unsigned int qsize = get_rxq_buff_size(dev);
	unsigned int out = get_rxq_tail(dev);
	unsigned int rest = circ_get_usage(qsize, in, out);
	unsigned int len;
	char hdr[SIPC5_MIN_HEADER_SIZE];

	/* Copy the header in a frame to the header buffer */
	circ_read(hdr, src, qsize, out, SIPC5_MIN_HEADER_SIZE);

	/* Check the config field in the header */
	if (unlikely(!sipc5_start_valid(hdr))) {
		mif_err("%s: ERR! %s BAD CFG 0x%02X (in:%d out:%d rest:%d)\n",
			ld->name, dev->name, hdr[SIPC5_CONFIG_OFFSET],
			in, out, rest);
		goto bad_msg;
	}

	/* Verify the length of the frame (data + padding) */
	len = sipc5_get_total_len(hdr);
	if (unlikely(len > rest)) {
		mif_err("%s: ERR! %s BAD LEN %d > rest %d\n",
			ld->name, dev->name, len, rest);
		goto bad_msg;
	}

	/* Allocate an skb */
	skb = mem_alloc_skb(len);
	if (!skb) {
		mif_err("%s: ERR! %s mem_alloc_skb(%d) fail\n",
			ld->name, dev->name, len);
		goto no_mem;
	}

	/* Read the frame from the RXQ */
	circ_read(skb_put(skb, len), src, qsize, out, len);

	/* Update tail (out) pointer to the frame to be read in the future */
	set_rxq_tail(dev, circ_new_ptr(qsize, out, len));

	/* Finish reading data before incrementing tail */
	smp_mb();

	return skb;

bad_msg:
	mif_err("%s: %s%s%s: ERR! BAD MSG: %02x %02x %02x %02x\n",
		FUNC, ld->name, arrow(RX), ld->mc->name,
		hdr[0], hdr[1], hdr[2], hdr[3]);
	set_rxq_tail(dev, in);	/* Reset tail (out) pointer */
	modemctl_notify_event(MDM_EVENT_CP_FORCE_CRASH);

no_mem:
	return NULL;
}

/**
@brief		extract all IPC link frames from a circular queue

In a while loop,\n
1) Receives each IPC link frame stored in the @b @@dev RXQ.\n
2) If the frame is a PS (network) data frame, stores it to an skb_rxq and
   schedules a delayed work for PS data reception.\n
3) Otherwise, passes it to the DEMUX layer immediately.\n

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance (IPC_FMT, etc.)

@retval "> 0"	if valid data received
@retval "= 0"	if no data received
@retval "< 0"	if ANY error
*/
static int rx_frames_from_dev(struct mem_link_device *mld,
			      struct mem_ipc_device *dev)
{
	struct link_device *ld = &mld->link_dev;
	struct sk_buff_head *skb_rxq = dev->skb_rxq;
	unsigned int qsize = get_rxq_buff_size(dev);
	unsigned int in = get_rxq_head(dev);
	unsigned int out = get_rxq_tail(dev);
	unsigned int size = circ_get_usage(qsize, in, out);
	int rcvd = 0;

	if (unlikely(circ_empty(in, out)))
		return 0;

	while (rcvd < size) {
		struct sk_buff *skb;
		u8 ch;
		struct io_device *iod;

		skb = rxq_read(mld, dev, in);
		if (!skb)
			break;

		ch = sipc5_get_ch(skb->data);
		iod = link_get_iod_with_channel(ld, ch);
		if (!iod) {
			mif_err("%s: ERR! No IOD for CH.%d\n", ld->name, ch);
			dev_kfree_skb_any(skb);
			modemctl_notify_event(MDM_EVENT_CP_FORCE_CRASH);
			break;
		}

		/* Record the IO device and the link device into the &skb->cb */
		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;

		skbpriv(skb)->lnk_hdr = iod->link_header;
		skbpriv(skb)->sipc_ch = ch;

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_demux(). */
		rcvd += skb->len;

		if (likely(sipc_ps_ch(ch)))
			skb_queue_tail(skb_rxq, skb);
		else
			pass_skb_to_demux(mld, skb);
	}

	if (rcvd < size) {
		struct link_device *ld = &mld->link_dev;
		mif_err("%s: WARN! rcvd %d < size %d\n", ld->name, rcvd, size);
	}

	return rcvd;
}

/**
@brief		receive all @b IPC message frames in all RXQs

In a for loop,\n
1) Checks any REQ_ACK received.\n
2) Receives all IPC link frames in every RXQ.\n
3) Sends RES_ACK if there was REQ_ACK from CP.\n
4) Checks any RES_ACK received.\n

@param mld	the pointer to a mem_link_device instance
@param mst	the pointer to a mem_snapshot instance
*/
static void recv_ipc_frames(struct mem_link_device *mld,
			    struct mem_snapshot *mst)
{
	int i;
	u16 intr = mst->int2ap;

	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		int rcvd;

		if (req_ack_valid(dev, intr))
			recv_req_ack(mld, dev, mst);

		rcvd = rx_frames_from_dev(mld, dev);
		if (rcvd < 0)
			break;

		schedule_link_to_demux(mld);

		if (req_ack_valid(dev, intr))
			send_res_ack(mld, dev);

		if (res_ack_valid(dev, intr))
			recv_res_ack(mld, dev, mst);
	}
}

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
static void pass_skb_to_net(struct mem_link_device *mld, struct sk_buff *skb)
{
	struct link_device *ld = &mld->link_dev;
	struct skbuff_private *priv;
	struct io_device *iod;
	int ret;

	priv = skbpriv(skb);
	if (unlikely(!priv)) {
		mif_err("%s: ERR! No PRIV in skb@%p\n", ld->name, skb);
		dev_kfree_skb_any(skb);
		modemctl_notify_event(MDM_CRASH_INVALID_SKBCB);
		return;
	}

	iod = priv->iod;
	if (unlikely(!iod)) {
		mif_err("%s: ERR! No IOD in skb@%p\n", ld->name, skb);
		dev_kfree_skb_any(skb);
		modemctl_notify_event(MDM_CRASH_INVALID_SKBIOD);
		return;
	}

	log_ipc_pkt(LNK_RX, iod->id, skb);

	ret = iod->recv_net_skb(iod, ld, skb);
	if (unlikely(ret < 0)) {
		struct modem_ctl *mc = ld->mc;
		mif_err_limited("%s: %s<-%s: ERR! %s->recv_net_skb fail (%d)\n",
				ld->name, iod->name, mc->name, iod->name, ret);
		dev_kfree_skb_any(skb);
	}
}

/**
@brief		try to extract all PS network frames from an SBD RB

In a while loop,\n
1) Receives each PS (network) data frame stored in the @b @@rb RB.\n
2) Pass the skb to NET_RX.\n

@param rb	the pointer to a mem_ring_buffer instance
@param budget	maximum number of packets to be received at one time

@retval "> 0"	if valid data received
@retval "= 0"	if no data received
@retval "< 0"	if ANY error
*/
static int rx_net_frames_from_rb(struct sbd_ring_buffer *rb, int budget)
{
	int rcvd = 0;
	struct link_device *ld = rb->ld;
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	unsigned int num_frames;

#ifdef CONFIG_LINK_DEVICE_NAPI
	num_frames = min_t(unsigned int, rb_usage(rb), budget);
#else
	num_frames = rb_usage(rb);
#endif

	while (rcvd < num_frames) {
		struct sk_buff *skb;

		skb = sbd_pio_rx(rb);
		if (!skb)
			break;

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_net(). */
		rcvd++;

		pass_skb_to_net(mld, skb);
	}

	if (rcvd < num_frames) {
		struct io_device *iod = rb->iod;
		struct link_device *ld = rb->ld;
		struct modem_ctl *mc = ld->mc;
		mif_err("%s: %s<-%s: WARN! rcvd %d < num_frames %d\n",
			ld->name, iod->name, mc->name, rcvd, num_frames);
	}

	return rcvd;
}

/**
@brief		extract all IPC link frames from an SBD RB

In a while loop,\n
1) receives each IPC link frame stored in the @b @@RB.\n
2) passes it to the DEMUX layer immediately.\n

@param rb	the pointer to a mem_ring_buffer instance

@retval "> 0"	if valid data received
@retval "= 0"	if no data received
@retval "< 0"	if ANY error
*/
static int rx_ipc_frames_from_rb(struct sbd_ring_buffer *rb)
{
	int rcvd = 0;
	struct link_device *ld = rb->ld;
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	unsigned int qlen = rb->len;
	unsigned int in = *rb->wp;
	unsigned int out = *rb->rp;
	unsigned int num_frames = circ_get_usage(qlen, in, out);

	while (rcvd < num_frames) {
		struct sk_buff *skb;

		skb = sbd_pio_rx(rb);
		if (!skb) {
#ifdef CONFIG_SEC_MODEM_DEBUG
			panic("skb alloc failed.");
#else
			modemctl_notify_event(MDM_CRASH_NO_MEM);
#endif
			break;
		}

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_demux(). */
		rcvd++;

		if (skbpriv(skb)->lnk_hdr) {
			u8 ch = rb->ch;
			u8 fch = sipc5_get_ch(skb->data);
			if (fch != ch) {
				mif_err("frm.ch:%d != rb.ch:%d\n", fch, ch);
				dev_kfree_skb_any(skb);

				modemctl_notify_event(MDM_EVENT_CP_ABNORMAL_RX);
				continue;
			}
		}

		pass_skb_to_demux(mld, skb);
	}

	if (rcvd < num_frames) {
		struct io_device *iod = rb->iod;
		struct modem_ctl *mc = ld->mc;
		mif_err("%s: %s<-%s: WARN! rcvd %d < num_frames %d\n",
			ld->name, iod->name, mc->name, rcvd, num_frames);
	}

	return rcvd;
}

int mem_netdev_poll(struct napi_struct *napi, int budget)
{
	int rcvd;
	struct vnet *vnet = netdev_priv(napi->dev);
	struct mem_link_device *mld =
		container_of(vnet->ld, struct mem_link_device, link_dev);
	struct sbd_ring_buffer *rb =
		sbd_ch2rb(&mld->sbd_link_dev, vnet->iod->id, RX);

	rcvd = rx_net_frames_from_rb(rb, budget);

	/* no more ring buffer to process */
	if (rcvd < budget) {
		napi_complete(napi);
		//vnet->ld->enable_irq(vnet->ld);
	}

	mif_debug("%d pkts\n", rcvd);

	return rcvd;
}

/**
@brief		receive all @b IPC message frames in all RXQs

In a for loop,\n
1) Checks any REQ_ACK received.\n
2) Receives all IPC link frames in every RXQ.\n
3) Sends RES_ACK if there was REQ_ACK from CP.\n
4) Checks any RES_ACK received.\n

@param mld	the pointer to a mem_link_device instance
@param mst	the pointer to a mem_snapshot instance
*/
static void recv_sbd_ipc_frames(struct mem_link_device *mld,
				struct mem_snapshot *mst)
{
	struct sbd_link_device *sl = &mld->sbd_link_dev;
	int i;

	for (i = 0; i < sl->num_channels; i++) {
		struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, RX);

		if (unlikely(rb_empty(rb)))
			continue;

		if (likely(sipc_ps_ch(rb->ch))) {
#ifdef CONFIG_LINK_DEVICE_NAPI
			//mld->link_dev.disable_irq(&mld->link_dev);
			if (napi_schedule_prep(&rb->iod->napi))
				__napi_schedule(&rb->iod->napi);
#else
			rx_net_frames_from_rb(rb, 0);
#endif
		} else {
			rx_ipc_frames_from_rb(rb);
		}
	}
}
#endif

/**
@brief		function for IPC message reception

Invokes cmd_handler for a command or recv_ipc_frames for IPC messages.

@param mld	the pointer to a mem_link_device instance
*/
static void ipc_rx_func(struct mem_link_device *mld)
{
#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
	struct sbd_link_device *sl = &mld->sbd_link_dev;
#endif

	while (1) {
		struct mst_buff *msb;
		u16 intr;

		msb = msb_dequeue(&mld->msb_rxq);
		if (!msb)
			break;

		intr = msb->snapshot.int2ap;

		if (cmd_valid(intr))
			mld->cmd_handler(mld, int2cmd(intr));

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
		if (sbd_active(sl))
			recv_sbd_ipc_frames(mld, &msb->snapshot);
		else
			recv_ipc_frames(mld, &msb->snapshot);
#else
		recv_ipc_frames(mld, &msb->snapshot);
#endif

#if 0
		msb_queue_tail(&mld->msb_log, msb);
#else
		msb_free(msb);
#endif
	}
}

/**
@brief		function for BOOT/DUMP data reception

@param ws	the pointer to a work_struct instance

@remark		RX for BOOT/DUMP must be performed by a WORK in order to avoid
		memory shortage.
*/
static void udl_rx_work(struct work_struct *ws)
{
	struct mem_link_device *mld;

	mld = container_of(ws, struct mem_link_device, udl_rx_dwork.work);

	ipc_rx_func(mld);
}

static void mem_rx_task(unsigned long data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (likely(cp_online(mc)))
		ipc_rx_func(mld);
	else
		queue_delayed_work(ld->rx_wq, &mld->udl_rx_dwork, 0);
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_COMMAND
/**
@weakgroup group_mem_link_command
@{
*/

/**
@brief		reset all member variables in every IPC device

@param mld	the pointer to a mem_link_device instance
*/
static inline void reset_ipc_map(struct mem_link_device *mld)
{
	int i;

	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		struct mem_ipc_device *dev = mld->dev[i];

		set_txq_head(dev, 0);
		set_txq_tail(dev, 0);
		set_rxq_head(dev, 0);
		set_rxq_tail(dev, 0);
	}
}

int mem_reset_ipc_link(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	unsigned int magic;
	unsigned int access;
	int i;

	set_access(mld, 0);
	set_magic(mld, 0);

	reset_ipc_map(mld);

	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		struct mem_ipc_device *dev = mld->dev[i];

		skb_queue_purge(dev->skb_txq);
		atomic_set(&dev->txq.busy, 0);
		dev->req_ack_cnt[TX] = 0;

		skb_queue_purge(dev->skb_rxq);
		atomic_set(&dev->rxq.busy, 0);
		dev->req_ack_cnt[RX] = 0;
	}

	atomic_set(&ld->netif_stopped, 0);

	set_magic(mld, MEM_IPC_MAGIC);
	set_access(mld, 1);

	magic = get_magic(mld);
	access = get_access(mld);
	if (magic != MEM_IPC_MAGIC || access != 1)
		return -EACCES;

	return 0;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_METHOD
/**
@weakgroup group_mem_link_method
@{
*/

/**
@brief		function for the @b init_comm method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static int mem_init_comm(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_ctl *mc = ld->mc;
	struct io_device *check_iod;
	int id = iod->id;
	int fmt2rfs = (SIPC5_CH_ID_RFS_0 - SIPC5_CH_ID_FMT_0);
	int rfs2fmt = (SIPC5_CH_ID_FMT_0 - SIPC5_CH_ID_RFS_0);

	if (atomic_read(&mld->cp_boot_done))
		return 0;

#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	if (mld->iosm) {
		struct sbd_link_device *sl = &mld->sbd_link_dev;
		struct sbd_ipc_device *sid = sbd_ch2dev(sl, iod->id);

		if (atomic_read(&sid->config_done)) {
			tx_iosm_message(mld, IOSM_A2C_OPEN_CH, (u32 *)&id);
			return 0;
		} else {
			mif_err("%s isn't configured channel\n", iod->name);
			return -ENODEV;
		}
	}
#endif

	switch (id) {
	case SIPC5_CH_ID_FMT_0 ... SIPC5_CH_ID_FMT_9:
		check_iod = link_get_iod_with_channel(ld, (id + fmt2rfs));
		if (check_iod ? atomic_read(&check_iod->opened) : true) {
			mif_err("%s: %s->INIT_END->%s\n",
				ld->name, iod->name, mc->name);
			send_ipc_irq(mld, cmd2int(CMD_INIT_END));
			atomic_set(&mld->cp_boot_done, 1);
		} else {
			mif_err("%s is not opened yet\n", check_iod->name);
		}
		break;

	case SIPC5_CH_ID_RFS_0 ... SIPC5_CH_ID_RFS_9:
		check_iod = link_get_iod_with_channel(ld, (id + rfs2fmt));
		if (check_iod) {
			if (atomic_read(&check_iod->opened)) {
				mif_err("%s: %s->INIT_END->%s\n",
					ld->name, iod->name, mc->name);
				send_ipc_irq(mld, cmd2int(CMD_INIT_END));
				atomic_set(&mld->cp_boot_done, 1);
			} else {
				mif_err("%s not opened yet\n", check_iod->name);
			}
		}
		break;

	default:
		break;
	}

	return 0;
}

/**
@brief		function for the @b terminate_comm method
		in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static void mem_terminate_comm(struct link_device *ld, struct io_device *iod)
{
#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	struct mem_link_device *mld = to_mem_link_device(ld);

	if (mld->iosm)
		tx_iosm_message(mld, IOSM_A2C_CLOSE_CH, (u32 *)&iod->id);
#endif
}

/**
@brief		function for the @b send method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the length of data transmitted if there is NO ERROR
@retval "< 0"	an error code
*/
static int mem_send(struct link_device *ld, struct io_device *iod,
		    struct sk_buff *skb)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_ctl *mc = ld->mc;
	enum dev_format id = iod->format;
	u8 ch = iod->id;

	switch (id) {
	case IPC_FMT:
	case IPC_RAW:
	case IPC_RFS:
		if (likely(sipc5_ipc_ch(ch)))
			return xmit_ipc(mld, iod, ch, skb);
		else
			return xmit_udl(mld, iod, ch, skb);

	case IPC_BOOT:
	case IPC_DUMP:
		if (sipc5_udl_ch(ch))
			return xmit_udl(mld, iod, ch, skb);
		break;

	default:
		break;
	}

	mif_err("%s:%s->%s: ERR! Invalid IO device (format:%s id:%d)\n",
		ld->name, iod->name, mc->name, dev_str(id), ch);

	return -ENODEV;
}

/**
@brief		function for the @b boot_on method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static void mem_boot_on(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	unsigned long flags;

	atomic_set(&mld->cp_boot_done, 0);

	spin_lock_irqsave(&ld->lock, flags);
	ld->state = LINK_STATE_OFFLINE;
	spin_unlock_irqrestore(&ld->lock, flags);

	cancel_tx_timer(mld, &mld->tx_timer);

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
#ifdef CONFIG_LTE_MODEM_XMM7260
	sbd_deactivate(&mld->sbd_link_dev);
#endif
	cancel_tx_timer(mld, &mld->sbd_tx_timer);

	if (mld->iosm) {
		memset(mld->base + CMD_RGN_OFFSET, 0, CMD_RGN_SIZE);
		mif_info("Control message region has been initialized\n");
	}
#endif

	purge_txq(mld);
}

/**
@brief		function for the @b xmit_boot method in a link_device instance

Copies a CP bootloader binary in a user space to the BOOT region for CP

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
@param arg	the pointer to a modem_firmware instance

@retval "= 0"	if NO error
@retval "< 0"	an error code
*/
static int mem_xmit_boot(struct link_device *ld, struct io_device *iod,
		     unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	void __iomem *dst;
	void __user *src;
	int err;
	struct modem_firmware mf;

	/**
	 * Get the information about the boot image
	 */
	memset(&mf, 0, sizeof(struct modem_firmware));

	err = copy_from_user(&mf, (const void __user *)arg, sizeof(mf));
	if (err) {
		mif_err("%s: ERR! INFO copy_from_user fail\n", ld->name);
		return -EFAULT;
	}

	/**
	 * Check the size of the boot image
	 */
	if (mf.size > mld->boot_size) {
		mif_err("%s: ERR! Invalid BOOT size %d\n", ld->name, mf.size);
		return -EINVAL;
	}
	mif_err("%s: BOOT size = %d bytes\n", ld->name, mf.size);

	/**
	 * Copy the boot image to the BOOT region
	 */
	memset(mld->boot_base, 0, mld->boot_size);

	dst = (void __iomem *)mld->boot_base;
	src = (void __user *)mf.binary;
	err = copy_from_user(dst, src, mf.size);
	if (err) {
		mif_err("%s: ERR! BOOT copy_from_user fail\n", ld->name);
		return err;
	}

	return 0;
}

/**
@brief		function for the @b dload_start method in a link_device instance

Set all flags and environments for CP binary download

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static int mem_start_download(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	reset_ipc_map(mld);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_BOOT))
		sbd_deactivate(&mld->sbd_link_dev);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_BOOT_ALIGNED))
		ld->aligned = true;
	else
		ld->aligned = false;

	if (mld->dpram_magic) {
		unsigned int magic;

		set_magic(mld, MEM_BOOT_MAGIC);
		magic = get_magic(mld);
		if (magic != MEM_BOOT_MAGIC) {
			mif_err("%s: ERR! magic 0x%08X != BOOT_MAGIC 0x%08X\n",
				ld->name, magic, MEM_BOOT_MAGIC);
			return -EFAULT;
		}
		mif_err("%s: magic == 0x%08X\n", ld->name, magic);
	}

	return 0;
}

/**
@brief		function for the @b firm_update method in a link_device instance

Updates download information for each CP binary image by copying download
information for a CP binary image from a user space to a local buffer in a
mem_link_device instance.

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
@param arg	the pointer to a std_dload_info instance
*/
static int mem_update_firm_info(struct link_device *ld, struct io_device *iod,
				unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	int ret;

	ret = copy_from_user(&mld->img_info, (void __user *)arg,
			     sizeof(struct std_dload_info));
	if (ret) {
		mif_err("ERR! copy_from_user fail!\n");
		return -EFAULT;
	}

	return 0;
}

/**
@brief		function for the @b dump_start method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static int mem_start_upload(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_DUMP))
		sbd_deactivate(&mld->sbd_link_dev);

	reset_ipc_map(mld);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_DUMP_ALIGNED))
		ld->aligned = true;
	else
		ld->aligned = false;

	if (mld->dpram_magic) {
		unsigned int magic;

		set_magic(mld, MEM_DUMP_MAGIC);
		magic = get_magic(mld);
		if (magic != MEM_DUMP_MAGIC) {
			mif_err("%s: ERR! magic 0x%08X != DUMP_MAGIC 0x%08X\n",
				ld->name, magic, MEM_DUMP_MAGIC);
			return -EFAULT;
		}
		mif_err("%s: magic == 0x%08X\n", ld->name, magic);
	}

	return 0;
}

/**
@brief		function for the @b stop method in a link_device instance

@param ld	the pointer to a link_device instance

@remark		It must be invoked after mc->state has already been changed to
		not STATE_ONLINE.
*/
static void mem_close_tx(struct link_device *ld)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	unsigned long flags;

	spin_lock_irqsave(&ld->lock, flags);
	ld->state = LINK_STATE_OFFLINE;
	spin_unlock_irqrestore(&ld->lock, flags);

	stop_tx(mld);
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_SETUP
/**
@weakgroup group_mem_link_setup
@{
*/

void __iomem *mem_vmap(phys_addr_t pa, size_t size, struct page *pages[])
{
	size_t num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	size_t i;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(pa);
		pa += PAGE_SIZE;
	}

	return vmap(pages, num_pages, VM_MAP, prot);
}

void mem_vunmap(void *va)
{
	vunmap(va);
}

/**
@brief		register a physical memory region for a BOOT region

@param mld	the pointer to a mem_link_device instance
@param start	the physical address of an IPC region
@param size	the size of the IPC region
*/
int mem_register_boot_rgn(struct mem_link_device *mld, phys_addr_t start,
			  size_t size)
{
	struct link_device *ld = &mld->link_dev;
	size_t num_pages = (size >> PAGE_SHIFT);
	struct page **pages;

	pages = kmalloc(sizeof(struct page *) * num_pages, GFP_ATOMIC);
	if (!pages)
		return -ENOMEM;

	mif_err("%s: BOOT_RGN start:%pa size:%zu\n", ld->name, &start, size);

	mld->boot_start = start;
	mld->boot_size = size;
	mld->boot_pages = pages;

	return 0;
}

/**
@brief		unregister a physical memory region for a BOOT region

@param mld	the pointer to a mem_link_device instance
*/
void mem_unregister_boot_rgn(struct mem_link_device *mld)
{
	kfree(mld->boot_pages);
	mld->boot_pages = NULL;
	mld->boot_size = 0;
	mld->boot_start = 0;
}

/**
@brief		setup the logical map for an BOOT region

@param mld	the pointer to a mem_link_device instance
@param start	the physical address of an IPC region
@param size	the size of the IPC region
*/
int mem_setup_boot_map(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	phys_addr_t start = mld->boot_start;
	size_t size = mld->boot_size;
	struct page **pages = mld->boot_pages;
	char __iomem *base;

	base = mem_vmap(start, size, pages);
	if (!base) {
		mif_err("%s: ERR! mem_vmap fail\n", ld->name);
		return -EINVAL;
	}
	memset(base, 0, size);

	mld->boot_base = (char __iomem *)base;

	mif_err("%s: BOOT_RGN phys_addr:%pa virt_addr:%p size:%zu\n",
		ld->name, &start, base, size);

	return 0;
}

static void remap_4mb_map_to_ipc_dev(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct shmem_4mb_phys_map *map;
	struct mem_ipc_device *dev;

	map = (struct shmem_4mb_phys_map *)mld->base;

	/* magic code and access enable fields */
	mld->magic = (u32 __iomem *)&map->magic;
	mld->access = (u32 __iomem *)&map->access;

	/* IPC_FMT */
	dev = &mld->ipc_dev[IPC_FMT];

	dev->id = IPC_FMT;
	strcpy(dev->name, "FMT");

	spin_lock_init(&dev->txq.lock);
	atomic_set(&dev->txq.busy, 0);
	dev->txq.head = &map->fmt_tx_head;
	dev->txq.tail = &map->fmt_tx_tail;
	dev->txq.buff = &map->fmt_tx_buff[0];
	dev->txq.size = SHM_4M_FMT_TX_BUFF_SZ;

	spin_lock_init(&dev->rxq.lock);
	atomic_set(&dev->rxq.busy, 0);
	dev->rxq.head = &map->fmt_rx_head;
	dev->rxq.tail = &map->fmt_rx_tail;
	dev->rxq.buff = &map->fmt_rx_buff[0];
	dev->rxq.size = SHM_4M_FMT_RX_BUFF_SZ;

	dev->msg_mask = MASK_SEND_FMT;
	dev->req_ack_mask = MASK_REQ_ACK_FMT;
	dev->res_ack_mask = MASK_RES_ACK_FMT;

	dev->tx_lock = &ld->tx_lock[IPC_FMT];
	dev->skb_txq = &ld->sk_fmt_tx_q;

	dev->rx_lock = &ld->rx_lock[IPC_FMT];
	dev->skb_rxq = &ld->sk_fmt_rx_q;

	dev->req_ack_cnt[TX] = 0;
	dev->req_ack_cnt[RX] = 0;

	mld->dev[IPC_FMT] = dev;

	/* IPC_RAW */
	dev = &mld->ipc_dev[IPC_RAW];

	dev->id = IPC_RAW;
	strcpy(dev->name, "RAW");

	spin_lock_init(&dev->txq.lock);
	atomic_set(&dev->txq.busy, 0);
	dev->txq.head = &map->raw_tx_head;
	dev->txq.tail = &map->raw_tx_tail;
	dev->txq.buff = &map->raw_tx_buff[0];
	dev->txq.size = SHM_4M_RAW_TX_BUFF_SZ;

	spin_lock_init(&dev->rxq.lock);
	atomic_set(&dev->rxq.busy, 0);
	dev->rxq.head = &map->raw_rx_head;
	dev->rxq.tail = &map->raw_rx_tail;
	dev->rxq.buff = &map->raw_rx_buff[0];
	dev->rxq.size = SHM_4M_RAW_RX_BUFF_SZ;

	dev->msg_mask = MASK_SEND_RAW;
	dev->req_ack_mask = MASK_REQ_ACK_RAW;
	dev->res_ack_mask = MASK_RES_ACK_RAW;

	dev->tx_lock = &ld->tx_lock[IPC_RAW];
	dev->skb_txq = &ld->sk_raw_tx_q;

	dev->rx_lock = &ld->rx_lock[IPC_RAW];
	dev->skb_rxq = &ld->sk_raw_rx_q;

	dev->req_ack_cnt[TX] = 0;
	dev->req_ack_cnt[RX] = 0;

	mld->dev[IPC_RAW] = dev;
}

/**
@brief		register a physical memory region for an IPC region

@param mld	the pointer to a mem_link_device instance
@param start	the physical address of an IPC region
@param size	the size of the IPC region
*/
int mem_register_ipc_rgn(struct mem_link_device *mld, phys_addr_t start,
			 size_t size)
{
	struct link_device *ld = &mld->link_dev;
	size_t num_pages = (size >> PAGE_SHIFT);
	struct page **pages;

	pages = kmalloc(sizeof(struct page *) * num_pages, GFP_ATOMIC);
	if (!pages)
		return -ENOMEM;

	mif_err("%s: IPC_RGN start:%pa size:%zu\n", ld->name, &start, size);

	mld->start = start;
	mld->size = size;
	mld->pages = pages;

	return 0;
}

/**
@brief		unregister a physical memory region for an IPC region

@param mld	the pointer to a mem_link_device instance
*/
void mem_unregister_ipc_rgn(struct mem_link_device *mld)
{
	kfree(mld->pages);
	mld->pages = NULL;
	mld->size = 0;
	mld->start = 0;
}

/**
@brief		setup the logical map for an IPC region

@param mld	the pointer to a mem_link_device instance
@param start	the physical address of an IPC region
@param size	the size of the IPC region
*/
int mem_setup_ipc_map(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	phys_addr_t start = mld->start;
	size_t size = mld->size;
	struct page **pages = mld->pages;
	u8 __iomem *base;

	if (mem_type_shmem(mld->type) && mld->size < SZ_4M)
		return -EINVAL;

	base = mem_vmap(start, size, pages);
	if (!base) {
		mif_err("%s: ERR! mem_vmap fail\n", ld->name);
		return -EINVAL;
	}
	memset(base, 0, size);

	mld->base = base;

	mif_err("%s: IPC_RGN phys_addr:%pa virt_addr:%p size:%zu\n",
		ld->name, &start, base, size);

	remap_4mb_map_to_ipc_dev(mld);

	return 0;
}

static int mem_rx_setup(struct link_device *ld)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	if (!zalloc_cpumask_var(&mld->dmask, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&mld->imask, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&mld->tmask, GFP_KERNEL))
		return -ENOMEM;

#ifdef CONFIG_ARGOS
	/* Below hard-coded mask values should be removed later on.
	 * Like net-sysfs, argos module also should support sysfs knob,
	 * so that user layer must be able to control these cpu mask. */
	cpumask_copy(mld->dmask, &hmp_slow_cpu_mask);

	cpumask_or(mld->imask, mld->imask, cpumask_of(3));

	argos_irq_affinity_setup_label(241, "IPC", mld->imask, mld->dmask);
#endif

	ld->tx_wq = create_singlethread_workqueue("mem_tx_work");
	if (!ld->tx_wq) {
		mif_err("%s: ERR! fail to create tx_wq\n", ld->name);
		return -ENOMEM;
	}

	ld->rx_wq = alloc_workqueue(
			"mem_rx_work", WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!ld->rx_wq) {
		mif_err("%s: ERR! fail to create rx_wq\n", ld->name);
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&ld->rx_delayed_work, link_to_demux_work);

	return 0;
}

/**
@brief		create a mem_link_device instance

@param type	the type of a memory interface medium
@param modem	the pointer to a modem_data instance (i.e. modem platform data)
*/
struct mem_link_device *mem_create_link_device(enum mem_iface_type type,
					       struct modem_data *modem)
{
	struct mem_link_device *mld;
	struct link_device *ld;
	int i;
	mif_err("+++\n");

	if (modem->ipc_version < SIPC_VER_50) {
		mif_err("%s<->%s: ERR! IPC version %d < SIPC_VER_50\n",
			modem->link_name, modem->name, modem->ipc_version);
		return NULL;
	}

	/*
	** Alloc an instance of mem_link_device structure
	*/
	mld = kzalloc(sizeof(struct mem_link_device), GFP_KERNEL);
	if (!mld) {
		mif_err("%s<->%s: ERR! mld kzalloc fail\n",
			modem->link_name, modem->name);
		return NULL;
	}

	/*
	** Retrieve modem-specific attributes value
	*/
	mld->type = type;
	mld->attrs = modem->link_attrs;

	/*====================================================================*\
		Initialize "memory snapshot buffer (MSB)" framework
	\*====================================================================*/
	if (msb_init() < 0) {
		mif_err("%s<->%s: ERR! msb_init() fail\n",
			modem->link_name, modem->name);
		goto error;
	}

	/*====================================================================*\
		Set attributes as a "link_device"
	\*====================================================================*/
	ld = &mld->link_dev;

	ld->name = modem->link_name;

	if (mld->attrs & LINK_ATTR(LINK_ATTR_SBD_IPC)) {
		mif_err("%s<->%s: LINK_ATTR_SBD_IPC\n", ld->name, modem->name);
		ld->sbd_ipc = true;
	}

	if (mld->attrs & LINK_ATTR(LINK_ATTR_IPC_ALIGNED)) {
		mif_err("%s<->%s: LINK_ATTR_IPC_ALIGNED\n",
			ld->name, modem->name);
		ld->aligned = true;
	}

	ld->ipc_version = modem->ipc_version;

	ld->mdm_data = modem;

	/*
	Set up link device methods
	*/
	ld->init_comm = mem_init_comm;
#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
	ld->terminate_comm = mem_terminate_comm;
#endif
	ld->send = mem_send;
	ld->netdev_poll = mem_netdev_poll;

	ld->boot_on = mem_boot_on;
	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_BOOT)) {
		if (mld->attrs & LINK_ATTR(LINK_ATTR_XMIT_BTDLR))
			ld->xmit_boot = mem_xmit_boot;
		ld->dload_start = mem_start_download;
		ld->firm_update = mem_update_firm_info;
	}

	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_DUMP)) {
		ld->dump_start = mem_start_upload;
	}

	ld->close_tx = mem_close_tx;

	INIT_LIST_HEAD(&ld->list);

	skb_queue_head_init(&ld->sk_fmt_tx_q);
	skb_queue_head_init(&ld->sk_raw_tx_q);

	skb_queue_head_init(&ld->sk_fmt_rx_q);
	skb_queue_head_init(&ld->sk_raw_rx_q);

	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		spin_lock_init(&ld->tx_lock[i]);
		spin_lock_init(&ld->rx_lock[i]);
	}

	spin_lock_init(&ld->netif_lock);
	atomic_set(&ld->netif_stopped, 0);

	if (mem_rx_setup(ld) < 0)
		goto error;

	/*====================================================================*\
		Set attributes as a "memory link_device"
	\*====================================================================*/
	if (mld->attrs & LINK_ATTR(LINK_ATTR_DPRAM_MAGIC)) {
		mif_err("%s<->%s: LINK_ATTR_DPRAM_MAGIC\n",
			ld->name, modem->name);
		mld->dpram_magic = true;
	}

#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	mld->iosm = true;
	mld->cmd_handler = iosm_event_bh;
	INIT_WORK(&mld->iosm_w, iosm_event_work);
#else
	mld->cmd_handler = mem_cmd_handler;
#endif

	/*====================================================================*\
		Initialize MEM locks, completions, bottom halves, etc
	\*====================================================================*/
	spin_lock_init(&mld->lock);

	/*
	** Initialize variables for TX & RX
	*/
	msb_queue_head_init(&mld->msb_rxq);
	msb_queue_head_init(&mld->msb_log);

	tasklet_init(&mld->rx_tsk, mem_rx_task, (unsigned long)mld);

	hrtimer_init(&mld->tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mld->tx_timer.function = tx_timer_func;

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
	hrtimer_init(&mld->sbd_tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mld->sbd_tx_timer.function = sbd_tx_timer_func;
#endif

	/*
	** Initialize variables for CP booting and crash dump
	*/
	INIT_DELAYED_WORK(&mld->udl_rx_dwork, udl_rx_work);

	mif_err("---\n");
	return mld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}

/**
@}
*/
#endif


