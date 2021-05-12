#define IOPCORE_COMPAT_ALT_READ 0x0001
#define IOPCORE_COMPAT_0_SKIP_VIDEOS 0x0002
#define IOPCORE_COMPAT_EMU_DVDDL 0x0004
#define IOPCORE_COMPAT_ACCU_READS 0x0008
#define IOPCORE_ENABLE_POFF 0x0100
#define IOPCORE_SMB_FORMAT_USBLD 0x0200

#define ISO_MAX_PARTS 10

struct cdvdman_settings_common
{
    u8 NumParts;
    u8 media;
    u16 flags;
    u32 layer1_start;
    u8 DiscID[5];
    u8 padding[3];
};

struct cdvdman_settings_hdd
{
    struct cdvdman_settings_common common;
    u32 lba_start;
};

struct cdvdman_settings_smb
{
    struct cdvdman_settings_common common;
    s8 filename[80];
    union
    {
        struct
        {
            //Please keep the string lengths in-sync with the limits within the UI.
            s8 smb_ip[16];
            u16 smb_port;
            s8 smb_share[32];
            s8 smb_prefix[32];
            s8 smb_user[32];
            s8 smb_password[32];
        };
        u16 FIDs[ISO_MAX_PARTS];
    };
};

struct cdvdman_settings_bdm
{
    struct cdvdman_settings_common common;
    u32 LBAs[ISO_MAX_PARTS];
};
