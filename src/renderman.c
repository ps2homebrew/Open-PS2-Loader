/*
  Copyright 2010, Volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <stdio.h>
#include <kernel.h>

#include "include/renderman.h"
#include "include/ioman.h"
#include "include/opl.h"

// Allocateable space in vram, as indicated in GsKit's code
#define __VRAM_SIZE 4194304

GSGLOBAL *gsGlobal;
s32 guiThreadID;

/** Helper texture list */
struct rm_texture_list_t
{
    GSTEXTURE *txt;
    GSCLUT *clut;
    struct rm_texture_list_t *next;
};

static struct rm_texture_list_t *uploadedTextures = NULL;

static int order;
static enum rm_vmode vmode = RM_VMODE_AUTO;

#define NUM_RM_VMODES 6

// RM Vmode -> GS Vmode conversion table
struct rm_mode
{
    char mode;
    char hsync; //In KHz
    short int width;
    short int height;
    short int VCK;
    short int interlace;
    short int field;
    short int PSM;
    short int aratio;
    short int PAR1; // Pixel Aspect Ratio 1 (For video modes with non-square pixels, like PAL/NTSC)
    short int PAR2; // Pixel Aspect Ratio 2 (For video modes with non-square pixels, like PAL/NTSC)
};

static struct rm_mode rm_mode_table[NUM_RM_VMODES] = {
    {-1,                 16,  640,   -1,  4, GS_INTERLACED,    GS_FIELD, GS_PSM_CT16S, RM_ARATIO_4_3,  1,  1}, // AUTO
    {GS_MODE_PAL,        16,  704,  576,  4, GS_INTERLACED,    GS_FIELD, GS_PSM_CT16S, RM_ARATIO_4_3, 11, 10}, // PAL@50Hz
    {GS_MODE_NTSC,       16,  704,  480,  4, GS_INTERLACED,    GS_FIELD, GS_PSM_CT24,  RM_ARATIO_4_3, 54, 59}, // NTSC@60Hz
    {GS_MODE_DTV_480P,   31,  704,  480,  2, GS_NONINTERLACED, GS_FRAME, GS_PSM_CT24,  RM_ARATIO_4_3,  1,  1}, // DTV480P@60Hz
    {GS_MODE_DTV_576P,   31,  704,  576,  2, GS_NONINTERLACED, GS_FRAME, GS_PSM_CT16S, RM_ARATIO_4_3,  1,  1}, // DTV576P@50Hz
    {GS_MODE_VGA_640_60, 31,  640,  480,  2, GS_NONINTERLACED, GS_FRAME, GS_PSM_CT24,  RM_ARATIO_4_3,  1,  1}, // VGA640x480@60Hz
};

// Display Aspect Ratio
static int iAspectWidth = 4;
static enum rm_aratio DAR = RM_ARATIO_4_3;

// Display dimensions after overscan compensation
static int iDisplayWidth;
static int iDisplayHeight;
static int iDisplayXOff;
static int iDisplayYOff;

// Transposition values - all rendering will be transposed (moved on screen) by these
static float transX = 0.0f;
static float transY = 0.0f;

// Transposition values including overscan compensation
static float fRenderXOff = 0.0f;
static float fRenderYOff = 0.0f;

const u64 gColWhite = GS_SETREG_RGBA(0xFF, 0xFF, 0xFF, 0x80);   // Alpha 0x80 -> solid white
const u64 gColBlack = GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x80);   // Alpha 0x80 -> solid black
const u64 gColDarker = GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x60);  // Alpha 0x60 -> transparent overlay color
const u64 gColFocus = GS_SETREG_RGBA(0xFF, 0xFF, 0xFF, 0x50);   // Alpha 0x50 -> transparent overlay color

const u64 gDefaultCol = GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80); // Special color for texture multiplication
const u64 gDefaultAlpha = GS_SETREG_ALPHA(0, 1, 0, 1, 0);

static void rmAppendUploadedTextures(GSTEXTURE *txt)
{
    struct rm_texture_list_t *entry = (struct rm_texture_list_t *)malloc(sizeof(struct rm_texture_list_t));
    entry->clut = NULL;
    entry->txt = txt;
    entry->next = uploadedTextures;
    uploadedTextures = entry;
}

