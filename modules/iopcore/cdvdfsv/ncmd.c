/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
*/

#include "cdvdfsv-internal.h"

typedef struct
{
    u32 lsn;
    u32 sectors;
    void *buf;
    sceCdRMode mode;
    void *eeaddr1;
    void *eeaddr2;
} RpcCdvd_t;

typedef struct
{
    u32 lsn;
    u32 sectors;
    void *buf;
    int cmd;
    sceCdRMode mode;
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

static sceCdRMode cdvdfsv_Stmode;

static SifRpcServerData_t cdvdNcmds_rpcSD;
static u8 cdvdNcmds_rpcbuf[1024];

static void *cbrpc_cdvdNcmds(int fno, void *buf, int size);
static inline void cdvd_readee(void *buf);
static inline void cdvdSt_read(void *buf);
static inline void cdvd_Stsubcmdcall(void *buf);
static inline void cdvd_readiopm(void *buf);
static inline void cdvd_readchain(void *buf);
static inline void rpcNCmd_cdreadDiskID(void *buf);
static inline void rpcNCmd_cdgetdisktype(void *buf);

enum CD_NCMD_CMDS {
    CD_NCMD_READ = 1,
    CD_NCMD_CDDAREAD,
    CD_NCMD_DVDREAD,
    CD_NCMD_GETTOC,
    CD_NCMD_SEEK,
    CD_NCMD_STANDBY,
    CD_NCMD_STOP,
    CD_NCMD_PAUSE,
    CD_NCMD_STREAM,
    CD_NCMD_CDDASTREAM,
    CD_NCMD_NCMD = 0x0C,
    CD_NCMD_READIOPMEM,
    CD_NCMD_DISKREADY,
    CD_NCMD_READCHAIN,
    CD_NCMD_READDISKID = 0x11,
    CD_NCMD_DISKTYPE = 0x17,

    CD_NCMD_COUNT
};

enum CDVD_ST_CMDS {
    CDVD_ST_CMD_START = 1,
    CDVD_ST_CMD_READ,
    CDVD_ST_CMD_STOP,
    CDVD_ST_CMD_SEEK,
    CDVD_ST_CMD_INIT,
    CDVD_ST_CMD_STAT,
    CDVD_ST_CMD_PAUSE,
    CDVD_ST_CMD_RESUME,
    CDVD_ST_CMD_SEEKF
};

//--------------------------------------------------------------
static inline void cdvd_readee(void *buf)
{ // Read Disc data to EE mem buffer
    u8 curlsn_buf[16];
    u32 nbytes, nsectors, sectors_to_read, size_64b, size_64bb, bytesent, temp;
    u16 sector_size;
    int flag_64b, fsverror;
    void *fsvRbuf = (void *)cdvdfsv_buf;
    void *eeaddr_64b, *eeaddr2_64b;
    cdvdfsv_readee_t readee;
    RpcCdvd_t *r = (RpcCdvd_t *)buf;

    if (r->sectors == 0) {
        *(int *)buf = 0;
        return;
    }

    sector_size = 2048;

    if (r->mode.datapattern == SCECdSecS2328)
        sector_size = 2328;
    if (r->mode.datapattern == SCECdSecS2340)
        sector_size = 2340;


    r->eeaddr1 = (void *)((u32)r->eeaddr1 & 0x1fffffff);
    r->eeaddr2 = (void *)((u32)r->eeaddr2 & 0x1fffffff);
    r->buf = (void *)((u32)r->buf & 0x1fffffff);

    sceCdDiskReady(0);

    sectors_to_read = r->sectors;
    bytesent = 0;

    memset((void *)curlsn_buf, 0, 16);

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
            if ((sectors_to_read == 0) || (sceCdGetError() == SCECdErABRT)) {
                sysmemSendEE((void *)&readee, (void *)r->eeaddr1, sizeof(cdvdfsv_readee_t));

                *((u32 *)&curlsn_buf[0]) = nbytes;
                sysmemSendEE((void *)curlsn_buf, (void *)r->eeaddr2, 16);

                *(int *)buf = nbytes;
                return;
            }

            if (flag_64b == 0) { // not 64 bytes aligned buf
                // The data of the last sector of the chunk will be used to correct buffer alignment.
                if (sectors_to_read < CDVDMAN_FS_SECTORS - 1)
                    nsectors = sectors_to_read;
                else
                    nsectors = CDVDMAN_FS_SECTORS - 1;
                temp = nsectors + 1;
            } else { // 64 bytes aligned buf
                if (sectors_to_read < CDVDMAN_FS_SECTORS)
                    nsectors = sectors_to_read;
                else
                    nsectors = CDVDMAN_FS_SECTORS;
                temp = nsectors;
            }

            if (sceCdRead(r->lsn, temp, (void *)fsvRbuf, NULL) == 0) {
                if (sceCdGetError() == SCECdErNO) {
                    fsverror = SCECdErREADCF;
                    sceCdSC(CDSC_SET_ERROR, &fsverror);
                }

                *(int *)buf = bytesent;
                return;
            }
            sceCdSync(0);

            size_64b = nsectors * sector_size;
            size_64bb = size_64b;

            if (!flag_64b) {
                if (sectors_to_read == r->sectors) // check that was the first read. Data read will be skewed by readee.b1len bytes into the adjacent sector.
                    memcpy((void *)readee.buf1, (void *)fsvRbuf, readee.b1len);

                if ((sectors_to_read == nsectors) && (readee.b1len)) // For the last sector read.
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

        // At the very last pass, copy readee.b2len bytes from the last sector, to complete the alignment correction.
        memcpy((void *)readee.buf2, (void *)(fsvRbuf + size_64b - readee.b2len), readee.b2len);
    }

    *(int *)buf = bytesent;
}

//-------------------------------------------------------------------------
static inline void cdvdSt_read(void *buf)
{
    RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
    u32 err;
    int r, rpos, remaining;
    void *ee_addr;

    for (rpos = 0, ee_addr = St->buf, remaining = St->sectors; remaining > 0; ee_addr += r * 2048, rpos += r, remaining -= r) {
        if ((r = sceCdStRead(remaining, (void *)((u32)ee_addr | 0x80000000), 0, &err)) < 1)
            break;
    }

    *(int *)buf = (rpos & 0xFFFF) | (err << 16);
}

//-------------------------------------------------------------------------
static inline void cdvd_Stsubcmdcall(void *buf)
{ // call a Stream Sub function (below) depending on stream cmd sent
    RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;

    switch (St->cmd) {
        case CDVD_ST_CMD_START:
            *(int *)buf = sceCdStStart(((RpcCdvdStream_t *)buf)->lsn, &cdvdfsv_Stmode);
            break;
        case CDVD_ST_CMD_READ:
            cdvdSt_read(buf);
            break;
        case CDVD_ST_CMD_STOP:
            *(int *)buf = sceCdStStop();
            break;
        case CDVD_ST_CMD_SEEK:
            *(int *)buf = sceCdStSeek(((RpcCdvdStream_t *)buf)->lsn);
            break;
        case CDVD_ST_CMD_INIT:
            *(int *)buf = sceCdStInit(((RpcCdvdStInit_t *)buf)->bufmax, ((RpcCdvdStInit_t *)buf)->bankmax, ((RpcCdvdStInit_t *)buf)->buf);
            break;
        case CDVD_ST_CMD_STAT:
            *(int *)buf = sceCdStStat();
            break;
        case CDVD_ST_CMD_PAUSE:
            *(int *)buf = sceCdStPause();
            break;
        case CDVD_ST_CMD_RESUME:
            *(int *)buf = sceCdStResume();
            break;
        case CDVD_ST_CMD_SEEKF:
            *(int *)buf = sceCdStSeekF(((RpcCdvdStream_t *)buf)->lsn);
            break;
        default:
            *(int *)buf = 0;
            break;
    };
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
        if (sceCdGetError() == SCECdErNO) {
            fsverror = SCECdErREADCFR;
            sceCdSC(CDSC_SET_ERROR, &fsverror);
        }
    }
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
                if (sceCdGetError() == SCECdErNO) {
                    fsverror = SCECdErREADCFR;
                    sceCdSC(CDSC_SET_ERROR, &fsverror);
                }

                *(int *)buf = 0;
                return;
            }
            sceCdSync(0);

            readpos += tsectors * 2048;
        } else { // EE addr
            while (tsectors > 0) {
                nsectors = (tsectors > CDVDMAN_FS_SECTORS) ? CDVDMAN_FS_SECTORS : tsectors;

                if (sceCdRead(lsn, nsectors, cdvdfsv_buf, NULL) == 0) {
                    if (sceCdGetError() == SCECdErNO) {
                        fsverror = SCECdErREADCF;
                        sceCdSC(CDSC_SET_ERROR, &fsverror);
                    }

                    *(int *)buf = 0;
                    return;
                }
                sceCdSync(0);
                sysmemSendEE(cdvdfsv_buf, (void *)addr, nsectors * 2048);

                lsn += nsectors;
                tsectors -= nsectors;
                addr += nsectors * 2048;
                readpos += nsectors * 2048;
            }
        }

        // The pointer to the read position variable within EE RAM is stored at ((RpcCdvdchain_t *)buf)[65].sectors.
        sysmemSendEE(&readpos, (void *)((RpcCdvdchain_t *)buf)[65].sectors, sizeof(readpos));
    }
}

