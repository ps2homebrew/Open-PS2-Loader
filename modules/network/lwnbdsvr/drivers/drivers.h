#ifndef DRIVERS_NBD_H
#define DRIVERS_NBD_H

#include "../irx_imports.h"
#include "../lwNBD/nbd_server.h"
#include "atad.h"

#ifdef __cplusplus
extern "C" {
#endif

static struct nbd_context hdd_atad =
    {
        .export_desc = "PlayStation 2 HDD via ATAD",
        .export_name = "hdd0",
        .blockshift = 9,
        .buffer = nbd_buffer,
        .export_init = atad_init,
        .read = atad_read,
        .write = atad_write,
        .flush = atad_flush,
};

struct nbd_context *nbd_contexts[] = {
    &hdd_atad,
    NULL,
};

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_NBD_H */
