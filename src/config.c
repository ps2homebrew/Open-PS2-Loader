/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include <string.h>
#include "fileio.h"
#include <sys/stat.h>

static char IPconfig_path[] = "mc?:/SYS-CONF/IPCONFIG.DAT";
int filesize=0;

/// read a line from the specified file handle, up to maxlen characters long, into the buffer
int readline(int fd, char* buffer, unsigned int maxlen) {
	char ch = '\0';
	buffer[0] = '\0';
	int res = 1, n = 0;
	
	while ((ch != 13)) {
		res=fioRead(fd, &ch, 1);
		
		if (res != 1) {
			res = 0;
			break;
		}
		
		if(ch != 10 && ch != 13) {
			buffer[n++] = ch;
			buffer[n] = '\0';
		} else {
			break;
		}
		
		if (n == maxlen - 1)
			break;
	};

	buffer[n] = '\0';
	return res;
}

// as there seems to be no strpos in my libs (?)
int strpos(const char* str, char c) {
	int pos;
	
	for (pos = 0; *str; ++pos, ++str)
		if (*str == c)
			return pos;

	return -1;
}

// a simple maximum of two inline
inline int max(int a, int b) {
	return a > b ? a : b;
}

// a simple minimum of two inline
inline int min(int a, int b) {
	return a < b ? a : b;
}

