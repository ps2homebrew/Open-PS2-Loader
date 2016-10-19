/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright (c) 2003  Marcus R. Brown <mrbrown@0xd6.org>
  Licenced under Academic Free License version 3.0
  Review OpenPS2Loader README & LICENSE files for further details.
  
  Code originally based on SMAP driver sources found in UDPTTY driver.
  Optimized mips functions from Eugene Plotnikov. 
*/

#include <tamtypes.h>
#include <stdio.h>
#include <thbase.h>
#include <thsemap.h>
#include <intrman.h>
#include <dev9.h>
#include <speedregs.h>
#include <dev9regs.h>
#include <smapregs.h>

#include "smsutils.h"
#include "smap.h"
#include "ip.h"
#include "arp.h"
#include "tcp.h"
#include "inet.h"

#define SMAP_RX_BUFSIZE 16384
#define SMAP_RX_BASE 0x4000
#define SMAP_RX_MINSIZE 14 // Ethernet header size
#define SMAP_RX_MAXSIZE (6 + 6 + 2 + 1500 + 4)
#define SMAP_TX_MAXSIZE (6 + 6 + 2 + 1500)

#define SMAP_BD_NEXT(x) (x) = (x) < (SMAP_BD_MAX_ENTRY - 1) ? (x) + 1 : 0

// TX descriptor
typedef struct
{
    u16 txwp_start;
    u16 txwp_end;
    u8 txbdi_start;
    u8 txbdi_end;
} smap_tx_stat_t;

static smap_tx_stat_t tx_stat;

// RX descriptor
typedef struct
{
    u8 rxbdi; // Index into current RX BD
    u8 pad[3];
} smap_rx_stat_t;

static smap_rx_stat_t rx_stat;

static int smap_xmit_mutex = -1;

static u8 rcpt_buf[SMAP_RX_MAXSIZE] __attribute__((aligned(64)));

//-------------------------------------------------------------------------
void smap_CopyToFIFO(u16, u32 *, int);
__asm__(
    ".set noreorder\n\t"
    ".set nomacro\n\t"
    ".set noat\n\t"
    ".text\n\t"
    "smap_CopyToFIFO:\n\t"
    "srl   $at, $a2, 4\n\t"
    "lui   $v1, 0xB000\n\t"
    "sh    $a0, 4100($v1)\n\t"
    "beqz  $at, 3f\n\t"
    "andi  $a2, $a2, 0xF\n\t"
    "4:\n\t"
    "lwr   $t0,  0($a1)\n\t"
    "lwl   $t0,  3($a1)\n\t"
    "lwr   $t1,  4($a1)\n\t"
    "lwl   $t1,  7($a1)\n\t"
    "lwr   $t2,  8($a1)\n\t"
    "lwl   $t2, 11($a1)\n\t"
    "lwr   $t3, 12($a1)\n\t"
    "lwl   $t3, 15($a1)\n\t"
    "addiu $at, $at, -1\n\t"
    "sw    $t0, 4352($v1)\n\t"
    "sw    $t1, 4352($v1)\n\t"
    "sw    $t2, 4352($v1)\n\t"
    "addiu $a1, $a1, 16\n\t"
    "bgtz  $at, 4b\n\t"
    "sw    $t3, 4352($v1)\n\t"
    "3:\n\t"
    "beqz  $a2, 1f\n\t"
    "nop\n\t"
    "2:\n\t"
    "lwr   $v0, 0($a1)\n\t"
    "lwl   $v0, 3($a1)\n\t"
    "addiu $a2, $a2, -4\n\t"
    "sw    $v0, 4352($v1)\n\t"
    "bnez  $a2, 2b\n\t"
    "addiu $a1, $a1, 4\n\t"
    "1:\n\t"
    "jr    $ra\n\t"
    "nop\n\t"
    ".set at\n\t"
    ".set macro\n\t"
    ".set reorder\n\t");

