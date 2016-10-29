#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013, 2014, 2015, 2016 doctorxyz
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#

#include <ee_cop0_defs.h>
#include <syscallnr.h>

.equ GS_BASE,	  0x12000000
.equ GS_PMODE,    0x0000
.equ GS_SMODE1,   0x0010
.equ GS_SMODE2,   0x0020
.equ GS_SRFSH,    0x0030
.equ GS_SYNCH1,   0x0040
.equ GS_SYNCH2,   0x0050
.equ GS_SYNCV,    0x0060
.equ GS_DISPFB1,  0x0070
.equ GS_DISPLAY1, 0x0080
.equ GS_DISPFB2,  0x0090
.equ GS_DISPLAY2, 0x00A0
.equ GS_BGCOLOUR, 0x00E0
.equ GS_CSR,      0x1000

# Built-in video modes (made possible by SP193 and reprep)
GS_MODE_DTV_480P=0x50
GS_MODE_DTV_576P=0x53
GS_MODE_DTV_1080P=0x54

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

.set push

.set noreorder # disable assembler reorder mode, so the code will not be optimized or changed in any way, giving complete instruction order control to the programmer
.set noat # disable assembler from using register $1 (known as the assembler temporary, or $at register) to hold intermediate values when performing macro expansions
.set macro # disable warning if any statement expands to more than one machine instruction
#.set nomacro # enable warning if any statement expands to more than one machine instruction

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#####################################################
# data section
#####################################################

.p2align 4	# align 16 bytes (IMPORTANT!!!)
.data

############
# Variables
############


.globl MIPS_Regs
MIPS_Regs:			.space	0x200

.globl Old_SetGsCrt
Old_SetGsCrt:		.word 0

.globl GSMSourceSetGsCrt
GSMSourceSetGsCrt:	.space 12
.equ Source_INT,      0 # WORD
.equ Source_MODE,     4 # WORD
.equ Source_FFMD,     8 # WORD

.p2align 4	# align 16 bytes (IMPORTANT!!!)

.globl GSMSourceGSRegs
GSMSourceGSRegs: .space 88
.equ Source_PMODE,     0 # DWORD
.equ Source_SMODE1,    8 # DWORD
.equ Source_SMODE2,   16 # DWORD
.equ Source_SRFSH,    24 # DWORD
.equ Source_SYNCH1,   32 # DWORD
.equ Source_SYNCH2,   40 # DWORD
.equ Source_SYNCV,    48 # DWORD
.equ Source_DISPFB1,  56 # DWORD
.equ Source_DISPLAY1, 64 # DWORD
.equ Source_DISPFB2,  72 # DWORD
.equ Source_DISPLAY2, 80 # DWORD

.p2align 4	# align 16 bytes (IMPORTANT!!!)

.globl GSMDestSetGsCrt
GSMDestSetGsCrt:	.space 12
.equ Target_INT,      0 # WORD
.equ Target_MODE,     4 # WORD
.equ Target_FFMD,     8 # WORD

.p2align 4	# align 16 bytes (IMPORTANT!!!)

.globl GSMDestGSRegs
GSMDestGSRegs: .space 88
.equ Target_PMODE,     0 # DWORD
.equ Target_SMODE1,    8 # DWORD
.equ Target_SMODE2,   16 # DWORD
.equ Target_SRFSH,    24 # DWORD
.equ Target_SYNCH1,   32 # DWORD
.equ Target_SYNCH2,   40 # DWORD
.equ Target_SYNCV,    48 # DWORD
.equ Target_DISPFB1,  56 # DWORD
.equ Target_DISPLAY1, 64 # DWORD
.equ Target_DISPFB2,  72 # DWORD
.equ Target_DISPLAY2, 80 # DWORD

.globl GSMFlags
GSMFlags: .space 17
.equ X_offset,              0 # WORD
.equ Y_offset,              4 # WORD
.equ ADAPTATION_fix,        8 # BYTE
.equ PMODE_fix,             9 # BYTE
.equ SMODE1_fix,           10 # BYTE
.equ SMODE2_fix,           11 # BYTE
.equ SRFSH_fix,            12 # BYTE
.equ SYNCH_fix,            13 # BYTE
.equ SYNCV_fix,            14 # BYTE
.equ DISPFB_fix,           15 # BYTE
.equ DISPLAY_fix,          16 # BYTE

.p2align 4	# align 16 bytes (IMPORTANT!!!)

GSMAdapts:	.space 19
.equ Adapted_DISPLAY1,             0 # DWORD
.equ Adapted_DISPLAY2,             8 # DWORD
.equ Interlace_FRAME_Mode_Flag,   16 # BYTE -> Double Height for SMODE2's INT=1 (Interlace Mode) and FFMD=1 (FRAME Mode. Read every line) 
.equ SMODE2_adaptation,           17 # BYTE -> Adapted SMODE2 patch value
.equ RC_adaptation,               18 # BYTE -> RC (Read Circuit) adaptation for avoiding garbage on screen ex.: Sega Genesis Collection (SLUS_215.42)

.p2align 4	# align 16 bytes (IMPORTANT!!!)

###############################
# Opcode emulation jump tables
###############################
#
# They differentiate between different kinds of access that may have triggered the debug trap we use.
# This way the number of cases does not affect the time delay for testing.

# First we have a table with jump offsets for opcode dependency
op_t:
.word ignore, ignore,  ignore, ignore				# 00-03
.word ignore, ignore,  ignore, ignore				# 04-07
.word ignore, ignore,  ignore, ignore				# 08-0B
.word ignore, ignore,  ignore, ignore				# 0C-0F
.word ignore, ignore,  ignore, ignore				# 10-13
.word ignore, ignore,  ignore, ignore				# 14-17
.word ignore, ignore,  ldl_op, ldr_op				# 18-1B
.word ignore, ignore,  lq_op,  sq_op				# 1C-1F
.word lb_op,  lh_op,   lwl_op, lw_op				# 20-23
.word lbu_op, lhu_op,  lwr_op, lwu_op				# 24-27
.word sb_op,  sh_op,   swl_op, sw_op				# 28-2B
.word sdl_op, sdr_op,  swr_op, ignore				# 2C-2F
.word ignore, ignore,  ignore, ignore				# 30-33
.word ignore, ignore,  ignore, ld_op				# 34-37
.word ignore, ignore,  ignore, ignore				# 38-3B
.word ignore, ignore,  ignore, sd_op				# 3C-3F

# Table 1 for branch opcodes when trapping branch delay slot
BD_t1:
.word B_com0, B_com1, B_J, B_JAL				# 00-03
.word B_BEQ, B_BNE, B_BLEZ, B_BGTZ				# 04-07
.word B_skip, B_skip, B_skip, B_skip				# 08-0B
.word B_skip, B_skip, B_skip, B_skip				# 0C-0F
.word B_BC0x, B_BC1x, B_skip, B_skip				# 10-13
.word B_BEQL, B_BNEL, B_BLEZL, B_BGTZL				# 14-17
.word B_skip, B_skip, B_skip, B_skip				# 18-1B
.word B_skip, B_skip, B_skip, B_skip				# 1C-1F
.word B_skip, B_skip, B_skip, B_skip				# 20-23
.word B_skip, B_skip, B_skip, B_skip				# 24-27
.word B_skip, B_skip, B_skip, B_skip				# 28-2B
.word B_skip, B_skip, B_skip, B_skip				# 2C-2F
.word B_skip, B_skip, B_skip, B_skip				# 30-33
.word B_skip, B_skip, B_skip, B_skip				# 34-37
.word B_skip, B_skip, B_skip, B_skip				# 38-3B
.word B_skip, B_skip, B_skip, B_skip				# 3C-3F

# Table 2 for branch sub-opcodes when trapping branch delay slot
BD_t2:
.word B_BLTZ, B_BGEZ, B_BLTZL, B_BGEZL				# 00-03
.word B_skip, B_skip, B_skip, B_skip				# 04-07
.word B_skip, B_skip, B_skip, B_skip				# 08-0B
.word B_skip, B_skip, B_skip, B_skip				# 0C-0F
.word B_BLTZAL, B_BGEZAL, B_BLTZALL, B_BGEZALL			# 10-13
.word B_skip, B_skip, B_skip, B_skip				# 14-17
.word B_skip, B_skip, B_skip, B_skip				# 18-1B
.word B_skip, B_skip, B_skip, B_skip				# 1C-1F

#####################################################
# .text section
#####################################################

.p2align 4	# align 16 bytes (IMPORTANT!!!)
.text

############
# Functions
############

#
# Hook_SetGsCrt:
#
.globl Hook_SetGsCrt
.ent	Hook_SetGsCrt
Hook_SetGsCrt:
addiu	$sp, $sp, -0x0010			# reserve 16 bytes stack space (1 reg)
sd	$ra, 0($sp)				# Push return address on stack
la	$v0, GSMSourceSetGsCrt
sw	$a0, Source_INT($v0)
sw	$a1, Source_MODE($v0)
sw	$a2, Source_FFMD($v0)

and	$a3, $a0,$a2				# a3 = Interlace & Field
andi	$a3, $a3,1				# a3 &= 1 limited to 1 bit
la	$v0, GSMAdapts
sb	$a3, Interlace_FRAME_Mode_Flag($v0)

//-----------------------------------------------------------------------------------------

// RC (Read Circuit) adaptation for avoiding garbage on screen ex.: Sega Genesis Collection (SLUS_215.42)
la	$v0, GSMAdapts
li	$a0, 1
sb	$a0, RC_adaptation($v0)

//-----------------------------------------------------------------------------------------

// Remove all breakpoints (even when they aren't enabled)
di			// Disable Interupts
li $a2, 0x8000
mtbpc $a2		// All breakpoints off (BED = 1)
sync.p			// Await instruction completion

//-----------------------------------------------------------------------------------------

la	$v0, GSMDestSetGsCrt
lw	$a0, Target_INT($v0)
lw	$a1, Target_MODE($v0)
lw	$a2, Target_FFMD($v0)
sync.l						# The opcodes sync.l and sync.p made a lot of games compatible
sync.p						# They ensure all values are stable before the following call

//-----------------------------------------------------------------------------------------

//
//This function will get called first, in place of the original SetGsCrt() syscall handler. It will check the video mode argument.
//If it's one of the extended video modes, do not call the original SetGsCrt() syscall handler, 
//but handle GS configuration on our own.
//Otherwise, call the original SetGsCrt() syscall handler and let it configure the GS for us.
//

li	$a3, GS_MODE_DTV_480P
bne	$a1, $a3, Skip_DTV_480P
nop

//-----------------------------------------------------------------------------------------

DTV_480P:

//$v0 = Base address
la	$v0, GSMDestSetGsCrt
//$a0 = Target_SMODE2
ld	$a0, Target_SMODE2($v0)

//$v0 = GS base address
lui		$v0, (GS_BASE >> 16)

