/*
 Copyright 2010, Volca
 Licenced under Academic Free License version 3.0
 Review OpenUsbLd README & LICENSE files for further details.
 */

#include "include/opl.h"
#include "include/fntsys.h"
#include "include/renderman.h"
#include "include/ioman.h"
#include "include/utf8.h"
#include "include/util.h"
#include "include/atlas.h"

#include <sys/types.h>
#include <ft2build.h>

#include FT_FREETYPE_H

extern void *poeveticanew_raw;
extern int size_poeveticanew_raw;

/// Maximal count of atlases per font
#define ATLAS_MAX    4
/// Atlas width in pixels
#define ATLAS_WIDTH  256
/// Atlas height in pixels
#define ATLAS_HEIGHT 256

// freetype vars
static FT_Library font_library;

static s32 gFontSemaId;
static ee_sema_t gFontSema;

static GSCLUT fontClut;

static const float fDPI = 72.0f;

/** Single entry in the glyph cache */
typedef struct
{
    int isValid;
    // size in pixels of the glyph
    int width, height;
    // offsetting of the glyph
    int ox, oy;
    // advancements in pixels after rendering this glyph
    int shx, shy;

    // atlas for which the allocation was done
    atlas_t *atlas;

    // atlas allocation position
    struct atlas_allocation_t *allocation;

} fnt_glyph_cache_entry_t;

/** A whole font definition */
typedef struct
{
    /** GLYPH CACHE. Every glyph in the ASCII range is cached when first used
     * this means no additional memory aside from the one needed to render the
     * character set is used.
     */
    fnt_glyph_cache_entry_t **glyphCache;

    /// Maximal font cache page index
    int cacheMaxPageID;

    /// Font face
    FT_Face face;

    /// Nonzero if font is used
    int isValid;

    int fontSize;

    /// Texture atlases (default to NULL)
    atlas_t *atlases[ATLAS_MAX];

    /// Pointer to data, if allocation takeover was selected (will be freed)
    void *dataPtr;
} font_t;

#define FNT_MAX_COUNT (16)

/// Array of font definitions
static font_t fonts[FNT_MAX_COUNT];

static rm_quad_t quad;
static uint32_t codepoint, state;
static fnt_glyph_cache_entry_t *glyph;
static FT_Bool use_kerning;
static FT_UInt glyph_index, previous;
static FT_Vector delta;

#define GLYPH_CACHE_PAGE_SIZE 256

#define GLYPH_PAGE_OK(font, page) ((pageid <= font->cacheMaxPageID) && (font->glyphCache[page]))

static void fntCacheFlushPage(fnt_glyph_cache_entry_t *page)
{
    int i;

    for (i = 0; i < GLYPH_CACHE_PAGE_SIZE; ++i, ++page) {
        page->isValid = 0;
        // we're not doing any atlasFree or such - atlas has to be rebuild
        page->allocation = NULL;
        page->atlas = NULL;
    }
}

static void fntCacheFlush(font_t *font)
{
    // Release all the glyphs from the cache
    int i;
    for (i = 0; i <= font->cacheMaxPageID; ++i) {
        if (font->glyphCache[i]) {
            fntCacheFlushPage(font->glyphCache[i]);
            free(font->glyphCache[i]);
            font->glyphCache[i] = NULL;
        }
    }

    free(font->glyphCache);
    font->glyphCache = NULL;
    font->cacheMaxPageID = -1;

    // free all atlasses too, they're invalid now anyway
    int aid;
    for (aid = 0; aid < ATLAS_MAX; ++aid) {
        atlasFree(font->atlases[aid]);
        font->atlases[aid] = NULL;
    }
}

