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

extern void *usb_cdvdman_irx;
extern int size_usb_cdvdman_irx;

extern void *smb_cdvdman_irx;
extern int size_smb_cdvdman_irx;

extern void *hdd_cdvdman_irx;
extern int size_hdd_cdvdman_irx;

extern void *cdvdfsv_irx;
extern int size_cdvdfsv_irx;

extern void *cddev_irx;
extern int size_cddev_irx;

void *usbd_irx;
int size_usbd_irx;

extern void *usbd_ps2_irx;
extern int size_usbd_ps2_irx;

extern void *usbd_ps3_irx;
extern int size_usbd_ps3_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *smsutils_irx;
extern int size_smsutils_irx;

extern void *smstcpip_irx;
extern int size_smstcpip_irx;

extern void *smsmap_irx;
extern int size_smsmap_irx;

extern void *netlog_irx;
extern int size_netlog_irx;

extern void *smbman_irx;
extern int size_smbman_irx;

extern void *discid_irx;
extern int size_discid_irx;

extern void *iomanx_irx;
extern int size_iomanx_irx;

extern void *filexio_irx;
extern int size_filexio_irx;

extern void *poweroff_irx;
extern int size_poweroff_irx;

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
	int ret;

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
    
    // load modules
    SifExecModuleBuffer(&discid_irx, size_discid_irx, 0, NULL, &ret);
    SifExecModuleBuffer(&iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
    SifExecModuleBuffer(&filexio_irx, size_filexio_irx, 0, NULL, &ret);
    SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, &ret);

    poweroffInit();

    gDev9_loaded = 0;
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
		
	set_ipconfig();
	
	gNetworkStartup = 4;

	id=SifExecModuleBuffer(&smsutils_irx, size_smsutils_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		goto fini;
	}
	
	gNetworkStartup = 3;
	
	id=SifExecModuleBuffer(&smstcpip_irx, size_smstcpip_irx, 0, NULL, &ret);
	if ((id < 0) || ret) {
		gNetworkStartup = -1;
		goto fini;
	}
	
	gNetworkStartup = 2;
	
	id=SifExecModuleBuffer(&smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig, &ret);	
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

int getDiscID(void *discID)
{
	int fd;

	fd = fioOpen("discID:", O_RDONLY);
	if (fd < 0)
		return fd;
	
	fioRead(fd, discID, 5);
	fioClose(fd);

	return 1;	
}

void LaunchLoaderElf(char *filename, int mode, int compatflags, int alt_ee_core)
{
	u8 *boot_elf = NULL;
	elf_header_t *eh;
	elf_pheader_t *eph;
	void *pdata;
	int i;
	char *argv[3];
	char config_str[255];
	char *mode_str = NULL;

	clear_game_patches();
	apply_game_patches(filename, mode);

	set_ipconfig();

	SendIrxKernelRAM(mode);

	// NB: LOADER.ELF is embedded
	if (alt_ee_core)
		boot_elf = (u8 *)&alt_loader_elf;
	else
		boot_elf = (u8 *)&loader_elf;
	eh = (elf_header_t *)boot_elf;
	if (_lw((u32)&eh->ident) != ELF_MAGIC)
		while (1);

	eph = (elf_pheader_t *)(boot_elf + eh->phoff);

	// Scan through the ELF's program headers and copy them into RAM, then
	// zero out any non-loaded regions.
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

	// Let's go.
	fioExit();
	SifInitRpc(0);
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	if (mode == USB_MODE)
		mode_str = "USB_MODE";
	else if (mode == ETH_MODE)
		mode_str = "ETH_MODE";
	else if (mode == HDD_MODE)
		mode_str = "HDD_MODE";

	sprintf(config_str, "%s %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d", mode_str, ps2_ip[0], ps2_ip[1], ps2_ip[2], ps2_ip[3], \
		ps2_netmask[0], ps2_netmask[1], ps2_netmask[2], ps2_netmask[3], \
		ps2_gateway[0], ps2_gateway[1], ps2_gateway[2], ps2_gateway[3]);

	char cmask[10];
	snprintf(cmask, 10, "%d", compatflags);
	argv[0] = config_str;	
	argv[1] = filename;
	argv[2] = cmask;

	ExecPS2((void *)eh->entry, 0, 3, argv);
} 

