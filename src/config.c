#include "include/usbld.h"
#include "string.h"
#include "fileio.h"
#include <sys/stat.h>

int filesize=0;

char* substr(char* cadena, int comienzo, int longitud)
{
	if (longitud == 0) return 0;
	
	char *nuevo = (char*)malloc(sizeof(char) * longitud);
	
	strncpy(nuevo, cadena + comienzo, longitud);
	
	nuevo[longitud]=0;
		
	return nuevo;
}

char* readline(int fd){
	char *buffer=malloc(sizeof(char)*filesize);
	memset(buffer,0,sizeof(buffer));
	char *ch="";
	int res;
	do{
		ch[0]=0;
		res=fioRead(fd, ch, 1);
		if(ch[0]!=10 && ch[0]!=13){
			strcat(buffer, ch);
		}
		if (res==0)return buffer;
	}while(ch[0]!=13);
	return buffer;
}

char *limpiarlinea(char *linea){
	int x=0;
	for(x=0;x<strlen(linea);x++){
		if (linea[x]!=9 && linea[0]!=32)break;
	}
	//char *res=malloc(sizeof(linea));
	//res= substr(linea, x, strlen(linea)-x+1);
	//printf("%s\n",res);

	return substr(linea, x, strlen(linea)-x+1);
}

int read_bg_color(const char *string)
{
	int cnt=0, n=0;
	bg_color[0]=0;
	bg_color[1]=0;
	bg_color[2]=0;

	if (!string || !*string) return 0;
	if (string[0]!='#') return 0;
	
	string++;

	while (*string){
		if ((*string >= '0') && (*string <= '9')){
			bg_color[n] = bg_color[n] * 16 + (*string - '0');
		}else if ( (*string >= 'A') && (*string <= 'F') ){
			bg_color[n] = bg_color[n] * 16 + (*string - 'A'+10);
		}else{
			break;
		}

		if(cnt==1){
			cnt=0;
			n++;
		}else{
			cnt++;
		}
		
		string++;
	}
	
	SetColor(bg_color[0],bg_color[1],bg_color[2]);

	return  0;

}

int read_text_color(const char *string)
{
	int cnt=0, n=0;
	text_color[0]=0;
	text_color[1]=0;
	text_color[2]=0;

	if (!string || !*string) return 0;
	if (string[0]!='#') return 0;
	
	string++;

	while (*string){
		if ((*string >= '0') && (*string <= '9')){
			text_color[n] = text_color[n] * 16 + (*string - '0');
		}else if ( (*string >= 'A') && (*string <= 'F') ){
			text_color[n] = text_color[n] * 16 + (*string - 'A'+10);
		}else{
			break;
		}

		if(cnt==1){
			cnt=0;
			n++;
		}else{
			cnt++;
		}
		
		string++;
	}
	
	TextColor(text_color[0],text_color[1],text_color[2],0xff);

	return  0;

}

int ReadConfig(char *archivo){

	int fd=fioOpen(archivo, O_RDONLY);
	if (fd<=0) {
		printf("No config. Exiting...\n");
		return 0;
	}
	filesize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	char *linea=readline(fd);
	if(!strncmp(linea,"<config>",8)){
		do{
			char *linea=readline(fd);
			if(linea[0]==0)break;
			linea=limpiarlinea(linea);
			if(!strncmp(linea,"<bgcolor>",9)){
				read_bg_color(substr(linea,9,strlen(linea)-19));
			}else if(!strncmp(linea,"<txtcolor>",10)){
				read_text_color(substr(linea,10,strlen(linea)-21));
			}else if(!strncmp(linea,"<theme>",7)){
				if(strcmp(theme,"<none>")){
					sprintf(theme, "%s",substr(linea,7,strlen(linea)-15));
				}
			}
		}while(strncmp(linea,"</config>",9));

	}
	fioClose(fd);
	return 1;
}

int SaveConfig(char *archivo){
	char config[2048];
	
	if(!strcmp(theme,"<none>")){
		theme[0]=0;
	}

	int fd=fioOpen(archivo, O_WRONLY | O_CREAT);
	if (fd>0) {
		sprintf(config,"<config>\r\n"
					   "\t<bgcolor>#%02X%02X%02X</bgcolor>\r\n"
					   "\t<txtcolor>#%02X%02X%02X</txtcolor>\r\n"
					   "\t<theme>%s</theme>\r\n"
					   "</config>",bg_color[0],bg_color[1],bg_color[2],
					   text_color[0],text_color[1],text_color[2],
					   theme);
		fioWrite(fd, config, strlen(config)+1);
	}
	fioClose(fd);
	
	return 1;
}

void ListDir(char* directory) {
   int ret=-1;
   int n_dir=0;
   int n_reg=0;
   fio_dirent_t record;
   int i=0;
   

   if ((ret = fioDopen(directory)) < 0) {
      //printf("Error opening dir\n");
      return;
   }

   i=0;
   n_dir=0;
   n_reg=0;
   
   theme_dir[0]=malloc(7);
   sprintf(theme_dir[0],"<none>");

   while (fioDread(ret, &record) > 0) {

      /*Keep track of number of files */
      if (FIO_SO_ISDIR(record.stat.mode)) {
		if(record.name[0]!='.' && (record.name[0]!='.' && record.name[1]!='.')){
			//printf("%s\n",record.name);
			theme_dir[n_dir-1]=malloc(sizeof(record.name));
			sprintf(theme_dir[n_dir-1],"%s",record.name);
		}
         n_dir++;
      }

      if (FIO_SO_ISREG(record.stat.mode)) {
         n_reg++;
      }
      i++;
	  
	  max_theme_dir=n_dir-1;

   }
   if (ret >= 0) fioDclose(ret);
   free(&record);

} 
