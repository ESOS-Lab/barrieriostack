/*
 *  Copyright (C) 2013, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef SEC_FILTER_H_
#define SEC_FILTER_H_
#include    <linux/version.h>
#include    <linux/init.h>
#include    <linux/kernel.h>
#include    <linux/module.h>
#include    <linux/netfilter.h>
#include    <linux/netfilter_ipv4.h>
#include    <linux/ip.h>
#include    <linux/tcp.h>
#include    <linux/netdevice.h>
#include    <linux/fs.h>
#include    <linux/cdev.h>
#include    <linux/wait.h>
#include    <linux/seqlock.h>
#include    <linux/spinlock.h>
#include    <net/netfilter/nf_queue.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)  // from 3.8, nf_queue functions does not use protocol family
#define     _PF_DEFINED
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)  
#define     _SKB_FRAG_STRUCT_CHANGED                // from version 3.2, the structre  skb_frag_struct was changed
#endif

#ifdef CONFIG_SEC_NET_FILTER
int nfqnl_enqueue_packet(struct nf_queue_entry *entry, unsigned int queuenum);
#endif

void    clean_Managers(void);


#define     PREPACKET_BUFFER_SIZE		11
#define     ALLOWED_ID                  5001

//  NOTIFICATION INFO.S START
#define FILTER_VERSION  0
#define URL_TYPE        0
#define URL_REFER_TYPE  1
#define DNS_TYPE        2
//  NOTIFICATION INFO.S END

//  USER COMMANDS   START
#define SET_FILTER_MODE     0
#define SET_USER_SELECT     1
#define SET_EXCEPTION_URL   2
#define SET_ERROR_MSG       3
#define SET_CLOSING_TIME    4
#define MAX_CMDS            5
//  USER COMMANDS END

#define FILTER_MODE_OFF                 0
#define	FILTER_MODE_CLOSING             -1
#define FILTER_MODE_ON_BLOCK            1
#define FILTER_MODE_ON_RESPONSE         2
#define FILTER_MODE_ON_BLOCK_REFER      11
#define FILTER_MODE_ON_RESPONSE_REFER   12

#define	SEC_MODULE_NAME "Samsung_URL_Filter"
//  MACRO FUNCTION START
#define SEC_FREE(x)                         if((x)!=NULL){kfree(x);(x)=NULL;}
//#define	USE_ATOMIC
#ifdef USE_ATOMIC
#define SEC_MALLOC(x)                       kzalloc((x),(in_atomic()==0)?GFP_KERNEL:GFP_ATOMIC)
#define SEC_spin_lock_irqsave(x, y)         spin_lock_irqsave((x), (y))
#define SEC_spin_unlock_irqrestore(x, y)    spin_unlock_irqrestore((x), (y))
#else
#define SEC_MALLOC(x)                       kzalloc((x),GFP_ATOMIC)
#define SEC_spin_lock_irqsave(x, y)         spin_lock_irqsave((x), (y))
#define SEC_spin_unlock_irqrestore(x, y)    spin_unlock_irqrestore((x), (y))
#endif
//  MACRO FUNCTION END

extern  int     filterMode;
extern  char    *errorMsg;
extern  int     errMsgSize;

#endif /* SEC_FILTER_H_ */
