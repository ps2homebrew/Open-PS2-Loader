#include <loadcore.h>
#include <modload.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>

#define JAL(addr) (0x0c000000 | (((addr)&0x03ffffff) >> 2))

/* This game has a function that can be called by multiple threads.
   Its inter-task protection mechanism does not work properly.

   It also uses iReleaseWaitThread() to wake up one of the threads whenever an activity occurs,
   but there is nothing preventing iReleaseWaitThread() from breaking the new inter-task protection mechanism (which uses a semaphore)
   or other system functions that block. */

static int lockSema;
static int evFlag;

#define EV_ACTIVITY    1
#define EV_TIMEOUT     2

static int _lock(void)
{   //Do polling, to keep the original behaviour.
    return PollSema(lockSema);
}

static int _unlock(void)
{
    return SignalSema(lockSema);
}

static unsigned int EventTimeoutCb(void *evfID)
{
    iSetEventFlag((int)evfID, EV_TIMEOUT);
    return 0;
}

static void _WaitEvent(void)
{
    iop_sys_clock_t sys_clock;
    u32 bits;

    sys_clock.lo = 368640; //10000us = 36.864MHz * 1000 * 1000 / 1000 * 10, as per the original timeout.
    sys_clock.hi = 0;
    SetAlarm(&sys_clock, &EventTimeoutCb, (void*)evFlag);
    WaitEventFlag(evFlag, EV_ACTIVITY|EV_TIMEOUT, WEF_OR|WEF_CLEAR, &bits);
    if (!(bits & EV_TIMEOUT))
        CancelAlarm(&EventTimeoutCb, (void*)evFlag);
}

static void _iSetEvent(void)
{
    iSetEventFlag(evFlag, EV_ACTIVITY);
}

int _start(int argc, char **argv)
{
    lc_internals_t *lc;
    ModuleInfo_t *m;
    iop_sema_t sema;
    iop_event_t event;
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
    {
        sema.initial = 1;
        sema.max = 1;
        sema.option = 0;
        sema.attr = SA_THFIFO;

        lockSema = CreateSema(&sema);

        event.attr = EA_SINGLE;
        event.option = 0;
        event.bits = 0;
        evFlag = CreateEventFlag(&event);

        /* Apply patch on module.  */
        *(vu32*)(m->text_start + 0x00003bf8) = JAL((u32)&_lock);
        *(vu32*)(m->text_start + 0x00003bfc) = 0x00000000;
        *(vu32*)(m->text_start + 0x00003c04) = 0x14400046; //bnez $v0, exit
        *(vu32*)(m->text_start + 0x00003c0c) = 0x00000000;
        *(vu32*)(m->text_start + 0x00003d18) = JAL((u32)&_unlock);
        *(vu32*)(m->text_start + 0x00003d1c) = 0x00000000;

        //Replace event system that used iReleaseWaitThread() - which could break other functions that use a semaphore.
        *(vu32*)(m->text_start + 0x00000348) = JAL((u32)&_WaitEvent);
        *(vu32*)(m->text_start + 0x00000ac8) = JAL((u32)&_iSetEvent);
        *(vu32*)(m->text_start + 0x00000bd0) = JAL((u32)&_iSetEvent);
        *(vu32*)(m->text_start + 0x00001b88) = JAL((u32)&_iSetEvent);

//        FlushIcache(); //Flush instruction cache as instructions were modified.

        StartModule(modId, "", 0, NULL, &modRet);

        return MODULE_RESIDENT_END;
    }

    printf("Could not find module %d.\n", modId);

    return MODULE_NO_RESIDENT_END;
}

