#include "lz4.h"
#include "zso.h"

// block offset cache, reduces IO access
u32 *ziso_idx_cache = NULL;
int ziso_idx_start_block = -1;

// header data that we need for the reader
u32 ziso_align;
u32 ziso_total_block;

// block buffers
u8 *ziso_tmp_buf = NULL;

void ziso_init(ZISO_header *header, u32 first_block)
{
    // read header information
    ziso_align = header->align;
    ziso_idx_start_block = -1;
    // calculate number of blocks without using uncompressed_size (avoid 64bit division)
    ziso_total_block = ((((first_block & 0x7FFFFFFF) << ziso_align) - sizeof(ZISO_header)) / 4) - 1;
    // allocate memory
    if (ziso_tmp_buf == NULL) {
        ziso_tmp_buf = ziso_alloc(2048 + sizeof(u32) * ZISO_IDX_MAX_ENTRIES + 64);
        if ((u32)ziso_tmp_buf & 63) // align 64
            ziso_tmp_buf = (void *)(((u32)ziso_tmp_buf & (~63)) + 64);
        if (ziso_tmp_buf) {
            ziso_idx_cache = ziso_tmp_buf + 2048;
        }
    }
}

/*
  The meat of the compressed sector reader.
  Taken from ARK-4's Inferno 2 ISO Driver.
  Tailored for OPL.
  While it should let you read pretty much any format, it's made to work with 2K blocks ZSO for a lightweight implementation.
*/
int ziso_read_sector(u8 *addr, u32 lsn, unsigned int count)
{

    if (lsn >= ziso_total_block) {
        return 0; // can't seek beyond file
    }

    u32 size = count * 2048;
    u32 o_lsn = lsn;
    u8 *c_buf = NULL;

    {
        // refresh index table if needed
        u32 starting_block = lsn;
        u32 ending_block = lsn + count + 1;
        if (ziso_idx_start_block < 0 || starting_block < ziso_idx_start_block || ending_block >= ziso_idx_start_block + ZISO_IDX_MAX_ENTRIES) {
            read_raw_data(ziso_idx_cache, ZISO_IDX_MAX_ENTRIES * sizeof(u32), starting_block * 4 + sizeof(ZISO_header), 0);
            ziso_idx_start_block = starting_block;
        }

        // reduce IO by doing one read of all compressed data into the end of the provided buffer
        u32 o_start = (ziso_idx_cache[starting_block - ziso_idx_start_block] & 0x7FFFFFFF);
        u32 o_end;
        if (ending_block < ziso_idx_start_block + ZISO_IDX_MAX_ENTRIES) { // ending block offset within cache
            o_end = ziso_idx_cache[ending_block - ziso_idx_start_block];
        } else { // need to read ending block from file
            read_raw_data(&o_end, sizeof(u32), starting_block * 4 + sizeof(ZISO_header), 0);
        }
        o_end &= 0x7FFFFFFF;
        // calculate size of compressed data and if we can copy it entirely into the provided buffer
        u32 compressed_size = (o_end - o_start) << ziso_align;
        if (size >= compressed_size) {
            c_buf = addr + size - compressed_size;                      // copy at the end to avoid overlap
            read_raw_data(c_buf, compressed_size, o_start, ziso_align); // read all compressed data at once
        }
    }

    // process each sector
    while (size > 0) {

        if (lsn >= ziso_total_block) {
            // EOF reached
            break;
        }

        if (lsn >= ziso_idx_start_block + ZISO_IDX_MAX_ENTRIES) {
            // refresh index cache
            read_raw_data(ziso_idx_cache, ZISO_IDX_MAX_ENTRIES * sizeof(u32), lsn * 4 + sizeof(ZISO_header), 0);
            ziso_idx_start_block = lsn;
        }

        // read compressed block offset and size
        u32 b_offset = ziso_idx_cache[lsn - ziso_idx_start_block];
        u32 b_size = ziso_idx_cache[lsn - ziso_idx_start_block + 1];
        u32 topbit = b_offset & 0x80000000; // extract top bit for decompressor
        b_offset = (b_offset & 0x7FFFFFFF); // remove top bit
        b_size = (b_size & 0x7FFFFFFF);     // remove top bit
        b_size = (b_size - b_offset) << ziso_align;

        // prevent reading more than a sector (eliminates padding if any)
        int r = MIN(b_size, 2048);

        // read block
        if (c_buf > addr) {         // fast read
            memcpy(addr, c_buf, r); // move compressed block to its correct position in the buffer
            c_buf += b_size;
        } else { // slow read, happens on rare cases where decompressed data overlaps compressed data
            r = read_raw_data(addr, r, b_offset, ziso_align);
        }

        // if the block is not compressed, it will already be in its correct place
        if (topbit == 0) {                                 // block is compressed
            memcpy(ziso_tmp_buf, addr, r);                 // read compressed block into temp buffer
            LZ4_decompress_fast(ziso_tmp_buf, addr, 2048); // decompress block
        }

        size -= 2048;
        addr += 2048;
        lsn++;
    }
    // calculate number of sectors read
    return lsn - o_lsn;
}
