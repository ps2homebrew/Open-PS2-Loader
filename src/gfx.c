/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/


#include "include/usbld.h"
#include "include/gfx.h"
#include "include/lang.h"
#include "include/pad.h"
#include <assert.h>

#define yfix(x) (x*gsGlobal->Height)/480
#define yfix2(x) x+(gsGlobal->Height==512 ? x*0.157f : 0)

					
extern void *font_raw;
extern int size_font_raw;

extern void *font_cyrillic_raw;
extern int size_font_cyrillic_raw;

extern void *exit_raw;
extern int size_exit_raw;

extern void *config_raw;
extern int size_config_raw;

extern void *games_raw;
extern int size_games_raw;

extern void *disc_raw;
extern int size_disc_raw;

extern void *theme_raw;
extern int size_theme_raw;

extern void *language_raw;
extern int size_language_raw;

extern void *apps_raw;
extern int size_apps_raw;

extern void *menu_raw;
extern int size_menu_raw;

extern void *scroll_raw;
extern int size_scroll_raw;

extern void *usb_raw;
extern int size_usb_raw;

extern void *save_raw;
extern int size_save_raw;

extern void *netconfig_raw;
extern int size_netconfig_raw;

extern void *network_raw;
extern int size_network_raw;

extern void *cross_raw;
extern int size_cross_raw;

extern void *triangle_raw;
extern int size_triangle_raw;

extern void *circle_raw;
extern int size_circle_raw;

extern void *square_raw;
extern int size_square_raw;

extern void *select_raw;
extern int size_select_raw;

extern void *start_raw;
extern int size_start_raw;

extern void *up_dn_raw;
extern int size_up_dn_raw;

extern void *lt_rt_raw;
extern int size_lt_rt_raw;

GSTEXTURE background;
GSTEXTURE background2;

// ID's of the texures
#define DISC_ICON 0
#define GAMES_ICON 1
#define CONFIG_ICON 2
#define EXIT_ICON 3
#define THEME_ICON 4
#define LANGUAGE_ICON 5
#define APPS_ICON 6
#define MENU_ICON 7
#define SCROLL_ICON 8
#define USB_ICON 9
#define NETCONFIG_ICON 10
#define NETWORK_ICON 11

GSTEXTURE disc_icon;
GSTEXTURE games_icon;
GSTEXTURE config_icon;
GSTEXTURE exit_icon;
GSTEXTURE theme_icon;
GSTEXTURE language_icon;
GSTEXTURE apps_icon;
GSTEXTURE menu_icon;
GSTEXTURE scroll_icon;
GSTEXTURE usb_icon;
GSTEXTURE save_icon;
GSTEXTURE netconfig_icon;
GSTEXTURE network_icon;

// Button icons
GSTEXTURE cross_icon;
GSTEXTURE triangle_icon;
GSTEXTURE square_icon;
GSTEXTURE circle_icon;
GSTEXTURE select_icon;
GSTEXTURE start_icon;
GSTEXTURE up_dn_icon;
GSTEXTURE lt_rt_icon;


u64 gColWhite = GS_SETREG_RGBAQ(0xFF,0xFF,0xFF,0x00,0x00);
u64 gColBlack = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00);
u64 gColDarker = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x60,0x00);
u64 gColText = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);

u64 Wave_a = GS_SETREG_RGBAQ(0x00,0x00,0x80,0x20,0x00);
u64 Wave_b = GS_SETREG_RGBAQ(0x00,0x00,0x40,0x20,0x00);
u64 Wave_c = GS_SETREG_RGBAQ(0x00,0x00,0xff,0x20,0x00);
u64 Wave_d = GS_SETREG_RGBAQ(0x00,0x00,0x20,0x20,0x00);

u64 Background_a = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00);
u64 Background_b = GS_SETREG_RGBAQ(0x00,0x00,0x20,0x00,0x00);
u64 Background_c = GS_SETREG_RGBAQ(0x00,0x00,0x20,0x00,0x00);
u64 Background_d = GS_SETREG_RGBAQ(0x00,0x00,0x80,0x00,0x00);

char *infotxt;
int gZ;

// Count of icons before and after the selected one
unsigned int _vbefore = 1;
unsigned int _vafter = 1;
// spacing between icons, vertical.
unsigned int _vspacing = 80;

// forward decls.
void MsgBox();
void drawConfig();
void LoadResources();
void DestroyMenu();

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Initialization/Destruction of gfx ----------------
// -------------------------------------------------------------------------------------------
void initGFX() {
	waveframe=0;
	frame=0;
	h_anim=100;
	v_anim=100;
	direc=0;
	max_settings=2;
	gFont.Vram = 0;
	background.Mem = 0;
	background2.Mem = 0;
	dynamic_menu = 1;
	scroll_speed = 1;
	gConfig.head = NULL;
	gConfig.tail = NULL;

	bg_color[0]=0x00;
	bg_color[1]=0x00;
	bg_color[2]=0xff;
	
	text_color[0]=0xff;
	text_color[1]=0xff;
	text_color[2]=0xff;

	default_bg_color[0] = 0x00;
	default_bg_color[1] = 0x00;
	default_bg_color[2] = 0xff;	

	default_text_color[0]=0xff;
	default_text_color[1]=0xff;
	default_text_color[2]=0xff;


	infotxt = _l(_STR_WELCOME); // be sure to replace after doing a language change!
	
	// defaults
	setConfigInt(&gConfig, "scrolling", scroll_speed);
	setConfigInt(&gConfig, "menutype", dynamic_menu);
	setConfigStr(&gConfig, "theme", "");

	gsGlobal = gsKit_init_global();

	gsGlobal->PSM = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->ZBuffering = GS_SETTING_OFF;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	
	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
		    D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	// Initialize the DMAC
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_FROMSPR);
	dmaKit_chan_init(DMA_CHANNEL_TOSPR);

	gsKit_init_screen(gsGlobal);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

	// reset the contents of the screen to avoid garbage being displayed
	flip();
}

void DestroyGFX() {
	/// TODO: un-initialize the GFX
	
	DestroyMenu();
}

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
void initMenu() {
	LoadResources();

	menu = NULL;
	selected_item = NULL;
}

void DestroyMenu() {
	// destroy menu
	struct menu_list_t *cur = menu;
	
	while (cur) {
		struct menu_list_t *td = cur;
		cur = cur->next;
		
		if (&td->item)
			destroySubMenu(&td->item->submenu);
		
		free(td);
	}
}

struct menu_list_t* AllocMenuItem(struct menu_item_t* item) {
	struct menu_list_t* it;
	
