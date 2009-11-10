/* 
   smap.c

   Copyright (c)2001 Sony Computer Entertainment Inc.
   Copyright (c)2001 YAEGASHI Takeshi
   Copyright (2)2002 Dan Potter
   Copyright (c)2003 T Lindstrom
   Copyright (c)2003 adresd
   License: GPL

   $Id: smap.c 1105 2005-05-21 12:43:39Z pixel $
*/

/* Pulled from the eCos port */

// $Id: smap.c 1105 2005-05-21 12:43:39Z pixel $
// many routines are taken from drivers/ps2/smap.c of PS2 Linux kernel (GPL).


#include "smap.h"
#include "stddef.h"
#include "stdio.h"
#include "intrman.h"
#include "loadcore.h"
#include "thbase.h"
#include "dev9.h"
#include "ps2ip.h"

#include "sysclib.h"

#define	SET			1
#define	CLEAR			0
#define	TRUE			1
#define	FALSE			0
#define	ENABLE		1
#define	DISABLE		0
#define	START			1
#define	STOP			0
#define	RESET_ONLY	1
#define	RESET_INIT	0


//Flags
#define	SMAP_F_OPENED				(1<<0)
#define	SMAP_F_LINKESTABLISH		(1<<1)
#define	SMAP_F_LINKVALID			(1<<2)
#define	SMAP_F_CHECK_FORCE100M	(1<<3)
#define	SMAP_F_CHECK_FORCE10M	(1<<4)
#define	SMAP_F_TXDNV_DISABLE		(1<<16)
#define	SMAP_F_RXDNV_DISABLE		(1<<17)
#define	SMAP_F_PRINT_PKT			(1<<30)
#define	SMAP_F_PRINT_MSG			(1<<31)


#define	SMAP_TXBUFBASE		0x1000
#define	SMAP_TXBUFSIZE		(4*1024)
#define	SMAP_RXBUFBASE		0x4000
#define	SMAP_RXBUFSIZE		(16*1024)

#define	SMAP_ALIGN					16
#define	SMAP_TXMAXSIZE				(6+6+2+1500)
#define	SMAP_TXMAXTAILPAD			4
#define	SMAP_TXMAXPKTSZ_INFIFO	(SMAP_TXMAXSIZE+2)	//Multiple of 4
#define	SMAP_RXMAXSIZE				(6+6+2+1500+4)
#define	SMAP_RXMINSIZE				14							//Ethernet header size
#define	SMAP_RXMAXTAILPAD			4

#define	SMAP_LOOP_COUNT			10000
#define	SMAP_AUTONEGO_TIMEOUT	3000
#define	SMAP_AUTONEGO_RETRY		3
#define	SMAP_FORCEMODE_WAIT		2000
#define	SMAP_FORCEMODE_TIMEOUT	1000


//Buffer Descriptor(BD) Offset and Definitions
#define	SMAP_BD_BASE			0x3000
#define	SMAP_BD_BASE_TX		(SMAP_BD_BASE+0x0000)
#define	SMAP_BD_BASE_RX		(SMAP_BD_BASE+0x0200)
#define	SMAP_BD_SIZE			512
#define	SMAP_BD_MAX_ENTRY		64

#define	SMAP_BD_NEXT(x)	(x)=(x)<(SMAP_BD_MAX_ENTRY-1) ? (x)+1:0

//TX Control
#define	SMAP_BD_TX_READY		(1<<15)	//set:driver, clear:HW
#define	SMAP_BD_TX_GENFCS		(1<<9)	//generate FCS
#define	SMAP_BD_TX_GENPAD		(1<<8)	//generate padding
#define	SMAP_BD_TX_INSSA		(1<<7)	//insert source address
#define	SMAP_BD_TX_RPLSA		(1<<6)	//replace source address
#define	SMAP_BD_TX_INSVLAN	(1<<5)	//insert VLAN Tag
#define	SMAP_BD_TX_RPLVLAN	(1<<4)	//replace VLAN Tag

//TX Status
#define	SMAP_BD_TX_READY		(1<<15)	//set:driver, clear:HW
#define	SMAP_BD_TX_BADFCS		(1<<9)	//bad FCS
#define	SMAP_BD_TX_BADPKT		(1<<8)	//bad previous pkt in dependent mode
#define	SMAP_BD_TX_LOSSCR		(1<<7)	//loss of carrior sense
#define	SMAP_BD_TX_EDEFER		(1<<6)	//excessive deferal
#define	SMAP_BD_TX_ECOLL		(1<<5)	//excessive collision
#define	SMAP_BD_TX_LCOLL		(1<<4)	//late collision
#define	SMAP_BD_TX_MCOLL		(1<<3)	//multiple collision
#define	SMAP_BD_TX_SCOLL		(1<<2)	//single collision
#define	SMAP_BD_TX_UNDERRUN	(1<<1)	//underrun
#define	SMAP_BD_TX_SQE			(1<<0)	//SQE
#define	SMAP_BD_TX_ERRMASK	(SMAP_BD_TX_BADFCS|SMAP_BD_TX_BADPKT|SMAP_BD_TX_LOSSCR|SMAP_BD_TX_EDEFER|SMAP_BD_TX_ECOLL|	\
										 SMAP_BD_TX_LCOLL|SMAP_BD_TX_MCOLL|SMAP_BD_TX_SCOLL|SMAP_BD_TX_UNDERRUN|SMAP_BD_TX_SQE)

//RX Control
#define	SMAP_BD_RX_EMPTY	(1<<15)	//set:driver, clear:HW

//RX Status
#define	SMAP_BD_RX_EMPTY			(1<<15)	//set:driver, clear:HW
#define	SMAP_BD_RX_OVERRUN		(1<<9)	//overrun
#define	SMAP_BD_RX_PFRM			(1<<8)	//pause frame
#define	SMAP_BD_RX_BADFRM			(1<<7)	//bad frame
#define	SMAP_BD_RX_RUNTFRM		(1<<6)	//runt frame
#define	SMAP_BD_RX_SHORTEVNT		(1<<5)	//short event
#define	SMAP_BD_RX_ALIGNERR		(1<<4)	//alignment error
#define	SMAP_BD_RX_BADFCS			(1<<3)	//bad FCS
#define	SMAP_BD_RX_FRMTOOLONG	(1<<2)	//frame too long
#define	SMAP_BD_RX_OUTRANGE		(1<<1)	//out of range error
#define	SMAP_BD_RX_INRANGE		(1<<0)	//in range error
#define	SMAP_BD_RX_ERRMASK		(SMAP_BD_RX_OVERRUN|SMAP_BD_RX_PFRM|SMAP_BD_RX_BADFRM|SMAP_BD_RX_RUNTFRM|SMAP_BD_RX_SHORTEVNT|	\
											 SMAP_BD_RX_ALIGNERR|SMAP_BD_RX_BADFCS|SMAP_BD_RX_FRMTOOLONG|SMAP_BD_RX_OUTRANGE|					\
											 SMAP_BD_RX_INRANGE)


struct smapbd
{
	u16	ctrl_stat;
	u16	reserved;	//Must be zero
	u16	length;		//Number of bytes in pkt
	u16	pointer;
};

typedef struct smapbd volatile	SMapBD;


typedef struct SMapCircularBuffer
{
	SMapBD*	pBD;
	u16		u16PTRStart;
	u16		u16PTREnd;
	u8			u8IndexStart;
	u8			u8IndexEnd;
} SMapCB;

typedef struct SMap
{
	u32				u32Flags;
	u8 volatile*	pu8Base;
	u8					au8HWAddr[6];
	u32				u32TXMode;
	u8					u8PPWC;
	SMapCB			TX;
	u8					u8RXIndex;
	u16				u16RXPTR;
	SMapBD*			pRXBD;
#ifndef FORCE_100M_FD	
	struct			SMapPhySpecific *phy_specific;
#endif	
} SMap;

/*
 * PHY specific functions
 */
#ifndef FORCE_100M_FD
struct SMapPhySpecific {
	int (* get_link_status) (SMap*);
	int (* get_speed) (SMap *);
	int (* get_duplex) (SMap *);
};
#endif

//Register Offset and Definitions
#define	SMAP_PIOPORT_DIR	0x2C
#define	SMAP_PIOPORT_IN	0x2E
#define	SMAP_PIOPORT_OUT	0x2E
#define	PP_DOUT				(1<<4)	//Data output, read port
#define	PP_DIN				(1<<5)	//Data input,  write port
#define	PP_SCLK				(1<<6)	//Clock,       write port
#define	PP_CSEL				(1<<7)	//Chip select, write port
//operation code
#define	PP_OP_READ		2	//2b'10
#define	PP_OP_WRITE		1	//2b'01
#define	PP_OP_EWEN		0	//2b'00
#define	PP_OP_EWDS		0	//2b'00

#define	SMAP_BASE			0xb0000000

