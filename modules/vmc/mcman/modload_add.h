#ifndef _MODLOAD_ADD_H_
#define _MODLOAD_ADD_H_

#include "types.h"
#include "irx.h"

void SetSecrmanCallbacks(void *SecrCardBootFile_fnc, void *SecrDiskBootFile_fnc, void *SetLoadfileCallbacks_fnc);
#define I_SetSecrmanCallbacks DECLARE_IMPORT(12, SetSecrmanCallbacks)

void SetCheckKelfPathCallback(void *CheckKelfPath_fnc);
#define I_SetCheckKelfPathCallback DECLARE_IMPORT(13, SetCheckKelfPathCallback)

#endif
