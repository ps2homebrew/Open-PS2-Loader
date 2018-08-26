/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
   */

#include <stdio.h>
#include <errno.h>
#include <sysclib.h>
#include "smstcpip.h"
#include <thbase.h>
#include <thsemap.h>
#include <intrman.h>
#include <sifman.h>

#include "smsutils.h"
#include "oplsmb.h"
#include "smb.h"
#include "cdvd_config.h"

static int io_sema = -1;

#define WAITIOSEMA(x) WaitSema(x)
#define SIGNALIOSEMA(x) SignalSema(x)

// !!! ps2ip exports functions pointers !!!
extern int (*plwip_close)(int s);                                                                                                                    // #6
extern int (*plwip_connect)(int s, struct sockaddr *name, socklen_t namelen);                                                                        // #7
extern int (*plwip_recvfrom)(int s, void *header, int hlen, void *payload, int plen, unsigned int flags, struct sockaddr *from, socklen_t *fromlen); // #10
extern int (*plwip_send)(int s, void *dataptr, int size, unsigned int flags);                                                                        // #11
extern int (*plwip_socket)(int domain, int type, int protocol);                                                                                      // #13
extern u32 (*pinet_addr)(const char *cp);                                                                                                            // #24

extern struct cdvdman_settings_smb cdvdman_settings;

#define SMB_MAGIC 0x424d53ff

struct SMBHeader_t
{ //size = 36
    u32 sessionHeader;
    u32 Magic;
    u8 Cmd;
    short Eclass;
    short Ecode;
    u8 Flags;
    u16 Flags2;
    u8 Extra[12];
    u16 TID;
    u16 PID;
    u16 UID;
    u16 MID;
} __attribute__((packed));

struct NegociateProtocolRequest_t
{
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u16 ByteCount;           // 37
    u8 DialectFormat;        // 39
    char DialectName[0];     // 40
} __attribute__((packed));

struct NegociateProtocolResponse_t
{
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u16 DialectIndex;        // 37
    u8 SecurityMode;         // 39
    u16 MaxMpxCount;         // 40
    u16 MaxVC;               // 42
    u32 MaxBufferSize;       // 44
    u32 MaxRawBuffer;        // 48
    u32 SessionKey;          // 52
    u32 Capabilities;        // 56
    s64 SystemTime;          // 60
    u16 ServerTimeZone;      // 68
    u8 KeyLength;            // 70
    u16 ByteCount;           // 71
    u8 ByteField[0];         // 73
} __attribute__((packed));

struct SessionSetupAndXRequest_t
{
    struct SMBHeader_t smbH;   // 0
    u8 smbWordcount;           // 36
    u8 smbAndxCmd;             // 37
    u8 smbAndxReserved;        // 38
    u16 smbAndxOffset;         // 39
    u16 MaxBufferSize;         // 41
    u16 MaxMpxCount;           // 43
    u16 VCNumber;              // 45
    u32 SessionKey;            // 47
    u16 AnsiPasswordLength;    // 51
    u16 UnicodePasswordLength; // 53
    u32 reserved;              // 55
    u32 Capabilities;          // 59
    u16 ByteCount;             // 63
    u8 ByteField[0];           // 65
} __attribute__((packed));

struct SessionSetupAndXResponse_t
{
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u8 smbAndxCmd;           // 37
    u8 smbAndxReserved;      // 38
    u16 smbAndxOffset;       // 39
    u16 Action;              // 41
    u16 ByteCount;           // 43
    u8 ByteField[0];         // 45
} __attribute__((packed));

struct TreeConnectAndXRequest_t
{
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u8 smbAndxCmd;           // 37
    u8 smbAndxReserved;      // 38
    u16 smbAndxOffset;       // 39
    u16 Flags;               // 41
    u16 PasswordLength;      // 43
    u16 ByteCount;           // 45
    u8 ByteField[0];         // 47
} __attribute__((packed));

