#include "include/usbld.h"
#include "include/lang.h"
#include "include/util.h"
#include "include/iosupport.h"
#include "include/system.h"
#include "include/supportbase.h"

/// internal linked list used to populate the list from directory listing
struct game_list_t {
	base_game_info_t gameinfo;
	struct game_list_t *next;
};

int sbIsSameSize(const char* prefix, int prevSize) {
	int size = -1;
	char path[255];
	snprintf(path, 255, "%sul.cfg", prefix);

	int fd = openFile(path, O_RDONLY);
	if (fd >= 0) {
		size = getFileSize(fd);
		fioClose(fd);
	}

	return size == prevSize;
}

static int isValidIsoName(char *name)
{
	// SCUS_XXX.XX.ABCDEFGHIJKLMNOP.iso

	// Minimum is 17 char, GameID (11) + "." (1) + filename (1 min.) + ".iso" (4)
	int size = strlen(name);
	if ((size >= 17) && (name[4] == '_') && (name[8] == '.') && (name[11] == '.') && (stricmp(&name[size - 4], ".iso") == 0)) {
		size -= 16;
		if (size <= ISO_GAME_NAME_MAX)
			return size;
	}

	return 0;
}

static int scanForISO(char* path, char type, struct game_list_t** glist) {
	int fd, size, count = 0;
	fio_dirent_t record;

	if ((fd = fioDopen(path)) > 0) {
		while (fioDread(fd, &record) > 0) {
			if ((size = isValidIsoName(record.name)) > 0) {
				struct game_list_t *next = (struct game_list_t*)malloc(sizeof(struct game_list_t));
				
				next->next = *glist;
				*glist = next;
				
				base_game_info_t *game = &(*glist)->gameinfo;

				strncpy(game->name, &record.name[GAME_STARTUP_MAX], size);
				game->name[size] = '\0';
				strncpy(game->startup, record.name, GAME_STARTUP_MAX - 1);
				game->startup[GAME_STARTUP_MAX - 1] = '\0';
				game->parts = 0x01;
				game->media = type;
				game->isISO = 1;

				count++;
			}
		}
		fioDclose(fd);
	}

	return count;
}

void sbReadList(base_game_info_t **list, const char* prefix, int *fsize, int* gamecount) {
	int fd, size, id = 0;
	size_t count = 0;
	char path[255];

	free(*list);
	*list = NULL;
	*fsize = -1;
	*gamecount = 0;

	// temporary storage for the game names
	struct game_list_t *dlist_head = NULL;
	
	// count iso games in "cd" directory
	snprintf(path, 255, "%sCD", prefix);
	count += scanForISO(path, 0x12, &dlist_head);

	// count iso games in "dvd" directory
	snprintf(path, 255, "%sDVD", prefix);
	count += scanForISO(path, 0x14, &dlist_head);
        
        
	// count and process games in ul.cfg
	snprintf(path, 255, "%sul.cfg", prefix);
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
				memcpy(g->name, buffer, UL_GAME_NAME_MAX);
				g->name[UL_GAME_NAME_MAX] = '\0';
				memcpy(g->startup, &buffer[UL_GAME_NAME_MAX + 3], GAME_STARTUP_MAX);
				g->startup[GAME_STARTUP_MAX] = '\0';
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
        
	// copy the dlist into the list
	while ((id < count) && dlist_head) {
		// copy one game, advance
		struct game_list_t *cur = dlist_head;
		dlist_head = dlist_head->next;
		
		memcpy(&(*list)[id++], &cur->gameinfo, sizeof(base_game_info_t));
		free(cur);
	}
        
	*gamecount = count;
}

