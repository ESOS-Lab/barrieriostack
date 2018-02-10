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
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-core.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-cmd.h"
#include "fimc-is-dvfs.h"

/* sysfs variable for debug */
extern struct fimc_is_sysfs_debug sysfs_debug;

static void fimc_is_gframe_s_info(struct fimc_is_group_frame *item,
	u32 group_id, struct fimc_is_frame *frame)
{
	BUG_ON(!item);
	BUG_ON(!frame);
	BUG_ON(!frame->shot_ext);

	memcpy(&item->group_cfg[group_id], &frame->shot_ext->node_group,
		sizeof(struct camera2_node_group));
}

static void fimc_is_gframe_free_head(struct fimc_is_group_framemgr *framemgr,
	struct fimc_is_group_frame **item)
{
	if (framemgr->frame_free_cnt)
		*item = container_of(framemgr->frame_free_head.next,
			struct fimc_is_group_frame, list);
	else
		*item = NULL;
}

static void fimc_is_gframe_s_free(struct fimc_is_group_framemgr *framemgr,
	struct fimc_is_group_frame *item)
{
	BUG_ON(!framemgr);
	BUG_ON(!item);

	list_add_tail(&item->list, &framemgr->frame_free_head);
	framemgr->frame_free_cnt++;
}

static void fimc_is_gframe_print_free(struct fimc_is_group_framemgr *framemgr)
{
	struct list_head *temp;
	struct fimc_is_group_frame *gframe;

	BUG_ON(!framemgr);

	printk(KERN_ERR "[GFRM] fre(%d) :", framemgr->frame_free_cnt);

	list_for_each(temp, &framemgr->frame_free_head) {
		gframe = list_entry(temp, struct fimc_is_group_frame, list);
		printk(KERN_CONT "%d->", gframe->fcount);
	}

	printk(KERN_CONT "X\n");
}

static void fimc_is_gframe_group_head(struct fimc_is_group *group,
	struct fimc_is_group_frame **item)
{
	if (group->frame_group_cnt)
		*item = container_of(group->frame_group_head.next,
			struct fimc_is_group_frame, list);
	else
		*item = NULL;
}

static void fimc_is_gframe_s_group(struct fimc_is_group *group,
	struct fimc_is_group_frame *item)
{
	BUG_ON(!group);
	BUG_ON(!item);

	list_add_tail(&item->list, &group->frame_group_head);
	group->frame_group_cnt++;
}

static void fimc_is_gframe_print_group(struct fimc_is_group *group)
{
	struct list_head *temp;
	struct fimc_is_group_frame *gframe;

	BUG_ON(!group);

	printk(KERN_ERR "[GFRM] req(%d, %d) :", group->id, group->frame_group_cnt);

	list_for_each(temp, &group->frame_group_head) {
		gframe = list_entry(temp, struct fimc_is_group_frame, list);
		printk(KERN_CONT "%d->", gframe->fcount);
	}

	printk(KERN_CONT "X\n");
}

static int fimc_is_gframe_trans_fre_to_grp(struct fimc_is_group_framemgr *prev,
	struct fimc_is_group *next,
	struct fimc_is_group_frame *item)
{
	int ret = 0;

	BUG_ON(!prev);
	BUG_ON(!next);
	BUG_ON(!item);

	if (!prev->frame_free_cnt) {
		err("frame_free_cnt is zero");
		ret = -EFAULT;
		goto exit;
	}

	list_del(&item->list);
	prev->frame_free_cnt--;

	fimc_is_gframe_s_group(next, item);

exit:
	return ret;
}

static int fimc_is_gframe_trans_grp_to_grp(struct fimc_is_group *prev,
	struct fimc_is_group *next,
	struct fimc_is_group_frame *item)
{
	int ret = 0;
	u32 *input_crop, *output_crop;

	BUG_ON(!prev);
	BUG_ON(!next);
	BUG_ON(!item);

	if (!prev->frame_group_cnt) {
		err("frame_group_cnt is zero");
		ret = -EFAULT;
		goto p_err;
	}

	list_del(&item->list);
	prev->frame_group_cnt--;

	fimc_is_gframe_s_group(next, item);

	if (item->group_cfg[prev->id].capture[0].vid == next->source_vid) {
		output_crop = item->group_cfg[prev->id].capture[0].output.cropRegion;
		input_crop = item->group_cfg[next->id].leader.input.cropRegion;
	} else if (item->group_cfg[prev->id].capture[1].vid == next->source_vid) {
		output_crop = item->group_cfg[prev->id].capture[1].output.cropRegion;
		input_crop = item->group_cfg[next->id].leader.input.cropRegion;
	} else {
		merr("vid(%d) is invalid", prev, next->source_vid);
		ret = -EINVAL;
		goto p_err;
	}

	if ((output_crop[0] != input_crop[0]) || (output_crop[1] != input_crop[1]) ||
		(output_crop[2] != input_crop[2]) || (output_crop[3] != input_crop[3])) {
		mwarn("GRP%d & GRP%d is incoincidence((%d,%d,%d,%d) != (%d,%d,%d,%d))",
			prev, prev->id, next->id,
			output_crop[0], output_crop[1], output_crop[2], output_crop[3],
			input_crop[0], input_crop[1], input_crop[2], input_crop[3]);
		input_crop[0] = output_crop[0];
		input_crop[1] = output_crop[1];
		input_crop[2] = output_crop[2];
		input_crop[3] = output_crop[3];
	}

p_err:
	return ret;
}

static int fimc_is_gframe_trans_grp_to_fre(struct fimc_is_group *prev,
	struct fimc_is_group_framemgr *next,
	struct fimc_is_group_frame *item)
{
	int ret = 0;

	BUG_ON(!prev);
	BUG_ON(!next);
	BUG_ON(!item);

	if (!prev->frame_group_cnt) {
		err("frame_group_cnt is zero");
		ret = -EFAULT;
		goto exit;
	}

	list_del(&item->list);
	prev->frame_group_cnt--;

	fimc_is_gframe_s_free(next, item);

exit:
	return ret;
}

int fimc_is_gframe_cancel(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 target_fcount)
{
	int ret = -EINVAL;
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe, *temp;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);

	gframemgr = &groupmgr->framemgr[group->instance];

	spin_lock_irq(&gframemgr->frame_slock);

	list_for_each_entry_safe(gframe, temp, &group->frame_group_head, list) {
		if (gframe->fcount == target_fcount) {
			list_del(&gframe->list);
			group->frame_group_cnt--;
			mwarn("gframe%d is cancelled", group, target_fcount);
			fimc_is_gframe_s_free(gframemgr, gframe);
			ret = 0;
			break;
		}
	}

	spin_unlock_irq(&gframemgr->frame_slock);

	return ret;
}

void * fimc_is_gframe_rewind(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 target_fcount)
{
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe, *temp;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);

	gframemgr = &groupmgr->framemgr[group->instance];

	list_for_each_entry_safe(gframe, temp, &group->frame_group_head, list) {
		if (gframe->fcount == target_fcount)
			break;

		if (gframe->fcount > target_fcount) {
			mwarn("target fcount is invalid(%d > %d)", group,
				gframe->fcount, target_fcount);
			gframe = NULL;
			break;
		}

		list_del(&gframe->list);
		group->frame_group_cnt--;
		mwarn("gframe%d is cancelled(count : %d)", group,
			gframe->fcount, group->frame_group_cnt);
		fimc_is_gframe_s_free(gframemgr, gframe);
	}

	if (!group->frame_group_cnt) {
		merr("gframe%d can't be found", group, target_fcount);
		gframe = NULL;
	}

	return gframe;
}

