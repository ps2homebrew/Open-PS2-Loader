/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS I/O manager related routines
*/

#include <stdio.h>
#include <sysclib.h>
#include <errno.h>
#include <iomanX.h>
#include <thsemap.h>
#include <hdd-ioctl.h>

#include "pfs-opt.h"
#include "libpfs.h"
#include "pfs.h"
#include "pfs_fio.h"
#include "pfs_fioctl.h"

///////////////////////////////////////////////////////////////////////////////
//	Globals

static const u8 openFlagArray[] = { 0, 4, 2, 6, 0, 0, 0, 0 };
int pfsFioSema = 0;
pfs_file_slot_t *pfsFileSlots;

extern pfs_config_t pfsConfig;
extern pfs_cache_t *pfsCacheBuf;
extern u32 pfsCacheNumBuffers;
extern pfs_mount_t *pfsMountBuf;

///////////////////////////////////////////////////////////////////////////////
//	Function declarations

static int openFile(pfs_mount_t *pfsMount, pfs_file_slot_t *freeSlot, const char *filename, int openFlags, int mode);
static int fileTransferRemainder(pfs_file_slot_t *fileSlot, void *buf, int size, int operation);
static int fileTransfer(pfs_file_slot_t *fileSlot, u8 *buf, int size, int operation);
static void fioStatFiller(pfs_cache_t *clink, iox_stat_t *stat);
static s64 _seek(pfs_file_slot_t *fileSlot, s64 offset, int whence, int mode);
static int _remove(pfs_mount_t *pfsMount, const char *path, int mode);
static int mountDevice(pfs_block_device_t *blockDev, int fd, int unit, int flag);
static void _sync(void);

///////////////////////////////////////////////////////////////////////////////
//	Function definitions

int pfsFioCheckForLastError(pfs_mount_t *pfsMount, int rv)
{
	return pfsMount->lastError ? pfsMount->lastError : rv;
}

int pfsFioCheckFileSlot(pfs_file_slot_t *fileSlot)
{
	WaitSema(pfsFioSema);

	if(fileSlot->clink==NULL)
	{
		SignalSema(pfsFioSema);
		return -EBADF;
	}

	return 0;
}

void pfsFioCloseFileSlot(pfs_file_slot_t *fileSlot)
{
	pfs_mount_t *pfsMount=fileSlot->clink->pfsMount;

	if(fileSlot->fd->mode & O_WRONLY)
	{
		if(fileSlot->restsInfo.dirty!=0)
		{
			pfsMount->blockDev->transfer(pfsMount->fd, fileSlot->restsBuffer,
				fileSlot->restsInfo.sub, fileSlot->restsInfo.sector, 1, PFS_IO_MODE_WRITE);
		}
		pfsInodeSetTime(fileSlot->clink);	// set time :P
		fileSlot->clink->u.inode->attr|=PFS_FIO_ATTR_CLOSED;
		if(pfsMount->flags & PFS_FIO_ATTR_WRITEABLE)
			pfsCacheFlushAllDirty(pfsMount);
	}
	pfsCacheFree(fileSlot->block_pos.inode);
	pfsCacheFree(fileSlot->clink);
	memset(fileSlot, 0, sizeof(pfs_file_slot_t));
}

pfs_mount_t *pfsFioGetMountedUnit(int unit)
{
	pfs_mount_t *pfsMount;

	WaitSema(pfsFioSema);
	if((pfsMount=pfsGetMountedUnit(unit))==NULL)
		SignalSema(pfsFioSema);
	return pfsMount;
}

static int mountDevice(pfs_block_device_t *blockDev, int fd, int unit, int flag)
{
	u32 i;
	int rv;

	if((u32)unit >= pfsConfig.maxMount)
		return -EMFILE;

	if(pfsMountBuf[unit].flags & PFS_MOUNT_BUSY)
		return -EBUSY;

	for(i = 0; i < pfsConfig.maxMount; i++)
		if((pfsMountBuf[i].flags & PFS_MOUNT_BUSY) &&
			(blockDev == pfsMountBuf[i].blockDev) &&
			(fd == pfsMountBuf[i].fd)) // Cant mount the same partition more than once
			return -EBUSY;

	pfsMountBuf[unit].blockDev = blockDev;
	pfsMountBuf[unit].fd = fd;
	pfsMountBuf[unit].flags = flag;

	rv = pfsMountSuperBlock(&pfsMountBuf[unit]);
	if(rv < 0)
		return rv;

	pfsMountBuf[unit].flags |= PFS_MOUNT_BUSY;

	return 0;
}

