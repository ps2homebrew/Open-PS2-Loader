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
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>

#include "cdvdman.h"
#include "smsutils.h"

#define MODNAME "cdvd_ee_driver"
IRX_ID(MODNAME, 2, 2);

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
} cdapplySCmd_t;

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

typedef void *(*rpcSCmd_fn_t)(int fno, void *buf, int size);
typedef void *(*rpcNCmd_fn_t)(int fno, void *buf, int size);
typedef void (*cdvd_Stsubcall_fn_t)(void *buf);

// internal prototypes
static void init_thread(void *args);
static void cdvdfsv_startrpcthreads(void);
static void cdvdfsv_rpc0_th(void *args);
static void cdvdfsv_rpc1_th(void *args);
static void cdvdfsv_rpc2_th(void *args);
static void *cbrpc_cdinit(int fno, void *buf, int size);
static void *cbrpc_cdvdScmds(int fno, void *buf, int size);
static void *rpcSCmd_cdapplySCmd(int fno, void *buf, int size);
static void *rpcSCmd_cdreadclock(int fno, void *buf, int size);
static void *rpcSCmd_cdgetdisktype(int fno, void *buf, int size);
static void *rpcSCmd_cdgeterror(int fno, void *buf, int size);
static void *rpcSCmd_cdtrayreq(int fno, void *buf, int size);
static void *rpcSCmd_cdstatus(int fno, void *buf, int size);
static void *rpcSCmd_cdabort(int fno, void *buf, int size);
static void *rpcSCmd_cdpoweroff(int fno, void *buf, int size);
static void *rpcSCmd_cdmmode(int fno, void *buf, int size);
static void *rpcSCmd_cdreadGUID(int fno, void *buf, int size);
static void *rpcSCmd_cdsettimeout(int fno, void *buf, int size);
static void *rpcSCmd_cdreadModelID(int fno, void *buf, int size);
static void *rpcSCmd_cdreaddvddualinfo(int fno, void *buf, int size);
static void *rpcSCmd_dummy(int fno, void *buf, int size);
static void *cbrpc_cddiskready(int fno, void *buf, int size);
static void *cbrpc_cddiskready2(int fno, void *buf, int size);
static void *cbrpc_cdvdNcmds(int fno, void *buf, int size);
static void *rpcNCmd_cdread(int fno, void *buf, int size);
static void *rpcNCmd_cdgettoc(int fno, void *buf, int size);
static void *rpcNCmd_cdseek(int fno, void *buf, int size);
static void *rpcNCmd_cdstandby(int fno, void *buf, int size);
static void *rpcNCmd_cdstop(int fno, void *buf, int size);
static void *rpcNCmd_cdpause(int fno, void *buf, int size);
static void *rpcNCmd_cdstream(int fno, void *buf, int size);
static void *rpcNCmd_iopmread(int fno, void *buf, int size);
static void *rpcNCmd_cddiskready(int fno, void *buf, int size);
static void *rpcNCmd_cdreadchain(int fno, void *buf, int size);
static void *rpcNCmd_cdreadDiskID(int fno, void *buf, int size);
static void *rpcNCmd_cdgetdisktype(int fno, void *buf, int size);
static void *rpcNCmd_dummy(int fno, void *buf, int size);
static void *cbrpc_S596(int fno, void *buf, int size);
static void *cbrpc_cdsearchfile(int fno, void *buf, int size);
static void cdvd_Stsubcmdcall(void *buf);
static void cdvdSt_dummy(void *buf);
static void cdvdSt_start(void *buf);
static void cdvdSt_read(void *buf);
static void cdvdSt_stop(void *buf);
static void cdvdSt_seek(void *buf);
static void cdvdSt_init(void *buf);
static void cdvdSt_stat(void *buf);
static void cdvdSt_pause(void *buf);
static void cdvdSt_resume(void *buf);
static void cdvdSt_seekF(void *buf);
static void cdvd_iopmread(void *buf);
static void cdvd_readchain(void *buf);
static void cdvd_readee(void *buf);

