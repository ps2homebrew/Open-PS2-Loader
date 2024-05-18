#include <errno.h>
#include <dmacman.h>
#include <stdio.h>
#include <dev9.h>
#include <intrman.h>
#include <loadcore.h>
#include <modload.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <irx.h>

#include <smapregs.h>
#include <speedregs.h>

#include "smstcpip.h"

#include "main.h"

#include "xfer.h"

extern struct SmapDriverData SmapDriverData;

static inline int CopyToFIFOWithDMA(volatile u8 *smap_regbase, void *buffer, int length)
{
    int NumBlocks;
    int result;

    /*  Non-Sony: the original block size was (32*4 = 128) bytes.
        However, that resulted in slightly lower performance due to the IOP needing to copy more data. */
    if ((NumBlocks = length >> 6) > 0) {
        if (SpdDmaTransfer(1, buffer, NumBlocks << 16 | 0x10, DMAC_FROM_MEM) >= 0) {
            result = NumBlocks << 6;
        } else
            result = 0;
    } else
        result = 0;

    return result;
}

static inline int CopyFromFIFOWithDMA(volatile u8 *smap_regbase, void *buffer, int length)
{
    int result;
    USE_SPD_REGS;
    u16 OldDMACtrl;

    // Attempt to steal the DMA channel from the DEV9 module.
    if ((result = length / 64) > 0) {
        OldDMACtrl = SPD_REG16(SPD_R_DMA_CTRL);

        while (dmac_ch_get_chcr(IOP_DMAC_DEV9) & DMAC_CHCR_TR) {
        }

        SPD_REG16(SPD_R_DMA_CTRL) = 7; // SPEED revision 17 (ES2) and above only.

        SMAP_REG16(SMAP_R_RXFIFO_SIZE) = result;
        SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_DMAEN;

        // Transfer in 64-byte (16x4) blocks
        dmac_request(IOP_DMAC_DEV9, buffer, 0x10, result, DMAC_TO_MEM);
        dmac_transfer(IOP_DMAC_DEV9);

        result *= 64;

        /* Wait for DMA to complete. Do not use a semaphore as thread switching hurts throughput greatly.  */
        while (dmac_ch_get_chcr(IOP_DMAC_DEV9) & DMAC_CHCR_TR) {
        }
        while (SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_DMAEN) {
        };

        SPD_REG16(SPD_R_DMA_CTRL) = OldDMACtrl;
    } else
        result = 0;

    return result;
}

