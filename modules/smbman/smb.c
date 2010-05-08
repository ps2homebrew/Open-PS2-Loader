/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <stdio.h>
#include <sysclib.h>
#include <ps2ip.h>
#include <thbase.h>

#include "smb.h"
#include "lmauth.h"

#define SMB_MAGIC	0x424d53ff

struct SMBHeader_t {			//size = 36
	u32	sessionHeader;
	u32	Magic;
	u8	Cmd;
	short	Eclass;
	short	Ecode;
	u8	Flags;
	u16	Flags2;
	u8	Extra[12];
	u16	TID;
	u16	PID;
	u16	UID;
	u16	MID;
} __attribute__((packed));

struct NegociateProtocolRequest_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u16	ByteCount;		// 37
	u8	DialectFormat;		// 39
	char	DialectName[64];	// 40
} __attribute__((packed));

struct NegociateProtocolResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u16	DialectIndex;		// 37
	u8	SecurityMode;		// 39
	u16	MaxMpxCount;		// 40
	u16	MaxVC;			// 42
	u32	MaxBufferSize;		// 44
	u32	MaxRawBuffer;		// 48
	u32	SessionKey;		// 52
	u32	Capabilities;		// 56
	u8	SystemTime[8];		// 60
	u16	ServerTimeZone;		// 68
	u8	KeyLength;		// 70
	u16	ByteCount;		// 71
	u8	ByteField[1024];	// 73
} __attribute__((packed));

struct SessionSetupAndXRequest_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	MaxBufferSize;		// 41
	u16	MaxMpxCount;		// 43
	u16	VCNumber;		// 45
	u32	SessionKey;		// 47
	u16	AnsiPasswordLength;	// 51
	u16	UnicodePasswordLength;	// 53
	u32	reserved;		// 55
	u32	Capabilities;		// 59
	u16	ByteCount;		// 63
	u8	ByteField[1024];	// 65
} __attribute__((packed));

struct SessionSetupAndXResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	Action;			// 41
	u16	ByteCount;		// 43
	u8	ByteField[1024];	// 45
} __attribute__((packed));

struct TreeConnectRequest_t {
	u8	smbWordcount;
	u8	smbAndxCmd;
	u8	smbAndxReserved;
	u16	smbAndxOffset;
	u16	Flags;
	u16	PasswordLength;
	u16	ByteCount;
	u8	ByteField[1024];
} __attribute__((packed));

struct NTCreateAndXRequest_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u8	reserved;		// 41
	short	NameLength;		// 42
	u32	Flags;			// 44
	u32	RootDirectoryFid;	// 48
	u32	AccessMask;		// 52
	u32	AllocationSize;		// 56
	u32	AllocationSizeHigh;	// 60
	u32	FileAttributes;		// 64
	u32	ShareAccess;		// 68
	u32	CreateDisposition;	// 72
	u32	CreateOptions;		// 76
	u32	ImpersonationLevel;	// 80
	u8	SecurityFlags;		// 84
	u16	ByteCount;		// 85
	char	Name[1024];		// 87
} __attribute__((packed));

struct NTCreateAndXResponse_t {		// size = 107
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u8	OplockLevel;		// 41
	u16	FID;			// 42
	u32	Action;			// 44
	u8	CreationTime[8];	// 48
	u8	LastAccessTime[8];	// 56
	u8	LastWriteTime[8];	// 64
	u8	ChangedTime[8];		// 72
	u32	FileAttributes;		// 80
	u32	AllocationSize;		// 84
	u32	AllocationSizeHigh;	// 88
	u32	FileSize;		// 92
	u32	FileSizeHigh;		// 96
	u16	FileType;		// 100
	u16	IPCState;		// 102
	u8	IsDirectory;		// 104
	u16	ByteCount;		// 105
} __attribute__((packed));

struct OpenAndXRequest_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	Flags;			// 41
	u16	AccessMask;		// 43
	u16	SearchAttributes;	// 45
	u16	FileAttributes;		// 47
	u8	CreationTime[4];	// 49
	u16	CreateOptions;		// 53
	u32	AllocationSize;		// 55
	u32	reserved[2];		// 59
	u16	ByteCount;		// 67
	u8	Name[1024];		// 69
} __attribute__((packed));

