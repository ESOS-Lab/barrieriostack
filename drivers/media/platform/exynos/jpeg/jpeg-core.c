/* linux/drivers/media/platform/exynos/jpeg/jpeg-core.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Core file for Samsung H/W Jpeg driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/kmod.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/exynos_iovmm.h>
#include <asm/page.h>

#include <plat/cpu.h>

#include <linux/pm_runtime.h>

#include "jpeg.h"
#include "jpeg_regs.h"

int allow_custom_qtbl = 1;
module_param_named(allow_custom_qtbl, allow_custom_qtbl, int, 0600);

static char *jpeg_clock_names[JPEG_CLK_NUM] = {
	"gate", "gate2", "child1", "parent1",
	"child2", "parent2", "child3", "parent3"
};

static struct jpeg_fmt formats[] = {
	{
		.name		= "JPEG compressed format",
		.fourcc		= V4L2_PIX_FMT_JPEG_444,
		.reg_cfg	= 0x01000000,
		.bitperpixel	= { 32 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "JPEG compressed format",
		.fourcc		= V4L2_PIX_FMT_JPEG_422,
		.reg_cfg	= 0x02000000,
		.bitperpixel	= { 16 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "JPEG compressed format",
		.fourcc		= V4L2_PIX_FMT_JPEG_420,
		.reg_cfg	= 0x03000000,
		.bitperpixel	= { 12 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "JPEG compressed format",
		.fourcc		= V4L2_PIX_FMT_JPEG_422V,
		.reg_cfg	= 0x04000000,
		.bitperpixel	= { 16 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "JPEG compressed format",
		.fourcc		= V4L2_PIX_FMT_JPEG_GRAY,
		.reg_cfg	= 0x00000000,
		.bitperpixel	= { 16 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:4:4 packed, Y/CbCr",
		.fourcc		= V4L2_PIX_FMT_YUV444_2P,
		.reg_cfg	= 0x08000802,
		.bitperpixel	= { 32 },
		.color_planes	= 2,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:4:4 packed, Y/CrCb",
		.fourcc		= V4L2_PIX_FMT_YVU444_2P,
		.reg_cfg	= 0x00000802,
		.bitperpixel	= { 32 },
		.color_planes	= 2,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:4:4 packed, Y/Cb/Cr",
		.fourcc		= V4L2_PIX_FMT_YUV444_3P,
		.reg_cfg	= 0x00000A02,
		.bitperpixel	= { 32 },
		.color_planes	= 3,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, YCrYCb",
		.fourcc		= V4L2_PIX_FMT_YVYU,
		.reg_cfg	= 0x10004003,
		.bitperpixel	= { 16 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, CrYCbY",
		.fourcc		= V4L2_PIX_FMT_VYUY,
		.reg_cfg	= 0x30004003,
		.bitperpixel	= { 16 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.fourcc		= V4L2_PIX_FMT_UYVY,
		.reg_cfg	= 0x20004003,
		.bitperpixel	= { 16 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.fourcc		= V4L2_PIX_FMT_YUYV,
		.reg_cfg	= 0x00004003,
		.bitperpixel	= { 16 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CrCb",
		.fourcc		= V4L2_PIX_FMT_NV61,
		.reg_cfg	= 0x08005003,
		.bitperpixel	= { 16 },
		.color_planes	= 2,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.fourcc		= V4L2_PIX_FMT_NV16,
		.reg_cfg	= 0x00005003,
		.bitperpixel	= { 16 },
		.color_planes	= 2,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.fourcc		= V4L2_PIX_FMT_NV12,
		.reg_cfg	= 0x00020004,
		.bitperpixel	= { 12 },
		.color_planes	= 2,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.fourcc		= V4L2_PIX_FMT_NV21,
		.reg_cfg	= 0x08020004,
		.bitperpixel	= { 12 },
		.color_planes	= 2,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.fourcc		= V4L2_PIX_FMT_NV12M,
		.reg_cfg	= 0x00020004,
		.bitperpixel	= { 8, 4 },
		.color_planes	= 2,
		.mem_planes	= 2,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.fourcc		= V4L2_PIX_FMT_NV21M,
		.reg_cfg	= 0x08020004,
		.bitperpixel	= { 8, 4 },
		.color_planes	= 2,
		.mem_planes	= 2,
	}, {
		.name		= "YUV 4:2:0 contiguous 3-planar, Y/Cb/Cr",
		.fourcc		= V4L2_PIX_FMT_YUV420,
		.reg_cfg	= 0x00028004,
		.bitperpixel	= { 12 },
		.color_planes	= 3,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 3-planar, Y/Cr/Cb",
		.fourcc		= V4L2_PIX_FMT_YVU420,
		.reg_cfg	= 0x00028004,
		.bitperpixel	= { 12 },
		.color_planes	= 3,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.fourcc		= V4L2_PIX_FMT_YUV420M,
		.reg_cfg	= 0x00028004,
		.bitperpixel	= { 8, 2, 2 },
		.color_planes	= 3,
		.mem_planes	= 3,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.fourcc		= V4L2_PIX_FMT_YVU420M,
		.reg_cfg	= 0x00028004,
		.bitperpixel	= { 8, 2, 2 },
		.color_planes	= 3,
		.mem_planes	= 3,

	}, {
		.name		= "YUV 4:2:2V contiguous 2-planar, Y/Cr/Cb",
		.fourcc		= V4L2_PIX_FMT_YUV422V_2P,
		.reg_cfg	= 0x00100004,
		.bitperpixel	= { 16 },
		.color_planes	= 2,
		.mem_planes	= 1,
	}, {
		.name		= "YUV 4:2:2V contiguous 3-planar, Y/Cr/Cb",
		.fourcc		= V4L2_PIX_FMT_YUV422V_3P,
		.reg_cfg	= 0x00140004,
		.bitperpixel	= { 16 },
		.color_planes	= 3,
		.mem_planes	= 1,
	}, {
		.name		= "Gray",
		.fourcc		= V4L2_PIX_FMT_GREY,
		.reg_cfg	= 0x00000020,
		.bitperpixel	= { 32 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "RGB565",
		.fourcc		= V4L2_PIX_FMT_RGB565X,
		.reg_cfg	= 0x00000001,
		.bitperpixel	= { 16 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "ARGB-8-8-8-8, 32 bpp",
		.fourcc		= V4L2_PIX_FMT_RGB32,
		.reg_cfg	= 0x40000141,
		.bitperpixel	= { 32 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "ABGR-8-8-8-8, 32 bpp",
		.fourcc		= V4L2_PIX_FMT_BGR32,
		.reg_cfg	= 0x00000141,
		.bitperpixel	= { 32 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "RGB-8-8-8, 24 bpp",
		.fourcc		= V4L2_PIX_FMT_RGB24,
		.reg_cfg	= 0x00000181,
		.bitperpixel	= { 24 },
		.color_planes	= 1,
		.mem_planes	= 1,
	}, {
		.name		= "BGR-8-8-8, 24 bpp",
		.fourcc		= V4L2_PIX_FMT_BGR24,
		.reg_cfg	= 0x40000181,
		.bitperpixel	= { 24 },
		.color_planes	= 1,
		.mem_planes	= 1,
	},
};

static struct jpeg_fmt *find_format(u32 pixfmt)
{
	struct jpeg_fmt *fmt;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		fmt = &formats[i];
		if (fmt->fourcc == pixfmt)
			break;
	}

	return (i == ARRAY_SIZE(formats)) ? NULL : fmt;
}

static int jpeg_1shotdev_device_run(struct m2m1shot_context *m21ctx,
				struct m2m1shot_task *task)
{
	struct jpeg_ctx *ctx = m21ctx->priv;
	struct jpeg_dev *jpeg = ctx->jpeg_dev;

	if (!jpeg) {
		pr_err("jpeg is null\n");
		return -EINVAL;
	}

	BUG_ON(test_bit(DEV_RUN, &jpeg->state));
	BUG_ON(test_bit(DEV_SUSPEND, &jpeg->state));
	BUG_ON(!test_bit(DEV_RUNTIME_RESUME, &jpeg->state));

	jpeg_sw_reset(jpeg->regs);
	jpeg_set_interrupt(jpeg->regs);

	jpeg_set_huf_table_enable(jpeg->regs, true);
	jpeg_set_stream_size(jpeg->regs, ctx->width, ctx->height);

	if (allow_custom_qtbl &&
		!!(ctx->flags & EXYNOS_JPEG_CTX_CUSTOM_QTBL) &&
			ctx->custom_qtbl) {
		jpeg_set_enc_custom_tbl(jpeg->regs, ctx->custom_qtbl);
	} else {
		jpeg_set_enc_tbl(jpeg->regs, ctx->quality);
	}

	jpeg_set_encode_tbl_select(jpeg->regs);
	jpeg_set_encode_huffman_table(jpeg->regs);

	jpeg_set_image_fmt(jpeg->regs, ctx->in_fmt->reg_cfg |
					ctx->out_fmt->reg_cfg);

	if (is_jpeg_fmt(ctx->out_fmt->fourcc)) {
		jpeg_set_stream_addr(jpeg->regs,
				task->dma_buf_cap.plane[0].dma_addr);
		jpeg_set_image_addr(jpeg->regs, &task->dma_buf_out,
				ctx->in_fmt, ctx->width, ctx->height);
		jpeg_set_encode_hoff_cnt(jpeg->regs, ctx->out_fmt->fourcc);
	} else {
		jpeg_set_stream_addr(jpeg->regs,
				task->dma_buf_out.plane[0].dma_addr);
		jpeg_set_image_addr(jpeg->regs, &task->dma_buf_cap,
				ctx->out_fmt, ctx->width, ctx->height);
		jpeg_alpha_value_set(jpeg->regs, 0xff);
		jpeg_set_dec_bitstream_size(jpeg->regs,
				task->task.buf_out.plane[0].len);
	}

	set_bit(DEV_RUN, &jpeg->state);

	jpeg_set_timer_count(jpeg->regs, ctx->width * ctx->height * 8 + 0xff);

	if (is_jpeg_fmt(ctx->out_fmt->fourcc))
		jpeg_set_enc_dec_mode(jpeg->regs, ENCODING);
	else
		jpeg_set_enc_dec_mode(jpeg->regs, DECODING);

	return 0;
}

static void jpeg_1shotdev_finish_buffer(struct m2m1shot_context *m21ctx,
			struct m2m1shot_buffer_dma *buf_dma,
			int plane,
			enum dma_data_direction dir)
{
	m2m1shot_dma_addr_unmap(m21ctx->m21dev->dev, buf_dma, plane);
	m2m1shot_unmap_dma_buf(m21ctx->m21dev->dev,
				&buf_dma->plane[plane], dir);
}

static int jpeg_1shotdev_prepare_operation(struct m2m1shot_context *m21ctx,
						struct m2m1shot_task *task)
{
	struct jpeg_ctx *ctx = m21ctx->priv;
	struct m2m1shot *shot = &task->task;

	if (shot->reserved[1] != 0) {
		if (!ctx->custom_qtbl)
			ctx->custom_qtbl =
				kmalloc(NUM_QTABLE_VALUES * 2, GFP_KERNEL);
		if (!ctx->custom_qtbl) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Failed to allocate custom q-table\n",
				__func__);
			return -ENOMEM;
		}

		if (copy_from_user(ctx->custom_qtbl,
				(void __user *)shot->reserved[1],
						NUM_QTABLE_VALUES * 2)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Failed to copy q-tble from user\n",
				__func__);
			/* ctx->custom_qtbl is freed in free_context() */
			return -EFAULT;
		}

		ctx->flags |= EXYNOS_JPEG_CTX_CUSTOM_QTBL;
	} else {
		if ((shot->op.quality_level < 0) ||
					(shot->op.quality_level > 100)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: JPEG quality level %d is not supported\n",
				__func__, shot->op.quality_level);
			return -EINVAL;
		}

		ctx->flags &= ~EXYNOS_JPEG_CTX_CUSTOM_QTBL;
	}

	ctx->quality = shot->op.quality_level;

	return 0;
}

