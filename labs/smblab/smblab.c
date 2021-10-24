
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <fileio.h>
#include <fileXio_rpc.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sbv_patches.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <debug.h>
#include <sys/time.h>
#include <time.h>
#include <ps2smb.h>

#define DPRINTF(args...) \
    printf(args);        \
    scr_printf(args);

#define IP_ADDR "192.168.0.10"
#define NETMASK "255.255.255.0"
#define GATEWAY "192.168.0.1"

extern void poweroff_irx;
extern int size_poweroff_irx;
extern void ps2dev9_irx;
extern int size_ps2dev9_irx;
extern void smsutils_irx;
extern int size_smsutils_irx;
extern void smstcpip_irx;
extern int size_smstcpip_irx;
extern void smsmap_irx;
extern int size_smsmap_irx;
extern void smbman_irx;
extern int size_smbman_irx;
extern void udptty_irx;
extern int size_udptty_irx;
extern void ioptrap_irx;
extern int size_ioptrap_irx;
extern void ps2link_irx;
extern int size_ps2link_irx;
extern void iomanx_irx;
extern int size_iomanx_irx;
extern void filexio_irx;
extern int size_filexio_irx;

// for IP config
#define IPCONFIG_MAX_LEN 64
static char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
static int g_ipconfig_len;

static ShareEntry_t sharelist[128] __attribute__((aligned(64))); // Keep this aligned for DMA!

typedef struct
{
    u8 unused;
    u8 sec;
    u8 min;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} ps2time_t;

#define isYearLeap(year) (!((year) % 4) && (((year) % 100) || !((year) % 400)))

//--------------------------------------------------------------
// I wanted this to be done on IOP, but unfortunately, the compiler
// can't handle div ops on 64 bit numbers.

static ps2time_t *smbtime2ps2time(s64 smbtime, ps2time_t *ps2time)
{
    const int mdtab[2][12] = {
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31} // leap year
    };
    register u32 dayclock, dayno;
    s64 UnixSystemTime;
    register int year = 1970; // year of the Epoch

    memset((void *)ps2time, 0, sizeof(ps2time_t));

    // we add 5x10^6 to the number before scaling it to seconds and subtracting
    // the constant (seconds between 01/01/1601 and the Epoch: 01/01/1970),
    // so that we round in the same way windows does.
    UnixSystemTime = (s64)(((smbtime + 5000000) / 10000000) - 11644473600);

    dayclock = UnixSystemTime % 86400;
    dayno = UnixSystemTime / 86400;

    ps2time->sec = dayclock % 60;
    ps2time->min = (dayclock % 3600) / 60;
    ps2time->hour = dayclock / 3600;
    while (dayno >= (isYearLeap(year) ? 366 : 365)) {
        dayno -= (isYearLeap(year) ? 366 : 365);
        year++;
    }
    ps2time->year = year;
    ps2time->month = 0;
    while (dayno >= mdtab[isYearLeap(year)][ps2time->month]) {
        dayno -= mdtab[isYearLeap(year)][ps2time->month];
        ps2time->month++;
    }
    ps2time->day = dayno + 1;
    ps2time->month++;

    return (ps2time_t *)ps2time;
}

//--------------------------------------------------------------
void set_ipconfig(void)
{
    memset(g_ipconfig, 0, IPCONFIG_MAX_LEN);
    g_ipconfig_len = 0;

    strncpy(&g_ipconfig[g_ipconfig_len], IP_ADDR, 15);
    g_ipconfig_len += strlen(IP_ADDR) + 1;
    strncpy(&g_ipconfig[g_ipconfig_len], NETMASK, 15);
    g_ipconfig_len += strlen(NETMASK) + 1;
    strncpy(&g_ipconfig[g_ipconfig_len], GATEWAY, 15);
    g_ipconfig_len += strlen(GATEWAY) + 1;
}