#define SPD_R_REV_1                     0x02

#define	SMAP_INTR_STAT		0x28
#define	SMAP_INTR_CLR		0x128
#define	SMAP_INTR_ENABLE	0x2A
#define	SMAP_BD_MODE		0x102
#define	BD_SWAP				(1<<0)

#define	SMAP_TXFIFO_CTRL			0x1000
#define	TXFIFO_RESET				(1<<0)
#define	TXFIFO_DMAEN				(1<<1) 
#define	SMAP_TXFIFO_WR_PTR		0x1004
#define	SMAP_TXFIFO_SIZE			0x1008
#define	SMAP_TXFIFO_FRAME_CNT	0x100C
#define	SMAP_TXFIFO_FRAME_INC	0x1010
#define	SMAP_TXFIFO_DATA			0x1100

#define	SMAP_RXFIFO_CTRL			0x1030
#define	RXFIFO_RESET				(1<<0)
#define	RXFIFO_DMAEN				(1<<1) 
#define	SMAP_RXFIFO_RD_PTR		0x1034
#define	SMAP_RXFIFO_SIZE			0x1038
#define	SMAP_RXFIFO_FRAME_CNT	0x103C
#define	SMAP_RXFIFO_FRAME_DEC	0x1040
#define	SMAP_RXFIFO_DATA			0x1200

#define	SMAP_FIFO_ADDR		0x1300
#define	FIFO_CMD_READ		(1<<1)
#define	FIFO_DATA_SWAP		(1<<0)
#define	SMAP_FIFO_DATA		0x1308

#define	SMAP_REG8(pSMap,Offset)		(*(u8 volatile*)((pSMap)->pu8Base+(Offset)))
#define	SMAP_REG16(pSMap,Offset)	(*(u16 volatile*)((pSMap)->pu8Base+(Offset)))
#define	SMAP_REG32(pSMap,Offset)	(*(u32 volatile*)((pSMap)->pu8Base+(Offset)))

#define	SMAP_EEPROM_WRITE_WAIT		100000
#define	SMAP_PP_GET_Q(pSMap)			((SMAP_REG8((pSMap),SMAP_PIOPORT_IN)>>4)&1)
#define	SMAP_PP_SET_D(pSMap,D)		((pSMap)->u8PPWC=(D) ? ((pSMap)->u8PPWC|PP_DIN):((pSMap)->u8PPWC&~PP_DIN))
#define	SMAP_PP_SET_S(pSMap,S)		((pSMap)->u8PPWC=(S) ? ((pSMap)->u8PPWC|PP_CSEL):((pSMap)->u8PPWC&~PP_CSEL))
#define	SMAP_PP_CLK_OUT(pSMap,C)	{(pSMap)->u8PPWC=(C) ? ((pSMap)->u8PPWC|PP_SCLK):((pSMap)->u8PPWC&~PP_SCLK);	\
												 SMAP_REG8((pSMap),SMAP_PIOPORT_OUT)=(pSMap)->u8PPWC;}

//EMAC3 Register Offset and Definitions
#define	SMAP_EMAC3_BASE	0x2000
#define	SMAP_EMAC3_MODE0	(SMAP_EMAC3_BASE+0x00)
#define	E3_RXMAC_IDLE		(1<<31)
#define	E3_TXMAC_IDLE		(1<<30)
#define	E3_SOFT_RESET		(1<<29)
#define	E3_TXMAC_ENABLE	(1<<28)
#define	E3_RXMAC_ENABLE	(1<<27)
#define	E3_WAKEUP_ENABLE	(1<<26)

#define	SMAP_EMAC3_MODE1			(SMAP_EMAC3_BASE+0x04)
#define	E3_FDX_ENABLE				(1<<31)
#define	E3_INLPBK_ENABLE			(1<<30)	//internal loop back
#define	E3_VLAN_ENABLE				(1<<29)
#define	E3_FLOWCTRL_ENABLE		(1<<28)	//integrated flow ctrl(pause frame)
#define	E3_ALLOW_PF					(1<<27)	//allow pause frame
#define	E3_ALLOW_EXTMNGIF			(1<<25)	//allow external management IF
#define	E3_IGNORE_SQE				(1<<24)
#define	E3_MEDIA_FREQ_BITSFT		(22)
#define	E3_MEDIA_10M				(0<<22)
#define	E3_MEDIA_100M				(1<<22)
#define	E3_MEDIA_1000M				(2<<22)
#define	E3_MEDIA_MSK				(3<<22)
#define	E3_RXFIFO_SIZE_BITSFT	(20)
#define	E3_RXFIFO_512				(0<<20)
#define	E3_RXFIFO_1K				(1<<20)
#define	E3_RXFIFO_2K				(2<<20)
#define	E3_RXFIFO_4K				(3<<20)
#define	E3_TXFIFO_SIZE_BITSFT	(18)
#define	E3_TXFIFO_512				(0<<18)
#define	E3_TXFIFO_1K				(1<<18)
#define	E3_TXFIFO_2K				(2<<18)
#define	E3_TXREQ0_BITSFT			(15)
#define	E3_TXREQ0_SINGLE			(0<<15)
#define	E3_TXREQ0_MULTI			(1<<15)
#define	E3_TXREQ0_DEPEND			(2<<15)
#define	E3_TXREQ1_BITSFT			(13)
#define	E3_TXREQ1_SINGLE			(0<<13)
#define	E3_TXREQ1_MULTI			(1<<13)
#define	E3_TXREQ1_DEPEND			(2<<13)
#define	E3_JUMBO_ENABLE			(1<<12)

#define	SMAP_EMAC3_TxMODE0	(SMAP_EMAC3_BASE+0x08)
#define	E3_TX_GNP_0				(1<<31)	//get new packet
#define	E3_TX_GNP_1				(1<<30)	//get new packet
#define	E3_TX_GNP_DEPEND		(1<<29)	//get new packet
#define	E3_TX_FIRST_CHANNEL	(1<<28)

#define	SMAP_EMAC3_TxMODE1		(SMAP_EMAC3_BASE+0x0C)
#define	E3_TX_LOW_REQ_MSK			(0x1F)	//low priority request
#define	E3_TX_LOW_REQ_BITSFT		(27)		//low priority request
#define	E3_TX_URG_REQ_MSK			(0xFF)	//urgent priority request
#define	E3_TX_URG_REQ_BITSFT		(16)		//urgent priority request

#define	SMAP_EMAC3_RxMODE			(SMAP_EMAC3_BASE+0x10)
#define	E3_RX_STRIP_PAD			(1<<31)
#define	E3_RX_STRIP_FCS			(1<<30)
#define	E3_RX_RX_RUNT_FRAME		(1<<29)
#define	E3_RX_RX_FCS_ERR			(1<<28)
#define	E3_RX_RX_TOO_LONG_ERR	(1<<27)
#define	E3_RX_RX_IN_RANGE_ERR	(1<<26)
#define	E3_RX_PROP_PF				(1<<25)	//propagate pause frame
#define	E3_RX_PROMISC				(1<<24)
#define	E3_RX_PROMISC_MCAST		(1<<23)
#define	E3_RX_INDIVID_ADDR		(1<<22)
#define	E3_RX_INDIVID_HASH		(1<<21)
#define	E3_RX_BCAST					(1<<20)
#define	E3_RX_MCAST					(1<<19)

#define	SMAP_EMAC3_INTR_STAT		(SMAP_EMAC3_BASE+0x14)
#define	SMAP_EMAC3_INTR_ENABLE	(SMAP_EMAC3_BASE+0x18)
#define	E3_INTR_OVERRUN			(1<<25)	//this bit does NOT WORKED
#define	E3_INTR_PF					(1<<24)
#define	E3_INTR_BAD_FRAME			(1<<23)
#define	E3_INTR_RUNT_FRAME		(1<<22)
#define	E3_INTR_SHORT_EVENT		(1<<21)
#define	E3_INTR_ALIGN_ERR			(1<<20)
#define	E3_INTR_BAD_FCS			(1<<19)
#define	E3_INTR_TOO_LONG			(1<<18)
#define	E3_INTR_OUT_RANGE_ERR	(1<<17)
#define	E3_INTR_IN_RANGE_ERR		(1<<16)
#define	E3_INTR_DEAD_DEPEND		(1<<9)
#define	E3_INTR_DEAD_0				(1<<8)
#define	E3_INTR_SQE_ERR_0			(1<<7)
#define	E3_INTR_TX_ERR_0			(1<<6)
#define	E3_INTR_DEAD_1				(1<<5)
#define	E3_INTR_SQE_ERR_1			(1<<4)
#define	E3_INTR_TX_ERR_1			(1<<3)
#define	E3_INTR_MMAOP_SUCCESS	(1<<1)
#define	E3_INTR_MMAOP_FAIL		(1<<0)
#define	E3_INTR_ALL					(E3_INTR_OVERRUN|E3_INTR_PF|E3_INTR_BAD_FRAME|E3_INTR_RUNT_FRAME|E3_INTR_SHORT_EVENT|	\
											 E3_INTR_ALIGN_ERR|E3_INTR_BAD_FCS|E3_INTR_TOO_LONG|E3_INTR_OUT_RANGE_ERR|					\
											 E3_INTR_IN_RANGE_ERR|E3_INTR_DEAD_DEPEND|E3_INTR_DEAD_0| E3_INTR_SQE_ERR_0|				\
											 E3_INTR_TX_ERR_0|E3_INTR_DEAD_1|E3_INTR_SQE_ERR_1|E3_INTR_TX_ERR_1|							\
											 E3_INTR_MMAOP_SUCCESS|E3_INTR_MMAOP_FAIL)
