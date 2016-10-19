#include <errno.h>
#include <loadcore.h>
#include <sifcmd.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>

#include <irx.h>

#include "nbns.h"

IRX_ID("NetBIOS_Name_Service_resolver", 1, 1);

static SifRpcDataQueue_t SifQueueData;
static SifRpcServerData_t SifServerData;
static int RpcThreadID;
static unsigned char SifServerRxBuffer[64];
static unsigned char SifServerTxBuffer[32];

extern struct irx_export_table _exp_nbnsman;

static void *SifRpc_handler(int fno, void *buffer, int nbytes)
{
    switch (fno) {
        case NBNS_RPC_ID_FIND_NAME:
            ((struct nbnsFindNameResult *)SifServerTxBuffer)->result = nbnsFindName(buffer, ((struct nbnsFindNameResult *)SifServerTxBuffer)->address);
            break;
        default:
            *(int *)SifServerTxBuffer = -ENXIO;
    }

    return SifServerTxBuffer;
}

static void RpcThread(void *arg)
{
    sceSifSetRpcQueue(&SifQueueData, GetThreadId());
    sceSifRegisterRpc(&SifServerData, 0x00001B13, &SifRpc_handler, SifServerRxBuffer, NULL, NULL, &SifQueueData);
    sceSifRpcLoop(&SifQueueData);
}

int _start(int argc, char *argv[])
{
    int result;
    iop_thread_t thread;

    printf("NBNS resolver start\n");

    if (RegisterLibraryEntries(&_exp_nbnsman) == 0) {
        nbnsInit();

        thread.attr = TH_C;
        thread.option = 0x0B0B0001;
        thread.thread = &RpcThread;
        thread.priority = 0x20;
        thread.stacksize = 0x800;
        if ((RpcThreadID = CreateThread(&thread)) > 0) {
            StartThread(RpcThreadID, NULL);
            result = 0;
        } else {
            result = RpcThreadID;
            ReleaseLibraryEntries(&_exp_nbnsman);
        }
    } else {
        result = -1;
    }

    return (result == 0 ? MODULE_RESIDENT_END : MODULE_NO_RESIDENT_END);
}

int _exit(int argc, char *argv[])
{
    nbnsDeinit();
    ReleaseLibraryEntries(&_exp_nbnsman);
    return MODULE_NO_RESIDENT_END;
}
