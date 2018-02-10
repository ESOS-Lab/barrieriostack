/*
   Copyright (C) 2003-2007 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signaldata/TcKeyConf.hpp>
#include <BlockNumbers.h>

bool
printTCKEYCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  
  if (receiverBlockNo == API_PACKED) {
    return false;
    Uint32 Theader = * theData++;
    Uint32 TpacketLen = (Theader & 0x1F) + 3;
    Uint32 TrecBlockNo = Theader >> 16;
    
    do {
      fprintf(output, "Block: %d %d %d\n", TrecBlockNo, len, TpacketLen);
      printTCKEYCONF(output, theData, TpacketLen, TrecBlockNo);
      assert(len >= (1 + TpacketLen));
      len -= (1 + TpacketLen);
      theData += TpacketLen;
    } while(len);
    return true;
  }
  else {
    const TcKeyConf * const sig = (TcKeyConf *) theData;
    
    Uint32 i = 0;
    Uint32 confInfo = sig->confInfo;
    Uint32 noOfOp = TcKeyConf::getNoOfOperations(confInfo);
    if (noOfOp > 10) noOfOp = 10;
    fprintf(output, " apiConnectPtr: H'%.8x, gci: %u/%u, transId:(H'%.8x, H'%.8x)\n",
	    sig->apiConnectPtr,
            sig->gci_hi, *(Uint32*)&sig->operations[noOfOp],
            sig->transId1, sig->transId2);
    
    fprintf(output, " noOfOperations: %u, commitFlag: %s, markerFlag: %s\n", 
	    noOfOp,
	  (TcKeyConf::getCommitFlag(confInfo) == 0)?"false":"true",
	    (TcKeyConf::getMarkerFlag(confInfo) == 0)?"false":"true");
    fprintf(output, "Operations:\n");
    for(i = 0; i < noOfOp; i++) {
      if(sig->operations[i].attrInfoLen > TcKeyConf::DirtyReadBit)
	fprintf(output,
		" apiOperationPtr: H'%.8x, simplereadnode: %u\n",
		sig->operations[i].apiOperationPtr,
		sig->operations[i].attrInfoLen & (~TcKeyConf::DirtyReadBit));
      else
	fprintf(output,
		" apiOperationPtr: H'%.8x, attrInfoLen: %u\n",
		sig->operations[i].apiOperationPtr,
		sig->operations[i].attrInfoLen);
    }
  }
  
  return true;
}