int fimc_is_gframe_flush(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group)
{
	int ret = 0;
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe, *temp;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);

	gframemgr = &groupmgr->framemgr[group->instance];

	spin_lock_irq(&gframemgr->frame_slock);

	list_for_each_entry_safe(gframe, temp, &group->frame_group_head, list) {
		list_del(&gframe->list);
		group->frame_group_cnt--;
		mwarn("gframe%d is flushed(count : %d)", group,
			gframe->fcount, group->frame_group_cnt);
		fimc_is_gframe_s_free(gframemgr, gframe);
	}

	spin_unlock_irq(&gframemgr->frame_slock);

	return ret;
}

static void fimc_is_group_3a0_cancel(struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct fimc_is_video_ctx *vctx,
	u32 instance)
{
	BUG_ON(!vctx);
	BUG_ON(!framemgr);
	BUG_ON(!frame);
	BUG_ON(!queue);

	pr_err("[3A0:D:%d:%d] GRP0 CANCEL(%d, %d)\n", instance,
		V4L2_TYPE_IS_OUTPUT(queue->vbq->type),
		frame->fcount, frame->index);

	fimc_is_frame_trans_req_to_com(framemgr, frame);
	queue_done(vctx, queue, frame->index, VB2_BUF_STATE_ERROR);
}

static void fimc_is_group_3a1_cancel(struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct fimc_is_video_ctx *vctx,
	u32 instance)
{
	BUG_ON(!vctx);
	BUG_ON(!framemgr);
	BUG_ON(!frame);
	BUG_ON(!queue);

	pr_err("[3A1:D:%d:%d] GRP1 CANCEL(%d, %d)\n", instance,
		V4L2_TYPE_IS_OUTPUT(queue->vbq->type),
		frame->fcount, frame->index);

	fimc_is_frame_trans_req_to_com(framemgr, frame);
	queue_done(vctx, queue, frame->index, VB2_BUF_STATE_ERROR);
}

static void fimc_is_group_isp_cancel(struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 instance)
{
	struct fimc_is_queue *queue;

	BUG_ON(!framemgr);
	BUG_ON(!frame);

	pr_err("[ISP:D:%d] GRP2 CANCEL(%d, %d)\n", instance,
		frame->fcount, frame->index);

	queue = GET_SRC_QUEUE(vctx);

	fimc_is_frame_trans_req_to_com(framemgr, frame);
	queue_done(vctx, queue, frame->index, VB2_BUF_STATE_ERROR);
}

static void fimc_is_group_dis_cancel(struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 instance)
{
	struct fimc_is_queue *queue;

	BUG_ON(!framemgr);
	BUG_ON(!frame);

	pr_err("[DIS:D:%d] GRP3 CANCEL(%d, %d)\n", instance,
		frame->fcount, frame->index);

	queue = GET_SRC_QUEUE(vctx);

	fimc_is_frame_trans_req_to_com(framemgr, frame);
	queue_done(vctx, queue, frame->index, VB2_BUF_STATE_ERROR);
}

static void fimc_is_group_cancel(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	unsigned long flags;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_framemgr *ldr_framemgr;
	/* for M2M device */
	struct fimc_is_framemgr *sub_framemgr = NULL;
	struct fimc_is_queue *ldr_queue, *sub_queue;
	struct fimc_is_frame *sub_frame;

	BUG_ON(!group);
	BUG_ON(!ldr_frame);

	vctx = group->leader.vctx;
	if (!vctx) {
		merr("vctx is NULL, critical error", group);
		return;
	}

	ldr_framemgr = GET_SRC_FRAMEMGR(vctx);
	if (!ldr_framemgr) {
		merr("ldr_framemgr is NULL, critical error", group);
		return;
	}

	framemgr_e_barrier_irqs(ldr_framemgr, FMGR_IDX_21, flags);

	switch (group->id) {
	case GROUP_ID_3A0:
		ldr_queue = GET_SRC_QUEUE(vctx);
		fimc_is_group_3a0_cancel(ldr_framemgr, ldr_frame,
			ldr_queue, vctx, group->instance);
		/* for M2M device */
		sub_framemgr = GET_DST_FRAMEMGR(vctx);
		break;
	case GROUP_ID_3A1:
		ldr_queue = GET_SRC_QUEUE(vctx);
		fimc_is_group_3a1_cancel(ldr_framemgr, ldr_frame,
			ldr_queue, vctx, group->instance);
		/* for M2M device */
		sub_framemgr = GET_DST_FRAMEMGR(vctx);
		break;
	case GROUP_ID_ISP:
		fimc_is_group_isp_cancel(ldr_framemgr, ldr_frame,
			vctx, group->instance);
		break;
	case GROUP_ID_DIS:
		fimc_is_group_dis_cancel(ldr_framemgr, ldr_frame,
			vctx, group->instance);
		break;
	default:
		err("unresolved group id %d", group->id);
		break;
	}

	framemgr_x_barrier_irqr(ldr_framemgr, FMGR_IDX_21, flags);

	if (sub_framemgr) {
		framemgr_e_barrier_irqs(sub_framemgr, FMGR_IDX_22, flags);

		switch (group->id) {
			case GROUP_ID_3A0:
				sub_queue = GET_DST_QUEUE(vctx);
				fimc_is_frame_request_head(sub_framemgr, &sub_frame);
				if (sub_frame)
					fimc_is_group_3a0_cancel(sub_framemgr, sub_frame,
							sub_queue, vctx, group->instance);
				break;
			case GROUP_ID_3A1:
				sub_queue = GET_DST_QUEUE(vctx);
				fimc_is_frame_request_head(sub_framemgr, &sub_frame);
				if (sub_frame)
					fimc_is_group_3a1_cancel(sub_framemgr, sub_frame,
							sub_queue, vctx, group->instance);
				break;
			default:
				err("unresolved group id %d", group->id);
				break;
		}

		framemgr_x_barrier_irqr(sub_framemgr, FMGR_IDX_22, flags);
	}
}
#ifdef CONFIG_USE_VENDER_FEATURE
/* Flash Mode Control */
#ifdef CONFIG_LEDS_LM3560
extern int lm3560_reg_update_export(u8 reg, u8 mask, u8 data);
#endif
#ifdef CONFIG_LEDS_SKY81296
extern int sky81296_torch_ctrl(int state);
#endif

static void fimc_is_group_set_torch(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	if (group->prev)
		return;

	if (group->aeflashMode != ldr_frame->shot->ctl.aa.aeflashMode) {
		group->aeflashMode = ldr_frame->shot->ctl.aa.aeflashMode;
		switch (group->aeflashMode) {
		case AA_FLASHMODE_ON_ALWAYS: /*TORCH mode*/
#ifdef CONFIG_LEDS_LM3560
			lm3560_reg_update_export(0xE0, 0xFF, 0xEF);
#elif defined(CONFIG_LEDS_SKY81296)
			sky81296_torch_ctrl(1);
#endif
			break;
		case AA_FLASHMODE_START: /*Pre flash mode*/
#ifdef CONFIG_LEDS_LM3560
			lm3560_reg_update_export(0xE0, 0xFF, 0xEF);
#elif defined(CONFIG_LEDS_SKY81296)
			sky81296_torch_ctrl(1);
#endif
			break;
		case AA_FLASHMODE_CAPTURE: /*Main flash mode*/
			break;
		case AA_FLASHMODE_OFF: /*OFF mode*/
#ifdef CONFIG_LEDS_SKY81296
			sky81296_torch_ctrl(0);
#endif
			break;
		default:
			break;
		}
	}
	return;
}
#endif