//-------------------------------------------------------------------------
void smap_CopyFromFIFO(u16, u32 *, int);
__asm__(
    ".set noreorder\n\t"
    ".set nomacro\n\t"
    ".set noat\n\t"
    ".text\n\t"
    "smap_CopyFromFIFO:\n\t"
    "li    $v0, -4\n\t"
    "lui   $a3, 0xB000\n\t"
    "addiu $v1, $a2, 3\n\t"
    "and   $v1, $v1, $v0\n\t"
    "srl   $at, $v1, 5\n\t"
    "sh    $a0, 4148($a3)\n\t"
    "beqz  $at, 3f\n\t"
    "andi  $v1, $v1, 0x1F\n\t"
    "4:\n\t"
    "lw    $t0, 4608($a3)\n\t"
    "lw    $t1, 4608($a3)\n\t"
    "lw    $t2, 4608($a3)\n\t"
    "lw    $t3, 4608($a3)\n\t"
    "lw    $t4, 4608($a3)\n\t"
    "lw    $t5, 4608($a3)\n\t"
    "lw    $t6, 4608($a3)\n\t"
    "lw    $t7, 4608($a3)\n\t"
    "addiu $at, $at, -1\n\t"
    "sw    $t0,  0($a1)\n\t"
    "sw    $t1,  4($a1)\n\t"
    "sw    $t2,  8($a1)\n\t"
    "sw    $t3, 12($a1)\n\t"
    "sw    $t4, 16($a1)\n\t"
    "sw    $t5, 20($a1)\n\t"
    "sw    $t6, 24($a1)\n\t"
    "sw    $t7, 28($a1)\n\t"
    "bgtz  $at, 4b\n\t"
    "addiu $a1, $a1, 32\n\t"
    "3:\n\t"
    "beqz  $v1, 1f\n\t"
    "nop\n\t"
    "2:\n\t"
    "lw    $v0, 4608($a3)\n\t"
    "addiu $v1, $v1, -4\n\t"
    "sw    $v0, 0($a1)\n\t"
    "bnez  $v1, 2b\n\t"
    "addiu $a1, $a1, 4\n\t"
    "1:\n\t"
    "jr    $ra\n\t"
    "nop\n\t"
    ".set at\n\t"
    ".set macro\n\t"
    ".set reorder\n\t");

//-------------------------------------------------------------------------
static int smap_phy_read(int reg, u16 *data)
{
    USE_SMAP_EMAC3_REGS;
    u32 i, val;

    val = SMAP_E3_PHY_READ | (SMAP_DsPHYTER_ADDRESS << SMAP_E3_PHY_ADDR_BITSFT) |
          (reg & SMAP_E3_PHY_REG_ADDR_MSK);
    SMAP_EMAC3_SET(SMAP_R_EMAC3_STA_CTRL, val);

    // Wait for the read operation to complete
    for (i = 0; i < 100; i++) {
        if (SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP)
            break;
        DelayThread(1000);
    }
    if (i == 100 || !data)
        return 1;

    *data = SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL) >> SMAP_E3_PHY_DATA_BITSFT;
    return 0;
}

//-------------------------------------------------------------------------
static int smap_phy_write(int reg, u16 data)
{
    USE_SMAP_EMAC3_REGS;
    u32 i, val;

    val = ((data & SMAP_E3_PHY_DATA_MSK) << SMAP_E3_PHY_DATA_BITSFT) |
          SMAP_E3_PHY_WRITE | (SMAP_DsPHYTER_ADDRESS << SMAP_E3_PHY_ADDR_BITSFT) |
          (reg & SMAP_E3_PHY_REG_ADDR_MSK);
    SMAP_EMAC3_SET(SMAP_R_EMAC3_STA_CTRL, val);

    // Wait for the write operation to complete
    for (i = 0; i < 100; i++) {
        if (SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP)
            break;
        DelayThread(1000);
    }
    return (i == 100);
}

