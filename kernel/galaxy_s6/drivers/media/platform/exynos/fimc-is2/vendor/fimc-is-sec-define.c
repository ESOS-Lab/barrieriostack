/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pinctrl/pinctrl-samsung.h>

#include "fimc-is-sec-define.h"

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
#include <linux/i2c.h>
#include "fimc-is-device-eeprom.h"
#endif

bool crc32_fw_check = true;
bool crc32_setfile_check = true;
bool crc32_check = true;
bool crc32_check_factory = true;
bool crc32_header_check = true;
bool crc32_header_check_front = true;
bool crc32_check_factory_front = true;
bool fw_version_crc_check = true;
bool is_latest_cam_module = false;
bool is_final_cam_module = false;
#if defined(CONFIG_SOC_EXYNOS5433)
bool is_right_prj_name = true;
#endif
#ifdef CONFIG_COMPANION_USE
bool crc32_c1_fw_check = true;
bool crc32_c1_check = true;
bool crc32_c1_check_factory = true;
bool companion_lsc_isvalid = false;
bool companion_coef_isvalid = false;
#ifdef CONFIG_COMPANION_C2_USE
bool companion_front_lsc_isvalid = false;
#endif
#endif

#define FIMC_IS_DEFAULT_CAL_SIZE	(20 * 1024)
#define FIMC_IS_DUMP_CAL_SIZE	(172 * 1024)
#define FIMC_IS_LATEST_FROM_VERSION_A	'A'
#define FIMC_IS_LATEST_FROM_VERSION_B	'B'
#define FIMC_IS_LATEST_FROM_VERSION_C	'C'
#define FIMC_IS_LATEST_FROM_VERSION_D	'D'
#define FIMC_IS_LATEST_FROM_VERSION_M	'M'

//static bool is_caldata_read = false;
//static bool is_c1_caldata_read = false;
static bool force_caldata_dump = false;
static int cam_id = CAMERA_SINGLE_REAR;
bool is_dumped_fw_loading_needed = false;
bool is_dumped_c1_fw_loading_needed = false;
char fw_core_version;
//struct class *camera_class;
//struct device *camera_front_dev; /*sys/class/camera/front*/
//struct device *camera_rear_dev; /*sys/class/camera/rear*/
static struct fimc_is_from_info sysfs_finfo;
static struct fimc_is_from_info sysfs_pinfo;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
bool crc32_check_front = true;
bool is_final_cam_module_front = false;
static struct fimc_is_from_info sysfs_finfo_front;
static struct fimc_is_from_info sysfs_pinfo_front;
static char cal_buf_front[FIMC_IS_MAX_CAL_SIZE_FRONT];
#endif

static char cal_buf[FIMC_IS_MAX_CAL_SIZE];
static char fw_buf[FIMC_IS_MAX_FW_SIZE];
char loaded_fw[12] = {0, };
char loaded_companion_fw[30] = {0, };

bool fimc_is_sec_get_force_caldata_dump(void)
{
	return force_caldata_dump;
}

int fimc_is_sec_set_force_caldata_dump(bool fcd)
{
	force_caldata_dump = fcd;
	if (fcd)
		info("forced caldata dump enabled!!\n");
	return 0;
}

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo)
{
	*finfo = &sysfs_finfo;
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo;
	return 0;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_get_sysfs_finfo_front(struct fimc_is_from_info **finfo)
{
	*finfo = &sysfs_finfo_front;
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo_front(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo_front;
	return 0;
}

int fimc_is_sec_get_front_cal_buf(char **buf)
{
	*buf = &cal_buf_front[0];
	return 0;
}
#endif

int fimc_is_sec_get_cal_buf(char **buf)
{
	*buf = &cal_buf[0];
	return 0;
}

int fimc_is_sec_get_loaded_fw(char **buf)
{
	*buf = &loaded_fw[0];
	return 0;
}

int fimc_is_sec_get_loaded_c1_fw(char **buf)
{
	*buf = &loaded_companion_fw[0];
	return 0;
}

int fimc_is_sec_set_camid(int id)
{
	cam_id = id;
	return 0;
}

int fimc_is_sec_get_camid(void)
{
	return cam_id;
}

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name)
{
#if 0
	char buf[1];
	loff_t pos = 0;
	int pixelSize;

	read_data_from_file("/data/CameraID.txt", buf, 1, &pos);
	if (buf[0] == '0')
		cam_id = CAMERA_SINGLE_REAR;
	else if (buf[0] == '1')
		cam_id = CAMERA_SINGLE_FRONT;
	else if (buf[0] == '2')
		cam_id = CAMERA_DUAL_REAR;
	else if (buf[0] == '3')
		cam_id = CAMERA_DUAL_FRONT;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
		snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX135)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
		snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX134)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
		snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
	} else {
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
		if (pixelSize == 13) {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		} else if (pixelSize == 8) {
			snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		}
	}

	if (cam_id == CAMERA_SINGLE_FRONT ||
		cam_id == CAMERA_DUAL_FRONT) {
		snprintf(setf_name, sizeof(FIMC_IS_6B2_SETF), "%s", FIMC_IS_6B2_SETF);
	}
#else
	err("%s: waring, you're calling the disabled func!", __func__);
#endif
	return 0;
}

int fimc_is_sec_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_PUB_YEAR] - 58) * 10000;
	revision = revision + ((int)fw_ver[FW_PUB_MON] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_PUB_NUM] - 48) * 10;
	revision = revision + (int)fw_ver[FW_PUB_NUM + 1] - 48;

	return revision;
}

bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_CORE_VER] != fw_ver2[FW_CORE_VER]
		|| fw_ver1[FW_PIXEL_SIZE] != fw_ver2[FW_PIXEL_SIZE]
		|| fw_ver1[FW_PIXEL_SIZE + 1] != fw_ver2[FW_PIXEL_SIZE + 1]
		|| fw_ver1[FW_ISP_COMPANY] != fw_ver2[FW_ISP_COMPANY]
		|| fw_ver1[FW_SENSOR_MAKER] != fw_ver2[FW_SENSOR_MAKER]) {
		return false;
	}

	return true;
}

u8 fimc_is_sec_compare_ver(int position)
{
	u32 from_ver = 0, def_ver = 0;
	u8 ret = 0;
	char ver[3] = {'V', '0', '0'};
	struct fimc_is_from_info *finfo = NULL;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT)
		finfo = &sysfs_finfo_front;
	else
#endif
		finfo = &sysfs_finfo;

	def_ver = ver[0] << 16 | ver[1] << 8 | ver[2];
	from_ver = finfo->cal_map_ver[0] << 16 | finfo->cal_map_ver[1] << 8 | finfo->cal_map_ver[2];
	if (from_ver == def_ver) {
		return finfo->cal_map_ver[3];
	} else {
		err("FROM core version is invalid. version is %c%c%c%c",
			finfo->cal_map_ver[0], finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
		return 0;
	}

	return ret;
}

bool fimc_is_sec_check_from_ver(struct fimc_is_core *core, int position)
{
	struct fimc_is_from_info *finfo = NULL;
	struct exynos_platform_fimc_is *core_pdata = NULL;
	char compare_version;
	u8 from_ver;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return false;
	}

	if (core_pdata->skip_cal_loading) {
		err("skip_cal_loading implemented");
		return false;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT)
		finfo = &sysfs_finfo_front;
	else
#endif
		finfo = &sysfs_finfo;

	compare_version = FIMC_IS_LATEST_FROM_VERSION_B;
	from_ver = fimc_is_sec_compare_ver(position);

	if ((from_ver < FROM_VERSION_V003) ||
		(finfo->header_ver[10] < compare_version)) {
		err("invalid from version. from_ver %u, header_ver[10] %c", from_ver, finfo->header_ver[10]);
		return false;
	} else {
		return true;
	}
}
bool fimc_is_sec_check_cal_crc32(char *buf, int id)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 check_base;
	u32 check_length;
	u32 checksum_base;
	bool crc32_temp, crc32_header_temp;
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_companion_retention *ret_data;
	struct fimc_is_core *core;
	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	ret_data = &core->companion.retention_data;
	buf32 = (u32 *)buf;

	printk(KERN_INFO "+++ %s\n", __func__);

	crc32_temp = true;
#ifdef CONFIG_COMPANION_USE
	crc32_c1_check = true;
#endif

	/* Header data */
	check_base = 0;
	checksum = 0;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		finfo = &sysfs_finfo_front;
		checksum_base = ((check_base & 0xffffff00) + 0xfc) / 4;
		check_length = HEADER_CRC32_LEN_FRONT;
	} else
#endif
	{
		finfo = &sysfs_finfo;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		checksum_base = ((check_base & 0xffffff00) + 0xfc) / 4;
#else
		checksum_base = ((check_base & 0xfffff000) + 0xffc) / 4;
#endif
		check_length = HEADER_CRC32_LEN;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the header (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_temp = false;
		crc32_header_temp = false;
	} else if (checksum_base > 0x80000) {
		err("Camera: Header checksum address has error(0x%08X)", checksum_base * 4);
	} else {
		crc32_header_temp = true;
	}

	/* OEM */
	check_base = finfo->oem_start_addr / 4;
	checksum = 0;
	check_length = (finfo->oem_end_addr - finfo->oem_start_addr + 1);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		checksum_base = ((finfo->oem_end_addr & 0xffffff00) + 0xfc) / 4;
	} else
