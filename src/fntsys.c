/*
  Copyright 2010, Volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/integers.h"
#include "include/usbld.h"
#include "include/fntsys.h"
#include "include/renderman.h"
#include "include/ioman.h"
#include "include/utf8.h"
#include "include/util.h"

#include <ft2build.h>

#include FT_FREETYPE_H

extern void* freesansfont_raw;
extern int size_freesansfont_raw;

// freetype vars
static FT_Library font_library;

static rm_quad_t quad;

static int gCharHeight;

static s32 gFontSemaId;
static ee_sema_t gFontSema;

/** Single entry in the glyph cache */
typedef struct {
	int isValid;
	// RGBA bitmap ready to be rendered
	char *glyphData;
	// size in pixels of the glyph
	int width, height;
	// offsetting of the glyph
	int ox, oy;
	// advancements in pixels after rendering this glyph
	int shx, shy;
	// Texture version of the same
	GSTEXTURE texture;
}  fnt_glyph_cache_entry_t;

/** A whole font definition */
typedef struct {
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

    /// Nonzero for custom fonts
    int isDefault;

    /// Pointer to data, if allocation takeover was selected (will be freed)
    void *dataPtr;
} font_t;

#define FNT_MAX_COUNT (16)

/// Array of font definitions
static font_t fonts[FNT_MAX_COUNT];

#define GLYPH_CACHE_PAGE_SIZE 256

static int fntPrepareGlyphCachePage(font_t *font, int pageid) {
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
                font->glyphCache[pageid][i].glyphData = NULL;
	}
	
	return 1;
}

static void fntResetFontDef(font_t *fnt) {
    fnt->glyphCache = NULL;
    fnt->cacheMaxPageID = -1;
    fnt->isValid = 0;
    fnt->isDefault = 0;
    fnt->dataPtr = NULL;
}

void fntInit(void) {
	int error = FT_Init_FreeType( &font_library );

	if ( error ) {
		// just report over the ps2link
		LOG("Freetype init failed with %x!\n", error);
		// SleepThread();
	}

        gFontSema.init_count = 1;
        gFontSema.max_count = 1;
        gFontSema.option = 0;
        gFontSemaId = CreateSema(&gFontSema);

        int i;
        for (i = 0; i < FNT_MAX_COUNT; ++i)
            fntResetFontDef(&fonts[i]);

        // load the default font (will be id=0)
        fntLoad(NULL, -1, 0);
}

static void fntCacheFlushPage(fnt_glyph_cache_entry_t *page) {
	int i;
	
	for (i = 0; i < GLYPH_CACHE_PAGE_SIZE; ++i, ++page) {
		if (page->isValid) {
			free(page->glyphData);
			page->glyphData = NULL;
			page->isValid = 0;
		}
	}
}

static void fntCacheFlush(font_t *fnt) {
	int i;

	// Release all the glyphs from the cache
        for (i = 0; i <= fnt->cacheMaxPageID; ++i) {
                if (fnt->glyphCache[i]) {
                        fntCacheFlushPage(fnt->glyphCache[i]);
                        free(fnt->glyphCache[i]);
                        fnt->glyphCache[i] = NULL;
		}
	}
	
        free(fnt->glyphCache);
        fnt->glyphCache = NULL;
        fnt->cacheMaxPageID = -1;
}

static int fntNewFont() {
    int i;
    for (i = 0; i < FNT_MAX_COUNT; ++i) {
        if (fonts[i].isValid == 0) {
            fntResetFontDef(&fonts[i]);
            return i;
        }
    }

    return FNT_ERROR;
}

void fntDeleteFont(font_t *font) {
    // skip already deleted fonts
    if (!font->isValid)
        return;

    // free the glyph cache, unload the font
    fntCacheFlush(font);

    FT_Done_Face(font->face);

    if (font->dataPtr) {
        free(font->dataPtr);
        font->dataPtr = NULL;
    }

    font->isValid = 0;
}

