

#include <stdio.h>
#include <sysclib.h>

#include "des.h"

/*
 * LM_Password_Hash: this function create a LM password hash from a given password
 */
unsigned char *LM_Password_Hash(const unsigned char *password, unsigned char *cipher)
{
	unsigned char tmp_pass[14] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
	unsigned char K1[7];
	unsigned char K2[7];
	int i;

	/* keep only 14 bytes of the password (padded with nul bytes) */
	strncpy(tmp_pass, password, 14);

	/* turn the password to uppercase */
	for (i=0; i<14; i++) {
		tmp_pass[i] = toupper(tmp_pass[i]);
	}

	/* get 2 7bytes keys from password */
	memcpy(K1, &tmp_pass[0], 7);
	memcpy(K2, &tmp_pass[7], 7);

	/* encrypt the magic string with the keys */
	DES(K1, "KGS!@#$%", &cipher[0]);
	DES(K2, "KGS!@#$%", &cipher[8]);

	return (unsigned char *)cipher;
}

/*
 * LM_Response: this function create a LM response from a given LM hash & challenge
 */
unsigned char *LM_Response(const unsigned char *LMpasswordhash, unsigned char *chal, unsigned char *cipher)
{
	unsigned char P21[21];
	unsigned char K[7];
	int i;
 
	/* padd the LM password hash with 5 nul bytes */
	memcpy(&P21[0], LMpasswordhash, 16);
	memset(&P21[16], 0, 5);

	/* encrypt the challenge 3 times, using 7 bytes splitted keys */
	for (i=0; i<3; i++) {

		/* get the 7bytes key */
		memcpy(K, &P21[i*7], 7);		

		/* encrypt each the challenge with the keys */
		DES(K, chal, &cipher[i*8]);
	}

	return (unsigned char *)cipher;
}

