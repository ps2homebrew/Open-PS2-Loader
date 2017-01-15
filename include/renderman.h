#ifndef __RENDERMAN_H
#define __RENDERMAN_H

#include <gsToolkit.h>

/// DTV 576 Progressive Scan (720x576)
#define GS_MODE_DTV_576P 0x53

/// DTV 1080 Progressive Scan (1920x1080)
#define GS_MODE_DTV_1080P 0x54

#define DIM_UNDEF -1

#define ALIGN_NONE 0
#define ALIGN_CENTER 1

#define SCALING_NONE 0
#define SCALING_RATIO 1

/// GSKit CLUT base struct. This should've been in gsKit from the start :)
typedef struct
{
    u8 PSM;       ///< Pixel Storage Method (Color Format)
    u8 ClutPSM;   ///< CLUT Pixel Storage Method (Color Format)
    u32 *Clut;    ///< EE CLUT Memory Pointer
    u32 VramClut; ///< GS VRAM CLUT Memory Pointer
} GSCLUT;

typedef struct
{
    float x, y;
    float u, v;
} rm_tx_coord_t;

typedef struct
{
    rm_tx_coord_t ul;
    rm_tx_coord_t br;
    u64 color;
    GSTEXTURE *txt;
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
    RM_VMODE_NTSC,
    RM_VMODE_DTV480P,
    RM_VMODE_DTV576P,
    RM_VMODE_VGA_640_60
};

/** Initializes the rendering manager */
void rmInit();

/** Sets a new screen mode */
int rmSetMode(int force);

void rmEnd(void);

/** Fills the parameters with the screen width and height */
void rmGetScreenExtents(int *w, int *h);

/** Manually prepares a texture for rendering (should not be normally needed). 
* txt->Vram will be nonzero on success.
* @param txt The texture to upload (if not uploaded already)
* @return 1 if ok, 0 if error uploading happened (likely too big texture) */
int rmPrepareTexture(GSTEXTURE *txt);

/** Flushes all rendering buffers - renders the gs queue, removes all textures from GS ram */
void rmFlush(void);

/** Issues immediate rendering of the accumulated operations */
void rmDispatch(void);

void rmDrawQuad(rm_quad_t *q);

void rmSetupQuad(GSTEXTURE *txt, int x, int y, short aligned, int w, int h, short scaled, u64 color, rm_quad_t *q);

/** Queues a specified pixmap (tinted with colour) to be rendered on specified position */
void rmDrawPixmap(GSTEXTURE *txt, int x, int y, short aligned, int w, int h, short scaled, u64 color);

void rmDrawOverlayPixmap(GSTEXTURE *overlay, int x, int y, short aligned, int w, int h, short scaled, u64 color,
                         GSTEXTURE *inlay, int ulx, int uly, int urx, int ury, int blx, int bly, int brx, int bry);

/** Queues a opaque rectangle to be rendered */
void rmDrawRect(int x, int y, int w, int h, u64 color);

/** Queues a single color line to be rendered */
void rmDrawLine(int x, int y, int x1, int y1, u64 color);

/** Starts the frame - first to call every frame */
void rmStartFrame(void);

/** Ends the frame - last to call every frame */
void rmEndFrame(void);

/** Sets the display offset in units of pixels */
void rmSetDisplayOffset(int x, int y);

/** Sets the aspect ratio correction for the upcoming operations.
* When set, it will treat all pixmap widths/heights (not positions) as scaled with the ratios provided */
void rmSetAspectRatio(float width, float height);

/** Resets aspect ratio back to 1:1 */
void rmResetAspectRatio();

/** gets the current aspect ratio */
void rmGetAspectRatio(float *w, float *h);

void rmApplyAspectRatio(int *w, int *h);

void rmSetShiftRatio(float height);

void rmResetShiftRatio();

void rmApplyShiftRatio(int *y);

/** sets the transposition coordiantes (all content is transposed with these values) */
void rmSetTransposition(float x, float y);

//Returns H-sync frequency in KHz
unsigned char rmGetHsync(void);

#endif
