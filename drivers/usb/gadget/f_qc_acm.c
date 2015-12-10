/*
 * f_serial.c - generic USB serial function driver
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <mach/usb_gadget_xport.h>

#include "u_serial.h"
#include "gadget_chips.h"


/*
 * This function packages a simple "generic serial" port with no real
 * control mechanisms, just raw data transfer over two bulk endpoints.
 *
 * Because it's not standardized, this isn't as interoperable as the
 * CDC ACM driver.  However, for many purposes it's just as functional
 * if you can arrange appropriate host side drivers.
 */
#define GSERIAL_NO_PORTS 4

struct gser_descs {
	struct usb_endpoint_descriptor	*in;
	struct usb_endpoint_descriptor	*out;
#if 1 //def CONFIG_USB_DUN_SUPPORT
	struct usb_endpoint_descriptor	*notify;
#endif
};


struct f_gser {
	struct gserial			port;
	u8				ctrl_id, data_id;
	u8				port_num;

	struct gser_descs		fs;
	struct gser_descs		hs;
	u8				online;
	enum transport_type		transport;

#if 1 //def CONFIG_USB_DUN_SUPPORT
	u8				pending;
	spinlock_t			lock;
	struct usb_ep			*notify;
	struct usb_endpoint_descriptor	*notify_desc;
	struct usb_request		*notify_req;

	struct usb_cdc_line_coding	port_line_coding;

	/* SetControlLineState request */
	u16				port_handshake_bits;
#define ACM_CTRL_RTS	(1 << 1)	/* unused with full duplex */
#define ACM_CTRL_DTR	(1 << 0)	/* host is ready for data r/w */

	/* SerialState notification */
	u16				serial_state;
#define ACM_CTRL_OVERRUN	(1 << 6)
#define ACM_CTRL_PARITY		(1 << 5)
#define ACM_CTRL_FRAMING	(1 << 4)
#define ACM_CTRL_RI		(1 << 3)
#define ACM_CTRL_BRK		(1 << 2)
#define ACM_CTRL_DSR		(1 << 1)
#define ACM_CTRL_DCD		(1 << 0)
#endif
};

static unsigned int no_tty_ports;
static unsigned int no_hsic_sports;
static unsigned int nr_ports;

static struct port_info {
	enum transport_type	transport;
	unsigned		port_num;
	unsigned char		client_port_num;
} gserial_ports[GSERIAL_NO_PORTS];



static inline struct f_gser *func_to_gser(struct usb_function *f)
{
	return container_of(f, struct f_gser, port.func);
}

#if 1 //def CONFIG_USB_DUN_SUPPORT
static inline struct f_gser *port_to_gser(struct gserial *p)
{
	return container_of(p, struct f_gser, port);
}
#define GS_LOG2_NOTIFY_INTERVAL		5	/* 1 << 5 == 32 msec */
#define GS_NOTIFY_INTERVAL_MS		32
#define GS_NOTIFY_MAXPACKET		10	/* notification + 2 bytes */
#endif
/*-------------------------------------------------------------------------*/

/* interface descriptor: */

static struct usb_interface_assoc_descriptor
gser_iad_descriptor = {
	.bLength =		sizeof gser_iad_descriptor,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount =	2,	/* control + data */
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ACM,
	.bFunctionProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	/* .iFunction =		DYNAMIC */
};

static struct usb_interface_descriptor gser_control_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	/* .iInterface = DYNAMIC */
};

static struct usb_interface_descriptor gser_data_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

