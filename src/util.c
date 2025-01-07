/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/util.h"
#include "include/ioman.h"
#include "include/system.h"
#include <rom0_info.h>
#include "include/hdd.h"

#include "../modules/isofs/zso.h"

extern int probed_fd;
extern u32 probed_lba;


void guiWarning(const char *text, int count);



// a simple maximum of two
int max(int a, int b)
{
    return a > b ? a : b;
}

// a simple minimum of two
int min(int a, int b)
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
        _io_driver driver = {open, close, (int (*)(int, void *, int))read, O_RDONLY};
        GetRomNameWithIODriver(romver, &driver);

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
            default: // For Japanese and unidentified consoles.
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
    memset(logo, 0, sizeof(logo));
    if ((fd > 0) && (lba == 0)) { // BDM_MODE & ETH_MODE
        lseek(fd, 0, SEEK_SET);
        w = read(fd, logo, sizeof(logo)) == sizeof(logo);
    }
    if ((lba > 0) && (fd == 0)) {       // HDD_MODE
        for (k = 0; k <= 12 * 4; k++) { // NB: Disc sector size (2048 bytes) and HDD sector size (512 bytes) differ, hence why we multiplied the number of sectors (12) by 4.
            w = !(hddReadSectors(lba + k, 1, buffer));
            if (!w)
                break;
            buffer += 512;
        }
    }

    if (*(u32 *)logo == ZSO_MAGIC) {
        // initialize ZSO
        ziso_init((ZISO_header *)logo, *(u32 *)((u8 *)logo + sizeof(ZISO_header)));
        probed_fd = fd;
        probed_lba = lba;
        // read ZISO data
        w = (ziso_read_sector(logo, 0, 12) == 12);
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
            if (gEnableDebug)
                guiWarning(text, 12);
        } else {
            if (gEnableDebug)
                guiWarning("Not a valid PS2 Logo first byte!", 12);
        }
    } else {
        if (gEnableDebug)
            guiWarning("Error reading first 12 disc sectors (PS2 Logo)!", 25);
    }
    return ValidPS2Logo;
}


/*----------------------------------------------------------------------------------------*/
/* NOP delay.                                                                             */
/*----------------------------------------------------------------------------------------*/
void delay(int count)
{
    int i, ret;

    for (i = 0; i < count; i++) {
        ret = 0x01000000;
        while (ret--)
            asm("nop\nnop\nnop\nnop");
    }
}