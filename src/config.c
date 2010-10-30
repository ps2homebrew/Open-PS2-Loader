/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include "include/util.h"
#include "include/ioman.h"
#include <string.h>

#define CONFIG_INDEX_OPL		0
#define CONFIG_INDEX_COMPAT		1
#define CONFIG_INDEX_DNAS		2
#define CONFIG_INDEX_VMC		3

static config_set_t configFiles[CONFIG_FILE_NUM];

static int strToColor(const char *string, unsigned char *color) {
	int cnt=0, n=0;
	color[0]=0;
	color[1]=0;
	color[2]=0;

	if (!string || !*string) return 0;
	if (string[0]!='#') return 0;
	
	string++;

	while (*string) {
		int fh = fromHex(*string);
		if (fh >= 0) {
			color[n] = color[n] * 16 + fh;
		} else {
			break;
		}

		// Two characters per color
		if(cnt==1) {
			cnt=0;
			n++;
		}else{
			cnt++;
		}
		
		string++;
	}
	
	return 1;
}

static int splitAssignment(char* line, char* key, char* val) {
	// find "=". 
	// If found, the text before is key, after is val. 
	// Otherwise malformed string is encountered
	char* eqpos = strchr(line, '=');
	
	if (eqpos) {
		// copy the name and the value
		strncpy(key, line, eqpos - line);
		eqpos++;
		strcpy(val, eqpos);
	}
	return (int) eqpos;
}

static int configKeyValidate(const char* key) {
	if (strlen(key) == 0)
		return 0;
	
	return !strchr(key, '=');
}

static struct config_value_t* allocConfigItem(const char* key, const char* val) {
	struct config_value_t* it = (struct config_value_t*) malloc(sizeof(struct config_value_t));
	strncpy(it->key, key, 32);
	it->key[min(strlen(key), 31)] = '\0';
	strncpy(it->val, val, 255);
	it->val[min(strlen(val), 254)] = '\0';
	it->next = NULL;
	
	return it;
}

/// Low level key addition. Does not check for uniqueness.
static void addConfigValue(config_set_t* configSet, const char* key, const char* val) {
	if (!configSet->tail) {
		configSet->head = allocConfigItem(key, val);
		configSet->tail = configSet->head;
	} else {
		configSet->tail->next = allocConfigItem(key, val);
		configSet->tail = configSet->tail->next;
	}
}

static struct config_value_t* getConfigItemForName(config_set_t* configSet, const char* name) {
	struct config_value_t* val = configSet->head;
	
	while (val) {
		if (strncmp(val->key, name, 32) == 0)
			break;
		
		val = val->next;
	}
	
	return val;
}

void configInit(char *prefix) {
	char path[255];
	snprintf(path, 255, "%s/conf_opl.cfg", prefix);
	configAlloc(CONFIG_OPL, &configFiles[CONFIG_INDEX_OPL], path);
	snprintf(path, 255, "%s/conf_compatibility.cfg", prefix);
	configAlloc(CONFIG_COMPAT, &configFiles[CONFIG_INDEX_COMPAT], path);
	snprintf(path, 255, "%s/conf_dnas.cfg", prefix);
	configAlloc(CONFIG_DNAS, &configFiles[CONFIG_INDEX_DNAS], path);
	snprintf(path, 255, "%s/conf_vmc.cfg", prefix);
	configAlloc(CONFIG_VMC, &configFiles[CONFIG_INDEX_VMC], path);
}

void configEnd() {
	int index = 0;
	while(index < CONFIG_FILE_NUM) {
		config_set_t *configSet = &configFiles[index];

		configClear(configSet);
		free(configSet->filename);
		configSet->filename = NULL;
		index++;
	}
}

config_set_t *configAlloc(int type, config_set_t *configSet, char *fileName) {
	if (!configSet)
		configSet = (config_set_t*) malloc(sizeof(config_set_t));

	configSet->type = type;
	configSet->head = NULL;
	configSet->tail = NULL;
	if (fileName) {
		int length = strlen(fileName) + 1;
		configSet->filename = (char*) malloc(length * sizeof(char));
		memcpy(configSet->filename, fileName, length);
	} else
		configSet->filename = NULL;
	configSet->modified = 0;
	return configSet;
}

void configFree(config_set_t *configSet) {
	configClear(configSet);
	free(configSet->filename);
	free(configSet);
	configSet = NULL;
}

config_set_t *configGetByType(int type) {
	int index = 0;
	while(index < CONFIG_FILE_NUM) {
		config_set_t *configSet = &configFiles[index];

		if (configSet->type == type)
			return configSet;
		index++;
	}
	return NULL;
}

int configSetStr(config_set_t* configSet, const char* key, const char* value) {
	if (!configKeyValidate(key))
		return 0;
	
	struct config_value_t *it = getConfigItemForName(configSet, key);
	
	if (it) {
		if (strncmp(it->val, value, 255) != 0) {
			strncpy(it->val, value, 255);
			it->val[min(strlen(value), 254)] = '\0';
			configSet->modified = 1;
		}
	} else {
		addConfigValue(configSet, key, value);
		configSet->modified = 1;
	}
	
	return 1;
}

