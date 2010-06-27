#include "include/usbld.h"
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
extern void *exit_png;
extern void *config_png;
extern void *save_png;
extern void *usb_png;
extern void *hdd_png;
extern void *eth_png;
extern void *app_png;
extern void *disc_png;

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

// Not related to screen size, just to limit at some point
static int maxWidth = 640;
static int maxHeight = 512;

typedef struct {
	int id;
	char* name;
	void** texture;
} texture_t;

static texture_t internalDefault[TEXTURES_COUNT] = {
	{ LOAD0_ICON, "load0", &load0_png },
	{ LOAD1_ICON, "load1", &load1_png },
	{ LOAD2_ICON, "load2", &load2_png },
	{ LOAD3_ICON, "load3", &load3_png },
	{ LOAD4_ICON, "load4", &load4_png },
	{ LOAD5_ICON, "load5", &load5_png },
	{ LOAD6_ICON, "load6", &load6_png },
	{ LOAD7_ICON, "load7", &load7_png },
	{ EXIT_ICON, "exit", &exit_png },
	{ CONFIG_ICON, "config", &config_png },
	{ SAVE_ICON, "save", &save_png },
	{ USB_ICON, "usb", &usb_png },
	{ HDD_ICON, "hdd", &hdd_png },
	{ ETH_ICON, "eth", &eth_png },
	{ APP_ICON, "app", &app_png },
	{ DISC_ICON, "disc", &disc_png },
	{ CROSS_ICON, "cross", &cross_png },
	{ TRIANGLE_ICON, "triangle", &triangle_png },
	{ CIRCLE_ICON, "circle", &circle_png },
	{ SQUARE_ICON, "square", &square_png },
	{ SELECT_ICON, "select", &select_png },
	{ START_ICON, "start", &start_png },
	{ LEFT_ICON, "left", &left_png },
	{ RIGHT_ICON, "right", &right_png },
	{ UP_ICON, "up", &up_png },
	{ DOWN_ICON, "down", &down_png },
	{ L1_ICON, "L1", &L1_png },
	{ L2_ICON, "L2", &L2_png },
	{ R1_ICON, "R1", &R1_png },
	{ R2_ICON, "R2", &R2_png },
	{ COVER_OVERLAY, "cover_overlay", NULL },
	{ BACKGROUND_PICTURE, "background", NULL },		// No default background
	{ LOGO_PICTURE, "logo", &logo_png },
};

static void texUpdate(GSTEXTURE* texture, int width, int height) {
	texture->Width =  width;
	texture->Height = height;
}

void texPrepare(GSTEXTURE* texture, short psm) {
	texture->PSM = psm;
	texture->Filter = GS_FILTER_LINEAR;
	texture->Mem = NULL;
	texture->Vram = 0;
	texture->VramClut = 0;
	texture->Clut = NULL;
	//gsKit_setup_tbw(texture); already done in gsKit_texture_upload
}

int texDiscoverLoad(GSTEXTURE* texture, char* path, int texId, short psm) {
	int rc = texPngLoad(texture, path, texId, psm);
	/*
	if (rc < 0) {
		init_scr();
        	scr_clear();
	        scr_printf("PNG EXIT CODE: %d\n", rc);
	        sleep(20);
        }
        */
	if (rc >= 0)
		return 0;
	else if (psm == GS_PSM_CT24)
		return texJpgLoad(texture, path, texId, psm);
	return ERR_BAD_FILE;
}


/// PNG SUPPORT ///////////////////////////////////////////////////////////////////////////////////////


static int texPngEnd(png_structp pngPtr, png_infop infoPtr, FILE* file, int status) {
	if (file != NULL)
		fclose(file);

	if (infoPtr != NULL)
		png_destroy_read_struct(&pngPtr, &infoPtr, (png_infopp) NULL);

	return status;
}

static void texPngReadFunction(png_structp pngPtr, png_bytep data, png_size_t length)
{
	FILE* File = (FILE*) pngPtr->io_ptr;
	if(fread(data, length, 1, File) <= 0)
	{
		png_error(pngPtr, "Error reading via fread\n");
		return;
	}
}

static void texPngReadMemFunction(png_structp pngPtr, png_bytep data, png_size_t length)
{
	u8* memBuffer = (u8*) pngPtr->io_ptr;
	memcpy(data, memBuffer, length);
	pngPtr->io_ptr = memBuffer + length;
}


static void texPngReadPixels24(GSTEXTURE* texture, png_bytep* rowPointers) {
	struct pixel3 { unsigned char r,g,b; };
	struct pixel3 *Pixels = (struct pixel3 *) texture->Mem;

	int i, j, k = 0;
	for (i = 0; i < texture->Height; i++) {
		for (j = 0; j < texture->Width; j++) {
			memcpy(&Pixels[k++], &rowPointers[i][4 * j], 3);
		}
	}
}