//-------------------------------------------------------------------------
static int smap_phy_init(void)
{
    USE_SMAP_EMAC3_REGS;
    u32 val;
    int i;
    u16 phydata, idr1, idr2;

    // Reset the PHY
    smap_phy_write(SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_RST);
    // Wait for it to come out of reset
    for (i = 9; i; i--) {
        smap_phy_read(SMAP_DsPHYTER_BMCR, &phydata);
        if (!(phydata & SMAP_PHY_BMCR_RST))
            break;
        DelayThread(1000);
    }
    if (!i)
        return 1;

    // Confirm link status
    i = 5000;
    while (--i) {
        smap_phy_read(SMAP_DsPHYTER_BMSR, &phydata);
        if (phydata & SMAP_PHY_BMSR_LINK)
            break;
        DelayThread(1000);
    }

    smap_phy_write(SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_100M | SMAP_PHY_BMCR_DUPM);

    i = 2000;
    while (--i)
        DelayThread(1000);

    // Force 100 Mbps full duplex mode
    val = SMAP_EMAC3_GET(SMAP_R_EMAC3_MODE1);
    val |= (SMAP_E3_FDX_ENABLE | SMAP_E3_FLOWCTRL_ENABLE | SMAP_E3_ALLOW_PF);
    val &= ~SMAP_E3_MEDIA_MSK;
    val |= SMAP_E3_MEDIA_100M;
    SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE1, val);

    // DSP setup
    smap_phy_read(SMAP_DsPHYTER_PHYIDR1, &idr1);
    smap_phy_read(SMAP_DsPHYTER_PHYIDR2, &idr2);

    if (idr1 == SMAP_PHY_IDR1_VAL &&
        ((idr2 & SMAP_PHY_IDR2_MSK) == SMAP_PHY_IDR2_VAL)) {
        smap_phy_read(SMAP_DsPHYTER_PHYSTS, &phydata);
        if (phydata & SMAP_PHY_STS_10M)
            smap_phy_write(0x1a, 0x104);

        smap_phy_write(0x13, 0x0001);
        smap_phy_write(0x19, 0x1898);
        smap_phy_write(0x1f, 0x0000);
        smap_phy_write(0x1d, 0x5040);
        smap_phy_write(0x1e, 0x008c);
        smap_phy_write(0x13, 0x0000);
    }

    return 0;
}

//-------------------------------------------------------------------------
int smap_xmit(void *buf, int size)
{
    USE_SMAP_REGS;
    USE_SMAP_EMAC3_REGS;
    USE_SMAP_TX_BD;
    int txlen, r = 0;

    WaitSema(smap_xmit_mutex);

    // Get total length of the packet
    if (size > SMAP_TX_MAXSIZE) {
        r = 1;
        goto ssema;
    }

    txlen = (size + 3) & 0xfffc;

    // Get free mem in TX
    int txfree = ((tx_stat.txwp_start > tx_stat.txwp_end) ?
                      (SMAP_TX_BUFSIZE + tx_stat.txwp_end - tx_stat.txwp_start) :
                      (SMAP_TX_BUFSIZE - tx_stat.txwp_end + tx_stat.txwp_start));
    if (txlen > txfree) {
        r = 2;
        goto ssema;
    }

    // Check EMAC is ready
    if (SMAP_EMAC3_GET(SMAP_R_EMAC3_TxMODE0) & SMAP_E3_TX_GNP_0) {
        r = 2;
        goto ssema;
    }

    // Send from mem -> FIFO
    smap_CopyToFIFO(tx_stat.txwp_end, buf, txlen);

    // FIFO -> ethernet
    tx_bd[tx_stat.txbdi_end].length = size;
    tx_bd[tx_stat.txbdi_end].pointer = tx_stat.txwp_end + SMAP_TX_BASE;
    SMAP_REG8(SMAP_R_TXFIFO_FRAME_INC) = 1;
    tx_bd[tx_stat.txbdi_end].ctrl_stat = SMAP_BD_TX_READY | SMAP_BD_TX_GENFCS | SMAP_BD_TX_GENPAD;

    SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE0, SMAP_E3_TX_GNP_0);

    // Update tx pointers
    tx_stat.txwp_end = (tx_stat.txwp_end + txlen) % SMAP_TX_BUFSIZE;
    SMAP_BD_NEXT(tx_stat.txbdi_end);

