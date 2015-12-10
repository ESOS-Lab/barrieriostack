/******************************************************************************
 ** \file ring_buffer_c.c  RingBuffer struct definition
 *
 * Copyright 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *  
 * A copy of the GPL is available at 
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The BBD (Broadcom Bridge Driver)
 *
 * tabstop = 8
 */

#include <linux/string.h>
#include "ring_buffer_c.h"
#include "../bbd_internal.h"

void RingBuffer_RingBuffer(struct sRingBuffer* p,
                        unsigned long ulSizeRingBuffer)
{
    p->m_pBase = NULL;
    p->m_pEnd = NULL;
    p->m_pTail = NULL;
    p->m_pHead = NULL;
    p->m_ulSize = ulSizeRingBuffer;
    p->m_ulUsed = 0;
    p->m_bFull = false;
    p->m_pBase = (unsigned char *) bbd_calloc(ulSizeRingBuffer);
    if (p->m_pBase != NULL)
    {
        p->m_pTail = p->m_pBase;
        p->m_pHead = p->m_pBase;
        p->m_pEnd  = p->m_pBase + p->m_ulSize;
    }
}

void RingBuffer_dtor(struct sRingBuffer* p) 
{
    if (p->m_pBase)
        bbd_free(p->m_pBase);
    p->m_pBase = 0;
}

void RingBuffer_SetData(struct sRingBuffer* p, unsigned char raucData[], unsigned long ulSize)
{
    if (p->m_bFull || ulSize == 0)
    {
        return;
    }

    /* Copy what we can until the end of the buffer, and handle rollovers */
    if (p->m_pHead >= p->m_pTail)
    {
        unsigned long ulSizeTillEnd = (unsigned long )(p->m_pEnd-p->m_pHead);
        if (ulSizeTillEnd > ulSize)
        {
            ulSizeTillEnd = ulSize;
        }
        memcpy(p->m_pHead, raucData, ulSizeTillEnd);
        p->m_ulUsed += ulSizeTillEnd;
        p->m_pHead += ulSizeTillEnd;
        ulSize -= ulSizeTillEnd;
        raucData += ulSizeTillEnd;
        if (p->m_pHead == p->m_pEnd)
        {
            p->m_pHead = p->m_pBase;
            if (p->m_pTail == p->m_pBase)
            {
                p->m_bFull = true;
            }
        }
        if (ulSize == 0 || p->m_bFull)
        {
            return;
        }
    }

    /* at that point, the head is before the tail */
    {
    unsigned long ulSizeAvailable = RingBuffer_GetAvailableSize(p);
    if (ulSizeAvailable > ulSize)
    {
        ulSizeAvailable = ulSize;
    }

    memcpy(p->m_pHead, raucData, ulSizeAvailable);
    p->m_ulUsed += ulSizeAvailable;
    p->m_pHead  += ulSizeAvailable;
    ulSize -= ulSizeAvailable;

    if (p->m_pHead == p->m_pTail)
    {
        p->m_bFull = true;
    }
    }
}

unsigned long RingBuffer_GetAvailableSize(const struct sRingBuffer* const p) 
{
    return p->m_ulSize-p->m_ulUsed;
}


unsigned long RingBuffer_GetDataSize(const struct sRingBuffer* const p)
{
    return p->m_ulUsed;
}


unsigned long RingBuffer_GetData(struct sRingBuffer* p, unsigned char raucData[], unsigned long ulMaxSize)
{
    unsigned long ulAvail = RingBuffer_GetDataSize(p);
    if (ulMaxSize == 0 || ulAvail == 0)
    {
        return 0;
    }

    {
    unsigned long ulSizeTillEnd = (unsigned long)(p->m_pEnd-p->m_pTail);


    if (ulSizeTillEnd > ulMaxSize)
    {
        ulSizeTillEnd = ulMaxSize;
    }
    if (ulSizeTillEnd > ulAvail)
    {
        ulSizeTillEnd = ulAvail;
    }

    memcpy(raucData, p->m_pTail, ulSizeTillEnd);
    p->m_ulUsed -= ulSizeTillEnd;
    ulMaxSize -= ulSizeTillEnd;
    p->m_pTail += ulSizeTillEnd;
    raucData += ulSizeTillEnd;
    if (p->m_pTail == p->m_pEnd)
    {
        p->m_pTail = p->m_pBase;
    }
    p->m_bFull = false;
    /* we have at least copied one B, so it is not full anymore */

    /* recursive call on the rest of the array */
    return ulSizeTillEnd + RingBuffer_GetData(p, raucData, ulMaxSize);
    }
}

