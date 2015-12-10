/****************************************************************************** 
 ** \file bbd_packet_layer_c.h  Bridge (or mini-RpcEngine) inside the BBD
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

#ifndef BBD_PACKET_LAYER_C_H  // {
#define BBD_PACKET_LAYER_C_H

#include "transport_layer_c.h"
#include "../bbd_ifc.h"

#ifdef __cplusplus
extern "C" {
#endif  // } defined __cplusplus


/* --------------------  BBD-specific methods ------------------- */

bool BbdTransportLayer_IsPassThrough(const struct sTransportLayer * const p);
void BbdTransportLayer_SetPassThrough(struct sTransportLayer *p,
                bool bPassThrough);

/* Send a packet to the remote TransportLayer. */
void BbdTransportLayer_SendPacket(struct sTransportLayer *p,
                        unsigned char *pucData, unsigned short usSize);

/* kernel-space reliable packet API */
bool BbdTransportLayer_SendReliablePacket(struct sTransportLayer *p, struct sBbdReliableTransaction* trans);

void BbdTransportLayer_SetData(struct sTransportLayer *p,
                        unsigned char* pucData, unsigned short usSize);

bool BbdTransportLayer_SetControlMessage(struct sTransportLayer *p,
                        char* pcMessage);

void BbdTransportLayer_DispatchReliableAck(struct sTransportLayer *p,
                        struct stCallback *resp);


#ifdef __cplusplus
                        }
#endif  // } defined __cplusplus

#endif  // } BBD_PACKET_LAYER_H
