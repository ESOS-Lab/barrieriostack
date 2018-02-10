/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_shm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>

#include "s5p_mfc_common.h"

#include "s5p_mfc_mem.h"
#include "s5p_mfc_debug.h"

int s5p_mfc_init_shm(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_buf_size_v5 *buf_size = dev->variant->buf_size->buf;
	void *shm_alloc_ctx = dev->alloc_ctx[MFC_BANK_A_ALLOC_CTX];

	if (ctx->is_drm)
		shm_alloc_ctx = dev->alloc_ctx_sh;

	ctx->shm.alloc = s5p_mfc_mem_alloc_priv(shm_alloc_ctx,
						buf_size->shared_buf);
	if (IS_ERR(ctx->shm.alloc)) {
		mfc_err("failed to allocate shared memory\n");
		return PTR_ERR(ctx->shm.alloc);
	}

	/* shm_ofs only keeps the offset from base (port a) */
	ctx->shm.ofs = s5p_mfc_mem_daddr_priv(ctx->shm.alloc) - dev->port_a;
	ctx->shm.virt = s5p_mfc_mem_vaddr_priv(ctx->shm.alloc);
	if (!ctx->shm.virt) {
		s5p_mfc_mem_free_priv(ctx->shm.alloc);
		ctx->shm.ofs = 0;
		ctx->shm.alloc = NULL;

		mfc_err("failed to virt addr of shared memory\n");
		return -ENOMEM;
	}

	memset((void *)ctx->shm.virt, 0, buf_size->shared_buf);
	s5p_mfc_mem_clean_priv(ctx->shm.alloc, ctx->shm.virt, 0,
			buf_size->shared_buf);

	mfc_debug(2, "shm info addr: 0x%08x, phys: 0x%08lx\n",
		 (unsigned int)ctx->shm.virt, ctx->shm.ofs);

	return 0;
}

