/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#include "ioptrap.h"

void set_dba(u32 v)
{
    __asm__ __volatile__("mtc0 %0, $5"
                         :
                         : "r"(v));
}

void set_dbam(u32 v)
{
    __asm__ __volatile__("mtc0 %0, $9"
                         :
                         : "r"(v));
}

void set_dcic(u32 v)
{
    __asm__ __volatile__("mtc0 %0, $7"
                         :
                         : "r"(v));
}

u32 get_dba()
{
    u32 v;
    __asm__ __volatile__("mfc0 %0, $5"
                         : "=&r"(v));
    return v;
}

u32 get_dbam()
{
    u32 v;
    __asm__ __volatile__("mfc0 %0, $9"
                         : "=&r"(v));
    return v;
}

u32 get_dcic()
{
    u32 v;
    __asm__ __volatile__("mfc0 %0, $7"
                         : "=&r"(v));
    return v;
}
