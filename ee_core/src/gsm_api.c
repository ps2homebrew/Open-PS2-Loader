/*
#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#
*/

#include <tamtypes.h>
#include <kernel.h>
#include <syscallnr.h>

#define MAKE_J(func)		(u32)( (0x02 << 26) | (((u32)func) / 4) )	// Jump (MIPS instruction)
#define NOP					0x00000000									// No Operation (MIPS instruction)

extern void (*Old_SetGsCrt)(short int interlace, short int mode, short int ffmd);

struct VModeSettings{
	unsigned int interlace;
	unsigned int mode;
	unsigned int ffmd;
};

struct GSRegisterValues{
	u64 smode2;
	u64 display;
	u64 syncv;
};

struct GSRegisterFixValues{
	u8 automatic_adaptation;
	u8 smode2;
	u8 display;
	u8 syncv;
	u32 X_offset;
	u32 Y_offset;
};

struct GSRegisterAdaptationValues{
	u64 display;
	u8 double_height;
	u8 smode2;
};

extern struct VModeSettings Source_VModeSettings;
extern struct VModeSettings Target_VModeSettings;
extern struct GSRegisterValues Target_GSRegisterValues;
extern struct GSRegisterFixValues GSRegisterFixValues;

extern void Hook_SetGsCrt();
extern void GSHandler();

static unsigned int KSEG_backup[2];	//Copies of the original words at 0x80000100 and 0x80000104.

/*-------------------*/
/* Update GSM params */
/*-------------------*/
// Update parameters to be enforced by Hook_SetGsCrt syscall hook and GSHandler service routine functions
void UpdateGSMParams(u32 interlace, u32 mode, u32 ffmd, u64 display, u64 syncv, u64 smode2, int dx_offset, int dy_offset)
{
	Target_VModeSettings.interlace 	= (u32) interlace;
	Target_VModeSettings.mode	= (u32) mode;
	Target_VModeSettings.ffmd	= (u32) ffmd;

	Target_GSRegisterValues.smode2 	= (u8) smode2;
	Target_GSRegisterValues.display	= (u64) display;
	Target_GSRegisterValues.syncv	= (u64) syncv;

	GSRegisterFixValues.automatic_adaptation	= 0;	// Automatic Adaptation -> 0 = On, 1 = Off ; Default = 0 = On
	GSRegisterFixValues.display			= 0;	// DISPLAYx Fix ---------> 0 = On, 1 = Off ; Default = 0 = On
	GSRegisterFixValues.smode2			= 0;	// SMODE2 Fix -----------> 0 = On, 1 = Off ; Default = 0 = On
	GSRegisterFixValues.syncv			= 0;	// SYNCV Fix ------------> 0 = On, 1 = Off ; Default = 0 = On

	GSRegisterFixValues.X_offset	= dx_offset;	// X-axis offset -> Use it only when automatic adaptations formulas don't suffice
	GSRegisterFixValues.Y_offset	= dy_offset;	// Y-axis offset -> Use it only when automatic adaptations formulas don't suffice
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

	KSEG_backup[0]=*(volatile u32 *)0x80000100;
	KSEG_backup[1]=*(volatile u32 *)0x80000104;

	*(volatile u32 *)0x80000100 = MAKE_J((int)GSHandler);
	*(volatile u32 *)0x80000104 = NOP;
	ee_kmode_exit();

/*
	"li $a0, 0x12000000\n"	// Address base for trapping
	"li $a1, 0x1FFFFF1F\n"	// Address mask for trapping
	//We trap writes to 0x12000000 + 0x00,0x20,0x40,0x60,0x80,0xA0,0xC0,0xE0
	//We only want 0x20, 0x60, 0x80, 0xA0, but can't mask for that combination
	//But the trapping range is now extended to match all kernel access segments
*/

	// Set Data Address Write Breakpoint
	// Trap writes to GS registers, so as to control their values
	__asm__ __volatile__ (
	".set noreorder\n"
	".set noat\n"
	
	"li $a0, 0x12000000\n"	// Address base for trapping
	"li $a1, 0x1FFFFE1F\n"	// Address mask for trapping	//DOCTORXYZ
	//We trap writes to 0x12000000 + 0x00,0x20,0x40,0x60,0x80,0xA0,0xC0,0xE0,0x100,0x120,0x140,0x160,0x180,0x1A0,0x1C0,0x1E0	//DOCTORXYZ
	//We only want 0x20, 0x60, 0x80, 0xA0, 0x100, but can't mask for that combination //DOCTORXYZ
	//But the trapping range is now extended to match all kernel access segments

	"li $k0, 0x8000\n"
	"mtbpc $k0\n"			// All breakpoints off (BED = 1)

	"sync.p\n"						// Await instruction completion

	"mtdab	$a0\n"
	"mtdabm	$a1\n"

	"sync.p\n"						// Await instruction completion

	"mfbpc $k1\n"
	"sync.p\n"						// Await instruction completion

	"li $k0, 0x20200000\n"			// Data write breakpoint on (DWE, DUE = 1)
	"or $k1, $k1, $k0\n"
	"xori $k1, $k1, 0x8000\n"		// DEBUG exception trigger on (BED = 0)
	"mtbpc $k1\n"
	"sync.p\n"						//  Await instruction completion
	
	".set at\n"
	".set reorder\n"
	);

	EI();

	FlushCache(0);
	FlushCache(2);
}

static void Remove_GSHandler(void)
{
	DI();

	__asm__ __volatile__ (
	".set noreorder\n"
	".set noat\n"
	"li $k0, 0x8000\n"
	"mtbpc $k0\n"		// All breakpoints off (BED = 1)
	"sync.p\n"		// Await instruction completion
	
	".set at\n"
	".set reorder\n"
	);

	//Restore the original stuff at the level 2 exception handler.
	ee_kmode_enter();
	*(volatile u32 *)0x80000100=KSEG_backup[0];
	*(volatile u32 *)0x80000104=KSEG_backup[1];
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
