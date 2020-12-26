# Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
# Copyright 2006-2008 Polo
# Licenced under Academic Free License version 3.0
# Review OPNPS2LD README & LICENSE files for further details.
#
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Modified crt0.s
# Remove _ps2sdk_args_parse, _ps2sdk_libc_init and _ps2sdk_libc_deinit weak functions for size optimization

#define ABI_EABI64 // force all register names to EABI64 (legacy toolchain)
#include "as_reg_compat.h"

	.weak  _init
	.type  _init, @function

	.weak  _fini
	.type  _fini, @function

	.extern	_heap_size
	.extern	_stack
	.extern	_stack_size	

	.set	noat
	.set	noreorder

	.text
	.align	2

	nop
	nop

	.globl  _start
	.ent	 _start
_start:

zerobss:
	# clear bss area

	la	$v0, _fbss
	la	$v1, _end

1:
	sltu	$at, $v0, $v1
	beq	$at, $zero, 2f
	nop
	sq	$0, ($2)
	addiu	$v0, $v0, 16
	j	1b
	nop
2:

setupthread:
	# setup current thread

	la	$a0, _gp
	la	$a1, _stack
	la	$a2, _stack_size
	la	$a3, _args
	la	$t0, _root
	move	$gp, $a0
	addiu	$v1, $zero, 60
	syscall			# SetupThread(_gp, _stack, _stack_size, _args, _root)
	move	$sp, $v0

	# initialize heap
	la	$a0, _end
	la	$a1, _heap_size
	addiu	$v1, $zero, 61
	syscall			# SetupHeap(_end, _heap_size)

	# writeback data cache
	move	$a0, $zero
	addiu	$v1, $zero, 100
	syscall			# FlushCache(0)

ctors:
	# call global constructors (weak)
	la	$t0, _init
	beqz	$t0, 1f
	nop
	jalr	$t0		# _init()
	nop
1:
	# call main
	la	$v0, _args
	lw	$a0, ($v0)
	jal	main		# main(argc, argv)
	addiu	$a1, $v0, 4

	# call _exit
	j	_exit		# _exit(retval)
	move	$a0, $v0
	.end	_start

	.align	3

	.globl	_exit
	.ent	_exit
	.text
_exit:
	move	$s0, $a0		#Preserve the return value.

dtors:
	# call global destructors (weak)
	la	$t0, _fini
	beqz	$t0, 1f
	nop
	jalr	$t0		# _fini()
	nop
1:
	move	$a0, $s0
	addiu	$v1, $zero, 4
	syscall			# Exit(retval) (noreturn)
	.end	_exit

	.ent	_root
_root:
	addiu	$v1, $zero, 35
	syscall			# ExitThread() (noreturn)
	.end	_root
	.bss
	.align	6
_args:
	.space	4+8*4+128	# argc, 16 arguments, 256 bytes payload

