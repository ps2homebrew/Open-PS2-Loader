/*
  Copyright 2010, jimmikaelakel
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include "include/system.h"

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *smstcpip_irx;
extern int size_smstcpip_irx;

extern void *ingame_smstcpip_irx;
extern int size_ingame_smstcpip_irx;

extern void *smsmap_irx;
extern int size_smsmap_irx;

extern void *udptty_irx;
extern int size_udptty_irx;

extern void *smsutils_irx;
extern int size_smsutils_irx;


int debugSetActive(void) {
	int ret, ipconfiglen;
	char ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));

	ipconfiglen = sysSetIPConfig(ipconfig);

	ret = sysLoadModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
	if (ret < 0)
		return -1;

	ret = sysLoadModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL);
	if (ret < 0)
		return -2;

	ret = sysLoadModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL);
	if (ret < 0)
		return -3;

	ret = sysLoadModuleBuffer(&smsmap_irx, size_smsmap_irx, ipconfiglen, ipconfig);
	if (ret < 0)
		return -4;

	ret = sysLoadModuleBuffer(&udptty_irx, size_udptty_irx, 0, NULL);
	if (ret < 0)
		return -5;

	printf("OPL UDPtty debug enabled!\n");

	return 0;
}

