/*
 * usb_driver.c - USB Mass Storage Driver
 * See usbmass-ufi10.pdf and usbmassbulk_10.pdf
 */

#include <errno.h>
#include <sysclib.h>
#include <thsemap.h>
#include <stdio.h>
#include <usbd.h>
#include <usbd_macro.h>

//#define DEBUG  //comment out this line when not debugging

#include "mass_debug.h"
#include <usbhdfsd.h>
#include "usbhd_common.h"
#include "mass_stor.h"
#include "scache.h"
#include "part_driver.h"

#define getBI32(__buf) ((((u8 *)(__buf))[3] << 0) | (((u8 *)(__buf))[2] << 8) | (((u8 *)(__buf))[1] << 16) | (((u8 *)(__buf))[0] << 24))

#define USB_SUBCLASS_MASS_RBC 0x01
#define USB_SUBCLASS_MASS_ATAPI 0x02
#define USB_SUBCLASS_MASS_QIC 0x03
#define USB_SUBCLASS_MASS_UFI 0x04
#define USB_SUBCLASS_MASS_SFF_8070I 0x05
#define USB_SUBCLASS_MASS_SCSI 0x06

#define USB_PROTOCOL_MASS_CBI 0x00
#define USB_PROTOCOL_MASS_CBI_NO_CCI 0x01
#define USB_PROTOCOL_MASS_BULK_ONLY 0x50

#define TAG_TEST_UNIT_READY 0
#define TAG_REQUEST_SENSE 3
#define TAG_INQUIRY 18
#define TAG_READ_CAPACITY 37
#define TAG_READ 40
#define TAG_START_STOP_UNIT 33
#define TAG_WRITE 42

#define USB_BLK_EP_IN 0
#define USB_BLK_EP_OUT 1

#define USB_XFER_MAX_RETRIES 8
#define USB_IO_MAX_RETRIES 16

#define CBW_TAG 0x43425355
#define CSW_TAG 0x53425355

typedef struct _cbw_packet
{
    unsigned int signature;
    unsigned int tag;
    unsigned int dataTransferLength;
    unsigned char flags; //80->data in,  00->out
    unsigned char lun;
    unsigned char comLength;   //command data length
    unsigned char comData[16]; //command data
} cbw_packet __attribute__((packed));

typedef struct _csw_packet
{
    unsigned int signature;
    unsigned int tag;
    unsigned int dataResidue;
    unsigned char status;
} csw_packet __attribute__((packed));

typedef struct _inquiry_data
{
    u8 peripheral_device_type; // 00h - Direct access (Floppy), 1Fh none (no FDD connected)
    u8 removable_media;        // 80h - removeable
    u8 iso_ecma_ansi;
    u8 response_data_format;
    u8 additional_length;
    u8 res[3];
    u8 vendor[8];
    u8 product[16];
    u8 revision[4];
} inquiry_data __attribute__((packed));

typedef struct _sense_data
{
    u8 error_code;
    u8 res1;
    u8 sense_key;
    u8 information[4];
    u8 add_sense_len;
    u8 res3[4];
    u8 add_sense_code;
    u8 add_sense_qual;
    u8 res4[4];
} sense_data __attribute__((packed));

typedef struct _read_capacity_data
{
    u8 last_lba[4];
    u8 block_length[4];
} read_capacity_data __attribute__((packed));

static UsbDriver driver;

typedef struct _usb_callback_data
{
    int semh;
    int returnCode;
    int returnSize;
} usb_callback_data;

#define NUM_DEVICES 2
static mass_dev g_mass_device[NUM_DEVICES];

static void mass_stor_release(mass_dev *dev);

