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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/device.h>
#include <linux/module.h>

#include "modem_prj.h"
#include "modem_utils.h"

static int napi_weight = 64;
module_param(napi_weight, int, S_IRUGO);

static u8 sipc5_build_config(struct io_device *iod, struct link_device *ld,
			     unsigned int count);

static void sipc5_build_header(struct io_device *iod, struct link_device *ld,
			       u8 *buff, u8 cfg, u8 ctrl, unsigned int count);

static ssize_t show_waketime(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int msec;
	char *p = buf;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = container_of(miscdev, struct io_device,
			miscdev);

	msec = jiffies_to_msecs(iod->waketime);

	p += sprintf(buf, "raw waketime : %ums\n", msec);

	return p - buf;
}

static ssize_t store_waketime(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long msec;
	int ret;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = container_of(miscdev, struct io_device,
			miscdev);

	if (!iod) {
		pr_err("mif: %s: INVALID IO device\n", miscdev->name);
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &msec);
	if (ret)
		return count;

	iod->waketime = msecs_to_jiffies(msec);
	mif_err("%s: waketime = %lu ms\n", iod->name, msec);

	if (iod->format == IPC_MULTI_RAW) {
		struct modem_shared *msd = iod->msd;
		unsigned int i;

		for (i = SIPC_CH_ID_PDP_0; i < SIPC_CH_ID_BT_DUN; i++) {
			iod = get_iod_with_channel(msd, i);
			if (iod) {
				iod->waketime = msecs_to_jiffies(msec);
				mif_err("%s: waketime = %lu ms\n",
					iod->name, msec);
			}
		}
	}

	return count;
}

static struct device_attribute attr_waketime =
	__ATTR(waketime, S_IRUGO | S_IWUSR, show_waketime, store_waketime);

static ssize_t show_loopback(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;
	unsigned char *ip = (unsigned char *)&msd->loopback_ipaddr;
	char *p = buf;

	p += sprintf(buf, "%u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);

	return p - buf;
}

static ssize_t store_loopback(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;

	msd->loopback_ipaddr = ipv4str_to_be32(buf, count);

	return count;
}

static struct device_attribute attr_loopback =
	__ATTR(loopback, S_IRUGO | S_IWUSR, show_loopback, store_loopback);

static void iodev_showtxlink(struct io_device *iod, void *args)
{
	char **p = (char **)args;
	struct link_device *ld = get_current_link(iod);

	if (iod->io_typ == IODEV_NET && IS_CONNECTED(iod, ld))
		*p += sprintf(*p, "%s<->%s\n", iod->name, ld->name);
}

static ssize_t show_txlink(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;
	char *p = buf;

	iodevs_for_each(msd, iodev_showtxlink, &p);

	return p - buf;
}

static ssize_t store_txlink(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	/* don't change without gpio dynamic switching */
	return -EINVAL;
}

static struct device_attribute attr_txlink =
	__ATTR(txlink, S_IRUGO | S_IWUSR, show_txlink, store_txlink);

static inline void iodev_lock_wlock(struct io_device *iod)
{
	if (iod->waketime > 0 && !wake_lock_active(&iod->wakelock))
		wake_lock_timeout(&iod->wakelock, iod->waketime);
}

static int queue_skb_to_iod(struct sk_buff *skb, struct io_device *iod)
{
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	int len = skb->len;

	if (iod->attrs & IODEV_ATTR(ATTR_NO_CHECK_MAXQ))
		goto enqueue;

	if (rxq->qlen > MAX_IOD_RXQ_LEN) {
		mif_err_limited("%s: %s may be dead (rxq->qlen %d > %d)\n",
			iod->name, iod->app ? iod->app : "corresponding",
			rxq->qlen, MAX_IOD_RXQ_LEN);
		dev_kfree_skb_any(skb);
		goto exit;
	}

enqueue:
	mif_debug("%s: rxq->qlen = %d\n", iod->name, rxq->qlen);
	skb_queue_tail(rxq, skb);

exit:
	wake_up(&iod->wq);
	return len;
}

static int rx_drain(struct sk_buff *skb)
{
	dev_kfree_skb_any(skb);
	return 0;
}

static int rx_loopback(struct sk_buff *skb)
{
	struct io_device *iod = skbpriv(skb)->iod;
	struct link_device *ld = skbpriv(skb)->ld;
	int ret;

	ret = ld->send(ld, iod, skb);
	if (ret < 0) {
		mif_err("%s->%s: ERR! ld->send fail (err %d)\n",
			iod->name, ld->name, ret);
	}

	return ret;
}

static int gather_multi_frame(struct sipc5_link_header *hdr,
			      struct sk_buff *skb)
{
	struct multi_frame_control ctrl = hdr->ctrl;
	struct io_device *iod = skbpriv(skb)->iod;
	struct modem_ctl *mc = iod->mc;
	struct sk_buff_head *multi_q = &iod->sk_multi_q[ctrl.id];
	int len = skb->len;

#ifdef DEBUG_MODEM_IF
	/* If there has been no multiple frame with this ID, ... */
	if (skb_queue_empty(multi_q)) {
		struct sipc_fmt_hdr *fh = (struct sipc_fmt_hdr *)skb->data;
		mif_err("%s<-%s: start of multi-frame (ID:%d len:%d)\n",
			iod->name, mc->name, ctrl.id, fh->len);
	}
#endif
	skb_queue_tail(multi_q, skb);

	if (ctrl.more) {
		/* The last frame has not arrived yet. */
		mif_err("%s<-%s: recv multi-frame (ID:%d rcvd:%d)\n",
			iod->name, mc->name, ctrl.id, skb->len);
	} else {
		struct sk_buff_head *rxq = &iod->sk_rx_q;
		unsigned long flags;

		/* It is the last frame because the "more" bit is 0. */
		mif_err("%s<-%s: end of multi-frame (ID:%d rcvd:%d)\n",
			iod->name, mc->name, ctrl.id, skb->len);

		spin_lock_irqsave(&rxq->lock, flags);
		skb_queue_splice_tail_init(multi_q, rxq);
		spin_unlock_irqrestore(&rxq->lock, flags);

		wake_up(&iod->wq);
	}

	return len;
}

static inline int rx_frame_with_link_header(struct sk_buff *skb)
{
	struct sipc5_link_header *hdr;
	bool multi_frame = sipc5_multi_frame(skb->data);
	int hdr_len = sipc5_get_hdr_len(skb->data);

	/* Remove SIPC5 link header */
	hdr = (struct sipc5_link_header *)skb->data;
	skb_pull(skb, hdr_len);

	if (multi_frame)
		return gather_multi_frame(hdr, skb);
	else
		return queue_skb_to_iod(skb, skbpriv(skb)->iod);
}

static int rx_fmt_ipc(struct sk_buff *skb)
{
	if (skbpriv(skb)->lnk_hdr)
		return rx_frame_with_link_header(skb);
	else
		return queue_skb_to_iod(skb, skbpriv(skb)->iod);
}

static int rx_raw_misc(struct sk_buff *skb)
{
	struct io_device *iod = skbpriv(skb)->iod;

	if (skbpriv(skb)->lnk_hdr) {
		/* Remove the SIPC5 link header */
		skb_pull(skb, sipc5_get_hdr_len(skb->data));
	}

	return queue_skb_to_iod(skb, iod);
}

static int rx_multi_pdp(struct sk_buff *skb)
{
	struct link_device *ld = skbpriv(skb)->ld;
	struct io_device *iod = skbpriv(skb)->iod;
	struct net_device *ndev;
	struct iphdr *iphdr;
	int len = skb->len;
	int ret;

	ndev = iod->ndev;
	if (!ndev) {
		mif_info("%s: ERR! no iod->ndev\n", iod->name);
		return -ENODEV;
	}

	if (skbpriv(skb)->lnk_hdr) {
		/* Remove the SIPC5 link header */
		skb_pull(skb, sipc5_get_hdr_len(skb->data));
	}

	skb->dev = ndev;
	ndev->stats.rx_packets++;
	ndev->stats.rx_bytes += skb->len;

	/* check the version of IP */
	iphdr = (struct iphdr *)skb->data;
	if (iphdr->version == IPv6)
		skb->protocol = htons(ETH_P_IPV6);
	else
		skb->protocol = htons(ETH_P_IP);

	if (iod->use_handover) {
		struct ethhdr *ehdr;
		const char source[ETH_ALEN] = SOURCE_MAC_ADDR;

		ehdr = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));
		memcpy(ehdr->h_dest, ndev->dev_addr, ETH_ALEN);
		memcpy(ehdr->h_source, source, ETH_ALEN);
		ehdr->h_proto = skb->protocol;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb_reset_mac_header(skb);
		skb_pull(skb, sizeof(struct ethhdr));
	}

	log_ipc_pkt(PS_RX, iod->id, skb);

