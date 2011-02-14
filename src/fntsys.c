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
#include "include/atlas.h"

#include <ft2build.h>

#include FT_FREETYPE_H

extern void* freesansfont_raw;
extern int size_freesansfont_raw;

/// Maximal count of atlases per font
#define ATLAS_MAX 4
/// Atlas width in pixels
#define ATLAS_WIDTH 128
/// Atlas height in pixels
#define ATLAS_HEIGHT 128

// freetype vars
static FT_Library font_library;

static rm_quad_t quad;

static int gCharHeight;

static s32 gFontSemaId;
static ee_sema_t gFontSema;

/** Single entry in the glyph cache */
typedef struct {
	int isValid;
	// size in pixels of the glyph
	int width, height;
	// offsetting of the glyph
	int ox, oy;
	// advancements in pixels after rendering this glyph
	int shx, shy;
	
	// atlas for which the allocation was done
    atlas_t* atlas;

	// atlas allocation position
	struct atlas_allocation_t *allocation;

} fnt_glyph_cache_entry_t;

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

	/// Texture atlases (default to NULL)
	atlas_t *atlases[ATLAS_MAX];
	
	/// Pointer to data, if allocation takeover was selected (will be freed)
	void *dataPtr;
} font_t;

#define FNT_MAX_COUNT (16)

/// Array of font definitions
static font_t fonts[FNT_MAX_COUNT];

#define GLYPH_CACHE_PAGE_SIZE 256

#define GLYPH_PAGE_OK(font,page) ((pageid <= font->cacheMaxPageID) && (font->glyphCache[page]))

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
		font->glyphCache[pageid][i].atlas = NULL;
		font->glyphCache[pageid][i].allocation = NULL;
	}

	return 1;
}

static void fntResetFontDef(font_t *fnt) {
	LOG("fntResetFontDef\n");
	fnt->glyphCache = NULL;
	fnt->cacheMaxPageID = -1;
	fnt->isValid = 0;
	fnt->isDefault = 0;
	
	int aid;
	for(aid = 0; aid < ATLAS_MAX; ++aid)
		fnt->atlases[aid] = NULL;
		
	fnt->dataPtr = NULL;
}

