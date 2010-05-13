/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <intrman.h>
#include <ioman.h>
#include <io_common.h>
#include <sifman.h>
#include <sysclib.h>
#include <thsemap.h>
#include <errno.h>

#include "smbman.h"
#include "ioman_add.h"
#include "smb_fio.h"
#include "smb.h"
#include "auth.h"

int smbman_io_sema;

// driver ops func tab
void *smbman_ops[27] = {
	(void*)smb_init,
	(void*)smb_deinit,
	(void*)smb_dummy,
	(void*)smb_open,
	(void*)smb_close,
	(void*)smb_read,
	(void*)smb_write,
	(void*)smb_lseek,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_devctl,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy
};

// driver descriptor
static iop_ext_device_t smbdev = {
	"smb",
	IOP_DT_FS  | IOP_DT_FSEXT,
	1,
	"SMB",
	(struct _iop_ext_device_ops *)&smbman_ops
};

typedef struct {
	iop_file_t 	*f;
	int		smb_fid;
	u32		filesize;
	u32		position;
	u32		mode;
} FHANDLE;

#define MAX_FDHANDLES		64
FHANDLE smbman_fdhandles[MAX_FDHANDLES] __attribute__((aligned(64)));

static ShareEntry_t ShareList __attribute__((aligned(64))); // Keep this aligned for DMA!

//-------------------------------------------------------------- 
int smb_dummy(void)
{
	return -EPERM;
}

//-------------------------------------------------------------- 
int smb_init(iop_device_t *dev)
{
	iop_sema_t smp;

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 1;
	smp.option = 0;

	smbman_io_sema = CreateSema(&smp);

	return 0;
}

//-------------------------------------------------------------- 
int smb_initdev(void)
{
	register int i;
	FHANDLE *fh;
	
	DelDrv(smbdev.name);
	if (AddDrv((iop_device_t *)&smbdev))
		return 1;

	for (i=0; i<MAX_FDHANDLES; i++) {
		fh = (FHANDLE *)&smbman_fdhandles[i];
		fh->f = NULL;
		fh->smb_fid = -1;
		fh->filesize = 0;
		fh->position = 0;
		fh->mode = 0;
	}

	return 0;
}

//-------------------------------------------------------------- 
int smb_deinit(iop_device_t *dev)
{
	DeleteSema(smbman_io_sema);

	return 0;
}

//-------------------------------------------------------------- 
FHANDLE *smbman_getfilefreeslot(void)
{
	register int i;
	FHANDLE *fh;

	for (i=0; i<MAX_FDHANDLES; i++) {
		fh = (FHANDLE *)&smbman_fdhandles[i];
		if (fh->f == NULL)
			return fh;
	}

	return 0;
}

