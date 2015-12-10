/******************************************************************************
 ** \file transport_layer_c.c  TransportLayer struct definition
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

#include "../utils/bbd_utils.h"
#include "../bbd_internal.h"
#include "bbd_packet_layer_c.h"
#include "../utils/crc8bits_c.h"
#include "../utils/stream_codec_c.h"
#include "bbd_bridge_c.h"

/*
* The following are used for the software flow control (UART)
*/
static const unsigned char XON_CHARACTER = 0x11;
static const unsigned char XOFF_CHARACTER = 0x13;

/*
* The following are used for the protocol.
*/
static const unsigned char ESCAPE_CHARACTER         = 0xB0;
static const unsigned char SOP_CHARACTER            = 0x00;
static const unsigned char EOP_CHARACTER            = 0x01;
static const unsigned char ESCAPED_ESCAPE_CHARACTER = 0x03;
static const unsigned char ESCAPED_XON_CHARACTER    = 0x04;
static const unsigned char ESCAPED_XOFF_CHARACTER   = 0x05;

/*
* The following are the bit field definition for the flags
*/
static const unsigned short FLAG_PACKET_ACK      = (1<<0); // ACK of a received packet. Flag detail contains the ACK SeqId
static const unsigned short FLAG_RELIABLE_PACKET = (1<<1); // Indicates that this is a reliable packet. Flag detail contains the remote reliable seqId
static const unsigned short FLAG_RELIABLE_ACK    = (1<<2); // A reliable SeqId was Acked. Flag detail contains the acked reliable seqId
static const unsigned short FLAG_RELIABLE_NACK   = (1<<3); // A reliable SeqId error was detected. Flag detail contains the last remote reliable seqId
static const unsigned short FLAG_MSG_LOST        = (1<<4); // Remote PacketLayer detected lost packets (jumps in SeqId). The flag details contains the last remote seqId
static const unsigned short FLAG_MSG_GARBAGE     = (1<<5); // Garbage bytes detected. The Flag details will contains the number of garbage bytes (capped to 255)
static const unsigned short FLAG_SIZE_EXTENDED   = (1<<6); // Size of packet will have one byte extension (MSB), contained in the Flags detail
static const unsigned short FLAG_EXTENDED        = (1<<7); // If set, then the flag details will contains a Byte representing the MSB of the 16bit flags
static const unsigned short FLAG_INTERNAL_PACKET = (1<<8); // Packet in that message is internal, and should be processed by the TL
static const unsigned short FLAG_IGNORE_SEQID    = (1<<9); // Remote side requested to ignore the seqId, i.e. error should not be accounted for in the stats

/*
 * Internal functions
 */
static void TransportLayer_ParseIncomingData(struct sTransportLayer* p,
                        const unsigned char *pucData, unsigned short usLen);
static bool TransportLayer_PacketReceived(struct sTransportLayer* p);
static void TransportLayer_ReceivedReliableAck(struct sTransportLayer* p,
                        unsigned char ucAckedReliableSeqId);
static void TransportLayer_ReceivedReliableNack(struct sTransportLayer* p,
                        unsigned char ucLastReceivedReliableSeqId,
                        unsigned char ucAckedSeqId);
static void TransportLayer_SendPacketAck(struct sTransportLayer* p,
                        unsigned short usAckFlags);
static unsigned long TransportLayer_EscapeDataByte(struct sTransportLayer* p,
                        unsigned char *pucDestBuff, unsigned char ucData);
static unsigned long TransportLayer_EscapeData(struct sTransportLayer* p,
                        unsigned char *pucDestBuff,
                        const unsigned char *pucSrcBuff,
                        unsigned short usSize);
static unsigned long TransportLayer_EscapeDataAndUpdateCrcByte(
                        struct sTransportLayer* p,
                        unsigned char *pucDestBuff,
                        unsigned char ucData);
static unsigned long TransportLayer_EscapeDataAndUpdateCrc(
                        struct sTransportLayer* p,
                        unsigned char *pucDestBuff,
                        const unsigned char *pucSrcBuff,
                        unsigned short usSize);
static unsigned char TransportLayer_BuildAndSendPacket(
                        struct sTransportLayer* p,
                        unsigned short usFlags,
                        unsigned char *pucData,
                        unsigned short usSize,
                        unsigned char ucReliableTxSeqId);
static unsigned int TransportLayer_GetReliablePacketInTransitCnt(
                        const struct sTransportLayer* const p);
static void TransportLayer_SendInternalSeqIdRequest(struct sTransportLayer* p);
static void TransportLayer_OnInternalPacket(struct sTransportLayer* p,
                        unsigned char *pucData, unsigned short usSize);
static void TransportLayer_SendInternalPacket(struct sTransportLayer* p,
                        unsigned char ucId,
                        const unsigned char *pucData,
                        unsigned short usSize);
static void TransportLayer_OnInternalSeqIdRequest(struct sTransportLayer* p,
                        unsigned char ucReqId);
static void TransportLayer_OnInternalSeqIdResponse(struct sTransportLayer* p,
                        unsigned char ucReqId,
                        unsigned char ucLastRxSeqId,
                        unsigned char ucLastAckSeqId,
                        unsigned char ucReliableSeqId,
                        unsigned char ucTxSeqId,
                        unsigned char ucTxReliableSeqId,
                        unsigned char ucTxLastAckedSeqId);
static void TransportLayer_OnInternalStatsRequest(struct sTransportLayer* p);
static void TransportLayer_OnInternalStatsResponse(struct sTransportLayer* p,
                        struct stTransportLayerStats *rStats);
static void TransportLayer_OnInternalSettingsRequest(struct sTransportLayer* p);
static void TransportLayer_OnInternalSettingsResponse(struct sTransportLayer* p,
                        unsigned short usMaxOutgoingPacketSize,
                        unsigned short usMaxIncomingPacketSize,
                        unsigned short usReliableRetryTimeoutMs,
                        unsigned short usReliableMaxRetry,
                        unsigned short usReliableMaxPackets);

/*
* Protocol used to transport a packet from one TransportLayer to a remote TransportLayer
*
* Packet structure: |ESC|SOP|<ESCAPED DATA>|ESC|EOP|
*     - ESC : ESCAPE_CHARACTER
*     - SOP: SOP_CHARACTER
*     - EOP: EOP_CHARACTER
*     - ESCAPED_DATA: |SEQ_ID|<CRCED_DATA>|CRC|
*         -- SEQ_ID: Running Sequence Id
*         -- CRC: 8 bits CRC
*         -- CRCED_DATA: |SIZE|FLAGS|FLAGS_DETAILS|<PAYLOAD>|
*             --- SIZE: 1B containing the LSB for the size of the payload. Possible MSB contained in the FLAG_DETAIL
*             --- FLAGS: 1B containing different flags for this packet
*             --- FLAGS_DETAILS: 1B for each bit set in FLAGS (LSB for LSbit)
*             --- PAYLOAD: Packet payload
*
*/

/*
* Internal packet types
*/
enum
{
     INT_PKT_SEQID_REQUEST     /* Request to send the Sequence Ids */
    ,INT_PKT_SEQID_RESPONSE    /* Sequence Ids responses */
    ,INT_PKT_STATS_REQUEST     /* Request for the remote stats to be sent */
    ,INT_PKT_STATS_RESPONSE    /* Response with the stats */
    ,INT_PKT_SETTINGS_REQUEST  /* Request to get internal settings of remote side */
    ,INT_PKT_SETTINGS_RESPONSE /* Response with the settings */
};


/// enumeration for the state
enum
{
     WAIT_FOR_ESC_SOP = 0
    ,WAIT_FOR_SOP
    ,WAIT_FOR_MESSAGE_COMPLETE
    ,WAIT_FOR_EOP
};

/// Macro for returning an Exception
#define ASSERT(_test)       {if (!(_test)) {p->m_callback->OnException(p->m_callback, __FILE__, __LINE__);}}
#define ASSERT_RET(_test)   {if (!(_test)) {p->m_callback->OnException(p->m_callback, __FILE__, __LINE__);return;}}
#define ASSERT_RET_0(_test) {if (!(_test)) {p->m_callback->OnException(p->m_callback, __FILE__, __LINE__);return 0;}}

