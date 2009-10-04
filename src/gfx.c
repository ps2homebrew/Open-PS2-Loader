#include "include/usbld.h"
#include <assert.h>

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

GSGLOBAL *gsGlobal;
GSTEXTURE font;
GSFONT *gsFont;

GSTEXTURE background;

GSTEXTURE disc_icon;
GSTEXTURE games_icon;
GSTEXTURE config_icon;
GSTEXTURE exit_icon;
GSTEXTURE theme_icon;
GSTEXTURE language_icon;

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

char infotxt[256]="               WELCOME TO OPEN USB LOADER. (C) 2009 IFCARO <http://ps2dev.ifcaro.net>. BASED ON SOURCE CODE OF HD PROJECT <http://psx-scene.com>";
int z;

/// scroll speed selector
struct TSubMenuList* speed_item = NULL;
unsigned int curscroll = 1;
char scroll_speed[3][14] = {"Scroll Slow", "Scroll Medium", "Scroll Fast"};

void ChangeScrollSpeed() {
	curscroll = (curscroll + 1) % 3;
	speed_item->item.text = scroll_speed[curscroll];
	
	// update the pad delays for KEY_UP and KEY_DOWN
	// default delay is 7
	SetButtonDelay(KEY_UP, 10 - curscroll * 3); // 0,1,2 -> 10, 7, 4
	SetButtonDelay(KEY_DOWN, 10 - curscroll * 3);
}


char txt_settings[2][32]={"Configure theme", "Select language"};

// forward decls.
void MsgBox();
void DrawConfig();
void LoadResources();

// Count of icons before and after the selected one
unsigned int _vbefore = 1;
unsigned int _vafter = 1;
// spacing between icons, vertical.
unsigned int _vspacing = 80;

/// menu item selection callbacks
void ExecExit(struct TMenuItem* self, int vorder) {
	__asm__ __volatile__(
		"	li $3, 0x04;"
		"	syscall;"
		"	nop;"
	);
}

void ExecSettings(struct TMenuItem* self, int id) {
	if (id == 1) {
		DrawConfig();
	} else if(id == 2) {
		MsgBox();
	} else if (id == 3) {
		// scroll speed modifier
		ChangeScrollSpeed();
	}
}

/* Menu item definitions. Reversed order. */
struct TSubMenuList *settings_submenu = NULL;

struct TMenuItem settings_item = {
	&config_icon, "Settings", NULL, NULL, NULL, &ExecSettings, NULL, NULL
};

struct TMenuItem exit_item = {
	&exit_icon, "Exit", NULL, NULL, NULL, &ExecExit, NULL, NULL
};

