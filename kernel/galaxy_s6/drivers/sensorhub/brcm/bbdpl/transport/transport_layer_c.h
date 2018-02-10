/****************************************************************************** 
 ** \file transport_layer_c.h  TransportLayer struct declaration
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

#ifndef TRANSPORT_LAYER_C_H
#define TRANSPORT_LAYER_C_H

/*  This is the core transport layer for the Broadcom Bridge Driver
 *  It is a straightforward migration of C++ embedded code to C
 *  The BBD-specific aspects are:
 *  -  bool m_bPassThrough
 *  -  some extra callbacks
 *  -  some extra function with BbdTransportLayer_ prefix.
 */

#include "../utils/bbd_utils.h"
#include "../transport/transport_layer_custom.h"
#include "../utils/crc8bits_c.h"

#ifndef TRANSPORT_LAYER_H
/* prototype for callback associated with a packet for reliable transactions */
typedef void (*PacketDeliveryNotification)(void* pCbData, unsigned long ulCbData);
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct sBbdReliableTransaction
{
    unsigned char*             pucData;
    unsigned short             usSize;
    PacketDeliveryNotification cb;
    void*                      pCbData;
    unsigned long              ulCbData;
    // void*                      pDriverReserved;
    // unsigned long              ulDriverReserved;
    // void*                      pTlReserved;
};

struct sBbdReliableTransaction* BbdReliableTransaction_init(struct sBbdReliableTransaction* p);

struct stCallback
{
    PacketDeliveryNotification cb;
    void                      *pCbData;
    unsigned long              ulCbData;
};

struct sITransportLayerCb;

/*
 * Callbacks needed by the TransportLayer
 */
typedef void (*ITransportCB_OnTimerSet)(struct sITransportLayerCb*, long lTimerMs);
typedef unsigned long (*ITransportCB_OnTimeMsRequest)(struct sITransportLayerCb*);
typedef void (*ITransportCB_OnDataToSend)(struct sITransportLayerCb*, unsigned char *pucData, unsigned long ulSize);
typedef void (*ITransportCB_OnCommunicationError)(struct sITransportLayerCb*);
typedef void (*ITransportCB_OnPacketReceived)(struct sITransportLayerCb*, unsigned char *pucData, unsigned short usSize);
typedef void (*ITransportCB_OnException)(struct sITransportLayerCb*, const char filename[], unsigned int uiLine);
typedef void (*ITransportCB_OnRemoteSyncComplete)(struct sITransportLayerCb*);
typedef void (*ITransportCB_OnReliableAck)(struct sITransportLayerCb*, struct sBbdReliableTransaction*);
typedef void (*ITransportCB_OnDataToLhd)(struct sITransportLayerCb*, const unsigned char *pucData, unsigned short usSize);

struct sITransportLayerCb
{
    void *m_callbackData;

    /* TransportLayer can sleep for lTimerMs. -1 means the TransportLayer
     * is idle
     */
    ITransportCB_OnTimerSet OnTimerSet;

    /* TransportLayer needs to know about the platform time (32bit ms timer
     * monotically increasing)
     */
    ITransportCB_OnTimeMsRequest OnTimeMsRequest;

    /* Data must be sent to the remote TransportLayer */
    ITransportCB_OnDataToSend OnDataToSend;

    /* TransportLayer was not able to recover due to communication errors */
    ITransportCB_OnCommunicationError OnCommunicationError;

    /* The payload from a packet was received from the remote TransportLayer */
    ITransportCB_OnPacketReceived OnPacketReceived;

    /* an internal error occured */
    ITransportCB_OnException OnException;

    /* The synchronization with the remote side is complete, you can
     * start using the Transport Layer to communicate with the remote side
     */
    ITransportCB_OnRemoteSyncComplete OnRemoteSyncComplete;

    ITransportCB_OnReliableAck OnReliableAck;
    ITransportCB_OnDataToLhd   OnDataToLhd;
};

/*
 * statistics of the TransportLayer
 */
struct stTransportLayerStats
{
        unsigned long ulRxGarbageBytes;
        unsigned long ulRxPacketLost;
        /* this is approximate as it is reported and the report could be lost */
        unsigned long ulRemotePacketLost;
        /* this is approximate as it is reported and the report could be lost */
        unsigned long ulRemoteGarbage;
        unsigned long ulPacketSent;         /* number of normal packets sent */
        unsigned long ulPacketReceived;
        unsigned long ulAckReceived;
        unsigned long ulReliablePacketSent;
        unsigned long ulReliableRetransmit;
        unsigned long ulReliablePacketReceived;
        unsigned long ulMaxRetransmitCount;
};

struct stTransportLayerStats* stTransportLayerStats_stTransportLayerStats(
                       struct stTransportLayerStats* p);