#endif
	{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		checksum_base = ((finfo->oem_end_addr & 0xffffff00) + 0xfc) / 4;
#else
		checksum_base = ((finfo->oem_end_addr & 0xfffff000) + 0xffc) / 4;
#endif
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base],
					check_length, NULL, NULL);
	if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the OEM (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_temp = false;
	} else if (checksum_base > 0x80000 || checksum_base < 0) {
		err("Camera: OEM checksum address has error(0x%08X)", checksum_base * 4);
	}

	/* AWB */
	check_base = finfo->awb_start_addr / 4;
	checksum = 0;
	check_length = (finfo->awb_end_addr - finfo->awb_start_addr + 1) ;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		checksum_base = ((finfo->awb_end_addr & 0xffffff00) + 0xfc) / 4;
	} else
#endif
	{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		checksum_base = ((finfo->awb_end_addr & 0xffffff00) + 0xfc) / 4;
#else
		checksum_base = ((finfo->awb_end_addr & 0xfffff000) + 0xffc) / 4;
#endif
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base],
					check_length, NULL, NULL);
	if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the AWB (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_temp = false;
	} else if (checksum_base > 0x80000 || checksum_base < 0) {
		err("Camera: AWB checksum address has error(0x%08X)", checksum_base * 4);
	}

	/* Shading */
	check_base = finfo->shading_start_addr / 4;
	checksum = 0;
	check_length = (finfo->shading_end_addr - finfo->shading_start_addr + 1) ;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		checksum_base = 0x3BFC / 4;
	} else
#endif
	{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		checksum_base = 0x1ffc / 4;
#else
		checksum_base = ((0x4FFC & 0xfffff000) + 0xffc) / 4;
#endif
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base],
					check_length, NULL, NULL);
	if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the Shading (0x%08X != 0x%08X)",
				checksum, buf32[checksum_base]);
		crc32_temp = false;
	} else if (checksum_base > 0x80000 || checksum_base < 0) {
		err("Camera: Shading checksum address has error(0x%08X)", checksum_base * 4);
	}

#ifdef CONFIG_COMPANION_C2_USE
	/* c2 Shading */
	if (id == SENSOR_POSITION_FRONT) {
		check_base = finfo->c2_shading_start_addr / 4;
		checksum = 0;
		check_length = (finfo->c2_shading_end_addr - finfo->c2_shading_start_addr + 1);

		checksum_base = 0x1FFC / 4;

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the C2 Shading (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
		} else if (checksum_base > 0x80000 || checksum_base < 0) {
			err("Camera: C2 Shading checksum address has error(0x%08X)", checksum_base * 4);
		}
	}
#endif

#ifdef CONFIG_COMPANION_USE
	/* pdaf cal */
	if (id == SENSOR_POSITION_REAR) {
		check_base = finfo->pdaf_cal_start_addr / 4;
		checksum = 0;
		check_length = (finfo->pdaf_cal_end_addr - finfo->pdaf_cal_start_addr + 1);
		checksum_base = ((0x8FFF & 0xfffff000) + 0xffc) / 4;

		checksum = (u32)getCRC((u16 *)&buf32[check_base],
										check_length, NULL, NULL);
		if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the pdaf cal (0x%08X != 0x%08X)",
										checksum, buf32[checksum_base]);
			crc32_temp = false;
		} else if (checksum_base > 0x80000 || checksum_base < 0) {
			err("Camera: pdaf cal checksum address has error(0x%08X)", checksum_base * 4);
		}

		/* concord cal */
		check_base = finfo->concord_cal_start_addr / 4;
		checksum = 0;
		check_length = (finfo->concord_cal_end_addr - finfo->concord_cal_start_addr + 1);
		checksum_base = ((0xFFFC & 0xfffff000) + 0xffc) / 4;

		checksum = (u32)getCRC((u16 *)&buf32[check_base],
						check_length, NULL, NULL);
		if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
				err("Camera: CRC32 error at the concord cal (0x%08X != 0x%08X)",
						checksum, buf32[checksum_base]);
				crc32_c1_check = false;
		} else if (checksum_base > 0x80000 || checksum_base < 0) {
			err("Camera: concord cal checksum address has error(0x%08X)", checksum_base * 4);
		}
	}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		crc32_check_front = crc32_temp;
		crc32_header_check_front = crc32_header_temp;
		return crc32_check_front;
	} else
#endif
	{
		crc32_check = crc32_temp;
		crc32_header_check = crc32_header_temp;
#ifdef CONFIG_COMPANION_USE
		return crc32_check && crc32_c1_check;
#else
		return crc32_check;
#endif
	}
}

bool fimc_is_sec_check_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;

	buf32 = (u32 *)buf;

	info("Camera: Start checking CRC32 FW\n");

	crc32_fw_check = true;

	checksum = (u32)getCRC((u16 *)&buf32[0], (sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1), NULL, NULL);
	checksum_base = (0x2A8FFC & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_fw_check = false;
	}

	info("Camera: End checking CRC32 FW\n");

	return crc32_fw_check;
}

bool fimc_is_sec_check_setfile_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;

	buf32 = (u32 *)buf;

	info("Camera: Start checking CRC32 Setfile\n");

	crc32_setfile_check = true;

	checksum = (u32)getCRC((u16 *)&buf32[0],
		(sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1), NULL, NULL);
	checksum_base = (0x1167FC & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_setfile_check = false;
	}

	info("Camera: End checking CRC32 Setfile\n");

	return crc32_setfile_check;
}

#ifdef CONFIG_COMPANION_USE
bool fimc_is_sec_check_companion_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;

	buf32 = (u32 *)buf;

	info("Camera: Start checking CRC32 Companion FW\n");

	crc32_c1_fw_check = true;

	checksum = (u32)getCRC((u16 *)&buf32[0], (sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1), NULL, NULL);
	checksum_base = ((0x307FC & 0xffffffff)) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_c1_fw_check = false;
	}

	info("Camera: End checking CRC32 Companion FW\n");

	return crc32_c1_fw_check;
}
#endif

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx = -ENOENT;
	int fd, old_mask;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0664);
	if (fd < 0) {
		err("open file error: %s", name);
		sys_umask(old_mask);
		set_fs(old_fs);
		return -EINVAL;
	}

	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, count, pos);
		if (tx != count) {
			err("fail to write %s. ret %zd", name, tx);
			tx = -ENOENT;
		}

		vfs_fsync(fp, 0);
		fput(fp);
	} else {
		err("fail to get file *: %s", name);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

	return tx;
}

ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx;
	int fd;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = sys_open(name, O_RDONLY, 0664);
	if (fd < 0) {
		if (-ENOENT == fd)
			info("%s: file(%s) not exist!\n", __func__, name);
		else
			info("%s: error %d, failed to open %s\n", __func__, fd, name);

		set_fs(old_fs);
		return -EINVAL;
	}
	fp = fget(fd);
	if (fp) {
		tx = vfs_read(fp, buf, count, pos);
		fput(fp);
	}
	sys_close(fd);
	set_fs(old_fs);

	return count;
}

bool fimc_is_sec_file_exist(char *name)
{
	mm_segment_t old_fs;
	bool exist = true;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = sys_access(name, 0);
	if (ret) {
		exist = false;
		if (-ENOENT == ret)
			info("%s: file(%s) not exist!\n", __func__, name);
		else
			info("%s: error %d, failed to access %s\n", __func__, ret, name);
	}

	set_fs(old_fs);
	return exist;
}

void fimc_is_sec_make_crc32_table(u32 *table, u32 id)
{
	u32 i, j, k;

	for (i = 0; i < 256; ++i) {
		k = i;
		for (j = 0; j < 8; ++j) {
			if (k & 1)
				k = (k >> 1) ^ id;
			else
				k >>= 1;
		}
		table[i] = k;
	}
}

#if 0 /* unused */
static void fimc_is_read_sensor_version(void)
{
	int ret;
	char buf[0x50];

	memset(buf, 0x0, 0x50);

	printk(KERN_INFO "+++ %s\n", __func__);

	ret = fimc_is_spi_read(buf, 0x0, 0x50);

	printk(KERN_INFO "--- %s\n", __func__);

	if (ret) {
		err("fimc_is_spi_read - can't read sensor version\n");
	}

	err("Manufacturer ID(0x40): 0x%02x\n", buf[0x40]);
	err("Pixel Number(0x41): 0x%02x\n", buf[0x41]);
}

static void fimc_is_read_sensor_version2(void)
{
	char *buf;
	char *cal_data;
	u32 cur;
	u32 count = SETFILE_SIZE/READ_SIZE;
	u32 extra = SETFILE_SIZE%READ_SIZE;

	printk(KERN_ERR "%s\n", __func__);

	buf = (char *)kmalloc(READ_SIZE, GFP_KERNEL);
	cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

	memset(buf, 0x0, READ_SIZE);
	memset(cal_data, 0x0, SETFILE_SIZE);

	for (cur = 0; cur < SETFILE_SIZE; cur += READ_SIZE) {
		fimc_is_spi_read(buf, cur, READ_SIZE);
		memcpy(cal_data+cur, buf, READ_SIZE);
		memset(buf, 0x0, READ_SIZE);
	}

	if (extra != 0) {
		fimc_is_spi_read(buf, cur, extra);
		memcpy(cal_data+cur, buf, extra);
		memset(buf, 0x0, extra);
	}

	info("Manufacturer ID(0x40): 0x%02x\n", cal_data[0x40]);
	info("Pixel Number(0x41): 0x%02x\n", cal_data[0x41]);

	info("Manufacturer ID(0x4FE7): 0x%02x\n", cal_data[0x4FE7]);
	info("Pixel Number(0x4FE8): 0x%02x\n", cal_data[0x4FE8]);
	info("Manufacturer ID(0x4FE9): 0x%02x\n", cal_data[0x4FE9]);
	info("Pixel Number(0x4FEA): 0x%02x\n", cal_data[0x4FEA]);

	kfree(buf);
	kfree(cal_data);
}

