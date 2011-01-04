/*
  Copyright 2011, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

  ATA Driver for the HD Pro Kit, based on original ATAD form ps2sdk:	

  Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
  Licenced under Academic Free License version 2.0
  Review ps2sdk README & LICENSE files for further details.
 
  $Id: ps2atad.c 1455 2007-11-04 23:46:27Z roman_ps2dev $
  ATA device driver.
*/

#include <loadcore.h>
#include <intrman.h>
#include <thbase.h>
#include <thevent.h>
#include <stdio.h>
#include <sysclib.h>
#include <atahw.h>

#define MODNAME "atad_driver"
IRX_ID(MODNAME, 1, 1);

#define M_PRINTF(format, args...)	\
	printf(MODNAME ": " format, ## args)

#define BANNER "ATA device driver for HD Pro Kit %s\n"
#define VERSION "v1.0"


// HD Pro Kit is mapping the 1st word in ROM0 seg as a main ATA controller,
// The pseudo ATA controller registers are accessed (input/ouput) by writing
// an id to the main ATA controller (id specific to HDpro, see registers id below).
#define HDPROreg_IO8	      (*(volatile unsigned char *)0xBFC00000) 
#define HDPROreg_IO32	      (*(volatile unsigned int  *)0xBFC00000)

#define CDVDreg_STATUS        (*(volatile unsigned char *)0xBF40200A)

// Pseudo ATA controller registers id - Output
#define ATAreg_CONTROL_RD	0x68
#define ATAreg_SELECT_RD	0x70
#define ATAreg_STATUS_RD	0xf0
#define ATAreg_ERROR_RD		0x90
#define ATAreg_NSECTOR_RD	0x50
#define ATAreg_SECTOR_RD	0xd0
#define ATAreg_LCYL_RD 		0x30
#define ATAreg_HCYL_RD		0xb0
#define ATAreg_DATA_RD		0x41

// Pseudo ATA controller registers id - Input
#define ATAreg_CONTROL_WR	0x6a
#define ATAreg_SELECT_WR	0x72
#define ATAreg_COMMAND_WR	0xf2
#define ATAreg_FEATURE_WR	0x92
#define ATAreg_NSECTOR_WR	0x52
#define ATAreg_SECTOR_WR	0xd2
#define ATAreg_LCYL_WR 		0x32
#define ATAreg_HCYL_WR		0xb2
#define ATAreg_DATA_WR		0x12

// HD Pro uses PIO commands for reading/writing from HDD
#define ATA_C_READ_PIO		0x20
#define ATA_C_READ_PIO_EXT	0x24
#define ATA_C_WRITE_PIO		0x30
#define ATA_C_WRITE_PIO_EXT	0x34


typedef struct _ata_devinfo {
	int	exists;			/* Was successfully probed.  		*/
	int	has_packet;		/* Supports the PACKET command set.  	*/
	u32	total_sectors;		/* Total number of user sectors.  	*/
	u32	security_status;	/* Word 0x100 of the identify info.  	*/
} ata_devinfo_t;

static int ata_evflg = -1;

/* Used for indicating 48-bit LBA support.  */
static int lba_48bit[2] = {0, 0};

/* Local device info kept for drives 0 and 1.  */
static ata_devinfo_t atad_devinfo[2];

/* Data returned from DEVICE IDENTIFY is kept here.  Also, this is used by the
   security commands to set and unlock the password.  */
static u16 ata_param[256];

/* ATA command info.  */
typedef struct _ata_cmd_info {
	u8	command;
	u8	type;
} ata_cmd_info_t;

static ata_cmd_info_t ata_cmd_table[] = {
	{0,1},{3,1},{8,5},{0x20,2},{0x30,3},{0x32,8},{0x38,3},{0x40,1},{0x70,1},
	{0x87,2},{0x8e,7},{0x90,6},{0x91,1},{0x92,3},{0xa1,2},{0xb0,7},{0xc0,1},
	{0xc4,2},{0xc5,3},{0xc6,1},{0xc8,4},{0xca,4},{0xcd,3},{0xda,1},{0xde,1},
	{0xdf,1},{0xe0,1},{0xe1,1},{0xe2,1},{0xe3,1},{0xe4,2},{0xe5,1},{0xe6,1},
	{0xe7,1},{0xe8,3},{0xec,2},{0xed,1},{0xef,1},{0xf1,3},{0xf2,3},{0xf3,1},
	{0xf4,3},{0xf5,1},{0xf6,3},{0xf8,1},{0xf9,1},{0x25,4},{0x35,4},{0xea,1}
};
#define ATA_CMD_TABLE_SIZE	(sizeof ata_cmd_table/sizeof(ata_cmd_info_t))

static ata_cmd_info_t sec_ctrl_cmd_table[] = {
	{0xec,2},{0xf3,1},{0xf4,1},{0xf5,1},{0xf1,3},{0xf2,3},{0x30,3},{0x20,2}
};
#define SEC_CTRL_CMD_TABLE_SIZE	(sizeof sec_ctrl_cmd_table/sizeof(ata_cmd_info_t))

static ata_cmd_info_t smart_cmd_table[] = {
	{0xd0,2},{0xd2,1},{0xd3,1},{0xd4,1},{0xd5,2},{0xd6,3},{0xd8,1},{0xd9,1},
	{0xda,1}
};
#define SMART_CMD_TABLE_SIZE	(sizeof smart_cmd_table/sizeof(ata_cmd_info_t))

/* This is the state info tracked between ata_io_start() and ata_io_finish().  */
typedef struct _ata_cmd_state {
	int	type;		/* The ata_cmd_info_t type field. */
	void	*buf;
	u32	blkcount;	/* The number of 512-byte blocks (sectors) to transfer.  */
	int	dir;		/* DMA direction: 0 - to RAM, 1 - from RAM.  */
} ata_cmd_state_t;

static ata_cmd_state_t atad_cmd_state;

static int hdpro_io_active = 0;
static int intr_suspended = 0;
static int intr_state;

static int ata_init_devices(ata_devinfo_t *devinfo);
static int ata_wait_busy(int bits);
int ata_io_finish(void);
int ata_device_flush_cache(int device);

struct irx_export_table _exp_atad;

static u32 ata_alarm_cb(void *unused)
{
	iSetEventFlag(ata_evflg, 0x01);
	return 0;
}

static void suspend_intr(void)
{
	if (!intr_suspended) {
		CpuSuspendIntr(&intr_state);

		intr_suspended = 1;
	}
}

static void resume_intr(void)
{
	if (intr_suspended) {
		CpuResumeIntr(intr_state);

		intr_suspended = 0;
	}
}

static int hdpro_io_start(void)
{
	if (hdpro_io_active)
		return 0;

	hdpro_io_active = 0;

	suspend_intr();

	// HD Pro IO start commands sequence
	HDPROreg_IO8 = 0x72;
	CDVDreg_STATUS = 0;
	HDPROreg_IO8 = 0x34;
	CDVDreg_STATUS = 0;
	HDPROreg_IO8 = 0x61;
	CDVDreg_STATUS = 0;
	u32 res = HDPROreg_IO8;
	CDVDreg_STATUS = 0;

	resume_intr();

	// check result
	if ((res & 0xff) == 0xe7)
		hdpro_io_active = 1;

	return hdpro_io_active;
}

static int hdpro_io_finish(void)
{
	if (!hdpro_io_active)
		return 0;

	suspend_intr();

	// HD Pro IO finish commands sequence
	HDPROreg_IO8 = 0xf3;
	CDVDreg_STATUS = 0;

	resume_intr();

	DelayThread(200);

	if (HDPROreg_IO32 == 0x401a7800) // check the 1st in ROM0 seg get
		hdpro_io_active = 0;	 // back to it's original state

	return hdpro_io_active ^ 1;
}

static void hdpro_io_write(u8 cmd, u16 val)
{
	suspend_intr();

	// IO write to HD Pro
	HDPROreg_IO8 = cmd;
	CDVDreg_STATUS = 0;
	HDPROreg_IO8 = val;
	CDVDreg_STATUS = 0;
	HDPROreg_IO8 = (val & 0xffff) >> 8;
	CDVDreg_STATUS = 0;

	resume_intr();
}

static int hdpro_io_read(u8 cmd)
{
	suspend_intr();

	// IO read from HD Pro
	HDPROreg_IO8 = cmd;
	CDVDreg_STATUS = 0;
	u32 res0 = HDPROreg_IO8;
	CDVDreg_STATUS = 0;
	u32 res1 = HDPROreg_IO8;
	CDVDreg_STATUS = 0;
	res0 = (res0 & 0xff) | (res1 << 8);

	resume_intr();

	return res0 & 0xffff;
}

static int hdpro_io_init(void)
{
	suspend_intr();

	// HD Pro IO initialization commands sequence
	HDPROreg_IO8 = 0x13;
	CDVDreg_STATUS = 0;
	HDPROreg_IO8 = 0x00;
	CDVDreg_STATUS = 0;

	resume_intr();

	DelayThread(100);

	suspend_intr();

	HDPROreg_IO8 = 0x13;
	CDVDreg_STATUS = 0;
	HDPROreg_IO8 = 0x01;
	CDVDreg_STATUS = 0;

	resume_intr();

	DelayThread(3000);

	return ata_wait_busy(0x80);
}

int _start(int argc, char *argv[])
{
	int res = MODULE_NO_RESIDENT_END;
	iop_event_t event;

	printf(BANNER, VERSION);

	if ((res = RegisterLibraryEntries(&_exp_atad)) != 0) {
		M_PRINTF("Library is already registered, exiting.\n");
		goto out;
	}

	event.attr = 0;
	event.bits = 0;
	if ((ata_evflg = CreateEventFlag(&event)) < 0) {
		M_PRINTF("Couldn't create event flag, exiting.\n");
		goto out;
	}

	hdpro_io_start();

	HDPROreg_IO8 = 0xe3;
	CDVDreg_STATUS = 0;

	if (hdpro_io_init() != 0)
		goto out;

	if (ata_init_devices(atad_devinfo) != 0)
		goto out;

	res = MODULE_RESIDENT_END;
	M_PRINTF("Driver loaded.\n");

out:
	hdpro_io_finish();
	return res;
}

int shutdown(void)
{
	ata_device_flush_cache(0);
	hdpro_io_finish();

	return 0;
}

/* Export 8 */
int ata_get_error()
{
	return hdpro_io_read(ATAreg_ERROR_RD) & 0xff;
}

/* 0x80 for busy, 0x88 for bus busy.  */
static int ata_wait_busy(int bits)
{
	int i, didx, delay;
	int res = 0;

	for (i = 0; i < 80; i++) {

		hdpro_io_start();

		u16 r_control = hdpro_io_read(ATAreg_CONTROL_RD);

		hdpro_io_finish();

		if (!((r_control & 0xffff) & bits))
			goto out;

		didx = i / 10;
		switch (didx) {
			case 0:
				continue;
			case 1:
				delay = 100;
				break;
			case 2:
				delay = 1000;
				break;
			case 3:
				delay = 10000;
				break;
			case 4:
				delay = 100000;
				break;
			default:
				delay = 1000000;

		}

		DelayThread(delay);
	}

	res = -502;
	M_PRINTF("Timeout while waiting on busy (0x%02x).\n", bits);

out:
	hdpro_io_start();

	return res;
}

static int ata_device_select(int device)
{
	int res;

	if ((res = ata_wait_busy(0x88)) < 0)
		return res;

	/* If the device was already selected, nothing to do.  */
	if (((hdpro_io_read(ATAreg_SELECT_RD) >> 4) & 1) == device)
		return 0;

	/* Select the device.  */
	hdpro_io_write(ATAreg_SELECT_WR, (device & 1) << 4);
	res = hdpro_io_read(ATAreg_CONTROL_RD);

	return ata_wait_busy(0x88);
}

/* Export 6 */
int ata_io_start(void *buf, u32 blkcount, u16 feature, u16 nsector, u16 sector,
		u16 lcyl, u16 hcyl, u16 select, u16 command)
{
	iop_sys_clock_t cmd_timeout;
	ata_cmd_info_t *cmd_table;
	int i, res, type, cmd_table_size;
	int using_timeout, device = (select >> 4) & 1;
	u32 searchcmd;

	ClearEventFlag(ata_evflg, 0);

	if (!atad_devinfo[device & 1].exists)
		return -505;

	if ((res = ata_device_select(device & 1)) != 0)
		return res;

	/* For the SCE and SMART commands, we need to search on the subcommand
	specified in the feature register.  */
	if (command == ATA_C_SCE_SEC_CONTROL) {
		cmd_table = sec_ctrl_cmd_table;
		cmd_table_size = SEC_CTRL_CMD_TABLE_SIZE;
		searchcmd = feature;
	} else if (command == ATA_C_SMART) {
		cmd_table = smart_cmd_table;
		cmd_table_size = SMART_CMD_TABLE_SIZE;
		searchcmd = feature;
	} else {
		cmd_table = ata_cmd_table;
		cmd_table_size = ATA_CMD_TABLE_SIZE;
		searchcmd = command;
	}

	type = 0;
	for (i = 0; i < cmd_table_size; i++) {
		if ((searchcmd & 0xff) == cmd_table[i].command) {
			type = cmd_table[i].type;
			break;
		}
	}

	if (!(atad_cmd_state.type = type))
		return -506;

	atad_cmd_state.buf = buf;
	atad_cmd_state.blkcount = blkcount;

	/* Check that the device is ready if this the appropiate command.  */
	if (!(hdpro_io_read(ATAreg_CONTROL_RD) & 0x40)) {
		switch (command) {
			case 0x08:
			case 0x90:
			case 0x91:
			case 0xa0:
			case 0xa1:
				break;
			default:
				M_PRINTF("Error: Device %d is not ready.\n", device);
				return -501;
		}
	}

	/* Does this command need a timeout?  */
	using_timeout = 0;
	switch (type) {
		case 1:
		case 6:
			using_timeout = 1;
			break;
		case 4:
			/* Modified to include ATA_C_READ_DMA_EXT.  */
			atad_cmd_state.dir = ((command != 0xc8) && (command != 0x25));
			using_timeout = 1;
	}

	if (using_timeout) {
		cmd_timeout.lo = 0x41eb0000;
		cmd_timeout.hi = 0;

		if ((res = SetAlarm(&cmd_timeout, (void *)ata_alarm_cb, NULL)) < 0)
			return res;
	}

	/* Enable the command completion interrupt.  */
	suspend_intr();
	HDPROreg_IO8 = 0x21;
	CDVDreg_STATUS = 0;
	u32 dummy = HDPROreg_IO8;
	CDVDreg_STATUS = 0;
	resume_intr();
	dummy = 0;

	/* Finally!  We send off the ATA command with arguments.  */
	hdpro_io_write(ATAreg_CONTROL_WR, (using_timeout == 0) << 1);

	/* 48-bit LBA requires writing to the address registers twice,
	   24 bits of the LBA address is written each time.
	   Writing to registers twice does not affect 28-bit LBA since
	   only the latest data stored in address registers is used.  */
	hdpro_io_write(ATAreg_FEATURE_WR, (feature & 0xffff) >> 8);
	hdpro_io_write(ATAreg_NSECTOR_WR, (nsector & 0xffff) >> 8);
	hdpro_io_write(ATAreg_SECTOR_WR, (sector & 0xffff) >> 8);
	hdpro_io_write(ATAreg_LCYL_WR, (lcyl & 0xffff) >> 8);
	hdpro_io_write(ATAreg_HCYL_WR, (hcyl & 0xffff) >> 8);

	hdpro_io_write(ATAreg_FEATURE_WR, feature & 0xff);
	hdpro_io_write(ATAreg_NSECTOR_WR, nsector & 0xff);
	hdpro_io_write(ATAreg_SECTOR_WR, sector & 0xff);
	hdpro_io_write(ATAreg_LCYL_WR, lcyl & 0xff);
	hdpro_io_write(ATAreg_HCYL_WR, hcyl & 0xff);

	hdpro_io_write(ATAreg_SELECT_WR, select & 0xff);
	hdpro_io_write(ATAreg_COMMAND_WR, command & 0xff);

	return 0;
}

/* Do a PIO transfer, to or from the device.  */
static int ata_pio_transfer(ata_cmd_state_t *cmd_state)
{
	void *buf;
	int i, type;
	int res = 0, chk = 0;
	u16 status = hdpro_io_read(ATAreg_STATUS_RD);

	if (status & ATA_STAT_ERR) {
		M_PRINTF("Error: Command error: status 0x%02x, error 0x%02x.\n",
				status, ata_get_error());
		return -503;
	}

	/* DRQ must be set (data request).  */
	if (!(status & ATA_STAT_DRQ))
		return -504;

	type = cmd_state->type;

	if (type == 3 || type == 8) {
		/* PIO data out */
		buf = cmd_state->buf;

		HDPROreg_IO8 = 0x43;
		CDVDreg_STATUS = 0;

		for (i = 0; i < 256; i++) {
			u16 r_data = *(u16 *)buf;
			hdpro_io_write(ATAreg_DATA_WR, r_data);
			chk ^= r_data + i;
			cmd_state->buf = ++((u16 *)buf);
		}

		u16 out = hdpro_io_read(ATAreg_DATA_RD) & 0xffff;
		if (out != (chk & 0xffff))
			return -504;

		if (cmd_state->type == 8) {
			for (i = 0; i < 4; i++) {
				hdpro_io_write(ATAreg_DATA_WR, *(u8 *)buf);
				cmd_state->buf = ++((u8 *)buf);
			}
		}

	} else if (type == 2) {
		/* PIO data in  */
		buf = cmd_state->buf;

		suspend_intr();

		HDPROreg_IO8 = 0x53;
		CDVDreg_STATUS = 0;
		CDVDreg_STATUS = 0;

		for (i = 0; i < 256; i++) {

			u32 res0 = HDPROreg_IO8;
			CDVDreg_STATUS = 0;
			u32 res1 = HDPROreg_IO8;
			CDVDreg_STATUS = 0;

			res0 = (res0 & 0xff) | (res1 << 8);
			chk ^= res0 + i;

			*(u16 *)buf = res0 & 0xffff;
			cmd_state->buf = ++((u16 *)buf);
		}

		HDPROreg_IO8 = 0x51;
		CDVDreg_STATUS = 0;
		CDVDreg_STATUS = 0;

		resume_intr();

		u16 r_data = hdpro_io_read(ATAreg_DATA_RD) & 0xffff;
		if (r_data != (chk & 0xffff))
			return -504;
	}

	return res;
}

/* Export 5 */
/* Not sure if it's reset, but it disables ATA interrupts.  */
int ata_reset_devices()
{
	if (hdpro_io_read(ATAreg_CONTROL_RD) & 0x80)
		return -501;

	/* Dunno what this does.  */
	hdpro_io_write(ATAreg_CONTROL_WR, 6);
	DelayThread(100);

	/* Disable ATA interrupts.  */
	hdpro_io_write(ATAreg_CONTROL_WR, 2);
	DelayThread(3000);

	return ata_wait_busy(0x80);
}

static void ata_device_probe(ata_devinfo_t *devinfo)
{
	u16 nsector, sector, lcyl, hcyl, select;

	devinfo->exists = 0;
	devinfo->has_packet = 0;

	if (hdpro_io_read(ATAreg_CONTROL_RD) & 0x88)
		return;

	nsector = hdpro_io_read(ATAreg_NSECTOR_RD) & 0xff;
	sector = hdpro_io_read(ATAreg_SECTOR_RD) & 0xff;
	lcyl = hdpro_io_read(ATAreg_LCYL_RD) & 0xff;
	hcyl = hdpro_io_read(ATAreg_HCYL_RD) & 0xff;
	select = hdpro_io_read(ATAreg_SELECT_RD) & 0xff;

	if (nsector != 1)
		return;

	devinfo->exists = 1;

	if ((lcyl == 0x14) && (hcyl == 0xeb))
		devinfo->has_packet = 1;
}

/* Export 17 */
int ata_device_flush_cache(int device)
{
	int res;

	if (!hdpro_io_start())
		return -1;

	if (lba_48bit[device]) {
		res = ata_io_start(NULL, 1, 0, 0, 0, 0, 0, (device << 4) & 0xffff,
				ATA_C_FLUSH_CACHE_EXT);
	} else {
		res = ata_io_start(NULL, 1, 0, 0, 0, 0, 0, (device << 4) & 0xffff,
				ATA_C_FLUSH_CACHE);
	}
	if (!res)
		res = ata_io_finish();

	if (!hdpro_io_finish())
		return -2;

	return res;
}

/* Export 13 */
int ata_device_idle(int device, int period)
{
	int res;

	res = ata_io_start(NULL, 1, 0, period & 0xff, 0, 0, 0,
			(device << 4) & 0xffff, ATA_C_IDLE);
	if (res)
		return res;

	return ata_io_finish();
}

/* Set features - set transfer mode.  */
int ata_device_set_transfer_mode(int device, int type, int mode)
{
	int res;

	res = ata_io_start(NULL, 1, 3, (type|mode) & 0xff, 0, 0, 0,
			(device << 4) & 0xffff, ATA_C_SET_FEATURES);
	if (res)
		return res;

	res = ata_io_finish();
	if (res)
		return res;

	return 0;
}

static int ata_device_identify(int device, void *info)
{
	int res;

	res = ata_io_start(info, 1, 0, 0, 0, 0, 0, (device << 4) & 0xffff,
			ATA_C_IDENTIFY_DEVICE);
	if (res)
		return res;

	return ata_io_finish();
}

static int ata_init_devices(ata_devinfo_t *devinfo)
{
	int i, res;

	ata_reset_devices();

	ata_device_probe(&devinfo[0]);
	if (!devinfo[0].exists) {
		M_PRINTF("Error: Unable to detect HDD 0.\n");
		devinfo[1].exists = 0;
		return 0;
	}

	/* If there is a device 1, grab it's info too.  */
	if ((res = ata_device_select(1)) != 0)
		return res;
	if (hdpro_io_read(ATAreg_CONTROL_RD) & 0xff)
		ata_device_probe(&devinfo[1]);
	else
		devinfo[1].exists = 0;

	for (i = 0; i < 2; i++) {
		if (!devinfo[i].exists)
			continue;

		/* Send the IDENTIFY DEVICE command. if it doesn't succeed
		   devinfo is disabled.  */
		if (!devinfo[i].has_packet) {
			res = ata_device_identify(i, ata_param);
			devinfo[i].exists = (res == 0);
		}

		/* This next section is HDD-specific: if no device or it's a
		   packet (ATAPI) device, we're done.  */
		if (!devinfo[i].exists || devinfo[i].has_packet)
			continue;

		/* This section is to detect whether the HDD supports 48-bit LBA 
		   (IDENITFY DEVICE bit 10 word 83) and get the total sectors from
		   either words(61:60) for 28-bit or words(103:100) for 48-bit.  */
		if (ata_param[ATA_ID_COMMAND_SETS_SUPPORTED] & 0x0400) {
			lba_48bit[i] = 1;
			/* I don't think anyone would use a >2TB HDD but just in case.  */
			if (ata_param[ATA_ID_48BIT_SECTOTAL_HI]) {
				devinfo[i].total_sectors = 0xffffffff;
			} else {
				devinfo[i].total_sectors = 
					(ata_param[ATA_ID_48BIT_SECTOTAL_MI] << 16)|
					ata_param[ATA_ID_48BIT_SECTOTAL_LO];
			}
		} else {
			lba_48bit[i] = 0;
			devinfo[i].total_sectors = (ata_param[ATA_ID_SECTOTAL_HI] << 16)|
				ata_param[ATA_ID_SECTOTAL_LO];
		}
		devinfo[i].security_status = ata_param[ATA_ID_SECURITY_STATUS];

		ata_device_set_transfer_mode(i, 8, 0); /* PIO (flow control).  */
		ata_device_idle(i, 0);
	}
	return 0;
}

/* Export 7 */
int ata_io_finish(void)
{
	ata_cmd_state_t *cmd_state = &atad_cmd_state;
	u32 bits;
	int res = 0, type = cmd_state->type;
	u16 stat;

	if (type == 1 || type == 6) {	/* Non-data commands.  */

retry:
		suspend_intr();

		HDPROreg_IO8 = 0x21;
		CDVDreg_STATUS = 0;
		u32 ret = HDPROreg_IO8;
		CDVDreg_STATUS = 0;

		resume_intr();

		if (((ret & 0xff) & 1) == 0) {

			WaitEventFlag(ata_evflg, 0x03, 0x11, &bits);
			if (bits & 0x01) {	/* Timeout.  */
				M_PRINTF("Error: ATA timeout on a non-data command.\n");
				return -502;
			}

			DelayThread(500);
			goto retry;
		}

	} else if (type == 4) {		/* DMA.  */
			M_PRINTF("Error: DMA mode not implemented.\n");
			res = -502;
	} else {			/* PIO transfers.  */
		stat = hdpro_io_read(ATAreg_CONTROL_RD);
		if ((res = ata_wait_busy(0x80)) < 0)
			goto finish;

		/* Transfer each PIO data block.  */
		while (--cmd_state->blkcount != -1) {
			if ((res = ata_pio_transfer(cmd_state)) < 0)
				goto finish;
			if ((res = ata_wait_busy(0x80)) < 0)
				goto finish;
		}
	}

	if (res)
		goto finish;

	/* Wait until the device isn't busy.  */
	if (hdpro_io_read(ATAreg_STATUS_RD) & ATA_STAT_BUSY)
		res = ata_wait_busy(0x80);
	if ((stat = hdpro_io_read(ATAreg_STATUS_RD)) & ATA_STAT_ERR) {
		M_PRINTF("Error: Command error: status 0x%02x, error 0x%02x.\n",
				stat, ata_get_error());
		res = -503;
	}

finish:
	/* The command has completed (with an error or not), so clean things up.  */
	CancelAlarm((void *)ata_alarm_cb, NULL);

	return res;
}

/* Export 9 */
int ata_device_dma_transfer(int device, void *buf, u32 lba, u32 nsectors, int dir)
{
	int res = 0;
	u32 nbytes;
	u16 sector, lcyl, hcyl, select, command, len;

	if (!hdpro_io_start())
		return -1;

	while (nsectors) {
		len = (nsectors > 256) ? 256 : nsectors;

		/* Variable lba is only 32 bits so no change for lcyl and hcyl.  */
		lcyl = (lba >> 8) & 0xff;
		hcyl = (lba >> 16) & 0xff;

		if (lba_48bit[device]) {
			/* Setup for 48-bit LBA.  */
			/* Combine bits 24-31 and bits 0-7 of lba into sector.  */
			sector = ((lba >> 16) & 0xff00) | (lba & 0xff);
			/* 0x40 enables LBA.  */
			select = ((device << 4) | 0x40) & 0xffff;
			command = (dir == 1) ? ATA_C_WRITE_PIO_EXT : ATA_C_READ_PIO_EXT;
		} else {
			/* Setup for 28-bit LBA.  */
			sector = lba & 0xff;
			/* 0x40 enables LBA.  */
			select = ((device << 4) | ((lba >> 24) & 0xf) | 0x40) & 0xffff;
			command = (dir == 1) ? ATA_C_WRITE_PIO : ATA_C_READ_PIO;
		}

		if ((res = ata_io_start(buf, len, 0, len, sector, lcyl,
					hcyl, select, command)) != 0)
			return res;
		if ((res = ata_io_finish()) != 0)
			return res;

		nbytes = len * 512;
		(u8 *)buf += nbytes;
		lba += len;
		nsectors -= len;
	}

	if (!hdpro_io_finish())
		return -2;

	return res;
}

/* Export 4 */
ata_devinfo_t * ata_get_devinfo(int device)
{
	return &atad_devinfo[device];
}

/* Export 19 */
int ata_device_is_48bit(int device)
{
	return lba_48bit[device];
}

