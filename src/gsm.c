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
#include "../ee_core/include/coreconfig.h"

#include "include/pggsm.h"

static int gEnableGSM;       // Enables GSM - 0 for Off, 1 for On
static int gGSMVMode;        // See the related predef_vmode
static int gGSMXOffset;      // 0 - Off, Any other positive or negative value - Relative position for X Offset
static int gGSMYOffset;      // 0 - Off, Any other positive or negative value - Relative position for Y Offset
static int gGSMFIELDFix;     // Enables/disables the FIELD flipping emulation option. 0 for Off, 1 for On.
static int gGSMPixelPerfect; // Manual pixel perfect: 1 = apply gGSMVck, gGSMDw and gGSMDx. 0 for Off.
static int gGSMPPAuto;       // Automatic pixel perfect: 1 = ee_core calculates VCK, DW and DX in game. 0 for Off.
static int gGSMVck;          // VCK per pixel (DISPLAY.MAGH + 1), 1-16. 0 = not overridden.
static int gGSMDw;           // Raw DISPLAY.DW (width in VCK - 1), 1-4095. 0 = not overridden.
static int gGSMDx;           // Raw DISPLAY.DX (start of the image in VCK), 1-4095. 0 = not overridden.

// Pre-defined vmodes. Moved out of PrepareGSM() so that GSMCalcFromWidth() can read the DISPLAY values of the selected mode.

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

// Pixel perfect: offset added to the DX of the video mode before centring a narrower image.
// The NTSC DX (700) put the picture slightly to the right on a PS2-to-HDMI converter, 624 (-76) centres it.
// The same value is used by ee_core for Automatic pixel perfect: keep it in sync with PP_DX_BIAS in gsm_engine.S.
#define GSM_CALC_DX_BIAS (-76)

// Pixel perfect is only offered for the NTSC and PAL modes (the first 6 entries of predef_vmode):
// it was designed and tested for PS2-to-HDMI converters fed with 480i/576i signals.
int GSMIsPixelPerfectMode(int vmode)
{
    return (predef_vmode[vmode].mode == GS_MODE_NTSC || predef_vmode[vmode].mode == GS_MODE_PAL);
}

// Pixel perfect calculator, used by the "Game Internal Width" field of the GSM menu.
// Every game pixel gets the same VCK count as a 640px game in the selected mode (MAGH + 1 of the mode),
// so PS2-to-HDMI converters that sample at that rate never duplicate pixels unevenly. A narrower game
// therefore gets a narrower DW, and DX moves the picture to the centre of the mode. For NTSC:
//   512px -> VCK 4, DW 512 * 4 - 1 = 2047, DX 700 - 76 + (2559 - 2047) / 2 = 880
//   640px -> VCK 4, DW 2559, DX 624
// ee_core does the same calculation for Automatic pixel perfect (see gsm_engine.S).
void GSMCalcFromWidth(int vmode, int width, int *vck, int *dw, int *dx)
{
    u64 display = predef_vmode[vmode].display;
    int modeVck = (int)((display >> 23) & 0x0F) + 1;         // MAGH + 1: VCK per pixel of a 640px game in this mode
    int modeDW = (int)((display >> 32) & 0x0FFF);            // full width of the mode, in VCK - 1
    int modeDX = (int)(display & 0x0FFF) + GSM_CALC_DX_BIAS; // reference DX for a full width image
    int newDW = width * modeVck - 1;
    int newDX;

    if (newDW > 0x0FFF)
        newDW = 0x0FFF;
    newDX = modeDX + (modeDW - newDW) / 2; // half of the unused width on each side
    if (newDX < 0)
        newDX = 0;
    if (newDX > 0x0FFF)
        newDX = 0x0FFF;

    *vck = modeVck;
    *dw = newDW;
    *dx = newDX;
}

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
    gGSMPixelPerfect = 0;
    gGSMPPAuto = 0;
    gGSMVck = 4; // Manual pixel perfect defaults: the values for a 512px wide game in NTSC
    gGSMDw = 2047;
    gGSMDx = 880;

    if (configGetInt(configSet, CONFIG_ITEM_GSMSOURCE, &gGSMSource)) {
        // Load the rest of the per-game GSM configuration, only if GSM is enabled.
        if (configGetInt(configSet, CONFIG_ITEM_ENABLEGSM, &gEnableGSM) && gEnableGSM) {
            configGetInt(configSet, CONFIG_ITEM_GSMVMODE, &gGSMVMode);
            configGetInt(configSet, CONFIG_ITEM_GSMXOFFSET, &gGSMXOffset);
            configGetInt(configSet, CONFIG_ITEM_GSMYOFFSET, &gGSMYOffset);
            configGetInt(configSet, CONFIG_ITEM_GSMFIELDFIX, &gGSMFIELDFix);
            configGetInt(configSet, CONFIG_ITEM_GSMPIXELPERFECT, &gGSMPixelPerfect);
            configGetInt(configSet, CONFIG_ITEM_GSMPPAUTO, &gGSMPPAuto);
            configGetInt(configSet, CONFIG_ITEM_GSMVCK, &gGSMVck);
            configGetInt(configSet, CONFIG_ITEM_GSMDW, &gGSMDw);
            configGetInt(configSet, CONFIG_ITEM_GSMDX, &gGSMDx);
        }
    } else {
        if (configGetInt(configGame, CONFIG_ITEM_ENABLEGSM, &gEnableGSM) && gEnableGSM) {
            configGetInt(configGame, CONFIG_ITEM_GSMVMODE, &gGSMVMode);
            configGetInt(configGame, CONFIG_ITEM_GSMXOFFSET, &gGSMXOffset);
            configGetInt(configGame, CONFIG_ITEM_GSMYOFFSET, &gGSMYOffset);
            configGetInt(configGame, CONFIG_ITEM_GSMFIELDFIX, &gGSMFIELDFix);
            configGetInt(configGame, CONFIG_ITEM_GSMPIXELPERFECT, &gGSMPixelPerfect);
            configGetInt(configGame, CONFIG_ITEM_GSMPPAUTO, &gGSMPPAuto);
            configGetInt(configGame, CONFIG_ITEM_GSMVCK, &gGSMVck);
            configGetInt(configGame, CONFIG_ITEM_GSMDW, &gGSMDw);
            configGetInt(configGame, CONFIG_ITEM_GSMDX, &gGSMDx);
        }
    }
}

