/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

  The goal of the code in this file is to force syscall hooks to use
  an alternate stack when they need to use stack.  
*/

#include <ee_cop0_defs.h>
#include <syscallnr.h>

#define MAX_ARGS 	15
#define ARGBUF_SIZE 	580

	.set push
	.set noreorder
	.set noat


/**************************************************************************
 *
 * .text section
 *
 **************************************************************************/

	.text

	/* libkernel */
	.extern memset
	.extern memcpy
	.extern strlen
	.extern strncpy

	/* syshook.c */
	.extern g_argc
	.extern g_argv
	.extern t_loadElf
	.extern New_SifSetDma

	.globl  g_argbuf
	.globl	_LoadExecPS2
	.globl	Hook_LoadExecPS2
	.globl	_SifSetDma
	.globl	Hook_SifSetDma


/*
 * _LoadExecPS2: this function preserves args and change stack pointer
 * before to execute t_loadElf
 */
	.ent	_LoadExecPS2
_LoadExecPS2:

	/* save args */
	daddu	$s0, $a0, $zero /* ELF path 	*/
	daddu	$s1, $a1, $zero /* argc 	*/
	daddu	$s2, $a2, $zero /* argv 	*/

	/* dark blue BG color */
	lui	$v0, 0x1200
	ori	$v0, $v0, 0x00e0
	lui	$v1, 0x0040
	sd	$v1, 0x0000($v0)

	/* disable Intr */
	mfc0	$v1, EE_COP0_Status
	lui	$v0, 0x0010
	and	$v1, $v1, $v0
	daddu	$a0, $zero, $zero
	beq	$v1, $zero, 2f
	nop
1:
	di
	sync.p
	mfc0	$v1, EE_COP0_Status
	nop	
	and	$v1, $v1, $v0
	nop
	bne	$v1, $zero, 1b
	nop
2:
	/* entering Kernel mode */
	mfc0	$v0, EE_COP0_Status
	lui	$v1, 0xffff
	ori	$v1, $v1, 0xffe7
	and	$v0, $v0, $v1
	mtc0	$v0, EE_COP0_Status
	sync.p

	/* set g_argc to arg count , or MAX_ARGS if greater */
	slti	$v0, $s1, MAX_ARGS+1
	addiu	$v1, $zero, MAX_ARGS
	movn	$v1, $s1, $v0
	la	$v0, g_argc
	sw	$v1, 0x0000($v0)

	/* pointer to g_argbuf */
	la	$s3, g_argbuf

	/* clear g_argbuf */
	daddu	$a0, $s3, $zero
	daddu	$a1, $zero, $zero
	jal	memset
	addiu	$a2, $zero, ARGBUF_SIZE

	/* copy elfname to argv[0] */
	daddu	$a0, $s3, $zero
	daddu	$a1, $s0, $zero
	jal	strncpy
	addiu	$a2, $zero, ARGBUF_SIZE

	/* set g_argv[0] to g_argbuf */
	la	$v0, g_argv
	sw	$s3, 0x0000($v0)

	/* increment arg buffer pointer */
	jal	strlen
	daddu	$a0, $s0, $zero
	addiu	$v0, $v0, 1
	daddu	$s3, $s3, $v0

	/* increment g_argc */
	la	$v1, g_argc
	lw	$v0, 0x0000($v1)
	daddu	$s4, $v0, $zero /* g_argc(-1) */
	addiu	$v0, $v0, 1
	sw	$v0, 0x0000($v1)

	/* copy args from main ELF to args buffer */
	blez	$s4, 4f
	daddu	$s5, $zero, $zero /* counter */
3:
	/* get arg len and increment it */
	daddu	$a0, $s2, $zero
	sll	$v0, $s5, 2
	daddu	$a0, $a0, $v0
	jal	strlen
	lw	$a0, 0x0000($a0)
	addiu	$s6, $v0, 1 /* arg len */

	/* copy arg to main args buffer */
	daddu	$a0, $s3, $zero
	daddu	$a1, $s2, $zero
	sll	$v0, $s5, 2
	daddu	$a1, $a1, $v0
	lw	$a1, 0x0000($a1)
	jal	memcpy
	daddu	$a2, $s6, $zero

	/* store arg pointer to g_argv[counter+1] */
	la	$v0, g_argv
	sll	$v1, $s5, 2
	daddu	$v0, $v0, $v1
	sw	$s3, 0x0004($v0)

	/* increment arg buffer pointer with arg len + 1 */
	daddu	$s3, $s3, $s6

	/* loop on every args */
	addiu	$s5, $s5, 1
	slt	$v1, $s5, $s4
	bne	$v1, $zero, 3b
	nop