static int openFile(pfs_mount_t *pfsMount, pfs_file_slot_t *freeSlot, const char *filename, int openFlags, int mode)
{
	char file[256];
	int  result, result2, result3, result4;
	pfs_cache_t *parentInode, *fileInode, *cached;


	result = 0;	//no error
	// Get the inode for the directory which contains the file (dir) in filename
	// After this call, 'file' will contain the name of the file we're operating on.
	if ((parentInode=pfsInodeGetParent(pfsMount, NULL, filename, file, &result))==0)
	{
		return result;
	}

	// Get the inode for the actual file/directory contained in the parent dir
	fileInode=pfsInodeGetFileInDir(parentInode, file, &result);

	// If file already exists..
	if (fileInode)
	{
		u32 flags;
		u32 count;

		// Setup flags
		flags=openFlagArray[openFlags & O_RDWR];
		if (openFlags & O_TRUNC)	flags |= 2;
		if (openFlags & PFS_FDIRO)		flags |= 4;
		if ((mode & 0x10000) ||
		   ((openFlags & (O_CREAT|O_EXCL)) == (O_CREAT|O_EXCL))){
			result=-EEXIST;
		}
		else
		{
			count = 0;

			// Resolve actual file from a symlink
			while ((fileInode->u.inode->mode & FIO_S_IFMT) == FIO_S_IFLNK)
			{
				if (count++>=4)
				{
					result=-ELOOP;
					goto label;
				}

				parentInode=pfsInodeGetParent(pfsMount, parentInode, (char*)&fileInode->u.inode->data[1],
												file, &result);
				pfsCacheFree(fileInode);
				if ((parentInode==0) ||
				    ((fileInode=pfsInodeGetFileInDir(parentInode, file, &result))==0))
					goto label;
			}

			// Make sure that if a file is being opened, then inode does not point
			// to a directory, and vice versa.
			if ((openFlags & PFS_FDIRO) == 0)
			{
				if ((fileInode->u.inode->mode & FIO_S_IFMT) == FIO_S_IFDIR)
					result=-EISDIR;
			}else{
				if ((fileInode->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
					result=-ENOTDIR;
			}

			// Truncate file if required
			if ((result==0) &&
			    ((result=pfsCheckAccess(fileInode, flags & 0xFFFF))==0) &&
			    (openFlags & O_TRUNC))
			{
				cached=pfsCacheGetData(fileInode->pfsMount, fileInode->sub, fileInode->sector+1, PFS_CACHE_FLAG_NOLOAD | PFS_CACHE_FLAG_NOTHING, &result2);
				if (cached)
				{
					memset(cached->u.aentry, 0, sizeof(pfs_inode_t)); //1024
					cached->u.aentry->aLen=sizeof(pfs_inode_t);
					cached->flags |= PFS_CACHE_FLAG_DIRTY;
					pfsCacheFree(cached);
				}
				if (result2 == 0)
				{
					fileInode->u.inode->size = 0;
					fileInode->u.inode->attr &= ~PFS_FIO_ATTR_CLOSED; //~0x80==0xFF7F
					fileInode->flags |= PFS_CACHE_FLAG_DIRTY;
					pfsFreeZones(fileInode);
				}
			}
		}

	// Otherwise, if file doesnt already exist..
	}else{
		if ((openFlags & O_CREAT) && (result==-ENOENT) &&
		    ((result=pfsCheckAccess(parentInode, 2))==0) &&
		    (fileInode=pfsInodeCreate(parentInode, mode, pfsMount->uid,
						  pfsMount->gid, &result)))
		{
			if ((mode & FIO_S_IFMT) == FIO_S_IFLNK)
			{
				strcpy((char*)&fileInode->u.inode->data[1], (char*)freeSlot);
				freeSlot=NULL;
			}

			// If new file is a directory, the fill self and parent entries
			if ((mode & FIO_S_IFMT) == FIO_S_IFDIR)
			{
				cached=pfsCacheGetData(fileInode->pfsMount, fileInode->u.inode->data[1].subpart,
					fileInode->u.inode->data[1].number << fileInode->pfsMount->inode_scale,
					PFS_CACHE_FLAG_NOLOAD | PFS_CACHE_FLAG_NOTHING, &result3);
				if (cached)
				{
					pfsFillSelfAndParentDentries(cached,
						&fileInode->u.inode->inode_block,
						&parentInode->u.inode->inode_block);
					cached->flags |= PFS_CACHE_FLAG_DIRTY;
					pfsCacheFree(cached);
				}
				result=result3;
			// Otherwise if its just a regular file, just zero its attribute entry
			}else{
				cached=pfsCacheGetData(fileInode->pfsMount, fileInode->sub, fileInode->sector+1,
						PFS_CACHE_FLAG_NOLOAD | PFS_CACHE_FLAG_NOTHING, &result4);
				if (cached)
				{
					memset(cached->u.aentry, 0, sizeof(pfs_inode_t));
					cached->u.aentry->aLen=sizeof(pfs_inode_t);
					cached->flags |= PFS_CACHE_FLAG_DIRTY;
					pfsCacheFree(cached);
				}
				result=result4;
			}

			// Link new file with parent directory
			if ((result==0) && (cached=pfsDirAddEntry(parentInode, file, &fileInode->u.inode->inode_block,
							 mode, &result)))
			{
				pfsInodeSetTime(parentInode);
				cached->flags|=PFS_CACHE_FLAG_DIRTY;
				pfsCacheFree(cached);
			}
		}
	}
label:
	pfsCacheFree(parentInode);
	if ((result==0) && freeSlot && fileInode)
	{
		freeSlot->clink=fileInode;
		if (openFlags & O_APPEND)
			freeSlot->position = fileInode->u.inode->size;
		else
			freeSlot->position = 0;

		result=pfsBlockInitPos(freeSlot->clink, &freeSlot->block_pos, freeSlot->position);
		if (result==0)
		{
			if ((openFlags & O_WRONLY) &&
			    (fileInode->u.inode->attr & PFS_FIO_ATTR_CLOSED)){
				fileInode->u.inode->attr &= ~PFS_FIO_ATTR_CLOSED;
				fileInode->flags |= PFS_CACHE_FLAG_DIRTY;
				if (pfsMount->flags & PFS_FIO_ATTR_WRITEABLE)
					pfsCacheFlushAllDirty(pfsMount);
			}
			if ((result=pfsFioCheckForLastError(pfsMount, result))==0)
				return 0;
		}
		freeSlot->clink=NULL;
	}

	if(fileInode) pfsCacheFree(fileInode);

	return pfsFioCheckForLastError(pfsMount, result);
}

// Reads unaligned data, or remainder data (size < 512)
static int fileTransferRemainder(pfs_file_slot_t *fileSlot, void *buf, int size, int operation)
{
	u32 sector, pos;
	int result;
	pfs_blockpos_t *blockpos = &fileSlot->block_pos;
	pfs_mount_t *pfsMount = fileSlot->clink->pfsMount;
	pfs_restsInfo_t *info = &fileSlot->restsInfo;
	pfs_blockinfo_t *bi = pfsBlockGetCurrent(blockpos);

	sector = ((bi->number+blockpos->block_offset) << pfsMount->sector_scale) +
	   (blockpos->byte_offset >> 9);
	pos = blockpos->byte_offset & 0x1FF;

	if((info->sector != sector) || (info->sub!=bi->subpart))
	{
		if (fileSlot->restsInfo.dirty)
		{
			result=pfsMount->blockDev->transfer(pfsMount->fd, fileSlot->restsBuffer, info->sub, info->sector, 1, 1);
			if(result)
				return result;
			fileSlot->restsInfo.dirty=0;
		}

		info->sub=bi->subpart;
		info->sector=sector;

		if(pos || (fileSlot->position != fileSlot->clink->u.inode->size))
		{
			result = pfsMount->blockDev->transfer(pfsMount->fd, fileSlot->restsBuffer, info->sub, info->sector, 1, 0);
			if(result)
				return result | 0x10000;
		}
	}

	if (operation==0)
		memcpy(buf, fileSlot->restsBuffer+pos, size=min(size, 512-(int)pos));
	else
		memcpy(fileSlot->restsBuffer+pos, buf, size=min(size, 512-(int)pos));

	if (operation == 1)
	{
		if (pfsMount->flags & PFS_FIO_ATTR_WRITEABLE)
		{
			if ((result=pfsMount->blockDev->transfer(pfsMount->fd, fileSlot->restsBuffer, info->sub,
				 info->sector, operation, operation)))
				return result;
		}
		else
			info->dirty=operation;
	}
	return size;
}

// Does actual read/write of data from file
static int fileTransfer(pfs_file_slot_t *fileSlot, u8 *buf, int size, int operation)
{
	int result=0;
	pfs_blockpos_t *blockpos=&fileSlot->block_pos;
	pfs_mount_t *pfsMount=fileSlot->clink->pfsMount;
	u32 bytes_remain;
	u32 total = 0;

	// If we're writing and there is less free space in the last allocated block segment
	// than can hold the data being written, then try and expand the block segment
	if ((operation==1) &&
	    (fileSlot->clink->u.inode->number_data - 1 == blockpos->block_segment))
	{
		bytes_remain = (pfsBlockGetCurrent(blockpos)->count - blockpos->block_offset) * pfsMount->zsize - blockpos->byte_offset;//u32

		if (bytes_remain < size)
		{
			pfsBlockExpandSegment(fileSlot->clink, blockpos,
				((size-bytes_remain+pfsMount->zsize-1) & (~(pfsMount->zsize-1))) / pfsMount->zsize);
		}
	}

	while (size)
	{
		pfs_blockinfo_t *bi;
		u32 sectors;

		// Get block info for current file position
		bi=pfsBlockGetCurrent(blockpos);

		// Get amount of remaining data in current block
		bytes_remain=(bi->count - blockpos->block_offset) * pfsMount->zsize - blockpos->byte_offset;

		// If there is no space/data left in the current block segment, then we need to move onto the next
		if (bytes_remain==0)
		{
			// If we're at the end of allocated block segments, then allocate a new block segment
			if (blockpos->block_segment == fileSlot->clink->u.inode->number_data-1){
				if (operation==0){
					PFS_PRINTF(PFS_DRV_NAME" Panic: This is a bug!\n");
					return 0;
				}

				result = pfsBlockAllocNewSegment(fileSlot->clink, blockpos,
								(size - 1 + pfsMount->zsize) / pfsMount->zsize);
				if(result<0)
					break;

			// Otherwise, move to the next block segment
			}else
				if ((result=pfsBlockSeekNextSegment(fileSlot->clink, blockpos)))
				{
					return result;
				}

			bi=pfsBlockGetCurrent(blockpos);
			bytes_remain=(bi->count - blockpos->block_offset) * pfsMount->zsize - blockpos->byte_offset;
		}

		if (bytes_remain==0)
		{
			PFS_PRINTF(PFS_DRV_NAME" Panic: There is no zone.\n");
			return 0;
		}

		// If we are transferring size not a multiple of 512, under 512, or to
		// an unaligned buffer we need to use a special function rather than
		// just doing a ATA sector transfer.
		if ((blockpos->byte_offset & 0x1FF) || (size < 512) || ((u32)buf & 3))
		{
			if ((result=fileTransferRemainder(fileSlot, buf, size, operation)) < 0)
				break;
		}
		else
		{
			sectors = bytes_remain / 512;
			if ((size / 512) < sectors)	sectors = size / 512; //sectors=min(size/512, sectors)

			// Do the ATA sector transfer
			result=pfsMount->blockDev->transfer(pfsMount->fd, buf, bi->subpart,
				 ((bi->number + blockpos->block_offset) << pfsMount->sector_scale)+(blockpos->byte_offset / 512),
				 sectors, operation);
			if (result < 0)
			{
				result |= 0x10000; // TODO: EIO define
				break;
			}
			result = sectors * 512;
		}

		size -= result;
		fileSlot->position += result;
		buf += result;
		total += result;

		// If file has grown, need to mark inode as dirty
		if(fileSlot->clink->u.inode->size < fileSlot->position)
		{
			fileSlot->clink->u.inode->size = fileSlot->position;
			fileSlot->clink->flags |= PFS_CACHE_FLAG_DIRTY;
		}

		blockpos->block_offset+=pfsBlockSyncPos(blockpos, result);
	}
	return result < 0 ? result : total;
}

int	pfsFioInit(iop_device_t *f)
{
	iop_sema_t sema;

	sema.attr = 1;
	sema.option = 0;
	sema.initial = 1;
	sema.max = 1;

	pfsFioSema = CreateSema(&sema);

	return 0;
}

int	pfsFioDeinit(iop_device_t *f)
{
	pfsFioDevctlCloseAll();

	DeleteSema(pfsFioSema);

	return 0;
}

int	pfsFioFormat(iop_file_t *t, const char *dev, const char *blockdev, void *args, int arglen)
{
	int *arg = (int *)args;
	int fragment = 0;
	int fd;
	pfs_block_device_t *blockDev;
	int rv;

	// Has a fragment bit pattern been specified ?
	if((arglen == (3 * sizeof(int))) && (arg[1] == 0x2D66))	//arg[1] == "-f"
		fragment = arg[2];

	if((blockDev = pfsGetBlockDeviceTable(blockdev)) == 0)
		return -ENXIO;

	WaitSema(pfsFioSema);

	fd = open(blockdev, O_RDWR, 0644);

	if(fd < 0)
		rv = fd;
	else {
		rv = pfsFormat(blockDev, fd, arg[0], fragment);
		close(fd);
	}

	SignalSema(pfsFioSema);

	return rv;
}

int	pfsFioOpen(iop_file_t *f, const char *name, int flags, int mode)
{
	int rv = 0;
	u32 i;
	pfs_file_slot_t *freeSlot;
	pfs_mount_t *pfsMount;

	if(!name)
		return -ENOENT;

	if(!(pfsMount = pfsFioGetMountedUnit(f->unit)))
	{
		return -ENODEV;
	}

	// Find free file slot
	for(i = 0; i < pfsConfig.maxOpen; i++)
	{
		if(!pfsFileSlots[i].fd) {
			freeSlot = &pfsFileSlots[i];
			goto pfsOpen_slotFound;
		}
	}

	PFS_PRINTF(PFS_DRV_NAME" Error: There are no free file slots!\n");
	freeSlot = NULL;

pfsOpen_slotFound:

	if(!freeSlot) {
		rv = -EMFILE;
		goto pfsOpen_end;
	}

	// Do actual open
	if((rv = openFile(pfsMount, freeSlot, name, f->mode, (mode & 0xfff) | FIO_S_IFREG)))
		goto pfsOpen_end;

	freeSlot->fd = f;
	f->privdata = freeSlot;

pfsOpen_end:

	SignalSema(pfsFioSema);

	return rv;
}

int	pfsFioClose(iop_file_t *f)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t *)f->privdata;
	int rv;

	rv = pfsFioCheckFileSlot(fileSlot);
	if(rv)
		return rv;

	pfsFioCloseFileSlot(fileSlot);

	SignalSema(pfsFioSema);

	return rv;
}