int strToColor(const char *string, unsigned char *color) {
	int cnt=0, n=0;
	color[0]=0;
	color[1]=0;
	color[2]=0;

	if (!string || !*string) return 0;
	if (string[0]!='#') return 0;
	
	string++;

	while (*string) {
		if ((*string >= '0') && (*string <= '9')) {
			color[n] = color[n] * 16 + (*string - '0');
		} else if ( (*string >= 'A') && (*string <= 'F') ) {
			color[n] = color[n] * 16 + (*string - 'A'+10);
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

int splitAssignment(const char* line, char* key, char* val) {
	// find "=". 
	// If found, the text before is key, after is val. 
	// Otherwise malformed string is encountered
	int eqpos = strpos(line, '=');
	
	if (eqpos < 0)
		return 0;
	
	// copy the name and the value
	strncpy(key, line, eqpos);
	eqpos++;
	strncpy(val, &line[eqpos], strlen(line) - eqpos);
	
	return 1;
}

int configKeyValidate(const char* key) {
	if (strlen(key) == 0)
		return 0;
	
	int eqpos = strpos(key, '=');
	
	if (eqpos < 0)
		return 1;
	
	return 0;
}

void configValueToText(struct TConfigValue* val, char* buf, unsigned int size) {
	snprintf(buf, size, "%s=%s", val->key, val->val);
}

struct TConfigValue* allocConfigItem(const char* key, const char* val) {
	struct TConfigValue* it = (struct TConfigValue*)malloc(sizeof(struct TConfigValue));
	strncpy(it->key, key, 32);
	it->key[min(strlen(key), 31)] = '\0';
	strncpy(it->val, val, 255);
	it->val[min(strlen(val), 254)] = '\0';
	it->next = NULL;
	
	return it;
}

/// Low level key addition. Does not check for uniqueness.
void addConfigValue(struct TConfigSet* config, const char* key, const char* val) {
	if (!config->tail) {
		config->head = allocConfigItem(key, val);
		config->tail = config->head;
	} else {
		config->tail->next = allocConfigItem(key, val);
		config->tail = config->tail->next;
	}
}

struct TConfigValue* getConfigItemForName(struct TConfigSet* config, const char* name) {
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
	char ipconfig[255];
	
	IPconfig_path[2] = '0';
	int fd=fioOpen(IPconfig_path, O_RDONLY);
	if (fd<0) {
		IPconfig_path[2] = '1';
		fd=fioOpen(IPconfig_path, O_RDONLY);
		if (fd<0) {
			//DEBUG: printf("No config. Exiting...\n");
			IPconfig_path[2] = '?';
			return;
		}
	}
	filesize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	
	fioRead(fd, &ipconfig, filesize);
		
	sscanf(ipconfig, "%d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d", &ps2_ip[0], &ps2_ip[1], &ps2_ip[2], &ps2_ip[3],
															&ps2_netmask[0], &ps2_netmask[1], &ps2_netmask[2], &ps2_netmask[3],
															&ps2_gateway[0], &ps2_gateway[1], &ps2_gateway[2], &ps2_gateway[3]);

	
	fioClose(fd);
	
	return;
}

void writeIPConfig() {
	char ipconfig[255];
	
	if (!strncmp (IPconfig_path,"mc0:/SYS-CONF",13)){
		fioMkdir("mc0:/SYS-CONF");
	}else if (!strncmp (IPconfig_path,"mc1:/SYS-CONF",13)){
		fioMkdir("mc1:/SYS-CONF");
	}

	if (IPconfig_path[2] == '?')
		IPconfig_path[2] = '0';
	
	int fd=fioOpen(IPconfig_path, O_WRONLY | O_CREAT);
	if (fd<0) {
		if (IPconfig_path[2] == '0')
			IPconfig_path[2] = '1';
		else
			IPconfig_path[2] = '0';
		fd=fioOpen(IPconfig_path, O_WRONLY | O_CREAT);
		if (fd<0) {
			//DEBUG: printf("No config. Exiting...\n");
			IPconfig_path[2] = '?';
			return;
		}
	}
	
	sprintf(ipconfig, "%d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d\r\n", ps2_ip[0], ps2_ip[1], ps2_ip[2], ps2_ip[3],
															ps2_netmask[0], ps2_netmask[1], ps2_netmask[2], ps2_netmask[3],
															ps2_gateway[0], ps2_gateway[1], ps2_gateway[2], ps2_gateway[3]);

	fioWrite(fd, ipconfig, strlen(ipconfig)+1);
	
	fioClose(fd);
	
	return;
}


int readConfig(struct TConfigSet* config, const char *fname) {
	int fd=fioOpen(fname, O_RDONLY);
	
	if (fd<0) {
		//DEBUG: printf("No config. Exiting...\n");
		return 0;
	}
	
	char line[2048];
	unsigned int lineno = 0;
	
	while (readline(fd, line, 2048)) {
		lineno++;
		
		char key[255], val[255];
		memset(key, 0, 255);
		memset(val, 0, 255);
		
		if (splitAssignment(line, key, val)) {
			// insert config value
			setConfigStr(config, key, val);
		} else {
			// DEBUG: ignoring...
			// printf("Malformed config file '%s' line %d: '%s'\n", fname, lineno, line);
		}
	}
	
	fioClose(fd);
	return 1;
}

int writeConfig(struct TConfigSet* config, const char *fname) {

	if (!strncmp (fname,"mc0:/SYS-CONF",13)){
		fioMkdir("mc0:/SYS-CONF");
	}else if (!strncmp (fname,"mc1:/SYS-CONF",13)){
		fioMkdir("mc1:/SYS-CONF");
	}

	int fd=fioOpen(fname, O_WRONLY | O_CREAT | O_TRUNC);
	
	if (fd<0)
		return 0;
	
	char line[2048];
	struct TConfigValue* cur = config->head;
	
	while (cur) {
		line[0] = '\0';
		
		configValueToText(cur, line, 2048);
		
		int len = strlen(line);

		fioWrite(fd, line, len);
		fioWrite(fd, "\r\n", 2);

		// and advance
		cur = cur->next;
	}
	
	fioClose(fd);
	return 1;
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

void ListDir(char* directory) {
	int ret=-1;
	int n_dir=0;
	fio_dirent_t record;
	int i=0;
   
	// free the prev. list
	i = 0;
	while (theme_dir[i] && i < 256) {
		free(theme_dir[i]);
		theme_dir[i] = NULL;
		++i;
	}

	theme_dir[0]=malloc(7);
	sprintf(theme_dir[0],"<none>");
	n_dir=1;
	
	if ((ret = fioDopen(directory)) < 0) {
		//printf("Error opening dir\n");
		return;
	}

	while (fioDread(ret, &record) > 0) {
		/*Keep track of number of files */
		if (FIO_SO_ISDIR(record.stat.mode)) {
			if(record.name[0]!='.' && (record.name[0]!='.' && record.name[1]!='.')){
				//printf("%s\n",record.name);
				theme_dir[n_dir]=malloc(sizeof(record.name));
				sprintf(theme_dir[n_dir],"%s",record.name);
				n_dir++;
			}
		}
		
		if (n_dir >= 254)
			break; // ensure padding is there!
	}
	
	
	max_theme_dir = n_dir;
	theme_dir[max_theme_dir] = NULL;
	
	fioDclose(ret);
} 
