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

extern void *ps2ip_irx;
extern int size_ps2ip_irx;

extern void *ps2smap_irx;
extern int size_ps2smap_irx;

extern void *smbman_irx;
extern int size_smbman_irx;

// language id
int gLanguageID = 0;

int eth_inited = 0;

// frames since button was pressed
int inactiveFrames;

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
#define COMPAT_MODE_COUNT 5
struct UIItem diaCompatConfig[] = {
	{UI_LABEL, 110, NULL, {.label = {"<Game Label>", -1}}},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 111, NULL, {.label = {"", _STR_COMPAT_SETTINGS}}}, {UI_BREAK}, {UI_BREAK}, {UI_BREAK},

	{UI_LABEL, 0, NULL, {.label = {"Mode 1", -1}}}, {UI_SPACER}, {UI_BOOL, 10, "Load alt. core", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 2", -1}}}, {UI_SPACER}, {UI_BOOL, 11, "Alternative data read method", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 3", -1}}}, {UI_SPACER}, {UI_BOOL, 12, "Unhook Syscalls", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 4", -1}}}, {UI_SPACER}, {UI_BOOL, 13, "0 PSS mode", {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Mode 5", -1}}}, {UI_SPACER}, {UI_BOOL, 14, "Disable DVD-DL", {.intvalue = {0, 0}}}, {UI_BREAK},
	
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

#define UICFG_SAVE 114
struct UIItem diaUIConfig[] = {
	{UI_LABEL, 111, NULL, {.label = {"", _STR_SETTINGS}}},
	{UI_SPLITTER},

	{UI_LABEL, 0, NULL, {.label = {"", _STR_THEME}}}, {UI_SPACER}, {UI_ENUM, UICFG_THEME, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_LANGUAGE}}}, {UI_SPACER}, {UI_ENUM, UICFG_LANG,  NULL,{.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_SCROLLING}}}, {UI_SPACER}, {UI_ENUM, UICFG_SCROLL, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_MENUTYPE}}}, {UI_SPACER}, {UI_ENUM, UICFG_MENU, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"Bg. col", -1}}}, {UI_SPACER}, {UI_COLOUR, UICFG_BGCOL, NULL, {.colourvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Txt. col", -1}}}, {UI_SPACER}, {UI_COLOUR, UICFG_TXTCOL, NULL, {.colourvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"Exit to", -1}}}, {UI_SPACER}, {UI_ENUM, UICFG_EXITTO, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
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

int getImageCompatMask(const char* image, int ntype) {
	char gkey[255];
	unsigned int modemask;
	
	snprintf(gkey, 255, "%s_%d", image, ntype);
	
	if (!getConfigInt(&gConfig, gkey, &modemask))
		modemask = 0;
	
	return modemask;
}

void setImageCompatMask(const char* image, int ntype, int mask) {
	char gkey[255];
	
	snprintf(gkey, 255, "%s_%d", image, ntype);
	
	setConfigInt(&gConfig, gkey, mask);
}

// returns COMPAT_TEST on testing request
int showCompatConfig(const char* game, const char* prefix, int ntype) {
	char gkey[255];
	
	int modes = getImageCompatMask(prefix, ntype);
	
	int i;
	
	diaSetLabel(diaCompatConfig, 110, game);
	
	for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
		diaSetInt(diaCompatConfig, i+10, (modes & (1 << i)) > 0 ? 1 : 0);
	}
	
	// show dialog
	int result = diaExecuteDialog(diaCompatConfig);
	
	if (result == COMPAT_REMOVE) {
		configRemoveKey(&gConfig, gkey);
		MsgBox(_l(_STR_REMOVED_ALL_SETTINGS));
	} else if (result > 0) { // okay pressed or other button
		modes = 0;
		for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
			int mdpart;
			diaGetInt(diaCompatConfig, i+10, &mdpart);
			modes |= (mdpart ? 1 : 0) << i;
		}
		
		setImageCompatMask(prefix, ntype, modes);
		
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
	
	// and the current values
	diaSetInt(diaUIConfig, UICFG_SCROLL, scroll_speed);
	diaSetInt(diaUIConfig, UICFG_THEME, themeid);
	diaSetInt(diaUIConfig, UICFG_LANG, gLanguageID);
	diaSetInt(diaUIConfig, UICFG_MENU, dynamic_menu);
	diaSetColour(diaUIConfig, UICFG_BGCOL, default_bg_color);
	diaSetColour(diaUIConfig, UICFG_TXTCOL, default_text_color);
	diaSetInt(diaUIConfig, UICFG_EXITTO, exit_mode);
	
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
	setConfigStr(&gConfig, "pc_share", gPCShareName);

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

/// List of usb games
struct TMenuItem usb_games_item;
/// List of network games
struct TMenuItem eth_games_item;

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
		
		mi->submenu = *submenu;
		mi->current = *submenu;
	}

	fioClose(fd);
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
	int cmask = getImageCompatMask(usbGameList[id - 1].Image, USB_MODE);
	
	LaunchGame(&usbGameList[id - 1], USB_MODE, cmask);
}

void AltExecUSBGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	if (showCompatConfig(usbGameList[id - 1].Name, usbGameList[id - 1].Image, USB_MODE) == COMPAT_TEST)
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
void ExecETHGameSelection(struct TMenuItem* self, int id) {
	if (id == -1) {
		if (!eth_inited) {
			Start_LoadNetworkModules_Thread();
			eth_inited = 1;
			ClearETHSubMenu();
		}
		return;
	}
	
	padPortClose(0, 0);
	padPortClose(1, 0);
	padReset();
	
	int cmask = getImageCompatMask(ethGameList[id - 1].Image, ETH_MODE);
	
	LaunchGame(&ethGameList[id - 1], ETH_MODE, cmask);
}

void AltExecETHGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	if (showCompatConfig(ethGameList[id - 1].Name,  ethGameList[id - 1].Image, ETH_MODE) == COMPAT_TEST)
		ExecETHGameSelection(self, id);
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
	&exit_icon, "Exit", _STR_EXIT, NULL, NULL, NULL, &ExecExit, NULL, NULL
};

struct TMenuItem settings_item = {
	&config_icon, "Settings", _STR_SETTINGS, NULL, NULL, NULL, &ExecSettings, NULL, NULL
};

struct TMenuItem usb_games_item  = {
	&usb_icon, "USB Games", _STR_USB_GAMES, NULL, NULL, NULL, &ExecUSBGameSelection, &RefreshUSBGameList, &AltExecUSBGameSelection
};

struct TMenuItem hdd_games_item = {
	&games_icon, "HDD Games", _STR_HDD_GAMES, NULL, NULL, NULL
};

struct TMenuItem eth_games_item = {
	&network_icon, "Network Games", _STR_NET_GAMES, NULL, NULL, NULL, &ExecETHGameSelection, &RefreshETHGameList, &AltExecETHGameSelection
};

struct TMenuItem apps_item = {
	&apps_icon, "Apps", _STR_APPS, NULL, NULL, NULL
};


void InitMenuItems() {
	ClearUSBSubMenu();
	ClearETHSubMenu();
	
	// initialize the menu
	AppendSubMenu(&settings_submenu, &theme_icon, "Theme", 1, _STR_THEME);
	AppendSubMenu(&settings_submenu, &netconfig_icon, "Network config", 3, _STR_IPCONFIG);
	AppendSubMenu(&settings_submenu, &save_icon, "Save Changes", 7, _STR_SAVE_CHANGES);
	
	settings_item.submenu = settings_submenu;
	
	// add all menu items
	menu = NULL;
	AppendMenuItem(&exit_item);
	AppendMenuItem(&settings_item);
	AppendMenuItem(&usb_games_item);
	AppendMenuItem(&hdd_games_item);
	AppendMenuItem(&eth_games_item);
	AppendMenuItem(&apps_item);
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
	// autostart network?
	gNetAutostart = 0;
	// no change to the ipconfig was done
	gIPConfigChanged = 0;
	// Default PC share name
	strncpy(gPCShareName, "PS2SMB", 32);
	//Default exit mode
	exit_mode=0;
	
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

	// try to restore config
	// if successful, it will remember the config location for further writes
	restoreConfig();
	
	InitMenu();
	
	delay(1);
	
	/// Init custom menu items
	InitMenuItems();
	
	// first column is usb games
	MenuSetSelectedItem(&usb_games_item);
	
	StartPad();

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

// --------------------- Main --------------------
int main(void)
{
	init();
	
	Intro();
	
	// these seem to matter. Without them, something tends to crush into itself
	delay(1);
	
	FindUSBPartition();

	if (gNetAutostart != 0) {
		Start_LoadNetworkModules_Thread();
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
				
		// L1 - page up/down modifier (5 instead of 1 item skip)
		int pgmod = GetKeyPressed(KEY_L1);
		
		if(GetKey(KEY_LEFT)){
			MenuPrevH();
		}else if(GetKey(KEY_RIGHT)){
			MenuNextH();
		}else if(GetKey(KEY_UP)){
			int i;
			for (i = 0; i < 4*(pgmod ? 1 : 0) + 1; ++i)
				MenuPrevV();
		}else if(GetKey(KEY_DOWN)){
			int i;
			for (i = 0; i < 4*(pgmod ? 1 : 0) + 1; ++i)
				MenuNextV();
		}
		
		// home
		if (GetKeyOn(KEY_L2)) {
			struct TMenuItem* cur = MenuGetCurrent();
			
			if ((cur == &usb_games_item) || (cur == &eth_games_item)) {
				cur->current = cur->submenu;
			}
		}
		
		// end
		if (GetKeyOn(KEY_R2)) {
			struct TMenuItem* cur = MenuGetCurrent();
			
			if ((cur == &usb_games_item) || (cur == &eth_games_item)) {
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
			
			if ((cur == &usb_games_item) || (cur == &eth_games_item)) {
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