void InitMenu() {
	LoadResources();

	// initialize the menu
	AppendSubMenu(&settings_submenu, &theme_icon, "Theme", 1);
	AppendSubMenu(&settings_submenu, &language_icon, "Language", 2);
	speed_item = AppendSubMenu(&settings_submenu, &config_icon, scroll_speed[curscroll], 3);
	settings_item.submenu = settings_submenu;
	
	// add all menu items
	menu = NULL;
	AppendMenuItem(&exit_item);
	AppendMenuItem(&settings_item);

	// Epilogue
	selected_item = menu;
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

void InitGFX() {

	waveframe=0;
	frame=0;
	h_anim=100;
	v_anim=100;
	direc=0;
	max_settings=2;
	max_games=10;
	font.Vram = 0;

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


struct TSubMenuList* AllocSubMenuItem(GSTEXTURE *icon, char *text, int id) {
	struct TSubMenuList* it;
	
	it = malloc(sizeof(struct TSubMenuList));
	
	it->prev = NULL;
	it->next = NULL;
	it->item.icon = icon;
	it->item.text = text;
	it->item.id = id;
	
	return it;
}

struct TSubMenuList* AppendSubMenu(struct TSubMenuList** submenu, GSTEXTURE *icon, char *text, int id) {
	if (*submenu == NULL) {
		*submenu = AllocSubMenuItem(icon, text, id);
		return *submenu; 
	}
	
	struct TSubMenuList *cur = *submenu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	struct TSubMenuList *newitem = AllocSubMenuItem(icon, text, id);
	
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
		sprintf(introtxt,"Open USB Loader %s", USBLD_VERSION);
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

void LoadResources() {
	ReadConfig("mass:USBLD/usbld.cfg");
	LoadIcons();
	UpdateIcons();
	LoadFont(1);
	UpdateFont();
}

void DrawBackground(){
	z=1;

	gsKit_vram_clear(gsGlobal);

	//BACKGROUND
	if(background_image==1){
		background.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(background.Width, background.Height, background.PSM), GSKIT_ALLOC_USERBUFFER);
		gsKit_texture_upload(gsGlobal, &background);
		
		DrawSprite_texture(gsGlobal, &background, 0.0f,  // X1
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

void DrawText(int x, int y, char *texto, float scale, int centered){
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
		
		DrawText(305,120,"Theme configuration",1,1);
		
		if(editing_bg_color==0){
			sprintf(tmp,"Background color: #%02X%02X%02X", new_bg_color[0],new_bg_color[1],new_bg_color[2]);
			if(v_pos==0){
				TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			}else{
				TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			}
			DrawText(120,200,tmp,0.8f,0);
		}else if(editing_bg_color==1){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			sprintf(tmp,"Background color: #  %02X%02X",new_bg_color[1],new_bg_color[2]);
			DrawText(120,200,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			sprintf(tmp,"                   %02X", new_bg_color[0]);
			DrawText(120,200,tmp,0.8f,0);
		}else if(editing_bg_color==2){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			sprintf(tmp,"Background color: #%02X  %02X",new_bg_color[0],new_bg_color[2]);
			DrawText(120,200,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			sprintf(tmp,"                     %02X", new_bg_color[1]);
			DrawText(120,200,tmp,0.8f,0);
		}else if(editing_bg_color==3){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			sprintf(tmp,"Background color: #%02X%02X",new_bg_color[0],new_bg_color[1]);
			DrawText(120,200,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			sprintf(tmp,"                       %02X", new_bg_color[2]);
			DrawText(120,200,tmp,0.8f,0);
		}
		if(editing_text_color==0){
			sprintf(tmp,"Text color: #%02X%02X%02X", new_text_color[0],new_text_color[1],new_text_color[2]);
			if(v_pos==1){
				TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			}else{
				TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			}
			DrawText(120,240,tmp,0.8f,0);
		}else if(editing_text_color==1){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			sprintf(tmp,"Text color: #  %02X%02X",new_text_color[1],new_text_color[2]);
			DrawText(120,240,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			sprintf(tmp,"             %02X", new_text_color[0]);
			DrawText(120,240,tmp,0.8f,0);
		}else if(editing_text_color==2){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			sprintf(tmp,"Text color: #%02X  %02X",new_text_color[0],new_text_color[2]);
			DrawText(120,240,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			sprintf(tmp,"               %02X", new_text_color[1]);
			DrawText(120,240,tmp,0.8f,0);
		}else if(editing_text_color==3){
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			sprintf(tmp,"Text color: #%02X%02X",new_text_color[0],new_text_color[1]);
			DrawText(120,240,tmp,0.8f,0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			sprintf(tmp,"                 %02X", new_text_color[2]);
			DrawText(120,240,tmp,0.8f,0);
		}
		if(editing_theme_dir==0){
			sprintf(tmp,"Selected Theme: %s", theme);
			if(v_pos==2){
				TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			}else{
				TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			}
			DrawText(120,280,tmp,0.8f, 0);
		}else{
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
			sprintf(tmp,"Selected Theme:");
			DrawText(120,280,tmp,0.8f, 0);
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
			sprintf(tmp,"                %s", theme_dir[selected_theme_dir]);
			DrawText(120,280,tmp,0.8f, 0);
		}
		
		if(v_pos==3){
			TextColor(new_bg_color[0]/2,new_bg_color[1]/2,new_bg_color[2]/2, 0x80);
		}else{
			TextColor(new_text_color[0], new_text_color[1], new_text_color[2], 0xff);
		}
		DrawText(310,370,"Save changes",1, 1);
		
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
			}else if(GetKey(KEY_CIRCLE)){
				editing_bg_color=0;
				SetButtonDelay(KEY_UP, 5);
				SetButtonDelay(KEY_DOWN, 5);
				new_bg_color[0]=bg_color[0];
				new_bg_color[1]=bg_color[1];
				new_bg_color[2]=bg_color[2];
			}else if(GetKey(KEY_CROSS)){
				editing_bg_color=0;
				SetButtonDelay(KEY_UP, 5);
				SetButtonDelay(KEY_DOWN, 5);
				bg_color[0]=new_bg_color[0];
				bg_color[1]=new_bg_color[1];
				bg_color[2]=new_bg_color[2];
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
			}else if(GetKey(KEY_CIRCLE)){
				editing_text_color=0;
				SetButtonDelay(KEY_UP, 10);
				SetButtonDelay(KEY_DOWN, 10);
				new_text_color[0]=text_color[0];
				new_text_color[1]=text_color[1];
				new_text_color[2]=text_color[2];
			}else if(GetKey(KEY_CROSS)){
				editing_text_color=0;
				SetButtonDelay(KEY_UP, 10);
				SetButtonDelay(KEY_DOWN, 10);
				text_color[0]=new_text_color[0];
				text_color[1]=new_text_color[1];
				text_color[2]=new_text_color[2];

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
			}else if(GetKey(KEY_CIRCLE)){
				editing_theme_dir=0;
			}else if(GetKey(KEY_CROSS)){
				editing_theme_dir=0;
				strcpy(theme,theme_dir[selected_theme_dir]);
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
			}else if(GetKey(KEY_CROSS)){
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
				}else if(v_pos==3){
					SaveConfig("mass:USBLD/usbld.cfg");
					background_image=LoadBackground();
					LoadIcons();
					LoadFont();
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
}

void MsgBox(){
	while(1){
		ReadPad();
				
		DrawScreen();
		
		TextColor(0xff,0xff,0xff,0xff);
		
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
		DrawQuad(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, z, Darker);
		gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
		DrawLine(gsGlobal, 50.0f, 75.0f, 590.0f, 75.0f, 1, White);
		DrawLine(gsGlobal, 50.0f, 410.0f, 590.0f, 410.0f, 1, White);
		DrawText(310, 250, "Not available yet", 1.0f, 1);
		DrawText(400, 417, "O Back", 1.0f, 0);
		
		Flip();
		if(GetKey(KEY_CIRCLE))break;
	}
}

void LoadFont(int load_default){
		
	char tmp[255];
	
	font.Width = 256;
	font.Height = 256;
	font.PSM = GS_PSM_CT32;
	sprintf(tmp,"mass:USBLD/%s/%s",theme,"font.raw");

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

void UpdateFont(){
	font.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(font.Width, font.Height, font.PSM), GSKIT_ALLOC_USERBUFFER);
	gsKit_texture_upload(gsGlobal, &font);
}

int LoadRAW(char *path, GSTEXTURE *Texture){
	int fd, size, ret=0;
	void *buffer;
	
	fd=fioOpen(path, O_RDONLY);
	if(fd>=0){
		size = fioLseek(fd, 0, SEEK_END);  
		fioLseek(fd, 0, SEEK_SET);  	
		buffer=malloc(size);
		fioRead(fd, buffer, size);
		if(Texture->Mem!=NULL)free(Texture->Mem);
		Texture->Mem=buffer;
		ret=1;
	}
	fioClose(fd);
	return ret;
}

int LoadBackground(){
	int ret=0;
	char tmp[255];
	
	bg_color[0]=0x00;
	bg_color[1]=0x00;
	bg_color[2]=0xff;
	
	text_color[0]=0xff;
	text_color[1]=0xff;
	text_color[2]=0xff;
	
	sprintf(tmp,"mass:USBLD/%s/%s",theme,"Background.raw");
	
	if(LoadRAW(tmp, &background)==1){
		background.Width = 640;
		background.Height = 480;
		background.PSM = GS_PSM_CT24;
		background.Filter = GS_FILTER_LINEAR;
		ret=1;
	}
	
	return ret;
}

void LoadIcons(){
	char tmp[255];

	//EXIT
	exit_icon.Width = 64;
	exit_icon.Height = 64;
	exit_icon.PSM = GS_PSM_CT32;
	exit_icon.Filter = GS_FILTER_LINEAR;
	sprintf(tmp,"mass:USBLD/%s/%s",theme,"exit.raw");
	if(LoadRAW(tmp, &exit_icon)==0){
	exit_icon.Mem=(u32*)&exit_raw;
	//exit_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(exit_icon.Width, exit_icon.Height, exit_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	//gsKit_texture_upload(gsGlobal, &exit_icon);
	}
	
	//CONFIG
	config_icon.Width = 64;
	config_icon.Height = 64;
	config_icon.PSM = GS_PSM_CT32;
	config_icon.Filter = GS_FILTER_LINEAR;
	sprintf(tmp,"mass:USBLD/%s/%s",theme,"config.raw");
	if(LoadRAW(tmp, &config_icon)==0){
	config_icon.Mem=(u32*)&config_raw;
	//config_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(config_icon.Width, config_icon.Height, config_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	//gsKit_texture_upload(gsGlobal, &config_icon);
	}
	
	//GAMES
	games_icon.Width = 64;
	games_icon.Height = 64;
	games_icon.PSM = GS_PSM_CT32;
	games_icon.Filter = GS_FILTER_LINEAR;
	sprintf(tmp,"mass:USBLD/%s/%s",theme,"games.raw");
	if(LoadRAW(tmp, &games_icon)==0){
	games_icon.Mem=(u32*)&games_raw;
	//games_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(games_icon.Width, games_icon.Height, games_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	//gsKit_texture_upload(gsGlobal, &games_icon);
	}
	
	//DISC
	disc_icon.Width = 64;
	disc_icon.Height = 64;
	disc_icon.PSM = GS_PSM_CT32;
	disc_icon.Filter = GS_FILTER_LINEAR;
	sprintf(tmp,"mass:USBLD/%s/%s",theme,"disc.raw");
	if(LoadRAW(tmp, &disc_icon)==0){
	disc_icon.Mem=(u32*)&disc_raw;
	//disc_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(disc_icon.Width, disc_icon.Height, disc_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	//gsKit_texture_upload(gsGlobal, &disc_icon);
	}
	
	//THEME
	theme_icon.Width = 64;
	theme_icon.Height = 64;
	theme_icon.PSM = GS_PSM_CT32;
	theme_icon.Filter = GS_FILTER_LINEAR;
	sprintf(tmp,"mass:USBLD/%s/%s",theme,"theme.raw");
	if(LoadRAW(tmp, &theme_icon)==0){
	theme_icon.Mem=(u32*)&theme_raw;
	//theme_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(theme_icon.Width, theme_icon.Height, theme_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	//gsKit_texture_upload(gsGlobal, &theme_icon);	
	}
	
	//LANGUAGE
	language_icon.Width = 64;
	language_icon.Height = 64;
	language_icon.PSM = GS_PSM_CT32;
	language_icon.Filter = GS_FILTER_LINEAR;
	sprintf(tmp,"mass:USBLD/%s/%s",theme,"language.raw");
	if(LoadRAW(tmp, &language_icon)==0){
	language_icon.Mem=(u32*)&language_raw;
	//language_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(language_icon.Width, language_icon.Height, language_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	//gsKit_texture_upload(gsGlobal, &language_icon);
	}
}

void UpdateIcons(){

	//EXIT
	exit_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(exit_icon.Width, exit_icon.Height, exit_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	gsKit_texture_upload(gsGlobal, &exit_icon);

	//CONFIG
	config_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(config_icon.Width, config_icon.Height, config_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	gsKit_texture_upload(gsGlobal, &config_icon);
	
	//GAMES
	games_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(games_icon.Width, games_icon.Height, games_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	gsKit_texture_upload(gsGlobal, &games_icon);

	//DISC
	disc_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(disc_icon.Width, disc_icon.Height, disc_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	gsKit_texture_upload(gsGlobal, &disc_icon);
	
	//THEME
	theme_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(theme_icon.Width, theme_icon.Height, theme_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	gsKit_texture_upload(gsGlobal, &theme_icon);	
	
	//LANGUAGE
	language_icon.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(language_icon.Width, language_icon.Height, language_icon.PSM), GSKIT_ALLOC_USERBUFFER);
	gsKit_texture_upload(gsGlobal, &language_icon);

}

void DrawIcon(GSTEXTURE *img, int x, int y, float scale){
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

void DrawIconByIndex(int id, int x, int y, float scale){
	GSTEXTURE *img=0;
	switch(id){
		case 1:
			img=&exit_icon;
		break;
		case 2:
			img=&config_icon;
		break;
		case 3:
			img=&games_icon;
		break;
		case 4:
			img=&disc_icon;
		break;
		case 5:
			img=&theme_icon;
		break;
		case 6:
			img=&language_icon;
		break;
	}
	
	DrawIcon(img, x, y, scale);
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
			DrawText(h_anim + xpos + 23, yfix2(190), cur_item->item->text, 1.0f, 1);
		
		cur_item = cur_item->next;
		xpos += 100;
	}
}

void DrawInfo() {
	char txt[16];

	TextColor(text_color[0],text_color[1],text_color[2],0xff);
	
	strncpy(txt,&infotxt[frame/10],15);
	DrawText(300,yfix2(53),txt,1,0);
	
	if (frame>2000) {
	 frame=0;
	} else {
	 frame++;	
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
		DrawIcon(prev->item.icon, (100)+50, yfix2(((v_anim - 40) - others * _vspacing)),1);
		// Display the prev. item's text:
		/* if ((v_anim>=80) || (others > 1))
			DrawText((100)+150, (v_anim - 40) - others * _vspacing, prev->item.text,1.0f,0);
		
		*/
		prev = prev->prev; others++;
	}
	
	DrawIcon(cur->item.icon, (100)+50,yfix2((v_anim+130)),10);
	 if(v_anim==100)
	DrawText((100)+150,yfix2((v_anim+153)),cur->item.text,1.0f,0);

	cur = cur->next;
	
	others = 0;
	
	while (cur && (others <= _vafter)) {
		DrawIcon(cur->item.icon, (100)+50, yfix2((v_anim + 210 + others * _vspacing)),1);
		//DrawText((100)+150, v_anim + 235 + others * _vspacing,cur->item.text,1.0f,0);
		cur = cur->next;
		others++;
	}
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
			DrawIcon(cur_item->item->icon, xpos, 10, 20);
			DrawText(30, 80, cur_item->item->text, 1.0f, 0);
			xpos += 80;
		} else {
			DrawIcon(cur_item->item->icon, xpos, 10, 1);
			xpos += 60;
		}

		cur_item = cur_item->next;
	}	
	
	// ------------------------------------------------------
	// -------- 2. the submenu ------------------------------
	// ------------------------------------------------------

	// we want to display N previous, the current and N next items (N == sur_items)
	// spacing is 45 pixels
	// x offset is 10 pixels
	// y offset is 100 pixels
	// the selected item is displayed at 100 + 3 * 60 = 280 px.
	if (!selected_item)
		return;
	
	float iscale = -15.0f; // not a scale... an additional spacing in pixels
	int draw_icons = 1;
	int iconhalf = 30;
	int spacing = 30;
	
	int icon_h_spacing = 0;
	
	if (draw_icons)
		icon_h_spacing = 45;
	
	// count of items before and after selection
	int sur_items = 5;
	int curpos = 120 + sur_items * spacing;
	
	TextColor(text_color[0],text_color[1],text_color[2],0xff);
	
		
	struct TSubMenuList *cur = selected_item->item->current;
	
	if (!cur) // no rendering if empty
		return;
	
	// prev item
	struct TSubMenuList *prev = cur->prev;
	
	int others = 1;
	
	while (prev && (others <= sur_items)) {
		if (draw_icons)
			DrawIcon(prev->item.icon, 10, curpos - others * spacing - iconhalf, iscale);
		DrawText(10 + icon_h_spacing, curpos - others * spacing, prev->item.text,1.0f, 0);
		
		prev = prev->prev; others++;
	}
	
	// a sorta yellow colour for the selection
	TextColor(0xff, 0x080, 0x00, 0xff);
	if (draw_icons)
			DrawIcon(cur->item.icon, 10, curpos - iconhalf, iscale);
	DrawText(10 + icon_h_spacing, curpos, cur->item.text, 1.0f, 0);
		
	cur = cur->next;
	
	others = 1;
	
	TextColor(text_color[0],text_color[1],text_color[2],0xff);
	
	while (cur && (others <= sur_items)) {
		if (draw_icons)
			DrawIcon(cur->item.icon, 10, curpos + others * spacing - iconhalf, iscale);
		DrawText(10 + icon_h_spacing, curpos + others * spacing, cur->item.text, 1.0f, 0);
		
		cur = cur->next; others++;
	}
	
}

void SwapMenu() {
	if (drawScreenPtr) {
		drawScreenPtr = NULL;
	} else {
		drawScreenPtr = &DrawScreenStatic;
	}
}


void DrawScreen() {
	// do we have a custom renderer pointer?
	if (drawScreenPtr) {
		drawScreenPtr();
	} else {
		// default render:
		DrawBackground();
		DrawIcons();
		DrawInfo();
	}
}
