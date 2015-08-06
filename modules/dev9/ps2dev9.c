/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
# DEV9 Device Driver.
*/

#include <types.h>
#include <defs.h>
#include <loadcore.h>
#include <intrman.h>
#include <dmacman.h>
#include <thbase.h>
#include <thsemap.h>
#ifdef DEBUG
#include <stdio.h>
#endif
#include <dev9.h>
#ifdef POWEROFF
#include <poweroff.h>
#endif
#ifdef DEV9X_DEV
#include <ioman.h>
#include "ioman_add.h"
#endif

//#define INIT_SMAP	1	//Disabled to avoid breaking compatibility with clone/compatible adaptors.

#include <aifregs.h>
#include <dev9regs.h>
#include <speedregs.h>
#include <smapregs.h>

#define MODNAME "dev9_driver"
#define DRIVERNAME "dev9"
IRX_ID(MODNAME, 1, 1);

#ifdef DEBUG
#define M_PRINTF(format, args...)	\
	printf(MODNAME ": " format, ## args)
#else
#define M_PRINTF(format, args...)	\
	do {} while (0)
#endif
	
#define VERSION "1.0"
#define BANNER "\nDEV9 device driver v%s - Copyright (c) 2003 Marcus R. Brown\n\n"

/* SSBUS registers.  */
#define SSBUS_R_1418		0xbf801418
#define SSBUS_R_141c		0xbf80141c
#define SSBUS_R_1420		0xbf801420

static int dev9type = -1;	/* 0 for PCMCIA, 1 for expansion bay */
#ifdef PCMCIA
static int using_aif = 0;	/* 1 if using AIF on a T10K */
#endif

static void (*p_dev9_intr_cb)(int flag) = NULL;
static int dma_lock_sem = -1;	/* used to arbitrate DMA */

#ifdef PCMCIA
static int pcic_cardtype;	/* Translated value of bits 0-1 of 0xbf801462 */
static int pcic_voltage;	/* Translated value of bits 2-3 of 0xbf801462 */
#endif

static s16 eeprom_data[5];	/* 2-byte EEPROM status (0/-1 = invalid, 1 = valid),
				   6-byte MAC address,
				   2-byte MAC address checksum.  */

/* Each driver can register callbacks that correspond to each bit of the
   SMAP interrupt status register (0xbx000028).  */
static dev9_intr_cb_t dev9_intr_cbs[16];

static dev9_shutdown_cb_t dev9_shutdown_cbs[16];

static dev9_dma_cb_t dev9_predma_cbs[4], dev9_postdma_cbs[4];

static int dev9_intr_dispatch(int flag);

#ifdef POWEROFF
static void dev9x_on_shutdown(void*);
#endif

static void dev9_set_stat(int stat);
static int read_eeprom_data(void);

static int dev9_device_probe(void);
static int dev9_device_reset(void);
static int dev9_init(void);
static int speed_device_init(void);

#ifdef PCMCIA
static void pcmcia_set_stat(int stat);
static int pcic_ssbus_mode(int mode);
static int pcmcia_device_probe(void);
static int pcmcia_device_reset(void);
static int card_find_manfid(u32 manfid);
static int pcmcia_init(void);
#endif

static void expbay_set_stat(int stat);
static int expbay_device_probe(void);
static int expbay_device_reset(void);
static int expbay_init(void);

struct irx_export_table _exp_dev9;

#ifdef DEV9X_DEV
static int dev9x_dummy(void) { return 0; }
static int dev9x_devctl(iop_file_t *f, const char *name, int cmd, void *args, int arglen, void *buf, int buflen)
{
	if (cmd == 0x4401)
		return dev9type;

	return 0;
}

/* driver ops func tab */
static void *dev9x_ops[27] = {
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_devctl,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy,
	(void*)dev9x_dummy
};

/* driver descriptor */
static iop_ext_device_t dev9x_dev = {
	"dev9x", 
	IOP_DT_FS | 0x10000000, /* EXT FS */
	1,
	"DEV9",
	(struct _iop_ext_device_ops *)&dev9x_ops
};
#endif

int _start(int argc, char **argv)
{
	USE_DEV9_REGS;
	int idx, res = 1;
	u16 dev9hw;

	M_PRINTF(BANNER, VERSION);

#ifdef CHECK_LOADED
	iop_library_t *libptr;	
	libptr = GetLoadcoreInternalData()->let_next;
	while ((libptr != 0))
	{
		int i;
		for (i = 0; i <= sizeof(DRIVERNAME); i++) {
			if (libptr->name[i] != DRIVERNAME[i])
				break;
		}
		if (i > sizeof(DRIVERNAME)) {
			M_PRINTF("Driver already loaded.\n");
			return 1;
		}
		libptr = libptr->prev;
	}
#endif

	for (idx = 0; idx < 16; idx++)
		dev9_shutdown_cbs[idx] = NULL;

	dev9hw = DEV9_REG(DEV9_R_REV) & 0xf0;
	if (dev9hw == 0x20) {		/* CXD9566 (PCMCIA) */
		dev9type = 0;
#ifdef PCMCIA
		res = pcmcia_init();
#else
		return 1;
#endif
	} else if (dev9hw == 0x30) {	/* CXD9611 (Expansion Bay) */
		dev9type = 1;
		res = expbay_init();
	}

	if (res)
		return res;

#ifdef DEV9X_DEV
	DelDrv("dev9x");
	AddDrv((iop_device_t *)&dev9x_dev);
#endif

	if (RegisterLibraryEntries(&_exp_dev9) != 0) {
		return 1;
	}
#ifdef POWEROFF
	AddPowerOffHandler(dev9x_on_shutdown, 0);
#endif
	/* Normal termination.  */
	M_PRINTF("Dev9 loaded.\n");

	return 0;
}

int _exit(void) { return 0; }

/* Export 4 */
void dev9RegisterIntrCb(int intr, dev9_intr_cb_t cb)
{
	dev9_intr_cbs[intr] = cb;
}

/* Export 12 */
void dev9RegisterPreDmaCb(int ctrl, dev9_dma_cb_t cb){
	dev9_predma_cbs[ctrl] = cb;
}

/* Export 13 */
void dev9RegisterPostDmaCb(int ctrl, dev9_dma_cb_t cb){
	dev9_postdma_cbs[ctrl] = cb;
}

/* flag is 1 if a card (pcmcia) was removed or added */
static int dev9_intr_dispatch(int flag)
{
	USE_SPD_REGS;
	int i, bit;

	if (flag) {
		for (i = 0; i < 16; i++)
			if (dev9_intr_cbs[i] != NULL)
				dev9_intr_cbs[i](flag);
	}

	while (SPD_REG16(SPD_R_INTR_STAT) & SPD_REG16(SPD_R_INTR_MASK)) {
		for (i = 0; i < 16; i++) {
			if (dev9_intr_cbs[i] != NULL) {
				bit = (SPD_REG16(SPD_R_INTR_STAT) &
					SPD_REG16(SPD_R_INTR_MASK)) >> i;
				if (bit & 0x01)
					dev9_intr_cbs[i](flag);
			}
		}
	}

	return 0;
}

static void dev9_set_stat(int stat)
{
	if (dev9type == 0)
#ifdef PCMCIA
		pcmcia_set_stat(stat);
#else
		return;
#endif
	else if (dev9type == 1)
		expbay_set_stat(stat);
}

static int dev9_device_probe(void)
{
	if (dev9type == 0)
#ifdef PCMCIA
		return pcmcia_device_probe();
#else
		return -1;
#endif
	else if (dev9type == 1)
		return expbay_device_probe();

	return -1;
}

static int dev9_device_reset(void)
{
	if (dev9type == 0)
#ifdef PCMCIA
		return pcmcia_device_reset();
#else
		return -1;
#endif
	else if (dev9type == 1)
		return expbay_device_reset();

	return -1;
}

/* Export 6 */
void dev9Shutdown(void)
{
	int idx;
	USE_DEV9_REGS;

	for (idx = 0; idx < 16; idx++)
		if (dev9_shutdown_cbs[idx])
			dev9_shutdown_cbs[idx]();

	if (dev9type == 0) {	/* PCMCIA */
#ifdef PCMCIA
		DEV9_REG(DEV9_R_POWER) = 0;
		DEV9_REG(DEV9_R_1474) = 0;
#endif
	} else if (dev9type == 1) {
		DEV9_REG(DEV9_R_1466) = 1;
		DEV9_REG(DEV9_R_1464) = 0;
		DEV9_REG(DEV9_R_1460) = DEV9_REG(DEV9_R_1464);
		DEV9_REG(DEV9_R_POWER) = DEV9_REG(DEV9_R_POWER) & ~4;
		DEV9_REG(DEV9_R_POWER) = DEV9_REG(DEV9_R_POWER) & ~1;
	}
	DelayThread(1000000);
}

/* Export 7 */
void dev9IntrEnable(int mask)
{
	USE_SPD_REGS;
	int flags;

	CpuSuspendIntr(&flags);
	SPD_REG16(SPD_R_INTR_MASK) = SPD_REG16(SPD_R_INTR_MASK) | mask;
	CpuResumeIntr(flags);
}

/* Export 8 */
void dev9IntrDisable(int mask)
{
	USE_SPD_REGS;
	int flags;

	CpuSuspendIntr(&flags);
	SPD_REG16(SPD_R_INTR_MASK) = SPD_REG16(SPD_R_INTR_MASK) & ~mask;
	CpuResumeIntr(flags);
}

/* Export 5 */
int dev9DmaTransfer(int ctrl, void *buf, int bcr, int dir)
{
	USE_SPD_REGS;
	int res = 0, dmactrl;

	switch(ctrl){
	case 0:
	case 1:	dmactrl = ctrl;
		break;

	case 2:
	case 3:
		if (dev9_predma_cbs[ctrl] == NULL)	return -1;
		if (dev9_postdma_cbs[ctrl] == NULL)	return -1;
		dmactrl = (4 << ctrl);
		break;

	default:
		return -1;
	}

	if ((res = WaitSema(dma_lock_sem)) < 0)
		return res;

	SPD_REG16(SPD_R_DMA_CTRL) = (SPD_REG16(SPD_R_REV_1)<17)?(dmactrl&0x03)|0x04:(dmactrl&0x01)|0x06;

	if (dev9_predma_cbs[ctrl])
		dev9_predma_cbs[ctrl](bcr, dir);

	dmac_request(IOP_DMAC_DEV9, buf, bcr&0xFFFF, bcr>>16, dir);
	dmac_transfer(IOP_DMAC_DEV9);

	/* Wait for DMA to complete. Do not use a semaphore as thread switching hurts throughput greatly.  */
	while(dmac_ch_get_chcr(IOP_DMAC_DEV9)&DMAC_CHCR_TR){}
	res = 0;

	if (dev9_postdma_cbs[ctrl])
		dev9_postdma_cbs[ctrl](bcr, dir);

	SignalSema(dma_lock_sem);
	return res;
}

static int read_eeprom_data(void)
{
	USE_SPD_REGS;
	int i, j, res = -2;
	u8 val;

	if (eeprom_data[0] < 0)
		goto out;

	SPD_REG8(SPD_R_PIO_DIR)  = 0xe1;
	DelayThread(1);
	SPD_REG8(SPD_R_PIO_DATA) = 0x80;
	DelayThread(1);

	for (i = 0; i < 2; i++) {
		SPD_REG8(SPD_R_PIO_DATA) = 0xa0;
		DelayThread(1);
		SPD_REG8(SPD_R_PIO_DATA) = 0xe0;
		DelayThread(1);
	}
	for (i = 0; i < 7; i++) {
		SPD_REG8(SPD_R_PIO_DATA) = 0x80;
		DelayThread(1);
		SPD_REG8(SPD_R_PIO_DATA) = 0xc0;
		DelayThread(1);
	}
	SPD_REG8(SPD_R_PIO_DATA) = 0xc0;
	DelayThread(1);

	val = SPD_REG8(SPD_R_PIO_DATA);
	DelayThread(1);
	if (val & 0x10) {	/* Error.  */
		SPD_REG8(SPD_R_PIO_DATA) = 0;
		DelayThread(1);
		res = -1;
		eeprom_data[0] = 0;
		goto out;
	}

	SPD_REG8(SPD_R_PIO_DATA) = 0x80;
	DelayThread(1);

	/* Read the MAC address and checksum from the EEPROM.  */
	for (i = 0; i < 4; i++) {
		eeprom_data[i+1] = 0;

		for (j = 15; j >= 0; j--) {
			SPD_REG8(SPD_R_PIO_DATA) = 0xc0;
			DelayThread(1);
			val = SPD_REG8(SPD_R_PIO_DATA);
			if (val & 0x10)
				eeprom_data[i+1] |= (1<<j);
			SPD_REG8(SPD_R_PIO_DATA) = 0x80;
			DelayThread(1);
		}
	}

	SPD_REG8(SPD_R_PIO_DATA) = 0;
	DelayThread(1);
	eeprom_data[0] = 1;	/* The EEPROM data is valid.  */
	res = 0;

out:
	SPD_REG8(SPD_R_PIO_DIR) = 1;
	return res;
}

/* Export 9 */
int dev9GetEEPROM(u16 *buf)
{
	int i;

	if (!eeprom_data[0])
		return -1;
	if (eeprom_data[0] < 0)
		return -2;

	/* We only return the MAC address and checksum.  */
	for (i = 0; i < 4; i++)
		buf[i] = eeprom_data[i+1];

	return 0;
}

/* Export 10 */
void dev9LEDCtl(int ctl)
{
	USE_SPD_REGS;
	SPD_REG8(SPD_R_PIO_DATA) = (ctl == 0);
}

/* Export 11 */
int dev9RegisterShutdownCb(int idx, dev9_shutdown_cb_t cb){
	if (idx < 16)
	{
		dev9_shutdown_cbs[idx] = cb;
		return 0;
	}
	return -1;
}

static int dev9_init(void)
{
	int i;

	if ((dma_lock_sem = CreateMutex(IOP_MUTEX_UNLOCKED)) < 0)
		return -1;

	/* Enable the DEV9 DMAC channel.  */
	dmac_enable(IOP_DMAC_DEV9);

	/* Not quite sure what this enables yet.  */
	dev9_set_stat(0x103);

	/* Disable all device interrupts.  */
	dev9IntrDisable(0xffff);

	p_dev9_intr_cb = (void *)dev9_intr_dispatch;

	/* Reset the SMAP interrupt callback table. */
	for (i = 0; i < 16; i++)
		dev9_intr_cbs[i] = NULL;

	for (i = 0; i < 4; i++){
		dev9_predma_cbs[i] = NULL;
		dev9_postdma_cbs[i] = NULL;
	}

	/* Read in the MAC address.  */
	read_eeprom_data();
	/* Turn the LED off.  */
	dev9LEDCtl(0);
	return 0;
}

#ifdef INIT_SMAP
static int dev9_smap_read_phy(volatile u8 *emac3_regbase, unsigned int address, unsigned int *data){
	unsigned int i, PHYRegisterValue;
	int result;

	PHYRegisterValue=(address&SMAP_E3_PHY_REG_ADDR_MSK)|SMAP_E3_PHY_READ|((SMAP_DsPHYTER_ADDRESS&SMAP_E3_PHY_ADDR_MSK)<<SMAP_E3_PHY_ADDR_BITSFT);

	i=0;
	result=0;
	SMAP_EMAC3_SET(SMAP_R_EMAC3_STA_CTRL, PHYRegisterValue);

	do{
		if(SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL)&SMAP_E3_PHY_OP_COMP){
			if(SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL)&SMAP_E3_PHY_OP_COMP){
				if((result=SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL))&SMAP_E3_PHY_OP_COMP){
					result>>=SMAP_E3_PHY_DATA_BITSFT;
					break;
				}
			}
		}

		DelayThread(1000);
		i++;
	}while(i<100);

	if(i>=100){
		return 1;
	}else{
		*data = result;
		return 0;
	}
}

