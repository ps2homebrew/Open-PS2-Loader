#include "loader.h"
#include "modmgr.h"

void *imgdrv_irx;
int size_imgdrv_irx;

void *eesync_irx;
int size_eesync_irx;

void *cdvdman_irx;
int size_cdvdman_irx;

void *usbd_irx;
int size_usbd_irx;

void *usbhdfsd_irx;
int size_usbhdfsd_irx;

void *isofs_irx;
int size_isofs_irx;

void *ps2dev9_irx;
int size_ps2dev9_irx;

void *ps2ip_irx;
int size_ps2ip_irx;

void *ps2smap_irx;
int size_ps2smap_irx;

void *netlog_irx;
int size_netlog_irx;

void *smbman_irx;
int size_smbman_irx;

void *dummy_irx;
int size_dummy_irx;

static SifRpcClientData_t _lf_cd;
static int _lf_init  = 0;

/*----------------------------------------------------------------------------------------*/
/* Init LOADFILE RPC.                                                                     */
/*----------------------------------------------------------------------------------------*/
int LoadFileInit()
{
	int r;

	if (_lf_init)
		return 0;

	while ((r = SifBindRpc(&_lf_cd, 0x80000006, 0)) >= 0 && (!_lf_cd.server))
		nopdelay();

	if (r < 0)
		return -E_SIF_RPC_BIND;

	_lf_init = 1;

	return 0;
}


/*----------------------------------------------------------------------------------------*/
/* DeInit LOADFILE RPC.                                                                   */
/*----------------------------------------------------------------------------------------*/
void LoadFileExit()
{
	_lf_init = 0;
	memset(&_lf_cd, 0, sizeof(_lf_cd));
}

/*----------------------------------------------------------------------------------------*/
/* Load an irx module from path without waiting.                                               */
/*----------------------------------------------------------------------------------------*/
int LoadModuleAsync(const char *path, int arg_len, const char *args)
{
	struct _lf_module_load_arg arg;

	if (LoadFileInit() < 0)
		return -E_LIB_API_INIT;

	memset(&arg, 0, sizeof arg);

	strncpy(arg.path, path, LF_PATH_MAX - 1);
	arg.path[LF_PATH_MAX - 1] = 0;

	if ((args) && (arg_len))
	{
		arg.p.arg_len = arg_len > LF_ARG_MAX ? LF_ARG_MAX : arg_len;
		memcpy(arg.args, args, arg.p.arg_len);
	}
	else arg.p.arg_len = 0;

	if (SifCallRpc(&_lf_cd, LF_F_MOD_LOAD, SIF_RPC_M_NOWAIT, &arg, sizeof(arg), &arg, 8, NULL, NULL) < 0)
		return -E_SIF_RPC_CALL;

	return 0;
}

#define IRX_NUM 12

//-------------------------------------------------------------- 
void GetIrxKernelRAM(void) // load needed modules from the kernel ram
{
	int n;
	void *irx_tab = (void *)0x80030010;
	irxptr_t irxptr_tab[IRX_NUM];

	DIntr();
	ee_kmode_enter();
	
	memcpy(&irxptr_tab[0], irx_tab, sizeof(irxptr_tab));

	ee_kmode_exit();
	EIntr();
		
	n = 0;
	size_imgdrv_irx = irxptr_tab[n++].irxsize; 
	size_eesync_irx = irxptr_tab[n++].irxsize; 	
	size_cdvdman_irx = irxptr_tab[n++].irxsize; 
	size_usbd_irx = irxptr_tab[n++].irxsize;
	size_usbhdfsd_irx = irxptr_tab[n++].irxsize;
	size_isofs_irx = irxptr_tab[n++].irxsize;
	size_ps2dev9_irx = irxptr_tab[n++].irxsize; 
	size_ps2ip_irx = irxptr_tab[n++].irxsize;
	size_ps2smap_irx = irxptr_tab[n++].irxsize;
	size_netlog_irx = irxptr_tab[n++].irxsize;
	size_smbman_irx = irxptr_tab[n++].irxsize;
	size_dummy_irx = irxptr_tab[n++].irxsize;	
			
	n = 0;
	imgdrv_irx = (void *)irxptr_tab[n++].irxaddr;
	eesync_irx = (void *)irxptr_tab[n++].irxaddr;
	cdvdman_irx = (void *)irxptr_tab[n++].irxaddr;
	usbd_irx = (void *)irxptr_tab[n++].irxaddr;
	usbhdfsd_irx = (void *)irxptr_tab[n++].irxaddr;
	isofs_irx = (void *)irxptr_tab[n++].irxaddr;
	ps2dev9_irx = (void *)irxptr_tab[n++].irxaddr;
	ps2ip_irx = (void *)irxptr_tab[n++].irxaddr;
	ps2smap_irx = (void *)irxptr_tab[n++].irxaddr;
	netlog_irx = (void *)irxptr_tab[n++].irxaddr;
	smbman_irx = (void *)irxptr_tab[n++].irxaddr;
	dummy_irx = (void *)irxptr_tab[n++].irxaddr;	
}	

