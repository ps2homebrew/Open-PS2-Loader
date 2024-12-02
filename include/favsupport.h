#ifndef __FAV_SUPPORT_H
#define __FAV_SUPPORT_H

#include "include/iosupport.h"

#define FAV_MODE_UPDATE_DELAY 240

item_list_t *favGetObject(int initOnly);
unsigned char favGetFlags(item_list_t *itemList);

void writeFavouritesFile(submenu_item_t *items, int size);
submenu_item_t *readFavouritesFile(int *out_size);
void addFavouriteItem(const submenu_item_t *item);
void removeFavouriteByIdAndText(int id, const char *text);

#endif
