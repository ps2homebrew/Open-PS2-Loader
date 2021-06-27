/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#ifndef __SMB_H__
#define __SMB_H__

#define SMB_MAGIC 0x424d53ff

// SMB Headers are always 32 bytes long
#define SMB_HDR_SIZE 32

// FLAGS field bitmasks
#define SMB_FLAGS_SERVER_TO_REDIR      0x80
#define SMB_FLAGS_REQUEST_BATCH_OPLOCK 0x40
#define SMB_FLAGS_REQUEST_OPLOCK       0x20
#define SMB_FLAGS_CANONICAL_PATHNAMES  0x10
#define SMB_FLAGS_CASELESS_PATHNAMES   0x08
#define SMB_FLAGS_RESERVED             0x04
#define SMB_FLAGS_CLIENT_BUF_AVAIL     0x02
#define SMB_FLAGS_SUPPORT_LOCKREAD     0x01
#define SMB_FLAGS_MASK                 0x00

// FLAGS2 field bitmasks
#define SMB_FLAGS2_UNICODE_STRING     0x8000
#define SMB_FLAGS2_32BIT_STATUS       0x4000
#define SMB_FLAGS2_READ_IF_EXECUTE    0x2000
#define SMB_FLAGS2_DFS_PATHNAME       0x1000
#define SMB_FLAGS2_EXTENDED_SECURITY  0x0800
#define SMB_FLAGS2_RESERVED_01        0x0400
#define SMB_FLAGS2_RESERVED_02        0x0200
#define SMB_FLAGS2_RESERVED_03        0x0100
#define SMB_FLAGS2_RESERVED_04        0x0080
#define SMB_FLAGS2_IS_LONG_NAME       0x0040
#define SMB_FLAGS2_RESERVED_05        0x0020
#define SMB_FLAGS2_RESERVED_06        0x0010
#define SMB_FLAGS2_RESERVED_07        0x0008
#define SMB_FLAGS2_SECURITY_SIGNATURE 0x0004
#define SMB_FLAGS2_EAS                0x0002
#define SMB_FLAGS2_KNOWS_LONG_NAMES   0x0001
#define SMB_FLAGS2_MASK               0xf847

// Field offsets
#define SMB_OFFSET_CMD           4
#define SMB_OFFSET_NTSTATUS      5
#define SMB_OFFSET_ECLASS        5
#define SMB_OFFSET_ECODE         7
#define SMB_OFFSET_FLAGS         9
#define SMB_OFFSET_FLAGS2        10
#define SMB_OFFSET_EXTRA         12
#define SMB_OFFSET_TID           24
#define SMB_OFFSET_PID           26
#define SMB_OFFSET_UID           28
#define SMB_OFFSET_MID           30
#define SMB_OFFSET_WORDCOUNT     32
#define SMB_OFFSET_ANDX_CMD      33
#define SMB_OFFSET_ANDX_RESERVED 34
#define SMB_OFFSET_ANDX_OFFSET   35

// Transaction2 Request Field offsets
#define SMB_TRANS2_REQ_OFFSET_TOTALPARAMCOUNT 33
#define SMB_TRANS2_REQ_OFFSET_TOTALDATACOUNT  35
#define SMB_TRANS2_REQ_OFFSET_MAXPARAMCOUNT   37
#define SMB_TRANS2_REQ_OFFSET_MAXDATACOUNT    39
#define SMB_TRANS2_REQ_OFFSET_MAXSETUPCOUNT   41
#define SMB_TRANS2_REQ_OFFSET_RESERVED        42
#define SMB_TRANS2_REQ_OFFSET_FLAGS           43
#define SMB_TRANS2_REQ_OFFSET_TIMEOUT         45
#define SMB_TRANS2_REQ_OFFSET_RESERVED2       49
#define SMB_TRANS2_REQ_OFFSET_PARAMCOUNT      51
#define SMB_TRANS2_REQ_OFFSET_PARAMOFFSET     53
#define SMB_TRANS2_REQ_OFFSET_DATACOUNT       55
#define SMB_TRANS2_REQ_OFFSET_DATAOFFSET      57
#define SMB_TRANS2_REQ_OFFSET_SETUPCOUNT      59
#define SMB_TRANS2_REQ_OFFSET_RESERVED3       60
#define SMB_TRANS2_REQ_OFFSET_SETUP0          61

