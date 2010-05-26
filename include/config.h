#ifndef __CONFIG_H
#define __CONFIG_H

// single config value (linked list item)
struct TConfigValue {
        char key[32];
        char val[255];

        struct TConfigValue *next;
};

struct TConfigSet {
	struct TConfigValue *head;
	struct TConfigValue *tail;
};

// returns nonzero for valid config key name
int configKeyValidate(const char* key);
int setConfigStr(struct TConfigSet* config, const char* key, const char* value);
int getConfigStr(struct TConfigSet* config, const char* key, char** value);
int setConfigInt(struct TConfigSet* config, const char* key, const int value);
int getConfigInt(struct TConfigSet* config, char* key, int* value);
int setConfigColor(struct TConfigSet* config, const char* key, unsigned char* color);
int getConfigColor(struct TConfigSet* config, const char* key, unsigned char* color);
int configRemoveKey(struct TConfigSet* config, const char* key);

void readIPConfig();
void writeIPConfig();

char *getConfigDiscID(char* startup);
void setConfigDiscID(char* startup, const char *gid);
void removeConfigDiscID(char* startup);
void getConfigDiscIDBinary(char* startup, void* dst);

int readConfig(struct TConfigSet* config, char *fname);
int writeConfig(struct TConfigSet* config, char *fname);
void clearConfig(struct TConfigSet* config);

#endif
