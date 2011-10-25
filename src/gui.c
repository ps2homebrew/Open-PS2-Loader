/*
 Copyright 2010, Volca
 Licenced under Academic Free License version 3.0
 Review OpenUsbLd README & LICENSE files for further details.
 */

#include "include/usbld.h"
#include "include/gui.h"
#include "include/renderman.h"
#include "include/menusys.h"
#include "include/fntsys.h"
#include "include/ioman.h"
#include "include/lang.h"
#include "include/themes.h"
#include "include/pad.h"
#include "include/util.h"
#include "include/config.h"
#include "include/system.h"

#include <stdlib.h>
#include <libvux.h>

static int gScheduledOps;
static int gCompletedOps;
static int gTerminate;
static int gInitComplete;

static gui_callback_t gFrameHook;

static s32 gSemaId;
static s32 gGUILockSemaId;
static ee_sema_t gQueueSema;

static int screenWidth;
static float wideScreenScale;
static int screenHeight;

// forward decl.
static void guiShow();

#ifdef __DEBUG

#include <timer.h>

#define CLOCKS_PER_MILISEC 147456

// debug version displays an FPS meter
static u32 curtime = 0;
static u32 time_since_last = 0;
static u32 time_render = 0;

#endif

struct gui_update_list_t {
	struct gui_update_t* item;
	struct gui_update_list_t* next;
};

struct gui_update_list_t *gUpdateList;
struct gui_update_list_t *gUpdateEnd;

typedef struct {
	void (*handleInput)(void);
	void (*renderScreen)(void);
	short inMenu;
} gui_screen_handler_t;

static gui_screen_handler_t screenHandlers[] = {{ &menuHandleInputMain, &menuRenderMain, 0 },
												{ &menuHandleInputMenu, &menuRenderMenu, 1 },
												{ &menuHandleInputInfo, &menuRenderInfo, 1 } };

// default screen handler (menu screen)
static gui_screen_handler_t *screenHandler = &screenHandlers[GUI_SCREEN_MENU];

// screen transition handling
static gui_screen_handler_t *screenHandlerTarget = NULL;
static int transIndex, transMax, transitionX, transitionY;

// Helper perlin noise data
#define PLASMA_H 32
#define PLASMA_W 32
#define PLASMA_ROWS_PER_FRAME 6
#define FADE_SIZE 256

static GSTEXTURE gBackgroundTex;
static int pperm[512];
static float fadetbl[FADE_SIZE + 1];

static VU_VECTOR pgrad3[12] = {{ 1, 1, 0, 1 }, { -1, 1, 0, 1 }, { 1, -1, 0, 1 }, { -1, -1, 0, 1 },
								{ 1, 0, 1, 1 }, { -1, 0, 1, 1 }, { 1, 0, -1, 1 }, { -1, 0, -1, 1 },
								{ 0, 1, 1, 1 },	{ 0, -1, 1, 1 }, { 0, 1, -1, 1 }, { 0, -1, -1, 1 } };

void guiReloadScreenExtents() {
	rmGetScreenExtents(&screenWidth, &screenHeight);
}

void guiInit(void) {
	guiFrameId = 0;
	guiInactiveFrames = 0;

	gFrameHook = NULL;
	gTerminate = 0;
	gInitComplete = 0;
	gScheduledOps = 0;
	gCompletedOps = 0;

	gUpdateList = NULL;
	gUpdateEnd = NULL;

	gQueueSema.init_count = 1;
	gQueueSema.max_count = 1;
	gQueueSema.option = 0;

	gSemaId = CreateSema(&gQueueSema);
	gGUILockSemaId = CreateSema(&gQueueSema);

	guiReloadScreenExtents();

	// background texture - for perlin
	gBackgroundTex.Width = PLASMA_W;
	gBackgroundTex.Height = PLASMA_H;
	gBackgroundTex.Mem = memalign(128, PLASMA_W * PLASMA_H * 4);
	gBackgroundTex.PSM = GS_PSM_CT32;
	gBackgroundTex.Filter = GS_FILTER_LINEAR;
	gBackgroundTex.Vram = 0;
	gBackgroundTex.VramClut = 0;
	gBackgroundTex.Clut = NULL;

	wideScreenScale = 1.0f;

	// Precalculate the values for the perlin noise plasma
	int i;
	for (i = 0; i < 256; ++i) {
		pperm[i] = rand() % 256;
		pperm[i + 256] = pperm[i];
	}

	for (i = 0; i <= FADE_SIZE; ++i) {
		float t = (float) (i) / FADE_SIZE;

		fadetbl[i] = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
	}
}

void guiEnd() {
	if (gBackgroundTex.Mem)
		free(gBackgroundTex.Mem);

	DeleteSema(gSemaId);
	DeleteSema(gGUILockSemaId);
}

void guiLock(void) {
	WaitSema(gGUILockSemaId);
}

void guiUnlock(void) {
	SignalSema(gGUILockSemaId);
}

void guiStartFrame(void) {
#ifdef __DEBUG
	u32 newtime = cpu_ticks() / CLOCKS_PER_MILISEC;
	time_since_last = newtime - curtime;
	curtime = newtime;
#endif

	guiLock();
	rmStartFrame();
	guiFrameId++;
}

void guiEndFrame(void) {
	guiUnlock();
	rmFlush();

#ifdef __DEBUG
	u32 newtime = cpu_ticks() / CLOCKS_PER_MILISEC;
	time_render = newtime - curtime;
#endif

	rmEndFrame();
}

void guiShowAbout() {
	char OPLVersion[64];
	snprintf(OPLVersion, 64, _l(_STR_OUL_VER), USBLD_VERSION);

#ifdef VMC
	strcat(OPLVersion, " VMC");
#endif
#ifdef __CHILDPROOF
	strcat(OPLVersion, " CHILDPROOF");
#endif

	diaSetLabel(diaAbout, 1, OPLVersion);

	diaExecuteDialog(diaAbout, -1, 1, NULL);
}

