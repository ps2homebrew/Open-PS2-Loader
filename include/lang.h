#ifndef __LANG_H
#define __LANG_H

// list of localised string ids
#define _STR_WELCOME 0
#define _STR_OUL_VER 1
#define _STR_THEME_CONFIG 2
#define _STR_IP_CONFIG 3
#define _STR_SAVE_CHANGES 4
#define _STR_O_BACK 5
#define _STR_SCROLL_SLOW 6
#define _STR_SCROLL_MEDIUM 7
#define _STR_SCROLL_FAST 8
#define _STR_MENU_DYNAMIC 9
#define _STR_MENU_STATIC 10
#define _STR_CONFIG_THEME 11
#define _STR_IPCONFIG 12
#define _STR_SELECT_LANG 13
#define _STR_NO_ITEMS 14
#define _STR_NOT_AVAILABLE 15
#define _STR_SETTINGS_SAVED 16
#define _STR_ERROR_SAVING_SETTINGS 17
#define _STR_EXIT 18
#define _STR_SETTINGS 19
#define _STR_USB_GAMES 20
#define _STR_HDD_GAMES 21
#define _STR_NET_GAMES 22
#define _STR_APPS 23
#define _STR_THEME 24
#define _STR_LANGUAGE 25
// Menu item text directly...
#define _STR_LANG_NAME 26
#define _STR_START_NETWORK 27
#define _STR_NETWORK_LOADING 28
#define _STR_NETWORK_STARTUP_ERROR 29
#define _STR_NETWORK_AUTOSTART 30
#define _STR_ON 31
#define _STR_OFF 32
#define _STR_OK 33
#define _STR_COMPAT_SETTINGS 34
#define _STR_REMOVE_ALL_SETTINGS 35
#define _STR_REMOVED_ALL_SETTINGS 36

// Language ID's
#define _LANG_ID_ENGLISH 0
#define _LANG_ID_CZECH 1
#define _LANG_ID_SPANISH 2
// set to the maximal ID
#define _LANG_ID_MAX 2

// getter for a localised string version
extern char *_l(unsigned int id);

// language setter (see _LANG_ID constants)
void setLanguage(int langID);


#endif
