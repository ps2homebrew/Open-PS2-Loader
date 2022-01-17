// MC driver for PS2-NBD server
// alexparrado (2021)

#ifndef MCMAN_DRIVERS_NBD_H
#define MCMAN_DRIVERS_NBD_H

#include <lwnbd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mcman_driver
{
    nbd_context super;
    int device;
} mcman_driver;

int mcman_ctor(mcman_driver *const me, int device);

#ifdef __cplusplus
}
#endif
#endif /* MCMAN_DRIVERS_NBD_H */
