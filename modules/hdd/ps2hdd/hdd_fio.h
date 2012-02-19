/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: hdd_fio.h 1511 2009-01-15 09:11:30Z radad $
*/

#ifndef _HDD_FIO_H
#define _HDD_FIO_H


//
// IOCTL2 commands
//
#define APA_IOCTL2_ADD_SUB				0x00006801
#define APA_IOCTL2_DELETE_LAST_SUB		0x00006802
#define APA_IOCTL2_NUMBER_OF_SUBS		0x00006803
#define APA_IOCTL2_FLUSH_CACHE			0x00006804

#define APA_IOCTL2_TRANSFER_DATA		0x00006832	// used by pfs to read/write data :P
#define APA_IOCTL2_GETSIZE				0x00006833	// for main(0)/subs(1+)
#define APA_IOCTL2_SET_PART_ERROR		0x00006834	// set(sector of a apa header) that has a error :)
#define APA_IOCTL2_GET_PART_ERROR		0x00006835	// get(sector of a apa header) that has a error
#define APA_IOCTL2_GETHEADER			0x00006836	// get apa header

// structs for IOCTL2 commands
typedef struct
{
	u32		sub;		// main(0)/subs(1+) to read/write
	u32		sector;
	u32		size;		// in sectors
	u32		mode;		// ATAD_MODE_READ/ATAD_MODE_WRITE.....
	void	*buffer;
} hddIoctl2Transfer_t;

//
// DEVCTL commands
//
#define APA_DEVCTL_MAX_SECTORS		0x00004801	// max partition size(in sectors)
#define APA_DEVCTL_TOTAL_SECTORS	0x00004802
#define APA_DEVCTL_IDLE				0x00004803
#define APA_DEVCTL_FLUSH_CACHE		0x00004804
#define APA_DEVCTL_SWAP_TMP			0x00004805
#define APA_DEVCTL_DEV9_SHUTDOWN	0x00004806
#define APA_DEVCTL_STATUS			0x00004807
#define APA_DEVCTL_FORMAT			0x00004808
#define APA_DEVCTL_SMART_STAT		0x00004809
//#define APA_DEVCTL_FREE_SECTORS	0x0000480A// REMOVED! is not true free...

#define APA_DEVCTL_GETTIME				0x00006832 
#define APA_DEVCTL_SET_OSDMBR			0x00006833// arg = hddSetOsdMBR_t
#define APA_DEVCTL_GET_SECTOR_ERROR		0x00006834
#define APA_DEVCTL_GET_ERROR_PART_NAME	0x00006835// bufp = namebuffer[0x20]
#define APA_DEVCTL_ATA_READ				0x00006836// arg  = hddAtaTransfer_t 
#define APA_DEVCTL_ATA_WRITE			0x00006837// arg  = hddAtaTransfer_t 
#define APA_DEVCTL_SCE_IDENTIFY_DRIVE	0x00006838// bufp = buffer for atadSceIdentifyDrive 
//#define APA_DEVCTL_FREE_SECTORS2		0x00006839 
#define APA_DEVCTL_IS_48BIT				0x00006840
#define APA_DEVCTL_SET_TRANSFER_MODE	0x00006841
#define APA_DEVCTL_ATA_IOP_WRITE	0x00006842

// structs for DEVCTL commands

typedef struct
{
	u32 lba;
	u32 size;
	u8 data[0];
} hddAtaTransfer_t; 

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

typedef struct
{
	u32 start;
	u32 size;
} hddSetOsdMBR_t;


///////////////////////////////////////////////////////////////////////////////
//   Function declerations
int fioPartitionSizeLookUp(char *str);
int fioInputBreaker(char const **arg, char *outBuf, int maxout);
int fioDataTransfer(iop_file_t *f, void *buf, int size, int mode);
int getFileSlot(input_param *params, hdd_file_slot_t **fileSlot);

int ioctl2Transfer(u32 device, hdd_file_slot_t *fileSlot, hddIoctl2Transfer_t *arg);
void fioGetStatFiller(apa_cache *clink1, iox_stat_t *stat);
int ioctl2AddSub(hdd_file_slot_t *fileSlot, char *argp);
int ioctl2DeleteLastSub(hdd_file_slot_t *fileSlot);

int devctlSwapTemp(u32 device, char *argp);

// Functions which hook into IOMAN
int hddInit(iop_device_t *f);
int hddDeinit(iop_device_t *f);
int hddFormat(iop_file_t *f, const char *dev, const char *blockdev, void *arg, size_t arglen);
int hddOpen(iop_file_t *f, const char *name, int mode, int other_mode);
int hddClose(iop_file_t *f);
int hddRead(iop_file_t *f, void *buf, int size);
int hddWrite(iop_file_t *f, void *buf, int size);
int hddLseek(iop_file_t *f, unsigned long post, int whence);
int hddIoctl2(iop_file_t *f, int request, void *argp, unsigned int arglen, void *bufp, unsigned intbuflen);
int hddRemove(iop_file_t *f, const char *name);
int hddDopen(iop_file_t *f, const char *name);
int hddDread(iop_file_t *f, iox_dirent_t *dirent);
int hddGetStat(iop_file_t *f, const char *name, iox_stat_t *stat);
int hddReName(iop_file_t *f, const char *oldname, const char *newname);
int hddDevctl(iop_file_t *f, const char *devname, int cmd, void *arg, unsigned int arglen, void *bufp, unsigned int buflen);

int hddUnsupported(iop_file_t *f);

#endif /* _HDD_FIO_H */
