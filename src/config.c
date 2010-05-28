/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include "include/util.h"
#include "include/ioman.h"
#include <string.h>

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

int configKeyValidate(const char* key) {
	if (strlen(key) == 0)
		return 0;
	
	return !strchr(key, '=');
}

static struct TConfigValue* allocConfigItem(const char* key, const char* val) {
	struct TConfigValue* it = (struct TConfigValue*)malloc(sizeof(struct TConfigValue));
	strncpy(it->key, key, 32);
	it->key[min(strlen(key), 31)] = '\0';
	strncpy(it->val, val, 255);
	it->val[min(strlen(val), 254)] = '\0';
	it->next = NULL;
	
	return it;
}

/// Low level key addition. Does not check for uniqueness.
static void addConfigValue(struct TConfigSet* config, const char* key, const char* val) {
	if (!config->tail) {
		config->head = allocConfigItem(key, val);
		config->tail = config->head;
	} else {
		config->tail->next = allocConfigItem(key, val);
		config->tail = config->tail->next;
	}
}

static struct TConfigValue* getConfigItemForName(struct TConfigSet* config, const char* name) {
	struct TConfigValue* val = config->head;
	
	while (val) {
		if (strncmp(val->key, name, 32) == 0)
			break;
		
		val = val->next;
	}
	
	return val;
}

// --------------------------------------------------------------------------------------
// ------------------------------ Config getters and setters ----------------------------
// --------------------------------------------------------------------------------------
int setConfigStr(struct TConfigSet* config, const char* key, const char* value) {
	if (!configKeyValidate(key))
		return 0;
	
	struct TConfigValue *it = getConfigItemForName(config, key);
	
	if (it) {
		strncpy(it->val, value, 255);
		it->val[min(strlen(value), 254)] = '\0';
	} else {
		addConfigValue(config, key, value);
	}
	
	return 1;
}

// sets the value to point to the value str in the config. Do not overwrite - it will overwrite the string in config
int getConfigStr(struct TConfigSet* config, const char* key, char** value) {
	if (!configKeyValidate(key))
		return 0;
	
	struct TConfigValue *it = getConfigItemForName(config, key);
	
	if (it) {
		*value = it->val;
		return 1;
	} else {
		return 0;
	}
}
	
int setConfigInt(struct TConfigSet* config, const char* key, const int value) {
	char tmp[12];
	
	snprintf(tmp, 12, "%d", value);
	return setConfigStr(config, key, tmp);
}

int getConfigInt(struct TConfigSet* config, char* key, int* value) {
	char *valref = NULL;
	
	if (getConfigStr(config, key, &valref)) {
		// convert to int
		*value = atoi(valref);
		return 1;
	} else {
		return 0;
	}
}

int setConfigColor(struct TConfigSet* config, const char* key, unsigned char* color) {
	char tmp[8];
	
	snprintf(tmp, 8, "#%02X%02X%02X", color[0], color[1], color[2]);
	return setConfigStr(config, key, tmp);
}

int getConfigColor(struct TConfigSet* config, const char* key, unsigned char* color) {
	char *valref = NULL;
	
	if (getConfigStr(config, key, &valref)) {
		// convert to color
		strToColor(valref, color);
		return 1;
	} else {
		return 0;
	}
}

int configRemoveKey(struct TConfigSet* config, const char* key) {
	// remove config key from config set
	if (!configKeyValidate(key))
		return 0;
	
	struct TConfigValue* val = config->head;
	struct TConfigValue* prev = NULL;
	
	while (val) {
		if (strncmp(val->key, key, 32) == 0) {
			if (prev)
				prev->next = val->next;
			
			if (val == config->head)
				config->head = val->next;
			
			if (val == config->tail)
				config->tail = val->next;
			
			free(val);
		}
		
		prev = val;
		val = val->next;
	}
	
	return 1;
}

void readIPConfig() {
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

void writeIPConfig() {
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

char *getConfigDiscID(char* startup) {
	char *gid;
	char gkey[255];
	snprintf(gkey, 255, "%s_discID", startup);
	if (!getConfigStr(&gConfig, gkey, &gid))
		gid = ""; // TODO is this correct ?

	return gid;
}

void setConfigDiscID(char* startup, const char *gid) {
	char gkey[255];
	snprintf(gkey, 255, "%s_discID", startup);
	setConfigStr(&gConfig, gkey, gid);
}

void removeConfigDiscID(char* startup) {
	char gkey[255];
	snprintf(gkey, 255, "%s_discID", startup);
	configRemoveKey(&gConfig, gkey);
}

// dst has to have 5 bytes space
void getConfigDiscIDBinary(char* startup, void* dst) {
	char *gid;
	char gkey[255];
	snprintf(gkey, 255, "%s_discID", startup);
	if (!getConfigStr(&gConfig, gkey, &gid))
		gid = "";

	// convert from hex to binary
	memset(dst, 0, 5);
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

int readConfig(struct TConfigSet* config, char *fname) {
	file_buffer_t* fileBuffer = openFileBuffer(fname, O_RDONLY, 0, 4096);
	if (!fileBuffer) {
		LOG("No config. Exiting...\n");
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
			setConfigStr(config, key, val);
		} else {
			LOG("Malformed config file '%s' line %d: '%s'\n", fname, lineno, line);
		}
	}
	closeFileBuffer(fileBuffer);
	return 1;
}

int writeConfig(struct TConfigSet* config, char *fname) {
	file_buffer_t* fileBuffer = openFileBuffer(fname, O_WRONLY | O_CREAT | O_TRUNC, 0, 4096);
	if (fileBuffer) {
		char line[512];
		struct TConfigValue* cur = config->head;

		while (cur) {
			snprintf(line, 512, "%s=%s\r\n", cur->key, cur->val); // add windows CR+LF (0x0D 0x0A)
			writeFileBuffer(fileBuffer, line, strlen(line));

			// and advance
			cur = cur->next;
		}

		closeFileBuffer(fileBuffer);
		return 1;
	}
	return 0;
}

void clearConfig(struct TConfigSet* config) {
	while (config->head) {
		struct TConfigValue* cur = config->head;
		config->head = cur->next;
		
		free(cur);
	}
	
	config->head = NULL;
	config->tail = NULL;
}
