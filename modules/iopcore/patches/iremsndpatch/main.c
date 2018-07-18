#include <loadcore.h>
#include <thbase.h>

#define JAL(addr) (0x0c000000 | (((addr)&0x03ffffff) >> 2))
#define JMP(addr) (0x08000000 | (0x3ffffff & ((addr) >> 2)))

/* There was a memo dated July 2000, which went along the lines of saying that the atick functions of modmidi and modhsyn should be called,
   if related sequence data is stopped and removed.

   The explanation was that these modules only process sequence data when their atick functions are called.
   If data is removed and their atick functions are not called before further calls to the library are made,
   then the correct operation of the library cannot be guaranteed because the data would be dequeued.

   By logging the commands issued to the RPC server of IREMSND, it can be established that the game eventually calls sndIopSesqCmd_STOPSE.
   But sndIopSesqCmd_STOPSE does not ever call the modseseq and/or modhsyn atick functions!

   So to correct this, we wake up the tick thread, after the sndIopSesqCmd_STOPSE or sndIopSesqCmd_STOPSEID RPC functions are called.
   While Sony also documented that this could cause a change in tempo, it is a small price to pay for such a simple fix. */

static int *tickThreadId;

static int postStopSeseqTick(void)
{
    WakeupThread(*tickThreadId);
    return 1;
}

int _start(int argc, char **argv)
{
    lc_internals_t *lc;
    ModuleInfo_t *m, *prevM;
    u16 lo16, hi16;

    lc = GetLoadcoreInternalData();

    //Locate the last-registered module.
    m = lc->image_info;
    prevM = NULL;
    while(m->next != NULL)
    {
      prevM = m;
      m = m->next;
    }
    m = prevM;

    if (m != NULL)
    {
        //Generate pointer to the threadId variable.
	hi16 = *(vu16*)(m->text_start + 0x00000ac0);
	lo16 = *(vu16*)(m->text_start + 0x00000ac4);
	tickThreadId = (int*)(((u32)hi16 << 16) | lo16);

        //Apply patch on module.
	//sndIopSesqCmd_STOPSE
        *(vu32*)(m->text_start + 0x00001f4c) = JMP((u32)&postStopSeseqTick);
	//sndIopSesqCmd_STOPSEID
        *(vu32*)(m->text_start + 0x00001ff8) = JMP((u32)&postStopSeseqTick);

        return MODULE_RESIDENT_END;
    }

    return MODULE_NO_RESIDENT_END;
}

