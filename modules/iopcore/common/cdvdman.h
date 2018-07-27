// PS2 CDVD hardware registers
#define CDL_DATA_RDY 0x01
#define CDL_DATA_COMPLETE 0x02
#define CDL_DATA_END 0x04

#define CDVDreg_NCOMMAND (*(volatile unsigned char *)0xBF402004)
#define CDVDreg_READY (*(volatile unsigned char *)0xBF402005)
#define CDVDreg_NDATAIN (*(volatile unsigned char *)0xBF402005)
#define CDVDreg_ERROR (*(volatile unsigned char *)0xBF402006)
#define CDVDreg_HOWTO (*(volatile unsigned char *)0xBF402006)
#define CDVDreg_ABORT (*(volatile unsigned char *)0xBF402007)
#define CDVDreg_PWOFF (*(volatile unsigned char *)0xBF402008)
#define CDVDreg_9 (*(volatile unsigned char *)0xBF402008)
#define CDVDreg_STATUS (*(volatile unsigned char *)0xBF40200A)
#define CDVDreg_B (*(volatile unsigned char *)0xBF40200B)
#define CDVDreg_C (*(volatile unsigned char *)0xBF40200C)
#define CDVDreg_D (*(volatile unsigned char *)0xBF40200D)
#define CDVDreg_E (*(volatile unsigned char *)0xBF40200E)
#define CDVDreg_TYPE (*(volatile unsigned char *)0xBF40200F)
#define CDVDreg_13 (*(volatile unsigned char *)0xBF402013)
#define CDVDreg_SCOMMAND (*(volatile unsigned char *)0xBF402016)
#define CDVDreg_SDATAIN (*(volatile unsigned char *)0xBF402017)
#define CDVDreg_SDATAOUT (*(volatile unsigned char *)0xBF402018)
#define CDVDreg_KEYSTATE (*(volatile unsigned char *)0xBF402038)
#define CDVDreg_KEYXOR (*(volatile unsigned char *)0xBF402039)
#define CDVDreg_DEC (*(volatile unsigned char *)0xBF40203A)

// user cb CDVD events
#define SCECdFuncRead 1
#define SCECdFuncReadCDDA 2
#define SCECdFuncGetToc 3
#define SCECdFuncSeek 4
#define SCECdFuncStandby 5
#define SCECdFuncStop 6
#define SCECdFuncPause 7
#define SCECdFuncBreak 8

// Modes for cdInit()
#define CDVD_INIT_INIT 0x00    // init cd system and wait till commands can be issued
#define CDVD_INIT_NOCHECK 0x01 // init cd system
#define CDVD_INIT_EXIT 0x05    // de-init system

// CDVD stats
#define CDVD_STAT_STOP 0x00  // disc has stopped spinning
#define CDVD_STAT_OPEN 0x01  // tray is open
#define CDVD_STAT_SPIN 0x02  // disc is spinning
#define CDVD_STAT_READ 0x06  // reading from disc
#define CDVD_STAT_PAUSE 0x0A // disc is paused
#define CDVD_STAT_SEEK 0x12  // disc is seeking
#define CDVD_STAT_ERROR 0x20 // error occurred

// cdTrayReq() values
#define CDVD_TRAY_OPEN 0  // Tray Open
#define CDVD_TRAY_CLOSE 1 // Tray Close
#define CDVD_TRAY_CHECK 2 // Tray Check

// sceCdDiskReady() values
#define CDVD_READY_READY 0x02
#define CDVD_READY_NOTREADY 0x06

#define CdSpinMax 0
#define CdSpinNom 1
#define CdSpinStm 0

// cdGetError() return values
#define CDVD_ERR_FAIL -1      // error in cdGetError()
#define CDVD_ERR_NO 0x00      // no error occurred
#define CDVD_ERR_ABRT 0x01    // command was aborted due to cdBreak() call
#define CDVD_ERR_CMD 0x10     // unsupported command
#define CDVD_ERR_OPENS 0x11   // tray is open
#define CDVD_ERR_NODISC 0x12  // no disk inserted
#define CDVD_ERR_NORDY 0x13   // drive is busy processing another command
#define CDVD_ERR_CUD 0x14     // command unsupported for disc currently in drive
#define CDVD_ERR_IPI 0x20     // sector address error
#define CDVD_ERR_ILI 0x21     // num sectors error
#define CDVD_ERR_PRM 0x22     // command parameter error
#define CDVD_ERR_READ 0x30    // error while reading
#define CDVD_ERR_TRMOPN 0x31  // tray was opened
#define CDVD_ERR_EOM 0x32     // outermost error
#define CDVD_ERR_READCF 0xFD  // error setting command
#define CDVD_ERR_READCFR 0xFE // error setting command

