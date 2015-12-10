/* linux/drivers/media/video/exynos/gsc/gsc-regs.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS5 SoC series G-scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <mach/map.h>
#include "gsc-core.h"

void gsc_hw_set_dynamic_clock_gating(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->sysreg_gscl + GSCL_XIU_TOP_DCG_EN);
	cfg |= 0x1;
	writel(cfg, dev->sysreg_gscl + GSCL_XIU_TOP_DCG_EN);
	cfg = readl(dev->sysreg_gscl + GSCL_DYN_CLKGATE_DISABLE);
	cfg &= 0x0;
	writel(cfg, dev->sysreg_gscl + GSCL_DYN_CLKGATE_DISABLE);
}

void gsc_hw_set_pp_index_init(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->regs + GSC_ENABLE);
	cfg &= ~GSC_ENABLE_PP_IDX_INIT_MASK;
	cfg |= GSC_ENABLE_PP_IDX_INIT_SHADOW;
	writel(cfg, dev->regs + GSC_ENABLE);
}

void gsc_hw_set_lookup_table(struct gsc_dev *dev)
{
	writel(0xFFFFFFFF, dev->regs + GSC_IN_QOS_LUT07_00);
	writel(0xFFFFFFFF, dev->regs + GSC_IN_QOS_LUT15_08);
	writel(0xA9876543, dev->regs + GSC_OUT_QOS_LUT07_00);
	writel(0xFFFFEDCB, dev->regs + GSC_OUT_QOS_LUT15_08);
}

void gsc_hw_set_smart_if_pix_num(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->d_frame;

	u32 cfg = readl(dev->regs + GSC_SMART_IF_PIXEL_NUM);
	cfg = frame->crop.width * frame->crop.height;
	writel(cfg, dev->regs + GSC_SMART_IF_PIXEL_NUM);
}

void gsc_hw_set_smart_if_con(struct gsc_dev *dev, bool enable)
{
	u32 cfg = readl(dev->regs + GSC_SMART_IF_CON);

	if (enable)
		cfg = (GSC_SMART_IF_EN | GSC_HIRACHICAL_MODE);
	else
		cfg &= ~GSC_SMART_IF_EN;

	writel(cfg, dev->regs + GSC_SMART_IF_CON);
}

void gsc_hw_set_buscon_realtime(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->regs + GSC_BUSCON);
	cfg |= GSC_BUSCON_REAL_TIME_ACCESS_EN;
	writel(cfg, dev->regs + GSC_BUSCON);
}

void gsc_hw_set_qos_enable(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->regs + GSC_ENABLE);
	cfg |= GSC_ENABLE_QOS_ENABLE;
	writel(cfg, dev->regs + GSC_ENABLE);
}

void gsc_hw_enable_localout(struct gsc_ctx *ctx, bool enable)
{
	struct gsc_dev *dev = ctx->gsc_dev;

	if (enable) {
		/* realtime effort */
		gsc_hw_set_buscon_realtime(dev);
		gsc_hw_set_pp_index_init(dev);

		gsc_hw_set_smart_if_pix_num(ctx);
		gsc_hw_set_smart_if_con(dev, true);
		gsc_hw_set_lookup_table(dev);

		gsc_hw_set_qos_enable(dev);
		gsc_hw_enable_control(dev, true);
		gsc_dbg("GSC start(%d)", dev->id);
	} else {
		gsc_hw_set_smart_if_con(dev, false);
		gsc_hw_enable_control(dev, false);
		gsc_hw_set_sfr_update(ctx);
	}
}

void gsc_hw_set_sfr_update_force(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->regs + GSC_ENABLE);
	cfg |= GSC_ENABLE_SFR_UPDATE_FORCE;
	writel(cfg, dev->regs + GSC_ENABLE);
}

void gsc_hw_set_local_path_open_irq_mask(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_IRQ);
	if (mask)
		cfg |= GSC_IRQ_LOCAL_PATH_OPEN_MASK;
	else
		cfg &= ~GSC_IRQ_LOCAL_PATH_OPEN_MASK;
	writel(cfg, dev->regs + GSC_IRQ);
}

void gsc_hw_set_read_slave_error_mask(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_IRQ);
	if (mask)
		cfg |= GSC_IRQ_READ_SLAVE_ERROR_MASK;
	else
		cfg &= ~GSC_IRQ_READ_SLAVE_ERROR_MASK;
	writel(cfg, dev->regs + GSC_IRQ);
}

void gsc_hw_set_write_slave_error_mask(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_IRQ);
	if (mask)
		cfg |= GSC_IRQ_WRITE_SLAVE_ERROR_MASK;
	else
		cfg &= ~GSC_IRQ_WRITE_SLAVE_ERROR_MASK;
	writel(cfg, dev->regs + GSC_IRQ);
}

void gsc_hw_set_max_read_axi_issue_cap(struct gsc_dev *dev, u32 level)
{
	u32 cfg = readl(dev->regs + GSC_IN_CON);

	cfg &= ~GSC_IN_IC_MAX_MASK;
	cfg |= GSC_IN_IC_MAX_MO(level);

	writel(cfg, dev->regs + GSC_IN_CON);
}

void gsc_hw_set_max_write_axi_issue_cap(struct gsc_dev *dev, u32 level)
{
	u32 cfg = readl(dev->regs + GSC_OUT_CON);

	cfg &= ~GSC_OUT_IC_MAX_MASK;
	cfg |= GSC_OUT_IC_MAX_MO(level);

	writel(cfg, dev->regs + GSC_OUT_CON);
}

