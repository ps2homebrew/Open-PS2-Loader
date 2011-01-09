/*
  Copyright 2010, Volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include <stdio.h>
#include <kernel.h>

#include "include/renderman.h"
#include "include/ioman.h"

// Allocateable space in vram, as indicated in GsKit's code
#define __VRAM_SIZE 4194304

GSGLOBAL *gsGlobal;
s32 guiThreadID;

/** Helper texture list */
struct rm_texture_list_t {
	GSTEXTURE *txt;
	struct rm_texture_list_t *next;
};

typedef struct {
	int used;
	int x, y, x1, y1;
} rm_clip_rect_t;

static struct rm_texture_list_t *uploadedTextures = NULL;

static rm_clip_rect_t clipRect;

static int order;

static float aspectWidth;
static float aspectHeight;

// Transposition values - all rendering can be transposed (moved on screen) by these
// this is a post-clipping operation
static float transX = 0;
static float transY = 0;

const u64 gColWhite = GS_SETREG_RGBA(0xFF,0xFF,0xFF,0x00);
const u64 gColBlack = GS_SETREG_RGBA(0x00,0x00,0x00,0x00);
const u64 gColDarker = GS_SETREG_RGBA(0x00,0x00,0x00,0x60);

const u64 gDefaultCol = GS_SETREG_RGBA(0x80,0x80,0x80,0x80);
const u64 gDefaultAlpha = GS_SETREG_ALPHA(0,1,0,1,0);

static float shiftYVal;
static float (*shiftY)(float posY);

static float shiftYFunc(float posY) {
	//return (int) ceil(shiftYVal * posY);
	return (int) (shiftYVal * posY);
}

static float identityFunc(float posY) {
	return posY;
}

static int rmClipQuad(rm_quad_t* quad) {
	if (!clipRect.used)
		return 1;
	
	if (quad->ul.x > clipRect.x1)
		return 0;
	if (quad->ul.y > clipRect.y1)
		return 0;
	if (quad->br.x < clipRect.x)
		return 0;
	if (quad->br.y < clipRect.y)
		return 0;
	
	float dx = quad->br.x - quad->ul.x;
	float dy = quad->br.y - quad->ul.x;
	float du = (quad->br.u - quad->ul.u) / dx;
	float dv = (quad->br.v - quad->ul.v) / dy;
	
	if (quad->ul.x < clipRect.x) {
		// clip
		float d = clipRect.x - quad->ul.x;
		quad->ul.x = clipRect.x;
		quad->ul.u += d * du; 
	}
	
	if (quad->ul.y < clipRect.y) {
		// clip
		float d = clipRect.y - quad->ul.y;
		quad->ul.y = clipRect.y;
		quad->ul.v += d * dv; 
	}
	
	if (quad->br.x > clipRect.x1) {
		// clip
		float d = quad->br.x - clipRect.x1;
		quad->br.x = clipRect.x1;
		quad->br.u -= d * du; 
	}
	
	if (quad->ul.y < clipRect.y) {
		// clip
		float d = quad->br.y - clipRect.y1;
		quad->br.y = clipRect.y1;
		quad->br.v -= d * dv; 
	}
	
	return 1;
};

static void rmAppendUploadedTextures(GSTEXTURE *txt) {
	struct rm_texture_list_t *entry = (struct rm_texture_list_t *)malloc(sizeof(struct rm_texture_list_t));
	entry->txt = txt;
	entry->next = uploadedTextures;
	uploadedTextures = entry;
}


static int rmUploadTexture(GSTEXTURE* txt) {
	u32 size = gsKit_texture_size(txt->Width, txt->Height, txt->PSM);
	// alignment of the allocation
	size = (-GS_VRAM_BLOCKSIZE_256)&(size+GS_VRAM_BLOCKSIZE_256-1);

	// too large to fit VRAM with the currently allocated space?
	if(gsGlobal->CurrentPointer + size >= __VRAM_SIZE)
	{
		
		if (size >= __VRAM_SIZE) {
			// Only log this if the allocation is too large itself
			LOG("RM: Requested allocation is bigger than VRAM!\n"); 
			// We won't allocate this, it's too large
			txt->Vram = GSKIT_ALLOC_ERROR;
			return 0;
		}
		
		// Only log we needed to flush
		// TOO VERBOSE...
		// LOG("RM: Flush needed to free up VRAM\n");
		rmFlush();
	}
	
	txt->Vram = gsGlobal->CurrentPointer;
	gsGlobal->CurrentPointer += size;

	rmAppendUploadedTextures(txt);
	gsKit_texture_upload(gsGlobal, txt);
	return 1;
}

