/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: handler.S 595 2004-09-21 13:35:48Z pixel $
*/


#include "as_reg_compat.h"

#define GENERAL_ATSAVE             0x400
#define GENERAL_SRSAVE

         .global def_exc_handler
         .ent def_exc_handler
def_exc_handler:
         .set noreorder
         .set noat
         .word 0
         .word 0
         la $k0, __trap_frame
         sw $0,  0x10($k0)
         
         lw $1,  0x400($0)
         nop         
         sw $1,  0x14($k0)         

         sw $2,  0x18($k0)         
         sw $3,  0x1c($k0)         
         sw $4,  0x20($k0)         
         sw $5,  0x24($k0)         
         sw $6,  0x28($k0)         
         sw $7,  0x2c($k0)         
         sw $8,  0x30($k0)         
         sw $9,  0x34($k0)         
         sw $10, 0x38($k0)         
         sw $11, 0x3c($k0)         
         sw $12, 0x40($k0)         
         sw $13, 0x44($k0)         
         sw $14, 0x48($k0)         
         sw $15, 0x4c($k0)         
         sw $16, 0x50($k0)         
         sw $17, 0x54($k0)         
         sw $18, 0x58($k0)         
         sw $19, 0x5c($k0)         
         sw $20, 0x60($k0)         
         sw $21, 0x64($k0)         
         sw $22, 0x68($k0)         
         sw $23, 0x6c($k0)         
         sw $24, 0x70($k0)         
         sw $25, 0x74($k0)         
         sw $26, 0x78($k0)         
         sw $27, 0x7c($k0)         
         sw $28, 0x80($k0)         
         sw $29, 0x84($k0)
         sw $30, 0x88($k0)
         sw $31, 0x8c($k0)
         mfhi $a0
         nop
         sw $a0, 0x90($k0)

         mflo $a0
         nop
         sw $a0, 0x94($k0)

         
         #lw $a0, 0x404($0) # saved EPC
         mfc0 $a0, $14 #EPC
         nop
         sw $a0, 0($k0)
         
         #lw $a0, 0x408($0) # saved SR
         mfc0 $a0, $12 #SR
         nop
         sw $a0, 0xc($k0)
         
         mfc0 $a0, $8 # BadVaddr
         nop
         sw $a0, 0x8($k0)

         sw $0, 0x98($k0)


         #lw $a0, 0x40c($0) # saved Cause
         mfc0 $a0, $13 #Cause
         nop
         sw $a0, 4($k0)

         sll $a0, 25
         srl $a0, 27
         jal trap
         or $a1, $k0, $0

         la $k0, __trap_frame

         lw $a0, 0x94($k0)
         nop
         mtlo $a0

         lw $a0, 0x90($k0)
         nop
         mthi $a0

         #lw $0,  0x10($k0)         
         lw $1,  0x14($k0)         
         lw $2,  0x18($k0)         
         lw $3,  0x1c($k0)         
         lw $4,  0x20($k0)         
         lw $5,  0x24($k0)         
         lw $6,  0x28($k0)         
         lw $7,  0x2c($k0)         
         lw $8,  0x30($k0)         
         lw $9,  0x34($k0)         
         lw $10, 0x38($k0)         
         lw $11, 0x3c($k0)         
         lw $12, 0x40($k0)         
         lw $13, 0x44($k0)         
         lw $14, 0x48($k0)         
         lw $15, 0x4c($k0)         
         lw $16, 0x50($k0)         
         lw $17, 0x54($k0)         
         lw $18, 0x58($k0)         
         lw $19, 0x5c($k0)         
         lw $20, 0x60($k0)         
         lw $21, 0x64($k0)         
         lw $22, 0x68($k0)         
         lw $23, 0x6c($k0)         
         lw $24, 0x70($k0)         
         lw $25, 0x74($k0)         
         #lw $26, 0x78($k0)         
         #lw $27, 0x7c($k0)         
         lw $28, 0x80($k0)         
         lw $29, 0x84($k0)
         lw $30, 0x88($k0)
         lw $31, 0x8c($k0)

         lw $k1, 0xc($k0)
         nop
         srl $k1, 1
         sll $k1, 1
         mtc0 $k1, $12 /* restore SR */

         lw $k1, 0($k0)
         lw $k0, 0x410($0) /* k0 saved here by exception prologue (at 0x80) */
         jr $k1
         cop0 0x10
         nop
         .end def_exc_handler
         
         .global bp_exc_handler
         .ent bp_exc_handler
