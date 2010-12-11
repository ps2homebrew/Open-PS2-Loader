/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "loader.h"

static char ElfPath[255]; // it should be here to avoid it to be wiped by the clear user mem

int main(int argc, char **argv){

	DINIT();
	DPRINTF("OPL EE core start!\n");

	SifInitRpc(0);

#ifdef LOAD_EECORE_DOWN
	g_buf = (u8 *)0x01700000;
#else
	g_buf = (u8 *)0x00088000;
#endif
	DPRINTF("g_buf at 0x%08x\n", (int)g_buf);

	argv[1][11]=0x00; // fix for 8+3 filename.

	_strcpy(ElfPath, "cdrom0:\\");
	_strcat(ElfPath, argv[1]);
	_strcat(ElfPath, ";1");
	_strcpy(GameID, argv[1]);
	DPRINTF("Elf path = '%s'\n", ElfPath);
	DPRINTF("Game ID = '%s'\n", GameID);

	if (!_strncmp(argv[0], "USB_MODE", 8))
		GameMode = USB_MODE;
	else if (!_strncmp(argv[0], "ETH_MODE", 8))
		GameMode = ETH_MODE;
	else if (!_strncmp(argv[0], "HDD_MODE", 8))
		GameMode = HDD_MODE;
	DPRINTF("Game Mode = %d\n", GameMode);

	if (!_strncmp(&argv[0][9], "0", 1))
		ExitMode = OSDS_MODE;
	else if (!_strncmp(&argv[0][9], "1", 1))
		ExitMode = BOOT_MODE;
	else if (!_strncmp(&argv[0][9], "2", 1))
		ExitMode = APPS_MODE;
	DPRINTF("Exit Mode = %d\n", ExitMode);
	
	DisableDebug = 0;
	if (!_strncmp(&argv[0][11], "1", 1)) {
		DisableDebug = 1;
		DPRINTF("Debug color screens enabled\n");
	}

	char *p = _strtok(&argv[0][13], " ");
	USBDelay = _strtoui(p);
	DPRINTF("USB Delay = %d\n", USBDelay);
	p = _strtok(NULL, " ");
	_strcpy(g_ps2_ip, p);
	p = _strtok(NULL, " ");
	_strcpy(g_ps2_netmask, p);
	p = _strtok(NULL, " ");
	_strcpy(g_ps2_gateway, p);
	DPRINTF("IP=%s NM=%s GW=%s\n", g_ps2_ip, g_ps2_netmask, g_ps2_gateway);	

	// bitmask of the compat. settings
	g_compat_mask = _strtoui(argv[2]);
	DPRINTF("Compat Mask = 0x%02x\n", g_compat_mask);

	set_ipconfig();

	DPRINTF("Get back IOP modules from Kernel RAM...\n");	
	GetIrxKernelRAM();

	/* installing kernel hooks */
	DPRINTF("Installing Kernel Hooks...\n");
	Install_Kernel_Hooks();

	if(!DisableDebug)
		GS_BGCOLOUR = 0xff0000; 

	DPRINTF("Executing '%s'...\n", ElfPath);
	LoadExecPS2(ElfPath, 0, NULL);	

	if(!DisableDebug)
		GS_BGCOLOUR = 0x0000ff;
	DPRINTF("LoadExecPS2 failed!\n");

	SleepThread();

	return 0;
}

