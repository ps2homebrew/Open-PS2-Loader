// different devices need different cache configurations for better performance and compatibility
#define DBM_CACHE_SIZE 8 // enough to have stable IO
#define HDD_CACHE_SIZE 0 // not needed for HDD
#define SMB_CACHE_SIZE 4 // should be as low as possible

void DeviceInit(void);   //Called in _start()
void DeviceDeinit(void); //Called in _exit(). Interrupts are disabled.
int DeviceReady(void);   //Check if device is ready

void DeviceFSInit(void);  //Called when the filesystem layer is initialized
void DeviceLock(void);    //Prevents further accesses to the device.
void DeviceUnmount(void); //Called when OPL is shutting down.
void DeviceStop(void);    //Called before the PS2 is to be shut down.

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors);
u8 DeviceGetCacheSize();