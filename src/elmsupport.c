#include "include/opl.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/elmsupport.h"
#include "include/themes.h"
#include "include/system.h"
#include "include/ioman.h"

#include "include/usbsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"
#include "include/supportbase.h"

static int elmForceUpdate = 1;
static int elmItemCount = 0;

static config_set_t *configElm;

// forward declaration
static item_list_t elmItemList;

static struct config_value_t* elmGetConfigValue(int id) {
	struct config_value_t* cur = configElm->head;

	while (id--) {
		cur = cur->next;
	}

	return cur;
}

static char* elmGetELFName(char* name) {
	// Looking for the ELF name
	char* pos = strrchr(name, '/');
	if (!pos)
		pos = strrchr(name, ':');
	if (pos) {
		return pos + 1;
	}

	return name;
}

void elmInit(void) {
	LOG("ELMSUPPORT Init\n");
	elmForceUpdate = 1;
	configGetInt(configGetByType(CONFIG_OPL), "elm_frames_delay", &elmItemList.delay);
	configElm = configGetByType(CONFIG_ELM);
	elmItemList.enabled = 1;
}

item_list_t* elmGetObject(int initOnly) {
	if (initOnly && !elmItemList.enabled)
		return NULL;
	return &elmItemList;
}

static int elmNeedsUpdate(void) {
	if (elmForceUpdate) {
		elmForceUpdate = 0;
		return 1;
	}

	return 0;
}

static int elmUpdateItemList(void) {
	char path[256];
	static item_list_t *listSupport = NULL;
	int ret=0; //Return from configRead
	elmItemCount = 0;
	
	//Clear config if already exists
	if (configElm != NULL)
		configFree(configElm);
	
	//Try MC?:/OPL/conf_elms.cfg
	snprintf(path, sizeof(path), "%s/conf_elms.cfg", gBaseMCDir);
	configElm = configAlloc(CONFIG_ELM, NULL, path);
	ret = configRead(configElm);
	
	//Try HDD
	if ( ret == 0 && (listSupport = hddGetObject(1)) ) {
		if (configElm != NULL){
			configFree(configElm);
		}
		snprintf(path, sizeof(path), "%sconf_elms.cfg", hddGetPrefix());
		configElm = configAlloc(CONFIG_ELM, NULL, path);
		ret = configRead(configElm);
	}

	//Try ETH
	if ( ret == 0 && (listSupport = ethGetObject(1)) ) {
		if (configElm != NULL){
			configFree(configElm);
		}
		snprintf(path, sizeof(path), "%sconf_elms.cfg", ethGetPrefix());
		configElm = configAlloc(CONFIG_ELM, NULL, path);
		ret = configRead(configElm);	
	}

	//Try USB
	if ( ret == 0 && (listSupport = usbGetObject(1)) ){
		if (configElm != NULL){
			configFree(configElm);
		}
		snprintf(path, sizeof(path), "%sconf_elms.cfg", usbGetPrefix());
		configElm = configAlloc(CONFIG_ELM, NULL, path);
		ret = configRead(configElm);
	}

	//Count elm
	if (configElm->head) {
		struct config_value_t* cur = configElm->head;
		while (cur) {
			cur = cur->next;
			elmItemCount++;
		}
	}
	return elmItemCount;
}

static int elmGetItemCount(void) {
	return elmItemCount;
}

static char* elmGetItemName(int id) {
	struct config_value_t* cur = elmGetConfigValue(id);
	return cur->key;
}

static int elmGetItemNameLength(int id) {
	return 32;
}

static char* elmGetItemStartup(int id) {
	struct config_value_t* cur = elmGetConfigValue(id);
	return elmGetELFName(cur->val);
}

#ifndef __CHILDPROOF
static void elmDeleteItem(int id) {
	struct config_value_t* cur = elmGetConfigValue(id);
	fileXioRemove(cur->val);
	cur->key[0] = '\0';
	configElm->modified = 1;
	configWrite(configElm);

	elmForceUpdate = 1;
}

static void elmRenameItem(int id, char* newName) {
	struct config_value_t* cur = elmGetConfigValue(id);

	char value[256];
	strncpy(value, cur->val, sizeof(value));
	configRemoveKey(configElm, cur->key);
	configSetStr(configElm, newName, value);
	configWrite(configElm);

	elmForceUpdate = 1;
}
#endif

