/*
#
# DVE management.
#-------------------------------------------------------------
# Copyright (C) Mega Man, 2010-2014
#
# Based on code from Kernelloader. Code for TOOLS removed, updated for support up to the SCPH-70000.
# This does not support the SCPH-75000 and later.
#
*/

#define ABI_EABI64 // force all register names to EABI64 (legacy toolchain)
#include "as_reg_compat.h"

.global dveStat1
.global dveStat2

.data
dveStat1:
.word    0

dveStat2:
.word    0

.text

.set noreorder

.global dve_get_reg
.global dve_set_reg
.global dve_prepare_bus

.ent dve_prepare_bus
dve_prepare_bus:
    addiu   $sp, $sp, -0x10
    sd      $ra, 0($sp)

    # Code for TOOLs removed.
    li      $v1, 0x30
check_dev9_type:
    li      $v0, 0xBF80146E
    lbu     $v0, 0($v0)                 # This was actually loaded into a global variable during the "Initialize GS" step.
    andi    $v0, $v0, 0xF0
    bne     $v0, $v1, dve_pcmcia_stuff  # In the RTE SBIOS, this must be only either revision 2.x or 3.x.
    addiu   $a0, $zero, 554

dve_expbay_stuff:                       # SSBUS buffer type (v3.x)
    li      $v0, 0xBA00000A
    sh      $zero, 0($v0)

dve_expbay_wait_loop:                   # $a0 is initialized before the branch to dve_expbay_stuff.
    nop
    addiu   $a0, -1
    nop
    nop
    nop
    bgez    $a0, dve_expbay_wait_loop
    addiu   $v1, $zero, 1

    sh      $v1, 0($v0)

    jal     dve_get_reg
    addiu   $a0, $zero, 0x7E

    sw      $v0, dveStat1

    jal     dve_get_reg
    addiu   $a0, $zero, 0x7F

    sw      $v0, dveStat2

    lw      $a0, dveStat1
    addiu   $v1, $zero, 0x1D
    bne     $a0, $v1, dve_expbay_dve_check_ok
    addiu   $v1, $zero, 0x1C

dve_expbay_halt_loop:
    b       dve_expbay_halt_loop
    nop

dve_expbay_dve_check_ok:
    lw      $v0, dveStat1
    andi    $v0, $v0, 0xFE
    bne     $v0, $v1, dve_stuff_return
    ld      $ra, 0($sp)
    addiu   $a0, $zero, 0xB1
    nop
dve_expbay_dve_write_loop:
    jal     dve_set_reg
    move    $a1, $zero
    bnez    $v0, dve_expbay_dve_write_loop
    addiu   $a0, $zero, 0xB1

    b       dve_stuff_return
    ld      $ra, 0($sp)

dve_pcmcia_stuff:                       # PC CARD type (v2.x)
    li      $v0, 0xBF801470
    li      $v1, 0xBF801472
    sh      $zero, 0($v0)
    sh      $zero, 0($v1)
    nop

dve_pcmcia_waitloop:                    # $a0 is initialized before the branch to dve_pcmcia_stuff.
    nop
    addiu   $a0, -1
    nop
    nop
    nop
    bgez    $a0, dve_pcmcia_waitloop
    addiu   $v0, $zero, 1
    sh      $v0, 0($v1)
    ld      $ra, 0($sp)

dve_stuff_return:
    jr      $ra
    addiu   $sp, $sp, 0x10
.end dve_prepare_bus

/* Get DVE register */
/* Parameter: a0 */
.ent dve_get_reg
dve_get_reg:
    # Code for TOOLs removed.
    andi    $a0, $a0, 0xff
    li      $t0, 3
dev9_get_reg:
    lui     $v0, 0xBA00                 # Base address = 0xBA000000
    sh      $t0, 0x0006($v0)
    li      $t0, 0x42
    sh      $t0, 0x0002($v0)
    sh      $a0, 0x0010($v0)            # arg1
    li      $t0, 0x81
    sh      $t0, 0x0000($v0)
dev9_get_reg_wait_loop:
    lhu     $t0, 0x0006($v0)
    beqz    $t0, dev9_get_reg_wait_loop
    nop
    li      $t0, 3
    sh      $t0, 0x0006($v0)
    li      $t0, 0x43
    sh      $t0, 0x0002($v0)
    li      $t0, 0x81
    sh      $t0, 0x0000($v0)
dev9_set_reg_wait_loop2:
    lhu     $t0, 0x0006($v0)
    beqz    $t0, dev9_set_reg_wait_loop2
    nop
    lhu     $v1, 0x0010($v0)
    sync.l
    jr      $ra
    andi    $v0, $v1, 0xff
.end dve_get_reg

/* Set DVE register */
/* Parameter: a0, a1 */
.ent dve_set_reg
dve_set_reg:
    # Code for TOOLs removed.
    andi    $a1, $a1, 0xff
    andi    $a0, $a0, 0xff
dve_set_reg_dev9:
    lui     $t0, 0xBA00                 # Base address = 0xBA000000
    li      $v0, 3
    li      $v1, 0x42
    li      $t1, 0x82
dve_set_reg_dev9_loop:
    sh      $v0, 0x0006($t0)
    sh      $v1, 0x0002($t0)
    sh      $a0, 0x0010($t0)
    sh      $a1, 0x0012($t0)
    sh      $t1, 0x0000($t0)

    lhu     $t2, 0x0006($t0)
    andi    $t2, $t2, 1
    bnez    $t2, dve_set_reg_dev9_loop
    nop

dve_set_reg_dev9_loop2:
    lhu     $v0, 0x0006($t0)
    andi    $v0, $v0, 2
    beqz    $v0, dve_set_reg_dev9_loop2
    nop
    sync.l
    jr      $ra
    move    $v0, $zero
.end dve_set_reg
