/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright 2010, crazyc

  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include <cdvdman.h>
#include <stdio.h>
#include <loadcore.h>
#include <iomanX.h>
#include <intrman.h>
#include <mcman.h>
#include <thsemap.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <errno.h>

#include "genvmc.h"

#ifdef DEBUG
#define DPRINTF(args...) printf(args)
#else
#define DPRINTF(args...) \
    do {                 \
    } while (0)
#endif

#define MODNAME "genvmc"
IRX_ID(MODNAME, 1, 1);

// driver ops protypes
static int genvmc_dummy(void);
static int genvmc_init(iop_device_t *dev);
static int genvmc_deinit(iop_device_t *dev);
static int genvmc_devctl(iop_file_t *f, const char *name, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen);

// driver ops func tab
static iop_device_ops_t genvmc_ops = {
    &genvmc_init,
    &genvmc_deinit,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    &genvmc_devctl,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy,
    (void *)genvmc_dummy};

// driver descriptor
static iop_device_t genvmc_dev = {
    "genvmc",
    IOP_DT_FS | IOP_DT_FSEXT,
    1,
    "genvmc",
    &genvmc_ops};

#define sceMcDetectCard  McDetectCard
#define sceMcReadPage    McReadPage
#define sceMcGetCardType McGetMcType

// SONY superblock magic & version
static const char SUPERBLOCK_MAGIC[] = "Sony PS2 Memory Card Format ";
static const char SUPERBLOCK_VERSION[] = "1.2.0.0";

// superblock struct
typedef struct
{                          // size = 384
    u8 magic[28];          // Superblock magic, on PS2 MC : "Sony PS2 Memory Card Format "
    u8 version[12];        // Version number of the format used, 1.2 indicates full support for bad_block_list
    s16 pagesize;          // size in bytes of a memory card page
    u16 pages_per_cluster; // number of pages in a cluster
    u16 blocksize;         // number of pages in an erase block
    u16 unused;            // unused
    u32 clusters_per_card; // total size in clusters of the memory card
    u32 alloc_offset;      // Cluster offset of the first allocatable cluster. Cluster values in the FAT and directory entries are relative to this. This is the cluster immediately after the FAT
    u32 alloc_end;         // The cluster after the highest allocatable cluster. Relative to alloc_offset. Not used
    u32 rootdir_cluster;   // First cluster of the root directory. Relative to alloc_offset. Must be zero
    u32 backup_block1;     // Erase block used as a backup area during programming. Normally the the last block on the card, it may have a different value if that block was found to be bad
    u32 backup_block2;     // This block should be erased to all ones. Normally the the second last block on the card
    u8 unused2[8];
    u32 ifc_list[32];       // List of indirect FAT clusters. On a standard 8M card there's only one indirect FAT cluster
    int bad_block_list[32]; // List of erase blocks that have errors and shouldn't be used
    u8 cardtype;            // Memory card type. Must be 2, indicating that this is a PS2 memory card
    u8 cardflags;           // Physical characteristics of the memory card
    u16 unused3;
    u32 cluster_size;
    u32 FATentries_per_cluster;
    u32 clusters_per_block;
    int cardform;
    u32 rootdir_cluster2;
    u32 unknown1;
    u32 unknown2;
    u32 max_allocatable_clusters;
    u32 unknown3;
    u32 unknown4;
    int unknown5;
} MCDevInfo;

#define BLOCKKB 16

static MCDevInfo devinfo __attribute__((aligned(16)));
static u8 cluster_buf[(BLOCKKB * 1024) + 16] __attribute__((aligned(16)));

static int genvmc_io_sema = -1;
static int genvmc_thread_sema = -1;
static int genvmc_abort_sema = -1;
static int genvmc_abort_finished_sema = -1;

static int genvmc_abort = 0;

static statusVMCparam_t genvmc_stats;

