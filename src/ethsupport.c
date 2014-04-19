#include "include/usbld.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/supportbase.h"
#include "include/ethsupport.h"
#include "include/util.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/ioman.h"
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

extern void *smap_irx;
extern int size_smap_irx;

extern void *smbman_irx;
extern int size_smbman_irx;

#ifdef VMC
extern void *smb_mcemu_irx;
extern int size_smb_mcemu_irx;
#endif

static char ethPrefix[40];
static char* ethBase;
static int ethULSizePrev = -2;
static unsigned char ethModifiedCDPrev[8];
static unsigned char ethModifiedDVDPrev[8];
static int ethGameCount = 0;
static base_game_info_t *ethGames = NULL;

// forward declaration
static item_list_t ethGameList;

void ethSMBConnect(void) {
	smbLogOn_in_t logon;
	smbEcho_in_t echo;
	smbOpenShare_in_t openshare;
	int result;

	if (gETHPrefix[0] != '\0')
		sprintf(ethPrefix, "%s%s\\", ethBase, gETHPrefix);
	else
		sprintf(ethPrefix, ethBase);

	// open tcp connection with the server / logon to SMB server
	sprintf(logon.serverIP, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
	logon.serverPort = gPCPort;

	if (strlen(gPCPassword) > 0) {
		smbGetPasswordHashes_in_t passwd;
		smbGetPasswordHashes_out_t passwdhashes;

		// we'll try to generate hashed password first
		strncpy(logon.User, gPCUserName, 32);
		strncpy(passwd.password, gPCPassword, 32);

		if (fileXioDevctl(ethBase, SMB_DEVCTL_GETPASSWORDHASHES, (void *)&passwd, sizeof(passwd), (void *)&passwdhashes, sizeof(passwdhashes)) == 0) {
			// hash generated okay, can use
			memcpy((void *)logon.Password, (void *)&passwdhashes, sizeof(passwdhashes));
			logon.PasswordType = HASHED_PASSWORD;
			memcpy((void *)openshare.Password, (void *)&passwdhashes, sizeof(passwdhashes));
			openshare.PasswordType = HASHED_PASSWORD;
		} else {
			// failed hashing, failback to plaintext
			strncpy(logon.Password, gPCPassword, 32);
			logon.PasswordType = PLAINTEXT_PASSWORD;
			strncpy(openshare.Password, gPCPassword, 32);
			openshare.PasswordType = PLAINTEXT_PASSWORD;
		}
	} else {
		strncpy(logon.User, gPCUserName, 32);
		logon.PasswordType = NO_PASSWORD;
		openshare.PasswordType = NO_PASSWORD;
	}

	if ((result=fileXioDevctl(ethBase, SMB_DEVCTL_LOGON, (void *)&logon, sizeof(logon), NULL, 0)) >= 0) {
		// SMB server alive test
		strcpy(echo.echo, "ALIVE ECHO TEST");
		echo.len = strlen("ALIVE ECHO TEST");

		if (fileXioDevctl(ethBase, SMB_DEVCTL_ECHO, (void *)&echo, sizeof(echo), NULL, 0) >= 0) {
			gNetworkStartup = ERROR_ETH_SMB_OPENSHARE;

			if (gPCShareName[0]) {
				// connect to the share
				strcpy(openshare.ShareName, gPCShareName);

				if (fileXioDevctl(ethBase, SMB_DEVCTL_OPENSHARE, (void *)&openshare, sizeof(openshare), NULL, 0) >= 0) {
					// everything is ok
					gNetworkStartup = 0;
				}
			}
		}
		else{
			gNetworkStartup = ERROR_ETH_SMB_ECHO;
		}
	}
	else{
		gNetworkStartup = (result==-SMB_DEVCTL_LOGON_ERR_CONN)? ERROR_ETH_SMB_CONN : ERROR_ETH_SMB_LOGON;
	}
}

int ethSMBDisconnect(void) {
	int ret;

	// closing share
	ret = fileXioDevctl(ethBase, SMB_DEVCTL_CLOSESHARE, NULL, 0, NULL, 0);
	if (ret < 0)
		return -1;

	// logoff/close tcp connection from SMB server:
	ret = fileXioDevctl(ethBase, SMB_DEVCTL_LOGOFF, NULL, 0, NULL, 0);
	if (ret < 0)
		return -2;

	return 0;
}

static void ethInitSMB(void) {
	// connect
	ethSMBConnect();

	if (gNetworkStartup == 0) {
		// update Themes
		char path[255];
		sprintf(path, "%sTHM", ethPrefix);
		thmAddElements(path, "\\", ethGameList.mode);

		sprintf(path, "%sCFG", ethPrefix);
		checkCreateDir(path);

#ifdef VMC
		sprintf(path, "%sVMC", ethPrefix);
		checkCreateDir(path);
#endif
	} else if (gPCShareName[0] || !(gNetworkStartup >= ERROR_ETH_SMB_OPENSHARE)) {
		switch(gNetworkStartup){
			case ERROR_ETH_SMB_CONN:
				setErrorMessage(_STR_NETWORK_STARTUP_ERROR_CONN, gNetworkStartup);
				break;
			case ERROR_ETH_SMB_LOGON:
				setErrorMessage(_STR_NETWORK_STARTUP_ERROR_LOGON, gNetworkStartup);
				break;
			case ERROR_ETH_SMB_OPENSHARE:
				setErrorMessage(_STR_NETWORK_STARTUP_ERROR_SHARE, gNetworkStartup);
				break;
			default:
				setErrorMessage(_STR_NETWORK_STARTUP_ERROR, gNetworkStartup);
		}
	}
}

static void ethLoadModules(void) {
	int ipconfiglen;
	char ipconfig[IPCONFIG_MAX_LEN];

	LOG("ETHSUPPORT LoadModules\n");

	ipconfiglen = sysSetIPConfig(ipconfig);

	gNetworkStartup = ERROR_ETH_MODULE_PS2DEV9_FAILURE;
	if (sysLoadModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL) >= 0) {
		gNetworkStartup = ERROR_ETH_MODULE_SMSUTILS_FAILURE;
		if (sysLoadModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL) >= 0) {
			gNetworkStartup = ERROR_ETH_MODULE_SMSTCPIP_FAILURE;
			if (sysLoadModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL) >= 0) {
				gNetworkStartup = ERROR_ETH_MODULE_SMSMAP_FAILURE;
				if (sysLoadModuleBuffer(&smap_irx, size_smap_irx, ipconfiglen, ipconfig) >= 0) {
					gNetworkStartup = ERROR_ETH_MODULE_SMBMAN_FAILURE;
					if (sysLoadModuleBuffer(&smbman_irx, size_smbman_irx, 0, NULL) >= 0) {
						LOG("ETHSUPPORT Modules loaded\n");
						ethInitSMB();
						return;
					}
				}
			}
		}
	}

	setErrorMessage(_STR_NETWORK_STARTUP_ERROR, gNetworkStartup);
}

