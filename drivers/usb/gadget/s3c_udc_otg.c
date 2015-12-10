/*
 * drivers/usb/gadget/s3c_udc_otg.c
 * Samsung S3C on-chip full/high speed USB OTG 2.0 device controllers
 *
 * Copyright (C) 2008 for Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/platform_data/usb-otg-samsung.h>
#include <plat/cpu.h>
#include <linux/usb/phy.h>
#include <linux/usb/samsung_usb_phy.h>
#include <linux/usb/samsung_otg_regs.h>

#include "s3c_udc.h"

#undef DEBUG_S3C_UDC_SETUP
#undef DEBUG_S3C_UDC_EP0
#undef DEBUG_S3C_UDC_ISR
#undef DEBUG_S3C_UDC_OUT_EP
#undef DEBUG_S3C_UDC_IN_EP
#undef DEBUG_S3C_UDC

#define EP0_CON		0
#define EP1_OUT		1
#define EP2_IN		2
#define EP3_IN		3
#define EP_MASK		0xF

#define POLL_TIMEOUT	1000

#define BACK2BACK_SIZE	4

#if defined(DEBUG_S3C_UDC_SETUP) || defined(DEBUG_S3C_UDC_ISR)\
	|| defined(DEBUG_S3C_UDC_OUT_EP)

static char *state_names[] = {
	"WAIT_FOR_SETUP",
	"DATA_STATE_XMIT",
	"DATA_STATE_NEED_ZLP",
	"WAIT_FOR_OUT_STATUS",
	"DATA_STATE_RECV",
	};
#endif

#ifdef DEBUG_S3C_UDC_SETUP
#define DEBUG_SETUP(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_SETUP(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC_EP0
#define DEBUG_EP0(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_EP0(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC
#define DEBUG(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC_ISR
#define DEBUG_ISR(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_ISR(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC_OUT_EP
#define DEBUG_OUT_EP(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_OUT_EP(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_S3C_UDC_IN_EP
#define DEBUG_IN_EP(fmt, args...) printk(fmt, ##args)
#else
#define DEBUG_IN_EP(fmt, args...) do {} while (0)
#endif

#define	DRIVER_DESC	"S3C HS USB OTG Device Driver,"\
				"(c) 2008-2009 Samsung Electronics"
#define	DRIVER_VERSION	"15 March 2009"

struct s3c_udc	*the_controller;

static const char driver_name[] = "s3c-hsotg";
static const char driver_desc[] = DRIVER_DESC;
static const char ep0name[] = "ep0-control";

/* Max packet size*/
static unsigned int ep0_fifo_size = 64;
static unsigned int ep_fifo_size =  512;
static unsigned int ep_fifo_size2 = 1024;
static int reset_available = 1;
static int pullup_state;

/*
  Local declarations.
*/
static int s3c_ep_enable(struct usb_ep *ep,
				const struct usb_endpoint_descriptor *);
static int s3c_ep_disable(struct usb_ep *ep);
static struct usb_request *s3c_alloc_request(struct usb_ep *ep,
							gfp_t gfp_flags);
static void s3c_free_request(struct usb_ep *ep, struct usb_request *);

static int s3c_queue(struct usb_ep *ep, struct usb_request *, gfp_t gfp_flags);
static int s3c_dequeue(struct usb_ep *ep, struct usb_request *);
static int s3c_fifo_status(struct usb_ep *ep);
static void s3c_fifo_flush(struct usb_ep *ep);
static void s3c_ep0_read(struct s3c_udc *dev);
static void s3c_ep0_kick(struct s3c_udc *dev, struct s3c_ep *ep);
static void s3c_handle_ep0(struct s3c_udc *dev);
static int s3c_ep0_write(struct s3c_udc *dev);
static int write_fifo_ep0(struct s3c_ep *ep, struct s3c_request *req);
static void done(struct s3c_ep *ep,
			struct s3c_request *req, int status);
static void stop_activity(struct s3c_udc *dev,
			struct usb_gadget_driver *driver);
static int udc_enable(struct s3c_udc *dev);
static void udc_disable(struct s3c_udc *dev);
static void udc_set_address(struct s3c_udc *dev, unsigned char address);
static void reset_usbd(void);
static void reconfig_usbd(void);
static void set_max_pktsize(struct s3c_udc *dev, enum usb_device_speed speed);
static void nuke(struct s3c_ep *ep, int status);
static int s3c_udc_set_halt(struct usb_ep *_ep, int value);
static void s3c_udc_soft_connect(void);
static void s3c_udc_soft_disconnect(void);

static struct usb_ep_ops s3c_ep_ops = {
	.enable = s3c_ep_enable,
	.disable = s3c_ep_disable,

	.alloc_request = s3c_alloc_request,
	.free_request = s3c_free_request,

	.queue = s3c_queue,
	.dequeue = s3c_dequeue,

	.set_halt = s3c_udc_set_halt,
	.fifo_status = s3c_fifo_status,
	.fifo_flush = s3c_fifo_flush,
};

#ifdef CONFIG_USB_GADGET_DEBUG_FILES

static const char proc_node_name[] = "driver/udc";

static int
udc_proc_read(char *page, char **start, off_t off, int count,
	      int *eof, void *_dev)
{
	char *buf = page;
	struct s3c_udc *dev = _dev;
	char *next = buf;
	unsigned size = count;
	unsigned long flags;
	int t;

	if (off != 0)
		return 0;

	local_irq_save(flags);

	/* basic device status */
	t = scnprintf(next, size,
		      DRIVER_DESC "\n"
		      "%s version: %s\n"
		      "Gadget driver: %s\n"
		      "\n",
		      driver_name, DRIVER_VERSION,
		      dev->driver ? dev->driver->driver.name : "(none)");
	size -= t;
	next += t;

	local_irq_restore(flags);
	*eof = 1;
	return count - size;
}

