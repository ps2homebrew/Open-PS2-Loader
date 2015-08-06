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

#define COMPAT_MODE_3 	0x04 // Unhook Syscalls
#define COMPAT_MODE_6 	0x20 // Disable IGR

#define PADOPEN_HOOK  0

	.set push
	.set noreorder
	.set noat

/**************************************************************************
 *
 * .text section
 *
 **************************************************************************/

	.text

	.extern _end

	/* libkernel */
	.extern memset
	.extern memcpy
	.extern strlen
	.extern strncpy

	/* syshook.c */
	.extern g_argc
	.extern g_argv
	.extern t_loadElf
	.extern set_reg_hook
	.extern iop_reboot_count
	.extern New_SifSetDma
	.extern Old_SifSetDma
	.extern Old_SifSetReg
	.extern padOpen_hooked
	.extern disable_padOpen_hook
	.extern Old_ExecPS2
	.extern Old_CreateThread

	/* loader.h */
	.extern g_compat_mask
	.extern DisableDebug

	/* padhook.c */
	.extern Install_PadOpen_Hook

	.globl  g_argbuf
	.globl	Hook_LoadExecPS2
	.globl	Hook_SifSetDma
	.globl	Hook_SifSetReg
	.globl	Hook_ExecPS2
	.globl	Hook_CreateThread


/*
 * _LoadExecPS2: this function preserves args and change stack pointer
 * before to execute t_loadElf
 */
	.ent	_LoadExecPS2
_LoadExecPS2:
	/* Set disable_padOpen_hook */
	la	$v1, disable_padOpen_hook
	addiu	$v0, $zero, 1
	sw	$v0, 0x0000($v1)

	/* save args, not needed to preserve sX/ra registers values as we won't return */
	daddu	$s0, $a0, $zero /* ELF path 	*/
	daddu	$s1, $a1, $zero /* argc 	*/
	daddu	$s2, $a2, $zero /* argv 	*/

	lw	$v0, DisableDebug
	bne	$v0, $zero, 1f
	nop

	/* dark blue BG color */
	lui	$v0, 0x1200
	ori	$v0, $v0, 0x00e0
	lui	$v1, 0x0040
	sd	$v1, 0x0000($v0)

1:
	/* disable Intr */
	mfc0	$v1, EE_COP0_Status
	lui	$v0, 0x0010
	and	$v1, $v1, $v0
	daddu	$a0, $zero, $zero
	beq	$v1, $zero, 3f
	nop
2:
	di
	sync.p
	mfc0	$v1, EE_COP0_Status
	nop
	and	$v1, $v1, $v0
	nop
	bne	$v1, $zero, 2b
	nop
3:
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
	blez	$s4, 5f
	daddu	$s5, $zero, $zero /* counter */
4:
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
	bne	$v1, $zero, 4b
	nop
5:
	/* exiting Kernel mode */
	mfc0	$v0, EE_COP0_Status
	ori	$v0, $v0, 0x0010
	mtc0	$v0, EE_COP0_Status
	sync.p

	/* enable Intr */
	ei

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

	/* exit syscall to _LoadExecPS2 */
	la $v1, _LoadExecPS2
	sw $v1, 8($sp)
	jr $ra
	nop

	.end	Hook_LoadExecPS2

/*
 * _SifSetDma: function designed to use our own stack during IOP reboot trap
 */
	.ent	_SifSetDma
_SifSetDma:

	/* save original stack pointer */
	daddu	$a2, $sp, $zero

	/* change the stack pointer */
	la	$sp, _end

	/* call New_SifSetDma, preserving ra and a2 registers values */
	addiu	$sp, $sp, -0x10
	sd	$ra, 0x0000($sp)
	jal	New_SifSetDma
	sd	$a2, 0x0008($sp)

	/* restore a2 and ra registers */
	ld	$a2, 0x0008($sp)
	ld	$ra, 0x0000($sp)

	/* restore the original stack pointer */
	daddu	$sp, $a2, $zero

	jr	$ra
	addiu	$v0, $zero, 1

	.end	_SifSetDma

/*
 * Hook_SifSetDma: exit syscall to _SifSetDma when IOP reboot trapped
 */
	.ent	Hook_SifSetDma
