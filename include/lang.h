#ifndef __LANG_H
#define __LANG_H

#include "lang_autogen.h"

// Maximum external languages supported
#define MAX_LANGUAGE_FILES 15

extern char *internalEnglish[LANG_STR_COUNT];
// getter for a localised string version
extern char *_l(unsigned int id);

typedef struct
{
    char *filePath;
    char *name;
} language_t;

int lngAddLanguages(char *path, const char *separator, int mode);
void lngInit(void);
char *lngGetValue(void);
void lngEnd(void);

// Indices are shifted in GUI, as we add the internal english language at 0
int lngSetGuiValue(int langID);
int lngGetGuiValue(void);
int lngFindGuiID(const char *lang);
char **lngGetGuiList(void);
char *lngGetFilePath(int langID);

#endif