	it = malloc(sizeof(struct menu_list_t));
	
	it->prev = NULL;
	it->next = NULL;
	it->item = item;
	
	return it;
}

void appendMenuItem(struct menu_item_t* item) {
	assert(item);
	
	if (menu == NULL) {
		menu = AllocMenuItem(item);
		return;
	}
	
	struct menu_list_t *cur = menu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	struct menu_list_t *newitem = AllocMenuItem(item);
	
	// link
	cur->next = newitem;
	newitem->prev = cur;
}


struct submenu_list_t* AllocSubMenuItem(GSTEXTURE *icon, char *text, int id, int text_id) {
	struct submenu_list_t* it;
	
	it = malloc(sizeof(struct submenu_list_t));
	
	it->prev = NULL;
	it->next = NULL;
	it->item.icon = icon;
	it->item.text = text;
	it->item.text_id = text_id;
	it->item.id = id;
	
	return it;
}

struct submenu_list_t* appendSubMenu(struct submenu_list_t** submenu, GSTEXTURE *icon, char *text, int id, int text_id) {
	if (*submenu == NULL) {
		*submenu = AllocSubMenuItem(icon, text, id, text_id);
		return *submenu; 
	}
	
	struct submenu_list_t *cur = *submenu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	struct submenu_list_t *newitem = AllocSubMenuItem(icon, text, id, text_id);
	
	// link
	cur->next = newitem;
	newitem->prev = cur;
	
	return newitem;
}

void destroySubMenu(struct submenu_list_t** submenu) {
	// destroy sub menu
	struct submenu_list_t *cur = *submenu;
	
	while (cur) {
		struct submenu_list_t *td = cur;
		cur = cur->next;
		free(td);
	}
	
	*submenu = NULL;
}

char *GetMenuItemText(struct menu_item_t* it) {
	if (it->text_id >= 0)
		return _l(it->text_id);
	else
		return it->text;
}

char *GetSubItemText(struct submenu_item_t* it) {
	if (it->text_id >= 0)
		return _l(it->text_id);
	else
		return it->text;
}

void swap(struct submenu_list_t* a, struct submenu_list_t* b) {
	struct submenu_list_t *pa, *nb;
	pa = a->prev;
	nb = b->next;
	
	a->next = nb;
	b->prev = pa;
	b->next = a;
	a->prev = b;
	
	if (pa)
		pa->next = b;
	
	if (nb)
		nb->prev = a;
}

// Sorts the given submenu by comparing the on-screen titles
void sortSubMenu(struct submenu_list_t** submenu) {
	// a simple bubblesort
	// *submenu = mergeSort(*submenu);
	struct submenu_list_t *head = *submenu;
	int sorted = 0;
	
	if ((submenu == NULL) || (*submenu == NULL) || ((*submenu)->next == NULL))
		return;
	
	while (!sorted) {
		sorted = 1;
		
		struct submenu_list_t *tip = head;
		
		while (tip->next) {
			struct submenu_list_t *nxt = tip->next;
			
			char *txt1 = GetSubItemText(&tip->item);
			char *txt2 = GetSubItemText(&nxt->item);
			
			int cmp = strcmp(txt1, txt2);
			
			if (cmp > 0) {
				swap(tip, nxt);
				
				if (tip == head) 
					head = nxt;
				
				sorted = 0;
			} else {
				tip = tip->next;
			}
		}
	}
	
	*submenu = head;
}

void updateScrollSpeed() {
	// sanitize the settings
	if ((scroll_speed < 0) || (scroll_speed > 2))
		scroll_speed = 1;
	
	// update the pad delays for KEY_UP and KEY_DOWN
	// default delay is 7
	setButtonDelay(KEY_UP, 10 - scroll_speed * 3); // 0,1,2 -> 10, 7, 4
	setButtonDelay(KEY_DOWN, 10 - scroll_speed * 3);
}

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Rendering ----------------------------------------
// -------------------------------------------------------------------------------------------
void drawQuad(GSGLOBAL *gsGlobal, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int z, u64 color) {
	gsKit_prim_quad(gsGlobal,x1,yfix(y1),x2,yfix(y2),x3,yfix(y3),x4,yfix(y4),z,color);
}

void DrawQuad_gouraud(GSGLOBAL *gsGlobal, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int z, u64 color1, u64 color2, u64 color3, u64 color4){
	gsKit_prim_quad_gouraud(gsGlobal,x1,yfix(y1),x2,yfix(y2),x3,yfix(y3),x4,yfix(y4),z,color1,color2,color3,color4);
}

void drawLine(GSGLOBAL *gsGlobal,float x1, float y1,float x2, float y2, int z, u64 color) {
	gsKit_prim_line(gsGlobal, x1, yfix(y1), x2, yfix(y2), z, color);						
}

void DrawSprite_texture(GSGLOBAL *gsGlobal, const GSTEXTURE *Texture, float x1, float y1, float u1, float v1, float x2, int y2, float u2, float v2, int z, u64 color){
	gsKit_prim_sprite_texture(gsGlobal, Texture, x1, yfix(y1), u1, v1, x2, yfix(y2), u2, v2, z, color);
}

void flip(){
	gsKit_queue_exec(gsGlobal);
	gsKit_sync_flip(gsGlobal);
	gsKit_clear(gsGlobal, gColBlack);
}

void openIntro(){
	char introtxt[255];

	for(frame=0;frame<75;frame++){
		if(frame<=25){
			textColor(0x80,0x80,0x80,frame*4);
		}else if(frame>75 && frame<100){
			textColor(0x80,0x80,0x80,0x80-(frame-75)*4);
		}else if(frame>100){
			textColor(0x80,0x80,0x80,0x00);
			waveframe+=0.1f;
		}

		drawBackground();
		snprintf(introtxt, 255, _l(_STR_OUL_VER), USBLD_VERSION);
		drawText(270, 255, introtxt, 1.0f, 0);
		flip();
	}
}

void closeIntro(){
	char introtxt[255];
	int bg_loaded = loadBackground();

	for(frame=75;frame<125;frame++){
		if(frame>100){
			textColor(0x80,0x80,0x80,0x00);
			waveframe+=0.1f;
		}

		drawBackground();
		snprintf(introtxt, 255, _l(_STR_OUL_VER), USBLD_VERSION);
		drawText(270, 255, introtxt, 1.0f, 0);
		flip();
	}
	background_image=bg_loaded;
}

