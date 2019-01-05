#include "include/opl.h"
#include "include/lang.h"
#include "include/util.h"
#include "include/fntsys.h"
#include "include/ioman.h"

#include "include/usbsupport.h"
#include "include/hddsupport.h"

#include "include/iosupport.h"

// Language support
static char *internalEnglish[LANG_STR_COUNT] = {
    "English (internal)",
    "Open PS2 Loader %s",
    "Save changes",
    "Back",
    "Network config",
    "Advanced options",
    "<no values>",
    "Settings saved...",
    "Error writing settings!",
    "Exit",
    "Settings",
    "Menu",
    "USB Games",
    "HDD Games",
    "ETH Games",
    "Apps",
    "Theme",
    "Language",
    "The system will be powered off.",
    "Exit to Browser?",
    "Cancel updating?",
    "%d: HardDisk Drive not detected",
    "%d: HardDisk Drive not formatted",
    "%d: Network startup error",
    "%d: No network adaptor detected",
    "%d: Cannot connect to SMB server",
    "%d: Cannot log into SMB server",
    "%d: Cannot open SMB share",
    "%d: Cannot list SMB shares",
    "%d: Cannot list games",
    "%d: DHCP server unavailable",
    "%d: No network connection",
    "On",
    "Off",
    "OK",
    "Select",
    "Cancel",
    "Circle",
    "Cross",
    "Games List",
    "Game Settings",
    "Remove all settings",
    "Removed all keys for the game",
    "Scrolling",
    "Slow",
    "Medium",
    "Fast",
    "Default menu",
    "Load from disc",
    "Please wait",
    "Error while loading the Game ID",
    "Automatic sorting",
    "Error loading the language file",
    "Disable Debug Colors",
    "No controller detected, waiting...",
    "Enable Cover Art",
    "Wide screen",
    "Power off",
    "Loading config",
    "Saving config",
    "Start device",
    "Refresh",
    "USB device start mode",
    "HDD device start mode",
    "ETH device start mode",
    "Applications start mode",
    "Auto",
    "Manual",
    "Start HDL Server",
    "HDL Server Starting...",
    "HDL Server Running...",
    "Failed to start HDL Server.",
    "HDL Server Unloading...",
    "IGR path",
    "Background color",
    "Text color",
    "- PS2 -",
    "- SMB Server -",
    "IP address type",
    "Static",
    "DHCP",
    "IP address",
    "Address",
    "Mask",
    "Gateway",
    "DNS Server",
    "Port",
    "Share",
    "User",
    "Password",
    "<not set>",
    "Address type",
    "IP",
    "NetBIOS",
    "Accept",
    "Item will be permanently deleted, continue?",
    "Rename",
    "Delete",
    "Run",
    "Display Settings",
    "Enable write operations",
    "Check USB game fragmentation",
    "Remember last played game",
    "Select button",
    "Error, the game is fragmented",
    "Error, could not run the item",
    "Test",
    "Leave empty for GUEST auth.",
    "Accurate Reads",
    "Synchronous Mode",
    "Unhook Syscalls",
    "Skip Videos",
    "Emulate DVD-DL",
    "Disable IGR",
    "Unused",
    "Hide DEV9 module",
    "Changing the size will reformat the VMC",
    "Create",
    "Start",
    "Modify",
    "Abort",
    "Reset",
    "Use generic",
    "Configure VMC",
    "Name",
    "Size",
    "Status",
    "Progress",
    "VMC file exist",
    "Invalid VMC file, size is incorrect",
    "VMC file need to be created",
    "Error accessing VMC %s. Continue with the Memory Card in slot %d?",
    "Automatic refresh",
    "About",
    "Coders",
    "Quality Assurance",
    "USB prefix path",
    "Boots custom ELF after an IGR",
    "Value in minute(s), 0 to disable spin down",
    "Automatic HDD spin down",
    "Video mode",
    "Dialog color",
    "Selected color",
    "Display info page",
    "Info",
    "Custom ELF",
    "Color selection",
    "Reconnect",
    "Leave empty to list shares",
    "ETH prefix path",
    "Backspace",
    "Space",
    "Enter",
    "Mode",
    "VMC Slot 1",
    "VMC Slot 2",
    "Game ID",
    "DMA Mode",
    "V-Sync",
    "Mode 1",
    "Mode 2",
    "Mode 3",
    "Mode 4",
    "Mode 5",
    "Mode 6",
    "Mode 7",
    "Mode 8",
    "Configure GSM",
    "Ethernet link mode",
    "100Mbit full-duplex",
    "100Mbit half-duplex",
    "10Mbit full-duplex",
    "10Mbit half-duplex",
    "GSM Settings",
    "Enable GSM",
    "Toggles GSM ON or OFF",
    "VMODE",
    "Forced Custom Display Mode",
    "H-POS",
    "Horizontal Adjustment",
    "V-POS",
    "Vertical Adjustment",
    "Overscan",
    "Overscan Adjustment",
    "FMV Skip",
    "Skips Full Motion Videos",
    "Cheat Settings",
    "Enable PS2RD Cheat Engine",
    "Lets PS2RD Cheat Engine patch your game",
    "PS2RD Cheat Engine Mode",
    "Auto-select or Select game cheats",
    "Auto-select cheats",
    "Select game cheats",
    "Error: failed to load Cheat File",
    "No cheats found",
    "Download defaults",
    "Network update",
    "Re-download existing records?",
    "Update failed.",
    "Failed to connect to update server.",
    "Update completed.",
    "Update cancelled.",
    "Download settings from the network?",
    "Customized Settings",
    "Downloaded Defaults",
    "Auto start in %i s...",
    "Auto start",
    "Value in second(s), 0 to disable auto start",
    "PS2 Logo",
    "(Only displayed for a valid disc logo which matches the console's region)",
    "Configure PADEMU",
    "Pad Emulator Settings",
    "Enable PadEmulator",
    "Turns on/off PadEmulator for selected game.",
    "Pad Emulator mode",
    "Select Pad Emulator mode.",
    "DualShock3/4 USB",
    "DualShock3/4 BT",
    "Settings for port:",
    "Select Pad Emulator port for settings.",
    "Enable emulation",
    "Turns on/off Pad Emulator for selected port.",
    "Enable vibration",
    "Turns on/off vibration for Pad Emulator selected port.",
    "Usb bluetooth adapter mac address:",
    "DS Controller paired to mac address:",
    "Pair",
    "Pair DS Controller",
    "Pair DS Controller with bluetooth adapter mac address.",
    "Not connected",
    "Bluetooth adapter information",
    "Shows more information and supported features",
    "HCI Version:",
    "LMP Version:",
    "Manufacturer ID:",
    "Support features:",
    "Yes",
    "No",
    "Bluetooth adapter should be fully compatible with DS3/DS4 controllers.",
    "Bluetooth adapter may not work correctly with DS3/DS4 controllers.",
    "Enable Multitap emulation",
    "Turns on/off Multitap emulation for selected game.",
    "Multitap emulator on port",
    "Select port for Multitap emulation.",
    "Disable workaround for fake ds3",
    "Some fake ds3s need workaround, this option will disable it.",
    "Emulate FIELD flipping",
    "Fix for games that glitch under progressive video modes.",
    "Parental Lock Settings",
    "Parental Lock Password",
    "Leave blank to disable the parental lock.",
    "Enter Parental Lock Password",
    "Parental lock password incorrect.",
    "Parental lock disabled.",
    "Build Options:",
    "Error - this password cannot be used.",
    "VMC %s file is fragmented. Continue with Memory Card in slot %d?",
    "Audio Settings",
    "Enable Sound Effects",
    "Enable Boot Sound",
    "Sound Effects Volume",
    "Boot Sound Volume",
    "Confirm video mode change?",
    "Cache Game List",
};

