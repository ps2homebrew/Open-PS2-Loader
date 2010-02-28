/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: ps2dev9.c 1454 2007-11-04 23:19:57Z roman_ps2dev $
# DEV9 Device Driver.
*/

#include "types.h"
#include "defs.h"
#include "loadcore.h"
#include "intrman.h"
#include "dmacman.h"
#include "thbase.h"
#include "thsemap.h"
#ifdef DEBUG
#include "stdio.h"
#endif
#include "dev9.h"
#ifdef POWEROFF
#include "poweroff.h"
#endif
#ifdef DEV9X_DEV
#include "ioman.h"
#include "ioman_add.h"
#endif

#include "dev9regs.h"
#include "speedregs.h"
#include "smapregs.h"

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

#define DEV9_INTR		13

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
static int dma_complete_sem = -1; /* signalled on DMA transfer completion.  */

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
static int dev9_dma_intr(void *arg);

#ifdef POWEROFF
static void dev9x_on_shutdown(void*);
#endif

static void smap_set_stat(int stat);
static int read_eeprom_data(void);

static int smap_device_probe(void);
static int smap_device_reset(void);
static int smap_subsys_init(void);
static int smap_device_init(void);

#ifdef PCMCIA
static void pcmcia_set_stat(int stat);
static int pcic_ssbus_mode(int voltage);
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
int dev9x_dummy() { return 0; }
int dev9x_devctl(iop_file_t *f, const char *name, int cmd, void *args, int arglen, void *buf, int buflen)
{
	if (cmd == 0x4401)
		return dev9type;

	return 0;
}