//*GS_SMODE1=0x0000001740430504;
lui     $v1, 0x0017
ori     $v1, 0x4043
dsll    $v1, 16
ori     $v1, 0x0504
sd		$v1, GS_SMODE1($v0)	# store it

//*GS_SYNCH1=0x000402DA20034020;
lui     $v1, 0x0004
ori     $v1, 0x02DA
dsll    $v1, 16
ori     $v1, 0x2003
dsll    $v1, 16
ori     $v1, 0x4020
sd		$v1, GS_SYNCH1($v0)	# store it

//*GS_SYNCH2=0x0019D272;
lui     $v1, 0x0019
ori     $v1, 0xD272
sd		$v1, GS_SYNCH2($v0)	# store it

//*GS_SYNCV=0x00C78C0001E00006;
lui     $v1, 0x00C7
ori     $v1, 0x8C00
dsll    $v1, 16
ori     $v1, 0x01E0
dsll    $v1, 16
ori     $v1, 0x0006
sd		$v1, GS_SYNCV($v0)	# store it

//*GS_SMODE2=Target_SMODE2;
or      $v1, $zero, $a0				# 	v1 = $a0 = Target_SMODE2;
sd		$v1, GS_SMODE2($v0)	# store it

//*GS_SRFSH=0;
li      $v1, 4
sd		$v1, %lo(GS_SRFSH)($v0)	# store it

//*GS_SMODE1=0x0000001742410504;
lui     $v1, 0x0017
ori     $v1, 0x4241
dsll    $v1, 16
ori     $v1, 0x0504
sd		$v1, GS_SMODE1($v0)	# store it

//delay some miliseconds...
li $v1, 0x7A120
nop
DTV_480P_SMODE1_delay_loop:
vnop
vnop
addiu $v1, 0xFFFF
vnop
vnop
bnez $v1, DTV_480P_SMODE1_delay_loop
nop

//*GS_SMODE1=0x0000001742400504;	// Have bits 16 and 17 cleared.
lui     $v1, 0x0017
ori     $v1, 0x4240
dsll    $v1, 16
ori     $v1, 0x0504
sd		$v1, GS_SMODE1($v0)	# store it

beqzl $zero, Skip_Call_Original_SetGsCrt
nop

//-----------------------------------------------------------------------------------------

Skip_DTV_480P:
li	$a3, GS_MODE_DTV_576P
bne	$a1, $a3, Skip_DTV_576P
nop

//-----------------------------------------------------------------------------------------


DTV_576P:

//$v0 = Base address
la	$v0, GSMDestSetGsCrt
//$a0 = Target_SMODE2
ld	$a0, Target_SMODE2($v0)

//$v0 = GS base address
lui		$v0, (GS_BASE >> 16)

//*GS_SMODE1=0x00000017424B0504;
lui     $v1, 0x0017
ori     $v1, 0x424B
dsll    $v1, 16
ori     $v1, 0x0504
sd		$v1, GS_SMODE1($v0)	# store it

//*GS_SYNCH1=0x000402E02003C827;
lui     $v1, 0x0004
ori     $v1, 0x02E0
dsll    $v1, 16
ori     $v1, 0x2003
dsll    $v1, 16
ori     $v1, 0xC827
sd		$v1, GS_SYNCH1($v0)	# store it

//*GS_SYNCH2=0x0019CA67;
lui     $v1, 0x0019
ori     $v1, 0xCA67
sd		$v1, GS_SYNCH2($v0)	# store it

//*GS_SYNCV=0x00A9000002700005;
lui     $v1, 0x00A9
ori     $v1, 0x0000
dsll    $v1, 16
ori     $v1, 0x0270
dsll    $v1, 16
ori     $v1, 0x0005
sd		$v1, GS_SYNCV($v0)	# store it

//*GS_SMODE2=Target_SMODE2;
or      $v1, $zero, $a0				# 	v1 = $a0 = Target_SMODE2;
sd		$v1, GS_SMODE2($v0)	# store it

//*GS_SRFSH=0;
li      $v1, 4
sd		$v1, %lo(GS_SRFSH)($v0)	# store it

//*GS_SMODE1=0x0000001742490504;
lui     $v1, 0x0017
ori     $v1, 0x4249
dsll    $v1, 16
ori     $v1, 0x0504
sd		$v1, GS_SMODE1($v0)	# store it

//delay some miliseconds...
li $v1, 0x7A120
nop
DTV_576P_SMODE1_delay_loop:
vnop
vnop
addiu $v1, 0xFFFF
vnop
vnop
bnez $v1, DTV_576P_SMODE1_delay_loop
nop

//*GS_SMODE1=0x0000001742480504;	// Have bits 16 and 17 cleared.
lui     $v1, 0x0017
ori     $v1, 0x4248
dsll    $v1, 16
ori     $v1, 0x0504
sd		$v1, GS_SMODE1($v0)	# store it

beqzl $zero, Skip_Call_Original_SetGsCrt
nop

//-----------------------------------------------------------------------------------------

Skip_DTV_576P:
li	$a3, GS_MODE_DTV_1080P
bne	$a1, $a3, Skip_DTV_1080P
nop

//-----------------------------------------------------------------------------------------

DTV_1080P:

//$v0 = Base address
la	$v0, GSMDestSetGsCrt
//$a0 = Target_SMODE2
ld	$a0, Target_SMODE2($v0)

//$v0 = GS base address
lui		$v0, (GS_BASE >> 16)

//*GS_SMODE1=0x00000003422304B1
lui     $v1, 0x0003
ori     $v1, 0x4223
dsll    $v1, 16
ori     $v1, 0x04B1
or      $v1, $v0, $v1
sd		$v1, GS_SMODE1($v0)	# store it

//if((*GS_REG_CSR>>16&0xFF)>=0x19){
ld      $v1, GS_CSR($v0)	# load it
dsrl    $v1, 16
andi    $v1, 0xFF
sltiu   $v1, 0x19
bnez    $v1, DTV_1080P_else
nop

//		*GS_REG_SYNCH1=0x 0003 4420 0B04 182D;
lui     $v1, 0x0003
ori     $v1, 0x4420
dsll    $v1, 16
ori     $v1, 0x0B04
dsll    $v1, 16
ori     $v1, 0x182D
sd		$v1, GS_SYNCH1($v0)	# store it

//		*GS_REG_SYNCH2=0x 0020 FB61;
lui     $v1, 0x0020
ori     $v1, 0xFB61
sd		$v1, GS_SYNCH2($v0)	# store it

beqzl	$zero, DTV_1080P_endif
nop

//	}
//	else{
DTV_1080P_else:
//		*GS_REG_SYNCH1=0x 0003 4420 0B04 3829;
lui     $v1, 0x0003
ori     $v1, 0x4420
dsll    $v1, 16
ori     $v1, 0x0B04
dsll    $v1, 16
ori     $v1, 0x3829
sd		$v1, GS_SYNCH1($v0)	# store it

//		*GS_REG_SYNCH2=0x 0020 EB63;
lui     $v1, 0x0020
ori     $v1, 0xEB63
sd		$v1, GS_SYNCH2($v0)	# store it

//	}
DTV_1080P_endif:
//*GS_SYNCV=0x0150 E002 01C0 0005;
lui     $v1, 0x0150
ori     $v1, 0xE002
dsll    $v1, 16
ori     $v1, 0x01C0
dsll    $v1, 16
ori     $v1, 0x0005
sd		$v1, GS_SYNCV($v0)	# store it

//*GS_SMODE2=Target_SMODE2;
or      $v1, $zero, $a0				# 	v1 = $a0 = Target_SMODE2;
sd		$v1, GS_SMODE2($v0)	# store it

//*GS_SRFSH=0;
li      $v1, 4
sd		$v1, %lo(GS_SRFSH)($v0)	# store it

//*GS_SMODE1=0x00000003422204B1;
lui     $v1, 0x0003
ori     $v1, 0x4222
dsll    $v1, 16
ori     $v1, 0x04B1
sd		$v1, GS_SMODE1($v0)	# store it

//delay some miliseconds...
li $v1, 0x7A120
nop
DTV_1080P_SMODE1_delay_loop:
vnop
vnop
addiu $v1, 0xFFFF
vnop
vnop
bnez $v1, DTV_1080P_SMODE1_delay_loop
nop

//*GS_SMODE1=0x00000003422004B1;	// Have bits 16 and 17 cleared.
lui     $v1, 0x0003
ori     $v1, 0x4220
dsll    $v1, 16
ori     $v1, 0x04B1
sd		$v1, GS_SMODE1($v0)	# store it

beqzl	$zero, Skip_Call_Original_SetGsCrt
nop

//-----------------------------------------------------------------------------------------
Skip_DTV_1080P:
lw	$a3, Old_SetGsCrt		# a3 -> original SetGsCrt function
jalr	$a3					# Call original SetGsCrt
nop

//-----------------------------------------------------------------------------------------
Skip_Call_Original_SetGsCrt:

//Re-enable GSHandler whenever Hook_SetGsCrt is called
//This aggressive approach is needed for those titles which disable breakpoints 
//For instance: UP (SLUS 21864)
//-----------------------------------------------------------------------------------------

// Set Data Address Write Breakpoint
// Trap writes to GS registers, so as to control their values
	
li $a0, 0x12000000	// Address base for trapping
li $a1, 0x1FFFEF0F	// Address mask for trapping	//DOCTORXYZ
					// Trapping range is extended to match all kernel access segments

di			// Disable Interupts
sync.l			// Wait until the preceding loads are completed

li $a2, 0x8000
mtbpc $a2		// All breakpoints off (BED = 1)

sync.p			// Await instruction completion

mtdab	$a0
mtdabm	$a1

sync.p			// Await instruction completion

mfbpc $a3
sync.p			// Await instruction completion

li $a2, 0x20200000	// Data write breakpoint on (DWE, DUE = 1)
or $a3, $a3, $a2
xori $a3, $a3, 0x8000	// DEBUG exception trigger on (BED = 0)
mtbpc $a3

sync.p			//  Await instruction completion

ei			// Enable Interupts
nop

//-----------------------------------------------------------------------------------------

ld	$ra, 0($sp)				# Pull return address from stack 
jr	$ra					# Return to caller
addiu	$sp, $sp, 0x0010			# Restore sp during return

.end	Hook_SetGsCrt

#
# GSHandler:
# When the processor takes a level 2 exception, the processor switches to
# the kernel mode, by setting Status.ERL to 1.
#
.globl GSHandler
.ent	GSHandler
GSHandler:
sync.l
sync.p
sq	$k0, -0x10($zero)	# Store registers reserved for kernel
sq	$k1, -0x20($zero)	# usage in interrupt/trap handling 
				# Save all MIPS registers except zero($0) k0($26) and k1($27))
				# RA NB: NO!!! ALL registers are needed in this array, for evaluations
				# RA NB: Even the $zero register is needed, as it may be used in conditionals

