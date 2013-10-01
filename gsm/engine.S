#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#

#include <ee_cop0_defs.h>
#include <syscallnr.h>

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

# GS Registers
KSEG=0x00080000
GS_BASE=0x12000000
GS_SMODE1=GS_BASE+0x0010
GS_SMODE2=GS_BASE+0x0020
GS_SRFSH=GS_BASE+0x0030
GS_SYNCH1=GS_BASE+0x0040
GS_SYNCH2=GS_BASE+0x0050
GS_SYNCV=GS_BASE+0x0060
GS_DISPLAY1=GS_BASE+0x0080
GS_DISPLAY2=GS_BASE+0x00A0
GS_BGCOLOUR=GS_BASE+0x00E0
GS_CSR=GS_BASE+0x1000

# Additional video modes (made possible by SP193 and reprep)
GS_MODE_DTV_576P=0x53
GS_MODE_DTV_1080P=0x54

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#####################################################
# GSM Engine section
#####################################################

.section .gsm_engine, "a"

.p2align 4	# align 16 bytes (IMPORTANT!!!)

.set push

.set noreorder # disable assembler reorder mode, so the code will not be optimized or changed in any way, giving complete instruction order control to the programmer
.set noat # disable assembler from using register $1 (known as the assembler temporary, or $at register) to hold intermediate values when performing macro expansions
.set macro # disable warning if any statement expands to more than one machine instruction
#.set nomacro # enable warning if any statement expands to more than one machine instruction

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

########################
# Pointers to Functions
########################

.globl InitGSM_Ptr
InitGSM_Ptr:	.word InitGSM		# Pointer to InitGSM

.globl EnableGSM_Ptr
EnableGSM_Ptr:	.word EnableGSM		# Pointer to EnableGSM

.globl DisableGSM_Ptr
DisableGSM_Ptr:	.word DisableGSM	# Pointer to DisableGSM

##############
# Main Status 
##############
.globl GSMStatus
GSMStatus:	.word 0		# GSM Enable/Disable Status

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
lui	$v0,(KSEG >> 16)			# Base Address
sw	$a0, %lo(Source_INTERLACE)($v0)
sw	$a1, %lo(Source_MODE)($v0)
sw	$a2, %lo(Source_FFMD)($v0)
and	$a3, $a0,$a2				# a3 = Interlace & Field
andi	$a3, $a3,1				# a3 &= 1 limited to 1 bit
sb	$a3, %lo(DOUBLE_HEIGHT_adaptation)($v0)

//-----------------------------------------------------------------------------------------

// Remove all breakpoints (even when they aren't enabled)
di			// Disable Interupts
li $a2, 0x8000
mtbpc $a2		// All breakpoints off (BED = 1)
sync.p			// Await instruction completion

//-----------------------------------------------------------------------------------------

lb	$a0, %lo(skip_videos_fix)($v0)		# a0 = skip_videos_fix
bnel	$a0,$zero, Skip_skip_videos_Patch	# in case of Skip Videos fix is disabled (i.e. not equal to zero)
nop					# skip this fix
// 'skip videos' (sceMpegIsEnd) code - basic method by nachbrenner (original idea from CMX and bongsan)
// Source: http://replay.waybackmachine.org/20040419134616/http://nachbrenner.pcsx2.net/chapter1.html

la	$a2, Pattern_Code_sceMpegIsEnd					# $a2 = pattern start address
la	$a3, Pattern_Mask_sceMpegIsEnd					# $a3 = pattern mask start address
la	$a1, Replacement_Code_sceMpegIsEnd				# $a1 = replacement code address
jal	OnTheFlyPatcher
nop

//-----------------------------------------------------------------------------------------

Skip_skip_videos_Patch:
ld	$a0, %lo(Target_SMODE2)($v0)
sb	$a0, %lo(SMODE2_adaptation)($v0)
lw	$a0, %lo(Target_INTERLACE)($v0)
lw	$a1, %lo(Target_MODE)($v0)
lw	$a2, %lo(Target_FFMD)($v0)
sync.l						# The opcodes sync.l and sync.p made a lot of games compatible
sync.p						# They ensure all values are stable before the following call

//-----------------------------------------------------------------------------------------

//
//This function will get called first, in place of the original SetGsCrt() syscall handler. It will check the video mode argument.
//If it's one of the extended video modes, do not call the original SetGsCrt() syscall handler, 
//but handle GS configuration on our own.
//Otherwise, call the original SetGsCrt() syscall handler and let it configure the GS for us.
//

