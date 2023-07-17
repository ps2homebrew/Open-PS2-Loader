/*
#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013 Bat Rastard
# Copyright 2014, 2015, 2016 doctorxyz
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#
*/

#include "include/opl.h"
#include "include/config.h"
#include "include/util.h"
#include "include/system.h"
#include "include/ioman.h"
#include "include/renderman.h"

#include "include/pggsm.h"

static int gEnableGSM;   // Enables GSM - 0 for Off, 1 for On
static int gGSMVMode;    // See the related predef_vmode
static int gGSMXOffset;  // 0 - Off, Any other positive or negative value - Relative position for X Offset
static int gGSMYOffset;  // 0 - Off, Any other positive or negative value - Relative position for Y Offset
static int gGSMFIELDFix; // Enables/disables the FIELD flipping emulation option. 0 for Off, 1 for On.

void InitGSMConfig(config_set_t *configSet)
{
    config_set_t *configGame = configGetByType(CONFIG_GAME);

    // Default values.
    gGSMSource = 0;
    gEnableGSM = 0;
    gGSMVMode = 0;
    gGSMXOffset = 0;
    gGSMYOffset = 0;
    gGSMFIELDFix = 0;

    if (configGetInt(configSet, CONFIG_ITEM_GSMSOURCE, &gGSMSource)) {
        // Load the rest of the per-game GSM configuration, only if GSM is enabled.
        if (configGetInt(configSet, CONFIG_ITEM_ENABLEGSM, &gEnableGSM) && gEnableGSM) {
            configGetInt(configSet, CONFIG_ITEM_GSMVMODE, &gGSMVMode);
            configGetInt(configSet, CONFIG_ITEM_GSMXOFFSET, &gGSMXOffset);
            configGetInt(configSet, CONFIG_ITEM_GSMYOFFSET, &gGSMYOffset);
            configGetInt(configSet, CONFIG_ITEM_GSMFIELDFIX, &gGSMFIELDFix);
        }
    } else {
        if (configGetInt(configGame, CONFIG_ITEM_ENABLEGSM, &gEnableGSM) && gEnableGSM) {
            configGetInt(configGame, CONFIG_ITEM_GSMVMODE, &gGSMVMode);
            configGetInt(configGame, CONFIG_ITEM_GSMXOFFSET, &gGSMXOffset);
            configGetInt(configGame, CONFIG_ITEM_GSMYOFFSET, &gGSMYOffset);
            configGetInt(configGame, CONFIG_ITEM_GSMFIELDFIX, &gGSMFIELDFix);
        }
    }
}

int GetGSMEnabled(void)
{
    return gEnableGSM;
}