la	$k0, MIPS_Regs		# Store MIPS_Regs via k0
sq	$zero, 0($k0)		# $zero
sq	$at, 0x10($k0)		# at
sq	$v0, 0x20($k0)		# v0
sq	$v1, 0x30($k0)		# v1
sq	$a0, 0x40($k0)		# a0
sq	$a1, 0x50($k0)		# a1
sq	$a2, 0x60($k0)		# a2
sq	$a3, 0x70($k0)		# a3
sq	$t0, 0x80($k0)
sq	$t1, 0x90($k0)
sq	$t2, 0xA0($k0)
sq	$t3, 0xB0($k0)
sq	$t4, 0xC0($k0)
sq	$t5, 0xD0($k0)
sq	$t6, 0xE0($k0)
sq	$t7, 0xF0($k0)
sq	$s0, 0x100($k0)
sq	$s1, 0x110($k0)
sq	$s2, 0x120($k0)
sq	$s3, 0x130($k0)
sq	$s4, 0x140($k0)
sq	$s5, 0x150($k0)
sq	$s6, 0x160($k0)
sq	$s7, 0x170($k0)
sq	$t8, 0x180($k0)
sq	$t9, 0x190($k0)
				# 0x1A0 must be set later, to initial $k0 value
				# 0x1B0 must be set later, to initial $k1 value
sq	$gp, 0x1C0($k0)
sq	$sp, 0x1D0($k0)
sq	$fp, 0x1E0($k0)
sq	$ra, 0x1F0($k0)

lq	$t0, -0x10($zero)	# t0 = entry k0
lq	$t1, -0x20($zero)	# t1 = entry k1
sq	$t0, 0x1A0($k0)		# store entry k0 in register array
sq	$t1, 0x1B0($k0)		# store entry k1 in register array
	
#
# The read/write ErrorEPC register holds the virtual address at which instruction
# processing can resume after servicing an error. This address can be:
# - The virtual address of the instruction that caused the exception
# - The virtual address of the immediately preceding branch or jump instruction
# (when the instruction is in a branch delay slot, and the BD2 bit in the Cause
# register is set).
#
mfc0	$k1, $13		# k1 = Cause bits of last exception (COP0 reg 13)
srl	$k1, 30			# k1 is aligned for BD2 (Flags branch delay slot used)
				# 1 -> delay slot, 0 -> normal
andi	$k1, 1			# k1 = BD2
sll	$k1, 2			# k1 = BD2*4
mfc0	$k0, $30		# k0 = ErrorPC (COP0 reg 30) -> MIPS instruction
addu	$k0, $k1		# Add 4 to opcode address for Branch Delay Slot
				# Next get rt (target register) and write address
				# but first check that the instruction is one we patch
lw	$v0, 0($k0)		# v0 = MIPS instruction that caused trap

srl	$v1,$v0,26		# v1 = opcode (range 0x00-0x3F)
andi	$v1,$v1, 0x003F		# v1 = pure opcode number
sll	$v1,$v1,2		# v1 = op_num*2 (word offset for jump table)

la	$s0, GSMSourceGSRegs
la	$s1, GSMDestGSRegs
la	$s2, GSMFlags
la	$s3, GSMAdapts
li	$s4,0xB2000000			# s4 = GS base address in KSEG1

la	$a2, MIPS_Regs		# a2 -> MIPS_Regs
la	$a3, op_t		# a3 -> op_t

addu	$a0,$v1,$a3		# a0 -> active entry in op_t
lw	$a1,0($a0)		# a1 = opcode handler from op_t

jr	$a1			# jump to separate opcode handlers
nop				# with v0=instruction, a2->KSeg, a3->op_t
	
#  For the present we ignore read operations (should never happen. Not trapped)
ldl_op:
ldr_op:
lq_op:
lb_op:
lh_op:
lwl_op:
lw_op:
lbu_op:
lhu_op:
lwr_op:
lwu_op:
lq_op:
ld_op:
ignore:				# We just ignore weird opcodes that we don't implement
beqzl	$zero,exit_GSHandler
nop

#  For the present we treat all write operations as 'sd'
sq_op:
sb_op:
sh_op:
swl_op:
sw_op:
sdl_op:
sdr_op:
swr_op:
sd_op:
have_some_write:		# Opcode is a write, so we must check further
srl	$a1, $v0, 16
andi	$a1, $a1, 0x1f		# a1 = unscaled rt reg index
srl	$a0, $v0, 21
andi	$a0, $a0, 0x1f		# a0 = unscaled base reg index

sll	$k0, $a0, 4		# k0 = raw base_ix << 4 (scaled base_ix reg index)

addu	$v1, $a2, $k0		# v1 = &MIPS_Regs[base_ix]; (if type = u128)
lw	$a3, 0($v1)		# a3 = base register value
andi	$k1, $v0, 0xFFFF	# k1 = offset field of instruction
addu	$a3, $a3, $k1		# a3 = address which triggered breakpoint

sll	$k0, $a1, 4		# k0 = raw rt_ix << 4 (scaled rt_ix reg index)
addu	$v0, $a2, $k0		# v0 = &MIPS_Regs[rt_ix];
ld	$a1, 0($v0)		# a1 = value in rt

#  NB: The trapping method forces us to trap some GS registers we don't want.
#      It is crucial that the writing of those registers proceeds undisturbed.
#      This is handled by the final test case below, at label "not_wanted_reg".

#  Here a1=source_data, a2->.gsm_engine, a3=dest_address
#  NB: Since address is changed to offset by ANDI, it is valid for all segments
#  NB: We avoid masking a3 itself though, in case this is an unwanted register
#  NB: Remasking for KSEG1 should be done in each handler for wanted registers

andi	$v0,$a3,0xFFFF				# v0 = dest offset from GS_BASE

addi	$v1,$v0, -GS_PMODE
beqzl	$v1,have_PMODE_write			# in case of  dest == GS_reg_PMODE
nop
addi	$v1,$v0, -GS_SMODE1
beqzl	$v1,have_SMODE1_write			# in case of  dest == GS_reg_SMODE1
nop
addi	$v1,$v0, -GS_SMODE2
beqzl	$v1,have_SMODE2_write			# in case of  dest == GS_reg_SMODE2
nop
addi	$v1,$v0, -GS_SRFSH
beqzl	$v1,have_SRFSH_write			# in case of  dest == GS_reg_SRFSH
nop
addi	$v1,$v0, -GS_SYNCH1
beqzl	$v1,have_SYNCH1_write			# in case of  dest == GS_reg_SYNCH1
nop
addi	$v1,$v0, -GS_SYNCH2
beqzl	$v1,have_SYNCH2_write			# in case of  dest == GS_reg_SYNCH2
nop
addi	$v1,$v0, -GS_SYNCV
beqzl	$v1,have_SYNCV_write			# in case of  dest == GS_reg_SYNCV
nop
addi	$v1,$v0, -GS_DISPFB1
beqzl	$v1,have_DISPFB1_write			# in case of  dest == GS_reg_DISPFB1
nop
addi	$v1,$v0, -GS_DISPFB2
beqzl	$v1,have_DISPFB2_write			# in case of  dest == GS_reg_DISPFB2
nop
addi	$v1,$v0, -GS_DISPLAY1
beqzl	$v1,have_DISPLAY1_write			# in case of  dest == GS_reg_DISPLAY1
nop
addi	$v1,$v0, -GS_DISPLAY2
beqzl	$v1,have_DISPLAY2_write			# in case of  dest == GS_reg_DISPLAY2
nop

not_wanted_reg:					# Register unwanted, so perform op unchanged
sd	$a1,0($a3)				# Store source data unchanged to destination
beqzl	$zero,exit_GSHandler
nop

# ----------------------------
# PMODE
# .-------.------.------------.----------------------------------------------.------.
# | Name  | Pos. | Format     | Contents                                     | Mask |
# |-------+------+------------+----------------------------------------------+------+
# | EN1   | 0    | int 0:1:0  | Read Circuit 1 ON/OFF                        | 0x1  |
# |       |      |            | 0 OFF                                        |      |
# |       |      |            | 1 ON                                         |      |
# | EN2   | 1    | int 0:1:0  | Read Circuit 2 ON/OFF                        | 0x1  |
# |       |      |            | 0 OFF                                        |      |
# |       |      |            | 1 ON                                         |      |
# | CRTMD | 4:2  | int 0:3:0  | CRT Output Switching                         | 0x3  |
# |       |      |            | Always 001                                   |      |
# | MMOD  | 5    | int 0:1:0  | Alpha Value Selection for Alpha Blending     | 0x1  |
# |       |      |            | 0 Alpha Value of Read Circuit 1              |      |
# |       |      |            | 1 ALP Register Value                         |      |
# | AMOD  | 6    | int 0:1:0  | OUT1 Alpha Output Selection                  | 0x1  |
# |       |      |            | 0 Alpha Value of Read Circuit 1              |      |
# |       |      |            | 1 Alpha Value of Read Circuit 2              |      |
# | SLBG  | 7    | int 0:1:0  | Alpha Blending Method Selection              | 0x1  |
# |       |      |            | 0 Blended with the output of Read Circuit 2. |      |
# |       |      |            | 1 Blended with the background color          |      |
# | ALP   | 15:8 | int 0:8:0  | Fixed Alpha Value (0xff = 1.0)               | 0xFF |
# '-------^------^-------------^---------------------------------------------^------^
have_PMODE_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_PMODE($s0)			# Source_PMODE = a1
lb	$v0, PMODE_fix($s2)				# v0 = PMODE_fix
beql	$v0,$zero,store_v0_as_PMODE	# in case of PMODE_fix is disabled
or	$v0,$zero,$a1						# 	go use v0=a1 for PMODE
ld	$v0, Target_PMODE($s1)			# v0 = Target_PMODE
beql	$v0,$zero,exit_GSHandler		# in case of  Target_PMODE is zero
sd	$a1,0($a3)							# 	go use Source_PMODE

