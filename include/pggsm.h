/*
#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013 Bat Rastard
# Copyright 2014, 2015, 2016 doctorxyz
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#
*/

// Header For Per-Game GSM -Bat-

#ifndef __PGGSM_H
#define __PGGSM_H

#define GSM_VERSION "0.40"
#define GSM_ARGS    1
#include "../ee_core/include/coreconfig.h"

#define makeSMODE1(VHP, VCKSEL, SLCK2, NVCK, CLKSEL, PEVS, PEHS, PVS, PHS, GCONT, SPML, PCK2, XPCK, SINT, PRST, EX, CMOD, SLCK, T1248, LC, RC) \
    (u64)(((u64)(VHP) << 36) | ((u64)(VCKSEL) << 34) | ((u64)(SLCK2) << 33) |                                                                  \
          ((u64)(NVCK) << 32) | ((u64)(CLKSEL) << 30) | ((u64)(PEVS) << 29) |                                                                  \
          ((u64)(PEHS) << 28) | ((u64)(PVS) << 27) | ((u64)(PHS) << 26) |                                                                      \
          ((u64)(GCONT) << 25) | ((u64)(SPML) << 21) | ((u64)(PCK2) << 19) |                                                                   \
          ((u64)(XPCK) << 18) | ((u64)(SINT) << 17) | ((u64)(PRST) << 16) |                                                                    \
          ((u64)(EX) << 15) | ((u64)(CMOD) << 13) | ((u64)(SLCK) << 12) |                                                                      \
          ((u64)(T1248) << 10) | ((u64)(LC) << 3) | ((u64)(RC) << 0))
#define makeSYNCH1(HS, HSVS, HSEQ, HBP, HFP)                              \
    (u64)(((u64)(HS) << 43) | ((u64)(HSVS) << 32) | ((u64)(HSEQ) << 22) | \
          ((u64)(HBP) << 11) | ((u64)(HFP) << 0))
#define makeSYNCH2(HB, HF) \
    (u64)(((u64)(HB) << 11) | ((u64)(HF) << 0))
#define makeSYNCV(VS, VDP, VBPE, VBP, VFPE, VFP)                         \
    (u64)(((u64)(VS) << 53) | ((u64)(VDP) << 42) | ((u64)(VBPE) << 32) | \
          ((u64)(VBP) << 20) | ((u64)(VFPE) << 10) | ((u64)(VFP) << 0))
#define makeDISPLAY(DH, DW, MAGV, MAGH, DY, DX)                         \
    (u64)(((u64)(DH) << 44) | ((u64)(DW) << 32) | ((u64)(MAGV) << 27) | \
          ((u64)(MAGH) << 23) | ((u64)(DY) << 12) | ((u64)(DX) << 0))

typedef struct predef_vmode_struct
{
    u8 interlace;
    u8 mode;
    u8 ffmd;
    u64 display;
    u64 syncv;
} predef_vmode_struct;

void InitGSMConfig(config_set_t *configSet);
int GetGSMEnabled(void);
void PrepareGSM(char *cmdline, struct GsmConfig_t *config);

#endif
