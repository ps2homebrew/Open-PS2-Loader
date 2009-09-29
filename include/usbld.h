#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <string.h>
#include <loadfile.h>
#include <stdio.h>
#include <sbv_patches.h>
#include <libcdvd.h>
#include <libmc.h>
#include <libpad.h>
#include <debug.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <malloc.h>
#include <math.h>

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

float waveframe;
int frame;
int h_anim;
int v_anim;
int direc;
int selected_h;
int selected_v;
int max_v;
int max_settings;
int max_games;

int background_image;

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

void LoadGameList();

//SYSTEM

int usbdelay;

void Reset();
void delay(int count);
void LaunchGame(TGame *game);
void SendIrxKernelRAM(void);


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
void Flip();
void MsgBox();
void DrawWave(int y, int xoffset);
void DrawBackground();
void SetColor(int r, int g, int b);
void TextColor(int r, int g, int b, int a);
void DrawText(int x, int y, char *texto, float scale, int centered);
void DrawConfig();
void Aviso();
void Intro();
void LoadFont();
int LoadBackground();
void LoadIcons();
void UpdateIcons();
void DrawIcon(int id, int x, int y, float scale);
void DrawIcons();
void DrawUSBGames();
void DrawHDDGames();
void DrawApps();
void DrawSettings();
void DrawInfo();

//CONFIG
int ReadConfig(char *archivo);
int SaveConfig(char *archivo);
void ListDir(char* directory);
