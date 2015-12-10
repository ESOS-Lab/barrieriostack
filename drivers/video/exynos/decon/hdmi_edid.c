/*
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <uapi/linux/v4l2-dv-timings.h>

#include "hdmi.h"

#define EDID_SEGMENT_ADDR	(0x60 >> 1)
#define EDID_ADDR		(0xA0 >> 1)
#define EDID_SEGMENT_IGNORE	(2)
#define EDID_BLOCK_SIZE		128
#define EDID_SEGMENT(x)		((x) >> 1)
#define EDID_OFFSET(x)		(((x) & 1) * EDID_BLOCK_SIZE)
#define EDID_EXTENSION_FLAG	0x7E
#define EDID_NATIVE_FORMAT	0x83
#define EDID_BASIC_AUDIO	(1 << 6)
#define EDID_3D_STRUCTURE_ALL	0x1
#define EDID_3D_STRUCTURE_MASK	0x2
#define EDID_3D_FP_MASK		(1)
#define EDID_3D_TB_MASK		(1 << 6)
#define EDID_3D_SBS_MASK	(1 << 8)
#define EDID_3D_FP		0
#define EDID_3D_TB		6
#define EDID_3D_SBS		8

#ifdef CONFIG_OF
static const struct of_device_id edid_device_table[] = {
	        { .compatible = "samsung,exynos5-edid_driver" },
		{},
};
MODULE_DEVICE_TABLE(of, edid_device_table);
#endif

static struct i2c_client *edid_client;

/* Structure for Checking 3D Mandatory Format in EDID */
static const struct edid_3d_mandatory_preset {
	struct v4l2_dv_timings dv_timings;
	u16 xres;
	u16 yres;
	u16 refresh;
	u32 vmode;
	u32 s3d;
} edid_3d_mandatory_presets[] = {
	{ V4L2_DV_BT_CEA_1280X720P60_FP,	1280, 720, 60, FB_VMODE_NONINTERLACED, EDID_3D_FP },
	{ V4L2_DV_BT_CEA_1280X720P60_TB,	1280, 720, 60, FB_VMODE_NONINTERLACED, EDID_3D_TB },
	{ V4L2_DV_BT_CEA_1280X720P50_FP,	1280, 720, 50, FB_VMODE_NONINTERLACED, EDID_3D_FP },
	{ V4L2_DV_BT_CEA_1280X720P50_TB,	1280, 720, 50, FB_VMODE_NONINTERLACED, EDID_3D_TB },
	{ V4L2_DV_BT_CEA_1920X1080P24_FP,	1920, 1080, 24, FB_VMODE_NONINTERLACED, EDID_3D_FP },
	{ V4L2_DV_BT_CEA_1920X1080P24_TB,	1920, 1080, 24, FB_VMODE_NONINTERLACED, EDID_3D_TB },
};

