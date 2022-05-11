#ifndef ZSO_H
#define ZSO_H

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sysclib.h>

#define ZSO_MAGIC 0x4F53495A // ZISO

#define CISO_IDX_MAX_ENTRIES 256

#define MIN(x, y) ((x < y) ? x : y)

// CSO Header (same for ZSO)
typedef struct
{
    u32 magic;       // 0
    u32 header_size; // 4
    u64 total_bytes; // 8
    u32 block_size;  // 16
    u8 ver;          // 20
    u8 align;        // 21
    u8 rsv_06[2];    // 22
} CISO_header;


// block offset cache, reduces IO access
extern u32* ciso_idx_cache;
extern int ciso_idx_start_block;

// header data that we need for the reader
extern u32 ciso_align;
extern u32 ciso_total_block;

// block buffers
extern u8* ciso_com_buf;

void initZSO(CISO_header* header, u32 first_block);
int ciso_read_sector(void *buf, u32 sector, unsigned int count);

// This must be implemented by isofs/cdvdman
extern int read_raw_data(u8 *addr, u32 size, u32 offset, u32 shift);

#endif