#ifdef DEBUG_AA
static void fimc_is_group_debug_aa_shot(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef DEBUG_FLASH
	if (ldr_frame->shot->ctl.aa.aeflashMode != group->flashmode) {
		group->flashmode = ldr_frame->shot->ctl.aa.aeflashMode;
		pr_info("flash ctl : %d(%d)\n", group->flashmode, ldr_frame->fcount);
	}
#endif
}

static void fimc_is_group_debug_aa_done(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef DEBUG_FLASH
	if (ldr_frame->shot->dm.flash.firingStable != group->flash.firingStable) {
		group->flash.firingStable = ldr_frame->shot->dm.flash.firingStable;
		pr_info("flash stable : %d(%d)\n", group->flash.firingStable, ldr_frame->fcount);
	}

	if (ldr_frame->shot->dm.flash.flashReady!= group->flash.flashReady) {
		group->flash.flashReady = ldr_frame->shot->dm.flash.flashReady;
		pr_info("flash ready : %d(%d)\n", group->flash.flashReady, ldr_frame->fcount);
	}

	if (ldr_frame->shot->dm.flash.flashOffReady!= group->flash.flashOffReady) {
		group->flash.flashOffReady = ldr_frame->shot->dm.flash.flashOffReady;
		pr_info("flash off : %d(%d)\n", group->flash.flashOffReady, ldr_frame->fcount);
	}
#endif
}
#endif

static void fimc_is_group_start_trigger(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame)
{
	BUG_ON(!group);
	BUG_ON(!frame);

	atomic_inc(&group->rcount);
	queue_kthread_work(group->worker, &frame->work);
}

static void fimc_is_group_pump(struct kthread_work *work)
{
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_frame *frame;

	frame = container_of(work, struct fimc_is_frame, work);
	groupmgr = frame->work_data1;
	group = frame->work_data2;

	fimc_is_group_start(groupmgr, group, frame);
}

int fimc_is_groupmgr_probe(struct fimc_is_groupmgr *groupmgr)
{
	int ret = 0;
	u32 i, j;

	for (i = 0; i < FIMC_IS_MAX_NODES; ++i) {
		spin_lock_init(&groupmgr->framemgr[i].frame_slock);
		INIT_LIST_HEAD(&groupmgr->framemgr[i].frame_free_head);
		groupmgr->framemgr[i].frame_free_cnt = 0;

		for (j = 0; j < FIMC_IS_MAX_GFRAME; ++j) {
			groupmgr->framemgr[i].frame[j].fcount = 0;
			fimc_is_gframe_s_free(&groupmgr->framemgr[i],
				&groupmgr->framemgr[i].frame[j]);
		}

		for (j = 0; j < GROUP_ID_MAX; ++j)
			groupmgr->group[i][j] = NULL;
	}

	for (i = 0; i < GROUP_ID_MAX; ++i) {
		atomic_set(&groupmgr->group_refcount[i], 0);
		clear_bit(FIMC_IS_GGROUP_START, &groupmgr->group_state[i]);
		clear_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[i]);
		clear_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[i]);
	}

	return ret;
}

int fimc_is_group_probe(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	u32 entry)
{
	int ret = 0;

	BUG_ON(!groupmgr);
	BUG_ON(!group);

	group->id = GROUP_ID_INVALID;
	group->leader.entry = entry;

	clear_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);
	clear_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(FIMC_IS_GROUP_READY, &group->state);
	clear_bit(FIMC_IS_GROUP_RUN, &group->state);
	clear_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(FIMC_IS_GROUP_INIT, &group->state);

	return ret;
}

int fimc_is_group_open(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 id, u32 instance,
	struct fimc_is_video_ctx *vctx,
	struct fimc_is_device_ischain *device,
	fimc_is_start_callback start_callback)
{
	int ret = 0, i;
	char name[30];
	struct fimc_is_core *core;
	struct fimc_is_subdev *leader;
	struct fimc_is_framemgr *framemgr;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!device);
	BUG_ON(!vctx);
	BUG_ON(instance >= FIMC_IS_MAX_NODES);
	BUG_ON(id >= GROUP_ID_MAX);

	leader = &group->leader;
	framemgr = GET_SRC_FRAMEMGR(vctx);
	core = (struct fimc_is_core *)device->interface->core;

	mdbgd_ischain("%s(id %d)\n", device, __func__, id);

	if (test_bit(FIMC_IS_GROUP_OPEN, &group->state)) {
		merr("group%d already open", device, id);
		ret = -EMFILE;
		goto p_err;
	}

	/* 1. start kthread */
	if (!test_bit(FIMC_IS_GGROUP_START, &groupmgr->group_state[id])) {
		init_kthread_worker(&groupmgr->group_worker[id]);
		snprintf(name, sizeof(name), "fimc_is_gworker%d", id);
		groupmgr->group_task[id] = kthread_run(kthread_worker_fn,
			&groupmgr->group_worker[id], name);
		if (IS_ERR(groupmgr->group_task[id])) {
			err("failed to create group_task%d\n", id);
			ret = -ENOMEM;
			goto p_err;
		}

		ret = sched_setscheduler_nocheck(groupmgr->group_task[id], SCHED_FIFO, &param);
		if (ret) {
			merr("sched_setscheduler_nocheck is fail(%d)", group, ret);
			goto p_err;
		}

		set_bit(FIMC_IS_GGROUP_START, &groupmgr->group_state[id]);
	}

	group->worker = &groupmgr->group_worker[id];
	for (i = 0; i < FRAMEMGR_MAX_REQUEST; ++i)
		init_kthread_work(&framemgr->frame[i].work, fimc_is_group_pump);

	/* 2. Init Group */
	clear_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);
	clear_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(FIMC_IS_GROUP_READY, &group->state);
	clear_bit(FIMC_IS_GROUP_RUN, &group->state);
	clear_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(FIMC_IS_GROUP_INIT, &group->state);

	group->start_callback = start_callback;
	group->device = device;
	group->id = id;
	group->instance = instance;
	group->source_vid = 0;
	group->fcount = 0;
	group->pcount = 0;
	group->aeflashMode = 0; /* Flash Mode Control */
	atomic_set(&group->scount, 0);
	atomic_set(&group->rcount, 0);
	atomic_set(&group->backup_fcount, 0);
	atomic_set(&group->sensor_fcount, 1);
	sema_init(&group->smp_trigger, 0);

	INIT_LIST_HEAD(&group->frame_group_head);
	group->frame_group_cnt = 0;

#ifdef MEASURE_TIME
#ifdef INTERNAL_TIME
	measure_init(&group->time, group->instance, group->id, 66);
