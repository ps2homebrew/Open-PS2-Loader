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
# PFS inode manipulation routines
*/

#include "pfs.h"

///////////////////////////////////////////////////////////////////////////////
//   Globals
int blockSize=1;// block size(in sectors(512) )


///////////////////////////////////////////////////////////////////////////////
//   Function defenitions


void inodePrint(pfs_inode *inode)
{
	dprintf("ps2fs: inodePrint: Checksum = 0x%lX, Magic = 0x%lX\n", inode->checksum, inode->magic);
	dprintf("ps2fs:     Mode = 0x%X, attr = 0x%X\n", inode->mode, inode->attr);
	dprintf("ps2fs:     size = 0x%08lX%08lX\n", (u32)(inode->size >> 32), (u32)(inode->size));
}

int inodeCheckSum(pfs_inode *inode)
{
	u32 *ptr=(u32 *)inode;
	u32 sum=0;
	int i;

	for(i=1; i < 256; i++)
		sum+=ptr[i];
	return sum;
}

pfs_cache_t *inodeGetData(pfs_mount_t *pfsMount, u16 sub, u32 inode, int *result)
{
	return cacheGetData(pfsMount, sub, inode << pfsMount->inode_scale,
				CACHE_FLAG_SEGD, result);
}

void inodeUpdateTime(pfs_cache_t *clink)
{	// set the inode time's in cache
	getPs2Time(&clink->u.inode->mtime);
	memcpy(&clink->u.inode->ctime, &clink->u.inode->mtime, sizeof(pfs_datetime));
	memcpy(&clink->u.inode->atime, &clink->u.inode->mtime, sizeof(pfs_datetime));
	clink->flags|=CACHE_FLAG_DIRTY;
}

// Returns the cached inode for the file (dir) in the directory pointed
// to by the dir inode.
pfs_cache_t *inodeGetFileInDir(pfs_cache_t *dirInode, char *path, int *result)
{
	pfs_dentry *dentry;
	u32	size;
	pfs_cache_t *clink;

	if (path[0]==0)
		return cacheUsedAdd(dirInode);

	// If we're in the root dir, dont do anything for ".."
	if ((dirInode->sector ==
	     dirInode->pfsMount->root_dir.number << dirInode->pfsMount->inode_scale) &&
	    (dirInode->sub    == dirInode->pfsMount->root_dir.subpart) &&
	    (strcmp(path, "..") == 0))
		return cacheUsedAdd(dirInode);

	if ((*result=checkAccess(dirInode, 1)) < 0)
		return NULL;

	// Get dentry of file/dir specified by path from the dir pointed to
	// by the inode (dirInode). Then return the cached inode for that dentry.
	if ((clink=getDentry(dirInode, path, &dentry, &size, 0))){
		cacheAdd(clink);
		return inodeGetData(dirInode->pfsMount,
			dentry->sub, dentry->inode, result);
	}

	*result=-ENOENT;
	return NULL;
}

int inodeSync(pfs_blockpos_t *blockpos, u64 size, u32 used_segments)
{
	int result=0;
	u32 i;
	u16 count;

	for(i=blockSyncPos(blockpos, size); i; )
	{
		count=blockpos->inode->u.inode->data[fixIndex(blockpos->block_segment)].count;

		i+=blockpos->block_offset;

		if (i < count){
			blockpos->block_offset=i;
			break;
		}

		i-=count;

		if (blockpos->block_segment + 1 == used_segments)
		{
			blockpos->block_offset=count;
			if (i || blockpos->byte_offset){
				printf("ps2fs: panic: fp exceeds file.\n");
				return -EINVAL;
			}
		}else{
			blockpos->block_offset=0;
			blockpos->block_segment++;
		}

		if (fixIndex(blockpos->block_segment))
			continue;

		if ((blockpos->inode = blockGetNextSegment(blockpos->inode, &result)) == 0)
			break;

		i++;
	}

	return result;
}

