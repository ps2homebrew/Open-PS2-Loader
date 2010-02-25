#ifndef __GFX_H
#define __GFX_H

/// Forward decls.
struct menu_item_t;
struct menu_list_t;

/// a single submenu item
struct submenu_item_t {
	/// Icon used for rendering of this item
	GSTEXTURE *icon;
	
	/// item description
	char *text;
	
	/// item description in localised form (used if value is not negative)
	int text_id;
	
	/// item id
	int id;
};

struct submenu_list_t {
	struct submenu_item_t item;
	
	struct submenu_list_t *prev, *next;
};

/// a single menu item. Linked list impl (for the ease of rendering)
struct menu_item_t {
	/// Icon used for rendering of this item
	GSTEXTURE *icon;
	
	/// item description
	char *text;
	
	/// item description in localised form (used if value is not negative)
	int text_id;
	
	void *userdata;
	
	/// submenu, selection and page start (only used in static mode)
	struct submenu_list_t *submenu, *current, *pagestart;
	
	/// execute a menu/submenu item (id is the submenu id as specified in AppendSubMenu)
	void (*execute)(struct menu_item_t *self, int id);
	
	void (*refresh)(struct menu_item_t *self, short forceRefresh);
	
	/// Alternate execution (can be used for item configuration for example)
	void (*altExecute)(struct menu_item_t *self, int id);
};

struct menu_list_t {
	struct menu_item_t *item;
	
	struct menu_list_t *prev, *next;
};

/// menu item list.
struct menu_list_t *menu;
struct menu_list_t *selected_item;

GSTEXTURE disc_icon;
GSTEXTURE games_icon;
GSTEXTURE config_icon;
GSTEXTURE exit_icon;
GSTEXTURE theme_icon;
GSTEXTURE language_icon;
GSTEXTURE apps_icon;
GSTEXTURE menu_icon;
GSTEXTURE scroll_icon;
GSTEXTURE usb_icon;
GSTEXTURE save_icon;
GSTEXTURE netconfig_icon;
GSTEXTURE network_icon;

u64 gColWhite;
u64 gColBlack;
u64 gColDarker;
u64 gColText;

GSGLOBAL *gsGlobal;
GSTEXTURE gFont;
GSFONT *gsFont;

// TODO: Should not be global
// current Z value (for layering)
int gZ;

void initGFX();
void initMenu();

void appendMenuItem(struct menu_item_t* item);

struct submenu_list_t* appendSubMenu(struct submenu_list_t** submenu, GSTEXTURE *icon, char *text, int id, int text_id);
void destroySubMenu(struct submenu_list_t** submenu);
void sortSubMenu(struct submenu_list_t** submenu);

void updateScrollSpeed();

void flip();
void msgBox(char* text);

// restores the on-line running settings from the global config var.
void gfxStoreConfig();
// stores the graphics related settings into the global config var.
void gfxRestoreConfig();

void drawWave(int y, int xoffset);
void drawBackground();
void setColor(int r, int g, int b);
void textColor(int r, int g, int b, int a);
void drawText(int x, int y, const char *texto, float scale, int centered);
void drawQuad(GSGLOBAL *gsGlobal, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int z, u64 color);
void drawLine(GSGLOBAL *gsGlobal,float x1, float y1,float x2, float y2, int z, u64 color);
void drawConfig();
void drawIPConfig();
void uploadTexture(GSTEXTURE* txt);
void aviso();
void openIntro();
void closeIntro();
void loadFont();
void updateFont();
int loadRAW(char *path, GSTEXTURE *Texture);

void findTheme();
void loadTheme(int themeid);
int loadBackground();
void loadIcons();
void updateIcons();

void drawIcon(GSTEXTURE *img, int x, int y, float scale);
void drawIcons();
void drawInfo();
// on-screen hint with optional action button icon
void drawHint(const char* hint, int key);
void drawSubMenu();

// keyboard input screen
int showKeyb(char* text, size_t maxLen);

// color input screen
int showColSel(unsigned char *r, unsigned char *g, unsigned char *b);

void menuNextH();
void menuPrevH();
void menuNextV();
void menuPrevV();
struct menu_item_t* menuGetCurrent();
void menuItemExecute();
void menuItemAltExecute();
// Sets the selected item if it is found in the menu list
void menuSetSelectedItem(struct menu_item_t* item);
void refreshSubMenu(short forceRefresh);

// Static render
void drawScreenStatic();

// swap static/normal render
void swapMenu();

// Dynamic/Static render setter
void setMenuDynamic(int dynamic);

// Renders everything
void drawScreen();

#endif
