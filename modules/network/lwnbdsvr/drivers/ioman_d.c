#include "ioman_d.h"
#include <string.h>
#include <iox_stat.h>

static inline int ioman_read_(nbd_context const *const me, void *buffer, uint64_t offset, uint32_t length)
{
    // return ((ioman_driver const *)me)->device->ops.read(buffer, length);
    return 0;
}

static inline int ioman_write_(nbd_context const *const me, void *buffer, uint64_t offset, uint32_t length)
{
    return 0;
}

static inline int ioman_flush_(nbd_context const *const me)
{
    return 0;
}

int ioman_ctor(ioman_driver *const me, iop_device_t *device)
{
    int ret;
    iox_stat_t stat;

    // support only blockdevice
    if ((device->type & IOP_DT_BLOCK) == 0) {
        DEBUGLOG("Wrong by mask:%d %s\n", device->type, device->name);
        return -1;
    }

    static struct lwnbd_operations const nbdopts = {
        &ioman_read_,
        &ioman_write_,
        &ioman_flush_,
    };
    nbd_context_ctor(&me->super); /* call the superclass' ctor */
    me->super.vptr = &nbdopts;    /* override the vptr */
    me->device = device;
    strncpy(me->super.export_name, me->device->name, 31);
    strncpy(me->super.export_desc, me->device->desc, 31);
    me->super.buffer = nbd_buffer;

    ret = getstat(me->device->name, &stat);
    if (ret != 0) {
        DEBUGLOG("getstat error for %s \n", me->device->name);
        return -1;
    }

    me->super.eflags = NBD_FLAG_HAS_FLAGS;
    me->super.blocksize = 512;
    me->super.export_size = stat.size;
    LOG("found %s\n", me->super.export_name);
    return 0;
}