//-------------------------------------------------------------- 
int smb_open(iop_file_t *f, char *filename, int mode, int flags)
{
	register int r = 0;
	FHANDLE *fh;
	int smb_fid;
	u32 filesize;

	if (!filename)
		return -ENOENT;

	WaitSema(smbman_io_sema);

	fh = smbman_getfilefreeslot();
	if (fh) {
		r = smb_OpenAndX(filename, &smb_fid, &filesize, mode);
		if (r == 0) {
			f->privdata = fh;
			fh->f = f;
			fh->smb_fid = smb_fid;
			fh->mode = mode;
			fh->filesize = filesize;
			fh->position = filesize;
			if (fh->mode & O_TRUNC) {
				fh->position = 0;
				fh->filesize = 0;
			}
			r = 0;
		}
		else {
			if (r == -1)
				r = -EIO;
			else if (r == -2)
				r = -EPERM;
			else if (r == -3)
				r = -ENOENT;
			else
				r = -EIO;
		}
	}
	else
		r = -EMFILE;

	SignalSema(smbman_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int smb_close(iop_file_t *f)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int r = 0;

	WaitSema(smbman_io_sema);

	if (fh) {
		r = smb_Close(fh->smb_fid);
		if (r != 0) {
			r = -EIO;
			goto ssema;
		}
		memset(fh, 0, sizeof(FHANDLE));
		fh->smb_fid = -1;
		r = 0;
	}

ssema:
	SignalSema(smbman_io_sema);

	return r;
}

//-------------------------------------------------------------- 
void smb_closeAll(void)
{
	register int i;
	FHANDLE *fh;

	for (i=0; i<MAX_FDHANDLES; i++) {
		fh = (FHANDLE *)&smbman_fdhandles[i];
		if (fh->smb_fid != -1)
			smb_Close(fh->smb_fid);
	}
}

//-------------------------------------------------------------- 
int smb_lseek(iop_file_t *f, u32 pos, int where)
{
	register int r;
	FHANDLE *fh = (FHANDLE *)f->privdata;

	WaitSema(smbman_io_sema);

	switch (where) {
		case SEEK_CUR:
			r = fh->position + pos;
			if (r > fh->filesize) {
				r = -EINVAL;
				goto ssema;
			}
			break;
		case SEEK_SET:
			r = pos;
			if (fh->filesize < pos) {
				r = -EINVAL;
				goto ssema;
			}
			break;
		case SEEK_END:
			r = fh->filesize;
			break;
		default:
			r = -EINVAL;
			goto ssema;
	}

	fh->position = r;

ssema:
	SignalSema(smbman_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int smb_read(iop_file_t *f, void *buf, u32 size)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int r, rpos;
	register u32 nbytes;

	rpos = 0;

	if ((fh->position + size) > fh->filesize)
		size = fh->filesize - fh->position;

	WaitSema(smbman_io_sema);

	while (size) {
		nbytes = MAX_RD_BUF;
		if (size < nbytes)
			nbytes = size;

		r = smb_ReadAndX(fh->smb_fid, fh->position, (void *)(buf + rpos), (u16)nbytes);
		if (r < 0) {
   			rpos = -EIO;
   			goto ssema;
		}

		rpos += nbytes;
		size -= nbytes;
		fh->position += nbytes;
	}

ssema:
	SignalSema(smbman_io_sema);

	return rpos;
}

//-------------------------------------------------------------- 
int smb_write(iop_file_t *f, void *buf, u32 size)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int r, wpos;
	register u32 nbytes;

	if ((!(fh->mode & O_RDWR)) && (!(fh->mode & O_WRONLY)))
		return -EPERM;

	wpos = 0;

	WaitSema(smbman_io_sema);

	while (size) {
		nbytes = MAX_WR_BUF;
		if (size < nbytes)
			nbytes = size;

		r = smb_WriteAndX(fh->smb_fid, fh->position, (void *)(buf + wpos), (u16)nbytes);
		if (r < 0) {
   			wpos = -EIO;
   			goto ssema;
		}

		wpos += nbytes;
		size -= nbytes;
		fh->position += nbytes;
		if (fh->position > fh->filesize)
			fh->filesize += fh->position - fh->filesize;
	}

ssema:
	SignalSema(smbman_io_sema);

	return wpos;
}

//-------------------------------------------------------------- 
void DMA_sendEE(void *buf, int size, void *EE_addr)
{
	SifDmaTransfer_t dmat;
	int oldstate, id;

	dmat.dest = (void *)EE_addr;
	dmat.size = size;
	dmat.src = (void *)buf;
	dmat.attr = 0;

	id = 0;
	while (!id) {
		CpuSuspendIntr(&oldstate);
		id = sceSifSetDma(&dmat, 1);
		CpuResumeIntr(oldstate);
	}
	while (sceSifDmaStat(id) >= 0);
}

//-------------------------------------------------------------- 
static void smb_GetPasswordHashes(smbGetPasswordHashes_in_t *in, smbGetPasswordHashes_out_t *out)
{
	LM_Password_Hash((const unsigned char *)in->password, (unsigned char *)out->LMhash);
	NTLM_Password_Hash((const unsigned char *)in->password, (unsigned char *)out->NTLMhash);
}

//-------------------------------------------------------------- 
static int smb_LogOn(smbLogOn_in_t *logon)
{
	register int r;

	r = smb_NegociateProtocol(logon->serverIP, logon->serverPort, "NT LM 0.12");
	if (r < 0)
		return r;

	r = smb_SessionSetupAndX(logon->User, logon->Password, logon->PasswordType);
	if (r < 0)
		return r;

	return 0;
}

//-------------------------------------------------------------- 
static int smb_LogOff(void)
{
	smb_closeAll();

	return smb_LogOffAndX();
}

//-------------------------------------------------------------- 
static int smb_GetShareList(smbGetShareList_in_t *getsharelist)
{
	register int i, r, sharecount, shareindex;
	char tree_str[256];
	server_specs_t *specs;

	specs = (server_specs_t *)getServerSpecs();

	// Tree Connect on IPC slot
	sprintf(tree_str, "\\\\%s\\IPC$", specs->ServerIP);
	r = smb_TreeConnectAndX(tree_str, NULL, 0);
	if (r < 0)
		return r;

	// does a 1st enum to count shares (+IPC)
	r = smb_NetShareEnum((ShareEntry_t *)&ShareList, 0, 0);
	if (r < 0)
		return r;

	sharecount = r;
	shareindex = 0;

	// now we list the following shares if any 
	for (i=0; i<sharecount; i++) {

		r = smb_NetShareEnum((ShareEntry_t *)&ShareList, i, 1);
		if (r < 0)
			return r;

		// if the entry is not IPC, we send it on EE, and increment shareindex
		if ((strcmp(ShareList.ShareName, "IPC$")) && (shareindex < getsharelist->maxent)) {
			DMA_sendEE((void *)&ShareList, sizeof(ShareList), (void *)(getsharelist->EE_addr + (shareindex * sizeof(ShareEntry_t))));
			shareindex++;
		}
	}

	// disconnect the tree
	r = smb_TreeDisconnect();
	if (r < 0)
		return r;

	// return the number of shares	
	return shareindex;
}

//-------------------------------------------------------------- 
static int smb_OpenShare(smbOpenShare_in_t *openshare)
{
	register int r;
	char tree_str[256];
	server_specs_t *specs;

	specs = (server_specs_t *)getServerSpecs();
	sprintf(tree_str, "\\\\%s\\%s", specs->ServerIP, openshare->ShareName);

	r = smb_TreeConnectAndX(tree_str, openshare->Password, openshare->PasswordType);
	if (r < 0)
		r = -EIO;

	return r;
}

//-------------------------------------------------------------- 
static int smb_CloseShare(void)
{
	smb_closeAll();

	return smb_TreeDisconnect();
}

//-------------------------------------------------------------- 
static int smb_EchoServer(smbEcho_in_t *echo)
{
	return smb_Echo(echo->echo, echo->len);
}

//-------------------------------------------------------------- 
int smb_devctl(iop_file_t *f, const char *devname, int cmd, void *arg, u32 arglen, void *bufp, u32 buflen)
{
	register int r = 0;

	WaitSema(smbman_io_sema);

	switch(cmd) {

		case SMB_DEVCTL_GETPASSWORDHASHES:
			smb_GetPasswordHashes((smbGetPasswordHashes_in_t *)arg, (smbGetPasswordHashes_out_t *)bufp);
			r = 0;
			break;

		case SMB_DEVCTL_LOGON:
			r = smb_LogOn((smbLogOn_in_t *)arg);
			if (r < 0)
				r = -EIO;
			break;

		case SMB_DEVCTL_LOGOFF:
			r = smb_LogOff();
			if (r < 0) {
				if (r == -3)
					r = -EINVAL;
				else
					r = -EIO;
			}
			break;

		case SMB_DEVCTL_GETSHARELIST:
			r = smb_GetShareList((smbGetShareList_in_t *)arg);
			if (r < 0) {
				if (r == -3)
					r = -EINVAL;
				else
					r = -EIO;
			}			
			break;

		case SMB_DEVCTL_OPENSHARE:
			r = smb_OpenShare((smbOpenShare_in_t *)arg);
			if (r < 0)
				r = -EIO;
			break;

		case SMB_DEVCTL_CLOSESHARE:
			r = smb_CloseShare();
			if (r < 0) {
				if (r == -3)
					r = -EINVAL;
				else
					r = -EIO;
			}
			break;

		case SMB_DEVCTL_ECHO:
			r = smb_EchoServer((smbEcho_in_t *)arg);
			if (r < 0)
				r = -EIO;
			break;

		default:
			r = -EINVAL;
	}

	SignalSema(smbman_io_sema);

	return r;
}

