/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "smsutils.h"
#include "mass_common.h"
#include "mass_stor.h"
#include "dev9.h"
#include "smb.h"
#include "atad.h"
#include "ioplib_util.h"

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <ioman.h>
#include <thsemap.h>
#include <thmsgbx.h>
#include <errno.h>
#include <io_common.h>
#include <usbd.h>
#include <ps2ip.h>
#include "ioman_add.h"

#ifdef __IOPCORE_DEBUG
#define DPRINTF(args...)	printf(args)
#else
#define DPRINTF(args...)	do { } while(0)
#endif

#define MODNAME "dev9"
IRX_ID(MODNAME, 2, 8);


//------------------ Patch Zone ----------------------
#ifdef SMB_DRIVER
static char g_ISO_name[] = "######    GAMESETTINGS    ######";
#else
static char g_tag[] = "######    GAMESETTINGS    ######";
#endif
static char g_ISO_parts=0x69;
static char g_ISO_media=0x69;
static char g_gamesetting_alt_read=0;
static int g_gamesetting_disable_DVDDL=0;
static int g_gamesetting_0_pss=0;

#define ISO_MAX_PARTS	16
static int g_part_start[ISO_MAX_PARTS] = {
	0,	// is apa header LBA in HDD use, is Long filename in SMB+ISO
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
}; 

static char g_DiskID[] = "\0B00BS"; 	// the null byte is here to ensure the ISO name string 
					// (stored in array above) is terminated when it's 64 bytes

#ifdef SMB_DRIVER
static char g_pc_ip[]="xxx.xxx.xxx.xxx";
static int g_pc_port = 445; // &g_pc_ip + 16
static char g_pc_share[36]="PS2SMB";
static char g_smb_user[36]="GUEST";
static char g_smb_password[36]="\0";
#endif

//----------------------------------------------------

int *p_part_start = &g_part_start[0];

extern void *dummy_irx;
extern int size_dummy_irx;

struct irx_export_table _exp_cdvdman;
struct irx_export_table _exp_cdvdstm;
#ifdef SMB_DRIVER
struct irx_export_table _exp_dev9;
#endif
#ifdef HDD_DRIVER
struct irx_export_table _exp_dev9;
struct irx_export_table _exp_atad;
#endif
#ifdef USB_DRIVER
#ifdef __USE_DEV9
struct irx_export_table _exp_dev9;
#endif
#endif
struct irx_export_table _exp_smsutils;
#ifdef VMC_DRIVER
struct irx_export_table _exp_oplutils;
#endif

// PS2 CDVD hardware registers
#define CDVDreg_NCOMMAND      (*(volatile unsigned char *)0xBF402004)
#define CDVDreg_READY         (*(volatile unsigned char *)0xBF402005)
#define CDVDreg_NDATAIN       (*(volatile unsigned char *)0xBF402005)
#define CDVDreg_ERROR         (*(volatile unsigned char *)0xBF402006)
#define CDVDreg_HOWTO         (*(volatile unsigned char *)0xBF402006)
#define CDVDreg_ABORT         (*(volatile unsigned char *)0xBF402007)
#define CDVDreg_PWOFF         (*(volatile unsigned char *)0xBF402008)
#define CDVDreg_9             (*(volatile unsigned char *)0xBF402008)
#define CDVDreg_STATUS        (*(volatile unsigned char *)0xBF40200A)
#define CDVDreg_B             (*(volatile unsigned char *)0xBF40200B)
#define CDVDreg_C             (*(volatile unsigned char *)0xBF40200C)
#define CDVDreg_D             (*(volatile unsigned char *)0xBF40200D)
#define CDVDreg_E             (*(volatile unsigned char *)0xBF40200E)
#define CDVDreg_TYPE          (*(volatile unsigned char *)0xBF40200F)
#define CDVDreg_13            (*(volatile unsigned char *)0xBF402013)
#define CDVDreg_SCOMMAND      (*(volatile unsigned char *)0xBF402016)
#define CDVDreg_SDATAIN       (*(volatile unsigned char *)0xBF402017)
#define CDVDreg_SDATAOUT      (*(volatile unsigned char *)0xBF402018)
#define CDVDreg_KEYSTATE      (*(volatile unsigned char *)0xBF402038)
#define CDVDreg_KEYXOR        (*(volatile unsigned char *)0xBF402039)
#define CDVDreg_DEC           (*(volatile unsigned char *)0xBF40203A)

// user cb CDVD events 
#define SCECdFuncRead         1
#define SCECdFuncReadCDDA     2
#define SCECdFuncGetToc       3
#define SCECdFuncSeek         4
#define SCECdFuncStandby      5
#define SCECdFuncStop         6
#define SCECdFuncPause        7
#define SCECdFuncBreak        8

// Modes for cdInit()
#define CDVD_INIT_INIT		0x00		// init cd system and wait till commands can be issued
#define CDVD_INIT_NOCHECK	0x01		// init cd system
#define CDVD_INIT_EXIT		0x05		// de-init system

// CDVD stats
#define CDVD_STAT_STOP		0x00		// disc has stopped spinning
#define CDVD_STAT_OPEN		0x01		// tray is open
#define CDVD_STAT_SPIN		0x02		// disc is spinning
#define CDVD_STAT_READ		0x06		// reading from disc
#define CDVD_STAT_PAUSE		0x0A		// disc is paused
#define CDVD_STAT_SEEK		0x12		// disc is seeking
#define CDVD_STAT_ERROR		0x20		// error occurred

// cdTrayReq() values
#define CDVD_TRAY_OPEN		0		// Tray Open
#define CDVD_TRAY_CLOSE		1		// Tray Close
#define CDVD_TRAY_CHECK		2		// Tray Check

// sceCdDiskReady() values
#define CDVD_READY_READY	0x02
#define CDVD_READY_NOTREADY	0x06

#define CdSpinMax		0
#define CdSpinNom		1
#define CdSpinStm		0

// cdGetError() return values
#define CDVD_ERR_FAIL		-1		// error in cdGetError()
#define CDVD_ERR_NO		0x00		// no error occurred
#define CDVD_ERR_ABRT		0x01		// command was aborted due to cdBreak() call
#define CDVD_ERR_CMD		0x10		// unsupported command
#define CDVD_ERR_OPENS		0x11		// tray is open
#define CDVD_ERR_NODISC		0x12		// no disk inserted
#define CDVD_ERR_NORDY		0x13		// drive is busy processing another command
#define CDVD_ERR_CUD		0x14		// command unsupported for disc currently in drive
#define CDVD_ERR_IPI		0x20		// sector address error
#define CDVD_ERR_ILI		0x21		// num sectors error
#define CDVD_ERR_PRM		0x22		// command parameter error
#define CDVD_ERR_READ		0x30		// error while reading
#define CDVD_ERR_TRMOPN		0x31		// tray was opened
#define CDVD_ERR_EOM		0x32		// outermost error
#define CDVD_ERR_READCF		0xFD		// error setting command
#define CDVD_ERR_READCFR  	0xFE		// error setting command

struct dirTocEntry {
	short	length;
	u32	fileLBA;			// 2
	u32	fileLBA_bigend;			// 6
	u32	fileSize;			// 10
	u32	fileSize_bigend;		// 14
	u8	dateStamp[6];			// 18
	u8	reserved1;			// 24
	u8	fileProperties;			// 25
	u8	reserved2[6];			// 26
	u8	filenameLength;			// 32
	char	filename[128];			// 33
} __attribute__((packed));

typedef struct {
	u8 stat;
	u8 second;
	u8 minute;
	u8 hour;
	u8 week;
	u8 day;
	u8 month;
	u8 year;
} cd_clock_t;

typedef struct {
	u32	lsn;
	u32	size;
	char	name[16];
	u8	date[8];
} cd_file_t;

typedef struct {
	u32	lsn;
	u32	size;
	char	name[16];
	u8	date[8];
	u32	flag;
} cdl_file_t;

typedef struct {
	u8 minute;
	u8 second;
	u8 sector;
	u8 track;
} cd_location_t;

typedef struct {
	u8 trycount;
	u8 spindlctrl;
	u8 datapattern;
	u8 pad;
} cd_read_mode_t;

typedef struct {
	int err;
	int status;
	int Ststat;
	int Stbufmax;
	int Stlsn;
	int intr_ef;
	int disc_type_reg;
#ifdef ALT_READ_CORE
	int cdNCmd;
	int cddiskready;
#else		
	u32 cdread_lba;
	u32 cdread_sectors;
	void *cdread_buf;
	cd_read_mode_t cdread_mode;
#endif
} cdvdman_status_t;

typedef struct {
	iop_file_t *f;
	u32 lsn;
	u32 filesize;
	u32 position;
} FHANDLE;

