/*
#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013 doctorxyz
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#
*/

#include <syscallnr.h>
#include <kernel.h>
#include <iopcontrol.h>

#define MAKE_J(func)		(u32)( (0x02 << 26) | (((u32)func) / 4) )	// Jump (MIPS instruction)
#define KSEG0(addr)			((void*)(((u32)(addr)) | 0x80000000))

// Prototypes for External Functions
#define _GSM_ENGINE_ __attribute__((section(".gsm_engine")))		// Resident section

extern void *Old_SetGsCrt _GSM_ENGINE_;

extern u32 Source_INTERLACE _GSM_ENGINE_;
extern u32 Source_MODE _GSM_ENGINE_;
extern u32 Source_FFMD _GSM_ENGINE_;

extern u64 Calculated_DISPLAY1 _GSM_ENGINE_;
extern u64 Calculated_DISPLAY2 _GSM_ENGINE_;

extern u32 Target_INTERLACE _GSM_ENGINE_;
extern u32 Target_MODE _GSM_ENGINE_;
extern u32 Target_FFMD _GSM_ENGINE_;

extern u64 Target_SMODE2 _GSM_ENGINE_;
extern u64 Target_DISPLAY1 _GSM_ENGINE_;
extern u64 Target_DISPLAY2 _GSM_ENGINE_;
extern u64 Target_SYNCV _GSM_ENGINE_;

extern u8 automatic_adaptation _GSM_ENGINE_;
extern u8 DISPLAY_fix _GSM_ENGINE_;
extern u8 SMODE2_fix _GSM_ENGINE_;
extern u8 SYNCV_fix _GSM_ENGINE_;
extern u8 skip_videos_fix _GSM_ENGINE_;

extern u32 X_offset _GSM_ENGINE_;
extern u32 Y_offset _GSM_ENGINE_;

extern void Hook_SetGsCrt() _GSM_ENGINE_;
extern void GSHandler() _GSM_ENGINE_;

_GSM_ENGINE_ int DisableInterrupts()
{
        int eie, res;

        asm volatile ("mfc0\t%0, $12" : "=r" (eie));
        eie &= 0x10000;
        res = eie != 0;

        if (!eie)
                return 0;

        asm (".p2align 3");
        do {
                asm volatile ("di");
                asm volatile ("sync.p");
                asm volatile ("mfc0\t%0, $12" : "=r" (eie));
                eie &= 0x10000;
        } while (eie);

        return res;
}

_GSM_ENGINE_ int EnableInterrupts()
{
        int eie;

        asm volatile ("mfc0\t%0, $12" : "=r" (eie));
        eie &= 0x10000;
        asm volatile ("ei");

        return eie != 0;
}

_GSM_ENGINE_ int EEKernelModeEnter() {
	u32 status, mask;

	__asm__ volatile (
		".set\tpush\n\t"		\
		".set\tnoreorder\n\t"		\
		"mfc0\t%0, $12\n\t"		\
		"li\t%1, 0xffffffe7\n\t"	\
		"and\t%0, %1\n\t"		\
		"mtc0\t%0, $12\n\t"		\
		"sync.p\n\t"
		".set\tpop\n\t" : "=r" (status), "=r" (mask));

	return status;
}

_GSM_ENGINE_ int EEKernelModeExit() {
	int status;

	__asm__ volatile (
		".set\tpush\n\t"		\
		".set\tnoreorder\n\t"		\
		"mfc0\t%0, $12\n\t"		\
		"ori\t%0, 0x10\n\t"		\
		"mtc0\t%0, $12\n\t"		\
		"sync.p\n\t" \
		".set\tpop\n\t" : "=r" (status));

	return status;
}

_GSM_ENGINE_ void SetSyscall2(int number, void (*functionPtr)(void)) {
	__asm__ __volatile__ (
	".set noreorder\n"
	".set noat\n"
	"li $3, 0x74\n"
    "add $4, $0, %0    \n"   // Specify the argument #1
    "add $5, $0, %1    \n"   // Specify the argument #2
   	"syscall\n"
	"jr $31\n"
	"nop\n"
	".set at\n"
	".set reorder\n"
    :
    : "r"( number ), "r"( functionPtr )
    );
}

