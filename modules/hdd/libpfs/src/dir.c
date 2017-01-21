/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS directory parsing routines
*/

#include <errno.h>
#include <stdio.h>
#include <sysclib.h>
#include <iomanX.h>

#include "pfs-opt.h"
#include "libpfs.h"

extern u32 pfsMetaSize;

// Gets a dir entry from the inode specified by clink
pfs_cache_t *pfsGetDentry(pfs_cache_t *clink, char *path, pfs_dentry_t **dentry, u32 *size, int option)
{
	pfs_blockpos_t block_pos;
	pfs_dentry_t *d;
	u16 aLen;
	pfs_dentry_t *d2;
	pfs_cache_t *dentCache;
	u32 new_dentryLen=0,dentryLen;
	int len=0, result;

	if (path)
	{
		len = strlen(path);
		new_dentryLen = (len+8+3) &~3;
	}
	*size = 0;

	block_pos.inode = pfsCacheUsedAdd(clink);
	block_pos.block_segment = 1;
	block_pos.block_offset = 0;
	block_pos.byte_offset = 0;
	dentCache = pfsGetDentriesChunk(&block_pos, &result);

	if (dentCache != NULL)
	{
		d2=d=dentCache->u.dentry;
		while(*size < clink->u.inode->size)
		{
			// Read another dentry chunk if we need to
			if (d2 >= (pfs_dentry_t*)((u8*)dentCache->u.inode + pfsMetaSize))
			{
				if (pfsInodeSync(&block_pos, pfsMetaSize, clink->u.inode->number_data))
					break;
				pfsCacheFree(dentCache);

				if ((dentCache=pfsGetDentriesChunk(&block_pos, &result))==0)
					break;
				d=dentCache->u.dentry;
			}

			for (d2=(pfs_dentry_t*)((int)d+512); d < d2; d=(pfs_dentry_t *)((int)d + aLen))
			{
				aLen=(d->aLen & 0xFFF);

				if (aLen & 3){
					PFS_PRINTF(PFS_DRV_NAME": Error: dir-entry allocated length/4 != 0\n");
					goto _exit;
				}
				dentryLen = (d->pLen + 8 + 3) & 0x1FC;
				if (aLen < dentryLen){
					PFS_PRINTF(PFS_DRV_NAME": Error: dir-entry is too small\n");
					goto _exit;
				}
				if (d2 < (pfs_dentry_t*)((u8*)d + aLen)){
					PFS_PRINTF(PFS_DRV_NAME": Error: dir-entry across sectors\n");
					goto _exit;
				}

				// decide if the current dentry meets the required criteria, based on 'option'
				switch(option)
				{
					case 0: // result = 1 when paths are equal
						result = (len==d->pLen) && (memcmp(path, d->path, d->pLen)==0);
						break;
					case 1:	// result = 1 when either there is no inode or if there is enough space remaining in the inode.
						result = ((d->inode) || (aLen < new_dentryLen)) ? (aLen - dentryLen
								>= new_dentryLen) : 1;
						break;
					case 2: // result = 1 when dir path is not empty, "." or ".."
						result = d->pLen && strcmp(d->path, ".") && strcmp(d->path, "..");
						break;
					default:
						goto _exit;
				}

				if (result)
				{
					*dentry=d;
					pfsCacheFree(block_pos.inode);
					return dentCache;
				}
				*size+=aLen;
			}
		}
_exit:		pfsCacheFree(dentCache);
	}
	pfsCacheFree(block_pos.inode);
	return NULL;
}

int pfsGetNextDentry(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 *position, char *name, pfs_blockinfo_t *bi)
{
	int result;
	pfs_cache_t *dcache;
	pfs_dentry_t *dentry;
	u32 len=0;

	// loop until a non-empty entry is found or until the end of the dentry chunk is reached
	while((len == 0) && (*position < clink->u.inode->size))
	{

		if (!(dcache=pfsGetDentriesChunk(blockpos, &result)))
		{
			PFS_PRINTF(PFS_DRV_NAME": couldnt get dentries chunk for dread!\n");
			break;
		}

		dentry = (pfs_dentry_t*)((u8*)dcache->u.data + (blockpos->byte_offset % pfsMetaSize));

		len = dentry->pLen;
		memcpy(name, dentry->path, len);
		name[len] = '\0';

		bi->subpart = dentry->sub;
		bi->number = dentry->inode;
		pfsCacheFree(dcache);

		*position += dentry->aLen & 0xFFF;

		// update blockpos
		if (pfsInodeSync(blockpos, dentry->aLen & 0xFFF, clink->u.inode->number_data))
			break;
	}

	return len;
}

