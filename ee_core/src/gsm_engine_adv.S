#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013, 2014, 2015, 2016 doctorxyz
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#
# Advanced GSM operations
# This file is included by gsm_engine.S
#

#define ABI_EABI64 // force all register names to EABI64 (legacy toolchain)
#include "as_reg_compat.h"

.extern setdve_576P
.extern dveStat1
.extern dveStat2

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

.ent DTV_576P
DTV_576P:
    addiu   $sp, $sp, -0x10
    sd      $ra, 0($sp)

    # $a0 = GCONT setting (RGB or Y Pb/Cb Pr/Cr)
    sll     $a0, $a3, 25

    # $v0 = GS base address
    lui     $v0, (GS_BASE >> 16)

# *GS_SMODE1=0x00000017404B0504;
    # Reset (SINT and PRST bits set)
    lui     $v1, 0x0017
    ori     $v1, 0x404B
    dsll    $v1, 16
    ori     $v1, 0x0504
    or      $v1, $a0
    sd      $v1, GS_SMODE1($v0)             # store it

# *GS_SYNCH1=0x000402E02003C827;
    lui     $v1, 0x0004
    ori     $v1, 0x02E0
    dsll    $v1, 16
    ori     $v1, 0x2003
    dsll    $v1, 16
    ori     $v1, 0xC827
    sd      $v1, GS_SYNCH1($v0)             # store it

# *GS_SYNCH2=0x0019CA67;
    lui     $v1, 0x0019
    ori     $v1, 0xCA67
    sd      $v1, GS_SYNCH2($v0)             # store it

# *GS_SYNCV=0x00A9000002700005;
    lui     $v1, 0x00A9
    ori     $v1, 0x0000
    dsll    $v1, 16
    ori     $v1, 0x0270
    dsll    $v1, 16
    ori     $v1, 0x0005
    sd      $v1, GS_SYNCV($v0)              # store it

# *GS_SMODE2=Target_SMODE2;
    sd      $zero, GS_SMODE2($v0)           # This mode is a progressive mode. Hence it has to be non-interlaced and its FFMD setting does not matter.

# *GS_SRFSH=0;
    li      $v1, 4
    sd      $v1, GS_SRFSH($v0)              # store it

# As with 480P mode, there is no need to enable the PLL.

# *GS_SMODE1=0x0000001740490504;
# Sync on (clear SINT)
    lui     $v1, 0x0017
    ori     $v1, 0x4049
    dsll    $v1, 16
    ori     $v1, 0x0504
    or      $v1, $a0

    jal     setdve_576P                     # Set up DVE parameters
    sd      $v1, GS_SMODE1($v0)             # store it

    lw      $v1, dveStat1
    li      $v0, 0x1C
    andi    $v1, $v1, 0xFE
    bne     $v1, $v0, DTV576P_end
    addiu   $v1, $zero, 0x20

    lw      $v0, dveStat2
    andi    $v0, $v0, 0xF0
    bne     $v0, $v1, DTV576P_stat2_020_skip
    addiu   $a0, $zero, 0x63

    jal     dve_set_reg
    addiu   $a1, $zero, 0x88

DTV576P_stat2_020_skip:
    addiu   $a0, $zero, 0x30
    jal     dve_set_reg
    move    $a1, $zero

    addiu   $a0, $zero, 0x31
    jal     dve_set_reg
    addiu   $a1, $zero, 0x07

    addiu   $a0, $zero, 0x32
    jal     dve_set_reg
    addiu   $a1, $zero, 0x01

    addiu   $a0, $zero, 0x33
    jal     dve_set_reg
    addiu   $a1, $zero, 0x14

    addiu   $a0, $zero, 0x38
    jal     dve_set_reg
    addiu   $a1, $zero, 0x10

DTV576P_end:
    ld      $ra, 0($sp)
    jr      $ra
    addiu   $sp, $sp, 0x10

.end DTV_576P