struct TreeConnectAndXResponse_t
{
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u8 smbAndxCmd;           // 37
    u8 smbAndxReserved;      // 38
    u16 smbAndxOffset;       // 39
    u16 OptionalSupport;     // 41
    u16 ByteCount;           // 43
} __attribute__((packed));

struct OpenAndXRequest_t
{
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u8 smbAndxCmd;           // 37
    u8 smbAndxReserved;      // 38
    u16 smbAndxOffset;       // 39
    u16 Flags;               // 41
    u16 AccessMask;          // 43
    u16 SearchAttributes;    // 45
    u16 FileAttributes;      // 47
    u8 CreationTime[4];      // 49
    u16 CreateOptions;       // 53
    u32 AllocationSize;      // 55
    u32 reserved[2];         // 59
    u16 ByteCount;           // 67
    u8 ByteField[0];         // 69
} __attribute__((packed));

struct OpenAndXResponse_t
{
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u8 smbAndxCmd;           // 37
    u8 smbAndxReserved;      // 38
    u16 smbAndxOffset;       // 39
    u16 FID;                 // 41
    u16 FileAttributes;      // 43
    u8 LastWriteTime[4];     // 45
    u32 FileSize;            // 49
    u16 GrantedAccess;       // 53
    u16 FileType;            // 55
    u16 IPCState;            // 57
    u16 Action;              // 59
    u32 ServerFID;           // 61
    u16 reserved;            // 65
    u16 ByteCount;           // 67
} __attribute__((packed));

struct ReadAndXRequest_t
{                            // size = 63
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u8 smbAndxCmd;           // 37
    u8 smbAndxReserved;      // 38
    u16 smbAndxOffset;       // 39
    u16 FID;                 // 41
    u32 OffsetLow;           // 43
    u16 MaxCountLow;         // 47
    u16 MinCount;            // 49
    u32 MaxCountHigh;        // 51
    u16 Remaining;           // 55
    u32 OffsetHigh;          // 57
    u16 ByteCount;           // 61
} __attribute__((packed));

struct ReadAndXResponse_t
{
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u8 smbAndxCmd;           // 37
    u8 smbAndxReserved;      // 38
    u16 smbAndxOffset;       // 39
    u16 Remaining;           // 41
    u16 DataCompactionMode;  // 43
    u16 reserved;            // 45
    u16 DataLengthLow;       // 47
    u16 DataOffset;          // 49
    u32 DataLengthHigh;      // 51
    u8 reserved2[6];         // 55
    u16 ByteCount;           // 61
} __attribute__((packed));

struct WriteAndXRequest_t
{                            // size = 67
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u8 smbAndxCmd;           // 37
    u8 smbAndxReserved;      // 38
    u16 smbAndxOffset;       // 39
    u16 FID;                 // 41
    u32 OffsetLow;           // 43
    u32 Reserved;            // 47
    u16 WriteMode;           // 51
    u16 Remaining;           // 53
    u16 DataLengthHigh;      // 55
    u16 DataLengthLow;       // 57
    u16 DataOffset;          // 59
    u32 OffsetHigh;          // 61
    u16 ByteCount;           // 65
} __attribute__((packed));

struct CloseRequest_t {      // size = 45
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u16 FID;                 // 37
    u32 LastWrite;           // 39
    u16 ByteCount;           // 43
} __attribute__((packed));

struct CloseResponse_t {
    struct SMBHeader_t smbH; // 0
    u8 smbWordcount;         // 36
    u16 ByteCount;           // 37
} __attribute__((packed));

static server_specs_t server_specs;

#define SERVER_SHARE_SECURITY_LEVEL 0
#define SERVER_USER_SECURITY_LEVEL 1
#define SERVER_USE_PLAINTEXT_PASSWORD 0
#define SERVER_USE_ENCRYPTED_PASSWORD 1
#define LM_AUTH 0
#define NTLM_AUTH 1

static struct ReadAndXRequest_t smb_Read_Request = {
    {0x3b000000,
     SMB_MAGIC,
     SMB_COM_READ_ANDX,
     0, 0, 0, SMB_FLAGS2_32BIT_STATUS, "\0", 0, 0, 0, 0},
    12,
    SMB_COM_NONE,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0};

