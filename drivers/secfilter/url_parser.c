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
#include    "url_parser.h"

#define SKIPSPACE(x)//    {while(*(x) ==' '){(x)=(x)+1;}}

#define METHOD_MAX  3
char    *http_method[METHOD_MAX] = {"GET ", "POST ", "CONNECT "};

int makeNotifyInfo(tcp_TrackInfo *node)
{
    URL_Info    url_info;
    char        *newURL = NULL;
    int         hostLen = 0;
    int         urlLen  = 0;
    int         result  = 0;
    int         referLen    = 0;
    if (node != NULL)
    {
        if (node->host != NULL)
        {
            hostLen = strlen(node->host);
        }
        if (node->url != NULL)
        {
            urlLen  = strlen(node->url);
        }
        if (node->referer != NULL)
        {
            referLen    = strlen(node->referer);
        }
        url_info.id         = node->id;
        url_info.uid        = node->uid;
        url_info.version    = FILTER_VERSION;
        url_info.len        = hostLen+urlLen;       // HOST + URL   not include '\0'
        if (referLen == 0)
        {
        url_info.type       = URL_TYPE;
        node->urlLen        = sizeof(url_info)+url_info.len;
        }
        else
        {
            url_info.type   = URL_REFER_TYPE;
            node->urlLen    = sizeof(url_info)+url_info.len + sizeof(int)+ referLen ;
        }

        newURL  = SEC_MALLOC(node->urlLen+1);
        if (newURL != NULL)
        {
            char    *buffer = newURL;

            memcpy(buffer, &url_info, sizeof(url_info));
            buffer = &buffer[sizeof(url_info)];
            if (hostLen >0)
            {
                memcpy((void *)buffer, (void *)node->host, hostLen);
                SEC_FREE(node->host);
                buffer = &buffer[hostLen];
            }
            if (urlLen >0)
            {
                memcpy((void *)buffer, (void *)node->url, urlLen);
                SEC_FREE(node->url);
                buffer = &buffer[urlLen];
            }
            if (referLen >0)
            {
                *((int *)buffer) = referLen;
                buffer = &buffer[sizeof(int)];
                memcpy((void *)buffer, (void *)node->referer, referLen);
                SEC_FREE(node->referer);
            }
            result              = 1;
            node->notify_Index  = 0;
            node->url           = newURL;
        }
    }
    return result;
}

char *  appendString(char *preData, char *postData)
{
    int     preDataSize     = 0;
    int     postDataSize    = 0;
    int     totalStringSize = 0;
    char    *result         = NULL;

    if ( (preData != NULL) && (postData !=NULL))
    {
        preDataSize     = strlen(preData);
        postDataSize    = strlen(postData);
        if (postDataSize > 0)
        {
            totalStringSize = preDataSize+postDataSize+1;   // Include '\0'
            result  = SEC_MALLOC(totalStringSize);
            if (result != NULL)
            {
                memcpy(result, preData, preDataSize);
                memcpy(&result[preDataSize], postData, postDataSize);
            }
        }
    }
    return result;
}


// This function extract packet data from skb and check it is URL packet
tcp_TrackInfo*  isURL( struct sk_buff *skb)
{
    char            *request    = NULL;
    int             i           = 0;
    tcp_TrackInfo   *result     = NULL;
    struct	tcphdr  *tcph       = (struct tcphdr *)skb_transport_header(skb);                           // Get TCP header

    if (skb->data_len == 0)                                                                             // If packet is not fragmented
    {
        request = (( char *)tcph + tcph->doff*4);                                                       // Find TCP data position
    }
    else                                                                                                // If packet is fragmented
    {
        struct skb_shared_info *shinfo = skb_shinfo(skb);                                               // Get info about fragmented data
        if (shinfo != NULL)
        {
#ifdef _SKB_FRAG_STRUCT_CHANGED
            void *frag_addr = page_address(shinfo->frags[0].page) + shinfo->frags[0].page_offset;     // Get just first fragment
#else
            void *frag_addr = page_address(shinfo->frags[0].page.p) + shinfo->frags[0].page_offset;
#endif
            if (frag_addr != NULL)
            {
                request = (char *)frag_addr;                                                            // Get data of the fragment
            }
        }
    }
    if (request != NULL)                                                                                // If there is data
    {
        for (i = 0; i<METHOD_MAX; i++)                                                                  // Check it is started with HTTP requests
        {
            int	methodLen   = strlen(http_method[i]);
            if (strnicmp(request, http_method[i], methodLen) == 0)                                      // If it is started with HTTP request
            {
                result = make_tcp_TrackInfo(skb);                                                       // Make TCP track info based on skb
                break;
            }
        }
    }
    return result;
}

// This function retrieves packet data

