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
#define CONFIG_INDEX_LAST		1
#define CONFIG_INDEX_APPS		2
#define CONFIG_INDEX_VMODE		3

static config_set_t configFiles[CONFIG_FILE_NUM];
static char configPath[255] = "mc?:SYS-CONF/IPCONFIG.DAT";

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

/// true if given a whitespace character
int isWS(char c) {
	return c == ' ' || c == '\t';
}

static int splitAssignment(char* line, char* key, size_t keymax, char* val, size_t valmax) {
	// skip whitespace
	for (;isWS(*line); ++line);
	
	// find "=". 
	// If found, the text before is key, after is val. 
	// Otherwise malformed string is encountered
	
	char* eqpos = strchr(line, '=');
	
	if (eqpos) {
		// copy the name and the value
		size_t keylen = min(keymax, eqpos - line);
		
		strncpy(key, line, keylen);
		
		eqpos++;
		
		size_t vallen = min(valmax, strlen(line) - (eqpos - line));
		strncpy(val, eqpos, vallen);
	}
	
	return (int) eqpos;
}

static int parsePrefix(char* line, char* prefix) {
	// find "=". 
	// If found, the text before is key, after is val. 
	// Otherwise malformed string is encountered
	char* colpos = strchr(line, ':');
	
	if (colpos && colpos != line) {
		// copy the name and the value
		strncpy(prefix, line, colpos - line);
		
		return 1;
	} else {
		return 0;
	}
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

	if (prefix)
		snprintf(configPath, 255, "%s/IPCONFIG.DAT", prefix);
	else
		prefix = gBaseMCDir;

	snprintf(path, 255, "%s/conf_opl.cfg", prefix);
	configAlloc(CONFIG_OPL, &configFiles[CONFIG_INDEX_OPL], path);
	snprintf(path, 255, "%s/conf_last.cfg", prefix);
	configAlloc(CONFIG_LAST, &configFiles[CONFIG_INDEX_LAST], path);
	snprintf(path, 255, "%s/conf_apps.cfg", prefix);
	configAlloc(CONFIG_APPS, &configFiles[CONFIG_INDEX_APPS], path);
	snprintf(path, 255, "%s/conf_vmode.cfg", prefix);
	configAlloc(CONFIG_VMODE, &configFiles[CONFIG_INDEX_VMODE], path);
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
			if (it->key[0] != '#')
				configSet->modified = 1;
		}
	} else {
		addConfigValue(configSet, key, value);
		if (key[0] != '#')
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
			if (key[0] != '#')
				configSet->modified = 1;

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
		} else {
			prev = val;
			val = val->next;
		}
	}
	
	return 1;
}

void configReadIP() {
	int fd = openFile(configPath, O_RDONLY);
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
	int fd = openFile(configPath, O_WRONLY | O_CREAT);
	if (fd >= 0) {
		char ipconfig[255];
		sprintf(ipconfig, "%d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d\r\n", ps2_ip[0], ps2_ip[1], ps2_ip[2], ps2_ip[3],
			ps2_netmask[0], ps2_netmask[1], ps2_netmask[2], ps2_netmask[3],
			ps2_gateway[0], ps2_gateway[1], ps2_gateway[2], ps2_gateway[3]);
	
		fioWrite(fd, ipconfig, strlen(ipconfig));
		fioClose(fd);
	}
}

void configGetDiscID(config_set_t* configSet, char* discID) {
	char *valref = NULL;
	if (configGetStr(configSet, CONFIG_ITEM_DNAS, &valref))
		strncpy(discID, valref, 32);
	else
		discID[0] = '\0';
}

void configSetDiscID(config_set_t* configSet, const char *discID) {
	configSetStr(configSet, CONFIG_ITEM_DNAS, discID);
}

void configRemoveDiscID(config_set_t* configSet) {
	configRemoveKey(configSet, CONFIG_ITEM_DNAS);
}

