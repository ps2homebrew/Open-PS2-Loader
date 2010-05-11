/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <stdio.h>
#include <sysclib.h>
#include <ps2ip.h>
#include <thbase.h>
#include <io_common.h>

#include "smb.h"
#include "auth.h"

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

struct SMBTransactionRequest_t {	//size = 28
	u16	TotalParamCount;
	u16	TotalDataCount;
	u16	MaxParamCount;
	u16	MaxDataCount;
	u8	MaxSetupCount;
	u8	Reserved;
	u16	Flag;
	u32	Timeout;
	u16	Reserved2;
	u16	ParamCount;
	u16	ParamOffset;
	u16	DataCount;
	u16	DataOffset;
	u8	SetupCount;
	u8	Reserved3;
} __attribute__((packed));

struct SMBTransactionResponse_t {	//size = 20
	u16	TotalParamCount;
	u16	TotalDataCount;
	u16	Reserved;
	u16	ParamCount;
	u16	ParamOffset;
	u16	ParamDisplacement;
	u16	DataCount;
	u16	DataOffset;
	u16	DataDisplacement;
	u8	SetupCount;
	u8	Reserved2;
} __attribute__((packed));

struct NegociateProtocolRequest_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u16	ByteCount;		// 37
	u8	DialectFormat;		// 39
	char	DialectName[0];		// 40
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
	u8	ByteField[0];		// 73
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
	u8	ByteField[0];		// 65
} __attribute__((packed));

struct SessionSetupAndXResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	Action;			// 41
	u16	ByteCount;		// 43
} __attribute__((packed));

struct TreeConnectAndXRequest_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	Flags;			// 41
	u16	PasswordLength;		// 43
	u16	ByteCount;		// 45
	u8	ByteField[0];		// 47
} __attribute__((packed));

struct TreeConnectAndXResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	OptionalSupport;	// 41
	u16	ByteCount;		// 43
} __attribute__((packed));

struct TreeDisconnectRequest_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u16	ByteCount;		// 37
} __attribute__((packed));

struct TreeDisconnectResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u16	ByteCount;		// 37
} __attribute__((packed));

struct NetShareEnumRequest_t {
	struct SMBHeader_t smbH;			// 0
	u8	smbWordcount;				// 36
	struct SMBTransactionRequest_t smbTrans;	// 37
	u16	ByteCount;				// 65
	u8	ByteField[0];				// 67
} __attribute__((packed));

struct NetShareEnumResponse_t {
	struct SMBHeader_t smbH;			// 0
	u8	smbWordcount;				// 36
	struct SMBTransactionResponse_t smbTrans; 	// 37
	u16	ByteCount;				// 57
	u8	ByteField[0];				// 59
} __attribute__((packed));

struct LogOffAndXRequest_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	ByteCount;		// 41
} __attribute__((packed));

struct LogOffAndXResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	ByteCount;		// 41
} __attribute__((packed));

struct FindFirst2Request_t {
	struct SMBHeader_t smbH;			// 0
	u8	smbWordcount;				// 36
	struct SMBTransactionRequest_t smbTrans;	// 37
	u16	SubCommand;
	u16	ByteCount;				// 65
	u8	Padding[3];
	u16	SearchAttributes;
	u16	SearchCount;
	u16	Flags;
	u16	LevelOfInterest;
	u32	StorageType;
	u8	SearchPattern[0];			// 67
} __attribute__((packed));

struct FindFirst2Response_t {
	struct SMBHeader_t smbH;			// 0
	u8	smbWordcount;				// 36
	struct SMBTransactionResponse_t smbTrans; 	// 37
	u16	ByteCount;				// 57
/*
	u8	Padding[1];
	u16	SearchID;
	u16	SearchCount;
	u16	EndOfSearch;
	u16	EAErrorOffset;
	u16	LastNameOffset;
	u8	Padding2[2];
	u32	ResumeKey;
	u32	Created;
	u32	LastAccess;
	u32	LastWrite;
	u32	DataSize;
	u32 	AllocationSize;
	u16 	FileAttributes;
	u8	FileNameLen;
	u8	FileName[0];				// 59
*/
} __attribute__((packed));

struct FindNext2Request_t {
	struct SMBHeader_t smbH;			// 0
	u8	smbWordcount;				// 36
	struct SMBTransactionRequest_t smbTrans;	// 37
	u16	SubCommand;
	u16	ByteCount;				// 65
} __attribute__((packed));

