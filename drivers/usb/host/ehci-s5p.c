/*
 * SAMSUNG S5P USB HOST EHCI Controller
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Jingoo Han <jg1.han@samsung.com>
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/platform_data/usb-ehci-s5p.h>
#include <linux/usb/phy.h>
#include <linux/usb/samsung_usb_phy.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>

#include <mach/exynos-pm.h>

#include "ehci.h"

#if defined(CONFIG_MDM_HSIC_PM)
#include <linux/mdm_hsic_pm.h>
static const char hsic_pm_dev[] = "15510000.mdmpm_pdata";
#endif

#define DRIVER_DESC "EHCI s5p driver"

#define EHCI_INSNREG00(base)			(base + 0x90)
#define EHCI_INSNREG00_ENA_INCR16		(0x1 << 25)
#define EHCI_INSNREG00_ENA_INCR8		(0x1 << 24)
#define EHCI_INSNREG00_ENA_INCR4		(0x1 << 23)
#define EHCI_INSNREG00_ENA_INCRX_ALIGN		(0x1 << 22)
#define EHCI_INSNREG00_ENABLE_DMA_BURST	\
	(EHCI_INSNREG00_ENA_INCR16 | EHCI_INSNREG00_ENA_INCR8 |	\
	 EHCI_INSNREG00_ENA_INCR4 | EHCI_INSNREG00_ENA_INCRX_ALIGN)

static const char hcd_name[] = "ehci-s5p";
static struct hc_driver __read_mostly s5p_ehci_hc_driver;

static const char *s5p_ehci_clk_names[] = {"aclk", "aclk_axius", "aclk_ahb",
	"aclk_ahb2axi", "sclk_usbhost20", "sclk_usb20phy_hsic",
	"oscclk_phy", "freeclk", "phyclk", NULL};

static int (*bus_resume)(struct usb_hcd *);

struct s5p_ehci_hcd {
	struct clk *clk;
	struct clk **clocks;
	struct usb_phy *phy;
	struct usb_otg *otg;
	struct s5p_ehci_platdata *pdata;
	struct notifier_block lpa_nb;
	int power_on;
	int retention;
	unsigned post_lpa_resume:1;
};

#define to_s5p_ehci(hcd)      (struct s5p_ehci_hcd *)(hcd_to_ehci(hcd)->priv)

struct usb_hcd *tmp_hcd;

extern void print_usb2phy_registers(struct usb_phy *uphy);
extern void print_ehci_registers(struct usb_hcd *hcd);

void print_all_registers(void)
{
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(tmp_hcd);

	printk("\n");
	print_ehci_registers(tmp_hcd);
	print_usb2phy_registers(s5p_ehci->phy);
	printk("\n\n");
}

static int s5p_ehci_clk_prepare_enable(struct s5p_ehci_hcd *s5p_ehci)
{
	int i;
	int ret;

	if (s5p_ehci->clk) {
		ret = clk_prepare_enable(s5p_ehci->clk);
		if (ret)
			return ret;
	} else {
		for (i = 0; s5p_ehci->clocks[i] != NULL; i++) {
			ret = clk_prepare_enable(s5p_ehci->clocks[i]);
			if (ret)
				goto err;
		}
	}

	return 0;

err:
	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_disable_unprepare(s5p_ehci->clocks[i]);

	return ret;
}

static void s5p_ehci_clk_disable_unprepare(struct s5p_ehci_hcd *s5p_ehci)
{
	int i;

	if (s5p_ehci->clk) {
		clk_disable_unprepare(s5p_ehci->clk);
	} else {
		for (i = 0; s5p_ehci->clocks[i] != NULL; i++)
			clk_disable_unprepare(s5p_ehci->clocks[i]);
	}
}

static int s5p_ehci_clk_get(struct s5p_ehci_hcd *s5p_ehci, struct device *dev)
{
	const char *clk_id;
	struct clk *clk;
	int i;

	clk_id = "usbhost";
	clk = devm_clk_get(dev, clk_id);
	if (IS_ERR_OR_NULL(clk)) {
		dev_info(dev, "IP clock gating is N/A\n");
		s5p_ehci->clk = NULL;
		/* fallback to separate clock control */
		s5p_ehci->clocks = (struct clk **) devm_kmalloc(dev,
				ARRAY_SIZE(s5p_ehci_clk_names) *
					sizeof(struct clk *),
				GFP_KERNEL);
		if (!s5p_ehci->clocks)
			return -ENOMEM;

		for (i = 0; s5p_ehci_clk_names[i] != NULL; i++) {
			clk_id = s5p_ehci_clk_names[i];

			clk = devm_clk_get(dev, clk_id);
			if (IS_ERR_OR_NULL(clk))
				goto err;

			s5p_ehci->clocks[i] = clk;
		}

		s5p_ehci->clocks[i] = NULL;

	} else {
		s5p_ehci->clk = clk;
	}

	return 0;

