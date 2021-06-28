#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
# -------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013, 2014, 2015, 2016 doctorxyz
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#

#include <ee_cop0_defs.h>
#include <syscallnr.h>

#define ABI_EABI64 // force all register names to EABI64 (legacy toolchain)
#include "as_reg_compat.h"

#include "gsm_defines.h"

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

.set push

.set noreorder  # disable assembler reorder mode, so the code will not be optimized or changed in any way, giving complete instruction order control to the programmer
.set noat       # disable assembler from using register $1 (known as the assembler temporary, or $at register) to hold intermediate values when performing macro expansions
.set macro      # disable warning if any statement expands to more than one machine instruction
# .set nomacro   # enable warning if any statement expands to more than one machine instruction

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#####################################################
# data section
#####################################################

.p2align 4                                          # align 16 bytes (IMPORTANT!!!)
.data

############
# Variables
############


.globl MIPS_Regs
MIPS_Regs:          .space 0x220

.globl Old_SetGsCrt
Old_SetGsCrt:       .word  0

.globl GSMSourceSetGsCrt
GSMSourceSetGsCrt:  .space 6

.globl GSMDestSetGsCrt
GSMDestSetGsCrt:    .space 6

.p2align 4                                          # align 16 bytes (IMPORTANT!!!)

.globl GSMSourceGSRegs
GSMSourceGSRegs:    .space 88

.p2align 4                                          # align 16 bytes (IMPORTANT!!!)

.globl GSMDestGSRegs
GSMDestGSRegs:      .space 88

.globl GSMFlags
GSMFlags: .space 19

.p2align 4                                          # align 16 bytes (IMPORTANT!!!)

GSMAdapts:          .space 18

.p2align 4                                          # align 16 bytes (IMPORTANT!!!)

###############################
# Opcode emulation jump tables
###############################
#
# They differentiate between different kinds of access that may have triggered the debug trap we use.
# This way the number of cases does not affect the time delay for testing.

# First we have a table with jump offsets for opcode dependency
op_t:
.word ignore, ignore, ignore, ignore                # 00-03
.word ignore, ignore, ignore, ignore                # 04-07
.word ignore, ignore, ignore, ignore                # 08-0B
.word ignore, ignore, ignore, ignore                # 0C-0F
.word ignore, ignore, ignore, ignore                # 10-13
.word ignore, ignore, ignore, ignore                # 14-17
.word ignore, ignore, ldl_op, ldr_op                # 18-1B
.word ignore, ignore, lq_op, sq_op                  # 1C-1F
.word lb_op, lh_op, lwl_op, lw_op                   # 20-23
.word lbu_op, lhu_op, lwr_op, lwu_op                # 24-27
.word sb_op, sh_op, swl_op, sw_op                   # 28-2B
.word sdl_op, sdr_op, swr_op, ignore                # 2C-2F
.word ignore, ignore, ignore, ignore                # 30-33
.word ignore, ignore, ignore, ld_op                 # 34-37
.word ignore, ignore, ignore, ignore                # 38-3B
.word ignore, ignore, ignore, sd_op                 # 3C-3F

# Table 1 for branch opcodes when trapping branch delay slot
BD_t1:
.word B_com0, B_com1, B_J, B_JAL                    # 00-03
.word B_BEQ, B_BNE, B_BLEZ, B_BGTZ                  # 04-07
.word B_skip, B_skip, B_skip, B_skip                # 08-0B
.word B_skip, B_skip, B_skip, B_skip                # 0C-0F
.word B_BC0x, B_BC1x, B_skip, B_skip                # 10-13
.word B_BEQL, B_BNEL, B_BLEZL, B_BGTZL              # 14-17
.word B_skip, B_skip, B_skip, B_skip                # 18-1B
.word B_skip, B_skip, B_skip, B_skip                # 1C-1F
.word B_skip, B_skip, B_skip, B_skip                # 20-23
.word B_skip, B_skip, B_skip, B_skip                # 24-27
.word B_skip, B_skip, B_skip, B_skip                # 28-2B
.word B_skip, B_skip, B_skip, B_skip                # 2C-2F
.word B_skip, B_skip, B_skip, B_skip                # 30-33
.word B_skip, B_skip, B_skip, B_skip                # 34-37
.word B_skip, B_skip, B_skip, B_skip                # 38-3B
.word B_skip, B_skip, B_skip, B_skip                # 3C-3F

# Table 2 for branch sub-opcodes when trapping branch delay slot
BD_t2:
.word B_BLTZ, B_BGEZ, B_BLTZL, B_BGEZL              # 00-03
.word B_skip, B_skip, B_skip, B_skip                # 04-07
.word B_skip, B_skip, B_skip, B_skip                # 08-0B
.word B_skip, B_skip, B_skip, B_skip                # 0C-0F
.word B_BLTZAL, B_BGEZAL, B_BLTZALL, B_BGEZALL      # 10-13
.word B_skip, B_skip, B_skip, B_skip                # 14-17
.word B_skip, B_skip, B_skip, B_skip                # 18-1B
.word B_skip, B_skip, B_skip, B_skip                # 1C-1F

#####################################################
# .text section
#####################################################

.p2align 4                                          # align 16 bytes (IMPORTANT!!!)
.text

############
# Functions
############

#
# Hook_SetGsCrt:
#
.globl  Hook_SetGsCrt
.ent    Hook_SetGsCrt
Hook_SetGsCrt:
    addiu   $sp, $sp, -0x0010                       # reserve 16 bytes stack space (1 reg)
    sd      $ra, 0($sp)                             # Push return address on stack

    la      $v0, GSMSourceSetGsCrt
    sh      $a0, Source_INT($v0)
    sh      $a1, Source_MODE($v0)
    sh      $a2, Source_FFMD($v0)

# -----------------------------------------------------------------------------------------

    la      $v0, GSMDestSetGsCrt
    lh      $a0, Target_INT($v0)
    lh      $a1, Target_MODE($v0)
    lh      $a2, Target_FFMD($v0)
    sync.l                                          # Wait for any outstanding stores to registers to complete before calling SetGsCrt. A lot of games were made compatible because of this.

# -----------------------------------------------------------------------------------------

/*
 * This function will get called first, in place of the original SetGsCrt() syscall handler. It will check the video mode argument.
 * If it's one of the extended video modes, do not call the original SetGsCrt() syscall handler,
 * but handle GS configuration on our own.
 * Otherwise, call the original SetGsCrt() syscall handler and let it configure the GS for us.
 */

    li      $a3, GS_MODE_DTV_576P
    la      $t0, GSMFlags
    bne     $a1, $a3, Call_SetGsCrt
    lb      $v1, gs576P_param($t0)                  # v0 = 576P_parm
    andi    $v0, $v1, 2
    beqz    $v0, Call_SetGsCrt                      # If the console supports 576P, use SetGsCrt.
    andi    $a3, $v1, 1                             # GCONT setting.