static int jpeg_1shotdev_prepare_buffer(struct m2m1shot_context *m21ctx,
			struct m2m1shot_buffer_dma *buf_dma,
			int plane,
			enum dma_data_direction dir)
{
	int ret;

	ret = m2m1shot_map_dma_buf(m21ctx->m21dev->dev,
				&buf_dma->plane[plane], dir);
	if (ret)
		return ret;

	ret = m2m1shot_dma_addr_map(m21ctx->m21dev->dev, buf_dma, plane, dir);
	if (ret) {
		m2m1shot_unmap_dma_buf(m21ctx->m21dev->dev,
					&buf_dma->plane[plane], dir);
		return ret;
	}

	return 0;
}

static int jpeg_1shotdev_prepare_format(struct m2m1shot_context *m21ctx,
			struct m2m1shot_pix_format *fmt,
			enum dma_data_direction dir,
			size_t bytes_used[])
{
	struct jpeg_ctx *ctx = m21ctx->priv;
	struct jpeg_fmt *img_fmt;
	int i;

	img_fmt = find_format(fmt->fmt);
	if (!img_fmt) {
		dev_err(m21ctx->m21dev->dev,
			"%s: Pixel format %#x is not supported for %s\n",
			__func__, fmt->fmt,
			(dir == DMA_TO_DEVICE) ? "output" : "capture");
		return -EINVAL;
	}

	if (dir == DMA_TO_DEVICE)
		ctx->in_fmt = img_fmt;
	else
		ctx->out_fmt = img_fmt;

	if (!fmt->width || !fmt->height) {
		dev_err(m21ctx->m21dev->dev,
			"%s: neither width nor height can be zero\n",
			__func__);
		return -EINVAL;
	}

	if ((fmt->width > 16368) || (fmt->height > 16368)) {
		dev_err(m21ctx->m21dev->dev,
			"%s: requested image size %dx%d exceed 16368x16368\n",
			__func__, fmt->width, fmt->height);
		return -EINVAL;
	}

	ctx->width = fmt->width;
	ctx->height = fmt->height;

	for (i = 0; i < img_fmt->mem_planes; i++) {
		bytes_used[i] = fmt->width * fmt->height;
		bytes_used[i] *= img_fmt->bitperpixel[i];
		bytes_used[i] /= 8;
	}

	return img_fmt->mem_planes;
}

