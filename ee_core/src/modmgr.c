/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include "ee_core.h"
#include "modules.h"
#include "modmgr.h"
#include "util.h"
#include "coreconfig.h"

static SifRpcClientData_t _lf_cd;
static int _lf_init = 0;

/*----------------------------------------------------------------------------------------*/
/* Init LOADFILE RPC.                                                                     */
/*----------------------------------------------------------------------------------------*/
int LoadFileInit()
{
    int r;

    if (_lf_init)
        return 0;

    while ((r = SifBindRpc(&_lf_cd, 0x80000006, 0)) >= 0 && (!_lf_cd.server))
        nopdelay();

    if (r < 0)
        return -E_SIF_RPC_BIND;

    _lf_init = 1;

    return 0;
}

/*----------------------------------------------------------------------------------------*/
/* DeInit LOADFILE RPC.                                                                   */
/*----------------------------------------------------------------------------------------*/
void LoadFileExit()
{
    _lf_init = 0;
    memset(&_lf_cd, 0, sizeof(_lf_cd));
}


/*----------------------------------------------------------------------------------------*/
/* Load an irx module from path with waiting.                                             */
/*----------------------------------------------------------------------------------------*/
int LoadModule(const char *path, int arg_len, const char *args)
{
    struct _lf_module_load_arg arg;

    if (LoadFileInit() < 0)
        return -SCE_EBINDMISS;

    memset(&arg, 0, sizeof arg);

    strncpy(arg.path, path, LF_PATH_MAX - 1);
    arg.path[LF_PATH_MAX - 1] = 0;

    if ((args) && (arg_len)) {
        arg.p.arg_len = arg_len > LF_ARG_MAX ? LF_ARG_MAX : arg_len;
        memcpy(arg.args, args, arg.p.arg_len);
    } else
        arg.p.arg_len = 0;

    if (SifCallRpc(&_lf_cd, LF_F_MOD_LOAD, 0x0, &arg, sizeof(arg), &arg, 8, NULL, NULL) < 0)
        return -SCE_ECALLMISS;

    return arg.p.result;
}

/*----------------------------------------------------------------------------------------*/
/* Load an irx module from path without waiting.                                          */
/*----------------------------------------------------------------------------------------*/
int LoadMemModule(int mode, void *modptr, unsigned int modsize, int arg_len, const char *args)
{
    SifDmaTransfer_t sifdma;
    void *iopmem;
    int dma_id;

    if (LoadFileInit() < 0)
        return -SCE_EBINDMISS;

    /* Round the size up to the nearest 16 bytes. */
    // modsize = (modsize + 15) & -16;

    iopmem = SifAllocIopHeap(modsize);
    if (iopmem == NULL)
        return -E_IOP_NO_MEMORY;

    sifdma.src = modptr;
    sifdma.dest = iopmem;
    sifdma.size = modsize;
    sifdma.attr = 0;

    // All IOP modules should have already been written back to RAM by the FlushCache() calls within the GUI and the CRT.
    // SifWriteBackDCache(modptr, modsize);

    do {
        dma_id = SifSetDma(&sifdma, 1);
    } while (!dma_id);

    while (SifDmaStat(dma_id) >= 0) {
        ;
    }

    struct _lf_module_buffer_load_arg arg;

    memset(&arg, 0, sizeof arg);

    arg.p.ptr = iopmem;
    if ((args) && (arg_len)) {
        arg.q.arg_len = arg_len > LF_ARG_MAX ? LF_ARG_MAX : arg_len;
        memcpy(arg.args, args, arg.q.arg_len);
    } else
        arg.q.arg_len = 0;

    if (SifCallRpc(&_lf_cd, LF_F_MOD_BUF_LOAD, mode, &arg, sizeof(arg), &arg, 8, NULL, NULL) < 0)
        return -SCE_ECALLMISS;

    if (!(mode & SIF_RPC_M_NOWAIT))
        SifFreeIopHeap(iopmem);

    return arg.p.result;
}

