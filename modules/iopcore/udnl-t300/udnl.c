#include <dmacman.h>
#include <ioman.h>
#include <intrman.h>
#include <loadcore.h>
#include <stdio.h>
#include <sysmem.h>
#include <sysclib.h>
#include <thbase.h>

#include "ELF.h"
#include "COFF.h"

int CpuExecuteKmode(void *function, ...); //Exactly the same function as INTRMAN's export 14.

//#define DEBUG	1	//Comment out to disable debug messages.
#ifdef DEBUG
#define DEBUG_PRINTF(args...) printf(args)
#else
#define DEBUG_PRINTF(args...)
#endif

#define FULL_UDNL 1 //Comment out to build a UDNL module that updates the IOP with only its payload.
#define MAX_MODULES 256

#define alloca(size) __builtin_alloca(size) //The homebrew PS2SDK lacks alloca.h.

//Function prototypes
static void CopySection(const void *module, void *buffer, unsigned int FileSize);
static void *AllocMemory(int nbytes);

#ifdef FULL_UDNL
/* Evil function. */
/* 0x00000000 */
/* int isIllegalBootDevice(const char *arg1){
	unsigned int temp1, temp2, temp3, DeviceHash;

	while(*path==' ') path++;

	temp1=*path++;
	temp2=*path++;
	temp3=*path++;

	DeviceHash=(((((temp1<<8) | temp2) <<8) | temp3) << 8) ^ 0x72e7c42f;

	// "rom", "host" and "cdrom".
	if((DeviceHash>>8==0x000088A9) || (DeviceHash==0x1A88B75B) || ((DeviceHash==0x1183b640)) && (*path++)=='m')){
		return((*path-0x30)<0x0b?1:0);
	}else{
		return 1;
	}
} */
#endif

/*
	0x00 - RAM size in MB.
	0x04 - The boot mode (0 = hard reset, 1 = soft reset, 2 = update reset, 3 = update complete).
	0x08 - Updater command line (Only for when boot mode = 2).
	0x0C - Pointer to the entry point of the first module.
	0x10 - Result from QueryMemSize() - 0x00200000
	0x1C - pointer to the buffer (Size: Size of images + MAX_MODULES*sizeof(void*) + sizeof(struct ResetData) bytes)
*/

struct ResetData
{
    unsigned int MemSize;         /* 0x00 */
    unsigned int BootMode;        /* 0x04 */
    const char *command;          /* 0x08 */
    void *StartAddress;           /* 0x0C */
    void *IOPRPBuffer;            /* 0x10 */
    unsigned int IOPRPBufferSize; /* 0x14 */
    unsigned int NumModules;      /* 0x18 */
    const void **ModData;         /* 0x1C */
};

#ifdef FULL_UDNL
static int var_00001e20 = 0; /* 0x00001e20 - Not sure what this is, but it's probably a verbosity level flag, although unused. */
#endif

extern unsigned char IOPRP_img[];
extern unsigned int size_IOPRP_img;

struct RomImgData
{ /* 12 bytes */
    const void *ImageStart;
    const void *RomdirStart;
    const void *RomdirEnd;
};

struct ImageData
{ /* 24 bytes */
    const char *filename;
    int fd;
    int size;
    struct RomImgData stat;
};

struct RomDirEntry
{
    char name[10];
    unsigned short int ExtInfoEntrySize;
    unsigned int size;
};

/* 0x00001220 */
static void *GetIOPRPStat(const void *start, const void *end, struct RomImgData *ImageStat)
{
    const unsigned int *ptr, *ptr2;
    unsigned int offset;

    ptr = start;
    offset = 0;
    while ((unsigned int)ptr < (unsigned int)end) {
        ptr2 = &ptr[7];
        // Scan for RESET\0\0\0\0\0. The record should have a filesize of equal to its offset from the start of the image.
        if (ptr[0] == 0x45534552 && ptr2[-6] == 0x54 && (*(unsigned short int *)&ptr2[-5] == 0) && (((ptr2[-4] + 0xF) & 0xFFFFFFF0) == offset)) {
            ImageStat->ImageStart = start;
            ImageStat->RomdirStart = ptr;
            ImageStat->RomdirEnd = (void *)((unsigned int)ptr + *ptr2);
            return ImageStat;
        }

        offset += sizeof(struct RomDirEntry);
        ptr += 4;
    }

    ImageStat->ImageStart = 0;
    return NULL;
}

struct RomdirFileStat
{
    const struct RomDirEntry *romdirent;
    const void *data;
    const void *extinfo;
    unsigned int padding;
};

/* 0x000012c8 */
static struct RomdirFileStat *GetFileStatFromImage(const struct RomImgData *ImageStat, const char *filename, struct RomdirFileStat *stat)
{
    unsigned int i, offset, ExtInfoOffset;
    unsigned char filename_temp[12];
    const struct RomDirEntry *RomdirEntry;
    struct RomdirFileStat *result;

    offset = 0;
    ExtInfoOffset = 0;
    ((unsigned int *)filename_temp)[0] = 0;
    ((unsigned int *)filename_temp)[1] = 0;
    ((unsigned int *)filename_temp)[2] = 0;
    for (i = 0; *filename >= 0x21 && i < sizeof(filename_temp); i++) {
        filename_temp[i] = *filename;
        filename++;
    }

