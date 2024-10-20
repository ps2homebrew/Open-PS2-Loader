#include "include/opl.h"
#include "include/textures.h"
#include "include/util.h"
#include "include/ioman.h"
#include <libjpg_ps2_addons.h>
#include <png.h>

extern void *load0_png;
extern void *load1_png;
extern void *load2_png;
extern void *load3_png;
extern void *load4_png;
extern void *load5_png;
extern void *load6_png;
extern void *load7_png;
extern void *usb_png; // Leave BDM Icon as usb.png to maintain theme compat
extern void *usb_bd_png;
extern void *ilk_bd_png;
extern void *m4s_bd_png;
extern void *hdd_bd_png;
extern void *hdd_png;
extern void *eth_png;
extern void *app_png;
extern void *Index_0_png;
extern void *Index_1_png;
extern void *Index_2_png;
extern void *Index_3_png;
extern void *Index_4_png;

extern void *left_png;
extern void *right_png;
extern void *cross_png;
extern void *triangle_png;
extern void *circle_png;
extern void *square_png;
extern void *select_png;
extern void *start_png;
/* currently unused.
extern void *up_png;
extern void *down_png;
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
extern void *ZSO_png;
extern void *UL_png;
extern void *APPS_png;
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
extern void *apps_case_png;

static int texPngLoad(GSTEXTURE *texture, const char *path);
static int texPngLoadInternal(GSTEXTURE *texture, int texId);
static int texJpgLoad(GSTEXTURE *texture, const char *path);
static int texBmpLoad(GSTEXTURE *texture, const char *path);

// Not related to screen size, just to limit at some point
static int maxSize = 720 * 512 * 4;

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
    {BDM_ICON, "usb", &usb_png},
    {USB_ICON, "usb_bd", &usb_bd_png},
    {ILINK_ICON, "ilk_bd", &ilk_bd_png},
    {MX4SIO_ICON, "m4s_bd", &m4s_bd_png},
    {HDD_BD_ICON, "hdd_bd", &hdd_bd_png},
    {HDD_ICON, "hdd", &hdd_png},
    {ETH_ICON, "eth", &eth_png},
    {APP_ICON, "app", &app_png},
    {INDEX_0, "Index_0", &Index_0_png},
    {INDEX_1, "Index_1", &Index_1_png},
    {INDEX_2, "Index_2", &Index_2_png},
    {INDEX_3, "Index_3", &Index_3_png},
    {INDEX_4, "Index_4", &Index_4_png},
    {LEFT_ICON, "left", &left_png},
    {RIGHT_ICON, "right", &right_png},
    {CROSS_ICON, "cross", &cross_png},
    {TRIANGLE_ICON, "triangle", &triangle_png},
    {CIRCLE_ICON, "circle", &circle_png},
    {SQUARE_ICON, "square", &square_png},
    {SELECT_ICON, "select", &select_png},
    {START_ICON, "start", &start_png},
    /* currently unused.
    {UP_ICON, "up", &up_png},
    {DOWN_ICON, "down", &down_png},
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
    {ZSO_FORMAT, "ZSO", &ZSO_png},
    {UL_FORMAT, "UL", &UL_png},
    {APP_MEDIA, "APP", &APPS_png},
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
    {APPS_CASE_OVERLAY, "apps_case", &apps_case_png},
};

int texLookupInternalTexId(const char *name)
{
    int i;
    int result = -1;

    for (i = 0; i < TEXTURES_COUNT; i++) {
        if (!strcmp(name, internalDefault[i].name)) {
            result = internalDefault[i].id;
            break;
        }
    }

    return result;
}

static int texSizeValidate(int width, int height, short psm)
{
    if (width > 1024 || height > 1024)
        return -1;

    if (gsKit_texture_size(width, height, psm) > maxSize)
        return -1;

    return 0;
}

static void texPrepare(GSTEXTURE *texture)
{
    texture->Width = 0;                 // Must be set by loader
    texture->Height = 0;                // Must be set by loader
    texture->PSM = GS_PSM_CT24;         // Must be set by loader
    texture->ClutPSM = 0;               // Default, can be set by loader
    texture->TBW = 0;                   // gsKit internal value
    texture->Mem = NULL;                // Must be allocated by loader
    texture->Clut = NULL;               // Default, can be set by loader
    texture->Vram = 0;                  // VRAM allocation handled by texture manager
    texture->VramClut = 0;              // VRAM allocation handled by texture manager
    texture->Filter = GS_FILTER_LINEAR; // Default

    // Do not load the texture to VRAM directly, only load it to EE RAM
    texture->Delayed = 1;
}

void texFree(GSTEXTURE *texture)
{
    if (texture->Mem) {
        free(texture->Mem);
        texture->Mem = NULL;
    }
    if (texture->Clut) {
        free(texture->Clut);
        texture->Clut = NULL;
    }
}

typedef int (*fpTexLoad)(GSTEXTURE *texture, const char *path);
struct STexLoader
{
    char *sFileExtension;
    fpTexLoad load;
};
static struct STexLoader texLoader[] = {
    {"png", texPngLoad},
    {"jpg", texJpgLoad},
    {"bmp", texBmpLoad},
    {NULL, NULL}};

int texDiscoverLoad(GSTEXTURE *texture, const char *path, int texId)
{
    char filePath[256];
    int loaderId = 0;

    LOG("texDiscoverLoad(%s)\n", path);

    while (texLoader[loaderId].load != NULL) {
        if (texId != -1)
            snprintf(filePath, sizeof(filePath), "%s%s.%s", path, internalDefault[texId].name, texLoader[loaderId].sFileExtension);
        else
            snprintf(filePath, sizeof(filePath), "%s.%s", path, texLoader[loaderId].sFileExtension);

        int fd = open(filePath, O_RDONLY);
        if (fd > 0) {
            // File found, load it
            close(fd);
            return (texLoader[loaderId].load(texture, filePath) >= 0) ? 0 : ERR_BAD_FILE;
        }

        loaderId++;
    }

    return ERR_BAD_FILE;
}

int texLoadInternal(GSTEXTURE *texture, int texId)
{
    return texPngLoadInternal(texture, texId);
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
} png_texture_t;

static png_texture_t pngTexture;

static int texPngEnd(png_structp pngPtr, png_infop infoPtr, void *pFileBuffer, int status)
{
    if (pFileBuffer != NULL)
        free(pFileBuffer);

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

static void texPngReadPixels4(GSTEXTURE *texture, png_bytep *rowPointers, size_t size)
{
    unsigned char *pixel = (unsigned char *)texture->Mem;
    png_clut_t *clut = (png_clut_t *)texture->Clut;
    int i;

    memset(&clut[pngTexture.numPalette], 0, (16 - pngTexture.numPalette) * sizeof(clut[0]));

    for (i = 0; i < pngTexture.numPalette; i++) {
        clut[i].red = pngTexture.palette[i].red;
        clut[i].green = pngTexture.palette[i].green;
        clut[i].blue = pngTexture.palette[i].blue;
        clut[i].alpha = (i < pngTexture.numTrans) ? (pngTexture.trans[i] >> 1) : 0x80;
    }

    for (i = 0; i < texture->Height; i++)
        memcpy(&pixel[i * (texture->Width / 2)], rowPointers[i], texture->Width / 2);

    for (i = 0; i < size; i++)
        pixel[i] = (pixel[i] << 4) | (pixel[i] >> 4);
}

static void texPngReadPixels8(GSTEXTURE *texture, png_bytep *rowPointers, size_t size)
{
    unsigned char *pixel = (unsigned char *)texture->Mem;
    png_clut_t *clut = (png_clut_t *)texture->Clut;
    int i;

    memset(&clut[pngTexture.numPalette], 0, (256 - pngTexture.numPalette) * sizeof(clut[0]));

    for (i = 0; i < pngTexture.numPalette; i++) {
        clut[i].red = pngTexture.palette[i].red;
        clut[i].green = pngTexture.palette[i].green;
        clut[i].blue = pngTexture.palette[i].blue;
        clut[i].alpha = (i < pngTexture.numTrans) ? (pngTexture.trans[i] >> 1) : 0x80;
    }

    for (i = 0; i < pngTexture.numPalette; i++) {
        if ((i & 0x18) == 8) {
            png_clut_t tmp = clut[i];
            clut[i] = clut[i + 8];
            clut[i + 8] = tmp;
        }
    }

    for (i = 0; i < texture->Height; i++)
        memcpy(&pixel[i * texture->Width], rowPointers[i], texture->Width);
}

static void texPngReadPixels24(GSTEXTURE *texture, png_bytep *rowPointers, size_t size)
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

static void texPngReadPixels32(GSTEXTURE *texture, png_bytep *rowPointers, size_t size)
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
                           void (*texPngReadPixels)(GSTEXTURE *texture, png_bytep *rowPointers, size_t size))
{
    int rowBytes = png_get_rowbytes(pngPtr, infoPtr);
    size_t size = gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM);
    texture->Mem = memalign(128, size);

    // failed allocation
    if (!texture->Mem) {
        LOG("TEXTURES PngReadData: Failed to allocate %d bytes\n", size);
        return;
    }

    png_bytep *rowPointers = calloc(texture->Height, sizeof(png_bytep));

    png_bytep allRows = malloc(rowBytes * texture->Height);
    if (!allRows) {
        free(rowPointers);
        LOG("TEXTURES PngReadData: Failed to allocate memory for PNG rows\n");
        return;
    }

    for (int row = 0; row < texture->Height; row++)
        rowPointers[row] = &allRows[row * rowBytes];

    png_read_image(pngPtr, rowPointers);

    texPngReadPixels(texture, rowPointers, size);

    free(allRows);
    free(rowPointers);

    png_read_end(pngPtr, NULL);
}

static int texPngLoadAll(GSTEXTURE *texture, const char *filePath, int texId)
{
    texPrepare(texture);
    png_structp pngPtr = NULL;
    png_infop infoPtr = NULL;
    png_voidp readData = NULL;
    png_rw_ptr readFunction = NULL;
    void *PngFileBufferPtr;
    void *pFileBuffer = NULL;

    if (filePath) {
        int fd = open(filePath, O_RDONLY, 0);
        if (fd < 0)
            return ERR_BAD_FILE;

        int fileSize = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        pFileBuffer = malloc(fileSize);
        if (pFileBuffer == NULL) {
            close(fd);
            return ERR_BAD_FILE; // There's no out of memory error...
        }

        if (read(fd, pFileBuffer, fileSize) != fileSize) {
            LOG("texPngLoadAll: failed to read file %s\n", filePath);
            free(pFileBuffer);
            close(fd);
            return ERR_BAD_FILE;
        }

        close(fd);

        PngFileBufferPtr = pFileBuffer;
        readData = &PngFileBufferPtr;
        readFunction = &texPngReadMemFunction;
    } else {
        if (texId == -1 || !internalDefault[texId].texture)
            return ERR_BAD_FILE;

        PngFileBufferPtr = internalDefault[texId].texture;
        readData = &PngFileBufferPtr;
        readFunction = &texPngReadMemFunction;
    }

    pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);
    if (!pngPtr)
        return texPngEnd(pngPtr, infoPtr, pFileBuffer, ERR_READ_STRUCT);

    infoPtr = png_create_info_struct(pngPtr);
    if (!infoPtr)
        return texPngEnd(pngPtr, infoPtr, pFileBuffer, ERR_INFO_STRUCT);

    if (setjmp(png_jmpbuf(pngPtr)))
        return texPngEnd(pngPtr, infoPtr, pFileBuffer, ERR_SET_JMP);

    png_set_read_fn(pngPtr, readData, readFunction);

    unsigned int sigRead = 0;
    png_set_sig_bytes(pngPtr, sigRead);
    png_read_info(pngPtr, infoPtr);

    png_uint_32 pngWidth, pngHeight;
    int bitDepth, colorType, interlaceType;
    png_get_IHDR(pngPtr, infoPtr, &pngWidth, &pngHeight, &bitDepth, &colorType, &interlaceType, NULL, NULL);
    texture->Width = pngWidth;
    texture->Height = pngHeight;

    if (bitDepth == 16)
        png_set_strip_16(pngPtr);

    if (colorType == PNG_COLOR_TYPE_GRAY || bitDepth < 4) {
        png_set_expand(pngPtr);
        if (png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(pngPtr);
    }

    png_set_filler(pngPtr, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(pngPtr, infoPtr);

    void (*texPngReadPixels)(GSTEXTURE * texture, png_bytep * rowPointers, size_t size);
    switch (png_get_color_type(pngPtr, infoPtr)) {
        case PNG_COLOR_TYPE_RGB_ALPHA:
            texture->PSM = GS_PSM_CT32;
            texPngReadPixels = &texPngReadPixels32;
            break;
        case PNG_COLOR_TYPE_RGB:
            texture->PSM = GS_PSM_CT24;
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
            } else if (bitDepth == 8) {
                texture->PSM = GS_PSM_T8;
                texture->Clut = memalign(128, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));
                memset(texture->Clut, 0, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));

                texPngReadPixels = &texPngReadPixels8;
            } else
                return texPngEnd(pngPtr, infoPtr, pFileBuffer, ERR_BAD_DEPTH);
            break;
        default:
            return texPngEnd(pngPtr, infoPtr, pFileBuffer, ERR_BAD_DEPTH);
    }

    if (texSizeValidate(texture->Width, texture->Height, texture->PSM) < 0) {
        texFree(texture);

        return texPngEnd(pngPtr, infoPtr, pFileBuffer, ERR_BAD_DIMENSION);
    }

    texPngReadData(texture, pngPtr, infoPtr, texPngReadPixels);

    return texPngEnd(pngPtr, infoPtr, pFileBuffer, 0);
}

static int texPngLoad(GSTEXTURE *texture, const char *filePath)
{
    return texPngLoadAll(texture, filePath, -1);
}

static int texPngLoadInternal(GSTEXTURE *texture, int texId)
{
    return texPngLoadAll(texture, NULL, texId);
}

/// JPG SUPPORT ///////////////////////////////////////////////////////////////////////////////////////


static int texJpgLoad(GSTEXTURE *texture, const char *filePath)
{
    texPrepare(texture);
    int result = ERR_BAD_FILE;
    jpgData *jpg = NULL;

    jpg = jpgFromFilename(filePath, JPG_NORMAL);
    if (jpg) {
        texture->Width = jpg->width;
        texture->Height = jpg->height;
        texture->PSM = GS_PSM_CT24;
        texture->Mem = jpg->buffer;
        free(jpg);
        result = 0;
    }
    return result;
}


/// BMP SUPPORT ///////////////////////////////////////////////////////////////////////////////////////

extern GSGLOBAL *gsGlobal;
static int texBmpLoad(GSTEXTURE *texture, const char *filePath)
{
    texPrepare(texture);

    if (gsKit_texture_bmp(gsGlobal, texture, (char *)filePath) < 0)
        return ERR_BAD_FILE;

    texture->Filter = GS_FILTER_LINEAR;

    if (texSizeValidate(texture->Width, texture->Height, texture->PSM) < 0) {
        texFree(texture);
        return ERR_BAD_DIMENSION;
    }

    return 0;
}
