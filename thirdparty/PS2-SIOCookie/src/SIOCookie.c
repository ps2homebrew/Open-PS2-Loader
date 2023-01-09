#include "SIOCookie.h"
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sio.h>

#ifdef SIO_COOKIE_DEBUG
int printf(const char *format, ...);
#define DPRINTF(X...) printf(X)
#else
#define DPRINTF(X...)
#endif

// GLOBAL
FILE *EE_SIO;
//


ssize_t cookie_sio_write(void *c, const char *buf, size_t size);
ssize_t cookie_sio_read(void *c, char *buf, size_t size);
int cookie_sio_seek(void *c, _off64_t *offset, int whence);
int cookie_sio_close(void *c);
cookie_io_functions_t COOKIE_FNCTS;

struct memfile_cookie  {
    char   *buf;        /* Dynamically sized buffer for data */
    size_t  allocated;  /* Size of buf */
    size_t  endpos;     /* Number of characters in buf */
    off_t   offset;     /* Current file offset in buf */
};

int ee_sio_start(u32 baudrate, u8 lcr_ueps, u8 lcr_upen, u8 lcr_usbl, u8 lcr_umode, int vbuftype)
{
    DPRINTF("%s: start", __func__);
    DPRINTF("%s: sio_init(%u, %u, %u, %u, %u);\n", __func__, baudrate, lcr_ueps, lcr_upen, lcr_usbl, lcr_umode);
	sio_init(baudrate, lcr_ueps, lcr_upen, lcr_usbl, lcr_umode);
    struct memfile_cookie EE_SIO_COOKIE;
    EE_SIO_COOKIE.buf = NULL; // buffer will be used for EE_SIO RX pin in the future
    EE_SIO_COOKIE.allocated = 0;
    EE_SIO_COOKIE.offset = 0;
    EE_SIO_COOKIE.endpos = 0;
    
    COOKIE_FNCTS.read = cookie_sio_read;
    COOKIE_FNCTS.close = cookie_sio_close;
    COOKIE_FNCTS.seek = cookie_sio_seek;
    COOKIE_FNCTS.write = cookie_sio_write;
    EE_SIO = fopencookie(&EE_SIO_COOKIE, "w+", COOKIE_FNCTS);
    if (EE_SIO == NULL) {
        DPRINTF("EE_SIO stream is NULL\n");
        return EESIO_COOKIE_OPEN_IS_NULL;
    }
    setvbuf(EE_SIO, EE_SIO_COOKIE.buf, _IONBF, EE_SIO_COOKIE.allocated); // no buffering for this bad boy
    return EESIO_SUCESS;
}


ssize_t cookie_sio_write(void *c, const char *buf, size_t size)
{
    DPRINTF("%s: start", __func__);
    return sio_putsn(buf);
}

ssize_t cookie_sio_read(void *c, char *buf, size_t size)
{
    DPRINTF("%s: start", __func__);
    return sio_read(buf, size);
}

int cookie_sio_seek(void *c, _off64_t *offset, int whence)
{
    DPRINTF("%s: start", __func__);
    return 0;
}

int cookie_sio_close(void *c)
{
    DPRINTF("%s: start", __func__);
    return 0;
}
