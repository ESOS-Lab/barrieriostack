/*
 * mm.h
 *
 *  Created on: Aug 20, 2014
 *      Author: olic
 */

#ifndef ECRYPTFS_MM_H_
#define ECRYPTFS_MM_H_

void ecryptfs_mm_drop_cache(int userid);

unsigned long set_sensitive_mapping_pages(struct address_space *mapping,
		pgoff_t start, pgoff_t end);

void page_dump (struct page *p);

#endif /* ECRYPTFS_MM_H_ */