static void usb_callback(int resultCode, int bytes, void *arg)
{
    usb_callback_data *data = (usb_callback_data *)arg;
    data->returnCode = resultCode;
    data->returnSize = bytes;
    XPRINTF("USBHDFSD: callback: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
    SignalSema(data->semh);
}

static void usb_set_configuration(mass_dev *dev, int configNumber)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    XPRINTF("USBHDFSD: setting configuration controlEp=%i, confNum=%i \n", dev->controlEp, configNumber);
    ret = UsbSetDeviceConfiguration(dev->controlEp, configNumber, usb_callback, (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending set_configuration %d\n", ret);
    }
}

static void usb_set_interface(mass_dev *dev, int interface, int altSetting)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    XPRINTF("USBHDFSD: setting interface controlEp=%i, interface=%i altSetting=%i\n", dev->controlEp, interface, altSetting);
    ret = UsbSetInterface(dev->controlEp, interface, altSetting, usb_callback, (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending set_interface %d\n", ret);
    }
}

static int usb_bulk_clear_halt(mass_dev *dev, int endpoint)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    ret = UsbClearEndpointFeature(
        dev->controlEp, //Config pipe
        0,              //HALT feature
        (endpoint == USB_BLK_EP_IN) ? dev->bulkEpI : dev->bulkEpO,
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending clear halt %d\n", ret);
    }

    return ret;
}

static void usb_bulk_reset(mass_dev *dev, int mode)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    //Call Bulk only mass storage reset
    ret = UsbControlTransfer(
        dev->controlEp, //default pipe
        0x21,           //bulk reset
        0xFF,
        0,
        dev->interfaceNumber, //interface number
        0,                    //length
        NULL,                 //data
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret == USB_RC_OK) {
        //clear bulk-in endpoint
        if (mode & 0x01)
            ret = usb_bulk_clear_halt(dev, USB_BLK_EP_IN);
    }
    if (ret == USB_RC_OK) {
        //clear bulk-out endpoint
        if (mode & 0x02)
            ret = usb_bulk_clear_halt(dev, USB_BLK_EP_OUT);
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending reset %d to device %d.\n", ret, dev->devId);
        dev->status |= USBMASS_DEV_STAT_ERR;
    }
}

static int usb_bulk_status(mass_dev *dev, csw_packet *csw, unsigned int tag)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    csw->signature = CSW_TAG;
    csw->tag = tag;
    csw->dataResidue = 0;
    csw->status = 0;

    ret = UsbBulkTransfer(
        dev->bulkEpI, //bulk input pipe
        csw,          //data ptr
        13,           //data length
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;

#ifdef DEBUG
        if (cb_data.returnSize != 13)
            printf("USBHDFSD: bulk csw.status returnSize: %i != 13\n", cb_data.returnSize);
        if (csw->dataResidue != 0)
            printf("USBHDFSD: bulk csw.status residue: %i\n", csw->dataResidue);
        XPRINTF("USBHDFSD: bulk csw result: %d, csw.status: %i\n", ret, csw->status);
#endif
    }

    return ret;
}

/* see flow chart in the usbmassbulk_10.pdf doc (page 15)

	Returned values:
		<0 Low-level USBD error.
		0 = Command completed successfully.
		1 = Command failed.
		2 = Phase error.
*/
static int usb_bulk_manage_status(mass_dev *dev, unsigned int tag)
{
    int ret;
    csw_packet csw;

    //XPRINTF("USBHDFSD: usb_bulk_manage_status 1 ...\n");
    ret = usb_bulk_status(dev, &csw, tag);       /* Attempt to read CSW from bulk in endpoint */
    if (ret != USB_RC_OK) {                      /* STALL bulk in  -OR- Bulk error */
        usb_bulk_clear_halt(dev, USB_BLK_EP_IN); /* clear the stall condition for bulk in */

        XPRINTF("USBHDFSD: usb_bulk_manage_status error %d ...\n", ret);
        ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
    }

    /* CSW not valid  or stalled or phase error */
    if (ret != USB_RC_OK || csw.signature != CSW_TAG || csw.tag != tag || csw.status == 2) {
        printf("USBHDFSD: usb_bulk_manage_status call reset recovery ...\n");
        usb_bulk_reset(dev, 3); /* Perform reset recovery */
    }

    return ((ret == USB_RC_OK && csw.signature == CSW_TAG && csw.tag == tag) ? csw.status : -1);
}

static int usb_bulk_get_max_lun(mass_dev *dev)
{
    int ret;
    usb_callback_data cb_data;
    char max_lun;

    cb_data.semh = dev->ioSema;

    //Call Bulk only mass storage reset
    ret = UsbControlTransfer(
        dev->controlEp, //default pipe
        0xA1,
        0xFE,
        0,
        dev->interfaceNumber, //interface number
        1,                    //length
        &max_lun,             //data
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret == USB_RC_OK) {
        ret = max_lun;
    } else {
        //Devices that do not support multiple LUNs may STALL this command.
        usb_bulk_clear_halt(dev, USB_BLK_EP_IN);
        usb_bulk_clear_halt(dev, USB_BLK_EP_OUT);

        ret = -ret;
    }

    return ret;
}

