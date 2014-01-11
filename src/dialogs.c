#include "include/dialogs.h"
#include "include/usbld.h"
#include "include/dia.h"
#include "include/lang.h"
#include "include/gui.h"

#include <stdio.h>

// Dialog definition for IP configuration
struct UIItem diaIPConfig[] = {
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_IPCONFIG}}},
	
	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {"- PS2 -", -1}}}, {UI_BREAK},
	
	// ---- IP address ----
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_IP}}}, {UI_SPACER},
	
	{UI_INT, 2, 1, -1, 0, 0, {.intvalue = {192, 192, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 3, 1, -1, 0, 0, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 4, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 5, 1, -1, 0, 0, {.intvalue = {10, 10, 0, 255}}},	{UI_BREAK},

	//  ---- Netmask ----
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_MASK}}}, {UI_SPACER},
	
	{UI_INT, 6, 1, -1, 0, 0, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 7, 1, -1, 0, 0, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 8, 1, -1, 0, 0, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 9, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}}, {UI_BREAK},
	
	//  ---- Gateway ----
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_GATEWAY}}}, {UI_SPACER},
	
	{UI_INT, 10, 1, -1, 0, 0, {.intvalue = {192, 192, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 11, 1, -1, 0, 0, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 12, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 13, 1, -1, 0, 0, {.intvalue = {1, 1, 0, 255}}},

	{UI_BREAK}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_ETH_OPMODE}}}, {UI_SPACER}, {UI_ENUM, NETCFG_ETHOPMODE, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},

	{UI_SPLITTER},
	
	//  ---- PC ----
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {"- PC -", -1}}}, {UI_BREAK},
	
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_IP}}}, {UI_SPACER},
	
	{UI_INT, 14, 1, -1, 0, 0, {.intvalue = {192, 192, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 15, 1, -1, 0, 0, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 16, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 255}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {".", -1}}},
	{UI_INT, 17, 1, -1, 0, 0, {.intvalue = {1, 1, 0, 255}}}, {UI_BREAK},
	
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_PORT}}}, {UI_SPACER}, {UI_INT, 18, 1, -1, 0, 0, {.intvalue = {445, 445, 0, 1024}}}, {UI_BREAK},
	
	{UI_BREAK},
	
	//  ---- PC share name ----
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_SHARE}}}, {UI_SPACER}, {UI_STRING, 19, 1, _STR_HINT_SHARENAME, 0, 0, {.stringvalue = {"PS2SMB", "PS2SMB", NULL}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_USER}}}, {UI_SPACER}, {UI_STRING, 20, 1, -1, 0, 0, {.stringvalue = {"GUEST", "GUEST", NULL}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_PASSWORD}}}, {UI_SPACER}, {UI_PASSWORD, 21, 1, _STR_HINT_GUEST, 0, 0, {.stringvalue = {"", "", NULL}}},
	
	{UI_SPLITTER},

	//  ---- Ok ----
	{UI_BUTTON, NETCFG_OK, 1, -1, 0, 0, {.label = {NULL, -1}}},
	
	{UI_SPACER}, // UIItem #70

	{UI_BUTTON, NETCFG_RECONNECT, 1, -1, 0, 0, {.label = {NULL, _STR_RECONNECT}}},

	// end of dialog
	{UI_TERMINATOR}
};

struct UIItem diaCompatConfig[] = {
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_COMPAT_SETTINGS}}}, {UI_SPACER},
	{UI_LABEL, COMPAT_GAME, 1, -1, 0, 0, {.label = {"<Game Label>", -1}}},
	
	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_MODE1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE    , 1, _STR_HINT_MODE1, -10, 0, {.intvalue = {0, 0}}}, {UI_SPACER},
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_MODE2}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 1, 1, _STR_HINT_MODE2, -10, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_MODE3}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 2, 1, _STR_HINT_MODE3, -10, 0, {.intvalue = {0, 0}}}, {UI_SPACER},
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_MODE4}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 3, 1, _STR_HINT_MODE4, -10, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_MODE5}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 4, 1, _STR_HINT_MODE5, -10, 0, {.intvalue = {0, 0}}}, {UI_SPACER},
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_MODE6}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 5, 1, _STR_HINT_MODE6, -10, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_MODE7}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 6, 1, _STR_HINT_MODE7, -10, 0, {.intvalue = {0, 0}}}, {UI_SPACER},
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_MODE8}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 7, 1, _STR_HINT_MODE8, -10, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_BREAK},
	
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_CDVDMAN_TIMER}}}, {UI_SPACER}, {UI_INT, COMPAT_CDVDMAN_TIMER, 1, _STR_HINT_CDVDMAN_TIMER, 0, 0, {.intvalue = {0, 0, 0, 255}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_DMA_MODE}}}, {UI_SPACER}, {UI_ENUM, COMPAT_DMA, 1, -1, 0, 0, {.intvalue = {0, 0}}},
	
	{UI_SPLITTER},
	