static inline void CopyFromFIFO(volatile u8 *smap_regbase, void *buffer, unsigned int length, unsigned short int RxBdPtr)
{
    int result;

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = RxBdPtr;

    if ((result = CopyFromFIFOWithDMA(smap_regbase, buffer, length)) > 0) {
        length -= result;
        buffer = (void *)((u8 *)buffer + result);
    }

    __asm__ __volatile__(
        ".set noreorder\n\t"
        ".set nomacro\n\t"
        ".set noat\n\t"
        "   lui      $2, 0xB000\n\t"
        "   srl      $at, %1, 5\n\t"
        "   blez     $at, 3f\n\t"
        "   andi     %1, %1, 0x1F\n\t"
        "4:\n\t"
        "   lw      $8, 4608($2)\n\t"
        "   lw      $9, 4608($2)\n\t"
        "   lw      $10, 4608($2)\n\t"
        "   lw      $11, 4608($2)\n\t"
        "   lw      $12, 4608($2)\n\t"
        "   lw      $13, 4608($2)\n\t"
        "   lw      $14, 4608($2)\n\t"
        "   lw      $15, 4608($2)\n\t"
        "   addiu   $at, $at, -1\n\t"
        "   sw      $8,  0(%0)\n\t"
        "   sw      $9,  4(%0)\n\t"
        "   sw      $10,  8(%0)\n\t"
        "   sw      $11, 12(%0)\n\t"
        "   sw      $12, 16(%0)\n\t"
        "   sw      $13, 20(%0)\n\t"
        "   sw      $14, 24(%0)\n\t"
        "   sw      $15, 28(%0)\n\t"
        "   bgtz    $at, 4b\n\t"
        "   addiu   %0, %0, 32\n\t"
        "3:\n\t"
        "   blez     %1, 1f\n\t"
        "   nop\n\t"
        "2:\n\t"
        "   lw      $8, 4608($2)\n\t"
        "   addiu   %1, %1, -4\n\t"
        "   sw      $8, 0(%0)\n\t"
        "   bgtz    %1, 2b\n\t"
        "   addiu   %0, %0, 4\n\t"
        "1:\n\t"
        ".set at\n\t"
        ".set macro\n\t"
        ".set reorder\n\t" ::"r"(buffer),
        "r"(length)
        : "at", "v0", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7");
}

int HandleRxIntr(struct SmapDriverData *SmapDrivPrivData)
{
    USE_SMAP_RX_BD;
    int NumPacketsReceived;
    volatile smap_bd_t *PktBdPtr;
    volatile u8 *smap_regbase;
    struct pbuf *pbuf;
    u16 ctrl_stat, length, pointer, LengthRounded;

    smap_regbase = SmapDrivPrivData->smap_regbase;

    NumPacketsReceived = 0;

    /*  Non-Sony: Workaround for the hardware BUG whereby the Rx FIFO of the MAL becomes unresponsive or loses frames when under load.
        Check that there are frames to process, before accessing the BD registers. */
    while (SMAP_REG8(SMAP_R_RXFIFO_FRAME_CNT) > 0) {
        PktBdPtr = &rx_bd[SmapDrivPrivData->RxBDIndex % SMAP_BD_MAX_ENTRY];
        ctrl_stat = PktBdPtr->ctrl_stat;
        if (!(ctrl_stat & SMAP_BD_RX_EMPTY)) {
            length = PktBdPtr->length;
            LengthRounded = (length + 3) & ~3;
            pointer = PktBdPtr->pointer;

            if (ctrl_stat & (SMAP_BD_RX_INRANGE | SMAP_BD_RX_OUTRANGE | SMAP_BD_RX_FRMTOOLONG | SMAP_BD_RX_BADFCS | SMAP_BD_RX_ALIGNERR | SMAP_BD_RX_SHORTEVNT | SMAP_BD_RX_RUNTFRM | SMAP_BD_RX_OVERRUN)) {
            } else {
                if ((pbuf = pbuf_alloc(PBUF_RAW, LengthRounded, PBUF_POOL)) != NULL) {
                    CopyFromFIFO(SmapDrivPrivData->smap_regbase, pbuf->payload, length, pointer);

                    // Inform ps2ip that we've received data.
                    SMapLowLevelInput(pbuf);

                    NumPacketsReceived++;
                }
            }

            SMAP_REG8(SMAP_R_RXFIFO_FRAME_DEC) = 0;
            PktBdPtr->ctrl_stat = SMAP_BD_RX_EMPTY;
            SmapDrivPrivData->RxBDIndex++;
        } else
            break;
    }

    return NumPacketsReceived;
}

int SMAPSendPacket(const void *data, unsigned int length)
{
    int result;
    USE_SMAP_TX_BD;
    volatile u8 *smap_regbase;
    volatile u8 *emac3_regbase;
    volatile smap_bd_t *BD_ptr;
    u16 BD_data_ptr;
    unsigned int SizeRounded;

    if (SmapDriverData.SmapIsInitialized) {
        SizeRounded = (length + 3) & ~3;
        smap_regbase = SmapDriverData.smap_regbase;
        emac3_regbase = SmapDriverData.emac3_regbase;

        while (SMAP_EMAC3_GET(SMAP_R_EMAC3_TxMODE0) & SMAP_E3_TX_GNP_0) {
        };

        BD_data_ptr = SMAP_REG16(SMAP_R_TXFIFO_WR_PTR);
        BD_ptr = &tx_bd[SmapDriverData.TxBDIndex % SMAP_BD_MAX_ENTRY];

        if ((result = CopyToFIFOWithDMA(SmapDriverData.smap_regbase, (void *)data, length)) > 0) {
            SizeRounded -= result;
            data = (const void *)((u8 *)data + result);
        }

        __asm__ __volatile__(
            ".set noreorder\n\t"
            ".set nomacro\n\t"
            ".set noat\n\t"
            "   srl     $at, %1, 4\n\t"
            "   lui     $3, 0xB000\n\t"
            "   beqz    $at, 3f\n\t"
            "   andi    %1, %1, 0xF\n\t"
            "4:\n\t"
            "   lw      $8,  0(%0)\n\t"
            "   lw      $9,  4(%0)\n\t"
            "   lw      $10,  8(%0)\n\t"
            "   lw      $11, 12(%0)\n\t"
            "   addiu   $at, $at, -1\n\t"
            "   sw      $8, 4352($3)\n\t"
            "   sw      $9, 4352($3)\n\t"
            "   sw      $10, 4352($3)\n\t"
            "   addiu   %0, %0, 16\n\t"
            "   bgtz    $at, 4b\n\t"
            "   sw      $11, 4352($3)\n\t"
            "3:\n\t"
            "   beqz    %1, 1f\n\t"
            "   nop\n\t"
            "2:\n\t"
            "   lw      $2, 0(%0)\n\t"
            "   addiu   %1, %1, -4\n\t"
            "   sw      $2, 4352($3)\n\t"
            "   bnez    %1, 2b\n\t"
            "   addiu   %0, %0, 4\n\t"
            "1:\n\t"
            ".set reorder\n\t"
            ".set macro\n\t"
            ".set at\n\t" ::"r"(data),
            "r"(SizeRounded)
            : "at", "v0", "v1", "t0", "t1", "t2", "t3");

        BD_ptr->length = length;
        BD_ptr->pointer = BD_data_ptr;
        SMAP_REG8(SMAP_R_TXFIFO_FRAME_INC) = 0;
        BD_ptr->ctrl_stat = SMAP_BD_TX_READY | SMAP_BD_TX_GENFCS | SMAP_BD_TX_GENPAD;
        SmapDriverData.TxBDIndex++;

        SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE0, SMAP_E3_TX_GNP_0);

        result = 1;
    } else
        result = -1;

    return result;
}
