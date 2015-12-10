/* linux/drivers/usb/phy/phy-samsung-usb2.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Praveen Paneri <p.paneri@samsung.com>
 *
 * Samsung USB2.0 PHY transceiver; talks to S3C HS OTG controller, EHCI-S5P and
 * OHCI-EXYNOS controllers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/usb/otg.h>
#include <linux/usb/samsung_usb_phy.h>
#include <linux/platform_data/samsung-usbphy.h>

#include <mach/exynos-pm.h>

#include "phy-samsung-usb.h"

#ifdef CONFIG_SOC_EXYNOS5422
#define EXYNOS5_PICO_SLEEP
#endif

void print_usb2phy_registers(struct usb_phy *uphy)
{
	struct samsung_usbphy *sphy;
	void __iomem *regs;
	u32 reg;

	sphy = phy_to_sphy(uphy);
	regs = sphy->regs;

	printk("\n%s\n", __func__);

	reg = readl(regs + EXYNOS5_PHY_HOST_CTRL0);
	printk("EXYNOS5_PHY_HOST_CTRL0:\t0x%08x\n", reg);

	reg = readl(regs + EXYNOS5_PHY_HOST_TUNE0);
	printk("EXYNOS5_PHY_HOST_TUNE0:\t0x%08x\n", reg);

	reg = readl(regs + EXYNOS5_PHY_HSIC_CTRL1);
	printk("EXYNOS5_PHY_HSIC_CTRL1:\t0x%08x\n", reg);

	reg = readl(regs + EXYNOS5_PHY_HSIC_TUNE1);
	printk("EXYNOS5_PHY_HSIC_TUNE1:\t0x%08x\n", reg);

	reg = readl(regs + EXYNOS5_PHY_HOST_EHCICTRL);
	printk("EXYNOS5_PHY_HOST_EHCICTRL:\t0x%08x\n", reg);
}

static const char *samsung_usb2phy_clk_names[] = {"aclk", "aclk_axius", "aclk_ahb",
	"aclk_ahb2axi", "sclk_usbhost20", "sclk_usb20phy_hsic",
	"oscclk_phy", "freeclk", "phyclk", NULL};

static int samsung_usb2phy_clk_prepare(struct samsung_usbphy *sphy)
{
	int i;
	int ret;

	if (sphy->clk) {
		ret = clk_prepare(sphy->clk);
		if (ret)
			return ret;
	} else {
		for (i = 0; sphy->clocks[i] != NULL; i++) {
			ret = clk_prepare(sphy->clocks[i]);
			if (ret)
				goto err;
		}
	}

	return 0;

err:
	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(sphy->clocks[i]);

	return ret;
}

static int samsung_usb2phy_clk_enable(struct samsung_usbphy *sphy)
{
	int i;
	int ret;

	if (sphy->clk) {
		ret = clk_enable(sphy->clk);
		if (ret)
			return ret;
	} else {
		for (i = 0; sphy->clocks[i] != NULL; i++) {
			ret = clk_enable(sphy->clocks[i]);
			if (ret)
				goto err;
		}
	}

	return 0;

err:
	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_disable(sphy->clocks[i]);

	return ret;
}

static void samsung_usb2phy_clk_unprepare(struct samsung_usbphy *sphy)
{
	int i;

	if (sphy->clk) {
		clk_unprepare(sphy->clk);
	} else {
		for (i = 0; sphy->clocks[i] != NULL; i++)
			clk_unprepare(sphy->clocks[i]);
	}
}

static void samsung_usb2phy_clk_disable(struct samsung_usbphy *sphy)
{
	int i;

	if (sphy->clk) {
		clk_disable(sphy->clk);
	} else {
		for (i = 0; sphy->clocks[i] != NULL; i++)
			clk_disable(sphy->clocks[i]);
	}
}

static int samsung_usb2phy_clk_get(struct samsung_usbphy *sphy)
{
	const char	*clk_id;
	struct clk	*clk;
	int		i;

	if (sphy->drv_data->cpu_type == TYPE_EXYNOS5250 ||
			sphy->drv_data->cpu_type == TYPE_EXYNOS7420 ||
			sphy->drv_data->cpu_type == TYPE_EXYNOS5)
		clk_id = "usbhost";
	else
		clk_id = "otg";

	clk = devm_clk_get(sphy->dev, clk_id);
	if (IS_ERR_OR_NULL(clk)) {
		dev_info(sphy->dev, "IP clock gating is N/A\n");
		sphy->clk = NULL;
		/* fallback to separate clock control */
		sphy->clocks = (struct clk **) devm_kmalloc(sphy->dev,
				ARRAY_SIZE(samsung_usb2phy_clk_names) *
					sizeof(struct clk *),
				GFP_KERNEL);
		if (!sphy->clocks)
			return -ENOMEM;

		for (i = 0; samsung_usb2phy_clk_names[i] != NULL; i++) {
			clk_id = samsung_usb2phy_clk_names[i];

			clk = devm_clk_get(sphy->dev, clk_id);
			if (IS_ERR_OR_NULL(clk))
				goto err;

			sphy->clocks[i] = clk;
		}

		sphy->clocks[i] = NULL;

	} else {
		sphy->clk = clk;
	}

	return 0;