static struct WriteAndXRequest_t smb_Write_Request = {
    {0,
     SMB_MAGIC,
     SMB_COM_WRITE_ANDX,
     0, 0, 0, SMB_FLAGS2_32BIT_STATUS, "\0", 0, 0, 0, 0},
    14,
    SMB_COM_NONE,
    0,
    0,
    0,
    0,
    0,
    0x01,
    0,
    0,
    0,
    0x3f,
    0 //DataOffset = 0x3f, WriteMode = 1
};

static u16 UID, TID;
static int main_socket;

static u8 SMB_buf[1024];

//-------------------------------------------------------------------------
int rawTCP_SetSessionHeader(u32 size) // Write Session Service header: careful it's raw TCP transport here and not NBT transport
{
    // maximum for raw TCP transport (24 bits) !!!
    SMB_buf[0] = 0;
    SMB_buf[1] = (size >> 16) & 0xff;
    SMB_buf[2] = (size >> 8) & 0xff;
    SMB_buf[3] = size;

    return (int)size;
}

//-------------------------------------------------------------------------
int rawTCP_GetSessionHeader(void) // Read Session Service header: careful it's raw TCP transport here and not NBT transport
{
    register u32 size;

    // maximum for raw TCP transport (24 bits) !!!
    size = SMB_buf[3];
    size |= SMB_buf[2] << 8;
    size |= SMB_buf[1] << 16;

    return (int)size;
}

//-------------------------------------------------------------------------
int OpenTCPSession(struct in_addr dst_IP, u16 dst_port)
{
    register int sock, ret;
    struct sockaddr_in sock_addr;

    // Creating socket
    sock = plwip_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
        return -2;

    mips_memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_addr = dst_IP;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(dst_port);

    while (1) {
        ret = plwip_connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
        if (ret >= 0)
            break;
        DelayThread(500);
    }

    return sock;
}

//-------------------------------------------------------------------------
int GetSMBServerReply(void)
{
    register int rcv_size, totalpkt_size, pkt_size;

    rcv_size = plwip_send(main_socket, SMB_buf, rawTCP_GetSessionHeader() + 4, 0);
    if (rcv_size <= 0)
        return -1;

receive:
    rcv_size = plwip_recvfrom(main_socket, NULL, 0, SMB_buf, sizeof(SMB_buf), 0, NULL, NULL);
    if (rcv_size <= 0)
        return -2;

    if (SMB_buf[0] != 0) // dropping NBSS Session Keep alive
        goto receive;

    // Handle fragmented packets
    totalpkt_size = rawTCP_GetSessionHeader() + 4;

    while (rcv_size < totalpkt_size) {
        pkt_size = plwip_recvfrom(main_socket, NULL, 0, &SMB_buf[rcv_size], sizeof(SMB_buf) - rcv_size, 0, NULL, NULL);
        if (pkt_size <= 0)
            return -2;
        rcv_size += pkt_size;
    }

    return rcv_size;
}

