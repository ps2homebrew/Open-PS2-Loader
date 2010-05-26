#ifndef __HDD_SUPPORT_H
#define __HDD_SUPPORT_H

#include "include/iosupport.h"

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

void hddInit();
item_list_t* hddGetObject(int initOnly);

#endif
