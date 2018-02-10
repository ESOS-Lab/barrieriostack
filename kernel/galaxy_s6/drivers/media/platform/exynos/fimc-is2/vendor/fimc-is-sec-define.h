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

#ifndef FIMC_IS_SEC_DEFINE_H
#define FIMC_IS_SEC_DEFINE_H

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/regulator/consumer.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"

#include "fimc-is-device-sensor.h"
#include "fimc-is-device-ischain.h"
#include "crc32.h"
#include "fimc-is-companion.h"

#define FW_CORE_VER		0
#define FW_PIXEL_SIZE		1
#define FW_ISP_COMPANY		3
#define FW_SENSOR_MAKER		4
#define FW_PUB_YEAR		5
#define FW_PUB_MON		6
#define FW_PUB_NUM		7
#define FW_MODULE_COMPANY	9
#define FW_VERSION_INFO		10

#define FW_ISP_COMPANY_BROADCOMM	'B'
#define FW_ISP_COMPANY_TN		'C'
#define FW_ISP_COMPANY_FUJITSU		'F'
#define FW_ISP_COMPANY_INTEL		'I'
#define FW_ISP_COMPANY_LSI		'L'
#define FW_ISP_COMPANY_MARVELL		'M'
#define FW_ISP_COMPANY_QUALCOMM		'Q'
#define FW_ISP_COMPANY_RENESAS		'R'
#define FW_ISP_COMPANY_STE		'S'
#define FW_ISP_COMPANY_TI		'T'
#define FW_ISP_COMPANY_DMC		'D'

#define FW_SENSOR_MAKER_SF		'F'
#define FW_SENSOR_MAKER_SLSI		'L'
#define FW_SENSOR_MAKER_SONY		'S'

#define FW_MODULE_COMPANY_SEMCO		'S'
#define FW_MODULE_COMPANY_GUMI		'O'
#define FW_MODULE_COMPANY_CAMSYS	'C'
#define FW_MODULE_COMPANY_PATRON	'P'
#define FW_MODULE_COMPANY_MCNEX		'M'
#define FW_MODULE_COMPANY_LITEON	'L'
#define FW_MODULE_COMPANY_VIETNAM	'V'
#define FW_MODULE_COMPANY_SHARP		'J'
#define FW_MODULE_COMPANY_NAMUGA	'N'
#define FW_MODULE_COMPANY_POWER_LOGIX	'A'
#define FW_MODULE_COMPANY_DI		'D'

#define FW_2P2_F		"F16LL"
#define FW_2P2_I		"I16LL"
#define FW_2P2_A		"A16LL"
#define FW_3L2		"C13LL"
#define FW_IMX135	"C13LS"
#define FW_IMX134	"D08LS"
#define FW_IMX240	"A16LS"
#define FW_IMX240_Q	"H16US"
#define FW_IMX240_Q_C1	"H16UL"
#define FW_2P2_12M	"G16LL"
#define FW_4H5		"F08LL"
#define FW_2P3		"J16LL"
#define FW_2T2		"A20LL"