static int guiLangID = 0;
static char **lang_strs = internalEnglish;
static int nValidEntries = LANG_STR_COUNT;

static int nLanguages = 0;
static language_t languages[MAX_LANGUAGE_FILES];
static char **guiLangNames;

// localised string getter
char *_l(unsigned int id)
{
    return lang_strs[id];
}

static void lngFreeFromFile(void)
{
    if (guiLangID == 0)
        return;

    int strId = 0;
    for (; strId < nValidEntries; strId++) {
        free(lang_strs[strId]);
        lang_strs[strId] = NULL;
    }
    free(lang_strs);
    lang_strs = NULL;
}

static int lngLoadFont(const char *dir, const char *name)
{
    char path[256];

    snprintf(path, sizeof(path), "%s/font_%s.ttf", dir, name);
    LOG("LANG Custom TTF font path: %s\n", path);
    if (fntLoadDefault(path) == 0)
        return 0;

    snprintf(path, sizeof(path), "%s/font_%s.otf", dir, name);
    LOG("LANG Custom OTF font path: %s\n", path);
    if (fntLoadDefault(path) == 0)
        return 0;

    LOG("LANG cannot load font.\n");

    return -1;
}

static int lngLoadFromFile(char *path, char *name)
{
    int HddStartMode;
    file_buffer_t *fileBuffer = openFileBuffer(path, O_RDONLY, 1, 1024);
    if (fileBuffer) {
        // file exists, try to read it and load the custom lang
        lang_strs = (char **)malloc(LANG_STR_COUNT * sizeof(char **));

        int strId = 0;
        while (strId < LANG_STR_COUNT && readFileBuffer(fileBuffer, &lang_strs[strId])) {
            strId++;
        }
        closeFileBuffer(fileBuffer);

        LOG("LANG Loaded %d entries\n", strId);

        // remember how many entries were read from the file (for the free later)
        nValidEntries = strId;

        // if necessary complete lang with default internal
        while (strId < LANG_STR_COUNT) {
            LOG("LANG Default entry added: %s\n", internalEnglish[strId]);
            lang_strs[strId] = internalEnglish[strId];
            strId++;
        }

        if (lngLoadFont(gBaseMCDir, name) != 0) {
            if (lngLoadFont("mass0:", name) != 0) {
                if (configGetInt(configGetByType(CONFIG_OPL), CONFIG_OPL_HDD_MODE, &HddStartMode) && (HddStartMode == START_MODE_AUTO)) {
                    hddLoadModules();
                    lngLoadFont("pfs0:", name);
                }
            }
        }

        return 1;
    }
    return 0;
}

