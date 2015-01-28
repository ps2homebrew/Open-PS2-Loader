/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
# PFS I/O manager related routines
*/

#include "pfs.h"

///////////////////////////////////////////////////////////////////////////////
//   Globals

u8 openFlagArray[] = { 0, 4, 2, 6, 0, 0, 0, 0 };
int symbolicLinks = 0;
int pfsFioSema = 0;
pfs_file_slot_t *fileSlots;

///////////////////////////////////////////////////////////////////////////////
//   Function defenitions

int checkForLastError(pfs_mount_t *pfsMount, int rv)
{
	return pfsMount->lastError ? pfsMount->lastError : rv;
}

int checkFileSlot(pfs_file_slot_t *fileSlot)
{
	WaitSema(pfsFioSema);

	if(fileSlot->clink==NULL)
	{
		SignalSema(pfsFioSema);
		return -EBADF;
	}

	return 0;
}

void closeFileSlot(pfs_file_slot_t *fileSlot)
{
	pfs_mount_t *pfsMount=fileSlot->clink->pfsMount;

	if(fileSlot->fd->mode & O_WRONLY)
	{
		if(fileSlot->restsInfo.dirty!=0)
		{
			pfsMount->blockDev->transfer(pfsMount->fd, fileSlot->restsBuffer,
				fileSlot->restsInfo.sub, fileSlot->restsInfo.sector, 1, IOCTL2_TMODE_WRITE);
		}
		inodeUpdateTime(fileSlot->clink);	// set time :P
		fileSlot->clink->u.inode->attr|=FIO_ATTR_CLOSED;
		if(pfsMount->flags & FIO_ATTR_WRITEABLE)
			cacheFlushAllDirty(pfsMount);
	}
	cacheAdd(fileSlot->block_pos.inode);
	cacheAdd(fileSlot->clink);
	memset(fileSlot, 0, sizeof(pfs_file_slot_t));
}

pfs_mount_t *fioGetMountedUnit(int unit)
{
	pfs_mount_t *pfsMount;

	WaitSema(pfsFioSema);
	if((pfsMount=getMountedUnit(unit))==NULL)
		SignalSema(pfsFioSema);
	return pfsMount;
}

int mountDevice(block_device *blockDev, int fd, int unit, int flag)
{
	u32 i;
	int rv;

	if((u32)unit >= pfsConfig.maxMount)
		return -EMFILE;

	if(pfsMountBuf[unit].flags & MOUNT_BUSY)
		return -EBUSY;

	for(i = 0; i < pfsConfig.maxMount; i++)
		if((pfsMountBuf[i].flags & MOUNT_BUSY) &&
			(blockDev == pfsMountBuf[i].blockDev) &&
			(fd == pfsMountBuf[i].fd)) // Cant mount the same partition more than once
			return -EBUSY;

	//	dprintf("ps2fs: Found free mount buffer!\n");

	pfsMountBuf[unit].blockDev = blockDev;
	pfsMountBuf[unit].fd = fd;
	pfsMountBuf[unit].flags = flag;

	rv = mountSuperBlock(&pfsMountBuf[unit]);
	if(rv < 0)
		return rv;

	pfsMountBuf[unit].flags |= MOUNT_BUSY;
	dprintf("ps2fs: mount successfull!\n");

	return 0;
}

