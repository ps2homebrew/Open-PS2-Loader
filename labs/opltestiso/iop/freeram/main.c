#include <loadcore.h>
#include <sysmem.h>

#define MODNAME "freeram"

#ifdef IOP_MEM_8MB // DESR, DTL-T and namco system 256
#define MIDDLE_OF_RAM (4 * 1024 * 1024)
IRX_ID(MODNAME, 1, 8);
#else
#define MIDDLE_OF_RAM (1 * 1024 * 1024)
IRX_ID(MODNAME, 1, 2);
#endif

/*
 * In order to test the amount of free IOP RAM available, this module should be
 * AS SMALL AS POSSIBLE!
 *
 * Free IOP RAM is placed at exactly the middle of IOP RAM, where the EE can
 * read the value from. This memory location should not be used by anything!
 */

int _start()
{
    vu32 *pfreeram = (vu32 *)MIDDLE_OF_RAM;
    *pfreeram = QueryTotalFreeMemSize();
    return MODULE_NO_RESIDENT_END;
}