#define SDCARD_FW
#define FIMC_IS_FW_2P2				"fimc_is_fw2_2p2.bin"
#define FIMC_IS_FW_2P2_12M				"fimc_is_fw2_2p2_12m.bin"
#define FIMC_IS_FW_2P3				"fimc_is_fw2_2p3.bin"
#define FIMC_IS_FW_3L2				"fimc_is_fw2_3l2.bin"
#define FIMC_IS_FW_4H5				"fimc_is_fw2_4h5.bin"
#define FIMC_IS_FW_IMX134			"fimc_is_fw2_imx134.bin"
#define FIMC_IS_FW_IMX240		"fimc_is_fw2_imx240.bin"
#define FIMC_IS_FW_2T2_EVT0				"fimc_is_fw2_2t2_evt0.bin"
#define FIMC_IS_FW_2T2_EVT1				"fimc_is_fw2_2t2_evt1.bin"
#define FIMC_IS_FW_COMPANION_EVT0				"companion_fw_evt0.bin"
#define FIMC_IS_FW_COMPANION_EVT1				"companion_fw_evt1.bin"
#define FIMC_IS_FW_COMPANION_2P2_EVT1				"companion_fw_2p2_evt1.bin"
#define FIMC_IS_FW_COMPANION_2T2_EVT1				"companion_fw_2t2_evt1.bin"
#define FIMC_IS_FW_COMPANION_2P2_12M_EVT1				"companion_fw_2p2_12m_evt1.bin"
#define FIMC_IS_FW_COMPANION_IMX240_EVT1				"companion_fw_imx240_evt1.bin"
#define FIMC_IS_FW_SDCARD			"/data/media/0/fimc_is_fw.bin"
#define FIMC_IS_IMX240_SETF			"setfile_imx240.bin"
#define FIMC_IS_IMX135_SETF			"setfile_imx135.bin"
#define FIMC_IS_IMX134_SETF			"setfile_imx134.bin"
#define FIMC_IS_4H5_SETF			"setfile_4h5.bin"
#define FIMC_IS_3L2_SETF			"setfile_3l2.bin"
#define FIMC_IS_6B2_SETF			"setfile_6b2.bin"
#define FIMC_IS_8B1_SETF			"setfile_8b1.bin"
#define FIMC_IS_6D1_SETF			"setfile_6d1.bin"
#define FIMC_IS_2P2_SETF			"setfile_2p2.bin"
#define FIMC_IS_2P2_12M_SETF			"setfile_2p2_12m.bin"
#define FIMC_IS_2T2_SETF			"setfile_2t2.bin"
#define FIMC_IS_2P3_SETF			"setfile_2p3.bin"
#define FIMC_IS_COMPANION_MASTER_SETF			"companion_master_setfile.bin"
#define FIMC_IS_COMPANION_MODE_SETF			"companion_mode_setfile.bin"
#define FIMC_IS_COMPANION_2P2_MASTER_SETF			"companion_2p2_master_setfile.bin"
#define FIMC_IS_COMPANION_2P2_MODE_SETF			"companion_2p2_mode_setfile.bin"
#define FIMC_IS_COMPANION_IMX240_MASTER_SETF			"companion_imx240_master_setfile.bin"
#define FIMC_IS_COMPANION_IMX240_MODE_SETF			"companion_imx240_mode_setfile.bin"
#define FIMC_IS_COMPANION_2P2_12M_MASTER_SETF			"companion_2p2_12m_master_setfile.bin"
#define FIMC_IS_COMPANION_2P2_12M_MODE_SETF			"companion_2p2_12m_mode_setfile.bin"
#define FIMC_IS_COMPANION_2T2_MASTER_SETF			"companion_2t2_master_setfile.bin"
#define FIMC_IS_COMPANION_2T2_MODE_SETF			"companion_2t2_mode_setfile.bin"

#define FIMC_IS_CAL_SDCARD_FRONT		"/data/cal_data_front.bin"

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
#define FIMC_IS_MAX_CAL_SIZE	(8 * 1024)
#else
#define FIMC_IS_MAX_CAL_SIZE	(64 * 1024)
#endif
#define FIMC_IS_MAX_CAL_SIZE_FRONT	(16 * 1024)
#define FIMC_IS_MAX_COMPANION_FW_SIZE		(200 * 1024)

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
#define FIMC_IS_MAX_FW_SIZE			(8 * 1024)
#define FIMC_IS_MAX_SETFILE_SIZE	(1120 * 1024)
#define HEADER_CRC32_LEN			(80)
#define OEM_CRC32_LEN				(64)
#define AWB_CRC32_LEN				(32)
#define SHADING_CRC32_LEN			(6623)
#else
#define FIMC_IS_MAX_FW_SIZE			(2750 * 1024)
#define FIMC_IS_MAX_SETFILE_SIZE	(1120 * 1024)
#define HEADER_CRC32_LEN (224)
#define OEM_CRC32_LEN (192)
#define AWB_CRC32_LEN (32)
#define SHADING_CRC32_LEN (2336)
#endif

