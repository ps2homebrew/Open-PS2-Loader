#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_ALL		0xFF

#define CONFIG_OPL		0x01
#define CONFIG_COMPAT	0x02
#define CONFIG_DNAS		0x04
#define CONFIG_VMC		0x08
#define CONFIG_LAST		0x10
#define CONFIG_APPS		0x20
#define CONFIG_VMODE	0x40

#define CONFIG_FILE_NUM 7

struct config_value_t {
        char key[32];
        char val[255];

        struct config_value_t *next;
};

typedef struct {
	int type;
	struct config_value_t *head;
	struct config_value_t *tail;
	char *filename;
	int modified;
} config_set_t;

void configInit(char *prefix);
void configEnd();
config_set_t *configAlloc(int type, config_set_t *configSet, char *fileName);
void configFree(config_set_t *configSet);
config_set_t *configGetByType(int type);
int configSetStr(config_set_t* configSet, const char* key, const char* value);
int configGetStr(config_set_t* configSet, const char* key, char** value);
int configSetInt(config_set_t* configSet, const char* key, const int value);
int configGetInt(config_set_t* configSet, char* key, int* value);
int configSetColor(config_set_t* configSet, const char* key, unsigned char* color);
int configGetColor(config_set_t* configSet, const char* key, unsigned char* color);
int configRemoveKey(config_set_t* configSet, const char* key);

void configReadIP();
void configWriteIP();

void configGetDiscID(char* startup, char* discID);
void configSetDiscID(char* startup, const char *discID);
void configRemoveDiscID(char* startup);
void configGetDiscIDBinary(char* startup, void* dst);

int configGetCompatibility(char* startup, int mode, int *dmaMode);
void configSetCompatibility(char* startup, int mode, int compatMode, int dmaMode);

int configRead(config_set_t* configSet);
int configReadMulti(int types);
int configWrite(config_set_t* configSet);
int configWriteMulti(int types);
void configClear(config_set_t* configSet);

#ifdef VMC
void configGetVMC(char* startup, char* vmc, int mode, int slot);
void configSetVMC(char* startup, const char* vmc, int mode, int slot);
void configRemoveVMC(char *startup, int mode, int slot);
#endif

#endif
