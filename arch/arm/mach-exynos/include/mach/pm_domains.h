/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARCH_PM_RUNTIME_H
#define __ASM_ARCH_PM_RUNTIME_H __FILE__

#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/clk-private.h>
#include <linux/sched.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/bts.h>

#include <plat/devs.h>

#define PM_DOMAIN_PREFIX	"PM DOMAIN: "

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifdef CONFIG_PM_DOMAIN_DEBUG
#define DEBUG_PRINT_INFO(fmt, ...) printk(PM_DOMAIN_PREFIX pr_fmt(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INFO(fmt, ...)
#endif

/* In Exynos, the number of MAX_POWER_DOMAIN is less than 15 */
#define MAX_PARENT_POWER_DOMAIN	15

struct exynos_pm_domain;

struct exynos_pd_callback {
	const char *name;
	int (*init)(struct exynos_pm_domain *pd);
	int (*on_pre)(struct exynos_pm_domain *pd);
	int (*on)(struct exynos_pm_domain *pd, int power_flags);
	int (*on_post)(struct exynos_pm_domain *pd);
	int (*off_pre)(struct exynos_pm_domain *pd);
	int (*off)(struct exynos_pm_domain *pd, int power_flags);
	int (*off_post)(struct exynos_pm_domain *pd);
	unsigned int status;
};

struct exynos_pm_domain {
	struct generic_pm_domain genpd;
	char const *name;
	void __iomem *base;
	int (*on)(struct exynos_pm_domain *pd, int power_flags);
	int (*off)(struct exynos_pm_domain *pd, int power_flags);
	int (*check_status)(struct exynos_pm_domain *pd);
	struct exynos_pd_callback *cb;
	unsigned int status;
	unsigned int pd_option;
#if defined(CONFIG_SOC_EXYNOS7420)
	unsigned int pd_timeout;
#endif
#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5422_BTS)	\
	|| defined(CONFIG_EXYNOS5433_BTS) || defined(CONFIG_EXYNOS7420_BTS)
	unsigned int bts;
#endif

	struct mutex access_lock;
	void *priv;
#if defined(CONFIG_SOC_EXYNOS7420)
	void *pd_data;
#endif
};

#if defined(CONFIG_SOC_EXYNOS7420)
struct exynos_pd_clk {
	void __iomem *reg;
	u8 bit_offset;
	struct clk *clock;
	void __iomem *belonged_pd;
};

struct exynos_pd_reg {
	void __iomem *reg;
	u8 bit_offset;
};
#endif

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd);
#if defined(CONFIG_SOC_EXYNOS5433)
int exynos_pd_clk_get(struct exynos_pm_domain *pd);
#endif
#if defined(CONFIG_SOC_EXYNOS7420)
struct exynos7420_pd_data *exynos_pd_find_data(struct exynos_pm_domain *pd);
#endif

#endif /* __ASM_ARCH_PM_RUNTIME_H */
