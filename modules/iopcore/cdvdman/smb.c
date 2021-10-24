/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
   */

#include <stdio.h>
#include <errno.h>
#include <sysclib.h>
#include "smstcpip.h"
#include <limits.h>
#include <thbase.h>
#include <thsemap.h>
#include <intrman.h>
#include <sifman.h>

#include "oplsmb.h"
#include "smb.h"
#include "cdvd_config.h"

#include "smsutils.h"

#define USE_CUSTOM_RECV 1

//Round up the erasure amount, so that memset can erase memory word-by-word.
#define ZERO_PKT_ALIGNED(hdr, hdrSize) memset((hdr), 0, ((hdrSize) + 3) & ~3)

/* Limit the maximum chunk size of receiving operations, to avoid triggering the congestion avoidance algorithm of the SMB server.
   This is because the IOP cannot clear the received frames fast enough, causing the number of bytes in flight to grow exponentially.
   The TCP congestion avoidence algorithm may induce some latency, causing extremely poor performance.
   The value to use should be smaller than the TCP window size. Right now, it is 10240 (according to lwipopts.h). */
#define CLIENT_MAX_BUFFER_SIZE 8192      //Allow up to 8192 bytes to be received.
#define CLIENT_MAX_XMIT_SIZE   USHRT_MAX //Allow up to 65535 bytes to be transmitted.
#define CLIENT_MAX_RECV_SIZE   8192      //Allow up to 8192 bytes to be received.

int smb_io_sema = -1;

#define WAITIOSEMA(x)   WaitSema(x)
#define SIGNALIOSEMA(x) SignalSema(x)

// !!! ps2ip exports functions pointers !!!
extern int (*plwip_close)(int s);                                                                                                                 // #6
extern int (*plwip_connect)(int s, struct sockaddr *name, socklen_t namelen);                                                                     // #7
extern int (*plwip_recv)(int s, void *mem, int len, unsigned int flags);                                                                          // #9
extern int (*plwip_recvfrom)(int s, void *mem, int hlen, void *payload, int plen, unsigned int flags, struct sockaddr *from, socklen_t *fromlen); // #10
extern int (*plwip_send)(int s, void *dataptr, int size, unsigned int flags);                                                                     // #11
extern int (*plwip_socket)(int domain, int type, int protocol);                                                                                   // #13
extern int (*plwip_setsockopt)(int s, int level, int optname, const void *optval, socklen_t optlen);                                              // #19
extern u32 (*pinet_addr)(const char *cp);                                                                                                         // #24

extern struct cdvdman_settings_smb cdvdman_settings;

// Local function prototypes
static void nb_SetSessionMessage(u32 size);  // Write Session Service message
static int nb_GetSessionMessageLength(void); // Read Session Service message length
static u8 nb_GetPacketType(void);            // Read message type

static server_specs_t server_specs;

#define LM_AUTH   0
#define NTLM_AUTH 1

static u16 UID, TID;
static int main_socket = -1;

static struct
{
    //Direct transport packet header. This is also a NetBIOS session header.
    u32 sessionHeader; //The lower 24 bytes are the length of the payload in network byte-order, while the upper 8 bits must be set to 0 (Session Message Packet).
    union
    {
        u8 u8buff[MAX_SMB_BUF + MAX_SMB_BUF_HDR];
        u16 u16buff[(MAX_SMB_BUF + MAX_SMB_BUF_HDR) / sizeof(u16)];
        s16 s16buff[(MAX_SMB_BUF + MAX_SMB_BUF_HDR) / sizeof(s16)];
        NegotiateProtocolRequest_t negotiateProtocolRequest;
        NegotiateProtocolResponse_t negotiateProtocolResponse;
        SessionSetupAndXRequest_t sessionSetupAndXRequest;
        SessionSetupAndXResponse_t sessionSetupAndXResponse;
        TreeConnectAndXRequest_t treeConnectAndXRequest;
        TreeConnectAndXResponse_t treeConnectAndXResponse;
        OpenAndXRequest_t openAndXRequest;
        OpenAndXResponse_t openAndXResponse;
        ReadAndXRequest_t readAndXRequest;
        ReadAndXResponse_t readAndXResponse;
        WriteAndXRequest_t writeAndXRequest;
        WriteAndXResponse_t writeAndXResponse;
        CloseRequest_t closeRequest;
        CloseResponse_t closeResponse;
    } smb;
} __attribute__((packed)) SMB_buf;