// exported functions prototypes
int sceCdInit(int init_mode);							// #4
int sceCdStandby(void);								// #5
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode); 		// #6
int sceCdSeek(u32 lsn);								// #7
int sceCdGetError(void); 							// #8
int sceCdGetToc(void *toc); 							// #9
int sceCdSearchFile(cd_file_t *fp, const char *name); 				// #10
int sceCdSync(int mode); 							// #11
int sceCdGetDiskType(void);							// #12
int sceCdDiskReady(int mode); 							// #13
int sceCdTrayReq(int mode, u32 *traycnt); 					// #14
int sceCdStop(void); 								// #15
int sceCdPosToInt(cd_location_t *p); 						// #16
cd_location_t *sceCdIntToPos(int i, cd_location_t *p);				// #17
int sceCdRI(char *buf, int *stat); 						// #22
int sceCdReadClock(cd_clock_t *rtc); 						// #24
int sceCdStatus(void); 								// #28
int sceCdApplySCmd(int cmd, void *in, u32 in_size, void *out); 			// #29
int *sceCdCallback(void *func); 						// #37
int sceCdPause(void);								// #38
int sceCdBreak(void);								// #39
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode);	// #40
int sceCdGetReadPos(void); 							// #44
int sceCdSC(int code, int *param);						// #50
int sceCdRC(cd_clock_t *rtc);		 					// #51
int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr); 			// #56
int sceCdStRead(u32 sectors, void *buf, u32 mode, u32 *err); 			// #57
int sceCdStSeek(u32 lsn);							// #58
int sceCdStStart(u32 lsn, cd_read_mode_t *mode);				// #59
int sceCdStStat(void); 								// #60
int sceCdStStop(void);								// #61
int sceCdRead0(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode);		// #62
int sceCdRM(char *m, u32 *stat);						// #64
int sceCdStPause(void);								// #67
int sceCdStResume(void); 							// #68
int sceCdStSeekF(u32 lsn); 							// #77
int sceCdReadDiskID(void *DiskID);						// #79
int sceCdReadGUID(void *GUID);							// #80
int sceCdReadModelID(void *ModelID);						// #82
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start); 			// #83
int sceCdLayerSearchFile(cdl_file_t *fp, const char *name, int layer);		// #84

// internal functions prototypes
void usbd_init(void);
void ps2ip_init(void);
void fs_init(void);
void cdvdman_cdinit();
int cdvdman_ReadSect(u32 lsn, u32 nsectors, void *buf);
int cdvdman_readMechaconVersion(char *mname, u32 *stat);
int cdvdman_readID(int mode, u8 *buf);
FHANDLE *cdvdman_getfilefreeslot(void);
void cdvdman_trimspaces(char* str);
struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer);
int cdvdman_findfile(cd_file_t *pcd_file, const char *name, int layer);
int cdvdman_writeSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size);
int cdvdman_sendSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size);
int cdvdman_cb_event(int reason);
unsigned int event_alarm_cb(void *args);
void cdvdman_startThreads(void);
void cdvdman_create_semaphores(void);
void cdvdman_initdev(void);
void cdvdman_get_part_specs(u32 lsn);

// !!! usbd exports functions pointers !!!
int (*pUsbRegisterDriver)(UsbDriver *driver); 								// #4
void *(*pUsbGetDeviceStaticDescriptor)(int devId, void *data, u8 type); 				// #6
int (*pUsbSetDevicePrivateData)(int devId, void *data); 						// #7
int (*pUsbOpenEndpoint)(int devId, UsbEndpointDescriptor *desc); 					// #9
int (*pUsbCloseEndpoint)(int id); 									// #10
int (*pUsbTransfer)(int id, void *data, u32 len, void *option, UsbCallbackProc callback, void *cbArg); 	// #11
int (*pUsbOpenEndpointAligned)(int devId, UsbEndpointDescriptor *desc); 				// #12

// !!! ps2ip exports functions pointers !!!
int (*plwip_close)(int s); 										// #6
int (*plwip_connect)(int s, struct sockaddr *name, socklen_t namelen); 					// #7
int (*plwip_recvfrom)(int s, void *header, int hlen, void *payload, int plen, unsigned int flags, struct sockaddr *from, socklen_t *fromlen);	// #10
int (*plwip_send)(int s, void *dataptr, int size, unsigned int flags); 					// #11
int (*plwip_socket)(int domain, int type, int protocol); 						// #13
u32 (*pinet_addr)(const char *cp); 									// #24

// for "cdrom" devctl
#define CDIOC_CMDBASE		0x430C

// for "cdrom" ioctl2
#define CIOCSTREAMPAUSE		0x630D
#define CIOCSTREAMRESUME	0x630E
#define CIOCSTREAMSTAT		0x630F

// driver ops protypes
int cdrom_dummy(void);
int cdrom_init(iop_device_t *dev);
int cdrom_deinit(iop_device_t *dev);
int cdrom_open(iop_file_t *f, char *filename, int mode);
int cdrom_close(iop_file_t *f);
int cdrom_read(iop_file_t *f, void *buf, u32 size);
int cdrom_lseek(iop_file_t *f, u32 offset, int where);
int cdrom_getstat(iop_file_t *f, char *filename, iox_stat_t *stat);
int cdrom_dopen(iop_file_t *f, char *dirname);
int cdrom_dread(iop_file_t *f, iox_dirent_t *dirent);
int cdrom_dclose(iop_file_t *f);
int cdrom_ioctl(iop_file_t *f, u32 cmd, void *args);
s64 cdrom_lseek64(iop_file_t *f, s64 pos, int where);
int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen);
int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, u32 arglen, void *buf, u32 buflen);

// driver ops func tab
void *cdrom_ops[27] = {
	(void*)cdrom_init,
	(void*)cdrom_deinit,
	(void*)cdrom_dummy,
	(void*)cdrom_open,
	(void*)cdrom_close,
	(void*)cdrom_read,
	(void*)cdrom_dummy,
	(void*)cdrom_lseek,
	(void*)cdrom_ioctl,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_dopen,
	(void*)cdrom_dclose,
	(void*)cdrom_dread,
	(void*)cdrom_getstat,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_lseek64,
	(void*)cdrom_devctl,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_ioctl2	
};

// driver descriptor
static iop_ext_device_t cdrom_dev = {
	"cdrom", 
	IOP_DT_FS | IOP_DT_FSEXT,
	1,
	"CD-ROM ",
	(struct _iop_ext_device_ops *)&cdrom_ops
};

// devctl funcs
int (*devctl_fn)(void *args, void *buf);
int devctl_dummy(void *args, void *buf);
int devctl_dummy2(void *args, void *buf);
int devctl_retonly(void *args, void *buf);
int devctl_cdreadclock(void *args, void *buf);
int devctl_cdreadGUID(void *args, void *buf);
int devctl_cdreadDiskID(void *args, void *buf);
int devctl_cdgetdisktype(void *args, void *buf);
int devctl_cdgeterror(void *args, void *buf);
int devctl_cdtrayreq(void *args, void *buf);
int devctl_cdstatus(void *args, void *buf);
int devctl_cddiskready(void *args, void *buf);
int devctl_cdreadModelID(void *args, void *buf);
int devctl_cdStinit(void *args, void *buf);
int devctl_cdabort(void *args, void *buf);
int devctl_cdstandby(void *args, void *buf);
int devctl_cdstop(void *args, void *buf);
int devctl_cdpause(void *args, void *buf);
int devctl_cdgettoc(void *args, void *buf);
int devctl_intref(void *args, void *buf);

// devctl funcs array
void *devctl_tab[134] = {
    (void *)devctl_cdreadclock,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_cdreadGUID,
	(void *)devctl_cdreadDiskID,
	(void *)devctl_cdgetdisktype,
	(void *)devctl_cdgeterror,
	(void *)devctl_cdtrayreq,
	(void *)devctl_cdstatus,
	(void *)devctl_retonly,
	(void *)devctl_retonly,
	(void *)devctl_cddiskready,
	(void *)devctl_cdreadModelID,
	(void *)devctl_cdStinit,
	(void *)devctl_cdabort,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy2,
	(void *)devctl_dummy2,
	(void *)devctl_dummy2,
	(void *)devctl_dummy,
	(void *)devctl_cdstandby,
	(void *)devctl_cdstop,
	(void *)devctl_cdpause,
	(void *)devctl_cdgettoc,
	(void *)devctl_dummy,
	(void *)devctl_dummy2,
	(void *)devctl_dummy2,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_dummy,
	(void *)devctl_intref
};

#define MAX_FDHANDLES 		64
FHANDLE cdvdman_fdhandles[MAX_FDHANDLES];

typedef struct {
	u32 rootDirtocLBA;
	u32 rootDirtocLength;
} layer_info_t;

layer_info_t layer_info[2];

cdvdman_status_t cdvdman_stat;
static void *user_cb;

#ifndef HDD_DRIVER
static int cdvdman_layer1start = 0;
#endif

static int cdrom_io_sema;
static int cdvdman_cdreadsema;
static int cdvdman_searchfilesema;

#ifndef ALT_READ_CORE
static int cdvdman_lockreadsema;
static int sync_flag;
#endif

static char cdvdman_dirname[256];
static char cdvdman_filepath[256];
static char cdvdman_curdir[256];

// buffers
#define CDVDMAN_BUF_SECTORS 	2
static u8 cdvdman_buf[CDVDMAN_BUF_SECTORS*2048] __attribute__((aligned(64))); 

#define CDVDMAN_FS_SECTORS	2
#define CDVDMAN_FS_BUFSIZE	CDVDMAN_FS_SECTORS * 2048
static u8 cdvdman_fs_buf[CDVDMAN_FS_BUFSIZE + 2*2048] __attribute__((aligned(64))); 

