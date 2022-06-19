#include <loadcore.h>
#include <sysmem.h>

#define MODNAME "freeram"
IRX_ID(MODNAME, 1, 1);

/*
 * In order to test the amount of free IOP RAM available, this module should be
 * AS SMALL AS POSSIBLE!
 *
 * Free IOP RAM is placed at exactly the middle of IOP RAM, where the EE can
 * read the value from. This memory location should not be used by anything!
 */

int _start()
{
    vu32 *pfreeram = (vu32 *)(1 * 1024 * 1024);
    *pfreeram = QueryTotalFreeMemSize();
    return MODULE_NO_RESIDENT_END;
}
