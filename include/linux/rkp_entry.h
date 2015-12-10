/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _RKP_ENTRY_H
#define _RKP_ENTRY_H

#ifndef __ASSEMBLY__
#define RKP_PREFIX  UL(0x83800000)
#define RKP_CMDID(CMD_ID)  ((UL(CMD_ID) << 12 ) | RKP_PREFIX)

#define RKP_PGD_SET  RKP_CMDID(0x21)
#define RKP_PMD_SET  RKP_CMDID(0x22)
#define RKP_PTE_SET  RKP_CMDID(0x23)
#define RKP_PGD_FREE RKP_CMDID(0x24)
#define RKP_PGD_NEW  RKP_CMDID(0x25)

#define RKP_INIT	 RKP_CMDID(0)
#define RKP_DEF_INIT	 RKP_CMDID(1)

#define RKP_INIT_MAGIC 0x5afe0001
#define RKP_VMM_BUFFER 0x600000
#define RKP_RO_BUFFER  UL(0x800000)

/*** TODO: We need to export this so it is hard coded 
     at one place*/
#define RKP_ROBUF_START  UL(0x52400000)
#define RKP_RBUF_VA      (phys_to_virt(RKP_ROBUF_START))
#define RO_PAGES_ORDER 11  // (8MB/4KB)

#define RKP_PGT_BITMAP_LEN 0x18000
extern u8 rkp_pgt_bitmap[];

typedef struct rkp_init rkp_init_t;
extern u8 rkp_started;
extern void *rkp_ro_alloc(void);
extern void rkp_ro_free(void *free_addr);
#ifdef CONFIG_KNOX_KAP
extern int boot_mode_security;
#endif  //CONFIG_KNOX_KAP

struct rkp_init {
	u32 magic;
	u64 vmalloc_start;
	u64 vmalloc_end;
	u64 init_mm_pgd;
	u64 id_map_pgd;
	u64 zero_pg_addr;
	u64 rkp_pgt_bitmap;
	u32 rkp_pgt_bitmap_size;
	u64 _text;
	u64 _etext;
	u64 extra_memory_addr;
	u32 extra_memory_size;
	u64 physmap_addr;
	u64 _srodata;
	u64 _erodata;
} __attribute__((packed));

#ifdef CONFIG_RKP_KDP
typedef struct kdp_init
{
	u32 credSize;
	u32 cred_task;
	u32 mm_task;
	u32 uid_cred;
	u32 euid_cred;
	u32 bp_pgd_cred;
	u32 bp_task_cred;
	u32 type_cred;
	u32 security_cred;
	u32 pid_task;
	u32 rp_task;
	u32 comm_task;
	u32 pgd_mm;
	u32 usage_cred;
	u32 task_threadinfo;
} kdp_init_t;
#endif  /* CONFIG_RKP_KDP */

void rkp_call(unsigned long long cmd, unsigned long long arg0, unsigned long long arg1, unsigned long long arg2, unsigned long long arg3, unsigned long long arg4);

static inline u8 rkp_is_pg_protected(u64 va)
{
	u64 paddr = __pa(va) - PHYS_OFFSET;
	u64 index = (paddr>>PAGE_SHIFT);
	u64 *p = (u64 *)rkp_pgt_bitmap;
	u64 tmp = (index>>6);
	u64 rindex;
	u8 val;
	
	p += (tmp);
	rindex = index % 64;
	val = (((*p) & (1ULL<<rindex))?1:0);
	return val;
}

#endif //__ASSEMBLY__

#endif //_RKP_ENTRY_H
