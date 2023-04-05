#include "include/opl.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/supportbase.h"
#include "include/bdmsupport.h"
#include "include/util.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/ioman.h"
#include "include/system.h"
#include "include/extern_irx.h"
#include "include/cheatman.h"
#include "modules/iopcore/common/cdvd_config.h"

#include <usbhdfsd-common.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h> // fileXioIoctl, fileXioDevctl

#define U64_2XU32(val)  ((u32*)val)[1], ((u32*)val)[0]

static int iLinkModLoaded = 0;
static int mx4sioModLoaded = 0;

// Identifies the partition that the specified file is stored on and generates a full path to it.
int bdmFindPartition(char *target, const char *name, int write)
{
    int i, fd;
    char path[256];

    for (i = 0; i < MAX_BDM_DEVICES; i++) {
        if (gBDMPrefix[0] != '\0')
            sprintf(path, "mass%d:%s/%s", i, gBDMPrefix, name);
        else
            sprintf(path, "mass%d:%s", i, name);
        if (write)
            fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        else
            fd = open(path, O_RDONLY);

        if (fd >= 0) {
            if (gBDMPrefix[0] != '\0')
                sprintf(target, "mass%d:%s/", i, gBDMPrefix);
            else
                sprintf(target, "mass%d:", i);
            close(fd);
            return 1;
        }
    }

    // default to first partition (for themes, ...)
    if (gBDMPrefix[0] != '\0')
        sprintf(target, "mass0:%s/", gBDMPrefix);
    else
        sprintf(target, "mass0:");
    return 0;
}

static unsigned int BdmGeneration = 0;

static void bdmEventHandler(void *packet, void *opt)
{
    BdmGeneration++;
}

static void bdmLoadBlockDeviceModules(void)
{
    if (gEnableILK && !iLinkModLoaded) {
        // Load iLink Block Device drivers
        sysLoadModuleBuffer(&iLinkman_irx, size_iLinkman_irx, 0, NULL);
        sysLoadModuleBuffer(&IEEE1394_bd_irx, size_IEEE1394_bd_irx, 0, NULL);

        iLinkModLoaded = 1;
    }

    if (gEnableMX4SIO && !mx4sioModLoaded) {
        // Load MX4SIO Block Device drivers
        sysLoadModuleBuffer(&mx4sio_bd_irx, size_mx4sio_bd_irx, 0, NULL);

        mx4sioModLoaded = 1;
    }

    if (gEnableBdmHDD)
        hddLoadModules();
}

void bdmLoadModules(void)
{
    LOG("BDMSUPPORT LoadModules\n");

    // Load Block Device Manager (BDM)
    sysLoadModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL);

    // Load FATFS (mass:) driver
    sysLoadModuleBuffer(&bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL);

    // Load USB Block Device drivers
    sysLoadModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL);
    sysLoadModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL);

    // Load Optional Block Device drivers
    bdmLoadBlockDeviceModules();

    sysLoadModuleBuffer(&bdmevent_irx, size_bdmevent_irx, 0, NULL);
    SifAddCmdHandler(0, &bdmEventHandler, NULL);

    LOG("BDMSUPPORT Modules loaded\n");
}

void bdmInit(item_list_t* pItemList)
{
    LOG("BDMSUPPORT Init\n");

    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;
    pDeviceData->bdmULSizePrev = -2;
    pDeviceData->bdmModifiedCDPrev = 0;
    pDeviceData->bdmModifiedDVDPrev = 0;
    pDeviceData->bdmGameCount = 0;
    pDeviceData->bdmGames = NULL;
    configGetInt(configGetByType(CONFIG_OPL), "usb_frames_delay", &pItemList->delay);
    pItemList->enabled = 1;
}

