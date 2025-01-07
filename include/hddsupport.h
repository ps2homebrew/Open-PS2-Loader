#ifndef __HDD_SUPPORT_H
#define __HDD_SUPPORT_H

typedef struct
{
    u32 start;
    u32 length;
} apa_subs;

typedef struct
{
    int active;                 /* Activation flag */
    apa_subs parts[5];          /* Vmc file Apa partitions */
    pfs_blockinfo_t blocks[10]; /* Vmc file Pfs inode */
    int flags;                  /* Card flag */
    vmc_spec_t specs;           /* Card specifications */
} hdd_vmc_infos_t;

void hddInit();
item_list_t *hddGetObject(int initOnly);
void hddLoadModules(void);
void hddLoadSupportModules(void);
void hddLaunchGame(item_list_t *itemList, int id, config_set_t *configSet);

#endif
