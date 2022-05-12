#include "lz4.h"
#include "zso.h"

// block offset cache, reduces IO access
u32* ciso_idx_cache = NULL;
int ciso_idx_start_block = -1;

// header data that we need for the reader
u32 ciso_align;
u32 ciso_total_block;

// block buffers
u8* ciso_com_buf = NULL;

void initZSO(CISO_header* header, u32 first_block){
    // read header information
    ciso_align = header->align;
    ciso_idx_start_block = -1;
    // calculate number of blocks without using uncompressed_size (avoid 64bit division)
    ciso_total_block = ((((first_block & 0x7FFFFFFF) << ciso_align) - sizeof(CISO_header)) / 4) - 1;
    // allocate memory
    if (ciso_com_buf == NULL){
        ciso_com_buf = ciso_alloc(2048 + sizeof(u32)*CISO_IDX_MAX_ENTRIES + 64);
        if((u32)ciso_com_buf & 63) // align 64
            ciso_com_buf = (void*)(((u32)ciso_com_buf & (~63)) + 64);
        if (ciso_com_buf){
            ciso_idx_cache = ciso_com_buf + 2048;
        }
    }
}

/*
  The meat of the compressed sector reader.
  Taken from ARK-4's Inferno 2 ISO Driver.
  Tailored for OPL.
  While it should let you read pretty much any format, it's made to work with 2K blocks ZSO for a lightweight implementation.
*/
int ciso_read_sector(u8* addr, u32 lsn, unsigned int count)
{
    u32 size = count * 2048;
    u32 o_lsn = lsn;
    u8 *c_buf = NULL;

    {
        // refresh index table if needed
        u32 starting_block = lsn;
        u32 ending_block = lsn + count + 1;
        if (ciso_idx_start_block < 0 || starting_block < ciso_idx_start_block || ending_block >= ciso_idx_start_block + CISO_IDX_MAX_ENTRIES) {
            read_cso_data(ciso_idx_cache, CISO_IDX_MAX_ENTRIES * sizeof(u32), starting_block * 4 + sizeof(CISO_header), 0);
            ciso_idx_start_block = starting_block;
        }
        
        // reduce IO by doing one read of all compressed data into the end of the provided buffer
        u32 o_start = (ciso_idx_cache[starting_block - ciso_idx_start_block] & 0x7FFFFFFF);
        u32 o_end;
        if (ending_block < ciso_idx_start_block + CISO_IDX_MAX_ENTRIES) {
            o_end = ciso_idx_cache[ending_block - ciso_idx_start_block];
        }
        else{
            read_cso_data(&o_end, sizeof(u32), starting_block * 4 + sizeof(CISO_header), 0);
        }
        o_end &= 0x7FFFFFFF;
        u32 compressed_size = (o_end - o_start) << ciso_align;
        if (size >= compressed_size) {
            c_buf = addr + size - compressed_size;
            read_cso_data(c_buf, compressed_size, o_start, ciso_align);
        }
    }
    
    while (size > 0) {

        if (lsn >= ciso_total_block) {
            // EOF reached
            break;
        }

        if (lsn >= ciso_idx_start_block + CISO_IDX_MAX_ENTRIES) {
            // refresh index cache
            read_cso_data(ciso_idx_cache, CISO_IDX_MAX_ENTRIES * sizeof(u32), lsn * 4 + sizeof(CISO_header), 0);
            ciso_idx_start_block = lsn;
        }

        // read compressed block offset and size
        u32 b_offset = ciso_idx_cache[lsn - ciso_idx_start_block];
        u32 b_size = ciso_idx_cache[lsn - ciso_idx_start_block + 1];
        u32 topbit = b_offset & 0x80000000; // extract top bit for decompressor
        b_offset = (b_offset & 0x7FFFFFFF);
        b_size = (b_size & 0x7FFFFFFF);
        b_size = (b_size - b_offset) << ciso_align;

        int r = MIN(b_size, 2048);

        // read block
        if (c_buf > addr) { // fast read
            memcpy(addr, c_buf, r);
            c_buf += b_size;
        } else { // slow read
            r = read_cso_data(addr, r, b_offset, ciso_align);
        }

        // decompress block
        if (topbit == 0){
            memcpy(ciso_com_buf, addr, r); // read compressed block into temp buffer
            LZ4_decompress_fast(ciso_com_buf, addr, 2048);
        }

        size -= 2048;
        addr += 2048;
        lsn++;
    }
    return lsn-o_lsn;
}
