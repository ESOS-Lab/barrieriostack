/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_BINARY_H
#define FIMC_IS_BINARY_H

#define SDCARD_FW

#define VENDER_PATH

#ifdef VENDER_PATH
#define FIMC_IS_FW_PATH			"/system/vendor/firmware/"
#define FIMC_IS_FW_DUMP_PATH			"/data/"
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_FW_SDCARD			"/data/media/0/fimc_is_fw.bin"
#define FIMC_IS_FW				"fimc_is_fw.bin"
#else
#define FIMC_IS_FW_PATH			"/data/"
#define FIMC_IS_FW_DUMP_PATH			"/data/"
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/"
#define FIMC_IS_FW_SDCARD			"/data/fimc_is_fw2.bin"
#define FIMC_IS_FW				"fimc_is_fw2.bin"
#endif

#define FIMC_IS_FW_BASE_MASK			((1 << 26) - 1)
#define FIMC_IS_VERSION_SIZE			42
#define FIMC_IS_SETFILE_VER_OFFSET		0x40
#define FIMC_IS_SETFILE_VER_SIZE		52

#define FIMC_IS_CAL_SDCARD			"/data/cal_data.bin"
#define FIMC_IS_CAL_RETRY_CNT			(2)
#define FIMC_IS_FW_RETRY_CNT			(2)

#if defined(CONFIG_SOC_EXYNOS7420)
#define FIMC_IS_CAL_START_ADDR			(0x01FD0000)
#define FIMC_IS_CAL_START_ADDR_FRONT		(0x01FE0000)
#else
#define FIMC_IS_CAL_START_ADDR			(0x013D0000)
#define FIMC_IS_CAL_START_ADDR_FRONT		(0x013E0000)
#endif

enum fimc_is_bin_type {
	FIMC_IS_BIN_FW = 0,
	FIMC_IS_BIN_SETFILE,
};

#endif
