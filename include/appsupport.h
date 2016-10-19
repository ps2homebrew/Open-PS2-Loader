#ifndef __APP_SUPPORT_H
#define __APP_SUPPORT_H

#include "include/iosupport.h"

#define APP_MODE_UPDATE_DELAY 240

void appInit();
item_list_t *appGetObject(int initOnly);

#endif
