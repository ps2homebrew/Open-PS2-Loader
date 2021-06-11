#include <errno.h>
#include <intrman.h>
#include <loadcore.h>
#include <sifcmd.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>

#include <irx.h>

#include "httpclient.h"

IRX_ID("HTTP_Client", 1, 1);

static SifRpcDataQueue_t SifQueueData;
static SifRpcServerData_t SifServerData;
static int RpcThreadID;
static unsigned char SifServerRxBuffer[256];
static unsigned char SifServerTxBuffer[16];
static unsigned char DmaBuffer[512];

extern struct irx_export_table _exp_httpc;

static void *SifRpc_handler(int fno, void *buffer, int nbytes)
{
    SifDmaTransfer_t dmat;
    int OldState;

    switch (fno) {
        case HTTP_CLIENT_CMD_CONN_ESTAB:
            *(int *)SifServerTxBuffer = HttpEstabConnection(((struct HttpClientConnEstabArgs *)buffer)->server, ((struct HttpClientConnEstabArgs *)buffer)->port);
            break;
        case HTTP_CLIENT_CMD_CONN_CLOSE:
            HttpCloseConnection(((struct HttpClientConnCloseArgs *)buffer)->socket);
            break;
        case HTTP_CLIENT_CMD_SEND_GET_REQ:
            if (((struct HttpClientSendGetArgs *)buffer)->out_len > sizeof(DmaBuffer)) {
                printf("HttpClient: truncating output.\n");
                ((struct HttpClientSendGetArgs *)buffer)->out_len = sizeof(DmaBuffer);
            }

            ((struct HttpClientSendGetResult *)SifServerTxBuffer)->result = HttpSendGetRequest(((struct HttpClientSendGetArgs *)buffer)->socket, ((struct HttpClientSendGetArgs *)buffer)->UserAgent, ((struct HttpClientSendGetArgs *)buffer)->host, &((struct HttpClientSendGetArgs *)buffer)->mode, ((struct HttpClientSendGetArgs *)buffer)->hasMtime ? ((struct HttpClientSendGetArgs *)buffer)->mtime : NULL, ((struct HttpClientSendGetArgs *)buffer)->uri, (char *)DmaBuffer, &((struct HttpClientSendGetArgs *)buffer)->out_len);
            ((struct HttpClientSendGetResult *)SifServerTxBuffer)->mode = ((struct HttpClientSendGetArgs *)buffer)->mode;
            ((struct HttpClientSendGetResult *)SifServerTxBuffer)->out_len = ((struct HttpClientSendGetArgs *)buffer)->out_len;

            dmat.src = DmaBuffer;
            dmat.dest = ((struct HttpClientSendGetArgs *)buffer)->output;
            dmat.size = (((struct HttpClientSendGetArgs *)buffer)->out_len + 0xF) & ~0xF;
            dmat.attr = 0;

            CpuSuspendIntr(&OldState);
            while (sceSifSetDma(&dmat, 1) == 0)
                ;
            CpuResumeIntr(OldState);
            break;
        default:
            *(int *)SifServerTxBuffer = -ENXIO;
    }

    return SifServerTxBuffer;
}

static void RpcThread(void *arg)
{
    sceSifSetRpcQueue(&SifQueueData, GetThreadId());
    sceSifRegisterRpc(&SifServerData, 0x00001B14, &SifRpc_handler, SifServerRxBuffer, NULL, NULL, &SifQueueData);
    sceSifRpcLoop(&SifQueueData);
}

int _start(int argc, char *argv[])
{
    int result;
    iop_thread_t thread;

    printf("HTTPClient start\n");

    if (RegisterLibraryEntries(&_exp_httpc) == 0) {
        thread.attr = TH_C;
        thread.option = 0x00001B14;
        thread.thread = &RpcThread;
        thread.priority = 0x20;
        thread.stacksize = 0x800;
        if ((RpcThreadID = CreateThread(&thread)) > 0) {
            StartThread(RpcThreadID, NULL);
            result = 0;
        } else {
            result = RpcThreadID;
            ReleaseLibraryEntries(&_exp_httpc);
        }
    } else {
        result = -1;
    }

    return (result == 0 ? MODULE_RESIDENT_END : MODULE_NO_RESIDENT_END);
}

int _exit(int argc, char *argv[])
{
    ReleaseLibraryEntries(&_exp_httpc);

    TerminateThread(RpcThreadID);
    DeleteThread(RpcThreadID);

    return MODULE_NO_RESIDENT_END;
}
