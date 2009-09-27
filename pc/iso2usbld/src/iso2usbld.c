
#include "iso2usbld.h"

#define WR_SIZE 	524288

u32 crctab[0x400];
u8 systemcnf_buf[65536];

//-----------------------------------------------------------------------
void printVer(void)
{
	printf("%s version %s\n", PROGRAM_EXTNAME, PROGRAM_VER);
}

//-----------------------------------------------------------------------
void printUsage(void)
{
	printf("Usage: %s [SOURCE_ISO] [DEST_DRIVE] [GAME_NAME] [TYPE]\n", PROGRAM_NAME);
	printf("%s command-line version %s\n\n", PROGRAM_EXTNAME, PROGRAM_VER);
	printf("Example 1: %s C:\\ISO\\WORMS4.ISO E WORMS_4_MAYHEM DVD\n", PROGRAM_NAME);	
	printf("Example 2: %s \"C:\\ISO\\WORMS 4.ISO\" E \"WORMS 4: MAYHEM\" DVD\n", PROGRAM_NAME);		
	printf("Example 3: %s \"C:\\ISO\\MICRO MACHINES V4.ISO\" E \"Micro Machines v4\" CD\n", PROGRAM_NAME);			
}

//-----------------------------------------------------------------------
u32 crc32(const char *string)
{
    int crc, table, count, byte;

    for (table=0; table<256; table++) {
        crc = table << 24;

        for (count=8; count>0; count--) {
            if (crc < 0) crc = crc << 1;
            else crc = (crc << 1) ^ 0x04C11DB7;
        }
        crctab[255-table] = crc;
    }

    do {
        byte = string[count++];
        crc = crctab[byte ^ ((crc >> 24) & 0xFF)] ^ ((crc << 8) & 0xFFFFFF00);
    } while (string[count-1] != 0);

    return crc;
}

//-----------------------------------------------------------------------
int check_cfg(const char *drive, const char *game_name, const char *game_id)
{
	int r;
	FILE *fh_cfg;
	cfg_t cfg;
	char cfg_path[256];
	char cfg_image[256];	

#ifdef DEBUG
	printf("check_cfg drive:%s name:%s id:%s\n", drive, game_name, game_id);
#endif
	
	sprintf(cfg_path, "%s:\\ul.cfg", drive);
	sprintf(cfg_image, "ul.%s", game_id);

	fh_cfg = fopen(cfg_path, "rb");
	if (fh_cfg) {
		while ((r = fread(&cfg, 1, sizeof(cfg_t), fh_cfg)) != 0) {
			if (r != sizeof(cfg_t)) {
				fclose(fh_cfg);
				return -3;				
			}
			if (!strcmp(cfg.name, game_name)) {
				fclose(fh_cfg);
				return -1;
			}
			if (!strcmp(cfg.image, cfg_image)) {
				fclose(fh_cfg);
				return -2;
			}			
		}
		fclose(fh_cfg);
	}		
		
	return 0;
}

//-----------------------------------------------------------------------
int write_cfg(const char *drive, const char *game_name, const char *game_id, const char *media, int parts)
{
	FILE *fh_cfg;
	cfg_t cfg;
	char cfg_path[256];
	int r;

#ifdef DEBUG
	printf("write_cfg drive:%s name:%s id:%s media:%s parts:%d\n", drive, game_name, game_id, media, parts);
#endif
	
	sprintf(cfg_path, "%s:\\ul.cfg", drive);
	
	memset(&cfg, 0, sizeof(cfg_t));
	
	strncpy(cfg.name, game_name, 32);
	sprintf(cfg.image, "ul.%s", game_id);
	cfg.parts = parts;
	cfg.pad[4] = 0x08; // To be like USBA 
	
	if (!strcmp(media, "CD"))
		cfg.media = 0x12;
	else if (!strcmp(media, "DVD"))
		cfg.media = 0x14;	
	
	fh_cfg = fopen(cfg_path, "ab");
	if (!fh_cfg)
		return -1;
		
	r = fwrite(&cfg, 1, sizeof(cfg_t), fh_cfg);
	if (r != sizeof(cfg_t)) {
		fclose(fh_cfg);
		return -2;
	}
	
	fclose(fh_cfg);		
	
	return 0;
}

//----------------------------------------------------------------
int write_parts(const char *drive, const char *game_name, const char *game_id, u32 filesize, int parts)
{
	FILE *fh_part;
	char part_path[256];
	int i, r;
	u8 *buf;
	u32 nbytes, size, rpos, iso_pos;

#ifdef DEBUG
	printf("write_parts drive:%s name:%s id:%s filesize:0x%x parts:%d\n", drive, game_name, game_id, filesize, parts);
#endif
		
	iso_pos = 0;
	buf = malloc(WR_SIZE+2048);
	if (!buf)
		return -1;
		
	for (i=0; i<parts; i++) {
		sprintf(part_path, "%s:\\ul.%08X.%s.%02d", drive, crc32(game_name), game_id, i);

		fh_part = fopen(part_path, "wb");
		if (!fh_part) {
			free(buf);
			return -2;
		}
			
		nbytes = filesize;
		if (nbytes > 1073741824)
			nbytes = 1073741824;	
			
		rpos = 0;	
		if (nbytes) {
			do {
				if (nbytes > WR_SIZE)
					size = WR_SIZE;
				else	
					size = nbytes;
	
				r = isofs_ReadISO(iso_pos, size, buf);
				if (r != size) {
					free(buf);
					fclose(fh_part);
    				return -3;
				}				
				
				printf("Writing %d sectors to %s - LBA: %d\n", WR_SIZE >> 11, part_path, iso_pos >> 11);
				
				// write to file	
				r = fwrite(buf, 1, size, fh_part);			
				if (r != size) {
					free(buf);
					fclose(fh_part);
    				return -4;
				}
				
				size = r;		
				rpos += size;
				iso_pos += size;
				nbytes -= size;
			
			} while (nbytes);	
		}
		filesize -= rpos;				
		fclose(fh_part);			
	}
	
	free(buf);
	
	return 0;
}

