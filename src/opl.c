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
#include "include/menusys.h"

// temp
#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"
#include "include/appsupport.h"

#define UPDATE_FRAME_COUNT 250

#define IO_MENU_UPDATE_DEFFERED 2

// forward decl
static void handleHdlSrv();


typedef struct {
	item_list_t *support;
	
	/// menu item used with this list support
	struct menu_item_t menuItem;
	
	/// submenu list
	struct submenu_list_t *subMenu;
} opl_io_module_t;

// forward decl
static void moduleCleanup(opl_io_module_t* mod);

// frame counter
static int gFrameCounter;

static opl_io_module_t list_support[4];

static void initHints(struct menu_item_t* menuItem, int startMode) {
	menuAddHint(menuItem, _STR_SETTINGS, START_ICON);

	if (startMode == 1)
		menuAddHint(menuItem, _STR_START_DEVICE, CROSS_ICON);
	else {
		item_list_t *support = menuItem->userdata;
		if (support->itemGetCompatibility)
			menuAddHint(menuItem, _STR_COMPAT_SETTINGS, TRIANGLE_ICON);
	}
}

static void itemAltExec(struct menu_item_t *self, int id) {
	item_list_t *support = self->userdata;

	if (id < 0)
		return;

	if (support) {
		if (support->itemGetCompatibility)
			guiShowCompatConfig(id, support);
	}
	else
		guiMsgBox("NULL Support object. Please report");
}

static void itemExec(struct menu_item_t *self, int id) {
	item_list_t *support = self->userdata;

	if (support) {
		if (support->enabled) {
			if (id >= 0) {
				shutdown();
				support->itemLaunch(id);
			}
		}
		else { 
                	support->itemInit();
			menuRemoveHints(self); // This is okay since the itemExec is executed from menu (GUI) itself (no need to defer)
			initHints(self, 2);
		}
	}
	else
		guiMsgBox("NULL Support object. Please report");
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

	item->menuItem.altExecute = &itemAltExec;
	item->menuItem.refresh = NULL;
	item->menuItem.execute = &itemExec;

	item->menuItem.hints = NULL;

	initHints(&item->menuItem, startMode);

	struct gui_update_t *mc = guiOpCreate(GUI_OP_ADD_MENU);
	mc->menu.menu = &item->menuItem;
	mc->menu.subMenu = &item->subMenu;
	guiDeferUpdate(mc);
}

static void initSupport(item_list_t* itemList, int startMode, int mode) {
	if (!list_support[mode].support) {
		list_support[mode].support = itemList;
		initMenuForListSupport(&list_support[mode], startMode);
	}

	if (startMode == 2 && !list_support[mode].support->enabled) {
		list_support[mode].support->itemInit();
		gFrameCounter = UPDATE_FRAME_COUNT;
	}
}

static void initAllSupport() {
	int validMode = -1;

	if (gUSBStartMode) {
		validMode = USB_MODE;
		initSupport(usbGetObject(0), gUSBStartMode, USB_MODE);
	}

	if (gETHStartMode) {
		validMode = ETH_MODE;
		initSupport(ethGetObject(0), gETHStartMode, ETH_MODE);
	}

	if (gHDDStartMode) {
		validMode = HDD_MODE;
		initSupport(hddGetObject(0), gHDDStartMode, HDD_MODE);
	}

	if (validMode == -1) // no game device inited, so force Apps ...
		gAPPStartMode = 2;

	if (gAPPStartMode)
		initSupport(appGetObject(0), gAPPStartMode, APP_MODE);
}

static void deinitAllSupport() {
	moduleCleanup(&list_support[USB_MODE]);
	moduleCleanup(&list_support[ETH_MODE]);
	moduleCleanup(&list_support[HDD_MODE]);
	moduleCleanup(&list_support[APP_MODE]);
}

#define MENU_ID_START_HDL 100
#define MENU_ID_START_USB 101
#define MENU_ID_START_HDD 102
#define MENU_ID_START_ETH 103
#define MENU_ID_START_APP 104

void menuExecHookFunc(int id) {
	if (id == MENU_ID_START_HDL) {
		handleHdlSrv();
	}        
}

void menuFillHookFunc(struct submenu_list_t **menu) {
	// TODO: STR_ constants for the strings
	if (gHDDStartMode) // enabled at all?
		submenuAppendItem(menu, DISC_ICON, "Start HDL Server", MENU_ID_START_HDL, -1);
}

// ----------------------------------------------------------
// ------------------ Configuration handling ----------------
// ----------------------------------------------------------
int lscstatus = 0;
int lscret = 0;