static int fntPrepareGlyphCachePage(font_t *font, int pageid)
{
    if (pageid > font->cacheMaxPageID) {
        fnt_glyph_cache_entry_t **np = realloc(font->glyphCache, (pageid + 1) * sizeof(fnt_glyph_cache_entry_t *));

        if (!np)
            return 0;

        font->glyphCache = np;

        unsigned int page;
        for (page = font->cacheMaxPageID + 1; page <= pageid; ++page)
            font->glyphCache[page] = NULL;

        font->cacheMaxPageID = pageid;
    }

    // if it already was allocated, skip this
    if (font->glyphCache[pageid])
        return 1;

    // allocate the page
    font->glyphCache[pageid] = malloc(sizeof(fnt_glyph_cache_entry_t) * GLYPH_CACHE_PAGE_SIZE);

    int i;
    for (i = 0; i < GLYPH_CACHE_PAGE_SIZE; ++i) {
        font->glyphCache[pageid][i].isValid = 0;
        font->glyphCache[pageid][i].atlas = NULL;
        font->glyphCache[pageid][i].allocation = NULL;
    }

    return 1;
}

static void fntPrepareCLUT()
{
    fontClut.PSM = GS_PSM_T8;
    fontClut.ClutPSM = GS_PSM_CT32;
    fontClut.Clut = memalign(128, 256 * 4);
    fontClut.VramClut = 0;

    // generate the clut table
    size_t i;
    u32 *clut = fontClut.Clut;
    for (i = 0; i < 256; ++i) {
        u8 alpha = (i * 128) / 255;

        *clut = GS_SETREG_RGBA(0xFF, 0xFF, 0xFF, alpha);
        clut++;
    }
}

static void fntDestroyCLUT()
{
    free(fontClut.Clut);
    fontClut.Clut = NULL;
}

static void fntInitSlot(font_t *font)
{
    font->face = NULL;
    font->glyphCache = NULL;
    font->cacheMaxPageID = -1;
    font->dataPtr = NULL;
    font->isValid = 0;
    font->fontSize = 0;

    int aid = 0;
    for (; aid < ATLAS_MAX; ++aid)
        font->atlases[aid] = NULL;
}

static void fntDeleteSlot(font_t *font)
{
    // free the glyph cache, atlases, unload the font
    fntCacheFlush(font);

    FT_Done_Face(font->face);
    font->face = NULL;

    if (font->dataPtr) {
        free(font->dataPtr);
        font->dataPtr = NULL;
    }

    font->isValid = 0;
    font->fontSize = 0;
}

void fntRelease(int id)
{
    if (id > FNT_DEFAULT && id < FNT_MAX_COUNT)
        fntDeleteSlot(&fonts[id]);
}

static int fntLoadSlot(font_t *font, char *path, int fontSize)
{
    void *buffer = NULL;
    int bufferSize = -1;

    fntInitSlot(font);

    if (path) {
        buffer = readFile(path, -1, &bufferSize);
        if (!buffer) {
            LOG("FNTSYS Font file loading failed: %s\n", path);
            return FNT_ERROR;
        }
        font->dataPtr = buffer;
    } else {
        buffer = &poeveticanew_raw;
        bufferSize = size_poeveticanew_raw;
    }

    // load the font via memory handle
    int error = FT_New_Memory_Face(font_library, (FT_Byte *)buffer, bufferSize, 0, &font->face);
    if (error) {
        LOG("FNTSYS Freetype font loading failed with %x!\n", error);
        fntDeleteSlot(font);
        return FNT_ERROR;
    }

    font->isValid = 1;
    font->fontSize = fontSize;
    fntUpdateAspectRatio();

    return 0;
}

void fntInit()
{
    LOG("FNTSYS Init\n");
    int error = FT_Init_FreeType(&font_library);
    if (error) {
        // just report over the ps2link
        LOG("FNTSYS Freetype init failed with %x!\n", error);
        return;
    }

    fntPrepareCLUT();

    gFontSema.init_count = 1;
    gFontSema.max_count = 1;
    gFontSema.option = 0;
    gFontSemaId = CreateSema(&gFontSema);

    int i = 0;
    for (; i < FNT_MAX_COUNT; ++i)
        fntInitSlot(&fonts[i]);

    fntLoadDefault(NULL);
}

