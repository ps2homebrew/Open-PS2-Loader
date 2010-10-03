#include "include/usbld.h"
#include "include/ioman.h"
#include "include/hddsupport.h"

//#define TEST_WRITES

typedef struct				// size = 1024
{
	u32	checksum;		// HDL uses 0xdeadfeed magic here
	u32	magic;
	char	gamename[160];
	u8  	hdl_compat_flags;
	u8  	ops2l_compat_flags;
	u8	dma_type;
	u8	dma_mode;
	char	startup[60];
	u32 	layer1_start;
	u32 	discType;
	int 	num_partitions;
	struct {
		u32 	part_offset;	// in MB
		u32 	data_start;	// in sectors
		u32 	part_size;	// in KB
	} part_specs[65];
} hdl_apa_header;

//
// DEVCTL commands
//
#define APA_DEVCTL_MAX_SECTORS		0x00004801	// max partition size(in sectors)
#define APA_DEVCTL_TOTAL_SECTORS	0x00004802
#define APA_DEVCTL_IDLE			0x00004803
#define APA_DEVCTL_FLUSH_CACHE		0x00004804
#define APA_DEVCTL_SWAP_TMP		0x00004805
#define APA_DEVCTL_DEV9_SHUTDOWN	0x00004806
#define APA_DEVCTL_STATUS		0x00004807
#define APA_DEVCTL_FORMAT		0x00004808
#define APA_DEVCTL_SMART_STAT		0x00004809
#define APA_DEVCTL_GETTIME		0x00006832
#define APA_DEVCTL_SET_OSDMBR		0x00006833// arg = hddSetOsdMBR_t
#define APA_DEVCTL_GET_SECTOR_ERROR	0x00006834
#define APA_DEVCTL_GET_ERROR_PART_NAME	0x00006835// bufp = namebuffer[0x20]
#define APA_DEVCTL_ATA_READ		0x00006836// arg  = hddAtaTransfer_t
#define APA_DEVCTL_ATA_WRITE		0x00006837// arg  = hddAtaTransfer_t
#define APA_DEVCTL_SCE_IDENTIFY_DRIVE	0x00006838// bufp = buffer for atadSceIdentifyDrive 
#define APA_DEVCTL_IS_48BIT		0x00006840
#define APA_DEVCTL_SET_TRANSFER_MODE	0x00006841

static apa_partition_table_t *ptable = NULL;

// chunks_map
#define	MAP_AVAIL	'.'
#define	MAP_MAIN	'M'
#define	MAP_SUB		's'
#define	MAP_COLL	'x'
#define	MAP_ALLOC	'*'

static u8 hdd_buf[16384];

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
static int hddReadSectors(u32 lba, u32 nsectors, void *buf, int bufsize)
{
	u8 args[16];

	*(u32 *)&args[0] = lba;
	*(u32 *)&args[4] = nsectors;

	if (fileXioDevctl("hdd0:", APA_DEVCTL_ATA_READ, args, 8, buf, bufsize) != 0)
		return -1;

	return 0;
}

//-------------------------------------------------------------------------
static int hddWriteSectors(u32 lba, u32 nsectors, void *buf)
{
	int argsz;
	u8 *args = (u8 *)hdd_buf;

	*(u32 *)&args[0] = lba;
	*(u32 *)&args[4] = nsectors;
	memcpy(&args[8], buf, nsectors << 9);

	argsz = 8 + (nsectors << 9);

	if (fileXioDevctl("hdd0:", APA_DEVCTL_ATA_WRITE, args, argsz, NULL, 0) != 0)
		return -1;

	return 0;
}

//-------------------------------------------------------------------------
static int hddFlushCache(void)
{
	return fileXioDevctl("hdd0:", APA_DEVCTL_FLUSH_CACHE, NULL, 0, NULL, 0);
}