4:
	/* exiting Kernel mode */
	mfc0	$v0, EE_COP0_Status
	ori	$v0, $v0, 0x0010
	mtc0	$v0, EE_COP0_Status
	sync.p

	/* enable Intr */
	ei

	/* dark olive BG color */
	lui	$v0, 0x1200
	ori	$v0, $v0, 0x00e0
	lui	$v1, 0x0040
	ori	$v1, $v1, 0x4000
	sd	$v1, 0x0000($v0)

	/* FlushCache */
	daddu	$a0, $zero, $zero
	addiu	$v1, $zero, __NR_FlushCache
	syscall
	addiu	$a0, $zero, 2
	addiu	$v1, $zero, __NR_FlushCache
	syscall

	/* change stack pointer to top of user memory */
	lui	$v0, 0x0200
	daddu	$sp, $v0, $zero

	/*
	 * ExecPS2() does the following for us:
	 * - do a soft EE peripheral reset
	 * - terminate all threads and delete all semaphores
	 * - set up ELF loader thread and run it
	 */
	la 	$a0, t_loadElf
	daddu	$a1, $zero, $zero
	daddu	$a2, $zero, $zero
	daddu	$a3, $zero, $zero
	addiu	$v1, $zero, __NR_ExecPS2
	syscall
	nop

	.end	_LoadExecPS2

/*
 * Hook_LoadExecPS2: exit syscall to _LoadExecPS2
 */
	.ent	Hook_LoadExecPS2
Hook_LoadExecPS2:

	la $v1, _LoadExecPS2
	sw $v1, 8($sp)
	jr $ra
	nop

	.end	Hook_LoadExecPS2

/*
 * _SifSetDma
 */
	.ent	_SifSetDma
_SifSetDma:

	addiu	$sp, $sp, -0x10
	sd	$ra, 0x0000($sp)
	sd	$s0, 0x0008($sp)

	daddu	$s0, $sp, $zero

#ifdef LOAD_EECORE_DOWN
	lui	$v0, 0x0170
	/* ori	$v0, $v0, 0x0000 */
#else
	lui	$v0, 0x000e
	ori	$v0, $v0, 0x7000
#endif

	jal	New_SifSetDma
	daddu	$sp, $v0, $zero

	daddu	$sp, $s0, $zero

	ld	$s0, 0x0008($sp)
	ld	$ra, 0x0000($sp)
	jr	$ra
	addiu	$sp, $sp, 0x10

	.end	_SifSetDma

/*
 * Hook_SifSetDma: exit syscall to _SifSetDma
 */
	.ent	Hook_SifSetDma
Hook_SifSetDma:

	lw	$v1, 0x0c($a0)
	addiu	$v0, $zero, 0x44
	bne	$v0, $v1, 2f

	lw	$v1, 0x08($a0)
	addiu	$v0, $zero, 0x68
	beq	$v0, $v1, 1f

	addiu	$v0, $zero, 0x70
	bne	$v0, $v1, 2f
1:
	lw	$a2, 0x00($a0)
	lbu 	$v0, 0x00($a2)
	bne	$v0, $v1, 2f

	lui 	$a3, 0x8000
	ori 	$a3, $a3, 0x0003
	lw 	$v0, 0x08($a2)
	bne	$v0, $a3, 2f
	nop

	la	$v1, _SifSetDma
	jr 	$ra
	sw 	$v1, 0x08($sp)
2:
	lw	$v0, Old_SifSetDma
	jr 	$v0
	nop

	.end	Hook_SifSetDma


/**************************************************************************
 *
 * COMMON section
 *
 **************************************************************************/

	.section COMMON
g_argbuf:
	.space	(ARGBUF_SIZE)


	.set pop

