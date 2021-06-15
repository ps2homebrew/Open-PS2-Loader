/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: ioptrap.h 598 2004-09-21 19:14:27Z lukasz $
*/

#ifndef __IOPTRAP_H__
#define __IOPTRAP_H__

#include <types.h>

/* IDT_R3000_20.pdf 3-11 table 3.5 */
typedef enum {
    EXCEPTION_Int, /* note you can't really work with this one. */
    EXCEPTION_Mod,
    EXCEPTION_TLBL,
    EXCEPTION_TLBS,
    EXCEPTION_AdEL,
    EXCEPTION_AdES,
    EXCEPTION_IBE,
    EXCEPTION_DBE,
    EXCEPTION_Syscall,
    EXCEPTION_Bp,
    EXCEPTION_RI,
    EXCEPTION_CpU,
    EXCEPTION_Ov,
    EXCEPTION_RESERVED13,
    EXCEPTION_RESERVED14 /* would be FPE, but that's not going to happen on the IOP */
} exception_type_t;

typedef struct exception_frame
{
    u32 epc;
    u32 cause;
    u32 badvaddr;
    u32 sr;
    u32 regs[32];
    u32 hi;
    u32 lo;
    u32 dcic;
} exception_frame_t;

/* Beware: the handler will be run in a 'bad' state. Consider you are
   on interrupt mode, so, only i* functions. */
typedef void (*trap_exception_handler_t)(exception_type_t, exception_frame_t *);

#define DCIC_WR (1 << 27)
#define DCIC_RD (1 << 26)
#define DCIC_DA (1 << 25)
#define DCIC_PC (1 << 24)

#define ioptrap_IMPORTS_start DECLARE_IMPORT_TABLE(ioptrap, 1, 1)
#define ioptrap_IMPORTS_end   END_IMPORT_TABLE

const char *get_exception_name(exception_type_t type);
#define I_get_exception_name DECLARE_IMPORT(4, get_exception_name)

/* Will act as a setjmp. Returns 0 to say it was the first call,
   otherwise, returns the exception number. (note that you'll never
   get any interrupt...) Note that using this will completely disable
   user defined exception handlers. */
exception_type_t dbg_setjmp();
#define I_dbg_setjmp DECLARE_IMPORT(5, dbg_setjmp)

/* Will return the old handler. */
trap_exception_handler_t set_exception_handler(exception_type_t type, trap_exception_handler_t handler);
#define I_set_exception_handler DECLARE_IMPORT(6, set_exception_handler)
trap_exception_handler_t get_exception_handler(exception_type_t type);
#define I_get_exception_handler DECLARE_IMPORT(7, get_exception_handler)

/* Breakpoint stuff. */
void set_dba(u32 v);
#define I_set_dba DECLARE_IMPORT(8, set_dba)
void set_dbam(u32 v);
#define I_set_dbam DECLARE_IMPORT(9, set_dbam)
void set_dcic(u32 v);
#define I_set_dcic DECLARE_IMPORT(10, set_dcic)
u32 get_dba();
#define I_get_dba DECLARE_IMPORT(11, get_dba)
u32 get_dbam();
#define I_get_dbam DECLARE_IMPORT(12, get_dbam)
u32 get_dcic();
#define I_get_dcic DECLARE_IMPORT(13, get_dcic)

#endif
