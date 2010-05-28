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
#include "include/dialogs.h"
#include "include/util.h"
#include "include/config.h"

#include <stdlib.h>
#include <libvux.h>

#define MAX_GAME_HANDLERS 8

// temp
#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

static int gScheduledOps;
static int gCompletedOps;
static int gTerminate;
static int gInitComplete;

static gui_callback_t gFrameHook;
static gui_menufill_callback_t gMenuFillHook;
static gui_menuexec_callback_t gMenuExecHook;

static s32 gSemaId;
static s32 gGUILockSemaId;
static ee_sema_t gQueueSema;

static int gInactiveFrames;
static int gFrameCounter;
// incremental game id. if a new game is encountered it is incremented
static int gCurGameID;

static GSTEXTURE gBackgroundTex;

static int screenWidth;
static float wideScreenScale;

struct gui_update_list_t {
	struct gui_update_t* item;
	struct gui_update_list_t* next;
};

struct gui_update_list_t *gUpdateList;
struct gui_update_list_t *gUpdateEnd;

struct submenu_list_t *g_settings_submenu = NULL;
struct submenu_list_t *g_games_submenu = NULL;

typedef struct {
	void (*handleInput)(void);
	void (*renderScreen)(void);
} gui_screen_handler_t;

// forward decls.

// Main screen rendering/input
static void guiMainHandleInput(void);
static void guiMainRender(void);

// Main menu rendering/input 
static void guiMenuHandleInput(void);
static void guiMenuRender(void);

// default screen handler (main screen)
static gui_screen_handler_t mainScreenHandler = {
	&guiMainHandleInput,
	&guiMainRender
};

static gui_screen_handler_t menuScreenHandler = {
	&guiMenuHandleInput,
	&guiMenuRender
};

static gui_screen_handler_t *screenHandler = &mainScreenHandler;

// screen transition handling
static gui_screen_handler_t *screenHandlerTarget = NULL;
static int transition = 0;

// "main menu submenu"
static struct submenu_list_t *mainMenu;
// active item in the main menu
static struct submenu_list_t *mainMenuCurrent;

// Helper perlin noise data
#define PLASMA_H 32
#define PLASMA_W 32
#define PLASMA_ROWS_PER_FRAME PLASMA_H
#define FADE_SIZE 256

static int pperm[512];
static float fadetbl[FADE_SIZE + 1];

static VU_VECTOR pgrad3[12] = {
	{1,1,0,1},{-1,1,0,1},{1,-1,0,1},{-1,-1,0,1}, 
	{1,0,1,1},{-1,0,1,1},{1,0,-1,1},{-1,0,-1,1}, 
	{0,1,1,1},{0,-1,1,1},{0,1,-1,1},{0,-1,-1,1}
};

static void guiInitMainMenu() {
	if (mainMenu)
		submenuDestroy(&mainMenu);

	// initialize the menu
	submenuAppendItem(&mainMenu, DISC_ICON, "Theme", 1, _STR_SETTINGS);
	submenuAppendItem(&mainMenu, DISC_ICON, "Network config", 3, _STR_IPCONFIG);
	submenuAppendItem(&mainMenu, SAVE_ICON, "Save Changes", 7, _STR_SAVE_CHANGES);
	
	// Callback to fill in other items
	if (gMenuFillHook) // if found
		gMenuFillHook(&mainMenu);

	submenuAppendItem(&mainMenu, EXIT_ICON, "Exit", 9, _STR_EXIT);
	submenuAppendItem(&mainMenu, EXIT_ICON, "Power off", 11, _STR_POWEROFF);

	mainMenuCurrent = mainMenu;
}