store_v0_as_PMODE:
sync.l									# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p									# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# SMODE1
# .--------.-------.---------------.----------------------------------.-------.
# | Name   | Pos.  | Format        | Contents                         | Mask  |
# |--------+-------+---------------+----------------------------------+-------|
# | VHP    | 36:36 | int  0:1:0    | Mode Choice                      | 0x1   |
# |        |       |               | 0 Progressive                    |       |
# |        |       |               | 1 Interlace                      |       |
# | VCKSEL | 35:34 | int  0:2:0    |                                  | 0x3   |
# | SLCK2  | 33:33 | int  0:1:0    |                                  | 0x1   |
# | NVCK   | 32:32 | int  0:1:0    |                                  | 0x1   |
# | CLKSEL | 31:30 | int  0:2:0    |                                  | 0x3   |
# | PEVS   | 29:29 | int  0:1:0    |                                  | 0x1   |
# | PEHS   | 28:28 | int  0:1:0    |                                  | 0x1   |
# | PVS	   | 27:27 | int  0:1:0    |                                  | 0x1   |
# | PHS	   | 26:26 | int  0:1:0    |                                  | 0x1   |
# | GCONT  | 25:25 | int  0:1:0    | Component Color Mode             | 0x1   |
# |        |       |               | 0 RGB                            |       |
# |        |       |               | 1 YPbPr                          |       |
# | SPML   | 24:21 | int  0:4:0    | Sub-Pixel Magnification Level    | 0xF   |
# | PCK2   | 20:19 | int  0:2:0    |                                  | 0x1   |
# | XPCK   | 18:18 | int  0:1:0    |                                  | 0x1   |
# | SINT   | 17:17 | int  0:1:0    | PLL (Phase-locked loop)          | 0x1   |
# |        |       |               | 0 Off                            |       |
# |        |       |               | 1 On                             |       |
# | PRST   | 16:16 | int  0:1:0    | PCRTC Reset                      | 0x1   |
# |        |       |               | 0 Off                            |       |
# |        |       |               | 1 On                             |       |
# | EX     | 15:15 | int  0:1:0    |                                  | 0x1   |
# | CMOD   | 14:13 | int  0:2:0    | Color Subcarrier                 | 0x3   |
# |        |       |               | 2 NTSC                           |       |
# |        |       |               | 3 PAL                            |       |
# |        |       |               | 0 Used by other modes            |       |
# |        |       |               | 1 Untested                       |       |
# | SLCK   | 12:12 | int  0:1:0    |                                  | 0x1   |
# | T1248  | 11:10 | int  0:2:0    |                                  | 0x3   |
# | LC     |  9:3  | int  0:7:0    | PLL Loop Divider                 | 0x1F  |
# | RC     |  2:0  | int  0:3:0    | PLL Reference Divider            | 0x7   |
# '--------^-------^---------------^----------------------------------^-------'
# Reference Table
#                                 VHP VCKSEL SLCK2 NVCK CLKSEL PEVS PEHS PVS PHS GCONT SPML PCK2 XPCK SINT PRST EX CMOD SLCK T1248 LC RC
# NTSC-NI   (640x240(224))        0   1      1     1    1      0    0    0   0   0     4    0    0    1    1    0  2    0    1     32 4
# NTSC-I    (640x480(448))        0   1      1     1    1      0    0    0   0   0     4    0    0    1    1    0  2    0    1     32 4
# PAL-NI    (640x288(256))        0   1      1     1    1      0    0    0   0   0     4    0    0    1    1    0  3    0    1     32 4
# PAL-I     (640x576(512))        0   1      1     1    1      0    0    0   0   0     4    0    0    1    1    0  3    0    1     32 4
# VESA-1A   (640x480 59.940Hz)    1   0      1     1    1      0    0    0   0   0     2    0    0    1    0    0  0    0    1     15 2
# VESA-1C   (640x480 75.000Hz)    1   0      1     1    1      0    0    0   0   0     2    0    0    1    0    0  0    0    1     28 3
# VESA-2B   (800x600 60.317Hz)    1   0      1     1    1      0    0    0   0   0     2    0    0    1    0    0  0    0    1     71 6
# VESA-2D   (800x600 75.000Hz)    1   0      1     1    1      0    0    0   0   0     2    0    0    1    0    0  0    0    1     44 3
# VESA-3B   (1024x768 60.004Hz)   1   0      1     1    1      0    0    0   0   0     2    0    0    1    0    0  0    0    0     58 6
# VESA-3D   (1024x768 75.029Hz)   1   0      1     1    1      0    0    0   0   0     1    0    0    1    0    0  0    0    1     35 3
# VESA-4A   (1280x1024 60.020Hz)  1   0      1     1    1      0    0    0   0   0     1    0    0    1    0    0  0    0    0      8 1
# VESA-4B   (1280x1024 75.025Hz)  1   0      1     1    1      0    0    0   0   0     1    0    0    1    0    0  0    0    0     10 1
# DTV-480P  (720x480)             1   1      1     1    1      0    0    0   0   0     2    0    0    1    1    0  0    0    1     32 4
# DTV-576P  (720x576)             1   1      1     1    1      0    0    0   0   0     2    1    0    1    1    0  0    0    1     32 4
# DTV-1080I (1920x1080)           0   0      1     1    1      0    0    0   0   0     1    0    0    1    0    0  0    0    1     22 2
# DTV-1080P (1920x1080)           0   0      1     1    1      0    0    0   0   0     1    0    0    1    0    0  0    0    1     22 1
# DTV-720P  (1280x720)            1   0      1     1    1      0    0    0   0   0     1    0    0    1    0    0  0    0    1     22 2
#
# low-level PCRTC initialize
# A critical part of getting this to work is bits 16 (PRST) and 17(SINT).
# Every time you write to SMODE1 register without resetting these fields causes EE to stop processing. A hard reset was required at that point.
# GS rev.21 which responds to the above code.
# An older GS rev. seems to require setting bit 16 and then reseting bit 16 and then waiting for 2.5ms.
#
have_SMODE1_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_SMODE1($s0)				# Source_SMODE1 = a1
lb	$v0, SMODE1_fix($s2)				# v0 = SMODE1_fix
beql	$v0,$zero,store_v0_as_SMODE1	# in case of SMODE1_fix is disabled
or	$v0,$zero,$a1						# 	go use v0=a1 for SMODE1
ld	$v0, Target_SMODE1($s1)				# v0 = Target_SMODE1
beql	$v0,$zero,exit_GSHandler		# in case of  Target_SMODE1 is zero
sd	$a1,0($a3)							# 	go use Source_SMODE1

store_v0_as_SMODE1:
sync.l									# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p									# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# SMODE2
# .----.---.---------.-----------------------------------.
# |Name|Pos|Format   |Contents                           |
# +----+---+---------+-----------------------------------|
# |INT | 0 |int 0:1:0|Interlace Mode Setting             |
# |    |   |         |0 Non-Interlace Mode               |
# |    |   |         |1 Interlace Mode                   |
# |FFMD| 1 |int 0:1:0|Setting in Interlace Mode          |
# |    |   |         |0 FIELD Mode(Read every other line)|
# |    |   |         |1 FRAME Mode(Read every line)      |
# |DPMS|3:2|int 0:2:0|VESA DPMS Mode Setting             |
# |    |   |         |00 On          10 Suspend          |
# |    |   |         |01 Stand-by    11 Off              |
# ^----^---^---------^-----------------------------------.
have_SMODE2_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_SMODE2($s0)				# Source_SMODE2 = a1
lb	$v0, SMODE2_fix($s2)				# v0 = SMODE2_fix
beql	$v0,$zero,store_v0_as_SMODE2	# in case of  Separate SMODE2 fix disabled
or	$v0,$zero,$a1						# go use v0=a1 for SMODE2
srl	$v0,$a1,1							# v0 = a1 aligned for FFMD in bit0
and	$v0,$v0,$a1							# v0 bit 0 = INT & FFMD
andi	$v0,$v0,1						# v0 bit 0 = INT & FFMD isolated
sb	$v0, Interlace_FRAME_Mode_Flag($s3)	# store Interlace FRAME Mode Flag
beqz	$v0,store_v0_as_SMODE2			# in case of  no Double Height need
ld	$v0, Target_SMODE2($s1)				# go use Target_SMODE2 as adapted SMode2
										# otherwise just set v0 = Target_SMODE2
andi	$a1,$a1,2						# a1 = FFMD of Source_SMODE2
andi	$v0,$v0,0xFFFD					# v0 = Target_SMODE2 without FFMD
or	$v0,$v0,$a1							# v0 = Target_SMODE2 + Source FFMD
store_v0_as_SMODE2:
sync.l									# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p									# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# SRFSH
# .-----.---.---------.-----------------------------------------------------.
# |Name |Pos|Format   |Contents                                             |
# +-----+---+---------+-----------------------------------------------------|
# |SRFSH| 0 |int 0:?:0| DRAM Refresh Settings. Somewhat correlated to SPML. |
# ^-----^---^---------^-----------------------------------------------------.
# Reference Table
#                                 SRFSH SPML
#  NTSC-NI   (640x240(224))         8    4  
#  NTSC-I    (640x480(448))         8    4  
#  PAL-NI    (640x288(256))         8    4  
#  PAL-I     (640x576(512))         8    4  
#  VESA-1A   (640x480 59.940Hz)     4    2  
#  VESA-1C   (640x480 75.000Hz)     4    2  
#  VESA-2B   (800x600 60.317Hz)     4    2  
#  VESA-2D   (800x600 75.000Hz)     4    2  
#  VESA-3B   (1024x768 60.004Hz)    4    2  
#  VESA-3D   (1024x768 75.029Hz)    2    1  
#  VESA-4A   (1280x1024 60.020Hz)   2    1  
#  VESA-4B   (1280x1024 75.025Hz)   2    1  
#  DTV-480P  (720x480)              4    2  
#  DTV-1080I (1920x1080)            4    1  
#  DTV-720P  (1280x720)             4    1  
#
have_SRFSH_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_SRFSH($s0)			# Source_SRFSH = a1
lb	$v0, SRFSH_fix($s2)				# v0 = SRFSH_fix
beql	$v0,$zero,store_v0_as_SRFSH	# in case of SRFSH_fix is disabled
or	$v0,$zero,$a1						# 	go use v0=a1 for SRFSH
ld	$v0, Target_SRFSH($s1)			# v0 = Target_SRFSH
beql	$v0,$zero,exit_GSHandler		# in case of  Target_SRFSH is zero
sd	$a1,0($a3)							# 	go use Source_SRFSH

store_v0_as_SRFSH:
sync.l									# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p									# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# SYNCH1
# ----.-----.----------.-------.
#|Name|Pos. |Format    | Mask  |
# ----+-----+----------+-------|
#|HFP |10:0 |int 0:11:0| 0x7FF |
#|HBP |21:11|int 0:11:0| 0x7FF |
#|HSEQ|31:22|int 0:11:0| 0x7FF |
#|HSVS|42:32|int 0:11:0| 0x7FF |
#|HS  |53:43|int 0:11:0| 0x7FF |
# ----^-----^----------^-------'
have_SYNCH1_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_SYNCH1($s0)			# Source_SYNCH1 = a1
lb	$v0, SYNCH_fix($s2)				# v0 = SYNCH_fix
beql	$v0,$zero,store_v0_as_SYNCH1	# in case of SYNCH_fix is disabled
or	$v0,$zero,$a1						# 	go use v0=a1 for SYNCH1
ld	$v0, Target_SYNCH1($s1)			# v0 = Target_SYNCH1
beql	$v0,$zero,exit_GSHandler		# in case of  Target_SYNCH1 is zero
sd	$a1,0($a3)							# 	go use Source_SYNCH1

