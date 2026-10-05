#ifndef _CHEATCONFIG_H_
#define _CHEATCONFIG_H_

#include "cheatman.h"

/* Returns non-zero for standardized required-code headings. */
int cheatConfigIsMasterCode(const char *name);

/* Reset optional cheats to off, then restore enabled entries by normalized name. */
void cheatConfigLoadSelections(config_set_t *configSet);

/* Save enabled optional cheats as numbered per-game CFG values. */
void cheatConfigSaveSelections(config_set_t *configSet);

#endif /* _CHEATCONFIG_H_ */