#define HEADER_CRC32_LEN_FRONT          (0x70)
#define OEM_START_ADDR_FRONT            (0x0)
#define OEM_END_ADDR_FRONT              (0x04)
#define AWB_START_ADDR_FRONT            (0x08)
#define AWB_END_ADDR_FRONT              (0x0C)
#define SHADING_START_ADDR_FRONT        (0x18)
#define SHADING_END_ADDR_FRONT          (0x1C)
#define C2_SHADING_START_ADDR_FRONT     (0x10)
#define C2_SHADING_END_ADDR_FRONT       (0x14)
#define CAL_HEADER_VER_ADDR_FRONT       (0x30)
#define CAL_MAP_VER_ADDR_FRONT          (0x40)
#define PROJECT_NAME_ADDR_FRONT         (0x4C)
#define OEM_VER_ADDR_FRONT              (0x150)
#define AWB_VER_ADDR_FRONT              (0x220)
#define SHADING_VER_ADDR_FRONT          (0x3B00)
#define C2_SHADING_VER_ADDR_FRONT       (0x1D00)
#define CAL_MAP_VER_ADDR_FRONT          (0x40)

#define OEM_START_ADDR            (0x0)
#define OEM_END_ADDR              (0x04)
#define AWB_START_ADDR            (0x08)
#define AWB_END_ADDR              (0x0C)
#define SHADING_START_ADDR        (0x10)
#define SHADING_END_ADDR          (0x14)
#define CAL_HEADER_VER_ADDR       (0x20)
#define CAL_MAP_VER_ADDR          (0x30)
#define PROJECT_NAME_ADDR         (0x38)
#define OEM_VER_ADDR              (0x150)
#define AWB_VER_ADDR              (0x220)
#define SHADING_VER_ADDR          (0x1CE0)
#define CAL_MAP_VER_ADDR          (0x30)

#define SETFILE_SIZE	0x6000
#define READ_SIZE	0x100

#define FROM_VERSION_V002 '2'
#define FROM_VERSION_V003 '3'
#define FROM_VERSION_V004 '4'
#define FROM_VERSION_V005 '5'

enum {
        CC_BIN1 = 0,
        CC_BIN2,
        CC_BIN3,
        CC_BIN4,
        CC_BIN5,
        CC_BIN6,
        CC_BIN7,
        CC_BIN_MAX,
};

#ifdef CONFIG_COMPANION_USE
enum fimc_is_companion_sensor {
	COMPANION_SENSOR_2P2 = 1,
	COMPANION_SENSOR_IMX240 = 2,
	COMPANION_SENSOR_2T2 = 3
};
#endif