err:
	dev_err(sphy->dev, "couldn't get %s clock\n", clk_id);

	return -EINVAL;
}

static int samsung_usbphy_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	if (!otg)
		return -ENODEV;

	if (!otg->host)
		otg->host = host;

	return 0;
}

static bool exynos5_phyhost_is_on(void __iomem *regs)
{
	u32 reg;

	reg = readl(regs + EXYNOS5_PHY_HOST_CTRL0);

	return !(reg & HOST_CTRL0_SIDDQ);
}

static void samsung_exynos5_usb2phy_enable(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;
	u32 phyclk = sphy->ref_clk_freq;
	u32 phyhost;
	u32 phyotg;
	u32 phyhsic;
	u32 ehcictrl;
	u32 ohcictrl;

	/*
	 * phy_usage helps in keeping usage count for phy
	 * so that the first consumer enabling the phy is also
	 * the last consumer to disable it.
	 */

	atomic_inc(&sphy->phy_usage);

	if (exynos5_phyhost_is_on(regs)) {
		dev_info(sphy->dev, "Already power on PHY\n");
		return;
	}

	/*
	 * REVISIT: move below samsung_exynos5_cal_usb2phy_enable() function
	 * call when proper solution is applied to CAL code.
	 *
	 * Fix Exynos7420 HW issue when overcurrent condition happens in PICO
	 * PHY while nothing can be connected to the port (this PHY doesn't
	 * have SoC pins).
	 */
	if (sphy->drv_data->cpu_type == TYPE_EXYNOS7420)
		writel(HOST_OVRCURCTL_OVERCUR_SEL | HOST_OVRCURCTL_OVER_CURRENT,
			regs + EXYNOS5_PHY_HOST_OVRCURCTL);

	/*
	 * CAL (Chip Abstraction Layer) is used to keep synchronization
	 * between kernel code and F/W verification code for the manageability.
	 * When CAL option is enabled, CAL enable function will be called,
	 * and the legacy enable routine will be skipped.
	 */
	if (IS_ENABLED(CONFIG_SAMSUNG_USB2PHY_CAL)) {
		samsung_exynos5_cal_usb2phy_enable(regs);
		return;
	}

	/* Host configuration */
	phyhost = readl(regs + EXYNOS5_PHY_HOST_CTRL0);

	/* phy reference clock configuration */
	phyhost &= ~HOST_CTRL0_FSEL_MASK;
	phyhost |= HOST_CTRL0_FSEL(phyclk);

	/* host phy reset */
	phyhost &= ~(HOST_CTRL0_PHYSWRST |
			HOST_CTRL0_PHYSWRSTALL |
			HOST_CTRL0_SIDDQ |
			/* Enable normal mode of operation */
			HOST_CTRL0_FORCESUSPEND |
			HOST_CTRL0_FORCESLEEP);

	/* Link reset */
	phyhost |= (HOST_CTRL0_LINKSWRST |
			HOST_CTRL0_UTMISWRST);

	/* COMMON Block configuration during suspend */
#ifdef EXYNOS5_PICO_SLEEP
	phyhost &= ~(HOST_CTRL0_COMMONON_N);
	phyhost |= (HOST_CTRL0_FORCESLEEP);
#else
	phyhost |= (HOST_CTRL0_COMMONON_N);
#endif

	writel(phyhost, regs + EXYNOS5_PHY_HOST_CTRL0);
	udelay(10);
	phyhost &= ~(HOST_CTRL0_LINKSWRST |
			HOST_CTRL0_UTMISWRST);
	writel(phyhost, regs + EXYNOS5_PHY_HOST_CTRL0);

	/* OTG configuration */
	phyotg = readl(regs + EXYNOS5_PHY_OTG_SYS);

	/* phy reference clock configuration */
	phyotg &= ~OTG_SYS_FSEL_MASK;
	phyotg |= OTG_SYS_FSEL(phyclk);

	/* Enable normal mode of operation */
	phyotg &= ~(OTG_SYS_FORCESUSPEND |
			OTG_SYS_SIDDQ_UOTG |
			OTG_SYS_FORCESLEEP |
			OTG_SYS_REFCLKSEL_MASK |
			/* COMMON Block configuration during suspend */
			OTG_SYS_COMMON_ON);

	/* OTG phy & link reset */
	phyotg |= (OTG_SYS_PHY0_SWRST |
			OTG_SYS_LINKSWRST_UOTG |
			OTG_SYS_PHYLINK_SWRESET |
			OTG_SYS_OTGDISABLE |
			/* Set phy refclk */
			OTG_SYS_REFCLKSEL_CLKCORE);

	writel(phyotg, regs + EXYNOS5_PHY_OTG_SYS);
	udelay(10);
	phyotg &= ~(OTG_SYS_PHY0_SWRST |
			OTG_SYS_LINKSWRST_UOTG |
			OTG_SYS_PHYLINK_SWRESET);
	writel(phyotg, regs + EXYNOS5_PHY_OTG_SYS);

	/* HSIC phy configuration */
	phyhsic = (HSIC_CTRL_REFCLKDIV_12 |
			HSIC_CTRL_REFCLKSEL |
			HSIC_CTRL_PHYSWRST);
	writel(phyhsic, regs + EXYNOS5_PHY_HSIC_CTRL1);
	writel(phyhsic, regs + EXYNOS5_PHY_HSIC_CTRL2);
	udelay(10);
	phyhsic &= ~HSIC_CTRL_PHYSWRST;
	writel(phyhsic, regs + EXYNOS5_PHY_HSIC_CTRL1);
	writel(phyhsic, regs + EXYNOS5_PHY_HSIC_CTRL2);

	udelay(80);

	/* enable EHCI DMA burst */
	ehcictrl = readl(regs + EXYNOS5_PHY_HOST_EHCICTRL);
	ehcictrl |= (HOST_EHCICTRL_ENAINCRXALIGN |
				HOST_EHCICTRL_ENAINCR4 |
				HOST_EHCICTRL_ENAINCR8 |
				HOST_EHCICTRL_ENAINCR16);
	writel(ehcictrl, regs + EXYNOS5_PHY_HOST_EHCICTRL);

	/* set ohci_suspend_on_n */
	ohcictrl = readl(regs + EXYNOS5_PHY_HOST_OHCICTRL);
	ohcictrl |= HOST_OHCICTRL_SUSPLGCY;
	writel(ohcictrl, regs + EXYNOS5_PHY_HOST_OHCICTRL);
}