void gsc_hw_set_output_rotation(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	u32 cfg;
	cfg = readl(dev->regs + GSC_OUT_CON);
	cfg &= ~GSC_OUT_ROT_MASK;

	switch (ctx->gsc_ctrls.rotate->val) {
	case 270:
		cfg |= GSC_OUT_ROT_270;
		break;
	case 180:
		cfg |= GSC_OUT_ROT_180;
		break;
	case 90:
		if (ctx->gsc_ctrls.hflip->val)
			cfg |= GSC_OUT_ROT_90_XFLIP;
		else if (ctx->gsc_ctrls.vflip->val)
			cfg |= GSC_OUT_ROT_90_YFLIP;
		else
			cfg |= GSC_OUT_ROT_90;
		break;
	case 0:
		if (ctx->gsc_ctrls.hflip->val)
			cfg |= GSC_OUT_ROT_XFLIP;
		else if (ctx->gsc_ctrls.vflip->val)
			cfg |= GSC_OUT_ROT_YFLIP;
	}

	writel(cfg, dev->regs + GSC_OUT_CON);
}

void gsc_hw_set_sw_reset(struct gsc_dev *dev)
{
	u32 cfg = 0;

	cfg |= GSC_SW_RESET_SRESET;

	writel(cfg, dev->regs + GSC_SW_RESET);
}

void gsc_hw_set_disp_pixelasync_reset(struct gsc_dev *dev)
{
	u32 cfg = readl(SYSREG_DISPBLK_CFG2);
	cfg |= DISP1BLK_LO_MASK_ALL;
	cfg &= ~DISP1BLK_LO_MASK(dev->id);
	writel(cfg, SYSREG_DISPBLK_CFG2);
	/* DISPBLK1 FIFO S/W reset sequence
	   set FIFORST_DISP1 as 0 then, set FIFORST_DISP1 as 1 again */
	cfg = readl(SYSREG_DISP1BLK_CFG);
	cfg &= ~FIFORST_DISP1;
	writel(cfg, SYSREG_DISP1BLK_CFG);
	cfg |= FIFORST_DISP1;
	writel(cfg, SYSREG_DISP1BLK_CFG);

	cfg = readl(SYSREG_DISPBLK_CFG2);
	cfg |= DISP1BLK_LO_MASK_ALL;
	writel(cfg, SYSREG_DISPBLK_CFG2);
}

void gsc_hw_set_pixelasync_reset_output(struct gsc_dev *dev)
{
	u32 cfg = readl(SYSREG_GSCBLK_CFG0);
	cfg |= GSC_PXLASYNC_MASK_ALL;
	cfg &= ~GSC_PXLASYNC_MASK(dev->id);
	writel(cfg, SYSREG_GSCBLK_CFG0);
	/* GSCBLK Pixel asyncy FIFO S/W reset sequence
	   set PXLASYNC_SW_RESET as 0 then, set PXLASYNC_SW_RESET as 1 again */
	cfg &= ~GSC_PXLASYNC_RST(dev->id);
	writel(cfg, SYSREG_GSCBLK_CFG0);
	cfg |= GSC_PXLASYNC_RST(dev->id);
	writel(cfg, SYSREG_GSCBLK_CFG0);
	/* This is for prohibit of reset signal DISP0 */
	cfg |= GSC_PXLASYNC_MASK_ALL;
	writel(cfg, SYSREG_GSCBLK_CFG0);
}

int gsc_wait_reset(struct gsc_dev *dev)
{
	u32 cfg;
	u32 cnt = (loops_per_jiffy * HZ) / MSEC_PER_SEC;

	do {
		cfg = readl(dev->regs + GSC_SW_RESET);
		if (!cfg)
			return 0;
	} while (--cnt);

	return -EINVAL;
}

int gsc_wait_operating(struct gsc_dev *dev)
{
	u32 cfg;
	u32 cnt = (loops_per_jiffy * HZ) / MSEC_PER_SEC;

	do {
		cfg = readl(dev->regs + GSC_ENABLE);
		if (cfg & GSC_ENABLE_OP_STATUS)
			return 0;
	} while (--cnt);

	return -EBUSY;
}

int gsc_wait_stop(struct gsc_dev *dev)
{
	u32 cfg;
	int ret;

	ktime_t start = ktime_get();

	do {
		cfg = readl(dev->regs + GSC_ENABLE);
		if (!(cfg & GSC_ENABLE_STOP_STATUS_STOP_SEQ)) {
			gsc_dbg("GSC stop(%d)", dev->id);
			return 0;
		}
		usleep_range(10, 20);
	} while(ktime_us_delta(ktime_get(), start) < 1000000);

	/* This is workaround until next chips.
	 * If fimd is stop than gsc, gsc didn't work complete
	 */

	gsc_err("Stop timeout!!");
	gsc_hw_set_sw_reset(dev);
	ret = gsc_wait_reset(dev);
	if (ret < 0) {
		gsc_err("gscaler s/w reset timeout");
		return ret;
	}

	return 0;
}

void gsc_hw_set_in_chrom_stride(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->s_frame;
	u32 chrom_size, cfg;

	chrom_size = ALIGN(frame->f_width / 2, 16) * 2;
	cfg = GSC_IN_CHROM_STRIDE_VALUE(chrom_size);
	writel(cfg, dev->regs + GSC_IN_CHROM_STRIDE);
}

