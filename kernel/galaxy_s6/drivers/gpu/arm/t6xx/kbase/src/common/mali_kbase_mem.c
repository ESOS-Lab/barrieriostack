/*
 *
 * (C) COPYRIGHT 2010-2013 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * @file mali_kbase_mem.c
 * Base kernel memory APIs
 */
#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif				/* CONFIG_DMA_SHARED_BUFFER */

#include <kbase/mali_kbase_config.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_midg_regmap.h>
#include <kbase/src/common/mali_kbase_cache_policy.h>
#include <kbase/src/common/mali_kbase_hw.h>
#include <kbase/src/common/mali_kbase_gator.h>

/**
 * @brief Check the zone compatibility of two regions.
 */
STATIC int kbase_region_tracker_match_zone(struct kbase_va_region *reg1, struct kbase_va_region *reg2)
{
	return ((reg1->flags & KBASE_REG_ZONE_MASK) == (reg2->flags & KBASE_REG_ZONE_MASK));
}

KBASE_EXPORT_TEST_API(kbase_region_tracker_match_zone)

/* This function inserts a region into the tree. */
static void kbase_region_tracker_insert(struct kbase_context *kctx, struct kbase_va_region *new_reg)
{
	u64 start_pfn = new_reg->start_pfn;
	struct rb_node **link = &(kctx->reg_rbtree.rb_node);
	struct rb_node *parent = NULL;

	/* Find the right place in the tree using tree search */
	while (*link) {
		struct kbase_va_region *old_reg;

		parent = *link;
		old_reg = rb_entry(parent, struct kbase_va_region, rblink);

		/* RBTree requires no duplicate entries. */
		KBASE_DEBUG_ASSERT(old_reg->start_pfn != start_pfn);

		if (old_reg->start_pfn > start_pfn)
			link = &(*link)->rb_left;
		else
			link = &(*link)->rb_right;
	}

	/* Put the new node there, and rebalance tree */
	rb_link_node(&(new_reg->rblink), parent, link);
	rb_insert_color(&(new_reg->rblink), &(kctx->reg_rbtree));
}

/* Find allocated region enclosing range. */
struct kbase_va_region *kbase_region_tracker_find_region_enclosing_range(kbase_context *kctx, u64 start_pfn, u32 nr_pages)
{
	struct rb_node *rbnode;
	struct kbase_va_region *reg;
	u64 end_pfn = start_pfn + nr_pages;

	rbnode = kctx->reg_rbtree.rb_node;
	while (rbnode) {
		u64 tmp_start_pfn, tmp_end_pfn;
		reg = rb_entry(rbnode, struct kbase_va_region, rblink);
		tmp_start_pfn = reg->start_pfn;
		tmp_end_pfn = reg->start_pfn + reg->nr_alloc_pages;

		/* If start is lower than this, go left. */
		if (start_pfn < tmp_start_pfn)
			rbnode = rbnode->rb_left;
		/* If end is higher than this, then go right. */
		else if (end_pfn > tmp_end_pfn)
			rbnode = rbnode->rb_right;
		else	/* Enclosing */
			return reg;
	}

	return NULL;
}

/* Find allocated region enclosing free range. */
struct kbase_va_region *kbase_region_tracker_find_region_enclosing_range_free(kbase_context *kctx, u64 start_pfn, u32 nr_pages)
{
	struct rb_node *rbnode;
	struct kbase_va_region *reg;
	u64 end_pfn = start_pfn + nr_pages;

	rbnode = kctx->reg_rbtree.rb_node;
	while (rbnode) {
		u64 tmp_start_pfn, tmp_end_pfn;
		reg = rb_entry(rbnode, struct kbase_va_region, rblink);
		tmp_start_pfn = reg->start_pfn;
		tmp_end_pfn = reg->start_pfn + reg->nr_pages;

		/* If start is lower than this, go left. */
		if (start_pfn < tmp_start_pfn)
			rbnode = rbnode->rb_left;
		/* If end is higher than this, then go right. */
		else if (end_pfn > tmp_end_pfn)
			rbnode = rbnode->rb_right;
		else	/* Enclosing */
			return reg;
	}

	return NULL;
}

/* Find region enclosing given address. */
kbase_va_region *kbase_region_tracker_find_region_enclosing_address(kbase_context *kctx, mali_addr64 gpu_addr)
{
	struct rb_node *rbnode;
	struct kbase_va_region *reg;
	u64 gpu_pfn = gpu_addr >> PAGE_SHIFT;

	KBASE_DEBUG_ASSERT(NULL != kctx);

	rbnode = kctx->reg_rbtree.rb_node;
	while (rbnode) {
		u64 tmp_start_pfn, tmp_end_pfn;
		reg = rb_entry(rbnode, struct kbase_va_region, rblink);
		tmp_start_pfn = reg->start_pfn;
		tmp_end_pfn = reg->start_pfn + reg->nr_pages;

		/* If start is lower than this, go left. */
		if (gpu_pfn < tmp_start_pfn)
			rbnode = rbnode->rb_left;
		/* If end is higher than this, then go right. */
		else if (gpu_pfn >= tmp_end_pfn)
			rbnode = rbnode->rb_right;
		else	/* Enclosing */
			return reg;
	}

	return NULL;
}

KBASE_EXPORT_TEST_API(kbase_region_tracker_find_region_enclosing_address)

/* Find region with given base address */
kbase_va_region *kbase_region_tracker_find_region_base_address(kbase_context *kctx, mali_addr64 gpu_addr)
{
	u64 gpu_pfn = gpu_addr >> PAGE_SHIFT;
	struct rb_node *rbnode;
	struct kbase_va_region *reg;

	KBASE_DEBUG_ASSERT(NULL != kctx);

	rbnode = kctx->reg_rbtree.rb_node;
	while (rbnode) {
		reg = rb_entry(rbnode, struct kbase_va_region, rblink);
		if (reg->start_pfn > gpu_pfn)
			rbnode = rbnode->rb_left;
		else if (reg->start_pfn < gpu_pfn)
			rbnode = rbnode->rb_right;
		else if (gpu_pfn == reg->start_pfn)
			return reg;
		else
			rbnode = NULL;
	}

	return NULL;
}

KBASE_EXPORT_TEST_API(kbase_region_tracker_find_region_base_address)

/* Find region meeting given requirements */
static struct kbase_va_region *kbase_region_tracker_find_region_meeting_reqs(kbase_context *kctx, struct kbase_va_region *reg_reqs, u32 nr_pages, u32 align)
{
	struct rb_node *rbnode;
	struct kbase_va_region *reg;

	/* Note that this search is a linear search, as we do not have a target
	   address in mind, so does not benefit from the rbtree search */
	rbnode = rb_first(&(kctx->reg_rbtree));
	while (rbnode) {
		reg = rb_entry(rbnode, struct kbase_va_region, rblink);
		if ((reg->nr_pages >= nr_pages) && (reg->flags & KBASE_REG_FREE) && kbase_region_tracker_match_zone(reg, reg_reqs)) {

			/* Check alignment */
			u64 start_pfn = (reg->start_pfn + align - 1) & ~(align - 1);
			if ((start_pfn >= reg->start_pfn) && (start_pfn <= (reg->start_pfn + reg->nr_pages - 1)) && ((start_pfn + nr_pages - 1) <= (reg->start_pfn + reg->nr_pages - 1)))
				return reg;
		}
		rbnode = rb_next(rbnode);
	}

	return NULL;
}

/**
 * @brief Remove a region object from the global list.
 *
 * The region reg is removed, possibly by merging with other free and
 * compatible adjacent regions.  It must be called with the context
 * region lock held. The associated memory is not released (see
 * kbase_free_alloced_region). Internal use only.
 */
STATIC mali_error kbase_remove_va_region(kbase_context *kctx, struct kbase_va_region *reg)
{
	struct rb_node *rbprev;
	struct kbase_va_region *prev = NULL;
	struct rb_node *rbnext;
	struct kbase_va_region *next = NULL;

	int merged_front = 0;
	int merged_back = 0;
	mali_error err = MALI_ERROR_NONE;

	/* Try to merge with the previous block first */
	rbprev = rb_prev(&(reg->rblink));
	if (rbprev) {
		prev = rb_entry(rbprev, struct kbase_va_region, rblink);
		if ((prev->flags & KBASE_REG_FREE) && kbase_region_tracker_match_zone(prev, reg)) {
			/* We're compatible with the previous VMA, merge with it */
			prev->nr_pages += reg->nr_pages;
			rb_erase(&(reg->rblink), &kctx->reg_rbtree);
			reg = prev;
			merged_front = 1;
		}
	}

	/* Try to merge with the next block second */
	/* Note we do the lookup here as the tree may have been rebalanced. */
	rbnext = rb_next(&(reg->rblink));
	if (rbnext) {
		/* We're compatible with the next VMA, merge with it */
		next = rb_entry(rbnext, struct kbase_va_region, rblink);
		if ((next->flags & KBASE_REG_FREE) && kbase_region_tracker_match_zone(next, reg)) {
			next->start_pfn = reg->start_pfn;
			next->nr_pages += reg->nr_pages;
			rb_erase(&(reg->rblink), &kctx->reg_rbtree);
			merged_back = 1;
			if (merged_front) {
				/* We already merged with prev, free it */
				kbase_free_alloced_region(reg);
			}
		}
	}

	/* If we failed to merge then we need to add a new block */
	if (!(merged_front || merged_back)) {
		/*
		 * We didn't merge anything. Add a new free
		 * placeholder and remove the original one.
		 */
		struct kbase_va_region *free_reg;

		free_reg = kbase_alloc_free_region(kctx, reg->start_pfn, reg->nr_pages, reg->flags & KBASE_REG_ZONE_MASK);
		if (!free_reg) {
			err = MALI_ERROR_OUT_OF_MEMORY;
			goto out;
		}

		rb_replace_node(&(reg->rblink), &(free_reg->rblink), &(kctx->reg_rbtree));
	}

 out:
	return err;
}

