#include "include/opl.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/supportbase.h"
#include "include/usb-ioctl.h"
#include "include/usbsupport.h"
#include "include/util.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/ioman.h"
#include "include/system.h"
#ifdef CHEAT
#include "include/cheatman.h"
#endif
#include "modules/iopcore/common/cdvd_config.h"

extern void *usb_cdvdman_irx;
extern int size_usb_cdvdman_irx;

extern void *usbd_irx;
extern int size_usbd_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

extern void *usbhdfsdfsv_irx;
extern int size_usbhdfsdfsv_irx;

#ifdef VMC
extern void *usb_mcemu_irx;
extern int size_usb_mcemu_irx;
#endif

void *pusbd_irx = NULL;
int size_pusbd_irx = 0;

static char usbPrefix[40]; //Contains the full path to the folder where all the games are.
static int usbULSizePrev = -2;
static unsigned char usbModifiedCDPrev[8];
static unsigned char usbModifiedDVDPrev[8];
static int usbGameCount = 0;
static base_game_info_t *usbGames;

// forward declaration
static item_list_t usbGameList;

//Identifies the partition that the specified file is stored on and generates a full path to it.
int usbFindPartition(char *target, char *name)
{
    int i, fd;
    char path[256];

    for (i = 0; i < 5; i++) {
        if (gUSBPrefix[0] != '\0')
            sprintf(path, "mass%d:%s/%s", i, gUSBPrefix, name);
        else
            sprintf(path, "mass%d:%s", i, name);
        fd = fileXioOpen(path, O_RDONLY, 0666);

        if (fd >= 0) {
            if (gUSBPrefix[0] != '\0')
                sprintf(target, "mass%d:%s/", i, gUSBPrefix);
            else
                sprintf(target, "mass%d:", i);
            fileXioClose(fd);
            return 1;
        }
    }

    // default to first partition (for themes, ...)
    if (gUSBPrefix[0] != '\0')
        sprintf(target, "mass0:%s/", gUSBPrefix);
    else
        sprintf(target, "mass0:");
    return 0;
}

#define USBHDFSDFSV_FUNCNUM(x) (('U' << 8) | (x))

static unsigned int UsbGeneration = 0;

static void usbEventHandler(void *packet, void *opt)
{
    UsbGeneration++;
}

void usbLoadModules(void)
{
    LOG("USBSUPPORT LoadModules\n");
    //first search for a custom usbd module in MC?
    pusbd_irx = readFile("mc?:BEDATA-SYSTEM/USBD.IRX", -1, &size_pusbd_irx);
    if (!pusbd_irx) {
        pusbd_irx = readFile("mc?:BADATA-SYSTEM/USBD.IRX", -1, &size_pusbd_irx);
        if (!pusbd_irx) {
            pusbd_irx = readFile("mc?:BIDATA-SYSTEM/USBD.IRX", -1, &size_pusbd_irx);
            if (!pusbd_irx) { // If don't exist it uses embedded
                pusbd_irx = (void *)&usbd_irx;
                size_pusbd_irx = size_usbd_irx;
            }
        }
    }

    sysLoadModuleBuffer(pusbd_irx, size_pusbd_irx, 0, NULL);
    sysLoadModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL);
    sysLoadModuleBuffer(&usbhdfsdfsv_irx, size_usbhdfsdfsv_irx, 0, NULL);
    SifAddCmdHandler(0, &usbEventHandler, NULL);

    LOG("USBSUPPORT Modules loaded\n");
}

void usbInit(void)
{
    LOG("USBSUPPORT Init\n");
    usbULSizePrev = -2;
    memset(usbModifiedCDPrev, 0, 8);
    memset(usbModifiedDVDPrev, 0, 8);
    usbGameCount = 0;
    usbGames = NULL;
    configGetInt(configGetByType(CONFIG_OPL), "usb_frames_delay", &usbGameList.delay);
    usbGameList.enabled = 1;
}

