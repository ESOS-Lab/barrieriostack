/******************************************************************************* 
 ** \file transport_layer_custom.h  customization file for the TransportLayer
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

#ifndef TRANSPORT_LAYER_CUSTOM_H
#define TRANSPORT_LAYER_CUSTOM_H

/*
******************* CUSTOMIZATION FILE ********************
* This file can be edited for your project, and you can customize your transport layer by changing some of the default value
* Note that you can also define these value from your makefile directly
*/

/// if defined to 1, the reliable channel will be enabled. If defined to 0, it will be disabled and compiled out
#ifndef TLCUST_ENABLE_RELIABLE_PL
    #define TLCUST_ENABLE_RELIABLE_PL 1
#endif

// Defines the max size of outgoing packet. That should match the max size of incoming packet from the remote TL
#ifndef TLCUST_MAX_OUTGOING_PACKET_SIZE
    #define TLCUST_MAX_OUTGOING_PACKET_SIZE 2048
#endif

// Defines the max size of incoming packet. That should match the max size of outgoing packet from the remote TL
#ifndef TLCUST_MAX_INCOMING_PACKET_SIZE
    #define TLCUST_MAX_INCOMING_PACKET_SIZE 2048
#endif

// Defines the number of millisecond before retrying a reliable packet when no acknowledgement is received.
#ifndef TLCUST_RELIABLE_RETRY_TIMEOUT_MS
    #define TLCUST_RELIABLE_RETRY_TIMEOUT_MS 1000
#endif

// Defines the number of retries before declaring a communication error
#ifndef TLCUST_RELIABLE_MAX_RETRY
    #define TLCUST_RELIABLE_MAX_RETRY 10
#endif


// Defines the maximum number of reliable packets that can be in transit(MAX is 255)
#ifndef TLCUST_RELIABLE_MAX_PACKETS
    #define TLCUST_RELIABLE_MAX_PACKETS 150
#endif

#endif /* TRANSPORT_LAYER_CUSTOM_H */

