/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <sifcmd.h>
#include <sifman.h>
#include <ioman.h>
#include <thsemap.h>
#include <thmsgbx.h>
#include <errno.h>
#include <io_common.h>
#include "ioman_add.h"

//#define NETLOG_DEBUG

#define MODNAME "cdvdman"
IRX_ID(MODNAME, 1, 1);

struct irx_export_table _exp_cdvdman;
struct irx_export_table _exp_cdvdstm;

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
#define CDVD_TRAY_OPEN		0			// Tray Open
#define CDVD_TRAY_CLOSE		1			// Tray Close
#define CDVD_TRAY_CHECK		2			// Tray Check

// sceCdDiskReady() values
#define CDVD_READY_READY	0x02
#define CDVD_READY_NOTREADY	0x06

// cdGetError() return values
#define CDVD_ERR_FAIL		-1			// error in cdGetError()
#define CDVD_ERR_NO			0x00		// no error occurred
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

#define NCMD_INIT 			0x00
#define NCMD_READ 			0x01
#define NCMD_READCDDA 		0x02
#define NCMD_GETTOC 		0x03
#define NCMD_SEEK 			0x04
#define NCMD_STANDBY 		0x05
#define NCMD_STOP 			0x06
#define NCMD_PAUSE 			0x07

// CDVD Func 
#define SCECdFuncRead         1
#define SCECdFuncReadCDDA     2
#define SCECdFuncGetToc       3
#define SCECdFuncSeek         4
#define SCECdFuncStandby      5
#define SCECdFuncStop         6
#define SCECdFuncPause        7
#define SCECdFuncBreak        8


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
	u32 lsn; 			
	u32 size; 			
	char name[16]; 		
	u8 date[8]; 		
} cd_file_t;

typedef struct {
	u32 lsn; 			
	u32 size; 			
	char name[16]; 		
	u8 date[8];
	u32 flag; 		
} cdl_file_t;

struct TocEntry {
	u32		fileLBA;
	u32		fileSize;
	u8	    dateStamp[6];
	u8 		padding0[2];
	u8		fileProperties;
	u8		padding1[3];
	char	filename[128+1];
	u8		padding2[3];
} __attribute__((packed));

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

typedef struct {        // size =0x124
	cd_file_t cdfile; 	// 0x000
	char name[256]; 	// 0x020
	void *dest;			// 0x120
} SearchFilePkt_t;

typedef struct {		// size =0x128
	cdl_file_t cdfile; 	// 0x000
	char name[256]; 	// 0x024
	void *dest;			// 0x124
} SearchFilePktl_t;

typedef struct {
	u32 lsn;		// sector location to start reading from
	u32 sectors;	// number of sectors to read
	void *buf;		// buffer address to read to ( bit0: 0=EE, 1=IOP )
					// (EE addresses must be on 64byte alignment)
} cdvdman_chain_t;

typedef struct {
	u32 lsn;
	u32 sectors;
	void *buf;
	cd_read_mode_t mode;
	void *eeaddr1;
	void *eeaddr2;
} RpcCdvd_t;

typedef struct {
	u32 lsn;
	u32 sectors;
	void *buf;
	int cmd;
	cd_read_mode_t mode;
	u32 pad;
} RpcCdvdStream_t;

typedef struct {
	u32 bufmax;
	u32 bankmax;
	void *buf;
	u32 pad;	
} RpcCdvdStInit_t;

typedef struct {
	int cderror;
	int cdstat;
	int cdNCmd;
	int cddiskready;
	int cdcurlsn;
	int traychk;
	void *user_cb0;
	void *user_cb1;
	void *poff_cb;
	void *poffaddr;
	u8 pad[4];
	int cdmmode;
	void *Stiopbuf;
	int Ststat;
	int Stbufmax;
	int Stlsn;
	int Stsectmax;	
	int cdvdman_intr_ef;
} cdvdman_status_t;

typedef struct {
	int ncmd;
	u8 nbuf[28];
} cdvdman_NCmd_t;

typedef struct NCmdMbx {
	iop_message_t iopmsg;
	cdvdman_NCmd_t NCmdmsg;
	struct NCmdMbx *next;
} NCmdMbx_t;


// exported functions prototypes
int sceCdInit(int init_mode);												// #4
int sceCdStandby(void);														// #5
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode); 		// #6
int sceCdSeek(u32 lsn);														// #7
int sceCdGetError(void); 													// #8
int sceCdGetToc(void *toc); 												// #9
int sceCdSearchFile(cd_file_t *fp, const char *name); 						// #10
int sceCdSync(int mode); 													// #11
int sceCdGetDiskType(void);													// #12
int sceCdDiskReady(int mode); 												// #13
int sceCdTrayReq(int mode, u32 *traycnt); 									// #14
int sceCdStop(void); 														// #15
int sceCdPosToInt(cd_location_t *p); 										// #16
cd_location_t *sceCdIntToPos(int i, cd_location_t *p);						// #17
int sceCdRI(char *buf, int *stat); 											// #22
int sceCdReadClock(cd_clock_t *rtc); 										// #24
int sceCdStatus(void); 														// #28
int sceCdApplySCmd(u8 cmd, void *in, u32 in_size, void *out); 				// #29
int *sceCdCallback(void *func); 											// #37
int sceCdPause(void);														// #38
int sceCdBreak(void);														// #39
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode);	// #40
int sceCdGetReadPos(void); 													// #44
void *sceCdGetFsvRbuf(void);												// #47
int sceCdSC(int code, int *param);											// #50
int sceCdRC(cd_clock_t *rtc); 												// #51
int sceCdApplyNCmd(u8 cmd, void *in, u32 in_size);							// #54
int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr); 				// #56
int sceCdStRead(u32 sectors, void *buf, u32 mode, u32 *err); 				// #57
int sceCdStSeek(u32 lsn);													// #58			
int sceCdStStart(u32 lsn, cd_read_mode_t *mode);							// #59
int sceCdStStat(void); 														// #60
int sceCdStStop(void);														// #61	
int sceCdRead0(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode, int csec, void *cb);// #62
int sceCdRM(char *m, u32 *stat);											// #64
int sceCdStPause(void);														// #67
int sceCdStResume(void); 													// #68
int sceCdMmode(int mode); 													// #75
int sceCdStSeekF(u32 lsn); 													// #77
int *sceCdPOffCallback(void *func, void *addr); 							// #78
int sceCdReadDiskID(u32 *id);												// #79
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start); 					// #83
int sceCdLayerSearchFile(cdl_file_t *fp, const char *name, int layer);		// #84
int sceCdApplySCmd2(u8 cmd, void *in, u32 in_size, void *out);				// #112

// internal functions prototypes
void cdvdman_startrpcthreads(void);
void cdvdfsv_rpc0_th(void *args);
void cdvdfsv_rpc1_th(void *args);
void cdvdfsv_rpc2_th(void *args);
void *cbrpc_cdinit(u32 fno, void *buf, int size);
void *cbrpc_cdvdScmds(u32 fno, void *buf, int size);
void *(*rpcSCmd_fn)(u32 fno, void *buf, int size);
void *rpcSCmd_cdreadclock(u32 fno, void *buf, int size);
void *rpcSCmd_cdgetdisktype(u32 fno, void *buf, int size);
void *rpcSCmd_cdgeterror(u32 fno, void *buf, int size);
void *rpcSCmd_cdtrayreq(u32 fno, void *buf, int size);
void *rpcSCmd_cdapplySCmd(u32 fno, void *buf, int size);
void *rpcSCmd_cdstatus(u32 fno, void *buf, int size);
void *rpcSCmd_cdabort(u32 fno, void *buf, int size);
void *rpcSCmd_cdmmode(u32 fno, void *buf, int size);
void *rpcSCmd_dummy(u32 fno, void *buf, int size);
void *rpcSCmd_cdreaddvddualinfo(u32 fno, void *buf, int size);
void *cbrpc_cdvdNcmds(u32 fno, void *buf, int size);
void *(*rpcNCmd_fn)(u32 fno, void *buf, int size);
void *rpcNCmd_cdreadee(u32 fno, void *buf, int size);
void *rpcNCmd_cdgettoc(u32 fno, void *buf, int size);
void *rpcNCmd_cdseek(u32 fno, void *buf, int size);
void *rpcNCmd_cdstandby(u32 fno, void *buf, int size);
void *rpcNCmd_cdstop(u32 fno, void *buf, int size);
void *rpcNCmd_cdpause(u32 fno, void *buf, int size);
void *rpcNCmd_cdstream(u32 fno, void *buf, int size);
void *rpcNCmd_cdapplyNCmd(u32 fno, void *buf, int size);
void *rpcNCmd_iopmread(u32 fno, void *buf, int size);
void *rpcNCmd_cddiskready(u32 fno, void *buf, int size);
void *rpcNCmd_cdreadchain(u32 fno, void *buf, int size);
void *rpcNCmd_dummy(u32 fno, void *buf, int size);
void *cbrpc_S596(u32 fno, void *buf, int size);
void *cbrpc_fscall(u32 fno, void *buf, int size);
void *cbrpc_cddiskready(u32 fno, void *buf, int size);
void *cbrpc_cddiskready2(u32 fno, void *buf, int size);
int cdvd_readee(void *buf, int size);
int cdvdman_readee(u32 lsn, u32 sectors, void *buf, u8 datapattern, void *addr1, void *addr2);
int cdvd_iopmread(void *buf, int size);
int cdvdman_iopmemread(u32 lsn, u32 sectors, void *buf, void *addr);
int cdvd_readchain(void *buf, int size);
int cdvd_Stsubcmdcall(void *buf, int size);
int (*cdvd_Stsubcall_fn)(void *buf);
int cdvdSt_dummy(void *buf);
int cdvdSt_start(void *buf);
int cdvdSt_read(void *buf);
int cdvdSt_stop(void *buf);
int cdvdSt_seek(void *buf);
int cdvdSt_init(void *buf);
int cdvdSt_stat(void *buf);
int cdvdSt_pause(void *buf);
int cdvdSt_resume(void *buf);
int cdvdSt_seekF(void *buf);
void *sysmemAlloc(int size);
void sysmemFree(void *ptr);
void sysmemSendEE(void *buf, void *EE_addr, int size);
int cdvdman_createsema(int initial, int max);
int cdvdman_createthread(void *thread, u32 priority, u32 stacksize);
NCmdMbx_t *cdvdman_setNCmdMbx(void);
void cdvdman_getNCmdMbx(NCmdMbx_t *mbxbuf);
int cdvdman_createMbx(void);
void cdvdman_sendNCmdMbx(int mbxid, cdvdman_NCmd_t *NCmdmsg, int size);
cdvdman_NCmd_t *cdvdman_receiveNCmdMbx(int mbxid);

