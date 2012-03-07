#include "include/usbld.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/supportbase.h"
#include "include/usbsupport.h"
#include "include/util.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/ioman.h"
#include "include/system.h"

extern void *usb_cdvdman_irx;
extern int size_usb_cdvdman_irx;

extern void *usb_4Ksectors_cdvdman_irx;
extern int size_usb_4Ksectors_cdvdman_irx;

extern void *usbd_ps2_irx;
extern int size_usbd_ps2_irx;

extern void *usbd_ps3_irx;
extern int size_usbd_ps3_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

#ifdef VMC
extern void *usb_mcemu_irx;
extern int size_usb_mcemu_irx;
#endif

void *usbd_irx;
int size_usbd_irx;

static char usbPrefix[40];
static int usbULSizePrev = -2;
static unsigned char usbModifiedCDPrev[8];
static unsigned char usbModifiedDVDPrev[8];
static int usbGameCount = 0;
static base_game_info_t *usbGames;

// forward declaration
static item_list_t usbGameList;

int usbFindPartition(char *target, char *name) {
	int i, fd;
	char path[255];

	for(i=0; i<5; i++) {
		if (gUSBPrefix[0] != '\0')
			sprintf(path, "mass%d:%s/%s", i, gUSBPrefix, name);
		else
			sprintf(path, "mass%d:%s", i, name);
		fd = fioOpen(path, O_RDONLY);

		if(fd >= 0) {
			if (gUSBPrefix[0] != '\0')
				sprintf(target, "mass%d:%s/", i, gUSBPrefix);
			else
				sprintf(target, "mass%d:", i);
			fioClose(fd);
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

static void usbInitModules(void) {
	usbLoadModules();

	// update Themes
	usbFindPartition(usbPrefix, "ul.cfg");
	char path[255];
	sprintf(path, "%sTHM", usbPrefix);
	thmAddElements(path, "/", usbGameList.mode);

	sprintf(path, "%sCFG", usbPrefix);
	checkCreateDir(path);

#ifdef VMC
	sprintf(path, "%sVMC", usbPrefix);
	checkCreateDir(path);
#endif
}

void usbLoadModules(void) {
	LOG("USBSUPPORT LoadModules\n");
	//first it search for custom usbd in MC?
	usbd_irx = readFile("mc?:BEDATA-SYSTEM/USBD.IRX", -1, &size_usbd_irx);
	if (!usbd_irx) {
		usbd_irx = readFile("mc?:BADATA-SYSTEM/USBD.IRX", -1, &size_usbd_irx);
		if (!usbd_irx) {
			usbd_irx = readFile("mc?:BIDATA-SYSTEM/USBD.IRX", -1, &size_usbd_irx);
			if (!usbd_irx) { // If don't exist it uses embedded
				if (sysPS3Detect() == 0) {
					usbd_irx = (void *) &usbd_ps2_irx;
					size_usbd_irx = size_usbd_ps2_irx;
				} else {
					usbd_irx = (void *) &usbd_ps3_irx;
					size_usbd_irx = size_usbd_ps3_irx;
				}
			}
		}
	}

	sysLoadModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL);
	sysLoadModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL);

	delay(gUSBDelay);

	LOG("USBSUPPORT Modules loaded\n");
}

void usbInit(void) {
	LOG("USBSUPPORT Init\n");
	usbULSizePrev = -2;
	memset(usbModifiedCDPrev, 0, 8);
	memset(usbModifiedDVDPrev, 0, 8);
	usbGameCount = 0;
	usbGames = NULL;
	configGetInt(configGetByType(CONFIG_OPL), "usb_frames_delay", &usbGameList.delay);
	ioPutRequest(IO_CUSTOM_SIMPLEACTION, &usbInitModules);
	usbGameList.enabled = 1;
}

item_list_t* usbGetObject(int initOnly) {
	if (initOnly && !usbGameList.enabled)
		return NULL;
	return &usbGameList;
}

static int usbNeedsUpdate(void) {
	int result = 0;
	fio_stat_t stat;
	char path[255];

	usbFindPartition(usbPrefix, "ul.cfg");

	sprintf(path, "%sCD", usbPrefix);
	if (fioGetstat(path, &stat) != 0)
		memset(stat.mtime, 0, 8);
	if (memcmp(usbModifiedCDPrev, stat.mtime, 8)) {
		memcpy(usbModifiedCDPrev, stat.mtime, 8);
		result = 1;
	}

	sprintf(path, "%sDVD", usbPrefix);
	if (fioGetstat(path, &stat) != 0)
		memset(stat.mtime, 0, 8);
	if (memcmp(usbModifiedDVDPrev, stat.mtime, 8)) {
		memcpy(usbModifiedDVDPrev, stat.mtime, 8);
		result = 1;
	}

	if (!sbIsSameSize(usbPrefix, usbULSizePrev))
		result = 1;

	return result;
}

static int usbUpdateGameList(void) {
	sbReadList(&usbGames, usbPrefix, &usbULSizePrev, &usbGameCount);
	return usbGameCount;
}

static int usbGetGameCount(void) {
	return usbGameCount;
}

static void* usbGetGame(int id) {
	return (void*) &usbGames[id];
}

static char* usbGetGameName(int id) {
	return usbGames[id].name;
}

static int usbGetGameNameLength(int id) {
	if (usbGames[id].isISO)
		return ISO_GAME_NAME_MAX + 1;
	else
		return UL_GAME_NAME_MAX + 1;
}

static char* usbGetGameStartup(int id) {
	return usbGames[id].startup;
}

#ifndef __CHILDPROOF
static void usbDeleteGame(int id) {
	sbDelete(&usbGames, usbPrefix, "/", usbGameCount, id);
	usbULSizePrev = -2;
}

/*static void usbRenameGame(int id, char* newName) {
	// TODO when/if Jimmi add rename functionnality to usbhdfs, then we should use the above method
	//sbRename(&usbGames, usbPrefix, "/", usbGameCount, id, newName);
	usbULSizePrev = -2;
}*/
#endif

static void usbLaunchGame(int id, config_set_t* configSet) {
	int i, fd, val, index, compatmask = 0;
	char partname[255], filename[32];
	base_game_info_t* game = &usbGames[id];

	fd = fioDopen(usbPrefix);
	if (fd < 0) {
		guiMsgBox(_l(_STR_ERR_FILE_INVALID), 0, NULL);
		return;
	}

#ifdef VMC
	char vmc_name[32], vmc_path[255];
	int vmc_id, have_error = 0, size_mcemu_irx = 0;
	usb_vmc_infos_t usb_vmc_infos;
	vmc_superblock_t vmc_superblock;

	for (vmc_id = 0; vmc_id < 2; vmc_id++) {
		memset(&usb_vmc_infos, 0, sizeof(usb_vmc_infos_t));
		configGetVMC(configSet, vmc_name, vmc_id);
		if (vmc_name[0]) {
			have_error = 1;
			if (sysCheckVMC(usbPrefix, "/", vmc_name, 0, &vmc_superblock) > 0) {
				usb_vmc_infos.flags = vmc_superblock.mc_flag & 0xFF;
				usb_vmc_infos.flags |= 0x100;
				usb_vmc_infos.specs.page_size = vmc_superblock.page_size;
				usb_vmc_infos.specs.block_size = vmc_superblock.pages_per_block;
				usb_vmc_infos.specs.card_size = vmc_superblock.pages_per_cluster * vmc_superblock.clusters_per_card;

				// Check vmc cluster chain (write operation can cause dammage)
				sprintf(vmc_path, "%s/VMC/%s.bin", gUSBPrefix, vmc_name);
				if (fioIoctl(fd, 0xCAFEC0DE, vmc_path)) {
					LOG("USBSUPPORT Cluster Chain OK\n");
					if ((i = fioIoctl(fd, 0xBEEFC0DE, vmc_path)) != 0) {
						have_error = 0;
						usb_vmc_infos.active = 1;
						usb_vmc_infos.start_sector = i;
						LOG("USBSUPPORT Start Sector: 0x%X\n", usb_vmc_infos.start_sector);
					}
				}
			}
		}

		if (have_error) {
			char error[255];
			snprintf(error, 255, _l(_STR_ERR_VMC_CONTINUE), vmc_name, (vmc_id + 1));
			if (!guiMsgBox(error, 1, NULL)) {
				fioDclose(fd);
				return;
			}
		}

		for (i = 0; i < size_usb_mcemu_irx; i++) {
			if (((u32*)&usb_mcemu_irx)[i] == (0xC0DEFAC0 + vmc_id)) {
				if (usb_vmc_infos.active)
					size_mcemu_irx = size_usb_mcemu_irx;
				memcpy(&((u32*)&usb_mcemu_irx)[i], &usb_vmc_infos, sizeof(usb_vmc_infos_t));
				break;
			}
		}
	}
#endif

	void** irx = &usb_cdvdman_irx;
	int irx_size = size_usb_cdvdman_irx;
	for (i = 0; i < game->parts; i++) {
		if (game->isISO)
			sprintf(partname, "%s/%s/%s.%s%s", gUSBPrefix, (game->media == 0x12) ? "CD" : "DVD", game->startup, game->name, game->extension);
		else
			sprintf(partname, "%s/ul.%08X.%s.%02x", gUSBPrefix, USBA_crc32(game->name), game->startup, i);

		if (gCheckUSBFragmentation) {
			if (fioIoctl(fd, 0xCAFEC0DE, partname) == 0) {
				fioDclose(fd);
				guiMsgBox(_l(_STR_ERR_FRAGMENTED), 0, NULL);
				return;
			}
		}

		if (i == 0) {
			val = fioIoctl(fd, 0xDEADC0DE, partname);
			LOG("USBSUPPORT Mass storage device sector size = %d\n", val);
			if (val == 4096) {
				irx = &usb_4Ksectors_cdvdman_irx;
				irx_size = size_usb_4Ksectors_cdvdman_irx;
			}
			compatmask = sbPrepare(game, configSet, irx_size, irx, &index);
		}

		val = fioIoctl(fd, 0xBEEFC0DE, partname);
		memcpy((void*)((u32)irx + index + 44 + 4 * i), &val, 4);
	}

	if (gRememberLastPlayed) {
		configSetStr(configGetByType(CONFIG_LAST), "last_played", game->startup);
		saveConfig(CONFIG_LAST, 0);
	}

	fioDclose(fd);

	char *altStartup = NULL;
	if (configGetStr(configSet, CONFIG_ITEM_ALTSTARTUP, &altStartup))
		strncpy(filename, altStartup, 32);
	else
		sprintf(filename, "%s", game->startup);
	shutdown(NO_EXCEPTION); // CAREFUL: shutdown will call usbCleanUp, so usbGames/game will be freed
	FlushCache(0);

#ifdef VMC
	sysLaunchLoaderElf(filename, "USB_MODE", irx_size, irx, size_mcemu_irx, &usb_mcemu_irx, compatmask, compatmask & COMPAT_MODE_1);
#else
	sysLaunchLoaderElf(filename, "USB_MODE", irx_size, irx, compatmask, compatmask & COMPAT_MODE_1);
#endif
}

static config_set_t* usbGetConfig(int id) {
	return sbPopulateConfig(&usbGames[id], usbPrefix, "/");
}

static int usbGetImage(char* folder, int isRelative, char* value, char* suffix, GSTEXTURE* resultTex, short psm) {
	char path[255];
	if (isRelative)
		sprintf(path, "%s%s/%s_%s", usbPrefix, folder, value, suffix);
	else
		sprintf(path, "%s%s_%s", folder, value, suffix);
	return texDiscoverLoad(resultTex, path, -1, psm);
}

static void usbCleanUp(int exception) {
	if (usbGameList.enabled) {
		LOG("USBSUPPORT CleanUp\n");

		free(usbGames);
	}
}

#ifdef VMC
static int usbCheckVMC(char* name, int createSize) {
	return sysCheckVMC(usbPrefix, "/", name, createSize, NULL);
}
#endif

static item_list_t usbGameList = {
		USB_MODE, 0, COMPAT, 0, MENU_MIN_INACTIVE_FRAMES, "USB Games", _STR_USB_GAMES, &usbInit, &usbNeedsUpdate,
#ifdef __CHILDPROOF
		&usbUpdateGameList, &usbGetGameCount, &usbGetGame, &usbGetGameName, &usbGetGameNameLength, &usbGetGameStartup, NULL, NULL,
#else
		&usbUpdateGameList, &usbGetGameCount, &usbGetGame, &usbGetGameName, &usbGetGameNameLength, &usbGetGameStartup, &usbDeleteGame, NULL,
#endif
#ifdef VMC
		&usbLaunchGame, &usbGetConfig, &usbGetImage, &usbCleanUp, &usbCheckVMC, USB_ICON
#else
		&usbLaunchGame, &usbGetConfig, &usbGetImage, &usbCleanUp, USB_ICON
#endif
};
