/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>

  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#ifndef __GENVMC_H__
#define __GENVMC_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define PROGRAM_NAME    "genvmc"
#define PROGRAM_EXTNAME "VMC file generator for Open PS2 Loader"
#define PROGRAM_VER     "0.1.0"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

#endif
