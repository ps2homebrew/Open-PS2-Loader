/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (C)2001, Gustavo Scotti (gustavo@scotti.com)
# (c) 2003 Marcus R. Brown (mrbrown@0xd6.org)
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

/**
 * @file
 * EE Kernel functions
 */

#include "syscallnr_custom.h"

	.text
	.p2align 3

#ifdef USE_KMODE
#define SYSCALL(name) 		\
	.set	push;		\
	.set	noreorder;	\
	.text;			\
	.align 4;		\
	.globl	name;		\
	.type	name,@function;	\
	.ent	name,0;		\
name:	j	__syscall;	\
	li	$3, __NR_##name;\
	nop;			\
	.end	name;		\
	.size	name,.-name;	\
	.set	pop;
#else
#define SYSCALL(name) 		\
	.set	push;		\
	.set	noreorder;	\
	.text;			\
	.align 4;		\
	.globl	name;		\
	.type	name,@function;	\
	.ent	name,0;		\
name:	li	$3, __NR_##name;\
	syscall;		\
	jr	$31;		\
	nop;			\
	.end	name;		\
	.size	name,.-name;	\
	.set	pop;
#endif

#ifdef F___syscall
.set	push
.set	noreorder
.text
.align	4
.globl	__syscall
.type	__syscall,@function
.ent	__syscall,0
__syscall:
        mfc0        $2, $12
        andi        $2, $2, 0x18
        beqz        $2, _kMode
        slt         $2, $3, $0
        syscall
        jr          $31
        nop
_kMode:
        subu        $26, $0, $3
        movn        $3, $26, $2
        sll         $3, $3, 2
        lui         $26, 0x8000
        lhu         $2, 0x02F0($26)
        sll         $2, $2, 16
        lh          $26, 0x02F8($26)
        add         $2, $26
        addu        $3, $2
        lw          $26, 0x00($3)
        jr          $26
        nop
.end	__syscall
.size	__syscall,.-__syscall
.set	pop
#endif

#ifdef F_iWakeupThread
SYSCALL(iWakeupThread)
#endif