int rmPrepareTexture(GSTEXTURE* txt) {
	if (txt->Vram && txt->Vram != GSKIT_ALLOC_ERROR) // already uploaded
		return 1;
	
	return rmUploadTexture(txt);
}

static void rmDispatchRenderQueue(void) {
	gsKit_queue_exec(gsGlobal);
}

void rmFlush(void) {
	rmDispatchRenderQueue();

	// release all the uploaded textures
	gsKit_vram_clear(gsGlobal);
	
	while (uploadedTextures) {
		uploadedTextures->txt->Vram = 0;
		struct rm_texture_list_t *entry = uploadedTextures;
		uploadedTextures = uploadedTextures->next;
		free(entry);
	}
}

/*static void rmFlip(void) {
	if(!gsGlobal->FirstFrame)
	     gsKit_vsync_wait();

	if(!gsGlobal->FirstFrame)
	{
		//gsKit_vsync();

		if(gsGlobal->DoubleBuffering == GS_SETTING_ON)
		{
			GS_SET_DISPFB2( gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192,
				gsGlobal->Width / 64, gsGlobal->PSM, 0, 0 );
	 
			gsGlobal->ActiveBuffer ^= 1;
			gsGlobal->PrimContext ^= 1;
		}

	}

	gsKit_setactive(gsGlobal);
}*/

void rmStartFrame(void) {
	// gsKit_queue_reset(gsGlobal->Os_Queue);
	order = 0;
}

void rmEndFrame(void) {
	rmFlush();

	if(!gsGlobal->FirstFrame)
	{
		SleepThread();
		
		if(gsGlobal->DoubleBuffering == GS_SETTING_ON)
		{
			GS_SET_DISPFB2( gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192,
				gsGlobal->Width / 64, gsGlobal->PSM, 0, 0 );
	 
			gsGlobal->ActiveBuffer ^= 1;
			gsGlobal->PrimContext ^= 1;
		}

	}

	gsKit_setactive(gsGlobal);	
}

static int rmOnVSync(void) {
	iWakeupThread(guiThreadID);
	return 0;
}

void rmInit(void) {
	gsGlobal = gsKit_init_global();

	gsGlobal->PSM = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->ZBuffering = GS_SETTING_OFF;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->DoubleBuffering = GS_SETTING_ON;

	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
		    D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	// Initialize the DMAC
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_FROMSPR);
	dmaKit_chan_init(DMA_CHANNEL_TOSPR);

	gsKit_init_screen(gsGlobal);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

	gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
	
	order = 0;

	clipRect.used = 0;
	
	aspectWidth = 1.0f;
	aspectHeight = 1.0f;
	
	shiftYVal = 1.0f;
	shiftY = &shiftYFunc;

	transX = 0.0f;
	transY = 0.0f;
	
	// int callback_id = 
	guiThreadID = GetThreadId();
	gsKit_add_vsync_handler(&rmOnVSync);
	
	// reset the contents of the screen to avoid garbage being displayed
	gsKit_clear(gsGlobal, gColBlack);
	gsKit_sync_flip(gsGlobal);
}

void rmGetScreenExtents(int *w, int *h) {
	*w = gsGlobal->Width;
	*h = gsGlobal->Height;
}

void rmEnd(void) {
	rmFlush();
}

void rmDrawQuad(rm_quad_t* q) {
	if (!rmClipQuad(q))
		return;
	
	if (!rmPrepareTexture(q->txt)) // won't render if not ready!
		return;

	if (q->txt->PSM == GS_PSM_CT32)
		gsKit_set_primalpha(gsGlobal, gDefaultAlpha, 0);
	
	gsKit_prim_sprite_texture(gsGlobal, q->txt, q->ul.x + transX, q->ul.y + transY, q->ul.u,
		q->ul.v, q->br.x + transX, q->br.y + transY, q->br.u, q->br.v, order, q->color);
	order++;
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
}

/** If txt is null, don't use DIM_UNDEF size ! */
void rmSetupQuad(GSTEXTURE* txt, int x, int y, short aligned, int w, int h, u64 color, rm_quad_t* q) {
	float drawX, drawY;
	float drawW, drawH;

	if (aligned) {
		if (w != DIM_UNDEF)
			drawW = aspectWidth * w;
		else
			drawW = aspectWidth * txt->Width;
		drawX = x - drawW / 2;

		if (h != DIM_UNDEF)
			drawH = aspectHeight * h;
		else
			drawH = aspectHeight * txt->Height;
		drawY = shiftY(y) - drawH / 2;
	}
	else {
		if (w == DIM_UNDEF)
			drawW = aspectWidth * txt->Width;
		else if (w == DIM_INF)
			drawW = gsGlobal->Width - x;
		else
			drawW = aspectWidth * w;
		drawX = x;

		if (h == DIM_UNDEF)
			drawH = aspectHeight * txt->Height;
		else if (h == DIM_INF)
			drawH = gsGlobal->Height - y;
		else
			drawH = aspectHeight * h;
		drawY = shiftY(y);
	}

	q->ul.x = drawX;
	q->ul.y = drawY;
	q->br.x = drawX + drawW;
	q->br.y = drawY + drawH;

	q->color = color;

	if (txt) {
		q->txt = txt;
		q->ul.u = 0;
		q->ul.v = 0;
		q->br.u = txt->Width;
		q->br.v = txt->Height;
	}
}

