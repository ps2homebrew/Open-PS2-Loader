#include <loadcore.h>
#include <intrman.h>
#include <ioman.h>
#include <sifman.h>
#include <sysmem.h>

#include "loadcore_add.h"

#define MODNAME "SyncEE"
IRX_ID(MODNAME, 0x01, 0x01);

static int PostResetCallback(int* arg1, int arg2)
{
	sceSifSetSMFlag(SIF_STAT_BOOTEND);

	return 0;
}

int _start(int argc, char** argv)
{
	loadcore20(&PostResetCallback, 2, 0);

	return MODULE_RESIDENT_END;
}