# -----------------------------------------------------------------------------------------

    jal     DTV_576P
    nop

b Skip_Call_Original_SetGsCrt
nop

# -----------------------------------------------------------------------------------------


Call_SetGsCrt:
    lw      $a3, Old_SetGsCrt                       # a3 -> original SetGsCrt function
    jalr    $a3                                     # Call original SetGsCrt
    nop

# -----------------------------------------------------------------------------------------
Skip_Call_Original_SetGsCrt:

/*
 * Re-enable GSHandler whenever Hook_SetGsCrt is called
 * This aggressive approach is needed for those titles which disable breakpoints
 * For instance: UP (SLUS 21864)
 * -----------------------------------------------------------------------------------------
 */
    jal Enable_GSBreakpoint
    nop

# -----------------------------------------------------------------------------------------

    ld      $ra, 0($sp)                             # Pull return address from stack
    jr      $ra                                     # Return to caller
    addiu   $sp, $sp, 0x0010                        # Restore sp during return

.end  Hook_SetGsCrt

#
# GSHandler:
# When the processor takes a level 2 exception, the processor switches to
# the kernel mode, by setting Status.ERL to 1.
#
# Some parts has been taken from Toshiba's TX System RISC TX79 Core Architecture and PS2 Zone Loader's PNSelector
#
# $t0 = insn at ErrorEPC
# $t1 = rt
# $t2 = value in rt
# $t3 = base
# $t4 = address
# $t5 = address holding rt within array
# $t6 = address holding base within array
# $t7 = new value for rt
#

.globl GSHandler
.ent   GSHandler
GSHandler:
# $k1 is used to save the context by the Level 2 exception handler. $k1 is reserved for kernel usage in interrupt and trap handling.
    sq      $k1, -0x20($zero)                       # Store $k1 register in the same place as the kernel does, via kseg3.
    la      $k1, MIPS_Regs                          # Store MIPS_Regs via $k0
    # sq      $zero, 0x000($k1)
    sq      $at, 0x010($k1)
    sq      $v0, 0x020($k1)
    sq      $v1, 0x030($k1)
    sq      $a0, 0x040($k1)
    sq      $a1, 0x050($k1)
    sq      $a2, 0x060($k1)
    sq      $a3, 0x070($k1)
    sq      $t0, 0x080($k1)
    sq      $t1, 0x090($k1)
    sq      $t2, 0x0A0($k1)
    sq      $t3, 0x0B0($k1)
    sq      $t4, 0x0C0($k1)
    sq      $t5, 0x0D0($k1)
    sq      $t6, 0x0E0($k1)
    sq      $t7, 0x0F0($k1)
    sq      $s0, 0x100($k1)
    sq      $s1, 0x110($k1)
    sq      $s2, 0x120($k1)
    sq      $s3, 0x130($k1)
    sq      $s4, 0x140($k1)
    sq      $s5, 0x150($k1)
    sq      $s6, 0x160($k1)
    sq      $s7, 0x170($k1)
    sq      $t8, 0x180($k1)
    sq      $t9, 0x190($k1)
    sq      $k0, 0x1A0($k1)
    sq      $gp, 0x1C0($k1)
    sq      $sp, 0x1D0($k1)
    sq      $fp, 0x1E0($k1)
    sq      $ra, 0x1F0($k1)
    pmfhi   $v0
    sq      $v0, 0x200($k1)
    pmflo   $v0
    sq      $v0, 0x210($k1)
    lq      $v0, -0x20($zero)
    sq      $v0, 0x1B0($k1)                         # $k1 can now be saved properly.

#
# The read/write ErrorEPC register holds the virtual address at which instruction
# processing can resume after servicing an error. This address can be:
# - The virtual address of the instruction that caused the exception
# - The virtual address of the immediately preceding branch or jump instruction
# (when the instruction is in a branch delay slot, and the BD2 bit in the Cause
# register is set).
#
    mfc0    $k1, $13                                # k1 = Cause bits of last exception (COP0 reg 13)
    srl     $k1, 30                                 # k1 is aligned for BD2 (Flags branch delay slot used)
                                                    # 1 -> delay slot, 0 -> normal
    andi    $k1, 1                                  # k1 = BD2
    sll     $k1, 2                                  # k1 = BD2*4
    mfc0    $k0, $30                                # k0 = ErrorPC (COP0 reg 30) -> MIPS instruction
    addu    $k0, $k1                                # Add 4 to opcode address for Branch Delay Slot
                                                    # Next get rt (target register) and effective address
                                                    # but first check that the instruction is one we patch
    lw      $v0, 0($k0)                             # v0 = MIPS instruction that caused trap

    srl     $v1, $v0, 26                            # v1 = opcode (range 0x00-0x3F)
    andi    $v1, $v1, 0x003F                        # v1 = pure opcode number
    sll     $v1, $v1, 2                             # v1 = op_num*2 (word offset for jump table)

    la      $s0, GSMSourceGSRegs
    la      $s1, GSMDestGSRegs
    la      $s2, GSMFlags
    la      $s3, GSMAdapts
    li      $s4, GS_BASE

    la      $a2, MIPS_Regs                          # Locate MIPS_Regs via $a2
    la      $a3, op_t                               # a3 -> op_t

    addu    $a0, $v1, $a3                           # a0 -> active entry in op_t
    lw      $a1, 0($a0)                             # a1 = opcode handler from op_t

    jr      $a1                                     # jump to separate opcode handlers
    nop                                             # with v0=instruction, a2->KSeg, a3->op_t

get_access_info:
    srl     $a1, $v0, 16
    andi    $a1, $a1, 0x1f                          # a1 = unscaled rt reg index
    srl     $a0, $v0, 21
    andi    $a0, $a0, 0x1f                          # a0 = unscaled base reg index

    sll     $a3, $a0, 4                             # a3 = raw base_ix << 4 (scaled base_ix reg index)
    addu    $a3, $a2, $a3                           # a3 = &MIPS_Regs[base_ix]; (if type = u128)
    lw      $a3, 0($a3)                             # a3 = base register value
    andi    $v0, $v0, 0xFFFF                        # v0 = offset field of instruction
    addu    $a3, $a3, $v0                           # a3 = address which triggered breakpoint

    sll     $v0, $a1, 4                             # v0 = raw rt_ix << 4 (scaled rt_ix reg index)
    addu    $a1, $a2, $v0                           # a1 = &MIPS_Regs[rt_ix];

