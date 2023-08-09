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

    u16 sector_size = 2048;

    // Is is NULL in our emulated cdvdman routines so check if valid.
    if (mode) {
        // 0 is 2048
        if (mode->datapattern == SCECdSecS2328)
            sector_size = 2328;

        if (mode->datapattern == SCECdSecS2340)
            sector_size = 2340;
    }

    DPRINTF("sceCdRead lsn=%d sectors=%d sector_size=%d buf=%08x\n", (int)lsn, (int)sectors, (int)sector_size, (int)buf);

    if ((!(cdvdman_settings.common.flags & IOPCORE_COMPAT_ALT_READ)) || QueryIntrContext()) {
        result = cdvdman_AsyncRead(lsn, sectors, sector_size, buf);
    } else {
        result = cdvdman_SyncRead(lsn, sectors, sector_size, buf);
    }

    return result;
}

//-------------------------------------------------------------------------
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
{
    return sceCdRead(lsn, sectors, buf, mode);
}

//-------------------------------------------------------------------------
void lba_to_msf(s32 lba, u8 *m, u8 *s, u8 *f)
{
    lba += 150;
    *m = lba / (60 * 75);
    *s = (lba / 75) % 60;
    *f = lba % 75;
}

int cdvdman_fill_toc(u8 *tocBuff)
{

    u8 discType = cdvdman_stat.disc_type_reg & 0xFF;

    DPRINTF("cdvdman_fill_toc tocBuff=%08x discType=%02X\n", (int)tocBuff, discType);

    if (tocBuff == NULL) {
        return 0;
    }

    switch (discType) {
        case 0x12: // SCECdPS2CD
        case 0x13: // SCECdPS2CDDA
            u8 min;
            u8 sec;
            u8 frm;

            memset(tocBuff, 0, 1024);

            tocBuff[0] = 0x41;
            tocBuff[1] = 0x00;

            // Number of FirstTrack,
            // Always 1 until PS2CCDA support get's added.
            tocBuff[2] = 0xA0;
            tocBuff[7] = itob(1);

            // Number of LastTrack
            // Always 1 until PS2CCDA support get's added.
            tocBuff[12] = 0xA1;
            tocBuff[17] = itob(1);

            // DiskLength
            lba_to_msf(mediaLsnCount, &min, &sec, &frm);
            tocBuff[22] = 0xA2;
            tocBuff[27] = itob(min);
            tocBuff[28] = itob(sec);
            tocBuff[29] = itob(frm);

            // Later if PS2CCDA is added the tracks need to get filled in toc too.
            break;

        case 0x14: // SCECdPS2DVD
        case 0xFE: // SCECdDVDV
            // Toc for single layer DVD.
            memset(tocBuff, 0, 2048);

            // Write only what we need, memset has cleared the above buffers.
            //  Single Layer - Values are fixed.
            tocBuff[0] = 0x04;
            tocBuff[1] = 0x02;
            tocBuff[2] = 0xF2;
            tocBuff[4] = 0x86;
            tocBuff[5] = 0x72;

            // These values are fixed on all discs, except position 14 which is the OTP/PTP flags which are 0 in single layer.

            tocBuff[12] = 0x01;
            tocBuff[13] = 0x02;
            tocBuff[14] = 0x01;
            tocBuff[17] = 0x03;

            u32 maxlsn = mediaLsnCount + (0x30000 - 1);
            tocBuff[20] = (maxlsn >> 24) & 0xFF;
            tocBuff[21] = (maxlsn >> 16) & 0xff;
            tocBuff[22] = (maxlsn >> 8) & 0xff;
            tocBuff[23] = (maxlsn >> 0) & 0xff;
            break;

        default:
            // Check if we are DVD9 game and fill toc for it.

            if (!(cdvdman_settings.common.flags & IOPCORE_COMPAT_EMU_DVDDL)) {
                memset(tocBuff, 0, 2048);

                // Dual sided - Values are fixed.
                tocBuff[0] = 0x24;
                tocBuff[1] = 0x02;
                tocBuff[2] = 0xF2;
                tocBuff[4] = 0x41;
                tocBuff[5] = 0x95;

                // These values are fixed on all discs, except position 14 which is the OTP/PTP flags.
                tocBuff[12] = 0x01;
                tocBuff[13] = 0x02;
                tocBuff[14] = 0x21; // PTP
                tocBuff[15] = 0x10;

                // Values are fixed.
                tocBuff[17] = 0x03;

                u32 l1s = mediaLsnCount + 0x30000 - 1;
                tocBuff[20] = (l1s >> 24);
                tocBuff[21] = (l1s >> 16) & 0xff;
                tocBuff[22] = (l1s >> 8) & 0xff;
                tocBuff[23] = (l1s >> 0) & 0xff;

                return 1;
            }

            // Not known type.
            DPRINTF("cdvdman_fill_toc unimplemented for  discType=%02X\n", discType);
            return 0;
    }

    return 1;
}


int sceCdGetToc(u8 *toc)
{
    DPRINTF("sceCdGetToc toc=%08x\n", (int)toc);

    if (sync_flag)
        return 0;

    cdvdman_stat.err = SCECdErNO;
    int result = cdvdman_fill_toc(toc);

    if (!result) {
        cdvdman_stat.err = SCECdErREAD;
    }

    cdvdman_cb_event(SCECdFuncGetToc);

    return result;
}

//-------------------------------------------------------------------------
int sceCdSeek(u32 lsn)
{
    DPRINTF("sceCdSeek %d\n", (int)lsn);

    if (sync_flag)
        return 0;

    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.status = SCECdStatPause;

    // Set the invalid parament error in case of trying to seek more than max lsn.
    if (mediaLsnCount) {
        if (lsn >= mediaLsnCount) {
            DPRINTF("cdvdman_searchfile_init mediaLsnCount=%d\n", mediaLsnCount);
            cdvdman_stat.err = SCECdErIPI;
        }
    }

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
            return DeviceReady();
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
        memcpy(DiskID, cdvdman_settings.common.DiscID, 5);

    return 1;
}
