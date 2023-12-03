#include "include/opl.h"
#include "include/lang.h"
#include "include/util.h"
#include "include/fntsys.h"
#include "include/ioman.h"
#include "include/themes.h"
#include "include/sound.h"

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

static void lngFreeFromFile(char **lang_strs)
{
    if (guiLangID == 0)
        return;

    int strId = 0;
    for (; strId < nValidEntries; strId++) {
        free(lang_strs[strId]);
        lang_strs[strId] = NULL;
    }
    free(lang_strs);
}

static int lngLoadFont(const char *dir, const char *name)
{
    char path[256];

    snprintf(path, sizeof(path), "%sfont_%s.ttf", dir, name);
    LOG("LANG Custom TTF font path: %s\n", path);
    if (fntLoadDefault(path) == 0)
        return 0;

    snprintf(path, sizeof(path), "%sfont_%s.otf", dir, name);
    LOG("LANG Custom OTF font path: %s\n", path);
    if (fntLoadDefault(path) == 0)
        return 0;

    LOG("LANG cannot load font.\n");

    return -1;
}

static int lngLoadFromFile(char *path, char *name)
{
    char dir[128];

    file_buffer_t *fileBuffer = openFileBuffer(path, O_RDONLY, 1, 1024);
    if (fileBuffer) {
        // file exists, try to read it and load the custom lang
        char **curL = lang_strs;
        char **newL = (char **)calloc(LANG_STR_COUNT, sizeof(char *));

        int strId = 0;
        while (strId < LANG_STR_COUNT && readFileBuffer(fileBuffer, &newL[strId])) {
            strId++;
        }
        closeFileBuffer(fileBuffer);

        LOG("LANG Loaded %d entries\n", strId);

        int newEntries = strId;
        // if necessary complete lang with default internal
        while (strId < LANG_STR_COUNT) {
            LOG("LANG Default entry added: %s\n", internalEnglish[strId]);
            newL[strId] = internalEnglish[strId];
            strId++;
        }
        lang_strs = newL;
        lngFreeFromFile(curL);

        // remember how many entries were read from the file (for the free later)
        nValidEntries = newEntries;

        int len = strlen(path) - strlen(name) - 9; // -4 for extension,  -5 for prefix
        memcpy(dir, path, len);
        dir[len] = '\0';

        lngLoadFont(dir, name);

        return 1;
    }
    return 0;
}

char *lngGetValue(void)
{
    return guiLangNames[guiLangID];
}

static int lngReadEntry(int index, const char *path, const char *separator, const char *name, unsigned char d_type)
{
    if (d_type != DT_DIR) {
        if (strstr(name, ".lng") || strstr(name, ".LNG")) {

            language_t *currLang = &languages[nLanguages + index];

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

static void lngRebuildLangNames(void)
{
    if (guiLangNames)
        free(guiLangNames);

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

int lngAddLanguages(char *path, const char *separator, int mode)
{
    int result;

    result = listDir(path, separator, MAX_LANGUAGE_FILES - nLanguages, &lngReadEntry);
    nLanguages += result;
    lngRebuildLangNames();

    const char *temp;
    if (configGetStr(configGetByType(CONFIG_OPL), "language_text", &temp)) {
        if (lngSetGuiValue(lngFindGuiID(temp)))
            moduleUpdateMenu(mode, 0, 1);
    }

    return result;
}

void lngInit(void)
{
    fntInit();

    lngAddLanguages(gBaseMCDir, "/", -1);
}

void lngEnd(void)
{
    lngFreeFromFile(lang_strs);

    int i = 0;
    for (; i < nLanguages; i++) {
        free(languages[i].name);
        free(languages[i].filePath);
    }

    free(guiLangNames);

    fntEnd();
}

int lngSetGuiValue(int langID)
{
    if (langID != -1) {
        if (guiLangID != langID) {
            bgmMute();
            if (langID != 0) {
                language_t *currLang = &languages[langID - 1];
                if (lngLoadFromFile(currLang->filePath, currLang->name)) {
                    guiLangID = langID;
                    thmSetGuiValue(thmGetGuiValue(), 1);
                    bgmUnMute();
                    return 1;
                }
            }
            lang_strs = internalEnglish;
            guiLangID = 0;
            // lang switched back to internalEnglish, reload default font
            fntLoadDefault(NULL);
            thmSetGuiValue(thmGetGuiValue(), 1);
            bgmUnMute();
        }
    }
    return 0;
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
            if (strcasecmp(languages[i].name, lang) == 0)
                return i + 1; // shift for Gui id
        }
    }
    return 0;
}

char **lngGetGuiList(void)
{
    return guiLangNames;
}

char *lngGetFilePath(int langID)
{
    language_t *currLang = &languages[langID - 1];
    char *path = currLang->filePath;

    return path;
}