iop_sys_clock_t cdvdman_sysclock;

static int fs_inited = 0;

#ifdef HDD_DRIVER
int lba_48bit = 0;
int atad_inited = 0;
static hdl_apa_header apaHeader;

typedef struct {
	u32 part_offset; 	// in MB
	u32 data_start; 	// in sectors
	u32 part_size; 		// in KB
} cdvdman_partspecs_t;

cdvdman_partspecs_t cdvdman_partspecs;
#endif

#define CDVDMAN_MODULE_VERSION 0x225
static int cdvdman_debug_print_flag = 0;

static int cdvdman_no_tray_model = 0;
static int cdvdman_media_changed = 1;

static int cdvdman_cur_disc_type = 0;	/* real current disc type */
unsigned int ReadPos = 0;		/* Current buffer offset in 2048-byte sectors. */

//-------------------------------------------------------------------------
#ifdef ALT_READ_CORE

#define NCMD_INIT 		0x00
#define NCMD_READ 		0x01
#define NCMD_READCDDA 		0x02
#define NCMD_SEEK 		0x03
#define NCMD_STANDBY 		0x04
#define NCMD_STOP 		0x05
#define NCMD_PAUSE 		0x06

static int cdvdman_NCmdlocksema;
static int cdvdman_NCmdsema;
static int cdvdman_NCmdsemacount = 0;
static int cdvdman_mbxid;

typedef struct {
	int ncmd;
	u8 nbuf[28];
} cdvdman_NCmd_t;

typedef struct NCmdMbx {
	iop_message_t iopmsg;
	cdvdman_NCmd_t NCmdmsg;
	struct NCmdMbx *next;
} NCmdMbx_t;

cdvdman_NCmd_t cdvdman_NCmd;

static void *cdvdman_pMbxnext;
static void *cdvdman_pMbxcur;
static void *cdvdman_pMbxbuf = NULL;

#define NCMD_NUMBER		16
static u8 cdvdman_Mbxbuf[NCMD_NUMBER*sizeof(NCmdMbx_t)];

NCmdMbx_t *cdvdman_setNCmdMbx(void);
void cdvdman_getNCmdMbx(NCmdMbx_t *mbxbuf);
void cdvdman_sendNCmdMbx(int mbxid, cdvdman_NCmd_t *NCmdmsg, int size);
cdvdman_NCmd_t *cdvdman_receiveNCmdMbx(int mbxid);
void cdvdman_startNCmdthread(void);
void cdvdman_NCmdthread(void *args);
int cdvdman_sendNCmd(u8 ncmd, void *ndata, int ndlen);
void cdvdman_waitNCmdsema(void);
void cdvdman_signalNCmdsema(void);
void cdvdman_waitsignalNCmdsema(void);
int cdvdman_getNCmdstate(void);
void cdvdman_NCmdCall(u8 ncmd, void *ndata);
void (*NCmd_fn)(void *ndata);
void NCmd_cdInit(void *ndata);
void NCmd_cdRead(void *ndata);
void NCmd_cdReadCDDA(void *ndata);
void NCmd_cdSeek(void *ndata);
void NCmd_cdStandby(void *ndata);
void NCmd_cdStop(void *ndata);
void NCmd_cdPause(void *ndata);
int (*cdSync_fn)(void);
int cdSync_blk(void);
int cdSync_noblk(void);
int cdSync_dummy(void);

// NCmd funcs array
void *NCmd_tab[7] = {
    (void *)NCmd_cdInit,
    (void *)NCmd_cdRead,
    (void *)NCmd_cdReadCDDA,
    (void *)NCmd_cdSeek,
    (void *)NCmd_cdStandby,
    (void *)NCmd_cdStop,
    (void *)NCmd_cdPause
};

// cdSync funcs array
void *cdSync_tab[18] = {
    (void *)cdSync_blk,
    (void *)cdSync_noblk,
    (void *)cdSync_dummy,
    (void *)cdSync_blk,
    (void *)cdSync_dummy,
    (void *)cdSync_blk,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_dummy,
    (void *)cdSync_blk,
    (void *)cdSync_noblk
};

//-------------------------------------------------------------- 
NCmdMbx_t *cdvdman_setNCmdMbx(void)
{
	int i, oldstate;
	NCmdMbx_t *pmbx;

	if (!cdvdman_pMbxbuf) {
		// Here we init the thread message buffer
		cdvdman_pMbxbuf = cdvdman_Mbxbuf;
		mips_memset(cdvdman_pMbxbuf, 0, sizeof(NCmdMbx_t) * NCMD_NUMBER);

		pmbx = (NCmdMbx_t *)cdvdman_pMbxbuf;

		for (i = (NCMD_NUMBER-2); i >= 0; i--) {
			pmbx->next = (NCmdMbx_t *)(pmbx + 1);
			pmbx++; 
		}

		cdvdman_pMbxnext = cdvdman_pMbxbuf;
		pmbx->next = NULL;
	}

	if (!cdvdman_pMbxnext)
		return (NCmdMbx_t *)NULL;

	pmbx = (NCmdMbx_t *)cdvdman_pMbxnext;
	cdvdman_pMbxnext = pmbx->next;
	pmbx->next = (NCmdMbx_t *)cdvdman_pMbxcur;

	cdvdman_pMbxcur = (void *)pmbx;

	return (NCmdMbx_t *)pmbx;
}

//-------------------------------------------------------------- 
void cdvdman_getNCmdMbx(NCmdMbx_t *pmbx)
{	
	cdvdman_pMbxcur = (void *)pmbx->next;
	pmbx->next = (NCmdMbx_t *)cdvdman_pMbxnext;

	cdvdman_pMbxnext = (void *)pmbx;
}

//-------------------------------------------------------------- 
void cdvdman_sendNCmdMbx(int mbxid, cdvdman_NCmd_t *NCmdmsg, int size)
{
	NCmdMbx_t *pmbx;	

	pmbx = cdvdman_setNCmdMbx();
	
	if (size > sizeof(cdvdman_NCmd_t)) 	// 32
		size = sizeof(cdvdman_NCmd_t); 	// 32

	if (size)
		mips_memcpy((void *)&pmbx->NCmdmsg, (void *)NCmdmsg, size);

	if (QueryIntrContext())
		iSendMbx(mbxid, (void *)pmbx);
	else
		SendMbx(mbxid, (void *)pmbx);
}

//-------------------------------------------------------------- 
cdvdman_NCmd_t *cdvdman_receiveNCmdMbx(int mbxid)
{
	NCmdMbx_t *pmbx;
	int r;

	r = ReceiveMbx((void **)&pmbx, mbxid);
	if (r < 0)
		return (cdvdman_NCmd_t *)NULL;

	cdvdman_getNCmdMbx(pmbx);

	return (cdvdman_NCmd_t *)(&pmbx->NCmdmsg);
}

//-------------------------------------------------------------- 
int cdvdman_sendNCmd(u8 ncmd, void *ndata, int ndlen)
{
	if (cdvdman_NCmdlocksema) {
		if (cdvdman_stat.cdNCmd)
			return 0;

		WaitSema(cdvdman_NCmdsema);
	}

	cdvdman_stat.cdNCmd = 1;
	cdvdman_NCmd.ncmd = ncmd;

	if (ndlen)
		mips_memcpy(cdvdman_NCmd.nbuf, ndata, ndlen);

	cdvdman_sendNCmdMbx(cdvdman_mbxid, (cdvdman_NCmd_t *)&cdvdman_NCmd, sizeof(cdvdman_NCmd_t));

	return 1;		
}

//-------------------------------------------------------------- 
void cdvdman_waitNCmdsema(void)
{
	if (!cdvdman_NCmdsemacount) {
		cdvdman_NCmdlocksema = 0;
		WaitSema(cdvdman_NCmdsema);
	}

	cdvdman_NCmdsemacount++;
}

//-------------------------------------------------------------- 
void cdvdman_signalNCmdsema(void)
{
	if (!cdvdman_NCmdsemacount)
		return;

	cdvdman_NCmdsemacount--;

	if (!cdvdman_NCmdsemacount) {

		SignalSema(cdvdman_NCmdsema);

		cdvdman_NCmdlocksema = 1;
	}
}

//-------------------------------------------------------------- 
void cdvdman_waitsignalNCmdsema(void)
{
	WaitSema(cdvdman_NCmdsema);
	SignalSema(cdvdman_NCmdsema);
}

//-------------------------------------------------------------- 
int cdvdman_getNCmdstate(void)
{
	return cdvdman_stat.cdNCmd;
}

//-------------------------------------------------------------- 
void cdvdman_NCmdthread(void *args)
{
	cdvdman_NCmd_t *NCmd;

	while (1) {
		NCmd = cdvdman_receiveNCmdMbx(cdvdman_mbxid);

		cdvdman_NCmdCall(NCmd->ncmd, (void *)NCmd->nbuf);

		cdvdman_stat.cdNCmd = 0;

		if (cdvdman_NCmdlocksema)
			SignalSema(cdvdman_NCmdsema);

		//SetEventFlag(cdvdman_stat.cdvdman_intr_ef, 1);
	}
}

