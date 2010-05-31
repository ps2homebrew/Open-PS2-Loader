#include "include/usbld.h"
#include "include/lang.h"
#include "include/appsupport.h"
#include "include/themes.h"
#include "include/system.h"
#include "include/ioman.h"

#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"

static int appFirstStart;
static int appItemCount = 0;

static struct TConfigSet configApps;

// forward declaration
static item_list_t appItemList;

static struct TConfigValue* appGetConfigValue(int id) {
	struct TConfigValue* cur = configApps.head;
	
	while (id--) {
		cur = cur->next;
	}
	
	return cur;
}

void appInit(void) {
	LOG("appInit()\n");
	appFirstStart = 1;
	configApps.head = NULL;
	configApps.tail = NULL;

	appItemList.enabled = 1;
}

item_list_t* appGetObject(int initOnly) {
	if (initOnly && appItemList.mode == -1)
		return NULL;
	return &appItemList;
}

static int appNeedsUpdate(void) {
	// only update once
	if (appFirstStart) {
		appFirstStart = 0;
		return 1;
	}

	return 0;
}

static int appUpdateItemList(void) {
	char path[255];
	snprintf(path, 255, "%s/conf_apps.cfg", gBaseMCDir);
	readConfig(&configApps, path);

	if (configApps.head) {
		struct TConfigValue* cur = configApps.head;
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
	struct TConfigValue* cur = appGetConfigValue(id);
	return cur->key;
}

static char* appGetItemStartup(int id) {
	struct TConfigValue* cur = appGetConfigValue(id);
	return cur->val;
}

static void appLaunchItem(int id) {
	struct TConfigValue* cur = appGetConfigValue(id);
	sysExecElf(cur->val, 0, NULL);
}

static int appGetArt(char* name, GSTEXTURE* resultTex, const char* type, short psm) {
	// Looking for the ELF name
	char* pos = strrchr(name, '/');
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

static item_list_t appItemList = {
		APP_MODE, 0, 0, MENU_MIN_INACTIVE_FRAMES, "Applications", _STR_APPS, &appInit, &appNeedsUpdate, &appUpdateItemList, &appGetItemCount,
		NULL, &appGetItemName, &appGetItemStartup, NULL, NULL, &appLaunchItem, &appGetArt, NULL, APP_ICON
};
