#define HTTP_CMODE_CLOSED     0
#define HTTP_CMODE_PERSISTENT 1

// EE-side only
int HttpInit(void);
void HttpDeinit(void);

int HttpEstabConnection(char *server, u16 port);
void HttpCloseConnection(s32 HttpSocket);

/*  mtime[0] = Years since year 2000
    mtime[1] = Month, 0-11
    mtime[2] = day in month, 0-30
    mtime[3] = Hour (0-23)
    mtime[4] = Minute (0-59)
    mtime[5] = Second (0-59)
*/

int HttpSendGetRequest(s32 HttpSocket, const char *UserAgent, const char *host, s8 *mode, const u8 *mtime, const char *uri, char *output, u16 *out_len);

#define HTTP_CLIENT_SERVER_NAME_MAX 30
#define HTTP_CLIENT_USER_AGENT_MAX  16
#define HTTP_CLIENT_URI_MAX         128

enum HTTP_CLIENT_CMD {
    HTTP_CLIENT_CMD_CONN_ESTAB,
    HTTP_CLIENT_CMD_CONN_CLOSE,
    HTTP_CLIENT_CMD_SEND_GET_REQ,
};

struct HttpClientConnEstabArgs
{
    char server[HTTP_CLIENT_SERVER_NAME_MAX];
    u16 port;
};

struct HttpClientConnCloseArgs
{
    s32 socket;
};

struct HttpClientSendGetArgs
{
    s32 socket;
    char UserAgent[HTTP_CLIENT_USER_AGENT_MAX];
    char host[HTTP_CLIENT_SERVER_NAME_MAX];
    s8 mode;
    u8 hasMtime;
    u8 mtime[6];
    char uri[HTTP_CLIENT_URI_MAX];
    u16 out_len;
    void *output;
};

struct HttpClientSendGetResult
{
    s32 result;
    s8 mode;
    u8 padding;
    u16 out_len;
};

#ifdef _IOP
#define httpc_IMPORTS_start DECLARE_IMPORT_TABLE(httpc, 1, 1)
#define httpc_IMPORTS_end   END_IMPORT_TABLE

#define I_HttpEstabConnection DECLARE_IMPORT(4, HttpEstabConnection)
#define I_HttpCloseConnection DECLARE_IMPORT(5, HttpCloseConnection)
#define I_HttpSendGetRequest  DECLARE_IMPORT(6, HttpSendGetRequest)
#endif
