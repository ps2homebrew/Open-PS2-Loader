/*
 Copyright 2010, Volca
 Licenced under Academic Free License version 3.0
 Review OpenUsbLd README & LICENSE files for further details.
 */

#include "include/opl.h"
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
#include "include/ethsupport.h"
#include "include/compatupd.h"
#include "include/pggsm.h"
#include "include/cheatman.h"

#include "include/sound.h"
#include <audsrv.h>

#ifdef PADEMU
#include <libds34bt.h>
#include <libds34usb.h>
#endif

#include <limits.h>
#include <stdlib.h>
#include <libvux.h>

// Last Played Auto Start
#include <time.h>

static int gScheduledOps;
static int gCompletedOps;
static int gTerminate;
static int gInitComplete;

static gui_callback_t gFrameHook;

static s32 gSemaId;
static s32 gGUILockSemaId;
static ee_sema_t gQueueSema;

static int screenWidth;
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
static float fps = 0.0f;

extern GSGLOBAL *gsGlobal;
#endif

struct gui_update_list_t
{
    struct gui_update_t *item;
    struct gui_update_list_t *next;
};

struct gui_update_list_t *gUpdateList;
struct gui_update_list_t *gUpdateEnd;

typedef struct
{
    void (*handleInput)(void);
    void (*renderScreen)(void);
    short inMenu;
} gui_screen_handler_t;

static gui_screen_handler_t screenHandlers[] = {{&menuHandleInputMain, &menuRenderMain, 0},
                                                {&menuHandleInputMenu, &menuRenderMenu, 1},
                                                {&menuHandleInputInfo, &menuRenderInfo, 1}};

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

static VU_VECTOR pgrad3[12] = {{1, 1, 0, 1}, {-1, 1, 0, 1}, {1, -1, 0, 1}, {-1, -1, 0, 1}, {1, 0, 1, 1}, {-1, 0, 1, 1}, {1, 0, -1, 1}, {-1, 0, -1, 1}, {0, 1, 1, 1}, {0, -1, 1, 1}, {0, 1, -1, 1}, {0, -1, -1, 1}};

void guiReloadScreenExtents()
{
    rmGetScreenExtents(&screenWidth, &screenHeight);
}

void guiInit(void)
{
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

    // Precalculate the values for the perlin noise plasma
    int i;
    for (i = 0; i < 256; ++i) {
        pperm[i] = rand() % 256;
        pperm[i + 256] = pperm[i];
    }

    for (i = 0; i <= FADE_SIZE; ++i) {
        float t = (float)(i) / FADE_SIZE;

        fadetbl[i] = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }
}

void guiEnd()
{
    if (gBackgroundTex.Mem)
        free(gBackgroundTex.Mem);

    DeleteSema(gSemaId);
    DeleteSema(gGUILockSemaId);
}

void guiLock(void)
{
    WaitSema(gGUILockSemaId);
}

void guiUnlock(void)
{
    SignalSema(gGUILockSemaId);
}

void guiStartFrame(void)
{
#ifdef __DEBUG
    u32 newtime = cpu_ticks() / CLOCKS_PER_MILISEC;
    time_since_last = newtime - curtime;
    curtime = newtime;
#endif

    guiLock();
    rmStartFrame();
    guiFrameId++;
}

void guiEndFrame(void)
{
#ifdef __DEBUG
    u32 newtime = cpu_ticks() / CLOCKS_PER_MILISEC;
    time_render = newtime - curtime;
#endif

    rmEndFrame();
    guiUnlock();
}

void guiShowAbout()
{
    char OPLVersion[40];
    char OPLBuildDetails[40];

    toggleSfx = -1;
    snprintf(OPLVersion, sizeof(OPLVersion), _l(_STR_OPL_VER), OPL_VERSION);
    diaSetLabel(diaAbout, ABOUT_TITLE, OPLVersion);

    snprintf(OPLBuildDetails, sizeof(OPLBuildDetails), ""
#ifdef __RTL
        " RTL"
#endif
        " GSM %s"
#ifdef IGS
        " IGS %s"
#endif
#ifdef PADEMU
        " PADEMU"
#endif
        //Version numbers
        , GSM_VERSION
#ifdef IGS
        , IGS_VERSION
#endif
    );
    diaSetLabel(diaAbout, ABOUT_BUILD_DETAILS, OPLBuildDetails);

    diaExecuteDialog(diaAbout, -1, 1, NULL);
    toggleSfx = 0;
}

static int guiNetCompatUpdRefresh(int modified)
{
    int result;
    unsigned int done, total;

    if ((result = oplGetUpdateGameCompatStatus(&done, &total)) == OPL_COMPAT_UPDATE_STAT_WIP) {
        diaSetInt(diaNetCompatUpdate, NETUPD_PROGRESS, (done == 0 || total == 0) ? 0 : (int)((float)done / total * 100.0f));
    }

    return result;
}

static void guiShowNetCompatUpdateResult(int result)
{
    switch (result) {
        case OPL_COMPAT_UPDATE_STAT_DONE:
            //Completed with no errors.
            guiMsgBox(_l(_STR_NET_UPDATE_DONE), 0, NULL);
            break;
        case OPL_COMPAT_UPDATE_STAT_ERROR:
            //Completed with errors.
            guiMsgBox(_l(_STR_NET_UPDATE_FAILED), 0, NULL);
            break;
        case OPL_COMPAT_UPDATE_STAT_CONN_ERROR:
            //Completed with errors.
            guiMsgBox(_l(_STR_NET_UPDATE_CONN_FAILED), 0, NULL);
            break;
        case OPL_COMPAT_UPDATE_STAT_ABORTED:
            //User-aborted.
            guiMsgBox(_l(_STR_NET_UPDATE_CANCELLED), 0, NULL);
            break;
    }
}

void guiShowNetCompatUpdate(void)
{
    int ret, UpdateAll;
    u8 done, started;
    void *UpdateFunction;

    diaSetVisible(diaNetCompatUpdate, NETUPD_BTN_START, 1);
    diaSetVisible(diaNetCompatUpdate, NETUPD_BTN_CANCEL, 0);
    diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS_LBL, 0);
    diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS_PERC_LBL, 0);
    diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS, 0);
    diaSetInt(diaNetCompatUpdate, NETUPD_OPT_UPD_ALL, 0);
    diaSetEnabled(diaNetCompatUpdate, NETUPD_OPT_UPD_ALL, 1);

    done = 0;
    started = 0;
    UpdateFunction = NULL;
    while (!done) {
        ret = diaExecuteDialog(diaNetCompatUpdate, -1, 1, UpdateFunction);
        switch (ret) {
            case NETUPD_BTN_START:
                if (guiMsgBox(_l(_STR_CONFIRMATION_SETTINGS_UPDATE), 1, NULL)) {
                    guiRenderTextScreen(_l(_STR_PLEASE_WAIT));

                    if ((ret = ethLoadInitModules()) == 0) {
                        diaSetVisible(diaNetCompatUpdate, NETUPD_BTN_START, 0);
                        diaSetVisible(diaNetCompatUpdate, NETUPD_BTN_CANCEL, 1);
                        diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS_LBL, 1);
                        diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS_PERC_LBL, 1);
                        diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS, 1);
                        diaSetEnabled(diaNetCompatUpdate, NETUPD_OPT_UPD_ALL, 0);

                        diaGetInt(diaNetCompatUpdate, NETUPD_OPT_UPD_ALL, &UpdateAll);
                        oplUpdateGameCompat(UpdateAll);
                        UpdateFunction = &guiNetCompatUpdRefresh;
                        started = 1;
                    } else {
                        ethDisplayErrorStatus();
                    }
                }
                break;
            case UIID_BTN_CANCEL: //If the user pressed the cancel button.
            case NETUPD_BTN_CANCEL:
                if (started) {
                    if (guiMsgBox(_l(_STR_CONFIRMATION_CANCEL_UPDATE), 1, NULL)) {
                        guiRenderTextScreen(_l(_STR_PLEASE_WAIT));
                        oplAbortUpdateGameCompat();
                        //The process truly ends when the UI callback gets the update from the worker thread that the process has ended.
                    }
                } else {
                    done = 1;
                    started = 0;
                }
                break;
            default:
                guiShowNetCompatUpdateResult(ret);
                done = 1;
                started = 0;
                UpdateFunction = NULL;
                break;
        }
    }
}

static void guiShowNetCompatUpdateSingle(int id, item_list_t *support, config_set_t *configSet)
{
    int ConfigSource, result;

    ConfigSource = CONFIG_SOURCE_DEFAULT;
    configGetInt(configSet, CONFIG_ITEM_CONFIGSOURCE, &ConfigSource);

    if (guiMsgBox(_l(_STR_CONFIRMATION_SETTINGS_UPDATE), 1, NULL)) {
        guiRenderTextScreen(_l(_STR_PLEASE_WAIT));

        if ((result = ethLoadInitModules()) == 0) {
            if ((result = oplUpdateGameCompatSingle(id, support, configSet)) == OPL_COMPAT_UPDATE_STAT_DONE) {
                configSetInt(configSet, CONFIG_ITEM_CONFIGSOURCE, CONFIG_SOURCE_DLOAD);
            }
            guiShowNetCompatUpdateResult(result);
        } else {
            ethDisplayErrorStatus();
        }
    }
}

static int guiUpdater(int modified)
{
    int showAutoStartLast;

    if (modified) {
        diaGetInt(diaConfig, CFG_LASTPLAYED, &showAutoStartLast);
        diaSetVisible(diaConfig, CFG_LBL_AUTOSTARTLAST, showAutoStartLast);
        diaSetVisible(diaConfig, CFG_AUTOSTARTLAST, showAutoStartLast);
    }
    return 0;
}

