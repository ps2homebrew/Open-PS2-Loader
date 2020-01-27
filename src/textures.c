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
/* currently unused.
extern void *L1_png;
extern void *L2_png;
extern void *L3_png;
extern void *R1_png;
extern void *R2_png;
extern void *R3_png; */

extern void *background_png;
extern void *info_png;
extern void *cover_png;
extern void *disc_png;
extern void *screen_png;

extern void *ELF_png;
extern void *HDL_png;
extern void *ISO_png;
extern void *UL_png;
extern void *CD_png;
extern void *DVD_png;
extern void *Aspect_s_png;
extern void *Aspect_w_png;
extern void *Aspect_w1_png;
extern void *Aspect_w2_png;
extern void *Device_1_png;
extern void *Device_2_png;
extern void *Device_3_png;
extern void *Device_4_png;
extern void *Device_5_png;
extern void *Device_6_png;
extern void *Device_all_png;
extern void *Rating_0_png;
extern void *Rating_1_png;
extern void *Rating_2_png;
extern void *Rating_3_png;
extern void *Rating_4_png;
extern void *Rating_5_png;
extern void *Scan_240p_png;
extern void *Scan_240p1_png;
extern void *Scan_480i_png;
extern void *Scan_480p_png;
extern void *Scan_480p1_png;
extern void *Scan_480p2_png;
extern void *Scan_480p3_png;
extern void *Scan_480p4_png;
extern void *Scan_480p5_png;
extern void *Scan_576i_png;
extern void *Scan_576p_png;
extern void *Scan_720p_png;
extern void *Scan_1080i_png;
extern void *Scan_1080i2_png;
extern void *Scan_1080p_png;
extern void *Vmode_multi_png;
extern void *Vmode_ntsc_png;
extern void *Vmode_pal_png;

extern void *logo_png;
extern void *case_png;

// Not related to screen size, just to limit at some point
static int maxSize = 720*512*4;

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
    /* currently unused.
    {L1_ICON, "L1", &L1_png},
    {L2_ICON, "L2", &L2_png},
    {L3_ICON, "L3", &L3_png},
    {R1_ICON, "R1", &R1_png},
    {R2_ICON, "R2", &R2_png},
    {R3_ICON, "R3", &R3_png}, */
    {MAIN_BG, "background", &background_png},
    {INFO_BG, "info", &info_png},
    {COVER_DEFAULT, "cover", &cover_png},
    {DISC_DEFAULT, "disc", &disc_png},
    {SCREEN_DEFAULT, "screen", &screen_png},
    {ELF_FORMAT, "ELF", &ELF_png},
    {HDL_FORMAT, "HDL", &HDL_png},
    {ISO_FORMAT, "ISO", &ISO_png},
    {UL_FORMAT, "UL", &UL_png},
    {CD_MEDIA, "CD", &CD_png},
    {DVD_MEDIA, "DVD", &DVD_png},
    {ASPECT_STD, "Aspect_s", &Aspect_s_png},
    {ASPECT_WIDE, "Aspect_w", &Aspect_w_png},
    {ASPECT_WIDE1, "Aspect_w1", &Aspect_w1_png},
    {ASPECT_WIDE2, "Aspect_w2", &Aspect_w2_png},
    {DEVICE_1, "Device_1", &Device_1_png},
    {DEVICE_2, "Device_2", &Device_2_png},
    {DEVICE_3, "Device_3", &Device_3_png},
    {DEVICE_4, "Device_4", &Device_4_png},
    {DEVICE_5, "Device_5", &Device_5_png},
    {DEVICE_6, "Device_6", &Device_6_png},
    {DEVICE_ALL, "Device_all", &Device_all_png},
    {RATING_0, "Rating_0", &Rating_0_png},
    {RATING_1, "Rating_1", &Rating_1_png},
    {RATING_2, "Rating_2", &Rating_2_png},
    {RATING_3, "Rating_3", &Rating_3_png},
    {RATING_4, "Rating_4", &Rating_4_png},
    {RATING_5, "Rating_5", &Rating_5_png},
    {SCAN_240P, "Scan_240p", &Scan_240p_png},
    {SCAN_240P1, "Scan_240p1", &Scan_240p1_png},
    {SCAN_480I, "Scan_480i", &Scan_480i_png},
    {SCAN_480P, "Scan_480p", &Scan_480p_png},
    {SCAN_480P1, "Scan_480p1", &Scan_480p1_png},
    {SCAN_480P2, "Scan_480p2", &Scan_480p2_png},
    {SCAN_480P3, "Scan_480p3", &Scan_480p3_png},
    {SCAN_480P4, "Scan_480p4", &Scan_480p4_png},
    {SCAN_480P5, "Scan_480p5", &Scan_480p5_png},
    {SCAN_576I, "Scan_576i", &Scan_576i_png},
    {SCAN_576P, "Scan_576p", &Scan_576p_png},
    {SCAN_720P, "Scan_720p", &Scan_720p_png},
    {SCAN_1080I, "Scan_1080i", &Scan_1080i_png},
    {SCAN_1080I2, "Scan_1080i2", &Scan_1080i2_png},
    {SCAN_1080P, "Scan_1080p", &Scan_1080p_png},
    {VMODE_MULTI, "Vmode_multi", &Vmode_multi_png},
    {VMODE_NTSC, "Vmode_ntsc", &Vmode_ntsc_png},
    {VMODE_PAL, "Vmode_pal", &Vmode_pal_png},
    {LOGO_PICTURE, "logo", &logo_png},
    {CASE_OVERLAY, "case", &case_png},
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