//-------------------------------------------------------------------------
static void nb_SetSessionMessage(u32 size) // Write Session Service header: careful it's raw TCP transport here and not NBT transport
{
    // maximum for raw TCP transport (24 bits) !!!
    // Byte-swap length into network byte-order.
    SMB_buf.sessionHeader = ((size & 0xff0000) >> 8) | ((size & 0xff00) << 8) | ((size & 0xff) << 24);
}

//-------------------------------------------------------------------------
static int nb_GetSessionMessageLength(void) // Read Session Service header length: careful it's raw TCP transport here and not NBT transport
{
    u32 size;

    // maximum for raw TCP transport (24 bits) !!!
    // Byte-swap length from network byte-order.
    size = ((SMB_buf.sessionHeader << 8) & 0xff0000) | ((SMB_buf.sessionHeader >> 8) & 0xff00) | ((SMB_buf.sessionHeader >> 24) & 0xff);

    return (int)size;
}

static u8 nb_GetPacketType(void) // Read Session Service header type.
{
    // Byte-swap length from network byte-order.
    return ((u8)(SMB_buf.sessionHeader & 0xff));
}

//-------------------------------------------------------------------------
int OpenTCPSession(struct in_addr dst_IP, u16 dst_port)
{
    register int sock, ret;
    int opt;
    struct sockaddr_in sock_addr;

    // Creating socket
    sock = plwip_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
        return -2;

    opt = 1;
    plwip_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt));

    memset(&sock_addr, 0, sizeof(sock_addr));
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
static int SendData(int sock, char *buf, int size)
{
    int remaining, result;
    char *ptr;

    ptr = buf;
    remaining = size;
    while (remaining > 0) {
        result = plwip_send(sock, ptr, remaining, 0);
        if (result <= 0)
            return result;

        ptr += result;
        remaining -= result;
    }

    return size;
}

static int RecvData(int sock, char *buf, int size)
{
    int remaining, result;
    char *ptr;

    ptr = buf;
    remaining = size;
    while (remaining > 0) {
        result = plwip_recv(sock, ptr, remaining, 0);
        if (result <= 0)
            return result;

        ptr += result;
        remaining -= result;
    }

    return size;
}

//-------------------------------------------------------------------------
static int GetSMBServerReply(int shdrlen, void *spayload, int rhdrlen)
{
    int rcv_size, totalpkt_size, size;

    totalpkt_size = nb_GetSessionMessageLength() + 4;

    if (shdrlen == 0) {
        //Send the whole message, including the 4-byte direct transport packet header.
        rcv_size = SendData(main_socket, (char *)&SMB_buf, totalpkt_size);
        if (rcv_size <= 0)
            return -1;
    } else {
        size = shdrlen + 4;

        //Send the headers, followed by the payload.
        rcv_size = SendData(main_socket, (char *)&SMB_buf, size);
        if (rcv_size <= 0)
            return -1;

        rcv_size = SendData(main_socket, spayload, totalpkt_size - size);
        if (rcv_size <= 0)
            return -1;
    }

    //Read NetBIOS session message header. Drop NBSS Session Keep alive messages (type == 0x85, with no body), but process session messages (type == 0x00).
    do {
        rcv_size = RecvData(main_socket, (char *)&SMB_buf.sessionHeader, sizeof(SMB_buf.sessionHeader));
        if (rcv_size <= 0)
            return -2;
    } while (nb_GetPacketType() != 0);

    totalpkt_size = nb_GetSessionMessageLength();

    //If rhdrlen is not specified, retrieve the whole packet. Otherwise, retrieve only the headers (caller will retrieve the payload separately).
    size = (rhdrlen == 0) ? totalpkt_size : rhdrlen;
    rcv_size = RecvData(main_socket, (char *)&SMB_buf.smb, size);
    if (rcv_size <= 0)
        return -2;

    return totalpkt_size;
}