static int fimc_is_get_cal_data(void)
{
	int err = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long ret = 0;
	u8 mem0 = 0, mem1 = 0;
	u32 CRC = 0;
	u32 DataCRC = 0;
	u32 IntOriginalCRC = 0;
	u32 crc_index = 0;
	int retryCnt = 2;
	u32 header_crc32 =	0x1000;
	u32 oem_crc32 =		0x2000;
	u32 awb_crc32 =		0x3000;
	u32 shading_crc32 = 0x6000;
	u32 shading_header = 0x22C0;

	char *cal_data;

	crc32_check = true;
	printk(KERN_INFO "%s\n", __func__);
	printk(KERN_INFO "+++ %s\n", __func__);

	fimc_is_spi_read(cal_map_version, 0x60, 0x4);
	printk(KERN_INFO "cal_map_version = %.4s\n", cal_map_version);

	if (cal_map_version[3] == '5') {
		shading_crc32 = 0x6000;
		shading_header = 0x22C0;
	} else if (cal_map_version[3] == '6') {
		shading_crc32 = 0x4000;
		shading_header = 0x920;
	} else {
		shading_crc32 = 0x5000;
		shading_header = 0x22C0;
	}

	/* Make CRC Table */
	fimc_is_sec_make_crc32_table((u32 *)&crc_table, 0xEDB88320);


	retry:
		cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

		memset(cal_data, 0x0, SETFILE_SIZE);

		mem0 = 0, mem1 = 0;
		CRC = 0;
		DataCRC = 0;
		IntOriginalCRC = 0;
		crc_index = 0;

		fimc_is_spi_read(cal_data, 0, SETFILE_SIZE);

		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x80)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made HEADER CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[header_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[header_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[header_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[header_crc32-1]&0x00ff);
		printk(KERN_INFO "Original HEADER CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;

		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0xC0)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x1000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x1000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made OEM CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[oem_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[oem_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[oem_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[oem_crc32-1]&0x00ff);
		printk(KERN_INFO "Original OEM CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x20)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x2000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x2000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made AWB CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[awb_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[awb_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[awb_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[awb_crc32-1]&0x00ff);
		printk(KERN_INFO "Original AWB CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (shading_header)/2; crc_index++) {

			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x3000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x3000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made SHADING CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[shading_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[shading_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[shading_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[shading_crc32-1]&0x00ff);
		printk(KERN_INFO "Original SHADING CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		old_fs = get_fs();
		set_fs(KERNEL_DS);

		if (crc32_check == true) {
			printk(KERN_INFO "make cal_data.bin~~~~ \n");
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		} else {
			if (retryCnt > 0) {
				set_fs(old_fs);
				retryCnt--;
				goto retry;
			}
		}

/*
		{
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		}
*/

		if (fp != NULL)
			filp_close(fp, current->files);

	out:
		set_fs(old_fs);
		kfree(cal_data);
		return err;

}

#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_i2c_read(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	const u32 addr_size = 2, max_retry = 5;
	u8 addr_buf[addr_size];
	int retries = max_retry;
	int ret = 0;

	if (!client) {
		info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr */
	addr_buf[0] = ((u16)addr) >> 8;
	addr_buf[1] = (u8)addr;

	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, addr_buf, addr_size);
		if (likely(addr_size == ret))
			break;

		info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to write 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	/* Receive data */
	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_recv(client, buf, size);
		if (likely(ret == size))
			break;

		info("%s: i2c_master_recv failed(%d), try %d\n", __func__,  ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to read 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

int fimc_is_i2c_write(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	info("%s: do nothing\n", __func__);
	return 0;
}

static void fimc_is_i2c_config(struct i2c_client *client, bool onoff)
{
	struct device *i2c_dev = client->dev.parent->parent;
	struct pinctrl *pinctrl_i2c = NULL;
	struct fimc_is_device_eeprom *eeprom_device = i2c_get_clientdata(client);
	struct fimc_is_eeprom_gpio *gpio;

	gpio = &eeprom_device->gpio;

	info("(%s):onoff(%d)\n", __func__, onoff);
	if (onoff) {
		pin_config_set(gpio->pinname, gpio->sda,
			PINCFG_PACK(PINCFG_TYPE_FUNC, 0));
		pin_config_set(gpio->pinname, gpio->scl,
			PINCFG_PACK(PINCFG_TYPE_FUNC, 0));
		/* ON */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
	} else {
		/* OFF */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "off_i2c");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
		pin_config_set(gpio->pinname, gpio->sda,
			PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
		pin_config_set(gpio->pinname, gpio->scl,
			PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
	}
}

int fimc_is_sec_read_eeprom_header(struct device *dev)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct exynos_platform_fimc_is *core_pdata = NULL;
	u8 header_version[12] = {0, };
	struct i2c_client *client;
	client = core->eeprom_client0;
	core_pdata = dev_get_platdata(fimc_is_dev);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, true);
	}
#endif

	ret = fimc_is_i2c_read(client, header_version, 0x20, 0x0B);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, false);
	}
#endif

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(sysfs_finfo.header_ver, header_version, 11);
	sysfs_finfo.header_ver[11] = '\0';

	return ret;
}

int fimc_is_sec_readcal_eeprom(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct exynos_platform_fimc_is *core_pdata = NULL;
	struct fimc_is_from_info *finfo = NULL;
	int cal_size = 0;
	struct i2c_client *client = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		finfo = &sysfs_finfo_front;
		fimc_is_sec_get_front_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		client = core->eeprom_client1;
	} else
#endif
	{
		finfo = &sysfs_finfo;
		fimc_is_sec_get_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		client = core->eeprom_client0;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, true);
	}
#endif

	if (position == SENSOR_POSITION_FRONT) {
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver, CAL_MAP_VER_ADDR_FRONT, 4);
	} else {
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver, CAL_MAP_VER_ADDR, 4);
	}

	if (position == SENSOR_POSITION_FRONT) {
		ret = fimc_is_i2c_read(client, finfo->header_ver, CAL_HEADER_VER_ADDR_FRONT, 11);
	} else {
		ret = fimc_is_i2c_read(client, finfo->header_ver, CAL_HEADER_VER_ADDR, 11);
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, false);
	}
#endif

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	printk(KERN_INFO "Camera: Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	if (!fimc_is_sec_check_from_ver(core, position)) {
		info("Camera: Do not read eeprom cal data. EEPROM version is low.\n");
		return 0;
	}

crc_retry:

	/* read cal data */
	info("Camera: I2C read cal data\n");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, true);
	}
#endif

	fimc_is_i2c_read(client, buf, 0x0, cal_size);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, false);
	}
