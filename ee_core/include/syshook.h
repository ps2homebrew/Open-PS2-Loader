/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review Open-Ps2-Loader README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#ifndef SYSHOOK_H
#define SYSHOOK_H

#include <tamtypes.h>
#include <osd_config.h>

void Install_Kernel_Hooks(void);
void Remove_Kernel_Hooks(void);

extern u32 (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
extern int (*Old_SifSetReg)(u32 register_num, int register_value);
extern int (*Old_ExecPS2)(void *entry, void *gp, int num_args, char *args[]);
extern int (*Old_CreateThread)(ee_thread_t *thread_param);
extern void (*Old_Exit)(s32 exit_code);
extern void (*Old_SetOsdConfigParam)(ConfigParam *osdconfig);
extern void (*Old_GetOsdConfigParam)(ConfigParam *osdconfig);
void sysLoadElf(char *filename, int argc, char **argv);
void sysExit(s32 exit_code);

#endif /* SYSHOOK */
