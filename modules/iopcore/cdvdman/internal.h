#ifdef __IOPCORE_DEBUG
#define DPRINTF(args...)  printf(args)
#define iDPRINTF(args...) Kprintf(args)
#else
#define DPRINTF(args...)
#define iDPRINTF(args...)
#endif

struct SteamingData
{
    unsigned short int StBufmax;
    unsigned short int StBankmax;
    unsigned short int StBanksize;
    unsigned short int StWritePtr;
    unsigned short int StReadPtr;
    unsigned short int StStreamed;
    unsigned short int StStat;
    unsigned short int StIsReading;
    void *StIOP_bufaddr;
    u32 Stlsn;
};

typedef struct
{
    int err;
    int status;
    struct SteamingData StreamingData;
    int intr_ef;
    int disc_type_reg;
    u32 cdread_lba;
    u32 cdread_sectors;
    void *cdread_buf;
} cdvdman_status_t;

typedef void (*StmCallback_t)(void);

//Internal (common) function prototypes
void SetStm0Callback(StmCallback_t callback);
int cdvdman_AsyncRead(u32 lsn, u32 sectors, void *buf);
