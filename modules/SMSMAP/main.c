/*
 * Copyright (c) Tord Lindstrom (pukko@home.se)
 * Copyright (c) adresd ( adresd_ps2dev@yahoo.com )
 *
 */

#include <stdio.h>
#include <loadcore.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <vblank.h>
#include <intrman.h>

#include "ps2ip.h"
#include "smap.h"
#include "dev9.h"
#include "sysclib.h"

#define	UNKN_1464   *(u16 volatile*)0xbf801464

#define	IFNAME0	's'
#define	IFNAME1	'm'

#define	TIMER_INTERVAL		(100*1000)
#define	TIMEOUT				(300*1000)
#define	MAX_REQ_CNT			8

typedef struct ip_addr IPAddr;
typedef struct netif   NetIF;
typedef struct SMapIF  SMapIF;
typedef struct pbuf    PBuf;


static int		iSendMutex;
static int		iSendReqMutex;
static int		iSendReq = 0x80000000;
static int 		iReqNR=0;
static int 		iReqCNT=0;
static PBuf*	apReqQueue[MAX_REQ_CNT];
static int		iTimeoutCNT=0;

struct SMapIF {

 NetIF*	pNetIF;

};

static SMapIF SIF;

NetIF NIF;

#define	ERR_OK    0
#define	ERR_CONN -6
#define	ERR_IF  -11

static SMapStatus AddToQueue ( PBuf* pBuf ) {

 int        iIntFlags;
 SMapStatus Ret;

 CpuSuspendIntr ( &iIntFlags );

 if ( !iReqCNT ) {

  Ret = SMap_Send ( pBuf );

  if ( Ret == SMap_TX ) {

   iTimeoutCNT = 0;
   goto storeLast;

  }  /* end if */

 } else if ( iReqCNT < MAX_REQ_CNT ) {
storeLast:
  apReqQueue[ ( iReqNR + iReqCNT ) % MAX_REQ_CNT ] = pBuf;
  ++iReqCNT;
  ++pBuf -> ref;

  Ret = SMap_OK;

 } else Ret = SMap_TX;

 CpuResumeIntr ( iIntFlags );

 return	Ret;

}  /* end AddToQueue */

static void SendRequests ( void ) {

 while ( iReqCNT > 0 ) {

  PBuf*      lpReq = apReqQueue[ iReqNR ];
  SMapStatus lSts  = SMap_Send ( lpReq );

  iTimeoutCNT = 0;

  if ( lSts == SMap_TX ) return;

  pbuf_free ( lpReq );

  iReqNR = ( iReqNR + 1 ) % MAX_REQ_CNT;
  --iReqCNT;

 }  /* end while */

}  /* end SendRequests */

static void QueueHandler ( void ) {

 SendRequests ();

 if ( iSendReq >= 0 ) {

  iSignalSema ( iSendReq );
  iSendReq = 0x80000000;

 }  /* end if */

}  /* end QueueHandler */

static int SMapInterrupt ( int iFlag ) {

 int iFlags = SMap_GetIRQ ();

 if (  iFlags & ( INTR_TXDNV | INTR_TXEND )  ) {

  SMap_HandleTXInterrupt (  iFlags & ( INTR_TXDNV | INTR_TXEND )  );

  QueueHandler ();

  iFlags = SMap_GetIRQ ();

 }  /* end if */

 if (  iFlags & ( INTR_EMAC3 | INTR_RXDNV | INTR_RXEND )  )

  SMap_HandleRXEMACInterrupt (  iFlags & ( INTR_EMAC3 | INTR_RXDNV | INTR_RXEND )  );

 return 1;

}  /* end SMapInterrupt */

static unsigned int Timer ( void* pvArg ) {

 if ( iReqCNT ) {

  iTimeoutCNT += TIMER_INTERVAL;

  if (  iTimeoutCNT >= TIMEOUT && iTimeoutCNT < ( TIMEOUT + TIMER_INTERVAL )  ) QueueHandler ();

 }  /* end if */

 return	( unsigned int )pvArg;

}  /* end Timer */

