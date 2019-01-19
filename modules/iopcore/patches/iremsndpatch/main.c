#include <intrman.h>
#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>

#define JAL(addr) (0x0c000000 | (((addr)&0x03ffffff) >> 2))
#define JMP(addr) (0x08000000 | (0x3ffffff & ((addr) >> 2)))

/* There was a memo dated July 2000, which went along the lines of saying that the atick functions of modmidi and modhsyn should be called,
   if related sequence data is stopped and removed.

   The explanation was that these modules only process sequence data when their atick functions are called.
   If data is removed and their atick functions are not called before further calls to the library are made,
   then the correct operation of the library cannot be guaranteed because the data would be dequeued.

   By logging the commands issued to the RPC server of IREMSND, it can be established that the game eventually calls sndIopSesqCmd_STOPSE.
   But sndIopSesqCmd_STOPSE does not ever call the modseseq and/or modhsyn atick functions!

   So to correct this, we wake up the tick thread, after the sndIopSesqCmd_STOPSE, sndIopSesqCmd_STOPSEID and sndIopCmd_STOPALL RPC functions are called.
   While Sony also documented that this could cause a change in tempo, it is a small price to pay for such a simple fix.

   The inter-task protection mechanism was a simple one that involved a flag, which ran under the assumption that the atick thread will always run where the RPC thread does not.
   However, if the atick thread does get changed to WAIT, then the RPC thread can RUN (which allows the critical section to be violated). */

extern void _lock_RpcFunc(void);

static int *tickThreadId;
static int lockSema;

static int postStopSeseqTick(void)
{   //Even if the atick thread waits on the locking semaphore, it will wake up immediately when the RPC thread unlocks the critical section.
    WakeupThread(*tickThreadId);
    return 1;
}

int _lock(void)
{
    return WaitSema(lockSema);
}

int _unlock(void)
{
    return SignalSema(lockSema);
}

int _start(int argc, char **argv)
{
    lc_internals_t *lc;
    ModuleInfo_t *m;
    u16 hi16;
    s16 lo16;
    iop_sema_t sema;
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
        sema.initial = 1;
        sema.max = 1;
        sema.option = 0;
        sema.attr = 0;

        lockSema = CreateSema(&sema);

        //Generate pointer to the threadId variable.
	hi16 = *(vu16*)(m->text_start + 0x00000ac0);
	lo16 = *(volatile s16*)(m->text_start + 0x00000ac4);
	tickThreadId = (int*)(((u32)hi16 << 16) + lo16);

        //Apply patch on module. The module has not been initialized yet and has no running threads.
        //sndIopAtick
	*(vu32*)(m->text_start + 0x00000418) = JAL((u32)&_lock);
	*(vu32*)(m->text_start + 0x000018f4) = JMP((u32)&_unlock);

        //sndIopRpcFunc
	*(vu32*)(m->text_start + 0x00000d58) = JAL((u32)&_lock_RpcFunc);
	*(vu32*)(m->text_start + 0x00000d64) = 0x00000000; //nop
	*(vu32*)(m->text_start + 0x00000d80) = 0xac233d24; //sw v1, $3d24(at) - Replace instruction at 0xd64.
	*(vu32*)(m->text_start + 0x00000f54) = JAL((u32)&_unlock);
	*(vu32*)(m->text_start + 0x00000f58) = 0x00000000; //nop

	//sndIopSesqCmd_STOPSE
        *(vu32*)(m->text_start + 0x00001f4c) = JMP((u32)&postStopSeseqTick);
	//sndIopSesqCmd_STOPSEID
        *(vu32*)(m->text_start + 0x00001ff8) = JMP((u32)&postStopSeseqTick);
	//sndIopCmd_STOPALL
        *(vu32*)(m->text_start + 0x00000c30) = JMP((u32)&postStopSeseqTick);

        FlushIcache(); //Flush instruction cache as instructions were modified.

        return MODULE_RESIDENT_END;
    }

    printf("Could not find module %d.\n", modId);

    return MODULE_NO_RESIDENT_END;
}