static void rmAppendUploadedCLUTs(GSCLUT *clut)
{
    struct rm_texture_list_t *entry = (struct rm_texture_list_t *)malloc(sizeof(struct rm_texture_list_t));
    entry->txt = NULL;
    entry->clut = clut;
    entry->next = uploadedTextures;
    uploadedTextures = entry;
}

static int rmClutSize(GSCLUT *clut, u32 *size, u32 *w, u32 *h)
{
    switch (clut->PSM) {
        case GS_PSM_T4:
            *w = 8;
            *h = 2;
            break;
        case GS_PSM_T8:
            *w = 16;
            *h = 16;
            break;
        default:
            return 0;
    };

    switch (clut->ClutPSM) {
        case GS_PSM_CT32:
            *size = (*w) * (*h) * 4;
            break;
        case GS_PSM_CT24:
            *size = (*w) * (*h) * 4;
            break;
        case GS_PSM_CT16:
            *size = (*w) * (*h) * 2;
            break;
        case GS_PSM_CT16S:
            *size = (*w) * (*h) * 2;
            break;
        default:
            return 0;
    }

    return 1;
}

static int rmUploadClut(GSCLUT *clut)
{
    if (clut->VramClut && clut->VramClut != GSKIT_ALLOC_ERROR) // already uploaded
        return 1;

    u32 size;
    u32 w, h;

    if (!rmClutSize(clut, &size, &w, &h))
        return 0;

    size = (-GS_VRAM_BLOCKSIZE_256) & (size + GS_VRAM_BLOCKSIZE_256 - 1);

    // too large to fit VRAM with the currently allocated space?
    if (gsGlobal->CurrentPointer + size >= __VRAM_SIZE) {
        if (size >= __VRAM_SIZE) {
            // Only log this if the allocation is too large itself
            LOG("RENDERMAN Requested clut allocation is bigger than VRAM!\n");
            // We won't allocate this, it's too large
            clut->VramClut = GSKIT_ALLOC_ERROR;
            return 0;
        }

        rmFlush();
    }

    clut->VramClut = gsGlobal->CurrentPointer;
    gsGlobal->CurrentPointer += size;

    rmAppendUploadedCLUTs(clut);

    SyncDCache(clut->Clut, (u8 *)(clut->Clut) + size);
    gsKit_texture_send_inline(gsGlobal, clut->Clut, w, h, clut->VramClut, clut->ClutPSM, 1, GS_CLUT_PALLETE);
    return 1;
}

static int rmUploadTexture(GSTEXTURE *txt)
{
    // For clut based textures...
    if (txt->Clut) {
        // upload CLUT first
        if (!rmUploadClut((GSCLUT *)txt->Clut))
            return 0;

        // copy the new VramClut
        txt->VramClut = ((GSCLUT *)txt->Clut)->VramClut;
    }

    u32 size = gsKit_texture_size(txt->Width, txt->Height, txt->PSM);
    // alignment of the allocation
    size = (-GS_VRAM_BLOCKSIZE_256) & (size + GS_VRAM_BLOCKSIZE_256 - 1);

    // too large to fit VRAM with the currently allocated space?
    if (gsGlobal->CurrentPointer + size >= __VRAM_SIZE) {
        if (size >= __VRAM_SIZE) {
            // Only log this if the allocation is too large itself
            LOG("RENDERMAN Requested texture allocation is bigger than VRAM!\n");
            // We won't allocate this, it's too large
            txt->Vram = GSKIT_ALLOC_ERROR;
            return 0;
        }

        rmFlush();

        // Should not flush CLUT away. If this happenned we have to reupload
        if (txt->Clut) {
            if (!rmUploadClut((GSCLUT *)txt->Clut))
                return 0;

            txt->VramClut = ((GSCLUT *)txt->Clut)->VramClut;
        }

        // only could fit CLUT but not the pixmap with it!
        if (gsGlobal->CurrentPointer + size >= __VRAM_SIZE)
            return 0;
    }

    txt->Vram = gsGlobal->CurrentPointer;
    gsGlobal->CurrentPointer += size;

    rmAppendUploadedTextures(txt);

    // We can't do gsKit_texture_upload since it'd assume txt->Clut is the CLUT table directly
    // whereas we're using it as a pointer to our structure containg clut data
    gsKit_setup_tbw(txt);
    SyncDCache(txt->Mem, (u8 *)(txt->Mem) + size);
    gsKit_texture_send_inline(gsGlobal, txt->Mem, txt->Width, txt->Height, txt->Vram, txt->PSM, txt->TBW, txt->Clut ? GS_CLUT_TEXTURE : GS_CLUT_NONE);

    return 1;
}

