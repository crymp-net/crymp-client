#include "StdAfx.h"
#include "CryCommon/CryCore/crc32.h"



Crc32Gen::Crc32Gen()
{
	init_CRC32_Table();
}

// Call this function only once to initialize the CRC table.
void Crc32Gen::init_CRC32_Table()
{
	// This is the official polynomial used by CRC-32
	// in PKZip, WinZip and Ethernet.
	unsigned int ulPolynomial = 0x04c11db7;
	
	// 256 values representing ASCII character codes.
	for(int i = 0; i <= 0xFF; i++)
	{
		crc32_table[i] = reflect(i, 8) << 24;
		for (int j = 0; j < 8; j++)
			crc32_table[i] = (crc32_table[i] << 1) ^ (crc32_table[i] & (1 << 31) ? ulPolynomial : 0);
		crc32_table[i] = reflect(crc32_table[i], 32);
	}
}

unsigned int Crc32Gen::reflect(unsigned int ref, char ch)
{// Used only by Init_CRC32_Table().
	unsigned int value = 0;
	
	// Swap bit 0 for bit 7
	// bit 1 for bit 6, etc.
	for(int i = 1; i < (ch + 1); i++)
	{
		if(ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}

