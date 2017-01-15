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
    short int height;
    short int VCK;
};

static struct rm_mode rm_mode_table[NUM_RM_VMODES] = {
    {-1, 16, -1, -1},                  // AUTO
    {GS_MODE_PAL, 16, 512, 4},         // PAL@50Hz
    {GS_MODE_NTSC, 16, 448, 4},        // NTSC@60Hz
    {GS_MODE_DTV_480P, 31, 448, 2},    // DTV480P@60Hz
    {GS_MODE_DTV_576P, 31, 512, 2},    // DTV576P@50Hz
    {GS_MODE_VGA_640_60, 31, 480, 2},  // VGA640x480@60Hz
};

static float aspectWidth;
static float aspectHeight;

// Transposition values - all rendering can be transposed (moved on screen) by these
static float transX = 0;
static float transY = 0;

const u64 gColWhite = GS_SETREG_RGBA(0xFF, 0xFF, 0xFF, 0x00);
const u64 gColBlack = GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x00);
const u64 gColDarker = GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x60);
const u64 gColFocus = GS_SETREG_RGBA(0xFF, 0xFF, 0xFF, 0x50);

const u64 gDefaultCol = GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80);
const u64 gDefaultAlpha = GS_SETREG_ALPHA(0, 1, 0, 1, 0);

static float shiftYVal;
static float (*shiftY)(float posY);

static float shiftYFunc(float posY)
{
    return (int)(shiftYVal * posY);
}

static float identityFunc(float posY)
{
    return posY;
}

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
            gsGlobal->PrimContext ^= 1;
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

    aspectWidth = 1.0f;
    aspectHeight = 1.0f;

    shiftYVal = 1.0f;
    shiftY = &shiftYFunc;

    transX = 0.0f;
    transY = 0.0f;

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
        gsGlobal->Height = rm_mode_table[vmode].height;

        if (vmode == RM_VMODE_DTV480P || vmode == RM_VMODE_DTV576P || vmode == RM_VMODE_VGA_640_60) {
            gsGlobal->Interlace = GS_NONINTERLACED;
            gsGlobal->Field = GS_FRAME;
        } else {
            gsGlobal->Interlace = GS_INTERLACED;
            gsGlobal->Field = GS_FIELD;
        }
        gsGlobal->Width = 640;

        gsGlobal->PSM = GS_PSM_CT24;
        gsGlobal->PSMZ = GS_PSMZ_16S;
        gsGlobal->ZBuffering = GS_SETTING_OFF;
        gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
        gsGlobal->DoubleBuffering = GS_SETTING_ON;

        gsKit_init_screen(gsGlobal);

        rmSetDisplayOffset(gXOff, gYOff);

        gsKit_mode_switch(gsGlobal, GS_ONESHOT);

        gsKit_set_test(gsGlobal, GS_ZTEST_OFF);

        // reset the contents of the screen to avoid garbage being displayed
        gsKit_clear(gsGlobal, gColBlack);
        gsKit_sync_flip(gsGlobal);

        LOG("RENDERMAN New vmode: %d, %d x %d\n", vmode, gsGlobal->Width, gsGlobal->Height);
    }
    return changed;
}

void rmGetScreenExtents(int *w, int *h)
{
    *w = gsGlobal->Width;
    *h = gsGlobal->Height;
}

void rmEnd(void)
{
    rmFlush();
}

