#ifndef __UTIL_H
#define __UTIL_H

int max(int a, int b);
int min(int a, int b);
int fromHex(char digit);
char toHex(int digit);

enum CONSOLE_REGIONS {
    CONSOLE_REGION_INVALID = -1,
    CONSOLE_REGION_JAPAN = 0,
    CONSOLE_REGION_USA, // USA and HK/SG.
    CONSOLE_REGION_EUROPE,
    CONSOLE_REGION_CHINA,

    CONSOLE_REGION_COUNT
};

int InitConsoleRegionData(void);
const char *GetSystemDataPath(void);
char GetSystemFolderLetter(void);

int CheckPS2Logo(int fd, u32 lba);

void delay(int count);

#endif