void ethInit(void) {
	if (gNetworkStartup >= ERROR_ETH_SMB_LOGON) {
		LOG("ETHSUPPORT Re-Init\n");
		thmReinit(ethBase);
		ethULSizePrev = -2;
		ethGameCount = 0;
		ioPutRequest(IO_CUSTOM_SIMPLEACTION, &ethInitSMB);
	} else {
		LOG("ETHSUPPORT Init\n");
		ethBase = "smb0:";
		ethULSizePrev = -2;
		memset(ethModifiedCDPrev, 0, 8);
		memset(ethModifiedDVDPrev, 0, 8);
		ethGameCount = 0;
		ethGames = NULL;
		configGetInt(configGetByType(CONFIG_OPL), "eth_frames_delay", &ethGameList.delay);
		gNetworkStartup = ERROR_ETH_NOT_STARTED;
		ioPutRequest(IO_CUSTOM_SIMPLEACTION, &ethLoadModules);
		ethGameList.enabled = 1;
	}
}

item_list_t* ethGetObject(int initOnly) {
	if (initOnly && !ethGameList.enabled)
		return NULL;
	return &ethGameList;
}

static int ethNeedsUpdate(void) {
	if (ethULSizePrev == -2)
		return 1;

	if (gNetworkStartup == 0) {
		fio_stat_t stat;
		char path[255];

		sprintf(path, "%sCD", ethPrefix);
		if (fioGetstat(path, &stat) != 0)
			memset(stat.mtime, 0, 8);
		if (memcmp(ethModifiedCDPrev, stat.mtime, 8)) {
			memcpy(ethModifiedCDPrev, stat.mtime, 8);
			return 1;
		}

		sprintf(path, "%sDVD", ethPrefix);
		if (fioGetstat(path, &stat) != 0)
			memset(stat.mtime, 0, 8);
		if (memcmp(ethModifiedDVDPrev, stat.mtime, 8)) {
			memcpy(ethModifiedDVDPrev, stat.mtime, 8);
			return 1;
		}

		if (!sbIsSameSize(ethPrefix, ethULSizePrev))
			return 1;
	}

	return 0;
}

