#ifndef __USB_SUPPORT_H
#define __USB_SUPPORT_H

#include "include/iosupport.h"

#define USB_MODE_UPDATE_DELAY	MENU_UPD_DELAY_GENREFRESH

#ifdef VMC
#include "include/mcemu.h"
#include "mass_common.h"
#include "fat.h"

typedef struct fat_file_info{
	u32 cluster;
	u32 size;
} fat_info;

typedef struct
{
	int        active;       /* Activation flag */
	fat_info   file;
	fat_dir    fatDir;
	int        flags;        /* Card flag */
	vmc_spec_t specs;        /* Card specifications */
} usb_vmc_infos_t;
#endif

void usbInit();
item_list_t* usbGetObject(int initOnly);
int usbFindPartition(char *target, char *name);
void usbLoadModules(void);

#endif
