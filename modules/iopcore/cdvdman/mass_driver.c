/*
 * usb_driver.c - USB Mass Storage Driver
 * See usbmass-ufi10.pdf and usbmassbulk_10.pdf
 */

#include <errno.h>
#include <intrman.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>
#include <stdio.h>
#include <usbd.h>

//#define DEBUG  //comment out this line when not debugging

#include "smsutils.h"
#include "usbd_macro.h"
#include "mass_debug.h"
#include "mass_common.h"
#include "mass_stor.h"
#include "cdvd_config.h"

extern struct cdvdman_settings_usb cdvdman_settings;

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
#define TAG_READ_CAPACITY 37
#define TAG_READ 40
#define TAG_START_STOP_UNIT 33
#define TAG_WRITE 42

#define USB_BLK_EP_IN 0
#define USB_BLK_EP_OUT 1

#define USB_XFER_MAX_RETRIES 16

#define DEVICE_DETECTED 0x01
#define DEVICE_CONFIGURED 0x02
#define DEVICE_ERROR 0x80

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

static int io_sema;

#define WAITIOSEMA(x) WaitSema(x)
#define SIGNALIOSEMA(x) SignalSema(x)

typedef struct _usb_callback_data
{
    int sema;
    int returnCode;
    int returnSize;
} usb_callback_data;

#define USB_BLOCK_SIZE	4096	//Maximum single USB 1.1 transfer length.

typedef struct _usb_transfer_callback_data {
    int sema;
    int pipe;
    u8 *buffer;
    int returnCode;
    unsigned int remaining;
} usb_transfer_callback_data;

static mass_dev g_mass_device;
static void *gSectorBuffer;

static void usb_callback(int resultCode, int bytes, void *arg);
static int perform_bulk_transfer(usb_transfer_callback_data* data);
static void usb_transfer_callback(int resultCode, int bytes, void *arg);
static int usb_set_configuration(mass_dev* dev, int configNumber);
static int usb_set_interface(mass_dev* dev, int interface, int altSetting);
static int usb_bulk_clear_halt(mass_dev* dev, int endpoint);
static void usb_bulk_reset(mass_dev* dev, int mode);
static int usb_bulk_status(mass_dev* dev, csw_packet* csw, unsigned int tag);
static int usb_bulk_manage_status(mass_dev* dev, unsigned int tag);
static int usb_bulk_command(mass_dev* dev, cbw_packet* packet );
static int usb_bulk_transfer(mass_dev* dev, int direction, void* buffer, unsigned int transferSize);

static int mass_stor_warmup(mass_dev *dev);
static void mass_stor_release(mass_dev *dev);

static void usb_callback(int resultCode, int bytes, void *arg)
{
    usb_callback_data *data = (usb_callback_data *)arg;
    data->returnCode = resultCode;
    data->returnSize = bytes;
    XPRINTF("USBHDFSD: callback: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
    SignalSema(data->sema);
}

static int perform_bulk_transfer(usb_transfer_callback_data* data)
{
    int ret, len;

    len = data->remaining > USB_BLOCK_SIZE ? USB_BLOCK_SIZE : data->remaining;
    ret = UsbBulkTransfer(
                data->pipe,		//bulk pipe epI (Read) or epO (Write)
                data->buffer,		//data ptr
                len,			//data length
                &usb_transfer_callback,
                (void*)data
            );
    return ret;
}

static void usb_transfer_callback(int resultCode, int bytes, void *arg)
{
    int ret;
    usb_transfer_callback_data* data = (usb_transfer_callback_data*)arg;

    data->returnCode = resultCode;
    if(resultCode == USB_RC_OK)
    {  //Update transfer progress if successful.
        data->remaining -= bytes;
        data->buffer += bytes;
    }

    if((resultCode == USB_RC_OK) && (data->remaining > 0))
    {	//OK to continue.
        ret = perform_bulk_transfer(data);
        if (ret != USB_RC_OK)
        {
            data->returnCode = ret;
            SignalSema(data->sema);
        }
    }
    else
    {
        SignalSema(data->sema);
    }
}

static int usb_set_configuration(mass_dev *dev, int configNumber)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.sema = dev->ioSema;

    XPRINTF("USBHDFSD: setting configuration controlEp=%i, confNum=%i \n", dev->controlEp, configNumber);
    ret = UsbSetDeviceConfiguration(dev->controlEp, configNumber, usb_callback, (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.sema);
        ret = cb_data.returnCode;
    }

    return ret;
}