void guiShowConfig()
{
    int value;

    // configure the enumerations
    const char *selectButtons[] = {_l(_STR_CIRCLE), _l(_STR_CROSS), NULL};
    const char *deviceNames[] = {_l(_STR_USB_GAMES), _l(_STR_NET_GAMES), _l(_STR_HDD_GAMES), NULL};
    const char *deviceModes[] = {_l(_STR_OFF), _l(_STR_MANUAL), _l(_STR_AUTO), NULL};

    diaSetEnum(diaConfig, CFG_SELECTBUTTON, selectButtons);
    diaSetEnum(diaConfig, CFG_DEFDEVICE, deviceNames);
    diaSetEnum(diaConfig, CFG_USBMODE, deviceModes);
    diaSetEnum(diaConfig, CFG_HDDMODE, deviceModes);
    diaSetEnum(diaConfig, CFG_ETHMODE, deviceModes);
    diaSetEnum(diaConfig, CFG_APPMODE, deviceModes);

    diaSetInt(diaConfig, CFG_DEBUG, gDisableDebug);
    diaSetInt(diaConfig, CFG_PS2LOGO, gPS2Logo);
    diaSetInt(diaConfig, CFG_GAMELISTCACHE, gGameListCache);
    diaSetString(diaConfig, CFG_EXITTO, gExitPath);
    diaSetInt(diaConfig, CFG_ENWRITEOP, gEnableWrite);
    diaSetInt(diaConfig, CFG_HDDSPINDOWN, gHDDSpindown);
    diaSetInt(diaConfig, CFG_CHECKUSBFRAG, gCheckUSBFragmentation);
    diaSetString(diaConfig, CFG_USBPREFIX, gUSBPrefix);
    diaSetString(diaConfig, CFG_ETHPREFIX, gETHPrefix);
    diaSetInt(diaConfig, CFG_LASTPLAYED, gRememberLastPlayed);
    diaSetInt(diaConfig, CFG_AUTOSTARTLAST, gAutoStartLastPlayed);
    diaSetVisible(diaConfig, CFG_AUTOSTARTLAST, gRememberLastPlayed);
    diaSetVisible(diaConfig, CFG_LBL_AUTOSTARTLAST, gRememberLastPlayed);

    diaSetInt(diaConfig, CFG_SELECTBUTTON, gSelectButton == KEY_CIRCLE ? 0 : 1);
    diaSetInt(diaConfig, CFG_DEFDEVICE, gDefaultDevice);
    diaSetInt(diaConfig, CFG_USBMODE, gUSBStartMode);
    diaSetInt(diaConfig, CFG_HDDMODE, gHDDStartMode);
    diaSetInt(diaConfig, CFG_ETHMODE, gETHStartMode);
    diaSetInt(diaConfig, CFG_APPMODE, gAPPStartMode);

    int ret = diaExecuteDialog(diaConfig, -1, 1, &guiUpdater);
    if (ret) {
        diaGetInt(diaConfig, CFG_DEBUG, &gDisableDebug);
        diaGetInt(diaConfig, CFG_PS2LOGO, &gPS2Logo);
        diaGetInt(diaConfig, CFG_GAMELISTCACHE, &gGameListCache);
        diaGetString(diaConfig, CFG_EXITTO, gExitPath, sizeof(gExitPath));
        diaGetInt(diaConfig, CFG_ENWRITEOP, &gEnableWrite);
        diaGetInt(diaConfig, CFG_HDDSPINDOWN, &gHDDSpindown);
        diaGetInt(diaConfig, CFG_CHECKUSBFRAG, &gCheckUSBFragmentation);
        diaGetString(diaConfig, CFG_USBPREFIX, gUSBPrefix, sizeof(gUSBPrefix));
        diaGetString(diaConfig, CFG_ETHPREFIX, gETHPrefix, sizeof(gETHPrefix));
        diaGetInt(diaConfig, CFG_LASTPLAYED, &gRememberLastPlayed);
        diaGetInt(diaConfig, CFG_AUTOSTARTLAST, &gAutoStartLastPlayed);
        DisableCron = 1; //Disable Auto Start Last Played counter (we don't want to call it right after enable it on GUI)
        if (diaGetInt(diaConfig, CFG_SELECTBUTTON, &value))
            gSelectButton = value == 0 ? KEY_CIRCLE : KEY_CROSS;
        else
            gSelectButton = KEY_CIRCLE;
        diaGetInt(diaConfig, CFG_DEFDEVICE, &gDefaultDevice);
        diaGetInt(diaConfig, CFG_USBMODE, &gUSBStartMode);
        diaGetInt(diaConfig, CFG_HDDMODE, &gHDDStartMode);
        diaGetInt(diaConfig, CFG_ETHMODE, &gETHStartMode);
        diaGetInt(diaConfig, CFG_APPMODE, &gAPPStartMode);

        applyConfig(-1, -1);
    }
}

static int curTheme = -1;

static int guiUIUpdater(int modified)
{
    if (modified) {
        int temp, x, y;
        diaGetInt(diaUIConfig, UICFG_THEME, &temp);
        if (temp != curTheme) {
            curTheme = temp;
            if (temp == 0) {
                //Display the default theme's colours.
                diaSetItemType(diaUIConfig, UICFG_BGCOL, UI_COLOUR); // Must be correctly set before doing the diaS/GetColor !!
                diaSetItemType(diaUIConfig, UICFG_UICOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_TXTCOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_SELCOL, UI_COLOUR);
                diaSetColor(diaUIConfig, UICFG_BGCOL, gDefaultBgColor);
                diaSetColor(diaUIConfig, UICFG_UICOL, gDefaultUITextColor);
                diaSetColor(diaUIConfig, UICFG_TXTCOL, gDefaultTextColor);
                diaSetColor(diaUIConfig, UICFG_SELCOL, gDefaultSelTextColor);
            } else if (temp == thmGetGuiValue()) {
                //Display the current theme's colours.
                diaSetItemType(diaUIConfig, UICFG_BGCOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_UICOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_TXTCOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_SELCOL, UI_COLOUR);
                diaSetColor(diaUIConfig, UICFG_BGCOL, gTheme->bgColor);
                diaSetU64Color(diaUIConfig, UICFG_UICOL, gTheme->uiTextColor);
                diaSetU64Color(diaUIConfig, UICFG_TXTCOL, gTheme->textColor);
                diaSetU64Color(diaUIConfig, UICFG_SELCOL, gTheme->selTextColor);
            } else {
                //When another theme is highlighted in the list, its colours are not known. Don't show any colours.
                diaSetItemType(diaUIConfig, UICFG_BGCOL, UI_SPACER);
                diaSetItemType(diaUIConfig, UICFG_UICOL, UI_SPACER);
                diaSetItemType(diaUIConfig, UICFG_TXTCOL, UI_SPACER);
                diaSetItemType(diaUIConfig, UICFG_SELCOL, UI_SPACER);
            }

            //The user cannot adjust the current theme's colours.
            temp = !temp;
            diaSetEnabled(diaUIConfig, UICFG_BGCOL, temp);
            diaSetEnabled(diaUIConfig, UICFG_UICOL, temp);
            diaSetEnabled(diaUIConfig, UICFG_TXTCOL, temp);
            diaSetEnabled(diaUIConfig, UICFG_SELCOL, temp);
        }

        diaGetInt(diaUIConfig, UICFG_XOFF, &x);
        diaGetInt(diaUIConfig, UICFG_YOFF, &y);
        if ((x != gXOff) || (y != gYOff)) {
            gXOff = x;
            gYOff = y;
            rmSetDisplayOffset(x, y);
        }
        diaGetInt(diaUIConfig, UICFG_OVERSCAN, &temp);
        if (temp != gOverscan) {
            gOverscan = temp;
            rmSetOverscan(gOverscan);
            guiUpdateScreenScale();
        }
        diaGetInt(diaUIConfig, UICFG_WIDESCREEN, &temp);
        if (temp != gWideScreen) {
            gWideScreen = temp;
            rmSetAspectRatio((gWideScreen == 0) ? RM_ARATIO_4_3 : RM_ARATIO_16_9);
            guiUpdateScreenScale();
        }
    }

    return 0;
}

void guiShowUIConfig(void)
{
    curTheme = -1;

    // configure the enumerations
    const char *scrollSpeeds[] = {_l(_STR_SLOW), _l(_STR_MEDIUM), _l(_STR_FAST), NULL};
    const char *vmodeNames[] = {_l(_STR_AUTO)
        , "PAL 640x512i @50Hz 24bit"
        , "NTSC 640x448i @60Hz 24bit"
        , "EDTV 640x448p @60Hz 24bit"
        , "EDTV 640x512p @50Hz 24bit"
        , "VGA 640x480p @60Hz 24bit"
        , "PAL 704x576i @50Hz 24bit (HIRES)"
        , "NTSC 704x480i @60Hz 24bit (HIRES)"
        , "EDTV 704x480p @60Hz 24bit (HIRES)"
        , "EDTV 704x576p @50Hz 24bit (HIRES)"
        , "HDTV 1280x720p @60Hz 16bit (HIRES)"
        , "HDTV 1920x1080i @60Hz 16bit (HIRES)"
        , NULL};
    int previousVMode;

reselect_video_mode:
    previousVMode = gVMode;
    diaSetEnum(diaUIConfig, UICFG_SCROLL, scrollSpeeds);
    diaSetEnum(diaUIConfig, UICFG_THEME, (const char **)thmGetGuiList());
    diaSetEnum(diaUIConfig, UICFG_LANG, (const char **)lngGetGuiList());
    diaSetEnum(diaUIConfig, UICFG_VMODE, vmodeNames);
    diaSetInt(diaUIConfig, UICFG_SCROLL, gScrollSpeed);
    diaSetInt(diaUIConfig, UICFG_THEME, thmGetGuiValue());
    diaSetInt(diaUIConfig, UICFG_LANG, lngGetGuiValue());
    diaSetInt(diaUIConfig, UICFG_AUTOSORT, gAutosort);
    diaSetInt(diaUIConfig, UICFG_AUTOREFRESH, gAutoRefresh);
    diaSetInt(diaUIConfig, UICFG_INFOPAGE, gUseInfoScreen);
    diaSetInt(diaUIConfig, UICFG_COVERART, gEnableArt);
    diaSetInt(diaUIConfig, UICFG_WIDESCREEN, gWideScreen);
    diaSetInt(diaUIConfig, UICFG_VMODE, gVMode);
    diaSetInt(diaUIConfig, UICFG_XOFF, gXOff);
    diaSetInt(diaUIConfig, UICFG_YOFF, gYOff);
    diaSetInt(diaUIConfig, UICFG_OVERSCAN, gOverscan);
    guiUIUpdater(1);

    int ret = diaExecuteDialog(diaUIConfig, -1, 1, guiUIUpdater);
    if (ret) {
        int themeID = -1, langID = -1;
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
        diaGetInt(diaUIConfig, UICFG_VMODE, &gVMode);
        diaGetInt(diaUIConfig, UICFG_XOFF, &gXOff);
        diaGetInt(diaUIConfig, UICFG_YOFF, &gYOff);
        diaGetInt(diaUIConfig, UICFG_OVERSCAN, &gOverscan);

        applyConfig(themeID, langID);
        //wait 70ms for confirm sound to finish playing before clearing buffer
        guiDelay(0070);
        sfxInit(0);
    }

    if (previousVMode != gVMode) {
        if (guiConfirmVideoMode() == 0) {
            //Restore previous video mode, without changing the theme & language settings.
            gVMode = previousVMode;
            applyConfig(-1, -1);
            goto reselect_video_mode;
        }
    }
}

