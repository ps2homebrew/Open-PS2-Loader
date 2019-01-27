/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include <ee_cop0_defs.h>
#include <syscallnr.h>

.set push
.set noreorder
.set noat
#.set nomacro #Uncomment to set nomacro, to check for hidden macro instructions.

/**************************************************************************
 *
 * .text section
 *
 **************************************************************************/

.text

.globl SRWI_IncrementCntrlFlag
.ent SRWI_IncrementCntrlFlag
SRWI_IncrementCntrlFlag:
	#Wait for interrupts to be disabled, to avoid causing a hang if the di instruction binds within an interrupt handler.
	lui $at, 1
1:
	di
	sync.p
	mfc0 $v0, $12
	and $v0, $v0, $at #Wait until EIE == 0
        bne $v0, $zero, 1b
	nop
	lui $at, 0x003e
	lw $v0, -17388($at)
	addiu $v0, 1
	sw $v0, -17388($at)

	jr $ra
        ei
.end SRWI_IncrementCntrlFlag

.globl RnC3_AlwaysAllocMem
.ent RnC3_AlwaysAllocMem
RnC3_AlwaysAllocMem:	//Read the comments on the "Ratchet and Clank: Up Your Arsenal" patch in patches.c
	addu	$t0, $a0, $s1
	lui	$v1, 0x2000	//Bypass the cache because the cache has already been flushed by the game.
	or	$t0, $v1
	addiu	$v1, $zero, 1
	jr	$ra
	sb	$v1, 0x3D8($t0)	//Change "beqz $s7, 0x13" in iop_stash_daemon to "beqz $s7, 0x01"
.end RnC3_AlwaysAllocMem

.set pop