/* public constants that are built from the customization file */
#define MAX_OUTGOING_PACKET_SIZE  TLCUST_MAX_OUTGOING_PACKET_SIZE
#define MAX_INCOMING_PACKET_SIZE  TLCUST_MAX_INCOMING_PACKET_SIZE
#define RELIABLE_RETRY_TIMEOUT_MS TLCUST_RELIABLE_RETRY_TIMEOUT_MS
#define RELIABLE_MAX_RETRY        TLCUST_RELIABLE_MAX_RETRY
#define RELIABLE_MAX_PACKETS      TLCUST_RELIABLE_MAX_PACKETS

#define MAX_HEADER_SIZE 14

/*
 * The TransportLayer struct exchanges packets with a remote TransportLayer.
 * The data is a stream of bytes.
 * The TransportLayer is agnostic to the payload. It supports a reliable
 * channel that will retransmit packets until the other side acknowledges it
 * to ensure delivery and order is maintained.
 */

struct sTransportLayer
{
    struct sITransportLayerCb *m_callback;

    unsigned int m_uiParserState;
    unsigned char m_aucRxMessageBuf[MAX_INCOMING_PACKET_SIZE+MAX_HEADER_SIZE];
    unsigned int m_uiRxLen;
    unsigned char m_ucLastRxSeqId;
    unsigned long m_ulByteCntSinceLastValidPacket;

    unsigned char m_ucLastAckSeqId;
    unsigned char m_ucReliableSeqId;
    unsigned char m_ucReliableCrc;
    unsigned char m_ucDelayAckCount;
    unsigned short m_usReliableLen;

    unsigned char m_aucTxBuffer[(MAX_OUTGOING_PACKET_SIZE+MAX_HEADER_SIZE+2)*2];
    unsigned char m_ucTxSeqId;
    struct sCrc8Bits m_otTxCrc; /* tx CRC */

    struct stTransportLayerStats m_otCurrentStats;
    struct stTransportLayerStats m_otLastPLUpdatedStats;

    unsigned char m_ucTxReliableSeqId;
    unsigned char m_ucTxLastAckedSeqId;
    struct stReliableElement *m_pFirstReliableEl;
    struct stReliableElement *m_pLastReliableEl;
    unsigned long m_ulReliableByteCnt;

    bool m_bWait4ReliableAckToProcessNack;

    bool m_bOngoingSync;
    unsigned long m_ulSyncTimestamp;
    unsigned long m_ulRetryCnt;

    const char* m_pDebugName;

    /* Here are the BBD-specific items */
    bool m_bPassThrough;
};

/* constructor. Need a reference to the IPacketLayerCb for the
 * callback mechanism.
 */
struct sTransportLayer* TransportLayer_TransportLayer(struct sTransportLayer* p,
                                struct sITransportLayerCb* rCallback, const char *pDebugName);
void TransportLayer_dtor(struct sTransportLayer* p);

/* Sends a packet to the remote TransportLayer. */
void TransportLayer_SendPacket(struct sTransportLayer* p, unsigned char *pucData,
                        unsigned short usSize);

/* Sends a reliable packet to the remote TransportLayer. Delivery is
 * ensured, and PacketDeliveryNotification callback will be called
 * (if not NULL) when delivery is confirmed
 */
bool TransportLayer_SendReliablePacket(struct sTransportLayer* p,
                unsigned char *pucData, unsigned short usSize,
                PacketDeliveryNotification cb, void *pCbData,
                unsigned long ulCbData);

/* Push data from remote TransportLayer */
void TransportLayer_SetData(struct sTransportLayer* p, const unsigned char *pucData,
                unsigned short usSize);

/* Called to refresh internal timers. Should be called when timer
 * returned by OnTimerSet callback expires
 */
void TransportLayer_Tick(struct sTransportLayer* p);

/* Reset the TL seqId. That should be used if the Remote party was reset,
 * otherwise, the reliable transport will break down, and the statistics
 * will show inconsistent data
 */
void TransportLayer_Reset(struct sTransportLayer* p);

/* The TL layer will start a synchronization with the remote side. That
 * could be used when started a TL and not sure about the other side state.
 * A remote sync should happen without any other transactions (i.e. no send
 * packets!).
 * OnRemoteSyncComplete is called once this is complete
 */
void TransportLayer_StartRemoteSync(struct sTransportLayer* p);

/* Return the size of the data in the reliable buffer (i.e. which has
 * not been acked yet)
 */
unsigned long TransportLayer_GetReliableUsage(const struct sTransportLayer* const p);

/* Return true if it is possible to send a reliable packet
 * (limited by RELIABLE_MAX_PACKETS packets, and the intrinsic max is 255)
 */
bool TransportLayer_IsReliableAvailable(const struct sTransportLayer* const p);

/* Get a reference to the TL statistics */
const struct stTransportLayerStats* const TransportLayer_GetStats(
                const struct sTransportLayer* const p);

#ifdef __cplusplus
}
#endif

#endif /* TRANSPORT_LAYER_C_H */

