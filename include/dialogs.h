#ifndef __DIALOGS_H
#define __DIALOGS_H

#include "include/dia.h"

enum UI_ITEMS {
    UIID_BTN_CANCEL = 0,
    UIID_BTN_OK,

    UICFG_THEME = 10,
    UICFG_LANG,
    UICFG_SCROLL,
    UICFG_BGCOL,
    UICFG_UICOL,
    UICFG_TXTCOL,
    UICFG_SELCOL,
    UICFG_RESETCOL,
    UICFG_AUTOSORT,
    UICFG_COVERART,
    UICFG_WIDESCREEN,
    UICFG_AUTOREFRESH,
    UICFG_VMODE,
    UICFG_XOFF,
    UICFG_YOFF,
    UICFG_OVERSCAN,
    UICFG_NOTIFICATIONS,

    CFG_DEBUG,
    CFG_PS2LOGO,
    CFG_HDDGAMELISTCACHE,
    CFG_EXITTO,
    CFG_DEFDEVICE,
    CFG_BDMMODE,
    CFG_HDDMODE,
    CFG_ETHMODE,
    CFG_APPMODE,
    CFG_FAVMODE,
    CFG_BDMCACHE,
    CFG_HDDCACHE,
    CFG_SMBCACHE,
    CFG_ENABLEILK,
    CFG_ENABLEMX4SIO,
    CFG_LASTPLAYED,
    CFG_LBL_AUTOSTARTLAST,
    CFG_AUTOSTARTLAST,
    CFG_SELECTBUTTON,
    CFG_ENWRITEOP,
    CFG_BDMPREFIX,
    CFG_ETHPREFIX,
    CFG_HDDSPINDOWN,
    BLOCKDEVICE_BUTTON,

    ABOUT_TITLE,
    ABOUT_BUILD_DETAILS,

    CFG_PARENLOCK_PASSWORD,

    CFG_SFX,
    CFG_BOOT_SND,
    CFG_BGM,
    CFG_SFX_VOLUME,
    CFG_BOOT_SND_VOLUME,
    CFG_BGM_VOLUME,
    CFG_DEFAULT_BGM_PATH,

    CFG_XSENSITIVITY,
    CFG_YSENSITIVITY,

    NETCFG_SHOW_ADVANCED_OPTS,
    NETCFG_PS2_IP_ADDR_TYPE,
    NETCFG_PS2_IP_ADDR_0,
    NETCFG_PS2_IP_ADDR_1,
    NETCFG_PS2_IP_ADDR_2,
    NETCFG_PS2_IP_ADDR_3,
    NETCFG_PS2_NETMASK_0,
    NETCFG_PS2_NETMASK_1,
    NETCFG_PS2_NETMASK_2,
    NETCFG_PS2_NETMASK_3,
    NETCFG_PS2_GATEWAY_0,
    NETCFG_PS2_GATEWAY_1,
    NETCFG_PS2_GATEWAY_2,
    NETCFG_PS2_GATEWAY_3,
    NETCFG_PS2_DNS_0,
    NETCFG_PS2_DNS_1,
    NETCFG_PS2_DNS_2,
    NETCFG_PS2_DNS_3,
    NETCFG_SHARE_ADDR_TYPE,
    NETCFG_SHARE_NB_ADDR,
    NETCFG_SHARE_IP_ADDR_0,
    NETCFG_SHARE_IP_ADDR_1,
    NETCFG_SHARE_IP_ADDR_2,
    NETCFG_SHARE_IP_ADDR_3,
    NETCFG_SHARE_IP_ADDR_DOT_0,
    NETCFG_SHARE_IP_ADDR_DOT_1,
    NETCFG_SHARE_IP_ADDR_DOT_2,
    NETCFG_SHARE_PORT,
    NETCFG_SHARE_NAME,
    NETCFG_SHARE_USERNAME,
    NETCFG_SHARE_PASSWORD,
    NETCFG_ETHOPMODE,
    NETCFG_RECONNECT,
    NETCFG_OK,

    CHTCFG_CHEATSOURCE,
    CHTCFG_CHEATCFG,
    CHTCFG_ENABLECHEAT,
    CHTCFG_CHEATMODE,

    GSMCFG_GSMSOURCE,
    GSMCFG_GSCONFIG,
    GSMCFG_ENABLEGSM,
    GSMCFG_GSMVMODE,
    GSMCFG_GSMXOFFSET,
    GSMCFG_GSMYOFFSET,
    GSMCFG_GSMFIELDFIX,

    COMPAT_DMA = 100,
    COMPAT_ALTSTARTUP,
    COMPAT_GAMEID,
    COMPAT_DL_DEFAULTS,