static int dev9_smap_write_phy(volatile u8 *emac3_regbase, unsigned char address, unsigned short int value){
	unsigned int i, PHYRegisterValue;

	PHYRegisterValue=(address&SMAP_E3_PHY_REG_ADDR_MSK)|SMAP_E3_PHY_WRITE|((SMAP_DsPHYTER_ADDRESS&SMAP_E3_PHY_ADDR_MSK)<<SMAP_E3_PHY_ADDR_BITSFT);
	PHYRegisterValue|=((unsigned int)value)<<SMAP_E3_PHY_DATA_BITSFT;

	i=0;
	SMAP_EMAC3_SET(SMAP_R_EMAC3_STA_CTRL, PHYRegisterValue);

	for(; !(SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL)&SMAP_E3_PHY_OP_COMP); i++){
		DelayThread(1000);
		if(i>=100) break;
	}

	return((i>=100)?1:0);
}

static int dev9_smap_init(void)
{
	unsigned int value;
	USE_SPD_REGS;
	USE_SMAP_REGS;
	USE_SMAP_EMAC3_REGS;
	USE_SMAP_TX_BD;
	USE_SMAP_RX_BD;
	int i;

	//Do not perform SMAP initialization if the SPEED device does not have such an interface.
	if (!(SPD_REG16(SPD_R_REV_3)&SPD_CAPS_SMAP)) return 0;

	SMAP_REG8(SMAP_R_TXFIFO_CTRL) = SMAP_TXFIFO_RESET;
	for(i = 9; SMAP_REG8(SMAP_R_TXFIFO_CTRL)&SMAP_TXFIFO_RESET; i--)
	{
		if (i <= 0) return 1;
		DelayThread(1000);
	}

	SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_RESET;
	for(i = 9; SMAP_REG8(SMAP_R_RXFIFO_CTRL)&SMAP_RXFIFO_RESET; i--)
	{
		if (i <= 0) return 1;
		DelayThread(1000);
	}

	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_SOFT_RESET);
	for (i = 9; SMAP_EMAC3_GET(SMAP_R_EMAC3_MODE0)&SMAP_E3_SOFT_RESET; i--)
	{
		if (i <= 0) return 3;
		DelayThread(1000);
	}

	if (SPD_REG16(SPD_R_REV_1) >= 0x11) SMAP_REG8(SMAP_R_BD_MODE) = SMAP_BD_SWAP;

	for(i = 0; i < SMAP_BD_MAX_ENTRY; i++)
	{
		tx_bd[i].ctrl_stat = 0;
		tx_bd[i].reserved = 0;
		tx_bd[i].length = 0;
		tx_bd[i].pointer = 0;
	}

	for(i = 0; i < SMAP_BD_MAX_ENTRY; i++)
	{
		rx_bd[i].ctrl_stat = 0x80;	//SMAP_BD_RX_EMPTY
		rx_bd[i].reserved = 0;
		rx_bd[i].length = 0;
		rx_bd[i].pointer = 0;
	}

	SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_BITMSK;
	if (SPD_REG16(SPD_R_REV_1) < 0x11) SPD_REG8(0x100) = 1;

	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE1, SMAP_E3_FDX_ENABLE|SMAP_E3_IGNORE_SQE|SMAP_E3_MEDIA_100M|SMAP_E3_RXFIFO_2K|SMAP_E3_TXFIFO_1K|SMAP_E3_TXREQ0_MULTI|SMAP_E3_TXREQ1_SINGLE);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE1, 7<<SMAP_E3_TX_LOW_REQ_BITSFT | 0xF<<SMAP_E3_TX_URG_REQ_BITSFT);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_RxMODE, SMAP_E3_RX_RX_RUNT_FRAME|SMAP_E3_RX_RX_FCS_ERR|SMAP_E3_RX_RX_TOO_LONG_ERR|SMAP_E3_RX_RX_IN_RANGE_ERR|SMAP_E3_RX_PROP_PF|SMAP_E3_RX_PROMISC);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_STAT, SMAP_E3_INTR_TX_ERR_0|SMAP_E3_INTR_SQE_ERR_0|SMAP_E3_INTR_DEAD_0);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_ENABLE, SMAP_E3_INTR_TX_ERR_0|SMAP_E3_INTR_SQE_ERR_0|SMAP_E3_INTR_DEAD_0);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_ADDR_HI, 0);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_ADDR_LO, 0);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_PAUSE_TIMER, 0xFFFF);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_INTER_FRAME_GAP, 4);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_TX_THRESHOLD, 0xC<<SMAP_E3_TX_THRESHLD_BITSFT);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_RX_WATERMARK, 0x8<<SMAP_E3_RX_LO_WATER_BITSFT|0x40<<SMAP_E3_RX_HI_WATER_BITSFT);

	dev9_smap_write_phy(emac3_regbase, SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_RST);
	for (i = 9;; i--)
	{
		if (dev9_smap_read_phy(emac3_regbase, SMAP_DsPHYTER_BMCR, &value)) return 4;
		if (!(value & SMAP_PHY_BMCR_RST)) break;
		if (i <= 0) return 5;
	}

	dev9_smap_write_phy(emac3_regbase, SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_LPBK|SMAP_PHY_BMCR_100M|SMAP_PHY_BMCR_DUPM);
	DelayThread(10000);
	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_TXMAC_ENABLE|SMAP_E3_RXMAC_ENABLE);
	value = SMAP_REG16(SMAP_R_TXFIFO_WR_PTR) + SMAP_TX_BASE;

	for(i=0; i<0x5EA; i+=4) SMAP_REG32(SMAP_R_TXFIFO_DATA) = i;

	tx_bd[0].length = 0xEA05;
	tx_bd[0].pointer = (value >> 8) | (value << 8);
	SMAP_REG8(SMAP_R_TXFIFO_FRAME_INC) = 0;
	tx_bd[0].ctrl_stat = 0x83;	//SMAP_BD_TX_READY|SMAP_BD_TX_GENFCS|SMAP_BD_TX_GENPAD

	SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE0, SMAP_E3_TX_GNP_0);
	for (i = 9;; i--)
	{
		value = SPD_REG16(SPD_R_INTR_STAT);

		if ((value & (SMAP_INTR_RXEND|SMAP_INTR_TXEND|SMAP_INTR_TXDNV)) == (SMAP_INTR_RXEND|SMAP_INTR_TXEND|SMAP_INTR_TXDNV)) break;
		if (i <= 0) return 6;
		DelayThread(1000);
	}
	SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_SOFT_RESET);

	return 0;
}
#endif

