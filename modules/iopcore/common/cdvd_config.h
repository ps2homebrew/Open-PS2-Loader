
#ifndef __CDVD_CONFIG__
#define __CDVD_CONFIG__

#include <tamtypes.h>
#include <usbhdfsd-common.h>

// flags
#define IOPCORE_COMPAT_ALT_READ      0x0001
#define IOPCORE_COMPAT_0_SKIP_VIDEOS 0x0002
#define IOPCORE_COMPAT_EMU_DVDDL     0x0004
#define IOPCORE_COMPAT_ACCU_READS    0x0008
#define IOPCORE_ENABLE_POFF          0x0100
#define IOPCORE_SMB_FORMAT_USBLD     0x0200

// fakemodule_flags
#define FAKE_MODULE_FLAG_DEV9    (1 << 0) // not used, compiled in
#define FAKE_MODULE_FLAG_USBD    (1 << 1) // Used with BDM-USB or PADEMU
#define FAKE_MODULE_FLAG_SMAP    (1 << 2) // Used with SMB or BDM-UDPBD
#define FAKE_MODULE_FLAG_ATAD    (1 << 3) // not used, compiled in
#define FAKE_MODULE_FLAG_CDVDSTM (1 << 4) // not used, compiled in
#define FAKE_MODULE_FLAG_CDVDFSV (1 << 5) // not used, compiled in

#define ISO_MAX_PARTS 10

struct cdvdman_settings_common
{
    u8 NumParts;
    u8 media;
    u16 flags;
    u32 layer1_start;
    u8 DiscID[5];
    u8 zso_cache;
    u8 fakemodule_flags;
    u8 padding;
} __attribute__((packed));

struct cdvdman_settings_hdd
{
    struct cdvdman_settings_common common;
    u32 lba_start;
} __attribute__((packed));

struct cdvdman_settings_smb
{
    struct cdvdman_settings_common common;
    char filename[160];
    union
    {
        struct
        {
            // Please keep the string lengths in-sync with the limits within the UI.
            char smb_ip[16];
            u16 smb_port;
            char smb_share[32];
            char smb_prefix[32];
            char smb_user[32];
            char smb_password[32];
        };
        u16 FIDs[ISO_MAX_PARTS];
    };
} __attribute__((packed));

#define BDM_MAX_FILES 1  // ISO
#define BDM_MAX_FRAGS 64 // 64 * 8bytes = 512bytes

struct cdvdman_fragfile
{
    u8 frag_start; /// First fragment in the fragment table
    u8 frag_count; /// Munber of fragments in the fragment table
} __attribute__((packed));

struct cdvdman_settings_bdm
{
    struct cdvdman_settings_common common;

    // Fragmented files:
    // 0 = ISO
    struct cdvdman_fragfile fragfile[BDM_MAX_FILES];

    // Fragment table, containing the fragments of all files
    bd_fragment_t frags[BDM_MAX_FRAGS];
} __attribute__((packed));

#define CDVDMAN_SETTINGS_DEFAULT_COMMON                    \
    {                                                      \
        0x68, 0x68, 0x1234, 0x39393939, "DSKID", 16, 8, 16 \
    }
#define CDVDMAN_SETTINGS_DEFAULT_HDD 0x12345678
#define CDVDMAN_SETTINGS_DEFAULT_SMB                          \
    "######  FILENAME  ######",                               \
    {                                                         \
        {                                                     \
            "192.168.0.10", 0x8510, "PS2SMB", "", "GUEST", "" \
        }                                                     \
    }

#endif
