#include "include/dialogs.h"
#include "include/opl.h"
#include "include/dia.h"
#include "include/lang.h"
#include "include/gui.h"

#include <stdio.h>

// Network Config Menu
struct UIItem diaNetConfig[] = {
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_NETCONFIG}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_SHOW_ADVANCED_OPTS}}},
    {UI_SPACER},
    {UI_BOOL, NETCFG_SHOW_ADVANCED_OPTS, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_ETH_OPMODE}}},
    {UI_SPACER},
    {UI_ENUM, NETCFG_ETHOPMODE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_CAT_PS2}}},
    {UI_BREAK},

    // ---- IP address type ----
    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_IP_ADDRESS_TYPE}}},
    {UI_SPACER},
    {UI_ENUM, NETCFG_PS2_IP_ADDR_TYPE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    // ---- IP address ----
    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_IP_ADDRESS}}},
    {UI_SPACER},
    {UI_INT, NETCFG_PS2_IP_ADDR_0, 1, 1, -1, 0, 0, {.intvalue = {192, 192, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_IP_ADDR_1, 1, 1, -1, 0, 0, {.intvalue = {168, 168, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_IP_ADDR_2, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_IP_ADDR_3, 1, 1, -1, 0, 0, {.intvalue = {10, 10, 0, 255}}},
    {UI_BREAK},

    //  ---- Netmask ----
    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_MASK}}},
    {UI_SPACER},
    {UI_INT, NETCFG_PS2_NETMASK_0, 1, 1, -1, 0, 0, {.intvalue = {255, 255, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_NETMASK_1, 1, 1, -1, 0, 0, {.intvalue = {255, 255, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_NETMASK_2, 1, 1, -1, 0, 0, {.intvalue = {255, 255, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_NETMASK_3, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}},
    {UI_BREAK},

    //  ---- Gateway ----
    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_GATEWAY}}},
    {UI_SPACER},
    {UI_INT, NETCFG_PS2_GATEWAY_0, 1, 1, -1, 0, 0, {.intvalue = {192, 192, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_GATEWAY_1, 1, 1, -1, 0, 0, {.intvalue = {168, 168, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_GATEWAY_2, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_GATEWAY_3, 1, 1, -1, 0, 0, {.intvalue = {1, 1, 0, 255}}},
    {UI_BREAK},

    //  ---- DNS server ----
    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_DNS_SERVER}}},
    {UI_SPACER},
    {UI_INT, NETCFG_PS2_DNS_0, 1, 1, -1, 0, 0, {.intvalue = {192, 192, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_DNS_1, 1, 1, -1, 0, 0, {.intvalue = {168, 168, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_DNS_2, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_PS2_DNS_3, 1, 1, -1, 0, 0, {.intvalue = {1, 1, 0, 255}}},
    {UI_SPLITTER},

    //  ---- SMB Server ----
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_CAT_SMB_SERVER}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_ADDRESS_TYPE}}},
    {UI_SPACER},
    {UI_ENUM, NETCFG_SHARE_ADDR_TYPE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_ADDRESS}}},
    {UI_SPACER},
    {UI_STRING, NETCFG_SHARE_NB_ADDR, 1, 0, -1, 0, 0, {.stringvalue = {"", "", NULL}}},
    {UI_INT, NETCFG_SHARE_IP_ADDR_0, 1, 1, -1, 0, 0, {.intvalue = {192, 192, 0, 255}}},
    {UI_LABEL, NETCFG_SHARE_IP_ADDR_DOT_0, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_SHARE_IP_ADDR_1, 1, 1, -1, 0, 0, {.intvalue = {168, 168, 0, 255}}},
    {UI_LABEL, NETCFG_SHARE_IP_ADDR_DOT_1, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_SHARE_IP_ADDR_2, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}},
    {UI_LABEL, NETCFG_SHARE_IP_ADDR_DOT_2, 1, 1, -1, 0, 0, {.label = {".", -1}}},
    {UI_INT, NETCFG_SHARE_IP_ADDR_3, 1, 1, -1, 0, 0, {.intvalue = {1, 1, 0, 255}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_PORT}}},
    {UI_SPACER},
    {UI_INT, NETCFG_SHARE_PORT, 1, 1, -1, 0, 0, {.intvalue = {445, 445, 0, 1024}}},
    {UI_BREAK},

    {UI_BREAK},

    //  ---- SMB share name ----
    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_SHARE}}},
    {UI_SPACER},
    {UI_STRING, NETCFG_SHARE_NAME, 1, 1, _STR_HINT_SHARENAME, 0, 0, {.stringvalue = {"PS2SMB", "PS2SMB", NULL}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_USER}}},
    {UI_SPACER},
    {UI_STRING, NETCFG_SHARE_USERNAME, 1, 1, -1, 0, 0, {.stringvalue = {"GUEST", "GUEST", NULL}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_PASSWORD}}},
    {UI_SPACER},
    {UI_PASSWORD, NETCFG_SHARE_PASSWORD, 1, 1, _STR_HINT_GUEST, 0, 0, {.stringvalue = {"", "", NULL}}},
    {UI_BREAK},

    // buttons
    {UI_BREAK},
    {UI_BUTTON, NETCFG_OK, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},
    {UI_SPACER},
    {UI_BUTTON, NETCFG_RECONNECT, 1, 1, -1, 0, 0, {.label = {NULL, _STR_RECONNECT}}},
    {UI_BREAK},

    // end of dialog
    {UI_TERMINATOR}};