    if (ImageStat->RomdirStart != NULL) {
        RomdirEntry = ImageStat->RomdirStart;

        do {
            if (((unsigned int *)filename_temp)[0] == ((unsigned int *)RomdirEntry->name)[0] && ((unsigned int *)filename_temp)[1] == ((unsigned int *)RomdirEntry->name)[1] && (*(unsigned short int *)&((unsigned int *)filename_temp)[2] == *(unsigned short int *)&((unsigned int *)RomdirEntry->name)[2])) {

                stat->romdirent = RomdirEntry;
                stat->data = ImageStat->ImageStart + offset;
                stat->extinfo = NULL;

                if (RomdirEntry->ExtInfoEntrySize > 0) {
                    stat->extinfo = (void *)((unsigned int)ImageStat->RomdirEnd + ExtInfoOffset);
                    result = stat;
                    goto end;
                }
            }

            offset += (RomdirEntry->size + 0xF) & 0xFFFFFFF0;
            ExtInfoOffset += RomdirEntry->ExtInfoEntrySize;
            RomdirEntry++;
        } while (((unsigned int *)RomdirEntry->name)[0] != 0x00000000);

        result = NULL;
    } else
        result = NULL;

end:
    return result;
}

/* 0x0000055c */
static void ScanImagesForFile(const struct ImageData *ImageDataBuffer, unsigned int NumFiles, struct RomdirFileStat *stat, const char *filename)
{
    const struct ImageData *ImageData;
    int i;

    if (NumFiles > 0) {
        /*
			Let f(x)=((x-1)*2+(x-1))*8;
				f(1)=((1-1)*2+(1-1))*8=0
				f(2)=((2-1)*2+(2-1))*8=24
				f(3)=((3-1)*2+(3-1))*8=48

			ImageDataBuffer+(NumFiles<<1+NumFiles)<<3
		*/

        i = NumFiles - 1;
        ImageData = &ImageDataBuffer[i];

        do {
            if (ImageData->filename != NULL) {
                if (GetFileStatFromImage(&ImageData->stat, filename, stat) != 0) {
                    DEBUG_PRINTF("File: %s, image: %s\n", filename, ImageData->filename);
                    return;
                }
            }

            i--;
            ImageData--;
        } while (i >= 0);
    }

    printf("kupdate: panic ! \'%s\' not found\n", filename);
    __asm("break\n");
}

static void TerminateResidentLibraries(const char *message, unsigned int options, int mode)
{
    lc_internals_t *LoadcoreData;
    iop_library_t *ModuleData, *NextModule;
    void **ExportTable;
    int (*pexit)(int arg1);

    if ((LoadcoreData = GetLoadcoreInternalData()) != NULL) {
        ModuleData = LoadcoreData->let_next;
        while (ModuleData != NULL) {
            NextModule = ModuleData->prev;

            if (mode == 2) {
                if (!(ModuleData->flags & 6)) {
                    ModuleData = NextModule;
                    continue;
                }
            } else if ((ModuleData->flags & 6) == 2) { //Won't ever happen?
                ModuleData = NextModule;
                continue;
            }

            ExportTable = ModuleData->exports;
            if (ExportTable[1] != NULL && ExportTable[2] != NULL) {
                pexit = ExportTable[2];
                pexit(0);
            }

            ModuleData = NextModule;
        }
    }
}

struct ssbus_regs
{
    volatile unsigned int *address, *delay;
};

static struct ssbus_regs ssbus_regs[] = { //0x00001e24
    {
        (volatile unsigned int *)0xbf801000,
        (volatile unsigned int *)0xbf801008},
    {(volatile unsigned int *)0xbf801400,
     (volatile unsigned int *)0xbf80100C},
    {(volatile unsigned int *)0xbf801404,
     (volatile unsigned int *)0xbf801014},
    {(volatile unsigned int *)0xbf801408,
     (volatile unsigned int *)0xbf801018},
    {(volatile unsigned int *)0xbf80140C,
     (volatile unsigned int *)0xbf801414},
    {(volatile unsigned int *)0xbf801410,
     (volatile unsigned int *)0xbf80141C},
    {NULL,
     NULL}};

//0x00000f80
static volatile unsigned int *func_00000f80(volatile unsigned int *address)
{
    struct ssbus_regs *pSSBUS_regs;

    pSSBUS_regs = ssbus_regs;
    while (pSSBUS_regs->address != NULL) {
        if (pSSBUS_regs->address == address)
            break;
        pSSBUS_regs++;
    }

    return pSSBUS_regs->delay;
}

//0x00001b38
void func_00001b38(unsigned int arg1);

//0x00000c98
static void TerminateResidentEntriesDI(unsigned int options)
{
    int prid;
    volatile unsigned int **pReg;

    TerminateResidentLibraries(" kupdate:di: Terminate resident Libraries\n", options, 0);

    asm volatile("mfc0 %0, $15"
                 : "=r"(prid)
                 :);

    if (!(options & 1)) {
        pReg = (prid < 0x10 || ((*(volatile unsigned int *)0xbf801450) & 8)) ? *(volatile unsigned int ***)0xbfc02008 : *(volatile unsigned int ***)0xbfc0200C;

        while (pReg[0] != 0) {
            if (func_00000f80(pReg[0]) != 0)
                pReg[0] = (void *)0xFF;
            pReg[0] = pReg[1];
            pReg += 2;
        }
    }
    if (!(options & 2)) {
        func_00001b38((prid < 0x10 || ((*(volatile unsigned int *)0xbf801450) & 8)) ? *(volatile unsigned int *)0xbfc02010 : *(volatile unsigned int *)0xbfc02014);
    }
}

struct ModuleInfo
{
    unsigned int ModuleType;  //0x00
    void *EntryPoint;         //0x04
    void *gp;                 //0x08
    void *text_start;         //0x0C
    unsigned int text_size;   //0x10
    unsigned int data_size;   //0x14
    unsigned int bss_size;    //0x18
    unsigned int MemSize;     //0x1C
    struct iopmod_id *mod_id; //0x20
    unsigned int unknown_24;  //0x24
};