static int usb_set_interface(mass_dev *dev, int interface, int altSetting)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.sema = dev->ioSema;

    XPRINTF("USBHDFSD: setting interface controlEp=%i, interface=%i altSetting=%i\n", dev->controlEp, interface, altSetting);
    ret = UsbSetInterface(dev->controlEp, interface, altSetting, usb_callback, (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.sema);
        ret = cb_data.returnCode;
    }

    return ret;
}

static int usb_bulk_clear_halt(mass_dev *dev, int endpoint)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.sema = dev->ioSema;

    ret = UsbClearEndpointFeature(
        dev->controlEp, //Config pipe
        0,              //HALT feature
        (endpoint == USB_BLK_EP_IN) ? dev->bulkEpI : dev->bulkEpO,
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.sema);
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

    cb_data.sema = dev->ioSema;

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
        WaitSema(cb_data.sema);
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
        dev->status |= DEVICE_ERROR;
    }
}

static int usb_bulk_status(mass_dev *dev, csw_packet *csw, unsigned int tag)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.sema = dev->ioSema;

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
        WaitSema(cb_data.sema);
        ret = cb_data.returnCode;

#ifdef DEBUG
        if (cb_data.returnSize != 13)
            XPRINTF("USBHDFSD: bulk csw.status returnSize: %i != 13\n", cb_data.returnSize);
        if (csw->dataResidue != 0)
            XPRINTF("USBHDFSD: bulk csw.status residue: %i\n", csw->dataResidue);
        XPRINTF("USBHDFSD: bulk csw result: %d, csw.status: %i\n", ret, csw->status);
#endif
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending bulk status %d\n", ret);
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

        XPRINTF("USBHDFSD: usb_bulk_manage_status stall ...\n");
        ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
    }

    /* CSW not valid  or stalled or phase error */
    if (ret != USB_RC_OK || csw.signature != CSW_TAG || csw.tag != tag || csw.status == 2) {
        XPRINTF("USBHDFSD: usb_bulk_manage_status call reset recovery ...\n");
        usb_bulk_reset(dev, 3); /* Perform reset recovery */
    }

    return ((ret == USB_RC_OK && csw.signature == CSW_TAG && csw.tag == tag) ? csw.status : -1);
}

static int usb_bulk_command(mass_dev *dev, cbw_packet *packet)
{
    int ret;
    usb_callback_data cb_data;

    if (dev->status & DEVICE_ERROR) {
        XPRINTF("USBHDFSD: Rejecting I/O to offline device %d.\n", dev->devId);
        return -1;
    }

    cb_data.sema = dev->ioSema;

    ret = UsbBulkTransfer(
        dev->bulkEpO, //bulk output pipe
        packet,       //data ptr
        31,           //data length
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.sema);
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
    usb_transfer_callback_data cb_data;

    cb_data.sema = dev->ioSema;
    cb_data.pipe = (direction==USB_BLK_EP_IN) ? dev->bulkEpI : dev->bulkEpO;
    cb_data.buffer = buffer;
    cb_data.remaining = transferSize;

    ret = perform_bulk_transfer(&cb_data);
    if (ret == USB_RC_OK) {
        WaitSema(cb_data.sema);
        ret = cb_data.returnCode;
    }

    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - bulk data transfer %d. Clearing HALT state.\n", cb_data.returnCode);
        usb_bulk_clear_halt(dev, direction);
    }

    return ret;
}

static int cbw_scsi_test_unit_ready(mass_dev *dev)
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

static int cbw_scsi_request_sense(mass_dev *dev, void *buffer, int size)
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

static int cbw_scsi_start_stop_unit(mass_dev *dev)
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

static int cbw_scsi_read_capacity(mass_dev *dev, void *buffer, int size)
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