// Transaction2 Response Field offsets
#define SMB_TRANS2_RSP_OFFSET_TOTALPARAMCOUNT   33
#define SMB_TRANS2_RSP_OFFSET_TOTALDATACOUNT    35
#define SMB_TRANS2_RSP_OFFSET_RESERVED          37
#define SMB_TRANS2_RSP_OFFSET_PARAMCOUNT        39
#define SMB_TRANS2_RSP_OFFSET_PARAMOFFSET       41
#define SMB_TRANS2_RSP_OFFSET_PARAMDISPLACEMENT 43
#define SMB_TRANS2_RSP_OFFSET_DATACOUNT         45
#define SMB_TRANS2_RSP_OFFSET_DATAOFFSET        47
#define SMB_TRANS2_RSP_OFFSET_DATADISPLACEMENT  49
#define SMB_TRANS2_RSP_OFFSET_SETUPCOUNT        51
#define SMB_TRANS2_RSP_OFFSET_RESERVED2         52
#define SMB_TRANS2_RSP_OFFSET_BYTECOUNT         53
#define SMB_TRANS2_RSP_OFFSET_PAD               55

// SMB File Attributes Encoding (16-bit)
#define ATTR_READONLY  0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME    0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20

// SMB Extended File Attributes Encoding (32-bit)
#define EXT_ATTR_READONLY   0x001
#define EXT_ATTR_HIDDEN     0x002
#define EXT_ATTR_SYSTEM     0x004
#define EXT_ATTR_DIRECTORY  0x010
#define EXT_ATTR_ARCHIVE    0x020
#define EXT_ATTR_NORMAL     0x080
#define EXT_ATTR_TEMPORARY  0x100
#define EXT_ATTR_COMPRESSED 0x800

// SMB Information Level
#define SMB_INFO_STANDARD                 0x001
#define SMB_INFO_QUERY_EA_SIZE            0x002
#define SMB_INFO_QUERY_EAS_FROM_LIST      0x003
#define SMB_FIND_FILE_DIRECTORY_INFO      0x101
#define SMB_FIND_FILE_FULL_DIRECTORY_INFO 0x102
#define SMB_FIND_FILE_NAMES_INFO          0x103
#define SMB_FIND_FILE_BOTH_DIRECTORY_INFO 0x104
#define SMB_FIND_FILE_UNIX                0x202

// SMB Search Flags
#define CLOSE_SEARCH_AFTER_REQUEST 0x01
#define CLOSE_SEARCH_IF_EOS        0x02
#define RESUME_SEARCH              0x04
#define CONTINUE_SEARCH            0x08
#define BACKUP_INTENT_SEARCH       0x10

// SMB Server Capabilities
#define SERVER_CAP_EXTENDED_SECURITY     0x80000000
#define SERVER_CAP_COMPRESSED_DATA       0x40000000
#define SERVER_CAP_BULK_TRANSFER         0x20000000
#define SERVER_CAP_UNIX                  0x00800000
#define SERVER_CAP_LARGE_WRITEX          0x00008000
#define SERVER_CAP_LARGE_READX           0x00004000
#define SERVER_CAP_INFOLEVEL_PASSTHROUGH 0x00002000
#define SERVER_CAP_DFS                   0x00001000
#define SERVER_CAP_NT_FIND               0x00000200
#define SERVER_CAP_LOCK_AND_READ         0x00000100
#define SERVER_CAP_LEVEL_II_OPLOCKS      0x00000080
#define SERVER_CAP_STATUS32              0x00000040
#define SERVER_CAP_RPC_REMOTE_APIS       0x00000020
#define SERVER_CAP_NT_SMBS               0x00000010
#define SERVER_CAP_LARGE_FILES           0x00000008
#define SERVER_CAP_UNICODE               0x00000004
#define SERVER_CAP_MPX_MODE              0x00000002
#define SERVER_CAP_RAW_MODE              0x00000001

