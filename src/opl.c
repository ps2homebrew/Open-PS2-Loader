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
#include "include/compatupd.h"
#include "httpclient.h"

#include "include/supportbase.h"
#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"
#include "include/appsupport.h"

#ifdef CHEAT
#include "include/pgcht.h"
#endif

#ifdef __EESIO_DEBUG
#include <sio.h>
#define LOG_INIT() sio_init(38400, 0, 0, 0, 0)
#define LOG_ENABLE() \
    do {             \
    } while (0)
#else
#ifdef __DEBUG
#include "include/debug.h"
#define LOG_INIT() \
    do {           \
    } while (0)
#define LOG_ENABLE() ioPutRequest(IO_CUSTOM_SIMPLEACTION, &debugSetActive)
#else
#define LOG_INIT() \
    do {           \
    } while (0)
#define LOG_ENABLE() \
    do {             \
    } while (0)
#endif
#endif

static void RefreshAllLists(void);

typedef struct
{
    item_list_t *support;

    /// menu item used with this list support
    menu_item_t menuItem;

    /// submenu list
    submenu_list_t *subMenu;
} opl_io_module_t;

//Network support stuff.
#define HTTP_IOBUF_SIZE 512
static unsigned int CompatUpdateComplete, CompatUpdateTotal;
static unsigned char CompatUpdateStopFlag, CompatUpdateFlags;
static short int CompatUpdateStatus;