#endif
#endif

	/* 3. Subdev Init */
	mutex_init(&leader->mutex_state);
	leader->vctx = vctx;
	leader->group = group;
	leader->leader = NULL;
	leader->input.width = 0;
	leader->input.height = 0;
	leader->output.width = 0;
	leader->output.height = 0;
	clear_bit(FIMC_IS_SUBDEV_START, &leader->state);
	set_bit(FIMC_IS_SUBDEV_OPEN, &leader->state);

	/* 4. Configure Group & Subdev List */
	switch (id) {
	case GROUP_ID_3A0:
	case GROUP_ID_3A1:
		/* path configuration */
		group->prev = NULL;
		if (GET_FIMC_IS_NUM_OF_SUBIP(core, isp)) {
			group->next = &device->group_isp;
			/* HACK */
			group->next->prev = group;
		} else {
			group->next = NULL;
		}
		group->subdev[ENTRY_SCALERC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_TDNR] = NULL;
		group->subdev[ENTRY_SCALERP] = NULL;
		group->subdev[ENTRY_LHFD] = NULL;
		group->subdev[ENTRY_3AAC] = &device->taac;
		group->subdev[ENTRY_3AAP] = &device->taap;

		device->taac.leader = leader;
		device->taap.leader = leader;

		device->taac.group = group;
		device->taap.group = group;

		set_bit(FIMC_IS_GROUP_ACTIVE, &group->state);
		break;
	case GROUP_ID_ISP:
		/* path configuration */
		group->prev = NULL;
		group->next = NULL;
		if (GET_FIMC_IS_NUM_OF_SUBIP(core, scc))
			group->subdev[ENTRY_SCALERC] = &device->scc;
		else
			group->subdev[ENTRY_SCALERC] = NULL;
		/* dis is not included to any group initially */
		group->subdev[ENTRY_DIS] = NULL;
		if (GET_FIMC_IS_NUM_OF_SUBIP(core, dnr))
			group->subdev[ENTRY_TDNR] = &device->dnr;
		else
			group->subdev[ENTRY_TDNR] = NULL;

		if (GET_FIMC_IS_NUM_OF_SUBIP(core, scp))
			group->subdev[ENTRY_SCALERP] = &device->scp;
		else
			group->subdev[ENTRY_SCALERP] = NULL;

		if (GET_FIMC_IS_NUM_OF_SUBIP(core, fd))
			group->subdev[ENTRY_LHFD] = &device->fd;
		else
			group->subdev[ENTRY_LHFD] = NULL;
		group->subdev[ENTRY_3AAC] = NULL;
		group->subdev[ENTRY_3AAP] = NULL;

		device->scc.leader = leader;
		device->dis.leader = leader;
		device->dnr.leader = leader;
		device->scp.leader = leader;
		device->fd.leader = leader;

		device->scc.group = group;
		device->dis.group = group;
		device->dnr.group = group;
		device->scp.group = group;
		device->fd.group = group;

		set_bit(FIMC_IS_GROUP_ACTIVE, &group->state);
		break;
	case GROUP_ID_DIS:
		/* path configuration */
		group->prev = NULL;
		group->next = NULL;
		group->subdev[ENTRY_SCALERC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_TDNR] = NULL;
		group->subdev[ENTRY_SCALERP] = NULL;
		group->subdev[ENTRY_LHFD] = NULL;
		group->subdev[ENTRY_3AAC] = NULL;
		group->subdev[ENTRY_3AAP] = NULL;

		clear_bit(FIMC_IS_GROUP_ACTIVE, &group->state);
		break;
	default:
		merr("group id(%d) is invalid", vctx, id);
		ret = -EINVAL;
		goto p_err;
	}

	/* 5. Update Group Manager */
	groupmgr->group[instance][id] = group;
	atomic_inc(&groupmgr->group_refcount[id]);
	set_bit(FIMC_IS_GROUP_OPEN, &group->state);

p_err:
	mdbgd_group("%s(%d)\n", group, __func__, ret);
	return ret;
}

int fimc_is_group_close(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group)
{
	int ret = 0;
	u32 refcount, i;
	struct fimc_is_group_framemgr *gframemgr;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(group->id >= GROUP_ID_MAX);

	refcount = atomic_read(&groupmgr->group_refcount[group->id]);

	if (!test_bit(FIMC_IS_GROUP_OPEN, &group->state)) {
		merr("group%d already close", group, group->id);
		ret = -EMFILE;
		goto p_err;
	}

	/*
	 * Maybe there are some waiting smp_shot semaphores when finishing kthread
	 * in group close. This situation caused waiting kthread_stop to finish it
	 * We should check if there are smp_shot in waiting list.
	 */
	if (test_bit(FIMC_IS_GROUP_INIT, &group->state)) {
		while (!list_empty(&group->smp_shot.wait_list)) {
			warn("group%d frame reqs are waiting in semaphore[%d] when closing",
					group->id, group->smp_shot.count);
			up(&group->smp_shot);
		}
	}

	if ((refcount == 1) &&
		test_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[group->id]) &&
		groupmgr->group_task[group->id]) {

		set_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[group->id]);

		/* flush semaphore shot */
		atomic_inc(&group->smp_shot_count);

		/* flush semaphore trigger */
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
			up(&group->smp_trigger);

		/*
		 * flush kthread wait until all work is complete
		 * it's dangerous if all is not finished
		 * so it's commented currently
		 * flush_kthread_worker(&groupmgr->group_worker[group->id]);
		 */
		kthread_stop(groupmgr->group_task[group->id]);

		clear_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[group->id]);
		clear_bit(FIMC_IS_GGROUP_START, &groupmgr->group_state[group->id]);
		clear_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[group->id]);
	}

	groupmgr->group[group->instance][group->id] = NULL;
	atomic_dec(&groupmgr->group_refcount[group->id]);

	/* all group is closed */
	if (!groupmgr->group[group->instance][GROUP_ID_3A0] &&
		!groupmgr->group[group->instance][GROUP_ID_3A1] &&
		!groupmgr->group[group->instance][GROUP_ID_ISP] &&
		!groupmgr->group[group->instance][GROUP_ID_DIS]) {
		gframemgr = &groupmgr->framemgr[group->instance];
		if (gframemgr->frame_free_cnt != FIMC_IS_MAX_GFRAME) {
			mwarn("gframemgr free count is invalid(%d)", group,
				gframemgr->frame_free_cnt);
			INIT_LIST_HEAD(&gframemgr->frame_free_head);
			gframemgr->frame_free_cnt = 0;
			for (i = 0; i < FIMC_IS_MAX_GFRAME; ++i) {
				gframemgr->frame[i].fcount = 0;
				fimc_is_gframe_s_free(gframemgr,
					&gframemgr->frame[i]);
			}
		}
	}

	group->id = GROUP_ID_INVALID;
	clear_bit(FIMC_IS_GROUP_INIT, &group->state);
	clear_bit(FIMC_IS_GROUP_OPEN, &group->state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &group->leader.state);

p_err:
	mdbgd_group("%s(ref %d, %d)", group, __func__, (refcount - 1), ret);
	return ret;
}

int fimc_is_group_init(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	bool otf_input,
	u32 video_id)
{
	int ret = 0;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(group->id >= GROUP_ID_MAX);

	if (test_bit(FIMC_IS_GROUP_INIT, &group->state)) {
		merr("already initialized", group);
		/* HACK */
		/* ret = -EINVAL; */
		goto p_err;
	}

	group->source_vid = video_id;

	if (!test_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[group->id])) {
		if (otf_input)
			sema_init(&groupmgr->group_smp_res[group->id], MIN_OF_SHOT_RSC);
		else
			sema_init(&groupmgr->group_smp_res[group->id], 1);

		set_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[group->id]);
	}

	if (otf_input) {
		sema_init(&group->smp_shot, MIN_OF_SHOT_RSC);
		atomic_set(&group->smp_shot_count, MIN_OF_SHOT_RSC);
		set_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state);
		group->async_shots = MIN_OF_ASYNC_SHOTS;
		group->sync_shots = MIN_OF_SYNC_SHOTS;
	} else {
		sema_init(&group->smp_shot, 1);
		atomic_set(&group->smp_shot_count, 1);
		clear_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state);
		group->async_shots = 0;
		group->sync_shots = 1;
	}

	set_bit(FIMC_IS_GROUP_INIT, &group->state);