char * getPacketData( struct sk_buff *skb, tcp_TrackInfo *node)
{
    struct  iphdr   *iph    = (struct iphdr*) ip_hdr(skb);
    struct  tcphdr  *tcph   = (struct tcphdr *)skb_transport_header(skb);
    char            *result = NULL;
    int             length  = skb->len - (iph->ihl*4) - (tcph->doff*4);                             // Get TCP data length

    result  = SEC_MALLOC(length+node->buffered+1);                                                  // Allocate buffers
    if (result != NULL)
    {
        if (node->buffered > 0)                                                                     // If buffered data is exist
        {
            memcpy((void *)result, (void *)node->buffer, node->buffered);                           // Copy buffered data into new buffer
        }
        if (length >0)                                                                                              // If there is data to copy
        {
            if (skb->data_len == 0)                                                                                 // Check packet data is not fragmented
            {
                memcpy((void *)&result[node->buffered], (void *)((unsigned char *)tcph + tcph->doff*4) , length);   // Copy packet data into the buffer which already has copied buffered data
            }
            else                                                                                                    // If packet is fragmented
            {
                int                     i       = 0;
                int                     index   = node->buffered;
                struct skb_shared_info  *shinfo = skb_shinfo(skb);                                                  // Get fragmented info.
                for ( i = 0 ; i < shinfo->nr_frags ; i++)                                                           // Get every fragment
                {
#ifdef _SKB_FRAG_STRUCT_CHANGED
                    void *frag_addr = page_address(shinfo->frags[i].page) + shinfo->frags[i].page_offset;         // Get data from fragment
#else
                    void *frag_addr = page_address(shinfo->frags[i].page.p) + shinfo->frags[i].page_offset; 
#endif
                    if (frag_addr != NULL)
                    {
                        memcpy((void *)&result[index], (void *)frag_addr, shinfo->frags[i].size);                   // Copy each fragment data into buffer
                        index += shinfo->frags[i].size;                                                             // Move copy position
                    }
                }
            }
        }
        result[length+node->buffered] = '\0';
        node->buffered = 0;                                                                                         // Reset buffered data in track info
    }
    return result;
}


int saveToBuffer(tcp_TrackInfo *node, char *data, int size)
{
    int bufferSize  = 0;
    int result      = 0;
    if (size < PREPACKET_BUFFER_SIZE)
    {
        bufferSize  = size;
    }
    else
    {
        bufferSize  = PREPACKET_BUFFER_SIZE;
    }
    result  = size-bufferSize;
    memcpy((void *)node->buffer, (void *)&data[result] , bufferSize);
    data[result] = 0;
    node->buffered = bufferSize;
    return result;
}

int findStringByTag(tcp_TrackInfo *node, char **data, char *dataStart,  char *tagInfo)
{
    char    *dataEnd    = NULL;
    int     len         = 0;
    int     result      = -1;

    if ((dataStart != NULL) && (tagInfo != NULL))
    {
        dataEnd     = strstr(dataStart, tagInfo);
        if (dataEnd != NULL)    // Found End String
        {
            *dataEnd    = 0;
            len         = strlen(dataStart);
            result      = len;      // result has length only when tag's been found
        }
        else    // Next Packet is needed
        {
            int dataLen = strlen(dataStart);
            len = saveToBuffer(node, dataStart, dataLen);   // Last buffer size after back-up
        }

        if (*data != NULL)  //Place to Save has previous string
        {
            char *newData   = appendString(*data, dataStart);
            if (newData != NULL)    //Get appended string
            {
                SEC_FREE(*data);
                *data = newData;    // Replace with New URL
            }
        }
        else if (len >0)    // This is FIrst block of data
        {
            char *newData   = NULL;
            newData = SEC_MALLOC(len+1);
            if (newData != NULL)
            {
                memcpy((void *)newData, (void *)dataStart, len);
                newData[len] =0;
                *data = newData;
            }
        }
    }
    return result;
}


// This function gets URL from packet data and track info