static int bdmNeedsUpdate(item_list_t* pItemList)
{
    char path[256];
    static unsigned int OldGeneration = 0;
    static unsigned char ThemesLoaded = 0;
    static unsigned char LanguagesLoaded = 0;
    int result = 0;
    struct stat st;

    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    ioPutRequest(IO_CUSTOM_SIMPLEACTION, &bdmLoadBlockDeviceModules);

    if (pDeviceData->bdmULSizePrev != -2 && OldGeneration == BdmGeneration)
        return 0;
    OldGeneration = BdmGeneration;

    sprintf(path, "%sCD", pDeviceData->bdmPrefix);
    if (stat(path, &st) != 0)
        st.st_mtime = 0;
    if (pDeviceData->bdmModifiedCDPrev != st.st_mtime) {
        pDeviceData->bdmModifiedCDPrev = st.st_mtime;
        result = 1;
    }

    sprintf(path, "%sDVD", pDeviceData->bdmPrefix);
    if (stat(path, &st) != 0)
        st.st_mtime = 0;
    if (pDeviceData->bdmModifiedDVDPrev != st.st_mtime) {
        pDeviceData->bdmModifiedDVDPrev = st.st_mtime;
        result = 1;
    }

    if (!sbIsSameSize(pDeviceData->bdmPrefix, pDeviceData->bdmULSizePrev))
        result = 1;

    // update Themes
    if (!ThemesLoaded) {
        sprintf(path, "%sTHM", pDeviceData->bdmPrefix);
        if (thmAddElements(path, "/", 1) > 0)
            ThemesLoaded = 1;
    }

    // update Languages
    if (!LanguagesLoaded) {
        sprintf(path, "%sLNG", pDeviceData->bdmPrefix);
        if (lngAddLanguages(path, "/", pItemList->mode) > 0)
            LanguagesLoaded = 1;
    }

    sbCreateFolders(pDeviceData->bdmPrefix, 1);

    return result;
}

static int bdmUpdateGameList(item_list_t* pItemList)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    sbReadList(&pDeviceData->bdmGames, pDeviceData->bdmPrefix, &pDeviceData->bdmULSizePrev, &pDeviceData->bdmGameCount);
    return pDeviceData->bdmGameCount;
}

static int bdmGetGameCount(item_list_t* pItemList)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    return pDeviceData->bdmGameCount;
}

static void *bdmGetGame(item_list_t* pItemList, int id)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    return (void *)&pDeviceData->bdmGames[id];
}

static char *bdmGetGameName(item_list_t* pItemList, int id)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    return pDeviceData->bdmGames[id].name;
}

static int bdmGetGameNameLength(item_list_t* pItemList, int id)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    return ((pDeviceData->bdmGames[id].format != GAME_FORMAT_USBLD) ? ISO_GAME_NAME_MAX + 1 : UL_GAME_NAME_MAX + 1);
}

static char *bdmGetGameStartup(item_list_t* pItemList, int id)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    return pDeviceData->bdmGames[id].startup;
}

static void bdmDeleteGame(item_list_t* pItemList, int id)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    sbDelete(&pDeviceData->bdmGames, pDeviceData->bdmPrefix, "/", pDeviceData->bdmGameCount, id);
    pDeviceData->bdmULSizePrev = -2;
}

static void bdmRenameGame(item_list_t* pItemList, int id, char *newName)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    sbRename(&pDeviceData->bdmGames, pDeviceData->bdmPrefix, "/", pDeviceData->bdmGameCount, id, newName);
    pDeviceData->bdmULSizePrev = -2;
}

