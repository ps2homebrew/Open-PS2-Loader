#include <errno.h>
#include <iomanX.h>
#include <stdio.h>
#include <loadcore.h>
#include <usbhdfsd.h>
#include <sifcmd.h>

#include <irx.h>

#include "usb-ioctl.h"

IRX_ID("usbhdfsd_for_EE", 1, 1);

static int xmassInit(iop_device_t *device)
{
    return 0;
}

static int xmassUnsupported(void)
{
    return -1;
}

static int fat_CheckChain(fat_driver *fatd, u32 start)
{
    int i, chainSize, nextChain;

    unsigned int fileCluster;
    int clusterChainStart;

    fileCluster = start;

    if (fileCluster < 2)
        return 0;

    nextChain = 1;
    clusterChainStart = 1;

    while (nextChain) {
        chainSize = UsbMassFatGetClusterChain(fatd, fileCluster, fatd->cbuf, MAX_DIR_CLUSTER, clusterChainStart);
        clusterChainStart = 0;
        if (chainSize >= MAX_DIR_CLUSTER) { //the chain is full, but more chain parts exist
            fileCluster = fatd->cbuf[MAX_DIR_CLUSTER - 1];
        } else { //chain fits in the chain buffer completely - no next chain needed
            nextChain = 0;
        }

        //process the cluster chain (fatd->cbuf) and skip leading clusters if needed
        for (i = 0; i < (chainSize - 1); i++) {
            if ((fatd->cbuf[i] + 1) != fatd->cbuf[i + 1])
                return 0;
        }
    }

    return 1;
}

static int xmassDevctl(iop_file_t *fd, const char *name, int cmd, void *arg, unsigned int arglen, void *buf, unsigned int buflen)
{
    int result;
    fat_driver *fatd;

    if (fd->unit >= 2)
        return -ENXIO;

    switch (cmd) {
        case XUSBHDFSD_CHECK_CLUSTER_CHAIN:
            if ((fatd = UsbMassFatGetData(fd->unit)) != NULL) {
                result = fat_CheckChain(fatd, *(u32 *)arg);
            } else
                result = 0;

            return result;
        default:
            return -EINVAL;
    }
}

static iop_device_ops_t xmass_ops = {
    &xmassInit,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    (void *)&xmassUnsupported,
    &xmassDevctl,
};

static iop_device_t xmassDevice = {
    "xmass",
    IOP_DT_BLOCK | IOP_DT_FSEXT,
    1,
    "XMASS",
    &xmass_ops};

static void usbmass_cb(int cause)
{
    static SifCmdHeader_t EventCmdData;

    EventCmdData.opt = cause;
    sceSifSendCmd(0, &EventCmdData, sizeof(EventCmdData), NULL, NULL, 0);
}

int _start(int argc, char *argv[])
{
    if (AddDrv(&xmassDevice) == 0) {
        UsbMassRegisterCallback(0, &usbmass_cb);
        return MODULE_RESIDENT_END;
    } else {
        return MODULE_NO_RESIDENT_END;
    }
}
