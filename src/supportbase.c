#include "include/usbld.h"
#include "include/lang.h"
#include "include/util.h"
#include "include/iosupport.h"
#include "include/supportbase.h"

int sbIsSameSize(const char* prefix, int prevSize) {
	int size = -1;
	char path[64];
	snprintf(path, 64, "%sul.cfg", prefix);

	int fd = openFile(path, O_RDONLY);
	if (fd >= 0) {
		size = getFileSize(fd);
		fioClose(fd);
	}

	return size != prevSize;
}

int sbGetCompatibility(base_game_info_t* game, int mode) {
	char gkey[255];
	snprintf(gkey, 255, "%s_%d", game->startup, mode);

	unsigned int compatMode;
	if (!getConfigInt(&gConfig, gkey, &compatMode))
		compatMode = 0;

	return compatMode;
}

void sbSetCompatibility(base_game_info_t* game, int mode, int compatMode) {
	char gkey[255];
	snprintf(gkey, 255, "%s_%d", game->startup, mode);
	if (compatMode == 0)
		configRemoveKey(&gConfig, gkey);
	else
		setConfigInt(&gConfig, gkey, compatMode);
}

static int isValidIsoName(char *name)
{
	// SCUS_XXX.XX.ABCDEFGHIJKLMNOP.iso

	// Minimum is 17 char, GameID (11) + "." (1) + filename (1 min.) + ".iso" (4)
	int size = strlen(name);
	if ((size >= 17) && (name[4] == '_') && (name[8] == '.') && (name[11] == '.') && (stricmp(&name[size - 4], ".iso") == 0)) {
		size -= 16;
		if (size <= BASE_GAME_NAME_MAX)
			return size;
	}

	return 0;
}

static int scanForISO(char* path, char type, base_game_info_t **list, int index) {
	int fd, size, count = 0;
	fio_dirent_t record;

	if ((fd = fioDopen(path)) > 0) {
		while (fioDread(fd, &record) > 0)
			if ((size = isValidIsoName(record.name)) > 0) {
				if (index >= 0) {
					base_game_info_t *game = &(*list)[count + index];

					strncpy(game->name, &record.name[12], size);
					game->name[size] = '\0';
					strncpy(game->startup, record.name, BASE_GAME_STARTUP_MAX - 1);
					game->startup[BASE_GAME_STARTUP_MAX - 1] = '\0';
					game->parts = 0x01;
					game->media = type;
					game->isISO = 1;
				}
				count++;
			}
		fioDclose(fd);
	}

	return count;
}

void sbReadList(base_game_info_t **list, const char* prefix, int *fsize, int* gamecount) {
	int fd, size, id = 0;
	size_t count = 0;
	char path[64];

	free(*list);
	*list = NULL;
	*fsize = -1;
	*gamecount = 0;

	// count iso games in "cd" directory
	
	snprintf(path, 64, "%sCD\\\\", prefix); // *.iso
	count += scanForISO(path, 0x12, NULL, -1);

	// count iso games in "dvd" directory
	snprintf(path, 64, "%sDVD\\\\", prefix); // *.iso
	count += scanForISO(path, 0x14, NULL, -1);
        
        
	// count and process games in ul.cfg
	snprintf(path, 64, "%sul.cfg", prefix);
	fd = openFile(path, O_RDONLY);
	if(fd >= 0) {
		char buffer[0x040];

		size = getFileSize(fd);
		*fsize = size;
		count += size / 0x040;

		if (count > 0) {
			*list = (base_game_info_t*)malloc(sizeof(base_game_info_t) * count);

			while (size > 0) {
				fioRead(fd, buffer, 0x40);

				base_game_info_t *g = &(*list)[id++];

				// to ensure no leaks happen, we copy manually and pad the strings
				memcpy(g->name, buffer, BASE_GAME_NAME_MAX);
				g->name[BASE_GAME_NAME_MAX] = '\0';
				memcpy(g->startup, &buffer[BASE_GAME_NAME_MAX + 3], BASE_GAME_STARTUP_MAX);
				g->startup[BASE_GAME_STARTUP_MAX] = '\0';
				memcpy(&g->parts, &buffer[47], 1);
				memcpy(&g->media, &buffer[48], 1);
				g->isISO = 0;
				size -= 0x40;
			}
		}
		fioClose(fd);
	}
	else if (count > 0)
		*list = (base_game_info_t*)malloc(sizeof(base_game_info_t) * count);
        
	// process "cd" directory
	snprintf(path, 64, "%sCD\\\\", prefix); // *.iso
	id += scanForISO(path, 0x12, list, id);

	// process "dvd" directory
	snprintf(path, 64, "%sDVD\\\\", prefix); // *.iso
	id += scanForISO(path, 0x14, list, id);
        
        
	*gamecount = count;
}

int sbPrepare(base_game_info_t* game, int mode, char* isoname, int size_cdvdman, void** cdvdman_irx, int* patchindex) {
	int i;

	unsigned int compatmask = sbGetCompatibility(game, mode);

	char gameid[5];
	getConfigDiscIDBinary(game->startup, gameid);

	if (game->isISO)
		strcpy(isoname, game->startup);
	else {
		char gamename[BASE_GAME_NAME_MAX + 1];
		memset(gamename, 0, BASE_GAME_NAME_MAX + 1);
		strncpy(gamename, game->name, BASE_GAME_NAME_MAX);
		sprintf(isoname,"ul.%08X.%s", USBA_crc32(gamename), game->startup);
	}

	for (i = 0; i < size_cdvdman; i++) {
		if (!strcmp((const char*)((u32)cdvdman_irx + i),"######    GAMESETTINGS    ######")) {
			break;
		}
	}

	memcpy((void*)((u32)cdvdman_irx + i), isoname, strlen(isoname) + 1);
	memcpy((void*)((u32)cdvdman_irx + i + 33), &game->parts, 1);
	memcpy((void*)((u32)cdvdman_irx + i + 34), &game->media, 1);
	if (compatmask & COMPAT_MODE_2) {
		u32 alt_read_mode = 1;
		memcpy((void*)((u32)cdvdman_irx + i + 35), &alt_read_mode, 1);
	}
	if (compatmask & COMPAT_MODE_5) {
		u32 no_dvddl = 1;
		memcpy((void*)((u32)cdvdman_irx + i + 36), &no_dvddl, 4);
	}
	if (compatmask & COMPAT_MODE_4) {
		u32 no_pss = 1;
		memcpy((void*)((u32)cdvdman_irx + i + 40), &no_pss, 4);
	}

	// game id
	memcpy((void*)((u32)cdvdman_irx + i + 84), &gameid, 5);

	*patchindex = i;
	return compatmask;
}
