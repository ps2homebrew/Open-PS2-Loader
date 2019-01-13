#ifndef __APP_SUPPORT_H
#define __APP_SUPPORT_H

#include "include/iosupport.h"

#define APP_MODE_UPDATE_DELAY 240

#define APP_TITLE_MAX 128
#define APP_PATH_MAX 128
#define APP_BOOT_MAX 64

#define APP_CONFIG_TITLE "title"
#define APP_CONFIG_BOOT "boot"

#define APP_TITLE_CONFIG_FILE "title.cfg"

typedef struct
{
    char title[APP_TITLE_MAX + 1];
    char path[APP_PATH_MAX + 1];
    char boot[APP_BOOT_MAX + 1];
    u8 legacy;
} app_info_t;

void appInit();
item_list_t *appGetObject(int initOnly);

#endif
