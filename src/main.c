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
// --------------------------- Dialogs ---------------------------
// Dialog definition for IP configuration
struct UIItem diaIPConfig[] = {
	{UI_LABEL, 0, {.label = {"", _STR_IPCONFIG}}},
	
	{UI_SPLITTER},

	// ---- IP address ----
	{UI_LABEL, 0, {.label = {"- PS2 -", -1}}}, {UI_BREAK},
	
	{UI_LABEL, 0, {.label = {"IP", -1}}}, {UI_SPACER}, 
	
	{UI_INT, 2, {.intvalue = {192, 192, 0, 255}}}, {UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 3, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 4, {.intvalue = {0, 0, 0, 255}}}, {UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 5, {.intvalue = {10, 10, 0, 255}}},

	{UI_BREAK},

	//  ---- Netmask ----
	{UI_LABEL, 0, {.label = {"MASK", -1}}},
	{UI_SPACER}, 
	
	{UI_INT, 6, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 7, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 8, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 9, {.intvalue = {0, 0, 0, 255}}},
	
	{UI_BREAK},
	
	//  ---- Gateway ----
	{UI_LABEL, 0, {.label = {"GW", -1}}},
	{UI_SPACER}, 
	
	{UI_INT, 10, {.intvalue = {192, 192, 0, 255}}}, 	{UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 11, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 12, {.intvalue = {0, 0, 0, 255}}}, 	{UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 13, {.intvalue = {1, 1, 0, 255}}},
	
	{UI_SPLITTER},
	
	//  ---- PC ----
	{UI_LABEL, 0, {.label = {"- PC -", -1}}},
	{UI_BREAK},
	
	{UI_LABEL, 0, {.label = {"IP", -1}}},
	{UI_SPACER}, 
	
	{UI_INT, 14, {.intvalue = {192, 192, 0, 255}}},	{UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 15, {.intvalue = {168, 168, 0, 255}}},	{UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 16, {.intvalue = {0, 0, 0, 255}}},	{UI_LABEL, 0, {.label = {".", -1}}},
	{UI_INT, 17, {.intvalue = {1, 1, 0, 255}}},
	
	{UI_BREAK},
	
	{UI_LABEL, 0, {.label = {"PORT", -1}}},	{UI_SPACER}, {UI_INT, 18, {.intvalue = {445, 445, 0, 1024}}},
	
	{UI_BREAK},
	
	//  ---- PC share name ----
	{UI_LABEL, 0, {.label = {"SHARE NAME", -1}}}, {UI_SPACER}, {UI_STRING, 19, {.stringvalue = {"PS2SMB", "PS2SMB"}}}, {UI_BREAK},
	
	{UI_LABEL, 0, {.label = {"", _STR_NETWORK_AUTOSTART}}}, {UI_SPACER}, {UI_BOOL, 20, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	//  ---- Ok ----
	{UI_SPLITTER},
	{UI_OK},
	
	// end of dialogue
	{UI_TERMINATOR}
};

// Dialog for per-game compatibility settings
// Dialog definition for IP configuration
#define COMPAT_SAVE 100
#define COMPAT_TEST 101
#define COMPAT_REMOVE 102
struct UIItem diaCompatConfig[] = {
	{UI_LABEL, 110, {.label = {"<Game Label>", -1}}},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 111, {.label = {"", _STR_COMPAT_SETTINGS}}}, {UI_BREAK}, {UI_BREAK}, {UI_BREAK},

	{UI_LABEL, 0, {.label = {"Mode 1", -1}}}, {UI_SPACER}, {UI_BOOL, 10, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, {.label = {"Mode 2", -1}}}, {UI_SPACER}, {UI_BOOL, 11, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, {.label = {"Mode 3", -1}}}, {UI_SPACER}, {UI_BOOL, 12, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_OK}, {UI_SPACER}, {UI_BUTTON, COMPAT_SAVE, {.label = {"", _STR_SAVE_CHANGES}}}, 
		 {UI_SPACER}, {UI_BUTTON, COMPAT_TEST, {.label = {"Test", -1}}},
	
	{UI_SPLITTER},
	
	{UI_BUTTON, COMPAT_REMOVE, {.label = {"", _STR_REMOVE_ALL_SETTINGS}}},
	
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
	}
}