static void clearIOModuleT(opl_io_module_t *mod)
{
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
static void clearMenuGameList(opl_io_module_t *mdl);
static void moduleCleanup(opl_io_module_t *mod, int exception);
static void reset(void);

// frame counter
static unsigned int frameCounter;

static char errorMessage[256];

static opl_io_module_t list_support[4];

void moduleUpdateMenu(int mode, int themeChanged)
{
    if (mode == -1)
        return;

    opl_io_module_t *mod = &list_support[mode];

    if (!mod->support)
        return;

    // refresh Hints
    menuRemoveHints(&mod->menuItem);

    menuAddHint(&mod->menuItem, _STR_MENU, START_ICON);
    if (!mod->support->enabled)
        menuAddHint(&mod->menuItem, _STR_START_DEVICE, gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON);
    else {
        menuAddHint(&mod->menuItem, (gUseInfoScreen && gTheme->infoElems.first) ? _STR_INFO : _STR_RUN, gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON);

        if (!(mod->support->flags & MODE_FLAG_NO_COMPAT))
            menuAddHint(&mod->menuItem, _STR_COMPAT_SETTINGS, TRIANGLE_ICON);

        menuAddHint(&mod->menuItem, _STR_REFRESH, SELECT_ICON);

        if (gEnableWrite) {
            if (mod->support->itemRename)
                menuAddHint(&mod->menuItem, _STR_RENAME, gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON);
            if (mod->support->itemDelete)
                menuAddHint(&mod->menuItem, _STR_DELETE, SQUARE_ICON);
        }
    }

    // refresh Cache
    if (themeChanged)
        submenuRebuildCache(mod->subMenu);
}

static void itemExecSelect(struct menu_item *curMenu)
{
    item_list_t *support = curMenu->userdata;

    if (support) {
        if (support->enabled) {
            if (curMenu->current) {
                config_set_t *configSet = menuLoadConfig();
                support->itemLaunch(curMenu->current->item.id, configSet);
            }
        } else {
            support->itemInit();
            moduleUpdateMenu(support->mode, 0);
            if (!gAutoRefresh || (support->updateDelay == MENU_UPD_DELAY_NOUPDATE))
                ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
        }
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecCancel(struct menu_item *curMenu)
{
    if (!curMenu->current)
        return;

    if (!gEnableWrite)
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
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecCross(struct menu_item *curMenu)
{
    if (gSelectButton == KEY_CROSS)
        itemExecSelect(curMenu);
    else
        itemExecCancel(curMenu);
}

static void itemExecTriangle(struct menu_item *curMenu)
{
    if (!curMenu->current)
        return;

    item_list_t *support = curMenu->userdata;

    if (support) {
        if (!(support->flags & MODE_FLAG_NO_COMPAT)) {
            config_set_t *configSet = menuLoadConfig();
            if (guiShowCompatConfig(curMenu->current->item.id, support, configSet) == COMPAT_TEST)
                support->itemLaunch(curMenu->current->item.id, configSet);
        }
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecSquare(struct menu_item *curMenu)
{
    if (!curMenu->current)
        return;

    if (!gEnableWrite)
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
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void itemExecCircle(struct menu_item *curMenu)
{
    if (gSelectButton == KEY_CIRCLE)
        itemExecSelect(curMenu);
    else
        itemExecCancel(curMenu);
}

static void itemExecRefresh(struct menu_item *curMenu)
{
    item_list_t *support = curMenu->userdata;

    if (support && support->enabled)
        ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
}

static void initMenuForListSupport(int mode)
{
    opl_io_module_t *mod = &list_support[mode];
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

static void clearMenuGameList(opl_io_module_t *mdl)
{
    if (mdl->subMenu != NULL) {
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

static void initSupport(item_list_t *itemList, int startMode, int mode, int force_reinit)
{
    opl_io_module_t *mod = &list_support[mode];

    if (startMode) {
        if (!mod->support) {
            mod->support = itemList;
            initMenuForListSupport(mode);
        }

        if (((force_reinit) && (startMode && mod->support->enabled)) || (startMode == START_MODE_AUTO && !mod->support->enabled)) {
            mod->support->itemInit();
            moduleUpdateMenu(mode, 0);

            if (gAutoRefresh)
                RefreshAllLists();
            else
                ioPutRequest(IO_MENU_UPDATE_DEFFERED, &mod->support->mode); // can't use mode as the variable will die at end of execution
        }
    }
}

static void initAllSupport(int force_reinit)
{
    initSupport(usbGetObject(0), gUSBStartMode, USB_MODE, force_reinit);
    initSupport(ethGetObject(0), gETHStartMode, ETH_MODE, force_reinit || (gNetworkStartup >= ERROR_ETH_SMB_CONN));
    initSupport(hddGetObject(0), gHDDStartMode, HDD_MODE, force_reinit);
    initSupport(appGetObject(0), gAPPStartMode, APP_MODE, force_reinit);
}

static void deinitAllSupport(int exception)
{
    moduleCleanup(&list_support[USB_MODE], exception);
    moduleCleanup(&list_support[ETH_MODE], exception);
    moduleCleanup(&list_support[HDD_MODE], exception);
    moduleCleanup(&list_support[APP_MODE], exception);
}

// ----------------------------------------------------------
// ----------------------- Updaters -------------------------
// ----------------------------------------------------------
static void updateMenuFromGameList(opl_io_module_t *mdl)
{
    guiExecDeferredOps();
    clearMenuGameList(mdl);

    const char *temp = NULL;
    if (gRememberLastPlayed)
        configGetStr(configGetByType(CONFIG_LAST), "last_played", &temp);

    // read the new game list
    struct gui_update_t *gup = NULL;
    int count = mdl->support->itemUpdate();
    if (count > 0) {
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

            if (gRememberLastPlayed && temp && strcmp(temp, mdl->support->itemGetStartup(i)) == 0) {
                gup->submenu.selected = 1; //Select Last Played Game
            }

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

void menuDeferredUpdate(void *data)
{
    short int *mode = data;

    opl_io_module_t *mod = &list_support[*mode];
    if (!mod->support)
        return;

    // see if we have to update
    if (mod->support->itemNeedsUpdate()) {
        updateMenuFromGameList(mod);
    }
}

static void RefreshAllLists(void)
{
    int i;

    // schedule updates of all the list handlers
    for (i = 0; i < MODE_COUNT; i++) {
        if (list_support[i].support && list_support[i].support->enabled)
            ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[i].support->mode);
    }

    frameCounter = 0;
}

#define MENU_GENERAL_UPDATE_DELAY 60

static void menuUpdateHook()
{
    int i;

    // if timer exceeds some threshold, schedule updates of the available input sources
    frameCounter++;

    // schedule updates of all the list handlers
    if (gAutoRefresh) {
        for (i = 0; i < MODE_COUNT; i++) {
            if ((list_support[i].support && list_support[i].support->enabled) && ((list_support[i].support->updateDelay > 0) && (frameCounter % list_support[i].support->updateDelay == 0)))
                ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[i].support->mode);
        }
    }

    // Schedule updates of all list handlers that are to run every frame, regardless of whether auto refresh is active or not.
    if (frameCounter % MENU_GENERAL_UPDATE_DELAY == 0) {
        for (i = 0; i < MODE_COUNT; i++) {
            if ((list_support[i].support && list_support[i].support->enabled) && (list_support[i].support->updateDelay == 0))
                ioPutRequest(IO_MENU_UPDATE_DEFFERED, &list_support[i].support->mode);
        }
    }
}

static void clearErrorMessage(void)
{
    // reset the original frame hook
    frameCounter = 0;
    guiSetFrameHook(&menuUpdateHook);
}

static void errorMessageHook()
{
    guiMsgBox(errorMessage, 0, NULL);
    clearErrorMessage();
}

void setErrorMessageWithCode(int strId, int error)
{
    snprintf(errorMessage, sizeof(errorMessage), _l(strId), error);
    guiSetFrameHook(&errorMessageHook);
}

void setErrorMessage(int strId)
{
    snprintf(errorMessage, sizeof(errorMessage), _l(strId));
    guiSetFrameHook(&errorMessageHook);
}

// ----------------------------------------------------------
// ------------------ Configuration handling ----------------
// ----------------------------------------------------------

static int lscstatus = CONFIG_ALL;
static int lscret = 0;

static int tryAlternateDevice(int types)
{
    char path[64];
    int value;

    // check USB
    if (usbFindPartition(path, "conf_opl.cfg")) {
        configEnd();
        configInit(path);
        value = configReadMulti(types);
        config_set_t *configOPL = configGetByType(CONFIG_OPL);
        configSetInt(configOPL, CONFIG_OPL_USB_MODE, START_MODE_AUTO);
        return value;
    }

    // check HDD
    hddLoadModules();
    sprintf(path, "pfs0:conf_opl.cfg");
    value = fileXioOpen(path, O_RDONLY, 0666);
    if (value >= 0) {
        fileXioClose(value);
        configEnd();
        configInit("pfs0:");
        value = configReadMulti(types);
        config_set_t *configOPL = configGetByType(CONFIG_OPL);
        configSetInt(configOPL, CONFIG_OPL_HDD_MODE, START_MODE_AUTO);
        return value;
    }

    if (sysCheckMC() < 0) { // We don't want to get users into alternate mode for their very first launch of OPL (i.e no config file at all, but still want to save on MC)
        // set config path to either mass or hdd, to prepare the saving of a new config
        value = fileXioDopen("mass0:");
        if (value >= 0) {
            fileXioDclose(value);
            configEnd();
            configInit("mass0:");
        } else {
            configEnd();
            configInit("pfs0:");
        }
    }

    return 0;
}

static void _loadConfig()
{
    int value, themeID = -1, langID = -1;
    const char *temp;
    int result = configReadMulti(lscstatus);

    if (lscstatus & CONFIG_OPL) {
        if (!(result & CONFIG_OPL)) {
            result = tryAlternateDevice(lscstatus);
        }

        if (result & CONFIG_OPL) {
            config_set_t *configOPL = configGetByType(CONFIG_OPL);

            configGetInt(configOPL, CONFIG_OPL_SCROLLING, &gScrollSpeed);
            configGetColor(configOPL, CONFIG_OPL_BGCOLOR, gDefaultBgColor);
            configGetColor(configOPL, CONFIG_OPL_TEXTCOLOR, gDefaultTextColor);
            configGetColor(configOPL, CONFIG_OPL_UI_TEXTCOLOR, gDefaultUITextColor);
            configGetColor(configOPL, CONFIG_OPL_SEL_TEXTCOLOR, gDefaultSelTextColor);
            configGetInt(configOPL, CONFIG_OPL_USE_INFOSCREEN, &gUseInfoScreen);
            configGetInt(configOPL, CONFIG_OPL_ENABLE_COVERART, &gEnableArt);
            configGetInt(configOPL, CONFIG_OPL_WIDESCREEN, &gWideScreen);
            configGetInt(configOPL, CONFIG_OPL_VMODE, &gVMode);
            configGetInt(configOPL, CONFIG_OPL_XOFF, &gXOff);
            configGetInt(configOPL, CONFIG_OPL_YOFF, &gYOff);

            if (configGetStr(configOPL, CONFIG_OPL_THEME, &temp))
                themeID = thmFindGuiID(temp);

            if (configGetStr(configOPL, CONFIG_OPL_LANGUAGE, &temp))
                langID = lngFindGuiID(temp);

            if (configGetInt(configOPL, CONFIG_OPL_SWAP_SEL_BUTTON, &value))
                gSelectButton = value == 0 ? KEY_CIRCLE : KEY_CROSS;

            configGetInt(configOPL, CONFIG_OPL_DISABLE_DEBUG, &gDisableDebug);
            configGetInt(configOPL, CONFIG_OPL_PS2LOGO, &gPS2Logo);
            configGetStrCopy(configOPL, CONFIG_OPL_EXIT_PATH, gExitPath, sizeof(gExitPath));
            configGetInt(configOPL, CONFIG_OPL_AUTO_SORT, &gAutosort);
            configGetInt(configOPL, CONFIG_OPL_AUTO_REFRESH, &gAutoRefresh);
            configGetInt(configOPL, CONFIG_OPL_DEFAULT_DEVICE, &gDefaultDevice);
            configGetInt(configOPL, CONFIG_OPL_ENABLE_WRITE, &gEnableWrite);
            configGetInt(configOPL, CONFIG_OPL_HDD_SPINDOWN, &gHDDSpindown);
            configGetInt(configOPL, CONFIG_OPL_USB_CHECK_FRAG, &gCheckUSBFragmentation);
            configGetStrCopy(configOPL, CONFIG_OPL_USB_PREFIX, gUSBPrefix, sizeof(gUSBPrefix));
            configGetStrCopy(configOPL, CONFIG_OPL_ETH_PREFIX, gETHPrefix, sizeof(gETHPrefix));
            configGetInt(configOPL, CONFIG_OPL_REMEMBER_LAST, &gRememberLastPlayed);
            configGetInt(configOPL, CONFIG_OPL_AUTOSTART_LAST, &gAutoStartLastPlayed);
            configGetInt(configOPL, CONFIG_OPL_USB_MODE, &gUSBStartMode);
            configGetInt(configOPL, CONFIG_OPL_HDD_MODE, &gHDDStartMode);
            configGetInt(configOPL, CONFIG_OPL_ETH_MODE, &gETHStartMode);
            configGetInt(configOPL, CONFIG_OPL_APP_MODE, &gAPPStartMode);
        }
    }

    if (lscstatus & CONFIG_NETWORK) {
        if (!(result & CONFIG_NETWORK)) {
            result = tryAlternateDevice(lscstatus);
        }

        if (result & CONFIG_NETWORK) {
            config_set_t *configNet = configGetByType(CONFIG_NETWORK);

            configGetInt(configNet, CONFIG_NET_ETH_LINKM, &gETHOpMode);

            configGetInt(configNet, CONFIG_NET_PS2_DHCP, &ps2_ip_use_dhcp);
            configGetInt(configNet, CONFIG_NET_SMB_NBNS, &gPCShareAddressIsNetBIOS);
            configGetStrCopy(configNet, CONFIG_NET_SMB_NB_ADDR, gPCShareNBAddress, sizeof(gPCShareNBAddress));

            if (configGetStr(configNet, CONFIG_NET_SMB_IP_ADDR, &temp))
                sscanf(temp, "%d.%d.%d.%d", &pc_ip[0], &pc_ip[1], &pc_ip[2], &pc_ip[3]);

            configGetInt(configNet, CONFIG_NET_SMB_PORT, &gPCPort);

            configGetStrCopy(configNet, CONFIG_NET_SMB_SHARE, gPCShareName, sizeof(gPCShareName));
            configGetStrCopy(configNet, CONFIG_NET_SMB_USER, gPCUserName, sizeof(gPCUserName));
            configGetStrCopy(configNet, CONFIG_NET_SMB_PASSW, gPCPassword, sizeof(gPCPassword));

            if (configGetStr(configNet, CONFIG_NET_PS2_IP, &temp))
                sscanf(temp, "%d.%d.%d.%d", &ps2_ip[0], &ps2_ip[1], &ps2_ip[2], &ps2_ip[3]);
            if (configGetStr(configNet, CONFIG_NET_PS2_NETM, &temp))
                sscanf(temp, "%d.%d.%d.%d", &ps2_netmask[0], &ps2_netmask[1], &ps2_netmask[2], &ps2_netmask[3]);
            if (configGetStr(configNet, CONFIG_NET_PS2_GATEW, &temp))
                sscanf(temp, "%d.%d.%d.%d", &ps2_gateway[0], &ps2_gateway[1], &ps2_gateway[2], &ps2_gateway[3]);
            if (configGetStr(configNet, CONFIG_NET_PS2_DNS, &temp))
                sscanf(temp, "%d.%d.%d.%d", &ps2_dns[0], &ps2_dns[1], &ps2_dns[2], &ps2_dns[3]);
        }
    }

    applyConfig(themeID, langID);

    lscret = result;
    lscstatus = 0;
}

static void _saveConfig()
{
    char temp[256];

    if (lscstatus & CONFIG_OPL) {
        config_set_t *configOPL = configGetByType(CONFIG_OPL);
        configSetInt(configOPL, CONFIG_OPL_SCROLLING, gScrollSpeed);
        configSetStr(configOPL, CONFIG_OPL_THEME, thmGetValue());
        configSetStr(configOPL, CONFIG_OPL_LANGUAGE, lngGetValue());
        configSetColor(configOPL, CONFIG_OPL_BGCOLOR, gDefaultBgColor);
        configSetColor(configOPL, CONFIG_OPL_TEXTCOLOR, gDefaultTextColor);
        configSetColor(configOPL, CONFIG_OPL_UI_TEXTCOLOR, gDefaultUITextColor);
        configSetColor(configOPL, CONFIG_OPL_SEL_TEXTCOLOR, gDefaultSelTextColor);
        configSetInt(configOPL, CONFIG_OPL_USE_INFOSCREEN, gUseInfoScreen);
        configSetInt(configOPL, CONFIG_OPL_ENABLE_COVERART, gEnableArt);
        configSetInt(configOPL, CONFIG_OPL_WIDESCREEN, gWideScreen);
        configSetInt(configOPL, CONFIG_OPL_VMODE, gVMode);
        configSetInt(configOPL, CONFIG_OPL_XOFF, gXOff);
        configSetInt(configOPL, CONFIG_OPL_YOFF, gYOff);
        configSetInt(configOPL, CONFIG_OPL_DISABLE_DEBUG, gDisableDebug);
        configSetInt(configOPL, CONFIG_OPL_PS2LOGO, gPS2Logo);
        configSetStr(configOPL, CONFIG_OPL_EXIT_PATH, gExitPath);
        configSetInt(configOPL, CONFIG_OPL_AUTO_SORT, gAutosort);
        configSetInt(configOPL, CONFIG_OPL_AUTO_REFRESH, gAutoRefresh);
        configSetInt(configOPL, CONFIG_OPL_DEFAULT_DEVICE, gDefaultDevice);
        configSetInt(configOPL, CONFIG_OPL_ENABLE_WRITE, gEnableWrite);
        configSetInt(configOPL, CONFIG_OPL_HDD_SPINDOWN, gHDDSpindown);
        configSetInt(configOPL, CONFIG_OPL_USB_CHECK_FRAG, gCheckUSBFragmentation);
        configSetStr(configOPL, CONFIG_OPL_USB_PREFIX, gUSBPrefix);
        configSetStr(configOPL, CONFIG_OPL_ETH_PREFIX, gETHPrefix);
        configSetInt(configOPL, CONFIG_OPL_REMEMBER_LAST, gRememberLastPlayed);
        configSetInt(configOPL, CONFIG_OPL_AUTOSTART_LAST, gAutoStartLastPlayed);
        configSetInt(configOPL, CONFIG_OPL_USB_MODE, gUSBStartMode);
        configSetInt(configOPL, CONFIG_OPL_HDD_MODE, gHDDStartMode);
        configSetInt(configOPL, CONFIG_OPL_ETH_MODE, gETHStartMode);
        configSetInt(configOPL, CONFIG_OPL_APP_MODE, gAPPStartMode);

        configSetInt(configOPL, CONFIG_OPL_SWAP_SEL_BUTTON, gSelectButton == KEY_CIRCLE ? 0 : 1);
    }

    if (lscstatus & CONFIG_NETWORK) {
        config_set_t *configNet = configGetByType(CONFIG_NETWORK);

        snprintf(temp, sizeof(temp), "%d.%d.%d.%d", ps2_ip[0], ps2_ip[1], ps2_ip[2], ps2_ip[3]);
        configSetStr(configNet, CONFIG_NET_PS2_IP, temp);
        snprintf(temp, sizeof(temp), "%d.%d.%d.%d", ps2_netmask[0], ps2_netmask[1], ps2_netmask[2], ps2_netmask[3]);
        configSetStr(configNet, CONFIG_NET_PS2_NETM, temp);
        snprintf(temp, sizeof(temp), "%d.%d.%d.%d", ps2_gateway[0], ps2_gateway[1], ps2_gateway[2], ps2_gateway[3]);
        configSetStr(configNet, CONFIG_NET_PS2_GATEW, temp);
        snprintf(temp, sizeof(temp), "%d.%d.%d.%d", ps2_dns[0], ps2_dns[1], ps2_dns[2], ps2_dns[3]);
        configSetStr(configNet, CONFIG_NET_PS2_DNS, temp);

        configSetInt(configNet, CONFIG_NET_ETH_LINKM, gETHOpMode);
        configSetInt(configNet, CONFIG_NET_PS2_DHCP, ps2_ip_use_dhcp);
        configSetInt(configNet, CONFIG_NET_SMB_NBNS, gPCShareAddressIsNetBIOS);
        configSetStr(configNet, CONFIG_NET_SMB_NB_ADDR, gPCShareNBAddress);
        snprintf(temp, sizeof(temp), "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
        configSetStr(configNet, CONFIG_NET_SMB_IP_ADDR, temp);
        configSetInt(configNet, CONFIG_NET_SMB_PORT, gPCPort);
        configSetStr(configNet, CONFIG_NET_SMB_SHARE, gPCShareName);
        configSetStr(configNet, CONFIG_NET_SMB_USER, gPCUserName);
        configSetStr(configNet, CONFIG_NET_SMB_PASSW, gPCPassword);
    }

    lscret = configWriteMulti(lscstatus);
    lscstatus = 0;
}

void applyConfig(int themeID, int langID)
{
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

int loadConfig(int types)
{
    lscstatus = types;
    lscret = 0;

    guiHandleDeferedIO(&lscstatus, _l(_STR_LOADING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_loadConfig);

    return lscret;
}

int saveConfig(int types, int showUI)
{
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

#define COMPAT_UPD_MODE_UPD_USR 1   //Update all records, even those that were modified by the user.
#define COMPAT_UPD_MODE_NO_MTIME 2  //Do not check the modified time-stamp.
#define COMPAT_UPD_MODE_MTIME_GMT 4 //Modified time-stamp is in GMT, not JST.

#define EOPLCONNERR 0x4000 //Special error code for connection errors.

static int CompatAttemptConnection(void)
{
    unsigned char retries;
    int HttpSocket;

    for (retries = OPL_COMPAT_HTTP_RETRIES, HttpSocket = -1; !CompatUpdateStopFlag && retries > 0; retries--) {
        if ((HttpSocket = HttpEstabConnection(OPL_COMPAT_HTTP_HOST, OPL_COMPAT_HTTP_PORT)) >= 0) {
            break;
        }
    }

    return HttpSocket;
}

static void compatUpdate(item_list_t *support, unsigned char mode, config_set_t *configSet, int id)
{
    sceCdCLOCK clock;
    config_set_t *itemConfig, *downloadedConfig;
    u16 length;
    s8 ConnMode, hasMtime;
    u8 *HttpBuffer;
    int i, count, HttpSocket, result, retries, ConfigSource;
    iox_stat_t stat;
    u8 mtime[6];
    char device, uri[64];
    const char *startup;

    switch (support->mode) {
        case USB_MODE:
            device = 3;
            break;
        case ETH_MODE:
            mode |= COMPAT_UPD_MODE_MTIME_GMT;
            device = 2;
            break;
        case HDD_MODE:
            device = 1;
            break;
        default:
            device = -1;
    }

    if (device < 0) {
        LOG("CompatUpdate: unrecognized mode: %d\n", support->mode);
        CompatUpdateStatus = OPL_COMPAT_UPDATE_STAT_ERROR;
        return; //Shouldn't happen, but what if?
    }

    result = 0;
    LOG("CompatUpdate: updating for: device %d game %d\n", device, configSet == NULL ? -1 : id);

    if ((HttpBuffer = memalign(64, HTTP_IOBUF_SIZE)) != NULL) {
        count = configSet != NULL ? 1 : support->itemGetCount();

        if (count > 0) {
            ConnMode = HTTP_CMODE_PERSISTENT;
            if ((HttpSocket = CompatAttemptConnection()) >= 0) {
                //Update compatibility list.
                for (i = 0; !CompatUpdateStopFlag && result >= 0 && i < count; i++, CompatUpdateComplete++) {
                    startup = support->itemGetStartup(configSet != NULL ? id : i);

                    if (ConnMode == HTTP_CMODE_CLOSED) {
                        ConnMode = HTTP_CMODE_PERSISTENT;
                        if ((HttpSocket = CompatAttemptConnection()) < 0) {
                            result = HttpSocket | EOPLCONNERR;
                            break;
                        }
                    }

                    itemConfig = configSet != NULL ? configSet : support->itemGetConfig(i);
                    if (itemConfig != NULL) {
                        ConfigSource = CONFIG_SOURCE_DEFAULT;
                        if ((mode & COMPAT_UPD_MODE_UPD_USR) || !configGetInt(itemConfig, CONFIG_ITEM_CONFIGSOURCE, &ConfigSource) || ConfigSource != CONFIG_SOURCE_USER) {
                            if (!(mode & COMPAT_UPD_MODE_NO_MTIME) && (ConfigSource == CONFIG_SOURCE_DLOAD) && configGetStat(itemConfig, &stat)) { //Only perform a stat operation for downloaded setting files.
                                if (!(mode & COMPAT_UPD_MODE_MTIME_GMT)) {
                                    clock.second = itob(stat.mtime[1]);
                                    clock.minute = itob(stat.mtime[2]);
                                    clock.hour = itob(stat.mtime[3]);
                                    clock.day = itob(stat.mtime[4]);
                                    clock.month = itob(stat.mtime[5]);
                                    clock.year = itob((stat.mtime[6] | ((unsigned short int)stat.mtime[7] << 8)) - 2000);
                                    configConvertToGmtTime(&clock);

                                    mtime[0] = btoi(clock.year);      //Year
                                    mtime[1] = btoi(clock.month) - 1; //Month
                                    mtime[2] = btoi(clock.day) - 1;   //Day
                                    mtime[3] = btoi(clock.hour);      //Hour
                                    mtime[4] = btoi(clock.minute);    //Minute
                                    mtime[5] = btoi(clock.second);    //Second
                                } else {
                                    mtime[0] = (stat.mtime[6] | ((unsigned short int)stat.mtime[7] << 8)) - 2000; //Year
                                    mtime[1] = stat.mtime[5] - 1;                                                 //Month
                                    mtime[2] = stat.mtime[4] - 1;                                                 //Day
                                    mtime[3] = stat.mtime[3];                                                     //Hour
                                    mtime[4] = stat.mtime[2];                                                     //Minute
                                    mtime[5] = stat.mtime[1];                                                     //Second
                                }
                                hasMtime = 1;

                                LOG("CompatUpdate: LAST MTIME %04u/%02u/%02u %02u:%02u:%02u\n", (unsigned short int)mtime[0] + 2000, mtime[1] + 1, mtime[2] + 1, mtime[3], mtime[4], mtime[5]);
                            } else {
                                hasMtime = 0;
                            }

                            sprintf(uri, OPL_COMPAT_HTTP_URI, startup, device);
                            for (retries = OPL_COMPAT_HTTP_RETRIES; !CompatUpdateStopFlag && retries > 0; retries--) {
                                length = HTTP_IOBUF_SIZE;
                                result = HttpSendGetRequest(HttpSocket, OPL_USER_AGENT, OPL_COMPAT_HTTP_HOST, &ConnMode, hasMtime ? mtime : NULL, uri, HttpBuffer, &length);
                                if (result >= 0) {
                                    if (result == 200) {
                                        if ((downloadedConfig = configAlloc(0, NULL, NULL)) != NULL) {
                                            configReadBuffer(downloadedConfig, HttpBuffer, length);
                                            configMerge(itemConfig, downloadedConfig);
                                            configFree(downloadedConfig);
                                            configSetInt(itemConfig, CONFIG_ITEM_CONFIGSOURCE, CONFIG_SOURCE_DLOAD);
                                            if (!configWrite(itemConfig))
                                                result = -EIO;
                                        } else
                                            result = -ENOMEM;
                                    }

                                    break;
                                } else
                                    result |= EOPLCONNERR;

                                HttpCloseConnection(HttpSocket);

                                LOG("CompatUpdate: Connection lost. Retrying.\n");

                                //Connection lost. Attempt to re-connect.
                                ConnMode = HTTP_CMODE_PERSISTENT;
                                if ((HttpSocket = CompatAttemptConnection()) < 0) {
                                    result = HttpSocket | EOPLCONNERR;
                                    break;
                                }
                            }

                            LOG("CompatUpdate %d. %d, %s: %s %d\n", i + 1, device, startup, ConnMode == HTTP_CMODE_CLOSED ? "CLOSED" : "PERSISTENT", result);
                        } else {
                            LOG("CompatUpdate: skipping %s\n", startup);
                        }

                        if (configSet == NULL) //Do not free what is not ours.
                            configFree(itemConfig);
                    } else {
                        //Can't do anything because the config file cannot be opened/created.
                        LOG("CompatUpdate: skipping %s (no config)\n", startup);
                    }

                    if (ConnMode == HTTP_CMODE_CLOSED)
                        HttpCloseConnection(HttpSocket);
                }

                if (ConnMode == HTTP_CMODE_PERSISTENT)
                    HttpCloseConnection(HttpSocket);
            } else {
                result = HttpSocket | EOPLCONNERR;
            }
        }

        free(HttpBuffer);
    } else {
        result = -ENOMEM;
    }

    if (CompatUpdateStopFlag)
        CompatUpdateStatus = OPL_COMPAT_UPDATE_STAT_ABORTED;
    else {
        if (result >= 0)
            CompatUpdateStatus = OPL_COMPAT_UPDATE_STAT_DONE;
        else {
            CompatUpdateStatus = (result & EOPLCONNERR) ? OPL_COMPAT_UPDATE_STAT_CONN_ERROR : OPL_COMPAT_UPDATE_STAT_ERROR;
        }
    }
    LOG("CompatUpdate: completed with status %d\n", CompatUpdateStatus);
}

static void compatDeferredUpdate(void *data)
{
    opl_io_module_t *mod = &list_support[*(short int *)data];

    compatUpdate(mod->support, CompatUpdateFlags, NULL, -1);
}

int oplGetUpdateGameCompatStatus(unsigned int *done, unsigned int *total)
{
    *done = CompatUpdateComplete;
    *total = CompatUpdateTotal;
    return CompatUpdateStatus;
}

void oplAbortUpdateGameCompat(void)
{
    CompatUpdateStopFlag = 1;
    ioRemoveRequests(IO_COMPAT_UPDATE_DEFFERED);
}

void oplUpdateGameCompat(int UpdateAll)
{
    int i, started, count;

    CompatUpdateTotal = 0;
    CompatUpdateComplete = 0;
    CompatUpdateStopFlag = 0;
    CompatUpdateFlags = UpdateAll ? (COMPAT_UPD_MODE_NO_MTIME | COMPAT_UPD_MODE_UPD_USR) : 0;
    CompatUpdateStatus = OPL_COMPAT_UPDATE_STAT_WIP;

    // Schedule compatibility updates of all the list handlers
    for (i = 0, started = 0; i < MODE_COUNT; i++) {
        if (list_support[i].support && list_support[i].support->enabled && !(list_support[i].support->flags & MODE_FLAG_NO_UPDATE) && (count = list_support[i].support->itemGetCount()) > 0) {
            CompatUpdateTotal += count;
            ioPutRequest(IO_COMPAT_UPDATE_DEFFERED, &list_support[i].support->mode);
            started++;

            LOG("CompatUpdate: started for mode %d (%d games)\n", list_support[i].support->mode, count);
        }
    }

    if (started < 1) //Nothing done
        CompatUpdateStatus = OPL_COMPAT_UPDATE_STAT_DONE;
}

static int CompatUpdSingleID, CompatUpdSingleStatus;
static item_list_t *CompatUpdSingleSupport;
static config_set_t *CompatUpdSingleConfigSet;

static void _updateCompatSingle(void)
{
    compatUpdate(CompatUpdSingleSupport, COMPAT_UPD_MODE_UPD_USR, CompatUpdSingleConfigSet, CompatUpdSingleID);
    CompatUpdSingleStatus = 0;
}

int oplUpdateGameCompatSingle(int id, item_list_t *support, config_set_t *configSet)
{
    CompatUpdateStopFlag = 0;
    CompatUpdateStatus = OPL_COMPAT_UPDATE_STAT_WIP;
    CompatUpdateTotal = 1;
    CompatUpdateComplete = 0;
    CompatUpdSingleID = id;
    CompatUpdSingleSupport = support;
    CompatUpdSingleConfigSet = configSet;
    CompatUpdSingleStatus = 1;

    guiHandleDeferedIO(&CompatUpdSingleStatus, _l(_STR_PLEASE_WAIT), IO_CUSTOM_SIMPLEACTION, &_updateCompatSingle);

    return CompatUpdateStatus;
}

// ----------------------------------------------------------
// -------------------- HD SRV Support ----------------------
// ----------------------------------------------------------
extern void *ps2atad_irx;
extern int size_ps2atad_irx;

extern void *ps2hdd_irx;
extern int size_ps2hdd_irx;

extern void *hdldsvr_irx;
extern int size_hdldsvr_irx;

static int loadHdldSvr(void)
{
    int ret, padStatus;
    static char hddarg[] = "-o"
                           "\0"
                           "4"
                           "\0"
                           "-n"
                           "\0"
                           "20";

    // block all io ops, wait for the ones still running to finish
    ioBlockOps(1);
    guiExecDeferredOps();

    deinitAllSupport(NO_EXCEPTION);
    clearErrorMessage(); /*	At this point, an error might have been displayed (since background tasks were completed).
					Clear it, otherwise it will get displayed after the server is closed.	*/

    unloadPads();
    sysReset(0);

    ret = ethLoadInitModules();
    if (ret == 0) {
        ret = sysLoadModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL);
        if (ret >= 0) {
            ret = sysLoadModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg);
            if (ret >= 0) {
                ret = sysLoadModuleBuffer(&hdldsvr_irx, size_hdldsvr_irx, 0, NULL);
                if (ret >= 0)
                    ret = 0;
            }
        }
    }

    padInit(0);

    // init all pads
    padStatus = 0;
    while (!padStatus)
        padStatus = startPads();

    // now ready to display some status

    return ret;
}

static void unloadHdldSvr(void)
{
    ethDeinitModules();
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

void handleHdlSrv()
{
    // prepare for hdl, display screen with info
    guiRenderTextScreen(_l(_STR_STARTINGHDL));
    if (loadHdldSvr() == 0)
        guiMsgBox(_l(_STR_RUNNINGHDL), 0, NULL);
    else
        guiMsgBox(_l(_STR_STARTFAILHDL), 0, NULL);

    // restore normal functionality again
    guiRenderTextScreen(_l(_STR_UNLOADHDL));
    unloadHdldSvr();
}

// ----------------------------------------------------------
// --------------------- Init/Deinit ------------------------
// ----------------------------------------------------------
static void reset(void)
{
    sysReset(SYS_LOAD_MC_MODULES | SYS_LOAD_USB_MODULES | SYS_LOAD_ISOFS_MODULE);

#ifdef _DTL_T10000
    mcInit(MC_TYPE_XMC);
#else
    mcInit(MC_TYPE_MC);
#endif
}

static void moduleCleanup(opl_io_module_t *mod, int exception)
{
    if (!mod->support)
        return;

    if (mod->support->itemCleanUp)
        mod->support->itemCleanUp(exception);

    clearMenuGameList(mod);
}

void deinit(int exception)
{
    // Just deinit them if we won't show Debug Warnings later
    if (gDisableDebug) {
        unloadPads();
        ioEnd();
        guiEnd();
        menuEnd();
        lngEnd();
        thmEnd();
        rmEnd();
    }
    configEnd();

    deinitAllSupport(exception);
}

static void setDefaults(void)
{
    clearIOModuleT(&list_support[USB_MODE]);
    clearIOModuleT(&list_support[ETH_MODE]);
    clearIOModuleT(&list_support[HDD_MODE]);
    clearIOModuleT(&list_support[APP_MODE]);

    gBaseMCDir = "mc?:OPL";

    ps2_ip_use_dhcp = 1;
    gETHOpMode = ETH_OP_MODE_AUTO;
    gPCShareAddressIsNetBIOS = 1;
    gPCShareNBAddress[0] = '\0';
    ps2_ip[0] = 192;
    ps2_ip[1] = 168;
    ps2_ip[2] = 0;
    ps2_ip[3] = 10;
    ps2_netmask[0] = 255;
    ps2_netmask[1] = 255;
    ps2_netmask[2] = 255;
    ps2_netmask[3] = 0;
    ps2_gateway[0] = 192;
    ps2_gateway[1] = 168;
    ps2_gateway[2] = 0;
    ps2_gateway[3] = 1;
    pc_ip[0] = 192;
    pc_ip[1] = 168;
    pc_ip[2] = 0;
    pc_ip[3] = 2;
    ps2_dns[0] = 192;
    ps2_dns[1] = 168;
    ps2_dns[2] = 0;
    ps2_dns[3] = 1;
    gPCPort = 445;
    gPCShareName[0] = '\0';
    gPCUserName[0] = '\0';
    gPCPassword[0] = '\0';
    gNetworkStartup = ERROR_ETH_NOT_STARTED;
    gHDDSpindown = 20;
    gScrollSpeed = 1;
    gExitPath[0] = '\0';
    gDefaultDevice = APP_MODE;
    gAutosort = 1;
    gAutoRefresh = 0;
    gDisableDebug = 1;
    gPS2Logo = 0;
    gEnableWrite = 0;
    gRememberLastPlayed = 0;
    gAutoStartLastPlayed = 9;
    gSelectButton = KEY_CIRCLE; //Default to Japan.
    gCheckUSBFragmentation = 1;
    gUSBPrefix[0] = '\0';
    gETHPrefix[0] = '\0';
    gUseInfoScreen = 0;
    gEnableArt = 0;
    gWideScreen = 0;

    gUSBStartMode = START_MODE_DISABLED;
    gHDDStartMode = START_MODE_DISABLED;
    gETHStartMode = START_MODE_DISABLED;
    gAPPStartMode = START_MODE_DISABLED;

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
    gXOff = 0;
    gYOff = 0;

#ifdef CHEAT
    memset(gCheatList, 0, sizeof(gCheatList));
#endif

    // Last Played Auto Start
    KeyPressedOnce = 0;
    DisableCron = 1; //Auto Start Last Played counter disabled by default
    CronStart = 0;
    CronCurrent = 0;
    RemainSecs = 0;
}

static void init(void)
{
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

    //Compatibility update handler
    ioRegisterHandler(IO_COMPAT_UPDATE_DEFFERED, &compatDeferredUpdate);

    // handler for deffered menu updates
    ioRegisterHandler(IO_MENU_UPDATE_DEFFERED, &menuDeferredUpdate);
    cacheInit();

    gSelectButton = (InitConsoleRegionData() == CONSOLE_REGION_JAPAN) ? KEY_CIRCLE : KEY_CROSS;

    // try to restore config
    _loadConfig();
}

static void deferredInit(void)
{

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
int main(int argc, char *argv[])
{
    LOG_INIT();
    PREINIT_LOG("OPL GUI start!\n");

    ChangeThreadPriority(GetThreadId(), 31);

    // reset, load modules
    reset();
    init();

    // until this point in the code is reached, only PREINIT_LOG macro should be used
    LOG_ENABLE();

    // queue deffered init which shuts down the intro screen later
    ioPutRequest(IO_CUSTOM_SIMPLEACTION, &deferredInit);

    guiIntroLoop();
    guiMainLoop();

    return 0;
}
