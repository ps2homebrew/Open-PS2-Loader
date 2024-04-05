#include <string.h>
#include <kernel.h>
#include <sifrpc.h>

#include "httpclient.h"
#include "ioman.h"

static SifRpcClientData_t SifRpcClient;
static unsigned char RpcTxBuffer[256] ALIGNED(64);
static unsigned char RpcRxBuffer[64] ALIGNED(64);

int HttpInit(void)
{
    while (SifBindRpc(&SifRpcClient, 0x00001B14, 0) < 0 || SifRpcClient.server == NULL) {
        LOG("libhttpclient: bind failed\n");
        nopdelay();
    }

    return 0;
}

void HttpDeinit(void)
{
    memset(&SifRpcClient, 0, sizeof(SifRpcClientData_t));
}

int HttpEstabConnection(char *server, u16 port)
{
    int result;

    strncpy(((struct HttpClientConnEstabArgs *)RpcTxBuffer)->server, server, HTTP_CLIENT_SERVER_NAME_MAX - 1);
    ((struct HttpClientConnEstabArgs *)RpcTxBuffer)->server[HTTP_CLIENT_SERVER_NAME_MAX - 1] = '\0';
    ((struct HttpClientConnEstabArgs *)RpcTxBuffer)->port = port;

    if ((result = SifCallRpc(&SifRpcClient, HTTP_CLIENT_CMD_CONN_ESTAB, 0, RpcTxBuffer, sizeof(struct HttpClientConnEstabArgs), RpcRxBuffer, sizeof(s32), NULL, NULL)) >= 0)
        result = *(s32 *)RpcRxBuffer;

    return result;
}

void HttpCloseConnection(s32 HttpSocket)
{
    ((struct HttpClientConnCloseArgs *)RpcTxBuffer)->socket = HttpSocket;
    SifCallRpc(&SifRpcClient, HTTP_CLIENT_CMD_CONN_CLOSE, 0, RpcTxBuffer, sizeof(struct HttpClientConnCloseArgs), NULL, 0, NULL, NULL);
}

int HttpSendGetRequest(s32 HttpSocket, const char *UserAgent, const char *host, s8 *mode, const u8 *mtime, const char *uri, char *output, u16 *out_len)
{
    int result;

    ((struct HttpClientSendGetArgs *)RpcTxBuffer)->socket = HttpSocket;
    strncpy(((struct HttpClientSendGetArgs *)RpcTxBuffer)->UserAgent, UserAgent, HTTP_CLIENT_USER_AGENT_MAX - 1);
    ((struct HttpClientSendGetArgs *)RpcTxBuffer)->UserAgent[HTTP_CLIENT_USER_AGENT_MAX - 1] = '\0';
    strncpy(((struct HttpClientSendGetArgs *)RpcTxBuffer)->host, host, HTTP_CLIENT_SERVER_NAME_MAX - 1);
    ((struct HttpClientSendGetArgs *)RpcTxBuffer)->host[HTTP_CLIENT_SERVER_NAME_MAX - 1] = '\0';
    ((struct HttpClientSendGetArgs *)RpcTxBuffer)->mode = *mode;
    if (mtime != NULL) {
        memcpy(((struct HttpClientSendGetArgs *)RpcTxBuffer)->mtime, mtime, sizeof(((struct HttpClientSendGetArgs *)RpcTxBuffer)->mtime));
        ((struct HttpClientSendGetArgs *)RpcTxBuffer)->hasMtime = 1;
    } else {
        memset(((struct HttpClientSendGetArgs *)RpcTxBuffer)->mtime, 0, sizeof(((struct HttpClientSendGetArgs *)RpcTxBuffer)->mtime));
        ((struct HttpClientSendGetArgs *)RpcTxBuffer)->hasMtime = 0;
    }
    strncpy(((struct HttpClientSendGetArgs *)RpcTxBuffer)->uri, uri, HTTP_CLIENT_URI_MAX - 1);
    ((struct HttpClientSendGetArgs *)RpcTxBuffer)->uri[HTTP_CLIENT_URI_MAX - 1] = '\0';
    ((struct HttpClientSendGetArgs *)RpcTxBuffer)->output = output;
    ((struct HttpClientSendGetArgs *)RpcTxBuffer)->out_len = *out_len;

    if (!IS_UNCACHED_SEG(output))
        SifWriteBackDCache(output, *out_len);

    if ((result = SifCallRpc(&SifRpcClient, HTTP_CLIENT_CMD_SEND_GET_REQ, 0, RpcTxBuffer, sizeof(struct HttpClientSendGetArgs), RpcRxBuffer, sizeof(struct HttpClientSendGetResult), NULL, NULL)) >= 0) {
        result = ((struct HttpClientSendGetResult *)RpcRxBuffer)->result;
        *mode = ((struct HttpClientSendGetResult *)RpcRxBuffer)->mode;
        *out_len = ((struct HttpClientSendGetResult *)RpcRxBuffer)->out_len;
    }

    return result;
}
