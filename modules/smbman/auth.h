#ifndef __AUTH_H__
#define __AUTH_H__

// function prototypes
unsigned char *LM_Password_Hash(const unsigned char *password, unsigned char *cipher);
unsigned char *NTLM_Password_Hash(const unsigned char *password, unsigned char *cipher);
unsigned char *LM_Response(const unsigned char *LMhash, unsigned char *chal, unsigned char *cipher);

#endif