int rmPrepareTexture(GSTEXTURE *txt)
{
    //Upload, only if not already uploaded.
    return ((txt->Vram && txt->Vram != GSKIT_ALLOC_ERROR) ? 1 : rmUploadTexture(txt));
}

void rmDispatch(void)
{
    gsKit_queue_exec(gsGlobal);
}

void rmFlush(void)
{
    rmDispatch();

    // release all the uploaded textures
    gsKit_vram_clear(gsGlobal);

    while (uploadedTextures) {
        // free clut and txt if those are filled in
        if (uploadedTextures->txt) {
            uploadedTextures->txt->Vram = 0;
            uploadedTextures->txt->VramClut = 0;
        }

        if (uploadedTextures->clut)
            uploadedTextures->clut->VramClut = 0;

        struct rm_texture_list_t *entry = uploadedTextures;
        uploadedTextures = uploadedTextures->next;
        free(entry);
    }
}

void rmStartFrame(void)
{
    gsKit_clear(gsGlobal, gColBlack);
    order = 0;
}

void rmEndFrame(void)
{
    gsKit_set_finish(gsGlobal);

    rmFlush();

    // Wait for draw ops to finish
    gsKit_finish();

    if (!gsGlobal->FirstFrame) {
        SleepThread();

        if (gsGlobal->DoubleBuffering == GS_SETTING_ON) {
            GS_SET_DISPFB2(gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192,
                           gsGlobal->Width / 64, gsGlobal->PSM, 0, 0);

            gsGlobal->ActiveBuffer ^= 1;
        }
    }

    gsKit_setactive(gsGlobal);
}

static int rmOnVSync(void)
{
    iWakeupThread(guiThreadID);

    return 0;
}

void rmInit()
{
    gsGlobal = gsKit_init_global();

    rm_mode_table[RM_VMODE_AUTO].mode = gsGlobal->Mode;
    rm_mode_table[RM_VMODE_AUTO].height = gsGlobal->Height;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

    // Initialize the DMAC
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    rmSetMode(1);

    order = 0;

    guiThreadID = GetThreadId();
    gsKit_add_vsync_handler(&rmOnVSync);
}

int rmSetMode(int force)
{
    if (gVMode < RM_VMODE_AUTO || gVMode >= NUM_RM_VMODES)
        gVMode = RM_VMODE_AUTO;

    // we don't want to set the vmode without a reason...
    int changed = (vmode != gVMode || force);
    if (changed) {
        vmode = gVMode;

        gsGlobal->Mode = rm_mode_table[vmode].mode;
        gsGlobal->Width = rm_mode_table[vmode].width;
        gsGlobal->Height = rm_mode_table[vmode].height;
        gsGlobal->Interlace = rm_mode_table[vmode].interlace;
        gsGlobal->Field = rm_mode_table[vmode].field;
        gsGlobal->PSM = rm_mode_table[vmode].PSM;
        gsGlobal->PSMZ = GS_PSMZ_16S;
        gsGlobal->ZBuffering = GS_SETTING_OFF;
        gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
        gsGlobal->DoubleBuffering = GS_SETTING_ON;
        gsGlobal->Dithering = GS_SETTING_ON;

        if ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
            gsGlobal->Height /= 2;

        gsKit_init_screen(gsGlobal);

        rmSetDisplayOffset(gXOff, gYOff);
        rmSetOverscan(gOverscan);

        gsKit_mode_switch(gsGlobal, GS_ONESHOT);

        gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
        gsKit_set_primalpha(gsGlobal, gDefaultAlpha, 0);

        // reset the contents of the screen to avoid garbage being displayed
        gsKit_clear(gsGlobal, gColBlack);
        gsKit_sync_flip(gsGlobal);

        LOG("RENDERMAN New vmode: %d, %d x %d\n", vmode, gsGlobal->Width, gsGlobal->Height);
    }

    return changed;
}