static void samsung_usb2phy_enable(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;
	u32 phypwr;
	u32 phyclk;
	u32 rstcon;

	/* set clock frequency for PLL */
	phyclk = sphy->ref_clk_freq;
	phypwr = readl(regs + SAMSUNG_PHYPWR);
	rstcon = readl(regs + SAMSUNG_RSTCON);

	switch (sphy->drv_data->cpu_type) {
	case TYPE_S3C64XX:
		phyclk &= ~PHYCLK_COMMON_ON_N;
		phypwr &= ~PHYPWR_NORMAL_MASK;
		rstcon |= RSTCON_SWRST;
		break;
	case TYPE_EXYNOS4210:
		phypwr &= ~PHYPWR_NORMAL_MASK_PHY0;
		rstcon |= RSTCON_SWRST;
	default:
		break;
	}

	writel(phyclk, regs + SAMSUNG_PHYCLK);
	/* Configure PHY0 for normal operation*/
	writel(phypwr, regs + SAMSUNG_PHYPWR);
	/* reset all ports of PHY and Link */
	writel(rstcon, regs + SAMSUNG_RSTCON);
	udelay(10);
	rstcon &= ~RSTCON_SWRST;
	writel(rstcon, regs + SAMSUNG_RSTCON);
}

static void samsung_exynos5_usb2phy_disable(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;
	u32 phyhost;
	u32 phyotg;
	u32 phyhsic;

	if (atomic_dec_return(&sphy->phy_usage) > 0) {
		dev_info(sphy->dev, "still being used\n");
		return;
	}

	/*
	 * CAL (Chip Abstraction Layer) is used to keep synchronization
	 * between kernel code and F/W verification code for the manageability.
	 * When CAL option is enabled, CAL disable function will be called,
	 * and the legacy disable routine will be skipped.
	 */
	if (IS_ENABLED(CONFIG_SAMSUNG_USB2PHY_CAL)) {
		samsung_exynos5_cal_usb2phy_disable(regs);
		return;
	}

	phyhsic = (HSIC_CTRL_REFCLKDIV_12 |
			HSIC_CTRL_REFCLKSEL |
			HSIC_CTRL_SIDDQ |
			HSIC_CTRL_FORCESLEEP |
			HSIC_CTRL_FORCESUSPEND);
	writel(phyhsic, regs + EXYNOS5_PHY_HSIC_CTRL1);
	writel(phyhsic, regs + EXYNOS5_PHY_HSIC_CTRL2);

	phyhost = readl(regs + EXYNOS5_PHY_HOST_CTRL0);
#ifdef EXYNOS5_PICO_SLEEP
	phyhost |= (HOST_CTRL0_COMMONON_N);
#endif
	phyhost |= (HOST_CTRL0_SIDDQ |
			HOST_CTRL0_FORCESUSPEND |
			HOST_CTRL0_FORCESLEEP |
			HOST_CTRL0_PHYSWRST |
			HOST_CTRL0_PHYSWRSTALL);
	writel(phyhost, regs + EXYNOS5_PHY_HOST_CTRL0);

	phyotg = readl(regs + EXYNOS5_PHY_OTG_SYS);
	phyotg |= (OTG_SYS_FORCESUSPEND |
			OTG_SYS_SIDDQ_UOTG |
			OTG_SYS_FORCESLEEP);
	writel(phyotg, regs + EXYNOS5_PHY_OTG_SYS);
}