// rpcSCmd funcs array
static rpcSCmd_fn_t rpcSCmd_tab[40] = {
	&rpcSCmd_dummy,
	&rpcSCmd_cdreadclock,
	&rpcSCmd_dummy,		// cdwriteclock
	&rpcSCmd_cdgetdisktype,
	&rpcSCmd_cdgeterror,
	&rpcSCmd_cdtrayreq,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_cdapplySCmd,	// ApplySCmd
	&rpcSCmd_cdstatus,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_cdabort,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,
	&rpcSCmd_dummy,		// CancelPowerOff
	&rpcSCmd_dummy,		// BlueLedCtrl
	&rpcSCmd_cdpoweroff,
	&rpcSCmd_cdmmode,
	&rpcSCmd_dummy,		// SetThreadPriority
	&rpcSCmd_cdreadGUID,
	&rpcSCmd_cdsettimeout,
	&rpcSCmd_cdreadModelID,
	&rpcSCmd_cdreaddvddualinfo
};

// rpcNCmd funcs array
static rpcNCmd_fn_t rpcNCmd_tab[24] = {
	&rpcNCmd_dummy,
	&rpcNCmd_cdread,
	&rpcNCmd_cdread,		// Cdda Read
	&rpcNCmd_cdread,		// Dvd Read
	&rpcNCmd_cdgettoc,
	&rpcNCmd_cdseek,
	&rpcNCmd_cdstandby,
	&rpcNCmd_cdstop,
	&rpcNCmd_cdpause,
	&rpcNCmd_cdstream,
	&rpcNCmd_dummy,		// CDDA Stream
	&rpcNCmd_dummy,
	&rpcNCmd_dummy,		// Apply NCMD
	&rpcNCmd_iopmread,
	&rpcNCmd_cddiskready,
	&rpcNCmd_cdreadchain,
	&rpcNCmd_dummy,
	&rpcNCmd_cdreadDiskID,
	&rpcNCmd_dummy,
	&rpcNCmd_dummy,		// ???
	&rpcNCmd_dummy,
	&rpcNCmd_dummy,
	&rpcNCmd_dummy,
	&rpcNCmd_cdgetdisktype
};

// cdvdStsubcall funcs array
static cdvd_Stsubcall_fn_t cdvdStsubcall_tab[10] = {
	&cdvdSt_dummy,
	&cdvdSt_start,
	&cdvdSt_read,
	&cdvdSt_stop,
	&cdvdSt_seek,
	&cdvdSt_init,
	&cdvdSt_stat,
	&cdvdSt_pause,
	&cdvdSt_resume,
	&cdvdSt_seekF
};

static cd_read_mode_t cdvdfsv_Stmode;
static u8 *cdvdfsv_buf;

static SifRpcDataQueue_t rpc0_DQ;
static SifRpcDataQueue_t rpc1_DQ;
static SifRpcDataQueue_t rpc2_DQ;
static SifRpcServerData_t cdinit_rpcSD, cddiskready_rpcSD, cddiskready2_rpcSD, cdvdScmds_rpcSD;
static SifRpcServerData_t cdsearchfile_rpcSD, cdvdNcmds_rpcSD;
static SifRpcServerData_t S596_rpcSD;

static u8 cdinit_rpcbuf[16];
static u8 cddiskready_rpcbuf[16];
static u8 cddiskready2_rpcbuf[16];
static u8 cdvdScmds_rpcbuf[1024];
static u8 cdsearchfile_rpcbuf[304];
static u8 cdvdNcmds_rpcbuf[1024];
static u8 S596_rpcbuf[16];
static u8 curlsn_buf[16];

#define CDVDFSV_BUF_SECTORS		(CDVDMAN_FS_SECTORS+1)	//CDVDFSV will use CDVDFSV_BUF_SECTORS+1 sectors of space. Remember that the actual size of the buffer within CDVDMAN is CDVDMAN_FS_SECTORS+2.

static int init_thread_id;
static int rpc0_thread_id, rpc1_thread_id, rpc2_thread_id;

