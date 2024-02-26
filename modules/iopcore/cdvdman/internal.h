
#ifndef __CDVDMAN_INTERNAL__
#define __CDVDMAN_INTERNAL__

#include "dev9.h"
#include "oplsmb.h"
#include "smb.h"
#include "atad.h"
#include "ioplib_util.h"
#include "cdvdman_opl.h"
#include "cdvd_config.h"
#include "device.h"

#include <loadcore.h>
#include <stdio.h>
#include <sifman.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <ioman.h>
#include <thsemap.h>
#include <errno.h>
#include <io_common.h>
#include <usbd.h>
#include <cdvdman.h>
#include "ioman_add.h"

#include <defs.h>

#include "smsutils.h"

#ifdef __IOPCORE_DEBUG
#define DPRINTF(args...)  printf(args)
#define iDPRINTF(args...) Kprintf(args)
#else
#define DPRINTF(args...)
#define iDPRINTF(args...)
#endif

#ifdef HDD_DRIVER
#define CDVDMAN_SETTINGS_TYPE                    cdvdman_settings_hdd
#define CDVDMAN_SETTINGS_DEFAULT_DEVICE_SETTINGS CDVDMAN_SETTINGS_DEFAULT_HDD,
#elif SMB_DRIVER
#define CDVDMAN_SETTINGS_TYPE                    cdvdman_settings_smb
#define CDVDMAN_SETTINGS_DEFAULT_DEVICE_SETTINGS CDVDMAN_SETTINGS_DEFAULT_SMB,
#elif BDM_DRIVER
#define CDVDMAN_SETTINGS_TYPE cdvdman_settings_bdm
#define CDVDMAN_SETTINGS_DEFAULT_DEVICE_SETTINGS
#else
#error Unknown driver type. Please check the Makefile.
#endif

#define btoi(b) ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define itob(i) ((i) / 10 * 16 + (i) % 10) /* u_char to BCD */
struct SteamingData
{
    unsigned short int StBufmax;
    unsigned short int StBankmax;
    unsigned short int StBanksize;
    unsigned short int StWritePtr;
    unsigned short int StReadPtr;
    unsigned short int StStreamed;
    unsigned short int StStat;
    unsigned short int StIsReading;
    void *StIOP_bufaddr;
    u32 Stlsn;
};

typedef struct
{
    int err;
    u8 status; // SCECdvdDriveState
    struct SteamingData StreamingData;
    int intr_ef;
    int disc_type_reg; // SCECdvdMediaType
    u32 cdread_lba;
    u32 cdread_sectors;
    u16 sector_size;
    void *cdread_buf;
} cdvdman_status_t;

struct dirTocEntry
{
    short length;
    u32 fileLBA;         // 2
    u32 fileLBA_bigend;  // 6
    u32 fileSize;        // 10
    u32 fileSize_bigend; // 14
    u8 dateStamp[6];     // 18
    u8 reserved1;        // 24
    u8 fileProperties;   // 25
    u8 reserved2[6];     // 26
    u8 filenameLength;   // 32
    char filename[128];  // 33
} __attribute__((packed));

typedef void (*StmCallback_t)(void);

// Internal (common) function prototypes
extern void SetStm0Callback(StmCallback_t callback);
extern int cdvdman_AsyncRead(u32 lsn, u32 sectors, u16 sector_size, void *buf);
extern int cdvdman_SyncRead(u32 lsn, u32 sectors, u16 sector_size, void *buf);
extern int cdvdman_sendSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size);
extern void cdvdman_cb_event(int reason);

extern void cdvdman_init(void);
extern void cdvdman_fs_init(void);
extern void cdvdman_searchfile_init(void);
extern void cdvdman_initdev(void);

extern struct CDVDMAN_SETTINGS_TYPE cdvdman_settings;

#ifdef HDD_DRIVER
// HDD driver also uses this buffer, for aligning unaligned reads.
#define CDVDMAN_BUF_SECTORS 2
#else
// Normally this buffer is only used by 'searchfile', only 1 sector used
#define CDVDMAN_BUF_SECTORS 1
#endif
extern u8 cdvdman_buf[CDVDMAN_BUF_SECTORS * 2048];

extern int cdrom_io_sema;
extern int cdvdman_searchfilesema;

extern cdvdman_status_t cdvdman_stat;

extern unsigned char sync_flag;
extern unsigned char cdvdman_cdinited;
extern u32 mediaLsnCount;

#endif