li	$a3, GS_MODE_DTV_576P
bne	$a1, $a3, Skip_DTV_576P
nop

//-----------------------------------------------------------------------------------------

DTV_576P:

//$v0 = Base address
lui	$v0,(KSEG >> 16)
//$a0 = Target_SMODE2
ld	$a0, %lo(Target_SMODE2)($v0)

//$v0 = GS base address
lui		$v0, (GS_BASE >> 16)

//*GS_SMODE1=0x00000017424B0504;
lui     $v1, 0x0017
ori     $v1, 0x424B
dsll    $v1, 16
ori     $v1, 0x0504
sd		$v1, %lo(GS_SMODE1)($v0)	# store it

//*GS_SYNCH1=0x000402E02003C827;
lui     $v1, 0x0004
ori     $v1, 0x02E0
dsll    $v1, 16
ori     $v1, 0x2003
dsll    $v1, 16
ori     $v1, 0xC827
sd		$v1, %lo(GS_SYNCH1)($v0)	# store it

//*GS_SYNCH2=0x0019CA67;
lui     $v1, 0x0019
ori     $v1, 0xCA67
sd		$v1, %lo(GS_SYNCH2)($v0)	# store it

//*GS_SYNCV=0x00A9000002700005;
lui     $v1, 0x00A9
ori     $v1, 0x0000
dsll    $v1, 16
ori     $v1, 0x0270
dsll    $v1, 16
ori     $v1, 0x0005
sd		$v1, %lo(GS_SYNCV)($v0)	# store it

//*GS_SMODE2=Target_SMODE2;
or      $v1, $zero, $a0				# 	v1 = $a0 = Target_SMODE2;
sd		$v1, %lo(GS_SMODE2)($v0)	# store it

//*GS_SRFSH=0;
li      $v1, 4
sd		$v1, %lo(GS_SRFSH)($v0)	# store it

//*GS_SMODE1=0x0000001742490504;
lui     $v1, 0x0017
ori     $v1, 0x4249
dsll    $v1, 16
ori     $v1, 0x0504
sd		$v1, %lo(GS_SMODE1)($v0)	# store it

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
sd		$v1, %lo(GS_SMODE1)($v0)	# store it

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
lui	$v0,(KSEG >> 16)
//$a0 = Target_SMODE2
ld	$a0, %lo(Target_SMODE2)($v0)

//$v0 = GS base address
lui		$v0, (GS_BASE >> 16)

//*GS_SMODE1=0x00000003422304B1
lui     $v1, 0x0003
ori     $v1, 0x4223
dsll    $v1, 16
ori     $v1, 0x04B1
or      $v1, $v0, $v1
sd		$v1, %lo(GS_SMODE1)($v0)	# store it

//if((*GS_REG_CSR>>16&0xFF)>=0x19){
ld      $v1, %lo(GS_CSR)($v0)	# load it
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
sd		$v1, %lo(GS_SYNCH1)($v0)	# store it

//		*GS_REG_SYNCH2=0x 0020 FB61;
lui     $v1, 0x0020
ori     $v1, 0xFB61
sd		$v1, %lo(GS_SYNCH2)($v0)	# store it

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
sd		$v1, %lo(GS_SYNCH1)($v0)	# store it

//		*GS_REG_SYNCH2=0x 0020 EB63;
lui     $v1, 0x0020
ori     $v1, 0xEB63
sd		$v1, %lo(GS_SYNCH2)($v0)	# store it

//	}
DTV_1080P_endif:
//*GS_SYNCV=0x0150 E002 01C0 0005;
lui     $v1, 0x0150
ori     $v1, 0xE002
dsll    $v1, 16
ori     $v1, 0x01C0
dsll    $v1, 16
ori     $v1, 0x0005
sd		$v1, %lo(GS_SYNCV)($v0)	# store it

//*GS_SMODE2=Target_SMODE2;
or      $v1, $zero, $a0				# 	v1 = $a0 = Target_SMODE2;
sd		$v1, %lo(GS_SMODE2)($v0)	# store it

//*GS_SRFSH=0;
li      $v1, 4
sd		$v1, %lo(GS_SRFSH)($v0)	# store it

//*GS_SMODE1=0x00000003422204B1;
lui     $v1, 0x0003
ori     $v1, 0x4222
dsll    $v1, 16
ori     $v1, 0x04B1
sd		$v1, %lo(GS_SMODE1)($v0)	# store it

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
sd		$v1, %lo(GS_SMODE1)($v0)	# store it

beqzl	$zero, Skip_Call_Original_SetGsCrt
nop

