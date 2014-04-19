/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include "ee_core.h"
#include "modmgr.h"
#include "util.h"

void *ioprp_img;
int size_ioprp_img;

void *udnl_irx;
int size_udnl_irx;

void *imgdrv_irx;
int size_imgdrv_irx;

#ifdef VMC
void *mcemu_irx;
int size_mcemu_irx;
#endif

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

static SifRpcClientData_t _lf_cd;
static int _lf_init  = 0;

typedef struct {
	void *irxaddr;
	unsigned int irxsize;
} irxptr_t;

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

//--------------------------------------------------------------
void InitModulePointers(void)
{
	int n;
	irxptr_t *irxptr_tab = *(irxptr_t **)0x00088000;

	n = 0;
	size_ioprp_img = irxptr_tab[n++].irxsize;	//EESYNC: 1.4K, CDVDMAN: ~30K, CDVDFSV: 9.1.K
	size_udnl_irx = irxptr_tab[n++].irxsize;	//7.4K
	size_imgdrv_irx = irxptr_tab[n++].irxsize;	//1.2K
	size_usbd_irx = irxptr_tab[n++].irxsize;	//23.6K
	size_smsmap_irx = irxptr_tab[n++].irxsize;	//8.4K
	size_udptty_irx = irxptr_tab[n++].irxsize;	//3.5K
	size_ioptrap_irx = irxptr_tab[n++].irxsize;	//4.5K
	size_smstcpip_irx = irxptr_tab[n++].irxsize;	//62.9
#ifdef VMC
	size_mcemu_irx = irxptr_tab[n++].irxsize;
#endif

	n = 0;
	ioprp_img = (void *)irxptr_tab[n++].irxaddr;
	udnl_irx = (void *)irxptr_tab[n++].irxaddr;
	imgdrv_irx = (void *)irxptr_tab[n++].irxaddr;
	usbd_irx = (void *)irxptr_tab[n++].irxaddr;
	smsmap_irx = (void *)irxptr_tab[n++].irxaddr;
	udptty_irx = (void *)irxptr_tab[n++].irxaddr;
	ioptrap_irx = (void *)irxptr_tab[n++].irxaddr;
	smstcpip_irx = (void *)irxptr_tab[n++].irxaddr;
#ifdef VMC
	mcemu_irx = (void *)irxptr_tab[n++].irxaddr;
#endif
}

/*----------------------------------------------------------------------------------------*/
/* Load an irx module from an EE buffer.                                                  */
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

	//All IOP modules should have already been written back to RAM by the FlushCache() calls within the GUI and the CRT.
	//SifWriteBackDCache(modptr, modsize);

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
/* Load an ELF file from the specified path.                                              */
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
/* Find and change a module's name.                                                       */
/*----------------------------------------------------------------------------------------*/
void ChangeModuleName(const char *name, const char *newname)
{
	u8 search_name[60];
	smod_mod_info_t info;
	int len;

	if (!smod_get_next_mod(NULL, &info))
		return;

	len = strlen(name);

	do {
		smem_read(info.name, search_name, sizeof(search_name));

		if (!_memcmp(search_name, name, len)) {
			strncpy(search_name, newname, sizeof(search_name));
			search_name[sizeof(search_name)-1] = '\0';
			len=strlen(search_name);
			SyncDCache(search_name, search_name+len);
			smem_write(info.name, search_name, len);
			break;
		}
	} while (smod_get_next_mod(&info, &info));
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
