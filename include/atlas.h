#ifndef __ATLAS_H
#define __ATLAS_H

#include <gsToolkit.h>

struct atlas_allocation_t  {
	int x, y;
	int w, h;
	
	struct atlas_allocation_t *leaf1, *leaf2;
};

typedef struct {
	struct atlas_allocation_t *allocation;

	GSTEXTURE surface;
} atlas_t;

atlas_t *atlasNew(size_t width, size_t height);
void atlasFree(atlas_t *atlas);

/** Allocates a place in atlas for the given pixmap data.
 * Atlas expects 32bit pixels - all the time
 */
struct atlas_allocation_t *atlasPlace(atlas_t *atlas, size_t width, size_t height, u8 psm, void *surface);

#endif
