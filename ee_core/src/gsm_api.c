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

#include <tamtypes.h>
#include <kernel.h>
#include <syscallnr.h>

#include "gsm_api.h"
#include "dve_reg.h"

#define MAKE_J(func) (u32)((0x02 << 26) | (((u32)func) / 4)) // Jump (MIPS instruction)
#define NOP          0x00000000                              // No Operation (MIPS instruction)

extern void (*Old_SetGsCrt)(short int interlace, short int mode, short int ffmd);

// Taken from gsKit
// NTSC
#define GS_MODE_NTSC 0x02
// PAL
#define GS_MODE_PAL  0x03

struct GSMDestSetGsCrt
{
    s16 interlace;
    s16 mode;
    s16 ffmd;
} __attribute__((packed));

struct GSMDestGSRegs
{
    u64 pmode;
    u64 smode1;
    u64 smode2;
    u64 srfsh;
    u64 synch1;
    u64 synch2;
    u64 syncv;
    u64 dispfb1;
    u64 display1;
    u64 dispfb2;
    u64 display2;
} __attribute__((packed));

struct GSMFlags
{
    u32 dx_offset;
    u32 dy_offset;
    u8 ADAPTATION_fix;
    u8 PMODE_fix;
    u8 SMODE1_fix;
    u8 SMODE2_fix;
    u8 SRFSH_fix;
    u8 SYNCH_fix;
    u8 SYNCV_fix;
    u8 DISPFB_fix;
    u8 DISPLAY_fix;
    u8 FIELD_fix;
    u8 gs576P_param;
} __attribute__((packed));

extern struct GSMDestSetGsCrt GSMDestSetGsCrt;
extern struct GSMDestGSRegs GSMDestGSRegs;
extern struct GSMFlags GSMFlags;

extern void Hook_SetGsCrt();
extern void GSHandler();

static unsigned int KSEG_backup[2]; // Copies of the original words at 0x80000100 and 0x80000104.

/* Update GSM params */
/*-------------------*/
/*-------------------*/
// Update parameters to be enforced by Hook_SetGsCrt syscall hook and GSHandler service routine functions
void UpdateGSMParams(s16 interlace, s16 mode, s16 ffmd, u64 display, u64 syncv, u64 smode2, u32 dx_offset, u32 dy_offset, int k576p_fix, int kGsDxDyOffsetSupported, int FIELD_fix)
{
    unsigned int hvParam = GetGsVParam();
    int gs_DH, gs_DW, gs_DY, gs_DX;

    GSMDestSetGsCrt.interlace = interlace;
    GSMDestSetGsCrt.mode = mode;
    GSMDestSetGsCrt.ffmd = ffmd;

    GSMDestGSRegs.smode2 = (u8)smode2;
    GSMDestGSRegs.display1 = (u64)display;
    GSMDestGSRegs.display2 = (u64)display;
    GSMDestGSRegs.syncv = (u64)syncv;

    GSMFlags.dx_offset = (u32)dx_offset; // X-axis offset -> Use it only when automatic adaptations formulas don't suffice
    GSMFlags.dy_offset = (u32)dy_offset; // Y-axis offset -> Use it only when automatic adaptations formulas don't suffice
    // 0 = Off, 1 = On
    GSMFlags.ADAPTATION_fix = 1; // Default = 1 = On
    GSMFlags.PMODE_fix = 0;      // Default = 0 = Off
    GSMFlags.SMODE1_fix = 0;     // Default = 0 = Off
    GSMFlags.SMODE2_fix = 1;     // Default = 1 = On
    GSMFlags.SRFSH_fix = 0;      // Default = 0 = Off
    GSMFlags.SYNCH_fix = 0;      // Default = 0 = Off
    GSMFlags.SYNCV_fix = 0;      // Default = 0 = Off
    GSMFlags.DISPFB_fix = 1;     // Default = 1 = On
    GSMFlags.DISPLAY_fix = 1;    // Default = 1 = On
    GSMFlags.FIELD_fix = (FIELD_fix ? (1 << 2) : 0);
    GSMFlags.gs576P_param = (k576p_fix ? (1 << 1) : 0) | (hvParam & 1);

    if (kGsDxDyOffsetSupported && (!(mode >= GS_MODE_NTSC && mode <= GS_MODE_PAL))) {
        _GetGsDxDyOffset(mode, &gs_DX, &gs_DY, &gs_DW, &gs_DH);
        GSMFlags.dx_offset += gs_DX;
        GSMFlags.dy_offset += gs_DY;
    }
}