//-----------------------------------------------------------------------------------------
Skip_DTV_1080P:
lw	$a3, %lo(Old_SetGsCrt)($v0)		# a3 -> original SetGsCrt function
jalr	$a3							# Call original SetGsCrt
nop 

//-----------------------------------------------------------------------------------------
Skip_Call_Original_SetGsCrt:

//Re-enable GSHandler whenever Hook_SetGsCrt is called
//This aggressive approach is needed for those titles which disable breakpoints 
//For instance: UP (SLUS 21864)
//-----------------------------------------------------------------------------------------

// Set Data Address Write Breakpoint
// Trap writes to GS registers, so as to control their values
	
lui $a0, (GS_BASE >> 16)	// Address base for trapping
li $a1, 0x1FFFFE1F			// Address mask for trapping	//DOCTORXYZ
			//We trap writes to 0x12000000 + 0x00,0x20,0x40,0x60,0x80,0xA0,0xC0,0xE0,0x100,0x120,0x140,0x160,0x180,0x1A0,0x1C0,0x1E0	//DOCTORXYZ
			//We only want 0x20, 0x60, 0x80, 0xA0, 0x100, but can't mask for that combination //DOCTORXYZ
			//But the trapping range is now extended to match all kernel access segments

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

lui	$k0,(KSEG >> 16)	# Store MIPS_Regs via k0
sq	$zero, 0($k0)		# $zero
sq	$1, 0x10($k0)		# at
sq	$2, 0x20($k0)		# v0
sq	$3, 0x30($k0)		# v1
sq	$4, 0x40($k0)		# a0
sq	$5, 0x50($k0)		# a1
sq	$6, 0x60($k0)		# a2
sq	$7, 0x70($k0)		# a3
sq	$8, 0x80($k0)
sq	$9, 0x90($k0)
sq	$10, 0xA0($k0)
sq	$11, 0xB0($k0)
sq	$12, 0xC0($k0)
sq	$13, 0xD0($k0)
sq	$14, 0xE0($k0)
sq	$15, 0xF0($k0)
sq	$16, 0x100($k0)
sq	$17, 0x110($k0)
sq	$18, 0x120($k0)
sq	$19, 0x130($k0)
sq	$20, 0x140($k0)
sq	$21, 0x150($k0)
sq	$22, 0x160($k0)
sq	$23, 0x170($k0)
sq	$24, 0x180($k0)
sq	$25, 0x190($k0)
				# 0x1A0 must be set later, to initial $k0 value
				# 0x1B0 must be set later, to initial $k1 value
sq	$28, 0x1C0($k0)
sq	$29, 0x1D0($k0)
sq	$30, 0x1E0($k0)
sq	$31, 0x1F0($k0)

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

lui	$a2,(KSEG >> 16)	# a2 -> .gsm_engine
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
addi	$v1,$v0, %lo(GS_BASE - GS_SMODE2)
beqzl	$v1,have_SMODE2_write			# in case of  dest == GS_reg_SMODE2
nop
addi	$v1,$v0, %lo(GS_BASE - GS_SYNCV)
beqzl	$v1,have_SYNCV_write			# in case of  dest == GS_reg_SYNCV
nop
addi	$v1,$v0, %lo(GS_BASE - GS_DISPLAY1)
beqzl	$v1,have_DISPLAY1_write			# in case of  dest == GS_reg_DISPLAY1
nop
addi	$v1,$v0, %lo(GS_BASE - GS_DISPLAY2)
beqzl	$v1,have_DISPLAY2_write			# in case of  dest == GS_reg_DISPLAY2
nop

not_wanted_reg:					# Register unwanted, so perform op unchanged
sd	$a1,0($a3)				# Store source data unchanged to destination
beqzl	$zero,exit_GSHandler
nop

#  ----------------------------	
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
lui	$v0,0xB200					# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0					# a3 = GS register address in KSEG1
sd	$a1, %lo(Source_SMODE2)($a2)			# Source_SMODE2 = a1
lb	$v0, %lo(SMODE2_fix)($a2)			# v0 = SMODE2_fix
bnel	$v0,$zero,store_v0_as_SMODE2			# in case of  Separate SMODE2 fix disabled
or	$v0,$zero,$a1					# go use v0=a1 for SMODE2
srl	$v0,$a1,1					# v0 = a1 aligned for FFMD in bit0
and	$v0,$v0,$a1					# v0 bit 0 = INT & FFMD
andi	$v0,$v0,1					# v0 bit 0 = INT & FFMD isolated
sb	$v0, %lo(DOUBLE_HEIGHT_adaptation)($a2)		# store Adapt_DoubleHeight flag
beqz	$v0,1f						# in case of  no DoubleHeight need
ld	$v0, %lo(Target_SMODE2)($a2)			# go use Target_SMODE2 as adapted SMode2
																							# otherwise just set v0 = Target_SMODE2
