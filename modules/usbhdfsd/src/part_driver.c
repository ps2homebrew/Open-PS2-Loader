//---------------------------------------------------------------------------
//File name:    part_driver.c
//---------------------------------------------------------------------------
#include <stdio.h>

#include "usbhd_common.h"
#include "scache.h"
#include "part_driver.h"
#include "mass_stor.h"
#include "fat_driver.h"

//#define DEBUG  //comment out this line when not debugging

#include "mass_debug.h"

#define READ_SECTOR(d, a, b)	scache_readSector((d)->cache, (a), (void **)&b)

typedef struct _part_record {
    unsigned char sid;          //system id - 4=16bit FAT (16bit sector numbers)
                                //            5=extended partition
                                //            6=16bit FAT (32bit sector numbers)
    unsigned int  start;        // start sector of the partition
    unsigned int  count;        // length of the partititon (total number of sectors)
} part_record;

typedef struct _part_table {
    part_record  record[4];        //maximum of 4 primary partitions
} part_table;

typedef struct _part_raw_record {
    unsigned char    active;        //Set to 80h if this partition is active / bootable
    unsigned char    startH;        //Partition's starting head.
    unsigned char    startST[2];    //Partition's starting sector and track.
    unsigned char    sid;        //Partition's system ID number.
    unsigned char    endH;        //Partition's ending head.
    unsigned char    endST[2];    //Partition's ending sector and track.
    unsigned char    startLBA[4];    //Starting LBA (sector)
    unsigned char    size[4];    //Partition size in sectors.
} part_raw_record;

//---------------------------------------------------------------------------
USBHD_INLINE void part_getPartitionRecord(part_raw_record* raw, part_record* rec)
{
    rec->sid = raw->sid;
    rec->start = getI32(raw->startLBA);
    rec->count = getI32(raw->size);
}

//---------------------------------------------------------------------------
int part_getPartitionTable(mass_dev* dev, part_table* part)
{
    part_raw_record* part_raw;
    int              i;
    int              ret;
    unsigned char* sbuf;

    ret = READ_SECTOR(dev, 0, sbuf);  // read sector 0 - Disk MBR or boot sector
    if ( ret < 0 )
    {
        printf("USBHDFSD: part_getPartitionTable read failed %d!\n", ret);
        return -1;
    }

    printf("USBHDFSD: boot signature %X %X\n", sbuf[0x1FE], sbuf[0x1FF]);
    if (sbuf[0x1FE] == 0x55 && sbuf[0x1FF] == 0xAA)
    {
        for ( i = 0; i < 4; i++)
        {
            part_raw = ( part_raw_record* )(  sbuf + 0x01BE + ( i * 16 )  );
            part_getPartitionRecord(part_raw, &part->record[i]);
        }
        return 4;
    }
    else
    {
        for ( i = 0; i < 4; i++)
        {
            part->record[i].sid = 0;;
        }
        return 0;
    }
}

//---------------------------------------------------------------------------
int part_connect(mass_dev* dev)
{
    part_table partTable;
    int count = 0;
    int i;
    XPRINTF("USBHDFSD: part_connect devId %i \n", dev->devId);

    if (part_getPartitionTable(dev, &partTable) < 0)
        return -1;
    
    for ( i = 0; i < 4; i++)
    {
        if(
            partTable.record[ i ].sid == 6    ||
            partTable.record[ i ].sid == 4    ||
            partTable.record[ i ].sid == 1    ||  // fat 16, fat 12
            partTable.record[ i ].sid == 0x0B ||
            partTable.record[ i ].sid == 0x0C ||  // fat 32
            partTable.record[ i ].sid == 0x0E)    // fat 16 LBA
        {
            XPRINTF("USBHDFSD: mount partition %d\n", i);
            if (fat_mount(dev, partTable.record[i].start, partTable.record[i].count) >= 0)
                count++;
        }
    }

    if ( count == 0 )
    {  // no partition table detected
        // try to use "floppy" option
        printf("USBHDFSD: mount drive\n");
        if (fat_mount(dev, 0, dev->maxLBA) < 0)
            return -1;
    }

    return 0;
}

//---------------------------------------------------------------------------
void part_disconnect(mass_dev* dev)
{
    printf("USBHDFSD: part_disconnect devId %i \n", dev->devId);
    fat_forceUnmount(dev);
}

//---------------------------------------------------------------------------
//End of file:  part_driver.c
//---------------------------------------------------------------------------