#  NB: The trapping method forces us to trap some GS registers we don't want.
#      It is crucial that the writing of those registers proceeds undisturbed.
#      This is handled by the final test case below, at label "not_wanted_reg".

#  Here a1=pointer to entry for rt reg, a2->.gsm_engine, a3=dest_address
#  NB: Since address is changed to offset by ANDI, it is valid for all segments
#  NB: We avoid masking a3 itself though, in case this is an unwanted register
#  NB: Remasking for KSEG1 should be done in each handler for wanted registers

    jr      $ra
    andi    $v0, $a3, 0xFFFF                        # v0 = dest offset from GS_BASE

lb_op:
    b       have_some_read
    addu    $k0, $zero, $zero
lbu_op:
    b       have_some_read
    addiu   $k0, $zero, 1
ldl_op:
ldr_op:
ld_op:
    b       have_some_read
    addiu   $k0, $zero, 6
lq_op:
    b       have_some_read
    addiu   $k0, $zero, 7
lh_op:
    b       have_some_read
    addiu   $k0, $zero, 2
lhu_op:
    b       have_some_read
    addiu   $k0, $zero, 3
lwl_op:
lwr_op:
lw_op:
    b       have_some_read
    addiu   $k0, $zero, 4
lwu_op:
    b       have_some_read
    addiu   $k0, $zero, 5

have_some_read:                                     # Opcode is a read, so we must check further
    jal     get_access_info
    nop

    addi    $v1, $v0, -GS_CSR
    beqz    $v1, have_CSR_read                      # in case of  src == GS_reg_CSR
    nop

not_wanted_reg_read:                                # Register unwanted, so perform op unchanged
    ld      $v0, 0($a3)

load_new_value:
    beqz    $k0, do_load_byte                       # Byte
    addiu   $k0, $k0, -1
    beqz    $k0, do_load_ubyte                      # Unsigned Byte
    addiu   $k0, $k0, -1
    beqz    $k0, do_load_half                       # Halfword
    addiu   $k0, $k0, -1
    beqz    $k0, do_load_uhalf                      # Unsigned Halfword
    addiu   $k0, $k0, -1
    beqz    $k0, do_load_word                       # Word
    addiu   $k0, $k0, -1
    beqz    $k0, do_load_uword                      # Unsigned Word
    nop                                             # Fall through for Doubleword & Quadword (Upper 64 bits ignored)

# Store data to destination
do_load:
    b       exit_GSHandler
    sd      $v0, 0($a1)

do_load_byte:
    sll     $v0, $v0, 24
    b       do_load
    sra     $v0, $v0, 24

do_load_ubyte:
    sll     $v0, $v0, 24
    b       do_load
    srl     $v0, $v0, 24

do_load_half:
    sll     $v0, $v0, 16
    b       do_load
    sra     $v0, $v0, 16

do_load_uhalf:
    sll     $v0, $v0, 16
    b       do_load
    srl     $v0, $v0, 16

do_load_word:
    b       do_load
    sll     $v0, $v0, 0                             # truncated to 32-bits and sign-extended to 64.

do_load_uword:
    dsll32  $v0, $v0, 0
    b       do_load
    dsrl32  $v0, $v0, 0

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
have_some_write:                                    # Opcode is a write, so we must check further

    jal     get_access_info
    nop
    ld      $a1, 0($a1)                             # a1 = value in rt

    addi    $v1, $v0, -GS_PMODE
    beqz    $v1, have_PMODE_write                   # in case of  dest == GS_reg_PMODE
    addi    $v1, $v0, -GS_SMODE1
    beqz    $v1, have_SMODE1_write                  # in case of  dest == GS_reg_SMODE1
    addi    $v1, $v0, -GS_SMODE2
    beqz    $v1, have_SMODE2_write                  # in case of  dest == GS_reg_SMODE2
    addi    $v1, $v0, -GS_SRFSH
    beqz    $v1, have_SRFSH_write                   # in case of  dest == GS_reg_SRFSH
    addi    $v1, $v0, -GS_SYNCH1
    beqz    $v1, have_SYNCH1_write                  # in case of  dest == GS_reg_SYNCH1
    addi    $v1, $v0, -GS_SYNCH2
    beqz    $v1, have_SYNCH2_write                  # in case of  dest == GS_reg_SYNCH2
    addi    $v1, $v0, -GS_SYNCV
    beqz    $v1, have_SYNCV_write                   # in case of  dest == GS_reg_SYNCV
    addi    $v1, $v0, -GS_DISPFB1
    beqz    $v1, have_DISPFB1_write                 # in case of  dest == GS_reg_DISPFB1
    addi    $v1, $v0, -GS_DISPFB2
    beqz    $v1, have_DISPFB2_write                 # in case of  dest == GS_reg_DISPFB2
    addi    $v1, $v0, -GS_DISPLAY1
    beqz    $v1, have_DISPLAY1_write                # in case of  dest == GS_reg_DISPLAY1
    addi    $v1, $v0, -GS_DISPLAY2
    beqz    $v1, have_DISPLAY2_write                # in case of  dest == GS_reg_DISPLAY2
    nop

not_wanted_reg:                                     # Register unwanted, so perform op unchanged
    b       exit_GSHandler
    sd      $a1, 0($a3)                             # Store source data unchanged to destination

ignore:                                             # We just ignore weird opcodes that we don't implement
    b       exit_GSHandler
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
    sd      $a1, Source_PMODE($s0)                  # Source_PMODE = a1
    lb      $v0, PMODE_fix($s2)                     # v0 = PMODE_fix
    beql    $v0, $zero, store_v0_as_PMODE           # in case of PMODE_fix is disabled
    or      $v0, $zero, $a1                         #   go use Source_PMODE
    ld      $v0, Target_PMODE($s1)                  # v0 = Target_PMODE
    beql    $v0, $zero, store_v0_as_PMODE           # in case of  Target_PMODE is zero
    or      $v0, $zero, $a1                         #   go use Source_PMODE