#define create_proc_files() \
	create_proc_read_entry(proc_node_name, 0, NULL, udc_proc_read, dev)
#define remove_proc_files() \
	remove_proc_entry(proc_node_name, NULL)

#else	/* !CONFIG_USB_GADGET_DEBUG_FILES */

#define create_proc_files() do {} while (0)
#define remove_proc_files() do {} while (0)

#endif	/* CONFIG_USB_GADGET_DEBUG_FILES */

#include "s3c_udc_otg_xfer_dma.c"

/*
 * is_nonswitch - whether or not switch driver.
 *
 * Return true if switch driver isn't.
 */
static inline bool is_nonswitch(void)
{
#if defined(CONFIG_USB_GADGET_SWITCH) || defined(CONFIG_USB_EXYNOS_SWITCH)
	return false;
#else
	return true;
#endif
}

/*
 *	udc_disable - disable USB device controller
 */
static void udc_disable(struct s3c_udc *dev)
{
	u32 utemp;
	DEBUG_SETUP("%s: %p\n", __func__, dev);

	disable_irq(dev->irq);
	udc_set_address(dev, 0);

	dev->ep0state = WAIT_FOR_SETUP;
	dev->gadget.speed = USB_SPEED_UNKNOWN;
	dev->usb_address = 0;

	/* Mask the core interrupt */
	__raw_writel(0, dev->regs + S3C_UDC_OTG_GINTMSK);

	/* Put the OTG device core in the disconnected state.*/
	utemp = __raw_readl(dev->regs + S3C_UDC_OTG_DCTL);
	utemp |= SOFT_DISCONNECT;
	__raw_writel(utemp, dev->regs + S3C_UDC_OTG_DCTL);
	udelay(20);

	usb_phy_shutdown(dev->phy);

	clk_disable_unprepare(dev->clk);
	clk_disable_unprepare(dev->otg_aclk);
	clk_disable_unprepare(dev->otg_hclk);
	clk_disable_unprepare(dev->upsizer_otg);
	clk_disable_unprepare(dev->xiu_d_fsys1);
	clk_disable_unprepare(dev->upsizer_fsys1);
#if defined(CONFIG_FAKE_BATTERY_SAMSUNG)
	s3c_udc_cable_disconnect(dev);
#endif
}

/*
 *	udc_reinit - initialize software state
 */
static void udc_reinit(struct s3c_udc *dev)
{
	unsigned int i;

	DEBUG_SETUP("%s: %p\n", __func__, dev);

	/* device/ep0 records init */
	INIT_LIST_HEAD(&dev->gadget.ep_list);
	INIT_LIST_HEAD(&dev->gadget.ep0->ep_list);
	dev->ep0state = WAIT_FOR_SETUP;

	/* basic endpoint records init */
	for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
		struct s3c_ep *ep = &dev->ep[i];

		if (i != 0)
			list_add_tail(&ep->ep.ep_list, &dev->gadget.ep_list);

		ep->desc = 0;
		ep->stopped = 0;
		INIT_LIST_HEAD(&ep->queue);
		ep->pio_irqs = 0;
	}

	/* the rest was statically initialized, and is read-only */
}

#define BYTES2MAXP(x)	(x / 8)
#define MAXP2BYTES(x)	(x * 8)

/* until it's enabled, this UDC should be completely invisible
 * to any USB host.
 */
static int udc_enable(struct s3c_udc *dev)
{
	struct platform_device *pdev = dev->dev;
	int i, err[6];
	DEBUG_SETUP("%s: %p\n", __func__, dev);

	enable_irq(dev->irq);
	err[0] = clk_prepare_enable(dev->otg_aclk);
	err[1] = clk_prepare_enable(dev->otg_hclk);
	err[2] = clk_prepare_enable(dev->upsizer_otg);
	err[3] = clk_prepare_enable(dev->xiu_d_fsys1);
	err[4] = clk_prepare_enable(dev->upsizer_fsys1);
	err[5] = clk_prepare_enable(dev->upsizer_ahb_usbhs);
	for(i = 0; i < 5; i++) {
		if (err[i])
			dev_err(&pdev->dev, "Fail to get clock on %d th \n", i);
	}
	usb_phy_init(dev->phy);

	reconfig_usbd();

	DEBUG_SETUP("S3C USB 2.0 OTG Controller Core Initialized : 0x%x\n",
			__raw_readl(dev->regs + S3C_UDC_OTG_GINTMSK));

	dev->gadget.speed = USB_SPEED_UNKNOWN;

	return 0;
}

struct usb_gadget* s3c_udc_get_gadget(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct s3c_udc *udc = platform_get_drvdata(pdev);

	if (!udc)
		return NULL;

	return &udc->gadget;
}

int s3c_vbus_enable(struct usb_gadget *gadget, int is_active)
{
	unsigned long flags;
	struct s3c_udc *dev = container_of(gadget, struct s3c_udc, gadget);

	if (dev->udc_enabled != is_active) {
		dev->udc_enabled = is_active;

		if (!is_active) {
			spin_lock_irqsave(&dev->lock, flags);
			stop_activity(dev, dev->driver);
			spin_unlock_irqrestore(&dev->lock, flags);
			udc_disable(dev);
		} else {
			udc_reinit(dev);
			udc_enable(dev);
			if (pullup_state)
				s3c_udc_soft_connect();
		}
	} else {
		printk(KERN_INFO "usb: %s, udc_enabled : %d, is_active : %d\n",
				__func__, dev->udc_enabled, is_active);
	}

	return 0;
}

