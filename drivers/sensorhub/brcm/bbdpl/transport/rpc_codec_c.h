/****************************************************************************** 
 * \file rpc_codec_c.h  RPC Codec class declaration
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

#ifndef RPC_CODEC_C_H
#define RPC_CODEC_C_H


/* Enumeration of all the RPCs for the codec. DO NOT CHANGE THE ORDER,
 * DELETE, or INSERT anything to keep backward compatibility.
 */
#define RPC_DEFINITION(klass, method) RPC_##klass##_##method
enum
{
     RPC_DEFINITION(IRpcA, A)
    ,RPC_DEFINITION(IRpcA, B)
    ,RPC_DEFINITION(IRpcA, C)

    ,RPC_DEFINITION(IRpcB, A)
    ,RPC_DEFINITION(IRpcB, B)

    ,RPC_DEFINITION(IRpcC, A)
    ,RPC_DEFINITION(IRpcC, B)
    ,RPC_DEFINITION(IRpcC, C)

    ,RPC_DEFINITION(IRpcD, A)

    ,RPC_DEFINITION(IRpcE, A)
    ,RPC_DEFINITION(IRpcE, B)

    ,RPC_DEFINITION(IRpcF, A)
    ,RPC_DEFINITION(IRpcF, B)
    ,RPC_DEFINITION(IRpcF, C)

    ,RPC_DEFINITION(IRpcG, A)
    ,RPC_DEFINITION(IRpcG, B)
    ,RPC_DEFINITION(IRpcG, C)
    ,RPC_DEFINITION(IRpcG, D)
    ,RPC_DEFINITION(IRpcG, E)

    ,RPC_DEFINITION(IRpcH, A)
    ,RPC_DEFINITION(IRpcH, B)
    ,RPC_DEFINITION(IRpcH, C)
    ,RPC_DEFINITION(IRpcH, D)

    ,RPC_DEFINITION(IRpcI, A)
    ,RPC_DEFINITION(IRpcI, B)
    ,RPC_DEFINITION(IRpcI, C)

    ,RPC_DEFINITION(IRpcJ, A)
    ,RPC_DEFINITION(IRpcJ, B)

    ,RPC_DEFINITION(IRpcK, A)
    ,RPC_DEFINITION(IRpcK, B)
    ,RPC_DEFINITION(IRpcK, C)

    ,RPC_DEFINITION(IRpcL, A)

    ,RPC_DEFINITION(IRpcSensorRequest,  Data)
    ,RPC_DEFINITION(IRpcSensorResponse, Data)
};

#endif /* RPC_CODEC_C_H */
