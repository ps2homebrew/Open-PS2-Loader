#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
.set noreorder
.set nomacro

.globl ip_cksum
.globl inet_chksum
.globl inet_chksum_pseudo
.globl inet_chksum_pbuf
.globl inet_aton
.globl inet_addr
.globl inet_ntoa
.globl htons
.globl ntohs
.globl htonl
.globl ntohl

.section ".bss"
s_Str:  .space  16

.text

htons :
ntohs :
_htons:
    andi    $v1, $a0, 0xFFFF
    andi    $a1, $v1, 0xFF
    sll     $v0, $a1, 8
    srl     $a0, $v1, 8
    jr      $ra
    or      $v0, $v0, $a0

htonl :
_htonl:
ntohl :
    andi    $t1, $a0, 0xFF00
    sll     $t0, $a0, 24
    srl     $a3, $a0, 8
    sll     $a1, $t1, 8
    or      $a2, $t0, $a1
    andi    $v1, $a3, 0xFF00
    or      $v0, $a2, $v1
    srl     $a0, $a0, 24
    jr      $ra
    or      $v0, $v0, $a0

ip_cksum :
_ip_cksum:
    andi    $v0, $a0, 0x3
    move    $t0, $a0
    move    $t1, $a1
    move    $a0, $zero
    move    $t3, $zero
    move    $t2, $zero
    beqz    $v0, 1f
    move    $t4, $zero
    slti    $v1, $a1, 3
    bnez    $v1, 2f
    andi    $a1, $t0, 0x1
    bnez    $a1, 3f
    nop
14:
    andi    $a3, $t0, 0x2
    beqz    $a3, 4f
    slti    $t6, $t1, 72
    lhu     $t5, 0($t0)
    addiu   $t1, $t1, -2
    addu    $t2, $t2, $t5
    addiu   $t0, $t0, 2
1:
    slti    $t6, $t1, 72
4:
    bnez    $t6, 5f
    nop
    lw      $v1, 0($t0)
    lw      $a3, 4($t0)
6:
    lw      $a1, 8($t0)
    srl     $t7, $v1, 16
    addu    $t5, $t3, $t7
    lw      $t9, 12($t0)
    addu    $v1, $a0, $v1
    srl     $t6, $a3, 16
    addu    $a2, $t5, $t6
    addu    $t3, $v1, $a3
    lw      $t5, 16($t0)
    srl     $a3, $a1, 16
    addu    $v0, $a2, $a3
    addu    $t8, $t3, $a1
    srl     $a0, $t9, 16
    lw      $a1, 20($t0)
    addu    $t6, $v0, $a0
    addu    $v1, $t8, $t9
    srl     $t7, $t5, 16
    lw      $t9, 24($t0)
    addu    $a2, $t6, $t7
    addu    $t3, $v1, $t5
    srl     $a3, $a1, 16
    lw      $t5, 28($t0)
    addu    $v0, $a2, $a3
    addu    $t8, $t3, $a1
    srl     $a0, $t9, 16
    lw      $a1, 32($t0)
    addu    $t6, $v0, $a0
    addu    $v1, $t8, $t9
    srl     $t7, $t5, 16
    lw      $t9, 36($t0)
    addu    $a2, $t6, $t7
    addu    $t3, $v1, $t5
    srl     $a3, $a1, 16
    lw      $t5, 40($t0)
    addu    $v0, $a2, $a3
    addu    $t8, $t3, $a1
    srl     $a0, $t9, 16
    lw      $a1, 44($t0)
    addu    $t6, $v0, $a0
    addu    $v1, $t8, $t9
    srl     $t7, $t5, 16
    lw      $t9, 48($t0)
    addu    $a2, $t6, $t7
    addu    $t3, $v1, $t5
    srl     $a3, $a1, 16
    lw      $t5, 52($t0)
    addu    $v0, $a2, $a3
    addu    $t8, $t3, $a1
    srl     $a0, $t9, 16
    lw      $a1, 56($t0)
    addu    $v1, $t8, $t9
    addu    $t6, $v0, $a0
    lw      $t8, 60($t0)
    srl     $t7, $t5, 16
    addu    $t3, $v1, $t5
    addu    $a2, $t6, $t7
    srl     $a3, $a1, 16
    addiu   $t1, $t1, -64
    addu    $v0, $a2, $a3
    addu    $t9, $t3, $a1
    srl     $a0, $t8, 16
    slti    $t7, $t1, 72
    addu    $t3, $v0, $a0
    lw      $v1, 64($t0)
    lw      $a3, 68($t0)
    addu    $a0, $t9, $t8
    beqz    $t7, 6b
    addiu   $t0, $t0, 64
    sll     $v0, $t3, 16
    subu    $t9, $a0, $v0
    addu    $t8, $t2, $t9
    addu    $t2, $t8, $t3