void func_00001930(void);

enum IOP_MODULE_TYPES {
    IOP_MOD_TYPE_COFF = 1,
    IOP_MOD_TYPE_2,
    IOP_MOD_TYPE_ELF,
    IOP_MOD_TYPE_IRX
};

/* 0x00000b70 */
static int InitModuleInfo(const void *module, struct ModuleInfo *ModuleInfo)
{
    unsigned int Ident_10;
    const struct scnhdr *COFF_ScnHdr;
    const AOUTHDR *COFF_AoutHdr;
    const elf_header_t *ELF_Hdr;
    const elf_pheader_t *ELF_phdr;
    const struct iopmod *iopmod;

    /*	sizeof(struct coff_filehdr)	= 20 bytes
		sizeof(AOUTHDR)			= 56 bytes
		sizeof(struct scnhdr)		= 40 bytes */

    Ident_10 = *(unsigned int *)&((struct coff_filehdr *)module)->f_opthdr;
    COFF_AoutHdr = (AOUTHDR *)((unsigned int)module + sizeof(struct coff_filehdr));
    COFF_ScnHdr = (struct scnhdr *)((unsigned int)module + sizeof(struct coff_filehdr) + sizeof(AOUTHDR));
    if (((struct coff_filehdr *)module)->f_magic == MIPSELMAGIC && COFF_AoutHdr->magic == OMAGIC && ((struct coff_filehdr *)module)->f_nscns < 0x20 && ((Ident_10 & 0x0002FFFF) == 0x20038) && COFF_ScnHdr->s_paddr == COFF_AoutHdr->text_start) {
        if (COFF_AoutHdr->vstamp != 0x7001) {
            /* 0x00000bf8	- COFF */
            ModuleInfo->ModuleType = IOP_MOD_TYPE_COFF;
            ModuleInfo->EntryPoint = (void *)COFF_AoutHdr->entry;
            ModuleInfo->gp = (void *)COFF_AoutHdr->gp_value;
            ModuleInfo->text_start = (void *)COFF_AoutHdr->text_start;
            ModuleInfo->text_size = COFF_AoutHdr->tsize;
            ModuleInfo->data_size = COFF_AoutHdr->dsize;
            ModuleInfo->bss_size = COFF_AoutHdr->bsize;
            ModuleInfo->MemSize = COFF_AoutHdr->bss_start + COFF_AoutHdr->bsize - COFF_AoutHdr->text_start;
            ModuleInfo->mod_id = COFF_AoutHdr->mod_id;

            return ModuleInfo->ModuleType;
        } else {
            return -1;
        }
    } else {
        /* 0x00000C68	- ELF */
        ELF_Hdr = module;
        ELF_phdr = (elf_pheader_t *)((unsigned int)module + ELF_Hdr->phoff);

        if (((unsigned short int *)ELF_Hdr->ident)[2] == 0x101 && ELF_Hdr->machine == 8 && ELF_Hdr->phentsize == sizeof(elf_pheader_t) && ELF_Hdr->phnum == 2 && (ELF_phdr->type == (SHT_LOPROC | SHT_LOPROC_IOPMOD)) && (ELF_Hdr->type == ELF_TYPE_SCE_IRX || ELF_Hdr->type == 2)) {
            ModuleInfo->ModuleType = ELF_Hdr->type == 0xFF80 ? IOP_MOD_TYPE_IRX : IOP_MOD_TYPE_ELF;

            /* 0x00000ce8 */
            iopmod = (struct iopmod *)((unsigned int)module + ELF_phdr->offset);
            ModuleInfo->EntryPoint = (void *)iopmod->EntryPoint;
            ModuleInfo->gp = (void *)iopmod->gp;
            ModuleInfo->text_start = (void *)ELF_phdr[1].vaddr;
            ModuleInfo->text_size = iopmod->text_size;
            ModuleInfo->data_size = iopmod->data_size;
            ModuleInfo->bss_size = iopmod->bss_size;
            ModuleInfo->MemSize = ELF_phdr[1].memsz;
            ModuleInfo->mod_id = iopmod->mod_id;

            return ModuleInfo->ModuleType;
        } else {
            return -1;
        }
    }
}

/* 0x000011c8 */
static void CopySection(const void *module, void *buffer, unsigned int FileSize)
{
    unsigned int *dst;
    const unsigned int *src;
    const void *src_end;

    dst = buffer;
    src = module;
    src_end = (const void *)((unsigned int)module + (FileSize >> 2 << 2));
    while ((unsigned int)src < (unsigned int)src_end) {
        *dst = *src;
        src++;
        dst++;
    }
}

/* 0x00001200 */
static void ZeroSection(unsigned int *buffer, unsigned int NumWords)
{
    while (NumWords > 0) {
        *buffer = 0;
        NumWords--;
        buffer++;
    }
}

/* 0x00000f08 */
static void LoadELFModule(const void *module)
{
    const elf_header_t *ELF_Hdr;
    const elf_pheader_t *ELF_phdr;

    ELF_Hdr = module;
    ELF_phdr = (elf_pheader_t *)((unsigned int)module + ELF_Hdr->phoff);

    CopySection((void *)((unsigned int)module + ELF_phdr[1].offset), (void *)ELF_phdr[1].vaddr, ELF_phdr[1].filesz);

    if (ELF_phdr[1].filesz < ELF_phdr[1].memsz) {
        ZeroSection((unsigned int *)(ELF_phdr[1].vaddr + ELF_phdr[1].filesz), (ELF_phdr[1].memsz - ELF_phdr[1].filesz) >> 2);
    }
}