#ifdef CONFIG_LINK_DEVICE_NAPI
	ret = netif_receive_skb(skb);
#else
	if (in_interrupt())
		ret = netif_rx(skb);
	else
		ret = netif_rx_ni(skb);
#endif

	if (ret != NET_RX_SUCCESS) {
		mif_err_limited("%s: %s<-%s: ERR! netif_rx fail\n",
				ld->name, iod->name, iod->mc->name);
	}

	return len;
}

static int rx_demux(struct link_device *ld, struct sk_buff *skb)
{
	struct io_device *iod;
	u8 ch = skbpriv(skb)->sipc_ch;

	if (unlikely(ch == 0)) {
		mif_err("%s: ERR! invalid ch# %d\n", ld->name, ch);
		return -ENODEV;
	}

	/* IP loopback */
	if (ch == DATA_LOOPBACK_CHANNEL && ld->msd->loopback_ipaddr)
		ch = SIPC_CH_ID_PDP_0;

	iod = link_get_iod_with_channel(ld, ch);
	if (unlikely(!iod)) {
		mif_err("%s: ERR! no iod with ch# %d\n", ld->name, ch);
		return -ENODEV;
	}

	/* Don't care whether or not DATA_DRAIN_CHANNEL is opened */
	if (iod->id == DATA_DRAIN_CHANNEL)
		return rx_drain(skb);

	/* Don't care whether or not CP2AP_LOOPBACK_CHANNEL is opened. */
	if (iod->id == CP2AP_LOOPBACK_CHANNEL)
		return rx_loopback(skb);

	if (atomic_read(&iod->opened) <= 0) {
		mif_err_limited("%s: ERR! %s is not opened\n",
				ld->name, iod->name);
		modemctl_notify_event(MDM_EVENT_CP_ABNORMAL_RX);
		return -ENODEV;
	}

	if (sipc5_fmt_ch(ch))
		return rx_fmt_ipc(skb);
	else if (sipc_ps_ch(ch))
		return rx_multi_pdp(skb);
	else
		return rx_raw_misc(skb);
}

/**
@brief		receive the config field in an SIPC5 link frame

1) Checks a config field.\n
2) Calculates the length of link layer header in an incoming frame and stores
   the value to "frm->hdr_len".\n
3) Stores the config field to "frm->hdr" and add the size of config field to
   "frm->hdr_rcvd".\n

@param iod	the pointer to an io_device instance
@param ld	the pointer to a link_device instance
@param buff	the pointer to a buffer in which the frame is stored
@param size	the size of data in the buffer
@param[out] frm	the pointer to an sipc5_frame_data instance

@retval "> 0"	the length of a config field that was copied to @e @@frm
@retval "< 0"	an error code
*/
static int rx_frame_config(struct io_device *iod, struct link_device *ld,
		char *buff, unsigned int size, struct sipc5_frame_data *frm)
{
	unsigned int rest;
	unsigned int rcvd;

	if (unlikely(!sipc5_start_valid(buff))) {
		mif_err("%s->%s: ERR! INVALID config 0x%02x\n",
			ld->name, iod->name, buff[0]);
		return -EBADMSG;
	}

	frm->hdr_len = sipc5_get_hdr_len(buff);

	/* Calculate the size of a segment that will be copied */
	rest = frm->hdr_len;
	rcvd = SIPC5_CONFIG_SIZE;
	mif_debug("%s->%s: hdr_len:%d hdr_rcvd:%d rest:%d size:%d rcvd:%d\n",
		ld->name, iod->name, frm->hdr_len, frm->hdr_rcvd, rest, size,
		rcvd);

	/* Copy the config field of an SIPC5 link header to the header buffer */
	memcpy(frm->hdr, buff, rcvd);
	frm->hdr_rcvd += rcvd;

	return rcvd;
}

/**
@brief		prepare an skb to receive an SIPC5 link frame

1) Extracts the length of a link frame from the link header in "frm->hdr".\n
2) Allocates an skb.\n
3) Calculates the payload size in the link frame.\n
4) Calculates the padding size in the link frame.\n

@param iod	the pointer to an io_device instance
@param ld	the pointer to a link_device instance
@param[out] frm	the pointer to an sipc5_frame_data instance

@return		the pointer to an skb
*/
static struct sk_buff *rx_frame_prepare_skb(struct io_device *iod,
		struct link_device *ld, struct sipc5_frame_data *frm)
{
	struct sk_buff *skb;

	/* Get the frame length */
	frm->len = sipc5_get_frame_len(frm->hdr);

	/* Allocate an skb */
	skb = rx_alloc_skb(frm->len, iod, ld);
	if (!skb) {
		mif_err("%s->%s: ERR! rx_alloc_skb fail (size %d)\n",
			ld->name, iod->name, frm->len);
		return NULL;
	}

	/* Calculates the payload size */
	frm->pay_len = frm->len - frm->hdr_len;

	/* Calculates the padding size */
	if (sipc5_padding_exist(frm->hdr))
		frm->pad_len = (unsigned int)sipc5_calc_padding_size(frm->len);

	mif_debug("%s->%s: size %d (header:%d payload:%d padding:%d)\n",
		ld->name, iod->name, frm->len, frm->hdr_len, frm->pay_len,
		frm->pad_len);

	return skb;
}

