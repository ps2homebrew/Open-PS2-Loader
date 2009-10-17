#ifndef __LANG_H
#define __LANG_H

// list of localised string ids
#define _STR_WELCOME 0
#define _STR_OUL_VER 1
#define _STR_THEME_CONFIG 2
#define _STR_SAVE_CHANGES 3
#define _STR_O_BACK 4
#define _STR_SCROLL_SLOW 5
#define _STR_SCROLL_MEDIUM 6
#define _STR_SCROLL_FAST 7
#define _STR_MENU_DYNAMIC 8
#define _STR_MENU_STATIC 9
#define _STR_CONFIG_THEME 10
#define _STR_SELECT_LANG 11
#define _STR_NO_ITEMS 12
#define _STR_NOT_AVAILABLE 13
#define _STR_SETTINGS_SAVED 14
#define _STR_ERROR_SAVING_SETTINGS 15
#define _STR_EXIT 16
#define _STR_SETTINGS 17
#define _STR_USB_GAMES 18
#define _STR_HDD_GAMES 19
#define _STR_NET_GAMES 20
#define _STR_APPS 21
#define _STR_THEME 22
#define _STR_LANGUAGE 23
// Menu item text directly...
#define _STR_LANG_NAME 24

// Language ID's
#define _LANG_ID_ENGLISH 0
#define _LANG_ID_CZECH 1
// set to the maximal ID
#define _LANG_ID_MAX 1

// getter for a localised string version
extern char *_l(unsigned int id);

// language setter (see _LANG_ID constants)
void setLanguage(int langID);


#endif