store_v0_as_PMODE:
    b       exit_GSHandler                          # Now go exit
    sd      $v0, 0($a3)                             # after storing

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
    sd      $a1, Source_SMODE2($s0)                 # Source_SMODE2 = a1
    lb      $v0, SMODE2_fix($s2)                    # v0 = SMODE2_fix
    beql    $v0, $zero, store_v0_as_SMODE2          # in case of  Separate SMODE2 fix disabled
    or      $v0, $zero, $a1                         #   go use Source_SMODE2

    srl     $v0, $a1, 1                             # v0 = a1 aligned for FFMD in bit0
    and     $v0, $v0, $a1                           # v0 bit 0 = INT & FFMD
    andi    $v0, $v0, 1                             # v0 bit 0 = INT & FFMD isolated
    sb      $v0, Interlace_FRAME_Mode_Flag($s3)     # store Interlace FRAME Mode Flag
    beqz    $v0, store_v0_as_SMODE2                 # in case of  no Double Height need
    ld      $v0, Target_SMODE2($s1)                 #   go use Target_SMODE2 as adapted SMode2
                                                    # otherwise just set v0 = Target_SMODE2
    andi    $a1, $a1, 2                             # a1 = FFMD of Source_SMODE2
    andi    $v0, $v0, 0xFFFD                        # v0 = Target_SMODE2 without FFMD
    or      $v0, $v0, $a1                           # v0 = Target_SMODE2 + Source FFMD

store_v0_as_SMODE2:
    b       exit_GSHandler                          # Now go exit
    sd      $v0, 0($a3)                             # after storing

# ----------------------------
# DISPFB1
#
have_DISPFB1_write:
    sd      $a1, Source_DISPFB1($s0)                # Source_DISPFB1 = a1

jal have_DISPFB_write
    ld      $v0, Target_DISPFB1($s1)                # v0 = Target_DISPFB1

    b       exit_GSHandler                          # Now go exit
    sd      $v0, 0($a3)                             # after storing

# ----------------------------
# DISPFB2
#
have_DISPFB2_write:
    sd      $a1, Source_DISPFB2($s0)                # Source_DISPFB2 = a1

jal have_DISPFB_write
    ld      $v0, Target_DISPFB2($s1)                # v0 = Target_DISPFB2

    b       exit_GSHandler                          # Now go exit
    sd      $v0, 0($a3)                             # after storing

# ----------------------------
# DISPLAY1
#
have_DISPLAY1_write:                                # Here a1=source_data, a2->.gsm_engine, a3=dest_adress
    sd      $a1, Source_DISPLAY1($s0)               # request DISPLAY1 value = a1

    jal     have_DISPLAY_write
    ld      $v1, Target_DISPLAY1($s1)               # v1=forcing DISPLAY1 template

    sd      $v0, Adapted_DISPLAY1($s3)              # Store new DISPLAY1 value (for feedback)

    lb      $v1, DISPLAY_fix($s2)                   # v1 = DISPLAY_fix
    beql    $v1, $zero, store_v0_as_DISPLAY1        # in case of (DISPLAY_fix)
    ld      $v0, Target_DISPLAY1($s1)               #   use forced mode without adaption

store_v0_as_DISPLAY1:
    b       exit_GSHandler                          # Now go exit
    sd      $v0, 0($a3)                             # after storing

# ----------------------------
# DISPLAY2
#
have_DISPLAY2_write:                                # Here a1=source_data, a2->.gsm_engine, a3=dest_adress
    sd      $a1, Source_DISPLAY2($s0)               # request DISPLAY2 value = a1

    jal     have_DISPLAY_write
    ld      $v1, Target_DISPLAY2($s1)               # v1=forcing DISPLAY2 template

    sd      $v0, Adapted_DISPLAY2($s3)              # Store new DISPLAY2 value (for feedback)

    lb      $v1, DISPLAY_fix($s2)                   # v1 = DISPLAY_fix
    beql    $v1, $zero, store_v0_as_DISPLAY2        # in case of (DISPLAY_fix)
    ld      $v0, Target_DISPLAY2($s1)               #   use forced mode without adaption

store_v0_as_DISPLAY2:
    b       exit_GSHandler                          # Now go exit
    sd      $v0, 0($a3)                             # after storing

# ----------------------------
# DISPLAYx
#
have_DISPLAY_write:                                 # Here a1=source_data, a2->.gsm_engine, a3=dest_adress
#  Source_DISPLAY == Requested   DX, DY, MAGH, MAGV, DW and DH values
#  Target_DISPLAY == Modded(forced) DX, DY, MAGH, MAGV, DW and DH values
#  Both are 64 bit units with encoded bit fields like GS DISPLAY registers

#  Patch to adapt request to enforced mode in v1 MUST preserve a1, a2, a3

    lb      $v0, ADAPTATION_fix($s2)
    beql    $v0, $zero, have_DISPLAY_write_1        # in case of (ADAPTATION_fix)
    or      $a1, $zero, $v1                         #   simulate request same as forced mode

have_DISPLAY_write_1:
    li      $v0, 0                                  # preclear v0 as result DISPLAY accumulator

#  Here a0=free, a1=Source_DISPLAY, a2->.gsm_engine, a3=dest_address
#  Also v0=result_accumulator, v1=Target_DISPLAY, t0-t7=free

/*
 * Automatic adaptation formulas
 */
# ----- HORIZONTAL FIELDS -----
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | MAGH | 26:23 | int 0: 4:0 | magnification in horizontal direction | 0xF   |
# '------^-------^------------^---------------------------------------^-------^
    dsrl    $t0, $a1, 23
    andi    $t0, $t0, 0x0F
    addi    $t0, $t0, 1                             # t0 = Source_Width_Scale = Source_MAGH + 1
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DW   | 43:32 | int 0:12:0 | display area width - 1 (VCK units)    | 0xFFF |
# '------^-------^------------^---------------------------------------^-------^
    dsrl32  $t1, $a1, 0
    andi    $t1, $t1, 0x0FFF
    addi    $t1, $t1, 1                             # t1 = Source_Width = Source_DW + 1
    divu    $t1, $t0                                # LO = Source_Pixels_Width = Source_Width / Source_Width_Scale
    dsrl32  $t0, $v1, 0
    andi    $t5, $t0, 0x0FFF
    mflo    $t4                                     # t4 = LO = Source_Pixels_Width
    addi    $t6, $t5, 1                             # t6 = Target_Width = Target_DW + 1
    divu    $t6, $t4                                # LO = Target_Width_Scale = Target_Width / Source_Pixels_Width
    mflo    $t0                                     # t0 = LO = Target_Width_Scale
    bne     $t0, $zero, have_DISPLAY_write_2        # in case of (!Target_Width_Scale)
    nop                                             # {
    or      $t7, $zero, $t5                         #   t7 = Target_DW = Target_Width - 1
    sub     $t0, $t6, $t4                           #   t0 = Target_Width - Source_Pixels_Width
    li      $t4, 0                                  #   t4 = Target_MAGH = 0
    beql    $zero, $zero, have_DISPLAY_write_4      # }
    nop                                             # otherwise
                                                    #   Target_Width_Scale nonzero

have_DISPLAY_write_2:                               # {
    addi    $t1, $t0, -16                           #   t1 = Target_Width_Scale - 16
    bgtzl   $t1, have_DISPLAY_write_3               #   in case of (Target_Width_Scale > 16)
    or      $t0, $zero, 16                          #     t0 = Target_Width_Scale = 16;

