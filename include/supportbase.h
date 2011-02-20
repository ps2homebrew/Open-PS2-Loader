#ifndef __SUPPORT_BASE_H
#define __SUPPORT_BASE_H

#define UL_GAME_NAME_MAX	32
#define ISO_GAME_NAME_MAX	64
#define GAME_STARTUP_MAX	12

typedef struct
{
	char name[ISO_GAME_NAME_MAX + 1]; // MUST be the higher value from UL / ISO
	char startup[GAME_STARTUP_MAX + 1];
	unsigned char parts;
	unsigned char media;
	unsigned short isISO;
	int sizeMB;
} base_game_info_t;

int sbIsSameSize(const char* prefix, int prevSize);
void sbReadList(base_game_info_t **list, const char* prefix, int *fsize, int* gamecount);
int sbPrepare(base_game_info_t* game, int mode, char* isoname, int size_cdvdman, void** cdvdman_irx, int* patchindex);
void sbDelete(base_game_info_t **list, const char* prefix, const char* sep, int gamecount, int id);
void sbRename(base_game_info_t **list, const char* prefix, const char* sep, int gamecount, int id, char* newname);
config_set_t* sbPopulateConfig(base_game_info_t* game, const char* prefix, const char* sep);

#endif