/*
  Register entry point for the peripheral controller driver.
*/
static int s3c_udc_start(struct usb_gadget *gadget,
			 struct usb_gadget_driver *driver)
{
	struct s3c_udc *dev = the_controller;

	DEBUG_SETUP("%s: %s\n", __func__, driver->driver.name);

	if (!driver
		|| driver->max_speed < USB_SPEED_FULL
		|| !driver->disconnect || !driver->setup)
		return -EINVAL;
	if (!dev)
		return -ENODEV;
	if (dev->driver)
		return -EBUSY;

	/* first hook up the driver ... */
	dev->driver = driver;
	dev->gadget.dev.driver = &driver->driver;

	printk(KERN_INFO "bound driver '%s'\n",
			driver->driver.name);
	if (is_nonswitch()) {
		udc_enable(dev);
		dev->udc_enabled = 1;
	} else {
		printk(KERN_INFO "usb: Skip udc_enable\n");
	}
	return 0;
}

/*
  Unregister entry point for the peripheral controller driver.
*/
static int s3c_udc_stop(struct usb_gadget *gadget,
			struct usb_gadget_driver *driver)
{
	struct s3c_udc *dev = the_controller;
	unsigned long flags;

	if (!dev)
		return -ENODEV;
	if (!driver || driver != dev->driver)
		return -EINVAL;

	spin_lock_irqsave(&dev->lock, flags);
	dev->driver = NULL;
	stop_activity(dev, driver);
	spin_unlock_irqrestore(&dev->lock, flags);

	printk(KERN_INFO "Unregistered gadget driver '%s'\n",
			driver->driver.name);

	return 0;
}

static int poll_bit_set(void __iomem *ptr, u32 val, int timeout)
{
	u32 reg;

	do {
		reg = __raw_readl(ptr);
		if (reg & val)
			return 0;

		udelay(1);
	} while (timeout-- > 0);

	return -ETIME;
}

static int poll_bit_clear(void __iomem *ptr, u32 val, int timeout)
{
	u32 reg;

	do {
		reg = __raw_readl(ptr);
		if (!(reg & val))
			return 0;

		udelay(1);
	} while (timeout-- > 0);

	return -ETIME;
}

/*
 *	done - retire a request; caller blocked irqs
 */
static void done(struct s3c_ep *ep, struct s3c_request *req, int status)
{
	unsigned int stopped = ep->stopped;
	struct device *dev = &the_controller->dev->dev;
	u32 ep_num = ep_index(ep);
	struct s3c_udc *udc = ep->dev;
	u32 ctrl, ret;

	DEBUG("%s: %s %p, req = %p, stopped = %d\n",
		__func__, ep->ep.name, ep, &req->req, stopped);

	list_del_init(&req->queue);

	if (likely(req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	if (req->mapped) {
		if (ep_is_in(ep)) {
			ctrl = __raw_readl(udc->regs + S3C_UDC_OTG_DIEPCTL(ep_num));
			if (ctrl & DEPCTL_EPENA) {
				__raw_writel(DEPCTL_EPDIS | DEPCTL_SNAK | ctrl,
					udc->regs + S3C_UDC_OTG_DIEPCTL(ep_num));
				ret = poll_bit_clear(udc->regs + S3C_UDC_OTG_DIEPCTL(ep_num),
						DEPCTL_EPENA | DEPCTL_EPDIS, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "faild Endpoint disable and Set NAK\n");
					goto out;
				}

				__raw_writel(TxFIFONum(ep_num) | TxFFlush, udc->regs + S3C_UDC_OTG_GRSTCTL);
				ret = poll_bit_clear(udc->regs + S3C_UDC_OTG_GRSTCTL,
						TxFFlush, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "faild TX FIFO Flush\n");
					goto out;
				}
			}
		} else {
			ctrl =  __raw_readl(udc->regs + S3C_UDC_OTG_DOEPCTL(ep_num));
			if (ctrl & DEPCTL_EPENA) {
				__raw_writel(__raw_readl(udc->regs + S3C_UDC_OTG_DCTL) | SGOUTNak,
						udc->regs + S3C_UDC_OTG_DCTL);
				ret = poll_bit_set(udc->regs + S3C_UDC_OTG_GINTSTS,
						INT_GOUTNakEff, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "failed Set Global OUT NAK\n");
					goto out;
				}

				__raw_writel(DEPCTL_EPDIS | DEPCTL_SNAK | ctrl,
					udc->regs + S3C_UDC_OTG_DOEPCTL(ep_num));
				ret = poll_bit_clear(udc->regs + S3C_UDC_OTG_DOEPCTL(ep_num),
						DEPCTL_EPENA | DEPCTL_EPDIS, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "faild Endpoint disable and Set NAK\n");
					goto out;
				}

				__raw_writel(__raw_readl(udc->regs + S3C_UDC_OTG_DCTL) | CGOUTNak,
						udc->regs + S3C_UDC_OTG_DCTL);
				ret = poll_bit_clear(udc->regs + S3C_UDC_OTG_GINTSTS,
						INT_GOUTNakEff, POLL_TIMEOUT);
				if (ret) {
					dev_err(dev, "failed Clear Global OUT NAK\n");
					goto out;
				}
			}
		}
