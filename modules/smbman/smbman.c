/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <intrman.h>
#include <sifman.h>
#include <sifrpc.h>
#include <sysmem.h>
#include <thsemap.h>
#include <errno.h>

#include "smb.h"

#define MODNAME "smbman"
IRX_ID(MODNAME, 1, 1);

#define MAX_SECTORS 	7

static char g_pc_ip[]="xxx.xxx.xxx.xxx";
static int g_pc_port = 445; // &g_pc_ip + 16
static char g_pc_share[33]="PS2SMB";

int smbman_initdev(void);

struct irx_export_table _exp_smbman;

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{
	char tree_str[255];

	RegisterLibraryEntries(&_exp_smbman);
	
    // Open the Connection with SMB server
   	smb_NegociateProtocol(g_pc_ip, g_pc_port, "NT LM 0.12"); 

    // zero pad the string to be sure it does not overflow
    g_pc_share[32] = '\0';

    // Then open a session and a tree connect on the share resource
    sprintf(tree_str, "\\\\%s\\%s", g_pc_ip, g_pc_share);
    smb_SessionSetupTreeConnect("GUEST", tree_str);

    smbman_initdev();

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
	smb_Disconnect();

	return 0;
}

//-------------------------------------------------------------- 
// FS PART
//-------------------------------------------------------------- 

#include <ioman.h>
#include <io_common.h>

// smb driver ops functions prototypes
int smb_dummy(void);
int smb_init(iop_device_t *iop_dev);
int smb_deinit(iop_device_t *dev);
int smb_open(iop_file_t *f, char *filename, int mode, int flags);
int smb_close(iop_file_t *f);
int smb_lseek(iop_file_t *f, u32 pos, int where);
int smb_read(iop_file_t *f, void *buf, u32 size);

int smbman_io_sema;

// driver ops func tab
void *smbman_ops[17] = {
	(void*)smb_init,
	(void*)smb_deinit,
	(void*)smb_dummy,
	(void*)smb_open,
	(void*)smb_close,
	(void*)smb_read,
	(void*)smb_dummy,
	(void*)smb_lseek,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy,
	(void*)smb_dummy
};

// driver descriptor
static iop_device_t smbdev = {
	"smb", 
	IOP_DT_FS,
	1,
	"SMB ",
	(struct _iop_device_ops *)&smbman_ops
};

typedef struct {
	iop_file_t *f;
	u32 smb_fid;
	u32 filesize;
	u32 position;
} FHANDLE;

#define MAX_FDHANDLES 		64
FHANDLE smbman_fdhandles[MAX_FDHANDLES] __attribute__((aligned(64)));

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
int smbman_initdev(void)
{	
	DelDrv(smbdev.name);
	if (AddDrv(&smbdev))
		return 1;

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
	u16 smb_fid;
	u32 filesize;

	if (!filename)
		return -ENOENT;
			 
	WaitSema(smbman_io_sema);
		
	fh = smbman_getfilefreeslot();
	if (fh) {
		r = smb_NTCreateAndX(filename, &smb_fid, &filesize);
		if (r == 1) {
			f->privdata = fh;
			fh->f = f;
			fh->smb_fid = smb_fid;
			fh->filesize = filesize;
			fh->position = 0;
			r = 0;
		}
		else
			r = -ENOENT;
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
		if (r != 1) {
			r = -EIO;
			goto ssema;
		}
		memset(fh, 0, sizeof(FHANDLE));
		r = 0;
	}

ssema:	
	SignalSema(smbman_io_sema);

	return r;
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

	rpos = 0;	
	while (size) {
		nbytes = MAX_SMB_BUF;
		if (size < nbytes)
			nbytes = size;

		r = smb_ReadAndX(fh->smb_fid, fh->position, (void *)(buf + rpos), (u16)nbytes);
		if (r < 0) {
   			rpos = -EIO;
   			goto ssema;
		}

		rpos += nbytes;
		buf += nbytes;
		size -= nbytes;
		fh->position += nbytes;		
	}

ssema:
	SignalSema(smbman_io_sema);

	return rpos;	
}