int fntLoadFile(char *path, int fontSize)
{
    font_t *font;
    int i = 1;
    for (; i < FNT_MAX_COUNT; i++) {
        font = &fonts[i];
        if (!font->isValid) {
            if (fntLoadSlot(font, path, fontSize) != FNT_ERROR)
                return i;
            break;
        }
    }

    return FNT_ERROR;
}

int fntLoadDefault(char *path)
{
    font_t newFont, oldFont;

    if (fntLoadSlot(&newFont, path, FNTSYS_DEFAULT_SIZE) != FNT_ERROR) {
        // copy over the new font definition
        // we have to lock this phase, as the old font may still be used
        // Note: No check for concurrency is done here, which is kinda funky!
        WaitSema(gFontSemaId);
        memcpy(&oldFont, &fonts[FNT_DEFAULT], sizeof(font_t));
        memcpy(&fonts[FNT_DEFAULT], &newFont, sizeof(font_t));
        SignalSema(gFontSemaId);

        // delete the old font
        fntDeleteSlot(&oldFont);

        return 0;
    }

    return -1;
}

void fntEnd()
{
    LOG("FNTSYS End\n");
    // release all the fonts
    int id;
    for (id = 0; id < FNT_MAX_COUNT; ++id)
        fntDeleteSlot(&fonts[id]);

    // deinit freetype system
    FT_Done_FreeType(font_library);

    DeleteSema(gFontSemaId);

    fntDestroyCLUT();
}

static atlas_t *fntNewAtlas()
{
    atlas_t *atl = atlasNew(ATLAS_WIDTH, ATLAS_HEIGHT, GS_PSM_T8);

    atl->surface.ClutPSM = GS_PSM_CT32;
    atl->surface.Clut = fontClut.Clut;

    return atl;
}

static int fntGlyphAtlasPlace(font_t *font, fnt_glyph_cache_entry_t *glyph)
{
    FT_GlyphSlot slot = font->face->glyph;

    // LOG("FNTSYS GlyphAtlasPlace: Placing the glyph... %d x %d\n", slot->bitmap.width, slot->bitmap.rows);

    if (slot->bitmap.width == 0 || slot->bitmap.rows == 0) {
        // no bitmap glyph, just skip
        return 1;
    }

    int aid = 0;
    for (; aid < ATLAS_MAX; aid++) {
        // LOG("FNTSYS Placing aid %d...\n", aid);
        atlas_t **atl = &font->atlases[aid];
        if (!*atl) { // atlas slot not yet used
            // LOG("FNTSYS aid %d is new...\n", aid);
            *atl = fntNewAtlas();
        }

        glyph->allocation = atlasPlace(*atl, slot->bitmap.width, slot->bitmap.rows, slot->bitmap.buffer);
        if (glyph->allocation) {
            // LOG("FNTSYS Found placement\n", aid);
            glyph->atlas = *atl;

            return 1;
        }
    }

    LOG("FNTSYS No atlas free\n", aid);
    return 0;
}

/** Internal method. Makes sure the bitmap data for particular character are pre-rendered to the glyph cache */
static fnt_glyph_cache_entry_t *fntCacheGlyph(font_t *font, uint32_t gid)
{
    // calc page id and in-page index from glyph id
    int pageid = gid / GLYPH_CACHE_PAGE_SIZE;
    int idx = gid % GLYPH_CACHE_PAGE_SIZE;

    // do not call on every char of every font rendering call
    if (!GLYPH_PAGE_OK(font, pageid))
        if (!fntPrepareGlyphCachePage(font, pageid)) // failed to prepare the page...
            return NULL;

    fnt_glyph_cache_entry_t *page = font->glyphCache[pageid];
    /* Should never happen.
    if (!page) // safeguard
        return NULL;
    */

    fnt_glyph_cache_entry_t *glyph = &page[idx];
    if (glyph->isValid)
        return glyph;

    // not cached but valid. Cache
    if (!font->face) {
        LOG("FNTSYS Face is NULL!\n");
    }

    int error = FT_Load_Char(font->face, gid, FT_LOAD_RENDER);
    if (error) {
        LOG("FNTSYS Error loading glyph - %d\n", error);
        return NULL;
    }

    // find atlas placement for the glyph
    if (!fntGlyphAtlasPlace(font, glyph))
        return NULL;

    FT_GlyphSlot slot = font->face->glyph;
    glyph->width = slot->bitmap.width;
    glyph->height = slot->bitmap.rows;
    glyph->shx = slot->advance.x;
    glyph->shy = slot->advance.y;
    glyph->ox = slot->bitmap_left;
    glyph->oy = -slot->bitmap_top;

    glyph->isValid = 1;

    return glyph;
}

