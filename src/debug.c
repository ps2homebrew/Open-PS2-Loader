/*
  Copyright 2010, jimmikaelakel
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/ethsupport.h"
#include "include/system.h"
#include "include/ioman.h"

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *netman_irx;
extern int size_netman_irx;

extern void *dns_irx;
extern int size_dns_irx;

extern void *ps2ips_irx;
extern int size_ps2ips_irx;

extern void *ps2ip_irx;
extern int size_ps2ip_irx;

extern void *smap_irx;
extern int size_smap_irx;

extern void *udptty_irx;
extern int size_udptty_irx;

extern void *ioptrap_irx;
extern int size_ioptrap_irx;

extern void *ps2link_irx;
extern int size_ps2link_irx;

extern void *smsutils_irx;
extern int size_smsutils_irx;

int debugSetActive(void) {
	int ret;

	ret = sysLoadModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
	if (ret < 0)
		return -1;

	ret = sysLoadModuleBuffer(&netman_irx, size_netman_irx, 0, NULL);
	if (ret < 0)
		return -2;

	NetManInit();

	ret = sysLoadModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL);
	if (ret < 0)
		return -3;

	ret = sysLoadModuleBuffer(&ps2ip_irx, size_ps2ip_irx, 0, NULL);
	if (ret < 0)
		return -4;

	ret = sysLoadModuleBuffer(&smap_irx, size_smap_irx, 0, NULL);
	if (ret < 0)
		return -5;

	ret = sysLoadModuleBuffer(&dns_irx, size_dns_irx, 0, NULL);
	if (ret < 0)
		return -6;

	ret = sysLoadModuleBuffer(&ps2ips_irx, size_ps2ips_irx, 0, NULL);
	if (ret < 0)
		return -7;

	ps2ip_init();

	ethWaitValidNetIFLinkState();
	ethApplyNetIFConfig();
	ethWaitValidNetIFLinkState();

	ethApplyIPConfig();
	if(ps2_ip_use_dhcp) ethWaitValidDHCPState();

	ret = sysLoadModuleBuffer(&udptty_irx, size_udptty_irx, 0, NULL);
	if (ret < 0)
		return -8;

	ret = sysLoadModuleBuffer(&ioptrap_irx, size_ioptrap_irx, 0, NULL);
	if (ret < 0)
		return -9;

	ret = sysLoadModuleBuffer(&ps2link_irx, size_ps2link_irx, 0, NULL);
	if (ret < 0)
		return -10;

	return 0;
}