void gsc_hw_set_out_chrom_stride(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->d_frame;
	u32 chrom_size, cfg;

	chrom_size = ALIGN(frame->f_width / 2, 16) * 2;
	cfg = GSC_OUT_CHROM_STRIDE_VALUE(chrom_size);
	writel(cfg, dev->regs + GSC_OUT_CHROM_STRIDE);
}

void gsc_hw_set_in_pingpong_update(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->regs + GSC_ENABLE);
	cfg |= GSC_ENABLE_IN_PP_UPDATE;
	writel(cfg, dev->regs + GSC_ENABLE);
}

void gsc_hw_set_one_frm_mode(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_ENABLE);
	cfg &= ~(GSC_ENABLE_ON_CLEAR_MASK);
	if (mask)
		cfg |= GSC_ENABLE_ON_CLEAR_ONESHOT;
	writel(cfg, dev->regs + GSC_ENABLE);
}

void gsc_hw_set_fire_bit_sync_mode(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_ENABLE);
	cfg &= ~(GSC_ENABLE_PP_UPDATE_MODE_MASK);
	if (mask)
		cfg |= GSC_ENABLE_PP_UPDATE_FIRE_MODE;
	writel(cfg, dev->regs + GSC_ENABLE);
}

int gsc_hw_get_mxr_path_status(void)
{
	int i, cnt = 0;

	u32 cfg = readl(SYSREG_GSCBLK_CFG0);
	for (i = 0; i < GSC_MAX_DEVS; i++) {
		if (cfg & (GSC_OUT_DST_MXR_SEL(i)))
			cnt++;
	}
	return (cnt > 2) ? 1 : 0;
}

int gsc_hw_get_input_buf_mask_status(struct gsc_dev *dev)
{
	u32 cfg, status, bits = 0;

	cfg = readl(dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
	status = cfg & GSC_IN_BASE_ADDR_MASK;
	while (status) {
		status = status & (status - 1);
		bits++;
	}
	return bits;
}

int gsc_hw_get_done_input_buf_index(struct gsc_dev *dev)
{
	u32 cfg, curr_index, i;

	cfg = readl(dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
	curr_index = GSC_IN_CURR_GET_INDEX(cfg);
	for (i = curr_index; i > 1; i--) {
		if (cfg ^ (1 << (i - 2)))
			return i - 2;
	}

	for (i = dev->variant->in_buf_cnt; i > curr_index; i--) {
		if (cfg ^ (1 << (i - 1)))
			return i - 1;
	}

	return curr_index - 1;
}

int gsc_hw_get_done_output_buf_index(struct gsc_dev *dev)
{
	u32 cfg, curr_index, done_buf_index;
	unsigned long state_mask;
	u32 reqbufs_cnt = dev->cap.reqbufs_cnt;

	cfg = readl(dev->regs + GSC_OUT_BASE_ADDR_Y_MASK);
	curr_index = GSC_OUT_CURR_GET_INDEX(cfg);
	gsc_dbg("curr_index : %d", curr_index);
	state_mask = cfg & GSC_OUT_BASE_ADDR_MASK;

	done_buf_index = (curr_index == 0) ? reqbufs_cnt - 1 : curr_index - 1;

	do {
		/* Test done_buf_index whether masking or not */
		if (test_bit(done_buf_index, &state_mask))
			done_buf_index = (done_buf_index == 0) ?
				reqbufs_cnt - 1 : done_buf_index - 1;
		else
			return done_buf_index;
	} while (done_buf_index != curr_index);

	return -EBUSY;
}

void gsc_hw_set_frm_done_irq_mask(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_IRQ);
	if (mask)
		cfg |= GSC_IRQ_FRMDONE_MASK;
	else
		cfg &= ~GSC_IRQ_FRMDONE_MASK;
	writel(cfg, dev->regs + GSC_IRQ);
}

void gsc_hw_set_overflow_irq_mask(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_IRQ);
	if (mask)
		cfg |= GSC_IRQ_OR_MASK;
	else
		cfg &= ~GSC_IRQ_OR_MASK;
	writel(cfg, dev->regs + GSC_IRQ);
}

void gsc_hw_set_deadlock_irq_mask(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_IRQ);
	if (mask)
		cfg |= GSC_IRQ_DEADLOCK_MASK;
	else
		cfg &= ~GSC_IRQ_DEADLOCK_MASK;
	writel(cfg, dev->regs + GSC_IRQ);
}

void gsc_hw_set_gsc_irq_enable(struct gsc_dev *dev, bool mask)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_IRQ);
	if (mask)
		cfg |= GSC_IRQ_ENABLE;
	else
		cfg &= ~GSC_IRQ_ENABLE;
	writel(cfg, dev->regs + GSC_IRQ);
}

void gsc_hw_set_input_buf_mask_all(struct gsc_dev *dev)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
	cfg |= GSC_IN_BASE_ADDR_MASK;
	cfg |= GSC_IN_BASE_ADDR_PINGPONG(dev->variant->in_buf_cnt);

	writel(cfg, dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
}

