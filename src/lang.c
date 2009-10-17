#include "include/usbld.h"
#include "include/lang.h"

// Language support...

char *english[] = {
	"               WELCOME TO OPEN USB LOADER. (C) 2009 IFCARO <http://ps2dev.ifcaro.net>. BASED ON SOURCE CODE OF HD PROJECT <http://psx-scene.com>", // gfx.c:85
	"Open USB Loader %s", // gfx.c:320
	"Theme configuration", // gfx.c:502
	"Save changes", // gfx.c:586
	"O Back", // gfx.c:736
	"Scroll Slow", // main.c:21 
	"Scroll Medium", 
	"Scroll Fast",
	"Dynamic Menu",  // main.c:22
	"Static Menu",
	"Configure theme",  // main.c:23
	"Select language",  
	"No Items", // main.c:26
	"Not available yet", // main.c:158
	"Settings saved...", //  main.c:166
	"Error writing settings!", //  main.c:168
	"Exit", // Menu item strings
	"Settings",
	"USB Games",
	"HDD Games",
	"Network Games",
	"Apps",
	"Theme", // Submenu items
	"Language",
	"Language: English"
};

char *czech[] = {
	"               OPEN USB LOADER. (C) 2009 IFCARO <http://ps2dev.ifcaro.net>. ZALOZENO NA ZDROJ. KODECH PROJEKTU HD PROJECT <http://psx-scene.com>", // gfx.c:85
	"Open USB Loader %s", // gfx.c:320
	"Nastaveni vzhledu", // gfx.c:502
	"Ulozit zmeny", // gfx.c:586
	"O Zpet", // gfx.c:736
	"Posuv Pomaly", // main.c:21 
	"Posuv Stredni", 
	"Posuv Rychly",
	"Dynamicke Menu",  // main.c:22
	"Staticke Menu",
	"Nastaveni vzhledu",  // main.c:23
	"Volba jazyka",
	"Zadne Polozky", // main.c:26
	"Neni doposud k dispozici", // main.c:158
	"Nastaveni ulozeno...", //  main.c:166
	"Chyba Pri ukladani zmen!", //  main.c:168
	"Konec", // Menu item strings
	"Nastaveni",
	"USB Hry",
	"HDD Hry",
	"Sitove Hry",
	"Aplikace",
	"Vzhled", // Submenu items
	"Jazyk",
	"Language: Czech"
};

char **languages[] = {
	english, // ID 0
	czech    // ID 1
};

const char *english_name = "English";

char **lang_strs = english;

// localised string getter
char *_l(unsigned int id) {
	return lang_strs[id];
}

// language setter (uses LANG_ID as described in lang.h)
void setLanguage(int langID) {
	if (langID < 0)
		langID = 0;

	if (langID > _LANG_ID_MAX)
		langID = _LANG_ID_MAX;	
		
	lang_strs = languages[langID];
}