// returns COMPAT_TEST on testing request
int showCompatConfig(const char* game, const char* prefix, int ntype) {
	char temp[255];
	int mode[3];
	int i;
	
	diaSetLabel(diaCompatConfig, 110, game);
	
	for (i = 0; i < 3; ++i) {
		snprintf(temp, 255, "%s_%d_%d", prefix, ntype, i+1);
		
		if (!getConfigInt(&gConfig, temp, &mode[i]))
			mode[i] = 0;
		
		diaSetInt(diaCompatConfig, i+10, mode[i]);
	}
	
	// show dialog
	int result = diaExecuteDialog(diaCompatConfig);
	
	if (result == COMPAT_REMOVE) {
		for (i = 0; i < 3; ++i) {
			snprintf(temp, 255, "%s_%d_%d", prefix, ntype, i+1);
			configRemoveKey(&gConfig, temp);
		}
		
		MsgBox(_l(_STR_REMOVED_ALL_SETTINGS));
	} else if (result > 0) { // okay pressed or other button
		for (i = 0; i < 3; ++i) {
			diaGetInt(diaCompatConfig, i+10, &mode[i]);
			snprintf(temp, 255, "%s_%d_%d", prefix, ntype, i+1);
			setConfigInt(&gConfig, temp, mode[i]);
		}
		
		if (result == COMPAT_SAVE) // write the config
			storeConfig();
	}
	
	return result;
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
// Submenu items variant of the game list... keeped in sync with the game item list (firstgame, actualgame)
struct TSubMenuList* usb_submenu = NULL;
struct TSubMenuList* eth_submenu = NULL;

/// List of usb games
struct TMenuItem usb_games_item;
/// List of network games
struct TMenuItem network_games_item;

/// scroll speed selector
struct TSubMenuList* speed_item = NULL;

/// menu type selector
struct TSubMenuList* menutype_item = NULL;

unsigned int scroll_speed_txt[3] = {_STR_SCROLL_SLOW, _STR_SCROLL_MEDIUM, _STR_SCROLL_FAST};
unsigned int menu_type_txt[2] = { _STR_MENU_DYNAMIC,  _STR_MENU_STATIC};

void ClearUSBSubMenu() {
	DestroySubMenu(&usb_submenu);
	AppendSubMenu(&usb_submenu, &disc_icon, "", -1, _STR_NO_ITEMS);
	usb_games_item.submenu = usb_submenu;
	usb_games_item.current = usb_submenu;
}

void RefreshUSBGameList() {
	int fd, size, n;
	TGameList *list;
	char buffer[0x040];

	if(usbdelay<=100)  {
		usbdelay++;
		return;
	}
	  
	usbdelay = 0;
	fd=fioOpen("mass:ul.cfg", O_RDONLY);
	
	if(fd<0) { // file could not be opened. reset menu
		if(max_games>0) {
			actualgame=firstgame;
			while(actualgame->next!=0){
				actualgame=actualgame->next;
				free(actualgame->prev);
			}
			free(actualgame);
		}
		max_games=0;
		
		ClearUSBSubMenu();
		return;
	}

	// only refresh if the list is empty
	if (max_games==0) {
		DestroySubMenu(&usb_submenu);
		usb_submenu = NULL;
		usb_games_item.submenu = usb_submenu;
		usb_games_item.current = usb_submenu;

		size = fioLseek(fd, 0, SEEK_END);  
		fioLseek(fd, 0, SEEK_SET); 
		
		int id = 1; // game id (or order)
		for(n=0;n<size;n+=0x40,++id){
			fioRead(fd, buffer, 0x40);
			
			
			list=(TGameList*)malloc(sizeof(TGameList));
			
			// to ensure no leaks happen, we copy manually and pad the strings
			memcpy(&list->Game.Name, buffer, 32);
			list->Game.Name[32] = '\0';
			memcpy(&list->Game.Image, &buffer[32], 16);
			list->Game.Image[16] = '\0';
			memcpy(&list->Game.parts, &buffer[47], 1);
			memcpy(&list->Game.media, &buffer[48], 1);
			
			if(actualgame!=NULL){
				actualgame->next=list;
				list->prev=actualgame;
			}else{
				firstgame=list;
				list->prev=0;
			}
			list->next=0;
			actualgame=list;
			max_games++;
			
			
			
			AppendSubMenu(&usb_submenu, &disc_icon, actualgame->Game.Name, id, -1);
		}
		
			
		usb_games_item.submenu = usb_submenu;
		usb_games_item.current = usb_submenu;
		actualgame=firstgame;
	}

	fioClose(fd);
}

// --------------------- USB Menu item callbacks --------------------
void PrevUSBGame(struct TMenuItem *self) {
	if (!actualgame)
		return;
	
	if(actualgame->prev!=NULL){
		actualgame=actualgame->prev;
	}
}

void NextUSBGame(struct TMenuItem *self) {
	if (!actualgame)
		return;

	if(actualgame->next!=NULL){
		actualgame=actualgame->next;
	}
}

int ResetUSBOrder(struct TMenuItem *self) {
	actualgame = firstgame;
	return 0;
}

void ExecUSBGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	padPortClose(0, 0);
	padPortClose(1, 0);
	padReset();
	LaunchGame(&actualgame->Game, USB_MODE);
}

void AltExecUSBGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	if (showCompatConfig(actualgame->Game.Name,  actualgame->Game.Image, USB_MODE) == COMPAT_TEST)
		ExecUSBGameSelection(self, id);
}

void ClearETHSubMenu() {
	DestroySubMenu(&eth_submenu);
	
	if (eth_inited || gNetAutostart)
		AppendSubMenu(&eth_submenu, &disc_icon, "", -1, _STR_NO_ITEMS);
	else
		AppendSubMenu(&eth_submenu, &netconfig_icon, "", -1, _STR_START_NETWORK);
	
	network_games_item.submenu = eth_submenu;
	network_games_item.current = eth_submenu;
}

void RefreshETHGameList() {
	int fd, size, n;
	TGameList *list;
	char buffer[0x040];
	
	if(ethdelay<=100)  {
		ethdelay++;
		return;
	}
		  
	ethdelay = 0;
	fd=fioOpen("smb0:\\ul.cfg", O_RDONLY);
	
	if(fd<0) { // file could not be opened. reset menu
		if(eth_max_games>0) {
			eth_actualgame=eth_firstgame;
			while(eth_actualgame->next!=0){
				eth_actualgame=eth_actualgame->next;
				free(eth_actualgame->prev);
			}
			free(eth_actualgame);
		}
		eth_max_games=0;
		
		ClearETHSubMenu();
		return;
	}

	// only refresh if the list is empty
	if (eth_max_games==0) {
		DestroySubMenu(&eth_submenu);
		eth_submenu = NULL;
		network_games_item.submenu = eth_submenu;
		network_games_item.current = eth_submenu;

		size = fioLseek(fd, 0, SEEK_END);  
		fioLseek(fd, 0, SEEK_SET); 
		
		int id = 1; // game id (or order)
		for(n=0;n<size;n+=0x40,++id){
			fioRead(fd, &buffer, 0x40);
			list=(TGameList*)malloc(sizeof(TGameList));
			
			memcpy(&list->Game.Name, buffer, 32);
			list->Game.Name[32] = '\0';
			memcpy(&list->Game.Image, &buffer[32], 16);
			list->Game.Image[16] = '\0';
			memcpy(&list->Game.parts, &buffer[47], 1);
			memcpy(&list->Game.media, &buffer[48], 1);
			
			if(eth_actualgame!=NULL){
				eth_actualgame->next=list;
				list->prev=eth_actualgame;
			}else{
				eth_firstgame=list;
				list->prev=0;
			}
			list->next=0;
			eth_actualgame=list;
			eth_max_games++;
			
			AppendSubMenu(&eth_submenu, &disc_icon, eth_actualgame->Game.Name, id, -1);
		}
		
			
		network_games_item.submenu = eth_submenu;
		network_games_item.current = eth_submenu;
		eth_actualgame=eth_firstgame;
	}

	fioClose(fd);
}

// --------------------- Network Menu item callbacks --------------------
void PrevETHGame(struct TMenuItem *self) {
	if (!eth_actualgame)
		return;
	
	if(eth_actualgame->prev!=NULL){
		eth_actualgame=eth_actualgame->prev;
	}
}

void NextETHGame(struct TMenuItem *self) {
	if (!eth_actualgame)
		return;

	if(eth_actualgame->next!=NULL){
		eth_actualgame=eth_actualgame->next;
	}
}

int ResetETHOrder(struct TMenuItem *self) {
	eth_actualgame = eth_firstgame;
	return 0;
}

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
	LaunchGame(&eth_actualgame->Game, ETH_MODE);
}

void AltExecETHGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	if (showCompatConfig(eth_actualgame->Game.Name,  eth_actualgame->Game.Image, ETH_MODE) == COMPAT_TEST)
		ExecETHGameSelection(self, id);
}

