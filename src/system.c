/*
  Copyright 2009, Ifcaro
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/ethsupport.h"
#include "include/util.h"
#include "include/pad.h"
#include "include/system.h"
#include "include/ioman.h"
#include "include/ioprp.h"
#include "include/usbsupport.h"
#include "include/OSDHistory.h"
#include "include/renderman.h"
#include "../ee_core/include/modules.h"

#ifdef PADEMU
#include <libds3bt.h>
#include <libds3usb.h>
#endif

#ifdef VMC
typedef struct
{
    char VMC_filename[1024];
    int VMC_size_mb;
    int VMC_blocksize;
    int VMC_thread_priority;
    int VMC_card_slot;
} createVMCparam_t;

extern void *genvmc_irx;
extern int size_genvmc_irx;
#endif
#ifdef GSM
#include "include/pggsm.h"
#endif
#ifdef CHEAT
#include "include/pgcht.h"
#endif

extern void *udnl_irx;
extern int size_udnl_irx;

extern void *imgdrv_irx;
extern int size_imgdrv_irx;

extern void *cdvdfsv_irx;
extern int size_cdvdfsv_irx;

extern void *isofs_irx;
extern int size_isofs_irx;

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *smstcpip_irx;
extern int size_smstcpip_irx;

extern void *ingame_smstcpip_irx;
extern int size_ingame_smstcpip_irx;

extern void *smap_irx;
extern int size_smap_irx;

extern void *smap_ingame_irx;
extern int size_smap_ingame_irx;

extern void *smbinit_irx;
extern int size_smbinit_irx;

extern void *iomanx_irx;
extern int size_iomanx_irx;

extern void *filexio_irx;
extern int size_filexio_irx;

extern void *poweroff_irx;
extern int size_poweroff_irx;

extern void *ps2atad_irx;
extern int size_ps2atad_irx;

extern void *ps2hdd_irx;
extern int size_ps2hdd_irx;

#ifdef _DTL_T10000
extern void *sio2man_irx;
extern int size_sio2man_irx;

extern void *padman_irx;
extern int size_padman_irx;

extern void *mcman_irx;
extern int size_mcman_irx;

extern void *mcserv_irx;
extern int size_mcserv_irx;
#endif

extern void *hdldsvr_irx;
extern int size_hdldsvr_irx;

extern void *eecore_elf;
extern int size_eecore_elf;

extern void *elfldr_elf;
extern int size_elfldr_elf;

extern void *pusbd_irx;
extern int size_pusbd_irx;

#ifdef PADEMU
extern void *ds3bt_irx;
extern int size_ds3bt_irx;

extern void *ds3usb_irx;
extern int size_ds3usb_irx;

extern void *bt_pademu_irx;
extern int size_bt_pademu_irx;

extern void *usb_pademu_irx;
extern int size_usb_pademu_irx;
#endif

#ifdef __INGAME_DEBUG
extern void *udptty_ingame_irx;
extern int size_udptty_ingame_irx;

extern void *ioptrap_irx;
extern int size_ioptrap_irx;
#endif

#ifdef __DECI2_DEBUG
extern void *drvtif_irx;
extern int size_drvtif_irx;

extern void *tifinet_irx;
extern int size_tifinet_irx;
#endif

extern unsigned char IOPRP_img[];
extern unsigned int size_IOPRP_img;

#define MAX_MODULES 32
static void *g_sysLoadedModBuffer[MAX_MODULES];

#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1

typedef struct
{
    u8 ident[16]; // struct definition for ELF object header
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;

typedef struct
{
    u32 type; // struct definition for ELF program section header
    u32 offset;
    void *vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf_pheader_t;

void guiWarning(const char *text, int count);
void guiEnd();
void menuEnd();
void lngEnd();
void thmEnd();
void rmEnd();

int sysLoadModuleBuffer(void *buffer, int size, int argc, char *argv)
{

    int i, id, ret, index = 0;

    // check we have not reached MAX_MODULES
    for (i = 0; i < MAX_MODULES; i++) {
        if (g_sysLoadedModBuffer[i] == NULL) {
            index = i;
            break;
        }
    }
    if (i == MAX_MODULES)
        return -1;

    // check if the module was already loaded
    for (i = 0; i < MAX_MODULES; i++) {
        if (g_sysLoadedModBuffer[i] == buffer) {
            return 0;
        }
    }

    // load the module
    id = SifExecModuleBuffer(buffer, size, argc, argv, &ret);
    if ((id < 0) || (ret))
        return -2;

    // add the module to the list
    g_sysLoadedModBuffer[index] = buffer;

    return 0;
}

#define OPL_SIF_CMD_BUFF_SIZE 1
static SifCmdHandlerData_t OplSifCmdbuffer[OPL_SIF_CMD_BUFF_SIZE];
static unsigned char dev9Initialized = 0;

void sysInitDev9(void)
{
    if (!dev9Initialized) {
        sysLoadModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
        dev9Initialized = 1;
    }
}

void sysReset(int modload_mask)
{

    fileXioExit();
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();

    SifInitRpc(0);

#ifdef _DTL_T10000
    while (!SifIopReset("rom0:UDNL", 0))
        ;
#else
    while (!SifIopReset(NULL, 0))
        ;
#endif

    dev9Initialized = 0;
    while (!SifIopSync())
        ;

    SifInitRpc(0);
    SifSetCmdBuffer(OplSifCmdbuffer, OPL_SIF_CMD_BUFF_SIZE);
    sceCdInit(SCECdINoD);

    // init loadfile & iopheap services
    SifLoadFileInit();
    SifInitIopHeap();

    // apply sbv patches
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    // clears modules list
    memset((void *)&g_sysLoadedModBuffer[0], 0, MAX_MODULES * 4);

    // load modules
    sysLoadModuleBuffer(&iomanx_irx, size_iomanx_irx, 0, NULL);
    sysLoadModuleBuffer(&filexio_irx, size_filexio_irx, 0, NULL);

#ifdef _DTL_T10000
    SifExecModuleBuffer(&sio2man_irx, size_sio2man_irx, 0, NULL, NULL);

    if (modload_mask & SYS_LOAD_MC_MODULES) {
        SifExecModuleBuffer(&mcman_irx, size_mcman_irx, 0, NULL, NULL);
        SifExecModuleBuffer(&mcserv_irx, size_mcserv_irx, 0, NULL, NULL);
    }

    SifExecModuleBuffer(&padman_irx, size_padman_irx, 0, NULL, NULL);
#else
    SifLoadModule("rom0:SIO2MAN", 0, NULL);

    if (modload_mask & SYS_LOAD_MC_MODULES) {
        SifLoadModule("rom0:MCMAN", 0, NULL);
        SifLoadModule("rom0:MCSERV", 0, NULL);
    }

    SifLoadModule("rom0:PADMAN", 0, NULL);
#endif

    sysLoadModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL);

    if (modload_mask & SYS_LOAD_USB_MODULES) {
        usbLoadModules();
    }
    if (modload_mask & SYS_LOAD_ISOFS_MODULE) {
        sysLoadModuleBuffer(&isofs_irx, size_isofs_irx, 0, NULL);
    }

#ifdef VMC
    sysLoadModuleBuffer(&genvmc_irx, size_genvmc_irx, 0, NULL);
#endif

#ifdef PADEMU
    int ds3pads = 1; //only one pad enabled

    sysLoadModuleBuffer(&ds3usb_irx, size_ds3usb_irx, 4, (char *)&ds3pads);
    sysLoadModuleBuffer(&ds3bt_irx, size_ds3bt_irx, 4, (char *)&ds3pads);

    ds3usb_init();
    ds3bt_init();
#endif

    fileXioInit();
    poweroffInit();
}

void sysPowerOff(void)
{
    deinit(NO_EXCEPTION);
    fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0);
    poweroffShutdown();
}

static unsigned int crctab[0x400];

unsigned int USBA_crc32(char *string)
{
    int crc, table, count, byte;

    for (table = 0; table < 256; table++) {
        crc = table << 24;

        for (count = 8; count > 0; count--) {
            if (crc < 0)
                crc = crc << 1;
            else
                crc = (crc << 1) ^ 0x04C11DB7;
        }
        crctab[255 - table] = crc;
    }

    do {
        byte = string[count++];
        crc = crctab[byte ^ ((crc >> 24) & 0xFF)] ^ ((crc << 8) & 0xFFFFFF00);
    } while (string[count - 1] != 0);

    return crc;
}

int sysGetDiscID(char *hexDiscID)
{
    u8 key[16];

    if (sceCdStatus() == SCECdErOPENS) // If tray is open, error
        return -1;

    while (sceCdGetDiskType() == SCECdDETCT) {
        ;
    }
    if (sceCdGetDiskType() == SCECdNODISC)
        return -1;

    sceCdDiskReady(0);
    LOG("SYSTEM Disc drive is ready\n");
    int cdmode = sceCdGetDiskType();
    if (cdmode == SCECdNODISC)
        return -1;

    if ((cdmode != SCECdPS2DVD) && (cdmode != SCECdPS2CD) && (cdmode != SCECdPS2CDDA)) {
        sceCdStop();
        sceCdSync(0);
        LOG("SYSTEM Disc stopped, Disc is not ps2 disc!\n");
        return -2;
    }

    LOG("SYSTEM Disc standby\n");
    sceCdStandby();
    sceCdSync(0);

    LOG("SYSTEM Disc read key\n");
    if (sceCdReadKey(0, 0, 0x4b, key) == 0) {
        LOG("SYSTEM Cannot read CD/DVD key.\n");
        sceCdStop();
        sceCdSync(0);
        LOG("SYSTEM Disc stopped\n");
        return -3;
    }

    sceCdStop();

    // convert to hexadecimal string
    snprintf(hexDiscID, 15, "%02X %02X %02X %02X %02X", key[10], key[11], key[12], key[13], key[14]);
    LOG("SYSTEM PS2 Disc ID = %s\n", hexDiscID);

    sceCdSync(0);
    LOG("SYSTEM Disc stopped\n");

    return 1;
}

void sysExecExit()
{
    Exit(0);
}

//Module bits
#define CORE_IRX_USB 0x01
#define CORE_IRX_ETH 0x02
#define CORE_IRX_SMB 0x04
#define CORE_IRX_HDD 0x08
#define CORE_IRX_VMC 0x10
#define CORE_IRX_DEBUG 0x20
#define CORE_IRX_DECI2 0x40

#ifdef VMC
static unsigned int sendIrxKernelRAM(unsigned int modules, void *ModuleStorage, int size_cdvdman_irx, void **cdvdman_irx, int size_mcemu_irx, void **mcemu_irx)
{ // Send IOP modules that core must use to Kernel RAM
#else
static unsigned int sendIrxKernelRAM(unsigned int modules, void *ModuleStorage, int size_cdvdman_irx, void **cdvdman_irx)
{ // Send IOP modules that core must use to Kernel RAM
#endif
    irxtab_t *irxtable;
    irxptr_t *irxptr_tab;
    void *irxptr, *ioprp_image;
    int i, modcount;
    unsigned int curIrxSize, size_ioprp_image, total_size;

    irxtable = (irxtab_t *)ModuleStorage;
    irxptr_tab = (irxptr_t *)((unsigned char *)irxtable + sizeof(irxtab_t));
    ioprp_image = malloc(size_IOPRP_img + size_cdvdman_irx + size_cdvdfsv_irx + 256);
    size_ioprp_image = patch_IOPRP_image(ioprp_image, cdvdman_irx, size_cdvdman_irx);

    modcount = 0;
    //Basic modules
    irxptr_tab[modcount].info = size_udnl_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_UDNL);
    irxptr_tab[modcount++].ptr = (void *)&udnl_irx;
    irxptr_tab[modcount].info = size_ioprp_image | SET_OPL_MOD_ID(OPL_MODULE_ID_IOPRP);
    irxptr_tab[modcount++].ptr = ioprp_image;
    irxptr_tab[modcount].info = size_imgdrv_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_IMGDRV);
    irxptr_tab[modcount++].ptr = (void *)&imgdrv_irx;

#ifdef PADEMU
#define PADEMU_ARG || gEnablePadEmu
#else
#define PADEMU_ARG
#endif
    if ((modules & CORE_IRX_USB)PADEMU_ARG) {
        irxptr_tab[modcount].info = size_pusbd_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_USBD);
        irxptr_tab[modcount++].ptr = pusbd_irx;
    }
    if (modules & CORE_IRX_ETH) {
#ifdef __DECI2_DEBUG //FIXME: I don't know why, but the ingame SMAP driver cannot be used with the DECI2 modules. Perhaps that old bug with the network stack become unresponsive gets triggered? Until this is solved, use the normal SMAP driver.
        irxptr_tab[modcount].info = size_smap_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_SMAP);
        irxptr_tab[modcount++].ptr = (void *)&smap_irx;
#else
        irxptr_tab[modcount].info = size_smap_ingame_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_SMAP);
        irxptr_tab[modcount++].ptr = (void *)&smap_ingame_irx;
#endif
        irxptr_tab[modcount].info = size_ingame_smstcpip_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_SMSTCPIP);
        irxptr_tab[modcount++].ptr = (void *)&ingame_smstcpip_irx;
        irxptr_tab[modcount].info = size_smbinit_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_SMBINIT);
        irxptr_tab[modcount++].ptr = (void *)&smbinit_irx;
    }
#ifdef VMC
    if (modules & CORE_IRX_VMC) {
        irxptr_tab[modcount].info = size_mcemu_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_MCEMU);
        irxptr_tab[modcount++].ptr = (void *)mcemu_irx;
    }
#endif

#ifdef PADEMU
    if (gEnablePadEmu) {
        if (gPadEmuSettings & 0xFF) {
            irxptr_tab[modcount].info = size_bt_pademu_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_PADEMU);
            irxptr_tab[modcount++].ptr = (void *)&bt_pademu_irx;
        } else {
            irxptr_tab[modcount].info = size_usb_pademu_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_PADEMU);
            irxptr_tab[modcount++].ptr = (void *)&usb_pademu_irx;
        }
    }
#endif

#ifdef __INGAME_DEBUG
#ifdef __DECI2_DEBUG
    if (modules & CORE_IRX_DECI2) {
        irxptr_tab[modcount].info = size_drvtif_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_DRVTIF);
        irxptr_tab[modcount++].ptr = (void *)&drvtif_irx;
        irxptr_tab[modcount].info = size_tifinet_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_TIFINET);
        irxptr_tab[modcount++].ptr = (void *)&tifinet_irx;
    }
#else
    if (modules & CORE_IRX_DEBUG) {
        irxptr_tab[modcount].info = size_udptty_ingame_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_UDPTTY);
        irxptr_tab[modcount++].ptr = (void *)&udptty_ingame_irx;
        irxptr_tab[modcount].info = size_ioptrap_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_IOPTRAP);
        irxptr_tab[modcount++].ptr = (void *)&ioptrap_irx;
    }
#endif
#endif

    irxtable->modules = irxptr_tab;
    irxtable->count = modcount;

#ifdef __DECI2_DEBUG
    //For DECI2 debugging mode, the UDNL module will have to be stored within kernel RAM because there isn't enough space below user RAM.
    //total_size will hence not include the IOPRP image, but it's okay because the EE core is interested in protecting the module storage within user RAM.
    irxptr = (void *)0x00033000;
    LOG("SYSTEM DECI2 UDNL address start: %p end: %p\n", irxptr, irxptr + GET_OPL_MOD_SIZE(irxptr_tab[OPL_MODULE_ID_UDNL].info));
    DI();
    ee_kmode_enter();
    memcpy((void *)(0x80000000 | (unsigned int)irxptr), irxptr_tab[OPL_MODULE_ID_UDNL].ptr, GET_OPL_MOD_SIZE(irxptr_tab[OPL_MODULE_ID_UDNL].info));
    ee_kmode_exit();
    EI();

    irxptr_tab[0].ptr = irxptr; //UDNL is the first module.
#endif

    total_size = (sizeof(irxtab_t) + sizeof(irxptr_t) * modcount + 0xF) & ~0xF;
    irxptr = (void *)((((unsigned int)irxptr_tab + sizeof(irxptr_t) * modcount) + 0xF) & ~0xF);

#ifdef __DECI2_DEBUG
    for (i = 1; i < modcount; i++) {
#else
    for (i = 0; i < modcount; i++) {
#endif
        curIrxSize = GET_OPL_MOD_SIZE(irxptr_tab[i].info);

        if (curIrxSize > 0) {
            LOG("SYSTEM IRX %u address start: %p end: %p\n", GET_OPL_MOD_ID(irxptr_tab[i].info), irxptr, irxptr + curIrxSize);
            memcpy(irxptr, irxptr_tab[i].ptr, curIrxSize);

            irxptr_tab[i].ptr = irxptr;
            irxptr += ((curIrxSize + 0xF) & ~0xF);
            total_size += ((curIrxSize + 0xF) & ~0xF);
        } else {
            irxptr_tab[i].ptr = NULL;
        }
    }

    free(ioprp_image);

    LOG("SYSTEM IRX STORAGE %p - %p\n", ModuleStorage, (u8 *)ModuleStorage + total_size);

    return total_size;
}

#ifdef GSM
static void PrepareGSM(char *cmdline)
{
    /* Preparing GSM */
    LOG("Preparing GSM...\n");
    // Pre-defined vmodes
    // Some of following vmodes gives BOSD and/or freezing, depending on the console BIOS version, TV/Monitor set, PS2 cable (composite, component, VGA, ...)
    // Therefore there are many variables involved here that can lead us to success or fail, depending on the already mentioned circumstances.
    //
    static const predef_vmode_struct predef_vmode[30] = {
        //																		  DH    DW      MAGV    MAGH     DY     DX					  VS  VDP     VBPE VBP    VFPE  VFP
        {GS_INTERLACED, GS_MODE_NTSC, GS_FIELD, makeDISPLAY(447, 2559, 0, 3, 46, 700), makeSYNCV(6, 480, 6, 26, 6, 1)},
        {GS_INTERLACED, GS_MODE_NTSC, GS_FRAME, makeDISPLAY(223, 2559, 0, 3, 26, 700), makeSYNCV(6, 480, 6, 26, 6, 2)},
        {GS_INTERLACED, GS_MODE_PAL, GS_FIELD, makeDISPLAY(511, 2559, 0, 3, 70, 720), makeSYNCV(5, 576, 5, 33, 5, 1)},
        {GS_INTERLACED, GS_MODE_PAL, GS_FRAME, makeDISPLAY(255, 2559, 0, 3, 37, 720), makeSYNCV(5, 576, 5, 33, 5, 4)},
        {GS_INTERLACED, GS_MODE_PAL, GS_FIELD, makeDISPLAY(447, 2559, 0, 3, 46, 700), makeSYNCV(6, 480, 6, 26, 6, 1)},
        {GS_INTERLACED, GS_MODE_PAL, GS_FRAME, makeDISPLAY(223, 2559, 0, 3, 26, 700), makeSYNCV(6, 480, 6, 26, 6, 2)},
        {GS_NONINTERLACED, GS_MODE_DTV_480P, GS_FRAME, makeDISPLAY(255, 2559, 0, 1, 12, 736), makeSYNCV(6, 483, 3072, 30, 0, 6)},
        {GS_NONINTERLACED, GS_MODE_DTV_576P, GS_FRAME, makeDISPLAY(255, 2559, 0, 1, 23, 756), makeSYNCV(5, 576, 0, 39, 0, 5)},
        {GS_NONINTERLACED, GS_MODE_DTV_480P, GS_FRAME, makeDISPLAY(479, 1279, 0, 1, 51, 308), makeSYNCV(6, 483, 3072, 30, 0, 6)},
        {GS_NONINTERLACED, GS_MODE_DTV_576P, GS_FRAME, makeDISPLAY(575, 1279, 0, 1, 64, 320), makeSYNCV(5, 576, 0, 39, 0, 5)},
        {GS_NONINTERLACED, GS_MODE_DTV_720P, GS_FRAME, makeDISPLAY(719, 1279, 1, 1, 24, 302), makeSYNCV(5, 720, 0, 20, 0, 5)},
        {GS_INTERLACED, GS_MODE_DTV_1080I, GS_FIELD, makeDISPLAY(1079, 1919, 1, 2, 48, 238), makeSYNCV(10, 1080, 2, 28, 0, 5)},
        {GS_INTERLACED, GS_MODE_DTV_1080I, GS_FRAME, makeDISPLAY(1079, 1919, 0, 2, 48, 238), makeSYNCV(10, 1080, 2, 28, 0, 5)},
        {GS_NONINTERLACED, GS_MODE_DTV_1080P, GS_FRAME, makeDISPLAY(1079, 1919, 1, 2, 48, 238), makeSYNCV(10, 1080, 2, 28, 0, 5)},
        {GS_NONINTERLACED, GS_MODE_VGA_640_60, GS_FRAME, makeDISPLAY(479, 1279, 0, 1, 54, 276), makeSYNCV(2, 480, 0, 33, 0, 10)},
        {GS_NONINTERLACED, GS_MODE_VGA_640_72, GS_FRAME, makeDISPLAY(479, 1279, 0, 1, 18, 330), makeSYNCV(3, 480, 0, 28, 0, 9)},
        {GS_NONINTERLACED, GS_MODE_VGA_640_75, GS_FRAME, makeDISPLAY(479, 1279, 0, 1, 18, 360), makeSYNCV(3, 480, 0, 16, 0, 1)},
        {GS_NONINTERLACED, GS_MODE_VGA_640_85, GS_FRAME, makeDISPLAY(479, 1279, 0, 1, 18, 260), makeSYNCV(3, 480, 0, 16, 0, 1)},
        {GS_INTERLACED, GS_MODE_VGA_640_60, GS_FIELD, makeDISPLAY(959, 1279, 1, 1, 128, 291), makeSYNCV(2, 992, 0, 33, 0, 10)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_56, GS_FRAME, makeDISPLAY(599, 1599, 0, 1, 25, 450), makeSYNCV(2, 600, 0, 22, 0, 1)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_60, GS_FRAME, makeDISPLAY(599, 1599, 0, 1, 25, 465), makeSYNCV(4, 600, 0, 23, 0, 1)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_72, GS_FRAME, makeDISPLAY(599, 1599, 0, 1, 25, 465), makeSYNCV(6, 600, 0, 23, 0, 37)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_75, GS_FRAME, makeDISPLAY(599, 1599, 0, 1, 25, 510), makeSYNCV(3, 600, 0, 21, 0, 1)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_85, GS_FRAME, makeDISPLAY(599, 1599, 0, 1, 15, 500), makeSYNCV(3, 600, 0, 27, 0, 1)},
        {GS_NONINTERLACED, GS_MODE_VGA_1024_60, GS_FRAME, makeDISPLAY(767, 2047, 0, 2, 30, 580), makeSYNCV(6, 768, 0, 29, 0, 3)},
        {GS_NONINTERLACED, GS_MODE_VGA_1024_70, GS_FRAME, makeDISPLAY(767, 1023, 0, 0, 30, 266), makeSYNCV(6, 768, 0, 29, 0, 3)},
        {GS_NONINTERLACED, GS_MODE_VGA_1024_75, GS_FRAME, makeDISPLAY(767, 1023, 0, 0, 30, 260), makeSYNCV(3, 768, 0, 28, 0, 1)},
        {GS_NONINTERLACED, GS_MODE_VGA_1024_85, GS_FRAME, makeDISPLAY(767, 1023, 0, 0, 30, 290), makeSYNCV(3, 768, 0, 36, 0, 1)},
        {GS_NONINTERLACED, GS_MODE_VGA_1280_60, GS_FRAME, makeDISPLAY(1023, 1279, 1, 1, 40, 350), makeSYNCV(3, 1024, 0, 38, 0, 1)},
        {GS_NONINTERLACED, GS_MODE_VGA_1280_75, GS_FRAME, makeDISPLAY(1023, 1279, 1, 1, 40, 350), makeSYNCV(3, 1024, 0, 38, 0, 1)}}; //ends predef_vmode definition

    sprintf(cmdline, "%d %d %d %lu %lu %u %u %u", predef_vmode[gGSMVMode].interlace,
            predef_vmode[gGSMVMode].mode,
            predef_vmode[gGSMVMode].ffmd,
            predef_vmode[gGSMVMode].display,
            predef_vmode[gGSMVMode].syncv,
            ((predef_vmode[gGSMVMode].ffmd) << 1) | (predef_vmode[gGSMVMode].interlace),
            gGSMXOffset,
            gGSMYOffset);
}
#endif

