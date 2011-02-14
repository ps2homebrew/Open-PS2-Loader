/*
  Copyright 2010, Volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include <stdio.h>
#include "include/atlas.h"
#include <draw_types.h>


static inline struct atlas_allocation_t *allocNew(int x, int y, size_t width, size_t height) {
	struct atlas_allocation_t *al = (struct atlas_allocation_t *)malloc(sizeof(struct atlas_allocation_t));
	
	al->x = x;
	al->y = y;
	al->w = width;
	al->h = height;
	
	al->leaf1 = NULL;
	al->leaf2 = NULL;
	
	return al;
}

static inline void allocFree(struct atlas_allocation_t *alloc) {
	if (!alloc)
		return;
	
	// NOTE: If used on a tree component it 
	// would have to be ensured pointers to freed
	// allocation are fixed
	
	allocFree(alloc->leaf1); // safe
	allocFree(alloc->leaf2);
	
	free(alloc);
}

#define ALLOC_FITS(alloc,width,height) \
	((alloc->w >= width) && (alloc->h >= height))

#define ALLOC_ISFREE(alloc) \
	((!alloc->leaf1) && (!alloc->leaf2))


static inline struct atlas_allocation_t *allocPlace(struct atlas_allocation_t *alloc, size_t width, size_t height) {
	// do we fit?
	if (!ALLOC_FITS(alloc, width, height))
		return NULL;
	
	if (ALLOC_ISFREE(alloc)) {
		// extra space
		size_t dx = alloc->w - width;
		size_t dy = alloc->h - height;
		
		// make the wider piece also longer - less wasted space
		if (dx < dy) {
			alloc->leaf1 = allocNew(alloc->x + width, alloc->y, dx, height);
			alloc->leaf2 = allocNew(alloc->x, alloc->y + height, alloc->w, dy);
		} else {
			alloc->leaf1 = allocNew(alloc->x, alloc->y + height, width, dy);
			alloc->leaf2 = allocNew(alloc->x + width, alloc->y, dx, alloc->h);
		}
		
		return alloc;
	} else {
		// already occupied. Try children
		struct atlas_allocation_t* p = allocPlace(alloc->leaf1, width, height);
		if (p)
			return p;
		
		p = allocPlace(alloc->leaf2, width, height);
		if (p)
			return p;
	}
	
	return NULL;
}

atlas_t *atlasNew(size_t width, size_t height) {
	atlas_t *atlas = (atlas_t*)malloc(sizeof(atlas_t));
	
	atlas->allocation = allocNew(0, 0, width, height);
	
	atlas->surface.Width = width;
	atlas->surface.Height = height;
	
	atlas->surface.Filter = GS_FILTER_LINEAR;
	
	// TODO: Use specified pixel config, or T8/Clut only, 
	/* 
	 atlas->surface.ClutPSM = GS_PSM_CT32;
	 */
	atlas->surface.PSM = GS_PSM_CT32;
	atlas->surface.Mem = (u32*) memalign(128, gsKit_texture_size(width, height, GS_PSM_CT32));
	atlas->surface.Vram = 0;
	
	// zero out the atlas surface
	memset(atlas->surface.Mem, 0, width * height * 4);
	
	return atlas;
}

void atlasFree(atlas_t *atlas) {
	if (!atlas)
		return;
	
	allocFree(atlas->allocation);
	atlas->allocation = NULL;
	
	free(atlas->surface.Mem);
	atlas->surface.Mem = NULL;
	
	free(atlas);
}

struct atlas_allocation_t *atlasPlace(atlas_t *atlas, size_t width, size_t height, u8 psm, void *surface) {
	if ((psm != GS_PSM_CT32) && (psm != GS_PSM_T8))
		return NULL;
	
	if (!surface)
		return NULL;
	
	struct atlas_allocation_t *al = allocPlace(atlas->allocation, width, height);
	
	if (!al)
		return NULL;
	
	
	int x, y;
	
	// copy data
	if (psm == GS_PSM_CT32) {
		u32 *src = (u32*)surface;
		u32 *data = atlas->surface.Mem;
		data += al->y * atlas->allocation->w + al->x;
		
		for (y = 0; y < height; ++y) {
			memcpy(data, surface, width * 4);
			data += atlas->allocation->w;
			src += width;
		}
	} else if (psm == GS_PSM_T8) {
		u8 *src = (u8*)surface;
		u8 *data = (u8*)(atlas->surface.Mem + (al->y * atlas->allocation->w + al->x));
		
		for (y = 0; y < height; ++y) {
			for (x = 0; x < width; ++x) {
				char c = *src++;

				(*data++) = c;
				(*data++) = c;
				(*data++) = c;
				(*data++) = c;
			}
			
			// to the next scanline start
			data += 4 * (atlas->allocation->w - width);
		}
	}
	
	return al;
}