KBASE_EXPORT_TEST_API(kbase_remove_va_region)

/**
 * @brief Insert a VA region to the list, replacing the current at_reg.
 */
static mali_error kbase_insert_va_region_nolock(kbase_context *kctx, struct kbase_va_region *new_reg, struct kbase_va_region *at_reg, u64 start_pfn, u32 nr_pages)
{
	mali_error err = MALI_ERROR_NONE;

	/* Must be a free region */
	KBASE_DEBUG_ASSERT((at_reg->flags & KBASE_REG_FREE) != 0);
	/* start_pfn should be contained within at_reg */
	KBASE_DEBUG_ASSERT((start_pfn >= at_reg->start_pfn) && (start_pfn < at_reg->start_pfn + at_reg->nr_pages));
	/* at least nr_pages from start_pfn should be contained within at_reg */
	KBASE_DEBUG_ASSERT(start_pfn + nr_pages <= at_reg->start_pfn + at_reg->nr_pages);

	new_reg->start_pfn = start_pfn;
	new_reg->nr_pages = nr_pages;

	/* Regions are a whole use, so swap and delete old one. */
	if (at_reg->start_pfn == start_pfn && at_reg->nr_pages == nr_pages) {
		rb_replace_node(&(at_reg->rblink), &(new_reg->rblink), &(kctx->reg_rbtree));
		kbase_free_alloced_region(at_reg);
	}
	/* New region replaces the start of the old one, so insert before. */
	else if (at_reg->start_pfn == start_pfn) {
		at_reg->start_pfn += nr_pages;
		KBASE_DEBUG_ASSERT(at_reg->nr_pages >= nr_pages);
		at_reg->nr_pages -= nr_pages;

		kbase_region_tracker_insert(kctx, new_reg);
	}
	/* New region replaces the end of the old one, so insert after. */
	else if ((at_reg->start_pfn + at_reg->nr_pages) == (start_pfn + nr_pages)) {
		at_reg->nr_pages -= nr_pages;

		kbase_region_tracker_insert(kctx, new_reg);
	}
	/* New region splits the old one, so insert and create new */
	else {
		struct kbase_va_region *new_front_reg = kbase_alloc_free_region(kctx, at_reg->start_pfn, start_pfn - at_reg->start_pfn, at_reg->flags & KBASE_REG_ZONE_MASK);
		if (new_front_reg) {
			at_reg->nr_pages -= nr_pages + new_front_reg->nr_pages;
			at_reg->start_pfn = start_pfn + nr_pages;

			kbase_region_tracker_insert(kctx, new_front_reg);
			kbase_region_tracker_insert(kctx, new_reg);
		} else {
			err = MALI_ERROR_OUT_OF_MEMORY;
		}
	}

	return err;
}

/**
 * @brief Add a VA region to the list.
 */
mali_error kbase_add_va_region(kbase_context *kctx, struct kbase_va_region *reg, mali_addr64 addr, u32 nr_pages, u32 align)
{
	struct kbase_va_region *tmp;
	u64 gpu_pfn = addr >> PAGE_SHIFT;
	mali_error err = MALI_ERROR_NONE;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(NULL != reg);

	if (!align)
		align = 1;

	/* must be a power of 2 */
	KBASE_DEBUG_ASSERT((align & (align - 1)) == 0);
	KBASE_DEBUG_ASSERT(nr_pages > 0);

	/* Path 1: Map a specific address. Find the enclosing region, which *must* be free. */
	if (gpu_pfn) {
		KBASE_DEBUG_ASSERT(!(gpu_pfn & (align - 1)));

		tmp = kbase_region_tracker_find_region_enclosing_range_free(kctx, gpu_pfn, nr_pages);
		if (!tmp) {
			KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "Enclosing region not found: %08x gpu_pfn, %u nr_pages", (unsigned int)gpu_pfn, nr_pages);
			err = MALI_ERROR_OUT_OF_GPU_MEMORY;
			goto exit;
		}

		if ((!kbase_region_tracker_match_zone(tmp, reg)) || (!(tmp->flags & KBASE_REG_FREE))) {
			KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "Zone mismatch: %lu != %lu", tmp->flags & KBASE_REG_ZONE_MASK, reg->flags & KBASE_REG_ZONE_MASK);
			KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "!(tmp->flags & KBASE_REG_FREE): tmp->start_pfn=0x%llx tmp->flags=0x%lx tmp->nr_pages=0x%x gpu_pfn=0x%llx nr_pages=0x%x\n", tmp->start_pfn, tmp->flags, tmp->nr_pages, gpu_pfn, nr_pages);
			KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "in function %s (%p, %p, 0x%llx, 0x%x, 0x%x)\n", __func__, kctx, reg, addr, nr_pages, align);
			err = MALI_ERROR_OUT_OF_GPU_MEMORY;
			goto exit;
		}

		err = kbase_insert_va_region_nolock(kctx, reg, tmp, gpu_pfn, nr_pages);
		if (err) {
			KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "Failed to insert va region");
			err = MALI_ERROR_OUT_OF_GPU_MEMORY;
			goto exit;
		}

		goto exit;
	}

	/* Path 2: Map any free address which meets the requirements.  */
	{
		u64 start_pfn;
		tmp = kbase_region_tracker_find_region_meeting_reqs(kctx, reg, nr_pages, align);
		if (!tmp) {
			err = MALI_ERROR_OUT_OF_GPU_MEMORY;
			goto exit;
		}
		start_pfn = (tmp->start_pfn + align - 1) & ~(align - 1);
		err = kbase_insert_va_region_nolock(kctx, reg, tmp, start_pfn, nr_pages);
	}

 exit:
	return err;
}

KBASE_EXPORT_TEST_API(kbase_add_va_region)

/**
 * @brief Initialize the internal region tracker data structure.
 */
static void kbase_region_tracker_ds_init(kbase_context *kctx, struct kbase_va_region *pmem_reg, struct kbase_va_region *exec_reg, struct kbase_va_region *tmem_reg)
{
	kctx->reg_rbtree = RB_ROOT;
	kbase_region_tracker_insert(kctx, pmem_reg);
	kbase_region_tracker_insert(kctx, exec_reg);
	kbase_region_tracker_insert(kctx, tmem_reg);
}

void kbase_region_tracker_term(kbase_context *kctx)
{
	struct rb_node *rbnode;
	struct kbase_va_region *reg;
	do {
		rbnode = rb_first(&(kctx->reg_rbtree));
		if (rbnode) {
			rb_erase(rbnode, &(kctx->reg_rbtree));
			reg = rb_entry(rbnode, struct kbase_va_region, rblink);
			kbase_free_alloced_region(reg);
		}
	} while (rbnode);
}

/**
 * Initialize the region tracker data structure.
 */
mali_error kbase_region_tracker_init(kbase_context *kctx)
{
	struct kbase_va_region *pmem_reg;
	struct kbase_va_region *exec_reg;
	struct kbase_va_region *tmem_reg;

	/* Make sure page 0 is not used... */
	pmem_reg = kbase_alloc_free_region(kctx, 1, KBASE_REG_ZONE_EXEC_BASE - 1, KBASE_REG_ZONE_PMEM);
	if (!pmem_reg)
		return MALI_ERROR_OUT_OF_MEMORY;

	exec_reg = kbase_alloc_free_region(kctx, KBASE_REG_ZONE_EXEC_BASE, KBASE_REG_ZONE_EXEC_SIZE, KBASE_REG_ZONE_EXEC);
	if (!exec_reg) {
		kbase_free_alloced_region(pmem_reg);
		return MALI_ERROR_OUT_OF_MEMORY;
	}

	tmem_reg = kbase_alloc_free_region(kctx, KBASE_REG_ZONE_TMEM_BASE, KBASE_REG_ZONE_TMEM_SIZE, KBASE_REG_ZONE_TMEM);
	if (!tmem_reg) {
		kbase_free_alloced_region(pmem_reg);
		kbase_free_alloced_region(exec_reg);
		return MALI_ERROR_OUT_OF_MEMORY;
	}

	kbase_region_tracker_ds_init(kctx, pmem_reg, exec_reg, tmem_reg);

	return MALI_ERROR_NONE;
}

mali_error kbase_mem_init(struct kbase_device *kbdev)
{
	kbasep_mem_device *memdev;
	u32 max_shared_memory;
	KBASE_DEBUG_ASSERT(kbdev);

	memdev = &kbdev->memdev;

	max_shared_memory = (u32) kbasep_get_config_value(kbdev, kbdev->config_attributes, KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX);

	if (MALI_ERROR_NONE != kbase_mem_usage_init(&memdev->usage, max_shared_memory >> PAGE_SHIFT)) {
		return MALI_ERROR_FUNCTION_FAILED;
	}

	/* nothing to do, zero-inited when kbase_device was created */
	return MALI_ERROR_NONE;
}