/*------------------------------------------------------------------*/
/* Replace SetGsCrt in kernel. (Graphics Synthesizer Mode Selector) */
/*------------------------------------------------------------------*/
static inline void Install_Hook_SetGsCrt(void)
{
    Old_SetGsCrt = GetSyscallHandler(__NR_SetGsCrt);
    SetSyscall(__NR_SetGsCrt, Hook_SetGsCrt);
}

/*-----------------------------------------------------------------*/
/* Restore original SetGsCrt. (Graphics Synthesizer Mode Selector) */
/*-----------------------------------------------------------------*/
static inline void Remove_Hook_SetGsCrt(void)
{
    SetSyscall(__NR_SetGsCrt, Old_SetGsCrt);
}

/*----------------------------------------------------------------------------------------------------*/
/* Install Display Handler in place of the Core Debug Exception Handler (V_DEBUG handler)             */
/* Exception Vector Address for Debug Level 2 Exception when Stadus.DEV bit is 0 (normal): 0x80000100 */
/* 'Level 2' is a generalization of Error Level (from previous MIPS processors)                       */
/* When this exception is recognized, control is transferred to the applicable service routine;       */
/* in our case the service routine is 'GSHandler'!                                                    */
/*----------------------------------------------------------------------------------------------------*/
static void Install_GSHandler(void)
{
    DI();
    ee_kmode_enter();

    KSEG_backup[0] = *(volatile u32 *)0x80000100;
    KSEG_backup[1] = *(volatile u32 *)0x80000104;

    *(volatile u32 *)0x80000100 = MAKE_J((int)GSHandler);
    *(volatile u32 *)0x80000104 = NOP;
    ee_kmode_exit();

    // Set Data Address Write Breakpoint
    // Trap writes to GS registers, so as to control their values
    Enable_GSBreakpoint();
    EI();

    FlushCache(0);
    FlushCache(2);
}

static void Remove_GSHandler(void)
{
    DI();

    Disable_GSBreakpoint();

    // Restore the original stuff at the level 2 exception handler.
    ee_kmode_enter();
    *(volatile u32 *)0x80000100 = KSEG_backup[0];
    *(volatile u32 *)0x80000104 = KSEG_backup[1];
    ee_kmode_exit();
    EI();

    FlushCache(0);
    FlushCache(2);
}

/*-------------------------------------------*/
/* Enable Graphics Synthesizer Mode Selector */
/*-------------------------------------------*/
void EnableGSM(void)
{
    // Install Hook SetGsCrt
    Install_Hook_SetGsCrt();
    // Install Display Handler
    Install_GSHandler();
}

/*--------------------------------------------*/
/* Disable Graphics Synthesizer Mode Selector */
/*--------------------------------------------*/
void DisableGSM(void)
{
    // Remove Hook SetGsCrt
    Remove_Hook_SetGsCrt();
    // Remove Display Handler
    Remove_GSHandler();
}

/*--------------------------------------------*/
/* Set up the DVE for 576P mode               */
/*--------------------------------------------*/
void setdve_576P(void)
{ // The parameters are exactly the same as the 480P mode's. Regardless of the model, GS revision or region.
    dve_prepare_bus();

    dve_set_reg(0x77, 0x11);
    dve_set_reg(0x93, 0x01);
    dve_set_reg(0x91, 0x02);
}
