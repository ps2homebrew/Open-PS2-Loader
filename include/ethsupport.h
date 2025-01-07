#ifndef __ETH_SUPPORT_H
#define __ETH_SUPPORT_H

#define ETH_MODE_UPDATE_DELAY 300

typedef struct
{
    int active;       /* Activation flag */
    char fname[64];   /* File name (memorycard?.bin) */
    u16 fid;          /* SMB File ID */
    int flags;        /* Card flag */
    vmc_spec_t specs; /* Card specifications */
} smb_vmc_infos_t;

void ethDeinitModules(void);      // Module-only deinitialization, without the GUI's knowledge (for specific reasons, otherwise unused).
int ethLoadInitModules(void);     // Initializes Ethernet and applies configuration.
void ethDisplayErrorStatus(void); // Displays the current error status (if any). GUI must be already initialized.
int ethGetNetConfig(u8 *ip_address, u8 *netmask, u8 *gateway);
int ethApplyConfig(void);
item_list_t *ethGetObject(int initOnly);

#endif
