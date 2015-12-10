/*
 * SAMSUNG EXYNOS5433 Flattened Device Tree enabled machine
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
/* for mipi-lli reserve */
#ifdef CONFIG_MIPI_LLI
#include <linux/mipi-lli.h>
#endif

static void __init espresso5433_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
}

static void __init espresso5433_dt_machine_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static char const *espresso5433_dt_compat[] __initdata = {
	"samsung,exynos5433",
	NULL
};

static void __init espresso5433_reserve(void)
{
#ifdef CONFIG_MIPI_LLI
	/* mipi-lli */
	lli_phys_addr = memblock_alloc_base(MIPI_LLI_RESERVE_SIZE,
			MIPI_LLI_RESERVE_SIZE, MEMBLOCK_ALLOC_ANYWHERE);
	pr_info("memblock_reserve: [%#08lx-%#08lx] for mipi-lli\n",
			(unsigned long)lli_phys_addr,
			(unsigned long)lli_phys_addr + MIPI_LLI_RESERVE_SIZE);
#endif
	init_exynos_ion_contig_heap();
}

DT_MACHINE_START(ESPRESSO5433, "ESPRESSO5433")
	.init_irq	= exynos5_init_irq,
	.smp		= smp_ops(exynos_smp_ops),
	.map_io		= espresso5433_dt_map_io,
	.init_machine	= espresso5433_dt_machine_init,
	.init_late	= exynos_init_late,
	.init_time	= exynos_init_time,
	.dt_compat	= espresso5433_dt_compat,
	.restart        = exynos5_restart,
	.reserve	= espresso5433_reserve,
MACHINE_END
