#ifndef __BDM_SUPPORT_H
#define __BDM_SUPPORT_H

#include "include/iosupport.h"

#define BDM_MODE_UPDATE_DELAY MENU_UPD_DELAY_GENREFRESH

#include "include/mcemu.h"

typedef struct
{
    int active;       /* Activation flag */
    u32 start_sector; /* Start sector of vmc file */
    int flags;        /* Card flag */
    vmc_spec_t specs; /* Card specifications */
} bdm_vmc_infos_t;

#define MAX_BDM_DEVICES 5

void bdmInit();
item_list_t *bdmGetObject(int initOnly);
int bdmFindPartition(char *target, const char *name, int write);
void bdmLoadModules(void);
void bdmLaunchGame(int id, config_set_t *configSet);
void bdmSetPrefix(void);

#endif
