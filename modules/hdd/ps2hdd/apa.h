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
*/

#ifndef _APA_H
#define _APA_H

///////////////////////////////////////////////////////////////////////////////
//   Function declerations

void apaSaveError(u32 device, void *buffer, u32 lba, u32 err_lba);
void setPartErrorSector(u32 device, u32 lba);
int getPartErrorSector(u32 device, u32 lba, int *lba_out);
int getPartErrorName(u32 device, char *name);

int apaCheckPartitionMax(u32 device, s32 size);
apa_cache *apaFillHeader(u32 device, input_param *params, int start, int next, int prev, int length, int *err);
apa_cache *apaInsertPartition(u32 device, input_param *params, u32 sector, int *err);
apa_cache *apaFindPartition(u32 device, char *id, int *err);
void addEmptyBlock(apa_header *header, u32 *EmptyBlocks);
apa_cache *apaAddPartitionHere(u32 device, input_param *params, u32 *EmptyBlocks, u32 sector, int *err);
int apaOpen(u32 device, hdd_file_slot_t *fileSlot, input_param *params, int mode);
int apaRemove(u32 device, char *id);
apa_cache *apaRemovePartition(u32 device, u32 start, u32 next, u32 prev, u32 length);
void apaMakeEmpty(apa_cache *clink);
apa_cache *apaDeleteFixPrev(apa_cache *clink1, int *err);
apa_cache *apaDeleteFixNext(apa_cache *clink, int *err);
int apaDelete(apa_cache *clink);
int apaCheckSum(apa_header *header);
int apaReadHeader(u32 device, apa_header *header, u32 lba);
int apaWriteHeader(u32 device, apa_header *header, u32 lba);
int apaGetFormat(u32 device, int *format);
int apaGetPartitionMax(int totalLBA);
apa_cache *apaGetNextHeader(apa_cache *clink, int *err);
//int apaGetFreeSectors(u32 device, u32 *free, hdd_hddDeviceBuf *deviceinfo);

#endif /* _APA_H */
