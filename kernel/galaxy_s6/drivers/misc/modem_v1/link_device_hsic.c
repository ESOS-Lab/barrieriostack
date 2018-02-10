/* /linux/drivers/misc/modem_v2/modem_link_device_hsic.c
 *
 * Copyright (C) 2014 Samsung Electronics.
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

#include <linux/if_arp.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_hsic.h"

static void usb_free_urbs(struct usb_link_device *usb_ld,
		struct if_usb_devdata *pipe)
{
	struct urb *urb;
	struct usb_device *usbdev = usb_ld->usbdev;

	while ((urb = usb_get_from_anchor(&pipe->urbs))) {
		usb_poison_urb(urb);
		usb_free_coherent(usbdev, pipe->rx_buf_size,
				urb->transfer_buffer, urb->transfer_dma);
		usb_put_urb(urb);
		usb_free_urb(urb);
	}
}

static int usb_rx_submit(struct if_usb_devdata *pipe,
		struct urb *urb, gfp_t gfp_flags)
{
	int ret = 0;

	if (pipe->disconnected)
		return -ENOENT;

	usb_anchor_urb(urb, &pipe->reading);
	ret = usb_submit_urb(urb, gfp_flags);
	if (ret) {
		usb_unanchor_urb(urb);
		usb_anchor_urb(urb, &pipe->urbs);
		mif_err("submit urb fail with ret (%d)\n", ret);
		return ret;
	}

	usb_mark_last_busy(urb->dev);
	return ret;
}

static void usb_rx_complete(struct urb *urb)
{
	struct if_usb_devdata *pipe = urb->context;
	struct usb_link_device *usb_ld = pipe->usb_ld;
	struct io_device *iod = pipe->iod;
	int ret;

	if (pipe->usbdev)
		usb_mark_last_busy(pipe->usbdev);

	switch (urb->status) {
	case 0:
		if (!urb->actual_length) {
			mif_debug("urb has zero length!\n");
			goto rx_submit;
		}
		ret = iod->recv(iod, &usb_ld->ld, (char *)urb->transfer_buffer,
				urb->actual_length);
		if (ret < 0) {
			mif_err("io device recv error (%d)\n", ret);
			break;
		}
rx_submit:
		if (urb->status == 0) {
			if (pipe->usbdev)
				usb_mark_last_busy(pipe->usbdev);
			usb_rx_submit(pipe, urb, GFP_ATOMIC);
			return;
		}
		break;
	default:
		mif_err("urb error status = %d\n", urb->status);
		break;
	}

	usb_anchor_urb(urb, &pipe->urbs);
}

static int usb_send(struct link_device *ld, struct io_device *iod,
				struct sk_buff *skb)
{
	int ret;
	size_t tx_size;
	struct if_usb_devdata *pipe = NULL;
	struct usb_link_device *usb_ld = to_usb_link_device(ld);

	if (!usb_ld->if_usb_connected)
		return -EINVAL;

	tx_size = skb->len;
	pipe = &usb_ld->devdata[IF_USB_BOOT_EP];

	if (!pipe)
		return -EINVAL;

	/* Synchronous tx */
	ret = usb_tx_skb(pipe, skb);
	if (ret < 0) {
		mif_err("usb_tx_skb fail(%d)\n", ret);
		return -EINVAL;
	}

	return tx_size;
}

static void usb_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;

	switch (urb->status) {
	case 0:
		if (urb->actual_length != urb->transfer_buffer_length)
			mif_err("TX len=%d, Complete len=%d\n",
				urb->transfer_buffer_length,
				urb->actual_length);
		break;
	default:
		mif_err("TX error (%d)\n", urb->status);
	}

	usb_mark_last_busy(urb->dev);
	dev_kfree_skb_any(skb);
	usb_free_urb(urb);
}