store_v0_as_SYNCH1:
sync.l									# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p									# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# SYNCH2
# ----.-----.----------.-------.
#|Name|Pos. |Format    | Mask  |
# ----+-----+----------+-------|
#|HF  |10:0 |int 0:11:0| 0x7FF |
#|HB  |21:11|int 0:11:0| 0x7FF |
#'----^-----^----------^-------'
have_SYNCH2_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_SYNCH2($s0)			# Source_SYNCH2 = a1
lb	$v0, SYNCH_fix($s2)				# v0 = SYNCH_fix
beql	$v0,$zero,store_v0_as_SYNCH2	# in case of SYNCH_fix is disabled
or	$v0,$zero,$a1						# 	go use v0=a1 for SYNCH2
ld	$v0, Target_SYNCH2($s1)			# v0 = Target_SYNCH2
beql	$v0,$zero,exit_GSHandler		# in case of  Target_SYNCH2 is zero
sd	$a1,0($a3)							# 	go use Source_SYNCH2

store_v0_as_SYNCH2:
sync.l									# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p									# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# SYNCV
# .----.-----.----------.-------.
# |Name|Pos. |Format    | Mask  |
# |----+-----+----------+-------|
# |VFP | 9:0 |int 0:10:0| 0x3FF |
# |VFPE|19:10|int 0:10:0| 0x3FF |
# |VBP |31:20|int 0:12:0| 0xFFF |
# |VBPE|41:32|int 0:12:0| 0xFFF |
# |VDP |52:42|int 0:11:0| 0x7FF |
# |VS  |56:53|int 0:04:0| 0xF   |
# '----^-----^----------^-------'
have_SYNCV_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_SYNCV($s0)				# Source_SYNCV = a1
lb	$v0, SYNCV_fix($s2)					# v0 = SYNCV_fix
beql	$v0,$zero,store_v0_as_SYNCV		# in case of SYNCV_fix is disabled
or	$v0,$zero,$a1						# 	go use v0=a1 for SYNCV
ld	$v0, Target_SYNCV($s1)				# v0 = Target_SYNCV
beql	$v0,$zero,exit_GSHandler		# in case of  Target_SYNCV is zero
sd	$a1,0($a3)							# 	go use Source_SYNCV

store_v0_as_SYNCV:
sync.l									# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p									# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# DISPFB1
# 
# .-------.-------.------------.----------------------------------------------.-------.
# | Name  |  Pos. | Format     | Contents                                     | Mask  |
# |-------+-------+------------+----------------------------------------------+-------+
# | FBP   | 8:0   | int 0:9:0  | Base Pointer (Address/2048)                  | 0x1FF |
# | FBW   | 14:9  | int 0:6:0  | Buffer Width (Width/64)                      | 0x3F  |
# | PSM   | 19:15 | int 0:5:0  | Pixel Storage Format                         | 0x1F  |
# |       |       |            | 00000 PSMCT32                                |       |
# |       |       |            | 00001 PSMCT24                                |       |
# |       |       |            | 00010 PSMCT16                                |       |
# |       |       |            | 01010 PSMCT16S                               |       |
# |       |       |            | 10010 PS-GPU24                               |       |
# | DBX   | 42:32 | int 0:11:0 | X Position in Buffer of Upper Left Point of  | 0x7FF |
# |       |       |            | Rectangular Area (in units of pixels)        |       |
# | DBY   | 53:43 | int 0:11:0 | Y Position in Buffer of Upper Left Point of  | 0x7FF |
# |       |       |            | Rectangular Area (in units of pixels)        |       |
# '-------^------^-------------^---------------------------------------------^--------^
have_DISPFB1_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_DISPFB1($s0)			# Source_DISPFB1 = a1
lb	$v0, DISPFB_fix($s2)				# v0 = DISPFB_fix
beql	$v0,$zero,store_v0_as_DISPFB1	# in case of DISPFB_fix is disabled
or	$v0,$zero,$a1						# 	go use v0=a1 for DISPFB1
ld	$v0, Target_DISPFB1($s1)			# v0 = Target_DISPFB1
beql	$v0,$zero,store_v0_as_DISPFB1	# in case of  Target_DISPFB1 is zero
or	$v0,$zero,$a1						# 	go use v0=a1 for DISPFB1

store_v0_as_DISPFB1:
sync.l				# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p				# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# DISPFB2
# 
# .-------.-------.------------.----------------------------------------------.-------.
# | Name  |  Pos. | Format     | Contents                                     | Mask  |
# |-------+-------+------------+----------------------------------------------+-------+
# | FBP   | 8:0   | int 0:9:0  | Base Pointer (Address/2048)                  | 0x1FF |
# | FBW   | 14:9  | int 0:6:0  | Buffer Width (Width/64)                      | 0x3F  |
# | PSM   | 19:15 | int 0:5:0  | Pixel Storage Format                         | 0x1F  |
# |       |       |            | 00000 PSMCT32                                |       |
# |       |       |            | 00001 PSMCT24                                |       |
# |       |       |            | 00010 PSMCT16                                |       |
# |       |       |            | 01010 PSMCT16S                               |       |
# |       |       |            | 10010 PS-GPU24                               |       |
# | DBX   | 42:32 | int 0:11:0 | X Position in Buffer of Upper Left Point of  | 0x7FF |
# |       |       |            | Rectangular Area (in units of pixels)        |       |
# | DBY   | 53:43 | int 0:11:0 | Y Position in Buffer of Upper Left Point of  | 0x7FF |
# |       |       |            | Rectangular Area (in units of pixels)        |       |
# '-------^------^-------------^---------------------------------------------^--------^
have_DISPFB2_write:
lui	$v0,0xB200							# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0							# a3 = GS register address in KSEG1
sd	$a1, Source_DISPFB2($s0)			# Source_DISPFB2 = a1
lb	$v0, DISPFB_fix($s2)				# v0 = DISPFB_fix
beql	$v0,$zero,store_v0_as_DISPFB2	# in case of DISPFB_fix is disabled
or	$v0,$zero,$a1						# 	go use v0=a1 for DISPFB2
ld	$v0, Target_DISPFB2($s1)			# v0 = Target_DISPFB2
beql	$v0,$zero,store_v0_as_DISPFB2	# in case of  Target_DISPFB2 is zero
or	$v0,$zero,$a1						# 	go use v0=a1 for DISPFB2

store_v0_as_DISPFB2:
sync.l				# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p				# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler			# Now go exit
sd	$v0,0($a3)							# after storing

# ----------------------------
# DISPLAY1
# 
have_DISPLAY1_write:					# Here a1=source_data, a2->.gsm_engine, a3=dest_adress
lui	$v0,0xB200					# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0					# a3 = GS register address in KSEG1
sd	$a1, Source_DISPLAY1($s0)			# request DISPLAY1 value = a1
ld	$v1, Target_DISPLAY1($s1)			# v1=forcing DISPLAY1 template

#  Source_DISPLAY1 == Requested   DX, DY, MAGH, MAGV, DW and DH values
#  Target_DISPLAY1 == Modded(forced) DX, DY, MAGH, MAGV, DW and DH values
#  Both are 64 bit units with encoded bit fields like GS DISPLAY1 registers

#  Patch to adapt request to enforced mode in v1 MUST preserve a1, a2, a3

lb	$v0, ADAPTATION_fix($s2)
beql	$v0,$zero,have_DISPLAY1_write_1					# in case of (ADAPTATION_fix)
or	$a1,$zero,$v1					# 	simulate request same as forced mode

have_DISPLAY1_write_1:
li	$v0,0						# preclear v0 as result DISPLAY1 accumulator

#  Here a0=free, a1=Source_DISPLAY1, a2->.gsm_engine, a3=dest_address
#  Also v0=result_accumulator, v1=Target_DISPLAY1, t0-t7=free

//
// Automatic adaptation formulas
//
# ----- HORIZONTAL FIELDS -----
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | MAGH | 26:23 | int 0: 4:0 | magnification in horizontal direction | 0xF   |
# '------^-------^------------^---------------------------------------^-------^
dsrl	$t0,$a1,23
andi	$t0,$t0,0x0F
addi	$t0,$t0,1		# t0 = Source_Width_Scale = Source_MAGH + 1
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DW   | 43:32 | int 0:12:0 | display area width - 1 (VCK units)    | 0xFFF |
# '------^-------^------------^---------------------------------------^-------^
dsrl32	$t1,$a1,0
andi	$t1,$t1,0x0FFF
addi	$t1,$t1,1		# t1 = Source_Width = Source_DW + 1
divu	$zero,$t1,$t0	# LO = Source_Pixels_Width = Source_Width / Source_Width_Scale
dsrl32	$t0,$v1,0
andi	$t5,$t0,0x0FFF
mflo	$t4				# t4 = LO = Source_Pixels_Width
addi	$t6,$t5,1		# t6 = Target_Width = Target_DW + 1
nop
divu	$zero,$t6,$t4	# LO = Target_Width_Scale = Target_Width / Source_Pixels_Width
nop
nop
mflo	$t0				# t0 = LO = Target_Width_Scale
bne	$t0,$zero,have_DISPLAY1_write_2		# in case of (!Target_Width_Scale)
nop						# {
or	$t7,$zero,$t5		# t7 = Target_DW = Target_Width - 1
sub	$t0,$t6,$t4			# t0 = Target_Width - Source_Pixels_Width
li	$t4,0				# t4 = Target_MAGH = 0
beql	$zero,$zero,have_DISPLAY1_write_4		# }
nop						# otherwise
						# Target_Width_Scale nonzero

have_DISPLAY1_write_2:                     # {
addi	$t1,$t0,-16		# t1 = Target_Width_Scale - 16
bgtzl	$t1,have_DISPLAY1_write_3			# in case of (Target_Width_Scale > 0)
or	$t0,$zero,16		# 	t0 = Target_Width_Scale = 16;

have_DISPLAY1_write_3:
mult	$t4,$t0			# LO = Calculated_Width = (Target_MAGH * Target_Width_Scale)
nop
nop
mflo	$t1				# t1 = LO = Calculated_Width
addi	$t7,$t1,-1		# t7 = Calculated_DW = Calculated_Width - 1
addi	$t4,$t0,-1		# t4 = Calculated_MAGH = Target_Width_Scale - 1
sub	$t0,$t5,$t7			# t0 = Target_DW - Calculated_DW

have_DISPLAY1_write_4:                     # }
dsra $t0,$t0,1			# t0 = t0 / 2 = Half_Excess_Width (can be negative)
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DX   | 11:0  | int 0:12:0 | x pos in display area (VCK units)     | 0xFFF |
# '------^-------^------------^---------------------------------------^-------^
andi	$t1,$v1,0x0FFF	# t1 = Target_DX
add	$t6,$t0,$t1			# t6 = Calculated_DX = Target_DX + Half_Excess_Width
bltzl	$t6,have_DISPLAY1_write_5			# in case of (Calculated_DX < 0)
and	$t6,$t6,$zero		# 	Calculated_DX = 0;

