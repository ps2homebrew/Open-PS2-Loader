/*--    Theme Sound Effects    ----------------------------------------------------------------------------------------------
----    SP193 wrote the code, I just made small changes.    ---------------------------------------------------------------*/

#include <audsrv.h>
#include "include/sound.h"
#include "include/opl.h"
#include "include/ioman.h"
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

struct sfxEffect {
    const char *name;
    void *buffer;
    int size;
    int builtin;
};

static struct sfxEffect sfx_files[SFX_COUNT] = {
    {"boot.adp"},
    {"cancel.adp"},
    {"confirm.adp"},
    {"cursor.adp"},
    {"message.adp"},
    {"transition.adp"},
};

static struct audsrv_adpcm_t sfx[SFX_COUNT];

//Returns 0 if the specified file was read. The sfxEffect structure will not be updated unless the file is successfully read.
static int sfxRead(const char *full_path, struct sfxEffect *sfx)
{
    FILE* adpcm;
    void *buffer;
    int ret, size;

    LOG("sfxRead('%s')\n", full_path);

    adpcm = fopen(full_path, "rb");
    if (adpcm == NULL)
    {
        LOG("Failed to open adpcm file %s\n", full_path);
        return -ENOENT;
    }

    fseek(adpcm, 0, SEEK_END);
    size = ftell(adpcm);
    rewind(adpcm);

    buffer = memalign(64, size);
    if (buffer == NULL)
    {
        LOG("Failed to allocate memory for SFX\n");
        fclose(adpcm);
        return -ENOMEM;
    }

    ret = fread(buffer, 1, size, adpcm);
    fclose(adpcm);

    if(ret != size)
    {
        LOG("Failed to read SFX: %d (expected %d)\n", ret, size);
        free(buffer);
        return -EIO;
    }

    sfx->buffer = buffer;
    sfx->size = size;
    sfx->builtin = 0;

    return 0;
}

static void sfxInitDefaults(void)
{
    sfx_files[SFX_BOOT].buffer = boot_adp;
    sfx_files[SFX_BOOT].size = size_boot_adp;
    sfx_files[SFX_BOOT].builtin = 1;
    sfx_files[SFX_CANCEL].buffer = cancel_adp;
    sfx_files[SFX_CANCEL].size = size_cancel_adp;
    sfx_files[SFX_CANCEL].builtin = 1;
    sfx_files[SFX_CONFIRM].buffer = confirm_adp;
    sfx_files[SFX_CONFIRM].size = size_confirm_adp;
    sfx_files[SFX_CONFIRM].builtin = 1;
    sfx_files[SFX_CURSOR].buffer = cursor_adp;
    sfx_files[SFX_CURSOR].size = size_cursor_adp;
    sfx_files[SFX_CURSOR].builtin = 1;
    sfx_files[SFX_MESSAGE].buffer = message_adp;
    sfx_files[SFX_MESSAGE].size = size_message_adp;
    sfx_files[SFX_MESSAGE].builtin = 1;
    sfx_files[SFX_TRANSITION].buffer = transition_adp;
    sfx_files[SFX_TRANSITION].size = size_transition_adp;
    sfx_files[SFX_TRANSITION].builtin = 1;
}

//Returns 0 (AUDSRV_ERR_NOERROR) if the sound was loaded successfully.
static int sfxLoad(struct sfxEffect *sfxData, audsrv_adpcm_t *sfx)
{
    int ret;

    ret = audsrv_load_adpcm(sfx, sfxData->buffer, sfxData->size);
    if (sfxData->builtin == 0) {
        free(sfxData->buffer);
        sfxData->buffer = NULL; //Mark the buffer as freed.
    }

    return ret;
}

static int getFadeDelay(char *sound_path)
{
    FILE* bootSnd;
    char boot_path[256];
    int len;
    int logoFadeTime = 1400; //fade time from sound call to fade to main in milliseconds
    int byteRate = 176400 / 1000; //sample rate * channels * bits per sample /8 (/1000 to get in milliseconds)

    snprintf(boot_path, sizeof(boot_path), "%s/%s", sound_path, sfx_files[SFX_BOOT].name);
    bootSnd = fopen(boot_path, "rb");
    if (bootSnd == NULL)
    {
        LOG("Failed to open adpcm file %s\n", boot_path);
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

    for (i = 1; i < SFX_COUNT; i++)
    {
        audsrv_adpcm_set_volume(i, gSFXVolume);
    }

    audsrv_adpcm_set_volume(0, gBootSndVolume);
}

//Returns number of audio files successfully loaded, < 0 if an unrecoverable error occurred.
int sfxInit(int bootSnd)
{
    char sound_path[256];
    char full_path[256];
    int ret, loaded;
    int thmSfxEnabled = 0;
    int i = 1;

    audsrv_adpcm_init();

    sfxInitDefaults();
    sfxVolume();

    //Check default theme is not current theme
    int themeID = thmGetGuiValue();
    if (themeID != 0) {
        //Get theme path for sfx
        char *thmPath = thmGetFilePath(themeID);
        snprintf(sound_path, sizeof(sound_path), "%ssound", thmPath);

        //Check for custom sfx folder
        int fd = fileXioDopen(sound_path);
        if (fd >= 0)
            thmSfxEnabled = 1;

        fileXioDclose(fd);
    }

    //boot sound only needs to be read/loaded at init
    if (bootSnd) {
        i = 0;
        ret = getFadeDelay(sound_path);
        if (ret != 0)
            gFadeDelay = 1200;
    }

    loaded = 0;
    for (; i < SFX_COUNT; i++)
    {
        if (thmSfxEnabled)
        {
            snprintf(full_path, sizeof(full_path), "%s/%s", sound_path, sfx_files[i].name);
            ret = sfxRead(full_path, &sfx_files[i]);
            if (ret != 0)
            {
                LOG("SFX: %s could not be loaded. Using default sound %d.\n", full_path, ret);
            }
        }

        ret = sfxLoad(&sfx_files[i], &sfx[i]);
        if (ret == 0)
        {
            loaded++;
        }
        else
        {
           LOG("SFX: failed to load %s, error %d\n", full_path, ret);
        }
    }

    return loaded;
}

void sfxPlay(int id)
{
    if (gEnableSFX) {
        audsrv_ch_play_adpcm(id, &sfx[id]);
    }
}