static struct edid_preset {
	struct v4l2_dv_timings dv_timings;
	u16 xres;
	u16 yres;
	u16 refresh;
	u32 vmode;
	char *name;
	bool supported;
} edid_presets[] = {
	{ V4L2_DV_BT_CEA_720X480P59_94,	720,  480,  59, FB_VMODE_NONINTERLACED, "480p@59.94" },
	{ V4L2_DV_BT_CEA_720X576P50,	720,  576,  50, FB_VMODE_NONINTERLACED, "576p@50" },
	{ V4L2_DV_BT_CEA_1280X720P50,	1280, 720,  50, FB_VMODE_NONINTERLACED, "720p@50" },
	{ V4L2_DV_BT_CEA_1280X720P60,	1280, 720,  60, FB_VMODE_NONINTERLACED, "720p@60" },
	{ V4L2_DV_BT_CEA_1920X1080P24,	1920, 1080, 24, FB_VMODE_NONINTERLACED, "1080p@24" },
	{ V4L2_DV_BT_CEA_1920X1080P25,	1920, 1080, 25, FB_VMODE_NONINTERLACED, "1080p@25" },
	{ V4L2_DV_BT_CEA_1920X1080P30,	1920, 1080, 30, FB_VMODE_NONINTERLACED, "1080p@30" },
	{ V4L2_DV_BT_CEA_1920X1080P50,	1920, 1080, 50, FB_VMODE_NONINTERLACED, "1080p@50" },
	{ V4L2_DV_BT_CEA_1920X1080P60,	1920, 1080, 60, FB_VMODE_NONINTERLACED, "1080p@60" },
	{ V4L2_DV_BT_CEA_3840X2160P24,  3840, 2160, 24, FB_VMODE_NONINTERLACED, "2160p@24" },
	{ V4L2_DV_BT_CEA_3840X2160P25,	3840, 2160, 25, FB_VMODE_NONINTERLACED, "2160p@25" },
	{ V4L2_DV_BT_CEA_3840X2160P30,	3840, 2160, 30, FB_VMODE_NONINTERLACED, "2160p@30" },
	{ V4L2_DV_BT_CEA_4096X2160P24,	4096, 2160, 24, FB_VMODE_NONINTERLACED, "(4096x)2160p@24" },
#ifndef CONFIG_SEC_MHL_SUPPORT
	{ V4L2_DV_BT_CEA_1920X1080I50,	1920, 1080, 50, FB_VMODE_INTERLACED, "1080i@50" },
	{ V4L2_DV_BT_CEA_1920X1080I60,	1920, 1080, 60, FB_VMODE_INTERLACED, "1080i@60" },
#endif
};

