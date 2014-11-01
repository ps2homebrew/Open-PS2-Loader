/*
 * usb_mass.c - USB Mass storage driver for PS2
 */

#define MAJOR_VER 1
#define MINOR_VER 6

#include <loadcore.h>
#include <intrman.h>
#include <stdio.h>
#include <sysmem.h>
#include <usbhdfsd.h>

#include <irx.h>

IRX_ID("usbhdfsd", MAJOR_VER, MINOR_VER);

extern int InitFAT();
extern int InitFS();
extern int InitUSB();

extern struct irx_export_table _exp_usbmass;

int _start( int argc, char *argv[])
{
	printf("USB HDD FileSystem Driver v%d.%d\n", MAJOR_VER, MINOR_VER);

	if (RegisterLibraryEntries(&_exp_usbmass) != 0) {
		printf("USBHDFSD: Already registered.\n");
		return MODULE_NO_RESIDENT_END;
	}

	// initialize the FAT driver
	if(InitFAT() != 0)
	{
		printf("USBHDFSD: Error initializing FAT driver!\n");
		return MODULE_NO_RESIDENT_END;
	}

	// initialize the USB driver
	if(InitUSB() != 0)
	{
		printf("USBHDFSD: Error initializing USB driver!\n");
		return MODULE_NO_RESIDENT_END;
	}

	// initialize the file system driver
	if(InitFS() != 0)
	{
		printf("USBHDFSD: Error initializing FS driver!\n");
		return MODULE_NO_RESIDENT_END;
	}

	// return resident
	return MODULE_RESIDENT_END;
}

#ifndef WIN32
void *malloc(int size){
	void *result;
	int OldState;

	CpuSuspendIntr(&OldState);
	result = AllocSysMemory(ALLOC_FIRST, size, NULL);
	CpuResumeIntr(OldState);

	return result;
}

void free(void *ptr){
	int OldState;

	CpuSuspendIntr(&OldState);
	FreeSysMemory(ptr);
	CpuResumeIntr(OldState);
}
#endif
