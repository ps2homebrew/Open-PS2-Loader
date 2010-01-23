/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include "include/lang.h"

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *ps2atad_irx;
extern int size_ps2atad_irx;

extern void *ps2hdd_irx;
extern int size_ps2hdd_irx;

extern void *smbman_irx;
extern int size_smbman_irx;

extern void *smsutils_irx;
extern int size_smsutils_irx;

extern void *smstcpip_irx;
extern int size_smstcpip_irx;

extern void *smsmap_irx;
extern int size_smsmap_irx;
#define IPCONFIG_MAX_LEN        64
char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
int g_ipconfig_len;

extern void set_ipconfig(void);

// language id
int gLanguageID = 0;

int eth_inited = 0;
int hdd_inited = 0;

// frames since button was pressed
int inactiveFrames;

int hddfound;
hdl_games_list_t *hddGameList;


// minimal inactive frames for hint display
#define HINT_FRAMES_MIN 64

// --------------------------- Dialogs ---------------------------
// Dialog definition for IP configuration
struct UIItem diaIPConfig[] = {
	{UI_LABEL, 0, NULL, {.label = {"", _STR_IPCONFIG}}},
	
	{UI_SPLITTER},

	// ---- IP address ----
	{UI_LABEL, 0, NULL, {.label = {"- PS2 -", -1}}}, {UI_BREAK},
	
	{UI_LABEL, 0, NULL, {.label = {"IP", -1}}}, {UI_SPACER}, 
	
	{UI_INT, 2, NULL, {.intvalue = {192, 192, 0, 255}}}, {UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 3, NULL, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 4, NULL, {.intvalue = {0, 0, 0, 255}}}, {UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 5, NULL, {.intvalue = {10, 10, 0, 255}}},

	{UI_BREAK},

	//  ---- Netmask ----
	{UI_LABEL, 0, NULL, {.label = {"MASK", -1}}},
	{UI_SPACER}, 
	
	{UI_INT, 6, NULL, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 7, NULL, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 8, NULL, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 9, NULL, {.intvalue = {0, 0, 0, 255}}},
	
	{UI_BREAK},
	
	//  ---- Gateway ----
	{UI_LABEL, 0, NULL, {.label = {"GW", -1}}},
	{UI_SPACER}, 
	
	{UI_INT, 10, NULL, {.intvalue = {192, 192, 0, 255}}}, 	{UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 11, NULL, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 12, NULL, {.intvalue = {0, 0, 0, 255}}}, 	{UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 13, NULL, {.intvalue = {1, 1, 0, 255}}},
	
	{UI_SPLITTER},
	
	//  ---- PC ----
	{UI_LABEL, 0, NULL, {.label = {"- PC -", -1}}},
	{UI_BREAK},
	
	{UI_LABEL, 0, NULL, {.label = {"IP", -1}}},
	{UI_SPACER}, 
	
	{UI_INT, 14, NULL, {.intvalue = {192, 192, 0, 255}}},	{UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 15, NULL, {.intvalue = {168, 168, 0, 255}}},	{UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 16, NULL, {.intvalue = {0, 0, 0, 255}}},	{UI_LABEL, 0, NULL, {.label = {".", -1}}},
	{UI_INT, 17, NULL, {.intvalue = {1, 1, 0, 255}}},
	
	{UI_BREAK},
	
	{UI_LABEL, 0, NULL, {.label = {"PORT", -1}}},	{UI_SPACER}, {UI_INT, 18, NULL, {.intvalue = {445, 445, 0, 1024}}},
	
	{UI_BREAK},
	
	//  ---- PC share name ----
	{UI_LABEL, 0, NULL, {.label = {"SHARE NAME", -1}}}, {UI_SPACER}, {UI_STRING, 19, NULL, {.stringvalue = {"PS2SMB", "PS2SMB"}}}, {UI_BREAK},
	
	{UI_LABEL, 0, NULL, {.label = {"", _STR_NETWORK_AUTOSTART}}}, {UI_SPACER}, {UI_BOOL, 20, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	//  ---- Ok ----
	{UI_SPLITTER},
	{UI_OK},
	
	// end of dialogue
	{UI_TERMINATOR}
};

// Dialog for per-game compatibility settings
#define COMPAT_SAVE 100
#define COMPAT_TEST 101
#define COMPAT_REMOVE 102
#define COMPAT_LOADFROMDISC 104
#define COMPAT_MODE_BASE 10
#define COMPAT_GAMEID 50
#define COMPAT_MODE_COUNT 6
struct UIItem diaCompatConfig[] = {
	{UI_LABEL, 110, NULL, {.label = {"<Game Label>", -1}}},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 111, NULL, {.label = {"", _STR_COMPAT_SETTINGS}}}, {UI_BREAK}, {UI_BREAK}, {UI_BREAK},