void drawWave(int y, int xoffset){
	int w=0;
	float h=0;
	for(w=32;w<=640;w=w+32){
		gZ++;
		DrawQuad_gouraud(gsGlobal, w-32, y+50+sinf(waveframe+h+xoffset)*25.0f, w, y+50+sinf(waveframe+xoffset+h+0.1f)*25.0f, w-32, y-50+sinf(waveframe+h+xoffset)*-25.0f, w, y-50+sinf(waveframe+xoffset+h+0.1f)*-25.0f, gZ, Wave_b, Wave_b, Wave_a, Wave_a);
		DrawQuad_gouraud(gsGlobal, w-32, y-30+sinf(waveframe+h+xoffset)*-25.0f, w, y-30+sinf(waveframe+xoffset+h+0.1f)*-25.0f, w-32, y-50+sinf(waveframe+h+xoffset)*-25.0f, w, y-50+sinf(waveframe+xoffset+h+0.1f)*-25.0f, gZ, Wave_a, Wave_a, Wave_c, Wave_c);
		DrawQuad_gouraud(gsGlobal, w-32, y+50+sinf(waveframe+h+xoffset)*25.0f, w, y+50+sinf(waveframe+xoffset+h+0.1f)*25.0f, w-32, y+30+sinf(waveframe+h+xoffset)*25.0f, w, y+30+sinf(waveframe+xoffset+h+0.1f)*25.0f, gZ, Wave_d, Wave_d, Wave_b, Wave_b);
		h+=0.1f;
	}
}

void gfxRestoreConfig() {
	char *temp;
	
	// reinit all the values we are interested in from the config
	if (getConfigInt(&gConfig, "scrolling", &scroll_speed))
		updateScrollSpeed();
	
	getConfigInt(&gConfig, "menutype", &dynamic_menu);
	if (getConfigStr(&gConfig, "theme", &temp))
		strncpy(theme, temp, 32);
	if (getConfigColor(&gConfig, "bgcolor", default_bg_color))
		setColor(default_bg_color[0],default_bg_color[1],default_bg_color[2]);
				
	if (getConfigColor(&gConfig, "textcolor", default_text_color))
		textColor(default_text_color[0], default_text_color[1], default_text_color[2], 0xff);
	
	if ((dynamic_menu < 0) || (dynamic_menu > 1)) {
		dynamic_menu = 1;
		setConfigInt(&gConfig, "menutype", dynamic_menu);
	}
	
	if (getConfigInt(&gConfig, "language", &gLanguageID))
		setLanguage(gLanguageID);
		
	getConfigInt(&gConfig, "exitmode", &exit_mode);
	getConfigInt(&gConfig, "default_device", &default_device);
}

void gfxStoreConfig() {
	// set the config values to be sure they are up-to-date
	setConfigInt(&gConfig, "scrolling", scroll_speed);
	setConfigInt(&gConfig, "menutype", dynamic_menu);
	setConfigStr(&gConfig, "theme", theme);
	setConfigColor(&gConfig, "bgcolor", default_bg_color);
	setConfigColor(&gConfig, "textcolor", default_text_color);
	setConfigInt(&gConfig, "language", gLanguageID);
	setConfigInt(&gConfig, "exitmode", exit_mode);
	setConfigInt(&gConfig, "default_device", default_device);
}

void LoadResources() {
	readIPConfig();
	findTheme();
	loadIcons();
	updateIcons();
	loadFont(0);
	updateFont();
}

void drawBackground(){
	gZ=1;

	// The VRAM is too small to fit both the background and other images (icons + font)
	// Thus the vram is cleared every frame, the background uploaded and drawn, then texture part of vram cleared again 
	// Then the UpdateIcons uploads icons to vram and UpdateFont does the same for the font, before anything using them is rendered
	gsKit_vram_clear(gsGlobal);

	GSTEXTURE* cback = NULL;
	
	if (dynamic_menu) {
		cback = &background;
	} else {
		cback = &background2;
		
		// if no second background is given, use the first
		if (!cback->Mem)
			cback = &background;
	}
	//BACKGROUND
	if((background_image != 0) && cback && cback->Mem) {
		uploadTexture(cback);
	
		DrawSprite_texture(gsGlobal, cback, 0.0f,  // X1
			0.0f,  // Y2
			0.0f,  // U1
			0.0f,  // V1
			640.0f, // X2
			yfix(480), // Y2
			background.Width, // U2
			background.Height, // V2
			gZ,
			GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
		
		gsKit_queue_exec(gsGlobal);
		gsKit_vram_clear(gsGlobal);
	}else{
		DrawQuad_gouraud(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, 2, Background_a, Background_b, Background_c, Background_d);
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		drawWave(240,0);
		drawWave(260,2);
		drawWave(256,4);
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		if(waveframe>6.28f){
			waveframe=0;
		}else{
			waveframe+=0.01f;
		}
	}
	updateIcons();
	updateFont();
}

void setColor(int r, int g, int b){
	Wave_a = GS_SETREG_RGBAQ(r/2,g/2,b/2,0x20,0x00);
	Wave_b = GS_SETREG_RGBAQ(r/4,g/4,b/4,0x20,0x00);
	Wave_c = GS_SETREG_RGBAQ(r,g,b,0x20,0x00);
	Wave_d = GS_SETREG_RGBAQ(r/8,g/8,b/8,0x20,0x00);

	Background_a = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00);
	Background_b = GS_SETREG_RGBAQ(r/8,g/8,b/8,0x00,0x00);
	Background_c = GS_SETREG_RGBAQ(r/8,g/8,b/8,0x00,0x00);
	Background_d = GS_SETREG_RGBAQ(r/2,g/2,b/2,0x00,0x00);
}

void textColor(int r, int g, int b, int a) {
	gColText = GS_SETREG_RGBAQ(r,g,b,a,0x00);
}

void drawText(int x, int y, const char *texto, float scale, int centered){
	if(centered==1){
		x=x-((strlen(texto)-1)*gsFont->CharWidth/2);
	}

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
	gsKit_font_print_scaled(gsGlobal, gsFont, x, yfix(y), 1, scale, gColText, texto);
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
}

/// Uploads texture to vram
void uploadTexture(GSTEXTURE* txt) {
	txt->Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(txt->Width, txt->Height, txt->PSM), GSKIT_ALLOC_USERBUFFER);
	
	/* // VRAM ALLOCATION DEBUGGING
	if (txt->Vram == GSKIT_ALLOC_ERROR)
		printf("!!!Bad Vram allocation!!!");
	*/
	
	gsKit_texture_upload(gsGlobal, txt);
}