static void samsung_usb2phy_disable(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;
	u32 phypwr;

	phypwr = readl(regs + SAMSUNG_PHYPWR);

	switch (sphy->drv_data->cpu_type) {
	case TYPE_S3C64XX:
		phypwr |= PHYPWR_NORMAL_MASK;
		break;
	case TYPE_EXYNOS4210:
		phypwr |= PHYPWR_NORMAL_MASK_PHY0;
	default:
		break;
	}

	/* Disable analog and otg block power */
	writel(phypwr, regs + SAMSUNG_PHYPWR);
}

/*
 * The function passed to the usb driver for phy initialization
 */
static int samsung_usb2phy_init(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy;
	struct usb_bus *host = NULL;
	unsigned long flags;
	int ret = 0;

	sphy = phy_to_sphy(phy);

	dev_vdbg(sphy->dev, "%s\n", __func__);

	host = phy->otg->host;

	/* Enable the phy clock */
	ret = samsung_usb2phy_clk_enable(sphy);
	if (ret) {
		dev_err(sphy->dev, "%s: clk_enable failed\n", __func__);
		return ret;
	}

	spin_lock_irqsave(&sphy->lock, flags);

	sphy->usage_count++;

	if (sphy->usage_count - 1) {
		dev_vdbg(sphy->dev, "PHY is already initialized\n");
		goto exit;
	}

	if (host) {
		/* setting default phy-type for USB 2.0 */
		if (!strstr(dev_name(host->controller), "ehci") ||
				!strstr(dev_name(host->controller), "ohci"))
			samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_HOST);
	} else {
		samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_DEVICE);
	}

	/* Disable phy isolation */
	if (sphy->plat && sphy->plat->pmu_isolation) {
		sphy->plat->pmu_isolation(false);
	} else {
		if (sphy->drv_data->cpu_type != TYPE_EXYNOS7420)
			samsung_usbphy_set_isolation(sphy, false);
		if (sphy->has_hsic_pmureg == true)
			samsung_hsicphy_set_isolation(sphy, false);
	}

	/* Selecting Host/OTG mode; After reset USB2.0PHY_CFG: HOST */
	samsung_usbphy_cfg_sel(sphy);

	/* Initialize usb phy registers */
	if (sphy->drv_data->cpu_type == TYPE_EXYNOS5250 ||
		sphy->drv_data->cpu_type == TYPE_EXYNOS7420 ||
		sphy->drv_data->cpu_type == TYPE_EXYNOS5)
		samsung_exynos5_usb2phy_enable(sphy);
	else
		samsung_usb2phy_enable(sphy);