static int usb_bulk_command(mass_dev *dev, cbw_packet *packet)
{
    int ret;
    usb_callback_data cb_data;

    if (dev->status & USBMASS_DEV_STAT_ERR) {
        printf("USBHDFSD: Rejecting I/O to offline device %d.\n", dev->devId);
        return -1;
    }

    cb_data.semh = dev->ioSema;

    ret = UsbBulkTransfer(
        dev->bulkEpO, //bulk output pipe
        packet,       //data ptr
        31,           //data length
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending bulk command %d. Calling reset recovery.\n", ret);
        usb_bulk_reset(dev, 3);
    }

    return ret;
}

static int usb_bulk_transfer(mass_dev *dev, int direction, void *buffer, unsigned int transferSize)
{
    int ret;
    unsigned char *buf = (unsigned char *)buffer;
    int blockSize = transferSize;
    int offset = 0, pipe;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    pipe = (direction == USB_BLK_EP_IN) ? dev->bulkEpI : dev->bulkEpO;
    while (transferSize > 0) {
        if (transferSize < blockSize) {
            blockSize = transferSize;
        }

        ret = UsbBulkTransfer(
            pipe,           //bulk pipe epI(Read)  epO(Write)
            (buf + offset), //data ptr
            blockSize,      //data length
            usb_callback,
            (void *)&cb_data);
        if (ret != USB_RC_OK) {
            cb_data.returnCode = ret;
            break;
        } else {
            WaitSema(cb_data.semh);
            //XPRINTF("USBHDFSD: retCode=%i retSize=%i \n", cb_data.returnCode, cb_data.returnSize);
            if (cb_data.returnCode != USB_RC_OK) {
                break;
            }
            offset += cb_data.returnSize;
            transferSize -= cb_data.returnSize;
        }
    }

    if (cb_data.returnCode != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - bulk data transfer %d. Clearing HALT state.\n", cb_data.returnCode);
        usb_bulk_clear_halt(dev, direction);
    }

    return cb_data.returnCode;
}

static inline int cbw_scsi_test_unit_ready(mass_dev *dev)
{
    int result, retries;
    static cbw_packet cbw = {
        CBW_TAG,              // cbw.signature
        -TAG_TEST_UNIT_READY, // cbw.tag
        0,                    // cbw.dataTransferLength
        0x80,                 // cbw.flags
        0,                    // cbw.lun
        12,                   // cbw.comLength

        /* scsi command packet */
        {
            0x00, // test unit ready operation code
            0,    // lun/reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
        }};

    XPRINTF("USBHDFSD: cbw_scsi_test_unit_ready\n");

    for (result = -EIO, retries = USB_XFER_MAX_RETRIES; retries > 0; retries--) {
        if (usb_bulk_command(dev, &cbw) == USB_RC_OK) {
            if ((result = usb_bulk_manage_status(dev, -TAG_TEST_UNIT_READY)) >= 0)
                break;
        }
    }

    return result;
}

static inline int cbw_scsi_request_sense(mass_dev *dev, void *buffer, int size)
{
    int rcode, result, retries;
    static cbw_packet cbw = {
        CBW_TAG,            // cbw.signature
        -TAG_REQUEST_SENSE, // cbw.tag
        0,                  // cbw.dataTransferLength
        0x80,               // cbw.flags
        0,                  // cbw.lun
        12,                 // cbw.comLength

        /* scsi command packet */
        {
            0x03, // request sense operation code
            0,    // lun/reserved
            0,    // reserved
            0,    // reserved
            0,    // allocation length
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
        }};

    cbw.dataTransferLength = size;
    cbw.comData[4] = size;

    XPRINTF("USBHDFSD: cbw_scsi_request_sense\n");

    for (retries = USB_XFER_MAX_RETRIES; retries > 0; retries--) {
        if (usb_bulk_command(dev, &cbw) == USB_RC_OK) {
            rcode = usb_bulk_transfer(dev, USB_BLK_EP_IN, buffer, size);
            result = usb_bulk_manage_status(dev, -TAG_REQUEST_SENSE);

            if (rcode == USB_RC_OK && result == 0)
                return 0;
        }
    }

    return -EIO;
}

