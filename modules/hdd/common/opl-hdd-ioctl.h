#ifndef OPL_HDD_IOCTL_H
#define OPL_HDD_IOCTL_H

// Commands and structures for HDD.IRX
#define APA_DEVCTL_IS_48BIT		0x6840
#define APA_DEVCTL_SET_TRANSFER_MODE	0x6841
#define APA_DEVCTL_ATA_IOP_WRITE	0x6842

#define APA_IOCTL2_GETHEADER		0x6836

// structs for DEVCTL commands

typedef struct
{
	u32 lba;
	u32 size;
	u8 *data;
} hddAtaIOPTransfer_t; 

typedef struct
{
	int type;
	int mode;
} hddAtaSetMode_t; 

// Commands and structures for PFS.IRX
#define PFS_IOCTL2_GET_INODE		0x7007

#endif