void rmGetScreenExtentsNative(int *w, int *h)
{
    *w = iDisplayWidth;
    *h = iDisplayHeight;
}

void rmGetScreenExtents(int *w, int *h)
{
    // Emulate 640x480 (square pixel VGA)
    *w = 640;
    *h = 480;
}

void rmEnd(void)
{
    rmFlush();
    gsKit_deinit_global(gsGlobal);
}

#define X_SCALE(x) (((x)*iDisplayWidth) /640)
#define Y_SCALE(y) (((y)*iDisplayHeight)/480)
/** If txt is null, don't use DIM_UNDEF size */
static void rmSetupQuad(GSTEXTURE *txt, int x, int y, short aligned, int w, int h, short scaled, u64 color, rm_quad_t *q)
{
    if (w == DIM_UNDEF)
        w = txt->Width;
    if (h == DIM_UNDEF)
        h = txt->Height;

    // Legacy scaling
    x = X_SCALE(x);
    y = Y_SCALE(y);
    if (scaled & SCALING_RATIO)
        w = X_SCALE(w * iAspectWidth) >> 2;
    else
        w = X_SCALE(w);
    h = Y_SCALE(h);

    // Align LEFT/HCENTER/RIGHT
    if (aligned & ALIGN_HCENTER)
        q->ul.x = x - (w >> 1);
    else if (aligned & ALIGN_RIGHT)
        q->ul.x = x - w;
    else
        q->ul.x = x;
    q->br.x = q->ul.x + w;

    // Align TOP/VCENTER/BOTTOM
    if (aligned & ALIGN_VCENTER)
        q->ul.y = y - (h >> 1);
    else if (aligned & ALIGN_BOTTOM)
        q->ul.y = y - h;
    else
        q->ul.y = y;
    q->br.y = q->ul.y + h;

    q->color = color;
    if (txt) {
        q->txt = txt;
        q->ul.u = 0;
        q->ul.v = 0;
        q->br.u = txt->Width;
        q->br.v = txt->Height;
    }
}

void rmDrawQuad(rm_quad_t *q)
{
    if (!rmPrepareTexture(q->txt)) // won't render if not ready!
        return;

    if ((q->txt->PSM == GS_PSM_CT32) || (q->txt->Clut && q->txt->ClutPSM == GS_PSM_CT32))
        gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
    else
        gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;

    gsKit_prim_sprite_texture(gsGlobal, q->txt,
                              q->ul.x + fRenderXOff, q->ul.y + fRenderYOff,
                              q->ul.u, q->ul.v,
                              q->br.x + fRenderXOff, q->br.y + fRenderYOff,
                              q->br.u, q->br.v, order, q->color);
    order++;
}

void rmDrawPixmap(GSTEXTURE *txt, int x, int y, short aligned, int w, int h, short scaled, u64 color)
{
    rm_quad_t quad;
    rmSetupQuad(txt, x, y, aligned, w, h, scaled, color, &quad);
    rmDrawQuad(&quad);
}

void rmDrawOverlayPixmap(GSTEXTURE *overlay, int x, int y, short aligned, int w, int h, short scaled, u64 color,
                         GSTEXTURE *inlay, int ulx, int uly, int urx, int ury, int blx, int bly, int brx, int bry)
{
    rm_quad_t quad;
    rmSetupQuad(overlay, x, y, aligned, w, h, scaled, color, &quad);
    ulx = X_SCALE(ulx * iAspectWidth) >> 2;
    urx = X_SCALE(urx * iAspectWidth) >> 2;
    blx = X_SCALE(blx * iAspectWidth) >> 2;
    brx = X_SCALE(brx * iAspectWidth) >> 2;
    uly = Y_SCALE(uly);
    ury = Y_SCALE(ury);
    bly = Y_SCALE(bly);
    bry = Y_SCALE(bry);

    if (!rmPrepareTexture(inlay))
        return;

    if ((inlay->PSM == GS_PSM_CT32) || (inlay->Clut && inlay->ClutPSM == GS_PSM_CT32))
        gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
    else
        gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;

    gsKit_prim_quad_texture(gsGlobal, inlay,
                            quad.ul.x + ulx + fRenderXOff, quad.ul.y + uly + fRenderYOff,
                            0.0f, 0.0f,
                            quad.ul.x + urx + fRenderXOff, quad.ul.y + ury + fRenderYOff,
                            inlay->Width, 0.0f,
                            quad.ul.x + blx + fRenderXOff, quad.ul.y + bly + fRenderYOff,
                            0.0f, inlay->Height,
                            quad.ul.x + brx + fRenderXOff, quad.ul.y + bry + fRenderYOff,
                            inlay->Width, inlay->Height, order, gDefaultCol);
    order++;

    rmDrawQuad(&quad);
}