//--------------------------------------------------------------
int main(int argc, char *argv[2])
{
    int i, ret;

    init_scr();
    scr_clear();
    DPRINTF("smblab\n");

    SifInitRpc(0);

    DPRINTF("IOP Reset... ");

    while (!SifIopReset("", 0))
        ;
    while (!SifIopSync())
        ;
    ;
    fioExit();
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();
    SifExitCmd();

    SifInitRpc(0);
    FlushCache(0);
    FlushCache(2);

    SifLoadFileInit();
    SifInitIopHeap();

    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    SifLoadModule("rom0:SIO2MAN", 0, 0);
    SifLoadModule("rom0:MCMAN", 0, 0);
    SifLoadModule("rom0:MCSERV", 0, 0);

    DPRINTF("OK\n");

    set_ipconfig();

    DPRINTF("loading iomanX... ");
    SifExecModuleBuffer(&iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading fileXio... ");
    SifExecModuleBuffer(&filexio_irx, size_filexio_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading poweroff... ");
    SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading ps2dev9... ");
    SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading smsutils... ");
    SifExecModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading smstcpip... ");
    SifExecModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading smsmap... ");
    SifExecModuleBuffer(&smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading udptty... ");
    SifExecModuleBuffer(&udptty_irx, size_udptty_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading ioptrap... ");
    SifExecModuleBuffer(&ioptrap_irx, size_ioptrap_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading ps2link... ");
    SifExecModuleBuffer(&ps2link_irx, size_ps2link_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading smbman... ");
    SifExecModuleBuffer(&smbman_irx, size_smbman_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("modules load OK\n");

    fileXioInit();


    // ----------------------------------------------------------------
    // how to get password hashes:
    // ----------------------------------------------------------------

    smbGetPasswordHashes_in_t passwd;
    smbGetPasswordHashes_out_t passwdhashes;

    strcpy(passwd.password, "mypassw");

    DPRINTF("GETPASSWORDHASHES... ");
    ret = fileXioDevctl("smb:", SMB_DEVCTL_GETPASSWORDHASHES, (void *)&passwd, sizeof(passwd), (void *)&passwdhashes, sizeof(passwdhashes));
    if (ret == 0) {
        DPRINTF("OK\n");
        DPRINTF("LMhash   = 0x");
        for (i = 0; i < 16; i++)
            DPRINTF("%02X", passwdhashes.LMhash[i]);
        DPRINTF("\n");
        DPRINTF("NTLMhash = 0x");
        for (i = 0; i < 16; i++)
            DPRINTF("%02X", passwdhashes.NTLMhash[i]);
        DPRINTF("\n");
    } else
        DPRINTF("Error %d\n", ret);

    // we now have 32 bytes of hash (16 bytes LM hash + 16 bytes NTLM hash)
    // ----------------------------------------------------------------


    // ----------------------------------------------------------------
    // how to LOGON to SMB server:
    // ----------------------------------------------------------------

    smbLogOn_in_t logon;

    strcpy(logon.serverIP, "192.168.0.2");
    logon.serverPort = 445;
    strcpy(logon.User, "GUEST");
    /*
    strcpy(logon.User, "jimmikaelkael");
    // we could reuse the generated hash
    memcpy((void *)logon.Password, (void *)&passwdhashes, sizeof(passwdhashes));
    logon.PasswordType = HASHED_PASSWORD;
    // or log sending the plaintext password
    strcpy(logon.Password, "mypassw");
    logon.PasswordType = PLAINTEXT_PASSWORD;
    // or simply tell we're not sending password
    logon.PasswordType = NO_PASSWORD;
    */

    DPRINTF("LOGON... ");
    ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGON, (void *)&logon, sizeof(logon), NULL, 0);
    if (ret == 0) {
        DPRINTF("OK ");
    } else {
        DPRINTF("Error %d ", ret);
    }


    // ----------------------------------------------------------------
    // how to send an Echo to SMB server to test if it's alive:
    // ----------------------------------------------------------------
    smbEcho_in_t echo;

    strcpy(echo.echo, "ALIVE ECHO TEST");
    echo.len = strlen("ALIVE ECHO TEST");

    DPRINTF(" ECHO... ");
    ret = fileXioDevctl("smb:", SMB_DEVCTL_ECHO, (void *)&echo, sizeof(echo), NULL, 0);
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // how to get the available share list:
    // ----------------------------------------------------------------

    smbGetShareList_in_t getsharelist;

    getsharelist.EE_addr = (void *)&sharelist[0];
    getsharelist.maxent = 128;

    DPRINTF("GETSHARELIST... ");
    ret = fileXioDevctl("smb:", SMB_DEVCTL_GETSHARELIST, (void *)&getsharelist, sizeof(getsharelist), NULL, 0);
    if (ret >= 0) {
        DPRINTF("OK count = %d\n", ret);
        for (i = 0; i < ret; i++) {
            DPRINTF("\t\t - %s: %s\n", sharelist[i].ShareName, sharelist[i].ShareComment);
        }
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // how to open a share:
    // ----------------------------------------------------------------

    smbOpenShare_in_t openshare;

    strcpy(openshare.ShareName, "PS2SMB");
    // we could reuse the generated hash
    // memcpy((void *)logon.Password, (void *)&passwdhashes, sizeof(passwdhashes));
    // logon.PasswordType = HASHED_PASSWORD;
    // or log sending the plaintext password
    // strcpy(logon.Password, "mypassw");
    // logon.PasswordType = PLAINTEXT_PASSWORD;
    // or simply tell we're not sending password
    // logon.PasswordType = NO_PASSWORD;

    DPRINTF("OPENSHARE... ");
    ret = fileXioDevctl("smb:", SMB_DEVCTL_OPENSHARE, (void *)&openshare, sizeof(openshare), NULL, 0);
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // how to query disk informations: (you must be connected to a share)
    // ----------------------------------------------------------------
    smbQueryDiskInfo_out_t querydiskinfo;

    DPRINTF("QUERYDISKINFO... ");
    ret = fileXioDevctl("smb:", SMB_DEVCTL_QUERYDISKINFO, NULL, 0, (void *)&querydiskinfo, sizeof(querydiskinfo));
    if (ret == 0) {
        DPRINTF("OK\n");
        DPRINTF("Total Units = %d, BlocksPerUnit = %d\n", querydiskinfo.TotalUnits, querydiskinfo.BlocksPerUnit);
        DPRINTF("BlockSize = %d, FreeUnits = %d\n", querydiskinfo.BlockSize, querydiskinfo.FreeUnits);
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // getstat test:
    // ----------------------------------------------------------------

    DPRINTF("IO getstat... ");

    iox_stat_t stats;
    ret = fileXioGetStat("smb:\\", &stats);
    if (ret == 0) {
        DPRINTF("OK\n");

        s64 smb_ctime, smb_atime, smb_mtime;
        ps2time_t ctime, atime, mtime;

        memcpy((void *)&smb_ctime, stats.ctime, 8);
        memcpy((void *)&smb_atime, stats.atime, 8);
        memcpy((void *)&smb_mtime, stats.mtime, 8);

        smbtime2ps2time(smb_ctime, (ps2time_t *)&ctime);
        smbtime2ps2time(smb_atime, (ps2time_t *)&atime);
        smbtime2ps2time(smb_mtime, (ps2time_t *)&mtime);

        s64 hisize = stats.hisize;
        hisize = hisize << 32;
        s64 size = hisize | stats.size;

        DPRINTF("size = %ld, mode = %04x\n", size, stats.mode);

        DPRINTF("ctime = %04d.%02d.%02d %02d:%02d:%02d.%02d\n",
                ctime.year, ctime.month, ctime.day,
                ctime.hour, ctime.min, ctime.sec, ctime.unused);
        DPRINTF("atime = %04d.%02d.%02d %02d:%02d:%02d.%02d\n",
                atime.year, atime.month, atime.day,
                atime.hour, atime.min, atime.sec, atime.unused);
        DPRINTF("mtime = %04d.%02d.%02d %02d:%02d:%02d.%02d\n",
                mtime.year, mtime.month, mtime.day,
                mtime.hour, mtime.min, mtime.sec, mtime.unused);
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // create directory test:
    // ----------------------------------------------------------------
    DPRINTF("IO mkdir... ");
    ret = mkdir("smb:\\created");
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // rename file test:
    // ----------------------------------------------------------------
    /*
        DPRINTF("IO rename... ");
        ret = fileXioRename("smb:\\rename_me\\rename_me.txt", "smb:\\rename_me\\renamed.txt");
        if (ret == 0) {
            DPRINTF("OK\n");
        } else {
            DPRINTF("Error %d\n", ret);
        }
     */


    // ----------------------------------------------------------------
    // rename directory test:
    // ----------------------------------------------------------------
    DPRINTF("IO rename... ");
    ret = fileXioRename("smb:\\created", "smb:\\renamed");
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // delete file test:
    // ----------------------------------------------------------------
    /*
    DPRINTF("IO remove... ");
    ret = remove("smb:\\delete_me\\delete_me.txt");
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }
    */


    // ----------------------------------------------------------------
    // delete directory test:
    // ----------------------------------------------------------------
    DPRINTF("IO rmdir... ");
    ret = rmdir("smb:\\renamed");
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // open file test:
    // ----------------------------------------------------------------

    int fd = fileXioOpen("smb:\\BFTP.iso", O_RDONLY, 0666);
    if (fd >= 0) {
        // 64bit filesize test
        s64 filesize = fileXioLseek64(fd, 0, SEEK_END);
        u8 *p = (u8 *)&filesize;
        DPRINTF("filesize = ");
        for (i = 0; i < 8; i++) {
            DPRINTF("%02X ", p[i]);
        }
        DPRINTF("\n");

        // 64bit offset read test
        fileXioLseek64(fd, filesize - 2041, SEEK_SET);
        u8 buf[16];
        fileXioRead(fd, buf, 16);
        p = (u8 *)buf;
        DPRINTF("read = ");
        for (i = 0; i < 16; i++) {
            DPRINTF("%02X", p[i]);
        }
        DPRINTF("\n");

        // 64bit write test
        /*
        fileXioLseek64(fd, filesize - 16, SEEK_SET);
        fileXioWrite(fd, "\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC", 16);
        fileXioLseek64(fd, filesize - 16, SEEK_SET);
        fileXioRead(fd, buf, 16);
        p = (u8 *)buf;
        DPRINTF("read = ");
        for (i = 0; i < 16; i++) {
            DPRINTF("%02X", p[i]);
        }
        DPRINTF("\n");
        */

        fileXioClose(fd);
    }


    // ----------------------------------------------------------------
    // create file test:
    // ----------------------------------------------------------------

    fd = open("smb:\\testfile", O_RDWR | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        write(fd, "test", 4);
        close(fd);
    }

    // while(1);


    // ----------------------------------------------------------------
    // chdir test
    // ----------------------------------------------------------------

    DPRINTF("IO chdir... ");
    ret = fileXioChdir("smb:\\dossier");
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // dopen/dread/dclose test
    // ----------------------------------------------------------------

    iox_dirent_t dirent;

    DPRINTF("IO dopen... ");
    fd = fileXioDopen("smb:\\");
    if (fd >= 0) {
        DPRINTF("OK\n\t ");
        ret = 1;
        while (ret == 1) {
            ret = fileXioDread(fd, &dirent);
            if (ret == 1)
                DPRINTF("%s ", dirent.name);
        }
        fileXioDclose(fd);
        DPRINTF("\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }

    /*
    // ----------------------------------------------------------------
    // chdir test
    // ----------------------------------------------------------------

    DPRINTF("IO chdir... ");
    ret = fileXioChdir("smb:\\dossier2");
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // dopen/dread/dclose test
    // ----------------------------------------------------------------

    DPRINTF("IO dopen... ");
    fd = fileXioDopen("smb:\\");
    if (fd >= 0) {
        DPRINTF("OK\n\t ");
        ret = 1;
        while (ret == 1) {
            ret = fileXioDread(fd, &dirent);
            if (ret == 1)
                DPRINTF("%s ", dirent.name);
        }
        fileXioDclose(fd);
        DPRINTF("\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // chdir test
    // ----------------------------------------------------------------

    DPRINTF("IO chdir... ");
    ret = fileXioChdir("smb:\\..");
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // dopen/dread/dclose test
    // ----------------------------------------------------------------

    DPRINTF("IO dopen... ");
    fd = fileXioDopen("smb:\\");
    if (fd >= 0) {
        DPRINTF("OK\n\t ");
        ret = 1;
        while (ret == 1) {
            ret = fileXioDread(fd, &dirent);
            if (ret == 1)
                DPRINTF("%s ", dirent.name);
        }
        fileXioDclose(fd);
        DPRINTF("\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }
    */

    // ----------------------------------------------------------------
    // how to close a share:
    // ----------------------------------------------------------------

    DPRINTF("CLOSESHARE... ");
    ret = fileXioDevctl("smb:", SMB_DEVCTL_CLOSESHARE, NULL, 0, NULL, 0);
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    // ----------------------------------------------------------------
    // how to LOGOFF from SMB server:
    // ----------------------------------------------------------------

    DPRINTF("LOGOFF... ");
    ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGOFF, NULL, 0, NULL, 0);
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }


    SleepThread();
    return 0;
}