void bdmLaunchGame(item_list_t* pItemList, int id, config_set_t *configSet)
{
    int i, fd, index, compatmask = 0;
    int EnablePS2Logo = 0;
    int result;
    u64 startingLBA;
    unsigned int startCluster;
    char partname[256], filename[32];
    base_game_info_t *game;
    struct cdvdman_settings_bdm *settings;
    u32 layer1_start, layer1_offset;
    unsigned short int layer1_part;

    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    if (gAutoLaunchBDMGame == NULL)
        game = &pDeviceData->bdmGames[id];
    else
        game = gAutoLaunchBDMGame;

    char vmc_name[32], vmc_path[256], have_error = 0;
    int vmc_id, size_mcemu_irx = 0;
    bdm_vmc_infos_t bdm_vmc_infos;
    vmc_superblock_t vmc_superblock;

    for (vmc_id = 0; vmc_id < 2; vmc_id++) {
        memset(&bdm_vmc_infos, 0, sizeof(bdm_vmc_infos_t));
        configGetVMC(configSet, vmc_name, sizeof(vmc_name), vmc_id);
        if (vmc_name[0]) {
            have_error = 1;
            if (sysCheckVMC(pDeviceData->bdmPrefix, "/", vmc_name, 0, &vmc_superblock) > 0) {
                bdm_vmc_infos.flags = vmc_superblock.mc_flag & 0xFF;
                bdm_vmc_infos.flags |= 0x100;
                bdm_vmc_infos.specs.page_size = vmc_superblock.page_size;
                bdm_vmc_infos.specs.block_size = vmc_superblock.pages_per_block;
                bdm_vmc_infos.specs.card_size = vmc_superblock.pages_per_cluster * vmc_superblock.clusters_per_card;

                sprintf(vmc_path, "%sVMC/%s.bin", pDeviceData->bdmPrefix, vmc_name);

                fd = open(vmc_path, O_RDONLY);
                if (fd >= 0)
                {
                    // Get the absolute LBA of the VMC file and starting cluster number.
                    if (fileXioIoctl2(fd, USBMASS_IOCTL_GET_LBA, NULL, 0, &startingLBA, sizeof(startingLBA)) == 0 && 
                        (startCluster = fileXioIoctl2(fd, USBMASS_IOCTL_GET_CLUSTER, NULL, 0, NULL, 0)) != 0)
                    {
                        // Check VMC cluster chain for fragmentation (write operation can cause damage to the filesystem).
                        if (fileXioIoctl(fd, USBMASS_IOCTL_CHECK_CHAIN, "") == 1) {
                            LOG("BDMSUPPORT Cluster Chain OK\n");
                            have_error = 0;
                            bdm_vmc_infos.active = 1;
                            bdm_vmc_infos.start_sector = startingLBA;
                            LOG("BDMSUPPORT VMC slot %d start: 0x%08x%08x\n", vmc_id, U64_2XU32(&startingLBA));
                        } else {
                            LOG("BDMSUPPORT Cluster Chain NG\n");
                            have_error = 2;
                        }
                    }

                    close(fd);
                }
            }
        }

        if (gAutoLaunchBDMGame == NULL) {
            if (have_error) {
                char error[256];
                if (have_error == 2) // VMC file is fragmented
                    snprintf(error, sizeof(error), _l(_STR_ERR_VMC_FRAGMENTED_CONTINUE), vmc_name, (vmc_id + 1));
                else
                    snprintf(error, sizeof(error), _l(_STR_ERR_VMC_CONTINUE), vmc_name, (vmc_id + 1));
                if (!guiMsgBox(error, 1, NULL)) {
                    return;
                }
            }
        } else
            LOG("VMC error\n");

        for (i = 0; i < size_bdm_mcemu_irx; i++) {
            if (((u32 *)&bdm_mcemu_irx)[i] == (0xC0DEFAC0 + vmc_id)) {
                if (bdm_vmc_infos.active)
                    size_mcemu_irx = size_bdm_mcemu_irx;
                memcpy(&((u32 *)&bdm_mcemu_irx)[i], &bdm_vmc_infos, sizeof(bdm_vmc_infos_t));
                break;
            }
        }
    }

    void *irx = NULL;
    int irx_size = 0;

    if (!strcmp(pDeviceData->bdmDriver, "ata") && strlen(pDeviceData->bdmDriver) == 3)
    {
        irx = &bdm_ata_cdvdman_irx;
        irx_size = size_bdm_ata_cdvdman_irx;
    }
    else
    {
        irx = &bdm_cdvdman_irx;
        irx_size = size_bdm_cdvdman_irx;
    }

    compatmask = sbPrepare(game, configSet, irx_size, irx, &index);
    settings = (struct cdvdman_settings_bdm *)((u8 *)irx + index);
    if (settings == NULL)
        return;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
    memset(&settings->frags[0], 0, sizeof(bd_fragment_t) * BDM_MAX_FRAGS);
#pragma GCC diagnostic pop
    u8 iTotalFragCount = 0;

    //
    // Add ISO as fragfile[0] to fragment list
    //
    struct cdvdman_fragfile *iso_frag = &settings->fragfile[0];
    iso_frag->frag_start = 0;
    iso_frag->frag_count = 0;
    for (i = 0; i < game->parts; i++) {
        // Open file
        sbCreatePath(game, partname, pDeviceData->bdmPrefix, "/", i);
        fd = open(partname, O_RDONLY);
        if (fd < 0) {
            sbUnprepare(&settings->common);
            guiMsgBox(_l(_STR_ERR_FILE_INVALID), 0, NULL);
            return;
        }

        // Get fragment list
        int iFragCount = fileXioIoctl2(fd, USBMASS_IOCTL_GET_FRAGLIST, NULL, 0, (void *)&settings->frags[iTotalFragCount], sizeof(bd_fragment_t) * (BDM_MAX_FRAGS - iTotalFragCount));
        if (iFragCount > BDM_MAX_FRAGS) {
            // Too many fragments
            close(fd);
            sbUnprepare(&settings->common);
            guiMsgBox(_l(_STR_ERR_FRAGMENTED), 0, NULL);
            return;
        }
        iso_frag->frag_count += iFragCount;
        iTotalFragCount += iFragCount;

        if ((gPS2Logo) && (i == 0))
            EnablePS2Logo = CheckPS2Logo(fd, 0);

        close(fd);
    }

    // Initialize layer 1 information.
    sbCreatePath(game, partname, pDeviceData->bdmPrefix, "/", 0);
    layer1_start = sbGetISO9660MaxLBA(partname);

    switch (game->format) {
        case GAME_FORMAT_USBLD:
            layer1_part = layer1_start / 0x80000;
            layer1_offset = layer1_start % 0x80000;
            sbCreatePath(game, partname, pDeviceData->bdmPrefix, "/", layer1_part);
            break;
        default: // Raw ISO9660 disc image; one part.
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

    // adjust ZSO cache
    settings->common.zso_cache = bdmCacheSize;

    if ((result = sbLoadCheats(pDeviceData->bdmPrefix, game->startup)) < 0) {
        if (gAutoLaunchBDMGame == NULL) {
            switch (result) {
                case -ENOENT:
                    guiWarning(_l(_STR_NO_CHEATS_FOUND), 10);
                    break;
                default:
                    guiWarning(_l(_STR_ERR_CHEATS_LOAD_FAILED), 10);
            }
        } else
            LOG("Cheats error\n");
    }

    if (gRememberLastPlayed) {
        configSetStr(configGetByType(CONFIG_LAST), "last_played", game->startup);
        saveConfig(CONFIG_LAST, 0);
    }

    if (configGetStrCopy(configSet, CONFIG_ITEM_ALTSTARTUP, filename, sizeof(filename)) == 0)
        strcpy(filename, game->startup);

    if (gAutoLaunchBDMGame == NULL)
        deinit(NO_EXCEPTION, pItemList->mode); // CAREFUL: deinit will call bdmCleanUp, so bdmGames/game will be freed
    else {
        miniDeinit(configSet);

        free(gAutoLaunchBDMGame);
        gAutoLaunchBDMGame = NULL;
    }

    settings->bdDeviceId = pDeviceData->massDeviceIndex;
    if (!strcmp(pDeviceData->bdmDriver, "usb")) {
        settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_USBD;
        sysLaunchLoaderElf(filename, "BDM_USB_MODE", irx_size, irx, size_mcemu_irx, bdm_mcemu_irx, EnablePS2Logo, compatmask);
    } else if (!strcmp(pDeviceData->bdmDriver, "sd") && strlen(pDeviceData->bdmDriver) == 2) {
        settings->common.fakemodule_flags |= 0 /* TODO! fake ilinkman ? */;
        sysLaunchLoaderElf(filename, "BDM_ILK_MODE", irx_size, irx, size_mcemu_irx, bdm_mcemu_irx, EnablePS2Logo, compatmask);
    } else if (!strcmp(pDeviceData->bdmDriver, "sdc") && strlen(pDeviceData->bdmDriver) == 3) {
        settings->common.fakemodule_flags |= 0;
        sysLaunchLoaderElf(filename, "BDM_M4S_MODE", irx_size, irx, size_mcemu_irx, bdm_mcemu_irx, EnablePS2Logo, compatmask);
    }
    else if (!strcmp(pDeviceData->bdmDriver, "ata") && strlen(pDeviceData->bdmDriver) == 3)
    {
        settings->common.fakemodule_flags |= 0;
        settings->hddIsLBA48 = pDeviceData->bdmHddIsLBA48;
        sysLaunchLoaderElf(filename, "BDM_MASS_ATA_MODE", irx_size, irx, size_mcemu_irx, bdm_mcemu_irx, EnablePS2Logo, compatmask);
    }
}

static config_set_t *bdmGetConfig(item_list_t* pItemList, int id)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;
    return sbPopulateConfig(&pDeviceData->bdmGames[id], pDeviceData->bdmPrefix, "/");
}

