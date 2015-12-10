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

#ifndef CRC_8BITS_C_H
#define CRC_8BITS_C_H

struct sCrc8Bits
{
    unsigned char m_ucCrcState;
};

void Crc8Bits_Crc8Bits(struct sCrc8Bits *p);

void Crc8Bits_Reset(struct sCrc8Bits *p); // Reset the CRC

// Update CRC with array of bytes
unsigned char Crc8Bits_Update(struct sCrc8Bits *p, const unsigned char *pacData, unsigned long ulLen);

// Update CRC with a single byte
unsigned char Crc8Bits_UpdateByte(struct sCrc8Bits *p, unsigned char ucData);

// Get the CRC
unsigned char Crc8Bits_GetState(const struct sCrc8Bits *p);

// C function to compute the CRC of a memory region
unsigned char cGet8bitCrc(const unsigned char *pacData, unsigned long ulLen);

#endif  /* CRC_8BITS_C_H */
