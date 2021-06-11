/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: ioptrap.c 1619 2009-10-07 04:54:58Z Herben $
# IOP exception handling.
*/

#include "types.h"
#include "defs.h"
#include "irx.h"

#include "ioptrap.h"

#include "excepman.h"
#include "loadcore.h"
#include "intrman.h"
#include "sifman.h"
#include "stdio.h"
#include "sysclib.h"
#include "thbase.h"

IRX_ID("ioptrap_driver", 1, 1);

extern struct irx_export_table _exp_ioptrap;


static const char *exception_type_name[] = {
    "Interrupt",
    "TLB Modification",
    "TLB Miss Load",
    "TLB Miss Store",
    "Address Error Load",
    "Address Error Store",
    "Instruction Bus Error",
    "Data Bus Error",
    "Syscall",
    "Breakpoint",
    "Reserved Instruction",
    "Coprocessor Unusable",
    "Overflow",
    "Reserved 13",
    "Reserved 14" /* "Floating point exception" */
};

const char *get_exception_name(exception_type_t type)
{
    return exception_type_name[type];
}

exception_frame_t __trap_frame;
extern void def_exc_handler();
extern void bp_exc_handler();

int dbg_jmp_buf_setup = 0;
jmp_buf dbg_jmp_buf;

exception_type_t dbg_setjmp()
{
    int v;
    if (0 == (v = setjmp(dbg_jmp_buf))) {
        dbg_jmp_buf_setup = 1;
    } else {
        dbg_jmp_buf_setup = 0;
    }
    return v;
}

// from ps2link

/* Module info entry.  */
typedef struct _smod_mod_info
{
    struct _smod_mod_info *next;
    u8 *name;
    u16 version;
    u16 newflags; /* For modload shipped with games.  */
    u16 id;
    u16 flags; /* I believe this is where flags are kept for BIOS versions.  */
    u32 entry; /* _start */
    u32 gp;
    u32 text_start;
    u32 text_size;
    u32 data_size;
    u32 bss_size;
    u32 unused1;
    u32 unused2;
} smod_mod_info_t;


smod_mod_info_t *smod_get_next_mod(smod_mod_info_t *cur_mod)
{
    /* If cur_mod is 0, return the head of the list (IOP address 0x800).  */
    if (!cur_mod) {
        return (smod_mod_info_t *)0x800;
    } else {
        if (!cur_mod->next)
            return 0;
        else
            return cur_mod->next;
    }
    return 0;
}

char *ExceptionGetModuleName(u32 epc, u32 *r_epc)
{
    smod_mod_info_t *mod_info = 0;

    while ((mod_info = smod_get_next_mod(mod_info)) != 0) {
        if ((epc >= mod_info->text_start) && (epc <= (mod_info->text_start + mod_info->text_size))) {
            if (r_epc)
                *r_epc = epc - mod_info->text_start;

            return mod_info->name;
        }
    }

    return 0;
}

#define JUMP_BUF_PC 0
#define JUMP_BUF_SP 1
#define JUMP_BUF_FP 2
#define JUMP_BUF_S0 3
#define JUMP_BUF_S1 4
#define JUMP_BUF_S2 5
#define JUMP_BUF_S3 6
#define JUMP_BUF_S4 7
#define JUMP_BUF_S5 8
#define JUMP_BUF_S6 9
#define JUMP_BUF_S7 10
#define JUMP_BUF_GP 11

// Define this to something else if you want... maybe some ee_sio stuff :P
#define TRAP_PRINTF(args...) printf(args)

static trap_exception_handler_t handlers[16];

trap_exception_handler_t set_exception_handler(exception_type_t type, trap_exception_handler_t handler)
{
    trap_exception_handler_t old_handler = handlers[type];
    handlers[type] = handler;
    return old_handler;
}

trap_exception_handler_t get_exception_handler(exception_type_t type)
{
    return handlers[type];
}

