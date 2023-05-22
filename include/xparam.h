/*
#
# DECKARD Configs
#--------------------------
# Copyright 2023 krat0s.
#
*/


#define GM_IOP_TYPE (0x80000000)

#define PARAM_MDEC_DELAY_CYCLE         (0x00)
#define PARAM_SPU_INT_DELAY_LIMIT      (0x01)
#define PARAM_SPU_INT_DELAY_PPC_COEFF  (0x02)
#define PARAM_SPU2_INT_DELAY_LIMIT     (0x03)
#define PARAM_SPU2_INT_DELAY_PPC_COEFF (0x04)
#define PARAM_DMAC_CH10_INT_DELAY      (0x05)
#define PARAM_CPU_DELAY                (0x06)
#define PARAM_SPU_DMA_WAIT_LIMIT       (0x07)
#define PARAM_GPU_DMA_WAIT_LIMIT       (0x08)
#define PARAM_DMAC_CH10_INT_DELAY_DPC  (0x09)
#define PARAM_CPU_DELAY_DPC            (0x0A)
#define PARAM_USB_DELAYED_INT_ENABLE   (0x0B)
#define PARAM_TIMER_LOAD_DELAY         (0x0C)
#define PARAM_SIO0_DTR_SCK_DELAY       (0x0D)
#define PARAM_SIO0_DSR_SCK_DELAY_C     (0x0E)
#define PARAM_SIO0_DSR_SCK_DELAY_M     (0x0F)
#define PARAM_MIPS_DCACHE_ON           (0x10)
#define PARAM_CACHE_FLASH_CHANNELS     (0x11)

#define GM_IF ((vu32 *)0x1F801450)

void ResetDeckardXParams();
void ApplyDeckardXParam(const char *title);
