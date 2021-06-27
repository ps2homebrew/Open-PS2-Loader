
#ifndef __CDVDMAN__
#define __CDVDMAN__

// PS2 CDVD hardware registers
#define CDL_DATA_RDY      0x01
#define CDL_DATA_COMPLETE 0x02
#define CDL_DATA_END      0x04

#define CDVDreg_NCOMMAND (*(volatile unsigned char *)0xBF402004)
#define CDVDreg_READY    (*(volatile unsigned char *)0xBF402005)
#define CDVDreg_NDATAIN  (*(volatile unsigned char *)0xBF402005)
#define CDVDreg_ERROR    (*(volatile unsigned char *)0xBF402006)
#define CDVDreg_HOWTO    (*(volatile unsigned char *)0xBF402006)
#define CDVDreg_ABORT    (*(volatile unsigned char *)0xBF402007)
#define CDVDreg_PWOFF    (*(volatile unsigned char *)0xBF402008)
#define CDVDreg_9        (*(volatile unsigned char *)0xBF402009)
#define CDVDreg_STATUS   (*(volatile unsigned char *)0xBF40200A)
#define CDVDreg_B        (*(volatile unsigned char *)0xBF40200B)
#define CDVDreg_C        (*(volatile unsigned char *)0xBF40200C)
#define CDVDreg_D        (*(volatile unsigned char *)0xBF40200D)
#define CDVDreg_E        (*(volatile unsigned char *)0xBF40200E)
#define CDVDreg_TYPE     (*(volatile unsigned char *)0xBF40200F)
#define CDVDreg_13       (*(volatile unsigned char *)0xBF402013)
#define CDVDreg_SCOMMAND (*(volatile unsigned char *)0xBF402016)
#define CDVDreg_SDATAIN  (*(volatile unsigned char *)0xBF402017)
#define CDVDreg_SDATAOUT (*(volatile unsigned char *)0xBF402018)
#define CDVDreg_KEYSTATE (*(volatile unsigned char *)0xBF402038)
#define CDVDreg_KEYXOR   (*(volatile unsigned char *)0xBF402039)
#define CDVDreg_DEC      (*(volatile unsigned char *)0xBF40203A)

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
//The minimum size is 2, as one sector may be used for buffer alignment correction.
#define CDVDMAN_FS_SECTORS 8

//Codes for use with sceCdSC()
#define CDSC_GET_DEBUG_STATUS 0xFFFFFFF0 //Get debug status flag.
#define CDSC_GET_INTRFLAG     0xFFFFFFF5 //Get interrupt flag.
#define CDSC_IO_SEMA          0xFFFFFFF6 //Wait (param != 0) or signal (param == 0) high-level I/O semaphore.
#define CDSC_GET_VERSION      0xFFFFFFF7 //Get CDVDMAN version.
#define CDSC_SET_ERROR        0xFFFFFFFE //Used by CDVDFSV and CDVDSTM to set the error code (Typically READCF*).
#define CDSC_OPL_SHUTDOWN     0x00000001 //Shutdown OPL

#endif
