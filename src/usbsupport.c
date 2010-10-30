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

static char usbPrefix[8];
static int usbULSizePrev = 0;
static int usbGameCount = 0;
static base_game_info_t *usbGames;

// forward declaration
static item_list_t usbGameList;

int usbFindPartition(char *target, char *name) {
	int i, fd;
	char path[64];

	for(i=0; i<5; i++){
		snprintf(path, 64, "mass%d:%s", i, name);
		fd = fioOpen(path, O_RDONLY);

		if(fd >= 0) {
			snprintf(target, 8, "mass%d:", i);
			fioClose(fd);
			return 1;
		}
	}

	// default to first partition (for themes, ...)
	sprintf(target, "mass0:");
	return 0;
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
	usbFindPartition(usbPrefix, "ul.cfg");
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
	usbFindPartition(usbPrefix, "ul.cfg");
	sbReadList(&usbGames, usbPrefix, "/", &usbULSizePrev, &usbGameCount);
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
	return configGetCompatibility(usbGames[id].startup, usbGameList.mode, NULL);
}

static void usbSetGameCompatibility(int id, int compatMode, int dmaMode) {
	configSetCompatibility(usbGames[id].startup, usbGameList.mode, compatMode, -1);
}

#ifdef VMC
static int usbPrepareMcemu(base_game_info_t* game) {
	char vmc[2][32];
	char vmc_path[64];
	u32 vmc_size;
	int i, j, fd, size_mcemu_irx = 0;
	usb_vmc_infos_t usb_vmc_infos;
	vmc_superblock_t vmc_superblock;

	configGetVMC(game->startup, vmc[0], USB_MODE, 0);
	configGetVMC(game->startup, vmc[1], USB_MODE, 1);

	if(!vmc[0][0] && !vmc[1][0]) return 0;  // skip if both empty

	for(i=0; i<2; i++) {
		memset(&usb_vmc_infos, 0, sizeof(usb_vmc_infos_t));
		memset(&vmc_superblock, 0, sizeof(vmc_superblock_t));

		snprintf(vmc_path, 64, "%sVMC/%s.bin", usbPrefix, vmc[i]);

		fd = fioOpen(vmc_path, O_RDWR);
		if (fd >= 0) {
			size_mcemu_irx = -1;
			LOG("%s open\n", vmc_path);

			vmc_size = fioLseek(fd, 0, SEEK_END);
			fioLseek(fd, 0, SEEK_SET);
			fioRead(fd, (void*)&vmc_superblock, sizeof(vmc_superblock_t));

			LOG("File size : 0x%X\n", vmc_size);
			LOG("Magic     : %s\n", vmc_superblock.magic);
			LOG("Card type : %d\n", vmc_superblock.mc_type);

			if(!strncmp(vmc_superblock.magic, "Sony PS2 Memory Card Format", 27) && vmc_superblock.mc_type == 0x2) {
				usb_vmc_infos.flags            = vmc_superblock.mc_flag & 0xFF;
				usb_vmc_infos.flags           |= 0x100;
				usb_vmc_infos.specs.page_size  = vmc_superblock.page_size;                                       
				usb_vmc_infos.specs.block_size = vmc_superblock.pages_per_block;                                 
				usb_vmc_infos.specs.card_size  = vmc_superblock.pages_per_cluster * vmc_superblock.clusters_per_card;

				LOG("flags            : 0x%X\n", usb_vmc_infos.flags           );
				LOG("specs.page_size  : 0x%X\n", usb_vmc_infos.specs.page_size );
				LOG("specs.block_size : 0x%X\n", usb_vmc_infos.specs.block_size);
				LOG("specs.card_size  : 0x%X\n", usb_vmc_infos.specs.card_size );

				if(vmc_size == usb_vmc_infos.specs.card_size * usb_vmc_infos.specs.page_size) {
					int fd1 = fioDopen(usbPrefix);

					if (fd1 >= 0) {
						snprintf(vmc_path, 64, "VMC/%s.bin", vmc[i]);
						// Check vmc cluster chain (write operation can cause dammage)
						if(fioIoctl(fd1, 0xCAFEC0DE, vmc_path)) {
							LOG("Cluster Chain OK\n");
							if((j = fioIoctl(fd1, 0xBEEFC0DE, vmc_path)) != 0) {
								usb_vmc_infos.active = 1;
								usb_vmc_infos.start_sector = j;
								LOG("Start Sector: 0x%X\n", usb_vmc_infos.start_sector);
							}
						} // else Vmc file fragmented
					}
					fioDclose(fd1);
				}
			}
			fioClose(fd);
		}
		for (j=0; j<size_usb_mcemu_irx; j++) {
			if (((u32*)&usb_mcemu_irx)[j] == (0xC0DEFAC0 + i)) {
				if(usb_vmc_infos.active)
					size_mcemu_irx = size_usb_mcemu_irx;
				memcpy(&((u32*)&usb_mcemu_irx)[j], &usb_vmc_infos, sizeof(usb_vmc_infos_t));
				break;
			}
		}
	}
	return size_mcemu_irx;
}
#endif

