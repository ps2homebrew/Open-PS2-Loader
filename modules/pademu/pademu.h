/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

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

typedef struct
{
    int id;
    int (*pad_get_data)(u8 *dst, int size, int id);
    void (*pad_set_rumble)(u8 lrum, u8 rrum, int id);
    void (*pad_set_mode)(int mode, int lock, int id);
} pad_device_t;

typedef void (*Sio2McProc)(sio2_transfer_data_t *arg);

void *GetExportTable(char *libname, int version);
u32 GetExportTableSize(void *table);
void *GetExportEntry(void *table, u32 entry);
void *HookExportEntry(void *table, u32 entry, void *func);


#define pademu_IMPORTS_start DECLARE_IMPORT_TABLE(pademu, 1, 1)

void pademu_hookSio2man(sio2_transfer_data_t *td, Sio2McProc sio2proc);
#define I_pademu_hookSio2man DECLARE_IMPORT(4, pademu_hookSio2man)

void pademu_connect(pad_device_t *dev);
#define I_pademu_connect DECLARE_IMPORT(5, pademu_connect)

void pademu_disconnect(pad_device_t *dev);
#define I_pademu_disconnect DECLARE_IMPORT(6, pademu_disconnect)

#define pademu_IMPORTS_end END_IMPORT_TABLE