static int jpeg_1shotdev_free_context(struct m2m1shot_context *m21ctx)
{
	struct jpeg_ctx *ctx = m21ctx->priv;

	pm_runtime_put_sync(ctx->jpeg_dev->dev);

	kfree(ctx->custom_qtbl);
	kfree(ctx);

	return 0;
}

static int jpeg_1shotdev_init_context(struct m2m1shot_context *m21ctx)
{
	struct jpeg_dev *jpeg = dev_get_drvdata(m21ctx->m21dev->dev);
	struct jpeg_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->in_fmt = &formats[11];
	ctx->out_fmt = &formats[1];

	ctx->jpeg_dev = jpeg;

	m21ctx->priv = ctx;
	ctx->m21ctx = m21ctx;

	pm_runtime_get_sync(jpeg->dev);

	return 0;
}

static const struct m2m1shot_devops jpeg_oneshot_ops = {
	.init_context = jpeg_1shotdev_init_context,
	.free_context = jpeg_1shotdev_free_context,
	.prepare_format = jpeg_1shotdev_prepare_format,
	.prepare_buffer = jpeg_1shotdev_prepare_buffer,
	.prepare_operation = jpeg_1shotdev_prepare_operation,
	.finish_buffer = jpeg_1shotdev_finish_buffer,
	.device_run = jpeg_1shotdev_device_run,
};