#define	E3_DEAD_ALL					(E3_INTR_DEAD_DEPEND|E3_INTR_DEAD_0| E3_INTR_DEAD_1)

#define	SMAP_EMAC3_ADDR_HI	(SMAP_EMAC3_BASE+0x1C)
#define	SMAP_EMAC3_ADDR_LO	(SMAP_EMAC3_BASE+0x20)

#define	SMAP_EMAC3_VLAN_TPID		(SMAP_EMAC3_BASE+0x24)
#define	E3_VLAN_ID_MSK				0xFFFF

#define	SMAP_EMAC3_VLAN_TCI	(SMAP_EMAC3_BASE+0x28)
#define	E3_VLAN_TCITAG_MSK	0xFFFF

#define	SMAP_EMAC3_PAUSE_TIMER		(SMAP_EMAC3_BASE+0x2C)
#define	E3_PTIMER_MSK					0xFFFF

#define	SMAP_EMAC3_INDIVID_HASH1	(SMAP_EMAC3_BASE+0x30)
#define	SMAP_EMAC3_INDIVID_HASH2	(SMAP_EMAC3_BASE+0x34)
#define	SMAP_EMAC3_INDIVID_HASH3	(SMAP_EMAC3_BASE+0x38)
#define	SMAP_EMAC3_INDIVID_HASH4	(SMAP_EMAC3_BASE+0x3C)
#define	SMAP_EMAC3_GROUP_HASH1		(SMAP_EMAC3_BASE+0x40)
#define	SMAP_EMAC3_GROUP_HASH2		(SMAP_EMAC3_BASE+0x44)
#define	SMAP_EMAC3_GROUP_HASH3		(SMAP_EMAC3_BASE+0x48)
#define	SMAP_EMAC3_GROUP_HASH4		(SMAP_EMAC3_BASE+0x4C)
#define	E3_HASH_MSK						0xFFFF

#define	SMAP_EMAC3_LAST_SA_HI		(SMAP_EMAC3_BASE+0x50)
#define	SMAP_EMAC3_LAST_SA_LO		(SMAP_EMAC3_BASE+0x54)

#define	SMAP_EMAC3_INTER_FRAME_GAP		(SMAP_EMAC3_BASE+0x58)
#define	E3_IFGAP_MSK						0x3F

#define	SMAP_EMAC3_STA_CTRL		(SMAP_EMAC3_BASE+0x5C)
#define	E3_PHY_DATA_MSK			(0xFFFF)
#define	E3_PHY_DATA_BITSFT		(16)
#define	E3_PHY_OP_COMP				(1<<15)	//operation complete
#define	E3_PHY_ERR_READ			(1<<14)
#define	E3_PHY_STA_CMD_BITSFT	(12)
#define	E3_PHY_READ					(1<<12)
#define	E3_PHY_WRITE				(2<<12)
#define	E3_PHY_OPBCLCK_BITSFT	(10)
#define	E3_PHY_50M					(0<<10)
#define	E3_PHY_66M					(1<<10)
#define	E3_PHY_83M					(2<<10)
#define	E3_PHY_100M					(3<<10)
#define	E3_PHY_ADDR_MSK			(0x1F)
#define	E3_PHY_ADDR_BITSFT		(5)
#define	E3_PHY_REG_ADDR_MSK		(0x1F)

#define	SMAP_EMAC3_TX_THRESHOLD		(SMAP_EMAC3_BASE+0x60)
#define	E3_TX_THRESHLD_MSK			(0x1F)
#define	E3_TX_THRESHLD_BITSFT		(27)

#define	SMAP_EMAC3_RX_WATERMARK		(SMAP_EMAC3_BASE+0x64)
#define	E3_RX_LO_WATER_MSK			(0x1FF)
#define	E3_RX_LO_WATER_BITSFT		(23)
#define	E3_RX_HI_WATER_MSK			(0x1FF)
#define	E3_RX_HI_WATER_BITSFT		(7)

#define	SMAP_EMAC3_TX_OCTETS		(SMAP_EMAC3_BASE+0x68)
#define	SMAP_EMAC3_RX_OCTETS		(SMAP_EMAC3_BASE+0x6C)


//PHY Register Offset
#define	NS_OUI				0x080017
#define	DsPHYTER_ADDRESS	0x1
#define	DsPHYTER_BMCR		0x00
#define	PHY_BMCR_RST		(1<<15)		//ReSeT
#define	PHY_BMCR_LPBK		(1<<14)		//LooPBacK
#define	PHY_BMCR_100M		(1<<13)		//speed select, 1:100M, 0:10M
#define	PHY_BMCR_10M		(0<<13)		//speed select, 1:100M, 0:10M
#define	PHY_BMCR_ANEN		(1<<12)		//Auto-Negotiation ENable
#define	PHY_BMCR_PWDN		(1<<11)		//PoWer DowN
#define	PHY_BMCR_ISOL		(1<<10)		//ISOLate
#define	PHY_BMCR_RSAN		(1<<9)		//ReStart Auto-Negotiation
#define	PHY_BMCR_DUPM		(1<<8)		//DUPlex Mode, 1:FDX, 0:HDX
#define	PHY_BMCR_COLT		(1<<7)		//COLlision Test
#define	DsPHYTER_BMSR		0x01
#define	PHY_BMSR_ANCP		(1<<5)		//Auto-Negotiation ComPlete
#define	PHY_BMSR_LINK		(1<<2)		//LINK status
#define	DsPHYTER_PHYIDR1	0x02
#define	PHY_IDR1_VAL		(((NS_OUI<<2)>>8)&0xffff)
#define	DsPHYTER_PHYIDR2	0x03
#define	PHY_IDR2_VMDL		0x2			//Vendor MoDeL number
#define	PHY_IDR2_VAL		(((NS_OUI<<10)&0xFC00)|((PHY_IDR2_VMDL<<4)&0x3F0))
#define	PHY_IDR2_MSK		0xFFF0

#define	DsPHYTER_ANAR			0x04
#define	DsPHYTER_ANLPAR		0x05
#define	DsPHYTER_ANLPARNP		0x05
#define	DsPHYTER_ANER			0x06
#define	DsPHYTER_ANNPTR		0x07
//Extended registers
#define	DsPHYTER_PHYSTS		0x10
#define	PHY_STS_REL				(1<<13)	//Receive Error Latch
#define	PHY_STS_POST			(1<<12)	//POlarity STatus
#define	PHY_STS_FCSL			(1<<11)	//False Carrier Sense Latch
#define	PHY_STS_SD				(1<<10)	//100BT unconditional Signal Detect
#define	PHY_STS_DSL				(1<<9)	//100BT DeScrambler Lock
#define	PHY_STS_PRCV			(1<<8)	//Page ReCeiVed
#define	PHY_STS_RFLT			(1<<6)	//Remote FauLT
#define	PHY_STS_JBDT			(1<<5)	//JaBber DetecT
#define	PHY_STS_ANCP			(1<<4)	//Auto-Negotiation ComPlete
#define	PHY_STS_LPBK			(1<<3)	//LooPBacK status
#define	PHY_STS_DUPS			(1<<2)	//DUPlex Status,1:FDX,0:HDX
#define	PHY_STS_FDX				(1<<2)	//Full Duplex
#define	PHY_STS_HDX				(0<<2)	//Half Duplex
#define	PHY_STS_SPDS			(1<<1)	//SpeeD Status
#define	PHY_STS_10M				(1<<1)	//10Mbps
#define	PHY_STS_100M			(0<<1)	//100Mbps
#define	PHY_STS_LINK			(1<<0)	//LINK status
#define	DsPHYTER_FCSCR			0x14
#define	DsPHYTER_RECR			0x15
#define	DsPHYTER_PCSR			0x16
#define	DsPHYTER_PHYCTRL		0x19
#define	DsPHYTER_10BTSCR		0x1A
#define	DsPHYTER_CDCTRL		0x1B

/*
 * STEPHY1 10/100 Fast Ethernet 3.3V Transceiver (used in SCPH-10281)
 */