// ------------------------------------------------------------------------
int LoadIRXfromKernel(void *irxkernelmem, int irxsize, int arglen, char *argv)
{	
	DIntr();
	ee_kmode_enter();
	
	memcpy(g_buf, irxkernelmem, irxsize);
	
	ee_kmode_exit();
	EIntr();
		
	return LoadMemModule(g_buf, irxsize, arglen, argv);
}

/*----------------------------------------------------------------------------------------*/
/* Load an irx module from a EE buffer.                                                          */
/*----------------------------------------------------------------------------------------*/
int LoadMemModule(void *modptr, unsigned int modsize, int arg_len, const char *args)
{
	SifDmaTransfer_t sifdma;
	void            *iopmem;
	int              dma_id;

	if (LoadFileInit() < 0)
		return -E_LIB_API_INIT;

	/* Round the size up to the nearest 16 bytes. */
	// modsize = (modsize + 15) & -16;

	iopmem = SifAllocIopHeap(modsize);
	if (iopmem == NULL) return -E_IOP_NO_MEMORY;

	sifdma.src  = modptr;
	sifdma.dest = iopmem;
	sifdma.size = modsize;
	sifdma.attr = 0;

	SifWriteBackDCache(modptr, modsize);

	do
	{
		dma_id = SifSetDma(&sifdma, 1);
	} while (!dma_id);

	while (SifDmaStat(dma_id) > 0) {;}

	struct _lf_module_buffer_load_arg arg;

	memset(&arg, 0, sizeof arg);

	arg.p.ptr = iopmem;
	if ((args) && (arg_len))
	{
		arg.q.arg_len = arg_len > LF_ARG_MAX ? LF_ARG_MAX : arg_len;
		memcpy(arg.args, args, arg.q.arg_len);
	}
	else arg.q.arg_len = 0;

	if (SifCallRpc(&_lf_cd, LF_F_MOD_BUF_LOAD, 0, &arg, sizeof(arg), &arg, 8, NULL, NULL) < 0)
		return -E_SIF_RPC_CALL;

	SifFreeIopHeap(iopmem); 

	return arg.p.result;
}

/*----------------------------------------------------------------------------------------*/
/* Load an elf file from EE buffer.                                                          */
/*----------------------------------------------------------------------------------------*/
int LoadElf(const char *path, t_ExecData *data)
{
	struct _lf_elf_load_arg arg;

	if (LoadFileInit() < 0)
		return -E_LIB_API_INIT;

	u32 secname = 0x6c6c61;  /* "all" */

	strncpy(arg.path, path, LF_PATH_MAX - 1);
	strncpy(arg.secname, (char*)&secname, LF_ARG_MAX - 1);
	arg.path[LF_PATH_MAX - 1] = 0;
	arg.secname[LF_ARG_MAX - 1] = 0;

	if (SifCallRpc(&_lf_cd, LF_F_ELF_LOAD, 0, &arg, sizeof arg, &arg, sizeof(t_ExecData), NULL, NULL) < 0)
		return -E_SIF_RPC_CALL;

	if (arg.p.result < 0)
		return arg.p.result;

	if (data) {
		data->epc = arg.p.epc;
		data->gp  = arg.gp;
	}

	return 0;
}
