
#ifndef __ISO2USBLD_H__
#define __ISO2USBLD_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define PROGRAM_NAME "iso2usbld"
#define PROGRAM_EXTNAME "ISO installer for Open USB Loader"
#define PROGRAM_VER "0.1"

typedef	unsigned char 		u8;
typedef unsigned short 		u16;
typedef unsigned int		u32;
typedef signed char 		s8;
typedef signed short 		s16;
typedef signed int		s32;

typedef struct {
	char name[32];
	char image[15];
	u8   parts;
	u8   media;
	u8   pad[15];
} cfg_t;

u32 isofs_Init(const char *iso_path);
int isofs_Reset(void);
int isofs_Open(const char *filename);
int isofs_Close(int fd);
int isofs_Read(int fd, void *buf, u32 nbytes);
int isofs_Seek(int fd, u32 offset, int origin);
int isofs_ReadISO(u32 offset, u32 nbytes, void *buf);

#endif
