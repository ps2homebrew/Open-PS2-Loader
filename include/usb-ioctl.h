#define USBHDFSD_IOCTL_GETSECTOR	0x1000	//jimmikaelkael: Ioctl request code => get file start sector
#define USBHDFSD_IOCTL_GETCLUSTER	0x1001	//jimmikaelkael: Ioctl request code => get file start cluster
#define USBHDFSD_IOCTL_GETSIZE		0x1002	//jimmikaelkael: Ioctl request code => get file size
#define USBHDFSD_IOCTL_GETDEVSECTORSIZE	0x1003	//jimmikaelkael: Ioctl request code => get mass storage device sector size
#define USBHDFSD_IOCTL_GETFATSTART	0x1004
#define USBHDFSD_IOCTL_GETDATASTART	0x1005
#define USBHDFSD_IOCTL_GETCLUSTERSIZE	0x1006
#define USBHDFSD_IOCTL_CHECKCHAIN	0x1100	//polo: Ioctl request code => Check cluster chain
