#ifndef _MASS_STOR_H
#define _MASS_STOR_H 1

struct _mass_dev
{
    int controlEp;                //config endpoint id
    int bulkEpI;                  //in endpoint id
    int bulkEpO;                  //out endpoint id
    unsigned short int packetSzI; //packet size in
    unsigned short int packetSzO; //packet size out
    int devId;                    //device id
    unsigned char configId;       //configuration id
    unsigned char status;
    unsigned char interfaceNumber; //interface number
    unsigned char interfaceAlt;    //interface alternate setting
    unsigned int sectorSize;       // = 512; // store size of sector from usb mass
    unsigned int maxLBA;
    int ioSema;
};

int mass_stor_init(void);
int mass_stor_disconnect(int devId);
int mass_stor_connect(int devId);
int mass_stor_probe(int devId);
void mass_stor_readSector(unsigned int lba, unsigned short int nsectors, unsigned char *buffer);
void mass_stor_writeSector(unsigned int lba, unsigned short int nsectors, const unsigned char *buffer);
int mass_stor_configureDevice(void);
int mass_stor_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num);
int mass_stor_stop_unit(void);

#endif
