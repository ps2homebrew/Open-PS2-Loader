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

extern void *smsmap_irx;
extern int size_smsmap_irx;

extern void *smbman_irx;
extern int size_smbman_irx;

#ifdef VMC
extern void *smb_mcemu_irx;
extern int size_smb_mcemu_irx;
#endif

static char *ethPrefix = NULL;
static int ethULSizePrev = -2;
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
	char path[255];
	sprintf(path, "%sTHM", ethPrefix);
	thmAddElements(path, "\\", ethGameList.mode);

	sprintf(path, "%sCFG", ethPrefix);
	checkCreateDir(path);

#ifdef VMC
	sprintf(path, "%sVMC", ethPrefix);
	checkCreateDir(path);
#endif
}

void ethInit(void) {
	LOG("ethInit()\n");

	ethPrefix = "smb0:";
	ethULSizePrev = -2;
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
		char path[255];

		sprintf(path, "%sCD", ethPrefix);
		if (fioGetstat(path, &stat) != 0)
			memset(stat.mtime, 0, 8);
		if (memcmp(ethModifiedCDPrev, stat.mtime, 8)) {
			memcpy(ethModifiedCDPrev, stat.mtime, 8);
			result = 1;
		}

		sprintf(path, "%sDVD", ethPrefix);
		if (fioGetstat(path, &stat) != 0)
			memset(stat.mtime, 0, 8);
		if (memcmp(ethModifiedDVDPrev, stat.mtime, 8)) {
			memcpy(ethModifiedDVDPrev, stat.mtime, 8);
			result = 1;
		}

		if (!sbIsSameSize(ethPrefix, ethULSizePrev))
			result = 1;
	}
	return result;
}