//-------------------------------------------------------------- 
void cdvdman_startNCmdthread(void)
{
	int thid;
	iop_mbx_t mbx;
	iop_sema_t smp;
	iop_thread_t thread_param;

	mips_memset(&mbx, 0, sizeof(iop_mbx_t));
	cdvdman_mbxid = CreateMbx(&mbx);

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 0;
	smp.option = 0;

	cdvdman_NCmdsema = CreateSema(&smp);
	cdvdman_NCmdlocksema = 1;
	cdvdman_NCmdsemacount = 0;
	
	thread_param.thread = (void *)cdvdman_NCmdthread;
 	thread_param.stacksize = 0x2000;
	thread_param.priority = 0x01;
	thread_param.attr = TH_C;
	thread_param.option = 0;

	thid = CreateThread(&thread_param);
	StartThread(thid, NULL);
}

//-------------------------------------------------------------- 
void cdvdman_NCmdCall(u8 ncmd, void *ndata)
{
	if ((u32)(ncmd >= 7))
		return;

	NCmd_fn = NCmd_tab[ncmd];

	NCmd_fn(ndata);
}

//-------------------------------------------------------------- 
void NCmd_cdInit(void *ndata)
{
	cdvdman_cdinit();
}

//-------------------------------------------------------------- 
void NCmd_cdRead(void *ndata)
{
	int r;
	u32 lsn, sectors;
	void *buf;
	u8 *wdbuf = (u8 *)ndata;

	cdvdman_stat.err = CDVD_ERR_NO;

	lsn = *((u32 *)&wdbuf[0]);
	sectors = *((u32 *)&wdbuf[4]);
	buf = (void *)*((u32 *)&wdbuf[8]);

	r = sceCdRead0(lsn, sectors, buf, NULL);
	if (r == 0)
		cdvdman_stat.err = CDVD_ERR_READ;

	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncRead);
}

//-------------------------------------------------------------- 
void NCmd_cdReadCDDA(void *ndata)
{
	int r;
	u32 lsn, sectors;
	void *buf;
	u8 *wdbuf = (u8 *)ndata;

	cdvdman_stat.err = CDVD_ERR_NO;

	lsn = *((u32 *)&wdbuf[0]);
	sectors = *((u32 *)&wdbuf[4]);
	buf = (void *)*((u32 *)&wdbuf[8]);

	r = sceCdRead0(lsn, sectors, buf, NULL);
	if (r == 0)
		cdvdman_stat.err = CDVD_ERR_READ;

	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncReadCDDA);
}

//-------------------------------------------------------------- 
void NCmd_cdSeek(void *ndata)
{
	u32 lsn;
	u8 *wdbuf = (u8 *)ndata;

	lsn = *((u32 *)&wdbuf[0]);

	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncSeek);
}

//-------------------------------------------------------------- 
void NCmd_cdStandby(void *ndata)
{
	cdvdman_stat.status = CDVD_STAT_SPIN;

	cdvdman_cb_event(SCECdFuncStandby);
}

//-------------------------------------------------------------- 
void NCmd_cdStop(void *ndata)
{
	cdvdman_stat.status = CDVD_STAT_STOP;

	cdvdman_cb_event(SCECdFuncStop);
}

//-------------------------------------------------------------- 
void NCmd_cdPause(void *ndata)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncPause);
}

//-------------------------------------------------------------- 
int cdSync_blk(void)
{
	cdvdman_waitsignalNCmdsema();

	return 0;
}

//-------------------------------------------------------------- 
int cdSync_dummy(void)
{
	return 0;
}

//-------------------------------------------------------------- 
int cdSync_noblk(void)
{
	return cdvdman_getNCmdstate();
}

#endif // ALT_READ_CORE


//--------------------------------------------------------------
#ifdef USB_DRIVER
void usbd_init(void)
{
	modinfo_t info;
	getModInfo("usbd\0\0\0\0", &info);

	// Set functions pointers here
	pUsbRegisterDriver = info.exports[4];
	pUsbGetDeviceStaticDescriptor = info.exports[6];
	pUsbSetDevicePrivateData = info.exports[7];
	pUsbOpenEndpoint = info.exports[9];
	pUsbCloseEndpoint = info.exports[10];
	pUsbTransfer = info.exports[11];
	pUsbOpenEndpointAligned = info.exports[12];
}
#endif
#ifdef SMB_DRIVER
void ps2ip_init(void)
{
	modinfo_t info;
	getModInfo("ps2ip\0\0\0", &info);

	// Set functions pointers here
	plwip_close = info.exports[6];
	plwip_connect = info.exports[7];
	plwip_recvfrom = info.exports[10];
	plwip_send = info.exports[11];
	plwip_socket = info.exports[13];
	pinet_addr = info.exports[24];
}
#endif

//--------------------------------------------------------------
void fs_init(void)
{
	if (fs_inited)
		return;

	DPRINTF("fs_init\n");

#ifdef USB_DRIVER
	// initialize usbd exports
	usbd_init();

	FlushDcache();
	
	// initialize the mass driver
	mass_stor_init();

	// configure mass device
	while (mass_stor_configureDevice() <= 0);
#endif

#ifdef SMB_DRIVER
	register int i = 0;
	char tmp_str[255];

	ps2ip_init();

	// Open the Connection with SMB server
	smb_NegociateProtocol(g_pc_ip, g_pc_port, g_smb_user, g_smb_password);

	// zero pad the string to be sure it does not overflow
	g_pc_share[32] = '\0';

	// open a session
	smb_SessionSetupAndX();

	// Then tree connect on the share resource
	sprintf(tmp_str, "\\\\%s\\%s", g_pc_ip, g_pc_share);
	smb_TreeConnectAndX(tmp_str);

	char *path_str = "%s.%02x";
	char *game_name = NULL;

	// if g_part_start[0] not zero, then it is a plain ISO
	if (g_part_start[0] != 0) {
		path_str = (g_ISO_media == 0x12) ? "CD\\%s.%s.iso" : "DVD\\%s.%s.iso";
		game_name = (char *)&g_part_start[0];
	}

	sprintf(tmp_str, path_str, g_ISO_name, game_name);

	// Open all parts files
	do {
		smb_OpenAndX(tmp_str, (u16 *)&g_part_start[i++], 0);
		sprintf(tmp_str, "%s.%02x", g_ISO_name, i);

	} while(i < g_ISO_parts);
#endif

#ifdef HDD_DRIVER
	DPRINTF("fs_init: apa header LBA = %d\n", g_part_start[0]);

	int r = ata_device_dma_transfer(0, &apaHeader, g_part_start[0], 2, ATA_DIR_READ);
	if (r != 0)
		DPRINTF("fs_init: failed to read apa header %d\n", r);

	// checking HDL's deadfeed magic
	if (apaHeader.checksum != 0xdeadfeed)
		DPRINTF("fs_init: failed to find deadfeed magic\n");

	mips_memcpy(&cdvdman_partspecs, &apaHeader.part_specs[0], sizeof(cdvdman_partspecs));
#endif

	mips_memset(&cdvdman_fdhandles[0], 0, MAX_FDHANDLES * sizeof(FHANDLE));

	// Read the volume descriptor
	sceCdRead0(16, 1, cdvdman_buf, NULL);

	struct dirTocEntry *tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
	layer_info[0].rootDirtocLBA = tocEntryPointer->fileLBA;
	layer_info[0].rootDirtocLength = tocEntryPointer->length;

	// DVD DL support
	if (!g_gamesetting_disable_DVDDL) {
		#ifdef HDD_DRIVER
		int on_dual;
		u32 layer1_start;
		sceCdReadDvdDualInfo(&on_dual, &layer1_start);
		if (on_dual) {
			sceCdRead0(layer1_start + 16, 1, cdvdman_buf, NULL);
			tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
			layer_info[1].rootDirtocLBA = layer1_start + tocEntryPointer->fileLBA;
			layer_info[1].rootDirtocLength = tocEntryPointer->length;
		}
		#else
		u32 volSize = (*((u32 *)&cdvdman_buf[0x50]));
		sceCdRead0(volSize, 1, cdvdman_buf, NULL);
		if ((cdvdman_buf[0x00] == 1) && (!strncmp(&cdvdman_buf[0x01], "CD001", 5))) {
			cdvdman_layer1start = volSize - 16;
			tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
			layer_info[1].rootDirtocLBA = cdvdman_layer1start + tocEntryPointer->fileLBA;
			layer_info[1].rootDirtocLength = tocEntryPointer->length;
		}
		#endif
	}

	fs_inited = 1;
}

//-------------------------------------------------------------------------
void cdvdman_cdinit(void)
{
	DPRINTF("cdvdman_cdinit\n");

	fs_init();
}

