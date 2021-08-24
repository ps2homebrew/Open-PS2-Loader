
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
    int successed_exported_ctx = 0;
    struct nbd_context **ptr_ctx = nbd_contexts;

    //Platform specific block device detection then nbd_context initialization go here
    while (*ptr_ctx) {
        if ((*ptr_ctx)->export_init(*ptr_ctx) != 0) {
            printf("lwnbdsvr: failed to init %s driver!\n", (*ptr_ctx)->export_name);
        } else {
            printf("lwnbdsvr: export %s\n", (*ptr_ctx)->export_desc);
            successed_exported_ctx++;
        }
        ptr_ctx++;
    }

    if (!successed_exported_ctx) {
        printf("lwnbdsvr: nothing to export.\n");
        return -1;
    }

    printf("lwnbdsvr: init nbd_contexts ok.\n");

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