//----------------------------------------------------------------
int ParseSYSTEMCNF(char *system_cnf, char *boot_path)
{
	int fd, r, fsize;
	char *p, *p2;
	int path_found = 0;

#ifdef DEBUG
	printf("ParseSYSTEMCNF %s\n", system_cnf);
#endif
		
	fd = isofs_Open("\\SYSTEM.CNF;1");
	if (fd < 0)
		return -1;
		
	fsize = isofs_Seek(fd, 0, SEEK_END);
	isofs_Seek(fd, 0, SEEK_SET);
	
	r = isofs_Read(fd, systemcnf_buf, fsize);
	if (r != fsize) {
		isofs_Close(fd);
		return -2;
	}
		
	isofs_Close(fd);
	
#ifdef DEBUG
	printf("ParseSYSTEMCNF trying to retrieve elf path...\n");
#endif
		
	p = strtok((char *)systemcnf_buf, "\n");
	
	while (p) {
		p2 = strstr(p, "BOOT2");
		if (p2) {
			p2 += 5;
			
			while ((*p2 <= ' ') && (*p2 > '\0'))
				p2++;
				
			if (*p2 != '=')
				return -3;
			p2++;

			while ((*p2 <= ' ') && (*p2 > '\0')	&& (*p2!='\r') && (*p2!='\n'))
				p2++;
											
			if (*p2 == '\0')
				return -3;
			
			path_found = 1;	
			strcpy(boot_path, p2);
		}
		p = strtok(NULL, "\n");
	}
	
	if (!path_found)
		return -4;	
	
	return 0;
}

//-----------------------------------------------------------------------
int main(int argc, char **argv, char **env)
{
	int ret;
	char ElfPath[256];
	int num_parts;
	char *p, *GameID;
	u32 filesize;
	
	// args check
	if ((argc != 5) || (strcmp(argv[4], "CD") && strcmp(argv[4], "DVD")) \
		|| (strlen(argv[2]) != 1) || (strlen(argv[3]) > 32)) {
	
		printUsage();
		exit(EXIT_FAILURE);
	}
	
#ifdef DEBUG
	printf("DEBUG_MODE ON\n");
	printf("isofs Init...\n");	
#endif
	
	// Init isofs
	filesize = isofs_Init(argv[1]); 
	if (!filesize) {
		printf("Error: failed to open ISO file!\n");
		exit(EXIT_FAILURE);
	}

	// get needed number of parts
	num_parts = filesize >> 30;
	if (filesize & 0x3fffffff)
		num_parts++;

#ifdef DEBUG
	printf("ISO filesize: 0x%x\n", filesize);
	printf("Number of parts: %d\n", num_parts);
#endif
								
	// parse system.cnf in ISO file
	ret = ParseSYSTEMCNF("\\SYSTEM.CNF;1", ElfPath);	
	if (ret < 0) {
		switch (ret) {
			case -1:
				printf("Error: can't open SYSTEM.CNF from ISO file!\n");
				break;
			case -2:
				printf("Error: failed to read SYSTEM.CNF from ISO file!\n");
				break;
			case -3:
				printf("Error: failed to parse SYSTEM.CNF from ISO file!\n");
				break;				
			case -4:
				printf("Error: failed to locate elf path from ISO file!\n");
				break;								
		}
		isofs_Reset();
		exit(EXIT_FAILURE);		
	}	
	
#ifdef DEBUG
	printf("Elf Path: %s\n", ElfPath);
#endif
	
	// get GameID
	GameID = strtok(ElfPath, "cdrom0:\\");
	p = strstr(GameID, ";1");
	*p = 0;

#ifdef DEBUG
	printf("Game ID: %s\n", GameID);
#endif
		
	// check for existing game
	ret = check_cfg(argv[2], argv[3], GameID);
	if (ret < 0) {
		switch (ret) {
			case -1:
				printf("Error: a game with the same name is already installed on drive!\n");
				break;
			case -2:
				printf("Error: a game with the same ID is already installed on drive!\n");
				break;
			case -3:
				printf("Error: can't read ul.cfg on drive!\n");
				break;				
		}
		isofs_Reset();
		exit(EXIT_FAILURE);		
	}

	// write ISO parts to drive
	ret = write_parts(argv[2], argv[3], GameID, filesize, num_parts);
	if (ret < 0) {
		switch (ret) {
			case -1:
				printf("Error: failed to allocate memory to read ISO!\n");
				break;			
			case -2:
				printf("Error: game part creation failed!\n");
				break;
			case -3:
				printf("Error: failed to read datas from ISO file!\n");
				break;
			case -4:
				printf("Error: failed to write datas to part file!\n");
				break;				
		}
		isofs_Reset();
		exit(EXIT_FAILURE);		
	}
		
	// append the game to ul.cfg
	ret = write_cfg(argv[2], argv[3], GameID, argv[4], num_parts);
	if (ret < 0) {
		switch (ret) {
			case -1:
				printf("Error: can't open ul.cfg!\n");
				break;
			case -2:
				printf("Error: write to ul.cfg failed!\n");
				break;
		}
		isofs_Reset();
		exit(EXIT_FAILURE);		
	}

	isofs_Reset();
		
	printf("%s is installed!\n", argv[3]);
	
	// End program
	exit(EXIT_SUCCESS);

	return 0;
}
