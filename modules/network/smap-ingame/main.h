//In the SONY original, all the calls to DEBUG_PRINTF() were to sceInetPrintf().
#define DEBUG_PRINTF(args...) //printf(args)

#define MAX_FRAME_SIZE 1518

#define PRE_LWIP_130_COMPAT 1

/*
	Sorry, but even I can't explain the syntax used here. :(
	I know that _ori_gp has to be "early-clobbered" and the GP register will get clobbered... but I don't really know why GCC can't determine which registers it can and can't use automatically. And I don't really understand what "clobbering" registers is.
*/
#define SaveGP()                    \
    void *_ori_gp;                  \
    __asm volatile("move %0, $gp\n" \
                   "move $gp, %1"   \
                   : "=&r"(_ori_gp) \
                   : "r"(&_gp)      \
                   : "gp")

#define RestoreGP()                              \
    __asm volatile("move $gp, %0" ::"r"(_ori_gp) \
                   : "gp")

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

inline void SMapLowLevelInput(struct pbuf *pBuf);

#include "xfer.h"
