/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#ifndef __OPS_H__
#define __OPS_H__

#include <io_common.h>

int smbConnect(char *SMBServerIP, int SMBServerPort);
int smbLogin(char *User, char *Password, char *Share);
int smbDisconnect(void);
int smbEchoSMBServer(u8 *msg, int size);
int smbOpen(int unit, char *filename, int flags);
int smbClose(int fd);
int smbCloseAll(void);
int smbRead(int fd, void *buf, u32 nbytes);
int smbSeek(int fd, u32 offset, int origin);
int smbGetDir(char *dirname, int maxent, void *info);

#endif
