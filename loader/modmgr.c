/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "loader.h"
#include "modmgr.h"

void *imgdrv_irx;
int size_imgdrv_irx;

void *eesync_irx;
int size_eesync_irx;

void *cdvdman_irx;
int size_cdvdman_irx;

void *cdvdfsv_irx;
int size_cdvdfsv_irx;

void *cddev_irx;
int size_cddev_irx;

void *usbd_irx;
int size_usbd_irx;

void *ps2dev9_irx;
int size_ps2dev9_irx;

void *smstcpip_irx;
int size_smstcpip_irx;

void *smsmap_irx;
int size_smsmap_irx;

void *udptty_irx;
int size_udptty_irx;

void *ioptrap_irx;
int size_ioptrap_irx;

void *smbman_irx;
int size_smbman_irx;

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
/* Load an irx module from path with waiting.                                             */
/*----------------------------------------------------------------------------------------*/
int LoadModule(const char *path, int arg_len, const char *args)
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

	if (SifCallRpc(&_lf_cd, LF_F_MOD_LOAD, 0x0, &arg, sizeof(arg), &arg, 8, NULL, NULL) < 0)
		return -E_SIF_RPC_CALL;

	return arg.p.result;
}


/*----------------------------------------------------------------------------------------*/
/* Load an irx module from path without waiting.                                          */
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

#define IRX_NUM 10

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
	size_cdvdfsv_irx = irxptr_tab[n++].irxsize;
	size_cddev_irx = irxptr_tab[n++].irxsize;
	size_usbd_irx = irxptr_tab[n++].irxsize;
	size_smstcpip_irx = irxptr_tab[n++].irxsize;
	size_smsmap_irx = irxptr_tab[n++].irxsize;
	size_udptty_irx = irxptr_tab[n++].irxsize;
	size_ioptrap_irx = irxptr_tab[n++].irxsize;
			
	n = 0;
	imgdrv_irx = (void *)irxptr_tab[n++].irxaddr;
	eesync_irx = (void *)irxptr_tab[n++].irxaddr;
	cdvdman_irx = (void *)irxptr_tab[n++].irxaddr;
	cdvdfsv_irx = (void *)irxptr_tab[n++].irxaddr;
	cddev_irx = (void *)irxptr_tab[n++].irxaddr;
	usbd_irx = (void *)irxptr_tab[n++].irxaddr;
	smstcpip_irx = (void *)irxptr_tab[n++].irxaddr;
	smsmap_irx = (void *)irxptr_tab[n++].irxaddr;
	udptty_irx = (void *)irxptr_tab[n++].irxaddr;
	ioptrap_irx = (void *)irxptr_tab[n++].irxaddr;
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
/* Load an irx module from a EE buffer.                                                   */
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

	while (SifDmaStat(dma_id) >= 0) {;}

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
/* Load an elf file from EE buffer.                                                       */
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

/*----------------------------------------------------------------------------------------*/
/* Find and change a module name.                                                         */
/*----------------------------------------------------------------------------------------*/
void ChangeModuleName(const char *name, const char *newname)
{
	u8 search_name[60];
	smod_mod_info_t info;

	if (!smod_get_next_mod(0, &info))
		return;

	int len = strlen(name);

	do {
		smem_read(info.name, search_name, sizeof(search_name));

		if (!_memcmp(search_name, name, len)) {
			strncpy(search_name, newname, sizeof(search_name));
			search_name[sizeof(search_name)-1] = 0;
			smem_write(info.name, search_name, strlen(search_name));
			break;
		}
	} while (smod_get_next_mod(&info, &info));

	FlushCache(0);
}

/*----------------------------------------------------------------------------------------*/
/* List modules currently loaded in the system.                                           */
/*----------------------------------------------------------------------------------------*/
#ifdef __EESIO_DEBUG
void ListModules(void)
{
	int c = 0;
	smod_mod_info_t info;
	u8 name[60];

	if (!smod_get_next_mod(0, &info))
		return;
	DPRINTF("List of modules currently loaded in the system:\n");
	do {
		smem_read(info.name, name, sizeof name);
		if (!(c & 1)) DPRINTF(" ");
		DPRINTF("   %-21.21s  %2d  %3x%c", name, info.id, info.version, (++c & 1) ? ' ' : '\n');
	} while (smod_get_next_mod(&info, &info));
}
#endif

