/*
 * Low-level cheat engine
 *
 * Copyright (C) 2009-2010 Mathias Lafeldt <misfire@debugon.org>
 *
 * This file is part of PS2rd, the PS2 remote debugger.
 *
 * PS2rd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PS2rd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PS2rd.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id$
 */

#define ABI_EABI64 // force all register names to EABI64 (legacy toolchain)
#include "as_reg_compat.h"

#define F_EngineHandler
#define F_CodeHandler

#define F_HookSetupThread

/* Max number of hooks in hook list */
.equ MAX_HOOKS,5
/* Max number of codes in code list */
.equ MAX_CODES,250

	.text

	.set	push
	.set	noreorder
	.set	noat

	/*
	 * All global symbols are exported when built as ERL.
	 */
	.globl	EngineHandler
	.globl	jal_EngineHandler
	.globl	eh_backup
	.globl	CodeHandler

	.globl	HookSetupThread
	.globl	Old_SetupThread
	.globl	j_SetupThread
	.globl	numhooks
	.globl	hooklist

	.globl	numcodes
	.globl	codelist

	.globl	engine_info

/*******************************************************************************
 *
 * EngineHandler - Savely call the code handler (and other external functions).
 *
 * This function needs to be called many times a second, e.g. from scePadRead().
 *
 ******************************************************************************/
	.ent	EngineHandler
jal_EngineHandler:
	jal	EngineHandler
	nop
EngineHandler:
	addiu	$sp, -0xe0
	sd	$ra, 0($sp)
	sd	$at, 0x08($sp)
	sd	$v0, 0x10($sp)
	sd	$v1, 0x18($sp)
	sd	$a0, 0x20($sp)
	sd	$a1, 0x28($sp)
	sd	$a2, 0x30($sp)
	sd	$a3, 0x38($sp)
	sd	$t0, 0x40($sp)
	sd	$t1, 0x48($sp)
	sd	$t2, 0x50($sp)
	sd	$t3, 0x58($sp)
	sd	$t4, 0x60($sp)
	sd	$t5, 0x68($sp)
	sd	$t6, 0x70($sp)
	sd	$t7, 0x78($sp)
	sd	$s0, 0x80($sp)
	sd	$s1, 0x88($sp)
	sd	$s2, 0x90($sp)
	sd	$s3, 0x98($sp)
	sd	$s4, 0xa0($sp)
	sd	$s5, 0xa8($sp)
	sd	$s6, 0xb0($sp)
	sd	$s7, 0xb8($sp)
	sd	$t8, 0xc0($sp)
	sd	$t9, 0xc8($sp)
	sd	$gp, 0xd0($sp)
	sd	$fp, 0xd8($sp)

eh_jal:
	jal	CodeHandler
	nop
	ld	$fp, 0xd8($sp)
	ld	$gp, 0xd0($sp)
	ld	$t9, 0xc8($sp)
	ld	$t8, 0xc0($sp)
	ld	$s7, 0xb8($sp)
	ld	$s6, 0xb0($sp)
	ld	$s5, 0xa8($sp)
	ld	$s4, 0xa0($sp)
	ld	$s3, 0x98($sp)
	ld	$s2, 0x90($sp)
	ld	$s1, 0x88($sp)
	ld	$s0, 0x80($sp)
	ld	$t7, 0x78($sp)
	ld	$t6, 0x70($sp)
	ld	$t5, 0x68($sp)
	ld	$t4, 0x60($sp)
	ld	$t3, 0x58($sp)
	ld	$t2, 0x50($sp)
	ld	$t1, 0x48($sp)
	ld	$t0, 0x40($sp)
	ld	$a3, 0x38($sp)
	ld	$a2, 0x30($sp)
	ld	$a1, 0x28($sp)
	ld	$a0, 0x20($sp)
	ld	$v1, 0x18($sp)
	ld	$v0, 0x10($sp)
	ld	$at, 0x08($sp)

	/* call original function here */
eh_backup:
	nop
	nop

	/* finally, return to normal execution */
	ld	$ra, 0($sp)
	jr	$ra
	addiu	$sp, 0xe0
	.end	EngineHandler

/*******************************************************************************
 *
 * CodeHandler - Process all codes in the code list and handle the different
 * code types.
 *
 ******************************************************************************/
	.ent	CodeHandler
CodeHandler:
	/*
	 * $t0 - ptr to code list
	 * $t1 - ptr to jump table for code types
	 * $t3 - total number of codes
	 * $t4 - number of codes done
	 */
	lw	$t3, numcodes
	beqz	$t3, ch_ret
	li	$t4, 0
	la	$t0, codelist
	la	$t1, type_tab