void guiInit(void) {
	gFrameCounter = 0;
	gFrameHook = NULL;
	gMenuFillHook = NULL;
	gMenuExecHook = NULL;
	gTerminate = 0;
	gInitComplete = 0;
	gScheduledOps = 0;
	gCompletedOps = 0;

	gUpdateList = NULL;
	gUpdateEnd = NULL;
	
	gCurGameID = 1;
	
	gInactiveFrames = 0;
	
	gQueueSema.init_count = 1;
	gQueueSema.max_count = 1;
	gQueueSema.option = 0;
	
	mainMenu = NULL;
	
	gSemaId = CreateSema(&gQueueSema);
	gGUILockSemaId = CreateSema(&gQueueSema);
	
	int screenHeight;
	rmGetScreenExtents(&screenWidth, &screenHeight);
	menuInit();
	
	guiInitMainMenu();
	
	// background texture - for perlin
	gBackgroundTex.Width = PLASMA_W;
	gBackgroundTex.Height = PLASMA_H;
	gBackgroundTex.Mem = malloc(PLASMA_W * PLASMA_H * 4);
	gBackgroundTex.PSM = GS_PSM_CT32;
	gBackgroundTex.Filter = GS_FILTER_LINEAR;
	gBackgroundTex.Vram = 0;
	
	wideScreenScale = 1.0f;
	
	// Precalculate the values for the perlin noise plasma
	int i;
	for (i = 0; i < 256; ++i) {
		pperm[i] = rand() % 256;
		pperm[i + 256] = pperm[i];
	}
	
	for (i = 0; i <= FADE_SIZE; ++i) {
		float t = (float)(i) / FADE_SIZE;
		
		fadetbl[i] = t*t*t*(t*(t*6.0-15.0)+10.0); 
	}
}

void guiEnd() {
	mainMenuCurrent = NULL;
	submenuDestroy(&mainMenu);
}

void guiLock(void) {
	WaitSema(gGUILockSemaId);
}

void guiUnlock(void) {
	SignalSema(gGUILockSemaId);
}

void guiStartFrame(void) {
	guiLock();
	rmStartFrame();
	gFrameCounter++;
}

void guiEndFrame(void) {
	guiUnlock();
	rmEndFrame();
}

static void guiExecExit() {
	if(gExitMode==0) {
		__asm__ __volatile__(
			"	li $3, 0x04;"
			"	syscall;"
			"	nop;"
		);
	} else if(gExitMode==1) {
		shutdown();
		sysExecElf("mc0:/BOOT/BOOT.ELF", 0, NULL);
	} else if(gExitMode==2) {
		shutdown();
		sysExecElf("mc0:/APPS/BOOT.ELF", 0, NULL);
	}
}

