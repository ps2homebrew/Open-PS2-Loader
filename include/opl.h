#ifndef __USBLD_H
#define __USBLD_H

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
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
#include <fileXio_rpc.h>
#include <smod.h>
#include <smem.h>
#include <debug.h>
#include <ps2smb.h>
#include "config.h"
#ifdef VMC
#include <sys/fcntl.h>
#endif

#define OPL_VERSION		"0.9.3 WIP"
#define OPL_IS_DEV_BUILD	1		//Define if this build is a development build.

//IO type IDs
#define IO_CUSTOM_SIMPLEACTION			1	// handler for parameter-less actions
#define IO_MENU_UPDATE_DEFFERED			2
#define IO_CACHE_LOAD_ART			3	// io call to handle the loading of covers
#define IO_COMPAT_UPDATE_DEFFERED		4

//Codes have been planned to fit the design of the GUI functions within gui.c.
#define OPL_COMPAT_UPDATE_STAT_WIP		0
#define OPL_COMPAT_UPDATE_STAT_DONE		1
#define OPL_COMPAT_UPDATE_STAT_ERROR		-1
#define OPL_COMPAT_UPDATE_STAT_CONN_ERROR	-2
#define OPL_COMPAT_UPDATE_STAT_ABORTED		-3

void setErrorMessage(int strId);
void setErrorMessageWithCode(int strId, int error);
int loadConfig(int types);
int saveConfig(int types, int showUI);
void applyConfig(int themeID, int langID);
void menuDeferredUpdate(void* data);
void moduleUpdateMenu(int mode, int themeChanged);
void handleHdlSrv();
void deinit();

char *gBaseMCDir;

//// IP config

#define IPCONFIG_MAX_LEN	64

enum ETH_OP_MODES{
	ETH_OP_MODE_AUTO	= 0,
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
int gETHOpMode;	//See ETH_OP_MODES.
int gPCShareAddressIsNetBIOS;
char gPCShareNBAddress[17];
int pc_ip[4];
int gPCPort;
char gPCShareName[32];
char gPCUserName[32];
char gPCPassword[32];

//// Settings

// describes what is happening in the network startup thread (>0 means loading, <0 means error)...
int gNetworkStartup;
// true if the ip config should be saved as well
int gNetConfigChanged;
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
int gSelectButton;

// ------------------------------------------------------------------------------------------------------------------------

#ifdef CHEAT
#define CHEAT_VERSION "0.5.3.65.g774d1"

#define MAX_HOOKS	5
#define MAX_CODES	250
#define MAX_CHEATLIST	(MAX_HOOKS*2+MAX_CODES*2)

int	gEnableCheat; // Enables PS2RD Cheat Engine - 0 for Off, 1 for On
int	gCheatMode;  // Cheat Mode - 0 Enable all cheats, 1 Cheats selected by user

int gCheatList[MAX_CHEATLIST];	//Store hooks/codes addr+val pairs
#endif

// ------------------------------------------------------------------------------------------------------------------------

// 0,1,2 scrolling speed
int gScrollSpeed;
// Exit path
char gExitPath[32];
// Disable Debug Colors
int gDisableDebug;
// Default device
int gDefaultDevice;

int gEnableDandR;

char gUSBPrefix[32];
char gETHPrefix[32];

int gRememberLastPlayed;

#ifdef CHEAT
int gShowCheat; // Toggle to reveal "Cheat Settings" on Main Menu
#endif

unsigned char gDefaultBgColor[3];
unsigned char gDefaultTextColor[3];
unsigned char gDefaultSelTextColor[3];
unsigned char gDefaultUITextColor[3];

#define MENU_ITEM_HEIGHT 19

#endif