have_DISPLAY1_write_5:
sub	$t0,$t1,$t6			# t0 = Target_DX - Calculated_DX
bgtzl	$t0,have_DISPLAY1_write_6			# in case of (Target_DX > Calculated_DX)
add	$t7,$t7,$t0			# 	t7 = Calculated_DW = Calculated_DW + Target_DX - Calculated_DX  # Target DW adjusted

have_DISPLAY1_write_6:
andi	$t7,$t7,0x0FFF
andi	$t4,$t4,0x000F
andi	$t6,$t6,0x0FFF
dsll32	$t0,$t7,0		# t0 = Calculated_DW
or	$v0,$v0,$t0			# v0 = Adapted_DISPLAY1 = (Adapted_DISPLAY1) OR (Calculated_DW)
dsll	$t0,$t4,23
or	$v0,$v0,$t0			# v0 = Adapted_DISPLAY1 = (Adapted_DISPLAY1) OR (Calculated_MAGH)

lw	$t1, X_offset($s2)	# t1 = X_offset (signed)
add	$t6, $t6, $t1		# t6 = Calculated_DX = Calculated_DX + X_offset
bgez	$t6, have_DISPLAY1_write_7		# Is the result greater or equal to zero?
nop
move	$t6, $zero		# t6 = Calculated_DX = 0
have_DISPLAY1_write_7:
andi	$t6,$t6,0x0FFF

or	$v0,$v0,$t6			# v0 = Adapted_DISPLAY1 = (Adapted_DISPLAY1) OR (Calculated_DX)

# ----- VERTICAL FIELDS -------
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | MAGV | 28:27 | int 0: 2:0 | magnification in vertical direction   | 0x3   |
# '------^-------^------------^---------------------------------------^-------^
dsrl	$t0,$a1,27
andi	$t0,$t0,0x03
addi	$t0,$t0,1		# t0= Source_Height_Scale = Source_MAGV + 1
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DH   | 54:44 | int 0:11:0 | display area height - 1 (pixel units) | 0x7FF |
# '------^-------^------------^---------------------------------------^-------^
dsrl32	$t1,$a1,12
andi	$t1,$t1,0x07FF
addi	$t1,$t1,1		# Source_Height = Source_DH + 1
divu	$zero,$t1,$t0	# LO = Source_Pixels_Height = Source_Height / Source_Height_Scale
dsrl32	$t0,$v1,12
andi	$t5,$t0,0x07FF
mflo	$t4				# t4 = LO = Source_Pixels_Height
addi	$t6,$t5,1		# t6 = Target_Height = Target_DH + 1
nop
divu	$zero,$t6,$t4	# LO = Target_Height_Scale = Target_Height / Source_Pixels_Height
nop
nop
mflo	$t0				# t0 = LO = Target_Height_Scale
bne	$t0,$zero,have_DISPLAY1_write_8		# in case of (!Target_Height_Scale)
nop						# {
or	$t7,$zero,$t5		# t7 = Target_DH = Target_Height - 1
sub	$t0,$t6,$t4			# t0 = Target_Height - Source_Pixels_Height
li	$t4,0				# t4 = Target_MAGV = 0
beql	$zero,$zero,have_DISPLAY1_write_10		# }
nop						# otherwise
						# Target_Height_Scale nonzero

have_DISPLAY1_write_8:						# {
addi	$t1,$t0,-4		# t1 = Target_Height_Scale - 4
bgtzl	$t1,have_DISPLAY1_write_9			# in case of (Target_Height_Scale > 0)
or	$t0,$zero,4			# 	t0 = Target_Height_Scale = 4;

have_DISPLAY1_write_9:
mult	$t4,$t0			# LO = Calculated_Height = (Target_MAGV * Target_Height_Scale)
nop
nop
mflo	$t1				# t1 = LO = Calculated_Height
addi	$t7,$t1,-1		# t7 = Calculated_DH = Calculated_Height - 1
addi	$t4,$t0,-1		# t4 = Calculated_MAGV = Target_Height_Scale - 1
sub	$t0,$t5,$t7			# t0 = Target_DH - Calculated_DH

have_DISPLAY1_write_10:						# }
dsra $t0,$t0,1			# t0 = t0 / 2 = Half_Excess_Height (can be negative)
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DY   | 22:12 | int 0:11:0 | y pos in display area (raster units)  | 0x7FF |
# '------^-------^------------^---------------------------------------^-------^
dsrl	$t1,$v1,12
andi	$t1,$t1,0x07FF	# t1 = Target_DY
add	$t6,$t0,$t1			# t6 = Calculated_DY = Target_DY + Half_Excess_Height
bltzl	$t6,have_DISPLAY1_write_11			# in case of (Calculated_DY < 0)
and	$t6,$t6,$zero		#	Calculated_DY = 0;

have_DISPLAY1_write_11:
sub	$t0,$t1,$t6			# t0 = Target_DY - Calculated_DY
bgtzl	$t0,have_DISPLAY1_write_12			# in case of (Target_DY > Calculated_DY)
add	$t7,$t7,$t0			# 	t7 = Calculated_DH = Calculated_DH + Target_DY - Calculated_DY # Target DH adjusted

have_DISPLAY1_write_12:
lb	$t0, Interlace_FRAME_Mode_Flag($s3)	# in case of Double Height not needed
beql	$t0,$zero,have_DISPLAY1_write_13							# 	Calculation is complete
nop

ld	$t0, Target_SMODE2($s1)
andi	$t0,$t0,1							# in case of Target_SMODE2.INT = 1 (Interlace Mode)
bne	$t0,$zero,have_DISPLAY1_write_13							# 	Calculation is complete
nop
beql	$t4,$zero,have_DISPLAY1_write_13	# in case of Calculated_MAGV = 0
addi	$t4,$t4,1		# 	go use Calculated_MAGV = Calculated_MAGV + 1
addi	$t4,$t4,2		# Calculated_MAGV = Calculated_MAGV + 2 (Because scale was 2 or larger)
addi	$t0,$t4,-4		# Compare Calculated_MAGV with 4 (too large ?)
bgezl	$t0,have_DISPLAY1_write_13			# in case of  Calculated_MAGV too large
ori	$t4,$zero,3			# 	go use Calculated_MAGV = 3

have_DISPLAY1_write_13:
andi	$t7,$t7,0x07FF
andi	$t4,$t4,0x0003
andi	$t6,$t6,0x07FF
dsll32	$t0,$t7,12
or	$v0,$v0,$t0			# v0 = Adapted_DISPLAY1 = (Adapted_DISPLAY1) OR (Calculated_DH)
dsll	$t0,$t4,27
or	$v0,$v0,$t0			# v0 = Adapted_DISPLAY1 = (Adapted_DISPLAY1) OR (Calculated_MAGV)

lw	$t1, Y_offset($s2)		# t1 = Y_offset (signed)
add	$t6, $t6, $t1		# t6 = Calculated_DY = Calculated_DY + Y_offset
bgez	$t6, have_DISPLAY1_write_14		# Is the result greater or equal to zero?
nop
move	$t6, $zero
have_DISPLAY1_write_14:
andi	$t6,$t6,0x07FF

dsll	$t0,$t6,12
or	$v0,$v0,$t0        # v0 = Adapted_DISPLAY1 = (Adapted_DISPLAY1) OR (Calculated_DY)

# ------------------------------------------------------------------------------------------------------

sd	$v0, Adapted_DISPLAY1($s3)  # Store new DISPLAYx value (for feedback)

#  End of Patch to adapt request with the resulting request in v0


lb	$v1, DISPLAY_fix($s2)		# v1 = DISPLAY_fix
beql	$v1,$zero,store_v0_as_DISPLAY1			# in case of (DISPLAY_fix)
ld	$v0, Target_DISPLAY1($s1)	# use forced mode without adaption

store_v0_as_DISPLAY1:
sync.l				# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p				# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler_complex	# Now go exit (complex)
sd	$v0,0($a3)							# after storing

# ----------------------------
# DISPLAY2
# 
have_DISPLAY2_write:					# Here a1=source_data, a2->.gsm_engine, a3=dest_adress
lui	$v0,0xB200					# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0					# a3 = GS register address in KSEG1
sd	$a1, Source_DISPLAY2($s0)			# request DISPLAY2 value = a1
ld	$v1, Target_DISPLAY2($s1)			# v1=forcing DISPLAY2 template

#  Source_DISPLAY2 == Requested   DX, DY, MAGH, MAGV, DW and DH values
#  Target_DISPLAY2 == Modded(forced) DX, DY, MAGH, MAGV, DW and DH values
#  Both are 64 bit units with encoded bit fields like GS DISPLAY2 registers

#  Patch to adapt request to enforced mode in v1 MUST preserve a1, a2, a3

lb	$v0, ADAPTATION_fix($s2)
beql	$v0,$zero,have_DISPLAY2_write_1					# in case of (ADAPTATION_fix)
or	$a1,$zero,$v1					# 	simulate request same as forced mode

have_DISPLAY2_write_1:
li	$v0,0						# preclear v0 as result DISPLAY2 accumulator

#  Here a0=free, a1=Source_DISPLAY2, a2->.gsm_engine, a3=dest_address
#  Also v0=result_accumulator, v1=Target_DISPLAY2, t0-t7=free

//
// Automatic adaptation formulas
//
# ----- HORIZONTAL FIELDS -----
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | MAGH | 26:23 | int 0: 4:0 | magnification in horizontal direction | 0xF   |
# '------^-------^------------^---------------------------------------^-------^
dsrl	$t0,$a1,23
andi	$t0,$t0,0x0F
addi	$t0,$t0,1		# t0 = Source_Width_Scale = Source_MAGH + 1
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DW   | 43:32 | int 0:12:0 | display area width - 1 (VCK units)    | 0xFFF |
# '------^-------^------------^---------------------------------------^-------^
dsrl32	$t1,$a1,0
andi	$t1,$t1,0x0FFF
addi	$t1,$t1,1		# t1 = Source_Width = Source_DW + 1
divu	$zero,$t1,$t0	# LO = Source_Pixels_Width = Source_Width / Source_Width_Scale
dsrl32	$t0,$v1,0
andi	$t5,$t0,0x0FFF
mflo	$t4				# t4 = LO = Source_Pixels_Width
addi	$t6,$t5,1		# t6 = Target_Width = Target_DW + 1
nop
divu	$zero,$t6,$t4	# LO = Target_Width_Scale = Target_Width / Source_Pixels_Width
nop
nop
mflo	$t0				# t0 = LO = Target_Width_Scale
bne	$t0,$zero,have_DISPLAY2_write_2		# in case of (!Target_Width_Scale)
nop						# {
or	$t7,$zero,$t5		# t7 = Target_DW = Target_Width - 1
sub	$t0,$t6,$t4			# t0 = Target_Width - Source_Pixels_Width
li	$t4,0				# t4 = Target_MAGH = 0
beql	$zero,$zero,have_DISPLAY2_write_4		# }
nop						# otherwise
						# Target_Width_Scale nonzero