	{UI_LABEL, 0, NULL, {.label = {"Mode 1", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE    , "Load alt. core", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 2", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 1, "Alternative data read method", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 3", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 2, "Unhook Syscalls", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 4", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 3, "0 PSS mode", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 5", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 4, "Disable DVD-DL", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 6", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 5, "Disable IGR", {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"DMA Mode", -1}}}, {UI_SPACER}, {UI_ENUM, COMPAT_MODE_BASE + 6, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"Game ID", -1}}}, {UI_SPACER}, {UI_STRING, COMPAT_GAMEID, NULL, {.stringvalue = {"", ""}}}, {UI_BREAK},
	{UI_BUTTON, COMPAT_LOADFROMDISC, NULL, {.label = {NULL, _STR_LOAD_FROM_DISC}}},
	
	{UI_SPLITTER},
	
	{UI_OK}, {UI_SPACER}, {UI_BUTTON, COMPAT_SAVE, NULL, {.label = {"", _STR_SAVE_CHANGES}}}, 
		 {UI_SPACER}, {UI_BUTTON, COMPAT_TEST, NULL, {.label = {"Test", -1}}},
	
	{UI_SPLITTER},
	
	{UI_BUTTON, COMPAT_REMOVE, NULL, {.label = {"", _STR_REMOVE_ALL_SETTINGS}}},
	
	// end of dialogue
	{UI_TERMINATOR}
};

// Dialog for ui settings
#define UICFG_THEME 10
#define UICFG_LANG 11
#define UICFG_SCROLL 12
#define UICFG_MENU 13
#define UICFG_BGCOL 14
#define UICFG_TXTCOL 15
#define UICFG_EXITTO 16
#define UICFG_DEFDEVICE 17
#define UICFG_USEHDD 18
#define UICFG_AUTOSTARTHDD 19
#define UICFG_AUTOSORT 20

#define UICFG_SAVE 114
struct UIItem diaUIConfig[] = {
	{UI_LABEL, 111, NULL, {.label = {"", _STR_SETTINGS}}},
	{UI_SPLITTER},

	{UI_LABEL, 0, NULL, {.label = {"", _STR_THEME}}}, {UI_SPACER}, {UI_ENUM, UICFG_THEME, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_LANGUAGE}}}, {UI_SPACER}, {UI_ENUM, UICFG_LANG,  NULL,{.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_SCROLLING}}}, {UI_SPACER}, {UI_ENUM, UICFG_SCROLL, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_MENUTYPE}}}, {UI_SPACER}, {UI_ENUM, UICFG_MENU, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_DEFDEVICE}}}, {UI_SPACER}, {UI_ENUM, UICFG_DEFDEVICE, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_AUTOSORT}}}, {UI_SPACER}, {UI_BOOL, UICFG_AUTOSORT, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"Bg. col", -1}}}, {UI_SPACER}, {UI_COLOUR, UICFG_BGCOL, NULL, {.colourvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Txt. col", -1}}}, {UI_SPACER}, {UI_COLOUR, UICFG_TXTCOL, NULL, {.colourvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"Exit to", -1}}}, {UI_SPACER}, {UI_ENUM, UICFG_EXITTO, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"", _STR_USEHDD}}}, {UI_SPACER}, {UI_BOOL, UICFG_USEHDD, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_AUTOSTARTHDD}}}, {UI_SPACER}, {UI_BOOL, UICFG_AUTOSTARTHDD, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_OK}, {UI_SPACER}, {UI_BUTTON, UICFG_SAVE, NULL, {.label = {"", _STR_SAVE_CHANGES}}}, 
	
	{UI_SPLITTER},
	
	// end of dialogue
	{UI_TERMINATOR}
};

// Renders the IP dialog, stores the values if result is ok
void showIPConfig() {
	size_t i;
	// upload current values
	for (i = 0; i < 4; ++i) {
		diaSetInt(diaIPConfig, 2 + i, ps2_ip[i]);
		diaSetInt(diaIPConfig, 6 + i, ps2_netmask[i]);
		diaSetInt(diaIPConfig, 10 + i, ps2_gateway[i]);
		diaSetInt(diaIPConfig, 14 + i, pc_ip[i]);
	}
	
	diaSetInt(diaIPConfig, 18, gPCPort);
	diaSetString(diaIPConfig, 19, gPCShareName);
	diaSetInt(diaIPConfig, 20, gNetAutostart);
	
	// show dialog
	if (diaExecuteDialog(diaIPConfig)) {
		// sanitize the values
		// Ok pressed, store values
		for (i = 0; i < 4; ++i) {
			diaGetInt(diaIPConfig, 2 + i, &ps2_ip[i]);
			diaGetInt(diaIPConfig, 6 + i, &ps2_netmask[i]);
			diaGetInt(diaIPConfig, 10 + i, &ps2_gateway[i]);
			diaGetInt(diaIPConfig, 14 + i, &pc_ip[i]);
		}
		
		diaGetInt(diaIPConfig, 18, &gPCPort);
		diaGetString(diaIPConfig, 19, gPCShareName);
		diaGetInt(diaIPConfig, 20, &gNetAutostart);
		
		// has to be non-empty
		if (strlen(gPCShareName) == 0) {
			strncpy(gPCShareName, "PS2SMB", 32);
		}
		
		gIPConfigChanged = 1;
	}
}

int getImageCompatMask(int id, const char* image, int ntype) {
	// hdd has an exception
	if (ntype == HDD_MODE) {
		if (!hddfound)
			return 0;
		// read from the game's description
		return hddGameList->games[id - 1].ops2l_compat_flags;
	}
	
	char gkey[255];
	unsigned int modemask;
	
	snprintf(gkey, 255, "%s_%d", image, ntype);
	
	if (!getConfigInt(&gConfig, gkey, &modemask))
		modemask = 0;
	
	return modemask;
}

void setImageCompatMask(int id, const char* image, int ntype, int mask) {
	if (ntype == HDD_MODE) {
		if (!hddfound)
			return;
		
		// write to the game's description
		hddGameList->games[id - 1].ops2l_compat_flags = (unsigned char)mask;
		
		// !!!! TESTED by jimmi on his HDD: should be safe to go... but use at your own risk
		hddSetHDLGameInfo(id - 1, &hddGameList->games[id - 1]);
		
		
		return;
	}
	
	char gkey[255];
	
	snprintf(gkey, 255, "%s_%d", image, ntype);
	
	setConfigInt(&gConfig, gkey, mask);
}

char *getImageGameID(const char* image) {
	char gkey[255];
	char *gid;
	
	snprintf(gkey, 255, "gameid_%s", image);
	
	if (!getConfigStr(&gConfig, gkey, &gid))
		gid = "";
	
	return gid;
}

// dst has to have 5 bytes space
void getImageGameIDBin(const char* image, void* dst) {
	char gkey[255];
	char *gid;
	
	snprintf(gkey, 255, "gameid_%s", image);
	
	if (!getConfigStr(&gConfig, gkey, &gid))
		gid = "";
	
	// convert from hex to binary
	memset(dst, 0, 5);
	char* cdst = dst;
	int p = 0;
	while (*gid && p < 10) {
		int dv = -1;
		
		while (dv < 0 && *gid) // skip spaces, etc
			dv = fromHex(*(gid++));
		
		if (dv < 0)
			break;
		
		*cdst = *cdst * 16 + dv;
		if ((++p & 1) == 0)
			cdst++;
	}
}

void setImageGameID(const char* image, const char *gid) {
	char gkey[255];
	
	snprintf(gkey, 255, "gameid_%s", image);
	setConfigStr(&gConfig, gkey, gid);
}

/** Shows compatibility settings window
* @param id the game id (index into the game list + 1)
* @param game the textual name if the game (readable form)
* @param prefix the string id of the game, unique, without spaces
* @param ntype the source type of the execution environment (HDD_MODE, USB_MODE, ETH_MODE)
* @return id of the dialogue button used to exit the dialogue, or either -1 or 0 on error or success without result
* @note returns COMPAT_TEST on testing request
*/
int showCompatConfig(int id, const char* game, const char* prefix, int ntype) {
	char gkey[255];
	char *hexid;
	
	if (id <= 0)
		return -1;
				
	if(ntype==HDD_MODE){		
		const char* dmaModes[] = {
			"MDMA 0",
			"MDMA 1",
			"MDMA 2",
			"UDMA 0",
			"UDMA 1",
			"UDMA 2",
			"UDMA 3",
			"UDMA 4",
			"UDMA 5",
			"UDMA 6",
			NULL
		};
		int dmamode=0;
		
		if(hddGameList->games[id - 1].dma_type == MDMA_MODE){
			dmamode = hddGameList->games[id - 1].dma_mode;
		}else if(hddGameList->games[id - 1].dma_type == UDMA_MODE){
			dmamode = hddGameList->games[id - 1].dma_mode;
			dmamode=dmamode+3;
		}
		else {
			dmamode=7; // defaulting to UDMA 4
		}
		
		diaSetEnum(diaCompatConfig, COMPAT_MODE_BASE + 6, dmaModes);
		diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + 6, dmamode);
		
	}else{
		const char* dmaModes[] = {
			NULL
		};
		diaSetEnum(diaCompatConfig, COMPAT_MODE_BASE + 6, dmaModes);
	}
	
	int modes = getImageCompatMask(id, prefix, ntype);
	
	int i;
	
	diaSetLabel(diaCompatConfig, 110, game);
	
	for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
		diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + i, (modes & (1 << i)) > 0 ? 1 : 0);
	}
	
	// Find out the current game ID
	hexid = getImageGameID(prefix);
	diaSetString(diaCompatConfig, COMPAT_GAMEID, hexid);
	
	// show dialog
	int result = 0;
	
	do {
		result = diaExecuteDialog(diaCompatConfig);
		
		if (result == COMPAT_LOADFROMDISC) {
			// Draw "please wait..." screen
			DrawBackground();
			DrawHint(_l(_STR_PLEASE_WAIT), -1);
			Flip();
			
			// load game ID, inject
			char discID[5];
			memset(discID, 0, 5);
			
			if (getDiscID(discID) >= 0) {
				// convert to hexadecimal string
				char nhexid[16];
				
				unsigned int n;
				// this may seem stupid but it seems snprintf handles %X as signed...
				for(n = 0; n < 5; ++n) {
					nhexid[n*3    ] = toHex(discID[n] >> 4);
					nhexid[n*3 + 1] = toHex(discID[n] & 0x0f);
					nhexid[n*3 + 2] = ' ';
				}
				
				nhexid[14] = '\0';
				
				diaSetString(diaCompatConfig, COMPAT_GAMEID, nhexid);
			} else {
				MsgBox(_l(_STR_ERROR_LOADING_ID));
			}
		}
	} while (result == COMPAT_LOADFROMDISC);
	
	if (result == COMPAT_REMOVE) {
		configRemoveKey(&gConfig, gkey);
		MsgBox(_l(_STR_REMOVED_ALL_SETTINGS));
	} else if (result > 0) { // okay pressed or other button
		modes = 0;
		for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
			int mdpart;
			diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + i, &mdpart);
			modes |= (mdpart ? 1 : 0) << i;
		}
		
		if(ntype==HDD_MODE){
			int dmamode=0;
			
			diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + 6, &dmamode);
			
			if(dmamode<3){
				hddGameList->games[id - 1].dma_type = MDMA_MODE;
				hddGameList->games[id - 1].dma_mode = dmamode;

			}else{
				hddGameList->games[id - 1].dma_type = UDMA_MODE;
				hddGameList->games[id - 1].dma_mode = (dmamode-3);
			}
		}
		
		setImageCompatMask(id, prefix, ntype, modes);
		
		diaGetString(diaCompatConfig, COMPAT_GAMEID, hexid);
		setImageGameID(prefix, hexid);
		
		if (result == COMPAT_SAVE) // write the config
			storeConfig();
	}
	
	return result;
}