//--------------------------------------------------------------
static void long_multiply(u32 v1, u32 v2, u32 *HI, u32 *LO)
{
    register long a, b, c, d;
    register long x, y;

    a = (v1 >> 16) & 0xffff;
    b = v1 & 0xffff;
    c = (v2 >> 16) & 0xffff;
    d = v2 & 0xffff;

    *LO = b * d;
    x = a * d + c * b;
    y = ((*LO >> 16) & 0xffff) + x;

    *LO = (*LO & 0xffff) | ((y & 0xffff) << 16);
    *HI = (y >> 16) & 0xffff;

    *HI += a * c;
}

//--------------------------------------------------------------
static int mc_getmcrtime(sceMcStDateTime *time)
{
    register int retries;
    sceCdCLOCK cdtime;

    retries = 64;

    do {
        if (sceCdRC(&cdtime))
            break;
    } while (--retries > 0);

    if (cdtime.stat & 0x80) {
        time->Year = 2000;
        time->Month = 3;
        time->Day = 4;
        time->Hour = 5;
        time->Min = 6;
        time->Sec = 7;
        time->Resv2 = 0;
    } else {
        time->Resv2 = 0;
        time->Sec = btoi(cdtime.second);
        time->Min = btoi(cdtime.minute);
        time->Hour = btoi(cdtime.hour);
        time->Day = btoi(cdtime.day);

        if ((cdtime.month & 0x10) != 0) // Keep only valid bits: 0x1f (for month values 1-12 in BCD)
            time->Month = (cdtime.month & 0xf) + 0xa;
        else
            time->Month = cdtime.month & 0xf;

        time->Year = btoi(cdtime.year) + 2000;
    }

    return 0;
}

//--------------------------------------------------------------
static int mc_writecluster(int fd, int cluster, void *buf, int dup)
{
    register int r, size;
    MCDevInfo *mcdi = (MCDevInfo *)&devinfo;

    WaitSema(genvmc_abort_sema);

    if (genvmc_abort) {
        SignalSema(genvmc_abort_sema);
        return -2;
    } else {
        lseek(fd, cluster * mcdi->cluster_size, SEEK_SET);
        size = mcdi->cluster_size * dup;
        r = write(fd, buf, size);
        if (r != size)
            return -1;
    }

    SignalSema(genvmc_abort_sema);

    return 0;
}

//--------------------------------------------------------------
// this is designed to work only with the rom0 mcman for max compatibility
// but that means it isn't really possible to lock properly so don't write
// to the card during the operation. hints came from mcdump.c by Polo35
static int vmc_mccopy(char *filename, int slot, int *progress, char *msg)
{
    MCDevInfo *mcdi = (MCDevInfo *)&devinfo;
    unsigned int blocks, pagesblock, clustersblock, i, j;
    int ret;

    strcpy(msg, "Checking memcard...");
    if ((sceMcDetectCard(slot, 0) < -1) && (sceMcGetCardType(slot, 0) != 2))
        return -98;

    if (sceMcReadPage(slot, 0, 0, cluster_buf))
        return -99;

    memcpy(mcdi, cluster_buf, sizeof(MCDevInfo));
    if (memcmp(mcdi->magic, SUPERBLOCK_MAGIC, 28))
        return -100;

    pagesblock = (BLOCKKB * 1024) / mcdi->pagesize;
    clustersblock = pagesblock / mcdi->pages_per_cluster;
    blocks = mcdi->clusters_per_card / clustersblock;

    int genvmc_fh = open(filename, O_RDWR | O_CREAT | O_TRUNC);
    if (genvmc_fh < 0)
        return -101;

    strcpy(msg, "Copying memcard to VMC...");
    for (i = 0; i < blocks; i++) {
        *progress = i / (blocks / 99);
        for (j = 0; j < pagesblock; j++) {
            if (sceMcReadPage(slot, 0, (i * pagesblock) + j, &cluster_buf[j * mcdi->pagesize])) {
                ret = -102;
                goto exit;
            }
        }
        ret = mc_writecluster(genvmc_fh, i * clustersblock, cluster_buf, clustersblock);
        if (ret < 0) {
            if (ret == -2) // it's user abort
                ret = -1000;
            else
                ret = -103;
            goto exit;
        }
    }
    for (i = (blocks * clustersblock); i < mcdi->clusters_per_card; i++) {
        for (j = 0; j < mcdi->pages_per_cluster; j++) {
            if (sceMcReadPage(slot, 0, (i * mcdi->pages_per_cluster) + j, &cluster_buf[j * mcdi->pagesize])) {
                ret = -102;
                goto exit;
            }
        }
        ret = mc_writecluster(genvmc_fh, i, cluster_buf, 1);
        if (ret < 0) {
            if (ret == -2) // it's user abort
                ret = -1000;
            else
                ret = -103;
            goto exit;
        }
    }
    *progress = 100;
    ret = 0;
exit:
    close(genvmc_fh);
    return ret;
}