/* driver ops func tab */
void *dev9x_ops[27] = {
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
	iop_library_table_t *libtable;
	iop_library_t *libptr;	
	libtable = GetLibraryEntryTable();
	libptr = libtable->tail;
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

int __attribute__((unused)) shutdown() { return 0; }

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

/* Signal the end of a DMA transfer.  */
static int dev9_dma_intr(void *arg)
{
	int sem = *(int *)arg;

	iSignalSema(sem);
	return 1;
}

static void smap_set_stat(int stat)
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

static int smap_device_probe()
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

static int smap_device_reset()
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
void dev9Shutdown()
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
/* This differs from the "official" dev9 in that it puts the calling thread to
   sleep when doing the actual DMA transfer.  I'm not sure why SCEI didn't do
   this in dev9.irx, when they do it in PS2Linux's dmarelay.irx.  Anyway,
   since this no longer blocks, it should speed up anything else on the IOP
   when HDD or SMAP are doing DMA.  */
int dev9DmaTransfer(int ctrl, void *buf, int bcr, int dir)
{
	USE_SPD_REGS;
	volatile iop_dmac_chan_t *dev9_chan = (iop_dmac_chan_t *)DEV9_DMAC_BASE;
	int stat, res = 0, dmactrl;

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

	if (SPD_REG16(SPD_R_REV_1) < 17)
		dmactrl = (dmactrl & 0x03) | 0x04;
	else
		dmactrl = (dmactrl & 0x01) | 0x06;
	SPD_REG16(SPD_R_DMA_CTRL) = dmactrl;

	if (dev9_predma_cbs[ctrl])
		dev9_predma_cbs[ctrl](bcr, dir);

	EnableIntr(IOP_IRQ_DMA_DEV9);
	dev9_chan->madr = (u32)buf;
	dev9_chan->bcr  = bcr;
	dev9_chan->chcr = DMAC_CHCR_30|DMAC_CHCR_TR|DMAC_CHCR_CO|
		(dir & DMAC_CHCR_DR);

	/* Wait for DMA to complete.  */
	if ((res = WaitSema(dma_complete_sem)) >= 0)
		res = 0;

	DisableIntr(IOP_IRQ_DMA_DEV9, &stat);

	if (dev9_postdma_cbs[ctrl])
		dev9_postdma_cbs[ctrl](bcr, dir);

	SignalSema(dma_lock_sem);
	return res;
}

static int read_eeprom_data()
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

static int smap_subsys_init(void)
{
	int i, stat, flags;

	if ((dma_lock_sem = CreateMutex(IOP_MUTEX_UNLOCKED)) < 0)
		return -1;
	if ((dma_complete_sem = CreateMutex(IOP_MUTEX_LOCKED)) < 0)
		return -1;

	DisableIntr(IOP_IRQ_DMA_DEV9, &stat);
	CpuSuspendIntr(&flags);
	/* Enable the DEV9 DMAC channel.  */
	RegisterIntrHandler(IOP_IRQ_DMA_DEV9, 1, dev9_dma_intr, &dma_complete_sem);
	dmac_set_dpcr2(dmac_get_dpcr2() | 0x80);
	CpuResumeIntr(flags);

	/* Not quite sure what this enables yet.  */
	smap_set_stat(0x103);

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

static int smap_device_init(void)
{
#ifdef DEBUG
	USE_SPD_REGS;
	const char *spdnames[] = { "(unknown)", "TS", "ES1", "ES2" };
	int idx;
	u16 spdrev;
#endif

	eeprom_data[0] = 0;

	if (smap_device_probe() < 0) {
		M_PRINTF("PC card or expansion device isn't connected.\n");
		return -1;
	}

	smap_device_reset();

#ifdef PCMCIA
	int res;

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
static int pcic_get_cardtype()
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

static int pcic_get_voltage()
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

static int pcic_ssbus_mode(int voltage)
{
	USE_DEV9_REGS;
	USE_SPD_REGS;
	u16 stat = DEV9_REG(DEV9_R_1474) & 7;

	if (voltage != 3 && voltage != 5)
		return -1;

	DEV9_REG(DEV9_R_1460) = 2;
	if (stat)
		return -1;

	if (voltage == 3) {
		DEV9_REG(DEV9_R_1474) = 1;
		DEV9_REG(DEV9_R_1460) = 1;
		SPD_REG8(0x20) = 1;
		DEV9_REG(DEV9_R_1474) = voltage;
	} else if (voltage == 5) {
		DEV9_REG(DEV9_R_1474) = voltage;
		DEV9_REG(DEV9_R_1460) = 1;
		SPD_REG8(0x20) = 1;
		DEV9_REG(DEV9_R_1474) = 7;
	}
	_sw(0xe01a3043, SSBUS_R_1418);

	DelayThread(5000);
	DEV9_REG(DEV9_R_POWER) = DEV9_REG(DEV9_R_POWER) & ~1;
	return 0;
}

static int pcmcia_device_probe()
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
	USE_DEV9_REGS;
	u16 cstc1, cstc2;

	cstc1 = DEV9_REG(DEV9_R_1464);
	cstc2 = DEV9_REG(DEV9_R_1466);

	if (using_aif) {
		if (_lh(0xb4000004) & 0x04)
			_sh(0x04, 0xb4000004);
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
	int *mode;
	int flags;
	u16 cstc1, cstc2;

	_sw(0x51011, SSBUS_R_1420);
	_sw(0x1a00bb, SSBUS_R_1418);
	_sw(0xef1a3043, SSBUS_R_141c);

	/* If we are a T10K, then we go through AIF.  */
	if ((mode = QueryBootMode(6)) != NULL) {
		if ((*(u16 *)mode & 0xfe) == 0x60) {
			if (_lh(0xb4000000) == 0xa1) {
				_sh(4, 0xb4000006);
				using_aif = 1;
			} else {
				M_PRINTF("T10k detected, but AIF not detected.\n");
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

		if (smap_device_init() != 0)
			return 1;
	} else {
		_sw(0xe01a3043, SSBUS_R_1418);
	}

	if (smap_subsys_init() != 0)
		return 1;

	CpuSuspendIntr(&flags);
	RegisterIntrHandler(DEV9_INTR, 1, pcmcia_intr, NULL);
	EnableIntr(DEV9_INTR);
	CpuResumeIntr(flags);

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

static int expbay_device_probe()
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
	int flags;

	_sw(0x51011, SSBUS_R_1420);
	_sw(0xe01a3043, SSBUS_R_1418);
	_sw(0xef1a3043, SSBUS_R_141c);

	if ((DEV9_REG(DEV9_R_POWER) & 0x04) == 0) { /* if not already powered */
		DEV9_REG(DEV9_R_1466) = 1;
		DEV9_REG(DEV9_R_1464) = 0;
		DEV9_REG(DEV9_R_1460) = DEV9_REG(DEV9_R_1464);

		if (smap_device_init() != 0)
			return 1;
	}

	if (smap_subsys_init() != 0)
		return 1;

	CpuSuspendIntr(&flags);
	RegisterIntrHandler(DEV9_INTR, 1, expbay_intr, NULL);
	EnableIntr(DEV9_INTR);
	CpuResumeIntr(flags);

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