andi	$a1,$a1,2					# a1 = FFMD of Source_SMODE2
andi	$v0,$v0,0xFFFD					# v0 = Target_SMODE2 without FFMD
or	$v0,$v0,$a1					# v0 = Target_SMODE2 + Source FFMD
1: 							# Here v0 is adapted SMode2 value
sb	$v0, %lo(SMODE2_adaptation)($a2)		# Remember this adaption for later

store_v0_as_SMODE2:
sync.l							# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p							# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler				# Now go exit
sd	$v0,0($a3)					# after storing GS_reg_SMODE2

#  ----------------------------
# SYNCV
# .----.-----.----------.
# |Name|Pos. |Format    |
# |----+-----+----------+
# |VFP | 9:0 |int 0:10:0|
# |VFPE|19:10|int 0:10:0|
# |VBP |31:20|int 0:12:0|
# |VBPE|41:32|int 0:12:0|
# |VDP |52:42|int 0:11:0|
# |VS  |??:53|int 0:??:0|
# '----^-----^----------^
have_SYNCV_write:
lui	$v0,0xB200					# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0					# a3 = GS register address in KSEG1
sd	$a1, %lo(Source_SYNCV)($a2)			# Source_SYNCV = a1
lb	$v0, %lo(SYNCV_fix)($a2)			# v0 = SYNCV_fix
bnel	$v0,$zero,store_v0_as_SYNCV			# in case of  Separate SYNCV fix disabled
or	$v0,$zero,$a1					# 	go use v0=a1 for SYNCV
ld	$v0, %lo(Target_SYNCV)($a2)			# v0 = Target_SYNCV
beql	$v0,$zero,exit_GSHandler			# in case of  Target_SYNCV is zero
sd	$a1,0($a3)					# 	go use Source_SYNCV

store_v0_as_SYNCV:
sync.l				# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p				# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler				# Now go exit
sd	$v0,0($a3)					# after storing GS_SYNCV

#  ----------------------------
have_DISPLAY1_write:					# Here a1=source_data, a2->.gsm_engine, a3=dest_adress
lui	$v0,0xB200					# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0					# a3 = GS register address in KSEG1
sd	$a1, %lo(Source_DISPLAY1)($a2)			# request DISPLAY1 value = a1
ld	$v1, %lo(Target_DISPLAY1)($a2)			# v1=forcing DISPLAY1 template

#  Source_DISPLAY1 == Requested   DX, DY, MAGH, MAGV, DW and DH values
#  Target_DISPLAY1 == Modded(forced) DX, DY, MAGH, MAGV, DW and DH values
#  Both are 64 bit units with encoded bit fields like GS DISPLAY1 register

#  Patch to adapt request to enforced mode in v1 MUST preserve a1, a2, a3

lb	$v0, %lo(automatic_adaptation)($a2)
bnel	$v0,$zero,have_DISPLAY1_write_1					# in case of (automatic_adaptation)
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
beq	$zero,$zero,have_DISPLAY1_write_4		# }
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

la	$t0, X_offset
lw	$t1, 0($t0)			# t1 = X_offset (signed)
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
beq	$zero,$zero,have_DISPLAY1_write_10		# }
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
lb	$t0, %lo(DOUBLE_HEIGHT_adaptation)($a2)	# in case of  doubled height not needed
beq	$t0,$zero,have_DISPLAY1_write_13							# 	Calculation is complete

ld	$t0, %lo(Target_SMODE2)($a2)
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

la	$t0, Y_offset
lw	$t1, 0($t0)			# t1 = Y_offset (signed)
add	$t6, $t6, $t1		# t6 = Calculated_DY = Calculated_DY + Y_offset
bgez	$t6, have_DISPLAY1_write_14		# Is the result greater or equal to zero?
nop
move	$t6, $zero
have_DISPLAY1_write_14:
andi	$t6,$t6,0x07FF

dsll	$t0,$t6,12
or	$v0,$v0,$t0        # v0 = Adapted_DISPLAY1 = (Adapted_DISPLAY1) OR (Calculated_DY)

# ------------------------------------------------------------------------------------------------------