5:
    slti    $a0, $t1, 4
    move    $a1, $zero
    bnez    $a0, 7f
    move    $a2, $zero
8:
    lw      $a3, 0($t0)
    addiu   $t1, $t1, -4
    srl     $v1, $a3, 16
    slti    $t3, $t1, 4
    addu    $a2, $a2, $v1
    addiu   $t0, $t0, 4
    beqz    $t3, 8b
    addu    $a1, $a1, $a3
7:
    sll     $t6, $a2, 16
    subu    $t5, $a1, $t6
    addu    $a1, $t2, $t5
    slti    $v1, $t1, 2
    b       9f
    addu    $t2, $a1, $a2
10:
    lhu     $a2, 0($t0)
    addiu   $t1, $t1, -2
    slti    $v1, $t1, 2
    addu    $t2, $t2, $a2
    addiu   $t0, $t0, 2
9:
    beqz    $v1, 10b
    nop
2:
    blez    $t1, 11f
    addiu   $a0, $t1, -1
12:
    lbu     $t7, 0($t0)
    move    $t1, $a0
    addu    $t2, $t2, $t7
    addiu   $t0, $t0, 1
    bgtz    $t1, 12b
    addiu   $a0, $a0, -1
11:
    srl     $v1, $t2, 16
    beqz    $t4, 13f
    andi    $v0, $t2, 0xFFFF
    srl     $t3, $t2, 16
    andi    $a0, $t2, 0xFFFF
    addu    $v0, $a0, $t3
    srl     $t9, $v0, 16
    andi    $t8, $v0, 0xFFFF
    addu    $t4, $t8, $t9
    andi    $t0, $t4, 0xFF
    srl     $t2, $t4, 8
    andi    $v0, $t2, 0xFF
    sll     $v1, $t0, 8
13:
    addu    $a1, $v0, $v1
    srl     $v1, $a1, 16
    andi    $a3, $a1, 0xFFFF
    jr      $ra
    addu    $v0, $a3, $v1
3:
    lbu     $a2, 0($t0)
    addiu   $t1, $t1, -1
    sll     $t2, $a2, 8
    addiu   $t0, $t0, 1
    b       14b
    li      $t4, 1

inet_chksum:
    addiu   $sp, $sp, -8
    sw      $ra, 0($sp)
    bal     _ip_cksum
    andi    $a1, $a1, 0xFFFF
    b       1f
    srl     $v1, $v0, 16
2:
    andi    $v0, $v0, 0xFFFF
    addu    $v0, $v0, $v1
    srl     $v1, $v0, 16
1:
    bnez    $v1, 2b
    nop
    nor     $v1, $zero, $v0
    lw      $ra, 0($sp)
    andi    $v0, $v1, 0xFFFF
    jr      $ra
    addiu   $sp, $sp, 8

inet_chksum_pseudo:
    addiu   $sp, $sp, -56
    sw      $s7, 44($sp)
    sw      $s6, 40($sp)
    sw      $s4, 32($sp)
    sw      $s3, 28($sp)
    sw      $s2, 24($sp)
    sw      $s1, 20($sp)
    sw      $s0, 16($sp)
    sw      $ra, 48($sp)
    sw      $s5, 36($sp)
    move    $s0, $a0
    move    $s3, $a1
    move    $s4, $a2
    andi    $s6, $a3, 0xFF
    lhu     $s7, 72($sp)
    move    $s1, $zero
    beqz    $a0, 1f
    move    $s2, $zero
    li      $s5, 1
5:
    lw      $a0,  4($s0)
    bal     _ip_cksum
    lhu     $a1, 10($s0)
    b       2f
    addu    $s1, $s1, $v0
3:
    addu    $s1, $v0, $a0