static int speed_device_init(void)
{
#ifdef DEBUG
	USE_SPD_REGS;
	const char *spdnames[] = { "(unknown)", "TS", "ES1", "ES2" };
	int idx;
	u16 spdrev;
#endif
	int res;
#ifdef INIT_SMAP
	int InitCount;
#endif

	eeprom_data[0] = 0;

#ifdef INIT_SMAP
	for(InitCount=0; InitCount<8; InitCount++){
#endif
		if (dev9_device_probe() < 0) {
			M_PRINTF("PC card or expansion device isn't connected.\n");
			return -1;
		}

		dev9_device_reset();

#ifdef PCMCIA
		/* Locate the SPEED Lite chip and get the bus ready for the
		   PCMCIA device.  */
		if (dev9type == 0) {
			if ((res = card_find_manfid(0xf15300)))
				M_PRINTF("SPEED Lite not found.\n");

			if (!res && (res = pcic_ssbus_mode(5)))
				M_PRINTF("Unable to change SSBUS mode.\n");

			if (res) {
				dev9Shutdown();
				return -1;
			}
		}
#endif

#ifdef INIT_SMAP
		if((res = dev9_smap_init()) == 0){
			break;
		}

		dev9Shutdown();
		DelayThread(4500000);
	}

	if (res) {
#ifdef DEBUG
		M_PRINTF("SMAP initialization failed: %d\n", res);
#endif
		eeprom_data[0] = -1;
	}
#endif

#ifdef DEBUG
	/* Print out the SPEED chip revision.  */
	spdrev = SPD_REG16(SPD_R_REV_1);
	idx    = (spdrev & 0xffff) - 14;
	if (spdrev == 9)
		idx = 1;	/* TS */
	else if (spdrev < 9 || (spdrev < 16 || spdrev > 17))
		idx = 0;	/* Unknown revision */

	M_PRINTF("SPEED chip '%s', revision 0x%0X\n", spdnames[idx], spdrev);
#endif

	return 0;
}

