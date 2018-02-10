/* linux/drivers/media/video/exynos/fimg2d/fimg2d_ctx.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

int qos_id;

static inline bool is_yuvfmt(enum color_format fmt)
{
	switch (fmt) {
	case CF_YCBCR_420:
	case CF_YCBCR_422:
	case CF_YCBCR_444:
	case CF_L8:
		return true;
	default:
		return false;
	}
}

int bit_per_pixel(struct fimg2d_image *img, int plane)
{
	switch (img->fmt) {
	case CF_XRGB_8888:
	case CF_ARGB_8888:
	case CF_MSK_32BIT_8888:
		return 32;
	case CF_RGB_888:
		return 24;
	case CF_RGB_565:
	case CF_XRGB_1555:
	case CF_ARGB_1555:
	case CF_XRGB_4444:
	case CF_ARGB_4444:
	case CF_MSK_16BIT_565:
	case CF_MSK_16BIT_1555:
	case CF_MSK_16BIT_4444:
		return 16;
	case CF_YCBCR_420:
		return 8;
	case CF_YCBCR_422:
		if (img->order == P2_CRCB || img->order == P2_CBCR)
			return 8;
		else
			return (!plane) ? 16 : 0;
	case CF_YCBCR_444:
		return (!plane) ? 8 : 16;
	case CF_A8:
	case CF_L8:
	case CF_MSK_8BIT:
		return 8;
	case CF_MSK_4BIT:
		return 4;
	case CF_MSK_1BIT:
		return 1;
	default:
		return 0;
	}
}

static inline int pixel2offset(int x1, int bpp)
{
	return (x1 * bpp) >> 3;
}

static inline int image_stride(struct fimg2d_image *img, int plane)
{
	switch (img->fmt) {
	case CF_YCBCR_420:
		return img->width;
	case CF_YCBCR_422:
		if (img->order == P2_CRCB || img->order == P2_CBCR)
			return img->width;
		else
			return (!plane) ? img->width * 2 : 0;
	case CF_YCBCR_444:
		return (!plane) ? img->width : img->width * 2;
	default:
		return (!plane) ? img->stride : 0;
	}
}

static int fimg2d_check_address_range(unsigned long addr, size_t size)
{
	struct vm_area_struct *vma;
	int ret = 0;

	if (addr + size <= addr) {
		fimg2d_err("address overflow. addr:%#lx, size:%zd\n",
				addr, size);
		return -EINVAL;
	}

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, addr);

	/* traverse contiguous vmas list until getting the last one */
	while (vma && (vma->vm_end < addr + size) &&
		vma->vm_next && (vma->vm_next->vm_start == vma->vm_end)) {
		vma = vma->vm_next;
	}

	if (!vma) {
		fimg2d_err("vma is invalid for %zd bytes@%#lx\n", size, addr);
		ret = -EFAULT;
	}

	if (vma->vm_end < addr + size) {
		fimg2d_err("vma %#lx--%#lx overflow for %zd bytes@%#lx\n",
				vma->vm_start, vma->vm_end, size, addr);
		ret = -EFAULT;
	}

	up_read(&current->mm->mmap_sem);

	if (!ret) {
		/*
		 * Invoking COW gainst the first and last page if they can be
		 * accessed by CPU and G2D concurrently.
		 * checking the return value of get_user_pages_fast() is not
		 * required because the calls to get_user_pages_fast() are to
		 * invoke COW so that COW is not invoked against the first and
		 * the last pages while G2D is working.
		 */
		if (!IS_ALIGNED(addr, PAGE_SIZE))
			get_user_pages_fast(addr, 1, 1, NULL);

		if (!IS_ALIGNED(addr + size, PAGE_SIZE))
			get_user_pages_fast(addr + size, 1, 1, NULL);
	}

	return ret;
}