static struct edid_3d_preset {
	struct v4l2_dv_timings dv_timings;
	u16 xres;
	u16 yres;
	u16 refresh;
	u32 vmode;
	u32 s3d;
	char *name;
	bool supported;
} edid_3d_presets[] = {
	{ V4L2_DV_BT_CEA_1280X720P60_SB_HALF,	1280, 720, 60,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "720p@60_SBS" },
	{ V4L2_DV_BT_CEA_1280X720P60_TB,	1280, 720, 60,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "720p@60_TB" },
	{ V4L2_DV_BT_CEA_1280X720P50_SB_HALF,	1280, 720, 50,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "720p@50_SBS" },
	{ V4L2_DV_BT_CEA_1280X720P50_TB,	1280, 720, 50,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "720p@50_TB" },
	{ V4L2_DV_BT_CEA_1920X1080P24_FP,	1920, 1080, 24,
		FB_VMODE_NONINTERLACED, EDID_3D_FP, "1080p@24_FP" },
	{ V4L2_DV_BT_CEA_1920X1080P24_SB_HALF,	1920, 1080, 24,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "1080p@24_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080P24_TB,	1920, 1080, 24,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "1080p@24_TB" },
	{ V4L2_DV_BT_CEA_1920X1080I60_SB_HALF,	1920, 1080, 60,
		FB_VMODE_INTERLACED, EDID_3D_SBS, "1080i@60_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080I50_SB_HALF,	1920, 1080, 50,
		FB_VMODE_INTERLACED, EDID_3D_SBS, "1080i@50_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080P60_SB_HALF,	1920, 1080, 60,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "1080p@60_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080P60_TB,	1920, 1080, 60,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "1080p@60_TB" },
	{ V4L2_DV_BT_CEA_1920X1080P30_SB_HALF,	1920, 1080, 30,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "1080p@30_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080P30_TB,	1920, 1080, 30,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "1080p@30_TB" },
};

static struct v4l2_dv_timings preferred_preset;
static u32 edid_misc;
static int max_audio_channels;
static int audio_bit_rates;
static int audio_sample_rates;
static u32 source_phy_addr;

#ifdef CONFIG_SEC_MHL_EDID
static u8 exynos_hdmi_edid[EDID_MAX_LENGTH];
static enum MHL_MAX_RESOLUTION mhl_max_res = MHL_1080P_30;

int hdmi_forced_resolution = -1;

int hdmi_get_datablock_offset(u8 *edid, enum extension_edid_db datablock,
							int *offset)
{
	int i = 0, length = 0;
	u8 current_byte, disp;

	if (edid[0x7e] == 0x00)
		return 1;

	disp = edid[(0x80) + 2];
	pr_info("HDMI: Extension block present db %d %x\n", datablock, disp);
	if (disp == 0x4)
		return 1;

	i = 0x80 + 0x4;
	while (i < (0x80 + disp)) {
		current_byte = edid[i];
		if ((current_byte >> 5) == datablock) {
			*offset = i;
			pr_info("HDMI: datablock %d %d\n", datablock, *offset);
			return 0;
		} else {
			length = (current_byte &
				HDMI_EDID_EX_DATABLOCK_LEN_MASK) + 1;
			i += length;
		}
	}
	return 1;
}

int hdmi_send_audio_info(int onoff, struct hdmi_device *hdev)
{
	int offset, j = 0, length = 0;
	enum extension_edid_db vsdb;
	u16 audio_info = 0, data_byte = 0;
	char *edid = (char *) exynos_hdmi_edid;

	if (onoff) {
		/* get audio channel info */
		vsdb = DATABLOCK_AUDIO;
		if (!hdmi_get_datablock_offset(edid, vsdb, &offset)) {
			data_byte = edid[offset];
			length = data_byte & HDMI_EDID_EX_DATABLOCK_LEN_MASK;

			if (length >= HDMI_AUDIO_FORMAT_MAX_LENGTH)
				length = HDMI_AUDIO_FORMAT_MAX_LENGTH;

			for (j = 1 ; j < length ; j++) {
				if (j%3 == 1) {
					data_byte = edid[offset + j];
					audio_info |= 1 << (data_byte & 0x07);
				}
			}
			pr_info("HDMI: Audio supported ch = %d\n", audio_info);
		}

		/* get speaker info */
		vsdb = DATABLOCK_SPEAKERS;
		if (!hdmi_get_datablock_offset(edid, vsdb, &offset)) {
			data_byte = edid[offset + 1];
			pr_info("HDMI: speaker alloc = %d\n",
						(data_byte & 0x7F));
			audio_info |= (data_byte & 0x7F) << 8;
		}

		pr_info("HDMI: Audio info = %d\n", audio_info);
	}

	return audio_info;
}

#ifdef CONFIG_SAMSUNG_MHL_8240
int exynos_get_edid(u8 *edid, int size, u8 link_mode)
{
	mhl_max_res = MHL_1080P_30;

	pr_info("%s: size=%d\n", __func__, size);

	if (size > EDID_MAX_LENGTH) {
		pr_err("%s: size is over %d\n", __func__, size);
		return -1;
	}
	memcpy(exynos_hdmi_edid, edid, size);

	if (!(link_mode & (0x1 << 3)))
		mhl_max_res = MHL_1080P_30;
	else
		mhl_max_res = MHL_1080P_60;

	pr_info("%s, link_mode= %d max_resolution %d\n", __func__, link_mode, mhl_max_res);

	return 0;
}
EXPORT_SYMBOL(exynos_get_edid);

#else /* CONFIG_SAMSUNG_MHL_8240 */
enum MHL_MAX_RESOLUTION check_mhl_max_resolution(u8 mhl_ver, int
		mhl_link_mode, int tmds_speed)
{
	bool support_ppixel_mode =
		mhl_link_mode &  MHL_DEV_VID_LINK_SUPP_PPIXEL ? true : false;
	bool support_16ppixel_mode =
		mhl_link_mode &  MHL_DEV_VID_LINK_SUPP_16BPP ? true : false;

	if (mhl_ver < 0x20 && !support_ppixel_mode)
		return MHL_1080P_30;

	else if (mhl_ver < 0x30 && support_ppixel_mode)
		return MHL_1080P_60;

	else if (mhl_ver >= 0x30) {
		if (tmds_speed == MHL_XDC_TMDS_150)
			return MHL_576P_50;
		else if (tmds_speed == MHL_XDC_TMDS_300)
			return MHL_1080P_30;
		else if (!support_16ppixel_mode)
			return MHL_1080P_60;
		else
			return MHL_UHD;
	}

	return MHL_1080P_60;
}

int exynos_get_edid(u8 *edid, int size, u8 mhl_ver, int mhl_link_mode,
		int tmds_speed)
{
	mhl_max_res = MHL_1080P_30;

	if (size > EDID_MAX_LENGTH) {
		pr_err("%s: size is over %d\n", __func__, size);
		return -1;
	}
	memcpy(exynos_hdmi_edid, edid, size);

	mhl_max_res = check_mhl_max_resolution(mhl_ver, mhl_link_mode, tmds_speed);

	pr_info("%s, mhl_ver 0x%02x link_mode= %d tmds_speed= %d max_resolution	%d\n",
			__func__,	mhl_ver, mhl_link_mode,	tmds_speed, mhl_max_res);
	return 0;
}
EXPORT_SYMBOL(exynos_get_edid);
#endif /* CONFIG_SAMSUNG_MHL_8240 */

#else /* CONFIG_SEC_MHL_EDID */
static int edid_i2c_read(struct hdmi_device *hdev, u8 segment, u8 offset,
						   u8 *buf, size_t len)
{
	struct device *dev = hdev->dev;
	struct i2c_client *i2c = edid_client;
	int cnt = 0;
	int ret;
	struct i2c_msg msg[] = {
		{
			.addr = EDID_SEGMENT_ADDR,
			.flags = segment ? 0 : I2C_M_IGNORE_NAK,
			.len = 1,
			.buf = &segment
		},
		{
			.addr = EDID_ADDR,
			.flags = 0,
			.len = 1,
			.buf = &offset
		},
		{
			.addr = EDID_ADDR,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf
		}
	};

	if (!i2c)
		return -ENODEV;

	do {
		/*
		 * If the HS-I2C is used for DDC(EDID),
		 * shouldn't write the segment pointer.
		 */
		if (hdev->ip_ver == IP_VER_TV_7I) {
			ret = i2c_transfer(i2c->adapter, &msg[1],
					EDID_SEGMENT_IGNORE);
			if (ret == EDID_SEGMENT_IGNORE)
				break;
		} else {
			ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
			if (ret == ARRAY_SIZE(msg))
				break;
		}

		dev_dbg(dev, "%s: can't read data, retry %d\n", __func__, cnt);
		msleep(25);
		cnt++;
	} while (cnt < 5);

	if (cnt == 5) {
		dev_err(dev, "%s: can't read data, timeout\n", __func__);
		return -ETIME;
	}

	return 0;
}

static int
edid_read_block(struct hdmi_device *hdev, int block, u8 *buf, size_t len)
{
	struct device *dev = hdev->dev;
	int ret, i;
	u8 segment = EDID_SEGMENT(block);
	u8 offset = EDID_OFFSET(block);
	u8 sum = 0;

	if (len < EDID_BLOCK_SIZE)
		return -EINVAL;

	ret = edid_i2c_read(hdev, segment, offset, buf, EDID_BLOCK_SIZE);
	if (ret)
		return ret;

	for (i = 0; i < EDID_BLOCK_SIZE; i++)
		sum += buf[i];

	if (sum) {
		dev_err(dev, "%s: checksum error block=%d sum=%d\n", __func__,
								  block, sum);
		return -EPROTO;
	}

	return 0;
}
#endif /* CONFIG_SEC_MHL_EDID */

static int edid_read(struct hdmi_device *hdev, u8 **data)
{
#ifdef CONFIG_SEC_MHL_EDID
	*data = exynos_hdmi_edid;
	return 1 + (s8)exynos_hdmi_edid[0x7e];
#else
	u8 block0[EDID_BLOCK_SIZE];
	u8 *edid;
	int block = 0;
	int block_cnt, ret;

	ret = edid_read_block(hdev, 0, block0, sizeof(block0));
	if (ret)
		return ret;

	block_cnt = block0[EDID_EXTENSION_FLAG] + 1;

	edid = kmalloc(block_cnt * EDID_BLOCK_SIZE, GFP_KERNEL);
	if (!edid)
		return -ENOMEM;

	memcpy(edid, block0, sizeof(block0));

	while (++block < block_cnt) {
		ret = edid_read_block(hdev, block,
				edid + block * EDID_BLOCK_SIZE,
					       EDID_BLOCK_SIZE);
		if ((edid[EDID_NATIVE_FORMAT] & EDID_BASIC_AUDIO) >> 6)
			edid_misc = FB_MISC_HDMI;
		if (ret) {
			kfree(edid);
			return ret;
		}
	}

	*data = edid;
	return block_cnt;
#endif /* CONFIG_SEC_MHL_EDID */
}

static unsigned int get_ud_timing(struct fb_vendor *vsdb, unsigned int vic_idx)
{
	unsigned char val = 0;
	unsigned int idx = 0;

	val = vsdb->vic_data[vic_idx];
	switch (val) {
	case 0x01:
		idx = 0;
		break;
	case 0x02:
		idx = 1;
		break;
	case 0x03:
		idx = 2;
		break;
	case 0x04:
		idx = 3;
		break;
	}

	return idx;
}

static struct edid_preset *edid_find_preset(const struct fb_videomode *mode)
{
	struct edid_preset *preset = edid_presets;
	int i;

	for (i = 0; i < ARRAY_SIZE(edid_presets); i++, preset++) {
#ifdef CONFIG_SEC_MHL_EDID
		if (mhl_max_res == MHL_576P_50 && !(strncmp(preset->name, "720p@50", 10)))
			return NULL;
		else if (mhl_max_res == MHL_1080P_30  && !(strncmp(preset->name, "1080p@50", 10)))
			return NULL;
		else if (mhl_max_res == MHL_1080P_60 && !(strncmp(preset->name,	"2160p@24", 10)))
			return NULL;
#endif
		if (mode->refresh == preset->refresh &&
			mode->xres	== preset->xres &&
			mode->yres	== preset->yres &&
			mode->vmode	== preset->vmode) {
			return preset;
		}
	}

	return NULL;
}

static struct edid_3d_preset *edid_find_3d_mandatory_preset(const struct
				edid_3d_mandatory_preset *mandatory)
{
	struct edid_3d_preset *s3d_preset = edid_3d_presets;
	int i;

	for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
		if (mandatory->refresh == s3d_preset->refresh &&
			mandatory->xres	== s3d_preset->xres &&
			mandatory->yres	== s3d_preset->yres &&
			mandatory->s3d	== s3d_preset->s3d) {
			return s3d_preset;
		}
	}

	return NULL;
}

static void edid_find_3d_preset(struct fb_video *vic, struct fb_vendor *vsdb)
{
	struct edid_3d_preset *s3d_preset = edid_3d_presets;
	int i;

	if ((vsdb->s3d_structure_all & EDID_3D_FP_MASK) >> EDID_3D_FP) {
		s3d_preset = edid_3d_presets;
		for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
			if (vic->refresh == s3d_preset->refresh &&
				vic->xres	== s3d_preset->xres &&
				vic->yres	== s3d_preset->yres &&
				vic->vmode	== s3d_preset->vmode &&
				EDID_3D_FP	== s3d_preset->s3d) {
				if (s3d_preset->supported == false) {
					s3d_preset->supported = true;
					pr_info("EDID: found %s",
							s3d_preset->name);
				}
			}
		}
	}
	if ((vsdb->s3d_structure_all & EDID_3D_TB_MASK) >> EDID_3D_TB) {
		s3d_preset = edid_3d_presets;
		for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
			if (vic->refresh == s3d_preset->refresh &&
				vic->xres	== s3d_preset->xres &&
				vic->yres	== s3d_preset->yres &&
				EDID_3D_TB	== s3d_preset->s3d) {
				if (s3d_preset->supported == false) {
					s3d_preset->supported = true;
					pr_info("EDID: found %s",
							s3d_preset->name);
				}
			}
		}
	}
	if ((vsdb->s3d_structure_all & EDID_3D_SBS_MASK) >> EDID_3D_SBS) {
		s3d_preset = edid_3d_presets;
		for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
			if (vic->refresh == s3d_preset->refresh &&
				vic->xres	== s3d_preset->xres &&
				vic->yres	== s3d_preset->yres &&
				EDID_3D_SBS	== s3d_preset->s3d) {
				if (s3d_preset->supported == false) {
					s3d_preset->supported = true;
					pr_info("EDID: found %s",
							s3d_preset->name);
				}
			}
		}
	}
}