// Per-Game Settings
struct UIItem diaCompatConfig[] = {
    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_COMPAT_SETTINGS}}},
    {UI_SPACER},
    {UI_LABEL, COMPAT_GAME, 1, 1, -1, 0, 0, {.label = {"<Game Label>", -1}}},
    {UI_SPLITTER},

    {UI_LABEL, COMPAT_STATUS, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_MODE1}}},
    {UI_SPACER},
    {UI_BOOL, COMPAT_MODE_BASE, 1, 1, _STR_HINT_MODE1, -10, 0, {.intvalue = {0, 0}}},
    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_MODE2}}},
    {UI_SPACER},
    {UI_BOOL, COMPAT_MODE_BASE + 1, 1, 1, _STR_HINT_MODE2, -10, 0, {.intvalue = {0, 0}}},
    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_MODE3}}},
    {UI_SPACER},
    {UI_BOOL, COMPAT_MODE_BASE + 2, 1, 1, _STR_HINT_MODE3, -10, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_MODE4}}},
    {UI_SPACER},
    {UI_BOOL, COMPAT_MODE_BASE + 3, 1, 1, _STR_HINT_MODE4, -10, 0, {.intvalue = {0, 0}}},
    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_MODE5}}},
    {UI_SPACER},
    {UI_BOOL, COMPAT_MODE_BASE + 4, 1, 1, _STR_HINT_MODE5, -10, 0, {.intvalue = {0, 0}}},
    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_MODE6}}},
    {UI_SPACER},
    {UI_BOOL, COMPAT_MODE_BASE + 5, 1, 1, _STR_HINT_MODE6, -10, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_MODE7}}},
    {UI_SPACER},
    {UI_BOOL, COMPAT_MODE_BASE + 6, 1, 1, _STR_HINT_MODE7, -10, 0, {.intvalue = {0, 0}}},
    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_MODE8}}},
    {UI_SPACER},
    {UI_BOOL, COMPAT_MODE_BASE + 7, 1, 1, _STR_HINT_MODE8, -10, 0, {.intvalue = {0, 0}}},
    {UI_SPLITTER},

#ifdef GSM
    {UI_BUTTON, COMPAT_GSMCONFIG, 1, 1, -1, -30, 0, {.label = {NULL, _STR_GSCONFIG}}},
    {UI_SPACER},
#endif
#ifdef CHEAT
    {UI_BUTTON, COMPAT_CHEATCONFIG, 1, 1, -1, 0, 0, {.label = {NULL, _STR_CHEAT_SETTINGS}}},
#endif
#if defined(GSM) || defined(CHEAT)
    {UI_SPLITTER},
#endif

    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_DMA_MODE}}},
    {UI_SPACER},
    {UI_ENUM, COMPAT_DMA, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_SPLITTER},

#ifdef VMC
    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_VMC_SLOT1}}},
    {UI_SPACER},
#ifndef __CHILDPROOF
    {UI_BUTTON, COMPAT_VMC1_DEFINE, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},
    {UI_SPACER},
    {UI_BUTTON, COMPAT_VMC1_ACTION, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},
#else
    {UI_BUTTON, COMPAT_VMC1_DEFINE, 0, 1, -1, 0, 0, {.label = {NULL, -1}}},
#endif
    {UI_BREAK},
    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_VMC_SLOT2}}},
    {UI_SPACER},
#ifndef __CHILDPROOF
    {UI_BUTTON, COMPAT_VMC2_DEFINE, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},
    {UI_SPACER},
    {UI_BUTTON, COMPAT_VMC2_ACTION, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},
