/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

#include <ioman.h>
#include <intrman.h>
#include <loadcore.h>
#include <sifcmd.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>
#include <dmacman.h>
#include <xloadcore.h>

#include "sys_utils.h"

/* type for a pointer to LOADCORE's entry  RegisterLibraryEntires */
typedef int (*PtrRegisterLibraryEntires)(iop_library_t *lib);

struct _sio2_dma_arg
{ // size 12
    void *addr;
    int size;
    int count;
};

typedef struct
{
    u32 stat6c;

    u32 port_ctrl1[4];
    u32 port_ctrl2[4];

    u32 stat70;

    u32 regdata[16];

    u32 stat74;

    u32 in_size;
    u32 out_size;
    u8 *in;
    u8 *out;

    struct _sio2_dma_arg in_dma;
    struct _sio2_dma_arg out_dma;
} sio2_transfer_data_t;

typedef void (*Sio2McProc)(sio2_transfer_data_t *arg);

void *GetExportTable(char *libname, int version);
u32 GetExportTableSize(void *table);
void *GetExportEntry(void *table, u32 entry);
void *HookExportEntry(void *table, u32 entry, void *func);