#ifdef PCMCIA
static int pcic_get_cardtype(void)
{
	USE_DEV9_REGS;
	u16 val = DEV9_REG(DEV9_R_1462) & 0x03;

	if (val == 0)
		return 1;	/* 16-bit */
	else
	if (val < 3)
		return 2;	/* CardBus */
	return 0;
}

static int pcic_get_voltage(void)
{
	USE_DEV9_REGS;
	u16 val = DEV9_REG(DEV9_R_1462) & 0x0c;

	if (val == 0x04)
		return 3;
	if (val == 0 || val == 0x08)
		return 1;
	if (val == 0x0c)
		return 2;
	return 0;
}

static int pcic_power(int voltage, int flag)
{
	USE_DEV9_REGS;
	u16 cstc1, cstc2;
	u16 val = (voltage == 1) << 2;

	DEV9_REG(DEV9_R_POWER) = 0;

	if (voltage == 2)
		val |= 0x08;
	if (flag == 1)
		val |= 0x10;

	DEV9_REG(DEV9_R_POWER) = val;
	DelayThread(22000);

	if (DEV9_REG(DEV9_R_1462) & 0x100)
		return 0;

	DEV9_REG(DEV9_R_POWER) = 0;
	DEV9_REG(DEV9_R_1464) = cstc1 = DEV9_REG(DEV9_R_1464);
	DEV9_REG(DEV9_R_1466) = cstc2 = DEV9_REG(DEV9_R_1466);
	return -1;
}