sd	$v0, %lo(Adapted_DISPLAY1)($a2)  # Store new DISPLAY1 value (for feedback)

#  End of Patch to adapt request with the resulting request in v0


lb	$v1, %lo(DISPLAY_fix)($a2)					# v1 = DISPLAY_fix
bnel	$v1,$zero,store_v0_as_DISPLAY1							# in case of (DISPLAY_fix)
ld	$v0, %lo(Target_DISPLAY1)($a2)					# use forced mode without adaption

store_v0_as_DISPLAY1:
sync.l				# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p				# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler_complex				# Now go exit (complex)
sd	$v0,0($a3)					# after storing GS_DISPLAY1

#  ----------------------------
have_DISPLAY2_write:					# Here a1=source_data, a2->.gsm_engine, a3=dest_adress
lui	$v0,0xB200					# v0 = GS base address in KSEG1
andi	$a3,$a3,0xFFFF					# a3 = GS register offset
or	$a3,$a3,$v0					# a3 = GS register address in KSEG1
sd	$a1, %lo(Source_DISPLAY2)($a2)			# request DISPLAY2 value = a1
ld	$v1, %lo(Target_DISPLAY2)($a2)			# v1=forcing DISPLAY2 template

#  Source_DISPLAY2 == Requested   DX, DY, MAGH, MAGV, DW and DH values
#  Target_DISPLAY2 == Modded(forced) DX, DY, MAGH, MAGV, DW and DH values
#  Both are 64 bit units with encoded bit fields like GS DISPLAY2 register

#  Patch to adapt request to enforced mode in v1 MUST preserve a1, a2, a3

lb	$v0, %lo(automatic_adaptation)($a2)
bnel	$v0,$zero,have_DISPLAY2_write_1					# in case of (automatic_adaptation)
or	$a1,$zero,$v1					# 	simulate request same as forced mode

have_DISPLAY2_write_1:
li	$v0,0						# preclear v0 as result DISPLAY1 accumulator

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
beq	$zero,$zero,have_DISPLAY2_write_4		# }
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

la	$t0, X_offset
lw	$t1, 0($t0)			# t1 = X_offset (signed)
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
beq	$zero,$zero,have_DISPLAY2_write_10		# }
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
lb	$t0, %lo(DOUBLE_HEIGHT_adaptation)($a2)	# in case of  doubled height not needed
beq	$t0,$zero,have_DISPLAY2_write_13							# 	Calculation is complete

ld	$t0, %lo(Target_SMODE2)($a2)
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

la	$t0, Y_offset
lw	$t1, 0($t0)			# t1 = Y_offset (signed)
add	$t6, $t6, $t1		# t6 = Calculated_DY = Calculated_DY + Y_offset
bgez	$t6, have_DISPLAY2_write_14		# Is the result greater or equal to zero?
nop
move	$t6, $zero
have_DISPLAY2_write_14:
andi	$t6,$t6,0x07FF

dsll	$t0,$t6,12
or	$v0,$v0,$t0        # v0 = Adapted_DISPLAY2 = (Adapted_DISPLAY2) OR (Calculated_DY)

# ------------------------------------------------------------------------------------------------------

sd	$v0, %lo(Adapted_DISPLAY2)($a2)  # Store new DISPLAY2 value (for feedback)

#  End of Patch to adapt request with the resulting request in v0


lb	$v1, %lo(DISPLAY_fix)($a2)					# v1 = DISPLAY_fix
bnel	$v1,$zero,store_v0_as_DISPLAY2							# in case of (DISPLAY_fix)
ld	$v0, %lo(Target_DISPLAY2)($a2)					# use forced mode without adaption

store_v0_as_DISPLAY2:
sync.l				# The addition of these two lines (sync.l and sync.p) made a lot of titles compatible with GSM!
sync.p				# These ones give a break to ee take a breath after patching and before enter original SetGsCrt
beqzl	$zero,exit_GSHandler_complex				# Now go exit (complex)
sd	$v0,0($a3)					# after storing GS_DISPLAY2

exit_GSHandler_complex:

#li	$t0, 0xFFFFFF 					# White
#sd	$t0, %lo(GS_BGCOLOUR)($a0)		# Change BGCOLOR (for debugging purposes)

#  ----- Here we restore some registers, used for complex calculations for DISPLAY1 and DISPLAY2
lq	$10, 0xA0($a2)					# t2
lq	$11, 0xB0($a2)					# t3
lq	$12, 0xC0($a2)					# t4
lq	$13, 0xD0($a2)					# t5
lq	$14, 0xE0($a2)					# t6
lq	$15, 0xF0($a2)					# t7

