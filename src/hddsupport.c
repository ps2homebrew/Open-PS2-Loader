#include "include/opl.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/supportbase.h"
#include "include/util.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/ioman.h"
#include "include/system.h"
#include "include/imports.h"
#include "include/cheatman.h"
#include "modules/iopcore/common/cdvd_config.h"
#include "include/sound.h"

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h> // fileXioFormat, fileXioMount, fileXioUmount, fileXioDevctl
#include <io_common.h>   // FIO_MT_RDWR

#include <hdd-ioctl.h>
#include <libcdvd-common.h>
#include "opl-hdd-ioctl.h"

#define OPL_HDD_MODE_PS2LOGO_OFFSET 0x17F8

#include "../modules/isofs/zso.h"

extern int probed_fd;
extern u32 probed_lba;
extern u8 IOBuffer[2048];

static unsigned char hddForceUpdate = 0;
static unsigned char hddHDProKitDetected = 0;
static unsigned char hddModulesLoadCount = 0;
static unsigned char hddSupportModulesLoaded = 0;

static char *hddPrefix = "pfs0:";
static hdl_games_list_t hddGames;

// forward declaration
static item_list_t hddGameList;

static int hddLoadGameListCache(hdl_games_list_t *cache);
static int hddUpdateGameListCache(hdl_games_list_t *cache, hdl_games_list_t *game_list);

static void hddInitModules(void)
{
    hddLoadModules();
    hddLoadSupportModules();

    // update Themes
    char path[256];
    sprintf(path, "%sTHM", gHDDPrefix);
    thmAddElements(path, "/", 1);

    sprintf(path, "%sLNG", gHDDPrefix);
    lngAddLanguages(path, "/", hddGameList.mode);

    sbCreateFolders(gHDDPrefix, 0);
}

// HD Pro Kit is mapping the 1st word in ROM0 seg as a main ATA controller,
// The pseudo ATA controller registers are accessed (input/ouput) by writing
// an id to the main ATA controller
#define HDPROreg_IO8   (*(volatile unsigned char *)0xBFC00000)
#define CDVDreg_STATUS (*(volatile unsigned char *)0xBF40200A)

static int hddCheckHDProKit(void)
{
    int ret = 0;

    DIntr();
    ee_kmode_enter();
    // HD Pro IO start commands sequence
    HDPROreg_IO8 = 0x72;
    CDVDreg_STATUS = 0;
    HDPROreg_IO8 = 0x34;
    CDVDreg_STATUS = 0;
    HDPROreg_IO8 = 0x61;
    CDVDreg_STATUS = 0;
    u32 res = HDPROreg_IO8;
    CDVDreg_STATUS = 0;

    // check result
    if ((res & 0xff) == 0xe7) {
        // HD Pro IO finish commands sequence
        HDPROreg_IO8 = 0xf3;
        CDVDreg_STATUS = 0;
        ret = 1;
    }
    ee_kmode_exit();
    EIntr();

    if (ret)
        LOG("HDDSUPPORT HD Pro Kit detected!\n");

    return ret;
}

// Taken from libhdd:
#define PFS_ZONE_SIZE 8192
#define PFS_FRAGMENT  0x00000000

static void hddCheckOPLFolder(const char *mountPoint)
{
    DIR *dir;
    char path[32];

    sprintf(path, "%sOPL", mountPoint);

    dir = opendir(path);
    if (dir == NULL)
        mkdir(path, 0777);
    else
        closedir(dir);
}

static void hddFindOPLPartition(void)
{
    static config_set_t *config;
    char name[64];
    int fd, ret = 0;

    fileXioUmount(hddPrefix);

    ret = fileXioMount("pfs0:", "hdd0:__common", FIO_MT_RDWR);
    if (ret == 0) {
        fd = open("pfs0:OPL/conf_hdd.cfg", O_RDONLY);
        if (fd >= 0) {
            config = configAlloc(0, NULL, "pfs0:OPL/conf_hdd.cfg");
            configRead(config);

            configGetStrCopy(config, "hdd_partition", name, sizeof(name));
            snprintf(gOPLPart, sizeof(gOPLPart), "hdd0:%s", name);

            configFree(config);
            close(fd);

            return;
        }

        hddCheckOPLFolder(hddPrefix);

        fd = open("pfs0:OPL/conf_hdd.cfg", O_CREAT | O_TRUNC | O_WRONLY);
        if (fd >= 0) {
            config = configAlloc(0, NULL, "pfs0:OPL/conf_hdd.cfg");
            configRead(config);

            configSetStr(config, "hdd_partition", "+OPL");
            configWrite(config);

            configFree(config);
            close(fd);
        }
    }

    snprintf(gOPLPart, sizeof(gOPLPart), "hdd0:+OPL");

    return;
}

