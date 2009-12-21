/*
 * usb_driver - USB Mass Storage Driver code
 */

#include <sysclib.h>
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

extern int *p_part_start;

#define getBI32(__buf) ((((u8 *) (__buf))[3] << 0) | (((u8 *) (__buf))[2] << 8) | (((u8 *) (__buf))[1] << 16) | (((u8 *) (__buf))[0] << 24))

#define USB_SUBCLASS_MASS_RBC			0x01
#define USB_SUBCLASS_MASS_ATAPI			0x02
#define USB_SUBCLASS_MASS_QIC			0x03
#define USB_SUBCLASS_MASS_UFI			0x04
#define USB_SUBCLASS_MASS_SFF_8070I 	0x05
#define USB_SUBCLASS_MASS_SCSI			0x06

#define USB_PROTOCOL_MASS_CBI			0x00
#define USB_PROTOCOL_MASS_CBI_NO_CCI	0x01
#define USB_PROTOCOL_MASS_BULK_ONLY		0x50

#define TAG_TEST_UNIT_READY     0
#define TAG_REQUEST_SENSE	3
#define TAG_INQUIRY		18
#define TAG_READ_CAPACITY       37
#define TAG_READ		40
#define TAG_START_STOP_UNIT	33
#define TAG_WRITE		42

#define DEVICE_DETECTED		1
#define DEVICE_CONFIGURED	2

#define CBW_TAG 0x43425355
#define CSW_TAG 0x53425355

typedef struct _cbw_packet
{
	unsigned int signature;
	unsigned int tag;
	unsigned int dataTransferLength;
	unsigned char flags;            //80->data in,  00->out
	unsigned char lun;
	unsigned char comLength;		//command data length
	unsigned char comData[16];		//command data
} cbw_packet __attribute__((packed));

static cbw_packet cbw_test_unit_ready = {
	CBW_TAG,
	-TAG_TEST_UNIT_READY,
	0,					//TUR_REPLY_LENGTH
	0x80,
	0,
	12,
	{ 0x00, 			//test unit ready operation code
		 0, 			//lun/reserved
		 0 ,0 ,0 ,0 ,	//reserved		 
		 0 ,0 , 0 ,		//reserved
		 0 ,0 ,0 		//reserved
	}
};

static cbw_packet cbw_request_sense = {
	CBW_TAG,
	-TAG_REQUEST_SENSE,
	0,
	0x80,
	0,
	12,
	{ 0x03, 			//request sense operation code
		 0, 			//lun/reserved
		 0 ,0 , 		//reserved
		 0 ,			//allocation length
		 0 ,0 ,0 ,0 ,	//reserved
		 0 ,0 ,0 		//reserved
	}
};

static cbw_packet cbw_inquiry = {
	CBW_TAG,
	-TAG_INQUIRY,
	0,
	0x80,
	0,
	12,
	{ 0x12, 			//inquiry operation code
		 0, 			//lun/reserved
		 0,		 		//page code
		 0 , 			//reserved
		 0 ,			//inquiry reply length
		 0 ,0 ,0 ,0 ,	//reserved
		 0 ,0 ,0 		//reserved
	}
};

static cbw_packet cbw_start_stop_unit = {
	CBW_TAG,
	-TAG_START_STOP_UNIT,
	0,					//START_STOP_REPLY_LENGTH
	0x80,
	0,
	12,
	{ 0x1B, 			//start stop unit operation code
		 1, 			//lun/reserved/immed
		 0, 0 , 		//reserved
		 1 , 			//reserved/LoEj/Start "Start the media and acquire the format type"
		 0 ,0 ,0 ,0 ,	//reserved
		 0 ,0 ,0 		//reserved
	}
};

static cbw_packet cbw_read_capacity = {
	CBW_TAG,
	-TAG_READ_CAPACITY,
	0,
	0x80,
	0,
	12,
	{ 0x25, 			//read capacity operation code
		 0, 			//lun/reserved/RelAdr
		 0, 0 ,0 ,0 , 	//lba
		 0 , 			//reserved
		 0 ,0 , 		//reserved/PMI
		 0 ,0 ,0 		//reserved
	}
};

