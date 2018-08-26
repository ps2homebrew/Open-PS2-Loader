#include <atad.h>
#include <atahw.h>
#include <speedregs.h>

#include "opl-hdd-ioctl.h"
#include "xhdd.h"

int hdproata_device_set_transfer_mode(int device, int type, int mode)
{
    return 0;
}

/* Set features - enable/disable write cache.  */
int hdproata_device_set_write_cache(int device, int enable)
{   //As the start and finish functions are not exported, disable the write cache within the in-game ATAD instead.
    return 0;
}
