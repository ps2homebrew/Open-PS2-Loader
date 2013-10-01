#ifndef __DIALOGS_H
#define __DIALOGS_H

#include "include/dia.h"

#define COMPAT_MODE_BASE		200

#define COMPAT_CDVDMAN_TIMER	 98
#define COMPAT_DMA				 99
#define COMPAT_ALTSTARTUP		100
#define COMPAT_GAME				101
#define COMPAT_GAMEID			102
#define COMPAT_SAVE				103
#define COMPAT_TEST				104
#define COMPAT_REMOVE			105
#define COMPAT_NOEXIT 			0x70000000
#define COMPAT_LOADFROMDISC		(106 | COMPAT_NOEXIT)
#ifdef VMC
#define COMPAT_VMC1_ACTION		(107 | COMPAT_NOEXIT)
#define COMPAT_VMC2_ACTION		(108 | COMPAT_NOEXIT)
#define COMPAT_VMC1_DEFINE		(109 | COMPAT_NOEXIT)
#define COMPAT_VMC2_DEFINE		(110 | COMPAT_NOEXIT)

#define VMC_NAME				111
#define VMC_SIZE				112
#define VMC_BUTTON_CREATE		113
#define VMC_BUTTON_DELETE		114
#define VMC_STATUS				115
#define VMC_PROGRESS			116
#define VMC_REFRESH				117
#endif

#define UICFG_THEME				10
#define UICFG_LANG				11
#define UICFG_SCROLL			12
#define UICFG_BGCOL				13
#define UICFG_UICOL				14
#define UICFG_TXTCOL			15
#define UICFG_SELCOL			16
#define UICFG_AUTOSORT			17
#define UICFG_COVERART			18
#define UICFG_WIDESCREEN		19
#define UICFG_AUTOREFRESH		20
#define UICFG_VMODE				21
#define UICFG_VSYNC				22
#define UICFG_INFOPAGE			23

#define UICFG_GSM				24
#define UICFG_GSMVMODE			25
#define UICFG_GSMXOFFSET		26
#define UICFG_GSMYOFFSET		27
#define UICFG_GSMSKIPVIDEOS		28

#define CFG_EXITTO				30
#define CFG_DEFDEVICE			31
#define CFG_DEBUG				32
#define CFG_USBMODE				33
#define CFG_HDDMODE				34
#define CFG_ETHMODE				35
#define CFG_APPMODE				36
#define CFG_CHECKUSBFRAG		37
#define CFG_LASTPLAYED			38
#define CFG_DANDROP				39
#define CFG_USBDELAY			40
#define CFG_USBPREFIX			41
#define CFG_ETHPREFIX			42
#define CFG_HDDSPINDOWN			43

#define NETCFG_RECONNECT		44
#define NETCFG_OK			45
#define NETCFG_ETHOPMODE		46

extern struct UIItem diaIPConfig[];
extern struct UIItem diaCompatConfig[];
extern struct UIItem diaUIConfig[];
extern struct UIItem diaConfig[];
extern struct UIItem diaAbout[];
#ifdef VMC
extern struct UIItem diaVMC[];
#endif

#endif
