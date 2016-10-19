#include <errno.h>
#include <kernel.h>
#include <libcdvd.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "include/util.h"
#include "include/OSDHistory.h"

#define SONY_SYSDATA_ICON_SYS_SIZE 1776 //The OSDs have this weird bug (or a feature, whichever one you prefer) whereby the size of the icon file is hardcoded to 1776 bytes... even though that is way too long! Unfortunately, using the right size will cause the icon to be deemed as corrupted data by the HDDOSD. :/

extern unsigned char icon_sys_A[];
extern unsigned char icon_sys_J[];

#define DEBUG_PRINTF(args...)

int CreateSystemDataFolder(const char *path, char FolderRegionLetter)
{
    char fullpath[64];
    int fd, result, size;
    void *icon;

    sprintf(fullpath, "%s/icon.sys", path);
    if ((fd = open(fullpath, O_RDONLY)) < 0) {
        mkdir(path);
        if ((fd = open(fullpath, O_CREAT | O_TRUNC | O_WRONLY)) >= 0) {
            if (FolderRegionLetter == 'I') {
                icon = icon_sys_J;
                size = SONY_SYSDATA_ICON_SYS_SIZE;
            } else {
                icon = icon_sys_A;
                size = SONY_SYSDATA_ICON_SYS_SIZE;
            }
            result = write(fd, icon, size) == size ? 0 : -EIO; //Yes, just let it write past the end of the icon. Read the comment above.
            close(fd);
        } else
            result = fd;
    } else {
        close(fd);
        result = 0;
    }

    return result;
}

int LoadHistoryFile(const char *path, struct HistoryEntry *HistoryEntries)
{
    char fullpath[64];
    int fd, result;

    sprintf(fullpath, "%s/history", path);
    if ((fd = open(fullpath, O_RDONLY)) >= 0) {
        result = read(fd, HistoryEntries, MAX_HISTORY_ENTRIES * sizeof(struct HistoryEntry)) == (MAX_HISTORY_ENTRIES * sizeof(struct HistoryEntry)) ? 0 : -EIO;
        close(fd);
    } else
        result = fd;

    return result;
}

int SaveHistoryFile(const char *path, const struct HistoryEntry *HistoryEntries)
{
    char fullpath[64];
    int fd, result;

    sprintf(fullpath, "%s/history", path);
    if ((fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC)) >= 0) {
        result = write(fd, HistoryEntries, MAX_HISTORY_ENTRIES * sizeof(struct HistoryEntry)) == (MAX_HISTORY_ENTRIES * sizeof(struct HistoryEntry)) ? 0 : -EIO;
        close(fd);
    } else
        result = fd;

    return result;
}

int AddOldHistoryFileRecord(const char *path, const struct HistoryEntry *OldHistoryEntry)
{
    char fullpath[64];
    int fd, result;

    sprintf(fullpath, "%s/history.old", path);
    if ((fd = open(fullpath, O_WRONLY | O_APPEND)) >= 0) {
        lseek(fd, 0, SEEK_END);
        result = write(fd, OldHistoryEntry, sizeof(struct HistoryEntry)) == sizeof(struct HistoryEntry) ? 0 : -EIO;
        close(fd);
    } else
        result = fd;

    return result;
}

