void DeviceInit(void);		//Called in _start()
void DeviceDeinit(void);	//Called in _exit()

void DeviceFSInit(void);	//Called when the filesystem layer is initialized

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors);
