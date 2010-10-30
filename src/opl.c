/*
  Copyright 2009, Volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include "include/ioman.h"
#include "include/gui.h"
#include "include/renderman.h"
#include "include/lang.h"
#include "include/themes.h"
#include "include/textures.h"
#include "include/pad.h"
#include "include/texcache.h"
#include "include/dia.h"
#include "include/dialogs.h"
#include "include/menusys.h"
#include "include/system.h"
#include "include/debug.h"

// temp
#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"
#include "include/appsupport.h"

// for sleep()
#include <unistd.h>

#ifdef __EESIO_DEBUG
#include <sio.h>
#define LOG_INIT()		sio_init(38400, 0, 0, 0, 0)
#define LOG_ENABLE()		do { } while(0)
#else
#ifdef __DEBUG
#define LOG_INIT()		do { } while(0)
#define LOG_ENABLE()		ioPutRequest(IO_CUSTOM_SIMPLEACTION, &debugSetActive)
#else
#define LOG_INIT()		do { } while(0)
#define LOG_ENABLE()		do { } while(0)
#endif
#endif

#define UPDATE_FRAME_COUNT 250

#define IO_MENU_UPDATE_DEFFERED 2

extern void *usbd_irx;
extern int size_usbd_irx;

// forward decl
static void handleHdlSrv();


typedef struct {
	item_list_t *support;
	
	/// menu item used with this list support
	struct menu_item_t menuItem;
	
	/// submenu list
	struct submenu_list_t *subMenu;
} opl_io_module_t;

static void clearIOModuleT(opl_io_module_t *mod) {
	mod->subMenu = NULL;
	mod->support = NULL;
	mod->menuItem.execCross = NULL;
	mod->menuItem.execCircle = NULL;
	mod->menuItem.execSquare = NULL;
	mod->menuItem.execTriangle = NULL;
	mod->menuItem.hints = NULL;
	mod->menuItem.icon_id = -1;
	mod->menuItem.current = NULL;
	mod->menuItem.submenu = NULL;
	mod->menuItem.pagestart = NULL;
	mod->menuItem.refresh = NULL;
	mod->menuItem.text = NULL;
	mod->menuItem.text_id = -1;
	mod->menuItem.userdata = NULL;
}

// forward decl
static void moduleCleanup(opl_io_module_t* mod, int exception);

// frame counter
static int gFrameCounter;

static opl_io_module_t list_support[4];

static void initHints(struct menu_item_t* menuItem, int startMode) {
	menuAddHint(menuItem, _STR_SETTINGS, START_ICON);

	if (startMode == 1)
		menuAddHint(menuItem, _STR_START_DEVICE, CROSS_ICON);
	else {
		menuAddHint(menuItem, _STR_RUN, CROSS_ICON);
		item_list_t *support = menuItem->userdata;
		if (support->itemGetCompatibility)
			menuAddHint(menuItem, _STR_COMPAT_SETTINGS, TRIANGLE_ICON);
		if (gEnableDandR && support->itemRename)
			menuAddHint(menuItem, _STR_RENAME, CIRCLE_ICON);
		if (gEnableDandR && support->itemDelete)
			menuAddHint(menuItem, _STR_DELETE, SQUARE_ICON);
	}
}

static void itemExecCross(struct menu_item_t *self, int id) {
	item_list_t *support = self->userdata;

	if (support) {
		if (support->enabled) {
			if (id >= 0)
				support->itemLaunch(id);
		}
		else { 
			support->itemInit();
			menuRemoveHints(self); // This is okay since the itemExec is executed from menu (GUI) itself (no need to defer)
			initHints(self, 2);
		}
	}
	else
		guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecTriangle(struct menu_item_t *self, int id) {
	item_list_t *support = self->userdata;

	if (id < 0)
		return;

	if (support) {
		if (support->itemGetCompatibility) {
			if (guiShowCompatConfig(id, support) == COMPAT_TEST)
				itemExecCross(self, id);
		}
	}
	else
		guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecSquare(struct menu_item_t *self, int id) {
	item_list_t *support = self->userdata;

	if (id < 0)
		return;

	if (!gEnableDandR)
		return;

	if (support) {
		if (support->itemDelete) {
			if (guiMsgBox(_l(_STR_DELETE_WARNING), 1, NULL)) {
				support->itemDelete(id);
				gFrameCounter = UPDATE_FRAME_COUNT;
			}
		}
	}
	else
		guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecCircle(struct menu_item_t *self, int id) {
	item_list_t *support = self->userdata;

	if (id < 0)
		return;

	if (!gEnableDandR)
		return;

	if (support) {
		if (support->itemRename) {
			char newName[support->maxNameLength];
			strncpy(newName, self->current->item.text, support->maxNameLength);
			if (guiShowKeyboard(newName, support->maxNameLength)) {
				support->itemRename(id, newName);
				gFrameCounter = UPDATE_FRAME_COUNT;
			}
		}
	}
	else
		guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void initMenuForListSupport(opl_io_module_t* item, int startMode) {
	item->menuItem.icon_id = item->support->iconId;
	item->menuItem.text = NULL;
	item->menuItem.text_id = item->support->textId;

	item->menuItem.userdata = item->support;

	item->subMenu = NULL;

	item->menuItem.submenu = NULL;
	item->menuItem.current = NULL;
	item->menuItem.pagestart = NULL;

	item->menuItem.refresh = NULL;
	item->menuItem.execCross = &itemExecCross;
	item->menuItem.execTriangle = &itemExecTriangle;
	item->menuItem.execSquare = &itemExecSquare;
	item->menuItem.execCircle = &itemExecCircle;

	item->menuItem.hints = NULL;

	initHints(&item->menuItem, startMode);

	struct gui_update_t *mc = guiOpCreate(GUI_OP_ADD_MENU);
	mc->menu.menu = &item->menuItem;
	mc->menu.subMenu = &item->subMenu;
	guiDeferUpdate(mc);
}

static void initSupport(item_list_t* itemList, int startMode, int mode, int force_reinit) {
	if (!list_support[mode].support) {
		itemList->uip = 1; // stop updates until we're done with init
		list_support[mode].support = itemList;
		initMenuForListSupport(&list_support[mode], startMode);
		itemList->uip = 0;
	}

	if (((force_reinit) && (startMode && list_support[mode].support->enabled)) \
	  || (startMode == 2 && !list_support[mode].support->enabled)) {
		// stop updates until we're done with init of the device
		list_support[mode].support->uip = 1;
		list_support[mode].support->itemInit();
		gFrameCounter = UPDATE_FRAME_COUNT;
		list_support[mode].support->uip = 0;
	}
}

static void initAllSupport(int force_reinit) {
	if (gUSBStartMode)
		initSupport(usbGetObject(0), gUSBStartMode, USB_MODE, force_reinit);

	if (gETHStartMode)
		initSupport(ethGetObject(0), gETHStartMode, ETH_MODE, force_reinit);

	if (gHDDStartMode)
		initSupport(hddGetObject(0), gHDDStartMode, HDD_MODE, force_reinit);

	if (gAPPStartMode)
		initSupport(appGetObject(0), gAPPStartMode, APP_MODE, force_reinit);
}

static void deinitAllSupport(int exception) {
	moduleCleanup(&list_support[USB_MODE], exception);
	moduleCleanup(&list_support[ETH_MODE], exception);
	moduleCleanup(&list_support[HDD_MODE], exception);
	moduleCleanup(&list_support[APP_MODE], exception);
}

#define MENU_ID_START_HDL 100
#define MENU_ID_START_USB 101
#define MENU_ID_START_HDD 102
#define MENU_ID_START_ETH 103
#define MENU_ID_START_APP 104

static void menuExecHookFunc(int id) {
	if (id == MENU_ID_START_HDL) {
		handleHdlSrv();
	}        
}

static void menuFillHookFunc(struct submenu_list_t **menu) {
#ifndef __CHILDPROOF
	if (gHDDStartMode) // enabled at all?
		submenuAppendItem(menu, DISC_ICON, "Start HDL Server", MENU_ID_START_HDL, _STR_STARTHDL);
#endif
}

// ----------------------------------------------------------
// ------------------ Configuration handling ----------------
// ----------------------------------------------------------

static int lscstatus = CONFIG_ALL;
static int lscret = 0;

static int configTryAlternate(int types) {
	char path[64];

	// check USB
	gUSBStartMode = 2;
	initSupport(usbGetObject(0), gUSBStartMode, USB_MODE, 0);
	delay(10);
	if (usbFindPartition(path, "conf_opl.cfg")) {
		configEnd();
		configInit(path);
		return configReadMulti(types);
	}

	// check HDD
	gHDDStartMode = 2;
	initSupport(hddGetObject(0), gHDDStartMode, HDD_MODE, 0);
	delay(10);
	snprintf(path, 64, "pfs0:conf_opl.cfg");
	int fd = fioOpen(path, O_RDONLY);
	if(fd >= 0) {
		fioClose(fd);
		configEnd();
		configInit("pfs0:");
		return configReadMulti(types);
	}

	return 0;
}

static void _loadConfig() {
	int result = configReadMulti(lscstatus);

	if (lscstatus & CONFIG_OPL) {
		int themeID = -1, langID = -1;
		
		if (!result)
			result = configTryAlternate(lscstatus);

		if (result) {
			config_set_t *configOPL = configGetByType(CONFIG_OPL);
			char *temp;

			configGetInt(configOPL, "icons_cache_count", &gCountIconsCache);
			configGetInt(configOPL, "covers_cache_count", &gCountCoversCache);
			configGetInt(configOPL, "bg_cache_count", &gCountBackgroundsCache);
			configGetInt(configOPL, "scrolling", &gScrollSpeed);
			configGetColor(configOPL, "bg_color", gDefaultBgColor);
			configGetColor(configOPL, "text_color", gDefaultTextColor);
			configGetColor(configOPL, "ui_text_color", gDefaultUITextColor);
			configGetColor(configOPL, "sel_text_color", gDefaultSelTextColor);
			configGetInt(configOPL, "exit_mode", &gExitMode);
			configGetInt(configOPL, "enable_coverart", &gEnableArt);
			configGetInt(configOPL, "wide_screen", &gWideScreen);

			if (configGetStr(configOPL, "theme", &temp))
				themeID = thmFindGuiID(temp);

			if (configGetStr(configOPL, "language_text", &temp))
				langID = lngFindGuiID(temp);

			if (configGetStr(configOPL, "pc_ip", &temp))
				sscanf(temp, "%d.%d.%d.%d", &pc_ip[0], &pc_ip[1], &pc_ip[2], &pc_ip[3]);

			configGetInt(configOPL, "pc_port", &gPCPort);

			if (configGetStr(configOPL, "pc_share", &temp))
				strncpy(gPCShareName, temp, 32);
			if (configGetStr(configOPL, "pc_user", &temp))
				strncpy(gPCUserName, temp, 32);
			if (configGetStr(configOPL, "pc_pass", &temp))
				strncpy(gPCPassword, temp, 32);

			configGetInt(configOPL, "autosort", &gAutosort);
			configGetInt(configOPL, "default_device", &gDefaultDevice);
			configGetInt(configOPL, "disable_debug", &gDisableDebug);
			configGetInt(configOPL, "enable_delete_rename", &gEnableDandR);
			configGetInt(configOPL, "check_usb_frag", &gCheckUSBFragmentation);
			configGetInt(configOPL, "remember_last", &gRememberLastPlayed);
			configGetInt(configOPL, "usb_mode", &gUSBStartMode);
			configGetInt(configOPL, "hdd_mode", &gHDDStartMode);
			configGetInt(configOPL, "eth_mode", &gETHStartMode);
			configGetInt(configOPL, "app_mode", &gAPPStartMode);
		}
		
		applyConfig(themeID, langID);
	}

	lscret = result;
	lscstatus = 0;
}

static void _saveConfig() {
	if (lscstatus & CONFIG_OPL) {
		config_set_t *configOPL = configGetByType(CONFIG_OPL);
		configSetInt(configOPL, "icons_cache_count", gCountIconsCache);
		configSetInt(configOPL, "covers_cache_count", gCountCoversCache);
		configSetInt(configOPL, "bg_cache_count", gCountBackgroundsCache);
		configSetInt(configOPL, "scrolling", gScrollSpeed);
		configSetStr(configOPL, "theme", thmGetValue());
		configSetStr(configOPL, "language_text", lngGetValue());
		configSetColor(configOPL, "bg_color", gDefaultBgColor);
		configSetColor(configOPL, "text_color", gDefaultTextColor);
		configSetColor(configOPL, "ui_text_color", gDefaultUITextColor);
		configSetColor(configOPL, "sel_text_color", gDefaultSelTextColor);
		configSetInt(configOPL, "exit_mode", gExitMode);
		configSetInt(configOPL, "enable_coverart", gEnableArt);
		configSetInt(configOPL, "wide_screen", gWideScreen);

		char temp[255];
		sprintf(temp, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
		configSetStr(configOPL, "pc_ip", temp);
		configSetInt(configOPL, "pc_port", gPCPort);
		configSetStr(configOPL, "pc_share", gPCShareName);
		configSetStr(configOPL, "pc_user", gPCUserName);
		configSetStr(configOPL, "pc_pass", gPCPassword);
		configSetInt(configOPL, "autosort", gAutosort);
		configSetInt(configOPL, "default_device", gDefaultDevice);
		configSetInt(configOPL, "disable_debug", gDisableDebug);
		configSetInt(configOPL, "enable_delete_rename", gEnableDandR);
		configSetInt(configOPL, "check_usb_frag", gCheckUSBFragmentation);
		configSetInt(configOPL, "remember_last", gRememberLastPlayed);
		configSetInt(configOPL, "usb_mode", gUSBStartMode);
		configSetInt(configOPL, "hdd_mode", gHDDStartMode);
		configSetInt(configOPL, "eth_mode", gETHStartMode);
		configSetInt(configOPL, "app_mode", gAPPStartMode);
	}
	
	lscret = configWriteMulti(lscstatus);
	lscstatus = 0;
}

void applyConfig(int themeID, int langID) {
	infotxt = _l(_STR_WELCOME);

	if (gDefaultDevice < 0 || gDefaultDevice > APP_MODE)
		gDefaultDevice = APP_MODE;

	if (gCountIconsCache < gTheme->displayedItems)	// if user want to cache less than displayed items
		gTheme->itemsListIcons = 0;			// then disable itemslist icons, if not would load constantly

	guiUpdateScrollSpeed();
	guiUpdateScreenScale();

	// theme must be set after color, and lng after theme
	if (themeID != -1)
		thmSetGuiValue(themeID);
	if (langID != -1)
		lngSetGuiValue(langID);

	// has to be non-empty
	if (strlen(gPCShareName) == 0)
		strncpy(gPCShareName, "PS2SMB", 32);

	if (gPCPort < 0)
		gPCPort = 445;

	initAllSupport(0);
}

int loadConfig(int types) {
	lscstatus = types;
	lscret = 0;
	
	guiHandleDefferedIO(&lscstatus, _l(_STR_LOADING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_loadConfig);
	
	return lscret;
}

int saveConfig(int types, int showUI) {
	lscstatus = types;
	lscret = 0;
	
	guiHandleDefferedIO(&lscstatus, _l(_STR_SAVING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_saveConfig);
	
	if (showUI) {
		if (lscret)
			guiMsgBox(_l(_STR_SETTINGS_SAVED), 0, NULL);
		else
			guiMsgBox(_l(_STR_ERROR_SAVING_SETTINGS), 0, NULL);
	}
	
	return lscret;
}

// ----------------------------------------------------------
// ----------------------- Updaters -------------------------
// ----------------------------------------------------------
static void updateMenuFromGameList(opl_io_module_t* mdl) {
	if (!mdl)
		return;
	
	if (!mdl->support)
		return;
	
	// lock - gui has to be unused here
	guiLock();
	
	submenuDestroy(&mdl->subMenu);
	mdl->menuItem.submenu = NULL;
	mdl->menuItem.current = NULL;
	mdl->menuItem.pagestart = NULL;
	
	// unlock, the rest is deferred
	guiUnlock();
	
	char* temp = NULL;
	if (gRememberLastPlayed)
		configGetStr(configGetByType(CONFIG_OPL), "last_played", &temp);

	// read the new game list
	struct gui_update_t *gup = NULL;
	int count = mdl->support->itemUpdate();
	if (count) {
		int i;

		for (i = 0; i < count; ++i) {
			
			gup = guiOpCreate(GUI_OP_APPEND_MENU);
			
			gup->menu.menu = &mdl->menuItem;
			gup->menu.subMenu = &mdl->subMenu;
			
			gup->submenu.icon_id = DISC_ICON;
			gup->submenu.id = i;
			gup->submenu.text = mdl->support->itemGetName(i);
			gup->submenu.text_id = -1;
			gup->submenu.selected = 0;
			
			if (gRememberLastPlayed && temp && strcmp(temp, mdl->support->itemGetStartup(i)) == 0)
				gup->submenu.selected = 1;

			guiDeferUpdate(gup);
		}
	}
	
	if (gAutosort) {
		gup = guiOpCreate(GUI_OP_SORT);
		gup->menu.menu = &mdl->menuItem;
		gup->menu.subMenu = &mdl->subMenu;
		guiDeferUpdate(gup);
	}
}

static void menuDeferredUpdate(void* data) {
	opl_io_module_t* mdl = data;
	
	if (!mdl)
		return;
	
	if (!mdl->support)
		return;
	
	if (mdl->support->uip)
		return;
	
	// see if we have to update
	if (mdl->support->itemNeedsUpdate()) {
		mdl->support->uip = 1;

		updateMenuFromGameList(mdl);
		
		mdl->support->uip = 0;
	}
}

static void menuUpdateHook() {
	// if timer exceeds some threshold, schedule updates of the available input sources
	gFrameCounter++;
	
	if (gFrameCounter > UPDATE_FRAME_COUNT) {
		gFrameCounter = 0;
		
		// schedule updates of all the list handlers
		if (list_support[USB_MODE].support && list_support[USB_MODE].support->enabled)
			ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[USB_MODE]);
		if (list_support[ETH_MODE].support && list_support[ETH_MODE].support->enabled)
			ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[ETH_MODE]);
		if (list_support[HDD_MODE].support && list_support[HDD_MODE].support->enabled)
			ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[HDD_MODE]);
		if (list_support[APP_MODE].support && list_support[APP_MODE].support->enabled)
			ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[APP_MODE]);
	}
}

// ----------------------------------------------------------
// -------------------- HD SRV Support ----------------------
// ----------------------------------------------------------
extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *smsutils_irx;
extern int size_smsutils_irx;

extern void *smstcpip_irx;
extern int size_smstcpip_irx;

extern void *smsmap_irx;
extern int size_smsmap_irx;

extern void *ps2atad_irx;
extern int size_ps2atad_irx;

extern void *ps2hdd_irx;
extern int size_ps2hdd_irx;

extern void *hdldsvr_irx;
extern int size_hdldsvr_irx;

void loadHdldSvr(void) {
	int ret;
	static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";
	char ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));

	// block all io ops, wait for the ones still running to finish
	ioBlockOps(1);

	deinitAllSupport(NO_EXCEPTION);

	unloadPads();

	sysReset();

	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:PADMAN", 0, NULL);

	int iplen = sysSetIPConfig(ipconfig);

	ret = sysLoadModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
	if (ret < 0)
		return;

	ret = sysLoadModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL);
	if (ret < 0)
		return;

	ret = sysLoadModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL);
	if (ret < 0)
		return;

	ret = sysLoadModuleBuffer(&smsmap_irx, size_smsmap_irx, iplen, ipconfig);
	if (ret < 0)
		return;

	ret = sysLoadModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL);
	if (ret < 0)
		return;

	ret = sysLoadModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg);
	if (ret < 0)
		return;

	ret = sysLoadModuleBuffer(&hdldsvr_irx, size_hdldsvr_irx, 0, NULL);
	if (ret < 0)
		return;

	padInit(0);

	// init all pads
	ret = 0;
	while (!ret)
		ret = startPads();
		
	// now ready to display some status
}

void unloadHdldSvr(void) {
	unloadPads();

	sysReset();

	LOG_INIT();
	LOG_ENABLE();

	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:MCMAN", 0, NULL);
	SifLoadModule("rom0:MCSERV", 0, NULL);
	SifLoadModule("rom0:PADMAN", 0, NULL);
	mcInit(MC_TYPE_MC);

	// reinit the input pads
	padInit(0);
	
	int ret = 0;
	while (!ret)
		ret = startPads();

	// now start io again
	ioBlockOps(0);

	// init all supports again
	initAllSupport(1);
}

static void handleHdlSrv() {
	guiRenderTextScreen(_l(_STR_STARTINGHDL));
	
	// prepare for hdl, display screen with info
	loadHdldSvr();
	
	int terminate = 0;
	
	while (1) {
		if (terminate != 0)
			guiRenderTextScreen(_l(_STR_STOPHDL));
		else
			guiRenderTextScreen(_l(_STR_RUNNINGHDL));
	
		sleep(2);
		
		readPads();
		
		if(getKeyOn(KEY_CIRCLE) && terminate == 0) {
			terminate++;
		} else if(getKeyOn(KEY_CROSS) && terminate == 1) {
			terminate++;
		} else if (terminate > 0) 
			terminate--;
		
		if (terminate >= 2)
			break;
	}
	
	guiRenderTextScreen(_l(_STR_UNLOADHDL));
	
	// restore normal functionality again
	unloadHdldSvr();
}

// ----------------------------------------------------------
// --------------------- Init/Deinit ------------------------
// ----------------------------------------------------------
static void reset(void) {
	sysReset();
	
	SifInitRpc(0);

	SifLoadModule("rom0:SIO2MAN",0,0);
	SifLoadModule("rom0:MCMAN",0,0);
	SifLoadModule("rom0:MCSERV",0,0);
	SifLoadModule("rom0:PADMAN",0,0);
	mcInit(MC_TYPE_MC);
}

static void moduleCleanup(opl_io_module_t* mod, int exception) {
	if (!mod)
		return;

	if (!mod->support)
		return;

	if (mod->support->itemCleanUp)
		mod->support->itemCleanUp(exception);
}

void shutdown(int exception) {
	unloadPads();
	ioEnd();
	guiEnd();
	lngEnd();
	thmEnd();
	rmEnd();
	configEnd();

	deinitAllSupport(exception);
}


static void setDefaults(void) {
	usbd_irx = NULL;
	size_usbd_irx = 0;

	clearIOModuleT(&list_support[USB_MODE]);
	clearIOModuleT(&list_support[ETH_MODE]);
	clearIOModuleT(&list_support[HDD_MODE]);
	clearIOModuleT(&list_support[APP_MODE]);
	
	gBaseMCDir = "mc?:OPL";

	ps2_ip[0] = 192; ps2_ip[1] = 168; ps2_ip[2] =  0; ps2_ip[3] =  10;
	ps2_netmask[0] = 255; ps2_netmask[1] = 255; ps2_netmask[2] =  255; ps2_netmask[3] =  0;
	ps2_gateway[0] = 192; ps2_gateway[1] = 168; ps2_gateway[2] = 0; ps2_gateway[3] = 1;
	pc_ip[0] = 192;pc_ip[1] = 168; pc_ip[2] = 0; pc_ip[3] = 2;
	
	// SMB port on pc
	gPCPort = 445;
	
	// default values
	strncpy(gPCShareName, "", 32);
	strncpy(gPCUserName, "GUEST", 32);
	strncpy(gPCPassword,  "", 32);
	
	// loading progress of the network and hdd. Value "6" should mean not started yet...
	gNetworkStartup = 6;
	gHddStartup = 6;
	
	// no change to the ipconfig was done
	gIPConfigChanged = 0;
	// Default PC share name
	strncpy(gPCShareName, "PS2SMB", 32);
	gScrollSpeed = 1;
	//Default exit mode
	gExitMode = 0;
	// default menu
	gDefaultDevice = APP_MODE;
	// autosort defaults to zero
	gAutosort = 0;
	//Default disable debug colors
	gDisableDebug = 0;
	gEnableDandR = 0;
	gRememberLastPlayed = 0;
	gCheckUSBFragmentation = 0;
	// disable art by default
	gEnableArt = 0;
	gWideScreen = 0;

	gUSBStartMode = 0;
	gHDDStartMode = 0;
	gETHStartMode = 0;
	gAPPStartMode = 0;

	gCountIconsCache = 40;
	gCountCoversCache = 10;
	gCountBackgroundsCache = 5;

	gDefaultBgColor[0] = 0x028;
	gDefaultBgColor[1] = 0x0c5;
	gDefaultBgColor[2] = 0x0f9;

	gDefaultTextColor[0] = 0x0ff;
	gDefaultTextColor[1] = 0x0ff;
	gDefaultTextColor[2] = 0x0ff;

	gDefaultSelTextColor[0] = 0x0ff;
	gDefaultSelTextColor[1] = 0x080;
	gDefaultSelTextColor[2] = 0x000;

	gDefaultUITextColor[0] = 0x040;
	gDefaultUITextColor[1] = 0x080;
	gDefaultUITextColor[2] = 0x040;

	gFrameCounter = UPDATE_FRAME_COUNT;
}

static void init(void) {
	// default variable values
	setDefaults();

	padInit(0);
	configInit(gBaseMCDir);
	rmInit();
	lngInit();
	thmInit();
	guiInit();
	ioInit();
	
	guiSetMenuFillHook(&menuFillHookFunc);
	guiSetMenuExecHook(&menuExecHookFunc);
	
	startPads();

	// try to restore config
	_loadConfig();

	cacheInit();

	// handler for deffered menu updates
	ioRegisterHandler(IO_MENU_UPDATE_DEFFERED, &menuDeferredUpdate);
}

static void deferredInit(void) {

	// inform GUI main init part is over
	struct gui_update_t *id = guiOpCreate(GUI_INIT_DONE);
	guiDeferUpdate(id);

	// update hook...
	guiSetFrameHook(&menuUpdateHook);

	if (list_support[gDefaultDevice].support) {
		id = guiOpCreate(GUI_OP_SELECT_MENU);
		id->menu.menu = &list_support[gDefaultDevice].menuItem;
		guiDeferUpdate(id);
	}
}

// --------------------- Main --------------------
int main(void)
{
	LOG_INIT();

	// apply kernel patches
	sysApplyKernelPatches();

	// reset, load modules
	reset();

	init();

	// until this point in the code is reached, only PREINIT_LOG macro should be used
	LOG_ENABLE();

	// queue deffered init which shuts down the intro screen later
	ioPutRequest(IO_CUSTOM_SIMPLEACTION, &deferredInit);
	
	guiMainLoop();
	
	return 0;
}