//-------------------------------------------------------------------------
int smb_NegotiateProtocol(char *SMBServerIP, int SMBServerPort, char *Username, char *Password, u32 *capabilities, OplSmbPwHashFunc_t hash_callback)
{
    char *dialect = "NT LM 0.12";
    struct NegociateProtocolRequest_t *NPR = (struct NegociateProtocolRequest_t *)SMB_buf;
    register int length;
    struct in_addr dst_addr;
    iop_sema_t smp;

    smp.initial = 1;
    smp.max = 1;
    smp.option = 0;
    smp.attr = 1;
    io_sema = CreateSema(&smp);

    dst_addr.s_addr = pinet_addr(SMBServerIP);

    // Opening TCP session
    main_socket = OpenTCPSession(dst_addr, SMBServerPort);

negociate_retry:

    mips_memset(SMB_buf, 0, sizeof(SMB_buf));

    NPR->smbH.Magic = SMB_MAGIC;
    NPR->smbH.Cmd = SMB_COM_NEGOCIATE;
    NPR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
    NPR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
    length = strlen(dialect);
    NPR->ByteCount = length + 2;
    NPR->DialectFormat = 0x02;
    strcpy(NPR->DialectName, dialect);

    rawTCP_SetSessionHeader(37 + length);
    GetSMBServerReply();

    struct NegociateProtocolResponse_t *NPRsp = (struct NegociateProtocolResponse_t *)SMB_buf;

    // check sanity of SMB header
    if (NPRsp->smbH.Magic != SMB_MAGIC)
        goto negociate_retry;

    // check there's no error
    if (NPRsp->smbH.Eclass != STATUS_SUCCESS)
        goto negociate_retry;

    if (NPRsp->smbWordcount != 17)
        goto negociate_retry;

    *capabilities = NPRsp->Capabilities & (CLIENT_CAP_LARGE_READX | CLIENT_CAP_UNICODE | CLIENT_CAP_LARGE_FILES | CLIENT_CAP_STATUS32);
    if (NPRsp->Capabilities & SERVER_CAP_UNICODE)
        server_specs.StringsCF = 2;
    else
        server_specs.StringsCF = 1;

    if (NPRsp->SecurityMode & NEGOCIATE_SECURITY_USER_LEVEL)
        server_specs.SecurityMode = SERVER_USER_SECURITY_LEVEL;
    else
        server_specs.SecurityMode = SERVER_SHARE_SECURITY_LEVEL;

    if (NPRsp->SecurityMode & NEGOCIATE_SECURITY_CHALLENGE_RESPONSE)
        server_specs.PasswordType = SERVER_USE_ENCRYPTED_PASSWORD;
    else
        server_specs.PasswordType = SERVER_USE_PLAINTEXT_PASSWORD;

    // copy to global struct to keep needed information for further communication
    server_specs.MaxBufferSize = NPRsp->MaxBufferSize;
    server_specs.MaxMpxCount = NPRsp->MaxMpxCount;
    server_specs.SessionKey = NPRsp->SessionKey;
    mips_memcpy(server_specs.EncryptionKey, &NPRsp->ByteField[0], NPRsp->KeyLength);
    mips_memcpy(server_specs.PrimaryDomainServerName, &NPRsp->ByteField[NPRsp->KeyLength], sizeof(server_specs.PrimaryDomainServerName));
    strncpy(server_specs.Username, Username, sizeof(server_specs.Username));
    server_specs.Username[sizeof(server_specs.Username) - 1] = '\0';
    strncpy(server_specs.Password, Password, sizeof(server_specs.Password));
    server_specs.Password[sizeof(server_specs.Password) - 1] = '\0';
    server_specs.IOPaddr = (void *)&server_specs;
    server_specs.HashedFlag = (server_specs.PasswordType == SERVER_USE_ENCRYPTED_PASSWORD) ? 0 : -1;

    hash_callback(&server_specs);

    return 1;
}

