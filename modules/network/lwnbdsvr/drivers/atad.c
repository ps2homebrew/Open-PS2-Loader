#include "atad.h"

int atad_init(struct nbd_context *me)
{
    ata_devinfo_t *dev_info = ata_get_devinfo(0);
    if (dev_info != NULL && dev_info->exists) {
        me->export_size = (uint64_t)dev_info->total_sectors << me->blockshift;
        return 0;
    }
    return 1;
}

int atad_read(struct nbd_context *me, void *buffer, uint64_t offset, uint32_t length)
{
    return ata_device_sector_io(0, buffer, (uint32_t)offset, length, ATA_DIR_READ);
}

int atad_write(struct nbd_context *me, void *buffer, uint64_t offset, uint32_t length)
{
    return ata_device_sector_io(0, buffer, (uint32_t)offset, length, ATA_DIR_WRITE);
}

int atad_flush(struct nbd_context *me)
{
    return ata_device_flush_cache(0);
}