void LaunchGame(TGame *game, int mode, int compatmask, void* gameid)
{
	int i;
	char gamename[33];
	char isoname[32];
	char filename[32];
	char partname[64];
	char config_str[255];
	int fd, r;
		
	sprintf(filename,"%s",game->Image+3);
	
	memset(gamename, 0, 33);
	strncpy(gamename, game->Name, 32);

	sprintf(isoname,"ul.%08X.%s",crc32(gamename),filename);
	
	if (mode == USB_MODE) {
		for (i=0;i<size_usb_cdvdman_irx;i++){
			if(!strcmp((const char*)((u32)&usb_cdvdman_irx+i),"######    GAMESETTINGS    ######")){
				break;
			}
		}
	
		memcpy((void*)((u32)&usb_cdvdman_irx+i),isoname,strlen(isoname)+1);
		memcpy((void*)((u32)&usb_cdvdman_irx+i+33),&game->parts,1);
		memcpy((void*)((u32)&usb_cdvdman_irx+i+34),&game->media,1);
		if (compatmask & COMPAT_MODE_2) {
			u32 alt_read_mode = 1;
			memcpy((void*)((u32)&usb_cdvdman_irx+i+35),&alt_read_mode,1);
		}
		if (compatmask & COMPAT_MODE_5) {
			u32 no_dvddl = 1;
			memcpy((void*)((u32)&usb_cdvdman_irx+i+36),&no_dvddl,4);
		}		
		if (compatmask & COMPAT_MODE_4) {
			u32 no_pss = 1;
			memcpy((void*)((u32)&usb_cdvdman_irx+i+40),&no_pss,4);
		}	

		// game id
		memcpy((void*)((u32)&usb_cdvdman_irx+i+84), &gameid, 5);
		
		int j, offset = 44;
	
		fd = fioDopen(USB_prefix);
		if (fd < 0) {
			init_scr();
			scr_clear();
			scr_printf("\n\t Fatal error opening %s...\n", partname);
			while(1);
		}
		for (j=0; j<game->parts; j++) {
			sprintf(partname,"%s.%02x",isoname, j);
			r = fioIoctl(fd, 0xBEEFC0DE, partname);
			memcpy((void*)((u32)&usb_cdvdman_irx+i+offset),&r,4);
			offset+=4;
		}
		fioDclose(fd);
	}
	else if (mode == ETH_MODE) {
		for (i=0;i<size_smb_cdvdman_irx;i++){
			if(!strcmp((const char*)((u32)&smb_cdvdman_irx+i),"######    GAMESETTINGS    ######")){
				break;
			}
		}
	
		memcpy((void*)((u32)&smb_cdvdman_irx+i),isoname,strlen(isoname)+1);
		memcpy((void*)((u32)&smb_cdvdman_irx+i+33),&game->parts,1);
		memcpy((void*)((u32)&smb_cdvdman_irx+i+34),&game->media,1);
		if (compatmask & COMPAT_MODE_2) {
			u32 alt_read_mode = 1;
			memcpy((void*)((u32)&smb_cdvdman_irx+i+35),&alt_read_mode,1);
		}
		if (compatmask & COMPAT_MODE_5) {
			u32 no_dvddl = 1;
			memcpy((void*)((u32)&smb_cdvdman_irx+i+36),&no_dvddl,4);
		}
		if (compatmask & COMPAT_MODE_4) {
			u32 no_pss = 1;
			memcpy((void*)((u32)&smb_cdvdman_irx+i+40),&no_pss,4);
		}
		
		// game id
		memcpy((void*)((u32)&smb_cdvdman_irx+i+84), &gameid, 5);
		
		for (i=0;i<size_smb_cdvdman_irx;i++){
			if(!strcmp((const char*)((u32)&smb_cdvdman_irx+i),"xxx.xxx.xxx.xxx")){
				break;
			}
		}
		sprintf(config_str, "%d.%d.%d.%d", pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3]);
		memcpy((void*)((u32)&smb_cdvdman_irx+i),config_str,strlen(config_str)+1);	
		memcpy((void*)((u32)&smb_cdvdman_irx+i+16),&gPCPort, 4);
		memcpy((void*)((u32)&smb_cdvdman_irx+i+20), gPCShareName, 32);		
	}

	FlushCache(0);

	LaunchLoaderElf(filename, mode, compatmask, compatmask & COMPAT_MODE_1);
} 

void LaunchHDDGame(hdl_game_info_t *game, void* gameid)
{
	int i;
	char filename[32];

	if (game->dma_type == 0) {
		game->dma_type = UDMA_MODE;
		game->dma_mode = 4;
	}

	hddSetTransferMode(game->dma_type, game->dma_mode);

	sprintf(filename,"%s",game->startup);

	for (i=0;i<size_hdd_cdvdman_irx;i++){
		if(!strcmp((const char*)((u32)&hdd_cdvdman_irx+i),"######    GAMESETTINGS    ######")){
			break;
		}
	}

	/*
	if (game->hdl_compat_flags & HDL_COMPAT_MODE_1) {
		u32 alt_read_mode = 1;
		memcpy((void*)((u32)&hdd_cdvdman_irx+i+35),&alt_read_mode,1);
	}
	if (game->hdl_compat_flags & HDL_COMPAT_MODE_2) {
		u32 no_dvddl = 1;
		memcpy((void*)((u32)&hdd_cdvdman_irx+i+36),&no_dvddl,4);
	}
	*/
	if (game->ops2l_compat_flags & COMPAT_MODE_2) {
		u32 alt_read_mode = 1;
		memcpy((void*)((u32)&hdd_cdvdman_irx+i+35),&alt_read_mode,1);
	}
	if (game->ops2l_compat_flags & COMPAT_MODE_5) {
		u32 no_dvddl = 1;
		memcpy((void*)((u32)&hdd_cdvdman_irx+i+36),&no_dvddl,4);
	}
	if (game->ops2l_compat_flags & COMPAT_MODE_4) {
		u32 no_pss = 1;
		memcpy((void*)((u32)&hdd_cdvdman_irx+i+40),&no_pss,4);
	}

	// game id
	memcpy((void*)((u32)&hdd_cdvdman_irx+i+84), &gameid, 5);

	// patch 48bit flag
	u8 flag_48bit = hddIs48bit() & 0xff;
	memcpy((void*)((u32)&hdd_cdvdman_irx+i+34), &flag_48bit, 1);	

	// patch start_sector
	memcpy((void*)((u32)&hdd_cdvdman_irx+i+44), &game->start_sector, 4);		

	FlushCache(0);

	LaunchLoaderElf(filename, HDD_MODE, game->ops2l_compat_flags, game->ops2l_compat_flags & COMPAT_MODE_1);
} 