err:
	dev_err(dev, "couldn't get %s clock\n", clk_id);

	return -EINVAL;
}

#if defined(CONFIG_MDM_HSIC_PM)
static struct raw_notifier_head hsic_notifier;
int phy_register_notifier(struct notifier_block *nb)
{
	pr_info("mif: %s: register hsic_notifier\n", __func__);
	return raw_notifier_chain_register(&hsic_notifier, nb);
}
int phy_unregister_notifier(struct notifier_block *nb)
{
	pr_info("mif: %s:  unregister hsic_notifier\n", __func__);
	return raw_notifier_chain_unregister(&hsic_notifier, nb);
}

int usb_phy_prepare_shutdown(void)
{
	int ret;

	pr_info("[MIF] %s\n", __func__);
	ret = raw_notifier_call_chain(&hsic_notifier,
				STATE_HSIC_SUSPEND, NULL);
	return (ret == NOTIFY_DONE || ret == NOTIFY_OK) ? 0 : ret;
}

int usb_phy_prepare_wakeup(void)
{
	int ret = raw_notifier_call_chain(&hsic_notifier,
				STATE_HSIC_RESUME, NULL);
	return (ret == NOTIFY_DONE || ret == NOTIFY_OK) ? 0 : ret;
}
#endif

static void s5p_setup_vbus_gpio(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err;
	int gpio;

	if (!dev->of_node)
		return;

#if !defined(CONFIG_USB_EXYNOS_SWITCH)
	gpio = of_get_named_gpio(dev->of_node, "samsung,vbus-gpio", 0);
	if (!gpio_is_valid(gpio))
		return;

	err = devm_gpio_request_one(dev, gpio, GPIOF_OUT_INIT_HIGH,
				    "ehci_vbus_gpio");
	if (err)
		dev_err(dev, "can't request ehci vbus gpio %d", gpio);
	else
		gpio_set_value(gpio, 1);
#endif

	gpio = of_get_named_gpio(dev->of_node, "samsung,boost5v-gpio", 0);
	if (!gpio_is_valid(gpio))
		return;

	err = devm_gpio_request_one(dev, gpio, GPIOF_OUT_INIT_HIGH,
				    "usb_boost5v_gpio");
	if (err)
		dev_err(dev, "can't request usb boost5v gpio %d", gpio);
	else
		gpio_set_value(gpio, 1);
}

static int s5p_ehci_configurate(struct usb_hcd *hcd)
{
	int delay_count = 0;

	/* This is for waiting phy before ehci configuration */
	do {
		if (readl(hcd->regs))
			break;
		udelay(1);
		++delay_count;
	} while (delay_count < 200);
	if (delay_count)
		dev_info(hcd->self.controller, "phy delay count = %d\n",
			delay_count);

	return 0;
}

static void s5p_ehci_phy_init(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	if (s5p_ehci->phy) {
		usb_phy_init(s5p_ehci->phy);
		s5p_ehci->post_lpa_resume = 0;
	} else if (s5p_ehci->pdata->phy_init) {
		s5p_ehci->pdata->phy_init(pdev, USB_PHY_TYPE_HOST);
	} else {
		dev_err(&pdev->dev, "Failed to init ehci phy\n");
		return;
	}

	s5p_ehci_configurate(hcd);
}

static ssize_t show_ehci_power(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	return snprintf(buf, PAGE_SIZE, "EHCI Power %s\n",
			(s5p_ehci->power_on) ? "on" : "off");
}