#if 1 //def CONFIG_USB_DUN_SUPPORT
static struct usb_cdc_header_desc gser_header_desc  = {
	.bLength =		sizeof(gser_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		__constant_cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor
gser_call_mgmt_descriptor  = {
	.bLength =		sizeof(gser_call_mgmt_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,
	.bmCapabilities =	0,
	/* .bDataInterface = DYNAMIC */
};

static struct usb_cdc_acm_descriptor gser_descriptor  = {
	.bLength =		sizeof(gser_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,
	.bmCapabilities =	USB_CDC_CAP_LINE,
};

static struct usb_cdc_union_desc gser_union_desc  = {
	.bLength =		sizeof(gser_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};
#endif
/* full speed support: */
#if 1 //def CONFIG_USB_DUN_SUPPORT
static struct usb_endpoint_descriptor gser_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		1 << GS_LOG2_NOTIFY_INTERVAL,
};
#endif

static struct usb_endpoint_descriptor gser_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor gser_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *gser_fs_function[] = {
	(struct usb_descriptor_header *) &gser_iad_descriptor,
	(struct usb_descriptor_header *) &gser_control_interface_desc,
#if 1 //def CONFIG_USB_DUN_SUPPORT
	(struct usb_descriptor_header *) &gser_header_desc,
	(struct usb_descriptor_header *) &gser_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &gser_descriptor,
	(struct usb_descriptor_header *) &gser_union_desc,
	(struct usb_descriptor_header *) &gser_fs_notify_desc,
#endif
	(struct usb_descriptor_header *) &gser_data_interface_desc,
	(struct usb_descriptor_header *) &gser_fs_in_desc,
	(struct usb_descriptor_header *) &gser_fs_out_desc,
	NULL,
};

/* high speed support: */
#if 1 //def CONFIG_USB_DUN_SUPPORT
static struct usb_endpoint_descriptor gser_hs_notify_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		GS_LOG2_NOTIFY_INTERVAL+4,
};
#endif

static struct usb_endpoint_descriptor gser_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor gser_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_descriptor_header *gser_hs_function[] = {
	(struct usb_descriptor_header *) &gser_iad_descriptor,
	(struct usb_descriptor_header *) &gser_control_interface_desc,
#if 1 //def CONFIG_USB_DUN_SUPPORT
	(struct usb_descriptor_header *) &gser_header_desc,
	(struct usb_descriptor_header *) &gser_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &gser_descriptor,
	(struct usb_descriptor_header *) &gser_union_desc,
	(struct usb_descriptor_header *) &gser_hs_notify_desc,
#endif
	(struct usb_descriptor_header *) &gser_data_interface_desc,
	(struct usb_descriptor_header *) &gser_hs_in_desc,
	(struct usb_descriptor_header *) &gser_hs_out_desc,
	NULL,
};


#if 1

static struct usb_endpoint_descriptor gser_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor gser_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor gser_ss_bulk_comp_desc = {
	.bLength =              sizeof gser_ss_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *gser_ss_function[] = {
	(struct usb_descriptor_header *) &gser_iad_descriptor,
	(struct usb_descriptor_header *) &gser_control_interface_desc,
	(struct usb_descriptor_header *) &gser_header_desc,
	(struct usb_descriptor_header *) &gser_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &gser_descriptor,
	(struct usb_descriptor_header *) &gser_union_desc,
	(struct usb_descriptor_header *) &gser_hs_notify_desc,
	(struct usb_descriptor_header *) &gser_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &gser_data_interface_desc,
	(struct usb_descriptor_header *) &gser_ss_in_desc,
	(struct usb_descriptor_header *) &gser_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &gser_ss_out_desc,
	(struct usb_descriptor_header *) &gser_ss_bulk_comp_desc,
	NULL,
};

#endif

/* string descriptors: */

#define ACM_CTRL_IDX	0
#define ACM_DATA_IDX	1
#define ACM_IAD_IDX	2

static struct usb_string gser_string_defs[] = {
[ACM_CTRL_IDX].s = "CDC Abstract Control Model (ACM)",
	[ACM_DATA_IDX].s = "CDC ACM Data",
	[ACM_IAD_IDX ].s = "CDC Serial",
	{  } /* end of list */
};

static struct usb_gadget_strings gser_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		gser_string_defs,
};

static struct usb_gadget_strings *gser_strings[] = {
	&gser_string_table,
	NULL,
};

static int gport_setup(struct usb_configuration *c)
{
	int ret = 0;
	int port_idx;
	int i;

	pr_info("usb:(hsic) %s: no_hsic_sports: %u nr_ports: %u\n",
			__func__, no_hsic_sports, nr_ports);

	if (no_tty_ports){
		pr_info("usb:(hsic) %s: no_tty_ports=%d\n", __func__, no_tty_ports);
		//ret = gserial_setup(c->cdev->gadget, no_tty_ports);
	}
	if (no_hsic_sports) {
		pr_info("usb:(hsic) %s: no_hsic_sports: %u nr_ports: %u\n",
			__func__, no_hsic_sports, nr_ports);

		port_idx = ghsic_data_setup(no_hsic_sports, USB_GADGET_SERIAL);
		if (port_idx < 0)
			{
				pr_info("usb:(hsic) %s: ghsic_data_setup error : \n",
			__func__);
			return port_idx;
			}

		for (i = 0; i < nr_ports; i++) {
			if (gserial_ports[i].transport ==
					USB_GADGET_XPORT_HSIC) {
				gserial_ports[i].client_port_num = port_idx;
				port_idx++;
			}
		}

		/*clinet port num is same for data setup and ctrl setup*/
		ret = ghsic_ctrl_setup(no_hsic_sports, USB_GADGET_SERIAL);
		if (ret < 0)
		{
			pr_info("usb:(hsic) gport_setup error return [%d] \r\n",ret);
			return ret;
		}
		return ret;
	}
	return ret;
}

static int gport_connect(struct f_gser *gser)
{
	unsigned	port_num;
	int		ret;

	port_num = gserial_ports[gser->port_num].client_port_num;

	pr_info("usb:(hsic) %s: transport: %s f_gser: %p gserial: %p port_num: %d\n",
			__func__, xport_to_str(gser->transport),
			gser, &gser->port, gser->port_num);



	switch (gser->transport) {
	case USB_GADGET_XPORT_TTY:
		gserial_connect(&gser->port, port_num);
		break;
	case USB_GADGET_XPORT_HSIC:
		ret = ghsic_ctrl_connect(&gser->port, port_num);
		if (ret) {
			pr_debug("usb:(hsic) %s: ghsic_ctrl_connect failed: err:%d\n",
					__func__, ret);
			return ret;
		}
		ret = ghsic_data_connect(&gser->port, port_num);
		if (ret) {
			pr_debug("usb:(hsic) %s: ghsic_data_connect failed: err:%d\n",
					__func__, ret);
			ghsic_ctrl_disconnect(&gser->port, port_num);
			return ret;
		}
		break;
	default:
		pr_debug("usb:(hsic) %s: Un-supported transport: %s\n", __func__,
				xport_to_str(gser->transport));
		return -ENODEV;
	}

	return 0;
}

static int gport_disconnect(struct f_gser *gser)
{
	unsigned port_num;

	port_num = gserial_ports[gser->port_num].client_port_num;
	pr_debug("usb:(hsic) %s: transport: %s f_gser: %p gserial: %p port_num: %d\n",
			__func__, xport_to_str(gser->transport),
			gser, &gser->port, gser->port_num);



	switch (gser->transport) {
	case USB_GADGET_XPORT_TTY:
		gserial_disconnect(&gser->port);
		break;
	case USB_GADGET_XPORT_HSIC:
		ghsic_ctrl_disconnect(&gser->port, port_num);
		ghsic_data_disconnect(&gser->port, port_num);
		break;
	default:
		pr_err("%s: Un-supported transport:%s\n", __func__,
				xport_to_str(gser->transport));
		return -ENODEV;
	}

	return 0;
}

#if 1 //def CONFIG_USB_DUN_SUPPORT
static void gser_complete_set_line_coding(struct usb_ep *ep,
		struct usb_request *req)
{
	struct f_gser            *gser = ep->driver_data;

	if (req->status != 0) {
		pr_info("gser ttyGS%d completion, err %d\n",
				gser->port_num, req->status);
		return;
	}

	/* normal completion */
	if (req->actual != sizeof(gser->port_line_coding)) {
		pr_info("gser ttyGS%d short resp, len %d\n",
				gser->port_num, req->actual);
		usb_ep_set_halt(ep);
	} else {
		struct usb_cdc_line_coding	*value = req->buf;
		gser->port_line_coding = *value;
	}
}
/*-------------------------------------------------------------------------*/

static int
gser_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_gser            *gser = func_to_gser(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	 *req = cdev->req;
	int			 value = -EOPNOTSUPP;
	u16			 w_index = le16_to_cpu(ctrl->wIndex);
	u16			 w_value = le16_to_cpu(ctrl->wValue);
	u16			 w_length = le16_to_cpu(ctrl->wLength);

	pr_debug("usb:(hsic) %s: Type=0x%x, Rq=0x%x\n", __func__, ctrl->bRequestType, ctrl->bRequest);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	/* SET_LINE_CODING ... just read and save what the host sends */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_LINE_CODING:

		pr_debug("usb:(hsic) %s: IN: USB_CDC_REQ_SET_LINE_CODING\n", __func__);

		if (w_length != sizeof(struct usb_cdc_line_coding)
				|| w_index != gser->ctrl_id)
			goto invalid;

		value = w_length;
		cdev->gadget->ep0->driver_data = gser;
		req->complete = gser_complete_set_line_coding;
		break;

	/* GET_LINE_CODING ... return what host sent, or initial value */
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_GET_LINE_CODING:
			pr_debug("usb:(hsic) %s: OUT: USB_CDC_REQ_GET_LINE_CODING\n", __func__);
		if (w_index != gser->ctrl_id)
			goto invalid;

		value = min_t(unsigned, w_length,
				sizeof(struct usb_cdc_line_coding));
		memcpy(req->buf, &gser->port_line_coding, value);
		break;

	/* SET_CONTROL_LINE_STATE ... save what the host sent */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		if (w_index != gser->ctrl_id)
			goto invalid;

		pr_debug("usb:(hsic) %s: OUT: USB_CDC_REQ_SET_CONTROL_LINE_STATE (%d)\n", __func__, w_value);
		value = 0;
		gser->port_handshake_bits = w_value;
		if (gser->port.notify_modem) {
			unsigned port_num =
				gserial_ports[gser->port_num].client_port_num;
			pr_debug("usb:(hsic) %s: OUT: USB_CDC_REQ_SET_CONTROL_LINE_STATE (port_num=%d)\n", __func__, port_num);
			gser->port.notify_modem(&gser->port,
					port_num, w_value);
		}
		break;

	default:
invalid:
		pr_debug( "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		pr_debug("gser ttyGS%d req%02x.%02x v%04x i%04x l%d\n",
			gser->port_num, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			pr_err("gser response on ttyGS%d, err %d\n",
					gser->port_num, value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}
#endif
static int gser_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_gser		*gser = func_to_gser(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	/* we know alt == 0, so this is an activation or a reset */
#if 0
	int rc = 0;

	if (gser->notify->driver_data) {
		pr_debug( "reset generic ctl ttyGS%d\n", gser->port_num);
		usb_ep_disable(gser->notify);
	}

	gser->notify_desc = ep_choose(cdev->gadget,
			gser->hs.notify,
			gser->fs.notify);
	rc = usb_ep_enable(gser->notify, gser->notify_desc);

	if (rc) {
		pr_err( "can't enable %s, result %d\n",
					gser->notify->name, rc);
		return rc;
	}
	gser->notify->driver_data = gser;

	if (gser->port.in->driver_data) {
		pr_debug( "reset generic data ttyGS%d\n", gser->port_num);
		gport_disconnect(gser);
	} else {
		pr_debug( "activate generic data ttyGS%d\n", gser->port_num);
	}

	gser->port.in_desc = ep_choose(cdev->gadget,
			gser->hs.in, gser->fs.in);
	gser->port.out_desc = ep_choose(cdev->gadget,
			gser->hs.out, gser->fs.out);
	gport_connect(gser);

	gser->online = 1;
	return rc;
#else
	if (intf == gser->ctrl_id) {
		if (gser->notify->driver_data) {
			pr_debug( "reset acm control interface %d\n", intf);
			usb_ep_disable(gser->notify);
		} else {
			pr_debug( "init acm ctrl interface %d\n", intf);
			if (config_ep_by_speed(cdev->gadget, f, gser->notify))
				return -EINVAL;
		}
		usb_ep_enable(gser->notify);
		gser->notify->driver_data = gser;

	} else if (intf == gser->data_id) {
		if (gser->port.in->driver_data) {
			pr_debug( "reset acm ttyGS%d\n", gser->port_num);
			gserial_disconnect(&gser->port);
		}
		if (!gser->port.in->desc || !gser->port.out->desc) {
			pr_debug( "activate acm ttyGS%d\n", gser->port_num);
			if (config_ep_by_speed(cdev->gadget, f,
					       gser->port.in) ||
			    config_ep_by_speed(cdev->gadget, f,
					       gser->port.out)) {
				gser->port.in->desc = NULL;
				gser->port.out->desc = NULL;
				return -EINVAL;
			}
		}
//		gserial_connect(&acm->port, acm->port_num);
		gport_connect(gser);

	gser->online = 1;

	} else
		return -EINVAL;

	return 0;
#endif

}

static void gser_disable(struct usb_function *f)
{
	struct f_gser	*gser = func_to_gser(f);

	pr_debug( "generic ttyGS%d deactivated\n", gser->port_num);

	gport_disconnect(gser);

#if 1 //def CONFIG_USB_DUN_SUPPORT
	usb_ep_fifo_flush(gser->notify);
	usb_ep_disable(gser->notify);
	gser->notify->driver_data = NULL;
#endif
	gser->online = 0;
}

#if 1 //def CONFIG_USB_DUN_SUPPORT

#if 0
static int gser_notify_serial_state(struct f_gser *gser);

static void gser_cdc_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_gser		*gser = req->context;
	u8			doit = false;

	/* on this call path we do NOT hold the port spinlock,
	 * which is why ACM needs its own spinlock
	 */
	spin_lock(&gser->lock);
	if (req->status != -ESHUTDOWN)
		doit = gser->pending;
	gser->notify_req = req;
	spin_unlock(&gser->lock);

	if (doit)
		gser_notify_serial_state(gser);
}
#endif

/*-------------------------------------------------------------------------*/

/**
 * acm_cdc_notify - issue CDC notification to host
 * @acm: wraps host to be notified
 * @type: notification type
 * @value: Refer to cdc specs, wValue field.
 * @data: data to be sent
 * @length: size of data
 * Context: irqs blocked, acm->lock held, acm_notify_req non-null
 *
 * Returns zero on success or a negative errno.
 *
 * See section 6.3.5 of the CDC 1.1 specification for information
 * about the only notification we issue:  SerialState change.
 */
static int gser_notify(struct f_gser *gser, u8 type, u16 value,
		void *data, unsigned length)
{
	struct usb_ep			*ep = gser->notify;
	struct usb_request		*req;
	struct usb_cdc_notification	*notify;
	const unsigned			len = sizeof(*notify) + length;
	void				*buf;
	int				status;

	req = gser->notify_req;
	gser->notify_req = NULL;
	gser->pending = false;

	req->length = len;
	notify = req->buf;
	buf = notify + 1;

	notify->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	notify->bNotificationType = type;
	notify->wValue = cpu_to_le16(value);
	notify->wIndex = cpu_to_le16(gser->data_id);
	notify->wLength = cpu_to_le16(length);
	memcpy(buf, data, length);

	spin_unlock(&gser->lock);
	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	spin_lock(&gser->lock);
	if (status < 0) {
		pr_err( "gser ttyGS%d can't notify serial state, %d\n",
				gser->port_num, status);
		gser->notify_req = req;
	}

	return status;
}

static int gser_notify_serial_state(struct f_gser *gser)
{
	int			 status;

	spin_lock(&gser->lock);
	if (gser->notify_req) {
		pr_debug( "gser ttyGS%d serial state %04x\n",
				gser->port_num, gser->serial_state);
		status = gser_notify(gser, USB_CDC_NOTIFY_SERIAL_STATE,
				0, &gser->serial_state,
					sizeof(gser->serial_state));
	} else {
		gser->pending = true;
		status = 0;
	}
	spin_unlock(&gser->lock);
	return status;
}

static void gser_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_gser *gser = req->context;
	u8	      doit = false;

	/* on this call path we do NOT hold the port spinlock,
	 * which is why ACM needs its own spinlock
	 */
	spin_lock(&gser->lock);
	if (req->status != -ESHUTDOWN)
		doit = gser->pending;
	gser->notify_req = req;
	spin_unlock(&gser->lock);

	if (doit && gser->online)
		gser_notify_serial_state(gser);
}
static void gser_connect(struct gserial *port)
{
	struct f_gser *gser = port_to_gser(port);

	gser->serial_state |= ACM_CTRL_DSR | ACM_CTRL_DCD;
	gser_notify_serial_state(gser);
}

unsigned int gser_get_dtr(struct gserial *port)
{
	struct f_gser *gser = port_to_gser(port);

	if (gser->port_handshake_bits & ACM_CTRL_DTR)
		return 1;
	else
		return 0;
}

unsigned int gser_get_rts(struct gserial *port)
{
	struct f_gser *gser = port_to_gser(port);

	if (gser->port_handshake_bits & ACM_CTRL_RTS)
		return 1;
	else
		return 0;
}

unsigned int gser_send_carrier_detect(struct gserial *port, unsigned int yes)
{
	struct f_gser *gser = port_to_gser(port);
	u16			state;

	state = gser->serial_state;
	state &= ~ACM_CTRL_DCD;
	if (yes)
		state |= ACM_CTRL_DCD;

	gser->serial_state = state;
	return gser_notify_serial_state(gser);

}

unsigned int gser_send_ring_indicator(struct gserial *port, unsigned int yes)
{
	struct f_gser *gser = port_to_gser(port);
	u16			state;

	state = gser->serial_state;
	state &= ~ACM_CTRL_RI;
	if (yes)
		state |= ACM_CTRL_RI;

	gser->serial_state = state;
	return gser_notify_serial_state(gser);

}
static void gser_disconnect(struct gserial *port)
{
	struct f_gser *gser = port_to_gser(port);

	gser->serial_state &= ~(ACM_CTRL_DSR | ACM_CTRL_DCD);
	gser_notify_serial_state(gser);
}

static int gser_send_break(struct gserial *port, int duration)
{
	struct f_gser *gser = port_to_gser(port);
	u16			state;

	state = gser->serial_state;
	state &= ~ACM_CTRL_BRK;
	if (duration)
		state |= ACM_CTRL_BRK;

	gser->serial_state = state;
	return gser_notify_serial_state(gser);
}

static int gser_send_modem_ctrl_bits(struct gserial *port, int ctrl_bits)
{
	struct f_gser *gser = port_to_gser(port);

	pr_debug("usb:(hsic) %s: ctrl_bits=%d\n", __func__, ctrl_bits);

	gser->serial_state = ctrl_bits;

	return gser_notify_serial_state(gser);
}
#endif
/*-------------------------------------------------------------------------*/

/* serial function driver setup/binding */

static int
gser_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_gser		*gser = func_to_gser(f);
	int			status;
	struct usb_ep		*ep;

	printk("usb: %s\n", __func__);

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */
	if (gser_string_defs[0].id == 0) {
		status = usb_string_ids_tab(c->cdev, gser_string_defs);
		if (status < 0)
			return status;
		gser_control_interface_desc.iInterface =
			gser_string_defs[ACM_CTRL_IDX].id;
		gser_data_interface_desc.iInterface =
			gser_string_defs[ACM_DATA_IDX].id;
		gser_iad_descriptor.iFunction = gser_string_defs[ACM_IAD_IDX].id;
	}

	/* allocate instance-specific interface IDs */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	gser->ctrl_id = status;
	gser_iad_descriptor.bFirstInterface = status;

	gser_control_interface_desc.bInterfaceNumber = status;
	gser_union_desc .bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	gser->data_id = status;
	gser_data_interface_desc.bInterfaceNumber = status;
	gser_union_desc.bSlaveInterface0 = status;
	gser_call_mgmt_descriptor.bDataInterface = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &gser_fs_in_desc);
	if (!ep)
		goto fail;
	gser->port.in = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &gser_fs_out_desc);
	if (!ep)
		goto fail;
	gser->port.out = ep;
	ep->driver_data = cdev;	/* claim */

#if 1 //def CONFIG_USB_DUN_SUPPORT
	ep = usb_ep_autoconfig(cdev->gadget, &gser_fs_notify_desc);
	if (!ep)
		goto fail;
	gser->notify = ep;
	ep->driver_data = cdev;	/* claim */
	/* allocate notification */
	gser->notify_req = gs_alloc_req(ep,
			sizeof(struct usb_cdc_notification) + 2,
			GFP_KERNEL);
	if (!gser->notify_req)
		goto fail;