static inline int cbw_scsi_inquiry(mass_dev *dev, void *buffer, int size)
{
    int rcode, result, retries;
    static cbw_packet cbw = {
        CBW_TAG,      // cbw.signature
        -TAG_INQUIRY, // cbw.tag
        0,            // cbw.dataTransferLength
        0x80,         // cbw.flags
        0,            // cbw.lun
        12,           // cbw.comLength

        /* scsi command packet */
        {
            0x12, // inquiry operation code
            0,    // lun/reserved
            0,    // reserved
            0,    // reserved
            0,    // inquiry reply length
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
        }};

    XPRINTF("USBHDFSD: cbw_scsi_inquiry\n");

    cbw.dataTransferLength = size; //INQUIRY_REPLY_LENGTH
    cbw.comData[4] = size;         //inquiry reply length

    for (retries = USB_XFER_MAX_RETRIES; retries > 0; retries--) {
        if (usb_bulk_command(dev, &cbw) == USB_RC_OK) {
            rcode = usb_bulk_transfer(dev, USB_BLK_EP_IN, buffer, size);
            result = usb_bulk_manage_status(dev, -TAG_INQUIRY);

            if (rcode == USB_RC_OK && result == 0)
                return 0;
        }
    }

    return -EIO;
}

static inline int cbw_scsi_start_stop_unit(mass_dev *dev)
{
    int result, retries;
    static cbw_packet cbw = {
        CBW_TAG,              // cbw.signature
        -TAG_START_STOP_UNIT, // cbw.tag
        0,                    // cbw.dataTransferLength
        0x80,                 // cbw.flags
        0,                    // cbw.lun
        12,                   // cbw.comLength

        /* scsi command packet */
        {
            0x1B, // start stop unit operation code
            0,    // lun/reserved/immed
            0,    // reserved
            0,    // reserved
            1,    // reserved/LoEj/Start "Start the media and acquire the format type"
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
            0,    // reserved
        }};

    XPRINTF("USBHDFSD: cbw_scsi_start_stop_unit\n");

    for (result = -EIO, retries = USB_XFER_MAX_RETRIES; retries > 0; retries--) {
        if (usb_bulk_command(dev, &cbw) == USB_RC_OK) {
            if ((result = usb_bulk_manage_status(dev, -TAG_START_STOP_UNIT)) == 0)
                break;
        }
    }

    return result;
}

static inline int cbw_scsi_read_capacity(mass_dev *dev, void *buffer, int size)
{
    int rcode, result, retries;
    static cbw_packet cbw = {
        CBW_TAG,            // cbw.signature
        -TAG_READ_CAPACITY, // cbw.tag
        0,                  // cbw.dataTransferLength
        0x80,               // cbw.flags
        0,                  // cbw.lun
        12,                 // cbw.comLength

        /* scsi command packet */
        {
            0x25, // read capacity operation code
            0,    // lun/reserved/RelAdr
            0,    // LBA 1
            0,    // LBA 2
            0,    // LBA 3
            0,    // LBA 4
            0,    // reserved
            0,    // reserved
            0,    // reserved/PMI
            0,    // reserved
            0,    // reserved
            0,    // reserved
        }};

    cbw.dataTransferLength = size;

    XPRINTF("USBHDFSD: cbw_scsi_read_capacity\n");

    for (retries = USB_XFER_MAX_RETRIES; retries > 0; retries--) {
        if (usb_bulk_command(dev, &cbw) == USB_RC_OK) {
            rcode = usb_bulk_transfer(dev, USB_BLK_EP_IN, buffer, size);
            result = usb_bulk_manage_status(dev, -TAG_READ_CAPACITY);

            if (rcode == USB_RC_OK && result == 0)
                return 0;
        }
    }

    return -EIO;
}

