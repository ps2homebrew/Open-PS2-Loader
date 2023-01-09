#ifndef SIO_COOKIE_H
#define SIO_COOKIE_H

#define _GNU_SOURCE
#include <stdio.h>
#include <kernel.h>

int ee_sio_start(u32 baudrate, u8 lcr_ueps, u8 lcr_upen, u8 lcr_usbl, u8 lcr_umode, int vbuftype);

enum  {EESIO_SUCESS = 0, EESIO_COOKIE_OPEN_IS_NULL, EESIO_BUFFER_ALLOC_IS_NULL};
extern FILE *EE_SIO;
#endif
