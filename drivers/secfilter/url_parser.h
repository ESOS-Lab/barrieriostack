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

#ifndef URL_PARSER_H_
#define URL_PARSER_H_
#include    "sec_filter.h"
#include    "tcp_track.h"

typedef struct  __URLINFO
{
    short   version;
    short   type;
    int     id;
    unsigned int    uid;
    int     len;
}__attribute((packed)) URL_Info;

tcp_TrackInfo*  isURL(struct sk_buff *skb);
char *  getPacketData( struct sk_buff *skb, tcp_TrackInfo *node);
void    getURL(char *request, tcp_TrackInfo *node);
int     makeNotifyInfo(tcp_TrackInfo *node);
#endif /* URL_PARSER_H_ */
