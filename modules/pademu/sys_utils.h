/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

#ifndef __SYS_UTILS_H
#define __SYS_UTILS_H

#include <irx.h>

typedef struct
{
    int version;
    void **exports;
} modinfo_t;

#ifdef USE_SMSUTILS
/* SMS Utils Imports */
#define smsutils_IMPORTS_start DECLARE_IMPORT_TABLE(smsutils, 1, 1)

void mips_memcpy(void *, const void *, unsigned);
#define I_mips_memcpy DECLARE_IMPORT(4, mips_memcpy)

void mips_memset(void *, int, unsigned);
#define I_mips_memset DECLARE_IMPORT(5, mips_memset)

#define smsutils_IMPORTS_end END_IMPORT_TABLE
#else
#define mips_memset memset
#define mips_memcpy memcpy
#endif

#endif /* __MCEMU_UTILS_H */