item_list_t *usbGetObject(int initOnly)
{
    if (initOnly && !usbGameList.enabled)
        return NULL;
    return &usbGameList;
}

static int usbNeedsUpdate(void)
{
    char path[256];
    static unsigned int OldGeneration = 0;
    static unsigned char ThemesLoaded = 0;
    int result = 0;
    iox_stat_t stat;

    if (usbULSizePrev != -2 && OldGeneration == UsbGeneration)
        return 0;
    OldGeneration = UsbGeneration;

    usbFindPartition(usbPrefix, "ul.cfg");

    sprintf(path, "%sCD", usbPrefix);
    if (fileXioGetStat(path, &stat) != 0)
        memset(stat.mtime, 0, 8);
    if (memcmp(usbModifiedCDPrev, stat.mtime, 8)) {
        memcpy(usbModifiedCDPrev, stat.mtime, 8);
        result = 1;
    }

    sprintf(path, "%sDVD", usbPrefix);
    if (fileXioGetStat(path, &stat) != 0)
        memset(stat.mtime, 0, 8);
    if (memcmp(usbModifiedDVDPrev, stat.mtime, 8)) {
        memcpy(usbModifiedDVDPrev, stat.mtime, 8);
        result = 1;
    }

    if (!sbIsSameSize(usbPrefix, usbULSizePrev))
        result = 1;

    // update Themes
    if (!ThemesLoaded) {
        sprintf(path, "%sTHM", usbPrefix);
        if (thmAddElements(path, "/", usbGameList.mode) > 0)
            ThemesLoaded = 1;
    }

    sbCreateFolders(usbPrefix, 1);

    return result;
}

static int usbUpdateGameList(void)
{
    sbReadList(&usbGames, usbPrefix, &usbULSizePrev, &usbGameCount);
    return usbGameCount;
}

static int usbGetGameCount(void)
{
    return usbGameCount;
}

static void *usbGetGame(int id)
{
    return (void *)&usbGames[id];
}

static char *usbGetGameName(int id)
{
    return usbGames[id].name;
}

static int usbGetGameNameLength(int id)
{
    return ((usbGames[id].format != GAME_FORMAT_USBLD) ? ISO_GAME_NAME_MAX + 1 : UL_GAME_NAME_MAX + 1);
}

static char *usbGetGameStartup(int id)
{
    return usbGames[id].startup;
}

#ifndef __CHILDPROOF
static void usbDeleteGame(int id)
{
    sbDelete(&usbGames, usbPrefix, "/", usbGameCount, id);
    usbULSizePrev = -2;
}