void msgBox(char* text){
	while(1){
		readPads();
				
		drawScreen();
		
		textColor(0xff,0xff,0xff,0xff);
		
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		drawQuad(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, gZ, gColDarker);
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		drawLine(gsGlobal, 50.0f, 75.0f, 590.0f, 75.0f, 1, gColWhite);
		drawLine(gsGlobal, 50.0f, 410.0f, 590.0f, 410.0f, 1, gColWhite);
		drawText(310, 250, text, 1.0f, 1);
		drawText(400, 417, _l(_STR_O_BACK), 1.0f, 0);
		
		flip();
		if(getKey(KEY_CIRCLE))break;
	}
}

void loadFont(int load_default){
		
	char tmp[255];
	
	gFont.Width = 256;
	gFont.Height = 256;
	gFont.PSM = GS_PSM_CT32;
	snprintf(tmp, 255, "font.raw");

	if(load_default==1 || loadRAW(tmp, &gFont)==0){
		gFont.Mem=(u32*)&font_raw;
	}
	
	if (langIsCyrillic()) {
		gFont.Mem=(u32*)&font_cyrillic_raw;
	}

	gsFont = calloc(1,sizeof(GSFONT));
	gsFont->Texture = &gFont;
	gsFont->Additional=calloc(1,sizeof(short)*256);
	
	gsFont->HChars=16;
	gsFont->VChars=16;
	gsFont->CharWidth = gsFont->Texture->Width / 16;
	gsFont->CharHeight = gsFont->Texture->Height / 16;			
	
	int i;
	
	for (i=0; i<512; i++) {
		gsFont->Additional[i] = gsFont->CharWidth;
	}

}

void updateFont() {
	uploadTexture(&gFont);
}

int loadRAW(char *path, GSTEXTURE *Texture) {
	int fd;
	unsigned int size;
	char file[255];
	
	void *buffer;

	if(theme_zipped==0){
		snprintf(file, 255, "%s%s/%s", theme_prefix, theme, path);
		fd=fioOpen(file, O_RDONLY);
		
		if (fd < 0) // invalid file name, etc.
			return 0;
			
		if(Texture->Mem!=NULL) {
			free(Texture->Mem);
			Texture->Mem = NULL;
		}


		size = fioLseek(fd, 0, SEEK_END);  
		fioLseek(fd, 0, SEEK_SET);
		
		// TODO: should check the texture size... Is this the valid way?
		// if (size != gsKit_texture_size(txt->Width, txt->Height, txt->PSM)) {
		//	fioClose(fd);
		//	return 0;
		// }
		
		buffer=memalign(128, size); // The allocation is alligned to aid the DMA transfers
		fioRead(fd, buffer, size);
		
		Texture->Mem=buffer;
		
		fioClose(fd);
	}else{
		if(zipfile==NULL){
			snprintf(file, 255, "%s%s.zip", theme_prefix, theme);
			zipfile = unzOpen(file);
			if(zipfile==NULL){
				snprintf(file, 255, "%s%s.ZIP", theme_prefix, theme);
				zipfile = unzOpen(file);
			}
		}
		if(zipfile==NULL)return 0;
			unz_file_info info;
			
			if(unzLocateFile(zipfile,path,1) == UNZ_OK){
				char name[132];
				unzGetCurrentFileInfo(zipfile, &info, name,128, NULL,0, NULL,0);
				size = info.uncompressed_size;
				
				printf("ZIP: %s (%d)\n", name, size);
								
				buffer=memalign(128, size); // The allocation is alligned to aid the DMA transfers
			
				if(unzOpenCurrentFile(zipfile) == UNZ_OK){
					unzReadCurrentFile(zipfile, buffer, size);
					if(unzCloseCurrentFile(zipfile) == UNZ_CRCERROR)return 0;
					
					if(Texture->Mem!=NULL) {
						free(Texture->Mem);
						Texture->Mem = NULL;
					}
					
					Texture->Mem=buffer;
				}
				
				//unzClose(zipfile);
				
				return 1;
			}
		//unzClose(zipfile);
		
		return 0;
	}
	
	return 1;
}

void findTheme(){
	char tmp[255], tmp_zip[255], tmp_ZIP[255];
	int i, fd;
	
	for(i=0;i<3;i++){

		switch(i){
			case 0:
				snprintf(tmp, 255, "mc0:OPL/%s/", theme);
				snprintf(tmp_zip, 255, "mc0:OPL/%s.zip", theme);
				snprintf(tmp_ZIP, 255, "mc0:OPL/%s.ZIP", theme);
			break;
			case 1:
				snprintf(tmp, 255, "mc1:OPL/%s/", theme);
				snprintf(tmp_zip, 255, "mc1:OPL/%s.zip", theme);
				snprintf(tmp_ZIP, 255, "mc1:OPL/%s.ZIP", theme);
			break;
			case 2:
				snprintf(tmp, 255, "%sOPL/%s/", USB_prefix, theme);
				snprintf(tmp_zip, 255, "%sOPL/%s.zip", USB_prefix, theme);
				snprintf(tmp_ZIP, 255, "%sOPL/%s.ZIP", USB_prefix, theme);
			break;
		}
		
		
		if((fd=fioDopen(tmp))>=0){
			fioDclose(fd);
			if(i<2){
				snprintf(theme_prefix, 32, "mc%d:OPL/",i);
			}else{
				snprintf(theme_prefix, 32, "%sOPL/",USB_prefix);
			}
			theme_zipped=0;
			break;
		}else if((fd=fioOpen(tmp_zip, O_RDONLY))>=0){
			fioClose(fd);
			if(i<2){
				snprintf(theme_prefix, 32, "mc%d:OPL/",i);
			}else{
				snprintf(theme_prefix, 32, "%sOPL/",USB_prefix);
			}
			theme_zipped=1;
			break;
		}else if((fd=fioOpen(tmp_ZIP, O_RDONLY))>=0){
			fioClose(fd);
			if(i<2){
				snprintf(theme_prefix, 32, "mc%d:OPL/",i);
			}else{
				snprintf(theme_prefix, 32, "%sOPL/",USB_prefix);
			}
			theme_zipped=1;
			break;
		}
	
	}
	
}

void loadTheme(int themeid) {

	// themeid == 0 means the default theme
	strcpy(theme,theme_dir[themeid]);

	findTheme();

	loadIcons();
	loadFont(0);
	background_image=loadBackground(); //Load the background last because it closes the zip file there

	struct TConfigSet themeConfig;
	themeConfig.head = NULL;
	themeConfig.tail = NULL;

	// try to load the config from the theme dir
	char tmp[255];
	snprintf(tmp, 255, "mass:OPL/%s/theme.cfg", theme);

	// set default colors
	bg_color[0]=default_bg_color[0];
	bg_color[1]=default_bg_color[1];
	bg_color[2]=default_bg_color[2];

	text_color[0]=default_text_color[0];
	text_color[1]=default_text_color[1];
	text_color[2]=default_text_color[2];

	if (readConfig(&themeConfig, tmp)) {
		// load the color value overrides if present
		getConfigColor(&themeConfig, "bgcolor", bg_color);
		getConfigColor(&themeConfig, "textcolor", text_color);
	}

	setColor(bg_color[0],bg_color[1],bg_color[2]);
	textColor(text_color[0], text_color[1], text_color[2], 0xff);	
}

