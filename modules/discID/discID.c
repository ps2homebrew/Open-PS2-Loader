/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <sifman.h>
#include <intrman.h>
#include <sysclib.h>
#include <ioman.h>
#include <io_common.h>
#include <thsemap.h>
#include <errno.h>

#define MODNAME "discID"
IRX_ID(MODNAME, 1, 1);

#define CDVDreg_30		      (*(volatile unsigned char *)0xBF402030)
#define CDVDreg_31		      (*(volatile unsigned char *)0xBF402031)
#define CDVDreg_32		      (*(volatile unsigned char *)0xBF402032)
#define CDVDreg_33		      (*(volatile unsigned char *)0xBF402033)
#define CDVDreg_34		      (*(volatile unsigned char *)0xBF402034)
#define CDVDreg_KEYSTATE      (*(volatile unsigned char *)0xBF402038)
#define CDVDreg_KEYXOR        (*(volatile unsigned char *)0xBF402039)

typedef struct {
	u8 trycount; 		
	u8 spindlctrl; 		
	u8 datapattern; 	
	u8 pad; 			
} cd_read_mode_t;

// cdvdman imports
int sceCdInit(int init_mode);												// #4
int sceCdStandby(void);														// #5
int sceCdSync(int mode); 													// #11
int sceCdGetDiskType(void);													// #12
int sceCdDiskReady(int mode); 												// #13
int sceCdStop(void); 														// #15
int sceCdReadKey(int arg1, int arg2, u32 lsn, char *key);					// #35

// driver ops protypes
int discID_dummy(void);
int discID_init(iop_device_t *dev);
int discID_deinit(iop_device_t *dev);
int discID_open(iop_file_t *f, char *filename, int mode);
int discID_read(iop_file_t *f, void *buf, u32 size);
int discID_close(iop_file_t *f);

// driver ops func tab
void *discID_ops[17] = {
	(void*)discID_init,
	(void*)discID_deinit,
	(void*)discID_dummy,
	(void*)discID_open,
	(void*)discID_close,
	(void*)discID_read,
	(void*)discID_dummy,
	(void*)discID_dummy,
	(void*)discID_dummy,
	(void*)discID_dummy,
	(void*)discID_dummy,
	(void*)discID_dummy,
	(void*)discID_dummy,
	(void*)discID_dummy,
	(void*)discID_dummy,
	(void*)discID_dummy,
};

// driver descriptor
static iop_device_t discID_dev = {
	"discID", 
	IOP_DT_FS,
	1,
	"discID",
	(struct _iop_device_ops *)&discID_ops
};

iop_file_t discID_fhandle;
static int discID_io_sema = -1;

//-------------------------------------------------------------- 
int discID_dummy(void)
{
	return -EPERM;
}

//-------------------------------------------------------------- 
int discID_init(iop_device_t *dev)
{
	iop_sema_t smp;

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 1;
	smp.option = 0;

	discID_io_sema = CreateSema(&smp);
		
	return 0;
}

//-------------------------------------------------------------- 
int discID_deinit(iop_device_t *dev)
{
	DeleteSema(discID_io_sema);
	
	return 0;
}

//-------------------------------------------------------------- 
int discID_open(iop_file_t *f, char *filename, int mode)
{
	register int r = 0;
	iop_file_t *fh = &discID_fhandle;

	if (!filename)
		return -ENOENT;

	WaitSema(discID_io_sema);

	if (!fh->privdata) {
		f->privdata = fh;
		memcpy(fh, f, sizeof(iop_file_t));

		sceCdInit(0);
		sceCdDiskReady(0);
		sceCdStandby();
		sceCdSync(0);

		r = 0;
	}
	else
		r = -EMFILE;

	SignalSema(discID_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int discID_close(iop_file_t *f)
{
	iop_file_t *fh = (iop_file_t *)f->privdata;

	WaitSema(discID_io_sema);

	if (fh)
		memset(fh, 0, sizeof(iop_file_t));

	sceCdStop();
	sceCdSync(0);

	SignalSema(discID_io_sema);

	return 0;
}

//-------------------------------------------------------------- 
int discID_read(iop_file_t *f, void *buf, u32 size)
{
	register int r = 0;
	u8 key[16];
	u8 discID[16];

	WaitSema(discID_io_sema);

	memset(key, 0, 16);
	memset(discID, 0, 16);

	r = sceCdGetDiskType();
	if ((r >= 0x12) && (r < 0x15)) { // check it's PS2 Disc

		sceCdReadKey(0, 0, 0x4b, key);
		sceCdSync(0);

		if (CDVDreg_KEYSTATE & 4) {
			discID[0] = CDVDreg_30;
			discID[0] ^= CDVDreg_KEYXOR;
			discID[1] = CDVDreg_31;
			discID[1] ^= CDVDreg_KEYXOR;
			discID[2] = CDVDreg_32;
			discID[2] ^= CDVDreg_KEYXOR;
			discID[3] = CDVDreg_33;
			discID[3] ^= CDVDreg_KEYXOR;
			discID[4] = CDVDreg_34;
			discID[4] ^= CDVDreg_KEYXOR;

			memcpy(buf, discID, 5);

			r = 5;
		}
		else
			r = -EIO;
	}
	else
		r = -ENODEV;

	SignalSema(discID_io_sema);

	return r;
}

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{	
	DelDrv("discID");
	AddDrv((iop_device_t *)&discID_dev);

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
DECLARE_IMPORT_TABLE(cdvdman, 1, 1)
DECLARE_IMPORT(4, sceCdInit)
DECLARE_IMPORT(5, sceCdStandby)
DECLARE_IMPORT(11, sceCdSync)
DECLARE_IMPORT(12, sceCdGetDiskType)
DECLARE_IMPORT(13, sceCdDiskReady)
DECLARE_IMPORT(15, sceCdStop)
DECLARE_IMPORT(35, sceCdReadKey)
END_IMPORT_TABLE