// SMB Client Capabilities
#define CLIENT_CAP_EXTENDED_SECURITY SERVER_CAP_EXTENDED_SECURITY
#define CLIENT_CAP_LARGE_WRITEX      SERVER_CAP_LARGE_WRITEX
#define CLIENT_CAP_LARGE_READX       SERVER_CAP_LARGE_READX
#define CLIENT_CAP_NT_FIND           SERVER_CAP_NT_FIND
#define CLIENT_CAP_LEVEL_II_OPLOCKS  SERVER_CAP_LEVEL_II_OPLOCKS
#define CLIENT_CAP_STATUS32          SERVER_CAP_STATUS32
#define CLIENT_CAP_NT_SMBS           SERVER_CAP_NT_SMBS
#define CLIENT_CAP_LARGE_FILES       SERVER_CAP_LARGE_FILES
#define CLIENT_CAP_UNICODE           SERVER_CAP_UNICODE

// Security Modes
#define NEGOTIATE_SECURITY_SIGNATURES_REQUIRED 0x08
#define NEGOTIATE_SECURITY_SIGNATURES_ENABLED  0x04
#define NEGOTIATE_SECURITY_CHALLENGE_RESPONSE  0x02
#define NEGOTIATE_SECURITY_USER_LEVEL          0x01

// SMB Commands
#define SMB_COM_CREATE_DIRECTORY       0x00
#define SMB_COM_DELETE_DIRECTORY       0x01
#define SMB_COM_OPEN                   0x02
#define SMB_COM_CREATE                 0x03
#define SMB_COM_CLOSE                  0x04
#define SMB_COM_FLUSH                  0x05
#define SMB_COM_DELETE                 0x06
#define SMB_COM_RENAME                 0x07
#define SMB_COM_QUERY_INFORMATION      0x08
#define SMB_COM_SET_INFORMATION        0x09
#define SMB_COM_READ                   0x0a
#define SMB_COM_WRITE                  0x0b
#define SMB_COM_LOCK_BYTE_RANGE        0x0c
#define SMB_COM_UNLOCK_BYTE_RANGE      0x0d
#define SMB_COM_CREATE_TEMPORARY       0x0e
#define SMB_COM_CREATE_NEW             0x0f
#define SMB_COM_CHECK_DIRECTORY        0x10
#define SMB_COM_PROCESS_EXIT           0x11
#define SMB_COM_SEEK                   0x12
#define SMB_COM_LOCK_AND_READ          0x13
#define SMB_COM_WRITE_AND_UNLOCK       0x14
#define SMB_COM_READ_RAW               0x1a
#define SMB_COM_READ_MPX               0x1b
#define SMB_COM_READ_MPX_SECONDARY     0x1c
#define SMB_COM_WRITE_RAW              0x1d
#define SMB_COM_WRITE_MPX              0x1e
#define SMB_COM_WRITE_MPX_SECONDARY    0x1f
#define SMB_COM_WRITE_COMPLETE         0x20
#define SMB_COM_QUERY_SERVER           0x21
#define SMB_COM_SET_INFORMATION2       0x22
#define SMB_COM_QUERY_INFORMATION2     0x23
#define SMB_COM_LOCKING_ANDX           0x24
#define SMB_COM_TRANSACTION            0x25
#define SMB_COM_TRANSACTION_SECONDARY  0x26
#define SMB_COM_IOCTL                  0x27
#define SMB_COM_IOCTL_SECONDARY        0x28
#define SMB_COM_COPY                   0x29
#define SMB_COM_MOVE                   0x2a
#define SMB_COM_ECHO                   0x2b
#define SMB_COM_WRITE_AND_CLOSE        0x2c
#define SMB_COM_OPEN_ANDX              0x2d
#define SMB_COM_READ_ANDX              0x2e
#define SMB_COM_WRITE_ANDX             0x2f
#define SMB_COM_NEW_FILE_SIZE          0x30
#define SMB_COM_CLOSE_AND_TREE_DISC    0x31
#define SMB_COM_TRANSACTION2           0x32
#define SMB_COM_TRANSACTION2_SECONDARY 0x33
#define SMB_COM_FIND_CLOSE2            0x34
#define SMB_COM_FIND_NOTIFY_CLOSE      0x35
#define SMB_COM_TREE_CONNECT           0x70
#define SMB_COM_TREE_DISCONNECT        0x71
#define SMB_COM_NEGOTIATE              0x72
#define SMB_COM_SESSION_SETUP_ANDX     0x73
#define SMB_COM_LOGOFF_ANDX            0x74
#define SMB_COM_TREE_CONNECT_ANDX      0x75
#define SMB_COM_QUERY_INFORMATION_DISK 0x80
#define SMB_COM_SEARCH                 0x81
#define SMB_COM_FIND                   0x82
#define SMB_COM_FIND_UNIQUE            0x83
#define SMB_COM_FIND_CLOSE             0x84
#define SMB_COM_NT_TRANSACT            0xa0
#define SMB_COM_NT_TRANSACT_SECONDARY  0xa1
#define SMB_COM_NT_CREATE_ANDX         0xa2
#define SMB_COM_NT_CANCEL              0xa4
#define SMB_COM_NT_RENAME              0xa5
#define SMB_COM_OPEN_PRINT_FILE        0xc0
#define SMB_COM_WRITE_PRINT_FILE       0xc1
#define SMB_COM_CLOSE_PRINT_FILE       0xc2
#define SMB_COM_GET_PRINT_QUEUE        0xc3
#define SMB_COM_READ_BULK              0xd8
#define SMB_COM_WRITE_BULK             0xd9
#define SMB_COM_WRITE_BULK_DATA        0xda
#define SMB_COM_NONE                   0xff