/* 0x00000e88 */
static void LoadCOFFModule(const void *module)
{
    const AOUTHDR *COFF_AoutHdr;
    const struct scnhdr *ScnHdr;

    /*	sizeof(struct coff_filehdr)	= 20 bytes
		sizeof(AOUTHDR)				= 56 bytes
		sizeof(struct scnhdr)		= 40 bytes */

    COFF_AoutHdr = (AOUTHDR *)((unsigned int)module + sizeof(struct coff_filehdr));
    ScnHdr = (struct scnhdr *)((unsigned int)module + sizeof(struct coff_filehdr) + sizeof(AOUTHDR));

    CopySection((void *)((unsigned int)module + ScnHdr[0].s_size), (void *)COFF_AoutHdr->text_start, COFF_AoutHdr->tsize);
    CopySection((void *)((unsigned int)module + COFF_AoutHdr->tsize), (void *)COFF_AoutHdr->data_start, COFF_AoutHdr->dsize);

    if (COFF_AoutHdr->bss_start != 0 && COFF_AoutHdr->bsize != 0) {
        ZeroSection((unsigned int *)COFF_AoutHdr->bss_start, COFF_AoutHdr->bsize >> 2);
    }
}

/* 0x00000f6c */
static void LoadIRXModule(const void *module, struct ModuleInfo *ModuleInfo)
{
    const elf_header_t *ELF_hdr;
    const elf_pheader_t *ELF_phdr;
    const elf_shdr_t *ELF_shdr, *CurrentELF_shdr;
    unsigned int NumRelocs, i, SectionNum;
    const elf_rel *ELF_relocation;
    unsigned int temp, *WordPatchLocation;

    ELF_hdr = (elf_header_t *)module;
    ELF_phdr = (elf_pheader_t *)((unsigned int)module + ELF_hdr->phoff);

    ModuleInfo->gp = ModuleInfo->gp + (unsigned int)ModuleInfo->text_start;
    ModuleInfo->EntryPoint = ModuleInfo->EntryPoint +(unsigned int)ModuleInfo->text_start;

    if (ModuleInfo->mod_id + 1 != 0) {
        ModuleInfo->mod_id = ModuleInfo->mod_id + (unsigned int)ModuleInfo->text_start;
    }

    ELF_shdr = (elf_shdr_t *)((unsigned int)module + ELF_hdr->shoff);

    CopySection((void *)((unsigned int)module + ELF_phdr[1].offset), ModuleInfo->text_start, ELF_phdr[1].filesz);

    /* 0x00000fec */
    if (ELF_phdr[1].filesz < ELF_phdr[1].memsz) {
        ZeroSection((unsigned int *)(ModuleInfo->text_start + ELF_phdr[1].filesz), (ELF_phdr[1].memsz - ELF_phdr[1].filesz) >> 2);
    }

    /* 0x00001048 */
    for (SectionNum = 0, CurrentELF_shdr = ELF_shdr + 1; SectionNum < ELF_hdr->shnum; SectionNum++, CurrentELF_shdr++) {
        if (CurrentELF_shdr->type == SHT_REL) {
            NumRelocs = CurrentELF_shdr->size / CurrentELF_shdr->entsize;

            /* 0x0000107c - Warning: beware of sign extension! The code here depends on sign extension. */
            for (i = 0, ELF_relocation = (elf_rel *)((unsigned int)module + CurrentELF_shdr->offset); i < NumRelocs; i++, ELF_relocation++) {
                //			DEBUG_PRINTF("Reloc %d: %p\n", (unsigned char)ELF_relocation->info&0xFF, ModuleInfo->text_start+ELF_relocation->offset);	//Code for debugging only: Not originally present.

                switch (ELF_relocation->info & 0xFF) {
                    case R_MIPS_NONE:
                        break;
                    case R_MIPS_16:
                        WordPatchLocation = (unsigned int *)(ModuleInfo->text_start + ELF_relocation->offset);
                        *WordPatchLocation = (*WordPatchLocation & 0xFFFF0000) | (((unsigned int)ModuleInfo->text_start + *(short int *)(ModuleInfo->text_start + ELF_relocation->offset)) & 0xFFFF);
                        break;
                    case R_MIPS_32:
                        WordPatchLocation = (unsigned int *)(ModuleInfo->text_start + ELF_relocation->offset);
                        *WordPatchLocation += (unsigned int)ModuleInfo->text_start;
                        break;
                    case R_MIPS_REL32:
                        break;
                    case R_MIPS_26:
                        WordPatchLocation = (unsigned int *)(ModuleInfo->text_start + ELF_relocation->offset);
                        *WordPatchLocation = (((unsigned int)ModuleInfo->text_start + ((*WordPatchLocation & 0x03FFFFFF) << 2 | ((unsigned int)WordPatchLocation & 0xF0000000))) << 4 >> 6) | (*WordPatchLocation & 0xFC000000);
                        break;
                    case R_MIPS_HI16: //0x00001120	- Ouch. D:
                        temp = (((unsigned int)*(unsigned short int *)(ModuleInfo->text_start + ELF_relocation->offset)) << 16) + *(short int *)(ModuleInfo->text_start + ELF_relocation[1].offset);
                        temp += (unsigned int)ModuleInfo->text_start;
                        WordPatchLocation = (unsigned int *)(ModuleInfo->text_start + ELF_relocation->offset);
                        *WordPatchLocation = (((temp >> 15) + 1) >> 1 & 0xFFFF) | (*WordPatchLocation & 0xFFFF0000);
                        WordPatchLocation = (unsigned int *)(ModuleInfo->text_start + ELF_relocation[1].offset);
                        *WordPatchLocation = (*WordPatchLocation & 0xFFFF0000) | (temp & 0xFFFF);
                        ELF_relocation++;
                        i++;
                        break;
                    case R_MIPS_LO16:
                        break;
                }
            }
        }
    }
}

