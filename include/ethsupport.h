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

#define ERROR_ETH_NOT_STARTED			100
#define ERROR_ETH_MODULE_PS2DEV9_FAILURE	200
#define ERROR_ETH_MODULE_SMSUTILS_FAILURE	201
#define ERROR_ETH_MODULE_SMSTCPIP_FAILURE	202
#define ERROR_ETH_MODULE_SMSMAP_FAILURE		203
#define ERROR_ETH_MODULE_SMBMAN_FAILURE		204
#define ERROR_ETH_SMB_CONN			300
#define ERROR_ETH_SMB_LOGON			301
#define ERROR_ETH_SMB_ECHO			302
#define ERROR_ETH_SMB_OPENSHARE			303
#define ERROR_ETH_SMB_LISTSHARES		304
#define ERROR_ETH_SMB_LISTGAMES			305

void ethInit(void);
int ethLoadModules(void);
item_list_t* ethGetObject(int initOnly);

#endif
