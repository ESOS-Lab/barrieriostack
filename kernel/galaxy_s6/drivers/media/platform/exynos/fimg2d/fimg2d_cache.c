/* linux/drivers/media/video/exynos/fimg2d/fimg2d_cache.c
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

#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>

#include "fimg2d.h"
#include "fimg2d_cache.h"

#define LV1_SHIFT		20
#define LV1_PT_SIZE		SZ_1M
#define LV2_PT_SIZE		SZ_1K
#define LV2_BASE_MASK		0x3ff
#define LV2_PT_MASK		0xff000
#define LV2_SHIFT		12
#define LV1_DESC_MASK		0x3
#define LV2_DESC_MASK		0x2

int fimg2d_fixup_user_fault(unsigned long address)
{
	int ret;

	pr_info("%s: fault @ %#lx while cache flush\n", __func__, address);
	ret = fixup_user_fault(current, current->mm, address, 0);
	if (ret)
		pr_info("%s: failed to fixup fault @ %#lx\n", __func__, address);
	else
		pr_info("%s: successed fixup fault @ %#lx\n", __func__, address);

	return ret;
}

static inline unsigned long virt2phys(struct mm_struct *mm, unsigned long vaddr)
{
	unsigned long *pgd;
	unsigned long *lv1d, *lv2d;

	pgd = (unsigned long *)mm->pgd;

	lv1d = pgd + (vaddr >> LV1_SHIFT);

	if ((*lv1d & LV1_DESC_MASK) != 0x1) {
		fimg2d_debug("invalid LV1 descriptor, "
				"pgd %p lv1d 0x%lx vaddr 0x%lx\n",
				pgd, *lv1d, vaddr);
		return 0;
	}

	lv2d = (unsigned long *)phys_to_virt(*lv1d & ~LV2_BASE_MASK) +
				((vaddr & LV2_PT_MASK) >> LV2_SHIFT);

	if ((*lv2d & LV2_DESC_MASK) != 0x2) {
		fimg2d_debug("invalid LV2 descriptor, "
				"pgd %p lv2d 0x%lx vaddr 0x%lx\n",
				pgd, *lv2d, vaddr);
		return 0;
	}

	return (*lv2d & PAGE_MASK) | (vaddr & (PAGE_SIZE-1));
}

#ifdef CONFIG_OUTER_CACHE
void fimg2d_dma_sync_outer(struct mm_struct *mm, unsigned long vaddr,
					size_t size, enum cache_opr opr)
{
	int len;
	unsigned long cur, end, next, paddr;

	cur = vaddr;
	end = vaddr + size;

	if (opr == CACHE_CLEAN) {
		while (cur < end) {
			next = (cur + PAGE_SIZE) & PAGE_MASK;
			if (next > end)
				next = end;
			len = next - cur;

			paddr = virt2phys(mm, cur);
			if (paddr)
				outer_clean_range(paddr, paddr + len);
			cur += len;
		}
	} else if (opr == CACHE_FLUSH) {
		while (cur < end) {
			next = (cur + PAGE_SIZE) & PAGE_MASK;
			if (next > end)
				next = end;
			len = next - cur;

			paddr = virt2phys(mm, cur);
			if (paddr)
				outer_flush_range(paddr, paddr + len);
			cur += len;
		}
	}
}

void fimg2d_clean_outer_pagetable(struct mm_struct *mm, unsigned long vaddr,
					size_t size)
{
	unsigned long *pgd;
	unsigned long *lv1, *lv1end;
	unsigned long lv2pa;

	pgd = (unsigned long *)mm->pgd;

	lv1 = pgd + (vaddr >> LV1_SHIFT);
	lv1end = pgd + ((vaddr + size + LV1_PT_SIZE-1) >> LV1_SHIFT);

	/* clean level1 page table */
	outer_clean_range(virt_to_phys(lv1), virt_to_phys(lv1end));

	do {
		lv2pa = *lv1 & ~LV2_BASE_MASK;	/* lv2 pt base */
		/* clean level2 page table */
		outer_clean_range(lv2pa, lv2pa + LV2_PT_SIZE);
		lv1++;
	} while (lv1 != lv1end);
}
#endif /* CONFIG_OUTER_CACHE */

enum pt_status fimg2d_check_pagetable(struct mm_struct *mm,
		unsigned long vaddr, size_t size, int write)
{
#ifndef FIMG2D_IOVMM_PAGETABLE
	unsigned long *pgd;
	unsigned long *lv1d, *lv2d;

	pgd = (unsigned long *)mm->pgd;

	size += offset_in_page(vaddr);
	size = PAGE_ALIGN(size);

	while ((long)size > 0) {
		unsigned long *lv2d_first;
		unsigned long lv2d_base, lv2d_end;
		lv1d = pgd + (vaddr >> LV1_SHIFT);

		/*
		 * check level 1 descriptor
		 *	lv1 desc[1:0] = 00 --> fault
		 *	lv1 desc[1:0] = 01 --> page table
		 *	lv1 desc[1:0] = 10 --> section or supersection
		 *	lv1 desc[1:0] = 11 --> reserved
		 */
		if ((*lv1d & LV1_DESC_MASK) != 0x1) {
			fimg2d_debug("invalid LV1 descriptor, "
					"pgd %p lv1d 0x%lx vaddr 0x%lx\n",
					pgd, *lv1d, vaddr);
			return PT_FAULT;
		}
		dma_sync_single_for_device(NULL, virt_to_phys((void *) lv1d),
					sizeof(lv1d), DMA_TO_DEVICE);

		lv2d_base = (unsigned long)phys_to_virt(*lv1d & ~LV2_BASE_MASK);
		lv2d_end = lv2d_base + LV2_PT_SIZE;

		lv2d = (unsigned long *)lv2d_base +
			(((vaddr & LV2_PT_MASK) >> LV2_SHIFT));
		lv2d_first = lv2d;

		do {
			if (write && (!pte_present(*lv2d) ||
						!pte_write(*lv2d))) {
				struct vm_area_struct *vma;

				vma = find_vma(mm, vaddr);
				if (!vma) {
					pr_err("%s: vma is null\n", __func__);
					return PT_FAULT;
				}

				if (vma->vm_end < (vaddr + size)) {
					pr_err("%s: vma overflow: %#lx--%#lx, "
						"vaddr: %#lx, size: %zd\n",
						__func__, vma->vm_start,
						vma->vm_end, vaddr, size);
					return PT_FAULT;
				}

				handle_mm_fault(mm, vma, vaddr,
						FAULT_FLAG_WRITE);
			}

			/*
			 * check level 2 descriptor
			 *	lv2 desc[1:0] = 00 --> fault
			 *	lv2 desc[1:0] = 01 --> 64k pgae
			 *	lv2 desc[1:0] = 1x --> 4k page
			 */
			if ((*lv2d & LV2_DESC_MASK) != 0x2) {
				fimg2d_debug("invalid LV2 descriptor, "
						"pgd %p lv2d 0x%lx "
						"vaddr 0x%lx\n",
						pgd, *lv2d, vaddr);
				return PT_FAULT;
			}

			vaddr += PAGE_SIZE;
			size -= PAGE_SIZE;
		} while (lv2d++, ((unsigned long)lv2d < lv2d_end) && (size > 0));

		dma_sync_single_for_device(NULL,
				virt_to_phys((void *) lv2d_first),
				(size_t)(lv2d - lv2d_first), DMA_TO_DEVICE);
	}

#endif
	return PT_NORMAL;
}