#ifdef VMC
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_VMC_SLOT1}}}, {UI_SPACER},
#ifndef __CHILDPROOF
	{UI_BUTTON, COMPAT_VMC1_DEFINE, 1, -1, 0, 0, {.label = {NULL, -1}}}, {UI_SPACER}, {UI_BUTTON, COMPAT_VMC1_ACTION, 1, -1, 0, 0, {.label = {NULL, -1}}}, {UI_BREAK},
#else
	{UI_BUTTON, COMPAT_VMC1_DEFINE, 0, -1, 0, 0, {.label = {NULL, -1}}}, {UI_BREAK},
#endif
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_VMC_SLOT2}}}, {UI_SPACER},
#ifndef __CHILDPROOF
	{UI_BUTTON, COMPAT_VMC2_DEFINE, 1, -1, 0, 0, {.label = {NULL, -1}}}, {UI_SPACER}, {UI_BUTTON, COMPAT_VMC2_ACTION, 1, -1, 0, 0, {.label = {NULL, -1}}},
#else
	{UI_BUTTON, COMPAT_VMC2_DEFINE, 0, -1, 0, 0, {.label = {NULL, -1}}},
#endif

	{UI_SPLITTER},
#endif
	
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_GAME_ID}}}, {UI_SPACER}, {UI_STRING, COMPAT_GAMEID, 1, -1, 0, 0, {.stringvalue = {"", "", NULL}}},
	{UI_SPACER}, {UI_BUTTON, COMPAT_LOADFROMDISC, 1, -1, 0, 0, {.label = {NULL, _STR_LOAD_FROM_DISC}}},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_ALTSTARTUP}}}, {UI_SPACER}, {UI_STRING, COMPAT_ALTSTARTUP, 1, -1, 0, 0, {.stringvalue = {"", "", &guiAltStartupNameHandler}}},

	{UI_SPLITTER},
	
#ifndef __CHILDPROOF
	{UI_BUTTON, COMPAT_SAVE, 1, -1, 0, 0, {.label = {NULL, _STR_SAVE_CHANGES}}}, {UI_SPACER},
#endif
	{UI_BUTTON, COMPAT_TEST, 1, -1, 0, 0, {.label = {NULL, _STR_TEST}}}, {UI_BREAK},
	
	{UI_BREAK},

#ifndef __CHILDPROOF
	{UI_BUTTON, COMPAT_REMOVE, 1, -1, 0, 0, {.label = {NULL, _STR_REMOVE_ALL_SETTINGS}}},
#endif
	// end of dialog
	{UI_TERMINATOR}
};

struct UIItem diaConfig[] = {
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_SETTINGS}}},
	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_DEBUG}}}, {UI_SPACER}, {UI_BOOL, CFG_DEBUG, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_EXITTO}}}, {UI_SPACER}, {UI_STRING, CFG_EXITTO, 1, _STR_HINT_EXITPATH, 0, 0, {.stringvalue = {"", "", NULL}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_DANDROP}}}, {UI_SPACER}, {UI_BOOL, CFG_DANDROP, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_LASTPLAYED}}}, {UI_SPACER}, {UI_BOOL, CFG_LASTPLAYED, 1, -1, 0, 0, {.intvalue = {0, 0}}},{UI_BREAK},
