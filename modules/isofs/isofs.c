#include <errno.h>
#include <iomanX.h>
#include <limits.h>
#include <stdio.h>
#include <thsemap.h>
#include <loadcore.h>
#include <sysclib.h>

#include <irx.h>

#define DPRINTF(args...) //printf(args)

struct dirTocEntry
{
    short length;
    u32 fileLBA;         // 2
    u32 fileLBA_bigend;  // 6
    u32 fileSize;        // 10
    u32 fileSize_bigend; // 14
    u8 dateStamp[6];     // 18
    u8 reserved1;        // 24
    u8 fileProperties;   // 25
    u8 reserved2[6];     // 26
    u8 filenameLength;   // 32
    char filename[128];  // 33
} __attribute__((packed));

typedef struct
{
    u32 lsn;
    u32 size;
    char name[16];
    u8 date[8];
} cd_file_t;

typedef struct
{
    iop_file_t *f;
    u32 lsn;
    unsigned int filesize;
    unsigned int position;
} FHANDLE;

#define MAX_FDHANDLES 4

typedef struct
{
    u32 maxLBA;
    u32 rootDirtocLBA;
    u32 rootDirtocLength;
} layer_info_t;

struct MountData
{
    int fd;
    int sema;
    layer_info_t layer_info[2];
    FHANDLE cdvdman_fdhandles[MAX_FDHANDLES];
};

static struct MountData MountPoint;
static unsigned char cdvdman_buf[2048];

static int IsofsUnsupported(void)
{
    return -1;
}

static int IsofsInit(iop_device_t *device)
{
    iop_sema_t SemaData;

    SemaData.attr = 0;
    SemaData.option = 0;
    SemaData.initial = 1;
    SemaData.max = 1;
    MountPoint.sema = CreateSema(&SemaData);
    MountPoint.fd = -1;

    return 0;
}

static FHANDLE *cdvdman_getfilefreeslot(void)
{
    int i;
    FHANDLE *fh;

    for (i = 0; i < MAX_FDHANDLES; i++) {
        fh = (FHANDLE *)&MountPoint.cdvdman_fdhandles[i];
        if (fh->f == NULL)
            return fh;
    }

    return 0;
}

static int unmount(struct MountData *pMountData)
{
    if (pMountData->fd >= 0) {
        close(pMountData->fd);
        pMountData->fd = -1;
    }
    return 0;
}

static int IsofsDeinit(iop_device_t *device)
{
    WaitSema(MountPoint.sema);

    unmount(&MountPoint);

    DeleteSema(MountPoint.sema);

    return 0;
}

//-------------------------------------------------------------------------
static void longLseek(int fd, unsigned int lba)
{
    unsigned int remaining, toSeek;

    if (lba > INT_MAX / 2048) {
        lseek(fd, INT_MAX / 2048 * 2048, SEEK_SET);

        remaining = lba - INT_MAX / 2048;
        while(remaining > 0) {
            toSeek = remaining > INT_MAX / 2048 ? INT_MAX / 2048 : remaining;
            lseek(fd, toSeek * 2048, SEEK_CUR);
            remaining -= toSeek;
        }
    } else {
        lseek(fd, lba * 2048, SEEK_SET);
    }
}

//-------------------------------------------------------------------------
static int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start)
{
    if (MountPoint.layer_info[1].maxLBA > 0) {
        *layer1_start = MountPoint.layer_info[0].maxLBA;
        *on_dual = 1;
    } else {
        *layer1_start = 0;
        *on_dual = 0;
    }

    return 1;
}

//-------------------------------------------------------------------------
static int cdEmuRead(u32 lsn, unsigned int count, void *buffer)
{
    longLseek(MountPoint.fd, lsn);

    return (read(MountPoint.fd, buffer, count * 2048) == count * 2048 ? 0 : -EIO);
}

