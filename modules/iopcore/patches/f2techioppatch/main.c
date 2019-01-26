/** Fix for Shadow Man: 2econd Coming.
 *  The game does not allocate sufficient memory for holding compressed data before decompression.
 *  The corruption happens when the game tries to read 17 sectors due to the sound driver
 *  rounding up to 17 when the reading amount being not a multiple of CD-ROM sector sizes.
 *
 *  Even if 17 sectors are read, the uncompressed data does not seem to exceed 32768 bytes.
 */

#include <loadcore.h>
#include <modload.h>
#include <stdio.h>
#include <sysclib.h>

#define JAL(addr) (0x0c000000 | (((addr)&0x03ffffff) >> 2))
#define JMP(addr) (0x08000000 | (0x3ffffff & ((addr) >> 2)))

int _start(int argc, char **argv)
{
    lc_internals_t *lc;
    ModuleInfo_t *m;
    int modId, modRet;

    if (argc != 2)
    {
        printf("Missing module arg.\n");
        return MODULE_NO_RESIDENT_END;
    }

    modId = LoadModule(argv[1]);
    if (modId < 0)
    {
        printf("Failed to load %s (%d).\n", argv[1], modId);
        return MODULE_NO_RESIDENT_END;
    }

    lc = GetLoadcoreInternalData();

    //Locate the specified module.
    m = lc->image_info;
    while (m != NULL)
    {
        if (modId == m->id)
            break;

        m = m->next;
    }

    if (m != NULL)
    {   //Increase size of CompBuffers, to allow for 17 sectors to be stored.
        *(u16*)(m->text_start + 0x000016b8) = 0x8800; //Original: 0x8000
        *(u16*)(m->text_start + 0x000016bc) = 0x8800; //Original: 0x8000

        StartModule(modId, "", 0, NULL, &modRet);

        return MODULE_RESIDENT_END;
    }

    printf("Could not find module %d.\n", modId);

    return MODULE_NO_RESIDENT_END;
}

