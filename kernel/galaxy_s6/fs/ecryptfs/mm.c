/*
 * mm.c
 *
 *  Created on: Jul 21, 2014
 *      Author: olic
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include "ecryptfs_kernel.h"

extern spinlock_t inode_sb_list_lock;
static int ecryptfs_mm_debug = 0;
DEFINE_MUTEX(ecryptfs_mm_mutex);

struct ecryptfs_mm_drop_cache_param {
    int user_id;
};

static void ecryptfs_mm_drop_pagecache(struct super_block *sb, void *arg)
{
	struct inode *inode, *toput_inode = NULL;
	struct ecryptfs_mm_drop_cache_param *param = arg;

	//printk("%s() sb:%s, userid:%d\n", __func__, sb->s_type->name, param->user_id);

	if(!strcmp("ecryptfs", sb->s_type->name)) {
		struct ecryptfs_mount_crypt_stat *mount_crypt_stat =
				&(ecryptfs_superblock_to_private(sb)->mount_crypt_stat);

		if(mount_crypt_stat->userid == param->user_id) {
			struct ecryptfs_crypt_stat *crypt_stat;

			printk("%s() sb found userid:%d\n", __func__, param->user_id);

			spin_lock(&inode_sb_list_lock);
			list_for_each_entry(inode, &sb->s_inodes, i_sb_list)
			{
				int retries = 3;
				unsigned long ret;

				spin_lock(&inode->i_lock);
				crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;

				if(ecryptfs_mm_debug)
					printk("%s() inode found. [ino:%lu, state:%lu ref_count:%d efs_flag:0x%8x]\n",
						__func__, inode->i_ino,  inode->i_state, atomic_read(&inode->i_count),
						crypt_stat->flags);

				if ((inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW)) ||
						(inode->i_mapping->nrpages == 0)) {
					spin_unlock(&inode->i_lock);
					continue;
				}
				__iget(inode);
				spin_unlock(&inode->i_lock);

				spin_unlock(&inode_sb_list_lock);

				if(crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
					printk("freeing sensitive inode[mapped pagenum = %lu]\n",
							inode->i_mapping->nrpages);
					retry:
					ret = invalidate_mapping_pages(inode->i_mapping, 0, -1);
					if(ecryptfs_mm_debug)
						printk("invalidate_mapping_pages ret = %lu, [%lu] remained\n",
							ret, inode->i_mapping->nrpages);

					if(inode->i_mapping->nrpages != 0) {

						if(retries > 0) {
							printk("[%lu] mapped pages remained in sensitive inode, retry..\n",
									inode->i_mapping->nrpages);
							retries--;
							msleep(100);
							goto retry;
						} else {
							printk("[%lu] mapped pages remained in sensitive inode, Give up..\n",
									inode->i_mapping->nrpages);
						}

					}
				}else{
					printk("skipping protected inode\n");
				}

				iput(toput_inode);
				toput_inode = inode;
				spin_lock(&inode_sb_list_lock);
			}
			spin_unlock(&inode_sb_list_lock);
			iput(toput_inode);
		}
	}
}

static int
ecryptfs_mm_task(void *arg)
{
    struct ecryptfs_mm_drop_cache_param *param = arg;

	mutex_lock(&ecryptfs_mm_mutex);
	iterate_supers(ecryptfs_mm_drop_pagecache, param);
	mutex_unlock(&ecryptfs_mm_mutex);

	kfree(param);
	return 0;
}

void ecryptfs_mm_drop_cache(int userid) {
#if 1
	struct task_struct *task;
	struct ecryptfs_mm_drop_cache_param *param =
	        kzalloc(sizeof(*param), GFP_KERNEL);

    if (!param) {
        printk("%s :: skip. no memory to alloc param\n", __func__);
        return;
    }
    param->user_id = userid;

	printk("running cache cleanup thread - sdp-id : %d \n", userid);
	task = kthread_run(ecryptfs_mm_task, param, "sdp_cached");

	if (IS_ERR(task)) {
		printk(KERN_ERR "SDP : unable to create kernel thread: %ld\n",
		       PTR_ERR(task));
	}
#else
	ecryptfs_mm_task(&userid);
#endif
}

#include <linux/pagevec.h>
#include <linux/pagemap.h>
#include <linux/memcontrol.h>
#include <linux/atomic.h>

static void __page_dump(unsigned char *buf, int len, const char* str)
{
    unsigned int     i;
    char	s[512];

    s[0] = 0;
    for(i=0;i<len && i<16;++i) {
        char tmp[8];
        sprintf(tmp, " %02x", buf[i]);
        strcat(s, tmp);
    }

    if (len > 16) {
        char tmp[8];
        sprintf(tmp, " ...");
        strcat(s, tmp);
    }

    DEK_LOGD("%s [%s len=%d]\n", s, str, len);
}

#ifdef DEK_DEBUG
/*
 * This dump will appear in ramdump
 */
void page_dump (struct page *p) {
	void *d;
	d = kmap_atomic(p);
	if(d) {
		__page_dump((unsigned char *)d, PAGE_SIZE, "freeing");
		kunmap_atomic(d);
	}
}
#else
void page_dump (struct page *p) {
	// Do nothing
}
#endif
/**
 * set_sensitive_mapping_pages - Set sensitive all the unlocked pages of one inode
 * @mapping: the address_space which holds the pages to invalidate
 * @start: the offset 'from' which to invalidate
 * @end: the offset 'to' which to invalidate (inclusive)
 *
 * This function only Sets sensitive the unlocked pages.
 *
 * Return value
 * 0 : all pages in the mapping set as sensitive
 * n > 0 : number of normal pages
 * n < 0 : error
 */
unsigned long set_sensitive_mapping_pages(struct address_space *mapping,
		pgoff_t start, pgoff_t end)
{
	struct pagevec pvec;
	pgoff_t index = start;
	unsigned long count = 0;
	int i;

	/*
	 * Note: this function may get called on a shmem/tmpfs mapping:
	 * pagevec_lookup() might then return 0 prematurely (because it
	 * got a gangful of swap entries); but it's hardly worth worrying
	 * about - it can rarely have anything to free from such a mapping
	 * (most pages are dirty), and already skips over any difficulties.
	 */

	pagevec_init(&pvec, 0);
	while (index <= end && pagevec_lookup(&pvec, mapping, index,
			min(end - index, (pgoff_t)PAGEVEC_SIZE - 1) + 1)) {
		mem_cgroup_uncharge_start();
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];
			DEK_LOGD("%s : page [flag:0x%ld] freed\n", __func__, page->flags);
			DEK_LOGD("mapping->nrpages:%lu(now index:%lu) page count:%d, _mapcount:%d\n",
					mapping->nrpages, index,
					atomic_read(&page->_count), atomic_read(&(page)->_mapcount));
			//page_dump(page);
			/* We rely upon deletion not changing page->index */
			index = page->index;
			if (index > end)
				break;

			if (!trylock_page(page))
				continue;
			WARN_ON(page->index != index);
			SetPageSensitive(page);
			unlock_page(page);
			count++;
		}
		pagevec_release(&pvec);
		mem_cgroup_uncharge_end();
		cond_resched();
		index++;
	}
	return mapping->nrpages - count;
}