void _loadConfig() {
	readIPConfig();

	clearConfig(&gConfig);
	
	char path[255];
	snprintf(path, 255, "%s/conf_opl.cfg", gBaseMCDir);
	int result = readConfig(&gConfig, path);
	
	if (result) {
		char *temp;
		int themeID = -1, langID = -1;

		getConfigInt(&gConfig, "icons_cache_count", &gCountIconsCache);
		getConfigInt(&gConfig, "covers_cache_count", &gCountCoversCache);
		getConfigInt(&gConfig, "bg_cache_count", &gCountBackgroundsCache);
		getConfigInt(&gConfig, "scrolling", &gScrollSpeed);
		getConfigColor(&gConfig, "bg_color", gDefaultBgColor);
		getConfigColor(&gConfig, "text_color", gDefaultTextColor);
		getConfigColor(&gConfig, "ui_text_color", gDefaultUITextColor);
		getConfigColor(&gConfig, "sel_text_color", gDefaultSelTextColor);
		getConfigInt(&gConfig, "exit_mode", &gExitMode);
		getConfigInt(&gConfig, "enable_coverart", &gEnableArt);
		getConfigInt(&gConfig, "wide_screen", &gWideScreen);

		if (getConfigStr(&gConfig, "theme", &temp))
			themeID = thmFindGuiID(temp);

		if (getConfigStr(&gConfig, "language_text", &temp))
			langID = lngFindGuiID(temp);

		if (getConfigStr(&gConfig, "pc_ip", &temp))
			sscanf(temp, "%d.%d.%d.%d", &pc_ip[0], &pc_ip[1], &pc_ip[2], &pc_ip[3]);

		getConfigInt(&gConfig, "pc_port", &gPCPort);
		if (getConfigStr(&gConfig, "pc_share", &temp))
			strncpy(gPCShareName, temp, 32);
		getConfigInt(&gConfig, "autosort", &gAutosort);
		getConfigInt(&gConfig, "default_device", &gDefaultDevice);
		getConfigInt(&gConfig, "disable_debug", &gDisableDebug);
		getConfigInt(&gConfig, "usb_mode", &gUSBStartMode);
		getConfigInt(&gConfig, "hdd_mode", &gHDDStartMode);
		getConfigInt(&gConfig, "eth_mode", &gETHStartMode);
		getConfigInt(&gConfig, "app_mode", &gAPPStartMode);

		applyConfig(themeID, langID);
	}
	
	lscret = result;
	lscstatus = 0;
}

void _saveConfig() {
	if (gIPConfigChanged)
		writeIPConfig();

	setConfigInt(&gConfig, "icons_cache_count", gCountIconsCache);
	setConfigInt(&gConfig, "covers_cache_count", gCountCoversCache);
	setConfigInt(&gConfig, "bg_cache_count", gCountBackgroundsCache);
	setConfigInt(&gConfig, "scrolling", gScrollSpeed);
	setConfigStr(&gConfig, "theme", thmGetValue());
	setConfigStr(&gConfig, "language_text", lngGetValue());
	setConfigColor(&gConfig, "bg_color", gDefaultBgColor);
	setConfigColor(&gConfig, "text_color", gDefaultTextColor);
	setConfigColor(&gConfig, "ui_text_color", gDefaultUITextColor);
	setConfigColor(&gConfig, "sel_text_color", gDefaultSelTextColor);
	setConfigInt(&gConfig, "exit_mode", gExitMode);
	setConfigInt(&gConfig, "enable_coverart", gEnableArt);
	setConfigInt(&gConfig, "wide_screen", gWideScreen);

	char temp[255];
	sprintf(temp, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
	setConfigStr(&gConfig, "pc_ip", temp);
	setConfigInt(&gConfig, "pc_port", gPCPort);
	setConfigStr(&gConfig, "pc_share", gPCShareName);
	setConfigInt(&gConfig, "autosort", gAutosort);
	setConfigInt(&gConfig, "default_device", gDefaultDevice);
	setConfigInt(&gConfig, "disable_debug", gDisableDebug);
	setConfigInt(&gConfig, "usb_mode", gUSBStartMode);
	setConfigInt(&gConfig, "hdd_mode", gHDDStartMode);
	setConfigInt(&gConfig, "eth_mode", gETHStartMode);
	setConfigInt(&gConfig, "app_mode", gAPPStartMode);

	snprintf(temp, 255, "%s/conf_opl.cfg", gBaseMCDir);
	int result = writeConfig(&gConfig, temp);
	
	lscret = result;
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

	initAllSupport();
}

int loadConfig() {
	lscstatus = 1;
	lscret = 0;
	
	guiHandleDefferedIO(&lscstatus, _l(_STR_LOADING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_loadConfig);
	
	return lscret;
}

int saveConfig() {
	lscstatus = 1;
	lscret = 0;
	
	guiHandleDefferedIO(&lscstatus, _l(_STR_SAVING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_saveConfig);
	
	if (lscret) {
		guiMsgBox(_l(_STR_SETTINGS_SAVED));
	} else {
		guiMsgBox(_l(_STR_ERROR_SAVING_SETTINGS));
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
			
			guiDeferUpdate(gup);
		}
	}
	
	gup = guiOpCreate(GUI_OP_SORT);
	gup->menu.menu = &mdl->menuItem;
	gup->menu.subMenu = &mdl->subMenu;
	guiDeferUpdate(gup);
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
	int ret, id;
	static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";
	char ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));

	// block all io ops, wait for the ones still running to finish
	ioBlockOps(1);

	deinitAllSupport();

	unloadPads();

	sysReset();

	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:PADMAN", 0, NULL);

	int iplen = sysSetIPConfig(ipconfig);

    	id=SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
	if ((id < 0) || ret)
		return;

	id=SifExecModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL, &ret);
	if ((id < 0) || ret)
		return;

	id=SifExecModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL, &ret);
	if ((id < 0) || ret)
		return;

	id=SifExecModuleBuffer(&smsmap_irx, size_smsmap_irx, iplen, ipconfig, &ret);
	if ((id < 0) || ret)
		return;

	id=SifExecModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
	if ((id < 0) || ret)
		return;

	id=SifExecModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg, &ret);
	if ((id < 0) || ret)
		return;

	id=SifExecModuleBuffer(&hdldsvr_irx, size_hdldsvr_irx, 0, NULL, &ret);
	if ((id < 0) || ret)
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

	// init all supports again
	initAllSupport();

	// now start io again
	ioBlockOps(0);
}

