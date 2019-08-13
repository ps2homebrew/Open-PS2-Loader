/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/util.h"
#include "include/ioman.h"
#include <io_common.h>
#include <string.h>
#include <malloc.h>
#include <fileXio_rpc.h>
#include <osd_config.h>

#include "include/hdd.h"

extern void *icon_sys;
extern int size_icon_sys;
extern void *icon_icn;
extern int size_icon_icn;

static int mcID = -1;

void guiWarning(const char *text, int count);

int getFileSize(int fd)
{
    int size = fileXioLseek(fd, 0, SEEK_END);
    fileXioLseek(fd, 0, SEEK_SET);
    return size;
}

static void writeMCIcon(void)
{
    int fd;

    fd = openFile("mc?:OPL/opl.icn", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        fileXioWrite(fd, &icon_icn, size_icon_icn);
        fileXioClose(fd);
    }

    fd = openFile("mc?:OPL/icon.sys", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        fileXioWrite(fd, &icon_sys, size_icon_sys);
        fileXioClose(fd);
    }
}

static int checkMC()
{
    int dummy, ret;

    if (mcID == -1) {
        mcGetInfo(0, 0, &dummy, &dummy, &dummy);
        mcSync(0, NULL, &ret);

        int fd = fileXioDopen("mc0:OPL");
        if (fd < 0) {
            fd = fileXioDopen("mc1:OPL");
            if (fd < 0) {
                // No base dir found on any MC, will create the folder
                if (fileXioMkdir("mc0:OPL", 0777) >= 0) {
                    mcID = 0x30;
                    writeMCIcon();
                } else if (fileXioMkdir("mc1:OPL", 0777) >= 0) {
                    mcID = 0x31;
                    writeMCIcon();
                }
            } else {
                fileXioDclose(fd);
                mcID = 0x31;
            }
        } else {
            fileXioDclose(fd);
            mcID = 0x30;
        }
    }
    return mcID;
}

static int checkFile(char *path, int mode)
{
    // check if it is mc
    if (strncmp(path, "mc", 2) == 0) {

        // if user didn't explicitly asked for a MC (using '?' char)
        if (path[2] == 0x3F) {

            // Use default detected card
            if (checkMC() >= 0)
                path[2] = mcID;
            else
                return 0;
        }

        // in create mode, we check that the directory exist, or create it
        if (mode & O_CREAT) {
            char dirPath[256];
            char *pos = strrchr(path, '/');
            if (pos) {
                memcpy(dirPath, path, pos - path);
                dirPath[pos - path] = '\0';
                int fd = fileXioDopen(dirPath);
                if (fd < 0) {
                    if (fileXioMkdir(dirPath, 0777) < 0)
                        return 0;
                } else
                    fileXioDclose(fd);
            }
        }
    }
    return 1;
}

int openFile(char *path, int mode)
{
    if (checkFile(path, mode))
        return fileXioOpen(path, mode, 0666);
    else
        return -1;
}

void *readFile(char *path, int align, int *size)
{
    void *buffer = NULL;

    int fd = openFile(path, O_RDONLY);
    if (fd >= 0) {
        unsigned int realSize = getFileSize(fd);

        if ((*size > 0) && (*size != realSize)) {
            LOG("UTIL Invalid filesize, expected: %d, got: %d\n", *size, realSize);
            fileXioClose(fd);
            return NULL;
        }

        if (align > 0)
            buffer = memalign(64, realSize); // The allocation is aligned to aid the DMA transfers
        else
            buffer = malloc(realSize);

        if (!buffer) {
            LOG("UTIL ReadFile: Failed allocation of %d bytes", realSize);
            *size = 0;
        } else {
            fileXioRead(fd, buffer, realSize);
            fileXioClose(fd);
            *size = realSize;
        }
    }
    return buffer;
}

int listDir(char *path, const char *separator, int maxElem,
            int (*readEntry)(int index, const char *path, const char *separator, const char *name, unsigned int mode))
{
    int fdDir, index = 0;
    if (checkFile(path, O_RDONLY)) {
        iox_dirent_t record;

        fdDir = fileXioDopen(path);
        if (fdDir > 0) {
            while (index < maxElem && fileXioDread(fdDir, &record) > 0)
                index = readEntry(index, path, separator, record.name, record.stat.mode);

            fileXioDclose(fdDir);
        }
    }
    return index;
}