#define ST_OUI			0x0080E1
#define STEPHY1_XCR		0x00
#define STEPHY1_XSR		0x01
#define STEPHY1_PID1		0x02
#define	  STEPHY1_IDR1_VAL	0x1c04
#define STEPHY1_PID2		0x03
#define STEPHY1_ANA		0x04
#define STEPHY1_ANLPA		0x05
#define STEPHY1_ANE		0x06
/* extended registers */
#define STEPHY1_XCIIS		0x11 /* XCVR Configuration Information and Interrupt Status Register */
#define   STEPHY1_XCIIS_100M	(1 << 9)
#define   STEPHY1_XCIIS_FDX	(1 << 8)
#define   STEPHY1_XCIIS_LINK	(1 << 4)
#define STEPHY1_XIE		0x12 /* XCVR Interrupt Enable Register */
#define STEPHY1_100CTR		0x13 /* 100Base-TX PHY Control Status Register */
#define STEPHY1_XMC		0x14 /* XCVR Mode Control Register */

static SMap         SMap0;
extern struct netif NIF;

/*--------------------------------------------------------------------------*/

static void		TXRXEnable(SMap* pSMap,int iEnable);
static int		FIFOReset(SMap* pSMap);
static int		EMAC3SoftReset(SMap* pSMap);
static void		EMAC3SetDefValue(SMap* pSMap);
static void		EMAC3Init(SMap* pSMap,int iReset);
static void		EMAC3ReInit(SMap* pSMap);
static int		PhyInit(SMap* pSMap,int iReset);
static int		PhyReset(SMap* pSMap);
#ifdef FORCE_100M_FD
static void		__ForceSPD100M_FD(SMap* pSMap);
#else
static void		PhySetSpecific(SMap* pSMap);
static int		AutoNegotiation(SMap* pSMap,int iEnableAutoNego);
static int		ConfirmAutoNegotiation(SMap* pSMap);
static int		PhyGetSpeed(SMap* pSMap);
static int		PhyGetDuplex(SMap* pSMap);
static void		ForceSPD100M(SMap* pSMap);
static void		ForceSPD10M(SMap* pSMap);
static void		ConfirmForceSPD(SMap* pSMap);
#endif
static void		PhySetDSP(SMap* pSMap);
static void		Reset(SMap* pSMap,int iReset);
static int		GetNodeAddr(SMap* pSMap);
static void		BaseInit(SMap* pSMap);

#ifndef FORCE_100M_FD
/* NS DP83847 specific functions */
static int  DsPHYTER_get_link_status(SMap* pSMap);
static int  DsPHYTER_get_speed(SMap* pSMap);
static int  DsPHYTER_get_duplex(SMap* pSMap);

/* ST STEPHY1 specific functions */
static int  STEPHY1_get_link_status(SMap* pSMap);
static int  STEPHY1_get_speed(SMap* pSMap);
static int  STEPHY1_get_duplex(SMap* pSMap);

static struct SMapPhySpecific DsPHYTER_phy = {
	DsPHYTER_get_link_status,
	DsPHYTER_get_speed,
	DsPHYTER_get_duplex
};

static struct SMapPhySpecific STEPHY1_phy = {
	STEPHY1_get_link_status,
	STEPHY1_get_speed,
	STEPHY1_get_duplex
};
#endif

/*--------------------------------------------------------------------------*/
static inline u32 EMAC3REG_READ ( SMap* pSMap,u32 u32Offset ) {

 u32 hi = SMAP_REG16( pSMap, u32Offset     );
 u32 lo = SMAP_REG16( pSMap, u32Offset + 2 );

 return	( hi << 16 ) | lo;

}  /* end EMAC3REG_READ */

static inline void EMAC3REG_WRITE ( SMap* pSMap, u32 u32Offset,u32 u32V ) {

 SMAP_REG16( pSMap, u32Offset     ) = ( u32V >> 16 ) & 0xFFFF;
 SMAP_REG16( pSMap, u32Offset + 2 ) = u32V & 0xFFFF;

}  /* end EMAC3REG_WRITE */

static u16 ComputeFreeSize ( SMapCB const* pCB ) {

 u16 u16Start = pCB -> u16PTRStart;
 u16 u16End   = pCB -> u16PTREnd;

 return u16Start > u16End ? ( SMAP_TXBUFSIZE + u16End - u16Start )
                          : ( SMAP_TXBUFSIZE - u16End + u16Start );
}  /* end ComputeFreeSize */

extern void SMAP_CopyFromFIFO ( SMap*, struct pbuf* );

/*--------------------------------------------------------------------------*/
static void
TXRXEnable(SMap* pSMap,int iEnable)
{
	if	(iEnable)
	{

		//Enable tx/rx.

		EMAC3REG_WRITE(pSMap,SMAP_EMAC3_MODE0,E3_TXMAC_ENABLE|E3_RXMAC_ENABLE);
	}
	else
	{
		int	iA;
		u32	u32Val;

		//Disable tx/rx.

		EMAC3REG_WRITE(pSMap,SMAP_EMAC3_MODE0,EMAC3REG_READ(pSMap,SMAP_EMAC3_MODE0)&(~(E3_TXMAC_ENABLE|E3_RXMAC_ENABLE)));

		//Check EMAC3 idle status.

		for	(iA=SMAP_LOOP_COUNT;iA!=0;--iA)
		{
			u32Val=EMAC3REG_READ(pSMap,SMAP_EMAC3_MODE0);
			if	((u32Val&E3_RXMAC_IDLE)&&(u32Val&E3_TXMAC_IDLE))
			{
				return;
			}
		}
		dbgprintf("TXRXEnable: EMAC3 is still running(%x).\n",u32Val);
	}
}

extern void _smap_phy_write ( int, int );
__asm__(
 ".set noreorder\n\t"
 ".set nomacro\n\t"
 ".set noat\n\t"
 ".text\n\t"
 "_smap_phy_write:\n\t"
 "addiu $sp, $sp, -12\n\t"
 "andi  $a0, $a0, 0x1F\n\t"
 "sw    $ra, 0($sp)\n\t"
 "sw    $s0, 4($sp)\n\t"
 "lui   $s0, 0xB000\n\t"
 "andi  $a1, $a1, 0xFFFF\n\t"
 "ori   $a0, $a0, 0x2020\n\t"
 "ori   $s0, $s0, 0x205C\n\t"
 "sll   $a0, $a0, 16\n\t"
 "sw    $s1, 8($sp)\n\t"
 "or    $a0, $a0, $a1\n\t"
 "addiu $s1, $zero, 100\n\t"
 "sw    $a0, 0($s0)\n\t"
 "lw    $a0, 0($s0)\n\t"
 "lw    $a0, 0($s0)\n\t"
 "1:\n\t"
 "nop\n\t"
 "srl   $a0, $a0, 16\n\t"
 "andi  $a0, $a0, 0x8000\n\t"
 "bnez  $a0, 2f\n\t"
 "addiu $s1, $s1, -1\n\t"
 "jal   DelayThread\n\t"
 "addiu $a0, $zero, 1000\n\t"
 "bgtz  $s1, 1b\n\t"
 "lw    $a0, 0($s0)\n\t"
 "2:\n\t"
 "lw    $ra, 0($sp)\n\t"
 "lw    $s0, 4($sp)\n\t"
 "lw    $s1, 8($sp)\n\t"
 "jr    $ra\n\t"
 "addiu $sp, $sp, 12\n\t"
 ".set at\n\t"
 ".set macro\n\t"
 ".set reorder\n\t"
);

extern int _smap_phy_read ( int );
__asm__(
 ".set noreorder\n\t"
 ".set nomacro\n\t"
 ".set noat\n\t"
 ".text\n\t"
 "_smap_phy_read:\n\t"
 "addiu $sp, $sp, -12\n\t"
 "andi  $a0, $a0, 0x1F\n\t"
 "sw    $ra, 0($sp)\n\t"
 "sw    $s0, 4($sp)\n\t"
 "lui   $s0, 0xB000\n\t"
 "ori   $a0, $a0, 0x1020\n\t"
 "ori   $s0, $s0, 0x205C\n\t"
 "sll   $a0, $a0, 16\n\t"
 "sw    $s1, 8($sp)\n\t"
 "addiu $s1, $zero, 100\n\t"
 "sw    $a0, 0($s0)\n\t"
 "lw    $a0, 0($s0)\n\t"
 "lw    $a0, 0($s0)\n\t"
 "1:\n\t"
 "nop\n\t"
 "sll   $v0, $a0, 16\n\t"
 "srl   $a0, $a0, 16\n\t"
 "andi  $a0, $a0, 0x8000\n\t"
 "bnez  $a0, 2f\n\t"
 "srl   $v0, $v0, 16\n\t"
 "addiu $s1, $s1, -1\n\t"
 "jal   DelayThread\n\t"
 "addiu $a0, $zero, 1000\n\t"
 "bgtz  $s1, 1b\n\t"
 "lw    $a0, 0($s0)\n\t"
 "sll   $a0, $a0, 16\n\t"
 "srl   $v0, $a0, 16\n\t"
 "2:\n\t"
 "lw    $ra, 0($sp)\n\t"
 "lw    $s0, 4($sp)\n\t"
 "lw    $s1, 8($sp)\n\t"
 "jr    $ra\n\t"
 "addiu $sp, $sp, 12\n\t"
 ".set at\n\t"
 ".set macro\n\t"
 ".set reorder\n\t"
);

