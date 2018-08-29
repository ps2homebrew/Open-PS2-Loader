void DeviceInit(void);     //Called in _start()
void DeviceDeinit(void);   //Called in _exit(). Interrupts are disabled.

void DeviceFSInit(void);   //Called when the filesystem layer is initialized
void DeviceUnmount(void);  //Called when OPL is shutting down.

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors);
