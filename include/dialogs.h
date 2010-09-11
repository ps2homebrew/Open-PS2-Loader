#ifndef __DIALOGS_H
#define __DIALOGS_H

#include "include/dia.h"

#define COMPAT_SAVE 100
#define COMPAT_TEST 101
#define COMPAT_REMOVE 102
#define COMPAT_LOADFROMDISC 104
#define COMPAT_MODE_BASE 10
#define COMPAT_GAMEID 50
#define COMPAT_MODE_COUNT 8

#define UICFG_THEME 10
#define UICFG_LANG 11
#define UICFG_SCROLL 12
#define UICFG_BGCOL 13
#define UICFG_TXTCOL 14
#define UICFG_AUTOSORT 15
#define UICFG_COVERART 16
#define UICFG_WIDESCREEN 17

#define CFG_EXITTO 30
#define CFG_DEFDEVICE 31
#define CFG_DEBUG 32
#define CFG_USBMODE 33
#define CFG_HDDMODE 34
#define CFG_ETHMODE 35
#define CFG_APPMODE 36
#define CFG_CHECKUSBFRAG 37
#define CFG_LASTPLAYED 38
#define CFG_DANDROP 39

#define UICFG_SAVE 114

extern struct UIItem diaIPConfig[];
extern struct UIItem diaCompatConfig[];
extern struct UIItem diaUIConfig[];
extern struct UIItem diaConfig[];

#endif
