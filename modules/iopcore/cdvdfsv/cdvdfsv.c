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

typedef struct
{
    int func_ret;
    int cdvdfsv_ver;
    int cdvdman_ver;
    int debug_mode;
} cdvdinit_res_t;

typedef struct
{
    int result;
    cd_clock_t rtc;
} cdvdreadclock_res_t;

typedef struct
{
    int result;
    u32 param1;
    u32 param2;
} cdvdSCmd_res_t;

typedef struct
{
    u16 cmd;
    u16 in_size;
    void *in;
} cdapplySCmd_t;

typedef struct
{                     // size =0x124
    cd_file_t cdfile; // 0x000
    char name[256];   // 0x020
    void *dest;       // 0x120
} SearchFilePkt_t;

typedef struct
{                      // size =0x128
    cdl_file_t cdfile; // 0x000
    char name[256];    // 0x024
    void *dest;        // 0x124
} SearchFilePkt2_t;

typedef struct
{                      // size =0x12c
    cdl_file_t cdfile; // 0x000
    char name[256];    // 0x024
    void *dest;        // 0x124
    int layer;         // 0x128
} SearchFilePktl_t;

typedef struct
{
    u32 lsn;
    u32 sectors;
    void *buf;
    cd_read_mode_t mode;
    void *eeaddr1;
    void *eeaddr2;
} RpcCdvd_t;

typedef struct
{
    u32 lsn;
    u32 sectors;
    void *buf;
    int cmd;
    cd_read_mode_t mode;
    u32 pad;
} RpcCdvdStream_t;

typedef struct
{
    u32 bufmax;
    u32 bankmax;
    void *buf;
    u32 pad;
} RpcCdvdStInit_t;

typedef struct
{
    u32 lsn;      // sector location to start reading from
    u32 sectors;  // number of sectors to read
    void *buf;    // buffer address to read to ( bit0: 0=EE, 1=IOP )
} RpcCdvdchain_t; // (EE addresses must be on 64byte alignment)

typedef struct
{ // size = 144
    u32 b1len;
    u32 b2len;
    void *pdst1;
    void *pdst2;
    u8 buf1[64];
    u8 buf2[64];
} cdvdfsv_readee_t;

// internal prototypes
static void init_thread(void *args);
static void cdvdfsv_startrpcthreads(void);
static void cdvdfsv_rpc0_th(void *args);
static void cdvdfsv_rpc1_th(void *args);
static void cdvdfsv_rpc2_th(void *args);
static void *cbrpc_cdinit(int fno, void *buf, int size);
static void *cbrpc_cdvdScmds(int fno, void *buf, int size);
static void *cbrpc_cddiskready(int fno, void *buf, int size);
static void *cbrpc_cddiskready2(int fno, void *buf, int size);
static void *cbrpc_cdvdNcmds(int fno, void *buf, int size);
static void *cbrpc_S596(int fno, void *buf, int size);
static void *cbrpc_cdsearchfile(int fno, void *buf, int size);
static inline void rpcSCmd_cdreadclock(void *buf);
static inline void rpcSCmd_cdtrayreq(void *buf);
static inline void rpcSCmd_cdapplySCmd(void *buf);
static inline void rpcSCmd_cdreadGUID(void *buf);
static inline void rpcSCmd_cdsettimeout(void *buf);
static inline void rpcSCmd_cdreadModelID(void *buf);
static inline void rpcSCmd_cdreaddvddualinfo(void *buf);
static inline void rpcNCmd_cdreadDiskID(void *buf);
static inline void rpcNCmd_cdgetdisktype(void *buf);
static inline void cdvd_Stsubcmdcall(void *buf);
static inline void cdvdSt_read(void *buf);
static inline void cdvd_readchain(void *buf);
static inline void cdvd_readee(void *buf);
static inline void cdvd_readiopm(void *buf);
static void sysmemSendEE(void *buf, void *EE_addr, int size);
int sceCdChangeThreadPriority(int priority);

