
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

#define IP_ADDR "192.168.0.10"
#define NETMASK "255.255.255.0"
#define GATEWAY "192.168.0.1"

extern void poweroff_irx;
extern int size_poweroff_irx;
extern void ps2dev9_irx;
extern int size_ps2dev9_irx;
extern void smsutils_irx;
extern int size_smsutils_irx;
extern void lanman_irx;
extern int size_lanman_irx;
extern void iomanx_irx;
extern int size_iomanx_irx;
extern void filexio_irx;
extern int size_filexio_irx;

// for IP config
#define IPCONFIG_MAX_LEN 64
static char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
static int g_ipconfig_len;

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
    int ret, id;

    init_scr();
    scr_clear();
    scr_printf("\t lanlab\n\n");

    SifInitRpc(0);

    scr_printf("\t IOP Reset... ");

    while (!SifIopReset("rom0:UDNL rom0:EELOADCNF", 0))
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
    SifLoadModule("rom0:PADMAN", 0, 0);

    scr_printf("OK\n");

    set_ipconfig();

    scr_printf("\t loading iomanX... ");
    id = SifExecModuleBuffer(&iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
    scr_printf("ret=%d\n", ret);

    scr_printf("\t loading fileXio... ");
    id = SifExecModuleBuffer(&filexio_irx, size_filexio_irx, 0, NULL, &ret);
    scr_printf("ret=%d\n", ret);

    scr_printf("\t loading poweroff... ");
    id = SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, &ret);
    scr_printf("ret=%d\n", ret);

    scr_printf("\t loading ps2dev9... ");
    id = SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
    scr_printf("ret=%d\n", ret);

    scr_printf("\t loading smsutils... ");
    id = SifExecModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL, &ret);
    scr_printf("ret=%d\n", ret);

    scr_printf("\t loading lanman... ");
    id = SifExecModuleBuffer(&lanman_irx, size_lanman_irx, 0, NULL, &ret);
    scr_printf("ret=%d\n", ret);

    scr_printf("\t modules load OK\n");

    printf("printf test!\n");

    SleepThread();
    return 0;
}
