/*
  Copyright 2009, Ifcaro
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"

extern void *imgdrv_irx;
extern int size_imgdrv_irx;

extern void *eesync_irx;
extern int size_eesync_irx;

extern void *cdvdman_irx;
extern int size_cdvdman_irx;

void *usbd_irx;
int size_usbd_irx;

extern void *usbd_ps2_irx;
extern int size_usbd_ps2_irx;

extern void *usbd_ps3_irx;
extern int size_usbd_ps3_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

extern void *ingame_usbhdfsd_irx;
extern int size_ingame_usbhdfsd_irx;

extern void *isofs_irx;
extern int size_isofs_irx;

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *ps2ip_irx;
extern int size_ps2ip_irx;

extern void *ps2smap_irx;
extern int size_ps2smap_irx;

extern void *netlog_irx;
extern int size_netlog_irx;

extern void *smbman_irx;
extern int size_smbman_irx;

extern void *dummy_irx;
extern int size_dummy_irx;

extern void *loader_elf;
extern int size_loader_elf;

extern void *alt_loader_elf;
extern int size_alt_loader_elf;

extern void *_gp;

#define IPCONFIG_MAX_LEN	64
char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
int g_ipconfig_len;

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

int PS3Detect(){ //return 0=PS2 1=PS3-HARD 2=PS3-SOFT
	int xparam2, size=0, i=0; 
	void *buffer;
	xparam2 = fioOpen("rom0:XPARAM2", O_RDONLY);
	if (xparam2>0){
		size = lseek(xparam2, 0, SEEK_END);
		lseek(xparam2, 0, SEEK_SET);
		buffer=malloc(size);
		fioRead(xparam2, buffer, size);

		for (i=0;i<size;i++){
			if(!strcmp((const char*)((u32)buffer+i),"SCPS_110.01")){
				return 2;
			}
		}
	}
	
	fioClose(xparam2);
	
	if(xparam2 > 0) return 1;
	return 0;
}

void set_ipconfig(void)
{
	int i;
	char str[16];
	
	memset(g_ipconfig, 0, IPCONFIG_MAX_LEN);
	g_ipconfig_len = 0;
	
	// add ip to g_ipconfig buf
	sprintf(str, "%d.%d.%d.%d", ps2_ip[0], ps2_ip[1], ps2_ip[2], ps2_ip[3]);
	strncpy(&g_ipconfig[g_ipconfig_len], str, 15);
	g_ipconfig_len += strlen(str) + 1;

	// add netmask to g_ipconfig buf
	sprintf(str, "%d.%d.%d.%d", ps2_netmask[0], ps2_netmask[1], ps2_netmask[2], ps2_netmask[3]);
	strncpy(&g_ipconfig[g_ipconfig_len], str, 15);
	g_ipconfig_len += strlen(str) + 1;

	// add gateway to g_ipconfig buf
	sprintf(str, "%d.%d.%d.%d", ps2_gateway[0], ps2_gateway[1], ps2_gateway[2], ps2_gateway[3]);
	strncpy(&g_ipconfig[g_ipconfig_len], str, 15);
	g_ipconfig_len += strlen(str) + 1;
	
	for (i=0;i<size_smbman_irx;i++){
		if(!strcmp((const char*)((u32)&smbman_irx+i),"xxx.xxx.xxx.xxx")){
			break;
		}
	}
	sprintf(str, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
	memcpy((void*)((u32)&smbman_irx+i),str,strlen(str)+1);	
	memcpy((void*)((u32)&smbman_irx+i+16),&gPCPort, 4);
	memcpy((void*)((u32)&smbman_irx+i+20), gPCShareName, 32);
}

void th_LoadNetworkModules(void *args){
	
	int ret, id;
	
	gNetworkStartup = 4;
	
	set_ipconfig();
	
	id=SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		goto fini;
	}
	
	gNetworkStartup = 3;
	
	id=SifExecModuleBuffer(&ps2ip_irx, size_ps2ip_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		goto fini;
	}
	
	gNetworkStartup = 2;
	
	id=SifExecModuleBuffer(&ps2smap_irx, size_ps2smap_irx, g_ipconfig_len, g_ipconfig, &ret);	
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		goto fini;
	}
	
	gNetworkStartup = 1;
	
	id=SifExecModuleBuffer(&smbman_irx, size_smbman_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		goto fini;
	}

	gNetworkStartup = 0; // ok, all loaded
fini:		
	SleepThread();
}

void Start_LoadNetworkModules_Thread(void){
	
	ee_thread_t thread;
	s32 thread_id;

	memset(&thread, 0, sizeof(thread));

	thread.func				= (void *)th_LoadNetworkModules;
	thread.stack_size		= 0x100000;
	thread.gp_reg			= &_gp;
	thread.initial_priority	= 0x1;

	thread_id = CreateThread(&thread);

	StartThread(thread_id, NULL);
}

void LoadUSBD(){
	int fd, ps3model;
	
	//first it searchs for custom usbd in MC
	fd = fioOpen("mc0:/BEDATA-SYSTEM/USBD.IRX", O_RDONLY);
	if (fd < 0){
		fd = fioOpen("mc0:/BADATA-SYSTEM/USBD.IRX", O_RDONLY);
		if (fd < 0){
			fd = fioOpen("mc0:/BIDATA-SYSTEM/USBD.IRX", O_RDONLY);
		}
	}
	if(fd > 0){
		size_usbd_irx = fioLseek(fd, 1, SEEK_END);
		usbd_irx=malloc(size_usbd_irx);
		fioLseek(fd, 0, SEEK_SET);
		fioRead(fd, usbd_irx, size_usbd_irx);
		fioClose(fd);
	}else{ // If don't exist it uses embedded
		ps3model=PS3Detect();
		if(ps3model==0){
			usbd_irx=(void *)&usbd_ps2_irx;
			size_usbd_irx=size_usbd_ps2_irx;
		}else{
			usbd_irx=(void *)&usbd_ps3_irx;
			size_usbd_irx=size_usbd_ps3_irx;
		}
	}
	SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, NULL);
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

void LaunchGame(TGame *game, int mode, int compatmask)
{
	u8 *boot_elf = NULL;
	elf_header_t *eh;
	elf_pheader_t *eph;
	void *pdata;
	int i;
	char *argv[2];
	char gamename[33];
	char isoname[32];
	char filename[32];
	char config_str[255];
	char *mode_str = NULL;
	u8 mode_val;
	
	sprintf(filename,"%s",game->Image+3);
	
	memset(gamename, 0, 33);
	strncpy(gamename, game->Name, 32);

	sprintf(isoname,"ul.%08X.%s",crc32(gamename),filename);
	
	for (i=0;i<size_isofs_irx;i++){
		if(!strcmp((const char*)((u32)&isofs_irx+i),"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")){
			break;
		}
	}
	
	memcpy((void*)((u32)&isofs_irx+i),isoname,strlen(isoname)+1);
	memcpy((void*)((u32)&isofs_irx+i+33),&game->parts,1);
	memcpy((void*)((u32)&isofs_irx+i+34),&game->media,1);	
	if (mode == USB_MODE) {
		mode_val = USB_MODE;
		memcpy((void*)((u32)&isofs_irx+i+35),&mode_val,1);
		memcpy((void*)((u32)&isofs_irx+i+36),"USBD.IRX\nDECI2.IRX\nSNMON.IRX\0", 29);
		if (compatmask & COMPAT_MODE_4)
			memcpy((void*)((u32)&isofs_irx+i+36+28),"\nEYETOY.IRX\0", 12);
	}
	else if (mode == ETH_MODE) {
		mode_val = ETH_MODE;
		memcpy((void*)((u32)&isofs_irx+i+35),&mode_val,1);
		memcpy((void*)((u32)&isofs_irx+i+36),"DEV9.IRX\nSMAP.IRX\nDECI2.IRX\nSNMON.IRX\0", 38);
		int off_str = 37;
		if (compatmask & COMPAT_MODE_4) {
			memcpy((void*)((u32)&isofs_irx+i+36+off_str),"\nEYETOY.IRX\0", 12);
			off_str += 11;
		}
		if (compatmask & COMPAT_MODE_5)
			memcpy((void*)((u32)&isofs_irx+i+36+off_str),"\nUSBD.IRX\0", 10);
	}
		
	FlushCache(0);
		
	SendIrxKernelRAM();

/* NB: LOADER.ELF is embedded  */
	if (compatmask & COMPAT_MODE_1)
		boot_elf = (u8 *)&alt_loader_elf;
	else
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

	if (mode == USB_MODE)
		mode_str = "USB_MODE";
	else if (mode == ETH_MODE)
		mode_str = "ETH_MODE";
		
	sprintf(config_str, "%s %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d", mode_str, ps2_ip[0], ps2_ip[1], ps2_ip[2], ps2_ip[3], \
		ps2_netmask[0], ps2_netmask[1], ps2_netmask[2], ps2_netmask[3], \
		ps2_gateway[0], ps2_gateway[1], ps2_gateway[2], ps2_gateway[3]);
	
	char cmask[10];
	snprintf(cmask, 10, "%d", compatmask);
	argv[0] = config_str;	
	argv[1] = filename;
	argv[2] = cmask;
	
	ExecPS2((void *)eh->entry, 0, 3, argv);
} 

