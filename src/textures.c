#include "include/opl.h"
#include "include/textures.h"
#include "include/util.h"
#include "include/ioman.h"
#include <libjpg.h>
#include <png.h>

extern void *load0_png;
extern void *load1_png;
extern void *load2_png;
extern void *load3_png;
extern void *load4_png;
extern void *load5_png;
extern void *load6_png;
extern void *load7_png;
extern void *usb_png;
extern void *hdd_png;
extern void *eth_png;
extern void *app_png;

extern void *cross_png;
extern void *triangle_png;
extern void *circle_png;
extern void *square_png;
extern void *select_png;
extern void *start_png;
extern void *left_png;
extern void *right_png;
extern void *up_png;
extern void *down_png;
extern void *L1_png;
extern void *L2_png;
extern void *R1_png;
extern void *R2_png;

extern void *logo_png;
extern void *bg_overlay_png;

// Not related to screen size, just to limit at some point
static int maxWidth = 720;
static int maxHeight = 512;

typedef struct
{
    int id;
    char *name;
    void **texture;
} texture_t;

static texture_t internalDefault[TEXTURES_COUNT] = {
    {LOAD0_ICON, "load0", &load0_png},
    {LOAD1_ICON, "load1", &load1_png},
    {LOAD2_ICON, "load2", &load2_png},
    {LOAD3_ICON, "load3", &load3_png},
    {LOAD4_ICON, "load4", &load4_png},
    {LOAD5_ICON, "load5", &load5_png},
    {LOAD6_ICON, "load6", &load6_png},
    {LOAD7_ICON, "load7", &load7_png},
    {USB_ICON, "usb", &usb_png},
    {HDD_ICON, "hdd", &hdd_png},
    {ETH_ICON, "eth", &eth_png},
    {APP_ICON, "app", &app_png},
    {LEFT_ICON, "left", &left_png},
    {RIGHT_ICON, "right", &right_png},
    {UP_ICON, "up", &up_png},
    {DOWN_ICON, "down", &down_png},
    {CROSS_ICON, "cross", &cross_png},
    {TRIANGLE_ICON, "triangle", &triangle_png},
    {CIRCLE_ICON, "circle", &circle_png},
    {SQUARE_ICON, "square", &square_png},
    {SELECT_ICON, "select", &select_png},
    {START_ICON, "start", &start_png},
    {L1_ICON, "L1", &L1_png},
    {L2_ICON, "L2", &L2_png},
    {R1_ICON, "R1", &R1_png},
    {R2_ICON, "R2", &R2_png},
    {LOGO_PICTURE, "logo", &logo_png},
    {BG_OVERLAY, "bg_overlay", &bg_overlay_png},
};

int texLookupInternalTexId(const char *name)
{
    int i, result;

    for (result = -1, i = 0; i < TEXTURES_COUNT; i++) {
        if (!strcmp(name, internalDefault[i].name)) {
            result = internalDefault[i].id;
            break;
        }
    }

    return result;
}

static void texUpdate(GSTEXTURE *texture, int width, int height)
{
    texture->Width = width;
    texture->Height = height;
}

void texPrepare(GSTEXTURE *texture, short psm)
{
    texture->PSM = psm;
    texture->ClutPSM = 0;
    texture->Filter = GS_FILTER_LINEAR;
    texture->Mem = NULL;
    texture->Vram = 0;
    texture->VramClut = 0;
    texture->Clut = NULL;
    //gsKit_setup_tbw(texture); already done in gsKit_texture_upload
}

int texDiscoverLoad(GSTEXTURE *texture, const char *path, int texId, short psm)
{
    if (texPngLoad(texture, path, texId, psm) >= 0)
        return 0;

    if ((psm == GS_PSM_CT24) && texJpgLoad(texture, path, texId, psm) >= 0)
        return 0;

    if (texBmpLoad(texture, path, texId, psm) >= 0)
        return 0;

    return ERR_BAD_FILE;
}


/// PNG SUPPORT ///////////////////////////////////////////////////////////////////////////////////////