2:
    srl     $a0, $s1, 16
    bnez    $a0, 3b
    andi    $v0, $s1, 0xFFFF
    lhu     $a0, 10($s0)
    nop
    andi    $v1, $a0, 1
    beqz    $v1, 4f
    andi    $t0, $s1, 0xFF00
    andi    $t1, $s1, 0xFF
    subu    $a1, $s5, $s2
    sll     $a3, $t1, 8
    srl     $a2, $t0, 8
    or      $s1, $a3, $a2
    andi    $s2, $a1, 0xFF
4:
    lw      $s0, 0($s0)
    nop
    bnez    $s0, 5b
    nop
    beqz    $s2, 1f
    andi    $t4, $s1, 0xFF00
    andi    $t5, $s1, 0xFF
    sll     $t3, $t5, 8
    srl     $t2, $t4, 8
    or      $s1, $t3, $t2
1:
    lwl     $v1, 3($s3)
    lwl     $ra, 3($s3)
    lwr     $v1, 0($s3)
    lwl     $s5, 3($s4)
    lwr     $ra, 0($s3)
    lwl     $s2, 3($s4)
    andi    $v0, $v1, 0xFFFF
    lwr     $s5, 0($s4)
    addu    $t8, $s1, $v0
    srl     $t9, $ra, 16
    lwr     $s2, 0($s4)
    addu    $s3, $t8, $t9
    andi    $s4, $s5, 0xFFFF
    addu    $t6, $s3, $s4
    srl     $t7, $s2, 16
    move    $a0, $s6
    bal     _htons
    addu    $s1, $t6, $t7
    move    $a0, $s7
    bal     _htons
    addu    $s0, $s1, $v0
    b       6f
    addu    $v0, $s0, $v0
7:
    addu    $v0, $s6, $a0
6:
    srl     $a0, $v0, 16
    bnez    $a0, 7b
    andi    $s6, $v0, 0xFFFF
    nor     $s7, $zero, $v0
    andi    $v0, $s7, 0xFFFF
    lw      $ra, 48($sp)
    lw      $s7, 44($sp)
    lw      $s6, 40($sp)
    lw      $s5, 36($sp)
    lw      $s4, 32($sp)
    lw      $s3, 28($sp)
    lw      $s2, 24($sp)
    lw      $s1, 20($sp)
    lw      $s0, 16($sp)
    jr      $ra
    addiu   $sp, $sp, 56

inet_chksum_pbuf:
    addiu   $sp, $sp, -40
    sw      $s2, 24($sp)
    sw      $s1, 20($sp)
    sw      $s0, 16($sp)
    sw      $ra, 32($sp)
    sw      $s3, 28($sp)
    move    $s1, $a0
    move    $s0, $zero
    beqz    $a0, 1f
    move    $s2, $zero
    li      $s3, 1
5:
    lw      $a0,  4($s1)
    bal     _ip_cksum
    lhu     $a1, 10($s1)
    b       2f
    addu    $s0, $s0, $v0
3:
    addu    $s0, $v0, $a0
2:
    srl     $a0, $s0, 16
    bnez    $a0, 3b
    andi    $v0, $s0, 0xFFFF
    lhu     $a0, 10($s1)
    nop
    andi    $v1, $a0, 1
    beqz    $v1, 4f
    subu    $a1, $s3, $s2
    andi    $s2, $a1, 0xFF
    andi    $s0, $s0, 0xFFFF
4:
    lw      $s1, 0($s1)
    nop
    bnez    $s1, 5b
    nop
    beqz    $s2, 6f
    nor     $t2, $zero, $s0
    andi    $t0, $s0, 0xFF00
    andi    $t1, $s0, 0xFF
    sll     $a3, $t1, 8
    srl     $a2, $t0, 8
    or      $s0, $a3, $a2
1:
    nor     $t2, $zero, $s0
6:
    lw      $ra, 32($sp)
    lw      $s3, 28($sp)
    lw      $s2, 24($sp)
    lw      $s1, 20($sp)
    lw      $s0, 16($sp)
    andi    $v0, $t2, 0xFFFF
    jr      $ra
    addiu   $sp, $sp, 40

