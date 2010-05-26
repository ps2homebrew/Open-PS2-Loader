#ifndef __SUPPORT_BASE_H
#define __SUPPORT_BASE_H

#define BASE_GAME_NAME_MAX  		32
#define BASE_GAME_STARTUP_MAX  		12

typedef struct
{
	char name[BASE_GAME_NAME_MAX + 1];
	char startup[BASE_GAME_STARTUP_MAX + 1];
	unsigned char parts;
	unsigned char media;
	unsigned short isISO;
} base_game_info_t;

int sbIsSameSize(const char* prefix, int prevSize);
int sbGetCompatibility(base_game_info_t* game, int mode);
void sbSetCompatibility(base_game_info_t* game, int mode, int compatMode);
void sbReadList(base_game_info_t **list, const char* prefix, int *fsize, int* gamecount);
int sbPrepare(base_game_info_t* game, int mode, char* isoname, int size_cdvdman, void** cdvdman_irx, int* patchindex);

#endif
