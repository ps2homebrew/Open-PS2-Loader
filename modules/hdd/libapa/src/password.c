/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Password-handling routines
*/

#include <errno.h>
#include <iomanX.h>
#include <atad.h>
#include <sysclib.h>
#include <stdio.h>
#include <hdd-ioctl.h>

#include "apa-opt.h"
#include "libapa.h"

int apaPassCmp(const char *pw1, const char *pw2)
{
#ifdef APA_ENABLE_PASSWORDS
	 return memcmp(pw1, pw2, APA_PASSMAX) ? -EACCES : 0;
#else
	//Passwords are not supported, hence this check should always pass.
	return 0;
#endif
}

static void DESEncryptPassword(u32 id_lo, u32 id_hi, char *password_out, const char *password);

void apaEncryptPassword(const char *id, char *password_out, const char *password_in)
{
	char password[APA_PASSMAX];
	memcpy(password, password_in, APA_PASSMAX);
	DESEncryptPassword(*(u32*)(id), *(u32*)(id + 4), password_out, password);
}

struct KeyPair{
	u32 lo, hi;
};

//This is a standard DES-ECB implementation. It encrypts the partition ID with the password.
static void DESEncryptPassword(u32 id_lo, u32 id_hi, char *password_out, const char *password)
{
						//Left
	static const u8 PC1[]={			0x39, 0x31, 0x29, 0x21, 0x19, 0x11, 0x09,
						0x01, 0x3a, 0x32, 0x2a, 0x22, 0x1a, 0x12,
						0x0a, 0x02, 0x3b, 0x33, 0x2b, 0x23, 0x1b,
						0x13, 0x0b, 0x03, 0x3c, 0x34, 0x2c, 0x24,
						//Right
						0x3f, 0x37, 0x2f, 0x27, 0x1f, 0x17, 0x0f,
						0x07, 0x3e, 0x36, 0x2e, 0x26, 0x1e, 0x16,
						0x0e, 0x06, 0x3d, 0x35, 0x2d, 0x25, 0x1d,
						0x15, 0x0d, 0x05, 0x1c, 0x14, 0x0c, 0x04 };
	static const u8 Rotations[]={		1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1 };
	static const u8 PC2[]={			0x0e, 0x11, 0x0b, 0x18, 0x01, 0x05,
						0x03, 0x1c, 0x0f, 0x06, 0x15, 0x0a,
						0x17, 0x13, 0x0c, 0x04, 0x1a, 0x08,
						0x10, 0x07, 0x1b, 0x14, 0x0d, 0x02,
						0x29, 0x34, 0x1f, 0x25, 0x2f, 0x37,
						0x1e, 0x28, 0x33, 0x2d, 0x21, 0x30,
						0x2c, 0x31, 0x27, 0x38, 0x22, 0x35,
						0x2e, 0x2a, 0x32, 0x24, 0x1d, 0x20 };
	static const u8 IP[]={			0x3a, 0x32, 0x2a, 0x22, 0x1a, 0x12, 0x0a, 0x02, 0x3c, 0x34, 0x2c, 0x24, 0x1c, 0x14, 0x0c, 0x04,
						0x3e, 0x36, 0x2e, 0x26, 0x1e, 0x16, 0x0e, 0x06, 0x40, 0x38, 0x30, 0x28, 0x20, 0x18, 0x10, 0x08,

						0x39, 0x31, 0x29, 0x21, 0x19, 0x11, 0x09, 0x01, 0x3b, 0x33, 0x2b, 0x23, 0x1b, 0x13, 0x0b, 0x03,
						0x3d, 0x35, 0x2d, 0x25, 0x1d, 0x15, 0x0d, 0x05, 0x3f, 0x37, 0x2f, 0x27, 0x1f, 0x17, 0x0f, 0x07 };
	static const u8 Expansion[]={		0x20, 0x01, 0x02, 0x03, 0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b,
						0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x14, 0x15,
						0x16, 0x17, 0x18, 0x19, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x01 };
	static const u8 sbox[][64]={	{	0x0e, 0x04, 0x0d, 0x01, 0x02, 0x0f, 0x0b, 0x08, 0x03, 0x0a, 0x06, 0x0c, 0x05, 0x09, 0x00, 0x07,
						0x00, 0x0f, 0x07, 0x04, 0x0e, 0x02, 0x0d, 0x01, 0x0a, 0x06, 0x0c, 0x0b, 0x09, 0x05, 0x03, 0x08,
						0x04, 0x01, 0x0e, 0x08, 0x0d, 0x06, 0x02, 0x0b, 0x0f, 0x0c, 0x09, 0x07, 0x03, 0x0a, 0x05, 0x00,
						0x0f, 0x0c, 0x08, 0x02, 0x04, 0x09, 0x01, 0x07, 0x05, 0x0b, 0x03, 0x0e, 0x0a, 0x00, 0x06, 0x0d	},
					{	0x0f, 0x01, 0x08, 0x0e, 0x06, 0x0b, 0x03, 0x04, 0x09, 0x07, 0x02, 0x0d, 0x0c, 0x00, 0x05, 0x0a,
						0x03, 0x0d, 0x04, 0x07, 0x0f, 0x02, 0x08, 0x0e, 0x0c, 0x00, 0x01, 0x0a, 0x06, 0x09, 0x0b, 0x05,
						0x00, 0x0e, 0x07, 0x0b, 0x0a, 0x04, 0x0d, 0x01, 0x05, 0x08, 0x0c, 0x06, 0x09, 0x03, 0x02, 0x0f,
						0x0d, 0x08, 0x0a, 0x01, 0x03, 0x0f, 0x04, 0x02, 0x0b, 0x06, 0x07, 0x0c, 0x00, 0x05, 0x0e, 0x09	},
					{	0x0a, 0x00, 0x09, 0x0e, 0x06, 0x03, 0x0f, 0x05, 0x01, 0x0d, 0x0c, 0x07, 0x0b, 0x04, 0x02, 0x08,
						0x0d, 0x07, 0x00, 0x09, 0x03, 0x04, 0x06, 0x0a, 0x02, 0x08, 0x05, 0x0e, 0x0c, 0x0b, 0x0f, 0x01,
						0x0d, 0x06, 0x04, 0x09, 0x08, 0x0f, 0x03, 0x00, 0x0b, 0x01, 0x02, 0x0c, 0x05, 0x0a, 0x0e, 0x07,
						0x01, 0x0a, 0x0d, 0x00, 0x06, 0x09, 0x08, 0x07, 0x04, 0x0f, 0x0e, 0x03, 0x0b, 0x05, 0x02, 0x0c	},
					{	0x07, 0x0d, 0x0e, 0x03, 0x00, 0x06, 0x09, 0x0a, 0x01, 0x02, 0x08, 0x05, 0x0b, 0x0c, 0x04, 0x0f,
						0x0d, 0x08, 0x0b, 0x05, 0x06, 0x0f, 0x00, 0x03, 0x04, 0x07, 0x02, 0x0c, 0x01, 0x0a, 0x0e, 0x09,
						0x0a, 0x06, 0x09, 0x00, 0x0c, 0x0b, 0x07, 0x0d, 0x0f, 0x01, 0x03, 0x0e, 0x05, 0x02, 0x08, 0x04,
						0x03, 0x0f, 0x00, 0x06, 0x0a, 0x01, 0x0d, 0x08, 0x09, 0x04, 0x05, 0x0b, 0x0c, 0x07, 0x02, 0x0e	},
					{	0x02, 0x0c, 0x04, 0x01, 0x07, 0x0a, 0x0b, 0x06, 0x08, 0x05, 0x03, 0x0f, 0x0d, 0x00, 0x0e, 0x09,
						0x0e, 0x0b, 0x02, 0x0c, 0x04, 0x07, 0x0d, 0x01, 0x05, 0x00, 0x0f, 0x0a, 0x03, 0x09, 0x08, 0x06,
						0x04, 0x02, 0x01, 0x0b, 0x0a, 0x0d, 0x07, 0x08, 0x0f, 0x09, 0x0c, 0x05, 0x06, 0x03, 0x00, 0x0e,
						0x0b, 0x08, 0x0c, 0x07, 0x01, 0x0e, 0x02, 0x0d, 0x06, 0x0f, 0x00, 0x09, 0x0a, 0x04, 0x05, 0x03	},
					{	0x0c, 0x01, 0x0a, 0x0f, 0x09, 0x02, 0x06, 0x08, 0x00, 0x0d, 0x03, 0x04, 0x0e, 0x07, 0x05, 0x0b,
						0x0a, 0x0f, 0x04, 0x02, 0x07, 0x0c, 0x09, 0x05, 0x06, 0x01, 0x0d, 0x0e, 0x00, 0x0b, 0x03, 0x08,
						0x09, 0x0e, 0x0f, 0x05, 0x02, 0x08, 0x0c, 0x03, 0x07, 0x00, 0x04, 0x0a, 0x01, 0x0d, 0x0b, 0x06,
						0x04, 0x03, 0x02, 0x0c, 0x09, 0x05, 0x0f, 0x0a, 0x0b, 0x0e, 0x01, 0x07, 0x06, 0x00, 0x08, 0x0d	},
					{	0x04, 0x0b, 0x02, 0x0e, 0x0f, 0x00, 0x08, 0x0d, 0x03, 0x0c, 0x09, 0x07, 0x05, 0x0a, 0x06, 0x01,
						0x0d, 0x00, 0x0b, 0x07, 0x04, 0x09, 0x01, 0x0a, 0x0e, 0x03, 0x05, 0x0c, 0x02, 0x0f, 0x08, 0x06,
						0x01, 0x04, 0x0b, 0x0d, 0x0c, 0x03, 0x07, 0x0e, 0x0a, 0x0f, 0x06, 0x08, 0x00, 0x05, 0x09, 0x02,
						0x06, 0x0b, 0x0d, 0x08, 0x01, 0x04, 0x0a, 0x07, 0x09, 0x05, 0x00, 0x0f, 0x0e, 0x02, 0x03, 0x0c	},
					{	0x0d, 0x02, 0x08, 0x04, 0x06, 0x0f, 0x0b, 0x01, 0x0a, 0x09, 0x03, 0x0e, 0x05, 0x00, 0x0c, 0x07,
						0x01, 0x0f, 0x0d, 0x08, 0x0a, 0x03, 0x07, 0x04, 0x0c, 0x05, 0x06, 0x0b, 0x00, 0x0e, 0x09, 0x02,
						0x07, 0x0b, 0x04, 0x01, 0x09, 0x0c, 0x0e, 0x02, 0x00, 0x06, 0x0a, 0x0d, 0x0f, 0x03, 0x05, 0x08,
						0x02, 0x01, 0x0e, 0x07, 0x04, 0x0a, 0x08, 0x0d, 0x0f, 0x0c, 0x09, 0x00, 0x03, 0x05, 0x06, 0x0b	}};
	static const u8 Permutation[]={		0x10, 0x07, 0x14, 0x15, 0x1d, 0x0c, 0x1c, 0x11, 0x01, 0x0f, 0x17, 0x1a, 0x05, 0x12, 0x1f, 0x0a,
						0x02, 0x08, 0x18, 0x0e, 0x20, 0x1b, 0x03, 0x09, 0x13, 0x0d, 0x1e, 0x06, 0x16, 0x0b, 0x04, 0x19	};
	static const u8 FP[]={			0x28, 0x08, 0x30, 0x10, 0x38, 0x18, 0x40, 0x20, 0x27, 0x07, 0x2f, 0x0f, 0x37, 0x17, 0x3f, 0x1f,
						0x26, 0x06, 0x2e, 0x0e, 0x36, 0x16, 0x3e, 0x1e, 0x25, 0x05, 0x2d, 0x0d, 0x35, 0x15, 0x3d, 0x1d,
						0x24, 0x04, 0x2c, 0x0c, 0x34, 0x14, 0x3c, 0x1c, 0x23, 0x03, 0x2b, 0x0b, 0x33, 0x13, 0x3b, 0x1b,
						0x22, 0x02, 0x2a, 0x0a, 0x32, 0x12, 0x3a, 0x1a, 0x21, 0x01, 0x29, 0x09, 0x31, 0x11, 0x39, 0x19 };
	u32 BitMask_hi, BitMask_lo;
	u32 PermutedKeyMask_lo, PermutedKeyMask_hi, Rot2Mask_lo, Rot1Mask_lo, Rot2Mask_hi, Rot1Mask_hi;
	u32 BitPerm_lo, BitPerm_hi;
	u32 password_lo, password_hi;
	u32 key1, key2, key3, key4, input_lo, input_hi;
	u32 PermutedKey_lo, PermutedKey_hi, kVal_lo, kVal_hi, PermutedInput_lo, PermutedInput_hi, eVal_lo, eVal_hi, sVal_lo, sVal_hi, pVal_lo, pVal_hi, output_lo, output_hi;
	struct KeyPair pairC[17], pairD[17], pairK[17], pairL[17], pairR[17];
	struct KeyPair *pPairC, *pPairD, *pPairK, *pPairL, *pPairR;
	unsigned int i, j, k;
	int shift;

	//Phase 1 (Permute KEY with Permuted Choice 1)
	PermutedKey_lo = 0;
	PermutedKey_hi = 0;
	BitMask_lo = 0;
	BitMask_hi = 0x80000000;
	BitPerm_lo = 0;
	BitPerm_hi = 0x80000000;

	password_lo = *(const u32*)password;
	password_hi = *(const u32*)(password + 4);

	for(i = 0; i < 56; i++)
	{
		shift = PC1[i] - 1;

		if((shift << 26) >= 0)
		{	//0 to 31-bit shift
			if((shift << 26) > 0)	//Shift all bits left by shift (hi,lo >> shift)
				key1 = (BitMask_lo >> shift) | (BitMask_hi << (-shift));
			else	//0-bit shift, which should not happen.
				key1 = BitMask_lo >> shift;

			key2 = BitMask_hi >> shift;
		} else {	//>31-bit shift
			key2 = 0;
			key1 = BitMask_hi >> shift;
		}

		//If bit (shift) is set, set the current bit.
		if(((password_lo & key1) | (password_hi & key2)) != 0)
		{
			PermutedKey_lo |= BitPerm_lo;
			PermutedKey_hi |= BitPerm_hi;
		}

		//Shift all bits left by 1 (hi,lo >> 1)
		BitPerm_lo = (BitPerm_lo >> 1) | (BitPerm_hi << 31);
		BitPerm_hi >>= 1;
	}

	//Phase 2 (Key Schedule Calculation)
	//This mask is used to extract the lower 28-bits of a key.
	PermutedKeyMask_lo = 0x0FFFFFFF;
	PermutedKeyMask_hi = 0x00000000;

	Rot2Mask_lo = 3;
	Rot2Mask_hi = 0x00000000;
	Rot1Mask_lo = 1;
	Rot1Mask_hi = 0x00000000;

	//C-bits, upper 28-bits of Permuted Key
	pairC[0].lo = PermutedKey_hi >> 4;
	pairC[0].hi = 0;

	//D-bits, lower 28-bits of Permuted Key
	pairD[0].lo = ((PermutedKey_lo >> 8) | (PermutedKey_hi << 24)) & PermutedKeyMask_lo;
	pairD[0].hi = ((PermutedKey_hi >> 8) & PermutedKeyMask_hi);

	//Calculate all Cn and Dn.
	for(i = 0; i < 16; i++)
	{
		if(Rotations[i] != 1)
		{	//Rotate left twice
			//hi 26:0 | lo 31:26
			key1 = ((pairC[i].hi << 6) | (pairC[i].lo >> 26)) & Rot2Mask_lo;
			//lo 29:0
			//key1|key 4 results in: LLLLLLLLLLLLLLLLLLLLLLLLLLLLLXX, where X is from key1
			key4 = pairC[i].lo << 2;
			//hi 31:26
			//This part is discarded.
			key2 = (pairC[i].hi >> 26) & Rot2Mask_hi;
			//hi 29:2 | lo 31:30
			//key2|key3 results in: HHHHHHHHHHHHHHHHHHHHHHHHHHHHHLL
			key3 = (pairC[i].hi << 2) | (pairC[i].lo >> 30);
		} else {	//Rotate left once
			//hi 27:0 | lo 31:27
			key1 = ((pairC[i].hi << 5) | (pairC[i].lo >> 27)) & Rot1Mask_lo;
			//lo 30:0
			//key1|key4 results in: LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLX, where X is from key1
			key4 = pairC[i].lo << 1;
			//hi 31:27
			//This part is discarded.
			key2 = (pairC[i].hi >> 27) & Rot1Mask_hi;
			//hi 30:0 | lo 31:31
			//key2:key3 results in: HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHL
			key3 = (pairC[i].hi << 1) | (pairC[i].lo >> 31);
		}

		//Merge the two rotated parts together, for the hi and low pair.
		//Note: hi contains nothing.
		pairC[i + 1].lo = (key1 | key4) & PermutedKeyMask_lo;
		pairC[i + 1].hi = (key2 | key3) & PermutedKeyMask_hi;

		if(Rotations[i] != 1)
		{	//Rotate left twice
			//hi 26:0 | lo 31:26
			key1 = ((pairD[i].hi << 6) | (pairD[i].lo >> 26)) & Rot2Mask_lo;
			//lo 29:0
			//key1|key 4 results in: LLLLLLLLLLLLLLLLLLLLLLLLLLLLLXX, where X is from key1
			key4 = pairD[i].lo << 2;
			//hi 31:26
			//This part is discarded.
			key2 = (pairD[i].hi >> 26) & Rot2Mask_hi;
			//hi 29:2 | lo 31:30
			//key2|key3 results in: HHHHHHHHHHHHHHHHHHHHHHHHHHHHHLL
			key3 = (pairD[i].hi << 2) | (pairD[i].lo >> 30);
		} else {	//Rotate left once
			//hi 27:0 | lo 31:27
			key1 = ((pairD[i].hi << 5) | (pairD[i].lo >> 27)) & Rot1Mask_lo;
			//lo 30:0
			//key1|key4 results in: LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLX, where X is from key1
			key4 = pairD[i].lo << 1;
			//hi 31:27
			//This part is discarded.
			key2 = (pairD[i].hi >> 27) & Rot1Mask_hi;
			//hi 30:0 | lo 31:31
			//key2:key3 results in: HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHL
			key3 = (pairD[i].hi << 1) | (pairD[i].lo >> 31);
		}

		//Merge the two rotated parts together, for the hi and low pair.
		//Note: hi contains nothing.
		pairD[i + 1].lo = (key1 | key4) & PermutedKeyMask_lo;
		pairD[i + 1].hi = (key2 | key3) & PermutedKeyMask_hi;
	}

	//Phase 3
	BitMask_lo = 0x00000000;
	BitMask_hi = 0x80000000;

	// Determine all K, through the Permutation of CnDn by PC-2
	for(pPairC = &pairC[1],pPairD = &pairD[1],pPairK = &pairK[1]; pPairK < &pairK[17]; pPairC++,pPairD++,pPairK++)
	{
		kVal_lo = 0;
		kVal_hi = 0;
		BitPerm_lo = 0x00000000;
		BitPerm_hi = 0x80000000;

		/*	Calculate CnDn:
			Note: hi of both Cn and Dn are assumed to, and actually contain nothing.
			lo: (D lo 23:0) | (0 7:0)
				D lo	23:0	LLLLLLLLLLLLLLLLLLLLLLLL00000000
				0		7:0		00000000000000000000000000000000
			hi:	(C hi 27:0) | (D hi 23:0) | (D lo 31:24)
				C lo	27:0	llllllllllllllllllllllllllll0000
				D hi	23:0	HHHHHHHHHHHHHHHHHHHHHHHH00000000	All zero
				D lo	31:24	000000000000000000000000XXXXLLLL
				
				l = C lo	L = D lo
				h = C hi	H = D hi	X = unused bits (28/32-bit value)	*/
		input_lo = 0 | (pPairD->lo << 8);
		input_hi = (pPairC->lo << 4) | (pPairD->hi << 8) | (pPairD->lo >> 24);

		for(i = 0; i < 48; i++)
		{
			shift = PC2[i] - 1;

			if((shift << 26) >= 0)
			{	//0 to 31-bit shift
				if((shift << 26) > 0)	//Shift all bits left by shift (hi,lo >> shift)
					key1 = (BitMask_lo >> shift) | (BitMask_hi << (-shift));
				else	//0-bit shift, which should not happen.
					key1 = BitMask_lo >> shift;

				key2 = BitMask_hi >> shift;
			} else {	//>31-bit shift
				key2 = 0;
				key1 = BitMask_hi >> shift;
			}

			//If bit (shift) is set, set the current bit.
			if(((input_lo & key1) | (input_hi & key2)) != 0)
			{
				kVal_lo |= BitPerm_lo;
				kVal_hi |= BitPerm_hi;
			}

			//Shift all bits left by 1 (hi,lo >> 1)
			BitPerm_lo = (BitPerm_hi << 31) | (BitPerm_lo >> 1);
			BitPerm_hi >>= 1;
		}

		pPairK->lo = kVal_lo;
		pPairK->hi = kVal_hi;
	}

	//Phase 4 (Enciphering)
	BitMask_lo = 0x00000000;
	BitMask_hi = 0x80000000;
	BitPerm_lo = 0x00000000;
	BitPerm_hi = 0x80000000;
	PermutedInput_lo = 0;
	PermutedInput_hi = 0;

	//Initial Permutation (IP)
	for(i = 0; i < 64; i++)
	{
		shift = IP[i] - 1;

		if((shift << 26) >= 0)
		{	//0 to 31-bit shift
			if((shift << 26) > 0)	//Shift all bits left by shift (hi,lo >> shift)
				key1 = (BitMask_lo >> shift) | (BitMask_hi << (-shift));
			else	//0-bit shift, which should not happen.
				key1 = BitMask_lo >> shift;

			key2 = BitMask_hi >> shift;
		} else {	//>31-bit shift
			key2 = 0;
			key1 = BitMask_hi >> shift;
		}

		//If bit (shift) is set, set the current bit.
		if(((id_lo & key1) | (id_hi & key2)) != 0)
		{
			PermutedInput_lo |= BitPerm_lo;
			PermutedInput_hi |= BitPerm_hi;
		}

		//Shift all bits left by 1 (hi,lo >> 1)
		BitPerm_lo = (BitPerm_lo >> 1) | (BitPerm_hi << 31);
		BitPerm_hi >>= 1;
	}

	//Phase 5 (Key-dependent Computation)
	BitMask_lo = 0x00000000;
	BitMask_hi = 0x80000000;

	//L0 and R0 make up the permuted input block.
	//The lo values are not used.
	pairL[0].lo = PermutedInput_lo & 0x00000000;
	pairL[0].hi = PermutedInput_hi & 0xFFFFFFFF;
	pairR[0].lo = 0x00000000;
	pairR[0].hi = PermutedInput_lo << 0;

	for (j = 0, k = 1, pPairL = &pairL[1], pPairR = &pairR[1]; pPairR < &pairR[17]; j++, k++, pPairR++, pPairL++)
	{
		eVal_lo = 0;
		eVal_hi = 0;
		BitPerm_lo = 0x00000000;
		BitPerm_hi = 0x80000000;

		//Ln' = Rn
		pPairL->lo = input_lo = pairR[j].lo;
		pPairL->hi = input_hi = pairR[j].hi;

		//Calculate E(Rn)
		for(i = 0; i < 48; i++)
		{
			shift = Expansion[i] - 1;
			if((shift << 26) >= 0)
			{	//0 to 31-bit shift
				if((shift << 26) > 0)	//Shift all bits left by shift (hi,lo >> shift)
					key1 = (BitMask_lo >> shift) | (BitMask_hi << (-shift));
				else	//0-bit shift, which should not happen.
					key1 = BitMask_lo >> shift;

				key2 = BitMask_hi >> shift;
			} else {	//>31-bit shift
				key2 = 0;
				key1 = BitMask_hi >> shift;
			}

			//If bit (shift) is set, set the current bit.
			if(((input_lo & key1) | (input_hi & key2)) != 0)
			{
				eVal_lo |= BitPerm_lo;
				eVal_hi |= BitPerm_hi;
			}

			//Shift all bits left by 1 (hi,lo >> 1)
			BitPerm_lo = (BitPerm_hi << 31) | (BitPerm_lo >> 1);
			BitPerm_hi >>= 1;
		}

		//Phase 6 (Substitution Boxes)
		sVal_lo = 0;
		sVal_hi = 0;
		//Calculate Bn = Kn ^ E(Rn)
		//Also, shift the 48-bit Expansion value forward towards position 0.
		eVal_lo = (eVal_lo ^ pairK[k].lo) >> 16;
		eVal_hi = eVal_hi ^ pairK[k].hi;
		eVal_lo |= (eVal_hi << 16);
		eVal_hi >>= 16;

		//Calculate Sn(Bn), which is stored in the upper 32-bits of the result.
		for(i = 0, shift = 32; i < 8; i++, shift += 4)
		{
			input_lo = sbox[7 - i][((eVal_lo & 0x20) | ((eVal_lo & 0x3F) >> 1 & 0xF) | ((eVal_lo & 0x3F) << 4 & 0x10))];
			//Shift to the next 6-bit B-block
			eVal_lo = (eVal_lo >> 6) | (eVal_hi << 26);
			eVal_hi >>= 6;

			input_hi = 0;
			if((shift << 26) >= 0)
			{	//0 to 31-bit shift
				if((shift << 26) > 0)	//Shift all bits right by shift (hi,lo >> shift)
					key3 = (input_hi << shift) | (input_lo >> (-shift));
				else	//0-bit shift, which should not happen.
					key3 = input_hi << shift;

				key4 = input_lo << shift;
			} else {	//>31-bit shift
				key3 = input_lo << shift;
				key4 = 0;
			}

			sVal_lo |= key4;
			sVal_hi |= key3;
		}

		//Phase 7 (Permutation with function P)
		pVal_lo = 0;
		pVal_hi = 0;
		BitPerm_lo = 0x00000000;
		BitPerm_hi = 0x80000000;

		//Calculate P(Sn(K ^ E(Rn)))
		for(i = 0; i < 32; i++)
		{
			shift = Permutation[i] - 1;
			if((shift << 26) >= 0)
			{	//0 to 31-bit shift
				if((shift << 26) > 0)	//Shift all bits left by shift (hi,lo >> shift)
					key1 = (BitMask_lo >> shift) | (BitMask_hi << (-shift));
				else	//0-bit shift, which should not happen.
					key1 = BitMask_lo >> shift;

				key2 = BitMask_hi >> shift;
			} else {	//>31-bit shift
				key1 = BitMask_hi >> shift;
				key2 = 0;
			}

			//If bit (shift) is set, set the current bit.
			if(((sVal_lo & key1) | (sVal_hi & key2)) != 0)
			{
				pVal_lo |= BitPerm_lo;
				pVal_hi |= BitPerm_hi;
			}

			//Shift all bits left by 1 (hi,lo >> 1)
			BitPerm_lo = (BitPerm_lo >> 1) | (BitPerm_hi << 31);
			BitPerm_hi >>= 1;
		}

		//0x00003bc4	- Rn' = Ln ^ f(Rn,Kn)
		pPairR->lo = pairL[j].lo ^ pVal_lo;
		pPairR->hi = pairL[j].hi ^ pVal_hi;
	}

	//Phase 8 (Final Permutation)
	output_lo = 0;
	output_hi = 0;
	BitMask_lo = BitPerm_lo = 0x00000000;
	BitMask_hi = BitPerm_hi = 0x80000000;
	//Retrieve preoutput blocks
	//Ln and Rn lo do not contain anything, hence they are discarded.
	input_lo = pairR[16].lo | (pairL[16].hi >> 0);
	input_hi = pairR[16].hi | 0;

	//Subject the preoutput to the Final Permutation
	for(i = 0; i < 64; i++)
	{
		shift = FP[i] - 1;
		if((shift << 26) >= 0)
		{	//0 to 31-bit shift
			if((shift << 26) > 0)	//Shift all bits left by shift (hi,lo >> shift)
				key1 = (BitMask_lo >> shift) | (BitMask_hi << (-shift));
			else	//0-bit shift, which should not happen.
				key1 = BitMask_lo >> shift;

			key2 = BitMask_hi >> shift;
		} else {	//>31-bit shift
			key1 = BitMask_hi >> shift;
			key2 = 0;
		}

		//If bit (shift) is set, set the current bit.
		if(((input_lo & key1) | (input_hi & key2)) != 0)
		{
			output_lo |= BitPerm_lo;
			output_hi |= BitPerm_hi;
		}

		//Shift all bits left by 1 (hi,lo >> 1)
		BitPerm_lo = (BitPerm_lo >> 1) | (BitPerm_hi << 31);
		BitPerm_hi >>= 1;
	}

	//Encrypted output
	*(u32*)password_out = output_lo;
	*(u32*)(password_out + 4) = output_hi;
}