extern void _smap_bd_init ( void );
__asm__(
 ".set noreorder\n\t"
 ".set nomacro\n\t"
 ".set noat\n\t"
 ".text\n\t"
 "_smap_bd_init:\n\t"
 "lui   $v0, 0xB000\n\t"
 "addiu $at, $zero, 64\n\t"
 "ori   $v1, $v0, 0x3200\n\t"
 "ori   $v0, $v0, 0x3000\n\t"
 "ori   $a0, $zero, 0x8000\n\t"
 "1:\n\t"
 "sh    $zero, 0($v0)\n\t"
 "sh    $zero, 2($v0)\n\t"
 "sh    $zero, 4($v0)\n\t"
 "sh    $zero, 6($v0)\n\t"
 "addiu $v0, $v0, 8\n\t"
 "addiu $at, $at, -1\n\t"
 "sh    $a0,   0($v1)\n\t"
 "sh    $zero, 2($v1)\n\t"
 "sh    $zero, 4($v1)\n\t"
 "sh    $zero, 6($v1)\n\t"
 "bgtz  $at, 1b\n\t"
 "addiu $v1, $v1, 8\n\t"
 "jr    $ra\n\t"
 "nop\n\t"
 ".set at\n\t"
 ".set macro\n\t"
 ".set reorder\n\t"
);

static int
FIFOReset(SMap* pSMap)
{
	int	iA;
	int	iRetVal=0;

	//Reset TX FIFO.

	SMAP_REG8(pSMap,SMAP_TXFIFO_CTRL)=TXFIFO_RESET;

	//Reset RX FIFO.

	SMAP_REG8(pSMap,SMAP_RXFIFO_CTRL)=RXFIFO_RESET;

	//Confirm reset done.

	for	(iA=SMAP_LOOP_COUNT;iA!=0;--iA)
	{
		if	(!(SMAP_REG8(pSMap,SMAP_TXFIFO_CTRL)&TXFIFO_RESET))
		{
			break;
		}
	}
	if	(iA==0)
	{
		iRetVal|=1;
	}

	for	(iA=SMAP_LOOP_COUNT;iA!=0;--iA)
	{
		if	(!(SMAP_REG8(pSMap,SMAP_RXFIFO_CTRL)&RXFIFO_RESET))
		{
			break;
		}
	}
	if	(iA==0)
	{
		iRetVal|=2;
	}
	return	iRetVal;
}

static int
EMAC3SoftReset(SMap* pSMap)
{
	int	iA;

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_MODE0,E3_SOFT_RESET);
	for	(iA=SMAP_LOOP_COUNT;iA!=0;--iA)
	{
		if	(!(EMAC3REG_READ(pSMap,SMAP_EMAC3_MODE0)&E3_SOFT_RESET))
		{
			return	0;
		}
	}
	dbgprintf("EMAC3SoftReset: EMAC3 reset is in progress\n");
	return	-1;
}


static void
EMAC3SetDefValue(SMap* pSMap)
{
	//Set HW address.

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_ADDR_HI,((pSMap->au8HWAddr[0]<<8)|pSMap->au8HWAddr[1]));
	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_ADDR_LO,((pSMap->au8HWAddr[2]<<24)|(pSMap->au8HWAddr[3]<<16)|(pSMap->au8HWAddr[4]<<8)|
															pSMap->au8HWAddr[5]));

	//Inter-frame GAP.

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_INTER_FRAME_GAP,4);

	//Rx mode.

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_RxMODE,(E3_RX_STRIP_PAD|E3_RX_STRIP_FCS|E3_RX_INDIVID_ADDR|E3_RX_BCAST));

	//Tx fifo value for request priority. low = 7*8=56, urgent = 15*8=120.

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_TxMODE1,((7<<E3_TX_LOW_REQ_BITSFT)|(15<<E3_TX_URG_REQ_BITSFT)));

	//TX threshold, (12+1)*64=832.

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_TX_THRESHOLD,((12&E3_TX_THRESHLD_MSK)<<E3_TX_THRESHLD_BITSFT));

	//Rx watermark, low = 16*8=128, hi = 128*8=1024.

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_RX_WATERMARK,((16&E3_RX_LO_WATER_MSK)<<E3_RX_LO_WATER_BITSFT)|
																 ((128&E3_RX_HI_WATER_MSK)<<E3_RX_HI_WATER_BITSFT));

	//Enable all EMAC3 interrupts.

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_INTR_ENABLE,E3_INTR_ALL);
}


static void
EMAC3Init(SMap* pSMap,int iReset)
{

	//Reset EMAC3.

	EMAC3SoftReset(pSMap);

	//EMAC3 operating MODE.

	EMAC3REG_WRITE(pSMap,SMAP_EMAC3_MODE1,(E3_FDX_ENABLE|E3_IGNORE_SQE|E3_MEDIA_100M|E3_RXFIFO_2K|E3_TXFIFO_1K|E3_TXREQ0_SINGLE|
														E3_TXREQ1_SINGLE));

	//phy init.

	if	(PhyInit(pSMap,iReset)<0)
	{
		dbgprintf("EMAC3Init: Phy init error\n");
		return;
	}

	//This flag may be set when unloading

	if (iReset)
	{
		return;
	}

	dev9IntrDisable ( INTR_BITMSK );

	SMap_ClearIRQ(INTR_BITMSK);

	//Permanently set to default value.

	EMAC3SetDefValue(pSMap);
}

static void EMAC3ReInit ( SMap* pSMap ) {

 EMAC3SoftReset ( pSMap );
 EMAC3REG_WRITE( pSMap, SMAP_EMAC3_MODE1, pSMap -> u32TXMode );
 EMAC3SetDefValue ( pSMap );

}  /* end EMAC3ReInit */

static int PhyInit ( SMap* pSMap, int iReset ) {

 int iVal = PhyReset ( pSMap );

 if ( iVal < 0 ) return iVal;
 if ( iReset   ) return 0;

#ifndef FORCE_100M_FD 
 PhySetSpecific(pSMap);
#endif

#ifdef FORCE_100M_FD
 __ForceSPD100M_FD(pSMap);
#else
 iVal = AutoNegotiation ( pSMap, DISABLE );

 if ( iVal == 0 ) {

  pSMap -> u32Flags |= SMAP_F_LINKESTABLISH;
  PhySetDSP ( pSMap );

  return 0;

 }  /* end if */

 ForceSPD100M ( pSMap );
#endif

 return 0;

}  /* end PhyInit */

static int PhyReset ( SMap* pSMap ) {

 int i;

 _smap_phy_write ( DsPHYTER_BMCR, PHY_BMCR_RST | PHY_BMCR_100M | PHY_BMCR_ANEN | PHY_BMCR_DUPM );

 DelayThread ( 300 );

 for ( i = SMAP_LOOP_COUNT; i; --i ) {

  if (   !(  _smap_phy_read ( DsPHYTER_BMCR ) & PHY_BMCR_RST  )   ) return 0;

  DelayThread ( 300 );

 }  /* end for */

 return -1;

}  /* end PhyReset */

#ifndef FORCE_100M_FD
static void
PhySetSpecific(SMap* pSMap)
{
	int iID1;

	iID1 = _smap_phy_read ( DsPHYTER_PHYIDR1 );
	switch (iID1) {
	case PHY_IDR1_VAL:
	{
		pSMap->phy_specific = &DsPHYTER_phy;
		break;
	}
	case STEPHY1_IDR1_VAL:
	{
		pSMap->phy_specific = &STEPHY1_phy;
		break;
	}
	default:
	{
		pSMap->phy_specific = NULL;
		break;
	}
	}
}

static int
AutoNegotiation(SMap* pSMap,int iEnableAutoNego)
{
	int	iA;

	if	(iEnableAutoNego)
	{
        _smap_phy_write ( DsPHYTER_BMCR,PHY_BMCR_100M|PHY_BMCR_ANEN|PHY_BMCR_DUPM);
	}

	if	(ConfirmAutoNegotiation(pSMap)>=0)
	{
		return	0;
	}
	for	(iA=SMAP_AUTONEGO_RETRY;iA!=0;--iA)
	{
        _smap_phy_write ( DsPHYTER_BMCR,PHY_BMCR_100M|PHY_BMCR_ANEN|PHY_BMCR_DUPM|PHY_BMCR_RSAN);

		if	(ConfirmAutoNegotiation(pSMap)>=0)
		{
			return	0;
		}
	}

	return	-1;
}
#endif

