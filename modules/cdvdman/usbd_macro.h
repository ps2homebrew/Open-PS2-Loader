/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: usbd_macro.h 1600 2009-08-11 02:42:17Z jim $
# USB Driver function prototypes and constants.
*/

#if !defined(_USBD_MACRO_H)
#define _USBD_MACRO_H

// !!! usbd exports functions pointers !!!
extern int (*pUsbRegisterDriver)(UsbDriver *driver); 								// #4
extern void *(*pUsbGetDeviceStaticDescriptor)(int devId, void *data, u8 type); 					// #6
extern int (*pUsbSetDevicePrivateData)(int devId, void *data); 							// #7
extern int (*pUsbOpenEndpoint)(int devId, UsbEndpointDescriptor *desc); 					// #9
extern int (*pUsbCloseEndpoint)(int id); 									// #10
extern int (*pUsbTransfer)(int id, void *data, u32 len, void *option, UsbCallbackProc callback, void *cbArg); 	// #11
extern int (*pUsbOpenEndpointAligned)(int devId, UsbEndpointDescriptor *desc); 					// #12

static int UsbControlTransfer(int epID, int reqtyp, int req, int val, int index, int leng, void *dataptr, void *doneCB, void* arg)

{
  UsbDeviceRequest devreq; 
  devreq.requesttype = reqtyp;
  devreq.request = req;
  devreq.value = val;
  devreq.index = index;
  devreq.length = leng;

  return pUsbTransfer(epID, dataptr, devreq.length, &devreq, doneCB, arg);
}



/*
#define UsbControlTransfer(epID, reqtyp, req, val, index, len, dataptr, doneCB, arg) \
 ({ \ 
	UsbDeviceRequest devreq; \ 
	devreq.requesttype = (reqtyp); \ 
	devreq.request = (req); \ 
	devreq.value = (val); \ 
	devreq.index = (index); \ 
	devreq.length = (len); \ 
	UsbTransfer((epID), (dataptr), devreq.length, &devreq, (doneCB), (arg)); \ 
	}) 
*/

#define UsbIsochronousTransfer(epID, dataptr, len, delta, doneCB, arg) \
	pUsbTransfer((epID), (dataptr), (len), (void *)(delta), (doneCB), (arg))

#define UsbBulkTransfer(epID, dataptr, len, doneCB, arg) \
	pUsbTransfer((epID), (dataptr), (len), NULL, (doneCB), (arg))

#define UsbInterruptTransfer(epID, dataptr, len, doneCB, arg) \
	pUsbTransfer((epID), (dataptr), (len), NULL, (doneCB), (arg))

/* standard control transfers */

#define UsbClearDeviceFeature(epID, feature, doneCB, arg)	\
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_DEVICE, USB_REQ_CLEAR_FEATURE, \
	(feature), 0, 0, NULL, (doneCB), (arg))

#define UsbSetDeviceFeature(epID, feature, doneCB, arg)	\
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_DEVICE, USB_REQ_SET_FEATURE, \
	(feature), 0, 0, NULL, (doneCB), (arg))

#define UsbGetDeviceConfiguration(epID, dataptr, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_DEVICE, USB_REQ_GET_CONFIGURATION, \
	0, 0, 1, (dataptr), (doneCB), (arg))

#define UsbSetDeviceConfiguration(epID, config, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_DEVICE, USB_REQ_SET_CONFIGURATION, (config), 0, 0, NULL, (doneCB), (arg))

#define UsbGetDeviceDescriptor(epID, type, index, language, dataptr, len, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_DEVICE, USB_REQ_GET_DESCRIPTOR, \
	((type) << 8) | (index), (language), (len), (dataptr), (doneCB), (arg))

#define UsbSetDeviceDescriptor(epID, type, index, language, dataptr, len, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_DEVICE, USB_REQ_SET_DESCRIPTOR, \
	((type) << 8) | (index), (language), (len), (dataptr), (doneCB), (arg))

#define UsbGetDeviceStatus(epID, dataptr, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_DEVICE, USB_REQ_GET_STATUS, \
	0, 0, 2, (dataptr), (doneCB), (arg))

#define UsbSetDeviceAddress(epID, address, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_DEVICE, USB_REQ_SET_ADDRESS, \
	(address), 0, 0, NULL, (doneCB), (arg))

#define UsbClearInterfaceFeature(epID, feature, interface, doneCB, arg)	\
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_INTERFACE, USB_REQ_CLEAR_FEATURE, \
	(feature), (interface), 0, NULL, (doneCB), (arg))

#define UsbSetInterfaceFeature(epID, feature, interface, doneCB, arg)	\
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_INTERFACE, USB_REQ_SET_FEATURE, \
	(feature), (interface), 0, NULL, (doneCB), (arg))

#define UsbGetInterface(epID, interface, dataptr, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_INTERFACE, USB_REQ_GET_INTERFACE, \
	0, (interface), 1, (dataptr), (doneCB), (arg))

#define UsbSetInterface(epID, interface, alt_setting, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_INTERFACE, USB_REQ_SET_INTERFACE, \
	(alt_setting), (interface), 0, NULL, (doneCB), (arg))

#define UsbGetInterfaceDescriptor(epID, type, index, language, dataptr, len, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_INTERFACE, USB_REQ_GET_DESCRIPTOR, \
	((type) << 8) | (index), (language), (len), (dataptr), (doneCB), (arg))

#define UsbSetInterfaceDescriptor(epID, type, index, language, dataptr, len, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_INTERFACE, USB_REQ_SET_DESCRIPTOR, \
	((type) << 8) | (index), (language), (len), (dataptr), (doneCB), (arg))

#define UsbGetInterfaceStatus(epID, interface, dataptr, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_INTERFACE, USB_REQ_GET_STATUS, \
	0, (interface), 2, (dataptr), (doneCB), (arg))

#define UsbClearEndpointFeature(epID, feature, endpoint, doneCB, arg)	\
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE, \
	(feature), (endpoint), 0, NULL, (doneCB), (arg))

#define UsbSetEndpointFeature(epID, feature, endpoint, doneCB, arg)	\
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_ENDPOINT, USB_REQ_SET_FEATURE, \
	(feature), (endpoint), 0, NULL, (doneCB), (arg))

#define UsbGetEndpointStatus(epID, endpoint, dataptr, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_ENDPOINT, USB_REQ_GET_STATUS, \
	0, (endpoint), 2, (dataptr), (doneCB), (arg))

#define UsbGetEndpointDescriptor(epID, type, index, language, dataptr, len, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_ENDPOINT, USB_REQ_GET_DESCRIPTOR, \
	((type) << 8) | (index), (language), (len), (dataptr), (doneCB), (arg))

#define UsbSetEndpointDescriptor(epID, type, index, language, dataptr, len, doneCB, arg) \
	UsbControlTransfer((epID), USB_DIR_OUT | USB_RECIP_ENDPOINT, USB_REQ_SET_DESCRIPTOR, \
	((type) << 8) | (index), (language), (len), (dataptr), (doneCB), (arg))

#define UsbSynchEndpointFrame(epID, endpoint, pfn, doneCB, arg)	\
	UsbControlTransfer((epID), USB_DIR_IN | USB_RECIP_ENDPOINT, USB_REQ_SYNCH_FRAME, \
	0, (endpoint), 2, (pfn), (doneCB), (arg))

#endif
