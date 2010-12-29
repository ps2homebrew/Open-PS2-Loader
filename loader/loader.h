/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#ifndef _LOADER_H_
#define _LOADER_H_

#include <tamtypes.h>
#include <kernel.h>
#include <stdio.h>
#include <iopheap.h>
#include <sifrpc.h>
#include <string.h>
#include <sbv_patches.h>
#include <smem.h>
#include <smod.h>

#ifdef __EESIO_DEBUG
#include <sio.h>
#define DPRINTF(args...)	sio_printf(args)
#define DINIT()			sio_init(38400, 0, 0, 0, 0)
#else
#define DPRINTF(args...)	do { } while(0)
#define DINIT()			do { } while(0)
#endif


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
extern int iop_reboot_count;

extern int padOpen_hooked;

#define IPCONFIG_MAX_LEN	64
char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
int g_ipconfig_len;
char g_ps2_ip[16];
char g_ps2_netmask[16];
char g_ps2_gateway[16];
u32 g_compat_mask;

#define COMPAT_MODE_1 		0x01
#define COMPAT_MODE_2 		0x02
#define COMPAT_MODE_3 		0x04
#define COMPAT_MODE_4 		0x08
#define COMPAT_MODE_5 		0x10
#define COMPAT_MODE_6 		0x20
#define COMPAT_MODE_7 		0x40
#define COMPAT_MODE_8 		0x80

char GameID[16];
int GameMode;
#define USB_MODE 	0
#define ETH_MODE 	1
#define HDD_MODE 	2

char ExitPath[32];
int USBDelay;

int DisableDebug;
#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

/* modmgr.c */
int  LoadFileInit();
void LoadFileExit();
int  LoadModule(const char *path, int arg_len, const char *args);
int  LoadModuleAsync(const char *path, int arg_len, const char *args);
void GetIrxKernelRAM(void);
int  LoadIRXfromKernel(void *irxkernelmem, int irxsize, int arglen, char *argv);
int  LoadMemModule(void *modptr, unsigned int modsize, int arg_len, const char *args);
int  LoadElf(const char *path, t_ExecData *data);
void ChangeModuleName(const char *name, const char *newname);
void ListModules(void);

/* iopmgr.c */
int  New_Reset_Iop(const char *arg, int flag);
int  Reset_Iop(const char *arg, int flag);
int  Sync_Iop(void);

/* util.c */
inline void _strcpy(char *dst, const char *src);
inline void _strcat(char *dst, const char *src);
int _strncmp(const char *s1, const char *s2, int length);
int _strcmp(const char *s1, const char *s2);
char *_strchr(const char *string, int c);
char *_strrchr(const char * string, int c);
char *_strtok(char *strToken, const char *strDelimit);
char *_strstr(const char *string, const char *substring);
inline int _islower(int c);
inline int _toupper(int c);
int _memcmp(const void *s1, const void *s2, unsigned int length);
unsigned int _strtoui(const char* p);
void set_ipconfig(void);
u32 *find_pattern_with_mask(u32 *buf, u32 bufsize, u32 *pattern, u32 *mask, u32 len);
void CopyToIop(void *eedata, unsigned int size, void *iopptr);
int Patch_Mod(ioprp_t *ioprp_img, const char *name, void *modptr, int modsize);
int Build_EELOADCNF_Img(ioprp_t *ioprp_img, int have_xloadfile);
inline int XLoadfileCheck(void);
inline void delay(int count);
inline void Sbv_Patch(void);

/* syshook.c */
void Install_Kernel_Hooks(void);
void Remove_Kernel_Hooks(void);

u32  (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
int  (*Old_SifSetReg)(u32 register_num, int register_value);
void (*Old_LoadExecPS2)(const char *filename, int argc, char *argv[]);
int  (*Old_ExecPS2)(void *entry, void *gp, int num_args, char *args[]);
int  (*Old_CreateThread)(ee_thread_t *thread_param);

extern void (*InitializeTLB)(void);

/* asm.S */
void Hook_LoadExecPS2(const char *filename, int argc, char *argv[]);
u32 Hook_SifSetDma(SifDmaTransfer_t *sdd, s32 len);
int Hook_SifSetReg(u32 register_num, int register_value);
int Hook_ExecPS2(void *entry, void *gp, int num_args, char *args[]);
int Hook_CreateThread(ee_thread_t *thread_param);

/* spu.c */
void ResetSPU();

/* des.c */
unsigned char *DES(unsigned char *key, unsigned char *message, unsigned char *cipher);

/* md4.c */
unsigned char *MD4(unsigned char *message, int len, unsigned char *cipher);

/* smbauth.c */
void start_smbauth_thread(void);

/* patches.c */
void apply_patches(void);

/* padhook.c */
#define PADOPEN_HOOK  0
#define PADOPEN_CHECK 1

int Install_PadOpen_Hook(u32 mem_start, u32 mem_end, int mode);

/* loadmodulehook.c */
void loadModuleBuffer_patch(void);
void unloadModule_patch(void);

#endif
