/*
 Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
 Licenced under Academic Free License version 3.0
 Review Open-PS2-Loader README & LICENSE files for further details.

 code to be patched into the ExecPS2 syscall on SCPH-10000
*/

	.text

	.set	push
	.set	noreorder
	.set	noat

	.globl p_program_thid
	.globl p_800125f4
	.globl p_elfargs
	.globl p_80017400
	.globl p_ChangeThreadPriority
	.globl p_CancelWakeUpThread
	.globl p_TerminateThread
	.globl p_DeleteThread
	.globl p_kFNC80004e68
	.globl p_kFNC800021b0
	.globl p_bcopy
	.globl p_kFNC80002ac0
	.globl p_kFNC80002a80
	.globl p_InitializeGS
	.globl p_SetGSCrt
	.globl p_InitializeINTC
	.globl p_InitializeTIMER
	.globl p_ResetEE
	.globl p_InitializeFPU
	.globl p_InitializeScratchPad

	.globl	SoftPeripheralEEReset
	.globl	ExecPS2CleanUp

/*
 * SoftPeripheralEEReset
 */
	.ent	SoftPeripheralEEReset
SoftPeripheralEEReset:
	addiu	$sp, $sp, -0x10
	sd	$ra, 0x0000($sp)

	/* INTC reg stuff */
	lui	$a0, 0x1000
	ori	$v0, $a0, 0xf000
	addiu	$v1, $zero, 4
	sw	$v1, 0x0000($v0)
	ori	$a0, $a0, 0xf000
1:
	lw	$v0, 0x0000($a0)
	andi	$v0, $v0, 4
	nop
	nop
	nop
	beq	$v0, $zero, 1b
	nop

	/* Initialize GS */
	sw	$v1, 0x0000($a0)
	lw	$v0, p_InitializeGS
	jalr	$v0
	nop

	/* SetGSCrt mode */
	lw	$v0, p_SetGSCrt
	addiu	$a0, $zero, 1
	addiu	$a1, $zero, 2
	jalr	$v0
	addiu	$a2, $zero, 1

	/* Initialize INTC */
	lw	$v0, p_InitializeINTC
	jalr	$v0
	ori	$a0, $zero, 0xdffd

	/* Initialize TIMER */
	lw	$v0, p_InitializeTIMER
	jalr	$v0
	nop

	/* Do a ResetEE (initialize DMAC, VU0, VU1, GIF, VIF0, VIF1, IPU) */
	lw	$v0, p_ResetEE
	jalr	$v0
	addiu	$a0, $zero, 0x7f

	/* Initialize FPU */
	lw	$v0, p_InitializeFPU
	jalr	$v0
	nop

	/* Initialize Scratch Pad */
	lw	$v0, p_InitializeScratchPad
	jalr	$v0
	nop

	ld	$ra, 0x0000($sp)
	jr	$ra
	addiu	$sp, $sp, 0x10
	.end	SoftPeripheralEEReset


/*
 * ExecPS2CleanUp
 */
	.ent	ExecPS2CleanUp
ExecPS2CleanUp:
	addiu	$sp, $sp, -0xB0
	sd	$ra, 0x00a0($sp)
	sd	$fp, 0x0090($sp)
	sd	$s7, 0x0080($sp)
	sd	$s6, 0x0070($sp)
	sd	$s5, 0x0060($sp)
	sd	$s4, 0x0050($sp)
	sd	$s3, 0x0040($sp)
	sd	$s2, 0x0030($sp)
	sd	$s1, 0x0020($sp)
	sd	$s0, 0x0010($sp)

	addiu	$fp, $zero, 16
	daddu	$s5, $a0, $zero		/* store elf entry point into s5 */
	daddu	$s4, $a2, $zero		/* store argc into s4 */
	addiu	$s1, $zero, 0x4c
	addiu	$s0, $zero, 1

	sw	$a1, 0x0000($sp)	/* store gp pointer */
	sw	$a3, 0x0004($sp)	/* store argv */

	lw	$t0, p_program_thid
	lw	$s3, 0x0000($t0)

	lw	$v0, p_CancelWakeUpThread
	jalr	$v0
	daddu	$a0, $s3, $zero

	daddu	$a0, $s3, $zero
	lw	$v0, p_ChangeThreadPriority
	jalr	$v0
	daddu	$a1, $zero, $zero
	nop

	/* terminate all threads */