static void edid_find_3d_more_preset(struct fb_video *vic, char s3d_structure)
{
	struct edid_3d_preset *s3d_preset = edid_3d_presets;
	int i;

	for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
		if (vic->refresh == s3d_preset->refresh &&
			vic->xres	== s3d_preset->xres &&
			vic->yres	== s3d_preset->yres &&
			vic->vmode	== s3d_preset->vmode &&
			s3d_structure	== s3d_preset->s3d) {
			if (s3d_preset->supported == false) {
				s3d_preset->supported = true;
				pr_info("EDID: found %s", s3d_preset->name);
			}
		}
	}
}

static void edid_use_default_preset(void)
{
	int i;

	preferred_preset = hdmi_conf[HDMI_DEFAULT_TIMINGS_IDX].dv_timings;
	for (i = 0; i < ARRAY_SIZE(edid_presets); i++)
		edid_presets[i].supported =
			v4l_match_dv_timings(&edid_presets[i].dv_timings,
					&preferred_preset, 0);
	/* Default EDID setting is DVI */
	/* max_audio_channels = 2; */
}

void edid_extension_update(struct fb_monspecs *specs)
{
	struct edid_3d_preset *s3d_preset;
	struct edid_preset *ud_preset;
	const struct edid_3d_mandatory_preset *s3d_mandatory
					= edid_3d_mandatory_presets;
	unsigned int udmode_idx, vic_idx;
	bool first = true;
	int i;

	if (!specs->vsdb)
		return;

	/* number of 128bytes blocks to follow */
	source_phy_addr = specs->vsdb->phy_addr;

	/* find 3D mandatory preset */
	if (specs->vsdb->s3d_present) {
		for (i = 0; i < ARRAY_SIZE(edid_3d_mandatory_presets);
				i++, s3d_mandatory++) {
			s3d_preset = edid_find_3d_mandatory_preset(s3d_mandatory);
			if (s3d_preset) {
				pr_info("EDID: found %s", s3d_preset->name);
				s3d_preset->supported = true;
			}
		}
	}

	if (!specs->videodb)
		return;

	/* find 3D multi preset */
	if (specs->vsdb->s3d_multi_present == EDID_3D_STRUCTURE_ALL)
		for (i = 0; i < specs->videodb_len + 1; i++)
			edid_find_3d_preset(&specs->videodb[i], specs->vsdb);
	else if (specs->vsdb->s3d_multi_present == EDID_3D_STRUCTURE_MASK)
		for (i = 0; i < specs->videodb_len + 1; i++)
			if ((specs->vsdb->s3d_structure_mask & (1 << i)) >> i)
				edid_find_3d_preset(&specs->videodb[i],
						specs->vsdb);

	/* find 3D more preset */
	if (specs->vsdb->s3d_field) {
		for (i = 0; i < specs->videodb_len + 1; i++) {
			edid_find_3d_more_preset(&specs->videodb
					[specs->vsdb->vic_order[i]],
					specs->vsdb->s3d_structure[i]);
			if (specs->vsdb->s3d_structure[i] > EDID_3D_TB + 1)
				i++;
		}
	}

	/* find UHD preset */
#ifdef CONFIG_SEC_MHL_EDID
	if (mhl_max_res >= MHL_UHD) {
#endif
		if (specs->vsdb->vic_len) {
			for (vic_idx = 0; vic_idx < specs->vsdb->vic_len; vic_idx++) {
				udmode_idx = get_ud_timing(specs->vsdb, vic_idx);
				ud_preset =  edid_find_preset(&ud_modes[udmode_idx]);
				if (ud_preset) {
					pr_info("EDID: found %s", ud_preset->name);
					ud_preset->supported = true;
					if (first) {
						preferred_preset = ud_preset->dv_timings;
						pr_info("EDID: set %s", ud_preset->name);
						first = false;
					}
				}
			}
		}
#ifdef CONFIG_SEC_MHL_EDID
	}
#endif
}