exit:
	spin_unlock_irqrestore(&sphy->lock, flags);

	/* Disable the phy clock */
	samsung_usb2phy_clk_disable(sphy);

	dev_dbg(sphy->dev, "end of %s\n", __func__);

	return ret;
}

/*
 * The function passed to the usb driver for phy shutdown
 */
static void samsung_usb2phy_shutdown(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy;
	struct usb_bus *host = NULL;
	unsigned long flags;

	sphy = phy_to_sphy(phy);

	dev_vdbg(sphy->dev, "%s\n", __func__);

	host = phy->otg->host;

	if (samsung_usb2phy_clk_enable(sphy)) {
		dev_err(sphy->dev, "%s: clk_enable failed\n", __func__);
		return;
	}

	spin_lock_irqsave(&sphy->lock, flags);

	if (!sphy->usage_count) {
		dev_vdbg(sphy->dev, "PHY is already shutdown\n");
		goto exit;
	}

	sphy->usage_count--;

	if (sphy->usage_count) {
		dev_vdbg(sphy->dev, "PHY is still in use\n");
		goto exit;
	}

	if (host) {
		/* setting default phy-type for USB 2.0 */
		if (!strstr(dev_name(host->controller), "ehci") ||
				!strstr(dev_name(host->controller), "ohci"))
			samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_HOST);
	} else {
		samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_DEVICE);
	}

	/* De-initialize usb phy registers */
	if (sphy->drv_data->cpu_type == TYPE_EXYNOS5250 ||
		sphy->drv_data->cpu_type == TYPE_EXYNOS7420 ||
		sphy->drv_data->cpu_type == TYPE_EXYNOS5)
		samsung_exynos5_usb2phy_disable(sphy);
	else
		samsung_usb2phy_disable(sphy);

	/* Enable phy isolation */
	if (sphy->plat && sphy->plat->pmu_isolation) {
		sphy->plat->pmu_isolation(true);
	} else {
		if (sphy->drv_data->cpu_type != TYPE_EXYNOS7420)
			samsung_usbphy_set_isolation(sphy, true);
		if (sphy->has_hsic_pmureg == true)
			samsung_hsicphy_set_isolation(sphy, true);
	}

	dev_dbg(sphy->dev, "%s: End of setting for shutdown\n", __func__);
exit:
	spin_unlock_irqrestore(&sphy->lock, flags);

	samsung_usb2phy_clk_disable(sphy);
}

