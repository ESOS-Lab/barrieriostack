/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS TVOUT driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "decon_tv.h"
#include "regs-decon_tv.h"
#include "../decon_display/decon_fb.h"
#include "../../../staging/android/sw_sync.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/smc.h>

#include <media/exynos_mc.h>

#include <linux/dma-buf.h>
#include <linux/exynos_ion.h>
#include <linux/ion.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/exynos_iovmm.h>

#ifdef CONFIG_OF
static const struct of_device_id decon_tv_device_table[] = {
	        { .compatible = "samsung,exynos5-decon_tv_driver" },
		{},
};
MODULE_DEVICE_TABLE(of, decon_tv_device_table);
#endif

#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
struct pm_qos_request exynos5_decon_tv_int_qos;
static int prev_overlap_bw = 0;
#endif

int dex_log_level = 6;
module_param(dex_log_level, int, 0644);

static int dex_runtime_suspend(struct device *dev);
static int dex_runtime_resume(struct device *dev);

static struct decon_tv_porch decon_tv_porchs[] =
{
	{"DECONTV_720x480P60", 720, 480, 43, 1, 1, 92, 10, 36, V4L2_FIELD_NONE, 60},
	{"DECONTV_720x576P50", 720, 576, 47, 1, 1, 92, 10, 42, V4L2_FIELD_NONE, 50},
	{"DECONTV_1280x720P60", 1280, 720, 28, 1, 1, 192, 10, 168, V4L2_FIELD_NONE, 60},
	{"DECONTV_1280x720P50", 1280, 720, 28, 1, 1, 92, 10, 598, V4L2_FIELD_NONE, 50},
	{"DECONTV_1920x1080P60", 1920, 1080, 43, 1, 1, 92, 10, 178, V4L2_FIELD_NONE, 60},
	{"DECONTV_1920x1080P50", 1920, 1080, 43, 1, 1, 92, 10, 618, V4L2_FIELD_NONE, 50},
	{"DECONTV_1920x1080P30", 1920, 1080, 43, 1, 1, 92, 10, 178, V4L2_FIELD_NONE, 30},
	{"DECONTV_1920x1080P25", 1920, 1080, 43, 1, 1, 92, 10, 618, V4L2_FIELD_NONE, 25},
	{"DECONTV_1920x1080P24", 1920, 1080, 43, 1, 1, 92, 10, 728, V4L2_FIELD_NONE, 24},
	{"DECONTV_1920x1080I60", 1920, 540, 20, 1, 1, 92, 10, 178, V4L2_FIELD_INTERLACED, 60},
	{"DECONTV_3840x2160P30", 3840, 2160, 88, 1, 1, 458, 10, 92, V4L2_FIELD_NONE, 30},
	{"DECONTV_3840x2160P25", 3840, 2160, 88, 1, 1, 1338, 10, 92, V4L2_FIELD_NONE, 25},
	{"DECONTV_3840x2160P24", 3840, 2160, 88, 1, 1, 1558, 10, 92, V4L2_FIELD_NONE, 24},
	{"DECONTV_4096x2160P24", 4096, 2160, 88, 1, 1, 1302, 10, 92, V4L2_FIELD_NONE, 24},
};

struct decon_tv_porch *find_porch(struct v4l2_mbus_framefmt mbus_fmt)
{
	struct decon_tv_porch *porch;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(decon_tv_porchs); ++i) {
		porch = &decon_tv_porchs[i];
		if ((mbus_fmt.field == V4L2_FIELD_INTERLACED)
			&& (mbus_fmt.field == porch->vmode))
			return porch;
		if ((mbus_fmt.width == porch->xres)
			&& (mbus_fmt.height == porch->yres)
			&& (mbus_fmt.field == porch->vmode)
			&& (mbus_fmt.reserved[0] == porch->fps))
			return porch;
	}

	return &decon_tv_porchs[0];

}

static int dex_set_output(struct dex_device *dex)
{
	struct v4l2_subdev *hdmi_sd;
	struct v4l2_mbus_framefmt mbus_fmt;
	int ret;

	mbus_fmt.width = 0;
	mbus_fmt.height = 0;
	mbus_fmt.code = 0;
	mbus_fmt.field = 0;
	mbus_fmt.colorspace = 0;

	/* find sink pad of output via enabled link*/
	hdmi_sd = dex_remote_subdev(dex->windows[DEX_DEFAULT_WIN]);
	if (hdmi_sd == NULL)
		return -EINVAL;

	ret = v4l2_subdev_call(hdmi_sd, video, g_mbus_fmt, &mbus_fmt);
	WARN(ret, "failed to get mbus_fmt for output %s\n", hdmi_sd->name);

	dex->porch = find_porch(mbus_fmt);
	dex_dbg("find porch for %s\n", dex->porch->name);
	if (!dex->porch)
		return -EINVAL;

	dex_reg_porch(dex);

	return 0;
}

static int dex_enable(struct dex_device *dex)
{
	struct v4l2_subdev *hdmi_sd = NULL;
	int ret = 0;

	dex_dbg("enable decon_tv\n");
	mutex_lock(&dex->s_mutex);

	/*
	 * There is the limitation in start sequence
	 * between decon-tv, HDMI and HDMIPHY.
	 * It has nothing to do if a vsync is delivered to
	 * the both DECON-tv and HDMI side from HDMIPHY.
	 * However, if a vsync is delivered to in order
	 * HDMIPHY -> DECON-TV -> HDMI such as IP_VER_TV_5HP,
	 * start sequence must be followed.
	 */
	if (!dex->wb_path) {
		/* find sink pad of output via enabled link*/
		hdmi_sd = dex_remote_subdev(dex->windows[DEX_DEFAULT_WIN]);
		if (hdmi_sd == NULL) {
			ret = -EINVAL;
			goto out;
		}
		dex_dbg("find remote pad %s\n", hdmi_sd->name);

		/* start hdmiphy */
		ret = v4l2_subdev_call(hdmi_sd, core, s_power, 1);
		if (ret) {
			dex_err("failed to get power for HDMI\n");
			goto out;
		}
	}

	/* start decon-tv */
	dex_reg_streamon(dex);

	if (!dex->wb_path) {
		/* get timing information for decon-tv porch */
		ret = dex_set_output(dex);
		if (ret) {
			dex_err("failed get timings information\n");
			goto out;
		}

		/* standalone sholud be set before tg_enable */
		if (dex->ip_ver == IP_VER_TV_5HP)
			dex_tv_update(dex);

		/* start hdmi */
		ret = v4l2_subdev_call(hdmi_sd, video, s_stream, 1);
		if (ret) {
			dex_err("starting stream failed for HDMI\n");
			goto out;
		}

		/* standalone sholud be set after tg_enable */
		if (dex->ip_ver == IP_VER_TV_5H)
			dex_tv_update(dex);
	}

	dex->n_streamer++;
	mutex_unlock(&dex->s_mutex);
	dex_info("enabled decon_tv successfully\n");
	return 0;

out:
	mutex_unlock(&dex->s_mutex);
	return ret;
}

static int dex_disable(struct dex_device *dex)
{
	struct v4l2_subdev *hdmi_sd = NULL;
	int val;
	int ret = 0, timecnt = 2000;

	dex_dbg("disable decon_tv\n");
	mutex_lock(&dex->s_mutex);

	if (!dex->wb_path) {
		/* find sink pad of output via enabled link*/
		hdmi_sd = dex_remote_subdev(dex->windows[DEX_DEFAULT_WIN]);
		if (hdmi_sd == NULL) {
			ret = -EINVAL;
			goto out;
		}
		dex_dbg("find remote pad %s\n", hdmi_sd->name);

		flush_kthread_worker(&dex->update_worker);
	}

	dex_reg_streamoff(dex);
	do {
		val = dex_get_status(dex);
		timecnt--;
	}
	while (val && timecnt);

	if (timecnt == 0)
		dex_err("Failed to disable DECON-TV");
	else
		dex_dbg("DECON-TV has stopped");

	if (dex->ip_ver == IP_VER_TV_5H)
		dex_reg_sw_reset(dex);

	if (!dex->wb_path) {
		/* stop hdmi */
		ret = v4l2_subdev_call(hdmi_sd, video, s_stream, 0);
		if (ret) {
			dex_err("stopping stream failed for HDMI\n");
			goto out;
		}
		ret = v4l2_subdev_call(hdmi_sd, core, s_power, 0);
		if (ret) {
			dex_err("failed to put power for HDMI\n");
			goto out;
		}
	}

	dex->n_streamer--;
	mutex_unlock(&dex->s_mutex);
	dex_info("diabled decon_tv successfully\n");

	return 0;

out:
	mutex_unlock(&dex->s_mutex);
	return ret;
}