void showUIConfig() {
	// configure the enumerations
	const char* scrollSpeeds[] = {
		_l(_STR_SLOW),
		_l(_STR_MEDIUM),
		_l(_STR_FAST),
		NULL
	};
	
	// Theme list update
	ListDir("mass:USBLD/");
	
	const char* menuTypes[] = {
		_l(_STR_STATIC),
		_l(_STR_DYNAMIC),
		NULL
	};
	
	const char* exitTypes[] = {
		"Browser",
		"mc0:/BOOT/BOOT.ELF",
		"mc0:/APPS/BOOT.ELF",
		NULL
	};

	const char* deviceNames[] = {
		_l(_STR_USB_GAMES),
		_l(_STR_NET_GAMES),
		_l(_STR_HDD_GAMES),
		NULL
	};


	// search for the theme index in the current theme list
	size_t i;
	size_t themeid = 0;
	for(i = 0; i < max_theme_dir; ++i)  {
		if (strcmp(theme_dir[i], theme) == 0) {
			themeid = i;
			break;
		}
	}
	
	diaSetEnum(diaUIConfig, UICFG_SCROLL, scrollSpeeds);
	diaSetEnum(diaUIConfig, UICFG_THEME, (const char **)theme_dir);
	diaSetEnum(diaUIConfig, UICFG_LANG, getLanguageList());
	diaSetEnum(diaUIConfig, UICFG_MENU, menuTypes);
	diaSetEnum(diaUIConfig, UICFG_EXITTO, exitTypes);
	diaSetEnum(diaUIConfig, UICFG_DEFDEVICE, deviceNames);
	
	// and the current values
	diaSetInt(diaUIConfig, UICFG_SCROLL, scroll_speed);
	diaSetInt(diaUIConfig, UICFG_THEME, themeid);
	diaSetInt(diaUIConfig, UICFG_LANG, gLanguageID);
	diaSetInt(diaUIConfig, UICFG_MENU, dynamic_menu);
	diaSetColour(diaUIConfig, UICFG_BGCOL, default_bg_color);
	diaSetColour(diaUIConfig, UICFG_TXTCOL, default_text_color);
	diaSetInt(diaUIConfig, UICFG_EXITTO, exit_mode);
	diaSetInt(diaUIConfig, UICFG_DEFDEVICE, default_device);
	diaSetInt(diaUIConfig, UICFG_USEHDD, gUseHdd);
	diaSetInt(diaUIConfig, UICFG_AUTOSTARTHDD, gHddAutostart);
	diaSetInt(diaUIConfig, UICFG_AUTOSORT, gAutosort);
	
	int ret = diaExecuteDialog(diaUIConfig);
	if (ret) {
		// transfer values back into config
		diaGetInt(diaUIConfig, UICFG_SCROLL, &scroll_speed);
		diaGetInt(diaUIConfig, UICFG_THEME, &themeid);
		diaGetInt(diaUIConfig, UICFG_LANG, &gLanguageID);
		diaGetInt(diaUIConfig, UICFG_MENU, &dynamic_menu);
		diaGetColour(diaUIConfig, UICFG_BGCOL, default_bg_color);
		diaGetColour(diaUIConfig, UICFG_TXTCOL, default_text_color);
		diaGetInt(diaUIConfig, UICFG_EXITTO, &exit_mode);
		diaGetInt(diaUIConfig, UICFG_DEFDEVICE, &default_device);
		diaGetInt(diaUIConfig, UICFG_USEHDD, &gUseHdd);
		diaGetInt(diaUIConfig, UICFG_AUTOSTARTHDD, &gHddAutostart);
		diaGetInt(diaUIConfig, UICFG_AUTOSORT, &gAutosort);
		
		// update the value interpretation
		setLanguage(gLanguageID);
		UpdateScrollSpeed();
		SetMenuDynamic(dynamic_menu);
		LoadTheme(themeid);
		infotxt = _l(_STR_WELCOME);
		
		if (ret == UICFG_SAVE)
			storeConfig();
	}
}