static void usbRenameGame(int id, char *newName)
{
    char oldpath[256], newpath[256];
    base_game_info_t *game = &usbGames[id];
    int fd;

    if (game->format != GAME_FORMAT_USBLD) {
        if (game->format == GAME_FORMAT_OLD_ISO) {
            if (game->media == 0x12) {
                snprintf(oldpath, sizeof(oldpath), "%sCD/%s.%s%s", usbPrefix, game->startup, game->name, game->extension);
                snprintf(newpath, sizeof(newpath), "%sCD/%s.%s%s", usbPrefix, game->startup, newName, game->extension);
            } else {
                snprintf(oldpath, sizeof(oldpath), "%sDVD/%s.%s%s", usbPrefix, game->startup, game->name, game->extension);
                snprintf(newpath, sizeof(newpath), "%sDVD/%s.%s%s", usbPrefix, game->startup, newName, game->extension);
            }
        } else {
            if (game->media == 0x12) {
                snprintf(oldpath, sizeof(oldpath), "%sCD/%s%s", usbPrefix, game->name, game->extension);
                snprintf(newpath, sizeof(newpath), "%sCD/%s%s", usbPrefix, newName, game->extension);
            } else {
                snprintf(oldpath, sizeof(oldpath), "%sDVD/%s%s", usbPrefix, game->name, game->extension);
                snprintf(newpath, sizeof(newpath), "%sDVD/%s%s", usbPrefix, newName, game->extension);
            }
        }

        if ((fd = fileXioOpen(oldpath, O_RDONLY)) >= 0) {
            fileXioIoctl(fd, USBMASS_IOCTL_RENAME, newpath);
            fileXioClose(fd);
        }
    } else {
        const char *pathStr = "%sul.%08X.%s.%02x";
        unsigned int oldcrc = USBA_crc32(game->name);
        unsigned int newcrc = USBA_crc32(newName);
        int i;

        for (i = 0; i < game->parts; i++) {
            snprintf(oldpath, sizeof(oldpath), pathStr, usbPrefix, oldcrc, game->startup, i);
            snprintf(newpath, sizeof(newpath), pathStr, usbPrefix, newcrc, game->startup, i);
            if ((fd = fileXioOpen(oldpath, O_RDONLY)) >= 0) {
                fileXioIoctl(fd, USBMASS_IOCTL_RENAME, newpath);
                fileXioClose(fd);
            }
        }

        memset(game->name, 0, UL_GAME_NAME_MAX + 1);
        memcpy(game->name, newName, UL_GAME_NAME_MAX);
        sbRebuildULCfg(&usbGames, usbPrefix, usbGameCount, -1);
    }

    usbULSizePrev = -2;
}
#endif