/**
@brief		receive the header in an SIPC5 link frame

1) Stores a link layer header to "frm->hdr" temporarily while "frm->hdr_rcvd"
   is less than "frm->hdr_len".\n
2) Then,\n
    Allocates an skb,\n
    Copies the link header from "frm" to "skb",\n
    Register the skb to receive payload.\n

@param iod	the pointer to an io_device instance
@param ld	the pointer to a link_device instance
@param buff	the pointer to a buffer in which incoming data is stored
@param size	the size of data in the buffer
@param[out] frm	the pointer to an sipc5_frame_data instance

@retval "> 0"	the size of a segment that was copied to @e @@frm
@retval "< 0"	an error code
*/
static int rx_frame_header(struct io_device *iod, struct link_device *ld,
		char *buff, unsigned int size, struct sipc5_frame_data *frm)
{
	struct sk_buff *skb;
	unsigned int rest;
	unsigned int rcvd;

	/* Calculate the size of a segment that will be copied */
	rest = frm->hdr_len - frm->hdr_rcvd;
	rcvd = min(rest, size);
	mif_debug("%s->%s: hdr_len:%d hdr_rcvd:%d rest:%d size:%d rcvd:%d\n",
		ld->name, iod->name, frm->hdr_len, frm->hdr_rcvd, rest, size,
		rcvd);

	/* Copy a segment of an SIPC5 link header to "frm" */
	memcpy((frm->hdr + frm->hdr_rcvd), buff, rcvd);
	frm->hdr_rcvd += rcvd;

	if (frm->hdr_rcvd >= frm->hdr_len) {
		/* Prepare an skb with the information in {iod, ld, frm} */
		skb = rx_frame_prepare_skb(iod, ld, frm);
		if (!skb) {
			mif_err("%s->%s: ERR! rx_frame_prepare_skb fail\n",
				ld->name, iod->name);
			return -ENOMEM;
		}

		/* Copy an SIPC5 link header from "frm" to "skb" */
		memcpy(skb_put(skb, frm->hdr_len), frm->hdr, frm->hdr_len);

		/* Register the skb to receive payload */
		fragdata(iod, ld)->skb_recv = skb;
	}

	return rcvd;
}

/**
@brief		receive the payload in an SIPC5 link frame

Stores a link layer payload to <em> fragdata(iod, ld)->skb_recv </em>

@param iod	the pointer to an io_device instance
@param ld	the pointer to a link_device instance
@param buff	the pointer to a buffer in which incoming data is stored
@param size	the size of data in the buffer
@param[out] frm	the pointer to an sipc5_frame_data instance

@retval "> 0"	the size of a segment that was copied to @e $skb
@retval "< 0"	an error code
*/
static int rx_frame_payload(struct io_device *iod, struct link_device *ld,
		char *buff, unsigned int size, struct sipc5_frame_data *frm)
{
	struct sk_buff *skb = fragdata(iod, ld)->skb_recv;
	unsigned int rest;
	unsigned int rcvd;

	/* Calculate the size of a segment that will be copied */
	rest = frm->pay_len - frm->pay_rcvd;
	rcvd = min(rest, size);
	mif_debug("%s->%s: pay_len:%d pay_rcvd:%d rest:%d size:%d rcvd:%d\n",
		ld->name, iod->name, frm->pay_len, frm->pay_rcvd, rest, size,
		rcvd);

	/* Copy an SIPC5 link payload to "skb" */
	memcpy(skb_put(skb, rcvd), buff, rcvd);
	frm->pay_rcvd += rcvd;

	return rcvd;
}

static int rx_frame_padding(struct io_device *iod, struct link_device *ld,
		char *buff, unsigned int size, struct sipc5_frame_data *frm)
{
	struct sk_buff *skb = fragdata(iod, ld)->skb_recv;
	unsigned int rest;
	unsigned int rcvd;

	/* Calculate the size of a segment that will be dropped as padding */
	rest = frm->pad_len - frm->pad_rcvd;
	rcvd = min(rest, size);
	mif_debug("%s->%s: pad_len:%d pad_rcvd:%d rest:%d size:%d rcvd:%d\n",
		ld->name, iod->name, frm->pad_len, frm->pad_rcvd, rest, size,
		rcvd);

	/* Copy an SIPC5 link padding to "skb" */
	memcpy(skb_put(skb, rcvd), buff, rcvd);
	frm->pad_rcvd += rcvd;

	return rcvd;
}

static int rx_frame_done(struct io_device *iod, struct link_device *ld,
		struct sk_buff *skb)
{
	/* Cut off the padding of the current frame */
	skb_trim(skb, sipc5_get_frame_len(skb->data));
	mif_debug("%s->%s: frame length = %d\n", ld->name, iod->name, skb->len);

	return rx_demux(ld, skb);
}

static int recv_frame_from_buff(struct io_device *iod, struct link_device *ld,
		const char *data, unsigned int size)
{
	struct sipc5_frame_data *frm = &fragdata(iod, ld)->f_data;
	struct sk_buff *skb;
	char *buff = (char *)data;
	int rest = size;
	int done = 0;
	int err = 0;

	mif_debug("%s->%s: size %d (RX state = %s)\n", ld->name, iod->name,
		size, rx_state(iod->curr_rx_state));

	while (rest > 0) {
		switch (iod->curr_rx_state) {
		case IOD_RX_ON_STANDBY:
			fragdata(iod, ld)->skb_recv = NULL;
			memset(frm, 0, sizeof(struct sipc5_frame_data));

			done = rx_frame_config(iod, ld, buff, rest, frm);
			if (done < 0) {
				err = done;
				goto err_exit;
			}

			iod->next_rx_state = IOD_RX_HEADER;

			break;

		case IOD_RX_HEADER:
			done = rx_frame_header(iod, ld, buff, rest, frm);
			if (done < 0) {
				err = done;
				goto err_exit;
			}

			if (frm->hdr_rcvd >= frm->hdr_len)
				iod->next_rx_state = IOD_RX_PAYLOAD;
			else
				iod->next_rx_state = IOD_RX_HEADER;

			break;

		case IOD_RX_PAYLOAD:
			done = rx_frame_payload(iod, ld, buff, rest, frm);
			if (done < 0) {
				err = done;
				goto err_exit;
			}

			if (frm->pay_rcvd >= frm->pay_len) {
				if (frm->pad_len > 0)
					iod->next_rx_state = IOD_RX_PADDING;
				else
					iod->next_rx_state = IOD_RX_ON_STANDBY;
			} else {
				iod->next_rx_state = IOD_RX_PAYLOAD;
			}

			break;

		case IOD_RX_PADDING:
			done = rx_frame_padding(iod, ld, buff, rest, frm);
			if (done < 0) {
				err = done;
				goto err_exit;
			}

			if (frm->pad_rcvd >= frm->pad_len)
				iod->next_rx_state = IOD_RX_ON_STANDBY;
			else
				iod->next_rx_state = IOD_RX_PADDING;

			break;

		default:
			mif_err("%s->%s: ERR! INVALID RX state %d\n",
				ld->name, iod->name, iod->curr_rx_state);
			err = -EINVAL;
			goto err_exit;
		}

		if (iod->next_rx_state == IOD_RX_ON_STANDBY) {
			/*
			** A complete frame is in fragdata(iod, ld)->skb_recv.
			*/
			skb = fragdata(iod, ld)->skb_recv;
			err = rx_frame_done(iod, ld, skb);
			if (err < 0)
				goto err_exit;
		}

		buff += done;
		rest -= done;
		if (rest < 0)
			goto err_range;

		iod->curr_rx_state = iod->next_rx_state;
	}

	return size;

err_exit:
	if (fragdata(iod, ld)->skb_recv) {
		mif_err("%s->%s: ERR! clear frag (size:%d done:%d rest:%d)\n",
			ld->name, iod->name, size, done, rest);
		dev_kfree_skb_any(fragdata(iod, ld)->skb_recv);
		fragdata(iod, ld)->skb_recv = NULL;
	}
	iod->curr_rx_state = IOD_RX_ON_STANDBY;
	return err;

err_range:
	mif_err("%s->%s: ERR! size:%d done:%d rest:%d\n",
		ld->name, iod->name, size, done, rest);
	iod->curr_rx_state = IOD_RX_ON_STANDBY;
	return size;
}

