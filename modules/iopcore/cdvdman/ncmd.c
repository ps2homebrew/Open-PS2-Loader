/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

//-------------------------------------------------------------------------
int sceCdSync(int mode)
{
    DPRINTF("sceCdSync %d sync flag = %d\n", mode, sync_flag);

    if (!sync_flag)
        return 0;

    if ((mode == 1) || (mode == 17))
        return 1;

    while (sync_flag)
        WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);

    return 0;
}

//-------------------------------------------------------------------------
int sceCdRead(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
{
    int result;

    DPRINTF("sceCdRead lsn=%d sectors=%d buf=%08x\n", (int)lsn, (int)sectors, (int)buf);

    if ((!(cdvdman_settings.common.flags & IOPCORE_COMPAT_ALT_READ)) || QueryIntrContext()) {
        result = cdvdman_AsyncRead(lsn, sectors, buf);
    } else {
        result = cdvdman_SyncRead(lsn, sectors, buf);
    }

    return result;
}

//-------------------------------------------------------------------------
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
{
    return sceCdRead(lsn, sectors, buf, mode);
}

//-------------------------------------------------------------------------
int sceCdGetToc(u8 *toc)
{
    if (sync_flag)
        return 0;

    cdvdman_stat.err = SCECdErREAD;

    return 0; //Not supported
}

//-------------------------------------------------------------------------
int sceCdSeek(u32 lsn)
{
    DPRINTF("sceCdSeek %d\n", (int)lsn);

    if (sync_flag)
        return 0;

    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.status = SCECdStatPause;

    cdvdman_cb_event(SCECdFuncSeek);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdStandby(void)
{
    cdvdman_stat.err = SCECdErNO;
    cdvdman_stat.status = SCECdStatPause;

    cdvdman_cb_event(SCECdFuncStandby);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdStop(void)
{
    if (sync_flag)
        return 0;

    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.status = SCECdStatStop;
    cdvdman_cb_event(SCECdFuncStop);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdPause(void)
{
    DPRINTF("sceCdPause\n");

    if (sync_flag)
        return 0;

    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.status = SCECdStatPause;
    cdvdman_cb_event(SCECdFuncPause);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdDiskReady(int mode)
{
    DPRINTF("sceCdDiskReady %d\n", mode);
    cdvdman_stat.err = SCECdErNO;

    if (cdvdman_cdinited) {
        if (mode == 0) {
            while (sync_flag)
                WaitEventFlag(cdvdman_stat.intr_ef, 1, WEF_AND, NULL);
        }

        if (!sync_flag)
            return SCECdComplete;
    }

    return SCECdNotReady;
}

//-------------------------------------------------------------------------
int sceCdReadDiskID(unsigned int *DiskID)
{
    int i;
    u8 *p = (u8 *)DiskID;

    for (i = 0; i < 5; i++) {
        if (p[i] != 0)
            break;
    }
    if (i == 5)
        *((u16 *)DiskID) = (u16)0xadde;
    else
        mips_memcpy(DiskID, cdvdman_settings.common.DiscID, 5);

    return 1;
}
