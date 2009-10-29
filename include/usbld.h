#include "tamtypes.h"
#include "kernel.h"
#include "sifrpc.h"
#include "fileio.h"
#include "iopcontrol.h"
#include "iopheap.h"
#include "string.h"
#include "loadfile.h"
#include "stdio.h"
#include "sbv_patches.h"
#include <libcdvd.h>
#include <libpad.h>
#include <libmc.h>
#include <debug.h>
#include "gsKit.h"
#include "dmaKit.h"
#include "gsToolkit.h"
#include "malloc.h"
#include "math.h"

#include <debug.h>

#define USBLD_VERSION "0.41"

typedef
struct TGame
{
	char           Name[32];
	char           Image[15];
	unsigned char  parts;
	unsigned char  media;
} TGame;

typedef
struct TGameList
{
	void *prev;
	TGame Game;
	void *next;
} TGameList;

TGameList *firstgame;
TGameList *actualgame;
TGameList *eth_firstgame;
TGameList *eth_actualgame;

/// Forward decls.
struct TMenuItem;
struct TMenuList;

/// a single submenu item
struct TSubMenuItem {
	/// Icon used for rendering of this item
	GSTEXTURE *icon;
	
	/// item description
	char *text;
	
	/// item description in localised form (used if value is not negative)
	int text_id;
	
	/// item id
	int id;
	
};

struct TSubMenuList {
	struct TSubMenuItem item;
	
	struct TSubMenuList *prev, *next;
};

/// a single menu item. Linked list impl (for the ease of rendering)
struct TMenuItem {
	/// Icon used for rendering of this item
	GSTEXTURE *icon;
	
	/// item description
	char *text;
	
	/// item description in localised form (used if value is not negative)
	int text_id;
	
	void *userdata;
	
	/// submenu
	struct TSubMenuList *submenu, *current;
	
	/// execute a menu/submenu item (id is the submenu id as specified in AppendSubMenu)
	void (*execute)(struct TMenuItem *self, int id);
	
	void (*nextItem)(struct TMenuItem *self);
	
	void (*prevItem)(struct TMenuItem *self);
	
	int (*resetOrder)(struct TMenuItem *self);
	
	void (*refresh)(struct TMenuItem *self);
};

struct TMenuList {
	struct TMenuItem *item;
	
	struct TMenuList *prev, *next;
};

/// menu item list.
struct TMenuList *menu;
struct TMenuList *selected_item;

// Configuration handling
// single config value (linked list item)
struct TConfigValue {
        char key[15];
        char val[255];

        struct TConfigValue *next;
};

struct TConfigSet {
	struct TConfigValue *head;
	struct TConfigValue *tail;
};

// global config
struct TConfigSet gConfig;

float waveframe;
int frame;
int h_anim;
int v_anim;
int direc;
int max_settings;
int max_games;
int eth_max_games;
int gLanguageID;

int background_image;

// Bool - either true or false (1/0)
int dynamic_menu;

// 0,1,2 scrolling speed
unsigned int scroll_speed;

// IP config

int ps2_ip[4];
int ps2_netmask[4];
int ps2_gateway[4];
int pc_ip[4];

// Theme config

char theme[32];
int bg_color[3];
int text_color[3];
char *theme_dir[255];
int max_theme_dir;

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

void LoadGameList();

//SYSTEM

int usbdelay;
int ethdelay;

void Reset();
void delay(int count);
void Start_LoadNetworkModules_Thread(void);
void LoadUSBD();
void LaunchGame(TGame *game, int mode);
void SendIrxKernelRAM(void);

#define USB_MODE	0
#define ETH_MODE	1


//PAD

#define KEY_LEFT 1
#define KEY_DOWN 2
#define KEY_RIGHT 3
#define KEY_UP 4
#define KEY_START 5
#define KEY_R3 6
#define KEY_L3 7
#define KEY_SELECT 8
#define KEY_SQUARE 9
#define KEY_CROSS 10
#define KEY_CIRCLE 11
#define KEY_TRIANGLE 12
#define KEY_R1 13
#define KEY_L1 14
#define KEY_R2 15
#define KEY_L2 16

int StartPad();
void ReadPad();
int GetKey(int num);
void SetButtonDelay(int button, int btndelay);
void UnloadPad();

//GFX

void InitGFX();
void InitMenu();

void AppendMenuItem(struct TMenuItem* item);

struct TSubMenuList* AppendSubMenu(struct TSubMenuList** submenu, GSTEXTURE *icon, char *text, int id, int text_id);
void DestroySubMenu(struct TSubMenuList** submenu);
void UpdateScrollSpeed();

void Flip();
void MsgBox(char* text);

int LoadConfig(char* fname, int clearFirst);
int SaveConfig(char* fname);

void DrawWave(int y, int xoffset);
void DrawBackground();
void SetColor(int r, int g, int b);
void TextColor(int r, int g, int b, int a);
void DrawText(int x, int y, char *texto, float scale, int centered);
void DrawConfig();
void DrawIPConfig();
void UploadTexture(GSTEXTURE* txt);
void Aviso();
void Intro();
void LoadFont();
void UpdateFont();
int LoadRAW(char *path, GSTEXTURE *Texture);
int LoadBackground();
void LoadIcons();
void UpdateIcons();
void DrawIcon(GSTEXTURE *img, int x, int y, float scale);
void DrawIcons();
void DrawInfo();
void DrawSubMenu();

void MenuNextH();
void MenuPrevH();
void MenuNextV();
void MenuPrevV();
void MenuItemExecute();
// Sets the selected item if it is found in the menu list
void MenuSetSelectedItem(struct TMenuItem* item);
void RefreshSubMenu();

// Static render
void DrawScreenStatic();

// swap static/normal render
void SwapMenu();

// Dynamic/Static render setter
void SetMenuDynamic(int dynamic);

// Renders everything
void DrawScreen();



//CONFIG
void setConfigStr(struct TConfigSet* config, const char* key, const char* value);
int getConfigStr(struct TConfigSet* config, const char* key, char** value);
void setConfigInt(struct TConfigSet* config, const char* key, const int value);
int getConfigInt(struct TConfigSet* config, char* key, int* value);
void setConfigColor(struct TConfigSet* config, const char* key, int* color);
int getConfigColor(struct TConfigSet* config, const char* key, int* color);

void readIPConfig();
void writeIPConfig();

int readConfig(struct TConfigSet* config, char *fname);
int writeConfig(struct TConfigSet* config, char *fname);
void clearConfig(struct TConfigSet* config);

void ListDir(char* directory);