static void usbLaunchGame(int id, config_set_t *configSet)
{
    int i, fd, index, compatmask = 0;
    int EnablePS2Logo = 0;
#ifdef CHEAT
    int result;
#endif
#ifdef VMC
    unsigned int start;
#endif
    unsigned int startCluster;
    char partname[256], filename[32];
    base_game_info_t *game = &usbGames[id];
    struct cdvdman_settings_usb *settings;
    u32 layer1_start, layer1_offset;
    unsigned short int layer1_part;

#ifdef VMC
    char vmc_name[32], vmc_path[256], have_error = 0;
    int vmc_id, size_mcemu_irx = 0;
    usb_vmc_infos_t usb_vmc_infos;
    vmc_superblock_t vmc_superblock;

    for (vmc_id = 0; vmc_id < 2; vmc_id++) {
        memset(&usb_vmc_infos, 0, sizeof(usb_vmc_infos_t));
        configGetVMC(configSet, vmc_name, sizeof(vmc_name), vmc_id);
        if (vmc_name[0]) {
            have_error = 1;
            if (sysCheckVMC(usbPrefix, "/", vmc_name, 0, &vmc_superblock) > 0) {
                usb_vmc_infos.flags = vmc_superblock.mc_flag & 0xFF;
                usb_vmc_infos.flags |= 0x100;
                usb_vmc_infos.specs.page_size = vmc_superblock.page_size;
                usb_vmc_infos.specs.block_size = vmc_superblock.pages_per_block;
                usb_vmc_infos.specs.card_size = vmc_superblock.pages_per_cluster * vmc_superblock.clusters_per_card;

                sprintf(vmc_path, "%sVMC/%s.bin", usbPrefix, vmc_name);

                fd = fileXioOpen(vmc_path, O_RDONLY);
                if (fd >= 0) {
                    if ((start = (unsigned int)fileXioIoctl(fd, USBMASS_IOCTL_GET_LBA, vmc_path)) != 0 && (startCluster = (unsigned int)fileXioIoctl(fd, USBMASS_IOCTL_GET_CLUSTER, vmc_path)) != 0) {

                        // Check VMC cluster chain for fragmentation (write operation can cause damage to the filesystem).
                        if (fileXioDevctl("xmass0:", XUSBHDFSD_CHECK_CLUSTER_CHAIN, &startCluster, 4, NULL, 0) != 0) {
                            LOG("USBSUPPORT Cluster Chain OK\n");
                            have_error = 0;
                            usb_vmc_infos.active = 1;
                            usb_vmc_infos.start_sector = start;
                            LOG("USBSUPPORT VMC slot %d start: 0x%X\n", vmc_id, start);
                        } else {
                            LOG("USBSUPPORT Cluster Chain NG\n");
                        }
                    }

                    fileXioClose(fd);
                }
            }
        }

        if (have_error) {
            char error[256];
            snprintf(error, sizeof(error), _l(_STR_ERR_VMC_CONTINUE), vmc_name, (vmc_id + 1));
            if (!guiMsgBox(error, 1, NULL)) {
                return;
            }
        }

        for (i = 0; i < size_usb_mcemu_irx; i++) {
            if (((u32 *)&usb_mcemu_irx)[i] == (0xC0DEFAC0 + vmc_id)) {
                if (usb_vmc_infos.active)
                    size_mcemu_irx = size_usb_mcemu_irx;
                memcpy(&((u32 *)&usb_mcemu_irx)[i], &usb_vmc_infos, sizeof(usb_vmc_infos_t));
                break;
            }
        }
    }
#endif

    void **irx = &usb_cdvdman_irx;
    int irx_size = size_usb_cdvdman_irx;
    compatmask = sbPrepare(game, configSet, irx_size, irx, &index);
    settings = (struct cdvdman_settings_usb *)((u8 *)irx + index);
    for (i = 0; i < game->parts; i++) {
        switch (game->format) {
            case GAME_FORMAT_ISO:
                sprintf(partname, "%s%s/%s%s", usbPrefix, (game->media == 0x12) ? "CD" : "DVD", game->name, game->extension);
                break;
            case GAME_FORMAT_OLD_ISO:
                sprintf(partname, "%s%s/%s.%s%s", usbPrefix, (game->media == 0x12) ? "CD" : "DVD", game->startup, game->name, game->extension);
                break;
            default: //USBExtreme format.
                sprintf(partname, "%sul.%08X.%s.%02x", usbPrefix, USBA_crc32(game->name), game->startup, i);
        }

        fd = fileXioOpen(partname, O_RDONLY);
        if (fd >= 0) {
            settings->LBAs[i] = fileXioIoctl(fd, USBMASS_IOCTL_GET_LBA, partname);
            if (gCheckUSBFragmentation) {
                if ((startCluster = (unsigned int)fileXioIoctl(fd, USBMASS_IOCTL_GET_CLUSTER, partname)) == 0 || fileXioDevctl("xmass0:", XUSBHDFSD_CHECK_CLUSTER_CHAIN, &startCluster, 4, NULL, 0) == 0) {

                    fileXioClose(fd);
                    //Game is fragmented. Do not continue.
                    if (settings != NULL)
                        sbUnprepare(&settings->common);

                    guiMsgBox(_l(_STR_ERR_FRAGMENTED), 0, NULL);
                    return;
                }
            }

            if ((gPS2Logo) && (i == 0))
                EnablePS2Logo = CheckPS2Logo(fd, 0);

            fileXioClose(fd);
        } else {
            //Unable to open part of the game. Do not continue.
            if (settings != NULL)
                sbUnprepare(&settings->common);
            guiMsgBox(_l(_STR_ERR_FILE_INVALID), 0, NULL);
            return;
        }
    }

    //Initialize layer 1 information.
    switch (game->format) {
        case GAME_FORMAT_ISO:
            sprintf(partname, "%s%s/%s%s", usbPrefix, (game->media == 0x12) ? "CD" : "DVD", game->name, game->extension);
            break;
        case GAME_FORMAT_OLD_ISO:
            sprintf(partname, "%s%s/%s.%s%s", usbPrefix, (game->media == 0x12) ? "CD" : "DVD", game->startup, game->name, game->extension);
            break;
        default: //USBExtreme format.
            sprintf(partname, "%sul.%08X.%s.00", usbPrefix, USBA_crc32(game->name), game->startup);
    }

    layer1_start = sbGetISO9660MaxLBA(partname);

    switch (game->format) {
        case GAME_FORMAT_USBLD:
            layer1_part = layer1_start / 0x80000;
            layer1_offset = layer1_start % 0x80000;
            sprintf(partname, "%sul.%08X.%s.%02x", usbPrefix, USBA_crc32(game->name), game->startup, layer1_part);
            break;
        default: //Raw ISO9660 disc image; one part.
            layer1_part = 0;
            layer1_offset = layer1_start;
    }

    if (sbProbeISO9660(partname, game, layer1_offset) != 0) {
        layer1_start = 0;
        LOG("DVD detected.\n");
    } else {
        layer1_start -= 16;
        LOG("DVD-DL layer 1 @ part %u sector 0x%lx.\n", layer1_part, layer1_offset);
    }
    settings->common.layer1_start = layer1_start;

#ifdef CHEAT
    if ((result = sbLoadCheats(usbPrefix, game->startup)) < 0) {
        switch (result) {
            case -ENOENT:
                guiWarning(_l(_STR_NO_CHEATS_FOUND), 10);
                break;
            default:
                guiWarning(_l(_STR_ERR_CHEATS_LOAD_FAILED), 10);
        }
    }
#endif

    if (gRememberLastPlayed) {
        configSetStr(configGetByType(CONFIG_LAST), "last_played", game->startup);
        saveConfig(CONFIG_LAST, 0);
    }

    if (configGetStrCopy(configSet, CONFIG_ITEM_ALTSTARTUP, filename, sizeof(filename)) == 0)
        strcpy(filename, game->startup);
    deinit(NO_EXCEPTION); // CAREFUL: deinit will call usbCleanUp, so usbGames/game will be freed

#ifdef VMC
#define USB_MCEMU size_mcemu_irx, &usb_mcemu_irx,
#else
#define USB_MCEMU
#endif

    sysLaunchLoaderElf(filename, "USB_MODE", irx_size, irx, USB_MCEMU EnablePS2Logo, compatmask);
}