int	openFile(pfs_mount_t *pfsMount, pfs_file_slot_t *freeSlot, const char *filename, int openFlags, int mode)
{
	char file[256];
	int  result, result2, result3, result4;
	pfs_cache_t *parentInode, *fileInode, *cached;


	result = 0;	//no error
	// Get the inode for the directory which contains the file (dir) in filename
	// After this call, 'file' will contain the name of the file we're operating on.
	if ((parentInode=inodeGetParent(pfsMount, NULL, filename, file, &result))==0)
	{
		dprintf("ps2fs: Failed to get parent inode\n");
		return result;
	}

//	dprintf("ps2fs: Got parent inode!\n");
//	inodePrint(parentInode->u.inode);

	// Get the inode for the actual file/directory contained in the parent dir
	fileInode=inodeGetFileInDir(parentInode, file, &result);

//	dprintf("ps2fs: got file inode! (file: %s)\n", file);
//	inodePrint(fileInode->u.inode);

	// If file already exists..
	if (fileInode)
	{
		u32 flags;
		u32 count;

		dprintf("ps2fs: File inode already exists\n");

		// Setup flags
		flags=openFlagArray[openFlags & O_RDWR];
		if (openFlags & O_TRUNC)	flags |= 2;
		if (openFlags & FDIRO)		flags |= 4;
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
				dprintf("ps2fs: Resolving symlink..\n");

				if (count++>=4)
				{
					result=-ELOOP;
					goto label;
				}

				parentInode=inodeGetParent(pfsMount, parentInode, (char*)&fileInode->u.inode->data[1],
												file, &result);
				cacheAdd(fileInode);
				if ((parentInode==0) ||
				    ((fileInode=inodeGetFileInDir(parentInode, file, &result))==0))
					goto label;
			}

			// Make sure that if a file is being opened, then inode does not point
			// to a directory, and vice versa.
			if ((openFlags & FDIRO) == 0)
			{
				if ((fileInode->u.inode->mode & FIO_S_IFMT) == FIO_S_IFDIR)
					result=-EISDIR;
			}else{
				if ((fileInode->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
					result=-ENOTDIR;
			}

			// Truncate file if required
			if ((result==0) &&
			    ((result=checkAccess(fileInode, flags & 0xFFFF))==0) &&
			    (openFlags & O_TRUNC))
			{

				dprintf("ps2fs: Truncating file..\n");

				cached=cacheGetData(fileInode->pfsMount, fileInode->sub, fileInode->sector+1, CACHE_FLAG_NOLOAD | CACHE_FLAG_NOTHING, &result2);
				if (cached)
				{
					memset(cached->u.aentry, 0, sizeof(pfs_inode)); //1024
					cached->u.aentry->aLen=sizeof(pfs_inode);
					cached->flags |= CACHE_FLAG_DIRTY;
					cacheAdd(cached);
				}
				if (result2 == 0)
				{
					fileInode->u.inode->size = 0;
					fileInode->u.inode->attr &= ~FIO_ATTR_CLOSED; //~0x80==0xFF7F
					fileInode->flags |= CACHE_FLAG_DIRTY;
					ioctl2Free(fileInode);
				}
			}
		}

	// Otherwise, if file doesnt already exist..
	}else{

		dprintf("ps2fs: File inode does not exist..\n");

		if ((openFlags & O_CREAT) && (result==-ENOENT) &&
		    ((result=checkAccess(parentInode, 2))==0) &&
		    (fileInode=inodeCreateNewFile(parentInode, mode, pfsMount->uid,
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
				cached=cacheGetData(fileInode->pfsMount, fileInode->u.inode->data[1].subpart,
					fileInode->u.inode->data[1].number << fileInode->pfsMount->inode_scale,
					CACHE_FLAG_NOLOAD | CACHE_FLAG_NOTHING, &result3);
				if (cached)
				{
					fillSelfAndParentDentries(cached,
						&fileInode->u.inode->inode_block,
						&parentInode->u.inode->inode_block);
					cached->flags |= CACHE_FLAG_DIRTY;
					cacheAdd(cached);
				}
				result=result3;
			// Otherwise if its just a regular file, just zero its attribute entry
			}else{
				cached=cacheGetData(fileInode->pfsMount, fileInode->sub, fileInode->sector+1,
						CACHE_FLAG_NOLOAD | CACHE_FLAG_NOTHING, &result4);
				if (cached)
				{
					memset(cached->u.aentry, 0, sizeof(pfs_inode));
					cached->u.aentry->aLen=sizeof(pfs_inode);
					cached->flags |= CACHE_FLAG_DIRTY;
					cacheAdd(cached);
				}
				result=result4;
			}

			// Link new file with parent directory
			if ((result==0) && (cached=dirAddEntry(parentInode, file, &fileInode->u.inode->inode_block,
							 mode, &result)))
			{
				inodeUpdateTime(parentInode);
				cached->flags|=CACHE_FLAG_DIRTY;
				cacheAdd(cached);
			}
		}
	}
label:
	cacheAdd(parentInode);
	if ((result==0) && freeSlot && fileInode)
	{

//		dprintf("ps2fs: Setup file slot sizes\n");

		freeSlot->clink=fileInode;
		if (openFlags & O_APPEND)
			freeSlot->position = fileInode->u.inode->size;
		else
			freeSlot->position = 0;

		result=blockInitPos(freeSlot->clink, &freeSlot->block_pos, freeSlot->position);
		if (result==0)
		{
			if ((openFlags & O_WRONLY) &&
			    (fileInode->u.inode->attr & FIO_ATTR_CLOSED)){
				fileInode->u.inode->attr &= ~FIO_ATTR_CLOSED;
				fileInode->flags |= CACHE_FLAG_DIRTY;
				if (pfsMount->flags & FIO_ATTR_WRITEABLE)
					cacheFlushAllDirty(pfsMount);
			}
			if ((result=checkForLastError(pfsMount, result))==0)
				return 0;
		}
		freeSlot->clink=NULL;
	}

	if(fileInode) cacheAdd(fileInode);

	return checkForLastError(pfsMount, result);
}

// Reads unaligned data, or remainder data (size < 512)
int fileTransferRemainder(pfs_file_slot_t *fileSlot, void *buf, int size, int operation)
{
	u32 sector, pos;
	int result;
	pfs_blockpos_t *blockpos = &fileSlot->block_pos;
	pfs_mount_t *pfsMount = fileSlot->clink->pfsMount;
	pfs_restsInfo_t *info = &fileSlot->restsInfo;
	pfs_blockinfo *bi = blockGetCurrent(blockpos);

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
		if (pfsMount->flags & FIO_ATTR_WRITEABLE)
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
int fileTransfer(pfs_file_slot_t *fileSlot, u8 *buf, int size, int operation)
{
	int result=0;
	pfs_blockpos_t *blockpos=&fileSlot->block_pos;
	pfs_mount_t *pfsMount=fileSlot->clink->pfsMount;
	u32 bytes_remain;
	u32 total = 0;

//	dprintf2("ps2fs CALL: fileTransfer( , ,%d,%d)\n", size, operation);

	// If we're writing and there is less free space in the last allocated block segment
	// than can hold the data being written, then try and expand the block segment
	if ((operation==1) &&
	    (fileSlot->clink->u.inode->number_data - 1 == blockpos->block_segment))
	{
		bytes_remain = (blockGetCurrent(blockpos)->count - blockpos->block_offset) * pfsMount->zsize - blockpos->byte_offset;//u32

		if (bytes_remain < size)
		{
			blockExpandSegment(fileSlot->clink, blockpos,
				((size-bytes_remain+pfsMount->zsize-1) & (~(pfsMount->zsize-1))) / pfsMount->zsize);
		}
	}

	while (size)
	{
		pfs_blockinfo *bi;
		u32 sectors;

		// Get block info for current file position
		bi=blockGetCurrent(blockpos);

		// Get amount of remaining data in current block
		bytes_remain=(bi->count - blockpos->block_offset) * pfsMount->zsize - blockpos->byte_offset;

		// If there is no space/data left in the current block segment, then we need to move onto the next
		if (bytes_remain==0)
		{
			// If we're at the end of allocated block segments, then allocate a new block segment
			if (blockpos->block_segment == fileSlot->clink->u.inode->number_data-1){
				if (operation==0){
					printf("ps2fs: Panic: This is a bug!\n");
					return 0;
				}

				result = blockAllocNewSegment(fileSlot->clink, blockpos,
								(size - 1 + pfsMount->zsize) / pfsMount->zsize);
				if(result<0)
					break;

			// Otherwise, move to the next block segment
			}else
				if ((result=blockSeekNextSegment(fileSlot->clink, blockpos)))
				{
					dprintf("ps2fs: Failed to seek to next block segment!\n");
					return result;
				}

			bi=blockGetCurrent(blockpos);
			bytes_remain=(bi->count - blockpos->block_offset) * pfsMount->zsize - blockpos->byte_offset;
		}

		if (bytes_remain==0)
		{
			printf("ps2fs: Panic: There is no zone.\n");
			return 0;
		}

		// If we are transferring size not a multiple of 512, under 512, or to
		// an unaligned buffer we need to use a special function rather than
		// just doing a ATA sector transfer.
		if ((blockpos->byte_offset & 0x1FF) || (size < 512) || ((u32)buf & 3))
		{
			dprintf("ps2fs: Transfer unaligned (size = %d, op = %d)\n", size, operation);
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
			fileSlot->clink->flags |= CACHE_FLAG_DIRTY;
		}

		blockpos->block_offset+=blockSyncPos(blockpos, result);
	}
	return result < 0 ? result : total;
}

void	pfsPowerOffHandler(void* data)
{
	printf("pfs close all\n");
	devctlCloseAll();
}

int	pfsInit(iop_device_t *f)
{
	iop_sema_t sema;

	sema.attr = 1;
	sema.option = 0;
	sema.initial = 1;
	sema.max = 1;

	pfsFioSema = CreateSema(&sema);

	AddPowerOffHandler(pfsPowerOffHandler, 0);
	return 0;
}

int	pfsDeinit(iop_device_t *f)
{
	devctlCloseAll();

	DeleteSema(pfsFioSema);

	return 0;
}

int	pfsFormat(iop_file_t *t, const char *dev, const char *blockdev, void *args, size_t arglen)
{
	int *arg = (int *)args;
	int fragment = 0;
	int fd;
	block_device *blockDev;
	int rv;

	// Has a fragment bit pattern been specified ?
	if((arglen == (3 * sizeof(int))) && (arg[1] == 0x2D66))
		fragment = arg[2];

	if((blockDev = getDeviceTable(blockdev)) == 0)
		return -ENXIO;

	WaitSema(pfsFioSema);

	fd = open(blockdev, O_RDWR, 0644);

	dprintf("ps2fs: Format: fragment = 0x%X\n", fragment);

	if(fd < 0)
		rv = fd;
	else {
		rv = _format(blockDev, fd, arg[0], fragment);
		close(fd);
	}

	SignalSema(pfsFioSema);

	return rv;
}

int	pfsOpen(iop_file_t *f, const char *name, int flags, int mode)
{
	int rv = 0;
	u32 i;
	pfs_file_slot_t *freeSlot;
	pfs_mount_t *pfsMount;

	if(!name)
		return -ENOENT;

	if(!(pfsMount = fioGetMountedUnit(f->unit)))
	{
		dprintf("Couldnt get mounted unit!\n");
		return -ENODEV;
	}

	// Find free file slot
	for(i = 0; i < pfsConfig.maxOpen; i++)
	{
		if(!fileSlots[i].fd) {
			freeSlot = &fileSlots[i];
			goto pfsOpen_slotFound;
		}
	}

	printf("ps2fs: Error: There are no free file slots!\n");
	freeSlot = NULL;

pfsOpen_slotFound:

	if(!freeSlot) {
		rv = -EMFILE;
		goto pfsOpen_end;
	}

	// Do actual open
	if((rv = openFile(pfsMount, freeSlot, name, f->mode, (mode & 0xfff) | FIO_S_IFREG)))
		goto pfsOpen_end;

	dprintf("ps2fs: File opened successfully!\n");

	freeSlot->fd = f;
	f->privdata = freeSlot;

pfsOpen_end:

	SignalSema(pfsFioSema);

	return rv;
}

int	pfsClose(iop_file_t *f)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t *)f->privdata;
	int rv;

	rv = checkFileSlot(fileSlot);
	if(rv)
		return rv;

	closeFileSlot(fileSlot);

	SignalSema(pfsFioSema);

	return rv;
}