1:
	lw	$v0, p_80017400
	addu	$v0, $s1, $v0
	lw	$v0, 0x0008($v0)
	beql	$v0, $zero, 3f
	addiu	$s0, $s0, 1
	beql	$s0, $s3, 3f
	addiu	$s0, $s0, 1
	bne	$v0, $fp, 2f
	nop
	lw	$v0, p_DeleteThread
	jalr	$v0
	daddu	$a0, $s0, $zero
	beq	$zero, $zero, 3f
	addiu	$s0, $s0, 1
2:
	lw	$v0, p_TerminateThread
	jalr	$v0
	daddu	$a0, $s0, $zero
	lw	$v0, p_DeleteThread
	jalr	$v0
	daddu	$a0, $s0, $zero
	addiu	$s0, $s0, 1
3:
	slti	$v0, $s0, 256
	bne	$v0, $zero, 1b
	addiu 	$s1, $s1, 0x4c

	lw	$v0, p_kFNC80004e68
	jalr	$v0
	daddu	$s0, $zero, $zero

	lw	$v0, p_kFNC800021b0
	jalr	$v0
	nop

	/* Does a soft pheripheral EE reset */
	lw	$v0, p_800125f4
	jal	SoftPeripheralEEReset
	sw	$zero, 0x0000($v0)

	/* copy elf args to kernel memory */
	blez	$s4, 5f
	nop

	lw	$a0, p_elfargs		/* pointer where to store elf args in kernel memory */
	lw	$a1, 0x0004($sp)
4:
	sll	$v1, $s0, 2
	addiu 	$s0, $s0, 1
	addu	$v1, $v1, $a1
	lw	$v0, p_bcopy
	jalr	$v0
	lw	$a1, 0x0000($v1)
	daddu 	$a0, $v0, $zero
	slt	$v0, $s0, $s4
	bne	$v0, $zero, 4b
	lw	$a1, 0x0004($sp)

	/* */
5:
	addiu	$v0, $zero, 0x4c
	mult	$v0, $s3, $v0
	lw	$a1, p_80017400
	lw	$a0, p_elfargs
	lw	$a2, p_kFNC80002ac0
	addu	$v0, $v0, $a1
	sw	$a0, 0x0038($v0)
	lw	$a1, 0x0000($sp)
	sw	$s4, 0x0034($v0)
	sw	$s5, 0x000c($v0)
	sw	$s5, 0x0030($v0)
	sw	$a1, 0x0014($v0)
	sh	$zero, 0x001a($v0)
	sh	$zero, 0x0018($v0)
	sw	$zero, 0x0024($v0)
	sw	$zero, 0x001c($v0)
	jalr	$a2
	sw	$zero, 0x0020($v0)
	
	lw	$v1, p_kFNC80002a80
	jalr	$v1
	nop

	/* return entry point */
	daddu	$v0, $s5, $zero

	ld	$s0, 0x0010($sp)
	ld	$s1, 0x0020($sp)
	ld	$s2, 0x0030($sp)
	ld	$s3, 0x0040($sp)
	ld	$s4, 0x0050($sp)
	ld	$s5, 0x0060($sp)
	ld	$s6, 0x0070($sp)
	ld	$s7, 0x0080($sp)
	ld	$fp, 0x0090($sp)
	ld	$ra, 0x00a0($sp)
	jr	$ra
	addiu	$sp, $sp, 0xB0
	.end	ExecPS2CleanUp


/*
 * rodata section
 */

	.section .rodata

p_program_thid:
	.word	0x800125ec

p_800125f4:
	.word	0x800125f4

p_elfargs:
	.word	0x80012608

p_80017400:
	.word	0x80017400

p_CancelWakeUpThread:
	.word	0x80004970

p_ChangeThreadPriority:
	.word	0x80004288

p_TerminateThread:
	.word	0x80003e00

p_DeleteThread:
	.word	0x80003f00

p_kFNC80004e68:
	.word	0x80004e68

p_kFNC800021b0:
	.word	0x800021b0

p_bcopy:
	.word	0x80005560

p_kFNC80002ac0:
	.word	0x80002ac0

p_kFNC80002a80:
	.word	0x80002a80

p_InitializeGS:
	.word	0x8000aa60

p_SetGSCrt:
	.word	0x8000a060

p_InitializeINTC:
	.word	0x8000b8d0

p_InitializeTIMER:
	.word	0x8000b900

p_ResetEE:
	.word	0x8000ad68

p_InitializeFPU:
	.word	0x8000b7a8

p_InitializeScratchPad:
	.word	0x8000b840

	.set	pop
