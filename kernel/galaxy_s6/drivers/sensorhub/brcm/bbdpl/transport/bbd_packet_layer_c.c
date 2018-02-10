/****************************************************************************** 
 ** \file bbd_packet_layer_c.c  TransportLayer inside the BBD
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

#include "bbd_packet_layer_c.h"
#include <linux/string.h>

struct sBbdReliableTransaction* BbdReliableTransaction_init(struct sBbdReliableTransaction* p)
{
    memset(p, 0, sizeof(*p));
    return p;
}

int bbd_tty_autobaud(void);

bool BbdTransportLayer_SetControlMessage(struct sTransportLayer* p,
                char* pcMessage)
{
    if (strstr(pcMessage, BBD_CTRL_PASSTHRU_ON))
    {
            p->m_bPassThrough = true;
            return true;
    }
    if (strstr(pcMessage, BBD_CTRL_PASSTHRU_OFF))
    {
            p->m_bPassThrough = false;
            return true;
    }
    if (strstr(pcMessage, BBD_CTRL_PL_START_REMOTE_SYNC))
    {
            TransportLayer_StartRemoteSync(p);
            return true;
    }
    return false;
}

/* The BbdTransportLayer does reliable packet transfers via
 * the BbdReliableTransaction structure.
 */

bool BbdTransportLayer_SendReliablePacket(struct sTransportLayer* p,
                        struct sBbdReliableTransaction* trans)
{
    if (BbdTransportLayer_IsPassThrough(p))
    {
        p->m_callback->OnException(p->m_callback, __FILE__, __LINE__);
        return false;
    }
    return TransportLayer_SendReliablePacket(p, trans->pucData, trans->usSize,
                                         trans->cb, trans->pCbData, trans->ulCbData);
}

/* Send the reliable ack back to the user layer as we cannot call
 * the BBD directly.
 */

void BbdTransportLayer_DispatchReliableAck(struct sTransportLayer* p,
                        struct stCallback *resp)
{
    struct sBbdReliableTransaction trans;
    if (BbdTransportLayer_IsPassThrough(p))
    {
        p->m_callback->OnException(p->m_callback, __FILE__, __LINE__);
        return;
    }
    BbdReliableTransaction_init(&trans);
    trans.cb       = resp->cb;
    trans.pCbData  = resp->pCbData;
    trans.ulCbData = resp->ulCbData;
    
    /* notify user the packet was delivered */
    p->m_callback->OnReliableAck(p->m_callback, &trans);
}

void BbdTransportLayer_SetData(struct sTransportLayer* p,
                    unsigned char* pucData, unsigned short usSize)
{
    TransportLayer_SetData(p, pucData, usSize);
}

bool BbdTransportLayer_IsPassThrough(const struct sTransportLayer * const p)
{
        return p->m_bPassThrough;
}

void BbdTransportLayer_SetPassThrough(struct sTransportLayer *p,
                bool bPassThrough)
{
        p->m_bPassThrough = bPassThrough;
}

/* Send a packet to the remote TransportLayer. */

void BbdTransportLayer_SendPacket(struct sTransportLayer *p,
                unsigned char *pucData, unsigned short usSize)
{
    if (BbdTransportLayer_IsPassThrough(p))
    {
        p->m_callback->OnDataToSend(p->m_callback, pucData, usSize);
    }
    else
    {
        TransportLayer_SendPacket(p, pucData, usSize);
    }
}