pfs_cache_t *pfsFillDentry(pfs_cache_t *clink, pfs_dentry_t *dentry, char *path1, pfs_blockinfo_t *bi, u32 len, u16 mode)
{
	dentry->inode=bi->number;
	dentry->sub=(u8)bi->subpart;
	dentry->pLen=strlen(path1);
	dentry->aLen=len | (mode & FIO_S_IFMT);
	memcpy(dentry->path, path1, dentry->pLen & 0xFF);

	return clink;
}

pfs_cache_t *pfsDirAddEntry(pfs_cache_t *dir, char *filename, pfs_blockinfo_t *bi, u16 mode, int *result)
{
	pfs_dentry_t *dentry;
	u32 size;
	u32 len;
	pfs_cache_t *dcache;

	dcache=pfsGetDentry(dir, filename, &dentry, &size, 1);
	if (dcache){
		len=dentry->aLen & 0xFFF;
		if (dentry->pLen)
			len-=(dentry->pLen + 11) & 0x1FC;
		dentry->aLen=(dentry->aLen & FIO_S_IFMT) | ((dentry->aLen & 0xFFF) - len);
		dentry=(pfs_dentry_t *)((u8*)dentry + (dentry->aLen & 0xFFF));
	}else{
		int offset;

		if ((*result=pfsAllocZones(dir, sizeof(pfs_dentry_t), 0))<0)
			return NULL;
		dcache=pfsGetDentriesAtPos(dir, dir->u.inode->size, &offset, result);
		if (dcache==NULL)
			return NULL;

		dir->u.inode->size += sizeof(pfs_dentry_t);

		dentry=(pfs_dentry_t*)((u8*)dcache->u.dentry+offset);
		len=sizeof(pfs_dentry_t);
	}
	return pfsFillDentry(dcache, dentry, filename, bi, len, mode);
}

pfs_cache_t *pfsDirRemoveEntry(pfs_cache_t *clink, char *path)
{
	pfs_dentry_t *dentry;
	u32 size;
	u16 aLen;
	int i=0, val;
	pfs_dentry_t *dlast=NULL, *dnext;
	pfs_cache_t *c;

	if ((c=pfsGetDentry(clink, path, &dentry, &size, 0))){
		val=(int)dentry-(int)c->u.dentry;
		if (val<0)	val +=511;
		val /=512;	val *=512;
		dnext=(pfs_dentry_t*)((int)c->u.dentry+val);
		do{
			aLen = dnext->aLen & 0xFFF;

			if (dnext==dentry){
				if (dlast)
					dlast->aLen=(dlast->aLen & FIO_S_IFMT) | ((dlast->aLen & 0xFFF) + aLen);
				else{
					dnext->pLen=dnext->inode=0;

					if (size+aLen >= clink->u.inode->size)
							clink->u.inode->size -= aLen;
				}
				return c;
			}
			i+=aLen;
			dlast=dnext;
			dnext=(pfs_dentry_t *)((u8*)dnext + aLen);
		}while (i<512);
	}
	return NULL;
}

int pfsCheckDirForFiles(pfs_cache_t *clink)
{
	pfs_dentry_t *dentry;
	pfs_cache_t *dcache;
	u32 size;

	if((dcache=pfsGetDentry(clink, NULL, &dentry, &size, 2))){
		pfsCacheFree(dcache);
		return 0;
	}
	return 1;
}

void pfsFillSelfAndParentDentries(pfs_cache_t *clink, pfs_blockinfo_t *self, pfs_blockinfo_t *parent)
{
	pfs_dentry_t *dentry=clink->u.dentry;

	memset(dentry, 0, pfsMetaSize);
	dentry->inode=self->number;
	dentry->path[0]='.';
	dentry->path[1]='\0';
	dentry->sub=(u8)self->subpart;
	dentry->pLen=1;
	dentry->aLen=12 | FIO_S_IFDIR;

	dentry=(pfs_dentry_t *)((u8*)dentry + 12);

	dentry->inode=parent->number;
	dentry->path[0]='.';
	dentry->path[1]='.';
	dentry->path[2]='\0';
	dentry->sub=(u8)parent->subpart;
	dentry->pLen=2;
	dentry->aLen=500 | FIO_S_IFDIR;
}

