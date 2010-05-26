#ifndef __ETH_SUPPORT_H
#define __ETH_SUPPORT_H

#include "include/iosupport.h"

void ethInit();
item_list_t* ethGetObject(int initOnly);

int ethSMBConnect(void);
int ethSMBDisconnect(void);

#endif
