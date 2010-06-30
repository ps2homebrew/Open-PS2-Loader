#include "include/usbld.h"
#include "include/lang.h"
#include "include/supportbase.h"
#include "include/ethsupport.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/ioman.h"
#include "include/smbman.h"
#include "include/system.h"

extern void *smb_cdvdman_irx;
extern int size_smb_cdvdman_irx;

extern void *smb_pcmcia_cdvdman_irx;
extern int size_smb_pcmcia_cdvdman_irx;

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *smsutils_irx;
extern int size_smsutils_irx;

extern void *smstcpip_irx;
extern int size_smstcpip_irx;

extern void *smsmap_irx;
extern int size_smsmap_irx;

extern void *smbman_irx;
extern int size_smbman_irx;

static char *ethPrefix = NULL;
static int ethULSizePrev = 0;
static unsigned char ethModifiedCDPrev[8];
static unsigned char ethModifiedDVDPrev[8];
static int ethGameCount = 0;
static base_game_info_t *ethGames = NULL;

// forward declaration
static item_list_t ethGameList;

static void ethLoadModules(void) {
	int ret, ipconfiglen;
	char ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));

	LOG("ethLoadModules()\n");

	ipconfiglen = sysSetIPConfig(ipconfig);

	gNetworkStartup = 5;

	ret = sysLoadModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
	if (ret < 0) {
		gNetworkStartup = -1;
		return;
	}

	gNetworkStartup = 4;

	ret = sysLoadModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL);
	if (ret < 0) {
		gNetworkStartup = -1;
		return;
	}
	gNetworkStartup = 3;

	ret = sysLoadModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL);
	if (ret < 0) {
		gNetworkStartup = -1;
		return;
	}

	gNetworkStartup = 2;

	ret = sysLoadModuleBuffer(&smsmap_irx, size_smsmap_irx, ipconfiglen, ipconfig);
	if (ret < 0) {
		gNetworkStartup = -1;
		return;
	}

	gNetworkStartup = 1;

	ret = sysLoadModuleBuffer(&smbman_irx, size_smbman_irx, 0, NULL);
	if (ret < 0) {
		gNetworkStartup = -1;
		return;
	}

	gNetworkStartup = 0; // ok, all loaded

	LOG("ethLoadModules: modules loaded\n");

	// connect
	ethSMBConnect();

	// update Themes
	char path[32];
	sprintf(path, "%s\\THM\\", ethPrefix);
	thmAddElements(path, "\\");
}

void ethInit(void) {
	LOG("ethInit()\n");

	ethPrefix = "smb0:";
	ethULSizePrev = -1;
	memset(ethModifiedCDPrev, 0, 8);
	memset(ethModifiedDVDPrev, 0, 8);
	ethGameCount = 0;
	ethGames = NULL;
	gNetworkStartup = 6;
	
	ioPutRequest(IO_CUSTOM_SIMPLEACTION, &ethLoadModules);

	ethGameList.enabled = 1;
}

item_list_t* ethGetObject(int initOnly) {
	if (initOnly && !ethGameList.enabled)
		return NULL;
	return &ethGameList;
}

static int ethNeedsUpdate(void) {
	int result = 0;
	if (gNetworkStartup == 0) {
		fio_stat_t stat;
		char path[64];

		snprintf(path, 64, "%sCD\\\\", ethPrefix);
		fioGetstat(path, &stat);
		if (memcmp(ethModifiedCDPrev, stat.mtime, 8)) {
			memcpy(ethModifiedCDPrev, stat.mtime, 8);
			result = 1;
		}

		snprintf(path, 64, "%sDVD\\\\", ethPrefix);
		fioGetstat(path, &stat);
		if (memcmp(ethModifiedDVDPrev, stat.mtime, 8)) {
			memcpy(ethModifiedDVDPrev, stat.mtime, 8);
			result = 1;
		}

		if (sbIsSameSize(ethPrefix, ethULSizePrev))
			result = 1;
	}
	return result;
}

static int ethUpdateGameList(void) {
	if (gNetworkStartup != 0)
		return 0;
	
	sbReadList(&ethGames, ethPrefix, "\\", &ethULSizePrev, &ethGameCount);
	return ethGameCount;
}

static int ethGetGameCount(void) {
	return ethGameCount;
}

static void* ethGetGame(int id) {
	return (void*) &ethGames[id];
}

static char* ethGetGameName(int id) {
	return ethGames[id].name;
}

static char* ethGetGameStartup(int id) {
	return ethGames[id].startup;
}

static int ethGetGameCompatibility(int id, int *dmaMode) {
	return sbGetCompatibility(&ethGames[id], ethGameList.mode);
}

static void ethSetGameCompatibility(int id, int compatMode, int dmaMode, short save) {
	sbSetCompatibility(&ethGames[id], ethGameList.mode, compatMode);
}