void gsc_hw_set_input_buf_fixed(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
	cfg &= ~(0x3 << 16);

	writel(cfg, dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
}

void gsc_hw_set_output_buf_fixed(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->regs + GSC_OUT_BASE_ADDR_Y_MASK);
	cfg &= ~(0x1F << 16);

	writel(cfg, dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
}

void gsc_hw_set_output_buf_mask_all(struct gsc_dev *dev)
{
	u32 cfg;

	cfg = readl(dev->regs + GSC_OUT_BASE_ADDR_Y_MASK);
	cfg |= GSC_OUT_BASE_ADDR_MASK;
	cfg |= GSC_OUT_BASE_ADDR_PINGPONG(dev->variant->out_buf_cnt);

	writel(cfg, dev->regs + GSC_OUT_BASE_ADDR_Y_MASK);
	writel(cfg, dev->regs + GSC_OUT_BASE_ADDR_CB_MASK);
	writel(cfg, dev->regs + GSC_OUT_BASE_ADDR_CR_MASK);
}

void gsc_hw_set_input_apply_pending_bit(struct gsc_dev *dev)
{
	u32 cfg = readl(dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
	cfg &= ~0xf;
	cfg |= dev->out.pending_mask;
	writel(cfg, dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
}

void gsc_hw_set_input_buf_masking(struct gsc_dev *dev, u32 shift,
				bool enable)
{
	u32 cfg = readl(dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
	u32 mask = 1 << shift;

	cfg &= (~mask);
	cfg |= enable << shift;

	writel(cfg, dev->regs + GSC_IN_BASE_ADDR_Y_MASK);
}

void gsc_hw_set_output_buf_masking(struct gsc_dev *dev, u32 shift,
				bool enable)
{
	u32 cfg = readl(dev->regs + GSC_OUT_BASE_ADDR_Y_MASK);
	u32 mask = 1 << shift;

	cfg &= (~mask);
	cfg |= enable << shift;

	writel(cfg, dev->regs + GSC_OUT_BASE_ADDR_Y_MASK);
	writel(cfg, dev->regs + GSC_OUT_BASE_ADDR_CB_MASK);
	writel(cfg, dev->regs + GSC_OUT_BASE_ADDR_CR_MASK);
}

int gsc_hw_get_nr_unmask_bits(struct gsc_dev *dev)
{
	u32 bits = 0;
	u32 mask_bits = readl(dev->regs + GSC_OUT_BASE_ADDR_Y_MASK);
	mask_bits &= GSC_OUT_BASE_ADDR_MASK;

	while (mask_bits) {
		mask_bits = mask_bits & (mask_bits - 1);
		bits++;
	}
	bits = 16 - bits;

	return bits;
}

void gsc_hw_set_input_addr(struct gsc_dev *dev, struct gsc_addr *addr,
				int index)
{
	gsc_dbg("src_buf[%d]: 0x%pa, cb: 0x%pa, cr: 0x%pa", index,
		&addr->y, &addr->cb, &addr->cr);
	writel(addr->y, dev->regs + GSC_IN_BASE_ADDR_Y(index));
	writel(addr->cb, dev->regs + GSC_IN_BASE_ADDR_CB(index));
	writel(addr->cr, dev->regs + GSC_IN_BASE_ADDR_CR(index));

}

void gsc_hw_set_input_addr_fixed(struct gsc_dev *dev, struct gsc_addr *addr)
{
	gsc_dbg("src_buf: 0x%pa, cb: 0x%pa, cr: 0x%pa",
		&addr->y, &addr->cb, &addr->cr);
	writel(addr->y, dev->regs + GSC_IN_BASE_ADDR_Y(0));
	writel(addr->cb, dev->regs + GSC_IN_BASE_ADDR_CB(0));
	writel(addr->cr, dev->regs + GSC_IN_BASE_ADDR_CR(0));
}

void gsc_hw_set_output_addr_fixed(struct gsc_dev *dev, struct gsc_addr *addr)
{
	gsc_dbg("dst_buf: 0x%pa, cb: 0x%pa, cr: 0x%pa",
			&addr->y, &addr->cb, &addr->cr);
	writel(addr->y, dev->regs + GSC_OUT_BASE_ADDR_Y(0));
	writel(addr->cb, dev->regs + GSC_OUT_BASE_ADDR_CB(0));
	writel(addr->cr, dev->regs + GSC_OUT_BASE_ADDR_CR(0));
}

void gsc_hw_set_output_addr(struct gsc_dev *dev,
			     struct gsc_addr *addr, int index)
{
	gsc_dbg("dst_buf[%d]: 0x%pa, cb: 0x%pa, cr: 0x%pa",
			index, &addr->y, &addr->cb, &addr->cr);
	writel(addr->y, dev->regs + GSC_OUT_BASE_ADDR_Y(index));
	writel(addr->cb, dev->regs + GSC_OUT_BASE_ADDR_CB(index));
	writel(addr->cr, dev->regs + GSC_OUT_BASE_ADDR_CR(index));
}

void gsc_hw_set_freerun_clock_mode(struct gsc_dev *dev, bool mask)
{
	u32 cfg = readl(dev->regs + GSC_ENABLE);

	cfg &= ~(GSC_ENABLE_CLK_GATE_MODE_MASK);
	if (mask)
		cfg |= GSC_ENABLE_CLK_GATE_MODE_FREE;
	writel(cfg, dev->regs + GSC_ENABLE);
}

void gsc_hw_set_input_path(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;

	u32 cfg = readl(dev->regs + GSC_IN_CON);
	cfg &= ~(GSC_IN_PATH_MASK | GSC_IN_LOCAL_SEL_MASK);

	if (ctx->in_path == GSC_DMA) {
		cfg |= GSC_IN_PATH_MEMORY;
	} else {
		cfg |= GSC_IN_PATH_LOCAL;
		if (ctx->in_path == GSC_WRITEBACK) {
			cfg |= GSC_IN_LOCAL_WB;
		}
	}

	writel(cfg, dev->regs + GSC_IN_CON);
}

void gsc_hw_set_in_size(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->s_frame;
	u32 cfg;

	/* Set input pixel offset */
	cfg = GSC_SRCIMG_OFFSET_X(frame->crop.left);
	cfg |= GSC_SRCIMG_OFFSET_Y(frame->crop.top);
	writel(cfg, dev->regs + GSC_SRCIMG_OFFSET);

	/* Set input original size */
	cfg = GSC_SRCIMG_WIDTH(frame->f_width);
	cfg |= GSC_SRCIMG_HEIGHT(frame->f_height);
	writel(cfg, dev->regs + GSC_SRCIMG_SIZE);

	/* Set input cropped size */
	cfg = GSC_CROPPED_WIDTH(frame->crop.width);
	cfg |= GSC_CROPPED_HEIGHT(frame->crop.height);
	writel(cfg, dev->regs + GSC_CROPPED_SIZE);
}

void gsc_hw_set_in_image_rgb(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->s_frame;
	u32 cfg;

	cfg = readl(dev->regs + GSC_IN_CON);
	if (is_csc_eq_709) {
		if (ctx->gsc_ctrls.csc_range->val)
			cfg |= GSC_IN_RGB_HD_WIDE;
		else
			cfg |= GSC_IN_RGB_HD_NARROW;
	} else {
		if (ctx->gsc_ctrls.csc_range->val)
			cfg |= GSC_IN_RGB_SD_WIDE;
		else
			cfg |= GSC_IN_RGB_SD_NARROW;
	}

	if (frame->fmt->pixelformat == V4L2_PIX_FMT_RGB565)
		cfg |= GSC_IN_RGB565;
	else if (frame->fmt->pixelformat == V4L2_PIX_FMT_BGR32)
		cfg |= GSC_IN_XRGB8888;
	else if (frame->fmt->pixelformat == V4L2_PIX_FMT_RGB32) {
		if (gsc_cap_opened(dev))
			cfg |= GSC_IN_XRGB8888;
		else
			cfg |= GSC_IN_XRGB8888 | GSC_IN_RB_SWAP;
	}

	writel(cfg, dev->regs + GSC_IN_CON);
}

void gsc_hw_set_in_image_format(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->s_frame;
	u32 i, depth = 0;
	u32 cfg;

	cfg = readl(dev->regs + GSC_IN_CON);
	cfg &= ~(GSC_IN_RGB_TYPE_MASK | GSC_IN_YUV422_1P_ORDER_MASK |
		 GSC_IN_CHROMA_ORDER_MASK | GSC_IN_FORMAT_MASK |
		 GSC_IN_TILE_TYPE_MASK | GSC_IN_TILE_MODE |
		 GSC_IN_CHROM_STRIDE_SEL_MASK | GSC_IN_RB_SWAP_MASK);
	writel(cfg, dev->regs + GSC_IN_CON);

	if (is_rgb(frame->fmt->pixelformat)) {
		gsc_hw_set_in_image_rgb(ctx);
		return;
	}
	for (i = 0; i < frame->fmt->num_planes; i++)
		depth += frame->fmt->depth[i];

	switch (frame->fmt->nr_comp) {
	case 1:
		cfg |= GSC_IN_YUV422_1P;
		if (frame->fmt->yorder == GSC_LSB_Y)
			cfg |= GSC_IN_YUV422_1P_ORDER_LSB_Y;
		else
			cfg |= GSC_IN_YUV422_1P_OEDER_LSB_C;
		if (frame->fmt->corder == GSC_CBCR)
			cfg |= GSC_IN_CHROMA_ORDER_CBCR;
		else
			cfg |= GSC_IN_CHROMA_ORDER_CRCB;
		break;
	case 2:
		if (depth == 12)
			cfg |= GSC_IN_YUV420_2P;
		else
			cfg |= GSC_IN_YUV422_2P;
		if (frame->fmt->corder == GSC_CBCR)
			cfg |= GSC_IN_CHROMA_ORDER_CBCR;
		else
			cfg |= GSC_IN_CHROMA_ORDER_CRCB;
		break;
	case 3:
		if (depth == 12)
			cfg |= GSC_IN_YUV420_3P;
		else
			cfg |= GSC_IN_YUV422_3P;
		break;
	};

	if (is_AYV12(frame->fmt->pixelformat)) {
		cfg |= GSC_IN_CHROM_STRIDE_SEPAR;
		gsc_hw_set_in_chrom_stride(ctx);
	}

	if (is_tiled(frame->fmt))
		cfg |= GSC_IN_TILE_C_16x8 | GSC_IN_TILE_MODE;

	writel(cfg, dev->regs + GSC_IN_CON);
}

void gsc_hw_set_output_path(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;

	u32 cfg = readl(dev->regs + GSC_OUT_CON);
	cfg &= ~GSC_OUT_PATH_MASK;

	if (ctx->out_path == GSC_DMA)
		cfg |= GSC_OUT_PATH_MEMORY;
	else
		cfg |= GSC_OUT_PATH_LOCAL;

	writel(cfg, dev->regs + GSC_OUT_CON);
}

void gsc_hw_set_out_size(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->d_frame;
	u32 cfg;

	/* Set output original size */
	if (ctx->out_path == GSC_DMA) {
		cfg = GSC_DSTIMG_OFFSET_X(frame->crop.left);
		cfg |= GSC_DSTIMG_OFFSET_Y(frame->crop.top);
		writel(cfg, dev->regs + GSC_DSTIMG_OFFSET);

		cfg = GSC_DSTIMG_WIDTH(frame->f_width);
		cfg |= GSC_DSTIMG_HEIGHT(frame->f_height);
		writel(cfg, dev->regs + GSC_DSTIMG_SIZE);
	}

	/* Set output scaled size */
	if (is_rotation) {
		cfg = GSC_SCALED_WIDTH(frame->crop.height);
		cfg |= GSC_SCALED_HEIGHT(frame->crop.width);
	} else {
		cfg = GSC_SCALED_WIDTH(frame->crop.width);
		cfg |= GSC_SCALED_HEIGHT(frame->crop.height);
	}

	writel(cfg, dev->regs + GSC_SCALED_SIZE);
}

void gsc_hw_set_out_image_rgb(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->d_frame;
	u32 cfg;

	cfg = readl(dev->regs + GSC_OUT_CON);
	if (is_csc_eq_709)
		cfg |= GSC_OUT_RGB_HD_WIDE;
	else
		cfg |= GSC_OUT_RGB_SD_WIDE;

	if (ctx->out_path != GSC_DMA) {
		cfg |= GSC_OUT_XRGB8888;
	} else {
		if (frame->fmt->pixelformat == V4L2_PIX_FMT_RGB565)
			cfg |= GSC_OUT_RGB565;
		else if (frame->fmt->pixelformat == V4L2_PIX_FMT_BGR32)
			cfg |= GSC_OUT_XRGB8888;
		else if (frame->fmt->pixelformat == V4L2_PIX_FMT_RGB32)
			cfg |= GSC_OUT_XRGB8888 | GSC_OUT_RB_SWAP;
	}

	writel(cfg, dev->regs + GSC_OUT_CON);
}

void gsc_hw_set_out_image_format(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->d_frame;
	u32 i, depth = 0;
	u32 cfg;

	cfg = readl(dev->regs + GSC_OUT_CON);
	cfg &= ~(GSC_OUT_RGB_TYPE_MASK | GSC_OUT_YUV422_1P_ORDER_MASK |
		 GSC_OUT_CHROMA_ORDER_MASK | GSC_OUT_FORMAT_MASK |
		 GSC_OUT_CHROM_STRIDE_SEL_MASK | GSC_OUT_RB_SWAP_MASK);
	writel(cfg, dev->regs + GSC_OUT_CON);

	if (ctx->out_path != GSC_DMA || is_rgb(frame->fmt->pixelformat)) {
		gsc_hw_set_out_image_rgb(ctx);
		return;
	}

	for (i = 0; i < frame->fmt->num_planes; i++)
		depth += frame->fmt->depth[i];

	switch (frame->fmt->nr_comp) {
	case 1:
		cfg |= GSC_OUT_YUV422_1P;
		if (frame->fmt->yorder == GSC_LSB_Y)
			cfg |= GSC_OUT_YUV422_1P_ORDER_LSB_Y;
		else
			cfg |= GSC_OUT_YUV422_1P_OEDER_LSB_C;
		if (frame->fmt->corder == GSC_CBCR)
			cfg |= GSC_OUT_CHROMA_ORDER_CBCR;
		else
			cfg |= GSC_OUT_CHROMA_ORDER_CRCB;
		break;
	case 2:
		if (depth == 12)
			cfg |= GSC_OUT_YUV420_2P;
		else
			cfg |= GSC_OUT_YUV422_2P;
		if (frame->fmt->corder == GSC_CBCR)
			cfg |= GSC_OUT_CHROMA_ORDER_CBCR;
		else
			cfg |= GSC_OUT_CHROMA_ORDER_CRCB;
		break;
	case 3:
		cfg |= GSC_OUT_YUV420_3P;
		break;
	};

	if (is_AYV12(frame->fmt->pixelformat)) {
		cfg |= GSC_OUT_CHROM_STRIDE_SEPAR;
		gsc_hw_set_out_chrom_stride(ctx);
	}

	writel(cfg, dev->regs + GSC_OUT_CON);
}

void gsc_hw_set_prescaler(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_scaler *sc = &ctx->scaler;
	u32 cfg;

	cfg = GSC_PRESC_SHFACTOR(sc->pre_shfactor);
	cfg |= GSC_PRESC_H_RATIO(sc->pre_hratio);
	cfg |= GSC_PRESC_V_RATIO(sc->pre_vratio);
	writel(cfg, dev->regs + GSC_PRE_SCALE_RATIO);
}

void gsc_hw_set_mainscaler(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_scaler *sc = &ctx->scaler;
	u32 cfg;

	cfg = GSC_MAIN_H_RATIO_VALUE(sc->main_hratio);
	writel(cfg, dev->regs + GSC_MAIN_H_RATIO);

	cfg = GSC_MAIN_V_RATIO_VALUE(sc->main_vratio);
	writel(cfg, dev->regs + GSC_MAIN_V_RATIO);

	if ((!is_tiled(ctx->s_frame.fmt)) && (!is_flip(ctx)) &&
			(sc->main_hratio > GSC_SC_UP_MAX_RATIO) &&
			(sc->main_vratio > GSC_SC_UP_MAX_RATIO))
		sc->is_scaled_down = true;
	else
		sc->is_scaled_down = false;
}

void gsc_hw_set_input_rotation(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	u32 cfg;

	cfg = readl(dev->regs + GSC_IN_CON);
	cfg &= ~GSC_IN_ROT_MASK;

	switch (ctx->gsc_ctrls.rotate->val) {
	case 270:
		cfg |= GSC_IN_ROT_270;
		break;
	case 180:
		cfg |= GSC_IN_ROT_180;
		break;
	case 90:
		if (ctx->gsc_ctrls.hflip->val)
			cfg |= GSC_IN_ROT_90_XFLIP;
		else if (ctx->gsc_ctrls.vflip->val)
			cfg |= GSC_IN_ROT_90_YFLIP;
		else
			cfg |= GSC_IN_ROT_90;
		break;
	case 0:
		if (ctx->gsc_ctrls.hflip->val)
			cfg |= GSC_IN_ROT_XFLIP;
		else if (ctx->gsc_ctrls.vflip->val)
			cfg |= GSC_IN_ROT_YFLIP;
	}

	writel(cfg, dev->regs + GSC_IN_CON);
}

void gsc_hw_set_global_alpha(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	struct gsc_frame *frame = &ctx->d_frame;
	u32 cfg;

	cfg = readl(dev->regs + GSC_OUT_CON);
	cfg &= ~GSC_OUT_GLOBAL_ALPHA_MASK;

	if (!is_rgb(frame->fmt->pixelformat)) {
		gsc_dbg("Not a RGB format");
		return;
	}

	cfg |= GSC_OUT_GLOBAL_ALPHA(ctx->gsc_ctrls.global_alpha->val);
	writel(cfg, dev->regs + GSC_OUT_CON);
}

void gsc_hw_set_sfr_update(struct gsc_ctx *ctx)
{
	struct gsc_dev *dev = ctx->gsc_dev;
	u32 cfg;
	ktime_t start = ktime_get();

	do {
		cfg = readl(dev->regs + GSC_ENABLE);
		if (!(cfg & GSC_ENABLE_SFR_UPDATE))
			break;
		udelay(1);
	} while(ktime_us_delta(ktime_get(), start) < 20000);

	cfg |= GSC_ENABLE_SFR_UPDATE;
	writel(cfg, dev->regs + GSC_ENABLE);
}

void gsc_hw_set_mixer(int id)
{
	u32 cfg = readl(SYSREG_DISP1BLK_CFG);

	cfg &= ~MIXER_SRC_VALID_MASK_ALL;
	cfg |= MIXER0_SRC_GSC(id);
	cfg |= MIXER0_VALID;

	writel(cfg, SYSREG_DISP1BLK_CFG);
}

void gsc_hw_set_local_src(struct gsc_dev *dev, bool on)
{
	u32 cfg = readl(dev->sysreg_disp + DSD_CFG);
	cfg &= ~(0x3 << 7);

	if (on)
		cfg |= (0x3 << 7);

	writel(cfg, dev->sysreg_disp + DSD_CFG);
}

void gsc_hw_set_local_dst(struct gsc_dev *dev, int out, bool on)
{
	u32 cfg = readl(dev->sysreg_disp + DSD_CFG);

	if (soc_is_exynos5430()) {
		if (on) {
			cfg &= ~((0x3 << (4 + dev->id * 2)) |
				(0x1 << (0 + dev->id)));
			cfg |= (0x1 << (4 + dev->id * 2));
			writel(cfg, dev->sysreg_disp + DSD_CFG);
			writel(0x80000000,dev->sysreg_disp + DSD_RESERVE10);
		} else {
			cfg &= ~(0x3 << (4 + dev->id * 2));
			writel(cfg, dev->sysreg_disp + DSD_CFG);
		}
	} else {
		if (on) {
			cfg &= ~(0x3 << (3 + (dev->id * 2)));
			cfg |= (0 << (3 + ((dev->id) * 2)));
			writel(cfg, dev->sysreg_disp + DSD_CFG);
		} else {
			cfg &= ~(0x3 << (3 + (dev->id * 2)));
			writel(cfg, dev->sysreg_disp + DSD_CFG);
		}
	}
}

void gsc_hw_set_pixelasync_reset_wb(struct gsc_dev *dev)
{
	u32 cfg = readl(SYSREG_GSCBLK_CFG1);

	cfg |= GSC_PXLASYNC_MASK_ALL_WB;
	cfg &= ~GSC_PXLASYNC_MASK_WB(dev->id);
	writel(cfg, SYSREG_GSCBLK_CFG1);

	cfg &= ~GSC_BLK_SW_RESET_WB_DEST(dev->id);
	writel(cfg, SYSREG_GSCBLK_CFG1);
	cfg |= GSC_BLK_SW_RESET_WB_DEST(dev->id);
	writel(cfg, SYSREG_GSCBLK_CFG1);
	/*
	 * This bit should be masked if DISP0 is off
	 */
	cfg |= GSC_PXLASYNC_MASK_ALL_WB;
	writel(cfg, SYSREG_GSCBLK_CFG1);
}

void gsc_hw_set_for_wb(struct gsc_dev *dev)
{
	writel(0x80000000, dev->regs + 0xAE8);
}

void gsc_hw_set_sysreg_writeback(struct gsc_dev *dev, bool on)
{
	writel(0x00000001, dev->sysreg_gscl + GSCLBLK_CFG0);
}

void gsc_hw_set_pxlasync_camif_lo_mask(struct gsc_dev *dev, bool on)
{
	u32 cfg = 0;

	if (dev->id == 3) {
		cfg = readl(SYSREG_GSCBLK_CFG0);
		if (on)
			cfg |= PXLASYNC_LO_MASK_CAMIF_TOP;
		else
			cfg &= ~(PXLASYNC_LO_MASK_CAMIF_TOP);
		writel(cfg, SYSREG_GSCBLK_CFG0);
	} else {
		cfg = readl(SYSREG_GSCBLK_CFG2);
		if (on)
			cfg |= PXLASYNC_LO_MASK_CAMIF_GSCL(dev->id);
		else
			cfg &= ~PXLASYNC_LO_MASK_CAMIF_GSCL(dev->id);
		writel(cfg, SYSREG_GSCBLK_CFG2);
	}
}

void gsc_hw_set_h_coef(struct gsc_ctx *ctx)
{
	struct gsc_scaler *sc = &ctx->scaler;
	struct gsc_dev *dev = ctx->gsc_dev;
	int i, j, k, sc_ratio;

	if (sc->main_hratio <= GSC_SC_UP_MAX_RATIO)
		sc_ratio = 0;
	else if (sc->main_hratio <= GSC_SC_DOWN_RATIO_7_8)
		sc_ratio = 1;
	else if (sc->main_hratio <= GSC_SC_DOWN_RATIO_6_8)
		sc_ratio = 2;
	else if (sc->main_hratio <= GSC_SC_DOWN_RATIO_5_8)
		sc_ratio = 3;
	else if (sc->main_hratio <= GSC_SC_DOWN_RATIO_4_8)
		sc_ratio = 4;
	else if (sc->main_hratio <= GSC_SC_DOWN_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++) {
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 3; k++) {
				writel(h_coef_8t[sc_ratio][i][j],
				       dev->regs + GSC_HCOEF(i, j, k));
			}
		}
	}
}

void gsc_hw_set_v_coef(struct gsc_ctx *ctx)
{
	struct gsc_scaler *sc = &ctx->scaler;
	struct gsc_dev *dev = ctx->gsc_dev;
	int i, j, k, sc_ratio = 0;

	if (sc->main_vratio <= GSC_SC_UP_MAX_RATIO)
		sc_ratio = 0;
	else if (sc->main_vratio <= GSC_SC_DOWN_RATIO_7_8)
		sc_ratio = 1;
	else if (sc->main_vratio <= GSC_SC_DOWN_RATIO_6_8)
		sc_ratio = 2;
	else if (sc->main_vratio <= GSC_SC_DOWN_RATIO_5_8)
		sc_ratio = 3;
	else if (sc->main_vratio <= GSC_SC_DOWN_RATIO_4_8)
		sc_ratio = 4;
	else if (sc->main_vratio <= GSC_SC_DOWN_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++) {
		for (j = 0; j < 4; j++) {
			for (k = 0; k < 3; k++) {
				writel(v_coef_4t[sc_ratio][i][j],
				       dev->regs + GSC_VCOEF(i, j, k));
			}
		}
	}
}

#define gsc_dump_reg(base, offset, size)\
	print_hex_dump(KERN_ALERT, "+" #offset ": ", DUMP_PREFIX_OFFSET, 16, 4, \
			base + offset, size, false)
void gsc_hw_dump_regs(void __iomem *base)
{
	gsc_dump_reg(base, 0x000, 0x70);
	gsc_dump_reg(base, 0x080, 0x20);
	gsc_dump_reg(base, 0x0B0, 0x20);
	gsc_dump_reg(base, 0x10C, 0x04);
	gsc_dump_reg(base, 0x110, 0x40);
	gsc_dump_reg(base, 0x160, 0x40);
	gsc_dump_reg(base, 0x1B0, 0x40);

	/* coefficient registers */
	gsc_dump_reg(base, 0x200, 0x84);
	gsc_dump_reg(base, 0x290, 0x24);
	gsc_dump_reg(base, 0x300, 0x24);
	gsc_dump_reg(base, 0x330, 0x24);
	gsc_dump_reg(base, 0x360, 0x24);
	gsc_dump_reg(base, 0x390, 0x24);
	gsc_dump_reg(base, 0x3C0, 0x24);
	gsc_dump_reg(base, 0x3F0, 0x24);
	gsc_dump_reg(base, 0x420, 0x24);
	gsc_dump_reg(base, 0x450, 0x24);
	gsc_dump_reg(base, 0x500, 0x24);
	gsc_dump_reg(base, 0x530, 0x24);
	gsc_dump_reg(base, 0x560, 0x24);
	gsc_dump_reg(base, 0x590, 0x24);
	gsc_dump_reg(base, 0x600, 0x24);
	gsc_dump_reg(base, 0x630, 0x24);
	gsc_dump_reg(base, 0x660, 0x24);
	gsc_dump_reg(base, 0x690, 0x24);
	gsc_dump_reg(base, 0x6C0, 0x24);
	gsc_dump_reg(base, 0x6F0, 0x24);
	gsc_dump_reg(base, 0x720, 0x24);
	gsc_dump_reg(base, 0x750, 0x24);
	gsc_dump_reg(base, 0x800, 0x24);
	gsc_dump_reg(base, 0x830, 0x24);
	gsc_dump_reg(base, 0x860, 0x24);
	gsc_dump_reg(base, 0x890, 0x24);
	gsc_dump_reg(base, 0x900, 0x24);
	gsc_dump_reg(base, 0x930, 0x24);
	gsc_dump_reg(base, 0x960, 0x24);
	gsc_dump_reg(base, 0x990, 0x24);
	gsc_dump_reg(base, 0x9C0, 0x24);
	gsc_dump_reg(base, 0x9F0, 0x24);
	gsc_dump_reg(base, 0xA20, 0x24);
	gsc_dump_reg(base, 0xA50, 0x24);

	gsc_dump_reg(base, 0xA74, 0x08);
	gsc_dump_reg(base, 0xA80, 0x04);
	gsc_dump_reg(base, 0xAF0, 0x0C);

	/* Shadow registers */
	gsc_dump_reg(base, 0xB00, 0x2C);
	gsc_dump_reg(base, 0xB30, 0x10);
}