struct ModInfo
{
    struct ModInfo *next;        //0x00
    const char *name;            //0x04
    unsigned short int version;  //0x08
    unsigned short int newflags; //0x0A
    unsigned short int id;       //0x0C
    unsigned short int flags;    //0x0E
    void *EntryPoint;            //0x10
    void *gp;                    //0x14
    void *text_start;            //0x18
    unsigned int text_size;      //0x1C
    unsigned int data_size;      //0x20
    unsigned int bss_size;       //0x24
    unsigned int unused1;        //0x28
    unsigned int unused2;        //0x2C
};

/* 0x00000df8 - Initializes the module information structure, which exists 0x30 bytes before the module itself. */
static void InitLoadedModInfo(struct ModuleInfo *ModuleInfo, struct ModInfo *ModInfo)
{
    ModInfo->next = NULL;
    ModInfo->name = NULL;
    ModInfo->version = 0;
    ModInfo->newflags = 0;
    ModInfo->id = 0;

    if (ModuleInfo->mod_id + 1 != 0) {
        ModInfo->name = ModuleInfo->mod_id->name;
        ModInfo->version = ModuleInfo->mod_id->version;
    }

    ModInfo->EntryPoint = ModuleInfo->EntryPoint;
    ModInfo->gp = ModuleInfo->gp;
    ModInfo->text_start = ModuleInfo->text_start;
    ModInfo->text_size = ModuleInfo->text_size;
    ModInfo->data_size = ModuleInfo->data_size;
    ModInfo->bss_size = ModuleInfo->bss_size;
}

/* 0x00000d60 */
static int LoadModule(const void *module, struct ModuleInfo *ModuleInfo)
{
    switch (ModuleInfo->ModuleType) {
        case IOP_MOD_TYPE_ELF:
            LoadELFModule(module);
            break;
        case IOP_MOD_TYPE_COFF:
            LoadCOFFModule(module);
            break;
        case IOP_MOD_TYPE_IRX:
            LoadIRXModule(module, ModuleInfo);
            break;
        default:
            return -1;
    }

    /* 0x00000dd4 */
    InitLoadedModInfo(ModuleInfo, ModuleInfo->text_start - 0x30);

    return 0;
}

/* 0x000009d0 */
static void BeginBootupSequence(struct ResetData *ResetData, unsigned int options)
{
    struct ModuleInfo LoadedModules[2];
    int MemSizeInBytes;
    int (*ModuleEntryPoint)(int arg1);
    void *FreeMemStart;

    TerminateResidentEntriesDI(options);

    dmac_set_dpcr(0);
    dmac_set_dpcr2(0);
    dmac_set_dpcr3(0);

    MemSizeInBytes = ResetData->MemSize << 20;

    //Load SYSMEM
    switch (InitModuleInfo(ResetData->ModData[0], &LoadedModules[0])) {
        case IOP_MOD_TYPE_2:
        case IOP_MOD_TYPE_IRX:
            LoadedModules[0].text_start = (void *)((unsigned int)ResetData->StartAddress + 0x30);
        //Fall through.
        case IOP_MOD_TYPE_COFF:
        case IOP_MOD_TYPE_ELF:
            LoadModule(ResetData->ModData[0], &LoadedModules[0]);
            break;
    }

    /* 0x00000a58 */
    func_00001930();

    ModuleEntryPoint = LoadedModules[0].EntryPoint;
    FreeMemStart = (void *)ModuleEntryPoint(MemSizeInBytes);

    //Load LOADCORE
    switch (InitModuleInfo(ResetData->ModData[1], &LoadedModules[1])) {
        case IOP_MOD_TYPE_2:
        case IOP_MOD_TYPE_IRX:
            LoadedModules[1].text_start = (void *)((unsigned int)FreeMemStart + 0x30);
        //Fall through.
        case IOP_MOD_TYPE_COFF:
        case IOP_MOD_TYPE_ELF:
            LoadModule(ResetData->ModData[1], &LoadedModules[1]);
            break;
    }

    /* 0x00000ad0 */
    func_00001930();

    ResetData->StartAddress = LoadedModules[0].text_start;

    ModuleEntryPoint = LoadedModules[1].EntryPoint;
    ModuleEntryPoint((int)ResetData); //LOADCORE will start the bootup sequence.

    //HALT loop
    while (1)
        *(volatile unsigned char *)0x80000000 = 2;
}

/* 0x00000b08 */
static void *ParseStartAddress(const char **line)
{
    const char *ptr;
    unsigned char character;
    void *address;

    ptr = *line;
    address = 0;
    while (*ptr >= 0x30) {
        character = *ptr;
        ptr++;
        if (character < ':') {
            character -= 0x30;
        } else if (character < 'a') {
            character -= 0x4B;
        } else {
            character -= 0x6B;
        }

        address = (void *)(((unsigned int)address << 4) + character);
    }

    *line = ptr;

    return address;
}

struct ExtInfoField
{
    unsigned short int data;
    unsigned short int header; //Upper 8 bits contain the type.
};

/* 0x000013d8 */
static const struct ExtInfoField *GetFileInfo(const struct RomdirFileStat *stat, int mode)
{
    const struct RomDirEntry *RomDirEnt;
    const void *extinfo_end;
    const struct ExtInfoField *ExtInfoField;
    unsigned int ExtInfoHeader;

    RomDirEnt = stat->romdirent;
    extinfo_end = (unsigned char *)stat->extinfo + (RomDirEnt->ExtInfoEntrySize >> 2 << 2);
    ExtInfoField = stat->extinfo;

    while ((unsigned int)ExtInfoField < (unsigned int)extinfo_end) {
        ExtInfoHeader = *(unsigned int *)ExtInfoField;

        if (ExtInfoHeader >> 24 == mode) {
            return ExtInfoField;
        }

        ExtInfoField = ExtInfoField + ((ExtInfoHeader >> 16 & 0xFC) + 4);
    }

    return NULL;
}