//-------------------------------------------------------------------------
//These functions will process UTF-16 characters on a byte-level, so that they will be safe for use with byte-alignment.
static int asciiToUtf16(char *out, const char *in)
{
    int len;
    const char *pIn;
    char *pOut;

    for (pIn = in, pOut = out, len = 0; *pIn != '\0'; pIn++, pOut += 2, len += 2) {
        pOut[0] = *pIn;
        pOut[1] = '\0';
    }

    pOut[0] = '\0'; //NULL terminate.
    pOut[1] = '\0';
    len += 2;

    return len;
}

static int setStringField(char *out, const char *in)
{
    int len;

    if (server_specs.Capabilities & SERVER_CAP_UNICODE) {
        len = asciiToUtf16(out, in);
    } else {
        len = strlen(in) + 1;
        strcpy(out, in);
    }

    return len;
}

//-------------------------------------------------------------------------
int smb_NegotiateProtocol(char *SMBServerIP, int SMBServerPort, char *Username, char *Password, u32 *capabilities, OplSmbPwHashFunc_t hash_callback)
{
    char *dialect = "NT LM 0.12";
    NegotiateProtocolRequest_t *NPR = &SMB_buf.smb.negotiateProtocolRequest;
    NegotiateProtocolResponse_t *NPRsp = &SMB_buf.smb.negotiateProtocolResponse;
    register int length;
    struct in_addr dst_addr;
    iop_sema_t smp;

    smp.initial = 1;
    smp.max = 1;
    smp.option = 0;
    smp.attr = 1;
    smb_io_sema = CreateSema(&smp);

    dst_addr.s_addr = pinet_addr(SMBServerIP);

    // Opening TCP session
    main_socket = OpenTCPSession(dst_addr, SMBServerPort);

negotiate_retry:

    ZERO_PKT_ALIGNED(NPR, sizeof(NegotiateProtocolRequest_t));

    NPR->smbH.Magic = SMB_MAGIC;
    NPR->smbH.Cmd = SMB_COM_NEGOTIATE;
    NPR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
    NPR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
    length = strlen(dialect);
    NPR->ByteCount = length + 2;
    NPR->DialectFormat = 0x02;
    strcpy(NPR->DialectName, dialect);

    nb_SetSessionMessage(sizeof(NegotiateProtocolRequest_t) + length + 1);
    GetSMBServerReply(0, NULL, 0);

    // check sanity of SMB header
    if (NPRsp->smbH.Magic != SMB_MAGIC)
        goto negotiate_retry;

    // check there's no error
    if (NPRsp->smbH.Eclass != STATUS_SUCCESS)
        goto negotiate_retry;

    if (NPRsp->smbWordcount != 17)
        goto negotiate_retry;

    *capabilities = NPRsp->Capabilities & (CLIENT_CAP_LARGE_WRITEX | CLIENT_CAP_LARGE_READX | CLIENT_CAP_UNICODE | CLIENT_CAP_LARGE_FILES | CLIENT_CAP_STATUS32);

    server_specs.Capabilities = NPRsp->Capabilities;
    server_specs.SecurityMode = (NPRsp->SecurityMode & NEGOTIATE_SECURITY_USER_LEVEL) ? SERVER_USER_SECURITY_LEVEL : SERVER_SHARE_SECURITY_LEVEL;
    server_specs.PasswordType = (NPRsp->SecurityMode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) ? SERVER_USE_ENCRYPTED_PASSWORD : SERVER_USE_PLAINTEXT_PASSWORD;

    // copy to global struct to keep needed information for further communication
    server_specs.MaxBufferSize = NPRsp->MaxBufferSize;
    server_specs.MaxMpxCount = NPRsp->MaxMpxCount;
    server_specs.SessionKey = NPRsp->SessionKey;
    memcpy(server_specs.EncryptionKey, &NPRsp->ByteField[0], NPRsp->KeyLength);
    memcpy(server_specs.PrimaryDomainServerName, &NPRsp->ByteField[NPRsp->KeyLength], sizeof(server_specs.PrimaryDomainServerName));
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
    SessionSetupAndXRequest_t *SSR = &SMB_buf.smb.sessionSetupAndXRequest;
    SessionSetupAndXResponse_t *SSRsp = &SMB_buf.smb.sessionSetupAndXResponse;
    register int offset, useUnicode;
    int AuthType = NTLM_AUTH;
    int password_len = 0;

lbl_session_setup:
    ZERO_PKT_ALIGNED(SSR, sizeof(SessionSetupAndXRequest_t));

    useUnicode = (server_specs.Capabilities & SERVER_CAP_UNICODE) ? 1 : 0;

    SSR->smbH.Magic = SMB_MAGIC;
    SSR->smbH.Cmd = SMB_COM_SESSION_SETUP_ANDX;
    SSR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
    SSR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
    if (server_specs.Capabilities & SERVER_CAP_UNICODE)
        SSR->smbH.Flags2 |= SMB_FLAGS2_UNICODE_STRING;
    SSR->smbWordcount = 13;
    SSR->smbAndxCmd = SMB_COM_NONE; // no ANDX command
    SSR->MaxBufferSize = CLIENT_MAX_BUFFER_SIZE;
    SSR->MaxMpxCount = server_specs.MaxMpxCount >= 2 ? 2 : (u16)server_specs.MaxMpxCount;
    SSR->VCNumber = 1;
    SSR->SessionKey = server_specs.SessionKey;
    SSR->Capabilities = capabilities;

    // Fill ByteField
    offset = 0;

    if (server_specs.SecurityMode == SERVER_USER_SECURITY_LEVEL) {
        password_len = server_specs.PasswordLen;
        // Copy the password accordingly to auth type
        memcpy(&SSR->ByteField[offset], &server_specs.Password[(AuthType << 4) + (AuthType << 3)], password_len);
        // fill SSR->AnsiPasswordLength or SSR->UnicodePasswordLength accordingly to auth type
        if (AuthType == LM_AUTH)
            SSR->AnsiPasswordLength = password_len;
        else
            SSR->UnicodePasswordLength = password_len;
        offset += password_len;
    }

    if ((useUnicode) && (!(password_len & 1))) {
        SSR->ByteField[offset] = '\0';
        offset++; // pad needed only for unicode as aligment fix if password length is even
    }

    // Add User name
    offset += setStringField((char *)&SSR->ByteField[offset], server_specs.Username);

    // PrimaryDomain, acquired from Negotiate Protocol Response data
    offset += setStringField((char *)&SSR->ByteField[offset], server_specs.PrimaryDomainServerName);

    // NativeOS
    if (useUnicode && ((offset & 1) != 0)) {
        //If Unicode is used, the field must begin on a 16-bit aligned address.
        SSR->ByteField[offset] = '\0';
        offset++;
    }
    offset += setStringField((char *)&SSR->ByteField[offset], "PlayStation 2");

    // NativeLanMan
    if (useUnicode && ((offset & 1) != 0)) {
        //If Unicode is used, the field must begin on a 16-bit aligned address.
        SSR->ByteField[offset] = '\0';
        offset++;
    }
    offset += setStringField((char *)&SSR->ByteField[offset], "SMBMAN");

    SSR->ByteCount = offset;

    nb_SetSessionMessage(sizeof(SessionSetupAndXRequest_t) + offset);
    GetSMBServerReply(0, NULL, 0);

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
    TreeConnectAndXRequest_t *TCR = &SMB_buf.smb.treeConnectAndXRequest;
    TreeConnectAndXResponse_t *TCRsp = &SMB_buf.smb.treeConnectAndXResponse;
    register int offset;
    int AuthType = NTLM_AUTH;
    int password_len = 0;

    ZERO_PKT_ALIGNED(TCR, sizeof(TreeConnectAndXRequest_t));

    TCR->smbH.Magic = SMB_MAGIC;
    TCR->smbH.Cmd = SMB_COM_TREE_CONNECT_ANDX;
    TCR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
    TCR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
    if (server_specs.Capabilities & SERVER_CAP_UNICODE)
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
        memcpy(&TCR->ByteField[offset], &server_specs.Password[(AuthType << 4) + (AuthType << 3)], password_len);
    }
    TCR->PasswordLength = password_len;
    offset += password_len;

    if ((server_specs.Capabilities & SERVER_CAP_UNICODE) && (!(password_len & 1))) {
        TCR->ByteField[offset] = '\0';
        offset++; // pad needed only for unicode as aligment fix is password len is even
    }

    // Add share name
    offset += setStringField((char *)&TCR->ByteField[offset], ShareName);

    memcpy(&TCR->ByteField[offset], "?????\0", 6); // Service, any type of device
    offset += 6;

    TCR->ByteCount = offset;

    nb_SetSessionMessage(sizeof(TreeConnectAndXRequest_t) + offset);
    GetSMBServerReply(0, NULL, 0);

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
int smb_OpenAndX(char *filename, u8 *FID, int Write)
{
    OpenAndXRequest_t *OR = &SMB_buf.smb.openAndXRequest;
    OpenAndXResponse_t *ORsp = &SMB_buf.smb.openAndXResponse;
    register int offset;

    WAITIOSEMA(smb_io_sema);

    ZERO_PKT_ALIGNED(OR, sizeof(OpenAndXRequest_t));

    OR->smbH.Magic = SMB_MAGIC;
    OR->smbH.Cmd = SMB_COM_OPEN_ANDX;
    OR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES;
    OR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
    if (server_specs.Capabilities & SERVER_CAP_UNICODE)
        OR->smbH.Flags2 |= SMB_FLAGS2_UNICODE_STRING;
    OR->smbH.UID = UID;
    OR->smbH.TID = TID;
    OR->smbWordcount = 15;
    OR->smbAndxCmd = SMB_COM_NONE; // no ANDX command
    OR->AccessMask = Write ? 2 : 0;
    OR->FileAttributes = Write ? EXT_ATTR_NORMAL : EXT_ATTR_READONLY;
    OR->CreateOptions = 1;

    offset = 0;
    if (server_specs.Capabilities & SERVER_CAP_UNICODE) {
        OR->ByteField[offset] = '\0';
        offset++; // pad needed only for unicode as aligment fix
    }

    // Add filename
    offset += setStringField((char *)&OR->ByteField[offset], filename);
    OR->ByteCount = offset;

    nb_SetSessionMessage(sizeof(OpenAndXRequest_t) + offset + 1);
    GetSMBServerReply(0, NULL, 0);

    // check sanity of SMB header
    if (ORsp->smbH.Magic != SMB_MAGIC) {
        SIGNALIOSEMA(smb_io_sema);
        return -1;
    }

    // check there's no error
    if (ORsp->smbH.Eclass != STATUS_SUCCESS) {
        SIGNALIOSEMA(smb_io_sema);
        return -1000;
    }

    memcpy(FID, &ORsp->FID, 2);

    SIGNALIOSEMA(smb_io_sema);

    return 1;
}