static void guiShowUIConfig() {
	// configure the enumerations
	const char* scrollSpeeds[] = {	_l(_STR_SLOW), _l(_STR_MEDIUM), _l(_STR_FAST), NULL };
	const char* exitTypes[] = { "Browser", "mc0:/BOOT/BOOT.ELF", "mc0:/APPS/BOOT.ELF", NULL };
	const char* deviceNames[] = { _l(_STR_USB_GAMES), _l(_STR_NET_GAMES), _l(_STR_HDD_GAMES), NULL };
	const char* deviceModes[] = { _l(_STR_OFF), _l(_STR_MANUAL), _l(_STR_AUTO), NULL };

	diaSetEnum(diaUIConfig, UICFG_SCROLL, scrollSpeeds);
	diaSetEnum(diaUIConfig, UICFG_THEME, (const char **)thmGetGuiList());
	diaSetEnum(diaUIConfig, UICFG_LANG, (const char **)lngGetGuiList());
	diaSetEnum(diaUIConfig, UICFG_EXITTO, exitTypes);
	diaSetEnum(diaUIConfig, UICFG_DEFDEVICE, deviceNames);
	diaSetEnum(diaUIConfig, UICFG_USBMODE, deviceModes);
	diaSetEnum(diaUIConfig, UICFG_HDDMODE, deviceModes);
	diaSetEnum(diaUIConfig, UICFG_ETHMODE, deviceModes);
	diaSetEnum(diaUIConfig, UICFG_APPMODE, deviceModes);

	// and the current values
	diaSetInt(diaUIConfig, UICFG_SCROLL, gScrollSpeed);
	diaSetInt(diaUIConfig, UICFG_THEME, thmGetGuiValue());
	diaSetInt(diaUIConfig, UICFG_LANG, lngGetGuiValue());
	diaSetColor(diaUIConfig, UICFG_BGCOL, gDefaultBgColor);
	diaSetColor(diaUIConfig, UICFG_TXTCOL, gDefaultTextColor);
	diaSetInt(diaUIConfig, UICFG_EXITTO, gExitMode);
	diaSetInt(diaUIConfig, UICFG_DEFDEVICE, gDefaultDevice);
	diaSetInt(diaUIConfig, UICFG_USBMODE, gUSBStartMode);
	diaSetInt(diaUIConfig, UICFG_HDDMODE, gHDDStartMode);
	diaSetInt(diaUIConfig, UICFG_ETHMODE, gETHStartMode);
	diaSetInt(diaUIConfig, UICFG_APPMODE, gAPPStartMode);
	diaSetInt(diaUIConfig, UICFG_AUTOSORT, gAutosort);
	diaSetInt(diaUIConfig, UICFG_DEBUG, gDisableDebug);
	diaSetInt(diaUIConfig, UICFG_COVERART, gEnableArt);
	diaSetInt(diaUIConfig, UICFG_WIDESCREEN, gWideScreen);

	int ret = diaExecuteDialog(diaUIConfig);
	if (ret) {
		int themeID = -1, langID = -1;
		diaGetInt(diaUIConfig, UICFG_SCROLL, &gScrollSpeed);
		diaGetInt(diaUIConfig, UICFG_THEME, &themeID);
		diaGetInt(diaUIConfig, UICFG_LANG, &langID);
		diaGetColor(diaUIConfig, UICFG_BGCOL, gDefaultBgColor);
		diaGetColor(diaUIConfig, UICFG_TXTCOL, gDefaultTextColor);
		diaGetInt(diaUIConfig, UICFG_EXITTO, &gExitMode);
		diaGetInt(diaUIConfig, UICFG_DEFDEVICE, &gDefaultDevice);
		diaGetInt(diaUIConfig, UICFG_USBMODE, &gUSBStartMode);
		diaGetInt(diaUIConfig, UICFG_HDDMODE, &gHDDStartMode);
		diaGetInt(diaUIConfig, UICFG_ETHMODE, &gETHStartMode);
		diaGetInt(diaUIConfig, UICFG_APPMODE, &gAPPStartMode);
		diaGetInt(diaUIConfig, UICFG_AUTOSORT, &gAutosort);
		diaGetInt(diaUIConfig, UICFG_DEBUG, &gDisableDebug);
		diaGetInt(diaUIConfig, UICFG_COVERART, &gEnableArt);
		diaGetInt(diaUIConfig, UICFG_WIDESCREEN, &gWideScreen);

		applyConfig(themeID, langID);

		if (ret == UICFG_SAVE)
			saveConfig();
	}
}

static void guiShowIPConfig() {
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

	// show dialog
	if (diaExecuteDialog(diaIPConfig)) {
		// Ok pressed, store values
		for (i = 0; i < 4; ++i) {
			diaGetInt(diaIPConfig, 2 + i, &ps2_ip[i]);
			diaGetInt(diaIPConfig, 6 + i, &ps2_netmask[i]);
			diaGetInt(diaIPConfig, 10 + i, &ps2_gateway[i]);
			diaGetInt(diaIPConfig, 14 + i, &pc_ip[i]);
		}

		diaGetInt(diaIPConfig, 18, &gPCPort);
		diaGetString(diaIPConfig, 19, gPCShareName);
		gIPConfigChanged = 1;

		applyConfig(-1, -1);
	}
}