//-------------------------------------------------------------------------
int smb_SessionSetupAndX(u32 capabilities)
{
    struct SessionSetupAndXRequest_t *SSR = (struct SessionSetupAndXRequest_t *)SMB_buf;
    register int i, offset, CF;
    int AuthType = NTLM_AUTH;
    int password_len = 0;

lbl_session_setup:
    mips_memset(SMB_buf, 0, sizeof(SMB_buf));

    CF = server_specs.StringsCF;

    SSR->smbH.Magic = SMB_MAGIC;
    SSR->smbH.Cmd = SMB_COM_SESSION_SETUP_ANDX;
    SSR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
    SSR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
    if (CF == 2)
        SSR->smbH.Flags2 |= SMB_FLAGS2_UNICODE_STRING;
    SSR->smbWordcount = 13;
    SSR->smbAndxCmd = SMB_COM_NONE; // no ANDX command
    SSR->MaxBufferSize = server_specs.MaxBufferSize > 65535 ? 65535 : (u16)server_specs.MaxBufferSize;
    SSR->MaxMpxCount = server_specs.MaxMpxCount >= 2 ? 2 : (u16)server_specs.MaxMpxCount;
    SSR->VCNumber = 1;
    SSR->SessionKey = server_specs.SessionKey;
    SSR->Capabilities = capabilities;

    // Fill ByteField
    offset = 0;

    if (server_specs.SecurityMode == SERVER_USER_SECURITY_LEVEL) {
        password_len = server_specs.PasswordLen;
        // Copy the password accordingly to auth type
        mips_memcpy(&SSR->ByteField[offset], &server_specs.Password[(AuthType << 4) + (AuthType << 3)], password_len);
        // fill SSR->AnsiPasswordLength or SSR->UnicodePasswordLength accordingly to auth type
        if (AuthType == LM_AUTH)
            SSR->AnsiPasswordLength = password_len;
        else
            SSR->UnicodePasswordLength = password_len;
        offset += password_len;
    }

    if ((CF == 2) && (!(password_len & 1)))
        offset += 1; // pad needed only for unicode as aligment fix if password length is even

    for (i = 0; i < strlen(server_specs.Username); i++) {
        SSR->ByteField[offset] = server_specs.Username[i]; // add User name
        offset += CF;
    }
    offset += CF; // null terminator

    for (i = 0; server_specs.PrimaryDomainServerName[i] != 0; i += CF) {
        SSR->ByteField[offset] = server_specs.PrimaryDomainServerName[i]; // PrimaryDomain, acquired from Negociate Protocol Response Datas
        offset += CF;
    }
    offset += CF; // null terminator

    for (i = 0; i < (CF << 1); i++)
        SSR->ByteField[offset++] = 0; // NativeOS, NativeLanMan

    SSR->ByteCount = offset;

    rawTCP_SetSessionHeader(sizeof(struct SessionSetupAndXRequest_t) - 4 + offset);
    GetSMBServerReply();

    struct SessionSetupAndXResponse_t *SSRsp = (struct SessionSetupAndXResponse_t *)SMB_buf;

    // check sanity of SMB header
    if (SSRsp->smbH.Magic != SMB_MAGIC)
        return -1;

    // check there's no auth failure
    if ((server_specs.SecurityMode == SERVER_USER_SECURITY_LEVEL) && ((SSRsp->smbH.Eclass | (SSRsp->smbH.Ecode << 16)) == STATUS_LOGON_FAILURE) && (AuthType == NTLM_AUTH)) {
        AuthType = LM_AUTH;
        goto lbl_session_setup;
    }

    // check there's no error (NT STATUS error type!)
    if ((SSRsp->smbH.Eclass | SSRsp->smbH.Ecode) != STATUS_SUCCESS)
        return -1000;

    // keep UID
    UID = SSRsp->smbH.UID;

    return 1;
}

//-------------------------------------------------------------------------
int smb_TreeConnectAndX(char *ShareName)
{
    struct TreeConnectAndXRequest_t *TCR = (struct TreeConnectAndXRequest_t *)SMB_buf;
    register int i, offset, CF;
    int AuthType = NTLM_AUTH;
    int password_len = 0;

    mips_memset(SMB_buf, 0, sizeof(SMB_buf));

    CF = server_specs.StringsCF;

    TCR->smbH.Magic = SMB_MAGIC;
    TCR->smbH.Cmd = SMB_COM_TREE_CONNECT_ANDX;
    TCR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
    TCR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
    if (CF == 2)
        TCR->smbH.Flags2 |= SMB_FLAGS2_UNICODE_STRING;
    TCR->smbH.UID = UID;
    TCR->smbWordcount = 4;
    TCR->smbAndxCmd = SMB_COM_NONE; // no ANDX command

    // Fill ByteField
    offset = 0;

    password_len = 1;
    if (server_specs.SecurityMode == SERVER_SHARE_SECURITY_LEVEL) {
        password_len = server_specs.PasswordLen;
        // Copy the password accordingly to auth type
        mips_memcpy(&TCR->ByteField[offset], &server_specs.Password[(AuthType << 4) + (AuthType << 3)], password_len);
    }
    TCR->PasswordLength = password_len;
    offset += password_len;

    if ((CF == 2) && (!(password_len & 1)))
        offset += 1; // pad needed only for unicode as aligment fix is password len is even

    for (i = 0; i < strlen(ShareName); i++) {
        TCR->ByteField[offset] = ShareName[i]; // add Share name
        offset += CF;
    }
    offset += CF; // null terminator

    mips_memcpy(&TCR->ByteField[offset], "?????\0", 6); // Service, any type of device
    offset += 6;

    TCR->ByteCount = offset;

    rawTCP_SetSessionHeader(sizeof(struct TreeConnectAndXRequest_t) - 4 + offset);
    GetSMBServerReply();

    struct TreeConnectAndXResponse_t *TCRsp = (struct TreeConnectAndXResponse_t *)SMB_buf;

    // check sanity of SMB header
    if (TCRsp->smbH.Magic != SMB_MAGIC)
        return -1;

    // check there's no error (NT STATUS error type!)
    if ((TCRsp->smbH.Eclass | TCRsp->smbH.Ecode) != STATUS_SUCCESS)
        return -1000;

    // keep TID
    TID = TCRsp->smbH.TID;

    return 1;
}