have_DISPLAY_write_3:
    mult    $t1, $t4, $t0                           #   LO = Calculated_Width = (Source_Pixels_Width * Target_Width_Scale)
    addi    $t7, $t1, -1                            #   t7 = Calculated_DW = Calculated_Width - 1
    addi    $t4, $t0, -1                            #   t4 = Calculated_MAGH = Target_Width_Scale - 1
    sub     $t0, $t5, $t7                           #   t0 = Target_DW - Calculated_DW

have_DISPLAY_write_4:                               # }
    dsra    $t0, $t0, 1                             # t0 = t0 / 2 = Half_Excess_Width (can be negative)
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DX   | 11:0  | int 0:12:0 | x pos in display area (VCK units)     | 0xFFF |
# '------^-------^------------^---------------------------------------^-------^
    andi    $t1, $v1, 0x0FFF                        # t1 = Target_DX
    add     $t6, $t0, $t1                           # t6 = Calculated_DX = Target_DX + Half_Excess_Width
    bltzl   $t6, have_DISPLAY_write_5               # in case of (Calculated_DX < 0)
    and     $t6, $t6, $zero                         #   Calculated_DX = 0;

have_DISPLAY_write_5:
    sub     $t0, $t1, $t6                           # t0 = Target_DX - Calculated_DX
    bgtzl   $t0, have_DISPLAY_write_6               # in case of (Target_DX > Calculated_DX)
    add     $t7, $t7, $t0                           #   t7 = Calculated_DW = Calculated_DW + Target_DX - Calculated_DX  # Target DW adjusted

have_DISPLAY_write_6:
    andi    $t7, $t7, 0x0FFF
    andi    $t4, $t4, 0x000F
    andi    $t6, $t6, 0x0FFF
    dsll32  $t0, $t7, 0                             # t0 = Calculated_DW
    or      $v0, $v0, $t0                           # v0 = Adapted_DISPLAY = (Adapted_DISPLAY) OR (Calculated_DW)
    dsll    $t0, $t4, 23
    or      $v0, $v0, $t0                           # v0 = Adapted_DISPLAY = (Adapted_DISPLAY) OR (Calculated_MAGH)

    lw      $t1, X_offset($s2)                      # t1 = X_offset (signed)
    add     $t6, $t6, $t1                           # t6 = Calculated_DX = Calculated_DX + X_offset
    bltzl   $t6, have_DISPLAY_write_7               # Is the result not greater or equal to zero?
    move    $t6, $zero                              # t6 = Calculated_DX = 0
have_DISPLAY_write_7:
    andi    $t6, $t6, 0x0FFF

    or      $v0, $v0, $t6                           # v0 = Adapted_DISPLAY = (Adapted_DISPLAY) OR (Calculated_DX)

# ----- VERTICAL FIELDS -------
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | MAGV | 28:27 | int 0: 2:0 | magnification in vertical direction   | 0x3   |
# '------^-------^------------^---------------------------------------^-------^
    dsrl    $t0, $a1, 27
    andi    $t0, $t0, 0x03
    addi    $t0, $t0, 1                             # t0= Source_Height_Scale = Source_MAGV + 1
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DH   | 54:44 | int 0:11:0 | display area height - 1 (pixel units) | 0x7FF |
# '------^-------^------------^---------------------------------------^-------^
    dsrl32  $t1, $a1, 12
    andi    $t1, $t1, 0x07FF
    addi    $t1, $t1, 1                             # Source_Height = Source_DH + 1
    divu    $t1, $t0                                # LO = Source_Pixels_Height = Source_Height / Source_Height_Scale
    dsrl32  $t0, $v1, 12
    andi    $t5, $t0, 0x07FF
    mflo    $t4                                     # t4 = LO = Source_Pixels_Height
    addi    $t6, $t5, 1                             # t6 = Target_Height = Target_DH + 1
    divu    $t6, $t4                                # LO = Target_Height_Scale = Target_Height / Source_Pixels_Height
    mflo    $t0                                     # t0 = LO = Target_Height_Scale
    bne     $t0, $zero, have_DISPLAY_write_8        # in case of (!Target_Height_Scale)
    nop                                             # {
    or      $t7, $zero, $t5                         #   t7 = Target_DH = Target_Height - 1
    sub     $t0, $t6, $t4                           #   t0 = Target_Height - Source_Pixels_Height
    b       have_DISPLAY_write_10                   # }
    move    $t4, $zero                              # t4 = Target_MAGV = 0
                                                    # otherwise, Target_Height_Scale nonzero

have_DISPLAY_write_8:                               # {
    addi    $t1, $t0, -4                            #   t1 = Target_Height_Scale - 4
    bgtzl   $t1, have_DISPLAY_write_9               #   in case of (Target_Height_Scale > 4)
    or      $t0, $zero, 4                           #     t0 = Target_Height_Scale = 4;

have_DISPLAY_write_9:
    mult    $t1, $t4, $t0                           #   t1 = Calculated_Height = (Source_Pixels_Height * Target_Height_Scale)
    addi    $t7, $t1, -1                            #   t7 = Calculated_DH = Calculated_Height - 1
    addi    $t4, $t0, -1                            #   t4 = Calculated_MAGV = Target_Height_Scale - 1
    sub     $t0, $t5, $t7                           #   t0 = Target_DH - Calculated_DH

have_DISPLAY_write_10:                              # }
    dsra    $t0, $t0, 1                             # t0 = t0 / 2 = Half_Excess_Height (can be negative)
# .------.-------.------------.---------------------------------------.-------.
# | Name | Pos.  | Format     | Contents                              | Mask  |
# |      |       |            |                                       |       |
# |------+-------+------------+---------------------------------------+-------+
# | DY   | 22:12 | int 0:11:0 | y pos in display area (raster units)  | 0x7FF |
# '------^-------^------------^---------------------------------------^-------^
    dsrl    $t1, $v1, 12
    andi    $t1, $t1, 0x07FF                        # t1 = Target_DY
    add     $t6, $t0, $t1                           # t6 = Calculated_DY = Target_DY + Half_Excess_Height
    bltzl   $t6, have_DISPLAY_write_11              # in case of (Calculated_DY < 0)
    and     $t6, $t6, $zero                         #   Calculated_DY = 0;

have_DISPLAY_write_11:
    sub     $t0, $t1, $t6                           # t0 = Target_DY - Calculated_DY
    bgtzl   $t0, have_DISPLAY_write_12              # in case of (Target_DY > Calculated_DY)
    add     $t7, $t7, $t0                           #   t7 = Calculated_DH = Calculated_DH + Target_DY - Calculated_DY # Target DH adjusted