ssema:
    SignalSema(smap_xmit_mutex);

    return r;
}

//-------------------------------------------------------------------------
static int smap_tx_intr(int irq)
{
    USE_SMAP_REGS;
    USE_SMAP_TX_BD;
    register int txbdi, stat;
    register int ret = 1;

    // Clear irq
    SMAP_REG16(SMAP_R_INTR_CLR) = irq & SMAP_INTR_BITMSK;

    while (tx_stat.txbdi_start != tx_stat.txbdi_end) {

        txbdi = tx_stat.txbdi_start;
        stat = tx_bd[txbdi].ctrl_stat;

        if (stat & SMAP_BD_TX_ERROR)
            ret = 0;

        if (stat & SMAP_BD_TX_READY)
            goto out;

        tx_stat.txwp_start = (tx_stat.txwp_start + ((tx_bd[txbdi].length + 3) & 0xfffc)) % SMAP_TX_BUFSIZE;
        SMAP_BD_NEXT(tx_stat.txbdi_start);

        tx_bd[txbdi].ctrl_stat = 0;
        tx_bd[txbdi].reserved = 0;
        tx_bd[txbdi].length = 0;
    }

    tx_stat.txbdi_start = tx_stat.txbdi_end;

out:
    return ret;
}

//-------------------------------------------------------------------------
static int smap_rx_intr(int irq)
{
    USE_SMAP_REGS;
    USE_SMAP_RX_BD;
    u16 len;
    register int rxbdi, stat;
    register int ret = 1;

    // Clear irq
    irq &= SMAP_INTR_RXDNV | SMAP_INTR_RXEND;
    SMAP_REG16(SMAP_R_INTR_CLR) = irq & SMAP_INTR_BITMSK;

    while (1) {

        rxbdi = rx_stat.rxbdi;
        stat = rx_bd[rxbdi].ctrl_stat;

        if (stat & SMAP_BD_RX_EMPTY)
            break;

        len = rx_bd[rxbdi].length;

        // check for BD error and packet size
        if ((stat & SMAP_BD_RX_ERROR) || ((len < SMAP_RX_MINSIZE) || (len > SMAP_RX_MAXSIZE)))
            ret = 0;
        else {
            u16 rxrp = ((rx_bd[rxbdi].pointer - SMAP_RX_BASE) % SMAP_RX_BUFSIZE) & 0xfffc;
            // FIFO -> memory
            smap_CopyFromFIFO(rxrp, (u32 *)rcpt_buf, len);

            eth_hdr_t *eth = (eth_hdr_t *)rcpt_buf;
            if (eth->type == 0x0008) { // IP packet

                ip_hdr_t *ip = (ip_hdr_t *)&rcpt_buf[14];
                if ((ip->hlen == 0x45) && (inet_chksum(ip, 20) == 0)) {    // Check IPv4 & IP checksum
                    if ((!(ip->flags & 0x3f)) && (ip->frag_offset == 0)) { // Drop IP fragments
                        if (ip->proto == 0x06)                             // Check it's TCP packet
                            tcp_input(rcpt_buf, len);
                    }
                }
            } else if (eth->type == 0x0608) { // ARP packet
                arp_input(rcpt_buf, len);
                // ARP packet is then dropped
            }
        }

        SMAP_REG8(SMAP_R_RXFIFO_FRAME_DEC) = 1;
        rx_bd[rxbdi].ctrl_stat = SMAP_BD_RX_EMPTY;
        SMAP_BD_NEXT(rx_stat.rxbdi);
    }

    return ret;
}

