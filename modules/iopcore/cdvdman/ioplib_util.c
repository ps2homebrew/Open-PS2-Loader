/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>

#include "ioplib_util.h"

#ifdef __IOPCORE_DEBUG
#define DPRINTF(args...) printf(args)
#else
#define DPRINTF(args...) \
    do {                 \
    } while (0)
#endif

#define FAKEMOD_ID 0xdead

// MODLOAD's exports pointers
static int (*LoadStartModule)(char *modpath, int arg_len, char *args, int *modres);
static int (*StartModule)(int id, char *modname, int arg_len, char *args, int *modres);
static int (*LoadModuleBuffer)(void *ptr);
static int (*StopModule)(int id, int arg_len, char *args, int *modres);
static int (*UnloadModule)(int id);
static int (*SearchModuleByName)(char *modname);

// modules list to fake loading
static char *lm_modulefake_list[] = {
#ifdef __USE_DEV9
    "DEV9.IRX",
#endif
#ifdef USB_DRIVER
    "USBD.IRX",
#endif
#ifdef SMB_DRIVER
    "SMAP.IRX",
#endif
#ifdef HDD_DRIVER
    "ATAD.IRX",
#endif
    "CDVDSTM.IRX",
    NULL
};

static char *lmb_modulefake_list[] = {
#ifdef __USE_DEV9
    "dev9",
#endif
#ifdef USB_DRIVER
    "USB_driver",
#endif
#ifdef SMB_DRIVER
    "INET_SMAP_driver",
#endif
#ifdef HDD_DRIVER
    "atad_driver",
#endif
    "cdvd_st_driver",
    NULL
};

static u8 fakemod_flag = 0;
static u16 modloadVersion;

//--------------------------------------------------------------
int getModInfo(u8 *modname, modinfo_t *info)
{
    iop_library_t *libptr;
    register int i;

    libptr = GetLoadcoreInternalData()->let_next;
    while (libptr != 0) {
        for (i = 0; i < 8; i++) {
            if (libptr->name[i] != modname[i])
                break;
        }
        if (i == 8)
            break;
        libptr = libptr->prev;
    }

    if (!libptr)
        return 0;

    info->version = libptr->version;
    info->exports = (void **)(((struct irx_export_table *)libptr)->fptrs);
    return 1;
}

//--------------------------------------------------------------
static int checkFakemod(char *modname, char **fakemod_list)
{
    // check if module is in the list
    while (*fakemod_list) {
        if (strstr(modname, *fakemod_list)) {
            fakemod_flag = 1;
            return 1;
        }
        fakemod_list++;
    }

    return 0;
}

//--------------------------------------------------------------
static int isFakemod(void)
{
    if (fakemod_flag) {
        DPRINTF("isFakemod() module is on fakelist!!!\n");
        fakemod_flag = 0;
        return 1;
    }

    return 0;
}

//--------------------------------------------------------------
static int Hook_LoadStartModule(char *modpath, int arg_len, char *args, int *modres)
{
    DPRINTF("Hook_LoadStartModule() modpath = %s\n", modpath);

    checkFakemod(modpath, lm_modulefake_list);

    if (isFakemod())
    {
        *modres = modloadVersion > 0x102 ? 2 : 0; //Most of the new, loadable modules return REMOVABLE END.
        return FAKEMOD_ID;
    }

    return LoadStartModule(modpath, arg_len, args, modres);
}

//--------------------------------------------------------------
static int Hook_StartModule(int id, char *modname, int arg_len, char *args, int *modres)
{
    DPRINTF("Hook_StartModule() id=%d modname = %s\n", id, modname);

    if (isFakemod())
    {
        *modres = modloadVersion > 0x102 ? 2 : 0; //Most of the new, loadable modules return REMOVABLE END.
        return FAKEMOD_ID;
    }

    return StartModule(id, modname, arg_len, args, modres);
}

//--------------------------------------------------------------
static int Hook_LoadModuleBuffer(void *ptr)
{
    DPRINTF("Hook_LoadModuleBuffer() modname = %s\n", (char *)(ptr + 0x8e));

    if (checkFakemod((char *)(ptr + 0x8e), lmb_modulefake_list))
        return FAKEMOD_ID;

    return LoadModuleBuffer(ptr);
}

//--------------------------------------------------------------
static int Hook_StopModule(int id, int arg_len, char *args, int *modres)
{
    DPRINTF("Hook_StopModule() id=%d arg_len=%d\n", id, arg_len);

    if (id == FAKEMOD_ID)
    {
        *modres = 1; //Module unloads and returns FAREWELL END
        return 0;
    }

    return StopModule(id, arg_len, args, modres);
}

//--------------------------------------------------------------
static int Hook_UnloadModule(int id)
{
    DPRINTF("Hook_UnloadModule() id=%d\n", id);

    if (id == FAKEMOD_ID)
        return 0;

    return UnloadModule(id);
}

//--------------------------------------------------------------
static int Hook_SearchModuleByName(char *modname)
{
    DPRINTF("Hook_SearchModuleByName() modname = %s\n", modname);

    if (!strcmp(modname, "cdvd_ee_driver"))
        return FAKEMOD_ID;

    return SearchModuleByName(modname);
}

//--------------------------------------------------------------
void hookMODLOAD(void)
{
    // get modload export table
    modinfo_t info;
    getModInfo("modload\0", &info);

    // hook modload's LoadStartModule function
    LoadStartModule = (void *)info.exports[7];
    info.exports[7] = (void *)Hook_LoadStartModule;

    // hook modload's StartModule function
    StartModule = (void *)info.exports[8];
    info.exports[8] = (void *)Hook_StartModule;

    // hook modload's LoadModuleBuffer
    LoadModuleBuffer = (void *)info.exports[10];
    info.exports[10] = (void *)Hook_LoadModuleBuffer;

    // check modload version
    modloadVersion = info.version;
    if (info.version > 0x102) {

        // hook modload's StopModule
        StopModule = (void *)info.exports[20];
        info.exports[20] = (void *)Hook_StopModule;

        // hook modload's UnloadModule
        UnloadModule = (void *)info.exports[21];
        info.exports[21] = (void *)Hook_UnloadModule;

        // hook modload's SearchModuleByName
        SearchModuleByName = (void *)info.exports[22];
        info.exports[22] = (void *)Hook_SearchModuleByName;
    }

    // fix imports
    iop_library_t *lib = (iop_library_t *)((u32)info.exports - 0x14);

    struct irx_import_table *table;
    struct irx_import_stub *stub;

    FlushDcache();

    // go through each table that imports the library
    for (table = lib->caller; table != NULL; table = table->next) {
        // go through each import in the table
        for (stub = (struct irx_import_stub *)table->stubs; stub->jump != 0; stub++) {
            // patch the stub to jump to the address specified in the library export table for "fno"
            stub->jump = 0x08000000 | (((u32)lib->exports[stub->fno] << 4) >> 6);
        }
    }

    FlushIcache();
}