struct OpenAndXResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	FID;			// 41
	u16	FileAttributes;		// 43
	u8	LastWriteTime[4];	// 45
	u32	FileSize;		// 49
	u16	GrantedAccess;		// 53
	u16	FileType;		// 55
	u16	IPCState;		// 57
	u16	Action;			// 59
	u32	ServerFID;		// 61
	u16	reserved;		// 65
	u16	ByteCount;		// 67
} __attribute__((packed));

struct ReadAndXRequest_t {		// size = 63
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	FID;			// 41
	u32	OffsetLow;		// 43
	u16	MaxCountLow;		// 47
	u16	MinCount;		// 49
	u32	MaxCountHigh;		// 51
	u16	Remaining;		// 55
	u32	OffsetHigh;		// 57
	u16	ByteCount;		// 61
} __attribute__((packed));

struct ReadAndXResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	Remaining;		// 41
	u16	DataCompactionMode;	// 43
	u16	reserved;		// 45
	u16	DataLengthLow;		// 47
	u16	DataOffset;		// 49
	u32	DataLengthHigh;		// 51
	u8	reserved2[6];		// 55
	u16	ByteCount;		// 61
} __attribute__((packed));

struct CloseRequest_t {			// size = 45
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u16	FID;			// 37
	u32	LastWrite;		// 39
	u16	ByteCount;		// 43
} __attribute__((packed));

struct CloseResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u16	ByteCount;		// 37
} __attribute__((packed));

typedef struct {
	u32	MaxBufferSize;
	u32	MaxMpxCount;
	u32	SessionKey;
	u32	StringsCF;
	u32	SupportsNTSMB;
	u8	PrimaryDomainServerName[64];
	u8	EncryptionKey[8];
	int	SecurityMode;		// 0 = share level, 1 = user level
	int	PasswordType;		// 0 = PlainText passwords, 1 = use challenge/response
} server_specs_t;

#define SERVER_SHARE_SECURITY_LEVEL	0
#define SERVER_USER_SECURITY_LEVEL	1
#define SERVER_USE_PLAINTEXT_PASSWORD	0
#define SERVER_USE_ENCRYPTED_PASSWORD	1

static server_specs_t server_specs;

struct ReadAndXRequest_t smb_Read_Request = {
	{	0x3b000000,
		SMB_MAGIC,
		SMB_COM_READ_ANDX,
		0, 0, 0, 0, "\0", 0, 0, 0, 0
	},
	12, 
	SMB_COM_NONE,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u16 UID, TID;
static int main_socket;

static u8 SMB_buf[MAX_SMB_BUF+1024] __attribute__((aligned(64)));

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
	size  = SMB_buf[3];
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
	sock = lwip_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		return -1;

    memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_addr = dst_IP;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(dst_port);

	while (1) {
		ret = lwip_connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
		if (ret >= 0)
			break;
		DelayThread(100);
	}

	return sock;
}

//-------------------------------------------------------------------------
int GetSMBServerReply(void)
{
	register int rcv_size, totalpkt_size, pkt_size;

	rcv_size = lwip_send(main_socket, SMB_buf, rawTCP_GetSessionHeader() + 4, 0);
	if (rcv_size <= 0)
		return -1;

	rcv_size = lwip_recv(main_socket, SMB_buf, sizeof(SMB_buf), 0);
	if (rcv_size <= 0)
		return -2;

	// Handle fragmented packets
	totalpkt_size = rawTCP_GetSessionHeader() + 4;

	while (rcv_size < totalpkt_size) {
		pkt_size = lwip_recv(main_socket, &SMB_buf[rcv_size], sizeof(SMB_buf) - rcv_size, 0);
		if (pkt_size <= 0)
			return -2;
		rcv_size += pkt_size;
	}

	return rcv_size;
}

