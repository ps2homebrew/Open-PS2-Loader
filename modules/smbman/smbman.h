/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#ifndef __SMBMAN_H__
#define __SMBMAN_H__

// DEVCTL commands
#define DEVCTL_GETPASSWORDHASHES	0xC0DE0001
#define DEVCTL_LOGON			0xC0DE0002
#define DEVCTL_LOGOFF			0xC0DE0003
#define DEVCTL_GETSHARELIST		0xC0DE0004
#define DEVCTL_OPENSHARE		0xC0DE0005
#define DEVCTL_CLOSESHARE		0xC0DE0006

// helpers for DEVCTL commands

typedef struct
{
	u8 password[0];
} smbGetPasswordHashes_in_t;

typedef struct 			// size = 32
{
	u8 LMhash[16];
	u8 NTLMhash[16];
} smbGetPasswordHashes_out_t;

#endif
