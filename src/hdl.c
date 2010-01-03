
#include "include/usbld.h"
#include <fileXio_rpc.h>

// APA Partition
#define APA_MAGIC			0x00415041	// 'APA\0'
#define APA_IDMAX			PS2PART_IDMAX
#define APA_MAXSUB			64			// Maximum # of sub-partitions
#define APA_PASSMAX			8
#define APA_FLAG_SUB		0x0001
#define APA_MBR_VERSION		2

typedef struct {
	u8	unused;
	u8	sec;
	u8	min;
	u8	hour;
	u8	day;
	u8	month;
	u16	year;
} ps2time;

typedef struct
{
	u32		checksum;
	u32		magic;				// APA_MAGIC
	u32		next;
	u32 	prev;
	char	id[APA_IDMAX];		// 16
	char	rpwd[APA_PASSMAX];	// 48
	char	fpwd[APA_PASSMAX];  // 56
	u32		start;				// 64
	u32		length;				// 68
	u16		type;				// 72
	u16		flags;				// 74
	u32		nsub;				// 76
	ps2time	created;			// 80
	u32		main;				// 88
	u32		number;				// 92
	u32		modver;				// 96
	u32		pading1[7];			// 100
	char	pading2[128];		// 128
	struct {					// 256
		char 	magic[32];
		u32 	version;
		u32		nsector;
		ps2time	created;
		u32		osdStart;
		u32		osdSize;
		char	pading3[200];
	} mbr;
	struct {
		u32 start;
		u32 length;
	} subs[APA_MAXSUB];
} apa_header;

typedef struct
{
  int existing;
  int modified;
  int linked;
  apa_header header;
} apa_partition_t;

typedef struct
{
  u32 device_size_in_mb;
  u32 total_chunks;
  u32 allocated_chunks;
  u32 free_chunks;

  u8 *chunks_map;

  // existing partitions
  u32 part_alloc_;
  u32 part_count;
  apa_partition_t *parts;
} apa_partition_table_t;

typedef struct					// size = 1024
{
	u32		checksum;			// HDL uses 0xdeadfeed magic here
	u32		magic;
	char	gamename[160];
	u8  	compat_flags;
	u8		pad[3];
	char	startup[60];
	u32 	layer1_start;
	u32 	discType;
	int 	num_partitions;
	struct {
		u32 	part_offset; 	// in MB
		u32 	data_start; 	// in sectors
		u32 	part_size; 		// in KB				
	} part_specs[65];
} hdl_apa_header;

//
// DEVCTL commands
//
#define APA_DEVCTL_MAX_SECTORS		0x00004801	// max partition size(in sectors)
#define APA_DEVCTL_TOTAL_SECTORS	0x00004802
#define APA_DEVCTL_IDLE				0x00004803
#define APA_DEVCTL_FLUSH_CACHE		0x00004804
#define APA_DEVCTL_SWAP_TMP			0x00004805
#define APA_DEVCTL_DEV9_SHUTDOWN	0x00004806
#define APA_DEVCTL_STATUS			0x00004807
#define APA_DEVCTL_FORMAT			0x00004808
#define APA_DEVCTL_SMART_STAT		0x00004809
#define APA_DEVCTL_GETTIME				0x00006832 
#define APA_DEVCTL_SET_OSDMBR			0x00006833// arg = hddSetOsdMBR_t
#define APA_DEVCTL_GET_SECTOR_ERROR		0x00006834
#define APA_DEVCTL_GET_ERROR_PART_NAME	0x00006835// bufp = namebuffer[0x20]
#define APA_DEVCTL_ATA_READ				0x00006836// arg  = hddAtaTransfer_t 
#define APA_DEVCTL_ATA_WRITE			0x00006837// arg  = hddAtaTransfer_t 
#define APA_DEVCTL_SCE_IDENTIFY_DRIVE	0x00006838// bufp = buffer for atadSceIdentifyDrive 
#define APA_DEVCTL_IS_48BIT				0x00006840
#define APA_DEVCTL_SET_TRANSFER_MODE	0x00006841

//-------------------------------------------------------------------------
int hddCheck(void)
{
	register int ret;

	fileXioInit();
	ret = fileXioDevctl("hdd0:", APA_DEVCTL_STATUS, NULL, 0, NULL, 0);

	if ((ret >= 3) || (ret < 0))
		return -1;

	return ret;
}

//-------------------------------------------------------------------------
u32 hddGetTotalSectors(void)
{
	return fileXioDevctl("hdd0:", APA_DEVCTL_TOTAL_SECTORS, NULL, 0, NULL, 0);
}

//-------------------------------------------------------------------------
int hddIs48bit(void)
{
	return fileXioDevctl("hdd0:", APA_DEVCTL_IS_48BIT, NULL, 0, NULL, 0);
}

//-------------------------------------------------------------------------
int hddSetTransferMode(int type, int mode)
{
	u8 args[16];

	*(u32 *)&args[0] = type;
	*(u32 *)&args[4] = mode;

	return fileXioDevctl("hdd0:", APA_DEVCTL_SET_TRANSFER_MODE, args, 8, NULL, 0);
}

//-------------------------------------------------------------------------
int hddReadSectors(u32 lba, u32 nsectors, void *buf, int bufsize)
{
	u8 args[16];

	*(u32 *)&args[0] = lba;
	*(u32 *)&args[4] = nsectors;

	if (fileXioDevctl("hdd0:", APA_DEVCTL_ATA_READ, args, 8, buf, bufsize) != 0)
		return -1;

	return 0;
}