#ifdef __DECI2_DEBUG
/*
	Look for the start of the EE DECI2 manager initialization function.

	The stock EE kernel has no reset function, but the EE kernel is most likely already primed to self-destruct and in need of a good reset.
	What happens is that the OSD initializes the EE DECI2 TTY protocol at startup, but the EE DECI2 manager is never aware that the OSDSYS ever loads other programs.

	As a result, the EE kernel crashes immediately when the EE TTY gets used (when the IOP side of DECI2 comes up), when it invokes whatever that exists at the OSD's old ETTY handler's location. :(
*/
static int ResetDECI2(void)
{
    int result;
    unsigned int i, *ptr;
    void (*pDeci2ManagerInit)(void);
    static const unsigned int Deci2ManagerInitPattern[] = {
        0x3c02bf80, //lui v0, $bf80
        0x3c04bfc0, //lui a0, $bfc0
        0x34423800, //ori v0, v0, $3800
        0x34840102  //ori a0, a0, $0102
    };

    DI();
    ee_kmode_enter();

    result = -1;
    ptr = (void *)0x80000000;
    for (i = 0; i < 0x20000 / 4; i++) {
        if (ptr[i + 0] == Deci2ManagerInitPattern[0] &&
            ptr[i + 1] == Deci2ManagerInitPattern[1] &&
            ptr[i + 2] == Deci2ManagerInitPattern[2] &&
            ptr[i + 3] == Deci2ManagerInitPattern[3]) {
            pDeci2ManagerInit = (void *)&ptr[i - 14];
            pDeci2ManagerInit();
            result = 0;
            break;
        }
    }

    ee_kmode_exit();
    EI();

    return result;
}
#endif