static void pcmcia_set_stat(int stat)
{
	USE_DEV9_REGS;
	u16 val = stat & 0x01;

	if (stat & 0x10)
		val = 1;
	if (stat & 0x02)
		val |= 0x02;
	if (stat & 0x20)
		val |= 0x02;
	if (stat & 0x04)
		val |= 0x08;
	if (stat & 0x08)
		val |= 0x10;
	if (stat & 0x200)
		val |= 0x20;
	if (stat & 0x100)
		val |= 0x40;
	if (stat & 0x400)
		val |= 0x80;
	if (stat & 0x800)
		val |= 0x04;
	DEV9_REG(DEV9_R_1476) = val & 0xff;
}

static int pcic_ssbus_mode(int mode)
{
	USE_DEV9_REGS;
	USE_SPD_REGS;
	u16 stat = DEV9_REG(DEV9_R_1474) & 7;

	if (mode != 3 && mode != 5)
		return -1;

	DEV9_REG(DEV9_R_1460) = 2;
	if (stat)
		return -1;

	if (mode == 3) {
		DEV9_REG(DEV9_R_1474) = 1;
		DEV9_REG(DEV9_R_1460) = 1;
		SPD_REG8(0x20) = 1;
		DEV9_REG(DEV9_R_1474) = mode;
	} else if (mode == 5) {
		DEV9_REG(DEV9_R_1474) = mode;
		DEV9_REG(DEV9_R_1460) = 1;
		SPD_REG8(0x20) = 1;
		DEV9_REG(DEV9_R_1474) = 7;
	}
	_sw(0xe01a3043, SSBUS_R_1418);

	DelayThread(5000);
	DEV9_REG(DEV9_R_POWER) = DEV9_REG(DEV9_R_POWER) & ~1;
	return 0;
}