/* size will be the maximum line size possible */
file_buffer_t *openFileBuffer(char *fpath, int mode, short allocResult, unsigned int size)
{
    file_buffer_t *fileBuffer = NULL;
    unsigned char bom[3];

    int fd = openFile(fpath, mode);
    if (fd >= 0) {
        fileBuffer = (file_buffer_t *)malloc(sizeof(file_buffer_t));
        fileBuffer->size = size;
        fileBuffer->available = 0;
        fileBuffer->buffer = (char *)malloc(size * sizeof(char));
        if (mode == O_RDONLY) {
            fileBuffer->lastPtr = NULL;

            //Check for and skip the UTF-8 BOM sequence.
            if ((fileXioRead(fd, bom, sizeof(bom)) != 3) ||
                (bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF)) {
                //Not BOM, so rewind.
                fileXioLseek(fd, 0, SEEK_SET);
            }
        } else
            fileBuffer->lastPtr = fileBuffer->buffer;
        fileBuffer->allocResult = allocResult;
        fileBuffer->fd = fd;
        fileBuffer->mode = mode;
    }

    return fileBuffer;
}

/* size will be the maximum line size possible */
file_buffer_t *openFileBufferBuffer(short allocResult, const void *buffer, unsigned int size)
{
    file_buffer_t *fileBuffer = NULL;

    fileBuffer = (file_buffer_t *)malloc(sizeof(file_buffer_t));
    fileBuffer->size = size;
    fileBuffer->available = size;
    fileBuffer->buffer = (char *)malloc((size + 1) * sizeof(char));
    fileBuffer->lastPtr = fileBuffer->buffer; //O_RDONLY, but with the data in the buffer.
    fileBuffer->allocResult = allocResult;
    fileBuffer->fd = -1;
    fileBuffer->mode = O_RDONLY;

    memcpy(fileBuffer->buffer, buffer, size);
    fileBuffer->buffer[size] = '\0';

    return fileBuffer;
}

int readFileBuffer(file_buffer_t *fileBuffer, char **outBuf)
{
    int lineSize = 0, read, length;
    char *posLF = NULL;

    while (1) {
        // if lastPtr is set, then we continue the read from this point as reference
        if (fileBuffer->lastPtr) {
            // Calculate the remaining chars to the right of lastPtr
            lineSize = fileBuffer->available - (fileBuffer->lastPtr - fileBuffer->buffer);
            /*LOG("##### Continue read, position: %X (total: %d) line size (\\0 not inc.): %d end: %x\n",
					fileBuffer->lastPtr - fileBuffer->buffer, fileBuffer->available, lineSize, fileBuffer->lastPtr[lineSize]);*/
            posLF = strchr(fileBuffer->lastPtr, '\n');
        }

        if (!posLF) { // We can come here either when the buffer is empty, or if the remaining chars don't have a LF

            // if available, we shift the remaining chars to the left ...
            if (lineSize) {
                //LOG("##### LF not found, Shift %d characters from end to beginning\n", lineSize);
                memmove(fileBuffer->buffer, fileBuffer->lastPtr, lineSize);
            }

            // ... and complete the buffer if we're not at EOF
            if (fileBuffer->fd >= 0) {

                // Load as many characters necessary to fill the buffer
                length = fileBuffer->size - lineSize - 1;
                //LOG("##### Asking for %d characters to complete buffer\n", length);
                read = fileXioRead(fileBuffer->fd, fileBuffer->buffer + lineSize, length);
                fileBuffer->buffer[lineSize + read] = '\0';

                // Search again (from the lastly added chars only), the result will be "analyzed" in next if
                posLF = strchr(fileBuffer->buffer + lineSize, '\n');

                // Now update read context info
                lineSize = lineSize + read;
                //LOG("##### %d characters really read, line size now (\\0 not inc.): %d\n", read, lineSize);

                // If buffer not full it means we are at EOF
                if (fileBuffer->size != lineSize + 1) {
                    //LOG("##### Reached EOF\n");
                    fileXioClose(fileBuffer->fd);
                    fileBuffer->fd = -1;
                }
            }

            fileBuffer->lastPtr = fileBuffer->buffer;
            fileBuffer->available = lineSize;
        }

        if (posLF)
            lineSize = posLF - fileBuffer->lastPtr;

        // Check the previous char (on Windows there are CR/LF instead of single linux LF)
        if (lineSize)
            if (*(fileBuffer->lastPtr + lineSize - 1) == '\r')
                lineSize--;

        fileBuffer->lastPtr[lineSize] = '\0';
        *outBuf = fileBuffer->lastPtr;

        //LOG("##### Result line is \"%s\" size: %d avail: %d pos: %d\n", fileBuffer->lastPtr, lineSize, fileBuffer->available, fileBuffer->lastPtr - fileBuffer->buffer);

        // If we are at EOF and no more chars available to scan, then we are finished
        if (!lineSize && !fileBuffer->available && fileBuffer->fd == -1)
            return 0;

        if (fileBuffer->lastPtr[0] == '#') { // '#' for comment lines
            if (posLF)
                fileBuffer->lastPtr = posLF + 1;
            else
                fileBuffer->lastPtr = NULL;
            continue;
        }

        if (lineSize && fileBuffer->allocResult) {
            *outBuf = (char *)malloc((lineSize + 1) * sizeof(char));
            memcpy(*outBuf, fileBuffer->lastPtr, lineSize + 1);
        }

        // Either move the pointer to next chars, or set it to null to force a whole buffer read (if possible)
        if (posLF)
            fileBuffer->lastPtr = posLF + 1;
        else {
            fileBuffer->lastPtr = NULL;
        }

        return 1;
    }
}