void guiShowCompatConfig(int id, item_list_t *support) {
	int dmaMode = -1, compatMode = 0;
	char* startup = support->itemGetStartup(id);
	compatMode = support->itemGetCompatibility(id, &dmaMode);

	if(dmaMode != -1) {
		const char* dmaModes[] = { "MDMA 0", "MDMA 1", "MDMA 2", "UDMA 0", "UDMA 1", "UDMA 2", "UDMA 3", "UDMA 4", "UDMA 5", "UDMA 6", NULL };
		diaSetEnum(diaCompatConfig, COMPAT_MODE_BASE + 6, dmaModes);
		diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + 6, dmaMode);
	}
	else {
		const char* dmaModes[] = { NULL };
		diaSetEnum(diaCompatConfig, COMPAT_MODE_BASE + 6, dmaModes);
		diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + 6, 0);
	}

	diaSetLabel(diaCompatConfig, 110, support->itemGetName(id));

	int i, result;
	for (i = 0; i < COMPAT_MODE_COUNT; ++i)
		diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + i, (compatMode & (1 << i)) > 0 ? 1 : 0);

	// Find out the current game ID
	char hexid[32];
	getConfigDiscID(startup, hexid);
	diaSetString(diaCompatConfig, COMPAT_GAMEID, hexid);

	// show dialog
	do {
		result = diaExecuteDialog(diaCompatConfig);

		if (result == COMPAT_LOADFROMDISC) {
			char hexDiscID[15];
			if (sysGetDiscID(hexDiscID) >= 0)
				diaSetString(diaCompatConfig, COMPAT_GAMEID, hexDiscID);
			else
				guiMsgBox(_l(_STR_ERROR_LOADING_ID));
		}
	} while (result == COMPAT_LOADFROMDISC);

	if (result == COMPAT_REMOVE) {
		removeConfigDiscID(startup);
		support->itemSetCompatibility(id, 0, -1, 0);
		guiMsgBox(_l(_STR_REMOVED_ALL_SETTINGS));
	} else if (result > 0) { // okay pressed or other button
		compatMode = 0;
		for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
			int mdpart;
			diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + i, &mdpart);
			compatMode |= (mdpart ? 1 : 0) << i;
		}

		dmaMode = 0;
		diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + 6, &dmaMode);
		support->itemSetCompatibility(id, compatMode, dmaMode, result == COMPAT_SAVE);

		diaGetString(diaCompatConfig, COMPAT_GAMEID, hexid);
		if (hexid[0] != '\0')
			setConfigDiscID(startup, hexid);

		if (result == COMPAT_SAVE)
			saveConfig();
	}
}

int guiGetOpCompleted(int opid) {
	return gCompletedOps > opid;
}