have_DISPLAY_write_12:
    lb      $t0, Interlace_FRAME_Mode_Flag($s3)     # in case of Double Height not needed
    beql    $t0, $zero, have_DISPLAY_write_13       #   Calculation is complete
    nop

    ld      $t0, Target_SMODE2($s1)
    andi    $t0, $t0, 1                             # in case of Target_SMODE2.INT = 1 (Interlace Mode)
    bne     $t0, $zero, have_DISPLAY_write_13       #   Calculation is complete
    nop
    beql    $t4, $zero, have_DISPLAY_write_13       # in case of Calculated_MAGV = 0
    addi    $t4, $t4, 1                             #   go use Calculated_MAGV = Calculated_MAGV + 1
    addi    $t4, $t4, 2                             # Calculated_MAGV = Calculated_MAGV + 2 (Because scale was 2 or larger)
    addi    $t0, $t4, -4                            # Compare Calculated_MAGV with 4 (too large ?)
    bgezl   $t0, have_DISPLAY_write_13              # in case of  Calculated_MAGV too large
    ori     $t4, $zero, 3                           #   go use Calculated_MAGV = 3

have_DISPLAY_write_13:
    andi    $t7, $t7, 0x07FF
    andi    $t4, $t4, 0x0003
    andi    $t6, $t6, 0x07FF
    dsll32  $t0, $t7, 12
    or      $v0, $v0, $t0                           # v0 = Adapted_DISPLAY = (Adapted_DISPLAY) OR (Calculated_DH)
    dsll    $t0, $t4, 27
    or      $v0, $v0, $t0                           # v0 = Adapted_DISPLAY = (Adapted_DISPLAY) OR (Calculated_MAGV)

    lw      $t1, Y_offset($s2)                      # t1 = Y_offset (signed)
    add     $t6, $t6, $t1                           # t6 = Calculated_DY = Calculated_DY + Y_offset
    bltzl   $t6, have_DISPLAY_write_14              # Is the result not greater or equal to zero?
    move    $t6, $zero
have_DISPLAY_write_14:
    andi    $t6, $t6, 0x07FF

    dsll    $t0, $t6, 12

    jr      $ra
    or      $v0, $v0, $t0                           # v0 = Adapted_DISPLAY = (Adapted_DISPLAY) OR (Calculated_DY)

#  End of Patch to adapt request with the resulting request in v0

# ----------------------------
# DISPFBx
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
have_DISPFB_write:

#  Here a0=free, a1=Source_DISPFBx, a2->.gsm_engine, a3=dest_address
#  Also v0=result_accumulator, v0=Target_DISPFBx, t0-t7=free

    lb      $v1, DISPFB_fix($s2)                    # v1 = DISPFB_fix
    beql    $v1, $zero, store_v0_as_DISPFB          # in case of DISPFB_fix is disabled
    or      $v0, $zero, $a1                         #   go use v0=a1 for DISPFBx
    beql    $v0, $zero, store_v0_as_DISPFB          # in case of  Target_DISPFBx is zero
    or      $v0, $zero, $a1                         #   go use v0=a1 for DISPFBx

store_v0_as_DISPFB:
    jr      $ra
    nop

# ------------------------------------------------------------------------------------------------------

# ----------------------------
# CSR
#
have_CSR_read:                                      # Here a1=pointer to rt reg, a2->.gsm_engine, a3=source_adress, k0 = access mode

    slti    $t0, $k0, 2                             # Cannot process byte loads (necessary bits will be missing)
    bnez    $t0, not_wanted_reg_read

    lbu     $t0, Source_SMODE2($s0)                 # Only emulate if the game uses interlace, but a non-interlaced mode is selected.
    lbu     $t1, Target_SMODE2($s1)                 # Whereby source interlace != selected interlace, selected interlace == 0 (not interlace)
    xori    $t0, $t0, 1
    and     $t0, $t0, $t1
    andi    $t0, $t0, 1
    bnez    $t0, not_wanted_reg_read

# Emulate ODD/EVEN flipping of the CSR FIELD (bit 13).
    ld      $t0, GS_CSR($s4)                        # t0 = CSR. Load CSR to check on the HSINT status
    lbu     $t1, FIELD_fix($s2)                     # t1 = FIELD_fix
    andi    $t2, $t0, 4                             # Check HSINT event.
    beqz    $t2, not_wanted_reg_read                # Flip emulated FIELD status, only at the end of H-Blank.
    andi    $t2, $t1, 4                             # Check whether the FIELD fix is enabled.
    beqz    $t2, not_wanted_reg_read
    xori    $t1, $t1, 2                             # Flip counter. This counter is used to half the rate of flipping, since progressive modes have double the Horizontal frequency.
    andi    $t2, $t1, 2                             # If counter == 0,
    beqzl   $t2, skip_flip
    xori    $t1, $t1, 1                             # Flip odd/even FIELD status.
skip_flip:
    sb      $t1, FIELD_fix($s2)                     # Update status.
    andi    $t1, $t1, 1
    ori     $t0, $t0, 0x2000                        # Clear FIELD field 1 ^ 1 = always 0.
    xori    $t0, $t0, 0x2000
    sll     $t1, $t1, 13                            # Move bit into position.
    b       load_new_value
    or      $v0, $t0, $t1                           # Replace FIELD field.

exit_GSHandler:
    la      $k0, MIPS_Regs  # Restore MIPS_Regs via $k0
    # We will keep a subset of these registers (not restored here) for the remainder of this process.
    # lq      $zero, 0x000($k0)
    lq      $at, 0x010($k0)
    lq      $v0, 0x020($k0)
    lq      $v1, 0x030($k0)
    lq      $a0, 0x040($k0)
    lq      $a1, 0x050($k0)
    lq      $a2, 0x060($k0)
    lq      $a3, 0x070($k0)
    # lq      $t0, 0x080($k0)
    # lq      $t1, 0x090($k0)
    # lq      $t2, 0x0A0($k0)
    lq      $t3, 0x0B0($k0)
    lq      $t4, 0x0C0($k0)
    lq      $t5, 0x0D0($k0)
    lq      $t6, 0x0E0($k0)
    lq      $t7, 0x0F0($k0)
    lq      $s0, 0x100($k0)
    lq      $s1, 0x110($k0)
    lq      $s2, 0x120($k0)
    lq      $s3, 0x130($k0)
    lq      $s4, 0x140($k0)
    lq      $s5, 0x150($k0)
    lq      $s6, 0x160($k0)
    lq      $s7, 0x170($k0)
    lq      $t8, 0x180($k0)
    lq      $t9, 0x190($k0)
    # lq      $k0, 0x1A0($k0)
    # lq      $k1, 0x1B0($k0)
    # lq      $gp, 0x1C0($k0)
    # lq      $sp, 0x1D0($k0)
    # lq      $fp, 0x1E0($k0)
    # lq      $ra, 0x1F0($k0)
    lq      $k1, 0x200($k0)
    pmthi   $k1
    lq      $k1, 0x210($k0)
    pmtlo   $k1