int sbPrepare(base_game_info_t* game, int mode, char* isoname, int size_cdvdman, void** cdvdman_irx, int* patchindex) {
	int i;

	unsigned int compatmask = configGetCompatibility(game->startup, mode, NULL);

	char gameid[5];
	configGetDiscIDBinary(game->startup, gameid);

	if (game->isISO)
		strcpy(isoname, game->startup);
	else {
		char gamename[UL_GAME_NAME_MAX + 1];
		memset(gamename, 0, UL_GAME_NAME_MAX + 1);
		strncpy(gamename, game->name, UL_GAME_NAME_MAX);
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

	*patchindex = i;

	for (i = 0; i < size_cdvdman; i++) {
		if (!strcmp((const char*)((u32)cdvdman_irx + i),"B00BS")) {
			break;
		}
	}
	// game id
	memcpy((void*)((u32)cdvdman_irx + i), &gameid, 5);

	// patches cdvdfsv
	void *cdvdfsv_irx;
	int size_cdvdfsv_irx;

	sysGetCDVDFSV(&cdvdfsv_irx, &size_cdvdfsv_irx);
	u32 *p = (u32 *)cdvdfsv_irx;
	for (i = 0; i < (size_cdvdfsv_irx >> 2); i++) {
		if (*p == 0xC0DEC0DE) {
			if (compatmask & COMPAT_MODE_7)
				*p = 1;
			else
				*p = 0;
			break;
		}
		p++;
	}

	return compatmask;
}

static void sbRebuildULCfg(base_game_info_t **list, const char* prefix, int gamecount, int excludeID) {
	char path[255];
	snprintf(path, 255, "%sul.cfg", prefix);

	file_buffer_t* fileBuffer = openFileBuffer(path, O_WRONLY | O_CREAT | O_TRUNC, 0, 4096);
	if (fileBuffer) {
		int i;
		char buffer[0x40];
		base_game_info_t* game;

		memset(buffer, 0, 0x40);
		buffer[32] = 0x75; // u
		buffer[33] = 0x6C; // l
		buffer[34] = 0x2E; // .
		buffer[53] = 0x08; // just to be compatible with original ul.cfg

		for (i = 0; i < gamecount; i++) {
			game = &(*list)[i];

			if (!game->isISO  && (i != excludeID)) {
				memset(buffer, 0, UL_GAME_NAME_MAX);
				memset(&buffer[UL_GAME_NAME_MAX + 3], 0, GAME_STARTUP_MAX);

				memcpy(buffer, game->name, UL_GAME_NAME_MAX);
				memcpy(&buffer[UL_GAME_NAME_MAX + 3], game->startup, GAME_STARTUP_MAX);
				buffer[47] = game->parts;
				buffer[48] = game->media;

				writeFileBuffer(fileBuffer, buffer, 0x40);
			}
		}

		closeFileBuffer(fileBuffer);
	}
}

void sbDelete(base_game_info_t **list, const char* prefix, const char* sep, int gamecount, int id) {
	char path[255];
	base_game_info_t* game = &(*list)[id];

	if (game->isISO) {
		if (game->media == 0x12)
			snprintf(path, 255, "%sCD%s%s.%s.iso", prefix, sep, game->startup, game->name);
		else
			snprintf(path, 255, "%sDVD%s%s.%s.iso", prefix, sep, game->startup, game->name);
		fileXioRemove(path);
	} else {
		char *pathStr = "%sul.%08X.%s.%02x";
		unsigned int crc = USBA_crc32(game->name);
		int i = 0;
		do {
			snprintf(path, 255, pathStr, prefix, crc, game->startup, i++);
			fileXioRemove(path);
		} while(i < game->parts);

		sbRebuildULCfg(list, prefix, gamecount, id);
	}
}

void sbRename(base_game_info_t **list, const char* prefix, const char* sep, int gamecount, int id, char* newname) {
	char oldpath[255], newpath[255];
	base_game_info_t* game = &(*list)[id];

	if (game->isISO) {
		if (game->media == 0x12) {
			snprintf(oldpath, 255, "%sCD%s%s.%s.iso", prefix, sep, game->startup, game->name);
			snprintf(newpath, 255, "%sCD%s%s.%s.iso", prefix, sep, game->startup, newname);
		} else {
			snprintf(oldpath, 255, "%sDVD%s%s.%s.iso", prefix, sep, game->startup, game->name);
			snprintf(newpath, 255, "%sDVD%s%s.%s.iso", prefix, sep, game->startup, newname);
		}
		fileXioRename(oldpath, newpath);
	} else {
		memset(game->name, 0, UL_GAME_NAME_MAX);
		memcpy(game->name, newname, UL_GAME_NAME_MAX);

		char *pathStr = "%sul.%08X.%s.%02x";
		unsigned int oldcrc = USBA_crc32(game->name);
		unsigned int newcrc = USBA_crc32(newname);
		int i = 0;
		do {
			snprintf(oldpath, 255, pathStr, prefix, oldcrc, game->startup, i);
			snprintf(newpath, 255, pathStr, prefix, newcrc, game->startup, i++);
			fileXioRename(oldpath, newpath);
		} while(i < game->parts);

		sbRebuildULCfg(list, prefix, gamecount, -1);
	}
}

void sbPopulateConfig(base_game_info_t* game, config_set_t* config) {
	if (game->isISO)
		configSetStr(config, "#Format", "ISO");
	else
		configSetStr(config, "#Format", "UL");

	if (game->media == 0x12)
		configSetStr(config, "#Media", "CD");
	else
		configSetStr(config, "#Media", "DVD");

	configSetStr(config, "#Name", game->name);
	configSetStr(config, "#Startup", game->startup);
	// TODO IZD add size
	// TODO IZD load real config file
}