	gser->notify_req->complete = gser_notify_complete;
	gser->notify_req->context = gser;
#endif

	/* copy descriptors, and track endpoint copies */
	gser_hs_in_desc.bEndpointAddress = gser_fs_in_desc.bEndpointAddress;
	gser_hs_out_desc.bEndpointAddress = gser_fs_out_desc.bEndpointAddress;
	gser_hs_notify_desc.bEndpointAddress =
		gser_fs_notify_desc.bEndpointAddress;


	gser_ss_in_desc.bEndpointAddress = gser_fs_in_desc.bEndpointAddress;
	gser_ss_out_desc.bEndpointAddress = gser_fs_out_desc.bEndpointAddress;

	status = usb_assign_descriptors(f, gser_fs_function, gser_hs_function,
			gser_ss_function);
	if (status)
		goto fail;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		gser_hs_in_desc.bEndpointAddress =
				gser_fs_in_desc.bEndpointAddress;
		gser_hs_out_desc.bEndpointAddress =
				gser_fs_out_desc.bEndpointAddress;
#if 1 //def CONFIG_USB_DUN_SUPPORT
		gser_hs_notify_desc.bEndpointAddress =
				gser_fs_notify_desc.bEndpointAddress;
#endif

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(gser_hs_function);

		if (!f->hs_descriptors)
			goto fail;

#if 0
		gser->hs.in = usb_find_endpoint(gser_hs_function,
				f->hs_descriptors, &gser_hs_in_desc);
		gser->hs.out = usb_find_endpoint(gser_hs_function,
				f->hs_descriptors, &gser_hs_out_desc);
#if 1 //def CONFIG_USB_DUN_SUPPORT
		gser->hs.notify = usb_find_endpoint(gser_hs_function,
				f->hs_descriptors, &gser_hs_notify_desc);
#endif
#endif
	}

	pr_info("generic ttyGS%d: %s speed IN/%s OUT/%s\n",
			gser->port_num,
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			gser->port.in->name, gser->port.out->name);
	return 0;