int guiDeferUpdate(struct gui_update_t *op) {
	WaitSema(gSemaId);
	
	struct gui_update_list_t* up = (struct gui_update_list_t*)malloc(sizeof(struct gui_update_list_t));
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
	switch (item->type) {
		case GUI_INIT_DONE:
			gInitComplete = 1;
			break;
			
		case GUI_OP_ADD_MENU:
			menuAppendItem(item->menu.menu);
			break;
			
		case GUI_OP_APPEND_MENU:
			submenuAppendItem(item->menu.subMenu, 
					  item->submenu.icon_id,
					  item->submenu.text,
					  item->submenu.id,
					  item->submenu.text_id );
			
			if (!item->menu.menu->submenu) {
				item->menu.menu->submenu = *item->menu.subMenu;
				item->menu.menu->current = *item->menu.subMenu;
				item->menu.menu->pagestart = *item->menu.subMenu;
			}

			break;
			
		case GUI_OP_SELECT_MENU:
			menuSetSelectedItem(item->menu.menu);
			break;
			
		case GUI_OP_CLEAR_SUBMENU:
			submenuDestroy(item->menu.subMenu);
			item->menu.menu->submenu = NULL;
			item->menu.menu->current = NULL;
			item->menu.menu->pagestart = NULL;
			break;
			
		case GUI_OP_SORT:
			submenuSort(item->menu.subMenu);
			item->menu.menu->pagestart = NULL;
			item->menu.menu->current = *item->menu.subMenu;
			item->menu.menu->submenu = *item->menu.subMenu;
			break;
		
		case GUI_OP_PIXMAP_LOADED:
			submenuPixmapLoaded(item->pixmap.cache, item->pixmap.entry, item->pixmap.result);
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
			guiMsgBox(_l(_STR_NETWORK_STARTUP_ERROR));
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
	if (gTheme->loadingIcon.enabled) {
		GSTEXTURE* texture = thmGetTexture(LOAD0_ICON + (gFrameCounter >> 1) % gTheme->busyIconsCount);
		if (texture && texture->Mem) {
			u64 mycolor = GS_SETREG_RGBA(0x080, 0x080, 0x080, bfadeout);
			rmDrawPixmap(texture, gTheme->loadingIcon.posX, gTheme->loadingIcon.posY, gTheme->loadingIcon.aligned, gTheme->loadingIcon.width, gTheme->loadingIcon.height, mycolor);
		}
	}
}

static int wfadeout = 0x080;
static void guiRenderGreeting() {
	rmUnclip();
	
	u64 mycolor = GS_SETREG_RGBA(0x20, 0x20, 0x20, wfadeout);
	
	char introtxt[255];
	rmDrawRect(0, 0, ALIGN_NONE, DIM_INF, DIM_INF, mycolor);
	
	snprintf(introtxt, 255, _l(_STR_OUL_VER), USBLD_VERSION);
	fntRenderString(screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, introtxt, GS_SETREG_RGBA(0x060, 0x060, 0x060, wfadeout));
	return;
}

static float mix(float a, float b, float t) {
	return (1.0-t)*a + t*b; 
};

static float fade(float t) {
	return fadetbl[(int)(t*FADE_SIZE)];
};

// The same as mix, but with 8 (2*4) values mixed at once
static void VU0MixVec(VU_VECTOR *a, VU_VECTOR *b, float mix, VU_VECTOR* res) {
	asm __volatile__ (
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
	int gi000 = pperm[X+pperm[Y+pperm[Z]]] % 12; 
	int gi001 = pperm[X+pperm[Y+pperm[Z+1]]] % 12; 
	int gi010 = pperm[X+pperm[Y+1+pperm[Z]]] % 12; 
	int gi011 = pperm[X+pperm[Y+1+pperm[Z+1]]] % 12; 
	int gi100 = pperm[X+1+pperm[Y+pperm[Z]]] % 12; 
	int gi101 = pperm[X+1+pperm[Y+pperm[Z+1]]] % 12; 
	int gi110 = pperm[X+1+pperm[Y+1+pperm[Z]]] % 12; 
	int gi111 = pperm[X+1+pperm[Y+1+pperm[Z+1]]] % 12; 

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
	a.z  = Vu0DotProduct(&pgrad3[gi010], &vec); 
	
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

static float dir = 0.01;
static float perz = -100;
static int pery = 0;
static unsigned char curbgColor[3] = {0,0,0};

static int cdirection(unsigned char a, unsigned char b) {
	if (a == b)
		return 0;
	else if (a > b)
		return -1;
	else
		return 1;
}

void guiDrawBGColor() {
	u64 bgColor = GS_SETREG_RGBA(gTheme->bgColor[0], gTheme->bgColor[1], gTheme->bgColor[2], 0x080);
	rmDrawRect(0, 0, ALIGN_NONE, DIM_INF, DIM_INF, bgColor);
}

void guiDrawBGPicture() {
	GSTEXTURE* backTex = thmGetTexture(BACKGROUND_PICTURE);
	if (backTex && backTex->Mem) {
		rmDrawPixmap(backTex, 0, 0, ALIGN_NONE, DIM_INF, DIM_INF, gDefaultCol);
		rmFlush();
	}
	else
		guiDrawBGPlasma();
}

void guiDrawBGArt() {
	GSTEXTURE* someTex = menuGetCurrentArt();
	if (someTex && someTex->Mem) {
		rmDrawPixmap(someTex, 0, 0, ALIGN_NONE, DIM_INF, DIM_INF, gTheme->itemCover.color);
		rmFlush();
	}
	else
		guiDrawBGPicture();
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
			u32 fper = guiCalcPerlin((float)(2*x) / PLASMA_W, (float)(2*y) / PLASMA_H, perz) * 0x080 + 0x080;

			*buf = GS_SETREG_RGBA(
				(u32)(fper * curbgColor[0]) >> 8,
				(u32)(fper * curbgColor[1]) >> 8,
				(u32)(fper * curbgColor[2]) >> 8,
				0x080);

			++buf;
		}
	}

	pery = ymax;
	rmDrawPixmap(&gBackgroundTex, 0, 0, ALIGN_NONE, DIM_INF, DIM_INF, gDefaultCol);
}

static void guiDrawOverlays() {
	// are there any pending operations?
	rmSetTransposition(0,0);
	
	int pending = ioGetPendingRequestCount();

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
		else
			bfadeout += 0x0;
	}

	// is init still running?
	if (wfadeout > 0)
		guiRenderGreeting();
	
	if (bfadeout > 0)
		guiDrawBusy();
}

