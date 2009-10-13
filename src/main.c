#include "include/usbld.h"

extern void *usbd_irx;
extern int size_usbd_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

// Submenu items variant of the game list... keeped in sync with the game item list (firstgame, actualgame)
struct TSubMenuList* usb_submenu= NULL;
/// List of usb games
struct TMenuItem usb_games_item;

/// scroll speed selector
struct TSubMenuList* speed_item = NULL;

/// menu type selector
struct TSubMenuList* menutype_item = NULL;

char scroll_speed_txt[3][14] = {"Scroll Slow", "Scroll Medium", "Scroll Fast"};
char menu_type_txt[2][14] = {"Dynamic Menu", "Static Menu"};
char txt_settings[2][32]={"Configure theme", "Select language"};

void ClearUSBSubMenu() {
	DestroySubMenu(&usb_submenu);
	AppendSubMenu(&usb_submenu, &disc_icon, "No Items", -1);
	usb_games_item.submenu = usb_submenu;
	usb_games_item.current = usb_submenu;
}

void RefreshUSBGameList() {
	int fd, size, n;
	TGameList *list;
	TGame buffer;

	if(usbdelay<=100)  {
		usbdelay++;
		return;
	}
	  
	usbdelay = 0;
	fd=fioOpen("mass:ul.cfg", O_RDONLY);
	
	if(fd<0) { // file could not be opened. reset menu
		if(max_games>0) {
			actualgame=firstgame;
			while(actualgame->next!=0){
				actualgame=actualgame->next;
				free(actualgame->prev);
			}
			free(actualgame);
		}
		max_games=0;
		
		ClearUSBSubMenu();
		return;
	}

	// only refresh if the list is empty
	if (max_games==0) {
		DestroySubMenu(&usb_submenu);
		usb_submenu = NULL;
		usb_games_item.submenu = usb_submenu;
		usb_games_item.current = usb_submenu;

		size = fioLseek(fd, 0, SEEK_END);  
		fioLseek(fd, 0, SEEK_SET); 
		
		int id = 1; // game id (or order)
		for(n=0;n<size;n+=0x40,++id){
			fioRead(fd, &buffer, 0x40);
			list=(TGameList*)malloc(sizeof(TGameList));
			memcpy(&list->Game,&buffer,sizeof(TGame));
			if(actualgame!=NULL){
				actualgame->next=list;
				list->prev=actualgame;
			}else{
				firstgame=list;
				list->prev=0;
			}
			list->next=0;
			actualgame=list;
			max_games++;
			
			AppendSubMenu(&usb_submenu, &disc_icon, actualgame->Game.Name, id);
		}
		
			
		usb_games_item.submenu = usb_submenu;
		usb_games_item.current = usb_submenu;
		actualgame=firstgame;
	}

	
	fioClose(fd);
}

// --------------------- USB Menu item callbacks --------------------
void PrevUSBGame(struct TMenuItem *self) {
	if (!actualgame)
		return;
	
	if(actualgame->prev!=NULL){
		actualgame=actualgame->prev;
	}
}

void NextUSBGame(struct TMenuItem *self) {
	if (!actualgame)
		return;

	if(actualgame->next!=NULL){
		actualgame=actualgame->next;
	}
}

int ResetUSBOrder(struct TMenuItem *self) {
	actualgame = firstgame;
	return 0;
}

void ExecUSBGameSelection(struct TMenuItem* self, int id) {
	if (id == -1)
		return;
	
	padPortClose(0, 0);
	padPortClose(1, 0);
	padReset();
	LaunchGame(&actualgame->Game);
}

// --------------------- Exit/Settings Menu item callbacks --------------------
/// menu item selection callbacks
void ExecExit(struct TMenuItem* self, int vorder) {
	__asm__ __volatile__(
		"	li $3, 0x04;"
		"	syscall;"
		"	nop;"
	);
}