//-------------------------------------------------------------------------
//Do not call WaitSema() from this function because it would have been already called.
int smb_Close(int FID)
{
    int r;
    CloseRequest_t *CR = &SMB_buf.smb.closeRequest;
    CloseResponse_t *CRsp = &SMB_buf.smb.closeResponse;

    //  WAITIOSEMA(smb_io_sema);

    ZERO_PKT_ALIGNED(CR, sizeof(CloseRequest_t));

    CR->smbH.Magic = SMB_MAGIC;
    CR->smbH.Cmd = SMB_COM_CLOSE;
    CR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES;
    CR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
    CR->smbH.UID = (u16)UID;
    CR->smbH.TID = (u16)TID;
    CR->smbWordcount = 3;
    CR->FID = (u16)FID;

    nb_SetSessionMessage(sizeof(CloseRequest_t));
    r = GetSMBServerReply(0, NULL, 0);
    if (r <= 0) {
        //      SIGNALIOSEMA(smb_io_sema);
        return -EIO;
    }

    // check sanity of SMB header
    if (CRsp->smbH.Magic != SMB_MAGIC) {
        //      SIGNALIOSEMA(smb_io_sema);
        return -EIO;
    }

    // check there's no error
    if ((CRsp->smbH.Eclass | (CRsp->smbH.Ecode << 16)) != STATUS_SUCCESS) {
        //      SIGNALIOSEMA(smb_io_sema);
        return -EIO;
    }

    //  SIGNALIOSEMA(smb_io_sema);

    return 0;
}