//-------------------------------------------------------------------------
static void cdvdman_trimspaces(char *str)
{
    int i, len;
    char *p;

    len = strlen(str);
    if (len == 0)
        return;

    i = len - 1;

    for (i = len - 1; i != -1; i--) {
        p = &str[i];
        if ((*p != 0x20) && (*p != 0x2e))
            break;
        *p = 0;
    }
}

static struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer)
{
    char cdvdman_dirname[40]; // Maximum 30 characters, the '.', the ";" and the version number (1 - 32767)
    char *p = (char *)name, *p_tmp;
    char *slash;
    int r, len, filename_len, i;
    int tocPos;
    struct dirTocEntry *tocEntryPointer;

lbl_startlocate:
    DPRINTF("cdvdman_locatefile start locating, layer=%d\n", layer);

    while (*p == '/')
        p++;

    while (*p == '\\')
        p++;

    slash = strchr(p, '/');

    // if the path doesn't contain a '/' then look for a '\'
    if (!slash)
        slash = strchr(p, '\\');

    len = (u32)slash - (u32)p;

    // if a slash was found
    if (slash != NULL) {

        if (len >= sizeof(cdvdman_dirname))
            return NULL;

        // copy the path into main 'dir' var
        strncpy(cdvdman_dirname, p, len);
        cdvdman_dirname[len] = 0;
    } else {
        if (strlen(p) >= sizeof(cdvdman_dirname))
            return NULL;

        strcpy(cdvdman_dirname, p);

        //Correct filenames (for files), if necessary.
        if ((p_tmp = strchr(cdvdman_dirname, '.')) != NULL) {
            for (i = 0, p_tmp++; i < 3 && (*p_tmp != '\0'); i++, p_tmp++) {
                if (p_tmp[0] == ';')
                    break;
            }

            if (p_tmp - cdvdman_dirname + 3 < sizeof(cdvdman_dirname)) {
                p_tmp[0] = ';';
                p_tmp[1] = '1';
                p_tmp[2] = '\0';
            }
        }
    }

    while (tocLength > 0) {
        cdEmuRead(tocLBA, 1, cdvdman_buf);
        DPRINTF("cdvdman_locatefile tocLBA read done\n");

        tocLength -= 2048;
        tocLBA++;

        tocPos = 0;
        do {
            tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[tocPos];

            if (tocEntryPointer->length == 0)
                break;

            filename_len = tocEntryPointer->filenameLength;
            if (filename_len) {
                r = strcmp(cdvdman_dirname, tocEntryPointer->filename);
                if ((!r) && (!slash)) { // we searched a file so it's found
                    DPRINTF("cdvdman_locatefile found file! LBA=%d size=%d\n", (int)tocEntryPointer->fileLBA, (int)tocEntryPointer->fileSize);
                    return tocEntryPointer;
                } else if ((!r) && (tocEntryPointer->fileProperties & 2)) { // we found it but it's a directory
                    tocLBA = tocEntryPointer->fileLBA;
                    tocLength = tocEntryPointer->fileSize;
                    p = &slash[1];

                    int on_dual;
                    u32 layer1_start;
                    sceCdReadDvdDualInfo(&on_dual, &layer1_start);

                    if (layer)
                        tocLBA += layer1_start;

                    goto lbl_startlocate;
                } else {
                    tocPos += (tocEntryPointer->length << 16) >> 16;
                }
            }
        } while (tocPos < 2016);
    }

    DPRINTF("cdvdman_locatefile file not found!!!\n");

    return NULL;
}

