/******************************************************************************
 ** \file bbd_bridge_c.c  BbdBridge is a "mini-RpcEngine" inside the BBD
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

#include "bbd_bridge_c.h"
#include "bbd_engine.h"
#include "rpc_codec_c.h"
#include "transport_layer_c.h"
#include "bbd_packet_layer_c.h"
#include "../bbd_internal.h"
#include <linux/memory.h>

extern struct bbd_device *gpbbd_dev;

static bool BbdBridge_CheckPacketSanity(struct BbdBridge *p,
                unsigned char *pucData, unsigned short usSize);

static void BbdBridge_OnRpcReceived(struct BbdBridge *p,
                unsigned short usRpcId,
                unsigned char *pRpcPayload, unsigned short usRpcLen,
                unsigned char *pPacket,     unsigned short usPacketLen);

/*------------------------------------------------------------------------------
 *
 *      Constructor()
 *
 *------------------------------------------------------------------------------
 */

struct BbdBridge *BbdBridge_BbdBridge(struct BbdBridge *p,
                                      struct sITransportLayerCb* rCallbacks)
{
    p->callback = rCallbacks;
    // TransportLayerCb_TransportLayerCb(&p->m_otTLCallback, p);
    TransportLayer_TransportLayer(&p->m_otTL, rCallbacks, "BBDtl");
    p->m_pRpcDecoderHead = 0;
    StreamEncoder_StreamEncoder(&p->m_otStreamEncoder,
                    p->m_ucStreamBuffer,
                    sizeof(p->m_ucStreamBuffer));
    p->m_uiTransactionPayloadSize = 0;
    return p;
}

void BbdBridge_dtor(struct BbdBridge *p)
{
    //TransportLayerCb_dtor(&p->m_otTLCallback);
    TransportLayer_dtor(&p->m_otTL);
    /* StreamEncoder_dtor(&p->m_otStreamEncoder); */
}

void BbdBridge_RegisterRpcDecoder(struct BbdBridge *p,
                                  struct sRpcDecoder* pRpc)
{
    p->m_pRpcDecoderHead = pRpc;
}

void BbdBridge_UnregisterRpcDecoder(struct BbdBridge   *p,
                                    struct sRpcDecoder *pRpcDecoder)
{
    p->m_pRpcDecoderHead = 0;
}

/*------------------------------------------------------------------------------
 *
 *      OnPacketReceived()
 *
 *  Convert the output of normal transactions from the ESW PacketLayer
 *  into separate RPCs and send the data to the RpcEngine:
 *      - The only fully decoded RPCs are sensor responses.  These are
 *        sent directly to the SHMD.
 *      - packet data goes to /dev/bbd_packet
 *
 *------------------------------------------------------------------------------
 */

void BbdBridge_OnPacketReceived(struct sTransportLayer* pp,
                        unsigned char *pucData,
                        unsigned short usSize)
{
    struct BbdBridge* p = (struct BbdBridge *) pp;
    if (BbdBridge_CheckPacketSanity(p, pucData, usSize))
    {
        long lSize = (long)usSize;

        while (lSize > 0)
        {
            unsigned char* pucDataOrig =  pucData;
            unsigned short usRpcId     = *pucData++; 
            unsigned short usRpcLen;
            lSize--;

            if (usRpcId&0x80)
            {
                usRpcId &= ~0x80;
                usRpcId <<= 8;
                usRpcId |= *pucData++; lSize--;
            }

            usRpcLen = *pucData++; lSize--;
            if (usRpcLen&0x80)
            {
                usRpcLen &= ~0x80;
                usRpcLen <<= 8;
                usRpcLen |= *pucData++; lSize--;
            }

            BbdBridge_OnRpcReceived(p, usRpcId, pucData, usRpcLen,
                                pucDataOrig, usRpcLen+pucData-pucDataOrig);
            pucData += usRpcLen;
            lSize -= usRpcLen;
        }
    }
    else
    {
        p->callback->OnException(p->callback, __FILE__, __LINE__);
    }
}


