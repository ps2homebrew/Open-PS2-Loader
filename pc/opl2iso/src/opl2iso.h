/*
  Copyright 2010, volca
  Parts Copyright 2009, jimmikaelkael
  Copyright (c) 2002, A.Lee & Nicholas Van Veen
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from libcdvd by A.Lee & Nicholas Van Veen
  Review license_libcdvd file for further details.
*/

#ifndef __OPL2ISO_H__
#define __OPL2ISO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PROGRAM_NAME    "opl2iso"
#define PROGRAM_EXTNAME "UL.CFG to ISO converter"
#define PROGRAM_VER     "0.0.2"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

// padded for easy manipulation (e.g. the first two are one byte longer than needed)
typedef struct
{
    char name[33];
    char image[16];
    u8 parts;
    u8 media;
    u8 pad[15];
} cfg_t;

#endif
