/*
 * Copyright (c) Tord Lindstrom (pukko@home.se)
 * Copyright (c) adresd ( adresd_ps2dev@yahoo.com )
 *
 */

#include <defs.h>
#include <stdio.h>
#include <sysclib.h>
#include <loadcore.h>
#include <thbase.h>
#include "smsutils.h"
#include <intrman.h>

#include "smstcpip.h"

#include "main.h"

#define dbgprintf(args...) DEBUG_PRINTF(args)

IRX_ID("smap_driver", 2, 1);

#define IFNAME0 's'
#define IFNAME1 'm'

typedef struct ip_addr IPAddr;
typedef struct netif NetIF;
typedef struct SMapIF SMapIF;
typedef struct pbuf PBuf;

static NetIF NIF;

// From lwip/err.h and lwip/tcpip.h

#define ERR_OK   0   // No error, everything OK
#define ERR_CONN -6  // Not connected
#define ERR_IF   -11 // Low-level netif error

// SMapLowLevelOutput():

// This function is called by the TCP/IP stack when a low-level packet should be sent. It'll be invoked in the context of the
// tcpip-thread.

static err_t
SMapLowLevelOutput(NetIF *pNetIF, PBuf *pOutput)
{
    int TotalLength;
    void *buffer;
    struct pbuf *pbuf;
    static unsigned char FrameBuffer[MAX_FRAME_SIZE];

#if USE_GP_REGISTER
    void *OldGP;

    OldGP = SetModuleGP();
#endif

    if (pOutput->next != NULL) {
        TotalLength = 0;
        pbuf = pOutput;
        while (pbuf != NULL) {
            mips_memcpy(&FrameBuffer[TotalLength], pbuf->payload, pbuf->len);
            TotalLength += pbuf->len;
            pbuf = pbuf->next;
        }

        buffer = FrameBuffer;
    } else {
        buffer = pOutput->payload;
        TotalLength = pOutput->tot_len;
    }

    SMAPSendPacket(buffer, TotalLength);

#if USE_GP_REGISTER
    SetGP(OldGP);
#endif

    return ERR_OK;
}

// SMapOutput():
// This function is called by the TCP/IP stack when an IP packet should be sent. It'll be invoked in the context of the
// tcpip-thread, hence no synchronization is required.
//  For LWIP versions before v1.3.0.
#ifdef PRE_LWIP_130_COMPAT
static err_t SMapOutput(NetIF *pNetIF, PBuf *pOutput, IPAddr *pIPAddr)
{
    err_t result;
    PBuf *pBuf;

#if USE_GP_REGISTER
    void *OldGP;

    OldGP = SetModuleGP();
#endif

    pBuf = etharp_output(pNetIF, pIPAddr, pOutput);

    result = pBuf != NULL ? SMapLowLevelOutput(pNetIF, pBuf) : ERR_OK;

#if USE_GP_REGISTER
    SetGP(OldGP);
#endif

    return result;
}
#endif

// SMapIFInit():
// Should be called at the beginning of the program to set up the network interface.
static err_t SMapIFInit(NetIF *pNetIF)
{
#if USE_GP_REGISTER
    void *OldGP;

    OldGP = SetModuleGP();
#endif

    pNetIF->name[0] = IFNAME0;
    pNetIF->name[1] = IFNAME1;
#ifdef PRE_LWIP_130_COMPAT
    pNetIF->output = &SMapOutput; // For LWIP versions before v1.3.0.
#else
    pNetIF->output = &etharp_output;                                                  // For LWIP 1.3.0 and later.
#endif
    pNetIF->linkoutput = &SMapLowLevelOutput;
    pNetIF->hwaddr_len = NETIF_MAX_HWADDR_LEN;
#ifdef PRE_LWIP_130_COMPAT
    pNetIF->flags |= (NETIF_FLAG_UP | NETIF_FLAG_LINK_UP | NETIF_FLAG_BROADCAST); // For LWIP versions before v1.3.0. Set NETIF_FLAG_UP here for compatibility with SMSTCPIP.
#else
    pNetIF->flags |= (NETIF_FLAG_LINK_UP | NETIF_FLAG_ETHARP | NETIF_FLAG_BROADCAST); // For LWIP v1.3.0 and later.
#endif
    pNetIF->mtu = 1500;

    // Get MAC address.
    SMAPGetMACAddress(pNetIF->hwaddr);
    dbgprintf("MAC address : %02d:%02d:%02d:%02d:%02d:%02d\n", pNetIF->hwaddr[0], pNetIF->hwaddr[1], pNetIF->hwaddr[2],
              pNetIF->hwaddr[3], pNetIF->hwaddr[4], pNetIF->hwaddr[5]);

    // Enable sending and receiving of data.
    SMAPStart();

#if USE_GP_REGISTER
    SetGP(OldGP);
#endif

    return ERR_OK;
}

void SMapLowLevelInput(PBuf *pBuf)
{
    // When we receive data, the interrupt-handler will invoke this function, which means we are in an interrupt-context. Pass on
    // the received data to ps2ip.

    ps2ip_input(pBuf, &NIF);
}

static inline int SMapInit(IPAddr IP, IPAddr NM, IPAddr GW, int argc, char *argv[])
{
    int result;

    if (smap_init(argc, argv) >= 0) {
        dbgprintf("SMapInit: SMap initialized\n");

        netif_add(&NIF, &IP, &NM, &GW, NULL, &SMapIFInit, tcpip_input);
        netif_set_default(&NIF);
        //        netif_set_up(&NIF);    // Not supported by SMSTCPIP.
        dbgprintf("SMapInit: NetIF added to ps2ip\n");

        // Return 1 (true) to indicate success.
        result = 1;
    } else
        result = 0;

    return result;
}

int _start(int argc, char *argv[])
{
    IPAddr IP, NM, GW;
    int result;

    dbgprintf("SMAP: argc %d\n", iArgC);

    // Parse IP args.
    if (argc >= 4) {
        dbgprintf("SMAP: %s %s %s\n", argv[1], argv[2], argv[3]);
        IP.addr = inet_addr(argv[1]);
        NM.addr = inet_addr(argv[2]);
        GW.addr = inet_addr(argv[3]);
    } else {
        // Set some defaults.
        IP4_ADDR(&IP, 192, 168, 0, 80);
        IP4_ADDR(&NM, 255, 255, 255, 0);
        IP4_ADDR(&GW, 192, 168, 0, 1);
    }

    if (!SMapInit(IP, NM, GW, argc - 4, &argv[4])) {

        // Something went wrong.
        result = MODULE_NO_RESIDENT_END;
    } else
        result = MODULE_RESIDENT_END; // Initialized ok.

    return result;
}