int edid_update(struct hdmi_device *hdev)
{
	struct fb_monspecs specs;
	struct edid_preset *preset;
#ifdef CONFIG_SEC_MHL_EDID
	u16 pre_xres = 0;
	u16 pre_refresh = 0;
#endif
	bool first = true;
	u8 *edid = NULL;
	int channels_max = 0, support_bit_rates = 0, support_sample_rates = 0;
#ifdef CONFIG_SEC_MHL_EDID
	int basic_audio = 0;
#endif
	int block_cnt = 0;
	int ret = 0;
	int i;

	edid_misc = 0;
	max_audio_channels = 0;
	audio_sample_rates = 0;
	audio_bit_rates = 0;

	block_cnt = edid_read(hdev, &edid);
	if (block_cnt < 0)
		goto out;

	ret = fb_edid_to_monspecs(edid, &specs);
	if (ret < 0)
		goto out;
	for (i = 1; i < block_cnt; i++)
		ret = fb_edid_add_monspecs(edid + i * EDID_BLOCK_SIZE, &specs);
#ifdef CONFIG_SEC_MHL_EDID
	basic_audio = edid[EDID_NATIVE_FORMAT] & EDID_BASIC_AUDIO;
#endif

	preferred_preset = hdmi_conf[HDMI_DEFAULT_TIMINGS_IDX].dv_timings;
	for (i = 0; i < ARRAY_SIZE(edid_presets); i++)
		edid_presets[i].supported = false;
	for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++)
		edid_3d_presets[i].supported = false;

	/* find 2D preset */
	for (i = 0; i < specs.modedb_len; i++) {
		preset = edid_find_preset(&specs.modedb[i]);
		if (preset) {
			if (preset->supported == false) {
				pr_info("EDID: found %s\n", preset->name);
				preset->supported = true;
			}

#ifdef CONFIG_SEC_MHL_EDID
			if (preset->supported) {
				first = false;
				if (((preset->xres) > pre_xres) || (((preset->xres) == pre_xres)
							&& ((preset->refresh) >= pre_refresh))) {
					pre_xres = preset->xres;
					pre_refresh = preset->refresh;

					preferred_preset = preset->dv_timings;
					pr_info("EDID: set %s", preset->name);
				}
			}
#else
			if (first) {
				preferred_preset = preset->dv_timings;
				pr_info("EDID: set %s", preset->name);
				first = false;
			}
#endif
		}
	}

	/* number of 128bytes blocks to follow */
	if (ret > 0 && block_cnt > 1)
		edid_extension_update(&specs);
	else
		goto out;

	if (!edid_misc)
		edid_misc = specs.misc;
	pr_info("EDID: misc flags %08x", edid_misc);

