/*
  Copyright 2009, jimmikaelkael
  Copyright (c) 2002, A.Lee & Nicholas Van Veen
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from libcdvd by A.Lee & Nicholas Van Veen
  Review license_libcdvd file for further details.
*/

#include "iso2opl.h"

#define WR_SIZE 524288

u32 crctab[0x400];
u8 systemcnf_buf[65536];

//-----------------------------------------------------------------------
void printVer(void)
{
#ifdef _WIN32
    printf("%s version %s (Win32 Build)\n", PROGRAM_EXTNAME, PROGRAM_VER);
#else
    printf("%s version %s\n", PROGRAM_EXTNAME, PROGRAM_VER);
#endif
}

//-----------------------------------------------------------------------
void printUsage(void)
{
    printVer();
    printf("Usage: %s [SOURCE_ISO] [DEST_DRIVE] [GAME_NAME] [TYPE]\n", PROGRAM_NAME);
    printf("%s command-line version %s\n\n", PROGRAM_EXTNAME, PROGRAM_VER);
#ifdef _WIN32
    printf("Example 1: %s C:\\ISO\\WORMS4.ISO E WORMS_4_MAYHEM DVD\n", PROGRAM_NAME);
    printf("Example 2: %s \"C:\\ISO\\WORMS 4.ISO\" E \"WORMS 4: MAYHEM\" DVD\n", PROGRAM_NAME);
    printf("Example 3: %s \"C:\\ISO\\MICRO MACHINES V4.ISO\" E \"Micro Machines v4\" CD\n", PROGRAM_NAME);
    printf("Example 4: %s \"C:\\ISO\\WORMS 4.ISO\" E:\\MyDir WORMS_4 DVD\n", PROGRAM_NAME);
    printf("Example 5: %s \"C:\\ISO\\WORMS 4.ISO\" \\\\MyComputer\\PS2SMB WORMS_4 DVD\n", PROGRAM_NAME);
#else
    printf("Example 1: %s /home/user/WORMS4.ISO /media/disk WORMS_4_MAYHEM DVD\n", PROGRAM_NAME);
    printf("Example 2: %s \"/home/user/WORMS 4.ISO\" /media/disk \"WORMS 4: MAYHEM\" DVD\n", PROGRAM_NAME);
    printf("Example 3: %s \"/home/user/MICRO MACHINES V4.ISO\" /media/disk \"Micro Machines v4\" CD\n", PROGRAM_NAME);
#endif
}

//-----------------------------------------------------------------------
u32 crc32(const char *string)
{
    int crc, table, count, byte;

    for (table = 0; table < 256; table++) {
        crc = table << 24;

        for (count = 8; count > 0; count--) {
            if (crc < 0)
                crc = crc << 1;
            else
                crc = (crc << 1) ^ 0x04C11DB7;
        }
        crctab[255 - table] = crc;
    }

    do {
        byte = string[count++];
        crc = crctab[byte ^ ((crc >> 24) & 0xFF)] ^ ((crc << 8) & 0xFFFFFF00);
    } while (string[count - 1] != 0);

    return crc;
}

