#ifndef __USBLD_H
#define __USBLD_H

#include "tamtypes.h"
#include "kernel.h"
#include "sifrpc.h"
#include "fileio.h"
#include "iopcontrol.h"
#include "iopheap.h"
#include "string.h"
#include "loadfile.h"
#include "stdio.h"
#include "sbv_patches.h"
#include <libcdvd.h>
#include <libpad.h>
#include <libmc.h>
#include <debug.h>
#include "gsKit.h"
#include "dmaKit.h"
#include "gsToolkit.h"
#include "malloc.h"
#include "math.h"
#include <libpwroff.h>
#include <fileXio_rpc.h>
#include <debug.h>
#include "config.h"

#define USBLD_VERSION "0.7"

int saveConfig();
int loadConfig();
void applyConfig(int themeID, int langID);
void shutdown();

// global config
struct TConfigSet gConfig;

char* gBaseMCDir;

//// IP config

#define IPCONFIG_MAX_LEN	64

int ps2_ip[4];
int ps2_netmask[4];
int ps2_gateway[4];
int pc_ip[4];
int gPCPort;
char gPCShareName[32];

//// Settings

// describes what is happening in the network startup thread (>0 means loading, <0 means error)...
int gNetworkStartup;
// true if the ip config should be saved as well
int gIPConfigChanged;
/// Indicates the hdd module loading sequence
int gHddStartup;
/// 0 = off, 1 = manual, 2 = auto
int gUSBStartMode;
int gHDDStartMode;
int gETHStartMode;
int gAPPStartMode;
/// true if dev9 irx loaded already
int gDev9_loaded;
/// Sort the game lists automatically
int gAutosort;
/// true if icons and covers Art should be displayed
int gEnableArt;
int gWideScreen;
// 0,1,2 scrolling speed
int gScrollSpeed;
// Exit mode
int gExitMode;
// Disable Debug Colors
int gDisableDebug;
// Default device
int gDefaultDevice;

int gCountIconsCache;
int gCountCoversCache;
int gCountBackgroundsCache;

//// GUI

char *infotxt;

unsigned char gDefaultBgColor[3];
unsigned char gDefaultTextColor[3];
unsigned char gDefaultSelTextColor[3];
unsigned char gDefaultUITextColor[3];

#define MENU_ITEM_HEIGHT 19

//// SYSTEM

void *usbd_irx;
int size_usbd_irx;

void LoadHdldSvr(void);
void UnloadHdldSvr(int hdd_init, int network_init);

void delay(int count);
unsigned int USBA_crc32(char *string);
int sysGetDiscID(char *discID);
void sysReset(void);
void sysPowerOff(void);
int sysPcmciaCheck(void);
void sysLaunchLoaderElf(char *filename, char *mode_str, int size_cdvdman_irx, void **cdvdman_irx, int compatflags, int alt_ee_core);
int sysExecElf(char *path, int argc, char **argv);
int sysPS3Detect(void);
int sysSetIPConfig(char* ipconfig);
#endif