bool BbdBridge_CheckPacketSanity(struct BbdBridge* p,
                        unsigned char *pucData, unsigned short usSize)
{
    long lSize = (long) usSize;
    while (lSize > 0)
    {
        unsigned short usRpcId = *pucData++; 
        unsigned short usRpcLen;
        lSize--;

        if (usRpcId&0x80)
        {
            usRpcId &= ~0x80;
            usRpcId <<= 8;
            usRpcId |= *pucData++; lSize--;
        }

        usRpcLen = *pucData++; lSize--;
        if (usRpcLen&0x80)
        {
            usRpcLen &= ~0x80;
            usRpcLen <<= 8;
            usRpcLen |= *pucData++; lSize--;
        }

        pucData += usRpcLen;
        lSize -= usRpcLen;
    }

    return lSize == 0;
}


void BbdBridge_OnRpcReceived(struct BbdBridge *p,
                             unsigned short    usRpcId,
                             unsigned char    *pRpcPayload,
                             unsigned short    usRpcLen,
                             unsigned char    *pPacket,
                             unsigned short    usPacketLen)
{
    struct sStreamDecoder str;
    struct sRpcDecoder*   pDecoder = p->m_pRpcDecoderHead;

    StreamDecoder_StreamDecoder(&str, pRpcPayload, usRpcLen);

    /* Check our one decoder (sensors) */
    if (pDecoder && pDecoder->cb(pDecoder, usRpcId, &str))
    {
        /* ensure the stream has been emptied completely and did not go beyond
         */
        if (StreamCodec_GetAvailableSize(&str))
        {
            p->callback->OnException(p->callback,__FILE__, __LINE__);
        }
        else if (StreamCodec_Fail(&str))
        {
            p->callback->OnException(p->callback,__FILE__, __LINE__);
        }
    }
    else
    {
        /* Send this data upwards, to the LHD. */
        p->callback->OnDataToLhd(p->callback, pPacket, usPacketLen);
    }
}

/*------------------------------------------------------------------------------
 *
 *      SendSensorData()
 *
 *  Convert a raw sensor data stream from the SHMD into and RPC
 *  and send the data to the packet layer.
 *
 *  This is a "mini RpcEngine"
 *
 *------------------------------------------------------------------------------
 */

bool BbdBridge_SendSensorData(struct BbdBridge* p,
                unsigned char* pSensorData, unsigned short size)
{
    if (BbdEngine_Lock(__LINE__)) {
	    struct sStreamEncoder* str = &p->m_otStreamEncoder;
	    StreamCodec_Reset      (str);
	    StreamEncoder_PutU16   (str, size);
	    StreamEncoder_PutBuffer(str, pSensorData, size);

	    if (StreamCodec_Fail(str))
	    {
    		BbdEngine_Unlock();
		return false;
	    }
	    if (!BbdBridge_AddRpc(p, RPC_DEFINITION(IRpcSensorRequest, Data)))
	    {
    		BbdEngine_Unlock();
		return false;
	    }
	    TransportLayer_SendPacket(&p->m_otTL,
			    p->m_aucTransactionPayload,
			    p->m_uiTransactionPayloadSize);
	    p->m_uiTransactionPayloadSize = 0;

	    	BbdEngine_Unlock();
	    return true;
    }
    return false;
}

