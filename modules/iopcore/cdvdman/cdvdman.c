/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"
#include "../../isofs/zso.h"

#define MODNAME "cdvd_driver"
IRX_ID(MODNAME, 1, 1);

//------------------ Patch Zone ----------------------
struct CDVDMAN_SETTINGS_TYPE cdvdman_settings = {
    CDVDMAN_SETTINGS_DEFAULT_COMMON,
    CDVDMAN_SETTINGS_DEFAULT_DEVICE_SETTINGS};

//----------------------------------------------------
extern struct irx_export_table _exp_cdvdman;
extern struct irx_export_table _exp_cdvdstm;
extern struct irx_export_table _exp_smsutils;
extern struct irx_export_table _exp_oplutils;
#ifdef __USE_DEV9
extern struct irx_export_table _exp_dev9;
#endif

// reader function interface, raw reader impementation by default
int DeviceReadSectorsCached(u32 sector, void *buffer, unsigned int count);
int (*DeviceReadSectorsPtr)(u32 sector, void *buffer, unsigned int count) = &DeviceReadSectorsCached;

// internal functions prototypes
static void oplShutdown(int poff);
static int cdvdman_writeSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size);
static unsigned int event_alarm_cb(void *args);
static void cdvdman_signal_read_end(void);
static void cdvdman_signal_read_end_intr(void);
static void cdvdman_startThreads(void);
static void cdvdman_create_semaphores(void);
static int cdvdman_read(u32 lsn, u32 sectors, void *buf);

// Sector cache to improve IO
#define MAX_SECTOR_CACHE 32
static u8 sector_cache[MAX_SECTOR_CACHE][2048];
static int cur_sector = -1;

struct cdvdman_cb_data
{
    void (*user_cb)(int reason);
    int reason;
};

cdvdman_status_t cdvdman_stat;
static struct cdvdman_cb_data cb_data;

int cdrom_io_sema;
static int cdrom_rthread_sema;
static int cdvdman_scmdsema;
int cdvdman_searchfilesema;
static int cdvdman_ReadingThreadID;

static StmCallback_t Stm0Callback = NULL;
static iop_sys_clock_t gCallbackSysClock;

// buffers
u8 cdvdman_buf[CDVDMAN_BUF_SECTORS * 2048];

#define CDVDMAN_MODULE_VERSION 0x225
static int cdvdman_debug_print_flag = 0;

unsigned char sync_flag;
unsigned char cdvdman_cdinited = 0;
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
    u32 stat;

    DeviceLock();
    if (vmcShutdownCb != NULL)
        vmcShutdownCb();
    DeviceUnmount();
    if (poff) {
        DeviceStop();
#ifdef __USE_DEV9
        dev9Shutdown();
#endif
        sceCdPowerOff(&stat);
    }
}

//-------------------------------------------------------------------------
#ifdef __USE_DEV9
static void cdvdman_poff_thread(void *arg)
{
    SleepThread();

    oplShutdown(1);
}
#endif