static int hddCreateOPLPartition(const char *name)
{
    int formatArg[3] = {PFS_ZONE_SIZE, 0x2d66, PFS_FRAGMENT};
    int fd, result;
    char cmd[140];

    sprintf(cmd, "%s,,,128M,PFS", name);
    if ((fd = open(cmd, O_CREAT | O_TRUNC | O_WRONLY)) >= 0) {
        close(fd);
        result = fileXioFormat(hddPrefix, name, (const char *)&formatArg, sizeof(formatArg));
    } else {
        result = fd;
    }

    return result;
}

void hddLoadModules(void)
{
    int ret;

    LOG("HDDSUPPORT LoadModules %d\n", hddModulesLoadCount);

    if (hddModulesLoadCount == 0) {
        // Increment the load count as soon as possible to prevent thread scheduling from allowing another thread to
        // call into here and try to double load modules.
        hddModulesLoadCount = 1;

        // DEV9 must be loaded, as HDD.IRX depends on it. Even if not required by the I/F (i.e. HDPro)
        sysInitDev9();

        // try to detect HD Pro Kit (not the connected HDD),
        // if detected it loads the specific ATAD module
        hddHDProKitDetected = hddCheckHDProKit();
        if (hddHDProKitDetected) {
            LOG("[ATAD_HDPRO]:\n");
            ret = sysLoadModuleBuffer(&hdpro_atad_irx, size_hdpro_atad_irx, 0, NULL);
            LOG("[XHDD]:\n");
            sysLoadModuleBuffer(&xhdd_irx, size_xhdd_irx, 6, "-hdpro");
        } else {
            LOG("[ATAD]:\n");
            ret = sysLoadModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL);
            LOG("[XHDD]:\n");
            sysLoadModuleBuffer(&xhdd_irx, size_xhdd_irx, 0, NULL);
        }

        if (ret < 0) {
            LOG("HDD: No HardDisk Drive detected.\n");
            setErrorMessageWithCode(_STR_HDD_NOT_CONNECTED_ERROR, ERROR_HDD_IF_NOT_DETECTED);
            return;
        }
    } else
        hddModulesLoadCount++;

    LOG("HDDSUPPORT LoadModules done\n");
}

// Returns 1 for MBR/GPT, 0 for APA, and -1 if an error occured
int hddDetectNonSonyFileSystem()
{
    int result = -1;
    // Allocate memory for storing data for the first two sectors.
    u8 *pSectorData = (u8 *)malloc(512 * 2);
    if (pSectorData == NULL) {
        LOG("hddDetectNonSonyFileSystem: failed to allocate scratch memory\n");
        return -1;
    }

    // Trying to load the APA/PFS irx modules when a non-sony formatted HDD is connected (ie: MBR/GPT  w/ exFAT) runs
    // the risk of corrupting the HDD. To avoid that get the first two sectors and perform some sanity checks. If
    // we reasonably suspect the disk is not APA formatted bail out from loading the sony fs irx modules.
    result = fileXioDevctl("xhdd0:", ATA_DEVCTL_READ_PARTITION_SECTOR, NULL, 0, pSectorData, 512 * 2);
    if (result < 0) {
        LOG("hddDetectNonSonyFileSystem: failed to read data from hdd %d\n", result);
        free(pSectorData);
        return -1;
    }

    // Check for MBR signature.
    if (pSectorData[0x1FE] == 0x55 && pSectorData[0x1FF] == 0xAA) {
        // Found MBR partition type.
        LOG("hddDetectNonSonyFileSystem: found MBR partition data\n");
        result = 1;
    } else if (strncmp((const char *)&pSectorData[0x200], "EFI PART", 8) == 0) {
        // Found GPT partition type.
        LOG("hddDetectNonSonyFileSystem: found GPT partition data\n");
        result = 1;
    } else if (strncmp((const char *)&pSectorData[4], "APA", 3) == 0) {
        // Found APA partition type.
        LOG("hddDetectNonSonyFileSystem: found APA partition data\n");
        result = 0;
    } else {
        // Even though we didn't find evidence of non-APA partition data, if we load the APA irx module
        // it will write to the drive and potentially corrupt any data that might be there.
        LOG("hddDetectNonSonyFileSystem: partition data not recognized\n");
        result = -1;
    }

    // Cleanup and return.
    free(pSectorData);
    return result;
}

