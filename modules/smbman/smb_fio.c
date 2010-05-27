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
	(void*)smb_remove,
	(void*)smb_mkdir,
	(void*)smb_rmdir,
	(void*)smb_dopen,
	(void*)smb_dclose,
	(void*)smb_dread,
	(void*)smb_getstat,
	(void*)smb_dummy,
	(void*)smb_rename,
	(void*)smb_chdir,
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
	char		name[256];
} FHANDLE;

#define MAX_FDHANDLES		32
FHANDLE smbman_fdhandles[MAX_FDHANDLES] __attribute__((aligned(64)));

static ShareEntry_t ShareList __attribute__((aligned(64))); // Keep this aligned for DMA!
static u8 SearchBuf[4096] __attribute__((aligned(64)));
static char smb_curdir[4096] __attribute__((aligned(64)));
static char smb_curpath[4096] __attribute__((aligned(64)));

static int keepalive_mutex = -1;
static int keepalive_inited = 0;
static int keepalive_locked = 1;
static int keepalive_tid;
static iop_sys_clock_t keepalive_sysclock;

static int UID = -1;
static int TID = -1;

static smbLogOn_in_t glogon_info;
static smbOpenShare_in_t gopenshare_info;

static int smb_LogOn(smbLogOn_in_t *logon);
static int smb_OpenShare(smbOpenShare_in_t *openshare);

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
	register int r, opened_share = 0;

	while (1) {
		// wait for keepalive mutex
		WaitSema(keepalive_mutex);

		// ensure no IO is already processing
		WaitSema(smbman_io_sema);

		// echo the SMB server
		r = smb_Echo("PS2 KEEPALIVE ECHO", 18);
		if (r < 0) {
			keepalive_lock();

			if (TID != -1)
				opened_share = 1;

			if (UID != -1) {
				r = smb_LogOn((smbLogOn_in_t *)&glogon_info);
				if (r == 0) {
					if (opened_share)
						smb_OpenShare((smbOpenShare_in_t *)&gopenshare_info);
				}
			}

			keepalive_unlock();
		}

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
	iop_thread_t thread;

	// create a mutex for IO ops
	smbman_io_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

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
static FHANDLE *smbman_getfilefreeslot(void)
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
static char *prepare_path(char *path)
{
	register int i;

	char *p = (char *)path;
	char *p2 = (char *)&path[strlen(path)];

	while ((*p == '\\') || (*p == '/'))
		p++;

	while ((*p2 == '\\') || (*p2 == '/'))
		*p2-- = 0;

	for (i=0; i<strlen(p); i++) {
		if (p[i] == '/')
			p[i] = '\\';
	}

	if (strlen(p) > 0) {
		strncpy(smb_curpath, smb_curdir, sizeof(smb_curpath)-1-1-strlen(p));
		strcat(smb_curpath, "\\");
		strcat(smb_curpath, p);
	}
	else {
		strncpy(smb_curpath, smb_curdir, sizeof(smb_curpath)-1-1);
		strcat(smb_curpath, "\\");
	}

	return (char *)smb_curpath;
}

//-------------------------------------------------------------- 
int smb_open(iop_file_t *f, const char *filename, int mode, int flags)
{
	register int r = 0;
	FHANDLE *fh;
	s64 filesize;

	if (!filename)
		return -ENOENT;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	char *path = prepare_path((char *)filename);

	smb_io_lock();

	fh = smbman_getfilefreeslot();
	if (fh) {
		r = smb_OpenAndX(UID, TID, path, &filesize, mode);
		if (r < 0) {
			if (r == -1)
				r = -EIO;
			else if (r == -2)
				r = -EPERM;
			else if (r == -3)
				r = -ENOENT;
			else
				r = -EIO;
		}
		else {
			f->privdata = fh;
			fh->f = f;
			fh->smb_fid = r;
			fh->mode = mode;
			fh->filesize = filesize;
			fh->position = 0;
			if (fh->mode & O_TRUNC)
				fh->filesize = 0;
			else if (fh->mode & O_APPEND)
				fh->position = filesize;
			strncpy(fh->name, path, 255);
			r = 0;
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

	if ((UID == -1) || (TID == -1) || (fh->smb_fid == -1))
		return -EINVAL;

	smb_io_lock();

	if (fh) {
		if (fh->mode != O_DIROPEN) {
			r = smb_Close(UID, TID, fh->smb_fid);
			if (r != 0) {
				r = -EIO;
				goto io_unlock;
			}
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
			smb_Close(UID, TID, fh->smb_fid);
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

	if ((UID == -1) || (TID == -1) || (fh->smb_fid == -1))
		return -EINVAL;

	if ((fh->position + size) > fh->filesize)
		size = fh->filesize - fh->position;

	smb_io_lock();

	rpos = 0;

	while (size) {
		nbytes = MAX_RD_BUF;
		if (size < nbytes)
			nbytes = size;

		r = smb_ReadAndX(UID, TID, fh->smb_fid, fh->position, (void *)(buf + rpos), (u16)nbytes);
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

	if ((UID == -1) || (TID == -1) || (fh->smb_fid == -1))
		return -EINVAL;

	if ((!(fh->mode & O_RDWR)) && (!(fh->mode & O_WRONLY)))
		return -EPERM;

	smb_io_lock();

	wpos = 0;

	while (size) {
		nbytes = MAX_WR_BUF;
		if (size < nbytes)
			nbytes = size;

		r = smb_WriteAndX(UID, TID, fh->smb_fid, fh->position, (void *)(buf + wpos), (u16)nbytes);
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
int smb_remove(iop_file_t *f, const char *filename)
{
	register int r;

	if (!filename)
		return -ENOENT;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	char *path = prepare_path((char *)filename);

	smb_io_lock();

	r = smb_Delete(UID, TID, path);
	if (r < 0)
   		r = -EIO;

	smb_io_unlock();

	return r;
}

//-------------------------------------------------------------- 
int smb_mkdir(iop_file_t *f, const char *dirname)
{
	register int r;

	if (!dirname)
		return -ENOENT;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	char *path = prepare_path((char *)dirname);

	smb_io_lock();

	r = smb_ManageDirectory(UID, TID, path, SMB_COM_CREATE_DIRECTORY);
	if (r < 0)
   		r = -EIO;

	smb_io_unlock();

	return r;
}

//-------------------------------------------------------------- 
int smb_rmdir(iop_file_t *f, const char *dirname)
{
	register int r;
	PathInformation_t info;

	if (!dirname)
		return -ENOENT;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	char *path = prepare_path((char *)dirname);

	smb_io_lock();

	r = smb_QueryPathInformation(UID, TID, (PathInformation_t *)&info, path);
	if (r < 0) {
   		r = -EIO;
		goto io_unlock;
	}
	if (!info.IsDirectory) {
		r = -ENOTDIR;
		goto io_unlock;
	}

	r = smb_ManageDirectory(UID, TID, path, SMB_COM_DELETE_DIRECTORY);
	if (r < 0)
   		r = -EIO;

io_unlock:
	smb_io_unlock();

	return r;
}

//-------------------------------------------------------------- 
int smb_dopen(iop_file_t *f, const char *dirname)
{
	register int r = 0;
	PathInformation_t info;
	FHANDLE *fh;

	if (!dirname)
		return -ENOENT;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	char *path = prepare_path((char *)dirname);

	smb_io_lock();

	// test if the dir exists
	r = smb_QueryPathInformation(UID, TID, (PathInformation_t *)&info, path);
	if (r < 0) {
   		r = -EIO;
		goto io_unlock;
	}

	if (!info.IsDirectory) {
   		r = -ENOTDIR;
		goto io_unlock;
	}

	fh = smbman_getfilefreeslot();
	if (fh) {
		f->privdata = fh;
		fh->f = f;
		fh->mode = O_DIROPEN;
		fh->filesize = 0;
		fh->position = 0;

		strncpy(fh->name, path, 254);
		strcat(fh->name, "*");

		r = 0;	
	}
	else
		r = -EMFILE;

io_unlock:
	smb_io_unlock();

	return r;
}

//-------------------------------------------------------------- 
int smb_dclose(iop_file_t *f)
{
	return smb_close(f);
}

//-------------------------------------------------------------- 
int smb_dread(iop_file_t *f, iox_dirent_t *dirent)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int r;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	smb_io_lock();

	memset((void *)dirent, 0, sizeof(iox_dirent_t));

	SearchInfo_t *info = (SearchInfo_t *)SearchBuf;

	if (fh->smb_fid == -1) {
		r = smb_FindFirstNext2(UID, TID, fh->name, TRANS2_FIND_FIRST2, info);
		if (r < 0) {
			r = -EIO;
			goto io_unlock;
		}
		fh->smb_fid = info->SID;
		r = 1;
	}
	else {
		info->SID = fh->smb_fid;
		r = smb_FindFirstNext2(UID, TID, NULL, TRANS2_FIND_NEXT2, info);
		if (r < 0) {
			r = -EIO;
			goto io_unlock;
		}
		r = 1;
	}

	if ((r == 1) && (info->EOS == 0)) {
		memcpy(dirent->stat.ctime, &info->fileInfo.Created, 8);
		memcpy(dirent->stat.atime, &info->fileInfo.LastAccess, 8);
		memcpy(dirent->stat.mtime, &info->fileInfo.Change, 8);

		dirent->stat.size = (int)(info->fileInfo.EndOfFile & 0xffffffff);
		dirent->stat.hisize = (int)((info->fileInfo.EndOfFile >> 32) & 0xffffffff);

		if (info->fileInfo.FileAttributes & EXT_ATTR_DIRECTORY)
			dirent->stat.mode |= FIO_S_IFDIR;
		else 
			dirent->stat.mode |= FIO_S_IFREG;

		strncpy(dirent->name, info->FileName, 255);
	}
	else
		r = 0;

io_unlock:
	smb_io_unlock();

	return r;
}

//-------------------------------------------------------------- 
int smb_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat)
{
	register int r;
	PathInformation_t info;

	if (!filename)
		return -ENOENT;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	char *path = prepare_path((char *)filename);

	smb_io_lock();

	memset((void *)stat, 0, sizeof(iox_stat_t));

	r = smb_QueryPathInformation(UID, TID, (PathInformation_t *)&info, path);
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
int smb_rename(iop_file_t *f, const char *oldname, const char *newname)
{
	register int r;

	if ((!oldname) || (!newname))
		return -ENOENT;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	char *oldpath = prepare_path((char *)oldname);
	char *newpath = prepare_path((char *)newname);

	smb_io_lock();

	r = smb_Rename(UID, TID, oldpath, newpath);
	if (r < 0)
   		r = -EIO;

	smb_io_unlock();

	return r;
}

//-------------------------------------------------------------- 
int smb_chdir(iop_file_t *f, const char *dirname)
{
	register int r = 0, i;
	PathInformation_t info;

	if (!dirname)
		return -ENOENT;

	if ((UID == -1) || (TID == -1))
		return -EINVAL;

	char *path = prepare_path((char *)dirname);

	smb_io_lock();

	if ((path[strlen(path)-2] == '.') && (path[strlen(path)-1] == '.')) {
		char *p = (char *)smb_curdir;
		for (i=strlen(p)-1; i>=0; i--) {
			if (p[i] == '\\') {
				p[i] = 0;
				break;
			}
		}
	}
	else if (path[strlen(path)-1] == '.') {
		smb_curdir[0] = 0;
	}
	else {
		r = smb_QueryPathInformation(UID, TID, (PathInformation_t *)&info, path);
		if (r < 0) {
   			r = -EIO;
			goto io_unlock;
		}

		if (!info.IsDirectory) {
	   		r = -ENOTDIR;
			goto io_unlock;
		}

		strncpy(smb_curdir, path, sizeof(smb_curdir)-1);
	}

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

	if (UID != -1) {
		smb_LogOffAndX(UID);
		UID = -1;
	}

	r = smb_Connect(logon->serverIP, logon->serverPort);
	if (r < 0)
		return r;

	r = smb_NegociateProtocol();
	if (r < 0)
		return r;

	r = smb_SessionSetupAndX(logon->User, logon->Password, logon->PasswordType);
	if (r < 0)
		return r;

	UID = r;

	memcpy((void *)&glogon_info, (void *)logon, sizeof(smbLogOn_in_t));

	keepalive_unlock();

	return 0;
}

//-------------------------------------------------------------- 
static int smb_LogOff(void)
{
	register int r;

	if (UID == -1)
		return -3;

	if (TID != -1) {
		smb_closeAll();
		smb_TreeDisconnect(UID, TID);
		TID = -1;
	}

	r = smb_LogOffAndX(UID);
	if (r < 0)
		return r;

	UID = -1;

	keepalive_lock();

	smb_Disconnect();

	return 0;
}

//-------------------------------------------------------------- 
static int smb_GetShareList(smbGetShareList_in_t *getsharelist)
{
	register int i, r, sharecount, shareindex;
	char tree_str[256];
	server_specs_t *specs;

	specs = (server_specs_t *)getServerSpecs();

	if (TID != -1) {
		smb_TreeDisconnect(UID, TID);
		TID = -1;
	}

	// Tree Connect on IPC slot
	sprintf(tree_str, "\\\\%s\\IPC$", specs->ServerIP);
	r = smb_TreeConnectAndX(UID, tree_str, NULL, 0);
	if (r < 0)
		return r;

	TID = r;

	if (UID == -1)
		return -3;

	// does a 1st enum to count shares (+IPC)
	r = smb_NetShareEnum(UID, TID, (ShareEntry_t *)&ShareList, 0, 0);
	if (r < 0)
		return r;

	sharecount = r;
	shareindex = 0;

	// now we list the following shares if any 
	for (i=0; i<sharecount; i++) {

		r = smb_NetShareEnum(UID, TID, (ShareEntry_t *)&ShareList, i, 1);
		if (r < 0)
			return r;

		// if the entry is not IPC, we send it on EE, and increment shareindex
		if ((strcmp(ShareList.ShareName, "IPC$")) && (shareindex < getsharelist->maxent)) {
			DMA_sendEE((void *)&ShareList, sizeof(ShareList), (void *)(getsharelist->EE_addr + (shareindex * sizeof(ShareEntry_t))));
			shareindex++;
		}
	}

	// disconnect the tree
	r = smb_TreeDisconnect(UID, TID);
	if (r < 0)
		return r;

	TID = -1;

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

	if (TID != -1) {
		smb_TreeDisconnect(UID, TID);
		TID = -1;
	}

	sprintf(tree_str, "\\\\%s\\%s", specs->ServerIP, openshare->ShareName);
	r = smb_TreeConnectAndX(UID, tree_str, openshare->Password, openshare->PasswordType);
	if (r < 0)
		return r;

	TID = r;

	memcpy((void *)&gopenshare_info, (void *)openshare, sizeof(smbOpenShare_in_t));

	return 0;
}

//-------------------------------------------------------------- 
static int smb_CloseShare(void)
{
	register int r;

	if (TID == -1)
		return -3;

	smb_closeAll();

	r = smb_TreeDisconnect(UID, TID);
	if (r < 0)
		return r;

	TID = -1;

	return 0;
}

//-------------------------------------------------------------- 
static int smb_EchoServer(smbEcho_in_t *echo)
{
	return smb_Echo(echo->echo, echo->len);
}

//-------------------------------------------------------------- 
static int smb_QueryDiskInfo(smbQueryDiskInfo_out_t *querydiskinfo)
{
	if ((UID == -1) || (TID == -1))
		return -3;

	return smb_QueryInformationDisk(UID, TID, querydiskinfo);
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