typedef struct
{
    u8 stat;
    u8 second;
    u8 minute;
    u8 hour;
    u8 week;
    u8 day;
    u8 month;
    u8 year;
} cd_clock_t;

typedef struct
{
    u32 lsn;
    u32 size;
    char name[16];
    u8 date[8];
} cd_file_t;

typedef struct
{
    u32 lsn;
    u32 size;
    char name[16];
    u8 date[8];
    u32 flag;
} cdl_file_t;

typedef struct
{
    u8 minute;
    u8 second;
    u8 sector;
    u8 track;
} cd_location_t;

typedef struct
{
    u8 trycount;
    u8 spindlctrl;
    u8 datapattern;
    u8 pad;
} cd_read_mode_t;

#define CDIOC_FNUM(x) (('C' << 8) | (x))

enum CDIOC_CODE {
    CDIOC_READCLOCK = CDIOC_FNUM(0x0C),

    CDIOC_READGUID = CDIOC_FNUM(0x1D),
    CDIOC_READDISKGUID,
    CDIOC_GETDISKTYPE,
    CDIOC_GETERROR,
    CDIOC_TRAYREQ,
    CDIOC_STATUS,
    CDIOC_POWEROFF,
    CDIOC_MMODE,
    CDIOC_DISKRDY,
    CDIOC_READMODELID,
    CDIOC_STREAMINIT,
    CDIOC_BREAK,

    CDIOC_SPINNOM = CDIOC_FNUM(0x80),
    CDIOC_SPINSTM,
    CDIOC_TRYCNT,
    CDIOC_SEEK,
    CDIOC_STANDBY,
    CDIOC_STOP,
    CDIOC_PAUSE,
    CDIOC_GETTOC,
    CDIOC_SETTIMEOUT,
    CDIOC_READDVDDUALINFO,
    CDIOC_INIT,

    CDIOC_GETINTREVENTFLG = CDIOC_FNUM(0x91),
};

// DMA/reading alignment correction buffer. Used by CDVDMAN and CDVDFSV.
#define CDVDMAN_FS_SECTORS 6 //The size of the actual buffer within CDVDMAN is CDVDMAN_FS_SECTORS+2.

//Codes for use with sceCdSC()
#define CDSC_GET_DEBUG_STATUS 0xFFFFFFF0 //Get debug status flag.
#define CDSC_GET_INTRFLAG 0xFFFFFFF5     //Get interrupt flag.
#define CDSC_IO_SEMA 0xFFFFFFF6          //Wait (param != 0) or signal (param == 0) high-level I/O semaphore.
#define CDSC_GET_VERSION 0xFFFFFFF7      //Get CDVDMAN version.
#define CDSC_SET_ERROR 0xFFFFFFFE        //Used by CDVDFSV and CDVDSTM to set the error code (Typically READCF*).

// exported functions prototypes
#define cdvdman_IMPORTS_start DECLARE_IMPORT_TABLE(cdvdman, 1, 1)
#define cdvdman_IMPORTS_end END_IMPORT_TABLE