bool BbdBridge_AddRpc(struct BbdBridge* p, unsigned short usRpcId)
{
    struct sStreamCodec* str   = &p->m_otStreamEncoder;
    unsigned short usRpcLen   = StreamCodec_GetOffset(str);
    unsigned int   uiNumBytes = usRpcLen +1 +1;
                                    /* 1 for usRpcId, 1 for usRpcLen*/

    if (usRpcId > 128)          /* large RPC IDs use 2 bytes */
        ++uiNumBytes;
    if (usRpcLen > 128)
        ++uiNumBytes;

    if ((p->m_uiTransactionPayloadSize+uiNumBytes) >= MAX_OUTGOING_PACKET_SIZE)
    {
        /* not enough space */
        return false;
        /* ASSERT(0);  // too many RPCs! doesn't fit inside the TL */
    }

    if (usRpcId >= 0x80)
        p->m_aucTransactionPayload[p->m_uiTransactionPayloadSize++] =
                        ((usRpcId>>8) & 0xFF) | 0x80;
    p->m_aucTransactionPayload[p->m_uiTransactionPayloadSize++] = usRpcId&0xFF;

    if (usRpcLen >= 0x80)
        p->m_aucTransactionPayload[p->m_uiTransactionPayloadSize++] =
                        ((usRpcLen>>8) & 0xFF) | 0x80;
    p->m_aucTransactionPayload[p->m_uiTransactionPayloadSize++] = usRpcLen&0xFF;

    memcpy(&p->m_aucTransactionPayload[p->m_uiTransactionPayloadSize],
                        StreamCodec_GetStreamBuffer(str),
                        StreamCodec_GetOffset      (str)  );
    p->m_uiTransactionPayloadSize += usRpcLen;
    return true;
}

/* called when LHD has a packet to send */
void BbdBridge_SendPacket(struct BbdBridge *p, unsigned char* pkt, size_t len)
{
    if (!BbdEngine_Lock(__LINE__)) return;

    if (!gpbbd_dev || !gpbbd_dev->bbd_engine.constructed || !gpbbd_dev->bbd_engine.open)
    {
	printk("[SSPBBD]: %s BBD engine already closed. ignore injected data\n", __func__);
	goto unlock_exit;
    }

    BbdTransportLayer_SendPacket(&p->m_otTL, pkt, len);

unlock_exit:
    BbdEngine_Unlock();
}

bool BbdBridge_SendReliablePacket(struct BbdBridge *p,
                struct sBbdReliableTransaction *trans)
{
    bool result = false;

    if (!gpbbd_dev || !gpbbd_dev->bbd_engine.constructed || !gpbbd_dev->bbd_engine.open)
    {
	printk("[SSPBBD]: %s BBD engine already closed. ignore injected data\n", __func__);
	goto exit;
    }

    if (BbdEngine_Lock(__LINE__)) {
        result = BbdTransportLayer_SendReliablePacket(&p->m_otTL, trans);
        BbdEngine_Unlock();
    }
exit:
    return result;
}

bool BbdBridge_SetControlMessage(struct BbdBridge *p, char *msg)
{
    bool result = false;

    if (!gpbbd_dev || !gpbbd_dev->bbd_engine.constructed || !gpbbd_dev->bbd_engine.open)
    {
	printk("[SSPBBD]: %s BBD engine already closed. ignore injected data\n", __func__);
	goto exit;
    }

    if (BbdEngine_Lock(__LINE__)) {
        result = BbdTransportLayer_SetControlMessage(&p->m_otTL, msg);
        BbdEngine_Unlock();
    }
exit:
    return result;
}

/* called when remote data are received */

void BbdBridge_SetData(struct BbdBridge *p,
                const unsigned char *pucData,
                unsigned short usSize)
{
    if (!BbdEngine_Lock(__LINE__)) return;

    if (!gpbbd_dev || !gpbbd_dev->bbd_engine.constructed || !gpbbd_dev->bbd_engine.open)
    {
	printk("[SSPBBD]: %s BBD engine already closed. ignore injected data\n", __func__);
	goto unlock_exit;
    }

    if (BbdTransportLayer_IsPassThrough(&p->m_otTL))
    {
        p->callback->OnDataToLhd(p->callback, pucData, usSize);
    }
    else
    {
        TransportLayer_SetData(&p->m_otTL, pucData, usSize);
    }

unlock_exit:
    BbdEngine_Unlock();
}

/* Provide a StreamEncoder that can be used when building a transaction */

struct sStreamEncoder* BbdBridge_GetStreamEncoder(struct BbdBridge *p)
{
    return &p->m_otStreamEncoder;
}