static int bdmGetImage(item_list_t* pItemList, char *folder, int isRelative, char *value, char *suffix, GSTEXTURE *resultTex, short psm)
{
    char path[256];

    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    if (isRelative)
        snprintf(path, sizeof(path), "%s%s/%s_%s", pDeviceData->bdmPrefix, folder, value, suffix);
    else
        snprintf(path, sizeof(path), "%s%s_%s", folder, value, suffix);
    return texDiscoverLoad(resultTex, path, -1);
}

static int bdmGetTextId(item_list_t* pItemList)
{
    int mode = _STR_BDM_GAMES;

    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    if (!strcmp(pDeviceData->bdmDriver, "usb"))
        mode = _STR_USB_GAMES;
    else if (!strcmp(pDeviceData->bdmDriver, "sd") && strlen(pDeviceData->bdmDriver) == 2)
        mode = _STR_ILINK_GAMES;
    else if (!strcmp(pDeviceData->bdmDriver, "sdc") && strlen(pDeviceData->bdmDriver) == 3)
        mode = _STR_MX4SIO_GAMES;
    else if (!strcmp(pDeviceData->bdmDriver, "ata") && strlen(pDeviceData->bdmDriver) == 3)
        mode = _STR_HDD_GAMES;

    return mode;
}

static int bdmGetIconId(item_list_t* pItemList)
{
    int mode = BDM_ICON;

    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;

    if (!strcmp(pDeviceData->bdmDriver, "usb"))
        mode = USB_ICON;
    else if (!strcmp(pDeviceData->bdmDriver, "sd") && strlen(pDeviceData->bdmDriver) == 2)
        mode = ILINK_ICON;
    else if (!strcmp(pDeviceData->bdmDriver, "sdc") && strlen(pDeviceData->bdmDriver) == 3)
        mode = MX4SIO_ICON;
    else if (!strcmp(pDeviceData->bdmDriver, "ata") && strlen(pDeviceData->bdmDriver) == 3)
        mode = HDD_ICON;

    return mode;
}