// --------------------- Configuration handling --------------------
int loadConfig(const char* fname, int clearFirst) {
	if (clearFirst)
		clearConfig(&gConfig);
	
	// fill the config from file
	int result = readConfig(&gConfig, fname);
	
	if (!result)
		return 0;
	
	gfxRestoreConfig();
	
	infotxt = _l(_STR_WELCOME);
	
	char *temp;
	
	// restore non-graphics settings as well
	if (getConfigStr(&gConfig, "pc_ip", &temp))
		sscanf(temp, "%d.%d.%d.%d", &pc_ip[0], &pc_ip[1], &pc_ip[2], &pc_ip[3]);
	
	getConfigInt(&gConfig, "pc_port", &gPCPort);
	getConfigInt(&gConfig, "net_auto", &gNetAutostart);
	getConfigInt(&gConfig, "use_hdd", &gUseHdd);
	getConfigInt(&gConfig, "autostart_hdd", &gHddAutostart);
	getConfigInt(&gConfig, "autosort", &gAutosort);
	
	if (getConfigStr(&gConfig, "pc_share", &temp))
		strncpy(gPCShareName, temp, 32);
	
	if (gPCPort < 0)
		gPCPort = 445;
	
	return 1;
}

int saveConfig(const char* fname) {
	gfxStoreConfig();
	
	if (gPCPort < 0)
		gPCPort = 445;
	
	char temp[255];
	
	sprintf(temp, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
	
	setConfigStr(&gConfig, "pc_ip", temp);
	setConfigInt(&gConfig, "pc_port", gPCPort);
	setConfigInt(&gConfig, "net_auto", gNetAutostart);
	setConfigInt(&gConfig, "use_hdd", gUseHdd);
	setConfigInt(&gConfig, "autostart_hdd", gHddAutostart);
	setConfigStr(&gConfig, "pc_share", gPCShareName);
	setConfigInt(&gConfig, "autosort", gAutosort);
	
	// Not writing the IP config, too dangerous to change it by accident...
	return writeConfig(&gConfig, fname);
}

int restoreConfig() {
	readIPConfig();

	// look for mc0 and mc1 first, then mass
	// the first to load succesfully will store the config location
	const char* configLocations[] = {
		"mc0:/SYS-CONF/usbld.cfg",
		"mc1:/SYS-CONF/usbld.cfg",
		"mass:/USBLD/usbld.cfg",
		NULL
	};
	
	unsigned int loc = 0;
	gConfigPath = NULL;
	const char *curPath;
	
	while ((curPath = configLocations[loc++]) != NULL) {
		if (loadConfig(curPath, 1) != 0) {
			gConfigPath = curPath;
			break;
		}
	}
	
	return gConfigPath == NULL ? 0 : 1;
}

int storeConfig() {
	// use the global path if found, else default to mc0
	const char *confPath = gConfigPath;

	if (gIPConfigChanged)
		writeIPConfig();
	
	if (!confPath) // default to SYS-CONF on mc0
		confPath = "mc0:/SYS-CONF/usbld.cfg";
	
	int result = saveConfig(confPath);
	
	if (result) {
		MsgBox(_l(_STR_SETTINGS_SAVED));
	} else {
		MsgBox(_l(_STR_ERROR_SAVING_SETTINGS));
	}
		
	return result;
}

// --------------------- Menu variables, handling & initialization --------------------
// Submenu items variant of the game list... keeped in sync with the game item list
struct TSubMenuList* usb_submenu = NULL;
struct TSubMenuList* eth_submenu = NULL;
struct TSubMenuList* hdd_submenu = NULL;

/// List of usb games
struct TMenuItem usb_games_item;
/// List of network games
struct TMenuItem eth_games_item;
/// List of hdd games
struct TMenuItem hdd_games_item;

/// scroll speed selector
struct TSubMenuList* speed_item = NULL;

/// menu type selector
struct TSubMenuList* menutype_item = NULL;

unsigned int scroll_speed_txt[3] = {_STR_SCROLL_SLOW, _STR_SCROLL_MEDIUM, _STR_SCROLL_FAST};
unsigned int menu_type_txt[2] = { _STR_MENU_DYNAMIC,  _STR_MENU_STATIC};

void ClearSubMenu(struct TSubMenuList** submenu, struct TMenuItem* mi) {
	DestroySubMenu(submenu);
	
	if ((submenu != &eth_submenu) || (eth_inited || gNetAutostart))
		AppendSubMenu(submenu, &disc_icon, "", -1, _STR_NO_ITEMS);
	else
		AppendSubMenu(submenu, &netconfig_icon, "", -1, _STR_START_NETWORK);

	mi->submenu = *submenu;
	mi->current = *submenu;
}

void RefreshGameList(TGame **list, int* max_games, const char* prefix, struct TSubMenuList** submenu, struct TMenuItem* mi) {
	int fd, size;
	char buffer[0x040];

	char path[64];
	snprintf(path, 64, "%sul.cfg", prefix);
	fd=fioOpen(path, O_RDONLY);
	
	if(fd<0) { // file could not be opened. reset menu
		free(*list);
		*list = NULL;
		ClearSubMenu(submenu, mi);
		return;
	}

	// only refresh if the list is empty
	if (*max_games==0) {
		DestroySubMenu(submenu);
		*submenu = NULL;
		mi->submenu = *submenu;
		mi->current = *submenu;

		size = fioLseek(fd, 0, SEEK_END);  
		fioLseek(fd, 0, SEEK_SET); 
		
		size_t count = size / 0x040;
		*max_games = count;
		
		*list = (TGame*)malloc(sizeof(TGame) * count);
		
		int id;
		for(id = 1; id <= count; ++id) {
			fioRead(fd, buffer, 0x40);
			
			TGame *g = &(*list)[id-1];
			
			// to ensure no leaks happen, we copy manually and pad the strings
			memcpy(g->Name, buffer, 32);
			g->Name[32] = '\0';
			memcpy(g->Image, &buffer[32], 16);
			g->Image[16] = '\0';
			memcpy(&g->parts, &buffer[47], 1);
			memcpy(&g->media, &buffer[48], 1);
			
			AppendSubMenu(submenu, &disc_icon, g->Name, id, -1);
		}
		
		if (gAutosort)
			SortSubMenu(submenu);
		
		mi->submenu = *submenu;
		mi->current = *submenu;
		mi->pagestart = *submenu;
	}

	fioClose(fd);
}

int RefreshHDDGameList() {
	DestroySubMenu(&hdd_submenu);
	int ret = hddGetHDLGamelist(&hddGameList);
	hdd_submenu = NULL;
	
	if (ret != 0)
		return -1;

	// iterate the games, create items for them
	int id;
	for (id = 1; id <= hddGameList->count; id++) {
		hdl_game_info_t *game = &hddGameList->games[id - 1];

		AppendSubMenu(&hdd_submenu, &disc_icon, game->name, id, -1);
	};

	if (gAutosort)
		SortSubMenu(&hdd_submenu);
	
	hdd_games_item.submenu = hdd_submenu;
	hdd_games_item.current = hdd_submenu;
	hdd_games_item.pagestart = hdd_submenu;

	return 0;
}


void FindUSBPartition(){
	int i, fd;
	char path[64];
	
	snprintf(USB_prefix,8,"mass0:");
	
	for(i=0;i<5;i++){
		snprintf(path, 64, "mass%d:/ul.cfg", i);
		fd=fioOpen(path, O_RDONLY);
		
		if(fd>=0) {
			snprintf(USB_prefix,8,"mass%d:",i);
			fioClose(fd);
			break;
		}
		fioClose(fd);
	}
	
	return;
}

void ClearUSBSubMenu() {
	ClearSubMenu(&usb_submenu, &usb_games_item);
}

void RefreshUSBGameList() {
	if(usbdelay<=100)  {
		usbdelay++;
		return;
	}
	  
	usbdelay = 0;
	RefreshGameList(&usbGameList, &usb_max_games, USB_prefix, &usb_submenu, &usb_games_item);
}

// --------------------- USB Menu item callbacks --------------------
void ExecUSBGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	padPortClose(0, 0);
	padPortClose(1, 0);
	padReset();
	
	// find the compat mask
	int cmask = getImageCompatMask(id, usbGameList[id - 1].Image, USB_MODE);
	
	char* gid[5];
	getImageGameIDBin(usbGameList[id - 1].Image, gid);
	
	LaunchGame(&usbGameList[id - 1], USB_MODE, cmask, gid);
}

void AltExecUSBGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	if (showCompatConfig(id, usbGameList[id - 1].Name, usbGameList[id - 1].Image, USB_MODE) == COMPAT_TEST)
		ExecUSBGameSelection(self, id);
}

void ClearETHSubMenu() {
	ClearSubMenu(&eth_submenu, &eth_games_item);
}

void RefreshETHGameList() {
	if(ethdelay<=100)  {
		ethdelay++;
		return;
	}
	  
	ethdelay = 0;
	RefreshGameList(&ethGameList, &eth_max_games, "smb0:\\", &eth_submenu, &eth_games_item);
}

// --------------------- Network Menu item callbacks --------------------
// Forward decl.
void LoadNetworkModules(void);

void StartNetwork() {
	if (0) // left here if we find a way to let it work without issues (see bug no. 16)
		Start_LoadNetworkModules_Thread();
	else
		LoadNetworkModules();
}

void ExecETHGameSelection(struct TMenuItem* self, int id) {
	if (id == -1) {
		if (!eth_inited) {
			StartNetwork();
			eth_inited = 1;
			ClearETHSubMenu();
		}
		return;
	}
	
	padPortClose(0, 0);
	padPortClose(1, 0);
	padReset();
	
	int cmask = getImageCompatMask(id, ethGameList[id - 1].Image, ETH_MODE);
	
	char* gid[5];
	getImageGameIDBin(ethGameList[id - 1].Image, gid);
	
	LaunchGame(&ethGameList[id - 1], ETH_MODE, cmask, gid);
}

void AltExecETHGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	if (showCompatConfig(id, ethGameList[id - 1].Name,  ethGameList[id - 1].Image, ETH_MODE) == COMPAT_TEST)
		ExecETHGameSelection(self, id);
}


// --------------------- HDD Menu item callbacks --------------------
void LoadHddModules(void);

void StartHdd() {
	// For Testing HDD
	LoadHddModules();

	hddfound = !hddCheck();
}

void ExecHDDGameSelection(struct TMenuItem* self, int id) {
	if (id == -1) {
		if (!hdd_inited) {
			StartHdd();
			hdd_inited = 1;
			
			if (hddfound) {
				RefreshHDDGameList();
			} else {
				DestroySubMenu(&hdd_submenu);
				hdd_games_item.submenu = NULL;
				hdd_games_item.current = NULL;
			}
		}
		return;
	}
	
	padPortClose(0, 0);
	padPortClose(1, 0);
	padReset();
	
	// hash to ensure no spaces are present
	char imgnam[12];
	unsigned int crc = crc32(hddGameList->games[id - 1].name);
	snprintf(imgnam, 12, "%X", crc);
	
	char* gid[5];
	getImageGameIDBin(imgnam, gid);
	
	LaunchHDDGame(&hddGameList->games[id - 1], gid);
}

void AltExecHDDGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	// Example Game id storage impl (maybe hdd_id%d would be better?):
	char imgnam[12];
	unsigned int crc = crc32(hddGameList->games[id - 1].name);
	snprintf(imgnam, 12, "%X", crc);
	
	if (showCompatConfig(id, hddGameList->games[id - 1].name,  imgnam, HDD_MODE) == COMPAT_TEST)
		ExecHDDGameSelection(self, id);
}


// --------------------- Exit/Settings Menu item callbacks --------------------
/// menu item selection callbacks
void ExecExit(struct TMenuItem* self, int vorder) {

	if(exit_mode==0){
		__asm__ __volatile__(
			"	li $3, 0x04;"
			"	syscall;"
			"	nop;"
		);
	}else if(exit_mode==1){
		ExecElf("mc0:/BOOT/BOOT.ELF");
		MsgBox("Error launching mc0:/BOOT/BOOT.ELF");
	}else if(exit_mode==2){
		ExecElf("mc0:/APPS/BOOT.ELF");
		MsgBox("Error launching mc0:/APPS/BOOT.ELF");
	}
}

void ChangeScrollSpeed() {
	scroll_speed = (scroll_speed + 1) % 3;
	speed_item->item.text_id = scroll_speed_txt[scroll_speed];
	
	UpdateScrollSpeed();
}

void ChangeMenuType() {
	SwapMenu();
	
	menutype_item->item.text_id = menu_type_txt[dynamic_menu ? 0 : 1];
}

void ExecSettings(struct TMenuItem* self, int id) {
	if (id == 1) {
		showUIConfig();
	} else if (id == 3) {
		// ipconfig
		showIPConfig();
	} else if (id == 7) {
		storeConfig();
	}
}