// sets the value to point to the value str in the config. Do not overwrite - it will overwrite the string in config
int configGetStr(config_set_t* configSet, const char* key, char** value) {
	if (!configKeyValidate(key))
		return 0;
	
	struct config_value_t *it = getConfigItemForName(configSet, key);
	
	if (it) {
		*value = it->val;
		return 1;
	} else
		return 0;
}
	
int configSetInt(config_set_t* configSet, const char* key, const int value) {
	char tmp[12];
	snprintf(tmp, 12, "%d", value);
	return configSetStr(configSet, key, tmp);
}

int configGetInt(config_set_t* configSet, char* key, int* value) {
	char *valref = NULL;
	if (configGetStr(configSet, key, &valref)) {
		*value = atoi(valref);
		return 1;
	} else {
		return 0;
	}
}

int configSetColor(config_set_t* configSet, const char* key, unsigned char* color) {
	char tmp[8];
	snprintf(tmp, 8, "#%02X%02X%02X", color[0], color[1], color[2]);
	return configSetStr(configSet, key, tmp);
}

int configGetColor(config_set_t* configSet, const char* key, unsigned char* color) {
	char *valref = NULL;
	if (configGetStr(configSet, key, &valref)) {
		strToColor(valref, color);
		return 1;
	} else {
		return 0;
	}
}

int configRemoveKey(config_set_t* configSet, const char* key) {
	if (!configKeyValidate(key))
		return 0;
	
	struct config_value_t* val = configSet->head;
	struct config_value_t* prev = NULL;
	
	while (val) {
		if (strncmp(val->key, key, 32) == 0) {
			if (val == configSet->tail)
				configSet->tail = prev;

			val = val->next;
			if (prev) {
				free(prev->next);
				prev->next = val;
			}
			else {
				free(configSet->head);
				configSet->head = val;
			}

			configSet->modified = 1;
		} else {
			prev = val;
			val = val->next;
		}
	}
	
	return 1;
}

void configReadIP() {
	int fd = openFile("mc?:SYS-CONF/IPCONFIG.DAT", O_RDONLY);
	if (fd >= 0) {
		char ipconfig[255];
		int size = getFileSize(fd);
		fioRead(fd, &ipconfig, size);
		fioClose(fd);
	
		sscanf(ipconfig, "%d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d", &ps2_ip[0], &ps2_ip[1], &ps2_ip[2], &ps2_ip[3],
			&ps2_netmask[0], &ps2_netmask[1], &ps2_netmask[2], &ps2_netmask[3],
			&ps2_gateway[0], &ps2_gateway[1], &ps2_gateway[2], &ps2_gateway[3]);
	}
	
	return;
}

void configWriteIP() {
	int fd = openFile("mc?:SYS-CONF/IPCONFIG.DAT", O_WRONLY | O_CREAT);
	if (fd >= 0) {
		char ipconfig[255];
		sprintf(ipconfig, "%d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d\r\n", ps2_ip[0], ps2_ip[1], ps2_ip[2], ps2_ip[3],
			ps2_netmask[0], ps2_netmask[1], ps2_netmask[2], ps2_netmask[3],
			ps2_gateway[0], ps2_gateway[1], ps2_gateway[2], ps2_gateway[3]);
	
		fioWrite(fd, ipconfig, strlen(ipconfig));
		fioClose(fd);
	}
}

void configGetDiscID(char* startup, char* discID) {
	char *valref = NULL;
	char gkey[255];
	snprintf(gkey, 255, "%s_discID", startup);
	if (configGetStr(&configFiles[CONFIG_INDEX_DNAS], gkey, &valref))
		strncpy(discID, valref, 32);
	else
		discID[0] = '\0';
}

void configSetDiscID(char* startup, const char *discID) {
	char gkey[255];
	snprintf(gkey, 255, "%s_discID", startup);
	configSetStr(&configFiles[CONFIG_INDEX_DNAS], gkey, discID);
}

void configRemoveDiscID(char* startup) {
	char gkey[255];
	snprintf(gkey, 255, "%s_discID", startup);
	configRemoveKey(&configFiles[CONFIG_INDEX_DNAS], gkey);
}

// dst has to have 5 bytes space
void configGetDiscIDBinary(char* startup, void* dst) {
	memset(dst, 0, 5);

	char *gid = NULL;
	char gkey[255];
	snprintf(gkey, 255, "%s_discID", startup);
	if (configGetStr(&configFiles[CONFIG_INDEX_DNAS], gkey, &gid)) {
		// convert from hex to binary
		char* cdst = dst;
		int p = 0;
		while (*gid && p < 10) {
			int dv = -1;

			while (dv < 0 && *gid) // skip spaces, etc
				dv = fromHex(*(gid++));

			if (dv < 0)
				break;

			*cdst = *cdst * 16 + dv;
			if ((++p & 1) == 0)
				cdst++;
		}
	}
}