p_err:
	mdbgd_group("%s(otf : %d, %d)\n", group, __func__, otf_input, ret);
	return ret;
}

int fimc_is_group_change(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group_isp,
	struct fimc_is_frame *frame)
{
	int ret = 0;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader_isp, *leader_dis;
	struct fimc_is_group *group_dis;

	BUG_ON(!groupmgr);
	BUG_ON(!group_isp);
	BUG_ON(!group_isp->device);
	BUG_ON(!frame);

	device = group_isp->device;
	group_dis = &device->group_dis;
	leader_dis = &group_dis->leader;
	leader_isp = &group_isp->leader;

	if (frame->shot->ctl.aa.videoStabilizationMode) {
		if (!test_bit(FIMC_IS_GROUP_READY, &group_dis->state)) {
			merr("DIS group is not ready", group_dis);
			frame->shot->dm.aa.videoStabilizationMode = 0;
			ret = -EINVAL;
			goto p_err;
		}

		if (!test_bit(FIMC_IS_GROUP_ACTIVE, &group_dis->state)) {
			pr_info("[GRP%d] Change On\n", group_isp->id);

			/* HACK */
			sema_init(&group_dis->smp_trigger, 0);

			group_isp->prev = &device->group_3aa;
			group_isp->next = group_dis;
			group_isp->subdev[ENTRY_SCALERC] = &device->scc;
			group_isp->subdev[ENTRY_DIS] = &device->dis;
			group_isp->subdev[ENTRY_TDNR] = NULL;
			group_isp->subdev[ENTRY_SCALERP] = NULL;
			group_isp->subdev[ENTRY_LHFD] = NULL;

			group_dis->next = NULL;
			group_dis->prev = group_isp;
			group_dis->subdev[ENTRY_SCALERC] = NULL;
			group_dis->subdev[ENTRY_DIS] = NULL;
			group_dis->subdev[ENTRY_TDNR] = &device->dnr;
			group_dis->subdev[ENTRY_SCALERP] = &device->scp;
			group_dis->subdev[ENTRY_LHFD] = &device->fd;

			device->scc.leader = leader_isp;
			device->dis.leader = leader_isp;
			device->dnr.leader = leader_dis;
			device->scp.leader = leader_dis;
			device->fd.leader = leader_dis;

			device->scc.group = group_isp;
			device->dis.group = group_isp;
			device->dnr.group = group_dis;
			device->scp.group = group_dis;
			device->fd.group = group_dis;

			set_bit(FIMC_IS_GROUP_ACTIVE, &group_dis->state);
		}

		frame->shot->dm.aa.videoStabilizationMode = 1;
	} else {
		if (test_bit(FIMC_IS_GROUP_ACTIVE, &group_dis->state)) {
			pr_info("[GRP%d] Change Off\n", group_isp->id);
			group_isp->prev = &device->group_3aa;
			group_isp->next = NULL;
			group_isp->subdev[ENTRY_SCALERC] = &device->scc;
			group_isp->subdev[ENTRY_DIS] = &device->dis;
			group_isp->subdev[ENTRY_TDNR] = &device->dnr;
			group_isp->subdev[ENTRY_SCALERP] = &device->scp;
			group_isp->subdev[ENTRY_LHFD] = &device->fd;

			group_dis->next = NULL;
			group_dis->prev = NULL;
			group_dis->subdev[ENTRY_SCALERC] = NULL;
			group_dis->subdev[ENTRY_DIS] = NULL;
			group_dis->subdev[ENTRY_TDNR] = NULL;
			group_dis->subdev[ENTRY_SCALERP] = NULL;
			group_dis->subdev[ENTRY_LHFD] = NULL;

			device->scc.leader = leader_isp;
			device->dis.leader = leader_isp;
			device->dnr.leader = leader_isp;
			device->scp.leader = leader_isp;
			device->fd.leader = leader_isp;

			device->scc.group = group_isp;
			device->dis.group = group_isp;
			device->dnr.group = group_isp;
			device->scp.group = group_isp;
			device->fd.group = group_isp;

			clear_bit(FIMC_IS_GROUP_ACTIVE, &group_dis->state);
		}

		frame->shot->dm.aa.videoStabilizationMode = 0;
	}

p_err:
	return ret;
}

int fimc_is_group_process_start(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_sensor *sensor = NULL;
	struct fimc_is_framemgr *framemgr = NULL;
	u32 shot_resource = 1;
	u32 sensor_fcount;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->device);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(!queue);
	BUG_ON(!test_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[group->id]));
	BUG_ON(!test_bit(FIMC_IS_GROUP_INIT, &group->state));

	if (test_bit(FIMC_IS_GROUP_READY, &group->state)) {
		warn("already group start");
		goto p_err;
	}

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		framemgr = &queue->framemgr;
		if (!framemgr) {
			err("framemgr is null\n");
			ret = -EINVAL;
			goto p_err;
		}

		sensor = group->device->sensor;
		if (!sensor) {
			err("sensor is null\n");
			ret = -EINVAL;
			goto p_err;
		}

		/* async & sync shots */
		if (fimc_is_sensor_g_framerate(sensor) > 30)
			group->async_shots = max_t(int, MIN_OF_ASYNC_SHOTS,
					DIV_ROUND_UP(framemgr->frame_cnt, 3));
		else
			group->async_shots = MIN_OF_ASYNC_SHOTS;
		group->sync_shots = max_t(int, MIN_OF_SYNC_SHOTS,
				framemgr->frame_cnt - group->async_shots);

		/* shot resource */
		shot_resource = group->async_shots + MIN_OF_SYNC_SHOTS;
		sema_init(&groupmgr->group_smp_res[group->id], shot_resource);

		/* frame count */
		sensor_fcount = fimc_is_sensor_g_fcount(sensor) + 1;
		atomic_set(&group->sensor_fcount, sensor_fcount);
		atomic_set(&group->backup_fcount, sensor_fcount - 1);
		group->fcount = sensor_fcount - 1;

		info("[OTF] framerate: %d, async shots: %d, shot resource: %d\n",
				fimc_is_sensor_g_framerate(sensor),
				group->async_shots,
				shot_resource);

		memset(&group->intent_ctl, 0, sizeof(struct camera2_ctl));
	}

	sema_init(&group->smp_shot, shot_resource);
	atomic_set(&group->smp_shot_count, shot_resource);

	atomic_set(&group->scount, 0);
	atomic_set(&group->rcount, 0);
	sema_init(&group->smp_trigger, 0);

	set_bit(FIMC_IS_GROUP_READY, &group->state);

p_err:
	return ret;
}

