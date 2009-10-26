/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#ifndef __TCP_H__
#define __TCP_H__

#include <stdio.h>
#include <sysclib.h>
#include <intrman.h>
#include <sifman.h>
#include <thbase.h>
#include <ps2ip.h>

#include "poll.h"
#include "smb.h"

int tcp_ConnectSMBClient(char *SMBServerIP, int SMBServerPort);
int tcp_SessionSetup(char *User, char *Password, char *Share);
int tcp_DisconnectSMBClient(void);
int tcp_GetDir(char *name, int maxent, smb_FindFirst2_Entry *info);
int tcp_EchoSMBServer(u8 *msg, int sz_msg);
int tcp_Open(char *filename, u16 *FID, u32 *filesize);
int tcp_Read(u16 FID, void *buf, u32 offset, u16 nbytes);
int tcp_Close(u16 FID);

#endif