# -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

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
# | PVS    | 27:27 | int  0:1:0    |                                  | 0x1   |
# | PHS    | 26:26 | int  0:1:0    |                                  | 0x1   |
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
    sd      $a1, Source_SMODE1($s0)         # Source_SMODE1 = a1
    lb      $v0, SMODE1_fix($s2)            # v0 = SMODE1_fix
    beql    $v0, $zero, store_v0_as_SMODE1  # in case of SMODE1_fix is disabled
    or      $v0, $zero, $a1                 #   go use v0=a1 for SMODE1
    ld      $v0, Target_SMODE1($s1)         # v0 = Target_SMODE1
    beql    $v0, $zero, exit_GSHandler      # in case of  Target_SMODE1 is zero
    sd      $a1, 0($a3)                     #   go use Source_SMODE1

store_v0_as_SMODE1:
    b       exit_GSHandler                  # Now go exit
    sd      $v0, 0($a3)                     # after storing

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
    sd      $a1, Source_SRFSH($s0)          # Source_SRFSH = a1
    lb      $v0, SRFSH_fix($s2)             # v0 = SRFSH_fix
    beql    $v0, $zero, store_v0_as_SRFSH   # in case of SRFSH_fix is disabled
    or      $v0, $zero, $a1                 #   go use v0=a1 for SRFSH
    ld      $v0, Target_SRFSH($s1)          # v0 = Target_SRFSH
    beql    $v0, $zero, exit_GSHandler      # in case of  Target_SRFSH is zero
    sd      $a1, 0($a3)                     #   go use Source_SRFSH

store_v0_as_SRFSH:
    b       exit_GSHandler                  # Now go exit
    sd      $v0, 0($a3)                     # after storing

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
    sd      $a1, Source_SYNCH1($s0)         # Source_SYNCH1 = a1
    lb      $v0, SYNCH_fix($s2)             # v0 = SYNCH_fix
    beql    $v0, $zero, store_v0_as_SYNCH1  # in case of SYNCH_fix is disabled
    or      $v0, $zero, $a1                 #   go use v0=a1 for SYNCH1
    ld      $v0, Target_SYNCH1($s1)         # v0 = Target_SYNCH1
    beql    $v0, $zero, exit_GSHandler      # in case of  Target_SYNCH1 is zero
    sd      $a1, 0($a3)                     #   go use Source_SYNCH1

store_v0_as_SYNCH1:
    b       exit_GSHandler                  # Now go exit
    sd      $v0, 0($a3)                     # after storing

# ----------------------------
# SYNCH2
# ----.-----.----------.-------.
#|Name|Pos. |Format    | Mask  |
# ----+-----+----------+-------|
#|HF  |10:0 |int 0:11:0| 0x7FF |
#|HB  |21:11|int 0:11:0| 0x7FF |
#'----^-----^----------^-------'
have_SYNCH2_write:
    sd      $a1, Source_SYNCH2($s0)         # Source_SYNCH2 = a1
    lb      $v0, SYNCH_fix($s2)             # v0 = SYNCH_fix
    beql    $v0, $zero, store_v0_as_SYNCH2  # in case of SYNCH_fix is disabled
    or      $v0, $zero, $a1                 #   go use v0=a1 for SYNCH2
    ld      $v0, Target_SYNCH2($s1)         # v0 = Target_SYNCH2
    beql    $v0, $zero, exit_GSHandler      # in case of  Target_SYNCH2 is zero
    sd      $a1, 0($a3)                     #   go use Source_SYNCH2

store_v0_as_SYNCH2:
    b       exit_GSHandler                  # Now go exit
    sd      $v0, 0($a3)                     # after storing

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
    sd      $a1, Source_SYNCV($s0)          # Source_SYNCV = a1
    lb      $v0, SYNCV_fix($s2)             # v0 = SYNCV_fix
    beql    $v0, $zero, store_v0_as_SYNCV   # in case of SYNCV_fix is disabled
    or      $v0, $zero, $a1                 #   go use v0=a1 for SYNCV
    ld      $v0, Target_SYNCV($s1)          # v0 = Target_SYNCV
    beql    $v0, $zero, exit_GSHandler      # in case of  Target_SYNCV is zero
    sd      $a1, 0($a3)                     #   go use Source_SYNCV

store_v0_as_SYNCV:
    b       exit_GSHandler                  # Now go exit
    sd      $v0, 0($a3)                     # after storing
