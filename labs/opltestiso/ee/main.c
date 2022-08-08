// ps2sdk include files
#include <debug.h>
#include <loadfile.h>
#include <smem.h>
#include <smod.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <sbv_patches.h>
#include <libcdvd-common.h>

// posix (newlib) include files
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>

// This function is defined as weak in ps2sdkc, so how
// we are not using time zone, so we can safe some KB
void _ps2sdk_timezone_update() {}

DISABLE_PATCHED_FUNCTIONS(); // Disable the patched functionalities
// DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers

//#define PRINTF printf
#define PRINTF scr_printf

// Blocks sizes to test
#define FILE_SIZE             (5 * 1024 * 1024)
#define FILE_BLOCK_SIZE_MIN   (2 * 1024)
#define FILE_BLOCK_SIZE_MAX   (256 * 1024)
#define STREAM_BLOCK_SIZE_MIN (2 * 1024)
#define STREAM_BLOCK_SIZE_MAX (32 * 1024)
#define STREAM_BUFMAX         80 // 80 sectors = 160KiB
#define STREAM_BANKMAX        5  // max 5 ringbuffers inside buffer ?

#define FILE_RANDOM "cdrom:\\RANDOM.BIN"
#define FILE_ZERO   "cdrom:\\ZERO.BIN"

#define IRX_DEFINE(mod)               \
    extern unsigned char mod##_irx[]; \
    extern unsigned int size_##mod##_irx

IRX_DEFINE(freeram);

//--------------------------------------------------------------
void print_speed(clock_t clk_start, clock_t clk_end, u32 fd_size, u32 buf_size)
{
    unsigned int msec = (int)((clk_end - clk_start) / (CLOCKS_PER_SEC / 1000));
    PRINTF("\t\t- Read %04dKiB in %04dms, blocksize=%06d, speed=%04dKB/s\n", fd_size / 1024, msec, buf_size, fd_size / msec);
}

//--------------------------------------------------------------
void test_read_file_1(const char *filename, unsigned int block_size, unsigned int total_size)
{
    int size_left;
    int fd;
    char *buffer = NULL;
    clock_t clk_start, clk_end;

    if ((fd = open(filename, O_RDONLY /*, 0644*/)) <= 0) {
        PRINTF("\t\t- Could not find '%s'\n", filename);
        return;
    }

    buffer = malloc(block_size);

    clk_start = clock();
    size_left = total_size;
    while (size_left > 0) {
        int read_size = (size_left > block_size) ? block_size : size_left;
        if (read(fd, buffer, read_size) != read_size) {
            PRINTF("\t\t- Failed to read file.\n");
            return;
        }
        size_left -= read_size;
    }
    clk_end = clock();

    print_speed(clk_start, clk_end, total_size - size_left, block_size);

    free(buffer);

    close(fd);
}

//--------------------------------------------------------------
void test_read_file(const char *filename)
{
    unsigned int block_size;

    PRINTF("\t\tStart reading file %s file mode:\n", filename);
    for (block_size = FILE_BLOCK_SIZE_MIN; block_size <= FILE_BLOCK_SIZE_MAX; block_size *= 2)
        test_read_file_1(filename, block_size, FILE_SIZE);
}

//--------------------------------------------------------------
void test_read_stream_1(const char *filename, unsigned int block_size, unsigned int total_size)
{
    void *iopbuffer = SifAllocIopHeap(STREAM_BUFMAX * 2048);
    void *eebuffer = malloc(block_size);
    unsigned int sectors = block_size / 2048;
    unsigned int size_left = total_size;

    clock_t clk_start, clk_end;
    sceCdRMode mode = {1, SCECdSpinStm, SCECdSecS2048, 0};
    sceCdlFILE fp;
    u32 error;

    sceCdStInit(STREAM_BUFMAX, STREAM_BANKMAX, iopbuffer);
    sceCdSearchFile(&fp, filename);

    clk_start = clock();
    sceCdStStart(fp.lsn, &mode);
    while (size_left) {
        int rv = sceCdStRead(sectors, eebuffer, STMBLK, &error);
        if (rv != sectors) {
            PRINTF("\t\t- sceCdStRead = %d error = %d\n", rv, error);
            //    break;
        }
        if (error != SCECdErNO) {
            PRINTF("\t\t- ERROR %d\n", error);
            break;
        }
        size_left -= rv * 2048;
    }
    sceCdStStop();
    clk_end = clock();

    print_speed(clk_start, clk_end, total_size - size_left, block_size);

    free(eebuffer);
    SifFreeIopHeap(iopbuffer);
}

//--------------------------------------------------------------
void test_read_stream(const char *filename)
{
    unsigned int block_size;

    PRINTF("\t\tStart reading file %s streaming mode:\n", filename);
    for (block_size = STREAM_BLOCK_SIZE_MIN; block_size <= STREAM_BLOCK_SIZE_MAX; block_size *= 2)
        test_read_stream_1(filename, block_size, FILE_SIZE);
}