#else
    {UI_BUTTON, COMPAT_VMC2_DEFINE, 0, 1, -1, 0, 0, {.label = {NULL, -1}}},
#endif
    {UI_SPLITTER},
#endif

    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_GAME_ID}}},
    {UI_SPACER},
    {UI_STRING, COMPAT_GAMEID, 1, 1, -1, 0, 0, {.stringvalue = {"", "", NULL}}},
    {UI_SPACER},
    {UI_BUTTON, COMPAT_LOADFROMDISC, 1, 1, -1, 0, 0, {.label = {NULL, _STR_LOAD_FROM_DISC}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_ALTSTARTUP}}},
    {UI_SPACER},
    {UI_STRING, COMPAT_ALTSTARTUP, 1, 1, -1, 0, 0, {.stringvalue = {"", "", &guiAltStartupNameHandler}}},
    {UI_SPLITTER},

// buttons
#ifndef __CHILDPROOF

    {UI_BUTTON, COMPAT_SAVE, 1, 1, -1, 0, 0, {.label = {NULL, _STR_SAVE_CHANGES}}},
    {UI_SPACER},

#endif

    {UI_BUTTON, COMPAT_TEST, 1, 1, -1, 0, 0, {.label = {NULL, _STR_TEST}}},

#ifndef __CHILDPROOF

    {UI_SPACER},
    {UI_BUTTON, COMPAT_DL_DEFAULTS, 1, 1, -1, 0, 0, {.label = {NULL, _STR_DL_DEFAULTS}}},

#endif
    {UI_BREAK},

    {UI_BREAK},

#ifndef __CHILDPROOF

    {UI_BUTTON, COMPAT_REMOVE, 1, 1, -1, 0, 0, {.label = {NULL, _STR_REMOVE_ALL_SETTINGS}}},

#endif
    // end of dialog
    {UI_TERMINATOR}};