pfs_cache_t *inodeGetFile(pfs_mount_t *pfsMount, pfs_cache_t *clink,
			const char *name, int *result) {
	char path[256];
	pfs_cache_t *c;

	c=inodeGetParent(pfsMount, clink, name, path, result);
	if (c){
		c=inodeGetFileInDir(c, path, result);
		cacheAdd(c);
		return c;
	}
	return NULL;
}

void inodeFill(pfs_cache_t *ci, pfs_blockinfo *bi, u16 mode, u16 uid, u16 gid)
{
	u32 val;

	memset(ci->u.inode, 0, metaSize);

	ci->u.inode->magic=PFS_SEGD_MAGIC;

	ci->u.inode->inode_block.number=bi->number;
	ci->u.inode->inode_block.subpart=bi->subpart;
	ci->u.inode->inode_block.count=bi->count;

	ci->u.inode->last_segment.number=bi->number;
	ci->u.inode->last_segment.subpart=bi->subpart;
	ci->u.inode->last_segment.count=bi->count;

	ci->u.inode->mode=mode;
	ci->u.inode->uid=uid;
	ci->u.inode->gid=gid;

	if ((mode & FIO_S_IFMT) == FIO_S_IFDIR){
		ci->u.inode->attr=0xA0;
		ci->u.inode->size=sizeof(pfs_dentry);
		val=2;
	}else{
		ci->u.inode->size=0;
		val=1;
	}
	ci->u.inode->number_data=ci->u.inode->number_blocks=val;


	getPs2Time(&ci->u.inode->ctime);
	memcpy(&ci->u.inode->atime, &ci->u.inode->ctime, sizeof(pfs_datetime));
	memcpy(&ci->u.inode->mtime, &ci->u.inode->ctime, sizeof(pfs_datetime));

	ci->u.inode->number_segdesg=1;
	ci->u.inode->data[0].number =bi->number;
	ci->u.inode->data[0].subpart=bi->subpart;
	ci->u.inode->data[0].count  =bi->count;

	ci->u.inode->subpart=bi->subpart;

	ci->flags |= CACHE_FLAG_DIRTY;
}


// Given a path, this function will return the inode for the directory which holds
// the file (dir) specified by the filename.
//
// ie: if filename = /usr/local/ps2/games then it will return the inode for /usr/local/ps2
pfs_cache_t* inodeGetParent(pfs_mount_t *pfsMount, pfs_cache_t *clink, const char *filename,
								char *path, int *result)
{
	pfs_cache_t *link, *inode;
	char *filename2=(char*)filename;

	if (filename2[0]==0)
	{
		*result=-ENOENT;
		if (clink)	cacheAdd(clink);
		return NULL;
	}

	if (filename2[0] == '/')
	{

		// Get inode for root dir

		if (clink)	cacheAdd(clink);
		if ((clink=inodeGetData(pfsMount, pfsMount->root_dir.subpart,
					  pfsMount->root_dir.number, result))==0)
			return NULL;

//		dprintf("ps2fs: Got root dir inode!\n");
//		inodePrint(clink->u.inode);

	}
	else if (clink==NULL)
	{

		// Otherwise if relative path, get inode for current dir

		if ((clink=inodeGetData(pfsMount, pfsMount->current_dir.subpart,
					  pfsMount->current_dir.number, result))==0)
			return NULL;

//		dprintf("ps2fs: Got current dir inode!\n");
//		inodePrint(clink->u.inode);
	}

	do
	{
		filename2=splitPath(filename2, path, result);
		if (filename2==NULL)	return NULL;

		// If we've reached the end of the path, then we want to return
		// the cached inode for the directory which holds the file/dir in path
		if (filename2[0]==0)
		{
			if ((clink->u.inode->mode & FIO_S_IFMT) == FIO_S_IFDIR)
				return clink;

			cacheAdd(clink);
			*result=-ENOTDIR;	// not a directory
			return NULL;
		}

		inode=inodeGetFileInDir(clink, path, result);

		if (inode && ((inode->u.inode->mode & FIO_S_IFMT) == FIO_S_IFLNK))
		{
			if (symbolicLinks >= 4)
			{
				*result=-ELOOP;	// too many symbolic links
				cacheAdd(clink);
				cacheAdd(inode);
				return NULL;
			}

			symbolicLinks++;
			link=inodeGetFile(pfsMount, clink, (char*)&inode->u.inode->data[1], result);
			symbolicLinks--;

			clink=inode;
			if (link==0)
			{
				cacheAdd(inode);
				return NULL;
			}
			inode=link;
		}
		cacheAdd(clink);
		clink=inode;

	} while (inode);

	return NULL;
}