void sysLaunchLoaderElf(char *filename, char *mode_str, int size_cdvdman_irx, void **cdvdman_irx, MCEMU int EnablePS2Logo, unsigned int compatflags)
{
    unsigned int modules, ModuleStorageSize;
    void *ModuleStorage;
    u8 local_ip_address[4], local_netmask[4], local_gateway[4];
    u8 *boot_elf = NULL;
    elf_header_t *eh;
    elf_pheader_t *eph;
    void *pdata;
    int i;
#ifdef GSM
#define GSM_ARGS 1
#else
#define GSM_ARGS 0
#endif
#ifdef CHEAT
#define CHEAT_ARGS 1
#else
#define CHEAT_ARGS 0
#endif
#ifdef PADEMU
#define PADEMU_ARGS 2
#else
#define PADEMU_ARGS 0
#endif
    char *argv[4 + GSM_ARGS + CHEAT_ARGS + PADEMU_ARGS];
    char ModStorageConfig[32];
    char config_str[256];
#ifdef GSM
    char gsm_config_str[256];
#endif

    ethGetNetConfig(local_ip_address, local_netmask, local_gateway);
#ifndef __DEBUG
    AddHistoryRecordUsingFullPath(filename);
#endif

    if (gExitPath[0] == '\0')
        strncpy(gExitPath, "Browser", sizeof(gExitPath));

    //Wipe the low user memory region, since this region might not be wiped after OPL's EE core is installed.
    memset((void *)0x00082000, 0, 0x00100000 - 0x00082000);

    if (!strcmp(mode_str, "USB_MODE"))
        modules = CORE_IRX_USB;
    else if (!strcmp(mode_str, "ETH_MODE"))
        modules = CORE_IRX_ETH | CORE_IRX_SMB;
    else
        modules = CORE_IRX_HDD;

    ModuleStorage = (void *)((compatflags & COMPAT_MODE_7) ? OPL_MOD_STORAGE_HI : OPL_MOD_STORAGE);

#ifdef __DECI2_DEBUG
    modules |= CORE_IRX_DECI2 | CORE_IRX_ETH;
#elif defined(__INGAME_DEBUG)
    modules |= CORE_IRX_DEBUG | CORE_IRX_ETH;
#endif

#ifdef VMC
    modules |= CORE_IRX_VMC;
    LOG("SYSTEM LaunchLoaderElf loading modules with size_mcemu_irx = %d\n", size_mcemu_irx);
    ModuleStorageSize = (sendIrxKernelRAM(modules, ModuleStorage, size_cdvdman_irx, cdvdman_irx, size_mcemu_irx, mcemu_irx) + 0x3F) & ~0x3F;
#else
    LOG("SYSTEM LaunchLoaderElf loading modules\n");
    ModuleStorageSize = (sendIrxKernelRAM(modules, ModuleStorage, size_cdvdman_irx, cdvdman_irx) + 0x3F) & ~0x3F;
#endif
    sprintf(ModStorageConfig, "%u %u", (unsigned int)ModuleStorage, (unsigned int)ModuleStorage + ModuleStorageSize);

    if (!gDisableDebug) {
        char text[80];
        float usage;
        usage = (float)(ModuleStorageSize) / (float)(2 * 1024 * 1024) * 100;

        snprintf(text, sizeof(text), "IOP Usage:%.2f%%,by %s(CDVDFSV+CDVDMAN)", (float)usage, mode_str);
#ifdef VMC
        strcat(text, "+VMC");
#endif
        guiWarning(text, 20);
    }


#ifdef __DECI2_DEBUG
    ResetDECI2();
#endif

    // NB: LOADER.ELF is embedded
    boot_elf = (u8 *)&eecore_elf;
    eh = (elf_header_t *)boot_elf;
    eph = (elf_pheader_t *)(boot_elf + eh->phoff);

    // Scan through the ELF's program headers and copy them into RAM, then
    // zero out any non-loaded regions.
    for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;

        pdata = (void *)(boot_elf + eph[i].offset);
        memcpy(eph[i].vaddr, pdata, eph[i].filesz);

        if (eph[i].memsz > eph[i].filesz)
            memset(eph[i].vaddr + eph[i].filesz, 0, eph[i].memsz - eph[i].filesz);
    }

