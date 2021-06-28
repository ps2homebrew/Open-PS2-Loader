/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: ps2ip.c 577 2004-09-14 14:41:46Z pixel $
# PS2 TCP/IP STACK FOR IOP
*/

#include <types.h>
#include <stdio.h>
#include <intrman.h>
#include <loadcore.h>
#include <thbase.h>
#include <modload.h>
#include <sysclib.h>
#include <thevent.h>
#include <thsemap.h>
#include <sysmem.h>

#include "smsutils.h"

#include <lwip/memp.h>
#include <lwip/sys.h>
#include <lwip/tcpip.h>
#include <lwip/netif.h>
#include <lwip/dhcp.h>
#include <netif/loopif.h>
#include <netif/etharp.h>

#include "ps2ip_internal.h"
#include "arch/sys_arch.h"

#define SYS_MBOX_SIZE        64
#define SYS_THREAD_PRIO_BASE TCPIP_THREAD_PRIO

struct sys_mbox_msg
{
    struct sys_mbox_msg *pNext;
    void *pvMSG;
};

struct sys_mbox
{
    u16_t u16First;
    u16_t u16Last;
    void *apvMSG[SYS_MBOX_SIZE];
    sys_sem_t Mail;
    sys_sem_t Mutex;
    int iWaitPost;
    int iWaitFetch;
};

typedef struct ip_addr IPAddr;

#define MODNAME "TCP/IP Stack"
IRX_ID(MODNAME, 1, 5);

extern struct irx_export_table _exp_ps2ip;

#if LWIP_HAVE_LOOPIF
static NetIF LoopIF;
#endif /* LWIP_HAVE_LOOPIF */

static u16_t inline GenNextMBoxIndex(u16_t u16Index)
{
    return (u16Index + 1) % SYS_MBOX_SIZE;
} /* end GenNextMBoxIndex */

static int inline IsMessageBoxFull(sys_mbox_t apMBox)
{
    return GenNextMBoxIndex(apMBox->u16Last) == apMBox->u16First;
} /* end IsMessageBoxFull */

static int inline IsMessageBoxEmpty(sys_mbox_t apMBox)
{
    return apMBox->u16Last == apMBox->u16First;
} /* end IsMessageBoxEmpty */

#ifdef INTERRUPT_CTX_INPKT
void PostInputMSG(sys_mbox_t pMBox, void *pvMSG)
{

    pMBox->apvMSG[pMBox->u16Last] = pvMSG;
    pMBox->u16Last = GenNextMBoxIndex(pMBox->u16Last);

    if (pMBox->iWaitFetch > 0)
        iSignalSema(pMBox->Mail);

} /* end PostInputMSG */
#endif

#ifdef FULL_LWIP
int ps2ip_getconfig(char *pszName, t_ip_info *pInfo)
{

    struct netif *pNetIF = netif_find(pszName);

    if (pNetIF == NULL) {
        mips_memset(pInfo, 0, sizeof(*pInfo));
        return 0;
    } /* end if */

    strcpy(pInfo->netif_name, pszName);

    pInfo->ipaddr.s_addr = pNetIF->ip_addr.addr;
    pInfo->netmask.s_addr = pNetIF->netmask.addr;
    pInfo->gw.s_addr = pNetIF->gw.addr;
    mips_memcpy(pInfo->hw_addr, pNetIF->hwaddr, sizeof(pInfo->hw_addr));
#if LWIP_DHCP
    if (pNetIF->dhcp) {
        pInfo->dhcp_enabled = 1;
        pInfo->dhcp_status = pNetIF->dhcp->state;
    } else {
        pInfo->dhcp_enabled = 0;
        pInfo->dhcp_status = 0;
    } /* end else */
#else
    pInfo->dhcp_enabled = 0;
#endif /* LWIP_DHCP */
    return 1;

} /* end ps2ip_getconfig */

int ps2ip_setconfig(t_ip_info *pInfo)
{

    struct netif *pNetIF = netif_find(pInfo->netif_name);

    if (!pNetIF)
        return 0;

    netif_set_ipaddr(pNetIF, (IPAddr *)&pInfo->ipaddr);
    netif_set_netmask(pNetIF, (IPAddr *)&pInfo->netmask);
    netif_set_gw(pNetIF, (IPAddr *)&pInfo->gw);
#if LWIP_DHCP
    if (pInfo->dhcp_enabled) {
        if (!pNetIF->dhcp)
            dhcp_start(pNetIF);
    } else {
        if (pNetIF->dhcp)
            dhcp_stop(pNetIF);
    }  /* end else */
#endif /* LWIP_DHCP */
    return 1;

} /* end ps2ip_setconfig */
#endif