static err_t SMapLowLevelOutput ( NetIF* pNetIF, PBuf* pOutput ) {

 while ( 1 ) {

  int        iFlags;
  SMapStatus Res;

  if (   (  Res = AddToQueue ( pOutput )  ) == SMap_OK   ) return ERR_OK;
  if (      Res                             == SMap_Err  ) return ERR_IF;
  if (      Res                             == SMap_Con  ) return ERR_CONN;

  WaitSema ( iSendMutex );

  CpuSuspendIntr ( &iFlags );

  if ( iReqCNT == MAX_REQ_CNT ) {

   iSendReq = iSendReqMutex;
   CpuResumeIntr ( iFlags );

   WaitSema ( iSendReqMutex );

  } else CpuResumeIntr ( iFlags );

  SignalSema ( iSendMutex );

 }  /* end while */

}  /* end SMapLowLevelOutput */

static err_t SMapOutput ( NetIF* pNetIF, PBuf* pOutput, IPAddr* pIPAddr ) {

 PBuf* pBuf = etharp_output ( pNetIF, pIPAddr, pOutput );

 return pBuf ? SMapLowLevelOutput ( pNetIF, pBuf ) : ERR_OK;

}  /* end SMapOutput */

static err_t SMapIFInit ( NetIF* pNetIF ) {

 SIF.pNetIF = pNetIF;
 pNetIF -> state      = &NIF;
 pNetIF -> name[ 0 ]  = IFNAME0;
 pNetIF -> name[ 1 ]  = IFNAME1;
 pNetIF -> output     = SMapOutput;
 pNetIF -> linkoutput = SMapLowLevelOutput;
 pNetIF -> hwaddr_len = 6;
 pNetIF -> mtu        = 1500;

 memcpy (  pNetIF -> hwaddr, SMap_GetMACAddress (), 6  );

 SMap_Start ();

 return ERR_OK;

}  /* end SMapIFInit */

static int SMapInit ( IPAddr IP, IPAddr NM, IPAddr GW ) {

 int             i;
 iop_sys_clock_t ClockTicks;

 dev9IntrDisable ( INTR_BITMSK );
 EnableIntr ( IOP_IRQ_DEV9 );
 CpuEnableIntr ();

 UNKN_1464 = 3;

 if (   (  iSendMutex    = CreateMutex ( IOP_MUTEX_UNLOCKED )  ) < 0   ) return 0;
 if (   (  iSendReqMutex = CreateMutex ( IOP_MUTEX_UNLOCKED )  ) < 0   ) return	0;
 if	(   !SMap_Init ()                                                  ) return	0;

 for ( i = 2; i < 7; ++i ) dev9RegisterIntrCb ( i, SMapInterrupt );

 USec2SysClock ( TIMER_INTERVAL, &ClockTicks );
 SetAlarm (  &ClockTicks, Timer, ( void* )ClockTicks.lo  );

 netif_add ( &NIF, &IP, &NM, &GW, NULL, SMapIFInit, tcpip_input );
 netif_set_default ( &NIF );

 return 1;

}  /* end SMapInit */

int _start ( int iArgC,char** ppcArgV ) {

 IPAddr IP;
 IPAddr NM;
 IPAddr GW;

 if ( iArgC >= 4 ) {

  IP.addr = inet_addr( ppcArgV[ 1 ] );
  NM.addr = inet_addr( ppcArgV[ 2 ] );
  GW.addr = inet_addr (ppcArgV[ 3 ] );

 } else {

  IP4_ADDR( &IP, 192, 168,   0, 80 );
  IP4_ADDR( &NM, 255, 255, 255,  0 );
  IP4_ADDR( &GW, 192, 168,   0,  1 );

 }  /* end else */

 return !SMapInit( IP, NM, GW );

}  /* end _start */
