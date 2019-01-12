/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/util.h"
#include "include/ioman.h"
#include <string.h>

static u32 currentUID = 0;
static config_set_t configFiles[CONFIG_INDEX_COUNT];
static char legacyNetConfigPath[256] = "mc?:SYS-CONF/IPCONFIG.DAT";
static const char *configFilenames[CONFIG_INDEX_COUNT] = {
    "conf_opl.cfg",
    "conf_last.cfg",
    "conf_apps.cfg",
    "conf_network.cfg",
};

static int strToColor(const char *string, unsigned char *color)
{
    int cnt = 0, n = 0;
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;

    if (!string || !*string)
        return 0;
    if (string[0] != '#')
        return 0;

    string++;

    while (*string) {
        int fh = fromHex(*string);
        if (fh >= 0) {
            color[n] = color[n] * 16 + fh;
        } else {
            break;
        }

        // Two characters per color
        if (cnt == 1) {
            cnt = 0;
            n++;
        } else {
            cnt++;
        }

        string++;
    }

    return 1;
}

/// true if given a whitespace character
int isWS(char c)
{
    return c == ' ' || c == '\t';
}

static int splitAssignment(char *line, char *key, size_t keymax, char *val, size_t valmax)
{
    // skip whitespace
    for (; isWS(*line); ++line)
        ;

    // find "=".
    // If found, the text before is key, after is val.
    // Otherwise malformed string is encountered

    char *eqpos = strchr(line, '=');

    if (eqpos) {
        // copy the name and the value
        size_t keylen = min(keymax, eqpos - line);

        strncpy(key, line, keylen);

        eqpos++;

        size_t vallen = min(valmax, strlen(line) - (eqpos - line));
        strncpy(val, eqpos, vallen);
    }

    return (int)eqpos;
}

static int parsePrefix(char *line, char *prefix)
{
    // find "=".
    // If found, the text before is key, after is val.
    // Otherwise malformed string is encountered
    char *colpos = strchr(line, ':');

    if (colpos && colpos != line) {
        // copy the name and the value
        strncpy(prefix, line, colpos - line);

        return 1;
    } else {
        return 0;
    }
}

static int configKeyValidate(const char *key)
{
    if (strlen(key) == 0)
        return 0;

    return !strchr(key, '=');
}

static struct config_value_t *allocConfigItem(const char *key, const char *val)
{
    struct config_value_t *it = (struct config_value_t *)malloc(sizeof(struct config_value_t));
    strncpy(it->key, key, sizeof(it->key));
    it->key[sizeof(it->key) - 1] = '\0';
    strncpy(it->val, val, sizeof(it->val));
    it->val[sizeof(it->val) - 1] = '\0';
    it->next = NULL;

    return it;
}

/// Low level key addition. Does not check for uniqueness.
static void addConfigValue(config_set_t *configSet, const char *key, const char *val)
{
    if (!configSet->tail) {
        configSet->head = allocConfigItem(key, val);
        configSet->tail = configSet->head;
    } else {
        configSet->tail->next = allocConfigItem(key, val);
        configSet->tail = configSet->tail->next;
    }
}

static struct config_value_t *getConfigItemForName(config_set_t *configSet, const char *name)
{
    struct config_value_t *val = configSet->head;

    while (val) {
        if (strncmp(val->key, name, sizeof(val->key)) == 0)
            break;

        val = val->next;
    }

    return val;
}

void configInit(char *prefix)
{
    char path[256];
    int i;

    if (prefix)
        snprintf(legacyNetConfigPath, sizeof(legacyNetConfigPath), "%s/IPCONFIG.DAT", prefix);
    else
        prefix = gBaseMCDir;

    for (i = 0; i < CONFIG_INDEX_COUNT; i++) {
        snprintf(path, sizeof(path), "%s/%s", prefix, configFilenames[i]);
        configAlloc(1 << i, &configFiles[i], path);
    }
}

void configSetMove(char *prefix)
{
    char path[256];
    int i;

    if (prefix)
        snprintf(legacyNetConfigPath, sizeof(legacyNetConfigPath), "%s/IPCONFIG.DAT", prefix);
    else
        prefix = gBaseMCDir;

    for (i = 0; i < CONFIG_INDEX_COUNT; i++) {
        snprintf(path, sizeof(path), "%s/%s", prefix, configFilenames[i]);
        configMove(&configFiles[i], path);
    }
}

void configEnd()
{
    int index = 0;
    while (index < CONFIG_INDEX_COUNT) {
        config_set_t *configSet = &configFiles[index];

        configClear(configSet);
        free(configSet->filename);
        configSet->filename = NULL;
        index++;
    }
}

config_set_t *configAlloc(int type, config_set_t *configSet, char *fileName)
{
    if (!configSet)
        configSet = (config_set_t *)malloc(sizeof(config_set_t));

    configSet->uid = ++currentUID;
    configSet->type = type;
    configSet->head = NULL;
    configSet->tail = NULL;
    if (fileName) {
        int length = strlen(fileName) + 1;
        configSet->filename = (char *)malloc(length * sizeof(char));
        memcpy(configSet->filename, fileName, length);
    } else
        configSet->filename = NULL;
    configSet->modified = 0;
    return configSet;
}