void fntUpdateAspectRatio()
{
    int i;
    int w, h, wn, hn;
    float ws, hs;

    rmGetScreenExtents(&w, &h);
    rmGetScreenExtentsNative(&wn, &hn);
    // Scale height from virtual resolution (640x480) to the native display resolution
    hs = (float)hn / (float)h;
    // Scale width according to the PAR (Pixel Aspect Ratio)
    ws = hs * rmGetPAR();

    // Supersample height*2 when using interlaced frame mode
    if (rmGetInterlacedFrameMode() == 1)
        hs *= 2;

    // flush cache - it will be invalid after the setting
    for (i = 0; i < FNT_MAX_COUNT; i++) {
        if (fonts[i].isValid) {
            fntCacheFlush(&fonts[i]);
            // TODO: this seems correct, but the rest of the OPL UI (i.e. spacers) doesn't seem to be correctly scaled.
            FT_Set_Char_Size(fonts[i].face, fonts[i].fontSize * 64, fonts[i].fontSize * 64, fDPI * ws, fDPI * hs);
        }
    }
}

static void fntRenderGlyph(fnt_glyph_cache_entry_t *glyph, int pen_x, int pen_y)
{
    // only if glyph has atlas placement
    if (glyph->allocation) {
        /* TODO: Ineffective on many parts:
         * 1. Usage of floats for UV - fixed point should suffice (and is used internally by GS for UV)
         *
         * 2. GS_SETREG_TEX0 for every quad - why? gsKit should only set texture if demanded
         *    We should prepare a special fnt render method that would step over most of the
         *    performance problems under - beginning with rmSetupQuad and continuing into gsKit
         *    - this method would handle the preparation of the quads and GS upload itself,
         *    without the use of prim_quad_texture and rmSetupQuad...
         */
        quad.ul.x = pen_x + glyph->ox;
        if (rmGetInterlacedFrameMode() == 0)
            quad.ul.y = pen_y + glyph->oy;
        else
            quad.ul.y = (float)pen_y + ((float)glyph->oy / 2.0f);
        quad.ul.u = glyph->allocation->x;
        quad.ul.v = glyph->allocation->y;

        quad.br.x = quad.ul.x + glyph->width;
        if (rmGetInterlacedFrameMode() == 0)
            quad.br.y = quad.ul.y + glyph->height;
        else
            quad.br.y = quad.ul.y + ((float)glyph->height / 2.0f);
        quad.br.u = quad.ul.u + glyph->width;
        quad.br.v = quad.ul.v + glyph->height;

        quad.txt = &glyph->atlas->surface;

        rmDrawQuad(&quad);
    }
}