struct fimc_is_from_info {
	u32		bin_start_addr;
	u32		bin_end_addr;
	u32		oem_start_addr;
	u32		oem_end_addr;
	u32		awb_start_addr;
	u32		awb_end_addr;
	u32		shading_start_addr;
	u32		shading_end_addr;
	u32		setfile_start_addr;
	u32		setfile_end_addr;
	char	header_ver[12];
	char	cal_map_ver[5];
	char	setfile_ver[7];
	char	oem_ver[12];
	char	awb_ver[12];
	char	shading_ver[12];
	char	load_fw_name[50];
	char	load_setfile_name[50];
	char	project_name[9];
	bool	is_caldata_read;
#ifdef CONFIG_COMPANION_USE
	u32		concord_master_setfile_start_addr;
	u32		concord_master_setfile_end_addr;
	u32		concord_mode_setfile_start_addr;
	u32		concord_mode_setfile_end_addr;
	u32		lsc_gain_start_addr;
	u32		lsc_gain_end_addr;
	u32		pdaf_start_addr;
	u32		pdaf_end_addr;
	u32		coefficient_cal_start_addr;
	u32		coefficient_cal_end_addr;
	u32		pdaf_cal_start_addr;
	u32		pdaf_cal_end_addr;
	u32		concord_cal_start_addr;
	u32		concord_cal_end_addr;
	u32		concord_bin_start_addr;
	u32		concord_bin_end_addr;
	u32		lsc_i0_gain_addr;
	u32		lsc_j0_gain_addr;
	u32		lsc_a_gain_addr;
	u32		lsc_k4_gain_addr;
	u32		lsc_scale_gain_addr;
	u32		pdaf_crc_addr;
	u32		lsc_gain_crc_addr;
	u32		sensor_id;
#ifdef CONFIG_COMPANION_C1_USE
	u32		wcoefficient1_addr;
	u32		coef1_start;
	u32		coef1_end;
	u32		coef2_start;
	u32		coef2_end;
	u32		coef3_start;
	u32		coef3_end;
	u32		coef4_start;
	u32		coef4_end;
	u32		coef5_start;
	u32		coef5_end;
	u32		coef6_start;
	u32		coef6_end;
	u32		af_inf_addr;
	u32		af_macro_addr;
	u32		coef1_crc_addr;
	u32		coef2_crc_addr;
	u32		coef3_crc_addr;
	u32		coef4_crc_addr;
	u32		coef5_crc_addr;
	u32		coef6_crc_addr;
#endif
#ifdef CONFIG_COMPANION_C2_USE
	u32		grasTuning_AwbAshCord_N_addr;
	u32		grasTuning_awbAshCordIndexes_N_addr;
	u32		grasTuning_GASAlpha_M__N_addr;
	u32		grasTuning_GASBeta_M__N_addr;
	u32		grasTuning_GASOutdoorAlpha_N_addr;
	u32		grasTuning_GASOutdoorBeta_N_addr;
	u32		grasTuning_GASIndoorAlpha_N_addr;
	u32		grasTuning_GASIndoorBeta_N_addr;
	u32		xtalk_coef_start;
	u32		coef_offset_R;
	u32		coef_offset_G;
	u32		coef_offset_B;
	u32		pdaf_shad_start_addr;
	u32		pdaf_shad_end_addr;
	u32		pdaf_shad_crc_addr;
	u32		xtalk_coef_crc_addr;
	u32		lsc_parameter_crc_addr;
	u32		c2_shading_start_addr;
	u32		c2_shading_end_addr;
	char		c2_shading_ver[12];
#endif
	char		concord_header_ver[12];
	char		load_c1_fw_name[50];
	char		load_c1_mastersetf_name[50];
	char		load_c1_modesetf_name[50];
	bool		is_c1_caldata_read;
#endif
};

bool fimc_is_sec_get_force_caldata_dump(void);
int fimc_is_sec_set_force_caldata_dump(bool fcd);

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos);
bool fimc_is_sec_file_exist(char *name);

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo);
int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_get_sysfs_finfo_front(struct fimc_is_from_info **finfo);
int fimc_is_sec_get_sysfs_pinfo_front(struct fimc_is_from_info **pinfo);
int fimc_is_sec_get_front_cal_buf(char **buf);
#endif

int fimc_is_sec_get_cal_buf(char **buf);
int fimc_is_sec_get_loaded_fw(char **buf);
int fimc_is_sec_get_loaded_c1_fw(char **buf);

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name);
int fimc_is_sec_get_camid(void);
int fimc_is_sec_set_camid(int id);
int fimc_is_sec_get_pixel_size(char *header_ver);
int fimc_is_sec_fw_find(struct fimc_is_core *core);
int fimc_is_sec_run_fw_sel(struct device *dev, int position);

int fimc_is_sec_readfw(struct fimc_is_core *core);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_fw_sel_eeprom(struct device *dev, int id, bool headerOnly);
#endif
#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_readcal(struct fimc_is_core *core);
int fimc_is_sec_fw_sel(struct fimc_is_core *core, struct device *dev, bool headerOnly);
#endif
#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_concord_fw_sel(struct fimc_is_core *core, struct device *dev);
#endif
int fimc_is_sec_fw_revision(char *fw_ver);
int fimc_is_sec_fw_revision(char *fw_ver);
bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2);
u8 fimc_is_sec_compare_ver(int position);
bool fimc_is_sec_check_from_ver(struct fimc_is_core *core, int position);

bool fimc_is_sec_check_fw_crc32(char *buf);
bool fimc_is_sec_check_cal_crc32(char *buf, int id);
void fimc_is_sec_make_crc32_table(u32 *table, u32 id);

int fimc_is_sec_gpio_enable(struct exynos_platform_fimc_is *pdata, char *name, bool on);
int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver);
int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on);
#endif /* FIMC_IS_SEC_DEFINE_H */
