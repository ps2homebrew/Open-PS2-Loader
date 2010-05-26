#include "sys/fcntl.h"
#include "include/usbld.h"
#include "include/lang.h"
#include "include/hddsupport.h"
#include "include/textures.h"
#include "include/themes.h"
#include "include/ioman.h"

extern void *hdd_cdvdman_irx;
extern int size_hdd_cdvdman_irx;

extern void *hdd_pcmcia_cdvdman_irx;
extern int size_hdd_pcmcia_cdvdman_irx;

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *ps2atad_irx;
extern int size_ps2atad_irx;

extern void *ps2hdd_irx;
extern int size_ps2hdd_irx;

extern void *ps2fs_irx;
extern int size_ps2fs_irx;

static int hddFirstStart;
static char *hddPrefix = NULL;
static int hddGameCount = 0;
static hdl_games_list_t* hddGames;

// forward declaration
static item_list_t hddGameList;

static void hddLoadModules(void) {
	int ret, id;
	static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";

#ifndef __DEBUG
	if (!gDev9_loaded) {
		gHddStartup = 4;

		id=SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
		if ((id < 0) || ret) {
			gHddStartup = -1;
			return;
		}

		gDev9_loaded = 1;
	}
#endif

	gHddStartup = 3;

	id=SifExecModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gHddStartup = -1;
		return;
	}

	gHddStartup = 2;

	id=SifExecModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg, &ret);
	if ((id < 0) || ret) {
		gHddStartup = -1;
		return;
	}

	gHddStartup = 1;

	id=SifExecModuleBuffer(&ps2fs_irx, size_ps2fs_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gHddStartup = -1;
		return;
	}

	int res = fileXioMount(hddPrefix, "hdd0:+OPL", FIO_MT_RDONLY);
	if (res < 0) {
		fileXioUmount(hddPrefix);
		fileXioMount(hddPrefix, "hdd0:+OPL", FIO_MT_RDONLY);
	}

	gHddStartup = 0;

	// update Themes
	char path[32];
	sprintf(path, "%sTHM", hddPrefix);
	thmAddElements(path, "/");
}

void hddInit(void) {
	hddPrefix = "pfs0:";
	hddFirstStart = 1;
	hddGames = NULL;

	ioPutRequest(IO_CUSTOM_SIMPLEACTION, &hddLoadModules);

	hddGameList.enabled = 1;
}

item_list_t* hddGetObject(int initOnly) {
	if (initOnly && !hddGameList.enabled)
		return NULL;
	return &hddGameList;
}

static int hddNeedsUpdate(void) {
	// only update once
	if (hddFirstStart) {
		hddFirstStart = 0;
		return 1;
	}
	
	return 0;
}

static int hddUpdateGameList(void) {
	int ret = hddGetHDLGamelist(&hddGames);
	if (ret != 0)
		hddGameCount = 0;
	else
		hddGameCount = hddGames->count;
	return hddGameCount;
}

static int hddGetGameCount(void) {
	return hddGameCount;
}

static void* hddGetGame(int id) {
	return (void*) &hddGames->games[id];
}

static char* hddGetGameName(int id) {
	return hddGames->games[id].name;
}

static char* hddGetGameStartup(int id) {
	return hddGames->games[id].startup;
}

static int hddGetGameCompatibility(int id, int *dmaMode) {
	*dmaMode = 7; // defaulting to UDMA 4

	if(hddGames->games[id].dma_type == MDMA_MODE)
		*dmaMode = hddGames->games[id].dma_mode;
	else if(hddGames->games[id].dma_type == UDMA_MODE)
		*dmaMode = hddGames->games[id].dma_mode + 3;

	return hddGames->games[id].ops2l_compat_flags;
}

static void hddSetGameCompatibility(int id, int compatMode, int dmaMode, short save) {
	if (dmaMode == -1)
		dmaMode = 7;

	if(dmaMode < 3) {
		hddGames->games[id].dma_type = MDMA_MODE;
		hddGames->games[id].dma_mode = dmaMode;
	}
	else {
		hddGames->games[id].dma_type = UDMA_MODE;
		hddGames->games[id].dma_mode = (dmaMode - 3);
	}

	hddGames->games[id].ops2l_compat_flags = (unsigned char) compatMode;

	if (save)
		hddSetHDLGameInfo(id, &hddGames->games[id]);
}

static void hddLaunchGame(int id) {
	int i, size_irx = 0;
	void** irx = NULL;
	char filename[32];

	hdl_game_info_t* game = &hddGames->games[id];

	char* gid[5];
	getConfigDiscIDBinary(hddGames->games[id].startup, gid);

	if (game->dma_type == 0) {
		game->dma_type = UDMA_MODE;
		game->dma_mode = 4;
	}

	hddSetTransferMode(game->dma_type, game->dma_mode);

	sprintf(filename,"%s",game->startup);

	if (sysPcmciaCheck()) {
		size_irx = size_hdd_pcmcia_cdvdman_irx;
		irx = &hdd_pcmcia_cdvdman_irx;
	}
	else {
		size_irx = size_hdd_cdvdman_irx;
		irx = &hdd_cdvdman_irx;
	}

	for (i=0;i<size_irx;i++){
		if(!strcmp((const char*)((u32)irx+i),"######    GAMESETTINGS    ######")){
			break;
		}
	}

	if (game->ops2l_compat_flags & COMPAT_MODE_2) {
		u32 alt_read_mode = 1;
		memcpy((void*)((u32)irx+i+35),&alt_read_mode,1);
	}
	if (game->ops2l_compat_flags & COMPAT_MODE_5) {
		u32 no_dvddl = 1;
		memcpy((void*)((u32)irx+i+36),&no_dvddl,4);
	}
	if (game->ops2l_compat_flags & COMPAT_MODE_4) {
		u32 no_pss = 1;
		memcpy((void*)((u32)irx+i+40),&no_pss,4);
	}

	// game id
	memcpy((void*)((u32)irx+i+84), &gid, 5);

	// patch 48bit flag
	u8 flag_48bit = hddIs48bit() & 0xff;
	memcpy((void*)((u32)irx+i+34), &flag_48bit, 1);

	// patch start_sector
	memcpy((void*)((u32)irx+i+44), &game->start_sector, 4);

	FlushCache(0);

	sysLaunchLoaderElf(filename, "HDD_MODE", size_irx, irx, game->ops2l_compat_flags, game->ops2l_compat_flags & COMPAT_MODE_1);
}

static int hddGetArt(char* name, GSTEXTURE* resultTex, const char* type, short psm) {
	char path[32];
	sprintf(path, "%sART/%s_%s", hddPrefix, name, type);
	return texDiscoverLoad(resultTex, path, -1, psm);
}

static void hddCleanUp(void) {
	if (gHddStartup == 0)
		fileXioUmount(hddPrefix);
}

static item_list_t hddGameList = {
		HDD_MODE, 0, 0, 0, "HDD Games", _STR_HDD_GAMES, &hddInit, &hddNeedsUpdate, &hddUpdateGameList, &hddGetGameCount,
		&hddGetGame, &hddGetGameName, &hddGetGameStartup, &hddGetGameCompatibility, &hddSetGameCompatibility, &hddLaunchGame,
		&hddGetArt, &hddCleanUp, HDD_ICON
};