void kbase_mem_halt(kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

void kbase_mem_term(kbase_device *kbdev)
{
	kbasep_mem_device *memdev;
	KBASE_DEBUG_ASSERT(kbdev);

	memdev = &kbdev->memdev;

	kbase_mem_usage_term(&memdev->usage);
}

KBASE_EXPORT_TEST_API(kbase_mem_term)

mali_error kbase_mem_usage_init(struct kbasep_mem_usage *usage, u32 max_pages)
{
	KBASE_DEBUG_ASSERT(usage);

	atomic_set(&usage->cur_pages, 0);
	/* query the max page count */
	usage->max_pages = max_pages;

	return MALI_ERROR_NONE;
}

void kbase_mem_usage_term(kbasep_mem_usage *usage)
{
	KBASE_DEBUG_ASSERT(usage);
	/* No memory should be in use now */
	KBASE_DEBUG_ASSERT(0 == atomic_read(&usage->cur_pages));
	/* So any new alloc requests will fail */
	usage->max_pages = 0;
	/* So we assert on double term */
	atomic_set(&usage->cur_pages, INT_MAX);
}

mali_error kbase_mem_usage_request_pages(kbasep_mem_usage *usage, u32 nr_pages)
{
	int cur_pages;
	int old_cur_pages;

	KBASE_DEBUG_ASSERT(usage);
	KBASE_DEBUG_ASSERT(nr_pages);	/* 0 pages would be an error in the calling code */

	/*
	 * Fetch the initial cur_pages value
	 * each loop iteration below fetches
	 * it as part of the store attempt
	 */
	cur_pages = atomic_read(&usage->cur_pages);

	/* this check allows the simple if test in the loop below */
	if (usage->max_pages < nr_pages)
		goto usage_cap_exceeded;

	do {
		u32 new_cur_pages;

		/* enough pages to fullfill the request? */
		if (usage->max_pages - nr_pages < cur_pages) {
 usage_cap_exceeded:
			KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "Memory usage cap has been reached:\n" "\t%lu pages currently used\n" "\t%lu pages usage cap\n" "\t%lu new pages requested\n" "\twould result in %lu pages over the cap\n", (unsigned long)cur_pages, (unsigned long)usage->max_pages, (unsigned long)nr_pages, (unsigned long)(cur_pages + nr_pages - usage->max_pages));
			return MALI_ERROR_OUT_OF_MEMORY;
		}

		/* try to atomically commit the new count */
		old_cur_pages = cur_pages;
		new_cur_pages = cur_pages + nr_pages;
		cur_pages = atomic_cmpxchg(&usage->cur_pages, old_cur_pages, new_cur_pages);
		/* cur_pages will be like old_cur_pages if there was no race */
	} while (cur_pages != old_cur_pages);

#ifdef CONFIG_MALI_GATOR_SUPPORT
	kbase_trace_mali_total_alloc_pages_change((long long int)cur_pages);
#endif				/* CONFIG_MALI_GATOR_SUPPORT */

	return MALI_ERROR_NONE;
}

KBASE_EXPORT_TEST_API(kbase_mem_usage_request_pages)

void kbase_mem_usage_release_pages(kbasep_mem_usage *usage, u32 nr_pages)
{
	KBASE_DEBUG_ASSERT(usage);
	KBASE_DEBUG_ASSERT(nr_pages <= atomic_read(&usage->cur_pages));

	atomic_sub(nr_pages, &usage->cur_pages);
#ifdef CONFIG_MALI_GATOR_SUPPORT
	kbase_trace_mali_total_alloc_pages_change((long long int)atomic_read(&usage->cur_pages));
#endif				/* CONFIG_MALI_GATOR_SUPPORT */
}

KBASE_EXPORT_TEST_API(kbase_mem_usage_release_pages)

/**
 * @brief Wait for GPU write flush - only in use for BASE_HW_ISSUE_6367
 *
 * Wait 1000 GPU clock cycles. This delay is known to give the GPU time to flush its write buffer.
 * @note If GPU resets occur then the counters are reset to zero, the delay may not be as expected.
 */
#ifdef CONFIG_MALI_NO_MALI
static void kbase_wait_write_flush(kbase_context *kctx)
{
}
#else				/* CONFIG_MALI_NO_MALI */
static void kbase_wait_write_flush(kbase_context *kctx)
{
	u32 base_count = 0;
	/* A suspend won't happen here, because we're in a syscall from a userspace thread */
	kbase_pm_context_active(kctx->kbdev);
	kbase_pm_request_gpu_cycle_counter(kctx->kbdev);
	while (MALI_TRUE) {
		u32 new_count;
		new_count = kbase_reg_read(kctx->kbdev, GPU_CONTROL_REG(CYCLE_COUNT_LO), NULL);
		/* First time around, just store the count. */
		if (base_count == 0) {
			base_count = new_count;
			continue;
		}

		/* No need to handle wrapping, unsigned maths works for this. */
		if ((new_count - base_count) > 1000)
			break;
	}
	kbase_pm_release_gpu_cycle_counter(kctx->kbdev);
	kbase_pm_context_idle(kctx->kbdev);
}
#endif				/* CONFIG_MALI_NO_MALI */

/**
 * @brief Allocate a free region object.
 *
 * The allocated object is not part of any list yet, and is flagged as
 * KBASE_REG_FREE. No mapping is allocated yet.
 *
 * zone is KBASE_REG_ZONE_TMEM, KBASE_REG_ZONE_PMEM, or KBASE_REG_ZONE_EXEC
 *
 */
struct kbase_va_region *kbase_alloc_free_region(kbase_context *kctx, u64 start_pfn, u32 nr_pages, u32 zone)
{
	struct kbase_va_region *new_reg;

	KBASE_DEBUG_ASSERT(kctx != NULL);

	/* zone argument should only contain zone related region flags */
	KBASE_DEBUG_ASSERT((zone & ~KBASE_REG_ZONE_MASK) == 0);
	KBASE_DEBUG_ASSERT(nr_pages > 0);
	KBASE_DEBUG_ASSERT(start_pfn + nr_pages <= (UINT64_MAX / PAGE_SIZE));	/* 64-bit address range is the max */

	new_reg = kzalloc(sizeof(*new_reg), GFP_KERNEL);

	if (!new_reg) {
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "kzalloc failed");
		return NULL;
	}

	new_reg->kctx = kctx;
	new_reg->flags = zone | KBASE_REG_FREE;

	if (KBASE_REG_ZONE_TMEM == zone || KBASE_REG_ZONE_EXEC == zone)
		new_reg->flags |= KBASE_REG_GROWABLE;

	/* not imported by default */
	new_reg->imported_type = BASE_TMEM_IMPORT_TYPE_INVALID;

	new_reg->start_pfn = start_pfn;
	new_reg->nr_pages = nr_pages;
	new_reg->nr_alloc_pages = 0;
	INIT_LIST_HEAD(&new_reg->map_list);

	return new_reg;
}

KBASE_EXPORT_TEST_API(kbase_alloc_free_region)

/**
 * @brief Free a region object.
 *
 * The described region must be freed of any mapping.
 *
 * If the region is not flagged as KBASE_REG_FREE, the destructor
 * kbase_free_phy_pages() will be called.
 */
void kbase_free_alloced_region(struct kbase_va_region *reg)
{
	KBASE_DEBUG_ASSERT(NULL != reg);
	KBASE_DEBUG_ASSERT(list_empty(&reg->map_list));
	if (!(reg->flags & KBASE_REG_FREE)) {
		kbase_free_phy_pages(reg);
		KBASE_DEBUG_CODE(
					/* To detect use-after-free in debug builds */
					reg->flags |= KBASE_REG_FREE);
	}
	kfree(reg);
}

KBASE_EXPORT_TEST_API(kbase_free_alloced_region)

void kbase_mmu_update(kbase_context *kctx)
{
	/* Use GPU implementation-defined caching policy. */
	u32 memattr = ASn_MEMATTR_IMPL_DEF_CACHE_POLICY;
	u32 pgd_high;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	/* ASSERT that the context has a valid as_nr, which is only the case
	 * when it's scheduled in.
	 *
	 * as_nr won't change because the caller has the runpool_irq lock */
	KBASE_DEBUG_ASSERT(kctx->as_nr != KBASEP_AS_NR_INVALID);

	pgd_high = sizeof(kctx->pgd) > 4 ? (kctx->pgd >> 32) : 0;

	kbase_reg_write(kctx->kbdev, MMU_AS_REG(kctx->as_nr, ASn_TRANSTAB_LO), (kctx->pgd & ASn_TRANSTAB_ADDR_SPACE_MASK) | ASn_TRANSTAB_READ_INNER | ASn_TRANSTAB_ADRMODE_TABLE, kctx);

	/* Need to use a conditional expression to avoid "right shift count >= width of type"
	 * error when using an if statement - although the size_of condition is evaluated at compile
	 * time the unused branch is not removed until after it is type-checked and the error
	 * produced.
	 */
	pgd_high = sizeof(kctx->pgd) > 4 ? (kctx->pgd >> 32) : 0;

	kbase_reg_write(kctx->kbdev, MMU_AS_REG(kctx->as_nr, ASn_TRANSTAB_HI), pgd_high, kctx);

	kbase_reg_write(kctx->kbdev, MMU_AS_REG(kctx->as_nr, ASn_MEMATTR_LO), memattr, kctx);
	kbase_reg_write(kctx->kbdev, MMU_AS_REG(kctx->as_nr, ASn_MEMATTR_HI), memattr, kctx);
	kbase_reg_write(kctx->kbdev, MMU_AS_REG(kctx->as_nr, ASn_COMMAND), ASn_COMMAND_UPDATE, kctx);
}

KBASE_EXPORT_TEST_API(kbase_mmu_update)

void kbase_mmu_disable(kbase_context *kctx)
{
	KBASE_DEBUG_ASSERT(NULL != kctx);
	/* ASSERT that the context has a valid as_nr, which is only the case
	 * when it's scheduled in.
	 *
	 * as_nr won't change because the caller has the runpool_irq lock */
	KBASE_DEBUG_ASSERT(kctx->as_nr != KBASEP_AS_NR_INVALID);

	kbase_reg_write(kctx->kbdev, MMU_AS_REG(kctx->as_nr, ASn_TRANSTAB_LO), 0, kctx);
	kbase_reg_write(kctx->kbdev, MMU_AS_REG(kctx->as_nr, ASn_TRANSTAB_HI), 0, kctx);
	kbase_reg_write(kctx->kbdev, MMU_AS_REG(kctx->as_nr, ASn_COMMAND), ASn_COMMAND_UPDATE, kctx);
}

