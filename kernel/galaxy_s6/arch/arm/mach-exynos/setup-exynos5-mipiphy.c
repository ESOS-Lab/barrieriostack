/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <mach/regs-clock.h>

#define MIPI_PHY_BIT0					(1 << 0)
#define MIPI_PHY_BIT1					(1 << 1)

#if defined(CONFIG_SOC_EXYNOS5422)
static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	void __iomem *addr_phy;
	u32 cfg;

	addr_phy = S5P_MIPI_DPHY_CONTROL(id);

	cfg = __raw_readl(addr_phy);
	cfg = (cfg | S5P_MIPI_DPHY_SRESETN);

	__raw_writel(cfg, addr_phy);

	if (1) {
		cfg |= S5P_MIPI_DPHY_ENABLE;
	} else if (!(cfg & (S5P_MIPI_DPHY_SRESETN | S5P_MIPI_DPHY_MRESETN)
			& (~S5P_MIPI_DPHY_SRESETN))) {
		cfg &= ~S5P_MIPI_DPHY_ENABLE;
	}

	__raw_writel(cfg, addr_phy);

	return 0;
}
#elif (CONFIG_SOC_EXYNOS7420)

static u32 dphy_m4s4_status = 0;

static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	int ret = 0;
	static DEFINE_SPINLOCK(lock);
	void __iomem *pmu_addr;
	void __iomem *cmu_addr;
	unsigned long flags;
	u32 cfg;

	spin_lock_irqsave(&lock, flags);

	if (reset == S5P_MIPI_DPHY_SRESETN) {
		if (on) {
			switch(id) {
			case 0:
				++dphy_m4s4_status;
				if (dphy_m4s4_status == 1) {
					cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
					cfg = __raw_readl(cmu_addr);

					/* enable reset -> release reset */
					cfg &= ~(1 << 0);
					__raw_writel(cfg, cmu_addr);
					cfg |= (1 << 0);
					__raw_writel(cfg, cmu_addr);

					pmu_addr = S7P_MIPI_DPHY_CONTROL(0);
					__raw_writel(S5P_MIPI_DPHY_ENABLE, pmu_addr);
				}
				break;
			case 1:
				cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 8);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 8);
				__raw_writel(cfg, cmu_addr);

				pmu_addr = S7P_MIPI_DPHY_CONTROL(2);
				__raw_writel(S5P_MIPI_DPHY_ENABLE, pmu_addr);
				break;
			case 2:
				cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 12);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 12);
				__raw_writel(cfg, cmu_addr);

				pmu_addr = S7P_MIPI_DPHY_CONTROL(3);
				__raw_writel(S5P_MIPI_DPHY_ENABLE, pmu_addr);
				break;
			default:
				pr_err("id(%d) is invalid", id);
				ret =  -EINVAL;
				goto p_err;
				break;
			}
		} else {
			switch(id) {
			case 0:
				--dphy_m4s4_status;
				if (dphy_m4s4_status == 0) {
					cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
					cfg = __raw_readl(cmu_addr);

					cfg &= ~(1 << 0);
					__raw_writel(cfg, cmu_addr);
					cfg |= (1 << 0);
					__raw_writel(cfg, cmu_addr);

					pmu_addr = S7P_MIPI_DPHY_CONTROL(0);
					__raw_writel(0, pmu_addr);
				}
				break;
			case 1:
				cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 8);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 8);
				__raw_writel(cfg, cmu_addr);

				pmu_addr = S7P_MIPI_DPHY_CONTROL(2);
				__raw_writel(0, pmu_addr);
				break;
			case 2:
				cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 12);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 12);
				__raw_writel(cfg, cmu_addr);

				pmu_addr = S7P_MIPI_DPHY_CONTROL(3);
				__raw_writel(0, pmu_addr);
				break;
			default:
				pr_err("id(%d) is invalid", id);
				ret =  -EINVAL;
				goto p_err;
				break;
			}
		}
	} else { /* reset ==  S5P_MIPI_DPHY_MRESETN */
		if (on) {
			switch(id) {
			case 0:
				++dphy_m4s4_status;
				if (dphy_m4s4_status == 1) {
					cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
					cfg = __raw_readl(cmu_addr);

					cfg &= ~(1 << 0);
					__raw_writel(cfg, cmu_addr);
					cfg |= (1 << 0);
					__raw_writel(cfg, cmu_addr);

					pmu_addr = S7P_MIPI_DPHY_CONTROL(0);
					__raw_writel(S5P_MIPI_DPHY_ENABLE, pmu_addr);
				}
				break;
			case 1:
				cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 4);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 4);
				__raw_writel(cfg, cmu_addr);

				pmu_addr = S7P_MIPI_DPHY_CONTROL(1);
				__raw_writel(S5P_MIPI_DPHY_ENABLE, pmu_addr);
				break;
			default:
				pr_err("id(%d) is invalid", id);
				ret =  -EINVAL;
				goto p_err;
				break;
			}
		} else { /* off */
			switch(id) {
			case 0:
				--dphy_m4s4_status;
				if (dphy_m4s4_status == 0) {
					cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
					cfg = __raw_readl(cmu_addr);

					cfg &= ~(1 << 0);
					__raw_writel(cfg, cmu_addr);
					cfg |= (1 << 0);
					__raw_writel(cfg, cmu_addr);

					pmu_addr = S7P_MIPI_DPHY_CONTROL(0);
					__raw_writel(0, pmu_addr);
				}
				break;
			case 1:
				cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 4);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 4);
				__raw_writel(cfg, cmu_addr);

				pmu_addr = S7P_MIPI_DPHY_CONTROL(1);
				__raw_writel(0, pmu_addr);
				break;
			default:
				pr_err("id(%d) is invalid", id);
				ret =  -EINVAL;
				goto p_err;
				break;
			}
		}
	}

