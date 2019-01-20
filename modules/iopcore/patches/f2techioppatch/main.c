/** Fix for Shadow Man: 2econd Coming.
 *  The game does not allocate sufficient memory for holding compressed data before decompression.
 *  The corruption happens when the game tries to read 17 sectors due to the sound driver
 *  rounding up to 17 when the reading amount being not a multiple of CD-ROM sector sizes.
 */

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>

#define JAL(addr) (0x0c000000 | (((addr)&0x03ffffff) >> 2))
#define JMP(addr) (0x08000000 | (0x3ffffff & ((addr) >> 2)))

static void **CompBuffers;

static void *(*pIOP_SafeMalloc)(int size);
static void (*pIOP_SafeFree)(void *buffer);

int _start(int argc, char **argv)
{
    lc_internals_t *lc;
    ModuleInfo_t *m;
    int modId;

    if (argc != 2)
    {
        printf("Missing module ID arg.\n");
        return MODULE_NO_RESIDENT_END;
    }

    modId = strtol(argv[1], NULL, 16);

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
    {

	pIOP_SafeMalloc = (void*)(m->text_start + 0x00000d10);
	pIOP_SafeFree = (void*)(m->text_start + 0x00000ddc);
	CompBuffers =  (void**)(m->text_start + 0x00009bb0);

	pIOP_SafeFree(CompBuffers[0]);
	pIOP_SafeFree(CompBuffers[1]);
	CompBuffers[0] = pIOP_SafeMalloc(0x8800);	//Original: 0x8000
	CompBuffers[1] = pIOP_SafeMalloc(0x8800);	//Original: 0x8000

        return MODULE_RESIDENT_END;
    }

    printf("Could not find module %d.\n", modId);

    return MODULE_NO_RESIDENT_END;
}