int usb_tx_skb(struct if_usb_devdata *pipe, struct sk_buff *skb)
{
	int ret;
	struct usb_link_device *usb_ld = pipe->usb_ld;
	struct device *dev;
	struct urb *urb;
	gfp_t mem_flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;

	if (!usb_ld || !usb_ld->usbdev) {
		mif_info("usbdev is invalid\n");
		return -EINVAL;
	}

	if (!usb_ld->if_usb_connected)
		return -ENODEV;

	dev = &usb_ld->usbdev->dev;
	pm_runtime_get(dev);

	if (!skb) {
		mif_err("invalid skb\n");
		ret = -EINVAL;
		goto done;
	}

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb) {
		mif_err("alloc urb error\n");
		ret = -ENOMEM;
		goto done;
	}

	if (!(pipe->info->flags & FLAG_SEND_NZLP))
		urb->transfer_flags = URB_ZERO_PACKET;

	usb_fill_bulk_urb(urb, pipe->usbdev, pipe->tx_pipe, skb->data,
			skb->len, usb_tx_complete, (void *)skb);

	ret = usb_submit_urb(urb, mem_flags);
	if (ret < 0) {
		mif_err("usb_submit_urb with ret(%d)\n", ret);
		goto done;
	}
done:
	if (pipe->usbdev)
		usb_mark_last_busy(pipe->usbdev);
	pm_runtime_put(dev);

	return ret;
}

static void if_usb_disconnect(struct usb_interface *intf)
{
	struct if_usb_devdata *pipe = usb_get_intfdata(intf);
	struct usb_device *udev = interface_to_usbdev(intf);

	if (!pipe || pipe->disconnected)
		return;

	pipe->usb_ld->if_usb_connected = 0;

	if (pipe->info->unbind) {
		mif_info("unbind(%pf)\n", pipe->info->unbind);
		pipe->info->unbind(pipe, intf);
	}

	usb_kill_anchored_urbs(&pipe->reading);
	usb_free_urbs(pipe->usb_ld, pipe);

	mif_debug("put dev 0x%p\n", udev);
	usb_put_dev(udev);

	return;
}

static int xmm72xx_acm_bind(struct if_usb_devdata *pipe,
		struct usb_interface *intf, struct usb_link_device *usb_ld)
{
	int ret;
	const struct usb_cdc_union_desc *union_hdr = NULL;
	const struct usb_host_interface *data_desc;
	unsigned char *buf = intf->altsetting->extra;
	int buflen = intf->altsetting->extralen;
	struct usb_interface *data_intf = NULL;
	struct usb_interface *control_intf;
	struct usb_device *usbdev = interface_to_usbdev(intf);
	struct usb_driver *usbdrv = to_usb_driver(intf->dev.driver);

	if (!buflen) {
		if (intf->cur_altsetting->endpoint->extralen &&
				    intf->cur_altsetting->endpoint->extra) {
			buflen = intf->cur_altsetting->endpoint->extralen;
			buf = intf->cur_altsetting->endpoint->extra;
		} else {
			data_desc = intf->cur_altsetting;
			if (!data_desc) {
				mif_err("data_desc is NULL\n");
				return -EINVAL;
			}
			mif_err("cdc-data desc - bootrom\n");
			goto found_data_desc;
		}
	}

	while (buflen > 0) {
		if (buf[1] == USB_DT_CS_INTERFACE) {
			switch (buf[2]) {
			case USB_CDC_UNION_TYPE:
				if (union_hdr)
					break;
				union_hdr = (struct usb_cdc_union_desc *)buf;
				break;
			default:
				break;
			}
		}
		buf += buf[0];
		buflen -= buf[0];
	}

	if (!union_hdr) {
		mif_err("USB CDC is not union type\n");
		return -EINVAL;
	}

	control_intf = usb_ifnum_to_if(usbdev, union_hdr->bMasterInterface0);
	if (!control_intf) {
		mif_err("control_inferface is NULL\n");
		return -ENODEV;
	}

	pipe->status = control_intf->altsetting->endpoint;
	if (!usb_endpoint_dir_in(&pipe->status->desc)) {
		mif_err("not initerrupt_in ep\n");
		pipe->status = NULL;
	}

	data_intf = usb_ifnum_to_if(usbdev, union_hdr->bSlaveInterface0);
	if (!data_intf) {
		mif_err("data_inferface is NULL\n");
		return -ENODEV;
	}

