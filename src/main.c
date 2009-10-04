#include "include/usbld.h"

extern void *usbd_irx;
extern int size_usbd_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

// Submenu items variant of the game list... keeped in sync with the game item list (firstgame, actualgame)
struct TSubMenuList* usb_submenu= NULL;
struct TMenuItem usb_games_item;

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

	if(usbdelay>100) {
		usbdelay = 0;
		fd=fioOpen("mass:ul.cfg", O_RDONLY);
		if(fd>=0) {
			if (max_games==0) {
				DestroySubMenu(&usb_submenu);
				usb_submenu = NULL;
				usb_games_item.submenu = usb_submenu;
				usb_games_item.current = usb_submenu;

				size = fioLseek(fd, 0, SEEK_END);  
				fioLseek(fd, 0, SEEK_SET); 
				
				// TODO: decide if reload is necessary or not based on the size or date of the file...
				
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
		} else {
			if(max_games>0){
				actualgame=firstgame;
				while(actualgame->next!=0){
					actualgame=actualgame->next;
					free(actualgame->prev);
				}
				free(actualgame);
			}
			max_games=0;
			
			ClearUSBSubMenu();
		}
		fioClose(fd);
	} else {
		usbdelay++;
	}
}

// --------------------- Menu item callbacks --------------------
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

// --------------------- Menu initialization --------------------
struct TMenuItem usb_games_item  = {
	&games_icon, "USB Games", NULL, NULL, NULL, &ExecUSBGameSelection, &NextUSBGame, &PrevUSBGame, &ResetUSBOrder, &RefreshUSBGameList
};

struct TMenuItem hdd_games_item = {
	&games_icon, "HDD Games", NULL, NULL, NULL
};

struct TMenuItem network_games_item = {
	&games_icon, "Network Games", NULL, NULL, NULL
};

struct TMenuItem apps_item = {
	&games_icon, "Apps", NULL, NULL, NULL
};

void InitMenuItems() {
	ClearUSBSubMenu();
	
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
	
	StartPad();

	Intro();

	max_games=0;
	usbdelay=0;
	frame=0;
	TextColor(0x80,0x80,0x80,0x80);
	
	// first column is games
	MenuNextH();
	MenuNextH();
	
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
		} else if(GetKey(KEY_SELECT)){
			SwapMenu();
		}

		Flip();
		
		RefreshSubMenu();
	}

	return 0;
}

