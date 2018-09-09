#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <atad.h>
#include <iomanX.h>
#include <errno.h>

#include "opl-hdd-ioctl.h"
#include "xhdd.h"

#define MODNAME "xhdd"
IRX_ID(MODNAME, 1, 2);

static int isHDPro;

static int xhddInit(iop_device_t *device)
{
    return 0;
}

static int xhddUnsupported(void)
{
    return -1;
}

static int xhddDevctl(iop_file_t *fd, const char *name, int cmd, void *arg, unsigned int arglen, void *buf, unsigned int buflen)
{
    ata_devinfo_t *devinfo;

    if (fd->unit >= 2)
        return -ENXIO;

    switch (cmd) {
        case ATA_DEVCTL_IS_48BIT:
            return ((devinfo = ata_get_devinfo(fd->unit)) != NULL ? devinfo->lba48 : -1);
        case ATA_DEVCTL_SET_TRANSFER_MODE:
            if (!isHDPro)
                return ata_device_set_transfer_mode(fd->unit, ((hddAtaSetMode_t *)arg)->type, ((hddAtaSetMode_t *)arg)->mode);
            else
                return hdproata_device_set_transfer_mode(fd->unit, ((hddAtaSetMode_t *)arg)->type, ((hddAtaSetMode_t *)arg)->mode);
        default:
            return -EINVAL;
    }
}

static iop_device_ops_t xhdd_ops = {
    &xhddInit,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    (void *)&xhddUnsupported,
    &xhddDevctl,
};

static iop_device_t xhddDevice = {
    "xhdd",
    IOP_DT_BLOCK | IOP_DT_FSEXT,
    1,
    "XHDD",
    &xhdd_ops};

int _start(int argc, char *argv[])
{
    int i;

    isHDPro = 0;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-hdpro"))
            isHDPro = 1;
    }

    return AddDrv(&xhddDevice) == 0 ? MODULE_RESIDENT_END : MODULE_NO_RESIDENT_END;
}