static int fimg2d_check_address(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_dma *c;
	struct fimg2d_image *img;
	int i;

	for (i = 0; i < MAX_IMAGES; i++) {
		img = &cmd->image[i];
		if (!img->addr.type)
			continue;

		c = &cmd->dma[i].base;

		if (fimg2d_check_address_range(c->addr, c->size))
			return -EINVAL;

		/* 2nd plane */
		if (!is_yuvfmt(img->fmt))
			continue;

		if (img->order != P2_CRCB && img->order != P2_CBCR)
			continue;

		c = &cmd->dma[i].plane2;

		if (fimg2d_check_address_range(c->addr, c->size))
			return -EINVAL;
	}
	return 0;
}

static int fimg2d_check_params(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_image *img;
	struct fimg2d_scale *scl;
	struct fimg2d_clip *clp;
	struct fimg2d_rect *r;
	enum addr_space addr_type;
	int w, h, i;

	/* dst is mandatory */
	if (WARN_ON(!blt->dst || !blt->dst->addr.type))
		return -1;

	addr_type = blt->dst->addr.type;

	if ((blt->src && blt->src->addr.type != addr_type) ||
		(blt->msk && blt->msk->addr.type != addr_type) ||
		(blt->tmp && blt->tmp->addr.type != addr_type))
		return -1;

	/* DST op makes no effect */
	if (blt->op < 0 || blt->op == BLIT_OP_DST || blt->op >= BLIT_OP_END)
		return -1;

	for (i = 0; i < MAX_IMAGES; i++) {
		img = &cmd->image[i];
		if (!img->addr.type)
			continue;

		w = img->width;
		h = img->height;
		r = &img->rect;

		/* 8000: max width & height */
		if (w > 8000 || h > 8000)
			return -1;

		if (r->x1 < 0 || r->y1 < 0 ||
			r->x1 >= w || r->y1 >= h ||
			r->x1 >= r->x2 || r->y1 >= r->y2)
			return -1;

		/* DO support only UVA */
		if (img->addr.type != ADDR_USER)
			return -1;
	}

	clp = &blt->param.clipping;
	if (clp->enable) {
		img = &cmd->image[IDST];

		w = img->width;
		h = img->height;
		r = &img->rect;

		if (clp->x1 < 0 || clp->y1 < 0 ||
			clp->x1 >= w || clp->y1 >= h ||
			clp->x1 >= clp->x2 || clp->y1 >= clp->y2 ||
			clp->x1 >= r->x2 || clp->x2 <= r->x1 ||
			clp->y1 >= r->y2 || clp->y2 <= r->y1)
			return -1;
	}

	scl = &blt->param.scaling;
	if (scl->mode) {
		if (!scl->src_w || !scl->src_h || !scl->dst_w || !scl->dst_h)
			return -1;
	}

	return 0;
}

static void fimg2d_fixup_params(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_image *img;
	struct fimg2d_scale *scl;
	struct fimg2d_clip *clp;
	struct fimg2d_rect *r;
	int i;

	clp = &blt->param.clipping;
	scl = &blt->param.scaling;

	/* fix dst/clip rect */
	for (i = 0; i < MAX_IMAGES; i++) {
		img = &cmd->image[i];
		if (!img->addr.type)
			continue;

		r = &img->rect;

		if (i == IMAGE_DST && clp->enable) {
			if (clp->x2 > img->width)
				clp->x2 = img->width;
			if (clp->y2 > img->height)
				clp->y2 = img->height;
		} else {
			if (r->x2 > img->width)
				r->x2 = img->width;
			if (r->y2 > img->height)
				r->y2 = img->height;
		}
	}

	/* avoid devided-by-zero */
	if (scl->mode &&
		(scl->src_w == scl->dst_w && scl->src_h == scl->dst_h))
		scl->mode = NO_SCALING;
}