static int jpeg_sysmmu_fault_handler(struct iommu_domain *domain,
				struct device *dev, unsigned long fault_addr,
				int fault_flags, void *p)
{
	struct jpeg_dev *jpeg= p;

	if (test_bit(DEV_RUNTIME_RESUME, &jpeg->state)) {
		pr_info("JPEG dumping registers\n");
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
				jpeg->regs, 0x68, false);
		pr_info("End of JPEG dumping registers\n");
	}
	return 0;
}

static irqreturn_t jpeg_irq_handler(int irq, void *priv)
{
	unsigned int int_status;
	struct jpeg_dev *jpeg = priv;

	spin_lock(&jpeg->slock);

	int_status = jpeg_get_int_status(jpeg->regs);

	jpeg_clean_interrupt(jpeg->regs);
	clear_bit(DEV_RUN, &jpeg->state);

	if (int_status & 0x2) {
		if (jpeg->oneshot_dev) {
			struct m2m1shot_task *task =
				m2m1shot_get_current_task(jpeg->oneshot_dev);
			task->task.buf_cap.plane[0].len =
				jpeg_get_stream_size(jpeg->regs);
			m2m1shot_task_finish(jpeg->oneshot_dev, task, true);
		}
	} else {
		dev_err(jpeg->dev, "JPEG ERROR Interrupt (%#x) is triggered\n",
				int_status);
		jpeg_show_sfr_status(jpeg->regs);
		exynos_sysmmu_show_status(jpeg->dev);
	}

	spin_unlock(&jpeg->slock);

	return IRQ_HANDLED;
}

