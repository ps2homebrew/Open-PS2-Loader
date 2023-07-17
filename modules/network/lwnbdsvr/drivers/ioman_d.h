#ifndef IOMAN_DRIVERS_NBD_H
#define IOMAN_DRIVERS_NBD_H

#define MAX_DEVICES 32
#include <lwnbd.h>
#include <iomanX.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ioman_driver
{
    nbd_context super;
    iop_device_t *device;
} ioman_driver;

int ioman_ctor(ioman_driver *const me, iop_device_t *device);

#ifdef __cplusplus
}
#endif
#endif /* IOMAN_DRIVERS_NBD_H */