/* called from link device when a packet arrives for this io device */
static int io_dev_recv_data_from_link_dev(struct io_device *iod,
		struct link_device *ld, const char *data, unsigned int len)
{
	if (iod->link_header) {
		int err;

		iodev_lock_wlock(iod);

		err = recv_frame_from_buff(iod, ld, data, len);
		if (err < 0) {
			mif_err("%s->%s: ERR! recv_frame_from_buff fail "
				"(err %d)\n", ld->name, iod->name, err);
		}

		return err;
	} else {
		struct sk_buff *skb;

		mif_debug("%s->%s: len %d\n", ld->name, iod->name, len);

		/* save packet to sk_buff */
		skb = rx_alloc_skb(len, iod, ld);
		if (!skb) {
			mif_info("%s->%s: ERR! rx_alloc_skb fail\n",
				ld->name, iod->name);
			return -ENOMEM;
		}

		memcpy(skb_put(skb, len), data, len);
		queue_skb_to_iod(skb, iod);

		return len;
	}
}

static int recv_frame_from_skb(struct io_device *iod, struct link_device *ld,
		struct sk_buff *skb)
{
	struct sk_buff *clone;
	unsigned int rest;
	unsigned int rcvd;
	unsigned int tot;	/* total length including padding */
	int err = 0;

	/* Check the config field of the first frame in @skb */
	if (!sipc5_start_valid(skb->data)) {
		mif_err("%s->%s: ERR! INVALID config 0x%02X\n",
			ld->name, iod->name, skb->data[0]);
		err = -EINVAL;
		goto exit;
	}

	/* Get the total length of the frame with a padding */
	tot = sipc5_get_total_len(skb->data);

	/* Verify the total length of the first frame */
	rest = skb->len;
	if (unlikely(tot > rest)) {
		mif_err("%s->%s: ERR! tot %d > skb->len %d)\n",
			ld->name, iod->name, tot, rest);
		err = -EINVAL;
		goto exit;
	}

	/* If there is only one SIPC5 frame in @skb, */
	if (likely(tot == rest)) {
		/* Receive the SIPC5 frame and return immediately */
		err = rx_frame_done(iod, ld, skb);
		if (err < 0)
			goto exit;
		return 0;
	}

	/*
	** This routine is used only if there are multiple SIPC5 frames in @skb.
	*/
	rcvd = 0;
	while (rest > 0) {
		clone = skb_clone(skb, GFP_ATOMIC);
		if (unlikely(!clone)) {
			mif_err("%s->%s: ERR! skb_clone fail\n",
				ld->name, iod->name);
			err = -ENOMEM;
			goto exit;
		}

		/* Get the start of an SIPC5 frame */
		skb_pull(clone, rcvd);
		if (!sipc5_start_valid(clone->data)) {
			mif_err("%s->%s: ERR! INVALID config 0x%02X\n",
				ld->name, iod->name, clone->data[0]);
			dev_kfree_skb_any(clone);
			err = -EINVAL;
			goto exit;
		}

		/* Get the total length of the current frame with a padding */
		tot = sipc5_get_total_len(clone->data);
		if (unlikely(tot > rest)) {
			mif_err("%s->%s: ERR! dirty frame (tot %d > rest %d)\n",
				ld->name, iod->name, tot, rest);
			dev_kfree_skb_any(clone);
			err = -EINVAL;
			goto exit;
		}

		/* Cut off the padding of the current frame */
		skb_trim(clone, sipc5_get_frame_len(clone->data));

		/* Demux the frame */
		err = rx_demux(ld, clone);
		if (err < 0) {
			mif_err("%s->%s: ERR! rx_demux fail (err %d)\n",
				ld->name, iod->name, err);
			dev_kfree_skb_any(clone);
			goto exit;
		}

		/* Calculate the start of the next frame */
		rcvd += tot;

		/* Calculate the rest size of data in @skb */
		rest -= tot;
	}

exit:
	dev_kfree_skb_any(skb);
	return err;
}

/* called from link device when a packet arrives for this io device */
static int io_dev_recv_skb_from_link_dev(struct io_device *iod,
		struct link_device *ld, struct sk_buff *skb)
{
	enum dev_format dev = iod->format;
	int err;

	switch (dev) {
	case IPC_FMT ... IPC_DUMP:
		iodev_lock_wlock(iod);

		err = recv_frame_from_skb(iod, ld, skb);
		if (err < 0) {
			mif_err("%s->%s: ERR! recv_frame_from_skb fail "
				"(err %d)\n", ld->name, iod->name, err);
		}

		return err;

	default:
		mif_err("%s->%s: ERR! invalid iod\n", ld->name, iod->name);
		return -EINVAL;
	}
}

/*
@brief	called by a link device with the "recv_skb_single" method to upload each
	IPC/PS packet to the corresponding IO device
*/
static int io_dev_recv_skb_single_from_link_dev(struct io_device *iod,
						struct link_device *ld,
						struct sk_buff *skb)
{
	int err;

	iodev_lock_wlock(iod);

	if (skbpriv(skb)->lnk_hdr && ld->aligned) {
		/* Cut off the padding in the current SIPC5 frame */
		skb_trim(skb, sipc5_get_frame_len(skb->data));
	}

	err = rx_demux(ld, skb);
	if (err < 0) {
		mif_err_limited("%s<-%s: ERR! rx_demux fail (err %d)\n",
				iod->name, ld->name, err);
	}

	return err;
}

/*
@brief	called by a link device with the "recv_net_skb" method to upload each PS
	data packet to the network protocol stack
*/
static int io_dev_recv_net_skb_from_link_dev(struct io_device *iod,
					     struct link_device *ld,
					     struct sk_buff *skb)
{
	if (unlikely(atomic_read(&iod->opened) <= 0)) {
		struct modem_ctl *mc = iod->mc;
		mif_err_limited("%s: %s<-%s: ERR! %s is not opened\n",
				ld->name, iod->name, mc->name, iod->name);
		modemctl_notify_event(MDM_EVENT_CP_ABNORMAL_RX);
		return -ENODEV;
	}

	return rx_multi_pdp(skb);
}

/* inform the IO device that the modem is now online or offline or
 * crashing or whatever...
 */
static void io_dev_modem_state_changed(struct io_device *iod,
				       enum modem_state state)
{
	struct modem_ctl *mc = iod->mc;
	enum modem_state old_state = mc->phone_state;

	if (state == old_state)
		goto exit;