static int cdvdman_findfile(cd_file_t *pcdfile, const char *name, int layer)
{
    static char cdvdman_filepath[256];
    u32 lsn;
    struct dirTocEntry *tocEntryPointer;

    if ((!pcdfile) || (!name))
        return 0;

    DPRINTF("cdvdman_findfile %s layer%d\n", name, layer);

    strncpy(cdvdman_filepath, name, sizeof(cdvdman_filepath));
    cdvdman_filepath[sizeof(cdvdman_filepath) - 1] = '\0';
    cdvdman_trimspaces(cdvdman_filepath);

    DPRINTF("cdvdman_findfile cdvdman_filepath=%s\n", cdvdman_filepath);

    if (layer < 2) {
        if (MountPoint.layer_info[layer].rootDirtocLBA == 0) {
            return 0;
        }

        tocEntryPointer = cdvdman_locatefile(cdvdman_filepath, MountPoint.layer_info[layer].rootDirtocLBA, MountPoint.layer_info[layer].rootDirtocLength, layer);
        if (tocEntryPointer == NULL) {
            return 0;
        }

        lsn = tocEntryPointer->fileLBA;
        if (layer) {
            sceCdReadDvdDualInfo((int *)&pcdfile->lsn, &pcdfile->size);
            lsn += pcdfile->size;
        }

        pcdfile->lsn = lsn;
        pcdfile->size = tocEntryPointer->fileSize;

        strcpy(pcdfile->name, strrchr(name, '\\') + 1);
    } else {
        return 0;
    }

    DPRINTF("cdvdman_findfile found %s\n", name);

    return 1;
}

static int IsofsOpen(iop_file_t *f, const char *name, int flags, int mode)
{
    int r = 0;
    FHANDLE *fh;
    cd_file_t cdfile;

    if (MountPoint.fd < 0)
        return -EBADFD;

    WaitSema(MountPoint.sema);

    DPRINTF("isofs_open %s mode=%d\n", name, mode);

    fh = cdvdman_getfilefreeslot();
    if (fh) {
        r = cdvdman_findfile(&cdfile, name, f->unit);
        if (r) {
            f->privdata = fh;
            fh->f = f;

            if (f->mode == 0)
                f->mode = r;

            fh->filesize = cdfile.size;
            fh->lsn = cdfile.lsn;
            fh->position = 0;
            r = 0;

        } else
            r = -ENOENT;
    } else
        r = -EMFILE;

    DPRINTF("isofs_open ret=%d lsn=%d size=%d\n", r, (int)fh->lsn, (int)fh->filesize);

    SignalSema(MountPoint.sema);

    return r;
}

static int IsofsClose(iop_file_t *f)
{
    FHANDLE *fh = (FHANDLE *)f->privdata;

    WaitSema(MountPoint.sema);

    DPRINTF("isofs_close\n");

    if (fh)
        memset(fh, 0, sizeof(FHANDLE));

    SignalSema(MountPoint.sema);

    return 0;
}

static int IsofsRead(iop_file_t *f, void *buf, int size)
{
    static unsigned char cdvdman_fs_buf[2048];
    FHANDLE *fh = (FHANDLE *)f->privdata;
    unsigned int offset, nsectors, nbytes;
    int rpos;

    WaitSema(MountPoint.sema);

    DPRINTF("isofs_read size=%d file_position=%d\n", size, fh->position);

    if ((fh->position + size) > fh->filesize)
        size = fh->filesize - fh->position;

    rpos = 0;
    if (size > 0) {
        //Phase 1: read data until the offset of the file is nicely aligned to a 2048-byte boundary.
        if ((offset = fh->position % 2048) != 0) {
            nbytes = 2048 - offset;
            if (size < nbytes)
                nbytes = size;
            cdEmuRead(fh->lsn + (fh->position / 2048), 1, cdvdman_fs_buf);

            fh->position += nbytes;
            size -= nbytes;
            rpos += nbytes;

            memcpy(buf, &cdvdman_fs_buf[offset], nbytes);
            buf += nbytes;
        }

        //Phase 2: read the data to the middle of the buffer, in units of 2048.
        if ((nsectors = size / 2048) > 0) {
            nbytes = nsectors * 2048;

            cdEmuRead(fh->lsn + (fh->position / 2048), nsectors, buf);

            buf += nbytes;
            size -= nbytes;
            fh->position += nbytes;
            rpos += nbytes;
        }

        //Phase 3: read any remaining data that isn't divisible by 2048.
        if ((nbytes = size) > 0) {
            cdEmuRead(fh->lsn + (fh->position / 2048), 1, cdvdman_fs_buf);

            size -= nbytes;
            fh->position += nbytes;
            rpos += nbytes;

            memcpy(buf, cdvdman_fs_buf, nbytes);
        }
    }

    DPRINTF("isofs_read ret=%d\n", rpos);
    SignalSema(MountPoint.sema);

    return rpos;
}