static void guiSetGSMSettingsState(void)
{
    int isGSMEnabled;

    diaGetInt(diaGSConfig, GSMCFG_ENABLEGSM, &isGSMEnabled);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMVMODE, isGSMEnabled);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMXOFFSET, isGSMEnabled);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMYOFFSET, isGSMEnabled);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMFIELDFIX, isGSMEnabled);
}

static int guiGSMUpdater(int modified)
{
    if (modified) {
        guiSetGSMSettingsState();
    }

    return 0;
}

static void guiShowGSConfig(void)
{
    // configure the enumerations
    const char *gsmvmodeNames[] = {
        "NTSC",
        "NTSC Non Interlaced",
        "PAL",
        "PAL Non Interlaced",
        "PAL @60Hz",
        "PAL @60Hz Non Interlaced",
        "PS1 NTSC (HDTV 480p @60Hz)",
        "PS1 PAL (HDTV 576p @50Hz)",
        "HDTV 480p @60Hz",
        "HDTV 576p @50Hz",
        "HDTV 720p @60Hz",
        "HDTV 1080i @60Hz",
        "HDTV 1080i @60Hz Non Interlaced",
        "VGA 640x480p @60Hz",
        "VGA 640x480p @72Hz",
        "VGA 640x480p @75Hz",
        "VGA 640x480p @85Hz",
        "VGA 640x960i @60Hz",
        "VGA 800x600p @56Hz",
        "VGA 800x600p @60Hz",
        "VGA 800x600p @72Hz",
        "VGA 800x600p @75Hz",
        "VGA 800x600p @85Hz",
        "VGA 1024x768p @60Hz",
        "VGA 1024x768p @70Hz",
        "VGA 1024x768p @75Hz",
        "VGA 1024x768p @85Hz",
        "VGA 1280x1024p @60Hz",
        "VGA 1280x1024p @75Hz",
        NULL};

    diaSetEnum(diaGSConfig, GSMCFG_GSMVMODE, gsmvmodeNames);
    diaExecuteDialog(diaGSConfig, -1, 1, &guiGSMUpdater);
}

static void guiSetCheatSettingsState(void)
{
    int isCheatEnabled;

    diaGetInt(diaCheatConfig, CHTCFG_ENABLECHEAT, &isCheatEnabled);
    diaSetEnabled(diaCheatConfig, CHTCFG_CHEATMODE, isCheatEnabled);
}

static int guiCheatUpdater(int modified)
{
    if (modified) {
        guiSetCheatSettingsState();
    }

    return 0;
}

void guiShowCheatConfig(void)
{
    // configure the enumerations
    const char *cheatmodeNames[] = {_l(_STR_CHEATMODEAUTO), _l(_STR_CHEATMODESELECT), NULL};

    diaSetEnum(diaCheatConfig, CHTCFG_CHEATMODE, cheatmodeNames);

    diaExecuteDialog(diaCheatConfig, -1, 1, &guiCheatUpdater);
}

#ifdef PADEMU

//from https://www.bluetooth.com/specifications/assigned-numbers/host-controller-interface
static char *bt_ver_str[] = {
    "1.0b",
    "1.1",
    "1.2",
    "2.0 + EDR",
    "2.1 + EDR",
    "3.0 + HS",
    "4.0",
    "4.1",
    "4.2",
    "5.0",
};

static const char *PadEmuPorts_enums[][5] = {
    {"1P", "2P", NULL, NULL, NULL},
    {"1A", "1B", "1C", "1D", NULL},
    {"2A", "2B", "2C", "2D", NULL},
};

static u8 ds3_mac[6];
static u8 dg_mac[6];
static char ds3_str[18];
static char dg_str[18];
static char vid_str[4];
static char pid_str[4];
static char rev_str[4];
static char hci_str[26];
static char lmp_str[26];
static char man_str[4];
static int ds3macset = 0;
static int dgmacset = 0;
static int dg_discon = 0;
static int ver_set = 0, feat_set = 0;
static int PadEmuSettings = 0;

static char *bdaddr_to_str(u8 *bdaddr, char *addstr)
{
    int i;

    memset(addstr, 0, sizeof(addstr));

    for (i = 0; i < 6; i++) {
        sprintf(addstr, "%s%02X", addstr, bdaddr[i]);

        if (i < 5)
            sprintf(addstr, "%s:", addstr);
    }

    return addstr;
}

static char *hex_to_str(u8 *str, u16 hex)
{
    sprintf(str, "%04X", hex);

    return str;
}

static char *ver_to_str(u8 *str, u8 ma, u16 mi)
{
    if (ma > 9)
        ma = 0;

    sprintf(str, "%X.%04X    BT %s", ma, mi, bt_ver_str[ma]);

    return str;
}

static int guiPadEmuUpdater(int modified)
{
    int PadEmuEnable, PadEmuMode, PadPort, PadEmuVib, PadEmuPort, PadEmuMtap, PadEmuMtapPort, PadEmuWorkaround;
    static int oldPadPort;

    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_ENABLE, &PadEmuEnable);
    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_MODE, &PadEmuMode);
    diaGetInt(diaPadEmuConfig, PADCFG_PADPORT, &PadPort);
    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_PORT, &PadEmuPort);
    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_VIB, &PadEmuVib);

    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_MTAP, &PadEmuMtap);
    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_MTAP_PORT, &PadEmuMtapPort);
    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_WORKAROUND, &PadEmuWorkaround);

    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_MTAP, PadEmuEnable);
    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_MTAP_PORT, PadEmuMtap);

    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_MODE, PadEmuEnable);

    diaSetEnabled(diaPadEmuConfig, PADCFG_PADPORT, PadEmuEnable);
    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_VIB, PadEmuPort & PadEmuEnable);

    diaSetVisible(diaPadEmuConfig, PADCFG_USBDG_MAC, PadEmuMode & PadEmuEnable);
    diaSetVisible(diaPadEmuConfig, PADCFG_PAD_MAC, PadEmuMode & PadEmuEnable);
    diaSetVisible(diaPadEmuConfig, PADCFG_PAIR, PadEmuMode & PadEmuEnable);

    diaSetVisible(diaPadEmuConfig, PADCFG_USBDG_MAC_STR, PadEmuMode & PadEmuEnable);
    diaSetVisible(diaPadEmuConfig, PADCFG_PAD_MAC_STR, PadEmuMode & PadEmuEnable);
    diaSetVisible(diaPadEmuConfig, PADCFG_PAIR_STR, PadEmuMode & PadEmuEnable);

    diaSetVisible(diaPadEmuConfig, PADCFG_BTINFO, PadEmuMode & PadEmuEnable);
    diaSetVisible(diaPadEmuConfig, PADCFG_PADEMU_WORKAROUND, PadEmuMode & PadEmuEnable);
    diaSetVisible(diaPadEmuConfig, PADCFG_PADEMU_WORKAROUND_STR, PadEmuMode & PadEmuEnable);

    if (modified) {
        if (PadEmuMtap) {
            diaSetEnum(diaPadEmuConfig, PADCFG_PADPORT, PadEmuPorts_enums[PadEmuMtapPort]);
            diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_PORT, (PadPort == 0) & PadEmuEnable);
            PadEmuSettings |= 0x00000E00;
        } else {
            diaSetEnum(diaPadEmuConfig, PADCFG_PADPORT, PadEmuPorts_enums[0]);
            diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_PORT, PadEmuEnable);
            PadEmuSettings &= 0xFFFF03FF;
            if (PadPort > 1) {
                PadPort = 0;
                diaSetInt(diaPadEmuConfig, PADCFG_PADPORT, PadPort);
            }
        }

        if (PadPort != oldPadPort) {
            diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_PORT, (PadEmuSettings >> (8 + PadPort)) & 1);
            diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_VIB, (PadEmuSettings >> (16 + PadPort)) & 1);

            oldPadPort = PadPort;
        }
    }

    PadEmuSettings |= PadEmuMode | (PadEmuPort << (8 + PadPort)) | (PadEmuVib << (16 + PadPort)) | (PadEmuMtap << 24) | ((PadEmuMtapPort - 1) << 25) | (PadEmuWorkaround << 26);
    PadEmuSettings &= (~(!PadEmuMode) & ~(!PadEmuPort << (8 + PadPort)) & ~(!PadEmuVib << (16 + PadPort)) & ~(!PadEmuMtap << 24) & ~(!(PadEmuMtapPort - 1) << 25) & ~(!PadEmuWorkaround << 26));

    if (PadEmuMode) {
        if (ds34bt_get_status(0) & DS34BT_STATE_USB_CONFIGURED) {
            if (dg_discon) {
                dgmacset = 0;
                dg_discon = 0;
            }
            if (!dgmacset) {
                if (ds34bt_get_bdaddr(dg_mac)) {
                    dgmacset = 1;
                    diaSetLabel(diaPadEmuConfig, PADCFG_USBDG_MAC, bdaddr_to_str(dg_mac, dg_str));
                } else {
                    dgmacset = 0;
                }
            }
        } else {
            dg_discon = 1;
        }

        if (!dgmacset) {
            diaSetLabel(diaPadEmuConfig, PADCFG_USBDG_MAC, _l(_STR_NOT_CONNECTED));
        }

        if (ds34usb_get_status(0) & DS34USB_STATE_RUNNING) {
            if (!ds3macset) {
                if (ds34usb_get_bdaddr(0, ds3_mac)) {
                    ds3macset = 1;
                    diaSetLabel(diaPadEmuConfig, PADCFG_PAD_MAC, bdaddr_to_str(ds3_mac, ds3_str));
                } else {
                    ds3macset = 0;
                }
            }
        } else {
            diaSetLabel(diaPadEmuConfig, PADCFG_PAD_MAC, _l(_STR_NOT_CONNECTED));
            ds3macset = 0;
        }
    }

    return 0;
}