// This may be called, even if bdmInit() was not.
static void bdmCleanUp(item_list_t* pItemList, int exception)
{
    if (pItemList->enabled) {
        LOG("BDMSUPPORT CleanUp\n");

        bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;
        free(pDeviceData->bdmGames);

        // TODO: free pDeviceData and pItemList

        //      if ((exception & UNMOUNT_EXCEPTION) == 0)
        //          ...
    }
}

// This may be called, even if bdmInit() was not.
static void bdmShutdown(item_list_t* pItemList)
{
    if (pItemList->enabled) {
        LOG("BDMSUPPORT Shutdown\n");

        bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;
        free(pDeviceData->bdmGames);

        // TODO: free pDeviceData and pItemList
    }

    // As required by some (typically 2.5") HDDs, issue the SCSI STOP UNIT command to avoid causing an emergency park.
    fileXioDevctl("mass:", USBMASS_DEVCTL_STOP_ALL, NULL, 0, NULL, 0);
}

static int bdmCheckVMC(item_list_t* pItemList, char *name, int createSize)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;
    return sysCheckVMC(pDeviceData->bdmPrefix, "/", name, createSize, NULL);
}

static char *bdmGetPrefix(item_list_t* pItemList)
{
    bdm_device_data_t* pDeviceData = (bdm_device_data_t*)pItemList->priv;
    return pDeviceData->bdmPrefix;
}