#ifndef FORCE_100M_FD
static int
ConfirmAutoNegotiation(SMap* pSMap)
{
	int	iA;
	int	iPhyVal;
	int     spdrev;
	int link_up_flag = 0;
	int full_duplex_flag = 0;
	int speed_100m_flag = 0;	

	for	(iA=SMAP_AUTONEGO_TIMEOUT;iA!=0;--iA)
	{
		//Auto nego timeout is 3s.
		iPhyVal = _smap_phy_read(DsPHYTER_BMSR);
		if ((link_up_flag == 0) && (iPhyVal & PHY_BMSR_LINK))
			link_up_flag = 1;
 		if (iPhyVal & PHY_BMSR_ANCP)
		{
			break;
		}
		DelayThread(1000);	//wait 1ms
	}
	if	(iA==0)
	{
		return	-1;
	}
	
	// **ARGH: UGLY HACK! FIXME! **
	spdrev = SMAP_REG16(pSMap, SPD_R_REV_1);
	
	if (spdrev >= 0x13)
	{
		dbgprintf("Disabling autonegotiation sync on v12 - seems to work anyway - beware, hack inside.\n");
		return  0;
	}

	//Confirm speed & duplex mode.
	for	(iA=SMAP_LOOP_COUNT;iA!=0;--iA)
	{
        iPhyVal = _smap_phy_read ( DsPHYTER_BMSR );
		if ((link_up_flag == 0) && (iPhyVal & PHY_BMSR_LINK))
			link_up_flag = 1;
		if ((iPhyVal & PHY_BMSR_ANCP) && link_up_flag == 1) {
			speed_100m_flag = PhyGetSpeed(pSMap);
			full_duplex_flag = PhyGetDuplex(pSMap);
 			break;
		}
		DelayThread(1000);
	}
	if	(iA==0)
	{
		//Error.
		dbgprintf("ConfirmAutoNegotiation: Auto-negotiation error?? (BMSR=%x, link_up_flag=%d)\n", iPhyVal, link_up_flag);			
	}
	else
	{
		u32	u32E3Val=EMAC3REG_READ(pSMap,SMAP_EMAC3_MODE1);

		dbgprintf("ConfirmAutoNegotiation: Auto-negotiation complete. %s %s duplex mode.\n",
					 (speed_100m_flag == 0) ? "10Mbps":"100Mbps",(full_duplex_flag != 0) ? "Full":"Half");
				 
		if (full_duplex_flag)
		{
			//Full duplex mode.
			u32E3Val|=(E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF);
		}
		else
		{
			//Half duplex mode.
			u32E3Val&=~(E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF);
			if (speed_100m_flag == 0)
			{
				u32E3Val&=~E3_IGNORE_SQE;
			}
		}	
		u32E3Val&=~E3_MEDIA_MSK;
		u32E3Val|=speed_100m_flag==0 ? E3_MEDIA_10M:E3_MEDIA_100M;

		EMAC3REG_WRITE(pSMap,SMAP_EMAC3_MODE1,u32E3Val);
	}
	return	0;
}

static int
DsPHYTER_get_link_status(SMap* pSMap)
{
	int iPhyVal;

	iPhyVal = _smap_phy_read(DsPHYTER_PHYSTS);

	if (iPhyVal & PHY_STS_LINK)
		return 1;
		
	return 0;
}

static int
DsPHYTER_get_speed(SMap* pSMap)
{
	int iPhyVal;

	iPhyVal = _smap_phy_read(DsPHYTER_PHYSTS);

	if (iPhyVal & PHY_STS_100M)
		return 1;
		
	return 0;
}

static int
DsPHYTER_get_duplex(SMap* pSMap)
{
	int iPhyVal;

	iPhyVal = _smap_phy_read(DsPHYTER_PHYSTS);

	if (iPhyVal & PHY_STS_FDX)
		return 1;
		
	return 0;
}

static int
STEPHY1_get_link_status(SMap* pSMap)
{
	int iPhyVal;

	iPhyVal = _smap_phy_read(STEPHY1_XCIIS);

	if (!(iPhyVal & STEPHY1_XCIIS_LINK))
		return 1;
		
	return 0;
		
}

static int
STEPHY1_get_speed(SMap* pSMap)
{
	int iPhyVal;

	iPhyVal = _smap_phy_read(STEPHY1_XCIIS);

	if (iPhyVal & STEPHY1_XCIIS_100M)
		return 1;
		
	return 0;
}

static int
STEPHY1_get_duplex(SMap* pSMap)
{
	int iPhyVal;

	iPhyVal = _smap_phy_read(STEPHY1_XCIIS);

	if (iPhyVal & STEPHY1_XCIIS_FDX)
		return 1;
		
	return 0;
}

static int
PhyGetSpeed(SMap* pSMap)
{
	return pSMap->phy_specific->get_speed(pSMap);
}

static int
PhyGetDuplex(SMap* pSMap)
{
	return pSMap->phy_specific->get_duplex(pSMap);
}
#endif

#ifdef FORCE_100M_FD
static void __ForceSPD100M_FD(SMap* pSMap)
{
	int i;
	int link_flag = 0;
	
	/* Confirm link status, wait 1s is needed. */
	i = SMAP_FORCEMODE_TIMEOUT;
	while (--i) {
		int phy = _smap_phy_read(DsPHYTER_BMSR);
		if (phy & PHY_BMSR_LINK)
			break;
		DelayThread(1000);
	}
	if (i > 0)
		link_flag = 1;

	_smap_phy_write(DsPHYTER_BMCR, PHY_BMCR_100M|PHY_BMCR_DUPM);
	pSMap->u32Flags |= SMAP_F_CHECK_FORCE100M;

	i = SMAP_FORCEMODE_WAIT;
	while (--i)
		DelayThread(1000);
	
	if (link_flag) {	
		/* Force 100 Mbps full duplex mode */
		u32 u32E3Val = EMAC3REG_READ(pSMap, SMAP_EMAC3_MODE1);		
		u32E3Val |= (E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF);
		u32E3Val &= ~E3_MEDIA_MSK;
		u32E3Val |= E3_MEDIA_100M;
		EMAC3REG_WRITE(pSMap, SMAP_EMAC3_MODE1, u32E3Val);
		pSMap->u32Flags &= ~(SMAP_F_CHECK_FORCE100M|SMAP_F_CHECK_FORCE10M);
		pSMap->u32Flags |= SMAP_F_LINKESTABLISH;
		PhySetDSP(pSMap);
	}
}
#endif

#ifndef FORCE_100M_FD
static void ForceSPD100M ( SMap* apSMap ) {

 int i;

 _smap_phy_write ( DsPHYTER_BMCR, PHY_BMCR_100M );

 apSMap -> u32Flags |= SMAP_F_CHECK_FORCE100M;

 for ( i = SMAP_FORCEMODE_WAIT; i; --i ) DelayThread ( 1000 );

 ConfirmForceSPD ( apSMap );

}  /* end ForceSPD100M */
#endif

#ifndef FORCE_100M_FD
static void
ForceSPD10M(SMap* pSMap)
{
	int	iA;

    _smap_phy_write ( DsPHYTER_BMCR,PHY_BMCR_10M);

	pSMap->u32Flags|=SMAP_F_CHECK_FORCE10M;
	for	(iA=SMAP_FORCEMODE_WAIT;iA!=0;--iA)
	{
		DelayThread(1000);
	}
	ConfirmForceSPD(pSMap);
}
#endif

