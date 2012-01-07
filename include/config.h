#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_ALL		0xFF

#define CONFIG_OPL		0x01
#define CONFIG_LAST		0x02
#define CONFIG_APPS		0x04
#define CONFIG_VMODE	0x08

#define CONFIG_FILE_NUM 4

#define CONFIG_ITEM_NAME		"#Name"
#define CONFIG_ITEM_LONGNAME	"#LongName"
#define CONFIG_ITEM_SIZE		"#Size"
#define CONFIG_ITEM_FORMAT		"#Format"
#define CONFIG_ITEM_MEDIA		"#Media"
#define CONFIG_ITEM_STARTUP		"#Startup"
#define CONFIG_ITEM_ALTSTARTUP	"$AltStartup"
#define CONFIG_ITEM_VMC			"$VMC"
#define CONFIG_ITEM_COMPAT		"$Compatibility"
#define CONFIG_ITEM_DMA			"$DMA"
#define CONFIG_ITEM_DNAS		"$DNAS"

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

void configGetDiscID(config_set_t* configSet, char* discID);
void configSetDiscID(config_set_t* configSet, const char *discID);
void configRemoveDiscID(config_set_t* configSet);
void configGetDiscIDBinary(config_set_t* configSet, void* dst);

int configGetCompatibility(config_set_t* configSet, int *dmaMode);
void configSetCompatibility(config_set_t* configSet, int compatMode, int dmaMode);

void configGetAltStartup(config_set_t* configSet, char* elfname);
void configSetAltStartup(config_set_t* configSet, char *elfname);
void configRemoveAltStartup(config_set_t* configSet);

int configRead(config_set_t* configSet);
int configReadMulti(int types);
int configWrite(config_set_t* configSet);
int configWriteMulti(int types);
void configClear(config_set_t* configSet);

#ifdef VMC
void configGetVMC(config_set_t* configSet, char* vmc, int slot);
void configSetVMC(config_set_t* configSet, const char* vmc, int slot);
void configRemoveVMC(config_set_t* configSet, int slot);
#endif

#endif
