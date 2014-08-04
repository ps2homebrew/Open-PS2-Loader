/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
# ATA device driver.
# This module provides the low-level ATA support for hard disk drives.  It is
# 100% compatible with its proprietary counterpart called atad.irx.
#
# This module also include support for 48-bit feature set (done by Clement).
*/

#include <types.h>
#include <defs.h>
#include <irx.h>
#include <loadcore.h>
#include <thbase.h>
#include <thevent.h>
#include <stdio.h>
#include <sysclib.h>
#include <dev9.h>
#include "atad.h"

#include <speedregs.h>
#include <atahw.h>

#define MODNAME "atad"
IRX_ID(MODNAME, 2, 4);

#define M_PRINTF(format, args...)	\
	printf(MODNAME ": " format, ## args)

#define BANNER "ATA device driver %s - Copyright (c) 2003 Marcus R. Brown\n"
#define VERSION "v1.2"

static int ata_devinfo_init = 0;
static int ata_evflg = -1;

/* Used for indicating 48-bit LBA support.  */
static unsigned char lba_48bit[2] = {0, 0};

static unsigned char Disable48bitLBA = 0;	//Please read the comments in _start().

/* Local device info kept for drives 0 and 1.  */
static ata_devinfo_t atad_devinfo[2];

/* Data returned from DEVICE IDENTIFY is kept here.  Also, this is used by the
   security commands to set and unlock the password.  */
static unsigned short int ata_param[256];

/* ATA command info.  */
typedef struct _ata_cmd_info {
	unsigned char command;
	unsigned char type;
} ata_cmd_info_t;

static const ata_cmd_info_t ata_cmd_table[] = {
	{ATA_C_NOP,1},
	{ATA_C_CFA_REQUEST_EXTENDED_ERROR_CODE,1},
	{ATA_C_DEVICE_RESET,5},
	{ATA_C_READ_SECTOR,2},
	{ATA_C_READ_DMA_EXT,0x84},
	{ATA_C_WRITE_SECTOR,3},
	{ATA_C_WRITE_LONG,8},	//??? This seems to be WRITE_LONG, but the READ_LONG command isn't present (Why would Sony have only one of these two commands?). Both are obsolete too.
	{ATA_C_WRITE_DMA_EXT,0x84},
	{ATA_C_CFA_WRITE_SECTORS_WITHOUT_ERASE,3},
	{ATA_C_READ_VERIFY_SECTOR,1},
	{ATA_C_READ_VERIFY_SECTOR_EXT,0x81},
	{ATA_C_SEEK,1},
	{ATA_C_CFA_TRANSLATE_SECTOR,2},
	{ATA_C_SCE_SECURITY_CONTROL,7},
	{ATA_C_EXECUTE_DEVICE_DIAGNOSTIC,6},
	{ATA_C_INITIALIZE_DEVICE_PARAMETERS,1},
	{ATA_C_DOWNLOAD_MICROCODE,3},
	{ATA_C_IDENTIFY_PACKET_DEVICE,2},
	{ATA_C_SMART,7},
	{ATA_C_CFA_ERASE_SECTORS,1},
	{ATA_C_READ_MULTIPLE,2},
	{ATA_C_WRITE_MULTIPLE,3},
	{ATA_C_SET_MULTIPLE_MODE,1},
	{ATA_C_READ_DMA,4},
	{ATA_C_WRITE_DMA,4},
	{ATA_C_CFA_WRITE_MULTIPLE_WITHOUT_ERASE,3},
	{ATA_C_GET_MEDIA_STATUS,1},
	{ATA_C_MEDIA_LOCK,1},
	{ATA_C_MEDIA_UNLOCK,1},
	{ATA_C_STANDBY_IMMEDIATE,1},
	{ATA_C_IDLE_IMMEDIATE,1},
	{ATA_C_STANDBY,1},
	{ATA_C_IDLE,1},
	{ATA_C_READ_BUFFER,2},
	{ATA_C_CHECK_POWER_MODE,1},
	{ATA_C_SLEEP,1},
	{ATA_C_FLUSH_CACHE,1},
	{ATA_C_WRITE_BUFFER,3},
	{ATA_C_FLUSH_CACHE_EXT,1},
	{ATA_C_IDENTIFY_DEVICE,2},
	{ATA_C_MEDIA_EJECT,1},
	{ATA_C_SET_FEATURES,1},
	{ATA_C_SECURITY_SET_PASSWORD,3},
	{ATA_C_SECURITY_UNLOCK,3},
	{ATA_C_SECURITY_ERASE_PREPARE,1},
	{ATA_C_SECURITY_ERASE_UNIT,3},
	{ATA_C_SECURITY_FREEZE_LOCK,1},
	{ATA_C_SECURITY_DISABLE_PASSWORD,3},
	{ATA_C_READ_NATIVE_MAX_ADDRESS,1},
	{ATA_C_SET_MAX_ADDRESS,1}
};
#define ATA_CMD_TABLE_SIZE	(sizeof ata_cmd_table/sizeof(ata_cmd_info_t))

static const ata_cmd_info_t sec_ctrl_cmd_table[] = {
	{ATA_SCE_IDENTIFY_DRIVE,2},
	{ATA_SCE_SECURITY_ERASE_PREPARE,1},
	{ATA_SCE_SECURITY_ERASE_UNIT,1},
	{ATA_SCE_SECURITY_FREEZE_LOCK,1},
	{ATA_SCE_SECURITY_SET_PASSWORD,3},
	{ATA_SCE_SECURITY_UNLOCK,3},
	{ATA_SCE_SECURITY_WRITE_ID,3},
	{ATA_SCE_SECURITY_READ_ID,2}
};
#define SEC_CTRL_CMD_TABLE_SIZE	(sizeof sec_ctrl_cmd_table/sizeof(ata_cmd_info_t))

static const ata_cmd_info_t smart_cmd_table[] = {
	{ATA_S_SMART_READ_DATA,2},
	{ATA_S_SMART_ENABLE_DISABLE_AUTOSAVE,1},
	{ATA_S_SMART_SAVE_ATTRIBUTE_VALUES,1},
	{ATA_S_SMART_EXECUTE_OFF_LINE,1},
	{ATA_S_SMART_READ_LOG,2},
	{ATA_S_SMART_WRITE_LOG,3},
	{ATA_S_SMART_ENABLE_OPERATIONS,1},
	{ATA_S_SMART_DISABLE_OPERATIONS,1},
	{ATA_S_SMART_RETURN_STATUS,1}
};
#define SMART_CMD_TABLE_SIZE	(sizeof smart_cmd_table/sizeof(ata_cmd_info_t))

/* This is the state info tracked between ata_io_start() and ata_io_finish().  */
typedef struct _ata_cmd_state {
	int	type;		/* The ata_cmd_info_t type field. */
	void	*buf;
	unsigned int	blkcount;	/* The number of 512-byte blocks (sectors) to transfer.  */
	int	dir;		/* DMA direction: 0 - to RAM, 1 - from RAM.  */
} ata_cmd_state_t;

static ata_cmd_state_t atad_cmd_state;

static int ata_intr_cb(int flag);
static unsigned int ata_alarm_cb(void *unused);

static void ata_set_dir(int dir);

static void ata_pio_mode(int mode);
static void ata_multiword_dma_mode(int mode);
static void ata_ultra_dma_mode(int mode);

struct irx_export_table _exp_atad;

/*	In a modern ATAD.IRX module, the DMA ENabled bit should be set and unset from the pre and post DMA callbacks.
	However, some of the clone adaptors don't support this properly. Since some users out there cannot tell the difference
	between such a clone adaptor and a genuine Sony adaptor, it's probably just best to go with the design of the older ATAD modules.

	Older ATAD modules have this bit set within ata_set_dir() instead.	*/
/* static void AtadPreDmaCb(int bcr, int dir){
	USE_SPD_REGS;

	SPD_REG16(SPD_R_XFR_CTRL)|=0x80;
}

static void AtadPostDmaCb(int bcr, int dir){
	USE_SPD_REGS;

	SPD_REG16(SPD_R_XFR_CTRL)&=~0x80;
} */

int _start(int argc, char *argv[])
{
	USE_SPD_REGS;
	iop_event_t event;
	int res = 1;

	printf(BANNER, VERSION);

	if (!(SPD_REG16(SPD_R_REV_3) & SPD_CAPS_ATA) || !(SPD_REG16(SPD_R_REV_8) & 0x02)) {
		M_PRINTF("HDD is not connected, exiting.\n");
		goto out;
	}

	/*
		The PSX (Not the PlayStation or PSOne, but a PS2 with a DVR unit) has got an extra processor (Fujitsu MB91302A, aka the "DVRP") that seems to be emulating the console's PS2 ATA interface.
		The stock disks of all PSX units are definitely 48-bit LBA compliant because of their capacities, but the DVRP's emulation seems to have a design problem:
			1. It indicates that 48-bit LBA is supported.
			2. The 48-bit LBA capacity fields show the true capacity of the disk.
			3. Accesses to beyond the 28-bit LBA capacity (Which is 40.000GB by default) will result in I/O errors.
			4. (For some PSX units like the DESR-7500): Double-writes to the ATA registers seem to cause the ATA interface to stall.

		The problem is obviously in the DVRP's firmware, but we currently have no way to fix these bugs because the DVRP is even more heavily secured that the IOP.
		In the eyes of Sony, there isn't a problem because none of their retail PlayStation 2 software ever supported 48-bit LBA.

		The obvious workaround here would be to totally kill 48-bit LBA support when ATAD is loaded on a PSX.
	*/
	Disable48bitLBA = ((SPD_REG16(SPD_R_REV_3) & SPD_CAPS_DVR) && (SPD_REG16(SPD_R_REV_1) != 0xFF))?1:0;	//The check for revision 0xFF is to workaround the problem that the Chinese SATA network adaptor has: it reports 0xFF for a lot of fields (including the capabilities field), which unfortunately triggers off the workaround for the PSX. It reports 0xFF as its revision too, which can be used to identify it.

	if ((res = RegisterLibraryEntries(&_exp_atad)) != 0) {
		M_PRINTF("Library is already registered, exiting.\n");
		goto out;
	}

	event.attr = 0;
	event.bits = 0;
	if ((ata_evflg = CreateEventFlag(&event)) < 0) {
		M_PRINTF("Couldn't create event flag, exiting.\n");
		res = 1;
		goto out;
	}

	dev9RegisterIntrCb(1, &ata_intr_cb);
	dev9RegisterIntrCb(0, &ata_intr_cb);
	//Read the comment above about these callbacks.
/*	dev9RegisterPreDmaCb(0, &AtadPreDmaCb);
	dev9RegisterPostDmaCb(0, &AtadPostDmaCb);	*/

	res = 0;
	M_PRINTF("Driver loaded.\n");
out:
	return res;
}

int _exit(void) { return 0; }

static int ata_intr_cb(int flag)
{
	if (flag == 1) {	/* New card, invalidate device info.  */
		memset(atad_devinfo, 0, sizeof atad_devinfo);
	} else {
		dev9IntrDisable(SPD_INTR_ATA);
		iSetEventFlag(ata_evflg, 0x02);
	}

	return 1;
}

static unsigned int ata_alarm_cb(void *unused)
{
	iSetEventFlag(ata_evflg, 0x01);
	return 0;
}

/* Export 8 */
int ata_get_error(void)
{
	USE_ATA_REGS;
	return ata_hwport->r_error & 0xff;
}

/* 0x80 for busy, 0x88 for bus busy.  */
static int ata_wait_busy(int bits)
{
	USE_ATA_REGS;
	int i, didx, delay;

	for (i = 0; i < 80; i++) {
		if (!(ata_hwport->r_control & bits))
			return 0;

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

	M_PRINTF("Timeout while waiting on busy (0x%02x).\n", bits);
	return -502;
}

static int ata_device_select(int device)
{
	USE_ATA_REGS;
	int res;

	if ((res = ata_wait_busy(0x88)) < 0)
		return res;

	/* If the device was already selected, nothing to do.  */
	if (((ata_hwport->r_select >> 4) & 1) == device)
		return 0;

	/* Select the device.  */
	ata_hwport->r_select = (device & 1) << 4;
	res = ata_hwport->r_control;

	return ata_wait_busy(0x88);
}

/* Export 6 */
/*
	28-bit LBA:
		sector	(7:0)	-> LBA (7:0)
		lcyl	(7:0)	-> LBA (15:8)
		hcyl	(7:0)	-> LBA (23:16)
		device	(3:0)	-> LBA (27:24)

	48-bit LBA just involves writing the upper 24 bits in the format above into each respective register on the first write pass, before writing the lower 24 bits in the 2nd write pass. The LBA bits within the device field are not used in either write pass.
*/
int ata_io_start(void *buf, unsigned int blkcount, unsigned short int feature, unsigned short int nsector, unsigned short int sector, unsigned short int lcyl, unsigned short int hcyl, unsigned short int select, unsigned short int command)
{
	USE_ATA_REGS;
	iop_sys_clock_t cmd_timeout;
	const ata_cmd_info_t *cmd_table;
	int i, res, type, cmd_table_size;
	int using_timeout, device = (select >> 4) & 1;
	unsigned char searchcmd;

	ClearEventFlag(ata_evflg, 0);

	if (!atad_devinfo[device].exists)
		return -505;

	if ((res = ata_device_select(device)) != 0)
		return res;

	/* For the SCE and SMART commands, we need to search on the subcommand
	specified in the feature register.  */
	if (command == ATA_C_SCE_SECURITY_CONTROL) {
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
		searchcmd = command & 0xff;
	}

	type = 0;
	for (i = 0; i < cmd_table_size; i++) {
		if (searchcmd == cmd_table[i].command) {
			type = cmd_table[i].type;
			break;
		}
	}

	if (!(atad_cmd_state.type = type & 0x7F))
		return -506;

	atad_cmd_state.buf = buf;
	atad_cmd_state.blkcount = blkcount;

	/* Check that the device is ready if this the appropiate command.  */
	if (!(ata_hwport->r_control & 0x40)) {
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
	switch (type & 0x7F) {
		case 1:
		case 6:
			using_timeout = 1;
			break;
		case 4:
			atad_cmd_state.dir = (command != ATA_C_READ_DMA && command != ATA_C_READ_DMA_EXT);
			using_timeout = 1;
			break;
	}

	if (using_timeout) {
		cmd_timeout.lo = 0x41eb0000;
		cmd_timeout.hi = 0;

		/* SECURITY ERASE UNIT needs a bit more time.  */
		if (command == ATA_C_SCE_SECURITY_CONTROL && feature == ATA_SCE_SECURITY_ERASE_UNIT)
			USec2SysClock(180000000, &cmd_timeout);

		if ((res = SetAlarm(&cmd_timeout, &ata_alarm_cb, NULL)) < 0)
			return res;
	}

	/* Enable the command completion interrupt.  */
	if ((type & 0x7F) == 1)
		dev9IntrEnable(SPD_INTR_ATA0);

	/* Finally!  We send off the ATA command with arguments.  */
	ata_hwport->r_control = (using_timeout == 0) << 1;

	if(type&0x80){	//For the sake of achieving (greatly) improved performance, write the registers twice only if required! This is also required for compatibility with the buggy firmware of certain PSX units.
		/* 48-bit LBA requires writing to the address registers twice,
		   24 bits of the LBA address is written each time.
		   Writing to registers twice does not affect 28-bit LBA since
		   only the latest data stored in address registers is used.  */
		ata_hwport->r_feature = (feature >> 8) & 0xff;
		ata_hwport->r_nsector = (nsector >> 8) & 0xff;
		ata_hwport->r_sector  = (sector >> 8) & 0xff;
		ata_hwport->r_lcyl    = (lcyl >> 8) & 0xff;
		ata_hwport->r_hcyl    = (hcyl >> 8) & 0xff;
	}

	ata_hwport->r_feature = feature & 0xff;
	ata_hwport->r_nsector = nsector & 0xff;
	ata_hwport->r_sector  = sector & 0xff;
	ata_hwport->r_lcyl    = lcyl & 0xff;
	ata_hwport->r_hcyl    = hcyl & 0xff;
	ata_hwport->r_select  = select & 0xff;
	ata_hwport->r_command = command & 0xff;

	/* Turn on the LED.  */
	dev9LEDCtl(1);

	return 0;
}

/* Do a PIO transfer, to or from the device.  */
static inline int ata_pio_transfer(ata_cmd_state_t *cmd_state)
{
	USE_ATA_REGS;
	void *buf;
	int i, type;
	unsigned short int status = ata_hwport->r_status & 0xff;

	if (status & ATA_STAT_ERR) {
		M_PRINTF("Error: Command error: status 0x%02x, error 0x%02x.\n", status, ata_get_error());
		return -503;
	}

	/* DRQ must be set (data request).  */
	if (!(status & ATA_STAT_DRQ))
		return -504;

	type = cmd_state->type;

	if (type == 3 || type == 8) {
		/* PIO data out */
		buf = cmd_state->buf;
		for (i = 0; i < 256; i++) {
			ata_hwport->r_data = *(unsigned short int *)buf;
			cmd_state->buf = ++((unsigned short int *)buf);
		}
		if (cmd_state->type == 8) {
			for (i = 0; i < 4; i++) {
				ata_hwport->r_data = *(u8 *)buf;
				cmd_state->buf = ++((u8 *)buf);
			}
		}
	} else if (type == 2) {
		/* PIO data in  */
		buf = cmd_state->buf;
		for (i = 0; i < 256; i++) {
			*(unsigned short int *)buf = ata_hwport->r_data;
			cmd_state->buf = ++((unsigned short int *)buf);
		}
	}

	return 0;
}

/* Complete a DMA transfer, to or from the device.  */
static inline int ata_dma_complete(void *buf, int blkcount, int dir)
{
	USE_ATA_REGS;
	USE_SPD_REGS;
	unsigned int count, nbytes;
	u32 bits;
	int i, res;
	unsigned short int dma_stat;

	while (blkcount) {
		for (i = 0; i < 20; i++)
			if ((dma_stat = SPD_REG16(0x38) & 0x1f))
				goto next_transfer;

		if (dma_stat)
			goto next_transfer;

		dev9IntrEnable(SPD_INTR_ATA);
		/* Wait for the previous transfer to complete or a timeout.  */
		WaitEventFlag(ata_evflg, 0x03, WEF_CLEAR|WEF_OR, &bits);

		if (bits & 0x01) {	/* Timeout.  */
			M_PRINTF("Error: DMA timeout.\n");
			return -502;
		}
		/* No DMA completion bit? Spurious interrupt.  */
		if (!(SPD_REG16(SPD_R_INTR_STAT) & 0x02)) {
			if (ata_hwport->r_control & 0x01) {
				M_PRINTF("Error: Command error while doing DMA.\n");
				M_PRINTF("Error: Command error status 0x%02x, error 0x%02x.\n", ata_hwport->r_status, ata_get_error());
				return -503;
			} else {
				M_PRINTF("Warning: Got command interrupt, but not an error.\n");
				continue;
			}
		}

		dma_stat = SPD_REG16(0x38) & 0x1f;

next_transfer:
		count = (blkcount < dma_stat) ? blkcount : dma_stat;
		nbytes = count * 512;
		if ((res = dev9DmaTransfer(0, buf, (nbytes << 9)|32, dir)) < 0)
			return res;

		(u8 *)buf += nbytes;
		blkcount -= count;
	}

	return 0;
}

/* Export 7 */
int ata_io_finish(void)
{
	USE_SPD_REGS;
	USE_ATA_REGS;
	ata_cmd_state_t *cmd_state = &atad_cmd_state;
	u32 bits;
	int i, res = 0, type = cmd_state->type;
	unsigned short int stat;

	if (type == 1 || type == 6) {	/* Non-data commands.  */
		WaitEventFlag(ata_evflg, 0x03, WEF_CLEAR|WEF_OR, &bits);
		if (bits & 0x01) {	/* Timeout.  */
			M_PRINTF("Error: ATA timeout on a non-data command.\n");
			return -502;
		}
	} else if (type == 4) {		/* DMA.  */
		if ((res = ata_dma_complete(cmd_state->buf, cmd_state->blkcount,
						cmd_state->dir)) < 0)
			goto finish;

		for (i = 0; i < 100; i++)
			if ((stat = SPD_REG16(SPD_R_INTR_STAT) & 0x01))
				break;
		if (!stat) {
			dev9IntrEnable(SPD_INTR_ATA0);
			WaitEventFlag(ata_evflg, 0x03, WEF_CLEAR|WEF_OR, &bits);
			if (bits & 0x01) {
				M_PRINTF("Error: ATA timeout on DMA completion.\n");
				res = -502;
			}
		}
	} else {			/* PIO transfers.  */
		stat = ata_hwport->r_control;
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
	if (ata_hwport->r_status & ATA_STAT_BUSY)
		res = ata_wait_busy(0x80);
	if ((stat = ata_hwport->r_status) & ATA_STAT_ERR) {
		M_PRINTF("Error: Command error: status 0x%02x, error 0x%02x.\n", stat, ata_get_error());
		res = -503;
	}

finish:
	/* The command has completed (with an error or not), so clean things up.  */
	CancelAlarm(&ata_alarm_cb, NULL);
	/* Turn off the LED.  */
	dev9LEDCtl(0);

	return res;
}

/* Reset the ATA controller/bus.  */
static inline int ata_bus_reset(void)
{
	USE_SPD_REGS;
	SPD_REG16(SPD_R_IF_CTRL) = SPD_IF_ATA_RESET;
	DelayThread(100);
	SPD_REG16(SPD_R_IF_CTRL) = 0x48;
	DelayThread(3000);
	return ata_wait_busy(0x80);
}

/* Export 5 */
int ata_reset_devices(void)
{
	USE_ATA_REGS;

	if (ata_hwport->r_control & 0x80)
		return -501;

	/* Disables device interrupt assertion and asserts SRST. */
	ata_hwport->r_control = 6;
	DelayThread(100);

	/* Disable ATA interrupts.  */
	ata_hwport->r_control = 2;
	DelayThread(3000);
	
	return ata_wait_busy(0x80);
}

/* Export 17 */
int ata_device_flush_cache(int device)
{
	int res;
	
	if(!(res = ata_io_start(NULL, 1, 0, 0, 0, 0, 0, (device << 4) & 0xffff, lba_48bit[device]?ATA_C_FLUSH_CACHE_EXT:ATA_C_FLUSH_CACHE))) res=ata_io_finish();

	return res;
}

/* Export 13 */
int ata_device_idle(int device, int period)
{
	int res;

	if(!(res = ata_io_start(NULL, 1, 0, period & 0xff, 0, 0, 0, (device << 4) & 0xffff, ATA_C_IDLE))) res=ata_io_finish();

	return res;
}

static int ata_device_identify(int device, void *info)
{
	int res;

	if(!(res = ata_io_start(info, 1, 0, 0, 0, 0, 0, (device << 4) & 0xffff, ATA_C_IDENTIFY_DEVICE))) res=ata_io_finish();

	return res;
}

static int ata_device_pkt_identify(int device, void *info)
{
	int res;

	res = ata_io_start(info, 1, 0, 0, 0, 0, 0, (device << 4) & 0xffff, ATA_C_IDENTIFY_PACKET_DEVICE);
	if (!res)
		return ata_io_finish();
	return res;
}

/* Export 14 */
int ata_device_sce_identify_drive(int device, void *data)
{
	int res;

	if(!(res = ata_io_start(data, 1, ATA_SCE_IDENTIFY_DRIVE, 0, 0, 0, 0, (device << 4) & 0xffff, ATA_C_SCE_SECURITY_CONTROL))) res=ata_io_finish();

	return res;
}

static int ata_device_smart_enable(int device)
{
	int res;

	if(!(res = ata_io_start(NULL, 1, ATA_S_SMART_ENABLE_OPERATIONS, 0, 0, 0x4f, 0xc2, (device << 4) & 0xffff, ATA_C_SMART))) res=ata_io_finish();

	return res;
}

/* Export 16 */
int ata_device_smart_save_attr(int device)
{
	int res;

	if(!(res = ata_io_start(NULL, 1, ATA_S_SMART_SAVE_ATTRIBUTE_VALUES, 0, 0, 0x4f, 0xc2, (device << 4) & 0xffff, ATA_C_SMART))) res=ata_io_finish();

	return res;
}

/* Export 15 */
int ata_device_smart_get_status(int device)
{
	USE_ATA_REGS;
	int res;

	res = ata_io_start(NULL, 1, ATA_S_SMART_RETURN_STATUS, 0, 0, 0x4f, 0xc2, (device << 4) & 0xffff, ATA_C_SMART);
	if (res)
		return res;

	res = ata_io_finish();
	if (res)
		return res;

	/* Check to see if the report exceeded the threshold.  */
	if (((ata_hwport->r_lcyl&0xFF) != 0x4f) || ((ata_hwport->r_hcyl&0xFF) != 0xc2)) {
		M_PRINTF("Error: SMART report exceeded threshold.\n");
		return 1;
	}

	return res;
}

/* Set features - set transfer mode.  */
int ata_device_set_transfer_mode(int device, int type, int mode)
{
	int res;

	res = ata_io_start(NULL, 1, 3, (type|mode) & 0xff, 0, 0, 0, (device << 4) & 0xffff, ATA_C_SET_FEATURES);
	if (res)
		return res;

	res = ata_io_finish();
	if (res)
		return res;

	switch(type){
		case ATAD_XFER_MODE_MDMA:
			ata_multiword_dma_mode(mode);
			break;
		case ATAD_XFER_MODE_UDMA:
			ata_ultra_dma_mode(mode);
			break;
		case ATAD_XFER_MODE_PIO:
			ata_pio_mode(mode);
			break;
	}

	return 0;
}

/* Export 9 */
int ata_device_sector_io(int device, void *buf, unsigned int lba, unsigned int nsectors, int dir)
{
	int res = 0;
	unsigned short int sector, lcyl, hcyl, select, command, len;

	while (nsectors > 0) {
		ata_set_dir(dir);

		/* Variable lba is only 32 bits so no change for lcyl and hcyl.  */
		lcyl = (lba >> 8) & 0xff;
		hcyl = (lba >> 16) & 0xff;

		if (lba_48bit[device]) {
			/* Setup for 48-bit LBA.  */
			len = (nsectors > 65536) ? 65536 : nsectors;

			/* Combine bits 24-31 and bits 0-7 of lba into sector.  */
			sector = ((lba >> 16) & 0xff00) | (lba & 0xff);
			/* 0x40 enables LBA.  */
			select = ((device << 4) | 0x40) & 0xffff;
			command = (dir == 1) ? ATA_C_WRITE_DMA_EXT : ATA_C_READ_DMA_EXT;
		} else {
			/* Setup for 28-bit LBA.  */
			len = (nsectors > 256) ? 256 : nsectors;
			sector = lba & 0xff;
			/* 0x40 enables LBA.  */
			select = ((device << 4) | ((lba >> 24) & 0xf) | 0x40) & 0xffff;
			command = (dir == 1) ? ATA_C_WRITE_DMA : ATA_C_READ_DMA;
		}

		if ((res = ata_io_start(buf, len, 0, len, sector, lcyl, hcyl, select, command)) != 0)
			return res;
		if ((res = ata_io_finish()) != 0)
			return res;

		(u8 *)buf += (len * 512);
		lba += len;
		nsectors -= len;
	}

	return res;
}

static void ata_get_security_status(int device, ata_devinfo_t *devinfo, unsigned short int *param)
{
	if (ata_device_identify(device, param) == 0)
		devinfo[device].security_status = param[ATA_ID_SECURITY_STATUS];
}

/* Export 10 */
int ata_device_sce_sec_set_password(int device, void *password)
{
	ata_devinfo_t *devinfo = atad_devinfo;
	unsigned short int *param = ata_param;
	int res;

	if (devinfo[device].security_status & ATA_F_SEC_ENABLED) return 0;

	memset(param, 0, 512);
	memcpy(param + 1, password, 32);

	res = ata_io_start(param, 1, ATA_SCE_SECURITY_SET_PASSWORD, 0, 0, 0, 0, (device << 4) & 0xffff, ATA_C_SCE_SECURITY_CONTROL);
	if (res == 0)
		res = ata_io_finish();

	ata_get_security_status(device, devinfo, param);
	return res;
}

/* Export 11 */
int ata_device_sce_sec_unlock(int device, void *password)
{
	ata_devinfo_t *devinfo = atad_devinfo;
	unsigned short int *param = ata_param;
	int res;

	if (!(devinfo[device].security_status & ATA_F_SEC_LOCKED)) return 0;

	memset(param, 0, 512);
	memcpy(param + 1, password, 32);

	if ((res = ata_io_start(param, 1, ATA_SCE_SECURITY_UNLOCK, 0, 0, 0, 0, (device << 4) & 0xffff, ATA_C_SCE_SECURITY_CONTROL)) != 0)
		return res;
	if ((res = ata_io_finish()) != 0)
		return res;

	/* Check to see if the drive was actually unlocked.  */
	ata_get_security_status(device, devinfo, param);
	if (devinfo[device].security_status & ATA_F_SEC_LOCKED)
		return -509;

	return 0;
}

/* Export 12 */
int ata_device_sce_sec_erase(int device)
{
	ata_devinfo_t *devinfo = atad_devinfo;
	unsigned short int *param = NULL;
	int res;

	if (!(devinfo[device].security_status & ATA_F_SEC_ENABLED) || !(devinfo[device].security_status & ATA_F_SEC_LOCKED)) return 0;

	/* First send the mandatory ERASE PREPARE command.  */
	if ((res = ata_io_start(NULL, 1, ATA_SCE_SECURITY_ERASE_PREPARE, 0, 0, 0, 0, (device << 4) & 0xffff, ATA_C_SCE_SECURITY_CONTROL)) != 0)
		goto finish;
	if ((res = ata_io_finish()) != 0)
		goto finish;

	if ((res = ata_io_start(NULL, 1, ATA_SCE_SECURITY_ERASE_UNIT, 0, 0, 0, 0, (device << 4) & 0xffff, ATA_C_SCE_SECURITY_CONTROL)) == 0)
		res = ata_io_finish();

finish:
	ata_get_security_status(device, devinfo, param);
	return res;
}

static void ata_device_probe(ata_devinfo_t *devinfo)
{
	USE_ATA_REGS;
	unsigned short int nsector, sector, lcyl, hcyl, select;

	devinfo->exists = 0;
	devinfo->has_packet = 0;

	if (ata_hwport->r_control & 0x88)
		return;

	nsector = ata_hwport->r_nsector & 0xff;
	sector = ata_hwport->r_sector & 0xff;
	lcyl = ata_hwport->r_lcyl & 0xff;
	hcyl = ata_hwport->r_hcyl & 0xff;
	select = ata_hwport->r_select;

	if ((nsector != 1) || (sector != 1))
		return;
	devinfo->exists = 1;

	if ((lcyl == 0x14) && (hcyl == 0xeb))
		devinfo->has_packet = 1;
}

static int ata_init_devices(ata_devinfo_t *devinfo)
{
	USE_ATA_REGS;
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
	if (ata_hwport->r_control & 0xff)
		ata_device_probe(&devinfo[1]);
	else
		devinfo[1].exists = 0;

	ata_pio_mode(0);

	for (i = 0; i < 2; i++) {
		if (!devinfo[i].exists)
			continue;

		/* Send the IDENTIFY DEVICE command. if it doesn't succeed
		   devinfo is disabled.  */
		if (!devinfo[i].has_packet) {
			res = ata_device_identify(i, ata_param);
			devinfo[i].exists = (res == 0);
		} else {
			/* If it's a packet device, send the IDENTIFY PACKET
			   DEVICE command.  */
			res = ata_device_pkt_identify(i, ata_param);
			devinfo[i].exists = (res == 0);
		}

		/* This next section is HDD-specific: if no device or it's a
		   packet (ATAPI) device, we're done.  */
		if (!devinfo[i].exists || devinfo[i].has_packet)
			continue;

		/* This section is to detect whether the HDD supports 48-bit LBA 
		   (IDENITFY DEVICE bit 10 word 83) and get the total sectors from
		   either words(61:60) for 28-bit or words(103:100) for 48-bit.  */
		if (!Disable48bitLBA && (ata_param[ATA_ID_COMMAND_SETS_SUPPORTED] & 0x0400)) {
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

		/* Ultra DMA mode 4.  */
		ata_device_set_transfer_mode(i, ATAD_XFER_MODE_UDMA, 4);
		ata_device_smart_enable(i);
		/* Set idle timeout period to 21min 15s.  */
		ata_device_idle(i, 0xff);
	}
	return 0;
}

/* Export 4 */
ata_devinfo_t * ata_get_devinfo(int device)
{
	if(!ata_devinfo_init){
		if(ata_bus_reset() || ata_init_devices(atad_devinfo)) return NULL;
		ata_devinfo_init = 1;
	}

	return &atad_devinfo[device];
}

static void ata_set_dir(int dir)
{
	USE_SPD_REGS;
	unsigned short int val;

	SPD_REG16(0x38) = 3;
	val = SPD_REG16(SPD_R_IF_CTRL) & 1;
	val |= (dir == ATA_DIR_WRITE) ? 0x4c : 0x4e;
	SPD_REG16(SPD_R_IF_CTRL) = val;
	SPD_REG16(SPD_R_XFR_CTRL) = dir | 0x86;	//In a modern ATAD module, the DMA EN bit (0x80) is set and cleared from the pre and post DMA callbacks instead. Read the comment above about this.
}

static void ata_pio_mode(int mode)
{
	USE_SPD_REGS;
	unsigned short int val;

	switch (mode) {
		case 1:
			val = 0x72;
			break;
		case 2:
			val = 0x32;
			break;
		case 3:
			val = 0x24;
			break;
		case 4:
			val = 0x23;
			break;
		default:
			val = 0x92;
	}

	SPD_REG16(SPD_R_PIO_MODE) = val;
}

static void ata_multiword_dma_mode(int mode)
{
	USE_SPD_REGS;
	unsigned short int val;

	switch(mode){
		case 1:
			val = 0x45;
			break;
		case 2:
			val = 0x24;
			break;
		default:
			val = 0xff;
	}

	SPD_REG16(SPD_R_MWDMA_MODE) = val;
	SPD_REG16(SPD_R_IF_CTRL) = (SPD_REG16(SPD_R_IF_CTRL) & 0xfffe)|0x48;
}

static void ata_ultra_dma_mode(int mode)
{
	USE_SPD_REGS;
	unsigned short int val;

	switch (mode)
	{
		case 1:
			val = 0x85;
			break;
		case 2:
			val = 0x63;
			break;
		case 3:
			val = 0x62;
			break;
		case 4:
			val = 0x61;
			break;
		default:
			val = 0xa7;
	}

	SPD_REG16(SPD_R_UDMA_MODE) = val;
	SPD_REG16(SPD_R_IF_CTRL) |= 0x49;
}

/* Export 18 */
int ata_device_idle_immediate(int device){
	int res;

	if (!(res = ata_io_start(NULL, 1, 0, 0, 0, 0, 0, (device << 4)&0xFFFF, ATA_C_IDLE_IMMEDIATE))) res = ata_io_finish();

	return res;
}

/* Export 19 */
int ata_device_is_48bit(int device)
{
	return lba_48bit[device];
}