#ifdef CHEAT
#define CHEAT_SPECIFIER " %u"
#define CHEAT_ARGUMENT , gEnableCheat ? (unsigned int)gCheatList : 0
#else
#define CHEAT_SPECIFIER
#define CHEAT_ARGUMENT
#endif

#ifdef GSM
#define GSM_SPECIFIER " %d"
#define GSM_ARGUMENT , gEnableGSM
#else
#define GSM_SPECIFIER
#define GSM_ARGUMENT
#endif

#ifdef PADEMU
#define PADEMU_SPECIFIER " %d, %d"
#define PADEMU_ARGUMENT , gEnablePadEmu, (gPadEmuSettings >> 8)
#else
#define PADEMU_SPECIFIER
#define PADEMU_ARGUMENT
#endif

    i = 0;
    sprintf(config_str, "%s %d %d %s %d %u.%u.%u.%u %u.%u.%u.%u %u.%u.%u.%u %d" CHEAT_SPECIFIER GSM_SPECIFIER PADEMU_SPECIFIER,
            mode_str, gDisableDebug, EnablePS2Logo, gExitPath, gHDDSpindown,
            local_ip_address[0], local_ip_address[1], local_ip_address[2], local_ip_address[3],
            local_netmask[0], local_netmask[1], local_netmask[2], local_netmask[3],
            local_gateway[0], local_gateway[1], local_gateway[2], local_gateway[3],
            gETHOpMode
                CHEAT_ARGUMENT GSM_ARGUMENT PADEMU_ARGUMENT);
    argv[i] = config_str;
    i++;

    argv[i] = ModStorageConfig;
    i++;

    argv[i] = filename;
    i++;

    char cmask[10];
    snprintf(cmask, 10, "%d", compatflags);
    argv[i] = cmask;
    i++;

