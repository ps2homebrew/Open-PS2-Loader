/*
  _____     ___ ____ 
   ____|   |    ____|      PSX2 OpenSource Project
  |     ___|   |____       (C)2003, adresd (adresd_ps2dev@yahoo.com)
  ------------------------------------------------------------------------
			   Imports of 'ps2dev9.irx' functions for use here  
			   from PS2DRV, PS2DRV license applies to this file		
*/

	.text
	.set	noreorder


	.local	exp_stub
exp_stub:
	.word	0x41e00000
	.word	0
	.word	0x00000101
	.ascii	"dev9\0\0\0\0\0"
	.align	2

	.globl	dev9RegisterIntrCb				# 004
dev9RegisterIntrCb:
	j	$31
	li	$0, 4

	.globl	dev9DmaTransfer				# 005
dev9DmaTransfer:
	j	$31
	li	$0, 5

	.globl	dev9Shutdown			# 006
dev9Shutdown:
	j	$31
	li	$0, 6

	.globl	dev9IntrEnable				# 007
dev9IntrEnable:
	j	$31
	li	$0, 7

	.globl	dev9IntrDisable				# 008
dev9IntrDisable:
	j	$31
	li	$0, 8

	.globl	dev9GetEEPROM				# 009
dev9GetEEPROM:
	j	$31
	li	$0, 9

	.globl	dev9LEDCtl			# 00A
dev9LEDCtl:
	j	$31
	li	$0, 0x0A

	.word	0
	.word	0


