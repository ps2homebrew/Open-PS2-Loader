#include "../modules/isofs/zso.c"

int probed_fd = 0;
u32 probed_lba = 0;

void ReadHDDSectors(u32 lba, u8* buffer, u32 nsectors){
    int k, w;
    int lsn = probed_lba + (lba*4);
    for (k = 0; k <= nsectors * 4; k++) { // NB: Disc sector size (2048 bytes) and HDD sector size (512 bytes) differ, hence why we multiplied the number of sectors by 4.
        hddReadSectors(lsn + k, 1, buffer);
        buffer += 512;
    }
}

int read_cso_data(u8* addr, u32 size, u32 offset, u32 shift)
{
    if (probed_fd > 0){ // USB/ETH
        u64 pos = (u64)offset<<shift;
        lseek64(probed_fd, pos, SEEK_SET);
        return read(probed_fd, addr, size);
    }
    else{ // HDD
        u32 o_size = size;
        u32 lba = offset / (2048 >> shift); // avoid overflow by shifting sector size instead of offset
        u32 pos = (offset << shift) & 2047; // doesn't matter if it overflows since we only care about the 11 LSB anyways

        // read first block if not aligned to sector size
        if (pos) {
            int r = MIN(size, (2048 - pos));
            ReadHDDSectors(lba, ciso_tmp_buf, 1);
            memcpy(addr, ciso_tmp_buf + pos, r);
            size -= r;
            lba++;
            addr += r;
        }

        // read intermediate blocks if more than one block is left
        u32 n_blocks = size / 2048;
        if (size % 2048)
            n_blocks++;
        if (n_blocks > 1) {
            int r = 2048 * (n_blocks - 1);
            ReadHDDSectors(lba, addr, n_blocks - 1);
            size -= r;
            addr += r;
            lba += n_blocks - 1;
        }

        // read remaining data
        if (size) {
            ReadHDDSectors(lba, ciso_tmp_buf, 1);
            memcpy(addr, ciso_tmp_buf, size);
            size = 0;
        }

        // return remaining size
        return o_size - size;
    }
}

void* ciso_alloc(u32 size){
    return malloc(size);
}