	data_desc = data_intf->altsetting;
	if (!data_desc) {
		mif_err("data_desc is NULL\n");
		return -ENODEV;
	}

found_data_desc:
	/* if_usb_set_pipe */
	if ((usb_pipein(data_desc->endpoint[0].desc.bEndpointAddress)) &&
	    (usb_pipeout(data_desc->endpoint[1].desc.bEndpointAddress))) {
		pipe->rx_pipe = usb_rcvbulkpipe(usbdev,
				data_desc->endpoint[0].desc.bEndpointAddress);
		pipe->tx_pipe = usb_sndbulkpipe(usbdev,
				data_desc->endpoint[1].desc.bEndpointAddress);
	} else if ((usb_pipeout(data_desc->endpoint[0].desc.bEndpointAddress))
		&& (usb_pipein(data_desc->endpoint[1].desc.bEndpointAddress))) {
		pipe->rx_pipe = usb_rcvbulkpipe(usbdev,
				data_desc->endpoint[1].desc.bEndpointAddress);
		pipe->tx_pipe = usb_sndbulkpipe(usbdev,
				data_desc->endpoint[0].desc.bEndpointAddress);
	} else {
		mif_err("undefined endpoint\n");
		return -EINVAL;
	}

	mif_debug("EP tx:%x, rx:%x\n",
		data_desc->endpoint[0].desc.bEndpointAddress,
		data_desc->endpoint[1].desc.bEndpointAddress);

	pipe->usbdev = usb_get_dev(usbdev);
	pipe->usb_ld = usb_ld;
	pipe->data_intf = data_intf;
	pipe->disconnected = 0;

	if (data_intf) {
		ret = usb_driver_claim_interface(usbdrv, data_intf,
							(void *)pipe);
		if (ret < 0) {
			mif_err("usb_driver_claim() failed\n");
			return ret;
		}
	}
	usb_set_intfdata(intf, (void *)pipe);

	return 0;
}

static void xmm72xx_acm_unbind(struct if_usb_devdata *pipe,
	struct usb_interface *intf)
{
	usb_driver_release_interface(to_usb_driver(intf->dev.driver), intf);
	pipe->data_intf = NULL;
	pipe->usbdev = NULL;
	pipe->disconnected = 1;

	usb_set_intfdata(intf, NULL);
	return;
}

static int if_usb_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	int ret;
	int dev_index = 0;
	struct if_usb_devdata *pipe;
	int subclass = intf->altsetting->desc.bInterfaceSubClass;
	struct usb_device *usbdev = interface_to_usbdev(intf);
	struct usb_id_info *info = (struct usb_id_info *)id->driver_info;
	struct usb_link_device *usb_ld = info->usb_ld;
	struct urb *urb;
	int cnt = 1;

	pr_debug("%s: Class=%d, SubClass=%d, Protocol=%d\n", __func__,
		intf->altsetting->desc.bInterfaceClass,
		intf->altsetting->desc.bInterfaceSubClass,
		intf->altsetting->desc.bInterfaceProtocol);

	usb_ld->usbdev = usbdev;
	pm_runtime_forbid(&usbdev->dev);

	switch (subclass) {
	case USB_CDC_SUBCLASS_ACM:
	default:
		dev_index = 0;
		pipe = &usb_ld->devdata[dev_index];
		pipe->idx = dev_index;
		pipe->iod =
			link_get_iod_with_format(&usb_ld->ld, IPC_BOOT);
		pipe->rx_buf_size = (16 * 1024);
		break;
	}

	pipe->info = info;

	if (info->bind) {
		mif_info("bind(%pf), pipe(%d)\n", info->bind, dev_index);
		ret = info->bind(pipe, intf, usb_ld);
		if (ret < 0) {
			mif_err("SubClass bind(%pf) err=%d\n", info->bind, ret);
			goto error_exit;
		}
		do {
			urb = usb_alloc_urb(0, GFP_KERNEL);
			if (!urb) {
				mif_err("alloc urb fail\n");
				ret = -ENOMEM;
				goto error_exit;
			}
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
			urb->transfer_buffer = usb_alloc_coherent(usbdev,
					pipe->rx_buf_size, GFP_KERNEL,
					&urb->transfer_dma);
			if (!urb->transfer_buffer) {
				mif_err(
				"Failed to allocate transfer buffer\n");
				usb_free_urb(urb);
				ret = -ENOMEM;
				goto error_exit;
			}
			usb_fill_bulk_urb(urb, usbdev, pipe->rx_pipe,
				urb->transfer_buffer, pipe->rx_buf_size,
				usb_rx_complete, pipe);
			usb_anchor_urb(urb, &pipe->urbs);
#if 0
			ret = usb_rx_submit(pipe, urb, GFP_ATOMIC);
#else
			ret = usb_submit_urb(urb, GFP_ATOMIC);
#endif
			if (ret < 0) {
				mif_err("failed to submit urb (%d)\n", ret);
				goto error_exit;
			}
		} while (cnt++ < info->urb_cnt);
	} else {
		mif_err("SubClass bind func was not defined\n");
		ret = -EINVAL;
		goto error_exit;
	}

	pm_suspend_ignore_children(&usbdev->dev, true);

	switch (info->intf_id) {
	case BOOT_DOWN:
		usb_ld->if_usb_connected = 1;
		break;
	default:
		mif_err("undefined interface value(0x%x)\n", info->intf_id);
		break;
	}

	mif_info("successfully done\n");

	return 0;

