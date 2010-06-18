/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
*/

#include <intrman.h>
#include <loadcore.h>
#include <stdio.h>
#include <sifcmd.h>
#include <sifman.h>
#include <sysmem.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>

#include "smsutils.h"

#define MODNAME "cdvdfsv"
IRX_ID(MODNAME, 1, 1);

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
	u8	trycount;
	u8	spindlctrl;
	u8	datapattern;
	u8	pad;
} cd_read_mode_t;

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

// cdvdman imports
int sceCdInit(int init_mode);						// #4
int sceCdStandby(void);							// #5
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode); 	// #6
int sceCdSeek(u32 lsn);							// #7
int sceCdGetError(void); 						// #8
int sceCdSearchFile(cd_file_t *fp, const char *name); 			// #10
int sceCdSync(int mode); 						// #11
int sceCdGetDiskType(void);						// #12
int sceCdDiskReady(int mode); 						// #13
int sceCdTrayReq(int mode, u32 *traycnt); 				// #14
int sceCdStop(void); 							// #15
int sceCdReadClock(cd_clock_t *rtc); 					// #24
int sceCdStatus(void); 							// #28
int sceCdApplySCmd(int cmd, void *in, u32 in_size, void *out); 		// #29
int sceCdPause(void);							// #38
int sceCdBreak(void);							// #39
int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr); 		// #56
int sceCdStRead(u32 sectors, void *buf, u32 mode, u32 *err); 		// #57
int sceCdStSeek(u32 lsn);						// #58
int sceCdStStart(u32 lsn, cd_read_mode_t *mode);			// #59
int sceCdStStat(void); 							// #60
int sceCdStStop(void);							// #61
int sceCdRead0(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode);	// #62
int sceCdStPause(void);							// #67
int sceCdStResume(void); 						// #68
int sceCdMmode(int mode); 						// #75
int sceCdStSeekF(u32 lsn); 						// #77
int sceCdReadDiskID(void *DiskID);					// #79
int sceCdReadGUID(void *GUID);						// #80
int sceCdSetTimeout(int param, int timeout);				// #81
int sceCdReadModelID(void *ModelID);					// #82
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start); 		// #83
int sceCdLayerSearchFile(cdl_file_t *fp, const char *name, int layer);	// #84

typedef struct {
	int	func_ret;
	int	cdvdfsv_ver;
	int	cdvdman_ver;
	int	debug_mode;
} cdvdinit_res_t;

typedef struct {
	int	result;
	cd_clock_t rtc;
} cdvdreadclock_res_t;

typedef struct {
	int	result;
	u32	param1;
	u32	param2;
} cdvdSCmd_res_t;

typedef struct {
	u16	cmd;
	u16	in_size;
	void	*in;
} cdapplySCmd_t __attribute__((aligned(64)));

typedef struct {		// size =0x124
	cd_file_t cdfile;	// 0x000
	char	name[256]; 	// 0x020
	void	*dest;		// 0x120
} SearchFilePkt_t;

typedef struct {		// size =0x128
	cdl_file_t cdfile; 	// 0x000
	char	name[256]; 	// 0x024
	void	*dest;		// 0x124
} SearchFilePkt2_t;

typedef struct {		// size =0x12c
	cdl_file_t cdfile; 	// 0x000
	char	name[256]; 	// 0x024
	void	*dest;		// 0x124
	int	layer;		// 0x128
} SearchFilePktl_t;

typedef struct {
	u32	lsn;
	u32	sectors;
	void	*buf;
	cd_read_mode_t mode;
	void	*eeaddr1;
	void	*eeaddr2;
} RpcCdvd_t;

typedef struct {
	u32	lsn;
	u32	sectors;
	void	*buf;
	int	cmd;
	cd_read_mode_t mode;
	u32	pad;
} RpcCdvdStream_t;

typedef struct {
	u32	bufmax;
	u32	bankmax;
	void	*buf;
	u32	pad;
} RpcCdvdStInit_t;

typedef struct {
	u32	lsn;		// sector location to start reading from
	u32	sectors;	// number of sectors to read
	void	*buf;		// buffer address to read to ( bit0: 0=EE, 1=IOP )
} RpcCdvdchain_t;		// (EE addresses must be on 64byte alignment)

typedef struct { // size = 144
	u32	b1len;
	u32	b2len;
	void	*pdst1;
	void	*pdst2;
	u8	buf1[64];
	u8	buf2[64];
} cdvdfsv_readee_t;