static int pcmcia_device_probe(void)
{
#ifdef DEBUG
	const char *pcic_ct_names[] = { "No", "16-bit", "CardBus" };
#endif
	int voltage;

	pcic_voltage = pcic_get_voltage();
	pcic_cardtype = pcic_get_cardtype();
	voltage = (pcic_voltage == 2 ? 5 : (pcic_voltage == 1 ? 3 : 0));

	M_PRINTF("%s PCMCIA card detected. Vcc = %dV\n",
			pcic_ct_names[pcic_cardtype], voltage);

	if (pcic_voltage == 3 || pcic_cardtype != 1)
		return -1;

	return 0;
}

static int pcmcia_device_reset(void)
{
	USE_DEV9_REGS;
	u16 cstc1, cstc2;

	/* The card must be 16-bit (type 2?) */
	if (pcic_cardtype != 1)
		return -1;

	DEV9_REG(DEV9_R_147E) = 1;
	if (pcic_power(pcic_voltage, 1) < 0)
		return -1;

	DEV9_REG(DEV9_R_POWER) = DEV9_REG(DEV9_R_POWER) | 0x02;
	DelayThread(500000);

	DEV9_REG(DEV9_R_POWER) = DEV9_REG(DEV9_R_POWER) | 0x01;
	DEV9_REG(DEV9_R_1464) = cstc1 = DEV9_REG(DEV9_R_1464);
	DEV9_REG(DEV9_R_1466) = cstc2 = DEV9_REG(DEV9_R_1466);
	return 0;
}

