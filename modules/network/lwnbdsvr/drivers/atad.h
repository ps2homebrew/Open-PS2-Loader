#ifndef ATAD_DRIVERS_NBD_H
#define ATAD_DRIVERS_NBD_H

#include "../irx_imports.h"
#include "../lwNBD/nbd_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct atad_driver
{
    nbd_context super;
    int device;
} atad_driver;

int atad_ctor(atad_driver *const me, int device);

#ifdef __cplusplus
}
#endif
#endif /* ATAD_DRIVERS_NBD_H */
