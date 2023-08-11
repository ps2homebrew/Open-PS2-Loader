/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

typedef struct
{
    iop_file_t *f;
    u32 lsn;
    unsigned int filesize;
    unsigned int position;
} FHANDLE;

#define MAX_FDHANDLES 64
FHANDLE cdvdman_fdhandles[MAX_FDHANDLES];

#define CDVDFSV_ALIGNMENT 64
static u8 cdvdman_fs_buf[CDVDMAN_FS_SECTORS * 2048 + CDVDFSV_ALIGNMENT];

// for "cdrom" ioctl2
#define CIOCSTREAMPAUSE  0x630D
#define CIOCSTREAMRESUME 0x630E
#define CIOCSTREAMSTAT   0x630F

// driver ops protypes
static int cdrom_dummy(void);
static s64 cdrom_dummy64(void);
static int cdrom_init(iop_device_t *dev);
static int cdrom_deinit(iop_device_t *dev);
static int cdrom_open(iop_file_t *f, const char *filename, int mode);
static int cdrom_close(iop_file_t *f);
static int cdrom_read(iop_file_t *f, void *buf, int size);
static int cdrom_lseek(iop_file_t *f, int offset, int where);
static int cdrom_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat);
static int cdrom_dopen(iop_file_t *f, const char *dirname);
static int cdrom_dread(iop_file_t *f, iox_dirent_t *dirent);
static int cdrom_ioctl(iop_file_t *f, u32 cmd, void *args);
static int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen);
static int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen);

// driver ops func tab
static struct _iop_ext_device_ops cdrom_ops = {
    &cdrom_init,
    &cdrom_deinit,
    (void *)&cdrom_dummy,
    &cdrom_open,
    &cdrom_close,
    &cdrom_read,
    (void *)&cdrom_dummy,
    &cdrom_lseek,
    &cdrom_ioctl,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    &cdrom_dopen,
    &cdrom_close, // dclose -> close
    &cdrom_dread,
    &cdrom_getstat,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy64,
    (void *)&cdrom_devctl,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    &cdrom_ioctl2};

// driver descriptor
static iop_ext_device_t cdrom_dev = {
    "cdrom",
    IOP_DT_FS | IOP_DT_FSEXT,
    1,
    "CD-ROM ",
    &cdrom_ops};

static unsigned char fs_inited = 0;

//--------------------------------------------------------------
void cdvdman_fs_init(void)
{
    if (fs_inited)
        return;

    DPRINTF("cdvdman_fs_init\n");

    DeviceFSInit();

    memset(&cdvdman_fdhandles[0], 0, MAX_FDHANDLES * sizeof(FHANDLE));

    cdvdman_searchfile_init();

    fs_inited = 1;
}

//--------------------------------------------------------------
static FHANDLE *cdvdman_getfilefreeslot(void)
{
    register int i;
    FHANDLE *fh;

    for (i = 0; i < MAX_FDHANDLES; i++) {
        fh = (FHANDLE *)&cdvdman_fdhandles[i];
        if (fh->f == NULL)
            return fh;
    }

    return 0;
}

//--------------------------------------------------------------
static int cdvdman_open(iop_file_t *f, const char *filename, int mode)
{
    int r = 0;
    FHANDLE *fh;
    sceCdlFILE cdfile;

    WaitSema(cdrom_io_sema);
    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    cdvdman_init();

    if (f->unit < 2) {
        fh = cdvdman_getfilefreeslot();
        if (fh) {
            r = sceCdLayerSearchFile(&cdfile, filename, f->unit);
            if (r) {
                f->privdata = fh;
                fh->f = f;
                fh->filesize = cdfile.size;
                fh->lsn = cdfile.lsn;
                fh->position = 0;
                r = 0;

                DPRINTF("open ret=%d lsn=%d size=%d\n", r, (int)fh->lsn, (int)fh->filesize);
            } else
                r = -ENOENT;
        } else
            r = -EMFILE;
    } else
        r = -ENOENT;

    SignalSema(cdrom_io_sema);

    return r;
}