int fntLoadSlot(font_t *fnt, void* buffer, int bufferSize) {
	if (!buffer) {
		buffer = &freesansfont_raw;
		bufferSize = size_freesansfont_raw;
                fnt->isDefault = 1;
	}

	// load the font via memory handle
        int error = FT_New_Memory_Face( font_library, (FT_Byte*) buffer,  bufferSize,  0,  &fnt->face );
	
	if ( error ) { 
		// just report over the ps2link
		LOG("Freetype: Font loading failed with %x!\n", error);
		// SleepThread();
                return -1;
	} 
	
	gCharHeight = 16;
	
        error = FT_Set_Char_Size(fnt->face, 0, gCharHeight * 16, 300, 300);
	/*error = FT_Set_Pixel_Sizes( face,
		0, // pixel_width
		gCharHeight ); // pixel_height*/

	if ( error ) { 
		// just report over the ps2link
		LOG("Freetype: Error setting font pixel size with %x!\n", error);
		// SleepThread();
                return -1;
	}


        fnt->isValid = 1;
        return 0;
}


int fntLoad(void* buffer, int bufferSize, int takeover) {
        // we need a new slot in the font array
        int fontID = fntNewFont();

        if (fontID == FNT_ERROR)
            return FNT_ERROR;

        font_t *fnt = &fonts[fontID];

        if (fntLoadSlot(fnt, buffer, bufferSize) == FNT_ERROR)
            return FNT_ERROR;

        if (takeover)
            fnt->dataPtr = buffer;

        return fontID;
}

int fntLoadFile(char* path) {
    // load the buffer with font
    int size = -1;
    void* customFont = readFile(path, -1, &size);

    if (!customFont)
        return FNT_ERROR;

    int fontID = fntLoad(customFont, size, 1);

    return fontID;
}

void fntRelease(int id) {
    if (id < FNT_MAX_COUNT)
        fntDeleteFont(&fonts[id]);
}

/** Terminates the font rendering system */
void fntEnd(void) {
        // release all the fonts
        int id;
        for (id = 0 ; id < FNT_MAX_COUNT; ++id)
            fntDeleteFont(&fonts[id]);

	// deinit freetype system
	FT_Done_FreeType( font_library );

        DeleteSema(gFontSemaId);
}

/** Internal method. Updates texture part of glyph cache entry */
static void fntUpdateTexture(fnt_glyph_cache_entry_t* glyph) {
	glyph->texture.Width = glyph->width;
	glyph->texture.Height = glyph->height;
	glyph->texture.PSM = GS_PSM_CT32;
        glyph->texture.Filter = GS_FILTER_LINEAR;
	glyph->texture.Mem = (u32*)glyph->glyphData;
	glyph->texture.Vram = 0;
}

/** Internal method. Makes sure the bitmap data for particular character are pre-rendered to the glyph cache */
static fnt_glyph_cache_entry_t* fntCacheGlyph(font_t *fnt, uint32_t gid) {
	// calc page id and in-page index from glyph id
	int pageid = gid / GLYPH_CACHE_PAGE_SIZE;
	int idx = gid % GLYPH_CACHE_PAGE_SIZE;
	
        if (!fntPrepareGlyphCachePage(fnt, pageid)) // failed to prepare the page...
		return NULL;
	
        fnt_glyph_cache_entry_t *page = fnt->glyphCache[pageid];
	
	if (!page) // safeguard
		return NULL;
	
	fnt_glyph_cache_entry_t* glyph = &page[idx];
	
	if (glyph->isValid)
		return glyph;

        FT_GlyphSlot slot = fnt->face->glyph;
	
	// not cached but valid. Cache
	glyph->isValid = 1;
	
        if (!fnt->face) {
		LOG("FNT: Face is NULL!\n");
	}
	
        int error = FT_Load_Char( fnt->face, gid, FT_LOAD_RENDER );
	
	if (error) {
		LOG("FNT: Error loading glyph - %d\n", error);
		return glyph;
	}
	
	// copy the data
        int i = 0;
        int pixelcount = slot->bitmap.width * slot->bitmap.rows;
	
	// If we would know how to render grayscale (single channel) it could save memory
        glyph->glyphData = memalign(128, pixelcount * 4);

        // incrementing pointers should be cheaper than array indexing...
        char *src = slot->bitmap.buffer;
        char *data = glyph->glyphData;

	for (i = 0; i < pixelcount; ++i) {
                char c = *src++;
		
                *(data++) = c;
                *(data++) = c;
                *(data++) = c;
                *(data++) = c;
	}

        glyph->width = slot->bitmap.width;
        glyph->height = slot->bitmap.rows;
        glyph->shx = slot->advance.x;
        glyph->shy = slot->advance.y;
        glyph->ox  = slot->bitmap_left;
        glyph->oy  = gCharHeight - slot->bitmap_top;
	
	// update the texture too...
        fntUpdateTexture(glyph);
	
	return glyph;
}