// Enables debug (will turn on logging)
#define PL_DEBUG 0
#if PL_DEBUG
    #define PLLOG(fmt, ...) {printf("%s:",(p->m_pDebugName==NULL)?"":p->m_pDebugName);printf(fmt, ##__VA_ARGS__);}
#else
    #define PLLOG(fmt,...)
#endif

struct stReliableElement
{
    unsigned char ucReliableSeqId;
    unsigned char ucTxSeqId;
    unsigned char ucNumSent;
    unsigned char *pucData;
    bool bForceResend;
    unsigned long ulTimestampMs;
    unsigned short usDataSize;
    struct stCallback sCallback;
    struct stReliableElement *pNext;
};

struct stReliableElement* stReliableElement_stReliableElement(struct stReliableElement *p)
{
    memset(p, 0, sizeof(*p));
    /* p->ucReliableSeqId = 0; */
    /* p->ucTxSeqId = 0; */
    /* p->ucNumSent = 0; */
    /* p->pucData = NULL; */
    /* p->bForceResend = false; */
    /* p->ulTimestampMs = 0; */
    /* p->usDataSize = 0; */
    /* p->pNext = NULL; */
    /* p->sCallback.cb = NULL; */
    /* p->sCallback.pCbData = NULL; */
    /* p->sCallback.ulCbData = 0; */
    return p;
}

struct stTransportLayerStats* stTransportLayerStats_stTransportLayerStats(
        struct stTransportLayerStats* p)
{
    memset(p, 0, sizeof(*p));
    /* p->ulRxGarbageBytes   = 0; */
    /* p->ulRxPacketLost   = 0; */
    /* p->ulRemotePacketLost   = 0; */
    /* p->ulRemoteGarbage   = 0; */
    /* p->ulPacketSent   = 0; */
    /* p->ulPacketReceived   = 0; */
    /* p->ulAckReceived   = 0; */
    /* p->ulReliablePacketSent   = 0; */
    /* p->ulReliableRetransmit   = 0; */
    /* p->ulReliablePacketReceived   = 0; */
    /* p->ulMaxRetransmitCount   = 0; */
    return p;
}

struct sTransportLayer* TransportLayer_TransportLayer(struct sTransportLayer* p,
                            struct sITransportLayerCb *rCallback,
                            const char *pDebugName)
{
    memset(p, 0, sizeof(*p));
    p->m_pDebugName                     = pDebugName;
    p->m_callback                       = rCallback;
    p->m_uiParserState                  = WAIT_FOR_ESC_SOP;
    p->m_ucLastRxSeqId                  = 0xFF;
    p->m_ucReliableSeqId                = 0xFF;
    p->m_ucTxLastAckedSeqId             = 0xFF;
    p->m_bPassThrough                   = true;

    /* p->m_uiRxLen                        = 0; */
    /* p->m_ulByteCntSinceLastValidPacket  = 0; */
    /* p->m_ucLastAckSeqId                 = 0; */
    /* p->m_ucReliableCrc                  = 0; */
    /* p->m_usReliableLen                  = 0; */
    /* p->m_ucDelayAckCount                = 0; */
    /* p->m_ucTxSeqId                      = 0; */
    /* p->m_ucTxReliableSeqId              = 0; */
    /* p->m_pFirstReliableEl               = NULL; */
    /* p->m_pLastReliableEl                = NULL; */
    /* p->m_ulReliableByteCnt              = 0; */
    /* p->m_bWait4ReliableAckToProcessNack = false */
    /* p->m_bOngoingSync                   = false; */
    /* p->m_ulSyncTimestamp                = 0; */
    /* p->m_ulRetryCnt                     = 0; */

    {
        ASSERT_COMPILE(MAX_OUTGOING_PACKET_SIZE > MAX_HEADER_SIZE && MAX_OUTGOING_PACKET_SIZE < USHRT_MAX);
        ASSERT_COMPILE(MAX_INCOMING_PACKET_SIZE > MAX_HEADER_SIZE && MAX_INCOMING_PACKET_SIZE < USHRT_MAX);
    }

    TransportLayer_Reset(p);
    return p;
}

void TransportLayer_dtor(struct sTransportLayer* p)
{
    while (p->m_pFirstReliableEl != NULL)
    {
        struct stReliableElement *pToRemove = p->m_pFirstReliableEl;
        /* advance to next element */
        p->m_pFirstReliableEl = p->m_pFirstReliableEl->pNext;

        p->m_ulReliableByteCnt -= pToRemove->usDataSize;

        bbd_free(pToRemove->pucData);
        bbd_free(pToRemove);
    }

    p->m_bOngoingSync = false;  // BBD is closing. We need to prevent TransportLayer_Tick from adding next tick

    /* for now assert, but we should release the memory instead */
    ASSERT(TransportLayer_GetReliableUsage(p) == 0);
}

void TransportLayer_Reset(struct sTransportLayer* p)
{
    /* we cannot reset if we have ongoing reliable transactions */
    ASSERT_RET(TransportLayer_GetReliablePacketInTransitCnt(p) == 0);
    
    /* clear ongoing transactions . That means the user better reset
     * at the appropriate time, or it will mess up some packets
     */
    p->m_uiParserState = WAIT_FOR_ESC_SOP;
    p->m_uiRxLen = 0;
    p->m_ulByteCntSinceLastValidPacket = 0;

    p->m_ucLastRxSeqId = 0xFF;
    p->m_ucLastAckSeqId = 0;
    p->m_ucReliableSeqId = 0xFF;
    p->m_ucReliableCrc = 0;
    p->m_usReliableLen = 0;
    p->m_ucDelayAckCount = 0;

    p->m_ucTxSeqId = 0;
    p->m_ucTxLastAckedSeqId = 0xFF;
    p->m_ucTxReliableSeqId = 0;
    
    p->m_bWait4ReliableAckToProcessNack = false;
}

void TransportLayer_StartRemoteSync(struct sTransportLayer* p)
{
    {
    ASSERT_RET(TransportLayer_GetReliablePacketInTransitCnt(p) == 0); // we cannot start a sync if we have ongoing reliable transactions
    ASSERT_RET(!p->m_bOngoingSync);
    }

    PLLOG("TransportLayer_RemoteSync()\n");
    p->m_bOngoingSync = true;
    p->m_ulRetryCnt = 0;

    TransportLayer_SendInternalSeqIdRequest(p);

    TransportLayer_Tick(p); // refresh internal timers
}

void TransportLayer_SendInternalSeqIdRequest(struct sTransportLayer* p)
{
    unsigned short usFlags = FLAG_INTERNAL_PACKET | FLAG_IGNORE_SEQID; 
    unsigned char ucBuf[2];

    p->m_ulSyncTimestamp = p->m_callback->OnTimeMsRequest(p->m_callback);
    p->m_ulRetryCnt++;

    ucBuf[0] = INT_PKT_SEQID_REQUEST;
    ucBuf[1] = (unsigned char)(p->m_ulRetryCnt&0xFF);
    TransportLayer_BuildAndSendPacket(p, usFlags, ucBuf, sizeof(ucBuf), 0);
    PLLOG("TransportLayer_SendInternalSeqIdRequest(%lu, %lu)\n",
                        p->m_ulSyncTimestamp, p->m_ulRetryCnt);
}


void TransportLayer_SendPacket(struct sTransportLayer* p,
                        unsigned char *pucData, unsigned short usSize)
{
    ASSERT_RET(usSize <= MAX_OUTGOING_PACKET_SIZE);
    ASSERT_RET(!p->m_bOngoingSync); // TL cannot be used during sync

    /* no special flags when sending a regular packet */
   {
       unsigned short usFlags = 0;
    TransportLayer_BuildAndSendPacket(p, usFlags, pucData, usSize, 0);
    ++p->m_otCurrentStats.ulPacketSent;
   }
}