void hddLoadSupportModules(void)
{
    static char hddarg[] = "-o"
                           "\0"
                           "4"
                           "\0"
                           "-n"
                           "\0"
                           "20";
    static char pfsarg[] = "\0"
                           "-o" // max open
                           "\0"
                           "10" // Default value: 2
                           "\0"
                           "-n" // Number of buffers
                           "\0"
                           "40"; // Default value: 8 | Max value: 127

    LOG("HDDSUPPORT LoadSupportModules\n");

    // Check if the drive contains MBR/GPT partition data before we load the APA/PFS modules. If the drive is not
    // APA then loading the APA irx modules can corrupt the drive as it will try to write APA partition data.
    if (hddDetectNonSonyFileSystem() != 0) {
        // Drive is MBR/GPT style, or unknown, bail out or risk corrupting the drive.
        LOG("HDDSUPPORT LoadSupportModules bailing out early...\n");
        return;
    }

    if (!hddSupportModulesLoaded) {
        LOG("[HDD]:\n");
        int ret = sysLoadModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg);
        if (ret < 0) {
            LOG("HDD: No HardDisk Drive detected.\n");
            setErrorMessageWithCode(_STR_HDD_NOT_CONNECTED_ERROR, ERROR_HDD_MODULE_HDD_FAILURE);
            return;
        }

        // Check if a HDD unit is connected
        if (hddCheck() < 0) {
            LOG("HDD: No HardDisk Drive detected.\n");
            setErrorMessageWithCode(_STR_HDD_NOT_CONNECTED_ERROR, ERROR_HDD_NOT_DETECTED);
            return;
        }

        LOG("[PS2FS]:\n");
        ret = sysLoadModuleBuffer(&ps2fs_irx, size_ps2fs_irx, sizeof(pfsarg), pfsarg);
        if (ret < 0) {
            LOG("HDD: HardDisk Drive not formatted (PFS).\n");
            setErrorMessageWithCode(_STR_HDD_NOT_FORMATTED_ERROR, ERROR_HDD_MODULE_PFS_FAILURE);
            return;
        }

        hddSupportModulesLoaded = 1;
        LOG("HDDSUPPORT modules loaded\n");

        if (gOPLPart[0] == '\0')
            hddFindOPLPartition();

        fileXioUmount(hddPrefix);

        ret = fileXioMount(hddPrefix, gOPLPart, FIO_MT_RDWR);
        if (ret == -ENOENT) {
            // Attempt to create the partition.
            if ((hddCreateOPLPartition(gOPLPart)) >= 0)
                fileXioMount(hddPrefix, gOPLPart, FIO_MT_RDWR);
        }

        if (gOPLPart[5] != '+') {
            hddCheckOPLFolder(hddPrefix);
            gHDDPrefix = "pfs0:OPL/";
        }
    }
}

void hddInit(item_list_t *itemList)
{
    LOG("HDDSUPPORT Init\n");
    hddForceUpdate = 0; // Use cache at initial startup.
    configGetInt(configGetByType(CONFIG_OPL), "hdd_frames_delay", &hddGameList.delay);
    ioPutRequest(IO_CUSTOM_SIMPLEACTION, &hddInitModules);
    hddGameList.enabled = 1;
}

item_list_t *hddGetObject(int initOnly)
{
    if (initOnly && !hddGameList.enabled)
        return NULL;
    return &hddGameList;
}

static int hddNeedsUpdate(item_list_t *itemList)
{ /* Auto refresh is disabled by setting HDD_MODE_UPDATE_DELAY to MENU_UPD_DELAY_NOUPDATE, within hddsupport.h.
       Hence any update request would be issued by the user, which should be taken as an explicit request to re-scan the HDD. */
    return 1;
}