//--------------------------------------------------------------
static int vmc_mcformat(char *filename, int size_kb, int blocksize, int *progress, char *msg)
{
    register int i, r, b, ifc_index, fat_index;
    register int ifc_length, fat_length, alloc_offset;
    register int j = 0, z = 0;
    int oldstate;
    MCDevInfo *mcdi = (MCDevInfo *)&devinfo;

    strcpy(msg, "Creating VMC file...");
    int genvmc_fh = open(filename, O_RDWR | O_CREAT | O_TRUNC);
    if (genvmc_fh < 0)
        return -101;

    // set superblock magic & version
    memset((void *)&mcdi->magic, 0, sizeof(mcdi->magic) + sizeof(mcdi->version));
    strcpy((char *)&mcdi->magic, SUPERBLOCK_MAGIC);
    strcat((char *)&mcdi->magic, SUPERBLOCK_VERSION);

    // set mc specs
    mcdi->cluster_size = 1024;   // size in KB of clusters
    mcdi->blocksize = blocksize; // how many pages in a block of data
    mcdi->pages_per_cluster = 2; // how many pages in a cluster
    mcdi->pagesize = mcdi->cluster_size / mcdi->pages_per_cluster;
    mcdi->clusters_per_block = mcdi->blocksize / mcdi->pages_per_cluster;
    mcdi->clusters_per_card = (size_kb * 1024) / mcdi->cluster_size;
    mcdi->cardtype = 0x02; // PlayStation2 card type
    mcdi->cardflags = 0x2b;
    mcdi->cardform = -1;
    mcdi->FATentries_per_cluster = mcdi->cluster_size / sizeof(u32);

    // clear bad blocks list
    for (i = 0; i < 32; i++)
        mcdi->bad_block_list[i] = -1;

    // erase all clusters
    strcpy(msg, "Erasing VMC clusters...");
    memset(cluster_buf, 0xff, sizeof(cluster_buf));
    for (i = 0; i < mcdi->clusters_per_card; i += BLOCKKB) {
        *progress = i / (mcdi->clusters_per_card / 99);
        r = mc_writecluster(genvmc_fh, i, cluster_buf, BLOCKKB);
        if (r < 0) {
            if (r == -2) // it's user abort
                r = -1000;
            else
                r = -102;
            goto err_out;
        }
    }

    // calculate fat & ifc length
    fat_length = (((mcdi->clusters_per_card << 2) - 1) / mcdi->cluster_size) + 1; // get length of fat in clusters
    ifc_length = (((fat_length << 2) - 1) / mcdi->cluster_size) + 1;              // get number of needed ifc clusters

    if (!(ifc_length <= 32)) {
        ifc_length = 32;
        fat_length = mcdi->FATentries_per_cluster << 5;
    }

    // clear ifc list
    for (i = 0; i < 32; i++)
        mcdi->ifc_list[i] = -1;
    ifc_index = mcdi->blocksize / 2;
    i = ifc_index;
    for (j = 0; j < ifc_length; j++, i++)
        mcdi->ifc_list[j] = i;

    // keep fat cluster index
    fat_index = i;

    // allocate memory for ifc clusters
    CpuSuspendIntr(&oldstate);
    u8 *ifc_mem = AllocSysMemory(ALLOC_FIRST, (ifc_length * mcdi->cluster_size) + 0XFF, NULL);
    CpuResumeIntr(oldstate);
    if (!ifc_mem) {
        r = -103;
        goto err_out;
    }
    memset(ifc_mem, 0, ifc_length * mcdi->cluster_size);

    // build ifc clusters
    u32 *ifc = (u32 *)ifc_mem;
    for (j = 0; j < fat_length; j++, i++) {
        // just as security...
        if (i >= mcdi->clusters_per_card) {
            CpuSuspendIntr(&oldstate);
            FreeSysMemory(ifc_mem);
            CpuResumeIntr(oldstate);
            r = -104;
            goto err_out;
        }
        ifc[j] = i;
    }

    // write ifc clusters
    strcpy(msg, "Writing ifc clusters...");
    for (z = 0; z < ifc_length; z++) {
        r = mc_writecluster(genvmc_fh, mcdi->ifc_list[z], &ifc_mem[z * mcdi->cluster_size], 1);
        if (r < 0) {
            // freeing ifc clusters memory
            CpuSuspendIntr(&oldstate);
            FreeSysMemory(ifc_mem);
            CpuResumeIntr(oldstate);
            if (r == -2) // it's user abort
                r = -1000;
            else
                r = -105;
            goto err_out;
        }
    }

    // freeing ifc clusters memory
    CpuSuspendIntr(&oldstate);
    FreeSysMemory(ifc_mem);
    CpuResumeIntr(oldstate);

    // set alloc offset
    alloc_offset = i;

    // set backup blocks
    mcdi->backup_block1 = (mcdi->clusters_per_card / mcdi->clusters_per_block) - 1;
    mcdi->backup_block2 = (mcdi->clusters_per_card / mcdi->clusters_per_block) - 2;

    // calculate number of allocatable clusters per card
    u32 hi, lo, temp;
    long_multiply(mcdi->clusters_per_card, 0x10624dd3, &hi, &lo);
    temp = (hi >> 6) - (mcdi->clusters_per_card >> 31);
    mcdi->max_allocatable_clusters = (((((temp << 5) - temp) << 2) + temp) << 3) + 1;
    j = alloc_offset;

    // building/writing FAT clusters
    strcpy(msg, "Writing fat clusters...");
    i = (mcdi->clusters_per_card / mcdi->clusters_per_block) - 2; // 2 backup blocks
    for (z = 0; j < (i * mcdi->clusters_per_block); j += mcdi->FATentries_per_cluster) {

        memset(cluster_buf, 0, mcdi->cluster_size);
        u32 *fc = (u32 *)cluster_buf;
        int sz_u32 = (i * mcdi->clusters_per_block) - j;
        if (sz_u32 > mcdi->FATentries_per_cluster)
            sz_u32 = mcdi->FATentries_per_cluster;
        for (b = 0; b < sz_u32; b++)
            fc[b] = 0x7fffffff; // marking free cluster

        if (z == 0) {
            mcdi->alloc_offset = j;
            mcdi->rootdir_cluster = 0;
            fc[0] = 0xffffffff; // marking rootdir end
        }
        z += sz_u32;

        r = mc_writecluster(genvmc_fh, fat_index++, cluster_buf, 1);
        if (r < 0) {
            if (r == -2) // it's user abort
                r = -1000;
            else
                r = -107;
            goto err_out;
        }
    }

    // calculate alloc_end
    mcdi->alloc_end = (i * mcdi->clusters_per_block) - mcdi->alloc_offset;

    // just a security...
    if (z < mcdi->clusters_per_block) {
        r = -108;
        goto err_out;
    }

    mcdi->unknown1 = 0;
    mcdi->unknown2 = 0;
    mcdi->unknown5 = -1;
    mcdi->rootdir_cluster2 = mcdi->rootdir_cluster;

    // build root directory
    McFsEntry *rootdir_entry[2];
    sceMcStDateTime time;

    mc_getmcrtime(&time);
    rootdir_entry[0] = (McFsEntry *)&cluster_buf[0];
    rootdir_entry[1] = (McFsEntry *)&cluster_buf[sizeof(McFsEntry)];
    memset((void *)rootdir_entry[0], 0, sizeof(McFsEntry));
    memset((void *)rootdir_entry[1], 0, sizeof(McFsEntry));
    rootdir_entry[0]->mode = sceMcFileAttrExists | sceMcFile0400 | sceMcFileAttrSubdir | sceMcFileAttrReadable | sceMcFileAttrWriteable | sceMcFileAttrExecutable;
    rootdir_entry[0]->length = 2;
    memcpy((void *)&rootdir_entry[0]->created, (void *)&time, sizeof(sceMcStDateTime));
    memcpy((void *)&rootdir_entry[0]->modified, (void *)&time, sizeof(sceMcStDateTime));
    rootdir_entry[0]->cluster = 0;
    rootdir_entry[0]->dir_entry = 0;
    strcpy(rootdir_entry[0]->name, ".");
    rootdir_entry[1]->mode = sceMcFileAttrExists | sceMcFileAttrHidden | sceMcFile0400 | sceMcFileAttrSubdir | sceMcFileAttrWriteable | sceMcFileAttrExecutable;
    rootdir_entry[1]->length = 2;
    memcpy((void *)&rootdir_entry[1]->created, (void *)&time, sizeof(sceMcStDateTime));
    memcpy((void *)&rootdir_entry[1]->modified, (void *)&time, sizeof(sceMcStDateTime));
    rootdir_entry[1]->cluster = 0;
    rootdir_entry[1]->dir_entry = 0;
    strcpy(rootdir_entry[1]->name, "..");

    // write root directory cluster
    strcpy(msg, "Writing root directory cluster...");
    r = mc_writecluster(genvmc_fh, mcdi->alloc_offset + mcdi->rootdir_cluster, cluster_buf, 1);
    if (r < 0) {
        if (r == -2) // it's user abort
            r = -1000;
        else
            r = -109;
        goto err_out;
    }

    // set superblock formatted flag
    mcdi->cardform = 1;

    // finally write superblock
    strcpy(msg, "Writing superblock...");
    memset(cluster_buf, 0xff, mcdi->cluster_size);
    memcpy(cluster_buf, (void *)mcdi, sizeof(MCDevInfo));
    r = mc_writecluster(genvmc_fh, 0, cluster_buf, 1);
    if (r < 0) {
        if (r == -2) // it's user abort
            r = -1000;
        else
            r = -110;
        goto err_out;
    }

    close(genvmc_fh);

    *progress = 100;

    return 0;


err_out:
    close(genvmc_fh);

    return r;
}