//-------------------------------------------------------------------------
int smb_OpenAndX(char *filename, u16 *FID, int Write)
{
    struct OpenAndXRequest_t *OR = (struct OpenAndXRequest_t *)SMB_buf;
    register int i, offset, CF;

    WAITIOSEMA(io_sema);

    mips_memset(SMB_buf, 0, sizeof(SMB_buf));

    CF = server_specs.StringsCF;

    OR->smbH.Magic = SMB_MAGIC;
    OR->smbH.Cmd = SMB_COM_OPEN_ANDX;
    OR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES;
    OR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
    if (CF == 2)
        OR->smbH.Flags2 |= SMB_FLAGS2_UNICODE_STRING;
    OR->smbH.UID = UID;
    OR->smbH.TID = TID;
    OR->smbWordcount = 15;
    OR->smbAndxCmd = SMB_COM_NONE; // no ANDX command
    OR->AccessMask = Write ? 2 : 0;
    OR->FileAttributes = Write ? EXT_ATTR_NORMAL : EXT_ATTR_READONLY;
    OR->CreateOptions = 1;

    offset = 0;
    if (CF == 2)
        offset++; // pad needed only for unicode as aligment fix

    for (i = 0; i < strlen(filename); i++) {
        OR->ByteField[offset] = filename[i]; // add filename
        offset += CF;
    }
    offset += CF;

    OR->ByteCount = offset;

    rawTCP_SetSessionHeader(66 + offset);
    GetSMBServerReply();

    struct OpenAndXResponse_t *ORsp = (struct OpenAndXResponse_t *)SMB_buf;

    // check sanity of SMB header
    if (ORsp->smbH.Magic != SMB_MAGIC)
        return -1;

    // check there's no error
    if (ORsp->smbH.Eclass != STATUS_SUCCESS)
        return -1000;

    *FID = ORsp->FID;

    // Prepare header of Read/Write Request by advance
    smb_Read_Request.smbH.UID = UID;
    smb_Read_Request.smbH.TID = TID;

    smb_Write_Request.smbH.UID = UID;
    smb_Write_Request.smbH.TID = TID;

    SIGNALIOSEMA(io_sema);

    return 1;
}

//-------------------------------------------------------------------------
int smb_Close(int FID)
{
    int r;
    struct CloseRequest_t *CR = (struct CloseRequest_t *)SMB_buf;

    WAITIOSEMA(io_sema);

    mips_memset(SMB_buf, 0, sizeof(SMB_buf));

    CR->smbH.Magic = SMB_MAGIC;
    CR->smbH.Cmd = SMB_COM_CLOSE;
    CR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES;
    CR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
    CR->smbH.UID = (u16)UID;
    CR->smbH.TID = (u16)TID;
    CR->smbWordcount = 3;
    CR->FID = (u16)FID;

    rawTCP_SetSessionHeader(41);
    r = GetSMBServerReply();
    if (r <= 0)
        return -EIO;

    struct CloseResponse_t *CRsp = (struct CloseResponse_t *)SMB_buf;

    // check sanity of SMB header
    if (CRsp->smbH.Magic != SMB_MAGIC)
        return -EIO;

    // check there's no error
    if ((CRsp->smbH.Eclass | (CRsp->smbH.Ecode << 16)) != STATUS_SUCCESS)
        return -EIO;

    SIGNALIOSEMA(io_sema);

    return 0;
}

