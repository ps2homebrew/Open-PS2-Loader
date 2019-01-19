/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "smsutils.h"
#include "mass_common.h"
#include "mass_stor.h"
#include "dev9.h"
#include "oplsmb.h"
#include "smb.h"
#include "atad.h"
#include "ioplib_util.h"
#include "cdvdman.h"
#include "internal.h"
#include "cdvd_config.h"
#include "device.h"

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <ioman.h>
#include <thsemap.h>
#include <errno.h>
#include <io_common.h>
#include <usbd.h>
#include "ioman_add.h"

/*	Some modules (e.g. SMAP) will check for dev9 and its version number.
	In the interest of maintaining network support of games in HDD mode,
	the module ID of CDVDMAN is changed to DEV9.
	SMB mode will never support network support in games, as the SMAP interface cannot be shared.	*/
#ifdef HDD_DRIVER
#define MODNAME "dev9"
IRX_ID(MODNAME, 2, 8);
#else
#define MODNAME "cdvd_driver"
IRX_ID(MODNAME, 1, 1);
#endif

//------------------ Patch Zone ----------------------
#ifdef HDD_DRIVER
struct cdvdman_settings_hdd cdvdman_settings = {
    {0x69, 0x69,
     0x1234,
     0x39393939,
     "B00BS"},
    0x12345678};
#elif SMB_DRIVER
struct cdvdman_settings_smb cdvdman_settings = {
    {0x69, 0x69,
     0x1234,
     0x39393939,
     "B00BS"},
    "######  FILENAME  ######",
    {{"192.168.0.10",
      0x8510,
      "PS2SMB",
      "",
      "GUEST",
      ""}}};
#elif USB_DRIVER
struct cdvdman_settings_usb cdvdman_settings = {
    {0x69, 0x69,
     0x1234,
     0x39393939,
     "B00BS"},
};
#else
#error Unknown driver type. Please check the Makefile.
#endif

//----------------------------------------------------
extern struct irx_export_table _exp_cdvdman;
extern struct irx_export_table _exp_cdvdstm;
extern struct irx_export_table _exp_smsutils;
extern struct irx_export_table _exp_oplutils;
#ifdef __USE_DEV9
extern struct irx_export_table _exp_dev9;
#endif

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
    iop_file_t *f;
    u32 lsn;
    unsigned int filesize;
    unsigned int position;
} FHANDLE;

// internal functions prototypes
static void oplShutdown(int poff);
static void fs_init(void);
static void cdvdman_init(void);
static int cdvdman_readMechaconVersion(char *mname, u32 *stat);
static int cdvdman_readID(int mode, u8 *buf);
static FHANDLE *cdvdman_getfilefreeslot(void);
static void cdvdman_trimspaces(char *str);
static struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer);
static int cdvdman_findfile(cd_file_t *pcd_file, const char *name, int layer);
static int cdvdman_writeSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size);
static int cdvdman_sendSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size);
static int cdvdman_cb_event(int reason);
static unsigned int event_alarm_cb(void *args);
static void cdvdman_startThreads(void);
static void cdvdman_create_semaphores(void);
static void cdvdman_initdev(void);
static int cdvdman_read(u32 lsn, u32 sectors, void *buf);

// for "cdrom" ioctl2
#define CIOCSTREAMPAUSE 0x630D
#define CIOCSTREAMRESUME 0x630E
#define CIOCSTREAMSTAT 0x630F

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
    &cdrom_close, //dclose -> close
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

#define MAX_FDHANDLES 64
FHANDLE cdvdman_fdhandles[MAX_FDHANDLES];

typedef struct
{
    u32 rootDirtocLBA;
    u32 rootDirtocLength;
} layer_info_t;

static layer_info_t layer_info[2];

cdvdman_status_t cdvdman_stat;
static void *user_cb;

static int cdrom_io_sema;
static int cdrom_rthread_sema;
static int cdvdman_scmdsema;
static int cdvdman_searchfilesema;
static int cdvdman_ReadingThreadID;

static StmCallback_t Stm0Callback = NULL;
static iop_sys_clock_t gCallbackSysClock;

// buffers
#define CDVDMAN_BUF_SECTORS 2
static u8 cdvdman_buf[CDVDMAN_BUF_SECTORS * 2048];

#define CDVDMAN_FS_BUFSIZE CDVDMAN_FS_SECTORS * 2048
static u8 cdvdman_fs_buf[CDVDMAN_FS_BUFSIZE + 2 * 2048];

#define CDVDMAN_MODULE_VERSION 0x225
static int cdvdman_debug_print_flag = 0;

static unsigned char fs_inited = 0;
static unsigned char cdvdman_media_changed = 1;
static unsigned char sync_flag;
static unsigned char cdvdman_cdinited = 0;
static unsigned int ReadPos = 0; /* Current buffer offset in 2048-byte sectors. */

#ifdef __USE_DEV9
static int POFFThreadID;
#endif

typedef void (*oplShutdownCb_t)(void);
static oplShutdownCb_t vmcShutdownCb = NULL;

void oplRegisterShutdownCallback(oplShutdownCb_t cb)
{
    vmcShutdownCb = cb;
}

static void oplShutdown(int poff)
{
    int stat;

    DeviceLock();
    if(vmcShutdownCb != NULL)
        vmcShutdownCb();
    DeviceUnmount();
    if (poff)
    {
        DeviceStop();
#ifdef __USE_DEV9
        dev9Shutdown();
#endif
        sceCdPowerOff(&stat);
    }
}