void PrepareGSM(char *cmdline)
{
    /* Preparing GSM */
    LOG("Preparing GSM...\n");
    // Pre-defined vmodes
    // Some of following vmodes result in no display and/or freezing, depending on the console kernel and GS revision.
    // Therefore there are many variables involved here that can lead us to success or fail, depending on the already mentioned circumstances.
    //
    // clang-format off
    static const predef_vmode_struct predef_vmode[29] = {
        //                                                            DH    DW   MAGV MAGH  DY   DX              VS  VDP  VBPE  VBP VFPE  VFP
        {GS_INTERLACED,    GS_MODE_NTSC,        GS_FIELD, makeDISPLAY(447,  2559, 0,   3,   46,  700), makeSYNCV(6,  480,  6,    26,  6,   1)},
        {GS_INTERLACED,    GS_MODE_NTSC,        GS_FRAME, makeDISPLAY(223,  2559, 0,   3,   26,  700), makeSYNCV(6,  480,  6,    26,  6,   2)},
        {GS_INTERLACED,    GS_MODE_PAL,         GS_FIELD, makeDISPLAY(511,  2559, 0,   3,   70,  720), makeSYNCV(5,  576,  5,    33,  5,   1)},
        {GS_INTERLACED,    GS_MODE_PAL,         GS_FRAME, makeDISPLAY(255,  2559, 0,   3,   37,  720), makeSYNCV(5,  576,  5,    33,  5,   4)},
        {GS_INTERLACED,    GS_MODE_PAL,         GS_FIELD, makeDISPLAY(447,  2559, 0,   3,   46,  700), makeSYNCV(6,  480,  6,    26,  6,   1)},
        {GS_INTERLACED,    GS_MODE_PAL,         GS_FRAME, makeDISPLAY(223,  2559, 0,   3,   26,  700), makeSYNCV(6,  480,  6,    26,  6,   2)},
        {GS_NONINTERLACED, GS_MODE_DTV_480P,    GS_FRAME, makeDISPLAY(255,  2559, 0,   1,   12,  736), makeSYNCV(6,  483,  3072, 30,  0,   6)},
        {GS_NONINTERLACED, GS_MODE_DTV_576P,    GS_FRAME, makeDISPLAY(255,  2559, 0,   1,   23,  756), makeSYNCV(5,  576,  0,    39,  0,   5)},
        {GS_NONINTERLACED, GS_MODE_DTV_480P,    GS_FRAME, makeDISPLAY(479,  1439, 0,   1,   35,  232), makeSYNCV(6,  483,  3072, 30,  0,   6)},
        {GS_NONINTERLACED, GS_MODE_DTV_576P,    GS_FRAME, makeDISPLAY(575,  1439, 0,   1,   44,  255), makeSYNCV(5,  576,  0,    39,  0,   5)},
        {GS_NONINTERLACED, GS_MODE_DTV_720P,    GS_FRAME, makeDISPLAY(719,  1279, 1,   1,   24,  302), makeSYNCV(5,  720,  0,    20,  0,   5)},
        {GS_INTERLACED,    GS_MODE_DTV_1080I,   GS_FIELD, makeDISPLAY(1079, 1919, 1,   2,   48,  238), makeSYNCV(10, 1080, 2,    28,  0,   5)},
        {GS_INTERLACED,    GS_MODE_DTV_1080I,   GS_FRAME, makeDISPLAY(1079, 1919, 0,   2,   48,  238), makeSYNCV(10, 1080, 2,    28,  0,   5)},
        {GS_NONINTERLACED, GS_MODE_VGA_640_60,  GS_FRAME, makeDISPLAY(479,  1279, 0,   1,   54,  276), makeSYNCV(2,  480,  0,    33,  0,   10)},
        {GS_NONINTERLACED, GS_MODE_VGA_640_72,  GS_FRAME, makeDISPLAY(479,  1279, 0,   1,   18,  330), makeSYNCV(3,  480,  0,    28,  0,   9)},
        {GS_NONINTERLACED, GS_MODE_VGA_640_75,  GS_FRAME, makeDISPLAY(479,  1279, 0,   1,   18,  360), makeSYNCV(3,  480,  0,    16,  0,   1)},
        {GS_NONINTERLACED, GS_MODE_VGA_640_85,  GS_FRAME, makeDISPLAY(479,  1279, 0,   1,   18,  260), makeSYNCV(3,  480,  0,    16,  0,   1)},
        {GS_INTERLACED,    GS_MODE_VGA_640_60,  GS_FIELD, makeDISPLAY(959,  1279, 1,   1,   128, 291), makeSYNCV(2,  992,  0,    33,  0,   10)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_56,  GS_FRAME, makeDISPLAY(599,  1599, 0,   1,   25,  450), makeSYNCV(2,  600,  0,    22,  0,   1)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_60,  GS_FRAME, makeDISPLAY(599,  1599, 0,   1,   25,  465), makeSYNCV(4,  600,  0,    23,  0,   1)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_72,  GS_FRAME, makeDISPLAY(599,  1599, 0,   1,   25,  465), makeSYNCV(6,  600,  0,    23,  0,   37)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_75,  GS_FRAME, makeDISPLAY(599,  1599, 0,   1,   25,  510), makeSYNCV(3,  600,  0,    21,  0,   1)},
        {GS_NONINTERLACED, GS_MODE_VGA_800_85,  GS_FRAME, makeDISPLAY(599,  1599, 0,   1,   15,  500), makeSYNCV(3,  600,  0,    27,  0,   1)},
        {GS_NONINTERLACED, GS_MODE_VGA_1024_60, GS_FRAME, makeDISPLAY(767,  2047, 0,   2,   30,  580), makeSYNCV(6,  768,  0,    29,  0,   3)},
        {GS_NONINTERLACED, GS_MODE_VGA_1024_70, GS_FRAME, makeDISPLAY(767,  1023, 0,   0,   30,  266), makeSYNCV(6,  768,  0,    29,  0,   3)},
        {GS_NONINTERLACED, GS_MODE_VGA_1024_75, GS_FRAME, makeDISPLAY(767,  1023, 0,   0,   30,  260), makeSYNCV(3,  768,  0,    28,  0,   1)},
        {GS_NONINTERLACED, GS_MODE_VGA_1024_85, GS_FRAME, makeDISPLAY(767,  1023, 0,   0,   30,  290), makeSYNCV(3,  768,  0,    36,  0,   1)},
        {GS_NONINTERLACED, GS_MODE_VGA_1280_60, GS_FRAME, makeDISPLAY(1023, 1279, 1,   1,   40,  350), makeSYNCV(3,  1024, 0,    38,  0,   1)},
        {GS_NONINTERLACED, GS_MODE_VGA_1280_75, GS_FRAME, makeDISPLAY(1023, 1279, 1,   1,   40,  350), makeSYNCV(3,  1024, 0,    38,  0,   1)}}; //ends predef_vmode definition
    // clang-format on
    int k576p_fix, kGsDxDyOffsetSupported, fd, FIELD_fix;
    char romver[16], romverNum[5], *pROMDate;

#ifdef _DTL_T10000
    if (predef_vmode[gGSMVMode].mode == GS_MODE_DTV_576P) // There is no 576P code implemented for development TOOLs.
        gGSMVMode = 2;                                    // Change to PAL instead.
#endif

    k576p_fix = 0;
    kGsDxDyOffsetSupported = 0;
    if ((fd = open("rom0:ROMVER", O_RDONLY)) >= 0) {
        // Read ROM version
        read(fd, romver, sizeof(romver));
        close(fd);

        strncpy(romverNum, romver, 4);
        romverNum[4] = '\0';

        // ROMVER string format: VVVVRTYYYYMMDD\n
        pROMDate = &romver[strlen(romver) - 9];

        /* Enable 576P add-on code for v2.00 and earlier. Note that the earlier PSX models already seem to support the 576P mode, despite being older than the SCPH-70000 series.
           1. The PSX (v1.80) has the same GS as the SCPH-75000 (v2.20), which is also shared with one of the SCPH-70000 (v2.00) models.
           2. The SCPH-50000a/b has v1.90, but that seemed to be actually older than the v1.80 from the early PSX models.
           3. All SCPH-50000 and SCPH-70000 series consoles do not support 576P mode.

           However, it should be harmless to use the add-on code, even on consoles that support it.
           Note that there are also PSX sets with v2.10, hence this check should only include v2.00 and earlier. */
        k576p_fix = (strtoul(romverNum, NULL, 10) < 210);

        // Record if the _GetGsDxDyOffset syscall is supported.
        kGsDxDyOffsetSupported = (strtoul(pROMDate, NULL, 10) > 20010608);
    }

    FIELD_fix = gGSMFIELDFix != 0 ? 1 : 0;

    sprintf(cmdline, "%hhu %hhu %hhu %llu %llu %hu %u %u %d %d %d", predef_vmode[gGSMVMode].interlace,
            predef_vmode[gGSMVMode].mode,
            predef_vmode[gGSMVMode].ffmd,
            predef_vmode[gGSMVMode].display,
            predef_vmode[gGSMVMode].syncv,
            ((predef_vmode[gGSMVMode].ffmd) << 1) | (predef_vmode[gGSMVMode].interlace),
            (u32)gGSMXOffset,
            (u32)gGSMYOffset,
            k576p_fix,
            kGsDxDyOffsetSupported,
            FIELD_fix);
}
