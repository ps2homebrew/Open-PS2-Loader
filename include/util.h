#ifndef __UTIL_H
#define __UTIL_H

#include <gsToolkit.h>

int getFileSize(int fd);
int openFile(char *path, int mode);
void *readFile(char *path, int align, int *size);
int listDir(char *path, const char *separator, int maxElem,
            int (*readEntry)(int index, const char *path, const char *separator, const char *name, unsigned int mode));

typedef struct
{
    int fd;
    int mode;
    char *buffer;
    unsigned int size;
    unsigned int available;
    char *lastPtr;
    short allocResult;
} file_buffer_t;

file_buffer_t *openFileBufferBuffer(short allocResult, const void *buffer, unsigned int size);
file_buffer_t *openFileBuffer(char *fpath, int mode, short allocResult, unsigned int size);
int readFileBuffer(file_buffer_t *readContext, char **outBuf);
void writeFileBuffer(file_buffer_t *fileBuffer, char *inBuf, int size);
void closeFileBuffer(file_buffer_t *fileBuffer);

inline int max(int a, int b);
inline int min(int a, int b);
int fromHex(char digit);
char toHex(int digit);

enum CONSOLE_REGIONS {
    CONSOLE_REGION_INVALID = -1,
    CONSOLE_REGION_JAPAN = 0,
    CONSOLE_REGION_USA, //USA and HK/SG.
    CONSOLE_REGION_EUROPE,
    CONSOLE_REGION_CHINA,

    CONSOLE_REGION_COUNT
};

int InitConsoleRegionData(void);
const char *GetSystemDataPath(void);
char GetSystemFolderLetter(void);
int sysDeleteFolder(const char *folder);

int CheckPS2Logo(int fd, u32 lba);

inline void delay(int count);

#endif