have_DISPLAY2_write_2:                     # {
addi	$t1,$t0,-16		# t1 = Target_Width_Scale - 16
bgtzl	$t1,have_DISPLAY2_write_3			# in case of (Target_Width_Scale > 0)
or	$t0,$zero,16		# 	t0 = Target_Width_Scale = 16;

have_DISPLAY2_write_3:
mult	$t4,$t0			# LO = Calculated_Width = (Target_MAGH * Target_Width_Scale)
nop
nop
mflo	$t1				# t1 = LO = Calculated_Width
addi	$t7,$t1,-1		# t7 = Calculated_DW = Calculated_Width - 1
addi	$t4,$t0,-1		# t4 = Calculated_MAGH = Target_Width_Scale - 1
sub	$t0,$t5,$t7			# t0 = Target_DW - Calculated_DW

have_DISPLAY2_write_4:                     # }
dsra $t0,$t0,1			# t0 = t0 / 2 = Half_Excess_Width (can be negative)
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DX   | 11:0  | int 0:12:0 | x pos in display area (VCK units)     | 0xFFF |
# '------^-------^------------^---------------------------------------^-------^
andi	$t1,$v1,0x0FFF	# t1 = Target_DX
add	$t6,$t0,$t1			# t6 = Calculated_DX = Target_DX + Half_Excess_Width
bltzl	$t6,have_DISPLAY2_write_5			# in case of (Calculated_DX < 0)
and	$t6,$t6,$zero		# 	Calculated_DX = 0;

have_DISPLAY2_write_5:
sub	$t0,$t1,$t6			# t0 = Target_DX - Calculated_DX
bgtzl	$t0,have_DISPLAY2_write_6			# in case of (Target_DX > Calculated_DX)
add	$t7,$t7,$t0			# 	t7 = Calculated_DW = Calculated_DW + Target_DX - Calculated_DX  # Target DW adjusted

have_DISPLAY2_write_6:
andi	$t7,$t7,0x0FFF
andi	$t4,$t4,0x000F
andi	$t6,$t6,0x0FFF
dsll32	$t0,$t7,0		# t0 = Calculated_DW
or	$v0,$v0,$t0			# v0 = Adapted_DISPLAY2 = (Adapted_DISPLAY2) OR (Calculated_DW)
dsll	$t0,$t4,23
or	$v0,$v0,$t0			# v0 = Adapted_DISPLAY2 = (Adapted_DISPLAY2) OR (Calculated_MAGH)

lw	$t1, X_offset($s2)	# t1 = X_offset (signed)
add	$t6, $t6, $t1		# t6 = Calculated_DX = Calculated_DX + X_offset
bgez	$t6, have_DISPLAY2_write_7		# Is the result greater or equal to zero?
nop
move	$t6, $zero		# t6 = Calculated_DX = 0
have_DISPLAY2_write_7:
andi	$t6,$t6,0x0FFF

or	$v0,$v0,$t6			# v0 = Adapted_DISPLAY2 = (Adapted_DISPLAY2) OR (Calculated_DX)

# ----- VERTICAL FIELDS -------
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | MAGV | 28:27 | int 0: 2:0 | magnification in vertical direction   | 0x3   |
# '------^-------^------------^---------------------------------------^-------^
dsrl	$t0,$a1,27
andi	$t0,$t0,0x03
addi	$t0,$t0,1		# t0= Source_Height_Scale = Source_MAGV + 1
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DH   | 54:44 | int 0:11:0 | display area height - 1 (pixel units) | 0x7FF |
# '------^-------^------------^---------------------------------------^-------^
dsrl32	$t1,$a1,12
andi	$t1,$t1,0x07FF
addi	$t1,$t1,1		# Source_Height = Source_DH + 1
divu	$zero,$t1,$t0	# LO = Source_Pixels_Height = Source_Height / Source_Height_Scale
dsrl32	$t0,$v1,12
andi	$t5,$t0,0x07FF
mflo	$t4				# t4 = LO = Source_Pixels_Height
addi	$t6,$t5,1		# t6 = Target_Height = Target_DH + 1
nop
divu	$zero,$t6,$t4	# LO = Target_Height_Scale = Target_Height / Source_Pixels_Height
nop
nop
mflo	$t0				# t0 = LO = Target_Height_Scale
bne	$t0,$zero,have_DISPLAY2_write_8		# in case of (!Target_Height_Scale)
nop						# {
or	$t7,$zero,$t5		# t7 = Target_DH = Target_Height - 1
sub	$t0,$t6,$t4			# t0 = Target_Height - Source_Pixels_Height
li	$t4,0				# t4 = Target_MAGV = 0
beql	$zero,$zero,have_DISPLAY2_write_10		# }
nop						# otherwise
						# Target_Height_Scale nonzero

have_DISPLAY2_write_8:						# {
addi	$t1,$t0,-4		# t1 = Target_Height_Scale - 4
bgtzl	$t1,have_DISPLAY2_write_9			# in case of (Target_Height_Scale > 0)
or	$t0,$zero,4			# 	t0 = Target_Height_Scale = 4;

have_DISPLAY2_write_9:
mult	$t4,$t0			# LO = Calculated_Height = (Target_MAGV * Target_Height_Scale)
nop
nop
mflo	$t1				# t1 = LO = Calculated_Height
addi	$t7,$t1,-1		# t7 = Calculated_DH = Calculated_Height - 1
addi	$t4,$t0,-1		# t4 = Calculated_MAGV = Target_Height_Scale - 1
sub	$t0,$t5,$t7			# t0 = Target_DH - Calculated_DH

have_DISPLAY2_write_10:						# }
dsra $t0,$t0,1			# t0 = t0 / 2 = Half_Excess_Height (can be negative)
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DY   | 22:12 | int 0:11:0 | y pos in display area (raster units)  | 0x7FF |
# '------^-------^------------^---------------------------------------^-------^
dsrl	$t1,$v1,12
andi	$t1,$t1,0x07FF	# t1 = Target_DY
add	$t6,$t0,$t1			# t6 = Calculated_DY = Target_DY + Half_Excess_Height
bltzl	$t6,have_DISPLAY2_write_11			# in case of (Calculated_DY < 0)
and	$t6,$t6,$zero		#	Calculated_DY = 0;

have_DISPLAY2_write_11:
sub	$t0,$t1,$t6			# t0 = Target_DY - Calculated_DY
bgtzl	$t0,have_DISPLAY2_write_12			# in case of (Target_DY > Calculated_DY)
add	$t7,$t7,$t0			# 	t7 = Calculated_DH = Calculated_DH + Target_DY - Calculated_DY # Target DH adjusted

have_DISPLAY2_write_12:
lb	$t0, Interlace_FRAME_Mode_Flag($s3)	# in case of Double Height not needed
beql	$t0,$zero,have_DISPLAY2_write_13							# 	Calculation is complete
nop

ld	$t0, Target_SMODE2($s1)
andi	$t0,$t0,1							# in case of Target_SMODE2.INT = 1 (Interlace Mode)
bne	$t0,$zero,have_DISPLAY2_write_13							# 	Calculation is complete
nop
beql	$t4,$zero,have_DISPLAY2_write_13	# in case of Calculated_MAGV = 0
addi	$t4,$t4,1		# 	go use Calculated_MAGV = Calculated_MAGV + 1
addi	$t4,$t4,2		# Calculated_MAGV = Calculated_MAGV + 2 (Because scale was 2 or larger)
addi	$t0,$t4,-4		# Compare Calculated_MAGV with 4 (too large ?)
bgezl	$t0,have_DISPLAY2_write_13			# in case of  Calculated_MAGV too large
ori	$t4,$zero,3			# 	go use Calculated_MAGV = 3

have_DISPLAY2_write_13:
andi	$t7,$t7,0x07FF
andi	$t4,$t4,0x0003
andi	$t6,$t6,0x07FF
dsll32	$t0,$t7,12
or	$v0,$v0,$t0			# v0 = Adapted_DISPLAY2 = (Adapted_DISPLAY2) OR (Calculated_DH)
dsll	$t0,$t4,27
or	$v0,$v0,$t0			# v0 = Adapted_DISPLAY2 = (Adapted_DISPLAY2) OR (Calculated_MAGV)

lw	$t1, Y_offset($s2)		# t1 = Y_offset (signed)
add	$t6, $t6, $t1		# t6 = Calculated_DY = Calculated_DY + Y_offset
bgez	$t6, have_DISPLAY2_write_14		# Is the result greater or equal to zero?
nop
move	$t6, $zero
have_DISPLAY2_write_14:
andi	$t6,$t6,0x07FF

dsll	$t0,$t6,12
or	$v0,$v0,$t0        # v0 = Adapted_DISPLAY2 = (Adapted_DISPLAY2) OR (Calculated_DY)

# ------------------------------------------------------------------------------------------------------

sd	$v0, Adapted_DISPLAY2($s3)  # Store new DISPLAYx value (for feedback)

#  End of Patch to adapt request with the resulting request in v0


lb	$v1, DISPLAY_fix($s2)		# v1 = DISPLAY_fix
beql	$v1,$zero,store_v0_as_DISPLAY2			# in case of (DISPLAY_fix)
ld	$v0, Target_DISPLAY2($s1)	# use forced mode without adaption

store_v0_as_DISPLAY2:
sync.l				# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p				# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
sd	$v0,0($a3)		# store DISPLAY2

# ------------------------------------------------------------------------------------------------------

lb	$a1, RC_adaptation($s3)
beqzl	$a1,exit_GSHandler_complex	# in case of RC_adaptation is disabled, go exit (complex)
nop

sync.l				# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p				# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
sd	$zero, GS_DISPLAY1($s4)		# DISPLAY1 should never be displayed on this scenario (in order to avoid garbage on screen)
beqzl	$zero,exit_GSHandler_complex
sb	$zero, RC_adaptation($s3)				# Disable RC_adaptation, since this patch should be applied only once, each time Hook_SetGsCrt is called.
											# Sega Genesis Collection (SLUS_215.42) doesn't repeatedly write to GS Priviledge Registers, but God of War I (SCUS_973.99) does this!

# ------------------------------------------------------------------------------------------------------

exit_GSHandler_complex:

#li	$v0, 0xFFFFFF 					# White
#sd	$v0, GS_BGCOLOUR($s4)		# Change BGCOLOR (for debugging purposes)

#  ----- Here we restore some registers, used for complex calculations above
lq	$t2, 0xA0($a2)					# t2
lq	$t3, 0xB0($a2)					# t3
lq	$t4, 0xC0($a2)					# t4
lq	$t5, 0xD0($a2)					# t5
lq	$t6, 0xE0($a2)					# t6
lq	$t7, 0xF0($a2)					# t7