struct irx_export_table _exp_cdvdfsv;

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{
	iop_thread_t thread_param;

	RegisterLibraryEntries(&_exp_cdvdfsv);

	FlushDcache();
	CpuEnableIntr();

	thread_param.attr = TH_C;
	thread_param.option = 0;
	thread_param.thread = (void *)init_thread;
	thread_param.stacksize = 0x800;
	thread_param.priority = 0x50;

	init_thread_id = CreateThread(&thread_param);
	StartThread(init_thread_id, 0);

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
static void init_thread(void *args)
{
	if (!sceSifCheckInit())
		sceSifInit();

	sceSifInitRpc(0);

	cdvdfsv_buf = sceGetFsvRbuf();
	if (!cdvdfsv_buf)
		SleepThread();

	cdvdfsv_startrpcthreads();
}

//-------------------------------------------------------------------------
static void cdvdfsv_startrpcthreads(void)
{
	iop_thread_t thread_param;

	thread_param.attr = TH_C;
	thread_param.option = 0;
	thread_param.thread = (void *)cdvdfsv_rpc1_th;
	thread_param.stacksize = 0x1900;
	thread_param.priority = 0x51;

	rpc1_thread_id = CreateThread(&thread_param);
	StartThread(rpc1_thread_id, 0);

	thread_param.attr = TH_C;
	thread_param.option = 0;
	thread_param.thread = (void *)cdvdfsv_rpc2_th;
	thread_param.stacksize = 0x1900;
	thread_param.priority = 0x51;

	rpc2_thread_id = CreateThread(&thread_param);
	StartThread(rpc2_thread_id, 0);

	thread_param.attr = TH_C;
	thread_param.option = 0;
	thread_param.thread = (void *)cdvdfsv_rpc0_th;
	thread_param.stacksize = 0x800;
	thread_param.priority = 0x51;

	rpc0_thread_id = CreateThread(&thread_param);
	StartThread(rpc0_thread_id, 0);
}

//-------------------------------------------------------------------------
static void cdvdfsv_rpc0_th(void *args)
{
	sceSifSetRpcQueue(&rpc0_DQ, GetThreadId());

	sceSifRegisterRpc(&S596_rpcSD, 0x80000596, (void *)cbrpc_S596, S596_rpcbuf, NULL, NULL, &rpc0_DQ);
	sceSifRegisterRpc(&cddiskready2_rpcSD, 0x8000059c, (void *)cbrpc_cddiskready2, cddiskready2_rpcbuf, NULL, NULL, &rpc0_DQ);

	sceSifRpcLoop(&rpc0_DQ);
}

//-------------------------------------------------------------------------
static void cdvdfsv_rpc1_th(void *args)
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
static void cdvdfsv_rpc2_th(void *args)
{
	// Starts NCMD rpc server
	// Starts Search file rpc server

	sceSifSetRpcQueue(&rpc2_DQ, GetThreadId());

	sceSifRegisterRpc(&cdvdNcmds_rpcSD, 0x80000595, (void *)cbrpc_cdvdNcmds, cdvdNcmds_rpcbuf, NULL, NULL, &rpc2_DQ);
	sceSifRegisterRpc(&cdsearchfile_rpcSD, 0x80000597, (void *)cbrpc_cdsearchfile, cdsearchfile_rpcbuf, NULL, NULL, &rpc2_DQ);

	sceSifRpcLoop(&rpc2_DQ);
}

//-------------------------------------------------------------------------
static void *cbrpc_cdinit(int fno, void *buf, int size)
{	// CD Init RPC callback
	cdvdinit_res_t *r = (cdvdinit_res_t *)buf;

	r->func_ret = sceCdInit(*(int *)buf);

	r->cdvdfsv_ver = 0x223;
	r->cdvdman_ver = 0x223;
	//r->debug_mode = 0xff;

	return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cdvdScmds(int fno, void *buf, int size)
{	// CD SCMD RPC callback
	rpcSCmd_fn_t rpcSCmd_fn;

	if ((u32)(fno >= 40))
		return rpcSCmd_dummy(fno, buf, size);

	// Call SCMD RPC sub function
	rpcSCmd_fn = (void *)rpcSCmd_tab[fno];

	return rpcSCmd_fn(fno, buf, size);
}

//--------------------------------------------------------------
static void *rpcSCmd_cdapplySCmd(int fno, void *buf, int size)
{	// CD ApplySCMD RPC SCMD
	cdapplySCmd_t *SCmd = (cdapplySCmd_t *)buf;

	sceCdApplySCmd(SCmd->cmd, SCmd->in, SCmd->in_size, buf);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdreadclock(int fno, void *buf, int size)
{	// CD Read Clock RPC SCMD
	cdvdreadclock_res_t *r = (cdvdreadclock_res_t *)buf;

	r->result = sceCdReadClock(&r->rtc);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdgetdisktype(int fno, void *buf, int size)
{	// CD Get Disc Type RPC SCMD

	*(int *)buf = sceCdGetDiskType();
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdgeterror(int fno, void *buf, int size)
{	// CD Get Error RPC SCMD

	*(int *)buf = sceCdGetError();
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdtrayreq(int fno, void *buf, int size)
{	// CD Tray Req RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdTrayReq(*((int *)buf), &r->param1);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdstatus(int fno, void *buf, int size)
{	// CD Status RPC SCMD

	*(int *)buf = sceCdStatus();
	return buf;
}

//-------------------------------------------------------------------------
static  void *rpcSCmd_cdabort(int fno, void *buf, int size)
{	// CD Abort RPC SCMD

	*(int *)buf = sceCdBreak();
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdpoweroff(int fno, void *buf, int size)
{	// CD Power Off RPC SCMD

	*(int *)buf = sceCdPowerOff((int *)buf);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdmmode(int fno, void *buf, int size)
{	// CD Media Mode RPC SCMD

	*(int *)buf = sceCdMmode(*(int *)buf);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdreadGUID(int fno, void *buf, int size)
{	// CD Read GUID RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdReadGUID(&r->param1);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdsettimeout(int fno, void *buf, int size)
{	// CD Set TimeOut RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdSetTimeout(*(int *)buf, r->param1);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdreadModelID(int fno, void *buf, int size)
{	// CD Read Disk ID RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdReadModelID(&r->param1);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_cdreaddvddualinfo(int fno, void *buf, int size)
{	// CD Read Dvd DualLayer Info RPC SCMD

	cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

	r->result = sceCdReadDvdDualInfo((int *)&r->param1, &r->param2);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcSCmd_dummy(int fno, void *buf, int size)
{
	*(int *)buf = 0;
	return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cddiskready(int fno, void *buf, int size)
{	// CD Disk Ready RPC callback

	if (*(int *)buf == 0)
		*(int *)buf = sceCdDiskReady(0);
	else
		*(int *)buf = sceCdDiskReady(1);

	return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cddiskready2(int fno, void *buf, int size)
{	// CD Disk Ready2 RPC callback

	*(int *)buf = sceCdDiskReady(0);

	return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cdvdNcmds(int fno, void *buf, int size)
{	// CD NCMD RPC callback
	rpcNCmd_fn_t rpcNCmd_fn;
	void *result;
	int sc_param;

	if ((u32)(fno >= 24))
		return rpcNCmd_dummy(fno, buf, size);

	sceCdSC(0xFFFFFFF6, &fno);

	// Call NCMD RPC sub function
	rpcNCmd_fn = (void *)rpcNCmd_tab[fno];
	result=rpcNCmd_fn(fno, buf, size);

	sc_param=0;
	sceCdSC(0xFFFFFFF6, &sc_param);

	return result;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdread(int fno, void *buf, int size)
{	// CD Read RPC NCMD

	cdvd_readee(buf);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdgettoc(int fno, void *buf, int size)
{	// CD Get TOC RPC NCMD

	*(int *)buf = 1;
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdseek(int fno, void *buf, int size)
{	// CD Seek RPC NCMD

	*(int *)buf = sceCdSeek(*(u32 *)buf);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdstandby(int fno, void *buf, int size)
{	// CD Standby RPC NCMD

	*(int *)buf = sceCdStandby();
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdstop(int fno, void *buf, int size)
{	// CD Stop RPC NCMD

	*(int *)buf = sceCdStop();
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdpause(int fno, void *buf, int size)
{	// CD Pause RPC NCMD

	*(int *)buf = sceCdPause();
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdstream(int fno, void *buf, int size)
{	// CD Stream RPC NCMD

	cdvd_Stsubcmdcall(buf);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_iopmread(int fno, void *buf, int size)
{	// CD IOP Mem Read RPC NCMD

	cdvd_iopmread(buf);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cddiskready(int fno, void *buf, int size)
{	// CD Disk Ready RPC NCMD

	*(int *)buf = sceCdDiskReady(0);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdreadchain(int fno, void *buf, int size)
{	// CD ReadChain RPC NCMD

	cdvd_readchain(buf);
	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdreadDiskID(int fno, void *buf, int size)
{
	u8 *p = (u8 *)buf;

	mips_memset(p, 0, 10);
	*(int *)buf = sceCdReadDiskID(&p[4]);

	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_cdgetdisktype(int fno, void *buf, int size)
{
	u8 *p = (u8 *)buf;
	*(int *)&p[4] = sceCdGetDiskType();
	*(int *)&p[0] = 1;

	return buf;
}

//-------------------------------------------------------------------------
static void *rpcNCmd_dummy(int fno, void *buf, int size)
{
	*(int *)buf = 0;
	return buf;
}

//--------------------------------------------------------------
static void *cbrpc_S596(int fno, void *buf, int size)
{
	int cdvdman_intr_ef, dummy;

	if(fno==1){
		cdvdman_intr_ef=sceCdSC(0xFFFFFFF5, &dummy);
		ClearEventFlag(cdvdman_intr_ef, ~4);
		WaitEventFlag(cdvdman_intr_ef, 4, WEF_AND, NULL);
	}

	*(int*)buf=1;
	return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cdsearchfile(int fno, void *buf, int size)
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
static void cdvd_Stsubcmdcall(void *buf)
{	// call a Stream Sub function (below) depending on stream cmd sent
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
	cdvd_Stsubcall_fn_t cdvd_Stsubcall_fn;

	if ((u32)(St->cmd >= 10)) {
		cdvdSt_dummy(buf);
		return;
	}

	cdvd_Stsubcall_fn = cdvdStsubcall_tab[St->cmd];
	cdvd_Stsubcall_fn(buf);
}

//-------------------------------------------------------------------------
static void cdvdSt_dummy(void *buf)
{
	*(int *)buf = 0;
}

//-------------------------------------------------------------------------
static void cdvdSt_start(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

	*(int *)buf = sceCdStStart(St->lsn, &cdvdfsv_Stmode);
}

//--------------------------------------------------------------
static void sysmemSendEE(void *buf, void *EE_addr, int size)
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
static void cdvdSt_read(void *buf)
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
static void cdvdSt_stop(void *buf)
{
	*(int *)buf = sceCdStStop();
}

//-------------------------------------------------------------------------
static void cdvdSt_seek(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

	*(int *)buf = sceCdStSeek(St->lsn);
}

//-------------------------------------------------------------------------
static void cdvdSt_init(void *buf)
{
	RpcCdvdStInit_t *r = (RpcCdvdStInit_t *)buf;

	*(int *)buf = sceCdStInit(r->bufmax, r->bankmax, r->buf);
}

//-------------------------------------------------------------------------
static void cdvdSt_stat(void *buf)
{
	*(int *)buf = sceCdStStat();
}

//-------------------------------------------------------------------------
static void cdvdSt_pause(void *buf)
{
	*(int *)buf = sceCdStPause();
}

//-------------------------------------------------------------------------
static void cdvdSt_resume(void *buf)
{
	*(int *)buf = sceCdStResume();
}

//-------------------------------------------------------------------------
static void cdvdSt_seekF(void *buf)
{
	RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

	*(int *)buf = sceCdStSeekF(St->lsn);
}

//-------------------------------------------------------------------------
static void cdvd_iopmread(void *buf)
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
static void cdvd_readee(void *buf)
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
int sceCdChangeThreadPriority(int priority)
{
	iop_thread_info_t th_info;

	if ((u32)(priority - 9) < 0x73) {
		if (priority == 9)
			priority = 10;

		ReferThreadStatus(0, &th_info);

		ChangeThreadPriority(0, 0x08);
		ChangeThreadPriority(init_thread_id, priority-1);
		ChangeThreadPriority(rpc0_thread_id, priority);
		ChangeThreadPriority(rpc2_thread_id, priority);
		ChangeThreadPriority(rpc1_thread_id, priority);

		return 0;
	}

	return -403;
}