void trap(exception_type_t type, struct exception_frame *ex)
{
    u32 i, r_addr;
    char *module;
    if (dbg_jmp_buf_setup) {
        u32 *p = (u32 *)dbg_jmp_buf;
        /* simulate longjmp */
        ex->epc = p[JUMP_BUF_PC];
        ex->regs[29] = p[JUMP_BUF_SP];
        ex->regs[30] = p[JUMP_BUF_FP];
        ex->regs[16] = p[JUMP_BUF_S0];
        ex->regs[17] = p[JUMP_BUF_S1];
        ex->regs[18] = p[JUMP_BUF_S2];
        ex->regs[19] = p[JUMP_BUF_S3];
        ex->regs[20] = p[JUMP_BUF_S4];
        ex->regs[21] = p[JUMP_BUF_S5];
        ex->regs[22] = p[JUMP_BUF_S6];
        ex->regs[23] = p[JUMP_BUF_S7];
        ex->regs[28] = p[JUMP_BUF_GP];
        ex->regs[2] = type; /* return value from setjmp */
        return;
    }
    TRAP_PRINTF("IOP Exception : %s\n", exception_type_name[type]);
    TRAP_PRINTF("EPC=%08x CAUSE=%08x SR=%08x BADVADDR=%08x DCIC=%08x\n", (int)ex->epc, (int)ex->cause, (int)ex->sr, (int)ex->badvaddr, (int)ex->dcic);
    if ((module = ExceptionGetModuleName(ex->epc, &r_addr)))
        TRAP_PRINTF("module %s at unreloc offset %08lX\n", module, r_addr);
    if ((module = ExceptionGetModuleName(ex->regs[31], &r_addr)))
        TRAP_PRINTF("ra module %s at unreloc offset %08lX\n", module, r_addr);
    for (i = 0; i != 32; i += 4) {
        TRAP_PRINTF("r[%02d]=%08x r[%02d]=%08x r[%02d]=%08x r[%02d]=%08x\n",
                    (int)i, (int)ex->regs[i], (int)i + 1, (int)ex->regs[i + 1], (int)i + 2, (int)ex->regs[i + 2], (int)i + 3, (int)ex->regs[i + 3]);
    }

    if (handlers[type]) {
        handlers[type](type, ex);
    }

    if (type == EXCEPTION_Bp) {
        ex->dcic = 0;
    } else {
        if (ex->cause & (1 << 31)) {
            ex->cause &= ~(1 << 31); /* clear BD */
        } else {
            ex->epc += 4;
        }
    }
}

#ifdef TEST_TRAP
static void trigger()
{
    u32 v;
    printf("trigger=%p\n", trigger);
    printf("badaddr\n");
    if (0 == (v = setjmp(dbg_jmp_buf))) {
        dbg_jmp_buf_setup = 1;
        *(u32 *)0xdeadbeef = 0xfeedface;
    } else {
        printf("exception occurred in command, v=%08x\n", (int)v);
    }
    dbg_jmp_buf_setup = 0;
    printf("done.\n");
}

void main_thread(void *unused)
{
    int i = 0;

    DelayThread(1000 * 1000);

    printf("IOP: about to trap!\n");
    trigger();
    printf("IOP: back from trap!\n");
    while (1) {
        printf("IOP: still running %d\n", i++);
        DelayThread(2000 * 1000);
        if (i == 10)
            break;
    }
    ExitDeleteThread();
}

int create_main_thread(void)
{
    iop_thread_t thread;

    thread.attr = 0x2000000;
    thread.option = 0;
    thread.thread = main_thread;
    thread.stacksize = 0x8000;
    thread.priority = 0x18;
    return CreateThread(&thread);
}

void do_tests()
{
    int thid;
    thid = create_main_thread();
    StartThread(thid, NULL);
}
#else
#define do_tests()
#endif

int _start(int argc, char **argv)
{
    int rv;

    if (RegisterLibraryEntries(&_exp_ioptrap) != 0)
        return 1;

    memset(handlers, 0, sizeof(trap_exception_handler_t) * 16);
    printf("ioptrap starts.\n");
    if ((rv = RegisterDefaultExceptionHandler(def_exc_handler)) < 0) {
        printf("RegisterDefaultExceptionHandler failed, rv=%d\n", rv);
        return 1;
    }
    if ((rv = RegisterPriorityExceptionHandler(IOP_EXCEPTION_HDB, 0, bp_exc_handler)) < 0) {
        // shouldn't we release the default exception handler here... ?
        printf("RegisterDefaultExceptionHandler failed, rv=%d\n", rv);
        return 1;
    }
    do_tests();
    return 0;
}

int shutdown()
{
    return 0;
}
