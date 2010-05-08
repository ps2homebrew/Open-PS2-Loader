#ifndef __LMAUTH_H__
#define __LMAUTH_H__

// function prototypes
unsigned char *LM_Password_Hash(const unsigned char *password, unsigned char *cipher);
unsigned char *LM_Response(const unsigned char *LMhash, unsigned char *chal, unsigned char *cipher);

#endif