    COMPAT_LOADFROMDISC_ID,

    COMPAT_VMC1_ACTION_ID,
    COMPAT_VMC2_ACTION_ID,
    COMPAT_VMC1_DEFINE_ID,
    COMPAT_VMC2_DEFINE_ID,

    VMC_NAME,
    VMC_SIZE,
    VMC_BUTTON_CREATE,
    VMC_BUTTON_DELETE,
    VMC_STATUS,
    VMC_PROGRESS,
    VMC_REFRESH,

    NETUPD_OPT_UPD_ALL_LBL,
    NETUPD_OPT_UPD_ALL,
    NETUPD_PROGRESS_LBL,
    NETUPD_PROGRESS_PERC_LBL,
    NETUPD_PROGRESS,
    NETUPD_BTN_START,
    NETUPD_BTN_CANCEL,

    OSD_LANGUAGE_SOURCE,
    OSD_LANGUAGE_ENABLE,
    OSD_LANGUAGE_VALUE,
    OSD_TVASPECT_VALUE,
    OSD_VMODE_VALUE,

#ifdef PADEMU
    PADEMU_GLOBAL_BUTTON,
    PADCFG_PADEMU_SOURCE,
    PADCFG_PADEMU_CONFIG,
    PADCFG_PADEMU_ENABLE,
    PADCFG_PADEMU_MODE,
    PADCFG_PADEMU_PORT,
    PADCFG_PADEMU_VIB,
    PADCFG_PADPORT,
    PADCFG_USBDG_MAC,
    PADCFG_USBDG_MAC_STR,
    PADCFG_PAD_MAC,
    PADCFG_PAD_MAC_STR,
    PADCFG_PAIR,
    PADCFG_PAIR_STR,
    PADCFG_BTINFO,
    PADCFG_VID,
    PADCFG_PID,
    PADCFG_REV,
    PADCFG_HCIVER,
    PADCFG_LMPVER,
    PADCFG_MANID,
    PADCFG_FEAT_START,
    PADCFG_FEAT_END = PADCFG_FEAT_START + 64,
    PADCFG_BT_SUPPORTED,
    PADCFG_PADEMU_MTAP,
    PADCFG_PADEMU_MTAP_PORT,
    PADCFG_PADEMU_WORKAROUND,
    PADCFG_PADEMU_WORKAROUND_STR,

    PADMACRO_GLOBAL_BUTTON,
    PADMACRO_CFG_SOURCE,
    PADMACRO_SLOWDOWN_L,
    PADMACRO_SLOWDOWN_TOGGLE_L,
    PADMACRO_SLOWDOWN_R,
    PADMACRO_SLOWDOWN_TOGGLE_R,
    PADMACRO_INVERT_LX,
    PADMACRO_INVERT_LY,
    PADMACRO_INVERT_RX,
    PADMACRO_INVERT_RY,
    PADMACRO_TURBO_SPEED,

    COMPAT_MODE_BASE = 250,
#else
    COMPAT_MODE_BASE = 200,
#endif
};

#define COMPAT_NOEXIT       0x70000000
#define COMPAT_LOADFROMDISC (COMPAT_LOADFROMDISC_ID | COMPAT_NOEXIT)

#define COMPAT_VMC1_ACTION (COMPAT_VMC1_ACTION_ID | COMPAT_NOEXIT)
#define COMPAT_VMC2_ACTION (COMPAT_VMC2_ACTION_ID | COMPAT_NOEXIT)
#define COMPAT_VMC1_DEFINE (COMPAT_VMC1_DEFINE_ID | COMPAT_NOEXIT)
#define COMPAT_VMC2_DEFINE (COMPAT_VMC2_DEFINE_ID | COMPAT_NOEXIT)

#ifdef PADEMU
extern struct UIItem diaPadEmuConfig[];
extern struct UIItem diaPadMacroConfig[];
extern struct UIItem diaPadEmuInfo[];
#endif

extern struct UIItem diaNetConfig[];
extern struct UIItem diaUIConfig[];
extern struct UIItem diaAudioConfig[];
extern struct UIItem diaControllerConfig[];
extern struct UIItem diaCompatConfig[];
extern struct UIItem diaVMCConfig[];
extern struct UIItem diaGSConfig[];
extern struct UIItem diaCheatConfig[];

extern struct UIItem diaConfig[];
extern struct UIItem diaAbout[];
extern struct UIItem diaVMC[];
extern struct UIItem diaNetCompatUpdate[];
extern struct UIItem diaParentalLockConfig[];
extern struct UIItem diaBlockDevicesConfig[];

extern struct UIItem diaOSDConfig[];
#endif
