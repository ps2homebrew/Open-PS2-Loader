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
#include "src/unzip/unzip.h"
#include <libpwroff.h>
#include <fileXio_rpc.h>

#include <debug.h>

#define USBLD_VERSION "0.6"

// count of items per page (for page up/down navigation)
#define STATIC_PAGE_SIZE 10

char USB_prefix[8];
char theme_prefix[32];

typedef
struct TGame
{
	char           Name[33];
	char           Image[16];
	unsigned char  parts;
	unsigned char  media;
} TGame;

TGame *usbGameList;
TGame *ethGameList;

int usb_max_games;
int eth_max_games;

// Configuration handling
// single config value (linked list item)
struct TConfigValue {
        char key[32];
        char val[255];

        struct TConfigValue *next;
};

struct TConfigSet {
	struct TConfigValue *head;
	struct TConfigValue *tail;
};

// global config
struct TConfigSet gConfig;
// global config file path
const char *gConfigPath;

char *infotxt;

float waveframe;
int frame;
int h_anim;
int v_anim;
int direc;
int max_settings;
int gLanguageID;

int background_image;

// Bool - either true or false (1/0)
int dynamic_menu;

// 0,1,2 scrolling speed
unsigned int scroll_speed;

// Exit mode
int exit_mode;

// Disable Debug Colors
int disable_debug;

// Default device
int default_device;

// IP config

int ps2_ip[4];
int ps2_netmask[4];
int ps2_gateway[4];
int pc_ip[4];
int gPCPort;

char gPCShareName[32];

// describes what is happening in the network startup thread (>0 means loading, <0 means error)...
int gNetworkStartup;
// config parameter ("net_auto") - nonzero means net will autoload
int gNetAutostart;
// true if the ip config should be saved as well
int gIPConfigChanged;

/// Indicates the hdd module loading sequence
int gHddStartup;
/// true if the usage of hdd was selected in options
int gUseHdd;
/// true if the hdd should start automatically
int gHddAutostart;
/// true if dev9 irx loaded already
int gDev9_loaded;
/// Sort the game lists automatically
int gAutosort;

#define COMPAT_MODE_1 		0x01
#define COMPAT_MODE_2 		0x02
#define COMPAT_MODE_3 		0x04
#define COMPAT_MODE_4 		0x08
#define COMPAT_MODE_5 		0x10
#define COMPAT_MODE_6 		0x20

// Theme config

char theme[32];
unsigned char bg_color[3];
unsigned char text_color[3];
unsigned char default_bg_color[3];
unsigned char default_text_color[3];
char *theme_dir[255];
int max_theme_dir;
int theme_zipped;
unzFile zipfile;

void LoadGameList();
void netLoadDisplay();

//HDL

#define PS2PART_IDMAX			32
#define HDL_GAME_NAME_MAX  		64

typedef struct
{
	char 	partition_name[PS2PART_IDMAX + 1];
	char	name[HDL_GAME_NAME_MAX + 1];
	char	startup[8 + 1 + 3 + 1];
	u8 	hdl_compat_flags;
	u8 	ops2l_compat_flags;
	u8	dma_type;
	u8	dma_mode;
	u32 	layer_break;
	int 	disctype;
  	u32 	start_sector;
  	u32 	total_size_in_kb;
} hdl_game_info_t;

typedef struct
{
	u32 		count;
	hdl_game_info_t *games;
	u32 		total_chunks;
  	u32 		free_chunks;
} hdl_games_list_t;

int hddCheck(void);
u32 hddGetTotalSectors(void);
int hddIs48bit(void);
int hddSetTransferMode(int type, int mode);
int hddGetHDLGamelist(hdl_games_list_t **game_list);
int hddSetHDLGameInfo(int game_index, hdl_game_info_t *ginfo);

#define MDMA_MODE		0x20
#define UDMA_MODE		0x40

#define HDL_COMPAT_MODE_1 	0x01
#define HDL_COMPAT_MODE_2 	0x02
#define HDL_COMPAT_MODE_3 	0x04
#define HDL_COMPAT_MODE_4 	0x08
#define HDL_COMPAT_MODE_5 	0x10
#define HDL_COMPAT_MODE_6 	0x20
#define HDL_COMPAT_MODE_7 	0x40
#define HDL_COMPAT_MODE_8 	0x80

//SYSTEM
int usbdelay;
int ethdelay;

void Reset(void);
void delay(int count);
void LoadNetworkModules(void);
void LoadHddModules(void);
void LoadUsbModules(void);
int getDiscID(void *discID);
void LaunchGame(TGame *game, int mode, int compatmask, void* gameid);
void LaunchHDDGame(hdl_game_info_t *game, void* gameid);
int ExecElf(char *path, int argc, char **argv);
void SendIrxKernelRAM(int mode);
unsigned int USBA_crc32(char *string);

#define USB_MODE	0
#define ETH_MODE	1
#define HDD_MODE	2

// main.c config handling
int storeConfig();
int restoreConfig();

//CONFIG
int fromHex(char digit);
char toHex(int digit);

// returns nonzero for valid config key name
int configKeyValidate(const char* key);
int setConfigStr(struct TConfigSet* config, const char* key, const char* value);
int getConfigStr(struct TConfigSet* config, const char* key, char** value);
int setConfigInt(struct TConfigSet* config, const char* key, const int value);
int getConfigInt(struct TConfigSet* config, char* key, int* value);
int setConfigColor(struct TConfigSet* config, const char* key, unsigned char* color);
int getConfigColor(struct TConfigSet* config, const char* key, unsigned char* color);
int configRemoveKey(struct TConfigSet* config, const char* key);

void readIPConfig();
void writeIPConfig();

int readConfig(struct TConfigSet* config, const char *fname);
int writeConfig(struct TConfigSet* config, const char *fname);
void clearConfig(struct TConfigSet* config);

void ListDir(char* directory);
