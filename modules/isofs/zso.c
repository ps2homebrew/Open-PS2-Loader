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

    u32 last_block;
    u32 cur_block = lsn;

    if (lsn >= ziso_total_block) {
        return 0; // can't seek beyond file
    }

    if (lsn + count >= ziso_total_block) {
        count = ziso_total_block - lsn; // adjust sector count if reading beyond file
    }

    // refresh index table if needed
    if (ziso_idx_start_block < 0 || lsn < ziso_idx_start_block || lsn >= ziso_idx_start_block + ZISO_IDX_MAX_ENTRIES - 1) {
        read_raw_data((u8 *)ziso_idx_cache, ZISO_IDX_MAX_ENTRIES * sizeof(u32), lsn * 4 + sizeof(ZISO_header), 0);
        ziso_idx_start_block = lsn;
    }

    // calculate start and size of all compressed data
    if (cur_block + count < ziso_idx_start_block + ZISO_IDX_MAX_ENTRIES)
        last_block = ziso_idx_cache[cur_block + count - ziso_idx_start_block];
    else
        read_raw_data(&last_block, sizeof(u32), (cur_block + count) * 4 + sizeof(ZISO_header), 0);

    u32 o_start = (ziso_idx_cache[cur_block - ziso_idx_start_block] & 0x7FFFFFFF);
    u32 o_end = (last_block & 0x7FFFFFFF);
    u32 compressed_size = (o_end - o_start) << ziso_align;

    // read all compressed data to the end of provided buffer
    u8 *c_buff = addr + (count * 2048) - compressed_size;
    read_raw_data(c_buff, compressed_size, o_start, ziso_align);

    // process each sector
    for (int i = 0; i < count; i++) {

        // refresh block index cache if needed
        if (cur_block >= ziso_idx_start_block + ZISO_IDX_MAX_ENTRIES - 1) {
            read_raw_data((u8 *)ziso_idx_cache, ZISO_IDX_MAX_ENTRIES * sizeof(u32), cur_block * 4 + sizeof(ZISO_header), 0);
            ziso_idx_start_block = cur_block;
        }

        // read block offset and size
        u32 b_offset = ziso_idx_cache[cur_block - ziso_idx_start_block];
        u32 b_size = ziso_idx_cache[cur_block - ziso_idx_start_block + 1];
        u32 topbit = b_offset & 0x80000000;
        b_offset = (b_offset & 0x7FFFFFFF);
        b_size = (b_size & 0x7FFFFFFF);
        b_size = (b_size - b_offset) << ziso_align;

        // prevent reading more than a sector (eliminates padding if any)
        int r = MIN(b_size, 2048);

        // if the block is not compressed, it will already be in its correct place
        if (topbit == 0) {                                 // block is compressed
            memcpy(ziso_tmp_buf, c_buff, r);                 // read compressed block into temp buffer
            LZ4_decompress_fast(ziso_tmp_buf, addr, 2048); // decompress block
        }
        else{
            // move block to its correct position in the buffer
            memcpy(addr, c_buff, r);
        }

        cur_block++;
        addr += 2048;
        c_buff += b_size;
    }
    return cur_block - lsn;
}