static int fimg2d_calc_dma_size(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_image *img;
	struct fimg2d_clip *clp;
	struct fimg2d_rect *r;
	struct fimg2d_dma *c;
	enum addr_space addr_type;
	int i, y1, y2, stride, clp_h, bpp;

	addr_type = blt->dst->addr.type;

	if (addr_type != ADDR_USER && addr_type != ADDR_USER_CONTIG)
		return -1;

	clp = &blt->param.clipping;

	for (i = 0; i < MAX_IMAGES; i++) {
#ifdef FIMG2D_IOVMM_PAGETABLE
		exynos_iova_t iova = FIMG2D_IOVA_START +
					(FIMG2D_IOVA_PLANE_SIZE *
					 FIMG2D_IOVA_MAX_PLANES) * i;
#endif
		img = &cmd->image[i];

		r = &img->rect;

		if (i == IMAGE_DST && clp->enable) {
			y1 = clp->y1;
			y2 = clp->y2;
		} else {
			y1 = r->y1;
			y2 = r->y2;
		}

		/* 1st plane */
		bpp = bit_per_pixel(img, 0);
		stride = width2bytes(img->width, bpp);

		clp_h = y2 - y1;

		c = &cmd->dma[i].base;
		c->addr = img->addr.start + (stride * y1);
		c->size = stride * clp_h;
		c->offset = stride * y1;
#ifdef FIMG2D_IOVMM_PAGETABLE
		c->iova = iova + (img->addr.start & ~PAGE_MASK);
#else
		c->iova = c->addr;
#endif
		if (img->need_cacheopr) {
			c->cached = c->size;
			cmd->dma_all += c->cached;
		}

		if (i == IDST)
			fimg2d_debug("addr : %p, size : %zd\n",
					(void *)c->addr, c->size);

		if (!is_yuvfmt(img->fmt))
			continue;

		/* 2nd plane */
		if (img->order == P2_CRCB || img->order == P2_CBCR) {
			bpp = bit_per_pixel(img, 1);
			stride = width2bytes(img->width, bpp);
			if (img->fmt == CF_YCBCR_420)
				clp_h = (y2 - y1)/2;
			else
				clp_h = y2 - y1;

			c = &cmd->dma[i].plane2;
			c->addr = img->plane2.start + (stride * y1);
			c->size = stride * clp_h;
			c->offset = stride * y1;
#ifdef FIMG2D_IOVMM_PAGETABLE
			c->iova = iova + FIMG2D_IOVA_PLANE_SIZE +
					(img->plane2.start & ~PAGE_MASK);
#else
			c->iova = c->addr;
#endif
			if (img->need_cacheopr) {
				c->cached = c->size;
				cmd->dma_all += c->cached;
			}
		}
	}

	return 0;
}

#ifndef CCI_SNOOP
static void inner_flush_clip_range(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_image *img;
	struct fimg2d_clip *clp;
	struct fimg2d_rect *r;
	struct fimg2d_dma *c;
	int i, y, clp_x, clp_w, clp_h, dir;
	int x1, y1, x2, y2, bpp, stride;
	unsigned long start;

	clp = &blt->param.clipping;
	dir = DMA_TO_DEVICE;

	for (i = 0; i < MAX_IMAGES; i++) {
		if (i == IMAGE_DST)
			dir = DMA_BIDIRECTIONAL;

		img = &cmd->image[i];

		r = &img->rect;

		/* 1st plane */
		c = &cmd->dma[i].base;
		if (!c->cached)
			continue;

		if (i == IMAGE_DST && clp->enable) {
			x1 = clp->x1;
			y1 = clp->y1;
			x2 = clp->x2;
			y2 = clp->y2;
		} else {
			x1 = r->x1;
			y1 = r->y1;
			x2 = r->x2;
			y2 = r->y2;
		}

		bpp = bit_per_pixel(img, 0);
		stride = width2bytes(img->width, bpp);

		clp_x = pixel2offset(x1, bpp);
		clp_w = width2bytes(x2 - x1, bpp);
		clp_h = y2 - y1;

		if (is_inner_flushrange(stride - clp_w))
			fimg2d_dma_sync_inner(c->addr, c->cached, dir);
		else {
			for (y = 0; y < clp_h; y++) {
				start = c->addr + (stride * y) + clp_x;
				fimg2d_dma_sync_inner(start, clp_w, dir);
			}
		}

		/* 2nd plane */
		if (!is_yuvfmt(img->fmt))
			continue;

		if (img->order != P2_CRCB && img->order != P2_CBCR)
			continue;

		c = &cmd->dma[i].plane2;
		if (!c->cached)
			continue;

		bpp = bit_per_pixel(img, 1);
		stride = width2bytes(img->width, bpp);

		clp_x = pixel2offset(x1, bpp);
		clp_w = width2bytes(x2 - x1, bpp);
		if (img->fmt == CF_YCBCR_420)
			clp_h = (y2 - y1)/2;
		else
			clp_h = y2 - y1;

		if (is_inner_flushrange(stride - clp_w))
			fimg2d_dma_sync_inner(c->addr, c->cached, dir);
		else {
			for (y = 0; y < clp_h; y++) {
				start = c->addr + (stride * y) + clp_x;
				fimg2d_dma_sync_inner(c->addr, c->cached, dir);
			}
		}
	}
}

