/*
  Copyright 2011, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

  ATA Driver for the HD Pro Kit, based on original & updated ATAD from ps2sdk:

  Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
  Licenced under Academic Free License version 2.0
  Review ps2sdk README & LICENSE files for further details.
 
  ATA device driver.
*/

#include <loadcore.h>
#include <intrman.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <stdio.h>
#include <sysclib.h>
#include "atad.h"

#include <atahw.h>


#ifdef __IOPCORE_DEBUG
#define M_PRINTF(format, args...) \
    printf(format, ##args)
#else
#define M_PRINTF(format, args...) \
    do {                          \
    } while (0)
#endif

#define BANNER  "ATA device driver for HD Pro Kit %s\n"
#define VERSION "v1.0"


// HD Pro Kit is mapping the 1st word in ROM0 seg as a main ATA controller,
// The pseudo ATA controller registers are accessed (input/ouput) by writing
// an id to the main ATA controller (id specific to HDpro, see registers id below).
#define HDPROreg_IO8  (*(vu8 *)0xBFC00000)
#define HDPROreg_IO32 (*(vu32 *)0xBFC00000)

#define CDVDreg_STATUS (*(vu8 *)0xBF40200A)

// Pseudo ATA controller registers id - Output
#define ATAreg_CONTROL_RD 0x68
#define ATAreg_SELECT_RD  0x70
#define ATAreg_STATUS_RD  0xf0
#define ATAreg_ERROR_RD   0x90
#define ATAreg_NSECTOR_RD 0x50
#define ATAreg_SECTOR_RD  0xd0
#define ATAreg_LCYL_RD    0x30
#define ATAreg_HCYL_RD    0xb0
#define ATAreg_DATA_RD    0x41

// Pseudo ATA controller registers id - Input
#define ATAreg_CONTROL_WR 0x6a
#define ATAreg_SELECT_WR  0x72
#define ATAreg_COMMAND_WR 0xf2
#define ATAreg_FEATURE_WR 0x92
#define ATAreg_NSECTOR_WR 0x52
#define ATAreg_SECTOR_WR  0xd2
#define ATAreg_LCYL_WR    0x32
#define ATAreg_HCYL_WR    0xb2
#define ATAreg_DATA_WR    0x12

static int ata_evflg = -1;

/* Used for indicating 48-bit LBA support.  */
extern char lba_48bit;

int ata_io_sema = -1;

#define WAITIOSEMA(x)   WaitSema(x)
#define SIGNALIOSEMA(x) SignalSema(x)

#define ATA_EV_TIMEOUT  1
#define ATA_EV_COMPLETE 2 //Unused as there is no completion interrupt

/* Local device info.  */
static ata_devinfo_t atad_devinfo;

/* ATA command info.  */
typedef struct _ata_cmd_info
{
    u8 command;
    u8 type;
} ata_cmd_info_t;

static const ata_cmd_info_t ata_cmd_table[] = {
    {ATA_C_IDENTIFY_DEVICE, 0x02},
    {ATA_C_IDENTIFY_PACKET_DEVICE, 0x02},
    {ATA_C_SMART, 0x07},
    {ATA_C_SET_FEATURES, 0x01},
    {ATA_C_IDLE, 0x01},
    {ATA_C_READ_SECTOR, 0x02},
    {ATA_C_READ_SECTOR_EXT, 0x82},
    {ATA_C_WRITE_SECTOR, 0x03},
    {ATA_C_WRITE_SECTOR_EXT, 0x83},
    {ATA_C_FLUSH_CACHE, 0x01},
    {ATA_C_FLUSH_CACHE_EXT, 0x01}};
#define ATA_CMD_TABLE_SIZE (sizeof ata_cmd_table / sizeof(ata_cmd_info_t))

static const ata_cmd_info_t smart_cmd_table[] = {
    {ATA_S_SMART_ENABLE_OPERATIONS, 0x01}};
#define SMART_CMD_TABLE_SIZE (sizeof smart_cmd_table / sizeof(ata_cmd_info_t))

/* This is the state info tracked between ata_io_start() and ata_io_finish().  */
typedef struct _ata_cmd_state
{
    s32 type; /* The ata_cmd_info_t type field. */
    union
    {
        void *buf;
        u8 *buf8;
        u16 *buf16;
    };
    u32 blkcount; /* The number of 512-byte blocks (sectors) to transfer.  */
} ata_cmd_state_t;

static ata_cmd_state_t atad_cmd_state;

static int hdpro_io_active = 0;
static int intr_suspended = 0;
static int intr_state;

static int hdpro_io_start(void);
static int hdpro_io_finish(void);
static void hdpro_io_write(u8 cmd, u16 val);
static int hdpro_io_read(u8 cmd);
static int ata_bus_reset(void);
static int gen_ata_wait_busy(int bits);
int ata_device_set_write_cache(int device, int enable);

static unsigned int ata_alarm_cb(void *unused)
{
    iSetEventFlag(ata_evflg, ATA_EV_TIMEOUT);
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

int atad_start(void)
{
    int res = 1;
    iop_event_t event;

    M_PRINTF(BANNER, VERSION);

    event.attr = EA_SINGLE; //In v1.04, EA_MULTI was specified.
    event.bits = 0;
    if ((ata_evflg = CreateEventFlag(&event)) < 0) {
        M_PRINTF("Couldn't create event flag, exiting.\n");
        goto out;
    }

    hdpro_io_start();

    HDPROreg_IO8 = 0xe3;
    CDVDreg_STATUS = 0;

    if (ata_bus_reset() != 0)
        goto out;

    iop_sema_t smp;
    smp.initial = 1;
    smp.max = 1;
    smp.option = 0;
    smp.attr = SA_THPRI;
    ata_io_sema = CreateSema(&smp);

    res = 0;
    M_PRINTF("Driver loaded.\n");

out:
    hdpro_io_finish();
    return res;
}

/* Export 8 */
int ata_get_error()
{
    return hdpro_io_read(ATAreg_ERROR_RD) & 0xff;
}

#define ATA_WAIT_BUSY    0x80
#define ATA_WAIT_BUSBUSY 0x88

#define ata_wait_busy()     gen_ata_wait_busy(ATA_WAIT_BUSY)
#define ata_wait_bus_busy() gen_ata_wait_busy(ATA_WAIT_BUSBUSY)

/* 0x80 for busy, 0x88 for bus busy.
	In the original ATAD, the busy and bus-busy functions were separate, but similar.  */
static int gen_ata_wait_busy(int bits)
{
    int i, didx, delay;
    int res = 0;

    for (i = 0; i < 80; i++) {

        hdpro_io_start();

        u16 r_control = hdpro_io_read(ATAreg_CONTROL_RD);

        hdpro_io_finish();

        //Differs from the normal ATAD here.
        if (r_control == 0xffff)
            return ATA_RES_ERR_TIMEOUT;

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

    res = ATA_RES_ERR_TIMEOUT;
    M_PRINTF("Timeout while waiting on busy (0x%02x).\n", bits);

out:
    hdpro_io_start();

    return res;
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
    unsigned int res = HDPROreg_IO8;
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
        hdpro_io_active = 0;         // back to it's original state

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
    unsigned int res0 = HDPROreg_IO8;
    CDVDreg_STATUS = 0;
    unsigned int res1 = HDPROreg_IO8;
    CDVDreg_STATUS = 0;
    res0 = (res0 & 0xff) | (res1 << 8);

    resume_intr();

    return res0 & 0xffff;
}

/* Reset the ATA controller/bus.  */
static int ata_bus_reset(void)
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

    return ata_wait_busy();
}

static int ata_device_select(int device)
{
    int res;

    if ((res = ata_wait_bus_busy()) < 0)
        return res;

    /* If the device was already selected, nothing to do.  */
    if (((hdpro_io_read(ATAreg_SELECT_RD) >> 4) & 1) == device)
        return 0;

    /* Select the device.  */
    hdpro_io_write(ATAreg_SELECT_WR, (device & 1) << 4);
    hdpro_io_read(ATAreg_CONTROL_RD);

    return ata_wait_bus_busy();
}

/* Export 6 */
int ata_io_start(void *buf, u32 blkcount, u16 feature, u16 nsector, u16 sector, u16 lcyl, u16 hcyl, u16 select, u16 command)
{
    iop_sys_clock_t cmd_timeout;
    const ata_cmd_info_t *cmd_table;
    int i, res, type, cmd_table_size;
    int using_timeout, device = (select >> 4) & 1;
    u8 searchcmd;

    ClearEventFlag(ata_evflg, 0);

    if ((res = ata_device_select(device)) != 0)
        return res;

    /* For the SCE and SMART commands, we need to search on the subcommand
	specified in the feature register.  */
    if (command == ATA_C_SMART) {
        cmd_table = smart_cmd_table;
        cmd_table_size = SMART_CMD_TABLE_SIZE;
        searchcmd = (u8)feature;
    } else {
        cmd_table = ata_cmd_table;
        cmd_table_size = ATA_CMD_TABLE_SIZE;
        searchcmd = (u8)command;
    }

    type = 0;
    for (i = 0; i < cmd_table_size; i++) {
        if (searchcmd == cmd_table[i].command) {
            type = cmd_table[i].type;
            break;
        }
    }

    if (!(atad_cmd_state.type = type & 0x7F))
        return ATA_RES_ERR_CMD;

    atad_cmd_state.buf = buf;
    atad_cmd_state.blkcount = blkcount;

    /* Check that the device is ready if this the appropiate command.  */
    if (!(hdpro_io_read(ATAreg_CONTROL_RD) & 0x40)) {
        switch (command) {
            case ATA_C_DEVICE_RESET:
            case ATA_C_EXECUTE_DEVICE_DIAGNOSTIC:
            case ATA_C_INITIALIZE_DEVICE_PARAMETERS:
            case ATA_C_PACKET:
            case ATA_C_IDENTIFY_PACKET_DEVICE:
                break;
            default:
                M_PRINTF("Error: Device %d is not ready.\n", device);
                return ATA_RES_ERR_NOTREADY;
        }
    }

    /* Does this command need a timeout?  */
    using_timeout = 0;
    switch (type & 0x7F) {
        case 1:
        case 6:
            using_timeout = 1;
            break;
            //Support for DMA commands (type = 4) is removed because HDPro cannot support DMA. The original HDPro driver still had code for it though.
    }

    if (using_timeout) {
        cmd_timeout.lo = 0x41eb0000;
        cmd_timeout.hi = 0;

        if ((res = SetAlarm(&cmd_timeout, &ata_alarm_cb, NULL)) < 0)
            return res;
    }

    /* Enable the command completion interrupt.  */
    suspend_intr();
    HDPROreg_IO8 = 0x21;
    CDVDreg_STATUS = 0;
    (void)HDPROreg_IO8;
    CDVDreg_STATUS = 0;
    resume_intr();

    /* Finally!  We send off the ATA command with arguments.  */
    hdpro_io_write(ATAreg_CONTROL_WR, (using_timeout == 0) << 1);
    if (type & 0x80) { //For the sake of achieving improved performance, write the registers twice only if required!
        /* 48-bit LBA requires writing to the address registers twice,
		   24 bits of the LBA address is written each time.
		   Writing to registers twice does not affect 28-bit LBA since
		   only the latest data stored in address registers is used.  */
        hdpro_io_write(ATAreg_FEATURE_WR, (feature & 0xffff) >> 8);
        hdpro_io_write(ATAreg_NSECTOR_WR, (nsector & 0xffff) >> 8);
        hdpro_io_write(ATAreg_SECTOR_WR, (sector & 0xffff) >> 8);
        hdpro_io_write(ATAreg_LCYL_WR, (lcyl & 0xffff) >> 8);
        hdpro_io_write(ATAreg_HCYL_WR, (hcyl & 0xffff) >> 8);
    }

    hdpro_io_write(ATAreg_FEATURE_WR, feature & 0xff);
    hdpro_io_write(ATAreg_NSECTOR_WR, nsector & 0xff);
    hdpro_io_write(ATAreg_SECTOR_WR, sector & 0xff);
    hdpro_io_write(ATAreg_LCYL_WR, lcyl & 0xff);
    hdpro_io_write(ATAreg_HCYL_WR, hcyl & 0xff);

    hdpro_io_write(ATAreg_SELECT_WR, (select | ATA_SEL_LBA) & 0xff); //In v1.04, LBA was enabled in the ata_device_sector_io function.
    hdpro_io_write(ATAreg_COMMAND_WR, command & 0xff);

    return 0;
}

/* Do a PIO transfer, to or from the device.  */
static int ata_pio_transfer(ata_cmd_state_t *cmd_state)
{
    int i, type;
    int res = 0, chk = 0;
    u8 *buf8;
    u16 *buf16;
    u16 status = hdpro_io_read(ATAreg_STATUS_RD);

    if (status & ATA_STAT_ERR) {
        M_PRINTF("Error: Command error: status 0x%02x, error 0x%02x.\n", status, ata_get_error());
        return ATA_RES_ERR_IO;
    }

    /* DRQ must be set (data request).  */
    if (!(status & ATA_STAT_DRQ))
        return ATA_RES_ERR_NODATA;

    type = cmd_state->type;

    if (type == 3 || type == 8) {
        /* PIO data out */
        buf16 = cmd_state->buf16;

        HDPROreg_IO8 = 0x43;
        CDVDreg_STATUS = 0;

        for (i = 0; i < 256; i++) {
            u16 r_data = *buf16;
            hdpro_io_write(ATAreg_DATA_WR, r_data);
            chk ^= r_data + i;
            cmd_state->buf16 = ++buf16;
        }

        u16 out = hdpro_io_read(ATAreg_DATA_RD) & 0xffff;
        if (out != (chk & 0xffff))
            return ATA_RES_ERR_NODATA;

        if (cmd_state->type == 8) {
            buf8 = cmd_state->buf8;
            for (i = 0; i < 4; i++) {
                hdpro_io_write(ATAreg_DATA_WR, *buf8);
                cmd_state->buf8 = ++buf8;
            }
        }

    } else if (type == 2) {
        /* PIO data in  */
        buf16 = cmd_state->buf16;

        suspend_intr();

        HDPROreg_IO8 = 0x53;
        CDVDreg_STATUS = 0;
        CDVDreg_STATUS = 0;

        for (i = 0; i < 256; i++) {

            unsigned int res0 = HDPROreg_IO8;
            CDVDreg_STATUS = 0;
            unsigned int res1 = HDPROreg_IO8;
            CDVDreg_STATUS = 0;

            res0 = (res0 & 0xff) | (res1 << 8);
            chk ^= res0 + i;

            *buf16 = res0 & 0xffff;
            cmd_state->buf = ++buf16;
        }

        HDPROreg_IO8 = 0x51;
        CDVDreg_STATUS = 0;
        CDVDreg_STATUS = 0;

        resume_intr();

        u16 r_data = hdpro_io_read(ATAreg_DATA_RD) & 0xffff;
        if (r_data != (chk & 0xffff))
            return ATA_RES_ERR_NODATA;
    }

    return res;
}

/* Export 7 */
int ata_io_finish(void)
{
    ata_cmd_state_t *cmd_state = &atad_cmd_state;
    u32 bits;
    int res = 0, type = cmd_state->type;
    u16 stat;

    if (type == 1 || type == 6) { /* Non-data commands.  */

        //Unlike ATAD, poll until the device either completes its command or times out. There is no completion interrupt.
        while (1) {
            suspend_intr();

            HDPROreg_IO8 = 0x21;
            CDVDreg_STATUS = 0;
            unsigned int ret = HDPROreg_IO8;
            CDVDreg_STATUS = 0;

            resume_intr();

            if (((ret & 0xff) & 1) != 0)
                break;

            /* The original did not check on the return value of PollEventFlag,
               but PollEventFlag does not seem to return the event flag's bits if the wait condition is not satisfied. */
            if (PollEventFlag(ata_evflg, ATA_EV_TIMEOUT | ATA_EV_COMPLETE, WEF_CLEAR | WEF_OR, &bits) == 0) {
                if (bits & ATA_EV_TIMEOUT) { /* Timeout.  */
                    M_PRINTF("Error: ATA timeout on a non-data command.\n");
                    return ATA_RES_ERR_TIMEOUT;
                }
            }

            DelayThread(500);
        }

        /*  Support for DMA commands (type = 4) is removed because HDPro cannot support DMA.
        The original would return ATA_RES_ERR_TIMEOUT for type = 4. */
    } else { /* PIO transfers.  */
        stat = hdpro_io_read(ATAreg_CONTROL_RD);
        if ((res = ata_wait_busy()) < 0)
            goto finish;

        /* Transfer each PIO data block.  */
        while (--cmd_state->blkcount != -1) {
            if ((res = ata_pio_transfer(cmd_state)) < 0)
                goto finish;
            if ((res = ata_wait_busy()) < 0)
                goto finish;
        }
    }

    if (res)
        goto finish;

    /* Wait until the device isn't busy.  */
    if (hdpro_io_read(ATAreg_STATUS_RD) & ATA_STAT_BUSY)
        res = ata_wait_busy();
    if ((stat = hdpro_io_read(ATAreg_STATUS_RD)) & ATA_STAT_ERR) {
        M_PRINTF("Error: Command error: status 0x%02x, error 0x%02x.\n", stat, ata_get_error());
        res = ATA_RES_ERR_IO;
    }

finish:
    /* The command has completed (with an error or not), so clean things up.  */
    CancelAlarm(&ata_alarm_cb, NULL);

    return res;
}

/* Export 17 */
int ata_device_flush_cache(int device)
{
    int res;

    if (!hdpro_io_start())
        return -1;

    if (!(res = ata_io_start(NULL, 1, 0, 0, 0, 0, 0, (device << 4) & 0xffff, lba_48bit ? ATA_C_FLUSH_CACHE_EXT : ATA_C_FLUSH_CACHE)))
        res = ata_io_finish();

    if (!hdpro_io_finish())
        return -2;

    return res;
}

/* Export 9 */
int ata_device_sector_io(int device, void *buf, u32 lba, u32 nsectors, int dir)
{
    int res = 0;
    unsigned int nbytes;
    u16 sector, lcyl, hcyl, select, command, len;

    WAITIOSEMA(ata_io_sema);

    if (!hdpro_io_start())
        return -1;

    while (nsectors) {
        len = (nsectors > 256) ? 256 : nsectors;

        /* Variable lba is only 32 bits so no change for lcyl and hcyl.  */
        lcyl = (lba >> 8) & 0xff;
        hcyl = (lba >> 16) & 0xff;

        if (lba_48bit) {
            /* Setup for 48-bit LBA.  */
            /* Combine bits 24-31 and bits 0-7 of lba into sector.  */
            sector = ((lba >> 16) & 0xff00) | (lba & 0xff);
            /* In v1.04, LBA was enabled here.  */
            select = (device << 4) & 0xffff;
            command = (dir == 1) ? ATA_C_WRITE_SECTOR_EXT : ATA_C_READ_SECTOR_EXT;
        } else {
            /* Setup for 28-bit LBA.  */
            sector = lba & 0xff;
            /* In v1.04, LBA was enabled here.  */
            select = ((device << 4) | ((lba >> 24) & 0xf)) & 0xffff;
            command = (dir == 1) ? ATA_C_WRITE_SECTOR : ATA_C_READ_SECTOR;
        }

        //Unlike ATAD, retry indefinitely until the I/O operation succeeds.
        if ((res = ata_io_start(buf, len, 0, len, sector, lcyl,
                                hcyl, select, command)) != 0)
            continue;
        if ((res = ata_io_finish()) != 0)
            continue;

        nbytes = len * 512;
        buf = (void *)((u8 *)buf + nbytes);
        lba += len;
        nsectors -= len;
    }

    if (!hdpro_io_finish())
        return -2;

    SIGNALIOSEMA(ata_io_sema);

    return res;
}

/* Export 4 */
ata_devinfo_t *ata_get_devinfo(int device)
{
    return &atad_devinfo;
}

/* Set features - enable/disable write cache.  */
int ata_device_set_write_cache(int device, int enable)
{
    int res;

    if (!hdpro_io_start())
        return -1;

    if ((res = ata_io_start(NULL, 1, enable ? 0x02 : 0x82, 0, 0, 0, 0, (device << 4) & 0xffff, ATA_C_SET_FEATURES)) == 0)
        res = ata_io_finish();

    if (!hdpro_io_finish())
        return -2;

    return res;
}