// dst has to have 5 bytes space
void configGetDiscIDBinary(config_set_t* configSet, void* dst) {
	memset(dst, 0, 5);

	char *gid = NULL;
	if (configGetStr(configSet, CONFIG_ITEM_DNAS, &gid)) {
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
	
	char prefix[32];
	memset(prefix, 0, sizeof(prefix));
	
	while (readFileBuffer(fileBuffer, &line)) {
		lineno++;
		
		char key[32], val[255];
		memset(key, 0, sizeof(key));
		memset(val, 0, sizeof(val));
		
		if (splitAssignment(line, key, sizeof(key), val, sizeof(val))) {
			/* if the line does not start with whitespace,
			* the prefix ends and we have to reset it
			*/
			if (!isWS(line[0]))
				memset(prefix, 0, sizeof(prefix));

			// insert config value
			if (prefix[0]) {
				// we have a prefix
				char composedKey[66];
				
				snprintf(composedKey, 65, "%s_%s", prefix, key);
				composedKey[65] = '\0';
				
				configSetStr(configSet, composedKey, val);
			} else {
				configSetStr(configSet, key, val);
			}
		} else if (parsePrefix(line, prefix)) {
			// prefix is set, that's about it
		} else {
			LOG("Malformed config file '%s' line %d: '%s'\n", configSet->filename, lineno, line);
		}
	}
	closeFileBuffer(fileBuffer);
	configSet->modified = 0;
	return configSet->type;
}

int configWrite(config_set_t* configSet) {
	if (configSet->modified) {
		file_buffer_t* fileBuffer = openFileBuffer(configSet->filename, O_WRONLY | O_CREAT | O_TRUNC, 0, 4096);
		if (fileBuffer) {
			char line[512];

			struct config_value_t* cur = configSet->head;
			while (cur) {
				if ((cur->key[0] != '\0') && (cur->key[0] != '#')) {
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

int configGetCompatibility(config_set_t* configSet, int *dmaMode) {
	unsigned int compatMode;
	if (!configGetInt(configSet, CONFIG_ITEM_COMPAT, &compatMode))
		compatMode = 0;

	if (dmaMode) {
		*dmaMode = 7; // defaulting to UDMA 4
		configGetInt(configSet, CONFIG_ITEM_DMA, dmaMode);
	}

	return compatMode;
}

void configSetCompatibility(config_set_t* configSet, int compatMode, int dmaMode) {
	if (compatMode == 0) // means we want to delete the setting
		configRemoveKey(configSet, CONFIG_ITEM_COMPAT);
	else
		configSetInt(configSet, CONFIG_ITEM_COMPAT, compatMode);

	if (dmaMode != -1) {
		if (dmaMode == 7) // UDMA 4 is the default so don't save it (useless lines into the conf file)
			configRemoveKey(configSet, CONFIG_ITEM_DMA);
		else
			configSetInt(configSet, CONFIG_ITEM_DMA, dmaMode);
	}
}

void configGetAltStartup(config_set_t* configSet, char* elfname) {
	char *valref = NULL;
	if (configGetStr(configSet, CONFIG_ITEM_ALTSTARTUP, &valref))
		strncpy(elfname, valref, 32);
	else
		elfname[0] = '\0';
}

void configSetAltStartup(config_set_t* configSet, char *elfname) {
	int i = 0;
	for (; elfname[i]; i++) {
		if (elfname[i] > 96 && elfname[i] < 123)
			elfname[i] -= 32;
	}

	configSetStr(configSet, CONFIG_ITEM_ALTSTARTUP, elfname);
}

void configRemoveAltStartup(config_set_t* configSet) {
	configRemoveKey(configSet, CONFIG_ITEM_ALTSTARTUP);
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
			result |= configRead(configSet);
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
void configGetVMC(config_set_t* configSet, char* vmc, int slot) {
	char *valref = NULL;
	char gkey[255];
	snprintf(gkey, 255, "%s_%d", CONFIG_ITEM_VMC, slot);
	if (configGetStr(configSet, gkey, &valref))
		strncpy(vmc, valref, 32);
	else
		vmc[0] = '\0';
}

void configSetVMC(config_set_t* configSet, const char* vmc, int slot) {
	char gkey[255];
	if(vmc[0] == '\0') {
		configRemoveVMC(configSet, slot);
		return;
	}
	snprintf(gkey, 255, "%s_%d", CONFIG_ITEM_VMC, slot);
	configSetStr(configSet, gkey, vmc);
}

void configRemoveVMC(config_set_t* configSet, int slot) {
	char gkey[255];
	snprintf(gkey, 255, "%s_%d", CONFIG_ITEM_VMC, slot);
	configRemoveKey(configSet, gkey);
}
#endif