static ssize_t store_ehci_power(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	int power_on;
	int irq;
	int retval;

	if (sscanf(buf, "%d", &power_on) != 1)
		return -EINVAL;

	device_lock(dev);

	if (!power_on && s5p_ehci->power_on) {
		dev_info(dev, "EHCI turn off\n");
		pm_runtime_forbid(dev);
		s5p_ehci->power_on = 0;
		usb_remove_hcd(hcd);

		if (s5p_ehci->phy) {
			/* Shutdown PHY only if it wasn't shutdown before */
			if (!s5p_ehci->post_lpa_resume)
				usb_phy_shutdown(s5p_ehci->phy);
		} else if (s5p_ehci->pdata->phy_exit) {
			s5p_ehci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);
		}
	} else if (power_on) {
		dev_info(dev, "EHCI turn on\n");
		if (s5p_ehci->power_on) {
			pm_runtime_forbid(dev);
			usb_remove_hcd(hcd);
		} else {
			s5p_ehci_phy_init(pdev);
		}

		irq = platform_get_irq(pdev, 0);
		retval = usb_add_hcd(hcd, irq, IRQF_SHARED);
		if (retval < 0) {
			dev_err(dev, "Power On Fail\n");
			goto exit;
		}

		/*
		 * EHCI root hubs are expected to handle remote wakeup.
		 * So, wakeup flag init defaults for root hubs.
		 */
		device_wakeup_enable(&hcd->self.root_hub->dev);

		s5p_ehci->power_on = 1;
		pm_runtime_allow(dev);
	}
exit:
	device_unlock(dev);
	return count;
}

static DEVICE_ATTR(ehci_power, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
	show_ehci_power, store_ehci_power);

static inline int create_ehci_sys_file(struct ehci_hcd *ehci)
{
	return device_create_file(ehci_to_hcd(ehci)->self.controller,
			&dev_attr_ehci_power);
}

static inline void remove_ehci_sys_file(struct ehci_hcd *ehci)
{
	device_remove_file(ehci_to_hcd(ehci)->self.controller,
			&dev_attr_ehci_power);
}

static int
s5p_ehci_lpa_event(struct notifier_block *nb, unsigned long event, void *data)
{
	struct s5p_ehci_hcd *s5p_ehci = container_of(nb,
					struct s5p_ehci_hcd, lpa_nb);
	int ret = NOTIFY_OK;

	switch (event) {
	case LPA_ENTER:
#if defined(CONFIG_MDM_HSIC_PM)
		if (s5p_ehci->post_lpa_resume)
			break;
		usb_phy_prepare_shutdown();
		pr_info("set hsic lpa enter\n");
#endif
		/*
		 * For the purpose of reducing of power consumption in LPA mode
		 * the PHY should be completely shutdown and reinitialized after
		 * exit from LPA.
		 */
		if (s5p_ehci->phy)
			usb_phy_shutdown(s5p_ehci->phy);

		s5p_ehci->post_lpa_resume = 1;
		break;
	default:
		ret = NOTIFY_DONE;
	}

	return ret;
}

#if 1 /* for IAA_watchdog */
extern unsigned int watchdog_count;
#endif