static inline int cbw_scsi_read_sector(mass_dev *dev, unsigned int lba, void *buffer, unsigned short int sectorCount)
{
    int rcode, result;
    static cbw_packet cbw = {
        CBW_TAG,   // cbw.signature
        -TAG_READ, // cbw.tag
        0,         // cbw.dataTransferLength
        0x80,      // cbw.flags
        0,         // cbw.lun
        12,        // cbw.comLength

        /* scsi command packet */
        {
            0x28, // Operation code
            0,    // LUN/DPO/FUA/Reserved/Reldr
            0,    // lba 1 (MSB)
            0,    // lba 2
            0,    // lba 3
            0,    // lba 4 (LSB)
            0,    // reserved
            0,    // Transfer length MSB
            0,    // Transfer length LSB
            0,    // reserved
            0,    // reserved
            0,    // reserved
        }};
    XPRINTF("USBHDFSD: cbw_scsi_read_sector - 0x%08x %p 0x%04x\n", lba, buffer, sectorCount);

    cbw.dataTransferLength = dev->sectorSize * sectorCount;

    //scsi command packet
    cbw.comData[2] = (lba & 0xFF000000) >> 24;    //lba 1 (MSB)
    cbw.comData[3] = (lba & 0xFF0000) >> 16;      //lba 2
    cbw.comData[4] = (lba & 0xFF00) >> 8;         //lba 3
    cbw.comData[5] = (lba & 0xFF);                //lba 4 (LSB)
    cbw.comData[7] = (sectorCount & 0xFF00) >> 8; //Transfer length MSB
    cbw.comData[8] = (sectorCount & 0xFF);        //Transfer length LSB

    result = -EIO;
    if (usb_bulk_command(dev, &cbw) == USB_RC_OK) {
        rcode = usb_bulk_transfer(dev, USB_BLK_EP_IN, buffer, dev->sectorSize * sectorCount);
        result = usb_bulk_manage_status(dev, -TAG_READ);

        if (rcode != USB_RC_OK)
            result = -EIO;
    }

    return result;
}

static inline int cbw_scsi_write_sector(mass_dev *dev, unsigned int lba, const void *buffer, unsigned short int sectorCount)
{
    int rcode, result;
    static cbw_packet cbw = {
        CBW_TAG,    // cbw.signature
        -TAG_WRITE, // cbw.tag
        0,          // cbw.dataTransferLength
        0,          // cbw.flags
        0,          // cbw.lun
        12,         // cbw.comLength

        /* scsi command packet */
        {
            0x2A, //write operation code
            0,    //LUN/DPO/FUA/Reserved/Reldr
            0,    //lba 1 (MSB)
            0,    //lba 2
            0,    //lba 3
            0,    //lba 4 (LSB)
            0,    //Reserved
            0,    //Transfer length MSB
            0,    //Transfer length LSB
            0,    //reserved
            0,    //reserved
            0,    //reserved
        }};
    XPRINTF("USBHDFSD: cbw_scsi_write_sector - 0x%08x %p 0x%04x\n", lba, buffer, sectorCount);

    cbw.dataTransferLength = dev->sectorSize * sectorCount;

    //scsi command packet
    cbw.comData[2] = (lba & 0xFF000000) >> 24;    //lba 1 (MSB)
    cbw.comData[3] = (lba & 0xFF0000) >> 16;      //lba 2
    cbw.comData[4] = (lba & 0xFF00) >> 8;         //lba 3
    cbw.comData[5] = (lba & 0xFF);                //lba 4 (LSB)
    cbw.comData[7] = (sectorCount & 0xFF00) >> 8; //Transfer length MSB
    cbw.comData[8] = (sectorCount & 0xFF);        //Transfer length LSB

    result = -EIO;
    if (usb_bulk_command(dev, &cbw) == USB_RC_OK) {
        rcode = usb_bulk_transfer(dev, USB_BLK_EP_OUT, (void *)buffer, dev->sectorSize * sectorCount);
        result = usb_bulk_manage_status(dev, -TAG_WRITE);

        if (rcode != USB_RC_OK)
            result = -EIO;
    }

    return result;
}

static mass_dev *mass_stor_findDevice(int devId, int create)
{
    mass_dev *dev = NULL;
    int i;
    XPRINTF("USBHDFSD: mass_stor_findDevice devId %i\n", devId);
    for (i = 0; i < NUM_DEVICES; ++i) {
        if (g_mass_device[i].devId == devId) {
            XPRINTF("USBHDFSD: mass_stor_findDevice exists %i\n", i);
            dev = &g_mass_device[i];
            break;
        } else if (create && dev == NULL && g_mass_device[i].devId == -1) {
            dev = &g_mass_device[i];
            break;
        }
    }
    return dev;
}

