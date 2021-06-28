// In the SONY original, all the calls to DEBUG_PRINTF() were to sceInetPrintf().
#define DEBUG_PRINTF(args...) // printf(args)

#define MAX_FRAME_SIZE 1518

#define PRE_LWIP_130_COMPAT 1

struct SmapDriverData
{
    volatile u8 *smap_regbase;
    volatile u8 *emac3_regbase;
    unsigned char TxBDIndex;
    unsigned char RxBDIndex;
    unsigned char TxDNVBDIndex;
    int Dev9IntrEventFlag;
    int TxEndEventFlag;
    int IntrHandlerThreadID;
    int TxHandlerThreadID;
    unsigned char SmapIsInitialized;
    unsigned char NetDevStopFlag;
    unsigned char EnableLinkCheckTimer;
    unsigned char LinkStatus;
    unsigned char LinkMode;
    iop_sys_clock_t LinkCheckTimer;
};

/* Function prototypes */
int smap_init(int argc, char *argv[]);
int SMAPStart(void);
void SMAPStop(void);
int SMAPGetMACAddress(unsigned char *buffer);

void SMapLowLevelInput(struct pbuf *pBuf);

#include "xfer.h"