KBASE_EXPORT_TEST_API(kbase_mmu_disable)

mali_error kbase_gpu_mmap(kbase_context *kctx, struct kbase_va_region *reg, mali_addr64 addr, u32 nr_pages, u32 align)
{
	mali_error err;
	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(NULL != reg);

	err = kbase_add_va_region(kctx, reg, addr, nr_pages, align);
	if (MALI_ERROR_NONE != err)
		return err;

	err = kbase_mmu_insert_pages(kctx, reg->start_pfn, kbase_get_phy_pages(reg), reg->nr_alloc_pages, reg->flags & ((1 << KBASE_REG_FLAGS_NR_BITS) - 1));
	if (MALI_ERROR_NONE != err)
		kbase_remove_va_region(kctx, reg);

	return err;
}

KBASE_EXPORT_TEST_API(kbase_gpu_mmap)

mali_error kbase_gpu_munmap(kbase_context *kctx, struct kbase_va_region *reg)
{
	mali_error err;

	if (reg->start_pfn == 0)
		return MALI_ERROR_NONE;

	err = kbase_mmu_teardown_pages(kctx, reg->start_pfn, reg->nr_alloc_pages);
	if (MALI_ERROR_NONE != err)
		return err;

	err = kbase_remove_va_region(kctx, reg);
	return err;
}

/**
 * @brief Find a mapping keyed with ptr in region reg
 */
STATIC struct kbase_cpu_mapping *kbase_find_cpu_mapping(struct kbase_va_region *reg, struct vm_area_struct * vma)
{
	struct kbase_cpu_mapping *map;
	struct list_head *pos;

	KBASE_DEBUG_ASSERT(NULL != reg);

	list_for_each(pos, &reg->map_list) {
		map = list_entry(pos, kbase_cpu_mapping, link);
		if (map->private == vma)
			return map;
	}

	return NULL;
}

KBASE_EXPORT_TEST_API(kbase_find_cpu_mapping)

STATIC struct kbase_cpu_mapping *kbasep_find_enclosing_cpu_mapping_of_region(const struct kbase_va_region *reg, void *uaddr, size_t size)
{
	struct kbase_cpu_mapping *map;
	struct list_head *pos;

	KBASE_DEBUG_ASSERT(NULL != reg);

	if ((uintptr_t) uaddr + size < (uintptr_t) uaddr)	/* overflow check */
		return NULL;

	list_for_each(pos, &reg->map_list) {
		map = list_entry(pos, kbase_cpu_mapping, link);
		if (map->uaddr <= uaddr && ((uintptr_t) map->uaddr + (map->nr_pages << PAGE_SHIFT)) >= ((uintptr_t) uaddr + size))
			return map;
	}

	return NULL;
}

KBASE_EXPORT_TEST_API(kbasep_find_enclosing_cpu_mapping_of_region)

static void kbase_dump_mappings(struct kbase_va_region *reg)
{
	struct kbase_cpu_mapping *map;
	struct list_head *pos;

	KBASE_DEBUG_ASSERT(NULL != reg);

	list_for_each(pos, &reg->map_list) {
		map = list_entry(pos, kbase_cpu_mapping, link);
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "uaddr %p nr_pages %d page_off %016llx vma %p\n", map->uaddr, map->nr_pages, map->page_off, map->private);
	}
}

/**
 * @brief Delete a mapping keyed with ptr in region reg
 */
mali_error kbase_cpu_free_mapping(struct kbase_va_region *reg, struct vm_area_struct * vma)
{
	struct kbase_cpu_mapping *map;
	mali_error err = MALI_ERROR_NONE;
	KBASE_DEBUG_ASSERT(NULL != reg);
	map = kbase_find_cpu_mapping(reg, vma);
	if (!map) {
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "Freeing unknown mapping %p in region %p\n", vma, (void *)reg);
		kbase_dump_mappings(reg);
		err = MALI_ERROR_FUNCTION_FAILED;
		goto out;
	}

	/* As the tmem is being unmapped we need to update the pages used by the process */
	if ( (reg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_TMEM )
	{
		kbase_process_page_usage_inc(reg->kctx, map->nr_pages);
	}

	list_del(&map->link);
	kfree(map);

	if ((reg->flags & KBASE_REG_DELAYED_FREE) && list_empty(&reg->map_list))
		err = kbase_mem_free_region(reg->kctx, reg);

 out:
	return err;
}

KBASE_EXPORT_TEST_API(kbase_cpu_free_mapping)

struct kbase_cpu_mapping *kbasep_find_enclosing_cpu_mapping(kbase_context *kctx, mali_addr64 gpu_addr, void *uaddr, size_t size)
{
	struct kbase_cpu_mapping *map = NULL;
	const struct kbase_va_region *reg;

	KBASE_DEBUG_ASSERT(kctx != NULL);

	kbase_gpu_vm_lock(kctx);

	reg = kbase_region_tracker_find_region_enclosing_address(kctx, gpu_addr);
	if (NULL != reg)
		map = kbasep_find_enclosing_cpu_mapping_of_region(reg, uaddr, size);

	kbase_gpu_vm_unlock(kctx);

	return map;
}

KBASE_EXPORT_TEST_API(kbasep_find_enclosing_cpu_mapping)

static mali_error kbase_do_syncset(kbase_context *kctx, struct base_syncset *set, kbase_sync_kmem_fn sync_fn)
{
	mali_error err = MALI_ERROR_NONE;
	struct basep_syncset *sset = &set->basep_sset;
	struct kbase_va_region *reg;
	struct kbase_cpu_mapping *map;
	phys_addr_t *pa;
	u64 page_off, page_count;
	void *start;
	size_t size;
	u64 i;
	u32 offset_within_page;
	phys_addr_t base_phy_addr = 0;
	void *base_virt_addr = 0;
	size_t area_size = 0;

	kbase_os_mem_map_lock(kctx);

	kbase_gpu_vm_lock(kctx);

	/* find the region where the virtual address is contained */
	reg = kbase_region_tracker_find_region_enclosing_address(kctx, sset->mem_handle);
	if (!reg) {
		err = MALI_ERROR_FUNCTION_FAILED;
		goto out_unlock;
	}

	if (!(reg->flags & KBASE_REG_CPU_CACHED))
		goto out_unlock;

	start = (void *)(uintptr_t) sset->user_addr;
	size = (size_t) sset->size;

	map = kbasep_find_enclosing_cpu_mapping_of_region(reg, start, size);
	if (!map) {
		err = MALI_ERROR_FUNCTION_FAILED;
		goto out_unlock;
	}

	offset_within_page = (uintptr_t) start & (PAGE_SIZE - 1);
	page_off = map->page_off + (((uintptr_t) start - (uintptr_t) map->uaddr) >> PAGE_SHIFT);
	page_count = ((size + offset_within_page + (PAGE_SIZE - 1)) & PAGE_MASK) >> PAGE_SHIFT;
	pa = kbase_get_phy_pages(reg);

	for (i = 0; i < page_count; i++) {
		u32 offset = (uintptr_t) start & (PAGE_SIZE - 1);
		phys_addr_t paddr = pa[page_off + i] + offset;
		size_t sz = MIN(((size_t) PAGE_SIZE - offset), size);

		if (paddr == base_phy_addr + area_size && start == (void *)((uintptr_t) base_virt_addr + area_size)) {
			area_size += sz;
		} else if (area_size > 0) {
			sync_fn(base_phy_addr, base_virt_addr, area_size);
			area_size = 0;
		}

		if (area_size == 0) {
			base_phy_addr = paddr;
			base_virt_addr = start;
			area_size = sz;
		}

		start = (void *)((uintptr_t) start + sz);
		size -= sz;
	}

	if (area_size > 0)
		sync_fn(base_phy_addr, base_virt_addr, area_size);

	KBASE_DEBUG_ASSERT(size == 0);

 out_unlock:
	kbase_gpu_vm_unlock(kctx);
	kbase_os_mem_map_unlock(kctx);
	return err;
}

mali_error kbase_sync_now(kbase_context *kctx, struct base_syncset *syncset)
{
	mali_error err = MALI_ERROR_FUNCTION_FAILED;
	struct basep_syncset *sset;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(NULL != syncset);

	sset = &syncset->basep_sset;

	switch (sset->type) {
	case BASE_SYNCSET_OP_MSYNC:
		err = kbase_do_syncset(kctx, syncset, kbase_sync_to_memory);
		break;

	case BASE_SYNCSET_OP_CSYNC:
		err = kbase_do_syncset(kctx, syncset, kbase_sync_to_cpu);
		break;

	default:
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "Unknown msync op %d\n", sset->type);
		break;
	}

	return err;
}

KBASE_EXPORT_TEST_API(kbase_sync_now)
#ifdef CONFIG_DMA_SHARED_BUFFER
static mali_bool is_actively_imported_umm(kbase_va_region *reg)
{
	if (reg->imported_type == BASE_TMEM_IMPORT_TYPE_UMM) {
		if (reg->imported_metadata.umm.current_mapping_usage_count > 0)
			return MALI_TRUE;
	}
	return MALI_FALSE;
}
#else
static mali_bool is_actively_imported_umm(kbase_va_region *reg)
{
	return MALI_FALSE;
}
#endif				/* CONFIG_DMA_SHARED_BUFFER */