void guiShowConfig() {
	// configure the enumerations
	const char* deviceNames[] = { _l(_STR_USB_GAMES), _l(_STR_NET_GAMES), _l(_STR_HDD_GAMES), NULL };
	const char* deviceModes[] = { _l(_STR_OFF), _l(_STR_MANUAL), _l(_STR_AUTO), NULL };

	diaSetEnum(diaConfig, CFG_DEFDEVICE, deviceNames);
	diaSetEnum(diaConfig, CFG_USBMODE, deviceModes);
	diaSetEnum(diaConfig, CFG_HDDMODE, deviceModes);
	diaSetEnum(diaConfig, CFG_ETHMODE, deviceModes);
	diaSetEnum(diaConfig, CFG_APPMODE, deviceModes);

	diaSetInt(diaConfig, CFG_DEBUG, gDisableDebug);
	diaSetString(diaConfig, CFG_EXITTO, gExitPath);
	diaSetInt(diaConfig, CFG_DANDROP, gEnableDandR);
	diaSetInt(diaConfig, CFG_HDDSPINDOWN, gHDDSpindown);
	diaSetInt(diaConfig, CFG_CHECKUSBFRAG, gCheckUSBFragmentation);
	diaSetInt(diaConfig, CFG_USBDELAY, gUSBDelay);
	diaSetString(diaConfig, CFG_USBPREFIX, gUSBPrefix);
	diaSetInt(diaConfig, CFG_LASTPLAYED, gRememberLastPlayed);
	diaSetInt(diaConfig, CFG_DEFDEVICE, gDefaultDevice);
	diaSetInt(diaConfig, CFG_USBMODE, gUSBStartMode);
	diaSetInt(diaConfig, CFG_HDDMODE, gHDDStartMode);
	diaSetInt(diaConfig, CFG_ETHMODE, gETHStartMode);
	diaSetInt(diaConfig, CFG_APPMODE, gAPPStartMode);

	int ret = diaExecuteDialog(diaConfig, -1, 1, NULL);
	if (ret) {
		diaGetString(diaConfig, CFG_EXITTO, gExitPath);
		diaGetInt(diaConfig, CFG_DEBUG, &gDisableDebug);
		diaGetInt(diaConfig, CFG_DANDROP, &gEnableDandR);
		diaGetInt(diaConfig, CFG_HDDSPINDOWN, &gHDDSpindown);
		diaGetInt(diaConfig, CFG_CHECKUSBFRAG, &gCheckUSBFragmentation);
		diaGetInt(diaConfig, CFG_USBDELAY, &gUSBDelay);
		diaGetString(diaConfig, CFG_USBPREFIX, gUSBPrefix);
		diaGetInt(diaConfig, CFG_LASTPLAYED, &gRememberLastPlayed);
		diaGetInt(diaConfig, CFG_DEFDEVICE, &gDefaultDevice);
		diaGetInt(diaConfig, CFG_USBMODE, &gUSBStartMode);
		diaGetInt(diaConfig, CFG_HDDMODE, &gHDDStartMode);
		diaGetInt(diaConfig, CFG_ETHMODE, &gETHStartMode);
		diaGetInt(diaConfig, CFG_APPMODE, &gAPPStartMode);

		applyConfig(-1, -1, gVMode, gVSync);
	}
}

static int curTheme = -1;

static int guiUIUpdater(int modified) {
	if (modified) {
		int temp;
		diaGetInt(diaUIConfig, UICFG_THEME, &temp);
		if (temp != curTheme) {
			curTheme = temp;
			if (temp == 0) {
				diaUIConfig[32].type = UI_COLOUR; // Must be correctly set before doing the diaS/GetColor !!
				diaUIConfig[36].type = UI_COLOUR;
				diaUIConfig[40].type = UI_COLOUR;
				diaUIConfig[44].type = UI_COLOUR;
				diaSetColor(diaUIConfig, UICFG_BGCOL, gDefaultBgColor);
				diaSetColor(diaUIConfig, UICFG_UICOL, gDefaultUITextColor);
				diaSetColor(diaUIConfig, UICFG_TXTCOL, gDefaultTextColor);
				diaSetColor(diaUIConfig, UICFG_SELCOL, gDefaultSelTextColor);
			} else if (temp == thmGetGuiValue()) {
				diaUIConfig[32].type = UI_COLOUR;
				diaUIConfig[36].type = UI_COLOUR;
				diaUIConfig[40].type = UI_COLOUR;
				diaUIConfig[44].type = UI_COLOUR;
				diaSetColor(diaUIConfig, UICFG_BGCOL, gTheme->bgColor);
				diaSetU64Color(diaUIConfig, UICFG_UICOL, gTheme->uiTextColor);
				diaSetU64Color(diaUIConfig, UICFG_TXTCOL, gTheme->textColor);
				diaSetU64Color(diaUIConfig, UICFG_SELCOL, gTheme->selTextColor);
			} else {
				diaUIConfig[32].type = UI_SPACER;
				diaUIConfig[36].type = UI_SPACER;
				diaUIConfig[40].type = UI_SPACER;
				diaUIConfig[44].type = UI_SPACER;
			}

			temp = !temp;
			diaSetEnabled(diaUIConfig, UICFG_BGCOL, temp);
			diaSetEnabled(diaUIConfig, UICFG_UICOL, temp);
			diaSetEnabled(diaUIConfig, UICFG_TXTCOL, temp);
			diaSetEnabled(diaUIConfig, UICFG_SELCOL, temp);
		}
	}

	return 0;
}