//--------------------------------------------------------------
static int genvmc_dummy(void)
{
    return -EPERM;
}

//--------------------------------------------------------------
static int genvmc_init(iop_device_t *dev)
{
    genvmc_io_sema = CreateMutex(IOP_MUTEX_UNLOCKED);
    genvmc_thread_sema = CreateMutex(IOP_MUTEX_UNLOCKED);
    genvmc_abort_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

    return 0;
}

//--------------------------------------------------------------
static int genvmc_deinit(iop_device_t *dev)
{
    DeleteSema(genvmc_io_sema);
    DeleteSema(genvmc_thread_sema);
    DeleteSema(genvmc_abort_sema);

    return 0;
}

//--------------------------------------------------------------
static void VMC_create_thread(void *args)
{
    register int r;
    createVMCparam_t *param = (createVMCparam_t *)args;

    WaitSema(genvmc_thread_sema);

    WaitSema(genvmc_abort_sema);
    genvmc_abort = 0;
    SignalSema(genvmc_abort_sema);

    genvmc_stats.VMC_status = GENVMC_STAT_BUSY;
    genvmc_stats.VMC_error = 0;
    genvmc_stats.VMC_progress = 0;
    strcpy(genvmc_stats.VMC_msg, "Initializing...");

    if (param->VMC_card_slot == -1)
        r = vmc_mcformat(param->VMC_filename, param->VMC_size_mb * 1024, param->VMC_blocksize, &genvmc_stats.VMC_progress, genvmc_stats.VMC_msg);
    else
        r = vmc_mccopy(param->VMC_filename, param->VMC_card_slot, &genvmc_stats.VMC_progress, genvmc_stats.VMC_msg);

    if (r < 0) {
        genvmc_stats.VMC_status = GENVMC_STAT_AVAIL;
        genvmc_stats.VMC_error = r;

        if (r == -1000) { // user abort
            remove(param->VMC_filename);
            strcpy(genvmc_stats.VMC_msg, "VMC file creation aborted");
            SignalSema(genvmc_abort_finished_sema);
        } else
            strcpy(genvmc_stats.VMC_msg, "Failed to format VMC file");

        goto exit;
    }

    genvmc_stats.VMC_status = GENVMC_STAT_AVAIL;
    genvmc_stats.VMC_error = 0;
    genvmc_stats.VMC_progress = 100;
    strcpy(genvmc_stats.VMC_msg, "VMC file created");

exit:
    SignalSema(genvmc_thread_sema);
    ExitDeleteThread();
}

