/*--    Theme Sound Effects    ----------------------------------------------------------------------------------------------
----    SP193 wrote the code, I just made small changes.    ---------------------------------------------------------------*/

#include <audsrv.h>
#include "include/sound.h"
#include "include/opl.h"
#include "include/themes.h"

//default sfx
extern unsigned char boot_adp[];
extern unsigned int size_boot_adp;

extern unsigned char cancel_adp[];
extern unsigned int size_cancel_adp;

extern unsigned char confirm_adp[];
extern unsigned int size_confirm_adp;

extern unsigned char cursor_adp[];
extern unsigned int size_cursor_adp;

extern unsigned char message_adp[];
extern unsigned int size_message_adp;

extern unsigned char transition_adp[];
extern unsigned int size_transition_adp;

extern char sound_path[256];

struct sfxEffect {
    const char *name;
    void *buffer;
    int size;
};

static struct sfxEffect sfx_files[NUM_SFX_FILES] = {
    {"boot.adp"},
    {"cancel.adp"},
    {"confirm.adp"},
    {"cursor.adp"},
    {"message.adp"},
    {"transition.adp"},
};

struct audsrv_adpcm_t sfx[NUM_SFX_FILES];

//Returns 0 if the specified file was read. The sfxEffect structure will not be updated unless the file is successfully read.
static int sfxRead(const char *full_path, struct sfxEffect *sfx)
{
    FILE* adpcm;
    void *buffer;
    int ret, size;

    adpcm = fopen(full_path, "rb");
    if (adpcm == NULL)
    {
        printf("Failed to open adpcm file %s\n", full_path);
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
    sfx_files[0].buffer = boot_adp;
    sfx_files[0].size = size_boot_adp;
    sfx_files[1].buffer = cancel_adp;
    sfx_files[1].size = size_cancel_adp;
    sfx_files[2].buffer = confirm_adp;
    sfx_files[2].size = size_confirm_adp;
    sfx_files[3].buffer = cursor_adp;
    sfx_files[3].size = size_cursor_adp;
    sfx_files[4].buffer = message_adp;
    sfx_files[4].size = size_message_adp;
    sfx_files[5].buffer = transition_adp;
    sfx_files[5].size = size_transition_adp;
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

static int getFadeDelay(void)
{
    FILE* bootSnd;
    char boot_path[256];
    int len;
    int logoFadeTime = 1400; //fade time from sound call to fade to main in milliseconds
    int byteRate = 176400 / 1000; //sample rate * channels * bits per sample /8 (/1000 to get in milliseconds)

    sprintf(boot_path, "%s/%s", sound_path, sfx_files[0].name);
    bootSnd = fopen(boot_path, "rb");
    if (bootSnd == NULL)
    {
        printf("Failed to open adpcm file %s\n", boot_path);
        return -ENOENT;
    }

    fseek(bootSnd, 12, SEEK_SET);
    fread(&len, 4, 1, bootSnd);
    rewind(bootSnd);

    fclose(bootSnd);

    gFadeDelay = len / byteRate - logoFadeTime;

    return 0;
}

void sfxVolume(void)
{
    int i;

    for (i = 1; i < NUM_SFX_FILES; i++)
    {
        audsrv_adpcm_set_volume(i, gSFXVolume);
    }

    audsrv_adpcm_set_volume(0, gBootSndVolume);
}

//Returns number of audio files successfully loaded, < 0 if an unrecoverable error occurred.
int sfxInit(int bootSnd)
{
    char full_path[256];
    int ret, i, loaded;

    audsrv_adpcm_init();

    sfxInitDefaults();
    sfxVolume();
    //boot sound only needs to be read/loaded at init
    if (bootSnd) {
        ret = getFadeDelay();
        if (ret != 0) {
            gFadeDelay = 1200;
        }
        i = 0;
    }
    else {
        i = 1;
    }

    loaded = 0;
    for (; i < NUM_SFX_FILES; i++)
    {
        if (thmSfxEnabled < 0)
        {
            sprintf(full_path, "%s/%s", sound_path, sfx_files[i].name);
            ret = sfxRead(full_path, &sfx_files[i]);
            if (ret != 0)
            {
                printf("SFX: %s could not be loaded. Using default sound %d.\n", full_path, ret);
            }
        }

        ret = sfxLoad(&sfx_files[i], &sfx[i]);
        if (ret == 0)
        {
            loaded++;
        }
        else
        {
           printf("SFX: failed to load %s, error %d\n", full_path, ret);
        }
    }

    return loaded;
}