_GSM_ENGINE_ u32* GetROMSyscallVectorTableAddress(void) {
	//Search for Syscall Table in ROM
	u32 i;
	u32 startaddr;
	u32* ptr;
	u32* addr;
	startaddr = 0;
	for (i = 0x1FF00000; i < 0x1FFFFFFF; i+= 4)
	{
		if ( *(u32*)(i + 0) == 0x40196800 )
		{
			if ( *(u32*)(i + 4) == 0x3C1A8001 )
			{
				startaddr = i - 8;
				break;
			}
		}
	}
	ptr = (u32 *) (startaddr + 0x02F0);
	addr = (u32*)((ptr[0] << 16) | (ptr[2] & 0xFFFF));
	addr = (u32*)((u32)addr & 0x1fffffff);
	addr = (u32*)((u32)addr + startaddr);
	return addr;
}

_GSM_ENGINE_ void InitGSM(u32 interlace, u32 mode, u32 ffmd, u64 display, u64 syncv, u64 smode2, int dx_offset, int dy_offset, u8 skip_videos)
 {
	u32* ROMSyscallTableAddress;

	// Update GSM params
	DisableInterrupts();
	EEKernelModeEnter();

	Target_INTERLACE		= interlace;
	Target_MODE				= mode;
	Target_FFMD				= ffmd;
	Target_DISPLAY1			= display;
	Target_DISPLAY2			= display;
	Target_SYNCV			= syncv;
	Target_SMODE2			= smode2;
	X_offset				= dx_offset;		// X-axis offset -> Use it only when automatic adaptations formulas don't fit into your needs
	Y_offset				= dy_offset;		// Y-axis offset -> Use it only when automatic adaptations formulas dont't fit into your needs
	skip_videos_fix			= skip_videos ^ 1;	// Skip Videos Fix ------------> 0 = On, 1 = Off ; Default = 0 = On
	
	automatic_adaptation	= 0;				// Automatic Adaptation -> 0 = On, 1 = Off ; Default = 0 = On
	DISPLAY_fix				= 0;				// DISPLAYx Fix ---------> 0 = On, 1 = Off ; Default = 0 = On
	SMODE2_fix				= 0;				// SMODE2 Fix -----------> 0 = On, 1 = Off ; Default = 0 = On
	SYNCV_fix				= 0;				// SYNCV Fix ------------> 0 = On, 1 = Off ; Default = 0 = On

	EEKernelModeExit();
	EnableInterrupts();

	// Preparation for SetGsCrt Hooking
	ROMSyscallTableAddress = GetROMSyscallVectorTableAddress();
	Old_SetGsCrt = (void*)ROMSyscallTableAddress[2];

	// Remove all breakpoints (even when they aren't enabled)
	__asm__ __volatile__ (
	".set noreorder\n"
	".set noat\n"
	"li $k0, 0x8000\n"
	"mtbpc $k0\n"			// All breakpoints off (BED = 1)
	"sync.p\n"				// Await instruction completion
	".set at\n"
	".set reorder\n"
	);

	// Replace Core Debug Exception Handler (V_DEBUG handler) by GSHandler
	DisableInterrupts();
	EEKernelModeEnter();
	*(u32 *)0x80000100 = MAKE_J((int)GSHandler);
	*(u32 *)0x80000104 = 0;
	EEKernelModeExit();
	EnableInterrupts();
}

void StartGSM(void) {
	SetSyscall2(2, KSEG0(&Hook_SetGsCrt));
}

void StopGSM(void) {
	u32* ROMSyscallTableAddress;

	// Needed for SetGsCrt Unhooking
	ROMSyscallTableAddress = GetROMSyscallVectorTableAddress();

	DisableInterrupts();
	EEKernelModeEnter();
	
	// Restore SetGsCrt
	SetSyscall2(2, (void*)ROMSyscallTableAddress[2]);

	// Remove all breakpoints (even when they aren't enabled)
	__asm__ __volatile__ (
	".set noreorder\n"
	".set noat\n"
	"li $k0, 0x8000\n"
	"mtbpc $k0\n"			// All breakpoints off (BED = 1)
	"sync.p\n"				// Await instruction completion
	".set at\n"
	".set reorder\n"
	);

	EEKernelModeExit();
	EnableInterrupts();
}