static int texPngEnd(png_structp pngPtr, png_infop infoPtr, FILE *file, int status)
{
    if (file != NULL)
        fclose(file);

    if (infoPtr != NULL)
        png_destroy_read_struct(&pngPtr, &infoPtr, (png_infopp)NULL);

    return status;
}

static void texPngReadMemFunction(png_structp pngPtr, png_bytep data, png_size_t length)
{
    void **PngBufferPtr = png_get_io_ptr(pngPtr);

    memcpy(data, *PngBufferPtr, length);
    *PngBufferPtr = (u8 *)(*PngBufferPtr) + length;
}

static void texPngReadPixels24(GSTEXTURE *texture, png_bytep *rowPointers)
{
    struct pixel3
    {
        unsigned char r, g, b;
    };
    struct pixel3 *Pixels = (struct pixel3 *)texture->Mem;

    int i, j, k = 0;
    for (i = 0; i < texture->Height; i++) {
        for (j = 0; j < texture->Width; j++) {
            memcpy(&Pixels[k++], &rowPointers[i][4 * j], 3);
        }
    }
}

static void texPngReadPixels32(GSTEXTURE *texture, png_bytep *rowPointers)
{
    struct pixel
    {
        unsigned char r, g, b, a;
    };
    struct pixel *Pixels = (struct pixel *)texture->Mem;

    int i, j, k = 0;
    for (i = 0; i < texture->Height; i++) {
        for (j = 0; j < texture->Width; j++) {
            memcpy(&Pixels[k], &rowPointers[i][4 * j], 3);
            Pixels[k++].a = rowPointers[i][4 * j + 3] >> 1;
        }
    }
}

static void texPngReadData(GSTEXTURE *texture, png_structp pngPtr, png_infop infoPtr,
                           void (*texPngReadPixels)(GSTEXTURE *texture, png_bytep *rowPointers))
{
    int row, rowBytes = png_get_rowbytes(pngPtr, infoPtr);
    size_t size = gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM);
    texture->Mem = memalign(128, size);

    // failed allocation
    if (!texture->Mem) {
        LOG("TEXTURES PngReadData: Failed to allocate %d bytes\n", size);
        return;
    }

    png_bytep *rowPointers = calloc(texture->Height, sizeof(png_bytep));
    for (row = 0; row < texture->Height; row++) {
        rowPointers[row] = malloc(rowBytes);
    }
    png_read_image(pngPtr, rowPointers);

    texPngReadPixels(texture, rowPointers);

    for (row = 0; row < texture->Height; row++)
        free(rowPointers[row]);

    free(rowPointers);

    png_read_end(pngPtr, NULL);
}