// internal prototypes
void init_thread(void *args);
void cdvdfsv_startrpcthreads(void);
void cdvdfsv_rpc0_th(void *args);
void cdvdfsv_rpc1_th(void *args);
void cdvdfsv_rpc2_th(void *args);
void *cbrpc_cdinit(u32 fno, void *buf, int size);
void *cbrpc_cdvdScmds(u32 fno, void *buf, int size);
void *(*rpcSCmd_fn)(u32 fno, void *buf, int size);
void *rpcSCmd_cdapplySCmd(u32 fno, void *buf, int size);
void *rpcSCmd_cdreadclock(u32 fno, void *buf, int size);
void *rpcSCmd_cdgetdisktype(u32 fno, void *buf, int size);
void *rpcSCmd_cdgeterror(u32 fno, void *buf, int size);
void *rpcSCmd_cdtrayreq(u32 fno, void *buf, int size);
void *rpcSCmd_cdstatus(u32 fno, void *buf, int size);
void *rpcSCmd_cdabort(u32 fno, void *buf, int size);
void *rpcSCmd_cdpoweroff(u32 fno, void *buf, int size);
void *rpcSCmd_cdmmode(u32 fno, void *buf, int size);
void *rpcSCmd_cdreadGUID(u32 fno, void *buf, int size);
void *rpcSCmd_cdsettimeout(u32 fno, void *buf, int size);
void *rpcSCmd_cdreadModelID(u32 fno, void *buf, int size);
void *rpcSCmd_cdreaddvddualinfo(u32 fno, void *buf, int size);
void *rpcSCmd_dummy(u32 fno, void *buf, int size);
void *cbrpc_cddiskready(u32 fno, void *buf, int size);
void *cbrpc_cddiskready2(u32 fno, void *buf, int size);
void *cbrpc_cdvdNcmds(u32 fno, void *buf, int size);
void *(*rpcNCmd_fn)(u32 fno, void *buf, int size);
void *rpcNCmd_cdread(u32 fno, void *buf, int size);
void *rpcNCmd_cdgettoc(u32 fno, void *buf, int size);
void *rpcNCmd_cdseek(u32 fno, void *buf, int size);
void *rpcNCmd_cdstandby(u32 fno, void *buf, int size);
void *rpcNCmd_cdstop(u32 fno, void *buf, int size);
void *rpcNCmd_cdpause(u32 fno, void *buf, int size);
void *rpcNCmd_cdstream(u32 fno, void *buf, int size);
void *rpcNCmd_iopmread(u32 fno, void *buf, int size);
void *rpcNCmd_cddiskready(u32 fno, void *buf, int size);
void *rpcNCmd_cdreadchain(u32 fno, void *buf, int size);
void *rpcNCmd_cdreadDiskID(u32 fno, void *buf, int size);
void *rpcNCmd_cdgetdisktype(u32 fno, void *buf, int size);
void *rpcNCmd_dummy(u32 fno, void *buf, int size);
void *cbrpc_S596(u32 fno, void *buf, int size);
void *cbrpc_cdsearchfile(u32 fno, void *buf, int size);
void cdvd_Stsubcmdcall(void *buf);
void (*cdvd_Stsubcall_fn)(void *buf);
void cdvdSt_dummy(void *buf);
void cdvdSt_start(void *buf);
void cdvdSt_read(void *buf);
void cdvdSt_stop(void *buf);
void cdvdSt_seek(void *buf);
void cdvdSt_init(void *buf);
void cdvdSt_stat(void *buf);
void cdvdSt_pause(void *buf);
void cdvdSt_resume(void *buf);
void cdvdSt_seekF(void *buf);
void cdvd_iopmread(void *buf);
void cdvd_readchain(void *buf);
void cdvd_readee(void *buf);
void sendcurlsnEE(void *eeaddr, u32 lsn);

// rpcSCmd funcs array
void *rpcSCmd_tab[40] = {
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_cdreadclock,
	(void *)rpcSCmd_dummy,		// cdwriteclock
	(void *)rpcSCmd_cdgetdisktype,
	(void *)rpcSCmd_cdgeterror,
	(void *)rpcSCmd_cdtrayreq,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_dummy,
	(void *)rpcSCmd_cdapplySCmd,	// ApplySCmd
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
	(void *)rpcSCmd_dummy,		// CancelPowerOff
	(void *)rpcSCmd_dummy,		// BlueLedCtrl
	(void *)rpcSCmd_cdpoweroff,
	(void *)rpcSCmd_cdmmode,
	(void *)rpcSCmd_dummy,		// SetThreadPriority
	(void *)rpcSCmd_cdreadGUID,
	(void *)rpcSCmd_cdsettimeout,
	(void *)rpcSCmd_cdreadModelID,
	(void *)rpcSCmd_cdreaddvddualinfo
};

