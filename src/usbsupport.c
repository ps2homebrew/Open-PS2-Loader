#include "include/usbld.h"
#include "include/lang.h"
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

void *usbd_irx;
int size_usbd_irx;

static char usbPrefix[8];
static int usbULSizePrev = 0;
static int usbGameCount = 0;
static base_game_info_t *usbGames;

// forward declaration
static item_list_t usbGameList;

static int usbFindPartition(void) {
	int i, fd;
	char path[64];

	for(i=0; i<5; i++){
		snprintf(path, 64, "mass%d:ul.cfg", i);
		fd = fioOpen(path, O_RDONLY);

		if(fd >= 0) {
			snprintf(usbPrefix, 8, "mass%d:", i);
			fioClose(fd);
			return 1;
		}
	}

	// default to first partition (for themes, ...)
	sprintf(usbPrefix, "mass0:");
	return 1;
}

static void usbLoadModules(void) {
	LOG("usbLoadModules()\n");
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

	delay(3);

	LOG("usbLoadModules: modules loaded\n");

	// update Themes
	usbFindPartition();
	char path[32];
	sprintf(path, "%sTHM/", usbPrefix);
	thmAddElements(path, "/");
}

void usbInit(void) {
	LOG("usbInit()\n");
	usbULSizePrev = -1;
	usbGameCount = 0;
	usbGames = NULL;

	ioPutRequest(IO_CUSTOM_SIMPLEACTION, &usbLoadModules);

	usbGameList.enabled = 1;
}

item_list_t* usbGetObject(int initOnly) {
	if (initOnly && !usbGameList.enabled)
		return NULL;
	return &usbGameList;
}

static int usbNeedsUpdate(void) {
	if (usbULSizePrev == -1) // means we have not detected any usb device (yet), or no ul.cfg file present
		return 1;

	return sbIsSameSize(usbPrefix, usbULSizePrev);
}

static int usbUpdateGameList(void) {
	if (usbFindPartition())
		sbReadList(&usbGames, usbPrefix, "/", &usbULSizePrev, &usbGameCount);
	else {
		usbGameCount = 0;
		usbULSizePrev = -1;
	}
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

static char* usbGetGameStartup(int id) {
	return usbGames[id].startup;
}

#ifndef __CHILDPROOF
static void usbDeleteGame(int id) {
	sbDelete(&usbGames, usbPrefix, "/", usbGameCount, id);
	usbULSizePrev = -1;
}

/*static void usbRenameGame(int id, char* newName) {
	// TODO when/if Jimmi add rename functionnality to usbhdfs, then we should use the above method
	//sbRename(&usbGames, usbPrefix, "/", usbGameCount, id, newName);

	usbULSizePrev = -1;
}*/
#endif

static int usbGetGameCompatibility(int id, int *dmaMode) {
	return sbGetCompatibility(&usbGames[id], usbGameList.mode);
}

static void usbSetGameCompatibility(int id, int compatMode, int dmaMode, short save) {
	sbSetCompatibility(&usbGames[id], usbGameList.mode, compatMode);
}

static int usbLaunchGame(int id) {
	int fd, r, index, i, compatmask;
	char isoname[32], partname[64];
	base_game_info_t* game = &usbGames[id];

	fd = fioDopen(usbPrefix);
	if (fd < 0)
		return ERROR_FILE_INVALID;

	if (gCheckUSBFragmentation)
		for (i = 0; i < game->parts; i++) {
			if (game->isISO)
				sprintf(partname,"%s/%s.%s.iso", (game->media == 0x12) ? "CD" : "DVD", game->startup, game->name);
			else
				sprintf(partname,"%s.%02x",isoname, i);

			if (fioIoctl(fd, 0xCAFEC0DE, partname) == 0) {
				fioDclose(fd);
				return ERROR_FRAGMENTED;
			}
		}

	shutdown(NO_EXCEPTION);

	if (gRememberLastPlayed) {
		setConfigStr(&gConfig, "last_played", game->startup);
		_saveConfig();
	}

	void *irx = &usb_cdvdman_irx;
	int irx_size = size_usb_cdvdman_irx;
	int offset = 44;

	r = fioIoctl(fd, 0xDEADC0DE, partname);
	LOG("mass storage device sectorsize = %d\n", r);

	if (r == 4096) {
		irx = &usb_4Ksectors_cdvdman_irx;
		irx_size = size_usb_4Ksectors_cdvdman_irx;
	}

	compatmask = sbPrepare(game, usbGameList.mode, isoname, irx_size, irx, &index);

	for (i = 0; i < game->parts; i++) {
		if (game->isISO)
			sprintf(partname,"%s/%s.%s.iso", (game->media == 0x12) ? "CD" : "DVD", game->startup, game->name);
		else
			sprintf(partname,"%s.%02x",isoname, i);

		r = fioIoctl(fd, 0xBEEFC0DE, partname);
		memcpy((void*)((u32)&usb_cdvdman_irx + index + offset), &r, 4);
		offset += 4;
	}

	fioDclose(fd);

	FlushCache(0);

	sysLaunchLoaderElf(game->startup, "USB_MODE", irx_size, irx, compatmask, compatmask & COMPAT_MODE_1);

	return 1;
}

static int usbGetArt(char* name, GSTEXTURE* resultTex, const char* type, short psm) {
	char path[64];
	sprintf(path, "%sART/%s_%s", usbPrefix, name, type);
	return texDiscoverLoad(resultTex, path, -1, psm);
}

static void usbCleanUp(int exception) {
	LOG("usbCleanUp()\n");

	free(usbGames);
}

static item_list_t usbGameList = {
		USB_MODE, BASE_GAME_NAME_MAX + 1, 0, 0, MENU_MIN_INACTIVE_FRAMES, "USB Games", _STR_USB_GAMES, &usbInit, &usbNeedsUpdate,
#ifdef __CHILDPROOF
		&usbUpdateGameList, &usbGetGameCount, &usbGetGame, &usbGetGameName, &usbGetGameStartup, NULL, NULL,
#else
		&usbUpdateGameList, &usbGetGameCount, &usbGetGame, &usbGetGameName, &usbGetGameStartup, &usbDeleteGame, NULL,
#endif
		&usbGetGameCompatibility, &usbSetGameCompatibility, &usbLaunchGame, &usbGetArt, &usbCleanUp, USB_ICON
};
