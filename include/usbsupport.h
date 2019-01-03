#ifndef __USB_SUPPORT_H
#define __USB_SUPPORT_H

#include "include/iosupport.h"

#define USB_MODE_UPDATE_DELAY MENU_UPD_DELAY_GENREFRESH

#include "include/mcemu.h"

typedef struct
{
    int active;       /* Activation flag */
    u32 start_sector; /* Start sector of vmc file */
    int flags;        /* Card flag */
    vmc_spec_t specs; /* Card specifications */
} usb_vmc_infos_t;

#define MAX_USB_DEVICES 5

void usbInit();
item_list_t *usbGetObject(int initOnly);
int usbFindPartition(char *target, const char *name, int write);
void usbLoadModules(void);

#endif