unsigned char TransportLayer_BuildAndSendPacket(struct sTransportLayer* p,
                unsigned short usFlags, unsigned char *pucData,
                unsigned short usSize, unsigned char ucReliableTxSeqId)
{
    /* we should never have more than OUTGOING_PACKET_SIZE or we might
     * have stack corruption
     */
    ASSERT_RET_0(usSize <= MAX_OUTGOING_PACKET_SIZE);
    {
    unsigned char ucSizeMSB = (usSize>>8)&0xFF;
    unsigned char ucSizeLSB = usSize&0xFF;
    unsigned char ucTxSeqId = p->m_ucTxSeqId++;
    unsigned long ulPacketSize = 0;
    unsigned int i = 0;

    if (ucSizeMSB > 0)
    {
        /* if size is over 255B, mark the size_extension */
        usFlags |= FLAG_SIZE_EXTENDED;
    }

    if (usFlags == 0 && p->m_ucDelayAckCount)
    {
        dprint("Piggyback %u seq %u\n", 
                p->m_ucDelayAckCount, p->m_ucLastRxSeqId);
        // we have a data packet going out anyway, so piggyback
        // the ACK onto it.
        usFlags = FLAG_PACKET_ACK;  // delay no further
        p->m_ucDelayAckCount = 0;
    }

    if (usFlags > 0xFF)
    {
        /* if the flags use more than one byte, set the appropriate flag */
        usFlags |= FLAG_EXTENDED;
    }

    Crc8Bits_Reset(&p->m_otTxCrc);

    /* update the flags */
    if (p->m_otLastPLUpdatedStats.ulRxGarbageBytes !=
        p->m_otCurrentStats.ulRxGarbageBytes)
    {
        usFlags |= FLAG_MSG_GARBAGE;
    }
    if (p->m_otLastPLUpdatedStats.ulRxPacketLost !=
        p->m_otCurrentStats.ulRxPacketLost)
    {
        usFlags |= FLAG_MSG_LOST;
    }

    /* Header */
    p->m_aucTxBuffer[ulPacketSize++] = ESCAPE_CHARACTER;
    p->m_aucTxBuffer[ulPacketSize++] = SOP_CHARACTER;
    ulPacketSize += TransportLayer_EscapeDataByte(p,
                        &p->m_aucTxBuffer[ulPacketSize], ucTxSeqId);
    ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                        &p->m_aucTxBuffer[ulPacketSize], ucSizeLSB);

    /* first encode the flags LSB (flags MSB is part of an extension) */
    ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                        &p->m_aucTxBuffer[ulPacketSize], (usFlags&0xFF));
    /* and then encode the flags_details */
    for (i = 0; i < 16 && usFlags != 0; ++i)
    {
        unsigned short usFlagMask = (1<<i);
        unsigned short usFlagBit  = usFlags&usFlagMask;

        if (usFlagBit == 0)
        {
            continue;
        }

        if ( usFlagBit == FLAG_PACKET_ACK) /* acknowledgement */
        {
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p, 
                              &p->m_aucTxBuffer[ulPacketSize],
                              p->m_ucLastRxSeqId);
        }
        /* This is a reliable packet. we need to provide the proper Ack */
        else if (usFlagBit == FLAG_RELIABLE_PACKET)
        {
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p, 
                                &p->m_aucTxBuffer[ulPacketSize],
                                ucReliableTxSeqId);
        }
        else if (usFlagBit == FLAG_RELIABLE_ACK)
        {
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                                &p->m_aucTxBuffer[ulPacketSize],
                                 p->m_ucReliableSeqId);
        }
        else if (usFlagBit == FLAG_RELIABLE_NACK)
        {
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                                &p->m_aucTxBuffer[ulPacketSize],
                                p->m_ucReliableSeqId);
        }
        else if (usFlagBit == FLAG_MSG_LOST)
        {
            /* It cannot be more than 255 as we are acking every packet */
            unsigned char ucNumPacketLost = (unsigned char)
                        (p->m_otCurrentStats.ulRxPacketLost -
                         p->m_otLastPLUpdatedStats.ulRxPacketLost);

            p->m_otLastPLUpdatedStats.ulRxPacketLost =
                         p->m_otCurrentStats.ulRxPacketLost;
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                         &p->m_aucTxBuffer[ulPacketSize], ucNumPacketLost);
        }
        else if (usFlagBit == FLAG_MSG_GARBAGE)
        {
            unsigned long ulNumGarbageLost =
                        p->m_otCurrentStats.ulRxGarbageBytes -
                        p->m_otLastPLUpdatedStats.ulRxGarbageBytes;
            if (ulNumGarbageLost > 0xFF)
            {
                ulNumGarbageLost = 0xFF; /* max we can send is 1B */
            }
            p->m_otLastPLUpdatedStats.ulRxGarbageBytes += ulNumGarbageLost;

            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p, 
                        &p->m_aucTxBuffer[ulPacketSize],
                        (unsigned char)ulNumGarbageLost);
        }
        else if (usFlagBit == FLAG_SIZE_EXTENDED)
        {
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                        &p->m_aucTxBuffer[ulPacketSize], ucSizeMSB);
        }
        else if (usFlagBit == FLAG_EXTENDED)
        {
            /* encode the MSB of the 16bit flags */
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                        &p->m_aucTxBuffer[ulPacketSize], ((usFlags>>8)&0xFF));
        }
        else if (usFlagBit == FLAG_INTERNAL_PACKET)
        {
            /* we don't care about the details, but need to encode a
             * dummy byte
             */
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                        &p->m_aucTxBuffer[ulPacketSize], 0x0);
        }
        else if (usFlagBit == FLAG_IGNORE_SEQID)
        {
            /* we don't care about the details, but need to encode a
             * dummy byte
             */
            ulPacketSize += TransportLayer_EscapeDataAndUpdateCrcByte(p,
                        &p->m_aucTxBuffer[ulPacketSize], 0x0);
        }
        else
        {
            ASSERT(0); // safe assert, no need to return
            break;
        }

        usFlags &= ~usFlagBit;
    }

    ulPacketSize += TransportLayer_EscapeDataAndUpdateCrc(p,
                        &p->m_aucTxBuffer[ulPacketSize], pucData, usSize);

    {
    unsigned char ucCrc = Crc8Bits_GetState(&p->m_otTxCrc);
    ucCrc = ((ucCrc&0x0F)<<4) | ((ucCrc&0xF0)>>4);

    ulPacketSize += TransportLayer_EscapeDataByte(p,
                        &p->m_aucTxBuffer[ulPacketSize], ucCrc);

    p->m_aucTxBuffer[ulPacketSize++] = ESCAPE_CHARACTER;
    p->m_aucTxBuffer[ulPacketSize++] = EOP_CHARACTER;

    p->m_callback->OnDataToSend(p->m_callback, p->m_aucTxBuffer,
                        ulPacketSize);
    return ucTxSeqId;
    }
}
}

unsigned long TransportLayer_EscapeDataByte(struct sTransportLayer* p,
                unsigned char *pucDestBuff, unsigned char ucData)
{
    return TransportLayer_EscapeData(p, pucDestBuff, &ucData, 1);
}


unsigned long TransportLayer_EscapeData(struct sTransportLayer* p,
                unsigned char *pucDestBuff, const unsigned char *pucSrcBuff,
                unsigned short usSize)
{
    unsigned long ulCnt = 0;
    unsigned int i = 0;
    for (i = 0; i < usSize; ++i)
    {
        if (pucSrcBuff[i] == ESCAPE_CHARACTER)
        {
            pucDestBuff[ulCnt++] = ESCAPE_CHARACTER;
            pucDestBuff[ulCnt++] = ESCAPED_ESCAPE_CHARACTER;
        }
        else if (pucSrcBuff[i] == XON_CHARACTER)
        {
            pucDestBuff[ulCnt++] = ESCAPE_CHARACTER;
            pucDestBuff[ulCnt++] = ESCAPED_XON_CHARACTER;
        }
        else if (pucSrcBuff[i] == XOFF_CHARACTER)
        {
            pucDestBuff[ulCnt++] = ESCAPE_CHARACTER;
            pucDestBuff[ulCnt++] = ESCAPED_XOFF_CHARACTER;
        }
        else // regular data
        {
            pucDestBuff[ulCnt++] = pucSrcBuff[i];
        }
    }
    return ulCnt;
}

unsigned long TransportLayer_EscapeDataAndUpdateCrcByte(struct sTransportLayer* p,
                unsigned char *pucDestBuff,
                unsigned char ucData)
{
    Crc8Bits_UpdateByte(&p->m_otTxCrc, ucData);
    return TransportLayer_EscapeDataByte(p, pucDestBuff, ucData);
}

