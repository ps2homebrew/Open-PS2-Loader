#ifndef __THEMES_H
#define __THEMES_H

#include "include/textures.h"

#define MAX_THEMES_FILES 32
#define THM_MAX_FONTS 16

typedef struct {
	char* filePath;
	char* name;
} theme_file_t;

typedef struct {
	char* name;
	short enabled;
	int posX;
	int posY;
	short aligned;
	int width;
	int height;
	u64 color;
        int font;
} theme_element_t;

typedef struct {
	int useDefault;
	int usedHeight;
	int displayedItems;
	int itemsListIcons;

	unsigned char bgColor[3];
	u64 textColor;
	u64 uiTextColor;
	u64 selTextColor;

	theme_element_t menuIcon;
	theme_element_t menuText;
	theme_element_t itemsList;
	theme_element_t itemIcon;
	theme_element_t itemCover;
	theme_element_t itemText;
	theme_element_t hintText;
	theme_element_t loadingIcon;
	int busyIconsCount;

	int coverBlend_ulx, coverBlend_uly, coverBlend_urx, coverBlend_ury;
	int coverBlend_blx, coverBlend_bly, coverBlend_brx, coverBlend_bry;

	GSTEXTURE textures[TEXTURES_COUNT];
        int fonts[THM_MAX_FONTS]; //!< Storage of font handles for removal once not needed

	void (*drawBackground)();
	void (*drawAltBackground)();
} theme_t;

theme_t* gTheme;

void thmInit();
void thmAddElements(char* path, char* separator);
char* thmGetValue();
GSTEXTURE* thmGetTexture(unsigned int id);
void thmEnd();

// Indices are shifted in GUI, as we add the internal default theme at 0
void thmSetGuiValue(int themeGuiId);
int thmGetGuiValue();
int thmFindGuiID(char* theme);
char **thmGetGuiList();

#endif