static bool samsung_usb2phy_is_active(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy = phy_to_sphy(phy);
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&sphy->lock, flags);

	if (!sphy->usage_count || pm_runtime_suspended(sphy->dev))
		ret = false;
	else
		ret = true;

	spin_unlock_irqrestore(&sphy->lock, flags);

	return ret;
}

static int
samsung_usb2phy_lpa_event(struct notifier_block *nb,
			  unsigned long event,
			  void *data)
{
	struct samsung_usbphy *sphy = container_of(nb,
					struct samsung_usbphy, lpa_nb);
	int err = 0;

	switch (event) {
	case LPA_PREPARE:
	case LPC_PREPARE:
		if (samsung_usb2phy_is_active(&sphy->phy))
			err = -EBUSY;
		break;
	default:
		return NOTIFY_DONE;
	}

	return notifier_from_errno(err);
}

static int samsung_usb2phy_probe(struct platform_device *pdev)
{
	struct samsung_usbphy *sphy;
	struct usb_otg *otg;
	struct samsung_usbphy_data *pdata = pdev->dev.platform_data;
	const struct samsung_usbphy_drvdata *drv_data;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	int ret;

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	sphy = devm_kzalloc(dev, sizeof(*sphy), GFP_KERNEL);
	if (!sphy)
		return -ENOMEM;

	otg = devm_kzalloc(dev, sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return -ENOMEM;

	drv_data = samsung_usbphy_get_driver_data(pdev);

	sphy->drv_data = drv_data;
	sphy->dev = dev;

	ret = samsung_usb2phy_clk_get(sphy);
	if (ret)
		return ret;

	if (dev->of_node) {
		ret = samsung_usbphy_parse_dt(sphy);
		if (ret < 0)
			return ret;
	} else {
		if (!pdata) {
			dev_err(dev, "no platform data specified\n");
			return -EINVAL;
		}
	}

	sphy->plat		= pdata;
	sphy->regs		= phy_base;
	sphy->phy.dev		= sphy->dev;
	sphy->phy.label		= "samsung-usb2phy";
	sphy->phy.type		= USB_PHY_TYPE_USB2;
	sphy->phy.init		= samsung_usb2phy_init;
	sphy->phy.shutdown	= samsung_usb2phy_shutdown;
	sphy->phy.is_active	= samsung_usb2phy_is_active;
	sphy->ref_clk_freq	= samsung_usbphy_get_refclk_freq(sphy);

	sphy->phy.otg		= otg;
	sphy->phy.otg->phy	= &sphy->phy;
	sphy->phy.otg->set_host = samsung_usbphy_set_host;

	if (of_property_read_u32(sphy->dev->of_node,
		"samsung,hsicphy_en_mask", (u32 *)&drv_data->hsicphy_en_mask))
		dev_dbg(dev, "Failed to get hsicphy_en_mask\n");
	else if (of_property_read_u32(sphy->dev->of_node,
		"samsung,hsicphy_reg_offset",
		(u32 *)&drv_data->hsicphy_reg_offset))
		dev_dbg(dev, "Failed to get hsicphy_en_mask\n");
	else
		sphy->has_hsic_pmureg = true;

	spin_lock_init(&sphy->lock);

	ret = samsung_usb2phy_clk_prepare(sphy);
	if (ret) {
		dev_err(dev, "clk_prepare failed\n");
		return ret;
	}

	platform_set_drvdata(pdev, sphy);

	ret = usb_add_phy_dev(&sphy->phy);
	if (ret) {
		dev_err(dev, "Failed to add PHY\n");
		goto err1;
	}

	sphy->lpa_nb.notifier_call = samsung_usb2phy_lpa_event;
	sphy->lpa_nb.next = NULL;
	sphy->lpa_nb.priority = 0;

	ret = exynos_pm_register_notifier(&sphy->lpa_nb);
	if (ret)
		dev_err(dev, "Failed to register lpa notifier\n");

	pm_runtime_enable(dev);

	return 0;

err1:
	samsung_usb2phy_clk_unprepare(sphy);

	return ret;
}

static int samsung_usb2phy_remove(struct platform_device *pdev)
{
	struct samsung_usbphy *sphy = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);

	exynos_pm_unregister_notifier(&sphy->lpa_nb);
	usb_remove_phy(&sphy->phy);
	samsung_usb2phy_clk_unprepare(sphy);

	if (sphy->pmuregs)
		iounmap(sphy->pmuregs);
	if (sphy->sysreg)
		iounmap(sphy->sysreg);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int samsung_usb2phy_runtime_suspend(struct device *dev)
{
	struct samsung_usbphy *sphy = dev_get_drvdata(dev);
	void __iomem *regs = sphy->regs;
	unsigned long flags;
	u32 phyctrl;

	dev_dbg(dev, "%s\n", __func__);

	if (sphy->drv_data->cpu_type != TYPE_EXYNOS7420 &&
		sphy->drv_data->cpu_type != TYPE_EXYNOS5) {
		dev_info(dev, "Runtime PM is not supported for this cpu type\n");
		return 0;
	}

	spin_lock_irqsave(&sphy->lock, flags);

	/* set to suspend HSIC 1 and 2 */
	phyctrl = readl(regs + EXYNOS5_PHY_HSIC_CTRL1);
	phyctrl |= HSIC_CTRL_FORCESUSPEND;
	writel(phyctrl, regs + EXYNOS5_PHY_HSIC_CTRL1);

	phyctrl = readl(regs + EXYNOS5_PHY_HSIC_CTRL2);
	phyctrl |= HSIC_CTRL_FORCESUSPEND;
	writel(phyctrl, regs + EXYNOS5_PHY_HSIC_CTRL2);

	/* set to suspend standard of PHY20 */
	phyctrl = readl(regs + EXYNOS5_PHY_HOST_CTRL0);
	phyctrl |= HOST_CTRL0_FORCESUSPEND;
#ifdef EXYNOS5_PICO_SLEEP
	phyctrl |= HOST_CTRL0_COMMONON_N;
#endif
	writel(phyctrl, regs + EXYNOS5_PHY_HOST_CTRL0);


	spin_unlock_irqrestore(&sphy->lock, flags);

	/* Disable the phy clock */
	samsung_usb2phy_clk_disable(sphy);

	return 0;
}

static int samsung_usb2phy_runtime_resume(struct device *dev)
{
	struct samsung_usbphy *sphy = dev_get_drvdata(dev);
	void __iomem *regs = sphy->regs;
	unsigned long flags;
	u32 phyctrl;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	if (sphy->drv_data->cpu_type != TYPE_EXYNOS7420 &&
		sphy->drv_data->cpu_type != TYPE_EXYNOS5) {
		dev_info(dev, "Runtime PM is not supported for this cpu type\n");
		return 0;
	}

	/* Enable the phy clock */
	ret = samsung_usb2phy_clk_enable(sphy);
	if (ret) {
		dev_err(sphy->dev, "%s: clk_enable failed\n", __func__);
		return ret;
	}

	spin_lock_irqsave(&sphy->lock, flags);

	phyctrl = readl(regs + EXYNOS5_PHY_HOST_CTRL0);
	phyctrl &= ~HOST_CTRL0_FORCESUSPEND;
#ifdef EXYNOS5_PICO_SLEEP
	phyctrl &= ~(HOST_CTRL0_COMMONON_N);
#endif
	writel(phyctrl, regs + EXYNOS5_PHY_HOST_CTRL0);

	phyctrl = readl(regs + EXYNOS5_PHY_HSIC_CTRL1);
	phyctrl &= ~HSIC_CTRL_FORCESUSPEND;
	writel(phyctrl, regs + EXYNOS5_PHY_HSIC_CTRL1);

	phyctrl = readl(regs + EXYNOS5_PHY_HSIC_CTRL2);
	phyctrl &= ~HSIC_CTRL_FORCESUSPEND;
	writel(phyctrl, regs + EXYNOS5_PHY_HSIC_CTRL2);

	spin_unlock_irqrestore(&sphy->lock, flags);

	return 0;
}
#else
#define samsung_usb2phy_runtime_suspend	NULL
#define samsung_usb2phy_runtime_resume	NULL
#endif

static const struct dev_pm_ops samsung_usb2phy_pm_ops = {
	.runtime_suspend	= samsung_usb2phy_runtime_suspend,
	.runtime_resume		= samsung_usb2phy_runtime_resume,
};

static const struct samsung_usbphy_drvdata usb2phy_s3c64xx = {
	.cpu_type		= TYPE_S3C64XX,
	.devphy_en_mask		= S3C64XX_USBPHY_ENABLE,
};

static const struct samsung_usbphy_drvdata usb2phy_exynos4 = {
	.cpu_type		= TYPE_EXYNOS4210,
	.devphy_en_mask		= EXYNOS_USBPHY_ENABLE,
	.hostphy_en_mask	= EXYNOS_USBPHY_ENABLE,
};

static struct samsung_usbphy_drvdata usb2phy_exynos5250 = {
	.cpu_type		= TYPE_EXYNOS5250,
	.hostphy_en_mask	= EXYNOS_USBPHY_ENABLE,
	.hostphy_reg_offset	= EXYNOS_USBHOST_PHY_CTRL_OFFSET,
};

static struct samsung_usbphy_drvdata usb2phy_exynos7420 = {
	.cpu_type		= TYPE_EXYNOS7420,
	.hostphy_en_mask	= EXYNOS_USBPHY_ENABLE,
	.hostphy_reg_offset	= EXYNOS5_USB2PHY_CTRL_OFFSET,
};

static struct samsung_usbphy_drvdata usb2phy_exynos5 = {
	.cpu_type		= TYPE_EXYNOS5,
	.hostphy_en_mask	= EXYNOS_USBPHY_ENABLE,
	.hostphy_reg_offset	= EXYNOS5_USB2PHY_CTRL_OFFSET,
};

#ifdef CONFIG_OF
static const struct of_device_id samsung_usbphy_dt_match[] = {
	{
		.compatible = "samsung,s3c64xx-usb2phy",
		.data = &usb2phy_s3c64xx,
	}, {
		.compatible = "samsung,exynos4210-usb2phy",
		.data = &usb2phy_exynos4,
	}, {
		.compatible = "samsung,exynos5250-usb2phy",
		.data = &usb2phy_exynos5250,
	}, {
		.compatible = "samsung,exynos7420-usb2phy",
		.data = &usb2phy_exynos7420,
	}, {
		.compatible = "samsung,exynos5-usb2phy",
		.data = &usb2phy_exynos5,
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_usbphy_dt_match);
#endif

static struct platform_device_id samsung_usbphy_driver_ids[] = {
	{
		.name		= "s3c64xx-usb2phy",
		.driver_data	= (unsigned long)&usb2phy_s3c64xx,
	}, {
		.name		= "exynos4210-usb2phy",
		.driver_data	= (unsigned long)&usb2phy_exynos4,
	}, {
		.name		= "exynos5250-usb2phy",
		.driver_data	= (unsigned long)&usb2phy_exynos5250,
	}, {
		.name		= "exynos7420-usb2phy",
		.driver_data	= (unsigned long)&usb2phy_exynos7420,
	}, {
		.name		= "exynos5-usb2phy",
		.driver_data	= (unsigned long)&usb2phy_exynos5,
	},
	{},
};

MODULE_DEVICE_TABLE(platform, samsung_usbphy_driver_ids);

static struct platform_driver samsung_usb2phy_driver = {
	.probe		= samsung_usb2phy_probe,
	.remove		= samsung_usb2phy_remove,
	.id_table	= samsung_usbphy_driver_ids,
	.driver		= {
		.name	= "samsung-usb2phy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_usbphy_dt_match),
		.pm	= &samsung_usb2phy_pm_ops,
	},
};

module_platform_driver(samsung_usb2phy_driver);

MODULE_DESCRIPTION("Samsung USB 2.0 phy controller");
MODULE_AUTHOR("Praveen Paneri <p.paneri@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:samsung-usb2phy");