ch_loop:
	/*
	 * $t2 - ptr to current code
	 * $a0 - RAM address
	 * $a1 - code value
	 */
	sll	$at, $t4, 3
	addu	$t2, $t0, $at
	lw	$a0, 0($t2)
	beqz	$a0, next
	srl	$a2, $a0, 28
	sll	$a2, 2
	addu	$a2, $t1
	lw	$t5, 0($a2)
	beqz	$t5, next
	sll	$a0, 7
	srl	$a0, 7

	/* jump to code type handler */
	jr	$t5
	lw	$a1, 4($t2)
type_0:
	/*
	 * "8-bit constant write"
	 *
	 * 0-aaaaaaa 000000vv
	 *
	 * Constantly writes the 8-bit value @v to address @a.
	 */
	b	next
	sb	$a1, 0($a0)
type_1:
	/*
	 * "16-bit constant write"
	 *
	 * 1-aaaaaaa 0000vvvv
	 *
	 * Constantly writes the 16-bit value @v to address @a.
	 */
	b	next
	sh	$a1, 0($a0)
type_2:
	/*
	 * "32-bit constant write"
	 *
	 * 2-aaaaaaa vvvvvvvv
	 *
	 * Constantly writes the 32-bit value @v to address @a.
	 */
	b	next
	sw	$a1, 0($a0)
type_3:
	/*
	 * "Increment / Decrement"
	 *
	 * 8-bit increment
	 * 3-00000vv 0aaaaaaa
	 *
	 * 8-bit decrement
	 * 3-01000vv 0aaaaaaa
	 *
	 * 16-bit increment
	 * 3-020vvvv 0aaaaaaa
	 *
	 * 16-bit decrement
	 * 3-030vvvv 0aaaaaaa
	 *
	 * 32-bit increment
	 * 3-0400000 0aaaaaaa
	 * vvvvvvvv 00000000
	 *
	 * 32-bit decrement
	 * 3-0500000 0aaaaaaa
	 * vvvvvvvv 00000000
	 *
	 * It increments/decrements the current value at address @a by value @v.
	 */
	srl	$t6, $a0, 20
	andi	$at, $t6, 1	/* 0=inc, 1=dec */
	andi	$a0, 0xffff
	sll	$a1, 7
	srl	$a1, 7
	andi	$t6, 6		/* zero for 8 bit */
	andi	$a2, $t6, 2	/* non-zero for 16 bit */
	beqzl	$t6, 1f
	lbu	$t5, 0($a1)
	bnezl	$a2, 1f
	lhu	$t5, 0($a1)
	lw	$t5, 0($a1)
	lw	$a0, 8($t2)
	addiu	$t4, 1
1:
	beqzl	$at, 2f
	addu	$t5, $a0
	subu	$t5, $a0
2:
	beqzl	$t6, next
	sb	$t5, 0($a1)
	bnezl	$a2, next
	sh	$t5, 0($a1)
	b	next
	sw	$t5, 0($a1)
type_4:
	/*
	 * "32-bit constant serial write"
	 *
	 * 4-aaaaaaa nnnnssss
	 * vvvvvvvv iiiiiiii
	 *
	 * Starting with address @a, this code type will write the 32-bit value
	 * @v to @n addresses. In each cycle, the address is incremented by
	 * @s * 4 and the value is incremented by @i.
	 */
	lw	$a2, 8($t2)
	lw	$a3, 12($t2)
	srl	$t5, $a1, 16
	andi	$a1, 0xffff
	sll	$a1, 2
1:
	nop
	nop
	sw	$a2, 0($a0)
	addu	$a2, $a3
	addiu	$t5, -1
	bgtz	$t5, 1b
	addu	$a0, $a1
	b	next
	addiu	$t4, 1
type_5:
	/*
	 * "Copy bytes"
	 *
	 * 5-sssssss nnnnnnnn
	 * 0ddddddd 00000000
	 *
	 * Copies a block of @n bytes from source address @s to destination
	 * address @d.
	 */
	lw	$a2, 8($t2)
1:
	lb	$t5, 0($a0)
	nop
	sb	$t5, 0($a2)
	addiu	$a2, 1
	addiu	$a1, -1
	bgtz	$a1, 1b
	addiu	$a0, 1
	b	next
	addiu	$t4, 1