//-------------------------------------------------------------------------
static int smap_emac3_intr(int irq)
{
    USE_SMAP_REGS;
    USE_SMAP_EMAC3_REGS;
    register u32 stat;

    stat = SMAP_EMAC3_GET(SMAP_R_EMAC3_INTR_STAT);

    SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_STAT, stat);

    // clear irq
    SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_EMAC3;
    SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_STAT, SMAP_E3_INTR_ALL);

    return 1;
}

//-------------------------------------------------------------------------
static int smap_intr_handler(int state)
{
    USE_SPD_REGS;
    register int irq;

    irq = SPD_REG16(SPD_R_INTR_STAT) & SMAP_INTR_BITMSK;

    if (irq & (SMAP_INTR_TXDNV | SMAP_INTR_TXEND)) { // TX intr
        smap_tx_intr(irq & (SMAP_INTR_TXDNV | SMAP_INTR_TXEND));
        irq = SPD_REG16(SPD_R_INTR_STAT) & SMAP_INTR_BITMSK;
    }

    if (irq & (SMAP_INTR_RXDNV | SMAP_INTR_RXEND)) // RX intr
        smap_rx_intr(irq & (SMAP_INTR_RXDNV | SMAP_INTR_RXEND));
    else if (irq & SMAP_INTR_EMAC3) // EMAC3 intr
        smap_emac3_intr(irq & SMAP_INTR_EMAC3);

    return 1;
}

