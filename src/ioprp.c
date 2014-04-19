#include <kernel.h>
#include <string.h>

#include "include/ioprp.h"

struct romdir_entry
{
	char fileName[10];
	unsigned short int extinfo_size;
	unsigned int fileSize;
};

/* Pointers externally compiled modules */
extern unsigned char cdvdfsv_irx[];
extern unsigned int size_cdvdfsv_irx;

extern unsigned char eesync_irx[];
extern unsigned int size_eesync_irx;

extern unsigned char IOPRP_img[];
extern unsigned int size_IOPRP_img;

/* Local function prototypes */
static inline void patch_CDVDMAN(void *image_offset, struct romdir_entry *entryinfo, void *cdvdman, unsigned int size);
static inline void patch_CDVDFSV(void *image_offset, struct romdir_entry *entryinfo);
static inline void patch_EESYNC(void *image_offset, struct romdir_entry *entryinfo);
static inline void Align_offsets(void* base_address,unsigned int* offset_in,struct romdir_entry *romdir_in,unsigned int* offset_out,struct romdir_entry *romdir_out);

/*----------------------------------------------------------------------------------------*/
/* Replace modules in a IOPRP image.                                                      */
/*----------------------------------------------------------------------------------------*/
inline unsigned int patch_IOPRP_image(void *ioprp_image, void *cdvdman_module, unsigned int size_cdvdman)
{
	unsigned int offset_in,offset_out; /* For processing purposes */
	struct romdir_entry *romdir_in, *romdir_out;

	offset_in =0;
	offset_out=0;

	romdir_in=(struct romdir_entry*)IOPRP_img;
	romdir_out=(struct romdir_entry*)ioprp_image;

	while(romdir_in->fileName[0]!='\0'){
		memset(romdir_out, 0, sizeof(struct romdir_entry));

		if(!strcmp(romdir_in->fileName, "CDVDMAN")) patch_CDVDMAN((void*)(ioprp_image+offset_out), romdir_out, cdvdman_module, size_cdvdman);
		else if(!strcmp(romdir_in->fileName, "CDVDFSV")) patch_CDVDFSV((void*)(ioprp_image+offset_out), romdir_out);
		else if(!strcmp(romdir_in->fileName, "EESYNC")) patch_EESYNC((void*)(ioprp_image+offset_out), romdir_out);
		else{ /* Other modules that should not be tampered with */
			memcpy((void*)(ioprp_image+offset_out), (void *)(IOPRP_img+offset_in), romdir_in->fileSize); /* Copy the file/entry's contents. */
			romdir_out->fileSize=romdir_in->fileSize; /* Copy the file size. */
		}

		/* For ALL modules */
		strcpy(romdir_out->fileName, romdir_in->fileName); /* Copy module name */
		romdir_out->extinfo_size=romdir_in->extinfo_size; /* Copy EXTINFO size */
		Align_offsets(ioprp_image, &offset_in, romdir_in, &offset_out, romdir_out); /* Align all addresses to a multiple of 16 */

		romdir_in++;
		romdir_out++;
	}

	return offset_out;
}

/*----------------------------------------------------------------------------------------*/
/* Patch CDVDMAN in an IOPRP image                                                        */
/*----------------------------------------------------------------------------------------*/
static inline void patch_CDVDMAN(void *image_offset, struct romdir_entry *entryinfo, void *cdvdman, unsigned int size)
{
	memcpy(image_offset, cdvdman, size);
	entryinfo->fileSize=size;
}

/*----------------------------------------------------------------------------------------*/
/* Patch CDVDFSV in an IOPRP image                                                        */
/*----------------------------------------------------------------------------------------*/
static inline void patch_CDVDFSV(void *image_offset, struct romdir_entry *entryinfo)
{
	memcpy(image_offset, cdvdfsv_irx, size_cdvdfsv_irx);
	entryinfo->fileSize=size_cdvdfsv_irx;
}

/*----------------------------------------------------------------------------------------*/
/* Patch EESYNC in an IOPRP image                                                         */
/*----------------------------------------------------------------------------------------*/
static inline void patch_EESYNC(void *image_offset, struct romdir_entry *entryinfo)
{
	memcpy(image_offset, eesync_irx, size_eesync_irx);
	entryinfo->fileSize=size_eesync_irx;
}

/*----------------------------------------------------------------------------------------*/
/* Align offsets to multiples of 16, filling the gaps created with 0s.                    */
/*----------------------------------------------------------------------------------------*/
static inline void Align_offsets(void* base_address,unsigned int* offset_in,struct romdir_entry *romdir_in,unsigned int* offset_out,struct romdir_entry *romdir_out)
{
	/* For ALL modules; Align all addresses to a multiple of 16 */
	if(offset_in!=NULL){
		if ((romdir_in->fileSize&0xF)==0) *offset_in+=romdir_in->fileSize;
		else *offset_in+=(romdir_in->fileSize+0xF)&~0xF;
	}

	if(offset_out!=NULL){
		if ((romdir_out->fileSize&0xF)==0) *offset_out+=romdir_out->fileSize;
		else{ /* Fill the alignment gap with 0s */
			register unsigned int new_filesize=(romdir_out->fileSize+0xF)&~0xF;
			memset((void*)((u32)base_address+(*offset_out)+romdir_out->fileSize),0,new_filesize-romdir_out->fileSize);
			*offset_out+=new_filesize;
		}
	}
}