#endif

	if (position == SENSOR_POSITION_FRONT) {
		info("FRONT EEPROM header version = %s\n", finfo->header_ver);
		finfo->oem_start_addr = *((u32 *)&buf[OEM_START_ADDR_FRONT]);
		finfo->oem_end_addr = *((u32 *)&buf[OEM_END_ADDR_FRONT]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
		finfo->awb_start_addr = *((u32 *)&buf[AWB_START_ADDR_FRONT]);
		finfo->awb_end_addr = *((u32 *)&buf[AWB_END_ADDR_FRONT]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
		finfo->shading_start_addr = *((u32 *)&buf[SHADING_START_ADDR_FRONT]);
		finfo->shading_end_addr = *((u32 *)&buf[SHADING_END_ADDR_FRONT]);
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
		if (finfo->shading_end_addr > 0x3AFF) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->shading_end_addr = 0x3AFF;
		}
#ifdef CONFIG_COMPANION_C2_USE
		finfo->c2_shading_start_addr = *((u32 *)&buf[C2_SHADING_START_ADDR_FRONT]);
		finfo->c2_shading_end_addr = *((u32 *)&buf[C2_SHADING_END_ADDR_FRONT]);
		info("c2_shading start = 0x%08x, end = 0x%08x\n",
				(finfo->c2_shading_start_addr), (finfo->c2_shading_end_addr));
		if (finfo->c2_shading_end_addr > 0x1CFF) {
			err("C2 Shading end_addr has error!! 0x%08x", finfo->c2_shading_end_addr);
			finfo->c2_shading_end_addr = 0x1CFF;
		}

		/* C2 SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->c2_shading_ver, &buf[C2_SHADING_VER_ADDR_FRONT], 11);
		finfo->shading_ver[11] = '\0';

		finfo->lsc_i0_gain_addr = 0x1A02;
		info("Shading lsc_i0 start = 0x%08x\n", finfo->lsc_i0_gain_addr);
		finfo->lsc_j0_gain_addr = finfo->lsc_i0_gain_addr + 8;
		info("Shading lsc_j0 start = 0x%08x\n", finfo->lsc_j0_gain_addr);
		finfo->lsc_a_gain_addr = finfo->lsc_j0_gain_addr + 8;
		info("Shading lsc_a start = 0x%08x\n", finfo->lsc_a_gain_addr);
		finfo->lsc_k4_gain_addr = finfo->lsc_a_gain_addr + 16;
		info("Shading lsc_k4 start = 0x%08x\n", finfo->lsc_k4_gain_addr);
		finfo->lsc_scale_gain_addr = finfo->lsc_k4_gain_addr + 16;
		info("Shading lsc_scale start = 0x%08x\n", finfo->lsc_scale_gain_addr);
		finfo->grasTuning_AwbAshCord_N_addr = finfo->lsc_scale_gain_addr + 8;
		info("Shading grasTuning_AwbAshCord_N start = 0x%08x\n", finfo->grasTuning_AwbAshCord_N_addr);
		finfo->grasTuning_awbAshCordIndexes_N_addr = finfo->grasTuning_AwbAshCord_N_addr + 14;
		info("Shading grasTuning_awbAshCordIndexes_N start = 0x%08x\n",
				finfo->grasTuning_awbAshCordIndexes_N_addr);
		finfo->grasTuning_GASAlpha_M__N_addr = finfo->grasTuning_awbAshCordIndexes_N_addr + 14;
		info("Shading grasTuning_GASAlpha_M__N_addr start = 0x%08x\n", finfo->grasTuning_GASAlpha_M__N_addr);
		finfo->grasTuning_GASBeta_M__N_addr = finfo->grasTuning_GASAlpha_M__N_addr + 56;
		info("Shading grasTuning_GASBeta_M__N start = 0x%08x\n", finfo->grasTuning_GASBeta_M__N_addr);
		finfo->grasTuning_GASOutdoorAlpha_N_addr = finfo->grasTuning_GASBeta_M__N_addr + 56;
		info("Shading grasTuning_GASOutdoorAlpha_N start = 0x%08x\n",
				finfo->grasTuning_GASOutdoorAlpha_N_addr);
		finfo->grasTuning_GASOutdoorBeta_N_addr = finfo->grasTuning_GASOutdoorAlpha_N_addr + 8;
		info("Shading grasTuning_GASOutdoorBeta_N start = 0x%08x\n", finfo->grasTuning_GASOutdoorBeta_N_addr);
		finfo->grasTuning_GASIndoorAlpha_N_addr = finfo->grasTuning_GASOutdoorBeta_N_addr + 8;
		info("Shading grasTuning_GASIndoorAlpha_N start = 0x%08x\n", finfo->grasTuning_GASIndoorAlpha_N_addr);
		finfo->grasTuning_GASIndoorBeta_N_addr = finfo->grasTuning_GASIndoorAlpha_N_addr + 8;
		info("Shading grasTuning_GASIndoorBeta_N start = 0x%08x\n", finfo->grasTuning_GASIndoorBeta_N_addr);

		finfo->lsc_gain_start_addr = finfo->c2_shading_start_addr + 6;
		finfo->lsc_gain_end_addr = finfo->lsc_gain_start_addr + 5880 -1;
		info("LSC start = 0x%04x, end = 0x%04x\n", finfo->lsc_gain_start_addr, finfo->lsc_gain_end_addr);
		finfo->lsc_gain_crc_addr = 0x19FE;
		info("lsc_gain_crc_addr = 0x%04x,\n", finfo->lsc_gain_crc_addr);
		finfo->lsc_parameter_crc_addr = 0x1AE6;
		info("lsc_parameter_crc_addr = 0x%04x,\n", finfo->lsc_parameter_crc_addr);
		info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
#endif
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[CAL_HEADER_VER_ADDR_FRONT], 11);
		finfo->header_ver[11] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[CAL_MAP_VER_ADDR_FRONT], 4);

		memcpy(finfo->project_name, &buf[PROJECT_NAME_ADDR_FRONT], 8);
		finfo->project_name[8] = '\0';

		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[OEM_VER_ADDR_FRONT], 11);
		finfo->oem_ver[11] = '\0';

		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[AWB_VER_ADDR_FRONT], 11);
		finfo->awb_ver[11] = '\0';

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[SHADING_VER_ADDR_FRONT], 11);
		finfo->shading_ver[11] = '\0';
	} else {
		finfo->oem_start_addr = *((u32 *)&buf[OEM_START_ADDR]);
		finfo->oem_end_addr = *((u32 *)&buf[OEM_END_ADDR]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
		finfo->awb_start_addr = *((u32 *)&buf[AWB_START_ADDR]);
		finfo->awb_end_addr = *((u32 *)&buf[AWB_END_ADDR]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
		finfo->shading_start_addr = *((u32 *)&buf[SHADING_START_ADDR]);
		finfo->shading_end_addr = *((u32 *)&buf[SHADING_END_ADDR]);
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));

		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[CAL_HEADER_VER_ADDR], 11);
		finfo->header_ver[11] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[CAL_MAP_VER_ADDR], 4);

		memcpy(finfo->project_name, &buf[PROJECT_NAME_ADDR], 8);
		finfo->project_name[8] = '\0';

		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[OEM_VER_ADDR], 11);
		finfo->oem_ver[11] = '\0';

		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[AWB_VER_ADDR], 11);
		finfo->awb_ver[11] = '\0';

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[SHADING_VER_ADDR], 11);
		finfo->shading_ver[11] = '\0';
	}

	/* debug info dump */
#if defined(EEPROM_DEBUG)
	info("++++ EEPROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", finfo->header_ver);
	info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE],
		finfo->header_ver[FW_PIXEL_SIZE+1]);
	info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM],
		finfo->header_ver[FW_PUB_NUM+1]);
	info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	info("project_name : %s\n", finfo->project_name);
	info("Cal data map ver : %s\n", finfo->cal_map_ver);
	info("2. OEM info\n");
	info("Module info : %s\n", finfo->oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", finfo->awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", finfo->shading_ver);
	info("---- EEPROM data info\n");
#endif

	/* CRC check */
	if (position == SENSOR_POSITION_FRONT) {
		ret = fimc_is_sec_check_cal_crc32(buf, SENSOR_POSITION_FRONT);
	} else {
		ret = fimc_is_sec_check_cal_crc32(buf, SENSOR_POSITION_REAR);
	}
	if (!ret && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}
	} else
#endif
	{
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}
	}

#ifdef CONFIG_COMPANION_C2_USE
	if (fimc_is_sec_check_from_ver(core, position)) {
		/* If FROM LSC value is not valid, loading default lsc data */
		if (*((u32 *)&cal_buf_front[sysfs_finfo_front.lsc_gain_start_addr]) == 0x00000000) {
			companion_front_lsc_isvalid = false;
		} else {
			companion_front_lsc_isvalid = true;
		}
	}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_module_check) {
		if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
			is_final_cam_module_front = true;
		} else {
			is_final_cam_module_front = false;
		}
	} else {
		is_final_cam_module_front = true;
	}
#endif

exit:
	return ret;
}
#endif

