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

#include <types.h>
#include <defs.h>
#include <loadcore.h>
#include <intrman.h>
#include <iomanX.h>
#include <dmacman.h>
#include <thbase.h>
#include <thsemap.h>
#include <aifregs.h>
#include <dev9regs.h>
#include <speedregs.h>
#include <smapregs.h>
#include <stdio.h>

#include "dev9.h"

#ifdef HDD_DRIVER
extern char atad_inited;
#endif

#ifdef DEV9_DEBUG
#define M_PRINTF(format, args...) \
    printf(MODNAME ": " format, ##args)
#else
#define M_PRINTF(format, args...) \
    do {                          \
    } while (0)
#endif

#define VERSION "1.0"
#define BANNER  "\nDEV9 device driver v%s - Copyright (c) 2003 Marcus R. Brown\n\n"

/* SSBUS registers.  */
#define SSBUS_R_1418 0xbf801418
#define SSBUS_R_141c 0xbf80141c
#define SSBUS_R_1420 0xbf801420

static int dev9type = -1; /* 0 for PCMCIA, 1 for expansion bay */
static int using_aif = 0; /* 1 if using AIF on a T10K */

static void (*p_dev9_intr_cb)(int flag) = NULL;
static int dma_lock_sem = -1; /* used to arbitrate DMA */

static s16 eeprom_data[5]; /* 2-byte EEPROM status (0/-1 = invalid, 1 = valid),
				   6-byte MAC address,
				   2-byte MAC address checksum.  */

/* Each driver can register callbacks that correspond to each bit of the
   SMAP interrupt status register (0xbx000028).  */
static dev9_intr_cb_t dev9_intr_cbs[16];

static dev9_shutdown_cb_t dev9_shutdown_cbs[16];

static dev9_dma_cb_t dev9_predma_cbs[4], dev9_postdma_cbs[4];

static int dev9_intr_dispatch(int flag);

static void dev9_set_stat(int stat);
static int read_eeprom_data(void);

static int dev9_init(void);

static void pcmcia_set_stat(int stat);
#ifdef DEBUG
static int pcmcia_device_probe(void);
#endif
static int pcmcia_init(void);
static void expbay_set_stat(int stat);
static int expbay_init(void);

static int dev9x_dummy(void) { return 0; }
static int dev9x_devctl(iop_file_t *f, const char *name, int cmd, void *args, int arglen, void *buf, int buflen)
{
    switch (cmd) {
        case DDIOC_MODEL:
            return dev9type;
        case DDIOC_OFF:
            //Do not let the DEV9 interface to be switched off by other software.
            //dev9Shutdown();
            return 0;
        default:
            return 0;
    }
}

/* driver ops func tab */
static iop_device_ops_t dev9x_ops = {
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_dummy,
    (void *)dev9x_devctl};

/* driver descriptor */
static iop_device_t dev9x_dev = {
    "dev9x",
    IOP_DT_FS | IOP_DT_FSEXT,
    1,
    "DEV9",
    &dev9x_ops};

int dev9d_init(void)
{
    USE_DEV9_REGS;
    int idx, res = 1;
    u16 dev9hw;

    M_PRINTF(BANNER, VERSION);

    for (idx = 0; idx < 16; idx++)
        dev9_shutdown_cbs[idx] = NULL;

    dev9hw = DEV9_REG(DEV9_R_REV) & 0xf0;
    if (dev9hw == 0x20) { /* CXD9566 (PCMCIA) */
        dev9type = 0;
        res = pcmcia_init();
    } else if (dev9hw == 0x30) { /* CXD9611 (Expansion Bay) */
        dev9type = 1;
        res = expbay_init();
    }

    if (res)
        return res;

    DelDrv("dev9x");
    AddDrv(&dev9x_dev);

    return 0;
}

/* Export 4 */
void dev9RegisterIntrCb(int intr, dev9_intr_cb_t cb)
{
#ifdef HDD_DRIVER
    //Don't let anything else change the HDD interrupt handlers.
    if (intr < 2) {
        if (atad_inited)
            return;
    }
#endif
    dev9_intr_cbs[intr] = cb;
}

/* Export 12 */
void dev9RegisterPreDmaCb(int ctrl, dev9_dma_cb_t cb)
{
    dev9_predma_cbs[ctrl] = cb;
}

/* Export 13 */
void dev9RegisterPostDmaCb(int ctrl, dev9_dma_cb_t cb)
{
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
                       SPD_REG16(SPD_R_INTR_MASK)) >>
                      i;
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
        pcmcia_set_stat(stat);
    else if (dev9type == 1)
        expbay_set_stat(stat);
}