void    getURL(char *request, tcp_TrackInfo *node)  // request is already checked
{
    int     len         = 0;
    int     length      = 0;
    int     i           = 0;
    int     flagLast    = 0;
    char    *dataStart  = request;

    length          = strlen(dataStart);                                                // Get total length of data
    if ((length >= 4) && (strcmp(&dataStart[length-4], "\r\n\r\n") == 0))               // Check this packet data has \r\n\r\n. It means this packet has end of HTTP request
    {
        flagLast = 1;                                                                   // Set this packet is last packet
    }

    if (node->status == NOTSET)                                                         // This is first packet
    {
        for (i = 0 ; i<METHOD_MAX ; i++)                                                // Check HTTP method
        {
            int	methodLen   = strlen(http_method[i]);
            if (strnicmp(request, http_method[i], methodLen) == 0)                      // Request line should be started with method name
            {
                node->status    = GETTING_URL;                                              // Change status as GETTING_URL
                dataStart       = &request[methodLen];                                      // Move data position
                SKIPSPACE(dataStart);                                                       // Skip white spaces
                break;
            }
        }
    }

    if (node->status == GETTING_URL)                                                        // If it is GETTING_URL status
    {
        len = findStringByTag(node, &(node->url), dataStart, " HTTP/");                     // Find String ended with " HTTP/" and copy it into url
        if (len >= 0)
        {
            node->status = GOT_URL;                                                         // We got URL now
            dataStart= &dataStart[len+1];                                                   // Move data position
        }
    }

    if (node->status == GOT_URL)                                                                        // If we got URL
    {
        char    *referer        = NULL;
        char    *hostStart      = NULL;
        if ((filterMode == FILTER_MODE_ON_BLOCK_REFER) ||(filterMode == FILTER_MODE_ON_RESPONSE_REFER)) // Check Refer is needed
        {
            if (node->referer == NULL)                                                                  // If we do not find "Referer: "
            {
                referer = strstr(dataStart, "\r\nReferer: ");                                           // Find Referer:
                if (referer != NULL)
                {
                    referer = &referer[11];                                                             //if found, set referer position(11 is length of "\r\nReferer: "
                }
            }
        }
        if ((node->host == NULL) && (node->url[0] == '/'))                                              // If extract url need host name
        {
            hostStart = strstr(dataStart, "\r\nHost: ");                                                // Find "\r\nHost: "
            if (hostStart != NULL)
            {
                hostStart = &hostStart[8];                                                            // If found, set hostStart position(8 is length of "\r\nHost: "
            }
        }

        if ((referer != NULL) && (hostStart != NULL))                                                   // If this packet has referer and host at the same time
        {
            if (referer<hostStart)                                                                      // referer is located before host
            {
                SKIPSPACE(referer);
                findStringByTag(node, &(node->referer), referer, "\r\n");                               // Find end string and it should be there
                SKIPSPACE(hostStart);
                if (findStringByTag(node, &(node->host), hostStart, "\r\n") >=0)                        // If this packet has referer and host completely
                {
                    node->status = GOT_FULL_URL;                                                        // Change status to GOT_FULL_URL
                }
                else                                                                                    // If host is not finished
                {
                    node->status = GETTING_HOST;                                                        // Change status to GETTING_HOST
                }
            }
            else                                                                                        // Host is located before referer
            {
                SKIPSPACE(hostStart);
                findStringByTag(node, &(node->host), hostStart, "\r\n");                                // Find end string and it should be there
                SKIPSPACE(referer);
                if (findStringByTag(node, &(node->referer), referer, "\r\n")>=0)                        // If this packet has host and referer completely
                {
                    node->status = GOT_FULL_URL;                                                        // Change Status to GOT_RULL_URL
                }
                else                                                                                    // If referer is not finished
                {
                    node->status = GETTING_REFERER;                                                     // Change status to GETTING_REFERRER
                }
            }
        }
        else if (referer != NULL)                                                                      // If there is only referer
        {
            SKIPSPACE(referer);
            if (findStringByTag(node, &(node->referer), referer, "\r\n") >=0)                           // Find end string
            {
                if (flagLast == 1)                                                                      // If this is last packet of request
                {
                    node->status = GOT_FULL_URL;                                                        // Change status to GOT_FULL_URL
                }
                else                                                                                    // If it is not last packet it means, there can be host so set it to GOT_URL
                {
                    node->status = GOT_URL;
                }
            }
            else                                                                                        // If referer is not finished
            {
                node->status = GETTING_REFERER;                                                         // Change status to GETTING_REFERRER
            }
        }
        else if (hostStart != NULL)                                                                     // If there is only host
        {
            SKIPSPACE(hostStart);
            if (findStringByTag(node, &(node->host), hostStart, "\r\n")>=0)                             // Find end string
            {
                if (flagLast == 1)                                                                      // If it is last packet
                {
                    node->status = GOT_FULL_URL;                                                        // Change status to GOT_FULL_URL
                }
                else                                                                                    // If there is not last packet there can be referer so set it to GOT_URL
                {
                    node->status = GOT_URL;
                }
            }
            else                                                                                        // If host is not finished
            {
                node->status = GETTING_HOST;                                                            // Set it to GETTING_HOST
            }
        }
        else                                                                                            // There is not referer and host in this packet
        {
            if (flagLast == 1)  // This is last packet                                                  // If this is last packet
            {
                node->status    = GOT_FULL_URL;                                                         // Just use URL, so set status to GOT_FULL_URL
            }
            else                                                                                        // Content is not ended
            {
                saveToBuffer(node, dataStart, length);                                                  // save some end part into buffer
            }
        }
    }
    else if (node->status == GETTING_HOST)                                                              // If the status is GETTING_HOST
    {
        if (findStringByTag(node, &(node->host), dataStart, "\r\n")>=0)                                 // Find end string
        {
            if (flagLast == 1)                                                                          // If this is last packet
            {
                node->status = GOT_FULL_URL;                                                            // Set status to GOT_FULL_URL
            }
            else                                                                                        // If this is not last packet, there can be referer. So set status to GOT_URL
            {
                node->status = GOT_URL;
            }
        }
    }
    else if (node->status == GETTING_REFERER)                                                           // If the status is GETTING_REFFERER
    {
        if (findStringByTag(node, &(node->referer), dataStart, "\r\n")>=0)                              // Find end string
        {
            if (flagLast == 1)                                                                          // If this is last packet
            {
                node->status = GOT_FULL_URL;                                                            // Set status to GOT_FULL_URL
            }
            else                                                                                        // If this is not last packet, there can be host. So set status to GOT_URL
            {
                node->status = GOT_URL;
            }
        }
    }
}
