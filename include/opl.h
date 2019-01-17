#ifndef __OPL_H
#define __OPL_H

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <hdd-ioctl.h>
#include "opl-hdd-ioctl.h"
#include <iopcontrol.h>
#include <iopheap.h>
#include <string.h>
#include <loadfile.h>
#include <stdio.h>
#include <sbv_patches.h>
#include <libcdvd.h>
#include <libpad.h>
#include <libmc.h>
#include <netman.h>
#include <ps2ips.h>
#include <debug.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <malloc.h>
#include <math.h>
#include <osd_config.h>
#include <libpwroff.h>
#include <usbhdfsd-common.h>
#include <fileXio_rpc.h>
#include <smod.h>
#include <smem.h>
#include <debug.h>
#include <ps2smb.h>
#include "config.h"
#include <sys/fcntl.h>

// Last Played Auto Start
#include <time.h>

#define OPL_IS_DEV_BUILD 1 //Define if this build is a development build.

#ifdef OPL_IS_DEV_BUILD
#define OPL_FOLDER "CFG-DEV"
#else
#define OPL_FOLDER "CFG"
#endif

//Master password for disabling the parental lock.
#define OPL_PARENTAL_LOCK_MASTER_PASS	"989765"

//IO type IDs
#define IO_CUSTOM_SIMPLEACTION 1 // handler for parameter-less actions
#define IO_MENU_UPDATE_DEFFERED 2
#define IO_CACHE_LOAD_ART 3 // io call to handle the loading of covers
#define IO_COMPAT_UPDATE_DEFFERED 4

//Codes have been planned to fit the design of the GUI functions within gui.c.
#define OPL_COMPAT_UPDATE_STAT_WIP 0
#define OPL_COMPAT_UPDATE_STAT_DONE 1
#define OPL_COMPAT_UPDATE_STAT_ERROR -1
#define OPL_COMPAT_UPDATE_STAT_CONN_ERROR -2
#define OPL_COMPAT_UPDATE_STAT_ABORTED -3

#define OPL_VMODE_CHANGE_CONFIRMATION_TIMEOUT_MS 10000

int oplPath2Mode(const char *path);
int oplGetAppImage(const char *device, char *folder, int isRelative, char *value, char *suffix, GSTEXTURE *resultTex, short psm);
int oplScanApps(int (*callback)(const char *path, config_set_t *appConfig, void *arg), void *arg);

void setErrorMessage(int strId);
void setErrorMessageWithCode(int strId, int error);
int loadConfig(int types);
int saveConfig(int types, int showUI);
void applyConfig(int themeID, int langID);
void menuDeferredUpdate(void *data);
void moduleUpdateMenu(int mode, int themeChanged);
void handleHdlSrv();
void deinit(int exception, int modeSelected);

char *gBaseMCDir;

enum ETH_OP_MODES {
    ETH_OP_MODE_AUTO = 0,
    ETH_OP_MODE_100M_FDX,
    ETH_OP_MODE_100M_HDX,
    ETH_OP_MODE_10M_FDX,
    ETH_OP_MODE_10M_HDX,

    ETH_OP_MODE_COUNT
};

int ps2_ip_use_dhcp;
int ps2_ip[4];
int ps2_netmask[4];
int ps2_gateway[4];
int ps2_dns[4];
int gETHOpMode; //See ETH_OP_MODES.
int gPCShareAddressIsNetBIOS;
int pc_ip[4];
int gPCPort;
//Please keep these string lengths in-sync with the limits within CDVDMAN.
char gPCShareNBAddress[17];
char gPCShareName[32];
char gPCUserName[32];
char gPCPassword[32];

//// Settings

// describes what is happening in the network startup thread (>0 means loading, <0 means error)...
int gNetworkStartup;
int gHDDSpindown;
/// Refer to enum START_MODE within iosupport.h
int gUSBStartMode;
int gHDDStartMode;
int gETHStartMode;
int gAPPStartMode;

int gAutosort;
int gAutoRefresh;
int gUseInfoScreen;
int gEnableArt;
int gWideScreen;
int gVMode; // 0 - Auto, 1 - PAL, 2 - NTSC
int gXOff;
int gYOff;
int gOverscan;
int gSelectButton;
int gGameListCache;

int gEnableSFX;
int gEnableBootSND;
int gSFXVolume;
int gBootSndVolume;

int gFadeDelay;
int toggleSfx;

#ifdef IGS
#define IGS_VERSION "0.1"
#endif

// ------------------------------------------------------------------------------------------------------------------------

#ifdef PADEMU
int gEnablePadEmu;
int gPadEmuSettings;
#endif

// ------------------------------------------------------------------------------------------------------------------------

// 0,1,2 scrolling speed
int gScrollSpeed;
// Exit path
char gExitPath[32];
// Disable Debug Colors
int gDisableDebug;

int gPS2Logo;

// Default device
int gDefaultDevice;

int gEnableWrite;

int gCheckUSBFragmentation;
//These prefixes are relative to the device's name (meaning that they do not include the device name).
char gUSBPrefix[32];
char gETHPrefix[32];

int gRememberLastPlayed;

// Last Played Auto Start
int KeyPressedOnce;
int gAutoStartLastPlayed;
int RemainSecs, DisableCron;
double CronStart;

unsigned char gDefaultBgColor[3];
unsigned char gDefaultTextColor[3];
unsigned char gDefaultSelTextColor[3];
unsigned char gDefaultUITextColor[3];

#define MENU_ITEM_HEIGHT 19

// BLURT output
//char blurttext[128];
//#define BLURT	snprintf(blurttext, sizeof(blurttext), "%s\\%s(%d)", __FILE__ , __func__ , __LINE__ );delay(10);
//#define BLURT snprintf(blurttext, sizeof(blurttext), "%s(%d)", blurttext, __LINE__);
#endif
