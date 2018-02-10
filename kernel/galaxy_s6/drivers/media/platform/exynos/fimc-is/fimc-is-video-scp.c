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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"

const struct v4l2_file_operations fimc_is_scp_video_fops;
const struct v4l2_ioctl_ops fimc_is_scp_video_ioctl_ops;
const struct vb2_ops fimc_is_scp_qops;

int fimc_is_scp_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_video *video;

	BUG_ON(!data);

	core = (struct fimc_is_core *)data;
	video = &core->video_scp;

	if (!core->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_probe(video,
		FIMC_IS_VIDEO_SCP_NAME,
		FIMC_IS_VIDEO_SCP_NUM,
		VFL_DIR_RX,
		&core->mem,
		&core->v4l2_dev,
		&video->lock,
		&fimc_is_scp_video_fops,
		&fimc_is_scp_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/*
 * =============================================================================
 * Video File Opertation
 * =============================================================================
 */

static int fimc_is_scp_video_open(struct file *file)
{
	int ret = 0;
	u32 refcount;
	struct fimc_is_core *core;
	struct fimc_is_video *video;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_ischain *device;

	vctx = NULL;
	video = video_drvdata(file);
	core = container_of(video, struct fimc_is_core, video_scp);

	ret = open_vctx(file, video, &vctx, FRAMEMGR_ID_INVALID, FRAMEMGR_ID_SCP);
	if (ret) {
		err("open_vctx is fail(%d)", ret);
		goto p_err;
	}

	info("[SCP:V:%d] %s\n", vctx->instance, __func__);

	refcount = atomic_read(&core->video_isp.refcount);
	if (refcount > FIMC_IS_MAX_NODES || refcount < 1) {
		err("invalid ischain refcount(%d)", refcount);
		close_vctx(file, video, vctx);
		ret = -EINVAL;
		goto p_err;
	}

	device = &core->ischain[refcount - 1];

	ret = fimc_is_video_open(vctx,
		device,
		VIDEO_SCP_READY_BUFFERS,
		video,
		FIMC_IS_VIDEO_TYPE_CAPTURE,
		&fimc_is_scp_qops,
		NULL,
		&fimc_is_ischain_sub_ops);
	if (ret) {
		err("fimc_is_video_open is fail");
		close_vctx(file, video, vctx);
		goto p_err;
	}

	ret = fimc_is_subdev_open(&device->scp, vctx, NULL);
	if (ret) {
		err("fimc_is_subdev_open is fail");
		close_vctx(file, video, vctx);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_scp_video_close(struct file *file)
{
	int ret = 0;
	struct fimc_is_video *video = NULL;
	struct fimc_is_video_ctx *vctx = NULL;
	struct fimc_is_device_ischain *device = NULL;

	BUG_ON(!file);

	vctx = file->private_data;
	if (!vctx) {
		err("vctx is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video = vctx->video;
	if (!video) {
		merr("video is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	info("[SCP:V:%d] %s\n", vctx->instance, __func__);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_subdev_close(&device->scp);
	fimc_is_video_close(vctx);

	ret = close_vctx(file, video, vctx);
	if (ret < 0)
		err("close_vctx is fail(%d)", ret);

p_err:
	return ret;
}

static unsigned int fimc_is_scp_video_poll(struct file *file,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	ret = fimc_is_video_poll(file, vctx, wait);

	return ret;
}

static int fimc_is_scp_video_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	ret = fimc_is_video_mmap(file, vctx, vma);
	if (ret)
		merr("fimc_is_video_mmap is fail(%d)", vctx, ret);

	return ret;
}

const struct v4l2_file_operations fimc_is_scp_video_fops = {
	.owner		= THIS_MODULE,
	.open		= fimc_is_scp_video_open,
	.release	= fimc_is_scp_video_close,
	.poll		= fimc_is_scp_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= fimc_is_scp_video_mmap,
};

/*
 * =============================================================================
 * Video Ioctl Opertation
 * =============================================================================
 */

static int fimc_is_scp_video_querycap(struct file *file, void *fh,
	struct v4l2_capability *cap)
{
	struct fimc_is_core *core = video_drvdata(file);

	strncpy(cap->driver, core->pdev->name, sizeof(cap->driver) - 1);

	dbg("%s(devname : %s)\n", __func__, cap->driver);
	strncpy(cap->card, core->pdev->name, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
	cap->capabilities = V4L2_CAP_STREAMING
					| V4L2_CAP_VIDEO_CAPTURE
					| V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	return 0;
}

static int fimc_is_scp_video_enum_fmt_mplane(struct file *file, void *priv,
	struct v4l2_fmtdesc *f)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scp_video_get_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scp_video_set_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_queue *queue;
	struct fimc_is_device_ischain *ischain;

	BUG_ON(!vctx);
	BUG_ON(!vctx->device);
	BUG_ON(!format);

	mdbgv_scp("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	ischain = vctx->device;

	ret = fimc_is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("fimc_is_video_set_format_mplane is fail(%d)", vctx, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_scp_s_format(ischain,
		queue->framecfg.format.pixelformat,
		queue->framecfg.width,
		queue->framecfg.height);
	if (ret)
		merr("fimc_is_ischain_scp_s_format is fail(%d)", vctx, ret);

p_err:
	return ret;
}

static int fimc_is_scp_video_try_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scp_video_cropcap(struct file *file, void *fh,
						struct v4l2_cropcap *cropcap)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scp_video_get_crop(struct file *file, void *fh,
						struct v4l2_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scp_video_set_crop(struct file *file, void *fh,
	struct v4l2_crop *crop)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_queue *queue;
	struct fimc_is_device_ischain *device;

	BUG_ON(!vctx);

	mdbgv_scp("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	device = vctx->device;
	if (!device) {
		merr("device is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_ischain_scp_s_format(device,
		queue->framecfg.format.pixelformat,
		crop->c.width,
		crop->c.height);
	if (ret)
		merr("fimc_is_ischain_scp_s_format is fail(%d)", vctx, ret);

p_err:
	return ret;
}

static int fimc_is_scp_video_reqbufs(struct file *file, void *priv,
					struct v4l2_requestbuffers *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *subdev;
	struct fimc_is_subdev *leader;

	BUG_ON(!vctx);

	mdbgv_scp("%s(buffers : %d)\n", vctx, __func__, buf->count);

	device = vctx->device;
	subdev = &device->scp;
	leader = subdev->leader;

	if (!leader) {
		merr("leader is NULL ptr", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		merr("leader still running, not applied", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_reqbufs(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_reqbufs is fail(%d)", vctx, ret);

p_err:
	return ret;
}

static int fimc_is_scp_video_querybuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_scp("%s\n", vctx, __func__);

	ret = fimc_is_video_querybuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_querybuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_scp_video_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

#ifdef DBG_STREAMING
	mdbgv_scp("%s(index : %d)\n", vctx, __func__, buf->index);
#endif

	ret = fimc_is_video_qbuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_qbuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_scp_video_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

#ifdef DBG_STREAMING
	mdbgv_scp("%s\n", vctx, __func__);
#endif

	ret = fimc_is_video_dqbuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_dqbuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_scp_video_streamon(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_scp("%s\n", vctx, __func__);

	ret = fimc_is_video_streamon(file, vctx, type);
	if (ret)
		merr("fimc_is_video_streamon is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_scp_video_streamoff(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_scp("%s\n", vctx, __func__);

	ret = fimc_is_video_streamoff(file, vctx, type);
	if (ret)
		merr("fimc_is_video_streamoff is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_scp_video_enum_input(struct file *file, void *priv,
	struct v4l2_input *input)
{
	/* Todo : add to enum input control code */
	return 0;
}

static int fimc_is_scp_video_g_input(struct file *file, void *priv,
	unsigned int *input)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scp_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	return 0;
}

static int fimc_is_scp_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_framemgr *framemgr;

	BUG_ON(!vctx);
	BUG_ON(!ctrl);

	mdbgv_scp("%s\n", vctx, __func__);

	framemgr = GET_DST_FRAMEMGR(vctx);

	switch (ctrl->id) {
	case V4L2_CID_IS_G_COMPLETES:
		ctrl->value = framemgr->frame_com_cnt;
		break;
	default:
		err("unsupported ioctl(%d)\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int fimc_is_scp_video_g_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	return 0;
}

static int fimc_is_scp_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_ischain *device;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!vctx);
	BUG_ON(!ctrl);

	mdbgv_scp("%s\n", vctx, __func__);

	device = vctx->device;
	if (!device) {
		merr("device is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = GET_DST_FRAMEMGR(vctx);
	frame = NULL;

	switch (ctrl->id) {
	case V4L2_CID_IS_FORCE_DONE:
		if (framemgr->frame_pro_cnt) {
			err("force done can be performed(process count %d)",
				framemgr->frame_pro_cnt);
			ret = -EINVAL;
		} else if (!framemgr->frame_req_cnt) {
			err("force done can be performed(request count %d)",
				framemgr->frame_req_cnt);
			ret = -EINVAL;
		} else {
			framemgr_e_barrier_irqs(framemgr, FMGR_IDX_0, flags);

			fimc_is_frame_request_head(framemgr, &frame);
			if (frame) {
				fimc_is_frame_trans_req_to_com(framemgr, frame);
				buffer_done(vctx, frame->index);
			} else {
				err("frame is NULL");
				ret = -EINVAL;
			}

			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_0, flags);
		}
		break;
	case V4L2_CID_IS_COLOR_RANGE:
		if (test_bit(FIMC_IS_SUBDEV_START, &device->group_isp.leader.state)) {
			err("failed to change color range: device started already (0x%08x)",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->color_range &= ~FIMC_IS_SCP_CRANGE_MASK;

			if (ctrl->value)
				device->color_range	|=
					(FIMC_IS_CRANGE_LIMITED << FIMC_IS_SCP_CRANGE_SHIFT);
		}
		break;
	default:
		err("unsupported ioctl(%d)\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops fimc_is_scp_video_ioctl_ops = {
	.vidioc_querycap		= fimc_is_scp_video_querycap,
	.vidioc_enum_fmt_vid_cap_mplane	= fimc_is_scp_video_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= fimc_is_scp_video_get_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= fimc_is_scp_video_set_format_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= fimc_is_scp_video_try_format_mplane,
	.vidioc_cropcap			= fimc_is_scp_video_cropcap,
	.vidioc_g_crop			= fimc_is_scp_video_get_crop,
	.vidioc_s_crop			= fimc_is_scp_video_set_crop,
	.vidioc_reqbufs			= fimc_is_scp_video_reqbufs,
	.vidioc_querybuf		= fimc_is_scp_video_querybuf,
	.vidioc_qbuf			= fimc_is_scp_video_qbuf,
	.vidioc_dqbuf			= fimc_is_scp_video_dqbuf,
	.vidioc_streamon		= fimc_is_scp_video_streamon,
	.vidioc_streamoff		= fimc_is_scp_video_streamoff,
	.vidioc_enum_input		= fimc_is_scp_video_enum_input,
	.vidioc_g_input			= fimc_is_scp_video_g_input,
	.vidioc_s_input			= fimc_is_scp_video_s_input,
	.vidioc_g_ctrl			= fimc_is_scp_video_g_ctrl,
	.vidioc_s_ctrl			= fimc_is_scp_video_s_ctrl,
	.vidioc_g_ext_ctrls		= fimc_is_scp_video_g_ext_ctrl,
};

static int fimc_is_scp_queue_setup(struct vb2_queue *vbq,
	const struct v4l2_format *fmt,
	unsigned int *num_buffers,
	unsigned int *num_planes,
	unsigned int sizes[],
	void *allocators[])
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!vctx->video);

	mdbgv_scp("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	video = vctx->video;

	ret = fimc_is_queue_setup(queue,
		video->alloc_ctx,
		num_planes,
		sizes,
		allocators);
	if (ret)
		merr("fimc_is_queue_setup is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_scp_buffer_prepare(struct vb2_buffer *vb)
{
	return 0;
}

static inline void fimc_is_scp_wait_prepare(struct vb2_queue *vbq)
{
	fimc_is_queue_wait_prepare(vbq);
}

static inline void fimc_is_scp_wait_finish(struct vb2_queue *vbq)
{
	fimc_is_queue_wait_finish(vbq);
}

static int fimc_is_scp_start_streaming(struct vb2_queue *q,
	unsigned int count)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = q->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *subdev;

	BUG_ON(!vctx);

	mdbgv_scp("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	device = vctx->device;
	subdev = &device->scp;

	ret = fimc_is_queue_start_streaming(queue, device, subdev, vctx);
	if (ret)
		merr("fimc_is_queue_start_streaming is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_scp_stop_streaming(struct vb2_queue *q)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = q->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *subdev;

	BUG_ON(!vctx);

	mdbgv_scp("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	subdev = &device->scp;

	ret = fimc_is_queue_stop_streaming(queue, device, subdev, vctx);
	if (ret)
		merr("fimc_is_queue_stop_streaming is fail(%d)", vctx, ret);

p_err:
	return ret;
}

static void fimc_is_scp_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_video *video;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *subdev;

	BUG_ON(!vctx);

#ifdef DBG_STREAMING
	dbg_scp("%s\n", __func__);
#endif

	queue = GET_DST_QUEUE(vctx);
	video = vctx->video;
	device = vctx->device;
	subdev = &device->scp;

	ret = fimc_is_queue_buffer_queue(queue, video->vb2, vb);
	if (ret) {
		merr("fimc_is_queue_buffer_queue is fail(%d)", vctx, ret);
		return;
	}

	ret = fimc_is_subdev_buffer_queue(subdev, vb->v4l2_buf.index);
	if (ret) {
		merr("fimc_is_subdev_buffer_queue is fail(%d)", vctx, ret);
		return;
	}
}

static int fimc_is_scp_buffer_finish(struct vb2_buffer *vb)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_device_ischain *ischain = vctx->device;
	struct fimc_is_subdev *scp = &ischain->scp;

#ifdef DBG_STREAMING
	dbg_scp("%s(%d)\n", __func__, vb->v4l2_buf.index);
#endif

	ret = fimc_is_subdev_buffer_finish(scp, vb->v4l2_buf.index);

	return ret;
}

const struct vb2_ops fimc_is_scp_qops = {
	.queue_setup		= fimc_is_scp_queue_setup,
	.buf_prepare		= fimc_is_scp_buffer_prepare,
	.buf_queue		= fimc_is_scp_buffer_queue,
	.buf_finish		= fimc_is_scp_buffer_finish,
	.wait_prepare		= fimc_is_scp_wait_prepare,
	.wait_finish		= fimc_is_scp_wait_finish,
	.start_streaming	= fimc_is_scp_start_streaming,
	.stop_streaming		= fimc_is_scp_stop_streaming,
};
