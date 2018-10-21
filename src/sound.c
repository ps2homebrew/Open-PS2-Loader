/*
SP193 wrote the code, I just made very small changes.
*/

#include <audsrv.h>
#include "include/sound.h"
#include "include/opl.h"

//default sfx
extern unsigned char scrollV_adp[];
extern unsigned int size_scrollV_adp;

extern unsigned char scrollH_adp[];
extern unsigned int size_scrollH_adp;

extern unsigned char page_adp[];
extern unsigned int size_page_adp;

extern unsigned char cancel_adp[];
extern unsigned int size_cancel_adp;

extern unsigned char transition_adp[];
extern unsigned int size_transition_adp;

extern unsigned char alert_adp[];
extern unsigned int size_alert_adp;

extern unsigned char confirm_adp[];
extern unsigned int size_confirm_adp;

struct sfxEffect {
    const char *name;
    void *buffer;
    int size;
};

static struct sfxEffect sfx_files[NUM_SFX_FILES] = {
    {"scrollV.adp"},
    {"scrollH.adp"},
    {"page.adp"},
    {"cancel.adp"},
    {"transition.adp"},
    {"alert.adp"},
    {"confirm.adp"},
};

struct audsrv_adpcm_t sfx[NUM_SFX_FILES];

static void getAudioDevice(char *device_path)
{
    if ((gAudioPath == 1))
        sprintf(device_path, "mc0:SND/");
    if ((gAudioPath == 2))
        sprintf(device_path, "mc1:SND/");
    if ((gAudioPath == 3))
        sprintf(device_path, "pfs0:SND/");
    if ((gAudioPath == 4))
        sprintf(device_path, "mass0:SND/");
}

//Returns 0 if the specified file was read. The sfxEffect structure will not be updated unless the file is successfully read.
static int sfxRead(const char *snd_path, struct sfxEffect *sfx)
{
    FILE* adpcm;
    void *buffer;
    int ret, size;

    adpcm = fopen(snd_path, "rb");
    if (adpcm == NULL)
    {
        printf("Failed to open adpcm file %s\n", snd_path);
        return -ENOENT;
    }

    fseek(adpcm, 0, SEEK_END);
    size = ftell(adpcm);
    rewind(adpcm);

    buffer = memalign(64, size);
    if (buffer == NULL)
    {
        printf("Failed to allocate memory for SFX\n");
        fclose(adpcm);
        return -ENOMEM;
    }

    ret = fread(buffer, 1, size, adpcm);
    fclose(adpcm);

    if(ret != size)
    {
        printf("Failed to read SFX: %d (expected %d)\n", ret, size);
        free(buffer);
        return -EIO;
    }

    sfx->buffer = buffer;
    sfx->size = size;

    return 0;
}

static void sfxInitDefaults(void)
{
    sfx_files[0].buffer = scrollV_adp;
    sfx_files[0].size = size_scrollV_adp;
    sfx_files[1].buffer = scrollH_adp;
    sfx_files[1].size = size_scrollH_adp;
    sfx_files[2].buffer = page_adp;
    sfx_files[2].size = size_page_adp;
    sfx_files[3].buffer = cancel_adp;
    sfx_files[3].size = size_cancel_adp;
    sfx_files[4].buffer = transition_adp;
    sfx_files[4].size = size_transition_adp;
    sfx_files[5].buffer = alert_adp;
    sfx_files[5].size = size_alert_adp;
    sfx_files[6].buffer = confirm_adp;
    sfx_files[6].size = size_confirm_adp;
}

//Returns 0 (AUDSRV_ERR_NOERROR) if the sound was loaded successfully.
static int sfxLoad(struct sfxEffect *sfxData, audsrv_adpcm_t *sfx)
{
    int ret;

    ret = audsrv_load_adpcm(sfx, sfxData->buffer, sfxData->size);
    free(sfxData->buffer);
    sfxData->buffer = NULL; //Mark the buffer as freed.

    return ret;
}

//Returns number of audio files successfully loaded, < 0 if an unrecoverable error occurred.
int sfxInit(void)
{
    char snd_path[256];
    char device_path[128];
    int ret, i, loaded;

    audsrv_adpcm_init();

    sfxInitDefaults();
    getAudioDevice(device_path);

    loaded = 0;
    for (i = 0; i < NUM_SFX_FILES; i++)
    {
        audsrv_adpcm_set_volume(i, gSFXVolume);

        if ((gAudioPath > 0))
        {
            sprintf(snd_path, "%s/%s", device_path, sfx_files[i].name);
            ret = sfxRead(snd_path, &sfx_files[i]);
            if (ret != 0)
            {
                printf("SFX: %s could not be loaded. Using default sound %d.\n", snd_path, ret);
            }
        }

        ret = sfxLoad(&sfx_files[i], &sfx[i]);
        if (ret == 0)
        {
            loaded++;
        }
        else
        {
           printf("SFX: failed to load %s, error %d\n", snd_path, ret);
        }
    }

    return loaded;
}