// Settings Menu
struct UIItem diaConfig[] = {
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_SETTINGS}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_DEBUG}}},
    {UI_SPACER},
    {UI_BOOL, CFG_DEBUG, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_PS2LOGO}}},
    {UI_SPACER},
    {UI_BOOL, CFG_PS2LOGO, 1, 1, _STR_HINT_PS2LOGO, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_EXITTO}}},
    {UI_SPACER},
    {UI_STRING, CFG_EXITTO, 1, 1, _STR_HINT_EXITPATH, 0, 0, {.stringvalue = {"", "", NULL}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_ENABLE_WRITE}}},
    {UI_SPACER},
    {UI_BOOL, CFG_ENWRITEOP, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_LASTPLAYED}}},
    {UI_SPACER},
    {UI_BOOL, CFG_LASTPLAYED, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_SPACER},
    {UI_LABEL, CFG_LBL_AUTOSTARTLAST, 1, 1, -1, 0, 0, {.label = {NULL, _STR_AUTOSTARTLAST}}},
    {UI_SPACER},
    {UI_INT, CFG_AUTOSTARTLAST, 1, 1, _STR_HINT_AUTOSTARTLAST, 0, 0, {.intvalue = {0, 0, 0, 9}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_SELECTBUTTON}}},
    {UI_SPACER},
    {UI_ENUM, CFG_SELECTBUTTON, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_CHECKUSBFRAG}}},
    {UI_SPACER},
    {UI_BOOL, CFG_CHECKUSBFRAG, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_USB_PREFIX}}},
    {UI_SPACER},
    {UI_STRING, CFG_USBPREFIX, 1, 1, -1, 0, 0, {.stringvalue = {"", "", NULL}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_ETH_PREFIX}}},
    {UI_SPACER},
    {UI_STRING, CFG_ETHPREFIX, 1, 1, -1, 0, 0, {.stringvalue = {"", "", NULL}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_HDD_SPINDOWN}}},
    {UI_SPACER},
    {UI_INT, CFG_HDDSPINDOWN, 1, 1, _STR_HINT_SPINDOWN, 0, 0, {.intvalue = {20, 20, 0, 20}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_USBMODE}}},
    {UI_SPACER},
    {UI_ENUM, CFG_USBMODE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_HDDMODE}}},
    {UI_SPACER},
    {UI_ENUM, CFG_HDDMODE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_ETHMODE}}},
    {UI_SPACER},
    {UI_ENUM, CFG_ETHMODE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_APPMODE}}},
    {UI_SPACER},
    {UI_ENUM, CFG_APPMODE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_DEFDEVICE}}},
    {UI_SPACER},
    {UI_ENUM, CFG_DEFDEVICE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    // buttons
    {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
    {UI_BREAK},

    // end of dialog
    {UI_TERMINATOR}};

// Display Settings Menu
struct UIItem diaUIConfig[] = {
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_GFX_SETTINGS}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_THEME}}},
    {UI_SPACER},
    {UI_ENUM, UICFG_THEME, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_LANGUAGE}}},
    {UI_SPACER},
    {UI_ENUM, UICFG_LANG, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_SCROLLING}}},
    {UI_SPACER},
    {UI_ENUM, UICFG_SCROLL, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_AUTOSORT}}},
    {UI_SPACER},
    {UI_BOOL, UICFG_AUTOSORT, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_AUTOREFRESH}}},
    {UI_SPACER},
    {UI_BOOL, UICFG_AUTOREFRESH, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_COVERART}}},
    {UI_SPACER},
    {UI_BOOL, UICFG_COVERART, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_USE_INFO_SCREEN}}},
    {UI_SPACER},
    {UI_BOOL, UICFG_INFOPAGE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_TXTCOLOR}}},
    {UI_SPACER},
    {UI_COLOUR, UICFG_TXTCOL, 1, 1, -1, -10, 17, {.colourvalue = {0, 0}}},
    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_SELCOLOR}}},
    {UI_SPACER},
    {UI_COLOUR, UICFG_SELCOL, 1, 1, -1, -10, 17, {.colourvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_UICOLOR}}},
    {UI_SPACER},
    {UI_COLOUR, UICFG_UICOL, 1, 1, -1, -10, 17, {.colourvalue = {0, 0}}},
    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, -30, 0, {.label = {NULL, _STR_BGCOLOR}}},
    {UI_SPACER},
    {UI_COLOUR, UICFG_BGCOL, 1, 1, -1, -10, 17, {.colourvalue = {0, 0}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_VMODE}}},
    {UI_SPACER},
    {UI_ENUM, UICFG_VMODE, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {"X-Offset", -1}}},
    {UI_SPACER},
    {UI_INT, UICFG_XOFF, 1, 1, -1, 0, 0, {.intvalue = {0, 0, -300, 300}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {"Y-Offset", -1}}},
    {UI_SPACER},
    {UI_INT, UICFG_YOFF, 1, 1, -1, 0, 0, {.intvalue = {0, 0, -300, 300}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_WIDE_SCREEN}}},
    {UI_SPACER},
    {UI_BOOL, UICFG_WIDESCREEN, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    // buttons
    {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
    {UI_BREAK},

    // end of dialog
    {UI_TERMINATOR}};

// Per-Game Game Settings > GSM Menu (--Bat--)
#ifdef GSM
struct UIItem diaGSConfig[] = {
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_GSM_SETTINGS}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_ENABLEGSM}}},
    {UI_SPACER},
    {UI_BOOL, GSMCFG_ENABLEGSM, 1, 1, _STR_HINT_ENABLEGSM, 0, 0, {.intvalue = {1, 1}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_GSMVMODE}}},
    {UI_SPACER},
    {UI_ENUM, GSMCFG_GSMVMODE, 1, 1, _STR_HINT_GSMVMODE, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_XOFFSET}}},
    {UI_SPACER},
    {UI_INT, GSMCFG_GSMXOFFSET, 1, 1, _STR_HINT_XOFFSET, -5, 0, {.intvalue = {0, 0, -100, 100}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_YOFFSET}}},
    {UI_SPACER},
    {UI_INT, GSMCFG_GSMYOFFSET, 1, 1, _STR_HINT_YOFFSET, -5, 0, {.intvalue = {0, 0, -100, 100}}},
    {UI_BREAK},

    // buttons
    {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
    {UI_BREAK},

    // end of dialog
    {UI_TERMINATOR}};
#endif