static int dex_blank(int blank_mode, struct fb_info *info)
{
	struct dex_win *win = info->par;
	struct dex_device *dex = win->dex;
	int ret = 0;

	dex_dbg("blank mode %d\n", blank_mode);

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
		if (!dex->n_streamer) {
			dex_warn("decon_tv & HDMI already disabled\n");
			break;
		}
		ret = dex_disable(dex);
		if (ret < 0)
			dex_err("fail to disable decon_tv");
#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
		pm_qos_remove_request(&exynos5_decon_tv_int_qos);
		exynos5_update_media_layers(TYPE_TV, 0);
		prev_overlap_bw = 0;
#endif
#ifdef CONFIG_PM_RUNTIME
		ret = pm_runtime_put_sync(dex->dev);
		if (ret < 0)
			dex_err("fail to pm_runtime_put_sync()");
#else
		dex_runtime_suspend(dex->dev);
#endif
		break;
	case FB_BLANK_UNBLANK:
		if (dex->n_streamer) {
			dex_warn("decon_tv & HDMI already enabled\n");
			break;
		}
#ifdef CONFIG_PM_RUNTIME
		ret = pm_runtime_get_sync(dex->dev);
		if (ret < 0)
			dex_err("fail to pm_runtime_get_sync()");
#else
		dex_runtime_resume(dex->dev);
#endif
		ret = dex_enable(dex);
		if (ret < 0)
			dex_err("fail to enable decon_tv");
#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
		pm_qos_add_request(&exynos5_decon_tv_int_qos, PM_QOS_DEVICE_THROUGHPUT, 400000);
#endif
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int dex_get_hdmi_config(struct dex_device *dex,
               struct exynos_hdmi_data *hdmi_data)
{
	struct v4l2_subdev *hdmi_sd;
	struct v4l2_control ctrl;
	int ret = 0;
	dex_dbg("state : %d\n", hdmi_data->state);

	ctrl.id = 0;
	ctrl.value = 0;

	/* find sink pad of output via enabled link*/
	hdmi_sd = dex_remote_subdev(dex->windows[DEX_DEFAULT_WIN]);
	if (hdmi_sd == NULL)
		return -EINVAL;

	mutex_lock(&dex->mutex);

	switch (hdmi_data->state) {
	case EXYNOS_HDMI_STATE_PRESET:
		ret = v4l2_subdev_call(hdmi_sd, video, g_dv_timings, &hdmi_data->timings);
		if (ret)
			dex_err("failed to get current timings\n");
		dex_dbg("%dx%d@%s %lldHz %s(%#x)\n", hdmi_data->timings.bt.width,
			hdmi_data->timings.bt.height,
			hdmi_data->timings.bt.interlaced ? "I" : "P",
			hdmi_data->timings.bt.pixelclock,
			hdmi_data->timings.type ? "S3D" : "2D",
			hdmi_data->timings.type);
		break;
	case EXYNOS_HDMI_STATE_ENUM_PRESET:
		ret = v4l2_subdev_call(hdmi_sd, video, enum_dv_timings, &hdmi_data->etimings);
		if (ret)
			dex_err("failed to enumerate timings\n");
		break;
	case EXYNOS_HDMI_STATE_CEC_ADDR:
		ctrl.id = V4L2_CID_TV_SOURCE_PHY_ADDR;
		ret = v4l2_subdev_call(hdmi_sd, core, g_ctrl, &ctrl);
		if (ret)
			dex_err("failed to get physical address for CEC\n");
		hdmi_data->cec_addr = ctrl.value;
		dex_dbg("get physical address for CEC: %#x\n",
					hdmi_data->cec_addr);
		break;
	case EXYNOS_HDMI_STATE_AUDIO:
		ctrl.id = V4L2_CID_TV_MAX_AUDIO_CHANNELS;
		ret = v4l2_subdev_call(hdmi_sd, core, g_ctrl, &ctrl);
		if (ret)
			dex_err("failed to get hdmi audio information\n");
		hdmi_data->audio_info = ctrl.value;
		break;
	default:
		dex_warn("unrecongnized state %u", hdmi_data->state);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dex->mutex);
	return ret;
}

static int dex_set_hdmi_config(struct dex_device *dex,
               struct exynos_hdmi_data *hdmi_data)
{
	struct v4l2_subdev *hdmi_sd;
	struct v4l2_control ctrl;
	int ret = 0;
	dex_dbg("state : %d\n", hdmi_data->state);

	/* find sink pad of output via enabled link*/
	hdmi_sd = dex_remote_subdev(dex->windows[DEX_DEFAULT_WIN]);
	if (hdmi_sd == NULL)
		return -EINVAL;

	mutex_lock(&dex->mutex);

	switch (hdmi_data->state) {
	case EXYNOS_HDMI_STATE_PRESET:
		ret = v4l2_subdev_call(hdmi_sd, video, s_dv_timings, &hdmi_data->timings);
		if (ret)
			dex_err("failed to set timings newly\n");
		dex_dbg("%dx%d@%s %lldHz %s(%#x)\n", hdmi_data->timings.bt.width,
			hdmi_data->timings.bt.height,
			hdmi_data->timings.bt.interlaced ? "I" : "P",
			hdmi_data->timings.bt.pixelclock,
			hdmi_data->timings.type ? "S3D" : "2D",
			hdmi_data->timings.type);
		break;
	case EXYNOS_HDMI_STATE_HDCP:
		ctrl.id = V4L2_CID_TV_HDCP_ENABLE;
		ctrl.value = hdmi_data->hdcp;
		ret = v4l2_subdev_call(hdmi_sd, core, s_ctrl, &ctrl);
		if (ret)
			dex_err("failed to enable HDCP\n");
		dex_info("HDCP %s\n", ctrl.value ? "enabled" : "disabled");
		break;
	case EXYNOS_HDMI_STATE_AUDIO:
		ctrl.id = V4L2_CID_TV_SET_NUM_CHANNELS;
		ctrl.value = hdmi_data->audio_info;
		ret = v4l2_subdev_call(hdmi_sd, core, s_ctrl, &ctrl);
		if (ret)
			dex_err("failed to set hdmi audio information\n");
		break;
	default:
		dex_warn("unrecongnized state %u", hdmi_data->state);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dex->mutex);
	return ret;
}

static u32 dex_red_length(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 5;

	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 dex_red_offset(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 0;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 11;

	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 16;

	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 dex_green_length(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 6;

	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 dex_green_offset(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 5;
	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 dex_blue_length(int format)
{
	return dex_red_length(format);
}

static u32 dex_blue_offset(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
		return 16;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 10;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 0;

	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 dex_transp_length(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 1;

	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_RGB_565:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 0;

	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 dex_transp_offset(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
		return 24;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 15;

	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
		return dex_blue_offset(format);

	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return dex_red_offset(format);

	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 0;

	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 dex_padding(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
	case S3C_FB_PIXEL_FORMAT_RGB_565:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
		return 0;

	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}

}

static u32 dex_rgborder(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return WINCONx_BPPORDER_C_F_RGB;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return WINCONx_BPPORDER_C_F_BGR;

	default:
		dex_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static inline u32 wincon(u32 bits_per_pixel, u32 transp_length, u32 red_length)
{
	u32 data = 0;

	switch (bits_per_pixel) {
	case 8:
		data |= WINCONx_BPPMODE_8BPP_1232;
		data |= WINCONx_BYTSWP;
		break;
	case 16:
		if (transp_length == 1)
			data |= WINCONx_BPPMODE_16BPP_A1555
				| WINCONx_BLD_PIX;
		else if (transp_length == 4)
			data |= WINCONx_BPPMODE_16BPP_A4444
				| WINCONx_BLD_PIX;
		else
			data |= WINCONx_BPPMODE_16BPP_565;
		data |= WINCONx_HAWSWP;
		break;
	case 24:
	case 32:
		if (red_length == 6) {
			if (transp_length != 0)
				data |= WINCONx_BPPMODE_19BPP_A1666;
			else
				data |= WINCONx_BPPMODE_18BPP_666;
		} else if (transp_length == 1)
			data |= WINCONx_BPPMODE_25BPP_A1888
				| WINCONx_BLD_PIX;
		else if ((transp_length == 4) ||
			(transp_length == 8))
			data |= WINCONx_BPPMODE_32BPP_A8888
				| WINCONx_BLD_PIX;
		else
			data |= WINCONx_BPPMODE_24BPP_888;

		data |= WINCONx_WSWP;
		break;
	}

	if (transp_length != 1)
		data |= WINCONx_ALPHA_SEL;

	return data;
}

static inline u32 blendeq(enum s3c_fb_blending blending, u8 transp_length,
		int plane_alpha)
{
	u8 a, b;
	int is_plane_alpha = (plane_alpha < 255 && plane_alpha > 0) ? 1 : 0;

	if (transp_length == 1 && blending == S3C_FB_BLENDING_PREMULT)
		blending = S3C_FB_BLENDING_COVERAGE;

	switch (blending) {
	case S3C_FB_BLENDING_NONE:
		a = BLENDEQ_COEF_ONE;
		b = BLENDEQ_COEF_ZERO;
		break;

	case S3C_FB_BLENDING_PREMULT:
		if (!is_plane_alpha) {
			a = BLENDEQ_COEF_ONE;
			b = BLENDEQ_COEF_ONE_MINUS_ALPHA_A;
		} else {
			a = BLENDEQ_COEF_ALPHA0;
			b = BLENDEQ_COEF_ONE_MINUS_ALPHA_A;
		}
		break;

	case S3C_FB_BLENDING_COVERAGE:
		a = BLENDEQ_COEF_ALPHA_A;
		b = BLENDEQ_COEF_ONE_MINUS_ALPHA_A;
		break;

	default:
		return 0;
	}

	return BLENDEQ_A_FUNC(a) |
			BLENDEQ_B_FUNC(b) |
			BLENDEQ_P_FUNC(BLENDEQ_COEF_ZERO) |
			BLENDEQ_Q_FUNC(BLENDEQ_COEF_ZERO);
}

static bool dex_validate_x_alignment(struct dex_device *dex, int x, u32 w,
		u32 bits_per_pixel)
{
	uint8_t pixel_alignment = 32 / bits_per_pixel;

	if (x % pixel_alignment) {
		dex_err("left X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u)\n",
				pixel_alignment, bits_per_pixel, x);
		return 0;
	}
	if ((x + w) % pixel_alignment) {
		dex_err("right X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u, w = %u)\n",
				pixel_alignment, bits_per_pixel, x, w);
		return 0;
	}

	return 1;
}

static void dex_set_protected_content(struct dex_device *dex, bool enable)
{
	int  ret;

	if (dex->protected_content == enable)
		return;

	ret = exynos_smc(SMC_PROTECTION_SET, 0, DEV_DECON_TV, enable);
	if (ret)
		WARN(1, "decon-tv protection Enable failed. ret(%d)\n", ret);
	else
		dex_dbg("DRM %s\n", enable ? "enabled" : "disabled");

	dex->protected_content = enable;
}

#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
static int dex_get_overlap_bw(struct dex_device *dex,
			struct s3c_fb_win_config *win_config)
{
	struct s3c_fb_win_config *win_cfg;
	int overlap_bw = 0;
	int i;

	for (i = 1; i < DEX_MAX_WINDOWS; i++) {
		win_cfg = &win_config[i];
		if (win_cfg->state == S3C_FB_WIN_STATE_BUFFER) {
			overlap_bw += win_cfg->stride * win_cfg->h
				* dex->porch->fps;
		}
	}

	return overlap_bw;
}
#endif

static unsigned int dex_map_ion_handle(struct dex_device *dex,
		struct dex_dma_buf_data *dma, struct ion_handle *ion_handle,
		struct dma_buf *buf, int idx)
{
	dma->fence = NULL;
	dma->dma_buf = buf;

	dma->attachment = dma_buf_attach(dma->dma_buf, dex->dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		dex_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		dex_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	dma->dma_addr = ion_iovmm_map(dma->attachment, 0,
			dma->dma_buf->size, DMA_TO_DEVICE, idx);
	if (!dma->dma_addr || IS_ERR_VALUE(dma->dma_addr)) {
		dex_err("iovmm_map() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}

	exynos_ion_sync_dmabuf_for_device(dex->dev, dma->dma_buf,
						dma->dma_buf->size,
						DMA_TO_DEVICE);
	dma->ion_handle = ion_handle;

	return dma->dma_buf->size;

err_iovmm_map:
	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_BIDIRECTIONAL);
err_buf_map_attachment:
	dma_buf_detach(dma->dma_buf, dma->attachment);

err_buf_map_attach:
	return 0;
}

static void dex_free_dma_buf(struct dex_device *dex, struct dex_dma_buf_data *dma)
{
	if (!dma->dma_addr)
		return;

	if (dma->fence)
		sync_fence_put(dma->fence);

	ion_iovmm_unmap(dma->attachment , dma->dma_addr);
	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_BIDIRECTIONAL);
	exynos_ion_sync_dmabuf_for_cpu(dex->dev, dma->dma_buf,
					dma->dma_buf->size,
					DMA_BIDIRECTIONAL);
	dma_buf_detach(dma->dma_buf, dma->attachment);
	dma_buf_put(dma->dma_buf);
	ion_free(dex->ion_client, dma->ion_handle);
	memset(dma, 0, sizeof(struct dex_dma_buf_data));
}

static void dex_fence_wait(struct sync_fence *fence)
{
	int err = sync_fence_wait(fence, 1000);
	if (err >= 0)
		return;

	if (err == -ETIME)
		err = sync_fence_wait(fence, 10 * MSEC_PER_SEC);

	if (err < 0)
		dex_warn("error waiting on fence: %d\n", err);
}

static void dex_update(struct dex_device *dex, struct dex_reg_data *regs)
{
	struct dex_dma_buf_data old_dma_bufs[DEX_MAX_WINDOWS];
	bool wait_for_vsync;
	int count = 100;
	int i, ret = 0;
	int protection = 0;

	memset(&old_dma_bufs, 0, sizeof(old_dma_bufs));

	for (i = 1; i < DEX_MAX_WINDOWS; i++) {
		if (!dex->windows[i]->local) {
			protection += regs->protection[i];
			old_dma_bufs[i] = dex->windows[i]->dma_buf_data;
			if (regs->dma_buf_data[i].fence)
				dex_fence_wait(regs->dma_buf_data[i].fence);
		}
	}
	dex_set_protected_content(dex, !!protection);

#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
	if (prev_overlap_bw < regs->win_overlap_bw) {
		exynos5_update_media_layers(TYPE_TV, regs->win_overlap_bw);
		dex_dbg("+++window overlap bandwidth %d to %d\n",
				prev_overlap_bw, regs->win_overlap_bw);
		prev_overlap_bw = regs->win_overlap_bw;
	}
#endif

	do {
		dex_update_regs(dex, regs);
		dex_reg_wait4update(dex);
		wait_for_vsync = false;

		for (i = 1; i < DEX_MAX_WINDOWS; i++) {
			if (!dex->windows[i]->local) {
				ret = dex_reg_compare(dex, i, regs->buf_start[i]);
				if (ret) {
					wait_for_vsync = true;
					break;
				}
			}
		}
	} while (wait_for_vsync && count--);
	if (wait_for_vsync)
		dex_err("failed to update buffer\n");

	sw_sync_timeline_inc(dex->timeline, 1);

	ret = dex_wait_update(dex);
	if (ret < 0) {
		dex_reg_dump(dex);
		dex_err("failed update - timeout\n");
	}

	for (i = 1; i < DEX_MAX_WINDOWS; i++)
		if (!dex->windows[i]->local)
			dex_free_dma_buf(dex, &old_dma_bufs[i]);

#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
	if (prev_overlap_bw > regs->win_overlap_bw) {
		exynos5_update_media_layers(TYPE_TV, regs->win_overlap_bw);
		dex_dbg("---window overlap bandwidth %d to %d\n",
				prev_overlap_bw, regs->win_overlap_bw);
		prev_overlap_bw = regs->win_overlap_bw;
	}
#endif
}

static void dex_update_handler(struct kthread_work *work)
{
	struct dex_device *dex =
			container_of(work, struct dex_device, update_work);
	struct dex_reg_data *data, *next;
	struct list_head saved_list;

	mutex_lock(&dex->update_list_lock);
	saved_list = dex->update_list;
	list_replace_init(&dex->update_list, &saved_list);
	mutex_unlock(&dex->update_list_lock);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		dex_update(dex, data);
		list_del(&data->list);
		kfree(data);
	}
}

static void dex_set_wb_frame(struct dex_device *dex, struct dex_reg_data *regs)
{
	struct dex_dma_buf_data old_dma_bufs[DEX_MAX_WINDOWS];
	int i, ret;

	memset(&old_dma_bufs, 0, sizeof(old_dma_bufs));

	for (i = 1; i < DEX_MAX_WINDOWS; i++) {
		if (!dex->windows[i]->local) {
			old_dma_bufs[i] = dex->windows[i]->dma_buf_data;
			if (regs->dma_buf_data[i].fence)
				dex_fence_wait(regs->dma_buf_data[i].fence);
		}
	}

#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
	if (prev_overlap_bw < regs->win_overlap_bw) {
		exynos5_update_media_layers(TYPE_TV, regs->win_overlap_bw);
		dex_dbg("+++window overlap bandwidth %d to %d\n",
				prev_overlap_bw, regs->win_overlap_bw);
		prev_overlap_bw = regs->win_overlap_bw;
	}
#endif

	dex_reg_set_lineval(dex);
	dex_update_regs(dex, regs);

	dex_reg_wb_swtrigger(dex);
	sw_sync_timeline_inc(dex->wb_timeline, 1);
	ret = dex_wait_update(dex);
	if (ret < 0) {
		dex_reg_dump(dex);
		dex_err("failed update - timeout\n");
	}

	for (i = 1; i < DEX_MAX_WINDOWS; i++)
		if (!dex->windows[i]->local)
			dex_free_dma_buf(dex, &old_dma_bufs[i]);

#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
	if (prev_overlap_bw > regs->win_overlap_bw) {
		exynos5_update_media_layers(TYPE_TV, regs->win_overlap_bw);
		dex_dbg("---window overlap bandwidth %d to %d\n",
				prev_overlap_bw, regs->win_overlap_bw);
		prev_overlap_bw = regs->win_overlap_bw;
	}
#endif
}

static int dex_set_win_buffer(struct dex_device *dex, struct dex_win *win,
		struct s3c_fb_win_config *win_config, struct dex_reg_data *regs)
{
	struct ion_handle *handle;
	struct dma_buf *buf;
	struct dex_dma_buf_data dma_buf_data;
	size_t buf_size, window_size;
	int ret = 0;
	int idx = win->idx;
	u32 pagewidth;
	u8 alpha0, alpha1;

	if (win_config->format >= S3C_FB_PIXEL_FORMAT_MAX) {
		dex_err("unknown pixel format %u\n", win_config->format);
		return -EINVAL;
	}

	if (win_config->blending >= S3C_FB_BLENDING_MAX) {
		dex_err("unknown blending %u\n", win_config->blending);
		return -EINVAL;
	}

	if (win_config->w == 0 || win_config->h == 0 ||
		win_config->x < 0 || win_config->y < 0) {
		dex_err("win[%d] size is abnormal (w:%d, h:%d, x:%d, y:%d)\n",
				idx, win_config->w, win_config->h,
				win_config->x, win_config->y);
		return -EINVAL;
	}

	/* component calculation according to the format */
	win->fbinfo->var.red.length = dex_red_length(win_config->format);
	win->fbinfo->var.red.offset = dex_red_offset(win_config->format);
	win->fbinfo->var.green.length = dex_green_length(win_config->format);
	win->fbinfo->var.green.offset = dex_green_offset(win_config->format);
	win->fbinfo->var.blue.length = dex_blue_length(win_config->format);
	win->fbinfo->var.blue.offset = dex_blue_offset(win_config->format);
	win->fbinfo->var.transp.length = dex_transp_length(win_config->format);
	win->fbinfo->var.transp.offset = dex_transp_offset(win_config->format);
	win->fbinfo->var.bits_per_pixel = win->fbinfo->var.red.length +
					win->fbinfo->var.green.length +
					win->fbinfo->var.blue.length +
					win->fbinfo->var.transp.length +
					dex_padding(win_config->format);

	if (win_config->w * win->fbinfo->var.bits_per_pixel / 8 < 128) {
		dex_err("window must be at least 128 bytes wide (width = %u, bpp = %u)\n",
				win_config->w, win->fbinfo->var.bits_per_pixel);
		ret = -EINVAL;
		goto fail;
	}

	if (win_config->stride <
			win_config->w * win->fbinfo->var.bits_per_pixel / 8) {
		dex_err("stride shorter than buffer width (stride = %u, width = %u, bpp = %u)\n",
				win_config->stride, win_config->w,
				win->fbinfo->var.bits_per_pixel);
		ret = -EINVAL;
		goto fail;
	}

	if (!dex_validate_x_alignment(dex, win_config->x, win_config->w,
			win->fbinfo->var.bits_per_pixel)) {
		ret = -EINVAL;
		goto fail;
	}

	handle = ion_import_dma_buf(dex->ion_client, win_config->fd);
	if (IS_ERR(handle)) {
		dex_err("failed to import fd\n");
		ret = PTR_ERR(handle);
		goto fail;
	}

	buf = dma_buf_get(win_config->fd);
	if (IS_ERR_OR_NULL(buf)) {
		dex_err("dma_buf_get() failed: %ld\n", PTR_ERR(buf));
		ret = PTR_ERR(buf);
		goto fail_buf;
	}

	buf_size = dex_map_ion_handle(dex, &dma_buf_data, handle, buf, idx);
	if (!buf_size) {
		dex_err("failed to mapping buffer\n");
		ret = -ENOMEM;
		goto fail_map;
	}

	handle = NULL;
	buf = NULL;

	if (win_config->fence_fd >= 0) {
		dma_buf_data.fence = sync_fence_fdget(win_config->fence_fd);
		if (!dma_buf_data.fence) {
			dex_err("failed to import fence fd\n");
			ret = -EINVAL;
			goto fail_sync;
		}
	}

	window_size = win_config->stride * win_config->h;
	if (win_config->offset + window_size -
	   (win_config->offset % win_config->stride) > buf_size) {
		dex_err("window goes past end of buffer\n");
		dex_err("width = %u, height = %u, stride = %u, offset = %u, buf_size = %zu\n",
			win_config->w, win_config->h, win_config->stride,
			win_config->offset, buf_size);
		ret = -EINVAL;
		goto fail_sync;
	}
	if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
		alpha0 = win_config->plane_alpha;
		alpha1 = 0;
	} else if (win->fbinfo->var.transp.length == 1 &&
			win_config->blending == S3C_FB_BLENDING_NONE) {
		alpha0 = 0xff;
		alpha1 = 0xff;
	} else {
		alpha0 = 0x0;
		alpha1 = 0xff;
	}
	pagewidth = (win_config->w * win->fbinfo->var.bits_per_pixel) >> 3;

	/* save the window register information */
	regs->dma_buf_data[idx] = dma_buf_data;
	regs->wincon[idx] = wincon(win->fbinfo->var.bits_per_pixel,
				win->fbinfo->var.transp.length,
				win->fbinfo->var.red.length);
	regs->wincon[idx] |= dex_rgborder(win_config->format);
	regs->wincon[idx] |= WINCONx_BURSTLEN_8WORD;
	regs->vidosd_a[idx] = VIDOSDxA_TOPLEFT_X(win_config->x);
	regs->vidosd_a[idx] |= VIDOSDxA_TOPLEFT_Y(win_config->y);
	regs->vidosd_b[idx] = VIDOSDxB_BOTRIGHT_X(win_config->x
						+ win_config->w - 1);
	regs->vidosd_b[idx] |= VIDOSDxB_BOTRIGHT_Y(win_config->y
						+ win_config->h - 1);
	regs->vidosd_c[idx] = VIDOSDxC_ALPHA0_R_F(alpha0);
	regs->vidosd_c[idx] |= VIDOSDxC_ALPHA0_G_F(alpha0);
	regs->vidosd_c[idx] |= VIDOSDxC_ALPHA0_B_F(alpha0);
	regs->vidosd_d[idx] = VIDOSDxD_ALPHA1_R_F(alpha1);
	regs->vidosd_d[idx] |= VIDOSDxD_ALPHA1_G_F(alpha1);
	regs->vidosd_d[idx] |= VIDOSDxD_ALPHA1_B_F(alpha1);
	regs->buf_start[idx] = dma_buf_data.dma_addr + win_config->offset;
	regs->buf_end[idx] = regs->buf_start[idx] + window_size;
	regs->buf_size[idx] = VIDW_BUF_SIZE_OFFSET(win_config->stride - pagewidth) |
				VIDW_BUF_SIZE_PAGEWIDTH(pagewidth);
	regs->protection[idx] = win_config->protection;

	if (idx > 1) {
		if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
			if (win->fbinfo->var.transp.length) {
				if (win_config->blending != S3C_FB_BLENDING_NONE)
					regs->wincon[idx] |= WINCONx_ALPHA_MUL;
			} else {
				regs->wincon[idx] &= (~WINCONx_ALPHA_SEL);
				if (win_config->blending == S3C_FB_BLENDING_PREMULT)
					win_config->blending = S3C_FB_BLENDING_COVERAGE;
			}
		}
		regs->blendeq[idx - 1] = blendeq(win_config->blending,
		win->fbinfo->var.transp.length, win_config->plane_alpha);
	}
	dex_dbg("[%d] (%d,%d) %dx%d, %u, %u\n", idx, win_config->x, win_config->y,
			win_config->w, win_config->h, win_config->stride,
			win_config->offset);

	return 0;

fail_sync:
	dex_free_dma_buf(dex, &dma_buf_data);

fail_map:
	if (buf)
		dma_buf_put(buf);

fail_buf:
	if (handle)
		ion_free(dex->ion_client, handle);

fail:
	dex_err("failed save window configuration\n");
	return ret;
}

static int dex_set_win_config(struct dex_device *dex,
               struct s3c_fb_win_config_data *win_data)
{
	struct s3c_fb_win_config *win_config = win_data->config;
	struct dex_reg_data *regs;
	struct sync_fence *fence;
	struct sync_pt *pt;
	int fd;
	int ret = 0;
	int i = 0;

	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	mutex_lock(&dex->mutex);
	if (!dex->n_streamer) {
		dex->timeline_max++;
		pt = sw_sync_pt_create(dex->timeline, dex->timeline_max);
		fence = sync_fence_create("tv", pt);
		sync_fence_install(fence, fd);
		win_data->fence = fd;

		sw_sync_timeline_inc(dex->timeline, 1);
		goto err;
	}

	regs = kzalloc(sizeof(struct dex_reg_data), GFP_KERNEL);
	if (!regs) {
		dex_err("could not allocate dex_reg_data\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 1; i < DEX_MAX_WINDOWS; i++) {
		struct s3c_fb_win_config *config = &win_config[i];
		struct dex_win *win = dex->windows[i];
		bool enabled = 0;
		win->idx = i;

		if (!dex->windows[i]->local) {
			switch (config->state) {
			case S3C_FB_WIN_STATE_DISABLED:
				break;
			case S3C_FB_WIN_STATE_COLOR:
				break;
			case S3C_FB_WIN_STATE_BUFFER:
				ret = dex_set_win_buffer(dex, win, config, regs);
				if (!ret)
					enabled = 1;
				break;
			default:
				dex_warn("unrecongnized window state %u",
							config->state);
				ret = -EINVAL;
				break;
			}
		}
		if (enabled)
			regs->wincon[i] |= WINCONx_ENWIN;
		else
			regs->wincon[i] &= ~WINCONx_ENWIN;
	}

#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
	regs->win_overlap_bw = dex_get_overlap_bw(dex, win_config);
#endif

	if (ret) {
		for (i = 1; i < DEX_MAX_WINDOWS; i++)
			if (!dex->windows[i]->local)
				dex_free_dma_buf(dex, &regs->dma_buf_data[i]);
		goto err_free;
	} else {
		if (dex->wb_path) {
			dex->wb_timeline_max++;
			pt = sw_sync_pt_create(dex->wb_timeline, dex->wb_timeline_max);
			if (!pt) {
				dex_err("failed to create sync_pt\n");
				ret = -ENOMEM;
				goto err_free;
			}
			fence = sync_fence_create("tv", pt);
			if (!fence) {
				sync_pt_free(pt);
				dex_err("failed to craete fence\n");
				ret = -ENOMEM;
				goto err_free;
			}
			sync_fence_install(fence, fd);
			win_data->fence = fd;

			dex_set_wb_frame(dex, regs);
			kfree(regs);
		} else {
			dex->timeline_max++;
			pt = sw_sync_pt_create(dex->timeline, dex->timeline_max);
			if (!pt) {
				dex_err("failed to create sync_pt\n");
				ret = -ENOMEM;
				goto err_free;
			}
			fence = sync_fence_create("tv", pt);
			if (!fence) {
				sync_pt_free(pt);
				dex_err("failed to craete fence\n");
				ret = -ENOMEM;
				goto err_free;
			}
			sync_fence_install(fence, fd);
			win_data->fence = fd;

			mutex_lock(&dex->update_list_lock);
			list_add_tail(&regs->list, &dex->update_list);
			mutex_unlock(&dex->update_list_lock);
			queue_kthread_work(&dex->update_worker,
					&dex->update_work);
		}
	}
	goto err;

err_free:
	put_unused_fd(fd);
	kfree(regs);
	dex_err("unrecognized request, buffer freed\n");

err:
	mutex_unlock(&dex->mutex);
	return ret;
}

static int dex_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	int ret = 0;
	struct dex_win *windows = info->par;
	struct dex_device *dex = windows->dex;
	union {
		struct s3c_fb_user_window user_window;
		struct s3c_fb_user_chroma user_chroma;
		struct s3c_fb_win_config_data win_data;
		struct exynos_hdmi_data hdmi_data;
	} p;

	switch (cmd) {
	case EXYNOS_GET_HDMI_CONFIG:
		if (copy_from_user(&p.hdmi_data,
				   (struct exynos_hdmi_data __user *)arg,
				   sizeof(p.hdmi_data))) {
			ret = -EFAULT;
			break;
		}

		ret = dex_get_hdmi_config(dex, &p.hdmi_data);
		if (ret)
			break;

		if (copy_to_user((struct exynos_hdmi_data __user *)arg,
				&p.hdmi_data, sizeof(p.hdmi_data))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_SET_HDMI_CONFIG:
		if (copy_from_user(&p.hdmi_data,
				   (struct exynos_hdmi_data __user *)arg,
				   sizeof(p.hdmi_data))) {
			ret = -EFAULT;
			break;
		}

		ret = dex_set_hdmi_config(dex, &p.hdmi_data);
		break;

	case S3CFB_WIN_CONFIG:
		if (copy_from_user(&p.win_data,
				(struct s3c_fb_win_config_data __user *)arg,
				sizeof(p.win_data))) {
			ret = -EFAULT;
			break;
		}

		ret = dex_set_win_config(dex, &p.win_data);
		if (ret)
			 break;

		if (copy_to_user((struct s3c_fb_win_config_data __user *)arg,
				&p.win_data,
				sizeof(p.win_data))) {
			ret = -EFAULT;
			break;
		}
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static int dex_release(struct fb_info *info, int user)
{
	struct dex_win *win = info->par;
	struct dex_device *dex = win->dex;
	int ret = 0;
	struct media_entity *source = NULL;
	struct media_entity *sink = NULL;
	struct media_link *link;

	mutex_lock(&dex->s_mutex);

	if (dex->n_streamer) {
		dex_info("decon-tv stopped forcibly\n");
		flush_kthread_worker(&dex->update_worker);
#ifdef CONFIG_PM_RUNTIME
		ret = pm_runtime_put_sync(dex->dev);
		if (ret < 0)
			dex_err("fail to pm_runtime_put_sync()");
#else
		dex_runtime_suspend(dex->dev);
#endif
#if defined(CONFIG_DECONTV_USE_BUS_DEVFREQ)
		pm_qos_remove_request(&exynos5_decon_tv_int_qos);
		exynos5_update_media_layers(TYPE_TV, 0);
		prev_overlap_bw = 0;
#endif

		if (dex->wb_path) {
			source = &dex->wb_sd.entity;
			sink = &dex->mdev->gsc_wb_sd->entity;
			link = media_entity_find_link(source->pads, sink->pads);
			dex_info("%s - %s link disabled\n",
					source->name, sink->name);
			ret = media_entity_setup_link(link, 0);
			if (ret < 0)
				dex_err("fail to link disable\n");
			kfree(dex->porch);
		}
		dex->n_streamer--;
	}

	mutex_unlock(&dex->s_mutex);
	dex->wb_path = false;

	return ret;
}

/* ---------- FREAMBUFFER INTERFACE ----------- */
static struct fb_ops dex_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_blank	= dex_blank,
	.fb_ioctl	= dex_ioctl,
	.fb_release	= dex_release,
};

/* ---------- POWER MANAGEMENT ----------- */
static int dex_clk_set_parent(struct device *dev,
		const char *child, const char *parent)
{
	struct clk *p;
	struct clk *c;

	p = clk_get(dev, parent);
	if (IS_ERR(p)) {
		pr_err("%s: couldn't get clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c = clk_get(dev, child);
	if (IS_ERR(c)) {
		pr_err("%s: couldn't get clock : %s\n", __func__, child);
		return -EINVAL;
	}

	clk_set_parent(c, p);
	clk_put(p);
	clk_put(c);

	return 0;
}

static int dex_clk_set_rate(struct device *dev, const char *conid, unsigned int rate)
{
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: couldn't get clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_set_rate(target, rate);
	clk_put(target);

	return 0;
}

static void dex_clock_set(struct device *dev)
{
	dex_clk_set_parent(dev, "mout_sclk_decon_tv_eclk_a", "mout_bus_pll_div2");
	dex_clk_set_parent(dev, "mout_sclk_decon_tv_eclk_b", "mout_sclk_decon_tv_eclk_a");
	dex_clk_set_parent(dev, "mout_sclk_decon_tv_eclk_c", "mout_sclk_decon_tv_eclk_b");
	dex_clk_set_parent(dev, "dout_sclk_decon_tv_eclk", "mout_sclk_decon_tv_eclk_c");

	dex_clk_set_rate(dev, "dout_sclk_decon_tv_eclk", 413 * 1000000);

	dex_clk_set_parent(dev, "sclk_decon_tv_eclk_disp", "dout_sclk_decon_tv_eclk");
	dex_clk_set_parent(dev, "mout_sclk_decon_tv_eclk_user", "sclk_decon_tv_eclk_disp");
	dex_clk_set_parent(dev, "mout_sclk_decon_tv_eclk", "mout_sclk_decon_tv_eclk_user");
	dex_clk_set_parent(dev, "dout_sclk_decon_tv_eclk_disp", "mout_sclk_decon_tv_eclk");

	dex_clk_set_rate(dev, "dout_sclk_decon_tv_eclk_disp", 413 * 1000000);
}

static int dex_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dex_device *dex = platform_get_drvdata(pdev);
	struct dex_resources *res = &dex->res;

	dex_dbg("start\n");
	mutex_lock(&dex->mutex);

	if (dex->ip_ver == IP_VER_TV_5HP) {
		clk_prepare_enable(res->vclk);
		clk_prepare_enable(res->eclk);
		if (dex->wb_path)
			clk_prepare_enable(res->dsd);
	}
	clk_prepare_enable(res->decon_tv);
	dex_clock_set(dev);

	dex_reg_reset(dex);
	if (dex->wb_path)
		dex_reg_init_wb(dex);

	mutex_unlock(&dex->mutex);
	dex_dbg("finished\n");

	return 0;
}

static int dex_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dex_device *dex = platform_get_drvdata(pdev);
	struct dex_resources *res = &dex->res;

	dex_dbg("start\n");
	mutex_lock(&dex->mutex);

	clk_disable_unprepare(res->decon_tv);
	if (dex->ip_ver == IP_VER_TV_5HP) {
		clk_disable_unprepare(res->vclk);
		clk_disable_unprepare(res->eclk);
		if (dex->wb_path)
			clk_disable_unprepare(res->dsd);
	}

	mutex_unlock(&dex->mutex);
	dex_dbg("finished\n");

	return 0;
}

static const struct dev_pm_ops dex_pm_ops = {
	.runtime_suspend = dex_runtime_suspend,
	.runtime_resume	 = dex_runtime_resume,
};

/* ---------- MEDIA CONTROLLER MANAGEMENT ----------- */
static int dex_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *format)
{
	struct dex_device *dex = v4l2_subdev_to_dex_device(sd);
	struct decon_tv_porch *porch;

	if (format->pad != DEX_PAD_WB) {
		dex_err("it is possible only output format setting\n");
		return -EINVAL;
	}

	if (format->format.width * format->format.height > 2560 * 1600) {
		dex_err("size is bigger than 2560x1600\n");
		return -EINVAL;
	}

	if (format->format.width % 4) {
		dex_err("width size should be kept to 4 pixel aligned\n");
		return -EINVAL;
	}

	porch = kzalloc(sizeof(struct decon_tv_porch), GFP_KERNEL);
	if (!porch) {
		dex_err("could not allocate decon_tv_porch\n");
		return -ENOMEM;
	}

	dex->porch = porch;
	dex->porch->xres = format->format.width;
	dex->porch->yres = format->format.height;
	dex->porch->fps = 60;
	dex_dbg("decon-tv output size for writeback %dx%d\n",
			dex->porch->xres, dex->porch->yres);

	return 0;
}

static int dex_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct dex_win *win = v4l2_subdev_to_dex_win(sd);
	struct dex_device *dex = win->dex;

	if (enable)
		dex_reg_local_on(dex, win->idx);
	else
		dex_reg_local_off(dex, win->idx);

	return 0;
}

/* sub-device for GSC - DECON_TV - HDMI */
static const struct v4l2_subdev_video_ops dex_sd_video_ops = {
	.s_stream = dex_s_stream,
};

static const struct v4l2_subdev_ops dex_sd_ops = {
	.video = &dex_sd_video_ops,
};

/* sub-device for DECON_TV - GSC */
static const struct v4l2_subdev_video_ops dex_wb_video_ops = {
	.s_stream = dex_s_stream,
};

static const struct v4l2_subdev_pad_ops	dex_wb_pad_ops = {
	.set_fmt = dex_set_fmt,
};

static const struct v4l2_subdev_ops dex_writeback_ops = {
	.video = &dex_wb_video_ops,
	.pad = &dex_wb_pad_ops,
};

static int dex_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct dex_win *win = v4l2_subdev_to_dex_win(sd);
	int i;
	int gsc_num = 0;

	if (flags & MEDIA_LNK_FL_ENABLED) {
		win->use = 1;
		if (local->index == DEX_PAD_SINK)
			win->local = 1;
	} else {
		if (local->index == DEX_PAD_SINK)
			win->local = 0;
		win->use = 0;
		for (i = 0; i < entity->num_links; ++i)
			if (entity->links[i].flags & MEDIA_LNK_FL_ENABLED)
				win->use = 1;
	}

	if (!strcmp(remote->entity->name, "exynos-gsc-sd.0"))
		gsc_num = 0;
	else if (!strcmp(remote->entity->name, "exynos-gsc-sd.1"))
		gsc_num = 1;
	else if (!strcmp(remote->entity->name, "exynos-gsc-sd.2"))
		gsc_num = 2;

	win->gsc_num = gsc_num;
	dex_dbg("MC link setup between %s and %s\n",
				entity->name, remote->entity->name);

	return 0;
}

static int dex_wb_link_setup(struct media_entity *entity,
		      const struct media_pad *local,
		      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct dex_device *dex =
		container_of(sd, struct dex_device, wb_sd);

	if (flags & MEDIA_LNK_FL_ENABLED) {
		dex->wb_path = true;
		dex_info("link setup enabled\n");
	} else {
		dex->wb_path = false;
		dex_info("link setup disabled\n");
	}

	return 0;
}

static const struct media_entity_operations dex_entity_ops = {
	.link_setup = dex_link_setup,
};

static const struct media_entity_operations wb_entity_ops = {
	.link_setup = dex_wb_link_setup,
};

static int dex_register_subdev_nodes(struct dex_device *dex)
{
	int ret;
	struct exynos_md *md;

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		dex_err("failed to get output media device\n");
		ret = -ENODEV;
		return ret;
	}

	/*
	 * This function is for exposing sub-devices nodes to user space
	 * in case of marking with V4L2_SUBDEV_FL_HAS_DEVNODE flag.
	 *
	 * And it depends on probe sequence
	 * because v4l2_dev ptr is shared all of output devices below
	 *
	 * probe sequence of output devices
	 * output media device -> gscaler -> window in fimd
	 */
	ret = v4l2_device_register_subdev_nodes(&md->v4l2_dev);
	if (ret) {
		dex_err("failed to make nodes for subdev\n");
		return ret;
	}

	if (dex->ip_ver == IP_VER_TV_5HP) {
		ret = v4l2_device_register_subdev_nodes(&dex->mdev->v4l2_dev);
		if (ret) {
			dex_err("failed to make nodes for writeback\n");
			return ret;
		}
	}

	dex_dbg("register V4L2 subdev nodes for TV\n");

	return 0;
}

static int check_mdev_for_wb(struct device *dev, void *id)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (pdev->id == (enum mdev_node)id)
		return -1;

	return 0;
}

static int dex_find_capture_mdev(struct dex_device *dex)
{
	struct device_driver *driver;
	struct device *dev;
	struct exynos_md *md;

	driver = driver_find(MDEV_MODULE_NAME, &platform_bus_type);
	if (likely(driver)) {
		dev = driver_find_device(driver, NULL,
			(void *)MDEV_CAPTURE, check_mdev_for_wb);
		md = (struct exynos_md *)dev_get_drvdata(dev);
		dex->mdev = md;
	} else {
		dex_err("failed to find capture media device\n");
		return -EINVAL;
	}

	return 0;
}

static int dex_create_links_for_wb(struct dex_device *dex)
{
	int ret = media_entity_create_link(&dex->wb_sd.entity, 0,
			&dex->mdev->gsc_wb_sd->entity,
			GSC_CAP_PAD_SINK, 0);
	if (ret) {
		dex_err("failed create link for writeback\n");
		return ret;
	}

	return 0;
}

static int dex_create_links(struct dex_win *win)
{
	struct dex_device *dex = win->dex;
	struct exynos_md *md;
	int ret, i;
	int flags = 0;
	char err[80];

	if (win->idx == DEX_BACKGROUND)
		return 0;

	dex_dbg("decon_tv create links\n");
	memset(err, 0, sizeof(err));

	/* link creation : gscaler0~3[1] -> Window1~4[0] */
	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		dex_err("failed to get output media device\n");
		ret = -ENODEV;
		goto fail;
	}
	for (i = 0; i < MAX_GSC_SUBDEV; ++i) {
		if (md->gsc_sd[i] != NULL) {
			ret = media_entity_create_link(&md->gsc_sd[i]->entity,
					GSC_OUT_PAD_SOURCE, &win->sd.entity,
					DEX_PAD_SINK, flags);
			if (ret) {
				snprintf(err, sizeof(err), "%s --> %s",
					md->gsc_sd[i]->entity.name,
					win->sd.entity.name);
				goto fail;
			}
		}
	}

	/* link creation : Window1~4[pad1] -> hdmi[0] */
	if (!strcmp(dex->hdmi_sd->name, "s5p-hdmi-sd"))
		flags = MEDIA_LNK_FL_ENABLED;
	ret = media_entity_create_link(&win->sd.entity, DEX_PAD_SOURCE,
			&dex->hdmi_sd->entity, 0, flags);
	if (ret) {
		snprintf(err, sizeof(err), "%s --> %s",
				win->sd.entity.name,
				dex->hdmi_sd->entity.name);
		goto fail;
	}

	dex_dbg("decon_tv links are created successfully\n");
	return 0;

fail:
	dex_err("failed to create link : %s\n", err);
	return ret;
}

static int dex_register_wb_entity(struct dex_device *dex)
{
	struct v4l2_subdev *sd = &dex->wb_sd;
	struct media_pad *pad = &dex->pad;
	struct media_entity *me = &sd->entity;
	int ret;

	v4l2_subdev_init(sd, &dex_writeback_ops);
	sd->owner = THIS_MODULE;
	snprintf(sd->name, sizeof(sd->name), DEX_WB_NAME);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	pad->flags = MEDIA_PAD_FL_SOURCE;
	me->ops = &wb_entity_ops;
	ret = media_entity_init(me, DEX_WB_PADS_NUM, pad, 0);
	if (ret) {
		dex_err("failed to initialize media entity\n");
		return ret;
	}

	ret = v4l2_device_register_subdev(&dex->mdev->v4l2_dev, sd);
	if (ret) {
		dex_err("failed to register decon_tv subdev\n");
		return ret;
	}

	return 0;
}

static int dex_register_entity(struct dex_win *win)
{
	struct dex_device *dex = win->dex;
	struct v4l2_subdev *sd = &win->sd;
	struct v4l2_subdev *hdmi_sd;
	struct media_pad *pads = win->pads;
	struct media_entity *me = &sd->entity;
	struct exynos_md *md;
	int ret;

	if (win->idx == DEX_BACKGROUND)
		return 0;

	dex_dbg("decon_tv sub-dev entity init\n");

	/* init decon_tv sub-device */
	v4l2_subdev_init(sd, &dex_sd_ops);
	sd->owner = THIS_MODULE;
	snprintf(sd->name, sizeof(sd->name), "decon-tv-sd%d", win->idx);

	/* decon_tv sub-device can be opened in user space */
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* init decon_tv sub-device as entity */
	pads[DEX_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	pads[DEX_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	me->ops = &dex_entity_ops;
	ret = media_entity_init(me, DEX_PADS_NUM, pads, 0);
	if (ret) {
		dex_err("failed to initialize media entity\n");
		return ret;
	}

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		dex_err("failed to get output media device\n");
		return -ENODEV;
	}

	ret = v4l2_device_register_subdev(&md->v4l2_dev, sd);
	if (ret) {
		dex_err("failed to register decon_tv subdev\n");
		return ret;
	}

	/* find subdev of output devices */
	hdmi_sd = (struct v4l2_subdev *)module_name_to_driver_data("s5p-hdmi");
	if (!hdmi_sd) {
		dex_err("failed to get hdmi device\n");
		return -ENODEV;
	}
	dex->hdmi_sd = hdmi_sd;

	return 0;
}

static void dex_unregister_entity(struct dex_win *win)
{
	v4l2_device_unregister_subdev(&win->sd);
}

static int dex_acquire_windows(struct dex_device *dex, int idx,
					struct dex_win **dex_win)
{
	struct dex_win *win;
	struct fb_info *fbinfo;

	dex_dbg("acquire decon_tv window%d\n", idx);

	fbinfo = framebuffer_alloc(sizeof(struct dex_win), dex->dev);
	if (!fbinfo) {
		dex_err("failed to allocate framebuffer\n");
		return -ENOENT;
	}

	win = fbinfo->par;
	*dex_win = win;
	win->fbinfo = fbinfo;
	fbinfo->fbops = &dex_fb_ops;
	fbinfo->flags = FBINFO_FLAG_DEFAULT;
	win->dex = dex;
	win->idx = idx;

	dex_dbg("decon_tv window[%d] create\n", idx);
	return 0;
}

static void dex_release_windows(struct dex_device *dex, struct dex_win *win)
{
	if (win->fbinfo)
		framebuffer_release(win->fbinfo);
}

/* --------- DRIVER INITIALIZATION ---------- */
static int dex_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dex_device *dex;
	struct resource *res;
	struct fb_info *fbinfo;
	int i;
	int ret = 0;
	char device_name[MAX_NAME_SIZE];

	dev_info(dev, "probe start\n");

	dex = devm_kzalloc(dev, sizeof(struct dex_device), GFP_KERNEL);
	if (!dex) {
		dev_err(dev, "no memory for decon_tv device\n");
		return -ENOMEM;
	}

	/* setup pointer to master device */
	dex->dev = dev;

	/* store platform data ptr to decon_tv context */
	of_property_read_u32(dev->of_node, "ip_ver", &dex->ip_ver);
	dev_info(dev, "DECON-TV ip version %d\n", dex->ip_ver);

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dex->res.dex_regs = devm_request_and_ioremap(dev, res);
	if (dex->res.dex_regs == NULL) {
		dex_err("failed to claim register region\n");
		return -ENOENT;
	}

	spin_lock_init(&dex->reg_slock);
	/* Get IRQ resource and register IRQ handler. */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ret = devm_request_irq(dev, res->start, dex_irq_handler, 0,
			pdev->name, dex);
	if (ret) {
		dex_err("failed to install irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	ret = devm_request_irq(dev, res->start, dex_irq_handler, 0,
			pdev->name, dex);
	if (ret) {
		dex_err("failed to install irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
	ret = devm_request_irq(dev, res->start, dex_irq_handler, 0,
			pdev->name, dex);
	if (ret) {
		dex_err("failed to install irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 3);
	ret = devm_request_irq(dev, res->start, dex_irq_handler, 0,
			pdev->name, dex);
	if (ret) {
		dex_err("failed to install irq\n");
		return ret;
	}

	dex->res.decon_tv = clk_get(dex->dev, "gate_tv_mixer");
	if (IS_ERR_OR_NULL(dex->res.decon_tv)) {
		dex_err("failed to get clock 'decon_tv'\n");
		goto fail_resource;
	}
	if (dex->ip_ver == IP_VER_TV_5HP) {
		dex->res.vclk = clk_get(dex->dev, "sclk_decon_tv_vclk");
		if (IS_ERR_OR_NULL(dex->res.vclk)) {
			dex_err("failed to get clock 'vclk'\n");
			goto fail_clk_1;
		}
		dex->res.dsd = clk_get(dex->dev, "gate_dsd");
		if (IS_ERR_OR_NULL(dex->res.dsd)) {
			dex_err("failed to get clock 'gate_dsd'\n");
			goto fail_clk_2;
		}
		dex->res.eclk = clk_get(dex->dev, "gate_decontv_eclk");
		if (IS_ERR_OR_NULL(dex->res.eclk)) {
			dex_err("failed to get clock 'eclk'\n");
			goto fail_clk_3;
		}
	}

	mutex_init(&dex->mutex);
	mutex_init(&dex->s_mutex);
	init_waitqueue_head(&dex->vsync_wait);

	/* init work thread for update registers */
	INIT_LIST_HEAD(&dex->update_list);
	mutex_init(&dex->update_list_lock);
	init_kthread_worker(&dex->update_worker);

	dex->update_thread = kthread_run(kthread_worker_fn,
			&dex->update_worker, DEX_DRIVER_NAME);
	if (IS_ERR(dex->update_thread)) {
		ret = PTR_ERR(dex->update_thread);
		dex->update_thread = NULL;
		dex_err("failed to run update_regs thread\n");
		goto fail_thread;
	}
	init_kthread_work(&dex->update_work, dex_update_handler);

	snprintf(device_name, MAX_NAME_SIZE, "decon-tv");
	dex->timeline = sw_sync_timeline_create(device_name);
	if (!dex->timeline) {
		dex_err("failed to create timeline\n");
		goto fail_ion;
	}
	dex->timeline_max = 1;

	snprintf(device_name, MAX_NAME_SIZE, "decon-tv-wb");
	dex->wb_timeline = sw_sync_timeline_create(device_name);
	if (!dex->timeline) {
		dex_err("failed to create wb timeline\n");
		goto fail_ion;
	}
	dex->wb_timeline_max = 0;

	dex->ion_client = ion_client_create(ion_exynos, "decon_tv");
	if (IS_ERR(dex->ion_client)) {
		dex_err("failed to ion_client_create\n");
		goto fail_ion;
	}

	exynos_create_iovmm(dev, 5, 0);
	ret = iovmm_activate(dev);
	if (ret < 0) {
		dex_err("failed to reactivate vmm\n");
		goto fail_ion;
	}

	/* configure windows */
	for (i = 0; i < DEX_MAX_WINDOWS; i++) {
		ret = dex_acquire_windows(dex, i, &dex->windows[i]);
		if (ret < 0) {
			dex_err("failed to create window[%d]\n", i);
			for (; i >= 0; i--)
				dex_release_windows(dex, dex->windows[i]);
			goto fail_iovmm;
		}

		/* register a window subdev as entity */
		ret = dex_register_entity(dex->windows[i]);
		if (ret) {
			dex_err("failed to register mc entity\n");
			goto fail_iovmm;
		}

		/* create links connected to gscaler, decon_tv inputs and hdmi */
		ret = dex_create_links(dex->windows[i]);
		if (ret) {
			dex_err("failed to create link\n");
			goto fail_entity;
		}
	}

	/*
	 * Configure subdevice for Writeback
	 */
	if (dex->ip_ver == IP_VER_TV_5HP) {
		if (dex_find_capture_mdev(dex))
			goto fail_entity;

		if (dex_register_wb_entity(dex))
			goto fail_entity;

		if (dex_create_links_for_wb(dex))
			goto fail_wb;
	}

	ret = dex_register_subdev_nodes(dex);
	if (ret) {
		dex_err("failed to register decon-tv mc subdev node\n");
		goto fail_wb;
	}

	fbinfo = dex->windows[DEX_DEFAULT_WIN]->fbinfo;
	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		dex_err("failed to register framebuffer\n");
		goto fail_wb;
	}

	platform_set_drvdata(pdev, dex);
	pm_runtime_enable(dev);
	dev_info(dev, "decon_tv registered successfully");

	return 0;

fail_wb:
	if (dex->ip_ver == IP_VER_TV_5HP)
		v4l2_device_unregister_subdev(&dex->wb_sd);

fail_entity:
	dex_unregister_entity(dex->windows[i]);

fail_iovmm:
	iovmm_deactivate(dev);

fail_ion:
	if (dex->update_thread)
		kthread_stop(dex->update_thread);

fail_thread:
	if (dex->ip_ver == IP_VER_TV_5HP)
		clk_put(dex->res.eclk);

fail_clk_3:
	if (dex->ip_ver == IP_VER_TV_5HP)
		clk_put(dex->res.dsd);

fail_clk_2:
	if (dex->ip_ver == IP_VER_TV_5HP)
		clk_put(dex->res.vclk);

fail_clk_1:
	clk_put(dex->res.decon_tv);

fail_resource:
	return ret;
}

static int dex_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dex_device *dex = platform_get_drvdata(pdev);
	int i;

	pm_runtime_disable(dev);
	iovmm_deactivate(dev);
	unregister_framebuffer(dex->windows[0]->fbinfo);

	if (dex->update_thread)
		kthread_stop(dex->update_thread);

	for (i = 0; i < DEX_MAX_WINDOWS; i++)
		dex_release_windows(dex, dex->windows[i]);

	kfree(dex);

	dev_info(dev, "remove sucessful\n");
	return 0;
}

static struct platform_driver dex_driver __refdata = {
	.probe		= dex_probe,
	.remove		= dex_remove,
	.driver = {
		.name	= DEX_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= &dex_pm_ops,
		.of_match_table = of_match_ptr(decon_tv_device_table),
	}
};

static int s5p_decon_tv_register(void)
{
	platform_driver_register(&dex_driver);

	return 0;
}

static void s5p_decon_tv_unregister(void)
{
	platform_driver_unregister(&dex_driver);
}
late_initcall(s5p_decon_tv_register);
module_exit(s5p_decon_tv_unregister);

MODULE_AUTHOR("Ayoung Sim <a.sim@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS5 Soc series TVOUT driver");
MODULE_LICENSE("GPL");