//-----------------------------------------------------------------------
int check_cfg(const char *drive, const char *game_name, const char *game_id)
{
    int r;
    // int fh_cfg;
    FILE *fh_cfg;
    cfg_t cfg;
    char cfg_path[256];
    char cfg_image[256];

#ifdef DEBUG
    printf("check_cfg drive:%s name:%s id:%s\n", drive, game_name, game_id);
#endif

#ifdef _WIN32
    sprintf(cfg_path, "%s:\\ul.cfg", drive);
#else
    sprintf(cfg_path, "%s/ul.cfg", drive);
#endif
    sprintf(cfg_image, "ul.%s", game_id);


    /* fh_cfg = open(cfg_path, O_RDONLY);
    if (fh_cfg >= 0) {
        while ((r = read(fh_cfg, &cfg, sizeof(cfg_t))) != 0) {
            if (r != sizeof(cfg_t)) {
                close(fh_cfg);
                return -3;
            }
            if (!strcmp(cfg.name, game_name)) {
                close(fh_cfg);
                return -1;
            }
            if (!strcmp(cfg.image, cfg_image)) {
                close(fh_cfg);
                return -2;
            }
        }
        close(fh_cfg);
    } */

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
    // int fh_cfg;
    FILE *fh_cfg;
    cfg_t cfg;
    char cfg_path[256];
    int r;

#ifdef DEBUG
    printf("write_cfg drive:%s name:%s id:%s media:%s parts:%d\n", drive, game_name, game_id, media, parts);
#endif

#ifdef _WIN32
    if (strlen(drive) == 1)
        sprintf(cfg_path, "%s:\\ul.cfg", drive);
    else
        sprintf(cfg_path, "%s\\ul.cfg", drive);
#else
    sprintf(cfg_path, "%s/ul.cfg", drive);
#endif
    memset(&cfg, 0, sizeof(cfg_t));

    strncpy(cfg.name, game_name, 32);
    sprintf(cfg.image, "ul.%s", game_id);
    cfg.parts = parts;
    cfg.pad[4] = 0x08; // To be like USBA

    if (!strcmp(media, "CD"))
        cfg.media = 0x12;
    else if (!strcmp(media, "DVD"))
        cfg.media = 0x14;

    /* fh_cfg = open(cfg_path, O_WRONLY | O_CREAT | O_APPEND);
    if (fh_cfg < 0)
        return -1;

    r = write(fh_cfg, &cfg, sizeof(cfg_t));
    if (r != sizeof(cfg_t)) {
        close(fh_cfg);
        return -2;
    }

    close(fh_cfg); */

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
int write_parts(const char *drive, const char *game_name, const char *game_id, s64 filesize, int parts)
{
    // int fh_part;
    FILE *fh_part;
    char part_path[256];
    int i, r;
    u8 *buf;
    u32 size;
    s64 nbytes, rpos, iso_pos;

#ifdef DEBUG
    printf("write_parts drive:%s name:%s id:%s filesize:0x%llx parts:%d\n", drive, game_name, game_id, filesize, parts);
#endif

    iso_pos = 0;
    buf = malloc(WR_SIZE + 2048);
    if (!buf)
        return -1;

    for (i = 0; i < parts; i++) {
#ifdef _WIN32
        if (strlen(drive) == 1)
            sprintf(part_path, "%s:\\ul.%08X.%s.%02d", drive, crc32(game_name), game_id, i);
        else
            sprintf(part_path, "%s\\ul.%08X.%s.%02d", drive, crc32(game_name), game_id, i);
#else
        sprintf(part_path, "%s/ul.%08X.%s.%02d", drive, crc32(game_name), game_id, i);
#endif
        /*
        fh_part = open(part_path, O_WRONLY | O_TRUNC | O_CREAT);
        if (fh_part < 0) {
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
                    close(fh_part);
                    return -3;
                }

                printf("Writing %d sectors to %s - LBA: %d\n", WR_SIZE >> 11, part_path, (int)(iso_pos >> 11));

                // write to file
                r = write(fh_part, buf, size);
                if (r != size) {
                    free(buf);
                    close(fh_part);
                    return -4;
                }

                size = r;
                rpos += size;
                iso_pos += size;
                nbytes -= size;

            } while (nbytes);
        }
        filesize -= rpos;
        close(fh_part);
    }
        */
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

                printf("Writing %d sectors to %s - LBA: %d\n", WR_SIZE >> 11, part_path, (int)(iso_pos >> 11));

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

            while ((*p2 <= ' ') && (*p2 > '\0') && (*p2 != '\r') && (*p2 != '\n'))
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
s64 GetGameID(char *isofile, int isBigEndian, short closeOnEnd, char *GameID)
{
    int ret;
    char ElfPath[256];
    char *p;
    s64 filesize;

    // Init isofs
    filesize = isofs_Init(isofile, isBigEndian);
    if (!filesize) {
        printf("Error: failed to open ISO file: %s\n", isofile);
        return 0;
    }

    // parse system.cnf in ISO file
    ret = ParseSYSTEMCNF("\\SYSTEM.CNF;1", ElfPath);
    if (ret < 0) {
        switch (ret) {
            case -1:
                printf("Error: can't open SYSTEM.CNF from ISO file: %s\n", isofile);
                break;
            case -2:
                printf("Error: failed to read SYSTEM.CNF from ISO file: %s\n", isofile);
                break;
            case -3:
                printf("Error: failed to parse SYSTEM.CNF from ISO file: %s\n", isofile);
                break;
            case -4:
                printf("Error: failed to locate elf path from ISO file: %s\n", isofile);
                break;
        }
        return 0;
    }

#ifdef DEBUG
    printf("Elf Path: %s\n", ElfPath);
#endif

    // get GameID
    strcpy(GameID, &ElfPath[8]);
    p = strstr(GameID, ";1");
    *p = 0;

#ifdef DEBUG
    printf("Game ID: %s\n", GameID);
#endif

    if (closeOnEnd)
        isofs_Reset();

    return filesize;
}

//----------------------------------------------------------------
int compute_name(const char *drive, const char *game_name, const char *game_id)
{
#ifdef DEBUG
    printf("rename_iso drive:%s name:%s id:%s\n", drive, game_name, game_id);
#endif

#ifdef _WIN32
    if (strlen(drive) == 1)
        printf("%s:\\ul.%08X.%s.00\n", drive, crc32(game_name), game_id);
    else
        printf("%s\\ul.%08X.%s.00\n", drive, crc32(game_name), game_id);
#else
    printf("%s/ul.%08X.%s.00\n", drive, crc32(game_name), game_id);
#endif
    return 0;
}

//----------------------------------------------------------------
void scan_dir(int isBigEndian)
{
    struct dirent *ent;
    int size;
    char GameID[256];
    char *name;
    char fullname[512];
    char newname[512];
    struct stat buf;

    DIR *rep = opendir(".");
    if (rep != NULL) {
        while ((ent = readdir(rep)) != NULL) {
            name = ent->d_name;
            size = strlen(name);
            sprintf(fullname, "./%s", name);
            if (!stat(fullname, &buf) && !S_ISDIR(buf.st_mode)) {
                if (strstr(name, ".iso")) {
                    if ((size >= 17) && (name[4] == '_') && (name[8] == '.') && (name[11] == '.')) {
                        printf("%s seems to be correctly named\n", fullname);
                    } else if (GetGameID(fullname, isBigEndian, 1, GameID)) {
                        sprintf(newname, "./%s.%s", GameID, name);
                        if (rename(fullname, newname) == 0) {
                            printf("%s renamed to: %s\n", fullname, newname);
                        }
                    }
                }
            }
        }
        closedir(rep);
    }
}

//-----------------------------------------------------------------------
int main(int argc, char **argv, char **env)
{
    int ret;
    char GameID[256];
    s64 num_parts;
    char *p;
    s64 filesize;
    int isBigEnd;

    // Big Endianness test
    p = (char *)&isBigEnd;
    memset(p, 0, 4);
    p[3] = 1;

    if (isBigEnd != 1)
        isBigEnd = 0;

#ifdef DEBUG
    printf("DEBUG_MODE ON\n");
    printf("Endianness: %d\n", isBigEnd);
    printf("isofs Init...\n");
#endif

    // args check
    if ((argc > 1) && (strcmp(argv[1], "SCAN") == 0)) {
        scan_dir(isBigEnd);
        exit(EXIT_SUCCESS);
    }

    if ((argc < 5) || (strcmp(argv[4], "CD") && strcmp(argv[4], "DVD")) || (strlen(argv[3]) > 32)) {
        printUsage();
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf("DEBUG_MODE ON\n");
#endif

    filesize = GetGameID(argv[1], isBigEnd, 0, GameID);
    if (filesize == 0) {
        isofs_Reset();
        exit(EXIT_FAILURE);
    }

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

    // get needed number of parts
    num_parts = filesize / 1073741824;
    if (filesize & 0x3fffffff)
        num_parts++;

#ifdef DEBUG
    printf("ISO filesize: 0x%llx\n", filesize);
    printf("Number of parts: %lld\n", num_parts);
// return 0;
#endif

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