static void elmLaunchItem(int id, config_set_t* configSet) {
	struct config_value_t* cur = elmGetConfigValue(id);
	int fd = fileXioOpen(cur->val, O_RDONLY, 0666);
	if (fd >= 0) {
		fileXioClose(fd);

		int exception = NO_EXCEPTION;
		if (strncmp(cur->val, "pfs0:", 5) == 0)
			exception = UNMOUNT_EXCEPTION;

		char filename[256];
		sprintf(filename,"%s",cur->val);
		deinit(exception); // CAREFUL: deinit will call elmCleanUp, so configElm/cur will be freed
		sysExecElf(filename);
	}
	else
		guiMsgBox(_l(_STR_ERR_FILE_INVALID), 0, NULL);
}

static config_set_t* elmGetConfig(int id) {
	config_set_t* config = NULL;
	static item_list_t *listSupport = NULL;
	struct config_value_t* cur = elmGetConfigValue(id);
	int ret=0;
	
	//Search on HDD, SMB, USB for the CFG/GAME.ELF.cfg file.
	//HDD
	if ( (listSupport = hddGetObject(1)) ) {
		char path[256];
		#if OPL_IS_DEV_BUILD
			snprintf(path, sizeof(path), "%sCFG-DEV/%s.cfg", hddGetPrefix(), elmGetELFName(cur->val));
		#else
			snprintf(path, sizeof(path), "%sCFG/%s.cfg", hddGetPrefix(), elmGetELFName(cur->val));
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
			snprintf(path, sizeof(path), "%sCFG-DEV/%s.cfg", ethGetPrefix(), elmGetELFName(cur->val));
		#else
			snprintf(path, sizeof(path), "%sCFG/%s.cfg", ethGetPrefix(),elmGetELFName(cur->val));
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
			snprintf(path, sizeof(path), "%sCFG-DEV/%s.cfg", usbGetPrefix(),  elmGetELFName(cur->val));
		#else
			snprintf(path, sizeof(path), "%sCFG/%s.cfg", usbGetPrefix(), elmGetELFName(cur->val));
		#endif
		config = configAlloc(1, NULL, path);
		ret = configRead(config);
	}

	if (ret == 0){ //No config found on previous devices, create one.
		if (config != NULL)
			configFree(config);
		
		config = configAlloc(1, NULL, NULL);
	}
	
	configSetStr(config, CONFIG_ITEM_NAME, elmGetELFName(cur->val));
	configSetStr(config, CONFIG_ITEM_LONGNAME, cur->key);
	configSetStr(config, CONFIG_ITEM_STARTUP, cur->val);
	configSetStr(config, CONFIG_ITEM_FORMAT, "ELF");
	configSetStr(config, CONFIG_ITEM_MEDIA, "ELM");
	return config;
}

static int elmGetImage(char* folder, int isRelative, char* value, char* suffix, GSTEXTURE* resultTex, short psm) {
	//value = elmGetELFName(value);
	// Search every device from fastest to slowest (HDD > ETH > USB)
	static item_list_t *listSupport = NULL;
	if ( (listSupport = hddGetObject(1)) ) {
		if (listSupport->itemGetImage(folder, isRelative, value, suffix, resultTex, psm) >= 0)
			return 0;
	}

	if ( (listSupport = ethGetObject(1)) ) {
		if (listSupport->itemGetImage(folder, isRelative, value, suffix, resultTex, psm) >= 0)
			return 0;
	}

	if ( (listSupport = usbGetObject(1)) )
		return listSupport->itemGetImage(folder, isRelative, value, suffix, resultTex, psm);

	return -1;
}

static void elmCleanUp(int exception) {
	if (elmItemList.enabled) {
		LOG("ELMSUPPORT CleanUp\n");
	}
}

static item_list_t elmItemList = {
		ELM_MODE, 0, MODE_FLAG_NO_COMPAT|MODE_FLAG_NO_UPDATE, MENU_MIN_INACTIVE_FRAMES, ELM_MODE_UPDATE_DELAY, "ELF Loader Menu", _STR_ELM, &elmInit, &elmNeedsUpdate,	&elmUpdateItemList,
#ifdef __CHILDPROOF
		&elmGetItemCount, NULL, &elmGetItemName, &elmGetItemNameLength, &elmGetItemStartup, NULL, NULL, &elmLaunchItem,
#else
		&elmGetItemCount, NULL, &elmGetItemName, &elmGetItemNameLength, &elmGetItemStartup, &elmDeleteItem, &elmRenameItem, &elmLaunchItem,
#endif
#ifdef VMC
		&elmGetConfig, &elmGetImage, &elmCleanUp, NULL, ELM_ICON
#else
		&elmGetConfig, &elmGetImage, &elmCleanUp, ELM_ICON
#endif
};
