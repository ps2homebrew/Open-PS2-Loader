#include "../modules/isofs/zso.c"

int probed_fd = 0;
u32 probed_lba = 0;

static void ReadHDDSectors(u32 lba, u8* buffer, u32 nsectors)
{
    u32 lsn = probed_lba + (lba*4);
    hddReadSectors(lsn, nsectors*4, buffer);
}

static void longLseek(int fd, unsigned int lba)
{
    unsigned int remaining, toSeek;

    if (lba > INT_MAX / 2048) {
        lseek(fd, INT_MAX / 2048 * 2048, SEEK_SET);

        remaining = lba - INT_MAX / 2048;
        while (remaining > 0) {
            toSeek = remaining > INT_MAX / 2048 ? INT_MAX / 2048 : remaining;
            lseek(fd, toSeek * 2048, SEEK_CUR);
            remaining -= toSeek;
        }
    } else {
        lseek(fd, lba * 2048, SEEK_SET);
    }
}

int read_cso_data(u8* addr, u32 size, u32 offset, u32 shift)
{
    u32 lba = offset / (2048 >> shift); // avoid overflow by shifting sector size instead of offset
    u32 pos = (offset << shift) & 2047; // doesn't matter if it overflows since we only care about the 11 LSB anyways
    if (probed_fd > 0){ // USB/ETH
        logfile("read from USB/ETH sectors\n");
        longLseek(probed_fd, lba);
        lseek(probed_fd, pos, SEEK_CUR);
        return read(probed_fd, addr, size);
    }
    else{ // HDD
        logfile("read from HDD\n");
        u32 o_size = size;

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