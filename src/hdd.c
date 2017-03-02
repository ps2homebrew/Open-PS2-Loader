#include "include/opl.h"
#include "include/ioman.h"
#include "include/hddsupport.h"

typedef struct // size = 1024
{
    u32 checksum; // HDL uses 0xdeadfeed magic here
    u32 magic;
    char gamename[160];
    u8 hdl_compat_flags;
    u8 ops2l_compat_flags;
    u8 dma_type;
    u8 dma_mode;
    char startup[60];
    u32 layer1_start;
    u32 discType;
    int num_partitions;
    struct
    {
        u32 part_offset; // in MB
        u32 data_start;  // in sectors
        u32 part_size;   // in KB
    } part_specs[65];
} hdl_apa_header;

#define HDL_GAME_DATA_OFFSET 0x100000 // Sector 0x800 in the user data area.
#define HDL_FS_MAGIC 0x1337

//-------------------------------------------------------------------------
int hddCheck(void)
{
    int ret;

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
int hddSetIdleTimeout(int timeout)
{
    // From hdparm man:
    // A value of zero means "timeouts  are  disabled":  the
    // device will not automatically enter standby mode.  Values from 1
    // to 240 specify multiples of 5 seconds, yielding timeouts from  5
    // seconds to 20 minutes.  Values from 241 to 251 specify from 1 to
    // 11 units of 30 minutes, yielding timeouts from 30 minutes to 5.5
    // hours.   A  value  of  252  signifies a timeout of 21 minutes. A
    // value of 253 sets a vendor-defined timeout period between 8  and
    // 12  hours, and the value 254 is reserved.  255 is interpreted as
    // 21 minutes plus 15 seconds.  Note that  some  older  drives  may
    // have very different interpretations of these values.

    u8 args[16];

    *(u32 *)&args[0] = timeout & 0xff;

    return fileXioDevctl("hdd0:", APA_DEVCTL_IDLE, args, 4, NULL, 0);
}

//-------------------------------------------------------------------------
int hddReadSectors(u32 lba, u32 nsectors, void *buf)
{
    u8 args[16];

    *(u32 *)&args[0] = lba;
    *(u32 *)&args[4] = nsectors;

    if (fileXioDevctl("hdd0:", APA_DEVCTL_ATA_READ, args, 8, buf, nsectors * 512) != 0)
        return -1;

    return 0;
}

//-------------------------------------------------------------------------
static int hddWriteSectors(u32 lba, u32 nsectors, const void *buf)
{
    static u8 WriteBuffer[1088] ALIGNED(64);
    int argsz;
    u8 *args = (u8 *)WriteBuffer;

    *(u32 *)&args[0] = lba;
    *(u32 *)&args[4] = nsectors;
    memcpy(&args[8], buf, nsectors * 512);

    argsz = 8 + (nsectors * 512);

    if (fileXioDevctl("hdd0:", APA_DEVCTL_ATA_WRITE, args, argsz, NULL, 0) != 0)
        return -1;

    return 0;
}

//-------------------------------------------------------------------------
int hddGetFormat(void)
{
    return fileXioDevctl("hdd0:", APA_DEVCTL_FORMAT, NULL, 0, NULL, 0);
}

//-------------------------------------------------------------------------
static unsigned char IOBuffer[1024] ALIGNED(64);

struct GameDataEntry
{
    u32 lba, size;
    struct GameDataEntry *next;
    char id[APA_IDMAX];
};

static int hddGetHDLGameInfo(struct GameDataEntry *game, hdl_game_info_t *ginfo)
{
    int ret;

    ret = hddReadSectors(game->lba, 2, IOBuffer);
    if (ret == 0) {

        hdl_apa_header *hdl_header = (hdl_apa_header *)IOBuffer;

        strncpy(ginfo->partition_name, game->id, APA_IDMAX);
        ginfo->partition_name[APA_IDMAX] = '\0';
        strncpy(ginfo->name, hdl_header->gamename, HDL_GAME_NAME_MAX);
        strncpy(ginfo->startup, hdl_header->startup, sizeof(ginfo->startup) - 1);
        ginfo->hdl_compat_flags = hdl_header->hdl_compat_flags;
        ginfo->ops2l_compat_flags = hdl_header->ops2l_compat_flags;
        ginfo->dma_type = hdl_header->dma_type;
        ginfo->dma_mode = hdl_header->dma_mode;
        ginfo->layer_break = hdl_header->layer1_start;
        ginfo->disctype = hdl_header->discType;
        ginfo->start_sector = game->lba;
        ginfo->total_size_in_kb = game->size / 2; //size * 2048 / 1024 = 0.5x
    } else
        ret = -1;

    return ret;
}

//-------------------------------------------------------------------------
static struct GameDataEntry *GetGameListRecord(struct GameDataEntry *head, const char *partition)
{
    struct GameDataEntry *current;

    for (current = head; current != NULL; current = current->next) {
        if (!strncmp(current->id, partition, APA_IDMAX)) {
            return current;
        }
    }

    return NULL;
}

int hddGetHDLGamelist(hdl_games_list_t *game_list)
{
    struct GameDataEntry *head, *current, *next, *pGameEntry;
    unsigned int count, i;
    iox_dirent_t dirent;
    int fd, ret;

    hddFreeHDLGamelist(game_list);

    ret = 0;
    if ((fd = fileXioDopen("hdd0:")) >= 0) {
        head = current = NULL;
        count = 0;
        while (fileXioDread(fd, &dirent) > 0) {
            if (dirent.stat.mode == HDL_FS_MAGIC) {
                if ((pGameEntry = GetGameListRecord(head, dirent.name)) == NULL) {
                    if (head == NULL) {
                        current = head = malloc(sizeof(struct GameDataEntry));
                    } else {
                        current = current->next = malloc(sizeof(struct GameDataEntry));
                    }

                    if (current == NULL)
                        break;

                    strncpy(current->id, dirent.name, APA_IDMAX);
                    count++;
                    current->next = NULL;
                    current->size = 0;
                    current->lba = 0;
                    pGameEntry = current;
                }

                if (!(dirent.stat.attr & APA_FLAG_SUB)) {
                    // Note: The APA specification states that there is a 4KB area used for storing the partition's information, before the extended attribute area.
                    pGameEntry->lba = dirent.stat.private_5 + (HDL_GAME_DATA_OFFSET + 4096) / 512;
                }

                pGameEntry->size += (dirent.stat.size / 4); //size in sectors * (512 / 2048)
            }
        }

        fileXioDclose(fd);

        if (head != NULL) {
            if ((game_list->games = malloc(sizeof(hdl_game_info_t) * count)) != NULL) {
                for (i = 0, current = head; i < count; i++, current = current->next) {
                    if ((ret = hddGetHDLGameInfo(current, &game_list->games[i])) != 0)
                        break;
                }

                if (ret) {
                    free(game_list->games);
                    game_list->games = NULL;
                } else {
                    game_list->count = count;
                }
            } else {
                ret = ENOMEM;
            }

            for (current = head; current != NULL; current = next) {
                next = current->next;
                free(current);
            }
        }
    } else {
        ret = fd;
    }

    return ret;
}

//-------------------------------------------------------------------------
void hddFreeHDLGamelist(hdl_games_list_t *game_list)
{
    if (game_list->games != NULL) {
        free(game_list->games);
        game_list->games = NULL;
        game_list->count = 0;
    }
}

//-------------------------------------------------------------------------
int hddSetHDLGameInfo(hdl_game_info_t *ginfo)
{
    if (hddReadSectors(ginfo->start_sector, 2, IOBuffer) != 0)
        return -EIO;

    hdl_apa_header *hdl_header = (hdl_apa_header *)IOBuffer;

    // just change game name and compat flags !!!
    strncpy(hdl_header->gamename, ginfo->name, sizeof(hdl_header->gamename));
    hdl_header->gamename[sizeof(hdl_header->gamename) - 1] = '\0';
    //hdl_header->hdl_compat_flags = ginfo->hdl_compat_flags;
    hdl_header->ops2l_compat_flags = ginfo->ops2l_compat_flags;
    hdl_header->dma_type = ginfo->dma_type;
    hdl_header->dma_mode = ginfo->dma_mode;

    if (hddWriteSectors(ginfo->start_sector, 2, IOBuffer) != 0)
        return -EIO;

    return 0;
}

//-------------------------------------------------------------------------
int hddDeleteHDLGame(hdl_game_info_t *ginfo)
{
    char path[38];

    LOG("HDD Delete game: '%s'\n", ginfo->name);

    sprintf(path, "hdd0:%s", ginfo->partition_name);

    return fileXioRemove(path);
}
