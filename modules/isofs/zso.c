#include "lz4.h"
#include "zso.h"

// block offset cache, reduces IO access
u32 ciso_idx_cache[CISO_IDX_MAX_ENTRIES];
int ciso_idx_start_block = -1;

// header data that we need for the reader
u32 ciso_header_size;
u32 ciso_block_size;
u64 ciso_uncompressed_size;
u32 ciso_block_header;
u32 ciso_align;
u32 ciso_total_block;

// block buffers
u8 ciso_dec_buf[2048] __attribute__((aligned(64)));
u8 ciso_com_buf[3072] __attribute__((aligned(64)));

/*
  Decompressor wrapper function.
*/ 
void ciso_decompressor(void* src, int src_len, void* dst, int dst_len, u32 topbit){
    if (topbit) memcpy(dst, src, dst_len); // check for NC area
    else LZ4_decompress_fast(src, dst, dst_len);
}

/*
  The meat of the compressed sector reader.
  Taken from ARK-4's Inferno 2 ISO Driver.
  Tailored for OPL.
  While it should let you read pretty much any format, it's made to work with 2K blocks ZSO for a lightweight implementation.
*/
int ciso_read_sector(void* addr, u32 lsn, unsigned int count){
    u32 size = count*2048;
    u32 cur_block;
    u32 o_lsn = lsn;
    u8* com_buf = ciso_com_buf;
    u8* dec_buf = ciso_dec_buf;
    u8* c_buf = NULL;
    u32 i = 0;
    
    // refresh index table if needed
    u32 starting_block = lsn;
    u32 ending_block = lsn+count+1;
    if (ciso_idx_start_block < 0 || starting_block < ciso_idx_start_block || ending_block >= ciso_idx_start_block + CISO_IDX_MAX_ENTRIES){
        read_raw_data(ciso_idx_cache, CISO_IDX_MAX_ENTRIES*sizeof(u32), starting_block * 4 + ciso_header_size, 0);
        ciso_idx_start_block = starting_block;
    }
    if (ending_block < ciso_idx_start_block + CISO_IDX_MAX_ENTRIES){
        // reduce IO by doing one read of all compressed data into the end of the provided buffer
        u32 o_start = (ciso_idx_cache[starting_block-ciso_idx_start_block]&0x7FFFFFFF);
        u32 o_end = (ciso_idx_cache[ending_block-ciso_idx_start_block]&0x7FFFFFFF);
        u32 compressed_size = (o_end - o_start)<<ciso_align;
        if (size >= compressed_size){
            c_buf = (void*)((u32)addr + size - compressed_size);
            read_raw_data(c_buf, compressed_size, o_start, ciso_align);
        }
    }
    while(size > 0) {
        // calculate block number and offset within block
        cur_block = lsn;

        if(cur_block >= ciso_total_block) {
            // EOF reached
            break;
        }
        
        if (cur_block>=ciso_idx_start_block+CISO_IDX_MAX_ENTRIES){
            // refresh index cache
            read_raw_data(ciso_idx_cache, CISO_IDX_MAX_ENTRIES*sizeof(u32), cur_block * 4 + ciso_header_size, 0);
            ciso_idx_start_block = cur_block;
        }
        
        // read compressed block offset and size
        u32 b_offset = ciso_idx_cache[cur_block-ciso_idx_start_block];
        u32 b_size = ciso_idx_cache[cur_block-ciso_idx_start_block+1];
        u32 topbit = b_offset&0x80000000; // extract top bit for decompressor
        b_offset = (b_offset&0x7FFFFFFF) ;
        b_size = (b_size&0x7FFFFFFF);
        b_size = (b_size-b_offset) << ciso_align;

        // read block, skipping header if needed
        if (c_buf > addr){
            memcpy(com_buf, c_buf+ciso_block_header, b_size); // fast read
            c_buf += b_size;
        }
        else{ // slow read
            b_size = read_raw_data(com_buf, b_size, b_offset + ciso_block_header, ciso_align);
        }

        // decompress block
        ciso_decompressor(com_buf, b_size, dec_buf, ciso_block_size, topbit);
    
        // read data from block into buffer
        memcpy(addr, dec_buf, 2048);
        size -= 2048;
        addr += 2048;
        lsn++;
        i++;
    }
    return i;
}