pfs_cache_t* pfsSetDentryParent(pfs_cache_t *clink, pfs_blockinfo_t *bi, int *result)
{
	int offset;
	pfs_cache_t *dcache;

	dcache=pfsGetDentriesAtPos(clink, 0, &offset, result);
	if (dcache){
		pfs_dentry_t *d=(pfs_dentry_t*)(12+(u8*)dcache->u.data);
		d->inode=bi->number;
		d->sub	=(u8)bi->subpart;
	}
	return dcache;
}

// Returns the cached inode for the file (dir) in the directory pointed
// to by the dir inode.
pfs_cache_t *pfsInodeGetFileInDir(pfs_cache_t *dirInode, char *path, int *result)
{
	pfs_dentry_t *dentry;
	u32	size;
	pfs_cache_t *clink;

	if (path[0]==0)
		return pfsCacheUsedAdd(dirInode);

	// If we're in the root dir, don't do anything for ".."
	if ((dirInode->sector ==
	     dirInode->pfsMount->root_dir.number << dirInode->pfsMount->inode_scale) &&
	    (dirInode->sub    == dirInode->pfsMount->root_dir.subpart) &&
	    (strcmp(path, "..") == 0))
		return pfsCacheUsedAdd(dirInode);

	if ((*result=pfsCheckAccess(dirInode, 1)) < 0)
		return NULL;

	// Get dentry of file/dir specified by path from the dir pointed to
	// by the inode (dirInode). Then return the cached inode for that dentry.
	if ((clink=pfsGetDentry(dirInode, path, &dentry, &size, 0))){
		pfsCacheFree(clink);
		return pfsInodeGetData(dirInode->pfsMount,
			dentry->sub, dentry->inode, result);
	}

	*result=-ENOENT;
	return NULL;
}

pfs_cache_t *pfsInodeGetFile(pfs_mount_t *pfsMount, pfs_cache_t *clink,
			const char *name, int *result) {
	char path[256];
	pfs_cache_t *c;

	c=pfsInodeGetParent(pfsMount, clink, name, path, result);
	if (c){
		c=pfsInodeGetFileInDir(c, path, result);
		pfsCacheFree(c);
		return c;
	}
	return NULL;
}

void pfsInodeFill(pfs_cache_t *ci, pfs_blockinfo_t *bi, u16 mode, u16 uid, u16 gid)
{
	u32 val;

	memset(ci->u.inode, 0, pfsMetaSize);

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
		ci->u.inode->size=sizeof(pfs_dentry_t);
		val=2;
	}else{
		ci->u.inode->size=0;
		val=1;
	}
	ci->u.inode->number_data=ci->u.inode->number_blocks=val;


	pfsGetTime(&ci->u.inode->ctime);
	memcpy(&ci->u.inode->atime, &ci->u.inode->ctime, sizeof(pfs_datetime_t));
	memcpy(&ci->u.inode->mtime, &ci->u.inode->ctime, sizeof(pfs_datetime_t));

	ci->u.inode->number_segdesg=1;
	ci->u.inode->data[0].number =bi->number;
	ci->u.inode->data[0].subpart=bi->subpart;
	ci->u.inode->data[0].count  =bi->count;

	ci->u.inode->subpart=bi->subpart;

	ci->flags |= PFS_CACHE_FLAG_DIRTY;
}


// Given a path, this function will return the inode for the directory which holds
// the file (dir) specified by the filename.
//
// ie: if filename = /usr/local/ps2/games then it will return the inode for /usr/local/ps2
pfs_cache_t* pfsInodeGetParent(pfs_mount_t *pfsMount, pfs_cache_t *clink, const char *filename,
								char *path, int *result)
{
	static int pfsSymbolicLinks = 0;
	pfs_cache_t *link, *inode;
	char *filename2=(char*)filename;

	if (filename2[0]==0)
	{
		*result=-ENOENT;
		if (clink)	pfsCacheFree(clink);
		return NULL;
	}

	if (filename2[0] == '/')
	{

		// Get inode for root dir

		if (clink)	pfsCacheFree(clink);
		if ((clink=pfsInodeGetData(pfsMount, pfsMount->root_dir.subpart,
					  pfsMount->root_dir.number, result))==0)
			return NULL;
	}
	else if (clink==NULL)
	{

		// Otherwise if relative path, get inode for current dir

		if ((clink=pfsInodeGetData(pfsMount, pfsMount->current_dir.subpart,
					  pfsMount->current_dir.number, result))==0)
			return NULL;
	}

	do
	{
		filename2=pfsSplitPath(filename2, path, result);
		if (filename2==NULL)	return NULL;

		// If we've reached the end of the path, then we want to return
		// the cached inode for the directory which holds the file/dir in path
		if (filename2[0]==0)
		{
			if ((clink->u.inode->mode & FIO_S_IFMT) == FIO_S_IFDIR)
				return clink;

			pfsCacheFree(clink);
			*result=-ENOTDIR;	// not a directory
			return NULL;
		}

		inode=pfsInodeGetFileInDir(clink, path, result);

		if (inode && ((inode->u.inode->mode & FIO_S_IFMT) == FIO_S_IFLNK))
		{
			if (pfsSymbolicLinks >= 4)
			{
				*result=-ELOOP;	// too many symbolic links
				pfsCacheFree(clink);
				pfsCacheFree(inode);
				return NULL;
			}

			pfsSymbolicLinks++;
			link=pfsInodeGetFile(pfsMount, clink, (char*)&inode->u.inode->data[1], result);
			pfsSymbolicLinks--;

			clink=inode;
			if (link==0)
			{
				pfsCacheFree(inode);
				return NULL;
			}
			inode=link;
		}
		pfsCacheFree(clink);
		clink=inode;

	} while (inode);

	return NULL;
}