struct FindNext2Response_t {
	struct SMBHeader_t smbH;			// 0
	u8	smbWordcount;				// 36
	struct SMBTransactionResponse_t smbTrans; 	// 37
	u16	ByteCount;				// 57
/*
	u8	Padding[1];
	u16	SearchID;
	u16	SearchCount;
	u16	EndOfSearch;
	u16	EAErrorOffset;
	u16	LastNameOffset;
	u8	Padding2[2];
	u32	ResumeKey;
	u32	Created;
	u32	LastAccess;
	u32	LastWrite;
	u32	DataSize;
	u32 	AllocationSize;
	u16 	FileAttributes;
	u8	FileNameLen;
	u8	FileName[0];				// 59
*/
} __attribute__((packed));

/*
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
	char	Name[0];		// 87
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
*/

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
	u8	Name[0];		// 69
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

struct WriteAndXRequest_t {		// size = 63
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	FID;			// 41
	u32	OffsetLow;		// 43
	u32	Reserved;		// 47
	u16	WriteMode;		// 51
	u16	Remaining;		// 53
	u16	DataLengthHigh;		// 55
	u16	DataLengthLow;		// 57
	u16	DataOffset;		// 59
	u16	ByteCount;		// 61
} __attribute__((packed));

struct WriteAndXResponse_t {
	struct SMBHeader_t smbH;	// 0
	u8	smbWordcount;		// 36
	u8	smbAndxCmd;		// 37
	u8	smbAndxReserved;	// 38
	u16	smbAndxOffset;		// 39
	u16	Count;			// 41
	u16	Remaining;		// 43
	u16	CountHigh;		// 45
	u16	Reserved;		// 47
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
	char	ServerIP[16];
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

struct WriteAndXRequest_t smb_Write_Request = {
	{	0,
		SMB_MAGIC,
		SMB_COM_WRITE_ANDX,
		0, 0, 0, 0, "\0", 0, 0, 0, 0
	},
	12, 
	SMB_COM_NONE,
	0, 0, 0, 0, 0, 0x01, 0, 0, 0, 0x3b, 0 	// 0x01 is WriteThrough mode and 0x3b is DataOffset
};

#define LM_AUTH 	0
#define NTLM_AUTH 	1

static int UID = -1; 
static int TID = -1;
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

	strncpy(server_specs.ServerIP, SMBServerIP, 16);

	return 0;

conn_close:	
	lwip_close(main_socket);
	goto conn_open;

	return -1;
}