static void guiReadPads() {
	if (readPads())
		gInactiveFrames = 0;
	else
		gInactiveFrames++;
}

static void guiMainRender() {
	if (gInitComplete)
		gTheme->drawBackground();

	menuDrawStatic();
}

static void guiMainHandleInput() {
	if(getKey(KEY_LEFT)){
		menuPrevH();
	} else if(getKey(KEY_RIGHT)){
		menuNextH();
	} else if(getKey(KEY_UP)) {
		menuPrevV();
	} else if(getKey(KEY_DOWN)){
		menuNextV();
	} else if(getKey(KEY_L1)) {
		int i;
		for (i = 0; i < gTheme->displayedItems; ++i)
			menuPrevV();
	} else if(getKey(KEY_R1)){
		int i;
		for (i = 0; i < gTheme->displayedItems; ++i)
			menuNextV();
	}// home
	if (getKeyOn(KEY_L2)) {
		struct menu_item_t* cur = menuGetCurrent();
		
		cur->current = cur->submenu;
	}
	
	// end
	if (getKeyOn(KEY_R2)) {
		struct menu_item_t* cur = menuGetCurrent();
		
		if (!cur->current)
			cur->current = cur->submenu;
			
		if (cur->current)
			while (cur->current->next)
				cur->current = cur->current->next;
	}
	
	if(getKeyOn(KEY_CROSS)){
		// handle via callback in the menuitem
		menuItemExecute();
	} else if(getKeyOn(KEY_TRIANGLE)){
		// handle via callback in the menuitem
		menuItemAltExecute();
	}
	
	if(getKeyOn(KEY_START)) {
		// reinit main menu - show/hide items valid in the active context
		guiInitMainMenu();
		screenHandlerTarget = &menuScreenHandler;
	}
		
}

static void guiMenuRender() {
	gTheme->drawBackground();

	if (!mainMenu)
		return;
	
	// draw the animated menu
	if (!mainMenuCurrent)
		mainMenuCurrent = mainMenu;
	
	// iterate few items behind, few items forward
	struct submenu_list_t* it = mainMenu;

	// coordinate, etc
	// we start at center
	int w, h;
	
	int spacing = 25;
	int count = 0; int sitem = 0;
	
	// calculate the number of items
	for (; it; count++, it=it->next) {
		if (it == mainMenuCurrent)
			sitem = count;
	};
	
	it = mainMenu;
	
	const char* text = NULL;
	int x = screenWidth >> 1;
	int y = (gTheme->usedHeight >> 1) - (spacing * (count / 2));
	
	int fadeout = 0x080;
	int cp = 0; // current position
	
	for (;it; it = it->next, cp++) {
		text = submenuItemGetText(&it->item);
		fntCalcDimensions(text, &w, &h);
		
		fadeout = 0x080 - abs(sitem - cp) * 0x20;
		
		if (fadeout < 0x20)
			fadeout = 0x20;
		
		// TODO: colours from theme!
		u64 colour = GS_SETREG_RGBA(0x060, 0x060, 0x060, fadeout);
		
		if (it == mainMenuCurrent)
			colour = GS_SETREG_RGBA(0x080, 0x080, 0x040, 0x080);
		
		// render, advance
		fntRenderString(x - (w >> 1), y, ALIGN_NONE, text, colour);
		
		y += spacing;
	}
}

static void guiMenuHandleInput() {
	if (!mainMenu)
		return;
	
	if (!mainMenuCurrent)
		mainMenuCurrent = mainMenu;
	
	if (getKey(KEY_UP)) {
		if (mainMenuCurrent->prev)
			mainMenuCurrent = mainMenuCurrent->prev;
		else	// rewind to the last item
			while (mainMenuCurrent->next)
				mainMenuCurrent = mainMenuCurrent->next;
	}
	
	if (getKey(KEY_DOWN)) {
		if (mainMenuCurrent->next)
			mainMenuCurrent = mainMenuCurrent->next;
		else
			mainMenuCurrent = mainMenu;
	}
	
	if (getKeyOn(KEY_CROSS)) {
		// execute the item via looking at the id of it
		int id = mainMenuCurrent->item.id;
		
		if (id == 1) {
			guiShowUIConfig();
		} else if (id == 3) {
			// ipconfig
			guiShowIPConfig();
		} else if (id == 7) { // TODO: (Volca) As Izdubar points out, there ought be MNU_ID_something instead of this cryptic numerals...
			saveConfig();
		} else if (id == 9) { // TODO: should we call opl.shutdown first ?
			guiExecExit();
		} else if (id == 11) { // TODO: is there a rule for the id ? Also should we call opl.shutdown first ?
			sysPowerOff();
		} else {
			if (gMenuExecHook)
				gMenuExecHook(id);
		}
		
		// so the exit press wont propagate twice
		guiReadPads();
	}
	
	if(getKeyOn(KEY_START) || getKeyOn(KEY_CIRCLE)) {
		screenHandlerTarget = &mainScreenHandler;
	}
		
}