#ifndef FORCE_100M_FD
static void
ConfirmForceSPD(SMap* pSMap)
{
	int	iA;
	int	iPhyVal;

	//Confirm link status, wait 1s is needed.

	for	(iA=SMAP_FORCEMODE_TIMEOUT;iA!=0;--iA)
	{

        iPhyVal = _smap_phy_read ( DsPHYTER_BMSR );

		if	(iPhyVal&PHY_BMSR_LINK)
		{
			break;
		}
		DelayThread(1000);
	}
	if	(iA!=0)
	{
		u32	u32E3Val;

validlink:
		u32E3Val=EMAC3REG_READ(pSMap,SMAP_EMAC3_MODE1);
		u32E3Val&=~(E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF|E3_MEDIA_MSK);
		if (pSMap->u32Flags&SMAP_F_CHECK_FORCE100M)
		{
			dbgprintf("ConfirmForceSPD: 100Mbps Half duplex mode\n");
			u32E3Val|=E3_MEDIA_100M;
		}
		else if (pSMap->u32Flags&SMAP_F_CHECK_FORCE10M)
		{
			dbgprintf("ConfirmForceSPD: 10Mbps Half duplex mode\n");
			u32E3Val&=~E3_IGNORE_SQE;
			u32E3Val|=E3_MEDIA_10M;
		}
		EMAC3REG_WRITE(pSMap,SMAP_EMAC3_MODE1,u32E3Val);
		pSMap->u32Flags&=~(SMAP_F_CHECK_FORCE100M|SMAP_F_CHECK_FORCE10M);
		pSMap->u32Flags|=SMAP_F_LINKESTABLISH;
		PhySetDSP(pSMap);
		return;	//success
	}

	if	(pSMap->u32Flags&SMAP_F_CHECK_FORCE100M)
	{
		pSMap->u32Flags&=~SMAP_F_CHECK_FORCE100M;
		ForceSPD10M(pSMap);
	}
	else if	(pSMap->u32Flags&SMAP_F_CHECK_FORCE10M)
	{
		pSMap->u32Flags&=~SMAP_F_CHECK_FORCE10M;

		iPhyVal = _smap_phy_read ( DsPHYTER_PHYSTS );

		if	(iPhyVal&PHY_STS_LINK)
		{
			//Valid link.

			pSMap->u32Flags|=SMAP_F_CHECK_FORCE10M;
			goto	validlink;
		}
	}
}
#endif

static void
PhySetDSP(SMap* pSMap)
{
	int	iID1;
	int	iID2;
	int	iPhyVal;

	if (!(pSMap->u32Flags&SMAP_F_LINKESTABLISH))
	{

		//Link not established.

		return;
	}

	//Tvalue is used in emac3 re-init without phy init.

	pSMap->u32TXMode=EMAC3REG_READ(pSMap,SMAP_EMAC3_MODE1);

    iID1 = _smap_phy_read ( DsPHYTER_PHYIDR1 );
    iID2 = _smap_phy_read ( DsPHYTER_PHYIDR2 );

	if	(!((iID1==PHY_IDR1_VAL)&&((iID2&PHY_IDR2_MSK)==PHY_IDR2_VAL)))
	{
		pSMap->u32Flags|=SMAP_F_LINKVALID;
		return;
	}

	if (iID1 == STEPHY1_IDR1_VAL)
	{
		pSMap->u32Flags|=SMAP_F_LINKVALID;
		return;
	}
	
	if	(pSMap->u32Flags&SMAP_F_LINKVALID)
	{
		return;
	}

    _smap_phy_write ( 0x13, 0x0001 );
    _smap_phy_write ( 0x19, 0x1898 );
    _smap_phy_write ( 0x1F, 0x0000 );
    _smap_phy_write ( 0x1D, 0x5040 );
    _smap_phy_write ( 0x1E, 0x008C );
    _smap_phy_write ( 0x13, 0x0000 );

    iPhyVal = _smap_phy_read ( DsPHYTER_PHYSTS );

	if	((iPhyVal&(PHY_STS_DUPS|PHY_STS_SPDS|PHY_STS_LINK))==(PHY_STS_HDX|PHY_STS_10M|PHY_STS_LINK))
	{
        _smap_phy_write ( 0x1A, 0x0104 );
	}

	pSMap->u32Flags|=SMAP_F_LINKVALID;
}

static void Reset ( SMap* pSMap, int iReset ) {

 dev9IntrDisable ( INTR_BITMSK );
 SMap_ClearIRQ   ( INTR_BITMSK );

 SMAP_REG8( pSMap, SMAP_BD_MODE ) = 0;

 FIFOReset ( pSMap );
 EMAC3Init ( pSMap, iReset );

}  /* end Reset */

static inline void
EEPROMClockDataOut(SMap* pSMap,int val)
{
	SMAP_PP_SET_D(pSMap,val);

	SMAP_PP_CLK_OUT(pSMap,0);
	DelayThread(1);	//tDIS

	SMAP_PP_CLK_OUT(pSMap,1);
	DelayThread(1);	//tSKH, tDIH

	SMAP_PP_CLK_OUT(pSMap,0);
	DelayThread(1);	//tSKL
}


//1 clock with getting data

static inline int
EEPROMClockDataIn(SMap* pSMap)
{
	int	iRet;

	SMAP_PP_SET_D(pSMap,0);
	SMAP_PP_CLK_OUT(pSMap,0);
	DelayThread(1);	//tSKL

	SMAP_PP_CLK_OUT(pSMap,1);
	DelayThread(1);	//tSKH, tPD0,1
	iRet=SMAP_PP_GET_Q(pSMap);

	SMAP_PP_CLK_OUT(pSMap,0);
	DelayThread(1);	//tSKL

	return	iRet;
}


//Put address(6bit)

static void
EEPROMPutAddr(SMap* pSMap,u8 u8Addr)
{
	int	iA;

	u8Addr&=0x3f;
	for	(iA=0;iA<6;++iA)
	{
		EEPROMClockDataOut(pSMap,(u8Addr&0x20) ? 1:0);
		u8Addr<<=1;
	}
}


//Get data(16bit)

static u16
GetDataFromEEPROM(SMap* pSMap)
{
	int	iA;
	u16	u16Data=0;

	for	(iA=0;iA<16;++iA)
	{
		u16Data<<=1;
		u16Data|=EEPROMClockDataIn(pSMap);
	}
	return	u16Data;
}


//Instruction start(rise S, put start bit, op code)

static void
EEPROMStartOp(SMap* pSMap,int iOp)
{

	//Set port direction.

	SMAP_REG8(pSMap,SMAP_PIOPORT_DIR)=(PP_SCLK|PP_CSEL|PP_DIN);

	//Rise chip select.

	SMAP_PP_SET_S(pSMap,0);
	SMAP_PP_SET_D(pSMap,0);
	SMAP_PP_CLK_OUT(pSMap,0);
	DelayThread(1);	//tSKS

	SMAP_PP_SET_S(pSMap,1);
	SMAP_PP_SET_D(pSMap,0);
	SMAP_PP_CLK_OUT(pSMap,0);
	DelayThread(1);	//tCSS

	//Put start bit.

	EEPROMClockDataOut(pSMap,1);

	//Put op code.

	EEPROMClockDataOut(pSMap,(iOp>>1)&1);
	EEPROMClockDataOut(pSMap,iOp&1);
}


//Chip select low

static void
EEPROMCSLow(SMap* pSMap)
{
	SMAP_PP_SET_S(pSMap,0);
	SMAP_PP_SET_D(pSMap,0);
	SMAP_PP_CLK_OUT(pSMap,0);
	DelayThread(2);	//tSLSH
}


//EEPROM instruction

static void
ReadEEPROMExec(SMap* pSMap,u8 u8Addr,u16* pu16Data,int iN)
{
	int	iA;

	EEPROMStartOp(pSMap,PP_OP_READ);
	EEPROMPutAddr(pSMap,u8Addr);
	for	(iA=0;iA<iN;++iA)
	{
		*pu16Data++=GetDataFromEEPROM(pSMap);
	}
	EEPROMCSLow(pSMap);
}

static void ReadFromEEPROM ( SMap* pSMap,u8 u8Addr,u16* pu16Data,int iN ) {

 int lFlags;

 CpuSuspendIntr ( &lFlags );
  ReadEEPROMExec ( pSMap, u8Addr, pu16Data, iN );
 CpuResumeIntr( lFlags );

}  /* end ReadFromEEPROM */

static int
GetNodeAddr(SMap* pSMap)
{
	int	iA;
	u16*	pu16MAC=(u16*)pSMap->au8HWAddr;
	u16	u16CHKSum;
	u16	u16Sum=0;

	ReadFromEEPROM(pSMap,0x0,pu16MAC,3);
	ReadFromEEPROM(pSMap,0x3,&u16CHKSum,1);

	for	(iA=0;iA<3;++iA)
	{
		u16Sum+=*pu16MAC++;
	}
	if	(u16Sum!=u16CHKSum)
	{
		memset(pSMap->au8HWAddr,0,6);
		return	-1;
	}
	return	0;
}

static void BaseInit ( SMap* apSMap ) {

 apSMap -> pu8Base = ( u8 volatile* )SMAP_BASE;
 apSMap -> TX.pBD  = ( SMapBD* )( apSMap -> pu8Base + SMAP_BD_BASE_TX );
 apSMap -> pRXBD   = ( SMapBD* )( apSMap -> pu8Base + SMAP_BD_BASE_RX );

}  /* end BaseInit */

u8 const*
SMap_GetMACAddress(void)
{
	SMap*		pSMap=&SMap0;

	return	pSMap->au8HWAddr;
}

int SMap_GetIRQ ( void ) {
	SMap*		pSMap=&SMap0;

	return	SMAP_REG16(pSMap,SMAP_INTR_STAT)&INTR_BITMSK;
}


