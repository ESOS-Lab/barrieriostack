#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "ecryptfs_sdp_chamber.h"

#include <sdp/common.h>

#define CHAMBER_PATH_MAX 512
typedef struct __chamber_info {
	int userid;

	struct list_head list;
	char path[CHAMBER_PATH_MAX];
}chamber_info_t;

#define NO_DIRECTORY_SEPARATOR_IN_CHAMBER_PATH 1
/* Debug */
#define CHAMBER_DEBUG		0

#if CHAMBER_DEBUG
#define CHAMBER_LOGD(FMT, ...) printk("SDP_CHAMBER[%d] %s :: " FMT , current->pid, __func__, ##__VA_ARGS__)
#else
#define CHAMBER_LOGD(FMT, ...)
#endif /* PUB_CRYPTO_DEBUG */
#define CHAMBER_LOGE(FMT, ...) printk("SDP_CHAMBER[%d] %s :: " FMT , current->pid, __func__, ##__VA_ARGS__)


chamber_info_t *alloc_chamber_info(int userid, char *path) {
	chamber_info_t *new_chamber = kmalloc(sizeof(chamber_info_t), GFP_KERNEL);

	if(new_chamber == NULL) {
		CHAMBER_LOGE("can't alloc memory for chamber_info\n");
		return NULL;
	}

	new_chamber->userid = userid;
	snprintf(new_chamber->path, CHAMBER_PATH_MAX, "%s", path);

	return new_chamber;
}

int add_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		char *path) {
	chamber_info_t *new_chamber = NULL;

#if NO_DIRECTORY_SEPARATOR_IN_CHAMBER_PATH
	if(strchr(path, '/') != NULL) {
		CHAMBER_LOGE("Chamber directory cannot contain '/'\n");
		return -EINVAL;
	}
#endif

	new_chamber = alloc_chamber_info(mount_crypt_stat->userid, path);

	if(new_chamber == NULL) {
		return -ENOMEM;
	}

	spin_lock(&(mount_crypt_stat->chamber_dir_list_lock));
	CHAMBER_LOGD("Adding %s into chamber list\n", new_chamber->path);
	list_add_tail(&new_chamber->list, &(mount_crypt_stat->chamber_dir_list));
	spin_unlock(&(mount_crypt_stat->chamber_dir_list_lock));

	return 0;
}

chamber_info_t *find_chamber_info(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		char *path) {
	struct list_head *entry;

	spin_lock(&(mount_crypt_stat->chamber_dir_list_lock));

	CHAMBER_LOGD("%s\n", path);

	list_for_each(entry, &mount_crypt_stat->chamber_dir_list) {
		chamber_info_t *info;
		info = list_entry(entry, chamber_info_t, list);

		// Check path
		if(!strncmp(path, info->path, CHAMBER_PATH_MAX)) {
			CHAMBER_LOGD("Found %s from chamber list\n", info->path);

			spin_unlock(&(mount_crypt_stat->chamber_dir_list_lock));
			return info;
		}
	}

	spin_unlock(&(mount_crypt_stat->chamber_dir_list_lock));
	CHAMBER_LOGD("Not found\n");

	return NULL;
}

void del_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		char *path) {
	chamber_info_t *info = find_chamber_info(mount_crypt_stat, path);
	if(info == NULL) {
		CHAMBER_LOGD("nothing to remove\n");
		return;
	}

	spin_lock(&mount_crypt_stat->chamber_dir_list_lock);
	CHAMBER_LOGD("%s removed\n", info->path);
	list_del(&info->list);

	kfree(info);
	spin_unlock(&mount_crypt_stat->chamber_dir_list_lock);
}

int is_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		char *path) {
#if NO_DIRECTORY_SEPARATOR_IN_CHAMBER_PATH
	if(strchr(path, '/') != NULL) {
		CHAMBER_LOGD("%s containes '/'\n", path);
		return 0;
	}
#endif

	if(find_chamber_info(mount_crypt_stat, path) != NULL)
		return 1;

	return 0;
}

void set_chamber_flag(struct inode *inode) {
	struct ecryptfs_crypt_stat *crypt_stat;

	if(inode == NULL) {
		CHAMBER_LOGE("invalid inode\n");
		return;
	}

	crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;

	crypt_stat->flags |= ECRYPTFS_SDP_IS_CHAMBER_DIR;
	crypt_stat->flags |= ECRYPTFS_DEK_IS_SENSITIVE;
}