/* size should be a multiple of sector size */
int mass_stor_readSector(mass_dev *mass_device, unsigned int sector, unsigned char *buffer, unsigned short int count)
{
    //assert(size % mass_device->sectorSize == 0);
    //assert(sector <= mass_device->maxLBA);
    int retries;

    for (retries = USB_IO_MAX_RETRIES; retries > 0; retries--) {
        if (cbw_scsi_read_sector(mass_device, sector, buffer, count) == 0) {
            return count;
        }
    }
    return -EIO;
}

/* size should be a multiple of sector size */
int mass_stor_writeSector(mass_dev *mass_device, unsigned int sector, const unsigned char *buffer, unsigned short int count)
{
    //assert(size % mass_device->sectorSize == 0);
    //assert(sector <= mass_device->maxLBA);
    int retries;

    for (retries = USB_IO_MAX_RETRIES; retries > 0; retries--) {
        if (cbw_scsi_write_sector(mass_device, sector, buffer, count) == 0) {
            return count;
        }
    }
    return -EIO;
}

/* test that endpoint is bulk endpoint and if so, update device info */
static void usb_bulk_probeEndpoint(int devId, mass_dev *dev, UsbEndpointDescriptor *endpoint)
{
    if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
        /* out transfer */
        if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && dev->bulkEpO < 0) {
            dev->bulkEpO = UsbOpenEndpointAligned(devId, endpoint);
            XPRINTF("USBHDFSD: register Output endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpO, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
        } else
            /* in transfer */
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && dev->bulkEpI < 0) {
            dev->bulkEpI = UsbOpenEndpointAligned(devId, endpoint);
            XPRINTF("USBHDFSD: register Input endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpI, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
        }
    }
}

int mass_stor_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;
    UsbConfigDescriptor *config = NULL;
    UsbInterfaceDescriptor *intf = NULL;

    XPRINTF("USBHDFSD: probe: devId=%i\n", devId);

    mass_dev *mass_device = mass_stor_findDevice(devId, 0);

    /* only one device supported */
    if ((mass_device != NULL) && (mass_device->status & USBMASS_DEV_STAT_CONN)) {
        printf("USBHDFSD: Error - only one mass storage device allowed ! \n");
        return 0;
    }

    /* get device descriptor */
    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        XPRINTF("USBHDFSD: Error - Couldn't get device descriptor\n");
        return 0;
    }

    /* Check if the device has at least one configuration */
    if (device->bNumConfigurations < 1) {
        return 0;
    }

    /* read configuration */
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (config == NULL) {
        XPRINTF("USBHDFSD: Error - Couldn't get configuration descriptor\n");
        return 0;
    }
    /* check that at least one interface exists */
    XPRINTF("USBHDFSD: bNumInterfaces %d\n", config->bNumInterfaces);
    if ((config->bNumInterfaces < 1) ||
        (config->wTotalLength < (sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)))) {
        XPRINTF("USBHDFSD: Error - No interfaces available\n");
        return 0;
    }
    /* get interface */
    intf = (UsbInterfaceDescriptor *)((char *)config + config->bLength); /* Get first interface */
    XPRINTF("USBHDFSD: bInterfaceClass %X bInterfaceSubClass %X bInterfaceProtocol %X\n",
            intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);

    if ((intf->bInterfaceClass != USB_CLASS_MASS_STORAGE) ||
        (intf->bInterfaceSubClass != USB_SUBCLASS_MASS_SCSI &&
         intf->bInterfaceSubClass != USB_SUBCLASS_MASS_SFF_8070I) ||
        (intf->bInterfaceProtocol != USB_PROTOCOL_MASS_BULK_ONLY) ||
        (intf->bNumEndpoints < 2)) { //one bulk endpoint is not enough because
        return 0;                    //we send the CBW to te bulk out endpoint
    }
    return 1;
}

