#include "include/dialogs.h"
#include "include/usbld.h"
#include "include/dia.h"
#include "include/lang.h"
#include "include/gui.h"

#include <stdio.h>

// Dialog definition for IP configuration
struct UIItem diaIPConfig[] = {
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_IPCONFIG}}},
	
	{UI_SPLITTER},

	// ---- IP address ----
	{UI_LABEL, 0, 1, -1, {.label = {"- PS2 -", -1}}}, {UI_BREAK},
	
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_IP}}}, {UI_SPACER},
	
	{UI_INT, 2, 1, -1, {.intvalue = {192, 192, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 3, 1, -1, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 4, 1, -1, {.intvalue = {0, 0, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 5, 1, -1, {.intvalue = {10, 10, 0, 255}}},

	{UI_BREAK},

	//  ---- Netmask ----
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_MASK}}},
	{UI_SPACER}, 
	
	{UI_INT, 6, 1, -1, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 7, 1, -1, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 8, 1, -1, {.intvalue = {255, 255, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 9, 1, -1, {.intvalue = {0, 0, 0, 255}}},
	
	{UI_BREAK},
	
	//  ---- Gateway ----
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_GATEWAY}}},
	{UI_SPACER}, 
	
	{UI_INT, 10, 1, -1, {.intvalue = {192, 192, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 11, 1, -1, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 12, 1, -1, {.intvalue = {0, 0, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 13, 1, -1, {.intvalue = {1, 1, 0, 255}}},
	
	{UI_SPLITTER},
	
	//  ---- PC ----
	{UI_LABEL, 0, 1, -1, {.label = {"- PC -", -1}}},
	{UI_BREAK},
	
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_IP}}},
	{UI_SPACER}, 
	
	{UI_INT, 14, 1, -1, {.intvalue = {192, 192, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 15, 1, -1, {.intvalue = {168, 168, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 16, 1, -1, {.intvalue = {0, 0, 0, 255}}}, {UI_LABEL, 0, 1, -1, {.label = {".", -1}}},
	{UI_INT, 17, 1, -1, {.intvalue = {1, 1, 0, 255}}},
	
	{UI_BREAK},
	
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_PORT}}},	{UI_SPACER}, {UI_INT, 18, 1, -1, {.intvalue = {445, 445, 0, 1024}}},
	
	{UI_BREAK},
	
	//  ---- PC share name ----
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_SHARE}}}, {UI_SPACER}, {UI_STRING, 19, 1, -1, {.stringvalue = {"PS2SMB", "PS2SMB", NULL}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_USER}}}, {UI_SPACER}, {UI_STRING, 20, 1, -1,  {.stringvalue = {"GUEST", "GUEST", NULL}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_PASSWORD}}}, {UI_SPACER}, {UI_PASSWORD, 21, 1, _STR_HINT_GUEST,  {.stringvalue = {"", "", NULL}}}, {UI_BREAK},
	
	//  ---- Ok ----
	{UI_SPLITTER},
	{UI_OK, 0, 1, -1, {.label = {NULL, _STR_OK}}},
	
	// end of dialog
	{UI_TERMINATOR}
};