void guiShowUIConfig() {
	curTheme = -1;

	// configure the enumerations
	const char* scrollSpeeds[] = { _l(_STR_SLOW), _l(_STR_MEDIUM),	_l(_STR_FAST), NULL };
	const char* vmodeNames[] = { "AUTO", "PAL", "NTSC", NULL };
	diaSetEnum(diaUIConfig, UICFG_SCROLL, scrollSpeeds);
	diaSetEnum(diaUIConfig, UICFG_THEME, (const char **) thmGetGuiList());
	diaSetEnum(diaUIConfig, UICFG_LANG, (const char **) lngGetGuiList());
	diaSetEnum(diaUIConfig, UICFG_VMODE, vmodeNames);

	diaSetInt(diaUIConfig, UICFG_SCROLL, gScrollSpeed);
	diaSetInt(diaUIConfig, UICFG_THEME, thmGetGuiValue());
	diaSetInt(diaUIConfig, UICFG_LANG, lngGetGuiValue());
	guiUIUpdater(1);
	diaSetInt(diaUIConfig, UICFG_AUTOSORT, gAutosort);
	diaSetInt(diaUIConfig, UICFG_AUTOREFRESH, gAutoRefresh);
	diaSetInt(diaUIConfig, UICFG_INFOPAGE, gUseInfoScreen);
	diaSetInt(diaUIConfig, UICFG_COVERART, gEnableArt);
	diaSetInt(diaUIConfig, UICFG_WIDESCREEN, gWideScreen);
	diaSetInt(diaUIConfig, UICFG_VMODE, gVMode);
	diaSetInt(diaUIConfig, UICFG_VSYNC, gVSync);

	int ret = diaExecuteDialog(diaUIConfig, -1, 1, guiUIUpdater);
	if (ret) {
		int themeID = -1, langID = -1, newVMode = gVMode, newVSync = gVSync;
		diaGetInt(diaUIConfig, UICFG_SCROLL, &gScrollSpeed);
		diaGetInt(diaUIConfig, UICFG_LANG, &langID);
		diaGetInt(diaUIConfig, UICFG_THEME, &themeID);
		if (themeID == 0) {
			diaGetColor(diaUIConfig, UICFG_BGCOL, gDefaultBgColor);
			diaGetColor(diaUIConfig, UICFG_UICOL, gDefaultUITextColor);
			diaGetColor(diaUIConfig, UICFG_TXTCOL, gDefaultTextColor);
			diaGetColor(diaUIConfig, UICFG_SELCOL, gDefaultSelTextColor);
		}
		diaGetInt(diaUIConfig, UICFG_AUTOSORT, &gAutosort);
		diaGetInt(diaUIConfig, UICFG_AUTOREFRESH, &gAutoRefresh);
		diaGetInt(diaUIConfig, UICFG_INFOPAGE, &gUseInfoScreen);
		diaGetInt(diaUIConfig, UICFG_COVERART, &gEnableArt);
		diaGetInt(diaUIConfig, UICFG_WIDESCREEN, &gWideScreen);
		diaGetInt(diaUIConfig, UICFG_VMODE, &newVMode);
		diaGetInt(diaUIConfig, UICFG_VSYNC, &newVSync);

		applyConfig(themeID, langID, newVMode, newVSync);
	}
}

void guiShowIPConfig() {
	size_t i;
	// upload current values
	for (i = 0; i < 4; ++i) {
		diaSetInt(diaIPConfig, 2 + i, ps2_ip[i]);
		diaSetInt(diaIPConfig, 6 + i, ps2_netmask[i]);
		diaSetInt(diaIPConfig, 10 + i, ps2_gateway[i]);
		diaSetInt(diaIPConfig, 14 + i, pc_ip[i]);
	}

	diaSetInt(diaIPConfig, 18, gPCPort);
	diaSetString(diaIPConfig, 19, gPCShareName);
	diaSetString(diaIPConfig, 20, gPCUserName);
	diaSetString(diaIPConfig, 21, gPCPassword);

	// show dialog
	if (diaExecuteDialog(diaIPConfig, -1, 1, NULL)) {
		// Ok pressed, store values
		for (i = 0; i < 4; ++i) {
			diaGetInt(diaIPConfig, 2 + i, &ps2_ip[i]);
			diaGetInt(diaIPConfig, 6 + i, &ps2_netmask[i]);
			diaGetInt(diaIPConfig, 10 + i, &ps2_gateway[i]);
			diaGetInt(diaIPConfig, 14 + i, &pc_ip[i]);
		}

		diaGetInt(diaIPConfig, 18, &gPCPort);
		diaGetString(diaIPConfig, 19, gPCShareName);
		diaGetString(diaIPConfig, 20, gPCUserName);
		diaGetString(diaIPConfig, 21, gPCPassword);
		gIPConfigChanged = 1;

		applyConfig(-1, -1, gVMode, gVSync);
	}
}

int guiShowKeyboard(char* value, int maxLength) {
	char tmp[maxLength];
	strncpy(tmp, value, maxLength);

	int result = diaShowKeyb(tmp, maxLength);
	if (result) {
		strncpy(value, tmp, maxLength);
		value[maxLength - 1] = '\0';
	}

	return result;
}

#ifdef VMC
typedef struct { // size = 76
	int VMC_status; // 0=available, 1=busy
	int VMC_error;
	int VMC_progress;
	char VMC_msg[64];
}statusVMCparam_t;

#define OPERATION_CREATE	0
#define OPERATION_CREATING	1
#define OPERATION_ABORTING	2
#define OPERATION_ENDING	3
#define OPERATION_END		4

static short vmc_refresh;
static int vmc_operation;
static statusVMCparam_t vmc_status;

int guiVmcNameHandler(char* text, int maxLen) {
	int result = diaShowKeyb(text, maxLen);

	if (result)
	vmc_refresh = 1;

	return result;
}

