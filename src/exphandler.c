/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <stdio.h>
#include <tamtypes.h>
#include <kernel.h>
#include <debug.h>

#include "exceptions.h"

////////////////////////////////////////////////////////////////////////
typedef union __attribute__((packed))
{
    unsigned int uint128 __attribute__((mode(TI)));
    unsigned long uint64[2];
} eeReg;

////////////////////////////////////////////////////////////////////////
static const unsigned char regName[32][5] =
    {
        "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "t8", "t9", "s0", "s1", "s2", "s3", "s4", "s5",
        "s6", "s7", "k0", "k1", "gp", "sp", "fp", "ra"};

static char codeTxt[14][24] =
    {
        "Interrupt", "TLB modification", "TLB load/inst fetch", "TLB store",
        "Address load/inst fetch", "Address store", "Bus error (instr)",
        "Bus error (data)", "Syscall", "Breakpoint", "Reserved instruction",
        "Coprocessor unusable", "Arithmetic overflow", "Trap"};

char _exceptionStack[8 * 1024] __attribute__((aligned(16)));
eeReg _savedRegs[32 + 4] __attribute__((aligned(16)));

void* oldExceptionHandlers[14];
static s32 userThreadID = 0;
u32 _exceptionTriggered = 0;

////////////////////////////////////////////////////////////////////////
// The 'ee exception handler', only dumps registers to console or screen atm
void pkoDebug(int cause, int badvaddr, int status, int epc, eeReg *regs)
{
    int i;
    int code;
    static void (*excpPrintf)(const char *, ...);

    FlushCache(0);
    FlushCache(2);

    if (userThreadID) {
        TerminateThread(userThreadID);
        DeleteThread(userThreadID);
    }

    code = cause & 0x7c;

    //if (excepscrdump) {
        init_scr();
        excpPrintf = scr_printf;
    //} else
    //    excpPrintf = (void *)printf;

    _exceptionTriggered = 1;
    //_print("Exception Cause %08x  BadVAddr %08x  Status %08x  EPC %08x\n", cause, badvaddr, status, epc);

    excpPrintf("\n\n           EE Exception handler: %s exception\n\n",
               codeTxt[code >> 2]);

    excpPrintf("      Cause %08X  BadVAddr %08X  Status %08X  EPC %08X\n\n",
               cause, badvaddr, status, epc);

    for (i = 0; i < 32 / 2; i++) {
        excpPrintf("%4s: %016lX-%016lX %4s: %016lX-%016lX\n",
                   regName[i], regs[i].uint64[1], regs[i].uint64[0],
                   regName[i + 16], regs[i + 16].uint64[1], regs[i + 16].uint64[0]);
    }
    excpPrintf("\n");

    u32* pStack = (u32*)regs[29].uint64[0];
    for (i = 0; i < 5; i++)
    {
        excpPrintf("+0x%02X %08x %08x %08x %08x %08x %08x %08x %08x\n", (i * 0x20), pStack[(i * 0x20) + 0], pStack[(i * 0x20) + 1], pStack[(i * 0x20) + 2], pStack[(i * 0x20) + 4],
            pStack[(i * 0x20) + 4], pStack[(i * 0x20) + 5], pStack[(i * 0x20) + 6], pStack[(i * 0x20) + 7]);
    }
    SleepThread();
}


////////////////////////////////////////////////////////////////////////
// Installs ee exception handlers for the 'usual' exceptions
void installExceptionHandlers(void)
{
    int i;

    userThreadID = GetThreadId();

    // Skip exception #8 (syscall) & 9 (breakpoint)
    for (i = 1; i < 4; i++) {
        oldExceptionHandlers[i] = GetExceptionHandler(i);
        SetVTLBRefillHandler(i, pkoExceptionHandler);
    }
    for (i = 4; i < 8; i++) {
        oldExceptionHandlers[i] = GetExceptionHandler(i);
        SetVCommonHandler(i, pkoExceptionHandler);
    }
    for (i = 10; i < 14; i++) {
        oldExceptionHandlers[i] = GetExceptionHandler(i);
        SetVCommonHandler(i, pkoExceptionHandler);
    }

    FlushCache(0);
    FlushCache(2);
}

void restoreExceptionHandlers()
{
    int i;

    // Skip exception #8 (syscall) & 9 (breakpoint)
    for (i = 1; i < 4; i++) {
        SetVTLBRefillHandler(i, oldExceptionHandlers[i]);
    }
    for (i = 4; i < 8; i++) {
        SetVCommonHandler(i, oldExceptionHandlers[i]);
    }
    for (i = 10; i < 14; i++) {
        SetVCommonHandler(i, oldExceptionHandlers[i]);
    }

    FlushCache(0);
    FlushCache(2);
}