unsigned long TransportLayer_EscapeDataAndUpdateCrc(struct sTransportLayer* p,
                                                    unsigned char *pucDestBuff,
                                                    const unsigned char *pucSrcBuff,
                                                    unsigned short usSize)
{
    Crc8Bits_Update(&p->m_otTxCrc, pucSrcBuff, usSize);
    return TransportLayer_EscapeData(p, pucDestBuff, pucSrcBuff, usSize);
}

bool TransportLayer_SendReliablePacket( struct sTransportLayer* p,
                                        unsigned char *pucData,
                                        unsigned short usSize,
                                        PacketDeliveryNotification cb,
                                        void *pCbData,
                                        unsigned long ulCbData)
{
#if TLCUST_ENABLE_RELIABLE_PL
    bool userFail = false;
    ASSERT_RET_0(usSize <= MAX_OUTGOING_PACKET_SIZE);
    ASSERT_RET_0(TransportLayer_IsReliableAvailable(p));
    ASSERT_RET_0(!p->m_bOngoingSync);  // TL cannot be used during sync

    // allocate memory to hold the data
    {
        struct stReliableElement *pElement = (struct stReliableElement *) bbd_alloc(sizeof(struct stReliableElement));
    if (!pElement)
        return false;
    stReliableElement_stReliableElement(pElement);
    pElement->ucReliableSeqId = p->m_ucTxReliableSeqId;
    p->m_ucTxReliableSeqId = (p->m_ucTxReliableSeqId+1)&0xFF;
    pElement->ulTimestampMs = p->m_callback->OnTimeMsRequest(p->m_callback);
    pElement->ucNumSent = 1;
    pElement->pucData = (unsigned char *) bbd_alloc(usSize);
    if (!pElement->pucData) {
        bbd_free(pElement);
        return false;
    }
    userFail = copy_from_user(pElement->pucData, pucData, usSize);
    if (userFail) {
        bbd_free(pElement->pucData);
        bbd_free(pElement);
        return false;
    }
    // memcpy(pElement->pucData, pucData, usSize);
    pElement->usDataSize = usSize;
    pElement->sCallback.cb = cb;
    pElement->sCallback.pCbData = pCbData;
    pElement->sCallback.ulCbData = ulCbData;
    pElement->pNext = NULL;

    p->m_ulReliableByteCnt += usSize;

    // Let's insert that element
    if (p->m_pFirstReliableEl == NULL)
    {
        p->m_pFirstReliableEl = pElement;
        p->m_pLastReliableEl = pElement;
    }
    else
    {
        // link it to the list
        p->m_pLastReliableEl->pNext = pElement;
        p->m_pLastReliableEl = pElement;
    }

    pElement->ucTxSeqId = TransportLayer_BuildAndSendPacket(p, FLAG_RELIABLE_PACKET, pElement->pucData, pElement->usDataSize, pElement->ucReliableSeqId);
    PLLOG("TransportLayer_SendReliablePacket(Size %u, SeqId %u, %lu ms, RelSeqId %u)\n", usSize, pElement->ucTxSeqId, pElement->ulTimestampMs, pElement->ucReliableSeqId);
    p->m_otCurrentStats.ulReliablePacketSent++;

    TransportLayer_Tick(p); /* refresh timers */
    }
    return true;

#else /* TLCUST_ENABLE_RELIABLE_PL */
    ASSERT_RET(0);
    return false;
#endif
}

void TransportLayer_SetData(struct sTransportLayer* p, const unsigned char *pucData,
                unsigned short usSize)
{
    PLLOG("TransportLayer_SetData(Size %u)\n", usSize);
    TransportLayer_ParseIncomingData(p, pucData, usSize);
}

void TransportLayer_Tick(struct sTransportLayer* p)
{
    // check if there is anything in the reliable layer before bothering the upper layer
    unsigned long ulTimeNowMs = p->m_callback->OnTimeMsRequest(p->m_callback);

    long lTimerMs = RELIABLE_RETRY_TIMEOUT_MS;
    struct stReliableElement *pElement = p->m_pFirstReliableEl;
    if (p->m_bOngoingSync)
    {
        unsigned long ulDeltaMs = ulTimeNowMs - p->m_ulSyncTimestamp;
        if (ulDeltaMs >= RELIABLE_RETRY_TIMEOUT_MS)
        {
            // we need to restart sync
            if (p->m_ulRetryCnt > RELIABLE_MAX_RETRY)
            {
                p->m_callback->OnCommunicationError(p->m_callback);
            }
            else
            {
                TransportLayer_SendInternalSeqIdRequest(p);
            }
            lTimerMs = RELIABLE_RETRY_TIMEOUT_MS;
        }
        else
        {
            lTimerMs = RELIABLE_RETRY_TIMEOUT_MS - ulDeltaMs;
        }
    }
    else if (pElement == NULL)
    {
        lTimerMs = -1; // TransportLayer is Idle
    }
    else
    {
        while (pElement != NULL)
        {
            // we simply traverse the list, and if we see that there is a timeout expired, we simply resend the packet. If not, we call the OnTimerSet with next timeout.

            unsigned long ulDeltaMs = ulTimeNowMs - pElement->ulTimestampMs;
            if (ulDeltaMs >= RELIABLE_RETRY_TIMEOUT_MS || pElement->bForceResend)
            {
                if (pElement->ucNumSent == RELIABLE_MAX_RETRY)
                {
                    p->m_callback->OnCommunicationError(p->m_callback);
                }
                else
                {
                    if (p->m_otCurrentStats.ulMaxRetransmitCount < pElement->ucNumSent)
                    {
                        p->m_otCurrentStats.ulMaxRetransmitCount = pElement->ucNumSent;
                    }
                    pElement->ucNumSent++;// inc its count
                    pElement->ulTimestampMs = ulTimeNowMs; // update its timestamp to "eat" the system delays by restarting the timeout
                    /// ****** WARNING *****
                    // Known issue if the seqId that is sent again is the same as the previous one. As this is used to detect different NACKs, that could potentially
                    // create a CommunicationError()
                    // what we could do is send an empty message to the other side, which would slightly increase the bandwidth but save us from that trouble
                    // note that this would not occur very often anyway, so that should not be a problem

                    pElement->ucTxSeqId = TransportLayer_BuildAndSendPacket(p, FLAG_RELIABLE_PACKET, pElement->pucData, pElement->usDataSize, pElement->ucReliableSeqId);
                    PLLOG("TransportLayer_Tick(), Resending(Size %u, SeqId %u, RelSeqId %u, %lu ms, sent %u, %s)\n",
                                                        pElement->usDataSize, pElement->ucTxSeqId, pElement->ucReliableSeqId,
                                                        pElement->ulTimestampMs, pElement->ucNumSent, pElement->bForceResend?"Forced":"Timeout");
                    pElement->bForceResend = false;
                    p->m_otCurrentStats.ulReliableRetransmit++;
                }
            }
            else
            {
                signed long lTimeoutMs = RELIABLE_RETRY_TIMEOUT_MS - ulDeltaMs;
                if (lTimerMs > lTimeoutMs)
                {
                    lTimerMs = lTimeoutMs;
                }
            }

            pElement = pElement->pNext;
        }
    }

    p->m_callback->OnTimerSet(p->m_callback, lTimerMs);
}


unsigned long TransportLayer_GetReliableUsage(const struct sTransportLayer* const p)
{
    return p->m_ulReliableByteCnt;
}

unsigned int TransportLayer_GetReliablePacketInTransitCnt(const struct sTransportLayer* const p)
{
    return (unsigned int) ((p->m_ucTxReliableSeqId - p->m_ucTxLastAckedSeqId)&0xFF)-1;
}

bool TransportLayer_IsReliableAvailable(const struct sTransportLayer* const p)
{
#if TLCUST_ENABLE_RELIABLE_PL
    return TransportLayer_GetReliablePacketInTransitCnt(p) < RELIABLE_MAX_PACKETS;
#else /* TLCUST_ENABLE_RELIABLE_PL */
    return false;
#endif
}

const struct stTransportLayerStats* const TransportLayer_GetStats(const struct sTransportLayer* const p)
{
    return &p->m_otCurrentStats;
}