#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_read_from_header(struct device *dev)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	u8 header_version[12] = {0, };

	ret = fimc_is_spi_read(&core->spi0, header_version, 0x40, 0x0B);
	if (ret < 0) {
		printk(KERN_ERR "failed to fimc_is_spi_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(sysfs_finfo.header_ver, header_version, 11);
	sysfs_finfo.header_ver[11] = '\0';

	return ret;
}

int fimc_is_sec_readcal(struct fimc_is_core *core)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u16 id;

	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
	}

	/* reset spi */
	if (!core->spi0.device) {
		err("spi0 device is not available");
		goto exit;
	}

	ret = fimc_is_spi_reset(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_read_module_id(&core->spi0, &id, 0x0, 0x2);
	if (ret) {
		printk(KERN_ERR "fimc_is_spi_read_module_id (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Module ID = 0x%04x\n", id);

	ret = fimc_is_spi_read(&core->spi0, sysfs_finfo.cal_map_ver, 0x60, 0x4);
	if (ret) {
		printk(KERN_ERR "failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: Cal map_version = %c%c%c%c\n", sysfs_finfo.cal_map_ver[0],
			sysfs_finfo.cal_map_ver[1], sysfs_finfo.cal_map_ver[2], sysfs_finfo.cal_map_ver[3]);

crc_retry:
	/* read cal data */
	info("Camera: SPI read cal data\n");
	ret = fimc_is_spi_read(&core->spi0, cal_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&cal_buf[0x0]);
	sysfs_finfo.bin_end_addr = *((u32 *)&cal_buf[0x4]);
	info("Binary start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.oem_start_addr = *((u32 *)&cal_buf[0x8]);
	sysfs_finfo.oem_end_addr = *((u32 *)&cal_buf[0xC]);
	info("OEM start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.oem_start_addr), (sysfs_finfo.oem_end_addr));
	sysfs_finfo.awb_start_addr = *((u32 *)&cal_buf[0x10]);
	sysfs_finfo.awb_end_addr = *((u32 *)&cal_buf[0x14]);
	info("AWB start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.awb_start_addr), (sysfs_finfo.awb_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&cal_buf[0x18]);
	sysfs_finfo.shading_end_addr = *((u32 *)&cal_buf[0x1C]);
	info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
	sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[0x20]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[0x24]);
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));
#ifdef CONFIG_COMPANION_USE
	sysfs_finfo.concord_cal_start_addr = *((u32 *)&cal_buf[0x28]);
	sysfs_finfo.concord_cal_end_addr = *((u32 *)&cal_buf[0x2C]);
	info("concord cal start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_cal_start_addr, sysfs_finfo.concord_cal_end_addr);
	sysfs_finfo.concord_bin_start_addr = *((u32 *)&cal_buf[0x30]);
	sysfs_finfo.concord_bin_end_addr = *((u32 *)&cal_buf[0x34]);
	info("concord bin start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_bin_start_addr, sysfs_finfo.concord_bin_end_addr);
	sysfs_finfo.concord_master_setfile_start_addr = *((u32 *)&cal_buf[0xA8]);
	sysfs_finfo.concord_master_setfile_end_addr = sysfs_finfo.concord_master_setfile_start_addr + 16064;
	sysfs_finfo.concord_mode_setfile_start_addr = sysfs_finfo.concord_master_setfile_end_addr + 1;
	sysfs_finfo.concord_mode_setfile_end_addr = *((u32 *)&cal_buf[0xAC]);
	sysfs_finfo.pdaf_cal_start_addr = *((u32 *)&cal_buf[0xB0]);
	sysfs_finfo.pdaf_cal_end_addr = *((u32 *)&cal_buf[0xB4]);
#ifdef CONFIG_COMPANION_C1_USE
	if (fimc_is_sec_check_from_ver(core))
		sysfs_finfo.pdaf_cal_end_addr = 0x540F;
	else
		sysfs_finfo.pdaf_cal_end_addr = 0x521F;
#endif
	info("pdaf start = 0x%08x, end = 0x%08x\n", sysfs_finfo.pdaf_cal_start_addr, sysfs_finfo.pdaf_cal_end_addr);
	sysfs_finfo.lsc_i0_gain_addr = 0x4702;
	info("Shading lsc_i0 start = 0x%08x\n", sysfs_finfo.lsc_i0_gain_addr);
	sysfs_finfo.lsc_j0_gain_addr = sysfs_finfo.lsc_i0_gain_addr + 8;
	info("Shading lsc_j0 start = 0x%08x\n", sysfs_finfo.lsc_j0_gain_addr);
	sysfs_finfo.lsc_a_gain_addr = sysfs_finfo.lsc_j0_gain_addr + 8;
	info("Shading lsc_a start = 0x%08x\n", sysfs_finfo.lsc_a_gain_addr);
	sysfs_finfo.lsc_k4_gain_addr = sysfs_finfo.lsc_a_gain_addr + 16;
	info("Shading lsc_k4 start = 0x%08x\n", sysfs_finfo.lsc_k4_gain_addr);
	sysfs_finfo.lsc_scale_gain_addr = sysfs_finfo.lsc_k4_gain_addr + 16;
#ifdef CONFIG_COMPANION_C1_USE
	sysfs_finfo.lsc_gain_start_addr = sysfs_finfo.shading_start_addr + 0x14;
	sysfs_finfo.lsc_gain_end_addr = sysfs_finfo.lsc_gain_start_addr + 6600 -1;
	info("LSC start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.lsc_gain_start_addr, sysfs_finfo.lsc_gain_end_addr);
#endif
#ifdef CONFIG_COMPANION_C2_USE
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.lsc_scale_gain_addr);
	sysfs_finfo.grasTuning_AwbAshCord_N_addr = sysfs_finfo.lsc_scale_gain_addr + 8;
	info("Shading grasTuning_AwbAshCord_N start = 0x%08x\n", sysfs_finfo.grasTuning_AwbAshCord_N_addr);
	sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr = sysfs_finfo.grasTuning_AwbAshCord_N_addr + 14;
	info("Shading grasTuning_awbAshCordIndexes_N start = 0x%08x\n", sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr);
	sysfs_finfo.grasTuning_GASAlpha_M__N_addr = sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr + 14;
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.grasTuning_GASAlpha_M__N_addr);
	sysfs_finfo.grasTuning_GASBeta_M__N_addr = sysfs_finfo.grasTuning_GASAlpha_M__N_addr + 56;
	info("Shading grasTuning_GASBeta_M__N start = 0x%08x\n", sysfs_finfo.grasTuning_GASBeta_M__N_addr);
	sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr = sysfs_finfo.grasTuning_GASBeta_M__N_addr + 56;
	info("Shading grasTuning_GASOutdoorAlpha_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr);
	sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr = sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr + 8;
	info("Shading grasTuning_GASOutdoorBeta_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr);
	sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr = sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr + 8;
	info("Shading grasTuning_GASIndoorAlpha_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr);
	sysfs_finfo.grasTuning_GASIndoorBeta_N_addr = sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr + 8;
	info("Shading grasTuning_GASIndoorBeta_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASIndoorBeta_N_addr);
	sysfs_finfo.lsc_gain_start_addr = sysfs_finfo.shading_start_addr + 6;
	sysfs_finfo.lsc_gain_end_addr = sysfs_finfo.lsc_gain_start_addr + 5880 -1;
	info("LSC start = 0x%04x, end = 0x%04x\n", sysfs_finfo.lsc_gain_start_addr, sysfs_finfo.lsc_gain_end_addr);
#endif
	sysfs_finfo.pdaf_start_addr = sysfs_finfo.concord_cal_start_addr;
	sysfs_finfo.pdaf_end_addr = sysfs_finfo.pdaf_start_addr + 512 -1;
	info("pdaf start = 0x%04x, end = 0x%04x\n", sysfs_finfo.pdaf_start_addr, sysfs_finfo.pdaf_end_addr);
#ifdef CONFIG_COMPANION_C2_USE
	sysfs_finfo.pdaf_shad_start_addr = sysfs_finfo.pdaf_end_addr + 1 + 4;
	sysfs_finfo.pdaf_shad_end_addr = sysfs_finfo.pdaf_shad_start_addr + 1056 - 1;
	info("pdaf_shad start = 0x%04x, end = 0x%04x\n", sysfs_finfo.pdaf_shad_start_addr,
		sysfs_finfo.pdaf_shad_end_addr);
#endif
#ifdef CONFIG_COMPANION_C1_USE
	/*sysfs_finfo.coefficient_cal_start_addr = sysfs_finfo.pdaf_start_addr + 512 + 16;
	sysfs_finfo.coefficient_cal_end_addr = sysfs_finfo.coefficient_cal_start_addr + 24576 -1;
	info("coefficient_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coefficient_cal_start_addr, sysfs_finfo.coefficient_cal_end_addr);*/
	sysfs_finfo.coef1_start = sysfs_finfo.pdaf_start_addr + 512 + 16;
	sysfs_finfo.coef1_end = sysfs_finfo.coef1_start + 4032 -1;
	info("coefficient1_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef1_start, sysfs_finfo.coef1_end);
	sysfs_finfo.coef2_start = sysfs_finfo.coef1_end + 64 + 1;
	sysfs_finfo.coef2_end = sysfs_finfo.coef2_start + 4032 -1;
	info("coefficient2_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef2_start, sysfs_finfo.coef2_end);
	sysfs_finfo.coef3_start = sysfs_finfo.coef2_end + 64 + 1;
	sysfs_finfo.coef3_end = sysfs_finfo.coef3_start + 4032 -1;
	info("coefficient3_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef3_start, sysfs_finfo.coef3_end);
	sysfs_finfo.coef4_start = sysfs_finfo.coef3_end + 64 + 1;
	sysfs_finfo.coef4_end = sysfs_finfo.coef4_start + 4032 -1;
	info("coefficient4_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef4_start, sysfs_finfo.coef4_end);
	sysfs_finfo.coef5_start = sysfs_finfo.coef4_end + 64 + 1;
	sysfs_finfo.coef5_end = sysfs_finfo.coef5_start + 4032 -1;
	info("coefficient5_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef5_start, sysfs_finfo.coef5_end);
	sysfs_finfo.coef6_start = sysfs_finfo.coef5_end + 64 + 1;
	sysfs_finfo.coef6_end = sysfs_finfo.coef6_start + 4032 -1;
	info("coefficient6_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef6_start, sysfs_finfo.coef6_end);
	sysfs_finfo.wcoefficient1_addr = 0xF210;
	info("Shading wcoefficient1 start = 0x%04x\n", sysfs_finfo.wcoefficient1_addr);
#endif
#ifdef CONFIG_COMPANION_C2_USE
	sysfs_finfo.xtalk_coef_start = sysfs_finfo.pdaf_shad_end_addr + 4 + 24 + 1;
	sysfs_finfo.coef_offset_R = 0xEA40;
	sysfs_finfo.coef_offset_G = sysfs_finfo.coef_offset_R + 2;
	sysfs_finfo.coef_offset_B = sysfs_finfo.coef_offset_G + 2;
#endif
	memcpy(sysfs_finfo.concord_header_ver, &cal_buf[0x50], 11);
	sysfs_finfo.concord_header_ver[11] = '\0';
#ifdef CONFIG_COMPANION_C1_USE
	sysfs_finfo.af_inf_addr = 0x1000;
	sysfs_finfo.af_macro_addr = 0x1008;
	sysfs_finfo.coef1_crc_addr= 0xF21A;
	sysfs_finfo.coef2_crc_addr = 0xF21E;
	sysfs_finfo.coef3_crc_addr = 0xF222;
	sysfs_finfo.coef4_crc_addr = 0xF226;
	sysfs_finfo.coef5_crc_addr = 0xF22A;
	sysfs_finfo.coef6_crc_addr = 0xF22E;
#endif
	sysfs_finfo.lsc_gain_crc_addr = 0x46FE;
	sysfs_finfo.pdaf_crc_addr = 0x9200;
#ifdef CONFIG_COMPANION_C2_USE
	sysfs_finfo.pdaf_shad_crc_addr = 0x9624;
	sysfs_finfo.xtalk_coef_crc_addr = 0xEA46;
	sysfs_finfo.lsc_parameter_crc_addr = 0x47E6;

	info("xtalk_coef_start = 0x%04x\n", sysfs_finfo.xtalk_coef_start);
	info("lsc_gain_crc_addr = 0x%04x\n", sysfs_finfo.lsc_gain_crc_addr);
	info("pdaf_crc_addr = 0x%04x\n", sysfs_finfo.pdaf_crc_addr);
	info("pdaf_shad_crc_addr = 0x%04x\n", sysfs_finfo.pdaf_shad_crc_addr);
	info("xtalk_coef_crc_addr= 0x%04x\n", sysfs_finfo.xtalk_coef_crc_addr);
	info("lsc_parameter_crc_addr = 0x%04x\n", sysfs_finfo.lsc_parameter_crc_addr);
#endif
#endif

	if (sysfs_finfo.setfile_end_addr < 0x8000 || sysfs_finfo.setfile_end_addr > 0x3fffff) {
		info("setfile end_addr has error!!  0x%08x\n", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}

	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	memcpy(sysfs_finfo.header_ver, &cal_buf[0x40], 11);
	sysfs_finfo.header_ver[11] = '\0';
	memcpy(sysfs_finfo.cal_map_ver, &cal_buf[0x60], 4);
	memcpy(sysfs_finfo.setfile_ver, &cal_buf[0x64], 6);
	sysfs_finfo.setfile_ver[6] = '\0';
	memcpy(sysfs_finfo.oem_ver, &cal_buf[0x1FE0], 11);
	sysfs_finfo.oem_ver[11] = '\0';
	memcpy(sysfs_finfo.awb_ver, &cal_buf[0x2FE0], 11);
	sysfs_finfo.awb_ver[11] = '\0';
	memcpy(sysfs_finfo.shading_ver, &cal_buf[0x4FE0], 11);
	sysfs_finfo.shading_ver[11] = '\0';
	memcpy(sysfs_finfo.project_name, &cal_buf[0x6E], 8);
	sysfs_finfo.project_name[8] = '\0';

	fw_core_version = sysfs_finfo.header_ver[0];
	/* debug info dump */
//#if defined(FROM_DEBUG)
#if 1
	info("++++ FROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", sysfs_finfo.header_ver);
#ifdef CONFIG_COMPANION_USE
	info("Companion version info : %s\n", sysfs_finfo.concord_header_ver);
#endif
	info(" ID : %c\n", sysfs_finfo.header_ver[0]);
	info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[1],
							sysfs_finfo.header_ver[2]);
	info(" ISP ID : %c\n", sysfs_finfo.header_ver[3]);
	info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[4]);
	info(" Year : %c\n", sysfs_finfo.header_ver[5]);
	info(" Month : %c\n", sysfs_finfo.header_ver[6]);
	info(" Release num : %c%c\n", sysfs_finfo.header_ver[7],
							sysfs_finfo.header_ver[8]);
	info(" Manufacturer ID : %c\n", sysfs_finfo.header_ver[9]);
	info(" Module ver : %c\n", sysfs_finfo.header_ver[10]);
	info("Cal data map ver : %s\n", sysfs_finfo.cal_map_ver);
	info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	info("Project name : %s\n", sysfs_finfo.project_name);
	info("2. OEM info\n");
	info("Module info : %s\n", sysfs_finfo.oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", sysfs_finfo.awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", sysfs_finfo.shading_ver);
	info("---- FROM data info\n");
#endif

	/* CRC check */
#ifdef CONFIG_COMPANION_USE
	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
			retry--;
			goto crc_retry;
		}
	} else {
		fw_version_crc_check = false;
		crc32_check = false;
		crc32_c1_check = false;
	}
#else
	if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}
#endif

	if (sysfs_finfo.header_ver[3] == 'L') {
		crc32_check_factory = crc32_check;
#ifdef CONFIG_COMPANION_USE
		crc32_c1_check_factory = crc32_c1_check;
#endif
	} else {
		crc32_check_factory = false;
#ifdef CONFIG_COMPANION_USE
		crc32_c1_check_factory = false;
#endif
	}

#ifdef CONFIG_COMPANION_USE
	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (crc32_check && crc32_c1_check) {
			/* If FROM LSC value is not valid, loading default lsc data */
			if (*((u32 *)&cal_buf[sysfs_finfo.lsc_gain_start_addr]) == 0x00000000) {
				companion_lsc_isvalid = false;
			} else {
				companion_lsc_isvalid = true;
			}
#ifdef CONFIG_COMPANION_C2_USE
			if (*((u32 *)&cal_buf[sysfs_finfo.xtalk_coef_start]) == 0x00000000)
#endif
#ifdef CONFIG_COMPANION_C1_USE
			if (*((u32 *)&cal_buf[sysfs_finfo.coef1_start]) == 0x00000000)
#endif
			{
				companion_coef_isvalid = false;
			} else {
				companion_coef_isvalid = true;
			}
		} else {
			companion_lsc_isvalid = false;
			companion_coef_isvalid = false;
		}
	} else {
		companion_lsc_isvalid = true;
		companion_coef_isvalid = true;
	}
#endif

	if (!core_pdata->use_module_check) {
		is_latest_cam_module = true;
	} else {
		if (sysfs_finfo.header_ver[10] >= FIMC_IS_LATEST_FROM_VERSION_B) {
			is_latest_cam_module = true;
		} else {
			is_latest_cam_module = false;
		}
	}

	if (core_pdata->use_module_check) {
		if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
			is_final_cam_module = true;
		} else {
			is_final_cam_module = false;
		}
	} else {
		is_final_cam_module = true;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump FROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/from_cal.bin", cal_buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
				info("Failedl to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);
exit:
	return ret;
}

int fimc_is_sec_readfw(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char fw_path[100];
	char setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;

	info("Camera: FW, Setfile need to be dumped\n");

crc_retry:
	/* read fw data */
	info("Camera: Start SPI read fw data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.bin_start_addr, FIMC_IS_MAX_FW_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read fw data\n");

	/* CRC check */
	if (!fimc_is_sec_check_fw_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_fw_name);

	if (write_data_to_file(fw_path, fw_buf,
		sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: FW Data has dumped successfully\n");

setfile_crc_retry:
	/* read setfile data */
	info("Camera: Start SPI read setfile data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.setfile_start_addr, FIMC_IS_MAX_SETFILE_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read fw data\n");

	/* CRC check */
	if (!fimc_is_sec_check_setfile_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto setfile_crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_setfile_name);
	pos = 0;

	if (write_data_to_file(setfile_path, fw_buf,
			sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Setfile has dumped successfully\n");
	info("Camera: FW, Setfile were dumped successfully\n");

exit:
	return ret;
}
#endif

#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_read_companion_fw(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char fw_path[100];
	//char master_setfile_path[100];
	//char mode_setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;

	info("Camera: Companion FW, Setfile need to be dumped\n");

crc_retry:
	/* read companion fw data */
	info("Camera: Start SPI read companion fw data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.concord_bin_start_addr, FIMC_IS_MAX_COMPANION_FW_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read companion fw data\n");

	/* CRC check */
	if (!fimc_is_sec_check_companion_fw_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s",
		FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_fw_name);

	if (write_data_to_file(fw_path, fw_buf,
		sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Companion FW Data has dumped successfully\n");
#if 0
	snprintf(master_setfile_path, sizeof(master_setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_mastersetf_name);
	snprintf(mode_setfile_path, sizeof(mode_setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_modesetf_name);
	pos = 0;

	if (write_data_to_file(master_setfile_path,
			comp_fw_buf + sysfs_finfo.concord_master_setfile_start_addr,
			sysfs_finfo.concord_master_setfile_end_addr - sysfs_finfo.concord_master_setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}
	pos = 0;
	if (write_data_to_file(mode_setfile_path,
			comp_fw_buf + sysfs_finfo.concord_mode_setfile_start_addr,
			sysfs_finfo.concord_mode_setfile_end_addr - sysfs_finfo.concord_mode_setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Companion Setfile has dumped successfully\n");
#endif

exit:
	return ret;
}
#endif

#if 0
int fimc_is_sec_gpio_enable(struct exynos_platform_fimc_is *pdata, char *name, bool on)
{
	struct gpio_set *gpio;
	int ret = 0;
	int i = 0;

	for (i = 0; i < FIMC_IS_MAX_GPIO_NUM; i++) {
			gpio = &pdata->gpio_info->cfg[i];
			if (strcmp(gpio->name, name) == 0)
				break;
			else
				continue;
	}

	if (i == FIMC_IS_MAX_GPIO_NUM) {
		err("GPIO %s is not found!!", name);
		ret = -EINVAL;
		goto exit;
	}

	ret = gpio_request(gpio->pin, gpio->name);
	if (ret) {
		err("Request GPIO error(%s)", gpio->name);
		goto exit;
	}

	if (on) {
		switch (gpio->act) {
		case GPIO_PULL_NONE:
			s3c_gpio_cfgpin(gpio->pin, gpio->value);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			break;
		case GPIO_OUTPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_INPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_RESET:
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_direction_output(gpio->pin, 0);
			gpio_direction_output(gpio->pin, 1);
			break;
		default:
			err("unknown act for gpio");
			break;
		}
	} else {
		s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_DOWN);
	}

	gpio_free(gpio->pin);

exit:
	return ret;
}
#endif

int fimc_is_sec_get_pixel_size(char *header_ver)
{
	int pixelsize = 0;

	pixelsize += (int) (header_ver[FW_PIXEL_SIZE] - 0x30) * 10;
	pixelsize += (int) (header_ver[FW_PIXEL_SIZE + 1] - 0x30);

	return pixelsize;
}

int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver)
{
	struct regulator *regulator = NULL;
	int ret = 0;
	int minV, maxV;
	int pixelSize = 0;

	regulator = regulator_get(dev, "cam_sensor_core_1.2v");
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get fail",
			__func__);
		return -EINVAL;
	}
	pixelSize = fimc_is_sec_get_pixel_size(header_ver);

	if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SONY) {
		if (pixelSize == 13) {
			minV = 1050000;
			maxV = 1050000;
		} else if (pixelSize == 8) {
			minV = 1100000;
			maxV = 1100000;
		} else {
			minV = 1050000;
			maxV = 1050000;
		}
	} else if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SLSI) {
		minV = 1200000;
		maxV = 1200000;
	} else {
		minV = 1050000;
		maxV = 1050000;
	}

	ret = regulator_set_voltage(regulator, minV, maxV);

	if (ret >= 0)
		info("%s : set_core_voltage %d, %d successfully\n",
				__func__, minV, maxV);
	regulator_put(regulator);

	return ret;
}

/**
 * fimc_is_sec_ldo_enabled: check whether the ldo has already been enabled.
 *
 * @ return: true, false or error value
 */
static int fimc_is_sec_ldo_enabled(struct device *dev, char *name) {
	struct regulator *regulator = NULL;
	int enabled = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail", __func__, name);
		return -EINVAL;
	}

	enabled = regulator_is_enabled(regulator);

	regulator_put(regulator);

	return enabled;
}

int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on)
{
	struct regulator *regulator = NULL;
	int ret = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail", __func__, name);
		return -EINVAL;
	}

	if (on) {
		if (regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already enabled\n", name);
			goto exit;
		}

		ret = regulator_enable(regulator);
		if (ret) {
			err("%s : regulator_enable(%s) fail", __func__, name);
			goto exit;
		}
	} else {
		if (!regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already disabled\n", name);
			goto exit;
		}

		ret = regulator_disable(regulator);
		if (ret) {
			err("%s : regulator_disable(%s) fail", __func__, name);
			goto exit;
		}
	}

exit:
	regulator_put(regulator);

	return ret;
}

int fimc_is_sec_fw_find(struct fimc_is_core *core)
{
	int sensor_id = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_F) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_I) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_A)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K2P2;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_12M)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K2P2_12M;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P3)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K2P3;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4H5)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K4H5;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX240)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX240), "%s", FIMC_IS_FW_IMX240);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX240_SETF), "%s", FIMC_IS_IMX240_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_IMX240;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2T2)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2T2_EVT1), "%s", FIMC_IS_FW_2T2_EVT1);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2T2_SETF), "%s", FIMC_IS_2T2_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K2T2;
	} else {
		/* default firmware and setfile */
		sensor_id = core_pdata->rear_sensor_id;
		if (sensor_id == SENSOR_NAME_IMX240) {
			/* IMX240 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX240), "%s", FIMC_IS_FW_IMX240);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX240_SETF), "%s", FIMC_IS_IMX240_SETF);
		} else if (sensor_id == SENSOR_NAME_IMX134) {
			/* IMX134 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K2P2_12M) {
			/* 2P2_12M */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K2P2) {
			/* 2P2 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K4H5) {
			/* 4H5 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
		} else if ( sensor_id == SENSOR_NAME_S5K2P3 ) {
			/* 2P3 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K2T2) {
			/* 2T2 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2T2_EVT1), "%s", FIMC_IS_FW_2T2_EVT1);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2T2_SETF), "%s", FIMC_IS_2T2_SETF);
		} else {
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		}
	}

	return 0;
}

int fimc_is_sec_run_fw_sel(struct device *dev, int position)
{
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	int ret = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_REAR, true);
#else
/* When using C2 retention, Cal loading for both front and rear cam will be done at a time */
#if !defined(CONFIG_COMPANION_C2_USE)
			ret = fimc_is_sec_fw_sel(core, dev, true);
#endif
#endif
		}

		if (!sysfs_finfo_front.is_caldata_read || force_caldata_dump) {
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_FRONT, false);
			if (ret < 0) {
				err("failed to select firmware (%d)", ret);
				goto p_err;
			}
		}
	} else
