/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_mem.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_MEM_H_
#define __S5P_MFC_MEM_H_ __FILE__

#include <linux/platform_device.h>
#include <media/videobuf2-ion.h>

/* Offset base used to differentiate between CAPTURE and OUTPUT
*  while mmaping */
#define DST_QUEUE_OFF_BASE      (TASK_SIZE / 2)

#define MFC_BANK_A_ALLOC_CTX	0
#define MFC_BANK_B_ALLOC_CTX	1

#define MFC_FW_ALLOC_CTX	MFC_BANK_A_ALLOC_CTX

static inline dma_addr_t s5p_mfc_mem_plane_addr(
	struct s5p_mfc_ctx *c, struct vb2_buffer *v, unsigned int n)
{
	void *cookie = vb2_plane_cookie(v, n);
	dma_addr_t addr = 0;

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (c->is_drm)
		WARN_ON(vb2_ion_phys_address(cookie,
					(phys_addr_t *)&addr) != 0);
	else
		WARN_ON(vb2_ion_dma_address(cookie, &addr) != 0);
#else
	WARN_ON(vb2_ion_dma_address(cookie, &addr) != 0);
#endif

	return (unsigned long)addr;
}

static inline void *s5p_mfc_mem_alloc_priv(void *alloc_ctx, size_t size)
{
	return vb2_ion_private_alloc(alloc_ctx, size, 0, 0);
}

static inline void s5p_mfc_mem_free_priv(void *cookie)
{
	vb2_ion_private_free(cookie);
}

static inline dma_addr_t s5p_mfc_mem_daddr_priv(void *cookie)
{
	dma_addr_t addr = 0;

	WARN_ON(vb2_ion_dma_address(cookie, &addr) != 0);

	return addr;
}

static inline void *s5p_mfc_mem_vaddr_priv(void *cookie)
{
	return vb2_ion_private_vaddr(cookie);
}

static inline int s5p_mfc_mem_prepare(struct vb2_buffer *vb)
{
	return vb2_ion_buf_prepare(vb);
}

static inline int s5p_mfc_mem_finish(struct vb2_buffer *vb)
{
	return vb2_ion_buf_finish(vb);
}

struct vb2_mem_ops *s5p_mfc_mem_ops(void);
void **s5p_mfc_mem_init_multi(struct device *dev, unsigned int ctx_num);
void s5p_mfc_mem_cleanup_multi(void **alloc_ctxes, unsigned int ctx_num);

void s5p_mfc_mem_set_cacheable(void *alloc_ctx, bool cacheable);
void s5p_mfc_mem_clean_priv(void *vb_priv, void *start, off_t offset,
							size_t size);
void s5p_mfc_mem_inv_priv(void *vb_priv, void *start, off_t offset,
							size_t size);
int s5p_mfc_mem_clean_vb(struct vb2_buffer *vb, u32 num_planes);
int s5p_mfc_mem_inv_vb(struct vb2_buffer *vb, u32 num_planes);
int s5p_mfc_mem_flush_vb(struct vb2_buffer *vb, u32 num_planes);

void s5p_mfc_mem_suspend(void *alloc_ctx);
int s5p_mfc_mem_resume(void *alloc_ctx);
#endif /* __S5P_MFC_MEM_H_ */