int fimc_is_group_process_stop(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	int retry;
	u32 rcount;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_device_ischain *device;
	struct fimc_is_device_sensor *sensor;
	u32 group_id;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(!queue);

	if (!test_bit(FIMC_IS_GROUP_READY, &group->state)) {
		warn("already group stop");
		goto p_err;
	}

	device = group->device;
	if (!device) {
		merr("device is NULL", group);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state)) {
		warn("alredy close sensor was called");
		goto p_err;
	}

	sensor = device->sensor;
	framemgr = &queue->framemgr;

	if (test_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state)) {
		set_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
		clear_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);

		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state) &&
			!list_empty(&group->smp_trigger.wait_list)) {
			if (!sensor) {
				warn("sensor is NULL, forcely trigger");
				up(&group->smp_trigger);
				goto check_completion;
			}

			if (!test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state)) {
				warn("sensor is closed, forcely trigger");
				up(&group->smp_trigger);
				goto check_completion;
			}

			if (!test_bit(FIMC_IS_SENSOR_FRONT_START, &sensor->state)) {
				warn("front sensor is stopped, forcely trigger");
				up(&group->smp_trigger);
				goto check_completion;
			}

			if (!test_bit(FIMC_IS_SENSOR_BACK_START, &sensor->state)) {
				warn("back sensor is stopped, forcely trigger");
				up(&group->smp_trigger);
				goto check_completion;
			}
		}
	}

check_completion:
	retry = 150;
	while (--retry && framemgr->frame_req_cnt) {
		warn("%d frame reqs waiting...\n", framemgr->frame_req_cnt);
		msleep(20);
	}

	if (!retry) {
		merr("waiting(until request empty) is fail", group);
		ret = -EINVAL;
	}

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		retry = 10;
		while (--retry && framemgr->frame_pro_cnt) {
			warn("%d frame pros waiting...\n", framemgr->frame_pro_cnt);
			msleep(20);
		}

		if (!retry) {
			merr("waiting(until process empty) is fail", group);
			ret = -EINVAL;
		}
	}

	if (test_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state)) {
		ret = fimc_is_itf_force_stop(device, GROUP_ID(group->id));
		if (ret) {
			merr("fimc_is_itf_force_stop is fail", group);
			ret = -EINVAL;
		}
	} else {
		/* if there's only one group of isp, send group id by 3a0 */
		if ((group->id == GROUP_ID_ISP) &&
				GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
				GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
			group_id = GROUP_ID(GROUP_ID_3A0);
		else
			group_id = GROUP_ID(group->id);

		ret = fimc_is_itf_process_stop(device, group_id);
		if (ret) {
			merr("fimc_is_itf_process_stop is fail", group);
			ret = -EINVAL;
		}
	}

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		retry = 10;
		while (--retry && framemgr->frame_pro_cnt) {
			warn("%d frame pros waiting...\n", framemgr->frame_pro_cnt);
			msleep(20);
		}

		if (!retry) {
			merr("waiting(until process empty) is fail", group);
			ret = -EINVAL;
		}
	}

	rcount = atomic_read(&group->rcount);
	if (rcount) {
		merr("rcount is not empty(%d)", group, rcount);
		ret = -EINVAL;
	}

	fimc_is_gframe_flush(groupmgr, group);

	clear_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(FIMC_IS_GROUP_READY, &group->state);

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		pr_info("[OTF] sensor fcount: %d, fcount: %d\n",
			atomic_read(&group->sensor_fcount), group->fcount);

p_err:
	return ret;
}

int fimc_is_group_buffer_queue(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_queue *queue,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_ischain *device;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->device);
	BUG_ON(!group->device->sensor);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(!queue);
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	device = group->device;
	sensor = device->sensor;
	framemgr = &queue->framemgr;

	/* 1. check frame validation */
	frame = &framemgr->frame[index];
	if (!frame) {
		err("frame is null\n");
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!test_bit(FRAME_INI_MEM, &frame->memory))) {
		err("frame %d is NOT init", index);
		ret = EINVAL;
		goto p_err;
	}

#ifdef MEASURE_TIME
#ifdef INTERNAL_TIME
	do_gettimeofday(&frame->time_queued);
#endif
#endif

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_23, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_FREE) {
		if (frame->req_flag) {
			merr("req_flag of buffer%d is not clear(%08X)",
				group, frame->index, (u32)frame->req_flag);
			frame->req_flag = 0;
		}

		if (test_bit(OUT_SCC_FRAME, &frame->out_flag)) {
			merr("scc output is not generated", group);
			clear_bit(OUT_SCC_FRAME, &frame->out_flag);
		}

		if (test_bit(OUT_DIS_FRAME, &frame->out_flag)) {
			merr("dis output is not generated", group);
			clear_bit(OUT_DIS_FRAME, &frame->out_flag);
		}

		if (test_bit(OUT_SCP_FRAME, &frame->out_flag)) {
			merr("scp output is not generated", group);
			clear_bit(OUT_SCP_FRAME, &frame->out_flag);
		}

		if (test_bit(OUT_3AAC_FRAME, &frame->out_flag)) {
			merr("3aac output is not generated", group);
			clear_bit(OUT_3AAC_FRAME, &frame->out_flag);
		}

		if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state) &&
			(framemgr->frame_req_cnt >= 3))
			mwarn("GROUP%d is working lately(%d)",
				group, group->id, framemgr->frame_req_cnt);

		frame->fcount = frame->shot->dm.request.frameCount;
		frame->rcount = frame->shot->ctl.request.frameCount;
		frame->work_data1 = groupmgr;
		frame->work_data2 = group;

#ifdef FIXED_FPS_DEBUG
		frame->shot_ext->shot.ctl.aa.aeTargetFpsRange[0] = FIXED_FPS_VALUE;
		frame->shot_ext->shot.ctl.aa.aeTargetFpsRange[1] = FIXED_FPS_VALUE;
		frame->shot_ext->shot.ctl.sensor.frameDuration = 1000000000/FIXED_FPS_VALUE;
#endif

#ifdef ENABLE_FAST_SHOT
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			memcpy(&group->fast_ctl.aa, &frame->shot->ctl.aa,
				sizeof(struct camera2_aa_ctl));
			memcpy(&group->fast_ctl.scaler, &frame->shot->ctl.scaler,
				sizeof(struct camera2_scaler_ctl));
		}
#endif

		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	} else {
		err("frame(%d) is invalid state(%d)\n", index, frame->state);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_23, flags);

	if (unlikely(!test_bit(FRAME_MAP_MEM, &frame->memory) &&
		!test_bit(FIMC_IS_SENSOR_FRONT_START, &sensor->state))) {
		fimc_is_itf_map(device, GROUP_ID(group->id), frame->dvaddr_shot, frame->shot_size);
		set_bit(FRAME_MAP_MEM, &frame->memory);
	}

	fimc_is_group_start_trigger(groupmgr, group, frame);

p_err:
	return ret;
}

int fimc_is_group_buffer_finish(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->leader.vctx);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	framemgr = GET_GROUP_FRAMEMGR(group);

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	fimc_is_frame_complete_head(framemgr, &frame);
	if (frame) {
		if (frame->index == index) {
			fimc_is_frame_trans_com_to_fre(framemgr, frame);

			frame->shot_ext->free_cnt = framemgr->frame_fre_cnt;
			frame->shot_ext->request_cnt = framemgr->frame_req_cnt;
			frame->shot_ext->process_cnt = framemgr->frame_pro_cnt;
			frame->shot_ext->complete_cnt = framemgr->frame_com_cnt;
		} else {
			merr("buffer index is NOT matched(G%d, %d != %d)",
				group, group->id, index, frame->index);
			fimc_is_frame_print_all(framemgr);
			ret = -EINVAL;
		}
	} else {
		merr("frame is empty from complete", group);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);

#ifdef MEASURE_TIME
#ifdef INTERNAL_TIME
	do_gettimeofday(&frame->time_dequeued);
	measure_time(&group->time,
		&frame->time_queued, &frame->time_shot,
		&frame->time_shotdone, &frame->time_dequeued);