static item_list_t bdmGameList = {
    BDM_MODE, 2, 0, 0, MENU_MIN_INACTIVE_FRAMES, BDM_MODE_UPDATE_DELAY, NULL, NULL, &bdmGetTextId, &bdmGetPrefix, &bdmInit, &bdmNeedsUpdate,
    &bdmUpdateGameList, &bdmGetGameCount, &bdmGetGame, &bdmGetGameName, &bdmGetGameNameLength, &bdmGetGameStartup, &bdmDeleteGame, &bdmRenameGame,
    &bdmLaunchGame, &bdmGetConfig, &bdmGetImage, &bdmCleanUp, &bdmShutdown, &bdmCheckVMC, &bdmGetIconId};

void bdmEnumerateDevices()
{
    char path[10] = { 0 };

    LOG("bdmEnumerateDevices\n");

    // Because bdmLoadModules is called before the config file is loaded bdmLoadBlockDeviceModules will not have loaded any
    // optional bdm modules. Now that the config file has been loaded try loading any optional modules that weren't previously loaded.
    bdmLoadBlockDeviceModules();

    // Enumerate bdm devices mounted as massX:
    for (int i = BDM_MODE; i <= BDM_MODE4; i++)
    {
        // Format the device path and try to open the device.
        sprintf(path, "mass%d:/", i);
        int dir = fileXioDopen(path);
        LOG("opendir %s -> %d\n", path, dir);
        if (dir >= 0)
        {
            // Allocate and initialize the device support structure.
            item_list_t* pDeviceSupport = (item_list_t*)malloc(sizeof(item_list_t));
            memcpy(pDeviceSupport, &bdmGameList, sizeof(item_list_t));
            pDeviceSupport->mode = i;

            // Setup the per-device data.
            bdm_device_data_t* pDeviceData = (bdm_device_data_t*)malloc(sizeof(bdm_device_data_t));
            memset(pDeviceData, 0, sizeof(bdm_device_data_t));
            pDeviceData->bdmULSizePrev = -2;
            pDeviceSupport->priv = pDeviceData;

            if (gBDMPrefix[0] != '\0')
                snprintf(pDeviceData->bdmPrefix, sizeof(pDeviceData->bdmPrefix), "mass%d:%s/", i, gBDMPrefix);
            else
                snprintf(pDeviceData->bdmPrefix, sizeof(pDeviceData->bdmPrefix), "mass%d:", i);

            // Get the name of the underlying device driver that backs the fat fs.
            fileXioIoctl2(dir, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, &pDeviceData->bdmDriver, sizeof(pDeviceData->bdmDriver) - 1);
            fileXioIoctl2(dir, USBMASS_IOCTL_GET_DEVICE_NUMBER, NULL, 0, &pDeviceData->massDeviceIndex, sizeof(pDeviceData->massDeviceIndex));

            // If the device is backed by the ATA driver then get the supported LBA size for the drive.
            if (strncmp(pDeviceData->bdmDriver, "ata", 3) == 0)
            {
                // If atad is loaded then xhdd is also loaded, query the hdd to see if it supports LBA48 or not.
                pDeviceData->bdmHddIsLBA48 = fileXioDevctl("xhdd0:", ATA_DEVCTL_IS_48BIT, NULL, 0, NULL, 0);
                if (pDeviceData->bdmHddIsLBA48 < 0)
                {
                    // Failed to query the LBA limit of the device, fail safe to LBA28.
                    LOG("Mass device %d is backed by ATA but failed to get LBA limit %d\n", pDeviceData->massDeviceIndex, pDeviceData->bdmHddIsLBA48);
                    pDeviceData->bdmHddIsLBA48 = 0;
                }

                LOG("Mass device: %d (%d LBA%d) %s -> %s\n", i, pDeviceData->massDeviceIndex, (pDeviceData->bdmHddIsLBA48 == 1 ? 48 : 28), pDeviceData->bdmPrefix, pDeviceData->bdmDriver);
            }
            else
                LOG("Mass device: %d (%d) %s -> %s\n", i, pDeviceData->massDeviceIndex, pDeviceData->bdmPrefix, pDeviceData->bdmDriver);

            // Register the device structure into the UI.
            initSupport(pDeviceSupport, i, 0);

            // Close the device handle.
            fileXioDclose(dir);
        }
    }
}