void rmDrawRect(int x, int y, int w, int h, u64 color)
{
    float fx = X_SCALE(x) + fRenderXOff;
    float fy = Y_SCALE(y) + fRenderYOff;
    float fw = X_SCALE(w);
    float fh = Y_SCALE(h);

    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
    gsKit_prim_sprite(gsGlobal, fx, fy, fx + fw, fy + fh, order, color);
    order++;
}

void rmDrawLine(int x1, int y1, int x2, int y2, u64 color)
{
    float fx1 = X_SCALE(x1) + fRenderXOff;
    float fy1 = Y_SCALE(y1) + fRenderYOff;
    float fx2 = X_SCALE(x2) + fRenderXOff;
    float fy2 = Y_SCALE(y2) + fRenderYOff;

    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
    gsKit_prim_line(gsGlobal, fx1, fy1, fx2, fy2, order, color);
    order++;
}

void rmSetDisplayOffset(int x, int y)
{
    gsKit_set_display_offset(gsGlobal, x * rm_mode_table[vmode].VCK, y);
}

void rmSetAspectRatio(enum rm_aratio dar)
{
    DAR = dar;

    switch(DAR) {
        case RM_ARATIO_4_3:
            iAspectWidth = 4; // width = width * 4 / 4
            break;
        case RM_ARATIO_16_9:
            iAspectWidth = 3; // width = width * 3 / 4
            break;
    };
}

int rmWideScale(int x)
{
    return (x * iAspectWidth) >> 2;
}

// Get the pixel aspect ratio (how wide or narrow are the pixels?)
float rmGetPAR()
{
    float fPAR = (float)rm_mode_table[vmode].PAR1 / (float)rm_mode_table[vmode].PAR2;

    // In anamorphic mode the pixels are stretched to 16:9
    if ((DAR == RM_ARATIO_16_9) && (rm_mode_table[vmode].aratio == RM_ARATIO_4_3))
        fPAR *= 0.75f;

    // In interlaced frame mode, the pixel are (virtually) twice as high
    // FIXME: this looks ugly!
    //   we need the font to render at 1920x1080 instead of 1920x540
    if ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
        fPAR *= 2.0f;

    return fPAR;
}

int rmScaleX(int x)
{
    return X_SCALE(x);
}

int rmScaleY(int y)
{
    return Y_SCALE(y);
}

int rmUnScaleX(int x)
{
    return (x*640)/iDisplayWidth;
}

int rmUnScaleY(int y)
{
    return (y*480)/iDisplayHeight;
}

static void rmUpdateRenderOffsets()
{
    fRenderXOff = (float)iDisplayXOff + transX - 0.5f;
    fRenderYOff = (float)iDisplayYOff + transY - 0.5f;
}

void rmSetOverscan(int overscan)
{
    iDisplayXOff = (gsGlobal->Width  * overscan) / (2 * 1000);
    iDisplayYOff = (gsGlobal->Height * overscan) / (2 * 1000);
    iDisplayWidth  = gsGlobal->Width  - (2 * iDisplayXOff);
    iDisplayHeight = gsGlobal->Height - (2 * iDisplayYOff);
    rmUpdateRenderOffsets();
}

void rmSetTransposition(float x, float y)
{
    transX = X_SCALE(x);
    transY = Y_SCALE(y);
    rmUpdateRenderOffsets();
}

unsigned char rmGetHsync(void)
{
    return rm_mode_table[vmode].hsync;
}
