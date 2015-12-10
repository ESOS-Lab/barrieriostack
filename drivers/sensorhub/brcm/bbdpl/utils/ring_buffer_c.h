/*******************************************************************************
 ** \file ring_buffer.h  RingBuffer class declaration
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

#ifndef RING_BUFFER__C_H
#define RING_BUFFER__C_H

#include "bbd_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sRingBuffer
{
    unsigned char *m_pBase;
    unsigned char *m_pEnd;
    unsigned char *m_pTail;
    unsigned char *m_pHead;
    unsigned long m_ulSize;   
    unsigned long m_ulUsed;
    bool m_bFull;
};

/* Dynamic memroy will be allocated */
void RingBuffer_RingBuffer(struct sRingBuffer *p, unsigned long ulSizeRingBuffer);
void RingBuffer_dtor(struct sRingBuffer *p);

/* push data to the ring buffer. Bytes will be dropped if space is not available */
void RingBuffer_SetData(struct sRingBuffer *p,
        unsigned char raucData[], unsigned long ulSize);

/* request the available size */
unsigned long RingBuffer_GetAvailableSize(const struct sRingBuffer *const p);

/* request the size of the data available */
unsigned long RingBuffer_GetDataSize(const struct sRingBuffer *const p);

#if 0
/* InspectData will return data in the ring buffer, but will not "consume it" */
unsigned long RingBuffer_InspectData(struct sRingBuffer *p,
                const unsigned char raucData[], unsigned long ulMaxSize);
#endif

/* get data from the ring buffer. Returns the number of
 * bytes copied to raucData
 */

unsigned long RingBuffer_GetData(struct sRingBuffer *p,
                unsigned char raucData[], unsigned long ulMaxSize);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