static void cbw_scsi_sector_io(mass_dev *dev, short int dir, unsigned int lba, void *buffer, unsigned short int sectorCount)
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
    XPRINTF("USBHDFSD: cbw_scsi_sector_io (%s) - 0x%08x %p 0x%04x\n", dir == USB_BLK_EP_IN ? "READ" : "WRITE", lba, buffer, sectorCount);

    if (dir == USB_BLK_EP_IN) { //READ operation
        cbw.tag = -TAG_READ;
        cbw.flags = 0x80;
        cbw.comData[0] = 0x28; //Operation code
    } else {                   //WRITE operation
        cbw.tag = -TAG_WRITE;
        cbw.flags = 0;
        cbw.comData[0] = 0x2A; //Operation code
    }

    cbw.dataTransferLength = dev->sectorSize * sectorCount;

    //scsi command packet
    cbw.comData[2] = (lba & 0xFF000000) >> 24;    //lba 1 (MSB)
    cbw.comData[3] = (lba & 0xFF0000) >> 16;      //lba 2
    cbw.comData[4] = (lba & 0xFF00) >> 8;         //lba 3
    cbw.comData[5] = (lba & 0xFF);                //lba 4 (LSB)
    cbw.comData[7] = (sectorCount & 0xFF00) >> 8; //Transfer length MSB
    cbw.comData[8] = (sectorCount & 0xFF);        //Transfer length LSB

    do {
        result = -EIO;
        if (usb_bulk_command(dev, &cbw) == USB_RC_OK) {
            rcode = usb_bulk_transfer(dev, dir, buffer, dev->sectorSize * sectorCount);
            result = usb_bulk_manage_status(dev, cbw.tag);

            if (rcode != USB_RC_OK)
                result = -EIO;
        }

        if (result == 0)
            break;
        DelayThread(2000);
    } while (result != 0);
}

void mass_stor_readSector(unsigned int lba, unsigned short int nsectors, unsigned char *buffer)
{
    WAITIOSEMA(io_sema);

    cbw_scsi_sector_io(&g_mass_device, USB_BLK_EP_IN, lba, buffer, nsectors);

    SIGNALIOSEMA(io_sema);
}

void mass_stor_writeSector(unsigned int lba, unsigned short int nsectors, const unsigned char *buffer)
{
    WAITIOSEMA(io_sema);

    cbw_scsi_sector_io(&g_mass_device, USB_BLK_EP_OUT, lba, (void *)buffer, nsectors);

    SIGNALIOSEMA(io_sema);
}

/* test that endpoint is bulk endpoint and if so, update device info */
static void usb_bulk_probeEndpoint(int devId, mass_dev *dev, UsbEndpointDescriptor *endpoint)
{
    if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
        /* out transfer */
        if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && dev->bulkEpO < 0) {
            /* When sceUsbdOpenPipe() is used to work around the hardware errata that occurs when an unaligned memory address is specified,
               some USB devices become incompatible. Hence it is preferable to do alignment correction in software instead. */
            dev->bulkEpO = pUsbOpenEndpointAligned(devId, endpoint);
            dev->packetSzO = (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB;
            XPRINTF("USBHDFSD: register Output endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpO, endpoint->bEndpointAddress, dev->packetSzO);
        } else
            /* in transfer */
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && dev->bulkEpI < 0) {
            /* Open this pipe with sceUsbdOpenPipe, to allow unaligned addresses to be used.
               According to the Sony documentation and the USBD code,
               there is always an alignment check if the pipe is opened with the sceUsbdOpenPipeAligned(),
               even when there is never any correction for the bulk in pipe. */
            dev->bulkEpI = pUsbOpenEndpoint(devId, endpoint);
            dev->packetSzI = (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB;
            XPRINTF("USBHDFSD: register Input endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpI, endpoint->bEndpointAddress, dev->packetSzI);
        }
    }
}

int mass_stor_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;
    UsbConfigDescriptor *config = NULL;
    UsbInterfaceDescriptor *intf = NULL;

    XPRINTF("USBHDFSD: probe: devId=%i\n", devId);

    mass_dev *mass_device = &g_mass_device;

    /* only one device supported */
    if ((mass_device != NULL) && (mass_device->status & DEVICE_DETECTED)) {
        XPRINTF("USBHDFSD: Error - only one mass storage device allowed ! \n");
        return 0;
    }

    /* get device descriptor */
    device = (UsbDeviceDescriptor *)pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        XPRINTF("USBHDFSD: Error - Couldn't get device descriptor\n");
        return 0;
    }

    /* Check if the device has at least one configuration */
    if (device->bNumConfigurations < 1) {
        return 0;
    }

    /* read configuration */
    config = (UsbConfigDescriptor *)pUsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
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

    XPRINTF("USBHDFSD: connect: devId=%i\n", devId);
    dev = &g_mass_device;

    if (dev == NULL) {
        XPRINTF("USBHDFSD: Error - unable to allocate space!\n");
        return 1;
    }

    /* only one mass device allowed */
    if (dev->devId != -1) {
        XPRINTF("USBHDFSD: Error - only one mass storage device allowed !\n");
        return 1;
    }

    dev->status = 0;
    dev->sectorSize = 0;

    dev->bulkEpI = -1;
    dev->bulkEpO = -1;

    /* open the config endpoint */
    dev->controlEp = pUsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);

    config = (UsbConfigDescriptor *)pUsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength); /* Get first interface */

    // store interface numbers
    dev->interfaceNumber = interface->bInterfaceNumber;
    dev->interfaceAlt = interface->bAlternateSetting;

    epCount = interface->bNumEndpoints;
    endpoint = (UsbEndpointDescriptor *)pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
    usb_bulk_probeEndpoint(devId, dev, endpoint);

    for (i = 1; i < epCount; i++) {
        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);
        usb_bulk_probeEndpoint(devId, dev, endpoint);
    }

    // Bail out if we do NOT have enough bulk endpoints.
    if (dev->bulkEpI < 0 || dev->bulkEpO < 0) {
        mass_stor_release(dev);
        XPRINTF("USBHDFSD: Error - connect failed: not enough bulk endpoints! \n");
        return -1;
    }

    SemaData.initial = 0;
    SemaData.max = 1;
    SemaData.option = 0;
    SemaData.attr = 0;
    if ((dev->ioSema = CreateSema(&SemaData)) < 0) {
        mass_stor_release(dev);
        XPRINTF("USBHDFSD: Failed to allocate I/O semaphore.\n");
        return -1;
    }

    /*store current configuration id - can't call set_configuration here */
    dev->devId = devId;
    dev->configId = config->bConfigurationValue;
    dev->status = DEVICE_DETECTED;
    XPRINTF("USBHDFSD: connect ok: epI=%i, epO=%i \n", dev->bulkEpI, dev->bulkEpO);

    return 0;
}

