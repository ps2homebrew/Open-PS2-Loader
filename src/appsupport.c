#include "include/usbld.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/appsupport.h"
#include "include/themes.h"
#include "include/system.h"
#include "include/ioman.h"

#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"

static int appForceUpdate = 1;
static int appItemCount = 0;

static config_set_t *configApps;

// forward declaration
static item_list_t appItemList;

static struct config_value_t* appGetConfigValue(int id) {
	struct config_value_t* cur = configApps->head;

	while (id--) {
		cur = cur->next;
	}

	return cur;
}

void appInit(void) {
	LOG("appInit()\n");
	appForceUpdate = 1;

	configApps = configGetByType(CONFIG_APPS);

	appItemList.enabled = 1;
}

item_list_t* appGetObject(int initOnly) {
	if (initOnly && appItemList.mode == -1)
		return NULL;
	return &appItemList;
}

static int appNeedsUpdate(void) {
	if (appForceUpdate) {
		appForceUpdate = 0;
		return 1;
	}

	return 0;
}

static int appUpdateItemList(void) {
	appItemCount = 0;
	configClear(configApps);
	configRead(configApps);

	if (configApps->head) {
		struct config_value_t* cur = configApps->head;
		while (cur) {
			cur = cur->next;
			appItemCount++;
		}
	}
	return appItemCount;
}

static int appGetItemCount(void) {
	return appItemCount;
}

static char* appGetItemName(int id) {
	struct config_value_t* cur = appGetConfigValue(id);
	return cur->key;
}

static char* appGetItemStartup(int id) {
	struct config_value_t* cur = appGetConfigValue(id);
	return cur->val;
}

#ifndef __CHILDPROOF
static void appDeleteItem(int id) {
	struct config_value_t* cur = appGetConfigValue(id);
	fileXioRemove(cur->val);
	cur->key[0] = '\0';
	configApps->modified = 1;
	configWrite(configApps);

	appForceUpdate = 1;
}

static void appRenameItem(int id, char* newName) {
	struct config_value_t* cur = appGetConfigValue(id);

	char value[255];
	strncpy(value, cur->val, 255);
	configRemoveKey(configApps, cur->key);
	configSetStr(configApps, newName, value);
	configWrite(configApps);

	appForceUpdate = 1;
}
#endif

static void appLaunchItem(int id) {
	struct config_value_t* cur = appGetConfigValue(id);
	int fd = fioOpen(cur->val, O_RDONLY);
	if (fd >= 0) {
		fioClose(fd);

		int exception = NO_EXCEPTION;
		if (strncmp(cur->val, "pfs0:", 5) == 0)
			exception = UNMOUNT_EXCEPTION;

		char filename[255];
		sprintf(filename,"%s",cur->val);
		shutdown(exception); // CAREFUL: shutdown will call appCleanUp, so configApps/cur will be freed
		sysExecElf(filename, 0, NULL);
	}
	else
		guiMsgBox(_l(_STR_ERR_FILE_INVALID), 0, NULL);
}

static int appGetArt(char* name, GSTEXTURE* resultTex, const char* type, short psm) {
	// Looking for the ELF name
	char* pos = strrchr(name, '/');
	if (!pos)
		pos = strrchr(name, ':');
	if (pos) {
		// We search on ever devices from fatest to slowest (HDD > ETH > USB)
		static item_list_t *listSupport = NULL;
		if ( (listSupport = hddGetObject(1)) ) {
			if (listSupport->itemGetArt(pos + 1, resultTex, type, psm) >= 0)
				return 0;
		}

		if ( (listSupport = ethGetObject(1)) ) {
			if (listSupport->itemGetArt(pos + 1, resultTex, type, psm) >= 0)
				return 0;
		}

		if ( (listSupport = usbGetObject(1)) )
			return listSupport->itemGetArt(pos + 1, resultTex, type, psm);
	}
	
	return -1;
}

static void appCleanUp(int exception) {
	if (appItemList.enabled) {
		LOG("appCleanUp()\n");
	}
}

static item_list_t appItemList = {
		APP_MODE, 32, 0, 0, MENU_MIN_INACTIVE_FRAMES, "Applications", _STR_APPS, &appInit, &appNeedsUpdate,	&appUpdateItemList,
#ifdef __CHILDPROOF
		&appGetItemCount, NULL, &appGetItemName, &appGetItemStartup, NULL, NULL, NULL, NULL, &appLaunchItem,
#else
		&appGetItemCount, NULL, &appGetItemName, &appGetItemStartup, &appDeleteItem, &appRenameItem, NULL, NULL, &appLaunchItem,
#endif
#ifdef VMC
		&appGetArt, &appCleanUp, NULL, APP_ICON
#else
		&appGetArt, &appCleanUp, APP_ICON
#endif
};