//-------------------------------------------------------------------------
int smb_ReadFile(u16 FID, u32 offsetlow, u32 offsethigh, void *readbuf, u16 nbytes)
{
    register int rcv_size, pkt_size, expected_size;

    struct ReadAndXRequest_t *RR = &smb_Read_Request;

    WAITIOSEMA(io_sema);

    RR->FID = FID;
    RR->OffsetLow = offsetlow;
    RR->OffsetHigh = offsethigh;
    RR->MaxCountLow = nbytes;

    plwip_send(main_socket, RR, sizeof(struct ReadAndXRequest_t), 0);
receive:
    //offset 49 is the offset of the DataOffset field within the ReadAndXResponse structure.
    rcv_size = plwip_recvfrom(main_socket, SMB_buf, 49, readbuf, nbytes, 0, NULL, NULL);
    expected_size = rawTCP_GetSessionHeader() + 4;

    if (SMB_buf[0] != 0) // dropping NBSS Session Keep alive
        goto receive;

    // Handle fragmented packets
    while (rcv_size < expected_size) {
        pkt_size = plwip_recvfrom(main_socket, NULL, 0, &((u8 *)readbuf)[rcv_size - SMB_buf[49] - 4], expected_size - rcv_size, 0, NULL, NULL); // - rcv_size
        rcv_size += pkt_size;
    }

    SIGNALIOSEMA(io_sema);

    return 1;
}

//-------------------------------------------------------------------------
int smb_WriteFile(u16 FID, u32 offsetlow, u32 offsethigh, void *writebuf, u16 nbytes)
{
    struct WriteAndXRequest_t *WR = (struct WriteAndXRequest_t *)SMB_buf;

    WAITIOSEMA(io_sema);

    mips_memcpy(WR, &smb_Write_Request, sizeof(struct WriteAndXRequest_t));

    WR->FID = FID;
    WR->OffsetLow = offsetlow;
    WR->OffsetHigh = offsethigh;
    WR->Remaining = nbytes;
    WR->DataLengthLow = nbytes;
    WR->ByteCount = nbytes;

    //DataOffset points to the location of the data within the buffer, from right after sessionHeader, which Microsoft doesn't consider as being part of the SMB header.
    mips_memcpy((void *)(&SMB_buf[4 + WR->DataOffset]), writebuf, nbytes);

    rawTCP_SetSessionHeader(sizeof(struct WriteAndXRequest_t) - 4 + nbytes);
    GetSMBServerReply();

    SIGNALIOSEMA(io_sema);

    return 1;
}

//-------------------------------------------------------------------------
int smb_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num)
{
    register u32 sectors, offset, nbytes;
    u8 *p = (u8 *)buf;

    offset = lsn;

    while (nsectors > 0) {
        sectors = nsectors;
        if (sectors > MAX_SMB_SECTORS)
            sectors = MAX_SMB_SECTORS;

        nbytes = sectors << 11;

        smb_ReadFile(cdvdman_settings.FIDs[part_num], offset << 11, offset >> 21, p, nbytes);

        p += nbytes;
        offset += sectors;
        nsectors -= sectors;
    }

    return 1;
}

void smb_CloseAll(void)
{
    int i;

    for (i = 0; i < cdvdman_settings.common.NumParts; i++) {
        smb_Close(cdvdman_settings.FIDs[i]);
    }
}

//-------------------------------------------------------------------------
int smb_Disconnect(void)
{
    plwip_close(main_socket);

    return 1;
}