bp_exc_handler:
         .set noreorder
         .set noat
         .word 0
         .word 0
         la $k0, __trap_frame
         sw $0,  0x10($k0)         
         sw $1,  0x14($k0)         
         sw $2,  0x18($k0)         
         sw $3,  0x1c($k0)         
         sw $4,  0x20($k0)         
         sw $5,  0x24($k0)         
         sw $6,  0x28($k0)         
         sw $7,  0x2c($k0)         
         sw $8,  0x30($k0)         
         sw $9,  0x34($k0)         
         sw $10, 0x38($k0)         
         sw $11, 0x3c($k0)         
         sw $12, 0x40($k0)         
         sw $13, 0x44($k0)         
         sw $14, 0x48($k0)         
         sw $15, 0x4c($k0)         
         sw $16, 0x50($k0)         
         sw $17, 0x54($k0)         
         sw $18, 0x58($k0)         
         sw $19, 0x5c($k0)         
         sw $20, 0x60($k0)         
         sw $21, 0x64($k0)         
         sw $22, 0x68($k0)         
         sw $23, 0x6c($k0)         
         sw $24, 0x70($k0)         
         sw $25, 0x74($k0)         
         sw $26, 0x78($k0)         
         sw $27, 0x7c($k0)         
         sw $28, 0x80($k0)         
         sw $29, 0x84($k0)
         sw $30, 0x88($k0)
         sw $31, 0x8c($k0)
         mfhi $a0
         nop
         sw $a0, 0x90($k0)

         mflo $a0
         nop
         sw $a0, 0x94($k0)

         
         #lw $a0, 0x424($0) # saved EPC
         mfc0 $a0, $14 #EPC
         nop
         sw $a0, 0($k0)
         
         #lw $a0, 0x42c($0) # saved SR
         mfc0 $a0, $12 #SR
         nop
         sw $a0, 0xc($k0)
         
         mfc0 $a0, $8 # BadVaddr
         nop
         sw $a0, 0x8($k0)

         lw $a0, 0x430($0) # saved DCIC
         nop
         sw $a0, 0x98($k0)

         
         #lw $a0, 0x428($0) # saved Cause
         mfc0 $a0, $13 #Cause
         nop
         sw $a0, 4($k0)

         sll $a0, 25
         srl $a0, 27
         jal trap
         or $a1, $k0, $0

        
         la $k0, __trap_frame

         lw $a0, 0x94($k0)
         nop
         mtlo $a0

         lw $a0, 0x90($k0)
         nop
         mthi $a0

         lw $a0, 0x98($k0)
         nop
         mtc0 $a0, $7
         
         #lw $0,  0x10($k0)         
         lw $1,  0x14($k0)         
         lw $2,  0x18($k0)         
         lw $3,  0x1c($k0)         
         lw $4,  0x20($k0)         
         lw $5,  0x24($k0)         
         lw $6,  0x28($k0)         
         lw $7,  0x2c($k0)         
         lw $8,  0x30($k0)         
         lw $9,  0x34($k0)         
         lw $10, 0x38($k0)         
         lw $11, 0x3c($k0)         
         lw $12, 0x40($k0)         
         lw $13, 0x44($k0)         
         lw $14, 0x48($k0)         
         lw $15, 0x4c($k0)         
         lw $16, 0x50($k0)         
         lw $17, 0x54($k0)         
         lw $18, 0x58($k0)         
         lw $19, 0x5c($k0)         
         lw $20, 0x60($k0)         
         lw $21, 0x64($k0)         
         lw $22, 0x68($k0)         
         lw $23, 0x6c($k0)         
         lw $24, 0x70($k0)         
         lw $25, 0x74($k0)         
         #lw $26, 0x78($k0)         
         #lw $27, 0x7c($k0)         
         lw $28, 0x80($k0)         
         lw $29, 0x84($k0)
         lw $30, 0x88($k0)
         lw $31, 0x8c($k0)

         lw $k1, 0xc($k0)
         nop
         srl $k1, 1
         sll $k1, 1
         mtc0 $k1, $12 /* restore SR */

         lw $k1, 0($k0)
         lw $k0, 0x420($0) /* k0 saved here by debug exception prologue (at 0x40) */
         jr $k1
         cop0 0x10
         nop
         .end bp_exc_handler
         
