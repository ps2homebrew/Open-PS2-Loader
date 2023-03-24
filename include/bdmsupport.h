#ifndef __BDM_SUPPORT_H
#define __BDM_SUPPORT_H

#include "include/iosupport.h"

#define BDM_MODE_UPDATE_DELAY MENU_UPD_DELAY_GENREFRESH

#include "include/mcemu.h"

typedef struct
{
    int active;       /* Activation flag */
    u64 start_sector; /* Start sector of vmc file */
    int flags;        /* Card flag */
    vmc_spec_t specs; /* Card specifications */
} bdm_vmc_infos_t;

#define MAX_BDM_DEVICES 5

typedef struct
{
    int massDeviceIndex;            // Underlying device index backing the mass fs partition, ex: usb0 = 0, usb1 = 1, ata master = 0, ata slave = 1, etc.
    char bdmPrefix[40];             // Contains the full path to the folder where all the games are.
    int bdmULSizePrev;
    time_t bdmModifiedCDPrev;
    time_t bdmModifiedDVDPrev;
    int bdmGameCount;
    base_game_info_t *bdmGames;
    char bdmDriver[32];
    int bdmLbaSize;                 // Max size of a logical block address in bits
} bdm_device_data_t;

void bdmInit();
int bdmFindPartition(char *target, const char *name, int write);
void bdmLoadModules(void);
void bdmLaunchGame(item_list_t* pItemList, int id, config_set_t *configSet);

void bdmEnumerateDevices();

#endif
