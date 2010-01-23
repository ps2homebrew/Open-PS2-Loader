/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "loader.h"

char ElfPath[255]; // it should be here to avoid it to be wiped by the clear user mem

int main(int argc, char **argv){
	
	SifInitRpc(0);

#ifdef LOAD_EECORE_DOWN
	g_buf = (u8 *)0x01700000;
#else
	g_buf = (u8 *)0x00088000;
#endif  

	argv[1][11]=0x00; // fix for 8+3 filename.

	strcpy(ElfPath, "cdrom0:\\");
	strcat(ElfPath, argv[1]);
	strcat(ElfPath, ";1");
	strcpy(GameID, argv[1]);

	if (!strncmp(argv[0], "USB_MODE", 8))
		GameMode = USB_MODE;
	else if (!strncmp(argv[0], "ETH_MODE", 8))
		GameMode = ETH_MODE;
	else if (!strncmp(argv[0], "HDD_MODE", 8))
		GameMode = HDD_MODE;	
	
	if (!strncmp(&argv[0][9], "0", 1))
		ExitMode = OSDS_MODE;
	else if (!strncmp(&argv[0][9], "1", 1))
		ExitMode = BOOT_MODE;
	else if (!strncmp(&argv[0][9], "2", 1))
		ExitMode = APPS_MODE;
	
	char *p = strtok(&argv[0][11], " ");
	strcpy(g_ps2_ip, p);
	p = strtok(NULL, " ");
	strcpy(g_ps2_netmask, p);
	p = strtok(NULL, " ");
	strcpy(g_ps2_gateway, p);

	// bitmask of the compat. settings
	g_compat_mask = _strtoui(argv[2]);

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