void ChangeScrollSpeed() {
	scroll_speed = (scroll_speed + 1) % 3;
	speed_item->item.text = scroll_speed_txt[scroll_speed];
	
	UpdateScrollSpeed();
}

void ChangeMenuType() {
	SwapMenu();
	
	menutype_item->item.text = menu_type_txt[dynamic_menu ? 0 : 1];
}

void ExecSettings(struct TMenuItem* self, int id) {
	if (id == 1) {
		DrawConfig();
	} else if(id == 2) {
		MsgBox();
	} else if (id == 3) {
		// scroll speed modifier
		ChangeScrollSpeed();
	} else if (id == 4) {
		ChangeMenuType();
	} else if (id == 6) {
		SaveConfig("mass:USBLD/usbld.cfg");
	}
}

// --------------------- Menu initialization --------------------

struct TSubMenuList *settings_submenu = NULL;

struct TMenuItem exit_item = {
	&exit_icon, "Exit", NULL, NULL, NULL, &ExecExit, NULL, NULL
};

struct TMenuItem settings_item = {
	&config_icon, "Settings", NULL, NULL, NULL, &ExecSettings, NULL, NULL
};

struct TMenuItem usb_games_item  = {
	&usb_icon, "USB Games", NULL, NULL, NULL, &ExecUSBGameSelection, &NextUSBGame, &PrevUSBGame, &ResetUSBOrder, &RefreshUSBGameList
};

struct TMenuItem hdd_games_item = {
	&games_icon, "HDD Games", NULL, NULL, NULL
};

struct TMenuItem network_games_item = {
	&games_icon, "Network Games", NULL, NULL, NULL
};

struct TMenuItem apps_item = {
	&apps_icon, "Apps", NULL, NULL, NULL
};

void InitMenuItems() {
	ClearUSBSubMenu();
	
	// initialize the menu
	AppendSubMenu(&settings_submenu, &theme_icon, "Theme", 1);
	AppendSubMenu(&settings_submenu, &language_icon, "Language", 2);
	
	speed_item = AppendSubMenu(&settings_submenu, &scroll_icon, scroll_speed_txt[scroll_speed], 3);
	menutype_item = AppendSubMenu(&settings_submenu, &menu_icon, menu_type_txt[dynamic_menu ? 0 : 1], 4);
	
	settings_item.submenu = settings_submenu;
	
	// add all menu items
	menu = NULL;
	AppendMenuItem(&exit_item);
	AppendMenuItem(&settings_item);
	AppendMenuItem(&usb_games_item);
	AppendMenuItem(&hdd_games_item);
	// TODO: For example: AppendMenuItem(&network_games_item);
	AppendMenuItem(&apps_item);
}

// --------------------- Main --------------------
int main(void)
{
	Reset();
	InitGFX();
	
	SifLoadModule("rom0:SIO2MAN",0,0);
	SifLoadModule("rom0:MCMAN",0,0);
	SifLoadModule("rom0:PADMAN",0,0);
	
	SifInitRpc(0);
	
	int ret, id;
	
	id=SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, &ret);
	id=SifExecModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &ret);

	delay(3);
	
	InitMenu();
	
	/// Init custom menu items
	InitMenuItems();
	
	// first column is usb games
	MenuSetSelectedItem(&usb_games_item);
	
	StartPad();

	Intro();

	max_games=0;
	usbdelay=0;
	frame=0;
	TextColor(0x80,0x80,0x80,0x80);
	
	while(1)
	{
		ReadPad();

		DrawScreen();
				
		if(GetKey(KEY_LEFT)){
			MenuPrevH();
		}else if(GetKey(KEY_RIGHT)){
			MenuNextH();
		}else if(GetKey(KEY_UP)){
			MenuPrevV();
		}else if(GetKey(KEY_DOWN)){
			MenuNextV();
		}else if(GetKey(KEY_CROSS)){
			// handle via callback in the menuitem
			MenuItemExecute();
		}

		Flip();
		
		RefreshSubMenu();
	}

	return 0;
}