void TransportLayer_ParseIncomingData(struct sTransportLayer* p,
                const unsigned char *pucData, unsigned short usLen)
{
    unsigned short usIdx=0;

    while (usIdx != usLen)
    {
        unsigned char ucData = pucData[usIdx++];
        p->m_ulByteCntSinceLastValidPacket++;

        if (ucData == XON_CHARACTER || ucData == XOFF_CHARACTER)
        {
            continue;
        }

        switch(p->m_uiParserState)
        {
            case WAIT_FOR_ESC_SOP:
            {
                if (ucData == ESCAPE_CHARACTER)
                {
                    p->m_uiParserState = WAIT_FOR_SOP;
                    p->m_otCurrentStats.ulRxGarbageBytes += (p->m_ulByteCntSinceLastValidPacket -1); // if we had only one byte, then there is no garbage, any extra is garbage
                    p->m_ulByteCntSinceLastValidPacket = 1;
                }
            }
            break;

            case WAIT_FOR_SOP:
            {
                if (ucData == SOP_CHARACTER)
                {
                    p->m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
                    p->m_uiRxLen = 0;
                }
                else
                {
                    if (ucData != ESCAPE_CHARACTER)
                    {
                        p->m_uiParserState = WAIT_FOR_ESC_SOP;
                    }
                    else
                    {
                        p->m_otCurrentStats.ulRxGarbageBytes += 2;
                        p->m_ulByteCntSinceLastValidPacket = 1;
                    }

                }
            }
            break;

            case WAIT_FOR_MESSAGE_COMPLETE:
            {
                if (ucData == ESCAPE_CHARACTER)
                {
                    p->m_uiParserState = WAIT_FOR_EOP;
                }
                else if (p->m_uiRxLen < sizeof(p->m_aucRxMessageBuf))
                {
                    p->m_aucRxMessageBuf[p->m_uiRxLen++] = ucData;
                }
                else
                {
                    p->m_uiParserState = WAIT_FOR_ESC_SOP;
                }
            }
            break;

            case WAIT_FOR_EOP:
            {
                if (ucData == EOP_CHARACTER)
                {
                    if (TransportLayer_PacketReceived(p))
                    {
                        p->m_ulByteCntSinceLastValidPacket = 0; // we had a valid packet, restart the cnt
                    }

                    p->m_uiParserState = WAIT_FOR_ESC_SOP;
                }
                else if (ucData == ESCAPED_ESCAPE_CHARACTER)
                {
                    if (p->m_uiRxLen < _DIM(p->m_aucRxMessageBuf))
                    {
                        p->m_aucRxMessageBuf[p->m_uiRxLen++] = ESCAPE_CHARACTER;
                        p->m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
                    }
                    else
                    {
                        p->m_uiParserState = WAIT_FOR_ESC_SOP;
                    }
                }
                else if (ucData == ESCAPED_XON_CHARACTER)
                {
                    if (p->m_uiRxLen < _DIM(p->m_aucRxMessageBuf))
                    {
                        p->m_aucRxMessageBuf[p->m_uiRxLen++] = XON_CHARACTER;
                        p->m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
                    }
                    else
                    {
                        p->m_uiParserState = WAIT_FOR_ESC_SOP;
                    }
                }
                else if (ucData == ESCAPED_XOFF_CHARACTER)
                {
                    if (p->m_uiRxLen < _DIM(p->m_aucRxMessageBuf))
                    {
                        p->m_aucRxMessageBuf[p->m_uiRxLen++] = XOFF_CHARACTER;
                        p->m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
                    }
                    else
                    {
                        p->m_uiParserState = WAIT_FOR_ESC_SOP;
                    }
                }
                else if (ucData == SOP_CHARACTER)
                {
                    // we probably missed the ESC EOP, but we start receiving a new packet!
                    p->m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
                    // init the parser!
                    p->m_uiRxLen = 0;

                    p->m_otCurrentStats.ulRxGarbageBytes += (p->m_ulByteCntSinceLastValidPacket-2); // if we had only one byte, then there is no garbage, any extra is garbage
                    p->m_ulByteCntSinceLastValidPacket = 2;
                }
                else if (ucData == ESCAPE_CHARACTER)
                {
                     p->m_uiParserState = WAIT_FOR_SOP;
                }
                else
                {
                    p->m_uiParserState = WAIT_FOR_ESC_SOP;
                }
            }
            break;
        }
    }
}


