#include <atad.h>
#include <atahw.h>
#include <speedregs.h>

#include "opl-hdd-ioctl.h"
#include "xhdd.h"

static void ata_multiword_dma_mode(int mode)
{
    USE_SPD_REGS;
    u16 val;

    switch (mode) {
        case 1:
            val = 0x45;
            break;
        case 2:
            val = 0x24;
            break;
        default:
            val = 0xff;
    }

    SPD_REG16(SPD_R_MWDMA_MODE) = val;
    SPD_REG16(SPD_R_IF_CTRL) = (SPD_REG16(SPD_R_IF_CTRL) & 0xfffe) | 0x48;
}

static void ata_ultra_dma_mode(int mode)
{
    USE_SPD_REGS;
    u16 val;

    switch (mode) {
        case 1:
            val = 0x85;
            break;
        case 2:
            val = 0x63;
            break;
        case 3:
            val = 0x62;
            break;
        case 4:
            val = 0x61;
            break;
        default:
            val = 0xa7;
    }

    SPD_REG16(SPD_R_UDMA_MODE) = val;
    SPD_REG16(SPD_R_IF_CTRL) |= 0x49;
}

/* Set features - set transfer mode.  */
int ata_device_set_transfer_mode(int device, int type, int mode)
{
    int res;

    res = sceAtaExecCmd(NULL, 1, 3, (type | mode) & 0xff, 0, 0, 0, (device << 4) & 0xffff, ATA_C_SET_FEATURES);
    if (res)
        return res;

    res = sceAtaWaitResult();
    if (res)
        return res;

    // Note: PIO is not supported by sceAtaDmaTransfer.
    switch (type) {
        case ATA_XFER_MODE_MDMA:
            ata_multiword_dma_mode(mode);
            break;
        case ATA_XFER_MODE_UDMA:
            ata_ultra_dma_mode(mode);
            break;
    }

    return 0;
}