static int ethUpdateGameList(void) {
	if (gNetworkStartup != 0)
		return 0;

	sbReadList(&ethGames, ethPrefix, &ethULSizePrev, &ethGameCount);
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

#ifdef VMC
static int ethPrepareMcemu(base_game_info_t* game, config_set_t* configSet) {
	char vmc[2][32];
	char vmc_path[255];
	u32 vmc_size;
	int i, j, fd, size_mcemu_irx = 0;
	smb_vmc_infos_t smb_vmc_infos;
	vmc_superblock_t vmc_superblock;

	configGetVMC(configSet, vmc[0], 0);
	configGetVMC(configSet, vmc[1], 1);

	for(i=0; i<2; i++) {
		if(!vmc[i][0]) // skip if empty
			continue;

		memset(&smb_vmc_infos, 0, sizeof(smb_vmc_infos_t));
		memset(&vmc_superblock, 0, sizeof(vmc_superblock_t));

		snprintf(vmc_path, 255, "%s\\VMC\\%s.bin", ethPrefix, vmc[i]);

		fd = fileXioOpen(vmc_path, O_RDONLY, 0666);
		if (fd >= 0) {
			size_mcemu_irx = -1;
			LOG("%s open\n", vmc_path);

			vmc_size = fileXioLseek(fd, 0, SEEK_END);
			fileXioLseek(fd, 0, SEEK_SET);
			fileXioRead(fd, (void*)&vmc_superblock, sizeof(vmc_superblock_t));

			LOG("File size : 0x%X\n", vmc_size);
			LOG("Magic     : %s\n", vmc_superblock.magic);
			LOG("Card type : %d\n", vmc_superblock.mc_type);

			if(!strncmp(vmc_superblock.magic, "Sony PS2 Memory Card Format", 27) && vmc_superblock.mc_type == 0x2) {
				smb_vmc_infos.flags            = vmc_superblock.mc_flag & 0xFF;
				smb_vmc_infos.flags           |= 0x100;
				smb_vmc_infos.specs.page_size  = vmc_superblock.page_size;                                       
				smb_vmc_infos.specs.block_size = vmc_superblock.pages_per_block;                                 
				smb_vmc_infos.specs.card_size  = vmc_superblock.pages_per_cluster * vmc_superblock.clusters_per_card;

				LOG("flags            : 0x%X\n", smb_vmc_infos.flags           );
				LOG("specs.page_size  : 0x%X\n", smb_vmc_infos.specs.page_size );
				LOG("specs.block_size : 0x%X\n", smb_vmc_infos.specs.block_size);
				LOG("specs.card_size  : 0x%X\n", smb_vmc_infos.specs.card_size );

				if(vmc_size == smb_vmc_infos.specs.card_size * smb_vmc_infos.specs.page_size) {
					smb_vmc_infos.active = 1;
					smb_vmc_infos.fid    = 0xFFFF;
					snprintf(vmc_path, 255, "VMC\\%s.bin", vmc[i]);
					strncpy(smb_vmc_infos.fname, vmc_path, 32); // maybe a too small size here ...

					LOG("%s is a valid Vmc file\n", smb_vmc_infos.fname );
				}
			}
			fileXioClose(fd);
		}
		for (j=0; j<size_smb_mcemu_irx; j++) {
			if (((u32*)&smb_mcemu_irx)[j] == (0xC0DEFAC0 + i)) {
				if(smb_vmc_infos.active)
					size_mcemu_irx = size_smb_mcemu_irx;
				memcpy(&((u32*)&smb_mcemu_irx)[j], &smb_vmc_infos, sizeof(smb_vmc_infos_t));
				break;
			}
		}
	}
	return size_mcemu_irx;
}
#endif

static void ethLaunchGame(int id, config_set_t* configSet) {
	if (gNetworkStartup != 0) {
		guiMsgBox(_l(_STR_NETWORK_STARTUP_ERROR), 0, NULL);
		return;
	}

	int i, compatmask, size_irx = 0;
	void** irx = NULL;
	char isoname[32], filename[32];
	base_game_info_t* game = &ethGames[id];

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

	compatmask = sbPrepare(game, configSet, isoname, size_irx, irx, &i);

	// For ISO we use the part table to store the "long" name (only for init)
	if (game->isISO)
		memcpy((void*)((u32)irx + i + 44), game->name, strlen(game->name) + 1);

	for (i = 0; i < size_irx; i++) {
		if (!strcmp((const char*)((u32)irx + i),"xxx.xxx.xxx.xxx")) {
			break;
		}
	}

#ifdef VMC
	int size_mcemu_irx = ethPrepareMcemu(game, configSet);
	if (size_mcemu_irx == -1) {
		if (guiMsgBox(_l(_STR_ERR_VMC_CONTINUE), 1, NULL))
			size_mcemu_irx = 0;
		else
			return;
	}
#endif

	char config_str[255];
	sprintf(config_str, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
	memcpy((void*)((u32)irx + i), config_str, strlen(config_str) + 1);
	memcpy((void*)((u32)irx + i + 16), &gPCPort, 4);
	memcpy((void*)((u32)irx + i + 20), gPCShareName, 32);
	memcpy((void*)((u32)irx + i + 56), gPCUserName, 32);
	memcpy((void*)((u32)irx + i + 92), gPCPassword, 32);

	// disconnect from the active SMB session
	ethSMBDisconnect();

	char *altStartup = NULL;
	if (configGetStr(configSet, CONFIG_ITEM_ALTSTARTUP, &altStartup))
		strncpy(filename, altStartup, 32);
	else
		sprintf(filename, "%s", game->startup);
	shutdown(NO_EXCEPTION); // CAREFUL: shutdown will call ethCleanUp, so ethGames/game will be freed
	FlushCache(0);

#ifdef VMC
	sysLaunchLoaderElf(filename, "ETH_MODE", size_irx, irx, size_mcemu_irx, &smb_mcemu_irx, compatmask, compatmask & COMPAT_MODE_1);
#else
	sysLaunchLoaderElf(filename, "ETH_MODE", size_irx, irx, compatmask, compatmask & COMPAT_MODE_1);
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
		LOG("ethCleanUp()\n");

		free(ethGames);
	}
}

#ifdef VMC
static int ethCheckVMC(char* name, int createSize) {
	return sysCheckVMC(ethPrefix, "\\", name, createSize);
}
#endif

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
		
		ret = fileXioDevctl(ethPrefix, SMB_DEVCTL_GETPASSWORDHASHES, (void *)&passwd, sizeof(passwd), (void *)&passwdhashes, sizeof(passwdhashes));
	
		if (ret == 0) {
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
	
	ret = fileXioDevctl(ethPrefix, SMB_DEVCTL_LOGON, (void *)&logon, sizeof(logon), NULL, 0);
	if (ret < 0)
		return -2;

	// SMB server alive test
	strcpy(echo.echo, "ALIVE ECHO TEST");
	echo.len = strlen("ALIVE ECHO TEST");

	ret = fileXioDevctl(ethPrefix, SMB_DEVCTL_ECHO, (void *)&echo, sizeof(echo), NULL, 0);
	if (ret < 0)
		return -3;

	// connect to the share
	strcpy(openshare.ShareName, gPCShareName);

	ret = fileXioDevctl(ethPrefix, SMB_DEVCTL_OPENSHARE, (void *)&openshare, sizeof(openshare), NULL, 0);
	if (ret < 0)
		return -4;

	return 0;	
}

int ethSMBDisconnect(void) {
	int ret;

	// closing share
	ret = fileXioDevctl(ethPrefix, SMB_DEVCTL_CLOSESHARE, NULL, 0, NULL, 0);
	if (ret < 0)
		return -1;

	// logoff/close tcp connection from SMB server:
	ret = fileXioDevctl(ethPrefix, SMB_DEVCTL_LOGOFF, NULL, 0, NULL, 0);
	if (ret < 0)
		return -2;

	return 0;	
}

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