exit_GSHandler:
#  ----- Here we restore most registers used for all GSHandler traps
#  ----- Since only a few registers are used this way we only restore those
la	$k0, MIPS_Regs		# Restore MIPS_Regs via k0
lq	$at, 0x10($k0)		# at
lq	$v0, 0x20($k0)		# v0
lq	$v1, 0x30($k0)		# v1
lq	$a0, 0x40($k0)		# a0
lq	$a1, 0x50($k0)		# a1
lq	$a2, 0x60($k0)		# a2
lq	$a3, 0x70($k0)		# a3
lq	$s0, 0x100($k0)
lq	$s1, 0x110($k0)
lq	$s2, 0x120($k0)
lq	$s3, 0x130($k0)
lq	$s4, 0x140($k0)
lq	$s5, 0x150($k0)
lq	$s6, 0x160($k0)
lq	$s7, 0x170($k0)
lq	$t8, 0x180($k0)
lq	$t9, 0x190($k0)

# Past this point in GSHandler, use only k0,k1,t0,t1

mfc0	$k0, $13		# k0 = Cause of last exception
srl	$k0, 30			# BD2 Flags debug exception in branch delay slot.
				# 1 -> delay slot, 0 -> normal
andi	$k0, 1			# k0 = BD2 bit isolated
bnez $k0,	GSHandler_BranchDelaySlotException
				# Deal properly with Branch Delay Slot Exceptions (when needed)
nop

mfc0	$k0,$30			# k0 = ErrorEPC
addiu	$k0,$k0,4		# k0 = ErrorEPC+4 (-> next opcode)
mtc0	$k0,$30			# store k0 in Error Exception Program Counter
#sync				# ensure COP0 register update before proceeding

b	GSHandler_Final_Exit
nop

# ----------------------------
GSHandler_BranchDelaySlotException:
mfc0	$k0,$30		# k0 = Error Exception Program Counter
lw	$k0,0($k0)	# k0 = instruction at EPC location (branch or jump)
srl	$k1,$k0,26	# k1 = aligned for opcode (range 0x00-0x3F)
andi	$k1,$k1,0x3F	# k1 = pure opcode number
sll	$k1,$k1,2	# k1 = op_num*2 (offset for jump table)

la	$t1, BD_t1	# t1 -> BD_t1
addu	$k1,$k1,$t1	# k1 -> active entry in BD_t1
lw	$t0,0($k1)	# t0 = opcode handler from BD_t1
la	$t1, op_t	# t1 -> op_t

jr	$t0		# jump to branch/jump opcode handlers
nop			# with k0=instruction, t1->op_t

# ----------------------------
B_com1:			# This group contains 8 different branch operations
srl	$k1,$k0,16	# k1 = aligned for sub_opcode (range 0x00-0x1F)
andi	$k1,$k1,0x3F	# k1 = pure sub_opcode number
sll	$k1,$k1,2	# k1 = sub_op_num*2 (offset for jump table)

la	$t1, BD_t2	# t1 -> BD_t2
addu	$k1,$k1,$t1	# k1 -> active entry in BD_t2
lw	$t0,0($k1)	# t0 = opcode handler from BD_t2
la	$t1, op_t	# t1 -> op_t

jr	$t0		# jump to branch/jump opcode handlers
nop			# with k0=instruction, t1->op_t

# ----------------------------
B_com0:			# opcode 0x00 includes both JR and JALR
li	$t0,0xFC1F07FF	# t0 = bitmask for JALR
and	$k1,$k0, $t0	# k1 = potential JALR instruction
li	$t0,9		# t0 = JALR test constant
beq	$k1,$t0,B_JR_JALR	# in case of  JALR identified, go deal with it
nop
li	$t0,0xFC1FFFFF	# t0 = bitmask for JR
and	$k1,$k0, $t0	# k1 = potential JR instruction
li	$t0,8		# t0 = JR test constant
bne	$k1,$t0,B_skip	# in case of  JR not identified, go skip this code 
nop

B_JR_JALR:		# JR or JALR found, so make register indirect jump
srl	$k1,$k0, 21	# k1 = aligned for JR/JALR rs register number
andi	$k1,0x1F	# k1 = register number
sll	$t1,$k1, 4	# t1 = array index for saved register data
la	$t0,MIPS_Regs	# t0 -> saved register array
addu	$t0,$t1		# t0 -> register data of JR/JALR rs register
lw	$t0,0($t0)	# t0 = jump destination address
mtc0	$t0,$30		# store t0 in Error Exception Program Counter
#	sync		# ensure COP0 register update before proceeding
b	GSHandler_Final_Exit
nop

	# ----------------------------

B_J:
B_JAL:
# Here we have a definite jump with absolute address/4 in instruction
li	$t0, 0x3FFFFFF	# t0 = bitmask for jump destination bits
and	$k1, $k0, $t0	# k1 = destination bits (== destination/4)
sll	$t0, $k1, 2	# t0 = jump destination address
mtc0	$t0, $30	# store t0 in Error Exception Program Counter
#sync			# ensure COP0 register update before proceeding
b	GSHandler_Final_Exit
nop

#  ----------------------------
#  'likely' type branches will only trap on delay slot if branch is taken,
#  so for those cases we do not need to make any further tests of conditions
B_likely:
B_BGEZL:
B_BGEZALL:
B_BLTZL:
B_BLTZALL:
B_BEQL:
B_BNEL:
B_BLEZL:
B_BGTZL:
B_taken:
#  Here we have a 'branch taken' operation with relative offset/4 in instruction
li	$t0, 0xFFFF	# t0 = bitmask for branch offset bits
and	$k1, $k0, $t0	# k1 = branch offset bits (== offset/4)
sll	$k1, 2		# k1 = branch offset
mfc0	$t0, $30	# t0 = Error Exception Program Counter
addiu	$t0, 4		# t0 = ErrorEPC+4 (-> address after branch op)
addu	$t0, $k1	# t0 = jump destination address
mtc0	$t0, $30	# store t0 in Error Exception Program Counter
#sync			# ensure COP0 register update before proceeding
b	GSHandler_Final_Exit
nop

#  ----------------------------
B_BLTZ:
B_BLTZAL:
srl	$k1,$k0, 21	# k1 = aligned for rs register number
andi	$k1,0x1F	# k1 = register number
sll	$t1,$k1, 4	# t1 = array index for saved register data
la	$t0,MIPS_Regs	# t0 -> saved register array
addu	$t0,$t1		# t0 -> register data of rs register
lq	$t1,0($t0)	# t1 = rs register data
#sync
bltz	$t1,B_taken
nop

B_not_taken:
mfc0	$k0,$30			# k0 = ErrorEPC
addiu	$k0,$k0,8		# k0 = ErrorEPC+8 pass branch_op and delay_slot
mtc0	$k0,$30			# store k0 in Error Exception Program Counter
#sync				# ensure COP0 register update before proceeding
b	GSHandler_Final_Exit
nop
	
#  ----------------------------
B_BGEZ:
B_BGEZAL:
srl	$k1,$k0, 21	# k1 = aligned for rs register number
andi	$k1,0x1F	# k1 = register number
sll	$t1,$k1, 4	# t1 = array index for saved register data
la	$t0,MIPS_Regs	# t0 -> saved register array
addu	$t0,$t1		# t0 -> register data of rs register
lq	$t1,0($t0)	# t1 = rs register data
#sync
bgez	$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BLEZ:
srl	$k1,$k0, 21	# k1 = aligned for rs register number
andi	$k1,0x1F	# k1 = register number
sll	$t1,$k1, 4	# t1 = array index for saved register data
la	$t0,MIPS_Regs	# t0 -> saved register array
addu	$t0,$t1		# t0 -> register data of rs register
lq	$t1,0($t0)	# t1 = rs register data
#sync
blez	$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BGTZ:
srl	$k1,$k0, 21	# k1 = aligned for rs register number
andi	$k1,0x1F	# k1 = register number
sll	$t1,$k1, 4	# t1 = array index for saved register data
la	$t0,MIPS_Regs	# t0 -> saved register array
addu	$t0,$t1		# t0 -> register data of rs register
lq	$t1,0($t0)	# t1 = rs register data
#sync
bgtz	$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BEQ:
srl	$k1,$k0, 21	# k1 = aligned for rs register number
andi	$k1,0x1F	# k1 = rs register number
sll	$k1,$k1, 4	# k1 = array index for saved rs register data
la	$t0,MIPS_Regs	# t0 -> saved register array
addu	$k1,$t0		# k1 -> register data of rs register
lq	$t1,0($k1)	# t1 = rs register data
#sync
srl	$k1,$k0, 16	# k1 = aligned for rt register number
andi	$k1,0x1F	# k1 = rt register number
sll	$k1,$k1, 4	# k1 = array index for saved rt register data
addu	$k1,$t0		# k1 -> register data of rt register
lq	$t0,0($k1)	# t0 = rt register data
#sync
beq	$t0,$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BNE:
srl	$k1,$k0, 21	# k1 = aligned for rs register number
andi	$k1,0x1F	# k1 = rs register number
sll	$k1,$k1, 4	# k1 = array index for saved rs register data
la	$t0,MIPS_Regs	# t0 -> saved register array
addu	$k1,$t0		# k1 -> register data of rs register
lq	$t1,0($k1)	# t1 = rs register data
#sync
srl	$k1,$k0, 16	# k1 = aligned for rt register number
andi	$k1,0x1F	# k1 = rt register number
sll	$k1,$k1, 4	# k1 = array index for saved rt register data
addu	$k1,$t0		# k1 -> register data of rt register
lq	$t0,0($k1)	# t0 = rt register data
#sync
bne	$t0,$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BC0x:			# At present we do not implement COP0 branches
B_BC1x:			# At present we do not implement COP1 branches
B_skip:			# Unrecognized opcode, so just pass it by
mfc0	$k0, $30	# k0 = ErrorEPC
addiu	$k0, $k0, 4	# k0 = ErrorEPC+4 (-> next opcode)
mtc0	$k0, $30	# store k0 in Error Exception Program Counter
#sync			# ensure COP0 register update before proceeding

GSHandler_Final_Exit:
la	$k0, MIPS_Regs	# k0 -> .gsm_engine
lq	$t0, 0x80($k0)	# t0
lq	$t1, 0x90($k0)	# t1

#mfc0	$k0, $12	# Set user mode, interrupts on
#ori	$k0, $k0, 0x11
#mtc0	$k0, $12
#sync

lq	$k0, -0x10($zero)	# Restore k0,k1 reserved for OS Kernel
lq	$k1, -0x20($zero)
sync.p
sync.l
eret			# 	Return from exception
nop

end_GSHandler:
.end	GSHandler

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

.set pop
