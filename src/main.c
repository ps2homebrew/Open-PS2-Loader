#include "include/usbld.h"

extern void *usbd_irx;
extern int size_usbd_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

void LoadGameList(){
	int fd, size, n;
	TGameList *list;
	TGame buffer;

	if(usbdelay>100){
	fd=fioOpen("mass:ul.cfg", O_RDONLY);
	if(fd>=0){
		if (max_games==0){
			size = fioLseek(fd, 0, SEEK_END);  
			fioLseek(fd, 0, SEEK_SET);  	
			for(n=0;n<size;n+=0x40){
				fioRead(fd, &buffer, 0x40);
				list=(TGameList*)malloc(sizeof(TGameList));
				memcpy(&list->Game,&buffer,sizeof(buffer));
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
			}
			actualgame=firstgame;
		}
	}else{
		if(max_games>0){
			actualgame=firstgame;
			while(actualgame->next!=0){
				actualgame=actualgame->next;
				free(actualgame->prev);
			}
			free(actualgame);
		}
		max_games=0;
	}
	fioClose(fd);
	usbdelay=0;
	}else{
		usbdelay++;
	}
}

void PrevGame(){
	if(actualgame->prev!=NULL){
		actualgame=actualgame->prev;
	}
}

void NextGame(){
	if(actualgame->next!=NULL){
		actualgame=actualgame->next;
	}
}

int main(void)
{
	int ret, id;
	init_scr();
	scr_printf("\tOPEN USB LOADER Iniciado...\n\n");

	scr_printf("\tReset...\n");
	Reset();

	//InitGFX();

	ret=SifLoadModule("rom0:SIO2MAN",0,0);
	scr_printf("\trom0:SIO2MAN id=%d\n", ret);
	ret=SifLoadModule("rom0:MCMAN",0,0);
	scr_printf("\trom0:MCMAN id=%d\n", ret);
	ret=SifLoadModule("rom0:PADMAN",0,0);
	scr_printf("\trom0:PADMAN id=%d\n", ret);
	
	scr_printf("\tSifInitRpc...\n");
	SifInitRpc(0);
	
	id=SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, &ret);
	scr_printf("\tUSBD.IRX id=%d ret=%d\n", id, ret);
	delay(3);
	id=SifExecModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &ret);
	scr_printf("\tUSBHDFSD id=%d ret=%d\n", id,  ret);
	
	scr_printf("\tStartPad...\n");
	StartPad();

	delay(20);

InitGFX();
	
	Intro();

	max_games=0;
	usbdelay=0;
	frame=0;
	TextColor(0x80,0x80,0x80,0x80);
	
	while(1)
	{
		ReadPad();
				
		DrawBackground();
			
		DrawIcons();
		
		DrawInfo();
				
		if(GetKey(KEY_LEFT)){
			if(selected_h>0){
				h_anim=0;
				selected_h--;
				selected_v=0;
				actualgame=firstgame;
				v_anim=100;
				direc=1;
			}
		}else if(GetKey(KEY_RIGHT)){
			if(selected_h<4){
				h_anim=200;
				selected_h++;
				selected_v=0;
				actualgame=firstgame;
				v_anim=100;
				direc=3;
			}
		}else if(GetKey(KEY_UP)){
			if(selected_v>0){
				v_anim=0;
				selected_v--;
				direc=4;
				if(selected_h==2){
					PrevGame();
				}
			}
		}else if(GetKey(KEY_DOWN)){
			if(selected_v<max_v-1){
				v_anim=200;
				selected_v++;
				direc=2;
				if(selected_h==2){
					NextGame();
				}
			}
		}else if(GetKey(KEY_CROSS)){
			if(selected_h==0){
					__asm__ __volatile__(
					"	li $3, 0x04;"
					"	syscall;"
					"	nop;"
				);
			}else if(selected_h==1){
				if(selected_v==0){
					DrawConfig();
				}else{
					MsgBox();
				}
			}else if(selected_h==2 && max_games>0){
				padPortClose(0, 0);
				padPortClose(1, 0);
				padReset();
				LaunchGame(&actualgame->Game);
			}
		}
		Flip();
		if(selected_h==2){
			LoadGameList();
		}
	}

	return 0;
}

