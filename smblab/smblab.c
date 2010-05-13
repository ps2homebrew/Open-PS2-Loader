
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <fileio.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sbv_patches.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <debug.h>

#include "smbman.h"

#define	IP_ADDR "192.168.0.10"
#define	NETMASK "255.255.255.0"
#define	GATEWAY "192.168.0.1"

extern void poweroff_irx;
extern int  size_poweroff_irx;
extern void ps2dev9_irx;
extern int  size_ps2dev9_irx;
extern void smsutils_irx;
extern int  size_smsutils_irx;
extern void smstcpip_irx;
extern int  size_smstcpip_irx;
extern void smsmap_irx;
extern int  size_smsmap_irx;
extern void smbman_irx;
extern int  size_smbman_irx;
extern void iomanx_irx;
extern int  size_iomanx_irx;
extern void filexio_irx;
extern int  size_filexio_irx;

// for IP config
#define IPCONFIG_MAX_LEN	64
static char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
static int g_ipconfig_len;

static ShareEntry_t sharelist[128] __attribute__((aligned(64)));

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
	scr_printf("\t smblab\n\n");
	
	SifInitRpc(0);

	scr_printf("\t IOP Reset... ");
	
	while(!SifIopReset("rom0:UDNL rom0:EELOADCNF",0));
  	while(!SifIopSync());;
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

	scr_printf("OK\n");
        	  
	set_ipconfig();
 
	scr_printf("\t loading modules... ");
 
	id = SifExecModuleBuffer(&iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
	id = SifExecModuleBuffer(&filexio_irx, size_filexio_irx, 0, NULL, &ret);
	id = SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, &ret);
	id = SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
	id = SifExecModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL, &ret);
	id = SifExecModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL, &ret);
	id = SifExecModuleBuffer(&smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig, &ret);
	id = SifExecModuleBuffer(&smbman_irx, size_smbman_irx, 0, NULL, &ret);

	scr_printf("OK\n");

	fileXioInit();

	smbLogOn_in_t logon;

	strcpy(logon.serverIP, "192.168.0.2");
	logon.serverPort = 445;
	strcpy(logon.User, "GUEST");
	//strcpy(logon.User, "jimmikaelkael");
	//strcpy(logon.Password, "mypass");
	//logon.PasswordType = PLAINTEXT_PASSWORD;

	scr_printf("\t LOGON... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGON, (void *)&logon, sizeof(logon), NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);

	smbGetShareList_in_t getsharelist;
	getsharelist.EE_addr = (void *)&sharelist[0];
	getsharelist.maxent = 128;

	scr_printf("\t GETSHARELIST... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_GETSHARELIST, (void *)&getsharelist, sizeof(getsharelist), NULL, 0);
	if (ret >= 0) {
		scr_printf("OK count = %d\n", ret);
		int i;
		for (i=0; i<ret; i++) {
			scr_printf("\t\t - %s: %s\n", sharelist[i].ShareName, sharelist[i].ShareComment);
		}
	}
	else
		scr_printf("Error %d\n", ret);

	smbOpenShare_in_t openshare;

	strcpy(openshare.ShareName, "PS2SMB");
	//strcpy(openshare.Password, "mypass");
	//openshare.PasswordType = PLAINTEXT_PASSWORD;

	scr_printf("\t OPENSHARE... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_OPENSHARE, (void *)&openshare, sizeof(openshare), NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);

	int fd = open("smb:\\BFTP.iso", O_RDONLY);
	if (fd >= 0)
		close(fd);

	scr_printf("\t CLOSESHARE... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_CLOSESHARE, NULL, 0, NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);

	scr_printf("\t LOGOFF... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGOFF, NULL, 0, NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);

   	SleepThread();
   	return 0;
}