static int s5p_ehci_probe(struct platform_device *pdev)
{
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct device_node *node = (&pdev->dev)->of_node;
	struct s5p_ehci_hcd *s5p_ehci;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	struct resource *res;
	struct usb_phy *phy;
	int irq;
	int err;

	dev_err(&pdev->dev, "%s\n",__func__);
	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	s5p_setup_vbus_gpio(pdev);

	hcd = usb_create_hcd(&s5p_ehci_hc_driver,
			     &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		return -ENOMEM;
	}
	s5p_ehci = to_s5p_ehci(hcd);
	phy = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
	if (IS_ERR(phy)) {
		/* Fallback to pdata */
		if (!pdata) {
			usb_put_hcd(hcd);
			dev_warn(&pdev->dev, "no platform data or transceiver defined\n");
			return -EPROBE_DEFER;
		} else {
			s5p_ehci->pdata = pdata;
		}
	} else {
		s5p_ehci->phy = phy;
		s5p_ehci->otg = phy->otg;
	}

	err = of_property_read_u32_index(node, "l2-retention", 0,
						&s5p_ehci->retention);
	if (err)
		dev_err(&pdev->dev, " can not find l2-retention value\n");

	err = s5p_ehci_clk_get(s5p_ehci, &pdev->dev);

	if (err) {
		dev_err(&pdev->dev, "Failed to get clocks\n");
		goto fail_clk;
	}

	err = s5p_ehci_clk_prepare_enable(s5p_ehci);
	if (err)
		goto fail_clk;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get I/O memory\n");
		err = -ENXIO;
		goto fail_io;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "Failed to remap I/O memory\n");
		err = -ENOMEM;
		goto fail_io;
	}

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		err = -ENODEV;
		goto fail_io;
	}

	if (s5p_ehci->otg)
		s5p_ehci->otg->set_host(s5p_ehci->otg, &hcd->self);

	if (s5p_ehci->phy) {
		/* Make sure PHY is initialized */
		usb_phy_init(s5p_ehci->phy);
		/*
		 * PHY can be runtime suspended (e.g. by OHCI driver), so
		 * make sure PHY is active
		 */
#if defined(CONFIG_MDM_HSIC_PM)
		if(atomic_read(&phy->dev->power.usage_count) == 0){
			dev_err(&pdev->dev, "%s pm_runtime_get_sync \n",__func__);
			pm_runtime_get_sync(s5p_ehci->phy->dev);
		}
#else
		pm_runtime_get_sync(s5p_ehci->phy->dev);
#endif

	} else if (s5p_ehci->pdata->phy_init) {
		s5p_ehci->pdata->phy_init(pdev, USB_PHY_TYPE_HOST);
	}

	ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs;

	if (node) {
		u32 tmp_hsic_ports;

		ehci->has_synopsys_hsic_bug = of_property_read_bool(node,
							"has-synopsys-hsic");

		err = of_property_read_u32(node, "hsic-ports",
							&tmp_hsic_ports);
		if (err) {
			ehci->hsic_ports = 0;
			dev_info(&pdev->dev, "HSIC ports are not defined\n");
		} else {
			ehci->hsic_ports = tmp_hsic_ports;
		}
	} else {
		ehci->has_synopsys_hsic_bug =
					s5p_ehci->pdata->has_synopsys_hsic_bug;
		ehci->hsic_ports = s5p_ehci->pdata->hsic_ports;
	}

	/* DMA burst Enable */
	writel(EHCI_INSNREG00_ENABLE_DMA_BURST, EHCI_INSNREG00(hcd->regs));

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err) {
		dev_err(&pdev->dev, "Failed to add USB HCD\n");
		goto fail_add_hcd;
	}

	platform_set_drvdata(pdev, hcd);

	/*
	 * EHCI root hubs are expected to handle remote wakeup.
	 * So, wakeup flag init defaults for root hubs.
	 */
	device_wakeup_enable(&hcd->self.root_hub->dev);

	if (create_ehci_sys_file(ehci))
		dev_err(&pdev->dev, "Failed to create ehci sys file\n");

	s5p_ehci->lpa_nb.notifier_call = s5p_ehci_lpa_event;
	s5p_ehci->lpa_nb.next = NULL;
	s5p_ehci->lpa_nb.priority = 5;

	if (!s5p_ehci->retention) {
		err = exynos_pm_register_notifier(&s5p_ehci->lpa_nb);
		if (err)
			dev_err(&pdev->dev, "Failed to register lpa notifier\n");
	}

	s5p_ehci->power_on = 1;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

#if defined(CONFIG_MDM_HSIC_PM)
	pm_runtime_set_autosuspend_delay(&hcd->self.root_hub->dev,0);
	pm_runtime_forbid(&pdev->dev);
#endif

	tmp_hcd = hcd;
#if 1 /* for IAA_watchdog */
	watchdog_count = 0;
#endif

	return 0;

fail_add_hcd:
	if (s5p_ehci->phy) {
#if defined(CONFIG_MDM_HSIC_PM)
                if(atomic_read(&phy->dev->power.usage_count) == 1){
			pm_runtime_put_sync(s5p_ehci->phy->dev);
		}
#else
		pm_runtime_put_sync(s5p_ehci->phy->dev);
#endif

		usb_phy_shutdown(s5p_ehci->phy);
	} else if (s5p_ehci->pdata->phy_exit) {
		s5p_ehci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);
	}
fail_io:
	s5p_ehci_clk_disable_unprepare(s5p_ehci);
fail_clk:
	usb_put_hcd(hcd);
	return err;
}