void
SMap_ClearIRQ(int iFlags)
{
	SMap*		pSMap=&SMap0;

	SMAP_REG16(pSMap,SMAP_INTR_CLR)=iFlags&INTR_BITMSK;
	if	(iFlags&INTR_EMAC3)
	{
		EMAC3REG_WRITE(pSMap,SMAP_EMAC3_INTR_STAT,E3_INTR_ALL);
	}
}


int
SMap_Init(void)
{
	SMap*		pSMap=&SMap0;

	memset(pSMap,0,sizeof(*pSMap));

	BaseInit(pSMap);
	if	(GetNodeAddr(pSMap)<0)
	{
		return	FALSE;
	}

	Reset(pSMap,RESET_INIT);
	TXRXEnable(pSMap,DISABLE);

	pSMap -> TX.u16PTRStart  = 0;
	pSMap -> TX.u16PTREnd    = 0;
	pSMap -> TX.u8IndexStart = 0;
	pSMap -> TX.u8IndexEnd   = 0;
	pSMap -> u16RXPTR        = 0;
	pSMap -> u8RXIndex       = 0;
    _smap_bd_init ();

	if	(pSMap->u32Flags&SMAP_F_PRINT_MSG)
	{
		dbgprintf("SMap_Init: PlayStation 2 SMAP open\n");
	}

	if	(!(pSMap->u32Flags&SMAP_F_LINKVALID))
	{
		dbgprintf("SMap_Init: Waiting for link to stabilize...\n");
//		XXX: we have to add a linkvalid thread (chk linux smap.c)!!!!
//		while (!(smap->u32Flags & SMAP_F_LINKVALID)) {
//		delay();
	}
	FIFOReset(pSMap);
	EMAC3ReInit(pSMap);
	pSMap -> TX.u16PTRStart  = 0;
	pSMap -> TX.u16PTREnd    = 0;
	pSMap -> TX.u8IndexStart = 0;
	pSMap -> TX.u8IndexEnd   = 0;
	pSMap -> u16RXPTR        = 0;
	pSMap -> u8RXIndex       = 0;
    _smap_bd_init ();
	return	TRUE;
}


void
SMap_Start(void)
{
	SMap*		pSMap=&SMap0;

	if (pSMap->u32Flags&SMAP_F_OPENED)
	{
		return;
	}

	SMap_ClearIRQ(INTR_BITMSK);
	TXRXEnable(pSMap,ENABLE);
	dev9IntrEnable ( INTR_BITMSK );

	pSMap->u32Flags|=SMAP_F_OPENED;
}


void
SMap_Stop(void)
{
	SMap*		pSMap=&SMap0;

	TXRXEnable(pSMap,DISABLE);

	dev9IntrDisable(INTR_BITMSK);
	SMap_ClearIRQ(INTR_BITMSK);

	pSMap->u32Flags&=~SMAP_F_OPENED;
}

extern void SMAP_CopyToFIFO ( SMap*, u32*, int );

static int inline IsEMACReady ( SMap* apSMap ) {
 return !(  EMAC3REG_READ( apSMap, SMAP_EMAC3_TxMODE0 ) & E3_TX_GNP_0  );
}  /* end IsEMACReady */

SMapStatus SMap_Send ( struct pbuf* apPacket ) {

 static u32 sl_TXBuf[ ( SMAP_TXMAXSIZE + SMAP_TXMAXTAILPAD + 3 ) / 4 ]; 

 SMap*   lpSMap    = &SMap0;
 SMapBD* pTXBD     = &lpSMap -> TX.pBD[ lpSMap -> TX.u8IndexEnd ];
 int     iTotalLen = apPacket -> tot_len;
 int     lTXLen;
 u32*    lpSrc;

 if	(  !( lpSMap -> u32Flags & SMAP_F_LINKVALID )  ) return SMap_Con;

 if	( iTotalLen > SMAP_TXMAXSIZE ) return SMap_Err;

 lTXLen = ( iTotalLen + 3 ) & ~3;

 if (  lTXLen > ComputeFreeSize ( &lpSMap -> TX )  ) return SMap_TX;
 if (  !IsEMACReady ( lpSMap )                     ) return SMap_TX;

 if ( !apPacket -> next )
  lpSrc = apPacket -> payload;
 else {
  u8* lpDst = ( u8* )sl_TXBuf;
  while ( apPacket ) {
   int lLen = apPacket -> len;
   memcpy ( lpDst, apPacket -> payload, lLen );
   lpDst   += apPacket -> len;
   apPacket = apPacket -> next;
  }  /* end while */
  lpSrc = sl_TXBuf;
 }  /* end else */

 SMAP_CopyToFIFO ( lpSMap, lpSrc, lTXLen );

 pTXBD -> length  = iTotalLen;
 pTXBD -> pointer = lpSMap -> TX.u16PTREnd + SMAP_TXBUFBASE;
 SMAP_REG8( lpSMap, SMAP_TXFIFO_FRAME_INC ) = 1;
 pTXBD -> ctrl_stat = SMAP_BD_TX_READY | SMAP_BD_TX_GENFCS | SMAP_BD_TX_GENPAD;
 EMAC3REG_WRITE( lpSMap, SMAP_EMAC3_TxMODE0, E3_TX_GNP_0 );

 lpSMap -> TX.u16PTREnd = ( lpSMap -> TX.u16PTREnd + lTXLen ) % SMAP_TXBUFSIZE;
 SMAP_BD_NEXT( lpSMap -> TX.u8IndexEnd );

 return SMap_OK;

}  /* end SMap_Send */

int SMap_HandleTXInterrupt ( int iFlags ) {

 SMap* lpSMap   = &SMap0;
 int   iNoError = 1;

 SMap_ClearIRQ ( iFlags );

 while ( lpSMap -> TX.u8IndexStart != lpSMap -> TX.u8IndexEnd ) {

  SMapBD* pBD     = &lpSMap -> TX.pBD[ lpSMap -> TX.u8IndexStart ];
  int     iStatus = pBD -> ctrl_stat;

  if ( iStatus & SMAP_BD_TX_ERRMASK ) iNoError = 0;
  if ( iStatus & SMAP_BD_TX_READY   ) goto end;

  lpSMap -> TX.u16PTRStart = (  lpSMap -> TX.u16PTRStart + (  ( pBD -> length + 3 ) & ~3  )   ) % SMAP_TXBUFSIZE;
  SMAP_BD_NEXT( lpSMap -> TX.u8IndexStart );

  pBD -> length    = 0;
  pBD -> pointer   = 0;
  pBD -> ctrl_stat = 0;

 }  /* end while */

 lpSMap -> TX.u16PTRStart = lpSMap -> TX.u16PTREnd;
end:
 return iNoError;

}  /* end SMap_HandleTXInterrupt */

int SMap_HandleRXEMACInterrupt ( int iFlags ) {

 int   iNoError = 1;
 SMap* lpSMap   = &SMap0;

 if (  iFlags & ( INTR_RXDNV | INTR_RXEND )  ) {

  iFlags &= INTR_RXDNV | INTR_RXEND;
  SMap_ClearIRQ ( iFlags );

  while ( 1 ) {

   SMapBD* pRXBD   = &lpSMap -> pRXBD[ lpSMap -> u8RXIndex ];
   int     iStatus = pRXBD -> ctrl_stat;
   int     iPKTLen;

   if ( iStatus & SMAP_BD_RX_EMPTY ) break;

   iPKTLen = pRXBD -> length;

   if (  !( iStatus & SMAP_BD_RX_ERRMASK ) && ( iPKTLen >= SMAP_RXMINSIZE && iPKTLen <= SMAP_RXMAXSIZE )  ) {

    struct pbuf* pBuf = ( struct pbuf* )pbuf_alloc ( PBUF_RAW, iPKTLen, PBUF_POOL );

    if ( pBuf ) {

     lpSMap -> u16RXPTR = (  ( pRXBD -> pointer - SMAP_RXBUFBASE ) % SMAP_RXBUFSIZE  ) & ~3;

     SMAP_CopyFromFIFO ( lpSMap, pBuf );

     ps2ip_input ( pBuf, &NIF );

    } else iNoError = 0;

   } else iNoError = 0;

   SMAP_REG8( lpSMap, SMAP_RXFIFO_FRAME_DEC ) = 1;
   pRXBD -> ctrl_stat = SMAP_BD_RX_EMPTY;
   SMAP_BD_NEXT( lpSMap -> u8RXIndex );

  }  /* end while */

  iFlags = SMap_GetIRQ ();

 }  /* end if */

 if	( iFlags & INTR_EMAC3 ) {

  u32 lSts = EMAC3REG_READ( lpSMap, SMAP_EMAC3_INTR_STAT );

  EMAC3REG_WRITE( lpSMap, SMAP_EMAC3_INTR_STAT, lSts );
  SMap_ClearIRQ ( INTR_EMAC3 );

 }  /* end if */

 return iNoError;

}  /* end SMap_HandleRXEMACInterrupt */
