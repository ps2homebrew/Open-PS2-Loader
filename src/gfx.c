/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/


#include "include/usbld.h"
#include "include/lang.h"
#include <assert.h>

// Row height in dialogues
#define UI_ROW_HEIGHT 10
// UI spacing of the dialogues (pixels between consecutive items)
#define UI_SPACING_H 10
#define UI_SPACING_V 2
// spacer ui element width (simulates tab)
#define UI_SPACER_WIDTH 50
// minimal pixel width of spacer
#define UI_SPACER_MINIMAL 30
// length of breaking line in pixels
#define UI_BREAK_LEN 600

#define yfix(x) (x*gsGlobal->Height)/480
#define yfix2(x) x+(gsGlobal->Height==512 ? x*0.157f : 0)

					
extern void *font_raw;
extern int size_font_raw;

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

GSGLOBAL *gsGlobal;
GSTEXTURE font;
GSFONT *gsFont;

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


u64 White = GS_SETREG_RGBAQ(0xFF,0xFF,0xFF,0x00,0x00);
u64 Black = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00);
u64 Darker = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x60,0x00);
u64 color_text = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);

u64 Wave_a = GS_SETREG_RGBAQ(0x00,0x00,0x80,0x20,0x00);
u64 Wave_b = GS_SETREG_RGBAQ(0x00,0x00,0x40,0x20,0x00);
u64 Wave_c = GS_SETREG_RGBAQ(0x00,0x00,0xff,0x20,0x00);
u64 Wave_d = GS_SETREG_RGBAQ(0x00,0x00,0x20,0x20,0x00);

u64 Background_a = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00);
u64 Background_b = GS_SETREG_RGBAQ(0x00,0x00,0x20,0x00,0x00);
u64 Background_c = GS_SETREG_RGBAQ(0x00,0x00,0x20,0x00,0x00);
u64 Background_d = GS_SETREG_RGBAQ(0x00,0x00,0x80,0x00,0x00);

char *infotxt;
int z;

// Count of icons before and after the selected one
unsigned int _vbefore = 1;
unsigned int _vafter = 1;
// spacing between icons, vertical.
unsigned int _vspacing = 80;

// forward decls.
void MsgBox();
void DrawConfig();
void LoadResources();
void DestroyMenu();

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Initialization/Destruction of gfx ----------------
// -------------------------------------------------------------------------------------------
void InitGFX() {
	waveframe=0;
	frame=0;
	h_anim=100;
	v_anim=100;
	direc=0;
	max_settings=2;
	font.Vram = 0;
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
}

void DestroyGFX() {
	/// TODO: un-initialize the GFX
	
	DestroyMenu();
}

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
void InitMenu() {
	LoadResources();

	menu = NULL;
	selected_item = NULL;
}

void DestroyMenu() {
	// destroy menu
	struct TMenuList *cur = menu;
	
	while (cur) {
		struct TMenuList *td = cur;
		cur = cur->next;
		
		if (&td->item)
			DestroySubMenu(&td->item->submenu);
		
		free(td);
	}
}

struct TMenuList* AllocMenuItem(struct TMenuItem* item) {
	struct TMenuList* it;
	
	it = malloc(sizeof(struct TMenuList));
	
	it->prev = NULL;
	it->next = NULL;
	it->item = item;
	
	return it;
}

void AppendMenuItem(struct TMenuItem* item) {
	assert(item);
	
	if (menu == NULL) {
		menu = AllocMenuItem(item);
		return;
	}
	
	struct TMenuList *cur = menu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	struct TMenuList *newitem = AllocMenuItem(item);
	
	// link
	cur->next = newitem;
	newitem->prev = cur;
}


struct TSubMenuList* AllocSubMenuItem(GSTEXTURE *icon, char *text, int id, int text_id) {
	struct TSubMenuList* it;
	
	it = malloc(sizeof(struct TSubMenuList));
	
	it->prev = NULL;
	it->next = NULL;
	it->item.icon = icon;
	it->item.text = text;
	it->item.text_id = text_id;
	it->item.id = id;
	
	return it;
}

struct TSubMenuList* AppendSubMenu(struct TSubMenuList** submenu, GSTEXTURE *icon, char *text, int id, int text_id) {
	if (*submenu == NULL) {
		*submenu = AllocSubMenuItem(icon, text, id, text_id);
		return *submenu; 
	}
	
	struct TSubMenuList *cur = *submenu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	struct TSubMenuList *newitem = AllocSubMenuItem(icon, text, id, text_id);
	
	// link
	cur->next = newitem;
	newitem->prev = cur;
	
	return newitem;
}

void DestroySubMenu(struct TSubMenuList** submenu) {
	// destroy sub menu
	struct TSubMenuList *cur = *submenu;
	
	while (cur) {
		struct TSubMenuList *td = cur;
		cur = cur->next;
		free(td);
	}
	
	*submenu = NULL;
}

char *GetMenuItemText(struct TMenuItem* it) {
	if (it->text_id >= 0)
		return _l(it->text_id);
	else
		return it->text;
}

char *GetSubItemText(struct TSubMenuItem* it) {
	if (it->text_id >= 0)
		return _l(it->text_id);
	else
		return it->text;
}

void UpdateScrollSpeed() {
	// sanitize the settings
	if ((scroll_speed < 0) || (scroll_speed > 2))
		scroll_speed = 1;
	
	// update the pad delays for KEY_UP and KEY_DOWN
	// default delay is 7
	SetButtonDelay(KEY_UP, 10 - scroll_speed * 3); // 0,1,2 -> 10, 7, 4
	SetButtonDelay(KEY_DOWN, 10 - scroll_speed * 3);
}

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Rendering ----------------------------------------
// -------------------------------------------------------------------------------------------
void DrawQuad(GSGLOBAL *gsGlobal, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int z, u64 color){
	gsKit_prim_quad(gsGlobal,x1,yfix(y1),x2,yfix(y2),x3,yfix(y3),x4,yfix(y4),z,color);
}

void DrawQuad_gouraud(GSGLOBAL *gsGlobal, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int z, u64 color1, u64 color2, u64 color3, u64 color4){
	gsKit_prim_quad_gouraud(gsGlobal,x1,yfix(y1),x2,yfix(y2),x3,yfix(y3),x4,yfix(y4),z,color1,color2,color3,color4);
}

void DrawLine(GSGLOBAL *gsGlobal,float x1, float y1,float x2, float y2, int z, u64 color){
	gsKit_prim_line(gsGlobal, x1, yfix(y1), x2, yfix(y2), z, color);						
}

void DrawSprite_texture(GSGLOBAL *gsGlobal, const GSTEXTURE *Texture, float x1, float y1, float u1, float v1, float x2, int y2, float u2, float v2, int z, u64 color){
	gsKit_prim_sprite_texture(gsGlobal, Texture, x1, yfix(y1), u1, v1, x2, yfix(y2), u2, v2, z, color);
}