//-------------------------------------------------------------------------
int hddGetFormat(void)
{
	return fileXioDevctl("hdd0:", APA_DEVCTL_FORMAT, NULL, 0, NULL, 0);
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
static int apaAddPartition(apa_partition_table_t *table, apa_header *header, int existing)
{
	register u32 nbytes;

	if (table->part_count == table->part_alloc_) {

		// grow buffer of 16 parts
		nbytes = (table->part_alloc_ + 16) * sizeof(apa_partition_t);
		apa_partition_t *parts = malloc(nbytes);
		if(parts != NULL) {

			memset(parts, 0, nbytes);

			// copy existing parts
			if (table->parts != NULL)
				memcpy(parts, table->parts, table->part_count * sizeof(apa_partition_t));

			// free old parts mem
			free(table->parts);
			table->parts = parts;
			table->part_alloc_ += 16;
		}
		else 
			return -2;
	}

	// copy the part to its buffer
	memcpy(&table->parts[table->part_count].header, header, sizeof(apa_header));
	table->parts[table->part_count].existing = existing;
	table->parts[table->part_count].modified = !existing;
	table->parts[table->part_count].linked = 1;
	table->part_count++;

	return 0;
}

//-------------------------------------------------------------------------
static void apaFreePartitionTable(apa_partition_table_t *table)
{
	register int i;

	if (table != NULL) {
		for (i=0; i<table->part_count; i++) {
			if (table->parts)
				free(table->parts);
		}

		if (table->chunks_map)
			free(table->chunks_map);
	}
}

//-------------------------------------------------------------------------
static int apaSetPartitionTableStats(apa_partition_table_t *table)
{
	register int i;
	char *chunks_map;

	table->total_chunks = table->device_size_in_mb / 128;
	chunks_map = malloc(table->total_chunks);
	if (chunks_map == NULL)
		return -1;

	*chunks_map = MAP_AVAIL;
	for (i=0; i<table->total_chunks; i++)
		chunks_map[i] = MAP_AVAIL;

	// build occupided/available space map
	table->allocated_chunks = 0;
	table->free_chunks = table->total_chunks;
	for (i=0; i<table->part_count; i++) {
		apa_header *part_hdr = (apa_header *)&table->parts[i].header;
		int part_num = part_hdr->start / ((128 * 1024 * 1024) / 512);
		int num_parts = part_hdr->length / ((128 * 1024 * 1024) / 512);

		// "alloc" num_parts starting at part_num
		while (num_parts) {
			if (chunks_map[part_num] == MAP_AVAIL)
				chunks_map[part_num] = (part_hdr->main == 0 ? MAP_MAIN : MAP_SUB);
			else
				chunks_map[part_num] = MAP_COLL; // collision
			part_num++;
			num_parts--;
			table->allocated_chunks++;
			table->free_chunks--;
		}
	}

	if (table->chunks_map)
		free(table->chunks_map);

	table->chunks_map = (u8 *)chunks_map;

	return 0;
}

//-------------------------------------------------------------------------
static int apaReadPartitionTable(apa_partition_table_t **table)
{
	register u32 nsectors, lba;
	register int ret;

	// get number of sectors on device
	nsectors = hddGetTotalSectors();

	// if not already done allocate space for the partition table
	if (*table == NULL) {
		*table = malloc(sizeof(apa_partition_table_t));
		if (*table == NULL) {
			return -2;
		}
	}
	else // if partition table was already allocated, Free all 
		apaFreePartitionTable(*table);

	// clear the partition table
	memset(*table, 0, sizeof(apa_partition_table_t));

	// read the partition table
	lba = 0;
	do {
		apa_header apaHeader;
		// read partition header
		ret = hddReadSectors(lba, sizeof(apa_header) >> 9, &apaHeader, sizeof(apa_header));
		if (ret == 0) {
			// check checksum & header magic
			if ((apaCheckSum(&apaHeader) == apaHeader.checksum) && (apaHeader.magic == APA_MAGIC)) {
				if (apaHeader.start < nsectors) {
					//if ((apaHeader.flags == 0) && (apaHeader.type == 0x1337))
						ret = apaAddPartition(*table, &apaHeader, 1);
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

	if (ret == 0) {
		(*table)->device_size_in_mb = nsectors >> 11;
		ret = apaSetPartitionTableStats(*table);
	}
	else {
		apaFreePartitionTable(*table);
		ret = 20000 + (*table)->part_count;
	}

	return ret;	
}

//-------------------------------------------------------------------------
static int apaCheckLinkedList(apa_partition_table_t *table, int flag)

{
	register int i;

	for (i=0; i<table->part_count; i++) {
		apa_partition_t *prev = (apa_partition_t *)&table->parts[(i > 0 ? i - 1 : table->part_count - 1)];
		apa_partition_t *curr = (apa_partition_t *)&table->parts[i];
		apa_partition_t *next = (apa_partition_t *)&table->parts[(i + 1 < table->part_count ? i + 1 : 0)];

		if (curr->header.prev != prev->header.start) {
			if (!flag)
				return -1;
			else {
				curr->modified = 1;
				curr->header.prev = prev->header.start;
			}
		}
		if (curr->header.next != next->header.start) {
			if (!flag)
				return -1;
			else {
				curr->modified = 1;
				curr->header.next = next->header.start;
			}
		}
		if ((flag) && (curr->modified))
			curr->header.checksum = apaCheckSum(&curr->header);
	}

	return 0;
}

//-------------------------------------------------------------------------
static int apaCheckPartitionTable(apa_partition_table_t *table)
{
	register int i, j, k;

	u32 total_sectors = table->device_size_in_mb * 1024 * 2;

	for (i=0; i<table->part_count; i++) {
		apa_header *part_hdr = &table->parts[i].header;

		// check checksum
		if (part_hdr->checksum != apaCheckSum(part_hdr))
			return -1;

		// check for data behind end of HDD
		if (!((part_hdr->start < total_sectors) && (part_hdr->start + part_hdr->length <= total_sectors)))
			return -2;

		// check partition length is multiple of 128 MB
		if ((part_hdr->length % ((128 * 1024 * 1024) / 512)) != 0)
			return -3;

		// check partition start is multiple of partion length
		if ((part_hdr->start % part_hdr->length) != 0)
			return -4;

		// check all subs partitions
		if ((part_hdr->main == 0) && (part_hdr->flags == 0) && (part_hdr->start != 0)) {
			int count = 0;

			for (j=0; j<table->part_count; j++) {
				apa_header *part2_hdr = &table->parts[j].header;
				if (part2_hdr->main == part_hdr->start) { // sub-partition of current main partition
					int found;
					if (part2_hdr->flags != APA_FLAG_SUB)
						return -5;

					found = 0;
					for (k=0; k<part_hdr->nsub; k++) {
						if (part_hdr->subs[k].start == part2_hdr->start) { // in list
							if (part_hdr->subs[k].length != part2_hdr->length)
								return -6;
							found = 1;
							break;
						}
					}
					if (!found)
						return -7; // not found in the list

					count++;
				}
			}
			if (count != part_hdr->nsub)
				return -8; // wrong number of sub-partitions
		}
	}

	// verify double-linked list
	if (apaCheckLinkedList(table, 0) < 0)
		return -9; // bad links

	LOG("apaCheckPartitionTable OK!\n");

	return 0;
}

//-------------------------------------------------------------------------
static int apaWritePartitionTable(apa_partition_table_t *table)
{
	register int ret, i;

	LOG("apaWritePartitionTable\n");

	ret = apaCheckPartitionTable(table);
	if (ret < 0)
		return ret;

	for (i=0; i<table->part_count; i++) {

		if (table->parts[i].modified) {
			apa_header *part_hdr = &table->parts[i].header;
			LOG("writing 2 sectors at sector 0x%X\n", part_hdr->start);
#ifndef TEST_WRITES
			ret = hddWriteSectors(part_hdr->start, 2, (void *)part_hdr);
			if (ret < 0)
				return -10;
#endif
		}
	}

	return 0;
}

//-------------------------------------------------------------------------
static int apaFindPartition(apa_partition_table_t *table, char *partname)
{
	register int ret, i;

	ret = -1;

	for (i=0; i<table->part_count; i++) {

		apa_header *part_hdr = &table->parts[i].header;

		// if part found, return its index
		if ((part_hdr->main == 0) && (!strcmp(part_hdr->id, partname))) {
			ret = i;
			break;
		}
	}

	return ret;
}

//-------------------------------------------------------------------------
static int apaDeletePartition(apa_partition_table_t *table, char *partname)
{
	register int i, part_index;
	register int count = 1;
	u32 pending_deletes[APA_MAXSUB];

	LOG("apaDeletePartition %s\n", partname);

	// retrieve part index
	part_index = apaFindPartition(table, partname);
	if (part_index < 0)
		return -1;
     
	apa_header *part_hdr = &table->parts[part_index].header;

	// Do not delete system partitions
	if (part_hdr->type == 1)
		return -2;

	// preserve a list of starting sectors of partitions to be deleted
	pending_deletes[0] = part_hdr->start;
	LOG("apaDeletePartition: found part at %d \n", part_hdr->start / 262144);
	for (i=0; i<part_hdr->nsub; i++) {
		LOG("apaDeletePartition: found subpart at %d \n", part_hdr->subs[i].start / 262144);
		pending_deletes[count++] = part_hdr->subs[i].start;
	}

	LOG("apaDeletePartition: number of subpartitions=%d count=%d\n", part_hdr->nsub, count);

	// remove partitions from the double-linked list
	i = 0;
	while (i<table->part_count) {
		int j;
		int found = 0;

		for (j=0; j<count; j++) {
			if (table->parts[i].header.start == pending_deletes[j]) {
				found = 1;
				break;
			}
		}

		if (found) {
			// remove this partition
			int part_num = table->parts[i].header.start / 262144; // 262144 sectors == 128M
			int num_parts = table->parts[i].header.length / 262144;

			LOG("apaDeletePartition: partition found! num_parts=%d part_num=%d\n", num_parts, part_num);

			memmove((void *)&table->parts[i], (void *)&table->parts[i+1], sizeof(apa_partition_t) * (table->part_count-i-1));
			table->part_count--;

			// "free" num_parts starting at part_num
			while (num_parts) {
				table->chunks_map[part_num] = MAP_AVAIL;
				part_num++;
				num_parts--;
				table->allocated_chunks--;
				table->free_chunks++;
			}
		}
		else
			i++;
	}

	apaCheckLinkedList(table, 1);

	return 0;
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

		// calculate total size
		size = header->length;

		for (i = 0; i < header->nsub; i++)
			size += header->subs[i].length;

		memcpy(ginfo->partition_name, header->id, APA_IDMAX);
		ginfo->partition_name[APA_IDMAX] = 0;
		strncpy(ginfo->name, hdl_header->gamename, HDL_GAME_NAME_MAX);
		strncpy(ginfo->startup, hdl_header->startup, 12);
		ginfo->hdl_compat_flags = hdl_header->hdl_compat_flags;
		ginfo->ops2l_compat_flags = hdl_header->ops2l_compat_flags;
		ginfo->dma_type = hdl_header->dma_type;
		ginfo->dma_mode = hdl_header->dma_mode;
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

	ret = apaReadPartitionTable(&ptable);
	if (ret == 0) {

		u32 i, count = 0;
		void *tmp;

		if (*game_list != NULL) {
			for (i = 0; i < (*game_list)->count; i++) {
				if ((*game_list)->games != NULL)
					free((*game_list)->games);
			}
		}

		for (i = 0; i < ptable->part_count; i++)
			count += ((ptable->parts[i].header.flags == 0) && (ptable->parts[i].header.type == 0x1337));

		tmp = malloc(sizeof(hdl_game_info_t) * count);
		if (tmp != NULL) {

			memset(tmp, 0, sizeof(hdl_game_info_t) * count);
			*game_list = malloc(sizeof(hdl_games_list_t));
			if (*game_list != NULL) {

				u32 index = 0;
				memset(*game_list, 0, sizeof(hdl_games_list_t));
				(*game_list)->count = count;
				(*game_list)->games = tmp;
				(*game_list)->total_chunks = ptable->total_chunks;
				(*game_list)->free_chunks = ptable->free_chunks;

				for (i = 0; ((ret == 0) && (i < ptable->part_count)); i++) {
					apa_header *header = &ptable->parts[i].header;
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

//-------------------------------------------------------------------------
int hddFreeHDLGamelist(hdl_games_list_t *game_list)
{
	register int i;

	apaFreePartitionTable(ptable);

	if (game_list != NULL) {
		for (i = 0; i < game_list->count; i++) {
			if (game_list->games != NULL)
				free(game_list->games);
		}

		free(game_list);
	}

	return 0;
}

//-------------------------------------------------------------------------
int hddSetHDLGameInfo(hdl_game_info_t *ginfo)
{
	const u32 offset = 0x101000;
	char buf[1024];
	int part_index;

	if (ptable == NULL)
		return -1;

	// retrieve part index
	part_index = apaFindPartition(ptable, ginfo->partition_name);
	if (part_index < 0)
		return -1;

	apa_header *header = &ptable->parts[part_index].header;
	if (header == NULL)
		return -2;

	u32 start_sector = header->start + offset / 512;

	if (hddReadSectors(start_sector, 2, buf, 2 * 512) != 0)
		return -3;

	hdl_apa_header *hdl_header = (hdl_apa_header *)buf;

	// checking deadfeed magic & PS2 game magic
	if (hdl_header->checksum != 0xdeadfeed)
		return -4;

	// just change game name and compat flags !!!
	strncpy(hdl_header->gamename, ginfo->name, 160);
	hdl_header->gamename[159] = '\0';
	//hdl_header->hdl_compat_flags = ginfo->hdl_compat_flags;
	hdl_header->ops2l_compat_flags = ginfo->ops2l_compat_flags;
	hdl_header->dma_type = ginfo->dma_type;
	hdl_header->dma_mode = ginfo->dma_mode;

	if (hddWriteSectors(start_sector, 2, buf) != 0)
		return -5;

	hddFlushCache();

 	return 0;
}

//-------------------------------------------------------------------------
int hddDeleteHDLGame(hdl_game_info_t *ginfo)
{
	register int ret;

	LOG("hddDeleteHDLGame() game name='%s'\n", ginfo->name);

	if (ptable == NULL)
		return -1;

	ret = apaDeletePartition(ptable, ginfo->partition_name);
	if (ret < 0)
		return -2;

	ret = apaWritePartitionTable(ptable);
	if (ret < 0)
		return -3;

	hddFlushCache();

	LOG("hddDeleteHDLGame: '%s' deleted!\n", ginfo->name);

	return 0;
}

