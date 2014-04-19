/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include <iopcontrol.h>

#include "ee_core.h"
#include "iopmgr.h"
#include "modmgr.h"
#include "util.h"
#include "syshook.h"
#include "smbauth.h"

extern void *ioprp_img;
extern int size_ioprp_img;

extern void *imgdrv_irx;
extern int size_imgdrv_irx;

extern void *usbd_irx;
extern int size_usbd_irx;

#ifdef VMC
extern void *mcemu_irx;
extern int size_mcemu_irx;
#endif

extern void *smstcpip_irx;
extern int size_smstcpip_irx;

extern void *smsmap_irx;
extern int size_smsmap_irx;

extern void *udptty_irx;
extern int size_udptty_irx;

extern void *ioptrap_irx;
extern int size_ioptrap_irx;

extern int _iop_reboot_count;

static void ResetIopBuffer(void *IOPRP_img, unsigned int size_IOPRP_img, const char *args, unsigned int arglen){
	void *pIOP_buffer;
	unsigned int length_rounded, CommandLen;
	char command[RESET_ARG_MAX+1];

	if (GameMode == ETH_MODE)
		start_smbauth_thread();

	if(arglen>0){
		_strcpy(command, args);
		_strcpy(&command[arglen+1], "img0:");
		CommandLen=arglen+6;
	}
	else{
		_strcpy(command, "img0:");
		CommandLen=5;
	}

	length_rounded=(size_IOPRP_img+0xF)&~0xF;
	pIOP_buffer=SifAllocIopHeap(length_rounded);

	CopyToIop(IOPRP_img, length_rounded, pIOP_buffer);

	*(void**)(UNCACHED_SEG(&((unsigned char*)imgdrv_irx)[0x180])) = pIOP_buffer;
	*(u32*)(UNCACHED_SEG(&((unsigned char*)imgdrv_irx)[0x184])) = size_IOPRP_img;

	LoadMemModule(imgdrv_irx, size_imgdrv_irx, 0, NULL);

	//See the comment below in Reset_Iop() regarding this flag.
/*	DIntr();
	ee_kmode_enter();
	Old_SifSetReg(SIF_REG_SMFLAG, 0x40000);
	ee_kmode_exit();
	EIntr(); */

	LoadModuleAsync("rom0:UDNL", CommandLen, command);

	DIntr();
	ee_kmode_enter();
	Old_SifSetReg(SIF_REG_SMFLAG, 0x30000);
	Old_SifSetReg(0x80000002, 0);
	Old_SifSetReg(0x80000000, 0);
	ee_kmode_exit();
	EIntr();

	LoadFileExit();	//OPL's integrated LOADFILE RPC does not automatically unbind itself after IOP resets.

	_iop_reboot_count++; // increment reboot counter to allow RPC clients to detect unbinding!

	while (!SifIopSync()) {;}

	SifInitRpc(0);
	SifInitIopHeap();
	LoadFileInit();
	sbv_patch_enable_lmb();

	DPRINTF("Loading extra IOP modules...\n");
	if (GameMode == USB_MODE) {
		LoadMemModule(usbd_irx, size_usbd_irx, 0, NULL);
		delay(USBDelay);
	}
	else if (GameMode == ETH_MODE) {
		LoadMemModule(smstcpip_irx, size_smstcpip_irx, 0, NULL);
		LoadMemModule(smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig);
	}

#ifdef __LOAD_DEBUG_MODULES
	if(GameMode != ETH_MODE) {
		LoadMemModule(smstcpip_irx, size_smstcpip_irx, 0, NULL);
		LoadMemModule(smsmap_irx, size_smsmap_irx, g_ipconfig_len, g_ipconfig);
	}
	LoadMemModule(udptty_irx, size_udptty_irx, 0, NULL);
	LoadMemModule(ioptrap_irx, size_ioptrap_irx, 0, NULL);
#endif
}