//-------------------------------------------------------------------------
int smb_SessionSetupAndX(char *User, char *Password, int PasswordType)
{
	struct SessionSetupAndXRequest_t *SSR = (struct SessionSetupAndXRequest_t *)SMB_buf;
	register int i, offset, CF;
	u8 passwordhash[16];
	u8 *challenge;
	u8 LMresponse[24];
	u8 password[64];
	int passwordlen = 0;
	int AuthType = NTLM_AUTH;

	if (UID != -1)
		return -3;

lbl_session_setup:

	memset(SMB_buf, 0, sizeof(SMB_buf));

	CF = server_specs.StringsCF;

	SSR->smbH.Magic = SMB_MAGIC;
	SSR->smbH.Cmd = SMB_COM_SESSION_SETUP_ANDX;
	SSR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
	SSR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
	if (CF == 2)
		SSR->smbH.Flags2 |= SMB_FLAGS2_UNICODE_STRING;
	SSR->smbWordcount = 13;
	SSR->smbAndxCmd = SMB_COM_NONE;		// no ANDX command
	SSR->MaxBufferSize = (u16)server_specs.MaxBufferSize;
	SSR->MaxMpxCount = ((u16)server_specs.MaxMpxCount >= 2) ? 2 : (u16)server_specs.MaxMpxCount;
	SSR->VCNumber = 1;
	SSR->SessionKey = server_specs.SessionKey;
	SSR->Capabilities = CLIENT_CAP_LARGE_READX | CLIENT_CAP_UNICODE | CLIENT_CAP_LARGE_FILES | CLIENT_CAP_NT_SMBS | CLIENT_CAP_STATUS32;

	// Fill ByteField
	offset = 0;

	if ((server_specs.SecurityMode == SERVER_USER_SECURITY_LEVEL) && (Password)) {

		if (server_specs.PasswordType == SERVER_USE_ENCRYPTED_PASSWORD) {
			challenge = (u8 *)server_specs.EncryptionKey;
			passwordlen = 24;
			if (PasswordType == 0) {
				if (AuthType == LM_AUTH) {
					LM_Password_Hash(Password, passwordhash);
					SSR->AnsiPasswordLength = passwordlen;
				}
				else if (AuthType == NTLM_AUTH) {
					NTLM_Password_Hash(Password, passwordhash);
					SSR->UnicodePasswordLength = passwordlen;
				}
			}
			else if (PasswordType == 1) {
				if (AuthType == LM_AUTH) {
					memcpy(passwordhash, &Password[0], 16);
					SSR->AnsiPasswordLength = passwordlen;
				}
				if (AuthType == NTLM_AUTH) {
					memcpy(passwordhash, &Password[16], 16);
					SSR->UnicodePasswordLength = passwordlen;
				}
			}
			LM_Response(passwordhash, challenge, LMresponse);			

			for (i = 0; i < passwordlen; i++) {
				SSR->ByteField[offset] = LMresponse[i];	// add password
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

	if ((CF == 2) && (!(passwordlen & 1)))
		offset += 1;				// pad needed only for unicode as aligment fix if password length is even

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

	rawTCP_SetSessionHeader(61+offset);
	GetSMBServerReply();

	struct SessionSetupAndXResponse_t *SSRsp = (struct SessionSetupAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (SSRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no auth failure
	if ((server_specs.SecurityMode == SERVER_USER_SECURITY_LEVEL)
		&& ((SSRsp->smbH.Eclass | (SSRsp->smbH.Ecode << 16)) == STATUS_LOGON_FAILURE)
		&& (AuthType == NTLM_AUTH)) {
		AuthType = LM_AUTH;
		goto lbl_session_setup;
	}

	// check there's no error (NT STATUS error type!)
	if ((SSRsp->smbH.Eclass | SSRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -2;

	// keep UID
	UID = (int)SSRsp->smbH.UID;

	return 0;
}

//-------------------------------------------------------------------------
int smb_TreeConnectAndX(char *ShareName, char *Password, int PasswordType) // PasswordType: 0 = PlainText, 1 = Hash
{
	struct TreeConnectAndXRequest_t *TCR = (struct TreeConnectAndXRequest_t *)SMB_buf;
	register int i, offset, CF;
	u8 passwordhash[16];
	u8 *challenge;
	u8 LMresponse[24];
	u8 password[64];
	int passwordlen = 0;
	int AuthType = NTLM_AUTH;

	if (TID != -1)
		return -3;

lbl_tree_connect:

	memset(SMB_buf, 0, sizeof(SMB_buf));

	CF = server_specs.StringsCF;

	TCR->smbH.Magic = SMB_MAGIC;
	TCR->smbH.Cmd = SMB_COM_TREE_CONNECT_ANDX;
	TCR->smbH.Flags = SMB_FLAGS_CASELESS_PATHNAMES;
	TCR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_32BIT_STATUS;
	if (CF == 2)
		TCR->smbH.Flags2 |= SMB_FLAGS2_UNICODE_STRING;
	TCR->smbH.UID = UID;
	TCR->smbH.TID = TID;
	TCR->smbWordcount = 4;
	TCR->smbAndxCmd = SMB_COM_NONE;		// no ANDX command

	// Fill ByteField
	offset = 0;

	if (server_specs.SecurityMode == SERVER_SHARE_SECURITY_LEVEL) {

		if (Password) {

			if (server_specs.PasswordType == SERVER_USE_ENCRYPTED_PASSWORD) {
				challenge = (u8 *)server_specs.EncryptionKey;
				passwordlen = 24;
				if (PasswordType == 0) {
					if (AuthType == LM_AUTH)
						LM_Password_Hash(Password, passwordhash);
					else if (AuthType == NTLM_AUTH)
						NTLM_Password_Hash(Password, passwordhash);
				}
				else if (PasswordType == 1) {
					if (AuthType == LM_AUTH)
						memcpy(passwordhash, &Password[0], 16);
					if (AuthType == NTLM_AUTH)
						memcpy(passwordhash, &Password[16], 16);
				}
				LM_Response(passwordhash, challenge, LMresponse);

				for (i = 0; i < passwordlen; i++) {
					TCR->ByteField[offset] = LMresponse[i];	// add password
					offset++;
				}
			}
			else if (server_specs.PasswordType == SERVER_USE_PLAINTEXT_PASSWORD) {
				strncpy(password, Password, 14);
				passwordlen = strlen(password);
				//if (CF == 2)					// It seems that PlainText passwords and Unicode isn't meant to be...
				//	TCR->PasswordLength = passwordlen * 2;
				//else

				memcpy(&TCR->ByteField[offset], password, passwordlen);
				offset += passwordlen;
				//for (i = 0; i < passwordlen; i++) {
				//	TCR->ByteField[offset] = password[i];	// add password
				//	offset += 1; //CF;
				//}
			}
		}
		else {
			passwordlen = 1;
			offset += passwordlen;
		}
		TCR->PasswordLength = passwordlen;
	}	

	if ((CF == 2) && (!(passwordlen & 1)))
		offset += 1;				// pad needed only for unicode as aligment fix is password len is even

	for (i = 0; i < strlen(ShareName); i++) {
		TCR->ByteField[offset] = ShareName[i];	// add Share name
		offset += CF;
	}
	offset += CF;					// null terminator

	memcpy(&TCR->ByteField[offset], "?????\0", 6); 	// Service, any type of device
	offset += 6;

	TCR->ByteCount = offset;

	rawTCP_SetSessionHeader(43+offset);
	GetSMBServerReply();

	struct TreeConnectAndXResponse_t *TCRsp = (struct TreeConnectAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (TCRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no auth failure
	if ((server_specs.SecurityMode == SERVER_USER_SECURITY_LEVEL)
		&& ((TCRsp->smbH.Eclass | (TCRsp->smbH.Ecode << 16)) == STATUS_LOGON_FAILURE)
		&& (AuthType == NTLM_AUTH)) {
		AuthType = LM_AUTH;
		goto lbl_tree_connect;
	}

	// check there's no error (NT STATUS error type!)
	if ((TCRsp->smbH.Eclass | TCRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -2;

	// keep TID
	TID = (int)TCRsp->smbH.TID;

	return 0;
}

//-------------------------------------------------------------------------
int smb_NetShareEnum(void)
{
	struct NetShareEnumRequest_t *NSER = (struct NetShareEnumRequest_t *)SMB_buf;

	if (TID == -1)
		return -3;

	memset(SMB_buf, 0, sizeof(SMB_buf));

	NSER->smbH.Magic = SMB_MAGIC;
	NSER->smbH.Cmd = SMB_COM_TRANSACTION;
	NSER->smbH.UID = UID;
	NSER->smbH.TID = TID;
	NSER->smbWordcount = 14;
	
	NSER->smbTrans.TotalParamCount = 19;
	NSER->smbTrans.MaxParamCount = 1024;
	NSER->smbTrans.TotalParamCount = 19;
	NSER->smbTrans.MaxDataCount = 8096;
	NSER->smbTrans.ParamCount = 19;
	NSER->smbTrans.ParamOffset = 76;
	NSER->smbTrans.DataOffset = 95;

	NSER->ByteCount = 32;

	memcpy(&NSER->ByteField[0], "\\PIPE\\LANMAN\0\0\0WrLeh\0B13BWz\0\x01\0\xa0\x1f", 32);

	rawTCP_SetSessionHeader(95);
	GetSMBServerReply();

	struct NetShareEnumResponse_t *NSERsp = (struct NetShareEnumResponse_t *)SMB_buf;


	// check sanity of SMB header
	if (NSERsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if ((NSERsp->smbH.Eclass | NSERsp->smbH.Ecode) != STATUS_SUCCESS)
		return -2;

	// API status must be 0
	if (*((u16 *)&SMB_buf[NSERsp->smbTrans.ParamOffset+4]) != 0)
		return -1;

	// Available Entries
	//*((u16 *)&SMB_buf[NSERsp->smbTrans.ParamOffset+4+6])

	// first share name at &SMB_buf[NSERsp->smbTrans.DataOffset+4]

	return 0;
}

/*
//-------------------------------------------------------------------------
int smb_NTCreateAndX(char *filename, int *FID, u32 *filesize, int mode)
{
	struct NTCreateAndXRequest_t *NTCR = (struct NTCreateAndXRequest_t *)SMB_buf;
	register int length;

	memset(SMB_buf, 0, sizeof(SMB_buf));

	NTCR->smbH.Magic = SMB_MAGIC;
	NTCR->smbH.Cmd = SMB_COM_NT_CREATE_ANDX;
	NTCR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES; //| SMB_FLAGS_CASELESS_PATHNAMES;
	NTCR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	NTCR->smbH.UID = (u16)UID;
	NTCR->smbH.TID = (u16)TID;
	NTCR->smbWordcount = 24;
	NTCR->smbAndxCmd = SMB_COM_NONE;	// no ANDX command
	length = strlen(filename);
	NTCR->NameLength = length;
	NTCR->AccessMask = ((mode & O_RDWR) || (mode & O_WRONLY)) ? 0x2019f : 0x20089;
	NTCR->FileAttributes = ((mode & O_RDWR) || (mode & O_WRONLY)) ? EXT_ATTR_NORMAL : EXT_ATTR_READONLY;
	NTCR->ShareAccess = ((mode & O_RDWR) || (mode & O_WRONLY)) ? 0x03 : 0x01;
	NTCR->CreateDisposition = (mode & O_CREAT) ? 0x05 : 0x01;
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

	// check if access denied
	if ((NTCRsp->smbH.Eclass | (NTCRsp->smbH.Ecode << 16)) == STATUS_ACCESS_DENIED)
		return -2;

	// check there's no error
	if ((NTCRsp->smbH.Eclass | NTCRsp->smbH.Ecode) != STATUS_SUCCESS)
		return (NTCRsp->smbH.Eclass | (NTCRsp->smbH.Ecode << 16));

	*FID = (int)NTCRsp->FID;
	*filesize = NTCRsp->FileSize;

	smb_Read_Request.smbH.UID = (u16)UID;
	smb_Read_Request.smbH.TID = (u16)TID;

	return 0;
}
*/

//-------------------------------------------------------------------------
int smb_OpenAndX(char *filename, int *FID, u32 *filesize, int mode)
{
	struct OpenAndXRequest_t *OR = (struct OpenAndXRequest_t *)SMB_buf;
	register int length;

	//if (server_specs.SupportsNTSMB)
	//	return smb_NTCreateAndX(filename, FID, filesize, mode);

	memset(SMB_buf, 0, sizeof(SMB_buf));

	OR->smbH.Magic = SMB_MAGIC;
	OR->smbH.Cmd = SMB_COM_OPEN_ANDX;
	OR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES; //| SMB_FLAGS_CASELESS_PATHNAMES;
	OR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	OR->smbH.UID = (u16)UID;
	OR->smbH.TID = (u16)TID;
	OR->smbWordcount = 15;
	OR->smbAndxCmd = SMB_COM_NONE;		// no ANDX command
	OR->AccessMask = (!(mode & O_RDONLY) || (mode & O_WRONLY)) ? 0x02 : 0x00;
	OR->FileAttributes = (!(mode & O_RDONLY) || (mode & O_WRONLY)) ? EXT_ATTR_NORMAL : EXT_ATTR_READONLY;
	if (mode & O_CREAT)
		OR->CreateOptions |= 0x10;
	if (mode & O_TRUNC)
		OR->CreateOptions |= 0x02;
	else
		OR->CreateOptions |= 0x01;
	length = strlen(filename);
	OR->ByteCount = length+1;
	strcpy((char *)OR->Name, filename);
	
	rawTCP_SetSessionHeader(66+length);
	GetSMBServerReply();

	struct OpenAndXResponse_t *ORsp = (struct OpenAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (ORsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check if access denied
	if ((ORsp->smbH.Eclass | (ORsp->smbH.Ecode << 16)) == STATUS_ACCESS_DENIED)
		return -2;

	// check if access denied
	if ((ORsp->smbH.Eclass | (ORsp->smbH.Ecode << 16)) == STATUS_OBJECT_NAME_NOT_FOUND)
		return -3;

	// check there's no error
	if ((ORsp->smbH.Eclass | ORsp->smbH.Ecode) != STATUS_SUCCESS)
		return (ORsp->smbH.Eclass | (ORsp->smbH.Ecode << 16));

	*FID = (int)ORsp->FID;
	*filesize = ORsp->FileSize;

	// Prepare header of Read/Write Requests by advance
	smb_Read_Request.smbH.UID = (u16)UID;
	smb_Read_Request.smbH.TID = (u16)TID;
	smb_Write_Request.smbH.UID = (u16)UID;
	smb_Write_Request.smbH.TID = (u16)TID;

	return 0;
}

//-------------------------------------------------------------------------
int smb_ReadAndX(int FID, u32 fileoffset, void *readbuf, u16 nbytes)
{
	struct ReadAndXRequest_t *RR = (struct ReadAndXRequest_t *)SMB_buf;
	register int r;

	if (FID == -1)
		return -3;

	memcpy(RR, &smb_Read_Request.smbH.sessionHeader, sizeof(struct ReadAndXRequest_t));

	RR->FID = (u16)FID;
	RR->OffsetLow = fileoffset;
	RR->MaxCountLow = nbytes;

	GetSMBServerReply();

	struct ReadAndXResponse_t *RRsp = (struct ReadAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (RRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if ((RRsp->smbH.Eclass | RRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -2;

	r = RRsp->DataLengthLow;

	if (RRsp->DataOffset > 0)
		memcpy(readbuf, &SMB_buf[4 + RRsp->DataOffset], r);

	return r;
}

//-------------------------------------------------------------------------
int smb_WriteAndX(int FID, u32 fileoffset, void *writebuf, u16 nbytes)
{
	struct WriteAndXRequest_t *WR = (struct WriteAndXRequest_t *)SMB_buf;

	if (FID == -1)
		return -3;

	memcpy(WR, &smb_Write_Request.smbH.sessionHeader, sizeof(struct WriteAndXRequest_t));

	WR->FID = (u16)FID;
	WR->OffsetLow = fileoffset;
	WR->Remaining = nbytes;
	WR->DataLengthLow = nbytes;
	WR->ByteCount = nbytes;

	memcpy((void *)(&SMB_buf[4 + WR->DataOffset]), writebuf, nbytes);

	rawTCP_SetSessionHeader(59+nbytes);
	GetSMBServerReply();

	struct WriteAndXResponse_t *WRsp = (struct WriteAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (WRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if ((WRsp->smbH.Eclass | WRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -2;

	return nbytes;
}

//-------------------------------------------------------------------------
int smb_Close(int FID)
{
	struct CloseRequest_t *CR = (struct CloseRequest_t *)SMB_buf;

	if (FID == -1)
		return -3;

	memset(SMB_buf, 0, sizeof(SMB_buf));

	CR->smbH.Magic = SMB_MAGIC;
	CR->smbH.Cmd = SMB_COM_CLOSE;
	CR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES; //| SMB_FLAGS_CASELESS_PATHNAMES;
	CR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	CR->smbH.UID = (u16)UID;
	CR->smbH.TID = (u16)TID;
	CR->smbWordcount = 3;
	CR->FID = (u16)FID;

	rawTCP_SetSessionHeader(41);
	GetSMBServerReply();

	struct CloseResponse_t *CRsp = (struct CloseResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (CRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if ((CRsp->smbH.Eclass | CRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -2;

	return 0;
}

//-------------------------------------------------------------------------
int smb_TreeDisconnect(void)
{
	struct TreeDisconnectRequest_t *TDR = (struct TreeDisconnectRequest_t *)SMB_buf;

	if (TID == -1)
		return -3;

	memset(SMB_buf, 0, sizeof(SMB_buf));

	TDR->smbH.Magic = SMB_MAGIC;
	TDR->smbH.Cmd = SMB_COM_TREE_DISCONNECT;
	TDR->smbH.TID = (u16)TID;

	rawTCP_SetSessionHeader(35);
	GetSMBServerReply();

	struct TreeDisconnectResponse_t *TDRsp = (struct TreeDisconnectResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (TDRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if ((TDRsp->smbH.Eclass | TDRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -2;

	TID = -1;

	return 0;
}

//-------------------------------------------------------------------------
int smb_LogOffAndX(void)
{
	struct LogOffAndXRequest_t *LR = (struct LogOffAndXRequest_t *)SMB_buf;

	if (UID == -1)
		return -3;

	if (!(TID == -1))
		smb_TreeDisconnect();

	memset(SMB_buf, 0, sizeof(SMB_buf));

	LR->smbH.Magic = SMB_MAGIC;
	LR->smbH.Cmd = SMB_COM_LOGOFF_ANDX;
	LR->smbH.UID = (u16)UID;
	LR->smbWordcount = 2;
	LR->smbAndxCmd = SMB_COM_NONE;		// no ANDX command

	rawTCP_SetSessionHeader(39);
	GetSMBServerReply();

	struct LogOffAndXResponse_t *LRsp = (struct LogOffAndXResponse_t *)SMB_buf;

	// check sanity of SMB header
	if (LRsp->smbH.Magic != SMB_MAGIC)
		return -1;

	// check there's no error
	if ((LRsp->smbH.Eclass | LRsp->smbH.Ecode) != STATUS_SUCCESS)
		return -2;

	UID = -1;

	return 0;
}

//-------------------------------------------------------------------------
int smb_Disconnect(void)
{
	lwip_close(main_socket);

	return 0;
}