static int jpeg_clk_get(struct jpeg_dev *jpeg)
{
	struct device *dev = jpeg->dev;
	int i, ret;

	for (i = 0; i < JPEG_CLK_NUM; i++) {
		jpeg->clocks[i] = devm_clk_get(dev, jpeg_clock_names[i]);
		if (IS_ERR(jpeg->clocks[i]) &&
			!(jpeg->clocks[i] == ERR_PTR(-ENOENT))) {
			dev_err(dev, "Failed to get jpeg %s clock\n",
				jpeg_clock_names[i]);
			return PTR_ERR(jpeg->clocks[i]);

		}
	}

	if (!IS_ERR(jpeg->clocks[JPEG_GATE2_CLK])) {
		ret = clk_prepare(jpeg->clocks[JPEG_GATE2_CLK]);
		if (ret) {
			dev_err(dev,
				"%s: failed to prepare GATE2 clock(err %d)\n",
				__func__, ret);
			return ret;
		}
	}

	if (!IS_ERR(jpeg->clocks[JPEG_GATE_CLK])) {
		ret = clk_prepare(jpeg->clocks[JPEG_GATE_CLK]);
		if (ret) {
			if (!IS_ERR(jpeg->clocks[JPEG_GATE2_CLK]))
				clk_unprepare(jpeg->clocks[JPEG_GATE2_CLK]);
			dev_err(dev,
				"%s: failed to prepare GATE clock(err %d)\n",
				__func__, ret);
			return ret;
		}
	}

	return 0;
}

static void jpeg_clk_put(struct jpeg_dev *jpeg)
{
	int i;

	if (!IS_ERR(jpeg->clocks[JPEG_GATE_CLK]))
		clk_unprepare(jpeg->clocks[JPEG_GATE_CLK]);

	if (!IS_ERR(jpeg->clocks[JPEG_GATE2_CLK]))
		clk_unprepare(jpeg->clocks[JPEG_GATE2_CLK]);

	for (i = 0; i < JPEG_CLK_NUM; i++) {
		if (!IS_ERR(jpeg->clocks[i]))
			clk_put(jpeg->clocks[i]);
	}
}

static void jpeg_clock_gating(struct jpeg_dev *jpeg, bool on)
{
	if (on) {
		if (!IS_ERR(jpeg->clocks[JPEG_CHLD1_CLK]) &&
			!IS_ERR(jpeg->clocks[JPEG_PARN1_CLK]))
			if (clk_set_parent(jpeg->clocks[JPEG_CHLD1_CLK],
						jpeg->clocks[JPEG_PARN1_CLK]))
				dev_err(jpeg->dev,
					"Unable to set parent1 of child1\n");
		if (!IS_ERR(jpeg->clocks[JPEG_CHLD2_CLK]) &&
			!IS_ERR(jpeg->clocks[JPEG_PARN2_CLK]))
			if (clk_set_parent(jpeg->clocks[JPEG_CHLD2_CLK],
						jpeg->clocks[JPEG_PARN2_CLK]))
				dev_err(jpeg->dev,
					"Unable to set parent2 of child2\n");
		if (!IS_ERR(jpeg->clocks[JPEG_CHLD3_CLK]) &&
			!IS_ERR(jpeg->clocks[JPEG_PARN3_CLK]))
			if (clk_set_parent(jpeg->clocks[JPEG_CHLD3_CLK],
						jpeg->clocks[JPEG_PARN3_CLK]))
				dev_err(jpeg->dev,
					"Unable to set parent3 of child3\n");
		if (!IS_ERR(jpeg->clocks[JPEG_GATE2_CLK])) {
			clk_enable(jpeg->clocks[JPEG_GATE2_CLK]);
			dev_dbg(jpeg->dev, "jpeg clock2 enabled\n");
		}
		if (!IS_ERR(jpeg->clocks[JPEG_GATE_CLK])) {
			clk_enable(jpeg->clocks[JPEG_GATE_CLK]);
			dev_dbg(jpeg->dev, "jpeg clock enabled\n");
		}
	} else {
		if (!IS_ERR(jpeg->clocks[JPEG_GATE_CLK])) {
			clk_disable(jpeg->clocks[JPEG_GATE_CLK]);
			dev_dbg(jpeg->dev, "jpeg clock disabled\n");
		}
		if (!IS_ERR(jpeg->clocks[JPEG_GATE2_CLK])) {
			clk_disable(jpeg->clocks[JPEG_GATE2_CLK]);
			dev_dbg(jpeg->dev, "jpeg clock2 disabled\n");
		}
	}
}