//--------------------------------------------------------------
static int vmc_create(createVMCparam_t *param)
{
    DPRINTF("%s: vmc_create() filename=%s size_MB=%d blocksize=%d th_priority=0x%02x slot=%d\n", MODNAME,
            param->VMC_filename, param->VMC_size_mb, param->VMC_blocksize, param->VMC_thread_priority, param->VMC_card_slot);

    register int r, thid;
    iop_thread_t thread_param;

    thread_param.attr = TH_C;
    thread_param.option = 0;
    thread_param.thread = (void *)VMC_create_thread;
    thread_param.stacksize = 0x2000;
    thread_param.priority = (param->VMC_thread_priority < 0x0f) ? 0x0f : param->VMC_thread_priority;

    // creating VMC create thread
    thid = CreateThread(&thread_param);
    if (thid < 0)
        return -1;

    // starting VMC create thread
    r = StartThread(thid, (void *)param);
    if (r < 0)
        return -2;

    return 0;
}

//--------------------------------------------------------------
static int vmc_abort(void)
{
    WaitSema(genvmc_abort_sema);

    DPRINTF("%s: vmc_abort()\n", MODNAME);

    genvmc_abort = 1;
    SignalSema(genvmc_abort_sema);

    genvmc_abort_finished_sema = CreateMutex(IOP_MUTEX_LOCKED);
    WaitSema(genvmc_abort_finished_sema);
    DeleteSema(genvmc_abort_finished_sema);

    return 0;
}