out:
		dma_unmap_single(dev, req->req.dma, req->req.length,
			(ep->bEndpointAddress & USB_DIR_IN) ?
				DMA_TO_DEVICE : DMA_FROM_DEVICE);
		req->req.dma = DMA_ADDR_INVALID;
		req->mapped = 0;
		aligned_unmap_buf(req, ep_is_in(ep));
	}

	if (status && status != -ESHUTDOWN) {
		DEBUG("complete %s req %p stat %d len %u/%u\n",
			ep->ep.name, &req->req, status,
			req->req.actual, req->req.length);
	}

	/* don't modify queue heads during completion callback */
	ep->stopped = 1;

	spin_unlock(&ep->dev->lock);
	req->req.complete(&ep->ep, &req->req);
	spin_lock(&ep->dev->lock);

	ep->stopped = stopped;
}

/*
 *	nuke - dequeue ALL requests
 */
static void nuke(struct s3c_ep *ep, int status)
{
	struct s3c_request *req;

	DEBUG("%s: %s %p\n", __func__, ep->ep.name, ep);

	/* called with irqs blocked */
	while (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct s3c_request, queue);
		done(ep, req, status);
	}
}

static void stop_activity(struct s3c_udc *dev,
			  struct usb_gadget_driver *driver)
{
	int i;

	/* don't disconnect drivers more than once */
	if (dev->gadget.speed == USB_SPEED_UNKNOWN)
		driver = 0;
	dev->gadget.speed = USB_SPEED_UNKNOWN;

	/* prevent new request submissions, kill any outstanding requests  */
	for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
		struct s3c_ep *ep = &dev->ep[i];
		ep->stopped = 1;
		nuke(ep, -ESHUTDOWN);
	}

	/* report disconnect; the driver is already quiesced */
	if (driver) {
		spin_unlock(&dev->lock);
		driver->disconnect(&dev->gadget);
		spin_lock(&dev->lock);
	}

	/* re-init driver-visible data structures */
	udc_reinit(dev);
}

static void reset_usbd(void)
{
	struct s3c_udc *dev = the_controller;
	unsigned int utemp;
#ifdef DED_TX_FIFO
	int i;

	for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
		utemp = __raw_readl(dev->regs + S3C_UDC_OTG_DIEPCTL(i));

		if (utemp & DEPCTL_EPENA)
			__raw_writel(utemp|DEPCTL_EPDIS|DEPCTL_SNAK,
					dev->regs + S3C_UDC_OTG_DIEPCTL(i));

		/* OUT EP0 cannot be disabled */
		if (i != 0) {
			utemp = __raw_readl(dev->regs + S3C_UDC_OTG_DOEPCTL(i));

			if (utemp & DEPCTL_EPENA)
				__raw_writel(utemp|DEPCTL_EPDIS|DEPCTL_SNAK,
				dev->regs + S3C_UDC_OTG_DOEPCTL(i));
		}
	}
#endif

	/* 5. Configure OTG Core to initial settings of device mode.*/
	__raw_writel(1<<18|0x0<<0,
			dev->regs + S3C_UDC_OTG_DCFG);

	mdelay(1);

	/* 6. Unmask the core interrupts*/
	__raw_writel(GINTMSK_INIT, dev->regs + S3C_UDC_OTG_GINTMSK);

	/* 7. Set NAK bit of OUT EP0*/
	__raw_writel(DEPCTL_SNAK,
			dev->regs + S3C_UDC_OTG_DOEPCTL(EP0_CON));

	/* 8. Unmask EPO interrupts*/
	__raw_writel(((1<<EP0_CON)<<DAINT_OUT_BIT)|(1<<EP0_CON),
			dev->regs + S3C_UDC_OTG_DAINTMSK);

	/* 9. Unmask device OUT EP common interrupts*/
	__raw_writel(DOEPMSK_INIT, dev->regs + S3C_UDC_OTG_DOEPMSK);

	/* 10. Unmask device IN EP common interrupts*/
	__raw_writel(DIEPMSK_INIT, dev->regs + S3C_UDC_OTG_DIEPMSK);

	/* 11. Set Rx FIFO Size (in 32-bit words) */
	__raw_writel(RX_FIFO_SIZE, dev->regs + S3C_UDC_OTG_GRXFSIZ);

	/* 12. Set Non Periodic Tx FIFO Size*/
	__raw_writel((NPTX_FIFO_SIZE) << 16 | (NPTX_FIFO_START_ADDR) << 0,
		dev->regs + S3C_UDC_OTG_GNPTXFSIZ);

#ifdef DED_TX_FIFO
	for (i = 1; i < S3C_MAX_ENDPOINTS; i++)
		__raw_writel((PTX_FIFO_SIZE) << 16 |
				(NPTX_FIFO_START_ADDR + NPTX_FIFO_SIZE
				  + PTX_FIFO_SIZE*(i-1)) << 0,
				dev->regs + S3C_UDC_OTG_DIEPTXF(i));
#endif

	/* Flush the RX FIFO */
	__raw_writel(0x10, dev->regs + S3C_UDC_OTG_GRSTCTL);
	while (__raw_readl(dev->regs + S3C_UDC_OTG_GRSTCTL) & 0x10)
		;

	/* Flush all the Tx FIFO's */
	__raw_writel(0x10<<6, dev->regs + S3C_UDC_OTG_GRSTCTL);
	__raw_writel((0x10<<6)|0x20, dev->regs + S3C_UDC_OTG_GRSTCTL);
	while (__raw_readl(dev->regs + S3C_UDC_OTG_GRSTCTL) & 0x20)
		;

	/* 13. Clear NAK bit of OUT EP0*/
	/* For Slave mode*/
	__raw_writel(DEPCTL_CNAK,
		dev->regs + S3C_UDC_OTG_DOEPCTL(EP0_CON));

	/* 14. Initialize OTG Link Core.*/
	__raw_writel(GAHBCFG_INIT, dev->regs + S3C_UDC_OTG_GAHBCFG);
}

