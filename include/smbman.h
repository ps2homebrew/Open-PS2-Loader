/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#ifndef __SMBMAN_H__
#define __SMBMAN_H__

#define NO_PASSWORD		-1
#define PLAINTEXT_PASSWORD 	 0
#define HASHED_PASSWORD		 1

// DEVCTL commands
#define SMB_DEVCTL_CONNECT		0xC0DE0000
#define SMB_DEVCTL_GETPASSWORDHASHES	0xC0DE0001
#define SMB_DEVCTL_LOGON		0xC0DE0002
#define SMB_DEVCTL_LOGOFF		0xC0DE0003
#define SMB_DEVCTL_GETSHARELIST		0xC0DE0004
#define SMB_DEVCTL_OPENSHARE		0xC0DE0005
#define SMB_DEVCTL_CLOSESHARE		0xC0DE0006
#define SMB_DEVCTL_ECHO			0xC0DE0007
#define SMB_DEVCTL_QUERYDISKINFO	0xC0DE0008
#define SMB_DEVCTL_DISCONNECT		0xC0DE0009

// helpers for DEVCTL commands

typedef struct {		// size = 20
	char serverIP[16];
	int  serverPort;
} smbConnect_in_t;

typedef struct {
	char password[256];
} smbGetPasswordHashes_in_t;

typedef struct {		// size = 32
	u8 LMhash[16];
	u8 NTLMhash[16];
} smbGetPasswordHashes_out_t;

typedef struct {		// size = 536
	char User[256];
	char Password[256];
	int  PasswordType;	// PLAINTEXT_PASSWORD or HASHED_PASSWORD
} smbLogOn_in_t;

typedef struct {		// size = 8
	void *EE_addr;
	int   maxent;
} smbGetShareList_in_t;

typedef struct {		// size = 520
	char ShareName[256];
	char Password[256];
	int  PasswordType;	// PLAINTEXT_PASSWORD or HASHED_PASSWORD
} smbOpenShare_in_t;

typedef struct {		// size = 260
	char echo[256];
	int  len;
} smbEcho_in_t;

typedef struct {		// size = 16
	int TotalUnits;
	int BlocksPerUnit;
	int BlockSize;
	int FreeUnits;
} smbQueryDiskInfo_out_t;

typedef struct {		// size = 512
	char ShareName[256];
	char ShareComment[256];
} ShareEntry_t;

#endif