static void mass_stor_release(mass_dev *dev)
{
    int OldState;

    if (dev->bulkEpI >= 0) {
        pUsbCloseEndpoint(dev->bulkEpI);
    }

    if (dev->bulkEpO >= 0) {
        pUsbCloseEndpoint(dev->bulkEpO);
    }

    dev->bulkEpI = -1;
    dev->bulkEpO = -1;
    dev->controlEp = -1;
    dev->status = 0;

    if (gSectorBuffer != NULL) {
        CpuSuspendIntr(&OldState);
        FreeSysMemory(gSectorBuffer);
        CpuResumeIntr(OldState);
        gSectorBuffer = NULL;
    }
}

int mass_stor_disconnect(int devId)
{
    mass_dev *dev;
    dev = &g_mass_device;

    XPRINTF("USBHDFSD: disconnect: devId=%i\n", devId);

    if ((dev->status & DEVICE_DETECTED) && devId == dev->devId) {
        mass_stor_release(dev);
        dev->devId = -1;

        DeleteSema(dev->ioSema);
    }
    return 0;
}

static int mass_stor_warmup(mass_dev *dev)
{
    sense_data sd;
    read_capacity_data rcd;
    int stat, OldState;

    XPRINTF("USBHDFSD: mass_stor_warmup\n");

    if (!(dev->status & DEVICE_DETECTED)) {
        XPRINTF("USBHDFSD: Error - no mass storage device found!\n");
        return -1;
    }

    while ((stat = cbw_scsi_test_unit_ready(dev)) != 0) {
        XPRINTF("USBHDFSD: Error - cbw_scsi_test_unit_ready %d\n", stat);

        stat = cbw_scsi_request_sense(dev, &sd, sizeof(sense_data));
        if (stat != 0)
            XPRINTF("USBHDFSD: Error - cbw_scsi_request_sense %d\n", stat);

        if ((sd.error_code == 0x70) && (sd.sense_key != 0x00)) {
            XPRINTF("USBHDFSD: Sense Data key: %02X code: %02X qual: %02X\n", sd.sense_key, sd.add_sense_code, sd.add_sense_qual);

            if ((sd.sense_key == 0x02) && (sd.add_sense_code == 0x04) && (sd.add_sense_qual == 0x02)) {
                XPRINTF("USBHDFSD: Error - Additional initalization is required for this device!\n");
                if ((stat = cbw_scsi_start_stop_unit(dev)) != 0) {
                    XPRINTF("USBHDFSD: Error - cbw_scsi_start_stop_unit %d\n", stat);
                    return -1;
                }
            }
        }
    }

    if ((stat = cbw_scsi_read_capacity(dev, &rcd, sizeof(read_capacity_data))) != 0) {
        XPRINTF("USBHDFSD: Error - cbw_scsi_read_capacity %d\n", stat);
        return -1;
    }

    dev->sectorSize = getBI32(&rcd.block_length);
    dev->maxLBA = getBI32(&rcd.last_lba);
    XPRINTF("USBHDFSD: sectorSize %u maxLBA %u\n", dev->sectorSize, dev->maxLBA);

    if (dev->sectorSize > 2048) {
        CpuSuspendIntr(&OldState);
        gSectorBuffer = AllocSysMemory(ALLOC_FIRST, dev->sectorSize, NULL);
        CpuResumeIntr(OldState);
    } else {
        gSectorBuffer = NULL;
    }

    return 0;
}

