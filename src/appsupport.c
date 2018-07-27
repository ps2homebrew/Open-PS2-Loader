#include "include/opl.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/appsupport.h"
#include "include/themes.h"
#include "include/system.h"
#include "include/ioman.h"

#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"
//START of OPL_DB tweaks
#include "include/supportbase.h"
//END of OPL_DB tweaks

static int appForceUpdate = 1;
static int appItemCount = 0;

static config_set_t *configApps;

// forward declaration
static item_list_t appItemList;

static struct config_value_t *appGetConfigValue(int id)
{
    struct config_value_t *cur = configApps->head;

    while (id--) {
        cur = cur->next;
    }

    return cur;
}

static char *appGetELFName(char *name)
{
    // Looking for the ELF name
    char *pos = strrchr(name, '/');
    if (!pos)
        pos = strrchr(name, ':');
    if (pos) {
        return pos + 1;
    }

    return name;
}

void appInit(void)
{
    LOG("APPSUPPORT Init\n");
    appForceUpdate = 1;
    configGetInt(configGetByType(CONFIG_OPL), "app_frames_delay", &appItemList.delay);
    configApps = configGetByType(CONFIG_APPS);
    appItemList.enabled = 1;
}

item_list_t *appGetObject(int initOnly)
{
    if (initOnly && !appItemList.enabled)
        return NULL;
    return &appItemList;
}

static int appNeedsUpdate(void)
{
    if (appForceUpdate) {
        appForceUpdate = 0;
        return 1;
    }

    return 0;
}

//START of OPL_DB tweaks
static int appUpdateItemList(void) {
	char path[256];
	static item_list_t *listSupport = NULL;
	int ret=0; //Return from configRead
	appItemCount = 0;
	
	//Clear config if already exists
	if (configApps != NULL)
		configFree(configApps);
	
	//Try MC?:/OPL/conf_apps.cfg
	snprintf(path, sizeof(path), "%s/conf_apps.cfg", gBaseMCDir);
	configApps = configAlloc(CONFIG_APPS, NULL, path);
	ret = configRead(configApps);
	
	//Try HDD
	if ( ret == 0 && (listSupport = hddGetObject(1)) ) {
		if (configApps != NULL){
			configFree(configApps);
		}
		snprintf(path, sizeof(path), "%sconf_apps.cfg", hddGetPrefix());
		configApps = configAlloc(CONFIG_APPS, NULL, path);
		ret = configRead(configApps);
	}

	//Try ETH
	if ( ret == 0 && (listSupport = ethGetObject(1)) ) {
		if (configApps != NULL){
			configFree(configApps);
		}
		snprintf(path, sizeof(path), "%sconf_apps.cfg", ethGetPrefix());
		configApps = configAlloc(CONFIG_APPS, NULL, path);
		ret = configRead(configApps);	
	}

	//Try USB
	if ( ret == 0 && (listSupport = usbGetObject(1)) ){
		if (configApps != NULL){
			configFree(configApps);
		}
		snprintf(path, sizeof(path), "%sconf_apps.cfg", usbGetPrefix());
		configApps = configAlloc(CONFIG_APPS, NULL, path);
		ret = configRead(configApps);
	}

	//Count apps
 //END of OPL_DB tweaks
   if (configApps->head) {
        struct config_value_t *cur = configApps->head;
        while (cur) {
            cur = cur->next;
            appItemCount++;
        }
    }
    return appItemCount;
}

static int appGetItemCount(void)
{
    return appItemCount;
}

static char *appGetItemName(int id)
{
    struct config_value_t *cur = appGetConfigValue(id);
    return cur->key;
}

static int appGetItemNameLength(int id)
{
    return 32;
}

static char *appGetItemStartup(int id)
{
    struct config_value_t *cur = appGetConfigValue(id);
    return appGetELFName(cur->val);
}

#ifndef __CHILDPROOF
static void appDeleteItem(int id)
{
    struct config_value_t *cur = appGetConfigValue(id);
    fileXioRemove(cur->val);
    cur->key[0] = '\0';
    configApps->modified = 1;
    configWrite(configApps);

    appForceUpdate = 1;
}

static void appRenameItem(int id, char *newName)
{
    struct config_value_t *cur = appGetConfigValue(id);

    char value[256];
    strncpy(value, cur->val, sizeof(value));
    configRemoveKey(configApps, cur->key);
    configSetStr(configApps, newName, value);
    configWrite(configApps);

    appForceUpdate = 1;
}
#endif

