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

#ifndef __TCP_TRACKINFO__
#define __TCP_TRACKINFO__
#include    "sec_filter.h"

#include    <linux/netfilter.h>
#include    <linux/netfilter_ipv4.h>
#include    <linux/ip.h>
#include    <linux/tcp.h>

#define NOTSET          0
#define GETTING_URL     1
#define GOT_URL         2
#define GETTING_HOST    3
#define GETTING_REFERER 4
#define GOT_FULL_URL    5
#define NOTIFIED        6



#define BLOCK           100
#define ALLOW           101


typedef	struct __TCP_TRACKINFO
{
    struct  __TCP_TRACKINFO	*next;
    int             id;
    unsigned int    uid;
    unsigned int    srcIP;
    unsigned int    destIP;
    unsigned short  srcPort;
    unsigned short  destPort;
    int             status;
    int             urlLen;
    int             notify_Index;
    char            *url;
    char            *referer;
    char            *host;
    void            *q_entry;
    int             buffered;
    char            buffer[PREPACKET_BUFFER_SIZE];
}__attribute((packed))  tcp_TrackInfo;

typedef struct  __TCP_TRACKINFO_MANAGER
{
    spinlock_t      tcp_info_lock;
    tcp_TrackInfo   *head;
    tcp_TrackInfo   *tail;
    int             count;
}__attribute((packed)) tcp_Info_Manager;

void            clean_tcp_TrackInfos(tcp_Info_Manager *manager);
int             add_tcp_TrackInfo( tcp_Info_Manager *manager,  tcp_TrackInfo *node);
tcp_TrackInfo * add_tcp_TrackInfo_ToHead(tcp_Info_Manager *manager,  tcp_TrackInfo *node);
tcp_TrackInfo * find_tcp_TrackInfo(tcp_Info_Manager *manager, struct sk_buff *skb, int option);
tcp_TrackInfo * find_tcp_TrackInfo_withID(tcp_Info_Manager *manager, int id);
void            free_tcp_TrackInfo(tcp_TrackInfo *node);
tcp_TrackInfo * getFirst_tcp_TrackInfo(tcp_Info_Manager *manager);
tcp_TrackInfo * make_tcp_TrackInfo ( struct sk_buff *skb);
tcp_Info_Manager * create_tcp_Info_Manager(void);
void            change_tcp_Data(struct sk_buff *skb);
tcp_TrackInfo * find_tcp_TrackInfo_Reverse(tcp_Info_Manager *manager, struct sk_buff *skb);
#endif