static int s5p_ehci_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	pm_runtime_disable(&pdev->dev);

	s5p_ehci->power_on = 0;
	if (!s5p_ehci->retention)
		exynos_pm_unregister_notifier(&s5p_ehci->lpa_nb);
	remove_ehci_sys_file(hcd_to_ehci(hcd));
	usb_remove_hcd(hcd);

	if (s5p_ehci->otg)
		s5p_ehci->otg->set_host(s5p_ehci->otg, &hcd->self);

	if (s5p_ehci->phy) {
		/* Shutdown PHY only if it wasn't shutdown before */
		if (!s5p_ehci->post_lpa_resume)
			usb_phy_shutdown(s5p_ehci->phy);
	} else if (s5p_ehci->pdata->phy_exit) {
		s5p_ehci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);
	}
	s5p_ehci_clk_disable_unprepare(s5p_ehci);
#if defined(CONFIG_MDM_HSIC_PM)
	if (s5p_ehci->otg)
	s5p_ehci->otg->host = NULL;
#endif
	usb_put_hcd(hcd);

	return 0;
}

static void s5p_ehci_shutdown(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

#ifdef CONFIG_PM_RUNTIME
static int s5p_ehci_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	bool do_wakeup = device_may_wakeup(dev);
	int rc;

	dev_err(dev, "%s\n", __func__);

	rc = ehci_suspend(hcd, do_wakeup);

	if (s5p_ehci->phy)
		pm_runtime_put_sync(s5p_ehci->phy->dev);
	else if (pdata && pdata->phy_suspend)
		pdata->phy_suspend(pdev, USB_PHY_TYPE_HOST);

#if defined(CONFIG_MDM_HSIC_PM)
	request_active_lock_release(hsic_pm_dev);
	pr_info("%s: usage=%d, child=%d\n", __func__,
					atomic_read(&dev->power.usage_count),
					atomic_read(&dev->power.child_count));
#endif
	return rc;
}

static int s5p_ehci_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	int rc = 0;

	if (dev->power.is_suspended)
		return 0;

	dev_dbg(dev, "%s\n", __func__);

#if defined(CONFIG_MDM_HSIC_PM)
	request_active_lock_set(hsic_pm_dev);
#endif
	if (s5p_ehci->phy) {
		struct usb_phy *phy = s5p_ehci->phy;

		if (s5p_ehci->post_lpa_resume)
			usb_phy_init(phy);
		pm_runtime_get_sync(phy->dev);
	} else if (pdata && pdata->phy_resume) {
		rc = pdata->phy_resume(pdev, USB_PHY_TYPE_HOST);
		s5p_ehci->post_lpa_resume = !!rc;
	}

	if (s5p_ehci->post_lpa_resume)
		s5p_ehci_configurate(hcd);

	if (s5p_ehci->post_lpa_resume)
		ehci_resume(hcd, true);
	else
		ehci_resume(hcd, false);

	/*
	 * REVISIT: in case of LPA bus won't be resumed, so we do it here.
	 * Alternatively, we can try to setup HC in such a way that it starts
	 * to sense connections. In this case, root hub will be resumed from
	 * interrupt (ehci_irq()).
	 */
	if (s5p_ehci->post_lpa_resume)
		usb_hcd_resume_root_hub(hcd);

#if defined(CONFIG_MDM_HSIC_PM)
	if (s5p_ehci->post_lpa_resume) {
		usb_phy_prepare_wakeup();
		pr_info("set hsic lpa wakeup\n");
	}
#endif

	s5p_ehci->post_lpa_resume = 0;
#if defined(CONFIG_MDM_HSIC_PM)
	pr_info("%s: usage=%d, child=%d\n", __func__,
					atomic_read(&dev->power.usage_count),
					atomic_read(&dev->power.child_count));
#endif

	return 0;
}
#else
#define s5p_ehci_runtime_suspend	NULL
#define s5p_ehci_runtime_resume		NULL
#endif

#ifdef CONFIG_PM
static int s5p_ehci_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	struct platform_device *pdev = to_platform_device(dev);

	bool do_wakeup = device_may_wakeup(dev);
	int rc;

#if defined(CONFIG_MDM_HSIC_PM)
	rc = usb_phy_prepare_shutdown();
	if (rc)
		return rc;
