/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

#ifndef _MCEMU_H_
#define _MCEMU_H_

#include <ioman.h>
#include <intrman.h>
#include <loadcore.h>
#include <sifcmd.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>
#include <dmacman.h>

#include "mcemu_utils.h"
#include "device.h"

#define MODNAME "mcemu"

// debug output is handled elsewhere
#ifdef DEBUG
#define DPRINTF(format, args...) \
    printf(MODNAME ": " format, ##args)
#else
#define DPRINTF(format, args...) \
    do {                         \
    } while (0)
#endif

/* mcemu.c */

/* type for a pointer to the SECRMAN's entry SecrAuthCard */
typedef int (*PtrSecrAuthCard)(int port, int slot, int cnum);

/* type for a pointer to LOADCORE's entry  RegisterLibraryEntires */
typedef int (*PtrRegisterLibraryEntires)(iop_library_t *lib);

typedef int (*PtrMcIo)(int fd, void *buf, int nbyte);

/* SIO2 Packet for SIO2MAN calls */
typedef struct _Sio2Packet
{
    u32 iostatus;
    u32 data1[4];
    u32 data2[4];
    u32 data3;
    u32 ctrl[16];
    u32 data4;
    u32 pwr_len;
    u32 prd_len;
    void *pwr_buf;
    void *prd_buf;
    void *wrmaddr;
    u32 wrwords;
    u32 wrcount;
    void *rdmaddr;
    u32 rdwords;
    u32 rdcount;
} Sio2Packet;

#ifdef BDM_DRIVER
struct fat_file
{
    u32 cluster;
    u32 size;
};
#endif

#ifdef HDD_DRIVER
/* Apa partitions informations */
typedef struct
{
    u32 start;
    u32 length;
} apa_parts;

/* Block number/count pair (used in inodes) */
typedef struct
{
    u32 number;
    u16 subpart;
    u16 count;
} pfs_blocks;
#endif

/* Memory Card Spec (do not change this structure) */
typedef struct _McSpec
{
    u16 PageSize;  /* Page size in bytes (user data only) */
    u16 BlockSize; /* Block size in pages */
    u32 CardSize;  /* Total number of pages */
} McSpec;

/* Virtual Memory Card Image File Spec */
typedef struct _McImageSpec
{
    int active; /* Activation flag */

#ifdef BDM_DRIVER
    u32 stsec; /* Vmc file start sector */
#endif

#ifdef HDD_DRIVER
    apa_parts parts[5];    /* Vmc file Apa partitions */
    pfs_blocks blocks[10]; /* Vmc file Pfs inode blocks */
#endif

#ifdef SMB_DRIVER
    char fname[64]; /* Vmc file name (memorycard?.bin) */
    u16 fid;        /* SMB Vmc file id */
#endif

    int flags;    /* Memory Card Flags */
    McSpec cspec; /* Memory Card Spec */
} McImageSpec;

/* Descriptor for a virtual memory card */
typedef struct _MemoryCard
{
    int mcnum;    /* Memory Card Number (-1 if inactive) */
    int rpage;    /* Read page index */
    int rdoff;    /* Read offset in rpage */
    int wpage;    /* Write page index */
    int wroff;    /* Write offset in wpage */
    u8 *dbufp;    /* Pointer to a user-data buffer */
    u8 *cbufp;    /* Pointer to a ECC buffer */
    int rcoff;    /* Read ECC offset */
    int wcoff;    /* Write ECC offset */
    int tcode;    /* Termination code */
    McSpec cspec; /* Memory Card Spec */
    int flags;    /* Memory Card Flags */
} MemoryCard;

/* type for a pointer to SIO2MAN's entries */
typedef void (*Sio2McProc)(Sio2Packet *arg);
typedef void (*Sio2McProc2)();

int DummySecrAuthCard(int port, int slot, int cnum);
void Sio2McEmu(Sio2Packet *sd);

int hookSecrAuthCard(int port, int slot, int cnum);
void hookSio2man25(Sio2Packet *sd);
void hookSio2man51(Sio2Packet *sd);
u32 *hookSio2man67();
void hookSio2man(Sio2Packet *sd, Sio2McProc sio2proc);
int hookRegisterLibraryEntires(iop_library_t *lib);

void InstallSecrmanHook(void *exp);
void InstallSio2manHook(void *exp, int ver);
void InstallMcmanHook(void *exp);

void SioResponse(MemoryCard *mcd, void *buf, int length);
int MceEraseBlock(MemoryCard *mcd, int page);
int MceStartRead(MemoryCard *mcd, int page);
int MceStartWrite(MemoryCard *mcd, int page);
int MceRead(MemoryCard *mcd, void *buf, u32 size);
int MceWrite(MemoryCard *mcd, void *buf, u32 size);

/* mcemu_io.c */

int mc_configure(MemoryCard *mcs);

/* mcemu_rpc.c */

/* EE RPC id for libmc calls */
#define LIBMC_RPCNO 0x80000400

/* size of RPC buffer */
#define LIBMC_RPC_BUFFER_SIZE 0x80

int hookMcman62();
int hookMcman63(int fd, u32 eeaddr, int nbyte);
int hookMcman68(int fd, u32 eeaddr, int nbyte);



/* mcemu_sys.c */

void *GetExportTable(char *libname, int version);
u32 GetExportTableSize(void *table);
void *GetExportEntry(void *table, u32 entry);
void *HookExportEntry(void *table, u32 entry, void *func);

void *_SysAlloc(u64 size);
int _SysFree(void *area);

int GetInt(void *ptr);
u32 CalculateEDC(u8 *buf, u32 size);
void CalculateECC(u8 *buf, void *chk);

/* mcemu_var.c */

#define MCEMU_PORTS 2

extern const u8 xortable[256];

extern PtrSecrAuthCard pSecrAuthCard[MCEMU_PORTS];
extern McImageSpec vmcSpec[MCEMU_PORTS];
extern MemoryCard memcards[MCEMU_PORTS];
extern void *pFastBuf;

extern PtrRegisterLibraryEntires pRegisterLibraryEntires;
extern Sio2McProc pSio2man25, pSio2man51;
extern Sio2McProc2 psio2_mc_transfer_init, psio2_transfer_reset;
extern void (*pSio2man67)();

extern u8 mceccbuf[MCEMU_PORTS][0x20];
extern u8 mcdatabuf[0x200];

extern SifRpcClientData_t *pClientData;
extern void *pFastRpcBuf;

extern PtrMcIo pMcRead, pMcWrite;

#endif //_MCEMU_H_
