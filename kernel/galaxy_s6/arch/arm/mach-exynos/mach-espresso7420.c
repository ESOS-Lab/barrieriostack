/*
 * SAMSUNG EXYNOS5430 Flattened Device Tree enabled machine
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/clocksource.h>
#include <linux/exynos_ion.h>

#include <asm/mach/arch.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>
#include <plat/mfc.h>

#include "common.h"

const struct of_device_id of_iommu_bus_match_table[] = {
	{ .compatible = "samsung,exynos-iommu-bus", },
	{} /* Empty terminated list */
};

static void __init espresso7420_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
}

static void __init espresso7420_dt_machine_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
	of_platform_bus_probe(NULL, of_iommu_bus_match_table, NULL);
}

static char const *espresso7420_dt_compat[] __initdata = {
	"samsung,exynos7420",
	NULL
};

static void __init espresso7420_reserve(void)
{
	init_exynos_ion_contig_heap();
}

DT_MACHINE_START(ESPRESSO7420, "ESPRESSO7420")
	.init_irq	= exynos5_init_irq,
	.smp		= smp_ops(exynos_smp_ops),
	.map_io		= espresso7420_dt_map_io,
	.init_machine	= espresso7420_dt_machine_init,
	.init_late	= exynos_init_late,
	.init_time	= exynos_init_time,
	.dt_compat	= espresso7420_dt_compat,
	.restart        = exynos5_restart,
	.reserve	= espresso7420_reserve,
MACHINE_END
