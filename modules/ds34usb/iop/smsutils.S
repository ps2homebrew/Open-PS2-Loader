/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# Highly optimized versions of memcpy/memset.
# Pulled out 'sde' library from MIPS.
# by Eugene Plotnikov
*/

#include "as_reg_compat.h"

.ifdef .gasversion.
.section .mdebug.abi32
.nan legacy
.module softfloat
.module nooddspreg
.endif
.set noreorder
.set noat
.set nomacro

.globl mips_memcpy
.globl mips_memset

.text
mips_memcpy:
    move    $v0, $a0
    beqz    $a2, 1f
    sltiu   $t2, $a2, 12
    bnez    $t2, 2f
    xor     $v1, $a1, $a0
    andi    $v1, $v1, 3
    negu    $a3, $a0
    beqz    $v1, 3f
    andi    $a3, $a3, 3
    beqz    $a3, 4f
    subu    $a2, $a2, $a3
    lwr     $v1, 0($a1)
    lwl     $v1, 3($a1)
    addu    $a1, $a1, $a3
    swr     $v1, 0($a0)
    addu    $a0, $a0, $a3
4:
    andi    $v1, $a2, 15
    subu    $a3, $a2, $v1
    beqz    $a3, 5f
    move    $a2, $v1
    addu    $a3, $a3, $a1
6:
    lwr     $v1, 0($a1)
    lwl     $v1, 3($a1)
    lwr     $t0, 4($a1)
    lwl     $t0, 7($a1)
    lwr     $t1, 8($a1)
    lwl     $t1, 11($a1)
    lwr     $t2, 12($a1)
    lwl     $t2, 15($a1)
    sw      $v1, 0($a0)
    sw      $t0, 4($a0)
    sw      $t1, 8($a0)
    addiu   $a1, $a1, 16
    addiu   $a0, $a0, 16
    bne     $a1, $a3, 6b
    sw      $t2, -4($a0)
5:
    andi    $v1, $a2, 3
    subu    $a3, $a2, $v1
    beqz    $a3, 2f
    move    $a2, $v1
    addu    $a3, $a3, $a1
7:
    lwr     $v1, 0($a1)
    lwl     $v1, 3($a1)
    addiu   $a1, $a1, 4
    addiu   $a0, $a0, 4
    bne     $a1, $a3, 7b
    sw      $v1, -4($a0)
    b       2f
    nop
3:
    beqz    $a3, 8f
    subu    $a2, $a2, $a3
    lwr     $v1, 0($a1)
    addu    $a1, $a1, $a3
    swr     $v1, 0($a0)
    addu    $a0, $a0, $a3
8:
    andi    $v1, $a2, 15
    subu    $a3, $a2, $v1
    beqz    $a3, 9f
    move    $a2, $v1
    addu    $a3, $a3, $a1
10:
    lw      $v1, 0($a1)
    lw      $t0, 4($a1)
    lw      $t1, 8($a1)
    lw      $t2, 12($a1)
    sw      $v1, 0($a0)
    sw      $t0, 4($a0)
    sw      $t1, 8($a0)
    addiu   $a1, $a1, 16
    addiu   $a0, $a0, 16
    bne     $a1, $a3, 10b
    sw      $t2, -4($a0)
9:
    andi    $v1, $a2, 3
    subu    $a3, $a2, $v1
    beqz    $a3, 2f
    move    $a2, $v1
    addu    $a3, $a3, $a1
11:
    lw      $v1, 0($a1)
    addiu   $a1, $a1, 4
    addiu   $a0, $a0, 4
    bne     $a1, $a3, 11b
    sw      $v1, -4($a0)
2:
    beqz    $a2, 1f
    addu    $a3, $a2, $a1
12:
    lbu     $v1, 0($a1)
    addiu   $a1, $a1, 1
    addiu   $a0, $a0, 1
    bne     $a1, $a3, 12b
    sb      $v1, -1($a0)
1:
    jr      $ra

mips_memset:
    move    $v0, $a0
    beqz    $a2, 1f
    sltiu   $t2, $a2, 16
    bnez    $t2, 2f
    andi    $a1, $a1, 255
    sll     $t2, $a1, 8
    or      $a1, $a1, $t2
    sll     $t2, $a1, 16
    or      $a1, $a1, $t2
    andi    $v1, $a0, 3
    beqz    $v1, 3f
    li      $a3, 4
    subu    $a3, $a3, $v1
    subu    $a2, $a2, $a3
    swr     $a1, 0($a0)
    addu    $a0, $a0, $a3
3:
    andi    $v1, $a2, 15
    subu    $a3, $a2, $v1
    beqz    $a3, 4f
    move    $a2, $v1
    addu    $a3, $a3, $a0
5:
    sw      $a1, 0($a0)
    sw      $a1, 4($a0)
    sw      $a1, 8($a0)
    addiu   $a0, $a0, 16
    bne     $a0, $a3, 5b
    sw      $a1, -4($a0)
4:
    andi    $v1, $a2, 3
    subu    $a3, $a2, $v1
    beqz    $a3, 2f
    move    $a2, $v1
    addu    $a3, $a3, $a0
6:
    addiu   $a0, $a0, 4
    bne     $a0, $a3, 6b
    sw      $a1, -4($a0)
2:
    beqz    $a2, 1f
    addu    $a3, $a2, $a0
7:
    addiu   $a0, $a0, 1
    bne     $a0, $a3, 7b
    sb      $a1, -1($a0)
1:
_dummy:
    jr      $ra
    nop