void rmDrawPixmap(GSTEXTURE* txt, int x, int y, short aligned, int w, int h, u64 color) {
	rm_quad_t quad;
	rmSetupQuad(txt, x, y, aligned, w, h, color, &quad);
	rmDrawQuad(&quad);
}

void rmDrawOverlayPixmap(GSTEXTURE* overlay, int x, int y, short aligned, int w, int h, u64 color,
		GSTEXTURE* inlay, int ulx, int uly, int urx, int ury, int blx, int bly, int brx, int bry) {
	rm_quad_t quad;
	rmSetupQuad(overlay, x, y, aligned, w, h, color, &quad);

	if (!rmPrepareTexture(inlay))
		return;

	if (inlay->PSM == GS_PSM_CT32)
		gsKit_set_primalpha(gsGlobal, gDefaultAlpha, 0);

	gsKit_prim_quad_texture(gsGlobal, inlay, quad.ul.x + transX + aspectWidth * ulx, quad.ul.y + transY + uly, 0, 0,
			quad.ul.x + transX + aspectWidth * urx, quad.ul.y + transY + ury, inlay->Width, 0,
			quad.ul.x + transX + aspectWidth * blx, quad.ul.y + transY + bly, 0, inlay->Height,
			quad.ul.x + transX + aspectWidth * brx, quad.ul.y + transY + bry, inlay->Width, inlay->Height, order, gDefaultCol);
	order++;
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);

	rmDrawQuad(&quad);
}

void rmDrawRect(int x, int y, short aligned, int w, int h, u64 color) {
	// primitive clipping TODO: repair clipping ...
	/*float x1 = x + w;
	float y1 = y + h;
	
	if (clipRect.used) {
		if (x > clipRect.x1)
			return;
		if (y > clipRect.y1)
			return;
		if (x1 < clipRect.x)
			return;
		if (y1 < clipRect.y)
			return;
		
		if (x < clipRect.x)
			x = clipRect.x;
		
		if (x1 > clipRect.x1)
			x1 = clipRect.x1;
		
		if (y < clipRect.y)
			y = clipRect.y;
		
		if (y1 > clipRect.y1)
			y1 = clipRect.y1;
	}*/

	rm_quad_t quad;
	rmSetupQuad(NULL, x, y, aligned, w, h, color, &quad);
	
	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
	gsKit_prim_quad(gsGlobal, quad.ul.x + transX, quad.ul.y + transY, quad.br.x + transX, quad.ul.y + transY, quad.ul.x + transX, quad.br.y + transY, quad.br.x + transX, quad.br.y + transY, order, quad.color);
	order++;
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0); 
}

void rmDrawLine(int x, int y, int x1, int y1, u64 color) {
	// TODO: clipping. It is more complicated in this case
	gsKit_prim_line(gsGlobal, x + transX, shiftY(y) + transY, x1 + transX, shiftY(y1) + transY, order, color);
}

/** Sets the clipping rectangle */
void rmClip(int x, int y, int w, int h) {
	clipRect.x = x;
	clipRect.y = y;
	if (w == -1)
		clipRect.x1 = gsGlobal->Width;
	else
		clipRect.x1 = x + w;
	if (h == -1)
		clipRect.y1 = gsGlobal->Height;
	else
		clipRect.y1 = y + h;
	clipRect.used = 0; // TODO: 1;
}

/** Sets cipping to none */
void rmUnclip(void) {
	clipRect.used = 0;
}

void rmSetAspectRatio(float width, float height) {
	aspectWidth = width;
	aspectHeight = height;
}

void rmResetAspectRatio() {
	aspectWidth = 1.0f;
	aspectHeight = 1.0f;
}

void rmGetAspectRatio(float *w, float *h) {
	*w = aspectWidth;
	*h = aspectHeight;
}

void rmSetShiftRatio(float shiftYRatio) {
	shiftYVal = shiftYRatio;
	shiftY = &shiftYFunc;
}

void rmResetShiftRatio() {
	shiftY = &identityFunc;
}

void rmSetTransposition(float x, float y) {
	transX = x;
	transY = y;
}