error_exit:
	usb_free_urbs(usb_ld, pipe);
	return ret;
}

static struct usb_id_info hsic_boot_down_info = {
	.description = "HSIC boot",
	.flags = FLAG_SEND_NZLP,
	.intf_id = BOOT_DOWN,
	.bind = xmm72xx_acm_bind,
	.unbind = xmm72xx_acm_unbind,
};
static struct usb_device_id if_usb_ids[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0x8087, 0x07ef, USB_CLASS_CDC_DATA,
		0, USB_CDC_PROTO_NONE),
	.driver_info = (unsigned long)&hsic_boot_down_info,
	}, /* IMC XMM7260 BOOTROM */
	{}
};
MODULE_DEVICE_TABLE(usb, if_usb_ids);

static struct usb_driver if_usb_driver = {
	.name =		"cdc_modem",
	.probe =	if_usb_probe,
	.disconnect =	if_usb_disconnect,
	.id_table =	if_usb_ids,
	.supports_autosuspend = 0,
};

static int if_usb_init(struct link_device *ld)
{
	int ret;
	int i;
	struct usb_link_device *usb_ld = to_usb_link_device(ld);
	struct if_usb_devdata *pipe;
	struct usb_id_info *id_info;

	/* to connect usb link device with usb interface driver */
	for (i = 0; i < ARRAY_SIZE(if_usb_ids); i++) {
		id_info = (struct usb_id_info *)if_usb_ids[i].driver_info;
		if (id_info)
			id_info->usb_ld = usb_ld;
	}

	ret = usb_register(&if_usb_driver);
	if (ret) {
		mif_err("usb_register_driver() fail : %d\n", ret);
		return ret;
	}

	/* common devdata initialize */
	for (i = 0; i < usb_ld->max_link_ch; i++) {
		pipe = &usb_ld->devdata[i];
		init_usb_anchor(&pipe->urbs);
		init_usb_anchor(&pipe->reading);
		skb_queue_head_init(&pipe->sk_tx_q);
	}

	mif_info("if_usb_init() done : %d, usb_ld (0x%p)\n", ret, usb_ld);

	return 0;
}

/* Export private data symbol for ramdump debugging */
static struct usb_link_device *g_mif_usbld;

struct link_device *hsic_create_link_device(void *data)
{
	int ret;
	struct usb_link_device *usb_ld;
	struct link_device *ld;

	usb_ld = kzalloc(sizeof(struct usb_link_device), GFP_KERNEL);
	if (!usb_ld) {
		mif_err("get usb_ld fail -ENOMEM\n");
		return NULL;
	}
	g_mif_usbld = usb_ld;

	usb_ld->max_link_ch = 1;
	usb_ld->devdata = kzalloc(
		usb_ld->max_link_ch * sizeof(struct if_usb_devdata),
		GFP_KERNEL);
	if (!usb_ld->devdata) {
		mif_err("get devdata fail -ENOMEM\n");
		goto error;
	}

	INIT_LIST_HEAD(&usb_ld->ld.list);

	ld = &usb_ld->ld;

	ld->name = "hsic";
	ld->send = usb_send;

	ret = if_usb_init(ld);
	if (ret)
		goto error;

	mif_info("%s : create_link_device DONE\n", usb_ld->ld.name);
	return (void *)ld;
error:
	kfree(usb_ld->devdata);
	kfree(usb_ld);
	return NULL;
}

static void __exit if_usb_exit(void)
{
	usb_deregister(&if_usb_driver);
}