int inodeRemove(pfs_cache_t *parent, pfs_cache_t *inode, char *path)
{
	pfs_cache_t *entry;
	int rv=0;

	if((entry=dirRemoveEntry(parent, path))!=NULL)
	{
		inodeUpdateTime(parent);
		entry->flags|=CACHE_FLAG_DIRTY;
		cacheAdd(entry);
	}
	else
		rv=-ENOENT;

	cacheAdd(parent);
	if(rv==0)
	{
		inode->flags&=~CACHE_FLAG_DIRTY;
		bitmapFreeInodeBlocks(inode);
		if(parent->pfsMount->flags & FIO_ATTR_WRITEABLE)
			cacheFlushAllDirty(parent->pfsMount);
	}

	cacheAdd(inode);
	return rv;
}

pfs_cache_t *inodeCreateNewFile(pfs_cache_t *clink, u16 mode, u16 uid, u16 gid, int *result)
{
	pfs_blockinfo a, b;

	pfs_mount_t *pfsMount=clink->pfsMount;
	u32 j;
	u32 i;
	pfs_cache_t *inode;

	if ((mode & FIO_S_IFMT) == FIO_S_IFDIR)
	{
		if (pfsMount->num_subs > clink->u.inode->subpart)
			clink->u.inode->subpart++;
		else
			clink->u.inode->subpart=0;
		a.number =0;
		a.subpart=clink->u.inode->subpart;
		j= (pfsMount->zfree * (u64)100) / pfsMount->total_sector;
		i= (pfsMount->free_zone[a.subpart] * (u64)100) /
			(pfsMount->blockDev->getSize(pfsMount->fd, a.subpart) >> pfsMount->sector_scale);
		if ((i < j) && ((j-i) >= 11))
			a.subpart=getMaxthIndex(pfsMount);
	}else{
		a.number=clink->u.inode->inode_block.number;
		a.subpart=clink->u.inode->inode_block.subpart;
		a.count=clink->u.inode->inode_block.count;
	}
	a.count=1;

	// Search for a free zone, starting from parent dir inode block
	*result=searchFreeZone(pfsMount, &a, 2);
	if (*result<0)	return 0;
	inode=cacheGetData(pfsMount, a.subpart, a.number << pfsMount->inode_scale,
			CACHE_FLAG_SEGD | CACHE_FLAG_NOLOAD, result);
	if (inode == NULL)
		return NULL;

	// Initialise the inode (which has been allocate blocks specified by a)
	inodeFill(inode, (pfs_blockinfo*)&a, mode, uid, gid);
	if ((mode & FIO_S_IFMT) != FIO_S_IFDIR)
		return inode;

	b.number=a.number;
	b.subpart=a.subpart;
	b.count=a.count;

	*result=searchFreeZone(pfsMount, (pfs_blockinfo*)&a, 0);
	if (*result<0){
		cacheAdd(inode);
		bitmapFreeBlockSegment(pfsMount, (pfs_blockinfo*)&b);
		return NULL;
	}

	inode->u.inode->data[1].number=a.number;
	inode->u.inode->data[1].subpart=a.subpart;
	inode->u.inode->data[1].count =a.count;

	return inode;
}