fail:
#if 1 //def CONFIG_USB_DUN_SUPPORT
	if (gser->notify_req)
		gs_free_req(gser->notify, gser->notify_req);

	/* we might as well release our claims on endpoints */
	if (gser->notify)
		gser->notify->driver_data = NULL;
#endif
	/* we might as well release our claims on endpoints */
	if (gser->port.out)
		gser->port.out->driver_data = NULL;
	if (gser->port.in)
		gser->port.in->driver_data = NULL;

	pr_err("%s: can't bind, err %d\n", f->name, status);

	return status;
}

static void
gser_unbind(struct usb_configuration *c, struct usb_function *f)
{
#if 1 //def CONFIG_USB_DUN_SUPPORT
	struct f_gser *gser = func_to_gser(f);
#endif
	usb_free_all_descriptors(f);
#if 1 //def CONFIG_USB_DUN_SUPPORT
	if (gser->notify_req)
	gs_free_req(gser->notify, gser->notify_req);
#endif
	//kfree(gser->port.func.name);
	kfree(gser);
}


/**
 * gser_bind_config - add a generic serial function to a configuration
 * @c: the configuration to support the serial instance
 * @port_num: /dev/ttyGS* port this interface will use
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 *
 * Caller must have called @gserial_setup() with enough ports to
 * handle all the ones it binds.  Caller is also responsible
 * for calling @gserial_cleanup() before module unload.
 */
