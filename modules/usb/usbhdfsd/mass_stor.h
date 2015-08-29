#ifndef _MASS_STOR_H
#define _MASS_STOR_H 1

struct _mass_dev
{
	int controlEp;			//config endpoint id
	int bulkEpI;			//in endpoint id
	int bulkEpO;			//out endpoint id
	int devId;			//device id
	unsigned char configId;		//configuration id
	unsigned char status;
	unsigned char interfaceNumber;	//interface number
	unsigned char interfaceAlt;	//interface alternate setting
	unsigned int sectorSize;	// = 512; // store size of sector from usb mass
	unsigned int maxLBA;
	int ioSema;
	cache_set* cache;
	usbmass_cb_t callback;
};

int InitUSB(void);
int mass_stor_disconnect(int devId);
int mass_stor_connect(int devId);
int mass_stor_probe(int devId);
int mass_stor_readSector(mass_dev* mass_device, unsigned int sector, unsigned char* buffer, unsigned short int count);
int mass_stor_writeSector(mass_dev* mass_device, unsigned int sector, const unsigned char* buffer, unsigned short int count);
int mass_stor_configureNextDevice(void);

#endif