# WARNING: Past this point in GSHandler, use should only $k0, $k1, $t0, $t1, $t2
# Now we will attempt to skip the current instruction, as the handler has already acted on behalf of it.
# There is no function for stepping with the hardware breakpoint. Hence there is a need to consider how PC would have been advanced by the EE.
# $t2 will contain the current ErrorEPC value.

    mfc0    $k0, $13                                # k0 = Cause of last exception
    srl     $k0, 30                                 # BD2 Flags debug exception in branch delay slot.
                                                    # 1 -> delay slot, 0 -> normal
    andi    $k0, 1                                  # k0 = BD2 bit isolated

    bnez    $k0, GSHandler_BranchDelaySlotException # Deal properly with Branch Delay Slot Exceptions (when needed)
    mfc0    $t2, $30                                # t2 = Error Exception Program Counter (ErrorEPC/EPC)

    addiu   $k0, $t2, 4                             # k0 = ErrorEPC+4 (-> next opcode)
    mtc0    $k0, $30                                # store k0 in Error Exception Program Counter
    sync.p                                          # ensure COP0 register update before proceeding. This cannot be placed in a branch-delay slot.

    b       GSHandler_Final_Exit
    nop

# ----------------------------
GSHandler_BranchDelaySlotException:
    lw      $k0, 0($t2)                             # k0 = instruction at EPC location (branch or jump)
    srl     $k1, $k0, 26                            # k1 = aligned for opcode (range 0x00-0x3F)
    andi    $k1, $k1, 0x3F                          # k1 = pure opcode number
    sll     $k1, $k1, 2                             # k1 = op_num*2 (offset for jump table)

    la      $t1, BD_t1                              # t1 -> BD_t1
    addu    $k1, $k1, $t1                           # k1 -> active entry in BD_t1
    lw      $t0, 0($k1)                             # t0 = opcode handler from BD_t1
    la      $t1, op_t                               # t1 -> op_t

    jr      $t0                                     # jump to branch/jump opcode handlers
    nop                                             # with k0=instruction, t1->op_t

# ----------------------------
B_com1:                                             # This group contains 8 different branch operations
    srl     $k1, $k0, 16                            # k1 = aligned for sub_opcode (range 0x00-0x1F)
    andi    $k1, $k1, 0x3F                          # k1 = pure sub_opcode number
    sll     $k1, $k1, 2                             # k1 = sub_op_num*2 (offset for jump table)

    la      $t1, BD_t2                              # t1 -> BD_t2
    addu    $k1, $k1, $t1                           # k1 -> active entry in BD_t2
    lw      $t0, 0($k1)                             # t0 = opcode handler from BD_t2
    la      $t1, op_t                               # t1 -> op_t

    jr      $t0                                     # jump to branch/jump opcode handlers
    nop                                             # with k0=instruction, t1->op_t

# ----------------------------
B_com0:                                             # opcode 0x00 includes both JR and JALR
    li      $t0, 0xFC1F07FF                         # t0 = bitmask for JALR
    and     $k1, $k0, $t0                           # k1 = potential JALR instruction
    li      $t0, 9                                  # t0 = JALR test constant
    beq     $k1, $t0, B_JR_JALR                     # in case of  JALR identified, go deal with it
    nop
    li      $t0, 0xFC1FFFFF                         # t0 = bitmask for JR
    and     $k1, $k0, $t0                           # k1 = potential JR instruction
    li      $t0, 8                                  # t0 = JR test constant
    bne     $k1, $t0, B_skip                        # in case of  JR not identified, go skip this code
    nop

B_JR_JALR:                                          # JR or JALR found, so make register indirect jump
    srl     $k1, $k0, 21                            # k1 = aligned for JR/JALR rs register number
    andi    $k1, 0x1F                               # k1 = register number
    sll     $t1, $k1, 4                             # t1 = array index for saved register data
    la      $t0, MIPS_Regs                          # t0 -> saved register array
    addu    $t0, $t1                                # t0 -> register data of JR/JALR rs register
    lw      $t0, 0($t0)                             # t0 = jump destination address
    mtc0    $t0, $30                                # store t0 in Error Exception Program Counter
    sync.p                                          # ensure COP0 register update before proceeding. This cannot be placed in a branch-delay slot.
    b       GSHandler_Final_Exit
    nop

# ----------------------------

B_J:
B_JAL:
# Here we have a definite jump with absolute address/4 in instruction
    li      $t0, 0x3FFFFFF                          # t0 = bitmask for jump destination bits
    and     $k1, $k0, $t0                           # k1 = destination bits (== destination/4)
    sll     $t0, $k1, 2                             # t0 = jump destination address[27:0]
    addiu   $t1, $t2, 4
    sra     $t1, $t1, 28
    sll     $t1, $t1, 28                            # Extract (EPC+4)[31:28]
    or      $t0, $t0, $t1                           # Append (EPC+4)[31:28] to instruction index.
    mtc0    $t0, $30                                # store t0 in Error Exception Program Counter
    sync.p                                          # ensure COP0 register update before proceeding. This cannot be placed in a branch-delay slot.
    b       GSHandler_Final_Exit
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
    li      $t0, 0xFFFF                             # t0 = bitmask for branch offset bits
    and     $k1, $k0, $t0                           # k1 = branch offset bits (== offset/4)
    sll     $k1, 2                                  # k1 = branch offset
    addiu   $t0, $t2, 4                             # t0 = $t2+4 (ErrorEPC+4) (-> address after branch op)
    addu    $t0, $k1                                # t0 = jump destination address
    mtc0    $t0, $30                                # store t0 in Error Exception Program Counter
    sync.p                                          # ensure COP0 register update before proceeding. This cannot be placed in a branch-delay slot.
    b       GSHandler_Final_Exit
    nop

#  ----------------------------
B_BLTZ:
B_BLTZAL:
    srl     $k1, $k0, 21                            # k1 = aligned for rs register number
    andi    $k1, 0x1F                               # k1 = register number
    sll     $t1, $k1, 4                             # t1 = array index for saved register data
    la      $t0, MIPS_Regs                          # t0 -> saved register array
    addu    $t0, $t1                                # t0 -> register data of rs register
    ld      $t1, 0($t0)                             # t1 = rs register data
    bltz    $t1, B_taken
    nop

