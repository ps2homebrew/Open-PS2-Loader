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
# PFS directory parsing routines
*/

#include "pfs.h"

// Gets a dir entry from the inode specified by clink
pfs_cache_t *getDentry(pfs_cache_t *clink, char *path, pfs_dentry **dentry, u32 *size, int option)
{
	pfs_blockpos_t block_pos;
	u32	result;
	pfs_dentry *d;
	u16 aLen;
	pfs_dentry *d2;
	pfs_cache_t *dentCache;
	u32 dentryLen=0;
	int len=0;

	if (path)
	{
		len = strlen(path);
		dentryLen = (len+8+3) &~4;
	}
	*size = 0;

	block_pos.inode = cacheUsedAdd(clink);
	block_pos.block_segment = 1;
	block_pos.block_offset = 0;
	block_pos.byte_offset = 0;
	dentCache = getDentriesChunk(&block_pos, (int *)&result);

	if (dentCache)
	{
		d2=d=dentCache->u.dentry;
		while(*size < clink->u.inode->size)
		{
			// Read another dentry chunk if we need to
			if ((int)d2 >= ((int)dentCache->u.inode + metaSize))
			{
				if (inodeSync(&block_pos, metaSize, clink->u.inode->number_data))
					break;
				cacheAdd(dentCache);

				if ((dentCache=getDentriesChunk(&block_pos, (int *)&result))==0)
					break;
				d=dentCache->u.dentry;
			}

			for (d2=(pfs_dentry*)((int)d+512); d < d2; (int)d+=aLen)
			{
				aLen=(d->aLen & 0xFFF);

				if (aLen & 3){
					printf("ps2fs: Error: dir-entry allocated length/4 != 0\n");
					goto _exit;
				}
				if (aLen < ((d->pLen + 8 + 3) & 0x1FC)){
					printf("ps2fs: Error: dir-entry is too small\n");
					goto _exit;
				}
				if ((u32)d2 < ((u32)d + aLen)){
					printf("ps2fs: Error: dir-entry across sectors\n");
					goto _exit;
				}

				// decide if the current dentry meets the required criteria, based on 'option'
				switch(option)
				{
					case 0: // result = 1 when paths are equal
						result = (len==d->pLen) && (memcmp(path, d->path, d->pLen)==0);
						break;
					case 1: // hrm..
						result = ((d->inode) || (aLen < dentryLen)) ? ((aLen - ((d->pLen + 8 + 3) & 0x1FC))
								> dentryLen) : 1;
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
					cacheAdd(block_pos.inode);
					return dentCache;
				}
				*size+=aLen;
			}
		}
_exit:		cacheAdd(dentCache);
	}
	cacheAdd(block_pos.inode);
	return NULL;
}

pfs_cache_t *getDentriesChunk(pfs_blockpos_t *position, int *result)
{
	pfs_blockinfo *bi;
	pfs_mount_t *pfsMount=position->inode->pfsMount;

	bi = &position->inode->u.inode->data[fixIndex(position->block_segment)];

	return cacheGetData(pfsMount, bi->subpart,
		 ((bi->number + position->block_offset) << pfsMount->inode_scale) +
		 position->byte_offset / metaSize, CACHE_FLAG_NOTHING, result);
}

int getNextDentry(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 *position, char *name, pfs_blockinfo *bi)
{
	int result;
	pfs_cache_t *dcache;
	pfs_dentry *dentry;
	u32 len=0;

	// loop until a non-empty entry is found or until the end of the dentry chunk is reached
	while((len == 0) && (*position < clink->u.inode->size))
	{

		if (!(dcache=getDentriesChunk(blockpos, &result)))
		{
			dprintf("ps2fs: couldnt get dentries chunk for dread!\n");
			break;
		}

		dentry = (pfs_dentry*)((u32)dcache->u.data + (blockpos->byte_offset % metaSize));

		len = dentry->pLen;
		memcpy(name, dentry->path, len);
		name[len] = '\0';

		bi->subpart = dentry->sub;
		bi->number = dentry->inode;
		cacheAdd(dcache);

		*position += dentry->aLen & 0xFFF;

		// update blockpos
		if (inodeSync(blockpos, dentry->aLen & 0xFFF, clink->u.inode->number_data))
			break;
	}

	return len;
}

pfs_cache_t *getDentriesAtPos(pfs_cache_t *clink, u64 position, int *offset, int *result)
{
	pfs_blockpos_t blockpos;
	pfs_cache_t *r;
	*result=blockInitPos(clink, &blockpos, position);
	if (*result)	return 0;

	*offset=blockpos.byte_offset % metaSize;

	r=getDentriesChunk(&blockpos, result);
	cacheAdd(blockpos.inode);

	return r;
}

