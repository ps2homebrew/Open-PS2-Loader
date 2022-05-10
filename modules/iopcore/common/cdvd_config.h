
#ifndef __CDVD_CONFIG__
#define __CDVD_CONFIG__
#define IOPCORE_COMPAT_ALT_READ      0x0001
#define IOPCORE_COMPAT_0_SKIP_VIDEOS 0x0002
#define IOPCORE_COMPAT_EMU_DVDDL     0x0004
#define IOPCORE_COMPAT_ACCU_READS    0x0008
#define IOPCORE_ENABLE_POFF          0x0100
#define IOPCORE_SMB_FORMAT_USBLD     0x0200

#define ISO_MAX_PARTS 10

struct cdvdman_settings_common
{
    u8 NumParts;
    u8 media;
    u16 flags;
    u32 layer1_start;
    u8 DiscID[5];
    u8 bdm_cache;
    u8 hdd_cache;
    u8 smb_cache;
} __attribute__((packed));

struct cdvdman_settings_hdd
{
    struct cdvdman_settings_common common;
    u32 lba_start;
} __attribute__((packed));

struct cdvdman_settings_smb
{
    struct cdvdman_settings_common common;
    char filename[88];
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

struct cdvdman_settings_bdm
{
    struct cdvdman_settings_common common;
    u32 LBAs[ISO_MAX_PARTS];
} __attribute__((packed));

#endif
