#include "include/dialogs.h"
#include "include/usbld.h"
#include "include/dia.h"
#include "include/lang.h"

#include <stdio.h>

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
	
	//  ---- Ok ----
	{UI_SPLITTER},
	{UI_OK},
	
	// end of dialogue
	{UI_TERMINATOR}
};

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

struct UIItem diaUIConfig[] = {
	{UI_LABEL, 111, NULL, {.label = {"", _STR_SETTINGS}}},
	{UI_SPLITTER},

	{UI_LABEL, 0, NULL, {.label = {"", _STR_THEME}}}, {UI_SPACER}, {UI_ENUM, UICFG_THEME, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_LANGUAGE}}}, {UI_SPACER}, {UI_ENUM, UICFG_LANG,  NULL,{.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_SCROLLING}}}, {UI_SPACER}, {UI_ENUM, UICFG_SCROLL, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_AUTOSORT}}}, {UI_SPACER}, {UI_BOOL, UICFG_AUTOSORT, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_DEBUG}}}, {UI_SPACER}, {UI_BOOL, UICFG_DEBUG, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_COVERART}}}, {UI_SPACER}, {UI_BOOL, UICFG_COVERART, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_WIDE_SCREEN}}}, {UI_SPACER}, {UI_BOOL, UICFG_WIDESCREEN, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Exit to", -1}}}, {UI_SPACER}, {UI_ENUM, UICFG_EXITTO, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"Bg. col", -1}}}, {UI_SPACER}, {UI_COLOUR, UICFG_BGCOL, NULL, {.colourvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"Txt. col", -1}}}, {UI_SPACER}, {UI_COLOUR, UICFG_TXTCOL, NULL, {.colourvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_LABEL, 0, NULL, {.label = {"", _STR_USBMODE}}}, {UI_SPACER}, {UI_ENUM, UICFG_USBMODE, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_HDDMODE}}}, {UI_SPACER}, {UI_ENUM, UICFG_HDDMODE, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_ETHMODE}}}, {UI_SPACER}, {UI_ENUM, UICFG_ETHMODE, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_APPMODE}}}, {UI_SPACER}, {UI_ENUM, UICFG_APPMODE, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	{UI_LABEL, 0, NULL, {.label = {"", _STR_DEFDEVICE}}}, {UI_SPACER}, {UI_ENUM, UICFG_DEFDEVICE, NULL, {.intvalue = {0, 0}}}, {UI_BREAK},
	
	{UI_SPLITTER},
	
	{UI_OK}, {UI_SPACER}, {UI_BUTTON, UICFG_SAVE, NULL, {.label = {"", _STR_SAVE_CHANGES}}}, 
	
	{UI_SPLITTER},
	
	// end of dialogue
	{UI_TERMINATOR}
};