inet_aton :
_inet_aton:
    addiu   $sp, $sp, -40
    sw      $s0, 32($sp)
    sw      $ra, 36($sp)
    addiu   $t7, $sp, 16
    lbu     $a2, 0($a0)
    move    $s0, $a1
    move    $t3, $t7
    li      $t4, 48
    li      $t5, 46
    addiu   $v0, $a2, -48
    andi    $a3, $v0, 0xFF
    sltiu   $v1, $a3, 10
    beqz    $v1, 1f
    addiu   $t6, $sp, 28
14:
    move    $t0, $zero
    beq     $a2, $t4, 2f
    li      $t1, 10
10:
    li      $t2, 16
8:
    addiu   $t8, $a2, -97
    andi    $a1, $t8, 0xFF
    addiu   $v1, $a2, -32
    sltiu   $v0, $a3, 10
    sltiu   $v1, $v1, 96
    beqz    $v0, 3f
    sltiu   $a3, $a1, 6
4:
    mult    $t0, $t1
    addiu   $a0, $a0, 1
    mflo    $t0
    addu    $a3, $t0, $a2
    lbu     $a2, 0($a0)
    addiu   $t0, $a3, -48
    addiu   $a1, $a2, -48
    andi    $a3, $a1, 0xFF
    addiu   $t8, $a2, -97
    andi    $a1, $t8, 0xFF
    addiu   $v1, $a2, -32
    sltiu   $v0, $a3, 10
    sltiu   $v1, $v1, 96
    bnez    $v0, 4b
    sltiu   $a3, $a1, 6
3:
    addiu   $t9, $a2, -65
    sltiu   $a1, $a1, 26
    bne     $t1, $t2, 5f
    sltiu   $v0, $t9 ,6
    beqz    $v1, 5f
    nop
    bnez    $a3, 6f
    nop
    beqz    $v0, 5f
    nop
6:
    addiu   $a0, $a0, 1
    sll     $v1, $t0, 4
    bnez    $a1, 7f
    addiu   $v0, $a2, -87
    addiu   $v0, $a2, -55
7:
    lbu     $a2, 0($a0)
    or      $t0, $v1, $v0
    addiu   $ra, $a2, -48
    b       8b
    andi    $a3, $ra, 0xFF
2:
    addiu   $a0, $a0, 1
    lbu     $a2, 0($a0)
    nop
    xori    $t8, $a2, 0x78
    xori    $t2, $a2, 0x58
    sltiu   $a3, $t8, 1
    sltiu   $t1, $t2, 1
    or      $a1, $a3, $t1
    beqz    $a1, 9f
    addiu   $ra, $a2, -48
    addiu   $a0, $a0, 1
    lbu     $a2, 0($a0)
    li      $t1, 16
    addiu   $t9, $a2, -48
    b       10b
    andi    $a3, $t9, 0xFF
5:
    bne     $a2, $t5, 11f
    nop
    sltu    $a2, $t3, $t6
    beqz    $a2, 12f
    move    $v1, $zero
    addiu   $a0, $a0, 1
    lbu     $a2, 0($a0)
    sw      $t0, 0($t3)
    addiu   $v0, $a2, -48
    andi    $a3, $v0, 0xFF
    sltiu   $v1, $a3, 10
    bnez    $v1, 14b
    addiu   $t3, $t3, 4
1:
    move    $v1, $zero
12:
    lw      $ra, 36($sp)
    lw      $s0, 32($sp)
    move    $v0, $v1
    jr      $ra
    addiu   $sp, $sp, 40
9:
    andi    $a3, $ra, 0xFF
    b       10b
    li      $t1, 8
11:
    beqz    $a2, 15f
    subu    $t6, $t3, $t7
    addiu   $t4, $a2, -32
    sltiu   $a0, $t4, 96
    beqz    $a0, 12b
    move    $v1, $zero
    xori    $v0, $a2, 0x20
    xori    $t2, $a2, 0xC
    sltu    $t6, $zero, $v0
    sltu    $t1, $zero, $t2
    and     $t5, $t6, $t1
    beqz    $t5, 15f
    subu    $t6, $t3, $t7
    xori    $t9, $a2, 0xA
    xori    $a3, $a2, 0xD
    sltu    $t8, $zero, $t9
    sltu    $a1, $zero, $a3
    and     $v1, $t8, $a1
    bnez    $v1, 16f
    xori    $t4, $a2, 0xB
    subu    $t6, $t3, $t7