int GetGSMEnabled(void)
{
    return gEnableGSM;
}

void PrepareGSM(char *cmdline, struct GsmConfig_t *config)
{
    /* Preparing GSM */
    LOG("Preparing GSM...\n");
    // Pre-defined vmodes
    // Some of following vmodes result in no display and/or freezing, depending on the console kernel and GS revision.
    // Therefore there are many variables involved here that can lead us to success or fail, depending on the already mentioned circumstances.
    //
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
    // Pixel perfect: both modes are ignored outside NTSC and PAL, even if they were saved as on.
    if (!GSMIsPixelPerfectMode(gGSMVMode)) {
        gGSMPixelPerfect = 0;
        gGSMPPAuto = 0;
    }
    // The VCK, DW and DX overrides are only sent with Manual pixel perfect on.
    // Automatic pixel perfect calculates them in ee_core, and with both off GSM behaves as before.
    if (gGSMPixelPerfect == 0 || gGSMPPAuto != 0) {
        gGSMVck = 0;
        gGSMDw = 0;
        gGSMDx = 0;
    }
    // Clamp the manual overrides to what the DISPLAY fields can hold (MAGH: 4 bits, DW/DX: 12 bits).
    if (gGSMVck < 0 || gGSMVck > 16)
        gGSMVck = 0;
    if (gGSMDw < 0 || gGSMDw > 0xFFF)
        gGSMDw = 0;
    if (gGSMDx < 0 || gGSMDx > 0xFFF)
        gGSMDx = 0;

    if (cmdline) {
        sprintf(cmdline, "%hhu %hhu %hhu %llu %llu %hu %u %u %d %d %d %d %d %d %d", predef_vmode[gGSMVMode].interlace,
                predef_vmode[gGSMVMode].mode,
                predef_vmode[gGSMVMode].ffmd,
                predef_vmode[gGSMVMode].display,
                predef_vmode[gGSMVMode].syncv,
                ((predef_vmode[gGSMVMode].ffmd) << 1) | (predef_vmode[gGSMVMode].interlace),
                (u32)gGSMXOffset,
                (u32)gGSMYOffset,
                k576p_fix,
                kGsDxDyOffsetSupported,
                FIELD_fix,
                gGSMPPAuto != 0 ? 1 : 0,
                gGSMVck,
                gGSMDw,
                gGSMDx);
    }

    if (config) {
        config->interlace = predef_vmode[gGSMVMode].interlace;
        config->mode = predef_vmode[gGSMVMode].mode;
        config->ffmd = predef_vmode[gGSMVMode].ffmd;
        config->display = predef_vmode[gGSMVMode].display;
        config->syncv = predef_vmode[gGSMVMode].syncv;
        config->smode2 = ((predef_vmode[gGSMVMode].ffmd) << 1) | (predef_vmode[gGSMVMode].interlace);
        config->dx_offset = (u32)gGSMXOffset;
        config->dy_offset = (u32)gGSMYOffset;
        config->k576P_fix = k576p_fix;
        config->kGsDxDyOffsetSupported = kGsDxDyOffsetSupported;
        config->FIELD_fix = FIELD_fix;
        config->PPAuto_fix = gGSMPPAuto != 0 ? 1 : 0;
        config->Vck_override = gGSMVck;
        config->Dw_override = gGSMDw;
        config->Dx_override = gGSMDx;
    }
}