static int guiRefreshVMCConfig(item_list_t *support, char* name) {
	int size = support->itemCheckVMC(name, 0);

	if (size != -1) {
		diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_MODIFY));
		diaSetLabel(diaVMC, VMC_STATUS, _l(_STR_VMC_FILE_EXISTS));

		if (size == 8)
			diaSetInt(diaVMC, VMC_SIZE, 0);
		else if (size == 16)
			diaSetInt(diaVMC, VMC_SIZE, 1);
		else if (size == 32)
			diaSetInt(diaVMC, VMC_SIZE, 2);
		else if (size == 64)
			diaSetInt(diaVMC, VMC_SIZE, 3);
		else {
			diaSetInt(diaVMC, VMC_SIZE, 0);
			diaSetLabel(diaVMC, VMC_STATUS, _l(_STR_VMC_FILE_ERROR));
		}

		if (gEnableDandR) {
			diaSetEnabled(diaVMC, VMC_SIZE, 1);
			diaVMC[20].type = UI_SPLITTER;
		}
		else {
			diaSetEnabled(diaVMC, VMC_SIZE, 0);
			diaVMC[20].type = UI_TERMINATOR;
		}
	}
	else {
		diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_CREATE));
		diaSetLabel(diaVMC, VMC_STATUS, _l(_STR_VMC_FILE_NEW));

		diaSetInt(diaVMC, VMC_SIZE, 0);
		diaSetEnabled(diaVMC, VMC_SIZE, 1);
		diaVMC[20].type = UI_TERMINATOR;
	}

	return size;
}

static int guiVMCUpdater(int modified) {
	if (vmc_refresh) {
		vmc_refresh = 0;
		return VMC_REFRESH;
	}

	if ((vmc_operation == OPERATION_CREATING) || (vmc_operation == OPERATION_ABORTING)) {
		int result = fileXioDevctl("genvmc:", 0xC0DE0003, NULL, 0, (void*) &vmc_status, sizeof(vmc_status));
		if (result == 0) {
			diaSetLabel(diaVMC, VMC_STATUS, vmc_status.VMC_msg);
			diaSetInt(diaVMC, VMC_PROGRESS, vmc_status.VMC_progress);

			if (vmc_status.VMC_error != 0)
				LOG("genvmc updater: %d\n", vmc_status.VMC_error);

			if (vmc_status.VMC_status == 0x00) {
				diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_OK));
				vmc_operation = OPERATION_ENDING;
				return VMC_BUTTON_CREATE;
			}
		}
		else
			LOG("status result: %d\n", result);
	}

	return 0;
}

static int guiShowVMCConfig(int id, item_list_t *support, char *VMCName, int slot, int validate) {
	int result = validate ? VMC_BUTTON_CREATE : 0;
	char vmc[32];

	if (strlen(VMCName))
		strncpy(vmc, VMCName, 32);
	else {
		if (validate)
			return 1; // nothing to validate if no user input

		char* startup = support->itemGetStartup(id);
		snprintf(vmc, 32, "%s_%d", startup, slot);
	}

	vmc_refresh = 0;
	vmc_operation = OPERATION_CREATE;
	diaSetEnabled(diaVMC, VMC_NAME, 1);
	diaSetEnabled(diaVMC, VMC_SIZE, 1);
	diaSetInt(diaVMC, VMC_PROGRESS, 0);

	const char* VMCSizes[] = {"8 Mb", "16 Mb", "32 Mb", "64 Mb", NULL};
	diaSetEnum(diaVMC, VMC_SIZE, VMCSizes);
	int size = guiRefreshVMCConfig(support, vmc);
	diaSetString(diaVMC, VMC_NAME, vmc);

	do {
		if (result == VMC_BUTTON_CREATE) {
			if (vmc_operation == OPERATION_CREATE) { // User start creation of VMC
				int sizeUI;
				diaGetInt(diaVMC, VMC_SIZE, &sizeUI);
				if (sizeUI == 1)
				sizeUI = 16;
				else if (sizeUI == 2)
				sizeUI = 32;
				else if (sizeUI == 3)
				sizeUI = 64;
				else
				sizeUI = 8;

				if (sizeUI != size) {
					support->itemCheckVMC(vmc, sizeUI);

					diaSetEnabled(diaVMC, VMC_NAME, 0);
					diaSetEnabled(diaVMC, VMC_SIZE, 0);
					diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_ABORT));
					vmc_operation = OPERATION_CREATING;
				}
				else
					break;
			}
			else if (vmc_operation == OPERATION_ENDING) {
				if (validate)
					break; // directly close VMC config dialog

				vmc_operation = OPERATION_END;
			}
			else if (vmc_operation == OPERATION_END) { // User closed creation dialog of VMC
				break;
			}
			else if (vmc_operation == OPERATION_CREATING) { // User canceled creation of VMC
				fileXioDevctl("genvmc:", 0xC0DE0002, NULL, 0, NULL, 0);
				vmc_operation = OPERATION_ABORTING;
			}
		}
		else if (result == VMC_BUTTON_DELETE) {
			if (guiMsgBox(_l(_STR_DELETE_WARNING), 1, diaVMC)) {
				support->itemCheckVMC(vmc, -1);
				diaSetString(diaVMC, VMC_NAME, "");
				break;
			}
		}
		else if (result == VMC_REFRESH) { // User changed the VMC name
			diaGetString(diaVMC, VMC_NAME, vmc);
			size = guiRefreshVMCConfig(support, vmc);
		}

		result = diaExecuteDialog(diaVMC, result, 1, &guiVMCUpdater);

		if ((result == 0) && (vmc_operation == OPERATION_CREATE))
			break;
	} while (1);

	return result;
}

#endif