void fntInit(void) {
	LOG("fntInit\n");
	int error = FT_Init_FreeType(&font_library);

	if (error) {
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
	LOG("fntCacheFlushPage\n");
	int i;

	for (i = 0; i < GLYPH_CACHE_PAGE_SIZE; ++i, ++page) {
		page->isValid = 0;
		// we're not doing any atlasFree or such - atlas has to be rebuild
		page->allocation = NULL;
		page->atlas = NULL;
	}
}

static void fntCacheFlush(font_t *fnt) {
	LOG("fntCacheFlush\n");
	
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
	
	// free all atlasses too, they're invalid now anyway
	int aid;
	for(aid = 0; aid < ATLAS_MAX; ++aid) {
		atlasFree(fnt->atlases[aid]);
		fnt->atlases[aid] = NULL;
	}
}

static int fntNewFont() {
	LOG("fntNewFont\n");
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
	LOG("fntDeleteFont\n");
	
	// skip already deleted fonts
	if (!font->isValid)
		return;

	// free the glyph cache, atlases, unload the font
	fntCacheFlush(font);

	FT_Done_Face(font->face);

	if (font->dataPtr) {
		free(font->dataPtr);
		font->dataPtr = NULL;
	}

	font->isValid = 0;
}

int fntLoadSlot(font_t *fnt, void* buffer, int bufferSize) {
	LOG("fntLoadSlot\n");
	
	if (!buffer) {
		buffer = &freesansfont_raw;
		bufferSize = size_freesansfont_raw;
		fnt->isDefault = 1;
	}

	// load the font via memory handle
	int error = FT_New_Memory_Face(font_library, (FT_Byte*) buffer, bufferSize, 0, &fnt->face);

	if (error) {
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

	if (error) {
		// just report over the ps2link
		LOG("Freetype: Error setting font pixel size with %x!\n", error);
		// SleepThread();
		return -1;
	}

	fnt->isValid = 1;
	return 0;
}

int fntLoad(void* buffer, int bufferSize, int takeover) {
	LOG("fntLoad\n");
	
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
	LOG("fntLoadFile\n");
	// load the buffer with font
	int size = -1;
	void* customFont = readFile(path, -1, &size);

	if (!customFont)
		return FNT_ERROR;

	int fontID = fntLoad(customFont, size, 1);

	return fontID;
}

void fntRelease(int id) {
	LOG("fntRelease\n");
	if (id < FNT_MAX_COUNT)
		fntDeleteFont(&fonts[id]);
}

/** Terminates the font rendering system */
void fntEnd(void) {
	LOG("fntEnd\n");
	// release all the fonts
	int id;
	for (id = 0; id < FNT_MAX_COUNT; ++id)
		fntDeleteFont(&fonts[id]);

	// deinit freetype system
	FT_Done_FreeType(font_library);

	DeleteSema(gFontSemaId);
}

static int fntGlyphAtlasPlace(font_t *fnt, fnt_glyph_cache_entry_t* glyph) {
	FT_GlyphSlot slot = fnt->face->glyph;
	
 	LOG("fntGlyphAtlasPlace: Placing the glyph... %d x %d\n", slot->bitmap.width, slot->bitmap.rows);
	
	if (slot->bitmap.width == 0 || slot->bitmap.rows == 0) {
		// no bitmap glyph, just skip
		return 1;
	}
	
	int aid;
	
	for (aid = 0; aid < ATLAS_MAX; ++aid) {
		LOG("  * Placing aid %d...\n", aid);
		atlas_t **atl = &fnt->atlases[aid];
		if (!*atl) { // atlas slot not yet used
			LOG("  * aid %d is new...\n", aid);
			*atl = atlasNew(ATLAS_WIDTH, ATLAS_HEIGHT);
		}
		
		glyph->allocation = 
			atlasPlace(*atl, slot->bitmap.width, slot->bitmap.rows, GS_PSM_T8, slot->bitmap.buffer);
			
		if (glyph->allocation) {
			LOG("  * found placement\n", aid);
			glyph->atlas = *atl;
			
			return 1;
		}
	}
	
	LOG("  * ! no atlas free\n", aid);
	return 0;
}

/** Internal method. Makes sure the bitmap data for particular character are pre-rendered to the glyph cache */
static fnt_glyph_cache_entry_t* fntCacheGlyph(font_t *fnt, uint32_t gid) {
	// calc page id and in-page index from glyph id
	int pageid = gid / GLYPH_CACHE_PAGE_SIZE;
	int idx = gid % GLYPH_CACHE_PAGE_SIZE;

	// do not call on every char of every font rendering call
	if (!GLYPH_PAGE_OK(fnt,pageid))
		if (!fntPrepareGlyphCachePage(fnt, pageid)) // failed to prepare the page...
			return NULL;

	fnt_glyph_cache_entry_t *page = fnt->glyphCache[pageid];

	/* Should never happen. 
	if (!page) // safeguard
		return NULL;
	*/

	fnt_glyph_cache_entry_t* glyph = &page[idx];

	if (glyph->isValid)
		return glyph;

	// not cached but valid. Cache
	if (!fnt->face) {
		LOG("FNT: Face is NULL!\n");
	}

	int error = FT_Load_Char(fnt->face, gid, FT_LOAD_RENDER);

	if (error) {
		LOG("FNT: Error loading glyph - %d\n", error);
		return NULL;
	}

	// find atlas placement for the glyph
	if  (!fntGlyphAtlasPlace(fnt, glyph))
		return NULL;

	FT_GlyphSlot slot = fnt->face->glyph;
	glyph->width = slot->bitmap.width;
	glyph->height = slot->bitmap.rows;
	glyph->shx = slot->advance.x;
	glyph->shy = slot->advance.y;
	glyph->ox = slot->bitmap_left;
	glyph->oy = gCharHeight - slot->bitmap_top;

	glyph->isValid = 1;

	return glyph;
}

void fntSetAspectRatio(float aw, float ah) {
	LOG("fntSetAspectRatio\n");
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
	int w = 0, d;
	FT_Bool use_kerning = FT_HAS_KERNING(fnt->face);
	FT_UInt glyph_index, previous = 0;
	FT_Vector delta;

	// cache glyphs and render as we go
	for (; *string; ++string) {
		unsigned char c = *string;

		if (utf8Decode(&state, &codepoint, c)) // accumulate the codepoint value
			continue;

		fnt_glyph_cache_entry_t* glyph = fntCacheGlyph(fnt, codepoint);
		if (!glyph) {
			continue;
		}

		// kerning
		if (use_kerning && previous) {
			glyph_index = FT_Get_Char_Index(fnt->face, codepoint);
			if (glyph_index) {
				FT_Get_Kerning(fnt->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
				d = delta.x >> 6;
				x += d;
				w += d;
			}
			previous = glyph_index;
		}

		
		// only if glyph has atlas placement
		if (glyph->allocation) {
			/* TODO: Ineffective on many parts:
			 * 1. Usage of floats for UV - fixed point should suffice (and is used internally by GS for UV)
			 *
			 * 2. GS_SETREG_TEX0 for every quad - why? gsKit should only set texture if demanded
			 *    We should prepare a special fnt render method that would step over most of the
			 *    performance problems under - begining with rmSetupQuad and continuing into gsKit
			 *    - this method would handle the preparetion of the quads and GS upload itself,
			 *    without the use of prim_quad_texture and rmSetupQuad...
			 *    
			 * 3. rmSetupQuad is cool for a few quads a frame, but glyphs are too many in numbers
			 *    - seriously, all that branching for every letter is a bit much
			 *
			 * 4. We should use clut to keep the memory allocations sane - we're linear in the 32bit buffers
			 *    anyway, no reason to waste like we do!
			 */
			rmSetupQuad(NULL, x, y, ALIGN_NONE, glyph->width, glyph->height, colour, &quad);
			quad.ul.x += glyph->ox;
			quad.br.x += glyph->ox;
			quad.ul.y += glyph->oy;
			quad.br.y += glyph->oy;
			
			// UV is our own, custom thing here
			quad.txt = &glyph->atlas->surface;
			quad.ul.u = glyph->allocation->x;
			quad.ul.v = glyph->allocation->y;
			
			quad.br.u = glyph->allocation->x + glyph->width;
			quad.br.v = glyph->allocation->y + glyph->height;
			
			rmDrawQuad(&quad);
		}
		
		int ofs = glyph->shx >> 6;
		w += ofs;
		x += ofs;
		y += glyph->shy >> 6;
	}

	// return to the prev. aspect ratio
	rmSetAspectRatio(aw, ah);

	// rmDispatch();

	return w;
}

void fntCalcDimensions(int font, const unsigned char* str, int *w, int *h) {
	*w = 0;
	*h = 0;

	WaitSema(gFontSemaId);
	font_t *fnt = &fonts[font];
	SignalSema(gFontSemaId);

	uint32_t codepoint;
	uint32_t state = 0;
	FT_Bool use_kerning = FT_HAS_KERNING(fnt->face);
	FT_UInt glyph_index, previous = 0;
	FT_Vector delta;

	// cache glyphs and render as we go
	for (; *str; ++str) {
		unsigned char c = *str;

		if (utf8Decode(&state, &codepoint, c)) // accumulate the codepoint value
			continue;

		// Could just as well only get the glyph dimensions
		// but it is probable the glyphs will be needed anyway
		fnt_glyph_cache_entry_t* glyph = fntCacheGlyph(fnt, codepoint);
		if (!glyph) {
			continue;
		}

		// kerning
		if (use_kerning && previous) {
			glyph_index = FT_Get_Char_Index(fnt->face, codepoint);
			if (glyph_index) {
				FT_Get_Kerning(fnt->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
				*w += delta.x >> 6;
			}
			previous = glyph_index;
		}

		*w += glyph->shx >> 6;
		if (glyph->height > *h)
			*h = glyph->height;
	}
}

void fntReplace(int id, void* buffer, int bufferSize, int takeover, int asDefault) {
	font_t *fnt = &fonts[id];

	font_t ndefault, old;
	fntResetFontDef(&ndefault);
	fntLoadSlot(&ndefault, buffer, bufferSize);
	ndefault.isDefault = asDefault;

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