#ifdef INTERRUPT_CTX_INPKT
typedef struct InputMSG
{
    struct pbuf *pInput;
    struct netif *pNetIF;
} InputMSG;

#define MSG_QUEUE_SIZE 16

static InputMSG aMSGs[MSG_QUEUE_SIZE];
static u8_t u8FirstMSG = 0;
static u8_t u8LastMSG = 0;

static u8_t inline GetNextMSGQueueIndex(u8_t u8Index)
{
    return (u8Index + 1) % MSG_QUEUE_SIZE;
} /* end GetNextMSGQueueIndex */

static int inline IsMSGQueueFull(void)
{
    return GetNextMSGQueueIndex(u8LastMSG) == u8FirstMSG;
} /* end IsMSGQueueFull */

static void InputCB(void *pvArg)
{

    InputMSG *pMSG = (InputMSG *)pvArg;
    struct pbuf *pInput = pMSG->pInput;
    struct netif *pNetIF = pMSG->pNetIF;
    int iFlags;

    CpuSuspendIntr(&iFlags);
    u8FirstMSG = GetNextMSGQueueIndex(u8FirstMSG);
    CpuResumeIntr(iFlags);

    switch (((struct eth_hdr *)(pInput->payload))->type) {

        case ETHTYPE_IP:
            etharp_ip_input(pNetIF, pInput);
            pbuf_header(pInput, -14);
            ip_input(pInput, pNetIF);
            break;

        case ETHTYPE_ARP:
            etharp_arp_input(
                pNetIF, (struct eth_addr *)&pNetIF->hwaddr, pInput);
        default:
            pbuf_free(pInput);

    } /* end switch */

} /* end InputCB */

extern sys_mbox_t g_TCPIPMBox;

err_t ps2ip_input(struct pbuf *pInput, struct netif *pNetIF)
{
    // When ps2smap receive data, it invokes this function. It'll be called directly by the interrupthandler, which means we are
    // running in an interrupt-context. We'll pass on the data to the tcpip message-thread by adding a callback message. If the
    // messagebox is full, we can't wait for the tcpip-thread to process a message to make room for our message, since we're
    // in interrupt-context. If the messagebox or messagequeue is full, drop the packet.
    InputMSG *pIMSG;
    struct tcpip_msg *pMSG;

    if (IsMessageBoxFull(g_TCPIPMBox) || IsMSGQueueFull()) {
        pbuf_free(pInput);
        return ERR_OK;
    } /* end if */
      // Allocate messagequeue entry.
    pIMSG = &aMSGs[u8LastMSG];
    u8LastMSG = GetNextMSGQueueIndex(u8LastMSG);
    // Initialize the InputMSG.
    pIMSG->pInput = pInput;
    pIMSG->pNetIF = pNetIF;
    pMSG = (struct tcpip_msg *)memp_malloc(MEMP_TCPIP_MSG);

    if (!pMSG) {
        pbuf_free(pInput);
        return ERR_MEM;
    } /* end if */

    pMSG->type = TCPIP_MSG_CALLBACK;
    pMSG->msg.cb.f = InputCB;
    pMSG->msg.cb.ctx = pIMSG;
    PostInputMSG(g_TCPIPMBox, pMSG);

    return ERR_OK;

} /* end ps2ip_input */
#else
err_t ps2ip_input(struct pbuf *pInput, struct netif *pNetIF)
{
    // switch(htons(((struct eth_hdr*)(pInput->payload))->type))    // Don't know why, but using htons will cause this function to not work in the SMS LWIP stack.
    switch (((struct eth_hdr *)(pInput->payload))->type) {
        case ETHTYPE_IP:
            // IP-packet. Update ARP table, obtain first queued packet.
            etharp_ip_input(pNetIF, pInput);
            pbuf_header(pInput, (int)-sizeof(struct eth_hdr));
            pNetIF->input(pInput, pNetIF);
            break;
        case ETHTYPE_ARP:
            // ARP-packet. Pass pInput to ARP module, get ARP reply or ARP queued packet.
            // Pass to network layer.
            etharp_arp_input(pNetIF, (struct eth_addr *)&pNetIF->hwaddr, pInput);
        // Fall through: The SMS LWIP stack was modified, and etharp_arp_input does not free the PBUF on its own.
        default:
            // Unsupported ethernet packet-type. Free pInput.
            pbuf_free(pInput);
    }

    return ERR_OK;
}
#endif

