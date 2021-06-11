
#include "irx_imports.h"
#include "lwNBD/nbd_server.h"
#include "drivers/drivers.h"

#define MODNAME "lwnbdsvr"
IRX_ID(MODNAME, 1, 1);
static int nbd_tid;
extern struct irx_export_table _exp_lwnbdsvr;

int _start(int argc, char **argv)
{
    iop_thread_t nbd_thread;

    //TODO : platform specific block device detection then nbd_context initialization go here
    //TODO : many export in a loop
    if (nbd_contexts[0]->export_init(nbd_contexts[0]) != 0)
        return -1;

    // register exports
    RegisterLibraryEntries(&_exp_lwnbdsvr);

    nbd_thread.attr = TH_C;
    nbd_thread.thread = (void *)nbd_init;
    nbd_thread.priority = 0x10;
    nbd_thread.stacksize = 0x800;
    nbd_thread.option = 0;
    nbd_tid = CreateThread(&nbd_thread);

    StartThread(nbd_tid, (struct nbd_context **)nbd_contexts);
    return MODULE_RESIDENT_END;
}

int _shutdown(void)
{
    DeleteThread(nbd_tid);
    return 0;
}
