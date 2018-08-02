/*
#
# DVE management.
#-------------------------------------------------------------
# Copyright (C) Mega Man, 2010-2014
#
# Based on code from Kernelloader.
#
*/

.globl BoardInf

.data
BoardInf:
.word	0	#This is left unset on a CEX, but is obtained from 0xBF803204 via a byte load on a TOOL.
		#The same register is also accessed by the BOARDINF module and its value is stored in IOP BootMode 6.
		#The MPU-4 board in the TOOL will cause a 0x61 to be registered.

.text

.set noreorder
.set noat

.global dve_set_reg
.global dve_prepare_bus

.ent dve_prepare_bus				# Note: this does not support the SCPH-75000 and later.
dve_prepare_bus:
	li	$v0, 0xBF80146E
	lhu	$v1, 0($v0)			# Get DEV9C revision
	andi	$v1, 0xF0
	lw	$a0, BoardInf			# In the ROM kernel, this is a check against IDs 0x40, 0x60 and 0x61.
	beqz	$a0, check_dev9_type
	li	$v0, 0xBF803218
	li	$a1, 554
	li	$v1, 2
	sw	$v1, 0($v0)			# In the ROM kernel, this step is not done.
	sw	$zero, 0($v0)
	nop

prepare_bus_wait_loop:
	nop
	addiu	$a1, 0xFFFF
	bgez	$a1, prepare_bus_wait_loop
	nop
	li	$v0, 0xBF803218
	li	$v1, 2
	jr	$ra
	sw	$v1, 0($v0)

check_dev9_type:
	li	$v0, 0x20			# In the ROM kernel, this is assumed to be only either case.
	beq	$v1, $v0, dve_pcmcia_stuff
	li	$v0, 0x30
	beq	$v1, $v0, dve_expbay_stuff
	lui	$v0, 0xBA00
	jr	$ra
	nop

dve_pcmcia_stuff:				# PC CARD type (v2.x)
	li	$a0, 0xBF801470
	li	$v1, 0xBF801472
	li	$a1, 554
	li	$v0, 1
	sh	$zero, 0($a0)
	sh	$v0, 0($v1)			# In the ROM kernel, this step is not done.
	sh	$zero, 0($v1)
	nop

dve_pcmcia_waitloop:
	nop
	addiu	$a1, 0xFFFF
	bgez	$a1, dve_pcmcia_waitloop
	nop
	lui	$v0, 0xBF80
	j	dve_stuff_return
	ori	$v0, $v0, 0x1472

dve_expbay_stuff:				# SSBUS buffer type (v3.x)
	ori	$v0, 0xA
	li	$a1, 554
	li	$v1, 1
	sh	$v1, 0($v0)
	sh	$zero, 0($v0)

dve_expbay_wait_loop:
	nop
	addiu	$a1, 0xFFFF
	bgez	$a1, dve_expbay_wait_loop
	nop
	li	$v0, 0xBA00000A

dve_stuff_return:
	li	$v1, 1
	jr	$ra
	sh	$v1, 0($v0)
.end dve_prepare_bus


/* Set DVE register */
/* Parameter: a0, a1 */
.ent dve_set_reg
dve_set_reg:
	li	$t0, 0xBA000008
	li	$t1, 0xBA000006
	li	$v1, 0xBA000010
	li	$a2, 0xBA000012
	li	$a3, 0xBA000002
	li	$v0, 3
	sh	$v0, 0($t0)			# In the ROM kernel, this step is not done.
	sh	$v0, 0($t1)
	li	$v0, 0x42
	sh	$a0, 0($v1)
	lui	$v1, 0xBA00
	sh	$a1, 0($a2)
	sh	$v0, 0($a3)
	li	$v0, 0x82
	sh	$v0, 0xBA000000
	lhu	$a0, 0($t1)
	andi	$a0, 0xFFFF
	andi	$v0, $a0, 1
	bnez	$v0, dve_set_reg_return_neg_1	# In the ROM kernel, this function cannot fail.
	move	$a3, $zero

	li	$a2, 0x1869F
	li	$a1, 0xBA000006
	andi	$v0, $a0, 2

dve_set_reg_return_0:
	beqz	$v0, dve_set_reg_start_loop
	addiu	$v1, $a3, 1
	jr	$ra
	move	$v0, $zero

dve_set_reg_start_loop:
	li	$v0, 0x3E7
	nop

dve_set_reg_loop2:
	addiu	$v0, 0xFFFF
	bgez	$v0, dve_set_reg_loop2
	nop

	move	$a3, $v1
	slt		$v0, $a2, $a3
	bnez	$v0, dve_set_reg_return_neg_1
	nop
	lhu	$v0, 0($a1)
	andi	$a0, $v0, 0xFFFF
	andi	$v1, $a0, 1
	beqz	$v1, dve_set_reg_return_0
	andi	$v0, $a0, 2

dve_set_reg_return_neg_1:
	lui	$v0, 0xFFFF
	jr	$ra
	ori	$v0, $v0, 0xFFFF
.end dve_set_reg