static cbw_packet cbw_read_sector = {
	CBW_TAG,
	-TAG_READ,
	0,
	0x80,
	0,
	12,
	{ 0x28, 			//read operation code
		 0, 			//LUN/DPO/FUA/Reserved/Reldr
		 0, 0 ,0 ,0 , 	//lba
		 0 , 			//reserved
		 0 ,0 , 		//Transfer length MSB
		 0 ,0 ,0 		//reserved
	}
};

typedef struct _csw_packet
{
	unsigned int signature;
	unsigned int tag;
	unsigned int dataResidue;
	unsigned char status;
} csw_packet __attribute__((packed));

typedef struct _inquiry_data
{
    u8 peripheral_device_type;  // 00h - Direct access (Floppy), 1Fh none (no FDD connected)
    u8 removable_media; // 80h - removeable
    u8 iso_ecma_ansi;
    u8 repsonse_data_format;
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

//volatile int wait_for_connect = 1;

static int cb_sema;

typedef struct _usb_callback_data
{
    int returnCode;
    int returnSize;
} usb_callback_data;

static mass_dev g_mass_device;


static void usb_callback(int resultCode, int bytes, void *arg)
{
	usb_callback_data* data = (usb_callback_data*) arg;
	data->returnCode = resultCode;
	data->returnSize = bytes;
	XPRINTF("mass_driver: callback: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
	SignalSema(cb_sema);
}

static void usb_set_configuration(mass_dev* dev, int configNumber)
{
	register int ret;
	usb_callback_data cb_data;

	XPRINTF("mass_driver: setting configuration controlEp=%i, confNum=%i \n", dev->controlEp, configNumber);
	ret = UsbSetDeviceConfiguration(dev->controlEp, configNumber, usb_callback, (void*)&cb_data);

	if (ret != USB_RC_OK) {
		XPRINTF("mass_driver: Error - sending set_configuration %d\n", ret);
	} else {
		WaitSema(cb_sema);
	}
}

static void usb_set_interface(mass_dev* dev, int interface, int altSetting)
{
	register int ret;
	usb_callback_data cb_data;

	XPRINTF("mass_driver: setting interface controlEp=%i, interface=%i altSetting=%i\n", dev->controlEp, interface, altSetting);
	ret = UsbSetInterface(dev->controlEp, interface, altSetting, usb_callback, (void*)&cb_data);

	if (ret != USB_RC_OK) {
		XPRINTF("mass_driver: Error - sending set_interface %d\n", ret);
	} else {
		WaitSema(cb_sema);
	}
}

static void usb_bulk_clear_halt(mass_dev* dev, int direction)
{
	register int ret, endpoint;
	usb_callback_data cb_data;

	if (direction == 0) {
		endpoint = dev->bulkEpIAddr;
		//endpoint = dev->bulkEpI;
	} else {
		endpoint = dev->bulkEpOAddr;
	}

	ret = UsbClearEndpointFeature(
		dev->controlEp, //Config pipe
		0,				//HALT feature
		endpoint,
		usb_callback,
		(void*)&cb_data
		);

	if (ret != USB_RC_OK) {
		XPRINTF("mass_driver: Error - sending clear halt %d\n", ret);
	} else {
		WaitSema(cb_sema);
	}
}

static void usb_bulk_reset(mass_dev* dev, int mode)
{
	register int ret;
	usb_callback_data cb_data;

	//Call Bulk only mass storage reset
	ret = UsbControlTransfer(
		dev->controlEp, 		//default pipe
		0x21,					//bulk reset
		0xFF,
		0,
		dev->interfaceNumber, 	//interface number
		0,						//length
		NULL,					//data
		usb_callback,
		(void*) &cb_data
		);

	if (ret != USB_RC_OK) {
		XPRINTF("mass_driver: Error - sending reset %d\n", ret);
	} else {
		WaitSema(cb_sema);

		//clear bulk-in endpoint
		if (mode & 0x01) {
			usb_bulk_clear_halt(dev, 0);
		}

		//clear bulk-out endpoint
		if (mode & 0x02) {
			usb_bulk_clear_halt(dev, 1);
		}
	}
}

static int usb_bulk_status(mass_dev* dev, csw_packet* csw, int tag)
{
	register int ret;
	usb_callback_data cb_data;

	csw->signature = CSW_TAG;
	csw->tag = tag;
	csw->dataResidue = 0;
	csw->status = 0;

	ret = UsbBulkTransfer(
		dev->bulkEpI,		//bulk input pipe
		csw,			//data ptr
		13,			//data length
		usb_callback,
		(void*)&cb_data
		);

	if (ret != USB_RC_OK) {
		XPRINTF("mass_driver: Error - sending bulk status %d\n", ret);
		return -1;
	} else {
		WaitSema(cb_sema);
		if (cb_data.returnSize != 13)
			XPRINTF("mass_driver: bulk csw.status returnSize: %i\n", cb_data.returnSize);
		if (csw->dataResidue != 0)
			XPRINTF("mass_driver: bulk csw.status residue: %i\n", csw->dataResidue);
		XPRINTF("mass_driver: bulk csw.status: %i\n", csw->status);

		return csw->status;
	}
}

/* see flow chart in the usbmassbulk_10.pdf doc (page 15) */
int usb_bulk_manage_status(mass_dev* dev, int tag)
{
	register int ret;
	csw_packet csw;

	//XPRINTF("mass_driver: usb_bulk_manage_status 1 ...\n");
	ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
	if (ret < 0) { /* STALL bulk in  -OR- Bulk error */
		usb_bulk_clear_halt(dev, 0); /* clear the stall condition for bulk in */

		XPRINTF("mass_driver: usb_bulk_manage_status stall ...\n");
		ret = usb_bulk_status(dev, &csw, tag); /* Attempt to read CSW from bulk in endpoint */
	}

	/* CSW not valid  or stalled or phase error */
	if (csw.signature != CSW_TAG || csw.tag != tag || ret == 2) {
		XPRINTF("mass_driver: usb_bulk_manage_status call reset recovery ...\n");
		usb_bulk_reset(dev, 3);	/* Perform reset recovery */
	}

	return ret;
}

static int usb_bulk_get_max_lun(mass_dev* dev)
{
	register int ret;
	usb_callback_data cb_data;
	char max_lun;

	//Call Bulk only mass storage reset
	ret = UsbControlTransfer(
		dev->controlEp, 		//default pipe
		0xA1,
		0xFE,
		0,
		dev->interfaceNumber, 	//interface number
		1,						//length
		&max_lun,				//data
		usb_callback,
		(void*) &cb_data
		);

	if (ret != USB_RC_OK) {
		XPRINTF("mass_driver: Error - get_max_lun %d\n", ret);
		max_lun = -ret;
	} else {
		WaitSema(cb_sema);
	}

	return max_lun;
}

static void usb_bulk_command(mass_dev* dev, cbw_packet* packet )
{
	register int ret;
	usb_callback_data cb_data;

	ret =  UsbBulkTransfer(
		dev->bulkEpO,	//bulk output pipe
		packet,			//data ptr
		31,				//data length
		usb_callback,
		(void*)&cb_data
	);

	if (ret != USB_RC_OK) {
		XPRINTF("mass_driver: Error - sending bulk command %d\n", ret);
	} else {
		WaitSema(cb_sema);
	}
}

static int usb_bulk_transfer(int pipe, void* buffer, int transferSize)
{
	register int ret;
	char* buf = (char*) buffer;
	register int blockSize = transferSize;
	register int offset = 0;
	usb_callback_data cb_data;

	while (transferSize > 0) {
		if (transferSize < blockSize) {
			blockSize = transferSize;
		}

		ret = UsbBulkTransfer(
			pipe,			//bulk pipe epI(Read)  epO(Write)
			(buf + offset),	//data ptr
			blockSize,		//data length
			usb_callback,
			(void*)&cb_data
			);
		if (ret != USB_RC_OK) {
			XPRINTF("mass_driver: Error - sending bulk data transfer %d\n", ret);
			cb_data.returnCode = -1;
			break;
		} else {
			WaitSema(cb_sema);
			//XPRINTF("mass_driver: retCode=%i retSize=%i \n", cb_data.returnCode, cb_data.returnSize);
			if (cb_data.returnCode > 0) {
				break;
			}
			offset += cb_data.returnSize;
			transferSize-= cb_data.returnSize;
		}
	}

	return cb_data.returnCode;
}

inline int cbw_scsi_test_unit_ready(mass_dev* dev)
{
	register int ret;
	cbw_packet *cbw = &cbw_test_unit_ready;

	XPRINTF("mass_driver: cbw_scsi_test_unit_ready\n");

	usb_bulk_command(dev, cbw);

	ret = usb_bulk_manage_status(dev, -TAG_TEST_UNIT_READY);

	return ret;
}

inline int cbw_scsi_request_sense(mass_dev* dev, void *buffer, int size)
{
	register int ret;
	cbw_packet *cbw = &cbw_request_sense;

	XPRINTF("mass_driver: cbw_scsi_request_sense\n");

	cbw->dataTransferLength = size;	//REQUEST_SENSE_REPLY_LENGTH
	cbw->comData[4] = size;			//allocation length

	usb_bulk_command(dev, cbw);

	ret = usb_bulk_transfer(dev->bulkEpI, buffer, size);
	if (ret != 0)
		XPRINTF("mass_driver: cbw_scsi_request_sense error from usb_bulk_transfer %d\n", ret);

	ret = usb_bulk_manage_status(dev, -TAG_REQUEST_SENSE);

	return ret;
}

inline int cbw_scsi_inquiry(mass_dev* dev, void *buffer, int size)
{
	register int ret;
	cbw_packet *cbw = &cbw_inquiry;

	XPRINTF("mass_driver: cbw_scsi_inquiry\n");

	cbw->dataTransferLength = size;	//INQUIRY_REPLY_LENGTH
	cbw->comData[4] = size;			//inquiry reply length

	usb_bulk_command(dev, cbw);

	ret = usb_bulk_transfer(dev->bulkEpI, buffer, size);
	if (ret != 0)
		XPRINTF("mass_driver: cbw_scsi_inquiry error from usb_bulk_transfer %d\n", ret);

	usb_bulk_manage_status(dev, -TAG_INQUIRY);

	return ret; // TODO What to return???
}

inline int cbw_scsi_start_stop_unit(mass_dev* dev)
{
	register int ret;
	cbw_packet *cbw = &cbw_start_stop_unit;

	XPRINTF("mass_driver: cbw_scsi_start_stop_unit\n");

	usb_bulk_command(dev, cbw);

	ret = usb_bulk_manage_status(dev, -TAG_START_STOP_UNIT);

	return ret;
}

inline int cbw_scsi_read_capacity(mass_dev* dev, void *buffer, int size)
{
	register int ret, retryCount;
	cbw_packet *cbw = &cbw_read_capacity;

	XPRINTF("mass_driver: cbw_scsi_read_capacity\n");

	cbw->dataTransferLength = size;	//READ_CAPACITY_REPLY_LENGTH

	ret = 1;
	retryCount = 6;
	while (ret != 0 && retryCount > 0) {
		usb_bulk_command(dev, cbw);

		ret = usb_bulk_transfer(dev->bulkEpI, buffer, size);
		if (ret != 0)
			XPRINTF("mass_driver: cbw_scsi_read_capacity error from usb_bulk_transfer %d\n", ret);

		//HACK HACK HACK !!!
		//according to usb doc we should allways
		//attempt to read the CSW packet. But in some cases
		//reading of CSW packet just freeze ps2....:-(
		if (ret == USB_RC_STALL) {
			XPRINTF("mass_driver: call reset recovery ...\n");
			usb_bulk_reset(dev, 1);
		} else {
			ret = usb_bulk_manage_status(dev, -TAG_READ_CAPACITY);
		}
		retryCount--;
	}

	return ret;
}

inline int cbw_scsi_read_sector(mass_dev* dev, unsigned int lba, void* buffer, int sectorSize, int sectorCount)
{
	register int ret;
	cbw_packet *cbw = &cbw_read_sector;

	XPRINTF("mass_driver: cbw_scsi_read_sector\n");

	cbw->dataTransferLength = sectorSize * sectorCount;
	cbw->comData[2] = (lba & 0xFF000000) >> 24;		//lba 1 (MSB)
	cbw->comData[3] = (lba & 0xFF0000) >> 16;		//lba 2
	cbw->comData[4] = (lba & 0xFF00) >> 8;			//lba 3
	cbw->comData[5] = (lba & 0xFF);					//lba 4 (LSB)
	cbw->comData[7] = (sectorCount & 0xFF00) >> 8;	//Transfer length MSB
	cbw->comData[8] = (sectorCount & 0xFF);			//Transfer length LSB

	usb_bulk_command(dev, cbw);

	ret = usb_bulk_transfer(dev->bulkEpI, buffer, sectorSize * sectorCount);
	if (ret != 0)
		XPRINTF("mass_driver: cbw_scsi_read_sector error from usb_bulk_transfer %d\n", ret);

	ret = usb_bulk_manage_status(dev, -TAG_READ);

	return ret;
}

/* size should be a multiple of sector size */
int mass_stor_readSector(unsigned int lba, int nsectors, unsigned char* buffer)
{
	mass_dev* mass_device = &g_mass_device;

	return cbw_scsi_read_sector(mass_device, lba, buffer, mass_device->sectorSize, nsectors);
}

/* test that endpoint is bulk endpoint and if so, update device info */
static void usb_bulk_probeEndpoint(int devId, mass_dev* dev, UsbEndpointDescriptor* endpoint)
{
	if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
		/* out transfer */
		if ((endpoint->bEndpointAddress & 0x80) == 0 && dev->bulkEpO < 0) {
			dev->bulkEpOAddr = endpoint->bEndpointAddress;
			dev->bulkEpO = pUsbOpenEndpointAligned(devId, endpoint);
			dev->packetSzO = endpoint->wMaxPacketSizeHB * 256 + endpoint->wMaxPacketSizeLB;
			XPRINTF("mass_driver: register Output endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpO,dev->bulkEpOAddr, dev->packetSzO);
		} else
		/* in transfer */
		if ((endpoint->bEndpointAddress & 0x80) != 0 && dev->bulkEpI < 0) {
			dev->bulkEpIAddr = endpoint->bEndpointAddress;
			dev->bulkEpI = pUsbOpenEndpointAligned(devId, endpoint);
			dev->packetSzI = endpoint->wMaxPacketSizeHB * 256 + endpoint->wMaxPacketSizeLB;
			XPRINTF("mass_driver: register Input endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpI, dev->bulkEpIAddr, dev->packetSzI);
		}
	}
}

int mass_stor_probe(int devId)
{
	UsbDeviceDescriptor *device = NULL;
	UsbConfigDescriptor *config = NULL;
	UsbInterfaceDescriptor *intf = NULL;

	XPRINTF("mass_driver: probe: devId=%i\n", devId);

	mass_dev* mass_device = &g_mass_device;

	/* only one device supported */
	if ((mass_device != NULL) && (mass_device->status & DEVICE_DETECTED)) {
		XPRINTF("mass_driver: Error - only one mass storage device allowed ! \n");
		return 0;
	}

	/* get device descriptor */
	device = (UsbDeviceDescriptor*)pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
	if (device == NULL)  {
		XPRINTF("mass_driver: Error - Couldn't get device descriptor\n");
		return 0;
	}

	/* Check if the device has at least one configuration */
	if (device->bNumConfigurations < 1) { return 0; }

	/* read configuration */
	config = (UsbConfigDescriptor*)pUsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
	if (config == NULL) {
		XPRINTF("mass_driver: Error - Couldn't get configuration descriptor\n");
		return 0;
	}
	/* check that at least one interface exists */
	XPRINTF("mass_driver: bNumInterfaces %d\n", config->bNumInterfaces);
	if ((config->bNumInterfaces < 1) ||
		(config->wTotalLength < (sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)))) {
			XPRINTF("mass_driver: Error - No interfaces available\n");
			return 0;
	}
	/* get interface */
	intf = (UsbInterfaceDescriptor *) ((char *) config + config->bLength); /* Get first interface */
	XPRINTF("mass_driver: bInterfaceClass %X bInterfaceSubClass %X bInterfaceProtocol %X\n",
		intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);

	if ((intf->bInterfaceClass		!= USB_CLASS_MASS_STORAGE) ||
		(intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SCSI  &&
		 intf->bInterfaceSubClass	!= USB_SUBCLASS_MASS_SFF_8070I) ||
		(intf->bInterfaceProtocol	!= USB_PROTOCOL_MASS_BULK_ONLY) ||
		(intf->bNumEndpoints < 2))    { //one bulk endpoint is not enough because
			return 0;		//we send the CBW to te bulk out endpoint
	}

	return 1;
}

int mass_stor_connect(int devId)
{
	register int i, epCount;
	UsbDeviceDescriptor *device;
	UsbConfigDescriptor *config;
	UsbInterfaceDescriptor *interface;
	UsbEndpointDescriptor *endpoint;
	mass_dev* dev;

	//wait_for_connect = 0;

	XPRINTF("mass_driver: connect: devId=%i\n", devId);
	dev = &g_mass_device;

	if (dev == NULL) {
		XPRINTF("mass_driver: Error - unable to allocate space!\n");
		return 1;
	}

	/* only one mass device allowed */
	if (dev->devId != -1) {
		XPRINTF("mass_driver: Error - only one mass storage device allowed !\n");
		return 1;
	}

	dev->status = 0;
	dev->sectorSize = 0;

	dev->bulkEpI = -1;
	dev->bulkEpO = -1;

	/* open the config endpoint */
	dev->controlEp = pUsbOpenEndpoint(devId, NULL);

	device = (UsbDeviceDescriptor*)pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);

	config = (UsbConfigDescriptor*)pUsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

	interface = (UsbInterfaceDescriptor *) ((char *) config + config->bLength); /* Get first interface */

	// store interface numbers
	dev->interfaceNumber = interface->bInterfaceNumber;
	dev->interfaceAlt    = interface->bAlternateSetting;

	epCount = interface->bNumEndpoints;
	endpoint = (UsbEndpointDescriptor*) pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
	usb_bulk_probeEndpoint(devId, dev, endpoint);

	for (i = 1; i < epCount; i++) {
		endpoint = (UsbEndpointDescriptor*) ((char *) endpoint + endpoint->bLength);
		usb_bulk_probeEndpoint(devId, dev, endpoint);
	}

	/* we do NOT have enough bulk endpoints */
	if (dev->bulkEpI < 0  /* || dev->bulkEpO < 0 */ ) { /* the bulkOut is not needed now */
		if (dev->bulkEpI >= 0) {
			pUsbCloseEndpoint(dev->bulkEpI);
		}
		if (dev->bulkEpO >= 0) {
			pUsbCloseEndpoint(dev->bulkEpO);
		}
		XPRINTF("mass_driver: Error - connect failed: not enough bulk endpoints! \n");
		return -1;
	}

	/*store current configuration id - can't call set_configuration here */
	dev->devId = devId;
	dev->configId = config->bConfigurationValue;
	dev->status = DEVICE_DETECTED;
	XPRINTF("mass_driver: connect ok: epI=%i, epO=%i \n", dev->bulkEpI, dev->bulkEpO);

	return 0;
}

void mass_stor_release(mass_dev *dev)
{
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
}

int mass_stor_disconnect(int devId)
{
	mass_dev* dev = &g_mass_device;

	XPRINTF("mass_driver: disconnect: devId=%i\n", devId);

	if (dev == NULL) {
		XPRINTF("mass_driver: Error - disconnect: no device storage!\n");
		return 0;
	}

	if ((dev->status & DEVICE_DETECTED) && devId == dev->devId) {
		mass_stor_release(dev);
		dev->devId = -1;
	}

	return 0;
}

int mass_stor_warmup(mass_dev *dev)
{
	inquiry_data id;
	sense_data sd;
	read_capacity_data rcd;
	register int stat;

	XPRINTF("mass_driver: mass_stor_warmup\n");

	if (!(dev->status & DEVICE_DETECTED)) {
		XPRINTF("mass_driver: Error - no mass storage device found!\n");
		return -1;
	}

	stat = usb_bulk_get_max_lun(dev);
	XPRINTF("mass_driver: usb_bulk_get_max_lun %d\n", stat);

	mips_memset(&id, 0, sizeof(inquiry_data));
	if ((stat = cbw_scsi_inquiry(dev, &id, sizeof(inquiry_data))) < 0) {
		XPRINTF("mass_driver: Error - cbw_scsi_inquiry %d\n", stat);
		return -1;
	}

	XPRINTF("mass_driver: Vendor: %.8s\n", id.vendor);
	XPRINTF("mass_driver: Product: %.16s\n", id.product);
	XPRINTF("mass_driver: Revision: %.4s\n", id.revision);

	while((stat = cbw_scsi_test_unit_ready(dev)) != 0) {
		XPRINTF("mass_driver: Error - cbw_scsi_test_unit_ready %d\n", stat);
        
		stat = cbw_scsi_request_sense(dev, &sd, sizeof(sense_data));
		if (stat != 0)
			XPRINTF("mass_driver: Error - cbw_scsi_request_sense %d\n", stat);

		if ((sd.error_code == 0x70) && (sd.sense_key != 0x00)) {
			XPRINTF("mass_driver: Sense Data key: %02X code: %02X qual: %02X\n", sd.sense_key, sd.add_sense_code, sd.add_sense_qual);

			if ((sd.sense_key == 0x02) && (sd.add_sense_code == 0x04) && (sd.add_sense_qual == 0x02)) {
				XPRINTF("mass_driver: Error - Additional initalization is required for this device!\n");
				if ((stat = cbw_scsi_start_stop_unit(dev)) != 0) {
					XPRINTF("mass_driver: Error - cbw_scsi_start_stop_unit %d\n", stat);
					return -1;
				}
			}
		}
	}

	stat = cbw_scsi_request_sense(dev, &sd, sizeof(sense_data));
	if (stat != 0)
		XPRINTF("mass_driver: Error - cbw_scsi_request_sense %d\n", stat);

	if ((sd.error_code == 0x70) && (sd.sense_key != 0x00)) {
		XPRINTF("mass_driver: Sense Data key: %02X code: %02X qual: %02X\n", sd.sense_key, sd.add_sense_code, sd.add_sense_qual);
	}

	if ((stat = cbw_scsi_read_capacity(dev, &rcd, sizeof(read_capacity_data))) != 0) {
		XPRINTF("mass_driver: Error - cbw_scsi_read_capacity %d\n", stat);
		return -1;
	}

	dev->sectorSize = getBI32(&rcd.block_length);
	dev->maxLBA     = getBI32(&rcd.last_lba);
	XPRINTF("mass_driver: sectorSize %d maxLBA %d\n", dev->sectorSize, dev->maxLBA);

	return 0;
}

int mass_stor_configureDevice(void)
{
	// give the USB driver some time to detect the device
	//i = 10000;
	//while (wait_for_connect && (--i > 0))
	//    DelayThread(100);
	//wait_for_connect = 0;

	XPRINTF("mass_driver: configuring devices... \n");

	mass_dev *dev = &g_mass_device;
	if (dev->devId != -1 && (dev->status & DEVICE_DETECTED) && !(dev->status & DEVICE_CONFIGURED)) {
		int ret;
		usb_set_configuration(dev, dev->configId);
		usb_set_interface(dev, dev->interfaceNumber, dev->interfaceAlt);
		dev->status |= DEVICE_CONFIGURED;

		ret = mass_stor_warmup(dev);
		if (ret < 0) {
			XPRINTF("mass_driver: Error - failed to warmup device %d\n", dev->devId);
			mass_stor_release(dev);
		}

		return 1;
	}

	return 0;
}

int mass_stor_init(void)
{
	iop_sema_t smp;
	register int ret;

	g_mass_device.devId = -1;

	smp.initial = 0;
	smp.max = 1;
	smp.option = 0;
	smp.attr = 0;
	cb_sema = CreateSema(&smp);

	driver.next 		= NULL;
	driver.prev		    = NULL;
	driver.name 		= "mass-stor";
	driver.probe 		= mass_stor_probe;
	driver.connect		= mass_stor_connect;
	driver.disconnect	= mass_stor_disconnect;

	ret = pUsbRegisterDriver(&driver);
	XPRINTF("mass_driver: registerDriver=%i \n", ret);
	if (ret < 0) {
		XPRINTF("mass_driver: register driver failed! ret=%d\n", ret);
		return -1;
	}

	return 0;
}

//-------------------------------------------------------------------------
int mass_stor_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num)
{
	register int r = 0;
	register u32 sectors, nbytes;
	u8 *p = (u8 *)buf;
	
	while (nsectors > 0) {
		sectors = nsectors;
		if (sectors > 2)
			sectors = 2;

		nbytes = sectors << 11;	
		mass_stor_readSector(p_part_start[part_num] + (lsn << 2), sectors << 2, p);
		//mass_stor_readSector(part_lba + (lsn << 2), 8, cdvdman_io_buf);
		//mips_memcpy(p, cdvdman_io_buf, nbytes);
		
		lsn += sectors;
		r += sectors;
		p += nbytes;
		nsectors -= sectors;
	}	

	return 1;
}