int texPngLoad(GSTEXTURE *texture, const char *path, int texId, short psm)
{
    texPrepare(texture, psm);
    png_structp pngPtr = NULL;
    png_infop infoPtr = NULL;
    png_voidp readData = NULL;
    png_rw_ptr readFunction = NULL;
    FILE *file = NULL;
    void **PngFileBufferPtr;

    if (path) {
        char filePath[256];
        if (texId != -1)
            snprintf(filePath, sizeof(filePath), "%s%s.png", path, internalDefault[texId].name);
        else
            snprintf(filePath, sizeof(filePath), "%s.png", path);

        file = fopen(filePath, "rb");
        if (file == NULL)
            return ERR_BAD_FILE;

        readFunction = NULL; //Use default reading function.
    } else {
        if (!internalDefault[texId].texture)
            return ERR_BAD_FILE;

        PngFileBufferPtr = internalDefault[texId].texture;
        readData = &PngFileBufferPtr;
        readFunction = &texPngReadMemFunction;
    }

    pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);
    if (!pngPtr)
        return texPngEnd(pngPtr, infoPtr, file, ERR_READ_STRUCT);

    infoPtr = png_create_info_struct(pngPtr);
    if (!infoPtr)
        return texPngEnd(pngPtr, infoPtr, file, ERR_INFO_STRUCT);

    if (setjmp(png_jmpbuf(pngPtr)))
        return texPngEnd(pngPtr, infoPtr, file, ERR_SET_JMP);

    if (readFunction != NULL)
        png_set_read_fn(pngPtr, readData, readFunction);
    else
        png_init_io(pngPtr, file);

    unsigned int sigRead = 0;
    png_set_sig_bytes(pngPtr, sigRead);
    png_read_info(pngPtr, infoPtr);

    png_uint_32 pngWidth, pngHeight;
    int bitDepth, colorType, interlaceType;
    png_get_IHDR(pngPtr, infoPtr, &pngWidth, &pngHeight, &bitDepth, &colorType, &interlaceType, NULL, NULL);
    if (pngWidth > maxWidth || pngHeight > maxHeight)
        return texPngEnd(pngPtr, infoPtr, file, ERR_BAD_DIMENSION);
    texUpdate(texture, pngWidth, pngHeight);

    void (*texPngReadPixels)(GSTEXTURE * texture, png_bytep * rowPointers);
    switch (png_get_color_type(pngPtr, infoPtr)) {
        case PNG_COLOR_TYPE_RGB_ALPHA:
            // if PNG have alpha, then it fits for every case (even if we only wanted RGB)
            texture->PSM = GS_PSM_CT32;
            texPngReadPixels = &texPngReadPixels32;
            break;
        case PNG_COLOR_TYPE_RGB:
            if (psm != GS_PSM_CT24)
                return texPngEnd(pngPtr, infoPtr, file, ERR_MISSING_ALPHA);

            texPngReadPixels = &texPngReadPixels24;
            break;
        default:
            return texPngEnd(pngPtr, infoPtr, file, ERR_BAD_DEPTH);
    }

    png_set_strip_16(pngPtr);

    if (colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_expand(pngPtr);

    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
        png_set_expand(pngPtr);

    if (png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(pngPtr);

    png_set_filler(pngPtr, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(pngPtr, infoPtr);
    texPngReadData(texture, pngPtr, infoPtr, texPngReadPixels);

    return texPngEnd(pngPtr, infoPtr, file, 0);
}


/// JPG SUPPORT ///////////////////////////////////////////////////////////////////////////////////////


int texJpgLoad(GSTEXTURE *texture, const char *path, int texId, short psm)
{
    texPrepare(texture, GS_PSM_CT24);
    int result = ERR_BAD_FILE;
    jpgData *jpg = NULL;
    char filePath[256];

    if (texId != -1)
        snprintf(filePath, sizeof(filePath), "%s%s.jpg", path, internalDefault[texId].name);
    else
        snprintf(filePath, sizeof(filePath), "%s.jpg", path);
    FILE *file = fopen(filePath, "rb");
    if (file) {
        jpg = jpgOpenFILE(file, JPG_NORMAL);
        if (jpg != NULL) {
            if (jpg->width > maxWidth || jpg->height > maxHeight)
                return ERR_BAD_DIMENSION;

            size_t size = gsKit_texture_size_ee(jpg->width, jpg->height, psm);
            texture->Mem = memalign(128, size);

            // failed allocation
            if (!texture->Mem) {
                LOG("TEXTURES JpgLoad: Failed to allocate %d bytes\n", size);
            } else {
                // okay
                texUpdate(texture, jpg->width, jpg->height);

                jpgReadImage(jpg, (void *)texture->Mem);
                result = 0;
            }
        }
    }

    if (jpg)
        jpgClose(jpg);

    if (file)
        fclose(file);

    return result;
}


/// BMP SUPPORT ///////////////////////////////////////////////////////////////////////////////////////

extern GSGLOBAL *gsGlobal;
int texBmpLoad(GSTEXTURE *texture, const char *path, int texId, short psm)
{
    texPrepare(texture, GS_PSM_CT24);
    int result = ERR_BAD_FILE;
    char filePath[256];

    if (texId != -1)
        snprintf(filePath, sizeof(filePath), "%s%s.bmp", path, internalDefault[texId].name);
    else
        snprintf(filePath, sizeof(filePath), "%s.bmp", path);

    texture->Delayed = 1;
    result = gsKit_texture_bmp(gsGlobal, texture, filePath);
    texture->Filter = GS_FILTER_LINEAR;

    return result;
}
