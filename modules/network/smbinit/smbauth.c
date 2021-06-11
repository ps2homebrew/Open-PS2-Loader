/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include <tamtypes.h>
#include <sysclib.h>

#include "oplsmb.h"
#include "smbauth.h"
#include "des.h"
#include "md4.h"

#define SERVER_USE_PLAINTEXT_PASSWORD 0
#define SERVER_USE_ENCRYPTED_PASSWORD 1

/*
 * LM_Password_Hash: this function create a LM password hash from a given password
 */
static unsigned char *LM_Password_Hash(const char *password, unsigned char *cipher)
{
    char tmp_pass[14] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
    unsigned char K1[7];
    unsigned char K2[7];
    int i;

    /* keep only 14 bytes of the password (padded with nul bytes) */
    strncpy(tmp_pass, password, 14);

    /* turn the password to uppercase */
    for (i = 0; i < 14; i++) {
        tmp_pass[i] = toupper(tmp_pass[i]);
    }

    /* get 2 7bytes keys from password */
    memcpy(K1, &tmp_pass[0], 7);
    memcpy(K2, &tmp_pass[7], 7);

    /* encrypt the magic string with the keys */
    DES(K1, (unsigned char *)"KGS!@#$%", &cipher[0]);
    DES(K2, (unsigned char *)"KGS!@#$%", &cipher[8]);

    return (unsigned char *)cipher;
}

/*
 * NTLM_Password_Hash: this function create a NTLM password hash from a given password
 */
static unsigned char *NTLM_Password_Hash(const char *password, unsigned char *cipher)
{
    char passwd_buf[512];
    int i, j;

    memset(passwd_buf, 0, sizeof(passwd_buf));

    /* turn the password to unicode */
    for (i = 0, j = 0; i < strlen(password); i++, j += 2)
        passwd_buf[j] = password[i];

    /* get the message digest */
    MD4((unsigned char *)passwd_buf, j, cipher);

    return (unsigned char *)cipher;
}

/*
 * LM_Response: this function create a LM response from a given LM hash & challenge
 */
static unsigned char *LM_Response(const unsigned char *LMpasswordhash, unsigned char *chal, unsigned char *cipher)
{
    unsigned char P21[21];
    unsigned char K[7];
    int i;

    /* padd the LM password hash with 5 nul bytes */
    memcpy(&P21[0], LMpasswordhash, 16);
    memset(&P21[16], 0, 5);

    /* encrypt the challenge 3 times, using 7 bytes splitted keys */
    for (i = 0; i < 3; i++) {

        /* get the 7bytes key */
        memcpy(K, &P21[i * 7], 7);

        /* encrypt each the challenge with the keys */
        DES(K, chal, &cipher[i * 8]);
    }

    return (unsigned char *)cipher;
}

/*
 * GenerateLMHashes: function used to generate LM/NTLM hashes
 */
static void GenerateLMHashes(char *Password, int PasswordType, u8 *EncryptionKey, int *PasswordLen, char *Buffer)
{
    u8 LMpasswordhash[16];
    u8 NTLMpasswordhash[16];
    u8 LMresponse[24];
    u8 NTLMresponse[24];

    if (PasswordType == SERVER_USE_ENCRYPTED_PASSWORD) {
        /* Generate both LM & NTLM hashes */
        LM_Password_Hash(Password, LMpasswordhash);
        NTLM_Password_Hash(Password, NTLMpasswordhash);
        *PasswordLen = 24;
        LM_Response(LMpasswordhash, EncryptionKey, LMresponse);
        LM_Response(NTLMpasswordhash, EncryptionKey, NTLMresponse);
        memcpy(&Buffer[0], LMresponse, *PasswordLen);
        memcpy(&Buffer[24], NTLMresponse, *PasswordLen);
    } else if (PasswordType == SERVER_USE_PLAINTEXT_PASSWORD) {
        /* It seems that PlainText passwords and Unicode isn't meant to be... */
        *PasswordLen = strlen(Password);
        if (*PasswordLen > 14)
            *PasswordLen = 14;
        else if (*PasswordLen == 0)
            *PasswordLen = 1;
        memcpy(Buffer, Password, *PasswordLen);
    }
}

void SmbInitHashPassword(server_specs_t *ss)
{
    if (ss->HashedFlag == 0) {
        /* generate the LM and NTLM hash then fill Password buffer with both hashes */
        if (strlen(ss->Password) > 0)
            GenerateLMHashes(ss->Password, ss->PasswordType, ss->EncryptionKey, &ss->PasswordLen, ss->Password);
    }

    /* used to notify cdvdman that password are now hashed */
    ss->HashedFlag = 1;
}