int pfsFioRead(iop_file_t *f, void *buf, int size)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t*)f->privdata;
	int result = pfsFioCheckFileSlot(fileSlot);

	if (result)
	{
		return result;
	}

	// Check bounds, adjust size if necessary
	if(fileSlot->clink->u.inode->size <  (fileSlot->position + size))
		size = fileSlot->clink->u.inode->size - fileSlot->position;

	result = size ? fileTransfer(fileSlot, buf, size, 0) : 0;
	result = pfsFioCheckForLastError(fileSlot->clink->pfsMount, result);

	SignalSema(pfsFioSema);
	return result;
}

int pfsFioWrite(iop_file_t *f, void *buf, int size)
{
	pfs_mount_t *pfsMount;
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t*)f->privdata;
	int result = pfsFioCheckFileSlot(fileSlot);

	if(result)
		return result;

	pfsMount = fileSlot->clink->pfsMount;

	result = fileTransfer(fileSlot, buf, size, 1);

	if (pfsMount->flags & PFS_FIO_ATTR_WRITEABLE)
		pfsCacheFlushAllDirty(pfsMount);

	result = pfsFioCheckForLastError(pfsMount, result);
	SignalSema(pfsFioSema);
	return result;
}

static s64 _seek(pfs_file_slot_t *fileSlot, s64 offset, int whence, int mode)
{
	int rv;
	s64 startPos, newPos;

	if (mode & O_DIROPEN)
	{
		return -EISDIR;
	}

	switch (whence)
	{
	case SEEK_SET:
		startPos = 0;
		break;
	case SEEK_CUR:
		startPos= fileSlot->position;
		break;
	case SEEK_END:
		startPos = fileSlot->clink->u.inode->size;
		break;
	default:
		return -EINVAL;
	}
	newPos = startPos + offset;

	if (((offset < 0) && (startPos < newPos)) ||
		((offset > 0) && (startPos > newPos)) ||
		(fileSlot->clink->u.inode->size < newPos))
	{
		return -EINVAL;
	}

	pfsCacheFree(fileSlot->block_pos.inode);
	rv=pfsBlockInitPos(fileSlot->clink, &fileSlot->block_pos, newPos);
	if (rv==0)
		fileSlot->position = newPos;
	else {
		fileSlot->position = 0;
		pfsBlockInitPos(fileSlot->clink, &fileSlot->block_pos, 0);
	}

	if (rv)
	{
		return rv;
	}

	return newPos;
}

