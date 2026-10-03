// sio2man hook code taken from opl mcemu:
/*
  Copyright 2006-2008, Romz
  Copyright 2010, Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "pademu.h"

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
    u32 table_size;

    table_size = GetExportTableSize(table);
    if (entry < table_size) {
        int oldstate;
        void *oldfunc;
        register void **exp;
        unsigned int i;

        exp = (void **)table;

        CpuSuspendIntr(&oldstate);
        oldfunc = exp[entry];
        for (i = 0; i < table_size; i += 1)
            if (exp[i] == oldfunc)
                exp[i] = func;
        CpuResumeIntr(oldstate);

        return oldfunc;
    }

    return NULL;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