static void handleHdlSrv() {
	// prepare for hdl, display screen with info
	loadHdldSvr();
	
	// TODO: A propper screen with info display
	// show a message box for now
	guiMsgBox("HDL Server Running...");

	// did someone press circle?
	guiMsgBox("Press circle again to stop HDL...");
	
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

static void moduleCleanup(opl_io_module_t* mod) {
	if (!mod)
		return;

	if (!mod->support)
		return;

	if (mod->support->itemCleanUp)
		mod->support->itemCleanUp();
}

void shutdown() {
	unloadPads();
	ioEnd();
	guiEnd();
	lngEnd();
	thmEnd();
	rmEnd();
	// clearConfig(gConfig);

	deinitAllSupport();
}


static void setDefaults(void) {
	usbd_irx = NULL;
	size_usbd_irx = 0;

	list_support[USB_MODE].support = NULL;
	list_support[USB_MODE].subMenu = NULL;
	
	list_support[ETH_MODE].support = NULL;
	list_support[ETH_MODE].subMenu = NULL;
	
	list_support[HDD_MODE].support = NULL;
	list_support[HDD_MODE].subMenu = NULL;
	
	list_support[APP_MODE].support = NULL;
	list_support[APP_MODE].subMenu = NULL;

	gConfig.head = NULL;
	gConfig.tail = NULL;

	gBaseMCDir = "mc?:OPL";

	ps2_ip[0] = 192; ps2_ip[1] = 168; ps2_ip[2] =  0; ps2_ip[3] =  10;
	ps2_netmask[0] = 255; ps2_netmask[1] = 255; ps2_netmask[2] =  255; ps2_netmask[3] =  0;
	ps2_gateway[0] = 192; ps2_gateway[1] = 168; ps2_gateway[2] = 0; ps2_gateway[3] = 1;
	pc_ip[0] = 192;pc_ip[1] = 168; pc_ip[2] = 0; pc_ip[3] = 2;
	
	// SMB port on pc
	gPCPort = 445;
	
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

	gFrameCounter = UPDATE_FRAME_COUNT; // start refresh asap (risky ?)
}

static void init(void) {
	// default variable values
	setDefaults();

	padInit(0);
	rmInit();
	lngInit();
	thmInit();
	guiInit();
	ioInit();
	
	guiSetMenuFillHook(&menuFillHookFunc);
	guiSetMenuExecHook(&menuExecHookFunc);
	
	// try to restore config
	_loadConfig();
	
	cacheInit();

	// handler for deffered menu updates
	ioRegisterHandler(IO_MENU_UPDATE_DEFFERED, &menuDeferredUpdate);

	startPads();
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
	// reset, load modules
	reset();
	
	init();
	
	// queue deffered init which shuts down the intro screen later
	ioPutRequest(IO_CUSTOM_SIMPLEACTION, &deferredInit);
	
	guiMainLoop();
	
	return 0;
}
