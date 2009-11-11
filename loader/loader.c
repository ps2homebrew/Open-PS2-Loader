/*
  Copyright 2009, Ifcaro & jimmikaelkael
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

extern void *usbd_irx;
extern int size_usbd_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

extern void *isofs_irx;
extern int size_isofs_irx;
 
#include "loader.h"

char ElfPath[255]; // it should be here to avoid it to be wiped by the clear user mem

void set_ipconfig(void)
{
	memset(g_ipconfig, 0, IPCONFIG_MAX_LEN);
	g_ipconfig_len = 0;
	
	// add ip to g_ipconfig buf
	strncpy(&g_ipconfig[g_ipconfig_len], g_ps2_ip, 15);
	g_ipconfig_len += strlen(g_ps2_ip) + 1;

	// add netmask to g_ipconfig buf
	strncpy(&g_ipconfig[g_ipconfig_len], g_ps2_netmask, 15);
	g_ipconfig_len += strlen(g_ps2_netmask) + 1;

	// add gateway to g_ipconfig buf
	strncpy(&g_ipconfig[g_ipconfig_len], g_ps2_gateway, 15);
	g_ipconfig_len += strlen(g_ps2_gateway) + 1;
}

int main(int argc, char **argv){
	
	printf("Starting...\n");
	
	SifInitRpc(0);

#ifdef LOAD_EECORE_DOWN
	g_buf = (u8 *)0x01700000;
#else
	g_buf = (u8 *)0x00088000;
#endif  

	argv[1][12]=0x00; // fix for 8+3 filename.

	sprintf(ElfPath,"cdrom0:\\%s;1",argv[1]);
		
	if (!strncmp(argv[0], "USB_MODE", 8))
		GameMode = USB_MODE;
	else if (!strncmp(argv[0], "ETH_MODE", 8))
		GameMode = ETH_MODE;
	
	char *p = strtok(&argv[0][9], " ");
	strcpy(g_ps2_ip, p);
	p = strtok(NULL, " ");
	strcpy(g_ps2_netmask, p);
	p = strtok(NULL, " ");
	strcpy(g_ps2_gateway, p);

	// bitmask of the compat. settings
	p = strtok(NULL, " ");
	
	int mask = atoi(p);
	
	set_ipconfig();
	
	GetIrxKernelRAM();

	/* installing kernel hooks */
	Install_Kernel_Hooks();

	GS_BGCOLOUR = 0xff0000; 

	LoadExecPS2(ElfPath, 0, NULL);	

	GS_BGCOLOUR = 0x0000ff;
	SleepThread();

	return 0;
}