//--------------------------------------------------------------
static void fs_init(void)
{
    if (fs_inited)
        return;

    DPRINTF("fs_init\n");

    DeviceFSInit();

    mips_memset(&cdvdman_fdhandles[0], 0, MAX_FDHANDLES * sizeof(FHANDLE));

    // Read the volume descriptor
    sceCdRead(16, 1, cdvdman_buf, NULL);
    sceCdSync(0);

    struct dirTocEntry *tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
    layer_info[0].rootDirtocLBA = tocEntryPointer->fileLBA;
    layer_info[0].rootDirtocLength = tocEntryPointer->length;

    // DVD DL support
    if (!(cdvdman_settings.common.flags & IOPCORE_COMPAT_EMU_DVDDL)) {
        int on_dual;
        u32 layer1_start;
        sceCdReadDvdDualInfo(&on_dual, &layer1_start);
        if (on_dual) {
            sceCdRead(layer1_start + 16, 1, cdvdman_buf, NULL);
            sceCdSync(0);
            tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
            layer_info[1].rootDirtocLBA = layer1_start + tocEntryPointer->fileLBA;
            layer_info[1].rootDirtocLength = tocEntryPointer->length;
        }
    }

    fs_inited = 1;
}

//-------------------------------------------------------------------------
#ifdef __USE_DEV9
static void cdvdman_poff_thread(void *arg)
{
    SleepThread();

    oplShutdown(1);
}
#endif

static void cdvdman_init(void)
{
#ifdef __USE_DEV9
    iop_thread_t ThreadData;
#endif

    if (!cdvdman_cdinited) {
        cdvdman_stat.err = CDVD_ERR_NO;

        fs_init();

#ifdef __USE_DEV9
        if (cdvdman_settings.common.flags & IOPCORE_ENABLE_POFF) {
            ThreadData.attr = TH_C;
            ThreadData.option = 0xABCD0001;
            ThreadData.priority = 1;
            ThreadData.stacksize = 0x400;
            ThreadData.thread = &cdvdman_poff_thread;
            StartThread(POFFThreadID = CreateThread(&ThreadData), NULL);
        }
#endif

        cdvdman_cdinited = 1;
    }
}

int sceCdInit(int init_mode)
{
    cdvdman_init();
    return 1;
}

//-------------------------------------------------------------------------
int sceCdStandby(void)
{
    cdvdman_stat.err = CDVD_ERR_NO;
    cdvdman_stat.status = CDVD_STAT_PAUSE;

    cdvdman_cb_event(SCECdFuncStandby);

    return 1;
}

//-------------------------------------------------------------------------
static unsigned int cdvdemu_read_end_cb(void *arg)
{
    iSetEventFlag(cdvdman_stat.intr_ef, 0x1000);
    return 0;
}

static int cdvdman_read_sectors(u32 lsn, unsigned int sectors, void *buf)
{
    unsigned int SectorsToRead, remaining;
    void *ptr;

    DPRINTF("cdvdman_read lsn=%lu sectors=%u buf=%p\n", lsn, sectors, buf);

    cdvdman_stat.err = CDVD_ERR_NO;

    for (ptr = buf, remaining = sectors; remaining > 0;) {
        if (cdvdman_stat.err != CDVD_ERR_NO)
            break;

        SectorsToRead = remaining;

        if (cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS) {
            //Limit transfers to a maximum length of 8, with a restricted transfer rate.
            iop_sys_clock_t TargetTime;

            if (SectorsToRead > 8)
                SectorsToRead = 8;

            TargetTime.hi = 0;
            TargetTime.lo = 20460 * SectorsToRead; // approximately 2KB/3600KB/s = 555us required per 2048-byte data sector at 3600KB/s, so 555 * 36.864 = 20460 ticks per sector with a 36.864MHz clock.
            ClearEventFlag(cdvdman_stat.intr_ef, ~0x1000);
            SetAlarm(&TargetTime, &cdvdemu_read_end_cb, NULL);
        }

        if (DeviceReadSectors(lsn, ptr, SectorsToRead) != 0) {
            cdvdman_stat.err = CDVD_ERR_READ;
            if (cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS)
                CancelAlarm(&cdvdemu_read_end_cb, NULL);
            break;
        }

        // PS2LOGO Decryptor algorithm; based on misfire's code (https://github.com/mlafeldt/ps2logo)
        if (lsn < 13) {
            u32 j;
            u8 *logo = (u8 *)ptr;
            u8 key = logo[0];
            if (logo[0] != 0) {
                for (j = 0; j < (SectorsToRead * 2048); j++) {
                    logo[j] ^= key;
                    logo[j] = (logo[j] << 3) | (logo[j] >> 5);
                }
            }
        }

        ptr += SectorsToRead << 11;
        remaining -= SectorsToRead;
        lsn += SectorsToRead;
        ReadPos += SectorsToRead << 11;

        if (cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS) {
            //Sleep until the required amount of time has been spent.
            WaitEventFlag(cdvdman_stat.intr_ef, 0x1000, WEF_AND, NULL);
        }
    }

    return (cdvdman_stat.err == CDVD_ERR_NO ? 0 : 1);
}

static int cdvdman_read(u32 lsn, u32 sectors, void *buf)
{
    cdvdman_stat.status = CDVD_STAT_READ;

#ifdef HDD_DRIVER //As of now, only the ATA interface requires this. We do this here to share cdvdman_buf.
    if ((u32)(buf)&3) {
        //For transfers to unaligned buffers, a double-copy is required to avoid stalling the device's DMA channel.
        WaitSema(cdvdman_searchfilesema);

        u32 nsectors, nbytes;
        u32 rpos = lsn;

        while (sectors > 0) {
            nsectors = sectors;
            if (nsectors > CDVDMAN_BUF_SECTORS)
                nsectors = CDVDMAN_BUF_SECTORS;

            cdvdman_read_sectors(rpos, nsectors, cdvdman_buf);

            rpos += nsectors;
            sectors -= nsectors;
            nbytes = nsectors << 11;

            mips_memcpy(buf, cdvdman_buf, nbytes);

            buf = (void *)(buf + nbytes);
        }

        SignalSema(cdvdman_searchfilesema);
    } else {
#endif
        cdvdman_read_sectors(lsn, sectors, buf);
#ifdef HDD_DRIVER
    }
#endif

    ReadPos = 0; /* Reset the buffer offset indicator. */

    cdvdman_stat.status = CDVD_STAT_PAUSE;

    return 1;
}