void Flip(){
	gsKit_queue_exec(gsGlobal);
	gsKit_sync_flip(gsGlobal);
	gsKit_clear(gsGlobal, Black);
}

void Intro(){
	char introtxt[255];
	int bg_loaded=0;
	
	for(frame=0;frame<125;frame++){
		if(frame<=25){
			TextColor(0x80,0x80,0x80,frame*4);
		}else if(frame>75 && frame<100){
			TextColor(0x80,0x80,0x80,0x80-(frame-75)*4);
		}else if(frame>100){
			TextColor(0x80,0x80,0x80,0x00);
			waveframe+=0.1f;
		}

		DrawBackground();
		snprintf(introtxt, 255, _l(_STR_OUL_VER), USBLD_VERSION);
		DrawText(270, 255, introtxt, 1.0f, 0);
		Flip();
		if (frame==75)bg_loaded=LoadBackground();
	}
	background_image=bg_loaded;
	LoadFont(0);
}

void DrawWave(int y, int xoffset){
	int w=0;
	float h=0;
	for(w=32;w<=640;w=w+32){
		z++;
		DrawQuad_gouraud(gsGlobal, w-32, y+50+sinf(waveframe+h+xoffset)*25.0f, w, y+50+sinf(waveframe+xoffset+h+0.1f)*25.0f, w-32, y-50+sinf(waveframe+h+xoffset)*-25.0f, w, y-50+sinf(waveframe+xoffset+h+0.1f)*-25.0f, z, Wave_b, Wave_b, Wave_a, Wave_a);
		DrawQuad_gouraud(gsGlobal, w-32, y-30+sinf(waveframe+h+xoffset)*-25.0f, w, y-30+sinf(waveframe+xoffset+h+0.1f)*-25.0f, w-32, y-50+sinf(waveframe+h+xoffset)*-25.0f, w, y-50+sinf(waveframe+xoffset+h+0.1f)*-25.0f, z, Wave_a, Wave_a, Wave_c, Wave_c);
		DrawQuad_gouraud(gsGlobal, w-32, y+50+sinf(waveframe+h+xoffset)*25.0f, w, y+50+sinf(waveframe+xoffset+h+0.1f)*25.0f, w-32, y+30+sinf(waveframe+h+xoffset)*25.0f, w, y+30+sinf(waveframe+xoffset+h+0.1f)*25.0f, z, Wave_d, Wave_d, Wave_b, Wave_b);
		h+=0.1f;
	}
}

void gfxRestoreConfig() {
	char *temp;
	
	// reinit all the values we are interested in from the config
	if (getConfigInt(&gConfig, "scrolling", &scroll_speed))
		UpdateScrollSpeed();
	
	getConfigInt(&gConfig, "menutype", &dynamic_menu);
	if (getConfigStr(&gConfig, "theme", &temp))
		strncpy(theme, temp, 32);
	if (getConfigColor(&gConfig, "bgcolor", bg_color))
		SetColor(bg_color[0],bg_color[1],bg_color[2]);
				
	if (getConfigColor(&gConfig, "textcolor", text_color))
		TextColor(text_color[0], text_color[1], text_color[2], 0xff);
	
	if ((dynamic_menu < 0) || (dynamic_menu > 1)) {
		dynamic_menu = 1;
		setConfigInt(&gConfig, "menutype", dynamic_menu);
	}
	
	if (getConfigInt(&gConfig, "language", &gLanguageID))
		setLanguage(gLanguageID);
}

void gfxStoreConfig() {
	// set the config values to be sure they are up-to-date
	setConfigInt(&gConfig, "scrolling", scroll_speed);
	setConfigInt(&gConfig, "menutype", dynamic_menu);
	setConfigStr(&gConfig, "theme", theme);
	setConfigColor(&gConfig, "bgcolor", bg_color);
	setConfigColor(&gConfig, "textcolor", text_color);
	setConfigInt(&gConfig, "language", gLanguageID);
}

void LoadResources() {
	readIPConfig();
	LoadIcons();
	UpdateIcons();
	LoadFont(1);
	UpdateFont();
}

void DrawBackground(){
	z=1;

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
		UploadTexture(cback);
	
		DrawSprite_texture(gsGlobal, cback, 0.0f,  // X1
			0.0f,  // Y2
			0.0f,  // U1
			0.0f,  // V1
			640.0f, // X2
			yfix(480), // Y2
			background.Width, // U2
			background.Height, // V2
			z,
			GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
		
		gsKit_queue_exec(gsGlobal);
		gsKit_vram_clear(gsGlobal);
	}else{
		DrawQuad_gouraud(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, 2, Background_a, Background_b, Background_c, Background_d);
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		DrawWave(240,0);
		DrawWave(260,2);
		DrawWave(256,4);
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		if(waveframe>6.28f){
			waveframe=0;
		}else{
			waveframe+=0.01f;
		}
	}
	UpdateIcons();
	UpdateFont();
}

void SetColor(int r, int g, int b){
	Wave_a = GS_SETREG_RGBAQ(r/2,g/2,b/2,0x20,0x00);
	Wave_b = GS_SETREG_RGBAQ(r/4,g/4,b/4,0x20,0x00);
	Wave_c = GS_SETREG_RGBAQ(r,g,b,0x20,0x00);
	Wave_d = GS_SETREG_RGBAQ(r/8,g/8,b/8,0x20,0x00);

	Background_a = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00);
	Background_b = GS_SETREG_RGBAQ(r/8,g/8,b/8,0x00,0x00);
	Background_c = GS_SETREG_RGBAQ(r/8,g/8,b/8,0x00,0x00);
	Background_d = GS_SETREG_RGBAQ(r/2,g/2,b/2,0x00,0x00);
}

void TextColor(int r, int g, int b, int a){
	color_text = GS_SETREG_RGBAQ(r,g,b,a,0x00);
}

void DrawText(int x, int y, const char *texto, float scale, int centered){
	if(centered==1){
		x=x-((strlen(texto)-1)*gsFont->CharWidth/2);
	}

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
	gsKit_font_print_scaled(gsGlobal, gsFont, x, yfix(y), 1, scale, color_text, texto);
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
}