// --------------------- Exit/Settings Menu item callbacks --------------------
/// menu item selection callbacks
void ExecExit(struct TMenuItem* self, int vorder) {
	__asm__ __volatile__(
		"	li $3, 0x04;"
		"	syscall;"
		"	nop;"
	);
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
		DrawConfig();
	} else if(id == 2) {
		// set the language
		gLanguageID++;
		if (gLanguageID > _LANG_ID_MAX)
			gLanguageID = 0;
			
		setLanguage(gLanguageID);
	} else if (id == 3) {
		// ipconfig
		showIPConfig();
	} else if (id == 4) {
		// scroll speed modifier
		ChangeScrollSpeed();
	} else if (id == 5) {
		ChangeMenuType();
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
	&usb_icon, "USB Games", _STR_USB_GAMES, NULL, NULL, NULL, &ExecUSBGameSelection, &NextUSBGame, &PrevUSBGame, &ResetUSBOrder, &RefreshUSBGameList, &AltExecUSBGameSelection
};

struct TMenuItem hdd_games_item = {
	&games_icon, "HDD Games", _STR_HDD_GAMES, NULL, NULL, NULL
};

struct TMenuItem network_games_item = {
	&network_icon, "Network Games", _STR_NET_GAMES, NULL, NULL, NULL, &ExecETHGameSelection, &NextETHGame, &PrevETHGame, &ResetETHOrder, &RefreshETHGameList, &AltExecETHGameSelection
};

struct TMenuItem apps_item = {
	&apps_icon, "Apps", _STR_APPS, NULL, NULL, NULL
};


void InitMenuItems() {
	ClearUSBSubMenu();
	ClearETHSubMenu();
	
	// initialize the menu
	AppendSubMenu(&settings_submenu, &theme_icon, "Theme", 1, _STR_THEME);
	AppendSubMenu(&settings_submenu, &language_icon, "Language", 2, _STR_LANG_NAME);
	AppendSubMenu(&settings_submenu, &netconfig_icon, "Network config", 3, _STR_IPCONFIG);
	
	speed_item = AppendSubMenu(&settings_submenu, &scroll_icon, "", 4, scroll_speed_txt[scroll_speed]);
	menutype_item = AppendSubMenu(&settings_submenu, &menu_icon, "", 5, menu_type_txt[dynamic_menu ? 0 : 1]);
	AppendSubMenu(&settings_submenu, &save_icon, "Save Changes", 7, _STR_SAVE_CHANGES);
	
	settings_item.submenu = settings_submenu;
	
	// add all menu items
	menu = NULL;
	AppendMenuItem(&exit_item);
	AppendMenuItem(&settings_item);
	AppendMenuItem(&usb_games_item);
	AppendMenuItem(&hdd_games_item);
	AppendMenuItem(&network_games_item);
	AppendMenuItem(&apps_item);
}


// main initialization function
void init() {
	Reset();

	InitGFX();

	ps2_ip[0] = 192; ps2_ip[1] = 168; ps2_ip[2] =  0; ps2_ip[3] =  10;
	ps2_netmask[0] = 255; ps2_netmask[1] = 255; ps2_netmask[2] =  255; ps2_netmask[3] =  0;
	ps2_gateway[0] = 192; ps2_gateway[1] = 168; ps2_gateway[2] = 0; ps2_gateway[3] = 1;
	pc_ip[0] = 192;pc_ip[1] = 168; pc_ip[2] = 0; pc_ip[3] = 2;
	gPCPort = 445;
	// loading progress of the network
	gNetworkStartup = 0;
	// autostart network?
	gNetAutostart = 0;
	// no change to the ipconfig was done
	gIPConfigChanged = 0;
	// Default PC share name
	strncpy(gPCShareName, "PS2SMB", 32);
	
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

	eth_max_games=0;	
	max_games=0;
	usbdelay=0;
	ethdelay=0;
	frame=0;	

	// these seem to matter. Without them, something tends to crush into itself
	delay(1);
}

// --------------------- Main --------------------
int main(void)
{
	init();
	
	Intro();
	
	// these seem to matter. Without them, something tends to crush into itself
	delay(1);

	if (gNetAutostart != 0) {
		Start_LoadNetworkModules_Thread();
		eth_inited = 1;
	}
	
	TextColor(0x80,0x80,0x80,0x80);
	
	while(1)
	{
		ReadPad();

		DrawScreen();
				
		if(GetKey(KEY_LEFT)){
			MenuPrevH();
		}else if(GetKey(KEY_RIGHT)){
			MenuNextH();
		}else if(GetKey(KEY_UP)){
			MenuPrevV();
		}else if(GetKey(KEY_DOWN)){
			MenuNextV();
		}
		
		if(GetKeyOn(KEY_CROSS)){
			// handle via callback in the menuitem
			MenuItemExecute();
		} else if(GetKeyOn(KEY_TRIANGLE)){
			// handle via callback in the menuitem
			MenuItemAltExecute();
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