#ifdef GSM
    PrepareGSM(gsm_config_str);
    argv[i] = gsm_config_str;
    i++;
#endif

    if (!gDisableDebug) {
        guiWarning("Let's go.", 10);
        // deinit stuff
        unloadPads();
        ioEnd();
        guiEnd();
        menuEnd();
        lngEnd();
        thmEnd();
        rmEnd();
    }

#ifdef PADEMU
    ds3usb_reset();
    ds3bt_reset();
#endif
    // Let's go.
    fileXioExit();
    SifExitRpc();

    FlushCache(0);
    FlushCache(2);

    ExecPS2((void *)eh->entry, 0, i, argv);
}

int sysExecElf(char *path)
{
    u8 *boot_elf = NULL;
    elf_header_t *eh;
    elf_pheader_t *eph;
    void *pdata;
    int i;
    char *elf_argv[1];

    // NB: ELFLDR.ELF is embedded
    boot_elf = (u8 *)&elfldr_elf;
    eh = (elf_header_t *)boot_elf;
    if (_lw((u32)&eh->ident) != ELF_MAGIC)
        while (1)
            ;

    eph = (elf_pheader_t *)(boot_elf + eh->phoff);

    // Scan through the ELF's program headers and copy them into RAM, then
    // zero out any non-loaded regions.
    for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;

        pdata = (void *)(boot_elf + eph[i].offset);
        memcpy(eph[i].vaddr, pdata, eph[i].filesz);

        if (eph[i].memsz > eph[i].filesz)
            memset(eph[i].vaddr + eph[i].filesz, 0, eph[i].memsz - eph[i].filesz);
    }

    // Let's go.
    fileXioExit();
    SifExitRpc();

    elf_argv[0] = path;

    FlushCache(0);
    FlushCache(2);

    ExecPS2((void *)eh->entry, 0, 1, elf_argv);

    return 0;
}