static void inner_touch_range(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_dma *c;
	int i;

	for (i = 0; i < MAX_IMAGES; i++) {
		img = &cmd->image[i];

		/* 1st plane */
		c = &cmd->dma[i].base;
		if (!c->cached)
			continue;

		fimg2d_touch_range(c->addr, c->cached);

		/* 2nd plane */
		if (!is_yuvfmt(img->fmt))
			continue;

		if (img->order != P2_CRCB && img->order != P2_CBCR)
			continue;

		c = &cmd->dma[i].plane2;
		if (!c->cached)
			continue;

		fimg2d_touch_range(c->addr, c->cached);
	}
}
#endif

#ifdef CONFIG_OUTER_CACHE
static void outer_flush_clip_range(struct fimg2d_bltcmd *cmd)
{
	struct mm_struct *mm;
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_image *img;
	struct fimg2d_clip *clp;
	struct fimg2d_rect *r;
	struct fimg2d_dma *c;
	int clp_x, clp_w, clp_h, y, i, dir;
	int x1, y1, x2, y2;
	unsigned long start;

	if (WARN_ON(!cmd->ctx))
		return;

	mm = cmd->ctx->mm;

	clp = &blt->param.clipping;
	dir = CACHE_CLEAN;

	for (i = 0; i < MAX_IMAGES; i++) {
		img = &cmd->image[i];

		/* clean pagetable on outercache */
		c = &cmd->dma[i].base;
		if (c->size)
			fimg2d_clean_outer_pagetable(mm, c->addr, c->size);

		c = &cmd->dma[i].plane2;
		if (c->size)
			fimg2d_clean_outer_pagetable(mm, c->addr, c->size);

		if (i == IMAGE_DST)
			dir = CACHE_FLUSH;

		/* 1st plane */
		c = &cmd->dma[i].base;
		if (!c->cached)
			continue;

		r = &img->rect;

		if (i == IMAGE_DST && clp->enable) {
			x1 = clp->x1;
			y1 = clp->y1;
			x2 = clp->x2;
			y2 = clp->y2;
		} else {
			x1 = r->x1;
			y1 = r->y1;
			x2 = r->x2;
			y2 = r->y2;
		}

		bpp = bit_per_pixel(img, 0);
		stride = width2bytes(img->width, bpp);

		clp_x = pixel2offset(x1, bpp);
		clp_w = width2bytes(x2 - x1, bpp);
		clp_h = y2 - y1;

		if (is_outer_flushrange(stride - clp_w))
			fimg2d_dma_sync_outer(mm, c->addr, c->cached, dir);
		else {
			for (y = 0; y < clp_h; y++) {
				start = c->addr + (stride * y) + clp_x;
				fimg2d_dma_sync_outer(mm, start, clp_w, dir);
			}
		}

		/* 2nd plane */
		if (!is_yuvfmt(img->fmt))
			continue;

		if (img->order != P2_CRCB && img->order != P2_CBCR)
			continue;

		c = &cmd->dma[i].plane2;
		if (!c->cached)
			continue;

		bpp = bit_per_pixel(img, 1);
		stride = width2bytes(img->width, bpp);

		clp_x = pixel2offset(x1, bpp);
		clp_w = width2bytes(x2 - x1, bpp);
		if (img->fmt == CF_YCBCR_420)
			clp_h = (y2 - y1)/2;
		else
			clp_h = y2 - y1;

		if (is_outer_flushrange(stride - clp_w))
			fimg2d_dma_sync_outer(mm, c->addr, c->cached, dir);
		else {
			for (y = 0; y < clp_h; y++) {
				start = c->addr + (stride * y) + clp_x;
				fimg2d_dma_sync_outer(mm, start, clp_w, dir);
			}
		}
	}
}
#endif /* CONFIG_OUTER_CACHE */