//-------------------------------------------------------------------------
int smap_init(u8 *eth_addr_src)
{
    USE_SPD_REGS;
    USE_DEV9_REGS;
    USE_SMAP_REGS;
    USE_SMAP_EMAC3_REGS;
    USE_SMAP_TX_BD;
    USE_SMAP_RX_BD;
    u16 MAC[4];
    u16 MACcsum = 0;
    u8 hwaddr[6];
    u32 val;
    register int i;

    // Check out the SPEED chip revision
    if (!(SPD_REG16(SPD_R_REV_3) & SPD_CAPS_SMAP) || SPD_REG16(SPD_R_REV_1) <= 16)
        return 1;

    dev9IntrDisable(SMAP_INTR_BITMSK);
    EnableIntr(IOP_IRQ_DEV9);
    CpuEnableIntr();

    // PCMCIA Card removed flag
    DEV9_REG(DEV9_R_1464) = 0x03;

    // Get HW MAC address and check it
    if ((dev9GetEEPROM(&MAC[0]) < 0) || (!MAC[0] && !MAC[1] && !MAC[2]))
        return 1;
    for (i = 0; i < 3; i++)
        MACcsum += MAC[i];
    if (MACcsum != MAC[3])
        return 1;
    mips_memcpy(eth_addr_src, &MAC[0], 6);

    // Disable TX/RX
    val = SMAP_EMAC3_GET(SMAP_R_EMAC3_MODE0);
    val &= ~(SMAP_E3_TXMAC_ENABLE | SMAP_E3_RXMAC_ENABLE);
    SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, val);

    // Disable all EMAC3 interrupts
    SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_ENABLE, 0);

    // Reset the transmit FIFO
    SMAP_REG8(SMAP_R_TXFIFO_CTRL) = SMAP_TXFIFO_RESET;
    for (i = 9; i; i--) {
        if (!(SMAP_REG8(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_RESET))
            break;
        DelayThread(1000);
    }
    if (!i)
        return 2;

    // Reset the receive FIFO
    SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_RESET;
    for (i = 9; i; i--) {
        if (!(SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_RESET))
            break;
        DelayThread(1000);
    }
    if (!i)
        return 3;

    // Perform soft reset of EMAC3
    SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_SOFT_RESET);
    for (i = 9; i; i--) {
        if (!(SMAP_EMAC3_GET(SMAP_R_EMAC3_MODE0) & SMAP_E3_SOFT_RESET))
            break;
        DelayThread(1000);
    }
    if (!i)
        return 4;

    SMAP_REG8(SMAP_R_BD_MODE) = 0;

    // Initialize all RX and TX buffer descriptors
    for (i = 0; i < SMAP_BD_MAX_ENTRY; i++, tx_bd++) {
        tx_bd->ctrl_stat = 0;
        tx_bd->reserved = 0;
        tx_bd->length = 0;
        tx_bd->pointer = 0;
    }
    for (i = 0; i < SMAP_BD_MAX_ENTRY; i++, rx_bd++) {
        rx_bd->ctrl_stat = SMAP_BD_RX_EMPTY;
        rx_bd->reserved = 0;
        rx_bd->length = 0;
        rx_bd->pointer = 0;
    }

    // Clear all irq
    SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_BITMSK;

    // EMAC3 operating MODE
    val = SMAP_E3_FDX_ENABLE | SMAP_E3_IGNORE_SQE | SMAP_E3_MEDIA_100M |
          SMAP_E3_RXFIFO_2K | SMAP_E3_TXFIFO_1K | SMAP_E3_TXREQ0_SINGLE | SMAP_E3_TXREQ1_SINGLE;
    SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE1, val);

    // TX fifo value for request priority. low = 7*8=56, urgent = 15*8=120.
    val = (7 << SMAP_E3_TX_LOW_REQ_BITSFT) | (15 << SMAP_E3_TX_URG_REQ_BITSFT);
    SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE1, val);

    // RX mode
    val = SMAP_E3_RX_STRIP_PAD | SMAP_E3_RX_STRIP_FCS | SMAP_E3_RX_INDIVID_ADDR | SMAP_E3_RX_BCAST;
    SMAP_EMAC3_SET(SMAP_R_EMAC3_RxMODE, val);

    // Set HW MAC address
    mips_memcpy(hwaddr, eth_addr_src, 6);
    val = (u16)((hwaddr[0] << 8) | hwaddr[1]);
    SMAP_EMAC3_SET(SMAP_R_EMAC3_ADDR_HI, val);
    val = ((hwaddr[2] << 24) | (hwaddr[3] << 16) | (hwaddr[4] << 8) | hwaddr[5]);
    SMAP_EMAC3_SET(SMAP_R_EMAC3_ADDR_LO, val);

    // Inter-frame GAP
    SMAP_EMAC3_SET(SMAP_R_EMAC3_INTER_FRAME_GAP, 4);

    // TX threshold
    val = 12 << SMAP_E3_TX_THRESHLD_BITSFT;
    SMAP_EMAC3_SET(SMAP_R_EMAC3_TX_THRESHOLD, val);

    // RX watermark, low = 16*8=128, hi = 128*8=1024
    val = (16 << SMAP_E3_RX_LO_WATER_BITSFT) | (128 << SMAP_E3_RX_HI_WATER_BITSFT);
    SMAP_EMAC3_SET(SMAP_R_EMAC3_RX_WATERMARK, val);

    // Enable all EMAC3 interrupts
    SMAP_EMAC3_SET(SMAP_R_EMAC3_INTR_ENABLE, SMAP_E3_INTR_ALL);

    // Initialize the PHY
    if ((i = smap_phy_init()) != 0)
        return -i;

    tx_stat.txwp_start = tx_stat.txwp_end = 0;
    tx_stat.txbdi_start = tx_stat.txbdi_end = 0;
    rx_stat.rxbdi = 0;

    // Install the smap_intr_handler for all the SMAP interrupts
    for (i = 2; i < 7; i++)
        dev9RegisterIntrCb(i, smap_intr_handler);

    // Enable TX/RX
    SMAP_EMAC3_SET(SMAP_R_EMAC3_MODE0, SMAP_E3_TXMAC_ENABLE | SMAP_E3_RXMAC_ENABLE);
    DelayThread(10000);

    dev9IntrEnable(SMAP_INTR_BITMSK);

    smap_xmit_mutex = CreateMutex(IOP_MUTEX_UNLOCKED);

    return 0;
}