#ifdef GSM
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_SHOWGSM}}}, {UI_SPACER}, {UI_BOOL, CFG_SHOWGSM, 1, -1, 0, 0, {.intvalue = {0, 0}}},{UI_BREAK},
#endif

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_CHECKUSBFRAG}}}, {UI_SPACER}, {UI_BOOL, CFG_CHECKUSBFRAG, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_USB_DELAY}}}, {UI_SPACER}, {UI_INT, CFG_USBDELAY, 1, -1, 0, 0, {.intvalue = {3, 3, 0, 99}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_USB_PREFIX}}}, {UI_SPACER}, {UI_STRING, CFG_USBPREFIX, 1, -1, 0, 0, {.stringvalue = {"", "", NULL}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_ETH_PREFIX}}}, {UI_SPACER}, {UI_STRING, CFG_ETHPREFIX, 1, -1, 0, 0, {.stringvalue = {"", "", NULL}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_HDD_SPINDOWN}}}, {UI_SPACER}, {UI_INT, CFG_HDDSPINDOWN, 1, _STR_HINT_SPINDOWN, 0, 0, {.intvalue = {20, 20, 0, 20}}},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_USBMODE}}}, {UI_SPACER}, {UI_ENUM, CFG_USBMODE, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_HDDMODE}}}, {UI_SPACER}, {UI_ENUM, CFG_HDDMODE, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_ETHMODE}}}, {UI_SPACER}, {UI_ENUM, CFG_ETHMODE, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_APPMODE}}}, {UI_SPACER}, {UI_ENUM, CFG_APPMODE, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_DEFDEVICE}}}, {UI_SPACER}, {UI_ENUM, CFG_DEFDEVICE, 1, -1, 0, 0, {.intvalue = {0, 0}}},

	{UI_SPLITTER},

	{UI_OK, 0, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},

	// end of dialog
	{UI_TERMINATOR}
};

struct UIItem diaUIConfig[] = {
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_GFX_SETTINGS}}},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_THEME}}}, {UI_SPACER}, {UI_ENUM, UICFG_THEME, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_LANGUAGE}}}, {UI_SPACER}, {UI_ENUM, UICFG_LANG, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_SCROLLING}}}, {UI_SPACER}, {UI_ENUM, UICFG_SCROLL, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_AUTOSORT}}}, {UI_SPACER}, {UI_BOOL, UICFG_AUTOSORT, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_AUTOREFRESH}}}, {UI_SPACER}, {UI_BOOL, UICFG_AUTOREFRESH, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_COVERART}}}, {UI_SPACER}, {UI_BOOL, UICFG_COVERART, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_USE_INFO_SCREEN}}}, {UI_SPACER}, {UI_BOOL, UICFG_INFOPAGE, 1, -1, 0, 0, {.intvalue = {0, 0}}},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_TXTCOLOR}}}, {UI_SPACER}, {UI_COLOUR, UICFG_TXTCOL, 1, -1, -10, 17, {.colourvalue = {0, 0}}}, // UIItem #32
	{UI_SPACER},
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_SELCOLOR}}}, {UI_SPACER}, {UI_COLOUR, UICFG_SELCOL, 1, -1, -10, 17, {.colourvalue = {0, 0}}}, // UIItem #36
	{UI_BREAK},
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_UICOLOR}}}, {UI_SPACER}, {UI_COLOUR, UICFG_UICOL, 1, -1, -10, 17, {.colourvalue = {0, 0}}}, // UIItem #40
	{UI_SPACER},
	{UI_LABEL, 0, 1, -1, -30, 0, {.label = {NULL, _STR_BGCOLOR}}}, {UI_SPACER}, {UI_COLOUR, UICFG_BGCOL, 1, -1, -10, 17, {.colourvalue = {0, 0}}}, // UIItem #44
	{UI_BREAK},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_VMODE}}}, {UI_SPACER}, {UI_ENUM, UICFG_VMODE, 1, -1, 0, 0, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_VSYNC}}}, {UI_SPACER}, {UI_BOOL, UICFG_VSYNC, 1, -1, 0, 0, {.intvalue = {1, 1}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_WIDE_SCREEN}}}, {UI_SPACER}, {UI_BOOL, UICFG_WIDESCREEN, 1, -1, 0, 0, {.intvalue = {0, 0}}},
	{UI_BREAK},

	{UI_SPLITTER},

	{UI_OK, 0, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},

	// end of dialog
	{UI_TERMINATOR}
};

#ifdef GSM
	// GSM Menu