static int guiPadEmuInfoUpdater(int modified)
{
    hci_information_t info;
    u8 feat[8];
    int i, j;
    u8 data;
    int supported;

    if (ds34bt_get_status(0) & DS34BT_STATE_USB_CONFIGURED) {
        if (!ver_set) {
            if (ds34bt_get_version(&info)) {
                ver_set = 1;
                diaSetLabel(diaPadEmuInfo, PADCFG_VID, hex_to_str(vid_str, info.vid));
                diaSetLabel(diaPadEmuInfo, PADCFG_PID, hex_to_str(pid_str, info.pid));
                diaSetLabel(diaPadEmuInfo, PADCFG_REV, hex_to_str(rev_str, info.rev));
                diaSetLabel(diaPadEmuInfo, PADCFG_HCIVER, ver_to_str(hci_str, info.hci_ver, info.hci_rev));
                diaSetLabel(diaPadEmuInfo, PADCFG_LMPVER, ver_to_str(lmp_str, info.lmp_ver, info.lmp_subver));
                diaSetLabel(diaPadEmuInfo, PADCFG_MANID, hex_to_str(man_str, info.mf_name));
            } else {
                ver_set = 0;
            }
        }

        if (!feat_set) {
            if (ds34bt_get_features(feat)) {
                feat_set = 1;
                supported = 0;
                for (i = 0, j = 0; i < 64; i++) {
                    data = (feat[j] >> (i - j * 8)) & 1;
                    diaSetLabel(diaPadEmuInfo, PADCFG_FEAT_START + i, _l(_STR_NO - data));
                    j = (i + 1) / 8;
                    if (i == 25 || i == 26 || i == 39) {
                        if (data)
                            supported++;
                    }
                }
                if (supported == 3)
                    diaSetLabel(diaPadEmuInfo, PADCFG_BT_SUPPORTED, _l(_STR_BT_SUPPORTED));
                else
                    diaSetLabel(diaPadEmuInfo, PADCFG_BT_SUPPORTED, _l(_STR_BT_NOTSUPPORTED));
            } else {
                feat_set = 0;
            }
        }
    } else {
        ver_set = 0;
        feat_set = 0;
    }

    return 0;
}

static void guiShowPadEmuConfig(void)
{
    const char *PadEmuModes[] = {_l(_STR_DS34USB_MODE), _l(_STR_DS34BT_MODE), NULL};
    int PadEmuMtap, PadEmuMtapPort, PadEmuEnable, i;

    diaSetEnum(diaPadEmuConfig, PADCFG_PADEMU_MODE, PadEmuModes);

    PadEmuMtap = (PadEmuSettings >> 24) & 1;
    PadEmuMtapPort = ((PadEmuSettings >> 25) & 1) + 1;

    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_MODE, PadEmuSettings & 0xFF);

    diaSetInt(diaPadEmuConfig, PADCFG_PADPORT, 0);

    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_PORT, (PadEmuSettings >> 8) & 1);
    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_VIB, (PadEmuSettings >> 16) & 1);

    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_MTAP, PadEmuMtap);
    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_MTAP_PORT, PadEmuMtapPort);
    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_WORKAROUND, ((PadEmuSettings >> 26) & 1));

    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_ENABLE, &PadEmuEnable);
    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_PORT, PadEmuEnable);

    if (PadEmuMtap) {
        diaSetEnum(diaPadEmuConfig, PADCFG_PADPORT, PadEmuPorts_enums[PadEmuMtapPort]);
        PadEmuSettings |= 0x00000E00;
    } else {
        diaSetEnum(diaPadEmuConfig, PADCFG_PADPORT, PadEmuPorts_enums[0]);
        PadEmuSettings &= 0xFFFF03FF;
    }

    int result = -1;

    while (result != 0) {
        result = diaExecuteDialog(diaPadEmuConfig, result, 1, &guiPadEmuUpdater);

        if (result == PADCFG_PAIR) {
            if (ds3macset && dgmacset) {
                if (ds34usb_get_status(0) & DS34USB_STATE_RUNNING) {
                    if (ds34usb_set_bdaddr(0, dg_mac))
                        ds3macset = 0;
                }
            }
        }

        if (result == PADCFG_BTINFO) {
            for (i = PADCFG_FEAT_START; i < PADCFG_FEAT_END + 1; i++)
                diaSetLabel(diaPadEmuInfo, i, _l(_STR_NO));

            diaSetLabel(diaPadEmuInfo, PADCFG_VID, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_PID, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_REV, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_HCIVER, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_LMPVER, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_MANID, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_BT_SUPPORTED, _l(_STR_NOT_CONNECTED));
            ver_set = 0;
            feat_set = 0;
            diaExecuteDialog(diaPadEmuInfo, -1, 1, &guiPadEmuInfoUpdater);
        }

        if (result == UIID_BTN_OK)
            break;
    }
}
#endif

static int netConfigUpdater(int modified)
{
    int showAdvancedOptions, isNetBIOS, isDHCPEnabled, i;

    if (modified) {
        diaGetInt(diaNetConfig, NETCFG_SHOW_ADVANCED_OPTS, &showAdvancedOptions);

        diaGetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_TYPE, &isDHCPEnabled);
        diaGetInt(diaNetConfig, NETCFG_SHARE_ADDR_TYPE, &isNetBIOS);
        diaSetVisible(diaNetConfig, NETCFG_SHARE_NB_ADDR, isNetBIOS);

        for (i = 0; i < 4; i++) {
            diaSetVisible(diaNetConfig, NETCFG_SHARE_IP_ADDR_0 + i, !isNetBIOS);

            diaSetEnabled(diaNetConfig, NETCFG_PS2_IP_ADDR_0 + i, !isDHCPEnabled);
            diaSetEnabled(diaNetConfig, NETCFG_PS2_NETMASK_0 + i, !isDHCPEnabled);
            diaSetEnabled(diaNetConfig, NETCFG_PS2_GATEWAY_0 + i, !isDHCPEnabled);
            diaSetEnabled(diaNetConfig, NETCFG_PS2_DNS_0 + i, !isDHCPEnabled);
        }

        for (i = 0; i < 3; i++)
            diaSetVisible(diaNetConfig, NETCFG_SHARE_IP_ADDR_DOT_0 + i, !isNetBIOS);

        diaSetEnabled(diaNetConfig, NETCFG_SHARE_PORT, showAdvancedOptions);
        diaSetEnabled(diaNetConfig, NETCFG_ETHOPMODE, showAdvancedOptions);
    }

    return 0;
}

