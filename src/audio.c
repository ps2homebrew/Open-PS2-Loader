/*
 Copyright 2022-2025, Thanks to SP193 and KrahJohilto
 Copyright 2025 André Guilherme(Wofl3s)
 Licenced under Academic Free License version 3.0
 Review OpenPS2Loader README & LICENSE files for further details.
 */

#include <audsrv.h>

#include "include/audio.h"
#include "include/opl.h"
#include "include/ioman.h"

/*--    General Audio    ------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------------------------*/

void audioInit(void)
{
    if (audsrv_init() != 0) {
        LOG("AUDIO: Failed to initialize audsrv\n");
        LOG("AUDIO: Audsrv returned error string: %s\n", audsrv_get_error_string());
        return;
    }
}

void audioEnd(void)
{
    audsrv_quit();
}

void audioSetVolume(int sfx)
{
    int i;

    for (i = 1; i < sfx; i++)
        audsrv_adpcm_set_volume(i, gSFXVolume);

    audsrv_adpcm_set_volume(0, gBootSndVolume);
    audsrv_set_volume(gBGMVolume);
}