exit_GSHandler:
#  ----- Here we restore most registers used for all GSHandler traps
#  ----- Since only a few registers are used this way we only restore those
lui	$k0,(KSEG >> 16)	# Restore MIPS_Regs via k0
lq	$1, 0x10($k0)		# at
lq	$2, 0x20($k0)		# v0
lq	$3, 0x30($k0)		# v1
lq	$4, 0x40($k0)		# a0
lq	$5, 0x50($k0)		# a1
lq	$6, 0x60($k0)		# a2
lq	$7, 0x70($k0)		# a3

#  Past this point in GSHandler, use only k0,k1,t0,t1

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
lui	$t0,(KSEG >> 16)	# t0 -> saved register array
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
srl	$k1,$k0, 21		# k1 = aligned for rs register number
andi	$k1,0x1F		# k1 = register number
sll	$t1,$k1, 4		# t1 = array index for saved register data
lui	$t0,(KSEG >> 16)	# t0 -> saved register array
addu	$t0,$t1			# t0 -> register data of rs register
lq	$t1,0($t0)		# t1 = rs register data
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
srl	$k1,$k0, 21		# k1 = aligned for rs register number
andi	$k1,0x1F		# k1 = register number
sll	$t1,$k1, 4		# t1 = array index for saved register data
lui	$t0,(KSEG >> 16)	# t0 -> saved register array
addu	$t0,$t1			# t0 -> register data of rs register
lq	$t1,0($t0)		# t1 = rs register data
#sync
bgez	$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BLEZ:
srl	$k1,$k0, 21					# k1 = aligned for rs register number
andi	$k1,0x1F							# k1 = register number
sll	$t1,$k1, 4						# t1 = array index for saved register data
lui	$t0,(KSEG >> 16)			# t0 -> saved register array
addu	$t0,$t1							# t0 -> register data of rs register
lq	$t1,0($t0)						# t1 = rs register data
#sync
blez	$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BGTZ:
srl	$k1,$k0, 21					# k1 = aligned for rs register number
andi	$k1,0x1F							# k1 = register number
sll	$t1,$k1, 4						# t1 = array index for saved register data
lui	$t0,(KSEG >> 16)			# t0 -> saved register array
addu	$t0,$t1							# t0 -> register data of rs register
lq	$t1,0($t0)						# t1 = rs register data
#sync
bgtz	$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BEQ:
srl	$k1,$k0, 21					# k1 = aligned for rs register number
andi	$k1,0x1F							# k1 = rs register number
sll	$k1,$k1, 4						# k1 = array index for saved rs register data
lui	$t0,(KSEG >> 16)			# t0 -> saved register array
addu	$k1,$t0							# k1 -> register data of rs register
lq	$t1,0($k1)						# t1 = rs register data
#sync
srl	$k1,$k0, 16					# k1 = aligned for rt register number
andi	$k1,0x1F							# k1 = rt register number
sll	$k1,$k1, 4						# k1 = array index for saved rt register data
addu	$k1,$t0							# k1 -> register data of rt register
lq	$t0,0($k1)						# t0 = rt register data
#sync
beq	$t0,$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BNE:
srl	$k1,$k0, 21					# k1 = aligned for rs register number
andi	$k1,0x1F							# k1 = rs register number
sll	$k1,$k1, 4						# k1 = array index for saved rs register data
lui	$t0,(KSEG >> 16)			# t0 -> saved register array
addu	$k1,$t0							# k1 -> register data of rs register
lq	$t1,0($k1)						# t1 = rs register data
#sync
srl	$k1,$k0, 16					# k1 = aligned for rt register number
andi	$k1,0x1F							# k1 = rt register number
sll	$k1,$k1, 4						# k1 = array index for saved rt register data
addu	$k1,$t0							# k1 -> register data of rt register
lq	$t0,0($k1)						# t0 = rt register data
#sync
bne	$t0,$t1,B_taken
nop
b	B_not_taken
nop

#  ----------------------------
B_BC0x:								# At present we do not implement COP0 branches
B_BC1x:								# At present we do not implement COP1 branches
B_skip:								# Unrecognized opcode, so just pass it by
mfc0	$k0, $30						# k0 = ErrorEPC
addiu	$k0, $k0, 4				# k0 = ErrorEPC+4 (-> next opcode)
mtc0	$k0, $30						# store k0 in Error Exception Program Counter
#sync											# ensure COP0 register update before proceeding

