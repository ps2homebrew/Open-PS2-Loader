#define MAX_HISTORY_ENTRIES 21

/*
    If the record is valid, the launch count will be >0.

    Starts with:
        SLPM_550.52, data: 01 01 00 00 75 1a
    After 2 launches:
        SLPM_550.52, data: 02 01 00 00 75 1a
    After 15 launches:
        SLPM_550.52, data: 0e 11 04 00 75 1a
    Clock advanced 1 day:
        SLPM_550.52, data: 01 01 00 00 76 1a
    Clock advanced 1 year:
        SLPM_550.52, data: 01 01 00 00 75 1c
    Clock set to Jan 1 2013:
        SLPM_550.52, data: 01 01 00 00 21 1a

    // 0x002187e8    - Not sure what this is. rand?
    static unsigned int var_0028b020 = 1; // 0x0028b020
    static int func_002187e8(void)
    {
        var_0028b020 = (var_0028b020 * 0x41c64e6d + 0x3039);
        return (var_0028b020 & 0x7FFFFFFF);
    }

    // 0x002021a0, OSDSYS 1.00, ROM 1.01
    int value, LeastUsedRecord, LeastUsedRecordLaunchCount, LeastUsedRecordTimestamp;

    for (i = 0, LeastUsedRecord = 0, LeastUsedRecordTimestamp = LeastUsedRecordLaunchCount = 0x7FFFFFFF; i < 21; i++) {
        if (entry.LaunchCount < LeastUsedRecordLaunchCount) {
            LeastUsedRecord = i;
            LeastUsedRecordLaunchCount = entry.LaunchCount;
        }
        if (LeastUsedRecord == i) {
            if (entry.timestamp < LeastUsedRecordTimestamp) {
                LeastUsedRecordTimestamp = entry.timestamp;
                LeastUsedRecord = i;
            }
        }

        // 0x0020220c
        if (!strcmp(entry.filename, filename)) {
            if (entry.data[1] & 0x3F != 0x3F) {
                entry.LaunchCount++;
                if (entry.LaunchCount >= 0x80) {
                    entry.LaunchCount = 0x7F;
                }

                if (entry.LaunchCount >= 0xE) {
                    if ((entry.LaunchCount - 14) % 10) {
                            while((entry.data[1]>>(value=func_002187e8()%6)&1){};
                            entry.data[2]=value;
                            entry.data[1]|=1<<value;
                    }
                }
            } else {
                // 0x0020237c
                if (entry.LaunchCount < 0x40) {
                    entry.LaunchCount++;
                } else {
                    entry.LaunchCount = entry.data[1] & 0x3F;
                    entry.data[2] = 7;
                }
            }
        }
    }

// 0x002023bc
// The code here counts the number of blank slot, and uses the least used slot. If the least used slot is not empty, the original content is copied out (To be appended to history.old).
// If there are blank slots, it does this to determine which slot to use:
BlankSlotList[func_002187e8() % BlankSlots]

    Conclusion:
        data[0] = LaunchCount
        data[1] = ?? (A bitmask)
        data[2] = ?? (The last shift amount used for generating the value of data[1])
        data[3] = ?? (Seems to not be initialized)
        data[4-5] = Timestamp in format: YYYYYYYMMMMDDDDD
        Note: Year is since year 2000.
*/

#define OSD_HISTORY_GET_YEAR(datestamp)         ((datestamp) >> 9 & 0x7F)
#define OSD_HISTORY_GET_MONTH(datestamp)        ((datestamp) >> 5 & 0xF)
#define OSD_HISTORY_GET_DATE(datestamp)         ((datestamp)&0x1F)
#define OSD_HISTORY_SET_DATE(year, month, date) (((unsigned short int)(year)) << 9 | ((unsigned short int)(month)&0xF) << 5 | ((date)&0x1F))

struct HistoryEntry
{
    char name[16];
    unsigned char LaunchCount;
    unsigned char bitmask;
    unsigned char ShiftAmount;
    unsigned char padding;
    unsigned short int DateStamp;
};

// Functions
int LoadHistoryFile(const char *path, struct HistoryEntry *HistoryEntries);
int SaveHistoryFile(const char *path, const struct HistoryEntry *HistoryEntries);
int AddOldHistoryFileRecord(const char *path, const struct HistoryEntry *OldHistoryEntry);
int AddHistoryRecord(const char *name);
int AddHistoryRecordUsingFullPath(const char *path);