int guiShowCompatConfig(int id, item_list_t *support) {
	int dmaMode = -1, compatMode = 0;
	char* startup = support->itemGetStartup(id);
	compatMode = support->itemGetCompatibility(id, &dmaMode);

	if (dmaMode != -1) {
		const char* dmaModes[] = { "MDMA 0", "MDMA 1", "MDMA 2", "UDMA 0",	"UDMA 1", "UDMA 2", "UDMA 3", "UDMA 4", "UDMA 5", "UDMA 6",	NULL };
		diaSetEnum(diaCompatConfig, COMPAT_MODE_BASE + COMPAT_MODE_COUNT, dmaModes);
		diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + COMPAT_MODE_COUNT, dmaMode);
	} else {
		const char* dmaModes[] = { NULL };
		diaSetEnum(diaCompatConfig, COMPAT_MODE_BASE + COMPAT_MODE_COUNT, dmaModes);
		diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + COMPAT_MODE_COUNT, 0);
	}

	diaSetLabel(diaCompatConfig, COMPAT_GAME, support->itemGetName(id));

	int i, result = -1;
	for (i = 0; i < COMPAT_MODE_COUNT; ++i)
		diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + i, (compatMode & (1 << i)) > 0 ? 1 : 0);

	// Find out the current game ID
	char hexid[32];
	configGetDiscID(startup, hexid);
	diaSetString(diaCompatConfig, COMPAT_GAMEID, hexid);

#ifdef VMC
	int mode = support->mode;

	char vmc1[32];
	configGetVMC(startup, vmc1, mode, 0);
	diaSetLabel(diaCompatConfig, COMPAT_VMC1_DEFINE, vmc1);

	char vmc2[32]; // required as diaSetLabel use pointer to value
	configGetVMC(startup, vmc2, mode, 1);
	diaSetLabel(diaCompatConfig, COMPAT_VMC2_DEFINE, vmc2);
#endif

	// show dialog
	do {
#ifdef VMC
		if (strlen(vmc1))
			diaSetLabel(diaCompatConfig, COMPAT_VMC1_ACTION, _l(_STR_RESET));
		else
			diaSetLabel(diaCompatConfig, COMPAT_VMC1_ACTION, _l(_STR_USE_GENERIC));
		if (strlen(vmc2))
			diaSetLabel(diaCompatConfig, COMPAT_VMC2_ACTION, _l(_STR_RESET));
		else
			diaSetLabel(diaCompatConfig, COMPAT_VMC2_ACTION, _l(_STR_USE_GENERIC));
#endif

		result = diaExecuteDialog(diaCompatConfig, result, 1, NULL);

		if (result == COMPAT_LOADFROMDISC) {
			char hexDiscID[15];
			if (sysGetDiscID(hexDiscID) >= 0)
				diaSetString(diaCompatConfig, COMPAT_GAMEID, hexDiscID);
			else
				guiMsgBox(_l(_STR_ERROR_LOADING_ID), 0, NULL);
		}
#ifdef VMC
		else if (result == COMPAT_VMC1_DEFINE) {
			if(guiShowVMCConfig(id, support, vmc1, 0, 0))
				diaGetString(diaVMC, VMC_NAME, vmc1);
		} else if (result == COMPAT_VMC2_DEFINE) {
			if(guiShowVMCConfig(id, support, vmc2, 1, 0))
				diaGetString(diaVMC, VMC_NAME, vmc2);
		} else if (result == COMPAT_VMC1_ACTION) {
			if (strlen(vmc1))
				vmc1[0] = '\0';
			else
				snprintf(vmc1, 32, "generic_%d", 0);
		} else if (result == COMPAT_VMC2_ACTION) {
			if (strlen(vmc2))
				vmc2[0] = '\0';
			else
				snprintf(vmc2, 32, "generic_%d", 1);
		}
#endif
	} while (result >= COMPAT_NOEXIT);

	if (result == COMPAT_REMOVE) {
		configRemoveDiscID(startup);
#ifdef VMC
		configRemoveVMC(startup, mode, 0);
		configRemoveVMC(startup, mode, 1);
#endif
		support->itemSetCompatibility(id, 0, 7);
		saveConfig(CONFIG_COMPAT | CONFIG_DNAS | CONFIG_VMC, 1);
	} else if (result > 0) { // test button pressed or save button
		compatMode = 0;
		for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
			int mdpart;
			diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + i, &mdpart);
			compatMode |= (mdpart ? 1 : 0) << i;
		}

		if (dmaMode != -1)
			diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + COMPAT_MODE_COUNT, &dmaMode);

		support->itemSetCompatibility(id, compatMode, dmaMode);

		diaGetString(diaCompatConfig, COMPAT_GAMEID, hexid);
		if (hexid[0] != '\0')
			configSetDiscID(startup, hexid);
#ifdef VMC
		configSetVMC(startup, vmc1, mode, 0);
		configSetVMC(startup, vmc2, mode, 1);
		guiShowVMCConfig(id, support, vmc1, 0, 1);
		guiShowVMCConfig(id, support, vmc2, 1, 1);
#endif

		if (result == COMPAT_SAVE)
			saveConfig(CONFIG_COMPAT | CONFIG_DNAS | CONFIG_VMC, 1);
	}

	return result;
}

int guiGetOpCompleted(int opid) {
	return gCompletedOps > opid;
}

int guiDeferUpdate(struct gui_update_t *op) {
	WaitSema(gSemaId);

	struct gui_update_list_t* up = (struct gui_update_list_t*) malloc(sizeof(struct gui_update_list_t));
	up->item = op;
	up->next = NULL;

	if (!gUpdateList) {
		gUpdateList = up;
		gUpdateEnd = gUpdateList;
	} else {
		gUpdateEnd->next = up;
		gUpdateEnd = up;
	}

	SignalSema(gSemaId);

	return gScheduledOps++;
}