static int ethUpdateGameList(void) {
	if (gPCShareName[0]) {
		if (gNetworkStartup != 0)
			return 0;

		sbReadList(&ethGames, ethPrefix, &ethULSizePrev, &ethGameCount);
	} else {
		int i, count;
		ShareEntry_t sharelist[128];
		smbGetShareList_in_t getsharelist;
		getsharelist.EE_addr = (void *)&sharelist[0];
		getsharelist.maxent = 128;

		count = fileXioDevctl(ethBase, SMB_DEVCTL_GETSHARELIST, (void *)&getsharelist, sizeof(getsharelist), NULL, 0);
		if (count > 0) {
			free(ethGames);
			ethGames = (base_game_info_t*)malloc(sizeof(base_game_info_t) * count);
			for (i = 0; i < count; i++) {
				LOG("ETHSUPPORT Share found: %s\n", sharelist[i].ShareName);
				base_game_info_t *g = &ethGames[i];
				memcpy(g->name, sharelist[i].ShareName, 32);
				g->name[31] = '\0';
				sprintf(g->startup, "SHARE");
				g->extension[0] = '\0';
				g->parts = 0x00;
				g->media = 0x00;
				g->isISO = 0;
				g->sizeMB = 0;
			}
			ethGameCount = count;
		}
	}
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

static int ethGetGameNameLength(int id) {
	if (ethGames[id].isISO)
		return ISO_GAME_NAME_MAX + 1;
	else
		return UL_GAME_NAME_MAX + 1;
}

static char* ethGetGameStartup(int id) {
	return ethGames[id].startup;
}

#ifndef __CHILDPROOF
static void ethDeleteGame(int id) {
	sbDelete(&ethGames, ethPrefix, "\\", ethGameCount, id);
	ethULSizePrev = -2;
}

static void ethRenameGame(int id, char* newName) {
	sbRename(&ethGames, ethPrefix, "\\", ethGameCount, id, newName);
	ethULSizePrev = -2;
}
#endif

static void ethLaunchGame(int id, config_set_t* configSet) {
	int i, compatmask, size_irx = 0;
	void** irx = NULL;
	char filename[32];
	base_game_info_t* game = &ethGames[id];

	if (!gPCShareName[0]) {
		memcpy(gPCShareName, game->name, 32);
		ethULSizePrev = -2;
		ethGameCount = 0;
		ioPutRequest(IO_MENU_UPDATE_DEFFERED, &ethGameList.mode); // clear the share list
		ioPutRequest(IO_CUSTOM_SIMPLEACTION, &ethInitSMB);
		ioPutRequest(IO_MENU_UPDATE_DEFFERED, &ethGameList.mode); // reload the game list
		return;
	}

#ifdef VMC
	char vmc_name[32];
	int vmc_id, size_mcemu_irx = 0;
	smb_vmc_infos_t smb_vmc_infos;
	vmc_superblock_t vmc_superblock;

	for (vmc_id = 0; vmc_id < 2; vmc_id++) {
		memset(&smb_vmc_infos, 0, sizeof(smb_vmc_infos_t));
		configGetVMC(configSet, vmc_name, vmc_id);
		if (vmc_name[0]) {
			if (sysCheckVMC(ethPrefix, "\\", vmc_name, 0, &vmc_superblock) > 0) {
				smb_vmc_infos.flags = vmc_superblock.mc_flag & 0xFF;
				smb_vmc_infos.flags |= 0x100;
				smb_vmc_infos.specs.page_size = vmc_superblock.page_size;
				smb_vmc_infos.specs.block_size = vmc_superblock.pages_per_block;
				smb_vmc_infos.specs.card_size = vmc_superblock.pages_per_cluster * vmc_superblock.clusters_per_card;
				smb_vmc_infos.active = 1;
				smb_vmc_infos.fid = 0xFFFF;
				if (gETHPrefix[0])
					snprintf(smb_vmc_infos.fname, 64, "%s\\VMC\\%s.bin", gETHPrefix, vmc_name); // may still be too small size here ;) (should add 11 char !)
				else
					snprintf(smb_vmc_infos.fname, 64, "VMC\\%s.bin", vmc_name);
			} else {
				char error[255];
				snprintf(error, 255, _l(_STR_ERR_VMC_CONTINUE), vmc_name, (vmc_id + 1));
				if (!guiMsgBox(error, 1, NULL))
					return;
			}
		}

		for (i = 0; i < size_smb_mcemu_irx; i++) {
			if (((u32*)&smb_mcemu_irx)[i] == (0xC0DEFAC0 + vmc_id)) {
				if (smb_vmc_infos.active)
					size_mcemu_irx = size_smb_mcemu_irx;
				memcpy(&((u32*)&smb_mcemu_irx)[i], &smb_vmc_infos, sizeof(smb_vmc_infos_t));
				break;
			}
		}
	}
#endif

	if (gRememberLastPlayed) {
		configSetStr(configGetByType(CONFIG_LAST), "last_played", game->startup);
		saveConfig(CONFIG_LAST, 0);
	}

	if (sysPcmciaCheck()) {
		size_irx = size_smb_pcmcia_cdvdman_irx;
		irx = &smb_pcmcia_cdvdman_irx;
	}
	else {
		size_irx = size_smb_cdvdman_irx;
		irx = &smb_cdvdman_irx;
	}

	compatmask = sbPrepare(game, configSet, size_irx, irx, &i);

	// For ISO we use the part table to store the "long" name (only for init)
	if (game->isISO) {
		memcpy((void*)((u32)irx + i), game->extension, 5);
		strcpy((void*)((u32)irx + i + 5), game->startup);
		memcpy((void*)((u32)irx + i + 44), game->name, strlen(game->name) + 1);
	} else {
		sprintf(filename, "ul.%08X.%s", USBA_crc32(game->name), game->startup);
		memcpy((void*)((u32)irx + i), filename, strlen(filename) + 1);
	}

	for (i = 0; i < size_irx; i++) {
		if (!strcmp((const char*)((u32)irx + i),"xxx.xxx.xxx.xxx")) {
			break;
		}
	}

	char config_str[255];
	sprintf(config_str, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
	memcpy((void*)((u32)irx + i), config_str, strlen(config_str) + 1);
	memcpy((void*)((u32)irx + i + 16), &gPCPort, 4);
	memcpy((void*)((u32)irx + i + 20), gPCShareName, 32);
	memcpy((void*)((u32)irx + i + 52), gETHPrefix, 32);
	memcpy((void*)((u32)irx + i + 84), gPCUserName, 16);
	memcpy((void*)((u32)irx + i + 100), gPCPassword, 16);

	// disconnect from the active SMB session
	ethSMBDisconnect();

	const char *altStartup = NULL;
	if (configGetStr(configSet, CONFIG_ITEM_ALTSTARTUP, &altStartup))
		strncpy(filename, altStartup, 32);
	else
		sprintf(filename, "%s", game->startup);
	shutdown(NO_EXCEPTION); // CAREFUL: shutdown will call ethCleanUp, so ethGames/game will be freed

#ifdef VMC
	sysLaunchLoaderElf(filename, "ETH_MODE", size_irx, irx, size_mcemu_irx, &smb_mcemu_irx, compatmask);
#else
	sysLaunchLoaderElf(filename, "ETH_MODE", size_irx, irx, compatmask);
#endif
}

static config_set_t* ethGetConfig(int id) {
	return sbPopulateConfig(&ethGames[id], ethPrefix, "\\");
}

static int ethGetImage(char* folder, int isRelative, char* value, char* suffix, GSTEXTURE* resultTex, short psm) {
	char path[255];
	if (isRelative)
		sprintf(path, "%s%s\\%s_%s", ethPrefix, folder, value, suffix);
	else
		sprintf(path, "%s%s_%s", folder, value, suffix);
	return texDiscoverLoad(resultTex, path, -1, psm);
}

static void ethCleanUp(int exception) {
	if (ethGameList.enabled) {
		LOG("ETHSUPPORT CleanUp\n");

		free(ethGames);
	}
}

#ifdef VMC
static int ethCheckVMC(char* name, int createSize) {
	return sysCheckVMC(ethPrefix, "\\", name, createSize, NULL);
}
#endif

static item_list_t ethGameList = {
		ETH_MODE, 0, COMPAT, 0, MENU_MIN_INACTIVE_FRAMES, "ETH Games", _STR_NET_GAMES, &ethInit, &ethNeedsUpdate,
#ifdef __CHILDPROOF
		&ethUpdateGameList, &ethGetGameCount, &ethGetGame, &ethGetGameName, &ethGetGameNameLength, &ethGetGameStartup, NULL, NULL,
#else
		&ethUpdateGameList, &ethGetGameCount, &ethGetGame, &ethGetGameName, &ethGetGameNameLength, &ethGetGameStartup, &ethDeleteGame, &ethRenameGame,
#endif
#ifdef VMC
		&ethLaunchGame, &ethGetConfig, &ethGetImage, &ethCleanUp, &ethCheckVMC, ETH_ICON
#else
		&ethLaunchGame, &ethGetConfig, &ethGetImage, &ethCleanUp, ETH_ICON
#endif
};
