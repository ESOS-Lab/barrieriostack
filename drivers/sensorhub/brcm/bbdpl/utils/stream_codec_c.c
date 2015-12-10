/******************************************************************************
 ** \file stream_codec_c.c  StreamCodec class definition
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

#include "bbd_utils.h"
#include "stream_codec_c.h"

#define FAIL_CHECK(_test)       {if(!(_test)){p->m_bFail = true; return;  }}
#define FAIL_CHECK_RET0(_test)  {if(!(_test)){p->m_bFail = true; return 0;}}

void StreamCodec_StreamCodec(struct sStreamCodec* p, unsigned char* buf,
                              unsigned int bufsize)
{
    p->m_uiOffset    = 0;
    p->m_pBuf        = buf;
    p->m_uiMaxOffset = bufsize;
    p->m_uiBitOffset = 0;
    p->m_bFail       = false;
}

unsigned int StreamCodec_GetOffset(struct sStreamCodec* p)
{
    // When using bit packing, no other methods should be
    // called until it's aligned to a byte
    FAIL_CHECK_RET0(p->m_uiBitOffset == 0);

    return p->m_uiOffset;
}

unsigned int StreamCodec_GetAvailableSize(struct sStreamCodec* p)
{
    FAIL_CHECK_RET0(p->m_uiBitOffset == 0);
    return (p->m_uiMaxOffset-p->m_uiOffset);
}

const unsigned char* StreamCodec_GetStreamBuffer(struct sStreamCodec* p)
{
    return p->m_pBuf;
}

void StreamCodec_Reset(struct sStreamCodec* p)
{
    p->m_uiOffset = 0;
    p->m_uiBitOffset = 0;
    p->m_bFail = false;
}

bool StreamCodec_Fail(const struct sStreamCodec* const p)
{
    return p->m_bFail;
}

#if 0
void StreamCodec_PutU08(struct sStreamCodec* p, unsigned char u8)
{
    p->m_bFail = true;
}

void StreamCodec_PutS08(struct sStreamCodec* p, signed char u8)
{
    p->m_bFail = true;
}

void StreamCodec_PutU16(struct sStreamCodec* p, unsigned short u16)
{
    p->m_bFail = true;
}

void StreamCodec_PutS16 (struct sStreamCodec* p, signed short s16)
{
    p->m_bFail = true; 
}

void StreamCodec_PutU32(struct sStreamCodec* p, unsigned long u32)
{
    p->m_bFail = true;
}

void StreamCodec_PutS32(struct sStreamCodec* p, signed long s32)
{
    p->m_bFail = true;
}

void StreamCodec_PutBuffer(struct sStreamCodec* p, const unsigned char* buf, unsigned long size)
{
    p->m_bFail = true;
}

unsigned char StreamCodec_GetU08 (struct sStreamCodec* p)
{
    p->m_bFail = true;
    return 0;
}

signed char StreamCodec_GetS08 (struct sStreamCodec* p)
{
    p->m_bFail = true;
    return 0;
}

unsigned short StreamCodec_GetU16(struct sStreamCodec* p)
{
    p->m_bFail = true;
    return 0;
}

signed short StreamCodec_GetS16(struct sStreamCodec* p)
{
    p->m_bFail = true;
    return 0;
}

unsigned long StreamCodec_GetU32(struct sStreamCodec* p)
{
    p->m_bFail = true;
    return 0;
}

signed long StreamCodec_GetS32(struct sStreamCodec* p)
{
    p->m_bFail = true;
    return 0;
}

unsigned char* StreamCodec_GetBuffer(struct sStreamCodec* p,
                                unsigned long size)
{
    p->m_bFail = true;
    return p->m_pBuf;
}
#endif

void StreamDecoder_StreamDecoder(struct sStreamCodec* p,
                unsigned char* buf, unsigned int bufsize)
{
    StreamCodec_StreamCodec((struct sStreamCodec *) p, buf, bufsize);
}

unsigned char StreamDecoder_GetU08(struct sStreamCodec* p)
{
    FAIL_CHECK_RET0(p->m_uiBitOffset == 0);
    FAIL_CHECK_RET0(p->m_uiOffset+1<=p->m_uiMaxOffset);

    return p->m_pBuf[p->m_uiOffset++];
}

signed char StreamDecoder_GetS08(struct sStreamCodec* p)
{
    return (signed char)StreamDecoder_GetU08(p);
}

unsigned short StreamDecoder_GetU16(struct sStreamCodec* p)
{
    FAIL_CHECK_RET0(p->m_uiBitOffset == 0);
    FAIL_CHECK_RET0(p->m_uiOffset+2<=p->m_uiMaxOffset);
    {
    unsigned short usValue  = p->m_pBuf[p->m_uiOffset++];
                   usValue |= p->m_pBuf[p->m_uiOffset++]<<8;
    return usValue;
    }
}

signed short StreamDecoder_GetS16(struct sStreamCodec* p)
{
    return (signed short)StreamDecoder_GetU16(p);
}

unsigned long StreamDecoder_GetU32(struct sStreamCodec* p)
{
    FAIL_CHECK_RET0(p->m_uiBitOffset == 0);
    FAIL_CHECK_RET0(p->m_uiOffset+4<=p->m_uiMaxOffset);

    {
        unsigned long ulValue;
        ulValue  = p->m_pBuf[p->m_uiOffset++];
        ulValue |= p->m_pBuf[p->m_uiOffset++]<<8;
        ulValue |= p->m_pBuf[p->m_uiOffset++]<<16;
        ulValue |= p->m_pBuf[p->m_uiOffset++]<<24;

        return ulValue;
    }
}


signed long StreamDecoder_GetS32(struct sStreamCodec* p)
{
    return (signed long)StreamDecoder_GetU32(p);
}

unsigned char *StreamDecoder_GetBuffer (struct sStreamCodec* p, unsigned long size)
{
    unsigned char* buf = &p->m_pBuf[p->m_uiOffset];
    if (p->m_uiBitOffset != 0 || p->m_uiOffset+size>p->m_uiMaxOffset)
    {
        p->m_bFail = true;
    }
    else
    {
        p->m_uiOffset+=size;
    }
    return buf;
}



void StreamEncoder_StreamEncoder(struct sStreamCodec* p,
                        unsigned char* buf, unsigned int bufsize)
{
    StreamCodec_StreamCodec(p, buf, bufsize);
}

void StreamEncoder_PutU08(struct sStreamCodec* p, unsigned char u8)
{
    FAIL_CHECK(p->m_uiBitOffset == 0);
    FAIL_CHECK(p->m_uiOffset+1<=p->m_uiMaxOffset);

    p->m_pBuf[p->m_uiOffset++] = u8;
}

void StreamEncoder_PutS08 (struct sStreamCodec* p, signed char s8)
{
    StreamEncoder_PutU08(p, (unsigned char)s8);
}

void StreamEncoder_PutU16 (struct sStreamCodec* p, unsigned short u16)
{
    FAIL_CHECK(p->m_uiBitOffset == 0);
    FAIL_CHECK(p->m_uiOffset+2<=p->m_uiMaxOffset);

    {
        /* Serialize unsigned short value */
    p->m_pBuf[p->m_uiOffset++] = (u16)&0xFF;
    p->m_pBuf[p->m_uiOffset++] = (u16>>8)&0xFF;
    }
}

