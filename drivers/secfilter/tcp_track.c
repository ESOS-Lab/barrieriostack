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

#include    "sec_filter.h"
#include    "tcp_track.h"

static  int     notifyID    = 1;

tcp_TrackInfo   * make_tcp_TrackInfo ( struct sk_buff *skb)
{
    tcp_TrackInfo   *result = NULL;
    struct  iphdr   *iph    = (struct iphdr*) ip_hdr(skb);
    struct  tcphdr  *tcph   = (struct tcphdr *)skb_transport_header(skb);

    result  = SEC_MALLOC(sizeof(tcp_TrackInfo));
    if (result != NULL)
    {   struct sock *pSk = skb->sk;
        result->destIP      = iph->daddr;
        result->srcIP       = iph->saddr;
        result->destPort    = tcph->dest;
        result->srcPort     = tcph->source;
        result->status      = NOTSET;
        result->id          = notifyID++;
        result->uid         = 0;

        if (pSk != NULL)
        {
            struct socket   *pSk_socket  = pSk->sk_socket;
            if (pSk_socket != NULL)
            {
                struct file *pFile  = pSk_socket->file;
                if (pFile != NULL)
                {
                    const struct cred *pCred = pFile->f_cred;
                    if (pCred != NULL)
                    {
                        result->uid = (unsigned int)pCred->fsuid;
                    }
                }
            }
        }
        result->buffered    = 0;
    }
    return result;
}

#define SHORT_MSG_SIZE      131
#define SHORT_MSG           "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 60\r\n\r\n<html><head></head><body>This Site is Blocked!</body></html>"

void    change_tcp_Data(struct sk_buff *skb)
{
    struct iphdr    *iph        = (struct iphdr*)ip_hdr(skb);
    struct tcphdr   *tcph       = (struct tcphdr *)((char *)iph+(iph->ihl*4));
    char            *response   = SHORT_MSG;
    int             copySize    = SHORT_MSG_SIZE;

    if (skb->data_len == 0)
    {
        int dataSize = skb->len - (iph->ihl*4) - (tcph->doff*4);
        unsigned char   *tcpData    = ((unsigned char *)tcph + tcph->doff*4);

        if (dataSize > 0)
        {
            memset((void *)tcpData , ' ', dataSize);
            if (errorMsg != NULL)
            {
                if (dataSize > errMsgSize)
                {
                    response    = errorMsg;
                    copySize    = errMsgSize;
                }
            }

            if (copySize > dataSize)
            {
                copySize = dataSize;
            }
            memcpy(tcpData, response, copySize);
        }
    }
    else
    {
        int     i = 0;
        struct  skb_shared_info *shinfo = skb_shinfo(skb);
        for (i = 0 ; i < shinfo->nr_frags ; i++)
        {
            int     written = 0;
#ifdef _SKB_FRAG_STRUCT_CHANGED
            void    *frag_addr  = page_address(shinfo->frags[i].page) + shinfo->frags[i].page_offset;
#else
            void    *frag_addr  = page_address(shinfo->frags[i].page.p) + shinfo->frags[i].page_offset;
#endif
            if (frag_addr != NULL)
            {
                int dataSize    = shinfo->frags[i].size;
                if (dataSize >= (copySize-written))
                {
                    memset((void *)frag_addr, ' ', dataSize);
                    dataSize = (copySize-written);
                }
                memcpy((void *)frag_addr, &response[written], dataSize);
                written += dataSize;
                if (written >=copySize)
                {
                    break;
                }
            }
        }
    }
}

void	free_tcp_TrackInfo(tcp_TrackInfo *node)
{
    if (node != NULL)
    {
        SEC_FREE(node->host)
        SEC_FREE(node->referer);
        SEC_FREE(node->url);
        kfree(node);
    }
}

tcp_Info_Manager * create_tcp_Info_Manager(void)
{
    tcp_Info_Manager    *result = NULL;

    result = (tcp_Info_Manager *)SEC_MALLOC(sizeof(tcp_Info_Manager));
    if (result != NULL)
    {
        result->tcp_info_lock = __SPIN_LOCK_UNLOCKED(getting_TrackInfo.tcp_info_lock);
    }
    return result;
}

void    clean_tcp_TrackInfos(tcp_Info_Manager *manager)
{
    unsigned long   flags   = 0;
    tcp_TrackInfo   *node   = NULL;

    if (manager != NULL)
    {
        SEC_spin_lock_irqsave(&manager->tcp_info_lock, flags);
        while (manager->head != NULL)
        {
            node = manager->head;
            manager->head = manager->head->next;
            if (node->q_entry != NULL)
            {
                nf_reinject((struct nf_queue_entry *)node->q_entry, NF_DROP);
                node->q_entry = NULL;
            }
            free_tcp_TrackInfo(node);
        }
        manager->tail = NULL;
        manager->count  = 0;
        SEC_spin_unlock_irqrestore(&manager->tcp_info_lock, flags);
    }
}

tcp_TrackInfo * add_tcp_TrackInfo_ToHead(tcp_Info_Manager *manager, tcp_TrackInfo *node)
{
    unsigned long   flags   = 0;
    if ((manager != NULL) &&(node != NULL))
    {
        SEC_spin_lock_irqsave(&manager->tcp_info_lock, flags);
        node->next      = manager->head;
        manager->head   = node;
        manager->count++;
        if (manager->tail == NULL)
        {
            manager->tail = node;
        }
        SEC_spin_unlock_irqrestore(&manager->tcp_info_lock, flags);
    }
    return node;
}