static int fimg2d_check_dma_sync(struct fimg2d_bltcmd *cmd)
{
	struct mm_struct *mm = cmd->ctx->mm;

	fimg2d_calc_dma_size(cmd);

	if (fimg2d_check_address(cmd))
		return -EINVAL;

	if (fimg2d_check_pgd(mm, cmd))
		return -EFAULT;

#ifndef CCI_SNOOP
	fimg2d_debug("cache flush\n");
	perf_start(cmd, PERF_CACHE);
	if (is_inner_flushall(cmd->dma_all)) {
		inner_touch_range(cmd);
		flush_all_cpu_caches();
	} else {
		inner_flush_clip_range(cmd);
	}

#ifdef CONFIG_OUTER_CACHE
	if (is_outer_flushall(cmd->dma_all))
		outer_flush_all();
	else
		outer_flush_clip_range(cmd);
#endif
	perf_end(cmd, PERF_CACHE);
#endif
	return 0;
}

int fimg2d_check_pgd(struct mm_struct *mm, struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_dma *c;
	struct fimg2d_image *img;
	enum pt_status pt;
	int i, ret;


	for (i = 0; i < MAX_IMAGES; i++) {
		img = &cmd->image[i];
		if (!img->addr.type)
			continue;

		c = &cmd->dma[i].base;
		if (!c->size)
			continue;

		pt = fimg2d_check_pagetable(mm, c->addr, c->size, i == IDST);
		if (pt == PT_FAULT) {
			ret = -EFAULT;
			goto err_pgtable;
		}

		/* 2nd plane */
		if (!is_yuvfmt(img->fmt))
			continue;

		if (img->order != P2_CRCB && img->order != P2_CBCR)
			continue;

		c = &cmd->dma[i].plane2;
		if (!c->size)
			continue;

		pt = fimg2d_check_pagetable(mm, c->addr, c->size, i == IDST);
		if (pt == PT_FAULT) {
			ret = -EFAULT;
			goto err_pgtable;
		}
	}
	return 0;

err_pgtable:
	return ret;
}

int fimg2d_add_command(struct fimg2d_control *ctrl,
		struct fimg2d_context *ctx, struct fimg2d_blit __user *buf)
{
	unsigned long flags, qflags;
	struct fimg2d_blit *blt;
	struct fimg2d_bltcmd *cmd;
	int len = sizeof(struct fimg2d_image);
	int ret = 0;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	if (copy_from_user(&cmd->blt, buf, sizeof(cmd->blt))) {
		ret = -EFAULT;
		goto err;
	}

	cmd->ctx = ctx;

	blt = &cmd->blt;

	if (blt->src) {
		if (copy_from_user(&cmd->image[ISRC], blt->src, len)) {
			ret = -EFAULT;
			goto err;
		}
		blt->src = &cmd->image[ISRC];
	}

	if (blt->msk) {
		if (copy_from_user(&cmd->image[IMSK], blt->msk, len)) {
			ret = -EFAULT;
			goto err;
		}
		blt->msk = &cmd->image[IMSK];
	}