void fntSetAspectRatio(float aw, float ah) {
	// flush cache - it will be invalid after the setting
        int i;

        for (i = 0; i < FNT_MAX_COUNT; ++i) {
            if (fonts[i].isValid)
                fntCacheFlush(&fonts[i]);
        }
	
	// set new aspect ratio (Is this correct, I wonder?)
	// TODO: error = FT_Set_Char_Size(face, 0, gCharHeight*64, ah*300, aw*300);
}

int fntRenderString(int font, int x, int y, short aligned, const unsigned char* string, u64 colour) {
        // wait for font lock to unlock
        WaitSema(gFontSemaId);
        font_t *fnt = &fonts[font];
        SignalSema(gFontSemaId);

	if (aligned) {
		int w = 0, h = 0;
                fntCalcDimensions(font, string, &w, &h);
		x -= w >> 1;
		y -= MENU_ITEM_HEIGHT >> 1;
	}
	
	// backup the aspect ratio, restore 1:1 to have the font rendering clean
	float aw, ah;
	rmGetAspectRatio(&aw, &ah);
	rmResetAspectRatio();
	
	uint32_t codepoint;
	uint32_t state = 0;
	int w = 0;
	
	// cache glyphs and render as we go
	for (;*string; ++string) {
		unsigned char c = *string;
		
		if (utf8Decode(&state, &codepoint, c)) // accumulate the codepoint value
			continue;
		
                fnt_glyph_cache_entry_t* glyph = fntCacheGlyph(fnt, codepoint);
		
		if (!glyph) {
			//LOG("FNT: Glyph error, %x\n", c);
			continue;
		}

		rmSetupQuad(&glyph->texture, x, y, ALIGN_NONE, DIM_UNDEF, DIM_UNDEF, colour, &quad);
		quad.ul.x += glyph->ox; quad.br.x += glyph->ox;
		quad.ul.y += glyph->oy;	quad.br.y += glyph->oy;
		rmDrawQuad(&quad);

		int ofs = glyph->shx / 64;
		w += ofs;
		x += ofs;
		y += glyph->shy / 64;
	}

	// return to the prev. aspect ratio
	rmSetAspectRatio(aw, ah);
	return w;
}

void fntCalcDimensions(int font, const unsigned char* str, int *w, int *h) {
	*w = 0;
	*h = 0;

        font_t *fnt = &fonts[font];
	
	// backup the aspect ratio, restore 1:1 to have the font rendering clean
	uint32_t codepoint;
	uint32_t state = 0;
	
	// cache glyphs and render as we go
	for (;*str; ++str) {
		unsigned char c = *str;
		
		if (utf8Decode(&state, &codepoint, c)) // accumulate the codepoint value
			continue;
		
		// Could just as well only get the glyph dimensions
		// but it is probable the glyphs will be needed anyway
                fnt_glyph_cache_entry_t* glyph = fntCacheGlyph(fnt, codepoint);
		
		*w += glyph->shx / 64;
		*h = glyph->height;
	}
}

void fntReplace(int id, void* buffer, int bufferSize, int takeover) {
    font_t *fnt = &fonts[id];

    font_t ndefault, old;
    fntResetFontDef(&ndefault);
    fntLoadSlot(&ndefault, buffer, bufferSize);

    // copy over the new font definition
    // we have to lock this phase, as the old font may still be used
    WaitSema(gFontSemaId);
    memcpy(&old, fnt, sizeof(font_t));
    memcpy(fnt, &ndefault, sizeof(font_t));

    if (takeover)
        fnt->dataPtr = buffer;

    SignalSema(gFontSemaId);

    // delete the old font
    fntDeleteFont(&old);
}

void fntSetDefault(int id) {
    font_t *fnt = &fonts[id];

    // already default
    if (fnt->isDefault)
        return;

    font_t ndefault, old;
    fntResetFontDef(&ndefault);
    fntLoadSlot(&ndefault, NULL, -1);
    ndefault.isDefault = 1;

    // copy over the new font definition
    // we have to lock this phase, as the old font may still be used
    // Note: No check for concurrency is done here, which is kinda funky!
    WaitSema(gFontSemaId);
    memcpy(&old, fnt, sizeof(font_t));
    memcpy(fnt, &ndefault, sizeof(font_t));
    SignalSema(gFontSemaId);

    // delete the old font
    fntDeleteFont(&old);
}
