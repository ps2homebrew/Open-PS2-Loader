#ifndef _MASS_STOR_H
#define _MASS_STOR_H 1

struct _mass_dev
{
	int controlEp;		//config endpoint id
	int bulkEpI;		//in endpoint id
	unsigned char bulkEpIAddr; // in endpoint address
	int bulkEpO;		//out endpoint id
	unsigned char bulkEpOAddr; // out endpoint address
	int packetSzI;		//packet size in
	int packetSzO;		//packet size out
	int devId;		//device id
	int configId;	//configuration id
	int status;
	int interfaceNumber;	//interface number
	int interfaceAlt;	//interface alternate setting
	unsigned sectorSize; // = 512; // store size of sector from usb mass
	unsigned maxLBA;
	cache_set* cache;
};

int InitUSB(void);
int mass_stor_disconnect(int devId);
int mass_stor_connect(int devId);
int mass_stor_probe(int devId);
int mass_stor_readSector(mass_dev* mass_device, unsigned int sector, unsigned char* buffer, int size);
int mass_stor_writeSector(mass_dev* mass_device, unsigned int sector, unsigned char* buffer, int size);
int mass_stor_configureNextDevice(void);

#endif