/* vm lock must be held */
mali_error kbase_mem_free_region(kbase_context *kctx, kbase_va_region *reg)
{
	mali_error err;
	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(NULL != reg);
	BUG_ON(!mutex_is_locked(&kctx->reg_lock));

	if (!list_empty(&reg->map_list) || is_actively_imported_umm(reg)) {
		/*
		 * We still have mappings, can't free memory. This also handles the race condition with the unmap code
		 * (see kbase_cpu_vm_close()).
		 */
		err = MALI_ERROR_NONE;
		reg->flags |= KBASE_REG_DELAYED_FREE;
		goto out;
	}

	err = kbase_gpu_munmap(kctx, reg);
	if (err) {
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "Could not unmap from the GPU...\n");
		goto out;
	}

	if (kbase_hw_has_issue(kctx->kbdev, BASE_HW_ISSUE_6367)) {
		/* Wait for GPU to flush write buffer before freeing physical pages */
		kbase_wait_write_flush(kctx);
	}

	/* This will also free the physical pages */
	kbase_free_alloced_region(reg);

 out:
	return err;
}

KBASE_EXPORT_TEST_API(kbase_mem_free_region)

/**
 * @brief Free the region from the GPU and unregister it.
 *
 * This function implements the free operation on a memory segment.
 * It will loudly fail if called with outstanding mappings.
 */
mali_error kbase_mem_free(kbase_context *kctx, mali_addr64 gpu_addr)
{
	mali_error err = MALI_ERROR_NONE;
	struct kbase_va_region *reg;

	KBASE_DEBUG_ASSERT(kctx != NULL);

	if (0 == gpu_addr) {
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "gpu_addr 0 is reserved for the ringbuffer and it's an error to try to free it using kbase_mem_free\n");
		return MALI_ERROR_FUNCTION_FAILED;
	}
	kbase_gpu_vm_lock(kctx);

	if (gpu_addr < PAGE_SIZE) {
		/* an OS specific cookie, ask the OS specific code to validate it */
		reg = kbase_lookup_cookie(kctx, gpu_addr);
		if (!reg) {
			err = MALI_ERROR_FUNCTION_FAILED;
			goto out_unlock;
		}

		/* ask to unlink the cookie as we'll free it */
		kbase_unlink_cookie(kctx, gpu_addr, reg);

		kbase_free_alloced_region(reg);
	} else {
		/* A real GPU va */

		/* Validate the region */
		reg = kbase_region_tracker_find_region_base_address(kctx, gpu_addr);
		if (!reg) {
			KBASE_DEBUG_ASSERT_MSG(0, "Trying to free nonexistent region\n 0x%llX", gpu_addr);
			err = MALI_ERROR_FUNCTION_FAILED;
			goto out_unlock;
		}

		err = kbase_mem_free_region(kctx, reg);
	}

 out_unlock:
	kbase_gpu_vm_unlock(kctx);
	return err;
}

KBASE_EXPORT_TEST_API(kbase_mem_free)

void kbase_update_region_flags(struct kbase_va_region *reg, u32 flags, mali_bool is_growable)
{
	KBASE_DEBUG_ASSERT(NULL != reg);
	KBASE_DEBUG_ASSERT((flags & ~((1 << BASE_MEM_FLAGS_NR_BITS) - 1)) == 0);

	reg->flags |= kbase_cache_enabled(flags, reg->nr_pages);

	if ((flags & BASE_MEM_GROW_ON_GPF) || is_growable) {
		reg->flags |= KBASE_REG_GROWABLE;

		if (flags & BASE_MEM_GROW_ON_GPF)
			reg->flags |= KBASE_REG_PF_GROW;
	} else {
		/* As this region is not growable but the default is growable, we
		   explicitly clear the growable flag. */
		reg->flags &= ~KBASE_REG_GROWABLE;
	}

	if (flags & BASE_MEM_PROT_CPU_WR)
		reg->flags |= KBASE_REG_CPU_WR;

	if (flags & BASE_MEM_PROT_CPU_RD)
		reg->flags |= KBASE_REG_CPU_RD;

	if (flags & BASE_MEM_PROT_GPU_WR)
		reg->flags |= KBASE_REG_GPU_WR;

	if (flags & BASE_MEM_PROT_GPU_RD)
		reg->flags |= KBASE_REG_GPU_RD;

	if (0 == (flags & BASE_MEM_PROT_GPU_EX))
		reg->flags |= KBASE_REG_GPU_NX;

	if (flags & BASE_MEM_COHERENT_LOCAL)
		reg->flags |= KBASE_REG_SHARE_IN;
	else if (flags & BASE_MEM_COHERENT_SYSTEM)
		reg->flags |= KBASE_REG_SHARE_BOTH;

}
KBASE_EXPORT_TEST_API(kbase_update_region_flags)

/**
 * @brief The memory allocated by the helper will always get cleared
 */
mali_error kbase_alloc_phy_pages_helper(struct kbase_va_region *reg, u32 nr_pages_requested)
{
	kbase_context * kctx;
	phys_addr_t * page_array;

	KBASE_DEBUG_ASSERT(reg);
	KBASE_DEBUG_ASSERT(reg->kctx);

	/* Can't call this on TB buffers */
	KBASE_DEBUG_ASSERT(0 == (reg->flags & KBASE_REG_IS_TB));
	/* can't be called on imported types */
	KBASE_DEBUG_ASSERT(BASE_TMEM_IMPORT_TYPE_INVALID == reg->imported_type);
	/* Growth of too many pages attempted! (written this way to catch overflow)) */
	KBASE_DEBUG_ASSERT(reg->nr_pages - reg->nr_alloc_pages >= nr_pages_requested);
	/* A complete commit is required if not marked as growable */
	KBASE_DEBUG_ASSERT((reg->flags & KBASE_REG_GROWABLE) || (reg->nr_pages == nr_pages_requested));

	if (0 == nr_pages_requested) {
		/* early out if nothing to do */
		return MALI_ERROR_NONE;
	}

	kctx = reg->kctx;
	KBASE_DEBUG_ASSERT(kctx);

	if (MALI_ERROR_NONE != kbase_mem_usage_request_pages(&kctx->usage, nr_pages_requested))
		return MALI_ERROR_OUT_OF_MEMORY;

	if (MALI_ERROR_NONE != kbase_mem_usage_request_pages(&kctx->kbdev->memdev.usage, nr_pages_requested))
	{
		kbase_mem_usage_release_pages(&kctx->usage, nr_pages_requested);
		return MALI_ERROR_OUT_OF_MEMORY;
	}

	page_array = kbase_get_phy_pages(reg);

	if (MALI_ERROR_NONE != kbase_mem_allocator_alloc(&kctx->osalloc, nr_pages_requested, page_array + reg->nr_alloc_pages, reg->flags))
	{
		kbase_mem_usage_release_pages(&kctx->usage, nr_pages_requested);
		kbase_mem_usage_release_pages(&kctx->kbdev->memdev.usage, nr_pages_requested);
		return MALI_ERROR_OUT_OF_MEMORY;
	}

	reg->nr_alloc_pages += nr_pages_requested;

	if ( (reg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_TMEM)
	{
		kbase_process_page_usage_inc(reg->kctx, nr_pages_requested);
	}

	return MALI_ERROR_NONE;

}

void kbase_free_phy_pages_helper(struct kbase_va_region * reg, u32 nr_pages_to_free)
{
	kbase_context * kctx;
	phys_addr_t * page_array;
	mali_bool sync_back;
	KBASE_DEBUG_ASSERT(reg);
	/* Can't call this on TB buffers */
	KBASE_DEBUG_ASSERT(0 == (reg->flags & KBASE_REG_IS_TB));
	/* can't be called on imported types */
	KBASE_DEBUG_ASSERT(BASE_TMEM_IMPORT_TYPE_INVALID == reg->imported_type);
	/* Free of too many pages attempted! */
	KBASE_DEBUG_ASSERT(reg->nr_alloc_pages >= nr_pages_to_free);
	/* A complete free is required if not marked as growable */
	KBASE_DEBUG_ASSERT((reg->flags & KBASE_REG_GROWABLE) || (reg->nr_alloc_pages == nr_pages_to_free));

	/* early out if nothing to do */
	if (0 == nr_pages_to_free)
		return;

	kctx = reg->kctx;
	KBASE_DEBUG_ASSERT(kctx);

	page_array = kbase_get_phy_pages(reg);

	sync_back = ( reg->flags & KBASE_REG_CPU_CACHED ) ? MALI_TRUE : MALI_FALSE;
	kbase_mem_allocator_free(&kctx->osalloc, nr_pages_to_free, page_array + reg->nr_alloc_pages - nr_pages_to_free, sync_back);

	reg->nr_alloc_pages -= nr_pages_to_free;

	if ( (reg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_TMEM )
	{
		kbase_process_page_usage_dec(reg->kctx, nr_pages_to_free);
	}

	kbase_mem_usage_release_pages(&reg->kctx->usage, nr_pages_to_free);
	kbase_mem_usage_release_pages(&reg->kctx->kbdev->memdev.usage, nr_pages_to_free);
}


/* Frees all allocated pages of a region */
void kbase_free_phy_pages(struct kbase_va_region *reg)
{
	phys_addr_t *page_array;
	KBASE_DEBUG_ASSERT(NULL != reg);

	page_array = kbase_get_phy_pages(reg);

	if (reg->imported_type != BASE_TMEM_IMPORT_TYPE_INVALID) {
		switch (reg->imported_type) {
#ifdef CONFIG_UMP
		case BASE_TMEM_IMPORT_TYPE_UMP:
			{
				ump_dd_handle umph;
				umph = (ump_dd_handle) reg->imported_metadata.ump_handle;
				ump_dd_release(umph);
				break;
			}
#endif				/* CONFIG_UMP */
#ifdef CONFIG_DMA_SHARED_BUFFER
		case BASE_TMEM_IMPORT_TYPE_UMM:
			{
				dma_buf_detach(reg->imported_metadata.umm.dma_buf, reg->imported_metadata.umm.dma_attachment);
				dma_buf_put(reg->imported_metadata.umm.dma_buf);
				break;
			}
#endif				/* CONFIG_DMA_SHARED_BUFFER */
		default:
			/* unsupported types should never reach this point */
			KBASE_DEBUG_ASSERT(0);
			break;
		}
		reg->imported_type = BASE_TMEM_IMPORT_TYPE_INVALID;
	} else {
		if (reg->flags & KBASE_REG_IS_TB) {
			/* trace buffer being freed. Disconnect, then use vfree */
			/* save tb so we can free it after the disconnect call */
			void *tb;
			tb = reg->kctx->jctx.tb;
			kbase_device_trace_buffer_uninstall(reg->kctx);
			vfree(tb);
		} else if (reg->flags & (KBASE_REG_IS_RB | KBASE_REG_IS_MMU_DUMP)) {
			/* nothing to do */
		} else {
			kbase_free_phy_pages_helper(reg, reg->nr_alloc_pages);
		}
	}

	kbase_set_phy_pages(reg, NULL);
	vfree(page_array);
}

KBASE_EXPORT_TEST_API(kbase_free_phy_pages)

int kbase_alloc_phy_pages(struct kbase_va_region *reg, u32 vsize, u32 size)
{
	phys_addr_t *page_array;

	KBASE_DEBUG_ASSERT(NULL != reg);
	KBASE_DEBUG_ASSERT(vsize > 0);

	/* validate user provided arguments */
	if (size > vsize || vsize > reg->nr_pages)
		goto out_term;

	/* Prevent vsize*sizeof from wrapping around.
	 * For instance, if vsize is 2**29+1, we'll allocate 1 byte and the alloc won't fail.
	 */
	if ((size_t) vsize > ((size_t) -1 / sizeof(*page_array)))
		goto out_term;

	KBASE_DEBUG_ASSERT(0 != vsize);
	page_array = vmalloc_user(vsize * sizeof(*page_array));

	if (!page_array)
		goto out_term;

	kbase_set_phy_pages(reg, page_array);
	reg->nr_alloc_pages = 0;
	if (MALI_ERROR_NONE != kbase_alloc_phy_pages_helper(reg, size))
		goto out_free;

	return 0;

 out_free:
	vfree(page_array);
 out_term:
	return -1;
}

KBASE_EXPORT_TEST_API(kbase_alloc_phy_pages)

/** @brief Round to +inf a tmem growable delta in pages */
STATIC mali_bool kbasep_tmem_growable_round_delta(kbase_device *kbdev, s32 *delta_ptr)
{
	s32 delta;

	KBASE_DEBUG_ASSERT(delta_ptr != NULL);

	delta = *delta_ptr;

	if (delta >= 0) {
		u32 new_delta_unsigned = kbasep_tmem_growable_round_size(kbdev, (u32) delta);
		if (new_delta_unsigned > S32_MAX) {
			/* Can't support a delta of this size */
			return MALI_FALSE;
		}

		*delta_ptr = (s32) new_delta_unsigned;
	} else {
		u32 new_delta_unsigned = (u32) -delta;
		/* Round down */
		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_9630))
			new_delta_unsigned = new_delta_unsigned & ~(KBASEP_TMEM_GROWABLE_BLOCKSIZE_PAGES_HW_ISSUE_9630 - 1);
		else if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8316))
			new_delta_unsigned = new_delta_unsigned & ~(KBASEP_TMEM_GROWABLE_BLOCKSIZE_PAGES_HW_ISSUE_8316 - 1);
		else
			new_delta_unsigned = new_delta_unsigned & ~(KBASEP_TMEM_GROWABLE_BLOCKSIZE_PAGES - 1);

		*delta_ptr = (s32) -new_delta_unsigned;
	}

	return MALI_TRUE;
}