#ifndef __RTL
int fntRenderString(int id, int x, int y, short aligned, size_t width, size_t height, const char *string, u64 colour)
{
    // wait for font lock to unlock
    WaitSema(gFontSemaId);
    font_t *font = &fonts[id];
    SignalSema(gFontSemaId);

    // Convert to native display resolution
    x = rmScaleX(x);
    y = rmScaleY(y);
    width = rmScaleX(width);
    height = rmScaleY(height);

    if (aligned & ALIGN_HCENTER) {
        if (width) {
            x -= min(fntCalcDimensions(id, string), width) >> 1;
        } else {
            x -= fntCalcDimensions(id, string) >> 1;
        }
    }

    if (aligned & ALIGN_VCENTER) {
        y += rmScaleY(fonts[id].fontSize - 4) >> 1;
    } else {
        y += rmScaleY(fonts[id].fontSize - 2);
    }

    quad.color = colour;

    int pen_x = x;
    int xmax = x + width;
    int ymax = y + height;

    use_kerning = FT_HAS_KERNING(font->face);
    state = UTF8_ACCEPT;
    previous = 0;

    // Note: We need to change this so that we'll accumulate whole word before doing a layout with it
    // for now this method breaks on any character - which is a bit ugly

    // I don't want to do anything complicated though so I'd say
    // we should instead have a dynamic layout routine that'll replace spaces with newlines as appropriate
    // because that'll make the code run only once per N frames, not every frame

    // cache glyphs and render as we go
    for (; *string; ++string) {
        if (utf8Decode(&state, &codepoint, *string)) // accumulate the codepoint value
            continue;

        glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        // kerning
        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                pen_x += delta.x >> 6;
            }
            previous = glyph_index;
        }

        if (width) {
            if (codepoint == '\n') {
                pen_x = x;
                y += rmScaleY(MENU_ITEM_HEIGHT); // hmax is too tight and unordered, generally
                continue;
            }

            if (y > ymax) // stepped over the max
                break;

            if (pen_x + glyph->width > xmax) {
                pen_x = xmax + 1; // to be sure no other cahr will be written (even not a smaller one just following)
                continue;
            }
        }

        fntRenderGlyph(glyph, pen_x, y);
        pen_x += glyph->shx >> 6;
    }

    return rmUnScaleX(pen_x);
}

#else
static int isRTL(u32 character)
{
    return (((character >= 0x00000590 && character <= 0x000008FF) || (character >= 0x0000FB50 && character <= 0x0000FDFF) || (character >= 0x0000FE70 && character <= 0x0000FEFF) || (character >= 0x00010800 && character <= 0x00010FFF) || (character >= 0x0001E800 && character <= 0x0001EFFF)) ? 1 : 0);
}

static int isWeak(u32 character)
{
    return (((character >= 0x0000 && character <= 0x0040) || (character >= 0x005B && character <= 0x0060) || (character >= 0x007B && character <= 0x00BF) || (character >= 0x00D7 && character <= 0x00F7) || (character >= 0x02B9 && character <= 0x02FF) || (character >= 0x2000 && character <= 0x2BFF)) ? 1 : 0);
}

static void fntRenderSubRTL(font_t *font, const char *startRTL, const char *string, fnt_glyph_cache_entry_t *glyph, int x, int y)
{
    if (glyph) {
        x -= glyph->shx >> 6;
        fntRenderGlyph(glyph, x, y);
    }

    for (; startRTL != string; ++startRTL) {
        if (utf8Decode(&state, &codepoint, *startRTL))
            continue;

        glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                x -= delta.x >> 6;
            }
            previous = glyph_index;
        }

        x -= glyph->shx >> 6;
        fntRenderGlyph(glyph, x, y);
    }
}

