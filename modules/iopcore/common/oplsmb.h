
#ifndef __OPLSMB__
#define __OPLSMB__

typedef struct
{
    u32 MaxBufferSize;
    u32 SessionKey;
    char PrimaryDomainServerName[32];
    u8 EncryptionKey[8];
    u32 Capabilities;
    u16 MaxMpxCount;
    u8 SecurityMode; // 0 = share level, 1 = user level
    u8 PasswordType; // 0 = PlainText passwords, 1 = use challenge/response
    char Username[36];
    char Password[48]; // either PlainText, either hashed
    int PasswordLen;
    int HashedFlag;
    void *IOPaddr;
} server_specs_t;

typedef void (*OplSmbPwHashFunc_t)(server_specs_t *ss);

#define oplsmb_IMPORTS_start DECLARE_IMPORT_TABLE(oplsmb, 1, 1)
#define oplsmb_IMPORTS_end   END_IMPORT_TABLE

void smb_NegotiateProt(OplSmbPwHashFunc_t hash_callback);
#define I_smb_NegotiateProt DECLARE_IMPORT(4, smb_NegotiateProt)

#endif
