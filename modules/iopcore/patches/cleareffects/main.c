/**
  * Patch for games (e.g. Half Life) that have the SPU2 Digital Effects function inadvertently enabled with old settings from previously-run software.
  * Solve this problem by disabling sound effects via sceSdSetEfectAttr(), which will clear the necessary registers.
  */

#include <loadcore.h>
#include <libsd.h>
#include <sysclib.h>

int _start(int argc, char **argv)
{
    sceSdEffectAttr attr = {
	0,
	SD_EFFECT_MODE_OFF,
	0,
	0,
	0,
	0,
    };

    sceSdInit(0);

    sceSdSetEffectAttr(0, &attr);
    sceSdSetEffectAttr(1, &attr);

    return MODULE_NO_RESIDENT_END;
}