static void reconfig_usbd(void)
{
	struct s3c_udc *dev = the_controller;
	unsigned int utemp;

	/* 2. Soft-reset OTG Core and then unreset again. */
	__raw_writel(CORE_SOFT_RESET, dev->regs + S3C_UDC_OTG_GRSTCTL);

	utemp = (0<<15		/* PHY Low Power Clock sel */
		|1<<14		/* Non-Periodic TxFIFO Rewind Enable */
		|0<<9		/* [0:HNP disable, 1:HNP enable] */
		|0<<8		/* [ 0:SRP disable, 1:SRP enable] */
		|0<<7		/* Ulpi DDR sel */
		|0<<6		/* 0: high speed utmi+, 1: full speed serial */
		|0<<4		/* 0: utmi+, 1:ulpi */
		|0x7<<0);	/* HS/FS Timeout */
	/* [13:10] Turnaround time 0x5:16-bit 0x9:8-bit UTMI+ */
	/* [3] phy i/f 0:8bit, 1:16bit */
	if (soc_is_exynos4210())
		utemp |= (0x5<<10 | 1<<3);
	else
		utemp |= (0x9<<10 | 0<<3);
	__raw_writel(utemp, dev->regs + S3C_UDC_OTG_GUSBCFG);

	/* 3. Put the OTG device core in the disconnected state.*/
	utemp = __raw_readl(dev->regs + S3C_UDC_OTG_DCTL);
	utemp |= SOFT_DISCONNECT;
	__raw_writel(utemp, dev->regs + S3C_UDC_OTG_DCTL);

	reset_usbd();
}

static void set_max_pktsize(struct s3c_udc *dev, enum usb_device_speed speed)
{
	unsigned int ep_ctrl;
	int i;

	if (speed == USB_SPEED_HIGH) {
		ep0_fifo_size = 64;
		ep_fifo_size = 512;
		ep_fifo_size2 = 1024;
		dev->gadget.speed = USB_SPEED_HIGH;
	} else {
		ep0_fifo_size = 64;
		ep_fifo_size = 64;
		ep_fifo_size2 = 1023;
		dev->gadget.speed = USB_SPEED_FULL;
	}

	dev->ep[0].ep.maxpacket = ep0_fifo_size;
	for (i = 1; i < S3C_MAX_ENDPOINTS; i++) {
		/* fullspeed limitations don't apply to isochronous endpoints */
		if (dev->ep[i].bmAttributes == USB_ENDPOINT_XFER_ISOC)
			dev->ep[i].ep.maxpacket = ep_fifo_size2;
		else
			dev->ep[i].ep.maxpacket = ep_fifo_size;
	}

	/* EP0 - Control IN (64 bytes)*/
	ep_ctrl = __raw_readl(dev->regs + S3C_UDC_OTG_DIEPCTL(EP0_CON));
	__raw_writel(ep_ctrl|(0<<0), dev->regs + S3C_UDC_OTG_DIEPCTL(EP0_CON));

	/* EP0 - Control OUT (64 bytes)*/
	ep_ctrl = __raw_readl(dev->regs + S3C_UDC_OTG_DOEPCTL(EP0_CON));
	__raw_writel(ep_ctrl|(0<<0), dev->regs + S3C_UDC_OTG_DOEPCTL(EP0_CON));
}

static int s3c_ep_enable(struct usb_ep *_ep,
			     const struct usb_endpoint_descriptor *desc)
{
	struct s3c_ep *ep;
	struct s3c_udc *dev;
	unsigned long flags;

	DEBUG("%s: %p\n", __func__, _ep);
	ep = container_of(_ep, struct s3c_ep, ep);
	if (!_ep || !desc || ep->desc || _ep->name == ep0name
	    || desc->bDescriptorType != USB_DT_ENDPOINT
	    || ep->bEndpointAddress != desc->bEndpointAddress
	    || ep_maxpacket(ep) < le16_to_cpu(desc->wMaxPacketSize)) {

		DEBUG("%s: bad ep or descriptor\n", __func__);
		return -EINVAL;
	}

	/* xfer types must match, except that interrupt ~= bulk */
	if (ep->bmAttributes !=
			(desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
	    && ep->bmAttributes != USB_ENDPOINT_XFER_BULK
	    && desc->bmAttributes != USB_ENDPOINT_XFER_INT) {

		DEBUG("%s: %s type mismatch\n", __func__, _ep->name);
		return -EINVAL;
	}

	/* hardware _could_ do smaller, but driver doesn't */
	if ((desc->bmAttributes == USB_ENDPOINT_XFER_BULK
	     && le16_to_cpu(desc->wMaxPacketSize) != ep_maxpacket(ep))
	    || !desc->wMaxPacketSize) {

		DEBUG("%s: bad %s maxpacket\n", __func__, _ep->name);
		return -ERANGE;
	}

	dev = ep->dev;
	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN) {

		DEBUG("%s: bogus device state\n", __func__);
		return -ESHUTDOWN;
	}

	ep->stopped = 0;
	ep->desc = desc;
	ep->pio_irqs = 0;
	ep->ep.maxpacket = le16_to_cpu(desc->wMaxPacketSize);

	/* Reset halt state */
	s3c_udc_set_halt(_ep, 0);

	spin_lock_irqsave(&ep->dev->lock, flags);
	s3c_udc_ep_activate(ep);
	spin_unlock_irqrestore(&ep->dev->lock, flags);

	DEBUG("%s: enabled %s, stopped = %d, maxpacket = %d\n",
		__func__, _ep->name, ep->stopped, ep->ep.maxpacket);
	return 0;
}