/** If txt is null, don't use DIM_UNDEF size */
void rmSetupQuad(GSTEXTURE *txt, int x, int y, short aligned, int w, int h, short scaled, u64 color, rm_quad_t *q)
{
    if (aligned) {
        float dim;
        if (w == DIM_UNDEF)
            w = txt->Width;
        if (h == DIM_UNDEF)
            h = txt->Height;

        if (scaled)
            dim = aspectWidth * (w >> 1);
        else
            dim = w >> 1;
        q->ul.x = x - dim;
        q->br.x = x + dim;

        if (scaled)
            dim = aspectHeight * (h >> 1);
        else
            dim = h >> 1;
        q->ul.y = shiftY(y) - dim;
        q->br.y = shiftY(y) + dim;
    } else {
        if (w == DIM_UNDEF)
            w = txt->Width;
        if (h == DIM_UNDEF)
            h = txt->Height;

        q->ul.x = x;
        if (scaled)
            q->br.x = x + aspectWidth * w;
        else
            q->br.x = x + w;

        q->ul.y = shiftY(y);
        if (scaled)
            q->br.y = shiftY(y) + aspectHeight * h;
        else
            q->br.y = shiftY(y) + h;
    }

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
{                                  // NO scaling, NO shift, NO alignment
    if (!rmPrepareTexture(q->txt)) // won't render if not ready!
        return;

    if ((q->txt->PSM == GS_PSM_CT32) || (q->txt->Clut && q->txt->ClutPSM == GS_PSM_CT32)) {
        gsKit_set_primalpha(gsGlobal, gDefaultAlpha, 0);
    }

    gsKit_prim_sprite_texture(gsGlobal, q->txt,
                              q->ul.x + transX, q->ul.y + transY,
                              q->ul.u + 0.5f, q->ul.v + 0.5f,
                              q->br.x + transX, q->br.y + transY,
                              q->br.u - 0.375f, q->br.v + 0.375f, order, q->color);
    order++;

    gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
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

    if (!rmPrepareTexture(inlay))
        return;

    if (inlay->PSM == GS_PSM_CT32)
        gsKit_set_primalpha(gsGlobal, gDefaultAlpha, 0);

    gsKit_prim_quad_texture(gsGlobal, inlay, quad.ul.x + transX + aspectWidth * ulx, quad.ul.y + transY + uly, 0.5f, 0.5f,
                            quad.ul.x + transX + aspectWidth * urx, quad.ul.y + transY + ury, inlay->Width - 0.375f, 0.5f,
                            quad.ul.x + transX + aspectWidth * blx, quad.ul.y + transY + bly, 0.5f, inlay->Height - 0.375f,
                            quad.ul.x + transX + aspectWidth * brx, quad.ul.y + transY + bry, inlay->Width - 0.375f, inlay->Height - 0.375f, order, gDefaultCol);
    order++;
    gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);

    rmDrawQuad(&quad);
}

void rmDrawRect(int x, int y, int w, int h, u64 color)
{
    gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    gsKit_prim_quad(gsGlobal, x + transX, shiftY(y) + transY, x + w + transX, shiftY(y) + transY, x + transX, shiftY(y) + h + transY, x + w + transX, shiftY(y) + h + transY, order, color);
    order++;
    gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
}

void rmDrawLine(int x, int y, int x1, int y1, u64 color)
{
    gsKit_prim_line(gsGlobal, x + transX, shiftY(y) + transY, x1 + transX, shiftY(y1) + transY, order, color);
}

void rmSetDisplayOffset(int x, int y)
{
    gsKit_set_display_offset(gsGlobal, x * rm_mode_table[vmode].VCK, y);
}

void rmSetAspectRatio(float width, float height)
{
    aspectWidth = width;
    aspectHeight = height;
}

void rmResetAspectRatio()
{
    aspectWidth = 1.0f;
    aspectHeight = 1.0f;
}

void rmGetAspectRatio(float *w, float *h)
{
    *w = aspectWidth;
    *h = aspectHeight;
}

void rmApplyAspectRatio(int *w, int *h)
{
    *w = *w * aspectWidth;
    *h = *h * aspectHeight;
}

void rmSetShiftRatio(float shiftYRatio)
{
    shiftYVal = shiftYRatio;
    shiftY = &shiftYFunc;
}

void rmResetShiftRatio()
{
    shiftY = &identityFunc;
}

void rmApplyShiftRatio(int *y)
{
    *y = shiftY(*y);
}

void rmSetTransposition(float x, float y)
{
    transX = x;
    transY = y;
}

unsigned char rmGetHsync(void)
{
    return rm_mode_table[vmode].hsync;
}
