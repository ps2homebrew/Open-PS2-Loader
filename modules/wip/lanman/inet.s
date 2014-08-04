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
.globl htons
.globl ntohs
.globl htonl
.globl ntohl
.globl htona
.globl ntoha

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

htona :
_htona:
ntoha :
	lbu     $t0, 0($a0)
	lbu     $t1, 1($a0)
	lbu     $t2, 2($a0)
	lbu     $t3, 3($a0)
	move    $v0, $zero
	andi    $t0, $t0, 0xFF
	or		$v0, $v0, $t0
	sll		$v0, $v0, 24
	andi    $t1, $t1, 0xFF
	sll		$t1, $t1, 16	
	or		$v0, $v0, $t1
	andi    $t2, $t2, 0xFF
	sll		$t2, $t2, 8	
	or		$v0, $v0, $t2
	andi    $t3, $t3, 0xFF
    jr      $ra
	or		$v0, $v0, $t3

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
    addiu   $s6, $zero, 6 	#IP_PROTO_TCP
    move    $s7, $a3
    andi    $s7, $s7, 0xFFFF	#tot_len
    move    $s1, $zero
    beqz    $a0, 1f
    move    $s2, $zero
    li      $s5, 1
5:
    move    $a0, $s0 		#<-- payload
    bal     _ip_cksum
    move	$a1, $s7		#<-- len
    b       2f
    addu    $s1, $s1, $v0
3:
    addu    $s1, $v0, $a0
2:
    srl     $a0, $s1, 16
    bnez    $a0, 3b
    andi    $v0, $s1, 0xFFFF
    move	$a0, $s7		#<-- len
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