type_6:
	/*
	 * "Pointer write"
	 *
	 * 8-bit write
	 * 6-aaaaaaa 000000vv
	 * 00000000 iiiiiiii
	 *
	 * 16-bit write
	 * 6-aaaaaaa 0000vvvv
	 * 00010000 iiiiiiii
	 *
	 * 32-bit write
	 * 6-aaaaaaa vvvvvvvv
	 * 00020000 iiiiiiii
	 *
	 * Loads 32-bit base address from address @a, adds offset @i to it, and
	 * constantly writes the value @v to the final address.
	 */
	lw	$t5, 0($a0)
	li	$at, 0x3ffffffc
	and	$t5, $at
	beqz	$t5, 1f
	lhu	$a2, 10($t2)
	lw	$a3, 12($t2)
	addu	$t5, $a3
	beqzl	$a2, 1f
	sb	$a1, 0($t5)
	addiu	$a2, -1
	beqzl	$a2, 1f
	sh	$a1, 0($t5)
	sw	$a1, 0($t5)
1:
	b	next
	addiu	$t4, 1
type_7:
	/*
	 * "Boolean operation"
	 *
	 * 8-bit OR
	 * 7-aaaaaaa 000000vv
	 *
	 * 16-bit OR
	 * 7-aaaaaaa 0010vvvv
	 *
	 * 8-bit AND
	 * 7-aaaaaaa 002000vv
	 *
	 * 16-bit AND
	 * 7-aaaaaaa 0030vvvv
	 *
	 * 8-bit XOR
	 * 7-aaaaaaa 004000vv
	 *
	 * 16-bit XOR
	 * 7-aaaaaaa 0050vvvv
	 *
	 * Performs a bitwise logical operation between value @v and the value
	 * stored at address @a.
	 */
	srl	$t6, $a1, 20
	andi	$at, $t6, 1	/* 0=8 bit, 1=16 bit */
	andi	$t6, 6		/* 0=OR, 2=AND, 4=XOR */
	bnezl	$at, 1f
	lhu	$t5, 0($a0)
	lbu	$t5, 0($a0)
1:
	beqzl	$t6, 2f
	or	$t5, $a1
	addiu	$t6, -2
	beqzl	$t6, 2f
	and	$t5, $a1
	xor	$t5, $a1
2:
	bnezl	$at, next
	sh	$t5, 0($a0)
	b	next
	sb	$t5, 0($a0)
type_c:
	/*
	 * "32-bit do all following codes if equal to"
	 *
	 * C-aaaaaaa vvvvvvvv
	 *
	 * All following codes will be executed only if 32-bit value at address
	 * @a is equal to value @v.
	 */
	lw	$t5, 0($a0)
	bne	$t5, $a1, ch_ret
	nop
	b	next
	nop
type_d:
	/*
	 * "Do multi-lines if conditional"
	 *
	 * 16-bit test
	 * D-aaaaaaa nnt0vvvv
	 *
	 * 8-bit test
	 * D-aaaaaaa nnt100vv
	 *
	 * Compares value at address @a to value @v, and executes next @n code
	 * lines only if the test condition @t is true. Values for @t are:
	 *   0 equal      1 not equal
	 *   2 less than  3 greater than
	 *   4 NAND       5 AND
	 *   6 NOR        7 OR
	 */
	srl	$a3, $a1, 16
	andi	$a3, 1
	srl	$t6, $a1, 20
	srl	$a2, $a1, 24
	beqzl	$a2, 1f		/* set @n to 1 if 0 for compatibility */
	li	$a2, 1
1:
	beqz	$a3, 2f
	andi	$t6, 7
	lbu	$t5, 0($a0)
	b	3f
	andi	$a1, 0xff
2:
	lhu	$t5, 0($a0)
	andi	$a1, 0xffff
3:
	beqzl	$t6, 4f		/* equal */
	subu	$t5, $a1
	li	$at, 1		/* not equal */
	beql	$t6, $at, 5f
	subu	$t5, $a1
	li	$at, 2		/* less than */
	beql	$t6, $at, 5f
	sltu	$t5, $a1
	li	$at, 3		/* greater than */
	beql	$t6, $at, 5f
	sltu	$t5, $a1, $t5
	li	$at, 4		/* NAND */
	beql	$t6, $at, 4f
	and	$t5, $a1
	li	$at, 5		/* AND */
	beql	$t6, $at, 5f
	and	$t5, $a1
	li	$at, 6		/* NOR */
	beql	$t6, $at, 4f
	or	$t5, $a1
	b	5f		/* OR */
	or	$t5, $a1
4:				/* skip if non-zero */
	bnezl	$t5, next
	addu	$t4, $a2
	b	next
	nop