// --------------------- Menu initialization --------------------
struct TSubMenuList *settings_submenu = NULL;

struct TMenuItem exit_item = {
	&exit_icon, "Exit", _STR_EXIT, NULL, NULL, NULL, NULL, &ExecExit, NULL, NULL
};

struct TMenuItem settings_item = {
	&config_icon, "Settings", _STR_SETTINGS, NULL, NULL, NULL, NULL, &ExecSettings, NULL, NULL
};

struct TMenuItem usb_games_item  = {
	&usb_icon, "USB Games", _STR_USB_GAMES, NULL, NULL, NULL, NULL, &ExecUSBGameSelection, &RefreshUSBGameList, &AltExecUSBGameSelection
};

struct TMenuItem hdd_games_item = {
	&games_icon, "HDD Games", _STR_HDD_GAMES, NULL, NULL, NULL, NULL, &ExecHDDGameSelection, NULL, &AltExecHDDGameSelection
};

struct TMenuItem eth_games_item = {
	&network_icon, "Network Games", _STR_NET_GAMES, NULL, NULL, NULL, NULL, &ExecETHGameSelection, &RefreshETHGameList, &AltExecETHGameSelection
};

struct TMenuItem apps_item = {
	&apps_icon, "Apps", _STR_APPS, NULL, NULL, NULL, NULL
};


void InitMenuItems() {
	ClearUSBSubMenu();
	ClearETHSubMenu();
	
	// initialize the menu
	AppendSubMenu(&settings_submenu, &theme_icon, "Theme", 1, _STR_SETTINGS);
	AppendSubMenu(&settings_submenu, &netconfig_icon, "Network config", 3, _STR_IPCONFIG);
	AppendSubMenu(&settings_submenu, &save_icon, "Save Changes", 7, _STR_SAVE_CHANGES);
	
	settings_item.submenu = settings_submenu;
	
	// add all menu items
	menu = NULL;
	AppendMenuItem(&exit_item);
	AppendMenuItem(&settings_item);
	AppendMenuItem(&usb_games_item);
	
	// show hdd icon if found
	if (gUseHdd) {
		// hdd items population
		if (gHddAutostart && hddfound) {
			RefreshHDDGameList();
		} else {
			// clear the menu item, 
			// add start item, insert into menu
			DestroySubMenu(&hdd_submenu);
			
			AppendSubMenu(&hdd_submenu, &disc_icon, "", -1, _STR_STARTHDD);
			
			hdd_games_item.submenu = hdd_submenu;
			hdd_games_item.current = hdd_submenu;
			
		}
		
		AppendMenuItem(&hdd_games_item);
	}
	
	AppendMenuItem(&eth_games_item);
	
	// Commenting out for now, not used:
	// AppendMenuItem(&apps_item);
}

// main initialization function
void init() {
	Reset();

	InitGFX();

	theme_dir[0] = NULL;
	usb_max_games = 0;
	eth_max_games = 0;


	ps2_ip[0] = 192; ps2_ip[1] = 168; ps2_ip[2] =  0; ps2_ip[3] =  10;
	ps2_netmask[0] = 255; ps2_netmask[1] = 255; ps2_netmask[2] =  255; ps2_netmask[3] =  0;
	ps2_gateway[0] = 192; ps2_gateway[1] = 168; ps2_gateway[2] = 0; ps2_gateway[3] = 1;
	pc_ip[0] = 192;pc_ip[1] = 168; pc_ip[2] = 0; pc_ip[3] = 2;
	
	// SMB port on pc
	gPCPort = 445;
	// loading progress of the network
	gNetworkStartup = 0;
	// autostart network, hdd?
	gNetAutostart = 0;
	gUseHdd = 0;
	gHddAutostart = 0;
	// no change to the ipconfig was done
	gIPConfigChanged = 0;
	// Default PC share name
	strncpy(gPCShareName, "PS2SMB", 32);
	//Default exit mode
	exit_mode=0;
	// default device USB
	default_device = 0;
	// 1 if the harddrive is found
	hddfound = 0;
	hdd_inited = 0;
	// autosort defaults to zero
	gAutosort = 0;
	
	// default to english
	gLanguageID = _LANG_ID_ENGLISH;
	setLanguage(gLanguageID);

	gConfig.head = NULL;
	gConfig.tail = NULL;
	
	SifInitRpc(0);
	
	SifLoadModule("rom0:SIO2MAN",0,0);
	SifLoadModule("rom0:MCMAN",0,0);
	SifLoadModule("rom0:MCSERV",0,0);
	SifLoadModule("rom0:PADMAN",0,0);
	mcInit(MC_TYPE_MC);

	int ret, id;
		
	LoadUSBD();
	id=SifExecModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &ret);
	
	delay(3);

	StartPad();

	// try to restore config
	// if successful, it will remember the config location for further writes
	restoreConfig();
	
	// HDD startup
	if (gUseHdd && gHddAutostart) {
		hdd_inited = 1;
		StartHdd();
	}
	
	// initialize menu
	InitMenu();
	
	delay(1);
	
	/// Init custom menu items
	InitMenuItems();
	
	// first column is usb games
	switch (default_device) {
		case 1:	MenuSetSelectedItem(&eth_games_item);
			break;
		case 2:	MenuSetSelectedItem(&hdd_games_item);
			break;
		case 0:
		default:
			MenuSetSelectedItem(&usb_games_item);
	}
	
	usb_max_games=0;
	eth_max_games=0;	
	usbdelay=0;
	ethdelay=0;
	frame=0;
	inactiveFrames = 0;
	
	usbGameList = NULL;
	ethGameList = NULL;
	

	// these seem to matter. Without them, something tends to crush into itself
	delay(1);
}