//-------------------------------------------------------------------------
static int smb_ReadAndX(u16 FID, u32 offsetlow, u32 offsethigh, void *readbuf, int nbytes)
{
    ReadAndXRequest_t *RR = &SMB_buf.smb.readAndXRequest;
    ReadAndXResponse_t *RRsp = &SMB_buf.smb.readAndXResponse;
    register int r, DataLength;
#ifdef USE_CUSTOM_RECV
    int rcv_size, expected_size;
#else
    int padding;
#endif

    ZERO_PKT_ALIGNED(RR, sizeof(ReadAndXRequest_t));

    RR->smbH.Magic = SMB_MAGIC;
    RR->smbH.Flags2 = SMB_FLAGS2_32BIT_STATUS;
    RR->smbH.Cmd = SMB_COM_READ_ANDX;
    RR->smbH.UID = (u16)UID;
    RR->smbH.TID = (u16)TID;
    RR->smbWordcount = 12;
    RR->smbAndxCmd = SMB_COM_NONE; // no ANDX command
    RR->FID = (u16)FID;
    RR->OffsetLow = offsetlow;
    RR->OffsetHigh = offsethigh;
    RR->MaxCountLow = (u16)nbytes;
    RR->MaxCountHigh = (u16)(nbytes >> 16);

    nb_SetSessionMessage(sizeof(ReadAndXRequest_t));

#ifdef USE_CUSTOM_RECV
    //Send the whole message, including the 4-byte direct transport packet header.
    r = SendData(main_socket, (char *)&SMB_buf, sizeof(ReadAndXRequest_t) + 4);
    if (r <= 0)
        return -1;

    //offset 49 is the offset of the DataOffset field within the ReadAndXResponse structure.
    //recvfrom() is a custom function that will receive the reply.
    do {
        rcv_size = plwip_recvfrom(main_socket, &SMB_buf, 49, readbuf, nbytes, 0, NULL, NULL);
        if (rcv_size <= 0)
            return -2;
    } while (nb_GetPacketType() != 0); // dropping NBSS Session Keep alive

    expected_size = nb_GetSessionMessageLength() + 4;
    DataLength = (int)(((u32)RRsp->DataLengthHigh << 16) | RRsp->DataLengthLow);

    // Handle fragmented packets
    while (rcv_size < expected_size) {
        r = plwip_recvfrom(main_socket, NULL, 0, &((u8 *)readbuf)[rcv_size - RRsp->DataOffset - 4], expected_size - rcv_size, 0, NULL, NULL); // - rcv_size
        rcv_size += r;
    }
#else
    r = GetSMBServerReply(0, NULL, sizeof(ReadAndXResponse_t));
    if (r <= 0)
        return -EIO;

    // check there's no error
    if ((RRsp->smbH.Eclass | (RRsp->smbH.Ecode << 16)) != STATUS_SUCCESS)
        return -EIO;

    //Skip any padding bytes.
    padding = RRsp->DataOffset - sizeof(ReadAndXResponse_t);
    if (padding > 0) {
        r = RecvData(main_socket, (char *)(RRsp + 1), padding);
        if (r <= 0)
            return -2;
    }

    DataLength = (int)(((u32)RRsp->DataLengthHigh << 16) | RRsp->DataLengthLow);
    if (DataLength > 0) {
        r = RecvData(main_socket, readbuf, DataLength);
        if (r <= 0)
            return -2;
    }
#endif

    return DataLength;
}