int fntRenderString(int id, int x, int y, short aligned, size_t width, size_t height, const char *string, u64 colour)
{
    // wait for font lock to unlock
    WaitSema(gFontSemaId);
    font_t *font = &fonts[id];
    SignalSema(gFontSemaId);

    // Convert to native display resolution
    x = rmScaleX(x);
    y = rmScaleY(y);
    width = rmScaleX(width);
    height = rmScaleY(height);

    if (aligned & ALIGN_HCENTER) {
        if (width) {
            x -= min(fntCalcDimensions(id, string), width) >> 1;
        } else {
            x -= fntCalcDimensions(id, string) >> 1;
        }
    }

    if (aligned & ALIGN_VCENTER) {
        y += rmScaleY(fonts[id].fontSize - 4) >> 1;
    } else {
        y += rmScaleY(fonts[id].fontSize - 2);
    }

    quad.color = colour;

    int pen_x = x;
    int xmax = x + width;
    int ymax = y + height;

    use_kerning = FT_HAS_KERNING(font->face);
    state = UTF8_ACCEPT;
    previous = 0;

    short inRTL = 0;
    int delta_x, pen_xRTL = 0;
    fnt_glyph_cache_entry_t *glyphRTL = NULL;
    const char *startRTL = NULL;

    // cache glyphs and render as we go
    for (; *string; ++string) {
        if (utf8Decode(&state, &codepoint, *string)) // accumulate the codepoint value
            continue;

        glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        // kerning
        delta_x = 0;
        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                delta_x = delta.x >> 6;
            }
            previous = glyph_index;
        }


        if (width) {
            if (codepoint == '\n') {
                pen_x = x;
                y += rmScaleY(MENU_ITEM_HEIGHT); // hmax is too tight and unordered, generally
                continue;
            }

            if ((pen_x + glyph->width > xmax) || (y > ymax)) // stepped over the max
                break;
        }

        if (isRTL(codepoint)) {
            if (!inRTL && !isWeak(codepoint)) {
                inRTL = 1;
                pen_xRTL = pen_x;
                glyphRTL = glyph;
                startRTL = string + 1;
            }
        } else {
            if (inRTL && !isWeak(codepoint)) { // A LTR character is encountered. Render RTL characters before continuing.
                inRTL = 0;
                pen_x = pen_xRTL;
                fntRenderSubRTL(font, startRTL, string, glyphRTL, pen_xRTL, y);
            }
        }

        if (inRTL) {
            pen_xRTL += delta_x + (glyph->shx >> 6);
        } else {
            pen_x += delta_x;
            fntRenderGlyph(glyph, pen_x, y);
            pen_x += glyph->shx >> 6;
        }
    }

    if (inRTL) {
        pen_x = pen_xRTL;
        fntRenderSubRTL(font, startRTL, string, glyphRTL, pen_xRTL, y);
    }

    return rmUnScaleX(pen_x);
}
#endif

void fntFitString(int id, char *string, size_t width)
{
    size_t cw = 0;
    char *str = string;
    size_t spacewidth = fntCalcDimensions(id, " ");
    char *psp = NULL;
    width = rmScaleX(width);

    while (*str) {
        // scan forward to the next whitespace
        char *sp = str;
        for (; *sp && *sp != ' ' && *sp != '\n'; ++sp)
            ;

        // store what was there before
        char osp = *sp;

        // newline resets the situation
        if (osp == '\n') {
            cw = 0;
            str = ++sp;
            psp = NULL;
            continue;
        }

        // terminate after the word
        *sp = '\0';

        // Calc the font's width...
        // NOTE: The word was terminated, so we're seeing a single word
        // on that position
        size_t ww = fntCalcDimensions(id, str);

        if (cw + ww > width) {
            if (psp) {
                // we have a prev space to utilise (wrap on it)
                *psp = '\n';
                *sp = osp;
                cw = ww;
                psp = sp;
            } else {
                // no prev. space to hijack, must break after the word
                // this will mean overflowed text...
                *sp = '\n';
                cw = 0;
            }
        } else {
            cw += ww;
            *sp = osp;
            psp = sp;
        }

        cw += spacewidth;
        str = ++sp;
    }
}

int fntCalcDimensions(int id, const char *str)
{
    int w = 0;

    WaitSema(gFontSemaId);
    font_t *font = &fonts[id];
    SignalSema(gFontSemaId);

    uint32_t codepoint;
    uint32_t state = UTF8_ACCEPT;
    FT_Bool use_kerning = FT_HAS_KERNING(font->face);
    FT_UInt glyph_index, previous = 0;
    FT_Vector delta;

    // cache glyphs and render as we go
    for (; *str; ++str) {
        if (utf8Decode(&state, &codepoint, *str)) // accumulate the codepoint value
            continue;

        // Could just as well only get the glyph dimensions
        // but it is probable the glyphs will be needed anyway
        fnt_glyph_cache_entry_t *glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        // kerning
        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                w += delta.x >> 6;
            }
            previous = glyph_index;
        }

        w += glyph->shx >> 6;
    }

    return w;
}
