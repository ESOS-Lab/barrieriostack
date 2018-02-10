/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
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
#include <mali_kbase.h>
#include <linux/highmem.h>
#include <linux/mempool.h>
#include <linux/mm.h>
#include <linux/atomic.h>

#if SLSI_INTEGRATION
#define MEM_FREE_LIMITS  16384
#define MEM_FREE_DEFAULT 16384
#endif

static unsigned long kbase_mem_allocator_count(struct shrinker *s,
						struct shrink_control *sc)
{
	kbase_mem_allocator *allocator;
	allocator = container_of(s, kbase_mem_allocator, free_list_reclaimer);
	return atomic_read(&allocator->free_list_size);
}

static unsigned long kbase_mem_allocator_scan(struct shrinker *s,
						struct shrink_control *sc)
{
	kbase_mem_allocator *allocator;
	int i;
	int freed;

	allocator = container_of(s, kbase_mem_allocator, free_list_reclaimer);

	might_sleep();

	mutex_lock(&allocator->free_list_lock);
	i = MIN(atomic_read(&allocator->free_list_size), sc->nr_to_scan);
	freed = i;

	atomic_sub(i, &allocator->free_list_size);

	while (i--) {
		struct page *p;

		BUG_ON(list_empty(&allocator->free_list_head));
		p = list_first_entry(&allocator->free_list_head,
					struct page, lru);
		list_del(&p->lru);
		__free_page(p);
	}
	mutex_unlock(&allocator->free_list_lock);
	return atomic_read(&allocator->free_list_size);

}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
static int kbase_mem_allocator_shrink(struct shrinker *s,
					struct shrink_control *sc)
{
	if (sc->nr_to_scan == 0)
		return kbase_mem_allocator_count(s, sc);
	else
		return kbase_mem_allocator_scan(s, sc);
}
#endif

mali_error kbase_mem_allocator_init(kbase_mem_allocator *const allocator,
					unsigned int max_size)
{
	KBASE_DEBUG_ASSERT(NULL != allocator);

	INIT_LIST_HEAD(&allocator->free_list_head);

	mutex_init(&allocator->free_list_lock);

	atomic_set(&allocator->free_list_size, 0);

	allocator->free_list_max_size = max_size;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
	allocator->free_list_reclaimer.shrink = kbase_mem_allocator_shrink;
#else
	allocator->free_list_reclaimer.count_objects =
						kbase_mem_allocator_count;
	allocator->free_list_reclaimer.scan_objects = kbase_mem_allocator_scan;
#endif
	allocator->free_list_reclaimer.seeks = DEFAULT_SEEKS;
	/* Kernel versions prior to 3.1 :
	 * struct shrinker does not define batch */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0)
	allocator->free_list_reclaimer.batch = 0;
#endif

	register_shrinker(&allocator->free_list_reclaimer);

	return MALI_ERROR_NONE;
}
KBASE_EXPORT_TEST_API(kbase_mem_allocator_init)

void kbase_mem_allocator_term(kbase_mem_allocator *allocator)
{
	KBASE_DEBUG_ASSERT(NULL != allocator);

	unregister_shrinker(&allocator->free_list_reclaimer);
	mutex_lock(&allocator->free_list_lock);
	while (!list_empty(&allocator->free_list_head))
	{
		struct page * p;
		p = list_first_entry(&allocator->free_list_head, struct page, lru);
		list_del(&p->lru);
		__free_page(p);
	}
	atomic_set(&allocator->free_list_size, 0);
	mutex_unlock(&allocator->free_list_lock);
	mutex_destroy(&allocator->free_list_lock);
}
KBASE_EXPORT_TEST_API(kbase_mem_allocator_term)