void guiShowNetConfig(void)
{
    size_t i;
    const char *ethOpModes[] = {_l(_STR_AUTO), _l(_STR_ETH_100MFDX), _l(_STR_ETH_100MHDX), _l(_STR_ETH_10MFDX), _l(_STR_ETH_10MHDX), NULL};
    const char *addrConfModes[] = {_l(_STR_ADDR_TYPE_IP), _l(_STR_ADDR_TYPE_NETBIOS), NULL};
    const char *ipAddrConfModes[] = {_l(_STR_IP_ADDRESS_TYPE_STATIC), _l(_STR_IP_ADDRESS_TYPE_DHCP), NULL};
    diaSetEnum(diaNetConfig, NETCFG_PS2_IP_ADDR_TYPE, ipAddrConfModes);
    diaSetEnum(diaNetConfig, NETCFG_SHARE_ADDR_TYPE, addrConfModes);
    diaSetEnum(diaNetConfig, NETCFG_ETHOPMODE, ethOpModes);

    // upload current values
    diaSetInt(diaNetConfig, NETCFG_SHOW_ADVANCED_OPTS, 0);
    diaSetEnabled(diaNetConfig, NETCFG_ETHOPMODE, 0);
    diaSetEnabled(diaNetConfig, NETCFG_SHARE_PORT, 0);

    diaSetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_TYPE, ps2_ip_use_dhcp);
    diaSetInt(diaNetConfig, NETCFG_SHARE_ADDR_TYPE, gPCShareAddressIsNetBIOS);
    diaSetVisible(diaNetConfig, NETCFG_SHARE_NB_ADDR, gPCShareAddressIsNetBIOS);
    diaSetInt(diaNetConfig, NETCFG_SHARE_NB_ADDR, gPCShareAddressIsNetBIOS);
    diaSetString(diaNetConfig, NETCFG_SHARE_NB_ADDR, gPCShareNBAddress);

    for (i = 0; i < 4; ++i) {
        diaSetEnabled(diaNetConfig, NETCFG_PS2_IP_ADDR_0 + i, !ps2_ip_use_dhcp);
        diaSetEnabled(diaNetConfig, NETCFG_PS2_NETMASK_0 + i, !ps2_ip_use_dhcp);
        diaSetEnabled(diaNetConfig, NETCFG_PS2_GATEWAY_0 + i, !ps2_ip_use_dhcp);
        diaSetEnabled(diaNetConfig, NETCFG_PS2_DNS_0 + i, !ps2_ip_use_dhcp);

        diaSetVisible(diaNetConfig, NETCFG_SHARE_IP_ADDR_0 + i, !gPCShareAddressIsNetBIOS);
        diaSetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_0 + i, ps2_ip[i]);
        diaSetInt(diaNetConfig, NETCFG_PS2_NETMASK_0 + i, ps2_netmask[i]);
        diaSetInt(diaNetConfig, NETCFG_PS2_GATEWAY_0 + i, ps2_gateway[i]);
        diaSetInt(diaNetConfig, NETCFG_PS2_DNS_0 + i, ps2_dns[i]);
        diaSetInt(diaNetConfig, NETCFG_SHARE_IP_ADDR_0 + i, pc_ip[i]);
    }

    for (i = 0; i < 3; ++i)
        diaSetVisible(diaNetConfig, NETCFG_SHARE_IP_ADDR_DOT_0 + i, !gPCShareAddressIsNetBIOS);

    diaSetInt(diaNetConfig, NETCFG_SHARE_PORT, gPCPort);
    diaSetString(diaNetConfig, NETCFG_SHARE_NAME, gPCShareName);
    diaSetString(diaNetConfig, NETCFG_SHARE_USERNAME, gPCUserName);
    diaSetString(diaNetConfig, NETCFG_SHARE_PASSWORD, gPCPassword);
    diaSetInt(diaNetConfig, NETCFG_ETHOPMODE, gETHOpMode);

    //Update the spacer item between the OK and reconnect buttons (See dialogs.c).
    if (gNetworkStartup == 0) {
        diaSetLabel(diaNetConfig, NETCFG_OK, _l(_STR_OK));
        diaSetVisible(diaNetConfig, NETCFG_RECONNECT, 1);
    } else if (gNetworkStartup >= ERROR_ETH_SMB_CONN) {
        diaSetLabel(diaNetConfig, NETCFG_OK, _l(_STR_RECONNECT));
        diaSetVisible(diaNetConfig, NETCFG_RECONNECT, 0);
    } else {
        diaSetLabel(diaNetConfig, NETCFG_OK, _l(_STR_OK));
        diaSetVisible(diaNetConfig, NETCFG_RECONNECT, 0);
    }

    int result = diaExecuteDialog(diaNetConfig, -1, 1, &netConfigUpdater);
    if (result) {
        // Store values
        diaGetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_TYPE, &ps2_ip_use_dhcp);
        diaGetInt(diaNetConfig, NETCFG_SHARE_ADDR_TYPE, &gPCShareAddressIsNetBIOS);
        diaGetString(diaNetConfig, NETCFG_SHARE_NB_ADDR, gPCShareNBAddress, sizeof(gPCShareNBAddress));

        for (i = 0; i < 4; ++i) {
            diaGetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_0 + i, &ps2_ip[i]);
            diaGetInt(diaNetConfig, NETCFG_PS2_NETMASK_0 + i, &ps2_netmask[i]);
            diaGetInt(diaNetConfig, NETCFG_PS2_GATEWAY_0 + i, &ps2_gateway[i]);
            diaGetInt(diaNetConfig, NETCFG_PS2_DNS_0 + i, &ps2_dns[i]);
            diaGetInt(diaNetConfig, NETCFG_SHARE_IP_ADDR_0 + i, &pc_ip[i]);
        }
        diaGetInt(diaNetConfig, NETCFG_ETHOPMODE, &gETHOpMode);

        diaGetInt(diaNetConfig, NETCFG_SHARE_PORT, &gPCPort);
        diaGetString(diaNetConfig, NETCFG_SHARE_NAME, gPCShareName, sizeof(gPCShareName));
        diaGetString(diaNetConfig, NETCFG_SHARE_USERNAME, gPCUserName, sizeof(gPCUserName));
        diaGetString(diaNetConfig, NETCFG_SHARE_PASSWORD, gPCPassword, sizeof(gPCPassword));

        if (result == NETCFG_RECONNECT && gNetworkStartup < ERROR_ETH_SMB_CONN)
            gNetworkStartup = ERROR_ETH_SMB_LOGON;

        applyConfig(-1, -1);
    }
}

void guiShowParentalLockConfig(void)
{
    int result;
    char password[CONFIG_KEY_VALUE_LEN];
    config_set_t *configOPL = configGetByType(CONFIG_OPL);

    // Set current values
    configGetStrCopy(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD, password, CONFIG_KEY_VALUE_LEN); //This will return the current password, or a blank string if it is not set.
    diaSetString(diaParentalLockConfig, CFG_PARENLOCK_PASSWORD, password);

    result = diaExecuteDialog(diaParentalLockConfig, -1, 1, NULL);
    if (result) {
        diaGetString(diaParentalLockConfig, CFG_PARENLOCK_PASSWORD, password, CONFIG_KEY_VALUE_LEN);

        if (strlen(password) > 0) {
            if (strncmp(OPL_PARENTAL_LOCK_MASTER_PASS, password, CONFIG_KEY_VALUE_LEN) != 0) {
                // Store password
                configSetStr(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD, password);
            } else {
                // Password not acceptable (i.e. master password entered).
                guiMsgBox(_l(_STR_PARENLOCK_INVALID_PASSWORD), 0, NULL);
            }
        } else {
            configRemoveKey(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD);

            guiMsgBox(_l(_STR_PARENLOCK_DISABLE_WARNING), 0, diaParentalLockConfig);
        }

        menuSetParentalLockCheckState(1);
    }
}

void guiShowAudioConfig(void)
{
	int ret;

    diaSetInt(diaAudioConfig, CFG_SFX, gEnableSFX);
    diaSetInt(diaAudioConfig, CFG_BOOT_SND, gEnableBootSND);
    diaSetInt(diaAudioConfig, CFG_SFX_VOLUME, gSFXVolume);
    diaSetInt(diaAudioConfig, CFG_BOOT_SND_VOLUME, gBootSndVolume);

    ret = diaExecuteDialog(diaAudioConfig, -1, 1, &guiUpdater);
    if (ret) {
        diaGetInt(diaAudioConfig, CFG_SFX, &gEnableSFX);
        diaGetInt(diaAudioConfig, CFG_BOOT_SND, &gEnableBootSND);
        diaGetInt(diaAudioConfig, CFG_SFX_VOLUME, &gSFXVolume);
        diaGetInt(diaAudioConfig, CFG_BOOT_SND_VOLUME, &gBootSndVolume);
        applyConfig(-1, -1);
        sfxVolume();
    }
}

int guiShowKeyboard(char *value, int maxLength)
{
    char tmp[maxLength];
    strncpy(tmp, value, maxLength);

    int result = diaShowKeyb(tmp, maxLength, 0, NULL);
    if (result) {
        strncpy(value, tmp, maxLength);
        value[maxLength - 1] = '\0';
    }

    return result;
}

typedef struct
{                   // size = 76
    int VMC_status; // 0=available, 1=busy
    int VMC_error;
    int VMC_progress;
    char VMC_msg[64];
} statusVMCparam_t;

#define OPERATION_CREATE 0
#define OPERATION_CREATING 1
#define OPERATION_ABORTING 2
#define OPERATION_ENDING 3
#define OPERATION_END 4

static short vmc_refresh;
static int vmc_operation;
static statusVMCparam_t vmc_status;

int guiVmcNameHandler(char *text, int maxLen)
{
    int result = diaShowKeyb(text, maxLen, 0, NULL);

    if (result)
        vmc_refresh = 1;

    return result;
}

static int guiRefreshVMCConfig(item_list_t *support, char *name)
{
    int size = support->itemCheckVMC(name, 0);

    if (size != -1) {
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

        diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_MODIFY));
        diaSetVisible(diaVMC, VMC_BUTTON_DELETE, 1);
        if (gEnableWrite) {
            diaSetEnabled(diaVMC, VMC_SIZE, 1);
            diaSetEnabled(diaVMC, VMC_BUTTON_CREATE, 1);
            diaSetEnabled(diaVMC, VMC_BUTTON_DELETE, 1);
        } else {
            diaSetEnabled(diaVMC, VMC_SIZE, 0);
            diaSetEnabled(diaVMC, VMC_BUTTON_CREATE, 0);
            diaSetEnabled(diaVMC, VMC_BUTTON_DELETE, 0);
        }
    } else {
        diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_CREATE));
        diaSetLabel(diaVMC, VMC_STATUS, _l(_STR_VMC_FILE_NEW));

        diaSetInt(diaVMC, VMC_SIZE, 0);
        diaSetEnabled(diaVMC, VMC_SIZE, 1);
        diaSetEnabled(diaVMC, VMC_BUTTON_CREATE, 1);
        diaSetVisible(diaVMC, VMC_BUTTON_DELETE, 0);
    }

    return size;
}

static int guiVMCUpdater(int modified)
{
    if (vmc_refresh) {
        vmc_refresh = 0;
        return VMC_REFRESH;
    }

    if ((vmc_operation == OPERATION_CREATING) || (vmc_operation == OPERATION_ABORTING)) {
        int result = fileXioDevctl("genvmc:", 0xC0DE0003, NULL, 0, (void *)&vmc_status, sizeof(vmc_status));
        if (result == 0) {
            diaSetLabel(diaVMC, VMC_STATUS, vmc_status.VMC_msg);
            diaSetInt(diaVMC, VMC_PROGRESS, vmc_status.VMC_progress);

            if (vmc_status.VMC_error != 0)
                LOG("GUI VMCUpdater: %d\n", vmc_status.VMC_error);

            if (vmc_status.VMC_status == 0x00) {
                diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_OK));
                vmc_operation = OPERATION_ENDING;
                return VMC_BUTTON_CREATE;
            }
        } else
            LOG("GUI Status result: %d\n", result);
    }

    return 0;
}