/** Disable EP
 */
static int s3c_ep_disable(struct usb_ep *_ep)
{
	struct s3c_ep *ep;
	unsigned long flags;

	DEBUG("%s: %p\n", __func__, _ep);

	ep = container_of(_ep, struct s3c_ep, ep);
	if (!_ep || !ep->desc) {
		DEBUG("%s: %s not enabled\n", __func__,
		      _ep ? ep->ep.name : NULL);
		return -EINVAL;
	}

	spin_lock_irqsave(&ep->dev->lock, flags);

	/* Nuke all pending requests */
	nuke(ep, -ESHUTDOWN);

	ep->desc = 0;
	ep->stopped = 1;

	spin_unlock_irqrestore(&ep->dev->lock, flags);

	DEBUG("%s: disabled %s\n", __func__, _ep->name);
	return 0;
}

static struct usb_request *s3c_alloc_request(struct usb_ep *ep,
						 gfp_t gfp_flags)
{
	struct s3c_request *req;

	DEBUG("%s: %s %p\n", __func__, ep->name, ep);

	req = kmalloc(sizeof *req, gfp_flags);
	if (!req)
		return 0;

	memset(req, 0, sizeof *req);
	INIT_LIST_HEAD(&req->queue);
	return &req->req;
}

static void s3c_free_request(struct usb_ep *ep, struct usb_request *_req)
{
	struct s3c_request *req;

	DEBUG("%s: %p\n", __func__, ep);

	req = container_of(_req, struct s3c_request, req);
	WARN_ON(!list_empty(&req->queue));
	kfree(req);
}

/* dequeue JUST ONE request */
static int s3c_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct s3c_ep *ep;
	struct s3c_request *req;
	unsigned long flags;

	DEBUG("%s: %p\n", __func__, _ep);

	ep = container_of(_ep, struct s3c_ep, ep);
	if (!_ep || ep->ep.name == ep0name)
		return -EINVAL;

	spin_lock_irqsave(&ep->dev->lock, flags);

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req)
			break;
	}
	if (&req->req != _req) {
		spin_unlock_irqrestore(&ep->dev->lock, flags);
		return -EINVAL;
	}

	done(ep, req, -ECONNRESET);

	spin_unlock_irqrestore(&ep->dev->lock, flags);
	return 0;
}

/** Return bytes in EP FIFO
 */
static int s3c_fifo_status(struct usb_ep *_ep)
{
	int count = 0;
	struct s3c_ep *ep;

	ep = container_of(_ep, struct s3c_ep, ep);
	if (!_ep) {
		DEBUG("%s: bad ep\n", __func__);
		return -ENODEV;
	}

	DEBUG("%s: %d\n", __func__, ep_index(ep));

	/* LPD can't report unclaimed bytes from IN fifos */
	if (ep_is_in(ep))
		return -EOPNOTSUPP;

	return count;
}

/** Flush EP FIFO
 */
