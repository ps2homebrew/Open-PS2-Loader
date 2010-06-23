#ifndef __LANG_H
#define __LANG_H

// list of localized string ids
#define _STR_LOCALE					 0 // Localized language name
#define _STR_WELCOME					 1
#define _STR_OUL_VER					 2
#define _STR_THEME_CONFIG				 3
#define _STR_IP_CONFIG					 4
#define _STR_SAVE_CHANGES				 5
#define _STR_O_BACK					 6
#define _STR_SCROLL_SLOW				 7
#define _STR_SCROLL_MEDIUM				 8
#define _STR_SCROLL_FAST				 9
#define _STR_MENU_DYNAMIC				10
#define _STR_MENU_STATIC				11
#define _STR_CONFIG_THEME				12
#define _STR_IPCONFIG					13
#define _STR_SELECT_LANG				14
#define _STR_NO_ITEMS					15
#define _STR_NOT_AVAILABLE				16
#define _STR_SETTINGS_SAVED				17
#define _STR_ERROR_SAVING_SETTINGS			18
#define _STR_EXIT					19
#define _STR_SETTINGS					20
#define _STR_USB_GAMES					21
#define _STR_HDD_GAMES					22
#define _STR_NET_GAMES					23
#define _STR_APPS					24
#define _STR_THEME					25
#define _STR_LANGUAGE					26 // "Language" word
#define _STR_LANG_NAME					27 // Menu item text directly...
#define _STR_START_NETWORK				28
#define _STR_NETWORK_LOADING				29
#define _STR_NETWORK_STARTUP_ERROR			30
#define _STR_NETWORK_AUTOSTART				31
#define _STR_ON						32
#define _STR_OFF					33
#define _STR_OK						34
#define _STR_COMPAT_SETTINGS				35
#define _STR_REMOVE_ALL_SETTINGS			36
#define _STR_REMOVED_ALL_SETTINGS			37
#define _STR_SCROLLING					38
#define _STR_MENUTYPE					39
#define _STR_SLOW					40
#define _STR_MEDIUM					41
#define _STR_FAST					42
#define _STR_STATIC					43
#define _STR_DYNAMIC					44
#define _STR_DEFDEVICE					45
#define _STR_LOAD_FROM_DISC				46
#define _STR_PLEASE_WAIT				47
#define _STR_ERROR_LOADING_ID				48
#define _STR_USEHDD					49
#define _STR_AUTOSTARTHDD				50
#define _STR_STARTHDD					51
#define _STR_AUTOSORT					52
#define _STR_ERR_LOADING_LANGFILE			53
#define _STR_DEBUG					54
#define _STR_NO_CONTROLLER				55
#define _STR_COVERART					56
#define _STR_WIDE_SCREEN				57
#define _STR_POWEROFF					58
#define _STR_LOADING_SETTINGS				59
#define _STR_SAVING_SETTINGS				60
#define _STR_START_DEVICE				61
#define _STR_USBMODE					62
#define _STR_HDDMODE					63
#define _STR_ETHMODE					64
#define _STR_APPMODE					65
#define _STR_AUTO					66
#define _STR_MANUAL					67
#define _STR_STARTHDL					68
#define LANG_STR_COUNT 69

// Maximum external languages supported
#define MAX_LANGUAGE_FILES 15

// getter for a localised string version
extern char *_l(unsigned int id);

typedef struct {
	char* filePath;
	char* name;
} language_t;

void lngInit();
char* lngGetValue();
void lngEnd();

// Indices are shifted in GUI, as we add the internal english language at 0
void lngSetGuiValue(int langGuiId);
int lngGetGuiValue();
int lngFindGuiID(char* lang);
char **lngGetGuiList();

#endif
