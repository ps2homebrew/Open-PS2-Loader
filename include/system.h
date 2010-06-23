#ifndef __SYSTEM_H
#define __SYSTEM_H

void delay(int count);
unsigned int USBA_crc32(char *string);
int sysGetDiscID(char *discID);
void sysReset(void);
void sysPowerOff(void);
int sysPcmciaCheck(void);
void sysGetCDVDFSV(void **data_irx, int *size_irx);
void sysLaunchLoaderElf(char *filename, char *mode_str, int size_cdvdman_irx, void **cdvdman_irx, int compatflags, int alt_ee_core);
int sysExecElf(char *path, int argc, char **argv);
int sysPS3Detect(void);
int sysSetIPConfig(char* ipconfig);
int sysLoadModuleBuffer(void *buffer, int size, int argc, char *argv);
void sysApplyKernelPatches(void);

#endif