//Must be called from a thread context, with interrupts disabled.
static int cdvdman_common_lock(int IntrContext)
{
    if (sync_flag)
        return 0;

    if (IntrContext)
        iClearEventFlag(cdvdman_stat.intr_ef, ~1);
    else
        ClearEventFlag(cdvdman_stat.intr_ef, ~1);

    sync_flag = 1;

    return 1;
}

int cdvdman_AsyncRead(u32 lsn, u32 sectors, void *buf)
{
    int IsIntrContext, OldState;

    IsIntrContext = QueryIntrContext();

    CpuSuspendIntr(&OldState);

    if (!cdvdman_common_lock(IsIntrContext)) {
        CpuResumeIntr(OldState);
        DPRINTF("cdvdman_AsyncRead: exiting (sync_flag)...\n");
        return 0;
    }

    cdvdman_stat.cdread_lba = lsn;
    cdvdman_stat.cdread_sectors = sectors;
    cdvdman_stat.cdread_buf = buf;

    CpuResumeIntr(OldState);

    if (IsIntrContext)
        iSignalSema(cdrom_rthread_sema);
    else
        SignalSema(cdrom_rthread_sema);

    return 1;
}

static int cdvdman_SyncRead(u32 lsn, u32 sectors, void *buf)
{
    int IsIntrContext, OldState;

    IsIntrContext = QueryIntrContext();

    CpuSuspendIntr(&OldState);

    if (!cdvdman_common_lock(IsIntrContext)) {
        CpuResumeIntr(OldState);
        DPRINTF("cdvdman_SyncRead: exiting (sync_flag)...\n");
        return 0;
    }

    CpuResumeIntr(OldState);

    cdvdman_read(lsn, sectors, buf);

    cdvdman_cb_event(SCECdFuncRead);
    sync_flag = 0;
    SetEventFlag(cdvdman_stat.intr_ef, 9);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
    int result;

    DPRINTF("sceCdRead lsn=%d sectors=%d buf=%08x\n", (int)lsn, (int)sectors, (int)buf);

    if ((!(cdvdman_settings.common.flags & IOPCORE_COMPAT_ALT_READ)) || QueryIntrContext()) {
        result = cdvdman_AsyncRead(lsn, sectors, buf);
    } else {
        result = cdvdman_SyncRead(lsn, sectors, buf);
    }

    return result;
}

//-------------------------------------------------------------------------
int sceCdSeek(u32 lsn)
{
    DPRINTF("sceCdSeek %d\n", (int)lsn);

    cdvdman_stat.err = CDVD_ERR_NO;

    cdvdman_stat.status = CDVD_STAT_PAUSE;

    cdvdman_cb_event(SCECdFuncSeek);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdGetError(void)
{
    DPRINTF("sceCdGetError %d\n", cdvdman_stat.err);

    return cdvdman_stat.err;
}

//-------------------------------------------------------------------------
int sceCdGetToc(void *toc)
{
    cdvdman_stat.err = CDVD_ERR_READ;

    return 1;
}

//-------------------------------------------------------------------------
int sceCdSearchFile(cd_file_t *pcd_file, const char *name)
{
    DPRINTF("sceCdSearchFile %s\n", name);

    return cdvdman_findfile(pcd_file, name, 0);
}

//-------------------------------------------------------------------------
int sceCdSync(int mode)
{
    DPRINTF("sceCdSync %d sync flag = %d\n", mode, sync_flag);

    if (!sync_flag)
        return 0;

    if ((mode == 1) || (mode == 17))
        return 1;

    while (sync_flag)
        WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    return 0;
}

//-------------------------------------------------------------------------
static void cdvdman_initDiskType(void)
{
    cdvdman_stat.err = CDVD_ERR_NO;

    cdvdman_stat.disc_type_reg = (int)cdvdman_settings.common.media;
    DPRINTF("DiskType=0x%x\n", cdvdman_settings.common.media);
}

//-------------------------------------------------------------------------
int sceCdGetDiskType(void)
{
    return cdvdman_stat.disc_type_reg;
}

//-------------------------------------------------------------------------
int sceCdDiskReady(int mode)
{
    DPRINTF("sceCdDiskReady %d\n", mode);
    cdvdman_stat.err = CDVD_ERR_NO;

    if (cdvdman_cdinited) {
        if (mode == 0) {
            while (sync_flag)
                WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);
        }

        if (!sync_flag)
            return CDVD_READY_READY;
    }

    return CDVD_READY_NOTREADY;
}