void writeFileBuffer(file_buffer_t *fileBuffer, char *inBuf, int size)
{
    //LOG("writeFileBuffer avail: %d size: %d\n", fileBuffer->available, size);
    if (fileBuffer->available && fileBuffer->available + size > fileBuffer->size) {
        //LOG("writeFileBuffer flushing: %d\n", fileBuffer->available);
        fileXioWrite(fileBuffer->fd, fileBuffer->buffer, fileBuffer->available);
        fileBuffer->lastPtr = fileBuffer->buffer;
        fileBuffer->available = 0;
    }

    if (size > fileBuffer->size) {
        //LOG("writeFileBuffer direct write: %d\n", size);
        fileXioWrite(fileBuffer->fd, inBuf, size);
    } else {
        memcpy(fileBuffer->lastPtr, inBuf, size);
        fileBuffer->lastPtr += size;
        fileBuffer->available += size;

        //LOG("writeFileBuffer lastPrt: %d\n", (fileBuffer->lastPtr - fileBuffer->buffer));
    }
}

void closeFileBuffer(file_buffer_t *fileBuffer)
{
    if (fileBuffer->fd >= 0) {
        if (fileBuffer->mode != O_RDONLY && fileBuffer->available) {
            //LOG("writeFileBuffer final write: %d\n", fileBuffer->available);
            fileXioWrite(fileBuffer->fd, fileBuffer->buffer, fileBuffer->available);
        }
        fileXioClose(fileBuffer->fd);
    }
    free(fileBuffer->buffer);
    free(fileBuffer);
    fileBuffer = NULL;
}

// a simple maximum of two inline
inline int max(int a, int b)
{
    return a > b ? a : b;
}

// a simple minimum of two inline
inline int min(int a, int b)
{
    return a < b ? a : b;
}

// single digit from hex decode
int fromHex(char digit)
{
    if ((digit >= '0') && (digit <= '9')) {
        return (digit - '0');
    } else if ((digit >= 'A') && (digit <= 'F')) {
        return (digit - 'A' + 10);
    } else if ((digit >= 'a') && (digit <= 'f')) {
        return (digit - 'a' + 10);
    } else
        return -1;
}

static const char htab[16] = "0123456789ABCDEF";
char toHex(int digit)
{
    return htab[digit & 0x0F];
}

static short int ConsoleRegion = CONSOLE_REGION_INVALID;
static char SystemDataFolderPath[] = "BRDATA-SYSTEM";
static char SystemFolderLetter = 'R';

static void UpdateSystemPaths(void)
{
    char regions[CONSOLE_REGION_COUNT] = {'I', 'A', 'E', 'C'};

    SystemFolderLetter = regions[ConsoleRegion];
    SystemDataFolderPath[1] = SystemFolderLetter;
}

int InitConsoleRegionData(void)
{
    int result;
    char romver[16];

    if ((result = ConsoleRegion) < 0) {
        GetRomName(romver);

        switch (romver[4]) {
            case 'C':
                ConsoleRegion = CONSOLE_REGION_CHINA;
                break;
            case 'E':
                ConsoleRegion = CONSOLE_REGION_EUROPE;
                break;
            case 'H':
            case 'A':
                ConsoleRegion = CONSOLE_REGION_USA;
                break;
            default: //For Japanese and unidentified consoles.
                ConsoleRegion = CONSOLE_REGION_JAPAN;
        }

        result = ConsoleRegion;

        UpdateSystemPaths();
    }

    return result;
}

const char *GetSystemDataPath(void)
{
    return SystemDataFolderPath;
}

char GetSystemFolderLetter(void)
{
    return SystemFolderLetter;
}

int GetSystemRegion(void)
{
    return ConsoleRegion;
}