static int guiShowVMCConfig(int id, item_list_t *support, char *VMCName, int slot, int validate)
{
    int result = validate ? VMC_BUTTON_CREATE : 0;
    char vmc[32];

    if (strlen(VMCName))
        strncpy(vmc, VMCName, sizeof(vmc));
    else {
        if (validate)
            return 1; // nothing to validate if no user input

        char *startup = support->itemGetStartup(id);
        snprintf(vmc, sizeof(vmc), "%s_%d", startup, slot);
    }

    vmc_refresh = 0;
    vmc_operation = OPERATION_CREATE;
    diaSetEnabled(diaVMC, VMC_NAME, 1);
    diaSetEnabled(diaVMC, VMC_SIZE, 1);
    diaSetInt(diaVMC, VMC_PROGRESS, 0);

    const char *VMCSizes[] = {"8 Mb", "16 Mb", "32 Mb", "64 Mb", NULL};
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
                } else
                    break;
            } else if (vmc_operation == OPERATION_ENDING) {
                if (validate)
                    break; // directly close VMC config dialog

                vmc_operation = OPERATION_END;
            } else if (vmc_operation == OPERATION_END) { // User closed creation dialog of VMC
                break;
            } else if (vmc_operation == OPERATION_CREATING) { // User canceled creation of VMC
                fileXioDevctl("genvmc:", 0xC0DE0002, NULL, 0, NULL, 0);
                vmc_operation = OPERATION_ABORTING;
            }
        } else if (result == VMC_BUTTON_DELETE) {
            if (guiMsgBox(_l(_STR_DELETE_WARNING), 1, diaVMC)) {
                support->itemCheckVMC(vmc, -1);
                diaSetString(diaVMC, VMC_NAME, "");
                break;
            }
        } else if (result == VMC_REFRESH) { // User changed the VMC name
            diaGetString(diaVMC, VMC_NAME, vmc, sizeof(vmc));
            size = guiRefreshVMCConfig(support, vmc);
        }

        result = diaExecuteDialog(diaVMC, result, 1, &guiVMCUpdater);

        if ((result == 0) && (vmc_operation == OPERATION_CREATE))
            break;
    } while (1);

    return result;
}

int guiAltStartupNameHandler(char *text, int maxLen)
{
    int i;

    int result = diaShowKeyb(text, maxLen, 0, NULL);
    if (result) {
        for (i = 0; text[i]; i++) {
            if (text[i] > 96 && text[i] < 123)
                text[i] -= 32;
        }
    }

    return result;
}

int guiShowCompatConfig(int id, item_list_t *support, config_set_t *configSet)
{
    int ConfigSource, result;

    int dmaMode = 7; // defaulting to UDMA 4
    if (support->flags & MODE_FLAG_COMPAT_DMA) {
        configGetInt(configSet, CONFIG_ITEM_DMA, &dmaMode);
        const char *dmaModes[] = {"MDMA 0", "MDMA 1", "MDMA 2", "UDMA 0", "UDMA 1", "UDMA 2", "UDMA 3", "UDMA 4", NULL};
        diaSetEnum(diaCompatConfig, COMPAT_DMA, dmaModes);
        diaSetInt(diaCompatConfig, COMPAT_DMA, dmaMode);
    } else {
        const char *dmaModes[] = {NULL};
        diaSetEnum(diaCompatConfig, COMPAT_DMA, dmaModes);
        diaSetInt(diaCompatConfig, COMPAT_DMA, 0);
    }

    diaSetLabel(diaCompatConfig, COMPAT_GAME, support->itemGetName(id));

    ConfigSource = CONFIG_SOURCE_DEFAULT;
    if (configGetInt(configSet, CONFIG_ITEM_CONFIGSOURCE, &ConfigSource)) {
        diaSetLabel(diaCompatConfig, COMPAT_STATUS, ConfigSource == CONFIG_SOURCE_USER ? _l(_STR_CUSTOMIZED_SETTINGS) : _l(_STR_DOWNLOADED_DEFAULTS));
        diaSetVisible(diaCompatConfig, COMPAT_STATUS, 1);
    } else
        diaSetVisible(diaCompatConfig, COMPAT_STATUS, 0);

    int compatMode = 0;
    configGetInt(configSet, CONFIG_ITEM_COMPAT, &compatMode);
    int i;
    result = -1;
    for (i = 0; i < COMPAT_MODE_COUNT; ++i)
        diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + i, (compatMode & (1 << i)) > 0 ? 1 : 0);

// Begin Per-Game GSM Integration --Bat--
    int EnableGSM = 0;
    configGetInt(configSet, CONFIG_ITEM_ENABLEGSM, &EnableGSM);
    diaSetInt(diaGSConfig, GSMCFG_ENABLEGSM, EnableGSM);

    int GSMVMode = 0;
    configGetInt(configSet, CONFIG_ITEM_GSMVMODE, &GSMVMode);
    diaSetInt(diaGSConfig, GSMCFG_GSMVMODE, GSMVMode);

    int GSMXOffset = 0;
    configGetInt(configSet, CONFIG_ITEM_GSMXOFFSET, &GSMXOffset);
    diaSetInt(diaGSConfig, GSMCFG_GSMXOFFSET, GSMXOffset);

    int GSMYOffset = 0;
    configGetInt(configSet, CONFIG_ITEM_GSMYOFFSET, &GSMYOffset);
    diaSetInt(diaGSConfig, GSMCFG_GSMYOFFSET, GSMYOffset);

    int GSMFIELDFix = 0;
    configGetInt(configSet, CONFIG_ITEM_GSMFIELDFIX, &GSMFIELDFix);
    diaSetInt(diaGSConfig, GSMCFG_GSMFIELDFIX, GSMFIELDFix);

    guiSetGSMSettingsState();

// Begin of Per-Game CHEAT Integration --Bat--
    int EnableCheat = 0;
    configGetInt(configSet, CONFIG_ITEM_ENABLECHEAT, &EnableCheat);
    diaSetInt(diaCheatConfig, CHTCFG_ENABLECHEAT, EnableCheat);

    int CheatMode = 0;
    configGetInt(configSet, CONFIG_ITEM_CHEATMODE, &CheatMode);
    diaSetInt(diaCheatConfig, CHTCFG_CHEATMODE, CheatMode);

    guiSetCheatSettingsState();

#ifdef PADEMU
    int EnablePadEmu = 0;
    configGetInt(configSet, CONFIG_ITEM_ENABLEPADEMU, &EnablePadEmu);
    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_ENABLE, EnablePadEmu);

    PadEmuSettings = 0;

    configGetInt(configSet, CONFIG_ITEM_PADEMUSETTINGS, &PadEmuSettings);