int loadBackground() {
	char tmp[255];
	
	background.Width = 640;
	background.Height = 480;
	background.PSM = GS_PSM_CT24;
	background.Filter = GS_FILTER_LINEAR;

	snprintf(tmp, 255, "background.raw");
	
	int res = loadRAW(tmp, &background);
	
	// see if we can load the second one too
	background2.Width = 640;
	background2.Height = 480;
	background2.PSM = GS_PSM_CT24;
	background2.Filter = GS_FILTER_LINEAR;
	snprintf(tmp, 255, "background2.raw");
	
	// the second image is not mandatory
	loadRAW(tmp, &background2);
	
	if(zipfile!=NULL){
		unzClose(zipfile);
		zipfile=NULL;
	}
	
	return res;
}

void LoadIcon(GSTEXTURE* txt, char* iconname, void* defraw, size_t w, size_t h) {
	//char tmp[255];

	txt->Width = w;
	txt->Height = h;
	txt->PSM = GS_PSM_CT32;
	txt->Filter = GS_FILTER_LINEAR;
	
	//snprintf(tmp, 254, "mass:OPL/%s/%s",theme, iconname);
	
	if (txt->Mem == defraw) // if we have a default, clear it now so it doesn't get
		txt->Mem = NULL; // freed by accident
	
	if(loadRAW(iconname, txt)==0)
		txt->Mem=(u32*)defraw; // if load could not be done, set the default
}

/// Loads icons to ram
void loadIcons() {
	LoadIcon(&exit_icon, "exit.raw", &exit_raw, 64, 64);
	LoadIcon(&config_icon, "config.raw", &config_raw, 64, 64);
	LoadIcon(&games_icon, "games.raw", &games_raw, 64, 64);
	LoadIcon(&disc_icon, "disc.raw", &disc_raw, 64, 64);
	LoadIcon(&theme_icon, "theme.raw", &theme_raw, 64, 64);
	LoadIcon(&language_icon, "language.raw", &language_raw, 64, 64);
	LoadIcon(&apps_icon, "apps.raw", &apps_raw, 64, 64);
	LoadIcon(&menu_icon, "menu.raw", &menu_raw, 64, 64);
	LoadIcon(&scroll_icon, "scroll.raw", &scroll_raw, 64, 64);
	LoadIcon(&usb_icon, "usb.raw", &usb_raw, 64, 64);
	LoadIcon(&save_icon, "save.raw", &save_raw, 64, 64);
	LoadIcon(&netconfig_icon, "netconfig.raw", &netconfig_raw, 64, 64);
	LoadIcon(&network_icon, "network.raw", &network_raw, 64, 64);

	// button icons (small ones)
	LoadIcon(&cross_icon, "cross.raw", &cross_raw, 22, 22);
	LoadIcon(&triangle_icon, "triangle.raw", &triangle_raw, 22, 22);
	LoadIcon(&square_icon, "square.raw", &square_raw, 22, 22);
	LoadIcon(&circle_icon, "circle.raw", &circle_raw, 22, 22);
	LoadIcon(&select_icon, "select.raw", &select_raw, 22, 22);
	LoadIcon(&start_icon, "start.raw", &start_raw, 22, 22);
	LoadIcon(&up_dn_icon, "up_dn.raw", &up_dn_raw, 22, 22);
	LoadIcon(&lt_rt_icon, "lt_rt.raw", &lt_rt_raw, 22, 22);
}

/// Uploads the icons to vram
void updateIcons() {
	uploadTexture(&exit_icon);
	uploadTexture(&config_icon);
	uploadTexture(&games_icon);
	uploadTexture(&disc_icon);
	uploadTexture(&theme_icon);
	uploadTexture(&language_icon);
	uploadTexture(&apps_icon);
	uploadTexture(&menu_icon);
	uploadTexture(&scroll_icon);
	uploadTexture(&usb_icon);
	uploadTexture(&save_icon);
	uploadTexture(&network_icon);
	uploadTexture(&netconfig_icon);

	// Buttons
	uploadTexture(&cross_icon);
	uploadTexture(&triangle_icon);
	uploadTexture(&square_icon);
	uploadTexture(&circle_icon);
	uploadTexture(&select_icon);
	uploadTexture(&start_icon);
	uploadTexture(&up_dn_icon);
	uploadTexture(&lt_rt_icon);
	
}

