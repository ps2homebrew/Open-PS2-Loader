#ifndef __DIALOGS_H
#define __DIALOGS_H

#include "include/dia.h"

#define COMPAT_SAVE 100
#define COMPAT_TEST 101
#define COMPAT_REMOVE 102
#define COMPAT_LOADFROMDISC 104
#define COMPAT_MODE_BASE 10
#define COMPAT_GAMEID 50
#define COMPAT_MODE_COUNT 7

#define UICFG_THEME 10
#define UICFG_LANG 11
#define UICFG_SCROLL 12
#define UICFG_MENU 13
#define UICFG_BGCOL 14
#define UICFG_TXTCOL 15
#define UICFG_EXITTO 16
#define UICFG_DEFDEVICE 17
#define UICFG_AUTOSORT 20
#define UICFG_DEBUG 21
#define UICFG_COVERART 22
#define UICFG_WIDESCREEN 23
#define UICFG_USBMODE 24
#define UICFG_HDDMODE 25
#define UICFG_ETHMODE 26
#define UICFG_APPMODE 27
#define UICFG_SAVE 114

extern struct UIItem diaIPConfig[];
extern struct UIItem diaCompatConfig[];
extern struct UIItem diaUIConfig[];

#endif
