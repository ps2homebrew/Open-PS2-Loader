#ifndef __BDM_SUPPORT_H
#define __BDM_SUPPORT_H

typedef struct
{
    int massDeviceIndex; // Underlying device index backing the mass fs partition, ex: usb0 = 0, usb1 = 1, etc.
    char bdmPrefix[40];  // Contains the full path to the folder where all the games are.
    int bdmULSizePrev;
    time_t bdmModifiedCDPrev;
    time_t bdmModifiedDVDPrev;
    int bdmGameCount;
    base_game_info_t *bdmGames;
    char bdmDriver[32];
    int bdmDeviceType;      // Type of BDM device, see BDM_TYPE_* above
    int bdmDeviceTick;      // Used alongside BdmGeneration to tell if device data needs to be refreshed
    int bdmHddIsLBA48;      // 1 if the HDD supports LBA48, 0 if the HDD only supports LBA28
    int ataHighestUDMAMode; // Highest UDMA mode supported by the HDD
    unsigned char ThemesLoaded;
    unsigned char LanguagesLoaded;
    unsigned char ForceRefresh;
} bdm_device_data_t;

extern bdm_device_data_t *gAutoLaunchDeviceData;

int bdmFindPartition(char *target, const char *name, int write);
void bdmLoadModules(void);

void bdmInitSemaphore();
void bdmEnumerateDevices();

void bdmResolveLBA_UDMA(bdm_device_data_t *pDeviceData);

#endif