//-------------------------------------------------------------------------
int sceCdInit(int init_mode)
{
	cdvdman_stat.err = CDVD_ERR_NO;

#ifdef ALT_READ_CORE
	g_gamesetting_alt_read = 0; //to shut off warning
	cdvdman_stat.cddiskready = CDVD_READY_NOTREADY;
	cdvdman_sendNCmd(NCMD_INIT, NULL, 0); 
	cdvdman_waitsignalNCmdsema();
	cdvdman_stat.cddiskready = CDVD_READY_READY;
#else
	cdvdman_cdinit();
#endif

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStandby(void)
{	
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_SPIN;

#ifdef ALT_READ_CORE	
	cdvdman_sendNCmd(NCMD_STANDBY, NULL, 0);
#else
	cdvdman_cb_event(SCECdFuncStandby);
#endif

	return 1;
}

//-------------------------------------------------------------------------
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
	DPRINTF("sceCdRead lsn=%d sectors=%d buf=%08x\n", (int)lsn, (int)sectors, (int)buf);

#ifdef ALT_READ_CORE
	u8 wdbuf[16];
	cdvdman_stat.err = CDVD_ERR_NO;

	*((u32 *)&wdbuf[0]) = lsn;
	*((u32 *)&wdbuf[4]) = sectors;
	*((u32 *)&wdbuf[8]) = (u32)buf;

	cdvdman_stat.status = CDVD_STAT_READ;

	cdvdman_sendNCmd(NCMD_READ, (void *)wdbuf, 12);
#else
	cdvdman_stat.status = CDVD_STAT_READ;
	if ((QueryIntrContext()) || (g_gamesetting_alt_read == 0)) {
		if (sync_flag == 1) {
			DPRINTF("sceCdRead: exiting (sync_flag)...\n");
			return 0;
		}

		sync_flag = 1;
		cdvdman_stat.cdread_lba = lsn;
		cdvdman_stat.cdread_sectors = sectors;
		cdvdman_stat.cdread_buf = buf;
		cdvdman_stat.cdread_mode = *mode;

		if (QueryIntrContext())
			iSignalSema(cdvdman_lockreadsema);
		else
			SignalSema(cdvdman_lockreadsema);
	}
	else {
		sync_flag = 1;
		sceCdRead0(lsn, sectors, buf, mode);

		cdvdman_stat.status = CDVD_STAT_PAUSE;
		
		cdvdman_cb_event(SCECdFuncRead);
		sync_flag = 0;
	}
#endif

	return 1;
}

//-------------------------------------------------------------------------
int sceCdSeek(u32 lsn)
{
	DPRINTF("sceCdSeek %d\n", (int)lsn);

	cdvdman_stat.err = CDVD_ERR_NO;

#ifdef ALT_READ_CORE
	u8 wdbuf[16];

	*((u32 *)&wdbuf[0]) = lsn;

	cdvdman_stat.status = CDVD_STAT_SEEK;

	cdvdman_sendNCmd(NCMD_SEEK, (void *)wdbuf, 4);
#else
	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncSeek);
#endif

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

#ifdef ALT_READ_CORE
	if (mode >= 18)
		return cdSync_dummy();

	cdSync_fn = cdSync_tab[mode];

	return cdSync_fn();
#else
	if (!sync_flag)
		return 0;

	if ((mode == 1) || (mode == 17))
		return 1;

	while (sync_flag)
		DelayThread(5000);

	return 0;
#endif
}

//-------------------------------------------------------------------------
void cdvdman_initDiskType()
{
        cdvdman_stat.err = CDVD_ERR_NO;

#ifdef HDD_DRIVER
        fs_init();

        cdvdman_cur_disc_type = (int)apaHeader.discType;
#else
        cdvdman_cur_disc_type = (int)g_ISO_media;
#endif
	cdvdman_stat.disc_type_reg = cdvdman_cur_disc_type;
        DPRINTF("DiskType=0x%x\n", cdvdman_cur_disc_type);
}

//-------------------------------------------------------------------------
int sceCdGetDiskType(void)
{
        return 	cdvdman_stat.disc_type_reg;
}

//-------------------------------------------------------------------------
int sceCdDiskReady(int mode)
{
	DPRINTF("sceCdDiskReady %d\n", mode);
	cdvdman_stat.err = CDVD_ERR_NO;

	if (fs_inited) {
		if (mode == 0) {
			while (sceCdDiskReady(1) == CDVD_READY_NOTREADY)
				DelayThread(5000);
		}

#ifdef ALT_READ_CORE
		if (!cdvdman_stat.cdNCmd) {
			if (cdvdman_stat.cddiskready)
				return CDVD_READY_READY;
		}
#else	
		if (!sync_flag)
			return CDVD_READY_READY;
#endif
	}

	return CDVD_READY_NOTREADY;
}

//-------------------------------------------------------------------------
int sceCdTrayReq(int mode, u32 *traycnt)
{
        DPRINTF("sceCdTrayReq(%d, 0x%X)\n", mode, traycnt);

        if (mode == CDVD_TRAY_CHECK) {
        
                if (traycnt)
                        *traycnt = cdvdman_media_changed;

                cdvdman_media_changed = 0;

                return 1;
        }

        if (cdvdman_no_tray_model == 1) {
                return 1;
        }

        if (mode == CDVD_TRAY_OPEN) {
                cdvdman_stat.status = CDVD_STAT_OPEN;
                cdvdman_stat.disc_type_reg = 0;

                DelayThread(11000);

                cdvdman_stat.err = CDVD_ERR_OPENS; /* not sure about this error code */

                return 1;
        }
        else if (mode == CDVD_TRAY_CLOSE) {
                DelayThread(25000);

                cdvdman_stat.status = CDVD_STAT_PAUSE; /* not sure if the status is right, may be - CDVD_STAT_SPIN */
                cdvdman_stat.err = CDVD_ERR_NO; /* not sure if this error code is suitable here */
                cdvdman_stat.disc_type_reg = cdvdman_cur_disc_type;

                cdvdman_media_changed = 1;

                return 1;
        }

        return 0;
}

//-------------------------------------------------------------------------
int sceCdStop(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;

#ifdef ALT_READ_CORE
	cdvdman_sendNCmd(NCMD_STOP, NULL, 0);
#else	
	cdvdman_stat.status = CDVD_STAT_STOP;
	cdvdman_cb_event(SCECdFuncStop);
#endif
	return 1;
}

//-------------------------------------------------------------------------
int sceCdPosToInt(cd_location_t *p)
{	// TODO: Improve with logical ops only  

	register int result;

	result = ((u32)p->minute >> 16) *  10 + ((u32)p->minute & 0xF);
	result *= 60;
	result += ((u32)p->second >> 16) * 10 + ((u32)p->second & 0xF);
	result *= 75;
	result += ((u32)p->sector >> 16) *  10 + ((u32)p->sector & 0xF);
	result -= 150;

	return result;
}

//-------------------------------------------------------------------------
cd_location_t *sceCdIntToPos(int i, cd_location_t *p)
{	// TODO: Improve with logical ops only  

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

	DPRINTF("sceCdCallback %08x\n", (int)func);

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

#ifdef ALT_READ_CORE
	cdvdman_sendNCmd(NCMD_PAUSE, NULL, 0);
#else
	cdvdman_stat.status = CDVD_STAT_PAUSE;
	cdvdman_cb_event(SCECdFuncPause);
#endif

	return 1;
}

//-------------------------------------------------------------------------
int sceCdBreak(void)
{
	DPRINTF("sceCdBreak\n");

	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;

#ifdef ALT_READ_CORE
	if (QueryIntrContext() == 0) {
		while (cdvdman_stat.cdNCmd)
			DelayThread(100000);
	}
#endif

	cdvdman_stat.err = CDVD_ERR_ABRT;
	cdvdman_cb_event(SCECdFuncBreak);

	return 1;
}

//-------------------------------------------------------------------------
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
#ifdef	ALT_READ_CORE
	u8 wdbuf[16];
	cdvdman_stat.err = CDVD_ERR_NO;

	*((u32 *)&wdbuf[0]) = lsn;
	*((u32 *)&wdbuf[4]) = sectors;
	*((u32 *)&wdbuf[8]) = (u32)buf;

	cdvdman_stat.status = CDVD_STAT_READ;

	cdvdman_sendNCmd(NCMD_READCDDA, (void *)wdbuf, 12);

	return 1;
#else
	return sceCdRead(lsn, sectors, buf, mode);
#endif	
}

//-------------------------------------------------------------------------
int sceCdGetReadPos(void)
{
	DPRINTF("sceCdGetReadPos\n");

	return ReadPos;
}

//-------------------------------------------------------------------------
int sceCdSC(int code, int *param)
{
        DPRINTF("sceCdSC(0x%X, 0x%X)\n", code, param);

        if (code == 0xFFFFFFF7) {
                return CDVDMAN_MODULE_VERSION;
        }
        else if (code == 0xFFFFFFF0) {
                *param = (int)&cdvdman_debug_print_flag;
                return 0xFF;
        }

        /* dummy result */
        return 1;
}

//-------------------------------------------------------------------------
int sceCdRC(cd_clock_t *rtc)
{
	cdvdman_stat.err = CDVD_ERR_NO;

	return cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);
}

//-------------------------------------------------------------------------
int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.Ststat = 0;
	cdvdman_stat.Stbufmax = bufmax;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStRead(u32 sectors, void *buf, u32 mode, u32 *err)
{
	sceCdRead0(cdvdman_stat.Stlsn, sectors, buf, NULL);
	cdvdman_stat.Stlsn += sectors;

	if (err)
		*err = sceCdGetError();

	return sectors;	
}