int AddHistoryRecord(const char *name)
{
    struct HistoryEntry HistoryEntries[MAX_HISTORY_ENTRIES], *NewEntry, OldHistoryEntry;
    int i, value, LeastUsedRecord, LeastUsedRecordLaunchCount, LeastUsedRecordTimestamp, result;
    unsigned char BlankSlotList[MAX_HISTORY_ENTRIES], NumBlankSlots, IsNewRecord;
    sceCdCLOCK time;
    char SystemRegionLetter;
    char path[32];

    DEBUG_PRINTF("Adding history record: %s\n", name);

    sprintf(path, "mc0:/%s", GetSystemDataPath());
    if ((result = LoadHistoryFile(path, HistoryEntries)) != 0) {
        path[2] = '1';
        if ((result = LoadHistoryFile(path, HistoryEntries)) != 0) {
            DEBUG_PRINTF("Error: can't load history file.\n");

            SystemRegionLetter = GetSystemFolderLetter();

            path[2] = '0';
            if ((result = CreateSystemDataFolder(path, SystemRegionLetter)) != 0) {
                path[2] = '1';
                if ((result = CreateSystemDataFolder(path, SystemRegionLetter)) != 0) {
                    DEBUG_PRINTF("Error: Can't create system data folder: %d\n", result);
                    return result;
                }
            }

            memset(HistoryEntries, 0, sizeof(HistoryEntries));
        }
    }

    for (i = 0, LeastUsedRecord = 0, LeastUsedRecordTimestamp = LeastUsedRecordLaunchCount = INT_MAX, IsNewRecord = 1; i < MAX_HISTORY_ENTRIES; i++) {
        if (HistoryEntries[i].LaunchCount < LeastUsedRecordLaunchCount) {
            LeastUsedRecord = i;
            LeastUsedRecordLaunchCount = HistoryEntries[i].LaunchCount;
        }
        if (LeastUsedRecord == i) {
            if (HistoryEntries[i].DateStamp < LeastUsedRecordTimestamp) {
                LeastUsedRecordTimestamp = HistoryEntries[i].DateStamp;
                LeastUsedRecord = i;
            }
        }

        if (!strcmp(HistoryEntries[i].name, name)) {
            IsNewRecord = 0;

            if ((HistoryEntries[i].bitmask & 0x3F) != 0x3F) {
                HistoryEntries[i].LaunchCount++;
                if (HistoryEntries[i].LaunchCount >= 0x80) {
                    HistoryEntries[i].LaunchCount = 0x7F;
                }

                if (HistoryEntries[i].LaunchCount >= 14) {
                    if ((HistoryEntries[i].LaunchCount - 14) % 10) {
                        while ((HistoryEntries[i].bitmask >> (value = rand() % 6)) & 1) {
                        };
                        HistoryEntries[i].ShiftAmount = value;
                        HistoryEntries[i].bitmask |= 1 << value;
                    }
                }
            } else {
                if (HistoryEntries[i].LaunchCount < 0x40) {
                    HistoryEntries[i].LaunchCount++;
                } else {
                    HistoryEntries[i].LaunchCount = HistoryEntries[i].bitmask & 0x3F;
                    HistoryEntries[i].ShiftAmount = 7;
                }
            }
        }
    }

    if (IsNewRecord) {
        //Count and consolidate a list of blank slots.
        for (i = 0, NumBlankSlots = 0; i < MAX_HISTORY_ENTRIES; i++) {
            if (HistoryEntries[i].LaunchCount == 0) {
                BlankSlotList[NumBlankSlots] = i;
                NumBlankSlots++;
            }
        }

        if (NumBlankSlots > 0) {
            NewEntry = &HistoryEntries[result = BlankSlotList[rand() % NumBlankSlots]];
        } else {
            NewEntry = &HistoryEntries[LeastUsedRecord];
            memcpy(&OldHistoryEntry, NewEntry, sizeof(OldHistoryEntry));

            AddOldHistoryFileRecord(path, &OldHistoryEntry);
        }

        //Initialize the new entry.
        strncpy(NewEntry->name, name, sizeof(NewEntry->name));
        NewEntry->LaunchCount = 1;
        NewEntry->bitmask = 1;
        NewEntry->ShiftAmount = 0;
        sceCdReadClock(&time);
        NewEntry->DateStamp = OSD_HISTORY_SET_DATE(btoi(time.year), btoi(time.month & 0x7F), btoi(time.day));
    }

    return SaveHistoryFile(path, HistoryEntries);
}

static void GetBootFilename(const char *bootpath, char *filename)
{ //Does OPL have a function that strips out the filename from the full path to the file on the CD/DVD?
    int i, length;

    filename[0] = '\0';
    for (i = length = strlen(bootpath); i > 0; i--) {
        if (bootpath[i] == '\\' || bootpath[i] == ':' || bootpath[i] == '/') {
            if (length > 2 && bootpath[length - 1] == '1' && bootpath[length - 2] == ';')
                length -= 2;
            length -= (i + 1);
            strncpy(filename, &bootpath[i + 1], length);
            filename[length] = '\0';
            break;
        }
    }

    if (i == 0) { //The boot path contains only the filename.
        strcpy(filename, bootpath);
    }
}

int AddHistoryRecordUsingFullPath(const char *path)
{
    char filename[17];

    GetBootFilename(path, filename);
    return AddHistoryRecord(filename);
}