// rpcNCmd funcs array
void *rpcNCmd_tab[24] = {
	(void *)rpcNCmd_dummy,
	(void *)rpcNCmd_cdread,
	(void *)rpcNCmd_cdread,		// Cdda Read
	(void *)rpcNCmd_cdread,		// Dvd Read
	(void *)rpcNCmd_cdgettoc,
	(void *)rpcNCmd_cdseek,
	(void *)rpcNCmd_cdstandby,
	(void *)rpcNCmd_cdstop,
	(void *)rpcNCmd_cdpause,
	(void *)rpcNCmd_cdstream,
	(void *)rpcNCmd_dummy,		// CDDA Stream
	(void *)rpcNCmd_dummy,
	(void *)rpcNCmd_dummy,		// Apply NCMD
	(void *)rpcNCmd_iopmread,
	(void *)rpcNCmd_cddiskready,
	(void *)rpcNCmd_cdreadchain,
	(void *)rpcNCmd_dummy,
	(void *)rpcNCmd_cdreadDiskID,
	(void *)rpcNCmd_dummy,
	(void *)rpcNCmd_dummy,		// ???
	(void *)rpcNCmd_dummy,
	(void *)rpcNCmd_dummy,
	(void *)rpcNCmd_dummy,
	(void *)rpcNCmd_cdgetdisktype
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

cd_read_mode_t cdvdfsv_Stmode;
static u8 *cdvdfsv_buf;

u32 cbrpcS596_oldfno;
u32 cbrpcS596_fno;

SifRpcDataQueue_t rpc0_DQ __attribute__((aligned(16)));
SifRpcDataQueue_t rpc1_DQ __attribute__((aligned(16)));
SifRpcDataQueue_t rpc2_DQ __attribute__((aligned(16)));
SifRpcServerData_t cdinit_rpcSD, cddiskready_rpcSD, cddiskready2_rpcSD, cdvdScmds_rpcSD __attribute__((aligned(16)));
SifRpcServerData_t cdsearchfile_rpcSD, cdvdNcmds_rpcSD __attribute__((aligned(16)));
SifRpcServerData_t S596_rpcSD __attribute__((aligned(16)));

static u8 cdinit_rpcbuf[16] __attribute__((aligned(16)));
static u8 cddiskready_rpcbuf[16] __attribute__((aligned(16)));
static u8 cddiskready2_rpcbuf[16] __attribute__((aligned(16)));
static u8 cdvdScmds_rpcbuf[1024] __attribute__((aligned(16)));
static u8 cdsearchfile_rpcbuf[304] __attribute__((aligned(16)));
static u8 cdvdNcmds_rpcbuf[1024] __attribute__((aligned(16)));
static u8 S596_rpcbuf[16] __attribute__((aligned(16)));
static u8 curlsn_buf[16] __attribute__ ((aligned(64)));

#define CDVDFSV_BUF_SECTORS		2

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{
	iop_thread_t thread_param;
	int thread_id;

	FlushDcache();
	CpuEnableIntr();

	thread_param.attr = TH_C;
	thread_param.option = 0;
	thread_param.thread = (void *)init_thread;
	thread_param.stacksize = 0x2000;
	thread_param.priority = 0x51;

	thread_id = CreateThread(&thread_param);
	StartThread(thread_id, 0);

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
void init_thread(void *args)
{
	if (!sceSifCheckInit())
		sceSifInit();

	sceSifInitRpc(0);

	cdvdfsv_buf = AllocSysMemory(ALLOC_FIRST, (CDVDFSV_BUF_SECTORS << 11), NULL);
	if (!cdvdfsv_buf)
		SleepThread();

	cdvdfsv_startrpcthreads();
}

//-------------------------------------------------------------------------
void cdvdfsv_startrpcthreads(void)
{
	iop_thread_t thread_param;
	int thread_id;

	thread_param.attr = TH_C;
	thread_param.option = 0;
	thread_param.thread = (void *)cdvdfsv_rpc1_th;
	thread_param.stacksize = 0x2000;
	thread_param.priority = 0x51;

	thread_id = CreateThread(&thread_param);
	StartThread(thread_id, 0);

	thread_param.attr = TH_C;
	thread_param.option = 0;
	thread_param.thread = (void *)cdvdfsv_rpc2_th;
	thread_param.stacksize = 0x2000;
	thread_param.priority = 0x51;

	thread_id = CreateThread(&thread_param);
	StartThread(thread_id, 0);

	thread_param.attr = TH_C;
	thread_param.option = 0;
	thread_param.thread = (void *)cdvdfsv_rpc0_th;
	thread_param.stacksize = 0x2000;
	thread_param.priority = 0x51;

	thread_id = CreateThread(&thread_param);
	StartThread(thread_id, 0);
}

//-------------------------------------------------------------------------
void cdvdfsv_rpc0_th(void *args)
{
	sceSifSetRpcQueue(&rpc0_DQ, GetThreadId());

	sceSifRegisterRpc(&S596_rpcSD, 0x80000596, (void *)cbrpc_S596, S596_rpcbuf, NULL, NULL, &rpc0_DQ);
	sceSifRegisterRpc(&cddiskready2_rpcSD, 0x8000059c, (void *)cbrpc_cddiskready2, cddiskready2_rpcbuf, NULL, NULL, &rpc0_DQ);

	sceSifRpcLoop(&rpc0_DQ);
}

//-------------------------------------------------------------------------
void cdvdfsv_rpc1_th(void *args)
{
	// Starts cd Init rpc Server
	// Starts cd Disk ready rpc server
	// Starts SCMD rpc server;

	sceSifSetRpcQueue(&rpc1_DQ, GetThreadId());	

	sceSifRegisterRpc(&cdinit_rpcSD, 0x80000592, (void *)cbrpc_cdinit, cdinit_rpcbuf, NULL, NULL, &rpc1_DQ);
	sceSifRegisterRpc(&cdvdScmds_rpcSD, 0x80000593, (void *)cbrpc_cdvdScmds, cdvdScmds_rpcbuf, NULL, NULL, &rpc1_DQ);
	sceSifRegisterRpc(&cddiskready_rpcSD, 0x8000059a, (void *)cbrpc_cddiskready, cddiskready_rpcbuf, NULL, NULL, &rpc1_DQ);

	sceSifRpcLoop(&rpc1_DQ);
}

//-------------------------------------------------------------------------
void cdvdfsv_rpc2_th(void *args)
{
	// Starts NCMD rpc server
	// Starts Search file rpc server 

	sceSifSetRpcQueue(&rpc2_DQ, GetThreadId());

	sceSifRegisterRpc(&cdvdNcmds_rpcSD, 0x80000595, (void *)cbrpc_cdvdNcmds, cdvdNcmds_rpcbuf, NULL, NULL, &rpc2_DQ);
	sceSifRegisterRpc(&cdsearchfile_rpcSD, 0x80000597, (void *)cbrpc_cdsearchfile, cdsearchfile_rpcbuf, NULL, NULL, &rpc2_DQ);

	sceSifRpcLoop(&rpc2_DQ);
}

//-------------------------------------------------------------------------
void *cbrpc_cdinit(u32 fno, void *buf, int size)
{	// CD Init RPC callback

	cdvdinit_res_t *r = (cdvdinit_res_t *)buf;

	r->func_ret = sceCdInit(*(int *)buf);

	r->cdvdfsv_ver = 0x223;
	r->cdvdman_ver = 0x223;
	//r->debug_mode = 0xff;

	return buf;
}

//-------------------------------------------------------------------------
void *cbrpc_cdvdScmds(u32 fno, void *buf, int size)
{	// CD SCMD RPC callback

	if ((u32)(fno >= 40))
		return rpcSCmd_dummy(fno, buf, size);

	// Call SCMD RPC sub function
	rpcSCmd_fn = (void *)rpcSCmd_tab[fno];

	return rpcSCmd_fn(fno, buf, size);
}

//-------------------------------------------------------------- 
void *rpcSCmd_cdapplySCmd(u32 fno, void *buf, int size)
{	// CD ApplySCMD RPC SCMD

	cdapplySCmd_t *SCmd = (cdapplySCmd_t *)buf;

	sceCdApplySCmd(SCmd->cmd, SCmd->in, SCmd->in_size, buf);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdreadclock(u32 fno, void *buf, int size)
{	// CD Read Clock RPC SCMD
	cdvdreadclock_res_t *r = (cdvdreadclock_res_t *)buf;

	r->result = sceCdReadClock(&r->rtc);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdgetdisktype(u32 fno, void *buf, int size)
{	// CD Get Disc Type RPC SCMD

	*(int *)buf = sceCdGetDiskType();
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdgeterror(u32 fno, void *buf, int size)
{	// CD Get Error RPC SCMD

	*(int *)buf = sceCdGetError();
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdtrayreq(u32 fno, void *buf, int size)
{	// CD Tray Req RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdTrayReq(*((int *)buf), &r->param1);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdstatus(u32 fno, void *buf, int size)
{	// CD Status RPC SCMD

	*(int *)buf = sceCdStatus();
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdabort(u32 fno, void *buf, int size)
{	// CD Abort RPC SCMD

	*(int *)buf = sceCdBreak();
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdpoweroff(u32 fno, void *buf, int size)
{	// CD Power Off RPC SCMD

	*(int *)buf = 1;
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdmmode(u32 fno, void *buf, int size)
{	// CD Media Mode RPC SCMD

	*(int *)buf = sceCdMmode(*(int *)buf);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdreadGUID(u32 fno, void *buf, int size)
{	// CD Read GUID RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdReadGUID(&r->param1);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdsettimeout(u32 fno, void *buf, int size)
{	// CD Set TimeOut RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdSetTimeout(*(int *)buf, r->param1);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdreadModelID(u32 fno, void *buf, int size)
{	// CD Read Disk ID RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdReadModelID(&r->param1);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_cdreaddvddualinfo(u32 fno, void *buf, int size)
{	// CD Read Dvd DualLayer Info RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdReadDvdDualInfo((int *)&r->param1, &r->param2);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcSCmd_dummy(u32 fno, void *buf, int size)
{
	*(int *)buf = 0;
	return buf;
}

//-------------------------------------------------------------------------
void *cbrpc_cddiskready(u32 fno, void *buf, int size)
{	// CD Disk Ready RPC callback

	if (*(int *)buf == 0)
		*(int *)buf = sceCdDiskReady(0);
	else
		*(int *)buf = sceCdDiskReady(1);

	return buf;	
}

//-------------------------------------------------------------------------
void *cbrpc_cddiskready2(u32 fno, void *buf, int size)
{	// CD Disk Ready2 RPC callback

	*(int *)buf = sceCdDiskReady(0);

	return buf;
}

//-------------------------------------------------------------------------
void *cbrpc_cdvdNcmds(u32 fno, void *buf, int size)
{	// CD NCMD RPC callback

	if ((u32)(fno >= 24))
		return rpcNCmd_dummy(fno, buf, size);

	// Call NCMD RPC sub function
	rpcNCmd_fn = (void *)rpcNCmd_tab[fno];

	return rpcNCmd_fn(fno, buf, size);
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdread(u32 fno, void *buf, int size)
{	// CD Read RPC NCMD

	cdvd_readee(buf);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdgettoc(u32 fno, void *buf, int size)
{	// CD Get TOC RPC NCMD

	*(int *)buf = 1;
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdseek(u32 fno, void *buf, int size)
{	// CD Seek RPC NCMD

	*(int *)buf = sceCdSeek(*(u32 *)buf);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdstandby(u32 fno, void *buf, int size)
{	// CD Standby RPC NCMD

	*(int *)buf = sceCdStandby();
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdstop(u32 fno, void *buf, int size)
{	// CD Stop RPC NCMD

	*(int *)buf = sceCdStop();
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdpause(u32 fno, void *buf, int size)
{	// CD Pause RPC NCMD

	*(int *)buf = sceCdPause();
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdstream(u32 fno, void *buf, int size)
{	// CD Stream RPC NCMD

	cdvd_Stsubcmdcall(buf);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_iopmread(u32 fno, void *buf, int size)
{	// CD IOP Mem Read RPC NCMD

	cdvd_iopmread(buf);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cddiskready(u32 fno, void *buf, int size)
{	// CD Disk Ready RPC NCMD

	*(int *)buf = sceCdDiskReady(0);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdreadchain(u32 fno, void *buf, int size)
{	// CD ReadChain RPC NCMD

	cdvd_readchain(buf);
	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdreadDiskID(u32 fno, void *buf, int size)
{
	u8 *p = (u8 *)buf;

	mips_memset(p, 0, 10);
	*(int *)buf = sceCdReadDiskID(&p[4]);

	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_cdgetdisktype(u32 fno, void *buf, int size)
{
	u8 *p = (u8 *)buf;
	*(int *)&p[4] = sceCdGetDiskType();
	*(int *)&p[0] = 1;

	return buf;
}

//-------------------------------------------------------------------------
void *rpcNCmd_dummy(u32 fno, void *buf, int size)
{
	*(int *)buf = 0;
	return buf;
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

//-------------------------------------------------------------------------
void *cbrpc_cdsearchfile(u32 fno, void *buf, int size)
{	// CD Search File RPC callback

	SifDmaTransfer_t dmat;
	int r, oldstate;
	void *ee_addr;
	char *p;
	SearchFilePkt_t *pkt = (SearchFilePkt_t *)buf;
	SearchFilePkt2_t *pkt2 = (SearchFilePkt2_t *)buf;
	SearchFilePktl_t *pktl = (SearchFilePktl_t *)buf;
	int pktsize = 0;

	if (size == sizeof(SearchFilePkt2_t)) {
		ee_addr = (void *)pkt2->dest; 		// Search File: Called from Not Dual_layer Version
		p = (void *)&pkt2->name[0];
		pktsize = sizeof(cdl_file_t);
		r = sceCdSearchFile((cd_file_t *)buf, p);
		pkt2->cdfile.flag = 0;
	}
	else {
		if (size > sizeof(SearchFilePkt2_t)) {	// Search File: Called from Dual_layer Version
			ee_addr = (void *)pktl->dest;
			p = (char *)&pktl->name[0];
			pktsize = sizeof(cdl_file_t);
			r = sceCdLayerSearchFile((cdl_file_t *)buf, p, pktl->layer);
			pktl->cdfile.flag = 0;
		}
		else {					// Search File: Called from Old Library
			ee_addr = (void *)pkt->dest;
			p = (char *)&pkt->name[0];
			pktsize = sizeof(cd_file_t);
			r = sceCdSearchFile((cd_file_t *)buf, p);
		}
	}

	dmat.dest = (void *)ee_addr;
	dmat.size = pktsize;
	dmat.src = (void *)buf;
	dmat.attr = 0;

	CpuSuspendIntr(&oldstate);
	while (!sceSifSetDma(&dmat, 1));
	CpuResumeIntr(oldstate);
	//while (sceSifDmaStat >= 0);

	*(int *)buf = r;

	return buf;
}

//-------------------------------------------------------------------------
void cdvd_Stsubcmdcall(void *buf)
{	// call a Stream Sub function (below) depending on stream cmd sent

	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

	if ((u32)(St->cmd >= 10)) {
		cdvdSt_dummy(buf);
		return;
	}

	cdvd_Stsubcall_fn = cdvdStsubcall_tab[St->cmd];
	cdvd_Stsubcall_fn(buf);
}

//-------------------------------------------------------------------------
void cdvdSt_dummy(void *buf)
{
	*(int *)buf = 0;
}

//-------------------------------------------------------------------------
void cdvdSt_start(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

	*(int *)buf = sceCdStStart(St->lsn, &cdvdfsv_Stmode);
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

//-------------------------------------------------------------------------
void cdvdSt_read(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
	u32 nsectors, err;
	int r;
	int rpos = 0;
	void *ee_addr = (void *)St->buf;

	while (St->sectors > 0) {
		if (St->sectors > CDVDFSV_BUF_SECTORS)
			nsectors = CDVDFSV_BUF_SECTORS;
		else
			nsectors = St->sectors;

		r = sceCdStRead(nsectors, cdvdfsv_buf, 0, &err);

		sysmemSendEE(cdvdfsv_buf, ee_addr, nsectors << 11);

		ee_addr += nsectors << 11;
		rpos += r;
		St->sectors -= r;
	}

	*(int *)buf = rpos;
}

//-------------------------------------------------------------------------
void cdvdSt_stop(void *buf)
{
	*(int *)buf = sceCdStStop();
}

//-------------------------------------------------------------------------
void cdvdSt_seek(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

	*(int *)buf = sceCdStSeek(St->lsn);
}

//-------------------------------------------------------------------------
void cdvdSt_init(void *buf)
{
	RpcCdvdStInit_t *r = (RpcCdvdStInit_t *)buf;

	*(int *)buf = sceCdStInit(r->bufmax, r->bankmax, r->buf);
}

//-------------------------------------------------------------------------
void cdvdSt_stat(void *buf)
{
	*(int *)buf = sceCdStStat();
}

//-------------------------------------------------------------------------
void cdvdSt_pause(void *buf)
{
	*(int *)buf = sceCdStPause();
}

//-------------------------------------------------------------------------
void cdvdSt_resume(void *buf)
{
	*(int *)buf = sceCdStResume();
}

//-------------------------------------------------------------------------
void cdvdSt_seekF(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

	*(int *)buf = sceCdStSeekF(St->lsn);
}

//-------------------------------------------------------------------------
void cdvd_iopmread(void *buf)
{	
	RpcCdvd_t *r = (RpcCdvd_t *)buf;

	sceCdRead0(r->lsn, r->sectors, r->buf, NULL);

	*(int *)buf = 1;
}

//-------------------------------------------------------------------------
void cdvd_readchain(void *buf)
{
	int i;
	u32 nsectors, tsectors, lsn, addr;

	RpcCdvdchain_t *ch = (RpcCdvdchain_t *)buf;

	for (i=0; i<64; i++) {

		if ((ch->lsn == -1) || (ch->sectors == -1) || ((u32)ch->buf == -1))
			break;

		lsn = ch->lsn;
		tsectors = ch->sectors;
		addr = (u32)ch->buf & 0xfffffffc;

		while (tsectors != 0) {

			if (tsectors > CDVDFSV_BUF_SECTORS)
				nsectors = CDVDFSV_BUF_SECTORS;
			else
				nsectors = tsectors;
				
			if ((u32)ch->buf & 1) { // IOP addr
				sceCdRead0(lsn, nsectors, (void *)addr, NULL);
			}
			else {			// EE addr
				sceCdRead0(lsn, nsectors, cdvdfsv_buf, NULL);
				sysmemSendEE(cdvdfsv_buf, (void *)addr, nsectors << 11);
			}

			lsn += nsectors;
			tsectors -= nsectors;
			addr += nsectors << 11;
		}
		ch++;
	}

	*(int *)buf = 1;
}

//-------------------------------------------------------------- 
void cdvd_readee(void *buf)
{	// Read Disc datas to EE mem buffer
	u32 nbytes, nsectors, sectors_to_read, size_64b, size_64bb, bytesent, temp;
	int sector_size, flag_64b;
	void *fsvRbuf = (void *)cdvdfsv_buf;
	void *eeaddr_64b, *eeaddr2_64b;
	cdvdfsv_readee_t readee;
	RpcCdvd_t *r = (RpcCdvd_t *)buf;

	if (r->sectors == 0) {
		*(int *)buf = 0;
		return;
	}

	sector_size = 2328;

	if ((r->mode.datapattern & 0xff) != 1) {
		sector_size = 2340;
		if ((r->mode.datapattern & 0xff) != 2)
			sector_size = 2048;
	}

	r->eeaddr1 = (void *)((u32)r->eeaddr1 & 0x1fffffff);
	r->eeaddr2 = (void *)((u32)r->eeaddr2 & 0x1fffffff);
	r->buf = (void *)((u32)r->buf & 0x1fffffff);

	sceCdDiskReady(0);

	sectors_to_read = r->sectors;  
	bytesent = 0;

	if (r->eeaddr2)
		mips_memset((void *)curlsn_buf, 0, 16);

	readee.pdst1 = (void *)r->buf;
	eeaddr_64b = (void *)(((u32)r->buf + 0x3f) & 0xffffffc0); // get the next 64-bytes aligned address

	if ((u32)r->buf & 0x3f)
		readee.b1len = (((u32)r->buf & 0xffffffc0) - (u32)r->buf) + 64; // get how many bytes needed to get a 64 bytes alignment
	else
		readee.b1len = 0;

	nbytes = r->sectors * sector_size;

	temp = (u32)r->buf + nbytes;
	eeaddr2_64b = (void *)(temp & 0xffffffc0);
	temp -= (u32)eeaddr2_64b;
	readee.pdst2 = eeaddr2_64b;  // get the end address on a 64 bytes align 
	readee.b2len = temp; // get bytes remainder at end of 64 bytes align 
	fsvRbuf += temp;

	if (readee.b1len)
		flag_64b = 0; // 64 bytes alignment flag
	else {
		if (temp)
			flag_64b = 0;
		else
			flag_64b = 1;
	}

	while (1) {
		do {
			if ((sectors_to_read == 0) || (sceCdGetError() == CDVD_ERR_ABRT)) {

				if (r->eeaddr1)
					sysmemSendEE((void *)&readee, (void *)r->eeaddr1, sizeof(cdvdfsv_readee_t));

				if (r->eeaddr2) {
					*((u32 *)&curlsn_buf[0]) = nbytes;
					sysmemSendEE((void *)curlsn_buf, (void *)r->eeaddr2, 16);
				}

				*(int *)buf = 1;
				return;
			}

			if (flag_64b == 0) { // not 64 bytes aligned buf
				if (sectors_to_read < CDVDFSV_BUF_SECTORS)
					nsectors = sectors_to_read;
				else
					nsectors = CDVDFSV_BUF_SECTORS-1;
				temp = nsectors + 1;
			}
			else { // 64 bytes aligned buf
				if (sectors_to_read < (CDVDFSV_BUF_SECTORS+1))
					nsectors = sectors_to_read;
				else
					nsectors = CDVDFSV_BUF_SECTORS;
				temp = nsectors;
			}

			sceCdRead0(r->lsn, temp, (void *)fsvRbuf, NULL);

			size_64b = nsectors * sector_size;
			size_64bb = size_64b;

			if (!flag_64b) {
				if (sectors_to_read == r->sectors) // check that was the first read
					mips_memcpy((void *)readee.buf1, (void *)fsvRbuf, readee.b1len);

				if ((!flag_64b) && (sectors_to_read == nsectors) && (readee.b1len))
					size_64bb = size_64b - 64;
			}

			if (size_64bb > 0) {
				sysmemSendEE((void *)(fsvRbuf + readee.b1len), (void *)eeaddr_64b, size_64bb);
				bytesent += size_64bb;
			}

			if (r->eeaddr2) {
				temp = bytesent;
				if (temp < 0)
					temp += 2047;
				temp = temp >> 11;
				*((u32 *)&curlsn_buf[0]) = temp;
				sysmemSendEE((void *)curlsn_buf, (void *)r->eeaddr2, 16);
			}

			sectors_to_read -= nsectors;
			r->lsn += nsectors;
			eeaddr_64b += size_64b;

		} while ((flag_64b) || (sectors_to_read));

		mips_memcpy((void *)readee.buf2, (void *)(fsvRbuf + size_64b - readee.b2len), readee.b2len);
	}

	*(int *)buf = 0;
}

//-------------------------------------------------------------------------
DECLARE_IMPORT_TABLE(cdvdman, 1, 1)
DECLARE_IMPORT(4, sceCdInit)
DECLARE_IMPORT(5, sceCdStandby)
DECLARE_IMPORT(6, sceCdRead)
DECLARE_IMPORT(7, sceCdSeek)
DECLARE_IMPORT(8, sceCdGetError)
DECLARE_IMPORT(10, sceCdSearchFile)
DECLARE_IMPORT(11, sceCdSync)
DECLARE_IMPORT(12, sceCdGetDiskType)
DECLARE_IMPORT(13, sceCdDiskReady)
DECLARE_IMPORT(14, sceCdTrayReq)
DECLARE_IMPORT(15, sceCdStop)
DECLARE_IMPORT(24, sceCdReadClock)
DECLARE_IMPORT(28, sceCdStatus)
DECLARE_IMPORT(29, sceCdApplySCmd)
DECLARE_IMPORT(38, sceCdPause)
DECLARE_IMPORT(39, sceCdBreak)
DECLARE_IMPORT(56, sceCdStInit)
DECLARE_IMPORT(57, sceCdStRead)
DECLARE_IMPORT(58, sceCdStSeek)
DECLARE_IMPORT(59, sceCdStStart)
DECLARE_IMPORT(60, sceCdStStat)
DECLARE_IMPORT(61, sceCdStStop)
DECLARE_IMPORT(62, sceCdRead0)
DECLARE_IMPORT(67, sceCdStPause)
DECLARE_IMPORT(68, sceCdStResume)
DECLARE_IMPORT(75, sceCdMmode)
DECLARE_IMPORT(77, sceCdStSeekF)
DECLARE_IMPORT(79, sceCdReadDiskID)
DECLARE_IMPORT(80, sceCdReadGUID)
DECLARE_IMPORT(81, sceCdSetTimeout)
DECLARE_IMPORT(82, sceCdReadModelID)
DECLARE_IMPORT(83, sceCdReadDvdDualInfo)
DECLARE_IMPORT(84, sceCdLayerSearchFile)
END_IMPORT_TABLE
