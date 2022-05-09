#ifndef ZSO_H
#define ZSO_H

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sysclib.h>

#define ZSO_MAGIC 0x4F53495A // ZISO

#define CISO_IDX_MAX_ENTRIES 4096

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
extern u32 ciso_idx_cache[CISO_IDX_MAX_ENTRIES];
extern int ciso_idx_start_block;

// header data that we need for the reader
extern u64 ciso_uncompressed_size;
extern u32 ciso_align;
extern u32 ciso_total_block;

// block buffers
extern u8 ciso_dec_buf[2048];
extern u8 ciso_com_buf[3072];

void ciso_decompressor(void *src, int src_len, void *dst, int dst_len, u32 topbit);
int ciso_read_sector(void *buf, u32 sector, unsigned int count);

// This must be implemented by isofs/cdvdman
extern int read_raw_data(u8 *addr, u32 size, u32 offset, u32 shift);

#endif
