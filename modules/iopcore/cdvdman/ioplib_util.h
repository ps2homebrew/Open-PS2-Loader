/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#ifndef IOPLIB_UTIL_H
#define IOPLIB_UTIL_H

typedef struct
{
    int version;
    void **exports;
} modinfo_t;

int getModInfo(char *modname, modinfo_t *info);
void hookMODLOAD(void);

#endif /* IOPLIB_UTIL_H */