bool TransportLayer_PacketReceived(struct sTransportLayer* p)
{
    struct sCrc8Bits crc;
    Crc8Bits_Crc8Bits(&crc);

    /* minimum is seqId, payload size, flags, and Crc */
    if (p->m_uiRxLen >= 4)
    {
        /* compute CRC */
        /* CRC is not applied on itself, nor on the SeqId */
        unsigned char ucCrc = Crc8Bits_Update(&crc,
                        &p->m_aucRxMessageBuf[1], p->m_uiRxLen-2); 

        /* CRC has its nibble inverted for stronger CRC (as CRC of
         * a packet with itself is always 0, if EoP is not detected,
         * that always reset the CRC)
         */
        ucCrc = ((ucCrc&0x0F)<<4) | ((ucCrc&0xF0)>>4);
        if (ucCrc != p->m_aucRxMessageBuf[p->m_uiRxLen-1]) /* CRC is last */
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    /* passed CRC check */
    {
    unsigned char *pucData = &p->m_aucRxMessageBuf[0];
    unsigned short usLen = p->m_uiRxLen-1;

    unsigned char ucSeqId = *pucData++;             /* usLen--; */
    unsigned short usPayloadSize = *pucData++;      /* usLen--; */
    unsigned short usFlags = *pucData++;            /* usLen--; */

    bool bReliablePacket = false;
    unsigned char ucReliableSeqId=0;

    bool bReliableAckReceived = false;
    unsigned char ucReliableAckSeqId=0;

    bool bReliableNackReceived = false;
    unsigned char ucReliableNackSeqId=0;

    bool bAckReceived = false;

    bool bInternalPacket = false;
    bool bIgnoreSeqId = false;

    unsigned short usAckFlags = 0;
    unsigned int i=0;

    usLen -= 3;

    for (i = 0; i < 16 && usFlags != 0 && usLen > 0; ++i)
    {
        unsigned short usFlagMask = (1 << i);
        unsigned short usFlagBit = usFlags & usFlagMask;
        unsigned char ucFlagDetail = 0;

        if (usFlagBit == 0)
        {
            continue;
        }

        usFlags     &= ~usFlagBit;   /* clear the flag */
        ucFlagDetail = *pucData++;   /* extract flag details */
        --usLen;             

        if (usFlagBit == FLAG_PACKET_ACK) /* acknowledgement */
        {
            /* flag detail contain the acknowledged SeqId */
            unsigned char ucReceivedAckSeqId = ucFlagDetail;

            p->m_ucLastAckSeqId = ucReceivedAckSeqId;
            bAckReceived = true;
        }
        else if (usFlagBit == FLAG_RELIABLE_PACKET)
        {
            /* This is a reliable packet. we need to provide the proper Ack */
            ucReliableSeqId = ucFlagDetail;
            bReliablePacket = true;
        }
        else if (usFlagBit == FLAG_RELIABLE_ACK)
        {
            bReliableAckReceived = true;
            ucReliableAckSeqId = ucFlagDetail;
        }
        else if (usFlagBit == FLAG_RELIABLE_NACK)
        {
            bReliableNackReceived = true;
            ucReliableNackSeqId = ucFlagDetail;
        }
        else if (usFlagBit == FLAG_MSG_LOST)
        {
            /* remote TransportLayer lost had some SeqId jumps */
            p->m_otCurrentStats.ulRemotePacketLost += ucFlagDetail;
        }
        else if (usFlagBit == FLAG_MSG_GARBAGE)
        {
            /* remote TransportLayer detected garbage */
            p->m_otCurrentStats.ulRemoteGarbage += ucFlagDetail;
        }
        else if (usFlagBit == FLAG_SIZE_EXTENDED)
        {
            /* flag detail contains the MSB of the payload size */
            usPayloadSize |= (ucFlagDetail<<8);
        }
        else if (usFlagBit == FLAG_EXTENDED)
        {
            /* the flags are extended, which means that the details
             * contains the MSB of the 16bit flags
             */
            usFlags |= (ucFlagDetail<<8);
        }
        else if (usFlagBit == FLAG_INTERNAL_PACKET)
        {
            /* don't care about details */
            bInternalPacket = true;
        }
        else if (usFlagBit == FLAG_IGNORE_SEQID)
        {
            /* don't care about details */
            bIgnoreSeqId = true;
        }
        else
        {
            /* we did not process the flag, just put it back
             * this is an error, so we can break now, as there is no
             * point in continuing
             */
            usFlags |= usFlagBit;
            break;
        }
    }

    /* if flag is not garbage, entire packet should all be consumed */
    /* remaining length of the buffer should be the payload size */
    if (usFlags == 0 &&
        usPayloadSize == usLen)
    {
        /* we now have a valid packet (at least it passed all our
         * validity checks, so we are going to trust it
         */
        unsigned char ucExpectedTxSeqId = (p->m_ucLastRxSeqId+1)&0xFF;
        if (ucSeqId != ucExpectedTxSeqId
             && !bIgnoreSeqId
             && !p->m_bOngoingSync)
        {
            /* Some packets were lost, jump in the RxSeqId */
            p->m_otCurrentStats.ulRxPacketLost +=
                        ((ucSeqId - ucExpectedTxSeqId)&0xFF);
        }
        p->m_ucLastRxSeqId = ucSeqId; /* increase expected SeqId */

        if (!bAckReceived || usLen > 0 )
        {
            bool bDelayedEnough = (++p->m_ucDelayAckCount > 200);
            ++p->m_otCurrentStats.ulPacketReceived;
            // if this is a payload packet, we need to acknowledge it.
            // But don't clog the wires with power-hungry simple
            // acks unless we have too many (200) outstanding.
            if (bDelayedEnough)
            {
                dprint("Skip averted/%d %s %s\n", __LINE__,
                        (bAckReceived  ) ? "ACK"  : "!ack",
                        (bDelayedEnough) ? "ENUF" : "!enuf");
            usAckFlags |= FLAG_PACKET_ACK;
                p->m_ucDelayAckCount = 0;
            }
            else
            {
                dprint("Skip/%d %s %s %d\n", __LINE__,
                        (bAckReceived  ) ? "ACK"  : "!ack",
                        (bDelayedEnough) ? "ENUF" : "!enuf",
                        p->m_ucDelayAckCount);
            }
        }
        else
        {
            ++p->m_otCurrentStats.ulAckReceived;
        }

        {
        bool bProcessPacket = bReliablePacket || usLen > 0;
        if (bReliablePacket)
        {
            PLLOG("TransportLayer_Received Reliable(Size %u, SeqId %u)\n", usLen, ucReliableSeqId);
            /* if this is a reliable message, we need to make sure
             * we didn't received it before!
             * if we did, the Host probably didn't received the Ack,
             * so let's just send the ack
             * Reliable seqId is not enough, so we also use CRC and
             * Lenght to confirm this was the same message received!
             */
            if (ucReliableSeqId == p->m_ucReliableSeqId
                    && Crc8Bits_GetState(&crc) == p->m_ucReliableCrc
                    && usLen == p->m_usReliableLen)
            {
                /* already received that packet, remote TransportLayer
                 * probably lost the Acknowledgement, send it again
                 */
                usAckFlags |= FLAG_PACKET_ACK | FLAG_RELIABLE_ACK;
                bProcessPacket = false; /* we should not process it again */
            }
            else if (ucReliableSeqId == ((p->m_ucReliableSeqId+1)&0xFF))
            {
                /* this is a valid message, just do nothing but update
                 * the reliable info. the message will be processed below
                 */
                usAckFlags |= FLAG_PACKET_ACK | FLAG_RELIABLE_ACK;
                /* already received that packet, remote TransportLayer
                 * probably lost the Acknowledgement, send it again
                 */
                p->m_usReliableLen   = usLen;
                p->m_ucReliableSeqId = ucReliableSeqId;
                p->m_ucReliableCrc   = Crc8Bits_GetState(&crc);
                p->m_otCurrentStats.ulReliablePacketReceived++;
            }
            else
            {
                /* we received the wrong reliable SeqId */
                usAckFlags |= FLAG_PACKET_ACK | FLAG_RELIABLE_NACK;
                bProcessPacket = false; /* we cannot accept the packet */
            }
        }

        /* we have all the flags we need, time to send the acknowledgement */
        if (usAckFlags != 0)
        {
            if (usAckFlags & FLAG_PACKET_ACK)
            {
                p->m_ucDelayAckCount = 0;
            }
            TransportLayer_SendPacketAck(p, usAckFlags);
        }

        if (bReliableAckReceived)
        {
            TransportLayer_ReceivedReliableAck(p, ucReliableAckSeqId);
        }

        if (bReliableNackReceived)
        {
            TransportLayer_ReceivedReliableNack(p, ucReliableNackSeqId, p->m_ucLastAckSeqId);
        }

        if (bProcessPacket)
        {
            if (bInternalPacket)
            {
                TransportLayer_OnInternalPacket(p, pucData, usLen);
            }
            else
            {
                /* everything good, notify upper layer that we have
                 * the payload of a packet available
                 */
                BbdBridge_OnPacketReceived(p, pucData, usLen);
            }
        }
        return true;
        }
    }
    else
    {
        return false;
    }
    }
}

void TransportLayer_SendPacketAck(struct sTransportLayer* p, unsigned short usAckFlags)
{
    TransportLayer_BuildAndSendPacket(p, usAckFlags,
                NULL /*pucData*/,
                0 /*usDataSize*/,
                0 /*ucReliableTxSeqId*/);
}

#undef LARGE_FRAME_SIZE

void TransportLayer_ReceivedReliableAck(struct sTransportLayer* p,
                unsigned char ucAckedReliableSeqId)
{
#if TLCUST_ENABLE_RELIABLE_PL
#ifdef LARGE_FRAME_SIZE
    unsigned int i;
    unsigned int uiNumToNotify=0;
    struct stCallback aCB[RELIABLE_MAX_PACKETS];
#else
    struct stCallback* pCB;
#endif
    bool bHasAckedId = false;
    struct stReliableElement *pElement = p->m_pFirstReliableEl;

    /* by design, an ack ensure that all packets before the acked Id
     * have been received
     */
    PLLOG("TransportLayer_ReceivedReliableAck(RelSeqId %u)\n",
                ucAckedReliableSeqId);

    /* accumulate the state of all callbacks to call
     * (to avoid reentrant call while we are working on our linked list)
     */
    while (pElement != NULL)
    {
        if (pElement->ucReliableSeqId == ucAckedReliableSeqId)
        {
            bHasAckedId = true;
            break;
        }
        pElement = pElement->pNext;
    }

    /* it is possible that we received an ACK previously when we sent
     * the packet twice and the ACK took too much time
     */
    if (bHasAckedId)
    {
        while (p->m_pFirstReliableEl != NULL)
        {
            /* update last acked seqid */
            p->m_ucTxLastAckedSeqId = p->m_pFirstReliableEl->ucReliableSeqId;
            /* copy the callback structure */
#ifdef LARGE_FRAME_SIZE
            aCB[uiNumToNotify++] = p->m_pFirstReliableEl->sCallback;
#else
            pCB = &p->m_pFirstReliableEl->sCallback;
            if (pCB->cb != NULL)
            {
                /* notify user that the packet was delivered */
                BbdTransportLayer_DispatchReliableAck(p, pCB);
            }
#endif
            PLLOG("TransportLayer_NotifyReliableTransmittedAck(RelSeqId %u)\n",
                        p->m_pFirstReliableEl->ucReliableSeqId);
            {
            struct stReliableElement *pRemoveElement =  p->m_pFirstReliableEl;
            bool bDone = (p->m_pFirstReliableEl->ucReliableSeqId
                          == ucAckedReliableSeqId);

            p->m_ulReliableByteCnt -= p->m_pFirstReliableEl->usDataSize;
            /* just point to the next on the list */
            p->m_pFirstReliableEl   = p->m_pFirstReliableEl->pNext;

            /* we need to free the memory */
            bbd_free(pRemoveElement->pucData);
            bbd_free(pRemoveElement);

            if (bDone) /* found our element, we want to stop the loop */
            {
                break;
            }
            }
        }
    }

    if (p->m_pFirstReliableEl == NULL) /* no more elements */
    {
        p->m_pLastReliableEl = NULL;   /* set the last to NULL */
    }

    p->m_bWait4ReliableAckToProcessNack = false; // we just got a ack

#ifdef LARGE_FRAME_SIZE
    /* It is safe to notify user, as we are done playing with the pointers */
    for (i = 0; i < uiNumToNotify; ++i)
    {
        if (aCB[i].cb != NULL)
        {
            /* notify user that the packet was delivered */
            BbdTransportLayer_DispatchReliableAck(p, aCB + i);
        }
    }
#endif

    TransportLayer_Tick(p);
#else /* TLCUST_ENABLE_RELIABLE_PL */
    ASSERT_RET(0);
#endif
}


void TransportLayer_ReceivedReliableNack(struct sTransportLayer* p,
                unsigned char ucLastReceivedReliableSeqId,
                unsigned char ucAckedSeqId)
{
#if TLCUST_ENABLE_RELIABLE_PL
    unsigned char ucExpectedReliableSeqId = (ucLastReceivedReliableSeqId+1)&0xFF;
#ifdef LARGE_FRAME_SIZE
    unsigned int uiNumToNotify=0;
    struct stCallback aCB[RELIABLE_MAX_PACKETS];
#else
    struct stCallback* pCB;
#endif

    PLLOG("TransportLayer_ReceivedReliableNack(Expected RelSeqId %u, AckedSeqid %u) %s\n", ucExpectedReliableSeqId, ucAckedSeqId, p->m_bWait4ReliableAckToProcessNack?"Ignored":"");

    /* if we get a NACK, then we will ignore all the other one until
     * we get a ACK. Otherwise, we might be re-sending these too many times
     */
    if (p->m_bWait4ReliableAckToProcessNack)
    {
        return;
    }
    p->m_bWait4ReliableAckToProcessNack = true;

    /* Let's accumulate on the state all the callbacks to call
     * (to avoid reentrant call while we are working on our linked list)
     */

    if (p->m_pFirstReliableEl != NULL)
    {
        /* if we receive an NACK, we can assume that we were
         * acked up to that one! For the rest, we will simply timeout
         */
        struct stReliableElement *pElement = p->m_pFirstReliableEl;

        while (pElement != NULL)
        {
            /* to filter the "late" acks out, we make sure that the
             * TxSeqId is matching the one that is acked
             */
            if (pElement->ucTxSeqId == ucAckedSeqId)
            {
                /* we can now pay attention to that NACK. Let's ack all
                 * the messages until the expectedSeqId, and retransmit
                 * the other ones
                 */
                PLLOG("TransportLayer_FoundReliableValidElement(RelSeqId  %u, "
                                "TxSeqId %u)\n",
                                pElement->ucReliableSeqId,
                                pElement->ucTxSeqId);
                while (p->m_pFirstReliableEl != NULL)
                {
                    if (p->m_pFirstReliableEl->ucReliableSeqId
                        == ucExpectedReliableSeqId)
                    {
                        struct stReliableElement *pElement = p->m_pFirstReliableEl;
                        while (pElement != NULL)
                        {
                            /* we will have them resend now */
                            pElement->bForceResend = true;
                            pElement = pElement->pNext;
                        }
                        break;
                    }
                    else
                    {
                        p->m_ucTxLastAckedSeqId =
                        p->m_pFirstReliableEl->ucReliableSeqId;

                        /* update last acked seqid */
                        /* copy the callback structure */

#ifdef LARGE_FRAME_SIZE
                        aCB[uiNumToNotify++] = p->m_pFirstReliableEl->sCallback;
#else
                        pCB = &p->m_pFirstReliableEl->sCallback;
                        if (pCB->cb != NULL)
                        {
                            /* notify user that the packet was delivered */
                            BbdTransportLayer_DispatchReliableAck(p, pCB);
                        }
#endif

                        PLLOG("TransportLayer_NotifyReliableTransmittedAck(RelSeqId  %u)\n", p->m_pFirstReliableEl->ucReliableSeqId);
                        p->m_ulReliableByteCnt -=
                                p->m_pFirstReliableEl->usDataSize;

                        {
                        struct stReliableElement *pRemoveElement =
                                p->m_pFirstReliableEl;
                        p->m_pFirstReliableEl = p->m_pFirstReliableEl->pNext; // just point to the next on the list

                        // we need to free the memory
                        bbd_free(pRemoveElement->pucData);
                        bbd_free(pRemoveElement);
                        }
                    }
                }
                /* we are done, let's exit the loop */
                break;
            }
            else
            {
                pElement = pElement->pNext;
            }
        }
    }

#ifdef LARGE_FRAME_SIZE
    // It is safe to notify user, as we are done playing with the pointers
    {
    unsigned int i = 0;
    for (i = 0; i < uiNumToNotify; ++i)
    {
        if (aCB[i].cb)
        {
            /* notify user that the packet was delivered */
            BbdTransportLayer_DispatchReliableAck(p, aCB + i);
        }
    }
    }
#endif

    /* we have changed the reliable list, so we need to update the timers */
    TransportLayer_Tick(p);

#else /* TLCUST_ENABLE_RELIABLE_PL */
    ASSERT_RET(0);
#endif
}

/* we have an internal packet. */

void TransportLayer_OnInternalPacket(struct sTransportLayer* p,
                unsigned char *pucData, unsigned short usSize)
{
    unsigned char ucId = *pucData++;
    ASSERT_RET(usSize > 0);

    --usSize;

    switch (ucId)
    {
        case INT_PKT_SEQID_REQUEST:
        {
            unsigned char ucReqId = *pucData++;
            --usSize;
            ASSERT_RET(usSize == 0);
            TransportLayer_OnInternalSeqIdRequest(p, ucReqId);
        }
        break;

        case INT_PKT_SEQID_RESPONSE:
        {
            struct sStreamDecoder str;
            StreamDecoder_StreamDecoder(&str, pucData, usSize);
            {
            unsigned char ucReqId = StreamDecoder_GetU08(&str);
            unsigned char ucLastRxSeqId = StreamDecoder_GetU08(&str);
            unsigned char ucLastAckSeqId = StreamDecoder_GetU08(&str);
            unsigned char ucReliableSeqId = StreamDecoder_GetU08(&str);
            unsigned char ucTxSeqId = StreamDecoder_GetU08(&str);
            unsigned char ucTxReliableSeqId = StreamDecoder_GetU08(&str);
            unsigned char ucTxLastAckedSeqId = StreamDecoder_GetU08(&str);

            ASSERT_RET(!   StreamCodec_Fail(&str)
                        && StreamCodec_GetAvailableSize(&str) == 0);
            TransportLayer_OnInternalSeqIdResponse(p, ucReqId, ucLastRxSeqId,
                                    ucLastAckSeqId, ucReliableSeqId,
                                    ucTxSeqId, ucTxReliableSeqId,
                                    ucTxLastAckedSeqId);
            }
        }
        break;

        case INT_PKT_STATS_REQUEST:
        {
            ASSERT_RET(usSize == 0);
            TransportLayer_OnInternalStatsRequest(p);
        }
        break;

        case INT_PKT_STATS_RESPONSE:
        {
            struct sStreamDecoder str;
            struct stTransportLayerStats otStats;
            StreamDecoder_StreamDecoder(&str, pucData, usSize);

            otStats.ulRxGarbageBytes = StreamDecoder_GetU32(&str);
            otStats.ulRxPacketLost = StreamDecoder_GetU32(&str);  
            otStats.ulRemotePacketLost = StreamDecoder_GetU32(&str);
            otStats.ulRemoteGarbage = StreamDecoder_GetU32(&str);   
            otStats.ulPacketSent = StreamDecoder_GetU32(&str);
            otStats.ulPacketReceived = StreamDecoder_GetU32(&str);
            otStats.ulAckReceived = StreamDecoder_GetU32(&str);
            otStats.ulReliablePacketSent = StreamDecoder_GetU32(&str); 
            otStats.ulReliableRetransmit = StreamDecoder_GetU32(&str);
            otStats.ulReliablePacketReceived = StreamDecoder_GetU32(&str);
            otStats.ulMaxRetransmitCount = StreamDecoder_GetU32(&str);

            ASSERT_RET(!   StreamCodec_Fail(&str)
                        && StreamCodec_GetAvailableSize(&str) == 0);
            TransportLayer_OnInternalStatsResponse(p, &otStats);
        }
        break;

        case INT_PKT_SETTINGS_REQUEST:
        {
            ASSERT_RET(usSize == 0);
            TransportLayer_OnInternalSettingsRequest(p);
        }
        break;

        case INT_PKT_SETTINGS_RESPONSE:
        {
            struct sStreamDecoder str;
            unsigned short usMaxOutgoingPacketSize;
            unsigned short usMaxIncomingPacketSize;
            unsigned short usReliableRetryTimeoutMs;
            unsigned short usReliableMaxRetry;
            unsigned short usReliableMaxPackets;

            StreamDecoder_StreamDecoder(&str, pucData, usSize);
            usMaxOutgoingPacketSize  = StreamDecoder_GetU16(&str);
            usMaxIncomingPacketSize  = StreamDecoder_GetU16(&str);
            usReliableRetryTimeoutMs = StreamDecoder_GetU16(&str);
            usReliableMaxRetry       = StreamDecoder_GetU16(&str);
            usReliableMaxPackets     = StreamDecoder_GetU16(&str);

            ASSERT_RET(!   StreamCodec_Fail(&str)
                        && StreamCodec_GetAvailableSize(&str) == 0);
            TransportLayer_OnInternalSettingsResponse(p,
                        usMaxOutgoingPacketSize,
                        usMaxIncomingPacketSize,
                        usReliableRetryTimeoutMs, 
                        usReliableMaxRetry,
                        usReliableMaxPackets);
        }
        break;

        default:
            break;
    }
}

static unsigned char _ucBuf[MAX_OUTGOING_PACKET_SIZE];
void TransportLayer_SendInternalPacket(struct sTransportLayer* p,
                unsigned char ucId,
                const unsigned char *pucData,
                unsigned short usSize)
{
    unsigned short usFlags = FLAG_INTERNAL_PACKET;

    /* Yes we could have avoided a memcpy if the upper function
     * were to encode ucId into the buffer already,
     * but code readability is the preffered path, especially
     * as the internal packet are rarely used, and little data is exchanged.
     */
    _ucBuf[0] = ucId;
    ASSERT_RET(usSize < sizeof(_ucBuf));
    memcpy(&_ucBuf[1], pucData, usSize);

    TransportLayer_BuildAndSendPacket(p, usFlags, _ucBuf,
                        usSize+1, 0/*ucReliableTxSeqId*/);
}


void TransportLayer_OnInternalSeqIdRequest(struct sTransportLayer* p,
                unsigned char ucReqId)
{
        /* ensure proper packet */
        unsigned char ucBuf[7];
        struct sStreamEncoder otStream;

        StreamEncoder_StreamEncoder(&otStream, ucBuf, sizeof(ucBuf));
        StreamEncoder_PutU08(&otStream, ucReqId);
        StreamEncoder_PutU08(&otStream, p->m_ucLastRxSeqId);
        StreamEncoder_PutU08(&otStream, p->m_ucLastAckSeqId);
        StreamEncoder_PutU08(&otStream, p->m_ucReliableSeqId);
        StreamEncoder_PutU08(&otStream, p->m_ucTxSeqId);
        StreamEncoder_PutU08(&otStream, p->m_ucTxReliableSeqId);
        StreamEncoder_PutU08(&otStream, p->m_ucTxLastAckedSeqId);

        ASSERT_RET(!StreamCodec_Fail(&otStream));
        TransportLayer_SendInternalPacket(p,
                        INT_PKT_SEQID_RESPONSE,
                        StreamCodec_GetStreamBuffer(&otStream),
                        StreamCodec_GetOffset(&otStream));
}

void TransportLayer_OnInternalSeqIdResponse(struct sTransportLayer* p,
                unsigned char ucReqId,
                unsigned char ucLastRxSeqId,
                unsigned char ucLastAckSeqId,
                unsigned char ucReliableSeqId,
                unsigned char ucTxSeqId,
                unsigned char ucTxReliableSeqId,
                unsigned char ucTxLastAckedSeqId)
{
    /* is there ongoing rquest, and is the reqId matching? */
//hoi changed    if (p->m_bOngoingSync && ucReqId == p->m_ulRetryCnt)
    if (p->m_bOngoingSync)
    {
        /* Great, let's set our seqId appropriately, so we can
         * start communicating with the remote side, and not
         * have any garbage detected
         * +1 because the next TxSeqId is expected to be one
         * more than last received.
         * other +1 because this response triggered an ACK to
         * be sent to the remote party, so it indeed already have
         * incremented its seqId
         */
        p->m_ucTxSeqId          = (ucLastRxSeqId+1+1)&0xFF; 
        p->m_ucTxReliableSeqId  = (ucReliableSeqId+1)&0xFF;
        /* last acked should be -1 */
        p->m_ucTxLastAckedSeqId = (p->m_ucTxReliableSeqId-1)&0xFF;
        p->m_ucLastRxSeqId = (ucTxSeqId)&0xFF;
        p->m_ucReliableSeqId = (ucTxReliableSeqId-1)&0xFF;

        /* let's clean the Reliable state */
        p->m_ucReliableCrc = 0;
        p->m_usReliableLen = 0;

        p->m_bOngoingSync = false;
        TransportLayer_Tick(p); /* refresh internal timers */
        p->m_callback->OnRemoteSyncComplete(p->m_callback);
    }
}


void TransportLayer_OnInternalStatsRequest(struct sTransportLayer* p)
{
    unsigned char ucBuf[11*4];
    struct sStreamEncoder str;
    StreamEncoder_StreamEncoder(&str, ucBuf, sizeof(ucBuf));
    
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulRxGarbageBytes);
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulRxPacketLost);  
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulRemotePacketLost);
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulRemoteGarbage);   
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulPacketSent);
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulPacketReceived);
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulAckReceived);
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulReliablePacketSent); 
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulReliableRetransmit);
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulReliablePacketReceived);
    StreamEncoder_PutU32(&str, p->m_otCurrentStats.ulMaxRetransmitCount);

    ASSERT_RET(!StreamCodec_Fail(&str));

    TransportLayer_SendInternalPacket(p,
                        INT_PKT_STATS_RESPONSE,
                        StreamCodec_GetStreamBuffer(&str),
                        StreamCodec_GetOffset      (&str));
}