int mass_stor_connect(int devId)
{
    int i;
    int epCount;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;
    iop_sema_t SemaData;
    mass_dev *dev;

    printf("USBHDFSD: connect: devId=%i\n", devId);
    dev = mass_stor_findDevice(devId, 1);

    if (dev == NULL) {
        printf("USBHDFSD: Error - unable to allocate space!\n");
        return 1;
    }

    /* only one mass device allowed */
    if (dev->devId != -1) {
        printf("USBHDFSD: Error - only one mass storage device allowed !\n");
        return 1;
    }

    dev->status = 0;
    dev->sectorSize = 0;

    dev->bulkEpI = -1;
    dev->bulkEpO = -1;

    /* open the config endpoint */
    dev->controlEp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);

    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength); /* Get first interface */

    // store interface numbers
    dev->interfaceNumber = interface->bInterfaceNumber;
    dev->interfaceAlt = interface->bAlternateSetting;

    epCount = interface->bNumEndpoints;
    endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
    usb_bulk_probeEndpoint(devId, dev, endpoint);

    for (i = 1; i < epCount; i++) {
        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);
        usb_bulk_probeEndpoint(devId, dev, endpoint);
    }

    // Bail out if we do NOT have enough bulk endpoints.
    if (dev->bulkEpI < 0 || dev->bulkEpO < 0) {
        mass_stor_release(dev);
        printf("USBHDFSD: Error - connect failed: not enough bulk endpoints! \n");
        return -1;
    }

    SemaData.initial = 0;
    SemaData.max = 1;
    SemaData.option = 0;
    SemaData.attr = 0;
    if ((dev->ioSema = CreateSema(&SemaData)) < 0) {
        printf("USBHDFSD: Failed to allocate I/O semaphore.\n");
        return -1;
    }

    /*store current configuration id - can't call set_configuration here */
    dev->devId = devId;
    dev->configId = config->bConfigurationValue;
    dev->status = USBMASS_DEV_STAT_CONN;
    XPRINTF("USBHDFSD: connect ok: epI=%i, epO=%i \n", dev->bulkEpI, dev->bulkEpO);

    if (dev->callback != NULL)
        dev->callback(USBMASS_DEV_EV_CONN);

    return 0;
}

static void mass_stor_release(mass_dev *dev)
{
    if (dev->bulkEpI >= 0) {
        UsbCloseEndpoint(dev->bulkEpI);
    }

    if (dev->bulkEpO >= 0) {
        UsbCloseEndpoint(dev->bulkEpO);
    }

    dev->bulkEpI = -1;
    dev->bulkEpO = -1;
    dev->controlEp = -1;
    dev->status = 0;
}

int mass_stor_disconnect(int devId)
{
    mass_dev *dev;
    dev = mass_stor_findDevice(devId, 0);

    printf("USBHDFSD: disconnect: devId=%i\n", devId);

    if (dev == NULL) {
        printf("USBHDFSD: Error - disconnect: no device storage!\n");
        return 0;
    }

    if (dev->status & USBMASS_DEV_STAT_CONN) {
        mass_stor_release(dev);
        part_disconnect(dev);
        scache_kill(dev->cache);
        dev->cache = NULL;
        dev->devId = -1;

        DeleteSema(dev->ioSema);

        if (dev->callback != NULL)
            dev->callback(USBMASS_DEV_EV_DISCONN);
    }

    return 0;
}