/* Export 6 */
void dev9Shutdown(void)
{
    int idx;
    USE_DEV9_REGS;

    for (idx = 0; idx < 16; idx++)
        if (dev9_shutdown_cbs[idx])
            dev9_shutdown_cbs[idx]();

    if (dev9type == 0) { /* PCMCIA */
        DEV9_REG(DEV9_R_POWER) = 0;
        DEV9_REG(DEV9_R_1474) = 0;
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
    volatile iop_dmac_chan_t *dev9_chan = (iop_dmac_chan_t *)DEV9_DMAC_BASE;
    int res = 0, dmactrl;

    switch (ctrl) {
        case 0:
        case 1:
            dmactrl = ctrl;
            break;

        case 2:
        case 3:
            if (dev9_predma_cbs[ctrl] == NULL)
                return -1;
            if (dev9_postdma_cbs[ctrl] == NULL)
                return -1;
            dmactrl = (4 << ctrl);
            break;

        default:
            return -1;
    }

    if ((res = WaitSema(dma_lock_sem)) < 0)
        return res;

    SPD_REG16(SPD_R_DMA_CTRL) = (SPD_REG16(SPD_R_REV_1) < 17) ? (dmactrl & 0x03) | 0x04 : (dmactrl & 0x01) | 0x06;

    if (dev9_predma_cbs[ctrl])
        dev9_predma_cbs[ctrl](bcr, dir);

    dev9_chan->madr = (u32)buf;
    dev9_chan->bcr = bcr;
    dev9_chan->chcr = DMAC_CHCR_30 | DMAC_CHCR_TR | DMAC_CHCR_CO | (dir & DMAC_CHCR_DR);

    /* Wait for DMA to complete. Do not use a semaphore as thread switching hurts throughput greatly.  */
    while (dev9_chan->chcr & DMAC_CHCR_TR) {
    }
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

    SPD_REG8(SPD_R_PIO_DIR) = 0xe1;
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
    if (val & 0x10) { /* Error.  */
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
        eeprom_data[i + 1] = 0;

        for (j = 15; j >= 0; j--) {
            SPD_REG8(SPD_R_PIO_DATA) = 0xc0;
            DelayThread(1);
            val = SPD_REG8(SPD_R_PIO_DATA);
            if (val & 0x10)
                eeprom_data[i + 1] |= (1 << j);
            SPD_REG8(SPD_R_PIO_DATA) = 0x80;
            DelayThread(1);
        }
    }

    SPD_REG8(SPD_R_PIO_DATA) = 0;
    DelayThread(1);
    eeprom_data[0] = 1; /* The EEPROM data is valid.  */
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
        buf[i] = eeprom_data[i + 1];

    return 0;
}

/* Export 10 */
void dev9LEDCtl(int ctl)
{
    USE_SPD_REGS;
    SPD_REG8(SPD_R_PIO_DATA) = (ctl == 0);
}

/* Export 11 */
int dev9RegisterShutdownCb(int idx, dev9_shutdown_cb_t cb)
{
    if (idx < 16) {
        dev9_shutdown_cbs[idx] = cb;
        return 0;
    }
    return -1;
}

static int dev9_init(void)
{
    iop_sema_t sema;
    int i, flags;

    sema.attr = SA_THPRI;
    sema.initial = 1;
    sema.max = 1;
    if ((dma_lock_sem = CreateSema(&sema)) < 0)
        return -1;

    CpuSuspendIntr(&flags);
    /* Enable the DEV9 DMAC channel.  */
    dmac_set_dpcr2(dmac_get_dpcr2() | 0x80);
    CpuResumeIntr(flags);

    /* Not quite sure what this enables yet.  */
    dev9_set_stat(0x103);

    /* Disable all device interrupts.  */
    dev9IntrDisable(0xffff);

    /* Register interrupt dispatch callback handler. */
    p_dev9_intr_cb = (void *)dev9_intr_dispatch;

    /* Reset the SMAP interrupt callback table. */
    for (i = 0; i < 16; i++)
        dev9_intr_cbs[i] = NULL;

    for (i = 0; i < 4; i++) {
        dev9_predma_cbs[i] = NULL;
        dev9_postdma_cbs[i] = NULL;
    }

    /* Read in the MAC address.  */
    read_eeprom_data();
    /* Turn the LED off.  */
    dev9LEDCtl(0);
    return 0;
}

#ifdef DEBUG
static int pcic_get_cardtype(void)
{
    USE_DEV9_REGS;
    u16 val = DEV9_REG(DEV9_R_1462) & 0x03;

    if (val == 0)
        return 1; /* 16-bit */
    else if (val < 3)
        return 2; /* CardBus */
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
#endif

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

#ifdef DEBUG
static int pcmcia_device_probe(void)
{
    const char *pcic_ct_names[] = {"No", "16-bit", "CardBus"};
    int voltage;
    int pcic_cardtype; /* Translated value of bits 0-1 of 0xbf801462 */
    int pcic_voltage;  /* Translated value of bits 2-3 of 0xbf801462 */

    pcic_voltage = pcic_get_voltage();
    pcic_cardtype = pcic_get_cardtype();
    voltage = (pcic_voltage == 2 ? 5 : (pcic_voltage == 1 ? 3 : 0));

    M_PRINTF("%s PCMCIA card detected. Vcc = %dV\n",
             pcic_ct_names[pcic_cardtype], voltage);

    if (pcic_voltage == 3 || pcic_cardtype != 1)
        return -1;

    return 0;
}
#endif

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
            return 0; /* Unknown interrupt.  */
    }

    /* Acknowledge the interrupt.  */
    DEV9_REG(DEV9_R_1464) = cstc1;
    DEV9_REG(DEV9_R_1466) = cstc2;
    if (cstc1 & 0x03 || cstc2 & 0x03) { /* Card removed or added? */
        if (p_dev9_intr_cb)
            p_dev9_intr_cb(1);

        /* Shutdown the card.  */
        DEV9_REG(DEV9_R_POWER) = 0;
        DEV9_REG(DEV9_R_1474) = 0;

#ifdef DEBUG
        pcmcia_device_probe();
#endif
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
    int *mode, flags;

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

    //The DEV9 interface and SPEED should be already initialized.
    _sw(0xe01a3043, SSBUS_R_1418);

    if (dev9_init() != 0)
        return 1;

    CpuSuspendIntr(&flags);
    RegisterIntrHandler(IOP_IRQ_DEV9, 1, &pcmcia_intr, NULL);
    EnableIntr(IOP_IRQ_DEV9);
    CpuResumeIntr(flags);

    DEV9_REG(DEV9_R_147E) = 0;
    M_PRINTF("CXD9566 (PCMCIA type) initialized.\n");
    return 0;
}

static void expbay_set_stat(int stat)
{
    USE_DEV9_REGS;
    DEV9_REG(DEV9_R_1464) = stat & 0x3f;
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

    //The DEV9 interface and SPEED should be already initialized.

    if (dev9_init() != 0)
        return 1;

    CpuSuspendIntr(&flags);
    RegisterIntrHandler(IOP_IRQ_DEV9, 1, expbay_intr, NULL);
    EnableIntr(IOP_IRQ_DEV9);
    CpuResumeIntr(flags);

    DEV9_REG(DEV9_R_1466) = 0;
    M_PRINTF("CXD9611 (Expansion Bay type) initialized.\n");
    return 0;
}
