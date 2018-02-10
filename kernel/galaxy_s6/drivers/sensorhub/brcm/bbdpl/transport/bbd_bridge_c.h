/****************************************************************************** 
 ** \file bbd_bridge_c.h  TransportLayer inside the BBD
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

#ifndef BBD_BRIDGE_C_H  // {
#define BBD_BRIDGE_C_H

#include <linux/string.h>

/* #include "rpc_engine.h" */
// #include "../transport/bbd_packet_layer_c.h"
#include "transport_layer_c.h"
#include "../utils/stream_codec_c.h"

#define MAX_RPC_HEADER_SIZE   4                           /* 2 for SeqId, 2 for Size */
#define MAX_TRANSACTION_SIZE  MAX_OUTGOING_PACKET_SIZE    /* maximum size per transaction */
#define MAX_RPC_PAYLOAD       (MAX_TRANSACTION_SIZE-MAX_RPC_HEADER_SIZE) /* maximum size per RPC payload */

typedef bool (*ProcessRpc) (void* data, unsigned short usRpcId,
                                struct sStreamDecoder *rStream);

#ifdef __cplusplus
extern "C" {
#endif

struct sRpcDecoder
{
    ProcessRpc          cb;
    void               *data;
    struct sRpcDecoder *m_pNext;
};

/**
* The BbdBridge class is a "mini RPC engine" that resides inside the BBD Linux
* device driver.  BBD means "Broadcom Bridge Driver".
*
* The BbdBridge has these features and device channels:
*
*   LHD-->BBD
*       receives packets from the LHD via /dev/bbd_packet
*       receives reliable packets from the LHD via /dev/bbd_reliable
*       receives control strings via /dev/bbd_control
*               reliable sync request
*               enable/disable the packet layer 
*
*   LHD<--BBD
*       sends packets to the LHD via /dev/bbd_packet
*       sends reliable packet acknowledgements to the LHD via /dev/bbd_reliable
*       sends control results to /dev/bbd_control:
*               reliable sync completed
*
*   SHMD<--BBD
*       supports ONE RPC callback for sensors.  This data comes from the shmd device
*       driver interface.
*   SHMD-->BBD
*       Converts the raw sensor data into an RPC suitable for consumption by the ESW
*       and transport layer.  Only normal transactions are supported.
*
*   BBD internals
*       Maintains a packet layer that is the peer of the ESW's packet layer.
*       Has an option to disable this packet layer (for testing intermediate steps).
*       The packet layer is a BbdTransportLayer (slightly modified) TransportLayer
*
*   SHMD-->LHD
*       Reset request string.  So far, doesn't affect BbdBridge.
*
*   SHMD<--LHD
*       ESW status messages (READY, NOTREADY, CRASHED).
*/

struct BbdBridge 
{
    struct sTransportLayer    m_otTL;         /* BbdBridge-->TransportLayer */
    //struct sITransportLayerCb m_otTLCallback; /* BbdBridge<--TransportLayer */
    struct sITransportLayerCb* callback;       /* BBD<--BbdBridge     */

    /* pointer to the first decoder. If more are registered, then
     * they are added to a linked list
     */
    struct sRpcDecoder* m_pRpcDecoderHead;

    /* place to hold a (sensor) packet before it is sent */
    struct sStreamEncoder m_otStreamEncoder;
    unsigned char m_ucStreamBuffer       [MAX_RPC_PAYLOAD         ];
    unsigned char m_aucTransactionPayload[MAX_OUTGOING_PACKET_SIZE];
    unsigned int  m_uiTransactionPayloadSize;
};

struct BbdBridge *BbdBridge_BbdBridge(struct BbdBridge *p,
                                      struct sITransportLayerCb* rCallbacks);
void BbdBridge_dtor(struct BbdBridge *p);

void BbdBridge_RegisterRpcDecoder  (struct BbdBridge *p, struct sRpcDecoder *pRpcDecoder);
void BbdBridge_UnregisterRpcDecoder(struct BbdBridge *p, struct sRpcDecoder *pRpcDecoder);

bool BbdBridge_AddRpc(struct BbdBridge *p, unsigned short usRpcId);
bool BbdBridge_SendSensorData(struct BbdBridge *p, unsigned char* data, unsigned short size);

void BbdBridge_SetPassThrough(struct BbdBridge *p, bool passthru);

/* called when LHD has a packet to send */
void BbdBridge_SendPacket(struct BbdBridge *p, unsigned char* data, size_t len);
bool BbdBridge_SendReliablePacket(struct BbdBridge *p, struct sBbdReliableTransaction *trans);
bool BbdBridge_SetControlMessage(struct BbdBridge *p, char *msg);

/* called when remote data are received */
void BbdBridge_SetData(struct BbdBridge *p, const unsigned char *pData, unsigned short usSize);

/* called when remote packet is decoded */
void BbdBridge_OnPacketReceived(struct sTransportLayer* p,
                        unsigned char *pucData,
                        unsigned short usSize);

/* Provide a StreamEncoder that can be used when building a transaction */
struct sStreamEncoder* BbdBridge_GetStreamEncoder(struct BbdBridge *p);

#ifdef __cplusplus
}
#endif

#endif  // } BBD_BRIDGE_H