/* 0x00000878	- Scans through all loaded IOPRP images for the newest version of the specified module. */
static struct RomdirFileStat *SelectModuleFromImages(const struct ImageData *ImageDataBuffer, unsigned int NumFiles, const char *line, struct RomdirFileStat *stat_out)
{
    char filename[32];
    int count, NumFilesRemaining, ImageFileIndexNumber;
    struct RomdirFileStat *result;
    const struct ImageData *ImageDataPtr;
    struct RomdirFileStat stat;
    const struct ExtInfoField *ExtInfofield;
    unsigned int HighestFileVersionNum;
    unsigned short int FileVersionNum;

    count = 0;
    if (*line >= 0x21) {
        do {
            filename[count] = *line;
            count++;
            line++;
        } while (count < 10 && *line >= 0x21);
    }
    filename[count] = '\0';

    /* 0x000008e0 */
    ImageFileIndexNumber = -1;
    HighestFileVersionNum = 0;
    if ((NumFilesRemaining = NumFiles - 1) >= 0) {
        ImageDataPtr = &ImageDataBuffer[NumFilesRemaining];

        do {
            if (ImageDataPtr->filename != NULL) {
                if (GetFileStatFromImage(&ImageDataPtr->stat, filename, &stat) != NULL) {
                    ExtInfofield = GetFileInfo(&stat, 2);
                    FileVersionNum = 0;
                    GetFileInfo(&stat, 3);

                    /* 0x00000940 */
                    if (ExtInfofield != NULL) {
                        FileVersionNum = ExtInfofield->data;
                    }

                    if (ImageFileIndexNumber < 0 || HighestFileVersionNum < FileVersionNum) {
                        ImageFileIndexNumber = NumFilesRemaining;
                        HighestFileVersionNum = FileVersionNum;

                        DEBUG_PRINTF("SelectModule: %s, %s, 0x%x\n", filename, ImageDataPtr->filename, FileVersionNum);
                    }
                }
            }
            ImageDataPtr--;
        } while (--NumFilesRemaining >= 0);
    }

    if (ImageFileIndexNumber >= 0) {
        result = GetFileStatFromImage(&ImageDataBuffer[ImageFileIndexNumber].stat, filename, stat_out);
    } else
        result = NULL;

    return result;
}

//Code for debugging only - not originally present.
#ifdef DEBUG
static void DisplayModuleName(int id, const char *line)
{
    char filename[12];
    int i;

    for (i = 0; i < 11 && line[i] >= ' '; i++) {
        filename[i] = line[i];
    }
    filename[i] = '\0';

    DEBUG_PRINTF("%d: %s\n", id, filename);
}
#endif

/* 0x000005f4 - this function hates me and I hate it. */
static void ParseIOPBTCONF(const struct ImageData *ImageDataBuffer, unsigned int NumFiles, const struct RomdirFileStat *stat, struct ResetData *ResetData)
{
    unsigned int FilesRemaining, i, NumModules;
    const char *ptr;
    unsigned char filename_temp[16];
    struct RomdirFileStat FileStat;
    struct RomdirFileStat ModuleFileStat;
    const void **ModList;

    if ((unsigned int)stat->data < (unsigned int)stat->data + stat->romdirent->size) {
        FilesRemaining = NumFiles - 1;
        ModList = &ResetData->ModData[ResetData->NumModules];
        NumModules = 0;

        ptr = stat->data;
        do {
            /* 0x00000668 */
            switch (ptr[0]) {
                case '@': /* 0x00000670 */
                    ptr++;
                    ResetData->StartAddress = ParseStartAddress(&ptr);
                    break;
                case '!': /* 0x00000690 */
                    if (strncmp(ptr, "!addr ", 6) == 0) {
                        ptr += 6;
                        ModList[NumModules] = (void *)(((unsigned int)ParseStartAddress(&ptr) << 2) + 1);

                        /* 0x000007a8 */
                        ResetData->NumModules++;
                        NumModules++;
                        ModList[NumModules] = NULL;
                    } else if (strncmp(ptr, "!include ", 9) == 0) {
                        ptr += 9;

                        i = 0;
                        while ((unsigned int)ptr < (unsigned int)stat->data + stat->romdirent->size) {
                            if (*ptr >= 0x20) {
                                filename_temp[i] = *ptr;
                                ptr++;
                                i++;
                            } else
                                break;
                        }
                        filename_temp[i] = '\0';

                        ScanImagesForFile(ImageDataBuffer, FilesRemaining, &FileStat, filename_temp);
                        ParseIOPBTCONF(ImageDataBuffer, FilesRemaining, &FileStat, ResetData);
                        ModList = &ResetData->ModData[ResetData->NumModules];

                        /*
							I have problems seeing why this won't break down if "!include" is used in the middle of a list of modules:
								Before parsing included IOPBTCONF file:
									###_____________
									|  ^
								After parsing included IOPBTCONF file:
									###XXXXXXXX_____
									           |  ^		(The pointer would still be offset by 3, wouldn't it?)
								If 2 additional modules are added to the list of modules:
									###XXXXXXXX___XX_
										   |    ^	(If I'm not wrong, a gap of 3 would result)
							Legend:
								# -> Modules loaded during the first call to ParseIOPBTCONF.
								X -> Modules loaded when the included IOPBTCONF file is parsed.
								| -> The base pointer to the start of the module list.
								^ -> The pointer to the end of the module list.
								_ -> Blank module slots.

							$s2 seems to contain a relative pointer which points to the first blank module slot. It seems to be added to the base pointer ($s0) when the module list is accessed.
							When an included IOPBTCONF file is parsed, $s0 is adjusted but $s2 is not.

							If it's by design, then it will mean that Sony had intended only one additional IOPBTCONF file to be included in each file, and it must be included at only either the absolute start or end of the file.
						*/
                        NumModules = 0; //Therefore, unlike the Sony original, reset the module offset counter.
                    }
                    break;
                case '#': /* 0x0000077c */
                    break;
                default: /* 0x00000784 */
#ifdef DEBUG
                    DisplayModuleName(ResetData->NumModules, ptr); //Code for debugging only - not originally present.
#endif
                    if (SelectModuleFromImages(ImageDataBuffer, NumFiles, ptr, &ModuleFileStat) == NULL) {
                        __asm("break\n");
                    }

                    ModList[NumModules] = ModuleFileStat.data;

                    /* 0x000007a8 */
                    ResetData->NumModules++;
                    NumModules++;
                    ModList[NumModules] = NULL;
            }

            /* 0x000007c0 */
            if ((unsigned int)ptr < (unsigned int)stat->data + stat->romdirent->size) {
                while ((unsigned int)ptr < (unsigned int)stat->data + stat->romdirent->size) {
                    if (*ptr >= 0x20) {
                        ptr++;
                    } else
                        break;
                }
                if ((unsigned int)ptr < (unsigned int)stat->data + stat->romdirent->size) {
                    while ((unsigned int)ptr < (unsigned int)stat->data + stat->romdirent->size) {
                        if (*ptr < 0x20) {
                            ptr++;
                        } else
                            break;
                    }
                }
            }
        } while ((unsigned int)ptr < (unsigned int)stat->data + stat->romdirent->size);
    }
}

