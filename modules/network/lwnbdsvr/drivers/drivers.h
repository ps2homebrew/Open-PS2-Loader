

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
        .export_name = "hdd",
        .export_desc = "PlayStation 2 HDD via ATAD",
        .blockshift = 9,
        .buffer = nbd_buffer,
        .export_init = hdd_atad_init,
        .read = hdd_atad_read,
        .write = hdd_atad_write,
        .flush = hdd_atad_flush,
};

struct nbd_context *nbd_contexts[] = {
    &hdd_atad,
};

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_NBD_H */