void DrawConfig(){
	char tmp[255];
	int v_pos=0;
	int new_bg_color[3];
	int new_text_color[3];
	int editing_bg_color=0;
	int editing_text_color=0;
	int editing_theme_dir=0;
	int selected_theme_dir=0;
	int themechanged = 0;
	
	if(theme[0]==0){
		strcpy(theme,"<none>");
	}
	
	new_bg_color[0]=bg_color[0];
	new_bg_color[1]=bg_color[1];
	new_bg_color[2]=bg_color[2];
	new_text_color[0]=text_color[0];
	new_text_color[1]=text_color[1];
	new_text_color[2]=text_color[2];

	while(1){
		ReadPad();
				
		DrawScreen();

		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		DrawQuad(gsGlobal, 100.0f, 100.0f, 540.0f, 100.0f, 100.0f, 412.0f, 540.0f, 412.0f, z, Darker);
		DrawQuad(gsGlobal, 110.0f, 110.0f, 530.0f, 110.0f, 110.0f, 402.0f, 530.0f, 402.0f, z, Wave_a);
		DrawQuad_gouraud(gsGlobal, 100.0f, 100.0f, 540.0f, 100.0f, 110.0f, 110.0f, 530.0f, 110.0f, z, Wave_a, Wave_a, Wave_d, Wave_d);
		DrawQuad_gouraud(gsGlobal, 100.0f, 100.0f, 110.0f, 110.0f, 100.0f, 412.0f, 110.0f, 402.0f, z, Wave_a, Wave_b, Wave_a, Wave_b);
		DrawQuad_gouraud(gsGlobal, 110.0f, 402.0f, 530.0f, 402.0f, 100.0f, 412.0f, 540.0f, 412.0f, z, Wave_b, Wave_b, Wave_d, Wave_d);
		DrawQuad_gouraud(gsGlobal, 530.0f, 110.0f, 540.0f, 100.0f, 530.0f, 402.0f, 540.0f, 412.0f, z, Wave_b, Wave_d, Wave_b, Wave_d);
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		
		DrawText(305,120,_l(_STR_THEME_CONFIG),1,1);
		
		if(editing_bg_color==0){
			snprintf(tmp, 255, "Background color: #%02X%02X%02X", new_bg_color[0],new_bg_color[1],new_bg_color[2]);
			if(v_pos==0){
				TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			}else{
				TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			}
			DrawText(120,200,tmp,0.8f,0);
		}else if(editing_bg_color==1){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			snprintf(tmp, 255, "Background color: #  %02X%02X",new_bg_color[1],new_bg_color[2]);
			DrawText(120,200,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			snprintf(tmp, 255, "                   %02X", new_bg_color[0]);
			DrawText(120,200,tmp,0.8f,0);
		}else if(editing_bg_color==2){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			snprintf(tmp, 255, "Background color: #%02X  %02X",new_bg_color[0],new_bg_color[2]);
			DrawText(120,200,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			snprintf(tmp, 255, "                     %02X", new_bg_color[1]);
			DrawText(120,200,tmp,0.8f,0);
		}else if(editing_bg_color==3){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			snprintf(tmp, 255, "Background color: #%02X%02X",new_bg_color[0],new_bg_color[1]);
			DrawText(120,200,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			snprintf(tmp, 255, "                       %02X", new_bg_color[2]);
			DrawText(120,200,tmp,0.8f,0);
		}
		if(editing_text_color==0){
			snprintf(tmp, 255, "Text color: #%02X%02X%02X", new_text_color[0],new_text_color[1],new_text_color[2]);
			if(v_pos==1){
				TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			}else{
				TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			}
			DrawText(120,240,tmp,0.8f,0);
		}else if(editing_text_color==1){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			snprintf(tmp, 255, "Text color: #  %02X%02X",new_text_color[1],new_text_color[2]);
			DrawText(120,240,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			snprintf(tmp, 255, "             %02X", new_text_color[0]);
			DrawText(120,240,tmp,0.8f,0);
		}else if(editing_text_color==2){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			snprintf(tmp, 255, "Text color: #%02X  %02X",new_text_color[0],new_text_color[2]);
			DrawText(120,240,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			snprintf(tmp, 255, "               %02X", new_text_color[1]);
			DrawText(120,240,tmp,0.8f,0);
		}else if(editing_text_color==3){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			snprintf(tmp, 255, "Text color: #%02X%02X",new_text_color[0],new_text_color[1]);
			DrawText(120,240,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			snprintf(tmp, 255, "                 %02X", new_text_color[2]);
			DrawText(120,240,tmp,0.8f,0);
		}
		if(editing_theme_dir==0){
			snprintf(tmp, 255, "Selected Theme: %s", theme);
			if(v_pos==2){
				TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			}else{
				TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			}
			DrawText(120,280,tmp,0.8f, 0);
		}else{
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			snprintf(tmp, 255, "Selected Theme:");
			DrawText(120,280,tmp,0.8f, 0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			snprintf(tmp, 255, "                %s", theme_dir[selected_theme_dir]);
			DrawText(120,280,tmp,0.8f, 0);
		}
		
		if(v_pos==3){
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
		}else{
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
		}
		DrawText(310,370,_l(_STR_OK),1, 1);
		
		if(editing_bg_color>0){
			if(GetKey(KEY_UP)){
				if(new_bg_color[editing_bg_color-1]<255){
					new_bg_color[editing_bg_color-1]++;
				}
			}else if(GetKey(KEY_DOWN)){
				if(new_bg_color[editing_bg_color-1]>0){
					new_bg_color[editing_bg_color-1]--;
				}
			}else if(GetKey(KEY_LEFT)){
				if(editing_bg_color>1){
					editing_bg_color--;
				}
			}else if(GetKey(KEY_RIGHT)){
				if(editing_bg_color<3){
					editing_bg_color++;
				}
			}else if(GetKeyOn(KEY_CIRCLE)){
				editing_bg_color=0;
				SetButtonDelay(KEY_UP, 5);
				SetButtonDelay(KEY_DOWN, 5);
				new_bg_color[0]=bg_color[0];
				new_bg_color[1]=bg_color[1];
				new_bg_color[2]=bg_color[2];
			}else if(GetKeyOn(KEY_CROSS)){
				editing_bg_color=0;
				SetButtonDelay(KEY_UP, 5);
				SetButtonDelay(KEY_DOWN, 5);
			}
		}else if(editing_text_color>0){
			if(GetKey(KEY_UP)){
				if(new_text_color[editing_text_color-1]<255){
					new_text_color[editing_text_color-1]++;
				}
			}else if(GetKey(KEY_DOWN)){
				if(new_text_color[editing_text_color-1]>0){
					new_text_color[editing_text_color-1]--;
				}
			}else if(GetKey(KEY_LEFT)){
				if(editing_text_color>1){
					editing_text_color--;
				}
			}else if(GetKey(KEY_RIGHT)){
				if(editing_text_color<3){
					editing_text_color++;
				}
			}else if(GetKeyOn(KEY_CIRCLE)){
				editing_text_color=0;
				SetButtonDelay(KEY_UP, 10);
				SetButtonDelay(KEY_DOWN, 10);
				new_text_color[0]=text_color[0];
				new_text_color[1]=text_color[1];
				new_text_color[2]=text_color[2];
			}else if(GetKeyOn(KEY_CROSS)){
				editing_text_color=0;
				SetButtonDelay(KEY_UP, 10);
				SetButtonDelay(KEY_DOWN, 10);
			}
		}else if(editing_theme_dir>0){
			if(GetKey(KEY_UP)){
				if(selected_theme_dir>0){
					selected_theme_dir--;
				}
			}else if(GetKey(KEY_DOWN)){
				if(selected_theme_dir<max_theme_dir-1){
					selected_theme_dir++;
				}
			}else if(GetKeyOn(KEY_CIRCLE)){
				editing_theme_dir=0;
			}else if(GetKeyOn(KEY_CROSS)){
				editing_theme_dir=0;
				themechanged = 1;
			}
		}else{
			if(GetKey(KEY_UP)){
				if(v_pos>0){
					v_pos--;
				}
			}else if(GetKey(KEY_DOWN)){
				if(v_pos<3){
					v_pos++;
				}
			}else if(GetKeyOn(KEY_CROSS)){
				if(v_pos==0){
					SetButtonDelay(KEY_UP, 1);
					SetButtonDelay(KEY_DOWN, 1);
					editing_bg_color=1;
				}else if(v_pos==1){
					SetButtonDelay(KEY_UP, 1);
					SetButtonDelay(KEY_DOWN, 1);
					editing_text_color=1;
				}else if(v_pos==2){
					editing_theme_dir=1;
					ListDir("mass:USBLD");
				}else if(v_pos==3) {
					// store the settings into the global variables
					bg_color[0]=new_bg_color[0];
					bg_color[1]=new_bg_color[1];
					bg_color[2]=new_bg_color[2];
					
					text_color[0]=new_text_color[0];
					text_color[1]=new_text_color[1];
					text_color[2]=new_text_color[2];
				
					// Load the new theme if it changed
					if (themechanged) {
						strcpy(theme,theme_dir[selected_theme_dir]);
						background_image=LoadBackground();
						LoadIcons();
						LoadFont();
					}
					break;
				}
			}
			if(GetKey(KEY_CIRCLE)){
				SetColor(bg_color[0],bg_color[1],bg_color[2]);
				TextColor(text_color[0], text_color[1], text_color[2], 0xff);
				break;
			}
		}
		
		SetColor(new_bg_color[0],new_bg_color[1],new_bg_color[2]);
		TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
		
		Flip();
	}
	
	// revert to old button delays
	UpdateScrollSpeed();
}

/// Uploads texture to vram
void UploadTexture(GSTEXTURE* txt) {
	txt->Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(txt->Width, txt->Height, txt->PSM), GSKIT_ALLOC_USERBUFFER);
	
	/* // VRAM ALLOCATION DEBUGGING
	if (txt->Vram == GSKIT_ALLOC_ERROR)
		printf("!!!Bad Vram allocation!!!");
	*/
	
	gsKit_texture_upload(gsGlobal, txt);
}

void MsgBox(char* text){
	while(1){
		ReadPad();
				
		DrawScreen();
		
		TextColor(0xff,0xff,0xff,0xff);
		
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		DrawQuad(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, z, Darker);
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		DrawLine(gsGlobal, 50.0f, 75.0f, 590.0f, 75.0f, 1, White);
		DrawLine(gsGlobal, 50.0f, 410.0f, 590.0f, 410.0f, 1, White);
		DrawText(310, 250, text, 1.0f, 1);
		DrawText(400, 417, _l(_STR_O_BACK), 1.0f, 0);
		
		Flip();
		if(GetKey(KEY_CIRCLE))break;
	}
}

void LoadFont(int load_default){
		
	char tmp[255];
	
	font.Width = 256;
	font.Height = 256;
	font.PSM = GS_PSM_CT32;
	snprintf(tmp, 255, "mass:USBLD/%s/%s",theme,"font.raw");

	if(load_default==1 || LoadRAW(tmp, &font)==0){
		font.Mem=(u32*)&font_raw;
	}

	gsFont = calloc(1,sizeof(GSFONT));
	gsFont->Texture = &font;
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

void UpdateFont() {
	UploadTexture(&font);
}

int LoadRAW(char *path, GSTEXTURE *Texture) {
	int fd;
	unsigned int size;
	
	void *buffer;

	if(Texture->Mem!=NULL) {
		free(Texture->Mem);
		Texture->Mem = NULL;
	}

	fd=fioOpen(path, O_RDONLY);
	
	if (fd < 0) // invalid file name, etc.
		return 0;

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
	return 1;
}

void LoadTheme(int themeid) {
	// themeid == 0 means the default theme
	strcpy(theme,theme_dir[themeid]);
	background_image=LoadBackground();
	LoadIcons();
	LoadFont(0);

	struct TConfigSet themeConfig;
	themeConfig.head = NULL;
	themeConfig.tail = NULL;

	// try to load the config from the theme dir
	char tmp[255];
	snprintf(tmp, 255, "mass:USBLD/%s/theme.cfg", theme);

	// set default colors
	bg_color[0]=0x00;
	bg_color[1]=0x00;
	bg_color[2]=0xff;

	text_color[0]=0xff;
	text_color[1]=0xff;
	text_color[2]=0xff;

	if (readConfig(&themeConfig, tmp)) {
		// load the color value overrides if present
		getConfigColor(&themeConfig, "bgcolor", bg_color);
		getConfigColor(&themeConfig, "textcolor", text_color);
	}

	SetColor(bg_color[0],bg_color[1],bg_color[2]);
	TextColor(text_color[0], text_color[1], text_color[2], 0xff);	
}

int LoadBackground() {
	char tmp[255];
	
	background.Width = 640;
	background.Height = 480;
	background.PSM = GS_PSM_CT24;
	background.Filter = GS_FILTER_LINEAR;

	snprintf(tmp, 255, "mass:USBLD/%s/%s", theme, "background.raw");
	
	int res = LoadRAW(tmp, &background);
	
	// see if we can load the second one too
	background2.Width = 640;
	background2.Height = 480;
	background2.PSM = GS_PSM_CT24;
	background2.Filter = GS_FILTER_LINEAR;
	snprintf(tmp, 255, "mass:USBLD/%s/%s", theme, "background2.raw");
	
	// the second image is not mandatory
	LoadRAW(tmp, &background2);
	
	return res;
}

void LoadIcon(GSTEXTURE* txt, char* iconname, void* defraw, size_t w, size_t h) {
	char tmp[255];

	txt->Width = w;
	txt->Height = h;
	txt->PSM = GS_PSM_CT32;
	txt->Filter = GS_FILTER_LINEAR;
	
	snprintf(tmp, 254, "mass:USBLD/%s/%s",theme, iconname);
	
	if (txt->Mem == defraw) // if we have a default, clear it now so it doesn't get
		txt->Mem = NULL; // freed by accident
	
	if(LoadRAW(tmp, txt)==0)
		txt->Mem=(u32*)defraw; // if load could not be done, set the default
}

/// Loads icons to ram
void LoadIcons() {
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
}

/// Uploads the icons to vram
void UpdateIcons() {
	UploadTexture(&exit_icon);
	UploadTexture(&config_icon);
	UploadTexture(&games_icon);
	UploadTexture(&disc_icon);
	UploadTexture(&theme_icon);
	UploadTexture(&language_icon);
	UploadTexture(&apps_icon);
	UploadTexture(&menu_icon);
	UploadTexture(&scroll_icon);
	UploadTexture(&usb_icon);
	UploadTexture(&save_icon);
	UploadTexture(&network_icon);
	UploadTexture(&netconfig_icon);

	// Buttons
	UploadTexture(&cross_icon);
	UploadTexture(&triangle_icon);
	UploadTexture(&square_icon);
	UploadTexture(&circle_icon);
}

void DrawIcon(GSTEXTURE *img, int x, int y, float scale) {
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
			z,
			GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		z++;
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
			z,
			GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		z++;
	}
}


void DrawIcons() {
	// Menu item animation
	if(h_anim<100 && direc==1) {
		h_anim+=10;
	} else if(h_anim>100 && direc==3) {
		h_anim-=10;
	}
	
	if ((h_anim == 100) && (selected_item)) {
		assert(selected_item->item);
		
		DrawSubMenu();
	}
	
	TextColor(text_color[0],text_color[1],text_color[2],0xff);
	
	// we start at prev, if available
	struct TMenuList *cur_item = selected_item;
	int xpos = 50;
	
	if (selected_item->prev) {
		xpos = -50;
		cur_item = selected_item->prev;
	}
	
	while (cur_item) {
		DrawIcon(cur_item->item->icon, h_anim + xpos, yfix2(120), cur_item == selected_item ? 20:1);
		
		if(cur_item == selected_item && h_anim==100)
			DrawText(h_anim + xpos + 23, yfix2(205), GetMenuItemText(cur_item->item), 1.0f, 1);
		
		cur_item = cur_item->next;
		xpos += 100;
	}
}

void DrawInfo() {
	char txt[16];

	TextColor(text_color[0],text_color[1],text_color[2],0xff);
	
	strncpy(txt,&infotxt[frame/10],15);
	DrawText(300,yfix2(53),txt,1,0);
	
	if (frame>strlen(infotxt)*10) {
	 frame=0;
	} else {
	 frame++;	
	}
}

void DrawHint(const char* hint, int key) {
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
	}

	if (icon) {
		w += 25;
	}

	int x = 625 - w;
	
	// darkening quad...
	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);

	DrawQuad(gsGlobal, x    , yfix2(360.0f), 
		           x + w, yfix2(360.0f), 
			   x    , yfix2(400.0f), 
			   x + w, yfix2(400.0f), z, Darker);
			   
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);

	if (icon) {
		DrawIconN(icon, x+ 10, yfix2(370), 1.0f);
		DrawText(x + 40, yfix2(375), hint, 1, 0);
	} else {
		DrawText(x+ 15, yfix2(375), hint, 1, 0);
	}
}

void DrawSubMenu() {
	if (!selected_item)
		return;

	if(v_anim<100 && direc==4){
		v_anim+=10;
	}else if(v_anim>100 && direc==2){
		v_anim-=10;
	}
	
	TextColor(text_color[0],text_color[1],text_color[2],0xff);

	struct TSubMenuList *cur = selected_item->item->current;
	
	if (!cur) // no rendering if empty
		return;
	
	// prev item
	struct TSubMenuList *prev = cur->prev;
	
	
	int others = 0;
	
	while (prev && (others < _vbefore)) {
		DrawIcon(prev->item.icon, (100)+50, yfix2(((v_anim - 60) - others * _vspacing)),1);
		// Display the prev. item's text:
		/* if ((v_anim>=80) || (others > 1))
			DrawText((100)+150, (v_anim - 40) - others * _vspacing, prev->item.text,1.0f,0);
		
		*/
		prev = prev->prev; others++;
	}
	
	DrawIcon(cur->item.icon, (100)+50,yfix2((v_anim+130)),10);
	 if(v_anim==100)
	DrawText((100)+150,yfix2((v_anim+153)),GetSubItemText(&cur->item),1.0f,0);

	cur = cur->next;
	
	others = 0;
	
	while (cur && (others <= _vafter)) {
		DrawIcon(cur->item.icon, (100)+50, yfix2((v_anim + 210 + others * _vspacing)),1);
		//DrawText((100)+150, v_anim + 235 + others * _vspacing,cur->item.text,1.0f,0);
		cur = cur->next;
		others++;
	}
}


int ShowKeyb(char* text, size_t maxLen) {
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
	
	char *commands[4]={"BACKSPACE []", "SPACE /\\", "ENTER (Start)", "MODE (Sel.)"};
	
	keyb=keyb1;
	
	while(1){
		ReadPad();
				
		DrawBackground();
		
		TextColor(0xff,0xff,0xff,0xff);
		
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		DrawQuad(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, z, Darker);
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		
		//Text
		DrawText(50, yfix2(120), text, 1.0f, 0);
		
		// separating line for simpler orientation
		int ypos = yfix2(135);
		DrawLine(gsGlobal, 25, ypos, 600, ypos, 1, White);
		DrawLine(gsGlobal, 25, ypos + 1, 600, ypos + 1, 1, White);

		
		// Numbers
		for(i=0;i<=9;i++){
			if(i==selchar){
				TextColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				TextColor(0xff,0xff,0xff,0xff);
			}
			c[0]=keyb[i];
			DrawText(50+(i*32), yfix2(170), c, 1.0f, 0);
		}
		
		// QWERTYUIOP
		for(i=0;i<=9;i++){
			if(i+10==selchar){
				TextColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				TextColor(0xff,0xff,0xff,0xff);
			}
			c[0]=keyb[i+10];
			DrawText(50+(i*32), yfix2(200), c, 1.0f, 0);
		}
		
		// ASDFGHJKL'
		for(i=0;i<=9;i++){
			if(i+20==selchar){
				TextColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				TextColor(0xff,0xff,0xff,0xff);
			}
			c[0]=keyb[i+20];
			DrawText(50+(i*32), yfix2(230), c, 1.0f, 0);
		}
		
		// ZXCVBNM,.?
		for(i=0;i<=9;i++){
			if(i+30==selchar){
				TextColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				TextColor(0xff,0xff,0xff,0xff);
			}
			c[0]=keyb[i+30];
			DrawText(50+(i*32), yfix2(260), c, 1.0f, 0);
		}
		
		// Commands
		for(i=0;i<=3;i++){
			if(i==selcommand){
				TextColor(bg_color[0]/2,bg_color[1]/2,bg_color[2]/2, 0x80);
			}else{
				TextColor(0xff,0xff,0xff,0xff);
			}
			DrawText(384, yfix2((170+(30*i))), commands[i], 1.0f, 0);
		}
		
		Flip();
		
		if(GetKey(KEY_LEFT)){
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
		}else if(GetKey(KEY_RIGHT)){
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
		}else if(GetKey(KEY_UP)){
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
		}else if(GetKey(KEY_DOWN)){
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
		}else if(GetKeyOn(KEY_CROSS)){
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
		}else if(GetKey(KEY_SQUARE)){
			if(len>0){ // BACKSPACE
				len--;
				text[len]=0;
			}
		}else if(GetKey(KEY_TRIANGLE)){
			if(len<(maxLen-1) && selchar>-1){ // SPACE
				len++;
				c[0]=' ';
				strcat(text,c);
			}
		}else if(GetKeyOn(KEY_START)){
			return 1; //ENTER
		} else if (GetKeyOn(KEY_SELECT)) {
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
		
		if(GetKey(KEY_CIRCLE))break;
	}
	
	return 0;
}

void MenuNextH() {
	if (!selected_item) {
		selected_item = menu;
	}
	
	if(selected_item->next){
		h_anim=200;
		selected_item = selected_item->next;
		
		/*
		if (selected_item->item->resetOrder) // reset the order if callback exists
			selected_item->item->resetOrder(selected_item->item); // could also be getCurSelection to preserve the order
		else{
			selected_item->item->current = selected_item->item->submenu;
		}
		*/
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
		
		
		actualgame=firstgame;
		v_anim=100;
		direc=3;
	}
}

void MenuPrevH() {
	if (!selected_item) {
		selected_item = menu;
	}
	
	if(selected_item->prev){
		h_anim=0;
		selected_item = selected_item->prev;
		
		/*
		if (selected_item->item->resetOrder) // reset the order if callback exists
			selected_v = selected_item->item->resetOrder(selected_item->item); // can preserve order this way
		else {
			selected_v = 0;
			selected_item->item->current = selected_item->item->submenu;
		}
		*/
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
		
		actualgame=firstgame;
		v_anim=100;
		direc=1;
	}
}

void MenuNextV() {
	if (!selected_item)
		return;
	
	struct TSubMenuList *cur = selected_item->item->current;
	
	if(cur && cur->next) {
		if (selected_item->item->nextItem)
			selected_item->item->nextItem(selected_item->item);
	
		selected_item->item->current = cur->next;
		
		v_anim=200;
		direc=2;
	}
}

void MenuPrevV() {
	if (!selected_item)
		return;
	
	struct TSubMenuList *cur = selected_item->item->current;
	
	if(cur && cur->prev) {
		if (selected_item->item->prevItem)
			selected_item->item->prevItem(selected_item->item);
		
		selected_item->item->current = cur->prev;
			
		v_anim=0;
		direc=4;

	}
}

struct TMenuItem* MenuGetCurrent() {
	if (!selected_item)
		return NULL;

	return selected_item->item;
}

void MenuItemExecute() {
	if (selected_item && (selected_item->item->execute)) {
		// selected submenu id. default -1 = no selection
		int subid = -1;
		
		struct TSubMenuList *cur = selected_item->item->current;
		
		if (cur)
			subid = cur->item.id;
		
		selected_item->item->execute(selected_item->item, subid);
	}
}

void MenuItemAltExecute() {
	if (selected_item && (selected_item->item->altExecute)) {
		// selected submenu id. default -1 = no selection
		int subid = -1;
		
		struct TSubMenuList *cur = selected_item->item->current;
		
		if (cur)
			subid = cur->item.id;
		
		selected_item->item->altExecute(selected_item->item, subid);
	}
}

void MenuSetSelectedItem(struct TMenuItem* item) {
	struct TMenuList* itm = menu;
	
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

void RefreshSubMenu() {
	if (selected_item && (selected_item->item->refresh)) {
		selected_item->item->refresh(selected_item->item);
	}
}

void DrawScreenStatic() {
	// we render a static variant of the menu (no animations)
	// ------------------------------------------------------
	// -------- 0. the background ---------------------------
	// ------------------------------------------------------
	DrawBackground();
	
	// ------------------------------------------------------
	// -------- 1. the icons that symbolise the hmenu -------
	// ------------------------------------------------------
	TextColor(text_color[0],text_color[1],text_color[2], 0xff);
	
	// we start at 10, 10 and move by appropriate size right
	
	// we start at prev, if available
	struct TMenuList *cur_item = menu;
	
	int xpos = 30;
	
	while (cur_item) {
		if(cur_item == selected_item) {
			DrawIcon(cur_item->item->icon, xpos + 20, yfix2(20), 20);
 			DrawText(30, yfix2(100), GetMenuItemText(cur_item->item), 1.0f, 0);
 			xpos += 105;
		} else {
			DrawIcon(cur_item->item->icon, xpos, yfix2(20), 1);
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
	int sur_items = 5;
	int curpos = 125 + sur_items * spacing;
	
	TextColor(text_color[0],text_color[1],text_color[2],0xff);
	
		
	struct TSubMenuList *cur = selected_item->item->current;
	
	if (!cur) // no rendering if empty
		return;
	
	// prev item
	struct TSubMenuList *prev = cur->prev;
	
	int others = 1;
	
	while (prev && (others <= sur_items)) {
		if (draw_icons)
			DrawIcon(prev->item.icon, 10, yfix2((curpos - others * spacing - iconhalf)), iscale);
		DrawText(10 + icon_h_spacing, yfix2((curpos - others * spacing)), GetSubItemText(&prev->item),1.0f, 0);
		
		prev = prev->prev; others++;
	}
	
	// a sorta yellow colour for the selection
	TextColor(0xff, 0x080, 0x00, 0xff);
	if (draw_icons)
			DrawIcon(cur->item.icon, 10, yfix2((curpos - iconhalf)), iscale);
	DrawText(10 + icon_h_spacing, yfix2(curpos), GetSubItemText(&cur->item), 1.0f, 0);
		
	cur = cur->next;
	
	others = 1;
	
	TextColor(text_color[0],text_color[1],text_color[2],0xff);
	
	while (cur && (others <= sur_items)) {
		if (draw_icons)
			DrawIcon(cur->item.icon, 10, yfix2((curpos + others * spacing - iconhalf)), iscale);
		DrawText(10 + icon_h_spacing, yfix2((curpos + others * spacing)), GetSubItemText(&cur->item), 1.0f, 0);
		
		cur = cur->next; others++;
	}
}

void SwapMenu() {
	SetMenuDynamic(!dynamic_menu);
}

void SetMenuDynamic(int dynamic) {
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


void DrawScreen() {
	// dynamic or static render?
	if (!dynamic_menu) {
		DrawScreenStatic();
	} else {
		// default render:
		DrawBackground();
		DrawIcons();
		DrawInfo();
	}
}


// ----------------------------------------------------------------------------
// --------------------------- Dialogue handling ------------------------------
// ----------------------------------------------------------------------------
const char *diaGetLocalisedText(const char* def, int id) {
	if (id >= 0)
		return _l(id);
	
	return def;
}
// returns true if the item is controllable (e.g. a value can be changed on it)
int diaIsControllable(struct UIItem *ui) {
	return (ui->type >= UI_OK);
}

// returns true if the given item should be preceded with nextline
int diaShouldBreakLine(struct UIItem *ui) {
	return (ui->type == UI_SPLITTER || ui->type == UI_OK || ui->type == UI_BREAK);
}

// returns true if the given item should be superseded with nextline
int diaShouldBreakLineAfter(struct UIItem *ui) {
	return (ui->type == UI_SPLITTER);
}

// renders an ui item (either selected or not)
// sets width and height of the render into the parameters
void diaRenderItem(int x, int y, struct UIItem *item, int selected, int haveFocus, int *w, int *h) {
	// height fixed for now
	*h = UI_ROW_HEIGHT;
	
	// all texts are rendered up from the given point!
	
	if (selected) {
		if (haveFocus) // a slightly different color for focus instead of selection
			TextColor(0xff, 0x080, 0x00, 0xff);
		else
			TextColor(0x00, 0x0ff, 0x00, 0xff);
			
	} else {
		if (diaIsControllable(item))
			TextColor(0x00, 0x0ff, 0x80, 0xff);
		else
			TextColor(text_color[0], text_color[1], text_color[2], 0xff);

	}
	
	// let's see what do we have here?
	switch (item->type) {
		case UI_TERMINATOR:
			return;
		
		case UI_BUTTON:	
		case UI_LABEL: {
				// width is text lenght in pixels...
				const char *txt = diaGetLocalisedText(item->label.text, item->label.stringId);
				*w = strlen(txt) * gsFont->CharWidth;
			
				DrawText(x, y, txt, 1.0f, 0);
				break;
			}
		case UI_SPLITTER: {
				// a line. Thanks to the font rendering, we need to shift up by one font line
				*w = 0; // nothing to render at all
				*h = UI_SPACING_H;
				int ypos = y - UI_SPACING_V / 2; //  gsFont->CharHeight +
				// two lines for lesser eye strain :)
				DrawLine(gsGlobal, x, ypos, x + UI_BREAK_LEN, ypos, 1, White);
				DrawLine(gsGlobal, x, ypos + 1, x + UI_BREAK_LEN, ypos + 1, 1, White);
				break;
			}
		case UI_BREAK:
			*w = 0; // nothing to render at all
			*h = 0;
			break;

		case UI_SPACER: {
				// next column divisible by spacer
				*w = (UI_SPACER_WIDTH - x % UI_SPACER_WIDTH);
		
				if (*w < UI_SPACER_MINIMAL) {
					x += *w + UI_SPACER_MINIMAL;
					*w += (UI_SPACER_WIDTH - x % UI_SPACER_WIDTH);
				}

				*h = 0;
				break;
			}

		case UI_OK: {
				const char *txt = _l(_STR_OK);
				*w = strlen(txt) * gsFont->CharWidth;
				*h = gsFont->CharHeight;
				
				DrawText(x, y, txt, 1.0f, 0);
				break;
			}

		case UI_INT: {
				char tmp[10];
				
				snprintf(tmp, 10, "%d", item->intvalue.current);
				*w = strlen(tmp) * gsFont->CharWidth;
				
				DrawText(x, y, tmp, 1.0f, 0);
				break;
			}
		case UI_STRING: {
				*w = strlen(item->stringvalue.text) * gsFont->CharWidth;
				
				DrawText(x, y, item->stringvalue.text, 1.0f, 0);
				break;
			}
		case UI_BOOL: {
				const char *txtval = _l((item->intvalue.current) ? _STR_ON : _STR_OFF);
				*w = strlen(txtval) * gsFont->CharWidth;
				
				DrawText(x, y, txtval, 1.0f, 0);
				break;
			}
		case UI_ENUM: {
				const char* tv = item->intvalue.enumvalues[item->intvalue.current];

				if (!tv)
					tv = "<no value>";

				*w = strlen(tv) * gsFont->CharWidth;
				
				DrawText(x, y, tv, 1.0f, 0);
				break;
		}
	}
}

// renders whole ui screen (for given dialog setup)
void diaRenderUI(struct UIItem *ui, struct UIItem *cur, int haveFocus) {
	// clear screen, draw background
	DrawScreen();
	
	// darken for better contrast
	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
	DrawQuad(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, z, Darker);
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);


	// TODO: Sanitize these values
	int x0 = 20;
	int y0 = 20;
	
	// render all items
	struct UIItem *rc = ui;
	int x = x0, y = y0, hmax = 0;
	
	while (rc->type != UI_TERMINATOR) {
		int w = 0, h = 0;

		if (diaShouldBreakLine(rc)) {
			x = x0;
			
			if (hmax > 0)
				y += hmax + UI_SPACING_H; 
			
			hmax = 0;
		}
		
		diaRenderItem(x, y, rc, rc == cur, haveFocus, &w, &h);
		
		if (w > 0)
			x += w + UI_SPACING_V;
		
		hmax = (h > hmax) ? h : hmax;
		
		if (diaShouldBreakLineAfter(rc)) {
			x = x0;
			
			if (hmax > 0)
				y += hmax + UI_SPACING_H; 
			
			hmax = 0;
		}
		
		rc++;
	}
	
	// flip display
	Flip();
}

// sets the ui item value to the default again
void diaResetValue(struct UIItem *item) {
	switch(item->type) {
		case UI_INT:
		case UI_BOOL:
			item->intvalue.current = item->intvalue.def;
			return;
		case UI_STRING:
			strncpy(item->stringvalue.text, item->stringvalue.def, 32);
			return;
		default:
			return;
	}
}

int diaHandleInput(struct UIItem *item) {
	// circle loses focus, sets old values first
	if (GetKeyOn(KEY_CIRCLE)) {
		diaResetValue(item);
		return 0;
	}
	
	// cross loses focus without setting default
	if (GetKeyOn(KEY_CROSS))
		return 0;
	
	// UI item type dependant part:
	if (item->type == UI_BOOL) {
		// a trick. Set new value, lose focus
		item->intvalue.current = !item->intvalue.current;
		return 0;
	} if (item->type == UI_INT) {
		// to be sure
		SetButtonDelay(KEY_UP, 1);
		SetButtonDelay(KEY_DOWN, 1);

		// up and down
		if (GetKey(KEY_UP) && (item->intvalue.current < item->intvalue.max))
			item->intvalue.current++;
		
		if (GetKey(KEY_DOWN) && (item->intvalue.current > item->intvalue.min))
			item->intvalue.current--;
		
	} else if (item->type == UI_STRING) {
		char tmp[32];
		strncpy(tmp, item->stringvalue.text, 32);

		if (ShowKeyb(tmp, 32))
			strncpy(item->stringvalue.text, tmp, 32);

		return 0;
	} else if (item->type == UI_ENUM) {
		int cur = item->intvalue.current;

		if (item->intvalue.enumvalues[cur] == NULL) {
			if (cur > 0)
				item->intvalue.current--;
			else
				return 0;
		}

		if (GetKey(KEY_UP) && (cur > 0))
			item->intvalue.current--;
		
		++cur;

		if (GetKey(KEY_DOWN) && (item->intvalue.enumvalues[cur] != NULL))
			item->intvalue.current = cur;
	}
	
	return 1;
}

struct UIItem *diaGetFirstControl(struct UIItem *ui) {
	struct UIItem *cur = ui;
	
	while (!diaIsControllable(cur)) {
		if (cur->type == UI_TERMINATOR)
			return ui; 
		
		cur++;
	}
	
	return cur;
}

struct UIItem *diaGetLastControl(struct UIItem *ui) {
	struct UIItem *last = diaGetFirstControl(ui);
	struct UIItem *cur = last;
	
	while (cur->type != UI_TERMINATOR) {
		cur++;
		
		if (diaIsControllable(cur))
			last = cur;
	}
	
	return last;
}

struct UIItem *diaGetNextControl(struct UIItem *cur, struct UIItem* dflt) {
	while (cur->type != UI_TERMINATOR) {
		cur++;
		
		if (diaIsControllable(cur))
			return cur;
	}
	
	return dflt;
}

struct UIItem *diaGetPrevControl(struct UIItem* cur, struct UIItem *ui) {
	struct UIItem *newf = cur;
	
	while (newf != ui) {
		newf--;
		
		if (diaIsControllable(newf))
			return newf;
	}
	
	return cur;
}

// finds first control on previous line...
struct UIItem *diaGetPrevLine(struct UIItem* cur, struct UIItem *ui) {
	struct UIItem *newf = cur;
	
	int lb = 0;
	int hadCtrl = 0; // had the scanned line any control?
	
	while (newf != ui) {
		newf--;
		
		if ((lb > 0) && (diaIsControllable(newf)))
			hadCtrl = 1;
		
		if (diaShouldBreakLine(newf)) {// is this a line break?
			if (hadCtrl || lb == 0) {
				lb++;
				hadCtrl = 0;
			}
		}
		
		// twice the break? find first control
		if (lb == 2)
			return diaGetFirstControl(newf);
	}
	
	return cur;
}

struct UIItem *diaGetNextLine(struct UIItem* cur, struct UIItem *ui) {
	struct UIItem *newf = cur;
	
	int lb = 0;
	
	while (newf->type != UI_TERMINATOR) {
		newf++;
		
		if (diaShouldBreakLine(newf)) {// is this a line break?
				lb++;
		}
		
		if (lb == 1)
			return diaGetFirstControl(newf);
	}
	
	return cur;
}

int diaExecuteDialog(struct UIItem *ui) {
	struct UIItem *cur = diaGetFirstControl(ui);
	
	// what? no controllable item? Exit!
	if (cur == ui)
		return -1;
	
	int haveFocus = 0;
	
	// okay, we have the first selectable item
	// we can proceed with rendering etc. etc.
	while (1) {
		diaRenderUI(ui, cur, haveFocus);
		
		ReadPad();
		
		if (haveFocus) {
			haveFocus = diaHandleInput(cur);
			
			if (!haveFocus) {
				SetButtonDelay(KEY_UP, 5);
				SetButtonDelay(KEY_DOWN, 5);
			}
		} else {
			if (GetKey(KEY_LEFT)) {
				struct UIItem *newf = diaGetPrevControl(cur, ui);
				
				if (newf == cur)
					cur = diaGetLastControl(ui);
				else 
					cur = newf;
			}
			
			if (GetKey(KEY_RIGHT)) {
				struct UIItem *newf = diaGetNextControl(cur, cur);
				
				if (newf == cur)
					cur = diaGetFirstControl(ui);
				else 
					cur = newf;
			}
			
			if(GetKey(KEY_UP)) {
				// find
				struct UIItem *newf = diaGetPrevLine(cur, ui);
				
				if (newf == cur)
					cur = diaGetLastControl(ui);
				else 
					cur = newf;
			}
			
			if(GetKey(KEY_DOWN)) {
				// find
				struct UIItem *newf = diaGetNextLine(cur, ui);
				
				if (newf == cur)
					cur = diaGetFirstControl(ui);
				else 
					cur = newf;
			}
			
			// circle breaks focus or exits with false result
			if (GetKeyOn(KEY_CIRCLE))
					return 0;
			
			// see what key events we have
			if (GetKeyOn(KEY_CROSS)) {
				haveFocus = 1;
				
				if (cur->type == UI_BUTTON)
					return cur->id;

				if (cur->type == UI_OK)
					return 1;
			}
		}
	}
}

struct UIItem* diaFindByID(struct UIItem* ui, int id) {
	while (ui->type != UI_TERMINATOR) {
		if (ui->id == id)
			return ui;
			
		ui++;
	}
	
	return NULL;
}

int diaGetInt(struct UIItem* ui, int id, int *value) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if ((item->type != UI_INT) && (item->type != UI_BOOL) && (item->type != UI_ENUM))
		return 0;
	
	*value = item->intvalue.current;
	return 1;
}

int diaSetInt(struct UIItem* ui, int id, int value) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if ((item->type != UI_INT) && (item->type != UI_BOOL) && (item->type != UI_ENUM))
		return 0;
	
	item->intvalue.def = value;
	item->intvalue.current = value;
	return 1;
}

int diaGetString(struct UIItem* ui, int id, char *value) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if (item->type != UI_STRING)
		return 0;
	
	strncpy(value, item->stringvalue.text, 32);
	return 1;
}

int diaSetString(struct UIItem* ui, int id, const char *text) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if (item->type != UI_STRING)
		return 0;
	
	strncpy(item->stringvalue.def, text, 32);
	strncpy(item->stringvalue.text, text, 32);
	return 1;
}

int diaSetLabel(struct UIItem* ui, int id, const char *text) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if (item->type != UI_LABEL)
		return 0;
	
	item->label.text = text;
	return 1;
}

int diaSetEnum(struct UIItem* ui, int id, const char **enumvals) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if (item->type != UI_ENUM)
		return 0;
	
	item->intvalue.enumvalues = enumvals;
	return 1;
}
