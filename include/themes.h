#ifndef __THEMES_H
#define __THEMES_H

#include "include/textures.h"
#include "include/texcache.h"
#include "include/menusys.h"

#define THM_MAX_FILES 64
#define THM_MAX_FONTS 16

typedef struct {
	int upperLeft_x;
	int upperLeft_y;
	int upperRight_x;
	int upperRight_y;
	int lowerLeft_x;
	int lowerLeft_y;
	int lowerRight_x;
	int lowerRight_y;
	GSTEXTURE texture;
} image_overlay_t;

typedef struct {
	image_cache_t* cache;
	GSTEXTURE defaultTex; // an optional default texture when the cache fails
	int* attributeUid;
	int* attributeId;

	char* attribute;

	image_overlay_t* overlay; // an optional overlay
} attribute_image_t;

typedef struct {
	image_cache_t* cache;
	GSTEXTURE defaultTex; // an optional default texture when the cache fails

	char* pattern;

	image_overlay_t* overlay; // an optional overlay
} game_image_t;

typedef struct {
	GSTEXTURE texture;

	image_overlay_t* overlay; // an optional overlay
} static_image_t;

typedef struct {
	int displayedItems;

	char* decorator;
	game_image_t* decoratorImage;
} items_list_t;

typedef struct theme_element {
	int type;
	int posX;
	int posY;
	short aligned;
	int width;
	int height;
	u64 color;
	int font;

	void* extended;

	void (*drawElem)(struct menu_list* curMenu, struct submenu_list* curItem, struct theme_element* elem);
	void (*endElem)(struct theme_element* elem);

	struct theme_element* next;
} theme_element_t;

typedef struct {
	char* filePath;
	char* name;
} theme_file_t;

typedef struct theme {
	int useDefault;
	int usedHeight;

	unsigned char bgColor[3];
	u64 textColor;
	u64 uiTextColor;
	u64 selTextColor;

	theme_element_t* elems;
	int gameCacheCount;

	theme_element_t* itemsList;
	theme_element_t* loadingIcon;
	int loadingIconCount;

	GSTEXTURE textures[TEXTURES_COUNT];
	int fonts[THM_MAX_FONTS]; //!< Storage of font handles for removal once not needed
} theme_t;

theme_t* gTheme;

void thmInit();
void thmReloadScreenExtents();
void thmAddElements(char* path, char* separator);
char* thmGetValue();
GSTEXTURE* thmGetTexture(unsigned int id);
void thmEnd();

// Indices are shifted in GUI, as we add the internal default theme at 0
void thmSetGuiValue(int themeGuiId, int reload);
int thmGetGuiValue();
int thmFindGuiID(char* theme);
char **thmGetGuiList();

#endif
