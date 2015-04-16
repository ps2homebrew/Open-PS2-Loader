/* 
 * Adapted from java to C by jimmikaelkael <jimmikaelkael@wanadoo.fr>
 */

/**
 * Implements the MD4 message digest algorithm in Java.
 * 
 * References:
 * 
 *        Ronald L. Rivest,
 *        The MD4 Message-Digest Algorithm: <http://www.roxen.com/rfc/rfc1320.html>,
 *        IETF RFC-1320 (informational).
 *
 * Revision: 1.2
 * @author  Raif S. Naffah
 */

#include <string.h>

static unsigned int context[4];

/*
 * The basic MD4 atomic functions.
 */
static unsigned int FF(int a, int b, int c, int d, int x, int s)
{
	unsigned int t = (a + ((b & c) | (~b & d)) + x) & 0xFFFFFFFF;
	return ((t << s) & 0xFFFFFFFF) | (t >> (32 - s));
}

static unsigned int GG(int a, int b, int c, int d, int x, int s)
{
	unsigned int t = (a + ((b & c) | (b & d) | (c & d)) + x + 0x5A827999) & 0xFFFFFFFF;
	return ((t << s) & 0xFFFFFFFF) | (t >> (32 - s));
}

static unsigned int HH(int a, int b, int c, int d, int x, int s)
{
	unsigned int t = (a + (b ^ c ^ d) + x + 0x6ED9EBA1) & 0xFFFFFFFF;
	return ((t << s) & 0xFFFFFFFF) | (t >> (32 - s));
}

/**
 *    MD4 basic transformation.
 *
 *    Transforms context based on 512 bits from input block
 *
 *    @param    block    input block
 */
static void transform(unsigned char *block)
{
	int i;
	unsigned int X[16];

	for (i=0; i<16; i++)
		X[i] = (block[i*4+3] << 24) | (block[i*4+2] << 16) | (block[i*4+1] << 8) | block[i*4];

	unsigned int A = context[0];
	unsigned int B = context[1];
	unsigned int C = context[2];
	unsigned int D = context[3];

	A = FF(A, B, C, D, X[0], 3);
	D = FF(D, A, B, C, X[1], 7);
	C = FF(C, D, A, B, X[2], 11);
	B = FF(B, C, D, A, X[3], 19);
	A = FF(A, B, C, D, X[4], 3);
	D = FF(D, A, B, C, X[5], 7);
	C = FF(C, D, A, B, X[6], 11);
	B = FF(B, C, D, A, X[7], 19);
	A = FF(A, B, C, D, X[8], 3);
	D = FF(D, A, B, C, X[9], 7);
	C = FF(C, D, A, B, X[10], 11);
	B = FF(B, C, D, A, X[11], 19);
	A = FF(A, B, C, D, X[12], 3);
	D = FF(D, A, B, C, X[13], 7);
	C = FF(C, D, A, B, X[14], 11);
	B = FF(B, C, D, A, X[15], 19);

	A = GG(A, B, C, D, X[0], 3);
	D = GG(D, A, B, C, X[4], 5);
	C = GG(C, D, A, B, X[8], 9);
	B = GG(B, C, D, A, X[12], 13);
	A = GG(A, B, C, D, X[1], 3);
	D = GG(D, A, B, C, X[5], 5);
	C = GG(C, D, A, B, X[9], 9);
	B = GG(B, C, D, A, X[13], 13);
	A = GG(A, B, C, D, X[2], 3);
	D = GG(D, A, B, C, X[6], 5);
	C = GG(C, D, A, B, X[10], 9);
	B = GG(B, C, D, A, X[14], 13);
	A = GG(A, B, C, D, X[3], 3);
	D = GG(D, A, B, C, X[7], 5);
	C = GG(C, D, A, B, X[11], 9);
	B = GG(B, C, D, A, X[15], 13);

	A = HH(A, B, C, D, X[0], 3);
	D = HH(D, A, B, C, X[8], 9);
	C = HH(C, D, A, B, X[4], 11);
	B = HH(B, C, D, A, X[12], 15);
	A = HH(A, B, C, D, X[2], 3);
	D = HH(D, A, B, C, X[10], 9);
	C = HH(C, D, A, B, X[6], 11);
	B = HH(B, C, D, A, X[14], 15);
	A = HH(A, B, C, D, X[1], 3);
	D = HH(D, A, B, C, X[9], 9);
	C = HH(C, D, A, B, X[5], 11);
	B = HH(B, C, D, A, X[13], 15);
	A = HH(A, B, C, D, X[3], 3);
        D = HH(D, A, B, C, X[11], 9);
	C = HH(C, D, A, B, X[7], 11);
	B = HH(B, C, D, A, X[15], 15);

	context[0] += A;
	context[1] += B;
	context[2] += C;
	context[3] += D;

	context[0] &= 0xFFFFFFFF;
	context[1] &= 0xFFFFFFFF;
	context[2] &= 0xFFFFFFFF;
	context[3] &= 0xFFFFFFFF;
}

/**
 * Resets this object disregarding any temporary data present at the
 * time of the invocation of this call.
 */
static void engineReset()
{
	// initial values of MD4 i.e. A, B, C, D
	// as per rfc-1320; they are low-order byte first
	context[0] = 0x67452301;
	context[1] = 0xEFCDAB89;
	context[2] = 0x98BADCFE;
	context[3] = 0x10325476;
}

/** 
 * produce a MD4 message digest from message of len bytes 
 */
unsigned char *MD4(unsigned char *message, int len, unsigned char *cipher)
{
	unsigned char buffer[128];
	unsigned int b = len * 8;

	engineReset();

	while (len > 64) {
		memcpy(buffer, message, 64);
		transform(buffer);
		message += 64;
		len -= 64;
	}

	memset(buffer, 0, 128);
	memcpy(buffer, message, len);
	buffer[len] = 0x80;

	if (len < 56) {
		*((unsigned int *)&buffer[56]) = b;
		transform(buffer);
	} else {
		*((unsigned int *)&buffer[120]) = b;
		transform(buffer);
		transform(&buffer[64]);
	}

	*((unsigned int *)&cipher[0]) = context[0];
	*((unsigned int *)&cipher[4]) = context[1];
	*((unsigned int *)&cipher[8]) = context[2];
	*((unsigned int *)&cipher[12]) = context[3];

	return (unsigned char *)cipher;
}

