/*
  spu.c Open PS2 Loader
 
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Reset SPU function taken from PS2SDK freesd.
  Copyright (c) 2004 TyRaNiD <tiraniddo@hotmail.com>
  Copyright (c) 2004,2007 Lukasz Bruun <mail@lukasz.dk>
*/

#include "ee_core.h"
#include "util.h"


// SPU2 Registers
#define U16_REGISTER(x) ((volatile u16 *)(0xBF900000 | (x)))
#define U32_REGISTER(x) ((volatile u32 *)(0xBF800000 | (x)))
// SD_CORE_ATTR Macros
#define SD_SPU2_ON (1 << 15)
// SPU DMA Channels 0,1 - 1088 bytes apart
#define SD_DMA_CHCR(ch) ((volatile u32 *)(0xBF8010C8 + (ch * 1088)))
#define SD_DMA_START (1 << 24)
// SPDIF OUT
#define SD_C_SPDIF_OUT ((volatile u16 *)0xBF9007C0)
// Base of SPU2 regs is 0x0xBF900000
#define SD_BASE_REG(reg) ((volatile u16 *)(0xBF900000 + reg))
#define SD_A_REG(core, reg) SD_BASE_REG(0x1A0 + ((core) << 10) + (reg))
#define SD_A_KOFF_HI(core) SD_A_REG((core), 0x04)
#define SD_A_KOFF_LO(core) SD_A_REG((core), 0x06)
#define SD_P_REG(core, reg) SD_BASE_REG(0x760 + ((core)*40) + (reg))
#define SD_P_MVOLL(core) SD_P_REG((core), 0x00)
#define SD_P_MVOLR(core) SD_P_REG((core), 0x02)
#define SD_S_REG(core, reg) SD_BASE_REG(0x180 + ((core) << 10) + (reg))
#define SD_S_PMON_HI(core) SD_S_REG((core), 0x00)
#define SD_S_PMON_LO(core) SD_S_REG((core), 0x02)
#define SD_S_NON_HI(core) SD_S_REG((core), 0x04)
#define SD_S_NON_LO(core) SD_S_REG((core), 0x06)
#define SD_CORE_ATTR(core) SD_S_REG((core), 0x1A)

// Reset SPU sound processor
void ResetSPU(void)
{
    u32 core;
    volatile u16 *statx;

    DIntr();
    ee_kmode_enter();

    // Stop SPU Dma Core 0
    *SD_DMA_CHCR(0) &= ~SD_DMA_START;
    *U16_REGISTER(0x1B0) = 0;

    // Stop SPU Dma Core 1
    *SD_DMA_CHCR(1) &= ~SD_DMA_START;
    *U16_REGISTER(0x1B0 + 1024) = 0;

    // Initialize SPU2
    *U32_REGISTER(0x1404) = 0xBF900000;
    *U32_REGISTER(0x140C) = 0xBF900800;
    *U32_REGISTER(0x10F0) |= 0x80000;
    *U32_REGISTER(0x1570) |= 8;
    *U32_REGISTER(0x1014) = 0x200B31E1;
    *U32_REGISTER(0x1414) = 0x200B31E1;

    *SD_C_SPDIF_OUT = 0;
    delay(1);
    *SD_C_SPDIF_OUT = 0x8000;
    delay(1);

    *U32_REGISTER(0x10F0) |= 0xB0000;

    for (core = 0; core < 2; core++) {
        *U16_REGISTER(0x1B0) = 0;
        *SD_CORE_ATTR(core) = 0;
        delay(1);
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

    ee_kmode_exit();
    EIntr();
}