enum CDVDFSV_SCMD {
    CDVDFSV_SCMD_READCLOCK = 0x01,
    CDVDFSV_SCMD_WRITECLOCK,
    CDVDFSV_SCMD_DISKTYPE,
    CDVDFSV_SCMD_GETERROR,
    CDVDFSV_SCMD_TRAYREQ,
    CDVDFSV_SCMD_APPLYSCMD = 0x0B,
    CDVDFSV_SCMD_STATUS,
    CDVDFSV_SCMD_BREAK = 0x16,
    CDVDFSV_SCMD_CANCELPOFF = 0x1F,
    CDVDFSV_SCMD_BLUELEDCTRL,
    CDVDFSV_SCMD_POWEROFF,
    CDVDFSV_SCMD_MMODE,
    CDVDFSV_SCMD_THREADPRIO,
    CDVDFSV_SCMD_READGUID,
    CDVDFSV_SCMD_SETTIMEOUT,
    CDVDFSV_SCMD_READMODELID,
    CDVDFSV_SCMD_READDUALINFO
};

enum CDVDFSV_NCMD {
    CDVDFSV_NCMD_CDREAD = 1,
    CDVDFSV_NCMD_CDDAREAD,
    CDVDFSV_NCMD_DVDREAD,
    CDVDFSV_NCMD_GETTOC,
    CDVDFSV_NCMD_SEEK,
    CDVDFSV_NCMD_STANDBY,
    CDVDFSV_NCMD_STOP,
    CDVDFSV_NCMD_PAUSE,
    CDVDFSV_NCMD_STREAM,
    CDVDFSV_NCMD_CDDASTREAM,
    CDVDFSV_NCMD_APPLYNCMD = 0x0C,
    CDVDFSV_NCMD_IOPMREAD,
    CDVDFSV_NCMD_DISKRDY,
    CDVDFSV_NCMD_READCHAIN,
    CDVDFSV_NCMD_READDISKID = 0x11,
    CDVDFSV_NCMD_DISKTYPE = 0x17,

    CDVDFSV_NCMD_COUNT
};