void ps2ip_Stub(void)
{

} /* end ps2ip_Stub */

int ps2ip_ShutDown(void)
{

    return 1;

} /* end ps2ip_ShutDown */
#if LWIP_HAVE_LOOPIF
static void AddLoopIF(void)
{

    IPAddr IP;
    IPAddr NM;
    IPAddr GW;

    IP4_ADDR(&IP, 127, 0, 0, 1);
    IP4_ADDR(&NM, 255, 0, 0, 0);
    IP4_ADDR(&GW, 127, 0, 0, 1);

    netif_add(&LoopIF, &IP, &NM, &GW, NULL, loopif_init, tcpip_input);

} /* end AddLoopIF */
#endif /* LWIP_HAVE_LOOPIF */
int _start(int argc, char **argv)
{

    sys_sem_t lSema;

    RegisterLibraryEntries(&_exp_ps2ip);

    mem_init();
    memp_init();
    pbuf_init();
    netif_init();

    lSema = sys_sem_new(0);
    tcpip_init((void (*)(void *))SignalSema, (void *)lSema);
    WaitSema(lSema);
    sys_sem_free(lSema);
#if LWIP_HAVE_LOOPIF
    AddLoopIF();
#endif /* LWIP_HAVE_LOOPIF */

    return MODULE_RESIDENT_END;

} /* end _start */

sys_thread_t sys_thread_new(
    void (*pFunction)(void *), void *pvArg, int iPrio)
{

    iop_thread_t Info = {
        TH_C, 0, pFunction, 0x900, iPrio + SYS_THREAD_PRIO_BASE};
    int iThreadID;

    iThreadID = CreateThread(&Info);

    if (iThreadID < 0)
        return -1;

    StartThread(iThreadID, pvArg);

    return iThreadID;

} /* end sys_thread_new */

sys_mbox_t sys_mbox_new(void)
{

    sys_mbox_t pMBox;
    int OldState;

    CpuSuspendIntr(&OldState);
    pMBox = (sys_mbox_t)AllocSysMemory(ALLOC_FIRST, sizeof(struct sys_mbox), NULL);
    CpuResumeIntr(OldState);

    if (!pMBox)
        return NULL;

    pMBox->u16First = pMBox->u16Last = 0;
    pMBox->Mail = sys_sem_new(0);
    pMBox->Mutex = sys_sem_new(1);
    pMBox->iWaitPost = pMBox->iWaitFetch = 0;

    return pMBox;

} /* end sys_mbox_new */

void sys_mbox_free(sys_mbox_t pMBox)
{

    if (!pMBox)
        return;

    WaitSema(pMBox->Mutex);

    sys_sem_free(pMBox->Mail);
    sys_sem_free(pMBox->Mutex);

    FreeSysMemory(pMBox);

} /* end sys_mbox_free */

void sys_mbox_post(sys_mbox_t pMBox, void *pvMSG)
{

    sys_prot_t Flags;
    sys_sem_t sem;

    if (!pMBox)
        return;

    CpuSuspendIntr(&Flags);

    while (IsMessageBoxFull(pMBox)) {

        ++pMBox->iWaitPost;

        CpuResumeIntr(Flags);
        WaitSema(pMBox->Mail);
        CpuSuspendIntr(&Flags);

        --pMBox->iWaitPost;

    } /* end while */

    pMBox->apvMSG[pMBox->u16Last] = pvMSG;
    pMBox->u16Last = GenNextMBoxIndex(pMBox->u16Last);

    sem = (pMBox->iWaitFetch > 0) ? pMBox->Mail : SYS_SEM_NULL;

    CpuResumeIntr(Flags);

    if (sem != SYS_SEM_NULL)
        SignalSema(sem);

} /* end sys_mbox_post */