static void usbLaunchGame(int id) {
	int fd, r, index, i, compatmask;
	char isoname[32], partname[64], filename[32];
	base_game_info_t* game = &usbGames[id];

	fd = fioDopen(usbPrefix);
	if (fd < 0) {
		guiMsgBox(_l(_STR_ERR_FILE_INVALID), 0, NULL);
		return;
	}

	if (gCheckUSBFragmentation)
		for (i = 0; i < game->parts; i++) {
			if (game->isISO)
				sprintf(partname,"%s/%s.%s.iso", (game->media == 0x12) ? "CD" : "DVD", game->startup, game->name);
			else
				sprintf(partname,"%s.%02x",isoname, i);

			if (fioIoctl(fd, 0xCAFEC0DE, partname) == 0) {
				fioDclose(fd);
				guiMsgBox(_l(_STR_ERR_FRAGMENTED), 0, NULL);
				return;
			}
		}

	if (gRememberLastPlayed) {
		configSetStr(configGetByType(CONFIG_LAST), "last_played", game->startup);
		saveConfig(CONFIG_LAST, 0);
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

#ifdef VMC
	int size_mcemu_irx = usbPrepareMcemu(game);
	if (size_mcemu_irx == -1) {
		if (guiMsgBox(_l(_STR_ERR_VMC_CONTINUE), 1, NULL))
			size_mcemu_irx = 0;
		else
			return;
	}
#endif

	sprintf(filename,"%s",game->startup);
	shutdown(NO_EXCEPTION); // CAREFUL: shutdown will call usbCleanUp, so usbGames/game will be freed
	FlushCache(0);

#ifdef VMC
	sysLaunchLoaderElf(game->startup, "USB_MODE", irx_size, irx, size_mcemu_irx, &usb_mcemu_irx, compatmask, compatmask & COMPAT_MODE_1);
#else
	sysLaunchLoaderElf(game->startup, "USB_MODE", irx_size, irx, compatmask, compatmask & COMPAT_MODE_1);
#endif
}

static int usbGetArt(char* name, GSTEXTURE* resultTex, const char* type, short psm) {
	char path[64];
	sprintf(path, "%sART/%s_%s", usbPrefix, name, type);
	return texDiscoverLoad(resultTex, path, -1, psm);
}

static void usbCleanUp(int exception) {
	if (usbGameList.enabled) {
		LOG("usbCleanUp()\n");

		free(usbGames);
	}
}

#ifdef VMC
static int usbCheckVMC(char* name, int createSize) {
	return sysCheckVMC(usbPrefix, "/", name, createSize);
}
#endif

static item_list_t usbGameList = {
		USB_MODE, BASE_GAME_NAME_MAX + 1, 0, 0, MENU_MIN_INACTIVE_FRAMES, "USB Games", _STR_USB_GAMES, &usbInit, &usbNeedsUpdate,
#ifdef __CHILDPROOF
		&usbUpdateGameList, &usbGetGameCount, &usbGetGame, &usbGetGameName, &usbGetGameStartup, NULL, NULL,
#else
		&usbUpdateGameList, &usbGetGameCount, &usbGetGame, &usbGetGameName, &usbGetGameStartup, &usbDeleteGame, NULL,
#endif
#ifdef VMC
		&usbGetGameCompatibility, &usbSetGameCompatibility, &usbLaunchGame, &usbGetArt, &usbCleanUp, &usbCheckVMC, USB_ICON
#else
		&usbGetGameCompatibility, &usbSetGameCompatibility, &usbLaunchGame, &usbGetArt, &usbCleanUp, USB_ICON
#endif
};