// Setup[0] Transaction2 Subcommands
#define TRANS2_OPEN2                    0x00
#define TRANS2_FIND_FIRST2              0x01
#define TRANS2_FIND_NEXT2               0x02
#define TRANS2_QUERY_FS_INFORMATION     0x03
#define TRANS2_SET_FS_INFORMATION       0x04
#define TRANS2_QUERY_PATH_INFORMATION   0X05
#define TRANS2_SET_PATH_INFORMATION     0x06
#define TRANS2_QUERY_FILE_INFORMATION   0x07
#define TRANS2_SET_FILE_INFORMATION     0x08
#define TRANS2_FSCTL                    0x09
#define TRANS2_IOCTL2                   0x0a
#define TRANS2_FIND_NOTIFY_FIRST        0x0b
#define TRANS2_FIND_NOTIFY_NEXT         0x0c
#define TRANS2_CREATE_DIRECTORY         0x0d
#define TRANS2_SESSION_SETUP            0x0e
#define TRANS2_GET_DFS_REFERRAL         0x10
#define TRANS2_REPORT_DFS_INCONSISTENCY 0x11

// DOS Error Class
#define DOS_ECLASS_SUCCESS 0x00

// NT Status
#define STATUS_SUCCESS               0x00000000
#define STATUS_NO_MEDIA_IN_DEVICE    0xc0000013
#define STATUS_ACCESS_DENIED         0xc0000022
#define STATUS_OBJECT_NAME_NOT_FOUND 0xc0000034
#define STATUS_LOGON_FAILURE         0xc000006d

#define SERVER_SHARE_SECURITY_LEVEL   0
#define SERVER_USER_SECURITY_LEVEL    1
#define SERVER_USE_PLAINTEXT_PASSWORD 0
#define SERVER_USE_ENCRYPTED_PASSWORD 1