void drawIcon(GSTEXTURE *img, int x, int y, float scale) {
	if (img!=NULL) {
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		DrawSprite_texture(gsGlobal, img, x-scale,  // X1
			y-scale,  // Y2
			0.0f,  // U1
			0.0f,  // V1
			x+60.0f+scale, // X2
			y+60.0f+scale, // Y2
			img->Width, // U2
			img->Height, // V2
			gZ,
			GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		gZ++;
	}
}

void DrawIconN(GSTEXTURE *img, int x, int y, float scale) {
	if (img!=NULL) {
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		DrawSprite_texture(gsGlobal, img, x,  // X1
			y,  // Y2
			0.0f,  // U1
			0.0f,  // V1
			x+img->Width * scale, // X2
			y+img->Height * scale, // Y2
			img->Width, // U2
			img->Height, // V2
			gZ,
			GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		gZ++;
	}
}


void drawIcons() {
	// Menu item animation
	if(h_anim<100 && direc==1) {
		h_anim+=10;
	} else if(h_anim>100 && direc==3) {
		h_anim-=10;
	}
	
	if ((h_anim == 100) && (selected_item)) {
		assert(selected_item->item);
		
		drawSubMenu();
	}
	
	textColor(text_color[0],text_color[1],text_color[2],0xff);
	
	// we start at prev, if available
	struct menu_list_t *cur_item = selected_item;
	int xpos = 50;
	
	if (selected_item->prev) {
		xpos = -50;
		cur_item = selected_item->prev;
	}
	
	while (cur_item) {
		drawIcon(cur_item->item->icon, h_anim + xpos, yfix2(120), cur_item == selected_item ? 20:1);
		
		if(cur_item == selected_item && h_anim==100)
			drawText(h_anim + xpos + 23, yfix2(205), GetMenuItemText(cur_item->item), 1.0f, 1);
		
		cur_item = cur_item->next;
		xpos += 100;
	}
}

void drawInfo() {
	char txt[16];

	textColor(text_color[0],text_color[1],text_color[2],0xff);
	
	strncpy(txt,&infotxt[frame/10],15);
	drawText(300,yfix2(53),txt,1,0);
	
	if (frame>strlen(infotxt)*10) {
	 frame=0;
	} else {
	 frame++;	
	}
}

void drawHint(const char* hint, int key) {
	size_t w = strlen(hint) * gsFont->CharWidth + 45;

	// if key is drawn or not:
	GSTEXTURE *icon = NULL;

	switch (key) {
		case KEY_CROSS:
			icon = &cross_icon;
			break;
		case KEY_SQUARE:
			icon = &square_icon;
			break;
		case KEY_TRIANGLE:
			icon = &triangle_icon;
			break;
		case KEY_CIRCLE:
			icon = &circle_icon;
			break;
		case KEY_SELECT:
			icon = &select_icon;
			break;
		case KEY_START:
			icon = &start_icon;
			break;
		default:
			icon = NULL; 
	}

	if (icon) {
		w += 25;
	}

	int x = 625 - w;
	
	// darkening quad...
	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);

	drawQuad(gsGlobal, x    , yfix2(360.0f), 
		           x + w, yfix2(360.0f), 
			   x    , yfix2(400.0f), 
			   x + w, yfix2(400.0f), gZ, gColDarker);
			   
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
	textColor(0xff,0xff,0xff,0xff);

	if (icon) {
		DrawIconN(icon, x+ 10, yfix2(370), 1.0f);
		drawText(x + 40, yfix2(375), hint, 1, 0);
	} else {
		drawText(x + 15, yfix2(375), hint, 1, 0);
	}
}

void drawSubMenu() {
	if (!selected_item)
		return;

	if(v_anim<100 && direc==4){
		v_anim+=10;
	}else if(v_anim>100 && direc==2){
		v_anim-=10;
	}
	
	textColor(text_color[0],text_color[1],text_color[2],0xff);

	struct submenu_list_t *cur = selected_item->item->current;
	
	if (!cur) // no rendering if empty
		return;
	
	// prev item
	struct submenu_list_t *prev = cur->prev;
	
	
	int others = 0;
	
	while (prev && (others < _vbefore)) {
		drawIcon(prev->item.icon, (100)+50, yfix2(((v_anim - 60) - others * _vspacing)),1);
		// Display the prev. item's text:
		/* if ((v_anim>=80) || (others > 1))
			DrawText((100)+150, (v_anim - 40) - others * _vspacing, prev->item.text,1.0f,0);
		
		*/
		prev = prev->prev; others++;
	}
	
	drawIcon(cur->item.icon, (100)+50,yfix2((v_anim+130)),10);
	 if(v_anim==100)
	drawText((100)+150,yfix2((v_anim+153)),GetSubItemText(&cur->item),1.0f,0);

	cur = cur->next;
	
	others = 0;
	
	while (cur && (others <= _vafter)) {
		drawIcon(cur->item.icon, (100)+50, yfix2((v_anim + 210 + others * _vspacing)),1);
		//DrawText((100)+150, v_anim + 235 + others * _vspacing,cur->item.text,1.0f,0);
		cur = cur->next;
		others++;
	}
}


int showKeyb(char* text, size_t maxLen) {
	int i, len=strlen(text), selkeyb=1;
	int selchar=0, selcommand=-1;
	char c[2]="\0\0";
	char *keyb;
	
	char keyb1[40]={'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
				   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
				   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '\'',
				   'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '?'};
				   
	char keyb2[40]={'!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
				   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
				   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '"',
				   'Z', 'X', 'C', 'V', 'B', 'N', 'M', '-', '_', '/'};
				   
	char keyb3[40]={'à', 'á', 'â', 'ã', 'ä', 'å', 'æ', 'ç', '[', ']',
				   'è', 'é', 'ê', 'ë', 'ì', 'í', 'î', 'ï', ';', ':',
				   'ñ', 'ò', 'ó', 'ô', 'õ', 'ö', 'ø', 'œ', '`', '¡',
				   'ß', 'ù', 'ú', 'û', 'ü', 'ý', 'ÿ', ',', '.', '¿'};
				   
	char keyb4[40]={'À', 'Á', 'Â', 'Ã', 'Ä', 'Å', 'Æ', 'Ç', '<', '>',
				   'È', 'É', 'Ê', 'Ë', 'Ì', 'Í', 'Î', 'Ï', '=', '+',
				   'Ñ', 'Ò', 'Ó', 'Ô', 'Õ', 'Ö', 'Ø', 'Œ', '~', '"',
				   'ß', 'Ù', 'Ú', 'Û', 'Ü', 'Ý', 'ÿ', '-', '_', '/'};
	
	char *commands[4]={"BACKSPACE", "SPACE", "ENTER", "MODE"};

	GSTEXTURE *cmdicons[4] = {
		&square_icon,
		&triangle_icon,
		&start_icon,
		&select_icon
	};

	keyb=keyb1;
	
	while(1){
		readPads();
				
		drawBackground();
		
		textColor(0xff,0xff,0xff,0xff);
		
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		drawQuad(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, gZ, gColDarker);
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		
		//Text
		drawText(50, yfix2(120), text, 1.0f, 0);
		
		// separating line for simpler orientation
		int ypos = yfix2(135);
		drawLine(gsGlobal, 25, ypos, 600, ypos, 1, gColWhite);
		drawLine(gsGlobal, 25, ypos + 1, 600, ypos + 1, 1, gColWhite);

		
		// Numbers
		for(i=0;i<=9;i++){
			if(i==selchar){
				textColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				textColor(0xff,0xff,0xff,0xff);
			}
			c[0]=keyb[i];
			drawText(50+(i*32), yfix2(170), c, 1.0f, 0);
		}
		
		// QWERTYUIOP
		for(i=0;i<=9;i++){
			if(i+10==selchar){
				textColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				textColor(0xff,0xff,0xff,0xff);
			}
			c[0]=keyb[i+10];
			drawText(50+(i*32), yfix2(200), c, 1.0f, 0);
		}
		
		// ASDFGHJKL'
		for(i=0;i<=9;i++){
			if(i+20==selchar){
				textColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				textColor(0xff,0xff,0xff,0xff);
			}
			c[0]=keyb[i+20];
			drawText(50+(i*32), yfix2(230), c, 1.0f, 0);
		}
		
		// ZXCVBNM,.?
		for(i=0;i<=9;i++){
			if(i+30==selchar){
				textColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				textColor(0xff,0xff,0xff,0xff);
			}
			c[0]=keyb[i+30];
			drawText(50+(i*32), yfix2(260), c, 1.0f, 0);
		}
		
		// Commands
		for(i=0;i<=3;i++){
			if(i==selcommand){
				textColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				textColor(0xff,0xff,0xff,0xff);
			}

			DrawIconN(cmdicons[i], 384, yfix2((165+(30*i))), 1.0f);
			drawText(414, yfix2((170+(30*i))), commands[i], 1.0f, 0);
		}
		
		flip();
		
		if(getKey(KEY_LEFT)){
			if(selchar>-1 && selchar>0 && (selchar!=10 && selchar!=20 && selchar!=30)){
				selchar--;
			}else{
				if(selchar>-1){
					selcommand=selchar/10;
					selchar=-1;
				}else{
					selchar=selcommand*10+9;
					selcommand=-1;
				}
			}
		}else if(getKey(KEY_RIGHT)){
			if(selchar>-1 && selchar<39 && (selchar!=9 && selchar!=19 && selchar!=29)){
				selchar++;
			}else{
				if(selchar>-1){
					selcommand=selchar/10;
					selchar=-1;
				}else{
					selchar=selcommand*10;
					selcommand=-1;
					
				}
			}
		}else if(getKey(KEY_UP)){
			if(selchar>-1){
				if(selchar>9){
					selchar-=10;
				}else{
					selchar+=30;
				}
			}else{
				if(selcommand>0){
					selcommand--;
				}else{
					selcommand=3;
				}
			}
		}else if(getKey(KEY_DOWN)){
			if(selchar>-1){
				if(selchar<30){
					selchar+=10;
				}else{
					selchar-=30;
				}
			}else{
				if(selcommand<3){
					selcommand++;
				}else{
					selcommand=0;
				}
			}
		}else if(getKeyOn(KEY_CROSS)){
			if(len<(maxLen-1) && selchar>-1){
				len++;
				c[0]=keyb[selchar];
				strcat(text,c);
			}else if(selcommand==0){
				if(len>0){ // BACKSPACE
					len--;
					text[len]=0;
				}		
			}else if(selcommand==1){
				if(len<(maxLen-1)){ // SPACE
					len++;
					c[0]=' ';
					strcat(text,c);
				}
			}else if(selcommand==2){
				return 1; //ENTER
			}else if(selcommand==3){
				if(selkeyb<4){ // MODE
					selkeyb++;
				}else{
					selkeyb=1;
				}
				if(selkeyb==1)keyb=keyb1;
				if(selkeyb==2)keyb=keyb2;
				if(selkeyb==3)keyb=keyb3;
				if(selkeyb==4)keyb=keyb4;
			}
		}else if(getKey(KEY_SQUARE)){
			if(len>0){ // BACKSPACE
				len--;
				text[len]=0;
			}
		}else if(getKey(KEY_TRIANGLE)){
			if(len<(maxLen-1) && selchar>-1){ // SPACE
				len++;
				c[0]=' ';
				strcat(text,c);
			}
		}else if(getKeyOn(KEY_START)){
			return 1; //ENTER
		} else if (getKeyOn(KEY_SELECT)) {
			if(selkeyb<4){ // MODE
				selkeyb++;
			}else{
				selkeyb=1;
			}
			if(selkeyb==1)keyb=keyb1;
			if(selkeyb==2)keyb=keyb2;
			if(selkeyb==3)keyb=keyb3;
			if(selkeyb==4)keyb=keyb4;
		}
		
		if(getKey(KEY_CIRCLE))break;
	}
	
	return 0;
}

int showColSel(unsigned char *r, unsigned char *g, unsigned char *b) {
	int selc = 0;
	unsigned char col[3];

	col[0] = *r; col[1] = *g; col[2] = *b;
	setButtonDelay(KEY_LEFT, 1);
	setButtonDelay(KEY_RIGHT, 1);


	while(1) {
		readPads();
				
		drawBackground();
		
		textColor(0xff,0xff,0xff,0x080);
		
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		drawQuad(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, gZ, gColDarker);

		// "Color selection"
		drawText(50, yfix2(50), "Colour selection", 1.0f, 0);

		// 3 bars representing the colors...
		size_t co;
		for (co = 0; co < 3; ++co) {
			unsigned char cc[3] = {0,0,0};
			cc[co] = col[co];

			u64 dcol = GS_SETREG_RGBAQ(cc[0],cc[1],cc[2],0x00,0x00);
			float y = yfix2(75.0f + co * 25);

			drawQuad(gsGlobal, 75.0f, y, 275.0f, y, 75.0f, y + 20, 275.0f, y + 20, gZ, gColWhite);
			drawQuad(gsGlobal, 79.0f, y + 4, 79 + (190.0f*(cc[co]*100/255)/100), y + 4, 79.0f, y + 16, 79 + (190.0f*(cc[co]*100/255)/100), y + 16, gZ + 1, dcol);

			if (selc == co)
				// draw <> in front of the bar
				DrawIconN(&lt_rt_icon, 50, y, 1.0f);
				
		}

		// target color itself
		u64 dcol = GS_SETREG_RGBAQ(col[0],col[1],col[2],0x00,0x00);
		float x, y;
		x = 300;
		y = yfix2(75);
		drawQuad(gsGlobal, x, y, x+100, y, x, y+100, x+100, y+100, gZ, gColWhite);
		drawQuad(gsGlobal, x+5, y+5, x+95, y + 5, x + 5, y+95, x+95, y+95, gZ + 1, dcol);

		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		// separating line for simpler orientation
		flip();
		
		if(getKey(KEY_LEFT)) {
			if (col[selc] > 0)
				col[selc]--;
		}else if(getKey(KEY_RIGHT)) {
			if (col[selc] < 255)
				col[selc]++;
		}else if(getKey(KEY_UP)) {
			if (selc > 0)
				selc--;		
		}else if(getKey(KEY_DOWN)) {
			if (selc < 2)
				selc++;		
		}else if(getKeyOn(KEY_CROSS)) {
			*r = col[0];
			*g = col[1];
			*b = col[2];
			setButtonDelay(KEY_LEFT, 5);
			setButtonDelay(KEY_RIGHT, 5);
			return 1;
		}else if(getKeyOn(KEY_CIRCLE)) {
			setButtonDelay(KEY_LEFT, 5);
			setButtonDelay(KEY_RIGHT, 5);
			return 0;
		}
	}
	
	return 0;
}

void menuNextH() {
	if (!selected_item) {
		selected_item = menu;
	}
	
	if(selected_item->next){
		h_anim=200;
		selected_item = selected_item->next;
		
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
		
		v_anim=100;
		direc=3;
	}
}

void menuPrevH() {
	if (!selected_item) {
		selected_item = menu;
	}
	
	if(selected_item->prev){
		h_anim=0;
		selected_item = selected_item->prev;
		
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
		
		v_anim=100;
		direc=1;
	}
}

void menuNextV() {
	if (!selected_item)
		return;
	
	struct submenu_list_t *cur = selected_item->item->current;
	
	if(cur && cur->next) {
		selected_item->item->current = cur->next;
		
		v_anim=200;
		direc=2;
	}
}

void menuPrevV() {
	if (!selected_item)
		return;
	
	struct submenu_list_t *cur = selected_item->item->current;

	// if the current item is on the page start, move the page start one page up before moving the item
	if (selected_item->item->pagestart) {
		if (selected_item->item->pagestart == cur) {
			int itms = STATIC_PAGE_SIZE + 1; // +1 because the selection will move as well

			while (--itms && selected_item->item->pagestart->prev) {
				selected_item->item->pagestart = selected_item->item->pagestart->prev;	
			}
		}
	} else {
		selected_item->item->pagestart = cur;
	}
	
	if(cur && cur->prev) {
		selected_item->item->current = cur->prev;
			
		v_anim=0;
		direc=4;
	}
}

struct menu_item_t* menuGetCurrent() {
	if (!selected_item)
		return NULL;

	return selected_item->item;
}

void menuItemExecute() {
	if (selected_item && (selected_item->item->execute)) {
		// selected submenu id. default -1 = no selection
		int subid = -1;
		
		struct submenu_list_t *cur = selected_item->item->current;
		
		if (cur)
			subid = cur->item.id;
		
		selected_item->item->execute(selected_item->item, subid);
	}
}

void menuItemAltExecute() {
	if (selected_item && (selected_item->item->altExecute)) {
		// selected submenu id. default -1 = no selection
		int subid = -1;
		
		struct submenu_list_t *cur = selected_item->item->current;
		
		if (cur)
			subid = cur->item.id;
		
		selected_item->item->altExecute(selected_item->item, subid);
	}
}

void menuSetSelectedItem(struct menu_item_t* item) {
	struct menu_list_t* itm = menu;
	
	while (itm) {
		if (itm->item == item) {
			selected_item = itm;
			
			h_anim = 100;
			v_anim = 100;
			direc = 0;
			
			return;
		}
			
		
		itm = itm->next;
	}
}

void refreshSubMenu(short forceRefresh) {
	if (selected_item && (selected_item->item->refresh)) {
		selected_item->item->refresh(selected_item->item, forceRefresh);
	}
}

void drawScreenStatic() {
	// ------------------------------------------------------
	// -------- Preparations --------------------------------
	// ------------------------------------------------------
	// verify the item is in visible range
	int icnt = STATIC_PAGE_SIZE;
	int found = 0;

	struct submenu_list_t *cur = selected_item->item->current;
	struct submenu_list_t *ps  = selected_item->item->pagestart;

	while (icnt-- && ps) {
		if (ps == cur) {
			found = 1;
			break;
		}

		ps = ps->next;
	}

	// page not properly aligned?
	if (!found)
		selected_item->item->pagestart = cur;

	// reset to page start after cur. item visibility determination
	ps  = selected_item->item->pagestart;


	// we render a static variant of the menu (no animations)
	// ------------------------------------------------------
	// -------- 0. the background ---------------------------
	// ------------------------------------------------------
	drawBackground();
	
	// ------------------------------------------------------
	// -------- 1. the icons that symbolise the hmenu -------
	// ------------------------------------------------------
	textColor(text_color[0],text_color[1],text_color[2], 0xff);
	
	// we start at 10, 10 and move by appropriate size right
	
	// we start at prev, if available
	struct menu_list_t *cur_item = menu;
	
	int xpos = 30;
	
	while (cur_item) {
		if(cur_item == selected_item) {
			drawIcon(cur_item->item->icon, xpos + 20, yfix2(20), 20);
 			drawText(30, yfix2(100), GetMenuItemText(cur_item->item), 1.0f, 0);
 			xpos += 105;
		} else {
			drawIcon(cur_item->item->icon, xpos, yfix2(20), 1);
 			xpos += 65;
		}

		cur_item = cur_item->next;
	}	
	
	// ------------------------------------------------------
	// -------- 2. the submenu ------------------------------
	// ------------------------------------------------------

	// we want to display N previous, the current and N next items (N == sur_items)
	// spacing is 30 pixels
	// x offset is 10 pixels
	// y offset is 125 pixels
	// the selected item is displayed at 125 + 5 * 30 = 275 px.
	if (!selected_item) 
		return;
	
	float iscale = -15.0f; // not a scale... an additional spacing in pixels
	int draw_icons = 1;
	int iconhalf = 25;
	int spacing = 30;
	
	int icon_h_spacing = 0;
	
	if (draw_icons)
		icon_h_spacing = 45;
	
	// count of items before and after selection
	// int sur_items = 5;
	int curpos = 125; // + sur_items * spacing;
	
	textColor(text_color[0],text_color[1],text_color[2],0xff);
	
		
	if (!cur) // no rendering if empty
		return;
	
	int others = 0;
	
	while (ps && (others <= STATIC_PAGE_SIZE)) {
		if (draw_icons)
			drawIcon(ps->item.icon, 10, yfix2((curpos + others * spacing - iconhalf)), iscale);
		
		if (ps == cur)
			textColor(0xff, 0x080, 0x00, 0xff);
		else
			textColor(text_color[0],text_color[1],text_color[2],0xff);

		drawText(10 + icon_h_spacing, yfix2((curpos + others * spacing)), GetSubItemText(&ps->item), 1.0f, 0);
		
		ps = ps->next; others++;
	}
}

void swapMenu() {
	setMenuDynamic(!dynamic_menu);
}

void setMenuDynamic(int dynamic) {
	dynamic_menu = dynamic;
	
	// reset the animation counters
	h_anim = 100;
	v_anim = 100;
	direc = 0;
	
	// Fixup for NULL poiters in the cur. selections
	if (!selected_item) 
		selected_item = menu;
	
	if (selected_item) {
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
	}
}


void drawScreen() {
	// dynamic or static render?
	if (!dynamic_menu) {
		drawScreenStatic();
	} else {
		// default render:
		drawBackground();
		drawIcons();
		drawInfo();
	}
}