// Per Game Settings > Cheat Menu --Bat--
#ifdef CHEAT
struct UIItem diaCheatConfig[] = {
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_CHEAT_SETTINGS}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_ENABLECHEAT}}},
    {UI_SPACER},
    {UI_BOOL, CHTCFG_ENABLECHEAT, 1, 1, _STR_HINT_ENABLECHEAT, 0, 0, {.intvalue = {1, 1}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {NULL, _STR_CHEATMODE}}},
    {UI_SPACER},
    {UI_ENUM, CHTCFG_CHEATMODE, 1, 1, _STR_HINT_CHEATMODE, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    // buttons
    {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
    {UI_BREAK},

    // end of dialog
    {UI_TERMINATOR}};
#endif

// About Menu
struct UIItem diaAbout[] = {
    {UI_LABEL, 1, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},
    {UI_SPLITTER},

    // Coders
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_DEVS}}},
    {UI_BREAK},

    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {"BatRastard - crazyc - dlanor", -1}}},
    {UI_BREAK},

    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {"doctorxyz - hominem.te.esse - ifcaro", -1}}},
    {UI_BREAK},

    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {"izdubar - jimmikaelkael - misfire", -1}}},
    {UI_BREAK},

    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {"Polo35 - reprep - SP193 - volca", -1}}},
    {UI_BREAK},

    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {"... and the anonymous ...", -1}}},
    {UI_BREAK},

    {UI_BREAK},

    // Quality Assurance
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {NULL, _STR_QANDA}}},
    {UI_BREAK},

    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {"algol - Berion - danielB - El_Patas - EP - gledson999 - lee4", -1}}},
    {UI_BREAK},

    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {"LocalH - RandQalan - ShaolinAssassin - yoshi314 - zero35", -1}}},
    {UI_BREAK},

    {UI_BREAK},

    // Network update
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {NULL, _STR_NET_UPDATE}}},
    {UI_BREAK},

    {UI_SPACER},
    {UI_LABEL, 0, 1, 1, -1, 0, 15, {.label = {"icyson55", -1}}},
    {UI_BREAK},

    // buttons
    {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
    {UI_BREAK},

    // end of dialog
    {UI_TERMINATOR}};

// Per-Game Game Settings > VMC Menu
#ifdef VMC
struct UIItem diaVMC[] = {
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_VMC_SCREEN}}},
    {UI_SPLITTER},

    {UI_LABEL, 0, 1, 1, -1, -20, 0, {.label = {NULL, _STR_VMC_NAME}}},
    {UI_SPACER},
    {UI_STRING, VMC_NAME, 1, 1, -1, 0, 0, {.stringvalue = {"", "", &guiVmcNameHandler}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -20, 0, {.label = {NULL, _STR_VMC_SIZE}}},
    {UI_SPACER},
    {UI_ENUM, VMC_SIZE, 1, 1, _STR_HINT_VMC_SIZE, 0, 0, {.intvalue = {0, 0}}},
    {UI_BREAK},

    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -20, 0, {.label = {NULL, _STR_VMC_STATUS}}},
    {UI_SPACER},
    {UI_LABEL, VMC_STATUS, 0, 1, -1, 0, 0, {.label = {NULL, -1}}},
    {UI_BREAK},

    {UI_LABEL, 0, 1, 1, -1, -20, 0, {.label = {NULL, _STR_VMC_PROGRESS}}},
    {UI_SPACER},
    {UI_INT, VMC_PROGRESS, 0, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 100}}},
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"%", -1}}},
    {UI_BREAK},

    // buttons
    {UI_BUTTON, VMC_BUTTON_CREATE, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},
    {UI_SPACER},
    {UI_BUTTON, VMC_BUTTON_DELETE, 1, 1, -1, 0, 0, {.label = {NULL, _STR_DELETE}}},
    {UI_BREAK},

    // end of dialog
    {UI_TERMINATOR}};
#endif

// Network Update Menu
struct UIItem diaNetCompatUpdate[] = {
    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_NET_UPDATE}}},
    {UI_SPLITTER},

    {UI_LABEL, NETUPD_OPT_UPD_ALL_LBL, 1, 1, -1, -40, 0, {.label = {NULL, _STR_NET_UPDATE_ALL}}},
    {UI_SPACER},
    {UI_BOOL, NETUPD_OPT_UPD_ALL, 0, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 1}}},
    {UI_BREAK},

    {UI_LABEL, NETUPD_PROGRESS_LBL, 1, 1, -1, -40, 0, {.label = {NULL, _STR_VMC_PROGRESS}}},
    {UI_SPACER},
    {UI_INT, NETUPD_PROGRESS, 0, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 100}}},
    {UI_LABEL, NETUPD_PROGRESS_PERC_LBL, 1, 1, -1, 0, 0, {.label = {"%", -1}}},
    {UI_BREAK},

    // buttons
    {UI_BUTTON, NETUPD_BTN_START, 1, 1, -1, 0, 0, {.label = {NULL, _STR_START}}},
    {UI_SPACER},
    {UI_BUTTON, NETUPD_BTN_CANCEL, 1, 1, -1, 0, 0, {.label = {NULL, _STR_CANCEL}}},
    {UI_BREAK},

    // end of dialog
    {UI_TERMINATOR}};