int gser_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct f_gser	*gser;
	int		status;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string ID */
	if (gser_string_defs[0].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		gser_string_defs[0].id = status;
	}

	/* allocate and initialize one new instance */
	gser = kzalloc(sizeof *gser, GFP_KERNEL);
	if (!gser)
		return -ENOMEM;

#if 1 //def CONFIG_USB_DUN_SUPPORT
	spin_lock_init(&gser->lock);
#endif
	gser->port_num = port_num;

	gser->port.func.name = "gser";
	gser->port.func.strings = gser_strings;
	gser->port.func.bind = gser_bind;
	gser->port.func.unbind = gser_unbind;
	gser->port.func.set_alt = gser_set_alt;
	gser->port.func.disable = gser_disable;
	gser->transport		= gserial_ports[port_num].transport;
#if 1 //def CONFIG_USB_DUN_SUPPORT
	/* We support only two ports for now */
	if (port_num == 0)
		gser->port.func.name = "modem";
	else
		gser->port.func.name = "nmea";
	gser->port.func.setup = gser_setup;
	gser->port.connect = gser_connect;
	gser->port.send_modem_ctrl_bits = gser_send_modem_ctrl_bits;
	gser->port.disconnect = gser_disconnect;
	gser->port.send_break = gser_send_break;
