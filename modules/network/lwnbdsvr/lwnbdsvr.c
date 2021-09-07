#define APP_NAME "lwnbdsvr"

#include "irx_imports.h"
#include <lwnbd.h>
#include "drivers/atad_d.h"
#include "drivers/ioman_d.h"

IRX_ID(APP_NAME, 1, 1);
static int nbd_tid;
extern struct irx_export_table _exp_lwnbdsvr;

//need to be global to be accessible from thread
atad_driver hdd[2]; // could have 2 ATA disks
ioman_driver iodev[32];
nbd_context *nbd_contexts[10];

int _start(int argc, char **argv)
{
    iop_device_t **dev_list = GetDeviceList();
    iop_thread_t nbd_thread;
    int i, j, ret, successed_exported_ctx = 0;

    if (argc > 1) {
        strcpy(gdefaultexport, argv[1]);
        LOG("default export : %s\n", gdefaultexport);
    }

    for (i = 0; i < 2; i++) {
        ret = atad_ctor(&hdd[i], i);
        if (ret == 0) {
            nbd_contexts[successed_exported_ctx] = &hdd[i].super;
            successed_exported_ctx++;
        }
    }

    j = 0;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (dev_list[i] != NULL) {
            ret = ioman_ctor(&iodev[j], dev_list[i]);
            if (ret == 0) {
                nbd_contexts[successed_exported_ctx] = &iodev[j].super;
                successed_exported_ctx++;
                j++;
            }
        }
    }

    nbd_contexts[successed_exported_ctx] = NULL;
    if (!successed_exported_ctx) {
        LOG("nothing to export.\n");
        return -1;
    }

    LOG("init %d exports.\n", successed_exported_ctx);

    // register exports
    RegisterLibraryEntries(&_exp_lwnbdsvr);

    nbd_thread.attr = TH_C;
    nbd_thread.thread = (void *)nbd_init;
    nbd_thread.priority = 0x10;
    nbd_thread.stacksize = 0x800;
    nbd_thread.option = 0;
    nbd_tid = CreateThread(&nbd_thread);

    // int StartThreadArgs(int thid, int args, void *argp);
    StartThread(nbd_tid, (struct nbd_context **)nbd_contexts);
    return MODULE_RESIDENT_END;
}

int _shutdown(void)
{
    DeleteThread(nbd_tid);
    return 0;
}