#endif
#endif

	return ret;
}

int fimc_is_group_start(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	int ret = 0;
	struct fimc_is_device_ischain *device;
	struct fimc_is_resourcemgr *resourcemgr;
	struct fimc_is_group *group_next, *group_prev;
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe;
	int async_step = 0;
	bool try_sdown = false;
	bool try_rdown = false;
#ifdef ENABLE_DVFS
	int scenario_id;
#endif

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->leader.vctx);
	BUG_ON(!group->start_callback);
	BUG_ON(!group->device);
	BUG_ON(!ldr_frame);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(group->id >= GROUP_ID_MAX);

	atomic_dec(&group->rcount);

	if (test_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state)) {
		mwarn("g%dframe is cancelled(force stop)", group, group->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[group->id])) {
		merr("cancel by group stop #1", group);
		ret = -EINVAL;
		goto p_err;
	}

	PROGRAM_COUNT(1);
	ret = down_interruptible(&group->smp_shot);
	if (ret) {
		err("down is fail1(%d)", ret);
		goto p_err;
	}
	atomic_dec(&group->smp_shot_count);
	try_sdown = true;

	/* skip left operation, if group is closing */
	if (!test_bit(FIMC_IS_GROUP_READY, &group->state)) {
		mwarn("this group%d was already process stoped", group, group->id);
		ret = -EINVAL;
		goto p_err;
	}

	PROGRAM_COUNT(2);
	ret = down_interruptible(&groupmgr->group_smp_res[group->id]);
	if (ret) {
		err("down is fail2(%d)", ret);
		goto p_err;
	}
	try_rdown = true;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		if (atomic_read(&group->smp_shot_count) < MIN_OF_SYNC_SHOTS) {
			PROGRAM_COUNT(3);
			ret = down_interruptible(&group->smp_trigger);
			if (ret) {
				err("down is fail3(%d)", ret);
				goto p_err;
			}
		} else {
			/*
			 * backup fcount can not be bigger than sensor fcount
			 * otherwise, duplicated shot can be generated.
			 * this is problem can be caused by hal qbuf timing
			 */
			if (atomic_read(&group->backup_fcount) >=
				atomic_read(&group->sensor_fcount)) {
				PROGRAM_COUNT(4);
				ret = down_interruptible(&group->smp_trigger);
				if (ret) {
					err("down is fail4(%d)", ret);
					goto p_err;
				}
			} else {
				/*
				 * this statement is execued only at initial.
				 * automatic increase the frame count of sensor
				 * for next shot without real frame start
				 */

				/* it's a async shot time */
				async_step = 1;
			}
		}

		if (test_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[group->id])) {
			err("cancel by group stop #2");
			ret = -EINVAL;
			goto p_err;
		}

		ldr_frame->fcount = atomic_read(&group->sensor_fcount);
		atomic_set(&group->backup_fcount, ldr_frame->fcount);
		ldr_frame->shot->dm.request.frameCount = ldr_frame->fcount;
		ldr_frame->shot->dm.sensor.timeStamp = fimc_is_get_timestamp();

		/* real automatic increase */
		if (async_step && (atomic_read(&group->smp_shot_count) > MIN_OF_SYNC_SHOTS))
			atomic_inc(&group->sensor_fcount);
	}

	if (test_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[group->id])) {
		err("cancel by group stop #3");
		ret = -EINVAL;
		goto p_err;
	}

#ifdef ENABLE_VDIS
	if (group->id == GROUP_ID_ISP)
		fimc_is_group_change(groupmgr, group, ldr_frame);

	/* HACK */
	if ((group->id == GROUP_ID_DIS) &&
		test_bit(FIMC_IS_GROUP_ACTIVE, &group->state))
		down(&group->smp_trigger);
#endif

	PROGRAM_COUNT(5);
	device = group->device;
	resourcemgr = device->resourcemgr;
	group_next = group->next;
	group_prev = group->prev;
	gframemgr = &groupmgr->framemgr[group->instance];
	if (!gframemgr) {
		err("gframemgr is NULL(instance %d)", group->instance);
		ret = -EINVAL;
		goto p_err;
	}

	if (group_prev && !group_next) {
		/* tailer */
		spin_lock_irq(&gframemgr->frame_slock);
		fimc_is_gframe_group_head(group, &gframe);
		if (!gframe) {
			merr("g%dframe is NULL1", group, group->id);
			warn("GRP%d(res %d, rcnt %d), GRP2(res %d, rcnt %d)",
				device->group_3aa.id,
				groupmgr->group_smp_res[device->group_3aa.id].count,
				atomic_read(&device->group_3aa.rcount),
				groupmgr->group_smp_res[device->group_isp.id].count,
				atomic_read(&device->group_isp.rcount));
			fimc_is_gframe_print_group(group_prev);
			fimc_is_gframe_print_free(gframemgr);
			spin_unlock_irq(&gframemgr->frame_slock);
			ret = -EINVAL;
			goto p_err;
		}

		if (ldr_frame->fcount != gframe->fcount) {
			mwarn("grp%d shot mismatch(%d != %d)", group, group->id,
				ldr_frame->fcount, gframe->fcount);
			gframe = fimc_is_gframe_rewind(groupmgr, group,
				ldr_frame->fcount);
			if (!gframe) {
				spin_unlock_irq(&gframemgr->frame_slock);
				merr("rewinding is fail,can't recovery", group);
				goto p_err;
			}
		}

		fimc_is_gframe_s_info(gframe, group->id, ldr_frame);
		fimc_is_gframe_trans_grp_to_fre(group, gframemgr, gframe);
		spin_unlock_irq(&gframemgr->frame_slock);
	} else if (!group_prev && group_next) {
		/* leader */
		group->fcount++;
		spin_lock_irq(&gframemgr->frame_slock);
		fimc_is_gframe_free_head(gframemgr, &gframe);
		if (!gframe) {
			merr("g%dframe is NULL2", group, group->id);
			warn("GRP%d(res %d, rcnt %d), GRP2(res %d, rcnt %d)",
				device->group_3aa.id,
				groupmgr->group_smp_res[device->group_3aa.id].count,
				atomic_read(&device->group_3aa.rcount),
				groupmgr->group_smp_res[device->group_isp.id].count,
				atomic_read(&device->group_isp.rcount));
			fimc_is_gframe_print_free(gframemgr);
			fimc_is_gframe_print_group(group_next);
			spin_unlock_irq(&gframemgr->frame_slock);
			ret = -EINVAL;
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
			(ldr_frame->fcount != group->fcount)) {
			if (ldr_frame->fcount > group->fcount) {
				warn("grp%d shot mismatch(%d != %d)", group->id,
					ldr_frame->fcount, group->fcount);
				group->fcount = ldr_frame->fcount;
			} else {
				spin_unlock_irq(&gframemgr->frame_slock);
				err("grp%d shot mismatch(%d, %d)", group->id,
					ldr_frame->fcount, group->fcount);
				group->fcount--;
				ret = -EINVAL;
				goto p_err;
			}
		}

		gframe->fcount = ldr_frame->fcount;
		fimc_is_gframe_s_info(gframe, group->id, ldr_frame);
		fimc_is_gframe_trans_fre_to_grp(gframemgr, group_next, gframe);
		spin_unlock_irq(&gframemgr->frame_slock);
	} else if (group_prev && group_next) {
		spin_lock_irq(&gframemgr->frame_slock);
		fimc_is_gframe_group_head(group, &gframe);
		if (!gframe) {
			merr("g%dframe is NULL3", group, group->id);
			warn("GRP%d(res %d, rcnt %d), GRP2(res %d, rcnt %d)",
				device->group_3aa.id,
				groupmgr->group_smp_res[device->group_3aa.id].count,
				atomic_read(&device->group_3aa.rcount),
				groupmgr->group_smp_res[device->group_isp.id].count,
				atomic_read(&device->group_isp.rcount));
			fimc_is_gframe_print_group(group_prev);
			fimc_is_gframe_print_group(group_next);
			spin_unlock_irq(&gframemgr->frame_slock);
			ret = -EINVAL;
			goto p_err;
		}

		if (ldr_frame->fcount != gframe->fcount) {
			mwarn("grp%d shot mismatch(%d != %d)", group, group->id,
				ldr_frame->fcount, gframe->fcount);
			gframe = fimc_is_gframe_rewind(groupmgr, group,
				ldr_frame->fcount);
			if (!gframe) {
				spin_unlock_irq(&gframemgr->frame_slock);
				merr("rewinding is fail,can't recovery", group);
				goto p_err;
			}
		}

		fimc_is_gframe_s_info(gframe, group->id, ldr_frame);
		fimc_is_gframe_trans_grp_to_grp(group, group_next, gframe);
		spin_unlock_irq(&gframemgr->frame_slock);
	} else {
		/* single */
		group->fcount++;
		spin_lock_irq(&gframemgr->frame_slock);
		fimc_is_gframe_free_head(gframemgr, &gframe);
		if (!gframe) {
			merr("g%dframe is NULL4", group, group->id);
			warn("GRP%d(res %d, rcnt %d), GRP2(res %d, rcnt %d)",
				device->group_3aa.id,
				groupmgr->group_smp_res[device->group_3aa.id].count,
				atomic_read(&device->group_3aa.rcount),
				groupmgr->group_smp_res[device->group_isp.id].count,
				atomic_read(&device->group_isp.rcount));
			fimc_is_gframe_print_free(gframemgr);
			spin_unlock_irq(&gframemgr->frame_slock);
			ret = -EINVAL;
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
			(ldr_frame->fcount != group->fcount)) {
			if (ldr_frame->fcount > group->fcount) {
				warn("grp%d shot mismatch(%d != %d)", group->id,
					ldr_frame->fcount, group->fcount);
				group->fcount = ldr_frame->fcount;
			} else {
				spin_unlock_irq(&gframemgr->frame_slock);
				err("grp%d shot mismatch(%d, %d)", group->id,
					ldr_frame->fcount, group->fcount);
				group->fcount--;
				ret = -EINVAL;
				goto p_err;
			}
		}

		gframe->fcount = ldr_frame->fcount;
		spin_unlock_irq(&gframemgr->frame_slock);
	}