int netLoadInfo() {
	char txt[64];
	
	// display network status in lower left corner
	if (gNetworkStartup > 0) {
		snprintf(txt, 64, _l(_STR_NETWORK_LOADING), gNetworkStartup);
		DrawHint(txt, -1);

		return 1;
	}

	return 0;
}

void netLoadDisplay() {
	DrawBackground();
	netLoadInfo();
	Flip();
}

void LoadNetworkModules() {
	
	int ret, id;
		
	set_ipconfig();

	if (!gDev9_loaded) {
		gNetworkStartup = 5;
		netLoadDisplay();

    	id=SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
		if ((id < 0) || ret) {
			gNetworkStartup = -1;
			return;
		}

		gDev9_loaded = 1;
	}
	
	gNetworkStartup = 4;
	netLoadDisplay();
	
	id=SifExecModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		return;
	}
	gNetworkStartup = 3;
	netLoadDisplay();
	
	id=SifExecModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		return;
	}
	
	gNetworkStartup = 2;
	netLoadDisplay();
	
	id=SifExecModuleBuffer(&smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig, &ret);	
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		return;
	}
	
	gNetworkStartup = 1;
	netLoadDisplay();
	
	id=SifExecModuleBuffer(&smbman_irx, size_smbman_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		return;
	}

	gNetworkStartup = 0; // ok, all loaded
}

void LoadHddModules(void)
{	
	int ret, id;
	static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";

	if (!gDev9_loaded) {
		gHddStartup = 3;

    	id=SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
		if ((id < 0) || ret) {
			gHddStartup = -1;
			return;
		}

		gDev9_loaded = 1;
	}

	gHddStartup = 2;

	id=SifExecModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gHddStartup = -1;
		return;
	}

	gHddStartup = 1;

	id=SifExecModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg, &ret);		
	if ((id < 0) || ret) {
		gHddStartup = -1;
		return;
	}

	gHddStartup = 0;
}

// --------------------- Main --------------------
int main(void)
{
	init();
	
	Intro();
	
	// these seem to matter. Without them, something tends to crush into itself
	delay(1);
	
	FindUSBPartition();

	ReadPad();
	
	// If automatic network startup is selected, start it now
	if ((!GetKeyOn(KEY_L1)) && (gNetAutostart != 0)) {
		StartNetwork();
		eth_inited = 1;
	}

	TextColor(0x80,0x80,0x80,0x80);
	
	while(1)
	{
		if (ReadPad())
			inactiveFrames = 0;
		else
			inactiveFrames++;

		DrawScreen();
				
		if(GetKey(KEY_LEFT)){
			MenuPrevH();
		} else if(GetKey(KEY_RIGHT)){
			MenuNextH();
		} else if(GetKey(KEY_UP)) {
			MenuPrevV();
		} else if(GetKey(KEY_DOWN)){
			MenuNextV();
		} else if(GetKey(KEY_L1)) {
			int i;
			for (i = 0; i < STATIC_PAGE_SIZE; ++i)
				MenuPrevV();
		} else if(GetKey(KEY_R1)){
			int i;
			for (i = 0; i < STATIC_PAGE_SIZE; ++i)
				MenuNextV();
		}
		
		// home
		if (GetKeyOn(KEY_L2)) {
			struct TMenuItem* cur = MenuGetCurrent();
			
			if ((cur == &usb_games_item) || 
			    (cur == &eth_games_item) ||
			    (cur == &hdd_games_item)) {
				cur->current = cur->submenu;
			}
		}
		
		// end
		if (GetKeyOn(KEY_R2)) {
			struct TMenuItem* cur = MenuGetCurrent();
			
			if ((cur == &usb_games_item) || 
			    (cur == &eth_games_item) ||
			    (cur == &hdd_games_item)) {
				if (!cur->current)
					cur->current = cur->submenu;
				
				if (cur->current)
					while (cur->current->next)
						cur->current = cur->current->next;
			}
		}
		
		if(GetKeyOn(KEY_CROSS)){
			// handle via callback in the menuitem
			MenuItemExecute();
		} else if(GetKeyOn(KEY_TRIANGLE)){
			// handle via callback in the menuitem
			MenuItemAltExecute();
		}
		
		// sort the list on SELECT press
		if (GetKeyOn(KEY_SELECT)) {
			struct TMenuItem* cur = MenuGetCurrent();
			
			if ((cur == &usb_games_item) ||
 
			    (cur == &eth_games_item) ||
			    (cur == &hdd_games_item)) {
				SortSubMenu(&cur->submenu);
				cur->current = cur->submenu;
			}
		}
		

		
		if (!netLoadInfo()) {
			// see the inactivity
			if (inactiveFrames >= HINT_FRAMES_MIN) {
				struct TMenuItem* cur = MenuGetCurrent();
				// show hint (depends on the current menu item)

				if ((cur == &usb_games_item) && (usb_max_games > 0)) 
					DrawHint(_l(_STR_COMPAT_SETTINGS), KEY_TRIANGLE);
				else if ((cur == &eth_games_item) && (eth_max_games > 0))
					DrawHint(_l(_STR_COMPAT_SETTINGS), KEY_TRIANGLE);
				else if (cur == &settings_item)
					DrawHint(_l(_STR_SETTINGS), KEY_CROSS);
			}
		}

		Flip();
		
		RefreshSubMenu();
		
		// if error starting network happened, message out and disable
		if (gNetworkStartup < 0) {
			MsgBox(_l(_STR_NETWORK_STARTUP_ERROR));
			gNetworkStartup = 0;
		}
	}

	return 0;
}