/*----------------------------------------------------------------------------------------*/
/* Reset IOP to include our modules.							  */
/*----------------------------------------------------------------------------------------*/
int New_Reset_Iop(const char *arg, int arglen)
{
	DPRINTF("New_Reset_Iop start!\n");
	if(!DisableDebug)
		GS_BGCOLOUR = 0xFF00FF;	//Purple

	SifInitRpc(0);

	iop_reboot_count++;

	// Reseting IOP.
	while (!Reset_Iop(NULL, 0)) {;}
	while (!SifIopSync()){;}

	SifInitRpc(0);
	SifInitIopHeap();
	LoadFileInit();
	sbv_patch_enable_lmb();

	ResetIopBuffer(ioprp_img, size_ioprp_img, NULL, 0);
	if(!DisableDebug)
		GS_BGCOLOUR = 0x00A5FF;	//Orange

	if(arglen>0){
		if (GameMode == ETH_MODE){ 
			//Workaround for SMB mode's inability to communicate with the EE during the 2nd IOP reset, presumably because the SIF cannot be used after some of the SMFLAG bits are set.
			//By accessing cdrom0 here, CDVDMAN will initialize itself here instead of during the IOP reset. Everything works properly then.
			fioInit();
			fioOpen("cdrom0:\\sce_dev5;1", O_RDONLY);
			fioExit();
		}

		ResetIopBuffer(ioprp_img, size_ioprp_img, &arg[10], arglen-10);
		if(!DisableDebug)
			GS_BGCOLOUR = 0x00FFFF;	//Yellow
	}

#ifdef VMC
	if ((iop_reboot_count >= 2) && size_mcemu_irx) {
		LoadMemModule(mcemu_irx, size_mcemu_irx, 0, NULL);
	}
#endif

	if (g_compat_mask & COMPAT_MODE_8)
		ChangeModuleName("dev9", "cdvdman");


	DPRINTF("Exiting services...\n");
	SifExitIopHeap();
	LoadFileExit();
	SifExitRpc();

	DPRINTF("New_Reset_Iop complete!\n");
	// we have 4 SifSetReg calls to skip in ELF's SifResetIop, not when we use it ourselves
	if (set_reg_disabled)
		set_reg_hook = 4;

	if(!DisableDebug)
		GS_BGCOLOUR = 0x000000;	//Black

	return 1;
}

/*----------------------------------------------------------------------------------------*/
/* Reset IOP. This function replaces SifIopReset from the PS2SDK                          */
/*----------------------------------------------------------------------------------------*/
int Reset_Iop(const char *arg, int mode)
{
	struct _iop_reset_pkt reset_pkt;  /* Implicitly aligned. */
	struct t_SifDmaTransfer dmat;

	_iop_reboot_count++; // increment reboot counter to allow RPC clients to detect unbinding!

	SifStopDma();

	memset(&reset_pkt, 0, sizeof reset_pkt);

	reset_pkt.header.size = sizeof reset_pkt;
	reset_pkt.header.cid  = 0x80000003;

	reset_pkt.mode = mode;
	if (arg != NULL) {
		strncpy(reset_pkt.arg, arg, RESET_ARG_MAX);
		reset_pkt.arg[RESET_ARG_MAX] = '\0';

		reset_pkt.arglen = strlen(reset_pkt.arg) + 1;
	}

	dmat.src  = &reset_pkt;
	dmat.dest = (void *)SifGetReg(0x80000000);
	dmat.size = sizeof(reset_pkt);
	dmat.attr = 0x40 | SIF_DMA_INT_O;
	SifWriteBackDCache(&reset_pkt, sizeof(reset_pkt));

	DIntr();
	ee_kmode_enter();
	/*	Don't acknowledge this flag here. Sony had changed where this flag is acknowledged after a certain point, which might result in the SIF crashing terribly in some games upon the next IOP reset. :(
		The SIF implementation in every SDK version will be happy if this flag is not acknowledged here because the flag's state will be taken care of by the game's sceSifIopReset() function, and again when its sceSifIopSync() function runs. */
//	Old_SifSetReg(SIF_REG_SMFLAG, 0x40000);

	if (!Old_SifSetDma(&dmat, 1)){
		ee_kmode_exit();
		EIntr();
		return 0;
	}

	Old_SifSetReg(SIF_REG_SMFLAG, 0x30000);
	Old_SifSetReg(0x80000002, 0);
	Old_SifSetReg(0x80000000, 0);
	ee_kmode_exit();
	EIntr();

	return 1;
}
