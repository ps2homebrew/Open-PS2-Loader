#ifndef _LOADER_H_
#define _LOADER_H_

#include <tamtypes.h>
#include <kernel.h>
#include <stdio.h>
#include <iopheap.h>
#include <sifrpc.h>
#include <string.h>
#include <sbv_patches.h>
#include <debug.h>
#include <smod.h>
#include <smem.h>

#include "libcdvd.h"

#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

#define MAX_ARGS     0x40
#define MAX_MOD_ARGS 0x50
#define MAX_PATH     0x100

#define VMC_HDD     0x1
#define VMC_MASS    0x2
#define VMC_HOST    0x3


typedef
struct romdir
{
 char           fileName[10];
 unsigned short extinfo_size;
 int            fileSize;
} romdir_t;


typedef
struct ioprp
{
 void        *data_in;
 void        *data_out;
 unsigned int size_in;
 unsigned int size_out;
} ioprp_t;


typedef struct 
{
	u32 epc;
	u32 gp;
	u32 sp;
	u32 dummy;  
} t_ExecData;

typedef struct {
	void *irxaddr;
	int irxsize;
} irxptr_t;

u8 *g_buf;

extern int set_reg_hook;
extern int set_reg_disabled;

#define IPCONFIG_MAX_LEN	64
char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
int g_ipconfig_len;

int GameMode;
#define USB_MODE 	0
#define ETH_MODE 	1

/* modmgr.c */
int  LoadFileInit();
void LoadFileExit();
int  LoadModuleAsync(const char *path, int arg_len, const char *args);
void GetIrxKernelRAM(void);
int LoadIRXfromKernel(void *irxkernelmem, int irxsize, int arglen, char *argv);
int  LoadMemModule(void *modptr, unsigned int modsize, int arg_len, const char *args);
int  LoadElf(const char *path, t_ExecData *data) __attribute__((section(".unsafe")));

/* iopmgr.c */
int  New_Reset_Iop(const char *arg, int flag);
int  Reset_Iop(const char *arg, int flag);
int  Sync_Iop(void);

/* misc.c */
void CopyToIop(void *eedata, unsigned int size, void *iopptr);
int Patch_Mod(ioprp_t *ioprp_img, const char *name, void *modptr, int modsize);
int Patch_EELOADCNF_Img(ioprp_t *ioprp_img);
void delay(int count);
void Sbv_Patch(void);

/* syshook.c */
void Install_Kernel_Hooks(void);

u32  (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
int  (*Old_SifSetReg)(u32 register_num, int register_value);
void (*Old_LoadExecPS2)(const char *filename, int argc, char *argv[]);

#endif