	mc->phone_state = state;
	mif_err("%s->state changed (%s -> %s)\n", mc->name,
		cp_state_str(old_state), cp_state_str(state));

	if (state == STATE_CRASH_RESET || state == STATE_CRASH_EXIT) {
		if (mc->wake_lock && !wake_lock_active(mc->wake_lock)) {
			wake_lock(mc->wake_lock);
			mif_err("%s->wake_lock locked\n", mc->name);
		}
	}

exit:
	if (state == STATE_CRASH_RESET
	    || state == STATE_CRASH_EXIT
	    || state == STATE_NV_REBUILDING)
		wake_up(&iod->wq);
}

static void io_dev_sim_state_changed(struct io_device *iod, bool sim_online)
{
	if (atomic_read(&iod->opened) == 0) {
		mif_info("%s: ERR! not opened\n", iod->name);
	} else if (iod->mc->sim_state.online == sim_online) {
		mif_info("%s: SIM state not changed\n", iod->name);
	} else {
		iod->mc->sim_state.online = sim_online;
		iod->mc->sim_state.changed = true;
		mif_info("%s: SIM state changed {online %d, changed %d}\n",
			iod->name, iod->mc->sim_state.online,
			iod->mc->sim_state.changed);
		wake_up(&iod->wq);
	}
}

static void iodev_dump_status(struct io_device *iod, void *args)
{
	if (iod->format == IPC_RAW && iod->io_typ == IODEV_NET) {
		struct link_device *ld = get_current_link(iod);
		mif_com_log(iod->mc->msd, "%s: %s\n", iod->name, ld->name);
	}
}

static int misc_open(struct inode *inode, struct file *filp)
{
	struct io_device *iod = to_io_device(filp->private_data);
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;
	int ret;

	filp->private_data = (void *)iod;

	atomic_inc(&iod->opened);

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->init_comm) {
			ret = ld->init_comm(ld, iod);
			if (ret < 0) {
				mif_err("%s<->%s: ERR! init_comm fail(%d)\n",
					iod->name, ld->name, ret);
				atomic_dec(&iod->opened);
				return ret;
			}
		}
	}

	mif_err("%s (opened %d) by %s\n",
		iod->name, atomic_read(&iod->opened), current->comm);

	return 0;
}

static int misc_release(struct inode *inode, struct file *filp)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;

	if (atomic_dec_and_test(&iod->opened))
		skb_queue_purge(&iod->sk_rx_q);

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->terminate_comm)
			ld->terminate_comm(ld, iod);
	}

	mif_err("%s (opened %d) by %s\n",
		iod->name, atomic_read(&iod->opened), current->comm);

	return 0;
}

static unsigned int misc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct modem_ctl *mc;
	struct sk_buff_head *rxq;

	if (!iod)
		return POLLERR;

	mc = iod->mc;
	rxq = &iod->sk_rx_q;

	if (skb_queue_empty(rxq))
		poll_wait(filp, &iod->wq, wait);

	switch (mc->phone_state) {
	case STATE_BOOTING:
	case STATE_ONLINE:
		if (!mc->sim_state.changed) {
			if (!skb_queue_empty(rxq))
				return POLLIN | POLLRDNORM;
			else /* wq is waken up without rx, return for wait */
				return 0;
		}
		/* fall through, if sim_state has been changed */
	case STATE_CRASH_EXIT:
	case STATE_CRASH_RESET:
	case STATE_NV_REBUILDING:
		/* report crash only if iod is fmt/boot device */
		if (iod->format == IPC_FMT) {
			mif_err("%s: %s.state == %s\n", iod->name, mc->name,
				mc_state(mc));
			return POLLHUP;
		} else if (iod->format == IPC_BOOT || sipc5_boot_ch(iod->id)) {
			mif_err("%s: %s.state == %s\n", iod->name, mc->name,
				mc_state(mc));
			return POLLHUP;
		} else if (iod->format == IPC_DUMP || sipc5_dump_ch(iod->id)) {
			if (!skb_queue_empty(rxq))
				return POLLIN | POLLRDNORM;
			else
				return 0;
		} else {
			mif_err("%s: %s.state == %s\n", iod->name, mc->name,
				mc_state(mc));

			/* give delay to prevent infinite sys_poll call from
			 * select() in APP layer without 'sleep' user call takes
			 * almost 100% cpu usage when it is looked up by 'top'
			 * command.
			 */
			msleep(20);
		}
		break;

	case STATE_OFFLINE:
		/* fall through */
	default:
		break;
	}

	return 0;
}