#ifndef CONFIG_SEC_MHL_EDID
	if (!specs.audiodb)
		goto out;
#endif

	for (i = 0; i < specs.audiodb_len; i++) {
		if (specs.audiodb[i].format != FB_AUDIO_LPCM)
			continue;
		if (specs.audiodb[i].channel_count > channels_max) {
			channels_max = specs.audiodb[i].channel_count;
			support_sample_rates = specs.audiodb[i].sample_rates;
			support_bit_rates = specs.audiodb[i].bit_rates;
		}
	}

	if (edid_misc & FB_MISC_HDMI) {
		if (channels_max) {
			max_audio_channels = channels_max;
			audio_sample_rates = support_sample_rates;
			audio_bit_rates = support_bit_rates;
		} else {
#ifdef CONFIG_SEC_MHL_EDID
			if (basic_audio) {
				max_audio_channels = 2;
				audio_sample_rates = FB_AUDIO_48KHZ; /*default audio info*/
				audio_bit_rates = FB_AUDIO_16BIT;
			} else {
				max_audio_channels = 0;
				audio_sample_rates = 0;
				audio_bit_rates = 0;
			}
#else
			max_audio_channels = 2;
			audio_sample_rates = FB_AUDIO_44KHZ; /*default audio info*/
			audio_bit_rates = FB_AUDIO_16BIT;
#endif
		}
	} else {
		max_audio_channels = 0;
		audio_sample_rates = 0;
		audio_bit_rates = 0;
	}
	pr_info("EDID: Audio channels %d", max_audio_channels);