pfs_cache_t *fillInDentry(pfs_cache_t *clink, pfs_dentry *dentry, char *path1, pfs_blockinfo *bi, u32 len, u16 mode)
{
	dentry->inode=bi->number;
	dentry->sub=bi->subpart;
	dentry->pLen=strlen(path1);
	dentry->aLen=len | (mode & FIO_S_IFMT);
	memcpy(dentry->path, path1, dentry->pLen & 0xFF);

	return clink;
}

pfs_cache_t *dirAddEntry(pfs_cache_t *dir, char *filename, pfs_blockinfo *bi, u16 mode, int *result)
{
	pfs_dentry *dentry;
	u32 size;
	u32 len;
	pfs_cache_t *dcache;

	dcache=getDentry(dir, filename, &dentry, &size, 1);
	if (dcache){
		len=dentry->aLen & 0xFFF;
		if (dentry->pLen)
			len-=(dentry->pLen + 11) & 0x1FC;
		dentry->aLen=(dentry->aLen & FIO_S_IFMT) | ((dentry->aLen & 0xFFF) - len);
		(u32)dentry+=dentry->aLen & 0xFFF;
	}else{
		int offset;

		if ((*result=ioctl2Alloc(dir, sizeof(pfs_dentry), 0))<0)
			return NULL;
		dcache=getDentriesAtPos(dir, dir->u.inode->size, &offset, result);
		if (dcache==NULL)
			return NULL;

		dir->u.inode->size += sizeof(pfs_dentry);

		dentry=(pfs_dentry*)((u32)dcache->u.dentry+offset);
		len=sizeof(pfs_dentry);
	}
	return fillInDentry(dcache, dentry, filename, bi, len, mode);
}

pfs_cache_t *dirRemoveEntry(pfs_cache_t *clink, char *path)
{
	pfs_dentry *dentry;
	u32 size;
	int i=0, val;
	pfs_dentry *dlast=NULL, *dnext;
	pfs_cache_t *c;

	if ((c=getDentry(clink, path, &dentry, &size, 0))){
		val=(int)dentry-(int)c->u.dentry;
		if (val<0)	val +=511;
		val /=512;	val *=512;
		dnext=(pfs_dentry*)((int)c->u.dentry+val);
		do{
			if (dnext==dentry){
				if (dlast)
					dlast->aLen=(dlast->aLen & FIO_S_IFMT) | ((dlast->aLen & 0xFFF) + (dnext->aLen & 0xFFF));
				else{
					dnext->pLen=dnext->inode=0;

					if ((clink->u.inode->size) &&
					    (0>=dnext) && ((dnext==0) ||//strange?!?;
					    (size+(dnext->aLen & 0xFFF)>=clink->u.inode->size))) {
							clink->u.inode->size -= dnext->aLen & 0xFFF;
					}
				}
				return c;
			}
			i+=dnext->aLen & 0xFFF;
			dlast=dnext;
			(u32)dnext+=dnext->aLen & 0xFFF;
		}while (i<512);
	}
	return NULL;
}

int checkDirForFiles(pfs_cache_t *clink)
{
	pfs_dentry *dentry;
	pfs_cache_t *dcache;
	u32 size;

	if((dcache=getDentry(clink, NULL, &dentry, &size, 2))){
		cacheAdd(dcache);
		return 0;
	}
	return 1;
}

void fillSelfAndParentDentries(pfs_cache_t *clink, pfs_blockinfo *self, pfs_blockinfo *parent)
{
	pfs_dentry *dentry=clink->u.dentry;

	memset(dentry, 0, metaSize);
	dentry->inode=self->number;
	*(u32*)dentry->path='.';
	dentry->sub=self->subpart;
	dentry->pLen=1;
	dentry->aLen=12 | FIO_S_IFDIR;

	(u32)dentry+=12;

	dentry->inode=parent->number;
	*(u32*)dentry->path=('.'<<8) + '.';
	dentry->sub=parent->subpart;
	dentry->pLen=2;
	dentry->aLen=500 | FIO_S_IFDIR;
}

pfs_cache_t* setParent(pfs_cache_t *clink, pfs_blockinfo *bi, int *result)
{
	int offset;
	pfs_cache_t *dcache;

	dcache=getDentriesAtPos(clink, 0, &offset, result);
	if (dcache){
		pfs_dentry *d=(pfs_dentry*)(12+(u32)dcache->u.data);
		d->inode=bi->number;
		d->sub	=bi->subpart;
	}
	return dcache;
}