static long misc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	struct modem_ctl *mc = iod->mc;
	enum modem_state p_state;
	unsigned long size;
	int tx_link;

	switch (cmd) {
	case IOCTL_MODEM_ON:
		if (mc->ops.modem_on) {
			mif_err("%s: IOCTL_MODEM_ON\n", iod->name);
			iod->msd->is_crash_by_ril = false;

			return mc->ops.modem_on(mc);
		}
		mif_err("%s: !mc->ops.modem_on\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_OFF:
		if (mc->ops.modem_off) {
			mif_err("%s: IOCTL_MODEM_OFF\n", iod->name);
			return mc->ops.modem_off(mc);
		}
		mif_err("%s: !mc->ops.modem_off\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_RESET:
		if (mc->ops.modem_reset) {
			mif_err("%s: IOCTL_MODEM_RESET\n", iod->name);
			return mc->ops.modem_reset(mc);
		}
		mif_err("%s: !mc->ops.modem_reset\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_BOOT_ON:
		if (mc->ops.modem_boot_on) {
			mif_err("%s: IOCTL_MODEM_BOOT_ON\n", iod->name);
			return mc->ops.modem_boot_on(mc);
		}
		mif_err("%s: !mc->ops.modem_boot_on\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_BOOT_OFF:
		if (mc->ops.modem_boot_off) {
			mif_err("%s: IOCTL_MODEM_BOOT_OFF\n", iod->name);
			return mc->ops.modem_boot_off(mc);
		}
		mif_err("%s: !mc->ops.modem_boot_off\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_BOOT_DONE:
		mif_err("%s: IOCTL_MODEM_BOOT_DONE\n", iod->name);
		if (mc->ops.modem_boot_done)
			return mc->ops.modem_boot_done(mc);
		return 0;

	case IOCTL_MODEM_STATUS:
		mif_debug("%s: IOCTL_MODEM_STATUS\n", iod->name);

		p_state = mc->phone_state;

		if (p_state != STATE_ONLINE) {
			mif_debug("%s: IOCTL_MODEM_STATUS (state %s)\n",
				iod->name, cp_state_str(p_state));
		}

		if (mc->sim_state.changed) {
			enum modem_state s_state = mc->sim_state.online ?
					STATE_SIM_ATTACH : STATE_SIM_DETACH;
			mc->sim_state.changed = false;
			return s_state;
		}

		if (p_state == STATE_NV_REBUILDING)
			mc->phone_state = STATE_ONLINE;

		return p_state;

	case IOCTL_MODEM_XMIT_BOOT:
		if (ld->xmit_boot) {
			mif_info("%s: IOCTL_MODEM_XMIT_BOOT\n", iod->name);
			return ld->xmit_boot(ld, iod, arg);
		}
		mif_err("%s: !ld->xmit_boot\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_DL_START:
		if (ld->dload_start) {
			mif_info("%s: IOCTL_MODEM_DL_START\n", iod->name);
			return ld->dload_start(ld, iod);
		}
		mif_err("%s: !ld->dload_start\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_FW_UPDATE:
		if (ld->firm_update) {
			mif_info("%s: IOCTL_MODEM_FW_UPDATE\n", iod->name);
			return ld->firm_update(ld, iod, arg);
		}
		mif_err("%s: !ld->firm_update\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_FORCE_CRASH_EXIT:
		if (mc->ops.modem_force_crash_exit) {
			mif_err("%s: IOCTL_MODEM_FORCE_CRASH_EXIT\n",
				iod->name);
			iod->msd->is_crash_by_ril = true;

			return mc->ops.modem_force_crash_exit(mc);
		}
		mif_err("%s: !mc->ops.modem_force_crash_exit\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_DUMP_RESET:
		if (mc->ops.modem_dump_reset) {
			mif_info("%s: IOCTL_MODEM_DUMP_RESET\n", iod->name);
			return mc->ops.modem_dump_reset(mc);
		}
		mif_err("%s: !mc->ops.modem_dump_reset\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_DUMP_START:
		if (mc->ops.modem_dump_start) {
			mif_err("%s: IOCTL_MODEM_DUMP_START\n", iod->name);
			return mc->ops.modem_dump_start(mc);
		} else if (ld->dump_start) {
			mif_err("%s: IOCTL_MODEM_DUMP_START\n", iod->name);
			return ld->dump_start(ld, iod);
		}
		mif_err("%s: !dump_start\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_RAMDUMP_START:
		if (ld->dump_start) {
			mif_info("%s: IOCTL_MODEM_RAMDUMP_START\n", iod->name);
			return ld->dump_start(ld, iod);
		}
		mif_err("%s: !ld->dump_start\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_DUMP_UPDATE:
		if (ld->dump_update) {
			mif_info("%s: IOCTL_MODEM_DUMP_UPDATE\n", iod->name);
			return ld->dump_update(ld, iod, arg);
		}
		mif_err("%s: !ld->dump_update\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_RAMDUMP_STOP:
		if (ld->dump_finish) {
			mif_info("%s: IOCTL_MODEM_RAMDUMP_STOP\n", iod->name);
			return ld->dump_finish(ld, iod, arg);
		}
		mif_err("%s: !ld->dump_finish\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_CP_UPLOAD:
	{
		char *buff = iod->msd->cp_crash_info+
			     strlen(CP_CRASH_TAG)+
			     (iod->msd->is_crash_by_ril ?
				strlen(CP_CRASH_BY_RIL) : 0);
		void __user *user_buff = (void __user *)arg;

		mif_err("%s: ERR! IOCTL_MODEM_CP_UPLOAD\n", iod->name);
		strcpy(iod->msd->cp_crash_info, CP_CRASH_TAG);

		if (iod->msd->is_crash_by_ril)
			strcat(iod->msd->cp_crash_info, CP_CRASH_BY_RIL);

		if (arg) {
			if (copy_from_user(buff, user_buff, CP_CRASH_INFO_SIZE))
				return -EFAULT;
		}
		panic(iod->msd->cp_crash_info);
		return 0;
	}

	case IOCTL_MODEM_PROTOCOL_SUSPEND:
		mif_info("%s: IOCTL_MODEM_PROTOCOL_SUSPEND\n", iod->name);
		if (iod->format == IPC_MULTI_RAW) {
			iodevs_for_each(iod->msd, iodev_netif_stop, 0);
			return 0;
		}
		return -EINVAL;

	case IOCTL_MODEM_PROTOCOL_RESUME:
		mif_info("%s: IOCTL_MODEM_PROTOCOL_RESUME\n", iod->name);
		if (iod->format != IPC_MULTI_RAW) {
			iodevs_for_each(iod->msd, iodev_netif_wake, 0);
			return 0;
		}
		return -EINVAL;

	case IOCTL_MIF_LOG_DUMP:
	{
		void __user *user_buff = (void __user *)arg;

		iodevs_for_each(iod->msd, iodev_dump_status, 0);
		size = MAX_MIF_BUFF_SIZE;
		if (copy_to_user(user_buff, &size, sizeof(unsigned long)))
			return -EFAULT;
		mif_dump_log(mc->msd, iod);
		return 0;
	}

	case IOCTL_MODEM_SET_TX_LINK:
		mif_info("%s: IOCTL_MODEM_SET_TX_LINK\n", iod->name);
		if (copy_from_user(&tx_link, (void __user *)arg, sizeof(int)))
			return -EFAULT;

		mif_info("cur link: %d, new link: %d\n",
				ld->link_type, tx_link);

		if (ld->link_type != tx_link) {
			mif_info("change link: %d -> %d\n",
				ld->link_type, tx_link);
			ld = find_linkdev(iod->msd, tx_link);
			if (!ld) {
				mif_err("find_linkdev(%d) fail\n", tx_link);
				return -ENODEV;
			}

			set_current_link(iod, ld);

			ld = get_current_link(iod);
			mif_info("%s tx_link change success\n",	ld->name);
		}
		return 0;

	default:
		 /* If you need to handle the ioctl for specific link device,
		  * then assign the link ioctl handler to ld->ioctl
		  * It will be call for specific link ioctl */
		if (ld->ioctl)
			return ld->ioctl(ld, iod, cmd, arg);

		mif_info("%s: ERR! undefined cmd 0x%X\n", iod->name, cmd);
		return -EINVAL;
	}

	return 0;
}

static ssize_t misc_write(struct file *filp, const char __user *data,
			  size_t count, loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	struct modem_ctl *mc = iod->mc;
	struct sk_buff *skb;
	char *buff;
	int ret;
	u8 cfg;
	unsigned int headroom;
	unsigned int tailroom;
	unsigned int tx_bytes;

	if (iod->format <= IPC_RFS && iod->id == 0)
		return -EINVAL;

	if (unlikely(!cp_online(mc)) && sipc5_ipc_ch(iod->id)) {
		mif_debug("%s: ERR! %s->state == %s\n",
			iod->name, mc->name, mc_state(mc));
		return -EPERM;
	}

	if (iod->link_header) {
		cfg = sipc5_build_config(iod, ld, count);
		headroom = sipc5_get_hdr_len(&cfg);
		if (ld->aligned)
			tailroom = (unsigned int)sipc5_calc_padding_size(headroom + count);
		else
			tailroom = 0;
	} else {
		cfg = 0;
		headroom = 0;
		tailroom = 0;
	}

	tx_bytes = (unsigned int)(headroom + count + tailroom);

	skb = alloc_skb(tx_bytes, GFP_KERNEL);
	if (!skb) {
		mif_info("%s: ERR! alloc_skb fail (tx_bytes:%d)\n",
			iod->name, tx_bytes);
		return -ENOMEM;
	}

	/* Reserve the space for a link header */
	skb_reserve(skb, headroom);

	/* Copy an IPC message from the user space to the skb */
	buff = skb_put(skb, count);
	if (copy_from_user(buff, data, count)) {
		mif_err("%s->%s: ERR! copy_from_user fail (count %ld)\n",
			iod->name, ld->name, (long)count);
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	/* Store the IO device, the link device, etc. */
	skbpriv(skb)->iod = iod;
	skbpriv(skb)->ld = ld;

	skbpriv(skb)->lnk_hdr = iod->link_header;
	skbpriv(skb)->sipc_ch = iod->id;

	log_ipc_pkt(IOD_TX, iod->id, skb);

	/* Build SIPC5 link header*/
	if (cfg) {
		buff = skb_push(skb, headroom);
		sipc5_build_header(iod, ld, buff, cfg, 0, count);
	}

	/* Apply padding */
	if (tailroom)
		skb_put(skb, tailroom);

	/**
	 * Send the skb with a link device
	 */

	trace_mif_event(skb, tx_bytes, FUNC);

	ret = ld->send(ld, iod, skb);
	if (ret < 0) {
		mif_err("%s->%s: ERR! %s->send fail:%d (tx_bytes:%d len:%ld)\n",
			iod->name, mc->name, ld->name, ret, tx_bytes, (long)count);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return ret - headroom - tailroom;
}

static ssize_t misc_read(struct file *filp, char *buf, size_t count,
			loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	struct sk_buff *skb;
	int copied;

	if (skb_queue_empty(rxq)) {
		long tmo = msecs_to_jiffies(100);
		wait_event_timeout(iod->wq, !skb_queue_empty(rxq), tmo);
	}

	skb = skb_dequeue(rxq);
	if (unlikely(!skb)) {
		mif_info("%s: NO data in RXQ\n", iod->name);
		return 0;
	}

	copied = skb->len > count ? count : skb->len;

	if (copy_to_user(buf, skb->data, copied)) {
		mif_err("%s: ERR! copy_to_user fail\n", iod->name);
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	log_ipc_pkt(IOD_RX, iod->id, skb);

	mif_debug("%s: data:%d copied:%d qlen:%d\n",
		iod->name, skb->len, copied, rxq->qlen);

	if (skb->len > copied) {
		skb_pull(skb, copied);
		skb_queue_head(rxq, skb);
	} else {
		dev_kfree_skb_any(skb);
	}

	return copied;
}

#ifdef CONFIG_COMPAT
/* all compatible */
#define compat_misc_ioctl	misc_ioctl
#else
#define compat_misc_ioctl	NULL
#endif

static const struct file_operations misc_io_fops = {
	.owner = THIS_MODULE,
	.open = misc_open,
	.release = misc_release,
	.poll = misc_poll,
	.unlocked_ioctl = misc_ioctl,
	.compat_ioctl = misc_ioctl,
	.write = misc_write,
	.read = misc_read,
};

static int vnet_open(struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);
	struct io_device *iod = vnet->iod;
	struct modem_shared *msd = vnet->iod->msd;
	struct link_device *ld;
	int ret;

	atomic_inc(&iod->opened);

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->init_comm) {
			vnet->ld = ld;
			ret = ld->init_comm(ld, iod);
			if (ret < 0) {
				mif_err("%s<->%s: ERR! init_comm fail(%d)\n",
					iod->name, ld->name, ret);
				atomic_dec(&iod->opened);
				return ret;
			}
		}
	}

	netif_start_queue(ndev);
#ifdef CONFIG_LINK_DEVICE_NAPI
	napi_enable(&iod->napi);
#endif

	mif_err("%s (opened %d) by %s\n",
		iod->name, atomic_read(&iod->opened), current->comm);

	return 0;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);
	struct io_device *iod = vnet->iod;
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;

	if (atomic_dec_and_test(&iod->opened))
		skb_queue_purge(&vnet->iod->sk_rx_q);

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->terminate_comm)
			ld->terminate_comm(ld, iod);
	}

	netif_stop_queue(ndev);
#ifdef CONFIG_LINK_DEVICE_NAPI
	napi_disable(&iod->napi);
#endif

	mif_err("%s (opened %d) by %s\n",
		iod->name, atomic_read(&iod->opened), current->comm);

	return 0;
}

static int vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);
	struct io_device *iod = vnet->iod;
	struct link_device *ld = get_current_link(iod);
	struct modem_ctl *mc = iod->mc;
	unsigned int count = skb->len;
	struct sk_buff *skb_new = skb;
	char *buff;
	int ret;
	u8 cfg;
	unsigned int headroom;
	unsigned int tailroom;
	unsigned int tx_bytes;

	if (unlikely(!cp_online(mc))) {
		if (!netif_queue_stopped(ndev))
			netif_stop_queue(ndev);
		/* Just drop the TX packet */
		goto drop;
	}

	/* When use `handover' with Network Bridge,
	 * user -> bridge device(rmnet0) -> real rmnet(xxxx_rmnet0) -> here.
	 * bridge device is ethernet device unlike xxxx_rmnet(net device).
	 * We remove the an ethernet header of skb before using skb->len,
	 * because bridge device added an ethernet header to skb.
	 */
	if (iod->use_handover) {
		if (iod->id >= SIPC_CH_ID_PDP_0 && iod->id <= SIPC_CH_ID_PDP_14)
			skb_pull(skb, sizeof(struct ethhdr));
	}

	if (iod->link_header) {
		cfg = sipc5_build_config(iod, ld, count);
		headroom = sipc5_get_hdr_len(&cfg);
		if (ld->aligned)
			tailroom = (unsigned int)sipc5_calc_padding_size(headroom + count);
		else
			tailroom = 0;
	} else {
		cfg = 0;
		headroom = 0;
		tailroom = 0;
	}

	tx_bytes = headroom + count + tailroom;

	if (skb_headroom(skb) < headroom || skb_tailroom(skb) < tailroom) {
		skb_new = skb_copy_expand(skb, headroom, tailroom, GFP_ATOMIC);
		if (!skb_new) {
			mif_info("%s: ERR! skb_copy_expand fail\n", iod->name);
			goto retry;
		}
	}

	/* Store the IO device, the link device, etc. */
	skbpriv(skb_new)->iod = iod;
	skbpriv(skb_new)->ld = ld;

	skbpriv(skb_new)->lnk_hdr = iod->link_header;
	skbpriv(skb_new)->sipc_ch = iod->id;

	log_ipc_pkt(PS_TX, iod->id, skb_new);

	/* Build SIPC5 link header*/
	buff = skb_push(skb_new, headroom);
	if (cfg)
		sipc5_build_header(iod, ld, buff, cfg, 0, count);

	/* IP loop-back */
	if (iod->msd->loopback_ipaddr) {
		struct iphdr *ip_header = (struct iphdr *)skb->data;
		if (ip_header->daddr == iod->msd->loopback_ipaddr) {
			swap(ip_header->saddr, ip_header->daddr);
			buff[SIPC5_CH_ID_OFFSET] = DATA_LOOPBACK_CHANNEL;
		}
	}

	/* Apply padding */
	if (tailroom)
		skb_put(skb_new, tailroom);

	ret = ld->send(ld, iod, skb_new);
	if (unlikely(ret < 0)) {
		if (ret != -EBUSY) {
			mif_err_limited("%s->%s: ERR! %s->send fail:%d "
					"(tx_bytes:%d len:%d)\n",
					iod->name, mc->name, ld->name, ret,
					tx_bytes, count);
		}
		goto drop;
	}

	if (ret != tx_bytes) {
		mif_info("%s->%s: WARN! %s->send ret:%d (tx_bytes:%d len:%d)\n",
			iod->name, mc->name, ld->name, ret, tx_bytes, count);
	}

	ndev->stats.tx_packets++;
	ndev->stats.tx_bytes += count;

	/*
	If @skb has been expanded to $skb_new, @skb must be freed here.
	($skb_new will be freed by the link device.)
	*/
	if (skb_new != skb)
		dev_kfree_skb_any(skb);

	return NETDEV_TX_OK;

retry:
	/*
	If @skb has been expanded to $skb_new, only $skb_new must be freed here
	because @skb will be reused by NET_TX.
	*/
	if (skb_new)
		dev_kfree_skb_any(skb_new);

	return NETDEV_TX_BUSY;

drop:
	ndev->stats.tx_dropped++;

	dev_kfree_skb_any(skb);

	/*
	If @skb has been expanded to $skb_new, $skb_new must also be freed here.
	*/
	if (skb_new != skb)
		dev_kfree_skb_any(skb_new);

	return NETDEV_TX_OK;
}

static struct net_device_ops vnet_ops = {
	.ndo_open = vnet_open,
	.ndo_stop = vnet_stop,
	.ndo_start_xmit = vnet_xmit,
};

static void vnet_setup(struct net_device *ndev)
{
	ndev->netdev_ops = &vnet_ops;
	ndev->type = ARPHRD_PPP;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	ndev->addr_len = 0;
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}

static void vnet_setup_ether(struct net_device *ndev)
{
	ndev->netdev_ops = &vnet_ops;
	ndev->type = ARPHRD_ETHER;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST | IFF_SLAVE;
	ndev->addr_len = ETH_ALEN;
	random_ether_addr(ndev->dev_addr);
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}

/**
@brief		build a configuration field for an SIPC5 link frame header

@param iod	the pointer to the IO device
@param ld	the pointer to the link device
@param count	the length of the payload to be transmitted

@retval "> 0"	the config value for the header
@retval "= 0"	for non-SIPC5 formats
*/
static u8 sipc5_build_config(struct io_device *iod, struct link_device *ld,
			     unsigned int count)
{
	u8 cfg = SIPC5_START_MASK;

	if (iod->format > IPC_DUMP)
		return 0;

	if (ld->aligned)
		cfg |= SIPC5_PADDING_EXIST;

#if 0
	if ((count + SIPC5_MIN_HEADER_SIZE) > ld->mtu[dev])
		cfg |= SIPC5_MULTI_FRAME_CFG;
	else
#endif
	if ((count + SIPC5_MIN_HEADER_SIZE) > 0xFFFF)
		cfg |= SIPC5_EXT_LENGTH_CFG;

	return cfg;
}

/**
@brief		build a link layer header for an SIPC5 link frame

@param iod	the pointer to an IO device
@param ld	the pointer to a link device
@param buff	the pointer to a buffer for the header
@param cfg	the value for the config field in the header
@param ctrl	the value for the control field in the header
@param count	the length of a payload
*/
static void sipc5_build_header(struct io_device *iod, struct link_device *ld,
			       u8 *buff, u8 cfg, u8 ctrl, unsigned int count)
{
	u16 *sz16 = (u16 *)(buff + SIPC5_LEN_OFFSET);
	u32 *sz32 = (u32 *)(buff + SIPC5_LEN_OFFSET);
	unsigned int hdr_len = sipc5_get_hdr_len(&cfg);

	/* Store the config field and the channel ID field */
	buff[SIPC5_CONFIG_OFFSET] = cfg;
	buff[SIPC5_CH_ID_OFFSET] = iod->id;

	/* Store the frame length field */
	if (sipc5_ext_len(buff))
		*sz32 = (u32)(hdr_len + count);
	else
		*sz16 = (u16)(hdr_len + count);

	/* Store the control field */
	if (sipc5_multi_frame(buff))
		buff[SIPC5_CTRL_OFFSET] = ctrl;
}

int sipc5_init_io_device(struct io_device *iod)
{
	int ret = 0;
	int i;
	struct vnet *vnet;

	if (iod->attrs & IODEV_ATTR(ATTR_SBD_IPC))
		iod->sbd_ipc = true;

	if (iod->attrs & IODEV_ATTR(ATTR_NO_LINK_HEADER))
		iod->link_header = false;
	else
		iod->link_header = true;

	/* Get modem state from modem control device */
	iod->modem_state_changed = io_dev_modem_state_changed;
	iod->sim_state_changed = io_dev_sim_state_changed;

	/* Get data from link device */
	iod->recv = io_dev_recv_data_from_link_dev;
	iod->recv_skb = io_dev_recv_skb_from_link_dev;
	iod->recv_skb_single = io_dev_recv_skb_single_from_link_dev;
	iod->recv_net_skb = io_dev_recv_net_skb_from_link_dev;

	/* Register misc or net device */
	switch (iod->io_typ) {
	case IODEV_MISC:
		init_waitqueue_head(&iod->wq);
		skb_queue_head_init(&iod->sk_rx_q);

		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			mif_info("%s: ERR! misc_register failed\n", iod->name);

		break;

	case IODEV_NET:
		skb_queue_head_init(&iod->sk_rx_q);
		if (iod->use_handover)
			iod->ndev = alloc_netdev(sizeof(struct vnet), iod->name,
						 vnet_setup_ether);
		else
			iod->ndev = alloc_netdev(sizeof(struct vnet), iod->name,
						 vnet_setup);

		if (!iod->ndev) {
			mif_info("%s: ERR! alloc_netdev fail\n", iod->name);
			return -ENOMEM;
		}

#ifdef CONFIG_LINK_DEVICE_NAPI
		netif_napi_add(iod->ndev, &iod->napi,
				mem_netdev_poll, napi_weight);
#endif

		ret = register_netdev(iod->ndev);
		if (ret) {
			mif_info("%s: ERR! register_netdev fail\n", iod->name);
			free_netdev(iod->ndev);
		}

		mif_debug("iod 0x%p\n", iod);
		vnet = netdev_priv(iod->ndev);
		mif_debug("vnet 0x%p\n", vnet);
		vnet->iod = iod;

		break;

	case IODEV_DUMMY:
		skb_queue_head_init(&iod->sk_rx_q);

		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			mif_info("%s: ERR! misc_register fail\n", iod->name);

		ret = device_create_file(iod->miscdev.this_device,
					&attr_waketime);
		if (ret)
			mif_info("%s: ERR! device_create_file fail\n",
				iod->name);

		ret = device_create_file(iod->miscdev.this_device,
				&attr_loopback);
		if (ret)
			mif_err("failed to create `loopback file' : %s\n",
					iod->name);

		ret = device_create_file(iod->miscdev.this_device,
				&attr_txlink);
		if (ret)
			mif_err("failed to create `txlink file' : %s\n",
					iod->name);
		break;

	default:
		mif_info("%s: ERR! wrong io_type %d\n", iod->name, iod->io_typ);
		return -EINVAL;
	}

	for (i = 0; i < NUM_SIPC_MULTI_FRAME_IDS; i++)
		skb_queue_head_init(&iod->sk_multi_q[i]);

	return ret;
}


