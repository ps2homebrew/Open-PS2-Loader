#include "include/usbld.h"

extern void *imgdrv_irx;
extern int size_imgdrv_irx;

extern void *eesync_irx;
extern int size_eesync_irx;

extern void *cdvdman_irx;
extern int size_cdvdman_irx;

extern void *usbd_irx;
extern int size_usbd_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

extern void *isofs_irx;
extern int size_isofs_irx;

extern void *loader_elf;
extern int size_loader_elf;

#define ELF_MAGIC		0x464c457f
#define ELF_PT_LOAD		1

typedef struct
{
	u8	ident[16];			// struct definition for ELF object header
	u16	type;
	u16	machine;
	u32	version;
	u32	entry;
	u32	phoff;
	u32	shoff;
	u32	flags;
	u16	ehsize;
	u16	phentsize;
	u16	phnum;
	u16	shentsize;
	u16	shnum;
	u16	shstrndx;
} elf_header_t;

typedef struct
{
	u32	type;				// struct definition for ELF program section header
	u32	offset;
	void	*vaddr;
	u32	paddr;
	u32	filesz;
	u32	memsz;
	u32	flags;
	u32	align;
} elf_pheader_t;

typedef struct {
	void *irxaddr;
	int irxsize;
} irxptr_t;

void Reset()
{
	while(!SifIopReset("rom0:UDNL rom0:EELOADCNF",0));
  	while(!SifIopSync());
  	fioExit();
  	SifExitIopHeap();
  	SifLoadFileExit();
  	SifExitRpc();
  	SifExitCmd();
  	
  	SifInitRpc(0);
  	FlushCache(0);
  	FlushCache(2);
	 	
  	// init loadfile & iopheap services    		  	
	SifLoadFileInit();
	SifInitIopHeap();
	
	// apply sbv patches
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

void delay(int count) {
	int i;
	int ret;
	for (i  = 0; i < count; i++) {
	        ret = 0x01000000;
		while(ret--) asm("nop\nnop\nnop\nnop");
	}
}

unsigned int crctab[0x400];

unsigned int crc32(char *string)
{
    int crc, table, count, byte;

    for (table=0; table<256; table++)
    {
        crc = table << 24;

        for (count=8; count>0; count--)
        {
            if (crc < 0) crc = crc << 1;
            else crc = (crc << 1) ^ 0x04C11DB7;
        }
        crctab[255-table] = crc;
    }

    do {
        byte = string[count++];
        crc = crctab[byte ^ ((crc >> 24) & 0xFF)] ^ ((crc << 8) & 0xFFFFFF00);
    } while (string[count-1] != 0);

    return crc;
}

void LaunchGame(TGame *game)
{
	u8 *boot_elf;
	elf_header_t *eh;
	elf_pheader_t *eph;
	void *pdata;
	int i;
	char *argv[2];
	char isoname[32];
	char filename[32];
	
	sprintf(filename,"%s",game->Image+3);
	
	sprintf(isoname,"ul.%08X.%s",crc32(game->Name),filename);
	
	for (i=0;i<size_isofs_irx;i++){
		if(!strcmp((const char*)((u32)&isofs_irx+i),"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")){
			break;
		}
	}
	
	memcpy((void*)((u32)&isofs_irx+i),isoname,strlen(isoname)+1);
	memcpy((void*)((u32)&isofs_irx+i+33),&game->parts,1);
	
	SendIrxKernelRAM();

/* NB: LOADER.ELF is embedded  */
	boot_elf = (u8 *)&loader_elf;
	eh = (elf_header_t *)boot_elf;
	if (_lw((u32)&eh->ident) != ELF_MAGIC)
		while (1);

	eph = (elf_pheader_t *)(boot_elf + eh->phoff);

/* Scan through the ELF's program headers and copy them into RAM, then
									zero out any non-loaded regions.  */
	for (i = 0; i < eh->phnum; i++)
	{
		if (eph[i].type != ELF_PT_LOAD)
		continue;

		pdata = (void *)(boot_elf + eph[i].offset);
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz)
			memset(eph[i].vaddr + eph[i].filesz, 0,
					eph[i].memsz - eph[i].filesz);
	}

/* Let's go.  */
	fioExit();
	SifInitRpc(0);
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);
	
	argv[0] = filename;
	argv[1] = filename;
	
	ExecPS2((void *)eh->entry, 0, 2, argv);
} 

#define IRX_NUM 6

//-------------------------------------------------------------- 
void SendIrxKernelRAM(void) // Send IOP modules that core must use to Kernel RAM
{
	u32 *total_irxsize = (u32 *)0x80030000;
	void *irxtab = (void *)0x80030010;
	void *irxptr = (void *)0x80030100;
	irxptr_t irxptr_tab[IRX_NUM];
	void *irxsrc[IRX_NUM];
	int i, n;
	u32 irxsize;

	n = 0;
	irxptr_tab[n++].irxsize = size_imgdrv_irx; 
	irxptr_tab[n++].irxsize = size_eesync_irx; 
	irxptr_tab[n++].irxsize = size_cdvdman_irx;
	irxptr_tab[n++].irxsize = size_usbd_irx;
	irxptr_tab[n++].irxsize = size_usbhdfsd_irx;
	irxptr_tab[n++].irxsize = size_isofs_irx;
	/*irxptr_tab[n++].irxsize = size_ps2dev9_irx;	
	irxptr_tab[n++].irxsize = size_ps2ip_irx;	
	irxptr_tab[n++].irxsize = size_ps2smap_irx;	
	irxptr_tab[n++].irxsize = size_netlog_irx;	*/
	 	
	n = 0;
	irxsrc[n++] = (void *)&imgdrv_irx;
	irxsrc[n++] = (void *)&eesync_irx;	
	irxsrc[n++] = (void *)&cdvdman_irx;
	irxsrc[n++] = (void *)&usbd_irx;
	irxsrc[n++] = (void *)&usbhdfsd_irx;
	irxsrc[n++] = (void *)&isofs_irx;
	/*irxsrc[n++] = (void *)&ps2dev9_irx;
	irxsrc[n++] = (void *)&ps2ip_irx;
	irxsrc[n++] = (void *)&ps2smap_irx;
	irxsrc[n++] = (void *)&netlog_irx;*/
	
	irxsize = 0;		

	DIntr();
	ee_kmode_enter();
				
	for (i = 0; i <	IRX_NUM; i++) {
		irxptr_tab[i].irxaddr = irxptr; 
		
		memcpy((void *)irxptr_tab[i].irxaddr, (void *)irxsrc[i], irxptr_tab[i].irxsize);
		
		irxptr += irxptr_tab[i].irxsize;
		irxsize += irxptr_tab[i].irxsize;
	}
	
	memcpy((void *)irxtab, (void *)&irxptr_tab[0], sizeof(irxptr_tab));	
	
	*total_irxsize = irxsize;
	
	ee_kmode_exit();
	EIntr();
}	