mali_bool kbase_check_alloc_flags(u32 flags)
{
	/* Only known flags should be set. */
	if (flags & ~((1 << BASE_MEM_FLAGS_NR_BITS) - 1))
		return MALI_FALSE;

	/* At least one flag should be set */
	if (flags == 0)
		return MALI_FALSE;

	/* Either the GPU or CPU must be reading from the allocated memory */
	if ((flags & (BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_GPU_RD)) == 0)
		return MALI_FALSE;

	/* Either the GPU or CPU must be writing to the allocated memory */
	if ((flags & (BASE_MEM_PROT_CPU_WR | BASE_MEM_PROT_GPU_WR)) == 0)
		return MALI_FALSE;

	/* GPU cannot be writing to GPU executable memory and cannot grow the memory on page fault. */
	if ((flags & BASE_MEM_PROT_GPU_EX) && (flags & (BASE_MEM_PROT_GPU_WR | BASE_MEM_GROW_ON_GPF)))
		return MALI_FALSE;

	/* GPU should have at least read or write access otherwise there is no
	   reason for allocating pmem/tmem. */
	if ((flags & (BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_WR)) == 0)
		return MALI_FALSE;

	return MALI_TRUE;
}

struct kbase_va_region *kbase_tmem_alloc(kbase_context *kctx, u32 vsize, u32 psize, u32 extent, u32 flags, mali_bool is_growable)
{
	struct kbase_va_region *reg;
	mali_error err;
	u32 align = 1;
	u32 vsize_rounded = vsize;
	u32 psize_rounded = psize;
	u32 extent_rounded = extent;
	u32 zone = KBASE_REG_ZONE_TMEM;

	if (0 == vsize)
		goto out1;

	KBASE_DEBUG_ASSERT(NULL != kctx);

	if (!kbase_check_alloc_flags(flags))
		goto out1;

	if ((flags & BASE_MEM_GROW_ON_GPF) != MALI_FALSE) {
		/* Round up the sizes for growable on GPU page fault memory */
		vsize_rounded = kbasep_tmem_growable_round_size(kctx->kbdev, vsize);
		psize_rounded = kbasep_tmem_growable_round_size(kctx->kbdev, psize);
		extent_rounded = kbasep_tmem_growable_round_size(kctx->kbdev, extent);

		if (vsize_rounded < vsize || psize_rounded < psize || extent_rounded < extent) {
			/* values too large to round */
			return NULL;
		}
	}

	if (flags & BASE_MEM_PROT_GPU_EX)
		zone = KBASE_REG_ZONE_EXEC;

	if (extent > 0 && !(flags & BASE_MEM_GROW_ON_GPF)) {
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "BASE_MEM_GROW_ON_GPF flag not set when extent is greater than 0");
		goto out1;
	}

	reg = kbase_alloc_free_region(kctx, 0, vsize_rounded, zone);
	if (!reg)
		goto out1;

	reg->flags &= ~KBASE_REG_FREE;

	kbase_update_region_flags(reg, flags, is_growable);

	if (kbase_alloc_phy_pages(reg, vsize_rounded, psize_rounded))
		goto out2;

	reg->nr_alloc_pages = psize_rounded;
	reg->extent = extent_rounded;

	kbase_gpu_vm_lock(kctx);
	err = kbase_gpu_mmap(kctx, reg, 0, vsize_rounded, align);
	kbase_gpu_vm_unlock(kctx);

	if (err) {
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "kbase_gpu_mmap failed\n");
		goto out3;
	}

	return reg;

 out3:
	kbase_free_phy_pages(reg);
 out2:
	kfree(reg);
 out1:
	return NULL;
}

KBASE_EXPORT_TEST_API(kbase_tmem_alloc)

mali_error kbase_tmem_resize(kbase_context *kctx, mali_addr64 gpu_addr, s32 delta, u32 * const size, base_backing_threshold_status * const failure_reason)
{
	kbase_va_region *reg;
	mali_error ret = MALI_ERROR_FUNCTION_FAILED;
	phys_addr_t *phy_pages;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(size);
	KBASE_DEBUG_ASSERT(failure_reason);
	KBASE_DEBUG_ASSERT(gpu_addr != 0);

	kbase_gpu_vm_lock(kctx);

	/* Validate the region */
	reg = kbase_region_tracker_find_region_base_address(kctx, gpu_addr);
	if (!reg || (reg->flags & KBASE_REG_FREE)) {
		/* not a valid region or is free memory */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
		goto out_unlock;
	}

	if (!((KBASE_REG_ZONE_MASK & reg->flags) == KBASE_REG_ZONE_TMEM || (KBASE_REG_ZONE_MASK & reg->flags) == KBASE_REG_ZONE_EXEC)) {
		/* not a valid region - not tmem or exec region */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
		goto out_unlock;
	}
	if (0 == (reg->flags & KBASE_REG_GROWABLE)) {
		/* not growable */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_NOT_GROWABLE;
		goto out_unlock;
	}

	if ((delta != 0) && !list_empty(&reg->map_list)) {
		/* We still have mappings */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_MAPPED;
		goto out_unlock;
	}

	if (reg->flags & KBASE_REG_PF_GROW) {
		/* Apply rounding to +inf on the delta, which may cause a negative delta to become zero */
		if (kbasep_tmem_growable_round_delta(kctx->kbdev, &delta) == MALI_FALSE) {
			/* Can't round this big a delta */
			*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
			goto out_unlock;
		}
	}

	if (delta < 0 && (u32) -delta > reg->nr_alloc_pages) {
		/* Underflow */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
		goto out_unlock;
	}
	if (reg->nr_alloc_pages + delta > reg->nr_pages) {
		/* Would overflow the VA region */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
		goto out_unlock;
	}

	phy_pages = kbase_get_phy_pages(reg);

	if (delta > 0) {
		mali_error err;

		/* Allocate some more pages */
		if (MALI_ERROR_NONE != kbase_alloc_phy_pages_helper(reg, delta)) {
			*failure_reason = BASE_BACKING_THRESHOLD_ERROR_OOM;
			goto out_unlock;
		}
		err = kbase_mmu_insert_pages(kctx, reg->start_pfn + reg->nr_alloc_pages - delta, phy_pages + reg->nr_alloc_pages - delta, delta, reg->flags);
		if (MALI_ERROR_NONE != err) {
			kbase_free_phy_pages_helper(reg, delta);
			*failure_reason = BASE_BACKING_THRESHOLD_ERROR_OOM;
			goto out_unlock;
		}
	} else if (delta < 0) {
		mali_error err;
		/* Free some pages */

		/* Get the absolute value of delta. Note that we have to add one before and after the negation to avoid
		 * overflowing when delta is INT_MIN */
		u32 num_pages = (u32) (-(delta + 1)) + 1;

		err = kbase_mmu_teardown_pages(kctx, reg->start_pfn + reg->nr_alloc_pages - num_pages, num_pages);
		if (MALI_ERROR_NONE != err) {
			*failure_reason = BASE_BACKING_THRESHOLD_ERROR_OOM;
			goto out_unlock;
		}

		if (kbase_hw_has_issue(kctx->kbdev, BASE_HW_ISSUE_6367)) {
			/* Wait for GPU to flush write buffer before freeing physical pages */
			kbase_wait_write_flush(kctx);
		}

		kbase_free_phy_pages_helper(reg, num_pages);
	}
	/* else just a size query */

	*size = reg->nr_alloc_pages;

	ret = MALI_ERROR_NONE;

 out_unlock:
	kbase_gpu_vm_unlock(kctx);
	return ret;
}