static void guiHandleOp(struct gui_update_t* item) {
	submenu_list_t* result = NULL;

	switch (item->type) {
		case GUI_INIT_DONE:
			gInitComplete = 1;
			break;

		case GUI_OP_ADD_MENU:
			menuAppendItem(item->menu.menu);
			break;

		case GUI_OP_APPEND_MENU:
			result = submenuAppendItem(item->menu.subMenu, item->submenu.icon_id,
					item->submenu.text, item->submenu.id, item->submenu.text_id);

			if (!item->menu.menu->submenu) { // first subitem in list
				item->menu.menu->submenu = result;
				item->menu.menu->current = result;
				item->menu.menu->pagestart = result;
			} else if (item->submenu.selected) { // remember last game feat.
				item->menu.menu->current = result;
				item->menu.menu->pagestart = result;
				item->menu.menu->remindLast = 1;
			}

			break;

		case GUI_OP_SELECT_MENU:
			menuSetSelectedItem(item->menu.menu);
			screenHandler = &screenHandlers[GUI_SCREEN_MAIN];
			break;

		case GUI_OP_CLEAR_SUBMENU:
			submenuDestroy(item->menu.subMenu);
			item->menu.menu->submenu = NULL;
			item->menu.menu->current = NULL;
			item->menu.menu->pagestart = NULL;
			break;

		case GUI_OP_SORT:
			submenuSort(item->menu.subMenu);
			item->menu.menu->submenu = *item->menu.subMenu;

			if (!item->menu.menu->remindLast)
				item->menu.menu->current = item->menu.menu->submenu;

			item->menu.menu->pagestart = item->menu.menu->current;
			break;

		case GUI_OP_ADD_HINT:
			// append the hint list in the menu item
			menuAddHint(item->menu.menu, item->hint.text_id, item->hint.icon_id);
			break;

		default:
			LOG("GUI: ??? (%d)\n", item->type);
	}
}

static void guiHandleDeferredOps(void) {
	// TODO: Fit into the given time interval, skip rest of operations of over limit

	while (gUpdateList) {
		WaitSema(gSemaId);

		guiHandleOp(gUpdateList->item);

		if (gNetworkStartup < 0) {
			gNetworkStartup = 0;
			guiMsgBox(_l(_STR_NETWORK_STARTUP_ERROR), 0, NULL);
		}

		struct gui_update_list_t* td = gUpdateList;
		gUpdateList = gUpdateList->next;

		free(td);

		gCompletedOps++;

		SignalSema(gSemaId);
	}

	gUpdateEnd = NULL;
}

static int bfadeout = 0x0;
static void guiDrawBusy() {
	if (gTheme->loadingIcon) {
		GSTEXTURE* texture = thmGetTexture(LOAD0_ICON + guiFrameId % gTheme->loadingIconCount);
		if (texture && texture->Mem) {
			u64 mycolor = GS_SETREG_RGBA(0x080, 0x080, 0x080, bfadeout);
			rmDrawPixmap(texture, gTheme->loadingIcon->posX, gTheme->loadingIcon->posY, gTheme->loadingIcon->aligned, gTheme->loadingIcon->width, gTheme->loadingIcon->height, gTheme->loadingIcon->scaled, mycolor);
		}
	}
}

static int wfadeout = 0x0150;
static void guiRenderGreeting() {
	int fade = wfadeout > 0xFF ? 0xFF : wfadeout;
	u64 mycolor = GS_SETREG_RGBA(0x10, 0x10, 0x10, fade >> 1);
	rmDrawRect(0, 0, screenWidth, screenHeight, mycolor);
	/* char introtxt[255];
	 snprintf(introtxt, 255, _l(_STR_OUL_VER), USBLD_VERSION);
	 fntRenderString(screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, introtxt, GS_SETREG_RGBA(0x060, 0x060, 0x060, wfadeout));
	 */

	GSTEXTURE* logo = thmGetTexture(LOGO_PICTURE);
	if (logo) {
		mycolor = GS_SETREG_RGBA(0x080, 0x080, 0x080, fade >> 1);
		rmDrawPixmap(logo, screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, logo->Width, logo->Height, SCALING_RATIO, mycolor);
	}

	return;
}

static float mix(float a, float b, float t) {
	return (1.0 - t) * a + t * b;
}

static float fade(float t) {
	return fadetbl[(int) (t * FADE_SIZE)];
}

// The same as mix, but with 8 (2*4) values mixed at once
static void VU0MixVec(VU_VECTOR *a, VU_VECTOR *b, float mix, VU_VECTOR* res) {
	asm (
		"lqc2	vf1, 0(%0)\n" // load the first vector
		"lqc2	vf2, 0(%1)\n" // load the second vector
		"lw	$2, 0(%2)\n" // load value from ptr to reg
		"qmtc2	$2, vf3\n" // load the mix value from reg to VU
		"vaddw.x vf5, vf00, vf00\n" // vf5.x = 1
		"vsub.x vf4x, vf5x, vf3x\n" // subtract 1 - vf3,x, store the result in vf4.x
		"vmulax.xyzw ACC, vf1, vf3x\n" // multiply vf1 by vf3.x, store the result in ACC
		"vmaddx.xyzw vf1, vf2, vf4x\n" // multiply vf2 by vf4.x add ACC, store the result in vf1
		"sqc2	vf1, 0(%3)\n" // transfer the result in acc to the ee
		: : "r" (a), "r" (b), "r" (&mix), "r" (res)
	);
}