GSHandler_Final_Exit:
lui	$k0,(KSEG >> 16)			# k0 -> .gsm_engine
lq	$8, 0x80($k0)					# t0
lq	$9, 0x90($k0)					# t1

#mfc0	$k0, $12							# Set user mode, interrupts on
#ori	$k0, $k0, 0x11
#mtc0	$k0, $12
#sync

lq	$k0, -0x10($zero)					# Restore k0,k1 reserved for OS Kernel
lq	$k1, -0x20($zero)
sync.p
sync.l
eret											# 	Return from exception
nop

end_GSHandler:
.end	GSHandler

//--------------------
// On-The-Fly patcher
//--------------------

#.globl OnTheFlyPatcher
.ent OnTheFlyPatcher
OnTheFlyPatcher:

lui	$a0,(KSEG >> 16)		# Base Address
sw	$a2, %lo(Pattern_Code_Ptr)($a0)		# Store Pattern Code Address
sw	$a3, %lo(Pattern_Mask_Ptr)($a0)		# Store Pattern Mask Address
sw	$a1, %lo(Replacement_Code_Ptr)($a0)	# Store Replacement Code Address

sq	$v0, 0x20($a0)			# v0
sq	$v1, 0x30($a0)			# v1
sq	$t0, 0x80($a0)			# t0
sq	$t1, 0x90($a0)			# t1
sq	$t2, 0xA0($a0)			# t2
sq	$t3, 0xB0($a0)			# t3
sq	$t4, 0xC0($a0)			# t4
sq	$t5, 0xD0($a0)			# t5

li	$a0, 0x000D0000			# $a0 = scan start address
li	$a1, 0x02000000			# $a1 = scan end address
li	$v0, 0xFFFFFFFF			# $v0 = Ending (Wild Card) Value
lui	$t5,(KSEG >> 16)		# Base Address
beqzl	$zero, OnTheFlyPatcher_Start
nop

OnTheFlyPatcher_OuterLoop:
or	$a0,$zero,$t4			# $a0 = restore current scan address (from temporary $t4)
addu	$a0, $a0, 4			# increase scan address
slt	$t0, $a0, $a1			# compare current scan address with end scan address
beqz	$t0, OnTheFlyPatcher_End		# continue comparison until the end scan address
nop

OnTheFlyPatcher_Start:
or	$t4,$zero,$a0			# $t4 = temporary store of current scan address (obtained from $a0)
lw	$a2, %lo(Pattern_Code_Ptr)($t5)		# Restore Pattern Code Address
lw	$a3, %lo(Pattern_Mask_Ptr)($t5)		# Restore Pattern Mask Address

OnTheFlyPatcher_InnerLoop:
lw	$t0, 0($a0)				# $t0 = current scan value
lw	$t1, 0($a2)				# $t1 = current pattern value
lw	$t2, 0($a3)				# $t2 = current pattern mask value

beq	$v0, $t1, OnTheFlyPatcher_InnerLoop_Exit	# continue inner comparison until matched all whole thing
nop

and	$v1, $t0, $t2			# $v1 = current scan masked value
and	$t3, $t1, $t2			# $t3 = current pattern masked value
bne	$v1, $t3, OnTheFlyPatcher_OuterLoop	# stop inner comparison in case of the [current scan masked value] not be equal to the [current pattern masked value]
nop

addu	$a0, $a0, 4					# increase scan address
addu	$a2, $a2, 4					# increase pattern address
addu	$a3, $a3, 4					# increase pattern mask address
beqzl	$zero, OnTheFlyPatcher_InnerLoop	# continue inner comparison until matched all whole thing
nop

OnTheFlyPatcher_InnerLoop_Exit:

# PATCH THE ORIGINAL CODE

or	$a0,$zero,$t4					# $a0 = restore current scan address (from temporary $t4) = target address
lw	$a1, %lo(Replacement_Code_Ptr)($t5)	# Restore Replacement Code Address

OnTheFlyPatcher_Looping:
lw     $v1, 0($a1)					# $v1 = current replacement code value

beq	$v0, $v1, OnTheFlyPatcher_End			# continue replacing until the whole thing be done
nop

addiu   $a1, $a1, 4					# increase current replacement code address
sw      $v1, 0($a0)					# store the [current replacement code value] ($v1) into the [current target address] ($a0) 
addiu   $a0, $a0, 4					# increase current target address
beqzl    $zero, OnTheFlyPatcher_Looping			# continue replacing until the whole thing be done
nop

OnTheFlyPatcher_End:

