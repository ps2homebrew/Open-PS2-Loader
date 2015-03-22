#ifndef __IOSUPPORT_H
#define __IOSUPPORT_H

#include "include/config.h"

enum IO_MODES{
	USB_MODE	= 0,
	ETH_MODE,
	HDD_MODE,
	APP_MODE,

	MODE_COUNT
};

enum START_MODE{
	START_MODE_DISABLED	= 0,
	START_MODE_MANUAL,
	START_MODE_AUTO
};

#define NO_EXCEPTION		0x00
#define UNMOUNT_EXCEPTION	0x01

#define NO_COMPAT			0x00 // no compat support
#define COMPAT				0x01 // default compatibility flags
#define COMPAT_FULL			0x02 // default + DMA compat flags

#define COMPAT_MODE_1 		 0x01 // Accurate Reads
#define COMPAT_MODE_2 		 0x02 // Alternative data read method (Synchronous)
#define COMPAT_MODE_3 		 0x04 // Unhook Syscalls
#define COMPAT_MODE_4 		 0x08 // 0 PSS mode
#define COMPAT_MODE_5 		 0x10 // Disable DVD-DL
#define COMPAT_MODE_6 		 0x20 // Disable IGR
#define COMPAT_MODE_7 		 0x40 // Unused
#define COMPAT_MODE_8 		 0x80 // Hide dev9 module

#define COMPAT_MODE_COUNT		8

// minimal inactive frames for cover display, can be pretty low since it means no button is pressed :)
#define MENU_MIN_INACTIVE_FRAMES 8

#define MENU_UPD_DELAY_NOUPDATE		-1	//Menu won't be ever be updated, even if the user triggers a refresh.
#define MENU_UPD_DELAY_GENREFRESH	0	//Menu will be refreshed every MENU_GENERAL_UPDATE_DELAY frames, regardless of whether automatic refresh is enabled or not.

typedef struct
{
	int mode;

	char enabled;

	char haveCompatibilityMode;

	char allowManualStart;

	/// update in progress indicator
	char uip;

	/// max inactive frame delay
	int delay;

	/// Amount of frame to wait, before refreshing this menu's list. Setting an invalid value (<0) means no automatic refresh.
	/// 0 = General refresh, which means that it will be refreshed every MENU_GENERAL_UPDATE_DELAY frames, regardless of whether automatic refresh is enabled or not.
	int updateDelay;

	/// item description
	char* text;

	/// item description in localised form (used if value is not negative)
	int textId;

	void (*itemInit)(void);

	/** @return 1 if update is needed, 0 otherwise */
	int (*itemNeedsUpdate)(void);

	/** @return game count (0 on error) */
	int (*itemUpdate)(void);

	int (*itemGetCount)(void);

	void* (*itemGet)(int id);

	char* (*itemGetName)(int id);

	int (*itemGetNameLength)(int id);

	char* (*itemGetStartup)(int id);

	void (*itemDelete)(int id);

	void (*itemRename)(int id, char* newName);

	void (*itemLaunch)(int id, config_set_t* configSet);

	config_set_t* (*itemGetConfig)(int id);

	int (*itemGetImage)(char* folder, int isRelative, char* value, char* suffix, GSTEXTURE* resultTex, short psm);

	void (*itemCleanUp)(int exception);

#ifdef VMC
	int (*itemCheckVMC)(char* name, int createSize);
#endif

	int iconId;
} item_list_t;

#endif
