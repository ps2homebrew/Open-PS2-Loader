#ifndef __USBLD_H
#define __USBLD_H

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <string.h>
#include <loadfile.h>
#include <stdio.h>
#include <sbv_patches.h>
#include <libcdvd.h>
#include <libpad.h>
#include <libmc.h>
#include <debug.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <malloc.h>
#include <math.h>
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

#define USBLD_VERSION "0.9.3"

#define IO_MENU_UPDATE_DEFFERED 2

void setErrorMessage(int strId);
void setErrorMessageWithCode(int strId, int error);
int loadConfig(int types);
int saveConfig(int types, int showUI);
void applyConfig(int themeID, int langID);
void menuDeferredUpdate(void* data);
void moduleUpdateMenu(int mode, int themeChanged);
void handleHdlSrv();
void shutdown();

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

int ps2_ip[4];
int ps2_netmask[4];
int ps2_gateway[4];
int gETHOpMode;	//See ETH_OP_MODES.
int pc_ip[4];
int gPCPort;
char gPCShareName[32];
char gPCUserName[32];
char gPCPassword[32];

//// Settings

// describes what is happening in the network startup thread (>0 means loading, <0 means error)...
int gNetworkStartup;
// true if the ip config should be saved as well
int gIPConfigChanged;
int gHDDSpindown;
/// Indicates the hdd module loading sequence
int gHddStartup;
/// 0 = off, 1 = manual, 2 = auto
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

// ------------------------------------------------------------------------------------------------------------------------

/// DTV 576 Progressive Scan (720x576)
#define GS_MODE_DTV_576P  0x53

/// DTV 1080 Progressive Scan (1920x1080)
#define GS_MODE_DTV_1080P  0x54

#ifdef GSM
#define GSM_VERSION "0.36.R"

#define PS1_VMODE	1
#define SDTV_VMODE	2
#define HDTV_VMODE	3
#define VGA_VMODE	4

#define make_display_magic_number(dh, dw, magv, magh, dy, dx) \
	(((u64)(dh)<<44) | ((u64)(dw)<<32) | ((u64)(magv)<<27) | \
	((u64)(magh)<<23) | ((u64)(dy)<<12)   | ((u64)(dx)<<0)     )

typedef struct predef_vmode_struct {
	u8	category;
	char desc[34];
	u8	interlace;
	u8	mode;
	u8	ffmd;
	u64	display;
	u64	syncv;
} predef_vmode_struct;

int	gEnableGSM; // Enables GSM - 0 for Off, 1 for On
int	gGSMVMode;  // See the related predef_vmode
int	gGSMXOffset; // 0 - Off, Any other positive or negative value - Relative position for X Offset
int	gGSMYOffset; // 0 - Off, Any other positive or negative value - Relative position for Y Offset
int	gGSMSkipVideos; // 0 - Off, 1 - On
#endif

#ifdef CHEAT
#define CHEAT_VERSION "0.5.3.65.g774d1"

#define MAX_HOOKS	5
#define MAX_CODES	250
#define MAX_CHEATLIST	(MAX_HOOKS*2+MAX_CODES*2)

int	gEnableCheat; // Enables PS2RD Cheat Engine - 0 for Off, 1 for On
int	gCheatMode;  // Cheat Mode - 0 Enable all cheats, 1 Cheats selected by user

int gCheatList;	//Store hooks/codes addr+val pairs
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

int gCheckUSBFragmentation;
char gUSBPrefix[32];
char gETHPrefix[32];

int gRememberLastPlayed;

#ifdef CHEAT
int gShowCheat; // Toggle to reveal "Cheat Settings" on Main Menu
#endif

char *infotxt;

unsigned char gDefaultBgColor[3];
unsigned char gDefaultTextColor[3];
unsigned char gDefaultSelTextColor[3];
unsigned char gDefaultUITextColor[3];

#define MENU_ITEM_HEIGHT 19

#endif
