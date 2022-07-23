/*
 * Low-level cheat engine
 *
 * Copyright (C) 2009-2010 Mathias Lafeldt <misfire@debugon.org>
 *
 * This file is part of PS2rd, the PS2 remote debugger.
 *
 * PS2rd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PS2rd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PS2rd.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id$
 */

#include <tamtypes.h>
#include <kernel.h>
#include <syscallnr.h>
#include "include/cheat_api.h"

/*---------------------------------*/
/* Setup PS2RD Cheat Engine params */
/*---------------------------------*/
void SetupCheats()
{
    code_t code;

    int i, j, k, nextCodeCanBeHook;
    i = 0;
    j = 0;
    k = 0;
    nextCodeCanBeHook = 1;

    while (i < MAX_CHEATLIST) {

        code.addr = gCheatList[i];
        code.val = gCheatList[i + 1];
        i += 2;

        if ((code.addr == 0) && (code.val == 0))
            break;

        if (((code.addr & 0xfe000000) == 0x90000000) && nextCodeCanBeHook == 1) {
            hooklist[j] = code.addr & 0x01FFFFFC;
            j++;
            hooklist[j] = code.val;
            j++;
        } else {
            codelist[k] = code.addr;
            k++;
            codelist[k] = code.val;
            k++;
        }
        // Discard any false positives from being possible hooks
        if ((code.addr & 0xf0000000) == 0x40000000 || (code.addr & 0xf0000000) == 0x30000000) {
            nextCodeCanBeHook = 0;
        } else {
            nextCodeCanBeHook = 1;
        }
    }
    numhooks = j / 2;
    numcodes = k / 2;
}

/*-----------------------------------------------------*/
/* Replace SetupThread in kernel. (PS2RD Cheat Engine) */
/*-----------------------------------------------------*/
static inline void Install_HookSetupThread(void)
{
    Old_SetupThread = GetSyscallHandler(__NR_SetupThread);
    SetSyscall(__NR_SetupThread, HookSetupThread);
}

/*----------------------------------------------------*/
/* Restore original SetupThread. (PS2RD Cheat Engine) */
/*----------------------------------------------------*/
static inline void Remove_HookSetupThread(void)
{
    SetSyscall(__NR_SetupThread, Old_SetupThread);
}

/*---------------------------*/
/* Enable PS2RD Cheat Engine */
/*---------------------------*/
void EnableCheats(void)
{
    // Setup Cheats
    SetupCheats();
    // Install Hook SetupThread
    Install_HookSetupThread();
}

/*----------------------------*/
/* Disable PS2RD Cheat Engine */
/*----------------------------*/
void DisableCheats(void)
{
    // Remove Hook SetupThread
    Remove_HookSetupThread();
}