void StreamEncoder_PutS16 (struct sStreamCodec* p, signed short s16)
{
    StreamEncoder_PutU16(p, (unsigned short)s16);
}

void StreamEncoder_PutU32 (struct sStreamCodec* p, unsigned long u32)
{
    FAIL_CHECK(p->m_uiBitOffset == 0);
    FAIL_CHECK(p->m_uiOffset+4<=p->m_uiMaxOffset);

    /* Serialize unsigned long value */
    {
    p->m_pBuf[p->m_uiOffset++] = (unsigned char)((u32)&0xFF);
    p->m_pBuf[p->m_uiOffset++] = (unsigned char)((u32>>8)&0xFF);
    p->m_pBuf[p->m_uiOffset++] = (unsigned char)((u32>>16)&0xFF);
    p->m_pBuf[p->m_uiOffset++] = (unsigned char)((u32>>24)&0xFF);
    }
}

void StreamEncoder_PutS32 (struct sStreamCodec* p, signed long s32)
{
    StreamEncoder_PutU32(p, (unsigned long)s32);
}

void StreamEncoder_PutBuffer(struct sStreamCodec* p, const unsigned char* buf, unsigned long size)
{
    FAIL_CHECK(p->m_uiBitOffset == 0);
    FAIL_CHECK(p->m_uiOffset+size<=p->m_uiMaxOffset);

    {
        memcpy(&p->m_pBuf[p->m_uiOffset], buf, size);
    p->m_uiOffset += size;
    }
}