#endif
	rc = ehci_suspend(hcd, do_wakeup);

	if (s5p_ehci->otg)
		s5p_ehci->otg->set_host(s5p_ehci->otg, &hcd->self);

	if (s5p_ehci->phy) {
		/* Shutdown PHY only if it wasn't shutdown before */
		if (!s5p_ehci->post_lpa_resume)
			usb_phy_shutdown(s5p_ehci->phy);
	} else if (s5p_ehci->pdata->phy_exit) {
		s5p_ehci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);
	}

	s5p_ehci_clk_disable_unprepare(s5p_ehci);

	return rc;
}

static int s5p_ehci_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct  s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	struct platform_device *pdev = to_platform_device(dev);

	s5p_ehci_clk_prepare_enable(s5p_ehci);

	if (s5p_ehci->otg)
		s5p_ehci->otg->set_host(s5p_ehci->otg, &hcd->self);

	if (s5p_ehci->phy) {
		usb_phy_init(s5p_ehci->phy);
		s5p_ehci->post_lpa_resume = 0;

		/*
		 * We are going to change runtime status to active.
		 * Make sure we get the phy only if we didn't get it before.
		 */
		if (pm_runtime_suspended(dev))
			pm_runtime_get_sync(s5p_ehci->phy->dev);
	} else if (s5p_ehci->pdata->phy_init) {
		s5p_ehci->pdata->phy_init(pdev, USB_PHY_TYPE_HOST);
	}

	/* DMA burst Enable */
	writel(EHCI_INSNREG00_ENABLE_DMA_BURST, EHCI_INSNREG00(hcd->regs));

	ehci_resume(hcd, true);

	/* Update runtime PM status and clear runtime_error */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

#if defined(CONFIG_MDM_HSIC_PM)
	usb_phy_prepare_wakeup();
	pm_runtime_mark_last_busy(&hcd->self.root_hub->dev);
#endif
	return 0;
}

int s5p_ehci_bus_resume(struct usb_hcd *hcd)
{
	/* When suspend is failed, re-enable clocks & PHY */
	pm_runtime_resume(hcd->self.controller);

	return bus_resume(hcd);
}
#else
#define s5p_ehci_suspend	NULL
#define s5p_ehci_resume		NULL
#define s5p_ehci_bus_resume	NULL
#endif

static const struct dev_pm_ops s5p_ehci_pm_ops = {
	.suspend	= s5p_ehci_suspend,
	.resume		= s5p_ehci_resume,
	.runtime_suspend	= s5p_ehci_runtime_suspend,
	.runtime_resume		= s5p_ehci_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_ehci_match[] = {
	{ .compatible = "samsung,exynos4210-ehci" },
	{ .compatible = "samsung,exynos5-ehci" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ehci_match);
#endif

static struct platform_driver s5p_ehci_driver = {
	.probe		= s5p_ehci_probe,
	.remove		= s5p_ehci_remove,
	.shutdown	= s5p_ehci_shutdown,
	.driver = {
		.name	= "s5p-ehci",
		.owner	= THIS_MODULE,
		.pm	= &s5p_ehci_pm_ops,
		.of_match_table = of_match_ptr(exynos_ehci_match),
	}
};
static const struct ehci_driver_overrides s5p_overrides __initdata = {
	.extra_priv_size = sizeof(struct s5p_ehci_hcd),
};

static int __init ehci_s5p_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);
#if defined(CONFIG_MDM_HSIC_PM)
	RAW_INIT_NOTIFIER_HEAD(&hsic_notifier);
#endif
	ehci_init_driver(&s5p_ehci_hc_driver, &s5p_overrides);

	bus_resume = s5p_ehci_hc_driver.bus_resume;
	s5p_ehci_hc_driver.bus_resume = s5p_ehci_bus_resume;

	return platform_driver_register(&s5p_ehci_driver);
}
module_init(ehci_s5p_init);

static void __exit ehci_s5p_cleanup(void)
{
	platform_driver_unregister(&s5p_ehci_driver);
}
module_exit(ehci_s5p_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_ALIAS("platform:s5p-ehci");
MODULE_AUTHOR("Jingoo Han");
MODULE_AUTHOR("Joonyoung Shim");
MODULE_LICENSE("GPL v2");