static int card_find_manfid(u32 manfid)
{
	USE_DEV9_REGS;
	USE_SPD_REGS;
	u32 spdaddr, spdend, next, tuple;
	u8 hdr, ofs;

	DEV9_REG(DEV9_R_1460) = 2;
	_sw(0x1a00bb, SSBUS_R_1418);

	/* Scan the card for the MANFID tuple.  */
	spdaddr = 0;
	spdend =  0x1000;
	/* I hate this code, and it hates me.  */
	while (spdaddr < spdend) {
		hdr = SPD_REG8(spdaddr) & 0xff;
		spdaddr += 2;
		if (!hdr)
			continue;
		if (hdr == 0xff)
			break;
		if (spdaddr >= spdend)
			goto error;

		ofs = SPD_REG8(spdaddr) & 0xff;
		spdaddr += 2;
		if (ofs == 0xff)
			break;

		next = spdaddr + (ofs * 2);
		if (next >= spdend)
			goto error;

		if (hdr == 0x20) {
			if ((spdaddr + 8) >= spdend)
				goto error;

			tuple = (SPD_REG8(spdaddr + 2) << 24)|
				(SPD_REG8(spdaddr) << 16)|
				(SPD_REG8(spdaddr + 6) << 8)|
				 SPD_REG8(spdaddr + 4);
			if (manfid == tuple)
				return 0;
			M_PRINTF("MANFID 0x%08lx doesn't match expected 0x%08lx\n",
					tuple, manfid);
			return -1;
		}
		spdaddr = next;
	}

	M_PRINTF("MANFID 0x%08lx not found.\n", manfid);
	return -1;
error:
	M_PRINTF("Invalid tuples at offset 0x%08lx.\n", spdaddr - SPD_REGBASE);
	return -1;
}

static int pcmcia_intr(void *unused)
{
	USE_AIF_REGS;
	USE_DEV9_REGS;
	u16 cstc1, cstc2;

	cstc1 = DEV9_REG(DEV9_R_1464);
	cstc2 = DEV9_REG(DEV9_R_1466);

	if (using_aif) {
		if (aif_regs[AIF_INTSR] & AIF_INTR_PCMCIA)
			aif_regs[AIF_INTCL] = AIF_INTR_PCMCIA;
		else
			return 0;		/* Unknown interrupt.  */
	}

	/* Acknowledge the interrupt.  */
	DEV9_REG(DEV9_R_1464) = cstc1;
	DEV9_REG(DEV9_R_1466) = cstc2;
	if (cstc1 & 0x03 || cstc2 & 0x03) {	/* Card removed or added? */
		if (p_dev9_intr_cb)
			p_dev9_intr_cb(1);
		
		/* Shutdown the card.  */
		DEV9_REG(DEV9_R_POWER) = 0;	
		DEV9_REG(DEV9_R_1474) = 0;

		pcmcia_device_probe();
	}
	if (cstc1 & 0x80 || cstc2 & 0x80) {
		if (p_dev9_intr_cb)
			p_dev9_intr_cb(0);
	}

	DEV9_REG(DEV9_R_147E) = 1;
	DEV9_REG(DEV9_R_147E) = 0;
	return 1;
}