typedef struct
{
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
} __attribute__((packed)) SMBHeader_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u16 ByteCount;
    u8 DialectFormat;
    char DialectName[0];
} __attribute__((packed)) NegotiateProtocolRequest_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u16 DialectIndex;
    u8 SecurityMode;
    u16 MaxMpxCount;
    u16 MaxVC;
    u32 MaxBufferSize;
    u32 MaxRawBuffer;
    u32 SessionKey;
    u32 Capabilities;
    s64 SystemTime;
    u16 ServerTimeZone;
    u8 KeyLength;
    u16 ByteCount;
    u8 ByteField[0];
} __attribute__((packed)) NegotiateProtocolResponse_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 MaxBufferSize;
    u16 MaxMpxCount;
    u16 VCNumber;
    u32 SessionKey;
    u16 AnsiPasswordLength;
    u16 UnicodePasswordLength;
    u32 reserved;
    u32 Capabilities;
    u16 ByteCount;
    u8 ByteField[0];
} __attribute__((packed)) SessionSetupAndXRequest_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 Action;
    u16 ByteCount;
    u8 ByteField[0];
} __attribute__((packed)) SessionSetupAndXResponse_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 Flags;
    u16 PasswordLength;
    u16 ByteCount;
    u8 ByteField[0];
} __attribute__((packed)) TreeConnectAndXRequest_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 OptionalSupport;
    u16 ByteCount;
} __attribute__((packed)) TreeConnectAndXResponse_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 Flags;
    u16 AccessMask;
    u16 SearchAttributes;
    u16 FileAttributes;
    u8 CreationTime[4];
    u16 CreateOptions;
    u32 AllocationSize;
    u32 reserved[2];
    u16 ByteCount;
    u8 ByteField[0];
} __attribute__((packed)) OpenAndXRequest_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 FID;
    u16 FileAttributes;
    u8 LastWriteTime[4];
    u32 FileSize;
    u16 GrantedAccess;
    u16 FileType;
    u16 IPCState;
    u16 Action;
    u32 ServerFID;
    u16 reserved;
    u16 ByteCount;
} __attribute__((packed)) OpenAndXResponse_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 FID;
    u32 OffsetLow;
    u16 MaxCountLow;
    u16 MinCount;
    union
    {
        u32 Timeout;
        u16 MaxCountHigh;
    };
    u16 Remaining;
    u32 OffsetHigh;
    u16 ByteCount;
} __attribute__((packed)) ReadAndXRequest_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 Remaining;
    u16 DataCompactionMode;
    u16 reserved;
    u16 DataLengthLow;
    u16 DataOffset;
    u32 DataLengthHigh;
    u8 reserved2[6];
    u16 ByteCount;
} __attribute__((packed)) ReadAndXResponse_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 FID;
    u32 OffsetLow;
    u32 Reserved;
    u16 WriteMode;
    u16 Remaining;
    u16 DataLengthHigh;
    u16 DataLengthLow;
    u16 DataOffset;
    u32 OffsetHigh;
    u16 ByteCount;
} __attribute__((packed)) WriteAndXRequest_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u8 smbAndxCmd;
    u8 smbAndxReserved;
    u16 smbAndxOffset;
    u16 Count;
    u16 Remaining;
    u16 CountHigh;
    u16 Reserved;
} __attribute__((packed)) WriteAndXResponse_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u16 FID;
    u32 LastWrite;
    u16 ByteCount;
} __attribute__((packed)) CloseRequest_t;

typedef struct
{
    SMBHeader_t smbH;
    u8 smbWordcount;
    u16 ByteCount;
} __attribute__((packed)) CloseResponse_t;

// function prototypes
int smb_NegotiateProtocol(char *SMBServerIP, int SMBServerPort, char *Username, char *Password, u32 *capabilities, OplSmbPwHashFunc_t hash_callback); // process a Negotiate Procotol message
int smb_SessionSetupTreeConnect(char *share_name);
int smb_SessionSetupAndX(u32 capabilities); // process a Session Setup message, for NT LM 0.12 dialect, Non Extended Security negociated
int smb_TreeConnectAndX(char *ShareName);
int smb_OpenAndX(char *filename, u8 *FID, int Write); // process a Open AndX message
int smb_Close(int FID);
int smb_ReadFile(u16 FID, u32 offsetlow, u32 offsethigh, void *readbuf, int nbytes);
int smb_WriteFile(u16 FID, u32 offsetlow, u32 offsethigh, void *writebuf, int nbytes);
int smb_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num);
void smb_CloseAll(void);
int smb_Disconnect(void);

#define MAX_SMB_BUF     896 // must fit on u16 !!!
#define MAX_SMB_BUF_HDR 128 //Must be at least as large as the largest header structure.

#endif
