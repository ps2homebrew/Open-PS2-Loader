#include <atad.h>
#include "atad_d.h"

static inline int atad_read_(nbd_context const *const me, void *buffer, uint64_t offset, uint32_t length)
{
    return ata_device_sector_io(((atad_driver const *)me)->device, buffer, (uint32_t)offset, length, ATA_DIR_READ);
}

static inline int atad_write_(nbd_context const *const me, void *buffer, uint64_t offset, uint32_t length)
{
    return ata_device_sector_io(((atad_driver const *)me)->device, buffer, (uint32_t)offset, length, ATA_DIR_WRITE);
}

static inline int atad_flush_(nbd_context const *const me)
{
    return ata_device_flush_cache(((atad_driver const *)me)->device);
}

int atad_ctor(atad_driver *const me, int device)
{
    me->device = device;
    ata_devinfo_t *dev_info = ata_get_devinfo(me->device);

    static struct nbd_context_Vtbl const vtbl = {
        &atad_read_,
        &atad_write_,
        &atad_flush_,
    };
    nbd_context_ctor(&me->super); /* call the superclass' ctor */
    me->super.vptr = &vtbl;       /* override the vptr */
    // int ata_device_sce_identify_drive(int device, void *data);
    strcpy(me->super.export_desc, "PlayStation 2 HDD via ATAD");
    sprintf(me->super.export_name, "%s%d", "hdd", me->device);
    me->super.blocksize = 512;
    me->super.buffer = nbd_buffer;
    me->super.eflags = NBD_FLAG_HAS_FLAGS | NBD_FLAG_SEND_FLUSH;

    if (dev_info != NULL && dev_info->exists) {
        me->super.export_size = (uint64_t)dev_info->total_sectors * me->super.blocksize;
        return 0;
    }
    return 1;
}