static int texSizeValidate(int width, int height, short psm)
{
    if (width > 1024 || height > 1024)
        return -1;

    if (gsKit_texture_size(width, height, psm) > maxSize)
        return -1;

    return 0;
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

typedef struct
{
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;
} png_clut_t;

typedef struct
{
    png_colorp palette;
    int numPalette;
    int numTrans;
    png_bytep trans;
    png_clut_t *clut;
} png_texture_t;

static png_texture_t pngTexture;

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

static void texPngReadPixels4(GSTEXTURE *texture, png_bytep *rowPointers)
{
    unsigned char *pixel = (unsigned char *)texture->Mem;
    png_clut_t *clut = (png_clut_t *)texture->Clut;

    int i, j, k = 0;

    for (i = pngTexture.numPalette; i < 16; i++) {
        memset(&clut[i], 0, sizeof(clut[i]));
    }

    for (i = 0; i < pngTexture.numPalette; i++) {
        clut[i].red = pngTexture.palette[i].red;
        clut[i].green = pngTexture.palette[i].green;
        clut[i].blue = pngTexture.palette[i].blue;
        clut[i].alpha = 0x80;
    }

    for (i = 0; i < pngTexture.numTrans; i++)
        clut[i].alpha = pngTexture.trans[i] >> 1;

    for (i = 0; i < texture->Height; i++) {
        for (j = 0; j < texture->Width / 2; j++) {
            memcpy(&pixel[k], &rowPointers[i][1 * j], 1);
            pixel[k] = (pixel[k] << 4) | (pixel[k] >> 4);
            k++;
        }
    }
}

static void texPngReadPixels8(GSTEXTURE *texture, png_bytep *rowPointers)
{
    unsigned char *pixel = (unsigned char *)texture->Mem;
    png_clut_t *clut = (png_clut_t *)texture->Clut;

    int i, j, k = 0;

    for (i = pngTexture.numPalette; i < 256; i++) {
        memset(&clut[i], 0, sizeof(clut[i]));
    }

    for (i = 0; i < pngTexture.numPalette; i++) {
        clut[i].red = pngTexture.palette[i].red;
        clut[i].green = pngTexture.palette[i].green;
        clut[i].blue = pngTexture.palette[i].blue;
        clut[i].alpha = 0x80;
    }

    for (i = 0; i < pngTexture.numTrans; i++)
        clut[i].alpha = pngTexture.trans[i] >> 1;

    // rotate clut
    for (i = 0; i < pngTexture.numPalette; i++) {
        if ((i&0x18) == 8) {
            png_clut_t tmp = clut[i];
            clut[i] = clut[i+8];
            clut[i+8] = tmp;
        }
    }

    for (i = 0; i < texture->Height; i++) {
        for (j = 0; j < texture->Width; j++) {
            memcpy(&pixel[k++], &rowPointers[i][1 * j], 1);
        }
    }
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
    texUpdate(texture, pngWidth, pngHeight);

    if (colorType == PNG_COLOR_TYPE_GRAY)
        png_set_expand(pngPtr);

    if (bitDepth == 16)
        png_set_strip_16(pngPtr);

    png_set_filler(pngPtr, 0x80, PNG_FILLER_AFTER);
    png_read_update_info(pngPtr, infoPtr);

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
        case PNG_COLOR_TYPE_PALETTE:
            pngTexture.palette = NULL;
            pngTexture.numPalette = 0;
            pngTexture.trans = NULL;
            pngTexture.numTrans = 0;

            png_get_PLTE(pngPtr, infoPtr, &pngTexture.palette, &pngTexture.numPalette);
            png_get_tRNS(pngPtr, infoPtr, &pngTexture.trans, &pngTexture.numTrans, NULL);
            texture->ClutPSM = GS_PSM_CT32;

            if (bitDepth == 4) {
                texture->PSM = GS_PSM_T4;
                texture->Clut = memalign(128, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));
                memset(texture->Clut, 0, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));

                texPngReadPixels = &texPngReadPixels4;
            }
            else {
                texture->PSM = GS_PSM_T8;
                texture->Clut = memalign(128, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));
                memset(texture->Clut, 0, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));

                texPngReadPixels = &texPngReadPixels8;
            }
            break;
        default:
            return texPngEnd(pngPtr, infoPtr, file, ERR_BAD_DEPTH);
    }

    if (texSizeValidate(texture->Width, texture->Height, texture->PSM) < 0) {
        if (texture->Clut) {
            free(texture->Clut);
            texture->Clut = NULL;
        }

        return texPngEnd(pngPtr, infoPtr, file, ERR_BAD_DIMENSION);
    }

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
            if (texSizeValidate(jpg->width, jpg->height, psm) < 0)
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
    char filePath[256];

    if (texId != -1)
        snprintf(filePath, sizeof(filePath), "%s%s.bmp", path, internalDefault[texId].name);
    else
        snprintf(filePath, sizeof(filePath), "%s.bmp", path);

    texture->Delayed = 1;
    if (gsKit_texture_bmp(gsGlobal, texture, filePath) < 0)
        return ERR_BAD_FILE;

    texture->Filter = GS_FILTER_LINEAR;

    if (texSizeValidate(texture->Width, texture->Height, texture->PSM) < 0) {
        if (texture->Mem) {
            free(texture->Mem);
            texture->Mem = NULL;
        }
        if (texture->Clut) {
            free(texture->Clut);
            texture->Clut = NULL;
        }

        return ERR_BAD_DIMENSION;
    }

    return 0;
}
