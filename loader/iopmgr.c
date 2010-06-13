/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "loader.h"
#include "iopmgr.h"

extern void *imgdrv_irx;
extern int size_imgdrv_irx;

extern void *usbd_irx;
extern int size_usbd_irx;

extern void *cdvdman_irx;
extern int size_cdvdman_irx;

extern void *cdvdfsv_irx;
extern int size_cdvdfsv_irx;

extern void *eesync_irx;
extern int size_eesync_irx;

extern void *smstcpip_irx;
extern int size_smstcpip_irx;

extern void *smsmap_irx;
extern int size_smsmap_irx;

extern void *udptty_irx;
extern int size_udptty_irx;

/*----------------------------------------------------------------------------------------*/
/*
void list_modules(void)
{
	int c = 0;
	smod_mod_info_t info;
	u8 name[60];

	if (!smod_get_next_mod(0, &info))
		return;
	scr_printf("\n\t List of modules currently loaded in the system:\n\n");
	do {
		smem_read(info.name, name, sizeof name);
		if (!(c & 1)) scr_printf(" ");
		scr_printf("   \t%-21.21s  %2d  %3x%c", name, info.id, info.version, (++c & 1) ? ' ' : '\n');
	} while (smod_get_next_mod(&info, &info) != 0);
}
*/
/*----------------------------------------------------------------------------------------*/
/* Reset IOP processor. This fonction hook reset iop if an update sequence is requested.  */
/*----------------------------------------------------------------------------------------*/
int New_Reset_Iop(const char *arg, int flag){
	void   *rom_iop;
	int     i, j, fd=0; //r
	ioprp_t ioprp_img;
	char    ioprp_path[0x50];
	int eeloadcnf_reset = 0;		
	
	DPRINTF("New_Reset_Iop start!\n");
	if(!DisableDebug)
		GS_BGCOLOUR = 0xFF00FF;
	
	iop_reboot_count++;
	DPRINTF("IOP reboot count = %d\n", iop_reboot_count);
	
	SifInitRpc(0);
	
	if (iop_reboot_count > 2) { 
		// above 2nd IOP reset, we can't be sure we'll be able to read IOPRP without 
		// Resetting IOP (game IOPRP is loaded at 2nd reset), so...
		// Reseting IOP.
		
		SifExitRpc();
		SifExitIopHeap();
		LoadFileExit();

		// Reseting IOP.
		while (!Reset_Iop("rom0:UDNL rom0:EELOADCNF", 0)) {;}
		while (!Sync_Iop()){;}

		SifInitRpc(0);
		SifInitIopHeap();
		LoadFileInit();
		Sbv_Patch();

		LoadIRXfromKernel(cdvdman_irx, size_cdvdman_irx, 0, NULL);

		if(!DisableDebug)
			GS_BGCOLOUR = 0x00A5FF;
			
		if (GameMode == USB_MODE) {
			LoadIRXfromKernel(usbd_irx, size_usbd_irx, 0, NULL);
			delay(3);
#ifdef __LOAD_NET_MODULES
			LoadIRXfromKernel(smstcpip_irx, size_smstcpip_irx, 0, NULL);
			LoadIRXfromKernel(smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig);
			LoadIRXfromKernel(udptty_irx, size_udptty_irx, 0, NULL);
#endif
		}
		else if (GameMode == ETH_MODE) {
			LoadIRXfromKernel(smstcpip_irx, size_smstcpip_irx, 0, NULL);
			LoadIRXfromKernel(smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig);
#ifdef __LOAD_NET_MODULES
			LoadIRXfromKernel(udptty_irx, size_udptty_irx, 0, NULL);
#endif
		}
		else if (GameMode == HDD_MODE) {
#ifdef __LOAD_NET_MODULES
			LoadIRXfromKernel(smstcpip_irx, size_smstcpip_irx, 0, NULL);
			LoadIRXfromKernel(smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig);
			LoadIRXfromKernel(udptty_irx, size_udptty_irx, 0, NULL);
#endif
		}
	}

	// check for reboot with IOPRP from cdrom
	char *ioprp_pattern = "rom0:UDNL cdrom";
	for (i=0; i<0x50; i++) {
		for (j=0; j<15; j++) {
			if (arg[i+j] != ioprp_pattern[j])
				break;
		}
		if (j == 15) {
			// get IOPRP img file path
			strcpy(ioprp_path, &arg[i+10]);
			break;
		}
	}

	// check for reboot with IOPRP from rom	
	char *eeloadcnf_pattern = "rom0:UDNL rom";
	for (i=0; i<0x50; i++) {
		for (j=0; j<13; j++) {
			if (arg[i+j] != eeloadcnf_pattern[j])
				break;
		}
		if (j == 13) {
			strcpy(ioprp_path, &arg[i+10]);
			eeloadcnf_reset = 1;
			break;
		}
	}

	if (eeloadcnf_reset) {
		ioprp_img.data_in = (void *)g_buf;
		Build_EELOADCNF_Img(&ioprp_img);
	}
	else {
		DPRINTF("Reading IOPRP: '%s'\n", ioprp_path);

		fioInit();
		fd = open(ioprp_path, O_RDONLY);
		if (fd < 0){
			DPRINTF("Failed to open IOPRP...\n");
			if(!DisableDebug)
				GS_BGCOLOUR = 0x000080;

			// some games like SOCOM3 uses faulty IOPRP path like "cdrom0:\RUN\IRX\DNAS300.IMGG;1"
			// this part ensure it will not get stucked on red screen
			char *p = strrchr(ioprp_path, '.');
			if (p) {
				p[4] = ';';
				p[5] = '1';
				p[6] = 0;

				fd = open(ioprp_path, O_RDONLY);
			}

			if (fd < 0)
				while (1){;}
		}

		ioprp_img.size_in = lseek(fd, 0, SEEK_END);
		DPRINTF("IOPRP size: %d bytes\n", ioprp_img.size_in);
		if (ioprp_img.size_in % 0x40)
			ioprp_img.size_in = (ioprp_img.size_in & 0xffffffc0) + 0x40;

		lseek(fd, 0, SEEK_SET);

		ioprp_img.data_in  = (void *)g_buf;
		//if (eeloadcnf_reset)
		//	ioprp_img.data_out = (void *)(g_buf+102400);
		//else {	
			ioprp_img.data_out = ioprp_img.data_in;
			ioprp_img.size_out = ioprp_img.size_in;
		//}

		read(fd, ioprp_img.data_in , ioprp_img.size_in);

		close(fd);
		fioExit();
	
		DPRINTF("IOPRP readed\n");

		//if (eeloadcnf_reset) {
		//	DPRINTF("Patching EELOADCNF Image...\n");
		//	r = Patch_EELOADCNF_Img(&ioprp_img);
		//	if (r == 0){
		//		DPRINTF("Patching failed!\n");
		//		if(!DisableDebug)
		//			GS_BGCOLOUR = 0x00FF00;
		//		while (1){;}
		//	}
		//}
		//else {
			DPRINTF("Patching CDVDMAN...\n");
			Patch_Mod(&ioprp_img, "CDVDMAN", cdvdman_irx, size_cdvdman_irx);
			DPRINTF("Patching CDVDFSV...\n");
			Patch_Mod(&ioprp_img, "CDVDFSV", cdvdfsv_irx, size_cdvdfsv_irx);
			DPRINTF("Patching EESYNC...\n");
			Patch_Mod(&ioprp_img, "EESYNC", eesync_irx, size_eesync_irx);
		//}
	}

	DPRINTF("Exiting services...\n");
	SifExitRpc();
	SifExitIopHeap();
	LoadFileExit();
	
	// Reseting IOP.
	DPRINTF("Resetting IOP...\n");
	while (!Reset_Iop("rom0:UDNL rom0:EELOADCNF", 0)) {;}
	while (!Sync_Iop()){;}

	DPRINTF("Init services...\n");
	SifInitRpc(0);
	SifInitIopHeap();
	LoadFileInit();
	DPRINTF("Applying Sbv patches...\n");
	Sbv_Patch();
	
	DPRINTF("Sending patched IOPRP on IOP and try to reset with...\n");
	rom_iop = SifAllocIopHeap(ioprp_img.size_out);
	
	if (rom_iop){
		CopyToIop(ioprp_img.data_out, ioprp_img.size_out, rom_iop);
		
		// get back imgdrv from kernel ram		
		DIntr();
		ee_kmode_enter();
		memcpy(g_buf, imgdrv_irx, size_imgdrv_irx);
		ee_kmode_exit();
		EIntr();

		*(u32*)(&g_buf[0x180]) = (u32)rom_iop;
		*(u32*)(&g_buf[0x184]) = ioprp_img.size_out;
		FlushCache(0);

		LoadMemModule(g_buf, size_imgdrv_irx, 0, NULL);
		LoadModuleAsync("rom0:UDNL", 5, "img0:");

		SifExitRpc();
		LoadFileExit();

		DIntr();
		ee_kmode_enter();
		Old_SifSetReg(SIF_REG_SMFLAG, 0x10000);
		Old_SifSetReg(SIF_REG_SMFLAG, 0x20000);
		Old_SifSetReg(0x80000002, 0);
		Old_SifSetReg(0x80000000, 0);
		ee_kmode_exit();
		EIntr();
	}else{
		DPRINTF("IOP memory allocation (%d bytes) failed!\n", ioprp_img.size_out);
		if(!DisableDebug)
			GS_BGCOLOUR = 0xFF0000;
		while (1){;}
	}

	while (!Sync_Iop()) {;}

	SifExitIopHeap();
	DPRINTF("Init services...\n");
	SifInitRpc(0);
	SifInitIopHeap();
	LoadFileInit();
	DPRINTF("Applying Sbv patches...\n");
	Sbv_Patch();

	if(!DisableDebug)
		GS_BGCOLOUR = 0x00FFFF;

	DPRINTF("Loading extra IOP modules...\n");
	if (GameMode == USB_MODE) {
		LoadIRXfromKernel(usbd_irx, size_usbd_irx, 0, NULL);
		delay(3);
#ifdef __LOAD_NET_MODULES
		LoadIRXfromKernel(smstcpip_irx, size_smstcpip_irx, 0, NULL);
		LoadIRXfromKernel(smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig);
		LoadIRXfromKernel(udptty_irx, size_udptty_irx, 0, NULL);
#endif
	}
	else if (GameMode == ETH_MODE) {
		LoadIRXfromKernel(smstcpip_irx, size_smstcpip_irx, 0, NULL);
		LoadIRXfromKernel(smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig);
#ifdef __LOAD_NET_MODULES
		LoadIRXfromKernel(udptty_irx, size_udptty_irx, 0, NULL);
#endif
	}	
	else if (GameMode == HDD_MODE) {
#ifdef __LOAD_NET_MODULES
		LoadIRXfromKernel(smstcpip_irx, size_smstcpip_irx, 0, NULL);
		LoadIRXfromKernel(smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig);
		LoadIRXfromKernel(udptty_irx, size_udptty_irx, 0, NULL);
#endif
	}	

	FlushCache(0);	

	DPRINTF("Exiting services...\n");
	SifExitRpc();
	SifExitIopHeap();
	LoadFileExit();

	DPRINTF("New_Reset_Iop complete!\n");
	// we have 4 SifSetReg calls to skip in ELF's SifResetIop, not when we use it ourselves
	if (set_reg_disabled)
		set_reg_hook = 4;

 return 1;
}

