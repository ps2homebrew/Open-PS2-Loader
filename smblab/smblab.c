
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

static ShareEntry_t sharelist[128] __attribute__((aligned(64))); // Keep this aligned for DMA!

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
	int i, ret, id;
	
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


	// ----------------------------------------------------------------
	// how to get password hashes:
	// ----------------------------------------------------------------

	smbGetPasswordHashes_in_t passwd;
	smbGetPasswordHashes_out_t passwdhashes;

	strcpy(passwd.password, "mypassw");

	scr_printf("\t GETPASSWORDHASHES... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_GETPASSWORDHASHES, (void *)&passwd, sizeof(passwd), (void *)&passwdhashes, sizeof(passwdhashes));
	if (ret == 0) {
		scr_printf("OK\n");
		scr_printf("\t LMhash   = 0x");
		for (i=0; i<16; i++)
			scr_printf("%02X", passwdhashes.LMhash[i]);
		scr_printf("\n");
		scr_printf("\t NTLMhash = 0x");
		for (i=0; i<16; i++)
			scr_printf("%02X", passwdhashes.NTLMhash[i]);
		scr_printf("\n");
	}
	else
		scr_printf("Error %d\n", ret);

	// we now have 32 bytes of hash (16 bytes LM hash + 16 bytes NTLM hash)
	// ----------------------------------------------------------------


	// ----------------------------------------------------------------
	// how to LOGON to SMB server:
	// ----------------------------------------------------------------

	smbLogOn_in_t logon;

	strcpy(logon.serverIP, "192.168.0.2");
	logon.serverPort = 445;
	strcpy(logon.User, "GUEST");
	//strcpy(logon.User, "jimmikaelkael");
	// we could reuse the generated hash
	//memcpy((void *)logon.Password, (void *)&passwdhashes, sizeof(passwdhashes));
	//logon.PasswordType = HASHED_PASSWORD;
	// or log sending the plaintext password
	//strcpy(logon.Password, "mypassw");
	//logon.PasswordType = PLAINTEXT_PASSWORD;
	// or simply tell we're not sending password
	//logon.PasswordType = NO_PASSWORD;

	scr_printf("\t LOGON... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGON, (void *)&logon, sizeof(logon), NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);


	// ----------------------------------------------------------------
	// how to get the available share list:
	// ----------------------------------------------------------------

	smbGetShareList_in_t getsharelist;

	getsharelist.EE_addr = (void *)&sharelist[0];
	getsharelist.maxent = 128;

	scr_printf("\t GETSHARELIST... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_GETSHARELIST, (void *)&getsharelist, sizeof(getsharelist), NULL, 0);
	if (ret >= 0) {
		scr_printf("OK count = %d\n", ret);
		for (i=0; i<ret; i++) {
			scr_printf("\t\t - %s: %s\n", sharelist[i].ShareName, sharelist[i].ShareComment);
		}
	}
	else
		scr_printf("Error %d\n", ret);


	// ----------------------------------------------------------------
	// how to open a share:
	// ----------------------------------------------------------------

	smbOpenShare_in_t openshare;

	strcpy(openshare.ShareName, "PS2SMB");
	// we could reuse the generated hash
	//memcpy((void *)logon.Password, (void *)&passwdhashes, sizeof(passwdhashes));
	//logon.PasswordType = HASHED_PASSWORD;
	// or log sending the plaintext password
	//strcpy(logon.Password, "mypassw");
	//logon.PasswordType = PLAINTEXT_PASSWORD;
	// or simply tell we're not sending password
	//logon.PasswordType = NO_PASSWORD;

	scr_printf("\t OPENSHARE... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_OPENSHARE, (void *)&openshare, sizeof(openshare), NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);


	// ----------------------------------------------------------------
	// open file test:
	// ----------------------------------------------------------------

	int fd = fileXioOpen("smb:\\BFTP.iso", O_RDONLY, 0666);
	if (fd >= 0) {
		//s64 pos = 0x0100523200;
		//u8 *p = (u8 *)&pos;
		//scr_printf("\t pos = ");
		//for (i=0; i<8; i++) {
		//	scr_printf("%02X ", p[i]);
		//}
		//scr_printf("\n");
		s64 filesize = fileXioLseek64(fd, 0, SEEK_END);
		u8 *p = (u8 *)&filesize;
		scr_printf("\t filesize = ");
		for (i=0; i<8; i++) {
			scr_printf("%02X ", p[i]);
		}
		scr_printf("\n");
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


	// ----------------------------------------------------------------
	// how to close a share:
	// ----------------------------------------------------------------

	scr_printf("\t CLOSESHARE... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_CLOSESHARE, NULL, 0, NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);


	// ----------------------------------------------------------------
	// how to send an Echo to SMB server:
	// ----------------------------------------------------------------

	smbEcho_in_t echo;

	strcpy(echo.echo, "ECHO TEST");
	echo.len = strlen("ECHO TEST");

	scr_printf("\t ECHO... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_ECHO, (void *)&echo, sizeof(echo), NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);


	// ----------------------------------------------------------------
	// how to LOGOFF from SMB server:
	// ----------------------------------------------------------------

	scr_printf("\t LOGOFF... ");
	ret = fileXioDevctl("smb:", SMB_DEVCTL_LOGOFF, NULL, 0, NULL, 0);
	if (ret == 0)
		scr_printf("OK\n");
	else
		scr_printf("Error %d\n", ret);


   	SleepThread();
   	return 0;
}
