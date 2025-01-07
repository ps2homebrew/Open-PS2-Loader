/*
 Copyright 2022-2025, Thanks to SP193 and KrahJohilto
 Copyright 2025 André Guilherme(Wofl3s)
 Licenced under Academic Free License version 3.0
 Review OpenPS2Loader README & LICENSE files for further details.
 */

#include <audsrv.h>

#include "include/opl.h"
#include "include/audio.h"
#include "include/ioman.h"

extern int audio_initialized;
/*--    General Audio    ------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------------------------*/

void audioInit(void)
{
    if (!audio_initialized) {
        if (audsrv_init() != 0) {
            LOG("AUDIO: Failed to initialize audsrv\n");
            LOG("AUDIO: Audsrv returned error string: %s\n", audsrv_get_error_string());
            return;
        } else {
            audsrv_adpcm_init();
        }
        audio_initialized = 1;
    }
}

void audioEnd(void)
{
    if (!audio_initialized) {
        LOG("AUDIO: %s: ERROR: not initialized!\n", __FUNCTION__);
        return;
    }

    audsrv_quit();
    audio_initialized = 0;
}

void audioSetVolume()
{
    if (!audio_initialized) {
        LOG("AUDIO: %s: ERROR: not initialized!\n", __FUNCTION__);
        return;
    }

    audsrv_adpcm_set_volume(0, gBootSndVolume);
    audsrv_set_volume(gBGMVolume);
}

void audioSetSfxVolume(int sfx)
{
    int i;

    if (!audio_initialized) {
        LOG("AUDIO: %s: ERROR: not initialized!\n", __FUNCTION__);
        return;
    }

    for (i = 1; i < sfx; i++)
        audsrv_adpcm_set_volume(i, gSFXVolume);
}