#ifndef __HDD_H
#define __HDD_H

#include <hdd-ioctl.h>

#define HDL_GAME_NAME_MAX 64

// APA Partition
#define APA_IOCTL2_GETHEADER 0x6836

typedef struct
{
    u32 start;  // Sector address
    u32 length; // Sector count
} apa_sub_t;

typedef struct
{
    char partition_name[APA_IDMAX + 1];
    char name[HDL_GAME_NAME_MAX + 1];
    char startup[8 + 1 + 3 + 1];
    u8 hdl_compat_flags;
    u8 ops2l_compat_flags;
    u8 dma_type;
    u8 dma_mode;
    u8 disctype;
    u32 layer_break;
    u32 start_sector;
    u32 total_size_in_kb;
} hdl_game_info_t;

typedef struct
{
    u32 count;
    hdl_game_info_t *games;
} hdl_games_list_t;

typedef struct
{
    u32 number;
    u16 subpart;
    u16 count;
} pfs_blockinfo_t;

extern hdl_game_info_t *gAutoLaunchGame;

int hddReadSectors(u32 lba, u32 nsectors, void *buf);

// Array should be APA_MAXSUB+1 entries.
int hddGetPartitionInfo(const char *name, apa_sub_t *parts);

// Array should be max entries.
int hddGetFileBlockInfo(const char *name, const apa_sub_t *subs, pfs_blockinfo_t *blocks, int max);

int hddCheck(void);
u32 hddGetTotalSectors(void);
int hddIs48bit(void);
int hddSetTransferMode(int type, int mode);
void hddSetIdleTimeout(int timeout);
void hddSetIdleImmediate(void);
int hddGetHDLGamelist(hdl_games_list_t *game_list);
void hddFreeHDLGamelist(hdl_games_list_t *game_list);
int hddSetHDLGameInfo(hdl_game_info_t *ginfo);
int hddDeleteHDLGame(hdl_game_info_t *ginfo);


#endif
