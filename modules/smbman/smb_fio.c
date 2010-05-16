/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <intrman.h>
#include <ioman.h>
#include <io_common.h>
#include <sifman.h>
#include <sys/stat.h>
#include <sysclib.h>
#include <thbase.h>
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
	(void*)smb_getstat,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_lseek64,
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
	s64		filesize;
	s64		position;
	u32		mode;
} FHANDLE;

#define MAX_FDHANDLES		64
FHANDLE smbman_fdhandles[MAX_FDHANDLES] __attribute__((aligned(64)));

static ShareEntry_t ShareList __attribute__((aligned(64))); // Keep this aligned for DMA!

static int keepalive_mutex = -1;
static int keepalive_inited = 0;
static int keepalive_locked = 1;
static int keepalive_tid;
static iop_sys_clock_t keepalive_sysclock;

//-------------------------------------------------------------------------
// Timer Interrupt handler for Echoing the server every 3 seconds, when
// not already doing IO, and counting after an IO operation has finished
//
static unsigned int timer_intr_handler(void *args)
{
	iSignalSema(keepalive_mutex);
	iSetAlarm(&keepalive_sysclock, timer_intr_handler, NULL);

	return (unsigned int)args;
}

//-------------------------------------------------------------------------
static void keepalive_deinit(void)
{
	int oldstate;

	// Cancel the alarm
	if (keepalive_inited) {
		CpuSuspendIntr(&oldstate);
		CancelAlarm(timer_intr_handler, NULL);
		CpuResumeIntr(oldstate);
		keepalive_inited = 0;
	}
}

//-------------------------------------------------------------------------
static void keepalive_init(void)
{
	// set up the alarm
	if ((!keepalive_inited) && (!keepalive_locked)) {
		keepalive_deinit();
		SetAlarm(&keepalive_sysclock, timer_intr_handler, NULL);
		keepalive_inited = 1;
	}
}

//-------------------------------------------------------------------------
static void keepalive_lock(void)
{
	keepalive_locked = 1;
}

//-------------------------------------------------------------------------
static void keepalive_unlock(void)
{
	keepalive_locked = 0;
}

//-------------------------------------------------------------------------
static void smb_io_lock(void)
{
	keepalive_deinit();

	WaitSema(smbman_io_sema);
}

//-------------------------------------------------------------------------
static void smb_io_unlock(void)
{
	SignalSema(smbman_io_sema);
	
	keepalive_init();
}

//-------------------------------------------------------------------------
static void keepalive_thread(void *args)
{
	register int r;

	while (1) {
		// wait for keepalive mutex
		WaitSema(keepalive_mutex);

		// ensure no IO is already processing
		WaitSema(smbman_io_sema);

		// echo the SMB server
		r = smb_Echo("PS2 KEEPALIVE ECHO", 18);
		if (r < 0)
			keepalive_lock();

		SignalSema(smbman_io_sema);
	}
}

//-------------------------------------------------------------- 
int smb_dummy(void)
{
	return -EPERM;
}

//-------------------------------------------------------------- 
int smb_init(iop_device_t *dev)
{
	iop_sema_t smp;
	iop_thread_t thread;

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 1;
	smp.option = 0;

	smbman_io_sema = CreateSema(&smp);

	// create a mutex for keep alive
	keepalive_mutex = CreateMutex(IOP_MUTEX_LOCKED);

	// set the keepalive timer (3 seconds)
	USec2SysClock(3000*1000, &keepalive_sysclock);

	// starting the keepalive thead
	thread.attr = TH_C;
	thread.option = 0;
	thread.thread = (void *)keepalive_thread;
	thread.stacksize = 0x2000;
	thread.priority = 0x64;

	keepalive_tid = CreateThread(&thread);
	StartThread(keepalive_tid, NULL);

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
	keepalive_deinit();
	DeleteThread(keepalive_tid);

	DeleteSema(smbman_io_sema);

	DeleteSema(keepalive_mutex);
	keepalive_mutex = -1;

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
	s64 filesize;

	if (!filename)
		return -ENOENT;

	smb_io_lock();

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

	smb_io_unlock();

	return r;
}

//-------------------------------------------------------------- 
int smb_close(iop_file_t *f)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int r = 0;

	smb_io_lock();

	if (fh) {
		r = smb_Close(fh->smb_fid);
		if (r != 0) {
			r = -EIO;
			goto io_unlock;
		}
		memset(fh, 0, sizeof(FHANDLE));
		fh->smb_fid = -1;
		r = 0;
	}

io_unlock:
	smb_io_unlock();

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
	return (int)smb_lseek64(f, pos, where);
}