char *lngGetValue(void)
{
    return guiLangNames[guiLangID];
}

static int lngReadEntry(int index, const char *path, const char *separator, const char *name, unsigned int mode)
{
    if (!FIO_S_ISDIR(mode)) {
        if (strstr(name, ".lng") || strstr(name, ".LNG")) {

            language_t *currLang = &languages[index];

            // filepath for this language file
            int length = strlen(path) + 1 + strlen(name) + 1;
            currLang->filePath = (char *)malloc(length * sizeof(char));
            sprintf(currLang->filePath, "%s%s%s", path, separator, name);

            // extract name for this language (will be used for the English translation)
            length = strlen(name) - 5 - 4 + 1;
            currLang->name = (char *)malloc(length * sizeof(char));
            memcpy(currLang->name, name + 5, length);
            currLang->name[length - 1] = '\0';

            /*file_buffer_t* fileBuffer = openFileBuffer(currLang->filePath, 1, 1024);
			if (fileBuffer) {
				// read localized name of language from file
				if (readLineContext(fileBuffer, &currLang->name))
					readLineContext(fileBuffer, &currLang->fontName);
				closeFileBuffer(fileBuffer);
			}*/

            index++;
        }
    }
    return index;
}

void lngInit(void)
{
    fntInit();

    nLanguages = listDir(gBaseMCDir, "/", MAX_LANGUAGE_FILES, &lngReadEntry);

    // build the languages name list
    guiLangNames = (char **)malloc((nLanguages + 2) * sizeof(char **));

    // add default internal (english)
    guiLangNames[0] = internalEnglish[0];

    int i = 0;
    for (; i < nLanguages; i++) {
        guiLangNames[i + 1] = languages[i].name;
    }

    guiLangNames[nLanguages + 1] = NULL;
}

void lngEnd(void)
{
    lngFreeFromFile();

    int i = 0;
    for (; i < nLanguages; i++) {
        free(languages[i].name);
        free(languages[i].filePath);
    }

    free(guiLangNames);

    fntEnd();
}

void lngSetGuiValue(int langID)
{
    if (guiLangID != langID) {

        lngFreeFromFile();

        if (langID != 0) {
            language_t *currLang = &languages[langID - 1];
            if (lngLoadFromFile(currLang->filePath, currLang->name)) {
                guiLangID = langID;
                return;
            }
        }

        lang_strs = internalEnglish;
        guiLangID = 0;
    }
}

int lngGetGuiValue(void)
{
    return guiLangID;
}

int lngFindGuiID(const char *lang)
{
    if (lang) {
        int i = 0;
        for (; i < nLanguages; i++) {
            if (stricmp(languages[i].name, lang) == 0)
                return i + 1; // shift for Gui id
        }
    }
    return 0;
}

char **lngGetGuiList(void)
{
    return guiLangNames;
}