int pfsFioLseek(iop_file_t *f, int pos, int whence)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t*)f->privdata;
	int result = pfsFioCheckFileSlot(fileSlot);

	if (result)
		return result;

	result = (u32)_seek(fileSlot, (s64)pos, whence, f->mode);

	SignalSema(pfsFioSema);
	return result;
}

s64 pfsFioLseek64(iop_file_t *f, s64 offset, int whence)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t *)f->privdata;
	u64 rv;

	rv=pfsFioCheckFileSlot(fileSlot);
	if(!rv)
	{
		rv=_seek(fileSlot, offset, whence, f->mode);
		SignalSema(pfsFioSema);
	}
	return rv;
}

static int _remove(pfs_mount_t *pfsMount, const char *path, int mode)
{
	char szPath[256];
	int rv=0;
	pfs_cache_t *parent;
	pfs_cache_t *file;

	if((parent=pfsInodeGetParent(pfsMount, NULL, path, szPath, &rv))==NULL)
		return rv;

	if((file=pfsInodeGetFileInDir(parent, szPath, &rv))!=NULL)
	{
		if(mode!=0)
		{// remove dir
			if((file->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
				rv=-ENOTDIR;
			else if(pfsCheckDirForFiles(file)==0)
				rv=-ENOTEMPTY;
			else if(((u16)file->u.inode->inode_block.number==pfsMount->root_dir.number) &&
				((u16)file->u.inode->inode_block.subpart==pfsMount->root_dir.subpart))
				rv=-EBUSY;
			else if(((u16)file->u.inode->inode_block.number==pfsMount->current_dir.number) &&
				((u16)file->u.inode->inode_block.subpart==pfsMount->current_dir.subpart))
				rv=-EBUSY;
		}
		else// just a file
			if((file->u.inode->mode & FIO_S_IFMT)==FIO_S_IFDIR)
				rv=-EISDIR;

		if(rv==0)
		{
			if(file->u.inode->uid!=0)
				rv=pfsCheckAccess(file, 2);
			if(file->nused>=2)
				rv=-EBUSY;
			if(rv==0)
				return pfsInodeRemove(parent, file, szPath);
		}
		pfsCacheFree(file);
	}
	pfsCacheFree(parent);
	return pfsFioCheckForLastError(pfsMount, rv);
}

int	pfsFioRemove(iop_file_t *f, const char *name)
{
	pfs_mount_t *pfsMount;
	int rv;

	if(!(pfsMount = pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	rv = _remove(pfsMount, name, 0);

	SignalSema(pfsFioSema);

	return rv;
}

int	pfsFioMkdir(iop_file_t *f, const char *path, int mode)
{
	pfs_mount_t *pfsMount;
	int rv;

	mode = (mode & 0xfff) | 0x10000 | FIO_S_IFDIR;	// TODO: change to some constant/macro

	if(!(pfsMount = pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	rv = openFile(pfsMount, NULL, path, O_CREAT | O_WRONLY, mode);

	SignalSema(pfsFioSema);

	return rv;
}

int	pfsFioRmdir(iop_file_t *f, const char *path)
{
	pfs_mount_t *pfsMount;
	const char *temp;
	int rv;

	temp = path + strlen(path);
	if(*temp == '.')
		return -EINVAL;

	if(!(pfsMount = pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	rv = _remove(pfsMount, path, 0x01); // TODO: constant for 0x01 ?

	SignalSema(pfsFioSema);

	return rv;
}

int pfsFioDopen(iop_file_t *f, const char *name)
{
    return pfsFioOpen(f, name, 0, 0);
}

int	pfsFioDread(iop_file_t *f, iox_dirent_t *dirent)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t *)f->privdata;
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;
	pfs_blockinfo_t bi;
	int result;
	int rv;

	rv = pfsFioCheckFileSlot(fileSlot);
	if(rv < 0)
		return rv;

	pfsMount = fileSlot->clink->pfsMount;

	if((fileSlot->clink->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
	{
		rv = -ENOTDIR;
	}
	else
		if((rv = pfsGetNextDentry(fileSlot->clink, &fileSlot->block_pos, (u32 *)&fileSlot->position,
									dirent->name, &bi)) > 0)
		{

			clink = pfsInodeGetData(pfsMount, bi.subpart, bi.number, &result);
			if(clink != NULL)
			{
				fioStatFiller(clink, &dirent->stat);
				pfsCacheFree(clink);
			}

			if(result)
				rv = result;
		}

	rv = pfsFioCheckForLastError(pfsMount, rv);
	SignalSema(pfsFioSema);

	return rv;
}

static void fioStatFiller(pfs_cache_t *clink, iox_stat_t *stat)
{
	stat->mode = clink->u.inode->mode;
	stat->attr = clink->u.inode->attr;
	stat->size = (u32)clink->u.inode->size;
	stat->hisize = (u32)(clink->u.inode->size >> 32);
	memcpy(&stat->ctime, &clink->u.inode->ctime, sizeof(pfs_datetime_t));
	memcpy(&stat->atime, &clink->u.inode->atime, sizeof(pfs_datetime_t));
	memcpy(&stat->mtime, &clink->u.inode->mtime, sizeof(pfs_datetime_t));
	stat->private_0 = clink->u.inode->uid;
	stat->private_1 = clink->u.inode->gid;
	stat->private_2 = clink->u.inode->number_blocks;
	stat->private_3 = clink->u.inode->number_data;
	stat->private_4 = 0;
	stat->private_5 = 0;
}

int	pfsFioGetstat(iop_file_t *f, const char *name, iox_stat_t *stat)
{
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;
	int rv=0;

	if(!(pfsMount = pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	clink=pfsInodeGetFile(pfsMount, NULL, name, &rv);
	if(clink!=NULL)
	{
		fioStatFiller(clink, stat);
		pfsCacheFree(clink);
	}

	SignalSema(pfsFioSema);
	return pfsFioCheckForLastError(pfsMount, rv);
}

int	pfsFioChstat(iop_file_t *f, const char *name, iox_stat_t *stat, unsigned int statmask)
{
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;
	int rv = 0;

	if(!(pfsMount = pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	clink = pfsInodeGetFile(pfsMount, NULL, name, &rv);
	if(clink != NULL) {

		rv = pfsCheckAccess(clink, 0x02);
		if(rv == 0) {

			clink->flags |= PFS_CACHE_FLAG_DIRTY;

			if((statmask & FIO_CST_MODE) && ((clink->u.inode->mode & FIO_S_IFMT) != FIO_S_IFLNK))
				clink->u.inode->mode = (clink->u.inode->mode & FIO_S_IFMT) | (stat->mode & 0xfff);
			if(statmask & FIO_CST_ATTR)
				clink->u.inode->attr = (clink->u.inode->attr & 0xA0) | (stat->attr & 0xFF5F);
			if(statmask & FIO_CST_SIZE)
				rv = -EACCES;
			if(statmask & FIO_CST_CT)
				memcpy(&clink->u.inode->ctime, stat->ctime, sizeof(pfs_datetime_t));
			if(statmask & FIO_CST_AT)
				memcpy(&clink->u.inode->atime, stat->atime, sizeof(pfs_datetime_t));
			if(statmask & FIO_CST_MT)
				memcpy(&clink->u.inode->mtime, stat->mtime, sizeof(pfs_datetime_t));
			if(statmask & FIO_CST_PRVT) {
				clink->u.inode->uid = stat->private_0;
				clink->u.inode->gid = stat->private_1;
			}

			if(pfsMount->flags & PFS_FIO_ATTR_WRITEABLE)
				pfsCacheFlushAllDirty(pfsMount);
		}

		pfsCacheFree(clink);

	}

	SignalSema(pfsFioSema);
	return pfsFioCheckForLastError(pfsMount, rv);
}

int pfsFioRename(iop_file_t *ff, const char *old, const char *new)
{
	char path1[256], path2[256];
	int result=0;
	pfs_mount_t *pfsMount;
	int f;
	pfs_cache_t *parentOld=NULL, *parentNew=NULL;
	pfs_cache_t *removeOld=NULL, *removeNew=NULL;
	pfs_cache_t *iFileOld=NULL, *iFileNew=NULL;
	pfs_cache_t *newParent=NULL,*addNew=NULL;

	pfsMount=pfsFioGetMountedUnit(ff->unit);
	if (pfsMount==0)	return -ENODEV;

	parentOld=pfsInodeGetParent(pfsMount, NULL, old, path1, &result);
	if (parentOld){
		u32 nused=parentOld->nused;

		if (nused != 1){
			result=-EBUSY;
			goto exit;
		}

		if ((iFileOld=pfsInodeGetFileInDir(parentOld, path1, &result))==0) goto exit;

		if ((parentNew=pfsInodeGetParent(pfsMount, NULL, new, path2, &result))==0) goto exit;

		f=(iFileOld->u.inode->mode & FIO_S_IFMT) == FIO_S_IFDIR;

		if ((parentNew->nused != nused) && ((parentOld!=parentNew) || (parentNew->nused!=2))){
			result=-EBUSY;
			goto exit;
		}

		iFileNew=pfsInodeGetFileInDir(parentNew, path2, &result);
		if (iFileNew){
			if (f){
				if ((iFileNew->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
					result=-ENOTDIR;
				else
					if (pfsCheckDirForFiles(iFileNew)){
						if (iFileNew->nused >= 2)
							result=-EBUSY;
						else{
							if (iFileOld==iFileNew)
								goto exit;
						}
					}else
						result=-ENOTEMPTY;
			}else
				if ((iFileNew->u.inode->mode & FIO_S_IFMT) == FIO_S_IFDIR)
					result=-EISDIR;
				else
					if (iFileNew->nused >= 2)
						result=-EBUSY;
					else{
						if (iFileOld==iFileNew)
							goto exit;
					}
		}else
			if (result==-ENOENT)
				result=0;

		if (result)	goto exit;

		if (f && (parentOld!=parentNew)){
			pfs_cache_t *parent;

			parent=pfsCacheUsedAdd(parentNew);
			do{
				pfs_cache_t *tmp;

				if (parent==iFileOld){
					result=-EINVAL;
					break;
				}
				tmp=pfsInodeGetFileInDir(parent, "..", &result);
				pfsCacheFree(parent);
				if (tmp==parent)break;
				parent=tmp;
			}while(parent);
			pfsCacheFree(parent);
		}

		if (result==0){
			if (strcmp(path1, ".") && strcmp(path1, "..") &&
			    strcmp(path2, ".") && strcmp(path2, "..")){
				result=pfsCheckAccess(parentOld, 3);
				if (result==0)
					result=pfsCheckAccess(parentNew, 3);
			}else
				result=-EINVAL;

			if (result==0){
				if (iFileNew && ((removeNew=pfsDirRemoveEntry(parentNew, path2))==NULL))
					result=-ENOENT;
				else{
					removeOld=pfsDirRemoveEntry(parentOld, path1);
					if (removeOld==0)
						result=-ENOENT;
					else{
						addNew=pfsDirAddEntry(parentNew, path2, &iFileOld->u.inode->inode_block, iFileOld->u.inode->mode, &result);
						if (addNew && f && (parentOld!=parentNew))
							newParent=pfsSetDentryParent(iFileOld, &parentNew->u.inode->inode_block, &result);
					}
				}
			}
		}

		if (result){
			if (removeNew)	removeNew->pfsMount=NULL;
			if (removeOld)	removeOld->pfsMount=NULL;
			if (addNew)		addNew->pfsMount=NULL;
			if (iFileNew)	iFileNew->pfsMount=NULL;
			parentOld->pfsMount=NULL;
			parentNew->pfsMount=NULL;
		}else{
			if (parentOld==parentNew){
				if (removeOld!=addNew)
					removeOld->flags |= PFS_CACHE_FLAG_DIRTY;
			}else
			{
				pfsInodeSetTime(parentOld);
				removeOld->flags|=PFS_CACHE_FLAG_DIRTY;
			}
			pfsInodeSetTime(parentNew);
			addNew->flags|=PFS_CACHE_FLAG_DIRTY;

			if (newParent){
				pfsInodeSetTime(iFileOld);
				newParent->flags|=PFS_CACHE_FLAG_DIRTY;
				pfsCacheFree(newParent);
			}

			if (iFileNew){
				iFileNew->flags &= ~PFS_CACHE_FLAG_DIRTY;
				pfsBitmapFreeInodeBlocks(iFileNew);
			}

			if (pfsMount->flags & PFS_FIO_ATTR_WRITEABLE)
				pfsCacheFlushAllDirty(pfsMount);
		}
		if (removeOld)	pfsCacheFree(removeOld);
		if (addNew)		pfsCacheFree(addNew);
		if (removeNew)	pfsCacheFree(removeNew);
exit:
		if (iFileNew)	pfsCacheFree(iFileNew);
		pfsCacheFree(iFileOld);
		pfsCacheFree(parentOld);
		pfsCacheFree(parentNew);
	}
	SignalSema(pfsFioSema);
	return pfsFioCheckForLastError(pfsMount, result);
}

int pfsFioChdir(iop_file_t *f, const char *name)
{
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;
	int result = 0;

	if(!(pfsMount = pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	clink = pfsInodeGetFile(pfsMount, 0, name, &result);
	if(clink != NULL) {
		if((clink->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
			result = -ENOTDIR;
		else {

			result = pfsCheckAccess(clink, 0x01);

			if(result == 0)
				memcpy(&pfsMount->current_dir, &clink->u.inode->inode_block, sizeof(pfs_blockinfo_t));
		}

		pfsCacheFree(clink);
	}

	SignalSema(pfsFioSema);
	return pfsFioCheckForLastError(pfsMount, result);
}

static void _sync(void)
{
	u32 i;
	for(i=0;i<pfsConfig.maxOpen;i++)
	{
		pfs_restsInfo_t *info=&pfsFileSlots[i].restsInfo;
		if(info->dirty) {
			pfs_mount_t *pfsMount=pfsFileSlots[i].clink->pfsMount;
			pfsMount->blockDev->transfer(pfsMount->fd, &pfsFileSlots[i].restsBuffer,
				info->sub, info->sector, 1, PFS_IO_MODE_WRITE);
			pfsFileSlots[i].restsInfo.dirty=0;
		}
	}
}

int pfsFioSync(iop_file_t *f, const char *dev, int flag)
{
	pfs_mount_t *pfsMount;

	if(!(pfsMount = pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	_sync();
	pfsCacheFlushAllDirty(pfsMount);

	SignalSema(pfsFioSema);
	return pfsFioCheckForLastError(pfsMount, 0);
}

int pfsFioMount(iop_file_t *f, const char *fsname, const char *devname, int flag, void *arg, int arglen)
{
	int rv;
	int fd;
	pfs_block_device_t *blockDev;

	if(!(blockDev = pfsGetBlockDeviceTable(devname)))
		return -ENXIO;

	WaitSema(pfsFioSema);

	fd = open(devname, (flag & FIO_MT_RDONLY) ? O_RDONLY : O_RDWR, 0644); // ps2hdd.irx fd
	if(fd < 0)
		rv = fd;
	else {
		if((rv=mountDevice(blockDev, fd, f->unit, flag)) < 0)
			close(fd);
	}
	SignalSema(pfsFioSema);
	return rv;
}

int pfsFioUmount(iop_file_t *f, const char *fsname)
{
	u32 i;
	int rv=0;
	int	busy_flag=0;
	pfs_mount_t *pfsMount;

	if((pfsMount = pfsFioGetMountedUnit(f->unit))==NULL)
		return -ENODEV;

	for(i = 0; i < pfsConfig.maxOpen; i++)
	{
		if((pfsFileSlots[i].clink!=NULL) && (pfsFileSlots[i].clink->pfsMount==pfsMount))
		{
			busy_flag=1;
			break;
		}
	}
	if(busy_flag==0)
	{
		pfsCacheClose(pfsMount);
		close(pfsMount->fd);
		pfsClearMount(pfsMount);
	}
	else
		rv=-EBUSY;	// Mount device busy

	SignalSema(pfsFioSema);
	return rv;
}

int pfsFioSymlink(iop_file_t *f, const char *old, const char *new)
{
	int rv;
	pfs_mount_t *pfsMount;
	int mode=0x141FF;

	if(old==NULL || new==NULL)
		return -ENOENT;

	if(!(pfsMount=pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	rv = openFile(pfsMount, (pfs_file_slot_t *)old, (const char *)new, O_CREAT|O_WRONLY, mode);
	SignalSema(pfsFioSema);
	return rv;
}

int pfsFioReadlink(iop_file_t *f, const char *path, char *buf, int buflen)
{
	int rv=0;
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;

	if(buflen < 0)
		return -EINVAL;
	if(!(pfsMount=pfsFioGetMountedUnit(f->unit)))
		return -ENODEV;

	if((clink=pfsInodeGetFile(pfsMount, NULL, path, &rv))!=NULL)
	{
		if((clink->u.inode->mode & FIO_S_IFMT) == FIO_S_IFLNK)
			rv=-EINVAL;
		else
		{
			rv=strlen((char *)&clink->u.inode->data[1]);
			if(buflen < rv)
				rv=buflen;
			memcpy(buf, &clink->u.inode->data[1], rv);
		}
		pfsCacheFree(clink);
	}
	SignalSema(pfsFioSema);

	return pfsFioCheckForLastError(pfsMount, rv);
}

int pfsFioUnsupported(void)
{
	PFS_PRINTF(PFS_DRV_NAME" Error: Operation currently unsupported.\n");

	return -1;
}
