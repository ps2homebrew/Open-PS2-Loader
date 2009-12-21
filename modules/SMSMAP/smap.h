/* ps2-load-ip

   smap.h

   Copyright (c)2001 Sony Computer Entertainment Inc.
   Copyright (c)2001 YAEGASHI Takeshi
   Copyright (2)2002 Dan Potter
   Copyright (c)2003 T Lindstrom
   License: GPL

   $Id: smap.h 360 2004-05-16 13:57:25Z oobles $
*/

/*
 *  smap.h -- PlayStation 2 Ethernet device driver header file
 *
 *	Copyright (C) 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * Modified for ps2-load-ip by Dan Potter
 */

#ifndef	__SMAP_H__
#define	__SMAP_H__

#include "types.h"


struct pbuf;

typedef enum SMapStatus		{SMap_OK,SMap_Err,SMap_Con,SMap_TX} SMapStatus;


//Function prototypes
int			SMap_Init(void);
void			SMap_Start(void);
void			SMap_Stop(void);
int			SMap_CanSend(void);
SMapStatus	SMap_Send(struct pbuf* pPacket);
int			SMap_HandleTXInterrupt(int iFlags);
int			SMap_HandleRXEMACInterrupt(int iFlags);
u8 const*	SMap_GetMACAddress(void);
void			SMap_EnableInterrupts(int iFlags);
void			SMap_DisableInterrupts(int iFlags);
int			SMap_GetIRQ(void);
void			SMap_ClearIRQ(int iFlags);


#if		defined(DEBUG)
#define	dbgprintf(args...)	printf(args)
#else
#define	dbgprintf(args...)	((void)0)
#endif


#define	INTR_EMAC3		(1<<6)
#define	INTR_RXEND		(1<<5)
#define	INTR_TXEND		(1<<4)
#define	INTR_RXDNV		(1<<3)	//descriptor not valid
#define	INTR_TXDNV		(1<<2)	//descriptor not valid
#define	INTR_CLR_ALL	(INTR_RXEND|INTR_TXEND|INTR_RXDNV)
#define	INTR_ENA_ALL	(INTR_EMAC3|INTR_CLR_ALL)
#define	INTR_BITMSK		0x7C

#endif	/* __SMAP_H__ */