B_not_taken:
    addiu   $k0, $t2, 8                             # k0 = $t2+8 (ErrorEPC+8) pass branch_op and delay_slot
    mtc0    $k0, $30                                # store k0 in Error Exception Program Counter
    sync.p                                          # ensure COP0 register update before proceeding. This cannot be placed in a branch-delay slot.
    b       GSHandler_Final_Exit
    nop

#  ----------------------------
B_BGEZ:
B_BGEZAL:
    srl     $k1, $k0, 21                            # k1 = aligned for rs register number
    andi    $k1, 0x1F                               # k1 = register number
    sll     $t1, $k1, 4                             # t1 = array index for saved register data
    la      $t0, MIPS_Regs                          # t0 -> saved register array
    addu    $t0, $t1                                # t0 -> register data of rs register
    ld      $t1, 0($t0)                             # t1 = rs register data
    bgez    $t1, B_taken
    nop
    b       B_not_taken
    nop

#  ----------------------------
B_BLEZ:
    srl     $k1, $k0, 21                            # k1 = aligned for rs register number
    andi    $k1, 0x1F                               # k1 = register number
    sll     $t1, $k1, 4                             # t1 = array index for saved register data
    la      $t0, MIPS_Regs                          # t0 -> saved register array
    addu    $t0, $t1                                # t0 -> register data of rs register
    ld      $t1, 0($t0)                             # t1 = rs register data
    blez    $t1, B_taken
    nop
    b       B_not_taken
    nop

#  ----------------------------
B_BGTZ:
    srl     $k1, $k0, 21                            # k1 = aligned for rs register number
    andi    $k1, 0x1F                               # k1 = register number
    sll     $t1, $k1, 4                             # t1 = array index for saved register data
    la      $t0, MIPS_Regs                          # t0 -> saved register array
    addu    $t0, $t1                                # t0 -> register data of rs register
    ld      $t1, 0($t0)                             # t1 = rs register data
    bgtz    $t1, B_taken
    nop
    b       B_not_taken
    nop

#  ----------------------------
B_BEQ:
    srl     $k1, $k0, 21                            # k1 = aligned for rs register number
    andi    $k1, 0x1F                               # k1 = rs register number
    sll     $k1, $k1, 4                             # k1 = array index for saved rs register data
    la      $t0, MIPS_Regs                          # t0 -> saved register array
    addu    $k1, $t0                                # k1 -> register data of rs register
    ld      $t1, 0($k1)                             # t1 = rs register data
    srl     $k1, $k0, 16                            # k1 = aligned for rt register number
    andi    $k1, 0x1F                               # k1 = rt register number
    sll     $k1, $k1, 4                             # k1 = array index for saved rt register data
    addu    $k1, $t0                                # k1 -> register data of rt register
    ld      $t0, 0($k1)                             # t0 = rt register data
    beq     $t0, $t1, B_taken
    nop
    b       B_not_taken
    nop

#  ----------------------------
B_BNE:
    srl     $k1, $k0, 21                            # k1 = aligned for rs register number
    andi    $k1, 0x1F                               # k1 = rs register number
    sll     $k1, $k1, 4                             # k1 = array index for saved rs register data
    la      $t0, MIPS_Regs                          # t0 -> saved register array
    addu    $k1, $t0                                # k1 -> register data of rs register
    ld      $t1, 0($k1)                             # t1 = rs register data
    srl     $k1, $k0, 16                            # k1 = aligned for rt register number
    andi    $k1, 0x1F                               # k1 = rt register number
    sll     $k1, $k1, 4                             # k1 = array index for saved rt register data
    addu    $k1, $t0                                # k1 -> register data of rt register
    ld      $t0, 0($k1)                             # t0 = rt register data
    bne     $t0, $t1, B_taken
    nop
    b       B_not_taken
    nop

#  ----------------------------
B_BC0x:                                             # At present we do not implement COP0 branches
B_BC1x:                                             # At present we do not implement COP1 branches
B_skip:                                             # Unrecognized opcode, so just pass it by
    addiu   $k0, $t2, 4                             # k0 = $t2+4 (ErrorEPC+4) (-> next opcode)
    mtc0    $k0, $30                                # store k0 in Error Exception Program Counter
    sync.p                                          # ensure COP0 register update before proceeding. This cannot be placed in a branch-delay slot.

GSHandler_Final_Exit:
    la      $k1, MIPS_Regs                          # Restore MIPS_Regs via $k1
    lq      $t0, 0x080($k1)
    lq      $t1, 0x090($k1)
    lq      $t2, 0x0A0($k1)
    lq      $gp, 0x1C0($k1)
    lq      $sp, 0x1D0($k1)
    lq      $fp, 0x1E0($k1)
    lq      $ra, 0x1F0($k1)
    lq      $k0, 0x1A0($k1)
    lq      $k1, 0x1B0($k1)

    eret                                            # Return from exception. Subsequent instructions are not fetched.

end_GSHandler:
.end    GSHandler

#include "gsm_engine_adv.S"


# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

# Must be executed with interrupts disabled.
.globl Enable_GSBreakpoint
.ent Enable_GSBreakpoint
Enable_GSBreakpoint:
# Set Data Address Write Breakpoint
# Trap writes to GS registers, so as to control their values

    li      $a0, TRAP_BASE                          # Address base for trapping
    li      $a1, TRAP_MASK                          # Address mask for trapping
                                                    # Trapping range is extended to match all kernel access segments

    sync.l                                          # Synchronize pipeline. Wait for any preceding loads to complete.

    li      $a2, 0x8000
    mtbpc   $a2                                     # All breakpoints off (BED = 1)

    sync.p                                          # Await instruction completion

    mtdab   $a0
    mtdabm  $a1

    sync.p                                          # Await instruction completion

    mfbpc   $a3
    sync.p                                          # Await instruction completion

    li      $a2, 0x60280000                         # Data read & write breakpoint on (DRE, DWE, DUE, DKE = 1). Syscalls are run in kernel mode, for the EE.
    mtbpc   $a2

    sync.p                                          # Await instruction completion

    jr      $ra
    nop
.end Enable_GSBreakpoint

# Must be executed with interrupts disabled.
.globl Disable_GSBreakpoint
.ent Disable_GSBreakpoint
Disable_GSBreakpoint:
    sync.l                                          # Synchronize pipeline. Wait for any preceding loads to complete.

    li      $a2, 0x8000
    mtbpc   $a2                                     # All breakpoints off (BED = 1)
    sync.p                                          # Await instruction completion

    jr      $ra
    nop
.end Disable_GSBreakpoint

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

.set pop
