/*
  resetspu.c Open PS2 Loader
 
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  Reset SPU function taken from PS2SDK freesd.
  Copyright (c) 2004 TyRaNiD <tiraniddo@hotmail.com>
  Copyright (c) 2004,2007 Lukasz Bruun <mail@lukasz.dk>
*/

#include <intrman.h>
#include <libsd.h>
#include <loadcore.h>
#include <spu2regs.h>

static void nopdelay(void)
{
    unsigned int i;

    for(i=0; i < 0x10000; i++)
        asm volatile("nop\nnop\nnop\nnop\nnop");
}

static void spuReset(void)
{
    int core;
    volatile u16 *statx;

    // Initialize SSBUS access to SPU2
    *U32_REGISTER(0x1404) = 0xBF900000;
    *U32_REGISTER(0x140C) = 0xBF900800;
    *U32_REGISTER(0x10F0) |= 0x80000;
    *U32_REGISTER(0x1570) |= 8;
    *U32_REGISTER(0x1014) = 0x200B31E1;
    *U32_REGISTER(0x1414) = 0x200B31E1;

    // Stop SPU Dma Core 0
    *SD_DMA_CHCR(0) &= ~SD_DMA_START;
    *U16_REGISTER(0x1B0) = 0;

    // Stop SPU Dma Core 1
    *SD_DMA_CHCR(1) &= ~SD_DMA_START;
    *U16_REGISTER(0x1B0 + 1024) = 0;

    *SD_C_SPDIF_OUT = 0;
    nopdelay();
    *SD_C_SPDIF_OUT = 0x8000;
    nopdelay();

    *U32_REGISTER(0x10F0) |= 0xB0000;

    for (core = 0; core < 2; core++) {
        *U16_REGISTER(0x1B0) = 0;
        *SD_CORE_ATTR(core) = 0;
        nopdelay();
        *SD_CORE_ATTR(core) = SD_SPU2_ON;

        *SD_P_MVOLL(core) = 0;
        *SD_P_MVOLR(core) = 0;

        statx = U16_REGISTER(0x344 + (core * 1024));

        int i;
        for (i = 0; i <= 0xf00; i++) {
            if (!(*statx & 0x7FF))
                break;
        }

        *SD_A_KOFF_HI(core) = 0xFFFF;
        *SD_A_KOFF_LO(core) = 0xFFFF; // Should probably only be 0xFF
    }

    *SD_S_PMON_HI(1) = 0;
    *SD_S_PMON_LO(1) = 0;
    *SD_S_NON_HI(1) = 0;
    *SD_S_NON_LO(1) = 0;
}

int _start(int argc, char **argv)
{
    spuReset();

   /* Let the SPU2 assert any interrupt that it needed to assert here.
      Otherwise, the IOP may crash when OSDSYS loads CLEARSPU, as its interrupt handler jumps to a NULL pointer.
      For reasons, this doesn't seem to work when interrupts are enabled before the SPU reset. */
    EnableIntr(IOP_IRQ_DMA_SPU);
    EnableIntr(IOP_IRQ_DMA_SPU2);
    EnableIntr(IOP_IRQ_SPU);

    return MODULE_NO_RESIDENT_END;
}