enum CDVDFSV_ST_CMD {
    CDVDFSV_ST_START = 1,
    CDVDFSV_ST_READ,
    CDVDFSV_ST_STOP,
    CDVDFSV_ST_SEEK,
    CDVDFSV_ST_INIT,
    CDVDFSV_ST_STAT,
    CDVDFSV_ST_PAUSE,
    CDVDFSV_ST_RESUME,
    CDVDFSV_ST_SEEKF
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

#define CDVDFSV_BUF_SECTORS (CDVDMAN_FS_SECTORS + 1) //CDVDFSV will use CDVDFSV_BUF_SECTORS+1 sectors of space. Remember that the actual size of the buffer within CDVDMAN is CDVDMAN_FS_SECTORS+2.

static int rpc0_thread_id, rpc1_thread_id, rpc2_thread_id;

struct irx_export_table _exp_cdvdfsv;

//-------------------------------------------------------------------------
int _start(int argc, char *argv[])
{
    iop_thread_t thread_param;

    RegisterLibraryEntries(&_exp_cdvdfsv);

    thread_param.attr = TH_C;
    thread_param.option = 0;
    thread_param.thread = &init_thread;
    thread_param.stacksize = 0x800;
    thread_param.priority = 0x50;

    StartThread(CreateThread(&thread_param), NULL);

    return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
static void init_thread(void *args)
{
    sceSifInitRpc(0);

    cdvdfsv_buf = sceGetFsvRbuf();
    cdvdfsv_startrpcthreads();

    ExitDeleteThread();
}

//-------------------------------------------------------------------------
static void cdvdfsv_startrpcthreads(void)
{
    iop_thread_t thread_param;

    thread_param.attr = TH_C;
    thread_param.option = 0xABCD8001;
    thread_param.thread = (void *)cdvdfsv_rpc1_th;
    thread_param.stacksize = 0x1900;
    thread_param.priority = 0x51;

    rpc1_thread_id = CreateThread(&thread_param);
    StartThread(rpc1_thread_id, NULL);

    thread_param.attr = TH_C;
    thread_param.option = 0xABCD8002;
    thread_param.thread = (void *)cdvdfsv_rpc2_th;
    thread_param.stacksize = 0x1900;
    thread_param.priority = 0x51;

    rpc2_thread_id = CreateThread(&thread_param);
    StartThread(rpc2_thread_id, NULL);

    thread_param.attr = TH_C;
    thread_param.option = 0xABCD8000;
    thread_param.thread = (void *)cdvdfsv_rpc0_th;
    thread_param.stacksize = 0x800;
    thread_param.priority = 0x51;

    rpc0_thread_id = CreateThread(&thread_param);
    StartThread(rpc0_thread_id, NULL);
}

//-------------------------------------------------------------------------
static void cdvdfsv_rpc0_th(void *args)
{
    sceSifSetRpcQueue(&rpc0_DQ, GetThreadId());

    sceSifRegisterRpc(&S596_rpcSD, 0x80000596, &cbrpc_S596, S596_rpcbuf, NULL, NULL, &rpc0_DQ);
    sceSifRegisterRpc(&cddiskready2_rpcSD, 0x8000059c, &cbrpc_cddiskready2, cddiskready2_rpcbuf, NULL, NULL, &rpc0_DQ);

    sceSifRpcLoop(&rpc0_DQ);
}

//-------------------------------------------------------------------------
static void cdvdfsv_rpc1_th(void *args)
{
    // Starts cd Init rpc Server
    // Starts cd Disk ready rpc server
    // Starts SCMD rpc server;

    sceSifSetRpcQueue(&rpc1_DQ, GetThreadId());

    sceSifRegisterRpc(&cdinit_rpcSD, 0x80000592, &cbrpc_cdinit, cdinit_rpcbuf, NULL, NULL, &rpc1_DQ);
    sceSifRegisterRpc(&cdvdScmds_rpcSD, 0x80000593, &cbrpc_cdvdScmds, cdvdScmds_rpcbuf, NULL, NULL, &rpc1_DQ);
    sceSifRegisterRpc(&cddiskready_rpcSD, 0x8000059a, &cbrpc_cddiskready, cddiskready_rpcbuf, NULL, NULL, &rpc1_DQ);

    sceSifRpcLoop(&rpc1_DQ);
}

//-------------------------------------------------------------------------
static void cdvdfsv_rpc2_th(void *args)
{
    // Starts NCMD rpc server
    // Starts Search file rpc server

    sceSifSetRpcQueue(&rpc2_DQ, GetThreadId());

    sceSifRegisterRpc(&cdvdNcmds_rpcSD, 0x80000595, &cbrpc_cdvdNcmds, cdvdNcmds_rpcbuf, NULL, NULL, &rpc2_DQ);
    sceSifRegisterRpc(&cdsearchfile_rpcSD, 0x80000597, &cbrpc_cdsearchfile, cdsearchfile_rpcbuf, NULL, NULL, &rpc2_DQ);

    sceSifRpcLoop(&rpc2_DQ);
}

//-------------------------------------------------------------------------
static void *cbrpc_cdinit(int fno, void *buf, int size)
{ // CD Init RPC callback
    cdvdinit_res_t *r = (cdvdinit_res_t *)buf;

    r->func_ret = sceCdInit(*(int *)buf);

    r->cdvdfsv_ver = 0x223;
    r->cdvdman_ver = 0x223;
    //r->debug_mode = 0xff;

    return buf;
}

//-------------------------------------------------------------------------
static inline void rpcSCmd_cdreadclock(void *buf)
{ // CD Read Clock RPC SCMD

    cdvdreadclock_res_t *r = (cdvdreadclock_res_t *)buf;

    r->result = sceCdReadClock(&r->rtc);
}

//-------------------------------------------------------------------------
static inline void rpcSCmd_cdtrayreq(void *buf)
{ // CD Tray Req RPC SCMD

    cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

    r->result = sceCdTrayReq(*((int *)buf), &r->param1);
}

//--------------------------------------------------------------
static inline void rpcSCmd_cdapplySCmd(void *buf)
{ // CD ApplySCMD RPC SCMD

    cdapplySCmd_t *SCmd = (cdapplySCmd_t *)buf;

    sceCdApplySCmd(SCmd->cmd, SCmd->in, SCmd->in_size, buf);
}

//-------------------------------------------------------------------------
static inline void rpcSCmd_cdreadGUID(void *buf)
{ // CD Read GUID RPC SCMD

    cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

    r->result = sceCdReadGUID(&r->param1);
}

//-------------------------------------------------------------------------
static inline void rpcSCmd_cdsettimeout(void *buf)
{ // CD Set TimeOut RPC SCMD

    cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

    r->result = sceCdSetTimeout(*(int *)buf, r->param1);
}

//-------------------------------------------------------------------------
static inline void rpcSCmd_cdreadModelID(void *buf)
{ // CD Read Disk ID RPC SCMD

    cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

    r->result = sceCdReadModelID(&r->param1);
}

//-------------------------------------------------------------------------
static inline void rpcSCmd_cdreaddvddualinfo(void *buf)
{ // CD Read Dvd DualLayer Info RPC SCMD

    cdvdSCmd_res_t *r = (cdvdSCmd_res_t *)buf;

    r->result = sceCdReadDvdDualInfo((int *)&r->param1, &r->param2);
}

//-------------------------------------------------------------------------
static void *cbrpc_cdvdScmds(int fno, void *buf, int size)
{ // CD SCMD RPC callback

    switch (fno) {
        case CDVDFSV_SCMD_READCLOCK:
            rpcSCmd_cdreadclock(buf);
            break;
        case CDVDFSV_SCMD_DISKTYPE:
            *(int *)buf = sceCdGetDiskType();
            break;
        case CDVDFSV_SCMD_GETERROR:
            *(int *)buf = sceCdGetError();
            break;
        case CDVDFSV_SCMD_TRAYREQ:
            rpcSCmd_cdtrayreq(buf);
            break;
        case CDVDFSV_SCMD_APPLYSCMD:
            rpcSCmd_cdapplySCmd(buf);
            break;
        case CDVDFSV_SCMD_STATUS:
            *(int *)buf = sceCdStatus();
            break;
        case CDVDFSV_SCMD_BREAK:
            *(int *)buf = sceCdBreak();
            break;
        case CDVDFSV_SCMD_CANCELPOFF:
            *(int *)buf = 1;
            break;
        case CDVDFSV_SCMD_POWEROFF:
            *(int *)buf = sceCdPowerOff((int *)buf);
            break;
        case CDVDFSV_SCMD_MMODE:
            *(int *)buf = sceCdMmode(*(int *)buf);
            break;
        case CDVDFSV_SCMD_THREADPRIO:
            *(int *)buf = sceCdChangeThreadPriority(*(int *)buf);
            break;
        case CDVDFSV_SCMD_READGUID:
            rpcSCmd_cdreadGUID(buf);
            break;
        case CDVDFSV_SCMD_SETTIMEOUT:
            rpcSCmd_cdsettimeout(buf);
            break;
        case CDVDFSV_SCMD_READMODELID:
            rpcSCmd_cdreadModelID(buf);
            break;
        case CDVDFSV_SCMD_READDUALINFO:
            rpcSCmd_cdreaddvddualinfo(buf);
            break;
        default:
            *(int *)buf = 0;
            break;
    }

    return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cddiskready(int fno, void *buf, int size)
{ // CD Disk Ready RPC callback

    *(int *)buf = sceCdDiskReady((*(int *)buf == 0) ? 0 : 1);

    return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cddiskready2(int fno, void *buf, int size)
{ // CD Disk Ready2 RPC callback

    *(int *)buf = sceCdDiskReady(0);

    return buf;
}

//-------------------------------------------------------------------------
static inline void rpcNCmd_cdreadDiskID(void *buf)
{
    u8 *p = (u8 *)buf;

    mips_memset(p, 0, 10);
    *(int *)buf = sceCdReadDiskID(&p[4]);
}

//-------------------------------------------------------------------------
static inline void rpcNCmd_cdgetdisktype(void *buf)
{
    u8 *p = (u8 *)buf;
    *(int *)&p[4] = sceCdGetDiskType();
    *(int *)&p[0] = 1;
}

//-------------------------------------------------------------------------
static void *cbrpc_cdvdNcmds(int fno, void *buf, int size)
{ // CD NCMD RPC callback
    int sc_param;

    sceCdSC(CDSC_IO_SEMA, &fno);

    switch (fno) {
        case CDVDFSV_NCMD_CDREAD:
        case CDVDFSV_NCMD_CDDAREAD:
        case CDVDFSV_NCMD_DVDREAD:
            cdvd_readee(buf);
            break;
        case CDVDFSV_NCMD_GETTOC:
            *(int *)buf = 1;
            break;
        case CDVDFSV_NCMD_SEEK:
            *(int *)buf = sceCdSeek(*(u32 *)buf);
            break;
        case CDVDFSV_NCMD_STANDBY:
            *(int *)buf = sceCdStandby();
            break;
        case CDVDFSV_NCMD_STOP:
            *(int *)buf = sceCdStop();
            break;
        case CDVDFSV_NCMD_PAUSE:
            *(int *)buf = sceCdPause();
            break;
        case CDVDFSV_NCMD_STREAM:
            cdvd_Stsubcmdcall(buf);
            break;
        case CDVDFSV_NCMD_IOPMREAD:
            cdvd_readiopm(buf);
            break;
        case CDVDFSV_NCMD_DISKRDY:
            *(int *)buf = sceCdDiskReady(0);
            break;
        case CDVDFSV_NCMD_READCHAIN:
            cdvd_readchain(buf);
            break;
        case CDVDFSV_NCMD_READDISKID:
            rpcNCmd_cdreadDiskID(buf);
            break;
        case CDVDFSV_NCMD_DISKTYPE:
            rpcNCmd_cdgetdisktype(buf);
            break;
        default:
            *(int *)buf = 0;
            break;
    }

    sc_param = 0;
    sceCdSC(CDSC_IO_SEMA, &sc_param);

    return buf;
}

//--------------------------------------------------------------
static void *cbrpc_S596(int fno, void *buf, int size)
{
    int cdvdman_intr_ef, dummy, value;

    if (fno == 1) {
        cdvdman_intr_ef = sceCdSC(CDSC_GET_INTRFLAG, &dummy);
        ClearEventFlag(cdvdman_intr_ef, ~4);
        WaitEventFlag(cdvdman_intr_ef, 4, WEF_AND, NULL);
    }
    else if (fno == 0x0596) {
        //Terminate operations.
        //Shutdown OPL
        value = 0;
        sceCdSC(CDSC_OPL_SHUTDOWN, &value);
    }

    *(int *)buf = 1;
    return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cdsearchfile(int fno, void *buf, int size)
{ // CD Search File RPC callback

    int r;
    void *ee_addr;
    char *p;
    SearchFilePkt_t *pkt = (SearchFilePkt_t *)buf;
    SearchFilePkt2_t *pkt2 = (SearchFilePkt2_t *)buf;
    SearchFilePktl_t *pktl = (SearchFilePktl_t *)buf;
    int pktsize;

    if (size == sizeof(SearchFilePkt2_t)) {
        ee_addr = (void *)pkt2->dest; // Search File: Called from Not Dual_layer Version
        p = (void *)&pkt2->name[0];
        pktsize = sizeof(cdl_file_t);
        r = sceCdSearchFile((cd_file_t *)buf, p);
        pkt2->cdfile.flag = 0;
    } else {
        if (size > sizeof(SearchFilePkt2_t)) { // Search File: Called from Dual_layer Version
            ee_addr = (void *)pktl->dest;
            p = (char *)&pktl->name[0];
            pktsize = sizeof(cdl_file_t);
            r = sceCdLayerSearchFile((cdl_file_t *)buf, p, pktl->layer);
            pktl->cdfile.flag = 0;
        } else { // Search File: Called from Old Library
            ee_addr = (void *)pkt->dest;
            p = (char *)&pkt->name[0];
            pktsize = sizeof(cd_file_t);
            r = sceCdSearchFile((cd_file_t *)buf, p);
        }
    }

    sysmemSendEE(buf, ee_addr, pktsize);

    *(int *)buf = r;

    return buf;
}

//-------------------------------------------------------------------------
static inline void cdvd_Stsubcmdcall(void *buf)
{ // call a Stream Sub function (below) depending on stream cmd sent
    RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

    switch (St->cmd) {
        case CDVDFSV_ST_START:
            *(int *)buf = sceCdStStart(((RpcCdvdStream_t *)buf)->lsn, &cdvdfsv_Stmode);
            break;
        case CDVDFSV_ST_READ:
            cdvdSt_read(buf);
            break;
        case CDVDFSV_ST_STOP:
            *(int *)buf = sceCdStStop();
            break;
        case CDVDFSV_ST_SEEK:
            *(int *)buf = sceCdStSeek(((RpcCdvdStream_t *)buf)->lsn);
            break;
        case CDVDFSV_ST_INIT:
            *(int *)buf = sceCdStInit(((RpcCdvdStInit_t *)buf)->bufmax, ((RpcCdvdStInit_t *)buf)->bankmax, ((RpcCdvdStInit_t *)buf)->buf);
            break;
        case CDVDFSV_ST_STAT:
            *(int *)buf = sceCdStStat();
            break;
        case CDVDFSV_ST_PAUSE:
            *(int *)buf = sceCdStPause();
            break;
        case CDVDFSV_ST_RESUME:
            *(int *)buf = sceCdStResume();
            break;
        case CDVDFSV_ST_SEEKF:
            *(int *)buf = sceCdStSeekF(((RpcCdvdStream_t *)buf)->lsn);
            break;
        default:
            *(int *)buf = 0;
            break;
    };
}

//--------------------------------------------------------------
static void sysmemSendEE(void *buf, void *EE_addr, int size)
{
    SifDmaTransfer_t dmat;
    int oldstate, id;

    dmat.dest = (void *)EE_addr;
    dmat.size = size;
    dmat.src = (void *)buf;
    dmat.attr = 0;

    do {
        CpuSuspendIntr(&oldstate);
        id = sceSifSetDma(&dmat, 1);
        CpuResumeIntr(oldstate);
    } while (!id);

    while (sceSifDmaStat(id) >= 0)
        ;
}

//-------------------------------------------------------------------------
static inline void cdvdSt_read(void *buf)
{
    RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
    u32 err;
    int r, rpos, remaining;
    void *ee_addr;

    for (rpos = 0, ee_addr = St->buf, remaining = St->sectors; remaining > 0; ee_addr += r << 11, rpos += r, remaining -= r) {
        if ((r = sceCdStRead(remaining, (void *)((u32)ee_addr | 0x80000000), 0, &err)) < 1)
            break;
    }

    *(int *)buf = (rpos & 0xFFFF) | (err << 16);
}

//-------------------------------------------------------------------------
static inline void cdvd_readchain(void *buf)
{
    int i, fsverror;
    u32 nsectors, tsectors, lsn, addr, readpos;

    RpcCdvdchain_t *ch = (RpcCdvdchain_t *)buf;

    for (i = 0, readpos = 0; i < 64; i++, ch++) {

        if ((ch->lsn == -1) || (ch->sectors == -1) || ((u32)ch->buf == -1))
            break;

        lsn = ch->lsn;
        tsectors = ch->sectors;
        addr = (u32)ch->buf & 0xfffffffc;

        if ((u32)ch->buf & 1) { // IOP addr
            if (sceCdRead(lsn, tsectors, (void *)addr, NULL) == 0) {
                if (sceCdGetError() == CDVD_ERR_NO) {
                    fsverror = CDVD_ERR_READCFR;
                    sceCdSC(CDSC_SET_ERROR, &fsverror);
                }

                *(int *)buf = 0;
                return;
            }
            sceCdSync(0);

            readpos += tsectors * 2048;
        } else { // EE addr
            while (tsectors > 0) {
                nsectors = (tsectors > CDVDFSV_BUF_SECTORS) ? CDVDFSV_BUF_SECTORS : tsectors;

                if (sceCdRead(lsn, nsectors, cdvdfsv_buf, NULL) == 0) {
                    if (sceCdGetError() == CDVD_ERR_NO) {
                        fsverror = CDVD_ERR_READCF;
                        sceCdSC(CDSC_SET_ERROR, &fsverror);
                    }

                    *(int *)buf = 0;
                    return;
                }
                sceCdSync(0);
                sysmemSendEE(cdvdfsv_buf, (void *)addr, nsectors << 11);

                lsn += nsectors;
                tsectors -= nsectors;
                addr += nsectors << 11;
                readpos += nsectors << 11;
            }
        }

        //The pointer to the read position variable within EE RAM is stored at ((RpcCdvdchain_t *)buf)[65].sectors.
        sysmemSendEE(&readpos, (void *)((RpcCdvdchain_t *)buf)[65].sectors, sizeof(readpos));
    }
}

//--------------------------------------------------------------
static inline void cdvd_readee(void *buf)
{ // Read Disc data to EE mem buffer
    u8 curlsn_buf[16];
    u32 nbytes, nsectors, sectors_to_read, size_64b, size_64bb, bytesent, temp;
    int sector_size, flag_64b, fsverror;
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
    readee.pdst2 = eeaddr2_64b; // get the end address on a 64 bytes align
    readee.b2len = temp;        // get bytes remainder at end of 64 bytes align
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
                sysmemSendEE((void *)&readee, (void *)r->eeaddr1, sizeof(cdvdfsv_readee_t));

                *((u32 *)&curlsn_buf[0]) = nbytes;
                sysmemSendEE((void *)curlsn_buf, (void *)r->eeaddr2, 16);

                *(int *)buf = nbytes;
                return;
            }

            if (flag_64b == 0) { // not 64 bytes aligned buf
                if (sectors_to_read < CDVDFSV_BUF_SECTORS)
                    nsectors = sectors_to_read;
                else
                    nsectors = CDVDFSV_BUF_SECTORS - 1;
                temp = nsectors + 1;
            } else { // 64 bytes aligned buf
                if (sectors_to_read < (CDVDFSV_BUF_SECTORS + 1))
                    nsectors = sectors_to_read;
                else
                    nsectors = CDVDFSV_BUF_SECTORS;
                temp = nsectors;
            }

            if (sceCdRead(r->lsn, temp, (void *)fsvRbuf, NULL) == 0) {
                if (sceCdGetError() == CDVD_ERR_NO) {
                    fsverror = CDVD_ERR_READCF;
                    sceCdSC(CDSC_SET_ERROR, &fsverror);
                }

                *(int *)buf = bytesent;
                return;
            }
            sceCdSync(0);

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

            *((u32 *)&curlsn_buf[0]) = bytesent;
            sysmemSendEE((void *)curlsn_buf, (void *)r->eeaddr2, 16);

            sectors_to_read -= nsectors;
            r->lsn += nsectors;
            eeaddr_64b += size_64b;

        } while ((flag_64b) || (sectors_to_read));

        mips_memcpy((void *)readee.buf2, (void *)(fsvRbuf + size_64b - readee.b2len), readee.b2len);
    }

    *(int *)buf = bytesent;
}

static inline void cdvd_readiopm(void *buf)
{
    int r, fsverror;
    u32 readpos;

    r = sceCdRead(((RpcCdvd_t *)buf)->lsn, ((RpcCdvd_t *)buf)->sectors, ((RpcCdvd_t *)buf)->buf, NULL);
    while (sceCdSync(1)) {
        readpos = sceCdGetReadPos();
        sysmemSendEE(&readpos, ((RpcCdvd_t *)buf)->eeaddr2, sizeof(readpos));
        DelayThread(8000);
    }

    if (r == 0) {
        if (sceCdGetError() == CDVD_ERR_NO) {
            fsverror = CDVD_ERR_READCFR;
            sceCdSC(CDSC_SET_ERROR, &fsverror);
        }
    }
}

//-------------------------------------------------------------------------
int sceCdChangeThreadPriority(int priority)
{
    if ((u32)(priority - 9) < 0x73) {
        if (priority == 9)
            priority = 10;

        ChangeThreadPriority(rpc0_thread_id, priority);
        ChangeThreadPriority(rpc2_thread_id, priority);
        ChangeThreadPriority(rpc1_thread_id, priority);

        return 0;
    }

    return -403;
}
