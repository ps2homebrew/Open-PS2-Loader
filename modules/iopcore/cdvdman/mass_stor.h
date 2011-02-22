#ifndef _MASS_STOR_H
#define _MASS_STOR_H 1

struct _mass_dev
{
	int controlEp;			//config endpoint id
	int bulkEpI;			//in endpoint id
	unsigned char bulkEpIAddr; 	// in endpoint address
	int bulkEpO;			//out endpoint id
	unsigned char bulkEpOAddr; 	// out endpoint address
	int packetSzI;			//packet size in
	int packetSzO;			//packet size out
	int devId;			//device id
	int configId;			//configuration id
	int status;
	int interfaceNumber;		//interface number
	int interfaceAlt;		//interface alternate setting
	unsigned sectorSize; 		// = 512; // store size of sector from usb mass
	unsigned maxLBA;
	void* cache;
};

int mass_stor_init(void);
int mass_stor_disconnect(int devId);
int mass_stor_connect(int devId);
int mass_stor_probe(int devId);
int mass_stor_readSector(unsigned int lba, int nsectors, unsigned char* buffer);
int mass_stor_writeSector(unsigned int lba, int nsectors, unsigned char* buffer);
int mass_stor_configureDevice(void);
int mass_stor_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num);

#endif