static int pcmcia_init(void)
{
	USE_DEV9_REGS;
	USE_AIF_REGS;
	int *mode;
	u16 cstc1, cstc2;

	_sw(0x51011, SSBUS_R_1420);
	_sw(0x1a00bb, SSBUS_R_1418);
	_sw(0xef1a3043, SSBUS_R_141c);

	/* If we are a T10K, then we go through AIF.  */
	if ((mode = QueryBootMode(6)) != NULL) {
		if ((*(u16 *)mode & 0xfe) == 0x60) {
			M_PRINTF("T10K detected.\n");

			if (aif_regs[AIF_IDENT] == 0xa1) {
				aif_regs[AIF_INTEN] = AIF_INTR_PCMCIA;
				using_aif = 1;
			} else {
				M_PRINTF("AIF not detected.\n");
				return 1;
			}
		}
	}

	if (DEV9_REG(DEV9_R_POWER) == 0) {
		DEV9_REG(DEV9_R_POWER) = 0;
		DEV9_REG(DEV9_R_147E) = 1;
		DEV9_REG(DEV9_R_1460) = 0;
		DEV9_REG(DEV9_R_1474) = 0;
		DEV9_REG(DEV9_R_1464) = cstc1 = DEV9_REG(DEV9_R_1464);
		DEV9_REG(DEV9_R_1466) = cstc2 = DEV9_REG(DEV9_R_1466);
		DEV9_REG(DEV9_R_1468) = 0x10;
		DEV9_REG(DEV9_R_146A) = 0x90;
		DEV9_REG(DEV9_R_147C) = 1;
		DEV9_REG(DEV9_R_147A) = DEV9_REG(DEV9_R_147C);

		pcic_voltage = pcic_get_voltage();
		pcic_cardtype = pcic_get_cardtype();

		if (speed_device_init() != 0)
			return 1;
	} else {
		_sw(0xe01a3043, SSBUS_R_1418);
	}

	if (dev9_init() != 0)
		return 1;

	RegisterIntrHandler(IOP_IRQ_DEV9, 1, &pcmcia_intr, NULL);
	EnableIntr(IOP_IRQ_DEV9);

	DEV9_REG(DEV9_R_147E) = 0;
	M_PRINTF("CXD9566 (PCMCIA type) initialized.\n");
	return 0;
}
#endif

static void expbay_set_stat(int stat)
{
	USE_DEV9_REGS;
	DEV9_REG(DEV9_R_1464) = stat & 0x3f;
}

static int expbay_device_probe(void)
{
	USE_DEV9_REGS;
	return (DEV9_REG(DEV9_R_1462) & 0x01) ? -1 : 0;
}

static int expbay_device_reset(void)
{
	USE_DEV9_REGS;

	if (expbay_device_probe() < 0)
		return -1;

	DEV9_REG(DEV9_R_POWER) = (DEV9_REG(DEV9_R_POWER) & ~1) | 0x04;	/* power on */
	DelayThread(500000);

	DEV9_REG(DEV9_R_1460) = DEV9_REG(DEV9_R_1460) | 0x01;
	DEV9_REG(DEV9_R_POWER) = DEV9_REG(DEV9_R_POWER) | 0x01;
	DelayThread(500000);
	return 0;
}

static int expbay_intr(void *unused)
{
	USE_DEV9_REGS;

	if (p_dev9_intr_cb)
		p_dev9_intr_cb(0);

	DEV9_REG(DEV9_R_1466) = 1;
	DEV9_REG(DEV9_R_1466) = 0;
	return 1;
}

static int expbay_init(void)
{
	USE_DEV9_REGS;

	_sw(0x51011, SSBUS_R_1420);
	_sw(0xe01a3043, SSBUS_R_1418);
	_sw(0xef1a3043, SSBUS_R_141c);

	if ((DEV9_REG(DEV9_R_POWER) & 0x04) == 0) { /* if not already powered */
		DEV9_REG(DEV9_R_1466) = 1;
		DEV9_REG(DEV9_R_1464) = 0;
		DEV9_REG(DEV9_R_1460) = DEV9_REG(DEV9_R_1464);

		if (speed_device_init() != 0)
			return 1;
	}

	if (dev9_init() != 0)
		return 1;

	RegisterIntrHandler(IOP_IRQ_DEV9, 1, expbay_intr, NULL);
	EnableIntr(IOP_IRQ_DEV9);

	DEV9_REG(DEV9_R_1466) = 0;
	M_PRINTF("CXD9611 (Expansion Bay type) initialized.\n");
	return 0;
}

#ifdef POWEROFF
static void dev9x_on_shutdown(void*p)
{
	M_PRINTF("shutdown\n");
	dev9IntrDisable(-1);
	dev9Shutdown();
}
#endif