//--------------------------------------------------------------
void test_free_iop_ram()
{
    s32 freeram = 0;
    s32 usedram = 0;

    // Check free IOP RAM
    if (SifExecModuleBuffer(freeram_irx, size_freeram_irx, 0, NULL, NULL) < 0)
        PRINTF("\t\tcould not load %s\n", "freeram.irx");

    smem_read((void *)(1 * 1024 * 1024), &freeram, 4);
    usedram = (2 * 1024 * 1024) - freeram;
    PRINTF("\t\tFree IOP RAM: %4dKiB (%7d bytes)\n", freeram / 1024, freeram);
    PRINTF("\t\tUsed IOP RAM: %4dKiB (%7d bytes)\n", usedram / 1024, usedram);
}

//--------------------------------------------------------------
void print_header()
{
    PRINTF("\n\n\n");
    PRINTF("\t\tOPL tester v0.5\n");
}

//--------------------------------------------------------------
#define NDW -20
#define HDW -9
void print_iop_modules()
{
    smod_mod_info_t info;
    smod_mod_info_t *curr = NULL;
    char sName[21];
    int iPage = 1;
    int iModsPerPage = 0;
    int rv = -1;
    int i;
    u32 txtsz_total = 0;
    u32 dtasz_total = 0;
    u32 bsssz_total = 0;

    while (rv != 0) {
        scr_clear();
        print_header();
        PRINTF("\t\tLoaded Modules page %d:\n", iPage);
        PRINTF("\t\t%*s   %*s %*s %*s %*s\n", NDW, "name", HDW, "txtst", HDW, "txtsz", HDW, "dtasz", HDW, "bsssz");
        PRINTF("\t\t-----------------------------------------------------------\n");
        while ((rv = smod_get_next_mod(curr, &info)) != 0) {
            smem_read(info.name, sName, 20);
            sName[20] = 0;
            txtsz_total += info.text_size;
            dtasz_total += info.data_size;
            bsssz_total += info.bss_size;

            PRINTF("\t\t%*s | 0x%5x | 0x%5x | 0x%5x | 0x%5x\n", NDW, sName, info.text_start, info.text_size, info.data_size, info.bss_size);
            curr = &info;

            iModsPerPage++;
            if (iModsPerPage >= 16) {
                iModsPerPage = 0;
                iPage++;

                PRINTF("\t\tNext page in ");
                for (i = 3; i > 0; i--) {
                    PRINTF("%d ", i);
                    sleep(1);
                }

                break;
            }
        }
    }

    u32 total = txtsz_total + dtasz_total + bsssz_total;
    PRINTF("\t\t-----------------------------------------------------------\n");
    PRINTF("\t\t%*s |         | 0x%5x | 0x%5x | 0x%5x\n", NDW, "subtotal:", txtsz_total, dtasz_total, bsssz_total);
    PRINTF("\t\t%*s | 0x%5x (%dKiB)\n", NDW, "total:", total, total / 1024);
    PRINTF("\t\t-----------------------------------------------------------\n");
    test_free_iop_ram();
}

//--------------------------------------------------------------
void print_done()
{
    int i;

    PRINTF("\t\tDone. Next test in ");
    for (i = 3; i > 0; i--) {
        PRINTF("%d ", i);
        sleep(1);
    }
}

//--------------------------------------------------------------
int main()
{
    init_scr();

    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();

    SifInitRpc(0);
    // while (!SifIopReset("rom0:UDNL cdrom:\\IOPRP300.IMG", 0))
    while (!SifIopReset("", 0))
        ;
    while (!SifIopSync())
        ;
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();

    // Enable loading iop modules from EE memory
    sbv_patch_enable_lmb();

    // Load cdvdman
    // NOTE: on OPL this module will not be loaded
    if (SifLoadModule("rom0:CDVDMAN", 0, 0) < 0)
        PRINTF("\t\tcould not load %s\n", "rom0:CDVDMAN");

    // Load cdvdfsv
    // NOTE: on OPL this module will not be loaded
    if (SifLoadModule("rom0:CDVDFSV", 0, 0) < 0)
        PRINTF("\t\tcould not load %s\n", "rom0:CDVDFSV");

    sceCdInit(SCECdINIT);
    sceCdMmode(SCECdPS2DVD);

    while (1) {
        // speed test random file
        scr_clear();
        print_header();
        test_read_file(FILE_RANDOM);
        test_read_stream(FILE_RANDOM);
        test_free_iop_ram();
        print_done();

        // speed test zero file
        scr_clear();
        print_header();
        test_read_file(FILE_ZERO);
        test_read_stream(FILE_ZERO);
        test_free_iop_ram();
        print_done();

        // display modules and sizes
        print_iop_modules();
        print_done();
    }

    return 0;
}
