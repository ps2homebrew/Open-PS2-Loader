/*
  Copyright 2009, Volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
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
#include "include/config.h"
#include "include/util.h"

#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"
#include "include/appsupport.h"

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

static void RefreshAllLists(void);

extern void *pusbd_irx;
extern int size_pusbd_irx;

typedef struct {
	item_list_t *support;

	/// menu item used with this list support
	menu_item_t menuItem;

	/// submenu list
	submenu_list_t *subMenu;
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
	mod->menuItem.remindLast = 0;
	mod->menuItem.refresh = NULL;
	mod->menuItem.text = NULL;
	mod->menuItem.text_id = -1;
	mod->menuItem.userdata = NULL;
}

// forward decl
static void clearMenuGameList(opl_io_module_t* mdl);
static void moduleCleanup(opl_io_module_t* mod, int exception);
static void reset(void);

// frame counter
static unsigned int frameCounter;

static char errorMessage[256];

static opl_io_module_t list_support[4];

void moduleUpdateMenu(int mode, int themeChanged) {
	if (mode == -1)
		return;

	opl_io_module_t* mod = &list_support[mode];

	if (!mod->support)
		return;

	// refresh Hints
	menuRemoveHints(&mod->menuItem);

	menuAddHint(&mod->menuItem, _STR_SETTINGS, START_ICON);
	if (!mod->support->enabled)
		menuAddHint(&mod->menuItem, _STR_START_DEVICE, CROSS_ICON);
	else {
		if (gUseInfoScreen && gTheme->infoElems.first)
			menuAddHint(&mod->menuItem, _STR_INFO, CROSS_ICON);
		else
			menuAddHint(&mod->menuItem, _STR_RUN, CROSS_ICON);

		if (mod->support->haveCompatibilityMode)
			menuAddHint(&mod->menuItem, _STR_COMPAT_SETTINGS, TRIANGLE_ICON);

		menuAddHint(&mod->menuItem, _STR_REFRESH, SELECT_ICON);

		if (gEnableDandR) {
			if (mod->support->itemRename)
				menuAddHint(&mod->menuItem, _STR_RENAME, CIRCLE_ICON);
			if (mod->support->itemDelete)
				menuAddHint(&mod->menuItem, _STR_DELETE, SQUARE_ICON);
		}
	}

	// refresh Cache
	if (themeChanged)
		submenuRebuildCache(mod->subMenu);
}

static void itemExecCross(struct menu_item *curMenu) {
	item_list_t *support = curMenu->userdata;

	if (support) {
		if (support->enabled) {
			if (curMenu->current) {
				config_set_t* configSet = menuLoadConfig();
				support->itemLaunch(curMenu->current->item.id, configSet);
			}
		}
		else {
			support->itemInit();
			moduleUpdateMenu(support->mode, 0);
			if (!gAutoRefresh)
				ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
		}
	}
	else
		guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecTriangle(struct menu_item *curMenu) {
	if (!curMenu->current)
		return;

	item_list_t *support = curMenu->userdata;

	if (support) {
		if (support->haveCompatibilityMode) {
			config_set_t* configSet = menuLoadConfig();
			if (guiShowCompatConfig(curMenu->current->item.id, support, configSet) == COMPAT_TEST)
				support->itemLaunch(curMenu->current->item.id, configSet);
		}
	}
	else
		guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecSquare(struct menu_item *curMenu) {
	if (!curMenu->current)
		return;

	if (!gEnableDandR)
		return;

	item_list_t *support = curMenu->userdata;

	if (support) {
		if (support->itemDelete) {
			if (guiMsgBox(_l(_STR_DELETE_WARNING), 1, NULL)) {
				support->itemDelete(curMenu->current->item.id);
				if (gAutoRefresh)
					RefreshAllLists();
				else
					ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
			}
		}
	}
	else
		guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecCircle(struct menu_item *curMenu) {
	if (!curMenu->current)
		return;

	if (!gEnableDandR)
		return;

	item_list_t *support = curMenu->userdata;

	if (support) {
		if (support->itemRename) {
			int nameLength = support->itemGetNameLength(curMenu->current->item.id);
			char newName[nameLength];
			strncpy(newName, curMenu->current->item.text, nameLength);
			if (guiShowKeyboard(newName, nameLength)) {
				support->itemRename(curMenu->current->item.id, newName);
				if (gAutoRefresh)
					RefreshAllLists();
				else
					ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
			}
		}
	}
	else
		guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecRefresh(struct menu_item *curMenu) {
	item_list_t *support = curMenu->userdata;

	if (support && support->enabled)
		ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
}

static void initMenuForListSupport(int mode) {
	opl_io_module_t* mod = &list_support[mode];
	mod->menuItem.icon_id = mod->support->iconId;
	mod->menuItem.text = NULL;
	mod->menuItem.text_id = mod->support->textId;

	mod->menuItem.userdata = mod->support;

	mod->subMenu = NULL;

	mod->menuItem.submenu = NULL;
	mod->menuItem.current = NULL;
	mod->menuItem.pagestart = NULL;
	mod->menuItem.remindLast = 0;

	mod->menuItem.refresh = &itemExecRefresh;
	mod->menuItem.execCross = &itemExecCross;
	mod->menuItem.execTriangle = &itemExecTriangle;
	mod->menuItem.execSquare = &itemExecSquare;
	mod->menuItem.execCircle = &itemExecCircle;

	mod->menuItem.hints = NULL;

	moduleUpdateMenu(mode, 0);

	struct gui_update_t *mc = guiOpCreate(GUI_OP_ADD_MENU);
	mc->menu.menu = &mod->menuItem;
	mc->menu.subMenu = &mod->subMenu;
	guiDeferUpdate(mc);
}

static void clearMenuGameList(opl_io_module_t* mdl) {
	if(mdl->subMenu != NULL) {
		// lock - gui has to be unused here
		guiLock();

		submenuDestroy(&mdl->subMenu);
		mdl->menuItem.submenu = NULL;
		mdl->menuItem.current = NULL;
		mdl->menuItem.pagestart = NULL;
		mdl->menuItem.remindLast = 0;

		// unlock
		guiUnlock();
	}
}

static void initSupport(item_list_t* itemList, int startMode, int mode, int force_reinit) {
	opl_io_module_t* mod = &list_support[mode];
	if (!mod->support) {
		itemList->uip = 1; // stop updates until we're done with init
		mod->support = itemList;
		initMenuForListSupport(mode);
		itemList->uip = 0;
	}

	if (((force_reinit) && (startMode && mod->support->enabled)) \
	  || (startMode == 2 && !mod->support->enabled)) {
		// stop updates until we're done with init of the device
		mod->support->uip = 1;
		mod->support->itemInit();
		moduleUpdateMenu(mode, 0);
		mod->support->uip = 0;

		if (gAutoRefresh)
			RefreshAllLists();
		else
			ioPutRequest(IO_MENU_UPDATE_DEFFERED, &mod->support->mode); // can't use mode as the variable will die at end of execution
	}
}

static void initAllSupport(int force_reinit) {
	if (gUSBStartMode)
		initSupport(usbGetObject(0), gUSBStartMode, USB_MODE, force_reinit);

	if (gETHStartMode)
		initSupport(ethGetObject(0), gETHStartMode, ETH_MODE, force_reinit||(gNetworkStartup >= ERROR_ETH_SMB_CONN));

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

// ----------------------------------------------------------
// ----------------------- Updaters -------------------------
// ----------------------------------------------------------
static void updateMenuFromGameList(opl_io_module_t* mdl) {
	clearMenuGameList(mdl);

	const char* temp = NULL;
	if (gRememberLastPlayed)
		configGetStr(configGetByType(CONFIG_LAST), "last_played", &temp);

	// read the new game list
	struct gui_update_t *gup = NULL;
	int count = mdl->support->itemUpdate();
	if (count) {
		int i;

		for (i = 0; i < count; ++i) {

			gup = guiOpCreate(GUI_OP_APPEND_MENU);

			gup->menu.menu = &mdl->menuItem;
			gup->menu.subMenu = &mdl->subMenu;

			gup->submenu.icon_id = -1;
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

void menuDeferredUpdate(void* data) {
	int* mode = data;

	opl_io_module_t* mod = &list_support[*mode];
	if (!mod->support)
		return;

	if (mod->support->uip) {
		ioPutRequest(IO_MENU_UPDATE_DEFFERED, mode); // we are busy, so re-post refresh for later
		return;
	}

	// see if we have to update
	if (mod->support->itemNeedsUpdate()) {
		mod->support->uip = 1;
		updateMenuFromGameList(mod);
		mod->support->uip = 0;
	}
}

static void RefreshAllLists(void) {
	int i;

	// schedule updates of all the list handlers
	for(i=0; i<MODE_COUNT; i++){
		if (list_support[i].support && list_support[i].support->enabled)
				ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[i].support->mode);
	}

	frameCounter = 0;
}

#define MENU_GENERAL_UPDATE_DELAY	60

static void menuUpdateHook() {
	int i;

	// if timer exceeds some threshold, schedule updates of the available input sources
	frameCounter++;

	// schedule updates of all the list handlers
	if(gAutoRefresh) {
		for(i=0; i<MODE_COUNT; i++){
			if ((list_support[i].support && list_support[i].support->enabled)
				&& ((list_support[i].support->updateDelay>0) && (frameCounter % list_support[i].support->updateDelay == 0))
				)
					ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[i].support->mode);
		}
	}

	// Schedule updates of all list handlers that are to run every frame, regardless of whether auto refresh is active or not.
	if(frameCounter % MENU_GENERAL_UPDATE_DELAY == 0) {
		for(i=0; i<MODE_COUNT; i++){
			if ((list_support[i].support && list_support[i].support->enabled)
				&& (list_support[i].support->updateDelay==0))
					ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[i].support->mode);
		}
	}
}

static void errorMessageHook() {
	guiMsgBox(errorMessage, 0, NULL);

	// reset the original frame hook
	frameCounter = 0;
	guiSetFrameHook(&menuUpdateHook);
}

void setErrorMessageWithCode(int strId, int error) {
	snprintf(errorMessage, sizeof(errorMessage), _l(strId), error);
	guiSetFrameHook(&errorMessageHook);
}

void setErrorMessage(int strId) {
	snprintf(errorMessage, sizeof(errorMessage), _l(strId));
	guiSetFrameHook(&errorMessageHook);
}

// ----------------------------------------------------------
// ------------------ Configuration handling ----------------
// ----------------------------------------------------------

static int lscstatus = CONFIG_ALL;
static int lscret = 0;

static int tryAlternateDevice(int types) {
	char path[64];
	int value;

	// check USB
	if (usbFindPartition(path, "conf_opl.cfg")) {
		configEnd();
		configInit(path);
		value = configReadMulti(types);
		config_set_t *configOPL = configGetByType(CONFIG_OPL);
		configSetInt(configOPL, "usb_mode", 2);
		return value;
	}

	// check HDD
	hddLoadModules();
	sprintf(path, "pfs0:conf_opl.cfg");
	value = fioOpen(path, O_RDONLY);
	if(value >= 0) {
		fioClose(value);
		configEnd();
		configInit("pfs0:");
		value = configReadMulti(types);
		config_set_t *configOPL = configGetByType(CONFIG_OPL);
		configSetInt(configOPL, "hdd_mode", 2);
		return value;
	}

	if (sysCheckMC() < 0) { // We don't want to get users into alternate mode for their very first launch of OPL (i.e no config file at all, but still want to save on MC)
		// set config path to either mass or hdd, to prepare the saving of a new config
		value = fioDopen("mass0:");
		if (value >= 0) {
			fioDclose(value);
			configEnd();
			configInit("mass0:");
		}
		else {
			configEnd();
			configInit("pfs0:");
		}
	}

	return 0;
}

static void _loadConfig() {
	int result = configReadMulti(lscstatus);

	if (lscstatus & CONFIG_OPL) {
		int themeID = -1, langID = -1;

		if (!(result & CONFIG_OPL)) {
			result = tryAlternateDevice(lscstatus);
		}

		if (result & CONFIG_OPL) {
			config_set_t *configOPL = configGetByType(CONFIG_OPL);
			const char *temp;

			configGetInt(configOPL, "scrolling", &gScrollSpeed);
			configGetColor(configOPL, "bg_color", gDefaultBgColor);
			configGetColor(configOPL, "text_color", gDefaultTextColor);
			configGetColor(configOPL, "ui_text_color", gDefaultUITextColor);
			configGetColor(configOPL, "sel_text_color", gDefaultSelTextColor);
			configGetInt(configOPL, "use_info_screen", &gUseInfoScreen);
			configGetInt(configOPL, "enable_coverart", &gEnableArt);
			configGetInt(configOPL, "wide_screen", &gWideScreen);
			configGetInt(configOPL, "vmode", &gVMode);


#ifdef CHEAT
			configGetInt(configOPL, "enable_cheat", &gEnableCheat);
			configGetInt(configOPL, "cheatmode", &gCheatMode);
#endif

			if (configGetStr(configOPL, "theme", &temp))
				themeID = thmFindGuiID(temp);

			if (configGetStr(configOPL, "language_text", &temp))
				langID = lngFindGuiID(temp);

			configGetInt(configOPL, "eth_linkmode", &gETHOpMode);

			if (configGetStr(configOPL, "pc_ip", &temp))
				sscanf(temp, "%d.%d.%d.%d", &pc_ip[0], &pc_ip[1], &pc_ip[2], &pc_ip[3]);

			configGetInt(configOPL, "pc_port", &gPCPort);

			configGetStrCopy(configOPL, "pc_share", gPCShareName, sizeof(gPCShareName));
			configGetStrCopy(configOPL, "pc_user", gPCUserName, sizeof(gPCUserName));
			configGetStrCopy(configOPL, "pc_pass", gPCPassword, sizeof(gPCPassword));
			configGetStrCopy(configOPL, "exit_path", gExitPath, sizeof(gExitPath));

			configGetInt(configOPL, "autosort", &gAutosort);
			configGetInt(configOPL, "autorefresh", &gAutoRefresh);
			configGetInt(configOPL, "default_device", &gDefaultDevice);
			configGetInt(configOPL, "disable_debug", &gDisableDebug);
			configGetInt(configOPL, "enable_delete_rename", &gEnableDandR);
			configGetInt(configOPL, "hdd_spindown", &gHDDSpindown);
			configGetInt(configOPL, "check_usb_frag", &gCheckUSBFragmentation);
			configGetStrCopy(configOPL, "usb_prefix", gUSBPrefix, sizeof(gUSBPrefix));
			configGetStrCopy(configOPL, "eth_prefix", gETHPrefix, sizeof(gETHPrefix));
			configGetInt(configOPL, "remember_last", &gRememberLastPlayed);
#ifdef CHEAT
			configGetInt(configOPL, "show_cheat", &gShowCheat);
#endif
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
		configSetInt(configOPL, "scrolling", gScrollSpeed);
		configSetStr(configOPL, "theme", thmGetValue());
		configSetStr(configOPL, "language_text", lngGetValue());
		configSetColor(configOPL, "bg_color", gDefaultBgColor);
		configSetColor(configOPL, "text_color", gDefaultTextColor);
		configSetColor(configOPL, "ui_text_color", gDefaultUITextColor);
		configSetColor(configOPL, "sel_text_color", gDefaultSelTextColor);
		configSetInt(configOPL, "use_info_screen", gUseInfoScreen);
		configSetInt(configOPL, "enable_coverart", gEnableArt);
		configSetInt(configOPL, "wide_screen", gWideScreen);
		configSetInt(configOPL, "vmode", gVMode);


#ifdef CHEAT
		configSetInt(configOPL, "enable_cheat", gEnableCheat);
		configSetInt(configOPL, "cheatmode", gCheatMode);
#endif

		configSetInt(configOPL, "eth_linkmode", gETHOpMode);
		char temp[256];
		sprintf(temp, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
		configSetStr(configOPL, "pc_ip", temp);
		configSetInt(configOPL, "pc_port", gPCPort);
		configSetStr(configOPL, "pc_share", gPCShareName);
		configSetStr(configOPL, "pc_user", gPCUserName);
		configSetStr(configOPL, "pc_pass", gPCPassword);
		configSetStr(configOPL, "exit_path", gExitPath);
		configSetInt(configOPL, "autosort", gAutosort);
		configSetInt(configOPL, "autorefresh", gAutoRefresh);
		configSetInt(configOPL, "default_device", gDefaultDevice);
		configSetInt(configOPL, "disable_debug", gDisableDebug);
		configSetInt(configOPL, "enable_delete_rename", gEnableDandR);
		configSetInt(configOPL, "hdd_spindown", gHDDSpindown);
		configSetInt(configOPL, "check_usb_frag", gCheckUSBFragmentation);
		configSetStr(configOPL, "usb_prefix", gUSBPrefix);
		configSetStr(configOPL, "eth_prefix", gETHPrefix);
		configSetInt(configOPL, "remember_last", gRememberLastPlayed);
#ifdef CHEAT
		configSetInt(configOPL, "show_cheat", gShowCheat);
#endif
		configSetInt(configOPL, "usb_mode", gUSBStartMode);
		configSetInt(configOPL, "hdd_mode", gHDDStartMode);
		configSetInt(configOPL, "eth_mode", gETHStartMode);
		configSetInt(configOPL, "app_mode", gAPPStartMode);
	}

	lscret = configWriteMulti(lscstatus);
	lscstatus = 0;
}

void applyConfig(int themeID, int langID) {
	if (gDefaultDevice < 0 || gDefaultDevice > APP_MODE)
		gDefaultDevice = APP_MODE;

	guiUpdateScrollSpeed();
	guiUpdateScreenScale();

	guiSetFrameHook(&menuUpdateHook);

	int changed = rmSetMode(0);
	if (changed) {
		// reinit the graphics...
		thmReloadScreenExtents();
		guiReloadScreenExtents();
	}

	// theme must be set after color, and lng after theme
	changed = thmSetGuiValue(themeID, changed);
	if (langID != -1)
		lngSetGuiValue(langID);

	initAllSupport(0);

	menuReinitMainMenu();

	moduleUpdateMenu(USB_MODE, changed);
	moduleUpdateMenu(ETH_MODE, changed);
	moduleUpdateMenu(HDD_MODE, changed);
	moduleUpdateMenu(APP_MODE, changed);
}

int loadConfig(int types) {
	lscstatus = types;
	lscret = 0;

	guiHandleDeferedIO(&lscstatus, _l(_STR_LOADING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_loadConfig);

	return lscret;
}

int saveConfig(int types, int showUI) {
	lscstatus = types;
	lscret = 0;

	guiHandleDeferedIO(&lscstatus, _l(_STR_SAVING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_saveConfig);

	if (showUI) {
		if (lscret)
			guiMsgBox(_l(_STR_SETTINGS_SAVED), 0, NULL);
		else
			guiMsgBox(_l(_STR_ERROR_SAVING_SETTINGS), 0, NULL);
	}

	return lscret;
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

extern void *smap_irx;
extern int size_smap_irx;

extern void *ps2atad_irx;
extern int size_ps2atad_irx;

extern void *ps2hdd_irx;
extern int size_ps2hdd_irx;

extern void *hdldsvr_irx;
extern int size_hdldsvr_irx;

void loadHdldSvr(void) {
	int ret;
	static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";

	// block all io ops, wait for the ones still running to finish
	ioBlockOps(1);

	deinitAllSupport(NO_EXCEPTION);

	unloadPads();

	sysReset(0);

	ret = ethLoadModules();
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

	reset();

	LOG_INIT();
	LOG_ENABLE();

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

void handleHdlSrv() {
	// prepare for hdl, display screen with info
	guiRenderTextScreen(_l(_STR_STARTINGHDL));
	loadHdldSvr();

	guiMsgBox(_l(_STR_RUNNINGHDL), 0, NULL);

	// restore normal functionality again
	guiRenderTextScreen(_l(_STR_UNLOADHDL));
	unloadHdldSvr();
}

// ----------------------------------------------------------
// --------------------- Init/Deinit ------------------------
// ----------------------------------------------------------
static void reset(void) {
	sysReset(SYS_LOAD_MC_MODULES | SYS_LOAD_USB_MODULES | SYS_LOAD_ISOFS_MODULE);

#ifdef _DTL_T10000
	mcInit(MC_TYPE_XMC);
#else
	mcInit(MC_TYPE_MC);
#endif
}

static void moduleCleanup(opl_io_module_t* mod, int exception) {
	if (!mod->support)
		return;

	if (mod->support->itemCleanUp)
		mod->support->itemCleanUp(exception);

	clearMenuGameList(mod);
}

void shutdown(int exception) {
	unloadPads();
	ioEnd();
	guiEnd();
	menuEnd();
	lngEnd();
	thmEnd();
	rmEnd();
	configEnd();

	deinitAllSupport(exception);
}


static void setDefaults(void) {
	clearIOModuleT(&list_support[USB_MODE]);
	clearIOModuleT(&list_support[ETH_MODE]);
	clearIOModuleT(&list_support[HDD_MODE]);
	clearIOModuleT(&list_support[APP_MODE]);

	gBaseMCDir = "mc?:OPL";

	gETHOpMode=ETH_OP_MODE_AUTO;
	ps2_ip[0] = 192; ps2_ip[1] = 168; ps2_ip[2] =  0; ps2_ip[3] =  10;
	ps2_netmask[0] = 255; ps2_netmask[1] = 255; ps2_netmask[2] =  255; ps2_netmask[3] =  0;
	ps2_gateway[0] = 192; ps2_gateway[1] = 168; ps2_gateway[2] = 0; ps2_gateway[3] = 1;
	pc_ip[0] = 192;pc_ip[1] = 168; pc_ip[2] = 0; pc_ip[3] = 2;
	gPCPort = 445;
	gPCShareName[0] = '\0';
	gPCUserName[0] = '\0';
	gPCPassword[0] = '\0';
	gNetworkStartup = ERROR_ETH_NOT_STARTED;
	gHddStartup = 6;
	gHDDSpindown = 20;
	gIPConfigChanged = 0;
	gScrollSpeed = 1;
	gExitPath[0] = '\0';
	gDefaultDevice = APP_MODE;
	gAutosort = 1;
	gAutoRefresh = 0;
	gDisableDebug = 1;
	gEnableDandR = 0;
	gRememberLastPlayed = 0;
#ifdef CHEAT
	gShowCheat = 0;
#endif
	gCheckUSBFragmentation = 1;
	gUSBPrefix[0] = '\0';
	gETHPrefix[0] = '\0';
	gUseInfoScreen = 0;
	gEnableArt = 0;
	gWideScreen = 0;

	gUSBStartMode = 0;
	gHDDStartMode = 0;
	gETHStartMode = 0;
	gAPPStartMode = 0;

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

	frameCounter = 0;

	gVMode = RM_VMODE_AUTO;

#ifdef CHEAT
	gEnableCheat = 0;
	gCheatMode = 0;

	memset(gCheatList, 0, sizeof(gCheatList));
#endif
}

static void init(void) {
	// default variable values
	setDefaults();

	padInit(0);
	configInit(NULL);

	rmInit();
	lngInit();
	thmInit();
	guiInit();
	ioInit();
	menuInit();

	startPads();

	// handler for deffered menu updates
	ioRegisterHandler(IO_MENU_UPDATE_DEFFERED, &menuDeferredUpdate);
	cacheInit();

	InitConsoleRegionData();

	// try to restore config
	_loadConfig();
}

static void deferredInit(void) {

	// inform GUI main init part is over
	struct gui_update_t *id = guiOpCreate(GUI_INIT_DONE);
	guiDeferUpdate(id);

	if (list_support[gDefaultDevice].support) {
		id = guiOpCreate(GUI_OP_SELECT_MENU);
		id->menu.menu = &list_support[gDefaultDevice].menuItem;
		guiDeferUpdate(id);
	}
}

// --------------------- Main --------------------
int main(int argc, char* argv[])
{
	LOG_INIT();
	PREINIT_LOG("OPL GUI start!\n");

	ChangeThreadPriority(GetThreadId(), 31);

	#ifdef __DEBUG
	int use_early_debug = 0, exception_test = 0;
	int i;
	for (i=1; i<argc; i++) {
		if (!(strcmp(argv[i], "-use-early-debug"))) {
			use_early_debug = 1;
			PREINIT_LOG("OPL Using early debug.\n");
		}
		if (!(strcmp(argv[i], "-test-exception"))) {
			exception_test = 1;
			PREINIT_LOG("OPL Exception test requested.\n");
		}
	}
	#endif

	// reset, load modules
	reset();

	#ifdef __DEBUG
	if (use_early_debug) {
		configReadIP();
		debugSetActive();
	}

	if (exception_test) {
		// EXCEPTION test !
		u32 *p = (u32 *)0xDEADC0DE;
		*p = 0xDEFACED;
	}
	#endif

	init();

	// until this point in the code is reached, only PREINIT_LOG macro should be used
	LOG_ENABLE();

	// queue deffered init which shuts down the intro screen later
	ioPutRequest(IO_CUSTOM_SIMPLEACTION, &deferredInit);

	guiIntroLoop();
	guiMainLoop();

	return 0;
}