static int IsofsLseek(iop_file_t *f, int offset, int where)
{
    int r;
    FHANDLE *fh = (FHANDLE *)f->privdata;

    WaitSema(MountPoint.sema);

    DPRINTF("isofs_lseek offset=%ld where=%d\n", offset, where);

    switch (where) {
        case SEEK_CUR:
            r = fh->position + offset;
            if (r > fh->filesize) {
                r = -EINVAL;
                goto ssema;
            }
            break;
        case SEEK_SET:
            r = offset;
            if (fh->filesize < offset) {
                r = -EINVAL;
                goto ssema;
            }
            break;
        case SEEK_END:
            r = fh->filesize - offset;
            break;
        default:
            r = -EINVAL;
            goto ssema;
    }

    fh->position = r;
    if (fh->position > fh->filesize)
        fh->position = fh->filesize;

ssema:
    DPRINTF("isofs_lseek file offset=%d\n", (int)fh->position);
    SignalSema(MountPoint.sema);

    return r;
}

static int ProbeISO9660(int fd, unsigned int sector, layer_info_t *layer_info)
{
    int result;

    longLseek(fd, sector);
    if (read(fd, cdvdman_buf, 2048) == 2048) {
        if ((cdvdman_buf[0x00] == 1) && (!strncmp(&cdvdman_buf[0x01], "CD001", 5))) {
            struct dirTocEntry *tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];

            layer_info->maxLBA = *(u32 *)&cdvdman_buf[0x50];
            layer_info->rootDirtocLBA = tocEntryPointer->fileLBA;
            layer_info->rootDirtocLength = tocEntryPointer->length;

            result = 0;
        } else {
            result = EINVAL;
        }
    } else {
        result = EIO;
    }

    return result;
}

static int IsofsMount(iop_file_t *f, const char *fsname, const char *devname, int flags, void *arg, int arglen)
{
    int fd, result;

    if (MountPoint.fd >= 0)
        return -EBUSY;

    WaitSema(MountPoint.sema);

    memset(&MountPoint, 0, sizeof(struct MountData));

    if ((fd = open(devname, O_RDONLY)) >= 0) {
        if ((result = ProbeISO9660(fd, 16, &MountPoint.layer_info[0])) == 0)
            MountPoint.fd = fd;
        else
            close(fd);
    } else {
        result = fd;
    }

    SignalSema(MountPoint.sema);

    return result;
}

static int IsofsUmount(iop_file_t *f, const char *fsname)
{
    int result;

    if (MountPoint.fd < 0)
        return -EBADFD;

    WaitSema(MountPoint.sema);

    result = unmount(&MountPoint);

    SignalSema(MountPoint.sema);

    return result;
}

static iop_device_ops_t IsofsDeviceOps = {
    &IsofsInit,
    &IsofsDeinit,
    (void *)&IsofsUnsupported,
    &IsofsOpen,
    &IsofsClose,
    &IsofsRead,
    (void *)&IsofsUnsupported,
    &IsofsLseek,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    &IsofsMount,
    &IsofsUmount,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported,
    (void *)&IsofsUnsupported};

static iop_device_t IsofsDevice = {
    "iso",
    IOP_DT_FSEXT | IOP_DT_FS,
    1,
    "ISOFS",
    &IsofsDeviceOps};


int _start(int argc, char *argv[])
{
    if (AddDrv(&IsofsDevice) == 0) {
        printf("ISOFS driver start.\n");
        return MODULE_RESIDENT_END;
    }

    return MODULE_NO_RESIDENT_END;
}
