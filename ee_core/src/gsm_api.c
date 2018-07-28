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

#define MAKE_J(func) (u32)((0x02 << 26) | (((u32)func) / 4)) // Jump (MIPS instruction)
#define NOP 0x00000000                                       // No Operation (MIPS instruction)

extern void (*Old_SetGsCrt)(short int interlace, short int mode, short int ffmd);

struct GSMDestSetGsCrt
{
    unsigned int interlace;
    unsigned int mode;
    unsigned int ffmd;
};

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
};

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
};

extern struct GSMDestSetGsCrt GSMDestSetGsCrt;
extern struct GSMDestGSRegs GSMDestGSRegs;
extern struct GSMFlags GSMFlags;

extern void Hook_SetGsCrt();
extern void GSHandler();

static unsigned int KSEG_backup[2]; //Copies of the original words at 0x80000100 and 0x80000104.

/*-------------------*/
/* Update GSM params */
/*-------------------*/
// Update parameters to be enforced by Hook_SetGsCrt syscall hook and GSHandler service routine functions
void UpdateGSMParams(u32 interlace, u32 mode, u32 ffmd, u64 display, u64 syncv, u64 smode2, u32 dx_offset, u32 dy_offset)
{
    GSMDestSetGsCrt.interlace = (u32)interlace;
    GSMDestSetGsCrt.mode = (u32)mode;
    GSMDestSetGsCrt.ffmd = (u32)ffmd;

    GSMDestGSRegs.smode2 = (u8)smode2;
    GSMDestGSRegs.display1 = (u64)display;
    GSMDestGSRegs.display2 = (u64)display;
    GSMDestGSRegs.syncv = (u64)syncv;

    GSMFlags.dx_offset = (u32)dx_offset; // X-axis offset -> Use it only when automatic adaptations formulas don't suffice
    GSMFlags.dy_offset = (u32)dy_offset; // Y-axis offset -> Use it only when automatic adaptations formulas don't suffice
    // 0 = Off, 1 = On
    GSMFlags.ADAPTATION_fix = (u8)1; // Default = 1 = On
    GSMFlags.PMODE_fix = (u8)0;      // Default = 0 = Off
    GSMFlags.SMODE1_fix = (u8)0;     // Default = 0 = Off
    GSMFlags.SMODE2_fix = (u8)1;     // Default = 1 = On
    GSMFlags.SRFSH_fix = (u8)0;      // Default = 0 = Off
    GSMFlags.SYNCH_fix = (u8)0;      // Default = 0 = Off
    GSMFlags.SYNCV_fix = (u8)1;      // Default = 1 = On
    GSMFlags.DISPFB_fix = (u8)1;     // Default = 1 = On
    GSMFlags.DISPLAY_fix = (u8)1;    // Default = 1 = On
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

    //Restore the original stuff at the level 2 exception handler.
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
