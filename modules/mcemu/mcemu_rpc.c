/*
  Copyright 2006-2008, Romz
  Copyright 2010, Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "mcemu.h"

/* PS2 Memory Card cluster size */
/* do NOT change this value unless you are absolutely sure what you are doing */
#define MC2_CLUSTER_SIZE 0x400

/* size of memory to allocate for FastIO support */
#define FIO_ALLOC_SIZE (MC2_CLUSTER_SIZE + LIBMC_RPC_BUFFER_SIZE + sizeof(SifRpcClientData_t))

/* Replacement for MCMAN's library function #62 */
int hookMcman62()
{
    register char *ptr;

    /* checking if the memory block had been allocated */
    if (pFastBuf == NULL) {
        /* allocating memory for Fast I/O buffer, RPC buffer and RPC client data structure */
        ptr = (char *)_SysAlloc((FIO_ALLOC_SIZE + 0xFF) & ~(u64)0xFF);
        if (ptr == NULL) {
            DPRINTF("Not enough memory for FastIO support.\n");
            return 0;
        }

        /* initializing buffer pointers */
        pFastBuf = ptr;
        pFastRpcBuf = &ptr[MC2_CLUSTER_SIZE];
        pClientData = (SifRpcClientData_t *)&ptr[MC2_CLUSTER_SIZE + LIBMC_RPC_BUFFER_SIZE];
    } else
        ptr = NULL;

    /* binding to EE libmc's RPC server */
    if (SifBindRpc(pClientData, LIBMC_RPCNO, 0) < 0) {
        if (ptr != NULL) {
            /* freeing memory on RPC bind error */
            pFastBuf = NULL;
            _SysFree(ptr);
        }

        DPRINTF("libmc RPC bind error.\n");
        return 0;
    }

    return 1;
}

/* Reads file to EE memory for sceMcReadFast() support */
int hookMcman63(int fd, u32 eeaddr, int nbyte)
{
    int oldstate;
    SifDmaTransfer_t sdd;
    register int rlen;
    register int rval;
    register int size;
    register u32 id __attribute__((unused));
    register char *rpcbuf, *fiobuf;
    SifRpcClientData_t *cldata;

    // DPRINTF("sceMcReadFast(%d, 0x%X, 0x%X)\n", fd, eeaddr, nbyte);

    cldata = pClientData;
    rpcbuf = (char *)pFastRpcBuf;
    fiobuf = (char *)pFastBuf;

    for (rlen = nbyte; rlen > 0; rlen -= size) {
        size = (rlen > MC2_CLUSTER_SIZE) ? MC2_CLUSTER_SIZE : rlen;

        /* reading file with MCMAN's sceMcRead() call */
        rval = pMcRead(fd, fiobuf, size);
        if (rval < 0)
            return rval;

        /* preparing to transfer data to libmc */
        sdd.dest = (void *)((u32)fiobuf);
        sdd.src = (void *)eeaddr;
        sdd.size = MC2_CLUSTER_SIZE;
        sdd.attr = 0;

        *(int *)(&rpcbuf[0xC]) = size;

        /* sending data to EE */
        CpuSuspendIntr(&oldstate);
        id = sceSifSetDma(&sdd, 1);
        CpuResumeIntr(oldstate);

        /* informing libmc on new data */
        sceSifCallRpc(cldata, 2, 0, rpcbuf, LIBMC_RPC_BUFFER_SIZE, rpcbuf, LIBMC_RPC_BUFFER_SIZE, NULL, NULL);
    }

    return 0;
}

/* Write file from EE memory for sceMcWriteFast() support */
int hookMcman68(int fd, u32 eeaddr, int nbyte)
{
    SifRpcReceiveData_t od;
    register int rval;
    register int size;
    register int wlen;
    register u32 ea;
    register char *fiobuf;

    // DPRINTF("sceMcWriteFast(%d, 0x%X, 0x%X)\n", fd, eeaddr, nbyte);

    fiobuf = (char *)pFastBuf;

    ea = eeaddr;
    wlen = nbyte;

    /* check for EE address alignment (64 bytes) */
    rval = ea & 0x3F;
    if (rval) {
        /* getting unaligned data from EE side */
        SifRpcGetOtherData(&od, (void *)(ea & ~(u32)0x3F), fiobuf, MC2_CLUSTER_SIZE, 0);

        size = MC2_CLUSTER_SIZE - rval;
        if (wlen < size)
            size = wlen;

        /* writing unaligned data to a file */
        rval = pMcWrite(fd, &fiobuf[rval], size);
        if (rval < 0) {
            DPRINTF("sceMcWrite error %d\n", rval);
            return rval;
        }

        wlen -= size;
        ea += size;
    }

    for (; wlen > 0; wlen -= size, ea += size) {
        size = (wlen > MC2_CLUSTER_SIZE) ? MC2_CLUSTER_SIZE : wlen;

        /* receiving data from EE */
        SifRpcGetOtherData(&od, (void *)ea, fiobuf, MC2_CLUSTER_SIZE, 0);

        /* writing data to a file */
        rval = pMcWrite(fd, fiobuf, size);
        if (rval < 0) {
            DPRINTF("sceMcWrite error %d\n", rval);
            return rval;
        }
    }

    return 0;
}
