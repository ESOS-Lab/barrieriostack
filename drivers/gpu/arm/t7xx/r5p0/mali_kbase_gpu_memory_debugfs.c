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



#include <mali_kbase_gpu_memory_debugfs.h>

#ifdef CONFIG_DEBUG_FS
/** Show callback for the @c gpu_memory debugfs file.
 *
 * This function is called to get the contents of the @c gpu_memory debugfs
 * file. This is a report of current gpu memory usage.
 *
 * @param sfile The debugfs entry
 * @param data Data associated with the entry
 *
 * @return 0 if successfully prints data in debugfs entry file
 *         -1 if it encountered an error
 */

static int kbasep_gpu_memory_seq_show(struct seq_file *sfile, void *data)
{
	ssize_t ret = 0;
	struct list_head *entry;
	const struct list_head *kbdev_list;
/* MALI_SEC_INTEGRATION */
	size_t free_size = 0;

	kbdev_list = kbase_dev_list_get();
	list_for_each(entry, kbdev_list) {
		struct kbase_device *kbdev = NULL;
		struct kbasep_kctx_list_element *element;

		kbdev = list_entry(entry, struct kbase_device, entry);
		/* output the total memory usage and cap for this device */
/* MALI_SEC_INTEGRATION */
		mutex_lock(&kbdev->kctx_list_lock);
		list_for_each_entry(element, &kbdev->kctx_list, link) {
			free_size += atomic_read(&(element->kctx->osalloc.free_list_size));
		}
		mutex_unlock(&kbdev->kctx_list_lock);
		ret = seq_printf(sfile, "===========================================================\n");
		ret = seq_printf(sfile, " %16s  %18s  %20s\n", \
				"dev name", \
				"total used pages", \
				"total shrink pages");
		ret = seq_printf(sfile, "-----------------------------------------------------------\n");
		ret = seq_printf(sfile, " %16s  %18u  %20zu\n", \
				kbdev->devname, \
				atomic_read(&(kbdev->memdev.used_pages)), \
				free_size);
		ret = seq_printf(sfile, "===========================================================\n\n");
		ret = seq_printf(sfile, "%28s     %10s  %12s  %10s  %10s  %10s  %10s\n", \
				"context name", \
				"context addr", \
				"used pages", \
				"shrink pages", \
				"pmem pages", \
				"tmem pages", \
				"others");
		ret = seq_printf(sfile, "====================================================");
		ret = seq_printf(sfile, "=========================================================\n");
		mutex_lock(&kbdev->kctx_list_lock);
		list_for_each_entry(element, &kbdev->kctx_list, link) {
			/* output the memory usage and cap for each kctx
			* opened on this device */

			ret = seq_printf(sfile, "  (%24s), %s-0x%p    %12u  %10u  %10u  %10u  %10u\n", \
				element->kctx->name, \
				"kctx", \
				element->kctx, \
				atomic_read(&(element->kctx->used_pages)),
				atomic_read(&(element->kctx->osalloc.free_list_size)),
				atomic_read(&(element->kctx->used_pmem_pages)),
				atomic_read(&(element->kctx->used_tmem_pages)),
				atomic_read(&(element->kctx->used_pages)) - atomic_read(&(element->kctx->used_pmem_pages)) - atomic_read(&(element->kctx->used_tmem_pages)));
		}
		mutex_unlock(&kbdev->kctx_list_lock);

/* MALI_SEC_INTEGRATION */
#if 0
		ret = seq_printf(sfile, "%-16s  %10u\n", \
				kbdev->devname, \
				atomic_read(&(kbdev->memdev.used_pages)));
		mutex_lock(&kbdev->kctx_list_lock);
		list_for_each_entry(element, &kbdev->kctx_list, link) {
			/* output the memory usage and cap for each kctx
			* opened on this device */
			ret = seq_printf(sfile, "  %s-0x%p %10u\n", \
				"kctx",
				element->kctx, \
				atomic_read(&(element->kctx->used_pages)));
		}
		mutex_unlock(&kbdev->kctx_list_lock);
#endif
	}
	kbase_dev_list_put(kbdev_list);
	return ret;
}

/*
 *  File operations related to debugfs entry for gpu_memory
 */
STATIC int kbasep_gpu_memory_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, kbasep_gpu_memory_seq_show , NULL);
}

static const struct file_operations kbasep_gpu_memory_debugfs_fops = {
	.open = kbasep_gpu_memory_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release_private,
};

/*
 *  Initialize debugfs entry for gpu_memory
 */
mali_error kbasep_gpu_memory_debugfs_init(struct kbase_device *kbdev)
{
	kbdev->gpu_memory_dentry = debugfs_create_file("gpu_memory", \
					S_IRUGO, \
					kbdev->mali_debugfs_directory, \
					NULL, \
					&kbasep_gpu_memory_debugfs_fops);
	if (IS_ERR(kbdev->gpu_memory_dentry))
		return MALI_ERROR_FUNCTION_FAILED;

	return MALI_ERROR_NONE;
}

/*
 *  Terminate debugfs entry for gpu_memory
 */
void kbasep_gpu_memory_debugfs_term(struct kbase_device *kbdev)
{
	debugfs_remove(kbdev->gpu_memory_dentry);
}
#else
/*
 * Stub functions for when debugfs is disabled
 */
mali_error kbasep_gpu_memory_debugfs_init(struct kbase_device *kbdev)
{
	return MALI_ERROR_NONE;
}
void kbasep_gpu_memory_debugfs_term(struct kbase_device *kbdev)
{
}
#endif
