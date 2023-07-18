/*  Play history management functions
    Note: while the code for updating the data itself was a copy of the original,
    the code that reads/writes the history file was written with general know-how,
    for simplicity.
    The original code used libmc and was also designed to allow the host software (the browser) access the history.
    (the browser's boot animation would vary according to the user's play history)

    However, OPL does not need any of that, so it can be made simpler.    */

#include <errno.h>
#include <kernel.h>
#include <libcdvd.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <libmc.h>
#include <libmc-common.h>

#include "include/util.h"
#include "include/OSDHistory.h"

/*  The OSDs have this weird bug whereby the size of the icon file is hardcoded to 1776 bytes... even though that is way too long!
    Unfortunately, using the right size will cause the icon to be deemed as corrupted data by the HDDOSD. */
#define SONY_SYSDATA_ICON_SYS_SIZE 1776

extern unsigned char icon_sys_A[];
extern unsigned char icon_sys_J[];
extern unsigned char icon_sys_C[];

#define DEBUG_PRINTF(args...)

int CreateSystemDataFolder(const char *path, char FolderRegionLetter)
{
    char fullpath[64];
    int fd, result, size;
    void *icon;

    sprintf(fullpath, "%s/icon.sys", path);
    if ((fd = open(fullpath, O_RDONLY)) < 0) {
        mkdir(path, 0777);
        if ((fd = open(fullpath, O_CREAT | O_TRUNC | O_WRONLY)) >= 0) {
            switch (FolderRegionLetter) {
                case 'I':
                    icon = icon_sys_J;
                    size = SONY_SYSDATA_ICON_SYS_SIZE;
                    break;
                case 'C':
                    icon = icon_sys_C;
                    size = SONY_SYSDATA_ICON_SYS_SIZE;
                    break;
                default: // case 'A':
                    icon = icon_sys_A;
                    size = SONY_SYSDATA_ICON_SYS_SIZE;
                    break;
            }
            result = write(fd, icon, size) == size ? 0 : -EIO; // Yes, just let it write past the end of the icon. Read the comment above.
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

static u16 GetTimestamp(void)
{
    // The original obtained the time and date from globals.
    // return OSD_HISTORY_SET_DATE(currentYear, currentMonth, currentDate);
    sceCdCLOCK time;
    sceCdReadClock(&time);
    return OSD_HISTORY_SET_DATE(btoi(time.year), btoi(time.month & 0x7F), btoi(time.day));
}

int AddHistoryRecord(const char *name)
{
    struct HistoryEntry HistoryEntries[MAX_HISTORY_ENTRIES], *NewEntry, OldHistoryEntry;
    int i, value, LeastUsedRecord, LeastUsedRecordLaunchCount, LeastUsedRecordTimestamp, NewLaunchCount, result, mcType, format;
    u8 BlankSlotList[MAX_HISTORY_ENTRIES];
    int NumBlankSlots, NumSlotsUsed, IsNewRecord;
    char SystemRegionLetter;
    char path[32];
    DEBUG_PRINTF("OSDHistory@%s: starts\n", __func__);

    for (i = 0; i < 2; i++) { // increase number of slots for mt (not sure how multi tap works haven’t looked at docs ie mc0 - mc4 ?)
        mcGetInfo(i, 0, &mcType, NULL, &format);
        mcSync(0, NULL, &result);
        DEBUG_PRINTF("\tslot=%d, mctype=%d, format=%d\n", i, mcType, format);
        if ((mcType == sceMcTypePS2) && (format == MC_FORMATTED))
            break;
    }

    if ((mcType != sceMcTypePS2) || (format != MC_FORMATTED)) { // don't even waste time if there are no PS2 MC's
        DEBUG_PRINTF("\tNo PS2 memory cards detected\n");
        return -1;
    }

    DEBUG_PRINTF("\tAdding history record '%s' for slot %d\n", name, i);


    // For simplicity, create the data folder immediately if the history file does not exist (unlike the original).
    sprintf(path, "mc%d:/%s", i, GetSystemDataPath());
    if ((result = LoadHistoryFile(path, HistoryEntries)) != 0) {
        DEBUG_PRINTF("\tcan't load history file.\n");
        SystemRegionLetter = GetSystemFolderLetter();
        if (SystemRegionLetter == 'R') { // @El_isra: R is the default prefix on OPL and FreeMcBoot code, however, no known ps2 uses this prefix for system folders. So skip creating a folder named like that
            DEBUG_PRINTF("\n\tERROR: SystemRegionLetter is R\n\n");
            return -2;
        }
        if ((result = CreateSystemDataFolder(path, SystemRegionLetter)) != 0) {
            DEBUG_PRINTF("\tERROR: Can't create system data folder: error=%d, regionletter=%c\n", result, SystemRegionLetter);
            return result;
        }
        memset(HistoryEntries, 0, sizeof(HistoryEntries));
    }

    LeastUsedRecord = 0;
    LeastUsedRecordTimestamp = INT_MAX;
    LeastUsedRecordLaunchCount = INT_MAX;
    IsNewRecord = 1;

    for (i = 0; i < MAX_HISTORY_ENTRIES; i++) {
        if (HistoryEntries[i].LaunchCount < LeastUsedRecordLaunchCount) {
            LeastUsedRecord = i;
            LeastUsedRecordLaunchCount = HistoryEntries[i].LaunchCount;
        }
        if (LeastUsedRecordLaunchCount == HistoryEntries[i].LaunchCount) {
            if (HistoryEntries[i].DateStamp < LeastUsedRecordTimestamp) {
                LeastUsedRecordTimestamp = HistoryEntries[i].DateStamp;
                LeastUsedRecord = i;
            }
        }

        // In v1.0x, this was strcmp
        if (!strncmp(HistoryEntries[i].name, name, sizeof(HistoryEntries[i].name))) {
            IsNewRecord = 0;

            HistoryEntries[i].DateStamp = GetTimestamp();
            if ((HistoryEntries[i].bitmask & 0x3F) != 0x3F) {
                NewLaunchCount = HistoryEntries[i].LaunchCount + 1;
                if (NewLaunchCount >= 0x80) {
                    NewLaunchCount = 0x7F;
                }

                if (NewLaunchCount >= 14) {
                    if ((NewLaunchCount - 14) % 10 == 0) {
                        while ((HistoryEntries[i].bitmask >> (value = rand() % 6)) & 1) {
                        };
                        HistoryEntries[i].ShiftAmount = value;
                        HistoryEntries[i].bitmask |= 1 << value;
                    }
                }

                HistoryEntries[i].LaunchCount = NewLaunchCount;
            } else {
                // Was a check against 0x40 in v1.0x
                if (HistoryEntries[i].LaunchCount < 0x3F) {
                    HistoryEntries[i].LaunchCount++;
                } else {
                    HistoryEntries[i].LaunchCount = HistoryEntries[i].bitmask & 0x3F;
                    HistoryEntries[i].ShiftAmount = 7;
                }
            }
        }
    }
    /*
    i = 0;
    do { // Original does this. I guess, it is used to ensure that the next random value is truly random?
        rand();
        i++;
    } while (i < (currentMinute * 60 + currentSecond));
    */
    if (IsNewRecord) {
        // Count and consolidate a list of blank slots.
        NumBlankSlots = 0;
        NumSlotsUsed = 0;
        for (i = 0; i < MAX_HISTORY_ENTRIES; i++) {
            if (HistoryEntries[i].name[0] == '\0') {
                BlankSlotList[NumBlankSlots] = i;
                NumBlankSlots++;
            } else {
                // Not present in v1.0x.
                if (HistoryEntries[i].ShiftAmount == 0x7) {
                    NumSlotsUsed++;
                }
            }
        }

        if (NumSlotsUsed != MAX_HISTORY_ENTRIES) {
            if (NumBlankSlots > 0) {
                // Randomly choose an empty slot.
                NewEntry = &HistoryEntries[result = BlankSlotList[rand() % NumBlankSlots]];
            } else {
                // Copy out the victim record
                NewEntry = &HistoryEntries[LeastUsedRecord];
                memcpy(&OldHistoryEntry, NewEntry, sizeof(OldHistoryEntry));

                // Unlike the original, add the old history record here.
                AddOldHistoryFileRecord(path, &OldHistoryEntry);
            }

            // Initialize the new entry.
            strncpy(NewEntry->name, name, sizeof(NewEntry->name) - 1);
            NewEntry->LaunchCount = 1;
            NewEntry->bitmask = 1;
            NewEntry->ShiftAmount = 0;
            NewEntry->DateStamp = GetTimestamp();
        }
    }

    // Unlike the original, save here.
    return SaveHistoryFile(path, HistoryEntries);
}

static void GetBootFilename(const char *bootpath, char *filename)
{ // Extract filename from the full path to the file on the CD/DVD.
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

    if (i == 0) { // The boot path contains only the filename.
        strcpy(filename, bootpath);
    }
}

int AddHistoryRecordUsingFullPath(const char *path)
{
    char filename[17];

    GetBootFilename(path, filename);
    return AddHistoryRecord(filename);
}