/*----------------------------------------------------------------------------------------*/
/* Reset IOP processor. This fonction replace SifIopReset from Ps2Sdk                     */
/*----------------------------------------------------------------------------------------*/
int Reset_Iop(const char *arg, int flag){
	SifCmdResetData reset_pkt;
	struct t_SifDmaTransfer dmat;

	SifExitRpc();

	SifStopDma();

	memset(&reset_pkt, 0, sizeof(SifCmdResetData));

	reset_pkt.chdr.psize = sizeof(SifCmdResetData);
	reset_pkt.chdr.fcode = 0x80000003;

	reset_pkt.flag = flag;

	if (arg != NULL){
		strncpy(reset_pkt.arg, arg, 0x50-1);
		reset_pkt.arg[0x50-1] = '\0';
		reset_pkt.size = strlen(reset_pkt.arg) + 1;
	}

	dmat.src  = &reset_pkt;
	dmat.dest = (void *)SifGetReg(0x80000000); // SIF_REG_SUBADDR
	dmat.size = sizeof reset_pkt;
	dmat.attr = 0x40 | SIF_DMA_INT_O;
	SifWriteBackDCache(&reset_pkt, sizeof reset_pkt);

	DIntr();
	ee_kmode_enter();
	if (!Old_SifSetDma(&dmat, 1)){
		ee_kmode_exit();
		EIntr();
		return 0;
	}

	Old_SifSetReg(SIF_REG_SMFLAG, 0x10000);
	Old_SifSetReg(SIF_REG_SMFLAG, 0x20000);
	Old_SifSetReg(0x80000002, 0);
	Old_SifSetReg(0x80000000, 0);
	ee_kmode_exit();
	EIntr();

	return 1;
}

/*----------------------------------------------------------------------------------------*/
/* Synchronize IOP processor. This fonction replace SifIopReset from Ps2Sdk                     */
/*----------------------------------------------------------------------------------------*/
int Sync_Iop(void){
	if (SifGetReg(SIF_REG_SMFLAG) & 0x40000){
		return 1;
	}
	return 0;
}
