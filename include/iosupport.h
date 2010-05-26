#ifndef __IOSUPPORT_H
#define __IOSUPPORT_H

#define USB_MODE	0
#define ETH_MODE	1
#define HDD_MODE	2
#define APP_MODE	3

#define COMPAT_MODE_1 		0x01
#define COMPAT_MODE_2 		0x02
#define COMPAT_MODE_3 		0x04
#define COMPAT_MODE_4 		0x08
#define COMPAT_MODE_5 		0x10
#define COMPAT_MODE_6 		0x20

#define MDMA_MODE		0x20
#define UDMA_MODE		0x40

// minimal inactive frames for cover display, can be pretty low since it means no button is pressed :)
#define MENU_MIN_INACTIVE_FRAMES 3

typedef struct
{
	int mode;

	int enabled;

	/// update in progress indicator
	int uip;

	/// max inactive frame delay
	int delay;

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

	char* (*itemGetStartup)(int id);

	int (*itemGetCompatibility)(int id, int *dmaMode);

	void (*itemSetCompatibility)(int id, int compatMode, int dmaMode, short save);

	void (*itemLaunch)(int id);
	
	int (*itemGetArt)(char* name, GSTEXTURE* resultTex, const char* type, short psm);

	void (*itemCleanUp)(void);
	
	int iconId;
} item_list_t;

#endif