#ifdef DEBUG_AA
	fimc_is_group_debug_aa_shot(group, ldr_frame);
#endif
#ifdef CONFIG_USE_VENDER_FEATURE
	/* Flash Mode Control */
	fimc_is_group_set_torch(group, ldr_frame);
#endif

#ifdef ENABLE_DVFS
	mutex_lock(&resourcemgr->dvfs_ctrl.lock);
	if ((!pm_qos_request_active(&device->user_qos)) &&
			(sysfs_debug.en_dvfs)) {
		/* try to find dynamic scenario to apply */
		scenario_id = fimc_is_dvfs_sel_scenario(FIMC_IS_DYNAMIC_SN, device, ldr_frame);

		if (scenario_id > 0) {
			struct fimc_is_dvfs_scenario_ctrl *dynamic_ctrl = resourcemgr->dvfs_ctrl.dynamic_ctrl;
			info("GRP:%d tbl[%d] dynamic scenario(%d)-[%s]\n",
					group->id,
					resourcemgr->dvfs_ctrl.dvfs_table_idx,
					scenario_id,
					dynamic_ctrl->scenarios[dynamic_ctrl->cur_scenario_idx].scenario_nm);
			fimc_is_set_dvfs(device, scenario_id);
		}

		if ((scenario_id < 0) && (resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_frame_tick == 0)) {
			struct fimc_is_dvfs_scenario_ctrl *static_ctrl = resourcemgr->dvfs_ctrl.static_ctrl;
			info("GRP:%d tbl[%d] restore scenario(%d)-[%s]\n",
					group->id,
					resourcemgr->dvfs_ctrl.dvfs_table_idx,
					static_ctrl->cur_scenario_id,
					static_ctrl->scenarios[static_ctrl->cur_scenario_idx].scenario_nm);
			fimc_is_set_dvfs(device, static_ctrl->cur_scenario_id);
		}
	}
	mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
#endif

	PROGRAM_COUNT(6);
	ret = group->start_callback(group->device, ldr_frame);
	if (ret) {
		merr("start_callback is fail", group);
		fimc_is_group_cancel(group, ldr_frame);
		fimc_is_group_done(groupmgr, group, ldr_frame, VB2_BUF_STATE_ERROR);
	} else {
		atomic_inc(&group->scount);
	}

	PROGRAM_COUNT(7);
	return ret;

p_err:
	if (!test_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[group->id]))
		fimc_is_group_cancel(group, ldr_frame);

	if (try_sdown) {
		atomic_inc(&group->smp_shot_count);
		up(&group->smp_shot);
	}
	if (try_rdown)
		up(&groupmgr->group_smp_res[group->id]);

	return ret;
}

int fimc_is_group_done(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame,
	u32 done_state)
{
	int ret = 0;
	u32 resources;
	struct fimc_is_device_ischain *device;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!ldr_frame);
	BUG_ON(group->instance >= FIMC_IS_MAX_NODES);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(!group->device);

#ifdef ENABLE_VDIS
	/* current group notify to next group that shot done is arrvied */
	/* HACK */
	if (group->next && (group->id == GROUP_ID_ISP) &&
		test_bit(FIMC_IS_GROUP_ACTIVE, &group->next->state))
		up(&group->next->smp_trigger);
#endif

	/* check shot & resource count validation */
	resources = group->async_shots + group->sync_shots;

	if (group->smp_shot.count >= resources) {
		merr("G%d, shot count is invalid(%d >= %d+%d)", group,
			group->id, group->smp_shot.count, group->async_shots,
			group->sync_shots);
		atomic_set(&group->smp_shot_count, resources - 1);
		sema_init(&group->smp_shot, resources - 1);
	}

	if (groupmgr->group_smp_res[group->id].count >= resources) {
		merr("G%d, resource count is invalid(%d >=  %d+%d)", group,
			group->id, groupmgr->group_smp_res[group->id].count,
			group->async_shots, group->sync_shots);
		sema_init(&groupmgr->group_smp_res[group->id], resources - 1);
	}

	device = group->device;
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		(done_state != VB2_BUF_STATE_DONE)) {
		merr("GRP%d NOT DONE(reprocessing)\n", group, group->id);
		fimc_is_hw_logdump(device->interface);
	}

#ifdef DEBUG_AA
	fimc_is_group_debug_aa_done(group, ldr_frame);
#endif

	clear_bit(FIMC_IS_GROUP_RUN, &group->state);
	atomic_inc(&group->smp_shot_count);
	up(&group->smp_shot);
	up(&groupmgr->group_smp_res[group->id]);

	return ret;
}
