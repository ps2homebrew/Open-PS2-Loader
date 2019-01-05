#ifndef __SUPPORT_BASE_H
#define __SUPPORT_BASE_H

#define UL_GAME_NAME_MAX 32
#define ISO_GAME_NAME_MAX 64
#define ISO_GAME_EXTENSION_MAX 4
#define GAME_STARTUP_MAX 12

#define ISO_GAME_FNAME_MAX (ISO_GAME_NAME_MAX+ISO_GAME_EXTENSION_MAX)

enum GAME_FORMAT {
    GAME_FORMAT_USBLD = 0,
    GAME_FORMAT_OLD_ISO,
    GAME_FORMAT_ISO,
};

typedef struct
{
    char name[ISO_GAME_NAME_MAX + 1]; // MUST be the higher value from UL / ISO
    char startup[GAME_STARTUP_MAX + 1];
    char extension[ISO_GAME_EXTENSION_MAX + 1];
    u8 parts;
    u8 media;
    u8 format;
    u32 sizeMB;
} base_game_info_t;

typedef struct
{
    char name[UL_GAME_NAME_MAX];
    char startup[15];
    u8 parts;
    u8 media; //Disc type
    u8 unknown[4];
    u8 Byte08; //Always 0x08
    u8 unknown2[10];
} USBExtreme_game_entry_t;

int sbIsSameSize(const char *prefix, int prevSize);
int sbCreateSemaphore(void);
int sbReadList(base_game_info_t **list, const char *prefix, int *fsize, int *gamecount);
int sbPrepare(base_game_info_t *game, config_set_t *configSet, int size_cdvdman, void **cdvdman_irx, int *patchindex);
void sbUnprepare(void *pCommon);
void sbRebuildULCfg(base_game_info_t **list, const char *prefix, int gamecount, int excludeID);
void sbDelete(base_game_info_t **list, const char *prefix, const char *sep, int gamecount, int id);
void sbRename(base_game_info_t **list, const char *prefix, const char *sep, int gamecount, int id, char *newname);
config_set_t *sbPopulateConfig(base_game_info_t *game, const char *prefix, const char *sep);
void sbCreateFolders(const char *path, int createDiscImgFolders);

//ISO9660 filesystem management functions.
u32 sbGetISO9660MaxLBA(const char *path);
int sbProbeISO9660(const char *path, base_game_info_t *game, u32 layer1_offset);
int sbProbeISO9660_64(const char *path, base_game_info_t *game, u32 layer1_offset);

int sbLoadCheats(const char *path, const char *file);

#endif
