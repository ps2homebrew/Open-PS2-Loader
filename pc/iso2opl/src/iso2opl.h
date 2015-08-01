/*
  Copyright 2009, jimmikaelkael
  Copyright (c) 2002, A.Lee & Nicholas Van Veen  
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from libcdvd by A.Lee & Nicholas Van Veen
  Review license_libcdvd file for further details.
*/

#ifndef __ISO2OPL_H__
#define __ISO2OPL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PROGRAM_NAME "iso2opl"
#define PROGRAM_EXTNAME "ISO installer for Open PS2 Loader"
#define PROGRAM_VER "0.1.2"

typedef	unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;
typedef signed char		s8;
typedef signed short		s16;
typedef signed int		s32;
typedef signed long long	s64;

typedef struct {
	char	name[32];
	char	image[15];
	u8	parts;
	u8	media;
	u8	pad[15];
} cfg_t;

s64 isofs_Init(const char *iso_path, int isBigEndian);
int isofs_Reset(void);
int isofs_Open(const char *filename);
int isofs_Close(int fd);
int isofs_Read(int fd, void *buf, u32 nbytes);
int isofs_Seek(int fd, u32 offset, int origin);
int isofs_ReadISO(s64 offset, u32 nbytes, void *buf);

#endif