struct UIItem diaGSConfig[] = {
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_GSM_SETTINGS}}},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_ENABLEGSM}}}, {UI_SPACER}, {UI_BOOL, GSCFG_ENABLEGSM, 1, _STR_HINT_ENABLEGSM, 0, 0, {.intvalue = {1, 1}}},{UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_GSMVMODE}}}, {UI_SPACER}, {UI_ENUM, GSCFG_GSMVMODE, 1, _STR_HINT_GSMVMODE, 0, 0, {.intvalue = {0, 0}}},{UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_GSMSKIPVIDEOS}}}, {UI_SPACER}, {UI_BOOL, GSCFG_GSMSKIPVIDEOS, 1, _STR_HINT_GSMSKIPVIDEOS, 0, 0, {.intvalue = {1, 1}}},{UI_BREAK},
 	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_XOFFSET}}}, {UI_SPACER}, {UI_INT, GSCFG_GSMXOFFSET, 1, _STR_HINT_XOFFSET, -5, 0, {.intvalue = {0, 0, -100, 100}}},{UI_BREAK},
	{UI_LABEL, 0, 1, -1, -45, 0, {.label = {NULL, _STR_YOFFSET}}}, {UI_SPACER}, {UI_INT, GSCFG_GSMYOFFSET, 1, _STR_HINT_YOFFSET, -5, 0, {.intvalue = {0, 0, -100, 100}}},{UI_BREAK},
	{UI_SPLITTER},

	{UI_OK, 0, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},

	// end of dialog
	{UI_TERMINATOR}
};
#endif

struct UIItem diaAbout[] = {
	{UI_LABEL, 1, 1, -1, 0, 0, {.label = {NULL, -1}}},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_DEVS}}}, {UI_BREAK},

	{UI_BREAK},

	{UI_SPACER}, {UI_LABEL, 0, 1, -1, 0, 15, {.label = {"crazyc - dlanor - SP193", -1}}}, {UI_BREAK},
	{UI_SPACER}, {UI_LABEL, 0, 1, -1, 0, 15, {.label = {"doctorxyz - hominem.te.esse", -1}}}, {UI_BREAK},
	{UI_SPACER}, {UI_LABEL, 0, 1, -1, 0, 15, {.label = {"ifcaro - Polo35 - reprep", -1}}}, {UI_BREAK},
	{UI_SPACER}, {UI_LABEL, 0, 1, -1, 0, 15, {.label = {"izdubar - jimmikaelkael", -1}}}, {UI_BREAK},
	{UI_SPACER}, {UI_LABEL, 0, 1, -1, 0, 15, {.label = {"volca - BatRastard", -1}}}, {UI_BREAK},
    	{UI_SPACER}, {UI_LABEL, 0, 1, -1, 0, 15, {.label = {"...and the anonymous ...", -1}}}, {UI_BREAK},
	{UI_SPLITTER},

	{UI_OK, 0, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},

	// end of dialog
	{UI_TERMINATOR}
};

#ifdef VMC
struct UIItem diaVMC[] = {
	{UI_LABEL, 0, 1, -1, 0, 0, {.label = {NULL, _STR_VMC_SCREEN}}},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -20, 0, {.label = {NULL, _STR_VMC_NAME}}}, {UI_SPACER}, {UI_STRING, VMC_NAME, 1, -1, 0, 0, {.stringvalue = {"", "", &guiVmcNameHandler}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -20, 0, {.label = {NULL, _STR_VMC_SIZE}}}, {UI_SPACER}, {UI_ENUM, VMC_SIZE, 1, _STR_HINT_VMC_SIZE, 0, 0, {.intvalue = {0, 0}}},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, -20, 0, {.label = {NULL, _STR_VMC_STATUS}}}, {UI_SPACER}, {UI_LABEL, VMC_STATUS, 0, -1, 0, 0, {.label = {NULL, -1}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, -20, 0, {.label = {NULL, _STR_VMC_PROGRESS}}}, {UI_SPACER}, {UI_INT, VMC_PROGRESS, 0, -1, 0, 0, {.intvalue = {0, 0, 0, 100}}}, {UI_LABEL, 0, 1, -1, 0, 0, {.label = {"%", -1}}},

	{UI_SPLITTER},

	{UI_BUTTON, VMC_BUTTON_CREATE, 1, -1, 0, 0, {.label = {NULL, -1}}},

	{UI_SPLITTER}, // UIItem #20

	{UI_BUTTON, VMC_BUTTON_DELETE, 1, -1, 0, 0, {.label = {NULL, _STR_DELETE}}},

	// end of dialog
	{UI_TERMINATOR}
};
#endif
