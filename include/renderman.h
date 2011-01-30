#ifndef __RENDERMAN_H
#define __RENDERMAN_H

#include <gsToolkit.h>

#define DIM_INF			-1
#define DIM_UNDEF		-2

#define ALIGN_NONE		 0
#define ALIGN_CENTER	 1

typedef struct {
	float x, y;
	float u, v;
} rm_tx_coord_t;

typedef struct {
	rm_tx_coord_t ul;
	rm_tx_coord_t br;
	u64 color;
	GSTEXTURE* txt;
} rm_quad_t;

// Some convenience globals
const u64 gColWhite;
const u64 gColBlack;
const u64 gColDarker;
const u64 gColFocus;
const u64 gDefaultCol;
const u64 gDefaultAlpha;

enum rm_vmode {
    RM_VMODE_AUTO = 0,
    RM_VMODE_PAL,
    RM_VMODE_NTSC
} ;

/** Initializes the rendering manager */
void rmInit(int vsyncon, enum rm_vmode vmodeset);

/** Sets a new screen mode */
void rmSetMode(int vsyncon, enum rm_vmode vmodeset);

void rmEnd(void);

/** Fills the parameters with the screen width and height */
void rmGetScreenExtents(int *w, int *h);

/** Manually prepares a texture for rendering (should not be normally needed). 
* txt->Vram will be nonzero on success.
* @param txt The texture to upload (if not uploaded already)
* @return 1 if ok, 0 if error uploading happened (likely too big texture) */
int rmPrepareTexture(GSTEXTURE* txt);

/** Flushes all rendering buffers - renders the gs queue, removes all textures from GS ram */
void rmFlush(void);

/** Issues immediate rendering of the accumulated operations */
void rmDispatch(void);

void rmDrawQuad(rm_quad_t* q);

void rmSetupQuad(GSTEXTURE* txt, int x, int y, short aligned, int w, int h, u64 color, rm_quad_t* q);

/** Queues a specified pixmap (tinted with colour) to be rendered on specified position */
void rmDrawPixmap(GSTEXTURE* txt, int x, int y, short aligned, int w, int h, u64 color);

void rmDrawOverlayPixmap(GSTEXTURE* overlay, int x, int y, short aligned, int w, int h, u64 color,
		GSTEXTURE* inlay, int ulx, int uly, int urx, int ury, int blx, int bly, int brx, int bry);

/** Queues a opaque rectangle to be rendered */
void rmDrawRect(int x, int y, short aligned, int w, int h, u64 color);

/** Queues a single color line to be rendered */
void rmDrawLine(int x, int y, int x1, int y1, u64 color);

/** Starts the frame - first to call every frame */
void rmStartFrame(void);

/** Ends the frame - last to call every frame */
void rmEndFrame(void);

/** Sets the clipping rectangle */
void rmClip(int x, int y, int w, int h);

/** Sets cipping to none */
void rmUnclip(void);

/** Sets the aspect ratio correction for the upcoming operations.
* When set, it will treat all pixmap widths/heights (not positions) as scaled with the ratios provided */
void rmSetAspectRatio(float width, float height);

/** Resets aspect ratio back to 1:1 */
void rmResetAspectRatio();

/** gets the current aspect ratio */
void rmGetAspectRatio(float *w, float *h);

void rmSetShiftRatio(float height);

void rmResetShiftRatio();

/** sets the transposition coordiantes (all content is transposed with these values) */
void rmSetTransposition(float x, float y);

#endif
