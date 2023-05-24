/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
*/

#include "cdvdfsv-internal.h"

typedef struct
{
    int result;
    sceCdCLOCK rtc;
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

static SifRpcServerData_t cdvdScmds_rpcSD;
static u8 cdvdScmds_rpcbuf[1024];

static void *cbrpc_cdvdScmds(int fno, void *buf, int size);
static inline void rpcSCmd_cdreadclock(void *buf);
static inline void rpcSCmd_cdtrayreq(void *buf);
static inline void rpcSCmd_cdapplySCmd(void *buf);
static inline void rpcSCmd_cdreadGUID(void *buf);
static inline void rpcSCmd_cdsettimeout(void *buf);
static inline void rpcSCmd_cdreadModelID(void *buf);
static inline void rpcSCmd_cdreaddvddualinfo(void *buf);

enum CD_SCMD_CMDS {
    CD_SCMD_READCLOCK = 0x01,
    CD_SCMD_WRITECLOCK,
    CD_SCMD_GETDISKTYPE,
    CD_SCMD_GETERROR,
    CD_SCMD_TRAYREQ,
    CD_SCMD_SCMD = 0x0B,
    CD_SCMD_STATUS,
    CD_SCMD_BREAK = 0x16,
    CD_SCMD_CANCELPOWEROFF = 0x1F,
    CD_SCMD_BLUELEDCTRL,
    CD_SCMD_POWEROFF,
    CD_SCMD_MMODE,
    CD_SCMD_SETTHREADPRI,
    CD_SCMD_READGUID,
    CD_SCMD_SETTIMEOUT,
    CD_SCMD_READMODELID,
    CD_SCMD_READDUALINFO
};

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

    r->result = sceCdReadGUID((u64 *)&r->param1);
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
        case CD_SCMD_READCLOCK:
            rpcSCmd_cdreadclock(buf);
            break;
        case CD_SCMD_GETDISKTYPE:
            *(int *)buf = sceCdGetDiskType();
            break;
        case CD_SCMD_GETERROR:
            *(int *)buf = sceCdGetError();
            break;
        case CD_SCMD_TRAYREQ:
            rpcSCmd_cdtrayreq(buf);
            break;
        case CD_SCMD_SCMD:
            rpcSCmd_cdapplySCmd(buf);
            break;
        case CD_SCMD_STATUS:
            *(int *)buf = sceCdStatus();
            break;
        case CD_SCMD_BREAK:
            *(int *)buf = sceCdBreak();
            break;
        case CD_SCMD_CANCELPOWEROFF:
            *(int *)buf = 1;
            break;
        case CD_SCMD_POWEROFF:
            *(int *)buf = sceCdPowerOff((u32 *)buf);
            break;
        case CD_SCMD_MMODE:
            *(int *)buf = sceCdMmode(*(int *)buf);
            break;
        case CD_SCMD_SETTHREADPRI:
            *(int *)buf = sceCdChangeThreadPriority(*(int *)buf);
            break;
        case CD_SCMD_READGUID:
            rpcSCmd_cdreadGUID(buf);
            break;
        case CD_SCMD_SETTIMEOUT:
            rpcSCmd_cdsettimeout(buf);
            break;
        case CD_SCMD_READMODELID:
            rpcSCmd_cdreadModelID(buf);
            break;
        case CD_SCMD_READDUALINFO:
            rpcSCmd_cdreaddvddualinfo(buf);
            break;
        default:
            DPRINTF("cbrpc_cdvdScmds unknown rpc fno=%x buf=%x size=%x\n", fno, (int)buf, size);
            *(int *)buf = 0;
            break;
    }

    return buf;
}

void cdvdfsv_register_scmd_rpc(SifRpcDataQueue_t *rpc_DQ)
{
    sceSifRegisterRpc(&cdvdScmds_rpcSD, 0x80000593, &cbrpc_cdvdScmds, cdvdScmds_rpcbuf, NULL, NULL, rpc_DQ);
}