static void appLaunchItem(int id, config_set_t *configSet)
{
    struct config_value_t *cur = appGetConfigValue(id);
    int fd = fileXioOpen(cur->val, O_RDONLY, 0666);
    if (fd >= 0) {
        fileXioClose(fd);

        int exception = NO_EXCEPTION;
        if (strncmp(cur->val, "pfs0:", 5) == 0)
            exception = UNMOUNT_EXCEPTION;

        char filename[256];
        strncpy(filename, cur->val, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        deinit(exception); // CAREFUL: deinit will call appCleanUp, so configApps/cur will be freed
        sysExecElf(filename);
    } else
        guiMsgBox(_l(_STR_ERR_FILE_INVALID), 0, NULL);
}

//START of OPL_DB tweaks
static config_set_t* appGetConfig(int id) {
	config_set_t* config = NULL;
	static item_list_t *listSupport = NULL;
    struct config_value_t *cur = appGetConfigValue(id);
	int ret=0;
	
	//Search on HDD, SMB, USB for the CFG/GAME.ELF.CFG file.
	//HDD
	if ( (listSupport = hddGetObject(1)) ) {
		char path[256];
		#if OPL_IS_DEV_BUILD
			snprintf(path, sizeof(path), "%sCFG-DEV/%s.cfg", hddGetPrefix(), appGetELFName(cur->val));
		#else
			snprintf(path, sizeof(path), "%sCFG/%s.cfg", hddGetPrefix(), appGetELFName(cur->val));
		#endif
		config = configAlloc(1, NULL, path);
		ret = configRead(config);
	}

	//ETH
	if ( ret == 0 && (listSupport = ethGetObject(1)) ) {
		char path[256];
		if (config != NULL)
			configFree(config);
		
		#if OPL_IS_DEV_BUILD
			snprintf(path, sizeof(path), "%sCFG-DEV/%s.cfg", ethGetPrefix(), appGetELFName(cur->val));
		#else
			snprintf(path, sizeof(path), "%sCFG/%s.cfg", ethGetPrefix(),appGetELFName(cur->val));
		#endif
		config = configAlloc(1, NULL, path);
		ret = configRead(config);	
	}

	//USB
	if ( ret == 0 && (listSupport = usbGetObject(1)) ){
		char path[256];
		if (config != NULL)
			configFree(config);
		
		#if OPL_IS_DEV_BUILD
			snprintf(path, sizeof(path), "%sCFG-DEV/%s.cfg", usbGetPrefix(),  appGetELFName(cur->val));
		#else
			snprintf(path, sizeof(path), "%sCFG/%s.cfg", usbGetPrefix(), appGetELFName(cur->val));
		#endif
		config = configAlloc(1, NULL, path);
		ret = configRead(config);
	}

	if (ret == 0){ //No config found on previous devices, create one.
		if (config != NULL)
			configFree(config);
		
		config = configAlloc(1, NULL, NULL);
	}
	
//END of OPL_DB tweaks
    configSetStr(config, CONFIG_ITEM_NAME, appGetELFName(cur->val));
    configSetStr(config, CONFIG_ITEM_LONGNAME, cur->key);
    configSetStr(config, CONFIG_ITEM_STARTUP, cur->val);
//START of OPL_DB tweaks
	configSetStr(config, CONFIG_ITEM_FORMAT, "ELF");
	configSetStr(config, CONFIG_ITEM_MEDIA, "PS2");
//END of OPL_DB tweaks
    return config;
}

static int appGetImage(char *folder, int isRelative, char *value, char *suffix, GSTEXTURE *resultTex, short psm)
{
//START of OPL_DB tweaks
    //value = appGetELFName(value);
    // Search every device from fastest to slowest (HDD > ETH > USB)
//END of OPL_DB tweaks
    static item_list_t *listSupport = NULL;
    if ((listSupport = hddGetObject(1))) {
        if (listSupport->itemGetImage(folder, isRelative, value, suffix, resultTex, psm) >= 0)
            return 0;
    }

    if ((listSupport = ethGetObject(1))) {
        if (listSupport->itemGetImage(folder, isRelative, value, suffix, resultTex, psm) >= 0)
            return 0;
    }

    if ((listSupport = usbGetObject(1)))
        return listSupport->itemGetImage(folder, isRelative, value, suffix, resultTex, psm);

    return -1;
}

static void appCleanUp(int exception)
{
    if (appItemList.enabled) {
        LOG("APPSUPPORT CleanUp\n");
    }
}

static item_list_t appItemList = {
    APP_MODE, 0, MODE_FLAG_NO_COMPAT | MODE_FLAG_NO_UPDATE, MENU_MIN_INACTIVE_FRAMES, APP_MODE_UPDATE_DELAY, "Applications", _STR_APPS, &appInit, &appNeedsUpdate, &appUpdateItemList,
#ifdef __CHILDPROOF
    &appGetItemCount, NULL, &appGetItemName, &appGetItemNameLength, &appGetItemStartup, NULL, NULL, &appLaunchItem,
#else
    &appGetItemCount, NULL, &appGetItemName, &appGetItemNameLength, &appGetItemStartup, &appDeleteItem, &appRenameItem, &appLaunchItem,
#endif
#ifdef VMC
    &appGetConfig, &appGetImage, &appCleanUp, NULL, APP_ICON
#else
    &appGetConfig, &appGetImage, &appCleanUp, APP_ICON
#endif
};