Hook_SifSetDma:

	/* check ((SifDmaTransfer_t *)$a0)->attr == 0x44 */
	lw	$v1, 0x000c($a0)
	addiu	$v0, $zero, 0x44
	bne	$v0, $v1, 2f

	/* check ((SifDmaTransfer_t *)$a0)->size == 0x68 */
	lw	$v1, 0x0008($a0)
	addiu	$v0, $zero, 0x68
	beq	$v0, $v1, 1f

	/* check ((SifDmaTransfer_t *)$a0)->size == 0x70 */
	addiu	$v0, $zero, 0x70
	bne	$v0, $v1, 2f
1:
	/* check (SifCmdResetData *)((SifDmaTransfer_t *)$a0->src)->chdr.psize == ((SifDmaTransfer_t *)$a0)->size */
	lw	$a2, 0x0000($a0)
	lw 	$v0, 0x0000($a2)
	bne	$v0, $v1, 2f

	/* check (SifCmdResetData *)((SifDmaTransfer_t *)$a0->src)->chdr.fcode == 0x80000003 */
	lui 	$a3, 0x8000
	ori 	$a3, $a3, 0x0003
	lw 	$v0, 0x0008($a2)
	bne	$v0, $a3, 2f
	nop

	/* exit syscall to _SifSetDma */
	la	$v1, _SifSetDma
	jr 	$ra
	sw 	$v1, 0x0008($sp)
2:
	/* call & return with original SifSetDma */
	lw	$v0, Old_SifSetDma
	jr 	$v0
	nop

	.end	Hook_SifSetDma

/*
 * _Apply_Mode3: unhook SifSetDma/SifSetReg
 */
	.ent	_Apply_Mode3
_Apply_Mode3:

	/* save original stack pointer */
	daddu	$a0, $sp, $zero

	/* change the stack pointer */
	la	$sp, _end

	/* preserving ra and a0 registers values */
	addiu	$sp, $sp, -0x10
	sd	$ra, 0x0000($sp)
	sd	$a0, 0x0008($sp)

	/* unhook SifSetDma */
	addiu	$a0, $zero, __NR_SifSetDma
	lw 	$a1, Old_SifSetDma
	addiu	$v1, $zero, __NR_SetSyscall
	syscall

	/* unhook SifSetReg */
	addiu	$a0, $zero, __NR_SifSetReg
	lw 	$a1, Old_SifSetReg
	addiu	$v1, $zero, __NR_SetSyscall
	syscall

	/* FlushCache */
	daddu	$a0, $zero, $zero
	addiu	$v1, $zero, __NR_FlushCache
	syscall
	addiu	$a0, $zero, 2
	addiu	$v1, $zero, __NR_FlushCache
	syscall

	/* restore a0 and ra registers */
	ld	$a0, 0x0008($sp)
	ld	$ra, 0x0000($sp)

	/* restore the original stack pointer */
	daddu	$sp, $a0, $zero

	jr	$ra
	addiu	$v0, $zero, 1

	.end	_Apply_Mode3

/*
 * Hook_SifSetReg: disable SifSetReg and unhook SifSetDma/SifSetReg when needed
 */
	.ent	Hook_SifSetReg
Hook_SifSetReg:

	/* load set_reg_hook counter to a2 */
	la	$a2, set_reg_hook
	lw	$v0, 0x0000($a2)

	/* check set_reg_hook is != 0, otherwise execute normal SifSetReg */
	bne	$v0, $zero, 1f
	addiu	$v0, $v0, -1
	lw	$v0, Old_SifSetReg
	jr 	$v0
	nop
1:
	/* decrement set_reg_hook counter by 1 */
	sw	$v0, 0x0000($a2)
	bne	$v0, $zero, 3f
	nop

	lw	$v0, DisableDebug
	bne	$v0, $zero, 2f
	nop

	/* black BG color */
	lui	$v0, 0x1200
	ori	$v0, $v0, 0x00e0
	sd	$zero, 0x0000($v0)