void configMove(config_set_t *configSet, const char *fileName)
{
    int length = strlen(fileName) + 1;
    configSet->filename = realloc(configSet->filename, length);
    memcpy(configSet->filename, fileName, length);
}

void configFree(config_set_t *configSet)
{
    configClear(configSet);
    free(configSet->filename);
    free(configSet);
}

config_set_t *configGetByType(int type)
{
    int index = 0;
    while (index < CONFIG_INDEX_COUNT) {
        config_set_t *configSet = &configFiles[index];

        if (configSet->type == type)
            return configSet;
        index++;
    }
    return NULL;
}

int configSetStr(config_set_t *configSet, const char *key, const char *value)
{
    if (!configKeyValidate(key))
        return 0;

    struct config_value_t *it = getConfigItemForName(configSet, key);

    if (it) {
        if (strncmp(it->val, value, sizeof(it->val)) != 0) {
            strncpy(it->val, value, sizeof(it->val));
            it->val[sizeof(it->val) - 1] = '\0';
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
int configGetStr(config_set_t *configSet, const char *key, const char **value)
{
    if (!configKeyValidate(key))
        return 0;

    struct config_value_t *it = getConfigItemForName(configSet, key);

    if (it) {
        *value = it->val;
        return 1;
    } else
        return 0;
}

int configGetStrCopy(config_set_t *configSet, const char *key, char *value, int length)
{
    const char *valref = NULL;
    if (configGetStr(configSet, key, &valref)) {
        strncpy(value, valref, length);
        value[length - 1] = '\0';
        return 1;
    } else {
        value[0] = '\0';
        return 0;
    }
}

int configSetInt(config_set_t *configSet, const char *key, const int value)
{
    char tmp[12];
    snprintf(tmp, sizeof(tmp), "%d", value);
    return configSetStr(configSet, key, tmp);
}

int configGetInt(config_set_t *configSet, const char *key, int *value)
{
    const char *valref = NULL;
    if (configGetStr(configSet, key, &valref)) {
        *value = atoi(valref);
        return 1;
    } else {
        return 0;
    }
}

int configSetColor(config_set_t *configSet, const char *key, unsigned char *color)
{
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "#%02X%02X%02X", color[0], color[1], color[2]);
    return configSetStr(configSet, key, tmp);
}

int configGetColor(config_set_t *configSet, const char *key, unsigned char *color)
{
    const char *valref = NULL;
    if (configGetStr(configSet, key, &valref)) {
        strToColor(valref, color);
        return 1;
    } else {
        return 0;
    }
}

int configRemoveKey(config_set_t *configSet, const char *key)
{
    if (!configKeyValidate(key))
        return 0;

    struct config_value_t *val = configSet->head;
    struct config_value_t *prev = NULL;

    while (val) {
        if (strncmp(val->key, key, sizeof(val->key)) == 0) {
            if (key[0] != '#')
                configSet->modified = 1;

            if (val == configSet->tail)
                configSet->tail = prev;

            val = val->next;
            if (prev) {
                free(prev->next);
                prev->next = val;
            } else {
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

void configMerge(config_set_t *dest, const config_set_t *source)
{
    struct config_value_t *val;

    for (val = source->head; val != NULL; val = val->next) {
        configSetStr(dest, val->key, val->val);
    }
}

static int configReadLegacyIP(void)
{
    config_set_t *configSet;
    char temp[16];

    int fd = openFile(legacyNetConfigPath, O_RDONLY);
    if (fd >= 0) {
        char ipconfig[256];
        int size = getFileSize(fd);
        fileXioRead(fd, &ipconfig, size);
        fileXioClose(fd);

        sscanf(ipconfig, "%d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d", &ps2_ip[0], &ps2_ip[1], &ps2_ip[2], &ps2_ip[3],
               &ps2_netmask[0], &ps2_netmask[1], &ps2_netmask[2], &ps2_netmask[3],
               &ps2_gateway[0], &ps2_gateway[1], &ps2_gateway[2], &ps2_gateway[3]);

        configSet = &configFiles[CONFIG_INDEX_NETWORK];

        snprintf(temp, sizeof(temp), "%d.%d.%d.%d", ps2_ip[0], ps2_ip[1], ps2_ip[2], ps2_ip[3]);
        configSetStr(configSet, CONFIG_NET_PS2_IP, temp);
        snprintf(temp, sizeof(temp), "%d.%d.%d.%d", ps2_netmask[0], ps2_netmask[1], ps2_netmask[2], ps2_netmask[3]);
        configSetStr(configSet, CONFIG_NET_PS2_NETM, temp);
        snprintf(temp, sizeof(temp), "%d.%d.%d.%d", ps2_gateway[0], ps2_gateway[1], ps2_gateway[2], ps2_gateway[3]);
        configSetStr(configSet, CONFIG_NET_PS2_GATEW, temp);
        //The legacy format has no setting for the DNS server, so duplicate the gateway address.
        configSetStr(configSet, CONFIG_NET_PS2_DNS, temp);

        return 1;
    }

    return 0;
}

// dst has to have 5 bytes space
void configGetDiscIDBinary(config_set_t *configSet, void *dst)
{
    memset(dst, 0, 5);

    const char *gid = NULL;
    if (configGetStr(configSet, CONFIG_ITEM_DNAS, &gid)) {
        // convert from hex to binary
        char *cdst = dst;
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

static int configReadFileBuffer(file_buffer_t *fileBuffer, config_set_t *configSet)
{
    char *line;
    unsigned int lineno = 0;

    char prefix[CONFIG_KEY_NAME_LEN];
    memset(prefix, 0, sizeof(prefix));

    while (readFileBuffer(fileBuffer, &line)) {
        lineno++;

        char key[CONFIG_KEY_NAME_LEN], val[CONFIG_KEY_VALUE_LEN];
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
                char composedKey[CONFIG_KEY_NAME_LEN];

                snprintf(composedKey, sizeof(composedKey), "%s_%s", prefix, key);
                configSetStr(configSet, composedKey, val);
            } else {
                configSetStr(configSet, key, val);
            }
        } else if (parsePrefix(line, prefix)) {
            // prefix is set, that's about it
        } else {
            LOG("CONFIG Malformed file '%s' line %d: '%s'\n", configSet->filename, lineno, line);
        }
    }
    configSet->modified = 0;
    return 1;
}

int configReadBuffer(config_set_t *configSet, const void *buffer, int size)
{
    int ret;
    file_buffer_t *fileBuffer = openFileBufferBuffer(0, buffer, size);
    if (!fileBuffer) {
        configSet->modified = 0;
        return 0;
    }

    ret = configReadFileBuffer(fileBuffer, configSet);

    closeFileBuffer(fileBuffer);
    return ret;
}

int configRead(config_set_t *configSet)
{
    int ret;
    file_buffer_t *fileBuffer = openFileBuffer(configSet->filename, O_RDONLY, 0, 4096);
    if (!fileBuffer) {
        LOG("CONFIG No file %s.\n", configSet->filename);
        configSet->modified = 0;
        return 0;
    }

    ret = configReadFileBuffer(fileBuffer, configSet);

    closeFileBuffer(fileBuffer);
    return ret;
}

int configWrite(config_set_t *configSet)
{
    if (configSet->modified) {
        file_buffer_t *fileBuffer = openFileBuffer(configSet->filename, O_WRONLY | O_CREAT | O_TRUNC, 0, 4096);
        if (fileBuffer) {
            char line[512];

            struct config_value_t *cur = configSet->head;
            while (cur) {
                if ((cur->key[0] != '\0') && (cur->key[0] != '#')) {
                    snprintf(line, sizeof(line), "%s=%s\r\n", cur->key, cur->val); // add windows CR+LF (0x0D 0x0A)
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

int configGetStat(config_set_t *configSet, iox_stat_t *stat)
{
    return (fileXioGetStat(configSet->filename, stat) >= 0 ? 1 : 0);
}

void configClear(config_set_t *configSet)
{
    while (configSet->head) {
        struct config_value_t *cur = configSet->head;
        configSet->head = cur->next;

        free(cur);
    }

    configSet->head = NULL;
    configSet->tail = NULL;
    configSet->modified = 1;
}

int configReadMulti(int types)
{
    int result = 0, index;

    for (index = 0; index < CONFIG_INDEX_COUNT; index++) {
        config_set_t *configSet = &configFiles[index];

        if (configSet->type & types) {
            configClear(configSet);
            if (configRead(configSet))
                result |= configSet->type;
        }
    }

    //If the network configuration is to be loaded and one cannot be loaded, attempt to load from the legacy network config file.
    if ((types & CONFIG_NETWORK) && !(result & CONFIG_NETWORK))
        if (configReadLegacyIP())
            result |= CONFIG_NETWORK;

    return result;
}

int configWriteMulti(int types)
{
    int result = 0, index;

    for (index = 0; index < CONFIG_INDEX_COUNT; index++) {
        config_set_t *configSet = &configFiles[index];

        if (configSet->type & types)
            result += configWrite(configSet);
    }

    return result;
}

void configGetVMC(config_set_t *configSet, char *vmc, int length, int slot)
{
    char gkey[CONFIG_KEY_NAME_LEN];
    snprintf(gkey, sizeof(gkey), "%s_%d", CONFIG_ITEM_VMC, slot);
    configGetStrCopy(configSet, gkey, vmc, length);
}

void configSetVMC(config_set_t *configSet, const char *vmc, int slot)
{
    char gkey[CONFIG_KEY_NAME_LEN];
    if (vmc[0] == '\0') {
        configRemoveVMC(configSet, slot);
        return;
    }
    snprintf(gkey, sizeof(gkey), "%s_%d", CONFIG_ITEM_VMC, slot);
    configSetStr(configSet, gkey, vmc);
}

void configRemoveVMC(config_set_t *configSet, int slot)
{
    char gkey[CONFIG_KEY_NAME_LEN];
    snprintf(gkey, sizeof(gkey), "%s_%d", CONFIG_ITEM_VMC, slot);
    configRemoveKey(configSet, gkey);
}

