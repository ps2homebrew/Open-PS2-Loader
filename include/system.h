#ifndef __SYSTEM_H
#define __SYSTEM_H

#ifdef VMC
#include "include/mcemu.h"
#endif

#define SYS_LOAD_MC_MODULES		0x01
#define SYS_LOAD_PAD_MODULES	0x02

void delay(int count);
unsigned int USBA_crc32(char *string);
int sysGetDiscID(char *discID);
void sysReset(int modload_mask);
void sysExecExit();
void sysPowerOff(void);
int sysPcmciaCheck(void);
#ifdef VMC
void sysLaunchLoaderElf(char *filename, char *mode_str, int size_cdvdman_irx, void **cdvdman_irx, int size_mcemu_irx, void **mcemu_irx, unsigned int compatflags);
#else
void sysLaunchLoaderElf(char *filename, char *mode_str, int size_cdvdman_irx, void **cdvdman_irx, unsigned int compatflags);
#endif
int sysExecElf(char *path);
int sysPS3Detect(void);
int sysSetIPConfig(char* ipconfig);
int sysLoadModuleBuffer(void *buffer, int size, int argc, char *argv);
int sysCheckMC(void);
#ifdef VMC
int sysCheckVMC(const char* prefix, const char* sep, char* name, int createSize, vmc_superblock_t* vmc_superblock);
#endif

#endif
