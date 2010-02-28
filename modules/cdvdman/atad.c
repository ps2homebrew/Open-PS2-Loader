/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: ps2atad.c 1455 2007-11-04 23:46:27Z roman_ps2dev $
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
#include "dev9.h"
#include "atad.h"

#include <speedregs.h>
#include <atahw.h>

//#define NETLOG_DEBUG

#ifdef NETLOG_DEBUG
// !!! netlog exports functions pointers !!!
extern int (*pNetlogSend)(const char *format, ...);
extern int netlog_inited;
#endif

#ifdef DEV9_DEBUG
#define M_PRINTF(format, args...)	\
	printf(MODNAME ": " format, ## args)
#else
#define M_PRINTF(format, args...)	\
	do {} while (0)
#endif

#define BANNER "ATA device driver %s - Copyright (c) 2003 Marcus R. Brown\n"
#define VERSION "v1.1"

extern int lba_48bit;

static int ata_evflg = -1;

/* Local device info.  */
static ata_devinfo_t atad_devinfo;

/* ATA command info.  */
typedef struct _ata_cmd_info {
	u8	command;
	u8	type;
} ata_cmd_info_t;

static ata_cmd_info_t ata_cmd_table[] = {
	{0xc8, 0x04}, {0xec, 0x02}, {0xa1, 0x02}, {0xb0, 0x07}, {0xef, 0x01}, {0x25, 0x04}, {0xca, 0x04}, {0xe3, 0x01}
};
#define ATA_CMD_TABLE_SIZE	(sizeof ata_cmd_table/sizeof(ata_cmd_info_t))

static ata_cmd_info_t smart_cmd_table[] = {
	{0xd8, 0x01}
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

static int ata_intr_cb(int flag);
static u32 ata_alarm_cb(void *unused);

static void ata_dma_set_dir(int dir);

int atad_start(void)
{
	USE_SPD_REGS;
	iop_event_t event;
	int res = 1;

	M_PRINTF(BANNER, VERSION);

	if (!(SPD_REG16(SPD_R_REV_3) & SPD_CAPS_ATA) || !(SPD_REG16(SPD_R_REV_8) & 0x02)) {
		M_PRINTF("HDD is not connected, exiting.\n");
		goto out;
	}

	event.attr = 0;
	event.bits = 0;
	if ((ata_evflg = CreateEventFlag(&event)) < 0) {
		M_PRINTF("Couldn't create event flag, exiting.\n");
		res = 1;
		goto out;
	}

	dev9RegisterIntrCb(1, ata_intr_cb);
	dev9RegisterIntrCb(0, ata_intr_cb);

	res = 0;
	M_PRINTF("Driver loaded.\n");
out:
	return res;
}

static int ata_intr_cb(int flag)
{
	if (flag != 1) {	/* New card, invalidate device info.  */
		dev9IntrDisable(SPD_INTR_ATA);
		iSetEventFlag(ata_evflg, 0x02);
	}

	return 1;
}

static u32 ata_alarm_cb(void *unused)
{
	iSetEventFlag(ata_evflg, 0x01);
	return 0;
}

/* Export 8 */
int ata_get_error()
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
int ata_io_start(void *buf, u32 blkcount, u16 feature, u16 nsector, u16 sector,
		u16 lcyl, u16 hcyl, u16 select, u16 command)
{
	USE_ATA_REGS;
	USE_SPD_REGS;
	iop_sys_clock_t cmd_timeout;
	ata_cmd_info_t *cmd_table;
	int i, res, type, cmd_table_size;
	int using_timeout, device = (select >> 4) & 1;
	u8 searchcmd;

	ClearEventFlag(ata_evflg, 0);

	if ((res = ata_device_select(device)) != 0)
		return res;

	/* For the SCE and SMART commands, we need to search on the subcommand
	specified in the feature register.  */
	if ((command & 0xffff) == ATA_C_SMART) {
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
	switch (type) {
		case 1:
		case 6:
			using_timeout = 1;
			break;
		case 4:
			atad_cmd_state.dir = ATA_DIR_READ;
			using_timeout = 1;
	}

	if (using_timeout) {
		cmd_timeout.lo = 0x41eb0000;
		cmd_timeout.hi = 0;

		if ((res = SetAlarm(&cmd_timeout, (void *)ata_alarm_cb, NULL)) < 0)
			return res;
	}

	/* Enable the command completion interrupt.  */
	if (type == 1)
		dev9IntrEnable(SPD_INTR_ATA0);

	/* Finally!  We send off the ATA command with arguments.  */
	ata_hwport->r_control = (using_timeout == 0) << 1;

	/* 48-bit LBA requires writing to the address registers twice,
	   24 bits of the LBA address is written each time.
	   Writing to registers twice does not affect 28-bit LBA since
	   only the latest data stored in address registers is used.  */
	ata_hwport->r_feature = (feature >> 8) & 0xff;
	ata_hwport->r_nsector = (nsector >> 8) & 0xff;
	ata_hwport->r_sector  = (sector >> 8) & 0xff;
	ata_hwport->r_lcyl    = (lcyl >> 8) & 0xff;
	ata_hwport->r_hcyl    = (hcyl >> 8) & 0xff;

	ata_hwport->r_feature = feature & 0xff;
	ata_hwport->r_nsector = nsector & 0xff;
	ata_hwport->r_sector  = sector & 0xff;
	ata_hwport->r_lcyl    = lcyl & 0xff;
	ata_hwport->r_hcyl    = hcyl & 0xff;
	ata_hwport->r_select  = select & 0xff;
	ata_hwport->r_command = command & 0xff;

	/* Turn on the LED.  */
	SPD_REG8(SPD_R_PIO_DIR) = 1;
	SPD_REG8(SPD_R_PIO_DATA) = 0;

	return 0;
}

/* Do a PIO transfer, to or from the device.  */
static int ata_pio_transfer(ata_cmd_state_t *cmd_state)
{
	USE_SPD_REGS;
	SPD_REG8(SPD_R_PIO_DATA) = 0;
	return 0;
	
	USE_ATA_REGS;
	void *buf;
	int i, type;
	u16 status = ata_hwport->r_status & 0xff;

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
		for (i = 0; i < 256; i++) {
			ata_hwport->r_data = *(u16 *)buf;
			cmd_state->buf = ++((u16 *)buf);
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
			*(u16 *)buf = ata_hwport->r_data;
			cmd_state->buf = ++((u16 *)buf);
		}
	}

	return 0;
}

/* Complete a DMA transfer, to or from the device.  */
static int ata_dma_complete(void *buf, int blkcount, int dir)
{
	USE_ATA_REGS;
	USE_SPD_REGS;
	u32 bits, count, nbytes;
	int i, res;
	u16 dma_stat;

	while (blkcount) {
		for (i = 0; i < 20; i++)
			if ((dma_stat = SPD_REG16(0x38) & 0x1f))
				goto next_transfer;

		if (dma_stat)
			goto next_transfer;

		dev9IntrEnable(SPD_INTR_ATA);
		/* Wait for the previous transfer to complete or a timeout.  */
		WaitEventFlag(ata_evflg, 0x03, 0x11, &bits);

		if (bits & 0x01) {	/* Timeout.  */
			M_PRINTF("Error: DMA timeout.\n");
			return -502;
		}
		/* No DMA completion bit? Spurious interrupt.  */
		if (!(SPD_REG16(SPD_R_INTR_STAT) & 0x02)) {
			if (ata_hwport->r_control & 0x01) {
				M_PRINTF("Error: Command error while doing DMA.\n");
				M_PRINTF("Error: Command error status 0x%02x, error 0x%02x.\n",
						ata_hwport->r_status, ata_get_error());
				#ifdef NETLOG_DEBUG
					pNetlogSend("Error: Command error status 0x%02x, error 0x%02x.\n",
						ata_hwport->r_status, ata_get_error());
				#endif		
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
int ata_io_finish()
{
	USE_SPD_REGS;
	USE_ATA_REGS;
	ata_cmd_state_t *cmd_state = &atad_cmd_state;
	u32 bits;
	int i, res = 0, type = cmd_state->type;
	u16 stat;

	if (type == 1 || type == 6) {	/* Non-data commands.  */
		WaitEventFlag(ata_evflg, 0x03, 0x11, &bits);
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
			WaitEventFlag(ata_evflg, 0x03, 0x11, &bits);
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
		M_PRINTF("Error: Command error: status 0x%02x, error 0x%02x.\n",
				stat, ata_get_error());
		res = -503;
	}

finish:
	/* The command has completed (with an error or not), so clean things up.  */
	CancelAlarm((void *)ata_alarm_cb, NULL);
	/* Turn off the LED.  */
	SPD_REG8(SPD_R_PIO_DIR) = 1;
	SPD_REG8(SPD_R_PIO_DATA) = 1;

	return res;
}

/* Export 9 */
int ata_device_dma_transfer(int device, void *buf, u32 lba, u32 nsectors, int dir)
{
	int res = 0;
	u32 nbytes;
	u16 sector, lcyl, hcyl, select, command, len;

	while (nsectors) {
		len = (nsectors > 256) ? 256 : nsectors;

		ata_dma_set_dir(dir);

		/* Variable lba is only 32 bits so no change for lcyl and hcyl.  */		
		lcyl = (lba >> 8) & 0xff;
		hcyl = (lba >> 16) & 0xff;

		if (lba_48bit) {
			/* Setup for 48-bit LBA.  */
			/* Combine bits 24-31 and bits 0-7 of lba into sector.  */
			sector = ((lba >> 16) & 0xff00) | (lba & 0xff);
			/* 0x40 enables LBA.  */
			select = ((device << 4) | 0x40) & 0xffff;
			command = ATA_C_READ_DMA_EXT;
		} else {
			/* Setup for 28-bit LBA.  */
			sector = lba & 0xff;
			/* 0x40 enables LBA.  */
			select = ((device << 4) | ((lba >> 24) & 0xf) | 0x40) & 0xffff;
			command = ATA_C_READ_DMA;
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

	return res;
}

/* Export 4 */
ata_devinfo_t * ata_get_devinfo(int device)
{
	return &atad_devinfo;
}

static void ata_dma_set_dir(int dir)
{
	USE_SPD_REGS;
	u16 val;

	SPD_REG16(0x38) = 3;
	val = SPD_REG16(SPD_R_IF_CTRL) & 1;
	val |= (dir == 1) ? 0x4c : 0x4e;
	SPD_REG16(SPD_R_IF_CTRL) = val;
	SPD_REG16(SPD_R_XFR_CTRL) = dir | 0x86;
}
