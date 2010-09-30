#ifndef _CDVDMAN_ADD_H_
#define _CDVDMAN_ADD_H_

#include "types.h"
#include "irx.h"

#include <cdvdman.h>

int sceCdRC(cd_clock_t *rtc);
#define I_sceCdRC DECLARE_IMPORT(51 , sceCdRC)

#endif