out:
	/* No supported preset found, use default */
	if (first)
		edid_use_default_preset();

	if (block_cnt == -EPROTO)
		edid_misc = FB_MISC_HDMI;

#ifndef CONFIG_SEC_MHL_EDID
	kfree(edid);
#endif
	return block_cnt;
}

struct v4l2_dv_timings edid_preferred_preset(struct hdmi_device *hdev)
{
#ifdef CONFIG_SEC_MHL_EDID
	if(hdmi_forced_resolution >= 0 &&
			hdmi_forced_resolution < hdmi_pre_cnt)
		return
			edid_presets[hdmi_forced_resolution].dv_timings;
	else
#endif
	return preferred_preset;
}

bool edid_supports_hdmi(struct hdmi_device *hdev)
{
	return edid_misc & FB_MISC_HDMI;
}

u32 edid_audio_informs(struct hdmi_device *hdev)
{
	u32 value = 0, ch_info = 0;

	if (max_audio_channels > 0)
		ch_info |= (1 << (max_audio_channels - 1));
	if (max_audio_channels > 6)
		ch_info |= (1 << 5);
	value = ((audio_sample_rates << 19) | (audio_bit_rates << 16) |
			ch_info);
	return value;
}

int edid_source_phy_addr(struct hdmi_device *hdev)
{
	return source_phy_addr;
}

static int edid_probe(struct i2c_client *client,
				const struct i2c_device_id *dev_id)
{
	edid_client = client;
	edid_use_default_preset();
	dev_info(&client->adapter->dev, "probed exynos edid\n");
	return 0;
}

static int edid_remove(struct i2c_client *client)
{
	edid_client = NULL;
	dev_info(&client->adapter->dev, "removed exynos edid\n");
	return 0;
}

static struct i2c_device_id edid_idtable[] = {
	{"exynos_edid", 2},
};
MODULE_DEVICE_TABLE(i2c, edid_idtable);

static struct i2c_driver edid_driver = {
	.driver = {
		.name = "exynos_edid",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(edid_device_table),
	},
	.id_table	= edid_idtable,
	.probe		= edid_probe,
	.remove         = edid_remove,
};

static int __init edid_init(void)
{
	return i2c_add_driver(&edid_driver);
}

static void __exit edid_exit(void)
{
	i2c_del_driver(&edid_driver);
}
module_init(edid_init);
module_exit(edid_exit);