#endif
	{
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_REAR, false);
#else
			ret = fimc_is_sec_fw_sel(core, dev, false);
#endif
			if (ret < 0) {
				err("failed to select firmware (%d)", ret);
				goto p_err;
			}
		}
	}

p_err:
	if (position == SENSOR_POSITION_REAR) {
		if (core_pdata->check_sensor_vendor) {
			if (fimc_is_sec_check_from_ver(core, position)) {
				if (sysfs_finfo.header_ver[3] != 'L') {
					err("Not supported module. Module ver = %s", sysfs_finfo.header_ver);
					return  -EIO;
				}
			}
		}
	}
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	else {
		if (core_pdata->check_sensor_vendor) {
			if (fimc_is_sec_check_from_ver(core, position)) {
				if (sysfs_finfo_front.header_ver[3] != 'L') {
					err("Not supported front module. Module ver = %s", sysfs_finfo_front.header_ver);
					return  -EIO;
				}
			}
		}
	}
#endif

	info("%s:%d\n", __func__, ret);
	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_fw_sel_eeprom(struct device *dev, int id, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char phone_fw_version[12] = {0, };

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_ldo_enabled[2];
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);

	is_ldo_enabled[0] = false;
	is_ldo_enabled[1] = false;

	/* Use mutex for i2c read */
	mutex_lock(&core->spi_lock);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		if (!sysfs_finfo_front.is_caldata_read || force_caldata_dump) {
			if (force_caldata_dump)
				info("forced caldata dump!!\n");

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_VT")) {
				ret = fimc_is_sec_ldo_enable(dev, "VDDIO_1.8V_VT", true);
				if (ret) {
					err("fimc_is_sec_fw_sel_eeprom: error, failed to VDDIO_1.8V_VT(on)");
					goto exit;
				}
				is_ldo_enabled[0] = true;
			}

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
				ret = fimc_is_sec_ldo_enable(dev, "VDDIO_1.8V_CAM", true);
				if (ret) {
					err("fimc_is_sec_fw_sel_eeprom: error, failed to VDDIO_1.8V_CAM(on)");
					goto exit;
				}
				is_ldo_enabled[1] = true;
			}

			info("Camera: read cal data from Front EEPROM\n");
			if ((fimc_is_sec_readcal_eeprom(dev, SENSOR_POSITION_FRONT) != -EIO) &&
					crc32_header_check_front) {
				sysfs_finfo_front.is_caldata_read = true;
			}
		}
		goto exit;
	} else