int pfsInodeRemove(pfs_cache_t *parent, pfs_cache_t *inode, char *path)
{
	pfs_cache_t *entry;
	int rv=0;

	if((entry=pfsDirRemoveEntry(parent, path))!=NULL)
	{
		pfsInodeSetTime(parent);
		entry->flags|=PFS_CACHE_FLAG_DIRTY;
		pfsCacheFree(entry);
	}
	else
		rv=-ENOENT;

	pfsCacheFree(parent);
	if(rv==0)
	{
		inode->flags&=~PFS_CACHE_FLAG_DIRTY;
		pfsBitmapFreeInodeBlocks(inode);
		if(parent->pfsMount->flags & PFS_FIO_ATTR_WRITEABLE)
			pfsCacheFlushAllDirty(parent->pfsMount);
	}

	pfsCacheFree(inode);
	return rv;
}

pfs_cache_t *pfsInodeCreate(pfs_cache_t *clink, u16 mode, u16 uid, u16 gid, int *result)
{
	pfs_blockinfo_t a, b;

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
			a.subpart=pfsGetMaxIndex(pfsMount);
	}else{
		a.number=clink->u.inode->inode_block.number;
		a.subpart=clink->u.inode->inode_block.subpart;
		a.count=clink->u.inode->inode_block.count;
	}
	a.count=1;

	// Search for a free zone, starting from parent dir inode block
	*result=pfsBitmapSearchFreeZone(pfsMount, &a, 2);
	if (*result<0)	return 0;
	inode=pfsCacheGetData(pfsMount, a.subpart, a.number << pfsMount->inode_scale,
			PFS_CACHE_FLAG_SEGD | PFS_CACHE_FLAG_NOLOAD, result);
	if (inode == NULL)
		return NULL;

	// Initialise the inode (which has been allocate blocks specified by a)
	pfsInodeFill(inode, (pfs_blockinfo_t*)&a, mode, uid, gid);
	if ((mode & FIO_S_IFMT) != FIO_S_IFDIR)
		return inode;

	b.number=a.number;
	b.subpart=a.subpart;
	b.count=a.count;

	*result=pfsBitmapSearchFreeZone(pfsMount, (pfs_blockinfo_t*)&a, 0);
	if (*result<0){
		pfsCacheFree(inode);
		pfsBitmapFreeBlockSegment(pfsMount, (pfs_blockinfo_t*)&b);
		return NULL;
	}

	inode->u.inode->data[1].number=a.number;
	inode->u.inode->data[1].subpart=a.subpart;
	inode->u.inode->data[1].count =a.count;

	return inode;
}