void TransportLayer_OnInternalStatsResponse(struct sTransportLayer* p,
                struct stTransportLayerStats* rStats)
{
    /* what do we want to do with it?
     * Save it locally for access via sysfs
     */
}

void TransportLayer_OnInternalSettingsRequest(struct sTransportLayer* p)
{
    unsigned char ucBuf[5*2];
    struct sStreamEncoder str;
    StreamEncoder_StreamEncoder(&str, ucBuf, sizeof(ucBuf));
    
    StreamEncoder_PutU16(&str, MAX_OUTGOING_PACKET_SIZE);
    StreamEncoder_PutU16(&str, MAX_INCOMING_PACKET_SIZE);
    StreamEncoder_PutU16(&str, RELIABLE_RETRY_TIMEOUT_MS);
    StreamEncoder_PutU16(&str, RELIABLE_MAX_RETRY);
    StreamEncoder_PutU16(&str, RELIABLE_MAX_PACKETS);

    ASSERT_RET(!StreamCodec_Fail(&str));

    TransportLayer_SendInternalPacket(p,
                        INT_PKT_SETTINGS_RESPONSE,
                        StreamCodec_GetStreamBuffer(&str),
                        StreamCodec_GetOffset      (&str));
}

void  TransportLayer_OnInternalSettingsResponse(struct sTransportLayer* p,
                        unsigned short usMaxOutgoingPacketSize,
                        unsigned short usMaxIncomingPacketSize,
                        unsigned short usReliableRetryTimeoutMs, 
                        unsigned short usReliableMaxRetry,
                        unsigned short usReliableMaxPackets)
{
    /* what do we want to do with it?
     * Save it locally for access via sysfs
     */
}