#endif
	{
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
			is_dumped_fw_loading_needed = false;
			if (force_caldata_dump)
				info("forced caldata dump!!\n");

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
				ret = fimc_is_sec_ldo_enable(dev, "VDDIO_1.8V_CAM", true);
				if (ret) {
					err("fimc_is_sec_fw_sel_eeprom: error, failed to VDDIO_1.8V_CAM(on)");
					goto exit;
				}
				is_ldo_enabled[0] = true;
			}

			info("Camera: read cal data from Rear EEPROM\n");
			if (headerOnly) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
				fimc_is_sec_read_eeprom_header(dev);
#endif
			} else {
				if ((fimc_is_sec_readcal_eeprom(dev, SENSOR_POSITION_REAR) != -EIO) &&
						crc32_header_check) {
					sysfs_finfo.is_caldata_read = true;
				}
			}
		}
	}

	fimc_is_sec_fw_find(core);
	if (headerOnly) {
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, sysfs_finfo.load_fw_name);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		err("Camera: Failed open phone firmware");
		ret = -EIO;
		fp = NULL;
		goto read_phone_fw_exit;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n",
		fw_path, fsize);
	if (fsize > FIMC_IS_MAX_FW_SIZE) {
		info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
		read_buf = vmalloc(fsize);
		if (!read_buf) {
			err("failed to allocate memory");
			ret = -ENOMEM;
			goto read_phone_fw_exit;
		}
		temp_buf = read_buf;
	} else {
		memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
		temp_buf = fw_buf;
	}
	nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		err("failed to read firmware file, %ld Bytes", nread);
		ret = -EIO;
		goto read_phone_fw_exit;
	}

	strncpy(phone_fw_version, temp_buf + nread - 11, 11);
	strncpy(sysfs_pinfo.header_ver, temp_buf + nread - 11, 11);
	info("Camera: phone fw version: %s\n", phone_fw_version);

read_phone_fw_exit:
	if (read_buf) {
		vfree(read_buf);
		read_buf = NULL;
		temp_buf = NULL;
	}

	if (fp) {
		filp_close(fp, current->files);
		fp = NULL;
	}

	set_fs(old_fs);

exit:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		if (is_ldo_enabled[0] && !core->running_front_camera) {
			ret = fimc_is_sec_ldo_enable(dev, "VDDIO_1.8V_VT", false);
			if (ret)
				err("fimc_is_sec_fw_sel_eeprom: error, failed to VDDIO_1.8V_VT(off)");
		}
		if (is_ldo_enabled[1] && !core->running_rear_camera) {
			ret = fimc_is_sec_ldo_enable(dev, "VDDIO_1.8V_CAM", false);
			if (ret)
				err("fimc_is_sec_fw_sel_eeprom: error, failed to VDDIO_1.8V_CAM(off)");
		}
	} else
#endif
	{
		if (is_ldo_enabled[0] && !core->running_rear_camera) {
			ret = fimc_is_sec_ldo_enable(dev, "VDDIO_1.8V_CAM", false);
			if (ret)
				err("fimc_is_sec_fw_sel_eeprom: error, failed to VDDIO_1.8V_CAM(off)");
		}
	}

	mutex_unlock(&core->spi_lock);

	return ret;
}
#endif

#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_fw_sel(struct fimc_is_core *core, struct device *dev, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char dump_fw_path[100];
	char dump_fw_version[12] = {0, };
	char phone_fw_version[12] = {0, };
	int from_fw_revision = 0;
	int dump_fw_revision = 0;
	int phone_fw_revision = 0;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = true;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_spi_gpio *spi_gpio = &core->spi0.gpio;
#endif
	bool is_ldo_enabled = false;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null\n");
		return -EINVAL;
	}

	/* Use mutex for spi read */
	mutex_lock(&core->spi_lock);
	if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
		is_dumped_fw_loading_needed = false;
		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
			info("enable %s in the %s\n", "VDDIO_1.8V_CAM", __func__);
			ret = fimc_is_sec_ldo_enable(dev, "VDDIO_1.8V_CAM", true);
			if (ret) {
				err("fimc_is_sec_fw_sel: error, failed to cam_io(on)");
				goto exit;
			}

			is_ldo_enabled = true;
		}
		info("read cal data from FROM\n");
#ifdef CONFIG_COMPANION_USE
		fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);
#endif

		if (headerOnly) {
			fimc_is_sec_read_from_header(dev);
		} else {
			if ((fimc_is_sec_readcal(core) != -EIO) &&
					crc32_header_check) {
				sysfs_finfo.is_caldata_read = true;
			}
		}
#ifdef CONFIG_COMPANION_USE
		fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_OUTPUT, true);