#endif

	status = usb_add_function(c, &gser->port.func);
	if (status)
		kfree(gser);
	return status;
}

/**
 * gserial_init_port - bind a gserial_port to its transport
 */
static int gserial_init_port(int port_num, const char *name)
{
	enum transport_type transport;

	if (port_num >= GSERIAL_NO_PORTS)
	{
		pr_info("usb:(hsic) gserial_init_port - No port \r\n");
		return -ENODEV;
	}

	transport = str_to_xport(name);
	pr_debug("usb:(hsic) %s, port:%d, transport:%s\n", __func__,
			port_num, xport_to_str(transport));

	gserial_ports[port_num].transport = transport;
	gserial_ports[port_num].port_num = port_num;

	switch (transport) {
	case USB_GADGET_XPORT_TTY:
		gserial_ports[port_num].client_port_num = no_tty_ports;
		no_tty_ports++;
		break;
	case USB_GADGET_XPORT_HSIC:
		ghsic_ctrl_set_port_name("serial_hsic", name);
		ghsic_data_set_port_name("serial_hsic", name);

		/*client port number will be updated in gport_setup*/
		no_hsic_sports++;
		break;
	default:
		pr_info("%s: Un-supported transport transport: %u\n",
				__func__, gserial_ports[port_num].transport);
		return -ENODEV;
	}

	nr_ports++;
	pr_info("usb:(hsic) gserial_init_port return : nr_ports=%d / no_hsic_sports=%d \r\n",nr_ports,no_hsic_sports);
	return 0;
}