KBASE_EXPORT_TEST_API(kbase_tmem_resize)

mali_error kbase_tmem_set_size(kbase_context *kctx, mali_addr64 gpu_addr, u32 size, u32 * const actual_size, base_backing_threshold_status * const failure_reason)
{
	u32 delta = 0;
	kbase_va_region *reg;
	mali_error ret = MALI_ERROR_FUNCTION_FAILED;
	phys_addr_t *phy_pages;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(NULL != actual_size);
	KBASE_DEBUG_ASSERT(failure_reason);
	KBASE_DEBUG_ASSERT(gpu_addr != 0);

	kbase_gpu_vm_lock(kctx);

	/* Validate the region */
	reg = kbase_region_tracker_find_region_base_address(kctx, gpu_addr);
	if (!reg || (reg->flags & KBASE_REG_FREE)) {
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
		goto out_unlock;
	}

	if (!((KBASE_REG_ZONE_MASK & reg->flags) == KBASE_REG_ZONE_TMEM || (KBASE_REG_ZONE_MASK & reg->flags) == KBASE_REG_ZONE_EXEC)) {
		/* not a valid region - not tmem or exec region */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
		goto out_unlock;
	}
	if (0 == (reg->flags & KBASE_REG_GROWABLE)) {
		*actual_size = reg->nr_alloc_pages;
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_NOT_GROWABLE;
		goto out_unlock;
	}

	if (!list_empty(&reg->map_list)) {
		/* We still have mappings */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_MAPPED;
		goto out_unlock;
	}

	if (size && reg->flags & KBASE_REG_PF_GROW) {
		size = kbasep_tmem_growable_round_size(kctx->kbdev, size);
		/* check for rounding overflow */
		if (!size) {
			*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
			goto out_unlock;
		}
	}

	if (size > reg->nr_pages) {
		/* Would overflow the VA region */
		*failure_reason = BASE_BACKING_THRESHOLD_ERROR_INVALID_ARGUMENTS;
		goto out_unlock;
	}

	phy_pages = kbase_get_phy_pages(reg);

	if (size > reg->nr_alloc_pages) {
		mali_error err;
		delta = size - reg->nr_alloc_pages;
		/* Allocate some more pages */
		if (MALI_ERROR_NONE != kbase_alloc_phy_pages_helper(reg, delta)) {
			*failure_reason = BASE_BACKING_THRESHOLD_ERROR_OOM;
			goto out_unlock;
		}
		err = kbase_mmu_insert_pages(kctx, reg->start_pfn + reg->nr_alloc_pages - delta, phy_pages + reg->nr_alloc_pages - delta, delta, reg->flags);
		if (MALI_ERROR_NONE != err) {
			kbase_free_phy_pages_helper(reg, delta);
			*failure_reason = BASE_BACKING_THRESHOLD_ERROR_OOM;
			goto out_unlock;
		}
	} else if (size < reg->nr_alloc_pages) {
		mali_error err;
		delta = reg->nr_alloc_pages - size;

		/* Free some pages */
		err = kbase_mmu_teardown_pages(kctx, reg->start_pfn + reg->nr_alloc_pages - delta, delta);
		if (MALI_ERROR_NONE != err) {
			*failure_reason = BASE_BACKING_THRESHOLD_ERROR_OOM;
			goto out_unlock;
		}

		if (kbase_hw_has_issue(kctx->kbdev, BASE_HW_ISSUE_6367)) {
			/* Wait for GPU to flush write buffer before freeing physical pages */
			kbase_wait_write_flush(kctx);
		}

		kbase_free_phy_pages_helper(reg, delta);
	}

	*actual_size = reg->nr_alloc_pages;

	ret = MALI_ERROR_NONE;

 out_unlock:
	kbase_gpu_vm_unlock(kctx);

	return ret;
}

KBASE_EXPORT_TEST_API(kbase_tmem_set_size)

mali_error kbase_tmem_get_size(kbase_context *kctx, mali_addr64 gpu_addr, u32 * const actual_size)
{
	kbase_va_region *reg;
	mali_error ret = MALI_ERROR_FUNCTION_FAILED;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(NULL != actual_size);
	KBASE_DEBUG_ASSERT(gpu_addr != 0);

	kbase_gpu_vm_lock(kctx);

	/* Validate the region */
	reg = kbase_region_tracker_find_region_base_address(kctx, gpu_addr);
	if (!reg || (reg->flags & KBASE_REG_FREE))
		goto out_unlock;

	*actual_size = reg->nr_alloc_pages;

	ret = MALI_ERROR_NONE;

 out_unlock:
	kbase_gpu_vm_unlock(kctx);
	return ret;
}

KBASE_EXPORT_TEST_API(kbase_tmem_get_size)
#ifdef CONFIG_UMP
static struct kbase_va_region *kbase_tmem_from_ump(kbase_context *kctx, ump_secure_id id, u64 * const pages)
{
	struct kbase_va_region *reg;
	mali_error err;
	ump_dd_handle umph;
	u64 vsize;
	u64 block_count;
	const ump_dd_physical_block_64 *block_array;
	phys_addr_t *page_array;
	u64 i, j;
	int page = 0;
	ump_alloc_flags ump_flags;
	ump_alloc_flags cpu_flags;
	ump_alloc_flags gpu_flags;

	KBASE_DEBUG_ASSERT(NULL != pages);

	umph = ump_dd_from_secure_id(id);
	if (UMP_DD_INVALID_MEMORY_HANDLE == umph)
		return NULL;

	ump_flags = ump_dd_allocation_flags_get(umph);
	cpu_flags = (ump_flags >> UMP_DEVICE_CPU_SHIFT) & UMP_DEVICE_MASK;
	gpu_flags = (ump_flags >> kctx->kbdev->memdev.ump_device_id) & UMP_DEVICE_MASK;

	vsize = ump_dd_size_get_64(umph);
	vsize >>= PAGE_SHIFT;

	reg = kbase_alloc_free_region(kctx, 0, vsize, KBASE_REG_ZONE_TMEM);
	if (!reg)
		goto out1;

	reg->flags &= ~KBASE_REG_FREE;
	reg->flags |= KBASE_REG_GPU_NX;	/* UMP is always No eXecute */
	reg->flags &= ~KBASE_REG_GROWABLE;	/* UMP cannot be grown */

	reg->imported_type = BASE_TMEM_IMPORT_TYPE_UMP;

	reg->imported_metadata.ump_handle = umph;

	if ((cpu_flags & (UMP_HINT_DEVICE_RD | UMP_HINT_DEVICE_WR)) == (UMP_HINT_DEVICE_RD | UMP_HINT_DEVICE_WR))
		reg->flags |= KBASE_REG_CPU_CACHED;

	if (cpu_flags & UMP_PROT_DEVICE_WR)
		reg->flags |= KBASE_REG_CPU_WR;

	if (cpu_flags & UMP_PROT_DEVICE_RD)
		reg->flags |= KBASE_REG_CPU_RD;

	if ((gpu_flags & (UMP_HINT_DEVICE_RD | UMP_HINT_DEVICE_WR)) == (UMP_HINT_DEVICE_RD | UMP_HINT_DEVICE_WR))
		reg->flags |= KBASE_REG_GPU_CACHED;

	if (gpu_flags & UMP_PROT_DEVICE_WR)
		reg->flags |= KBASE_REG_GPU_WR;

	if (gpu_flags & UMP_PROT_DEVICE_RD)
		reg->flags |= KBASE_REG_GPU_RD;

	/* ump phys block query */
	ump_dd_phys_blocks_get_64(umph, &block_count, &block_array);

	KBASE_DEBUG_ASSERT(0 != vsize);
	page_array = vmalloc_user(vsize * sizeof(*page_array));

	if (!page_array)
		goto out2;

	for (i = 0; i < block_count; i++) {
		for (j = 0; j < (block_array[i].size >> PAGE_SHIFT); j++) {
			page_array[page] = block_array[i].addr + (j << PAGE_SHIFT);
			page++;
		}
	}

