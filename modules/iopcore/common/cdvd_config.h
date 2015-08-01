#define IOPCORE_COMPAT_ALT_READ		0x0001
#define IOPCORE_COMPAT_0_PSS		0x0002
#define IOPCORE_COMPAT_EMU_DVDDL	0x0004
#define IOPCORE_COMPAT_ACCU_READS	0x0008
#define IOPCORE_ENABLE_POFF		0x0100
#define IOPCORE_SMB_FORMAT_USBLD	0x0200

#define ISO_MAX_PARTS 10

struct cdvdman_settings_common{
	u8 NumParts;
	u8 media;
	u16 flags;
	u32 cb_timer;
	u32 layer1_start;
	u8 DiscID[5];
	u8 padding[3];
};

struct cdvdman_settings_hdd{
	struct cdvdman_settings_common common;
	u32 lba_start;
};

struct cdvdman_settings_smb{
	struct cdvdman_settings_common common;
	s8 filename[80];
	union{
		struct{
			s8 pc_ip[16];
			u16 pc_port;
			s8 pc_share[33];
			s8 pc_prefix[33];
			s8 smb_user[17];
			s8 smb_password[17];
		};
		u16 FIDs[ISO_MAX_PARTS];
	};
};

struct fat_file{
	u32 cluster;
	u32 size;
};

struct cdvdman_settings_usb{
	struct cdvdman_settings_common common;
	u32 layer1_start;
	u32 fatStart;
	u32 dataStart;
	u16 clusterSize;
	u16 padding;
	struct fat_file files[ISO_MAX_PARTS];
};