int pfsCheckAccess(pfs_cache_t *clink, int flags)
{
	int mode;

	// Bail if trying to write to read-only mount
	if ((clink->pfsMount->flags & FIO_MT_RDONLY) && (flags & O_WRONLY))
		return -EROFS;

	if (((clink->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR) &&
		((clink->u.inode->mode & 0111) == 0))
		mode=6;
	else
		mode=7;
	return ((mode & flags) & 7) == flags ? 0 : -EACCES;
}

char* pfsSplitPath(char *filename, char *path, int *result)
{
	int i=0;
	int j=0;

	for (i=0; filename[i] == '/'; i++)
		;

	for (; i<1024 && filename[i] != '/'; i++){
		if (filename[i] == 0)	break;
		if (j < 255)
			path[j++] = filename[i];
	}

	if (j<256)
		path[j]=0;

	while (filename[i] == '/')
		if (i<1024)
			i++;
		else
			break;
	if (i<1024)
		return &filename[i];

	*result=-ENAMETOOLONG;
	return 0;
}

u16 pfsGetMaxIndex(pfs_mount_t *pfsMount)
{
	u32 max=0, maxI=0, i, v;

	for (i=0; i < pfsMount->num_subs + 1; i++)  //enumerate all subs
	{
		v =	(pfsMount->free_zone[i] * (u64)100) /
			(pfsMount->blockDev->getSize(pfsMount->fd, i) >> pfsMount->sector_scale);
		if (max < v)
		{
			max=v;
			maxI=i;
		}
	}
	return maxI;
}

int pfsAllocZones(pfs_cache_t *clink, int msize, int mode)
{
	pfs_blockpos_t blockpos;
	int result=0;
	u32 val;
	int zsize;

	zsize=clink->pfsMount->zsize;
	val=((msize-1 + zsize) & (-zsize)) / zsize;

	if(mode==0)
		if (((clink->u.inode->number_blocks-clink->u.inode->number_segdesg) *(u64)zsize)
			>= (clink->u.inode->size + msize))
			return 0;

	if((blockpos.inode = pfsBlockGetLastSegmentDescriptorInode(clink, &result)))
	{
		blockpos.block_offset=blockpos.byte_offset=0;
		blockpos.block_segment=clink->u.inode->number_data-1;
		val-=pfsBlockExpandSegment(clink, &blockpos, val);
		while (val && ((result=pfsBlockAllocNewSegment(clink, &blockpos, val))>=0)){
			val-=result;
			result=0;
		}
		pfsCacheFree(blockpos.inode);
	}
	return result;
}

void pfsFreeZones(pfs_cache_t *pfree)
{
	pfs_blockinfo_t b;
	int result;
	pfs_mount_t *pfsMount = pfree->pfsMount;
	pfs_inode_t *inode = pfree->u.inode;
	u32 nextsegdesc = 1, limit = inode->number_data, i, j = 0, zones;
	pfs_blockinfo_t *bi;
	pfs_cache_t *clink;

	zones = (u32)(inode->size / pfsMount->zsize);
	if(inode->size % pfsMount->zsize)
		zones++;
	if(inode->number_segdesg + zones == inode->number_blocks)
		return;

	j = zones;
	b.number = 0;
	clink = pfsCacheUsedAdd(pfree);

	// iterate through each of the block segments used by the inode
	for (i = 1; i < limit && j; i++)
	{
		if(pfsFixIndex(i) == 0)
		{
			if ((clink = pfsBlockGetNextSegment(clink, &result)) == 0)
				return;

			nextsegdesc++;
		}
		else
			if(j < clink->u.inode->data[pfsFixIndex(i)].count)
			{
				clink->u.inode->data[pfsFixIndex(i)].count -= j;
				b.subpart = clink->u.inode->data[pfsFixIndex(i)].subpart;
				b.count = j;
				b.number = clink->u.inode->data[pfsFixIndex(i)].number +
					clink->u.inode->data[pfsFixIndex(i)].count;
				j = 0;
				clink->flags |= PFS_CACHE_FLAG_DIRTY;
			}
			else
				j -= clink->u.inode->data[pfsFixIndex(i)].count;
	}

	pfree->u.inode->number_data = i;
	pfree->u.inode->number_blocks = zones + nextsegdesc;
	pfree->u.inode->number_segdesg = nextsegdesc;
	pfree->u.inode->last_segment.number = clink->u.inode->data[0].number;
	pfree->u.inode->last_segment.subpart= clink->u.inode->data[0].subpart;
	pfree->u.inode->last_segment.count  = clink->u.inode->data[0].count;
	pfree->flags |= PFS_CACHE_FLAG_DIRTY;

	if (b.number)
		pfsBitmapFreeBlockSegment(pfsMount, &b);

	while(i < limit)
	{
		if (pfsFixIndex(i) == 0)
		{
			if((clink = pfsBlockGetNextSegment(clink, &result)) == 0)
				return;
		}
		bi = &clink->u.inode->data[pfsFixIndex(i++)];
		pfsBitmapFreeBlockSegment(pfsMount, bi);
		pfsCacheMarkClean(pfsMount, bi->subpart, bi->number<<pfsMount->inode_scale,
			(bi->number+bi->count)<<pfsMount->inode_scale);
	}

	pfsCacheFree(clink);
}