int configRead(config_set_t* configSet) {
	file_buffer_t* fileBuffer = openFileBuffer(configSet->filename, O_RDONLY, 0, 4096);
	if (!fileBuffer) {
		LOG("No config %s.\n", configSet->filename);
		configSet->modified = 0;
		return 0;
	}
	
	char* line;
	unsigned int lineno = 0;
	while (readFileBuffer(fileBuffer, &line)) {
		lineno++;
		
		char key[255], val[255];
		memset(key, 0, 255);
		memset(val, 0, 255);
		
		if (splitAssignment(line, key, val)) {
			// insert config value
			configSetStr(configSet, key, val);
		} else {
			LOG("Malformed config file '%s' line %d: '%s'\n", configSet->filename, lineno, line);
		}
	}
	closeFileBuffer(fileBuffer);
	configSet->modified = 0;
	return 1;
}

int configWrite(config_set_t* configSet) {
	if (configSet->modified) {
		file_buffer_t* fileBuffer = openFileBuffer(configSet->filename, O_WRONLY | O_CREAT | O_TRUNC, 0, 4096);
		if (fileBuffer) {
			char line[512];
			struct config_value_t* cur = configSet->head;

			while (cur) {
				if (cur->key[0] != '\0') {
					snprintf(line, 512, "%s=%s\r\n", cur->key, cur->val); // add windows CR+LF (0x0D 0x0A)
					writeFileBuffer(fileBuffer, line, strlen(line));
				}

				// and advance
				cur = cur->next;
			}

			closeFileBuffer(fileBuffer);
			configSet->modified = 0;
			return 1;
		}
		return 0;
	}
	return 1;
}

void configClear(config_set_t* configSet) {
	while (configSet->head) {
		struct config_value_t* cur = configSet->head;
		configSet->head = cur->next;
		
		free(cur);
	}
	
	configSet->head = NULL;
	configSet->tail = NULL;
	configSet->modified = 1;
}

int configGetCompatibility(char* startup, int mode, int *dmaMode) {
	char gkey[255];
	snprintf(gkey, 255, "%s_%d", startup, mode);

	unsigned int compatMode;
	if (!configGetInt(&configFiles[CONFIG_INDEX_COMPAT], gkey, &compatMode))
		compatMode = 0;

	if (dmaMode) {
		*dmaMode = 7; // defaulting to UDMA 4
		snprintf(gkey, 255, "%s_DMA", startup);
		configGetInt(&configFiles[CONFIG_INDEX_COMPAT], gkey, dmaMode);
	}

	return compatMode;
}

void configSetCompatibility(char* startup, int mode, int compatMode, int dmaMode) {
	char gkey[255];
	snprintf(gkey, 255, "%s_%d", startup, mode);
	if (compatMode == 0) // means we want to delete the setting
		configRemoveKey(&configFiles[CONFIG_INDEX_COMPAT], gkey);
	else
		configSetInt(&configFiles[CONFIG_INDEX_COMPAT], gkey, compatMode);

	if (dmaMode != -1) {
		snprintf(gkey, 255, "%s_DMA", startup);
		if (dmaMode == 7) // UDMA 4 is the default so don't save it (useless lines into the conf file)
			configRemoveKey(&configFiles[CONFIG_INDEX_COMPAT], gkey);
		else
			configSetInt(&configFiles[CONFIG_INDEX_COMPAT], gkey, dmaMode);
	}
}

int configReadMulti(int types) {
	int result = 0;

	if (CONFIG_OPL & types)
		configReadIP();

	int index = 0;
	while(index < CONFIG_FILE_NUM) {
		config_set_t *configSet = &configFiles[index];

		if (configSet->type & types) {
			configClear(configSet);
			result += configRead(configSet);
		}
		index++;
	}

	return result;
}

int configWriteMulti(int types) {
	int result = 0;

	if ((CONFIG_OPL & types) && gIPConfigChanged)
		configWriteIP();

	int index = 0;
	while(index < CONFIG_FILE_NUM) {
		config_set_t *configSet = &configFiles[index];

		if (configSet->type & types)
			result += configWrite(configSet);
		index++;
	}

	return result;
}

#ifdef VMC
void configGetVMC(char* startup, char* vmc, int mode, int slot) {
	char *valref = NULL;
	char gkey[255];
	snprintf(gkey, 255, "%s_%d_%d", startup, mode, slot);
	if (configGetStr(&configFiles[CONFIG_INDEX_VMC], gkey, &valref))
		strncpy(vmc, valref, 32);
	else
		vmc[0] = '\0';
}

void configSetVMC(char* startup, const char* vmc, int mode, int slot) {
	char gkey[255];
	if(vmc[0] == '\0') {
		configRemoveVMC(startup, mode, slot);
		return;
	}
	snprintf(gkey, 255, "%s_%d_%d", startup, mode, slot);
	configSetStr(&configFiles[CONFIG_INDEX_VMC], gkey, vmc);
}

void configRemoveVMC(char *startup, int mode, int slot) {
	char gkey[255];
	snprintf(gkey, 255, "%s_%d_%d", startup, mode, slot);
	configRemoveKey(&configFiles[CONFIG_INDEX_VMC], gkey);
}
#endif