int _start(int argc, char *argv[])
{
    int ImageDataTotalSize;
#ifdef FULL_UDNL
    struct ImageData *ImageData;
#endif
    struct ImageData *ImageDataBuffer;
    void *buffer, *IoprpBuffer;
    struct ResetData *ResetData;
    int TotalSize;
#ifdef FULL_UDNL
    int fd, file_sz = 0, i, NumFiles;
#endif
    struct RomdirFileStat FileStat;
    unsigned int BootMode4, BootMode3, options;
    void *BootModePtr;
    struct RomdirFileStat ROMStat;

    BootMode4 = 0xFFFFFFFF;
    BootMode3 = 0;
    if ((BootModePtr = QueryBootMode(4)) != NULL) {
        BootMode4 = *(unsigned char *)BootModePtr;
    }
    if ((BootModePtr = QueryBootMode(3)) != NULL) {
        BootMode3 = *(unsigned int *)((unsigned char *)BootModePtr + 4);
    }

    /*
		Let f(x)=((x+2)*2+(x+2))*8, where x>0
			f(1)=((1+2)*2+(1+2))*8=72
			f(2)=((2+2)*2+(2+2))*8=96
			f(3)=((3+2)*2+(3+2))*8=120

		Therefore, each element is 24 bytes long, with an additional 48 bytes extra allocated for the built-in IOPRP images (The boot ROM and the DATA section of the UDNL module).

		--- Original disassembly ---
		ImageDataTotalSize=((argc+2)<<1);
		ImageDataTotalSize=ImageDataTotalSize+(argc+2);
		ImageDataTotalSize=(ImageDataTotalSize<<3);

		Note: The function reserves an additional 32-bytes of space on the stack when the function enters.
			The pointers are then offset by 0x10 bytes.
		$s7=$sp+0x10	<- Start of image list buffer.
		$s4=$sp+0x40	<- Points to the image entries within the image list buffer.
	 */
    ImageDataTotalSize = (argc + 2) * sizeof(struct ImageData);
    ImageDataBuffer = alloca(ImageDataTotalSize);
    memset(ImageDataBuffer, 0, ImageDataTotalSize);

    TotalSize = MAX_MODULES * sizeof(void *) + sizeof(struct ResetData) + ((size_IOPRP_img + 0xF) & ~0xF); //Unlike the ROM UDNL module, allocate space for the embedded IOPRP image as well like the DVD player UDNL module does.

#ifdef FULL_UDNL
    i = 1;
    NumFiles = 2;
    ImageData = &ImageDataBuffer[2];
    options = 0;
    while (i < argc) {
        if (strcmp(argv[i], "-v") == 0) {
            var_00001e20++;
        } else if (strcmp(argv[i], "-nobusini") == 0 || strcmp(argv[i], "-nb") == 0) {
            options |= 1;
        } else if (strcmp(argv[i], "-nocacheini") == 0 || strcmp(argv[i], "-nc") == 0) {
            options |= 2;
        } else {
            /*		if(isIllegalBootDevice(argv[i])){	//This block of commented-out code (and the commented-out isIllegalBootDevice() function) is what all Sony UDNL modules have, to prevent the loading of IOPRP images from unsecured devices.
				if(BootMode4&2) goto end;
				else{
					SleepThread();
					__asm("break");
				}
			} */

            if ((fd = open(argv[i], O_RDONLY)) < 0) {
                printf("kupdate: pannic ! file \'%s\' can't open\n", argv[i]);
                if (BootMode4 & 2)
                    goto end;
                else {
                    SleepThread();
                    __asm("break");
                }
            }

            file_sz = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            if (file_sz <= 0) {
                close(fd);
                break;
            } else {
                TotalSize += (file_sz + 0xF) & ~0xF;
                ImageData->fd = fd;
                ImageData->size = file_sz;
                ImageData->filename = argv[i];
                NumFiles++;
                ImageData++;
            }
        }

        i++;
    }
#endif

    /* 0x000002f8 */
    if ((buffer = AllocMemory(TotalSize)) == NULL) {
        printf("kupdate: pannic ! can not alloc memory\n");
        if (BootMode4 & 2)
            goto end;
        else {
            SleepThread();
            __asm("break");
        }
    }

    ResetData = buffer;
    memset(ResetData, 0, sizeof(struct ResetData));
    ResetData->ModData = (void *)buffer + sizeof(struct ResetData);
    IoprpBuffer = (void *)((unsigned int)buffer + MAX_MODULES * sizeof(void *) + sizeof(struct ResetData));
    ResetData->IOPRPBuffer = (void *)((unsigned int)IoprpBuffer & 0x1FFFFF00);
    ResetData->MemSize = QueryMemSize() >> 20;
    if (ResetData->MemSize == 8) {
        if (BootMode3 & 0x00000080) {
            printf("  Logical memory size 8MB ---> 2MB\n");
            ResetData->MemSize = 2;
        }
    }
    ResetData->BootMode = 3;
    ResetData->command = NULL;
    if (GetIOPRPStat((void *)0xbfc00000, (void *)0xbfc10000, &ImageDataBuffer[0].stat) != NULL) {
        ImageDataBuffer[0].filename = "ROM";
    }
    if (BootMode3 & 0x00000100) {
        if (GetFileStatFromImage(&ImageDataBuffer[0].stat, "OLDROM", &ROMStat) != 0 && GetIOPRPStat(ROMStat.data, ROMStat.data + 0x40000, &ImageDataBuffer[1].stat)) {
            memcpy(&ImageDataBuffer[0].stat, &ImageDataBuffer[1].stat, sizeof(ImageDataBuffer[0].stat));
            printf("  use alternate ROM image\n");
        }
    }

    /* 0x00000398 */
    /*	Originally, the Sony boot ROM UDNL module did this. However, it doesn't work well because the rest of the data used in the reset will go unprotected. The Sony DVD player UDNL modules allocate memory for the embedded IOPRP image and copies the IOPRP image into the buffer, like if it was read in from a file.
	if(size_IOPRP_img>=0x10 && GetIOPRPStat(IOPRP_img, &IOPRP_img[size_IOPRP_img], &ImageDataBuffer[1].stat)!=NULL){
		ImageDataBuffer[1].filename="DATA";
		ResetData->IOPRPBuffer=(void*)((unsigned int)IOPRP_img&~0xF);
	} */
    if (size_IOPRP_img >= 0x10) { //Hence, do this instead:
        memcpy(IoprpBuffer, IOPRP_img, size_IOPRP_img);
        if (GetIOPRPStat(IoprpBuffer, (void *)((unsigned int)IoprpBuffer + size_IOPRP_img), &ImageDataBuffer[1].stat) != NULL) {
            ImageDataBuffer[1].filename = "DATA";
            IoprpBuffer += (size_IOPRP_img + 0xF) & ~0xF;
        }
    }

#ifdef FULL_UDNL
    /* 0x000003dc */
    i = 2;
    if (i < NumFiles) {
        ImageData = &ImageDataBuffer[2];

        do {
            if (read(ImageData->fd, IoprpBuffer, ImageData->size) == ImageData->size) {
                if (GetIOPRPStat(IoprpBuffer, IoprpBuffer + 0x4000, &ImageData->stat) != NULL) {
                    /* 0x00000420 */
                    IoprpBuffer += (ImageData->size + 0xF) & ~0xF;
                }
            }

            close(ImageData->fd);
            i++;
            ImageData++;
        } while (i < NumFiles);
    }
#endif

    /* 0x00000448 */
    ResetData->IOPRPBufferSize = IoprpBuffer - ResetData->IOPRPBuffer + 0x100;
#ifdef FULL_UDNL
    ScanImagesForFile(ImageDataBuffer, NumFiles, &FileStat, "IOPBTCONF");
    ParseIOPBTCONF(ImageDataBuffer, NumFiles, &FileStat, ResetData);
#else
    ScanImagesForFile(ImageDataBuffer, 2, &FileStat, "IOPBTCONF");
    ParseIOPBTCONF(ImageDataBuffer, 2, &FileStat, ResetData);
#endif

    DEBUG_PRINTF("Beginning IOP bootup sequence.\n");

    TerminateResidentLibraries(" kupdate:ei: Terminate resident Libraries\n", options, 2);
    CpuExecuteKmode(&BeginBootupSequence, ResetData, options);

end:
    return MODULE_NO_RESIDENT_END;
}

/* 0x000004cc */
static void *AllocMemory(int nbytes)
{
    void *BlockTopAddress;
    int result;

    BlockTopAddress = QueryBlockTopAddress((void *)0x1860);
    BlockTopAddress = (void *)((unsigned int)BlockTopAddress & 0x7FFFFFFF);
    do {
        while ((result = QueryBlockSize(BlockTopAddress)) >= 0 || (result & 0x7FFFFFFF) < nbytes) {
            BlockTopAddress = QueryBlockTopAddress((void *)(((unsigned int)BlockTopAddress & 0x7FFFFFFF) + (result & 0x7FFFFFFF)));
            BlockTopAddress = (void *)((unsigned int)BlockTopAddress & 0x7FFFFFFF);
        }
    } while ((result & 0x7FFFFFFF) < nbytes);

    return AllocSysMemory(2, nbytes, BlockTopAddress);
}
