/*
  Copyright 2009-2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright 2009-2010, misfire <misfire@xploderfreax.de>

  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <tamtypes.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <string.h>
#include <stdio.h>

//START of OPL_DB tweaks
// ELF-loading stuff
#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1


//------------------------------
typedef struct
{
    u8 ident[16];  // struct definition for ELF object header
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;
//------------------------------
typedef struct
{
    u32 type;  // struct definition for ELF program section header
    u32 offset;
    void *vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf_pheader_t;
//END of OPL_DB tweaks

static inline void BootError(char *filename)
{
    char *argv[2];
    argv[0] = "BootError";
    argv[1] = filename;

    ExecOSD(2, argv);
}

static inline void InitializeUserMemory(unsigned int start, unsigned int end)
{
    unsigned int i;

    for (i = start; i < end; i += 64) {
        asm(
            "\tsq $0, 0(%0) \n"
            "\tsq $0, 16(%0) \n"
            "\tsq $0, 32(%0) \n"
            "\tsq $0, 48(%0) \n" ::"r"(i));
    }
}

//START of OPL_DB tweaks
unsigned int _strtoui(const char *p)
{
    if (!p)
        return 0;

    int r = 0;

    while (*p) {
        if ((*p < '0') || (*p > '9'))
            return r;

        r = r * 10 + (*p++ - '0');
    }

    return r;
}



void RunLoaderElf(char *filename,u8 *pElf)
{
    u8 *boot_elf;
    elf_header_t *eh;
    elf_pheader_t *eph;
    void *pdata;
    int  i;
    char *argv[2];
	int elfsize=0;
	int boverlap;
	int vaddr;
	
    /* NB: LOADER.ELF is embedded  */
    boot_elf = (u8 *)pElf;
    eh = (elf_header_t *)boot_elf;
    if (_lw((u32)&eh->ident) != ELF_MAGIC)
        while (1)
            ;
    eph = (elf_pheader_t *)(boot_elf + eh->phoff);

	// calc elf size
    for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;

		if( elfsize <(eph[i].offset+eph[i].filesz)){
			elfsize = eph[i].offset+eph[i].filesz;
		}
    }	
	// check for overlap 
	boverlap=0;
	for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;
        vaddr = (int)eph[i].vaddr;
		if( vaddr <((int)pElf)) {
			if(vaddr+eph[i].memsz>((int)pElf))
			boverlap=1;
			break;
		}
		
		if( vaddr >((int)pElf)) {
			if(vaddr+eph[i].memsz<((int)pElf))
			boverlap=1;
			break;
		}
    }	
	if(boverlap)	
	{
	   u32 u;
	   u = (u32)pElf;
	   u = u >>20;
	   u = u+16;
	   
	   u = u&0x1F;
	   
	   if(u==0)
		   u=1;
	   if(u>24)
		   u=24;
	   u = u << 20;
	   u = u & ( 32*1024*1024-1);

	   memcpy((void*)u,pElf,elfsize);
	   boot_elf = (u8 *)u;
	   
       eh = (elf_header_t *)boot_elf;
       if (_lw((u32)&eh->ident) != ELF_MAGIC)
       while (1)
            ;
       eph = (elf_pheader_t *)(boot_elf + eh->phoff);		
	}		
									
	/* Scan through the ELF's program headers and copy them into RAM, then
									zero out any non-loaded regions.  */								
    for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;

        pdata = (void *)(boot_elf + eph[i].offset);
        memcpy(eph[i].vaddr, pdata, eph[i].filesz);

        if (eph[i].memsz > eph[i].filesz)
            memset(eph[i].vaddr + eph[i].filesz, 0,
                   eph[i].memsz - eph[i].filesz);
    }
	
	
	
    FlushCache(0);
    FlushCache(2);

    argv[0] = filename;
    ExecPS2((void *)eh->entry, 0, 1, argv);
}
//END of OPL_DB tweaks


int main(int argc, char *argv[])
{
	//START of OPL_DB tweaks
	if(argc>0 && (argv[0][0]=='m')
			&& (argv[0][1]=='e')
			&& (argv[0][2]=='m')
			&& (argv[0][3]==':'))
	{
		// boot from memaddr ;
		unsigned int n=_strtoui(&(argv[0][4]));
		RunLoaderElf(argv[1],(void*)n);
		BootError(argv[0]);	
		return 0;
	}
	//END of OPL_DB tweaks
	
    int result;
    t_ExecData exd;

    SifInitRpc(0);

    exd.epc = 0;

    //clear memory.
    InitializeUserMemory(0x00100000, GetMemorySize());
    FlushCache(0);

    SifLoadFileInit();

    result = SifLoadElf(argv[0], &exd);

    SifLoadFileExit();

    if (result == 0 && exd.epc != 0) {
        //Final IOP reset, to fill the IOP with the default modules.
		
        while (!SifIopReset(NULL, 0)) {
        };

        FlushCache(0);
        FlushCache(2);

        while (!SifIopSync()) {
        };

        //Sync with the SIF library on the IOP, or it may crash the IOP kernel during the next reset (Depending on the how the next program initializes the IOP).
        SifInitRpc(0);
        //Load modules.
        SifLoadFileInit();
        SifLoadModule("rom0:SIO2MAN", 0, NULL);
        SifLoadModule("rom0:MCMAN", 0, NULL);
        SifLoadModule("rom0:MCSERV", 0, NULL);
		
		
        SifLoadFileExit();
	
        SifExitRpc();

        ExecPS2((void *)exd.epc, (void *)exd.gp, argc, argv);
    } else {
        SifExitRpc();
    }

    BootError(argv[0]);

    return 0;
}
