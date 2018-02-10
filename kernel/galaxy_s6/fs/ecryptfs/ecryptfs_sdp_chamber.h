/*
 * ecryptfs_sdp_chamber.h
 *
 *  Created on: Dec 19, 2014
 *      Author: olic.moon@samsung.com
 */

#ifndef ECRYPTFS_SDP_CHAMBER_H_
#define ECRYPTFS_SDP_CHAMBER_H_

#include "ecryptfs_kernel.h"

int add_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		char *path);
void del_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		char *path);
int is_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		char *path);

void set_chamber_flag(struct inode *inode);

#define IS_UNDER_ROOT(dentry) (dentry->d_parent->d_inode == dentry->d_sb->s_root->d_inode)
#define IS_CHAMBER_DENTRY(dentry) (ecryptfs_inode_to_private(dentry->d_inode)->crypt_stat.flags & ECRYPTFS_SDP_IS_CHAMBER_DIR)
#define IS_SENSITIVE_DENTRY(dentry) (ecryptfs_inode_to_private(dentry->d_inode)->crypt_stat.flags & ECRYPTFS_DEK_IS_SENSITIVE)

#endif /* ECRYPTFS_SDP_CHAMBER_H_ */
