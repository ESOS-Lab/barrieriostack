/*
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
/*******************************************************************************/
/** \file bbd_ifc.h Interface between the BBD and Rpc code in user space
*
*******************************************************************************/

#ifndef BBD_IFC_H  /* { */
#define BBD_IFC_H

#define BBD_ESW_CTRL_MAX_STR_LEN	100

/* BBD control (LHD <--- BBD <--- SHMD*/
#define BBD_CTRL_RESET_REQ	"BBD:RESET_REQ"	/* SHMD requests ESW reset */
#define BBD_CTRL_TICK	        "BBD:tick"	/* timer tick inside the bridge */

/* ESW status (LHD ---> BBD */
#define ESW_CTRL_READY		"ESW:READY"
#define ESW_CTRL_NOTREADY	"ESW:NOTREADY"
#define ESW_CTRL_CRASHED	"ESW:CRASHED"

#define BBD_CTRL_PASSTHRU_ON    "BBD:PassThru=1"
#define BBD_CTRL_PASSTHRU_OFF   "BBD:PassThru=0"
#define BBD_CTRL_SHMD_ON        "BBD:SHMD=1"
#define BBD_CTRL_SHMD_OFF       "BBD:SHMD=0"
#define BBD_CTRL_DEBUG_ON       "BBD:DEBUG=1"
#define BBD_CTRL_DEBUG_OFF      "BBD:DEBUG=0"

#define SSP_DEBUG_ON		"SSP:DEBUG=1"
#define SSP_DEBUG_OFF		"SSP:DEBUG=0"
#define SSI_DEBUG_ON		"SSI:DEBUG=1"
#define SSI_DEBUG_OFF		"SSI:DEBUG=0"

#define BBD_CTRL_SPI            "BBD:Serial=SPI"
#define BBD_CTRL_TTY            "BBD:Serial=TTY"
#define BBD_CTRL_SIO            "BBD:Serial=SIO"

#define SHMD_RESET_MCU		"SHMD:RESET_MCU"

/* Packet Layer handshake LHD <---> BBD */
#define BBD_CTRL_PL_START_REMOTE_SYNC      "PL:StartRemoteSync"
#define BBD_CTRL_PL_REMOTE_SYNC_COMPLETE   "PL:RemoteSyncComplete"
#define BBD_CTRL_PL_ON_RELIABLE_ERROR      "PL:OnReliableError"
#define BBD_CTRL_PL_ON_EXCEPTION           "PL:OnException:"
#define BBD_CTRL_PL_ON_COMMUNICATION_ERROR "PL:OnCommunicationError"


#endif /* } BBD_IFC_H */
