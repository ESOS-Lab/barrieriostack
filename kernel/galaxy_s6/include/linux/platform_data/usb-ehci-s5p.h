/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __PLAT_SAMSUNG_EHCI_H
#define __PLAT_SAMSUNG_EHCI_H __FILE__

struct s5p_ehci_platdata {
	int (*phy_init)(struct platform_device *pdev, int type);
	int (*phy_exit)(struct platform_device *pdev, int type);
	int (*phy_suspend)(struct platform_device *pdev, int type);
	int (*phy_resume)(struct platform_device *pdev, int type);

	unsigned long hsic_ports;	/* bit vector (one bit per port) */
	unsigned has_synopsys_hsic_bug:1;	/* Synopsys HSIC port */
};

extern void s5p_ehci_set_platdata(struct s5p_ehci_platdata *pd);
#if defined(CONFIG_MDM_HSIC_PM)
extern int usb_phy_prepare_shutdown(void);
extern int usb_phy_prepare_wakeup(void);
#endif

#endif /* __PLAT_SAMSUNG_EHCI_H */
