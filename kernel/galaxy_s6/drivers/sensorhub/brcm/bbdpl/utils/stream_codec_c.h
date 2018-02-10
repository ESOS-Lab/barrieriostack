/*******************************************************************************
 ** \file stream_codec_c.h  StreamCodec struct declaration
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

#ifndef STREAM_CODEC_C_H  /* { */
#define STREAM_CODEC_C_H

#include "bbd_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sStreamCodec
{
    unsigned int m_uiOffset;
    unsigned char* m_pBuf;
    unsigned int m_uiMaxOffset;
    bool m_bBigEndian;
    unsigned int m_uiBitOffset;
    bool m_bFail;
};

#define sStreamEncoder sStreamCodec
#define sStreamDecoder sStreamCodec

void StreamCodec_StreamCodec(struct sStreamCodec* p, unsigned char* buf,
                              unsigned int bufsize);
/* get size of buffer used */
unsigned int StreamCodec_GetOffset(struct sStreamCodec *p);

/* get available size in buffer */
unsigned int StreamCodec_GetAvailableSize(struct sStreamCodec *p);

/* get pointer to the buffer of the stream */
const unsigned char* StreamCodec_GetStreamBuffer(struct sStreamCodec *p); 

/* clears the buffer. Ready to be used again on same memory buffer */
void StreamCodec_Reset(struct sStreamCodec *p);

// Fail returns true if an error occured. 
bool StreamCodec_Fail(const struct sStreamCodec * const p);

void StreamEncoder_StreamEncoder(struct sStreamCodec *p,unsigned char* buf, unsigned int bufsize);

void StreamEncoder_PutU08(struct sStreamCodec *p,unsigned char);
void StreamEncoder_PutS08(struct sStreamCodec *p,signed char);
void StreamEncoder_PutU16(struct sStreamCodec *p,unsigned short);
void StreamEncoder_PutS16(struct sStreamCodec *p,signed short);
void StreamEncoder_PutU32(struct sStreamCodec *p,unsigned long);
void StreamEncoder_PutS32(struct sStreamCodec *p,signed long);
void StreamEncoder_PutBuffer(struct sStreamCodec *p,const unsigned char*, unsigned long);

void StreamDecoder_StreamDecoder(struct sStreamCodec *p,
                        unsigned char* buf, unsigned int bufsize);
  unsigned char StreamDecoder_GetU08(struct sStreamCodec *p);
    signed char StreamDecoder_GetS08(struct sStreamCodec *p);
 unsigned short StreamDecoder_GetU16(struct sStreamCodec *p);
   signed short StreamDecoder_GetS16(struct sStreamCodec *p);
  unsigned long StreamDecoder_GetU32(struct sStreamCodec *p);
    signed long StreamDecoder_GetS32(struct sStreamCodec *p);
 unsigned char* StreamDecoder_GetBuffer(struct sStreamCodec *p, unsigned long);

#ifdef __cplusplus
}
#endif

#endif /* } STREAM_CODEC_C_H */