#endif
		/*select AF actuator*/
		if (!crc32_header_check) {
			info("Camera : CRC32 error for all section.\n");
		}

		fimc_is_sec_fw_find(core);
		if (headerOnly) {
			goto exit;
		}

		snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, sysfs_finfo.load_fw_name);

		snprintf(dump_fw_path, sizeof(dump_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_fw_name);
		info("Camera: f-rom fw version: %s\n", sysfs_finfo.header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		fp = filp_open(dump_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			info("Camera: There is no dumped firmware\n");
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			dump_fw_path, fsize);
		if (fsize > FIMC_IS_MAX_FW_SIZE) {
			info("Dumped FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw;
			}
			temp_buf = read_buf;
		} else {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
			temp_buf = fw_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_fw_version, temp_buf + nread-11, 11);
		info("Camera: dumped fw version: %s\n", dump_fw_version);

read_phone_fw:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			err("Camera: Failed open phone firmware");
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n", fw_path, fsize);
		if (fsize > FIMC_IS_MAX_FW_SIZE) {
			info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw_exit;
			}
			temp_buf = read_buf;
		} else {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
			temp_buf = fw_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_fw_version, temp_buf + nread - 11, 11);
		strncpy(sysfs_pinfo.header_ver, temp_buf + nread - 11, 11);
		info("Camera: phone fw version: %s\n", phone_fw_version);

read_phone_fw_exit:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		from_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.header_ver);
		phone_fw_revision = fimc_is_sec_fw_revision(phone_fw_version);
		if (is_dump_existed) {
			dump_fw_revision = fimc_is_sec_fw_revision(dump_fw_version);
		}

		info("from_fw_revision = %d, phone_fw_revision = %d, dump_fw_revision = %d\n",
			from_fw_revision, phone_fw_revision, dump_fw_revision);

		if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, phone_fw_version)) ||
			(from_fw_revision > phone_fw_revision)) {
			is_dumped_fw_loading_needed = true;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver,
							dump_fw_version)) {
					is_dump_needed = true;
				} else if (from_fw_revision > dump_fw_revision) {
					is_dump_needed = true;
				} else {
					is_dump_needed = false;
				}
			} else {
				is_dump_needed = true;
			}
		} else {
			is_dump_needed = false;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(phone_fw_version,
					dump_fw_version)) {
					is_dumped_fw_loading_needed = false;
				} else if (phone_fw_revision > dump_fw_revision) {
					is_dumped_fw_loading_needed = false;
				} else {
					is_dumped_fw_loading_needed = true;
				}
			} else {
				is_dumped_fw_loading_needed = false;
			}
		}

		if (is_dump_needed) {
			info("Dump ISP Firmware.\n");
#ifdef CONFIG_COMPANION_USE
			fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);
#endif
			ret = fimc_is_sec_readfw(core);
#ifdef CONFIG_COMPANION_USE
			fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, true);
#endif
			if (ret < 0) {
				if (!crc32_fw_check || !crc32_setfile_check) {
					is_dumped_fw_loading_needed = false;
					err("Firmware CRC is not valid. Does not use dumped firmware.\n");
				}
			}
		}

		if (phone_fw_version[0] == 0) {
			strcpy(sysfs_pinfo.header_ver, "NULL");
		}

		if (is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, sysfs_finfo.header_ver, 11);
		} else if (!is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, dump_fw_version, 11);
		} else {
			strncpy(loaded_fw, phone_fw_version, 11);
		}

	} else {
		info("already loaded the firmware, Phone version=%s, F-ROM version=%s\n",
			sysfs_pinfo.header_ver, sysfs_finfo.header_ver);
	}

exit:
#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_pin(spi_gpio, PINCFG_TYPE_FUNC, 0);
#endif
	if (is_ldo_enabled && !core->running_rear_camera) {
		info("disable %s in the %s\n", "VDDIO_1.8V_CAM", __func__);
		ret = fimc_is_sec_ldo_enable(dev, "VDDIO_1.8V_CAM", false);
		if (ret)
			err("fimc_is_sec_fw_sel: error, failed to cam_io(off)");
	}

	mutex_unlock(&core->spi_lock);

	return ret;
}
#endif

#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_concord_fw_sel(struct fimc_is_core *core, struct device *dev)
{
	int ret = 0;
	char c1_fw_path[100];
	char dump_c1_fw_path[100];
	char dump_c1_fw_version[12] = {0, };
	char phone_c1_fw_version[12] = {0, };
	int from_c1_fw_revision = 0;
	int dump_c1_fw_revision = 0;
	int phone_c1_fw_revision = 0;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = true;
	int sensor_id = 0;
	struct fimc_is_spi_gpio *spi_gpio = &core->spi0.gpio;

	if ((!sysfs_finfo.is_c1_caldata_read &&
	    (cam_id == CAMERA_SINGLE_REAR /* || cam_id == CAMERA_DUAL_FRONT*/)) ||
	    force_caldata_dump) {
		is_dumped_c1_fw_loading_needed = false;
		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		info("Load companion fw from FROM, fw header version = %s\n", sysfs_finfo.concord_header_ver);
			sysfs_finfo.is_c1_caldata_read = true;

		if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_F) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_I) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_A)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX240)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_12M)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_12M_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2T2)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2T2_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2T2_EVT1), "%s", FIMC_IS_FW_COMPANION_2T2_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_2T2;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2T2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2T2_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2T2_MODE_SETF), "%s", FIMC_IS_COMPANION_2T2_MODE_SETF);
		} else {
			err("Companion FW selection failed! Default FW will be used");
			sensor_id = core->pdata->rear_sensor_id;
			if (sensor_id == SENSOR_NAME_IMX240) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2P2_12M) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_12M_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2P2) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2T2) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2T2_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2T2_EVT1), "%s", FIMC_IS_FW_COMPANION_2T2_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_2T2;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2T2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2T2_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2T2_MODE_SETF), "%s", FIMC_IS_COMPANION_2T2_MODE_SETF);
			} else {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
			}
		}

		snprintf(dump_c1_fw_path, sizeof(dump_c1_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_fw_name);
		info("Camera: f-rom fw version: %s\n", sysfs_finfo.concord_header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = 0;
		fp = filp_open(dump_c1_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			info("Camera: There is no dumped Companion firmware\n");
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			dump_c1_fw_path, fsize);
		if (fsize > FIMC_IS_MAX_FW_SIZE) {
			info("Dumped Companion FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw;
			}
			temp_buf = read_buf;
		} else {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
			temp_buf = fw_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_c1_fw_version, temp_buf + nread - 16, 11);
		info("Camera: dumped companion fw version: %s\n", dump_c1_fw_version);

read_phone_fw:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);
		if (ret < 0)
			goto exit;

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(c1_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			err("Camera: Failed open phone companion firmware");
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			c1_fw_path, fsize);
		if (fsize > FIMC_IS_MAX_FW_SIZE) {
			info("Phone Companion FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw_exit;
			}
			temp_buf = read_buf;
		} else {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
			temp_buf = fw_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read companion firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_c1_fw_version, temp_buf + nread - 16, 11);
		strncpy(sysfs_pinfo.concord_header_ver, temp_buf + nread - 16, 11);
		info("Camera: phone companion fw version: %s\n", phone_c1_fw_version);

read_phone_fw_exit:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		from_c1_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.concord_header_ver);
		phone_c1_fw_revision = fimc_is_sec_fw_revision(phone_c1_fw_version);
		if (is_dump_existed) {
			dump_c1_fw_revision = fimc_is_sec_fw_revision(dump_c1_fw_version);
		}

		info("from_c1_fw_revision = %d, phone_c1_fw_revision = %d, dump_c1_fw_revision = %d\n",
			from_c1_fw_revision, phone_c1_fw_revision, dump_c1_fw_revision);

		if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, phone_c1_fw_version)) ||
				(from_c1_fw_revision > phone_c1_fw_revision)) {
			is_dumped_c1_fw_loading_needed = true;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver,
							dump_c1_fw_version)) {
					is_dump_needed = true;
				} else if (from_c1_fw_revision > dump_c1_fw_revision) {
					is_dump_needed = true;
				} else {
					is_dump_needed = false;
				}
			} else {
				is_dump_needed = true;
			}
		} else {
			is_dump_needed = false;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(phone_c1_fw_version,
							dump_c1_fw_version)) {
					is_dumped_c1_fw_loading_needed = false;
				} else if (phone_c1_fw_revision > dump_c1_fw_revision) {
					is_dumped_c1_fw_loading_needed = false;
				} else {
					is_dumped_c1_fw_loading_needed = true;
				}
			} else {
				is_dumped_c1_fw_loading_needed = false;
			}
		}

		if (is_dump_needed) {
			info("Dump companion Firmware.\n");
			fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);
			ret = fimc_is_sec_read_companion_fw(core);
			fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, true);
			if (ret < 0) {
				if (!crc32_c1_fw_check) {
					is_dumped_c1_fw_loading_needed = false;
					err("Companion Firmware CRC is not valid. Does not use dumped firmware.\n");
				}
			}
		}

		if (phone_c1_fw_version[0] == 0) {
			strcpy(sysfs_pinfo.concord_header_ver, "NULL");
		}

		if (is_dump_needed && is_dumped_c1_fw_loading_needed) {
			strncpy(loaded_companion_fw, sysfs_finfo.concord_header_ver, 11);
		} else if (!is_dump_needed && is_dumped_c1_fw_loading_needed) {
			strncpy(loaded_companion_fw, dump_c1_fw_version, 11);
		} else {
			strncpy(loaded_companion_fw, phone_c1_fw_version, 11);
		}
	} else {
		info("already loaded the firmware, Phone_Comp version=%s, F-ROM_Comp version=%s\n",
			sysfs_pinfo.concord_header_ver, sysfs_finfo.concord_header_ver);
	}

exit:
	return ret;
}
#endif