tcp_TrackInfo * getFirst_tcp_TrackInfo(tcp_Info_Manager *manager)
{
    unsigned long   flags   = 0;
    tcp_TrackInfo   *result = NULL;
    if (manager != NULL)
    {
        SEC_spin_lock_irqsave(&manager->tcp_info_lock, flags);
        if (manager->head != NULL)
        {
            result          = manager->head;
            manager->head   = manager->head->next;
            manager->count--;
            if (manager->head == NULL)
            {
                manager->tail= NULL;
            }
            result->next = NULL;
        }
        SEC_spin_unlock_irqrestore(&manager->tcp_info_lock, flags);
    }
    return result;
}

int add_tcp_TrackInfo( tcp_Info_Manager *manager, tcp_TrackInfo *node)
{
    unsigned long   flags   = 0;
    int             result  = 0;
    if ((manager != NULL) && (node != NULL))
    {
        SEC_spin_lock_irqsave(&manager->tcp_info_lock, flags);
        if (manager->tail != NULL)
        {
            manager->tail->next = node;
        }
        else
        {
            manager->head   = node;
        }
        manager->tail = node;
        manager->count++;
        SEC_spin_unlock_irqrestore(&manager->tcp_info_lock, flags);
        result  = 1;
    }
    return result;
}

tcp_TrackInfo * find_tcp_TrackInfo(tcp_Info_Manager *manager, struct sk_buff *skb, int option)
{
    struct  iphdr   *iph        = (struct iphdr*) ip_hdr(skb);
    struct  tcphdr  *tcph       = (struct tcphdr *)skb_transport_header(skb);
    unsigned long   flags       = 0;
    tcp_TrackInfo   *curNode    = NULL;
    tcp_TrackInfo   *preNode    = NULL;

    if (manager	!= NULL)
    {
        SEC_spin_lock_irqsave(&manager->tcp_info_lock, flags);
        curNode = manager->head;
        while (curNode != NULL)
        {
            if ((curNode->srcIP == iph->saddr) &&
                (curNode->destIP == iph->daddr) &&
                (curNode->srcPort == tcph->source) &&
                (curNode->destPort == tcph->dest))
            {
                break;
            }
            preNode = curNode;
            curNode = curNode->next;
        }

        if ((option == 1) && (curNode != NULL))
        {
            if (preNode != NULL)                // Cur Node is Head
            {
                preNode->next = curNode->next;
            }
            if (manager->head == curNode)
            {
                manager->head = curNode->next;
            }
            if (manager->tail == curNode)
            {
                manager->tail = preNode;
            }
            curNode->next = NULL;
            manager->count--;
        }
        SEC_spin_unlock_irqrestore(&manager->tcp_info_lock, flags);
    }
    return curNode;
}

tcp_TrackInfo * find_tcp_TrackInfo_withID(tcp_Info_Manager	*manager, int id)
{
    tcp_TrackInfo   *curNode    = NULL;
    tcp_TrackInfo   *preNode    = NULL;
    unsigned long   flags       = 0;

    if (manager	!= NULL)
    {
        SEC_spin_lock_irqsave(&manager->tcp_info_lock, flags);
        curNode = manager->head;
        while (curNode != NULL)
        {
            if (curNode->id == id)
            {
                break;
            }
            preNode = curNode;
            curNode = curNode->next;
        }
        if (curNode != NULL)
        {
            if (preNode != NULL)			// Cur Node is Head
            {
                preNode->next = curNode->next;
            }
            if (manager->head == curNode)
            {
                manager->head = curNode->next;
            }
            if (manager->tail == curNode)
            {
                manager->tail = preNode;
            }
            curNode->next = NULL;
            manager->count--;
        }
        SEC_spin_unlock_irqrestore(&manager->tcp_info_lock, flags);
    }
    return curNode;
}

tcp_TrackInfo * find_tcp_TrackInfo_Reverse(tcp_Info_Manager *manager, struct sk_buff *skb)
{
    struct iphdr    *iph        = (struct iphdr*) ip_hdr(skb);
    struct tcphdr   *tcph       = (struct tcphdr *)((char *)iph+(iph->ihl*4));
    //struct tcphdr *tcph       = (struct tcphdr *)skb_transport_header(skb);
    unsigned long   flags       = 0;
    tcp_TrackInfo   *curNode    = NULL;
    tcp_TrackInfo   *result     = NULL;

    if (manager != NULL)
    {
        SEC_spin_lock_irqsave(&manager->tcp_info_lock, flags);
        curNode = manager->head;
        while (curNode != NULL)
        {
            if ((curNode->srcIP == iph->daddr) &&
                (curNode->destIP == iph->saddr) &&
                (curNode->srcPort == tcph->dest) &&
                (curNode->destPort == tcph->source)	)
            {
                result = curNode;
                break;
            }
            curNode = curNode->next;
        }
        SEC_spin_unlock_irqrestore(&manager->tcp_info_lock, flags);
    }
    return result;
}
