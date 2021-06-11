#include <string.h>
#include <kernel.h>
#include <sifrpc.h>

#include "nbns.h"
#include "ioman.h"

static SifRpcClientData_t SifRpcClient;
static char RpcBuffer[64] ALIGNED(64);

int nbnsInit(void)
{
    while (SifBindRpc(&SifRpcClient, 0x00001B13, 0) < 0 || SifRpcClient.server == NULL) {
        LOG("libnbns: bind failed\n");
        nopdelay();
    }

    return 0;
}

void nbnsDeinit(void)
{
    memset(&SifRpcClient, 0, sizeof(SifRpcClientData_t));
}

int nbnsFindName(const char *name, unsigned char *ip_address)
{
    int result;

    strncpy(RpcBuffer, name, 16);
    RpcBuffer[16] = '\0';

    if ((result = SifCallRpc(&SifRpcClient, NBNS_RPC_ID_FIND_NAME, 0, RpcBuffer, 17, RpcBuffer, sizeof(struct nbnsFindNameResult), NULL, NULL)) >= 0) {
        result = ((struct nbnsFindNameResult *)RpcBuffer)->result;
        memcpy(ip_address, ((struct nbnsFindNameResult *)RpcBuffer)->address, 4);
    }

    return result;
}