int sceCdInit(int init_mode); // #4
#define I_sceCdInit DECLARE_IMPORT(4, sceCdInit)
int sceCdStandby(void); // #5
#define I_sceCdStandby DECLARE_IMPORT(5, sceCdStandby)
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode); // #6
#define I_sceCdRead DECLARE_IMPORT(6, sceCdRead)
int sceCdSeek(u32 lsn); // #7
#define I_sceCdSeek DECLARE_IMPORT(7, sceCdSeek)
int sceCdGetError(void); // #8
#define I_sceCdGetError DECLARE_IMPORT(8, sceCdGetError)
int sceCdGetToc(void *toc); // #9
#define I_sceCdGetToc DECLARE_IMPORT(9, sceCdGetToc)
int sceCdSearchFile(cd_file_t *fp, const char *name); // #10
#define I_sceCdSearchFile DECLARE_IMPORT(10, sceCdSearchFile)
int sceCdSync(int mode); // #11
#define I_sceCdSync DECLARE_IMPORT(11, sceCdSync)
int sceCdGetDiskType(void); // #12
#define I_sceCdGetDiskType DECLARE_IMPORT(12, sceCdGetDiskType)
int sceCdDiskReady(int mode); // #13
#define I_sceCdDiskReady DECLARE_IMPORT(13, sceCdDiskReady)
int sceCdTrayReq(int mode, u32 *traycnt); // #14
#define I_sceCdTrayReq DECLARE_IMPORT(14, sceCdTrayReq)
int sceCdStop(void); // #15
#define I_sceCdStop DECLARE_IMPORT(15, sceCdStop)
int sceCdPosToInt(cd_location_t *p); // #16
#define I_sceCdPosToInt DECLARE_IMPORT(16, sceCdPosToInt)
cd_location_t *sceCdIntToPos(int i, cd_location_t *p); // #17
#define I_sceCdIntToPos DECLARE_IMPORT(17, sceCdIntToPos)
int sceCdRI(char *buf, int *stat); // #22
#define I_ssceCdRI DECLARE_IMPORT(22, sceCdRI)
int sceCdReadClock(cd_clock_t *rtc); // #24
#define I_sceCdReadClock DECLARE_IMPORT(24, sceCdReadClock)
int sceCdStatus(void); // #28
#define I_sceCdStatus DECLARE_IMPORT(28, sceCdStatus)
int sceCdApplySCmd(int cmd, void *in, u32 in_size, void *out); // #29
#define I_sceCdApplySCmd DECLARE_IMPORT(29, sceCdApplySCmd)
int *sceCdCallback(void *func); // #37
#define I_sceCdCallback DECLARE_IMPORT(37, sceCdCallback)
int sceCdPause(void); // #38
#define I_sceCdPause DECLARE_IMPORT(38, sceCdPause)
int sceCdBreak(void); // #39
#define I_sceCdBreak DECLARE_IMPORT(39, sceCdBreak)
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode); // #40
#define I_sceCdReadCdda DECLARE_IMPORT(40, sceCdReadCdda)
int sceCdGetReadPos(void); // #44
#define I_sceCdGetReadPos DECLARE_IMPORT(44, sceCdGetReadPos)
void *sceGetFsvRbuf(void); // #47
#define I_sceGetFsvRbuf DECLARE_IMPORT(47, sceGetFsvRbuf)
int sceCdSC(int code, int *param); // #50
#define I_sceCdSC DECLARE_IMPORT(50, sceCdSC)
int sceCdRC(cd_clock_t *rtc); // #51
#define I_sceCdRC DECLARE_IMPORT(51, sceCdRC)
int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr); // #56
#define I_sceCdStInit DECLARE_IMPORT(56, sceCdStInit)
int sceCdStRead(u32 sectors, void *buf, u32 mode, u32 *err); // #57
#define I_sceCdStRead DECLARE_IMPORT(57, sceCdStRead)
int sceCdStSeek(u32 lsn); // #58
#define I_sceCdStSeek DECLARE_IMPORT(58, sceCdStSeek)
int sceCdStStart(u32 lsn, cd_read_mode_t *mode); // #59
#define I_sceCdStStart DECLARE_IMPORT(59, sceCdStStart)
int sceCdStStat(void); // #60
#define I_sceCdStStat DECLARE_IMPORT(60, sceCdStStat)
int sceCdStStop(void); // #61
#define I_sceCdStStop DECLARE_IMPORT(61, sceCdStStop)
int sceCdRead0(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode); // #62
#define I_sceCdRead0 DECLARE_IMPORT(62, sceCdRead0)
int sceCdRM(char *m, u32 *stat); // #64
#define I_sceCdRM DECLARE_IMPORT(64, sceCdRM)
int sceCdStPause(void); // #67
#define I_sceCdStPause DECLARE_IMPORT(67, sceCdStPause)
int sceCdStResume(void); // #68
#define I_sceCdStResume DECLARE_IMPORT(68, sceCdStResume)
int sceCdPowerOff(int *stat); // #74
#define I_sceCdPowerOff DECLARE_IMPORT(74, sceCdPowerOff)
int sceCdMmode(int mode); // #75
#define I_sceCdMmode DECLARE_IMPORT(75, sceCdMmode)
int sceCdStSeekF(u32 lsn); // #77
#define I_sceCdStSeekF DECLARE_IMPORT(77, sceCdStSeekF)
int sceCdReadDiskID(void *DiskID); // #79
#define I_sceCdReadDiskID DECLARE_IMPORT(79, sceCdReadDiskID)
int sceCdReadGUID(void *GUID); // #80
#define I_sceCdReadGUID DECLARE_IMPORT(80, sceCdReadGUID)
int sceCdSetTimeout(int param, int timeout); // #81
#define I_sceCdSetTimeout DECLARE_IMPORT(81, sceCdSetTimeout)
int sceCdReadModelID(void *ModelID); // #82
#define I_sceCdReadModelID DECLARE_IMPORT(82, sceCdReadModelID)
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start); // #83
#define I_sceCdReadDvdDualInfo DECLARE_IMPORT(83, sceCdReadDvdDualInfo)
int sceCdLayerSearchFile(cdl_file_t *fp, const char *name, int layer); // #84
#define I_sceCdLayerSearchFile DECLARE_IMPORT(84, sceCdLayerSearchFile)