	if (blt->tmp) {
		if (copy_from_user(&cmd->image[ITMP], blt->tmp, len)) {
			ret = -EFAULT;
			goto err;
		}
		blt->tmp = &cmd->image[ITMP];
	}

	if (blt->dst) {
		if (copy_from_user(&cmd->image[IDST], blt->dst, len)) {
			ret = -EFAULT;
			goto err;
		}
		blt->dst = &cmd->image[IDST];
	}

	fimg2d_dump_command(cmd);

	perf_start(cmd, PERF_TOTAL);

	if (fimg2d_check_params(cmd)) {
		ret = -EINVAL;
		goto err;
	}

	fimg2d_fixup_params(cmd);

	if (fimg2d_check_dma_sync(cmd)) {
		ret = -EFAULT;
		goto err;
	}

	g2d_spin_lock(&ctrl->qoslock, qflags);
	if ((blt->qos_lv >= G2D_LV0) && (blt->qos_lv < G2D_LV_END)) {
		ctrl->pre_qos_lv = ctrl->qos_lv;
		ctrl->qos_lv = blt->qos_lv;
		fimg2d_debug("pre_qos_lv:%d, qos_lv:%d qos_id:%d\n",
				ctrl->pre_qos_lv, ctrl->qos_lv, blt->qos_lv);
	} else {
		fimg2d_err("invalid qos_lv:0x%x\n", blt->qos_lv);
		g2d_spin_unlock(&ctrl->qoslock, qflags);
		ret = -EINVAL;
		goto err;
	}
	g2d_spin_unlock(&ctrl->qoslock, qflags);

	/* add command node and increase ncmd */
	g2d_spin_lock(&ctrl->bltlock, flags);
	if (atomic_read(&ctrl->drvact) || atomic_read(&ctrl->suspended)) {
		fimg2d_debug("driver is unavailable, do sw fallback\n");
		g2d_spin_unlock(&ctrl->bltlock, flags);
		ret = -EPERM;
		goto err;
	}
	atomic_inc(&ctx->ncmd);
	fimg2d_enqueue(&cmd->node, &ctrl->cmd_q);
	fimg2d_debug("ctx %p pgd %p ncmd(%d) seq_no(%u)\n",
			cmd->ctx, (unsigned long *)cmd->ctx->mm->pgd,
			atomic_read(&ctx->ncmd), cmd->blt.seq_no);
	g2d_spin_unlock(&ctrl->bltlock, flags);

	return 0;

err:
	kfree(cmd);
	return ret;
}

void fimg2d_del_command(struct fimg2d_control *ctrl, struct fimg2d_bltcmd *cmd)
{
	unsigned long flags;
	struct fimg2d_context *ctx = cmd->ctx;

	perf_end(cmd, PERF_TOTAL);
	perf_print(cmd);
	g2d_spin_lock(&ctrl->bltlock, flags);
	fimg2d_dequeue(&cmd->node);
	kfree(cmd);
	atomic_dec(&ctx->ncmd);
#ifdef BLIT_WORKQUE
	/* wake up context */
	if (!atomic_read(&ctx->ncmd))
		wake_up(&ctx->wait_q);
#endif
	g2d_spin_unlock(&ctrl->bltlock, flags);
}

struct fimg2d_bltcmd *fimg2d_get_command(struct fimg2d_control *ctrl)
{
	unsigned long flags;
	struct fimg2d_bltcmd *cmd;

	g2d_spin_lock(&ctrl->bltlock, flags);
	cmd = fimg2d_get_first_command(ctrl);
	g2d_spin_unlock(&ctrl->bltlock, flags);
	return cmd;
}

void fimg2d_add_context(struct fimg2d_control *ctrl, struct fimg2d_context *ctx)
{
	atomic_set(&ctx->ncmd, 0);
	init_waitqueue_head(&ctx->wait_q);

	atomic_inc(&ctrl->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&ctrl->nctx));
}

void fimg2d_del_context(struct fimg2d_control *ctrl, struct fimg2d_context *ctx)
{
	atomic_dec(&ctrl->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&ctrl->nctx));
}
