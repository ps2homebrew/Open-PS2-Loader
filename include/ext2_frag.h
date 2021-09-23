#define EXT2_FRAG_H_
#include "bdmsupport.h"


typedef int (*_ext2_frag_append_t)(void *ff, u32 offset, u32 sector, u32 count);

int _EXT2_get_fragments(const char *in_path, _ext2_frag_append_t append_fragment, void *callback_data);



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