//-------------------------------------------------------------- 
int smb_read(iop_file_t *f, void *buf, int size)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int r, rpos;
	register u32 nbytes;

	if ((fh->position + size) > fh->filesize)
		size = fh->filesize - fh->position;

	smb_io_lock();

	rpos = 0;

	while (size) {
		nbytes = MAX_RD_BUF;
		if (size < nbytes)
			nbytes = size;

		r = smb_ReadAndX(fh->smb_fid, fh->position, (void *)(buf + rpos), (u16)nbytes);
		if (r < 0) {
   			rpos = -EIO;
   			goto io_unlock;
		}

		rpos += nbytes;
		size -= nbytes;
		fh->position += nbytes;
	}

io_unlock:
	smb_io_unlock();

	return rpos;
}

//-------------------------------------------------------------- 
int smb_write(iop_file_t *f, void *buf, int size)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int r, wpos;
	register u32 nbytes;

	if ((!(fh->mode & O_RDWR)) && (!(fh->mode & O_WRONLY)))
		return -EPERM;

	smb_io_lock();

	wpos = 0;

	while (size) {
		nbytes = MAX_WR_BUF;
		if (size < nbytes)
			nbytes = size;

		r = smb_WriteAndX(fh->smb_fid, fh->position, (void *)(buf + wpos), (u16)nbytes);
		if (r < 0) {
   			wpos = -EIO;
   			goto io_unlock;
		}

		wpos += nbytes;
		size -= nbytes;
		fh->position += nbytes;
		if (fh->position > fh->filesize)
			fh->filesize += fh->position - fh->filesize;
	}

io_unlock:
	smb_io_unlock();

	return wpos;
}

//-------------------------------------------------------------- 
int smb_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat)
{
	register int r;
	PathInformation_t info;

	smb_io_lock();

	memset((void *)stat, 0, sizeof(iox_stat_t));

	r = smb_QueryPathInformation((PathInformation_t *)&info, (char *)filename);
	if (r < 0) {
		r = -EIO;
		goto io_unlock;
	}

	memcpy(stat->ctime, &info.Created, 8);
	memcpy(stat->atime, &info.LastAccess, 8);
	memcpy(stat->mtime, &info.Change, 8);

	stat->size = (int)(info.EndOfFile & 0xffffffff);
	stat->hisize = (int)((info.EndOfFile >> 32) & 0xffffffff);

	if (info.FileAttributes & EXT_ATTR_DIRECTORY)
		stat->mode |= FIO_S_IFDIR;
	else 
		stat->mode |= FIO_S_IFREG;

io_unlock:
	smb_io_unlock();

	return r;
}

//-------------------------------------------------------------- 
s64 smb_lseek64(iop_file_t *f, s64 pos, int where)
{
	s64 r;
	FHANDLE *fh = (FHANDLE *)f->privdata;

	smb_io_lock();

	switch (where) {
		case SEEK_CUR:
			r = fh->position + pos;
			if (r > fh->filesize) {
				r = -EINVAL;
				goto io_unlock;
			}
			break;
		case SEEK_SET:
			r = pos;
			if (fh->filesize < pos) {
				r = -EINVAL;
				goto io_unlock;
			}
			break;
		case SEEK_END:
			r = fh->filesize;
			break;
		default:
			r = -EINVAL;
			goto io_unlock;
	}

	fh->position = r;

io_unlock:
	smb_io_unlock();

	return r;
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
static int smb_QueryDiskInfo(smbQueryDiskInfo_out_t *querydiskinfo)
{
	return smb_QueryInformationDisk(querydiskinfo);
}

//-------------------------------------------------------------- 
int smb_devctl(iop_file_t *f, const char *devname, int cmd, void *arg, u32 arglen, void *bufp, u32 buflen)
{
	register int r = 0;

	smb_io_lock();

	switch(cmd) {

		case SMB_DEVCTL_GETPASSWORDHASHES:
			smb_GetPasswordHashes((smbGetPasswordHashes_in_t *)arg, (smbGetPasswordHashes_out_t *)bufp);
			r = 0;
			break;

		case SMB_DEVCTL_LOGON:
			r = smb_LogOn((smbLogOn_in_t *)arg);
			if (r < 0)
				r = -EIO;
			else
				keepalive_unlock();
			break;

		case SMB_DEVCTL_LOGOFF:
			r = smb_LogOff();
			if (r < 0) {
				if (r == -3)
					r = -EINVAL;
				else
					r = -EIO;
			
			}
			else
				keepalive_lock();
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

		case SMB_DEVCTL_QUERYDISKINFO:
			r = smb_QueryDiskInfo((smbQueryDiskInfo_out_t *)bufp);
			if (r < 0) {
				if (r == -3)
					r = -EINVAL;
				else
					r = -EIO;
			}
			break;

		default:
			r = -EINVAL;
	}

	smb_io_unlock();

	return r;
}