15:
    sra     $t7, $t6, 2
    addiu   $a0, $t7, 1
    li      $t3, 2
    beq     $a0, $t3, 17f
    slti    $t1, $a0, 3
    beqz    $t1, 18f
    li      $t2, 3
    beqz    $a0, 12b
    move    $v1, $zero
10:
    beqz    $s0,19f
    li      $v1, 1
    bal     _htonl
    move    $a0, $t0
    sw      $v0, 0($s0)
    li      $v1, 1
19:
    lw      $ra, 36($sp)
    lw      $s0, 32($sp)
    move    $v0, $v1
    jr      $ra
    addiu   $sp, $sp, 40
17:
    lui     $a1, 0xFF
    ori     $v1, $a1, 0xFFFF
    sltu    $t8, $v1, $t0
    bnez    $t8, 12b
    move    $v1, $zero
    lw      $a3, 16($sp)
    nop
    sll     $v0, $a3, 24
    b       10b
    or      $t0, $t0, $v0
18:
    beq     $a0, $t2, 21f
    li      $v0, 4
    bne     $a0, $v0, 10b
    sltiu   $t7, $t0, 256
    beqz    $t7, 12b
    move    $v1, $zero
    lw      $a1, 16($sp)
    lw      $v1, 20($sp)
    lw      $t2, 24($sp)
    sll     $v0, $a1, 24
    sll     $t8, $v1, 16
    or      $t6, $v0, $t8
    sll     $t1, $t2, 8
    or      $v0, $t6, $t1
    b       10b
    or      $t0, $t0, $v0
16:
    xori    $t5, $a2, 0x9
    sltu    $a2, $zero, $t5
    sltu    $a0, $zero, $t4
    and     $ra, $a2, $a0
    bnez    $ra, 12b
    move    $v1, $zero
    b       15b
    subu    $t6, $t3, $t7
21:
    li      $ra, 0xFFFF
    sltu    $t9, $ra, $t0
    bnez    $t9, 12b
    move    $v1, $zero
    lw      $t3, 16($sp)
    lw      $t5, 20($sp)
    sll     $a0, $t3, 24
    sll     $t4, $t5, 16
    or      $a2, $a0, $t4
    b       10b
    or      $t0, $t0, $a2

inet_addr:
    addiu   $sp, $sp, -32
    sw      $ra, 24($sp)
    bal     _inet_aton
    addiu   $a1, $sp, 16
    beqz    $v0, 1f
    li      $v1, -1
    lw      $v1, 16($sp)
1:
    lw      $ra, 24($sp)
    move    $v0, $v1
    jr      $ra
    addiu   $sp, $sp, 32

inet_ntoa:
    lui     $v0, %hi( s_Str )
    addiu   $sp, $sp, -8
    addiu   $t5, $v0, %lo( s_Str )
    sw      $a0, 8($sp)
    move    $a3, $t5
    addiu   $t1, $sp, 8
    move    $t2, $zero
    li      $t0, 10
    li      $t4, 255
    li      $t3, 46
4:
    lbu     $a1, 0($t1)
    move    $v1, $zero
1:
    divu    $zero, $a1, $t0
    addu    $a0, $sp, $v1
    addiu   $v1, $v1, 1
    andi    $v1, $v1, 0xFF
    mfhi    $a1
    addiu   $t6, $a1, 48
    mflo    $a2
    andi    $a1, $a2, 0xFF
    bnez    $a1, 1b
    sb      $t6, 0($a0)
    addiu   $t7, $v1, -1
    andi    $v1, $t7, 0xFF
    beq     $v1, $t4, 2f
    sb      $a2, 0($t1)
    li      $a1, 255
3:
    addu    $t9, $sp, $v1
    lbu     $a2, 0($t9)
    addiu   $t8, $v1, -1
    andi    $v1, $t8, 0xFF
    sb      $a2, 0($a3)
    bne     $v1, $a1, 3b
    addiu   $a3, $a3, 1
2:
    addiu   $a0, $t2, 1
    andi    $t2, $a0, 0xFF
    sltiu   $v0, $t2, 4
    sb      $t3, 0($a3)
    addiu   $t1, $t1, 1
    bnez    $v0, 4b
    addiu   $a3, $a3, 1
    move    $v0, $t5
    addiu   $sp, $sp, 8
    jr      $ra
    sb      $zero, -1($a3)
