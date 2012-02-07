#ifndef __LANG_H
#define __LANG_H

// list of localized string ids
#define _STR_LOCALE						  0 // Localized language name
#define _STR_WELCOME					  1
#define _STR_OUL_VER					  2
#define _STR_SAVE_CHANGES				  3
#define _STR_O_BACK						  4
#define _STR_IPCONFIG					  5
#define _STR_NO_ITEMS					  6
#define _STR_SETTINGS_SAVED				  7
#define _STR_ERROR_SAVING_SETTINGS		  8
#define _STR_EXIT						  9
#define _STR_SETTINGS					 10
#define _STR_USB_GAMES					 11
#define _STR_HDD_GAMES					 12
#define _STR_NET_GAMES					 13
#define _STR_APPS						 14
#define _STR_THEME						 15
#define _STR_LANGUAGE					 16
#define _STR_NETWORK_STARTUP_ERROR		 17
#define _STR_ON							 18
#define _STR_OFF						 19
#define _STR_OK							 20
#define _STR_COMPAT_SETTINGS			 21
#define _STR_REMOVE_ALL_SETTINGS		 22
#define _STR_REMOVED_ALL_SETTINGS		 23
#define _STR_SCROLLING					 24
#define _STR_SLOW						 25
#define _STR_MEDIUM						 26
#define _STR_FAST						 27
#define _STR_DEFDEVICE					 28
#define _STR_LOAD_FROM_DISC				 29
#define _STR_PLEASE_WAIT				 30
#define _STR_ERROR_LOADING_ID			 31
#define _STR_AUTOSORT					 32
#define _STR_ERR_LOADING_LANGFILE		 33
#define _STR_DEBUG						 34
#define _STR_NO_CONTROLLER				 35
#define _STR_COVERART					 36
#define _STR_WIDE_SCREEN				 37
#define _STR_POWEROFF					 38
#define _STR_LOADING_SETTINGS			 39
#define _STR_SAVING_SETTINGS			 40
#define _STR_START_DEVICE				 41
#define _STR_USBMODE					 42
#define _STR_HDDMODE					 43
#define _STR_ETHMODE					 44
#define _STR_APPMODE					 45
#define _STR_AUTO						 46
#define _STR_MANUAL						 47
#define _STR_STARTHDL					 48
#define _STR_STARTINGHDL				 49
#define _STR_RUNNINGHDL					 50
#define _STR_STOPHDL					 51
#define _STR_UNLOADHDL					 52
#define _STR_EXITTO						 53
#define _STR_BGCOLOR					 54
#define _STR_TXTCOLOR					 55
#define _STR_IP							 56
#define _STR_MASK						 57
#define _STR_GATEWAY					 58
#define _STR_PORT						 59
#define _STR_SHARE						 60
#define _STR_USER						 61
#define _STR_PASSWORD					 62
#define _STR_NOT_SET					 63
#define _STR_X_ACCEPT					 64
#define _STR_DELETE_WARNING				 65
#define _STR_RENAME						 66
#define _STR_DELETE						 67
#define _STR_RUN						 68
#define _STR_GFX_SETTINGS				 69
#define _STR_DANDROP					 70
#define _STR_CHECKUSBFRAG				 71
#define _STR_LASTPLAYED					 72
#define _STR_ERR_FRAGMENTED				 73
#define _STR_ERR_FILE_INVALID			 74
#define _STR_TEST						 75
#define _STR_HINT_GUEST					 76
#define _STR_HINT_MODE1					 77
#define _STR_HINT_MODE2					 78
#define _STR_HINT_MODE3					 79
#define _STR_HINT_MODE4					 80
#define _STR_HINT_MODE5					 81
#define _STR_HINT_MODE6					 82
#define _STR_HINT_MODE7					 83
#define _STR_HINT_MODE8					 84
#define _STR_HINT_MODE9					 85
#define _STR_HINT_VMC_SIZE				 86
#define _STR_CREATE						 87
#define _STR_MODIFY						 88
#define _STR_ABORT						 89
#define _STR_RESET						 90
#define _STR_USE_GENERIC				 91
#define _STR_VMC_SCREEN					 92
#define _STR_VMC_NAME					 93
#define _STR_VMC_SIZE					 94
#define _STR_VMC_STATUS					 95
#define _STR_VMC_PROGRESS				 96
#define _STR_VMC_FILE_EXISTS			 97
#define _STR_VMC_FILE_ERROR				 98
#define _STR_VMC_FILE_NEW				 99
#define _STR_ERR_VMC_CONTINUE			100
#define _STR_AUTOREFRESH				101
#define _STR_ABOUT						102
#define _STR_DEVS						103
#define _STR_USB_DELAY					104
#define _STR_USB_PREFIX					105
#define _STR_HINT_EXITPATH				106
#define _STR_HINT_SPINDOWN				107
#define _STR_HDD_SPINDOWN				108
#define _STR_VMODE						109
#define _STR_UICOLOR					110
#define _STR_SELCOLOR					111
#define _STR_USE_INFO_SCREEN			112
#define _STR_INFO						113
#define _STR_ALTSTARTUP					114
#define _STR_COLOR_SELECTION			115
#define _STR_RECONNECT					116

#define LANG_STR_COUNT 117

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
