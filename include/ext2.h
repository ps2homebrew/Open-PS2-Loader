/********************************************************************************
 * ext2.h - devoptab file routines for EXT2/3/4-based devices.                  *
 *                                                                              *
 * Copyright (c) 2010 Dimok                                                     *
 *                                                                              *
 * This program/include file is free software; you can redistribute it and/or   *
 * modify it under the terms of the GNU General Public License as published     *
 * by the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                          *
 *                                                                              *
 * This program/include file is distributed in the hope that it will be         *
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty          *
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                                 *
 *                                                                              *
 * You should have received a copy of the GNU General Public License            *
 * along with this program; if not, write to the Free Software Foundation,      *
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA                *
 ********************************************************************************/
#ifndef __EXT2_H_
#define __EXT2_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "bdmsupport.h"
#include "ext2_frag.h"

/**
 * EXT2 cache options
 *
 * It is recommended to use more pages instead of large page sizes for cache due to the sporadic write behaviour of ext file system.
 * It will significantly increase the speed. A page size of 32 is mostly suffiecient. The larger the page count the faster the
 * read/write between smaller files will be. Larger page sizes result in faster read/write of single big files.
 */
#define EXT2_CACHE_DEFAULT_PAGE_COUNT   64      /* The default number of pages in the cache */
#define EXT2_CACHE_DEFAULT_PAGE_SIZE    32      /* The default number of sectors per cache page */

/* EXT2 mount flags */
#define EXT2_FLAG_RW                    0x00001 /* Open the filesystem for reading and writing.  Without this flag, the filesystem is opened for reading only. */
#define EXT2_FLAG_FORCE                 0x00400 /* Open the filesystem regardless of the feature sets listed in the superblock */
#define EXT2_FLAG_JOURNAL_DEV_OK        0x01000 /* Only open external journal devices if this flag is set (e.g. ext3/ext4) */
#define EXT2_FLAG_64BITS                0x20000 /* Use the new style 64-Bit bitmaps. For more information see gen_bitmap64.c */
#define EXT2_FLAG_PRINT_PROGRESS        0x40000 /* If this flag is set the progress of file operations will be printed to stdout */
#define EXT2_FLAG_SKIP_MMP              0x100000 /* Open without multi-mount protection check. */
#define EXT2_FLAG_DEFAULT               (EXT2_FLAG_RW | EXT2_FLAG_64BITS | EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SKIP_MMP)

/**
 * Find all EXT2/3/4 partitions on a block device.
 *
 * @param INTERFACE The block device to search
 * @param PARTITIONS (out) A pointer to receive the array of partition start sectors
 *
 * @return The number of entries in PARTITIONS or -1 if an error occurred (see errno)
 * @note The caller is responsible for freeing PARTITIONS when finished with it
 */
void bdm_connect_fs(struct file_system *fs)
{
 /**
 * EXT2 cache options
 *
 * It is recommended to use more pages instead of large page sizes for cache due to the sporadic write behaviour of ext file system.
 * It will significantly increase the speed. A page size of 32 is mostly suffiecient. The larger the page count the faster the
 * read/write between smaller files will be. Larger page sizes result in faster read/write of single big files.
 */
    #define EXT2_CACHE_DEFAULT_PAGE_COUNT   64      /* The default number of pages in the cache */
    #define EXT2_CACHE_DEFAULT_PAGE_SIZE    32      /* The default number of sectors per cache page */

    /* EXT2 mount flags */
    #define EXT2_FLAG_RW                    0x00001 /* Open the filesystem for reading and writing.  Without this flag, the filesystem is opened for reading only. */
    #define EXT2_FLAG_FORCE                 0x00400 /* Open the filesystem regardless of the feature sets listed in the superblock */
    #define EXT2_FLAG_JOURNAL_DEV_OK        0x01000 /* Only open external journal devices if this flag is set (e.g. ext3/ext4) */
    #define EXT2_FLAG_64BITS                0x20000 /* Use the new style 64-Bit bitmaps. For more information see gen_bitmap64.c */
    #define EXT2_FLAG_PRINT_PROGRESS        0x40000 /* If this flag is set the progress of file operations will be printed to stdout */
    #define EXT2_FLAG_SKIP_MMP              0x100000 /* Open without multi-mount protection check. */
    #define EXT2_FLAG_DEFAULT               (EXT2_FLAG_RW | EXT2_FLAG_64BITS | EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SKIP_MMP)
}

 int ext2FindPartitions(const DISC_INTERFACE *interface, sec_t **partitions);

 /**
 * Mount a EXT2/3/4 partition from a specific sector on a block device.
 *
 * @param NAME The name to mount the device under (can then be accessed as "NAME:/")
 * @param INTERFACE The block device to mount
 * @param STARTSECTOR The sector the partition begins at
 * @param CACHEPAGECOUNT The total number of pages in the device cache
 * @param CACHEPAGESIZE The number of sectors per cache page
 * @param FLAGS Additional mounting flags (see above)
 *
 * @return True if mount was successful, false if no partition was found or an error occurred (see errno)
 */
bool ext2Mount(const char *name, const DISC_INTERFACE *interface, sec_t startSector, u32 cachePageCount, u32 cachePageSize, u32 flags);

/**
 * Unmount a EXT2/3/4 partition.
 *
 * @param NAME The name of mount used in ext2Mount()
 */
void ext2Unmount(const char *name);

/**
 * Get the volume name of a mounted EXT2/3/4 partition.
 *
 * @param NAME The name of mount
 *
 * @return The volumes name if successful or NULL if an error occurred (see errno)
 */
const char *ext2GetVolumeName (const char *name);

/**
 * Set the volume name of a mounted EXT2/3/4 partition.
 *
 * @param NAME The name of mount
 * @param VOLUMENAME The new volume name
 *
 * @return True if mount was successful, false if an error occurred (see errno)
 * @note The mount must be write-enabled else this will fail
 */
bool ext2SetVolumeName (const char *name, const char *volumeName);

#ifdef __cplusplus
}
#endif

#endif