//-------------------------------------------------------------------------
static int apaCheckSum(apa_header *header)
{
	u32 *ptr=(u32 *)header;
	u32 sum=0;
	int i;

	for(i=1; i < 256; i++)
		sum+=ptr[i];
	return sum;
}

//-------------------------------------------------------------------------
static int apaAddPartition(apa_partition_table_t *table, apa_header *header, int existing, int linked)
{
	if (table->part_count == table->part_alloc_) { // grow buffer
		u32 bytes = (table->part_alloc_ + 16) * sizeof(apa_partition_t);
		apa_partition_t *tmp = malloc(bytes);
		if(tmp != NULL)
		{
			memset(tmp, 0, bytes);
			if (table->parts != NULL) // copy existing
				memcpy(tmp, table->parts, table->part_count * sizeof(apa_partition_t));
			free(table->parts);
			table->parts = tmp;
			table->part_alloc_ += 16;
		}
		else return -2;
	}

	memcpy(&table->parts[table->part_count].header, header, sizeof(apa_header));
	table->parts[table->part_count].existing = existing;
	table->parts[table->part_count].modified = !existing;
	table->parts[table->part_count].linked = linked;
	++table->part_count;

	return 0;
}

//-------------------------------------------------------------------------
static int apaReadPartitionTable(apa_partition_table_t **table)
{
	register u32 nsectors, lba;
	register int ret;

	nsectors = hddGetTotalSectors();
	lba = 0;

	*table = malloc(sizeof(apa_partition_table_t));
	if (*table == NULL) {
		return -2;
	}

	memset(*table, 0, sizeof(apa_partition_table_t));

	do {
		apa_header apaHeader;
		ret = hddReadSectors(lba, sizeof(apa_header) >> 9, &apaHeader, sizeof(apa_header));
		if (ret == 0) {
			if ((apaCheckSum(&apaHeader) == apaHeader.checksum) && (apaHeader.magic == APA_MAGIC)) {
				if (apaHeader.start < nsectors) {
					if ((apaHeader.flags == 0) && (apaHeader.type == 0x1337))
						ret = apaAddPartition(*table, &apaHeader, 1, 1);
					if (ret == 0)
						lba = apaHeader.next;
				}
				else {
					ret = 7; 	// data behind end-of-HDD
					break;
				}
			}
			else ret = 1;
		}

		if ((*table)->part_count > 10000)
			ret = 7;

	} while (ret == 0 && lba != 0);

	if (ret == 0)
		(*table)->device_size_in_mb = nsectors >> 11;
	else {
		free(*table);
		ret = 20000 + (*table)->part_count;
	}

	return ret;	
}

//-------------------------------------------------------------------------
static int hddGetHDLGameInfo(apa_header *header, hdl_game_info_t *ginfo)
{
	u32 i, size;
 	const u32 offset = 0x101000;
 	char buf[1024];
 	register int ret;

 	u32 start_sector = header->start + offset / 512;
 	
 	ret = hddReadSectors(start_sector, 2, buf, 2 * 512);
 	if (ret == 0) {
	 	
	 	hdl_apa_header *hdl_header = (hdl_apa_header *)buf;
	 	
	 	// checking deadfeed magic & PS2 game magic
	 	if (hdl_header->checksum != 0xdeadfeed)
	 		return -2;

		// calculate total size
		size = header->length;

		for (i = 0; i < header->nsub; i++)
			size += header->subs[i].length;

		memcpy(ginfo->partition_name, header->id, APA_IDMAX);
		ginfo->partition_name[APA_IDMAX] = 0;
		strcpy(ginfo->name, hdl_header->gamename);
		strcpy(ginfo->startup, hdl_header->startup);
		ginfo->compat_flags = hdl_header->compat_flags;
		ginfo->layer_break = hdl_header->layer1_start;
		ginfo->disctype = hdl_header->discType;
		ginfo->start_sector = start_sector;
		ginfo->total_size_in_kb = size / 2;
	}
	else ret = -1;

 	return ret;
}

//-------------------------------------------------------------------------
int hddGetHDLGamelist(hdl_games_list_t **game_list)
{
	register int ret;
	apa_partition_table_t *table;

	ret = apaReadPartitionTable(&table);
	if (ret == 0) {
		u32 i, count = 0;
		void *tmp;

		for (i = 0; i < table->part_count; i++)
			count += ((table->parts[i].header.flags == 0) && (table->parts[i].header.type == 0x1337));

		tmp = malloc(sizeof(hdl_game_info_t) * count);
		if (tmp != NULL) {

			memset(tmp, 0, sizeof(hdl_game_info_t) * count);
			*game_list = malloc(sizeof(hdl_games_list_t));
			if (*game_list != NULL) {
				
				u32 index = 0;
				memset(*game_list, 0, sizeof(hdl_games_list_t));
				(*game_list)->count = count;
				(*game_list)->games = tmp;
				(*game_list)->total_chunks = table->total_chunks;
				(*game_list)->free_chunks = table->free_chunks;

				for (i = 0; ((ret == 0) && (i < table->part_count)); i++) {
					apa_header *header = &table->parts[i].header;
					if ((header->flags == 0) && (header->type == 0x1337))
						ret = hddGetHDLGameInfo(header, (*game_list)->games + index++);
				}

				if (ret != 0)
					free(*game_list);
			} else
				ret = -2;

			if (ret != 0)
				free(tmp);

		} else ret = -2;
	}

	return ret;
}




