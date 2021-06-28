/*
  Copyright 2006-2008, Romz
  Copyright 2010, Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "mcemu.h"

/* prototype for LOADCORE's function */
void *QueryLibraryEntryTable(iop_library_t *lib);

//---------------------------------------------------------------------------
/* Returns a pointer to a library entry table */
void *GetExportTable(char *libname, int version)
{
    if (libname != NULL) {
        iop_library_t lib;
        register int i;
        register char *psrc;

        mips_memset(&lib, 0, sizeof(iop_library_t));
        lib.version = version;

        for (i = 0, psrc = libname; (i < 8) && (*psrc); i++, psrc++)
            lib.name[i] = *psrc;

        return QueryLibraryEntryTable(&lib);
    }

    return NULL;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Returns number of entries in the export table */
u32 GetExportTableSize(void *table)
{
    register void **exp;
    register u32 size;

    exp = (void **)table;
    size = 0;

    if (exp != NULL)
        while (*exp++ != NULL)
            size++;

    return size;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Returns an entry from the export table */
void *GetExportEntry(void *table, u32 entry)
{
    if (entry < GetExportTableSize(table)) {
        register void **exp;

        exp = (void **)table;

        return exp[entry];
    }

    return NULL;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Replaces an entry in the export table */
void *HookExportEntry(void *table, u32 entry, void *func)
{
    if (entry < GetExportTableSize(table)) {
        int oldstate;
        register void **exp, *temp;

        exp = (void **)table;
        exp = &exp[entry];

        CpuSuspendIntr(&oldstate);
        temp = *exp;
        *exp = func;
        func = temp;
        CpuResumeIntr(oldstate);

        return func;
    }

    return NULL;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Allocates system memory on IOP */
void *_SysAlloc(u64 size)
{
    int oldstate;
    register void *p;

    CpuSuspendIntr(&oldstate);
    p = AllocSysMemory(ALLOC_FIRST, size, NULL);
    CpuResumeIntr(oldstate);

    return p;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Frees system memory on IOP */
int _SysFree(void *area)
{
    int oldstate;
    register int r;

    CpuSuspendIntr(&oldstate);
    r = FreeSysMemory(area);
    CpuResumeIntr(oldstate);

    return r;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Makes an integer value */
int GetInt(void *ptr)
{
    register u8 *p;

    p = (u8 *)ptr;

    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Calculates a "checksum" */
u32 CalculateEDC(u8 *buf, u32 size)
{
    register int i, x;
    register u8 *p;

    p = buf;
    i = x = 0;
    while (i++ < size)
        x ^= *p++;

    return x & 0xFF;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Calculates ECC for a 0x80 bytes of data */
void CalculateECC(u8 *buf, void *chk)
{
    register u8 *ptr;
    register int i, c1, c2, b, c3;

    ptr = buf;
    i = 0;
    c1 = c2 = c3 = 0;

    /* calculating ECC for a 0x80-bytes buffer */
    do {
        b = xortable[*ptr++];
        c1 ^= b;
        if (b & 0x80) {
            c2 ^= ~i;
            c3 ^= i;
        }
    } while (++i < 0x80);

    /* storing ECC */
    ptr = (u8 *)chk;
    ptr[0] = ~c1 & 0x77;
    ptr[1] = ~c2 & 0x7F;
    ptr[2] = ~c3 & 0x7F;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// End of file: mcsiosys.c
//---------------------------------------------------------------------------
