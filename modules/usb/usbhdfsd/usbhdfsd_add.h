#ifndef _USBHDFSD_ADD_H
#define _USBHDFSD_ADD_H

//Extended IOCTL function codes
#define USBHDFSD_IOCTL_GETCLUSTER 0xBEEFC0DE        //jimmikaelkael: Ioctl request code => get file start cluster
#define USBHDFSD_IOCTL_GETDEVSECTORSIZE 0xDEADC0DE  //jimmikaelkael: Ioctl request code => get mass storage device sector size
#define USBHDFSD_IOCTL_CHECKCHAIN 0xCAFEC0DE  //polo: Ioctl request code => Check cluster chain

#endif //_USBHDFSD_ADD_H