struct UIItem diaCompatConfig[] = {
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_COMPAT_SETTINGS}}}, {UI_SPACER},
	{UI_LABEL, COMPAT_GAME, 1, -1, {.label = {"<Game Label>", -1}}},
	
	{UI_SPLITTER},
	

	{UI_LABEL, 0, 1, -1, {.label = {"Mode 1", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE    , 1, _STR_HINT_MODE1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {"Mode 2", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 1, 1, _STR_HINT_MODE2, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {"Mode 3", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 2, 1, _STR_HINT_MODE3, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {"Mode 4", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 3, 1, _STR_HINT_MODE4, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {"Mode 5", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 4, 1, _STR_HINT_MODE5, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {"Mode 6", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 5, 1, _STR_HINT_MODE6, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {"Mode 7", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 6, 1, _STR_HINT_MODE7, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {"Mode 8", -1}}}, {UI_SPACER}, {UI_BOOL, COMPAT_MODE_BASE + 7, 1, _STR_HINT_MODE8, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, 1, -1, {.label = {"DMA Mode", -1}}}, {UI_SPACER}, {UI_ENUM, COMPAT_MODE_BASE + COMPAT_MODE_COUNT, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
#ifdef VMC
	{UI_LABEL, 0, 1, -1, {.label = {"VMC Slot 1", -1}}}, {UI_SPACER},
#ifndef __CHILDPROOF
	{UI_BUTTON, COMPAT_VMC1_DEFINE, 1, -1, {.label = {NULL, -1}}}, {UI_SPACER}, {UI_BUTTON, COMPAT_VMC1_ACTION, 1, -1, {.label = {NULL, -1}}}, {UI_BREAK},
#else
	{UI_BUTTON, COMPAT_VMC1_DEFINE, 0, -1, {.label = {NULL, -1}}}, {UI_BREAK},
#endif
	{UI_LABEL, 0, 1, -1, {.label = {"VMC Slot 2", -1}}}, {UI_SPACER},
#ifndef __CHILDPROOF
	{UI_BUTTON, COMPAT_VMC2_DEFINE, 1, -1, {.label = {NULL, -1}}}, {UI_SPACER}, {UI_BUTTON, COMPAT_VMC2_ACTION, 1, -1, {.label = {NULL, -1}}}, {UI_BREAK},
#else
	{UI_BUTTON, COMPAT_VMC2_DEFINE, 0, -1, {.label = {NULL, -1}}}, {UI_BREAK},
#endif

	{UI_SPLITTER},
#endif
	
	{UI_LABEL, 0, 1, -1, {.label = {"Game ID", -1}}}, {UI_SPACER}, {UI_STRING, COMPAT_GAMEID, 1, -1, {.stringvalue = {"", "", NULL}}},
	{UI_SPACER}, {UI_BUTTON, COMPAT_LOADFROMDISC, 1, -1, {.label = {NULL, _STR_LOAD_FROM_DISC}}},

	{UI_SPLITTER},
	
#ifndef __CHILDPROOF
	{UI_BUTTON, COMPAT_SAVE, 1, -1, {.label = {NULL, _STR_SAVE_CHANGES}}}, {UI_SPACER},
#endif
	{UI_BUTTON, COMPAT_TEST, 1, -1, {.label = {NULL, _STR_TEST}}},
	
	{UI_SPLITTER},
#ifndef __CHILDPROOF
	{UI_BUTTON, COMPAT_REMOVE, 1, -1, {.label = {NULL, _STR_REMOVE_ALL_SETTINGS}}},
#endif
	// end of dialog
	{UI_TERMINATOR}
};

struct UIItem diaConfig[] = {
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_SETTINGS}}},
	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_DEBUG}}}, {UI_SPACER}, {UI_BOOL, CFG_DEBUG, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_EXITTO}}}, {UI_SPACER}, {UI_ENUM, CFG_EXITTO, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_DANDROP}}}, {UI_SPACER}, {UI_BOOL, CFG_DANDROP, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	//{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_CHECKUSBFRAG}}}, {UI_SPACER}, {UI_BOOL, CFG_CHECKUSBFRAG, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_LASTPLAYED}}}, {UI_SPACER}, {UI_BOOL, CFG_LASTPLAYED, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_USBMODE}}}, {UI_SPACER}, {UI_ENUM, CFG_USBMODE, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_HDDMODE}}}, {UI_SPACER}, {UI_ENUM, CFG_HDDMODE, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_ETHMODE}}}, {UI_SPACER}, {UI_ENUM, CFG_ETHMODE, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_APPMODE}}}, {UI_SPACER}, {UI_ENUM, CFG_APPMODE, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_DEFDEVICE}}}, {UI_SPACER}, {UI_ENUM, CFG_DEFDEVICE, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},

	{UI_SPLITTER},
	{UI_OK, 0, 1, -1, {.label = {NULL, _STR_OK}}},

	// end of dialog
	{UI_TERMINATOR}
};

struct UIItem diaUIConfig[] = {
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_GFX_SETTINGS}}},
	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_THEME}}}, {UI_SPACER}, {UI_ENUM, UICFG_THEME, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_LANGUAGE}}}, {UI_SPACER}, {UI_ENUM, UICFG_LANG,  1, -1,{.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_SCROLLING}}}, {UI_SPACER}, {UI_ENUM, UICFG_SCROLL, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_AUTOSORT}}}, {UI_SPACER}, {UI_BOOL, UICFG_AUTOSORT, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_COVERART}}}, {UI_SPACER}, {UI_BOOL, UICFG_COVERART, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_WIDE_SCREEN}}}, {UI_SPACER}, {UI_BOOL, UICFG_WIDESCREEN, 1, -1, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_BGCOLOR}}}, {UI_SPACER}, {UI_COLOUR, UICFG_BGCOL, 1, -1, {.colourvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_TXTCOLOR}}}, {UI_SPACER}, {UI_COLOUR, UICFG_TXTCOL, 1, -1, {.colourvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	{UI_OK, 0, 1, -1, {.label = {NULL, _STR_OK}}},

	// end of dialog
	{UI_TERMINATOR}
};

#ifdef VMC
struct UIItem diaVMC[] = {
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_VMC_SCREEN}}},
	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_VMC_NAME}}}, {UI_SPACER}, {UI_STRING, VMC_NAME, 1, -1, {.stringvalue = {"", "", &guiVmcNameHandler}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_VMC_SIZE}}}, {UI_SPACER}, {UI_ENUM, VMC_SIZE, 1, _STR_HINT_VMC_SIZE, {.intvalue = {0, 0}}}, {UI_BREAK},

	{UI_SPLITTER},

	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_VMC_STATUS}}}, {UI_SPACER}, {UI_LABEL, VMC_STATUS, 0, -1, {.label = {NULL, -1}}}, {UI_BREAK},
	{UI_LABEL, 0, 1, -1, {.label = {NULL, _STR_VMC_PROGRESS}}}, {UI_SPACER}, {UI_INT, VMC_PROGRESS, 0, -1, {.intvalue = {0, 0, 0, 100}}}, {UI_LABEL, 0, 1, -1, {.label = {"%", -1}}}, {UI_BREAK},

	{UI_SPLITTER},
	{UI_BUTTON, VMC_BUTTON_CREATE, 1, -1, {.label = {NULL, -1}}},

	{UI_SPLITTER},
	{UI_BUTTON, VMC_BUTTON_DELETE, 1, -1, {.label = {NULL, _STR_DELETE}}},

	// end of dialog
	{UI_TERMINATOR}
};
#endif