// renders the screen and handles inputs. Also handles screen transitions between
// numerous screen handlers.
// for now we only have left-to right screen transition
static void guiShow() {
	// is there a transmission effect going on or are
	// we in a normal rendering state?
	if (screenHandlerTarget) {
		// advance the effect
		
		// render the old screen, transposed
		rmSetTransposition(-transition, 0);
		screenHandler->renderScreen();
		
		// render new screen transposed again
		rmSetTransposition(screenWidth - transition, 0);
		screenHandlerTarget->renderScreen();
		
		// reset transposition to zero
		rmSetTransposition(0,0);
		
		// move the transition indicator forward
		transition += min(transition / 2, (screenWidth - transition) / 2) + 1;
		
		if (transition > screenWidth) {
			transition = 0;
			screenHandler = screenHandlerTarget;
			screenHandlerTarget = NULL;
		}
	} else {
		// reset transposition
		rmSetTransposition(0,0);
		// render with the set screen handler
		screenHandler->renderScreen();
	}
}

void guiMainLoop(void) {
	while (!gTerminate) {
		guiStartFrame();
		
		cacheNextFrame();
		menuSetInactiveFrames(gInactiveFrames);
		
		if (!screenHandler) 
			screenHandler = &mainScreenHandler;
		
		// Read the pad states to prepare for input processing in the screen handler
		guiReadPads();
		
		// handle inputs and render screen
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

void guiSetMenuFillHook(gui_menufill_callback_t cback) {
	gMenuFillHook = cback;
}

void guiSetMenuExecHook(gui_menuexec_callback_t cback) {
	gMenuExecHook = cback;
}

struct gui_update_t *guiOpCreate(gui_op_type_t type) {
	struct gui_update_t *op = (struct gui_update_t *)malloc(sizeof(struct gui_update_t));
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

void guiMsgBox(const char* text) {
	int terminate = 0;
	while(!terminate) {
		guiStartFrame();
		
		readPads();

		if(getKeyOn(KEY_CIRCLE)) 
			terminate = 1;
		
		guiShow(0);
		
		rmDrawRect(0, 0, ALIGN_NONE, DIM_INF, DIM_INF, gColDarker);

		rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
		rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);
		
		fntRenderString(screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, text, gTheme->textColor);
		fntRenderString(500, 417, ALIGN_NONE, _l(_STR_O_BACK), gTheme->selTextColor);
		
		guiEndFrame();
	}
}

void guiHandleDefferedIO(int *ptr, const unsigned char* message, int type, void *data) {
	ioPutRequest(type, data);
	
	while(*ptr) {
		guiStartFrame();
		
		readPads();
		
		guiShow(0);
		
		rmDrawRect(0, 0, ALIGN_NONE, DIM_INF, DIM_INF, gColDarker);
		
		fntRenderString(screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, message, gTheme->textColor);
		
		// so the io status icon will be rendered
		guiDrawOverlays();
		
		guiEndFrame();
	}
}

void guiRenderTextScreen(const unsigned char* message) {
	guiStartFrame();
		
	guiShow(0);
		
	rmDrawRect(0, 0, ALIGN_NONE, DIM_INF, DIM_INF, gColDarker);
		
	fntRenderString(screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, message, gTheme->textColor);
		
	// so the io status icon will be rendered
	guiDrawOverlays();
		
	guiEndFrame();
}