static int hddUpdateGameList(item_list_t *itemList)
{
    hdl_games_list_t hddGamesNew;
    int ret;

    if (((ret = hddLoadGameListCache(&hddGames)) != 0) || (hddForceUpdate)) {
        hddGamesNew.count = 0;
        hddGamesNew.games = NULL;
        ret = hddGetHDLGamelist(&hddGamesNew);
        if (ret == 0) {
            hddUpdateGameListCache(&hddGames, &hddGamesNew);
            hddFreeHDLGamelist(&hddGames);
            hddGames = hddGamesNew;
        }
    }

    hddForceUpdate = 1; // Subsequent refresh operations will cause the HDD to be scanned.

    return (ret == 0 ? hddGames.count : 0);
}

static int hddGetGameCount(item_list_t *itemList)
{
    return hddGames.count;
}

static void *hddGetGame(item_list_t *itemList, int id)
{
    return (void *)&hddGames.games[id];
}

static char *hddGetGameName(item_list_t *itemList, int id)
{
    return hddGames.games[id].name;
}

static int hddGetGameNameLength(item_list_t *itemList, int id)
{
    return HDL_GAME_NAME_MAX + 1;
}

static char *hddGetGameStartup(item_list_t *itemList, int id)
{
    return hddGames.games[id].startup;
}

static void hddDeleteGame(item_list_t *itemList, int id)
{
    hddDeleteHDLGame(&hddGames.games[id]);
    hddForceUpdate = 1;
}

static void hddRenameGame(item_list_t *itemList, int id, char *newName)
{
    hdl_game_info_t *game = &hddGames.games[id];
    strcpy(game->name, newName);
    hddSetHDLGameInfo(&hddGames.games[id]);
    hddForceUpdate = 1;
}

