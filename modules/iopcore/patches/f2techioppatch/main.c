/** Fix for Shadow Man: 2econd Coming.
 *  The game does not allocate sufficient memory for holding compressed data before decompression.
 *  The corruption happens when the game tries to read 17 sectors due to the sound driver
 *  rounding up to 17 when the reading amount being not a multiple of CD-ROM sector sizes.
 */

#include <loadcore.h>
#include <intrman.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <sifman.h>
#include <stdio.h>

#define JAL(addr) (0x0c000000 | (((addr)&0x03ffffff) >> 2))
#define JMP(addr) (0x08000000 | (0x3ffffff & ((addr) >> 2)))

static void **CompBuffers;

static void *(*pIOP_SafeMalloc)(int size);
static void (*pIOP_SafeFree)(void *buffer);

int _start(int argc, char **argv)
{
    lc_internals_t *lc;
    ModuleInfo_t *m, *secondLastMod, *lastMod;
    int OldState, HighestID;

    lc = GetLoadcoreInternalData();

    //Locate the 2nd last-registered module, which is the module loaded before this.
    m = lc->image_info;
    lastMod = NULL;
    secondLastMod = NULL;
    HighestID = -1;
    while (m != NULL)
    {
        if (HighestID < m->id)
        {
            HighestID = m->id;
            secondLastMod = lastMod;
            lastMod = m;
        }

        m = m->next;
    }

    if (secondLastMod != NULL)
    {
        m = secondLastMod;

        CpuSuspendIntr(&OldState);
	pIOP_SafeMalloc = (void*)(m->text_start + 0x00000d10);
	pIOP_SafeFree = (void*)(m->text_start + 0x00000ddc);
	CompBuffers =  (void**)(m->text_start + 0x00009bb0);
        CpuResumeIntr(OldState);

        FlushIcache();

	pIOP_SafeFree(CompBuffers[0]);
	pIOP_SafeFree(CompBuffers[1]);
	CompBuffers[0] = pIOP_SafeMalloc(0x8800);	//Original: 0x8000
	CompBuffers[1] = pIOP_SafeMalloc(0x8800);	//Original: 0x8000

        return MODULE_RESIDENT_END;
    }

    return MODULE_NO_RESIDENT_END;
}