static void texPngReadPixels32(GSTEXTURE* texture, png_bytep* rowPointers) {
	struct pixel { unsigned char r,g,b,a; };
	struct pixel *Pixels = (struct pixel *) texture->Mem;

	int i, j, k = 0;
	for (i = 0; i < texture->Height; i++) {
		for (j = 0; j < texture->Width; j++) {
			memcpy(&Pixels[k], &rowPointers[i][4 * j], 3);
			Pixels[k++].a = rowPointers[i][4 * j + 3] >> 1;
		}
	}
}

static void texPngReadData(GSTEXTURE* texture, png_structp pngPtr, png_infop infoPtr,
		void (*texPngReadPixels)(GSTEXTURE* texture, png_bytep *rowPointers)) {
	int row, rowBytes = png_get_rowbytes(pngPtr, infoPtr);
	size_t size = gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM);
	texture->Mem = memalign(128, size);
	
	// failed allocation
	if (!texture->Mem) {
		LOG("texPngReadData: Failed to allocate %d bytes\n", size);
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

int texPngLoad(GSTEXTURE* texture, char* path, int texId, short psm) {
	texPrepare(texture, psm);
	png_structp pngPtr = NULL;
	png_infop infoPtr = NULL;
	png_voidp readData = NULL;
	png_rw_ptr readFunction = NULL;
	FILE* file = NULL;

	if (path) {
		char filePath[255];
		if (texId != -1)
			snprintf(filePath, 255, "%s%s.png", path, internalDefault[texId].name);
		else
			snprintf(filePath, 255, "%s.png", path);

		file = fopen(filePath, "r");
		if (file == NULL)
			return ERR_BAD_FILE;

		readData = file;
		readFunction = &texPngReadFunction;
	}
	else {
		readData = internalDefault[texId].texture;
		if (!readData)
			return ERR_BAD_FILE;

		readFunction = &texPngReadMemFunction;
	}

	pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp) NULL, NULL, NULL);
	if(!pngPtr)
		return texPngEnd(pngPtr, infoPtr, file, ERR_READ_STRUCT);

	infoPtr = png_create_info_struct(pngPtr);
	if(!infoPtr)
		return texPngEnd(pngPtr, infoPtr, file, ERR_INFO_STRUCT);

	if(setjmp(pngPtr->jmpbuf))
		return texPngEnd(pngPtr, infoPtr, file, ERR_SET_JMP);

	png_set_read_fn(pngPtr, readData, readFunction);
	unsigned int sigRead = 0;
	png_set_sig_bytes(pngPtr, sigRead);
	png_read_info(pngPtr, infoPtr);

	png_uint_32 pngWidth, pngHeight;
	int bitDepth, colorType, interlaceType;
	png_get_IHDR(pngPtr, infoPtr, &pngWidth, &pngHeight, &bitDepth, &colorType, &interlaceType, NULL, NULL);
	if (pngWidth > maxWidth || pngHeight > maxHeight)
		return texPngEnd(pngPtr, infoPtr, file, ERR_BAD_DIMENSION);
	texUpdate(texture, pngWidth, pngHeight);

	void (*texPngReadPixels)(GSTEXTURE* texture, png_bytep *rowPointers);
	if(pngPtr->color_type == PNG_COLOR_TYPE_RGB_ALPHA)
	{
		// if PNG have alpha, then it fits for every case (even if we only wanted RGB)
		texture->PSM = GS_PSM_CT32;
		texPngReadPixels = &texPngReadPixels32;
	}
	else if(pngPtr->color_type == PNG_COLOR_TYPE_RGB)
	{
		if (psm != GS_PSM_CT24)
			return texPngEnd(pngPtr, infoPtr, file, ERR_MISSING_ALPHA);

		texPngReadPixels = &texPngReadPixels24;
	}
	else
		return texPngEnd(pngPtr, infoPtr, file, ERR_BAD_DEPTH);

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


int texJpgLoad(GSTEXTURE* texture, char* path, int texId, short psm) {
	texPrepare(texture, GS_PSM_CT24);
	int result = ERR_BAD_FILE;
	jpgData *jpg = NULL;
	char filePath[255];

	if (texId != -1)
		snprintf(filePath, 255, "%s%s.jpg", path, internalDefault[texId].name);
	else
		snprintf(filePath, 255, "%s.jpg", path);
	FILE* file = fopen(filePath, "r");
	if (file) {
		jpg = jpgOpenFILE(file, JPG_NORMAL);
		if (jpg != NULL) {
			if (jpg->width > maxWidth || jpg->height > maxHeight)
				return ERR_BAD_DIMENSION;

			size_t size = gsKit_texture_size_ee(jpg->width, jpg->height, psm);
			texture->Mem = memalign(128, size);
			
			// failed allocation
			if (!texture->Mem) {
				LOG("texJpgLoad: Failed to allocate %d bytes\n", size);
			} else {
				// okay
				texUpdate(texture, jpg->width, jpg->height);
			
				jpgReadImage(jpg, (void*) texture->Mem);
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