static float guiCalcPerlin(float x, float y, float z) {
	// Taken from: http://people.opera.com/patrickl/experiments/canvas/plasma/perlin-noise-classical.js
	// By Sean McCullough

	// Find unit grid cell containing point 
	int X = floor(x);
	int Y = floor(y);
	int Z = floor(z);

	// Get relative xyz coordinates of point within that cell 
	x = x - X;
	y = y - Y;
	z = z - Z;

	// Wrap the integer cells at 255 (smaller integer period can be introduced here) 
	X = X & 255;
	Y = Y & 255;
	Z = Z & 255;

	// Calculate a set of eight hashed gradient indices 
	int gi000 = pperm[X + pperm[Y + pperm[Z]]] % 12;
	int gi001 = pperm[X + pperm[Y + pperm[Z + 1]]] % 12;
	int gi010 = pperm[X + pperm[Y + 1 + pperm[Z]]] % 12;
	int gi011 = pperm[X + pperm[Y + 1 + pperm[Z + 1]]] % 12;
	int gi100 = pperm[X + 1 + pperm[Y + pperm[Z]]] % 12;
	int gi101 = pperm[X + 1 + pperm[Y + pperm[Z + 1]]] % 12;
	int gi110 = pperm[X + 1 + pperm[Y + 1 + pperm[Z]]] % 12;
	int gi111 = pperm[X + 1 + pperm[Y + 1 + pperm[Z + 1]]] % 12;

	// The gradients of each corner are now: 
	// g000 = grad3[gi000]; 
	// g001 = grad3[gi001]; 
	// g010 = grad3[gi010]; 
	// g011 = grad3[gi011]; 
	// g100 = grad3[gi100]; 
	// g101 = grad3[gi101]; 
	// g110 = grad3[gi110]; 
	// g111 = grad3[gi111]; 
	// Calculate noise contributions from each of the eight corners 
	VU_VECTOR vec;
	vec.x = x;
	vec.y = y;
	vec.z = z;
	vec.w = 1;

	VU_VECTOR a, b;

	// float n000 
	a.x = Vu0DotProduct(&pgrad3[gi000], &vec);

	vec.y -= 1;

	// float n010
	a.z = Vu0DotProduct(&pgrad3[gi010], &vec);

	vec.x -= 1;

	//float n110 
	b.z = Vu0DotProduct(&pgrad3[gi110], &vec);

	vec.y += 1;

	// float n100 
	b.x = Vu0DotProduct(&pgrad3[gi100], &vec);

	vec.z -= 1;

	// float n101
	b.y = Vu0DotProduct(&pgrad3[gi101], &vec);

	vec.y -= 1;

	// float n111 
	b.w = Vu0DotProduct(&pgrad3[gi111], &vec);

	vec.x += 1;

	// float n011 
	a.w = Vu0DotProduct(&pgrad3[gi011], &vec);

	vec.y += 1;

	// float n001 
	a.y = Vu0DotProduct(&pgrad3[gi001], &vec);

	// Compute the fade curve value for each of x, y, z 
	float u = fade(x);
	float v = fade(y);
	float w = fade(z);

	// TODO: Low priority... This could be done on VU0 (xyzw for the first 4 mixes)
	// The result in sw
	// Interpolate along x the contributions from each of the corners 
	VU_VECTOR rv;
	VU0MixVec(&b, &a, u, &rv);

	// TODO: The VU0MixVec could as well mix the results (as follows) - might improve performance...
	// Interpolate the four results along y 
	float nxy0 = mix(rv.x, rv.z, v);
	float nxy1 = mix(rv.y, rv.w, v);
	// Interpolate the two last results along z 
	float nxyz = mix(nxy0, nxy1, w);

	return nxyz;
}

static float dir = 0.02;
static float perz = -100;
static int pery = 0;
static unsigned char curbgColor[3] = { 0, 0, 0 };

static int cdirection(unsigned char a, unsigned char b) {
	if (a == b)
		return 0;
	else if (a > b)
		return -1;
	else
		return 1;
}

void guiDrawBGPlasma() {
	int x, y;

	// transition the colors
	curbgColor[0] += cdirection(curbgColor[0], gTheme->bgColor[0]);
	curbgColor[1] += cdirection(curbgColor[1], gTheme->bgColor[1]);
	curbgColor[2] += cdirection(curbgColor[2], gTheme->bgColor[2]);

	// it's PLASMA_ROWS_PER_FRAME rows a frame to stop being a resource hog
	if (pery >= PLASMA_H) {
		pery = 0;
		perz += dir;

		if (perz > 100.0f || perz < -100.0f)
			dir = -dir;
	}

	u32 *buf = gBackgroundTex.Mem + PLASMA_W * pery;
	int ymax = pery + PLASMA_ROWS_PER_FRAME;

	if (ymax > PLASMA_H)
		ymax = PLASMA_H;

	for (y = pery; y < ymax; y++) {
		for (x = 0; x < PLASMA_W; x++) {
			u32 fper = guiCalcPerlin((float) (2 * x) / PLASMA_W, (float) (2 * y) / PLASMA_H, perz) * 0x080 + 0x080;

			*buf = GS_SETREG_RGBA(
					(u32)(fper * curbgColor[0]) >> 8,
					(u32)(fper * curbgColor[1]) >> 8,
					(u32)(fper * curbgColor[2]) >> 8,
					0x080);

			++buf;
		}

	}

	pery = ymax;
	rmDrawPixmap(&gBackgroundTex, 0, 0, ALIGN_NONE, screenWidth, screenHeight, SCALING_NONE, gDefaultCol);
}

int guiDrawIconAndText(int iconId, int textId, int font, int x, int y, u64 color) {
	GSTEXTURE* iconTex = thmGetTexture(iconId);
	if (iconTex && iconTex->Mem) {
		rmDrawPixmap(iconTex, x, y, ALIGN_NONE, iconTex->Width, iconTex->Height, SCALING_RATIO, gDefaultCol);
		x += iconTex->Width + 2;
	}

	x = fntRenderString(font, x, y, ALIGN_NONE, _l(textId), color);

	return x;
}

static void guiDrawOverlays() {
	// are there any pending operations?
	int pending = ioHasPendingRequests();

	if (gInitComplete)
		wfadeout--;

	if (!pending) {
		if (bfadeout > 0x0)
			bfadeout -= 0x08;
		else
			bfadeout = 0x0;
	} else {
		if (bfadeout < 0x080)
			bfadeout += 0x20;
	}

	// is init still running?
	if (wfadeout > 0)
		guiRenderGreeting();

	if (bfadeout > 0)
		guiDrawBusy();

#ifdef __DEBUG
	// fps meter
	char fps[10];

	if (time_since_last != 0)
		snprintf(fps, 10, "%3.1f FPS", 1000.0f / (float) time_since_last);
	else
		snprintf(fps, 10, "---- FPS");

	fntRenderString(FNT_DEFAULT, screenWidth - 60, gTheme->usedHeight - 20, ALIGN_CENTER, fps, GS_SETREG_RGBA(0x060, 0x060, 0x060, 0x060));

	snprintf(fps, 10, "%3d ms", time_render);

	fntRenderString(FNT_DEFAULT, screenWidth - 60, gTheme->usedHeight - 45, ALIGN_CENTER, fps, GS_SETREG_RGBA(0x060, 0x060, 0x060, 0x060));
#endif
}