int mass_stor_configureDevice(void)
{
    XPRINTF("USBHDFSD: configuring device... \n");

    mass_dev *dev = &g_mass_device;
    if (dev->devId != -1 && (dev->status & DEVICE_DETECTED) && !(dev->status & DEVICE_CONFIGURED)) {
        int ret;
        if ((ret = usb_set_configuration(dev, dev->configId)) != USB_RC_OK) {
            XPRINTF("USBHDFSD: Error - sending set_configuration %d\n", ret);
            mass_stor_release(dev);
            return -1;
        }

        if ((ret = usb_set_interface(dev, dev->interfaceNumber, dev->interfaceAlt)) != USB_RC_OK) {
            XPRINTF("USBHDFSD: Error - sending set_interface %d\n", ret);
            mass_stor_release(dev);
            return -1;
        }

        dev->status |= DEVICE_CONFIGURED;

        ret = mass_stor_warmup(dev);
        if (ret < 0) {
            XPRINTF("USBHDFSD: Error - failed to warmup device %d\n", dev->devId);
            mass_stor_release(dev);
            return -1;
        }

        return 1;
    }

    return 0;
}

int mass_stor_init(void)
{
    register int ret;

    g_mass_device.devId = -1;

    iop_sema_t smp;
    smp.initial = 1;
    smp.max = 1;
    smp.option = 0;
    smp.attr = SA_THPRI;
    io_sema = CreateSema(&smp);

    driver.next = NULL;
    driver.prev = NULL;
    driver.name = "mass-stor";
    driver.probe = mass_stor_probe;
    driver.connect = mass_stor_connect;
    driver.disconnect = mass_stor_disconnect;

    ret = pUsbRegisterDriver(&driver);
    XPRINTF("mass_driver: registerDriver=%i \n", ret);
    if (ret < 0) {
        XPRINTF("mass_driver: register driver failed! ret=%d\n", ret);
        return -1;
    }

    return 0;
}

/*	There are many times of sector sizes for the many devices out there. But because:
	1. USB transfers have a limit of 4096 bytes (2 CD/DVD sectors).
	2. we need to keep things simple to save IOP clock cycles and RAM
	...devices with sector sizes that are either a multiple or factor of the CD/DVD sector will be supported.

	i.e. 512 (typical), 1024, 2048 and 4096 bytes can be supported, while 3072 bytes (if there's even such a thing) will be unsupported.
	Devices with sector sizes above 4096 bytes are unsupported.	*/
int mass_stor_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num)
{
    u32 sectors, nbytes, DiskSectorsToRead, lba;
    u8 *p = (u8 *)buf;

    if (g_mass_device.sectorSize > 2048) {
        //4096-byte sectors
        lba = cdvdman_settings.LBAs[part_num] + (lsn / 2);
        if ((lsn % 2) > 0) {
            //Start sector is in the middle of a physical sector.
            mass_stor_readSector(lba, 1, gSectorBuffer);
            mips_memcpy(p, &((unsigned char *)gSectorBuffer)[2048], 2048);
            lba++;
            lsn++;
            nsectors--;
            p += 2048;
        }
    } else {
        lba = cdvdman_settings.LBAs[part_num] + (lsn * (2048 / g_mass_device.sectorSize));
    }

    while (nsectors > 0) {
        sectors = nsectors;

        nbytes = sectors * 2048;
        DiskSectorsToRead = nbytes / g_mass_device.sectorSize;

        if (DiskSectorsToRead == 0) {
            // nsectors should be 1 at this point.
            mass_stor_readSector(lba, 1, gSectorBuffer);
            mips_memcpy(p, gSectorBuffer, 2048);
        } else {
            mass_stor_readSector(lba, DiskSectorsToRead, p);
        }

        lba += DiskSectorsToRead;
        lsn += sectors;
        p += nbytes;
        nsectors -= sectors;
    }

    return 1;
}