//-------------------------------------------------------------------------
int sceCdTrayReq(int mode, u32 *traycnt)
{
    DPRINTF("sceCdTrayReq(%d, 0x%lX)\n", mode, *traycnt);

    if (mode == CDVD_TRAY_CHECK) {
        if (traycnt)
            *traycnt = cdvdman_media_changed;

        cdvdman_media_changed = 0;

        return 1;
    }

    if (mode == CDVD_TRAY_OPEN) {
        cdvdman_stat.status = CDVD_STAT_OPEN;
        cdvdman_stat.disc_type_reg = 0;

        DelayThread(11000);

        cdvdman_stat.err = CDVD_ERR_OPENS; /* not sure about this error code */

        return 1;
    } else if (mode == CDVD_TRAY_CLOSE) {
        DelayThread(25000);

        cdvdman_stat.status = CDVD_STAT_PAUSE; /* not sure if the status is right, may be - CDVD_STAT_SPIN */
        cdvdman_stat.err = CDVD_ERR_NO;        /* not sure if this error code is suitable here */
        cdvdman_stat.disc_type_reg = (int)cdvdman_settings.common.media;

        cdvdman_media_changed = 1;

        return 1;
    }

    return 0;
}

//-------------------------------------------------------------------------
int sceCdStop(void)
{
    cdvdman_stat.err = CDVD_ERR_NO;

    cdvdman_stat.status = CDVD_STAT_STOP;
    cdvdman_cb_event(SCECdFuncStop);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdPosToInt(cd_location_t *p)
{
    register int result;

    result = ((u32)p->minute >> 16) * 10 + ((u32)p->minute & 0xF);
    result *= 60;
    result += ((u32)p->second >> 16) * 10 + ((u32)p->second & 0xF);
    result *= 75;
    result += ((u32)p->sector >> 16) * 10 + ((u32)p->sector & 0xF);
    result -= 150;

    return result;
}

//-------------------------------------------------------------------------
cd_location_t *sceCdIntToPos(int i, cd_location_t *p)
{
    register int sc, se, mi;

    i += 150;
    se = i / 75;
    sc = i - se * 75;
    mi = se / 60;
    se = se - mi * 60;
    p->sector = (sc - (sc / 10) * 10) + (sc / 10) * 16;
    p->second = (se / 10) * 16 + se - (se / 10) * 10;
    p->minute = (mi / 10) * 16 + mi - (mi / 10) * 10;

    return p;
}

//-------------------------------------------------------------------------
int sceCdRI(char *buf, int *stat)
{
    u8 rdbuf[16];

    cdvdman_sendSCmd(0x12, NULL, 0, rdbuf, 9);

    if (stat)
        *stat = (int)rdbuf[0];

    mips_memcpy((void *)buf, (void *)&rdbuf[1], 8);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdReadClock(cd_clock_t *rtc)
{
    int rc;

    cdvdman_stat.err = CDVD_ERR_NO;

    rc = cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);

    rtc->week = 0;
    rtc->month &= 0x7f;

    return rc;
}

//-------------------------------------------------------------------------
int sceCdStatus(void)
{
    DPRINTF("sceCdStatus %d\n", cdvdman_stat.status);

    return cdvdman_stat.status;
}

//-------------------------------------------------------------------------
int sceCdApplySCmd(int cmd, void *in, u32 in_size, void *out)
{
    DPRINTF("sceCdApplySCmd\n");

    return cdvdman_sendSCmd(cmd & 0xff, in, in_size, out, 16);
}

//-------------------------------------------------------------------------
int *sceCdCallback(void *func)
{
    int oldstate;
    void *old_cb;

    DPRINTF("sceCdCallback %p\n", func);

    old_cb = user_cb;

    CpuSuspendIntr(&oldstate);

    user_cb = func;

    CpuResumeIntr(oldstate);

    return (int *)old_cb;
}

//-------------------------------------------------------------------------
int sceCdPause(void)
{
    cdvdman_stat.err = CDVD_ERR_NO;

    cdvdman_stat.status = CDVD_STAT_PAUSE;
    cdvdman_cb_event(SCECdFuncPause);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdBreak(void)
{
    DPRINTF("sceCdBreak\n");

    cdvdman_stat.err = CDVD_ERR_NO;
    cdvdman_stat.status = CDVD_STAT_PAUSE;

    cdvdman_stat.err = CDVD_ERR_ABRT;
    cdvdman_cb_event(SCECdFuncBreak);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
    return sceCdRead(lsn, sectors, buf, mode);
}

//-------------------------------------------------------------------------
int sceCdGetReadPos(void)
{
    DPRINTF("sceCdGetReadPos\n");

    return ReadPos;
}

//-------------------------------------------------------------------------
void *sceGetFsvRbuf(void)
{
    return cdvdman_fs_buf;
}

//-------------------------------------------------------------------------
int sceCdSC(int code, int *param)
{
    int result;

    DPRINTF("sceCdSC(0x%X, 0x%X)\n", code, *param);

    switch (code) {
        case CDSC_GET_INTRFLAG:
            result = cdvdman_stat.intr_ef;
            break;
        case CDSC_IO_SEMA:
            if (*param) {
                WaitSema(cdrom_io_sema);
            } else
                SignalSema(cdrom_io_sema);

            result = *param; //EE N-command code.
            break;
        case CDSC_GET_VERSION:
            result = CDVDMAN_MODULE_VERSION;
            break;
        case CDSC_GET_DEBUG_STATUS:
            *param = (int)&cdvdman_debug_print_flag;
            result = 0xFF;
            break;
        case CDSC_SET_ERROR:
            result = cdvdman_stat.err = *param;
            break;
        case CDSC_OPL_SHUTDOWN:
            oplShutdown(*param);
            result = 1;
            break;
        default:
            result = 1; // dummy result
    }

    return result;
}

//-------------------------------------------------------------------------
int sceCdRC(cd_clock_t *rtc)
{
    cdvdman_stat.err = CDVD_ERR_NO;

    return cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);
}

//-------------------------------------------------------------------------
static int cdvdman_readMechaconVersion(char *mname, u32 *stat)
{
    u8 rdbuf[16];
    u8 wrbuf[16];

    wrbuf[0] = 0;
    cdvdman_sendSCmd(0x03, wrbuf, 1, rdbuf, 4);

    *stat = rdbuf[0] & 0x80;
    rdbuf[0] &= 0x7f;

    mips_memcpy(mname, &rdbuf[0], 4);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdRM(char *m, u32 *stat)
{
    register int r;
    u8 rdbuf[16];
    u8 wrbuf[16];

    *stat = 0;
    r = cdvdman_readMechaconVersion(rdbuf, stat);
    if ((r == 1) && (0x104FE < (rdbuf[3] | (rdbuf[2] << 8) | (rdbuf[1] << 16)))) {

        mips_memcpy(&m[0], "M_NAME_UNKNOWN\0\0", 16);
        *stat |= 0x40;
    } else {
        wrbuf[0] = 0;
        cdvdman_sendSCmd(0x17, wrbuf, 1, rdbuf, 9);

        *stat = rdbuf[0];
        mips_memcpy(&m[0], &rdbuf[1], 8);

        wrbuf[0] = 8;
        cdvdman_sendSCmd(0x17, wrbuf, 1, rdbuf, 9);

        *stat |= rdbuf[0];
        mips_memcpy(&m[8], &rdbuf[1], 8);
    }

    return 1;
}

//-------------------------------------------------------------------------
int sceCdPowerOff(int *stat)
{
    return cdvdman_sendSCmd(0x0F, NULL, 0, (unsigned char *)stat, 1);
}

//-------------------------------------------------------------------------
int sceCdReadDiskID(void *DiskID)
{
    int i;
    u8 *p = (u8 *)DiskID;

    for (i = 0; i < 5; i++) {
        if (p[i] != 0)
            break;
    }
    if (i == 5)
        *((u16 *)DiskID) = (u16)0xadde;
    else
        mips_memcpy(DiskID, cdvdman_settings.common.DiscID, 5);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start)
{
    if (cdvdman_settings.common.flags & IOPCORE_COMPAT_EMU_DVDDL) {
        //Make layer 1 point to layer 0.
        *layer1_start = 0;
        *on_dual = 1;
    } else {
        *layer1_start = cdvdman_settings.common.layer1_start;
        *on_dual = (cdvdman_settings.common.layer1_start > 0) ? 1 : 0;
    }

    return 1;
}

//-------------------------------------------------------------------------
int sceCdLayerSearchFile(cdl_file_t *fp, const char *name, int layer)
{
    DPRINTF("sceCdLayerSearchFile %s\n", name);

    return cdvdman_findfile((cd_file_t *)fp, name, layer);
}

//--------------------------------------------------------------
int cdvdman_readID(int mode, u8 *buf)
{
    u8 lbuf[16];
    int stat;
    int r;

    r = sceCdRI(lbuf, &stat);
    if ((r == 0) || (stat))
        return 0;

    if (mode == 0) { // GUID
        u32 *GUID0 = (u32 *)&buf[0];
        u32 *GUID1 = (u32 *)&buf[4];
        *GUID0 = lbuf[0] | 0x08004600; //Replace the MODEL ID segment with the SCE OUI, to get the console's IEEE1394 EUI-64.
        *GUID1 = *(u32 *)&lbuf[4];
    } else { // ModelID
        u32 *ModelID = (u32 *)&buf[0];
        *ModelID = (*(u32 *)&lbuf[0]) >> 8;
    }

    return 1;
}

//--------------------------------------------------------------
int sceCdReadGUID(void *GUID)
{
    return cdvdman_readID(0, GUID);
}

//--------------------------------------------------------------
int sceCdReadModelID(void *ModelID)
{
    return cdvdman_readID(1, ModelID);
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
    cd_file_t cdfile;

    WaitSema(cdrom_io_sema);

    cdvdman_init();

    if (f->unit < 2) {
        sceCdDiskReady(0);

        fh = cdvdman_getfilefreeslot();
        if (fh) {
            r = cdvdman_findfile(&cdfile, filename, f->unit);
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

static int cdrom_purifyPath(char *path)
{
    int len;

    len = strlen(path);
    if ((len >= 3) && (path[len - 1] != '1' || path[len - 2] != ';')) //SCE does this too, hence assuming that the version suffix will be either totally there or absent. The only version supported is 1.
    {                                                                 //Instead of using strcat like the original, append the version suffix manually for efficiency.
        path[len] = ';';
        path[len + 1] = '1';
        path[len + 2] = '\0';

        return 0;
    }

    return 1;
}

static int cdrom_open(iop_file_t *f, const char *filename, int mode)
{
    int result;
    char path_buffer[128]; //Original buffer size in the SCE CDVDMAN module.

    DPRINTF("cdrom_open %s mode=%d layer %d\n", filename, mode, f->unit);

    strncpy(path_buffer, filename, sizeof(path_buffer));
    cdrom_purifyPath(path_buffer);

    if ((result = cdvdman_open(f, path_buffer, mode)) >= 0)
        f->mode = O_RDONLY; //SCE fixes the open flags to O_RDONLY for open().

    return result;
}

//--------------------------------------------------------------
static int cdrom_close(iop_file_t *f)
{
    FHANDLE *fh = (FHANDLE *)f->privdata;

    WaitSema(cdrom_io_sema);

    DPRINTF("cdrom_close\n");

    mips_memset(fh, 0, sizeof(FHANDLE));
    f->mode = 0; //SCE invalidates FDs by clearing the open flags.

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

    DPRINTF("cdrom_read size=%d file_position=%d\n", size, fh->position);

    if ((fh->position + size) > fh->filesize)
        size = fh->filesize - fh->position;

    sceCdDiskReady(0);

    rpos = 0;
    if (size > 0) {
        //Phase 1: read data until the offset of the file is nicely aligned to a 2048-byte boundary.
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
            mips_memcpy(buf, &cdvdman_fs_buf[offset], nbytes);
            buf += nbytes;
        }

        //Phase 2: read the data to the middle of the buffer, in units of 2048.
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

        //Phase 3: read any remaining data that isn't divisible by 2048.
        if ((nbytes = size) > 0) {
            while (sceCdRead(fh->lsn + (fh->position / 2048), 1, cdvdman_fs_buf, NULL) == 0)
                DelayThread(10000);

            size -= nbytes;
            fh->position += nbytes;
            rpos += nbytes;

            sceCdSync(0);
            mips_memcpy(buf, cdvdman_fs_buf, nbytes);
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
    char path_buffer[128]; //Original buffer size in the SCE CDVDMAN module.

    DPRINTF("cdrom_getstat %s layer %d\n", filename, f->unit);

    strncpy(path_buffer, filename, sizeof(path_buffer));
    cdrom_purifyPath(path_buffer); //Unlike the SCE original, purify the path right away.

    sceCdDiskReady(0);
    return sceCdLayerSearchFile((cdl_file_t *)&stat->attr, path_buffer, f->unit) - 1;
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

    DPRINTF("cdrom_dread fh->lsn=%lu\n", fh->lsn);

    sceCdDiskReady(0);
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

    WaitSema(cdrom_io_sema);

    if (cmd != 0x10000) // Spin Ctrl op
        r = -EINVAL;

    SignalSema(cdrom_io_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen)
{
    int r = 0;

    WaitSema(cdrom_io_sema);

    switch (cmd) {
        case CIOCSTREAMPAUSE:
            sceCdStPause();
            break;
        case CIOCSTREAMRESUME:
            sceCdStResume();
            break;
        case CIOCSTREAMSTAT:
            r = sceCdStStat();
            break;
        default:
            r = -EINVAL;
    }

    SignalSema(cdrom_io_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen)
{
    int result;

    WaitSema(cdrom_io_sema);

    result = 0;
    switch (cmd) {
        case CDIOC_READCLOCK:
            sceCdReadClock((cd_clock_t *)buf);
            break;
        case CDIOC_READGUID:
            sceCdReadGUID(buf);
            break;
        case CDIOC_READDISKGUID:
            sceCdReadDiskID(buf);
            break;
        case CDIOC_GETDISKTYPE:
            *(int *)buf = sceCdGetDiskType();
            break;
        case CDIOC_GETERROR:
            *(int *)buf = sceCdGetError();
            break;
        case CDIOC_TRAYREQ:
            sceCdTrayReq(*(int *)args, (u32 *)buf);
            break;
        case CDIOC_STATUS:
            *(int *)buf = sceCdStatus();
            break;
        case CDIOC_POWEROFF:
        case CDIOC_MMODE:
            result = 1;
            break;
        case CDIOC_DISKRDY:
            *(int *)buf = sceCdDiskReady(*(int *)args);
            break;
        case CDIOC_READMODELID:
            sceCdReadModelID(buf);
            break;
        case CDIOC_STREAMINIT:
            sceCdStInit(((u32 *)args)[0], ((u32 *)args)[1], (void *)((u32 *)args)[2]);
            break;
        case CDIOC_BREAK:
            sceCdBreak();
            break;
        case CDIOC_SPINNOM:
        case CDIOC_SPINSTM:
        case CDIOC_TRYCNT:
        case CDIOC_READDVDDUALINFO:
        case CDIOC_INIT:
            result = 0;
            break;
        case CDIOC_STANDBY:
            sceCdStandby();
            break;
        case CDIOC_STOP:
            sceCdStop();
            break;
        case CDIOC_PAUSE:
            sceCdPause();
            break;
        case CDIOC_GETTOC:
            sceCdGetToc(buf);
            break;
        case CDIOC_GETINTREVENTFLG:
            *(int *)buf = cdvdman_stat.intr_ef;
            result = cdvdman_stat.intr_ef;
            break;
        default:
            result = -EIO;
            break;
    }

    SignalSema(cdrom_io_sema);

    return result;
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

//-------------------------------------------------------------------------
static struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer)
{
    char cdvdman_dirname[32]; /* Like below, but follow the original SCE limitation of 32-characters.
						Some games specify filenames like "\\FILEFILE.EXT;1", which result in a length longer than just 14.
						SCE does not perform bounds-checking on this buffer.	*/
    char cdvdman_curdir[15];  /* Maximum 14 characters: the filename (8) + '.' + extension (3) + ';' + '1'.
						Unlike the SCE original which used a 32-character buffer,
						we'll assume that ISO9660 disc images are all _strictly_ compliant with ISO9660 level 1.	*/
    char *p = (char *)name;
    char *slash;
    int r, len, filename_len;
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
#ifdef __IOPCORE_DEBUG
        if (len >= sizeof(cdvdman_dirname)) {
            DPRINTF("cdvdman_locatefile: segment too long: (%d chars) \"%s\"\n", len, p);
            asm volatile("break\n");
        }
#endif

        // copy the path into main 'dir' var
        strncpy(cdvdman_dirname, p, len);
        cdvdman_dirname[len] = 0;
    } else {
#ifdef __IOPCORE_DEBUG
        len = strlen(p);

        if (len >= sizeof(cdvdman_dirname)) {
            DPRINTF("cdvdman_locatefile: filename too long: (%d chars) \"%s\"\n", len, p);
            asm volatile("break\n");
        }
#endif

        strcpy(cdvdman_dirname, p);
    }

    while (tocLength > 0) {
        if (sceCdRead(tocLBA, 1, cdvdman_buf, NULL) == 0)
            return NULL;
        sceCdSync(0);
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
                strncpy(cdvdman_curdir, tocEntryPointer->filename, filename_len); // copy filename
                cdvdman_curdir[filename_len] = 0;

#ifdef __IOPCORE_DEBUG
                DPRINTF("cdvdman_locatefile strcmp %s %s\n", cdvdman_dirname, cdvdman_curdir);
#endif

                r = strncmp(cdvdman_dirname, cdvdman_curdir, 12);
                if ((!r) && (!slash)) { // we searched a file so it's found
                    DPRINTF("cdvdman_locatefile found file! LBA=%d size=%d\n", (int)tocEntryPointer->fileLBA, (int)tocEntryPointer->fileSize);
                    return tocEntryPointer;
                } else if ((!r) && (tocEntryPointer->fileProperties & 2)) { // we found it but it's a directory
                    tocLBA = tocEntryPointer->fileLBA;
                    tocLength = tocEntryPointer->fileSize;
                    p = &slash[1];

                    if (!(cdvdman_settings.common.flags & IOPCORE_COMPAT_EMU_DVDDL)) {
                        int on_dual;
                        u32 layer1_start;
                        sceCdReadDvdDualInfo(&on_dual, &layer1_start);

                        if (layer)
                            tocLBA += layer1_start;
                    }

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

//-------------------------------------------------------------------------
static int cdvdman_findfile(cd_file_t *pcdfile, const char *name, int layer)
{
    static char cdvdman_filepath[256];
    u32 lsn;
    struct dirTocEntry *tocEntryPointer;
    layer_info_t *pLayerInfo;

    cdvdman_init();

    if (cdvdman_settings.common.flags & IOPCORE_COMPAT_EMU_DVDDL)
        layer = 0;
    pLayerInfo = (layer != 0) ? &layer_info[1] : &layer_info[0]; //SCE CDVDMAN simply treats a non-zero value as a signal for the 2nd layer.

    WaitSema(cdvdman_searchfilesema);

    DPRINTF("cdvdman_findfile %s layer%d\n", name, layer);

    strncpy(cdvdman_filepath, name, sizeof(cdvdman_filepath));
    cdvdman_filepath[sizeof(cdvdman_filepath) - 1] = '\0';
    cdvdman_trimspaces(cdvdman_filepath);

    DPRINTF("cdvdman_findfile cdvdman_filepath=%s\n", cdvdman_filepath);

    if (pLayerInfo->rootDirtocLBA == 0) {
        SignalSema(cdvdman_searchfilesema);
        return 0;
    }

    tocEntryPointer = cdvdman_locatefile(cdvdman_filepath, pLayerInfo->rootDirtocLBA, pLayerInfo->rootDirtocLength, layer);
    if (tocEntryPointer == NULL) {
        SignalSema(cdvdman_searchfilesema);
        return 0;
    }

    lsn = tocEntryPointer->fileLBA;
    if (layer) {
        sceCdReadDvdDualInfo((int *)&pcdfile->lsn, &pcdfile->size);
        lsn += pcdfile->size;
    }

    pcdfile->lsn = lsn;
    // Skip Videos: Apply 0 (zero) filesize to PSS videos
    if ((cdvdman_settings.common.flags & IOPCORE_COMPAT_0_SKIP_VIDEOS) &&
        ((!strncmp(&cdvdman_filepath[strlen(cdvdman_filepath) - 6], ".PSS", 4)) ||
         (!strncmp(&cdvdman_filepath[strlen(cdvdman_filepath) - 6], ".pss", 4))))
        pcdfile->size = 0;
    else
        pcdfile->size = tocEntryPointer->fileSize;

    strcpy(pcdfile->name, strrchr(name, '\\') + 1);

    DPRINTF("cdvdman_findfile found %s\n", name);

    SignalSema(cdvdman_searchfilesema);

    return 1;
}

//-------------------------------------------------------------------------
static int cdvdman_writeSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size)
{
    int i;
    u8 dummy;
    u8 *p;
    u8 rdbuf[64];

    WaitSema(cdvdman_scmdsema);

    if (CDVDreg_SDATAIN & 0x80) {
        SignalSema(cdvdman_scmdsema);
        return 0;
    }

    if (!(CDVDreg_SDATAIN & 0x40)) {
        do {
            dummy = CDVDreg_SDATAOUT;
        } while (!(CDVDreg_SDATAIN & 0x40));
    }

    if (in_size > 0) {
        for (i = 0; i < in_size; i++) {
            p = (void *)(in + i);
            CDVDreg_SDATAIN = *p;
        }
    }

    CDVDreg_SCOMMAND = cmd;
    dummy = CDVDreg_SCOMMAND;

    while (CDVDreg_SDATAIN & 0x80) {
        ;
    }

    i = 0;
    if (!(CDVDreg_SDATAIN & 0x40)) {
        do {
            p = (void *)(rdbuf + i);
            *p = CDVDreg_SDATAOUT;
            i++;
        } while (!(CDVDreg_SDATAIN & 0x40));
    }

    if (out_size > i)
        out_size = i;

    mips_memcpy((void *)out, (void *)rdbuf, out_size);

    SignalSema(cdvdman_scmdsema);

    return 1;
}

//--------------------------------------------------------------
static int cdvdman_sendSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size)
{
    int r, retryCount = 0;

retry:

    r = cdvdman_writeSCmd(cmd & 0xff, in, in_size, out, out_size);
    if (r == 0) {
        DelayThread(2000);
        if (++retryCount <= 2500)
            goto retry;
    }

    DelayThread(2000);

    return 1;
}

//--------------------------------------------------------------
struct cdvdman_cb_data
{
    void (*user_cb)(int reason);
    int reason;
};

static int cdvdman_cb_event(int reason)
{
    static struct cdvdman_cb_data cb_data;

    if (user_cb) {
        cb_data.user_cb = user_cb;
        cb_data.reason = reason;

        DPRINTF("cdvdman_cb_event reason: %d - setting cb alarm...\n", reason);

        if (QueryIntrContext())
            iSetAlarm(&gCallbackSysClock, &event_alarm_cb, &cb_data);
        else
            SetAlarm(&gCallbackSysClock, &event_alarm_cb, &cb_data);
    }

    return 1;
}

//-------------------------------------------------------------------------
static unsigned int event_alarm_cb(void *args)
{
    struct cdvdman_cb_data *cb_data = args;

    cb_data->user_cb(cb_data->reason);
    return 0;
}

//-------------------------------------------------------------------------
static void cdvdman_cdread_Thread(void *args)
{
    while (1) {
        WaitSema(cdrom_rthread_sema);

        cdvdman_read(cdvdman_stat.cdread_lba, cdvdman_stat.cdread_sectors, cdvdman_stat.cdread_buf);

        sync_flag = 0;
        SetEventFlag(cdvdman_stat.intr_ef, 9);

        /* This streaming callback is not compatible with the original SONY stream channel 0 (IOP) callback's design.
			The original is run from the interrupt handler, but we want it to run
			from a threaded environment because it's easier to protect critical regions. */
        if (Stm0Callback != NULL)
            Stm0Callback();
        else
            cdvdman_cb_event(SCECdFuncRead); //Only runs if streaming is not in action.
    }
}

//-------------------------------------------------------------------------
static void cdvdman_startThreads(void)
{
    iop_thread_t thread_param;

    cdvdman_stat.status = CDVD_STAT_PAUSE;
    cdvdman_stat.err = CDVD_ERR_NO;

    thread_param.thread = &cdvdman_cdread_Thread;
    thread_param.stacksize = 0x440;
    thread_param.priority = 0x0f;
    thread_param.attr = TH_C;
    thread_param.option = 0xABCD0000;

    cdvdman_ReadingThreadID = CreateThread(&thread_param);
    StartThread(cdvdman_ReadingThreadID, NULL);
}

//-------------------------------------------------------------------------
static void cdvdman_create_semaphores(void)
{
    iop_sema_t smp;

    smp.initial = 1;
    smp.max = 1;
    smp.attr = 0;
    smp.option = 0;

    cdvdman_scmdsema = CreateSema(&smp);
    smp.initial = 0;
    cdrom_rthread_sema = CreateSema(&smp);
}

//-------------------------------------------------------------------------
static void cdvdman_initdev(void)
{
    iop_event_t event;

    event.attr = EA_MULTI;
    event.option = 0;
    event.bits = 1;

    cdvdman_stat.intr_ef = CreateEventFlag(&event);

    DelDrv("cdrom");
    AddDrv((iop_device_t *)&cdrom_dev);
}

//-------------------------------------------------------------------------
static int intrh_cdrom(void *common)
{
    if (CDVDreg_PWOFF & CDL_DATA_RDY)
        CDVDreg_PWOFF = CDL_DATA_RDY;

    if (CDVDreg_PWOFF & CDL_DATA_END) {
        //If IGR is enabled: Do not acknowledge the interrupt here. The EE-side IGR code will monitor and acknowledge it.
        if (cdvdman_settings.common.flags & IOPCORE_ENABLE_POFF) {
            CDVDreg_PWOFF = CDL_DATA_END; //Acknowldge power-off request.
        }
        iSetEventFlag(cdvdman_stat.intr_ef, 0x14); //Notify FILEIO and CDVDFSV of the power-off event.

//Call power-off callback here. OPL doesn't handle one, so do nothing.
#ifdef __USE_DEV9
        if (cdvdman_settings.common.flags & IOPCORE_ENABLE_POFF) {
            //If IGR is disabled, switch off the console.
            iWakeupThread(POFFThreadID);
        }
#endif
    } else
        CDVDreg_PWOFF = CDL_DATA_COMPLETE; //Acknowledge interrupt

    return 1;
}

static inline void InstallIntrHandler(void)
{
    RegisterIntrHandler(IOP_IRQ_CDVD, 1, &intrh_cdrom, NULL);
    EnableIntr(IOP_IRQ_CDVD);

    //Acknowledge hardware events (e.g. poweroff)
    if (CDVDreg_PWOFF & CDL_DATA_END)
        CDVDreg_PWOFF = CDL_DATA_END;
    if (CDVDreg_PWOFF & CDL_DATA_RDY)
        CDVDreg_PWOFF = CDL_DATA_RDY;
}

int _start(int argc, char **argv)
{
    // register exports
    RegisterLibraryEntries(&_exp_cdvdman);
    RegisterLibraryEntries(&_exp_cdvdstm);

    RegisterLibraryEntries(&_exp_smsutils);
#ifdef __USE_DEV9
    RegisterLibraryEntries(&_exp_dev9);
    dev9d_init();
#endif

    DeviceInit();

    RegisterLibraryEntries(&_exp_oplutils);

    // Setup the callback timer.
    USec2SysClock((cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS) ? 5000 : 0, &gCallbackSysClock);

    // create SCMD/searchfile semaphores
    cdvdman_create_semaphores();

    // start cdvdman threads
    cdvdman_startThreads();

    // register cdrom device driver
    cdvdman_initdev();
    InstallIntrHandler();

    // hook MODLOAD's exports
    hookMODLOAD();

    // init disk type stuff
    cdvdman_initDiskType();

    return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
void SetStm0Callback(StmCallback_t callback)
{
    Stm0Callback = callback;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
    DeviceDeinit();

    return 0;
}