//-------------------------------------------------------------------------
int smb_NegociateProtocol(char *SMBServerIP, int SMBServerPort, char *dialect)
{
	struct NegociateProtocolRequest_t *NPR = (struct NegociateProtocolRequest_t *)SMB_buf;
	register int length;
	struct in_addr dst_addr;

	dst_addr.s_addr = inet_addr(SMBServerIP);

conn_open:

	// Opening TCP session
	main_socket = OpenTCPSession(dst_addr, SMBServerPort);
	if (main_socket < 0)
		goto conn_open;

	memset(SMB_buf, 0, sizeof(SMB_buf));

	NPR->smbH.Magic = SMB_MAGIC;
	NPR->smbH.Cmd = SMB_COM_NEGOCIATE;
	NPR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
	NPR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
	length = strlen(dialect);
	NPR->ByteCount = length+2;
	NPR->DialectFormat = 0x02;
	strcpy(NPR->DialectName, dialect);

	rawTCP_SetSessionHeader(37+length);
	GetSMBServerReply();

	struct NegociateProtocolResponse_t *NPRsp = (struct NegociateProtocolResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (NPRsp->smbH.Magic != SMB_MAGIC)
		goto conn_close;

	// check there's no error
	if (NPRsp->smbH.Eclass != STATUS_SUCCESS)
		goto conn_close;

	if (NPRsp->smbWordcount != 17)
		goto conn_close;

	if (NPRsp->Capabilities & SERVER_CAP_UNICODE)
		server_specs.StringsCF = 2;
	else
		server_specs.StringsCF = 1;

	if (NPRsp->Capabilities & SERVER_CAP_NT_SMBS)
		server_specs.SupportsNTSMB = 1;
	else
		server_specs.SupportsNTSMB = 0;

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
	memcpy(&server_specs.EncryptionKey[0], &NPRsp->ByteField[0], NPRsp->KeyLength);
	memcpy(&server_specs.PrimaryDomainServerName[0], &NPRsp->ByteField[NPRsp->KeyLength], 64);

	return 1;

conn_close:	
	lwip_close(main_socket);
	goto conn_open;

	return 0;
}

//-------------------------------------------------------------------------
int smb_SessionSetupTreeConnect(char *User, char *Password, char *share_name)
{
	struct SessionSetupAndXRequest_t *SSR = (struct SessionSetupAndXRequest_t *)SMB_buf;
	register int i, offset, ss_size, CF;
	u8 LMpasswordhash[16];
	u8 *challenge;
	u8 LMresponse[24];
	u8 password[64];
	int passwordlen = 0;

	memset(SMB_buf, 0, sizeof(SMB_buf));

	CF = server_specs.StringsCF;

	SSR->smbH.Magic = SMB_MAGIC;
	SSR->smbH.Cmd = SMB_COM_SESSION_SETUP_ANDX;
	SSR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
	SSR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
	if (CF == 2)
		SSR->smbH.Flags2 |= SMB_FLAGS2_UNICODE_STRING;
	SSR->smbWordcount = 13;
	SSR->MaxBufferSize = (u16)server_specs.MaxBufferSize;
	SSR->MaxMpxCount = ((u16)server_specs.MaxMpxCount >= 2) ? 2 : (u16)server_specs.MaxMpxCount;
	SSR->VCNumber = 1;
	SSR->SessionKey = server_specs.SessionKey;
	SSR->Capabilities = CLIENT_CAP_LARGE_READX | CLIENT_CAP_UNICODE | CLIENT_CAP_LARGE_FILES | CLIENT_CAP_NT_SMBS | CLIENT_CAP_STATUS32;

	// Fill ByteField
	offset = 0;


	if (server_specs.SecurityMode == SERVER_USER_SECURITY_LEVEL) {

		if (server_specs.PasswordType == SERVER_USE_ENCRYPTED_PASSWORD) {
			challenge = (u8 *)server_specs.EncryptionKey;
			LM_Password_Hash(Password, LMpasswordhash);
			LM_Response(LMpasswordhash, challenge, LMresponse);
			passwordlen = 24;
			memcpy(password, LMresponse, passwordlen);
			SSR->AnsiPasswordLength = passwordlen;

			for (i = 0; i < passwordlen; i++) {
				SSR->ByteField[offset] = password[i];	// add password
				offset++;
			}
		}
		else if (server_specs.PasswordType == SERVER_USE_PLAINTEXT_PASSWORD) {
			strncpy(password, Password, 14);
			passwordlen = strlen(password);
			//if (CF == 2)					// It seems that PlainText passwords and Unicode isn't meant to be...
			//	SSR->UnicodePasswordLength = passwordlen * 2;
			//else
			SSR->AnsiPasswordLength = passwordlen;

			memcpy(&SSR->ByteField[offset], password, passwordlen);
			offset += passwordlen;
			//for (i = 0; i < passwordlen; i++) {
			//	SSR->ByteField[offset] = password[i];	// add password
			//	offset += 1; //CF;
			//}
		}
	}	

	if (CF == 2)
		offset += 1;				// pad needed only for unicode as aligment fix

	for (i = 0; i < strlen(User); i++) {
		SSR->ByteField[offset] = User[i]; 	// add User name
		offset += CF;
	}
	offset += CF;					// null terminator

	for (i = 0; server_specs.PrimaryDomainServerName[i] != 0; i+=CF) {
		SSR->ByteField[offset] = server_specs.PrimaryDomainServerName[i]; // PrimaryDomain, acquired from Negociate Protocol Response Datas
		offset += CF;
	}
	offset += CF;					// null terminator

	for (i = 0; i < (CF << 1); i++)
		SSR->ByteField[offset++] = 0;		// NativeOS, NativeLanMan	

	SSR->ByteCount = offset;

	SSR->smbAndxCmd = SMB_COM_TREE_CONNECT_ANDX;
	SSR->smbAndxOffset = 61 + offset;
	ss_size = offset;

	struct TreeConnectRequest_t *TCR = (struct TreeConnectRequest_t *)&SSR->ByteField[offset];
	TCR->smbWordcount = 4;
	TCR->smbAndxCmd = SMB_COM_NONE;
	TCR->PasswordLength = 1;

	offset = 1;
	for (i = 0; i < strlen(share_name); i++) {
		TCR->ByteField[offset] = share_name[i];	// add Share name
		offset += CF;
	}
	offset += CF;					// null terminator

	memcpy(&TCR->ByteField[offset], "?????\0", 6); 	// Service, any type of device
	offset += 6;

	TCR->ByteCount = offset;

	rawTCP_SetSessionHeader(72+ss_size+offset);
	GetSMBServerReply();

	struct SessionSetupAndXResponse_t *SSRsp = (struct SessionSetupAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (SSRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error (NT STATUS error type!)
	if ((SSRsp->smbH.Eclass | SSRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -1000;

	// keep UID & TID
	UID = SSRsp->smbH.UID;
	TID = SSRsp->smbH.TID;

	return 1;
}

//-------------------------------------------------------------------------
int smb_NTCreateAndX(char *filename, u16 *FID, u32 *filesize)
{
	struct NTCreateAndXRequest_t *NTCR = (struct NTCreateAndXRequest_t *)SMB_buf;
	register int length;

	memset(SMB_buf, 0, sizeof(SMB_buf));

	NTCR->smbH.Magic = SMB_MAGIC;
	NTCR->smbH.Cmd = SMB_COM_NT_CREATE_ANDX;
	NTCR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES; //| SMB_FLAGS_CASELESS_PATHNAMES;
	NTCR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	NTCR->smbH.UID = UID;
	NTCR->smbH.TID = TID;
	NTCR->smbWordcount = 24;
	NTCR->smbAndxCmd = SMB_COM_NONE;	// no ANDX command
	length = strlen(filename);
	NTCR->NameLength = length;
	NTCR->AccessMask = 0x20089;
	NTCR->FileAttributes = EXT_ATTR_READONLY;
	NTCR->ShareAccess = 1;
	NTCR->CreateDisposition = 1;
	NTCR->ImpersonationLevel = 2;
	NTCR->SecurityFlags = 0x03;
	NTCR->ByteCount = length+1;
	strcpy(NTCR->Name, filename);

	rawTCP_SetSessionHeader(84+length);
	GetSMBServerReply();

	struct NTCreateAndXResponse_t *NTCRsp = (struct NTCreateAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (NTCRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if ((NTCRsp->smbH.Eclass | NTCRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -1000;

	*FID = NTCRsp->FID;
	*filesize = NTCRsp->FileSize;

	smb_Read_Request.smbH.UID = UID;
	smb_Read_Request.smbH.TID = TID;

	return 1;
}

//-------------------------------------------------------------------------
int smb_OpenAndX(char *filename, u16 *FID, u32 *filesize)
{
	struct OpenAndXRequest_t *OR = (struct OpenAndXRequest_t *)SMB_buf;
	register int length;

	if (server_specs.SupportsNTSMB)
		return smb_NTCreateAndX(filename, FID, filesize);

	memset(SMB_buf, 0, sizeof(SMB_buf));

	OR->smbH.Magic = SMB_MAGIC;
	OR->smbH.Cmd = SMB_COM_OPEN_ANDX;
	OR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES; //| SMB_FLAGS_CASELESS_PATHNAMES;
	OR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	OR->smbH.UID = UID;
	OR->smbH.TID = TID;
	OR->smbWordcount = 15;
	OR->smbAndxCmd = SMB_COM_NONE;		// no ANDX command
	OR->FileAttributes = EXT_ATTR_READONLY;
	OR->CreateOptions = 1;
	length = strlen(filename);
	OR->ByteCount = length+1;
	strcpy((char *)OR->Name, filename);
	
	rawTCP_SetSessionHeader(66+length);
	GetSMBServerReply();

	struct OpenAndXResponse_t *ORsp = (struct OpenAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (ORsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if (ORsp->smbH.Eclass != STATUS_SUCCESS)
		return -1000;

	*FID = ORsp->FID;
	*filesize = ORsp->FileSize;

	// Prepare header of Read Request by advance
	smb_Read_Request.smbH.UID = UID;
	smb_Read_Request.smbH.TID = TID;

	return 1;
}

//-------------------------------------------------------------------------
int smb_ReadAndX(u16 FID, u32 fileoffset, void *readbuf, u16 nbytes) // Builds a Read AndX Request message
{
	struct ReadAndXRequest_t *RR = (struct ReadAndXRequest_t *)SMB_buf;
	register int r;

	memcpy(RR, &smb_Read_Request.smbH.sessionHeader, sizeof(struct ReadAndXRequest_t));

	RR->FID = FID;
	RR->OffsetLow = fileoffset;
	RR->MaxCountLow = nbytes;

	GetSMBServerReply();

	struct ReadAndXResponse_t *RRsp = (struct ReadAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (RRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if (RRsp->smbH.Eclass != STATUS_SUCCESS)
		return -1000;

	r = RRsp->DataLengthLow;

	if (RRsp->DataOffset > 0)
		memcpy(readbuf, &SMB_buf[4 + RRsp->DataOffset], r);

	return r;
}

//-------------------------------------------------------------------------
int smb_Close(u16 FID)
{
	struct CloseRequest_t *CR = (struct CloseRequest_t *)SMB_buf;

	memset(SMB_buf, 0, sizeof(SMB_buf));

	CR->smbH.Magic = SMB_MAGIC;
	CR->smbH.Cmd = SMB_COM_CLOSE;
	CR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES; //| SMB_FLAGS_CASELESS_PATHNAMES;
	CR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	CR->smbH.UID = UID;
	CR->smbH.TID = TID;
	CR->smbWordcount = 3;
	CR->FID = FID;

	rawTCP_SetSessionHeader(41);
	GetSMBServerReply();

	struct CloseResponse_t *CRsp = (struct CloseResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (CRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if (CRsp->smbH.Eclass != STATUS_SUCCESS)
		return -1000;

	return 1;
}

//-------------------------------------------------------------------------
int smb_Disconnect(void)
{
	lwip_close(main_socket);

	return 1;
}
