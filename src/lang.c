#include "include/usbld.h"
#include "include/lang.h"
#include "include/util.h"
#include "include/fntsys.h"
#include "include/ioman.h"

// Language support
static char *internalEnglish[LANG_STR_COUNT] = {
	"English (internal)",
	"               WELCOME TO OPEN PS2 LOADER. MAIN CODE BASED ON SOURCE CODE OF HD PROJECT <http://psx-scene.com> ADAPTATION TO USB ADVANCE FORMAT AND INITIAL GUI BY IFCARO <http://ps2dev.ifcaro.net> MOST OF LOADER CORE IS MADE BY JIMMIKAELKAEL. ALL THE GUI IMPROVEMENTS ARE MADE BY VOLCA. THANKS FOR USING OUR PROGRAM ^^",
	"Open PS2 Loader %s",
	"Theme configuration",
	"IP configuration",
	"Save changes",
	"O Back",
	"Scroll Slow",
	"Scroll Medium", 
	"Scroll Fast",
	"Dynamic Menu",
	"Static Menu",
	"Configure theme",
	"Network config",
	"Select language",  
	"No Items",
	"Not available yet",
	"Settings saved...",
	"Error writing settings!",
	"Exit",
	"Settings",
	"USB Games",
	"HDD Games",
	"Network Games",
	"Apps",
	"Theme",
	"Language",
	"Language: English",
	"Start network",
	"Network loading: %d",
	"Network startup error",
	"Network startup automatic",
	"On",
	"Off",
	"Ok",
	"Compatibility settings",
	"Remove all settings",
	"Removed all keys for the game",
	"Scrolling", // _STR_SCROLLING
	"Menu type", // _STR_MENUTYPE
	"Slow", // _STR_SLOW
	"Medium", // _STR_MEDIUM
	"Fast", // _STR_FAST
	"Static", // _STR_STATIC
	"Dynamic", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE
	"Load from disc", // _STR_LOAD_FROM_DISC
	"Please wait", // _STR_PLEASE_WAIT
	"Error while loading the Game ID", // _STR_ERROR_LOADING_ID
	"Use Hard Drive", // _STR_USEHDD
	"Autostart Hard Drive", // _STR_AUTOSTARTHDD
	"Start Hard Drive", // _STR_STARTHDD
	"Automatic sorting", // _STR_AUTOSORT
	"Error loading the language file", // _STR_ERR_LOADING_LANGFILE
	"Disable Debug Colors", // _STR_DEBUG	
	"No controller detected, waiting...", // _STR_NO_CONTROLLER
	"Enable Cover Art", // _STR_COVERART
	"Wide screen", // _STR_WIDE_SCREEN
	"Power off", // _STR_POWEROFF
	"Loading config",
	"Saving config",
	"Start device",
	"USB device start mode",
	"HDD device start mode",
	"ETH device start mode",
	"Application start mode",
	"Auto",
	"Manual",
};

static int guiLangID = 0;
static char **lang_strs = internalEnglish;
static int nValidEntries = LANG_STR_COUNT;

static int nLanguages = 0;
static language_t languages[MAX_LANGUAGE_FILES];
static char **guiLangNames;

// localised string getter
char *_l(unsigned int id) {
	return lang_strs[id];
}

static void lngFreeFromFile() {
	if (guiLangID == 0)
		return;

	int strId = 0;
	for(; strId < nValidEntries; strId++) {
		free(lang_strs[strId]);
		lang_strs[strId] = NULL;
	}
	free(lang_strs);
	lang_strs = NULL;
}

static int lngLoadFromFile(char* path, char *name) {
	read_context_t* readContext = openReadContext(path, 1, 1024);
	if (readContext) {
		// file exists, try to read it and load the custom lang
		lang_strs = (char**) malloc(LANG_STR_COUNT * sizeof(char**));

		int strId = 0;
		while (strId < LANG_STR_COUNT && readLineContext(readContext, &lang_strs[strId])) {
			strId++;
		}
		closeReadContext(readContext);
		
		LOG("LANG: #### Loaded %d entries\n", strId);

		// remember how many entries were read from the file (for the free later)
		nValidEntries = strId;

		// if necessary complete lang with default internal
		while (strId < LANG_STR_COUNT) {
			LOG("LANG: #### Default entry added: %s\n", internalEnglish[strId]);
			lang_strs[strId] = internalEnglish[strId];
			strId++;
		}

		int size = -1;
		char path[255];
		snprintf(path, 255, "%s/font_%s.ttf", gBaseMCDir, name);
		
		LOG("#### Custom font path: %s\n", path);
		
		void* customFont = readFile(path, -1, &size);
		if (customFont)
			fntLoad(customFont, size, 1);
		else
			fntLoad(NULL, -1, 1);

		return 1;
	}
	return 0;
}

char* lngGetValue() {
	return guiLangNames[guiLangID];
}

static int lngReadEntry(int index, char* path, char* separator, char* name, unsigned int mode) {
	if (!FIO_SO_ISDIR(mode)) {
		if(strstr(name, ".lng") || strstr(name, ".LNG")) {

			language_t* currLang = &languages[index];

			// filepath for this language file
			int length = strlen(path) + 1 + strlen(name) + 1;
			currLang->filePath = (char*) malloc(length * sizeof(char));
			sprintf(currLang->filePath, "%s%s%s", path, separator, name);

			// extract name for this language (will be used for the English translation)
			length = strlen(name) - 5 - 4 + 1;
			currLang->name = (char*) malloc(length * sizeof(char));
			memcpy(currLang->name, name + 5, length);
			currLang->name[length - 1] = '\0';

			/*read_context_t* readContext = openReadContext(currLang->filePath, 1, 1024);
			if (readContext) {
				// read localized name of language from file
				if (readLineContext(readContext, &currLang->name))
					readLineContext(readContext, &currLang->fontName);
				closeReadContext(readContext);
			}*/

			index++;
		}
	}
	return index;
}

void lngInit() {
	fntInit();

	nLanguages = listDir(gBaseMCDir, "/", MAX_LANGUAGE_FILES, &lngReadEntry);

	// build the languages name list
	guiLangNames = (char**) malloc((nLanguages + 2) * sizeof(char**));

	// add default internal (english)
	guiLangNames[0] = internalEnglish[0];

	int i = 0;
	for (; i < nLanguages; i++) {
		guiLangNames[i + 1] =  languages[i].name;
	}

	guiLangNames[nLanguages + 1] = NULL;
}

void lngEnd() {
	lngFreeFromFile();

	int i = 0;
	for (; i < nLanguages; i++) {
		free(languages[i].name);
		free(languages[i].filePath);
	}

	free(guiLangNames);

	fntEnd();
}

void lngSetGuiValue(int langID) {
	if (guiLangID != langID) {

		lngFreeFromFile();

		if (langID != 0) {
			language_t* currLang = &languages[langID - 1];
			if (lngLoadFromFile(currLang->filePath, currLang->name)) {
				guiLangID = langID;
				return;
			}
		}

		lang_strs = internalEnglish;
		guiLangID = 0;
	}
}

int lngGetGuiValue() {
	return guiLangID;
}

int lngFindGuiID(char* lang) {
	if (lang) {
		int i = 0;
		for (; i < nLanguages; i++) {
			if (stricmp(languages[i].name, lang) == 0)
				return i + 1; // shift for Gui id
		}
	}
	return 0;
}

char **lngGetGuiList() {
	return guiLangNames;
}