p_err:
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}
#else

static __inline__ u32 exynos5_phy0_is_running(u32 reset)
{
	u32 ret = 0;

	/* When you try to disable DSI, CHECK CAM0 PD STATUS */
	if (reset == S5P_MIPI_DPHY_MRESETN) {
		if (readl(EXYNOS5430_CAM0_STATUS) & 0x1)
			ret = __raw_readl(S5P_VA_SYSREG_CAM0 + 0x1014) & MIPI_PHY_BIT0;
	/* When you try to disable CSI, CHECK DISP PD STATUS */
	} else if (reset == S5P_MIPI_DPHY_SRESETN) {
		if (readl(EXYNOS5430_DISP_STATUS) & 0x1)
			ret = __raw_readl(S5P_VA_SYSREG_DISP + 0x000C) & MIPI_PHY_BIT0;
	}

	return ret;
}

static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	static DEFINE_SPINLOCK(lock);
	void __iomem *addr_phy;
	void __iomem *addr_reset;
	unsigned long flags;
	u32 cfg;

	addr_phy = S5P_MIPI_DPHY_CONTROL(id);

	spin_lock_irqsave(&lock, flags);

	/* PHY PMU enable */
	if (on) {
		cfg = __raw_readl(addr_phy);
		cfg |= S5P_MIPI_DPHY_ENABLE;
		__raw_writel(cfg, addr_phy);
	}

	/* PHY reset */
	switch(id) {
	case 0:
		if (reset == S5P_MIPI_DPHY_SRESETN) {
			if (readl(EXYNOS5430_CAM0_STATUS) & 0x1) {
				addr_reset = S5P_VA_SYSREG_CAM0 + 0x1014;
				cfg = __raw_readl(addr_reset);
				cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
			}
		} else {
			if (readl(EXYNOS5430_DISP_STATUS) & 0x1) {
				addr_reset = S5P_VA_SYSREG_DISP + 0x000c;
				cfg = __raw_readl(addr_reset);

				/* 0: enable reset, 1: release reset */
				cfg = (cfg & ~MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
				cfg = (cfg | MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
			}
		}
		break;
	case 1:
		if (readl(EXYNOS5430_CAM0_STATUS) & 0x1) {
			addr_reset = S5P_VA_SYSREG_CAM0 + 0x1014;
			cfg = __raw_readl(addr_reset);
			cfg = on ? (cfg | MIPI_PHY_BIT1) : (cfg & ~MIPI_PHY_BIT1);
			__raw_writel(cfg, addr_reset);
		}
		break;
	case 2:
		if (readl(EXYNOS5430_CAM1_STATUS) & 0x1) {
			addr_reset = S5P_VA_SYSREG_CAM1 + 0x1020;
			cfg = __raw_readl(addr_reset);
			cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
			__raw_writel(cfg, addr_reset);
		}
		break;
	default:
		pr_err("id(%d) is invalid", id);
		spin_unlock_irqrestore(&lock, flags);
		return -EINVAL;
	}

	/* PHY PMU disable */
	if (!on) {
		cfg = __raw_readl(addr_phy);
		if (id == 0) {
			if (!exynos5_phy0_is_running(reset))
				cfg &= ~S5P_MIPI_DPHY_ENABLE;
		} else {
			cfg &= ~S5P_MIPI_DPHY_ENABLE;
		}
		__raw_writel(cfg, addr_phy);
	}

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}
#endif

int exynos5_csis_phy_enable(int id, bool on)
{
	return __exynos5_mipi_phy_control(id, on, S5P_MIPI_DPHY_SRESETN);
}
EXPORT_SYMBOL(exynos5_csis_phy_enable);

int exynos5_dism_phy_enable(int id, bool on)
{
	return __exynos5_mipi_phy_control(id, on, S5P_MIPI_DPHY_MRESETN);
}
EXPORT_SYMBOL(exynos5_dism_phy_enable);