static config_set_t *usbGetConfig(int id)
{
    return sbPopulateConfig(&usbGames[id], usbPrefix, "/");
}

static int usbGetImage(char *folder, int isRelative, char *value, char *suffix, GSTEXTURE *resultTex, short psm)
{
    char path[256];
    if (isRelative)
        sprintf(path, "%s%s/%s_%s", usbPrefix, folder, value, suffix);
    else
        sprintf(path, "%s%s_%s", folder, value, suffix);
    return texDiscoverLoad(resultTex, path, -1, psm);
}

static void usbCleanUp(int exception)
{
    if (usbGameList.enabled) {
        LOG("USBSUPPORT CleanUp\n");

        free(usbGames);
    }
}

#ifdef VMC
static int usbCheckVMC(char *name, int createSize)
{
    return sysCheckVMC(usbPrefix, "/", name, createSize, NULL);
}
#endif

static item_list_t usbGameList = {
    USB_MODE, 0, 0, MENU_MIN_INACTIVE_FRAMES, USB_MODE_UPDATE_DELAY, "USB Games", _STR_USB_GAMES, &usbInit, &usbNeedsUpdate,
#ifdef __CHILDPROOF
    &usbUpdateGameList, &usbGetGameCount, &usbGetGame, &usbGetGameName, &usbGetGameNameLength, &usbGetGameStartup, NULL, NULL,
#else
    &usbUpdateGameList, &usbGetGameCount, &usbGetGame, &usbGetGameName, &usbGetGameNameLength, &usbGetGameStartup, &usbDeleteGame, &usbRenameGame,
#endif
#ifdef VMC
    &usbLaunchGame, &usbGetConfig, &usbGetImage, &usbCleanUp, &usbCheckVMC, USB_ICON
#else
    &usbLaunchGame, &usbGetConfig, &usbGetImage, &usbCleanUp, USB_ICON
#endif
};
