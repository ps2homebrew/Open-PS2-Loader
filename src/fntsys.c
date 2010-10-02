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

#include <ft2build.h>

#include FT_FREETYPE_H

extern void* freesansfont_raw;
extern int size_freesansfont_raw;

// freetype vars
static FT_Face face;
static FT_GlyphSlot slot;
static FT_Library font_library;

static rm_quad_t quad;

static int gCharHeight;

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

/** GLYPH CACHE. Every glyph in the ASCII range is cached when first used
* this means no additional memory aside from the one needed to render the
* character set is used.
*/
static fnt_glyph_cache_entry_t **glyphCache;
static int cacheMaxPageID;
#define GLYPH_CACHE_PAGE_SIZE 256

static int fntPrepareGlyphCachePage(int pageid) {
	if (pageid > cacheMaxPageID) {
		fnt_glyph_cache_entry_t **np = realloc(glyphCache, (pageid + 1) * sizeof(fnt_glyph_cache_entry_t *));
		
		if (!np)
			return 0;
		
		glyphCache = np;
		
		unsigned int page;
		for (page = cacheMaxPageID + 1; page <= pageid; ++page)
			glyphCache[page] = NULL;
		
		cacheMaxPageID = pageid;
	}
	
	// if it already was allocated, skip this
	if (glyphCache[pageid])
		return 1;
	
	// allocate the page
	glyphCache[pageid] = malloc(sizeof(fnt_glyph_cache_entry_t) * GLYPH_CACHE_PAGE_SIZE);
	
	int i;
	
	for (i = 0; i < GLYPH_CACHE_PAGE_SIZE; ++i) {
		glyphCache[pageid][i].isValid = 0;
		glyphCache[pageid][i].glyphData = NULL; 
	}
	
	return 1;
}

void fntInit(void) {
	cacheMaxPageID = -1;
	glyphCache = NULL;
	
	int error = FT_Init_FreeType( &font_library );

	if ( error ) {
		// just report over the ps2link
		LOG("Freetype init failed with %x!\n", error);
		// SleepThread();
	}

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

static void fntCacheFlush(void) {
	int i;

	// Release all the glyphs from the cache
	for (i = 0; i <= cacheMaxPageID; ++i) {
		if (glyphCache[i]) {
			fntCacheFlushPage(glyphCache[i]);
			free(glyphCache[i]);
			glyphCache[i] = NULL;
		}
	}
	
	free(glyphCache);
	glyphCache = NULL;
	cacheMaxPageID = -1;
}

void fntLoad(void* buffer, int bufferSize, short clean) {
	if (clean)
		fntCacheFlush();

	if (!buffer) {
		buffer = &freesansfont_raw;
		bufferSize = size_freesansfont_raw;
	}

	// load the font via memory handle
	int error = FT_New_Memory_Face( font_library, (FT_Byte*) buffer,  bufferSize,  0,  &face );
	
	if ( error ) { 
		// just report over the ps2link
		LOG("Freetype: Font loading failed with %x!\n", error);
		// SleepThread();
	} 
	
	gCharHeight = 16;
	
	error = FT_Set_Char_Size(face, 0, gCharHeight * 16, 300, 300);
	/*error = FT_Set_Pixel_Sizes( face,
		0, // pixel_width
		gCharHeight ); // pixel_height*/

	if ( error ) { 
		// just report over the ps2link
		LOG("Freetype: Error setting font pixel size with %x!\n", error);
		// SleepThread();
	}
}

/** Terminates the font rendering system */
void fntEnd(void) {
	// release all the glyphs
	fntCacheFlush();
	
	// deinit freetype system
	FT_Done_FreeType( font_library );
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
static fnt_glyph_cache_entry_t* fntCacheGlyph(uint32_t gid) {
	// calc page id and in-page index from glyph id
	int pageid = gid / GLYPH_CACHE_PAGE_SIZE;
	int idx = gid % GLYPH_CACHE_PAGE_SIZE;
	
	if (!fntPrepareGlyphCachePage(pageid)) // failed to prepare the page...
		return NULL;
	
	fnt_glyph_cache_entry_t *page = glyphCache[pageid];
	
	if (!page) // safeguard
		return NULL;
	
	fnt_glyph_cache_entry_t* glyph = &page[idx];
	
	if (glyph->isValid)
		return glyph;

	slot = face->glyph;
	
	// not cached but valid. Cache
	glyph->isValid = 1;
	
	if (!face) {
		LOG("FNT: Face is NULL!\n");
	}
	
	int error = FT_Load_Char( face, gid, FT_LOAD_RENDER ); 
	
	if (error) {
		LOG("FNT: Error loading glyph - %d\n", error);
		return glyph;
	}
	
	// copy the data
	int i, j = 0;
	int pixelcount = slot->bitmap.width * slot->bitmap.rows;
	
	// If we would know how to render grayscale (single channel) it could save memory
	glyph->glyphData = malloc(pixelcount * 4);
	
	for (i = 0; i < pixelcount; ++i) {
		char c = slot->bitmap.buffer[i];
		
		glyph->glyphData[j++] = c;
		glyph->glyphData[j++] = c;
		glyph->glyphData[j++] = c;
		glyph->glyphData[j++] = c;
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
	fntCacheFlush();
	
	// set new aspect ratio (Is this correct, I wonder?)
	// TODO: error = FT_Set_Char_Size(face, 0, gCharHeight*64, ah*300, aw*300);
}

int fntRenderString(int x, int y, short aligned, const unsigned char* string, u64 colour) {
	if (aligned) {
		int w = 0, h = 0;
		fntCalcDimensions(string, &w, &h);
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
		
		fnt_glyph_cache_entry_t* glyph = fntCacheGlyph(codepoint);
		
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

void fntCalcDimensions(const unsigned char* str, int *w, int *h) {
	*w = 0;
	*h = 0;
	
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
		fnt_glyph_cache_entry_t* glyph = fntCacheGlyph(codepoint);
		
		*w += glyph->shx / 64;
		*h = glyph->height;
	}
}