int pfsRead(iop_file_t *f, void *buf, int size)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t*)f->privdata;
	int result = checkFileSlot(fileSlot);

	if (result)
	{
		dprintf("ps2fs: bad file slot on read\n");
		return result;
	}

	// Check bounds, adjust size if necessary
	if(fileSlot->clink->u.inode->size <  (fileSlot->position + size))
		size = fileSlot->clink->u.inode->size - fileSlot->position;

	result = size ? fileTransfer(fileSlot, buf, size, 0) : 0;
	result = checkForLastError(fileSlot->clink->pfsMount, result);

	SignalSema(pfsFioSema);
	return result;
}

int pfsWrite(iop_file_t *f, void *buf, int size)
{
	pfs_mount_t *pfsMount;
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t*)f->privdata;
	int result = checkFileSlot(fileSlot);

	if(result)
		return result;

	pfsMount = fileSlot->clink->pfsMount;

	result = fileTransfer(fileSlot, buf, size, 1);

	if (pfsMount->flags & FIO_ATTR_WRITEABLE)
		cacheFlushAllDirty(pfsMount);

	result = checkForLastError(pfsMount, result);
	SignalSema(pfsFioSema);
	return result;
}

s64 _seek(pfs_file_slot_t *fileSlot, s64 offset, int whence, int mode)
{
	int rv;
	s64 startPos, newPos;

	if (mode & O_DIROPEN)
	{
		dprintf("ps2fs: Cant seek: directory\n");
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
		dprintf("ps2fs: Cant seek: invalid args\n");
		return -EINVAL;
	}

	cacheAdd(fileSlot->block_pos.inode);
	rv=blockInitPos(fileSlot->clink, &fileSlot->block_pos, newPos);
	if (rv==0)
		fileSlot->position = newPos;
	else {
		fileSlot->position = 0;
		blockInitPos(fileSlot->clink, &fileSlot->block_pos, 0);
	}

	if (rv)
	{
		dprintf("ps2fs: Cant seek: failed to blockInitPos\n");
		return rv;
	}

	dprintf("ps2fs: Seek successfull! newPos = %ld\n", (u32)newPos);

	return newPos;
}