lui	$a0,(KSEG >> 16)		# Base Address
lq	$t5, 0xD0($a0)			# t5
lq	$t4, 0xC0($a0)			# t4
lq	$t3, 0xB0($a0)			# t3
lq	$t2, 0xA0($a0)			# t2
lq	$t1, 0x90($a0)			# t1
lq	$t0, 0x80($a0)			# t0
lq	$v1, 0x30($a0)			# v1
lq	$v0, 0x20($a0)			# v0

jr $ra
nop

.end OnTheFlyPatcher


# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#####################################################
# Read-Only Data
#####################################################
.section .rodata

#
# Opcode emulation jump tables
#
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

#
# sceMpegIsEnd pattern, extracted from "Woody Woodpecker: Escape from Buzz Buzzard Park (SLES-50613 PAL)"
#
.p2align 4	# align 16 bytes (IMPORTANT!!!)
Pattern_Code_sceMpegIsEnd:
.word 0x8c830040 # lw	$v1, $0040($a0)
.word 0x03e00008 # jr	$ra            
.word 0x8c620000 # lw	$v0, 0($v1)    
.word 0xFFFFFFFF # Ending (Wild Card) Value
Pattern_Mask_sceMpegIsEnd:						
.word 0xFFFFFFFF
.word 0xFFFFFFFF
.word 0xFFFFFFFF
Replacement_Code_sceMpegIsEnd:
.word 0x8c830040 # lw	$v1, $0040($a0)
.word 0x03e00008 # jr	$ra            
.word 0x24020001 # addiu 	$v0, $zero, 1 <- HERE!!!
.word 0xFFFFFFFF # Ending (Wild Card) Value

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#####################################################
# Uninitialized Variables section
#####################################################

.section .bss

.globl MIPS_Regs
MIPS_Regs:			.space	0x200

.globl Old_SetGsCrt
Old_SetGsCrt:		.word 0

.globl Source_INTERLACE
Source_INTERLACE:	.word 0
.globl Source_MODE
Source_MODE:		.word 0
.globl Source_FFMD
Source_FFMD:		.word 0

.globl Source_SMODE2
Source_SMODE2:		.dword 0
.globl Source_DISPLAY1
Source_DISPLAY1:		.dword 0
.globl Source_DISPLAY2
Source_DISPLAY2:		.dword 0
.globl Source_SYNCV
Source_SYNCV:		.dword 0

.globl DOUBLE_HEIGHT_adaptation
DOUBLE_HEIGHT_adaptation:		.byte 0	# Flag -> Height Doubling at INT&FFMD

.globl SMODE2_adaptation
SMODE2_adaptation:			.byte 0	# Flag -> Adapted SMODE2 patch value

.p2align 4	# align 16 bytes (IMPORTANT!!!)

.globl Target_INTERLACE
Target_INTERLACE:	.word 0
.globl Target_MODE
Target_MODE:		.word 0
.globl Target_FFMD
Target_FFMD:		.word 0

.globl Target_SMODE2
Target_SMODE2:		.dword 0
.globl Target_DISPLAY1
Target_DISPLAY1:		.dword 0
.globl Target_DISPLAY2
Target_DISPLAY2:		.dword 0
.globl Target_SYNCV
Target_SYNCV:		.dword 0

.globl automatic_adaptation
automatic_adaptation:	.byte 0			# Byte flag
.globl DISPLAY_fix
DISPLAY_fix:		.byte 0			# Byte flag
.globl SMODE2_fix
SMODE2_fix:		.byte 0			# Byte flag
.globl SYNCV_fix
SYNCV_fix:		.byte 0			# Byte flag

.globl skip_videos_fix
skip_videos_fix:	.byte 0			# Byte flag

.p2align 4	# align 16 bytes (IMPORTANT!!!)

.globl X_offset
X_offset:		.word 0			# X-axis offset
.globl Y_offset
Y_offset:		.word 0			# Y-axis offset

.p2align 4	# align 16 bytes (IMPORTANT!!!)

.globl Adapted_DISPLAY1
Adapted_DISPLAY1:	.dword 0
.globl Adapted_DISPLAY2
Adapted_DISPLAY2:	.dword 0

.globl Pattern_Code_Ptr
Pattern_Code_Ptr:	.word 0			# Pointer to Pattern Code

.globl Pattern_Mask_Ptr
Pattern_Mask_Ptr:	.word 0			# Pointer to Pattern Mask Address

.globl Replacement_Code_Ptr
Replacement_Code_Ptr:	.word 0			# Pointer to Replacement Code Address

.set pop
