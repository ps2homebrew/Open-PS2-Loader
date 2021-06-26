/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
*/

#include "cdvdfsv-internal.h"

typedef struct
{                      // size =0x124
    sceCdlFILE cdfile; // 0x000
    char name[256];    // 0x020
    void *dest;        // 0x120
} SearchFilePkt_t;

typedef struct
{                      // size =0x128
    sceCdlFILE cdfile; // 0x000
    u32 flag;          // 0x020
    char name[256];    // 0x024
    void *dest;        // 0x124
} SearchFilePkt2_t;

typedef struct
{                      // size =0x12c
    sceCdlFILE cdfile; // 0x000
    u32 flag;          // 0x020
    char name[256];    // 0x024
    void *dest;        // 0x124
    int layer;         // 0x128
} SearchFilePktl_t;

static SifRpcServerData_t cdsearchfile_rpcSD;
static u8 cdsearchfile_rpcbuf[304];

static void *cbrpc_cdsearchfile(int fno, void *buf, int size);

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
        pktsize = sizeof(sceCdlFILE) + sizeof(u32);
        r = sceCdSearchFile((sceCdlFILE *)buf, p);
        pkt2->flag = 0;
    } else {
        if (size > sizeof(SearchFilePkt2_t)) { // Search File: Called from Dual_layer Version
            ee_addr = (void *)pktl->dest;
            p = (char *)&pktl->name[0];
            pktsize = sizeof(sceCdlFILE) + sizeof(u32);
            r = sceCdLayerSearchFile((sceCdlFILE *)buf, p, pktl->layer);
            pktl->flag = 0;
        } else { // Search File: Called from Old Library
            ee_addr = (void *)pkt->dest;
            p = (char *)&pkt->name[0];
            pktsize = sizeof(sceCdlFILE);
            r = sceCdSearchFile((sceCdlFILE *)buf, p);
        }
    }

    sysmemSendEE(buf, ee_addr, pktsize);

    *(int *)buf = r;

    return buf;
}

void cdvdfsv_register_searchfile_rpc(SifRpcDataQueue_t *rpc_DQ)
{
    sceSifRegisterRpc(&cdsearchfile_rpcSD, 0x80000597, &cbrpc_cdsearchfile, cdsearchfile_rpcbuf, NULL, NULL, rpc_DQ);
}