5:				/* skip if zero */
	beqzl	$t5, next
	addu	$t4, $a2
	b	next
	nop
type_e:
	/*
	 * "Do multi-lines if conditional (deprecated)"
	 *
	 * 16-bit test
	 * E-0nnvvvv taaaaaaa
	 *
	 * 8-bit test
	 * E-1nn00vv taaaaaaa
	 *
	 * This type is internally converted to the D type - see above.
	 */
	andi	$a2, $a0, 0xffff
	srl	$at, $a1, 28
	sll	$at, 20
	or	$a2, $at
	srl	$at, $a0, 24
	sll	$at, 16
	or	$a2, $at
	srl	$at, $a0, 16
	sll	$at, 24
	or	$a2, $at
	sll	$a0, $a1, 7
	srl	$a0, 7
	li	$t5, 0xd0000000
	or	$t5, $a0
	move	$a1, $a2
	sw	$t5, 0($t2)	/* store converted address */
	b	type_d
	sw	$a1, 4($t2)	/* store converted value */
next:
	/* next code */
	addiu	$t4, 1
	sltu	$at, $t4, $t3
	bnez	$at, ch_loop
	nop
ch_ret:
	jr	$ra
	nop
	.end	CodeHandler

	/* jump table for code types */
type_tab:
	.word	type_0
	.word	type_1
	.word	type_2
	.word	type_3
	.word	type_4
	.word	type_5
	.word	type_6
	.word	type_7
	.word	0 /* TODO */
	.word	0 /* type 9 is a hook code */
	.word	0 /* TODO */
	.word	0 /* TODO */
	.word	type_c
	.word	type_d
	.word	type_e
	.word	0 /* type F is a hook code */

/*******************************************************************************
 *
 * HookSetupThread - Pre-call hook function for syscall SetupThread().
 *
 * It runs through all hooks in the hook list and hard-codes a jal to
 * EngineHandler() if a hook's value is equal to the opcode at the hook address;
 * replaced opcodes are backed up. After processing the boot codes, it calls
 * the original SetupThread() function.
 *
 * NOTE: Only the _first_ matching hook will be activated.
 *
 * Here are the syscall parameters - those regs must not be modified!
 *
 * void *SetupThread(void *gp,	$a0
 *	void *stack,		$a1
 *	s32 stack_size,		$a2
 *	void *args,		$a3
 *	void *root_func)	$t0
 *
 ******************************************************************************/
	.ent	HookSetupThread
HookSetupThread:
	addiu	$sp, -0x10
	sd	$ra, 0($sp)

	/*
	 * $t1 - number of hooks to do
	 * $t3 - ptr to hook list entry
	 * $v0 - hook address
	 */
	lw	$t1, numhooks
	beqz	$t1, 3f
	nop
	la	$t3, hooklist
1:
	/* hook only if opcodes are equal */
	lw	$v0, 0($t3)
	beqz	$v0, 2f
	lw	$v1, 4($t3)
	lw	$t2, 0($v0)
	bne	$t2, $v1, 2f
	nop

	/* back up opcodes */
	la	$at, eh_backup
	sw	$t2, 0($at)
	lw	$v1, 4($v0)
	sw	$v1, 4($at)

	/* inject jal to engine handler */
	lw	$v1, jal_EngineHandler
	sw	$v1, 0($v0)
	b	3f
	sw	$zero, 4($v0)
2:
	/* next hook */
	addiu	$t1, -1
	bgtz	$t1, 1b
	addiu	$t3, 8
3:
	ld	$ra, 0($sp)
	addiu	$sp, 0x10

	/* jump to original SetupThread() */
	lw	$t1, Old_SetupThread		# $t1 -> original SetupThread function
	j	$t1
	nop 
	.end	HookSetupThread

/*******************************************************************************
 *
 * .rodata section
 *
 ******************************************************************************/
	.section .rodata

/*
 * Some information about the engine used by the loader.
 */
engine_info:
	.word	0x00000002 /* version 0.2 */

/*******************************************************************************
 *
 * .bss section
 *
 ******************************************************************************/
	.section .bss

/*
 * original SetupThread address
 */
Old_SetupThread:
	.word	0

/*
 * Hook list to be filled by loader.
 */
numhooks:
	.word	0
hooklist:
	.word	0:(MAX_HOOKS*2)

/*
 * Code list to be filled by loader.
 */
numcodes:
	.word	0
codelist:
	.word	0:(MAX_CODES*2)

	.set	pop

/* EOF */