int cdrom_dummy(void);
s64 cdrom_dummy64(void);
int cdrom_init(iop_device_t *dev);
int cdrom_deinit(iop_device_t *dev);
int cdrom_open(iop_file_t *f, char *filename, int mode);
int cdrom_close(iop_file_t *f);
int cdrom_read(iop_file_t *f, void *buf, u32 size);
int cdrom_lseek(iop_file_t *f, u32 pos, int where);
int cdrom_getstat(iop_file_t *f, char *filename, fio_stat_t *stat);
int cdrom_dopen(iop_file_t *f, char *dirname);
int cdrom_dread(iop_file_t *f, fio_dirent_t *dirent);
int cdrom_dclose(iop_file_t *f);
int	cdrom_ioctl(int fd, int cmd, void *args);
int cdrom_devctl(const char *name, int cmd, void *args, int arglen, void *buf, int buflen);
int cdrom_ioctl2(int fd, int cmd, void *args, int arglen, void *buf, int buflen);

void cdvdman_initdev(void);
void cdvdman_cdinit(void);
int cdvdman_readDisc(u32 lsn, u32 sectors, void *buf);
int cdvdman_readcd(u32 lsn, u32 sectors, void *buf);
int (*cdSync_fn)(void);
int cdSync_blk(void);
int cdSync_noblk(void);
int cdSync_dummy(void);
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
void NCmd_cdGetToc(void *ndata);
void NCmd_cdReadCDDA(void *ndata);
void NCmd_cdSeek(void *ndata);
void NCmd_cdStandby(void *ndata);
void NCmd_cdStop(void *ndata);
void NCmd_cdPause(void *ndata);
void (*cbfunc)(int reason);
void cdvdman_handleUserCallback(int reason);
void cdvdman_callUserCBfunc(void *cb, int reason);
unsigned int alarm_cb(void *cb);
int cdvdman_findfile(const char *filepath, cd_file_t *cdfile);
void cdvdman_createSCmdsema(void);
int cdvdman_writeSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size);
int cdvdman_sendSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size);


// !!! isofs exports functions pointers !!!
iop_device_t *(*isofs_GetDevice)(void); 				 			 // Export #4
int (*isofs_ReadSect)(u32 lsn, u32 nsectors, void *buf); 			 // Export #5
int (*isofs_FindFile)(const char *fname, struct TocEntry *tocEntry); // Export #6
int (*isofs_GetDiscType)(void); 									 // Export #7

iop_device_t *isofs_dev;


#ifdef NETLOG_DEBUG
// !!! netlog exports functions pointers !!!
int (*pNetlogSend)(const char *format, ...);
#endif

// rpcSCmd funcs array
void *rpcSCmd_tab[40] = {
    (void *)rpcSCmd_dummy,
    (void *)rpcSCmd_cdreadclock,
	(void *)rpcSCmd_dummy, // cdwriteclock
	(void *)rpcSCmd_cdgetdisktype,
	(void *)rpcSCmd_cdgeterror,
	(void *)rpcSCmd_cdtrayreq,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_cdapplySCmd,
	(void *)rpcSCmd_cdstatus,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_cdabort,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy, // CancelPowerOff
	(void *)rpcSCmd_dummy, // BlueLedCtrl
	(void *)rpcSCmd_dummy, // PowerOff
	(void *)rpcSCmd_cdmmode,
	(void *)rpcSCmd_dummy, // SetThreadPriority
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_cdreaddvddualinfo
};

// rpcNCmd funcs array
void *rpcNCmd_tab[16] = {
    (void *)rpcNCmd_dummy,
    (void *)rpcNCmd_cdreadee, // Cd Read
    (void *)rpcNCmd_cdreadee, // Cdda Read
    (void *)rpcNCmd_cdreadee, // Dvd Read
    (void *)rpcNCmd_cdgettoc,
    (void *)rpcNCmd_cdseek,
    (void *)rpcNCmd_cdstandby,
    (void *)rpcNCmd_cdstop,
    (void *)rpcNCmd_cdpause,
    (void *)rpcNCmd_cdstream,
    (void *)rpcNCmd_dummy,	  // CDDA Stream
    (void *)rpcNCmd_dummy,    
    (void *)rpcNCmd_cdapplyNCmd,
    (void *)rpcNCmd_iopmread,
    (void *)rpcNCmd_cddiskready,
    (void *)rpcNCmd_cdreadchain
};

// cdvdStsubcall funcs array
void *cdvdStsubcall_tab[10] = {
    (void *)cdvdSt_dummy,
    (void *)cdvdSt_start,
    (void *)cdvdSt_read,
    (void *)cdvdSt_stop,
    (void *)cdvdSt_seek,
    (void *)cdvdSt_init,
    (void *)cdvdSt_stat,
    (void *)cdvdSt_pause,
    (void *)cdvdSt_resume,
    (void *)cdvdSt_seekF
};

// NCmd funcs array
void *NCmd_tab[8] = {
    (void *)NCmd_cdInit,	
    (void *)NCmd_cdRead,
    (void *)NCmd_cdReadCDDA,    
    (void *)NCmd_cdGetToc,    
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


int cdvdman_SCmdsema;

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
	(void*)cdrom_dummy64,
	(void*)cdrom_devctl,
	(void*)cdrom_dummy,
	(void*)cdrom_dummy,
	(void*)cdrom_ioctl2	
};

// driver descriptor
static iop_ext_device_t cdrom_dev = {
	"cdrom", 
	IOP_DT_FS, // | 0x10000000, // Fails to load modules correctly with EXT FS
	1,
	"CD-ROM ",
	(struct _iop_ext_device_ops *)&cdrom_ops
};


typedef struct {
	u32 rpc_S;
	void *cbrpc_fnc;
	u32 stacksize;
	u32 priority;
	u32 rpc_bufsize;
} rpcthread_param_t;

static rpcthread_param_t rpc_thp[3] = {
	{
		0x80000596,
		(void *)cbrpc_S596,
		0x2000,
		0x14,
		16
	}, 
	{
		0x8000059c,
		(void *)cbrpc_cddiskready2,
		0x2000,
		0x14,
		16
	}, 
	{
		0,
		NULL,
		0,
		0,
		0
	}	 
};


static struct {		
	int rpc_func_ret;
	int cdvdfsv_ver;
	int cdvdman_ver;
	int debug_mode;
} cdvdinit_res __attribute__((aligned(64)));

static struct {
	int result;
	cd_clock_t rtc;
} cdvdreadclock_res __attribute__((aligned(64)));

static struct {
	u8 out[16];
} cdvdapplySCmd_res __attribute__((aligned(64)));

int cdvdSCmd_res;

static struct {
	int result;
	u32 param1;
	u32 param2;	
} cdvdSCmd2_res __attribute__((aligned(64)));

typedef struct { // size = 144
	u32 b1len;
	u32 b2len;
	void *pdst1;
	void *pdst2;
	u8 buf1[64];
	u8 buf2[64];
} cdvdman_readee_t __attribute__((aligned(64)));

typedef struct {
	u16 cmd;
	u16 in_size;
	void *in;
} cdvdman_applySCmd_t __attribute__((aligned(64)));

typedef struct {
	u8 cmd;
	u8 pad;
	u16 in_size;
	void *in;
} cdvdman_applyNCmd_t __attribute__((aligned(64)));

int cdvdNCmd_res;
int cdvdsearchfile_res;
int cdvddiskready_res;
int cdvddiskready2_res;

int cdvdman_mbxid;
int cdvdman_NCmdsema;
int cdvdman_userCBsema;
int cdvdman_NCmdlocksema;
int cdvdman_NCmdsemacount;
int cdvdman_userCBreason;
int cdvdman_readcdsema;
int cdvdman_searchfilesema;
int cdvdman_inited;
int cdvdman_cdNCmdBreak;

cdvdman_readee_t cdvdman_eereadx;

static int cdvdman_diskready = CDVD_READY_READY;

u32 cbrpcS596_oldfno;
u32 cbrpcS596_fno;

void *cdvdman_pMbxnext;
void *cdvdman_pMbxcur;
void *cdvdman_pMbxbuf;

u8 cdvdman_iopmread_buf[16] __attribute__ ((aligned(64)));
u8 cdvdman_fsvbuf[8288] __attribute__ ((aligned(64)));

