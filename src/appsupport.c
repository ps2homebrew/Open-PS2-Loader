#include "include/usbld.h"
#include "include/lang.h"
#include "include/appsupport.h"
#include "include/themes.h"
#include "include/system.h"
#include "include/ioman.h"

#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"

static int appForceUpdate = 1;
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
	appForceUpdate = 1;
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
	if (appForceUpdate) {
		appForceUpdate = 0;
		return 1;
	}

	return 0;
}

static int appUpdateItemList(void) {
	appItemCount = 0;
	clearConfig(&configApps);

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

static void appDeleteItem(int id) {
	struct TConfigValue* cur = appGetConfigValue(id);
	fileXioRemove(cur->val);
	cur->key[0] = '\0';

	char path[255];
	snprintf(path, 255, "%s/conf_apps.cfg", gBaseMCDir);
	writeConfig(&configApps, path);

	appForceUpdate = 1;
}

static void appRenameItem(int id, char* newName) {
	struct TConfigValue* cur = appGetConfigValue(id);

	char value[255];
	strncpy(value, cur->val, 255);
	configRemoveKey(&configApps, cur->key);
	setConfigStr(&configApps, newName, value);

	char path[255];
	snprintf(path, 255, "%s/conf_apps.cfg", gBaseMCDir);
	writeConfig(&configApps, path);

	appForceUpdate = 1;
}

static void appLaunchItem(int id) {
	struct TConfigValue* cur = appGetConfigValue(id);
	int fd = fioOpen(cur->val, O_RDONLY);
	if (fd >= 0) {
		fioClose(fd);

		int exception = NO_EXCEPTION;
		if (strncmp(cur->val, "pfs0:", 5) == 0)
			exception = UNMOUNT_EXCEPTION;

		shutdown(exception);
		sysExecElf(cur->val, 0, NULL);
	}
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
	LOG("appCleanUp()\n");

	clearConfig(&configApps);
}

static item_list_t appItemList = {
		APP_MODE, 32, 0, 0, MENU_MIN_INACTIVE_FRAMES, "Applications", _STR_APPS, &appInit, &appNeedsUpdate,	&appUpdateItemList,
		&appGetItemCount, NULL, &appGetItemName, &appGetItemStartup, &appDeleteItem, &appRenameItem, NULL, NULL, &appLaunchItem,
		&appGetArt, &appCleanUp, APP_ICON
};