int smb_ReadFile(u16 FID, u32 offsetlow, u32 offsethigh, void *readbuf, int nbytes)
{
    int result, remaining, toRead;
    char *ptr;

    remaining = nbytes;
    ptr = readbuf;

    WAITIOSEMA(smb_io_sema);

    while (remaining > 0) {
        toRead = remaining > CLIENT_MAX_RECV_SIZE ? CLIENT_MAX_RECV_SIZE : remaining;

        result = smb_ReadAndX(FID, offsetlow, offsethigh, ptr, toRead);
        if (result <= 0)
            return result;

        //Check for and handle overflow.
        if (offsetlow + result < offsetlow)
            offsethigh++;
        offsetlow += result;
        ptr += result;
        remaining -= result;
    }

    SIGNALIOSEMA(smb_io_sema);

    return nbytes;
}

//-------------------------------------------------------------------------
static int smb_WriteAndX(u16 FID, u32 offsetlow, u32 offsethigh, void *writebuf, int nbytes)
{
    const int padding = 1; //1 padding byte, to keep the payload aligned for writing performance.
    WriteAndXRequest_t *WR = &SMB_buf.smb.writeAndXRequest;
    WriteAndXResponse_t *WRsp = &SMB_buf.smb.writeAndXResponse;
    int r;

    ZERO_PKT_ALIGNED(WR, sizeof(WriteAndXRequest_t) + padding);

    WR->smbH.Magic = SMB_MAGIC;
    WR->smbH.Flags2 = SMB_FLAGS2_32BIT_STATUS;
    WR->smbH.Cmd = SMB_COM_WRITE_ANDX;
    WR->smbH.UID = (u16)UID;
    WR->smbH.TID = (u16)TID;
    WR->smbWordcount = 14;
    WR->smbAndxCmd = SMB_COM_NONE; // no ANDX command
    WR->FID = (u16)FID;
    WR->OffsetLow = offsetlow;
    WR->OffsetHigh = offsethigh;
    WR->WriteMode = 0x0001; //WritethroughMode
    WR->DataOffset = sizeof(WriteAndXRequest_t) + padding;
    WR->Remaining = (u16)nbytes;
    WR->DataLengthLow = (u16)nbytes;
    WR->DataLengthHigh = (u16)(nbytes >> 16);
    WR->ByteCount = (u16)nbytes + padding;

    nb_SetSessionMessage(sizeof(WriteAndXRequest_t) + padding + nbytes);
    r = GetSMBServerReply(sizeof(WriteAndXRequest_t) + padding, writebuf, 0);
    if (r <= 0)
        return -EIO;

    // check there's no error
    if ((WRsp->smbH.Eclass | (WRsp->smbH.Ecode << 16)) != STATUS_SUCCESS)
        return -EIO;

    return nbytes;
}