void hddLaunchGame(item_list_t *itemList, int id, config_set_t *configSet)
{
    int i, size_irx = 0;
    int EnablePS2Logo = 0;
    int result;
    void *irx = NULL;
    char filename[32];
    hdl_game_info_t *game;
    struct cdvdman_settings_hdd *settings;

    bgmEnd();

    if (gAutoLaunchGame == NULL)
        game = &hddGames.games[id];
    else
        game = gAutoLaunchGame;

    apa_sub_t parts[APA_MAXSUB + 1];
    char vmc_name[2][32];
    int part_valid = 0, size_mcemu_irx = 0, nparts;
    hdd_vmc_infos_t hdd_vmc_infos;
    memset(&hdd_vmc_infos, 0, sizeof(hdd_vmc_infos_t));

    configGetVMC(configSet, vmc_name[0], sizeof(vmc_name[0]), 0);
    configGetVMC(configSet, vmc_name[1], sizeof(vmc_name[1]), 1);

    if (vmc_name[0][0] || vmc_name[1][0]) {
        nparts = hddGetPartitionInfo(gOPLPart, parts);
        if (nparts > 0 && nparts <= 5) {
            for (i = 0; i < nparts; i++) {
                hdd_vmc_infos.parts[i].start = parts[i].start;
                hdd_vmc_infos.parts[i].length = parts[i].length;
                LOG("HDDSUPPORT hdd_vmc_infos.parts[%d].start : 0x%X\n", i, hdd_vmc_infos.parts[i].start);
                LOG("HDDSUPPORT hdd_vmc_infos.parts[%d].length : 0x%X\n", i, hdd_vmc_infos.parts[i].length);
            }
            part_valid = 1;
        }
    }

    if (part_valid) {
        char vmc_path[256];
        int vmc_id, have_error = 0;
        vmc_superblock_t vmc_superblock;
        pfs_blockinfo_t blocks[11];

        for (vmc_id = 0; vmc_id < 2; vmc_id++) {
            if (vmc_name[vmc_id][0]) {
                have_error = 1;
                hdd_vmc_infos.active = 0;
                if (sysCheckVMC(gHDDPrefix, "/", vmc_name[vmc_id], 0, &vmc_superblock) > 0) {
                    hdd_vmc_infos.flags = vmc_superblock.mc_flag & 0xFF;
                    hdd_vmc_infos.flags |= 0x100;
                    hdd_vmc_infos.specs.page_size = vmc_superblock.page_size;
                    hdd_vmc_infos.specs.block_size = vmc_superblock.pages_per_block;
                    hdd_vmc_infos.specs.card_size = vmc_superblock.pages_per_cluster * vmc_superblock.clusters_per_card;

                    // Check vmc inode block chain (write operation can cause damage)
                    snprintf(vmc_path, sizeof(vmc_path), "%sVMC/%s.bin", gHDDPrefix, vmc_name[vmc_id]);
                    if ((nparts = hddGetFileBlockInfo(vmc_path, parts, blocks, 11)) > 0) {
                        have_error = 0;
                        hdd_vmc_infos.active = 1;
                        for (i = 0; i < nparts - 1; i++) {
                            hdd_vmc_infos.blocks[i].number = blocks[i + 1].number;
                            hdd_vmc_infos.blocks[i].subpart = blocks[i + 1].subpart;
                            hdd_vmc_infos.blocks[i].count = blocks[i + 1].count;
                            LOG("HDDSUPPORT hdd_vmc_infos.blocks[%d].number     : 0x%X\n", i, hdd_vmc_infos.blocks[i].number);
                            LOG("HDDSUPPORT hdd_vmc_infos.blocks[%d].subpart    : 0x%X\n", i, hdd_vmc_infos.blocks[i].subpart);
                            LOG("HDDSUPPORT hdd_vmc_infos.blocks[%d].count      : 0x%X\n", i, hdd_vmc_infos.blocks[i].count);
                        }
                    } else { // else VMC file is too fragmented
                        LOG("HDDSUPPORT Block Chain NG\n");
                        have_error = 2;
                    }
                }

                if (have_error) {
                    if (gAutoLaunchGame == NULL) {
                        char error[256];
                        if (have_error == 2) // VMC file is fragmented
                            snprintf(error, sizeof(error), _l(_STR_ERR_VMC_FRAGMENTED_CONTINUE), vmc_name[vmc_id], (vmc_id + 1));
                        else
                            snprintf(error, sizeof(error), _l(_STR_ERR_VMC_CONTINUE), vmc_name[vmc_id], (vmc_id + 1));
                        if (!guiMsgBox(error, 1, NULL))
                            return;
                    } else
                        LOG("VMC error\n");
                }

                for (i = 0; i < size_hdd_mcemu_irx; i++) {
                    if (((u32 *)&hdd_mcemu_irx)[i] == (0xC0DEFAC0 + vmc_id)) {
                        if (hdd_vmc_infos.active)
                            size_mcemu_irx = size_hdd_mcemu_irx;
                        memcpy(&((u32 *)&hdd_mcemu_irx)[i], &hdd_vmc_infos, sizeof(hdd_vmc_infos_t));
                        break;
                    }
                }
            }
        }
    }

    if (gRememberLastPlayed) {
        configSetStr(configGetByType(CONFIG_LAST), "last_played", game->startup);
        saveConfig(CONFIG_LAST, 0);
    }

    char gid[5];
    configGetDiscIDBinary(configSet, gid);

    int dmaType = 0, dmaMode = 7, compatMode = 0;
    configGetInt(configSet, CONFIG_ITEM_COMPAT, &compatMode);
    configGetInt(configSet, CONFIG_ITEM_DMA, &dmaMode);
    if (dmaMode < 3)
        dmaType = 0x20;
    else {
        dmaType = 0x40;
        dmaMode -= 3;
    }
    hddSetTransferMode(dmaType, dmaMode);
    // gHDDSpindown [0..20] -> spindown [0..240] -> seconds [0..1200]
    hddSetIdleTimeout(gHDDSpindown * 12);

    if (hddHDProKitDetected) {
        size_irx = size_hdd_hdpro_cdvdman_irx;
        irx = &hdd_hdpro_cdvdman_irx;
    } else {
        size_irx = size_hdd_cdvdman_irx;
        irx = &hdd_cdvdman_irx;
    }

    sbPrepare(NULL, configSet, size_irx, irx, &i);

    if ((result = sbLoadCheats(gHDDPrefix, game->startup)) < 0) {
        if (gAutoLaunchGame == NULL) {
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

    settings = (struct cdvdman_settings_hdd *)((u8 *)irx + i);

    // patch 48bit flag
    settings->common.media = hddIs48bit() & 0xff;

    // patch start_sector
    settings->lba_start = game->start_sector;

    if (configGetStrCopy(configSet, CONFIG_ITEM_ALTSTARTUP, filename, sizeof(filename)) == 0)
        strcpy(filename, game->startup);

    if (gPS2Logo)
        EnablePS2Logo = CheckPS2Logo(0, game->start_sector + OPL_HDD_MODE_PS2LOGO_OFFSET);

    // Check for ZSO to correctly adjust layer1 start
    settings->common.layer1_start = 0; // cdvdman will read it from APA header
    hddReadSectors(game->start_sector + OPL_HDD_MODE_PS2LOGO_OFFSET, 1, IOBuffer);
    if (*(u32 *)IOBuffer == ZSO_MAGIC) {
        probed_fd = 0;
        probed_lba = game->start_sector + OPL_HDD_MODE_PS2LOGO_OFFSET;
        ziso_init((ZISO_header *)IOBuffer, *(u32 *)((u8 *)IOBuffer + sizeof(ZISO_header)));
        ziso_read_sector(IOBuffer, 16, 1);
        u32 maxLBA = *(u32 *)(IOBuffer + 80);
        if (maxLBA > 0 && maxLBA < ziso_total_block) {   // dual layer check
            settings->common.layer1_start = maxLBA - 16; // adjust second layer start
        }
    }

    if (gAutoLaunchGame == NULL)
        deinit(NO_EXCEPTION, HDD_MODE); // CAREFUL: deinit will call hddCleanUp, so hddGames/game will be freed
    else {
        miniDeinit(configSet);

        free(gAutoLaunchGame);
        gAutoLaunchGame = NULL;

        fileXioUmount("pfs0:");
        fileXioDevctl("pfs:", PDIOC_CLOSEALL, NULL, 0, NULL, 0);
    }

    settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_DEV9;
    settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_ATAD;

    // adjust ZSO cache
    settings->common.zso_cache = hddCacheSize;

    sysLaunchLoaderElf(filename, "HDD_MODE", size_irx, irx, size_mcemu_irx, hdd_mcemu_irx, EnablePS2Logo, compatMode);
}

static config_set_t *hddGetConfig(item_list_t *itemList, int id)
{
    char path[256];
    hdl_game_info_t *game = &hddGames.games[id];

    snprintf(path, sizeof(path), "%sCFG/%s.cfg", gHDDPrefix, game->startup);
    config_set_t *config = configAlloc(0, NULL, path);
    configRead(config); // Does not matter if the config file exists or not.

    configSetStr(config, CONFIG_ITEM_NAME, game->name);
    configSetInt(config, CONFIG_ITEM_SIZE, game->total_size_in_kb >> 10);
    configSetStr(config, CONFIG_ITEM_FORMAT, "HDL");
    configSetStr(config, CONFIG_ITEM_MEDIA, game->disctype == SCECdPS2CD ? "CD" : "DVD");
    configSetStr(config, CONFIG_ITEM_STARTUP, game->startup);

    return config;
}

static int hddGetImage(item_list_t *itemList, char *folder, int isRelative, char *value, char *suffix, GSTEXTURE *resultTex, short psm)
{
    char path[256];
    if (isRelative)
        snprintf(path, sizeof(path), "%s%s/%s_%s", gHDDPrefix, folder, value, suffix);
    else
        snprintf(path, sizeof(path), "%s%s_%s", folder, value, suffix);
    return texDiscoverLoad(resultTex, path, -1);
}

static int hddGetTextId(item_list_t *itemList)
{
    return _STR_HDD_GAMES;
}

static int hddGetIconId(item_list_t *itemList)
{
    return HDD_ICON;
}

// This may be called, even if hddInit() was not.
static void hddCleanUp(item_list_t *itemList, int exception)
{
    LOG("HDDSUPPORT CleanUp\n");

    if (hddGameList.enabled) {
        hddFreeHDLGamelist(&hddGames);

        if ((exception & UNMOUNT_EXCEPTION) == 0)
            fileXioUmount(hddPrefix);
    }

    // UI may have loaded modules outside of HDD mode, so deinitialize regardless of the enabled status.
    if (hddSupportModulesLoaded) {
        fileXioDevctl("pfs:", PDIOC_CLOSEALL, NULL, 0, NULL, 0);

        hddSupportModulesLoaded = 0;
    }
}

static int hddCheckVMC(item_list_t *itemList, char *name, int createSize)
{
    return sysCheckVMC(gHDDPrefix, "/", name, createSize, NULL);
}

// This may be called, even if hddInit() was not.
static void hddShutdown(item_list_t *itemList)
{
    LOG("HDDSUPPORT Shutdown\n");

    if (hddGameList.enabled) {
        hddFreeHDLGamelist(&hddGames);
        fileXioUmount(hddPrefix);
    }

    // UI may have loaded modules outside of HDD mode, so deinitialize regardless of the enabled status.
    if (hddSupportModulesLoaded) {
        /* Close all files */
        fileXioDevctl("pfs:", PDIOC_CLOSEALL, NULL, 0, NULL, 0);

        hddSupportModulesLoaded = 0;
    }

    if (hddModulesLoadCount > 0) {
        hddModulesLoadCount -= 1;
        if (hddModulesLoadCount == 0) {
            // DEV9 will remain active if ETH is in use, so put the HDD in IDLE state.
            // The HDD should still enter standby state after 21 minutes & 15 seconds, as per the ATAD defaults.
            hddSetIdleImmediate();
        }

        // Only shut down dev9 from here, if it was initialized from here before.
        sysShutdownDev9();
    }
}

static int hddLoadGameListCache(hdl_games_list_t *cache)
{
    char filename[256];
    FILE *file;
    hdl_game_info_t *games;
    int result, size, count;

    if (!gHDDGameListCache)
        return 1;

    hddFreeHDLGamelist(cache);

    sprintf(filename, "%sgames.bin", gHDDPrefix);
    file = fopen(filename, "rb");
    if (file != NULL) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        rewind(file);

        count = size / sizeof(hdl_game_info_t);
        if (count > 0) {
            games = memalign(64, count * sizeof(hdl_game_info_t));
            if (games != NULL) {
                if (fread(games, sizeof(hdl_game_info_t), count, file) == count) {
                    cache->count = count;
                    cache->games = games;
                    LOG("hddLoadGameListCache: %d games loaded.\n", count);
                    result = 0;
                } else {
                    LOG("hddLoadGameListCache: I/O error.\n");
                    free(games);
                    result = EIO;
                }
            } else {
                LOG("hddLoadGameListCache: failed to allocate memory.\n");
                result = ENOMEM;
            }
        } else {
            result = -1; // Empty file
        }

        fclose(file);
    } else {
        result = ENOENT;
    }

    return result;
}

static int hddUpdateGameListCache(hdl_games_list_t *cache, hdl_games_list_t *game_list)
{
    char filename[256];
    FILE *file;
    int result, i, j, modified;

    if (!gHDDGameListCache)
        return 1;

    if (cache->count > 0) {
        modified = 0;
        for (i = 0; i < cache->count; i++) {
            for (j = 0; j < game_list->count; j++) {
                if (strncmp(cache->games[i].partition_name, game_list->games[j].partition_name, APA_IDMAX + 1) == 0)
                    break;
            }

            if (j == game_list->count) {
                LOG("hddUpdateGameListCache: game added.\n");
                modified = 1;
                break;
            }
        }

        if ((!modified) && (game_list->count != cache->count)) {
            LOG("hddUpdateGameListCache: game removed.\n");
            modified = 1;
        }
    } else {
        modified = (game_list->count > 0) ? 1 : 0;
    }

    if (!modified)
        return 0;
    LOG("hddUpdateGameListCache: caching new game list.\n");

    sprintf(filename, "%sgames.bin", gHDDPrefix);
    if (game_list->count > 0) {
        file = fopen(filename, "wb");
        if (file != NULL) {
            result = (fwrite(game_list->games, sizeof(hdl_game_info_t), game_list->count, file) == game_list->count) ? 0 : EIO;
            fclose(file);
        } else {
            result = EIO;
        }
    } else {
        // Last game deleted.
        remove(filename);
        result = 0;
    }

    return result;
}

static char *hddGetPrefix(item_list_t *itemList)
{
    return gHDDPrefix;
}

static item_list_t hddGameList = {
    HDD_MODE, 0, 0, MODE_FLAG_COMPAT_DMA, MENU_MIN_INACTIVE_FRAMES, HDD_MODE_UPDATE_DELAY, NULL, NULL, &hddGetTextId, &hddGetPrefix, &hddInit, &hddNeedsUpdate, &hddUpdateGameList,
    &hddGetGameCount, &hddGetGame, &hddGetGameName, &hddGetGameNameLength, &hddGetGameStartup, &hddDeleteGame, &hddRenameGame,
    &hddLaunchGame, &hddGetConfig, &hddGetImage, &hddCleanUp, &hddShutdown, &hddCheckVMC, &hddGetIconId};
