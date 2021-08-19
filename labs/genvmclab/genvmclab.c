
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

#include "../../modules/vmc/genvmc/genvmc.h"

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
extern void usbd_irx;
extern int size_usbd_irx;
extern void usbhdfsd_irx;
extern int size_usbhdfsd_irx;
extern void genvmc_irx;
extern int size_genvmc_irx;
extern void mcman_irx;
extern int size_mcman_irx;

// for IP config
#define IPCONFIG_MAX_LEN 64
static char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
static int g_ipconfig_len;

//--------------------------------------------------------------
void delay(int count)
{
    int i;
    int ret;
    for (i = 0; i < count; i++) {
        ret = 0x01000000;
        while (ret--)
            asm("nop\nnop\nnop\nnop");
    }
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
    int ret;

    init_scr();
    scr_clear();

    DPRINTF("genvmclab start...\n");

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
    // SifLoadModule("rom0:MCMAN", 0, 0);

    DPRINTF("OK\n");

    set_ipconfig();

    // HomeBrew mcman required by genvmc for VMCcopy function to work (on my V3, mcman
    // from ROM is buggy and we can't read MC pages directly)
    DPRINTF("loading mcman... ");
    SifExecModuleBuffer(&mcman_irx, size_mcman_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading iomanX... ");
    SifExecModuleBuffer(&iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading fileXio... ");
    SifExecModuleBuffer(&filexio_irx, size_filexio_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading poweroff... "); // modules for debugging
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

    DPRINTF("loading genvmc... "); // the module in testing
    SifExecModuleBuffer(&genvmc_irx, size_genvmc_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading usbd... "); // usb drivers needed to create file on it
    SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    DPRINTF("loading usbhdfsd... ");
    SifExecModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &ret);
    DPRINTF("ret=%d\n", ret);

    delay(3); // some delay is required by usb mass storage driver

    DPRINTF("modules load OK\n");

    fileXioInit();

    // ----------------------------------------------------------------
    // how to get a blank vmc file created
    // ----------------------------------------------------------------
    createVMCparam_t p;
    statusVMCparam_t vmc_stats;

    memset(&p, 0, sizeof(createVMCparam_t));
    strcpy(p.VMC_filename, "mass:8MB_VMC0.bin");
    p.VMC_card_slot = -1; // do not forget this for blank VMC file creation
    p.VMC_size_mb = 8;
    p.VMC_blocksize = 16; // usually official MC has blocksize of 16
    p.VMC_thread_priority = 0xf;
    DPRINTF("requesting VMC file creation... ");
    ret = fileXioDevctl("genvmc:", GENVMC_DEVCTL_CREATE_VMC, (void *)&p, sizeof(p), NULL, 0);
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }

    memset(&vmc_stats, 0, sizeof(statusVMCparam_t));
    DPRINTF("waiting VMC file created...\n");
    while (1) {

        ret = fileXioDevctl("genvmc:", GENVMC_DEVCTL_STATUS, NULL, 0, (void *)&vmc_stats, sizeof(vmc_stats));
        if (ret == 0) {
            printf("progress: %d message: %s\n", vmc_stats.VMC_progress, vmc_stats.VMC_msg);
            if (vmc_stats.VMC_status == GENVMC_STAT_AVAIL)
                break;
        }

        delay(2);
    }
    DPRINTF("Done\n");
    DPRINTF("VMC Error = %d\n", vmc_stats.VMC_error);

    // ----------------------------------------------------------------
    // how to get a vmc file created from an existing MC
    // ----------------------------------------------------------------
    memset(&p, 0, sizeof(createVMCparam_t));
    strcpy(p.VMC_filename, "mass:8MB_VMC1.bin");
    p.VMC_card_slot = 0; // 0=slot 1, 1=slot 2
    p.VMC_thread_priority = 0xf;
    DPRINTF("requesting VMC file creation... ");
    ret = fileXioDevctl("genvmc:", GENVMC_DEVCTL_CREATE_VMC, (void *)&p, sizeof(p), NULL, 0);
    if (ret == 0) {
        DPRINTF("OK\n");
    } else {
        DPRINTF("Error %d\n", ret);
    }

    memset(&vmc_stats, 0, sizeof(statusVMCparam_t));
    DPRINTF("waiting VMC file created...\n");
    while (1) {

        ret = fileXioDevctl("genvmc:", GENVMC_DEVCTL_STATUS, NULL, 0, (void *)&vmc_stats, sizeof(vmc_stats));
        if (ret == 0) {
            printf("progress: %d message: %s\n", vmc_stats.VMC_progress, vmc_stats.VMC_msg);
            if (vmc_stats.VMC_status == GENVMC_STAT_AVAIL)
                break;
        }

        delay(2);
    }
    DPRINTF("Done\n");
    DPRINTF("VMC Error = %d\n", vmc_stats.VMC_error);

    SleepThread();
    return 0;
}
