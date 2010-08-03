/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

  The goal of the code in this file is to force syscall hooks to use
  an alternate stack when they need to use stack.  
*/

	.text

	.set push
	.set noreorder
	.set noat

	.extern New_LoadExecPS2
	.extern New_SifSetDma

	.globl	_LoadExecPS2
	.globl	_SifSetDma


	.ent	_LoadExecPS2
_LoadExecPS2:

#ifdef LOAD_EECORE_DOWN
	lui	$v0, 0x0170
	//ori	$v0, $v0, 0x0000
#else
	lui	$v0, 0x000e
	ori	$v0, $v0, 0x7000
#endif

	jal	New_LoadExecPS2
	daddu	$sp, $v0, $zero

	.end	_LoadExecPS2


	.ent	_SifSetDma
_SifSetDma:

	addiu	$sp, $sp, -0x20
	sd	$ra, 0x0000($sp)
	sd	$s0, 0x0010($sp)

	daddu	$s0, $sp, $zero

#ifdef LOAD_EECORE_DOWN
	lui	$v0, 0x0170
	//ori	$v0, $v0, 0x0000
#else
	lui	$v0, 0x000e
	ori	$v0, $v0, 0x7000
#endif
	daddu	$sp, $v0, $zero

	jal	New_SifSetDma
	nop

	daddu	$sp, $s0, $zero

	ld	$s0, 0x0010($sp)
	ld	$ra, 0x0000($sp)
	jr	$ra
	addiu	$sp, $sp, 0x20

	.end	_SifSetDma


	.set pop