static void ethLaunchGame(int id) {
	if (gNetworkStartup != 0)
		return;

	shutdown(NO_EXCEPTION);

	int i, compatmask, size_irx = 0;
	void** irx = NULL;
	char isoname[32];
	base_game_info_t* game = &ethGames[id];

	if (sysPcmciaCheck()) {
		size_irx = size_smb_pcmcia_cdvdman_irx;
		irx = &smb_pcmcia_cdvdman_irx;
	}
	else {
		size_irx = size_smb_cdvdman_irx;
		irx = &smb_cdvdman_irx;
	}

	compatmask = sbPrepare(game, ethGameList.mode, isoname, size_irx, irx, &i);

	// For ISO we use the part table to store the "long" name (only for init)
	if (game->isISO)
		memcpy((void*)((u32)irx + i + 44), game->name, strlen(game->name) + 1);

	for (i = 0; i < size_irx; i++) {
		if (!strcmp((const char*)((u32)irx + i),"xxx.xxx.xxx.xxx")) {
			break;
		}
	}

	// disconnect from the active SMB session
	ethSMBDisconnect();

	char config_str[255];
	sprintf(config_str, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
	memcpy((void*)((u32)irx + i), config_str, strlen(config_str) + 1);
	memcpy((void*)((u32)irx + i + 16), &gPCPort, 4);
	memcpy((void*)((u32)irx + i + 20), gPCShareName, 32);
	memcpy((void*)((u32)irx + i + 56), gPCUserName, 32);
	memcpy((void*)((u32)irx + i + 92), gPCPassword, 32);

	FlushCache(0);

	sysLaunchLoaderElf(game->startup, "ETH_MODE", size_irx, irx, compatmask, compatmask & COMPAT_MODE_1);
}

static int ethGetArt(char* name, GSTEXTURE* resultTex, const char* type, short psm) {
	char path[64];
	sprintf(path, "%sART\\\\%s_%s", ethPrefix, name, type);
	return texDiscoverLoad(resultTex, path, -1, psm);
}

int ethSMBConnect(void) {
	int ret;
	smbLogOn_in_t logon;
	smbEcho_in_t echo;
	smbOpenShare_in_t openshare;

	// open tcp connection with the server / logon to SMB server
	sprintf(logon.serverIP, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
	logon.serverPort = gPCPort;
	
	if (strlen(gPCPassword) > 0) {
		smbGetPasswordHashes_in_t passwd;
		smbGetPasswordHashes_out_t passwdhashes;
		
		// we'll try to generate hashed password first
		strncpy(logon.User, gPCUserName, 32);
		strncpy(passwd.password, gPCPassword, 32);
		
		ret = fileXioDevctl("smb:", SMB_DEVCTL_GETPASSWORDHASHES, (void *)&passwd, sizeof(passwd), (void *)&passwdhashes, sizeof(passwdhashes));
	
		if (ret == 0) {
			// hash generated okay, can use
			memcpy((void *)logon.Password, (void *)&passwdhashes, sizeof(passwdhashes));
			logon.PasswordType = HASHED_PASSWORD;
		} else {
			// failed hashing, failback to plaintext
			strncpy(logon.Password, gPCPassword, 32);
			logon.PasswordType = PLAINTEXT_PASSWORD;
		}
	} else {
		strcpy(logon.User, "GUEST");
		logon.PasswordType = NO_PASSWORD;
	}
	
	ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGON, (void *)&logon, sizeof(logon), NULL, 0);
	if (ret < 0)
		return -2;

	// SMB server alive test
	strcpy(echo.echo, "ALIVE ECHO TEST");
	echo.len = strlen("ALIVE ECHO TEST");

	ret = fileXioDevctl("smb:", SMB_DEVCTL_ECHO, (void *)&echo, sizeof(echo), NULL, 0);
	if (ret < 0)
		return -3;

	// connect to the share
	strcpy(openshare.ShareName, gPCShareName);
	openshare.PasswordType = NO_PASSWORD;

	ret = fileXioDevctl("smb:", SMB_DEVCTL_OPENSHARE, (void *)&openshare, sizeof(openshare), NULL, 0);
	if (ret < 0)
		return -4;

	return 0;	
}

int ethSMBDisconnect(void) {
	int ret;

	// closing share
	ret = fileXioDevctl("smb:", SMB_DEVCTL_CLOSESHARE, NULL, 0, NULL, 0);
	if (ret < 0)
		return -1;

	// logoff/close tcp connection from SMB server:
	ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGOFF, NULL, 0, NULL, 0);
	if (ret < 0)
		return -2;

	return 0;	
}



static item_list_t ethGameList = {
		ETH_MODE, 0, 0, MENU_MIN_INACTIVE_FRAMES, "ETH Games", _STR_NET_GAMES, &ethInit, &ethNeedsUpdate, &ethUpdateGameList, &ethGetGameCount,
		&ethGetGame, &ethGetGameName, &ethGetGameStartup, &ethGetGameCompatibility, &ethSetGameCompatibility, &ethLaunchGame,
		&ethGetArt, NULL, ETH_ICON
};