void cdvdman_init(void)
{
#ifdef __USE_DEV9
    iop_thread_t ThreadData;
#endif

    if (!cdvdman_cdinited) {
        cdvdman_stat.err = SCECdErNO;

        cdvdman_fs_init();

#ifdef __USE_DEV9
        if (cdvdman_settings.common.flags & IOPCORE_ENABLE_POFF) {
            ThreadData.attr = TH_C;
            ThreadData.option = 0xABCD0001;
            ThreadData.priority = 1;
            ThreadData.stacksize = 0x1000;
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
static unsigned int cdvdemu_read_end_cb(void *arg)
{
    iSetEventFlag(cdvdman_stat.intr_ef, 0x1000);
    return 0;
}

/*
  This small improvement will mostly benefit ZSO files.
  For the same size of an ISO sector, we can have more than one ZSO blocks.
  If we do a consecutive read of many ISO sectors we will have a huge amount of ZSO sectors ready.
  Therefore reducing IO access for ZSO files.
*/
int DeviceReadSectorsCached(u32 lsn, void *buffer, unsigned int sectors)
{
    if (sectors <= MAX_SECTOR_CACHE){
        if (cur_sector < 0 || lsn<cur_sector || (lsn+sectors)-cur_sector >= MAX_SECTOR_CACHE){
            DeviceReadSectors(lsn, sector_cache, MAX_SECTOR_CACHE);
            cur_sector = lsn;
        }
        int pos = lsn-cur_sector;
        memcpy(buffer, &(sector_cache[pos]), 2048*sectors);
        return SCECdErNO;
    }
    return DeviceReadSectors(lsn, buffer, sectors);
}

/*
  For ZSO we need to be able to read at arbitrary offsets with arbitrary sizes.
  Since we can only do sector-based reads, this funtions acts as a wrapper.
  It will do at most 3 IO reads, most of the time only 1.
*/
int read_raw_data(u8* addr, u32 size, u32 offset){
    u32 o_size = size;
    u32 lba = offset/2048;
    u32 pos = offset & (2048 - 1);
    
    // read first block if not aligned to sector size
    if (pos){
    	int r = MIN(size, (2048 - pos));
        DeviceReadSectorsCached(lba, ciso_dec_buf, 1);
    	memcpy(addr, ciso_dec_buf+pos, r);
    	size -= r;
    	lba++;
    	addr += r;
    }
    
    // read intermediate blocks if more than one block is left
    u32 n_blocks = size/2048;
    if (size%2048) n_blocks++;
    if (n_blocks>1){
    	int r = 2048*(n_blocks-1);
        DeviceReadSectorsCached(lba, addr, n_blocks-1);
    	size -= r;
    	addr += r;
    	lba += n_blocks-1;
    }
    
    // read remaining data
    if (size){
        DeviceReadSectorsCached(lba, ciso_dec_buf, 1);
    	memcpy(addr, ciso_dec_buf, size);
    	size = 0;
    }

    // return remaining size
    return o_size-size;
}

int DeviceReadSectorsCompressed(u32 lsn, void *addr, unsigned int count)
{
    return (ciso_read_sector(addr, lsn, count) == count)? SCECdErNO : SCECdErEOM;
}

static int probed = 0;
static int ProbeZSO(){
    if (DeviceReadSectorsCached(0, ciso_dec_buf, 1) != SCECdErNO)
        return 0;
    probed = 1;
    if (*(u32*)ciso_dec_buf == ZSO_MAGIC){
        CISO_header* header = (CISO_header*)ciso_dec_buf;
        DeviceReadSectorsPtr = &DeviceReadSectorsCompressed;
        ciso_header_size = header->header_size;
        ciso_block_size = header->block_size;
        ciso_uncompressed_size = header->total_bytes;
        ciso_align = header->align;
        ciso_block_header = 0;
        // calculate number of blocks without using uncompressed_size (avoid 64bit division)
        ciso_total_block = ((((*(u32*)(ciso_dec_buf+sizeof(CISO_header)) & 0x7FFFFFFF) << ciso_align) - ciso_header_size) / 4) - 1;
    }
    return 1;
}

static int cdvdman_read_sectors(u32 lsn, unsigned int sectors, void *buf)
{
    unsigned int SectorsToRead, remaining;
    void *ptr;

    DPRINTF("cdvdman_read lsn=%lu sectors=%u buf=%p\n", lsn, sectors, buf);

    if (probed == 0) // Probe for ZSO before first read
        if (!ProbeZSO()) return 1;

    cdvdman_stat.err = SCECdErNO;
    for (ptr = buf, remaining = sectors; remaining > 0;) {
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

        cdvdman_stat.err = DeviceReadSectorsPtr(lsn, ptr, SectorsToRead);
        if (cdvdman_stat.err != SCECdErNO) {
            if (cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS)
                CancelAlarm(&cdvdemu_read_end_cb, NULL);
            break;
        }

        /* PS2LOGO Decryptor algorithm; based on misfire's code (https://github.com/mlafeldt/ps2logo)
           The PS2 logo is stored within the first 12 sectors, scrambled.
           This algorithm exploits the characteristic that the value used for scrambling will be recorded,
           when it is XOR'ed against a black pixel. The first pixel is black, hence the value of the first byte
           was the value used for scrambling. */
        if (lsn < 13) {
            u32 j;
            u8 *logo = (u8 *)ptr;
            static u8 key = 0;
            if (lsn == 0) //First sector? Copy the first byte as the value for unscrambling the logo.
                key = logo[0];
            if (key != 0) {
                for (j = 0; j < (SectorsToRead * 2048); j++) {
                    logo[j] ^= key;
                    logo[j] = (logo[j] << 3) | (logo[j] >> 5);
                }
            }
        }

        ptr += SectorsToRead * 2048;
        remaining -= SectorsToRead;
        lsn += SectorsToRead;
        ReadPos += SectorsToRead * 2048;

        if (cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS) {
            //Sleep until the required amount of time has been spent.
            WaitEventFlag(cdvdman_stat.intr_ef, 0x1000, WEF_AND, NULL);
        }
    }

    return (cdvdman_stat.err == SCECdErNO ? 0 : 1);
}

static int cdvdman_read(u32 lsn, u32 sectors, void *buf)
{
    cdvdman_stat.status = SCECdStatRead;

    buf = (void *)PHYSADDR(buf);
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
            nbytes = nsectors * 2048;

            memcpy(buf, cdvdman_buf, nbytes);

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

    cdvdman_stat.status = SCECdStatPause;

    return 1;
}

//-------------------------------------------------------------------------
u32 sceCdGetReadPos(void)
{
    DPRINTF("sceCdGetReadPos\n");

    return ReadPos;
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

int cdvdman_SyncRead(u32 lsn, u32 sectors, void *buf)
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
static void cdvdman_initDiskType(void)
{
    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.disc_type_reg = (int)cdvdman_settings.common.media;
    DPRINTF("DiskType=0x%x\n", cdvdman_settings.common.media);
}

//-------------------------------------------------------------------------
u32 sceCdPosToInt(sceCdlLOCCD *p)
{
    register u32 result;

    result = ((u32)p->minute >> 16) * 10 + ((u32)p->minute & 0xF);
    result *= 60;
    result += ((u32)p->second >> 16) * 10 + ((u32)p->second & 0xF);
    result *= 75;
    result += ((u32)p->sector >> 16) * 10 + ((u32)p->sector & 0xF);
    result -= 150;

    return result;
}

//-------------------------------------------------------------------------
sceCdlLOCCD *sceCdIntToPos(u32 i, sceCdlLOCCD *p)
{
    register u32 sc, se, mi;

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
sceCdCBFunc sceCdCallback(sceCdCBFunc func)
{
    int oldstate;
    void *old_cb;

    DPRINTF("sceCdCallback %p\n", func);

    if (sceCdSync(1))
        return NULL;

    CpuSuspendIntr(&oldstate);

    old_cb = cb_data.user_cb;
    cb_data.user_cb = func;

    CpuResumeIntr(oldstate);

    return old_cb;
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
static int cdvdman_writeSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size)
{
    int i;
    u8 *p;
    u8 rdbuf[64];

    WaitSema(cdvdman_scmdsema);

    if (CDVDreg_SDATAIN & 0x80) {
        SignalSema(cdvdman_scmdsema);
        return 0;
    }

    if (!(CDVDreg_SDATAIN & 0x40)) {
        do {
            (void)CDVDreg_SDATAOUT;
        } while (!(CDVDreg_SDATAIN & 0x40));
    }

    if (in_size > 0) {
        for (i = 0; i < in_size; i++) {
            p = (void *)(in + i);
            CDVDreg_SDATAIN = *p;
        }
    }

    CDVDreg_SCOMMAND = cmd;
    (void)CDVDreg_SCOMMAND;

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

    memcpy((void *)out, (void *)rdbuf, out_size);

    SignalSema(cdvdman_scmdsema);

    return 1;
}

//--------------------------------------------------------------
int cdvdman_sendSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size)
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
void cdvdman_cb_event(int reason)
{
    if (cb_data.user_cb != NULL) {
        cb_data.reason = reason;

        DPRINTF("cdvdman_cb_event reason: %d - setting cb alarm...\n", reason);

        if (QueryIntrContext())
            iSetAlarm(&gCallbackSysClock, &event_alarm_cb, &cb_data);
        else
            SetAlarm(&gCallbackSysClock, &event_alarm_cb, &cb_data);
    } else {
        cdvdman_signal_read_end();
    }
}

static unsigned int event_alarm_cb(void *args)
{
    struct cdvdman_cb_data *cb_data = args;

    cdvdman_signal_read_end_intr();
    if (cb_data->user_cb != NULL) //This interrupt does not occur immediately, hence check for the callback again here.
        cb_data->user_cb(cb_data->reason);
    return 0;
}

//-------------------------------------------------------------------------
/* Use these to signal that the reading process is complete.
   Do not run the user callback after the drive can be deemed ready,
   as this may break games that were not designed to expect the callback to be run
   after the drive becomes visibly ready via the libcdvd API.
   Hence if a user callback is registered, signal completion from
   within the interrupt handler, before the user callback is run. */
static void cdvdman_signal_read_end(void)
{
    sync_flag = 0;
    SetEventFlag(cdvdman_stat.intr_ef, 9);
}

static void cdvdman_signal_read_end_intr(void)
{
    sync_flag = 0;
    iSetEventFlag(cdvdman_stat.intr_ef, 9);
}

static void cdvdman_cdread_Thread(void *args)
{
    while (1) {
        WaitSema(cdrom_rthread_sema);

        cdvdman_read(cdvdman_stat.cdread_lba, cdvdman_stat.cdread_sectors, cdvdman_stat.cdread_buf);

        /* This streaming callback is not compatible with the original SONY stream channel 0 (IOP) callback's design.
	   The original is run from the interrupt handler, but we want it to run
	   from a threaded environment because our interrupt is emulated. */
        if (Stm0Callback != NULL) {
            cdvdman_signal_read_end();

            /* Check that the streaming callback was not cleared, as this pointer may get changed between function calls.
               As per the original semantics, once it is cleared, then it should not be called. */
            if (Stm0Callback != NULL)
                Stm0Callback();
        } else
            cdvdman_cb_event(SCECdFuncRead); //Only runs if streaming is not in action.
    }
}

//-------------------------------------------------------------------------
static void cdvdman_startThreads(void)
{
    iop_thread_t thread_param;

    cdvdman_stat.status = SCECdStatPause;
    cdvdman_stat.err = SCECdErNO;

    thread_param.thread = &cdvdman_cdread_Thread;
    thread_param.stacksize = 0x1000;
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
