/****************************************************************************** 
 ** \file bbd_engine.h  Engine to control the BBD bridge
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

#ifndef BBD_ENGINE_H_ /* { */
#define BBD_ENGINE_H_

#include "bbd_bridge_c.h"
#include <linux/mutex.h>

//----------------------------------------------------------------------------
//
//      BBD Engine
//
//  Thin wrapper for the BbdBridge class to connect it to the bbd device driver.
//
//----------------------------------------------------------------------------

struct BbdEngine
{
    struct BbdBridge   bridge;
    struct mutex       lock;
    struct sRpcDecoder m_rpcSensor;
    unsigned char      m_aucTransactionPayload[MAX_OUTGOING_PACKET_SIZE];
    unsigned long      m_uiTransactionPayloadSize;
    struct sITransportLayerCb callback;
    bool                      tickWorking;
    bool                      constructed;
    bool                      open;
};

struct BbdEngine* BbdEngine_BbdEngine(struct BbdEngine* p);

bool BbdEngine_OnControlMessageToSend(void* p, const char* pcMsg);
void BbdEngine_dtor(struct BbdEngine* p);
ssize_t BbdEngine_SendSensorData(struct BbdEngine* p, unsigned char *pbuff, size_t size);
void BbdEngine_SetUp(struct BbdEngine *p);
unsigned long BbdEngine_OnTimer(void);
bool BbdEngine_Lock  (int from);
void BbdEngine_Unlock(void);

#endif // } BBD_ENGINE_H_
