
#include "irx_imports.h"
#include "lwNBD/nbd_server.h"
#include "drivers/atad.h"

#define MODNAME "lwnbdsvr"
IRX_ID(MODNAME, 1, 1);
static int nbd_tid;
extern struct irx_export_table _exp_lwnbdsvr;

//need to be global to be accessible from thread
atad_driver hdd;
nbd_context *nbd_contexts[] = {
    &hdd.super,
    NULL,
};

int _start(int argc, char **argv)
{
    iop_thread_t nbd_thread;
    int ret, successed_exported_ctx = 0;

    ret = atad_ctor(&hdd, 0);
    if (ret == 0)
        successed_exported_ctx = 1;

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