static void guiReadPads() {
	if (readPads())
		guiInactiveFrames = 0;
	else
		guiInactiveFrames++;
}

// renders the screen and handles inputs. Also handles screen transitions between numerous
// screen handlers. For now we only have left-to right screen transition
static void guiShow() {
	// is there a transmission effect going on or are
	// we in a normal rendering state?
	if (screenHandlerTarget) {
		// advance the effect

		// render the old screen, transposed
		rmSetTransposition(transIndex * transitionX, transIndex * transitionY);
		screenHandler->renderScreen();

		// render new screen transposed again
		rmSetTransposition((transIndex - transMax) * transitionX, (transIndex - transMax) * transitionY);
		screenHandlerTarget->renderScreen();

		// reset transposition to zero
		rmSetTransposition(0, 0);

		// move the transition indicator forward
		transIndex += (min(transIndex, transMax - transIndex) >> 1) + 1;

		if (transIndex > transMax) {
			transitionX = 0;
			transitionY = 0;
			screenHandler = screenHandlerTarget;
			screenHandlerTarget = NULL;
		}
	} else
		// render with the set screen handler
		screenHandler->renderScreen();
}

void guiMainLoop(void) {
	while (!gTerminate) {
		guiStartFrame();

		// Read the pad states to prepare for input processing in the screen handler
		guiReadPads();

		// handle inputs and render screen
		if (wfadeout < 0x0FF)
			guiShow();

		// Render overlaying gui thingies :)
		guiDrawOverlays();

		// handle deferred operations
		guiHandleDeferredOps();

		if (gFrameHook)
			gFrameHook();

		guiEndFrame();

		// if not transiting, handle input
		// done here so we can use renderman if needed
		if (!screenHandlerTarget && screenHandler)
			screenHandler->handleInput();
	}
}

void guiSetFrameHook(gui_callback_t cback) {
	gFrameHook = cback;
}

void guiSwitchScreen(int target, int transition) {
	if (transition == TRANSITION_LEFT) {
		transitionX = 1;
		transMax = screenWidth;
	} else if (transition == TRANSITION_RIGHT) {
		transitionX = -1;
		transMax = screenWidth;
	} else if (transition == TRANSITION_UP) {
		transitionY = 1;
		transMax = screenHeight;
	} else if (transition == TRANSITION_DOWN) {
		transitionY = -1;
		transMax = screenHeight;
	}
	transIndex = 0;

	screenHandlerTarget = &screenHandlers[target];
}

struct gui_update_t *guiOpCreate(gui_op_type_t type) {
	struct gui_update_t *op = (struct gui_update_t *) malloc(sizeof(struct gui_update_t));
	memset(op, 0, sizeof(struct gui_update_t));
	op->type = type;
	return op;
}

void guiUpdateScrollSpeed(void) {
	// sanitize the settings
	if ((gScrollSpeed < 0) || (gScrollSpeed > 2))
		gScrollSpeed = 1;

	// update the pad delays for KEY_UP and KEY_DOWN
	// default delay is 7
	// fast - 100 ms
	// medium - 300 ms
	// slow - 500 ms
	setButtonDelay(KEY_UP, 500 - gScrollSpeed * 200); // 0,1,2 -> 500, 300, 100
	setButtonDelay(KEY_DOWN, 500 - gScrollSpeed * 200);
}

void guiUpdateScreenScale(void) {
	if (gWideScreen)
		wideScreenScale = 0.75f;
	else
		wideScreenScale = 1.0f;

	// apply the scaling to renderman and font rendering
	rmSetAspectRatio(wideScreenScale, 1.0f);
	fntSetAspectRatio(wideScreenScale, 1.0f);
}

int guiMsgBox(const char* text, int addAccept, struct UIItem *ui) {
	int terminate = 0;
	while (!terminate) {
		guiStartFrame();

		readPads();

		if (getKeyOn(KEY_CIRCLE))
			terminate = 1;
		else if (getKeyOn(KEY_CROSS))
			terminate = 2;

		if (ui)
			diaRenderUI(ui, screenHandler->inMenu, NULL, 0);
		else
			guiShow();

		rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

		rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
		rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);

		fntRenderString(FNT_DEFAULT, screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, text, gTheme->textColor);
		guiDrawIconAndText(CIRCLE_ICON, _STR_O_BACK, FNT_DEFAULT, 500, 417, gTheme->selTextColor);
		if (addAccept)
			guiDrawIconAndText(CROSS_ICON, _STR_X_ACCEPT, FNT_DEFAULT, 70, 417, gTheme->selTextColor);

		guiEndFrame();
	}

	return terminate - 1;
}

void guiHandleDefferedIO(int *ptr, const unsigned char* message, int type, void *data) {
	ioPutRequest(type, data);

	while (*ptr) {
		guiStartFrame();

		readPads();

		guiShow();

		rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

		fntRenderString(FNT_DEFAULT, screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, message, gTheme->textColor);

		// so the io status icon will be rendered
		guiDrawOverlays();

		guiEndFrame();
	}
}

void guiRenderTextScreen(const unsigned char* message) {
	guiStartFrame();

	guiShow();

	rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

	fntRenderString(FNT_DEFAULT, screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, message, gTheme->textColor);

	// so the io status icon will be rendered
	guiDrawOverlays();

	guiEndFrame();
}
