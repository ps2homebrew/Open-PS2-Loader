#ifndef __HDD_SUPPORT_H
#define __HDD_SUPPORT_H

#include "include/iosupport.h"

#define HDD_MODE_UPDATE_DELAY MENU_UPD_DELAY_NOUPDATE

#define HDL_GAME_NAME_MAX 64

// APA Partition
#define APA_IOCTL2_GETHEADER 0x6836

typedef struct
{
    char partition_name[APA_IDMAX + 1];
    char name[HDL_GAME_NAME_MAX + 1];
    char startup[8 + 1 + 3 + 1];
    u8 hdl_compat_flags;
    u8 ops2l_compat_flags;
    u8 dma_type;
    u8 dma_mode;
    u32 layer_break;
    int disctype;
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
    u8 unused;
    u8 sec;
    u8 min;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} ps2time_t;

typedef struct
{
    u32 start;  // Sector address
    u32 length; // Sector count
} apa_sub_t;

typedef struct
{
    u32 checksum;
    u32 magic; // APA_MAGIC
    u32 next;
    u32 prev;
    char id[APA_IDMAX];
    char rpwd[APA_PASSMAX];
    char fpwd[APA_PASSMAX];
    u32 start;
    u32 length;
    u16 type;
    u16 flags;
    u32 nsub;
    ps2time_t created;
    u32 main;
    u32 number;
    u32 modver;
    u32 pading1[7];
    char pading2[128];
    struct
    {
        char magic[32];
        u32 version;
        u32 nsector;
        ps2time_t created;
        u32 osdStart;
        u32 osdSize;
        char pading3[200];
    } mbr;
    apa_sub_t subs[APA_MAXSUB];
} apa_header_t;

typedef struct
{
    u32 number;
    u16 subpart;
    u16 count;
} pfs_blockinfo_t;

typedef struct
{
    u32 checksum;
    u32 magic;
    pfs_blockinfo_t inode_block;
    pfs_blockinfo_t next_segment;
    pfs_blockinfo_t last_segment;
    pfs_blockinfo_t unused;
    pfs_blockinfo_t data[114];
    u16 mode;
    u16 attr;
    u16 uid;
    u16 gid;
    ps2time_t atime;
    ps2time_t ctime;
    ps2time_t mtime;
    u64 size;
    u32 number_blocks;
    u32 number_data;
    u32 number_segdesg;
    u32 subpart;
    u32 reserved[4];
} pfs_inode_t;

typedef struct
{
    u32 start;
    u32 length;
} apa_subs;

#ifdef VMC
#include "include/mcemu.h"
typedef struct
{
    int active;                 /* Activation flag */
    apa_subs parts[5];          /* Vmc file Apa partitions */
    pfs_blockinfo_t blocks[10]; /* Vmc file Pfs inode */
    int flags;                  /* Card flag */
    vmc_spec_t specs;           /* Card specifications */
} hdd_vmc_infos_t;
#endif

int hddCheck(void);
u32 hddGetTotalSectors(void);
int hddIs48bit(void);
int hddSetTransferMode(int type, int mode);
int hddSetIdleTimeout(int timeout);
int hddGetHDLGamelist(hdl_games_list_t *game_list);
void hddFreeHDLGamelist(hdl_games_list_t *game_list);
int hddSetHDLGameInfo(hdl_game_info_t *ginfo);
int hddDeleteHDLGame(hdl_game_info_t *ginfo);

void hddInit();
item_list_t *hddGetObject(int initOnly);
void hddLoadModules(void);

#endif
