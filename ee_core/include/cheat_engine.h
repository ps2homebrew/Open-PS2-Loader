/*
 * Interface to cheat engine
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

#ifndef _CHEATENGINE_H_
#define _CHEATENGINE_H_

#define MAX_HOOKS     5
#define MAX_CODES     250
#define MAX_CHEATLIST (MAX_HOOKS * 2 + MAX_CODES * 2)

extern void (*Old_SetupThread)(void *gp, void *stack, s32 stack_size, void *args, void *root_func);
extern void HookSetupThread(void *gp, void *stack, s32 stack_size, void *args, void *root_func);

extern u32 numhooks;
extern u32 numcodes;
extern u32 hooklist[];
extern u32 codelist[];

#endif /* _CHEATENGINE_H_ */