int GetOPLModInfo(int id, void **pointer, unsigned int *size)
{
    USE_LOCAL_EECORE_CONFIG;
    int i, result;
    irxtab_t *irxtable = (irxtab_t *)config->ModStorageStart;

    for (i = 0, result = -1; i < irxtable->count; i++) {
        if (GET_OPL_MOD_ID(irxtable->modules[i].info) == id) {
            *pointer = irxtable->modules[i].ptr;
            *size = GET_OPL_MOD_SIZE(irxtable->modules[i].info);
            result = 0;
            break;
        }
    }

    return result;
}

int LoadOPLModule(int id, int mode, int arg_len, const char *args)
{
    int result;
    void *pointer;
    unsigned int size;

    if ((result = GetOPLModInfo(id, &pointer, &size)) == 0) {
        if (size > 0) {
            result = LoadMemModule(mode, pointer, size, arg_len, args);
            if (result < 1) { // El_isra: only check for MODLOAD errors for simplicity
                if (EnableDebug) {
                    DBGCOL(0x00FFDC, MODMGR, "IRX loading error (from MODLOAD)");
                    delay(3);
                }
                if (result == -400) {
                    DBGCOL_BLNK(1, 0x0000FF, true, MODMGR, "MODLOAD: out of IOP Memory"); // yellow blinking
                    while (1)
                        ;
                }
            }
        } else {
            if (EnableDebug) {
                DBGCOL(0xDCFF00, MODMGR, "IRX size is 0 or less");
                delay(3);
            }
        }
    }

    BGCOLND(0x000000);
    return result;
}

/*----------------------------------------------------------------------------------------*/
/* Load an ELF file from the specified path.                                              */
/*----------------------------------------------------------------------------------------*/
int LoadElf(const char *path, t_ExecData *data)
{
    struct _lf_elf_load_arg arg;

    if (LoadFileInit() < 0)
        return -SCE_EBINDMISS;

    u32 secname = 0x6c6c61; /* "all" */

    strncpy(arg.path, path, LF_PATH_MAX - 1);
    strncpy(arg.secname, (char *)&secname, LF_ARG_MAX - 1);
    arg.path[LF_PATH_MAX - 1] = 0;
    arg.secname[LF_ARG_MAX - 1] = 0;

    if (SifCallRpc(&_lf_cd, LF_F_ELF_LOAD, 0, &arg, sizeof arg, &arg, sizeof(t_ExecData), NULL, NULL) < 0)
        return -SCE_ECALLMISS;

    if (arg.epc != 0) {
        data->epc = arg.epc;
        data->gp = arg.gp;

        return 0;
    } else
        return -SCE_ELOADMISS;
}

/*----------------------------------------------------------------------------------------*/
/* Find and change a module's name.                                                       */
/*----------------------------------------------------------------------------------------*/
void ChangeModuleName(const char *name, const char *newname)
{
    char search_name[60];
    smod_mod_info_t info;
    int len;

    if (!smod_get_next_mod(NULL, &info))
        return;

    len = strlen(name);

    do {
        smem_read(info.name, search_name, sizeof(search_name));

        if (!_memcmp(search_name, name, len)) {
            strncpy(search_name, newname, sizeof(search_name));
            search_name[sizeof(search_name) - 1] = '\0';
            len = strlen(search_name);
            SyncDCache(search_name, search_name + len);
            smem_write(info.name, search_name, len);
            break;
        }
    } while (smod_get_next_mod(&info, &info));
}

/*----------------------------------------------------------------------------------------*/
/* List modules currently loaded in the system.                                           */
/*----------------------------------------------------------------------------------------*/
#ifdef __EESIO_DEBUG
void ListModules(void)
{
    int c = 0;
    smod_mod_info_t info;
    u8 name[60];

    if (!smod_get_next_mod(0, &info))
        return;
    DPRINTF("List of modules currently loaded in the system:\n");
    do {
        smem_read(info.name, name, sizeof name);
        if (!(c & 1))
            DPRINTF(" ");
        DPRINTF("   %-21.21s  %2d  %3x%c", name, info.id, info.version, (++c & 1) ? ' ' : '\n');
    } while (smod_get_next_mod(&info, &info));
}
#endif