//--------------------------------------------------------------
static int vmc_status(statusVMCparam_t *param)
{
    // copy global genvmc stats to output param
    memcpy((void *)param, (void *)&genvmc_stats, sizeof(statusVMCparam_t));

    return 0;
}

//--------------------------------------------------------------
static int genvmc_devctl(iop_file_t *f, const char *name, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen)
{
    register int r = 0;

    if (!name)
        return -ENOENT;

    WaitSema(genvmc_io_sema);

    switch (cmd) {

        // VMC file creation request command
        case GENVMC_DEVCTL_CREATE_VMC:
            r = vmc_create((createVMCparam_t *)args);
            if (r < 0)
                r = -EIO;
            break;

        // VMC file creation abort command
        case GENVMC_DEVCTL_ABORT:
            r = vmc_abort();
            if (r < 0)
                r = -EIO;
            break;

        // VMC file creation status command
        case GENVMC_DEVCTL_STATUS:
            r = vmc_status((statusVMCparam_t *)buf);
            if (r < 0)
                r = -EIO;
            break;

        default:
            r = -EINVAL;
    }

    SignalSema(genvmc_io_sema);

    return r;
}

//--------------------------------------------------------------
int _start(int argc, char **argv)
{
    DPRINTF("%s start!\n", MODNAME);

    DelDrv("genvmc");

    if (AddDrv((iop_device_t *)&genvmc_dev) < 0)
        return MODULE_NO_RESIDENT_END;

    return MODULE_RESIDENT_END;
}