int smb_WriteFile(u16 FID, u32 offsetlow, u32 offsethigh, void *writebuf, int nbytes)
{
    int result, remaining, toWrite;
    char *ptr;

    remaining = nbytes;
    ptr = writebuf;

    WAITIOSEMA(smb_io_sema);

    while (remaining > 0) {
        toWrite = remaining > CLIENT_MAX_XMIT_SIZE ? CLIENT_MAX_XMIT_SIZE : remaining;

        result = smb_WriteAndX(FID, offsetlow, offsethigh, ptr, toWrite);
        if (result <= 0)
            return result;

        //Check for and handle overflow.
        if (offsetlow + result < offsetlow)
            offsethigh++;
        offsetlow += result;
        ptr += result;
        remaining -= result;
    }

    SIGNALIOSEMA(smb_io_sema);

    return nbytes;
}

//-------------------------------------------------------------------------
int smb_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num)
{
    return smb_ReadFile(cdvdman_settings.FIDs[part_num], lsn * 2048, lsn >> 21, buf, (int)(nsectors * 2048));
}

void smb_CloseAll(void)
{
    int i, fd;

    for (i = 0; i < cdvdman_settings.common.NumParts; i++) {
        fd = cdvdman_settings.FIDs[i];
        if (fd >= 0) {
            smb_Close(fd);
            cdvdman_settings.FIDs[i] = -1;
        }
    }
}

//-------------------------------------------------------------------------
int smb_Disconnect(void)
{
    if (main_socket >= 0) {
        plwip_close(main_socket);
        main_socket = -1;
    }

    return 1;
}