static void s3c_fifo_flush(struct usb_ep *_ep)
{
	struct s3c_ep *ep;

	ep = container_of(_ep, struct s3c_ep, ep);
	if (unlikely(!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		DEBUG("%s: bad ep\n", __func__);
		return;
	}

	DEBUG("%s: %d\n", __func__, ep_index(ep));
}

/* ---------------------------------------------------------------------------
 *	device-scoped parts of the api to the usb controller hardware
 * ---------------------------------------------------------------------------
 */

static int s3c_udc_get_frame(struct usb_gadget *_gadget)
{
	struct s3c_udc *dev = container_of(_gadget, struct s3c_udc, gadget);
	/*fram count number [21:8]*/
	unsigned int frame = __raw_readl(dev->regs + S3C_UDC_OTG_DSTS);

	return frame & 0x3ff00;
}

static int s3c_udc_wakeup(struct usb_gadget *_gadget)
{
	return -ENOTSUPP;
}

static int s3c_set_selfpowered(struct usb_gadget *_gadget, int is_on)
{
	struct s3c_udc *dev = container_of(_gadget, struct s3c_udc, gadget);
	unsigned long	flags;

	spin_lock_irqsave(&dev->lock, flags);
	dev->selfpowered = (is_on != 0);
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}

static void s3c_udc_soft_connect(void)
{
	struct s3c_udc *dev = the_controller;
	u32 uTemp;

	uTemp = __raw_readl(dev->regs + S3C_UDC_OTG_DCTL);
	uTemp = uTemp & ~SOFT_DISCONNECT;
	__raw_writel(uTemp, dev->regs + S3C_UDC_OTG_DCTL);
}

static void s3c_udc_soft_disconnect(void)
{
	struct s3c_udc *dev = the_controller;
	u32 uTemp;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	stop_activity(dev, dev->driver);
	spin_unlock_irqrestore(&dev->lock, flags);

	uTemp = __raw_readl(dev->regs + S3C_UDC_OTG_DCTL);
	uTemp |= SOFT_DISCONNECT;
	__raw_writel(uTemp, dev->regs + S3C_UDC_OTG_DCTL);
}

static int s3c_udc_pullup(struct usb_gadget *gadget, int is_on)
{
	if (is_on)
		s3c_udc_soft_connect();
	else
		s3c_udc_soft_disconnect();

	pullup_state = is_on;
	return 0;
}

static const struct usb_gadget_ops s3c_udc_ops = {
	.get_frame = s3c_udc_get_frame,
	.wakeup = s3c_udc_wakeup,
	.set_selfpowered = s3c_set_selfpowered,
	.pullup = s3c_udc_pullup,
	.vbus_session = s3c_vbus_enable,
	.udc_start = s3c_udc_start,
	.udc_stop = s3c_udc_stop,
};

static void nop_release(struct device *dev)
{
	DEBUG("%s %s\n", __func__, dev->bus_id);
}

static struct s3c_udc memory = {
	.selfpowered = true,
	.usb_address = 0,

	.gadget = {
		.ops = &s3c_udc_ops,
		.ep0 = &memory.ep[0].ep,
		.name = driver_name,
		.dev = {
			.release = nop_release,
		},
	},

	/* control endpoint */
	.ep[0] = {
		.ep = {
			.name = ep0name,
			.ops = &s3c_ep_ops,
			.maxpacket = EP0_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = 0,
		.bmAttributes = 0,

		.ep_type = ep_control,
		.fifo = (unsigned int) S3C_UDC_OTG_EP0_FIFO,
	},

	/* first group of endpoints */
	.ep[1] = {
		.ep = {
			.name = "ep1in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 1,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP1_FIFO,
	},

	.ep[2] = {
		.ep = {
			.name = "ep2out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 2,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP2_FIFO,
	},

	.ep[3] = {
		.ep = {
			.name = "ep3in-int",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 3,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo = (unsigned int) S3C_UDC_OTG_EP3_FIFO,
	},
	.ep[4] = {
		.ep = {
			.name = "ep4out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 4,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP4_FIFO,
	},
	.ep[5] = {
		.ep = {
			.name = "ep5in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 5,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP5_FIFO,
	},
	.ep[6] = {
		.ep = {
			.name = "ep6in-int",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 6,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo = (unsigned int) S3C_UDC_OTG_EP6_FIFO,
	},
	.ep[7] = {
		.ep = {
			.name = "ep7out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 7,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP7_FIFO,
	},
	.ep[8] = {
		.ep = {
			.name = "ep8in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 8,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP8_FIFO,
	},
	.ep[9] = {
		.ep = {
			.name = "ep9in-int",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 9,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo = (unsigned int) S3C_UDC_OTG_EP9_FIFO,
	},
	.ep[10] = {
		.ep = {
			.name = "ep10out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 10,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP10_FIFO,
	},
	.ep[11] = {
		.ep = {
			.name = "ep11in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 11,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP11_FIFO,
	},
	.ep[12] = {
		.ep = {
			.name = "ep12in-int",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 12,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo = (unsigned int) S3C_UDC_OTG_EP12_FIFO,
	},
	.ep[13] = {
		.ep = {
			.name = "ep13out-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 13,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
		.fifo = (unsigned int) S3C_UDC_OTG_EP13_FIFO,
	},
	.ep[14] = {
		.ep = {
			.name = "ep14in-bulk",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 14,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
		.fifo = (unsigned int) S3C_UDC_OTG_EP14_FIFO,
	},
	.ep[15] = {
		.ep = {
			.name = "ep15-iso",
			.ops = &s3c_ep_ops,
			.maxpacket = EP_FIFO_SIZE2,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 15,
		.bmAttributes = USB_ENDPOINT_XFER_ISOC,

		.ep_type = ep_isochronous,
		.fifo = (unsigned int) S3C_UDC_OTG_EP15_FIFO,
	},
};

static int s3c_udc_probe(struct platform_device *pdev)
{
	struct s3c_udc *dev = &memory;
	struct resource *res;
	unsigned int irq;
	int retval;
	int err;

	DEBUG("%s: %p\n", __func__, pdev);

	dev->phy = devm_usb_get_phy(&pdev->dev, USB_PHY_TYPE_USB2);
	if (IS_ERR(dev->phy)) {
		dev_err(&pdev->dev, "no usb2 phy configured\n");
		return -EPROBE_DEFER;
	}

	spin_lock_init(&dev->lock);
	dev->dev = pdev;

	dev->gadget.dev.parent = &pdev->dev;
	dev->gadget.dev.dma_mask = pdev->dev.dma_mask;

	dev->gadget.speed = USB_SPEED_UNKNOWN;
	dev->gadget.max_speed = USB_SPEED_HIGH;
	dev->gadget.is_otg = 0;
	dev->gadget.is_a_peripheral = 0;
	dev->gadget.b_hnp_enable = 0;
	dev->gadget.a_hnp_support = 0;
	dev->gadget.a_alt_hnp_support = 0;

	the_controller = dev;
	platform_set_drvdata(pdev, dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		DEBUG("cannot find register resource 0\n");
		return -EINVAL;
	}

	dev->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (!dev->regs) {
		DEBUG("cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}
	udc_reinit(dev);

	/* get clk for OTG*/
	dev->otg_aclk = devm_clk_get(&pdev->dev, "otg_aclk");
	dev->otg_hclk = devm_clk_get(&pdev->dev, "otg_hclk");
	dev->upsizer_otg = devm_clk_get(&pdev->dev, "upsizer_otg");
	dev->xiu_d_fsys1 = devm_clk_get(&pdev->dev, "xiu_d_fsys1");
	dev->upsizer_fsys1 = devm_clk_get(&pdev->dev, "upsizer_fsys1");
	dev->upsizer_ahb_usbhs = devm_clk_get(&pdev->dev, "upsizer_ahb_usbhs");

	dev->usb_ctrl = dma_alloc_coherent(&pdev->dev,
			sizeof(struct usb_ctrlrequest)*BACK2BACK_SIZE,
			&dev->usb_ctrl_dma, GFP_KERNEL);

	if (!dev->usb_ctrl) {
		DEBUG(KERN_ERR "%s: can't get usb_ctrl dma memory\n",
			driver_name);
		retval = -ENOMEM;
		goto err_get_ctrl_buf;
	}

	dev->ep0_data = dma_alloc_coherent(&pdev->dev,
			EP0_FIFO_SIZE,
			&dev->ep0_data_dma, GFP_KERNEL);

	if (!dev->ep0_data) {
		DEBUG(KERN_ERR "%s: can't get ep0_data dma memory\n",
			driver_name);
		retval = -ENOMEM;
		goto err_get_data_buf;
	}

	err = clk_prepare_enable(dev->otg_aclk);
		clk_prepare_enable(dev->otg_hclk);
		clk_prepare_enable(dev->upsizer_otg);
		clk_prepare_enable(dev->xiu_d_fsys1);
		clk_prepare_enable(dev->upsizer_fsys1);
	if (err)
		DEBUG(KERN_ERR "%s: can't enable clk \n", driver_name);

	/* Mask any interrupt left unmasked by the bootloader */
	__raw_writel(0, dev->regs + S3C_UDC_OTG_GINTMSK);

	/* irq setup after old hardware state is cleaned up */
	irq = platform_get_irq(pdev, 0);
	retval =
	    request_irq(irq, s3c_udc_irq, 0, driver_name, dev);

	if (retval != 0) {
		DEBUG(KERN_ERR "%s: can't get irq %i, err %d\n", driver_name,
		      dev->irq, retval);
		retval = -EBUSY;
		clk_disable_unprepare(dev->otg_aclk);
		clk_disable_unprepare(dev->otg_hclk);
		clk_disable_unprepare(dev->upsizer_otg);
		clk_disable_unprepare(dev->xiu_d_fsys1);
		clk_disable_unprepare(dev->upsizer_fsys1);
		goto err_irq;
	}
	dev->irq = irq;

	disable_irq(dev->irq);

	retval = usb_add_gadget_udc(&pdev->dev, &dev->gadget);
	if (retval) {
		dev_err(&pdev->dev, "failed to add gadget to udc list\n");
		goto err_add_udc;
	}

	create_proc_files();

	return retval;

err_add_udc:
	free_irq(dev->irq, dev);
err_irq:
	dma_free_coherent(&pdev->dev,
			EP0_FIFO_SIZE,
			dev->ep0_data, dev->ep0_data_dma);
err_get_data_buf:
	dma_free_coherent(&pdev->dev,
			sizeof(struct usb_ctrlrequest)*BACK2BACK_SIZE,
			dev->usb_ctrl, dev->usb_ctrl_dma);
err_get_ctrl_buf:
	clk_put(dev->clk);
	return retval;
}

static int s3c_udc_remove(struct platform_device *pdev)
{
	struct s3c_udc *dev = platform_get_drvdata(pdev);

	DEBUG("%s: %p\n", __func__, pdev);

	remove_proc_files();
	usb_gadget_unregister_driver(dev->driver);
	usb_del_gadget_udc(&dev->gadget);
	device_unregister(&dev->gadget.dev);

	clk_put(dev->clk);
	if (dev->ep0_data)
		dma_free_coherent(&pdev->dev,
				EP0_FIFO_SIZE,
				dev->ep0_data, dev->ep0_data_dma);
	if (dev->usb_ctrl)
		dma_free_coherent(&pdev->dev,
				sizeof(struct usb_ctrlrequest)*BACK2BACK_SIZE,
				dev->usb_ctrl, dev->usb_ctrl_dma);
	free_irq(dev->irq, dev);

	platform_set_drvdata(pdev, 0);

	the_controller = 0;

	return 0;
}

#ifdef CONFIG_PM
static int s3c_udc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct s3c_udc *dev = the_controller;
	int i;

	if (dev->driver) {
		if (dev->driver->suspend)
			dev->driver->suspend(&dev->gadget);

		/* Terminate any outstanding requests  */
		for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
			struct s3c_ep *ep = &dev->ep[i];
			unsigned long flags;

			spin_lock_irqsave(&dev->lock, flags);
			ep->stopped = 1;
			nuke(ep, -ESHUTDOWN);
			spin_unlock_irqrestore(&dev->lock, flags);
		}

		if (dev->driver->disconnect)
			dev->driver->disconnect(&dev->gadget);
	}

	return 0;
}

static int s3c_udc_resume(struct platform_device *pdev)
{
	struct s3c_udc *dev = the_controller;

	if (dev->driver) {
		udc_reinit(dev);
		s3c_udc_soft_connect();

		if (dev->driver->resume)
			dev->driver->resume(&dev->gadget);
	}

	return 0;
}
#else
#define s3c_udc_suspend NULL
#define s3c_udc_resume  NULL
#endif /* CONFIG_PM */

/*-------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id exynos_udc_match[] = {
	{ .compatible = "samsung,exynos_udc" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_udc_match);
#endif
static struct platform_driver s3c_udc_driver = {
	.probe		= s3c_udc_probe,
	.remove		= s3c_udc_remove,
	.suspend	= s3c_udc_suspend,
	.resume		= s3c_udc_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-hsotg",
		.of_match_table = of_match_ptr(exynos_udc_match),
	},
};

static int __init udc_init(void)
{
	int ret;

	ret = platform_driver_register(&s3c_udc_driver);
	if (!ret)
		printk(KERN_INFO "%s : %s\n"
			"%s : version %s\n",
			driver_name, DRIVER_DESC,
			driver_name, DRIVER_VERSION);

	return ret;
}

static void __exit udc_exit(void)
{
	platform_driver_unregister(&s3c_udc_driver);
	printk(KERN_INFO "Unloaded %s version %s\n",
				driver_name, DRIVER_VERSION);
}

module_init(udc_init);
module_exit(udc_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL");
