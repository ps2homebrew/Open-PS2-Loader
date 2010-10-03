#ifndef __ETH_SUPPORT_H
#define __ETH_SUPPORT_H

#include "include/iosupport.h"

#ifdef VMC
#include "include/mcemu.h"
typedef struct {
	int        active;    /* Activation flag */
	char       fname[32]; /* File name (memorycard?.bin) */
	u16        fid;       /* SMB File ID */
	int        flags;     /* Card flag */
	vmc_spec_t specs;     /* Card specifications */
} smb_vmc_infos_t;
#endif

void ethInit();
item_list_t* ethGetObject(int initOnly);

int ethSMBConnect(void);
int ethSMBDisconnect(void);

#endif