#endif

    // Find out the current game ID
    char hexid[32];
    configGetStrCopy(configSet, CONFIG_ITEM_DNAS, hexid, sizeof(hexid));
    diaSetString(diaCompatConfig, COMPAT_GAMEID, hexid);

    char altStartup[32];
    configGetStrCopy(configSet, CONFIG_ITEM_ALTSTARTUP, altStartup, sizeof(altStartup));
    diaSetString(diaCompatConfig, COMPAT_ALTSTARTUP, altStartup);

    // VMC
    char vmc1[32];
    configGetVMC(configSet, vmc1, sizeof(vmc1), 0);
    diaSetLabel(diaCompatConfig, COMPAT_VMC1_DEFINE, vmc1);

    char vmc2[32]; // required as diaSetLabel use pointer to value
    configGetVMC(configSet, vmc2, sizeof(vmc2), 1);
    diaSetLabel(diaCompatConfig, COMPAT_VMC2_DEFINE, vmc2);

    // show dialog
    do {
        // VMC
        if (strlen(vmc1))
            diaSetLabel(diaCompatConfig, COMPAT_VMC1_ACTION, _l(_STR_RESET));
        else
            diaSetLabel(diaCompatConfig, COMPAT_VMC1_ACTION, _l(_STR_USE_GENERIC));
        if (strlen(vmc2))
            diaSetLabel(diaCompatConfig, COMPAT_VMC2_ACTION, _l(_STR_RESET));
        else
            diaSetLabel(diaCompatConfig, COMPAT_VMC2_ACTION, _l(_STR_USE_GENERIC));

        result = diaExecuteDialog(diaCompatConfig, result, 1, NULL);

        if (result == COMPAT_GSMCONFIG) {
            guiShowGSConfig();
        }
#ifdef PADEMU
        if (result == COMPAT_PADEMUCONFIG) {
            guiShowPadEmuConfig();
        }
#endif
        if (result == COMPAT_CHEATCONFIG) {
            guiShowCheatConfig();
        }

        if (result == COMPAT_LOADFROMDISC) {
            char hexDiscID[15];
            if (sysGetDiscID(hexDiscID) >= 0)
                diaSetString(diaCompatConfig, COMPAT_GAMEID, hexDiscID);
            else
                guiMsgBox(_l(_STR_ERROR_LOADING_ID), 0, NULL);
        }
        //VMC
        else if (result == COMPAT_VMC1_DEFINE) {
            if (menuCheckParentalLock() == 0) {
                if (guiShowVMCConfig(id, support, vmc1, 0, 0))
                    diaGetString(diaVMC, VMC_NAME, vmc1, sizeof(vmc1));
            }
        } else if (result == COMPAT_VMC2_DEFINE) {
            if (menuCheckParentalLock() == 0) {
                if (guiShowVMCConfig(id, support, vmc2, 1, 0))
                    diaGetString(diaVMC, VMC_NAME, vmc2, sizeof(vmc2));
            }
        } else if (result == COMPAT_VMC1_ACTION) {
            if (menuCheckParentalLock() == 0) {
                if (strlen(vmc1))
                    vmc1[0] = '\0';
                 else
                     snprintf(vmc1, sizeof(vmc1), "generic_%d", 0);
            }
        } else if (result == COMPAT_VMC2_ACTION) {
            if (menuCheckParentalLock() == 0) {
                if (strlen(vmc2))
                    vmc2[0] = '\0';
                 else
                    snprintf(vmc2, sizeof(vmc2), "generic_%d", 1);
            }
        }
    } while (result >= COMPAT_NOEXIT);

    if (result == COMPAT_REMOVE) {
        if (menuCheckParentalLock() == 0) {
            configRemoveKey(configSet, CONFIG_ITEM_CONFIGSOURCE);
            configRemoveKey(configSet, CONFIG_ITEM_DMA);
            configRemoveKey(configSet, CONFIG_ITEM_COMPAT);
            configRemoveKey(configSet, CONFIG_ITEM_DNAS);
            configRemoveKey(configSet, CONFIG_ITEM_ALTSTARTUP);

            //GSM
            configRemoveKey(configSet, CONFIG_ITEM_ENABLEGSM);
            configRemoveKey(configSet, CONFIG_ITEM_GSMVMODE);
            configRemoveKey(configSet, CONFIG_ITEM_GSMXOFFSET);
            configRemoveKey(configSet, CONFIG_ITEM_GSMYOFFSET);
            configRemoveKey(configSet, CONFIG_ITEM_GSMFIELDFIX);

            //Cheats
            configRemoveKey(configSet, CONFIG_ITEM_ENABLECHEAT);
            configRemoveKey(configSet, CONFIG_ITEM_CHEATMODE);

#ifdef PADEMU
            //PADEMU
            configRemoveKey(configSet, CONFIG_ITEM_ENABLEPADEMU);
            configRemoveKey(configSet, CONFIG_ITEM_PADEMUSETTINGS);
#endif

            //VMC
            configRemoveVMC(configSet, 0);
            configRemoveVMC(configSet, 1);

            menuSaveConfig();
        }
    } else if (result > 0) { // test button pressed or save button
        compatMode = 0;
        for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
            int mdpart;
            diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + i, &mdpart);
            compatMode |= (mdpart ? 1 : 0) << i;
        }

        if (support->flags & MODE_FLAG_COMPAT_DMA) {
            diaGetInt(diaCompatConfig, COMPAT_DMA, &dmaMode);
            if (dmaMode != 7)
                configSetInt(configSet, CONFIG_ITEM_DMA, dmaMode);
            else
                configRemoveKey(configSet, CONFIG_ITEM_DMA);
        }

        if (compatMode != 0)
            configSetInt(configSet, CONFIG_ITEM_COMPAT, compatMode);
        else
            configRemoveKey(configSet, CONFIG_ITEM_COMPAT);

        //GSM
        diaGetInt(diaGSConfig, GSMCFG_ENABLEGSM, &EnableGSM);
        if (EnableGSM != 0)
            configSetInt(configSet, CONFIG_ITEM_ENABLEGSM, EnableGSM);
        else
            configRemoveKey(configSet, CONFIG_ITEM_ENABLEGSM);

        diaGetInt(diaGSConfig, GSMCFG_GSMVMODE, &GSMVMode);
        if (GSMVMode != 0)
            configSetInt(configSet, CONFIG_ITEM_GSMVMODE, GSMVMode);
        else
            configRemoveKey(configSet, CONFIG_ITEM_GSMVMODE);

        diaGetInt(diaGSConfig, GSMCFG_GSMXOFFSET, &GSMXOffset);
        if (GSMXOffset != 0)
            configSetInt(configSet, CONFIG_ITEM_GSMXOFFSET, GSMXOffset);
        else
            configRemoveKey(configSet, CONFIG_ITEM_GSMXOFFSET);

        diaGetInt(diaGSConfig, GSMCFG_GSMYOFFSET, &GSMYOffset);
        if (GSMYOffset != 0)
            configSetInt(configSet, CONFIG_ITEM_GSMYOFFSET, GSMYOffset);
        else
            configRemoveKey(configSet, CONFIG_ITEM_GSMYOFFSET);

        diaGetInt(diaGSConfig, GSMCFG_GSMFIELDFIX, &GSMFIELDFix);
        if (GSMFIELDFix != 0)
            configSetInt(configSet, CONFIG_ITEM_GSMFIELDFIX, GSMFIELDFix);
        else
            configRemoveKey(configSet, CONFIG_ITEM_GSMFIELDFIX);

        //Cheats
        diaGetInt(diaCheatConfig, CHTCFG_ENABLECHEAT, &EnableCheat);
        if (EnableCheat != 0)
            configSetInt(configSet, CONFIG_ITEM_ENABLECHEAT, EnableCheat);
        else
            configRemoveKey(configSet, CONFIG_ITEM_ENABLECHEAT);

        diaGetInt(diaCheatConfig, CHTCFG_CHEATMODE, &CheatMode);
        if (CheatMode != 0)
            configSetInt(configSet, CONFIG_ITEM_CHEATMODE, CheatMode);
        else
            configRemoveKey(configSet, CONFIG_ITEM_CHEATMODE);

#ifdef PADEMU
        //PADEMU
        diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_ENABLE, &EnablePadEmu);

        if (EnablePadEmu != 0)
            configSetInt(configSet, CONFIG_ITEM_ENABLEPADEMU, EnablePadEmu);
        else
            configRemoveKey(configSet, CONFIG_ITEM_ENABLEPADEMU);

        if (PadEmuSettings != 0)
            configSetInt(configSet, CONFIG_ITEM_PADEMUSETTINGS, PadEmuSettings);
        else
            configRemoveKey(configSet, CONFIG_ITEM_PADEMUSETTINGS);
#endif

        diaGetString(diaCompatConfig, COMPAT_GAMEID, hexid, sizeof(hexid));
        if (hexid[0] != '\0')
            configSetStr(configSet, CONFIG_ITEM_DNAS, hexid);

        diaGetString(diaCompatConfig, COMPAT_ALTSTARTUP, altStartup, sizeof(altStartup));
        if (altStartup[0] != '\0')
            configSetStr(configSet, CONFIG_ITEM_ALTSTARTUP, altStartup);
        else
            configRemoveKey(configSet, CONFIG_ITEM_ALTSTARTUP);

        //VMC
        configSetVMC(configSet, vmc1, 0);
        configSetVMC(configSet, vmc2, 1);
        guiShowVMCConfig(id, support, vmc1, 0, 1);
        guiShowVMCConfig(id, support, vmc2, 1, 1);

        switch (result) {
            case COMPAT_SAVE:
                configSetInt(configSet, CONFIG_ITEM_CONFIGSOURCE, CONFIG_SOURCE_USER);
                menuSaveConfig();
                break;
            case COMPAT_DL_DEFAULTS:
                guiShowNetCompatUpdateSingle(id, support, configSet);
                break;
        }
    }

    return result;
}

int guiGetOpCompleted(int opid)
{
    return gCompletedOps > opid;
}