mali_error kbase_mem_allocator_alloc(kbase_mem_allocator *allocator, size_t nr_pages, phys_addr_t *pages)
{
	struct page * p;
	void * mp;
	int i;
	int num_from_free_list;
	struct list_head from_free_list = LIST_HEAD_INIT(from_free_list);

	might_sleep();

	KBASE_DEBUG_ASSERT(NULL != allocator);

	/* take from the free list first */
	mutex_lock(&allocator->free_list_lock);
	num_from_free_list = MIN(nr_pages, atomic_read(&allocator->free_list_size));
	atomic_sub(num_from_free_list, &allocator->free_list_size);
	for (i = 0; i < num_from_free_list; i++)
	{
		BUG_ON(list_empty(&allocator->free_list_head));
		p = list_first_entry(&allocator->free_list_head, struct page, lru);
		list_move(&p->lru, &from_free_list);
	}
	mutex_unlock(&allocator->free_list_lock);
	i = 0;

	/* Allocate as many pages from the pool of already allocated pages. */
	list_for_each_entry(p, &from_free_list, lru)
	{
		pages[i] = PFN_PHYS(page_to_pfn(p));
		i++;
	}

	if (i == nr_pages)
		return MALI_ERROR_NONE;

	/* If not all pages were sourced from the pool, request new ones. */
	for (; i < nr_pages; i++)
	{
		p = alloc_page(GFP_HIGHUSER);
		if (NULL == p)
		{
			goto err_out_roll_back;
		}
		mp = kmap(p);
		if (NULL == mp)
		{
			__free_page(p);
			goto err_out_roll_back;
		}
		memset(mp, 0x00, PAGE_SIZE); /* instead of __GFP_ZERO, so we can do cache maintenance */
		kbase_sync_to_memory(PFN_PHYS(page_to_pfn(p)), mp, PAGE_SIZE);
		kunmap(p);
		pages[i] = PFN_PHYS(page_to_pfn(p));
	}

	return MALI_ERROR_NONE;

err_out_roll_back:
	while (i--)
	{
		struct page * p;
		p = pfn_to_page(PFN_DOWN(pages[i]));
		pages[i] = (phys_addr_t)0;
		__free_page(p);
	}

	return MALI_ERROR_OUT_OF_MEMORY;
}
KBASE_EXPORT_TEST_API(kbase_mem_allocator_alloc)

#if SLSI_INTEGRATION
void kbase_mem_set_max_size(struct kbase_context *kctx)
{
	struct kbase_mem_allocator *allocator = &kctx->osalloc;
	mutex_lock(&allocator->free_list_lock);
	allocator->free_list_max_size = MEM_FREE_DEFAULT;
	mutex_unlock(&allocator->free_list_lock);
}
KBASE_EXPORT_TEST_API(kbase_mem_set_max_size)

void kbase_mem_free_list_cleanup(kbase_context *kctx)
{
	int tofree,i=0;
	kbase_mem_allocator *allocator = &kctx->osalloc;
	mutex_lock(&allocator->free_list_lock);
	tofree = MAX(MEM_FREE_LIMITS, atomic_read(&allocator->free_list_size)) - MEM_FREE_LIMITS;
	if (tofree > 0)
	{
		struct page * p;
		allocator->free_list_max_size = MEM_FREE_LIMITS;
		for(i=0; i < tofree; i++)
		{
			p = list_first_entry(&allocator->free_list_head, struct page, lru);
			list_del(&p->lru);
			__free_page(p);
		}
		atomic_set(&allocator->free_list_size, MEM_FREE_LIMITS);
	}
	mutex_unlock(&allocator->free_list_lock);
}
KBASE_EXPORT_TEST_API(kbase_mem_free_list_cleanup)
#endif

void kbase_mem_allocator_free(kbase_mem_allocator *allocator, size_t nr_pages, phys_addr_t *pages, mali_bool sync_back)
{
	int i = 0;
	int page_count = 0;
	int tofree;

	LIST_HEAD(new_free_list_items);

	KBASE_DEBUG_ASSERT(NULL != allocator);

	might_sleep();

	/* Starting by just freeing the overspill.
	* As we do this outside of the lock we might spill too many pages
	* or get too many on the free list, but the max_size is just a ballpark so it is ok
	* providing that tofree doesn't exceed nr_pages
	*/
	tofree = MAX((int)allocator->free_list_max_size - atomic_read(&allocator->free_list_size),0);
	tofree = nr_pages - MIN(tofree, nr_pages);
	for (; i < tofree; i++)
	{
		if (likely(0 != pages[i]))
		{
			struct page * p;

			p = pfn_to_page(PFN_DOWN(pages[i]));
			pages[i] = (phys_addr_t)0;
			__free_page(p);
		}
	}

	for (; i < nr_pages; i++)
	{
		if (likely(0 != pages[i]))
		{
			struct page * p;

			p = pfn_to_page(PFN_DOWN(pages[i]));
			pages[i] = (phys_addr_t)0;
			/* Sync back the memory to ensure that future cache invalidations
			 * don't trample on memory.
			 */
			if( sync_back )
			{
				void* mp = kmap(p);
				if( NULL != mp)
				{
					kbase_sync_to_cpu(PFN_PHYS(page_to_pfn(p)), mp, PAGE_SIZE);
					kunmap(p);
				}

			}
			list_add(&p->lru, &new_free_list_items);
			page_count++;
		}
	}
	mutex_lock(&allocator->free_list_lock);
	list_splice(&new_free_list_items, &allocator->free_list_head);
	atomic_add(page_count, &allocator->free_list_size);
	mutex_unlock(&allocator->free_list_lock);
}
KBASE_EXPORT_TEST_API(kbase_mem_allocator_free)