2:
	/* check compat mode 3 is enabled */
	lw	$v0, g_compat_mask
	andi	$v0, $v0, COMPAT_MODE_3
	beq	$v0, $zero, 3f

	/* check iop_reboot_count is == 2 */
	addiu	$v1, $zero, 2
	lw	$v0, iop_reboot_count
	bne	$v0, $v1, 3f
	nop

	/* exit syscall to _Apply_Mode3 */
	la	$v1, _Apply_Mode3
	sw	$v1, 0x0008($sp)
3:
	jr	$ra
	addiu	$v0, $zero, 1

	.end	Hook_SifSetReg

/*
 * Hook_ExecPS2:
 */
	.ent	Hook_ExecPS2
Hook_ExecPS2:

	/* not needed to preserve sX/ra registers values as ExecPS2 won't return */
	daddu	$s0, $a0, $zero
	daddu	$s1, $a1, $zero
	daddu	$s2, $a2, $zero
	daddu	$s3, $a3, $zero

	/* check entry point is >= 0x00100000 */
	lui	$v0, 0x0010
	sltu	$v0, $s0, $v0
	bne	$v0, $zero, 1f
	nop

	/* check compat mode 6 is disabled */
	lw	$v0, g_compat_mask
	andi	$v0, $v0, COMPAT_MODE_6
	bne	$v0, $zero, 1f
	nop

	/* save original stack pointer */
	daddu	$s4, $sp, $zero

	/* change the stack pointer */
	la	$sp, _end

	/* call Install_PadOpen_Hook */
	lui	$a0, 0x0010
	lui	$a1, 0x01ff
	jal	Install_PadOpen_Hook
	addiu	$a2, $zero, PADOPEN_HOOK
	la	$v1, padOpen_hooked
	sw	$v0, 0x0000($v1)

	/* restore stack pointer */
	daddu	$sp, $s4, $zero
1:
	/* call ExecPS2 */
	daddu	$a0, $s0, $zero
	daddu	$a1, $s1, $zero
	daddu	$a2, $s2, $zero
	lw	$v0, Old_ExecPS2
	jr 	$v0
	daddu	$a3, $s3, $zero

	.end	Hook_ExecPS2

/*
 * Hook_CreateThread:
 */
	.ent	Hook_CreateThread
Hook_CreateThread:
	/* check disable_padOpen_hook != 0 */
	la	$v1, disable_padOpen_hook
	lw	$v0, 0x0000($v1)
	bne	$v0, $zero, 2f
	nop

	/* check padOpen_hooked == 0 */
	la	$v1, padOpen_hooked
	lw	$v0, 0x0000($v1)
	bne	$v0, $zero, 2f

	/* check thread_param->initial_priority == 0 */
	lw	$v0, 0x0014($a0)
	beq	$v0, $zero, 1f

	/* check thread_param->initial_priority < 5 */
	slti	$v0, $v0, 5
	beq	$v0, $zero, 2f

	/* check thread_param->current_priority == 0 */
	lw	$v0, 0x0018($a0)
	bne	$v0, $zero, 2f
1:
	/* save original stack pointer */
	daddu	$a1, $sp, $zero

	/* change the stack pointer */
	la	$sp, _end

	/* preserves ra, a1 and a0 registers values */
	addiu	$sp, $sp, -0x20
	sd	$ra, 0x0000($sp)
	sd	$a1, 0x0008($sp)
	sd	$a0, 0x0010($sp)

	/* call Install_PadOpen_Hook */
	lui	$a0, 0x0010
	lui	$a1, 0x01ff
	jal	Install_PadOpen_Hook
	addiu	$a2, $zero, PADOPEN_HOOK
	la	$v1, padOpen_hooked
	sw	$v0, 0x0000($v1)

	/* restore a0, a1 and ra registers */
	ld	$a0, 0x0010($sp)
	ld	$a1, 0x0008($sp)
	ld	$ra, 0x0000($sp)

	/* restore the original stack pointer */
	daddu	$sp, $a1, $zero
2:
	/* call CreateThread */
	lw	$v0, Old_CreateThread
	jr 	$v0
	nop

	.end	Hook_CreateThread


/**************************************************************************
 *
 * COMMON section
 *
 **************************************************************************/

	.section COMMON
g_argbuf:
	.space	(ARGBUF_SIZE)


	.set pop
