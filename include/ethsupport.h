#ifndef __ETH_SUPPORT_H
#define __ETH_SUPPORT_H

#include "include/iosupport.h"

#define ETH_MODE_UPDATE_DELAY	300

#ifdef VMC
#include "include/mcemu.h"
typedef struct {
	int        active;    /* Activation flag */
	char       fname[64]; /* File name (memorycard?.bin) */
	u16        fid;       /* SMB File ID */
	int        flags;     /* Card flag */
	vmc_spec_t specs;     /* Card specifications */
} smb_vmc_infos_t;
#endif

void ethInit(void);
int ethLoadModules(void);
int ethGetNetConfig(u8 *ip_address, u8 *netmask, u8 *gateway);
int ethWaitValidNetIFLinkState(void);
int ethWaitValidDHCPState(void);
int ethApplyNetIFConfig(void);
int ethGetNetIFLinkStatus(void);
int ethApplyIPConfig(void);
int ethGetDHCPStatus(void);
item_list_t* ethGetObject(int initOnly);

#endif