// SCE does this too, hence assuming that the version suffix will be either totally there or absent. The only version supported is 1.
// Instead of using strcat like the original, append the version suffix manually for efficiency.
static int cdrom_purifyPath(char *path)
{
    int len;

    len = strlen(path);

    // Adjusted to better handle cases. Was adding ;1 on every case no matter what.

    if (len >= 3) {
        // Path is already valid.
        if ((path[len - 2] == ';') && (path[len - 1] == '1')) {
            return 1;
        }

        // Path is missing only version.
        if (path[len - 1] == ';') {
            path[len] = '1';
            path[len + 1] = '\0';
            return 0;
        }

        // Path has no terminator or version at all.
        path[len] = ';';
        path[len + 1] = '1';
        path[len + 2] = '\0';

        return 0;
    }

    return 1;
}

//--------------------------------------------------------------
static int cdrom_dummy(void)
{
    return -EPERM;
}

static s64 cdrom_dummy64(void)
{
    return -EPERM;
}

//--------------------------------------------------------------
static int cdrom_init(iop_device_t *dev)
{
    iop_sema_t smp;

    DPRINTF("cdrom_init\n");

    smp.initial = 1;
    smp.max = 1;
    smp.attr = 1;
    smp.option = 0;

    cdrom_io_sema = CreateSema(&smp);
    cdvdman_searchfilesema = CreateSema(&smp);

    return 0;
}

//--------------------------------------------------------------
static int cdrom_deinit(iop_device_t *dev)
{
    DPRINTF("cdrom_deinit\n");

    DeleteSema(cdrom_io_sema);
    DeleteSema(cdvdman_searchfilesema);

    return 0;
}

static int cdrom_open(iop_file_t *f, const char *filename, int mode)
{
    int result;
    char path_buffer[128]; // Original buffer size in the SCE CDVDMAN module.

    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    DPRINTF("cdrom_open %s mode=%d layer %d\n", filename, mode, f->unit);

    strncpy(path_buffer, filename, sizeof(path_buffer));
    cdrom_purifyPath(path_buffer);

    if ((result = cdvdman_open(f, path_buffer, mode)) >= 0)
        f->mode = O_RDONLY; // SCE fixes the open flags to O_RDONLY for open().

    return result;
}

//--------------------------------------------------------------
static int cdrom_close(iop_file_t *f)
{
    FHANDLE *fh = (FHANDLE *)f->privdata;

    WaitSema(cdrom_io_sema);
    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    DPRINTF("cdrom_close\n");

    memset(fh, 0, sizeof(FHANDLE));
    f->mode = 0; // SCE invalidates FDs by clearing the open flags.

    SignalSema(cdrom_io_sema);

    return 0;
}

//--------------------------------------------------------------
static int cdrom_read(iop_file_t *f, void *buf, int size)
{
    FHANDLE *fh = (FHANDLE *)f->privdata;
    unsigned int offset, nsectors, nbytes;
    int rpos;

    WaitSema(cdrom_io_sema);
    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    DPRINTF("cdrom_read size=%db (%ds) file_position=%d\n", size, size / 2048, fh->position);

    if ((fh->position + size) > fh->filesize)
        size = fh->filesize - fh->position;

    rpos = 0;
    if (size > 0) {
        // Phase 1: read data until the offset of the file is nicely aligned to a 2048-byte boundary.
        if ((offset = fh->position % 2048) != 0) {
            nbytes = 2048 - offset;
            if (size < nbytes)
                nbytes = size;
            while (sceCdRead(fh->lsn + (fh->position / 2048), 1, cdvdman_fs_buf, NULL) == 0)
                DelayThread(10000);

            fh->position += nbytes;
            size -= nbytes;
            rpos += nbytes;

            sceCdSync(0);
            memcpy(buf, &cdvdman_fs_buf[offset], nbytes);
            buf = (void *)((u8 *)buf + nbytes);
        }

        // Phase 2: read the data to the middle of the buffer, in units of 2048.
        if ((nsectors = size / 2048) > 0) {
            nbytes = nsectors * 2048;

            while (sceCdRead(fh->lsn + (fh->position / 2048), nsectors, buf, NULL) == 0)
                DelayThread(10000);

            buf += nbytes;
            size -= nbytes;
            fh->position += nbytes;
            rpos += nbytes;

            sceCdSync(0);
        }

        // Phase 3: read any remaining data that isn't divisible by 2048.
        if ((nbytes = size) > 0) {
            while (sceCdRead(fh->lsn + (fh->position / 2048), 1, cdvdman_fs_buf, NULL) == 0)
                DelayThread(10000);

            fh->position += nbytes;
            rpos += nbytes;

            sceCdSync(0);
            memcpy(buf, cdvdman_fs_buf, nbytes);
        }
    }

    DPRINTF("cdrom_read ret=%d\n", rpos);
    SignalSema(cdrom_io_sema);

    return rpos;
}

