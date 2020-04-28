#ifndef __THEMES_H
#define __THEMES_H

#include "include/textures.h"
#include "include/texcache.h"
#include "include/menusys.h"

#define THM_MAX_FILES 64
#define THM_MAX_FONTS 16

typedef struct
{
    // optional, only for overlays
    int upperLeft_x;
    int upperLeft_y;
    int upperRight_x;
    int upperRight_y;
    int lowerLeft_x;
    int lowerLeft_y;
    int lowerRight_x;
    int lowerRight_y;

    // basic texture information
    char *name;
    GSTEXTURE source;
} image_texture_t;

typedef struct
{
    // Attributes for: AttributeImage
    int currentUid;
    u32 currentConfigId;
    char *currentValue;

    // Attributes  for: AttributeImage & GameImage
    image_cache_t *cache;
    int cacheLinked;

    // Attributes for: AttributeImage & GameImage & StaticImage
    image_texture_t *defaultTexture;
    int defaultTextureLinked;

    image_texture_t *overlayTexture;
    int overlayTextureLinked;
} mutable_image_t;

typedef struct
{
    // Attributes for: AttributeText & StaticText
    char *value;
    int sizingMode;

    // Attributes for: AttributeText
    char *alias;
    int displayMode;

    u32 currentConfigId;
    char *currentValue;
} mutable_text_t;

typedef struct
{
    int displayedItems;

    const char *decorator;
    mutable_image_t *decoratorImage;
} items_list_t;

typedef struct theme_element
{
    int type;
    int posX;
    int posY;
    short aligned;
    int width;
    int height;
    short scaled;
    u64 color;
    int font;

    void *extended;

    void (*drawElem)(struct menu_list *menu, struct submenu_list *item, config_set_t *config, struct theme_element *elem);
    void (*endElem)(struct theme_element *elem);

    struct theme_element *next;
} theme_element_t;

typedef struct
{
    theme_element_t *first;
    theme_element_t *last;
} theme_elems_t;

typedef struct
{
    char *filePath;
    char *name;
} theme_file_t;

typedef struct theme
{
    int useDefault;
    int usedHeight;

    unsigned char bgColor[3];
    u64 textColor;
    u64 uiTextColor;
    u64 selTextColor;

    theme_elems_t mainElems;
    theme_elems_t infoElems;

    int gameCacheCount;

    theme_element_t *itemsList;
    theme_element_t *loadingIcon;
    int loadingIconCount;

    GSTEXTURE textures[TEXTURES_COUNT];
    int fonts[THM_MAX_FONTS]; //!< Storage of font handles for removal once not needed
} theme_t;

extern theme_t *gTheme;

void thmInit(void);
void thmReinit(const char *path);
void thmReloadScreenExtents(void);
int thmAddElements(char *path, const char *separator, int forceRefresh);
const char *thmGetValue(void);
GSTEXTURE *thmGetTexture(unsigned int id);
void thmEnd(void);

// Indices are shifted in GUI, as we add the internal default theme at 0
int thmSetGuiValue(int themeID, int reload);
int thmGetGuiValue(void);
int thmFindGuiID(const char *theme);
const char **thmGetGuiList(void);
char *thmGetFilePath(int themeID);

#endif