cdvdman_status_t cdvdman_stat;

cdvdman_NCmd_t cdvdman_NCmd;

char cdvdman_filepath[1025];

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{	
	// register cdvdman exports		
	RegisterLibraryEntries(&_exp_cdvdman);
	
	// register dummy cdvdstm exports, so that
	// further load of CDVDSTM will fail	
	RegisterLibraryEntries(&_exp_cdvdstm);
	
	// Create SCMD semaphore
	cdvdman_createSCmdsema();
	
	// Start NCMD server thread
	cdvdman_startNCmdthread();
	
	// Start RPC servers threads
	cdvdman_startrpcthreads();
	
	// Init "cdrom" device
	cdvdman_initdev();
			
	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------- 
void cdvdman_startrpcthreads(void)
{
	// Create and starts all needed rpc server threads
	int thid;
	rpcthread_param_t *th_p;
	
	thid = cdvdman_createthread((void *)cdvdfsv_rpc1_th, 0x51, 0x2000);
	StartThread(thid, 0);
	
	thid = cdvdman_createthread((void *)cdvdfsv_rpc2_th, 0x51, 0x2000);
	StartThread(thid, 0);

	th_p = (rpcthread_param_t *)rpc_thp;
	
	while (th_p->rpc_S) {
		thid = cdvdman_createthread((void *)cdvdfsv_rpc0_th, th_p->priority, th_p->stacksize);
		StartThread(thid, th_p);
		th_p++;
	}
}

//-------------------------------------------------------------- 
void cdvdfsv_rpc0_th(void *args)
{
	// To Start extra rpc server (diskready2, etc...) 
	rpcthread_param_t *th_p;
	SifRpcDataQueue_t *qdS;
	SifRpcServerData_t *sdS;
	void *rpc_buf;
	
	th_p = (rpcthread_param_t *)args;
	
	qdS = (SifRpcDataQueue_t *)sysmemAlloc(sizeof(SifRpcDataQueue_t));
	sdS = (SifRpcServerData_t *)sysmemAlloc(sizeof(SifRpcServerData_t));
	rpc_buf = sysmemAlloc(th_p->rpc_bufsize);
		
	memset(qdS, 0, sizeof(SifRpcDataQueue_t));
	memset(sdS, 0, sizeof(SifRpcServerData_t));
	
	sceSifSetRpcQueue(qdS, GetThreadId());	
	sceSifRegisterRpc(sdS, th_p->rpc_S, (void *)th_p->cbrpc_fnc, rpc_buf, NULL, NULL, qdS);
	sceSifRpcLoop(qdS);
}

//-------------------------------------------------------------- 
void cdvdfsv_rpc1_th(void *args)
{
	// Starts cd Init rpc Server
	// Starts cd Disk ready rpc server
	// Starts SCMD rpc server;
	SifRpcDataQueue_t qdS;
	SifRpcServerData_t *sdS1, *sdS2, *sdS3;
	void *rpc_buf1, *rpc_buf2, *rpc_buf3;

	memset(&qdS, 0, sizeof(SifRpcDataQueue_t));
	
	sceSifSetRpcQueue(&qdS, GetThreadId());	

	sdS1 = (SifRpcServerData_t *)sysmemAlloc(sizeof(SifRpcServerData_t));	
	memset(sdS1, 0, sizeof(SifRpcServerData_t));
	rpc_buf1 = sysmemAlloc(16);	
	sceSifRegisterRpc(sdS1, 0x8000059a, (void *)cbrpc_cddiskready, rpc_buf1, NULL, NULL, &qdS);

	sdS2 = (SifRpcServerData_t *)sysmemAlloc(sizeof(SifRpcServerData_t));			
	memset(sdS2, 0, sizeof(SifRpcServerData_t));
	rpc_buf2 = sysmemAlloc(16);	
	sceSifRegisterRpc(sdS2, 0x80000592, (void *)cbrpc_cdinit, rpc_buf2, NULL, NULL, &qdS);
	
	sdS3 = (SifRpcServerData_t *)sysmemAlloc(sizeof(SifRpcServerData_t));			
	memset(sdS3, 0, sizeof(SifRpcServerData_t));
	rpc_buf3 = sysmemAlloc(16);	
	sceSifRegisterRpc(sdS3, 0x80000593, (void *)cbrpc_cdvdScmds, rpc_buf3, NULL, NULL, &qdS);
	
	sceSifRpcLoop(&qdS);			
}

//-------------------------------------------------------------- 
void cdvdfsv_rpc2_th(void *args)
{
	// Starts NCMD rpc server
	// Starts Search file rpc server 
	SifRpcDataQueue_t qdS;
	SifRpcServerData_t *sdS1, *sdS2;
	void *rpc_buf1, *rpc_buf2;

	memset(&qdS, 0, sizeof(SifRpcDataQueue_t));
	
	sceSifSetRpcQueue(&qdS, GetThreadId());	

	sdS1 = (SifRpcServerData_t *)sysmemAlloc(sizeof(SifRpcServerData_t));	
	memset(sdS1, 0, sizeof(SifRpcServerData_t));
	rpc_buf1 = sysmemAlloc(1024);	
	sceSifRegisterRpc(sdS1, 0x80000595, (void *)cbrpc_cdvdNcmds, rpc_buf1, NULL, NULL, &qdS);

	sdS2 = (SifRpcServerData_t *)sysmemAlloc(sizeof(SifRpcServerData_t));			
	memset(sdS2, 0, sizeof(SifRpcServerData_t));
	rpc_buf2 = sysmemAlloc(512);	
	sceSifRegisterRpc(sdS2, 0x80000597, (void *)cbrpc_fscall, rpc_buf2, NULL, NULL, &qdS);

	sceSifRpcLoop(&qdS);				
}

//-------------------------------------------------------------- 
void *cbrpc_cdinit(u32 fno, void *buf, int size)
{	// CD Init RPC callback

	if (fno)
		return (void *)NULL;
		
	cdvdinit_res.rpc_func_ret = sceCdInit(*(int *)buf);
	
	cdvdinit_res.cdvdfsv_ver = 0x201;
	cdvdinit_res.cdvdman_ver = 0x201;
	cdvdinit_res.debug_mode = 0xff;
	
	return (void *)&cdvdinit_res;
}

//-------------------------------------------------------------- 
void *cbrpc_cdvdScmds(u32 fno, void *buf, int size)
{	// CD SCMD RPC callback

	if ((u32)(fno >= 40))
		return rpcSCmd_dummy(fno, buf, size);
	
	// Call SCMD RPC sub function		
	rpcSCmd_fn = (void *)rpcSCmd_tab[fno];
	
	return rpcSCmd_fn(fno, buf, size);	
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdreadclock(u32 fno, void *buf, int size)
{	// CD Read Clock RPC SCMD
	
	cdvdreadclock_res.result = sceCdReadClock(&cdvdreadclock_res.rtc);
	return (void *)&cdvdreadclock_res;
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdgetdisktype(u32 fno, void *buf, int size)
{	// CD Get Disc Type RPC SCMD

	cdvdSCmd_res = sceCdGetDiskType();
	return (void *)&cdvdSCmd_res;
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdgeterror(u32 fno, void *buf, int size)
{	// CD Get Error RPC SCMD
	
	cdvdSCmd_res = sceCdGetError();
	return (void *)&cdvdSCmd_res;	
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdtrayreq(u32 fno, void *buf, int size)
{	// CD Tray Req RPC SCMD
	
	cdvdSCmd2_res.result = sceCdTrayReq(*((int *)buf), &cdvdSCmd2_res.param1);
	return (void *)&cdvdSCmd2_res;
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdapplySCmd(u32 fno, void *buf, int size)
{	// CD ApplySCMD RPC SCMD

	cdvdman_applySCmd_t *SCmd = (cdvdman_applySCmd_t *)buf;
	
	sceCdApplySCmd(SCmd->cmd, SCmd->in, SCmd->in_size, &cdvdapplySCmd_res.out);
	return (void *)&cdvdapplySCmd_res;		
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdstatus(u32 fno, void *buf, int size)
{	// CD Status RPC SCMD

	cdvdSCmd_res = sceCdStatus();
	return (void *)&cdvdSCmd_res;		
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdabort(u32 fno, void *buf, int size)
{	// CD Abort RPC SCMD

	cdvdSCmd_res = sceCdBreak();
	return (void *)&cdvdSCmd_res;			
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdmmode(u32 fno, void *buf, int size)
{	// CD Media Mode RPC SCMD

	cdvdSCmd_res = sceCdMmode(*(int *)buf);
	return (void *)&cdvdSCmd_res;				
}

//-------------------------------------------------------------- 
void *rpcSCmd_dummy(u32 fno, void *buf, int size)
{
	return (void *)NULL;
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdreaddvddualinfo(u32 fno, void *buf, int size)
{	// CD Read Dvd DualLayer Info RPC SCMD

	cdvdSCmd2_res.result = sceCdReadDvdDualInfo((int *)&cdvdSCmd2_res.param1, &cdvdSCmd2_res.param2);
	return (void *)&cdvdSCmd2_res;	
}

//-------------------------------------------------------------- 
void *cbrpc_cdvdNcmds(u32 fno, void *buf, int size)
{	// CD NCMD RPC callback

	if ((u32)(fno >= 16))
		return rpcNCmd_dummy(fno, buf, size);

	// Call NCMD RPC sub function						
	rpcNCmd_fn = (void *)rpcNCmd_tab[fno];
	
	return rpcNCmd_fn(fno, buf, size);		
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdreadee(u32 fno, void *buf, int size)
{	// CD Read RPC NCMD

	cdvdNCmd_res = cdvd_readee(buf, size);
	return (void *)&cdvdNCmd_res;		
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdgettoc(u32 fno, void *buf, int size)
{	// CD Get TOC RPC NCMD
	
	cdvdNCmd_res = sceCdGetToc(buf);
	return (void *)&cdvdNCmd_res;			
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdseek(u32 fno, void *buf, int size)
{	// CD Seek RPC NCMD
	
	cdvdNCmd_res = sceCdSeek(*(u32 *)buf);
	return (void *)&cdvdNCmd_res;				
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdstandby(u32 fno, void *buf, int size)
{	// CD Standby RPC NCMD

	cdvdNCmd_res = sceCdStandby();
	return (void *)&cdvdNCmd_res;			
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdstop(u32 fno, void *buf, int size)
{	// CD Stop RPC NCMD
	
	cdvdNCmd_res = sceCdStop();
	return (void *)&cdvdNCmd_res;				
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdpause(u32 fno, void *buf, int size)
{	// CD Pause RPC NCMD
	
	cdvdNCmd_res = sceCdPause();
	return (void *)&cdvdNCmd_res;				
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdstream(u32 fno, void *buf, int size)
{	// CD Stream RPC NCMD
	
	cdvdNCmd_res = cdvd_Stsubcmdcall(buf, size);
	return (void *)&cdvdNCmd_res;	
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdapplyNCmd(u32 fno, void *buf, int size)
{	// CD ApplyNCMD RPC NCMD

	cdvdman_applyNCmd_t *NCmd = (cdvdman_applyNCmd_t *)buf;
	
	cdvdNCmd_res = sceCdApplyNCmd(NCmd->cmd, NCmd->in, NCmd->in_size);
	sceCdSync(0);
	
	return (void *)&cdvdNCmd_res;	
}

//-------------------------------------------------------------- 
void *rpcNCmd_iopmread(u32 fno, void *buf, int size)
{	// CD IOP Mem Read RPC NCMD

	cdvdNCmd_res = cdvd_iopmread(buf, size);
	return (void *)&cdvdNCmd_res;			
}

//-------------------------------------------------------------- 
void *rpcNCmd_cddiskready(u32 fno, void *buf, int size)
{	// CD Disk Ready RPC NCMD

	if (cdvdman_diskready == CDVD_READY_READY) 
		cdvdNCmd_res = sceCdDiskReady(1);
	else 
		cdvdNCmd_res = cdvdman_diskready;
	
	cdvdNCmd_res = CDVD_READY_READY;
	return (void *)&cdvdNCmd_res;				
}

//-------------------------------------------------------------- 
void *rpcNCmd_cdreadchain(u32 fno, void *buf, int size)
{	// CD ReadChain RPC NCMD
	
	cdvdNCmd_res = cdvd_readchain(buf, size);
	return (void *)&cdvdNCmd_res;				
}

//-------------------------------------------------------------- 
void *rpcNCmd_dummy(u32 fno, void *buf, int size)
{
	return (void *)NULL;
}

//-------------------------------------------------------------- 
void *cbrpc_S596(u32 fno, void *buf, int size)
{ 	
	if (fno != 1)
		return (void *)NULL;
	
	if (*(u32 *)buf == fno)
		return (void *)&cbrpcS596_oldfno;
		
	cbrpcS596_oldfno = cbrpcS596_fno;

	cbrpcS596_fno = fno;
	
	return (void *)&cbrpcS596_oldfno;					
}

//-------------------------------------------------------------- 
void *cbrpc_fscall(u32 fno, void *buf, int size)
{	// CD Search File RPC callback
	
	void *ee_addr;
	char *p;
	SearchFilePkt_t *pkt = (SearchFilePkt_t *)buf;
	SearchFilePktl_t *pktl = (SearchFilePktl_t *)buf;
	
	if (fno != 0)
		return (void *)NULL;
	
	if (size == sizeof(SearchFilePktl_t)) {
		ee_addr = (void *)pktl->dest; 			 // Search File: Called from Not_Dual_layer Version
		p = (void *)&pktl->name[0];
	}
	else {
		if (size > sizeof(SearchFilePktl_t)) {
			if (size != (sizeof(SearchFilePktl_t) + 4))
				return (void *)NULL;
			ee_addr = (void *)pktl->dest;
			p = (char *)&pktl->name[0];	
		}
		else {
			if (size != sizeof(SearchFilePkt_t)) // Search File: Called from Old Library
				return (void *)NULL;
			ee_addr = (void *)pkt->dest;		
			p = (char *)&pkt->name[0];	
		}
	}
	
	cdvdsearchfile_res = sceCdSearchFile((cd_file_t *)buf, p);
	sysmemSendEE((void *)buf, (void *)ee_addr, sizeof(cd_file_t));
	
	return (void *)&cdvdsearchfile_res;
}

//-------------------------------------------------------------- 
void *cbrpc_cddiskready(u32 fno, void *buf, int size)
{	// CD Disk Ready RPC callback

	if (fno != 0)
		return (void *)NULL;

	if (*(u32 *)buf == 0)
		sceCdDiskReady(0);
	
	cdvddiskready_res = sceCdDiskReady(1);

	return (void *)&cdvddiskready_res;	
}

//-------------------------------------------------------------- 
void *cbrpc_cddiskready2(u32 fno, void *buf, int size)
{	// CD Disk Ready 2 RPC callback

	if (fno != 0)
		return (void *)NULL;

	cdvddiskready2_res = sceCdDiskReady(0);			
	
	return (void *)&cdvddiskready2_res;		
}

//-------------------------------------------------------------- 
int cdvd_readee(void *buf, int size)
{
	RpcCdvd_t *r = (RpcCdvd_t *)buf;
		
	return cdvdman_readee(r->lsn, r->sectors, r->buf, r->mode.datapattern, (void *)r->eeaddr1, (void *)r->eeaddr2);
}

//-------------------------------------------------------------- 
int cdvdman_readee(u32 lsn, u32 sectors, void *buf, u8 datapattern, void *addr1, void *addr2)
{	// Read Disc datas to EE mem buffer
	
	u32 nbytes, nsectors, sectors_to_read, size_64b, size_64bb, bytesent, temp;
	int sector_size, flag_64b;
	void *fsvRbuf, *eeaddr_64b, *eeaddr2_64b;	
	
	if (sectors == 0) {
		cdvdman_stat.cderror = CDVD_ERR_ILI;
		return 0;
	}
	
	sector_size = 2328;
	
	if ((datapattern & 0xff) != 1) {
		sector_size = 2340;
		if ((datapattern & 0xff) != 2) 
			sector_size = 2048;
	}
	
	cdvdman_diskready = CDVD_READY_NOTREADY;
	
	addr1 = (void *)((u32)addr1 & 0x1fffffff);
	addr2 = (void *)((u32)addr2 & 0x1fffffff);
	buf = (void *)((u32)buf & 0x1fffffff);
	
	fsvRbuf = sceCdGetFsvRbuf();
		
	sceCdDiskReady(0);
	
	sectors_to_read = sectors;  
	bytesent = 0;
	
	sceCdSync(0);
	
	if (addr2)
		memset((void *)cdvdman_iopmread_buf, 0, 16);
		
	cdvdman_eereadx.pdst1 = (void *)buf;
	eeaddr_64b = (void *)(((u32)buf + 0x3f) & 0xffffffc0); // get the next 64-bytes aligned address

	if ((u32)buf & 0x3f)
		cdvdman_eereadx.b1len = (((u32)buf & 0xffffffc0) - (u32)buf) + 64; // get how many bytes needed to get a 64 bytes alignment
	else
		cdvdman_eereadx.b1len = 0;
		
	nbytes = sectors * sector_size;
	
	temp = (u32)buf + nbytes;
	eeaddr2_64b = (void *)(temp & 0xffffffc0);
	temp -= (u32)eeaddr2_64b;
	cdvdman_eereadx.pdst2 = eeaddr2_64b;  // get the end address on a 64 bytes align 
	cdvdman_eereadx.b2len = temp; // get bytes remainder at end of 64 bytes align 
	fsvRbuf += temp;
	
	if (cdvdman_eereadx.b1len)
		flag_64b = 0; // 64 bytes alignment flag
	else {
		if (temp)
			flag_64b = 0;
		else
			flag_64b = 1;	
	}	
			
	cdvdman_waitNCmdsema();
	
	do {		
		do {
	
			if ((sectors_to_read == 0) || (cdvdman_cdNCmdBreak)) {
			
				if (addr1)
					sysmemSendEE((void *)&cdvdman_eereadx, (void *)addr1, sizeof(cdvdman_readee_t));
				
				if (addr2) {
					*((u32 *)&cdvdman_iopmread_buf[0]) = nbytes;
					sysmemSendEE((void *)cdvdman_iopmread_buf, (void *)addr2, 16);
				}
					
				cdvdman_signalNCmdsema();
			
				cdvdman_diskready = CDVD_READY_READY;
			
				return 1;
			}		
	
			if (flag_64b == 0) { // not 64 bytes aligned buf
				if (sectors_to_read < 4)
					nsectors = sectors_to_read;
				else
					nsectors = 3;	
				temp = nsectors + 1;
			}
			else { // 64 bytes aligned buf
				if (sectors_to_read < 5)
					nsectors = sectors_to_read;
				else
					nsectors = 4;
				temp = nsectors;		
			}
			
			cdvdman_readcd(lsn, temp, (void *)fsvRbuf);
	
			size_64b = nsectors * sector_size;
			size_64bb = size_64b;
		
			if (!flag_64b) {
				if (sectors_to_read == sectors) // check that was the first read
					memcpy((void *)cdvdman_eereadx.buf1, (void *)fsvRbuf, cdvdman_eereadx.b1len);
								
				if ((!flag_64b) && (sectors_to_read == nsectors) && (cdvdman_eereadx.b1len))
					size_64bb = size_64b - 64;
			}
								
			if (size_64bb > 0) { 
				sysmemSendEE((void *)(fsvRbuf + cdvdman_eereadx.b1len), (void *)eeaddr_64b, size_64bb);
				bytesent += size_64bb;
			}
				
			if (addr2) {
				temp = bytesent;
				if (temp < 0)
					temp += 2047;
				temp = temp >> 11;
				*((u32 *)&cdvdman_iopmread_buf[0]) = temp;
				sysmemSendEE((void *)cdvdman_iopmread_buf, (void *)addr2, 16);	
			}	
					
			sectors_to_read -= nsectors;
			lsn += nsectors;
			eeaddr_64b += size_64b;
		
		} while ((flag_64b) || (sectors_to_read));
	
		memcpy((void *)cdvdman_eereadx.buf2, (void *)(fsvRbuf + size_64b - cdvdman_eereadx.b2len), cdvdman_eereadx.b2len);
		
	} while (1);
				
	return 0;
}

//-------------------------------------------------------------- 
int cdvd_iopmread(void *buf, int size)
{	
	RpcCdvd_t *r = (RpcCdvd_t *)buf;

	return cdvdman_iopmemread(r->lsn, r->sectors, r->buf, (void *)r->eeaddr2);
}

//-------------------------------------------------------------- 
int cdvdman_iopmemread(u32 lsn, u32 sectors, void *buf, void *addr)
{	// Read Disc datas to IOP mem buffer, and send number of bytes readed to EE mem
	
	void *EEaddr;
		
	cdvdman_diskready = CDVD_READY_NOTREADY;
	
	EEaddr = (void *)((u32)(addr) & 0x1fffffff);
	
	cdvdman_waitNCmdsema();
	
	cdvdman_readcd(lsn, sectors, buf);
	
	if (EEaddr) {
		memset(cdvdman_iopmread_buf, 0, 16);
		*(u32 *)cdvdman_iopmread_buf = sectors << 11;
		sysmemSendEE(cdvdman_iopmread_buf, (void *)EEaddr, 16);	
	}
	
	cdvdman_signalNCmdsema();
	
	cdvdman_diskready = CDVD_READY_READY;	
	
	return 1;
}

//-------------------------------------------------------------- 
int cdvd_readchain(void *buf, int size)
{
	cdvdman_chain_t *ch = (cdvdman_chain_t *)buf;
		
	if (ch->lsn != -1) {
				
		do {		
			if ((u32)ch->buf & 1) // Bit 0 is used to know if we want data on IOP or EE
				cdvdman_iopmemread(ch->lsn, ch->sectors, (void *)(((u32)ch->buf) & 0xfffffffc), NULL);
			else
				cdvdman_readee(ch->lsn, ch->sectors, ch->buf, 0, NULL, NULL);	
			ch++;				
		} while (ch->lsn != -1);	
	}
	
	return 1;
}

//-------------------------------------------------------------- 
int cdvd_Stsubcmdcall(void *buf, int size)
{	// call a Stream Sub function (below) depending on stream cmd sent
	
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
			
	if ((u32)(St->cmd >= 10))
		return cdvdSt_dummy(buf);
	
	cdvd_Stsubcall_fn = cdvdStsubcall_tab[St->cmd]; 	
	
	return cdvd_Stsubcall_fn(buf);
}

//-------------------------------------------------------------- 
int cdvdSt_dummy(void *buf)
{
	return 0;
}

//-------------------------------------------------------------- 
int cdvdSt_start(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
	
	return sceCdStStart(St->lsn, &St->mode);
}

//-------------------------------------------------------------- 
int cdvdSt_read(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
	u32 *err;
	
	err = (u32 *)St->buf;
	
	return sceCdStRead(St->sectors, cdvdman_stat.Stiopbuf, 0xdeadfeed, err);
}

//-------------------------------------------------------------- 
int cdvdSt_stop(void *buf)
{
	return sceCdStStop();
}

//-------------------------------------------------------------- 
int cdvdSt_seek(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
		
	return sceCdStSeek(St->lsn);
}

//-------------------------------------------------------------- 
int cdvdSt_init(void *buf)
{
	RpcCdvdStInit_t *r = (RpcCdvdStInit_t *)buf;
		
	return sceCdStInit(r->bufmax, r->bankmax, r->buf);
}

//-------------------------------------------------------------- 
int cdvdSt_stat(void *buf)
{
	return sceCdStStat();
}

//-------------------------------------------------------------- 
int cdvdSt_pause(void *buf)
{
	return sceCdStPause();
}

//-------------------------------------------------------------- 
int cdvdSt_resume(void *buf)
{
	return sceCdStResume();
}

//-------------------------------------------------------------- 
int cdvdSt_seekF(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
		
	return sceCdStSeekF(St->lsn);
}

//-------------------------------------------------------------- 
void *sysmemAlloc(int size)
{
	int oldstate;
	void *ptr;
	
	CpuSuspendIntr(&oldstate);
	ptr = AllocSysMemory(ALLOC_FIRST, size, NULL);
	CpuResumeIntr(oldstate);
	
	return ptr;
}

//-------------------------------------------------------------- 
void sysmemFree(void *ptr)
{
	int oldstate;
	
	CpuSuspendIntr(&oldstate);
	FreeSysMemory(ptr);
	CpuResumeIntr(oldstate);	
}

//-------------------------------------------------------------- 
void sysmemSendEE(void *buf, void *EE_addr, int size)
{
	SifDmaTransfer_t dmat;
	int oldstate, id;
	
	size += 3;
	size &= 0xfffffffc;
					
	dmat.dest = (void *)EE_addr;
	dmat.size = size;
	dmat.src = (void *)buf;
	dmat.attr = 0;

	id = 0;
	while (!id) {
		CpuSuspendIntr(&oldstate);
		id = sceSifSetDma(&dmat, 1);
		CpuResumeIntr(oldstate);
	}
	
	while (sceSifDmaStat(id) >= 0);
}

//-------------------------------------------------------------- 
int cdvdman_createsema(int initial, int max)
{
	iop_sema_t smp;

	smp.initial = initial;
	smp.max = max;
	smp.attr = 0;	
	smp.option = 0;
	
	return CreateSema(&smp);	
}

//-------------------------------------------------------------- 
int cdvdman_createthread(void *thread, u32 priority, u32 stacksize)
{
	iop_thread_t thread_param;

	memset(&thread_param, 0, sizeof(iop_thread_t));
	
	thread_param.thread = (void *)thread;
 	thread_param.stacksize = stacksize;
	thread_param.priority = priority;
	thread_param.attr = TH_C;
				
	return CreateThread(&thread_param);	
}

//-------------------------------------------------------------- 
NCmdMbx_t *cdvdman_setNCmdMbx(void)
{
	int i;
	NCmdMbx_t *pmbx;
	
	if (!cdvdman_pMbxbuf) {
		// Here we init the thread message buffer
		cdvdman_pMbxbuf = sysmemAlloc(sizeof(NCmdMbx_t) * 4);

		memset(cdvdman_pMbxbuf, 0, sizeof(NCmdMbx_t) * 4);
		
		pmbx = (NCmdMbx_t *)cdvdman_pMbxbuf;
		
		for (i = 2; i >= 0; i--) {
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
int cdvdman_createMbx(void)
{
	iop_mbx_t mbx;
	
	memset(&mbx, 0, sizeof(iop_mbx_t));
	return CreateMbx(&mbx);	
}

//-------------------------------------------------------------- 
void cdvdman_sendNCmdMbx(int mbxid, cdvdman_NCmd_t *NCmdmsg, int size)
{
	NCmdMbx_t *pmbx;	
	
	pmbx = cdvdman_setNCmdMbx();
	
	if (size > sizeof(cdvdman_NCmd_t)) // 32
		size = sizeof(cdvdman_NCmd_t); // 32
		
	if (size)
		memcpy((void *)&pmbx->NCmdmsg, (void *)NCmdmsg, size);
	
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
int cdrom_dummy(void)
{
	return -5;
}

//-------------------------------------------------------------- 
s64 cdrom_dummy64(void)
{ 
	return -5;
}

//-------------------------------------------------------------- 
int cdrom_init(iop_device_t *dev)
{
	return 0;
}

//-------------------------------------------------------------- 
int cdrom_deinit(iop_device_t *dev)
{
	return 0;
}

//-------------------------------------------------------------- 
int cdrom_open(iop_file_t *f, char *filename, int mode)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("cdrom_open %s mode = %d\n", filename, mode);	
#endif
			
	return isofs_dev->ops->open(f, filename, mode);
}

//-------------------------------------------------------------- 
int cdrom_close(iop_file_t *f)
{	
#ifdef NETLOG_DEBUG	
	pNetlogSend("cdrom_close %d\n", (int)f);	
#endif
			
	return isofs_dev->ops->close(f);
}

//-------------------------------------------------------------- 
int cdrom_read(iop_file_t *f, void *buf, u32 size)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("cdrom_read %d size = %d\n", (int)f, size);	
#endif
			
	return isofs_dev->ops->read(f, buf, size);
}

//-------------------------------------------------------------- 
int cdrom_lseek(iop_file_t *f, u32 pos, int where)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("cdrom_lseek %d pos = %d where = %d\n", (int)f, pos, where);	
#endif
			
	return isofs_dev->ops->lseek(f, pos, where);
}

//-------------------------------------------------------------- 
int cdrom_getstat(iop_file_t *f, char *filename, fio_stat_t *stat)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("cdrom_getstat %s\n", filename);	
#endif
			
	return isofs_dev->ops->getstat(f, filename, stat);
}

//-------------------------------------------------------------- 
int cdrom_dopen(iop_file_t *f, char *dirname)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("cdrom_dopen %s\n", dirname);	
#endif
			
	return isofs_dev->ops->dopen(f, dirname);
}

//-------------------------------------------------------------- 
int cdrom_dread(iop_file_t *f, fio_dirent_t *dirent)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("cdrom_dread %d\n", (int)f);	
#endif
			
	return isofs_dev->ops->dread(f, dirent);
}

//-------------------------------------------------------------- 
int cdrom_dclose(iop_file_t *f)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("cdrom_dclose %d\n", (int)f);	
#endif
			
	return isofs_dev->ops->dclose(f);
}

//-------------------------------------------------------------- 
int	cdrom_ioctl(int fd, int cmd, void *args)
{
	return 0;
}

//-------------------------------------------------------------- 
int cdrom_devctl(const char *name, int cmd, void *args, int arglen, void *buf, int buflen)
{
	return 0;
}

//-------------------------------------------------------------- 
int cdrom_ioctl2(int fd, int cmd, void *args, int arglen, void *buf, int buflen)
{
	return 0;
}

//-------------------------------------------------------------- 
void cdvdman_initdev(void)
{
	DelDrv("cdrom");
	AddDrv((iop_device_t *)&cdrom_dev);
}	

//-------------------------------------------------------------- 
void cdvdman_cdinit(void)
{
	iop_library_table_t *libtable;
	iop_library_t *libptr;
	int i;
	void **export_tab;
	char isofs_modname[8] = "isofs\0\0\0";
#ifdef NETLOG_DEBUG
	char netlog_modname[8] = "netlog\0\0";
#endif
	
	if (cdvdman_inited)
		return;
		
	cdvdman_readcdsema = cdvdman_createsema(1, 1);
	cdvdman_searchfilesema = cdvdman_createsema(1, 1);
	
	cdvdman_cdNCmdBreak = 0;
		
	// Get isofs lib ptr
	libtable = GetLibraryEntryTable();
	libptr = libtable->tail;
	while (libptr != 0) {
		for (i=0; i<8; i++) {
			if (libptr->name[i] != isofs_modname[i])
				break;
		} 
		if (i==8)
			break;	
		libptr = libptr->prev;
	}
	
	// Get isofs export table	 
	export_tab = (void **)(((struct irx_export_table *)libptr)->fptrs);
	
	// *****************************	
	// Set functions pointers here

	isofs_GetDevice = export_tab[4];
	isofs_ReadSect = export_tab[5];	
	isofs_FindFile = export_tab[6];	
	isofs_GetDiscType = export_tab[7];		
	//...
	
	isofs_dev = isofs_GetDevice();

#ifdef NETLOG_DEBUG	
	// Get netlog lib ptr
	libtable = GetLibraryEntryTable();
	libptr = libtable->tail;
	while (libptr != 0) {
		for (i=0; i<8; i++) {
			if (libptr->name[i] != netlog_modname[i])
				break;
		} 
		if (i==8)
			break;	
		libptr = libptr->prev;
	}
	
	// Get netlog export table	 
	export_tab = (void **)(((struct irx_export_table *)libptr)->fptrs);
	
	// *****************************	
	// Set functions pointers here
	pNetlogSend = export_tab[6];
	//...
	
	pNetlogSend("cdvdman_cdinit\n");	
#endif
	
	cdvdman_inited = 1;	
}

//-------------------------------------------------------------- 
int cdvdman_readDisc(u32 lsn, u32 sectors, void *buf)
{
	int r = 0;
	
#ifdef NETLOG_DEBUG		
	pNetlogSend("cdvdman_readDisc lsn=%d sectors=%d\n", lsn, sectors);
#endif
				
	if ((sectors == 0) || (cdvdman_cdNCmdBreak))
		return 0;
	
	/////////////////////////////////////////////////////////////
	//
	// Here it must perform read operations, using exports
	// of isofs irx
	//
	/////////////////////////////////////////////////////////////
		
	r = isofs_ReadSect(lsn, sectors, buf);
	if ((!r) || (cdvdman_cdNCmdBreak))
		return 0;
		
	return 1;
}

//-------------------------------------------------------------- 
int cdvdman_readcd(u32 lsn, u32 sectors, void *buf)
{
	int r;

#ifdef NETLOG_DEBUG		
	//pNetlogSend("cdvdman_readcd lsn=%d sectors=%d\n", lsn, sectors);
#endif
			
	if (cdvdman_cdNCmdBreak)
		return 2;
				
	WaitSema(cdvdman_readcdsema);	
	
	r = cdvdman_readDisc(lsn, sectors, (void *)(((u32)buf) & 0x0fffffff));
	if (r) {
		r = (cdvdman_cdNCmdBreak > 0) ? 2 : 0;				
	}
	else {
		r = 1;
#ifdef NETLOG_DEBUG 		
		pNetlogSend("cdvdman_readcd failed!!!\n");
#endif		
		//SleepThread();
	}
	
	cdvdman_cdNCmdBreak = 0;	
		
	SignalSema(cdvdman_readcdsema);
	
	return r;
}

//-------------------------------------------------------------- 
int sceCdInit(int init_mode)
{
	iop_event_t event;

	if (init_mode == CDVD_INIT_EXIT) {
		return 1;
	}
	
	memset((void *)&cdvdman_stat, 0, sizeof(cdvdman_status_t));
	
	cdvdman_stat.cderror = CDVD_ERR_NORDY;
	cdvdman_stat.cdstat = CDVD_STAT_PAUSE;
	
	cdvdman_sendNCmd(NCMD_INIT, NULL, 0); 
	
	cdvdman_waitsignalNCmdsema();
	
	cdvdman_stat.cddiskready = CDVD_READY_READY;
	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	event.attr = 2;
	event.option = 0;
	event.bits = 0;
	
	cdvdman_stat.cdvdman_intr_ef = CreateEventFlag(&event);
		
	return 1;
}

//-------------------------------------------------------------- 
int sceCdStandby(void)
{
	int r;

	cdvdman_stat.cderror = CDVD_ERR_NO;
		
	r = cdvdman_sendNCmd(NCMD_STANDBY, NULL, 0);	
	return (r > 0) ? 1 : 0;	
}

//-------------------------------------------------------------- 
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
	int r;
	u8 wdbuf[16];

#ifdef NETLOG_DEBUG		
	//pNetlogSend("sceCdRead lsn=%d sectors=%d\n", lsn, sectors);
#endif

	cdvdman_stat.cderror = CDVD_ERR_NO;	
		
	*((u32 *)&wdbuf[0]) = lsn;
	*((u32 *)&wdbuf[4]) = sectors;
	*((u32 *)&wdbuf[8]) = (u32)buf;
	
	cdvdman_stat.cdstat = CDVD_STAT_READ;
	
	r = cdvdman_sendNCmd(NCMD_READ, (void *)wdbuf, 12);
	if (!r) {
		cdvdman_stat.cdstat = CDVD_STAT_PAUSE;	
		return 0;
	}
	
	return 1;	
}

//-------------------------------------------------------------- 
int sceCdSeek(u32 lsn)
{
	int r;
	u8 wdbuf[16];
		
	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	*((u32 *)&wdbuf[0]) = lsn;
	
	cdvdman_stat.cdstat = CDVD_STAT_SEEK;

	r = cdvdman_sendNCmd(NCMD_SEEK, (void *)wdbuf, 4);
	if (!r) {
		cdvdman_stat.cdstat = CDVD_STAT_PAUSE;	
		return 0;
	}
	
	cdvdman_stat.cdcurlsn = lsn;
	
	return 1;	
}

//-------------------------------------------------------------- 
int sceCdGetError(void)
{	
	return cdvdman_stat.cderror;
}

//-------------------------------------------------------------- 
int sceCdGetToc(void *toc) 
{
	u8 wdbuf[16];

	cdvdman_stat.cderror = CDVD_ERR_NO;	
		
	*((u32 *)&wdbuf[0]) = (u32)toc;
	
	cdvdman_stat.cdstat = CDVD_STAT_READ;
	
	cdvdman_sendNCmd(NCMD_GETTOC, (void *)wdbuf, 4);
				
	return 1;
}

//-------------------------------------------------------------- 
int sceCdSync(int mode)
{	
	if (mode >= 18)
		return cdSync_dummy();
	
	cdSync_fn = cdSync_tab[mode];
	
	return cdSync_fn();
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

//--------------------------------------------------------------
int sceCdGetDiskType(void)
{	
	cdvdman_stat.cderror = CDVD_ERR_NO;
				
	return isofs_GetDiscType();
}

//-------------------------------------------------------------- 
int sceCdDiskReady(int mode)
{
	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	if (mode == 0) {
		while (!(sceCdDiskReady(1)))
			DelayThread(5000);
	}
	
	if (!cdvdman_stat.cdNCmd) {
		if (cdvdman_stat.cddiskready)
			return CDVD_READY_READY;
	}
	
	return CDVD_READY_NOTREADY;
}

//-------------------------------------------------------------- 
int sceCdTrayReq(int mode, u32 *traycnt)
{	
	////////////////////////////////////////////////////////////
	//
	// We don't want to have the Tray opened since all is virtual...
	//
	////////////////////////////////////////////////////////////

	cdvdman_stat.cderror = CDVD_ERR_NO;
			
	if (mode == CDVD_TRAY_CLOSE) 
		cdvdman_stat.traychk = 0;
	else if (mode == CDVD_TRAY_OPEN)
		cdvdman_stat.traychk = 1;
		
	if (traycnt)
		*traycnt = cdvdman_stat.traychk;		
	
	return 1;
}

//-------------------------------------------------------------- 
int sceCdStop(void)
{
	int r;

	cdvdman_stat.cderror = CDVD_ERR_NO;
			
	r = cdvdman_sendNCmd(NCMD_STOP, NULL, 0);
		
	return (r > 0) ? 1 : 0;
}

//-------------------------------------------------------------- 
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

//-------------------------------------------------------------- 
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

//-------------------------------------------------------------- 
int sceCdRI(char *buf, int *stat)
{
	int r;
	u8 rdbuf[16];

	cdvdman_stat.cderror = CDVD_ERR_NO;
			
	r = cdvdman_sendSCmd(0x12, NULL, 0, rdbuf, 9);
	
	*stat = (int)rdbuf[0];
	memcpy((void *)buf, (void *)&rdbuf[1], 8);
	
	return r;
}

//-------------------------------------------------------------- 
int sceCdReadClock(cd_clock_t *rtc)
{
	int rc;

	cdvdman_stat.cderror = CDVD_ERR_NO;
				
	rc = cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);
	
	rtc->week = 0;
	rtc->month &= 0x7f;

	return rc;
}

//-------------------------------------------------------------- 
int sceCdStatus(void)
{
	return cdvdman_stat.cdstat;
}

//-------------------------------------------------------------- 
int sceCdApplySCmd(u8 cmd, void *in, u32 in_size, void *out)
{	
	return cdvdman_sendSCmd(cmd, in, in_size, out, 16);
}

//-------------------------------------------------------------- 
int *sceCdCallback(void *func)
{
	if ((u32)func & 1)
		func = cdvdman_stat.user_cb1;
		
	cdvdman_stat.user_cb0 = func;
		
	if (func)
		cdvdman_stat.user_cb1 = func;
		
	return (int *)1;	
}

//-------------------------------------------------------------- 
int sceCdPause(void)
{ 
	int r;

	cdvdman_stat.cderror = CDVD_ERR_NO;
			
	r = cdvdman_sendNCmd(NCMD_PAUSE, NULL, 0);
		
	return (r > 0) ? 1 : 0;
}

//-------------------------------------------------------------- 
int sceCdBreak(void)
{
#ifdef NETLOG_DEBUG		
	pNetlogSend("sceCdBreak\n");
#endif

	cdvdman_stat.cderror = CDVD_ERR_NO;
	cdvdman_stat.cdstat = CDVD_STAT_PAUSE;	
	
	cdvdman_cdNCmdBreak = 1;
	
	if (!(QueryIntrContext())) {
		
		while (cdvdman_stat.cdNCmd)
			DelayThread(100000);
	}

	cdvdman_cdNCmdBreak = 0;
	
	cdvdman_stat.cderror = CDVD_ERR_ABRT;

	cdvdman_handleUserCallback(SCECdFuncBreak);
			
	return 1;
}

//-------------------------------------------------------------- 
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
	int r;
	u8 wdbuf[16];

#ifdef NETLOG_DEBUG		
	//pNetlogSend("sceCdReadCdda lsn=%d sectors=%d\n", lsn, sectors);
#endif

	cdvdman_stat.cderror = CDVD_ERR_NO;	
		
	*((u32 *)&wdbuf[0]) = lsn;
	*((u32 *)&wdbuf[4]) = sectors;
	*((u32 *)&wdbuf[8]) = (u32)buf;
	
	cdvdman_stat.cdstat = CDVD_STAT_READ;
	
	r = cdvdman_sendNCmd(NCMD_READ, (void *)wdbuf, 12);
	if (!r) {
		cdvdman_stat.cdstat = CDVD_STAT_PAUSE;	
		return 0;
	}
	
	return 1;	
}

//-------------------------------------------------------------- 
int sceCdGetReadPos(void)
{ 	
	return cdvdman_stat.cdcurlsn;
}

//-------------------------------------------------------------- 
void *sceCdGetFsvRbuf(void)
{
	return (void *)cdvdman_fsvbuf;
}

//-------------------------------------------------------------- 
int sceCdSC(int code, int *param)
{
	if (code == -11)
		return cdvdman_stat.cdvdman_intr_ef;
	
	return 1;
}

//-------------------------------------------------------------- 
int sceCdRC(cd_clock_t *rtc)
{
	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	return cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);	
}

//-------------------------------------------------------------- 
int sceCdApplyNCmd(u8 cmd, void *in, u32 in_size)
{
	while (!cdvdman_sendNCmd(cmd, in, in_size))
		DelayThread(2000);
		
	sceCdSync(0);
	
	return 1;
}

//-------------------------------------------------------------- 
int sceCdRead0(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode, int csec, void *cb)
{
	int r;

#ifdef NETLOG_DEBUG		
	pNetlogSend("sceCdRead0 lsn=%d sectors=%d\n", lsn, sectors);
#endif
							
	WaitSema(cdvdman_readcdsema);	
	
	r = cdvdman_readDisc(lsn, sectors, (void *)(((u32)buf) & 0x0fffffff));
	if (!r)
		cdvdman_stat.cderror = CDVD_ERR_READ;
		
	SignalSema(cdvdman_readcdsema);
	
	return r;
}

//-------------------------------------------------------------- 
int sceCdRM(char *m, u32 *stat)
{
	// TODO !!!
	
	return 1;	
}

//-------------------------------------------------------------- 
int sceCdMmode(int mode)
{
	cdvdman_stat.cdmmode = mode;
	
	return 1;
}

//-------------------------------------------------------------- 
int *sceCdPOffCallback(void *func, void *addr)
{
	void *old_cb;
		
	old_cb = cdvdman_stat.poff_cb;
	cdvdman_stat.poffaddr = addr;
	cdvdman_stat.poff_cb = func;
	
	return (int *)old_cb;
}

//-------------------------------------------------------------- 
int sceCdReadDiskID(u32 *id)
{
	*((u16 *)id) = (u16)0xadde;
		
	return 1;
}

//-------------------------------------------------------------- 
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start)
{
	*on_dual = 0;
	
	return 1;
}

//-------------------------------------------------------------- 
int sceCdLayerSearchFile(cdl_file_t *fp, const char *name, int layer)
{
	return sceCdSearchFile((cd_file_t *)fp, name);
}

//-------------------------------------------------------------- 
int sceCdApplySCmd2(u8 cmd, void *in, u32 in_size, void *out)
{
	return cdvdman_sendSCmd(cmd, in, in_size, out, 16);	
}

//-------------------------------------------------------------- 
int sceCdSearchFile(cd_file_t *pcd_file, const char *name)
{
	int r;
	cd_file_t cdfile;

#ifdef NETLOG_DEBUG
	//pNetlogSend("sceCdSearchFile %s\n", name);
#endif
	
	r = cdvdman_findfile(name, &cdfile);
	if (!r)
		return 0;
	
	memcpy((void *)pcd_file, (void *)&cdfile, sizeof(cd_file_t));
		
	return 1;	
}

//-------------------------------------------------------------- 
int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("sceCdStInit bufmax=%d bankmax=%d iop_buf=%08x\n", bufmax, bankmax, iop_bufaddr);
#endif

	cdvdman_stat.cderror = CDVD_ERR_NO;
	cdvdman_stat.Ststat = 0;
	cdvdman_stat.Stbufmax = bufmax;
	cdvdman_stat.Stsectmax = -1;
		
	if (!iop_bufaddr) {
		cdvdman_stat.Stiopbuf = sceCdGetFsvRbuf();
		cdvdman_stat.Stsectmax = 4;
	}
	else	
		cdvdman_stat.Stiopbuf = iop_bufaddr;
	
	return 1;
}

//-------------------------------------------------------------- 
int sceCdStRead(u32 sectors, void *buf, u32 mode, u32 *err)
{
	
	u32 rsectors;
	int nsectors, rpos, bpos;
	u8 *p;
	
#ifdef NETLOG_DEBUG	
	pNetlogSend("sceCdStRead sectors=%d mode=%x buf=%x\n", sectors, mode, buf);
#endif	
	
	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	nsectors = 0;
	bpos = 0;
	
	do {
		rsectors = sectors - nsectors;
		rpos = nsectors << 11;
		
		if (cdvdman_stat.Stsectmax == -1) {
			bpos = rpos;
		}
		else {
			if (sectors > cdvdman_stat.Stsectmax)
				rsectors = cdvdman_stat.Stsectmax;		
		}
		
		p = (u8 *)buf;
		sceCdRead0(cdvdman_stat.Stlsn, rsectors, &p[bpos], NULL, 0, NULL);
		cdvdman_stat.Stlsn += rsectors;
		
		if (mode == 0xdeadfeed) { 
			// catch calls from rpc, in this case err is an EE address to send datas
			p = (u8 *)err;
			sysmemSendEE((void *)buf, &p[rpos], rsectors << 11);
			DelayThread(5000);
		}
	
		nsectors += rsectors;
		
	} while (nsectors < sectors);

	if (mode != 0xdeadfeed) {	
		if (err)
			*err = sceCdGetError();		
	}

	return sectors;	
}

//-------------------------------------------------------------- 
int sceCdStSeek(u32 lsn)
{
#ifdef NETLOG_DEBUG		
	pNetlogSend("sceCdStSeek lsn=%d\n", lsn);		
#endif

	cdvdman_stat.cderror = CDVD_ERR_NO;
	cdvdman_stat.Stlsn = lsn;
	
	return 1;
}

//-------------------------------------------------------------- 
int sceCdStSeekF(u32 lsn)
{	
	return sceCdStSeek(lsn);
}

//-------------------------------------------------------------- 
int sceCdStStart(u32 lsn, cd_read_mode_t *mode)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("sceCdStStart lsn=%d\n", lsn);
#endif


	if (mode->datapattern)
		cdvdman_stat.cderror = CDVD_ERR_READ;
	else {
		cdvdman_stat.cderror = CDVD_ERR_NO;		
		cdvdman_stat.Stlsn = lsn;
		cdvdman_stat.Ststat = 0;
		cdvdman_stat.cdstat = CDVD_STAT_SPIN;
	}	

	return 1;	
}

//-------------------------------------------------------------- 
int sceCdStStop(void)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("sceCdStStop\n");
#endif

	cdvdman_stat.Ststat = 1;
	cdvdman_stat.cderror = CDVD_ERR_NO;	
	cdvdman_stat.cdstat = CDVD_STAT_PAUSE;	

	return 1;
}

//-------------------------------------------------------------- 
int sceCdStStat(void)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("sceCdStStat\n");
#endif

	if (cdvdman_stat.Ststat == 1) 
		return 0;

	return cdvdman_stat.Stbufmax;	
}

//-------------------------------------------------------------- 
int sceCdStPause(void)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("sceCdStPause\n");
#endif

	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	return 1;
}

//-------------------------------------------------------------- 
int sceCdStResume(void)
{
#ifdef NETLOG_DEBUG	
	pNetlogSend("sceCdStResume\n");
#endif

	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	return 1;
}

//-------------------------------------------------------------- 
void cdvdman_startNCmdthread(void)
{
	int thid;
	
	cdvdman_mbxid = cdvdman_createMbx();
	cdvdman_NCmdsema = cdvdman_createsema(1, 1);
	cdvdman_userCBsema = cdvdman_createsema(1, 1);
	cdvdman_NCmdlocksema = 1;
	cdvdman_NCmdsemacount = 0;
	
	thid = cdvdman_createthread((void *)cdvdman_NCmdthread, 1, 0x2000);
	StartThread(thid, NULL);
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
		memcpy(cdvdman_NCmd.nbuf, ndata, ndlen);
		
	ClearEventFlag(cdvdman_stat.cdvdman_intr_ef, 1);
	
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
						
		SetEventFlag(cdvdman_stat.cdvdman_intr_ef, 1);
	}
}

//-------------------------------------------------------------- 
void cdvdman_NCmdCall(u8 ncmd, void *ndata)
{
	if ((u32)(ncmd >= 8))
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

	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	lsn = *((u32 *)&wdbuf[0]);
	sectors = *((u32 *)&wdbuf[4]);
	buf = (void *)*((u32 *)&wdbuf[8]);

	r = cdvdman_readcd(lsn, sectors, buf);
	if (r) {
		if (r == 1)
			cdvdman_stat.cderror = CDVD_ERR_READ;
		else if (r == 2)
			cdvdman_stat.cderror = CDVD_ERR_ABRT;		
	}
	
	cdvdman_stat.cdstat = CDVD_STAT_PAUSE;
	
	cdvdman_stat.cdcurlsn = lsn + sectors;
	
	if (r >= 0)
		cdvdman_handleUserCallback(SCECdFuncRead);
}

//-------------------------------------------------------------- 
void NCmd_cdReadCDDA(void *ndata)
{
	int r;
	u32 lsn, sectors;
	void *buf;
	u8 *wdbuf = (u8 *)ndata;

	cdvdman_stat.cderror = CDVD_ERR_NO;
	
	lsn = *((u32 *)&wdbuf[0]);
	sectors = *((u32 *)&wdbuf[4]);
	buf = (void *)*((u32 *)&wdbuf[8]);

	r = cdvdman_readcd(lsn, sectors, buf);
	if (r) {
		if (r == 1)
			cdvdman_stat.cderror = CDVD_ERR_READ;
		else if (r == 2)
			cdvdman_stat.cderror = CDVD_ERR_ABRT;		
	}
	
	cdvdman_stat.cdstat = CDVD_STAT_PAUSE;
	
	cdvdman_stat.cdcurlsn = lsn + sectors;
	
	if (r >= 0)
		cdvdman_handleUserCallback(SCECdFuncReadCDDA);
}

//-------------------------------------------------------------- 
void NCmd_cdGetToc(void *ndata)
{
	cdvdman_stat.cderror = CDVD_ERR_READ;
	cdvdman_stat.cdstat = CDVD_STAT_PAUSE;
			
	cdvdman_handleUserCallback(SCECdFuncGetToc); 
}

//-------------------------------------------------------------- 
void NCmd_cdSeek(void *ndata)
{
	u32 lsn;
	u8 *wdbuf = (u8 *)ndata;

	lsn = *((u32 *)&wdbuf[0]);
		
	cdvdman_stat.cdstat = CDVD_STAT_PAUSE;
	cdvdman_stat.cdcurlsn = lsn;
	
	cdvdman_handleUserCallback(SCECdFuncSeek); 
}

//-------------------------------------------------------------- 
void NCmd_cdStandby(void *ndata)
{
	cdvdman_stat.cdstat = CDVD_STAT_SPIN;
	
	cdvdman_handleUserCallback(SCECdFuncStandby);	
}

//-------------------------------------------------------------- 
void NCmd_cdStop(void *ndata)
{
	cdvdman_stat.cdstat = CDVD_STAT_STOP;
	
	cdvdman_handleUserCallback(SCECdFuncStop);
}

//-------------------------------------------------------------- 
void NCmd_cdPause(void *ndata)
{
	cdvdman_stat.cderror = CDVD_ERR_NO;
	cdvdman_stat.cdstat = CDVD_STAT_PAUSE;	
	
	cdvdman_handleUserCallback(SCECdFuncPause);
}

//-------------------------------------------------------------- 
void cdvdman_handleUserCallback(int reason)
{
	if (cdvdman_stat.user_cb0) {
		
		cdvdman_stat.cdNCmd = 0;
		
		cdvdman_callUserCBfunc(cdvdman_stat.user_cb0, reason);
	}
}

//-------------------------------------------------------------- 
void cdvdman_callUserCBfunc(void *cb, int reason)
{
	iop_sys_clock_t sys_clock;
	
	cdvdman_userCBreason = reason;
	
	if (QueryIntrContext()) {
		cbfunc = cb;
		cbfunc(reason);
	} 
	else {
		USec2SysClock(1, &sys_clock);
		
		if (!(SetAlarm(&sys_clock, alarm_cb, cb)))
			WaitSema(cdvdman_userCBsema);
	}
	
}

//-------------------------------------------------------------- 
unsigned int alarm_cb(void *cb)
{
	if (cb) {
		cbfunc = cb;
		cbfunc(cdvdman_userCBreason);
	}
		
	iSignalSema(cdvdman_userCBsema);
	
	return 0;	
}

//-------------------------------------------------------------- 
int cdvdman_findfile(const char *filepath, cd_file_t *cdfile)
{
	int r;
	static struct TocEntry tocEntry;	

	WaitSema(cdvdman_searchfilesema);

	char *p = (char *)filepath;
	if ((!strncmp(p, "\\\\", 2)) || (!strncmp(p, "//", 2)))
		p++;

	strcpy(cdvdman_filepath, p);

	r = isofs_FindFile(cdvdman_filepath, &tocEntry);
	if (!r) {
		SignalSema(cdvdman_searchfilesema);
		return 0;
	}
	
	cdfile->lsn = tocEntry.fileLBA;
	cdfile->size = tocEntry.fileSize; 
	strncpy(cdfile->name, tocEntry.filename, 16); 		

	// must fill date too

	SignalSema(cdvdman_searchfilesema);
			
	return 1;	
}

//-------------------------------------------------------------- 
void cdvdman_createSCmdsema(void)
{
	cdvdman_SCmdsema = cdvdman_createsema(1, 1);	
}	

//-------------------------------------------------------------- 
int cdvdman_writeSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size)
{
	int i;
	u8 dummy;
	u8 *p;
	u8 rdbuf[64];
		
	if (PollSema(cdvdman_SCmdsema) == 0xfffffe5b)
		return 0;
		
	if (CDVDreg_SDATAIN & 0x80)	{
		SignalSema(cdvdman_SCmdsema);
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
		
	while (CDVDreg_SDATAIN & 0xffffff80) {;}
	
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
	
	SignalSema(cdvdman_SCmdsema);
	
	return 1;
}

//-------------------------------------------------------------- 
int cdvdman_sendSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size)
{
	int r;
	
retry:
	
	r = cdvdman_writeSCmd(cmd & 0xff, in, in_size, out, out_size);
	if (!r) {
		DelayThread(2000);
		goto retry;
	}
	
	DelayThread(2000);
		
	return 1;
}