int CheckPS2Logo(int fd, u32 lba)
{
    u8 logo[12 * 2048] ALIGNED(64);
    void *buffer = logo;
    u32 j, k, w;
    u8 ValidPS2Logo = 0;
    u32 ps2logochecksum = 0;
    char text[1024];

    w = 0;
    if ((fd > 0) && (lba == 0)) // USB_MODE & ETH_MODE
        w = fileXioRead(fd, logo, sizeof(logo)) == sizeof(logo);
    if ((lba > 0) && (fd == 0)) {       // HDD_MODE
        for (k = 0; k <= 12 * 4; k++) { // NB: Disc sector size (2048 bytes) and HDD sector size (512 bytes) differ, hence why we multiplied the number of sectors (12) by 4.
            w = !(hddReadSectors(lba + k, 1, buffer));
            if (!w)
                break;
            buffer += 512;
        }
    }

    if (w) {

        u8 key = logo[0];
        if (logo[0] != 0) {
            for (j = 0; j < (12 * 2048); j++) {
                logo[j] ^= key;
                logo[j] = (logo[j] << 3) | (logo[j] >> 5);
            }
            for (j = 0; j < (12 * 2048); j++) {
                ps2logochecksum += (u32)logo[j];
            }
            // PS2LOGO NTSC Checksum = 0x120519
            // PS2LOGO PAL  Checksum = 0x1555AB
            snprintf(text, sizeof(text), "%s Disc PS2 Logo(checksum 0x%06X) & %s console ", ((ps2logochecksum == 0x1555AB) ? "PAL" : "NTSC"), ps2logochecksum, ((ConsoleRegion == CONSOLE_REGION_EUROPE) ? "PAL" : "NTSC"));
            if (((ps2logochecksum == 0x1555AB) && (ConsoleRegion == CONSOLE_REGION_EUROPE)) || ((ps2logochecksum == 0x120519) && (ConsoleRegion != CONSOLE_REGION_EUROPE))) {
                ValidPS2Logo = 1;
                strcat(text, "match.");
            } else {
                strcat(text, "don't match!");
            }
            if (!gDisableDebug)
                guiWarning(text, 12);
        } else {
            if (!gDisableDebug)
                guiWarning("Not a valid PS2 Logo first byte!", 12);
        }
    } else {
        if (!gDisableDebug)
            guiWarning("Error reading first 12 disc sectors (PS2 Logo)!", 25);
    }
    return ValidPS2Logo;
}

struct DirentToDelete {
    struct DirentToDelete *next;
     char *filename;
};

int sysDeleteFolder(const char *folder)
{
    int fd, result;
    char *path;
    iox_dirent_t dirent;
    struct DirentToDelete *head, *start;

    result = 0;
    start = head = NULL;
    if((fd = fileXioDopen(folder)) >= 0) {
        /* Generate a list of files in the directory. */
        while(fileXioDread(fd, &dirent) > 0) {
            if((strcmp(dirent.name, ".") == 0) || ((strcmp(dirent.name, "..") == 0)))
                continue;

            if(FIO_S_ISDIR(dirent.stat.mode)) {
                if((path = malloc(strlen(folder)+strlen(dirent.name) + 2)) != NULL) {
                    sprintf(path, "%s/%s", folder, dirent.name);
                        result = sysDeleteFolder(path);
                        free(path);
                }
            } else {
                if(start == NULL) {
                    head = malloc(sizeof(struct DirentToDelete));
                    if(head == NULL)
                        break;
                    start = head;
                } else {
                    if((head->next = malloc(sizeof(struct DirentToDelete))) == NULL)
                        break;

                    head=head->next;
                }

                head->next=NULL;

                if((head->filename = malloc(strlen(dirent.name) + 1)) != NULL)
                    strcpy(head->filename, dirent.name);
                else
                   break;
            }
        }

        fileXioDclose(fd);
    } else
        result = fd;

    if (result >= 0) {
        /* Delete the files. */
        for (head = start; head != NULL; head = start) {
            if(head->filename != NULL) {
                if((path = malloc(strlen(folder) + strlen(head->filename) + 2)) != NULL) {
                    sprintf(path, "%s/%s", folder, head->filename);
                    result=fileXioRemove(path);
                    if (result < 0)
                        LOG("sysDeleteFolder: failed to remove %s: %d\n", path, result);

                    free(path);
                }
                free(head->filename);
            }

            start = head->next;
            free(head);
        }

        if(result >= 0) {
            result = fileXioRmdir(folder);
            LOG("sysDeleteFolder: failed to rmdir %s: %d\n", folder, result);
        }
    }

    return result;
}

/*----------------------------------------------------------------------------------------*/
/* NOP delay.                                                                             */
/*----------------------------------------------------------------------------------------*/
inline void delay(int count)
{
    int i, ret;

    for (i = 0; i < count; i++) {
        ret = 0x01000000;
        while (ret--)
            asm("nop\nnop\nnop\nnop");
    }
}