int guiDeferUpdate(struct gui_update_t *op)
{
    WaitSema(gSemaId);

    struct gui_update_list_t *up = (struct gui_update_list_t *)malloc(sizeof(struct gui_update_list_t));
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

static void guiHandleOp(struct gui_update_t *item)
{
    submenu_list_t *result = NULL;

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
            } else if (item->submenu.selected) { // remember last played game feature
                item->menu.menu->current = result;
                item->menu.menu->pagestart = result;
                item->menu.menu->remindLast = 1;

                // Last Played Auto Start
                if ((gAutoStartLastPlayed) && !(KeyPressedOnce))
                    DisableCron = 0; //Release Auto Start Last Played counter
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

static void guiHandleDeferredOps(void)
{

    WaitSema(gSemaId);
    while (gUpdateList) {

        guiHandleOp(gUpdateList->item);

        struct gui_update_list_t *td = gUpdateList;
        gUpdateList = gUpdateList->next;

        free(td);

        gCompletedOps++;
    }
    SignalSema(gSemaId);

    gUpdateEnd = NULL;
}

void guiExecDeferredOps(void)
{
    //Clears deferred operations list by executing them.
    guiHandleDeferredOps();
}

static int bfadeout = 0x0;
static void guiDrawBusy()
{
    if (gTheme->loadingIcon) {
        GSTEXTURE *texture = thmGetTexture(LOAD0_ICON + (guiFrameId >> 1) % gTheme->loadingIconCount);
        if (texture && texture->Mem) {
            u64 mycolor = GS_SETREG_RGBA(0x80, 0x80, 0x80, bfadeout);
            rmDrawPixmap(texture, gTheme->loadingIcon->posX, gTheme->loadingIcon->posY, gTheme->loadingIcon->aligned, gTheme->loadingIcon->width, gTheme->loadingIcon->height, gTheme->loadingIcon->scaled, mycolor);
        }
    }
}

static int wfadeout = 0x80;
static void guiRenderGreeting()
{
    u64 mycolor = GS_SETREG_RGBA(0x00, 0x00, 0x00, wfadeout);
    rmDrawRect(0, 0, screenWidth, screenHeight, mycolor);

    GSTEXTURE *logo = thmGetTexture(LOGO_PICTURE);
    if (logo) {
        mycolor = GS_SETREG_RGBA(0x80, 0x80, 0x80, wfadeout);
        rmDrawPixmap(logo, screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, logo->Width, logo->Height, SCALING_RATIO, mycolor);
    }
}

static float mix(float a, float b, float t)
{
    return (1.0 - t) * a + t * b;
}

static float fade(float t)
{
    return fadetbl[(int)(t * FADE_SIZE)];
}

// The same as mix, but with 8 (2*4) values mixed at once
static void VU0MixVec(VU_VECTOR *a, VU_VECTOR *b, float mix, VU_VECTOR *res)
{
    asm(
        "lqc2	vf1, 0(%0)\n"          // load the first vector
        "lqc2	vf2, 0(%1)\n"          // load the second vector
        "lw	$2, 0(%2)\n"               // load value from ptr to reg
        "qmtc2	$2, vf3\n"             // load the mix value from reg to VU
        "vaddw.x vf5, vf00, vf00\n"    // vf5.x = 1
        "vsub.x vf4x, vf5x, vf3x\n"    // subtract 1 - vf3,x, store the result in vf4.x
        "vmulax.xyzw ACC, vf1, vf3x\n" // multiply vf1 by vf3.x, store the result in ACC
        "vmaddx.xyzw vf1, vf2, vf4x\n" // multiply vf2 by vf4.x add ACC, store the result in vf1
        "sqc2	vf1, 0(%3)\n"          // transfer the result in acc to the ee
        :
        : "r"(a), "r"(b), "r"(&mix), "r"(res));
}

static float guiCalcPerlin(float x, float y, float z)
{
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
static unsigned char curbgColor[3] = {0, 0, 0};

static int cdirection(unsigned char a, unsigned char b)
{
    if (a == b)
        return 0;
    else if (a > b)
        return -1;
    else
        return 1;
}

void guiDrawBGPlasma()
{
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
            u32 fper = guiCalcPerlin((float)(2 * x) / PLASMA_W, (float)(2 * y) / PLASMA_H, perz) * 0x80 + 0x80;

            *buf = GS_SETREG_RGBA(
                (u32)(fper * curbgColor[0]) >> 8,
                (u32)(fper * curbgColor[1]) >> 8,
                (u32)(fper * curbgColor[2]) >> 8,
                0x80);

            ++buf;
        }
    }

    pery = ymax;
    rmInvalidateTexture(&gBackgroundTex);
    rmDrawPixmap(&gBackgroundTex, 0, 0, ALIGN_NONE, screenWidth, screenHeight, SCALING_NONE, gDefaultCol);
}

int guiDrawIconAndText(int iconId, int textId, int font, int x, int y, u64 color)
{
    GSTEXTURE *iconTex = thmGetTexture(iconId);
    int w = (iconTex->Width * 20) / iconTex->Height;
    int h = 20;

    if (iconTex && iconTex->Mem) {
        y += h >> 1;
        rmDrawPixmap(iconTex, x, y, ALIGN_VCENTER, w, h, SCALING_RATIO, gDefaultCol);
        x += rmWideScale(w) + 2;
    }
    else {
        // HACK: font is aligned to VCENTER, the default height icon height is 20
        y += 10;
    }

    x = fntRenderString(font, x, y, ALIGN_VCENTER, 0, 0, _l(textId), color);

    return x;
}

static void guiDrawOverlays()
{
    // are there any pending operations?
    int pending = ioHasPendingRequests();

    if (!pending) {
        if (bfadeout > 0x0)
            bfadeout -= 0x08;
        else
            bfadeout = 0x0;
    } else {
        if (bfadeout < 0x80)
            bfadeout += 0x08;
    }

    if (bfadeout > 0 && !toggleSfx)
        guiDrawBusy();

#ifdef __DEBUG
    char text[20];
    int x = screenWidth - 120;
    int y = 15;
    int yadd = 15;

    snprintf(text, sizeof(text), "VRAM:");
    fntRenderString(gTheme->fonts[0], x, y, ALIGN_LEFT, 0, 0, text, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
    y += yadd;

    snprintf(text, sizeof(text), "%dKiB FIXED", gsGlobal->CurrentPointer / 1024);
    fntRenderString(gTheme->fonts[0], x, y, ALIGN_LEFT, 0, 0, text, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
    y += yadd;

    snprintf(text, sizeof(text), "%dKiB TEXMAN", ((4*1024*1024) - gsGlobal->CurrentPointer) / 1024);
    fntRenderString(gTheme->fonts[0], x, y, ALIGN_LEFT, 0, 0, text, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
    y += yadd;
    y += yadd; // Empty line

    if (time_since_last != 0) {
        fps = fps * 0.99 + 10.0f / (float)time_since_last;

        snprintf(text, sizeof(text), "%.1f FPS", fps);
        fntRenderString(gTheme->fonts[0], x, y, ALIGN_LEFT, 0, 0, text, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
        y += yadd;
    }
#endif

    // Last Played Auto Start
    if ((!pending) && (wfadeout <= 0) && (DisableCron == 0)) {
        if (CronStart == 0) {
            CronStart = clock() / CLOCKS_PER_SEC;
        } else {
            char strAutoStartInNSecs[21];
            double CronCurrent;

            CronCurrent = clock() / CLOCKS_PER_SEC;
            RemainSecs = gAutoStartLastPlayed - (CronCurrent - CronStart);
            snprintf(strAutoStartInNSecs, sizeof(strAutoStartInNSecs), _l(_STR_AUTO_START_IN_N_SECS), RemainSecs);
            fntRenderString(gTheme->fonts[0], screenWidth / 2, screenHeight / 2, ALIGN_CENTER, 0, 0, strAutoStartInNSecs, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
        }
    }

    // BLURT output
//    if (!gDisableDebug)
//        fntRenderString(gTheme->fonts[0], 0, screenHeight - 24, ALIGN_NONE, 0, 0, blurttext, GS_SETREG_RGBA(255, 255, 0, 128));
}

static void guiReadPads()
{
    if (readPads())
        guiInactiveFrames = 0;
    else
        guiInactiveFrames++;
}

// renders the screen and handles inputs. Also handles screen transitions between numerous
// screen handlers. For now we only have left-to right screen transition
static void guiShow()
{
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

void guiDelay(int milliSeconds)
{
    clock_t time_end = time_end = clock() + milliSeconds * CLOCKS_PER_SEC / 1000;
    while (clock() < time_end) {}

    toggleSfx = 0;
}

void guiIntroLoop(void)
{
    int endIntro = 0;

     if (gEnableSFX && gEnableBootSND)
         toggleSfx = -1;

    while (!endIntro) {
        guiStartFrame();

        if (wfadeout < 0x80 && toggleSfx)
            guiDelay(gFadeDelay);

        if (wfadeout < 0x80 && !toggleSfx)
            guiShow();

        if (gInitComplete)
            wfadeout -= 2;

        if (wfadeout > 0)
            guiRenderGreeting();
        else
            endIntro = 1;

        guiDrawOverlays();

        guiHandleDeferredOps();

        guiEndFrame();

        if (!screenHandlerTarget && screenHandler)
            screenHandler->handleInput();
    }
}

void guiMainLoop(void)
{
    while (!gTerminate) {
        guiStartFrame();

        // Read the pad states to prepare for input processing in the screen handler
        guiReadPads();

        // handle inputs and render screen
        guiShow();

        // Render overlaying gui thingies :)
        guiDrawOverlays();

        // handle deferred operations
        guiHandleDeferredOps();

        guiEndFrame();

        // if not transiting, handle input
        // done here so we can use renderman if needed
        if (!screenHandlerTarget && screenHandler)
            screenHandler->handleInput();

        if (gFrameHook)
            gFrameHook();
    }
}

void guiSetFrameHook(gui_callback_t cback)
{
    gFrameHook = cback;
}

void guiSwitchScreen(int target, int transition)
{
    sfxPlay(SFX_TRANSITION);
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

struct gui_update_t *guiOpCreate(gui_op_type_t type)
{
    struct gui_update_t *op = (struct gui_update_t *)malloc(sizeof(struct gui_update_t));
    memset(op, 0, sizeof(struct gui_update_t));
    op->type = type;
    return op;
}

void guiUpdateScrollSpeed(void)
{
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

void guiUpdateScreenScale(void)
{
    fntUpdateAspectRatio();
}

int guiMsgBox(const char *text, int addAccept, struct UIItem *ui)
{
    int terminate = 0;

    sfxPlay(SFX_MESSAGE);

    while (!terminate) {
        guiStartFrame();

        readPads();

        if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE))
            terminate = 1;
        else if (getKeyOn(gSelectButton))
            terminate = 2;

        if (ui)
            diaRenderUI(ui, screenHandler->inMenu, NULL, 0);
        else
            guiShow();

        rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

        rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
        rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);

        fntRenderString(gTheme->fonts[0], screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, 0, 0, text, gTheme->textColor);
        guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, _STR_BACK, gTheme->fonts[0], 500, 417, gTheme->selTextColor);
        if (addAccept)
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON, _STR_ACCEPT, gTheme->fonts[0], 70, 417, gTheme->selTextColor);

        guiEndFrame();
    }

    if (terminate == 1) {
        sfxPlay(SFX_CANCEL);
    }
    if (terminate == 2) {
        sfxPlay(SFX_CONFIRM);
    }

    return terminate - 1;
}

void guiHandleDeferedIO(int *ptr, const unsigned char *message, int type, void *data)
{
    ioPutRequest(type, data);

    while (*ptr)
        guiRenderTextScreen(message);
}

void guiRenderTextScreen(const unsigned char *message)
{
    guiStartFrame();

    guiShow();

    rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

    fntRenderString(gTheme->fonts[0], screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, 0, 0, message, gTheme->textColor);

    guiDrawOverlays();

    guiEndFrame();
}

void guiWarning(const char *text, int count)
{
    guiStartFrame();

    guiShow();

    rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

    rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
    rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);

    fntRenderString(gTheme->fonts[0], screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, screenWidth, screenHeight, text, gTheme->textColor);

    guiEndFrame();

    delay(count);
}

int guiConfirmVideoMode(void)
{
    clock_t timeStart, timeNow, timeElasped;
    int terminate = 0;

    sfxPlay(SFX_MESSAGE);

    timeStart = clock() / (CLOCKS_PER_SEC / 1000);
    while (!terminate) {
        guiStartFrame();

        readPads();

        if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE))
            terminate = 1;
        else if (getKeyOn(gSelectButton))
            terminate = 2;

        //If the user fails to respond within the timeout period, deem it as a cancel operation.
        timeNow = clock() / (CLOCKS_PER_SEC / 1000);
        timeElasped = (timeNow < timeStart) ? UINT_MAX - timeStart + timeNow + 1 : timeNow - timeStart;
        if (timeElasped >= OPL_VMODE_CHANGE_CONFIRMATION_TIMEOUT_MS)
            terminate = 1;

        guiShow();

        rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

        rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
        rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);

        fntRenderString(gTheme->fonts[0], screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, 0, 0, _l(_STR_CFM_VMODE_CHG), gTheme->textColor);
        guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, _STR_BACK, gTheme->fonts[0], 500, 417, gTheme->selTextColor);
        guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON, _STR_ACCEPT, gTheme->fonts[0], 70, 417, gTheme->selTextColor);

        guiEndFrame();
    }

    if (terminate == 1) {
        sfxPlay(SFX_CANCEL);
    }
    if (terminate == 2) {
        sfxPlay(SFX_CONFIRM);
    }

    return terminate - 1;
}

