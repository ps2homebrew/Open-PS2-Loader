#include "as_reg_compat.h"

.set noreorder

.text

.extern _lock
.extern _unlock

.globl _lock_RpcFunc
.ent _lock_RpcFunc
_lock_RpcFunc:
	#$a0 already preserved in s2
	move $s1, $a1

	addiu $sp, $sp, -0x18
	sw $ra, 0x010($sp)

	jal _lock
	move $s0, $a2

	lw $ra, 0x010($sp)

	#Restore $a0, $a1, $a2
	move $a0, $s2
	move $a1, $s1
	move $a2, $s0

	jr $ra
	addiu $sp, $sp, 0x18
.end _lock_RpcFunc