static int mass_stor_warmup(mass_dev *dev)
{
    inquiry_data id;
    sense_data sd;
    read_capacity_data rcd;
    int stat;

    XPRINTF("USBHDFSD: mass_stor_warmup\n");

    if (!(dev->status & USBMASS_DEV_STAT_CONN)) {
        printf("USBHDFSD: Error - no mass storage device found!\n");
        return -1;
    }

    stat = usb_bulk_get_max_lun(dev);
    XPRINTF("USBHDFSD: usb_bulk_get_max_lun %d\n", stat);

    memset(&id, 0, sizeof(inquiry_data));
    if ((stat = cbw_scsi_inquiry(dev, &id, sizeof(inquiry_data))) < 0) {
        printf("USBHDFSD: Error - cbw_scsi_inquiry %d\n", stat);
        return -1;
    }

    printf("USBHDFSD: Vendor: %.8s\n", id.vendor);
    printf("USBHDFSD: Product: %.16s\n", id.product);
    printf("USBHDFSD: Revision: %.4s\n", id.revision);

    while ((stat = cbw_scsi_test_unit_ready(dev)) != 0) {
        printf("USBHDFSD: Error - cbw_scsi_test_unit_ready %d\n", stat);

        stat = cbw_scsi_request_sense(dev, &sd, sizeof(sense_data));
        if (stat != 0)
            printf("USBHDFSD: Error - cbw_scsi_request_sense %d\n", stat);

        if ((sd.error_code == 0x70) && (sd.sense_key != 0x00)) {
            printf("USBHDFSD: Sense Data key: %02X code: %02X qual: %02X\n", sd.sense_key, sd.add_sense_code, sd.add_sense_qual);

            if ((sd.sense_key == 0x02) && (sd.add_sense_code == 0x04) && (sd.add_sense_qual == 0x02)) {
                printf("USBHDFSD: Error - Additional initalization is required for this device!\n");
                if ((stat = cbw_scsi_start_stop_unit(dev)) != 0) {
                    printf("USBHDFSD: Error - cbw_scsi_start_stop_unit %d\n", stat);
                    return -1;
                }
            }
        }
    }

    if ((stat = cbw_scsi_read_capacity(dev, &rcd, sizeof(read_capacity_data))) != 0) {
        printf("USBHDFSD: Error - cbw_scsi_read_capacity %d\n", stat);
        return -1;
    }

    dev->sectorSize = getBI32(&rcd.block_length);
    dev->maxLBA = getBI32(&rcd.last_lba);
    printf("USBHDFSD: sectorSize %u maxLBA %u\n", dev->sectorSize, dev->maxLBA);

    return 0;
}

int mass_stor_configureNextDevice(void)
{
    int i;

    XPRINTF("USBHDFSD: configuring devices... \n");

    for (i = 0; i < NUM_DEVICES; ++i) {
        mass_dev *dev = &g_mass_device[i];
        if (dev->devId != -1 && (dev->status & USBMASS_DEV_STAT_CONN) && !(dev->status & USBMASS_DEV_STAT_CONF)) {
            int ret;
            usb_set_configuration(dev, dev->configId);
            usb_set_interface(dev, dev->interfaceNumber, dev->interfaceAlt);
            dev->status |= USBMASS_DEV_STAT_CONF;

            ret = mass_stor_warmup(dev);
            if (ret < 0) {
                printf("USBHDFSD: Error - failed to warmup device %d\n", dev->devId);
                mass_stor_release(dev);
                continue;
            }

            dev->cache = scache_init(dev, dev->sectorSize); // modified by Hermes
            if (dev->cache == NULL) {
                printf("USBHDFSD: Error - scache_init failed \n");
                continue;
            }

            return part_connect(dev) >= 0;
        }
    }
    return 0;
}

int InitUSB(void)
{
    int i;
    int ret = 0;
    for (i = 0; i < NUM_DEVICES; ++i) {
		g_mass_device[i].status = 0;
        g_mass_device[i].devId = -1;
	}

    driver.next = NULL;
    driver.prev = NULL;
    driver.name = "mass-stor";
    driver.probe = mass_stor_probe;
    driver.connect = mass_stor_connect;
    driver.disconnect = mass_stor_disconnect;

    ret = UsbRegisterDriver(&driver);
    XPRINTF("USBHDFSD: registerDriver=%i \n", ret);
    if (ret < 0) {
        printf("USBHDFSD: register driver failed! ret=%d\n", ret);
        return (-1);
    }

    return (0);
}

int UsbMassGetDeviceInfo(int device, UsbMassDeviceInfo_t *info)
{
    int result;
    mass_dev *pDev;

    if (device >= 0 && device < NUM_DEVICES) {
        pDev = &g_mass_device[device];

        info->status = pDev->status;
        info->SectorSize = pDev->sectorSize;
        info->MaxLBA = pDev->maxLBA;

        result = 0;
    } else
        result = -ENODEV;

    return result;
}

int UsbMassRegisterCallback(int device, usbmass_cb_t callback)
{
    int result;

    if (device >= 0 && device < NUM_DEVICES) {
        g_mass_device[device].callback = callback;
        result = 0;
		if(g_mass_device[device].status & USBMASS_DEV_STAT_CONN) {
			//If the device is already connected, let the callback know.
			if(callback != NULL) callback(USBMASS_DEV_EV_CONN);
		}
    } else
        result = -ENODEV;

    return result;
}
