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

   .weak  _init
   .type  _init, @function

   .weak  _fini
   .type  _fini, @function

   .extern   _heap_size
   .extern   _stack
   .extern   _stack_size

   .set   noat
   .set   noreorder

   .text
   .align   2

   nop
   nop

   .globl  _start
   .ent    _start
_start:

zerobss:
   # clear bss area

   la   $2, _fbss
   la   $3, _end

1:
   sltu   $1, $2, $3
   beq   $1, $0, 2f
   nop
   sq   $0, ($2)
   addiu   $2, $2, 16
   j   1b
   nop
2:

setupthread:
   # setup current thread

   la   $4, _gp
   la   $5, _stack
   la   $6, _stack_size
   la   $7, _args
   la   $8, _root
   move   $gp, $4
   addiu   $3, $0, 60
   syscall         # SetupThread(_gp, _stack, _stack_size, _args, _root)
   move   $sp, $2

   # initialize heap

   la   $4, _end
   la   $5, _heap_size
   addiu   $3, $0, 61
   syscall         # SetupHeap(_end, _heap_size)

   # writeback data cache

   move   $4, $0
   addiu   $3, $0, 100
   syscall         # FlushCache(0)

ctors:
   # call global constructors (weak)

   la   $8, _init
   beqz   $8, 1f
   nop
   jalr   $8      # _init()
   nop
1:

   # call main
   ei
   la   $2, _args
   lw   $4, ($2)
   jal   main      # main(argc, argv)
   addiu   $5, $2, 4

   # call _exit

   j   _exit      # _exit(retval)
   move   $4, $2
   .end   _start

   .align   3

   .globl   _exit
   .ent   _exit
   .text
_exit:
   la   $2, _retval
   sw   $4, ($2)

dtors:
   # call global destructors (weak)

   la   $8, _fini
   beqz   $8, 1f
   nop
   jalr   $8      # _fini()
   nop
1:

   la   $2, _retval
   lw   $4, ($2)

   addiu   $3, $0, 4
   syscall         # Exit(retval) (noreturn)

   .end   _exit

   .ent   _root
_root:
   addiu   $3, $0, 35
   syscall         # ExitThread() (noreturn)
   .end   _root

   .bss
   .align   6
_args:
   .space   4+16*4+256   # argc, 16 arguments, 256 bytes payload
_retval:
   .space   4