#define IRX_NUM 12

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
	irxptr_tab[n++].irxsize = size_ingame_usbhdfsd_irx;
	irxptr_tab[n++].irxsize = size_isofs_irx;
	irxptr_tab[n++].irxsize = size_ps2dev9_irx;	
	irxptr_tab[n++].irxsize = size_ps2ip_irx;	
	irxptr_tab[n++].irxsize = size_ps2smap_irx;	
	irxptr_tab[n++].irxsize = size_netlog_irx;
	irxptr_tab[n++].irxsize = size_smbman_irx;	
	irxptr_tab[n++].irxsize = size_dummy_irx;	
	 	
	n = 0;
	irxsrc[n++] = (void *)&imgdrv_irx;
	irxsrc[n++] = (void *)&eesync_irx;	
	irxsrc[n++] = (void *)&cdvdman_irx;
	irxsrc[n++] = (void *)usbd_irx;
	irxsrc[n++] = (void *)&ingame_usbhdfsd_irx;
	irxsrc[n++] = (void *)&isofs_irx;
	irxsrc[n++] = (void *)&ps2dev9_irx;
	irxsrc[n++] = (void *)&ps2ip_irx;
	irxsrc[n++] = (void *)&ps2smap_irx;
	irxsrc[n++] = (void *)&netlog_irx;
	irxsrc[n++] = (void *)&smbman_irx;
	irxsrc[n++] = (void *)&dummy_irx;	
	
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