static int jpeg_probe(struct platform_device *pdev)
{
	struct jpeg_dev *jpeg;
	struct resource *res;
	int i, ret;

	jpeg = devm_kzalloc(&pdev->dev, sizeof(struct jpeg_dev), GFP_KERNEL);
	if (!jpeg) {
		dev_err(&pdev->dev, "%s: not enough memory\n", __func__);
		return -ENOMEM;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "ip_ver", &jpeg->ver);
	if (ret) {
		dev_err(&pdev->dev, "%s: ip_ver doesn't exist\n", __func__);
		return -EINVAL;
	}

	jpeg->dev = &pdev->dev;

	spin_lock_init(&jpeg->slock);

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	jpeg->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (jpeg->regs == NULL) {
		dev_err(&pdev->dev, "failed to claim register region\n");
		return -ENOENT;
	}

	/* Get IRQ resource and register IRQ handler. */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get IRQ resource\n");
		return -ENXIO;
	}

	/* Get memory resource and map SFR region. */
	ret = devm_request_irq(&pdev->dev, res->start,
			(void *)jpeg_irq_handler, 0,
			pdev->name, jpeg);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq\n");
		return ret;
	}

	/* clock */
	for (i = 0; i < JPEG_CLK_NUM; i++)
		jpeg->clocks[i] = ERR_PTR(-ENOENT);

	ret = jpeg_clk_get(jpeg);
	if (ret)
		return ret;

	jpeg->oneshot_dev = m2m1shot_create_device(&pdev->dev,
		&jpeg_oneshot_ops, "jpeg", pdev->id, -1);
	if (IS_ERR(jpeg->oneshot_dev)) {
		pr_err("%s: Failed to create m2m1shot device\n", __func__);
		ret = PTR_ERR(jpeg->oneshot_dev);
		goto err_m2m1shot;
	}

	platform_set_drvdata(pdev, jpeg);

	ret = exynos_create_iovmm(&pdev->dev, 3, 3);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Failed(%d) to create IOVMM\n", __func__, ret);
		goto err_iovmm;
	}

	ret = iovmm_activate(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Failed(%d) to activate IOVMM\n", __func__, ret);
		/* nothing to do for exynos_create_iovmm() */
		goto err_iovmm;
	}

	iovmm_set_fault_handler(&pdev->dev,
			jpeg_sysmmu_fault_handler, jpeg);

	pm_runtime_enable(&pdev->dev);
	if (!IS_ENABLED(CONFIG_PM_RUNTIME)) {
		jpeg_clock_gating(jpeg, true);
		set_bit(DEV_RUNTIME_RESUME, &jpeg->state);
	}

	dev_info(&pdev->dev, "JPEG driver register successfully");
	return 0;
err_iovmm:
	m2m1shot_destroy_device(jpeg->oneshot_dev);
err_m2m1shot:
	jpeg_clk_put(jpeg);
	return ret;
}

static int jpeg_remove(struct platform_device *pdev)
{
	struct jpeg_dev *jpeg = platform_get_drvdata(pdev);

	jpeg_clk_put(jpeg);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int jpeg_suspend(struct device *dev)
{
	struct jpeg_dev *jpeg = dev_get_drvdata(dev);

	set_bit(DEV_SUSPEND, &jpeg->state);
	return 0;
}

static int jpeg_resume(struct device *dev)
{
	struct jpeg_dev *jpeg = dev_get_drvdata(dev);

	clear_bit(DEV_SUSPEND, &jpeg->state);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int jpeg_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct jpeg_dev *jpeg = platform_get_drvdata(pdev);

	jpeg_clock_gating(jpeg, false);
	clear_bit(DEV_RUNTIME_RESUME, &jpeg->state);

	return 0;
}

static int jpeg_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct jpeg_dev *jpeg = platform_get_drvdata(pdev);

	jpeg_clock_gating(jpeg, true);
	set_bit(DEV_RUNTIME_RESUME, &jpeg->state);

	return 0;
}
#endif


static const struct dev_pm_ops jpeg_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(jpeg_suspend, jpeg_resume)
	SET_RUNTIME_PM_OPS(jpeg_runtime_suspend, jpeg_runtime_resume, NULL)
};

static const struct of_device_id exynos_jpeg_match[] = {
	{
		.compatible = "samsung,exynos-jpeg",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_jpeg_match);

static struct platform_driver jpeg_driver = {
	.probe		= jpeg_probe,
	.remove		= jpeg_remove,
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &jpeg_pm_ops,
		.of_match_table = of_match_ptr(exynos_jpeg_match),
	}
};

module_platform_driver(jpeg_driver);

MODULE_AUTHOR("khw0178.kim@samsung.com>");
MODULE_DESCRIPTION("H/W JPEG Device Driver");
MODULE_LICENSE("GPL");
