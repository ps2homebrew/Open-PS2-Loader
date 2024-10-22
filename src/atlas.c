/*
  Copyright 2010, Volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <stdio.h>
#include <malloc.h>
#include "include/opl.h"
#include "include/atlas.h"
#include "include/renderman.h"

static inline struct atlas_allocation_t *allocNew(int x, int y, size_t width, size_t height)
{
    struct atlas_allocation_t *al = (struct atlas_allocation_t *)malloc(sizeof(struct atlas_allocation_t));

    al->x = x;
    al->y = y;
    al->w = width;
    al->h = height;

    al->leaf1 = NULL;
    al->leaf2 = NULL;

    return al;
}

static inline void allocFree(struct atlas_allocation_t *alloc)
{
    if (!alloc)
        return;

    // NOTE: If used on a tree component it
    // would have to be ensured pointers to freed
    // allocation are fixed

    allocFree(alloc->leaf1); // safe
    allocFree(alloc->leaf2);

    free(alloc);
}

#define ALLOC_FITS(alloc, width, height) \
    ((alloc->w >= width) && (alloc->h >= height))

#define ALLOC_ISFREE(alloc) \
    ((!alloc->leaf1) && (!alloc->leaf2))


static inline struct atlas_allocation_t *allocPlace(struct atlas_allocation_t *alloc, size_t width, size_t height)
{
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
        struct atlas_allocation_t *p = allocPlace(alloc->leaf1, width, height);
        if (p)
            return p;

        p = allocPlace(alloc->leaf2, width, height);
        if (p)
            return p;
    }

    return NULL;
}

atlas_t *atlasNew(size_t width, size_t height, u8 psm)
{
    atlas_t *atlas = (atlas_t *)malloc(sizeof(atlas_t));

    atlas->allocation = allocNew(0, 0, width, height);

    atlas->surface.Width = width;
    atlas->surface.Height = height;

    atlas->surface.Filter = GS_FILTER_NEAREST;

    size_t txtsize = gsKit_texture_size(width, height, psm);
    atlas->surface.PSM = psm;
    atlas->surface.Mem = (u32 *)memalign(128, txtsize);
    atlas->surface.Vram = 0;

    // defaults to no clut
    atlas->surface.ClutPSM = 0;
    atlas->surface.Clut = NULL;
    atlas->surface.VramClut = 0;

    // zero out the atlas surface
    memset(atlas->surface.Mem, 0x0, txtsize);

    return atlas;
}

void atlasFree(atlas_t *atlas)
{
    if (!atlas)
        return;

    allocFree(atlas->allocation);
    atlas->allocation = NULL;

    rmUnloadTexture(&atlas->surface);
    free(atlas->surface.Mem);
    atlas->surface.Mem = NULL;

    free(atlas);
}

static size_t pixelSize(u8 psm)
{
    switch (psm) {
        case GS_PSM_CT32:
            return 4;
        case GS_PSM_CT24:
            return 4;
        case GS_PSM_CT16:
            return 2;
        case GS_PSM_CT16S:
            return 2;
        case GS_PSM_T8:
            return 1;
        default:
            return 0;
    }
}

// copies the data into atlas
static void atlasCopyData(atlas_t *atlas, struct atlas_allocation_t *al, size_t width, size_t height, const void *surface)
{
    int y;
    size_t ps = pixelSize(atlas->surface.PSM);

    if (!ps)
        return;

    const char *src = surface;
    char *data = (char *)atlas->surface.Mem;

    // advance the pointer to the atlas position start (first pixel)
    data += ps * (al->y * atlas->allocation->w + al->x);

    size_t rowsize = width * ps;

    for (y = 0; y < height; ++y) {
        memcpy(data, src, rowsize);
        data += ps * atlas->allocation->w;
        src += ps * width;
    }
}

struct atlas_allocation_t *atlasPlace(atlas_t *atlas, size_t width, size_t height, const void *surface)
{
    if (!surface)
        return NULL;

    struct atlas_allocation_t *al = allocPlace(atlas->allocation, width + 1, height + 1);

    if (!al)
        return NULL;

    atlasCopyData(atlas, al, width, height, surface);

    rmInvalidateTexture(&atlas->surface);

    return al;
}