int pfsLseek(iop_file_t *f, int pos, int whence)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t*)f->privdata;
	int result = checkFileSlot(fileSlot);

	if (result)
		return result;

	result = (u32)_seek(fileSlot, (s64)pos, whence, f->mode);

	SignalSema(pfsFioSema);
	return result;
}

s64 pfsLseek64(iop_file_t *f, s64 offset, int whence)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t *)f->privdata;
	u64 rv;

	rv=checkFileSlot(fileSlot);
	if(!rv)
	{
		rv=_seek(fileSlot, offset, whence, f->mode);
		SignalSema(pfsFioSema);
	}
	return rv;
}

int _remove(pfs_mount_t *pfsMount, const char *path, int mode)
{
	char szPath[256];
	int rv=0;
	pfs_cache_t *parent;
	pfs_cache_t *file;

	if((parent=inodeGetParent(pfsMount, NULL, path, szPath, &rv))==NULL)
		return rv;

	if((file=inodeGetFileInDir(parent, szPath, &rv))!=NULL)
	{
		if(mode!=0)
		{// remove dir
			if((file->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
				rv=-ENOTDIR;
			else if(checkDirForFiles(file)==0)
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
				rv=checkAccess(file, 2);
			if(file->nused>=2)
				rv=-EBUSY;
			if(rv==0)
				return inodeRemove(parent, file, szPath);
		}
		cacheAdd(file);
	}
	cacheAdd(parent);
	return checkForLastError(pfsMount, rv);
}


int	pfsRemove(iop_file_t *f, const char *name)
{
	pfs_mount_t *pfsMount;
	int rv;

	if(!(pfsMount = fioGetMountedUnit(f->unit)))
		return -ENODEV;

	rv = _remove(pfsMount, name, 0);

	SignalSema(pfsFioSema);

	return rv;
}

int	pfsMkdir(iop_file_t *f, const char *path, int mode)
{
	pfs_mount_t *pfsMount;
	int rv;

	mode = (mode & 0xfff) | 0x10000 | FIO_S_IFDIR;	// TODO: change to some constant/macro

	if(!(pfsMount = fioGetMountedUnit(f->unit)))
		return -ENODEV;

	rv = openFile(pfsMount, NULL, path, O_CREAT | O_WRONLY, mode);

	SignalSema(pfsFioSema);

	return rv;
}

int	pfsRmdir(iop_file_t *f, const char *path)
{
	pfs_mount_t *pfsMount;
	const char *temp;
	int rv;

	temp = path + strlen(path);
	if(*temp == '.')
		return -EINVAL;

	if(!(pfsMount = fioGetMountedUnit(f->unit)))
		return -ENODEV;

	rv = _remove(pfsMount, path, 0x01); // TODO: constant for 0x01 ?

	SignalSema(pfsFioSema);

	return rv;
}

int pfsDopen(iop_file_t *f, const char *name)
{
    return pfsOpen(f, name, 0, 0);
}

int	pfsDread(iop_file_t *f, iox_dirent_t *dirent)
{
	pfs_file_slot_t *fileSlot = (pfs_file_slot_t *)f->privdata;
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;
	pfs_blockinfo bi;
	int result;
	int rv;

//	dprintf2("ps2fs CALL: pfsDread();\n");

	rv = checkFileSlot(fileSlot);
	if(rv < 0)
		return rv;

	pfsMount = fileSlot->clink->pfsMount;

	if((fileSlot->clink->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
	{
		dprintf("ps2fs: dread error: not directory!\n");
		rv = -ENOTDIR;
	}
	else
		if((rv = getNextDentry(fileSlot->clink, &fileSlot->block_pos, (u32 *)&fileSlot->position,
									dirent->name, &bi)) > 0)
		{

			clink = inodeGetData(pfsMount, bi.subpart, bi.number, &result);
			if(clink != NULL)
			{
				fioStatFiller(clink, &dirent->stat);
				cacheAdd(clink);
			}

			if(result)
				rv = result;
		}

	rv = checkForLastError(pfsMount, rv);
	SignalSema(pfsFioSema);

	return rv;
}

void fioStatFiller(pfs_cache_t *clink, iox_stat_t *stat)
{
	stat->mode = clink->u.inode->mode;
	stat->attr = clink->u.inode->attr;
	stat->size = (u32)clink->u.inode->size;
	stat->hisize = (u32)(clink->u.inode->size >> 32);
	memcpy(&stat->ctime, &clink->u.inode->ctime, sizeof(pfs_datetime));
	memcpy(&stat->atime, &clink->u.inode->atime, sizeof(pfs_datetime));
	memcpy(&stat->mtime, &clink->u.inode->mtime, sizeof(pfs_datetime));
	stat->private_0 = clink->u.inode->uid;
	stat->private_1 = clink->u.inode->gid;
	stat->private_2 = clink->u.inode->number_blocks;
	stat->private_3 = clink->u.inode->number_data;
	stat->private_4 = 0;
	stat->private_5 = 0;
}

int	pfsGetstat(iop_file_t *f, const char *name, iox_stat_t *stat)
{
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;
	int rv=0;

	if(!(pfsMount = fioGetMountedUnit(f->unit)))
		return -ENODEV;

	clink=inodeGetFile(pfsMount, NULL, name, &rv);
	if(clink!=NULL)
	{
		fioStatFiller(clink, stat);
		cacheAdd(clink);
	}

	SignalSema(pfsFioSema);
	return checkForLastError(pfsMount, rv);
}

int	pfsChstat(iop_file_t *f, const char *name, iox_stat_t *stat, unsigned int statmask)
{
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;
	int rv = 0;

	if(!(pfsMount = fioGetMountedUnit(f->unit)))
		return -ENODEV;

	clink = inodeGetFile(pfsMount, NULL, name, &rv);
	if(clink != NULL) {

		rv = checkAccess(clink, 0x02);
		if(rv == 0) {

			clink->flags |= 0x01;

			if((statmask & FIO_CST_MODE) && ((clink->u.inode->mode & FIO_S_IFMT) != FIO_S_IFLNK))
				clink->u.inode->mode = (clink->u.inode->mode & FIO_S_IFMT) | (stat->mode & 0xfff);
			if(statmask & FIO_CST_ATTR)
				clink->u.inode->attr = (clink->u.inode->attr & 0xA0) | (stat->attr & 0xFF5F);
			if(statmask & FIO_CST_SIZE)
				rv = -EACCES;
			if(statmask & FIO_CST_CT)
				memcpy(&clink->u.inode->ctime, stat->ctime, sizeof(pfs_datetime));
			if(statmask & FIO_CST_AT)
				memcpy(&clink->u.inode->atime, stat->atime, sizeof(pfs_datetime));
			if(statmask & FIO_CST_MT)
				memcpy(&clink->u.inode->mtime, stat->mtime, sizeof(pfs_datetime));
			if(statmask & FIO_CST_PRVT) {
				clink->u.inode->uid = stat->private_0;
				clink->u.inode->gid = stat->private_1;
			}

			if(pfsMount->flags & FIO_ATTR_WRITEABLE)
				cacheFlushAllDirty(pfsMount);
		}

		cacheAdd(clink);

	}

	SignalSema(pfsFioSema);
	return checkForLastError(pfsMount, rv);
}

int pfsRename(iop_file_t *ff, const char *old, const char *new)
{
	char path1[256], path2[256];
	int result=0;
	pfs_mount_t *pfsMount;
	int f;
	pfs_cache_t *parentOld=NULL, *parentNew=NULL;
	pfs_cache_t *removeOld=NULL, *removeNew=NULL;
	pfs_cache_t *iFileOld=NULL, *iFileNew=NULL;
	pfs_cache_t *newParent=NULL,*addNew=NULL;

	pfsMount=fioGetMountedUnit(ff->unit);
	if (pfsMount==0)	return -ENODEV;

	parentOld=inodeGetParent(pfsMount, NULL, old, path1, &result);
	if (parentOld){
		u32 nused=parentOld->nused;

		if (nused != 1){
			result=-EBUSY;
			goto exit;
		}

		if ((iFileOld=inodeGetFileInDir(parentOld, path1, &result))==0) goto exit;

		if ((parentNew=inodeGetParent(pfsMount, NULL, new, path2, &result))==0) goto exit;

		f=(iFileOld->u.inode->mode & FIO_S_IFMT) == FIO_S_IFDIR;

		if ((parentNew->nused != nused) && ((parentOld!=parentNew) || (parentNew->nused!=2))){
			result=-EBUSY;
			goto exit;
		}

		iFileNew=inodeGetFileInDir(parentNew, path2, &result);
		if (iFileNew){
			if (f){
				if ((iFileNew->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
					result=-ENOTDIR;
				else
					if (checkDirForFiles(iFileNew)){
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

			parent=cacheUsedAdd(parentNew);
			do{
				pfs_cache_t *tmp;

				if (parent==iFileOld){
					result=-EINVAL;
					break;
				}
				tmp=inodeGetFileInDir(parent, "..", &result);
				cacheAdd(parent);
				if (tmp==parent)break;
				parent=tmp;
			}while(parent);
			cacheAdd(parent);
		}

		if (result==0){
			if (strcmp(path1, ".") && strcmp(path1, "..") &&
			    strcmp(path2, ".") && strcmp(path2, "..")){
				result=checkAccess(parentOld, 3);
				if (result==0)
					result=checkAccess(parentNew, 3);
			}else
				result=-EINVAL;

			if (result==0){
				if (iFileNew && ((removeNew=dirRemoveEntry(parentNew, path2))==NULL))
					result=-ENOENT;
				else{
					removeOld=dirRemoveEntry(parentOld, path1);
					if (removeOld==0)
						result=-ENOENT;
					else{
						addNew=dirAddEntry(parentNew, path2, &iFileOld->u.inode->inode_block, iFileOld->u.inode->mode, &result);
						if (addNew && f && (parentOld!=parentNew))
							newParent=setParent(iFileOld, &parentNew->u.inode->inode_block, &result);
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
					removeOld->flags |= CACHE_FLAG_DIRTY;
			}else
			{
				inodeUpdateTime(parentOld);
				removeOld->flags|=CACHE_FLAG_DIRTY;
			}
			inodeUpdateTime(parentNew);
			addNew->flags|=CACHE_FLAG_DIRTY;

			if (newParent){
				inodeUpdateTime(iFileOld);
				newParent->flags|=CACHE_FLAG_DIRTY;
				cacheAdd(newParent);
			}

			if (iFileNew){
				iFileNew->flags &= ~CACHE_FLAG_DIRTY;
				bitmapFreeInodeBlocks(iFileNew);
			}

			if (pfsMount->flags & FIO_ATTR_WRITEABLE)
				cacheFlushAllDirty(pfsMount);
		}
		if (removeOld)	cacheAdd(removeOld);
		if (addNew)		cacheAdd(addNew);
		if (removeNew)	cacheAdd(removeNew);
exit:
		if (iFileNew)	cacheAdd(iFileNew);
		cacheAdd(iFileOld);
		cacheAdd(parentOld);
		cacheAdd(parentNew);
	}
	SignalSema(pfsFioSema);
	return checkForLastError(pfsMount, result);
}

int pfsChdir(iop_file_t *f, const char *name)
{
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;
	int result = 0;

	if(!(pfsMount = fioGetMountedUnit(f->unit)))
		return -ENODEV;

	clink = inodeGetFile(pfsMount, 0, name, &result);
	if(clink != NULL) {
		if((clink->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR)
			result = -ENOTDIR;
		else {

			result = checkAccess(clink, 0x01);

			if(result == 0)
				memcpy(&pfsMount->current_dir, &clink->u.inode->inode_block, sizeof(pfs_blockinfo));
		}

		cacheAdd(clink);
	}

	SignalSema(pfsFioSema);
	return checkForLastError(pfsMount, result);
}

void _sync()
{
	u32 i;
	for(i=0;i<pfsConfig.maxOpen;i++)
	{
		pfs_restsInfo_t *info=&fileSlots[i].restsInfo;
		if(info->dirty) {
			pfs_mount_t *pfsMount=fileSlots[i].clink->pfsMount;
			pfsMount->blockDev->transfer(pfsMount->fd, &fileSlots[i].restsBuffer,
				info->sub, info->sector, 1, IOCTL2_TMODE_WRITE);
			fileSlots[i].restsInfo.dirty=0;
		}
	}
}

int pfsSync(iop_file_t *f, const char *dev, int flag)
{
	pfs_mount_t *pfsMount;

	if(!(pfsMount = fioGetMountedUnit(f->unit)))
		return -ENODEV;

	_sync();
	cacheFlushAllDirty(pfsMount);
	hddFlushCache(pfsMount->fd);

	SignalSema(pfsFioSema);
	return checkForLastError(pfsMount, 0);
}

int pfsMount(iop_file_t *f, const char *fsname, const char *devname, int flag, void *arg, size_t arglen)
{
	int rv;
	int fd;
	block_device *blockDev;

	if(!(blockDev = getDeviceTable(devname)))
		return -ENXIO;

	WaitSema(pfsFioSema);

	fd = open(devname, (flag & FIO_MT_RDONLY) ? O_RDONLY : O_RDWR, 0644); // ps2hdd.irx fd
	if(fd < 0)
		rv = fd;
	else {
		dprintf("ps2fs: Mounting device..\n");
		if((rv=mountDevice(blockDev, fd, f->unit, flag)) < 0)
			close(fd);
	}
	SignalSema(pfsFioSema);
	return rv;
}

void _umount(pfs_mount_t *pfsMount)
{
	u32 i;

	cacheFlushAllDirty(pfsMount);
	for(i=1; i < numBuffers+1;i++){
		if(cacheBuf[i].pfsMount==pfsMount)
			cacheBuf[i].pfsMount=NULL;
	}

	hddFlushCache(pfsMount->fd);
}

int pfsUmount(iop_file_t *f, const char *fsname)
{
	u32 i;
	int rv=0;
	int	busy_flag=0;
	pfs_mount_t *pfsMount;

	if((pfsMount = fioGetMountedUnit(f->unit))==NULL)
		return -ENODEV;

	for(i = 0; i < pfsConfig.maxOpen; i++)
	{
		if((fileSlots[i].clink!=NULL) && (fileSlots[i].clink->pfsMount==pfsMount))
		{
			busy_flag=1;
			break;
		}
	}
	if(busy_flag==0)
	{
		_umount(pfsMount);
		close(pfsMount->fd);
		clearMount(pfsMount);
	}
	else
		rv=-EBUSY;	// Mount device busy

	SignalSema(pfsFioSema);
	return rv;
}

int pfsSymlink(iop_file_t *f, const char *old, const char *new)
{
	int rv;
	pfs_mount_t *pfsMount;
	int mode=0x141FF;

	if(old==NULL || new==NULL)
		return -ENOENT;

	if(!(pfsMount=fioGetMountedUnit(f->unit)))
		return -ENODEV;

	rv = openFile(pfsMount, (pfs_file_slot_t *)old, (const char *)new, O_CREAT|O_WRONLY, mode);
	SignalSema(pfsFioSema);
	return rv;
}

int pfsReadlink(iop_file_t *f, const char *path, char *buf, int buflen)
{
	int rv=0;
	pfs_mount_t *pfsMount;
	pfs_cache_t *clink;

	if(buflen < 0)
		return -EINVAL;
	if(!(pfsMount=fioGetMountedUnit(f->unit)))
		return -ENODEV;

	if((clink=inodeGetFile(pfsMount, NULL, path, &rv))!=NULL)
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
		cacheAdd(clink);
	}
	SignalSema(pfsFioSema);

	return checkForLastError(pfsMount, rv);
}

int fioUnsupported()
{
	printf("ps2fs: Error: Operation currently unsupported.\n");

	return -1;
}