int ExecElf(char *path){
	int fd, size, i;
	void *buffer;
	elf_header_t *eh;
	elf_pheader_t *eph;
	char *argv[1];
	void *pdata;

	if ((fd = fioOpen(path, O_RDONLY)) < 0) {
		return -1;
	}

	size = fioLseek(fd, 0, SEEK_END);
	if (!size) {
		fioClose(fd);
		return -1;
	}
	buffer=malloc(size);
	eh = (elf_header_t *)buffer;

	fioLseek(fd, 0, SEEK_SET);

	fioRead(fd, buffer, size);
	fioClose(fd);

	/* Load the ELF into RAM.  */
	if (_lw((u32)&eh->ident) != ELF_MAGIC) {
		return -1;
	}

	eph = (elf_pheader_t *)(buffer + eh->phoff);

	/* Scan through the ELF's program headers and copy them into RAM, then
	   zero out any non-loaded regions.  */
	for (i = 0; i < eh->phnum; i++) {
		if (eph[i].type != ELF_PT_LOAD)
			continue;

		pdata = (void *)(buffer + eph[i].offset);
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz)
			memset(eph[i].vaddr + eph[i].filesz, 0,
					eph[i].memsz - eph[i].filesz);
	}

	/* Let's go.  */
	argv[0] = path;

	fioExit();

	FlushCache(0);
	FlushCache(2);

	ExecPS2((void *)eh->entry, 0, 1, argv);
	
	return 0;
}

#define IRX_NUM 10

//-------------------------------------------------------------- 
void SendIrxKernelRAM(int mode) // Send IOP modules that core must use to Kernel RAM
{
	u32 *total_irxsize = (u32 *)0x80031000;
	void *irxtab = (void *)0x80031010;
	void *irxptr = (void *)0x80031100;
	irxptr_t irxptr_tab[IRX_NUM];
	void *irxsrc[IRX_NUM];
	int i, n;
	u32 irxsize;

	n = 0;
	irxptr_tab[n++].irxsize = size_imgdrv_irx; 
	irxptr_tab[n++].irxsize = size_eesync_irx; 
	if (mode == USB_MODE)
		irxptr_tab[n++].irxsize = size_usb_cdvdman_irx;
	if (mode == ETH_MODE)
		irxptr_tab[n++].irxsize = size_smb_cdvdman_irx;
	if (mode == HDD_MODE)
		irxptr_tab[n++].irxsize = size_hdd_cdvdman_irx;
	irxptr_tab[n++].irxsize = size_cdvdfsv_irx;
	irxptr_tab[n++].irxsize = size_cddev_irx;
	irxptr_tab[n++].irxsize = size_usbd_irx;
	irxptr_tab[n++].irxsize = size_ps2dev9_irx;
	irxptr_tab[n++].irxsize = size_smstcpip_irx;
	irxptr_tab[n++].irxsize = size_smsmap_irx;	
	irxptr_tab[n++].irxsize = size_netlog_irx;
	 	
	n = 0;
	irxsrc[n++] = (void *)&imgdrv_irx;
	irxsrc[n++] = (void *)&eesync_irx;
	if (mode == USB_MODE)
		irxsrc[n++] = (void *)&usb_cdvdman_irx;
	if (mode == ETH_MODE)
		irxsrc[n++] = (void *)&smb_cdvdman_irx;
	if (mode == HDD_MODE)
		irxsrc[n++] = (void *)&hdd_cdvdman_irx;
	irxsrc[n++] = (void *)&cdvdfsv_irx;
	irxsrc[n++] = (void *)&cddev_irx;
	irxsrc[n++] = (void *)usbd_irx;
	irxsrc[n++] = (void *)&ps2dev9_irx;
	irxsrc[n++] = (void *)&smstcpip_irx;
	irxsrc[n++] = (void *)&smsmap_irx;
	irxsrc[n++] = (void *)&netlog_irx;
	
	irxsize = 0;		

	DIntr();
	ee_kmode_enter();
				
	for (i = 0; i <	IRX_NUM; i++) {
		if ((((u32)irxptr + irxptr_tab[i].irxsize) >= 0x80050000) && ((u32)irxptr < 0x80060000))
			irxptr = (void *)0x80060000;
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