//-------------------------------------------------------------------------
int sceCdStSeek(u32 lsn)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.Stlsn = lsn;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStStart(u32 lsn, cd_read_mode_t *mode)
{
	if (mode->datapattern)
		cdvdman_stat.err = CDVD_ERR_READ;
	else {
		cdvdman_stat.err = CDVD_ERR_NO;
		cdvdman_stat.Stlsn = lsn;
		cdvdman_stat.Ststat = 0;
		cdvdman_stat.status = CDVD_STAT_PAUSE;
	}

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStStat(void)
{
	if (cdvdman_stat.Ststat == 1) 
		return 0;

	return cdvdman_stat.Stbufmax;
}

//-------------------------------------------------------------------------
int sceCdStStop(void)
{
	cdvdman_stat.Ststat = 1;
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;

	return 1;
}

//-------------------------------------------------------------------------
#ifndef HDD_DRIVER
int cdvdman_ReadSect(u32 lsn, u32 nsectors, void *buf)
{
	register u32 r, sectors_to_read, lbound, ubound, nlsn, offslsn;
	register int i, esc_flag = 0;
	u8 *p = (u8 *)buf;

	lbound = 0;
	ubound = (g_ISO_parts > 1) ? 0x80000 : 0xFFFFFFFF;
	offslsn = lsn;
	r = nlsn = 0;
	sectors_to_read = nsectors;

	for (i=0; i<g_ISO_parts; i++, lbound=ubound, ubound+=0x80000, offslsn-=0x80000) {

		if (lsn>=lbound && lsn<ubound){
			if ((lsn + nsectors) > (ubound-1)) {
				sectors_to_read = ubound - lsn;
				nsectors -= sectors_to_read;
				nlsn = ubound;
			}
			else
				esc_flag = 1;

#ifdef USB_DRIVER
			mass_stor_ReadCD(offslsn, sectors_to_read, &p[r], i);
#endif
#ifdef SMB_DRIVER
			smb_ReadCD(offslsn, sectors_to_read, &p[r], i);
#endif
			r += sectors_to_read << 11;
			sectors_to_read = nsectors;
			lsn = nlsn;
		}

		if (esc_flag)
			break;
	}

	return 1;
}
#endif

//-------------------------------------------------------------------------
int sceCdRead0(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
	register int r;

	r = QueryIntrContext();

	if ((u32)(buf) & 3) {
		WaitSema(cdvdman_searchfilesema);

		u32 nsectors, nbytes;
		u32 rpos = lsn;

		while (sectors > 0) {
			nsectors = sectors;
			if (nsectors > CDVDMAN_BUF_SECTORS)
				nsectors = CDVDMAN_BUF_SECTORS;

			DPRINTF("sceCdRead0 lsn=%d rpos=%d nsectors=%d buf=%08x\n", (int)lsn, (int)rpos, (int)nsectors, (int)buf);

			sceCdRead0(rpos, nsectors, cdvdman_buf, mode);	

			rpos += nsectors;
			sectors -= nsectors;
			nbytes = nsectors << 11;

			mips_memcpy(buf, cdvdman_buf, nbytes);

			buf = (void *)(buf + nbytes);
		}
		SignalSema(cdvdman_searchfilesema);
	}
	else {
		if (r) {
			DPRINTF("sceCdRead0 exiting (Intr context)...\n");
			return 0;
		}

		WaitSema(cdvdman_cdreadsema);

		DPRINTF("sceCdRead0 lsn=%d sectors=%d buf=%08x\n", (int)lsn, (int)sectors, (int)buf);

		cdvdman_stat.err = CDVD_ERR_NO;

		#ifdef HDD_DRIVER
		u32 offset = 0;
		while (sectors) {
			if (!((lsn >= cdvdman_partspecs.part_offset) && (lsn < (cdvdman_partspecs.part_offset + (cdvdman_partspecs.part_size >> 11)))))
				cdvdman_get_part_specs(lsn);

			if (cdvdman_stat.err != CDVD_ERR_NO)
				break;

			u32 nsectors = (cdvdman_partspecs.part_offset + (cdvdman_partspecs.part_size >> 11)) - lsn;
			if (sectors < nsectors)
				nsectors = sectors;

			u32 lba = cdvdman_partspecs.data_start + ((lsn - cdvdman_partspecs.part_offset) << 2);
			if (ata_device_dma_transfer(0, (void *)(buf + offset), lba, nsectors << 2, ATA_DIR_READ) != 0) {
				cdvdman_stat.err = CDVD_ERR_READ;
				break;
			}
			offset += nsectors << 11;
			sectors -= nsectors;
			lsn += nsectors;
			ReadPos += nsectors;
		}

		#else
		cdvdman_ReadSect(lsn, sectors, buf);
		#endif

		DPRINTF("sceCdRead0 ret=%d\n", r);

		SignalSema(cdvdman_cdreadsema);
	}

	ReadPos = 0;	/* Reset the buffer offset indicator. */

	return 1;
}

//-------------------------------------------------------------------------
int cdvdman_readMechaconVersion(char *mname, u32 *stat)
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
	}
	else {
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
int sceCdStPause(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStResume(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStSeekF(u32 lsn)
{	
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.Stlsn = lsn;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdReadDiskID(void *DiskID)
{
	int i;
	u8 *p = (u8 *)DiskID;

	for (i=0; i<5; i++) {
		if (p[i]!=0)
			break;
	}
	if (i == 5)
		*((u16 *)DiskID) = (u16)0xadde;
	else
		mips_memcpy(DiskID, &g_DiskID[1], 5);

	return 1;
}

//-------------------------------------------------------------------------
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start)
{
	if (g_gamesetting_disable_DVDDL) {
		*layer1_start = 0;
		*on_dual = 1;
	}
	else {
		#ifdef HDD_DRIVER
		*layer1_start = apaHeader.layer1_start;
		*on_dual = (apaHeader.layer1_start > 0) ? 1 : 0;
		#else
		*layer1_start = cdvdman_layer1start;
		*on_dual = (cdvdman_layer1start > 0) ? 1 : 0;
		#endif
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
		*GUID0 = lbuf[0] | 0x08004600;
		*GUID1 = *(u32 *)&lbuf[4];
	}
	else { // ModelID
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
int cdrom_dummy(void)
{
	return -EPERM;
}

//-------------------------------------------------------------- 
int cdrom_init(iop_device_t *dev)
{
	iop_sema_t smp;

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 1;
	smp.option = 0;

	cdrom_io_sema = CreateSema(&smp);

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 1;	
	smp.option = 0;

	cdvdman_searchfilesema = CreateSema(&smp);

	DPRINTF("cdrom_init\n");

	return 0;
}

//-------------------------------------------------------------- 
int cdrom_deinit(iop_device_t *dev)
{
	DeleteSema(cdrom_io_sema);

	DPRINTF("cdrom_deinit\n");

	return 0;
}

//-------------------------------------------------------------- 
FHANDLE *cdvdman_getfilefreeslot(void)
{
	register int i;
	FHANDLE *fh;

	for (i=0; i<MAX_FDHANDLES; i++) {
		fh = (FHANDLE *)&cdvdman_fdhandles[i];
		if (fh->f == NULL)
			return fh;
	}

	return 0;
}

//-------------------------------------------------------------- 
int cdrom_open(iop_file_t *f, char *filename, int mode)
{
	register int r = 0;
	FHANDLE *fh;
	cd_file_t cdfile;

	if (!filename)
		return -ENOENT;
 
	WaitSema(cdrom_io_sema);

	fs_init();

	DPRINTF("cdrom_open %s mode=%d\n", filename, mode);

	fh = cdvdman_getfilefreeslot();
	if (fh) {
		r = cdvdman_findfile(&cdfile, filename, f->unit);
		if (r) {
			f->privdata = fh;
			fh->f = f;
			if (!g_gamesetting_disable_DVDDL) {
				if (f->mode == 0)
					f->mode = r;
			}
			fh->filesize = cdfile.size;
			fh->lsn = cdfile.lsn;
			fh->position = 0;
			r = 0;

		}
		else
			r = -ENOENT;
	}
	else
		r = -EMFILE;

	DPRINTF("cdrom_open ret=%d lsn=%d size=%d\n", r, (int)fh->lsn, (int)fh->filesize);

	SignalSema(cdrom_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int cdrom_close(iop_file_t *f)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;

	WaitSema(cdrom_io_sema);

	DPRINTF("cdrom_close\n");

	if (fh)
		mips_memset(fh, 0, sizeof(FHANDLE));

	SignalSema(cdrom_io_sema);

	return 0;
}

//-------------------------------------------------------------- 
int cdrom_read(iop_file_t *f, void *buf, u32 size)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int rpos, sectorpos;
	register u32 nsectors, nbytes;

	WaitSema(cdrom_io_sema);

	DPRINTF("cdrom_read size=%d file_position=%d\n", (int)size, (int)fh->position);

	rpos = 0;

	if ((fh->position + size) > fh->filesize)
		size = fh->filesize - fh->position;

	while (size) {
		nbytes = CDVDMAN_FS_BUFSIZE;
		if (size < nbytes)
			nbytes = size;

		nsectors = nbytes >> 11;
		sectorpos = fh->position & 2047;

		if (sectorpos)
			nsectors++;

		if (nbytes & 2047)
			nsectors++;

		sceCdRead0(fh->lsn + ((fh->position & -2048) >> 11), nsectors, cdvdman_fs_buf, NULL);
		mips_memcpy(buf, &cdvdman_fs_buf[sectorpos], nbytes);

		rpos += nbytes;
		buf += nbytes;
		size -= nbytes;
		fh->position += nbytes;
	}

	DPRINTF("cdrom_read ret=%d\n", (int)rpos);
	SignalSema(cdrom_io_sema);

	return rpos;
}

//-------------------------------------------------------------- 
int cdrom_lseek(iop_file_t *f, u32 offset, int where)
{
	register int r;
	FHANDLE *fh = (FHANDLE *)f->privdata;

	WaitSema(cdrom_io_sema);

	DPRINTF("cdrom_lseek offset=%d where=%d\n", (int)offset, (int)where);

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
	DPRINTF("cdrom_lseek file offset=%d\n", (int)fh->position);
	SignalSema(cdrom_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int cdrom_getstat(iop_file_t *f, char *filename, iox_stat_t *stat)
{
	return sceCdLayerSearchFile((cdl_file_t *)&stat->attr, filename, f->unit) - 1;
}

//-------------------------------------------------------------- 
int cdrom_dopen(iop_file_t *f, char *dirname)
{
	return cdrom_open(f, dirname, 8);
}

//-------------------------------------------------------------- 
int cdrom_dread(iop_file_t *f, iox_dirent_t *dirent)
{
	register int r = 0;
	register u32 mode;
	FHANDLE *fh = (FHANDLE *)f->privdata;
	struct dirTocEntry *tocEntryPointer;

	WaitSema(cdrom_io_sema);

	DPRINTF("cdrom_dread fh->lsn=%d\n", (int)fh->lsn);

	sceCdRead0(fh->lsn, 1, cdvdman_fs_buf, NULL);

	do {
		r = 0;
		tocEntryPointer = (struct dirTocEntry *)&cdvdman_fs_buf[fh->position];
		if (tocEntryPointer->length == 0)
			break;

		fh->position += tocEntryPointer->length;
		r = 1;
	}
	while (tocEntryPointer->filenameLength == 1);

	mode = 0x2124;
	if (tocEntryPointer->fileProperties & 2)
		mode = 0x116d;

	dirent->stat.mode = mode;
	dirent->stat.size = tocEntryPointer->fileSize;
	strncpy(dirent->name, tocEntryPointer->filename, 256);

	DPRINTF("cdrom_dread r=%d mode=%04x name=%s\n", r, (int)mode, dirent->name);

	SignalSema(cdrom_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int cdrom_dclose(iop_file_t *f)
{
	return cdrom_close(f);
}

//-------------------------------------------------------------- 
int cdrom_ioctl(iop_file_t *f, u32 cmd, void *args)
{
	register int r = 0;

	WaitSema(cdrom_io_sema);

	if (cmd != 0x10000)	// Spin Ctrl op
		r = -EINVAL;

	SignalSema(cdrom_io_sema);

	return r;
}

//-------------------------------------------------------------- 
s64 cdrom_lseek64(iop_file_t *f, s64 pos, int where)
{ 
	DPRINTF("cdrom_lseek64 where=%d\n", (int)where);
	return (s64)cdrom_lseek(f, (u32)pos, where);
}

//-------------------------------------------------------------- 
int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, u32 arglen, void *buf, u32 buflen)
{
	register int r = 0;

	WaitSema(cdrom_io_sema);

	if (cmd == CIOCSTREAMPAUSE)
		sceCdStPause();
	else if (cmd == CIOCSTREAMRESUME)
		sceCdStResume();
	else if (cmd == CIOCSTREAMSTAT)
		r = sceCdStStat();
	else
		r = -EINVAL;

	SignalSema(cdrom_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen)
{
	register int r;

	if (!name)
		return -ENOENT;

	WaitSema(cdrom_io_sema);

	cmd -= CDIOC_CMDBASE;
	if (cmd > 133)
		return devctl_dummy(args, buf);

	devctl_fn = devctl_tab[cmd];
	r = devctl_fn(args, buf);

	SignalSema(cdrom_io_sema);

	return r;
}

//-------------------------------------------------------------------------
int devctl_dummy(void *args, void *buf)
{
	return -EIO;
}

//-------------------------------------------------------------------------
int devctl_dummy2(void *args, void *buf)
{
	return 0;
}

//-------------------------------------------------------------------------
int devctl_retonly(void *args, void *buf)
{
	return 1;
}

//-------------------------------------------------------------------------
int devctl_cdreadclock(void *args, void *buf)
{
	sceCdReadClock((cd_clock_t *)buf);
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdreadGUID(void *args, void *buf)
{
	sceCdReadGUID(buf);
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdreadDiskID(void *args, void *buf)
{
	sceCdReadDiskID(buf);
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdgetdisktype(void *args, void *buf)
{
	*(int *)buf = sceCdGetDiskType();
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdgeterror(void *args, void *buf)
{
	*(int *)buf = sceCdGetError();
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdtrayreq(void *args, void *buf)
{
	sceCdTrayReq(*(int *)args, (u32 *)args);
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdstatus(void *args, void *buf)
{
	*(int *)buf = sceCdStatus();
	return 0;	
}

//-------------------------------------------------------------------------
int devctl_cddiskready(void *args, void *buf)
{
	*(int *)buf = sceCdDiskReady(*(int *)args);
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdreadModelID(void *args, void *buf)
{	
	sceCdReadModelID(buf);
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdStinit(void *args, void *buf)
{
	u8 *p = (u8 *)buf;
	u32 bufmax  = *(u32 *)&p[0];
	u32 bankmax = *(u32 *)&p[4];
	u32 iopbuf  = *(u32 *)&p[8];

	sceCdStInit(bufmax, bankmax, (void *)iopbuf);
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdabort(void *args, void *buf)
{
	sceCdBreak();
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdstandby(void *args, void *buf)
{
	sceCdStandby();
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdstop(void *args, void *buf)
{
	sceCdStop();
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdpause(void *args, void *buf)
{
	sceCdPause();
	return 0;
}

//-------------------------------------------------------------------------
int devctl_cdgettoc(void *args, void *buf)
{
	sceCdGetToc(buf);
	return 0;
}

//-------------------------------------------------------------------------
int devctl_intref(void *args, void *buf)
{
	*(int *)buf = cdvdman_stat.intr_ef;
	return cdvdman_stat.intr_ef;
}

//-------------------------------------------------------------------------
void cdvdman_trimspaces(char* str)
{
	int i, len;
	char *p;

	len = strlen(str);
	if (len == 0)
		return;

	i = len - 1;

	for (i = len-1; i != -1; i--) {
		p = &str[i];
		if ((*p != 0x20) && (*p != 0x2e))
			break;
		*p = 0;
	}

	while (!strcmp(&str[len-3], ";;1")) { // Tenchu: Wrath oh Heaven fix
		str[len-2] = '1';
		str[len-1] = 0;
		len = strlen(str);		
	}

	while (!strcmp(&str[len-4], ";1;1")) { // SmackDown: shut your mouth fix
		str[len-2] = 0;
		len = strlen(str);
	}
}

//-------------------------------------------------------------------------
struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer)
{
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
		slash = strchr (p, '\\');

	len = (u32)slash - (u32)p;

	// if a slash was found
	if (slash != NULL) {

		if (len >= 256)
			return NULL;

		// copy the path into main 'dir' var
		strncpy(cdvdman_dirname, p, len);
		cdvdman_dirname[len] = 0;
	}
	else {
		if (strlen(p) >= 256)
			return NULL;
			
		strcpy(cdvdman_dirname, p);
	}

	while (tocLength > 0)	{
		sceCdRead0(tocLBA, 1, cdvdman_buf, NULL);
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
				strncpy(cdvdman_curdir, tocEntryPointer->filename, 256); // copy filename
				cdvdman_curdir[filename_len] = 0;

				DPRINTF("cdvdman_locatefile strcmp %s %s\n", cdvdman_dirname, cdvdman_curdir);

				r = strcmp(cdvdman_dirname, cdvdman_curdir);
				if ((!r) && (!slash)) { // we searched a file so it's found
					DPRINTF("cdvdman_locatefile found file! LBA=%d size=%d\n", (int)tocEntryPointer->fileLBA, (int)tocEntryPointer->fileSize);
					return tocEntryPointer;
				}
				else if ((!r) && (tocEntryPointer->fileProperties & 2)) { // we found it but it's a directory
					tocLBA = tocEntryPointer->fileLBA;
					tocLength = tocEntryPointer->fileSize;
					p = &slash[1];

					if (!g_gamesetting_disable_DVDDL) {
						int on_dual;
						u32 layer1_start;
						sceCdReadDvdDualInfo(&on_dual, &layer1_start);

						if (layer)
							tocLBA += layer1_start;
					}

					goto lbl_startlocate;
				}
				else {
					tocPos += (tocEntryPointer->length << 16) >> 16;
				}
			}
		}
		while (tocPos < 2016);
	}

	DPRINTF("cdvdman_locatefile file not found!!!\n");

	return NULL;
}

//-------------------------------------------------------------------------
int cdvdman_findfile(cd_file_t *pcdfile, const char *name, int layer)
{
	register int len;
	register u32 lsn;
	struct dirTocEntry *tocEntryPointer;

	if ((!pcdfile) || (!name))
		return 0;

	WaitSema(cdvdman_searchfilesema);

	DPRINTF("cdvdman_findfile %s layer%d\n", name, layer);

	strncpy(cdvdman_filepath, name, 256);

	cdvdman_trimspaces(cdvdman_filepath);
	
	DPRINTF("cdvdman_findfile cdvdman_filepath=%s\n", cdvdman_filepath);

	if (g_gamesetting_disable_DVDDL)
		layer = 0;

	if (layer < 2) {
		if (layer_info[layer].rootDirtocLBA == 0) {
			SignalSema(cdvdman_searchfilesema);
			return 0;
		}

		tocEntryPointer = cdvdman_locatefile(cdvdman_filepath, layer_info[layer].rootDirtocLBA, layer_info[layer].rootDirtocLength, layer);
		if (tocEntryPointer == NULL) {
			len = strlen(name);
			if (len < 256) {
				sprintf(cdvdman_filepath, "%s;1", name);
				cdvdman_trimspaces(cdvdman_filepath);
				tocEntryPointer = cdvdman_locatefile(cdvdman_filepath, layer_info[layer].rootDirtocLBA, layer_info[layer].rootDirtocLength, layer);
				if (tocEntryPointer == NULL) {
					SignalSema(cdvdman_searchfilesema);
					return 0;
				}
			}
			if (len == 0) {
				SignalSema(cdvdman_searchfilesema);
				return 0;
			}
		}

		lsn = tocEntryPointer->fileLBA;
		if (layer) {
			sceCdReadDvdDualInfo((int *)&pcdfile->lsn, &pcdfile->size);
			lsn += pcdfile->size;
		}

		pcdfile->lsn = lsn;
		if ((g_gamesetting_0_pss) && \
			((!strncmp(&cdvdman_filepath[strlen(cdvdman_filepath)-6], ".PSS", 4)) || \
			(!strncmp(&cdvdman_filepath[strlen(cdvdman_filepath)-6], ".pss", 4))))
			pcdfile->size = 0;
		else
			pcdfile->size = tocEntryPointer->fileSize;

		strcpy(pcdfile->name, strrchr(name, '\\')+1);
	}
	else {
		SignalSema(cdvdman_searchfilesema);
		return 0;
	}

	DPRINTF("cdvdman_findfile found %s\n", name);

	SignalSema(cdvdman_searchfilesema);

	return 1;
}

//-------------------------------------------------------------------------
int cdvdman_writeSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size)
{
	int i;
	u8 dummy;
	u8 *p;
	u8 rdbuf[64];

	WaitSema(cdvdman_cdreadsema);

	if (CDVDreg_SDATAIN & 0x80) {
		SignalSema(cdvdman_cdreadsema);
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

	while (CDVDreg_SDATAIN & 0x80) {;}

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

	SignalSema(cdvdman_cdreadsema);

	return 1;
}

//-------------------------------------------------------------- 
int cdvdman_sendSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size)
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
static u8 cb_args[8] __attribute__((aligned(16)));
int cdvdman_cb_event(int reason)
{
	iop_sys_clock_t sys_clock;	
	int oldstate;
	u8 *ptr;

	if (user_cb) {
		CpuSuspendIntr(&oldstate);
		//ptr = AllocSysMemory(ALLOC_FIRST, 8, NULL);
		ptr = (u8 *)cb_args;
		CpuResumeIntr(oldstate);

		if (ptr) {
			*((u32 *)&ptr[0]) = (u32)user_cb;
			*((u32 *)&ptr[4]) = reason;

			sys_clock.lo = 100;
			sys_clock.hi = 0;

			DPRINTF("cdvdman_cb_event reason: %d - setting cb alarm...\n", reason);

			if (QueryIntrContext())
				iSetAlarm(&sys_clock, event_alarm_cb, ptr);
			else
				SetAlarm(&sys_clock, event_alarm_cb, ptr);
		}
	}

	return 1;
}

//-------------------------------------------------------------------------
void (*cbfunc)(int reason);

unsigned int event_alarm_cb(void *args)
{
	register int reason;
	u8 *ptr = (u8 *)args;

	cbfunc = (void *)*((u32 *)&ptr[0]);
	reason = *((u32 *)&ptr[4]);

	//FreeSysMemory(args);

	cbfunc(reason);

	return 0;
}

//-------------------------------------------------------------------------
#ifndef ALT_READ_CORE
void cdvdman_cdread_Thread(void *args)
{
	while (1) {
		WaitSema(cdvdman_lockreadsema);

		while (QueryIntrContext())
			DelayThread(10000);

		sceCdRead0(cdvdman_stat.cdread_lba, cdvdman_stat.cdread_sectors, cdvdman_stat.cdread_buf, &cdvdman_stat.cdread_mode);

		sync_flag = 0;
		cdvdman_stat.status = CDVD_STAT_PAUSE;

		cdvdman_cb_event(SCECdFuncRead);
	}
}

//-------------------------------------------------------------------------
void cdvdman_startThreads(void)
{
	iop_thread_t thread_param;
	register int thid;

	cdvdman_stat.status = CDVD_STAT_PAUSE;
	cdvdman_stat.err = CDVD_ERR_NO;

	thread_param.thread = (void *)cdvdman_cdread_Thread;
	thread_param.stacksize = 0x2000;
	thread_param.priority = 0x0f;
	thread_param.attr = TH_C;
	thread_param.option = 0;

	thid = CreateThread(&thread_param);
	StartThread(thid, NULL);
}
#endif

//-------------------------------------------------------------------------
void cdvdman_create_semaphores(void)
{
	iop_sema_t smp;

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 0;
	smp.option = 0;

	cdvdman_cdreadsema = CreateSema(&smp);

#ifndef ALT_READ_CORE
	smp.initial = 0;
	smp.max = 1;
	smp.attr = 0;
	smp.option = 0;

	cdvdman_lockreadsema = CreateSema(&smp);
#endif
}

//-------------------------------------------------------------------------
void cdvdman_initdev(void)
{
	iop_event_t event;

	event.attr = 2;
	event.option = 0;
	event.bits = 0;

	cdvdman_stat.intr_ef = CreateEventFlag(&event);
	ClearEventFlag(cdvdman_stat.intr_ef, 0);

	DelDrv("cdrom");
	AddDrv((iop_device_t *)&cdrom_dev);
}

//-------------------------------------------------------------------------
#ifdef HDD_DRIVER
void cdvdman_get_part_specs(u32 lsn)
{
	register int i;
	cdvdman_partspecs_t *ps = (cdvdman_partspecs_t *)&apaHeader.part_specs[0];

	for (i = 0; i < apaHeader.num_partitions; i++) {
		if (lsn >= ps->part_offset) {
			if (lsn < (ps->part_offset + (ps->part_size >> 11))) {
				mips_memcpy(&cdvdman_partspecs, ps, sizeof(cdvdman_partspecs));
				break;
			}
		}
		ps++;
	}

	if (i >= apaHeader.num_partitions)
		cdvdman_stat.err = CDVD_ERR_READ;
}
#endif

//-------------------------------------------------------------------------
int _start(int argc, char **argv)
{
	// register exports
	RegisterLibraryEntries(&_exp_cdvdman);
	RegisterLibraryEntries(&_exp_cdvdstm);
#ifdef SMB_DRIVER
	RegisterLibraryEntries(&_exp_dev9);
	dev9_init();
#endif
#ifdef HDD_DRIVER
#ifdef HD_PRO
#ifdef __IOPCORE_DEBUG
	RegisterLibraryEntries(&_exp_dev9);
#endif
#else
	RegisterLibraryEntries(&_exp_dev9);
#endif
	RegisterLibraryEntries(&_exp_atad);

#ifdef HD_PRO
#ifdef __IOPCORE_DEBUG
	dev9_init();
#endif
#else
	dev9_init();
#endif
	atad_start();

	atad_inited = 1;
#endif
#ifdef USB_DRIVER
#ifdef __USE_DEV9
	RegisterLibraryEntries(&_exp_dev9);
	dev9_init();
#endif
#endif
	RegisterLibraryEntries(&_exp_smsutils);
#ifdef VMC_DRIVER
	RegisterLibraryEntries(&_exp_oplutils);
#endif
	// create SCMD/searchfile semaphores
	cdvdman_create_semaphores();

	// start cdvdman threads
#ifdef ALT_READ_CORE
	cdvdman_startNCmdthread();
#else
	cdvdman_startThreads();
#endif
	// register cdrom device driver
	cdvdman_initdev();

#ifndef SMB_DRIVER
	g_tag[0] = 0; // just to shut off warning
#endif
#ifdef HDD_DRIVER
	if (g_ISO_media != 0x69)
		lba_48bit = g_ISO_media;
	g_ISO_parts = 0x69; // just to shut off warning
#endif

	// hook MODLOAD's exports
	hookMODLOAD();

	// init disk type stuff
	cdvdman_initDiskType();

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
#ifdef SMB_DRIVER
	smb_Disconnect();
#endif

	return 0;
}
