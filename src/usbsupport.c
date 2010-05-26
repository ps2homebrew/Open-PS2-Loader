#include "include/usbld.h"
#include "include/lang.h"
#include "include/supportbase.h"
#include "include/usbsupport.h"
#include "include/util.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/ioman.h"

extern void *usb_cdvdman_irx;
extern int size_usb_cdvdman_irx;

extern void *usbd_ps2_irx;
extern int size_usbd_ps2_irx;

extern void *usbd_ps3_irx;
extern int size_usbd_ps3_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

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
	return 0;
}

static void usbLoadModules(void) {
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

	SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, NULL);
	SifExecModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, NULL);

	delay(3);

	// update Themes
	usbFindPartition();
	char path[32];
	sprintf(path, "%sTHM", usbPrefix);
	thmAddElements(path, "/");
}

void usbInit(void) {
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
	if (usbULSizePrev == -1) // means we have not detected any usb device (yet)
		return 1;

	return sbIsSameSize(usbPrefix, usbULSizePrev);
}

static int usbUpdateGameList(void) {
	if (usbFindPartition())
		sbReadList(&usbGames, usbPrefix, &usbULSizePrev, &usbGameCount);
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

static int usbGetGameCompatibility(int id, int *dmaMode) {
	return sbGetCompatibility(&usbGames[id], usbGameList.mode);
}

static void usbSetGameCompatibility(int id, int compatMode, int dmaMode, short save) {
	sbSetCompatibility(&usbGames[id], usbGameList.mode, compatMode);
}

static void usbLaunchGame(int id) {
	int i, compatmask;
	char isoname[32];
	base_game_info_t* game = &usbGames[id];

	compatmask = sbPrepare(game, usbGameList.mode, isoname, size_usb_cdvdman_irx, &usb_cdvdman_irx, &i);

	int j, offset = 44;
	char partname[64];
	int fd, r;

	fd = fioDopen(usbPrefix);
	if (fd < 0) {
		init_scr();
		scr_clear();
		scr_printf("\n\t Fatal error opening %s...\n", usbPrefix);
		while(1);
	}
	for (j = 0; j < game->parts; j++) {
		sprintf(partname,"%s.%02x",isoname, j);
		LOG("partname: %s\n", partname);
		r = fioIoctl(fd, 0xBEEFC0DE, partname);
		memcpy((void*)((u32)&usb_cdvdman_irx + i + offset), &r, 4);
		offset += 4;
	}
	fioDclose(fd);

	FlushCache(0);

	sysLaunchLoaderElf(game->startup, "USB_MODE", size_usb_cdvdman_irx, &usb_cdvdman_irx, compatmask, compatmask & COMPAT_MODE_1);
}

static int usbGetArt(char* name, GSTEXTURE* resultTex, const char* type, short psm) {
	char path[64];
	sprintf(path, "%sART/%s_%s", usbPrefix, name, type);
	return texDiscoverLoad(resultTex, path, -1, psm);
}

static item_list_t usbGameList = {
		USB_MODE, 0, 0, MENU_MIN_INACTIVE_FRAMES, "USB Games", _STR_USB_GAMES, &usbInit, &usbNeedsUpdate, &usbUpdateGameList, &usbGetGameCount,
		&usbGetGame, &usbGetGameName, &usbGetGameStartup, &usbGetGameCompatibility, &usbSetGameCompatibility, &usbLaunchGame,
		&usbGetArt, NULL, USB_ICON
};