u32_t sys_arch_mbox_fetch(sys_mbox_t pMBox, void **ppvMSG, u32_t u32Timeout)
{

    sys_prot_t Flags;
    sys_sem_t sem = SYS_SEM_NULL;
    u32_t u32Time = 0;

    CpuSuspendIntr(&Flags);

    while (IsMessageBoxEmpty(pMBox)) {

        u32_t u32WaitTime;

        ++pMBox->iWaitFetch;

        CpuResumeIntr(Flags);
        u32WaitTime = sys_arch_sem_wait(pMBox->Mail, u32Timeout);
        CpuSuspendIntr(&Flags);

        --pMBox->iWaitFetch;

        if (u32WaitTime == SYS_ARCH_TIMEOUT) {
            u32Time = u32WaitTime;
            goto end;
        } /* end if */

        u32Time += u32WaitTime;
        u32Timeout -= u32WaitTime;

    } /* end while */

    if (ppvMSG != NULL) // This pointer may be NULL.
        *ppvMSG = pMBox->apvMSG[pMBox->u16First];
    pMBox->u16First = GenNextMBoxIndex(pMBox->u16First);

    sem = (pMBox->iWaitPost > 0) ? pMBox->Mail : SYS_SEM_NULL;
end:
    CpuResumeIntr(Flags);

    if (sem != SYS_SEM_NULL)
        SignalSema(sem);

    return u32Time;

} /* end sys_arch_mbox_fetch */

sys_sem_t sys_sem_new(u8_t aCount)
{

    iop_sema_t lSema = {SA_THPRI, 1, aCount, 1};
    int retVal;

    retVal = CreateSema(&lSema);

    if (retVal <= 0)
        retVal = SYS_SEM_NULL;

    return retVal;

} /* end sys_sem_new */

static unsigned int TimeoutHandler(void *pvArg)
{
    iReleaseWaitThread((int)pvArg);
    return 0;
}

static u32_t ComputeTimeDiff(const iop_sys_clock_t *pStart, const iop_sys_clock_t *pEnd)
{
    iop_sys_clock_t Diff;
    u32 iSec, iUSec, iDiff;

    Diff.lo = pEnd->lo - pStart->lo;
    Diff.hi = pEnd->hi - pStart->hi - (pStart->lo > pEnd->lo);

    SysClock2USec(&Diff, &iSec, &iUSec);
    iDiff = (iSec * 1000) + (iUSec / 1000);

    return ((iDiff != 0) ? iDiff : 1);
}

u32_t sys_arch_sem_wait(sys_sem_t aSema, u32_t aTimeout)
{
    u32 WaitTime;

    if (aTimeout == 0)
        return (WaitSema(aSema) == 0 ? 0 : SYS_ARCH_TIMEOUT);
    else if (aTimeout == 1)
        return (PollSema(aSema) == 0 ? 0 : SYS_ARCH_TIMEOUT);
    else {

        iop_sys_clock_t lTimeout;
        iop_sys_clock_t Start;
        iop_sys_clock_t End;
        int lTID = GetThreadId();

        GetSystemTime(&Start);
        USec2SysClock(aTimeout * 1000, &lTimeout);
        SetAlarm(&lTimeout, &TimeoutHandler, (void *)lTID);

        if (!WaitSema(aSema)) {
            CancelAlarm(&TimeoutHandler, (void *)lTID);
            GetSystemTime(&End);

            WaitTime = ComputeTimeDiff(&Start, &End);
            if (WaitTime > aTimeout)
                WaitTime = aTimeout;
        } else
            WaitTime = SYS_ARCH_TIMEOUT;

    } /* end else */

    return WaitTime;

} /* end sys_arch_sem_wait */

void sys_sem_free(sys_sem_t aSema)
{

    if (aSema != SYS_SEM_NULL)
        DeleteSema(aSema);

} /* end sys_sem_free */

#if MEM_LIBC_MALLOC
void *ps2ip_mem_malloc(int size)
{
    int OldState;
    void *ret;

    CpuSuspendIntr(&OldState);
    ret = AllocSysMemory(ALLOC_LAST, size, NULL);
    CpuResumeIntr(OldState);

    return ret;
}

void ps2ip_mem_free(void *rmem)
{
    int OldState;

    CpuSuspendIntr(&OldState);
    FreeSysMemory(rmem);
    CpuResumeIntr(OldState);
}

/* Only pbuf_realloc() uses mem_realloc(), which uses this function.
   As pbuf_realloc can only shrink PBUFs, I don't think SYSMEM will fail to allocate a smaller region.    */
void *ps2ip_mem_realloc(void *rmem, int newsize)
{
    int OldState;
    void *ret;

    CpuSuspendIntr(&OldState);
    FreeSysMemory(rmem);
    ret = AllocSysMemory(ALLOC_ADDRESS, newsize, rmem);
    CpuResumeIntr(OldState);

    return ret;
}
#endif
