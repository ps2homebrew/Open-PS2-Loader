/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

  The goal of the code in this file is to force syscall hooks to use
  an alternate stack when they need to use stack.
*/

#include <ee_cop0_defs.h>
#include <syscallnr.h>

	.set push
	.set noreorder
	.set noat

/**************************************************************************
 *
 * .text section
 *
 **************************************************************************/

	.text

	.globl SRWI_IncrementCntrlFlag
	.ent SRWI_IncrementCntrlFlag
SRWI_IncrementCntrlFlag:
	di
	lui $at, 0x003d;
	ori $at, 0xbc14
	lw $v0, 0($at)
	addiu $v0, 1
	sw $v0, 0($at)

	jr $ra
	ei
	.end SRWI_IncrementCntrlFlag

	.set pop
