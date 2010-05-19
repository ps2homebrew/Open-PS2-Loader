/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <stdio.h>
#include <sysclib.h>
#include <ps2ip.h>
#include <thbase.h>

#include "smsutils.h"
#include "smb.h"

// !!! ps2ip exports functions pointers !!!
extern int (*plwip_close)(int s); 						// #6
extern int (*plwip_connect)(int s, struct sockaddr *name, socklen_t namelen); 	// #7
extern int (*plwip_recv)(int s, void *mem, int len, unsigned int flags);	// #9
extern int (*plwip_send)(int s, void *dataptr, int size, unsigned int flags); 	// #11
extern int (*plwip_socket)(int domain, int type, int protocol); 		// #13
extern u32 (*pinet_addr)(const char *cp); 					// #24

extern int *p_part_start;

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
	u8	EncryptionKey[8];	// 73
	u8	PrimaryDomainServerName[1024]; // 81
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

typedef struct {
	u32	MaxBufferSize;
	u32	MaxMpxCount;
	u32	SessionKey;
	u32	StringsCF;
	u8	PrimaryDomainServerName[32];
} server_specs_t;

static server_specs_t server_specs;

struct ReadAndXRequest_t smb_Read_Request = {
	{	0,
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
	rcv_size = plwip_recv(main_socket, SMB_buf, sizeof(SMB_buf), 0);
	if (rcv_size <= 0)
		return -2;

	if (SMB_buf[0] != 0)	// dropping NBSS Session Keep alive
		goto receive;

	// Handle fragmented packets
	totalpkt_size = rawTCP_GetSessionHeader() + 4;

	while (rcv_size < totalpkt_size) {
		pkt_size = plwip_recv(main_socket, &SMB_buf[rcv_size], sizeof(SMB_buf) - rcv_size, 0);
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
	NPR->ByteCount = length+2;
	NPR->DialectFormat = 0x02;
	strcpy(NPR->DialectName, dialect);

	rawTCP_SetSessionHeader(37+length);
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

	if (NPRsp->Capabilities & SERVER_CAP_UNICODE)
		server_specs.StringsCF = 2;
	else
		server_specs.StringsCF = 1;

	// copy to global struct to keep needed information for further communication
	server_specs.MaxBufferSize = NPRsp->MaxBufferSize;
	server_specs.MaxMpxCount = NPRsp->MaxMpxCount;
	server_specs.SessionKey = NPRsp->SessionKey;
	mips_memcpy(&server_specs.PrimaryDomainServerName[0], &NPRsp->PrimaryDomainServerName[0], 32);

	return 1;
}

//-------------------------------------------------------------------------
int smb_SessionSetupTreeConnect(char *User, char *share_name)
{
	struct SessionSetupAndXRequest_t *SSR = (struct SessionSetupAndXRequest_t *)SMB_buf;
	register int i, offset, ss_size, CF;

	mips_memset(SMB_buf, 0, sizeof(SMB_buf));

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
	SSR->Capabilities = CLIENT_CAP_LARGE_READX | CLIENT_CAP_UNICODE | CLIENT_CAP_LARGE_FILES | CLIENT_CAP_STATUS32;

	// Fill ByteField
	offset = 1;					// skip AnsiPassword
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
		TCR->ByteField[offset] = share_name[i]; // add Share name
		offset += CF;
	}
	offset += CF;					// null terminator

	mips_memcpy(&TCR->ByteField[offset], "?????\0", 6); // Service, any type of device
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
int smb_OpenAndX(char *filename, u16 *FID)
{
	struct OpenAndXRequest_t *OR = (struct OpenAndXRequest_t *)SMB_buf;
	register int length;

	mips_memset(SMB_buf, 0, sizeof(SMB_buf));

	OR->smbH.Magic = SMB_MAGIC;
	OR->smbH.Cmd = SMB_COM_OPEN_ANDX;
	OR->smbH.Flags = SMB_FLAGS_CANONICAL_PATHNAMES; //| SMB_FLAGS_CASELESS_PATHNAMES;
	OR->smbH.Flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	OR->smbH.UID = UID;
	OR->smbH.TID = TID;
	OR->smbWordcount = 15;
	OR->smbAndxCmd = SMB_COM_NONE;	// no ANDX command
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

	// Prepare header of Read Request by advance
	smb_Read_Request.smbH.UID = UID;
	smb_Read_Request.smbH.TID = TID;

	struct ReadAndXRequest_t *RR = (struct ReadAndXRequest_t *)SMB_buf;

	mips_memcpy(RR, &smb_Read_Request.smbH.sessionHeader, sizeof(struct ReadAndXRequest_t));

	return 1;
}

//-------------------------------------------------------------------------
int smb_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num)
{	
	register u32 sectors, offset, nbytes;
	register u16 FID;
	register int rcv_size, pkt_size;
	u8 *p = (u8 *)buf;

	struct ReadAndXRequest_t *RR = (struct ReadAndXRequest_t *)SMB_buf;
	struct ReadAndXResponse_t *RRsp = (struct ReadAndXResponse_t *)SMB_buf;

	FID = (u16)p_part_start[part_num];
	offset = lsn << 11;
		
	while (nsectors > 0) {
		sectors = nsectors;
		if (sectors > MAX_SMB_SECTORS)
			sectors = MAX_SMB_SECTORS;

		nbytes = sectors << 11;	

		RR->smbH.sessionHeader = 0x3b000000;
		RR->smbH.Flags = 0;
		RR->FID = FID;
		RR->OffsetLow = offset;
		RR->MaxCountLow = nbytes;
		RR->MinCount = 0;
		RR->ByteCount = 0;
		RR->OffsetHigh = 0;

		plwip_send(main_socket, SMB_buf, 63, 0);
receive:
		rcv_size = plwip_recv(main_socket, SMB_buf, sizeof(SMB_buf), 0);

		if (SMB_buf[0] != 0)	// dropping NBSS Session Keep alive
			goto receive;

		// Handle fragmented packets
		while (rcv_size < (rawTCP_GetSessionHeader() + 4)) {
			pkt_size = plwip_recv(main_socket, &SMB_buf[rcv_size], sizeof(SMB_buf), 0); // - rcv_size
			rcv_size += pkt_size;
		}

		mips_memcpy(p, &SMB_buf[4 + RRsp->DataOffset], nbytes);

		p += nbytes;
		offset += nbytes;
		nsectors -= sectors;
	}

	return 1;
}

//-------------------------------------------------------------------------
int smb_Disconnect(void)
{
	plwip_close(main_socket);

	return 1;
}