//--------------------------------------------------------------
static int cdrom_lseek(iop_file_t *f, int offset, int where)
{
    int r;
    FHANDLE *fh = (FHANDLE *)f->privdata;

    WaitSema(cdrom_io_sema);
    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    DPRINTF("cdrom_lseek offset=%d where=%d\n", offset, where);

    switch (where) {
        case SEEK_CUR:
            r = fh->position += offset;
            break;
        case SEEK_SET:
            r = fh->position = offset;
            break;
        case SEEK_END:
            r = fh->position = fh->filesize - offset;
            break;
        default:
            r = fh->position;
    }

    if (fh->position > fh->filesize)
        r = fh->position = fh->filesize;

    DPRINTF("cdrom_lseek file offset=%d\n", fh->position);
    SignalSema(cdrom_io_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat)
{
    char path_buffer[128]; // Original buffer size in the SCE CDVDMAN module.

    DPRINTF("cdrom_getstat %s layer %d\n", filename, f->unit);
    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    strncpy(path_buffer, filename, sizeof(path_buffer));
    cdrom_purifyPath(path_buffer); // Unlike the SCE original, purify the path right away.

    return sceCdLayerSearchFile((sceCdlFILE *)&stat->attr, path_buffer, f->unit) - 1;
}

//--------------------------------------------------------------
static int cdrom_dopen(iop_file_t *f, const char *dirname)
{
    DPRINTF("cdrom_dopen %s layer %d\n", dirname, f->unit);

    return cdvdman_open(f, dirname, 8);
}

//--------------------------------------------------------------
static int cdrom_dread(iop_file_t *f, iox_dirent_t *dirent)
{
    register int r = 0;
    register u32 mode;
    FHANDLE *fh = (FHANDLE *)f->privdata;
    struct dirTocEntry *tocEntryPointer;

    WaitSema(cdrom_io_sema);
    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    DPRINTF("cdrom_dread fh->lsn=%lu\n", fh->lsn);

    if ((r = sceCdRead(fh->lsn, 1, cdvdman_fs_buf, NULL)) == 1) {
        sceCdSync(0);

        do {
            r = 0;
            tocEntryPointer = (struct dirTocEntry *)&cdvdman_fs_buf[fh->position];
            if (tocEntryPointer->length == 0)
                break;

            fh->position += tocEntryPointer->length;
            r = 1;
        } while (tocEntryPointer->filenameLength == 1);

        mode = 0x2124;
        if (tocEntryPointer->fileProperties & 2)
            mode = 0x116d;

        dirent->stat.mode = mode;
        dirent->stat.size = tocEntryPointer->fileSize;
        strncpy(dirent->name, tocEntryPointer->filename, tocEntryPointer->filenameLength);
        dirent->name[tocEntryPointer->filenameLength] = '\0';

        DPRINTF("cdrom_dread r=%d mode=%04x name=%s\n", r, (int)mode, dirent->name);
    } else
        DPRINTF("cdrom_dread r=%d\n", r);

    SignalSema(cdrom_io_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_ioctl(iop_file_t *f, u32 cmd, void *args)
{
    register int r = 0;

    DPRINTF("cdrom_ioctl 0x%X\n", cmd);

    WaitSema(cdrom_io_sema);

    if (cmd != 0x10000) // Spin Ctrl op
        r = -EINVAL;

    SignalSema(cdrom_io_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen)
{
    int result;

    WaitSema(cdrom_io_sema);
    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    result = 0;
    switch (cmd) {
        case CDIOC_READCLOCK:
            result = sceCdReadClock((sceCdCLOCK *)buf);
            if (result != 1)
                result = -EIO;
            break;
        case CDIOC_READGUID:
            result = sceCdReadGUID(buf);
            break;
        case CDIOC_READDISKGUID:
            result = sceCdReadDiskID(buf);
            break;
        case CDIOC_GETDISKTYPE:
            *(int *)buf = sceCdGetDiskType();
            break;
        case CDIOC_GETERROR:
            *(int *)buf = sceCdGetError();
            break;
        case CDIOC_TRAYREQ:
            result = sceCdTrayReq(*(int *)args, (u32 *)buf);
            if (result != 1)
                result = -EIO;
            break;
        case CDIOC_STATUS:
            *(int *)buf = sceCdStatus();
            break;
        case CDIOC_POWEROFF:
            result = sceCdPowerOff((u32 *)args);
            if (result != 1)
                result = -EIO;
            break;
        case CDIOC_MMODE:
            result = 1;
            break;
        case CDIOC_DISKRDY:
            *(int *)buf = sceCdDiskReady(*(int *)args);
            break;
        case CDIOC_READMODELID:
            result = sceCdReadModelID(buf);
            break;
        case CDIOC_STREAMINIT:
            result = sceCdStInit(((u32 *)args)[0], ((u32 *)args)[1], (void *)((u32 *)args)[2]);
            break;
        case CDIOC_BREAK:
            result = sceCdBreak();
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_SPINNOM:
        case CDIOC_SPINSTM:
        case CDIOC_TRYCNT:
        case CDIOC_READDVDDUALINFO:
        case CDIOC_INIT:
            result = 0;
            break;
        case CDIOC_STANDBY:
            result = sceCdStandby();
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_STOP:
            result = sceCdStop();
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_PAUSE:
            result = sceCdPause();
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_GETTOC:
            result = sceCdGetToc(buf);
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_GETINTREVENTFLG:
            *(int *)buf = cdvdman_stat.intr_ef;
            result = cdvdman_stat.intr_ef;
            break;
        default:
            DPRINTF("cdrom_devctl unknown, cmd=0x%X\n", cmd);
            result = -EIO;
            break;
    }

    SignalSema(cdrom_io_sema);

    return result;
}

//--------------------------------------------------------------
static int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen)
{
    int r = 0;

    // There was a check here on whether the file was opened with mode 8.

    WaitSema(cdrom_io_sema);
    WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    switch (cmd) {
        case CIOCSTREAMPAUSE:
            r = sceCdStPause();
            break;
        case CIOCSTREAMRESUME:
            r = sceCdStResume();
            break;
        case CIOCSTREAMSTAT:
            r = sceCdStStat();
            break;
        default:
            DPRINTF("cdrom_ioctl2 unknown, cmd=0x%X\n", cmd);
            r = -EINVAL;
    }

    SignalSema(cdrom_io_sema);

    return r;
}

//-------------------------------------------------------------------------
void *sceGetFsvRbuf(void)
{
    return cdvdman_fs_buf;
}

//-------------------------------------------------------------------------
void cdvdman_initdev(void)
{
    iop_event_t event;

    event.attr = EA_MULTI;
    event.option = 0;
    event.bits = 1;

    cdvdman_stat.intr_ef = CreateEventFlag(&event);

    DelDrv("cdrom");
    AddDrv((iop_device_t *)&cdrom_dev);
}