	kbase_set_phy_pages(reg, page_array);

	reg->nr_alloc_pages = vsize;
	reg->extent = vsize;

	kbase_gpu_vm_lock(kctx);
	err = kbase_gpu_mmap(kctx, reg, 0, vsize, 1 /* no alignment */);
	kbase_gpu_vm_unlock(kctx);

	if (err) {
		KBASE_DEBUG_PRINT_WARN(KBASE_MEM, "kbase_gpu_mmap failed\n");
		goto out3;
	}

	*pages = vsize;

	return reg;

 out3:
	vfree(page_array);
 out2:
	kfree(reg);
 out1:
	ump_dd_release(umph);
	return NULL;

}

#endif				/* CONFIG_UMP */

#ifdef CONFIG_DMA_SHARED_BUFFER
static struct kbase_va_region *kbase_tmem_from_umm(kbase_context *kctx, int fd, u64 * const pages)
{
	struct kbase_va_region *reg;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *dma_attachment;
	phys_addr_t *page_array;
	unsigned long nr_pages;
	mali_error err;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf))
		goto no_buf;

	dma_attachment = dma_buf_attach(dma_buf, kctx->kbdev->osdev.dev);
	if (!dma_attachment)
		goto no_attachment;

	nr_pages = (dma_buf->size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	reg = kbase_alloc_free_region(kctx, 0, nr_pages, KBASE_REG_ZONE_TMEM);
	if (!reg)
		goto no_region;

	reg->flags &= ~KBASE_REG_FREE;
	reg->flags |= KBASE_REG_GPU_NX;	/* UMM is always No eXecute */
	reg->flags &= ~KBASE_REG_GROWABLE;	/* UMM cannot be grown */
	reg->flags |= KBASE_REG_GPU_CACHED;

	/* no read or write permission given on import, only on run do we give the right permissions */

	reg->imported_type = BASE_TMEM_IMPORT_TYPE_UMM;

	reg->imported_metadata.umm.st = NULL;
	reg->imported_metadata.umm.dma_buf = dma_buf;
	reg->imported_metadata.umm.dma_attachment = dma_attachment;
	reg->imported_metadata.umm.current_mapping_usage_count = 0;

	KBASE_DEBUG_ASSERT(0 != nr_pages);
	page_array = vmalloc_user(nr_pages * sizeof(*page_array));

	if (!page_array)
		goto no_page_array;

	memset(page_array, 0, nr_pages * sizeof(*page_array));

	kbase_set_phy_pages(reg, page_array);

	reg->nr_alloc_pages = nr_pages;
	reg->extent = nr_pages;

	kbase_gpu_vm_lock(kctx);
	err = kbase_add_va_region(kctx, reg, 0, nr_pages, 1);
	kbase_gpu_vm_unlock(kctx);
	if (err != MALI_ERROR_NONE)
		goto no_addr_reserve;

	*pages = nr_pages;

	return reg;

 no_addr_reserve:
	vfree(page_array);
 no_page_array:
	kfree(reg);
 no_region:
	dma_buf_detach(dma_buf, dma_attachment);
 no_attachment:
	dma_buf_put(dma_buf);
 no_buf:
	return NULL;
}
#endif				/* CONFIG_DMA_SHARED_BUFFER */

struct kbase_va_region *kbase_tmem_import(kbase_context *kctx, base_tmem_import_type type, int handle, u64 * const pages)
{
	switch (type) {
#ifdef CONFIG_UMP
	case BASE_TMEM_IMPORT_TYPE_UMP:
		{
			ump_secure_id id;
			id = (ump_secure_id) handle;
			return kbase_tmem_from_ump(kctx, id, pages);
		}
#endif				/* CONFIG_UMP */
#ifdef CONFIG_DMA_SHARED_BUFFER
	case BASE_TMEM_IMPORT_TYPE_UMM:
		return kbase_tmem_from_umm(kctx, handle, pages);
#endif				/* CONFIG_DMA_SHARED_BUFFER */
	default:
		return NULL;
	}
}

mali_error kbase_tmem_set_attributes(kbase_context *kctx, mali_addr64 gpu_addr, u32  attributes )
{
	kbase_va_region *reg;
	u32 requested_attributes = 0;
	u32 prev_attributes = 0;
	mali_error       ret = MALI_ERROR_FUNCTION_FAILED;

	KBASE_DEBUG_ASSERT(NULL != kctx);

	if( 0 == gpu_addr )
		return ret;

	kbase_gpu_vm_lock(kctx);

	/* Find the region */
	reg = kbase_region_tracker_find_region_base_address(kctx, gpu_addr);
	if (!reg || (reg->flags & KBASE_REG_FREE))
			goto out_unlock;

	/* Verify if this is actually a tmem region */
	if( !(reg->flags & KBASE_REG_ZONE_TMEM) )
		goto out_unlock;

	/* Verify if there is anything to be updated and if the attributes are valid */

	/* Get the attributes ( only ) */
	if( BASE_MEM_COHERENT_SYSTEM & attributes )
		requested_attributes |= KBASE_REG_SHARE_BOTH;
	else if ( BASE_MEM_COHERENT_LOCAL & attributes )
		requested_attributes |= KBASE_REG_SHARE_IN;

	if ( requested_attributes != ( reg->flags & (KBASE_REG_SHARE_BOTH | KBASE_REG_SHARE_IN)) )
	{
		prev_attributes = reg->flags;
		reg->flags &= ~(KBASE_REG_SHARE_BOTH|KBASE_REG_SHARE_IN);
		reg->flags |= requested_attributes;
	}
	else
	{
		/* Nothing to be updated - leave */
		ret = MALI_ERROR_NONE;
		goto out_unlock;
	}

	/* Currently supporting only imported memory */
	switch(reg->imported_type)
	{
#ifdef CONFIG_UMP
	case BASE_TMEM_IMPORT_TYPE_UMP:
		ret = kbase_mmu_update_pages(kctx, reg->start_pfn, kbase_get_phy_pages(reg), reg->nr_alloc_pages, reg->flags );
		break;
#endif
#ifdef 	CONFIG_DMA_SHARED_BUFFER
	case BASE_TMEM_IMPORT_TYPE_UMM:
		ret = MALI_ERROR_NONE;
		if(reg->imported_metadata.umm.current_mapping_usage_count)
		{
			/*Update the pages only if the dma buff is already mapped*/
			KBASE_DEBUG_ASSERT(reg->imported_metadata.umm.dma_attachment);
			KBASE_DEBUG_ASSERT(reg->imported_metadata.umm.st);

			ret = kbase_mmu_update_pages(kctx, reg->start_pfn, kbase_get_phy_pages(reg), reg->nr_alloc_pages, reg->flags | KBASE_REG_GPU_WR | KBASE_REG_GPU_RD );

		}

		break;
#endif
	default:
		break;
	}

	if( MALI_ERROR_NONE != ret )
	{
		/* Rollback case failed to update page tables */
		reg->flags = prev_attributes;
	}

out_unlock:
	kbase_gpu_vm_unlock(kctx);
	return ret;
}
KBASE_EXPORT_TEST_API(kbase_tmem_set_attributes)

mali_error kbase_tmem_get_attributes(kbase_context *kctx, mali_addr64 gpu_addr, u32 * const attributes )
{
	kbase_va_region *reg;
	u32 current_attributes = 0;
	mali_error   ret = MALI_ERROR_FUNCTION_FAILED;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(NULL != attributes);

	if( 0 == gpu_addr )
		return ret;

	kbase_gpu_vm_lock(kctx);

	/* Find the region */
	reg = kbase_region_tracker_find_region_base_address(kctx, gpu_addr);
	if (!reg || (reg->flags & KBASE_REG_FREE))
			goto out_unlock;

	/* Verify if this is actually a tmem region */
	if( !(reg->flags & KBASE_REG_ZONE_TMEM) )
		goto out_unlock;

	/* Verify if this is an imported memory */
	if(BASE_TMEM_IMPORT_TYPE_INVALID != reg->imported_type)
	{
		/* Get the attributes ( only ) */
		if( KBASE_REG_GPU_WR & reg->flags )
			current_attributes |= BASE_MEM_PROT_GPU_WR;
		if( KBASE_REG_GPU_RD & reg->flags )
			current_attributes |= BASE_MEM_PROT_GPU_RD;
		if( KBASE_REG_GPU_NX & reg->flags )
			current_attributes |= BASE_MEM_PROT_GPU_EX;
		if( KBASE_REG_SHARE_BOTH & reg->flags )
			current_attributes |= BASE_MEM_COHERENT_SYSTEM;
		if ( KBASE_REG_SHARE_IN & reg->flags )
			current_attributes |= BASE_MEM_COHERENT_LOCAL;
	}

	*attributes = current_attributes;
	ret = MALI_ERROR_NONE;

out_unlock:
	kbase_gpu_vm_unlock(kctx);
	return ret;
}
KBASE_EXPORT_TEST_API(kbase_tmem_get_attributes)


/**
 * @brief Acquire the per-context region list lock
 */
void kbase_gpu_vm_lock(kbase_context *kctx)
{
	KBASE_DEBUG_ASSERT(kctx != NULL);
	mutex_lock(&kctx->reg_lock);
}

KBASE_EXPORT_TEST_API(kbase_gpu_vm_lock)

/**
 * @brief Release the per-context region list lock
 */
void kbase_gpu_vm_unlock(kbase_context *kctx)
{
	KBASE_DEBUG_ASSERT(kctx != NULL);
	mutex_unlock(&kctx->reg_lock);
}

KBASE_EXPORT_TEST_API(kbase_gpu_vm_unlock)