int sysCheckMC(void)
{
    int dummy, ret;

    mcGetInfo(0, 0, &dummy, &dummy, &dummy);
    mcSync(0, NULL, &ret);

    if (-1 == ret || 0 == ret)
        return 0;

    mcGetInfo(1, 0, &dummy, &dummy, &dummy);
    mcSync(0, NULL, &ret);

    if (-1 == ret || 0 == ret)
        return 1;

    return -11;
}

#ifdef VMC
// createSize == -1 : delete, createSize == 0 : probing, createSize > 0 : creation
int sysCheckVMC(const char *prefix, const char *sep, char *name, int createSize, vmc_superblock_t *vmc_superblock)
{
    int size = -1;
    char path[256];
    snprintf(path, sizeof(path), "%sVMC%s%s.bin", prefix, sep, name);

    if (createSize == -1)
        fileXioRemove(path);
    else {
        int fd = fileXioOpen(path, O_RDONLY, FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH);
        if (fd >= 0) {
            size = fileXioLseek(fd, 0, SEEK_END);

            if (vmc_superblock) {
                memset(vmc_superblock, 0, sizeof(vmc_superblock_t));
                fileXioLseek(fd, 0, SEEK_SET);
                fileXioRead(fd, (void *)vmc_superblock, sizeof(vmc_superblock_t));

                LOG("SYSTEM File size  : 0x%X\n", size);
                LOG("SYSTEM Magic      : %s\n", vmc_superblock->magic);
                LOG("SYSTEM Card type  : %d\n", vmc_superblock->mc_type);
                LOG("SYSTEM Flags      : 0x%X\n", (vmc_superblock->mc_flag & 0xFF) | 0x100);
                LOG("SYSTEM Page_size  : 0x%X\n", vmc_superblock->page_size);
                LOG("SYSTEM Block_size : 0x%X\n", vmc_superblock->pages_per_block);
                LOG("SYSTEM Card_size  : 0x%X\n", vmc_superblock->pages_per_cluster * vmc_superblock->clusters_per_card);

                if (!strncmp(vmc_superblock->magic, "Sony PS2 Memory Card Format", 27) && vmc_superblock->mc_type == 0x2 && size == vmc_superblock->pages_per_cluster * vmc_superblock->clusters_per_card * vmc_superblock->page_size) {
                    LOG("SYSTEM VMC file structure valid: %s\n", path);
                } else
                    size = 0;
            }

            if (size % 1048576) // invalid size, should be a an integer (8, 16, 32, 64, ...)
                size = 0;
            else
                size /= 1048576;

            fileXioClose(fd);

            if (createSize && (createSize != size))
                fileXioRemove(path);
        }


        if (createSize && (createSize != size)) {
            createVMCparam_t createParam;
            strcpy(createParam.VMC_filename, path);
            createParam.VMC_size_mb = createSize;
            createParam.VMC_blocksize = 16;
            createParam.VMC_thread_priority = 0x0f;
            createParam.VMC_card_slot = -1;
            fileXioDevctl("genvmc:", 0xC0DE0001, (void *)&createParam, sizeof(createParam), NULL, 0);
        }
    }
    return size;
}
#endif