//-------------------------------------------------------------------------
static inline void rpcNCmd_cdreadDiskID(void *buf)
{
    u8 *p = (u8 *)buf;

    memset(p, 0, 10);
    *(int *)buf = sceCdReadDiskID((unsigned int *)&p[4]);
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
        case CD_NCMD_READ:
        case CD_NCMD_CDDAREAD:
        case CD_NCMD_DVDREAD:
            cdvd_readee(buf);
            break;
        case CD_NCMD_GETTOC:
            u32 eeaddr = *(u32 *)buf;
            DPRINTF("cbrpc_cdvdNcmds GetToc eeaddr=%08x\n", (int)eeaddr);
            char toc[2064];
            memset(toc, 0, 2064);
            int result = sceCdGetToc((u8 *)toc);
            *(int *)buf = result;
            if (result)
                sysmemSendEE(toc, (void *)eeaddr, 2064);
            break;
        case CD_NCMD_SEEK:
            *(int *)buf = sceCdSeek(*(u32 *)buf);
            break;
        case CD_NCMD_STANDBY:
            *(int *)buf = sceCdStandby();
            break;
        case CD_NCMD_STOP:
            *(int *)buf = sceCdStop();
            break;
        case CD_NCMD_PAUSE:
            *(int *)buf = sceCdPause();
            break;
        case CD_NCMD_STREAM:
            cdvd_Stsubcmdcall(buf);
            break;
        case CD_NCMD_READIOPMEM:
            cdvd_readiopm(buf);
            break;
        case CD_NCMD_DISKREADY:
            *(int *)buf = sceCdDiskReady(0);
            break;
        case CD_NCMD_READCHAIN:
            cdvd_readchain(buf);
            break;
        case CD_NCMD_READDISKID:
            rpcNCmd_cdreadDiskID(buf);
            break;
        case CD_NCMD_DISKTYPE:
            rpcNCmd_cdgetdisktype(buf);
            break;
        default:
            DPRINTF("cbrpc_cdvdNcmds unknown rpc fno=%x buf=%x size=%x\n", fno, (int)buf, size);
            *(int *)buf = 0;
            break;
    }

    sc_param = 0;
    sceCdSC(CDSC_IO_SEMA, &sc_param);

    return buf;
}

void cdvdfsv_register_ncmd_rpc(SifRpcDataQueue_t *rpc_DQ)
{
    sceSifRegisterRpc(&cdvdNcmds_rpcSD, 0x80000595, &cbrpc_cdvdNcmds, cdvdNcmds_rpcbuf, NULL, NULL, rpc_DQ);
}
